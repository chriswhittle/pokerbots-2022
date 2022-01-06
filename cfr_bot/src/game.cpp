#include "game.h"

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
    // process bets
    else if (new_action >= BET && new_action < RAISE) {
        pip[ind] = min(stack[ind], (int) (pot * BET_SIZES[new_action-BET]));
    }
    // process raises
    else if (new_action >= RAISE) {
        int shove = stack[ind]-pip[1-ind];
        int raise_amount = min(shove, (int) ((pot + 2*pip[1-ind]) * RAISE_SIZES[new_action-RAISE]));
        // round up to min-raise (unless it's a shove)
        if ((raise_amount < (pip[1-ind] - pip[ind])) && (raise_amount != shove)) {
            raise_amount = pip[1-ind] - pip[ind];
        }
        pip[ind] = pip[1-ind] + raise_amount;
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
    key_shift += ACTION_BIT_SHIFT;

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

    // can also fold or raise if facing a bet
    if (pip[1-ind] > 0) {
        assert(pip[ind] <= pip[1-ind]);
        if (pip[ind] < pip[1-ind]) {
            possible_actions.push_back(FOLD);
        }
        if (pip[1-ind] < stack[ind]) { // can raise if calling =/= all-in
            for (int i = 0; i < RAISE_SIZES.size(); i++) {
                possible_actions.push_back(RAISE+i);
                // if our stack is smaller than the raise size,
                // just list one raise action
                if (((int) RAISE_SIZES[i]*(pot + 2*pip[1-ind])) + pip[ind] > stack[ind]) {
                    break;
                }
            }
        }
    }
    // can bet if not facing a bet
    else if (pip[1-ind] < stack[ind]) {
        for (int i = 0; i < BET_SIZES.size(); i++) {
            possible_actions.push_back(BET+i);
        }
    }

    return possible_actions;
}

// deal a board to the river and two hands
void deal_game(
    array<int, BOARD_SIZE> &board,
    array<int, HAND_SIZE> &c1, array<int, HAND_SIZE> &c2) {
    ULL dead = 0;
    int ind;

    // deal hands
    int i = 0;
    while (i < 2*HAND_SIZE) {
        ind = sample_card_dist();
        if ((dead & CARD_MASKS_TABLE[ind]) == 0) {
            if (i < HAND_SIZE) {
                c1[i] = ind;
            }
            else {
                c2[i - HAND_SIZE] = ind;
            }

            dead |= CARD_MASKS_TABLE[ind];
            i++;
        }
    }

    ULL cur_dead;

    // deal complete board
    int i = 0;

    while (i < BOARD_SIZE) {
        ind = sample_card_dist();
        if ((cur_dead & CARD_MASKS_TABLE[ind]) == 0) {
            board[i] = ind;
            cur_dead |= CARD_MASKS_TABLE[ind];
            i++;
        }
    }

    return;
}
