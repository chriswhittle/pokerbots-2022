#ifndef REAL_POKER_GAME
#define REAL_POKER_GAME

#include <cassert>

#include "define.h"
#include "eval7pp.h"
#include "compute_equity.h"

using namespace std;

// define actions for bet history
const int FOLD = 1;
const int CHECK_CALL = 2;

// relative to pot size
const int BET = 3;
const array<float, 2> BET_SIZES = {0.67, 100}; // 2 bet sizes, 100 = all-in

const int RAISE = 3 + BET_SIZES.size();
const array<float, 2> RAISE_SIZES = {1, 100}; // 2 raise sizes, 100 = all-in

const int HAND_SIZE = 2;
const int FLOP_SIZE = 3;
const int TURN_SIZE = 4;
const int BOARD_SIZE = 5;
const int NUM_STREETS = 4;

const int NUM_BOARDS_ = 3;
const int NUM_CARDS = HAND_SIZE*NUM_BOARDS_;
const int BIG_BLIND = 2;
const int BOARD_ANTES[3] = {1*BIG_BLIND, 2*BIG_BLIND, 3*BIG_BLIND};
const int STARTING_STACK = 100*BIG_BLIND;

const int NUM_RANKS = 13;
const int NUM_SUITS = 4;

// action history for a single board
const int ACTION_BIT_SHIFT = 3;

ostream& print_action(ostream& os, int action);
inline ostream& print_actions(ostream& os, const vector<int>& acts) {
    for (auto act : acts) {
        print_action(os, act);
        os << " ";
    }
    return os;
}

struct BoardActionHistory {
    vector<int> actions;
    int button = 0;
    int ind = 0;
    int street = 0;
    int action_starts[3] = { 0, 0, 0 };

    bool finished = false;
    bool showdown = false;
    int winner = 0;

    int pot = 0;
    int ante = 0;

    array<int, 2> stack = {{STARTING_STACK, STARTING_STACK}};
    array<int, 2> pip = {{0, 0}};

    array<int, 2> won = {{0, 0}};

    ULL action_key = 0;
    int key_shift = 0;

    BoardActionHistory() {}

    //// NOTE: `init_winner` specifies who _would_ win if we reach showdown
    BoardActionHistory(int init_button, int init_winner, int ante)
        : button(init_button), ind(init_button), winner(init_winner), pot(ante), ante(ante) {
        pip[button] = BIG_BLIND/2;
        pip[1-button] = BIG_BLIND;
    }

    void update(int new_action);

    vector<int> get_available_actions() const;

};

inline ostream& operator<<(ostream& os, BoardActionHistory& p) {
    os << "BoardActionHistory(street=" << p.street << ",";
    os << "pot=" << p.pot << ",";
    os << "pips=" << p.pip << ",";
    os << "stacks=" << p.stack << ",";
    os << "ind=" << p.ind << ",";
    os << "button=" << p.button << ",";
    os << "winner=" << p.winner << ")";
    return os;
}

// shuffle cards around given an allocation
inline void allocate_hands(
    const int allocation[NUM_BOARDS_], array<int, NUM_CARDS> c,
    array<array<int, HAND_SIZE>, NUM_BOARDS_> &c_out) {
    for (int i = 0; i < NUM_BOARDS_; i++) {
        for (int j = 0; j < HAND_SIZE; j++) {
            c_out[i][j] = c[allocation[i]*HAND_SIZE+j];
        }
    }
}

// heuristic for pairing up cards before allocation
void allocate_heuristic(array<int, NUM_CARDS> &c, const DataContainer &data);

// deal NUM_BOARDS_ boards to the river and two hands
void deal_game(
    array<array<int, BOARD_SIZE>, NUM_BOARDS_> &boards,
    array<int, NUM_CARDS> &c1, array<int, NUM_CARDS> &c2);

#endif
