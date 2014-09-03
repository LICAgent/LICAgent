#ifndef __MCTS_STRUCT_H__
#define __MCTS_STRUCT_H__

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vector>
#include "controller.h"
#include "config.h"
#include "common.h"
#include "mcts.h"

using std::map;
using std::unordered_map;
using std::unordered_set;
using std::pair;
using std::set;
using std::vector;

class mcts_node;

struct mcts_stack_node{
    mcts_node* node;
    vector<int> choice;

    void print() const;
};

struct mcts_node_edge{
    mcts_node_edge(mcts_node* n,const vector<int> c)
    :node(n)
    ,choices(c)
    {}

    mcts_node* node;
    vector<int> choices;
};

struct state_key{
    explicit state_key(terms* s)
    :state(s)
    {}

    terms* state;

    static bool init_zobrist(terms* bases);
    static void dealloc_zobrist();

    static bool use_zobrist_hash;
    static map<string,int> zobrist_tab;
};

namespace std{
    template<>
    class equal_to<state_key>{
        public:
            bool operator()(const state_key& k1,const state_key& k2) const;
    };

    template<>
    class hash<state_key>{
        public:
            std::size_t operator()(const state_key& k) const;
    };

    template<>
    class hash<vector<int> >{
        public:
        size_t operator()(const vector<int>& e) const{
            size_t ret = 0;
            for(int i=0;i<e.size();i++){
                ret ^= e[i];
            }

            return ret;
        }
    };

    template<>
    class equal_to<vector<int> >{
        public:
        bool operator()(const vector<int>& e1,const vector<int>& e2) const{
            if(e1.size() != e2.size()) return false;
            for(int i=0;i<e1.size();i++){
                if(e1[i] != e2[i])
                    return false;
            }

            return true;
        }
    };

    template<>
    class hash<mcts_node_edge>{
        public:
        size_t operator()(const mcts_node_edge& edge) const{
            return hash<vector<int> >()(edge.choices);
        }
    };

    template<>
    class equal_to<mcts_node_edge>{
        public:
        bool operator()(const mcts_node_edge& ne1,const mcts_node_edge& ne2) const{
            const vector<int>& e1 = ne1.choices;
            const vector<int>& e2 = ne2.choices;

            return equal_to<vector<int> >()(e1,e2);
        }
    };

};

typedef vector<unordered_set<pred_wrapper>*> rave_set;

typedef unordered_map<state_key,mcts_node*> state_node_type;
typedef pair<state_key,mcts_node*> state_node_pair;

typedef unordered_set<mcts_node_edge> edge_set;

//========================================================================

typedef struct edge_info{
    edge_info()
    :Q(0.0),Q2(0.0),bestQ(0.0)
    ,raveQ(0.0),regQ(0.0)
    ,use_rave(false),use_reg(false)
    ,N(0),sgamma(0.0),rgamma(0.0)
    {}

    double Q,Q2,bestQ;
    double raveQ, regQ;

    bool use_rave, use_reg;

    int N;
    double sgamma,rgamma;
} edge_info;

class mcts_player;

class mcts_node{
public:
    mcts_node(terms* s,mcts_player* pl);
    ~mcts_node();

    void init(controller* con);
    void init_joint_choice(int i,int n,vector<int>& p);

    //main entry points
    void UCB_select(vector<int>& choice,double Cp);
    gdl_term* choose_best(int i);
    mw_vec* get_mws(int i);

    //helper function for UCB_select
    int UCB1(int i,double Cp);

    double get_score(int i,int j,int Nisum,double Cp);

    terms* move_from_choice(const vector<int>& choice);
    int choice_from_move(const terms* move,vector<int>& choices);
    mcts_node* get_child(const vector<int>& choices);

    bool is_expanded();
    void expand(mcts_node* child,const vector<int>& choices);
    void fill_regress(mcts_node* child,const vector<int>& choices);

    void update_stats(const vector<int>& choice,const vector<double>& outcome,double gamma);
    void update_rave(const vector<int>& choice,const vector<double>& outcome,double gamma, const rave_set& playout_moves);

    int all_joint_moves;
    bool use_joint_choices;
    unordered_set<vector<int> > joint_choices;

    vector<int> expanded_count;

    //edge statistics
    vector2d<edge_info>::type edges; //[role_index][terms_index]

    double get_Q(int i,int j);
    double get_Q2(int i,int j);
    double get_best(int i,int j);
    int get_N(int i,int j);
    double get_G(int i,int j);

    double get_Q_(int i,int j);

    void updateQ(int i,int j,double v,double gamma);
    void update_raveQ(int i,int j,double v,double gamma);

    //edges between nodes
    edge_set children;
    edge_set parents;

    vector<terms*>* moves; //indexed by role_index

    gdl_term* get_move(int i,int j);
    terms* get_imove(int i);

    //state of this node
    terms* state;
    bool terminal;
    vector<int> gvs;

    //helper member variable used in inc_ref
    bool mark; //avoid cycles in inc_ref
    int ref;

    //main player
    mcts_player* play;

    static bool same_pointer_flag;
};

#endif
