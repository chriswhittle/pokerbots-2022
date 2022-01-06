#include <signal.h>
#include <stdlib.h>
#include <cassert>

#include "tqdm.h"
#include "eval7pp.h"

#include "cfr.h"
#include "compute_equity.h"
#include "define.h"

using namespace std;

int N_CFR_ITER = 1000000;
int N_EVAL_ITER = 50;
double EPS_GREEDY_EPSILON = 0.1;

string GAME = "cpp_poker";
string TAG = "dev";

string infosets_path;
InfosetDict infosets;

string DATA_PATH = "../../data/";

bool VERBOSE = false;

pair<double, double> mccfr(array<int, BOARD_SIZE> &board,
                            BoardActionHistory history,
                            array<int, NUM_STREETS> &card_info_state1,
                            array<int, NUM_STREETS> &card_info_state2,
                            InfosetDict &infosets) {

    vector<int> available_actions = history.get_available_actions();

    if (available_actions.size() == 0) {
        assert(history.finished);
        assert((history.won[0] != 0 && history.won[1] != 0) || history.winner == -1);
        assert(history.won[0] + history.won[1] == 0);
        if (VERBOSE) {
            cout << "DEBUG: terminal history " << history << endl;
            auto val = pair<double,double>(history.won[0], history.won[1]);
            cout << "-> value " << val << endl;
        }
        return make_pair(history.won[0], history.won[1]);
    }


    auto& card_info_state = (history.ind == 0) ? card_info_state1 : card_info_state2;
    ULL key = info_to_key(history.ind ^ history.button,
                          history.street, card_info_state[history.street],
                          history);

    CFRInfoset& infoset = fetch_infoset(infosets, key, available_actions.size());

    assert(infoset.cumu_regrets.size() == available_actions.size());

    // our (traverser's) action
    if (history.ind == 0) {

        vector<double> strategy = infoset.get_regret_matching_strategy();
        assert(strategy.size() == available_actions.size());

        vector<double> utils;
        pair<double, double> tot_val = {0, 0};
        for (int i = 0; i < available_actions.size(); i++) {
            BoardActionHistory new_history = history;
            new_history.update(available_actions[i]);
            auto sub_val = mccfr(
                board, new_history, card_info_state1, card_info_state2, infosets);

            tot_val = tot_val + strategy[i]*sub_val;
            utils.push_back(sub_val.first);
        }

        // utils -> regrets
        for (int i = 0; i < utils.size(); i++) {
            utils[i] -= tot_val.first;
        }

        if (VERBOSE) {
            cout << "history: " << history << endl;
            cout << "acts: ";
            for (auto& act : available_actions) {
                print_action(cout, act);
                cout << " ";
            }
            cout << endl;
        }
        infoset.record(utils, strategy);
        return tot_val;
    }
    // villain's action
    else {

        // sample action from strategy
        int action = infoset.get_action_index(EPS_GREEDY_EPSILON);

        BoardActionHistory new_history = history;
        new_history.update(available_actions[action]);
        if (VERBOSE) {
            cout << "Sampling action ";
            print_action(cout, available_actions[action]);
            cout << endl;
        }
        return mccfr(board, new_history, card_info_state1, card_info_state2, infosets);

    }
}

pair<double, double> mccfr_top(array<int, BOARD_SIZE> &board,
                                array<int, HAND_SIZE> &c1,
                                array<int, HAND_SIZE> &c2,
                                int ind, // 0 = button (SB),
                                // 1 = non-button (BB) for traverser
                                // (traverser in first index)
                                InfosetDict &infosets,
                                DataContainer &data) {
    int winner;

    // compute card infostates
    array<int, NUM_STREETS> card_info_state1 = get_cards_info_state(
        c1, board, data, N_EVAL_ITER);
    array<int, NUM_STREETS> card_info_states2 = get_cards_info_state(
                c2, board, data, N_EVAL_ITER);

    // compute winner of board
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
    // cout << "board[";
    // for (auto& c : board) {
    //     cout << pretty_card(c) << " ";
    // }
    // cout << "] c1[";
    // for (auto& c : c1[j]) {
    //     cout << pretty_card(c) << " ";
    // }
    // cout << "] c2[";
    // for (auto& c : c2[j]) {
    //     cout << pretty_card(c) << " ";
    // }
    // cout << "]" << endl;
    // cout << "winner = " << winner << endl;

    // initialize empty action history
    BoardActionHistory history(ind, winner, 0);

    if (VERBOSE) cout << "== BEGIN MCCFR [" << j << "] ==" << endl;
    auto vals = mccfr(
        board, history, card_info_state1, card_info_states2, infosets);
    if (VERBOSE) cout << "== END MCCFR [" << j << "] ==" << endl;

    return vals;
}

void catch_interrupt(int signum) {
    cout << "Keyboard interrupt, saving progress" << endl;

    save_infosets_to_file(infosets_path, infosets);
    exit(signum);
}

void run_mccfr(int n_cfr_iter, int n_eval_iter, string tag, DataContainer &data) {
    infosets_path = DATA_PATH + "cfr_data/" + GAME + "_infosets_" + tag + ".txt";

    ifstream infosets_file(infosets_path);
    if (infosets_file.good()) {
        load_infosets_from_file(infosets_path, infosets);
    }
    cout << "Initial infosets count " << infosets.size() << endl;

    signal(SIGINT, catch_interrupt);

    pair<double, double> train_val = {0, 0};

    tqdm pbar;
    for (int i = 0; i < N_CFR_ITER; i++) {
        pbar.progress(i, N_CFR_ITER);

        array<int, BOARD_SIZE> board;
        array<int, HAND_SIZE> c1;
        array<int, HAND_SIZE> c2;
        deal_game(board, c1, c2);
        // initialize bet history

        int ind = i%2; // alternate position

        auto val = mccfr_top(board, c1, c2, ind, infosets, data);
        train_val = train_val + (1./N_CFR_ITER) * val;
    }
    cout << "Average button value during train = " << train_val << endl;

    cout << fetch_infoset(infosets, 0, NUM_ALLOCATIONS) << endl;
    cout << fetch_infoset(infosets, 1, NUM_ALLOCATIONS) << endl;
    cout << "Final infosets count " << infosets.size() << endl;

    save_infosets_to_file(infosets_path, infosets);
}

int main() {

    DataContainer data(
        DATA_PATH + "equity_data/flop_buckets_10.txt",
        DATA_PATH + "equity_data/turn_clusters_10.txt",
        DATA_PATH + "equity_data/river_clusters_10.txt");

    run_mccfr(N_CFR_ITER, N_EVAL_ITER, TAG, data);

    return 0;
}
