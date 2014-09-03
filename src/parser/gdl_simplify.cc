#include "gdl_simplify.h"
#include "slog.h"
#include <unordered_set>
#include <sstream>
#include <map>

using std::stringstream;
using std::unordered_set;
using std::map;

void gdl_simplify::simplify(vector<gdl_elem*>& g){
    del_and_or(g);
    var_constrain(g);
}

static void clear_g(vector<gdl_elem*>& g){
    auto it = g.begin();

    while(it != g.end()){
        gdl_elem* elem = *it;

        if(elem->contains<gdl_term*>()){
            gdl_term* t = elem->get<gdl_term*>();
            delete t;
        } else if(elem->contains<gdl_rule*>()){
            gdl_rule* r = elem->get<gdl_rule*>();
            delete r;
        }
        delete elem;

        it++;
    }

    g.clear();
}

//@NOTE: we are assuming that we don't get rules of the form (not (or ...)) or any other complicated composite boolean terms
static int contains_or(const gdl_rule* r){
    for(int i=0;i<r->tails.size(); i++){
        if(r->tails[i]->get_pred() == "or") return 1;
    }

    return 0;
}

//@NOTE: we are assuming that we don't get rules of the form (not (and ...)) or any other complicated composite boolean terms
static int contains_and(const gdl_rule* r){
    for(int i=0;i<r->tails.size(); i++){
        if(r->tails[i]->get_pred() == "and") return 1;
    }

    return 0;
}

static void strip_of_and(gdl_rule* r){
    vector<gdl_term*> ntails;

    for(int i=0;i<r->tails.size();i++){
        gdl_term* tail = r->tails[i];

        if(tail->get_pred() == "and"){
            for(int j=1;j<tail->ast->children.size();j++){
                gdl_term* nt = new gdl_term(tail->ast->children[j]);

                ntails.push_back(nt);
            }
        } else {
            ntails.push_back(new gdl_term(*tail));
        }

        delete tail;
    }

    r->tails = ntails;
}

static void split_first_or(const gdl_rule* r,vector<gdl_rule*>& ret){
    for(int i=0;i<r->tails.size();i++){
        if(r->tails[i]->get_pred() == "or"){
            gdl_term* tail = r->tails[i];

            for(int j=1;j<tail->ast->children.size();j++){
                gdl_rule* nr = new gdl_rule(*r);

                delete nr->tails[i];
                nr->tails[i] = new gdl_term(tail->ast->children[j]);

                ret.push_back(nr);
            }

            break;
        }
    }
}

static unordered_set<rule_wrapper>* split_or(const gdl_rule* r){
    unordered_set<rule_wrapper>* ret = new unordered_set<rule_wrapper>();

    gdl_rule* nr = new gdl_rule(*r);
    ret->insert(rule_wrapper(nr));

    int or_count = 1;
    while(or_count){
        //test
        //fprintf(stdout,"begin spliting:\n");
        //for(auto it = ret->begin(); ret->end() != it; it++){
            //fprintf(stdout,"\t%s\n",it->r->convertS().c_str());
        //}
        //fprintf(stdout,"^^^^^^^^^^^^^^^^^^^^^^^^^^\n\n");

        vector<rule_wrapper> rws(ret->begin(), ret->end());
        ret->clear();

        for(int ri=0;ri<rws.size();ri++){
            gdl_rule* r = rws[ri].r;
            if(contains_or(r)){
                vector<gdl_rule*> vnr;
                split_first_or(r,vnr);

                for(int i=0;i<vnr.size();i++){
                    ret->insert(rule_wrapper(vnr[i]));
                }

                delete r;
            } else {
                ret->insert(rws[ri]);
            }
        }

        or_count = 0;
        for(auto it = ret->begin(); it != ret->end(); it++){
            gdl_rule* r = it->r;
            if(contains_or(r)){
                or_count++;
            }
        }
    }

    return ret;
}

void gdl_simplify::del_and_or(vector<gdl_elem*>& g){
    vector<gdl_elem*> ng;

    int change = 1;
    while(change){
        change = 0;

        for(int gi=0;gi<g.size();gi++){
            gdl_elem* elem = g[gi];

            if(elem->contains<gdl_term*>()){
                gdl_term* nt = new gdl_term(*(elem->get<gdl_term*>()));
                gdl_elem* nelem = new gdl_elem(nt);

                ng.push_back(nelem);
            } else if(elem->contains<gdl_rule*>()){
                gdl_rule* nr = new gdl_rule(*(elem->get<gdl_rule*>())); //new

                if(contains_and(nr)){
                    strip_of_and(nr);
                }

                if(!contains_or(nr)){
                    gdl_elem* nelem = new gdl_elem(nr);

                    ng.push_back(nelem);
                } else {
                    unordered_set<rule_wrapper>* split_rule = split_or(nr); //new

                    change = 1;

                    for(auto it = split_rule->begin(); split_rule->end() != it; it++){
                        gdl_elem* nelem = new gdl_elem(it->r);
                        ng.push_back(nelem);
                    }

                    delete split_rule;
                    delete nr;
                }
            }
        }

        clear_g(g);
        g = ng;
        ng.clear();
    }
}

static pred_form get_form_from_term(const gdl_term* t){
    gdl_term* nt = pred_form::transform(t);

    return pred_form(nt);
}

static unordered_set<pred_form>* get_forms(const string& key,map<string,unordered_set<pred_form>* >& dom){
    unordered_set<pred_form>* ret = NULL;

    auto it = dom.find(key);
    if(it == dom.end()){
        ret = new unordered_set<pred_form>();
        dom.insert(std::pair<string,unordered_set<pred_form>* >(key,ret));

        assert(dom.find(key) != dom.end());
    } else {
        ret = it->second;
    }

    return ret;
}

static int ast_more_general(const gdl_ast* t1, const gdl_ast* t2){
    if(t1->type() == AST_ATOM){
        if(!strcmp(t1->sym(),"##")){
            return 1;
        } else {
            if(t2->type() != AST_ATOM) return 0; //cannot unify, hence not more general
            return !strcmp(t1->sym(),t2->sym());
        }
    }

    if(t2->type() == AST_ATOM){
        return 0;
    }
    if(t1->children.size() != t2->children.size()){
        return 0;
    }

    for(int i=0;i<t1->children.size();i++){
        if(!ast_more_general(t1->children[i],t2->children[i])){
            return 0;
        }
    }

    return 1;
}

static int more_general(const pred_form& pf1, const pred_form& pf2){
    return ast_more_general(pf1.t->ast,pf2.t->ast) && !std::equal_to<pred_form>()(pf1,pf2);
}

static int var_count = 0;
static string get_var_name(){
    stringstream ss;
    ss << "?gdl_simplify_var_" << var_count++;

    return ss.str();
}

static gdl_ast* replace_hash_with_var(const gdl_ast* t){
    if(t->type() == AST_ATOM){
        if(!strcmp(t->sym(),"##")){
            string var = get_var_name();
            return new gdl_ast(var.c_str());
        }

        return new gdl_ast(*t);
    }

    gdl_ast* ret = new gdl_ast();
    for(int i=0;i<t->children.size();i++){
        ret->children.push_back(replace_hash_with_var(t->children[i]));
    }

    return ret;
}

static int unify_with_form(const gdl_ast* orig,const gdl_ast* new_form_ast, map<string, gdl_ast*>& symtab){
    if(orig->type() == AST_ATOM){
        if(orig->sym()[0] == '?'){
            gdl_ast* nt = replace_hash_with_var(new_form_ast); //new
            symtab.insert(std::pair<string,gdl_ast*>(orig->sym(),nt));

            return 1;
        } else {
            if(new_form_ast->type() == AST_ATOM){
                if(!strcmp(new_form_ast->sym(),"##") ||
                   !strcmp(new_form_ast->sym(),orig->sym())){
                    return 1;
                }
            }

            return 0;
        }
    }

    if(new_form_ast->type() == AST_ATOM) return 0;
    if(new_form_ast->children.size() != orig->children.size()) return 0;

    for(int i=0;i<orig->children.size();i++){
        if(!unify_with_form(orig->children[i],new_form_ast->children[i],symtab)){
            return 0;
        }
    }

    return 1;
}

static gdl_ast* substitute_ast(gdl_ast* t,const map<string, gdl_ast*>& symtab){
    if(t->type() == AST_ATOM){
        if(t->sym()[0] == '?'){
            auto result = symtab.find(t->sym());

            if(result != symtab.end()){
                gdl_ast* sub = result->second;
                delete t;

                return new gdl_ast(*sub);
            }
        }

        return t;
    }

    for(int i=0;i<t->children.size();i++){
        t->children[i] = substitute_ast(t->children[i],symtab);
    }

    return t;
}

static void substitute(gdl_rule* r,const map<string, gdl_ast*>& symtab){
    r->head->ast = substitute_ast(r->head->ast,symtab);
    for(int i=0;i<r->tails.size();i++){
        r->tails[i]->ast = substitute_ast(r->tails[i]->ast,symtab);
    }
}

static int expand_rule(const pred_form& pf_to, const gdl_rule* r, int i, vector<gdl_rule*>& new_rules){
    gdl_term* tail = r->tails[i];
    map<string, gdl_ast*> symtab; //variable name -> ast structure

    if(!unify_with_form(tail->ast,pf_to.t->ast,symtab)){
        for(auto it = symtab.begin(); symtab.end() != it; it++){
            delete it->second;
        }

        return 0;
    }

    gdl_rule* nr = new gdl_rule(*r);
    substitute(nr,symtab);
    new_rules.push_back(nr);

    for(auto it = symtab.begin(); symtab.end() != it; it++){
        delete it->second;
    }

    return 1;
}

static void expand_rules(const pred_form& pf_from,const pred_form& pf_to, const vector<gdl_elem*>& g, vector<gdl_rule*>& new_rules){
    for(int gi=0;gi<g.size();gi++){
        gdl_elem* elem = g[gi];

        if(elem->contains<gdl_rule*>()){
            const gdl_rule* rule = elem->get<gdl_rule*>();

            for(int i=0;i<rule->tails.size();i++){
                gdl_term* tail = rule->tails[i];
                if(tail->grounded()) continue;

                pred_form pf = get_form_from_term(tail); //new

                if(std::equal_to<pred_form>()(pf,pf_from)){
                    if(expand_rule(pf_to,rule,i,new_rules)){
                        delete pf.t;

                        //@NOTE: assumes that every rule only has one such predicate in the body
                        break;
                    }
                }

                delete pf.t;
            }
        }
    }
}

void gdl_simplify::var_constrain(vector<gdl_elem*>& g){
    map<string,unordered_set<pred_form>* > head_forms;
    map<string,unordered_set<pred_form>* > tail_forms;

    for(int gi=0;gi<g.size();gi++){
        gdl_elem* elem = g[gi];

        if(elem->contains<gdl_rule*>()){
            gdl_rule* rule = elem->get<gdl_rule*>();

            string key = rule->head->get_pred();
            pred_form pf = get_form_from_term(rule->head); //new

            unordered_set<pred_form>* uset = get_forms(key,head_forms);
            if(uset->find(pf) == uset->end()){
                uset->insert(pf);
            } else {
                delete pf.t;
            }

            for(int i=0;i<rule->tails.size();i++){
                gdl_term* tail = rule->tails[i];
                pred_form pf = get_form_from_term(tail); //new

                unordered_set<pred_form>* uset = get_forms(tail->get_pred(),tail_forms);
                if(uset->find(pf) == uset->end()){
                    uset->insert(pf);
                } else {
                    delete pf.t;
                }
            }
        }
    }

#ifdef GDL_FLATTEN_DEBUG
    //test
    slog("gdl_simplify","head_forms:\n");
    for(auto it=head_forms.begin(); head_forms.end() != it; it++){
        string key = it->first;
        if(tail_forms.find(key) == tail_forms.end()) continue;

        fprintf(stderr,"%s:\n",key.c_str());
        unordered_set<pred_form>* uset = it->second;
        for(auto it2 = uset->begin(); uset->end() != it2; it2++){
            fprintf(stderr,"\t%s\n",it2->t->convertS().c_str());
        }
    }
    slog("gdl_simplify","tail_forms:\n");
    for(auto it=tail_forms.begin(); tail_forms.end() != it; it++){
        string key = it->first;
        if(head_forms.find(key) == head_forms.end()) continue;

        fprintf(stderr,"%s:\n",it->first.c_str());
        unordered_set<pred_form>* uset = it->second;
        for(auto it2 = uset->begin(); uset->end() != it2; it2++){
            fprintf(stderr,"\t%s\n",it2->t->convertS().c_str());
        }
    }
#endif

    vector<gdl_rule*> new_rules;

    var_count = 0;
    for(auto it_h = head_forms.begin(); it_h != head_forms.end(); it_h++){
        string pred = it_h->first;
        unordered_set<pred_form>* forms_h = it_h->second;

        unordered_set<pred_form>* forms_t = get_forms(pred,tail_forms);
        if(forms_t->size() > 1 || forms_t->size() == 0) continue;
        pred_form the_tail_form = *(forms_t->begin());

        int to_expand = true;

        for(auto it_fh = forms_h->begin(); it_fh != forms_h->end(); it_fh++){
            pred_form pfh = *it_fh;

            if(!more_general(the_tail_form,pfh)){
                to_expand = false;
            }
        }

        if(!to_expand) continue;

        for(auto it_fh = forms_h->begin(); it_fh != forms_h->end(); it_fh++){
            pred_form pfh = *it_fh;

#ifdef GDL_FLATTEN_DEBUG
            fprintf(stderr,"from %s to %s\n",the_tail_form.t->convertS().c_str(),pfh.t->convertS().c_str());
#endif
            expand_rules(the_tail_form,pfh,g,new_rules);
        }
    }

    //@NOTE: remove duplicate rules in new_rules, there can be no duplicate new rules under current assumptions

    for(int i=0;i<new_rules.size();i++){
        gdl_elem* nelem = new gdl_elem(new_rules[i]);
        g.push_back(nelem);
    }

    //delete head_forms && tail_forms
    for(auto it = head_forms.begin(); head_forms.end() != it; it++){
        unordered_set<pred_form>* uset = it->second;
        for(auto it2 = uset->begin(); uset->end() != it2;it2++){
            delete it2->t;
        }

        delete uset;
    }
    for(auto it = tail_forms.begin(); tail_forms.end() != it; it++){
        unordered_set<pred_form>* uset = it->second;
        for(auto it2 = uset->begin(); uset->end() != it2;it2++){
            delete it2->t;
        }

        delete uset;
    }
}
