#ifndef REAL_POKER_GAME
#define REAL_POKER_GAME

#include <cassert>
#include <math.h>

#include "define.h"
#include "eval7pp.h"
#include "compute_equity.h"

using namespace std;

// swap probabilities on flop/turn/river for swap hold 'em
const array<float, 3> SWAP_ODDS = {0.1, 0.05, 0};

// define actions for bet history
const int FOLD = 1;
const int CHECK_CALL = 2;

// relative to pot size; bet and raise size vectors should be sorted
const int BET = 3;
const vector<float> BET_SIZES = {0.33, 0.67, 1.5, 3, 100}; // 5 bet sizes, 100 = all-in

const int RAISE = 3 + BET_SIZES.size();
const vector<float> RAISE_SIZES = {1, 3, 6, 10, 100}; // 5 raise sizes, 100 = all-in

const int HAND_SIZE = 2;
const int FLOP_SIZE = 3;
const int TURN_SIZE = 4;
const int BOARD_SIZE = 5;
const int NUM_STREETS = 4;

const int BIG_BLIND_ = 2;
const int STARTING_STACK_ = 100*BIG_BLIND_;

const int NUM_RANKS = 13;
const int NUM_SUITS = 4;

// action history for a single board
const int SHIFT_ACTIONS = 3; // 3 bits per action

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

    array<int, 2> stack = {{STARTING_STACK_, STARTING_STACK_}};
    array<int, 2> pip = {{0, 0}};

    array<int, 2> won = {{0, 0}};

    ULL action_key = 0;
    int key_shift = 0;

    BoardActionHistory() {}

    //// NOTE: `init_winner` specifies who _would_ win if we reach showdown
    BoardActionHistory(int init_button, int init_winner, int ante)
        : button(init_button), ind(init_button), winner(init_winner), pot(ante), ante(ante) {
        pip[button] = BIG_BLIND_/2;
        pip[1-button] = BIG_BLIND_;
    }

    void update(int new_action);

    int bet_action_to_pip(int action) const;

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

// deal board to the river and two hands
void deal_game(
    array<int, BOARD_SIZE> &board,
    array<int, HAND_SIZE> &c1, array<int, HAND_SIZE> &c2);

void deal_game_swaps(
    array<int, BOARD_SIZE> &board,
    array<array<int, HAND_SIZE>, NUM_STREETS> &c1,
    array<array<int, HAND_SIZE>, NUM_STREETS> &c2,
    array<float, NUM_STREETS-1> swap_odds);

#endif
