#include <boost/atomic.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <iostream>
#include <random>

#include <signal.h>
#include <stdlib.h>
#include <cassert>

#include "tqdm.h"
#include "eval7pp.h"

#include "gametree.h"
#include "compute_equity.h"
#include "define.h"

using namespace std;

// run settings
#define N_THREADS 8

const bool VERBOSE = false;

// MCCFR constants
const int N_CFR_ITER = 300000000;
const int N_EVAL_ITER = 100;
const double EPS_GREEDY_EPSILON = 0.1;

// path strings
string GAME = "cpp_poker";
string TAG = "cwhittle-buckets50";

string infosets_path;
string DATA_PATH = "../../pokerbots-2021-data/cpp/";

// data structures
InfosetDict infosets;
DataContainer data(
    DATA_PATH + "equity_data/alloc_buckets_1.txt",
    DATA_PATH + "equity_data/preflop_equities.txt",
    DATA_PATH + "equity_data/flop_buckets_50.txt",
    DATA_PATH + "equity_data/turn_clusters_50.txt",
    DATA_PATH + "equity_data/river_clusters_50.txt");

/////////////////////////////////////////////////////////////////////
/////////////////////// PRODUCER THREAD /////////////////////////////
/////////////////////////////////////////////////////////////////////

struct RoundDeals {
    int prod_id;

    // after cards are paired by heuristic, we have 3 pairs and 3 boards (and 4 streets for each, 2 players)
    // card_info_states[player][board_num][pair_num][street_num]
    array<array<array<array<int, NUM_STREETS>, NUM_BOARDS_>, NUM_BOARDS_>, 2> card_info_states;

    // alloc_info_states[player]
    array<int, 2> alloc_info_states;
    
    // we have 3 pairs vs 3 pairs on 3 boards
    // winner_matrix[board_num][player_pair][villain_pair]
    array<array<array<int, NUM_BOARDS_>, NUM_BOARDS_>, NUM_BOARDS_> winner_matrix;
};

// queue for each producer thread
volatile bool done = false;
array<boost::lockfree::spsc_queue<RoundDeals, boost::lockfree::capacity<1024>>, N_THREADS> queues;
vector<boost::thread> prod_threads; // threads global so we can kill them

// producer that:
// 1) deals out cards (6x2 for each player, 5x3 for each board)
// 2) pairs up cards for each player using heuristic
// 3) computes 3x3 board+hand (x4 for streets) infostates (x2 for players)
// 4) computes 3x3x3 board+hand vs hand winner matrix
void producer(int id) {
    // set up producer
    random_device rd;
    mt19937 gen(rd());
    assert(id < N_THREADS);
    auto& my_queue = queues[id];

    // do computations
    while (!done) {
        RoundDeals my_round_deal{.prod_id = id};

        // deal out boards and cards
        array<array<int, BOARD_SIZE>, NUM_BOARDS_> boards;
        array<int, NUM_CARDS> c1;
        array<int, NUM_CARDS> c2;
        deal_game(boards, c1, c2);

        // players pair cards according to heuristic
        allocate_heuristic(c1, data);
        allocate_heuristic(c2, data);

        // convert to 2D (but keep ordering the same)
        // ALLOCATIONS[0] = {0,1,2}
        array<array<int, HAND_SIZE>, NUM_BOARDS_> c1_hands;
        array<array<int, HAND_SIZE>, NUM_BOARDS_> c2_hands;
        allocate_hands(ALLOCATIONS[0], c1, c1_hands);
        allocate_hands(ALLOCATIONS[0], c2, c2_hands);

        // get the alloc infostate
        int alloc_infostate1 = get_cards_info_state_alloc(c1_hands, data);
        int alloc_infostate2 = get_cards_info_state_alloc(c2_hands, data);
        my_round_deal.alloc_info_states = {alloc_infostate1, alloc_infostate2};

        // calculate card infostates
        // also pre-compute (board, hand) evaluations
        // hand_strengths[player_num][board_num][hand_num]
        array<array<array<int, NUM_BOARDS_>, NUM_BOARDS_>, 2> hand_strengths;
        for (int p = 0; p < 2; p++) {
            array<array<int, HAND_SIZE>, NUM_BOARDS_> c = (p == 0) ? c1_hands : c2_hands;

            // iterate over boards
            for (int i = 0; i < NUM_BOARDS_; i++) {
                ULL board_mask = indices_to_mask(boards[i]);
                // iterate over hands
                for (int j = 0; j < NUM_BOARDS_; j++) {
                    my_round_deal.card_info_states[p][i][j] = get_cards_info_state(
                            c[j], boards[i], data, N_EVAL_ITER);

                    ULL c_mask = indices_to_mask(c[j]);
                    assert((board_mask & c_mask) == 0);
                    hand_strengths[p][i][j] = evaluate(c_mask | board_mask, 7);
                }
            }
        }

        // compute winner matrix
        // iterate over boards
        for (int i = 0; i < NUM_BOARDS_; i++) {
            // iterate over player hands
            for (int j = 0; j < NUM_BOARDS_; j++) {
                // iterate over villain hands
                for (int k = 0; k < NUM_BOARDS_; k++) {
                    
                    if (hand_strengths[0][i][j] == hand_strengths[1][i][k]) {
                        my_round_deal.winner_matrix[i][j][k] = -1;
                    }
                    else {
                        my_round_deal.winner_matrix[i][j][k] = hand_strengths[0][i][j] < hand_strengths[1][i][k];
                    }
                }
            }
        }

	my_queue.push(my_round_deal); // don't care if queue is backed up
    }

    cout << "Producer " << id << " shutting down" << endl;
}

/////////////////////////////////////////////////////////////////////
/////////////////////// CONSUMER THREAD /////////////////////////////
/////////////////////////////////////////////////////////////////////

/////////////////////////////////////
////////// CFR LOGIC ////////////////
/////////////////////////////////////

// one-board CFR logic
pair<double, double> mccfr(int board_num, int winner,
                            GameTreeNode &node,
                            array<int, NUM_STREETS> &card_info_state1,
                            array<int, NUM_STREETS> &card_info_state2) {

    // reached leaf node
    if (node.children.size() == 0) {
        assert(node.finished);
        if (VERBOSE) {
            cout << "DEBUG: terminal game tree node " << node << endl;
            auto val = node.won;
            cout << "-> value " << val << endl;
        }

        // if no showdown, just return amount won according to history
        if (!node.showdown) {
            assert(node.won != 0);
            return make_pair(node.won, -node.won);
        }
        // showdown with chop
        else if (winner == -1) {
            return make_pair(0, 0);
        }
        // showdown without chop
        else {
            // gametree doesn't keep track of antes, add ante/2
            // (node assumes player won)
            assert(node.won > 0);
            int winner_mult = (winner == 0) ? 1 : -1;
            return make_pair(node.won * winner_mult, -node.won * winner_mult);
        }
    }


    auto& card_info_state = (node.ind == 0) ? card_info_state1 : card_info_state2;
    ULL key = info_to_key(node.history_key, board_num+1, card_info_state[node.street]);

    CFRInfoset& infoset = fetch_infoset(infosets, key, node.children.size());

    assert(infoset.cumu_regrets.size() == node.children.size());

    // our (traverser's) action
    if (node.ind == 0) {

        vector<double> strategy = infoset.get_regret_matching_strategy();
        assert(strategy.size() == node.children.size());

        vector<double> utils(node.children.size());
        pair<double, double> tot_val = {0, 0};
        for (int i = 0; i < node.children.size(); i++) {
            auto sub_val = mccfr(board_num, winner, node.children[i],
                                card_info_state1, card_info_state2);

            tot_val = tot_val + strategy[i]*sub_val;
            utils[i] = sub_val.first;
        }

        // utils -> regrets
        for (int i = 0; i < utils.size(); i++) {
            utils[i] -= tot_val.first;
        }

        if (VERBOSE) {
            cout << "node[" << board_num << "]: " << node << endl;
            cout << "num acts: " << node.children.size() << endl;
        }
        infoset.record(utils, strategy);
        return tot_val;
    }
    // villain's action
    else {

        // sample action from strategy
        int action = infoset.get_action_index(EPS_GREEDY_EPSILON);

        if (VERBOSE) {
            cout << "Sampling child #" << action << endl;
        }
        return mccfr(board_num, winner, node.children[action],
                        card_info_state1, card_info_state2);

    }
}

// performs allocate stage before recursing into one-board tree
// 0 = button (SB), 1 = non-button (BB) for traverser (traverser in first index)
pair<double, double> mccfr_top(RoundDeals round_deal, int ind, array<GameTreeNode, NUM_BOARDS_> &roots) {

    // villain randomly picks an allocation from strategy
    ULL villain_key = info_to_key(1-ind, 0, round_deal.alloc_info_states[1-ind]);
    auto villain_infoset = fetch_infoset(infosets, villain_key, NUM_ALLOCATIONS);
    int villain_allocation = villain_infoset.get_action_index(EPS_GREEDY_EPSILON);

    // get player's strategy for allocation
    ULL my_key = info_to_key(ind, 0, round_deal.alloc_info_states[ind]);
    CFRInfoset& infoset = fetch_infoset(infosets, my_key, NUM_ALLOCATIONS);
    vector<double> strategy = infoset.get_regret_matching_strategy();
    assert(strategy.size() == NUM_ALLOCATIONS);

    pair<double, double> tot_vals = {0, 0};

    // traverse allocation choices
    vector<double> utils(NUM_ALLOCATIONS);
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        pair<double, double> board_tot_vals = {0, 0};

        // do traversal for each board
        for (int j = 0; j < NUM_BOARDS_; j++) {
            
            if (VERBOSE) cout << "== BEGIN MCCFR [" << j << "] ==" << endl;
            auto sub_vals = mccfr(
                j, round_deal.winner_matrix[j][ALLOCATIONS[i][j]][ALLOCATIONS[villain_allocation][j]],
                //               this board=^  ^=which pair is in board j for allocation i ^ which pair is in board j for villain allocation
                roots[j],
                round_deal.card_info_states[0][j][ALLOCATIONS[i][j]],
                round_deal.card_info_states[1][j][ALLOCATIONS[villain_allocation][j]]
            );
            if (VERBOSE) cout << "== END MCCFR [" << j << "] ==" << endl;

            board_tot_vals = board_tot_vals + sub_vals;
        }

        tot_vals = tot_vals + strategy[i] * board_tot_vals;
        utils[i] = board_tot_vals.first;
    }

    // utils -> regrets
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        utils[i] -= tot_vals.first;
    }

    infoset.record(utils, strategy);

    return tot_vals;
}

/////////////////////////////////////
////////// RUN LOGIC ////////////////
/////////////////////////////////////

struct MultiStats {
    array<ULL, N_THREADS> queue_misses = {};
    array<ULL, N_THREADS> queue_hits = {};
};

// check all thread queues for finished round deal
// and take one
RoundDeals consume_round_deal(int& thread_id, MultiStats& stats) {
  RoundDeals round_deal;
  int miss_count = 0;
  thread_id += 1;
  thread_id %= N_THREADS;
  while (!queues[thread_id].pop(round_deal)) {
    thread_id += 1;
    thread_id %= N_THREADS;
    stats.queue_misses[thread_id] += 1;
    miss_count += 1;
    if (miss_count % N_THREADS == 0) { // sleep after a full cycle of misses
	boost::this_thread::sleep(boost::posix_time::microseconds(100));
    }
  }
  stats.queue_hits[thread_id] += 1;
  return round_deal;
}

// save infoset progress on program interrupt
void catch_interrupt(int signum) {
    cout << "Keyboard interrupt, saving progress..." << endl;

    done = true;
    save_infosets_to_file(infosets_path, infosets);
    exit(signum);
}

void run_mccfr() {
    int thread_id = 0;
    MultiStats multi_stats;

    // if infoset already exists, load in progress
    infosets_path = DATA_PATH + "cfr_train_data/" + GAME + "_infosets_" + TAG + ".txt";
    ifstream infosets_file(infosets_path);
    if (infosets_file.good()) {
        load_infosets_from_file(infosets_path, infosets);

        // print current allocation strategies
        cout << fetch_infoset(infosets, 0, NUM_ALLOCATIONS) << endl;
        cout << fetch_infoset(infosets, 1, NUM_ALLOCATIONS) << endl << endl;
    }
    cout << "Initial infosets count " << infosets.size() << endl;

    // save infoset on ctrl-C
    signal(SIGINT, catch_interrupt);

    // traverse and cache game tree (one for each button position)
    // also need to cache trees for different antes since it changes pot size
    // and affects bet sizings => different trees
    array<array<GameTreeNode, NUM_BOARDS_>, 2> roots;
    for (int btn = 0; btn < 2; btn++) {
        for (int i = 0; i < NUM_BOARDS_; i++) {
            BoardActionHistory history(btn, 0, BOARD_ANTES[i]);
            roots[btn][i] = build_game_tree(history);
        }
    }

    pair<double, double> train_val = {0, 0};

    // MCCFR loop
    tqdm pbar;
    for (int i = 0; i < N_CFR_ITER; ++i) {
        pbar.progress(i, N_CFR_ITER);

        int ind = i%2; // alternate position
        RoundDeals round_deal = consume_round_deal(thread_id, multi_stats);

        auto val = mccfr_top(round_deal, ind, roots[ind]);
        train_val = train_val + (1./N_CFR_ITER) * val;

        if ((i+1) % 50000000 == 0) {
            cout << "Reached iter " << i+1 << ", checkpointing..." << endl;
            string infosets_path = DATA_PATH + "cfr_train_data/" + GAME + "_infosets_" + TAG + "_checkpoint" + to_string(i+1) + ".txt";
            save_infosets_to_file(infosets_path, infosets);
        }
    }
    cout << "DEBUG: misses = " << multi_stats.queue_misses << endl;
    cout << "DEBUG: hits = " << multi_stats.queue_hits << endl;

    // print infoset state
    cout << "Average button value during train = " << train_val << endl;

    cout << fetch_infoset(infosets, 0, NUM_ALLOCATIONS) << endl;
    cout << fetch_infoset(infosets, 1, NUM_ALLOCATIONS) << endl;
    cout << "Final infosets count " << infosets.size() << endl;

    // save updated infoset
    save_infosets_to_file(infosets_path, infosets);

    // tell producers to end
    done = true;
}

int main() {
    // set up producers
    for (int i = 0; i < N_THREADS; ++i) {
        prod_threads.push_back(boost::thread([i](){producer(i);}));
    }

    run_mccfr();

    for (int i = 0; i < N_THREADS; ++i) {
	prod_threads[i].join();
    }

    cout << "Main thread done." << endl;
    return 0;
}
