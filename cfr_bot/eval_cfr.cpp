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
    array<array<int, BOARD_SIZE>, NUM_BOARDS_> boards;
    array<int, HAND_SIZE> c1;
    array<int, HAND_SIZE> c2;
    deal_game(boards, c1, c2);

    // both players pair cards according to heuristic
    allocate_heuristic(c1, data1);
    allocate_heuristic(c2, data2);

    // convert to 2D (but keep ordering the same)
    // ALLOCATIONS[0] = {0,1,2}
    array<array<int, HAND_SIZE>, NUM_BOARDS_> c1_hands;
    array<array<int, HAND_SIZE>, NUM_BOARDS_> c2_hands;
    allocate_hands(ALLOCATIONS[0], c1, c1_hands);
    allocate_hands(ALLOCATIONS[0], c2, c2_hands);

    pair<int, int> stacks = {STARTING_STACK, STARTING_STACK};

    // sample allocs
    int alloc_info_state1 = get_cards_info_state_alloc(c1_hands, data1);
    ULL my_key = info_to_key(button, 0, alloc_info_state1);
    auto alloc_info1 = fetch_infoset(infosets1, my_key, NUM_ALLOCATIONS);
    int my_allocation = alloc_info1.get_action_index_avg();
    if (verbose) cout << "P1 alloc " << my_allocation << endl;
    // array<array<int, HAND_SIZE>, NUM_BOARDS_> c1_hands;
    allocate_hands(ALLOCATIONS[my_allocation], c1, c1_hands);
    int alloc_info_state2 = get_cards_info_state_alloc(c2_hands, data2);
    ULL villain_key = info_to_key(1-button, 0, alloc_info_state2);
    auto alloc_info2 = fetch_infoset(infosets2, villain_key, NUM_ALLOCATIONS);
    int villain_allocation = alloc_info2.get_action_index_avg();
    if (verbose) cout << "P2 alloc " << villain_allocation << endl;
    // array<array<int, HAND_SIZE>, NUM_BOARDS_> c2_hands;
    allocate_hands(ALLOCATIONS[villain_allocation], c2, c2_hands);

    pair<double, double> tot_vals = {0, 0};

    // play out each board
    for (int j = 0; j < NUM_BOARDS_; j++) {
        if (verbose) cout << "* Board " << j << " *" << endl;
        array<int, NUM_STREETS> card_info_state1 = get_cards_info_state(
            c1_hands[j], boards[j], data1, N_EVAL_ITER);
        array<int, NUM_STREETS> card_info_state2 = get_cards_info_state(
            c2_hands[j], boards[j], data2, N_EVAL_ITER);
        
        // compute winner of board
        int winner;
        ULL board_mask = indices_to_mask(boards[j]);
        ULL c1_mask = indices_to_mask(c1_hands[j]);
        ULL c2_mask = indices_to_mask(c2_hands[j]);
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
            for (auto& c : boards[j]) {
                cout << pretty_card(c) << " ";
            }
            cout << "] c1[";
            for (auto& c : c1_hands[j]) {
                cout << pretty_card(c) << " ";
            }
            cout << "] c2[";
            for (auto& c : c2_hands[j]) {
                cout << pretty_card(c) << " ";
            }
            cout << "]" << endl;
            cout << "winner = P" << winner+1 << endl;
        }
        

        // initialize empty action history
        BoardActionHistory history(button, winner, BOARD_ANTES[j]);
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
            ULL key = info_to_key(player,
                                  j+1, history.street,
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
        pair<double, double> sub_vals = make_pair(history.won[0], history.won[1]);
        if (verbose) cout << "final vals: " << sub_vals << endl;
        tot_vals = tot_vals + sub_vals;
    }
    if (verbose) cout << endl;

    return tot_vals;
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

        // print current allocation strategies
        cout << fetch_infoset(infosets1, 0, NUM_ALLOCATIONS) << endl;
        cout << fetch_infoset(infosets1, 1, NUM_ALLOCATIONS) << endl << endl;
    }
    cout << "Infosets1 count " << infosets1.size() << endl;
    ifstream infosets2_file(infosets2_path);
    if (infosets2_file.good()) {
        load_infosets_from_file(infosets2_path, infosets2);

        // print current allocation strategies
        cout << fetch_infoset(infosets2, 0, NUM_ALLOCATIONS) << endl;
        cout << fetch_infoset(infosets2, 1, NUM_ALLOCATIONS) << endl << endl;
    }
    cout << "Infosets2 count " << infosets2.size() << endl;

    Stats stats;
    pair<double, double> tot_vals;
    vector<double> val_samples;
    const int n_runouts = 2000000;
    tqdm pbar;
    for (int i = 0; i < n_runouts; ++i) {
        pbar.progress(i, n_runouts);
        int button = i % 2; // alternate button
        pair<double, double> val = run_out_game(
            infosets1, infosets2, data1, data2, button, stats, false);
        tot_vals = tot_vals + (1. / n_runouts) * val;
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
        DATA_PATH + "equity_data/alloc_buckets_1.txt",
        DATA_PATH + "equity_data/preflop_equities.txt",
        DATA_PATH + "equity_data/flop_buckets_150.txt",
        DATA_PATH + "equity_data/turn_clusters_150.txt",
        DATA_PATH + "equity_data/river_clusters_150.txt");
    DataContainer data2(
        DATA_PATH + "equity_data/alloc_buckets_1.txt",
        DATA_PATH + "equity_data/preflop_equities.txt",
        DATA_PATH + "equity_data/flop_buckets_150.txt",
        DATA_PATH + "equity_data/turn_clusters_150.txt",
        DATA_PATH + "equity_data/river_clusters_150.txt");

    eval_cfr(data1, data2);

    return 0;
}
