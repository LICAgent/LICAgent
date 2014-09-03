#include "game.h"
#include "parser.h"
#include "slog.h"
#include <sstream>

using std::stringstream;
using std::endl;

//#define USE_AST_HASH1

//===============================
// gdl_ast
//===============================

gdl_ast::gdl_ast()
:t(AST_LIST)
{}

gdl_ast::gdl_ast(const char* s)
:t(AST_ATOM)
,_sym(s)
{}

gdl_ast::gdl_ast(const deque<gdl_ast*>* lt)
:t(AST_LIST)
{
    assert(lt->size() > 0);

    deque<gdl_ast*>::const_iterator it = lt->begin();
    while(it != lt->end()){
        children.push_back(*it);
        it++;
    }
}

gdl_ast::gdl_ast(const gdl_ast& ast)
{
    t = ast.t;

    if(ast.type() == AST_ATOM){
        _sym = ast._sym;
    } else {
        vector<gdl_ast*>::const_iterator it = ast.children.begin();
        while(it != ast.children.end()){
            gdl_ast* sub = *it;
            children.push_back(new gdl_ast(*sub));

            it++;
        }
    }
}

unsigned int gdl_ast::hash() const{
#ifdef USE_AST_HASH1
    if(type() == AST_ATOM){
        return std::hash<string>()(_sym);
    }

    unsigned int ret = 0;
    for(int i=0;i<children.size();i++){
        unsigned int h = 0;
        if(children[i]->type() == AST_ATOM){
            h = std::hash<string>()(_sym);
        } else {
            gdl_ast* child = children[i];
            h = std::hash<string>()(child->convertS());
        }

        ret ^= h;
    }

    return ret;
#else
    return std::hash<string>()(convertS());
#endif

}

gdl_ast::~gdl_ast(){
    vector<gdl_ast*>::iterator it = children.begin();
    while(it != children.end()){
        delete *it;
        it++;
    }
}

void replace_plus(string& s){
    for(unsigned int i=0;i<s.length();i++){
        if(s[i] == '+'){
            s[i] = 'x';
        } else if(s[i] == '-'){
            s[i] = 'l';
        }
    }
}

static string convert_sym(const string& sym){
    if(sym[0] == '?'){
        string ret = sym;
        ret[0] = '_';

        replace_plus(ret);
        return ret;
    }

    if(!strcmp(sym.c_str(),"true")){
        return string("state");
    } else if(!strcmp(sym.c_str(),"init")){
        return string("state");
    } else if(!strcmp(sym.c_str(),"succ")){
        return string("succs");
    } else if(!strcmp(sym.c_str(),"state")){
        return string("z_state_z");
    } else if(!strcmp(sym.c_str(),"member")){
        return string("z_member_z");
    } else if(!strcmp(sym.c_str(),"plus")){
        return string("z_plus_z");
    } else if(!strcmp(sym.c_str(),"minus")){
        return string("z_minus_z");
    } else if(!strcmp(sym.c_str(),"substract")){
        return string("z_substract_z");
    } else if(!strcmp(sym.c_str(),"subtract")){
        return string("z_subtract_z");
    } else if(!strcmp(sym.c_str(),"number")){
        return string("z_number_z");
    } else if(!strcmp(sym.c_str(),"save")){
        return string("z_save_z");
    } else if(!strcmp(sym.c_str(),"between")){
        return string("z_between_z");
    }

    if(isdigit(sym[0])){
        bool has_non_digit = false;
        for(unsigned int i=0;i<sym.length();i++){
            if(!isdigit(sym[i])){
                has_non_digit = true;
                break;
            }
        }

        if(has_non_digit){
            string ret = string("n_") + sym;
            replace_plus(ret);
            return ret;
        }
    }

    string ret = sym;

    replace_plus(ret);
    return ret;
}

string gdl_ast::convert_lp() const{
    if(type() == AST_ATOM){
        string ret = _sym;
        replace_plus(ret);

        if(ret[0] == '?'){
            ret[0] = 'V';
        }

        return ret;
    }

    string ret = children[0]->convert_lp();

    if(children.size() > 1){
        ret += "(";
    }

    vector<gdl_ast*>::const_iterator it = children.begin();
    it++;
    while(it != children.end()){
        ret += (*it)->convert_lp();
        it++;
        
        if(it != children.end()){
            ret += ",";
        }
    }

    if(children.size() > 1){
        ret += ")";
    }

    return ret;
}

string gdl_ast::convert() const{
    if(type() == AST_ATOM){
        return convert_sym(_sym);
    }

    string ret = children[0]->convert();

    if(children.size() > 1){
        ret += "(";
    }

    vector<gdl_ast*>::const_iterator it = children.begin();
    it++;
    while(it != children.end()){
        ret += (*it)->convert();
        it++;
        
        if(it != children.end()){
            ret += ",";
        }
    }

    if(children.size() > 1){
        ret += ")";
    }

    return ret;
}

string gdl_ast::convertS() const{
    if(type() == AST_ATOM){
        return _sym;
    }

    string ret = "(";
    vector<gdl_ast*>::const_iterator it = children.begin();
    while(it != children.end()){
        ret += (*it)->convertS();
        it++;

        if(it != children.end()){
            ret += " ";
        }
    }
    ret += ")";

    return ret;
}

bool gdl_ast::operator==(const gdl_ast& t) const{
    //return convertS() == t.convertS();

    if(type() != t.type())
        return false;

    if(type() == AST_ATOM){
        return _sym == t._sym;
    }

    if(children.size() != t.children.size()){
        return false;
    }

    for(unsigned int i=0;i<children.size();i++){
        if((*children[i]) != (*t.children[i]))
            return false;
    }

    return true;
}

string gdl_ast::get_pred() const{
    if(type() == AST_ATOM) return sym();

    return children[0]->convertS();
}

bool gdl_ast::grounded() const{
    if(type() == AST_ATOM){
        return _sym[0] != '?';
    }

    auto it = children.begin();
    while(it != children.end()){
        if(!(*it)->grounded()){
            return false;
        }

        it++;
    }

    return true;
}

//===============================
// gdl
//===============================

gdl::gdl(const gdl_ast* ast){
    assert(ast->type() == AST_LIST);

    //using namespace std;
    //cout<<"BOMBSHELL:"<<endl;
    //cout<<ast->convert()<<endl<<endl;

    vector<gdl_ast*>::const_iterator it = ast->children.begin();
    while(it != ast->children.end()){
        gdl_elem* elem;
        gdl_term* term;
        gdl_rule* rule;

        gdl_ast* c = *it;
        if(c->type() == AST_ATOM){
            term = new gdl_term(c);
            elem = new gdl_elem(term);
        } else {
            gdl_ast* head = c->children[0];

            assert(head->type() == AST_ATOM);
            if(!strcmp(head->sym(),"<=")){
                if(c->children.size() > 2){
                    rule = new gdl_rule(c);
                    elem = new gdl_elem(rule);
                } else {
                    term = new gdl_term(c->children[1]);
                    elem = new gdl_elem(term);
                }
            } else {
                term = new gdl_term(c);
                elem = new gdl_elem(term);
            }
        }

        desc.push_back(elem);
        it++;
    }
}

gdl::~gdl(){ //need also delete gdl_rules and gdl_terms
    vector<gdl_elem*>::iterator it = desc.begin();

    while(it != desc.end()){
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
}

void gdl::print(FILE* fp){
    vector<gdl_elem*>::iterator it = desc.begin();
    while(it != desc.end()){
        gdl_elem* elem = *it;

        if(elem->contains<gdl_term*>()){
            gdl_term* t = elem->get<gdl_term*>();

            string s = t->convert();
            fprintf(fp,"%s.\n",s.c_str());
        } else if(elem->contains<gdl_rule*>()){
            gdl_rule* r = elem->get<gdl_rule*>();
            
            string s = r->convert();
            fprintf(fp,"%s.\n",s.c_str());
        }

        it++;
    }
}

void gdl::printS(FILE* fp){
    vector<gdl_elem*>::iterator it = desc.begin();
    while(it != desc.end()){
        gdl_elem* elem = *it;

        if(elem->contains<gdl_term*>()){
            gdl_term* t = elem->get<gdl_term*>();

            string s = t->convertS();
            fprintf(fp,"%s\n",s.c_str());
        } else if(elem->contains<gdl_rule*>()){
            gdl_rule* r = elem->get<gdl_rule*>();
            
            string s = r->convertS();
            fprintf(fp,"%s\n",s.c_str());
        }

        it++;
    }
}

string gdl::lparse_ast(const gdl_ast* t){
    string pred = t->children[0]->sym();
    stringstream ret;

    if(pred == "init"){
        ret << "true(" << lparse_ast(t->children[1]) << ",1)";
    } else if(pred == "legal" || pred == "goal"){
        ret << pred << "(" << lparse_ast(t->children[1]) << "," << lparse_ast(t->children[2]);
        ret << ",T) :- time(T)";
    } else if(pred == "next"){
        ret << "legal(" << lparse_ast(t->children[1]) << "," << lparse_ast(t->children[2]);
        ret << ",T+1) :- time(T)";
    } else {
        ret << t->convert_lp();
    }

    return ret.str();
}

string gdl::lparse_rule_tail(const gdl_ast* t){
    stringstream ret;

    string pred;
    if(t->type() == AST_ATOM){
        pred = t->convert_lp();
    } else {
        pred = t->children[0]->convert_lp();
    }

    if(pred == "not"){
        ret << "not ";
        ret << lparse_rule_tail(t->children[1]);
    } else if(pred == "distinct"){
        assert(t->children[1]->type() == AST_ATOM);
        assert(t->children[2]->type() == AST_ATOM);

        ret << t->children[1]->convert_lp() << " != " << t->children[2]->convert_lp();
    } else {
        if(tpreds.find(pred) == tpreds.end()){
            ret << t->convert_lp();
        } else {
            ret << pred << "(";
            for(int i=1;i<t->children.size();i++){
                ret << lparse_rule_tail(t->children[i]);

                if(i != t->children.size()-1){
                    ret << ",";
                }
            }
            if(t->type() != AST_ATOM){
                ret << ",";
            }
            ret << "T)";
        }
    }

    return ret.str();
}

string gdl::lparse_rule(const gdl_rule* t){
    string pred = t->head->get_pred();
    stringstream ret;

    fprintf(stderr,"pred = %s\n",pred.c_str());

    bool add_time = false;

    if(pred == "next"){
        ret << "true(" << lparse_ast(t->head->ast->children[1]) << ",T+1) :- ";
        add_time = true;
    } else if(pred == "terminal"){
        ret << "terminal(T) :- ";
        add_time = true;
    } else if(pred == "init"){
        ret << "true(" << lparse_ast(t->head->ast->children[1]) << ",1) :- ";
        add_time = true;
    } else {
        ret << lparse_rule_tail(t->head->ast) << " :- ";

        if(tpreds.find(pred) != tpreds.end()){
            add_time = true;
        }
    }

    for(int i=0;i<t->tails.size();i++){
        gdl_term* tail = t->tails[i];

        ret << lparse_rule_tail(tail->ast);
        if(i != t->tails.size() - 1){
            ret << ",";
        }
    }
    if(add_time){
        ret << ", time(T)";
    }

    ret << ".";

    return ret.str();
}

void gdl::compute_tpreds(){
    tpreds.insert("true");
    tpreds.insert("does");
    tpreds.insert("goal");
    tpreds.insert("terminal");
    tpreds.insert("legal");

    bool change = true;
    while(change){
        change = false;

        for(int i=0;i<desc.size();i++){
            gdl_elem* elem = desc[i];
            if(elem->contains<gdl_term*>()) continue;

            gdl_rule* rule = elem->get<gdl_rule*>();

            if(tpreds.find(rule->head->get_pred()) != tpreds.end()) continue;

            for(int i=0;i<rule->tails.size();i++){
                gdl_term* tail = rule->tails[i];

                if(tail->get_pred() == "not"){
                    gdl_ast* child = tail->ast->children[0];
                    if(tpreds.find(child->get_pred()) != tpreds.end()){
                        tpreds.insert(rule->head->get_pred());
                        change = true;

                        goto L1;
                    }
                    continue;

                    L1: break;
                } else {
                    if(tpreds.find(tail->get_pred()) != tpreds.end()){
                        tpreds.insert(rule->head->get_pred());
                        change = true;

                        break;
                    }
                }
            }
        }
    }

    fprintf(stderr,"tpreds:\n");
    for(auto it = tpreds.begin(); it != tpreds.end(); it++){
        fprintf(stderr,"%s\n",it->c_str());
    }
}

string gdl::convert_lparse(){
    if(tpreds.size() == 0){
        compute_tpreds();
    }

    stringstream ret;

    ret << "#hide." << endl;
    ret << "#show does/3." << endl;

    ret << "terminal(T+1) :- terminal(T), time(T)." << endl;
    ret << "goal(R,M,T+1) :- goal(R,M,T), time(T), terminal(T)." << endl;

    ret << "1 { does(R,M,T) : input(R,M) } 1 :- role(R), time(T), not terminal(T)." << endl;
    ret << ":- does(R,M,T) , not legal(R,M,T), time(T), role(R), input(R,M)." << endl;
    ret << ":- 0 { terminal(T) : time(T) } 0." << endl;
    ret << ":- terminal(T), role(R), not goal(R,100,T), time(T)." << endl;

    for(int i=0;i<desc.size();i++){
        string line;

        gdl_elem* elem = desc[i];
        if(elem->contains<gdl_term*>()){
            line = lparse_ast(elem->get<gdl_term*>()->ast);
            ret << line << "." << endl;
        } else {
            line = lparse_rule(elem->get<gdl_rule*>());
            ret << line << endl;
        }
    }

    return ret.str();
}

//===============================
// gdl_term
//===============================


gdl_term::gdl_term(const gdl_ast* t){
    ast = new gdl_ast(*t);
}

gdl_term::gdl_term(const gdl_term& t){
    ast = new gdl_ast(*t.ast);
}

gdl_term::~gdl_term(){
    delete ast;
}

string gdl_term::convertS() const{
    return ast->convertS();
}

string gdl_term::convert() const{
    return ast->convert();
}

unsigned int gdl_term::hash() const{
    return ast->hash();
}

bool gdl_term::operator==(const gdl_term& t) const{
    return *(ast) == *(t.ast);
}

string gdl_term::get_pred() const{
    return ast->get_pred();

}

int gdl_term::get_arity() const{
    if(ast->type() == AST_ATOM) return 0;

    return ast->children.size() - 1;
}

//===============================
// gdl_rule
//===============================

gdl_rule::gdl_rule(const gdl_ast* t){
    assert(t->children.size() > 2);
    assert(!strcmp(t->children[0]->sym(),"<="));

    head = new gdl_term(t->children[1]);
    for(unsigned int i=2;i<t->children.size();i++){
        tails.push_back(new gdl_term(t->children[i]));
    }
}

gdl_rule::gdl_rule(const gdl_rule& r){
    head = new gdl_term(*r.head);

    vector<gdl_term*>::const_iterator it = r.tails.begin();
    while(it != r.tails.end()){
        gdl_term* sub = *it;
        tails.push_back(new gdl_term(*sub));
        it++;
    }
}

gdl_rule::~gdl_rule(){
    delete head;
    vector<gdl_term*>::iterator it = tails.begin();
    while(it != tails.end()){
        delete *it;
        it++;
    }
}

string gdl_rule::convertS() const{
    string ret = "(<= " + head->convertS() + " ";

    vector<gdl_term*>::const_iterator it = tails.begin();
    while(it != tails.end()){
        ret += (*it)->convertS();

        it++;

        if(it != tails.end()){
            ret += " ";
        }
    }
    ret += ")";

    return ret;
}

string gdl_rule::convert() const{
    string ret = head->convert() + " :- ";

    vector<gdl_term*> ntails = tails;
    int n = ntails.size();

    string hpred = head->get_pred();
    for(int i=0;i<n;i++){
        if(ntails[i]->get_pred() == hpred){
            gdl_term* recur = ntails[i];
            ntails.erase(ntails.begin() + i);
            ntails.push_back(recur);
            
            n--;
        }
    }

    vector<gdl_term*>::const_iterator it = ntails.begin();
    while(it != ntails.end()){
        string ts = (*it)->convert();
        ret += ts;

        it++;

        if(it != ntails.end()){
            ret += ",";
        }
    }

    return ret;
}

string gdl_rule::convert_lp() const{
    string ret = head->convert_lp() + " :- ";

    vector<gdl_term*> ntails = tails;
    int n = ntails.size();

    string hpred = head->get_pred();
    for(int i=0;i<n;i++){
        if(ntails[i]->get_pred() == hpred){
            gdl_term* recur = ntails[i];
            ntails.erase(ntails.begin() + i);
            ntails.push_back(recur);
            
            n--;
        }
    }

    vector<gdl_term*>::const_iterator it = ntails.begin();
    while(it != ntails.end()){
        string ts = (*it)->convert_lp();
        ret += ts;

        it++;

        if(it != ntails.end()){
            ret += ",";
        }
    }

    return ret;
}

unsigned int gdl_rule::hash() const{
    unsigned int ret = 0;
    vector<gdl_term*>::const_iterator it = tails.begin();
    while(it != tails.end()){
        ret ^= (*it)->hash();

        it++;
    }
    ret ^= head->hash();

    return ret;
}

//=======================================================
//move_weight
//=======================================================

static double to_double(const string& s){
    stringstream stream(s);
    double ret;
    stream >> ret;

    return ret;
}

mw_vec* move_weight::parse_vec(const string& s,string& src){ //@NOTE: all output are now on one line
    gdl_ast* o_mw_ast = parser::parse_ast(s); //new
    if(!o_mw_ast){
        return NULL;
    }
    if(o_mw_ast->type() == AST_ATOM || o_mw_ast->children.size() > 1){
        fprintf(stderr,"strange o_mw_ast:%s\n",o_mw_ast->convertS().c_str());

        delete o_mw_ast;
        return NULL;
    }
    gdl_ast* mw_ast = o_mw_ast->children[0];
    if(mw_ast->type() == AST_ATOM || mw_ast->children.size() <= 1){
        fprintf(stderr,"strange mw_ast:%s\n",mw_ast->convertS().c_str());

        delete o_mw_ast;
        return NULL;
    }

    string mw_src = mw_ast->children[0]->convertS();
    if(mw_src != "single" && mw_src != "mcts" && mw_src != "clasp"){
        delete o_mw_ast;

        return NULL;
    }
    src = mw_src;

    mw_vec* ret = new mw_vec(); //new
    for(int i=1;i<mw_ast->children.size();i++){
        gdl_ast* ast = mw_ast->children[i];

        if(ast->children.size() != 3){
            delete mw_ast;
            move_weight::free_vec(ret);

            return NULL;
        }

        gdl_ast* move = ast->children[0];
        gdl_ast* util_ast = ast->children[2];
        gdl_ast* weight_ast = ast->children[1];

        if(util_ast->type() != AST_ATOM || weight_ast->type() != AST_ATOM){
            delete mw_ast;
            move_weight::free_vec(ret);

            return NULL;
        }

        double Q = to_double(util_ast->convertS());
        double w = to_double(weight_ast->convertS());
        gdl_term* tmove = new gdl_term(move); //new

        move_weight* mw = new move_weight(tmove,w,Q);
        ret->push_back(mw);

        delete tmove;
    }
    delete o_mw_ast;

    return ret;
}

void move_weight::free_vec(mw_vec* mws){
    for(int i=0;i<mws->size();i++){
        delete mws->at(i);
    }

    delete mws;
}

string move_weight::convertS(const mw_vec* mws, const string& src){ //@NOTE: get all output in one line!
    stringstream stream;
    stream << "(" << src;

    for(int i=0;i<mws->size();i++){
        move_weight* mw = mws->at(i);

        char str_w[100],str_Q[100];

        sprintf(str_w,"%.3f",mw->w);
        sprintf(str_Q,"%.3f",mw->Q);

        stream << " (" << mw->t->convertS() << " " << str_w << " " << str_Q << ")";
    }
    stream << ")";

    string ret = stream.str();

    slog("move_weight","convertS result:\n%s\n",ret.c_str());

    return ret;
}

const gdl_term* move_weight::best_move_in_vec(const mw_vec* mws,double* max){
    double mQ = -1.0;
    int reti = -1;
    vector<int> ties;

    for(int i=0;i<mws->size();i++){
        move_weight* mw = mws->at(i);

        if(mw->Q > mQ){
            mQ = mw->Q;
            ties.clear();
            ties.push_back(i);
        } else if(mw->Q == mQ){
            ties.push_back(i);
        }
    }
    assert(ties.size() != 0);
    //if(ties.size() == 0){
        //return NULL;
    //}

    reti = ties[rand() % ties.size()];
    const gdl_term* ret = mws->at(reti)->t;

    if(max){
        *max = mQ;
    }

    return ret;
}

//==================================================================
// pred_form
//==================================================================

static void pf_traverse(const gdl_ast* t,int* nvars){
    switch(t->type()){
        case AST_ATOM:
        {
            if(t->_sym == "##"){
                (*nvars)++;
            }

            break;
        }
        case AST_LIST:
        {
            for(int i=0;i<t->children.size();i++){
                pf_traverse(t->children[i],nvars);
            }
            break;
        }
    }
}

pred_form::pred_form(gdl_term* _t){
    t = _t;

    pf_traverse(t->ast,&nvars);
}

static void transform_ast(gdl_ast* t){
    if(t->type() == AST_ATOM){
        t->_sym = "##";

        return;
    }

    for(int i=1;i<t->children.size();i++){
        transform_ast(t->children[i]);
    }
}

gdl_term* pred_form::transform(const gdl_term* t){
    gdl_term* ret = new gdl_term(*t);
    if(ret->ast->type() != AST_ATOM){
        transform_ast(ret->ast);
    }

    return ret;
}

pred_form::~pred_form(){ }

namespace std{
    std::size_t hash<pred_form>::operator()(const pred_form& pf) const{
        return pf.t->hash();
    }

    bool equal_to<pred_form>::operator()(const pred_form& pf1,const pred_form& pf2) const{
        return (*pf1.t) == (*pf2.t);
    }
};
