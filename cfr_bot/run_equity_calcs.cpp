#include <iostream>
#include <vector>
#include "compute_equity.h"

using namespace std;

string DATA_PATH = "../../data/equity_data/";

const int N_RUNOUTS = 40000000;

int main() {
    
    save_equities_to_file(DATA_PATH + "flop_equities.txt", 3);
    save_equities_to_file_monte_carlo<N_RUNOUTS>(DATA_PATH + "turn_equities.txt", 4);
    save_equities_to_file_monte_carlo<N_RUNOUTS>(DATA_PATH + "river_equities.txt", 5, 0);

    return 0;
}