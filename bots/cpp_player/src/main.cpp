#include <chrono>
#include <memory>

#include <skeleton/actions.h>
#include <skeleton/constants.h>
#include <skeleton/runner.h>
#include <skeleton/states.h>

#include "cfr.h"
#include "eval7pp.h"

using namespace pokerbots::skeleton;
using namespace std::chrono;

#include "player.h"

const int N_MC_ITER = 10000;
const bool VERBOSE = false;

struct Bot {
    int net_lead = 0;
    int rounds_played = 0;
    bool check_fold_win = false;

    array<array<int, HAND_SIZE>, NUM_BOARDS_> hands_indices;
    array<int, NUM_BOARDS> card_infostates;
    array<int, NUM_BOARDS> card_infostates_street = {0};
    
    ULL common_dead = 0;
    ULL villain_dead = 0;
    array<vector<int>, NUM_BOARDS> board_cards;

    DataContainer data;
    InfosetDict infosets;

    array<BoardActionHistory, NUM_BOARDS> histories;

    time_point<high_resolution_clock> _start;
    double _bot_time = 0.0;

    Bot() : data("data/alloc_buckets.txt",
                 "data/preflop_equities.txt",
                 "data/flop_buckets.txt",
                 "data/turn_clusters.txt",
                 "data/river_clusters.txt") {
        _start = high_resolution_clock::now();
        load_infosets_from_file("data/infosets.txt", infosets);
        load_clusters_from_file("data/flop_clusters.txt", data.flop_clusters);
    }
    
    void handleNewRound(
            GameInfoPtr gameState, RoundStatePtr roundState, int active) {

        cout << "=====Round " << gameState->roundNum << "=====" << endl;

        auto _timer_start = high_resolution_clock::now();
        for (int i = 0; i < NUM_BOARDS; i++) {
            histories[i] = BoardActionHistory(active, 0, BOARD_ANTES[i]);
        }

        // compute and assign pairings and allocations
        array<int, NUM_CARDS> full_card_indices;
        transform(roundState->hands[active].begin(), roundState->hands[active].end(), full_card_indices.begin(), card_string_to_index);
        allocate_heuristic(full_card_indices, data);
        array<array<int, HAND_SIZE>, NUM_BOARDS_> paired_cards;
        allocate_hands(ALLOCATIONS[0], full_card_indices, paired_cards);
        int alloc_info_state = get_cards_info_state_alloc(paired_cards, data);
        ULL alloc_key = info_to_key(active, 0, alloc_info_state);
        auto alloc_info = fetch_infoset(infosets, alloc_key, NUM_ALLOCATIONS);
        if (VERBOSE) {
            cout << "Alloc strategy: " << alloc_info.cumu_strategy << endl;
            cout << "(visited " << alloc_info.t << " times)" << endl << endl;
        }
        int allocation = alloc_info.get_action_index_avg();
        allocate_hands(ALLOCATIONS[allocation], full_card_indices, hands_indices);

        common_dead = 0;
        villain_dead = 0;
        for (int i = 0; i < NUM_CARDS; i++) {
            // track cards in our hand that can't appear in villain range or on board
            common_dead |= CARD_MASKS_TABLE[full_card_indices[i]];
        }

        for (int i = 0; i < NUM_BOARDS; i++) {
            // calculate initial card infostates
            card_infostates[i] = get_cards_info_state_preflop(hands_indices[i]);
            card_infostates_street[i] = 0;
            // empty our memory of the boards
            board_cards[i].clear();
        }

        auto _timer_end = high_resolution_clock::now();
        _bot_time += duration_cast<microseconds>(_timer_end - _timer_start).count() / 1000000.0;

        /////// log
        if (VERBOSE) {
            cout << "Dealt = ";
            for (int i = 0; i < NUM_CARDS; i++) cout << roundState->hands[active][i] << ", ";
            cout << endl;

            cout << "Paired and ranked = " << hands_indices << endl;

            cout << "Picked allocation = ";
            for (int i = 0; i < NUM_BOARDS; i++) cout << ALLOCATIONS[allocation][i] << ", ";
            cout << endl;
        }
        ///////
    }
    
    void handleRoundOver(
            GameInfoPtr gameState, TerminalStatePtr terminalState, int active) {
        cout << "=====End " << gameState->roundNum << " / " << NUM_ROUNDS << "=====" << endl;
        cout << endl << endl;
        if (gameState->roundNum == NUM_ROUNDS) {
            auto _end = high_resolution_clock::now();
            cout << "Total wall time = "
                 << duration_cast<microseconds>(_end - _start).count() / 1000000.0
                 << endl;
            cout << "Total bot time = " << _bot_time << endl;
        }
    }
    
    std::vector<Action> getActions(
            GameInfoPtr gameState, RoundStatePtr roundState, int active) {
        auto _timer_start = high_resolution_clock::now();
        auto legalActions = roundState->legalActions();
        auto myCards = roundState->hands[active];
        std::vector<Action> proposed_actions;
        array<double, NUM_BOARDS> proposed_action_probs = {0};

        array<unordered_set<Action::Type>, NUM_BOARDS> legal_actions = roundState->legalActions();
        
        array<bool, NUM_BOARDS> still_playing = {false, false, false};

        int stack = roundState->stacks[active];
        int villain_stack = roundState->stacks[1-active];
        int street = roundState->street;

        array<int, NUM_BOARDS> pips = {0};
        array<int, NUM_BOARDS> villain_pips = {0};
        array<int, NUM_BOARDS> pots = {0};

        array<int, NUM_BOARDS> proposed_pips = {0};
        array<int, NUM_BOARDS> continue_costs = {0};

        array<int, NUM_BOARDS> proposed_internal_actions = {0};

        if (VERBOSE) {
            cout << "<<Stacks = " << stack << ", " << villain_stack
                 << "; street = " << street << ">>" << endl << endl;
        }

        // once we see new cards on the boards, add them to our
        // memory of the board cards
        BoardStatePtr bs;
        for (int i = 0; i < NUM_BOARDS; i++) {
            if (street > card_infostates_street[i] && (bs = dynamic_pointer_cast<const BoardState>(roundState->boardStates[i]))) {
                for (int j = card_infostates_street[i]; j < street; j++) {
                    int card_index = card_string_to_index(bs->deck[j]);
                    board_cards[i].push_back(card_index);
                    villain_dead |= CARD_MASKS_TABLE[card_index];
                }
            }
        }

        // main loop for determining action
        for (int i = 0; i < NUM_BOARDS; i++) {

            if (VERBOSE) cout << "[Board " << (i+1) << "]" << endl;

            // return allocated hand for this board
            if (in_set(legal_actions[i], Action::ASSIGN)) {
                array<Card, 2> cur_hand;
                transform(hands_indices[i].begin(), hands_indices[i].end(), cur_hand.begin(), card_index_to_string);
                proposed_actions.push_back(Action(Action::ASSIGN, cur_hand));

                if (VERBOSE) cout << "Assigned " << cur_hand[0] << cur_hand[1] << endl;
                continue;
            }
            
            // if we have enough of a lead check/fold to victory
            if (check_fold_win || (gameState->bankroll - gameState->oppBankroll)
                                    > ((NUM_ROUNDS - gameState->roundNum)*21 + 3)) {

                check_fold_win = true;
                if (in_set(legal_actions[i], Action::CHECK)) {
                      proposed_actions.push_back(Action(Action::CHECK));
                }
                else {
                      proposed_actions.push_back(Action(Action::FOLD));
                }

                cout << "Check/folding to win." << endl;
                continue;

            }

            // opponent folded, board is settled or we're out of chips
            if ((legal_actions[i].size() == 1 && in_set(legal_actions[i], Action::CHECK)
                && std::dynamic_pointer_cast<const TerminalState>(roundState->boardStates[i]))
                || stack == 0) {
                proposed_actions.push_back(Action(Action::CHECK));
                if (VERBOSE) cout << "Folded, board settled or out of chips." << endl;
                continue;
            }

            // if we make it this far we're still playing this board
            still_playing[i] = true;

            // update internal representation of game tree
            // and of cards in hand + on board
            BoardStatePtr bs = dynamic_pointer_cast<const BoardState>(roundState->boardStates[i]);
            int pip = bs->pips[active];
            int villain_pip = bs->pips[1-active];
            int pot = bs->pot;

            pips[i] = pip;
            villain_pips[i] = villain_pip;
            pots[i] = pot;

            // pips can be frozen on equal non-zero values while waiting for other boards
            if ((villain_stack == 0 && pip > 0 && pip == villain_pip)) {
                if (VERBOSE) cout << "Still waiting for other boards." << endl;
            }

            if (VERBOSE) {
                cout << "Pot = " << pot << "; engine pip = " << pip << ", " << villain_pip << endl;
                cout << "Board cards: " << bs->deck << endl;
            }

            // update internal history objects
            update_internal_tracking(pip, villain_pip, pot, street, histories[i]);

            // update card infostate to match engine street
            if (street > card_infostates_street[i]) {
                card_infostates[i] = new_card_infostate(street, hands_indices[i],
                                                        board_cards[i], data, N_MC_ITER,
                                                        // common_dead, villain_dead);
                                                        0, 0);
                card_infostates_street[i] = street;
            }

            // check to see if we're waiting for other boards to finish this street
            if (legal_actions[i].size() == 1 && in_set(legal_actions[i], Action::CHECK)
                || (villain_stack == 0 && villain_pip == 0)) {
                proposed_actions.push_back(Action(Action::CHECK));
                if (VERBOSE) cout << "Waiting for other boards/evaluation." << endl;
                continue;
            }

            vector<int> available_actions = histories[i].get_available_actions();

            // check that our internal state isn't ahead of the engine
            if (histories[i].street == 1 && street < 3 ||
                histories[i].street == 2 && street < 4 ||
                histories[i].street == 3 && street < 5 ||
                histories[i].finished) {
                // could happen if e.g. we check to button, they bet small
                // (which looks like a check => finishes action)
                if (VERBOSE) cout << "CFR ahead of action, check/call to catch up." << endl;
                if (in_set(legal_actions[i], Action::CHECK)) {
                    proposed_actions.push_back(Action::CHECK);
                }
                else if (in_set(legal_actions[i], Action::CALL)) {
                    proposed_actions.push_back(Action::CALL);
                    continue_costs[i] = villain_pip - pip;
                    proposed_pips[i] = continue_costs[i];
                }
                continue;
            }

            // fetch appropriate infoset
            unsigned long long key = info_to_key(histories[i].ind ^ histories[i].button,
                                        i+1, histories[i].street,
                                        card_infostates[i],
                                        histories[i]);
            CFRInfoset& infoset = fetch_infoset(infosets, key, available_actions.size());

            if (infoset.t == 0) {
                if (VERBOSE) cout << "WARNING: No information for this state." << endl;
            }

            if (VERBOSE) {
                cout << "Available actions: ";
                print_actions(cout, available_actions) << endl;
                cout << "Strategy: " << infoset.cumu_strategy << endl;
                cout << "(visited " << infoset.t << " times)" << endl << endl;
            }

            // get proposed action
            int action_index = infoset.get_action_index_avg();
            int action = available_actions[action_index];
            // get probability of this action
            double actions_norm = accumulate(infoset.cumu_strategy.begin(), infoset.cumu_strategy.end(), 0.0);
            if (actions_norm > 0.0) { // do (1 - prob.) since we sort ascending later
                proposed_action_probs[i] = 1 - infoset.cumu_strategy[action_index];
            }
            else {
                proposed_action_probs[i] = 1 - 1. / available_actions.size();
            }

            // convert the proposed action to an action for the engine
            // don't update bet histories in case we re-assign
            // (since can't bet more than opponent stack, we have to
            // leave some behind)
            proposed_actions.push_back(
                map_infoset_action_to_action(action, pip, villain_pip,
                                                pot, stack, villain_stack));
            proposed_internal_actions[i] = action;


            // track proposed chips we want to put in
            continue_costs[i] = villain_pip - pip;
            if (proposed_actions[i].actionType == Action::RAISE) {
                proposed_pips[i] = proposed_actions[i].amount - pip;
            }
            else if (proposed_actions[i].actionType == Action::CALL) {
                proposed_pips[i] = continue_costs[i];
            }

        }

        std::vector<Action> my_actions = proposed_actions;

        // only allowed to make bets such that the opponent can match all of them
        int called_pips = 0;
        for (int i = 0; i < NUM_BOARDS; i++) {
            if (still_playing[i]) {
                if (proposed_actions[i].actionType != Action::FOLD) {
                    called_pips += continue_costs[i];
                }
            }
        }		
		int bet_limit = min(stack, villain_stack + called_pips);

        // if we're putting more chips in than we have:
        // (1) distribute calls
        // (2) distribute raises to at least be min-raises
        // (3) distribute the rest across the raises
        // (currently with no logic to optimize this; just in order of boards)
        if (accumulate(proposed_pips.begin(), proposed_pips.end(), 0) > bet_limit) {
            if (VERBOSE) {
                cout << "~Proposed pip " << proposed_pips << " > stack "
                     << stack << " => tweak actions." << endl;
            }

            int remaining_stack = bet_limit;
            array<int, NUM_BOARDS> raise_amounts = {0};

            auto board_order = sort_indexes<double, NUM_BOARDS>(proposed_action_probs);
            cout << "Prioritizing boards: " << board_order << endl;
            
            // subtract call / continue cost
            for (int ind = 0; ind < NUM_BOARDS; ind++) { // distribute calls by prob. priority
                int i = board_order[ind];
                if (still_playing[i]) {
                    if (proposed_actions[i].actionType != Action::FOLD) {
                        remaining_stack -= continue_costs[i];
                        raise_amounts[i] = continue_costs[i];
                    }
                    else {
                        raise_amounts[i] = -1; // fold
                    }
                }
            }

            if (remaining_stack < 0) {
                if (VERBOSE) cout << "WARNING: Not enough to make all continues." << endl;
            }

            // allocate bets/raises
            // first, put enough chips in to do the minimum legal aggressive action
            // then top up to pot bets where appropriate
            // then distribute chips beyond that
            int new_raise;
            for (int full_bets = 0; full_bets < 3; full_bets++) {
                for (int ind = 0; ind < NUM_BOARDS; ind++) { // distribute bets by prob. priority
                    int i = board_order[ind];
                    if (proposed_actions[i].actionType == Action::RAISE) {                        
                        // on first pass, put in min-bets/raises
                        // (2 for bets or villain_pip + (pip - villain_pip) for raises)
                        if (full_bets == 0) {
                            new_raise = (villain_pips[i] == 0) ? 2 : 2*villain_pips[i] - pips[i];
                        }
                        // on second pass, top up to pot bets
                        if (full_bets == 1) {
                            new_raise = min(proposed_pips[i], pips[i] + villain_pips[i] + pots[i]);
                        }
                        // on third pass, distribute remaining chips
                        if (full_bets == 2) {
                            new_raise = proposed_pips[i];
                        }
                        new_raise = max(min(new_raise, remaining_stack + raise_amounts[i]), 0);
                        remaining_stack -= (new_raise - raise_amounts[i]);
                        raise_amounts[i] = new_raise;
                    }
                }
            }

            // build new actions
            for (int i = 0; i < NUM_BOARDS; i++) {
                if (raise_amounts[i] == -1) {
                    my_actions[i] = Action::FOLD;
                    histories[i].update(FOLD);
                }
                else if (raise_amounts[i] > villain_pips[i]) {                    
                    int raise_size = min(villain_stack + villain_pips[i], raise_amounts[i]);

                    my_actions[i] = Action(Action::RAISE, raise_size);

                    int pot_after_call = 2*villain_pips[i] + pots[i];
                    histories[i].update(
                        map_bet_to_infoset_bet(raise_size, pot_after_call, histories[i]));
                    // histories[i].update(
                    //     map_bet_to_infoset_bet(pips[i], villain_pips[i],
                    //                             pots[i], histories[i], 0));
                }
                else {
                    if (in_set(legal_actions[i], Action::CHECK)) {
                        my_actions[i] = Action::CHECK;
                    }
                    else if (in_set(legal_actions[i], Action::CALL)) {
                        my_actions[i] = Action::CALL;
                    }

                    if (!histories[i].finished) {
                        histories[i].update(CHECK_CALL);
                    }
                }
            }

        }
        else {
            for (int i = 0; i < NUM_BOARDS; i++) {
                // update our internal game states if we didn't have to shuffle
                // around bets (only if that board isn't finished or we weren't
                // just allocating)
                if (!histories[i].finished && proposed_internal_actions[i] != 0) {
                    histories[i].update(proposed_internal_actions[i]);
                }
            }
        }

        if (VERBOSE) {
            for (int i = 0; i < my_actions.size(); i++) {
                cout << "Board " << (i+1) << ": " << my_actions[i] << endl;
            }
            cout << endl;
        }

        auto _timer_end = high_resolution_clock::now();
        _bot_time += duration_cast<microseconds>(_timer_end - _timer_start).count() / 1000000.0;
        
        return  my_actions;
    }

};

int main(int argc, char *argv[]) {
    auto [host, port] = parseArgs(argc, argv);
    srand (static_cast <unsigned> (time(0)));
    runBot<Bot>(host, port);
    return 0;
}
