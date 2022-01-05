#ifndef EVAL7PP_EVALUATE
#define EVAL7PP_EVALUATE

#include "arrays.h"

extern int CLUB_OFFSET;
extern int DIAMOND_OFFSET;
extern int HEART_OFFSET;
extern int SPADE_OFFSET;

extern int HANDTYPE_SHIFT;
extern int TOP_CARD_SHIFT;
extern int SECOND_CARD_SHIFT;
extern int THIRD_CARD_SHIFT;
extern int CARD_WIDTH;
extern unsigned int TOP_CARD_MASK;
extern unsigned int SECOND_CARD_MASK;
extern unsigned int FIFTH_CARD_MASK;

extern unsigned int HANDTYPE_VALUE_STRAIGHTFLUSH;
extern unsigned int HANDTYPE_VALUE_FOUR_OF_A_KIND;
extern unsigned int HANDTYPE_VALUE_FULLHOUSE;
extern unsigned int HANDTYPE_VALUE_FLUSH;
extern unsigned int HANDTYPE_VALUE_STRAIGHT;
extern unsigned int HANDTYPE_VALUE_TRIPS;
extern unsigned int HANDTYPE_VALUE_TWOPAIR;
extern unsigned int HANDTYPE_VALUE_PAIR;
extern unsigned int HANDTYPE_VALUE_HIGHCARD;

int evaluate(unsigned long long cards, unsigned int num_cards);

inline int evaluate(unsigned long long cards) {
    return evaluate(cards, __builtin_popcountll(cards)); // needs gcc
}

#endif
