from math import ceil
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt

from infoset_functions import *

N_CKPT = 100000000
N_CKPT_NUM = 14

# define keys of interest to look at
keys = [
    168 << 3, # AA on BTN
    1 << 3, # 23o on BTN
    137 << 3, # Q9s on BTN
    469134607325,
    1850794632153519,
    2513475667670,
    56460775150,
    90812056150
]

# define checkpoint files to read
INFOSET_PATH = Path('../../data/cfr_data')
ckpts = list(range(N_CKPT, (N_CKPT_NUM+1)*N_CKPT, N_CKPT))
infoset_filenames = [
    f'v4_infosets_150post_ckpt{n}.txt' 
    for n in ckpts
]

keys_set = set(keys)
probabilities = []
visits = []

# loop over each infoset checkpoint, getting strategy for each key
for f in infoset_filenames:
    probabilities += [ {} ]
    visits += [ {} ]

    print("Loading", INFOSET_PATH / f, "...")
    with open(INFOSET_PATH / f) as file:
        for line in file.readlines():
            d = line.strip().split(' ')
            try:
                cur_key = int(d[0])
                if cur_key in keys_set:
                    probabilities[-1][cur_key] = [float(x) for x in d[3::2]]
                    visits[-1][cur_key] = int(d[1])

                    if len(probabilities[-1]) == len(keys):
                        break
            except ValueError:
                continue

N_COLS = 4
scaled_iters = np.array(ckpts)/1e6
plt.rcParams.update({
    'figure.figsize': (12, 8),
    'axes.xmargin': 0,
    'font.size': 11,
    'legend.fontsize': 8,
    'xtick.labelsize': 7,
    'ytick.labelsize': 7,
    'axes.grid.which': 'both'
})
plt.figure()
for i, key in enumerate(keys):
    num_actions = len(probabilities[0][key])
    plt.subplot(int(ceil(len(keys)/N_COLS)), min(len(keys), N_COLS), i+1)
    plt.title(f'{key}')
    
    if key_is_facing_bet(key):
        actions = ['call', 'fold']
        actions += [f'raise size #{j+1}' for j in range(num_actions-2)]
    else:
        actions = ['check']
        actions += [f'bet size #{j+1}' for j in range(num_actions-1)]

    for j in range(num_actions):
        plt.plot(scaled_iters, 
                [probabilities[f][key][j] 
                    for f in range(len(infoset_filenames))],
                '.--', label=actions[j])


    plt.xlabel('CFR iterations/1M')
    plt.ylabel('Action probabilities')
    plt.legend(loc='best')
    
    ax2 = plt.gca().twiny()
    ax2.set_xticks(scaled_iters)
    ax2.set_xticklabels(
        [str(visits[f][key]) for f in range(len(infoset_filenames))],
        rotation=45)
    ax2.set_xlabel('Visit counts')
plt.tight_layout()
plt.show()