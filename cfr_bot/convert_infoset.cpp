#include <chrono>
#include "cfr.h"

using namespace std;
using namespace std::chrono;

string DATA_PATH = "../../data/cfr_data/";

int main(int argc, char *argv[]) {

    InfosetDict infosets;

    string load_path = DATA_PATH + argv[1] + ".txt";
    string save_path = DATA_PATH + argv[1] + ".bin";

    cout << "Loading from " << load_path << endl;

    auto start = high_resolution_clock::now();
    load_infosets_from_file(load_path, infosets);
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

    cout << "Loaded " << infosets.size() << " in " << duration.count() << " ms." << endl;

    start = high_resolution_clock::now();
    save_infosets_to_file_bin(save_path, infosets);
    stop = high_resolution_clock::now();
    duration = duration_cast<milliseconds>(stop - start);

    cout << "Saved to " << save_path << " in " << duration.count() << " ms." << endl;

    start = high_resolution_clock::now();
    load_infosets_from_file_bin(save_path, &infosets);
    stop = high_resolution_clock::now();
    duration = duration_cast<milliseconds>(stop - start);

    cout << "Re-loaded " << infosets.size() << " from binary in " << duration.count() << " ms." << endl;

    return 0;
}