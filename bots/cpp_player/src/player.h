#ifndef REAL_POKER_PLAYER
#define REAL_POKER_PLAYER

// include after skeleton

#include <string>

#include "game.h"

// converting between engine cards and internal logic cards
const array<char, NUM_RANKS> RANKS = {'2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J', 'Q', 'K', 'A'};
const array<char, NUM_SUITS> SUITS = {'s', 'h', 'd', 'c'};
using Card = std::string;

int card_string_to_index(Card c) {
    return NUM_RANKS*distance(SUITS.begin(), find(SUITS.begin(), SUITS.end(), c.at(1)))
             + distance(RANKS.begin(), find(RANKS.begin(), RANKS.end(), c.at(0)));
}

Card card_index_to_string(int c) {
    return string(1,RANKS[c%NUM_RANKS]) + SUITS[c/NUM_RANKS];
}

// nice action printing
ostream& operator<<(ostream& os, Action& p) {
    switch (p.actionType) {
        case Action::RAISE:
            os << "raise to " << p.amount;
            break;
        case Action::CALL:
            os << "call";
            break;
        case Action::CHECK:
            os << "check";
            break;
        case Action::FOLD:
            os << "fold";
            break;
    }
    return os;
}


// decides whether to map value to upper or lower bound. 
// returns true if value should be mapped to lower bound. 
bool harmonic_analysis(float lower_bound, float upper_bound, float value_to_map) {
    float mapping_prob = ((upper_bound - value_to_map)*(1 + lower_bound))
                                    /((upper_bound - lower_bound)*(1 + value_to_map));
    float r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    cout << "Rounding action to lower size with probability "
        << mapping_prob << ": " << ((r < mapping_prob) ? "SUCCESS" : "FAILURE") << endl;
    return r < mapping_prob;
}

// decides whether to map value to upper or lower bound. 
// returns true if value should be mapped to lower bound. 
bool rounding_analysis(float lower_bound, float upper_bound, float value_to_map) {
    return value_to_map < (lower_bound + upper_bound)/2;
}

// map a player bet/raise onto our action tree, given engine bet/raise size
// (above call) and engine pot size (including call value)
int map_bet_to_infoset_bet(int bet, int pot, const BoardActionHistory& history) {
    bool is_bet = (history.pip[1-history.ind] == 0); // according to internal game state
    int act_type = is_bet ? BET : RAISE;
    auto& act_sizes = is_bet ? BET_SIZES : RAISE_SIZES;

    vector<int> available_actions = history.get_available_actions();
    vector<float> bet_sizes = {0.0};
    vector<int> bet_actions = {CHECK_CALL};
    
    // get pot size from internal history
    int cfr_pot = history.pot + 2*history.pip[1-history.ind];
    for (int act : available_actions) {
        if (act >= act_type && act < act_type + act_sizes.size()) {
            // get internal bet sizing;
            // subtract off our pip since we only care about the raise part
            // (i.e. pip beyond a call) relative to the pot
            int cfr_bet = history.bet_action_to_pip(act) - history.pip[1-history.ind];
            float bet_size = (float) cfr_bet / cfr_pot;

            bet_sizes.push_back(bet_size);
            bet_actions.push_back(act);
        }
    }
    assert(bet_sizes.size() > 0);
    if (bet_sizes.size() == 1) {
        return bet_actions[0];
    }

    // do the mapping based on bet relative to pot size
    float relative_bet = bet / (float)pot;
    float bet_below = -1;
    int act_below = -1;
    for (int i = 0; i < bet_sizes.size(); ++i) {
        if (bet_sizes[i] <= relative_bet) {
            bet_below = bet_sizes[i];
            act_below = bet_actions[i];
        }
    }

    assert(bet_below != -1); // should always have the check/call option
    
    float bet_above = -1;
    int act_above = -1;
    for (int i = bet_sizes.size()-1; i >= 0; --i) {
        if (bet_sizes[i] > relative_bet) {
            bet_above = bet_sizes[i];
            act_above = bet_actions[i];
        }
    }

    if (bet_above == -1) { // bet above all available options, return all-in
        return bet_actions[bet_actions.size()-1];
    }

    if (harmonic_analysis(bet_below, bet_above, relative_bet)) {
    // if (rounding_analysis(bet_below, bet_above, relative_bet)) {
        return act_below;
    }
    else {
        return act_above;
    }
}

int new_card_infostate(int street,
    array<int, HAND_SIZE> hand_cards,
    vector<int> board_cards,
    const DataContainer &data,
    int n_mc_iter,
    ULL common_dead,
    ULL villain_dead) {
    // update our card infostate
    // (only need to do for flop, turn, river since 
    // preflop is done at start of round)
    // street is according to the engine
    if (street == 3) {

        array<int, FLOP_SIZE> board_cards_array;
        copy_n(board_cards.begin(), FLOP_SIZE, board_cards_array.begin());

        return get_cards_info_state_flop(hand_cards, board_cards_array, data);
        
        // calculate bucket on the fly so we can take into account
        // dead cards
        // return get_bucket_from_clusters(
        //     indices_to_mask(hand_cards),
        //     indices_to_mask(board_cards_array),
        //     FLOP_SIZE, data.flop_clusters, n_mc_iter,
        //     common_dead, villain_dead);
    }
    else if (street == 4) {

        array<int, TURN_SIZE> board_cards_array;
        copy_n(board_cards.begin(), TURN_SIZE, board_cards_array.begin());

        return get_bucket_from_clusters(
            indices_to_mask(hand_cards),
            indices_to_mask(board_cards_array),
            TURN_SIZE, data.turn_clusters, n_mc_iter,
            common_dead, villain_dead);
    }
    else if (street == 5) {

        array<int, BOARD_SIZE> board_cards_array;
        copy_n(board_cards.begin(), BOARD_SIZE, board_cards_array.begin());

        // do river evaluation exactly
        return get_bucket_from_clusters(
            indices_to_mask(hand_cards),
            indices_to_mask(board_cards_array),
            BOARD_SIZE, data.river_clusters, 0,
            common_dead, villain_dead);
    }

    cout << "WARNING: invalid street to calculate card infostate on: " << street << endl;
    return 0;
}

// infer villain's last action (assumes action is on us) for internal tree-traversal purposes
// and update card infostates
void update_internal_tracking(
    int pip, int villain_pip,
    int pot, int street,
    BoardActionHistory &history) {

    // if the engine street is ahead of our internal state
    // then villain must have closed the action
    if (history.street == 0 && street > 2 ||
        history.street == 1 && street > 3 ||
        history.street == 2 && street > 4) {
        vector<int> possible_actions = history.get_available_actions();

        if (in_vector(possible_actions, CHECK_CALL)) {
            history.update(CHECK_CALL);
            cout << "Assume opponent check/called to catch up." << endl;
        }
        else {
            cout << "WARNING: CFR state said opponent couldn't close action, but we're behind." << endl;
        }
    }

    // check if our internal state is expecting another action
    // from opponent
    if (!history.finished && history.ind == 1) {

        if (pip > villain_pip) {
            cout << "WARNING: interpreted fold from opponent but still computing actions." << endl;
            history.update(FOLD);
        }
        else if (pip == villain_pip) {
            cout << "Interpreted opponent check/call." << endl;
            history.update(CHECK_CALL);
        }
        else {
            // int bet_action = map_bet_to_infoset_bet(pip, villain_pip, pot, history);
            int pot_after_call = 2*pip + pot;
            int bet_size = villain_pip - pip;
            int bet_action = map_bet_to_infoset_bet(bet_size, pot_after_call, history);
            history.update(bet_action);

            // logging
            cout << "Interpreted opponent " << ((pip == 0) ? "bet " : "raise to ")
                << villain_pip << " chips -> ";
            if (bet_action == CHECK_CALL) {
                cout << "check/call.";
            }
            else if (bet_action < RAISE) {
                cout << "bet " << BET_SIZES[bet_action-BET] << " * pot.";
            }
            else {
                cout << "raise to " << RAISE_SIZES[bet_action-RAISE] << " * pot.";
            }
            cout << endl;
        }

    }
    
}

// map the action from CFR to a valid action given board state
// uses pips and stacks since the engine's legal actions
// doesn't seem accurate and gets us in trouble
Action map_infoset_action_to_action(int action, int pip,
                                    int villain_pip, int pot,
                                    int stack, int villain_stack) {
    if (action == FOLD) {
        cout << "Action mapping: fold -> fold" << endl;
        return Action::FOLD;
    }
    else if (action == CHECK_CALL) {
        cout << "Action mapping: check/call -> ";
        if (pip < villain_pip) {
            cout << "call" << endl;
            return Action::CALL;
        }
        else {
            cout << "check" << endl;
            return Action::CHECK;
        }
    }
    else if (action >= BET && action < RAISE) {
        cout << "Action mapping: bet " << BET_SIZES[action-BET] << " * pot -> ";

        // don't bet less than a min-bet
        int raise = max({(int) round(pip + (pot + villain_pip*2) * BET_SIZES[action-BET]),
                        BIG_BLIND, 2*villain_pip - pip});
        
        // ensure we don't raise above all-in
        raise = min(raise, min(stack+pip, villain_stack+villain_pip));

        if (villain_pip > 0 && (villain_stack == 0 || stack <= villain_pip)) {
            cout << "call" << endl;
            return Action::CALL;
        }
        else if (villain_stack == 0 && villain_pip == 0) {
            cout << "check";
            return Action::CHECK;
        }
        else {
            cout << "raise to " << raise << " chips" << endl;
            return Action(Action::RAISE, raise);
        }

    }
    else if (action >= RAISE) {
        cout << "Action mapping: raise " << RAISE_SIZES[action-RAISE] << " * pot -> ";

        // don't raise less than a min-raise
        int raise = max({(int) round(villain_pip + (pot + villain_pip*2) * RAISE_SIZES[action-RAISE]),
                        BIG_BLIND, 2*villain_pip - pip});
        
        // ensure we don't raise above all-in
        raise = min(raise, min(stack+pip, villain_stack+villain_pip));

        if (villain_stack == 0 || stack <= villain_pip) {
            cout << "call" << endl;
            return Action::CALL;
        }
        else {
            cout << "raise to " << raise << " chips" << endl;
            return Action(Action::RAISE, raise);
        }

    }
    
    cout << "WARNING: unexpected CFR action." << endl;
    return Action::CHECK;
}

#endif
