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
import copy

import matplotlib

STACK_SIZE = 200

def parse_logs(path):
    '''
    Read through logfile and output change in chip count as a function of hands.
    '''
    with open(path) as file:
        content = file.read()

    # get opponent name
    opp = {'A': 'B', 'B': 'A'}

    # count hands
    lines = content.splitlines()
    for line in lines:
        if '#' in line:
            num_rounds = int(line.split('#')[1].split(',')[0])

    # initialize deltas with bonus chips in pot
    deltas = {'A': np.zeros(num_rounds+1), 'B': np.zeros(num_rounds+1)}

    # save hand histories for sorting later
    histories = []   

    round_num = 0
    first_action = {'A': True, 'B': True}
    only_folding = {'A': 0, 'B': 0}

    street = ''
    streets = ['Preflop', 'Flop', 'Turn', 'River']
    won_by_street = {'A': {x: 0 for x in streets}, 'B': {x: 0 for x in streets}}
    won_by_street_checkpoint = copy.deepcopy(won_by_street)
    for line in lines:
        # start of round
        # Round #1, A (0), B (0)
        if '#' in line:
            round_num += 1

            street = streets[0]

            histories += ['']    

            first_action = {'A': True, 'B': True}
        elif (any([x in line for x in streets[1:]])
                    and line.split(' ')[-1] != f'({STACK_SIZE})'):
            street = line.split(' ')[0]
        elif 'awarded' in line:
            awarded = int(line.split(' ')[-1])
            deltas[line[0]][round_num] = awarded
            won_by_street[line[0]][street] += awarded
        elif 'folds' in line:
            if only_folding[line[0]] is None and first_action[line[0]]:
                only_folding[line[0]] = round_num
                won_by_street_checkpoint = copy.deepcopy(won_by_street)
        elif ('bets' in line or 'calls' in line or 'raises' in line) and first_action[line[0]]:
            only_folding[line[0]] = None
            first_action[line[0]] = False
        if len(histories) > 0:
            histories[-1] += line + '\n'
        
    # get last round before check/folding began
    max_round = round_num
    if only_folding['A'] is not None or only_folding['B'] is not None:
        max_round = min([only_folding[k] for k in only_folding if only_folding[k] is not None])
    else:
        won_by_street_checkpoint = won_by_street
    
    # sort hand logs by amount won/lost
    histories = [s for _, s in sorted(zip(np.abs(deltas['A'][1:]), histories), reverse=True)]

    return deltas, histories, won_by_street_checkpoint, max_round

def plot_chips(deltas, max_round, filepath):
    '''
    Plot chips gained over each hand.
    '''
    plt.figure()
    for l in ['A', 'B']:
        plt.plot(np.cumsum(deltas[l]), label=l)

    plt.axvline(x=max_round, c='gray', ls='--')

    plt.legend(loc='best')
    plt.xlabel('No. of hands')
    plt.ylabel('Amount won [chips]')
    plt.grid(which='both')
    plt.tight_layout()
    plt.savefig(filepath)

def plot_delta_hist(deltas, max_round, filepath, bins=80):
    '''
    Plot histogram of pots won.
    '''
    plt.figure()
    plt.title('Distribution of pots won')
    for l in ['A', 'B']:
        pots = deltas[l][:max_round]
        plt.hist(pots[pots>0], bins=bins, alpha=0.5, label=l)

    plt.legend(loc='best')
    plt.xlabel('Pot size')
    plt.ylabel('Number of pots')
    plt.grid(which='both')
    plt.tight_layout()
    plt.savefig(filepath)

def plot_won_by_street(won_by_street, filepath):
    '''
    Plot histogram of pots won by street.
    '''
    plt.figure()
    plt.title('Distribution of pots won by street')
    for l in ['A']:
        w = won_by_street[l]
        plt.bar(list(w.keys()), list(w.values()), alpha=0.5, label=l)

    plt.legend(loc='best')
    plt.xlabel('Street')
    plt.ylabel('Amount won [chips]')
    plt.grid(which='both')
    plt.tight_layout()
    plt.savefig(filepath)

if __name__ == "__main__":
    # command line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("logpath", help="path to the logfile")
    parser.add_argument("-o", "--out", help="prefix for output")
    parser.add_argument("-w", "--window", help="show chip plot in window", dest='window', action='store_true')
    parser.set_defaults(window=False)
    args = parser.parse_args()

    if not args.window:
        matplotlib.use('agg')
    plt.rcParams.update({
        'figure.figsize': (11, 7),
        'axes.xmargin': 0,
        'font.size': 16
    })

    
    if not os.path.exists('output'):
        os.makedirs('output')
    out_prefix = args.out if args.out else f"output/{os.path.splitext(os.path.basename(args.logpath))[0]}"

    # parse logs
    deltas, histories, won_by_street, max_round = parse_logs(args.logpath)

    # plot
    plot_chips(deltas, max_round, f'{out_prefix}_chips.png')
    plot_delta_hist(deltas, max_round, f'{out_prefix}_deltas.png')
    plot_won_by_street(won_by_street, f'{out_prefix}_won_by_street.png')

    # save hand histories sorted by amount won/lost
    with open(f'{out_prefix}_sorted.txt', 'w') as file:
        file.write('\n'.join(histories))

    if args.window:
        plt.show()
