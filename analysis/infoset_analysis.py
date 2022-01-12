from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt

plt.rcParams.update({
    'figure.figsize': (14, 9),
    'axes.xmargin': 0,
    'font.size': 16,
    'axes.grid.which': 'both'
})

INFOSET_PATH = Path('../../data/cfr_data')
INFOSET_FILENAME = 'overbet_swap_infosets_16bit_500post_ckpt2000000000.txt'

print(f'Reading from {INFOSET_FILENAME}...')

visit_count_dict = {}
visit_counts = []

uniform_count = 0
with open(INFOSET_PATH / INFOSET_FILENAME) as file:
    for line in file.readlines():
        d = line.replace('\n', '').split(' ')

        visit_count = int(d[1])

        visit_counts += [visit_count]

        if visit_count in visit_count_dict:
            visit_count_dict[visit_count] += 1
        else:
            visit_count_dict[visit_count] = 1
        
        if all([d[3]==d[3+i*2] for i in range((len(d)-2)//2)]):
            uniform_count += 1

print(f'{uniform_count} strategies were uniform ({100*uniform_count/len(visit_counts):.1f}%)')

for n in [0, 1]:
    print(f'{visit_count_dict[n]} visited {n} times')

N_BINS = 50
logbins = np.logspace(0, np.log10(np.max(visit_counts)), N_BINS)

plt.figure()
plt.hist(visit_counts, bins=logbins)
plt.xlabel('Visit counts')
plt.xscale('log')
plt.show()