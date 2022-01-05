#include "evaluate.h"

int CLUB_OFFSET = 0;
int DIAMOND_OFFSET = 13;
int HEART_OFFSET = 26;
int SPADE_OFFSET = 39;

int HANDTYPE_SHIFT = 24;
int TOP_CARD_SHIFT = 16;
int SECOND_CARD_SHIFT = 12;
int THIRD_CARD_SHIFT = 8;
int CARD_WIDTH = 4;
unsigned int TOP_CARD_MASK = 0x000F0000;
unsigned int SECOND_CARD_MASK = 0x0000F000;
unsigned int FIFTH_CARD_MASK = 0x0000000F;

unsigned int HANDTYPE_VALUE_STRAIGHTFLUSH = (( (unsigned int)8) << HANDTYPE_SHIFT);
unsigned int HANDTYPE_VALUE_FOUR_OF_A_KIND = (( (unsigned int)7) << HANDTYPE_SHIFT);
unsigned int HANDTYPE_VALUE_FULLHOUSE = (( (unsigned int)6) << HANDTYPE_SHIFT);
unsigned int HANDTYPE_VALUE_FLUSH = (( (unsigned int)5) << HANDTYPE_SHIFT);
unsigned int HANDTYPE_VALUE_STRAIGHT = (( (unsigned int)4) << HANDTYPE_SHIFT);
unsigned int HANDTYPE_VALUE_TRIPS = (( (unsigned int)3) << HANDTYPE_SHIFT);
unsigned int HANDTYPE_VALUE_TWOPAIR = (( (unsigned int)2) << HANDTYPE_SHIFT);
unsigned int HANDTYPE_VALUE_PAIR = (( (unsigned int)1) << HANDTYPE_SHIFT);
unsigned int HANDTYPE_VALUE_HIGHCARD = (( (unsigned int)0) << HANDTYPE_SHIFT);

int evaluate(unsigned long long cards, unsigned int num_cards) {
    unsigned int retval = 0, four_mask, three_mask, two_mask;
    
    unsigned int sc = (unsigned int)((cards >> (CLUB_OFFSET)) & 0x1fffUL);
    unsigned int sd = (unsigned int)((cards >> (DIAMOND_OFFSET)) & 0x1fffUL);
    unsigned int sh = (unsigned int)((cards >> (HEART_OFFSET)) & 0x1fffUL);
    unsigned int ss = (unsigned int)((cards >> (SPADE_OFFSET)) & 0x1fffUL);

    unsigned int ranks = sc | sd | sh | ss;
    unsigned int n_ranks = N_BITS_TABLE[ranks];
    unsigned int n_dups = (unsigned int)(num_cards - n_ranks);
    
    unsigned int st, t, kickers, second, tc, top;
    
    if (n_ranks >= 5) {
        if (N_BITS_TABLE[ss] >= 5) {
            if (STRAIGHT_TABLE[ss] != 0) {
                return HANDTYPE_VALUE_STRAIGHTFLUSH + (unsigned int)(STRAIGHT_TABLE[ss] << TOP_CARD_SHIFT);
            }
            else {
                retval = HANDTYPE_VALUE_FLUSH + TOP_FIVE_CARDS_TABLE[ss];
            }
        }
        else if (N_BITS_TABLE[sc] >= 5) {
            if (STRAIGHT_TABLE[sc] != 0) {
                return HANDTYPE_VALUE_STRAIGHTFLUSH + (unsigned int)(STRAIGHT_TABLE[sc] << TOP_CARD_SHIFT);
            }
            else {
                retval = HANDTYPE_VALUE_FLUSH + TOP_FIVE_CARDS_TABLE[sc];
            }
        }
        else if (N_BITS_TABLE[sd] >= 5) {
            if (STRAIGHT_TABLE[sd] != 0) {
                return HANDTYPE_VALUE_STRAIGHTFLUSH + (unsigned int)(STRAIGHT_TABLE[sd] << TOP_CARD_SHIFT);
            }
            else {
                retval = HANDTYPE_VALUE_FLUSH + TOP_FIVE_CARDS_TABLE[sd];
            }
        }
        else if (N_BITS_TABLE[sh] >= 5) {
            if (STRAIGHT_TABLE[sh] != 0) {
                return HANDTYPE_VALUE_STRAIGHTFLUSH + (unsigned int)(STRAIGHT_TABLE[sh] << TOP_CARD_SHIFT);
            }
            else {
                retval = HANDTYPE_VALUE_FLUSH + TOP_FIVE_CARDS_TABLE[sh];
            }
        }
        else {
            st = STRAIGHT_TABLE[ranks];
            if (st != 0) {
                retval = HANDTYPE_VALUE_STRAIGHT + (st << TOP_CARD_SHIFT);
            }

        if (retval != 0 && n_dups < 3) {
            return retval;
            }
        }
    }

    if (n_dups == 0) {
        return HANDTYPE_VALUE_HIGHCARD + TOP_FIVE_CARDS_TABLE[ranks];
    }
    else if (n_dups == 1) {
        two_mask = ranks ^ (sc ^ sd ^ sh ^ ss);
        retval = (unsigned int)(HANDTYPE_VALUE_PAIR + (TOP_CARD_TABLE[two_mask] << TOP_CARD_SHIFT));
        t = ranks ^ two_mask;
        kickers = (TOP_FIVE_CARDS_TABLE[t] >> CARD_WIDTH) & ~FIFTH_CARD_MASK;
        retval += kickers;
        return retval;
    }
    else if (n_dups == 2) {
        two_mask = ranks ^ (sc ^ sd ^ sh ^ ss);
        if (two_mask != 0) {
            t = ranks ^ two_mask;
            retval = (unsigned int)(HANDTYPE_VALUE_TWOPAIR
                + (TOP_FIVE_CARDS_TABLE[two_mask]
                & (TOP_CARD_MASK | SECOND_CARD_MASK))
                + (TOP_CARD_TABLE[t] << THIRD_CARD_SHIFT));
            return retval;
        }
        else {
            three_mask = ((sc & sd) | (sh & ss)) & ((sc & sh) | (sd & ss));
            retval = (unsigned int)(HANDTYPE_VALUE_TRIPS + (TOP_CARD_TABLE[three_mask] << TOP_CARD_SHIFT));
            t = ranks ^ three_mask;
            second = TOP_CARD_TABLE[t];
            retval += (second << SECOND_CARD_SHIFT);
            t ^= (1U << (int)second);
            retval += (unsigned int)(TOP_CARD_TABLE[t] << THIRD_CARD_SHIFT);
            return retval;
        }
    }
    else {
        four_mask = sh & sd & sc & ss;
        if (four_mask != 0) {
            tc = TOP_CARD_TABLE[four_mask];
            retval = (unsigned int)(HANDTYPE_VALUE_FOUR_OF_A_KIND
                + (tc << TOP_CARD_SHIFT)
                + ((TOP_CARD_TABLE[ranks ^ (1U << (int)tc)]) << SECOND_CARD_SHIFT));
            return retval;
        }
        two_mask = ranks ^ (sc ^ sd ^ sh ^ ss);
        if (N_BITS_TABLE[two_mask] != n_dups) {
            three_mask = ((sc & sd) | (sh & ss)) & ((sc & sh) | (sd & ss));
            retval = HANDTYPE_VALUE_FULLHOUSE;
            tc = TOP_CARD_TABLE[three_mask];
            retval += (tc << TOP_CARD_SHIFT);
            t = (two_mask | three_mask) ^ (1U << (int)tc);
            retval += (unsigned int)(TOP_CARD_TABLE[t] << SECOND_CARD_SHIFT);
            return retval;
        }
        if (retval != 0) {
            return retval;
        }
        else {
            retval = HANDTYPE_VALUE_TWOPAIR;
            top = TOP_CARD_TABLE[two_mask];
            retval += (top << TOP_CARD_SHIFT);
            second = TOP_CARD_TABLE[two_mask ^ (1 << (int)top)];
            retval += (second << SECOND_CARD_SHIFT);
            retval += (unsigned int)((TOP_CARD_TABLE[ranks ^ (1U << (int)top) ^ (1 << (int)second)]) << THIRD_CARD_SHIFT);
            return retval;
        }
    }

}
