#ifndef REAL_POKER_COMPUTE_EQUITIES
#define REAL_POKER_COMPUTE_EQUITIES

#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <algorithm>
#include <unordered_map>

#include "eval7pp.h"
#include "define.h"
#include "tqdm.h"

using namespace std;

// EquityClusters = list of clusters for turn/river
using EquityClusters = vector<array<double, NUM_RANGES>>;
inline istream& operator>>(istream &in, EquityClusters& p)
{
    string line;
    double e;

    while (getline(in, line)) {
        istringstream iss(line);

        array<double, NUM_RANGES> values;
        for (int i = 0; i < NUM_RANGES; i++) {
            iss >> e;
            values[i] = e;
        }

        p.push_back(values);

    }

    return in;
}

// BucketDict = mapping from (hand, flop) to bucket
using BucketDict = unordered_map<int, int>;
inline ostream& operator<<(ostream& os, BucketDict& p) {
    for (auto kv : p) {
        os << kv.first << kv.second << endl;
    }
    return os;
}
inline istream& operator>>(istream &in, BucketDict& p)
{
    int key;
    int value;

    while (in >> key) {
        in >> value;
        p[key] = value;
    }

    return in;
}

// EquityDict = mapping from (hand, flop) to NUM_RANGES equities
using EquityDict = unordered_map<int, vector<float>>;
inline ostream& operator<<(ostream& os, EquityDict& p) {
    for (auto kv : p) {
        os << kv.first;
        for (int i = 0; i < kv.second.size(); i++) {
            os << " " << kv.second[i];
        }
        os << endl;
    }
    return os;
}
inline istream& operator>>(istream &in, EquityDict& p)
{
    string line;
    int key;
    float e;

    while (getline(in, line)) {
        istringstream iss(line);
        if (!(iss >> key)) {
            break;
        }

        vector<float> value;
        for (int i = 0; i < NUM_RANGES; i++) {
            iss >> e;
            value.push_back(e);
        }

        p[key] = value;

    }

    return in;
}

// PreflopEquityDict = mapping from (preflop hand) -> equity against random range
using PreflopEquityDict = unordered_map<int, float>;
inline ostream& operator<<(ostream& os, PreflopEquityDict& p) {
    for (auto kv : p) {
        os << kv.first << " " << kv.second << endl;
    }
    return os;
}
inline istream& operator>>(istream &in, PreflopEquityDict& p)
{
    int key;
    float value;

    while (in >> key) {
        in >> value;
        p[key] = value;
    }

    return in;
}

inline int suit_rank_to_index(int suit, int rank) {
    return 13*suit + rank;
}

inline void get_combos(vector<vector<int>> &output, int cards, int count = 1, bool isomorphic = true, vector<vector<int>> suffix = {{}}) {
    int new_count;

    if (cards == 0) {
        output = suffix;
        return;
    }
    else {
        for (int i = 0; i < count; i++) {

            vector<vector<int>> new_combos;
            vector<vector<int>> new_suffix;

            for (int j = 0; j < suffix.size(); j++) {
                vector<int> cur_suffix = suffix[j];
                cur_suffix.push_back(i);

                new_suffix.push_back(cur_suffix);
            }

            if (isomorphic) {
                new_count = (i+1 == count) ? min(count+1, 4) : min(count, 4);
            }
            else {
                new_count = i+1;
            }

            get_combos(new_combos, cards-1, new_count, isomorphic, new_suffix);
            output.insert(output.end(), new_combos.begin(), new_combos.end());
        }
    }
}

inline void get_rank_combos(vector<vector<int>> &output, int cards) {
    get_combos(output, cards, 13, false);
}

inline void get_suit_combos(vector<vector<int>> &output, int cards) {
    get_combos(output, cards, 1);
}


template <int RUNOUTS>
inline void save_equities_to_file_monte_carlo(string filename, int board_cards, int equity_iterations = 1000) {
    unsigned long long hand;
    unsigned long long board;

    {

        ofstream outfile(filename);

        tqdm pbar;
        for (int i = 0; i < RUNOUTS; i++) {
            pbar.progress(i, RUNOUTS);

            hand = 0;
            board = 0;

            for (int j = 0; j < 2; j++) hand |= deal_card(hand);
            for (int j = 0; j < board_cards; j++) board |= deal_card(board | hand);

            for (int j = 0; j < NUM_RANGES; j++) {
                if (j > 0) {
                    outfile << " ";
                }

                if (equity_iterations > 0) { // MC the runout
                    outfile << hand_vs_range_monte_carlo(hand, RANGES[j], NUM_RANGE[j], board, board_cards, equity_iterations);
                }
                else { // exactly calculate equity
                    outfile << hand_vs_range_exact(hand, RANGES[j], NUM_RANGE[j], board, board_cards);
                }
            }

            outfile << endl;

        }

    }

}

inline void save_equities_to_file(string filename, int board_cards, int iterations = 1000) {

    EquityDict equities;

    vector<vector<int>> suit_combos;
    get_suit_combos(suit_combos, board_cards + 2);

    vector<vector<int>> hand_rank_combos;
    get_rank_combos(hand_rank_combos, 2);

    vector<vector<int>> board_rank_combos;
    get_rank_combos(board_rank_combos, board_cards);

    tqdm pbar;
    for (int i = 0; i < board_rank_combos.size(); i++) {
        pbar.progress(i, board_rank_combos.size());

        for (int j = 0; j < suit_combos.size(); j++) {
            for (int k = 0; k < hand_rank_combos.size(); k++) {

                // combine suit and rank combos to get array of cards
                vector<int> cards;

                for (int l = 0; l < board_cards+2; l++) {
                    int card;
                    if (l < board_cards) {
                        card = suit_rank_to_index(suit_combos[j][l], board_rank_combos[i][l]);
                    }
                    else {
                        card = suit_rank_to_index(suit_combos[j][l], hand_rank_combos[k][l-board_cards]);
                    }

                    cards.push_back(card);
                }

                set<int> cards_set(cards.begin(), cards.end());

                // check that there are no duplicate cards
                if (cards_set.size() == board_cards+2) {
                    unsigned long long hand = 0;
                    unsigned long long board = 0;

                    int index = 0;
                    int mult = 1;

                    for (int l = 0; l < board_cards+2; l++) {
                        if (l < board_cards) {
                            board |= CARD_MASKS_TABLE[cards[l]];
                        }
                        else {
                            hand |= CARD_MASKS_TABLE[cards[l]];
                        }

                        index += cards[l]*mult;
                        mult *= 52;
                    }

                    // compute equities against each range
                    vector<float> hand_equities;
                    for (int l = 0; l < NUM_RANGES; l++) {
                        hand_equities.push_back(hand_vs_range_monte_carlo(hand,
                                                    RANGES[l], NUM_RANGE[l], 
                                                    board, board_cards, iterations));
                    }
                    equities[index] = hand_equities;
                }
            }
        }

    }

    {
        ofstream outfile(filename);
        outfile << equities;
    }

}

// note that this is separate from the preflop card infostate
// calculation done elsewhere
inline int preflop_card_indices_to_index(int c1, int c2) {
    int rank1 = max(c1 % 13, c2 % 13);
    int rank2 = min(c1 % 13, c2 % 13);

    if ((rank1 == rank2) || (c1/13 != c2/13)) {
        rank2 += 13;
    }

    return rank1*26 + rank2;
}

inline void save_preflop_equities_to_file(string filename, int iterations = 1000000) {

    PreflopEquityDict equities;

    vector<vector<int>> hand_rank_combos;
    get_rank_combos(hand_rank_combos, 2);

    tqdm pbar;
    for (int i = 0; i < hand_rank_combos.size(); i++) {
        pbar.progress(i, hand_rank_combos.size());

        for (int suited = 0; suited <= 1; suited++) {

            int first_index = hand_rank_combos[i][0];
            int second_index = hand_rank_combos[i][1];
            
            if ((first_index != second_index) || (suited == 0)) {

                unsigned long long hand = CARD_MASKS_TABLE[first_index];
                if (suited == 0 || (hand_rank_combos[i][0] == hand_rank_combos[i][1])) {
                    second_index += 13;
                }
                hand |= CARD_MASKS_TABLE[second_index];

                float equity = hand_vs_random_monte_carlo(hand, 0, 0, iterations);

                equities[first_index*26 + second_index] = equity;

            }

        }

    }

    {
        ofstream outfile(filename);
        outfile << equities;
    }

}

inline void load_clusters_from_file(string filename, EquityClusters &clusters) {
    ifstream infile(filename);
    infile >> clusters;
}

inline void load_buckets_from_file(string filename, BucketDict &buckets) {
    ifstream infile(filename);
    infile >> buckets;
}

inline void load_equities_from_file(string filename, EquityDict &equities) {
    ifstream infile(filename);
    infile >> equities;
}

inline void load_preflop_equities_from_file(string filename, PreflopEquityDict &preflop_equities) {
    ifstream infile(filename);
    infile >> preflop_equities;
}

struct DataContainer {
    BucketDict alloc_buckets;
    BucketDict flop_buckets;
    EquityClusters flop_clusters;
    EquityClusters turn_clusters;
    EquityClusters river_clusters;
    PreflopEquityDict preflop_equities;

    DataContainer(
        string alloc_buckets_filename, string preflop_equities_filename,
        string flop_buckets_filename, string turn_clusters_filename,
        string river_clusters_filename) {
        load_buckets_from_file(alloc_buckets_filename, alloc_buckets);
        load_preflop_equities_from_file(preflop_equities_filename, preflop_equities);
        load_buckets_from_file(flop_buckets_filename, flop_buckets);
        load_clusters_from_file(turn_clusters_filename, turn_clusters);
        load_clusters_from_file(river_clusters_filename, river_clusters);
    }
};

#endif
