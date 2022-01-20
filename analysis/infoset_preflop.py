import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
from matplotlib import cm
from matplotlib import colors
from tqdm import tqdm

from infoset_functions import *

def plot_action_grid(actions_grid, num_actions, is_facing_bet, save_path = None):
    cmap = cm.get_cmap('Pastel2')
    cmap = colors.ListedColormap(cmap.colors[:num_actions])

    plt.rcParams.update({
        'figure.figsize': (10, 7),
        'font.size': 11
    })
    fig, ax = plt.subplots()
    plt.pcolormesh(actions_grid, cmap=cmap, vmin=-0.5, vmax=num_actions-0.5)
    cbar = plt.colorbar()
    cbar.set_ticks(np.arange(num_actions))
    action_labels = []
    if is_facing_bet:
        action_labels += ['Call', 'Fold']
        action_labels += [f'Raise size #{i+1}' for i in range(num_actions-2)]
    else:
        action_labels += ['Check']
        action_labels += [f'Bet size #{i+1}' for i in range(num_actions-1)]
    cbar.set_ticklabels(action_labels)
    cbar.ax.tick_params(size=0)

    ax.xaxis.set_ticks_position('top')
    ax.xaxis.set_label_position('top')
    plt.xlabel('Suited')
    plt.ylabel('Off-suit')

    ticks = np.arange(NUM_RANKS) + 0.5
    tick_labels = list(RANKS)
    for axis in [ax.xaxis, ax.yaxis]:
        axis.set_ticks(ticks)
        axis.set_ticklabels(tick_labels)
        axis.set_tick_params(length=0)
    ax.invert_xaxis()

    plt.tight_layout()
    if save_path is not None:
        plt.savefig(save_path)

if __name__ == "__main__":
    # define checkpoint files to read
    INFOSET_PATH = Path('../../data/cfr_data')
    INFOSET_FILENAME = 'v4_infosets_150post_ckpt1600000000.txt'

    # load strategy from infosets file
    actions = {}
    strategies = {}
    print("Loading", INFOSET_PATH / INFOSET_FILENAME, "...")
    with open(INFOSET_PATH / INFOSET_FILENAME) as file:
        for line in tqdm(file.readlines()):
            d = line.strip().split(' ')
            try:
                key = int(d[0])
                t = int(d[1])
                strategy = [float(x) for x in d[3::2]]
                regrets = [float(x) for x in d[2::2]]

                strategies[key] = strategy
                actions[key] = purify_action(t, regrets, strategy, key)
            except ValueError:
                continue

    actions_grid = np.zeros((NUM_RANKS, NUM_RANKS))

    # get preflop SB strategy
    for i in range(NUM_RANKS):
        for j in range(NUM_RANKS):
            key = (i*NUM_RANKS + j) << (PLAYER_SHIFT + STREET_SHIFT)
            actions_grid[i,j] = actions[key]
    num_actions = len(strategies[key])

    plot_action_grid(actions_grid, num_actions, key_is_facing_bet(key),
                        'output/range_open.png')

    # get BB 3bet strategy
    for i in range(NUM_RANKS):
        for j in range(NUM_RANKS):
            key = 1
            key |= (i*NUM_RANKS + j) << (PLAYER_SHIFT + STREET_SHIFT)
            key |= 3 << (PLAYER_SHIFT + STREET_SHIFT + CARD_SHIFT)

            actions_grid[i,j] = actions[key]
    num_actions = len(strategies[key])

    plot_action_grid(actions_grid, num_actions, key_is_facing_bet(key),
                        'output/range_3bet.png')

    # get SB 4bet strategy
    for i in range(NUM_RANKS):
        for j in range(NUM_RANKS):
            key = (i*NUM_RANKS + j) << (PLAYER_SHIFT + STREET_SHIFT)
            key |= 3 << (PLAYER_SHIFT + STREET_SHIFT + CARD_SHIFT)
            key |= 3 << (PLAYER_SHIFT + STREET_SHIFT + CARD_SHIFT + ACTION_SHIFT)

            actions_grid[i,j] = actions[key]
    num_actions = len(strategies[key])

    plot_action_grid(actions_grid, num_actions, key_is_facing_bet(key),
                        'output/range_4bet.png')
    plt.show()