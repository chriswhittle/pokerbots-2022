'''
Run e.g.

python read_logs.py game_log.txt
python read_logs.py game_log.txt -o output/output_prefix_here

Outputs:
-plot of chips as a function of hands for each player
-hand histories sorted by largest pots
'''

import os
import numpy as np
import matplotlib.pyplot as plt
import argparse
import eval7

import matplotlib
matplotlib.use('agg')
plt.rcParams.update({
    'figure.figsize': (14, 9),
    'axes.xmargin': 0,
    'font.size': 16
})

RANKS = {k:v for k,v in zip('23456789TJQKA',list(range(13)))}
CARDS = {f'{r}{s}': eval7.Card(f'{r}{s}') for r in RANKS.keys() for s in 'schd'}

def parse_logs(path):
    '''
    Read through logfile and output change in chip count as a function of hands.
    '''
    with open(path) as file:
        content = file.read()

    # count hands
    lines = content.splitlines()
    for line in lines:
        if '#' in line:
            num_rounds = int(line.split('#')[1].split(',')[0])

    # initialize deltas with bonus chips in pot
    deltas = {'A': np.zeros(num_rounds), 'B': np.zeros(num_rounds)}

    # save hand histories for sorting later
    histories = []   

    round_num = 0
    for line in lines:
        # start of round
        # Round #1, A (0), B (0)
        if '#' in line:
            round_num += 1

            histories += ['']
        elif 'awarded' in line:
            deltas[line[0]][round_num-1] = int(line.split(' ')[-1])

        if len(histories) > 0:
            histories[-1] += line + '\n'
    
    # sort hand logs by amount won/lost
    histories = [s for _, s in sorted(zip(np.abs(deltas['A']), histories), reverse=True)]

    return deltas, histories

def plot_chips(deltas, filepath):
    '''
    Plot chips gained over each hand.
    '''
    plt.figure()
    for l in ['A', 'B']:
        plt.plot(np.cumsum(deltas[l]), label=l)

    plt.legend(loc='best')
    plt.xlabel('No. of hands')
    plt.ylabel('Amount won [chips]')
    plt.grid(which='both')
    plt.tight_layout()
    plt.savefig(filepath)

if __name__ == "__main__":
    # command line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("logpath", help="path to the logfile")
    parser.add_argument("-o", "--out", help="prefix for output")
    args = parser.parse_args()

    if not os.path.exists('output'):
        os.makedirs('output')
    out_prefix = args.out if args.out else f"output/{os.path.splitext(os.path.basename(args.logpath))[0]}"

    # parse logs
    deltas, histories = parse_logs(args.logpath)

    # plot
    plot_chips(deltas, f'{out_prefix}_chips.png')

    # save hand histories sorted by amount won/lost
    with open(f'{out_prefix}_sorted.txt', 'w') as file:
        file.write('\n'.join(histories))
