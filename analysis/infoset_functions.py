import numpy as np

NUM_RANKS = 13
RANKS = '23456789TJQKA'

# bit shifts used for decoding infoset key
PLAYER_SHIFT = 1
STREET_SHIFT = 2
CARD_SHIFT = 13
ACTION_SHIFT = 3

# given the infoset key, are we facing a bet?
# used to determine if fold is valid action
def key_is_facing_bet(key):
    key = key >> (PLAYER_SHIFT+STREET_SHIFT+CARD_SHIFT)

    if key == 0:
        return True

    while key != 0:
        last_action = key % (2**ACTION_SHIFT)
        key = key >> ACTION_SHIFT
    
    return last_action > 2

# given action probabilities, return index of the most probable action
# logic copied from cfr.cpp
def purify_action(t, regrets, strategy, key, fold_threshold = 0.5,
                    visit_count_threshold = 100):
    is_facing_bet = key_is_facing_bet(key)

    # if fold threshold is exceeded, just fold;
    # if fold threshold = 0.5, behaviour is equivalent to continuing here.
    # set fold threshold below 0.5 to bias the bot to a conservative playstyle
    if is_facing_bet and strategy[1] > fold_threshold:
        return 1

    # sum up probabilities of aggressive actions
    bet_probability = 0
    max_bet_probability = 0
    max_bet_index = 0
    for i  in range(1 + is_facing_bet, len(strategy)):
        bet_probability += strategy[i]

        if strategy[i] > max_bet_probability:
            max_bet_probability = strategy[i]
            max_bet_index = i

    # build meta-probabilities of check/call, [fold], bet/raise
    meta_probabilities = [strategy[0]]
    if is_facing_bet:
        meta_probabilities += [strategy[1]]
    meta_probabilities += [bet_probability]

    # get index of most probable meta-action
    pure_action = np.argmax(meta_probabilities)

    # if the chosen action is the last action (i.e. to bet), then return the
    # index of the most probable bet action
    # otherwise return the chosen action (check/call or fold)
    action = max_bet_index if pure_action == len(meta_probabilities)-1 else pure_action

    # if we haven't visited this node much, our cumulative strategy won't
    # have converged. As a heuristic, we should switch to a fold
    # if the fold cumulative regret is lower than that of our chosen action
    if t < visit_count_threshold and is_facing_bet and regrets[1] > regrets[action]:
        action = 1
        
    return action