#include "game.h"

random_device GAME_RD;
mt19937 GAME_GEN(GAME_RD());
uniform_real_distribution<> GAME_RAND(0, 1);

ostream& print_action(ostream& os, int action) {
    if (action == FOLD) {
        return os << "fold";
    }
    else if (action == CHECK_CALL) {
        return os << "check/call";
    }
    else if (action >= BET && action < RAISE) {
        float bet_val = BET_SIZES[action-BET];
        if (bet_val == 100) {
            return os << "all-in";
        }
        else {
            return os << "bet[" << bet_val << "pot]";
        }
    }
    else if (action >= RAISE) {
        float raise_val = RAISE_SIZES[action-RAISE];
        if (raise_val == 100) {
            return os << "all-in";
        }
        else {
            return os << "raise[" << raise_val << "pot]";
        }
    }
    else {
        throw runtime_error("Unknown action");
    }
}

int BoardActionHistory::bet_action_to_pip(int action) const {
    assert(action >= BET && action < RAISE + RAISE_SIZES.size());

    vector<float> sizes;
    int action_offset;
    int min_bet;
    // action is a bet
    if (action < RAISE) {
        sizes = BET_SIZES;
        action_offset = BET;
        min_bet = BIG_BLIND;
    }
    // action is a raise
    else {
        sizes = RAISE_SIZES;
        action_offset = RAISE;
        min_bet = pip[1-ind] - pip[ind];
    }

    int shove = stack[ind]-pip[1-ind];
    int bet_amount = min(shove, (int) round((pot + 2*pip[1-ind]) * sizes[action-action_offset]));

    // round up to minimum bet (unless a shove)
    // 1 BB for betting, previous bet for raise
    if (bet_amount < min_bet && bet_amount != shove) {
        bet_amount = min_bet;
    }

    return pip[1-ind] + bet_amount;

}

void BoardActionHistory::update(int new_action) {
    assert(in_vector(get_available_actions(), new_action));
    bool end_street = false;
        
    //// handle stacks and pips
    if (new_action == CHECK_CALL) {
        // calling pre-flop on the button
        if (actions.size() == 0) {
            pip[ind] = pip[1-ind];
        }
        // end street and process call
        else if (pip[ind] < pip[1-ind] || (actions.size() == 1 && pip[ind] == pip[1-ind])) {
            int called = min(stack[ind], pip[1-ind]);
            pot += 2*called;
            for (int i = 0; i < 2; i++) {
                stack[i] -= called;
                pip[i] = 0;
            }
            end_street = true;
                
            // if calling puts us all in jump to evaluating hands
            if (stack[ind] == 0 || stack[1-ind] == 0) {
                finished = true;
                street = 3;
            }
        }
        // or end street on button check
        else if (ind == button) {
            end_street = true;
        }
    }
    // if player folds, give pot to other player and process value
    else if (new_action == FOLD) {
        finished = true;
        pot += pip[ind];
        stack[ind] -= pip[ind];
        stack[1-ind] += pot;
    }
    // process bets and raises
    else if (new_action >= BET && new_action < RAISE + RAISE_SIZES.size()) {
        pip[ind] = bet_action_to_pip(new_action);
    }
    else {
        throw runtime_error("Unknown action");
    }

    //// add action to log
    actions.push_back(new_action);
    // if end of a street, update index for new street
    if (end_street) {
        if (street < 3) {
            action_starts[street] = actions.size();
        }
        else {
            // evaluate end of hand
            if (winner == -1) {
                stack[0] += pot/2;
                stack[1] += pot/2;
            }
            else {
                stack[winner] += pot;
            }
            finished = true;
            showdown = true;
        }
        street++;

        // go back to the non-button player if street ends
        ind = 1-button;
    }
    // pass turn to next player if street not over
    else {
        ind = 1-ind;
    }

    //// calculate amounts won when hand is finished
    if (finished) {
        for (int i = 0; i < 2; i++) won[i] = stack[i] - STARTING_STACK - ante/2;
    }

    //// keep record of integer key for infoset dictionary
    action_key |= ((ULL) new_action << key_shift);
    key_shift += SHIFT_ACTIONS;

}

vector<int> BoardActionHistory::get_available_actions() const {
    vector<int> possible_actions;

    // no actions if board has been folded
    if (finished) {
        return possible_actions;
    }

    // can check / call if not / facing a bet
    possible_actions.push_back(CHECK_CALL);

    // only "check" down if we don't have a stack
    if (stack[ind] == 0) {
        return possible_actions;
    }

    // set up bet/raise logic
    vector<float> sizes;
    int action_offset;
    // can also fold or raise if facing a bet
    if (pip[1-ind] > 0) {
        assert(pip[ind] <= pip[1-ind]);

        if (pip[ind] < pip[1-ind]) {
            possible_actions.push_back(FOLD);
        }

        sizes = RAISE_SIZES;
        action_offset = RAISE;

    }
    // can bet if not facing a bet
    else {
        sizes = BET_SIZES;
        action_offset = BET;
    }

    // put bets/raises onto available action vector
    // can only bet/raise if calling =/= all-in
    int current_pip;
    int previous_pip = -1;
    if (pip[1-ind] < stack[ind]) {
        for (int i = 0; i < sizes.size(); i++) {
            current_pip = bet_action_to_pip(action_offset + i);

            // only add action if the pip amount differs from the 
            // previous pip amount (relies on bet sizes being sorted).
            // Avoids listing multiple actions that are all just shoves
            // as well as small bets that round to the same value
            if (current_pip != previous_pip) {
                possible_actions.push_back(action_offset + i);
            }

            previous_pip = current_pip;
        }
    }

    return possible_actions;
}

// deal board to the river and two hands (as they are on each street, with a 
// possibility to swap cards before each street)
void deal_game_swaps(
    array<int, BOARD_SIZE> &board,
    array<array<int, HAND_SIZE>, NUM_STREETS> &c1,
    array<array<int, HAND_SIZE>, NUM_STREETS> &c2,
    array<float, NUM_STREETS-1> swap_odds) {
    ULL dead = 0;
    int ind;

    // deal complete board
    int i = 0;
    while (i < BOARD_SIZE) {
        ind = sample_card_dist();
        if ((dead & CARD_MASKS_TABLE[ind]) == 0) {
            board[i] = ind;
            dead |= CARD_MASKS_TABLE[ind];
            i++;
        }
    }

    // deal hands
    for (int i = 0; i < NUM_STREETS; i++) {
        // check dealing on each street
        for (int j = 0; j < 2*HAND_SIZE; j++) {
            // if pre-flop or if a swap rolls, deal a new card
            if (i == 0 || GAME_RAND(GAME_GEN) < swap_odds[i-1]) {

                // keep drawing until we get a new card
                ind = sample_card_dist();
                while ((CARD_MASKS_TABLE[ind] & dead) != 0) {
                    ind = sample_card_dist();
                }

                // first two cards to player 1, last two to player 2
                if (j < HAND_SIZE) {
                    c1[i][j] = ind;
                }
                else {
                    c2[i][j - HAND_SIZE] = ind;
                }

                dead |= CARD_MASKS_TABLE[ind];
            }
            // else use the same card from previous street
            else {
                if (j < HAND_SIZE) {
                    c1[i][j] = c1[i-1][j];
                }
                else {
                    c2[i][j - HAND_SIZE] = c2[i-1][j - HAND_SIZE];
                }
            }

        }
    }

    return;
}

// deal a board to the river and two hands
void deal_game(
    array<int, BOARD_SIZE> &board,
    array<int, HAND_SIZE> &c1, array<int, HAND_SIZE> &c2) {
    array<array<int, HAND_SIZE>, NUM_STREETS> c1_full;
    array<array<int, HAND_SIZE>, NUM_STREETS> c2_full;

    array<float, NUM_STREETS-1> swap_odds;
    for (int i = 0; i < NUM_STREETS-1; i++) {
        swap_odds[i] = 0;
    }

    deal_game_swaps(board, c1_full, c2_full, swap_odds);

    c1 = c1_full[0];
    c2 = c2_full[0];
}