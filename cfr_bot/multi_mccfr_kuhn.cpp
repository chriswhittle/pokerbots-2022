#include <boost/atomic.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <iostream>
#include <random>
#include <bitset>

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
const int N_CFR_ITER = 1000000;
const int N_EVAL_ITER = 100;
// const double EPS_GREEDY_EPSILON = 0.1;
const double EPS_GREEDY_EPSILON = 0;

// path strings
string GAME = "cpp_kuhn_poker";
string TAG = "kuhn";

string infosets_path;
string DATA_PATH = "./";

// data structures
InfosetDict infosets;

int unvisited;

/////////////////////////////////////////////////////////////////////
/////////////////////// PRODUCER THREAD /////////////////////////////
/////////////////////////////////////////////////////////////////////

struct RoundDeals {
    int prod_id;

    array<int, 2> card_info_states;
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
    std::uniform_int_distribution<> dis(1, 3);
    assert(id < N_THREADS);
    auto& my_queue = queues[id];

    // do computations
    while (!done) {
        RoundDeals my_round_deal{.prod_id = id};

        int a = 0, b = 0;
        while (a == b) {
            a = dis(gen)%3;
            b = dis(gen)%3;
        }
        my_round_deal.card_info_states = {a, b};

	my_queue.push(my_round_deal); // don't care if queue is backed up
    }

    cout << "Producer " << id << " shutting down" << endl;
}

/////////////////////////////////////////////////////////////////////
/////////////////////// CONSUMER THREAD /////////////////////////////
/////////////////////////////////////////////////////////////////////

// first bit for button
// first 2 bits to card
// remaining history
ULL kuhn_info_to_key(int card, ULL history_key) {
    ULL key = history_key;

    key |= (card << 1);
    
    return key;
}

/////////////////////////////////////
////////// CFR LOGIC ////////////////
/////////////////////////////////////

// one-board CFR logic
pair<double, double> mccfr(
                            GameTreeNode &node,
                            array<int, 2> &card_info_states) {

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
        // showdown without chop
        else {
            // node assumes player won
            assert(node.won > 0);
            int winner_mult = (card_info_states[0] > card_info_states[1]) ? 1 : -1;
            return make_pair(node.won * winner_mult, -node.won * winner_mult);
        }
    }

    ULL key = kuhn_info_to_key(card_info_states[node.ind], node.history_key);

    CFRInfoset& infoset = fetch_infoset(infosets, key, node.children.size());

    assert(infoset.cumu_regrets.size() == node.children.size());

    // our (traverser's) action
    if (node.ind == 0) {

        vector<double> strategy = infoset.get_regret_matching_strategy();
        assert(strategy.size() == node.children.size());

        vector<double> utils(node.children.size());
        pair<double, double> tot_val = {0, 0};
        for (int i = 0; i < node.children.size(); i++) {
            
            auto sub_val = mccfr(node.children[i], card_info_states);

            tot_val = tot_val + strategy[i]*sub_val;
            utils[i] = sub_val.first;
        }

        // utils -> regrets
        for (int i = 0; i < utils.size(); i++) {
            utils[i] -= tot_val.first;
        }

        if (VERBOSE) {
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
        return mccfr(node.children[action],
                        card_info_states);

    }
}

// performs allocate stage before recursing into one-board tree
// 0 = button (SB), 1 = non-button (BB) for traverser (traverser in first index)
pair<double, double> mccfr_top(RoundDeals round_deal, GameTreeNode &root) {

    auto tot_vals = mccfr(root, round_deal.card_info_states);

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
    infosets_path = DATA_PATH + GAME + "_infosets_" + TAG + ".txt";
    ifstream infosets_file(infosets_path);
    if (infosets_file.good()) {
        load_infosets_from_file(infosets_path, infosets);
    }
    cout << "Initial infosets count " << infosets.size() << endl;

    // save infoset on ctrl-C
    signal(SIGINT, catch_interrupt);

    cout << "Starting traversal..." << endl;

    // traverse and cache game tree (one for each button position)
    // (history_key, won, ind, button, street, finished, showdown)
    // actions are (1,2)
    array<GameTreeNode, 2> roots;
    for (int btn = 0; btn < 2; btn++) {
        GameTreeNode root(0, 0, btn, btn, 0, false, false);

        GameTreeNode node1(1 | (1 << 3), 0, 1-btn, btn, 1, false, false);
        GameTreeNode node2(1 | (2 << 3), 0, 1-btn, btn, 1, false, false);

        GameTreeNode node11(0 | (1 << 3) | (1 << 5), 1, btn, btn, 2, true, true);
        GameTreeNode node12(0 | (1 << 3) | (2 << 5), 0, btn, btn, 2, false, false);
        GameTreeNode node21(0 | (2 << 3) | (1 << 5), (btn == 0) ? 1 : -1, btn, btn, 2, true, false);
        GameTreeNode node22(0 | (2 << 3) | (2 << 5), 2, btn, btn, 2, true, true);

        GameTreeNode node121(1 | (1 << 3) | (2 << 5) | (1 << 7), (btn == 0) ? -1 : 1, 1-btn, btn, 3, true, false);
        GameTreeNode node122(1 | (1 << 3) | (2 << 5) | (2 << 7), 2, 1-btn, btn, 3, true, true);

        node12.children.push_back(node121);
        node12.children.push_back(node122);

        node1.children.push_back(node11);
        node1.children.push_back(node12);
        node2.children.push_back(node21);
        node2.children.push_back(node22);

        root.children.push_back(node1);
        root.children.push_back(node2);

        roots[btn] = root;
    }
    
    pair<double, double> train_val = {0, 0};

    // MCCFR loop
    tqdm pbar;
    for (int i = 0; i < N_CFR_ITER; ++i) {
        pbar.progress(i, N_CFR_ITER);

        int ind = i%2; // alternate position
        RoundDeals round_deal = consume_round_deal(thread_id, multi_stats);

        auto val = mccfr_top(round_deal, roots[ind]);
        train_val = train_val + (1./N_CFR_ITER) * val;
    }
    cout << "DEBUG: misses = " << multi_stats.queue_misses << endl;
    cout << "DEBUG: hits = " << multi_stats.queue_hits << endl;

    // print infoset state
    cout << "Average button value during train = " << train_val << endl;

    cout << "Final infosets count " << infosets.size() << endl;

    cout << "Unvisited info sets " << unvisited << endl;

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
