from math import ceil
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
from tqdm import tqdm

from infoset_functions import *

N_CKPT = 100000000
N_CKPT_NUM = 15

# define checkpoint files to read
INFOSET_PATH = Path('../../data/cfr_data')
ckpts = list(range(N_CKPT, (N_CKPT_NUM+1)*N_CKPT, N_CKPT))
infoset_filenames = [
    f'v4_infosets_150post_ckpt{n}.txt' 
    for n in ckpts
]

prev_actions = {}
action_changes = np.zeros(len(ckpts)-1)

# loop over each infoset checkpoint, counting changed actions for each checkpoint
for i, f in enumerate(infoset_filenames):
    cur_actions = {}

    print("Loading", INFOSET_PATH / f, "...")
    with open(INFOSET_PATH / f) as file:
        for line in tqdm(file.readlines()):
            d = line.strip().split(' ')
            try:
                key = int(d[0])
                t = int(d[1])
                strategy = [float(x) for x in d[3::2]]
                regrets = [float(x) for x in d[2::2]]

                action = purify_action(t, regrets, strategy, key)
                cur_actions[key] = action

                if i > 0 and (key not in prev_actions or action != prev_actions[key]):
                    action_changes[i-1] += 1
            except ValueError:
                continue
    prev_actions = cur_actions

scaled_iters = np.array(ckpts)/1e6
plt.rcParams.update({
    'figure.figsize': (10, 7),
    'axes.xmargin': 0,
    'font.size': 11,
    'legend.fontsize': 8,
    'xtick.labelsize': 7,
    'ytick.labelsize': 7,
    'axes.grid.which': 'both'
})
plt.figure()
plt.plot(scaled_iters[1:], action_changes/1e3, '.--')
plt.xlabel('CFR iterations/1M')
plt.ylabel('Number of action changes between checkpoints in purified strategies')
plt.ylim([0, None])
plt.tight_layout()
plt.savefig('output/total_convergence.png')
plt.show()