#include "gdl_flatten.h"
#include <queue>
#include <assert.h>
#include <iostream>
#include <stack>
#include <map>
#include <algorithm>
#include "slog.h"

#define FREE_SET_PTR(container,member) \
    for(auto it = container->begin(); it != container->end(); it++){ \
        delete it->member; \
    } \
    delete container;

#define FREE_SET(container,member) \
    for(auto it = container.begin(); it != container.end(); it++){ \
        delete it->member; \
    }

typedef pair<pred_index,string> pi_inst;
using std::queue;
using std::stack;

namespace std{
    template<>
    class hash<pi_inst>{
    public:
        size_t operator()(const pi_inst& p) const{
            stringstream ss;
            ss << p.first.pred << "[" << p.first.index << "]:"<<p.second;

            return hash<string>()(ss.str());
        }
    };

    template<>
    class equal_to<pi_inst>{
    public:
        bool operator()(const pi_inst& p1,const pi_inst& p2) const{
            return std::equal_to<pred_index>()(p1.first,p2.first) && p1.second == p2.second;
        }
    };
};

gdl_flatten::gdl_flatten(gdl* g){
    game = g;
}

gdl_flatten::~gdl_flatten(){
    FREE_SET(pi_inits,second);
    FREE_SET(domain_graph,second);

    FREE_SET(extended_facts,t);
    FREE_SET(facts,t);

    for(auto it = pf_inits.begin(); pf_inits.end() != it; it++){
        delete it->first.t;

        unordered_set<pred_wrapper>* uset = it->second;
        FREE_SET_PTR(uset,t);
    }
}

void gdl_flatten::add_grounded_rule(const gdl_rule* r,vector<gdl_elem*>& grounded_gdl){
    string head_pred = r->head->get_pred();

    if(head_pred == "base" || head_pred == "input" || head_pred == "init"){
        gdl_term* nt = new gdl_term(*(r->head));
        grounded_gdl.push_back(new gdl_elem(nt));
    } else {
        gdl_rule* nr = new gdl_rule(*r);
        trim_rule(nr);

        if(nr->tails.size() > 0){
            grounded_gdl.push_back(new gdl_elem(nr));
        } else {
            gdl_term* nt = new gdl_term(*nr->head);
            grounded_gdl.push_back(new gdl_elem(nt));
            delete nr;
        }
    }
}

void gdl_flatten::initiate_domains(){
    queue<pi_inst> Q;
    unordered_set<pi_inst> inQ;

    for(auto it = pi_inits.begin(); it != pi_inits.end(); it++){
        unordered_set<string>* dom = it->second;

        for(auto it2 = dom->begin(); it2 != dom->end(); it2++){
            pi_inst pii(it->first,*it2);

            assert(inQ.find(pii) == inQ.end());
            Q.push(pii);
            inQ.insert(pii);
        }
    }

    while(Q.size() > 0){
        pi_inst pii = Q.front();
        Q.pop();
        inQ.erase(pii);

        pred_index pi = pii.first;
        if(domain_graph.find(pi) == domain_graph.end()){
            continue;
        }

        unordered_set<pred_index>* adjs = domain_graph[pi];

        for(auto it = adjs->begin(); it != adjs->end();it++){
            pred_index adj = *it;
            assert(pi_inits.find(adj) != pi_inits.end());
            unordered_set<string>* inits = pi_inits[adj];
            if(inits->find(pii.second) == inits->end()){
                inits->insert(pii.second);
                pi_inst npii(adj,pii.second);

                if(inQ.find(npii) == inQ.end()){
                    inQ.insert(npii);
                    Q.push(npii);
                }
            }
        }
    }
}

static pred_form get_form_from_term(const gdl_term* t);

static bool special_pred(const string& p){
    return p == "and" || p == "or" || p == "not" || p == "distinct" || p == "true";
}

static bool special_pred1(const string& pred){
    //return pred == "base" || pred == "true" || pred == "not";
    return pred == "not";
}

static bool special_pred2(const string& pred){
    static set<string> S;
    if(S.size() == 0){
        S.insert("role");
        S.insert("base");
        S.insert("input");
        S.insert("init");
        S.insert("legal");
        S.insert("next");
        S.insert("does");
        S.insert("terminal");
        S.insert("goal");
        S.insert("true");
    }

    return S.find(pred) != S.end();
}

int gdl_flatten::add_to_pf_inits(gdl_term* t){
    pred_form pf = get_form_from_term(t);

    auto it = pf_inits.find(pf);
    if(it == pf_inits.end()){
        unordered_set<pred_wrapper>* uset = new unordered_set<pred_wrapper>();
        pf_inits.insert(pair<pred_form, unordered_set<pred_wrapper>* >(pf,uset));

        it = pf_inits.find(pf);
        assert(it != pf_inits.end());
    } else {
        delete pf.t;
    }

    int ret = 0;

    if(t->grounded()){
        gdl_term* nt = NULL;

        string pred = t->get_pred();
        if(special_pred1(pred)){
            nt = new gdl_term(t->ast->children[1]);
        } else {
            nt = new gdl_term(*t);
        }

        unordered_set<pred_wrapper>* uset = it->second;
        if(uset->find(pred_wrapper(nt)) == uset->end()){
            uset->insert(pred_wrapper(nt));

            ret = 1;
        } else {
            delete nt;
        }
    }

    return ret;
}

static pred_form get_form_from_term(const gdl_term* t){
    gdl_term* tf = NULL;

    string pred = t->get_pred();
    if(special_pred1(pred)){
        gdl_term* tmp = new gdl_term(t->ast->children[1]);
        tf = pred_form::transform(tmp);

        delete tmp;
    } else {
        tf = pred_form::transform(t);
    }

    return pred_form(tf);
}

static const gdl_ast* strip_of_not(const gdl_term* term){
    const gdl_ast* ret = term->ast;
    while(ret->get_pred() == "not" && ret->type() != AST_ATOM){
        ret = ret->children[1];
    }

    return ret;
}

static int non_fact_head(const string& pred){
    return pred == "input" || pred == "legal" || pred == "next"  
        || pred == "base"  || pred == "terminal" || pred == "goal";
}

static int non_fact_pred(const string& pred){
    return pred == "true" || pred == "does";
}

int gdl_flatten::only_facts(const gdl_rule* r){
    const gdl_ast* head_ast = strip_of_not(r->head);
    if(non_fact_head(head_ast->get_pred())){
        return 0;
    }

    for(int i=0;i<r->tails.size();i++){
        const gdl_ast* tail_ast = strip_of_not(r->tails[i]);
        gdl_term* term = new gdl_term(tail_ast); //new

        if(non_fact_pred(term->get_pred())){
            delete term;

            return 0;
        }

        if(term->get_pred() == "distinct"){
            delete term;
            continue;
        }

        if(term->get_pred() == r->head->get_pred()){
            delete term;
            continue;
        }

        pred_form pf = get_form_from_term(term); //new
        if(facts.find(pf) == facts.end() && extended_facts.find(pf) == extended_facts.end()){
            delete pf.t;
            delete term;

            return 0;
        }

        delete term;
    }

    return 1;
}

void gdl_flatten::find_facts_and_extended_facts(){
    //find facts
    for(int di=0;di < game->desc.size(); di++){
        gdl_elem* elem = game->desc[di];
        if(elem->contains<gdl_term*>()){
            gdl_term* term = elem->get<gdl_term*>();

            string pred = term->get_pred();
            if(pred != "base" && pred != "init"){
                pred_form pf = get_form_from_term(term); //new

                if(facts.find(pf) == facts.end()){
                    facts.insert(pf);
                } else {
                    delete pf.t;
                }
            }
        }
    }
    for(int di=0;di < game->desc.size(); di++){
        gdl_elem* elem = game->desc[di];
        if(elem->contains<gdl_rule*>()){
            gdl_rule* rule = elem->get<gdl_rule*>();
            
            pred_form pfh = get_form_from_term(rule->head); //new
            auto it = facts.find(pfh);
            if(it != facts.end()){
                gdl_term* t = it->t;
                facts.erase(it);

                delete t;
            }

            delete pfh.t;
        }
    }

    //find extended_facts
    int change = 1;

    while(change){
        change = 0;

        for(int di=0;di < game->desc.size(); di++){
            gdl_elem* elem = game->desc[di];

            if(elem->contains<gdl_rule*>()){
                gdl_rule* rule = elem->get<gdl_rule*>();

                if(only_facts(rule)){
                    pred_form pf = get_form_from_term(rule->head);

                    if(extended_facts.find(pf) == extended_facts.end()){
                        extended_facts.insert(pf);
                        change = 1;
                    } else {
                        delete pf.t;
                    }
                }
            }
        }
    }

    for(int di=0;di < game->desc.size(); di++){
        gdl_elem* elem = game->desc[di];

        if(elem->contains<gdl_rule*>()){
            gdl_rule* rule = elem->get<gdl_rule*>();
            //test
            //fprintf(stderr,"inspecting rule:%s\n",rule->convertS().c_str());

            if(!only_facts(rule)){
                //test
                //fprintf(stderr,"not only facts:%s\n",rule->convertS().c_str());

                pred_form pf = get_form_from_term(rule->head);

                auto result = extended_facts.find(pf);
                if(result != extended_facts.end()){
                    delete result->t;
                    extended_facts.erase(result);
                }

                delete pf.t;
            }
        }
    }
}

void gdl_flatten::build_facts_pf_inits(){
    for(int di=0; di < game->desc.size();di++){
        gdl_elem* elem = game->desc[di];

        if(elem->contains<gdl_term*>()){
            gdl_term* term = elem->get<gdl_term*>();

            if(add_to_pf_inits(term)){
                //test
                //fprintf(stderr,"new term:%s\n",term->convertS().c_str());
            }
        }
    }
}

int gdl_flatten::build_extended_facts_pf_inits(){
    int ret = 0;

    for(int di=0; di < game->desc.size();di++){
        gdl_elem* elem = game->desc[di];

        if(elem->contains<gdl_term*>()){
            gdl_term* term = elem->get<gdl_term*>();

            if(add_to_pf_inits(term)){
                //test
                //fprintf(stderr,"new term:%s\n",term->convertS().c_str());
                ret = 1;
            }
        } else if(elem->contains<gdl_rule*>()){
            gdl_rule* rule = elem->get<gdl_rule*>();
            pred_form pfh = get_form_from_term(rule->head);

            if(extended_facts.find(pfh) != extended_facts.end()){ //only expand extended_facts
                if(propagate(rule,false)){
                    ret = 1;
                }
            }

            delete pfh.t;
        }
    }
    
    return ret;
}

int gdl_flatten::build_pf_inits(){
    int ret = 0;

    for(int di=0; di < game->desc.size();di++){
        gdl_elem* elem = game->desc[di];

        if(elem->contains<gdl_rule*>()){
            gdl_rule* rule = elem->get<gdl_rule*>();
            pred_form pfh = get_form_from_term(rule->head);

            if(extended_facts.find(pfh) == extended_facts.end()){ //expand non-extended_facts
                //test
                //fprintf(stderr,"applying rule:%s\n",rule->convertS().c_str());

                if(propagate(rule,true)){
                    ret = 1;
                }
            }

            delete pfh.t;
        }
    }

    if(legal_transfer()){
        ret = 1;
    }

    return ret;
}

int gdl_flatten::transfer(const string& from, const string& to){
    int ret = 0;

    for(auto it = pf_inits.begin(); pf_inits.end() != it; it++){
        pred_form key = it->first;
        unordered_set<pred_wrapper>* inits = it->second;

        if(key.t->get_pred() == from){
            gdl_ast* nast = new gdl_ast(*key.t->ast); //new
            nast->children[0]->set_sym(to);
            gdl_term* lterm = new gdl_term(nast); //new
            pred_form lpf = get_form_from_term(lterm); //new

            //test
            //fprintf(stderr,"%s -> %s\n",key.t->convertS().c_str(), lterm->convertS().c_str());

            unordered_set<pred_wrapper>* linits = get_inits(lpf);
            for(auto it2 = inits->begin(); inits->end() != it2; it2++){
                pred_wrapper pw = *it2;

                gdl_term* nt = new gdl_term(*pw.t); //new
                nt->ast->children[0]->set_sym(to);

                if(linits->find(pred_wrapper(nt)) == linits->end()){
                    linits->insert(pred_wrapper(nt));

                    ret = 1;
                } else {
                    delete nt;
                }
            }

            delete nast;
            delete lterm;
            delete lpf.t;
        }
    }

    return ret;
}

int gdl_flatten::legal_transfer(){ //@TODO: implement
    int ret = 0;
    int t;

    //input -> legal
    t = transfer("input","legal");
    ret = ret || t;

    //legal -> does
    t = transfer("legal","does");
    ret = ret || t;

    //base -> true
    t = transfer("base","true");
    ret = ret || t;

    return ret;
}

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

unordered_set<pred_wrapper>* gdl_flatten::get_inits(const pred_form& pf){
    unordered_set<pred_wrapper>* ret = NULL;

    auto it = pf_inits.find(pf);
    if(it == pf_inits.end()){
        ret = new unordered_set<pred_wrapper>();
        pred_form npf = pf;
        npf.t = new gdl_term(*pf.t); //copyinf pf

        pf_inits.insert(pair<pred_form, unordered_set<pred_wrapper>* >(npf,ret));
    } else {
        ret = it->second;
    }

    return ret;
}

static const gdl_ast* get_unify_ast(const gdl_term* t){
    gdl_ast* ret = t->ast;

    while(special_pred1(ret->get_pred()) && ret->type() != AST_ATOM){
        ret = ret->children[1];
    }

    return ret;
}

int gdl_flatten::strictly_illegal(const gdl_rule* r){
    //check for distinct and facts
    int ret = 0;

    for(int ti = 0; ti < r->tails.size(); ti++){
        const gdl_term* term = r->tails[ti];
        if(!term->grounded()) continue;

        if(term->get_pred() == "not"){
            const gdl_ast* child = strip_of_not(term);
            gdl_term* tmp = new gdl_term(child); //new
            pred_form pf = get_form_from_term(tmp); //new

            if(facts.find(pf) != facts.end()){
                unordered_set<pred_wrapper>* uset = get_inits(pf);
                if(uset->find(pred_wrapper(tmp)) != uset->end()){ //we are dealing with not here
                    ret = 1;
                }
            }

            delete tmp;
            delete pf.t;
        } else if(term->get_pred() == "distinct"){
            string c1 = term->ast->children[1]->sym();
            string c2 = term->ast->children[2]->sym();
            ret = c1 == c2;
        } else {
            pred_form pf = get_form_from_term(term); //new

            if(facts.find(pf) != facts.end()){
                gdl_term* nt = new gdl_term(*term);

                unordered_set<pred_wrapper>* uset = get_inits(pf);
                if(uset->find(pred_wrapper(nt)) == uset->end()){
                    ret = 1;
                }

                delete nt;
            }

            delete pf.t;
        }

        if(ret) break;
    }

    return ret;
}

int gdl_flatten::loosely_illegal(const gdl_rule* r){
    if(strictly_illegal(r)){ return 1; }

    int ret = 0;

    for(int ti = 0; ti < r->tails.size(); ti++){
        const gdl_term* term = r->tails[ti];
        if(!term->grounded()) continue;

        if(term->get_pred() == "not"){
            const gdl_ast* child = strip_of_not(term);
            gdl_term* tmp = new gdl_term(child); //new
            pred_form pf = get_form_from_term(tmp); //new

            if(extended_facts.find(pf) != extended_facts.end()){
                unordered_set<pred_wrapper>* uset = get_inits(pf);
                if(uset->find(pred_wrapper(tmp)) != uset->end()){ //we are dealing with not here
                    ret = 1;
                }
            }

            delete tmp;
            delete pf.t;
        } else if(term->get_pred() == "distinct"){
            continue;
        } else {
            pred_form pf = get_form_from_term(term); //new

            if(extended_facts.find(pf) != extended_facts.end()){
                gdl_term* nt = new gdl_term(*term);

                unordered_set<pred_wrapper>* uset = get_inits(pf);
                if(uset->find(pred_wrapper(nt)) == uset->end()){
                    ret = 1;
                }

                delete nt;
            }

            delete pf.t;
        }

        if(ret) break;
    }

    return ret;
}

void gdl_flatten::clear_seen_rule(){
    for(auto it = rules_seen.begin(); rules_seen.end() != it; it++){
        delete it->r;
    }
    rules_seen.clear();
}

void gdl_flatten::add_seen_rule(const gdl_rule* r){
    gdl_rule* nr = new gdl_rule(*r);
    if(rules_seen.find(rule_wrapper(nr)) == rules_seen.end()){
        rules_seen.insert(rule_wrapper(nr));
    } else {
        delete nr;
    }
}

int gdl_flatten::have_seen_rule(const gdl_rule* r){
    gdl_rule* nr = new gdl_rule(*r);
    int ret = rules_seen.find(rule_wrapper(nr)) != rules_seen.end();

    delete nr;
    return ret;
}

static void collect_vars(gdl_ast* t, set<string>& vars){
    if(t->type() == AST_ATOM){
        if(t->_sym[0] == '?'){
            vars.insert(t->_sym);
        }

        return;
    }

    for(int i=0;i<t->children.size();i++){
        collect_vars(t->children[i],vars);
    }
}

static bool share_variable(gdl_term* t1,gdl_term* t2){
    set<string> vars1,vars2;

    collect_vars(t1->ast,vars1);
    collect_vars(t2->ast,vars2);

    set<string> intersect;
    std::set_intersection(vars1.begin(), vars1.end(),
            vars2.begin(), vars2.end(),
            std::inserter(intersect,intersect.begin()));

    return intersect.size() > 0;
}

int gdl_flatten::propagate(const gdl_rule* r,bool loosely){
    if(loosely){
        if(loosely_illegal(r)){
            return 0;
        }
    } else {
        if(strictly_illegal(r)){
            return 0;
        }
    }

    if(have_seen_rule(r)){
        return 0;
    }
    add_seen_rule(r);

    //test
    //fprintf(stderr,"propagating rule:%s\n",r->convertS().c_str());

    int ret = 0;
    if(r->head->grounded()){
        ret = add_to_pf_inits(r->head);
        if(ret){
            //test
            //fprintf(stderr,"new term:%s\n",r->head->convertS().c_str());
            //fprintf(stderr,"\trelated rule:%s\n",r->convertS().c_str());
        }
    } else {
        vector<const gdl_ast*> expanding_tails;

        for(int ti=0;ti<r->tails.size();ti++){
            gdl_term* tail = r->tails[ti];
            if(tail->get_pred() == "distinct") continue;
            if(tail->grounded()) continue;

            if(!share_variable(r->head,tail)) continue;

            const gdl_ast* ast2 = get_unify_ast(tail);

            pred_form pf = get_form_from_term(tail); //new
            unordered_set<pred_wrapper>* inits = get_inits(pf);

            for(auto it = inits->begin(); inits->end() != it; it++){
                gdl_term* init = it->t;
                const gdl_ast* ast1 = get_unify_ast(init);

                symtab_type symtab;
                if(unify(ast1,ast2,symtab) && symtab.size() > 0){
                    gdl_rule* nr = new gdl_rule(*r); //new
                    substitute(nr,symtab);

                    int tmp = propagate(nr,loosely);
                    ret = ret || tmp;

                    delete nr;
                }
            }

            delete pf.t;

            break;
        }
    }

    return ret;
}

int gdl_flatten::flatten(vector<gdl_elem*>& grounded_gdl){
    int ret = 1;

    timer* T = new timer();

    build_domain_graph();

#ifdef GDL_FLATTEN_DEBUG
    slog("gdl_flatten","domain graph:\n");
    print_domain_graph();
#endif

    initiate_domains();

#ifdef GDL_FLATTEN_DEBUG
    slog("gdl_flatten","domains found:\n");
    print_pi_inits();
#endif

    find_facts_and_extended_facts();

    build_facts_pf_inits();

    while(build_extended_facts_pf_inits()){
        clear_seen_rule();
    }
    clear_seen_rule();

    while(build_pf_inits()){
        clear_seen_rule();
    }
    clear_seen_rule();

#ifdef GDL_FLATTEN_DEBUG
    slog("gdl_flatten","cartesian domains found:\n");
    print_pf_inits();
#endif

    fprintf(stderr,"before expanding rules:%lf\n",T->time_elapsed());
    delete T;

    for(auto it = game->desc.begin(); it != game->desc.end(); it++){
        gdl_elem* elem = *it;
        if(elem->contains<gdl_term*>()){
            const gdl_term* t = elem->get<gdl_term*>();
            if(special_pred2(t->get_pred())){ //retain special facts
                gdl_term* nt = new gdl_term(*t);
                grounded_gdl.push_back(new gdl_elem(nt));

                continue;
            }

            assert(t->grounded());

            pred_form pf = get_form_from_term(t);
            if(extended_facts.find(pf) == extended_facts.end() && facts.find(pf) == facts.end()){
                gdl_term* nt = new gdl_term(*t);
                grounded_gdl.push_back(new gdl_elem(nt));
            }

            delete pf.t;
        } else if(elem->contains<gdl_rule*>()){
            const gdl_rule* r = elem->get<gdl_rule*>();

            unordered_set<rule_wrapper>* expanded_rule = expand_rule(r);

            for(auto it2 = expanded_rule->begin(); expanded_rule->end() != it2; it2++){
                gdl_rule* er = it2->r;

                if(special_pred2(er->head->get_pred())){
                    add_grounded_rule(er,grounded_gdl);
                    continue;
                }

                assert(er->grounded());

                pred_form pf = get_form_from_term(er->head);
                if(extended_facts.find(pf) == extended_facts.end() && facts.find(pf) == facts.end()){
                    add_grounded_rule(er,grounded_gdl);
                }

                delete pf.t;
            }
            FREE_SET_PTR(expanded_rule,r);
        }
    }

    return ret;
}

void gdl_flatten::build_domain_graph(){
    using namespace std;

    for(auto it = game->desc.begin(); it != game->desc.end(); it++){
        gdl_elem* elem = *it;
        if(elem->contains<gdl_term*>()){
            gdl_term* t = elem->get<gdl_term*>();
            terms* ts = extract_terms(t);

            assert(t->grounded());

            for(auto it2 = ts->begin(); it2 != ts->end(); it2++){
                gdl_term* st = *it2;

                add_pi_from_term(st);
            }

            controller::free_terms(ts);
        } else if(elem->contains<gdl_rule*>()){
            gdl_rule* r = elem->get<gdl_rule*>();
            terms* ts = extract_terms(r);

            for(auto it2 = ts->begin(); it2 != ts->end(); it2++){
                gdl_term* st = *it2;

                add_pi_from_term(st);
            }

            for(unsigned int i=0;i<ts->size();i++){
                for(unsigned int j=0;j<ts->size();j++){
                    if(i == j) continue;

                    add_domain_graph_edge(ts->at(i),ts->at(j));
                }
            }

            controller::free_terms(ts);
        }
    }
}

void gdl_flatten::add_pi_from_term(const gdl_term* t){
    if(special_pred(t->get_pred())){
        return;
    }

    const gdl_ast* ast = t->ast;
    for(unsigned int i=1;i<ast->children.size(); i++){
        const gdl_ast* child = ast->children[i];
        if(child->type() == AST_ATOM){
            pred_index pi(t->get_pred(),i);
            unordered_set<string>* inits = NULL;

            auto result = pi_inits.find(pi);
            if(result == pi_inits.end()){
                inits = new unordered_set<string>();
                pi_inits.insert(pair<pred_index, unordered_set<string>* >(pi,inits));
            } else {
                inits = result->second;
            }

            if(child->sym()[0] != '?'){
                inits->insert(child->sym());
            }
        }
    }
}

void gdl_flatten::add_domain_graph_edge(const gdl_term* t1, const gdl_term* t2){
    const gdl_ast* ast1 = t1->ast;
    const gdl_ast* ast2 = t2->ast;
    string pred1 = t1->get_pred();
    string pred2 = t2->get_pred();

    if(special_pred(pred1) || special_pred(pred2)){
        return;
    }

    for(unsigned int i=1;i<ast1->children.size();i++){
        const gdl_ast* child1 = ast1->children[i];
        if(child1->type() == AST_LIST) continue;
        if(child1->sym()[0] != '?') continue;

        for(unsigned int j=0;j<ast2->children.size();j++){
            const gdl_ast* child2 = ast2->children[j];
            if(child2->type() == AST_LIST) continue;
            if(child2->sym()[0] != '?') continue;

            if(!strcmp(child1->sym(),child2->sym())){
                pred_index pi1(pred1,i);
                pred_index pi2(pred2,j);

                add_pi_edge(pi1,pi2);
                add_pi_edge(pi2,pi1);
            }
        }
    }
}

void gdl_flatten::add_pi_edge(const pred_index& pi1,const pred_index& pi2){
    unordered_set<pred_index>* adjs = NULL;

    auto result = domain_graph.find(pi1);
    if(result == domain_graph.end()){
        adjs = new unordered_set<pred_index>();
        domain_graph.insert(pair<pred_index,unordered_set<pred_index>* >(pi1,adjs));
    } else {
        adjs = result->second;
    }

    adjs->insert(pi2);
}

static void collect_preds(const gdl_ast* t,terms* v){
    if(t->type() == AST_ATOM) return;

    gdl_term* tt = new gdl_term(t);
    v->push_back(tt);

    auto it = tt->ast->children.begin();
    while(it != tt->ast->children.end()){
        collect_preds(*it,v);

        it++;
    }
}

static int get_nvars(const gdl_ast* ast){
    if(ast->type() == AST_ATOM){
        if(ast->sym()[0] == '?') return 1;
        return 0;
    }

    int ret = 0;
    for(int i=0;i<ast->children.size();i++){
        ret += get_nvars(ast->children[i]);
    }

    return ret;
}

void gdl_flatten::initiate_variables_in_rule(const gdl_rule* rule,unordered_set<rule_wrapper>* uset){
    stack<gdl_rule*> S;

    S.push(new gdl_rule(*rule));

    while(S.size() > 0){
        gdl_rule* cur = S.top();
        S.pop();

        if(loosely_illegal(cur)){
            delete cur;
            continue;
        }

        if(cur->grounded()){
            if(uset->find(rule_wrapper(cur)) == uset->end()){
                uset->insert(rule_wrapper(cur));
            } else {
                delete cur;
            }

            continue;
        }

        const gdl_term* chosen_tail = NULL;
        for(int i=0;i<cur->tails.size();i++){
            const gdl_term* tail = cur->tails[i];
            if(tail->get_pred() == "distinct"){
                continue;
            }
            if(tail->grounded()){
                continue;
            }
            if(chosen_tail == NULL){
                chosen_tail = tail;
                break;
            }
        }

        const gdl_ast* unify_ast_tail = get_unify_ast(chosen_tail);
        gdl_term* nt = new gdl_term(unify_ast_tail); //new
        pred_form pf = get_form_from_term(nt); //new
        unordered_set<pred_wrapper>* inits = get_inits(pf);

        for(auto it = inits->begin(); inits->end() != it; it++){
            gdl_term* init = it->t;
            const gdl_ast* unify_ast_init = get_unify_ast(init);

            symtab_type symtab;
            if(unify(unify_ast_init,unify_ast_tail,symtab) && symtab.size() > 0){
                gdl_rule* nr = new gdl_rule(*cur); //new
                substitute(nr,symtab);

                S.push(nr);
            }
        }

        delete nt;
        delete pf.t;
        delete cur;
    }
}

unordered_set<rule_wrapper>* gdl_flatten::expand_rule(const gdl_rule* r){
#ifdef GDL_FLATTEN_DEBUG
    fprintf(stderr,"expanding rule:%s\n",r->convertS().c_str());
#endif

    unordered_set<rule_wrapper>* uset = new unordered_set<rule_wrapper>();
    initiate_variables_in_rule(r,uset);

#ifdef GDL_FLATTEN_DEBUG
    fprintf(stderr,"\n");
#endif

    return uset;
}

void gdl_flatten::trim_rule(gdl_rule* r){
    vector<gdl_term*> trimed_tails;
    for(int i=0;i<r->tails.size();i++){
        gdl_term* tail = r->tails[i];

        if(tail->get_pred() == "distinct"){
            delete tail;
            continue;
        }

        pred_form pf = get_form_from_term(tail);
        if(extended_facts.find(pf) == extended_facts.end() && facts.find(pf) == facts.end()){
            trimed_tails.push_back(tail);
        } else {
            //test
            //fprintf(stderr,"trimming %s\n",tail->convertS().c_str());

            delete tail;
        }
        delete pf.t;
    }

    r->tails = trimed_tails;
}

static void substitute_ast(gdl_ast* t,const symtab_type& symtab){
    if(t->type() == AST_ATOM){
        if(t->sym()[0] == '?'){
            string key = t->sym();
            if(symtab.find(key) != symtab.end()){
                auto it = symtab.find(key);
                t->set_sym(it->second);
            }
        }

        return;
    }

    for(int i=0;i<t->children.size();i++){
        substitute_ast(t->children[i],symtab);
    }
}

void gdl_flatten::substitute(gdl_rule* r,const symtab_type& symtab){
    substitute_ast(r->head->ast,symtab);
    for(int i=0;i<r->tails.size();i++){
        substitute_ast(r->tails[i]->ast,symtab);
    }
}

terms* gdl_flatten::extract_terms(const gdl_term* t){
    terms* ret = new terms();
    collect_preds(t->ast,ret);

    return ret;
}

terms* gdl_flatten::extract_terms(const gdl_rule* r){
    terms* ret = new terms();
    collect_preds(r->head->ast,ret);

    for(auto tail_iter = r->tails.begin();tail_iter != r->tails.end(); tail_iter++){
        collect_preds((*tail_iter)->ast,ret);
    }

    return ret;
}

void gdl_flatten::printS(const vector<gdl_elem*>& grounded_gdl,FILE* fp){
    using namespace std;

    for(auto it = grounded_gdl.begin(); grounded_gdl.end() != it; it++){
        gdl_elem* elem = *it;
        if(elem->contains<gdl_term*>()){
            gdl_term* t = elem->get<gdl_term*>();
            fprintf(fp,"%s\n",t->convertS().c_str());
        } else if(elem->contains<gdl_rule*>()){
            gdl_rule* r = elem->get<gdl_rule*>();
            fprintf(fp,"%s\n",r->convertS().c_str());
        }
    }
}

void gdl_flatten::print_pi_inits(){
    using namespace std;
    for(auto kv_iter = pi_inits.begin(); pi_inits.end() != kv_iter; kv_iter++){
        pred_index key = kv_iter->first;
        unordered_set<string>* inits = kv_iter->second;

        cerr<<key.pred<<"["<<key.index<<"]:"<<endl;
        for(auto it = inits->begin(); inits->end() != it; it++){
            cerr<<"\t"<<*it<<endl;
        }
    }
}

void gdl_flatten::print_domain_graph(){
    using namespace std;

    for(auto kv_iter = domain_graph.begin(); domain_graph.end() != kv_iter; kv_iter++){
        pred_index key = kv_iter->first;
        unordered_set<pred_index>* adjs = kv_iter->second;

        cerr<<key.pred<<"["<<key.index<<"]"<<endl;
        for(auto it = adjs->begin(); adjs->end() != it; it++){
            pred_index adj = *it;
            cerr<<"\t"<<adj.pred<<"["<<adj.index<<"]"<<endl;
        }
    }
}


void gdl_flatten::print_pf_inits(){
    for(auto it = pf_inits.begin(); it != pf_inits.end(); it++){
        pred_form pf = it->first;
        unordered_set<pred_wrapper>* uset = it->second;

        fprintf(stderr,"form:%s\n", pf.t->convertS().c_str());
        for(auto u_it = uset->begin(); u_it != uset->end(); u_it++){
            fprintf(stderr,"\t%s\n",u_it->t->convertS().c_str());
        }
        fprintf(stderr,"\n");
    }
    fprintf(stderr,"\n");

    fprintf(stderr,"facts:\n");
    for(auto it = facts.begin(); it != facts.end(); it++){
        fprintf(stderr,"\t%s\n",it->t->convertS().c_str());
    }
}

namespace std{
    size_t hash<pred_index>::operator()(const pred_index& p) const{
        stringstream ss;
        ss << p.pred << "[" << p.index << "]";

        return hash<string>()(ss.str());
    }

    bool equal_to<pred_index>::operator()(const pred_index& p1,const pred_index& p2) const{
        return p1.index == p2.index && p1.pred == p2.pred;
    }
};
