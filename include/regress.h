#ifndef __REGRESS_H__
#define __REGRESS_H__

#include "game.h"
#include "gdl_flatten.h"
#include "controller.h"

class rnode;

class regress{
public:
    static rnode* regress_term(gdl* g,domains_type* dom,const gdl_term* head,timer* _T,double lim);

    static timer* T;
    static double time_limit;

    static gdl* game;
    static domains_type* domain;
};

typedef enum{
    RN_AND = 100,
    RN_OR,
    RN_NOT,
    RN_ATOM,
    RN_FACT,
} rnode_type;

class rnode{
public:
    rnode(const rnode& rt);

    rnode(rnode_type rt);
    rnode(const gdl_term* _t, bool fact = false);
    ~rnode();

    gdl_term* compose();
    
    rnode_type type(){ return rtype; }
    bool flat(){ return rtype == RN_ATOM; }

    static bool atom(const gdl_term* t);

    static rnode* rnode_from_head(const gdl_term* head);
    static rnode* rnode_from_body(gdl_rule* rule);

    string convertS();

    int gauge_com();
    double eval(terms* state);

    static rnode* simplify(rnode* rn);

private:
    double eval_r(unordered_set<pred_wrapper>& uset_s);
    gdl_ast* compose_ast();

    vector<rnode*> children;
    gdl_ast* ast;
    rnode_type rtype;
};

#endif
