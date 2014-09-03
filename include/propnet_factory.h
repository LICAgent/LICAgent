#ifndef __PROPNET_FACTORY_H__
#define __PROPNET_FACTORY_H__

#include "propnet.h"
#include "propnet_struct.h"

class propnet;

class propnet_factory{
public:
    propnet_factory(gdl* g,int sc);
    ~propnet_factory();

    propnet* create_propnet(const char* kif);

private:
    bool build_net();
    bool flatten(const char* kif);

    node* add_node(const gdl_term* t);
    node* get_node(const gdl_term* t);
    node* not_in_tail(const gdl_term* t);

    void add_const_true_and_false();
    and_node* new_and_node();
    or_node* new_or_node();
    not_node* new_not_node();

    void rewrite_id();
    void simplify();
    bool check_consistency();

    void mark_legal_nodes();

    bool topo_sort();
    bool check_order_consistency();

    void build_compac();

    gdl* flat_game;

    gdl* game;
    int setup_clock;
};

#endif
