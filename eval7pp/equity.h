#ifndef EVAL7PP_EQUITY
#define EVAL7PP_EQUITY

// #include <stdlib.h>
#include <iostream>
#include <random>
#include <array>

#include "evaluate.h"

using namespace std;

extern thread_local random_device rd;
extern thread_local mt19937 gen;
// minstd_rand gen(rd()); // should be much faster than Mersenne, at the cost of quality

inline int sample_card_dist() {
    return gen() % 52;
}

inline int sample_card_dist(unsigned long long dead) {
    unsigned int ind;
    unsigned long long choice;

    while (true) {
    ind = sample_card_dist();
    choice = CARD_MASKS_TABLE[ind];
    if ((dead & choice) == 0)
        return ind;
    }
}

template <typename T, size_t N>
inline unsigned long long indices_to_mask(array<T, N> indices) {
    unsigned long long mask = 0;
    for (int i = 0; i < N; i++) {
        mask |= CARD_MASKS_TABLE[indices[i]];
    }
    return mask;
}

inline unsigned long long indices_to_mask(int indices[], int num_indices) {
    unsigned long long mask = 0;
    for (int i = 0; i < num_indices; i++) {
        mask |= CARD_MASKS_TABLE[indices[i]];
    }
    return mask;
}

inline unsigned long long deal_card() {
    return CARD_MASKS_TABLE[sample_card_dist()];
}

inline unsigned long long deal_card(unsigned long long dead) {
    unsigned int ind;
    unsigned long long choice;

    while (true) {
        ind = sample_card_dist();
        choice = CARD_MASKS_TABLE[ind];
        if ((dead & choice) == 0) {
            return choice;
        }
    }
}

inline void filter_range(unsigned long long dead,
        unsigned long long range[],
        int num_range,
        unsigned long long new_range[],
        int& new_num_range) {

    int j = 0;
    for (int i = 0; i < num_range; i++) {
        if ((range[i] & dead) == 0) {
            new_range[j] = range[i];
            j++;
        }
    }

    new_num_range = j;
}

float hand_vs_hand_monte_carlo(
    unsigned long long hand,
    unsigned long long villain_hand,
    unsigned long long start_board,
    int num_board,
    int iterations);

inline float hand_vs_hand_monte_carlo(
    unsigned long long hand,
    unsigned long long villain_hand,
    unsigned long long start_board,
    int iterations) {
    return hand_vs_hand_monte_carlo(hand, villain_hand, start_board, __builtin_popcountll(start_board), iterations); // needs gcc
}

float hand_vs_range_monte_carlo(
    unsigned long long hand,
    unsigned long long full_villain_range[],
    int num_full_villain_range,
    unsigned long long start_board,
    int num_board,
    int iterations,
    unsigned long long common_dead = 0,
    unsigned long long villain_dead = 0);
float hand_vs_range_monte_carlo(
    unsigned long long hand,
    unsigned long long villain_range[],
    int num_villain_range,
    unsigned long long start_board,
    int iterations);

float hand_vs_random_monte_carlo(
    unsigned long long hand,
    unsigned long long start_board,
    int num_board,
    int iterations);

inline float hand_vs_random_monte_carlo(unsigned long long hand,
        unsigned long long start_board,
        int iterations) {

    return hand_vs_random_monte_carlo(hand, start_board, __builtin_popcountll(start_board), iterations); // needs gcc
}

void hand_vs_hand_exact_iterate(
    unsigned long long hand,
    unsigned long long villain_hand,
    unsigned long long board,
    int num_board,
    int num_card,
    unsigned long long dead,
    unsigned int& count,
    unsigned int& total);

inline float hand_vs_hand_exact(unsigned long long hand,
        unsigned long long villain_hand,
        unsigned long long start_board,
        int num_board) {
    
    unsigned int count = 0, total = 0;
    unsigned long long dead = hand | villain_hand | start_board;
    hand_vs_hand_exact_iterate(hand, villain_hand, start_board, 5-num_board, 52, dead, count, total);

    return 0.5 * (double)count / (double)total;
}

inline float hand_vs_hand_exact(unsigned long long hand,
        unsigned long long villain_hand,
        unsigned long long start_board) {
    return hand_vs_hand_exact(hand, villain_hand, start_board, __builtin_popcountll(start_board)); // needs gcc
}

void hand_vs_range_exact_iterate(
    unsigned long long hand,
    unsigned long long villain_range[],
    int num_villain_range,
    unsigned long long board,
    int num_board,
    int num_card,
    unsigned long long dead,
    unsigned int& count,
    unsigned int& total);

float hand_vs_range_exact(
    unsigned long long hand,
    unsigned long long full_villain_range[],
    int num_full_villain_range,
    unsigned long long start_board,
    int num_board,
    unsigned long long common_dead = 0,
    unsigned long long villain_dead = 0);

inline float hand_vs_range_exact(unsigned long long hand,
        unsigned long long villain_range[],
        int num_villain_range,
        unsigned long long start_board) {
    return hand_vs_range_exact(hand, villain_range, num_villain_range, start_board, __builtin_popcountll(start_board)); // needs gcc
}

#endif
