#include <cassert>

#include "game.h"
#include "cfr.h"
#include "compute_equity.h"

#include "tqdm.h"

using namespace std;

string DATA_PATH = "../../pokerbots-2021-data/cpp/equity_data/";

// same as in game.h but return the index and not the bucket
int to_alloc_infostate(array<array<int, HAND_SIZE>, NUM_BOARDS_> &c) {
    int index = 0;
    int mult = 1;
    for (auto& hand : c) {
        int i = get_cards_info_state_preflop(hand);
        index += i * mult;
        mult *= NUM_RANKS*NUM_RANKS;
    }
    return index;
}

array<int, HAND_SIZE> from_preflop_index(int ind) {
    int rank1 = ind / NUM_RANKS;
    int rank2 = ind % NUM_RANKS;

    if (rank1 == rank2) { // pair
        return {rank1, rank2 + NUM_RANKS};
    }
    else if (rank1 > rank2) { // suited
        return {rank1, rank2};
    }
    else { // offsuit
        return {rank1, rank2+NUM_RANKS};
    }
}

int to_combinatorics_index(array<int, HAND_SIZE> h) {
    int rank1 = max(h[0]%NUM_RANKS, h[1]%NUM_RANKS);
    int rank2 = min(h[0]%NUM_RANKS, h[1]%NUM_RANKS);

    if (rank1 == rank2 || h[0]/NUM_RANKS != h[1]/NUM_RANKS) {
        return rank1 + NUM_RANKS*NUM_SUITS*(rank2+13);
    }
    else {
        return rank1 + NUM_RANKS*NUM_SUITS*rank2; // suited
    }
}


int main() {

    DataContainer data(
        DATA_PATH + "alloc_buckets_25.txt",
        DATA_PATH + "preflop_equities.txt",
        DATA_PATH + "flop_buckets_10.txt",
        DATA_PATH + "turn_clusters_10.txt",
        DATA_PATH + "river_clusters_10.txt");
    
    string filename = "full_alloc_equities.txt";

    EquityDict allocs_equities;
    load_equities_from_file(DATA_PATH + "alloc_equities.txt", allocs_equities);

    EquityDict full_allocs_equities;
    tqdm pbar;
    array<array<int, HAND_SIZE>, NUM_BOARDS_> hands;
    vector<float> equities;
    for (int i = 0; i < allocs_equities.size(); i++) {
        hands[0] = from_preflop_index(i);
        for (int j = i; j < allocs_equities.size(); j++) {
            pbar.progress(j, allocs_equities.size());
            hands[1] = from_preflop_index(j);
            for (int k = j; k < allocs_equities.size(); k++) {
                hands[2] = from_preflop_index(k);

                array<int, HAND_SIZE*NUM_BOARDS_> full_hands;
                array<float, NUM_BOARDS_> pair_equities;
                for (int l = 0; l < NUM_BOARDS_; l++) {
                    full_hands[2*l] = hands[l][0];
                    full_hands[2*l+1] = hands[l][1];

                    pair_equities[l] = data.preflop_equities.at(preflop_card_indices_to_index(hands[l][0], hands[l][1]));
                }
                
                // sort each pair by equity
                array<int, HAND_SIZE*NUM_BOARDS_> new_full_hands;
                int a = 0;
                for (auto b : sort_indexes<float, NUM_BOARDS_>(pair_equities)) {
                    new_full_hands[a++] = full_hands[2*b];
                    new_full_hands[a++] = full_hands[2*b+1];
                }

                array<array<int, HAND_SIZE>, NUM_BOARDS_> ordered_hands;
                allocate_hands(ALLOCATIONS[0], new_full_hands, ordered_hands);

                int ind = to_alloc_infostate(ordered_hands);
                vector<float> e(NUM_BOARDS_*NUM_RANGES);
                full_allocs_equities[ind] = e;
                for (int l = 0; l < NUM_BOARDS_; l++) {
                    equities = allocs_equities.at(to_combinatorics_index(ordered_hands[l]));
                    for (int m = 0; m < NUM_RANGES; m++) {
                        full_allocs_equities[ind][l*NUM_RANGES + m] = equities[m];
                    }
                }
            }
        }
    }

    cout << "Saving..." << endl;
    {
        ofstream outfile(filename);
        outfile << full_allocs_equities;
    }

    return 0;
}