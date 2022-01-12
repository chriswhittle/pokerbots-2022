#include <cassert>

#include "game.h"
#include "gametree.h"
#include <bitset>
using namespace std;

void test_immediate_fold() {
    int ante = 2*BIG_BLIND;
    BoardActionHistory history(0, -1, ante);
    // cout << history << endl;
    // print_actions(cout, history.get_available_actions());
    // cout << endl;
    history.update(FOLD);
    // cout << history << endl;
    // print_actions(cout, history.get_available_actions());
    // cout << endl;
    assert(history.finished);
    assert(history.won[0] == (int)(-0.5*BIG_BLIND - ante/2));
    assert(history.won[1] == (int)(0.5*BIG_BLIND + ante/2));
    cout << "\033[0;32m[PASSED test_immediate_fold]\033[0m" << endl;
}

void test_immediate_fold2() {
    int ante = 5*BIG_BLIND;
    BoardActionHistory history(0, -1, ante);
    history.update(FOLD);
    assert(history.finished);
    assert(history.won[0] == (int)(-0.5*BIG_BLIND - ante/2));
    assert(history.won[1] == (int)(0.5*BIG_BLIND + ante/2));
    cout << "\033[0;32m[PASSED test_immediate_fold2]\033[0m" << endl;
}

void test_limp_checkdown() {
    int ante = 2*BIG_BLIND;
    int winner = 1;
    BoardActionHistory history(0, winner, ante);
    history.update(CHECK_CALL);
    assert(!history.finished);
    assert(history.street == 0);
    assert(history.ind == 1);
    // BB gets the option
    history.update(CHECK_CALL);
    assert(!history.finished);
    assert(history.street == 1);
    assert(history.ind == 1);
    history.update(CHECK_CALL);
    assert(!history.finished);
    assert(history.street == 1);
    assert(history.ind == 0);
    history.update(CHECK_CALL);
    assert(!history.finished);
    assert(history.street == 2);
    assert(history.ind == 1);
    history.update(CHECK_CALL);
    assert(!history.finished);
    assert(history.street == 2);
    assert(history.ind == 0);
    history.update(CHECK_CALL);
    assert(!history.finished);
    assert(history.street == 3);
    assert(history.ind == 1);
    history.update(CHECK_CALL);
    assert(!history.finished);
    assert(history.street == 3);
    assert(history.ind == 0);
    history.update(CHECK_CALL);
    assert(history.finished);
    
    assert(history.won[0] == (int)(-BIG_BLIND-ante/2));
    assert(history.won[1] == (int)(BIG_BLIND+ante/2));
    cout << "\033[0;32m[PASSED test_limp_checkdown]\033[0m" << endl;
}

void test_bb_raise_fold() {
    int ante = 2*BIG_BLIND;
    BoardActionHistory history(0, -1, ante);
    assert(history.pot == ante);
    assert(history.pip[0] == BIG_BLIND/2);
    assert(history.pip[1] == BIG_BLIND);
    history.update(CHECK_CALL);
    assert(history.street == 0);
    assert(history.ind == 1);
    assert(history.pip[0] == BIG_BLIND);
    assert(history.pip[1] == BIG_BLIND);
    assert(history.pot == ante);
    history.update(RAISE); // pot raise
    assert(history.street == 0);
    assert(history.ind == 0);
    assert(history.pip[0] == BIG_BLIND);
    assert(history.pip[1] == 5*BIG_BLIND);
    assert(history.pot == ante);
    history.update(CHECK_CALL);
    assert(history.street == 1);
    assert(history.ind == 1);
    assert(history.pot == ante + 10*BIG_BLIND);
    assert(history.pip[0] == 0);
    assert(history.pip[1] == 0);
    history.update(BET+1); // 2/3 pot bet
    assert(history.ind == 0);
    assert(history.pip[0] == 0);
    assert(history.pip[1] == 8*BIG_BLIND);
    history.update(FOLD);
    
    assert(history.finished);
    assert(history.won[0] == -6*BIG_BLIND);
    assert(history.won[1] == 6*BIG_BLIND);
    cout << "\033[0;32m[PASSED test_bb_raise_fold]\033[0m" << endl;
}

void test_allin_fold() {
    int ante = 2*BIG_BLIND;
    BoardActionHistory history(0, -1, ante);
    history.update(RAISE+1); // all-in
    assert(history.street == 0);
    assert(history.ind == 1);
    assert(history.pip[0] == STARTING_STACK);
    assert(history.pip[1] == BIG_BLIND);
    assert(history.pot == ante);
    history.update(FOLD);

    assert(history.finished);
    assert(history.won[0] == 2*BIG_BLIND);
    assert(history.won[1] == -2*BIG_BLIND);
    cout << "\033[0;32m[PASSED test_allin_fold]\033[0m" << endl;
}


void test_allin_fold2() {
    int ante = 2*BIG_BLIND;
    BoardActionHistory history(0, -1, ante);
    history.update(CHECK_CALL); // all-in
    history.update(RAISE+1); // all-in
    assert(history.street == 0);
    assert(history.ind == 0);
    assert(history.pip[0] == BIG_BLIND);
    assert(history.pip[1] == STARTING_STACK);
    assert(history.pot == ante);
    history.update(FOLD);

    assert(history.finished);
    assert(history.won[0] == -2*BIG_BLIND);
    assert(history.won[1] == 2*BIG_BLIND);
    cout << "\033[0;32m[PASSED test_allin_fold2]\033[0m" << endl;
}

void check_card_dist() {
    array<double, 52> hist;
    for (int i = 0; i < 52; ++i) {
        hist[i] = -1./52;
    }
    const int n_samples = 100000;
    for (int i = 0; i < n_samples; ++i) {
        int c = sample_card_dist();
        hist[c] += 1./n_samples;
    }
    cout << "card dist deviations: " << hist << endl;
}

void traverse_game_tree(GameTreeNode node, set<ULL> &keys, int &msb) {

    assert(keys.find(node.history_key) == keys.end());
    keys.insert(node.history_key);

    // keep track of maximum number of bits used in infoset key
    int cur_msb = 0;
    ULL n = node.history_key;
    while (n != 0) {
        n /= 2;
        cur_msb++;
    }
    if (cur_msb > msb) {
        msb = cur_msb;
    }

    for (int i = 0; i < node.children.size(); i++) {
        traverse_game_tree(node.children[i], keys, msb);
    }

}

void test_unique_action_keys() {
    
    set<ULL> keys;
    BoardActionHistory history(0, 0, 0);
    GameTreeNode root = build_game_tree(history);
    int msb = 0;

    traverse_game_tree(root, keys, msb);
    
    cout << "\033[0;32m[PASSED test_unique_action_keys with "
        << keys.size() << " keys (" << (msb) << " bits used at most)]\033[0m" << endl;
}

// checks specific to swap hold 'em

void test_deal_swaps() {

    array<int, BOARD_SIZE> board;
    array<array<int, HAND_SIZE>, NUM_STREETS> c1;
    array<array<int, HAND_SIZE>, NUM_STREETS> c2;
    ULL card;

    const int n_samples = 100000;

    array<int, NUM_STREETS-1> swap_count;
    for (int i = 0; i < NUM_STREETS-1; i++) swap_count[i] = 0;

    for (int i = 0; i < n_samples; i++) {
        deal_game_swaps(board, c1, c2, SWAP_ODDS);

        // check board collisions
        ULL dealt = 0;
        for (int j = 0; j < BOARD_SIZE; j++) {
            card = CARD_MASKS_TABLE[board[j]];
            assert((card & dealt) == 0);
            dealt |= card;
        }

        // check player 1 collisions
        for (int j = 0; j < NUM_STREETS; j++) {
            for (int k = 0; k < HAND_SIZE; k++) {
                card = CARD_MASKS_TABLE[c1[j][k]];

                if (c1[j][k] != c1[j-1][k]) {
                    swap_count[j-1]++;
                }

                assert((card & dealt) == 0 || c1[j][k] == c1[j-1][k]);
                dealt |= card;
            }
        }
        
        // check player 2 collisions
        for (int j = 0; j < NUM_STREETS; j++) {
            for (int k = 0; k < HAND_SIZE; k++) {
                card = CARD_MASKS_TABLE[c2[j][k]];

                if (c2[j][k] != c2[j-1][k]) {
                    swap_count[j-1]++;
                }

                assert((card & dealt) == 0 || c2[j][k] == c2[j-1][k]);
                dealt |= card;
            }
        }

    }

    cout << "\033[0;32m[PASSED test_deal_swaps]\033[0m" << endl;

    // divide by 4 since 2 players each have 2 cards = 4 chances to swap
    cout << "Swap fractions: ";
    for (int i = 0; i < NUM_STREETS-1; i++)
        cout << static_cast <float> (swap_count[i]) / n_samples / 4 << ", ";
    cout << endl;

}

int main() {
    cout << "== Running all game tests ==" << endl;

    // tests
    test_immediate_fold();
    test_immediate_fold2();
    test_limp_checkdown();
    test_bb_raise_fold();
    test_allin_fold();
    test_allin_fold2();
    test_unique_action_keys();

    // visual checks
    check_card_dist();

    // swap hold 'em checks
    test_deal_swaps();

    cout << "\033[1;32m[PASSED ALL TESTS]\033[0m" << endl;

    return 0;
}