#include <iostream>

#include "eval7pp.h"

int main() {

    int N_ITER = 100;

    /////// evaluating hands
    std::cout << "==Evaluating hands==\n";
    // strength of ace high
    std::cout << "A high: " << evaluate(CARD_MASKS_TABLE[12]) << "\n";
    // strength of aces
    std::cout << "AA: " << evaluate(CARD_MASKS_TABLE[12] | CARD_MASKS_TABLE[11]) << "\n";
    // strength of a straight
    std::cout << "23456: " << evaluate(CARD_MASKS_TABLE[0] | CARD_MASKS_TABLE[1]
                         | CARD_MASKS_TABLE[2] | CARD_MASKS_TABLE[3] | CARD_MASKS_TABLE[17]) << "\n";
    // strength of a straight flush
    std::cout << "23456s: " << evaluate(CARD_MASKS_TABLE[0] | CARD_MASKS_TABLE[1]
                         | CARD_MASKS_TABLE[2] | CARD_MASKS_TABLE[3] | CARD_MASKS_TABLE[4]) << "\n";
    std::cout << "\n";

    /////// hand vs hand equity (exact)
    unsigned long long aces = CARD_MASKS_TABLE[12] | CARD_MASKS_TABLE[25];
    unsigned long long kings = CARD_MASKS_TABLE[11] | CARD_MASKS_TABLE[24];
    unsigned long long board = CARD_MASKS_TABLE[37] | CARD_MASKS_TABLE[0] | CARD_MASKS_TABLE[14];

    std::cout << "==Hand vs hand equity (exact)==\n";
    // equity of aces over kings
    std::cout << "KK vs AA (preflop): " << hand_vs_hand_exact(kings, aces, 0) << "\n";
    // equity of aces over kings with a king on the flop
    std::cout << "KK vs AA on K32: " << hand_vs_hand_exact(kings, aces, board) << "\n";
    std::cout << "\n";

    /////// hand vs hand equity (MC)
    std::cout << "==Hand vs hand equity (MC)==\n";
    // equity of aces over kings
    std::cout << "KK vs AA (preflop): " << hand_vs_hand_monte_carlo(kings, aces, 0, N_ITER) << "\n";
    // equity of aces over kings with a king on the flop
    std::cout << "KK vs AA on K32: " << hand_vs_hand_monte_carlo(kings, aces, board, N_ITER) << "\n";
    std::cout << "\n";
    
    
    /////// hand vs range equity (exact)
    unsigned long long queens = CARD_MASKS_TABLE[10] | CARD_MASKS_TABLE[23];
    unsigned long long range[2] = {queens, aces};
    int num_range = sizeof(range)/sizeof(range[0]);
    unsigned long long low_board = CARD_MASKS_TABLE[0] | CARD_MASKS_TABLE[1] | CARD_MASKS_TABLE[14];

    std::cout << "==Hand vs range equity (exact)==\n";
    // equity of aces over kings
    std::cout << "KK vs {QQ,AA} (preflop): " << hand_vs_range_exact(kings, range, num_range, 0) << "\n";
    // equity of aces over kings with a king on the flop
    std::cout << "KK vs {QQ,AA} on 332: " << hand_vs_range_exact(kings, range, num_range, low_board) << "\n";
    std::cout << "\n";
    
    /////// hand vs range equity (MC)
    std::cout << "==Hand vs range equity (MC)==\n";
    // equity of aces over kings
    std::cout << "KK vs {QQ,AA} (preflop): " << hand_vs_range_monte_carlo(kings, range, num_range, 0, N_ITER) << "\n";
    // equity of aces over kings with a king on the flop
    std::cout << "KK vs {QQ,AA} on 332: " << hand_vs_range_monte_carlo(kings, range, num_range, low_board, N_ITER) << "\n";
    std::cout << "\n";

    /////// hand vs random hand equity (MC)
    N_ITER = 10000;
    std::cout << "==Hand vs random hand equity (MC)==\n";
    // equity of kings against a random hand
    std::cout << "AA: " << hand_vs_random_monte_carlo(aces, 0, N_ITER) << "\n";
    std::cout << "KK: " << hand_vs_random_monte_carlo(kings, 0, N_ITER) << "\n";
    std::cout << "AKs: " << hand_vs_random_monte_carlo(CARD_MASKS_TABLE[12] | CARD_MASKS_TABLE[11], 0, N_ITER) << "\n";
    std::cout << "KK on 332: " << hand_vs_random_monte_carlo(kings, low_board, N_ITER) << "\n";
    std::cout << "72o: " << hand_vs_random_monte_carlo(CARD_MASKS_TABLE[0] | CARD_MASKS_TABLE[18], 0, N_ITER) << "\n";
}
