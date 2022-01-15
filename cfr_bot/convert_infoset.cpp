#include <chrono>
#include "cfr.h"
#include "binary.h"

using namespace std;
using namespace std::chrono;

string DATA_PATH = "../../data/cfr_data/";

int main(int argc, char *argv[]) {

    InfosetDict infosets;

    string load_path = DATA_PATH + argv[1] + ".txt";
    string save_path = DATA_PATH + argv[1] + ".bin";
    string save_path_pure = DATA_PATH + argv[1] + ".bin.pure";

    // load infosets from text file
    cout << "Loading from " << load_path << endl;

    auto start = high_resolution_clock::now();
    load_infosets_from_file(load_path, infosets);
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

    cout << "Loaded " << infosets.size() << " in " << duration.count() << " ms." << endl;

    // convert full CFR infoset to purified infoset
    InfosetDictPure infosets_pure;
    int pruned = 0;
    for (pair<ULL, CFRInfoset> infoset : infosets) {
        // only save infosets that actually have strategic information
        if (infoset.second.t > 1) {
            infosets_pure[infoset.first] = purify_infoset(infoset.second,
                                                            infoset.first);
        }
        else {
            pruned++;
        }
    }
    cout << pruned << " empty infostates removed from purified infoset." << endl;

    // save infosets to binary
    start = high_resolution_clock::now();
    save_infosets_to_file_bin(save_path, infosets);
    stop = high_resolution_clock::now();
    duration = duration_cast<milliseconds>(stop - start);

    cout << "Saved to " << save_path << " in " << duration.count() << " ms." << endl;

    // re-load infosets from binary to check load time
    start = high_resolution_clock::now();
    load_infosets_from_file_bin(save_path, &infosets);
    stop = high_resolution_clock::now();
    duration = duration_cast<milliseconds>(stop - start);

    cout << "Re-loaded " << infosets.size() << " from binary in " << duration.count() << " ms." << endl;

    // save purified infosets to binary
    start = high_resolution_clock::now();
    save_infosets_to_file_bin(save_path_pure, infosets_pure);
    stop = high_resolution_clock::now();
    duration = duration_cast<milliseconds>(stop - start);

    cout << "Saved to " << save_path << " in " << duration.count() << " ms (purified)." << endl;

    // re-load purified infosets from binary to check load time
    start = high_resolution_clock::now();
    load_infosets_from_file_bin(save_path_pure, &infosets_pure);
    stop = high_resolution_clock::now();
    duration = duration_cast<milliseconds>(stop - start);

    cout << "Re-loaded " << infosets_pure.size() << " from binary in " << duration.count() << " ms (purified)." << endl;

    return 0;
}