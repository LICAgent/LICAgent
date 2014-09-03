#ifndef __PROPNET_SIM_H__
#define __PROPNET_SIM_H__

#include "game.h"
#include <set>
#include <sstream>
#include "config.h"

using std::set;
using std::stringstream;

typedef enum{
    BASE = 1,
    NEXT,
    VIEW,

    LEGAL,
    DOES,

    GOAL,
    TERM,

    AND,
    OR,
    NOT
} node_type;

class or_node;

class node{
public:
    node()
    {}

    virtual ~node(){}

    virtual node_type type() const = 0;

    int id;
    set<node*> inputs;
    set<node*> outputs;
};

class prop_node: public node{
public:
    prop_node(const gdl_ast* ast,node_type tp);
    ~prop_node();

    node_type type() const{ return typ; }

    gdl_ast* prop;
    node_type typ;
};

class action_node: public node{
public:
    action_node(const string& r,const gdl_ast* a,node_type tp);
    ~action_node();

    node_type type() const{ return typ; }

    string role;
    gdl_ast* action;
    node_type typ;
};

class goal_node: public node{
public:
    goal_node(const string& r,int gv){
        role = r;
        goal_value = gv;
    }
    ~goal_node(){ }

    node_type type() const{ return GOAL; }

    string role;
    int goal_value;
};

class not_node: public node{
public:
    node_type type() const{ return NOT; }
};

class and_node: public node{
public:
    int count;
    node_type type() const{ return AND; }
};

class or_node: public node{
public:
    int count;
    node_type type() const{ return OR; }
};

#endif
