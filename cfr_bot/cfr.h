#ifndef REAL_POKER_CFR
#define REAL_POKER_CFR

#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <random>
#include <cassert>

#include "define.h"
#include "game.h"
#include "compute_equity.h"

using namespace std;

extern random_device CFR_RD;
extern mt19937 CFR_GEN;
extern uniform_real_distribution<> CFR_RAND;

const int SHIFT_PLAYER_IND = 1; // 1 bit for player position
const int SHIFT_STREET = 2; // 2 bits for street
const int SHIFT_CARD_INFO = 13; // 13 bits for card info
// 3 bits per action defined in game.h

// abstract CFR infoset to be used by the player
struct CFRInfosetBase {
    virtual int get_action_index_avg() = 0;
};

// purified CFR infoset (strategy is a pure action)
struct CFRInfosetPure : CFRInfosetBase {
    char action = 0;

    CFRInfosetPure();
    CFRInfosetPure(int init_action);
    int get_action_index_avg();
};

// CFR infoset with full information (regrets, strategy) used in training
struct CFRInfoset : CFRInfosetBase {
    ULL info = 0;
    vector<double> cumu_regrets;
    vector<double> cumu_strategy;
    int t = 0;

    CFRInfoset();
    CFRInfoset(int num_actions);
    CFRInfoset(vector<double> init_cumu_regrets, vector<double> init_cumu_strategy, int init_t);

    void record(vector<double> regrets, vector<double> strategy);
    int get_action_index_avg();
    vector<double> get_avg_strategy();
    int get_action_index(double eps = 0.0);
    vector<double> get_regret_matching_strategy();

};

inline ostream& operator<<(ostream& os, CFRInfosetPure& p) {
    os << "CFRInfosetPure(action=" << p.action << ")";
    return os;
}

inline ostream& operator<<(ostream& os, CFRInfoset& p) {
    os << "CFRInfoset(t=" << p.t << ",";
    os << "avgstrat=" << p.get_avg_strategy() << ")";
    return os;
}

// used to see if the player is facing a bet based on the infoset key
// without having to reproduce the full game tree
// (used to decide whether folding is a valid action)
inline bool key_is_facing_bet(ULL full_key) {
    ULL key = full_key >> (SHIFT_PLAYER_IND + SHIFT_STREET + SHIFT_CARD_INFO);
    int last_action;

    // if no actions have occurred, the player is in the SB and can fold
    if (key == 0) {
        return true;
    }

    while (key != 0) {
        last_action = key & ((1 << SHIFT_ACTIONS) - 1);
        key = key >> SHIFT_ACTIONS;
    }

    return last_action >= BET;
}

using InfosetDict = unordered_map<ULL, CFRInfoset>;
using InfosetDictPure = unordered_map<ULL, CFRInfosetPure>;

inline CFRInfoset& fetch_infoset(InfosetDict &infosets,
                                ULL key, int num_actions) {
    if (infosets.find(key) == infosets.end()) {
        CFRInfoset new_cfr_infoset(num_actions);
        infosets.insert(make_pair(key, new_cfr_infoset));
    }
    return infosets[key];
}

inline CFRInfosetPure& fetch_infoset(InfosetDictPure &infosets,
                                    ULL key, int num_actions) {
    if (infosets.find(key) == infosets.end()) {
        // if infoset hasn't been visited, we should prefer to fold or check
        // if possible for variance reduction
        CFRInfosetPure new_cfr_infoset(
            (key_is_facing_bet(key) ? 1 : 0)
        );
        infosets.insert(make_pair(key, new_cfr_infoset));
    }
    return infosets[key];
}

inline ostream& operator<<(ostream& os, InfosetDict& p) {
    for (auto kv : p) {
        os << kv.first << " ";
        os << kv.second.t << " ";
        for (int i = 0; i < kv.second.cumu_regrets.size(); i++) {
            os << kv.second.cumu_regrets[i] << " ";
            os << kv.second.cumu_strategy[i] << " ";
        }
        os << endl;
    }
    return os;
}

inline istream& operator>>(istream &in, InfosetDict& p)
{
    string line;
    ULL key;
    int t;
    double e;

    while (getline(in, line)) {
        istringstream iss(line);
        if (!(iss >> key)) {
            break;
        }

        iss >> t;

        vector<double> cumu_regrets;
        vector<double> cumu_strategy;
        while (iss >> e) {
            cumu_regrets.push_back(e);

            iss >> e;
            cumu_strategy.push_back(e);
        }

        CFRInfoset infoset(cumu_regrets, cumu_strategy, t);
        p.insert(make_pair(key, infoset));

    }

    return in;
}

inline void save_infosets_to_file(string filename, InfosetDict &infosets) {
    {
        ofstream outfile(filename);
        outfile << infosets;
    }
}

inline void load_infosets_from_file(string filename, InfosetDict &infosets) {

    {
        ifstream infile(filename);
        infile >> infosets;
    }

}

template<size_t N>
inline array<int, N> get_ranks_from_indices(array<int, N> a) {
    array<int, N> ranks;
    for (int i = 0; i < N; i++) {
        ranks[i] = a[i] % NUM_RANKS;
    }
    return ranks;
}

// for sorting cards by rank (descending)
inline bool compare_ranks_desc(int a, int b) {
    return a%NUM_RANKS > b%NUM_RANKS;
}

// compute equities and use cluster centers to give bucket
int get_bucket_from_clusters(
    ULL hand, ULL board, int num_board, const EquityClusters &clusters,
    int iterations, ULL common_dead = 0, ULL villain_dead = 0);

inline int get_cards_info_state_preflop(array<int, HAND_SIZE> &c) {
    sort(c.begin(), c.end(), compare_ranks_desc);
    array<int, HAND_SIZE> ranks = get_ranks_from_indices(c);
    if (ranks[0] == ranks[1]) {
        return ranks[0]*NUM_RANKS + ranks[0];
    }
    else if (c[0]/NUM_RANKS == c[1]/NUM_RANKS) { // suited
        return ranks[0]*NUM_RANKS + ranks[1];
    }
    else { // offsuit
        return ranks[1]*NUM_RANKS + ranks[0];
    }
}

inline int get_cards_info_state_flop(array<int, HAND_SIZE> c, array<int, FLOP_SIZE> flop, const DataContainer &data) {
    sort(c.begin(), c.end(), compare_ranks_desc);
    sort(flop.begin(), flop.begin() + FLOP_SIZE, compare_ranks_desc);

    int suit_count = 0;
    unordered_map<int, int> suit_iso;

    int key = 0;
    int mult = 1;

    int card, suit;
    for (int i = 0; i < FLOP_SIZE + HAND_SIZE; i++) {
        card = (i < FLOP_SIZE) ? flop[i] : c[i-FLOP_SIZE];
        suit = (card)/NUM_RANKS;
        if (suit_iso.find(suit) == suit_iso.end()) {
            suit_iso[suit] = suit_count;
            suit_count++;
        }
        key += mult*(card%NUM_RANKS + NUM_RANKS*suit_iso[suit]);
        mult *= NUM_RANKS*NUM_SUITS;
    }

    assert(data.flop_buckets.find(key) != data.flop_buckets.end());
    return data.flop_buckets.at(key);
}

// get infostate for all streets, with different hole cards each street
inline array<int, NUM_STREETS> get_cards_info_state(
    array<array<int, HAND_SIZE>, NUM_STREETS> c, array<int, BOARD_SIZE> board,
    const DataContainer &data, int iterations = 1000) {
    array<int, NUM_STREETS> info_states;

    //// pre-flop
    info_states[0] = get_cards_info_state_preflop(c[0]);

    //// flop
    array<int, FLOP_SIZE> flop;
    copy(board.begin(), board.begin()+FLOP_SIZE, flop.begin());
    info_states[1] = get_cards_info_state_flop(c[1], flop, data);

    //// turn
    ULL hand_mask_turn = indices_to_mask(c[2]);
    array<int, TURN_SIZE> turn_board;
    copy(board.begin(), board.begin() + TURN_SIZE, turn_board.begin());
    ULL turn_mask = indices_to_mask(turn_board);

    info_states[2] = get_bucket_from_clusters(
        hand_mask_turn, turn_mask, TURN_SIZE, data.turn_clusters, iterations);

    //// river
    ULL hand_mask_river = indices_to_mask(c[3]);
    ULL board_mask = indices_to_mask(board);

    info_states[3] = get_bucket_from_clusters(
        hand_mask_river, board_mask, board.size(), data.river_clusters, 0);

    return info_states;
}

// get infostate for all streets
inline array<int, NUM_STREETS> get_cards_info_state(
    array<int, HAND_SIZE> c, array<int, BOARD_SIZE> board,
    const DataContainer &data, int iterations = 1000) {

    array<array<int, HAND_SIZE>, NUM_STREETS> c_full;
    for (int i = 0; i < NUM_STREETS; i++) {
        c_full[i] = c;
    }
    return get_cards_info_state(c_full, board, data, iterations);

}

ULL info_to_key(int player_ind, int street, int card_info, BoardActionHistory &history);

const int SHIFT_CARD_INFO_TOTAL = SHIFT_PLAYER_IND + SHIFT_STREET;
ULL info_to_key(ULL history_key, int card_info);

CFRInfosetPure purify_infoset(CFRInfoset full_infoset, ULL key,
                                double fold_threshold = 0.5,
                                int visit_count_threshold = 100);

#endif