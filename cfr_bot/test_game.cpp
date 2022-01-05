#include <cassert>

#include "game.h"

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
    history.update(BET); // 2/3 pot bet
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

int main() {
    cout << "== Running all game tests ==" << endl;

    // tests
    test_immediate_fold();
    test_immediate_fold2();
    test_limp_checkdown();
    test_bb_raise_fold();
    test_allin_fold();
    test_allin_fold2();

    // visual checks
    check_card_dist();
    return 0;
}
