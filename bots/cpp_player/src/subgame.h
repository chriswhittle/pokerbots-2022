#ifndef REAL_POKER_PLAYER_SUBGAME
#define REAL_POKER_PLAYER_SUBGAME

#include "player.h"
#include "cfr.h"

using namespace std;

const int DECK_SIZE = 52;
const int SUBGAME_MC_ITER = 300;

// on the river (all board cards dealt), solve the remainder of the game tree.
// We assume there are no swaps on the river (i.e. possible hands on the river
// must match the actions taken on the turn).
// We also assume a purified strategy.
struct Subgame {
    const vector<int> board_cards;
    const BoardActionHistory &history;
    const DataContainer &data;
    InfosetDictPure &infosets;

    BoardActionHistory retrace;

    Subgame(vector<int> init_board_cards, BoardActionHistory &init_history, 
            DataContainer &init_data, InfosetDictPure &init_infosets) : 
            board_cards(init_board_cards), history(init_history),
            data(init_data), infosets(init_infosets) {

        // we should be on the river before doing realtime solving
        assert(board_cards.size() == BOARD_SIZE);
    }

    // function to retrace actions in a history and construct conditions needed to
    // build up player river ranges based on actions taken in prior streets.
    // 2 (per player) x 3 (preflop/flop/turn) x number of actions taken
    // by that player on that street:
    // <key w/o card infostate, index of action taken>
    array<array<vector<pair<ULL, short>>, 3>, 2> build_range_conditions() {

        retrace = BoardActionHistory(history.button, 0, 0);
        array<array<vector<pair<ULL, short>>, 3>, 2> range_conditions;

        for (int i = 0; i < history.actions.size(); i++) {
            if (retrace.street < 3) { // if we're before the river
                // get key defined by the history (no card information)
                ULL key = info_to_key(retrace.ind ^ retrace.button, 
                                        retrace.street, 0, retrace);

                // convert the action taken to an index
                vector<int> retrace_actions = retrace.get_available_actions();
                assert(find(retrace_actions.begin(), retrace_actions.end(),
                            history.actions[i]) != retrace_actions.end());
                int retrace_action_index = find(retrace_actions.begin(),
                                                retrace_actions.end(),
                                                history.actions[i])
                                            - retrace_actions.begin();

                // add the action taken as a new condition to be met by all hands
                // in each range
                range_conditions[retrace.ind][retrace.street].push_back(
                    make_pair(key, retrace_action_index)
                );
                
                retrace.update(history.actions[i]);
            }

        }

        return range_conditions;

    }

    // check hand against range conditions
    bool check_hand_actions(array<int, HAND_SIZE> &hand_indices, int street,
                            const vector<pair<ULL, short>> &range_conditions) {
        // first compute hand card infostate key
        ULL card_key;
        if (street == 0) {
            card_key = get_cards_info_state_preflop(hand_indices);
        }
        else {
            // should be on flop or turn
            assert(street == 3 || street == 4);
            card_key = new_card_infostate(street, hand_indices, board_cards,
                                                data, SUBGAME_MC_ITER, 0, 0);
        }

        bool in_range = true;
        for (int i = 0; i < range_conditions.size(); i++) {
            ULL full_key = info_to_key(range_conditions[i].first, card_key);
            auto &infoset = fetch_infoset(infosets, full_key, 0);

            if (infoset.action != range_conditions[i].second) {
                in_range = false;
                break;
            }
        }

        return in_range;
    }

    // run CFR to solve river subgame
    void solve() {
        // construct conditions needed to build each player range based on
        // actions taken in prior streets.
        array<array<vector<pair<ULL, short>>, 3>, 2> range_conditions = 
        build_range_conditions();

        // mask for board cards that need to be excluded from player ranges
        ULL dead = 0;
        for (int i = 0; i < BOARD_SIZE; i++) {
            dead |= CARD_MASKS_TABLE[board_cards[i]];
        }

        // now go through all hands and see which match the range defined by
        // the pre-flop/flop/turn actions;
        // without swaps, all hands in the range should satisfy conditions on all
        // streets, but with swaps, they should just satisfy turn conditions at a
        // minimum and earlier streets before swaps

        // ranges defined by conditions on each street
        // (one set for each player)
        array<set<array<int, HAND_SIZE>>, 2> pre_range;
        array<set<array<int, HAND_SIZE>>, 2> flop_range;
        array<set<array<int, HAND_SIZE>>, 2> turn_range;

        // ranges defined by intersection on pre/flop and flop/turn
        array<set<array<int, HAND_SIZE>>, 2> pre_flop_range;
        array<set<array<int, HAND_SIZE>>, 2> flop_turn_range;
        
        // ranges defined by conditions on all streets
        array<set<array<int, HAND_SIZE>>, 2> full_range;

        for (int i = 0; i < DECK_SIZE; i++) {
            for (int j = i+1; j < DECK_SIZE; j++) {

                // if no cards in this hand appear on the board
                ULL hand = CARD_MASKS_TABLE[i] | CARD_MASKS_TABLE[j];
                if ((hand & dead) == 0) {

                    array<int, HAND_SIZE> hand_indices = {i, j};

                    // check conditions for each player
                    for (int p = 0; p < 2; p++) {

                        // check pre-flop actions
                        bool pre_match = check_hand_actions(hand_indices, 0, range_conditions[p][0]);
                        // check flop actions
                        bool flop_match = check_hand_actions(hand_indices, 3, range_conditions[p][1]);
                        // check turn actions
                        bool turn_match = check_hand_actions(hand_indices, 4, range_conditions[p][2]);

                        //////////////////////////////////////
                        // now add hand to approriate ranges

                        if (pre_match) { // pre
                            pre_range[p].insert(hand_indices);

                            if (flop_match) { // pre + flop
                                pre_flop_range[p].insert(hand_indices);

                                if (turn_match) { // pre + flop + turn
                                    full_range[p].insert(hand_indices);
                                }
                            }
                        }

                        if (flop_match) { // flop
                            flop_range[p].insert(hand_indices);

                            if (turn_match) {
                                flop_turn_range[p].insert(hand_indices);
                            }
                        }

                        if (turn_match) { // turn
                            turn_range[p].insert(hand_indices);
                        }

                        ////////////////////////////////////////

                    } // end player loop

                } 

            } // card #2 loop

        } // card #1 loop

    }

};

#endif