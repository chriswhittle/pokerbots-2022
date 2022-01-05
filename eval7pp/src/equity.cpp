#include "equity.h"

thread_local random_device rd;
thread_local mt19937 gen(rd());

float hand_vs_hand_monte_carlo(unsigned long long hand,
        unsigned long long villain_hand,
        unsigned long long start_board,
        int num_board,
        int iterations) {
    unsigned int count = 0;
    unsigned int option_index = 0;
    unsigned long long option;
    unsigned long long dealt;
    unsigned int hero;
    unsigned int villain;
    unsigned long long board;

    dealt = hand | villain_hand;
    for (int i = 0; i < iterations; i++) {
        board = start_board;

        for (int j = 0; j < (5 - num_board); j++) {
            board |= deal_card(board | dealt);
        }
        hero = evaluate(board | hand, 7);
        villain = evaluate(board | villain_hand, 7);
        if (hero > villain) {
            count += 2;
        }
        else if (hero == villain) {
            count += 1;
        }
    }
    return 0.5 * (double)count / (double)iterations;
}

float hand_vs_range_monte_carlo(unsigned long long hand,
        unsigned long long full_villain_range[],
        int num_full_villain_range,
        unsigned long long start_board,
        int num_board,
        int iterations,
        unsigned long long common_dead,
        unsigned long long villain_dead) {
    // common_dead = cards that cannot appear on the board or in villain's range
    // villain_dead = cards that can appear on the board but not in villain's range
    unsigned int count = 0;
    unsigned long long option;
    unsigned long long dealt;
    unsigned int hero;
    unsigned int villain;
    unsigned long long villain_hand;
    unsigned long long board;

    unsigned long long villain_range[num_full_villain_range];
    int num_villain_range;
    filter_range(hand | start_board | common_dead | villain_dead,
            full_villain_range, num_full_villain_range,
            villain_range, num_villain_range);

    unsigned long long dead = hand | common_dead;

    int option_index = 0; // iterate over villain range to evenly sample
    for (int i = 0; i < iterations; i++) {
        villain_hand = villain_range[option_index];
        option_index += 1;
        if (option_index >= num_villain_range) {
            option_index = 0;
        }

        dealt = dead | villain_hand;
        
        board = start_board;
        for (int j = 0; j < (5 - num_board); j++) {
            board |= deal_card(board | dealt);
        }
        hero = evaluate(board | hand, 7);
        villain = evaluate(board | villain_hand, 7);

        if (hero > villain) {
            count += 2;
        }
        else if (hero == villain) {
            count += 1;
        }
    }
    return 0.5 * (double)count / (double)iterations;
}
float hand_vs_range_monte_carlo(unsigned long long hand,
        unsigned long long villain_range[],
        int num_villain_range,
        unsigned long long start_board,
        int iterations) {

    return hand_vs_range_monte_carlo(hand, villain_range, num_villain_range, 
        start_board, __builtin_popcountll(start_board), iterations); // needs gcc
}


float hand_vs_random_monte_carlo(unsigned long long hand,
        unsigned long long start_board,
        int num_board,
        int iterations) {
    unsigned int count = 0;
    unsigned long long dealt;
    unsigned int hero;
    unsigned int villain;
    unsigned long long villain_hand;
    unsigned long long board;

    for (int i = 0; i < iterations; i++) {
        villain_hand = 0;
        for (int j = 0; j < 2; j++) {
            villain_hand |= deal_card(hand | start_board);
        }

        dealt = hand | villain_hand;
        
        board = start_board;
        for (int j = 0; j < (5 - num_board); j++) {
            board |= deal_card(board | dealt);
        }
        hero = evaluate(board | hand, 7);
        villain = evaluate(board | villain_hand, 7);

        if (hero > villain) {
            count += 2;
        }
        else if (hero == villain) {
            count += 1;
        }
    }
    return 0.5 * (double)count / (double)iterations;
}

void hand_vs_hand_exact_iterate(unsigned long long hand,
        unsigned long long villain_hand,
        unsigned long long board,
        int num_board,
        int num_card,
        unsigned long long dead,
        unsigned int& count,
        unsigned int& total) {

    if (num_board == 0) {

        unsigned int hero = evaluate(board | hand, 7);
        unsigned int villain = evaluate(board | villain_hand, 7);

        if (hero > villain) {
            count += 2;
        }
        else if (hero == villain) {
            count += 1;
        }

        total++;
    }
    else {
        for (int i = num_board-1; i < num_card; i++) {
            if ((CARD_MASKS_TABLE[i] & dead) == 0) {
                hand_vs_hand_exact_iterate(hand, villain_hand, board | CARD_MASKS_TABLE[i],
                    num_board-1, i, dead | CARD_MASKS_TABLE[i], count, total);
            }
        }
    }

}

void hand_vs_range_exact_iterate(unsigned long long hand,
        unsigned long long villain_range[],
        int num_villain_range,
        unsigned long long board,
        int num_board,
        int num_card,
        unsigned long long dead,
        unsigned int& count,
        unsigned int& total) {

    if (num_board == 0) {

        unsigned int hero = evaluate(board | hand, 7);
        unsigned int villain;

        for (int i = 0; i < num_villain_range; i++) {

            if ((dead & villain_range[i]) == 0) {

                villain = evaluate(board | villain_range[i], 7);

                if (hero > villain) {
                    count += 2;
                }
                else if (hero == villain) {
                    count += 1;
                }

                total++;

            }

        }
    }
    else {
        for (int i = num_board-1; i < num_card; i++) {
            if ((CARD_MASKS_TABLE[i] & dead) == 0) {
                hand_vs_range_exact_iterate(hand, villain_range, num_villain_range, 
                    board | CARD_MASKS_TABLE[i], num_board-1, i, dead | CARD_MASKS_TABLE[i],
                    count, total);
            }
        }
    }

}

float hand_vs_range_exact(unsigned long long hand,
        unsigned long long full_villain_range[],
        int num_full_villain_range,
        unsigned long long start_board,
        int num_board,
        unsigned long long common_dead,
        unsigned long long villain_dead) {
    // common_dead = cards that cannot appear on the board or in villain's range
    // villain_dead = cards that can appear on the board but not in villain's range

    unsigned long long villain_range[num_full_villain_range];
    int num_villain_range;
    filter_range(hand | start_board | common_dead | villain_dead,
            full_villain_range, num_full_villain_range,
            villain_range, num_villain_range);
    
    unsigned int count = 0, total = 0;
    hand_vs_range_exact_iterate(hand, villain_range,
                                num_villain_range, start_board,
                                5-num_board, 52,
                                hand | start_board | common_dead,
                                count, total);

    return 0.5 * (double)count / (double)total;
}
