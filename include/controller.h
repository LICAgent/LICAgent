#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__ 

#include "config.h"
#include "game.h"

typedef vector<gdl_term*> terms;
typedef vector<gdl_term*>::iterator terms_iter;

class controller{
public:
    virtual ~controller(){}
    virtual bool init(){ return true; }

    virtual bool terminal() = 0;
    virtual int goal(const string& role) = 0;
    virtual bool legal(const string& role, const gdl_term& move) = 0;

    virtual vector<string> roles() = 0;
    virtual terms* state() = 0;
    virtual terms* get_move(const string& role) = 0;

    virtual int make_move(const terms&) = 0;
    virtual int retract(const terms&) = 0;
    virtual int retract_and_move(const terms&, const terms&);

    virtual terms* get_bases() = 0;
    virtual terms* get_inputs(const string&) = 0;

    vector<terms*>* all_moves(); //list of available moves of each role

    static void print_terms(const terms* t,FILE* fp);
    static void print_terms_stdout(const terms* t);

    static void free_moves(vector<terms*>* ltl);
    static void free_terms(terms* t);
    static terms* copy_terms(const terms* t);
};

#endif
