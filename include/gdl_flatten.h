#ifndef __GDL_FLATTEN_H__
#define __GDL_FLATTEN_H__

#include "common.h"
#include "game.h"
#include "controller.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <set>

using std::set;
using std::unordered_map;
using std::unordered_set;
using std::string;
using std::pair;

typedef unordered_map<pred_index, unordered_set<string>* > domains_type;

typedef std::map<string,string> symtab_type;
typedef std::pair<string,string> spair;

class gdl_flatten{
public:
    gdl_flatten(gdl* g);
    ~gdl_flatten();

    void build_domain_graph();
    void initiate_domains();

    domains_type* build_and_get_domain(){
        build_domain_graph();
        initiate_domains();

        return &pi_inits;
    }

    int flatten(vector<gdl_elem*>& grounded_gdl);

    static void printS(const vector<gdl_elem*>& grounded_gdl,FILE* fp);

    static terms* extract_terms(const gdl_term* t);
    static terms* extract_terms(const gdl_rule* r);
private:
    void add_pi_from_term(const gdl_term* t);
    void add_domain_graph_edge(const gdl_term* t1, const gdl_term* t2);

    void substitute(gdl_rule* r,const symtab_type& symtab);

    void add_pi_edge(const pred_index& pi1,const pred_index& pi2);

    void initiate_variables_in_rule(const gdl_rule* r,unordered_set<rule_wrapper>* uset);
    unordered_set<rule_wrapper>* expand_rule(const gdl_rule* r);

    void add_grounded_rule(const gdl_rule* r,vector<gdl_elem*>& grounded_gdl);
    void trim_rule(gdl_rule* r);

    domains_type pi_inits;
    unordered_map<pred_index, unordered_set<pred_index>* > domain_graph;

    //predicate dependency graph
    void build_predicate_dep_graph();

    int build_pf_inits();
    int build_extended_facts_pf_inits();

    int propagate(const gdl_rule* r,bool loosely);
    int add_to_pf_inits(gdl_term* t);
    unordered_set<pred_wrapper>* get_inits(const pred_form& pf);

    int transfer(const string& from, const string& to);
    int legal_transfer();

    int strictly_illegal(const gdl_rule* r);
    int loosely_illegal(const gdl_rule* r);
    unordered_set<pred_form> extended_facts;

    int only_facts(const gdl_rule* r);
    void find_facts_and_extended_facts();
    void build_facts_pf_inits();
    unordered_set<pred_form> facts;
    unordered_map<pred_form, unordered_set<pred_wrapper>* > pf_inits;

    void add_seen_rule(const gdl_rule* r);
    int have_seen_rule(const gdl_rule* r);
    void clear_seen_rule();
    unordered_set<rule_wrapper> rules_seen;
    
    gdl* game;
    const timer* T;
    int setup_clock;

public:
//debug
    void print_domain_graph();
    void print_pi_inits();

    void print_pf_inits();
};

#endif
