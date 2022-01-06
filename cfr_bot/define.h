#ifndef REAL_POKER_DEFINE
#define REAL_POKER_DEFINE

#include <iostream>
#include <array>
#include <vector>
#include <unordered_set>
#include <numeric>
#include <algorithm>

#include "eval7pp.h"

using namespace std;

// shorthand for long type names
using ULL = unsigned long long;

// define fixed ranges for k-means
constexpr int NUM_RANGES = 8;
extern const int NUM_RANGE[8];
extern unsigned long long RANGES[8][460];

// nice array printing
template<typename T, size_t N>
inline ostream& operator<<(ostream& os, const array<T, N>& p) {
    os << "[";
    for (int i = 0; i < N; i++) {
        if (i != 0) {
            os << ", ";
        }
        os << p[i];
    }
    os << "]";
    return os;
}
template<typename T>
inline ostream& operator<<(ostream& os, const vector<T>& p) {
    os << "[";
    for (int i = 0; i < p.size(); i++) {
        if (i != 0) {
            os << ", ";
        }
        os << p[i];
    }
    os << "]";
    return os;
}
template<typename S, typename T>
inline ostream& operator<<(ostream& os, const pair<S, T>& p) {
    os << "(" << p.first << ", " << p.second << ")";
    return os;
}

template <typename T,typename U>
inline pair<T,U> operator+(const pair<T,U> & l, const pair<T,U> & r) {
    return {l.first+r.first,l.second+r.second};
}

template <typename T,typename U>
inline pair<U,U> operator*(const T & l, const pair<U,U> & r) {
    return {l*r.first,l*r.second};
}

// https://stackoverflow.com/questions/1577475/c-sorting-and-keeping-track-of-indexes
template <typename T, int N>
inline array<size_t, N> sort_indexes(const array<T, N> &v) {

  // initialize original index locations
  array<size_t, N> idx;
  iota(idx.begin(), idx.end(), 0);

  // sort indexes based on comparing values in v
  // using std::stable_sort instead of std::sort
  // to avoid unnecessary index re-orderings
  // when v contains elements of equal values
  stable_sort(idx.begin(), idx.end(),
       [&v](size_t i1, size_t i2) {return v[i1] < v[i2];});

  return idx;
}

template <typename T>
inline bool in_vector(const vector<T>& v, const T& elt) {
    return find(v.begin(), v.end(), elt) != v.end();
}

template <typename T>
inline bool in_set(const unordered_set<T>& v, const T& elt) {
    return v.find(elt) != v.end();
}

extern const string suits[];
extern const string ranks[];
inline string pretty_card(int c) {
    return ranks[c % 13] + suits[c / 13];
}


#endif
