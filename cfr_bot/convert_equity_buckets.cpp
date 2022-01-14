#include <chrono>
#include "cfr.h"
#include "binary.h"

using namespace std;
using namespace std::chrono;

string DATA_PATH = "../../data/equity_data/";

int main(int argc, char *argv[]) {

    BucketDict flop_buckets;

    string load_path = DATA_PATH + argv[1] + ".txt";
    string save_path = DATA_PATH + argv[1] + ".bin";

    // load equity buckets from text file
    cout << "Loading from " << load_path << endl;

    auto start = high_resolution_clock::now();
    load_buckets_from_file(load_path, flop_buckets);
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

    cout << "Loaded " << flop_buckets.size() << " in " << duration.count() << " ms." << endl;

    // save equity buckets to binary
    start = high_resolution_clock::now();
    save_buckets_to_file_bin(save_path, flop_buckets);
    stop = high_resolution_clock::now();
    duration = duration_cast<milliseconds>(stop - start);

    cout << "Saved to " << save_path << " in " << duration.count() << " ms." << endl;

    // re-load equity buckets from binary to check load time
    start = high_resolution_clock::now();
    load_buckets_from_file_bin(save_path, &flop_buckets);
    stop = high_resolution_clock::now();
    duration = duration_cast<milliseconds>(stop - start);

    cout << "Re-loaded " << flop_buckets.size() << " from binary in " << duration.count() << " ms." << endl;

    return 0;
}