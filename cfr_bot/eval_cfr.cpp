#include <signal.h>
#include <stdlib.h>
#include <bitset>
#include <cassert>
#include <random>

#include "tqdm.h"
#include "eval7pp.h"

#include "cfr.h"
#include "compute_equity.h"
#include "define.h"

using namespace std;

random_device EVAL_RD;
mt19937 EVAL_GEN(EVAL_RD());

const bool VERBOSE = false;
const int N_RUNOUTS = 2000000;

int N_EVAL_ITER = 50;
InfosetDict infosets1, infosets2;

string DATA_PATH = "../../data/";
string infosets1_path = DATA_PATH + "cfr_data/dev1.txt";
string infosets2_path = DATA_PATH + "cfr_data/dev2.txt";

struct Stats {
    array<int, 2> boards_folded = {0, 0};
    array<int, 2> showdowns_won = {0, 0};
    array<double, 2> fold_val = {0, 0};
    array<double, 2> showdown_val = {0, 0};
    int showdowns_chopped = 0;
};
ostream& operator<<(ostream& os, const Stats& stats) {
    os << "stats(";
    os << "folds=" << stats.boards_folded << ",";
    os << "showwins=" << stats.showdowns_won << ",";
    os << "showchops=" << stats.showdowns_chopped << ";";
    os << "foldval=" << stats.fold_val << ",";
    os << "showval=" << stats.showdown_val << ")";
    return os;
}

/// Run CFR bot against itself
pair<double, double> run_out_game(InfosetDict &infosets1, InfosetDict &infosets2,
                                  const DataContainer& data1, const DataContainer& data2,
                                  int button, Stats &stats, bool verbose = false) {
    if (verbose) cout << "== Game ==" << endl;
    array<int, BOARD_SIZE> board;
    array<int, HAND_SIZE> c1;
    array<int, HAND_SIZE> c2;
    deal_game(board, c1, c2);

    pair<int, int> stacks = {STARTING_STACK_, STARTING_STACK_};

    array<int, NUM_STREETS> card_info_state1 = get_cards_info_state(
        c1, board, data1, N_EVAL_ITER);
    array<int, NUM_STREETS> card_info_state2 = get_cards_info_state(
        c2, board, data2, N_EVAL_ITER);
    
    // compute winner of board
    int winner;
    ULL board_mask = indices_to_mask(board);
    ULL c1_mask = indices_to_mask(c1);
    ULL c2_mask = indices_to_mask(c2);
    assert((board_mask & c1_mask) == 0);
    assert((board_mask & c2_mask) == 0);
    assert((c1_mask & c2_mask) == 0);
    int c1_hand_eval = evaluate(c1_mask | board_mask, 7);
    int c2_hand_eval = evaluate(c2_mask | board_mask, 7);

    if (c1_hand_eval == c2_hand_eval) {
        winner = -1;
    }
    else {
        winner = c1_hand_eval < c2_hand_eval;
    }

    // Check showdown looks okay
    if (verbose) {
        cout << "board[";
        for (auto& c : board) {
            cout << pretty_card(c) << " ";
        }
        cout << "] c1[";
        for (auto& c : c1) {
            cout << pretty_card(c) << " ";
        }
        cout << "] c2[";
        for (auto& c : c2) {
            cout << pretty_card(c) << " ";
        }
        cout << "]" << endl;
        cout << "winner = P" << winner+1 << endl;
    }

    // initialize empty action history
    BoardActionHistory history(button, winner, 0);
    while (!history.finished) {
        vector<int> available_actions = history.get_available_actions();
        assert(available_actions.size() > 0);
        array<int, NUM_STREETS> card_info_state;
        // player: 0 = SB, 1 = BB
        // history.ind: absolute position (determines which of c1, c2 they hold)
        if (history.ind == 0) {
            card_info_state = card_info_state1;
        }
        else {
            card_info_state = card_info_state2;
        }
        int ind = history.ind;
        int player = ind ^ history.button;
        ULL key = info_to_key(player, history.street,
                                card_info_state[history.street],
                                history);
        bitset<64> key_bits(key);
        auto& infosets = (history.ind == 0) ? infosets1 : infosets2;
        CFRInfoset& infoset = fetch_infoset(infosets, key, available_actions.size());
        assert(infoset.cumu_regrets.size() == available_actions.size());
        if (verbose) {
            cout << "history = " << history << endl;
            cout << "info = " << key_bits << endl;
            cout << "infoset.t = " << infoset.t << endl;
            // sample action from strategy
            auto strategy = infoset.get_avg_strategy(); // DEBUG
            cout << "Strategy P" << ind+1 << " " << strategy << endl;
            cout << "Cumu regrets P" << ind+1 << " " << infoset.cumu_regrets << endl;
        }
        int action = infoset.get_action_index_avg();
        if (verbose) {
            cout << "Sampled action P" << ind+1 << " [" << action << "]: ";
            print_action(cout, available_actions[action]);
            cout << endl << endl;
        }
        history.update(available_actions[action]);
        if (available_actions[action] == FOLD) {
            assert(history.finished);
            stats.boards_folded[ind] += 1;
            stats.fold_val[ind] += history.won[ind];
            stats.fold_val[1-ind] += history.won[1-ind];
        }
        else if (history.finished) { // showdown
            if (winner == -1) {
                stats.showdowns_chopped += 1;
                assert(history.won[0] == 0);
            }
            else {
                stats.showdowns_won[winner] += 1;
                stats.showdown_val[winner] += history.won[winner];
                stats.showdown_val[1-winner] += history.won[1-winner];
            }
        }
    }
    if (verbose) cout << "final history = " << history << endl;
    assert(history.get_available_actions().size() == 0);
    assert((history.won[0] != 0 && history.won[1] != 0) || history.winner == -1);
    assert(history.won[0] + history.won[1] == 0);
    pair<double, double> vals = make_pair(history.won[0], history.won[1]);
    if (verbose) cout << "final vals: " << vals << endl;

    if (verbose) cout << endl;

    return vals;
}

pair<double,double> bootstrap(const vector<double>& vals, int n_boot) {
    uniform_int_distribution<int> dist(0, vals.size()-1);
    vector<double> boots;
    double boot_sq_mean;
    double boot_mean;
    for (int i = 0; i < n_boot; ++i) {
        double v = 0;
        for (int j = 0; j < vals.size(); ++j) {
            int k = dist(EVAL_GEN);
            v += vals[k] / vals.size();
        }
        boot_mean += v / n_boot;
        boot_sq_mean += v*v / n_boot;
    }
    return make_pair(boot_mean, sqrt(boot_sq_mean - boot_mean*boot_mean));
}


void eval_cfr(const DataContainer& data1, const DataContainer& data2) {
    ifstream infosets1_file(infosets1_path);
    if (infosets1_file.good()) {
        load_infosets_from_file(infosets1_path, infosets1);
    }
    cout << "Infosets1 count " << infosets1.size() << endl;
    ifstream infosets2_file(infosets2_path);
    if (infosets2_file.good()) {
        load_infosets_from_file(infosets2_path, infosets2);
    }
    cout << "Infosets2 count " << infosets2.size() << endl;

    Stats stats;
    pair<double, double> tot_vals;
    vector<double> val_samples;
    tqdm pbar;
    for (int i = 0; i < N_RUNOUTS; ++i) {
        pbar.progress(i, N_RUNOUTS);
        int button = i % 2; // alternate button
        pair<double, double> val = run_out_game(
            infosets1, infosets2, data1, data2, button, stats, VERBOSE);
        tot_vals = tot_vals + (1. / N_RUNOUTS) * val;
        val_samples.push_back(val.first);
    }
    pair<double, double> boot_val = bootstrap(val_samples, 100);
    cout << "Avg value = " << tot_vals << endl;
    cout << "P1 value boot = " << boot_val.first << " " << boot_val.second << endl;
    cout << "Stats: " << stats << endl;
}


int main() {

    // different data for each player in case we change bucketing
    DataContainer data1(
        DATA_PATH + "equity_data/flop_buckets_150.txt",
        DATA_PATH + "equity_data/turn_clusters_150.txt",
        DATA_PATH + "equity_data/river_clusters_150.txt");
    DataContainer data2(
        DATA_PATH + "equity_data/flop_buckets_150.txt",
        DATA_PATH + "equity_data/turn_clusters_150.txt",
        DATA_PATH + "equity_data/river_clusters_150.txt");

    eval_cfr(data1, data2);

    return 0;
}
