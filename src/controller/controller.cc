#include "controller.h"

vector<terms*>* controller::all_moves(){ //list of available moves of each role
    vector<terms*>* ret = new vector<terms*>();
    vector<string> r = roles();
    vector<string>::iterator it = r.begin();

    while(it != r.end()){
        terms* t = get_move(*it);

        assert(t);
        ret->push_back(t);

        it++;
    }

    return ret;
}

void controller::free_moves(vector<terms*>* ltl){
    vector<terms*>::iterator it = ltl->begin();
    while(it != ltl->end()){
        free_terms(*it);
        it++;
    }

    delete ltl;
}

void controller::print_terms(const terms* t,FILE* fp){
    vector<gdl_term*>::const_iterator it = t->begin();
    while(it != t->end()){
        fprintf(fp,"%s\n",(*it)->convertS().c_str());
        it++;
    }
}

void controller::print_terms_stdout(const terms* t){
    print_terms(t,stdout);
}

void controller::free_terms(terms* t){
    vector<gdl_term*>::iterator it = t->begin();
    while(it != t->end()){
        delete *it;
        it++;
    }

    delete t;
}

terms* controller::copy_terms(const terms* t){
    terms* ret = new terms();

    vector<gdl_term*>::const_iterator it = t->begin();
    while(it != t->end()){
        gdl_term* term = *it;
        ret->push_back(new gdl_term(*term));

        it++;
    }

    return ret;
}

int controller::retract_and_move(const terms& state, const terms& moves){
    int r = retract(state);
    if(r){
        r = make_move(moves);
    }

    return r;
}
