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
#include "binary.h"

using namespace std;

// run settings
#define N_THREADS 8

const bool VERBOSE = false;
const bool SAVE_BINARY = false;

// MCCFR constants
const int N_CFR_ITER = 2000000000;
const int N_CFR_CHECKPOINTS = 100000000;
const int N_EVAL_ITER = 100;
// const double EPS_GREEDY_EPSILON = 0.1;
const double EPS_GREEDY_EPSILON = 0.;

// path strings
string GAME = "v2";
string TAG = "500post";

string infosets_path_partial;
string infosets_path;
string DATA_PATH = "../../data/";

// data structures
InfosetDict infosets;
DataContainer data(
    DATA_PATH + "equity_data/flop_buckets_500.txt",
    DATA_PATH + "equity_data/turn_clusters_500.txt",
    DATA_PATH + "equity_data/river_clusters_500.txt");

/////////////////////////////////////////////////////////////////////
/////////////////////// PRODUCER THREAD /////////////////////////////
/////////////////////////////////////////////////////////////////////

struct RoundDeals {
    int prod_id;

    // 2 players, each with 4 streets of different info states
    // card_info_states[player][street_num]
    array<array<int, NUM_STREETS>, 2> card_info_states;
    
    // which player wins if it gets to showdown
    int winner;
};

// queue for each producer thread
volatile bool done = false;
array<boost::lockfree::spsc_queue<RoundDeals, boost::lockfree::capacity<1024>>, N_THREADS> queues;
vector<boost::thread> prod_threads; // threads global so we can kill them

// producer that:
// 1) deals out cards (2 for each player, 5 for the board)
// 2) computes board+hand (x4 for pf/f/t/r) infostates (x2 for players)
// 3) computes winner if it gets to showdown
void producer(int id) {
    // set up producer
    random_device rd;
    mt19937 gen(rd());
    assert(id < N_THREADS);
    auto& my_queue = queues[id];

    // do computations
    while (!done) {
        RoundDeals my_round_deal{.prod_id = id};

        // deal out board and cards
        array<int, BOARD_SIZE> board;
        array<array<int, HAND_SIZE>, NUM_STREETS> c1;
        array<array<int, HAND_SIZE>, NUM_STREETS> c2;
        deal_game_swaps(board, c1, c2, SWAP_ODDS);

        // generate bitmasks for player hands and board
        ULL board_mask = indices_to_mask(board);

        // calculate card infostates
        // also pre-compute (board, hand) evaluations
        // hand_strengths[player_num]
        array<int, 2> hand_strengths;
        for (int p = 0; p < 2; p++) {
            array<array<int, HAND_SIZE>, NUM_STREETS> c = (p == 0) ? c1 : c2;

            my_round_deal.card_info_states[p] = get_cards_info_state(
                    c, board, data, N_EVAL_ITER);

            // just use river cards for showdown
            ULL c_mask = indices_to_mask(c[NUM_STREETS-1]);
            assert((board_mask & c_mask) == 0);
            hand_strengths[p] = evaluate(c_mask | board_mask, 7);
        }

        // compute winner if it gets to showdown
        if (hand_strengths[0] == hand_strengths[1]) { // tie
            my_round_deal.winner = -1;
        }
        else {
            my_round_deal.winner = hand_strengths[0] < hand_strengths[1];
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

pair<double, double> mccfr(int winner,
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
            // node assumes player won
            assert(node.won > 0);
            int winner_mult = (winner == 0) ? 1 : -1;
            return make_pair(node.won * winner_mult, -node.won * winner_mult);
        }
    }


    auto& card_info_state = (node.ind == 0) ? card_info_state1 : card_info_state2;
    ULL key = info_to_key(node.history_key, card_info_state[node.street]);

    CFRInfoset& infoset = fetch_infoset(infosets, key, node.children.size());

    assert(infoset.cumu_regrets.size() == node.children.size());

    // our (traverser's) action
    if (node.ind == 0) {

        vector<double> strategy = infoset.get_regret_matching_strategy();
        assert(strategy.size() == node.children.size());

        vector<double> utils(node.children.size());
        pair<double, double> tot_val = {0, 0};
        for (int i = 0; i < node.children.size(); i++) {
            auto sub_val = mccfr(winner, node.children[i],
                                card_info_state1, card_info_state2);

            tot_val = tot_val + strategy[i]*sub_val;
            utils[i] = sub_val.first;
        }

        // utils -> regrets
        for (int i = 0; i < utils.size(); i++) {
            utils[i] -= tot_val.first;
        }

        if (VERBOSE) {
            cout << "node: " << node << endl;
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
        return mccfr(winner, node.children[action],
                        card_info_state1, card_info_state2);

    }
}

// performs allocate stage before recursing into one-board tree
// 0 = button (SB), 1 = non-button (BB) for traverser (traverser in first index)
pair<double, double> mccfr_top(RoundDeals round_deal, int ind, GameTreeNode &root) {

    // traverse game tree
    if (VERBOSE) cout << "== BEGIN MCCFR ==" << endl;
    auto vals = mccfr(
        round_deal.winner, root,
        round_deal.card_info_states[0],
        round_deal.card_info_states[1]
    );
    if (VERBOSE) cout << "== END MCCFR ==" << endl;

    return vals;
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
    if (SAVE_BINARY) 
        save_infosets_to_file_bin(infosets_path_partial + ".bin", infosets);
    exit(signum);
}

void run_mccfr() {
    int thread_id = 0;
    MultiStats multi_stats;

    // if infoset already exists, load in progress
    infosets_path_partial = DATA_PATH + "cfr_data/" + GAME + "_infosets_" + TAG;
    infosets_path = infosets_path_partial + ".txt";
    ifstream infosets_file(infosets_path);
    
    cout << "Loading " << infosets_path << endl;
    if (infosets_file.good()) {
        load_infosets_from_file(infosets_path, infosets);
    }
    cout << "Initial infosets count " << infosets.size() << endl;

    // save infoset on ctrl-C
    signal(SIGINT, catch_interrupt);

    // traverse and cache game tree (one for each button position)
    // also need to cache trees for different antes since it changes pot size
    // and affects bet sizings => different trees
    array<GameTreeNode, 2> roots;
    for (int btn = 0; btn < 2; btn++) {
        BoardActionHistory history(btn, 0, 0);
        roots[btn] = build_game_tree(history);
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

        if ((i+1) % N_CFR_CHECKPOINTS == 0) {
            cout << "Reached iter " << i+1 << ", checkpointing..." << endl;

            string infosets_path_partial = DATA_PATH + "cfr_data/" + GAME + "_infosets_" + TAG + "_ckpt" + to_string(i+1);
            string infosets_path = infosets_path_partial + ".txt";

            // full dataset for resuming training
            save_infosets_to_file(infosets_path, infosets);
            // partial dataset for the player
            if (SAVE_BINARY)
                save_infosets_to_file_bin(infosets_path_partial + ".bin", infosets);
        }
    }
    cout << "DEBUG: misses = " << multi_stats.queue_misses << endl;
    cout << "DEBUG: hits = " << multi_stats.queue_hits << endl;

    // print infoset state
    cout << "Average button value during train = " << train_val << endl;
    cout << "Final infosets count " << infosets.size() << endl;

    // save updated infoset
    save_infosets_to_file(infosets_path, infosets);
    if (SAVE_BINARY)
        save_infosets_to_file_bin(infosets_path_partial + ".bin", infosets);

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
