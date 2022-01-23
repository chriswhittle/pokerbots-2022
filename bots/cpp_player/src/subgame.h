#ifndef REAL_POKER_PLAYER_SUBGAME
#define REAL_POKER_PLAYER_SUBGAME

#include <set>
#include <algorithm>
#include <chrono>

#include "eval7pp.h"
#include "player.h"
#include "cfr.h"
#include "gametree.h"

using namespace std;
using namespace std::chrono;

const int DECK_SIZE = 52;
const int SUBGAME_MC_ITER = 300;

const int SUBGAME_CFR_ITER = 100000;

random_device SUB_RD;
mt19937 SUB_GEN(SUB_RD());
uniform_real_distribution<> SUB_RAND(0, 1);

// on the river (all board cards dealt), solve the remainder of the game tree.
// We assume there are no swaps on the river (i.e. possible hands on the river
// must match the actions taken on the turn).
// We also assume a purified strategy.
struct Subgame {
    const int subgame_cfr_iter = SUBGAME_CFR_ITER;

    const vector<int> board_cards;
    const BoardActionHistory &history;
    const DataContainer &data;
    InfosetDictPure &infosets;

    InfosetDict subgame_infosets;

    BoardActionHistory retrace;

    // ranges defined by conditions on each street
    // (one set for each player)
    array<set<array<int, HAND_SIZE>>, 2> pre_range;
    array<vector<array<int, HAND_SIZE>>, 2> pre_range_list;
    array<set<array<int, HAND_SIZE>>, 2> flop_range;
    array<vector<array<int, HAND_SIZE>>, 2> flop_range_list;
    array<set<array<int, HAND_SIZE>>, 2> turn_range;
    array<vector<array<int, HAND_SIZE>>, 2> turn_range_list;

    // ranges defined by intersection on pre/flop and flop/turn
    array<set<array<int, HAND_SIZE>>, 2> pre_flop_range;
    array<vector<array<int, HAND_SIZE>>, 2> pre_flop_range_list;
    array<set<array<int, HAND_SIZE>>, 2> flop_turn_range;
    array<vector<array<int, HAND_SIZE>>, 2> flop_turn_range_list;
    
    // ranges defined by conditions on all streets
    array<set<array<int, HAND_SIZE>>, 2> full_range;
    array<vector<array<int, HAND_SIZE>>, 2> full_range_list;

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

    // deal a hand given defined ranges for pre/flop/turn
    // uses swap probabilities defined for the game
    array<int, HAND_SIZE> deal_subgame_hand(int player, ULL dead) {

        // roll for swaps (dictates which ranges we should sample from)
        array<int, 2> swaps = {0, 0}; // swaps on the flop and turn respectively
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < HAND_SIZE; j++) {
                if (SUB_RAND(SUB_GEN) < SWAP_ODDS[i]) {
                    swaps[i]++;
                }
            }
        }

        vector<array<int, HAND_SIZE>> range;
        // select from range that satisfies pre/flop/turn
        if (swaps[0] == 0 && swaps[1] == 0) {
            range = full_range_list[player];
        }
        // select from range that satisfies flop/turn
        else if (swaps[1] == 0) {
            range = flop_turn_range_list[player];
        }
        // select from just the turn range
        else {
            range = turn_range_list[player];
        }

        array<int, HAND_SIZE> new_hand_indices;
        array<int, HAND_SIZE> swapped_hand_indices;
        ULL new_hand_mask;
        int new_card;
        set<array<int, HAND_SIZE>> sub_range;
        while (true) {

            // sample hand from range
            new_hand_indices = range[SUB_GEN() % range.size()];
            new_hand_mask = CARD_MASKS_TABLE[new_hand_indices[0]] |
                            CARD_MASKS_TABLE[new_hand_indices[1]];

            swapped_hand_indices = new_hand_indices;

            array<bool, 2> valid_hand = {true, true};
            for (int s = 1; s >= 0; s--) {
                if (swaps[s] == 1) {
                    // draw new card for pre/flop
                    new_card = sample_card_dist(dead | new_hand_mask);
                    swapped_hand_indices[SUB_GEN() % HAND_SIZE] = new_card;
                    new_hand_mask |= CARD_MASKS_TABLE[new_card];

                    // swapped card is valid if the new hand is in our:
                    // 1) flop range if we also swapped pre->flop
                    if (s == 1 && swaps[0] == 1) {
                        sub_range = flop_range[player];
                    }
                    // 2) intersection of pre/flop range if we didn't swap pre->flop
                    else if (s == 1) {
                        sub_range = pre_flop_range[player];
                    }
                    // 3) pre range if we're looking at flop swaps
                    else {
                        sub_range = pre_range[player];
                    }
                    
                    // valid for this swap if it's in the previous range
                    valid_hand[s] = (
                        sub_range.find(swapped_hand_indices) 
                        != sub_range.end()
                    );
                }
            }

            // this hand is valid if:
            // 1) we had 0 or 2 swaps on flop + turn
            // 2) we had 1 swap on either/both flop + turn, but we swap a card
            // into the appropriate range
            if (valid_hand[0] && valid_hand[1]) {
                return new_hand_indices;
            }

        }

    }

    // run CFR iteration starting from river
    pair<double, double> subgame_cfr(int winner, GameTreeNode &node,
                            int card_key1, int card_key2) {

        // reached leaf node
        if (node.children.size() == 0) {
            assert(node.finished);
            // if no showdown, just return amount won according to history
            if (!node.showdown) {
                assert(node.won != 0);
                return make_pair(node.won, -node.won);
            }
            // showdown with chop
            else if (winner == -1) {
                return make_pair(0, 0);
            }
            // showdown without chop
            else {
                // node assumes player won
                assert(node.won > 0);
                int winner_mult = (winner == 0) ? 1 : -1;
                return make_pair(node.won * winner_mult, -node.won * winner_mult);
            }
        }

        // choose card key based on which player is next to act
        auto& card_info_state = (node.ind == 0) ? card_key1 : card_key2;
        // generate full key
        ULL key = info_to_key(node.history_key, card_info_state);

        // fetch infoset from subgame infoset dict
        CFRInfoset& infoset = fetch_infoset(subgame_infosets, key, node.children.size());

        assert(infoset.cumu_regrets.size() == node.children.size());

        ///////////////////////////////////////////////////            
        // we traverse both player's actions (asymmetric)
        
        // get strategy from infoset
        vector<double> strategy = infoset.get_regret_matching_strategy();
        assert(strategy.size() == node.children.size());
        
        // iterate over possible actions
        vector<double> utils(node.children.size());
        pair<double, double> tot_val = {0, 0};
        for (int i = 0; i < node.children.size(); i++) {
            auto sub_val = subgame_cfr(winner, node.children[i], card_key1, card_key2);

            tot_val = tot_val + strategy[i]*sub_val;
            utils[i] = sub_val.first;
        }

        // utils -> regrets
        for (int i = 0; i < utils.size(); i++) {
            utils[i] -= tot_val.first;
        }

        infoset.record(utils, strategy);
        return tot_val;

    }

    // run CFR to solve river subgame
    void solve() {
        // construct conditions needed to build each player range based on
        // actions taken in prior streets.
        array<array<vector<pair<ULL, short>>, 3>, 2> range_conditions = 
        build_range_conditions();

        // mask for board cards that need to be excluded from player ranges
        ULL board_mask = 0;
        for (int i = 0; i < BOARD_SIZE; i++) {
            board_mask |= CARD_MASKS_TABLE[board_cards[i]];
        }

        // now go through all hands and see which match the range defined by
        // the pre-flop/flop/turn actions;
        // without swaps, all hands in the range should satisfy conditions on all
        // streets, but with swaps, they should just satisfy turn conditions at a
        // minimum and earlier streets before swaps
        for (int i = 0; i < DECK_SIZE; i++) {
            for (int j = i+1; j < DECK_SIZE; j++) {

                // if no cards in this hand appear on the board
                ULL hand = CARD_MASKS_TABLE[i] | CARD_MASKS_TABLE[j];
                if ((hand & board_mask) == 0) {

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
                            pre_range_list[p].push_back(hand_indices);

                            if (flop_match) { // pre + flop
                                pre_flop_range[p].insert(hand_indices);
                                pre_flop_range_list[p].push_back(hand_indices);

                                if (turn_match) { // pre + flop + turn
                                    full_range[p].insert(hand_indices);
                                    full_range_list[p].push_back(hand_indices);
                                }
                            }
                        }

                        if (flop_match) { // flop
                            flop_range[p].insert(hand_indices);
                            flop_range_list[p].push_back(hand_indices);

                            if (turn_match) {
                                flop_turn_range[p].insert(hand_indices);
                                flop_turn_range_list[p].push_back(hand_indices);
                            }
                        }

                        if (turn_match) { // turn
                            turn_range[p].insert(hand_indices);
                            turn_range_list[p].push_back(hand_indices);
                        }

                        ////////////////////////////////////////

                    } // end player loop

                } 

            } // card #2 loop

        } // card #1 loop

        ////// run CFR
        // internal board state should be on river (= 3 in internal state)
        assert(retrace.street == 3);

        // build a game tree using the initial river action as the root
        GameTreeNode river_root = build_game_tree(retrace);

        // CFR iterations
        pair<double, double> train_val = {0, 0};
        time_point<high_resolution_clock> cfr_start = high_resolution_clock::now();
        for (int i = 0; i < subgame_cfr_iter; i++) {
            // deal hands for each player
            array<array<int, HAND_SIZE>, 2> cfr_hand_indices;
            array<ULL, 2> cfr_hand_masks = {0, 0};
            for (int p = 0; p < 2; p++) {
                cfr_hand_indices[p] = deal_subgame_hand(0, cfr_hand_masks[0]);
                cfr_hand_masks[p] = CARD_MASKS_TABLE[cfr_hand_indices[p][0]]
                        | CARD_MASKS_TABLE[cfr_hand_indices[p][1]];
            }

            // determine winner given hands
            int winner = evaluate(cfr_hand_masks[0] | board_mask, 7)
                            < evaluate(cfr_hand_masks[1] | board_mask, 7);


            auto val = subgame_cfr(
                winner, river_root,
                get_cards_info_state_preflop(cfr_hand_indices[0]),
                get_cards_info_state_preflop(cfr_hand_indices[1]));

            train_val = train_val + (1./subgame_cfr_iter) * val;
        }

        cout << "Final value for river subgame = " << train_val;
        cout << " (" << (duration_cast<std::chrono::milliseconds>(
                            high_resolution_clock::now() - cfr_start).count());
        cout << " ms)" << endl;

    }

};

#endif