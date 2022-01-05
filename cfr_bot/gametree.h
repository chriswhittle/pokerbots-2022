#ifndef REAL_POKER_GAMETREE
#define REAL_POKER_GAMETREE

#include "cfr.h"

using namespace std;

struct GameTreeNode {
    vector<GameTreeNode> children;
    ULL history_key; // infoset key without board_num and cardinfo

    // net change in chips for p1; assumes p1 wins
    // (should be negated if cards favour villain)
    int won;

    int ind;
    int button;
    int street;

    bool finished;
    bool showdown;

    GameTreeNode() {}

    GameTreeNode(ULL init_key, int init_won, int init_ind,
                int init_button, int init_street,
                bool init_finished, bool init_showdown) :
                history_key(init_key), won(init_won), ind(init_ind),
                button(init_button), street(init_street), 
                finished(init_finished), showdown(init_showdown) {}

    GameTreeNode(ULL init_history_key, BoardActionHistory &history) :
                history_key(init_history_key), won(history.won[0]),
                ind(history.ind), button(history.button), street(history.street),
                finished(history.finished), showdown(history.showdown) {}
};

inline ostream& operator<<(ostream& os, GameTreeNode& p) {
    os << "GameTreeNode(key=" << p.history_key << ",";
    os << "num_children=" << p.children.size() << ",";
    os << "street=" << p.street << ",";
    os << "finished=" << p.finished << ",";
    os << "showdown=" << p.finished << ")";
    return os;
}

// recurse down game tree node
inline GameTreeNode build_game_tree(BoardActionHistory history) {

    // make infoset key without board_num or card infostate
    ULL key = info_to_key(history.ind ^ history.button, 0, history.street, 0, history);
    GameTreeNode new_node(key, history);

    vector<int> available_actions = history.get_available_actions();

    assert(available_actions.size() > 0 || history.finished);

    for (int i = 0; i < available_actions.size(); i++) {
        BoardActionHistory new_history = history;
        new_history.update(available_actions[i]);
        new_node.children.push_back(build_game_tree(new_history));
    }

    return new_node;

}



#endif
