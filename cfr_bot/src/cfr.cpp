#include <algorithm>

#include "cfr.h"

using namespace std;

random_device CFR_RD;
mt19937 CFR_GEN(CFR_RD());
uniform_real_distribution<> CFR_RAND(0, 1);

//////////////////////////////////////////
/////////// pure CFR infoset /////////////
//////////////////////////////////////////

CFRInfosetPure::CFRInfosetPure() {}

CFRInfosetPure::CFRInfosetPure(int init_action) : action(init_action) {}

int CFRInfosetPure::get_action_index_avg() {
    return action;
}

CFRInfosetPure purify_infoset(CFRInfoset full_infoset, ULL key,
                                double fold_threshold,
                                int visit_count_threshold) {

    bool is_facing_bet = key_is_facing_bet(key);

    double bet_probability = 0;
    vector<double> strategy = full_infoset.get_avg_strategy();

    // if fold threshold is exceeded, just fold;
    // if fold threshold = 0.5, behaviour is equivalent to continuing here.
    // set fold threshold below 0.5 to bias the bot to a conservative playstyle
    if (is_facing_bet && strategy[1] > fold_threshold) {
        return CFRInfosetPure(1);
    }

    // sum up probabilities of aggressive actions
    double max_bet_probability = 0;
    int max_bet_index = 0;
    for (int i = 1 + ((is_facing_bet) ? 1 : 0); i < strategy.size(); i++) {
        bet_probability += strategy[i];

        if (strategy[i] > max_bet_probability) {
            max_bet_probability = strategy[i];
            max_bet_index = i;
        }
    }

    // build meta-probabilities of check/call, [fold], bet/raise
    vector<double> meta_probabilities;
    meta_probabilities.push_back(strategy[0]);
    if (is_facing_bet) {
        meta_probabilities.push_back(strategy[1]);
    }
    meta_probabilities.push_back(bet_probability);

    // get index of most probable meta-action
    int pure_action = distance(meta_probabilities.begin(),
            max_element(meta_probabilities.begin(), meta_probabilities.end())
        );

    // if the chosen action is the last action (i.e. to bet), then return the
    // index of the most probable bet action
    // otherwise return the chosen action (check/call or fold)
    int action = (pure_action == meta_probabilities.size()-1 
                    && max_bet_index != 0) 
        ? max_bet_index : pure_action;

    // if we haven't visited this node much, our cumulative strategy won't
    // have converged. As a heuristic, we should switch to a fold
    // if the fold cumulative regret is lower than that of our chosen action
    if (full_infoset.t < visit_count_threshold
        && is_facing_bet
        && full_infoset.cumu_regrets[1] > full_infoset.cumu_regrets[action]) {
        action = 1;
    }

    return CFRInfosetPure(action);

}

//////////////////////////////////////////
///// full CFR infoset for training //////
//////////////////////////////////////////

CFRInfoset::CFRInfoset() {}

CFRInfoset::CFRInfoset(int num_actions)
        : cumu_regrets(num_actions, 0), cumu_strategy(num_actions, 0) {}

CFRInfoset::CFRInfoset(vector<double> init_cumu_regrets, vector<double> init_cumu_strategy, int init_t)
        : cumu_regrets(init_cumu_regrets), cumu_strategy(init_cumu_strategy), t(init_t) {}

void CFRInfoset::record(vector<double> regrets, vector<double> strategy) {
    double new_weight = 1 / (double) (t+1);
    double old_weight = (double) t / (double) (t+1);

    // cout.precision(20);
    // cout << "init cumu_regrets " << cumu_regrets << endl;
    // cout << "record[t=" << t << "] regrets " << regrets << endl;
    assert(regrets.size() == strategy.size());
    assert(regrets.size() == cumu_regrets.size());
    assert(regrets.size() == cumu_strategy.size());
    for (int i = 0; i < regrets.size(); i++) {
        cumu_regrets[i] = regrets[i] * new_weight + cumu_regrets[i] * old_weight;
        cumu_strategy[i] = strategy[i] * new_weight + cumu_strategy[i] * old_weight;
    }
    // cout << "final cumu_regrets " << cumu_regrets << endl;
    t += 1;
}

int CFRInfoset::get_action_index_avg() {
    double norm = accumulate(cumu_strategy.begin(), cumu_strategy.end(), 0.0);
    if (norm > 0.0) {
        discrete_distribution<> d(cumu_strategy.begin(), cumu_strategy.end());
        return d(CFR_GEN);
    }
    else {
        uniform_int_distribution<> d(0, cumu_strategy.size()-1);
        return d(CFR_GEN);
    }
}

vector<double> CFRInfoset::get_avg_strategy() {
    double norm = accumulate(cumu_strategy.begin(), cumu_strategy.end(), 0.0);
    vector<double> strategy(cumu_strategy.size(), 1./cumu_strategy.size());
    if (norm > 0.0) {
        for (int i = 0; i < cumu_strategy.size(); ++i) {
            strategy[i] = cumu_strategy[i] / norm;
        }
    }
    return strategy;
}

int CFRInfoset::get_action_index(double eps) {
    // epsilon-greedy: take random action with prob eps
    if (CFR_RAND(CFR_GEN) < eps) {
        return CFR_GEN() % cumu_regrets.size();
    }

    vector<double> strategy = get_regret_matching_strategy();
    discrete_distribution<> d(strategy.begin(), strategy.end());

    return d(CFR_GEN);
}

vector<double> CFRInfoset::get_regret_matching_strategy() {
    double total_pos_regrets = 0;
    vector<double> cumu_pos_regrets = cumu_regrets;
            
    for (int i = 0; i < cumu_regrets.size(); i++) {
        cumu_pos_regrets[i] = max(cumu_pos_regrets[i], 0.0);
        total_pos_regrets += cumu_pos_regrets[i];
    }

    vector<double> strategy(cumu_regrets.size(), 1. / (double) cumu_regrets.size());

    if (total_pos_regrets != 0) {
        for (int i = 0; i < cumu_regrets.size(); i++) {
            strategy[i] = cumu_pos_regrets[i] / total_pos_regrets;
        }
    }

    return strategy;
}

int get_bucket_from_clusters(
    ULL hand, ULL board, int num_board,
    const EquityClusters &clusters, int iterations,
    ULL common_dead, ULL villain_dead) {

    double equity, dist;

    int bucket = -1;
    double smallest_dist = clusters.size();

    vector<double> distance_squared;
    for (int i = 0; i < clusters.size(); i++) distance_squared.push_back(0);

    for (int i = 0; i < NUM_RANGES; i++) {
        if (iterations > 0) {
            equity = hand_vs_range_monte_carlo(hand, RANGES[i], NUM_RANGE[i],
                                                board, num_board, iterations,
                                                common_dead, villain_dead);
        }
        else {
            equity = hand_vs_range_exact(hand, RANGES[i], NUM_RANGE[i],
                                            board, num_board,
                                            common_dead, villain_dead);
        }

        for (int j = 0; j < clusters.size(); j++) {
            dist = equity - clusters[j][i];
            distance_squared[j] += dist*dist;

            if (i == NUM_RANGES-1) {
                if (distance_squared[j] < smallest_dist) {
                    bucket = j;
                    smallest_dist = distance_squared[j];
                }
            }
        }
    }
    assert(bucket != -1);

    return bucket;
}

ULL info_to_key(int player_ind, int street, int card_info, BoardActionHistory &history) {
    int action_bits;
    ULL key = 0;

    int shift = 0;

    assert(player_ind >= 0);
    key |= player_ind;
    shift += SHIFT_PLAYER_IND;

    assert(street >= 0);
    ULL street_bits = (ULL) street << shift;
    assert((key & street_bits) == 0);
    key |= street_bits;
    shift += SHIFT_STREET;

    assert(card_info >= 0);
    ULL card_bits = (ULL) card_info << shift;
    assert((key & card_bits) == 0);
    key |= card_bits;
    shift += SHIFT_CARD_INFO;

    for (int i = 0; i < history.actions.size(); i++) {
        assert(history.actions[i] > 0);

        action_bits = history.actions[i];
        // change the action ID to fit within 3 bits
        // so our first bet size will have the same action id as our first raise
        // but context will still produce unique keys
        // (unique IDs between bets and raises are still used in code logic)
        if (action_bits >= RAISE) {
            action_bits -= BET_SIZES.size();
        }

        ULL act_bits = (((ULL) action_bits) << shift);
        assert((key & act_bits) == 0);
        key |= act_bits;
        shift += SHIFT_ACTIONS;
    }
    assert(shift <= 64); // check for ULL overflow

    return key;
}

ULL info_to_key(ULL history_key, int card_info) {
    ULL key = history_key;
    
    assert(card_info >= 0);
    ULL card_bits = (ULL) card_info << SHIFT_CARD_INFO_TOTAL;
    assert((key & card_bits) == 0);
    key |= card_bits;
    
    return key;
}