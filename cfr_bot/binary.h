#ifndef REAL_POKER_BINARY
#define REAL_POKER_BINARY

#include "cfr.h"
#include "compute_equity.h"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>

using namespace std;

///////////////////////////////////////
////// CFR infoset serialization //////
///////////////////////////////////////

// full CFR infoset serialization
BOOST_SERIALIZATION_SPLIT_FREE(CFRInfoset);
namespace boost {
    namespace serialization {

        template<class Archive>
        void save(Archive & ar, const CFRInfoset & infoset,
                        const unsigned int version) {

            ar & infoset.t;

            vector<unsigned short> new_cumu_strategy;
            for (int i = 0; i < infoset.cumu_strategy.size(); i++) {
                new_cumu_strategy.push_back(infoset.cumu_strategy[i] * USHRT_MAX);
            }

            ar & new_cumu_strategy;

        }

        template<class Archive>
        void load(Archive & ar, CFRInfoset & infoset,
                        const unsigned int version) {
            
            ar & infoset.t;

            vector<unsigned short> cumu_strategy;
            ar & cumu_strategy;

            vector<double> new_cumu_strategy;
            for (int i = 0; i < cumu_strategy.size(); i++) {
                new_cumu_strategy.push_back(((double) cumu_strategy[i]) / USHRT_MAX);
            }

            infoset.cumu_strategy = new_cumu_strategy;

        }

    }
}

// pure CFR infoset serialization
BOOST_SERIALIZATION_SPLIT_FREE(CFRInfosetPure);
namespace boost {
    namespace serialization {
        template<class Archive>
        void save(Archive & ar, const CFRInfosetPure & infoset,
                        const unsigned int version) {
            ar & infoset.action;
        }
        template<class Archive>
        void load(Archive & ar, CFRInfosetPure & infoset,
                        const unsigned int version) {
            ar & infoset.action;
        }
    }
}

// saving/loading full infosets to/from binary files
template<class T>
inline void save_infosets_to_file_bin(string filename, unordered_map<ULL, T>& infoset_dict) {
    ofstream filestream(filename);
    boost::archive::binary_oarchive archive(filestream,
                                            boost::archive::no_codecvt);

    archive << infoset_dict;
}

template<class T>
inline void load_infosets_from_file_bin(string filename, unordered_map<ULL, T>* infoset_dict) {
    ifstream filestream(filename);
    boost::archive::binary_iarchive archive(filestream,
                                            boost::archive::no_codecvt);

    archive >> *infoset_dict;
}

//////////////////////////////////////////
////// Equity buckets serialization //////
//////////////////////////////////////////

// saving/load BucketDict to/from binary
inline void save_buckets_to_file_bin(string filename, BucketDict& bucket_dict) {
    ofstream filestream(filename);
    boost::archive::binary_oarchive archive(filestream,
                                            boost::archive::no_codecvt);

    archive << bucket_dict;
}


inline void load_buckets_from_file_bin(string filename, BucketDict* bucket_dict) {
    ifstream filestream(filename);
    boost::archive::binary_iarchive archive(filestream,
                                            boost::archive::no_codecvt);

    archive >> *bucket_dict;
}

#endif