#ifndef __PROPNET_H__
#define __PROPNET_H__

#include "controller.h"
#include <unordered_map>
#include <unordered_set>
#include <tr2/dynamic_bitset>

#include "config.h"
#include "common.h"

#include "propnet_factory.h"
#include "propnet_struct.h"

using std::unordered_map;
using std::unordered_set;
using std::tr2::dynamic_bitset;

//used in gdb
void print_wire_id(int id);
void print_node_id(int id);

class propnet: public controller{
public:
    propnet();
    ~propnet();

    static void clear_propnet();

    bool init();
    static void handle_isolated_nodes();

    bool terminal();
    int goal(const string& role);
    bool legal(const string& role, const gdl_term& move);

    vector<string> roles();
    terms* state();
    terms* get_move(const string& role);

    int make_move(const terms&);
    int retract(const terms&);
    int retract_and_move(const terms&, const terms&);

    terms* get_bases();
    terms* get_inputs(const string& role);

    void print_net(FILE* fp);
    void print_bases(FILE* fp);
    void print_nexts(FILE* fp);

    propnet* duplicate();

    void begin_bench();
    void end_bench();

private:
    static node* find_node_from_term(gdl_term* t);

    static prop_node* bases_from_next(prop_node* n);
    static prop_node* nexts_from_base(prop_node* n);
    static action_node* legals_from_does(action_node* n);

    static prop_node* bases_from_term(const gdl_term* t);
    static action_node* does_from_role_term(const string& r,const gdl_term* t);
    static action_node* legal_from_role_term(const string& r,const gdl_term* t);

    goal_node* goals_from_role(const string& r);

    void clear_state();
    void process(int id);
    void propagate(const unordered_set<int>& base_set, const unordered_set<int>& does_set);
    void first_propagate();

    //debug
    void check_consistency();

//set of all nodes
    static vector<node*> all_nodes;
    static unordered_map<pred_wrapper, node*> pred_map; //only a partial map, does not include connectives

//subsets of nodes
    static set<node*> bases;
    static set<node*> inits; //inits is a subset of bases
    static set<node*> nexts;

    static set<node*> does;
    static set<node*> legals;

    static set<node*> goals;
    static node* term;

    static set<node*> conns;

//special constant nodes
    static node* const_true;
    static node* const_false;

//list of roles
    static vector<string> _roles;

//marked legals & does
    static unordered_set<int> marked_legals;

//more compact representation of wires
    static vector2d<int>::type compac_outputs;
    static vector<int> compac_inputs_count;
    static vector<int> compac_input;
    static vector<node_type> compac_types;

//non-static members:
    double propagate_time;

//current state
    unordered_set<int> cur_state;
    unordered_set<int> cur_legals;
    unordered_set<int> cur_nexts;
    unordered_set<int> cur_goals;

    dynamic_bitset<> *prev_vals;
    dynamic_bitset<> *vals;
    unordered_map<int,int> conn_counts;

    void init_dynamic();
    void clear_dynamics();

    void inc_count(int i);
    void dec_count(int i);
    int get_count(int i);

    void set_bit(int i,int v);
    int get_bit(int i);

    int get_prev_bit(int i);
    void set_prev_bit(int i,int v);

    friend class propnet_factory;
    friend void print_wire_id(int id);
    friend void print_node_id(int id);
};

#endif
