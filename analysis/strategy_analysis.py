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
import re

import matplotlib
from matplotlib.patches import Rectangle
#matplotlib.use('agg')
plt.rcParams.update({
    'figure.figsize': (14, 9),
    'axes.xmargin': 0,
    'font.size': 16
})

RANKS = {k:v for k,v in zip('23456789TJQKA',list(range(13)))}
CARDS = {f'{r}{s}': eval7.Card(f'{r}{s}') for r in RANKS.keys() for s in 'schd'}

hand_odds = np.load('preflop_equities.npy')
#get the preflop equity for a pair of cards
def get_equity(hand):
	rank_a = RANKS[hand[0][0]]
	rank_b = RANKS[hand[1][0]]
	suited = hand[0][1]==hand[1][1]
	if suited == 0:
		return hand_odds[min(rank_a,rank_b)][max(rank_a,rank_b)]
	return hand_odds[max(rank_a,rank_b)][min(rank_a,rank_b)]

def get_vpips_preflop_actions(path):
	'''
	Read through logfile and output change in chip count as a function of hands.
	'''
	with open(path) as file:
		content = file.read()

    # count hands
	lines = content.splitlines()[2:]
	for line in lines:
		if '#' in line:
			num_rounds = int(line.split('#')[1].split(',')[0])

	# initialize deltas with bonus chips in pot
	deltas = {'A': np.zeros(num_rounds), 'B': np.zeros(num_rounds)}
	vpips = {'A':[],'B':[]} #fractional vpip over time
	vpips_total = {'A':0,'B':0} #total vpip times
	walks = {'A':0,'B':0} #how many times A or B got a walk

	#record actions as a function of preflop equity bucket
	sb_actions = {'A':[[] for i in range(10)],'B':[[] for i in range(10)]} #0 = fold, 1 = call, 2 = raise
	bb_actions = {'A':[[] for i in range(10)],'B':[[] for i in range(10)]} #0 = fold, 1 = call, 2 = raise, 3 = check
	action_nums = {'folds':0,'calls':1,'raises':2,'checks':3,'ran':0}

	round_num = 0
	street = 'preflop'
	sb = ''
	bb = ''
	line_ind = 0
	while line_ind < len(lines):
		#check for start of a hand
		if '#' in lines[line_ind]:
			round_num += 1
			street = 'preflop'
			sb = lines[line_ind].strip().split()[2]
			bb = lines[line_ind].strip().split()[4]
		#check for end of a hand
		elif 'awarded' in lines[line_ind]:
			deltas[sb][round_num-1] = int(lines[line_ind].split()[-1])
			deltas[bb][round_num-1] = int(lines[line_ind+1].split()[-1])
			line_ind += 2
			continue
		else:
			line_ind += 1
			continue

		sb_hand_pre = re.sub('[\[\]]','',lines[line_ind+3]).split()[2:]
		bb_hand_pre = re.sub('[\[\]]','',lines[line_ind+4]).split()[2:]

		#record actions as a fn of preflop hand equity
		sb_actions[sb][int(get_equity(sb_hand_pre)//10)].append(action_nums[lines[line_ind+5].split()[1]])
		if not 'folds' in lines[line_ind+5]: #if sb folded then nothing to record for bb
			bb_actions[bb][int(get_equity(bb_hand_pre)//10)].append(action_nums[lines[line_ind+6].split()[1]])

		#sb folds, bb gets walk
		if 'folds' in lines[line_ind+5]:
			walks[bb] += 1
			vpips[sb].append(vpips_total[sb]/(round_num-walks[sb]))
			vpips[bb].append(vpips[bb][-1])
			line_ind += 6
			continue
		#sb calls or raises, counts as a vpip
		elif 'calls' in lines[line_ind+5] or 'raises' in lines[line_ind+5]:
			vpips_total[sb] += 1
			vpips[sb].append(vpips_total[sb]/(round_num-walks[sb]))

		#if bb calls a raises or raises themselves, that's a vpip
		if 'calls' in lines[line_ind+6] or 'raises' in lines[line_ind+6]:
			vpips_total[bb] += 1

		vpips[bb].append(vpips_total[bb]/(round_num-walks[bb]))

		line_ind += 7

	return vpips, sb_actions, bb_actions

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

def plot_vpips(vpip_data,filepath):
	'''
	Plot vpip for each player over each hand
	'''
	plt.figure()
	for l in ['A','B']:
		plt.plot(vpip_data[l],label = l)
	plt.legend(loc = 0)
	plt.xlabel('No. of hands')
	plt.ylabel('VPIP (%)')
	plt.grid(which='both')
	plt.tight_layout()
	plt.savefig(filepath)

def plot_preflop_actions(sb,bb,filepath):
	'''
	Plot preflop actions vs hand equity
	'''
	fig,(ax1,ax2) = plt.subplots(1,2)

	for z,ax,pos in [[0,ax1,sb],[1,ax2,bb]]:
		for x,player in enumerate(['A','B']):
			for i in range(10):
				actions = pos[player][i]
				total = len(actions)
				if total == 0:continue
				
				folds = actions.count(0)/total * 100
				calls = actions.count(1)/total * 100
				raises = actions.count(2)/total * 100
				if z==1: checks = actions.count(3)/total*100
				print(player,folds,calls,raises)

				if i == 5 and player == 'A':

					ax.add_patch(Rectangle((5+10*i-1+x,0),1,folds,color='C0',label='Folds'))
					ax.add_patch(Rectangle((5+10*i-1+x,folds),1,calls,color='C1',label='Calls'))
					ax.add_patch(Rectangle((5+10*i-1+x,folds+calls),1,raises,color='C2',label='Raises'))
					if z==1: ax.add_patch(Rectangle((5+10*i-1+x,folds+calls+raises),1,checks,color='C3',label='Checks'))
				else:
					ax.add_patch(Rectangle((5+10*i-1+x,0),1,folds,color='C0'))
					ax.add_patch(Rectangle((5+10*i-1+x,folds),1,calls,color='C1'))
					ax.add_patch(Rectangle((5+10*i-1+x,folds+calls),1,raises,color='C2'))
					if z==1: ax.add_patch(Rectangle((5+10*i-1+x,folds+calls+raises),1,checks,color='C3'))
				if x == 0:
					ax.plot([5+10*i]*2,[0,100],'k-',linewidth=2)

		ax.grid(which='both')
		ax.set_xlim([30,110])
		ax.set_ylim([-10,110])
		ax.legend(loc=0)
		ax.set_xlabel('Preflop Equity')

	ax1.set_title('Small Blind Action (left = A, right = B)')
	ax2.set_title('Big Blind Action (left = A, right = B)')
	plt.tight_layout()
	plt.savefig(filepath)

def plot_preflop_actions_v2(sb,bb,filepath):
	'''
	Plot preflop actions as filled ranges
	'''
	fig,axs = plt.subplots(2,2)
	for z,plyr in enumerate(['A','B']):
		plt.subplot(2,2,z+1)
		actions = sb[plyr]
		totals = [max(1,len(actions[i])) for i in range(10)]
		folds = [actions[i].count(0)/totals[i]*100 for i in range(10)]
		calls = [actions[i].count(1)/totals[i]*100+folds[i] for i in range(10)]
		raises = [actions[i].count(2)/totals[i]*100+calls[i] for i in range(10)]
		folds = folds[3:-1]
		calls = calls[3:-1]
		raises = raises[3:-1]

		x = [35,45,55,65,75,85]
		plt.fill_between(x,[0]*6,folds,color='C0')
		plt.fill_between(x,folds,calls,color='C1')
		plt.fill_between(x,calls,[100]*6,color='C2')
		plt.xlabel('Preflop equity SB')
		plt.ylabel('Action percentage')
		plt.title(f'Player {plyr}')

		if z == 1:
			plt.scatter([0,0],[0,1],c='C0',label='Fold')
			plt.scatter([0,0],[0,1],c='C1',label='Calls')
			plt.scatter([0,0],[0,1],c='C2',label='Raises')
			plt.xlim([35,85])
			plt.legend(loc = 0)

	for z,plyr in enumerate(['A','B']):
		plt.subplot(2,2,2+z+1)
		actions = bb[plyr]
		totals = [max(1,len(actions[i])) for i in range(10)]
		folds = [actions[i].count(0)/totals[i]*100 for i in range(10)]
		calls = [actions[i].count(1)/totals[i]*100+folds[i] for i in range(10)]
		raises = [actions[i].count(2)/totals[i]*100+calls[i] for i in range(10)]
		checks = [actions[i].count(3)/totals[i]*100+raises[i] for i in range(10)]

		folds = folds[3:-1]
		calls = calls[3:-1]
		raises = raises[3:-1]
		checks = checks[3:-1]


		x = [35,45,55,65,75,85]
		plt.fill_between(x,[0]*6,folds,color='C0')
		plt.fill_between(x,folds,calls,color='C1')
		plt.fill_between(x,calls,raises,color='C1')
		plt.fill_between(x,raises,[100]*6,color='C3')
		plt.xlabel('Preflop equity BB')
		plt.ylabel('Action percentage')
		plt.title(f'Player {plyr}')

		if z == 1:
			plt.scatter([0,0],[0,1],c='C0',label='Fold')
			plt.scatter([0,0],[0,1],c='C1',label='Calls')
			plt.scatter([0,0],[0,1],c='C2',label='Raises')
			plt.scatter([0,0],[0,1],c='C3',label='Checks')
			plt.xlim([35,85])
			plt.legend(loc = 0)

	plt.tight_layout()
	plt.savefig(filepath)




if __name__ == "__main__":
    # command line arguments
	parser = argparse.ArgumentParser()
	parser.add_argument("logpath", help="path to the logfile")
	parser.add_argument("-o", "--out", help="prefix for output")
	parser.add_argument("-s","--show",help="automatically show plots")
	args = parser.parse_args()

	if not os.path.exists('output'):
		os.makedirs('output')
	out_prefix = args.out if args.out else f"output/{os.path.splitext(os.path.basename(args.logpath))[0]}"

    # parse logs
	deltas,vpips, sb_actions, bb_actions = parse_logs(args.logpath)

    #plot
	#plot_chips(deltas, f'{out_prefix}_chips.png')
	plot_vpips(vpips,f'{out_prefix}_vpips.png')
	plot_preflop_actions_v2(sb_actions,bb_actions,f'{out_prefix}_preflop_actions.png')

	if args.show:
		plt.show()
