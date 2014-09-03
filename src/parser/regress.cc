#include "regress.h"
#include <algorithm>

//temporary time_limit
//#define NO_TIME_LIMIT

typedef std::map<string,string> symtab_type;
typedef std::pair<string,string> spair;

timer* regress::T = NULL;
double regress::time_limit = 0.0;
gdl* regress::game = NULL;
domains_type* regress::domain = NULL;

static int unify(const gdl_ast* base, const gdl_ast* var, symtab_type& symtab){
    if(base->type() == AST_ATOM){
        if(var->type() == AST_ATOM){
            string svar = var->convertS();
            string bvar = base->convertS();

            if(svar[0] == '?'){
                symtab.insert(spair(svar,bvar));
            } else {
                return bvar == svar;
            }
        } else {
            return 0;
        }
    }

    if(base->children.size() != var->children.size()){
        return 0;
    }

    for(int i=0;i<base->children.size();i++){
        gdl_ast* bc = base->children[i];
        gdl_ast* vc = var->children[i];

        if(!unify(bc,vc,symtab)){
            return 0;
        }
    }

    return 1;
}

static gdl_ast* traverse(gdl_ast* t,symtab_type& symtab){
    if(t->type() == AST_ATOM){
        string tvar = t->convertS();
        if(tvar[0] == '?'){
            if(symtab.find(tvar) != symtab.end()){
                string svar = symtab[tvar];
                delete t;

                gdl_ast* ret = new gdl_ast(svar.c_str());
                return ret;
            }
        }

        return t;
    }

    for(int i=0;i<t->children.size();i++){
        t->children[i] = traverse(t->children[i],symtab);
    }

    return t;
}

static void substitute(gdl_rule* r,symtab_type& symtab){
    r->head->ast = traverse(r->head->ast,symtab);
    for(int i=0;i<r->tails.size();i++){
        r->tails[i]->ast = traverse(r->tails[i]->ast,symtab);
    }
}

rnode* regress::regress_term(gdl* g,domains_type* dom,const gdl_term* head,timer* _T,double lim){
    regress::game = g;
    regress::T = _T;
    regress::time_limit = lim;
    regress::domain = dom;

    rnode* ret = rnode::rnode_from_head(head);

    if(ret){
        ret = rnode::simplify(ret);
    }

    return ret;
}

//=======================================

rnode::rnode(rnode_type rt){
    assert(rt != RN_ATOM);

    ast = NULL;
    rtype = rt;
}

rnode::rnode(const gdl_term* _t,bool fact){
    ast = new gdl_ast(*(_t->ast));
    rtype = fact ? RN_FACT : RN_ATOM;
}

rnode::~rnode(){
    for(int i=0;i<children.size();i++){
        delete children[i];
    }

    if(ast)
        delete ast;
}

rnode::rnode(const rnode& rt){
    rtype = rt.rtype;
    if(rt.ast){
        ast = new gdl_ast(*rt.ast);
    } else {
        ast = NULL;
    }

    for(int i=0;i<rt.children.size();i++){
        children.push_back(new rnode(*rt.children[i]));
    }
}

double rnode::eval(terms* state){
    unordered_set<pred_wrapper> uset;
    for(int i=0;i<state->size();i++){
        pred_wrapper pw(state->at(i));
        uset.insert(pw);
    }

    return eval_r(uset);
}

//@TODO: tune
double q = 2.0;
double t = 0.6;
double th = 0.2;

static double F(double a,double b){
    double a_q = pow(a,q);
    double b_q = pow(b,q);
    return pow(a_q + b_q, 1.0/q);
}

static double T_prime(double a,double b){
    return std::max(0.0,1.0 - F(1-a,1-b));
}

static double T(double a,double b){
    if(a > t && b > t){
        return std::max(t,T_prime(a,b));
    }

    return T_prime(a,b);
}

static double S(double a,double b){
    return 1 - T(1-a,1-b);
}

double rnode::eval_r(unordered_set<pred_wrapper>& uset_s){
    double ret = 0.0;
    switch(type()){
        case RN_AND:
        {
            ret = 1.0;
            for(int i=0;i<children.size();i++){
                double b = children[i]->eval_r(uset_s);
                ret = T(ret,b);
            }
            break;
        }
        case RN_OR:
        {
            ret = 0.0;
            for(int i=0;i<children.size();i++){
                double b = children[i]->eval_r(uset_s);
                ret = S(ret,b);
            }
            break;
        }
        case RN_NOT:
        {
            ret = 1.0 - children[0]->eval_r(uset_s);
            break;
        }
        case RN_ATOM:
        {
            gdl_term* t = new gdl_term(ast);
            pred_wrapper pw(t);
            if(uset_s.find(pw) == uset_s.end()){
                ret = th;
            } else {
                ret = 1-th;
            }

            delete t;
            break;
        }
        case RN_FACT:
        {
            ret = 1-th;
            break;
        }
    }

    return ret;
}

int rnode::gauge_com(){
    int ret = 0;
    switch(type()){
        case RN_AND:
        {
            for(int i=0;i<children.size();i++){
                ret += children[i]->gauge_com();
            }
            ret += children.size();

            break;
        }
        case RN_OR:
        {
            for(int i=0;i<children.size();i++){
                ret += children[i]->gauge_com();
            }
            ret += children.size();

            break;
        }
        case RN_NOT:
        {
            ret = children[0]->gauge_com() + 1;

            break;
        }
        case RN_ATOM:
        {
            ret++;

            break;
        }
        case RN_FACT:
        {
            break;
        }
    }

    return ret;
}

gdl_term* rnode::compose(){
    gdl_ast* ast = compose_ast();
    gdl_term* ret = new gdl_term(ast);

    delete ast;

    return ret;
}

gdl_ast* rnode::compose_ast(){
    gdl_ast* ret = NULL;
    switch(type()){
        case RN_AND:
        {
            ret = new gdl_ast();
            ret->children.push_back(new gdl_ast("and"));
            for(int i=0;i<children.size();i++){
                ret->children.push_back(children[i]->compose_ast());
            }
            break;
        }
        case RN_OR:
        {
            ret = new gdl_ast();
            ret->children.push_back(new gdl_ast("or"));
            for(int i=0;i<children.size();i++){
                ret->children.push_back(children[i]->compose_ast());
            }
            break;
        }
        case RN_NOT:
        {
            ret = new gdl_ast();
            ret->children.push_back(new gdl_ast("not"));

            assert(children.size() == 1);
            ret->children.push_back(children[0]->compose_ast());
            break;
        }
        case RN_FACT: case RN_ATOM:
        {
            ret = new gdl_ast(*ast);
            break;
        }
    }

    return ret;
} 

bool rnode::atom(const gdl_term* t){
    if(!t->grounded()) return false;

    string pred = t->get_pred();
    return pred == "state" || pred == "true" || pred == "distinct";
}

rnode* rnode::simplify(rnode* rn){
    if(rn == NULL) return rn;

    rnode* ret = rn;
    switch(rn->type()){
        case RN_FACT: case RN_ATOM:
        {
            break;
        }
        case RN_NOT:
        {
            ret->children[0] = simplify(ret->children[0]);
            break;
        }
        case RN_AND: case RN_OR:
        {
            if(ret->children.size() == 1){
                rnode* n_rn = new rnode(*rn->children[0]);
                delete rn;

                ret = simplify(n_rn);
            } else {
                for(int i=0;i<ret->children.size();i++){
                    ret->children[i] = simplify(ret->children[i]);
                }
            }
            break;
        }
    }

    return ret;
}

rnode* rnode::rnode_from_head(const gdl_term* head){ //timed
    //fprintf(stderr,"head=%s\n",head->convertS().c_str());

    assert(head->grounded());

    if(atom(head)){
        rnode* ret = new rnode(head);
        return ret;
    }

    string pred = head->get_pred();
    if(pred == "and" || pred == "or" || pred == "not"){
        rnode* ret = NULL;

        if(pred == "and"){
            ret = new rnode(RN_AND);
        } else if(pred == "or"){
            ret = new rnode(RN_OR);
        } else {
            ret = new rnode(RN_NOT);
        }

        assert(head->ast->children.size() > 1);

        for(int i=1;i<head->ast->children.size();i++){
            gdl_term* t = new gdl_term(head->ast->children[i]); //new

            rnode* sub = rnode_from_head(t);

            if(sub){
                ret->children.push_back(sub);
            } else {
                delete ret;
                ret = NULL;

                delete t;

                break;
            }

            delete t;
        }

        return ret;
    }

#ifndef NO_TIME_LIMIT
    if(regress::T->time_elapsed() >= regress::time_limit){ //out of time!
        return NULL;
    }
#endif

    rnode* ret = NULL;
    for(auto it = regress::game->desc.begin(); regress::game->desc.end() != it; it++){
        gdl_elem* elem = *it;
        if(elem->contains<gdl_term*>()){
            gdl_term* t = elem->get<gdl_term*>();

            symtab_type dummy;
            if(unify(head->ast,t->ast,dummy)){
                return new rnode(head,true);
            }

        } else if(elem->contains<gdl_rule*>()){
            gdl_rule* rule = elem->get<gdl_rule*>();

            symtab_type symtab;
            if(unify(head->ast,rule->head->ast,symtab)){
                gdl_rule* nrule = new gdl_rule(*rule);
                if(symtab.size() > 0){
                    substitute(nrule,symtab);
                }

                if(!ret){
                    ret = new rnode(RN_OR);
                }

                rnode* child = rnode::rnode_from_body(nrule);

                if(child){
                    ret->children.push_back(child);
                } else {
#ifndef NO_TIME_LIMIT
                    if(regress::T->time_elapsed() >= regress::time_limit){ //this is caused by time out
                        if(ret)
                            delete ret;
                        delete nrule;

                        return NULL;
                    }
#endif

                    //this (i.e. child == NULL) is caused by bad unification, do nothing
                }

                delete nrule;
            }
        }
    }

    return ret;
}

static terms* init_term(gdl_term* term);
static vector<gdl_rule*>* init_rule(gdl_rule* rule);

rnode* rnode::rnode_from_body(gdl_rule* rule){
#ifndef NO_TIME_LIMIT
    if(regress::T->time_elapsed() >= regress::time_limit){ //out of time!
        return NULL;
    }
#endif

    //fprintf(stderr,"rule: %s\n",rule->convertS().c_str());

    vector<gdl_rule*>* vr = init_rule(rule); //new

    if(vr == NULL){
        return NULL;
    }

    rnode* ret = new rnode(RN_OR); //new
    for(int ri=0;ri<vr->size();ri++){
        gdl_rule* r = vr->at(ri);

        //fprintf(stderr,"initiated rule: %s ->\n",rule->convertS().c_str());
        //fprintf(stderr,"\t%s\n",r->convertS().c_str());

        assert(r->grounded());

        rnode* sub = new rnode(RN_AND); //new
        for(int i=0;i<r->tails.size();i++){
            gdl_term* tail = r->tails[i];

            rnode* child = rnode_from_head(tail); //new
            if(child == NULL){
                delete sub;
                sub = NULL;
                break;
            }

            sub->children.push_back(child);
        }

        if(sub){
            ret->children.push_back(sub);
        }

#ifndef NO_TIME_LIMIT
        if(regress::T->time_elapsed() >= regress::time_limit){
            delete ret;
            ret = NULL;

            break;
        }
#endif
    }

    for(int i=0;i<vr->size();i++){
        delete vr->at(i);
    }
    delete vr;

    return ret;
}

static bool flat(gdl_ast* t){
    if(t->type() == AST_ATOM) return false;

    for(int i=0;i<t->children.size();i++){
        if(t->children[i]->type() != AST_ATOM) return false;
    }

    return true;
}

static int term_find_var_dom(gdl_ast* t,string& var,vector<string>& dom){
    if(t->type() == AST_ATOM){
        return 0;
    }

    if(flat(t)){
        if(t->children[0]->convertS() == "distinct") return 0;

        int i;
        for(i=1;i<t->children.size();i++){
            string s = t->children[i]->convertS();
            if(s[0] == '?'){
                break;
            }
        }
        if(i == t->children.size()){
            return 0;
        }

        string pred = t->children[0]->convertS();
        pred_index pi(pred,i);

        if(regress::domain->find(pi) == regress::domain->end()){
            return 0;
        }

        unordered_set<string>* udom = regress::domain->at(pi);
        var = t->children[i]->convertS();
        for(auto it = udom->begin(); udom->end() != it; it++){
            dom.push_back(*it);
        }

        return 1;
    }

    for(int i=0;i<t->children.size();i++){
        gdl_ast* child = t->children[i];
        if(term_find_var_dom(child,var,dom)){
            return 1;
        }
    }

    return 0;
}

static gdl_ast* term_substitute(gdl_ast* ast,const string& k,const string& v){
    if(ast->type() == AST_ATOM){
        string s = ast->convertS();
        if(s == k){
            delete ast;
            return new gdl_ast(v.c_str());
        }

        return ast;
    }

    for(int i=0;i<ast->children.size();i++){
        ast->children[i] = term_substitute(ast->children[i],k,v);
    }
    return ast;
}

static terms* init_term(gdl_term* term){
    unordered_set<pred_wrapper>* uset = new unordered_set<pred_wrapper>(); //new
    uset->insert(pred_wrapper(new gdl_term(*term))); //new

    terms* ret = NULL;
    bool change = true;

    while(change){
        change = false;

#ifndef NO_TIME_LIMIT
        if(regress::T->time_elapsed() >= regress::time_limit){
            goto destruct;
        }
#endif

        vector<gdl_term*> pred_vec;

        for(auto it = uset->begin(); uset->end() != it; it++){
            gdl_term* t = it->t;

            if(!t->grounded()){
                string var;
                vector<string> dom;

                term_find_var_dom(t->ast,var,dom);
                for(int j=0;j<dom.size();j++){
                    gdl_term* nt = new gdl_term(*t); //new
                    term_substitute(nt->ast,var,dom[j]);

                    if(uset->find(pred_wrapper(nt)) == uset->end()){
                        change = true;
                        pred_vec.push_back(nt);
                    } else {
                        delete nt;
                    }
                }

                delete t;
            } else {
                pred_vec.push_back(t);
            }
        }

        uset->clear();
        for(int i=0;i<pred_vec.size();i++){
            if(uset->find(pred_wrapper(pred_vec[i])) ==  uset->end()){
                uset->insert(pred_wrapper(pred_vec[i]));
            } else {
                delete pred_vec[i];
            }
        }
    }

    ret = new terms(); //new
    for(auto it = uset->begin(); uset->end() != it; it++){
        if(it->t->grounded()){
            ret->push_back(it->t);
        } else {
            delete it->t;
        }
    }
    delete uset;

    return ret;

destruct:
    for(auto it = uset->begin(); uset->end() != it; it++){
        delete it->t;
    }
    delete uset;

    return NULL;
}

static vector<gdl_rule*>* init_rule(gdl_rule* rule){ //timed
    assert(rule->head->grounded());

    vector<gdl_rule*>* ret = NULL;

    unordered_set<rule_wrapper>* uset = new unordered_set<rule_wrapper>(); //new
    uset->insert(new gdl_rule(*rule)); //new

    bool change = true;
    while(change){
        change = false;

#ifndef NO_TIME_LIMIT
        if(regress::T->time_elapsed() >= regress::time_limit){
            goto destruct;
        }
#endif

        vector<gdl_rule*> rule_vec;
        auto it = uset->begin();
        while(it != uset->end()){
            auto current = it++;
            gdl_rule* r = current->r;
            uset->erase(current);

            if(r->grounded()){
                rule_vec.push_back(r);
                continue;
            }

            terms* ts = NULL;
            gdl_term* t = NULL;

            for(int i=0;i<r->tails.size();i++){
                gdl_term* tail = r->tails[i];

                if(!tail->grounded()){
                    t = tail;
                    break;
                }
            }
            assert(t);

            ts = init_term(t); //new
            if(ts){
                for(int i=0;i<ts->size();i++){
                    gdl_term* nt = ts->at(i);
                    assert(nt->grounded());

                    symtab_type symtab;
                    assert(unify(nt->ast,t->ast,symtab));

                    gdl_rule* nr = new gdl_rule(*r);
                    substitute(nr,symtab);

                    if(uset->find(rule_wrapper(nr)) == uset->end()){
                        rule_vec.push_back(nr);
                        change = true;
                    } else {
                        delete nr;
                    }
                }

                controller::free_terms(ts);
            } else { //time out!
                for(int i=0;i<rule_vec.size();i++){
                    delete rule_vec[i];
                }
                
                delete r;
                goto destruct;
            }

            delete r;
        }

        uset->clear();
        for(int i=0;i<rule_vec.size();i++){
            if(uset->find(rule_wrapper(rule_vec[i])) == uset->end()){
                uset->insert(rule_wrapper(rule_vec[i]));
            } else {
                delete rule_vec[i];
            }
        }
    }

    ret = new vector<gdl_rule*>();
    for(auto it = uset->begin(); uset->end() != it; it++){
        if(!it->r) continue; //already freed

        if(it->r->grounded()){
            ret->push_back(it->r);
        } else {
            delete it->r;
        }
    }
    delete uset;
    
    return ret;

destruct:
    for(auto it = uset->begin(); uset->end() != it; it++){
        delete it->r;
    }
    delete uset;

    return NULL;
}

string rnode::convertS(){
    gdl_ast* ast = compose_ast();
    string ret = ast->convertS();
    delete ast;

    return ret;
}
