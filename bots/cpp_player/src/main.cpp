#include <chrono>
#include <memory>

#include <skeleton/actions.h>
#include <skeleton/constants.h>
#include <skeleton/runner.h>
#include <skeleton/states.h>

#include "cfr.h"
#include "binary.h"
#include "eval7pp.h"

using namespace pokerbots::skeleton;
using namespace std::chrono;

#include "player.h"
#include "subgame.h"

const int N_MC_ITER = 10000;
const bool VERBOSE = false;

// the bot will expect an unordered_map<ULL, CFRInfosetPure> infoset
// if the following is set (else unordered_map<ULL, CFRInfoset>)
#define PLAYER_USE_PURE

// should the bot attempt to solve the river subgame?
const bool SOLVE_RIVER_SUBGAME = false;

struct Bot {
    int net_lead = 0;
    int rounds_played = 0;
    bool check_fold_win = false;

    array<int, HAND_SIZE> hand_indices;
    int card_infostate;
    int card_infostate_street = 0;
    
    ULL dead = 0;
    vector<int> board_cards;

    DataContainer data;
    #ifdef PLAYER_USE_PURE
    InfosetDictPure infosets;
    #else
    InfosetDict infosets;
    #endif

    BoardActionHistory history;

    bool subgame_solved = false;

    time_point<high_resolution_clock> _start;
    double _bot_time = 0.0;

    Bot() : data() {
        _start = high_resolution_clock::now();
        // load equity data
        // load flop buckets from binary
        load_buckets_from_file_bin("data/flop_buckets.bin", &data.flop_buckets);
        load_clusters_from_file("data/turn_clusters.txt", data.turn_clusters);
        load_clusters_from_file("data/river_clusters.txt", data.river_clusters);
        cout << "Loaded equity data in " <<
                (duration_cast<std::chrono::milliseconds>(
                    high_resolution_clock::now() - _start
                ).count()) << " ms" << endl;

        // load infosets
        load_infosets_from_file_bin("data/infosets.bin", &infosets);

        cout << "Loaded " << infosets.size() << " in " << 
        duration_cast<std::chrono::milliseconds>(
                            high_resolution_clock::now() - _start
                        ).count() << " ms" << endl;
    }
    
    void handleNewRound(
            GameInfoPtr gameState, RoundStatePtr roundState, int active) {

        cout << "=====Round #" << gameState->roundNum << "=====" << endl;

        auto _timer_start = high_resolution_clock::now();
        history = BoardActionHistory(active, 0, 0);

        // convert card strings to card numbers
        transform(roundState->hands[active].begin(),
                    roundState->hands[active].end(),
                    hand_indices.begin(), card_string_to_index);

        // calculate initial card infostate
        card_infostate = get_cards_info_state_preflop(hand_indices);
        card_infostate_street = 0;
        // empty our memory of the boards
        board_cards.clear();

        // river subgame still needs to be solved
        subgame_solved = false;

        auto _timer_end = high_resolution_clock::now();
        _bot_time += duration_cast<microseconds>(_timer_end - _timer_start).count() / 1000000.0;

        /////// log
        if (VERBOSE) {
            cout << "Dealt = ";
            for (int i = 0; i < HAND_SIZE; i++) cout << roundState->hands[active][i] << ", ";
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
    

    void updateBotTime(time_point<high_resolution_clock> _timer_start) {
        auto _timer_end = high_resolution_clock::now();
        _bot_time += duration_cast<microseconds>(_timer_end - _timer_start).count() / 1000000.0;
    }

    Action getAction(
            GameInfoPtr gameState, RoundStatePtr roundState, int active) {
        auto _timer_start = high_resolution_clock::now();
        Action proposed_action;

        auto legal_actions = roundState->legalActions();

        int stack = roundState->stacks[active];
        int villain_stack = roundState->stacks[1-active];
        int street = roundState->street;

        int proposed_pip = 0;
        int continue_cost = 0;

        if (VERBOSE) {
            cout << "<<Stacks = " << stack << ", " << villain_stack
                 << "; street = " << street << ">>" << endl << endl;
        }

        // update our internal tracking of hole cards
        // convert card strings to card numbers
        transform(roundState->hands[active].begin(),
                    roundState->hands[active].end(),
                    hand_indices.begin(), card_string_to_index);

        // once we see new cards on the boards, add them to our
        // memory of the board cards
        if (street > card_infostate_street) {
            for (int i = card_infostate_street; i < street; i++) {
                int card_index = card_string_to_index(roundState->deck[i]);
                board_cards.push_back(card_index);
            }
        }

        // if we have enough of a lead check/fold to victory
        if (check_fold_win || 2*gameState->bankroll >
                                ((NUM_ROUNDS - gameState->roundNum)*3 + 4)) {

            check_fold_win = true;
            if (in_set(legal_actions, Action::CHECK)) {
                proposed_action = Action::CHECK;
            }
            else {
                proposed_action = Action::FOLD;
            }

            cout << "Check/folding to win." << endl;

            updateBotTime(_timer_start);
            return proposed_action;

        }

        // if both players are all in
        if ((legal_actions.size() == 1 && in_set(legal_actions, Action::CHECK)
            && std::dynamic_pointer_cast<const TerminalState>(roundState))
            || stack == 0) {
            proposed_action = Action::CHECK;
            if (VERBOSE) cout << "Both players all in, check." << endl;
            
            updateBotTime(_timer_start);
            return proposed_action;
        }

        // update internal representation of game tree
        // and of cards in hand + on board
        int pip = roundState->pips[active];
        int villain_pip = roundState->pips[1-active];
        int pot = pokerbots::skeleton::STARTING_STACK*2
                    - roundState->stacks[0] - roundState->stacks[1]
                    - pip - villain_pip;

        if (VERBOSE) {
            cout << "Pot = " << pot << "; engine pip = " << pip << ", " << villain_pip << endl;
            cout << "Hole cards: " << roundState->hands[active] << endl;
            cout << "Board cards: " << roundState->deck << endl;
            cout << "Hole cards (interpreted): " << pretty_card(hand_indices) << endl;
            cout << "Board cards (interpreted): " << pretty_card(board_cards) << endl;
        }

        // update internal history objects
        update_internal_tracking(pip, villain_pip, pot, street, history);

        if (VERBOSE) {
            cout << "Updated internal history to " << history << endl;
        }

        // update card infostate to match engine street
        if (street > card_infostate_street) {
            card_infostate = new_card_infostate(street, hand_indices,
                                                board_cards, data, N_MC_ITER,
                                                // dead, 0);
                                                0, 0);
            card_infostate_street = street;

            if (VERBOSE) {
                cout << "Updated card infostate to " << card_infostate << endl;
            }
        }

        // check that our internal state isn't ahead of the engine
        if (history.street == 1 && street < 3 ||
            history.street == 2 && street < 4 ||
            history.street == 3 && street < 5 ||
            history.finished) {
            // could happen if e.g. we check to button, they bet small
            // (which looks like a check => finishes action)
            if (VERBOSE) cout << "CFR ahead of action, check/call to catch up." << endl;
            if (in_set(legal_actions, Action::CHECK)) {
                proposed_action = Action::CHECK;
            }
            else if (in_set(legal_actions, Action::CALL)) {
                proposed_action = Action::CALL;
                continue_cost = villain_pip - pip;
                proposed_pip = continue_cost;
            }
            
            updateBotTime(_timer_start);
            return proposed_action;
        }

        // realtime solve river subgame
        if (SOLVE_RIVER_SUBGAME && street == 5 && !subgame_solved) {
            // build subtime object and run CFR
            Subgame subgame(board_cards, history, data, infosets);
            subgame.solve();

            // mark river subgame as solved
            subgame_solved = true;
        }

        vector<int> available_actions = history.get_available_actions();

        // fetch appropriate infoset
        unsigned long long key = info_to_key(history.ind ^ history.button,
                                    history.street,
                                    card_infostate,
                                    history);

        #ifdef PLAYER_USE_PURE
            if (infosets.find(key) == infosets.end()) {
                cout << "WARNING: No information for this state." << endl;
            }
        #endif

        auto& infoset = fetch_infoset(infosets, key, available_actions.size());

        cout << "Infostate key: " << key << endl;
        if (VERBOSE) {
            #ifdef PLAYER_USE_PURE
            cout << "Pure strategy: ";
            print_action(cout, available_actions[infoset.action]);
            cout << endl;
            #else
            if (infoset.t == 0) cout << "WARNING: No information for this state." << endl;
            cout << "Available actions: ";
            print_actions(cout, available_actions);
            cout << endl;
            cout << "Strategy: " << infoset.cumu_strategy << endl;
            cout << "(visited " << infoset.t << " times)" << endl << endl;
            #endif
        }

        // get proposed action
        int action_index = infoset.get_action_index_avg();
        int action = available_actions[action_index];

        // update history with action
        history.update(action);

        // convert the proposed action to an action for the engine
        proposed_action = map_infoset_action_to_action(
            action, pip, villain_pip, pot, stack, villain_stack);

        // if we think we're calling a shove but the real action is not a shove,
        // should just shove ourselves so we don't misplay future streets
        if (history.finished &&
            history.pot == STARTING_STACK*2 &&
            villain_stack > 0) {
            if (VERBOSE) cout << "Internally all in, shove to match." << endl;
            return Action(Action::RAISE, pip + stack);
        }

        if (VERBOSE) {
            cout << "Board: " << proposed_action << endl;
            cout << endl;
        }

        updateBotTime(_timer_start);
        return  proposed_action;
    }

};

int main(int argc, char *argv[]) {
    auto [host, port] = parseArgs(argc, argv);
    srand (static_cast <unsigned> (time(0)));
    runBot<Bot>(host, port);
    return 0;
}
