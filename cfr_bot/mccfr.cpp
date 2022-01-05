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
string TAG = "bugfix";

string infosets_path;
InfosetDict infosets;

string DATA_PATH = "../../pokerbots-2021-data/cpp/";

bool VERBOSE = false;

pair<double, double> mccfr(int board_num,
                            array<int, BOARD_SIZE> &board,
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
                          board_num+1, history.street,
                          card_info_state[history.street],
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
                board_num, board, new_history, card_info_state1, card_info_state2, infosets);

            tot_val = tot_val + strategy[i]*sub_val;
            utils.push_back(sub_val.first);
        }

        // utils -> regrets
        for (int i = 0; i < utils.size(); i++) {
            utils[i] -= tot_val.first;
        }

        if (VERBOSE) {
            cout << "history[" << board_num << "]: " << history << endl;
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
        return mccfr(board_num, board, new_history, card_info_state1, card_info_state2, infosets);

    }
}

pair<double, double> mccfr_top(array<array<int, BOARD_SIZE>, NUM_BOARDS_> &boards,
                                array<int, NUM_CARDS> &c1,
                                array<int, NUM_CARDS> &c2,
                                int ind, // 0 = button (SB), 1 = non-button (BB) for traverser (traverser in first index)
                                InfosetDict &infosets,
                                DataContainer &data) {

    // both players pair cards according to heuristic
    allocate_heuristic(c1, data);
    allocate_heuristic(c2, data);

    // villain randomly picks an allocation from strategy
    auto villain_infoset = fetch_infoset(infosets, 1-ind, NUM_ALLOCATIONS);
    int villain_allocation = villain_infoset.get_action_index(EPS_GREEDY_EPSILON);
    array<array<int, HAND_SIZE>, NUM_BOARDS_> c2_hands;
    allocate_hands(ALLOCATIONS[villain_allocation], c2, c2_hands);

    // get player's strategy for allocation
    CFRInfoset& infoset = fetch_infoset(infosets, ind, NUM_ALLOCATIONS);
    vector<double> strategy = infoset.get_regret_matching_strategy();
    assert(strategy.size() == NUM_ALLOCATIONS);

    pair<double, double> tot_vals = {0, 0};
    int winner;

    // compute card infostates for non-traverser's chosen allocation
    array<array<int, NUM_STREETS>, NUM_BOARDS_> card_info_states2;
    for (int i = 0; i < NUM_BOARDS_; i++) {
        card_info_states2[i] = get_cards_info_state(
                c2_hands[i], boards[i], data, N_EVAL_ITER);
    }

    // traverse allocation choices
    vector<double> utils;
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        pair<double, double> board_tot_vals = {0, 0};

        // build cards for this allocation
        array<array<int, HAND_SIZE>, NUM_BOARDS_> c1_hands;
        allocate_hands(ALLOCATIONS[i], c1, c1_hands);

        // do traversal for each board
        for (int j = 0; j < NUM_BOARDS_; j++) {

            array<int, NUM_STREETS> card_info_state1 = get_cards_info_state(
                c1_hands[j], boards[j], data, N_EVAL_ITER);

            // compute winner of board
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
            // cout << "board[";
            // for (auto& c : boards[j]) {
            //     cout << pretty_card(c) << " ";
            // }
            // cout << "] c1[";
            // for (auto& c : c1_hands[j]) {
            //     cout << pretty_card(c) << " ";
            // }
            // cout << "] c2[";
            // for (auto& c : c2_hands[j]) {
            //     cout << pretty_card(c) << " ";
            // }
            // cout << "]" << endl;
            // cout << "winner = " << winner << endl;

            // initialize empty action history
            BoardActionHistory history(ind, winner, BOARD_ANTES[j]);

            if (VERBOSE) cout << "== BEGIN MCCFR [" << j << "] ==" << endl;
            auto sub_vals = mccfr(
                j, boards[j], history, card_info_state1, card_info_states2[j], infosets);
            if (VERBOSE) cout << "== END MCCFR [" << j << "] ==" << endl;

            board_tot_vals = board_tot_vals + sub_vals;
        }

        tot_vals = tot_vals + strategy[i] * board_tot_vals;
        utils.push_back(board_tot_vals.first);
    }

    // utils -> regrets
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        utils[i] -= tot_vals.first;
    }

    infoset.record(utils, strategy);

    return tot_vals;
}

void catch_interrupt(int signum) {
    cout << "Keyboard interrupt, saving progress" << endl;

    save_infosets_to_file(infosets_path, infosets);
    exit(signum);
}

void run_mccfr(int n_cfr_iter, int n_eval_iter, string tag, DataContainer &data) {
    infosets_path = DATA_PATH + "cfr_train_data/" + GAME + "_infosets_" + tag + ".txt";

    ifstream infosets_file(infosets_path);
    if (infosets_file.good()) {
        load_infosets_from_file(infosets_path, infosets);

        // print current allocation strategies
        cout << fetch_infoset(infosets, 0, NUM_ALLOCATIONS) << endl;
        cout << fetch_infoset(infosets, 1, NUM_ALLOCATIONS) << endl << endl;
    }
    cout << "Initial infosets count " << infosets.size() << endl;

    signal(SIGINT, catch_interrupt);

    pair<double, double> train_val = {0, 0};

    tqdm pbar;
    for (int i = 0; i < N_CFR_ITER; i++) {
        pbar.progress(i, N_CFR_ITER);

        array<array<int, BOARD_SIZE>, NUM_BOARDS_> boards;
        array<int, NUM_CARDS> c1;
        array<int, NUM_CARDS> c2;
        deal_game(boards, c1, c2);
        // initialize bet history

        int ind = i%2; // alternate position

        auto val = mccfr_top(boards, c1, c2, ind, infosets, data);
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
        DATA_PATH + "equity_data/alloc_buckets_1.txt",
        DATA_PATH + "equity_data/preflop_equities.txt",
        DATA_PATH + "equity_data/flop_buckets_10.txt",
        DATA_PATH + "equity_data/turn_clusters_10.txt",
        DATA_PATH + "equity_data/river_clusters_10.txt");

    run_mccfr(N_CFR_ITER, N_EVAL_ITER, TAG, data);

    return 0;
}
