#include "propnet.h"
#include "gdl_flatten.h"
#include <sstream>
#include <map>
#include <queue>
#include "slog.h"

//#define PROPNET_CHECK_CONSISTENCY
//#define PROPNET_PROFILE

using std::pair;
using std::queue;

propnet::propnet()
{ }

propnet::~propnet(){
    clear_dynamics();
}

void propnet::clear_propnet(){
    for(auto i = all_nodes.begin(); all_nodes.end() != i; i++){
        delete *i;
    }
    all_nodes.clear();

    for(auto it = pred_map.begin(); pred_map.end() != it; it++){
        delete it->first.t;
    }
    pred_map.clear();

    bases.clear();
    inits.clear();
    nexts.clear();
    does.clear();
    legals.clear();
    goals.clear();
    conns.clear();
    term = const_true = const_false = NULL;

    _roles.clear();

    compac_outputs.clear();
    compac_inputs_count.clear();
    compac_input.clear();
    compac_types.clear();

    marked_legals.clear();
}

void propnet::begin_bench(){
    propagate_time = 0.0;
}

void propnet::end_bench(){
    fprintf(stderr,"propagate_time:%lf\n",propagate_time);
}

void propnet::handle_isolated_nodes(){
    queue<int> Q;
    set<int> marked;

    for(auto it = bases.begin();it != bases.end(); it++){
        node* n = *it;
        Q.push(n->id);
        marked.insert(n->id);
    }
    for(auto it = does.begin();it != does.end(); it++){
        node* n = *it;
        Q.push(n->id);
        marked.insert(n->id);
    }
    Q.push(const_true->id);
    marked.insert(const_true->id);
    Q.push(const_false->id);
    marked.insert(const_false->id);

    while(Q.size() > 0){
        int cur = Q.front();
        Q.pop();

        node* n = all_nodes[cur];
        for(auto it = n->outputs.begin(); it != n->outputs.end(); it++){
            node* n_out = *it;
            int n_out_id = n_out->id;

            if(marked.find(n_out_id) == marked.end()){
                Q.push(n_out_id);
                marked.insert(n_out_id);
            }
        }
    }

    for(int i=0;i<all_nodes.size();i++){
        if(marked.find(i) == marked.end()){
            node* n = all_nodes[i];

            //if bases and does cannot affect this node, then presumably this is an illegal predicate
            for(auto it = n->inputs.begin(); n->inputs.end() != it; it++){
                node* n_in = *it;
                n_in->outputs.erase(n);
            }
            n->inputs.clear();

            const_false->outputs.insert(n);
            n->inputs.insert(const_false);
        }
    }
}

bool propnet::init(){
    init_dynamic();

    for(auto it = inits.begin(); inits.end() != it; it++){
        node* n = *it;
        set_bit(n->id,1);

        cur_state.insert(n->id);
    }

    //full propagation
    first_propagate();

#ifdef PROPNET_CHECK_CONSISTENCY
    check_consistency();
#endif

    return true;
}

void propnet::first_propagate(){
    queue<int> Q;
    unordered_map<int,int> conn_vs;

    for(auto it = bases.begin();it != bases.end(); it++){
        node* n = *it;
        Q.push(n->id);
    }
    for(auto it = does.begin();it != does.end(); it++){
        node* n = *it;
        Q.push(n->id);
    }
    Q.push(const_true->id);
    Q.push(const_false->id);

    int vs = 0;

    while(Q.size() > 0){
        int cur = Q.front();
        Q.pop();

        vs++;

        //test
        //fprintf(stderr,"cur=%d, val=%d\n",cur,get_bit(cur));

        node* n = all_nodes[cur];
        int n_val = get_bit(cur);
        for(auto it = n->outputs.begin(); it != n->outputs.end(); it++){
            node* n_out = *it;
            int n_out_id = n_out->id;

            switch(n_out->type()){
                case AND: case OR:
                {
                    if(get_bit(cur)){
                        inc_count(n_out_id);
                    }

                    if(conn_vs.find(n_out_id) == conn_vs.end()){
                        conn_vs.insert(pair<int,int>(n_out_id,1));
                    } else {
                        conn_vs[n_out_id]++;
                    }

                    //test
                    //fprintf(stderr,"conn_vs[%d]=%d, inputs.size=%d\n",n_out_id,conn_vs[n_out_id],n_out->inputs.size());

                    if(conn_vs[n_out_id] == n_out->inputs.size()){
                        //test
                        //fprintf(stderr,"adding %d\n",n_out_id);

                        int val;
                        if(n_out->type() == AND){
                            val = get_count(n_out_id) == n_out->inputs.size();
                        } else {
                            val = get_count(n_out_id) > 0;
                        }
                        set_bit(n_out_id,val);

                        Q.push(n_out_id);
                    }
                    break;
                }
                case NOT:
                {
                    set_bit(n_out_id,1-n_val);
                    Q.push(n_out_id);

                    break;
                }
                case LEGAL:
                {
                    set_bit(n_out_id,n_val);
                    if(get_bit(n_out_id)){
                        cur_legals.insert(n_out_id);
                    }
                    Q.push(n_out_id);

                    break;
                }
                case NEXT:
                {
                    set_bit(n_out_id,n_val);
                    if(get_bit(n_out_id)){
                        cur_nexts.insert(n_out_id);
                    }
                    Q.push(n_out_id);

                    break;
                }
                case GOAL:
                {
                    set_bit(n_out_id,n_val);
                    if(get_bit(n_out_id)){
                        cur_goals.insert(n_out_id);
                    }
                    Q.push(n_out_id);

                    break;
                }
                default:
                {
                    set_bit(n_out_id,n_val);
                    Q.push(n_out_id);

                    break;
                }
            }

            //test
            //fprintf(stderr,"\tn_out_id=%d, val=%d\n",n_out_id,get_bit(n_out_id));
        }
        set_prev_bit(cur,n_val);
    }

    //test
    //fprintf(stderr,"vs=%d, all_nodes.size()=%d\n",vs,all_nodes.size());
}

vector<string> propnet::roles(){
    return _roles;
}

bool propnet::terminal(){
    return get_bit(term->id);
}

int propnet::goal(const string& role){
    goal_node* n = goals_from_role(role);

    if(!n) return -1;

    return n->goal_value;
}

bool propnet::legal(const string& role, const gdl_term& move){
    action_node* n = legal_from_role_term(role,&move);

    if(!n) return false;

    return get_bit(n->id) == 1;
}

terms* propnet::state(){
    terms* ret = new terms();
    for(auto it = cur_state.begin(); cur_state.end() != it; it++){
        int id = *it;
        prop_node* n = (prop_node*)all_nodes[id];

        gdl_term* nt = new gdl_term(n->prop);
        ret->push_back(nt);
    }

    return ret;
}

terms* propnet::get_move(const string& role){
    terms* ret = new terms();
    for(auto it = cur_legals.begin(); cur_legals.end() != it; it++){
        int id = *it;
        action_node* n = (action_node*)all_nodes[id];

        assert(get_bit(id) == 1);

        if(n->role == role){
            if(marked_legals.find(id) != marked_legals.end()){ //don't return if the move doesn't affect
                gdl_term* nt = new gdl_term(n->action);
                ret->push_back(nt);
            }
        }
    }

    if(ret->size() == 0){ //if there are no relevant moves, return the first legal move
        for(auto it = cur_legals.begin(); cur_legals.end() != it; it++){
            int id = *it;
            action_node* n = (action_node*)all_nodes[id];

            if(n->role == role){
                gdl_term* nt = new gdl_term(n->action);
                ret->push_back(nt);

                break;
            }
        }
    }

    return ret;
}

int propnet::make_move(const terms& moves){
    unordered_set<int> move_nodes;
    for(int i=0;i<moves.size();i++){
        action_node* n = does_from_role_term(_roles[i],moves[i]);
        set_bit(n->id,1);

        move_nodes.insert(n->id);
    }

#ifdef PROPNET_PROFILE
{
    timer* T = new timer();
#endif

    propagate(unordered_set<int>(),move_nodes);

#ifdef PROPNET_PROFILE
    propagate_time += T->time_elapsed();
    delete T;
}
#endif

#ifdef PROPNET_CHECK_CONSISTENCY
    check_consistency();
#endif

    //clear state:
    clear_state();

    //transition and calculate difference between two states
    unordered_set<int> diff_state = cur_state;
    cur_state.clear();

    for(auto it = cur_nexts.begin(); cur_nexts.end() != it; it++){
        int id = *it;
        prop_node* next_n = (prop_node*)all_nodes[id];

        node* n = bases_from_next(next_n);
        int n_id = n->id;
        cur_state.insert(n_id);
        set_bit(n_id,1);

        if(diff_state.find(n_id) == diff_state.end()){
            diff_state.insert(n_id);
        } else {
            diff_state.erase(n_id);
        }

        assert(n->inputs.size() == 0);
    }

    //clear actions
    for(auto it = move_nodes.begin(); it != move_nodes.end(); it++){
        int id = *it;
        set_bit(id,0);
    }

#ifdef PROPNET_PROFILE
{
    timer* T = new timer();
#endif

    propagate(diff_state,move_nodes);

#ifdef PROPNET_PROFILE
    propagate_time += T->time_elapsed();
    delete T;
}
#endif

#ifdef PROPNET_CHECK_CONSISTENCY
    check_consistency();
#endif
    
    return 0;
}

int propnet::retract(const terms& sts){
    clear_state();

    unordered_set<int> diff_state = cur_state;
    cur_state.clear();
    for(int i=0;i<sts.size();i++){
        gdl_term* p = sts[i];

        prop_node* n = bases_from_term(p);

        int n_id = n->id;
        set_bit(n_id,1);
        cur_state.insert(n_id);

        if(diff_state.find(n_id) == diff_state.end()){
            diff_state.insert(n_id);
        } else {
            diff_state.erase(n_id);
        }
    }

#ifdef PROPNET_PROFILE
{
    timer* T = new timer();
#endif

    propagate(diff_state,unordered_set<int>());

#ifdef PROPNET_PROFILE
    propagate_time += T->time_elapsed();
    delete T;
}
#endif

#ifdef PROPNET_CHECK_CONSISTENCY
    check_consistency();
#endif

    return 0;
}

int propnet::retract_and_move(const terms& sts, const terms& moves){ //two propagates instead of three with two separate calls
    clear_state();

    unordered_set<int> diff_state = cur_state;
    cur_state.clear();
    for(int i=0;i<sts.size();i++){
        gdl_term* p = sts[i];
        prop_node* n = bases_from_term(p);

        int n_id = n->id;
        set_bit(n_id,1);
        cur_state.insert(n_id);

        if(diff_state.find(n_id) == diff_state.end()){
            diff_state.insert(n_id);
        } else {
            diff_state.erase(n_id);
        }
    }
    unordered_set<int> move_nodes;
    for(int i=0;i<moves.size();i++){
        action_node* n = does_from_role_term(_roles[i],moves[i]);
        set_bit(n->id,1);

        move_nodes.insert(n->id);
    }

#ifdef PROPNET_PROFILE
{
    timer* T = new timer();
#endif

    propagate(diff_state,move_nodes);

#ifdef PROPNET_PROFILE
    propagate_time += T->time_elapsed();
    delete T;
}
#endif

#ifdef PROPNET_CHECK_CONSISTENCY
    check_consistency();
#endif

//================
//move
//================

    //clear state:
    clear_state();

    //transition and calculate difference between two states
    diff_state = cur_state;
    cur_state.clear();

    for(auto it = cur_nexts.begin(); cur_nexts.end() != it; it++){
        int id = *it;
        prop_node* next_n = (prop_node*)all_nodes[id];

        node* n = bases_from_next(next_n);
        int n_id = n->id;
        cur_state.insert(n_id);
        set_bit(n_id,1);

        // (prev_state <union> cur_state) - (prev_state <intersect> cur_state)
        if(diff_state.find(n_id) == diff_state.end()){
            diff_state.insert(n_id);
        } else {
            diff_state.erase(n_id);
        }

        assert(n->inputs.size() == 0);
    }

    //clear actions
    for(auto it = move_nodes.begin(); it != move_nodes.end(); it++){
        int id = *it;

        set_bit(id,0);
    }

#ifdef PROPNET_PROFILE
{
    timer* T = new timer();
#endif

    propagate(diff_state,move_nodes);

#ifdef PROPNET_PROFILE
    propagate_time += T->time_elapsed();
    delete T;
}
#endif

#ifdef PROPNET_CHECK_CONSISTENCY
    check_consistency();
#endif
    
    return 1;
}

//===========================================================

node* propnet::find_node_from_term(gdl_term* t){
    auto result = pred_map.find(pred_wrapper(t));
    if(result == pred_map.end()) return NULL;

    return result->second;
}

prop_node* propnet::bases_from_next(prop_node* n){
    gdl_term* tmp = new gdl_term(n->prop);
    prop_node* ret = bases_from_term(tmp);

    delete tmp;
    return ret;
}

prop_node* propnet::nexts_from_base(prop_node* n){
    gdl_ast* ast = new gdl_ast();
    ast->children.push_back(new gdl_ast("next"));
    ast->children.push_back(new gdl_ast(*n->prop));
    gdl_term* nt = new gdl_term(ast);

    prop_node* ret = (prop_node*)find_node_from_term(nt);

    delete nt;
    delete ast;
    return ret;
}

action_node* propnet::legals_from_does(action_node* n){
    gdl_ast* ast = new gdl_ast();
    ast->children.push_back(new gdl_ast("legal"));
    ast->children.push_back(new gdl_ast(n->role.c_str()));
    ast->children.push_back(new gdl_ast(*n->action));
    gdl_term* nt = new gdl_term(ast);

    action_node* ret = (action_node*)find_node_from_term(nt);

    delete nt;
    delete ast;
    return ret;
}

prop_node* propnet::bases_from_term(const gdl_term* t){
    gdl_ast* ast = new gdl_ast();
    ast->children.push_back(new gdl_ast("true"));
    ast->children.push_back(new gdl_ast(*t->ast));
    gdl_term* nt = new gdl_term(ast);

    prop_node* ret = (prop_node*)find_node_from_term(nt);

    delete ast;
    delete nt;
    return ret;
}

action_node* propnet::does_from_role_term(const string& r,const gdl_term* t){
    gdl_ast* ast = new gdl_ast();
    ast->children.push_back(new gdl_ast("does"));
    ast->children.push_back(new gdl_ast(r.c_str()));
    ast->children.push_back(new gdl_ast(*t->ast));
    gdl_term* nt = new gdl_term(ast);

    action_node* ret = (action_node*)find_node_from_term(nt);

    delete ast;
    delete nt;
    return ret;
}

action_node* propnet::legal_from_role_term(const string& r,const gdl_term* t){
    gdl_ast* ast = new gdl_ast();
    ast->children.push_back(new gdl_ast("legal"));
    ast->children.push_back(new gdl_ast(r.c_str()));
    ast->children.push_back(new gdl_ast(*t->ast));
    gdl_term* nt = new gdl_term(ast);

    action_node* ret = (action_node*)find_node_from_term(nt);

    delete ast;
    delete nt;
    return ret;
}

goal_node* propnet::goals_from_role(const string& r){
    goal_node* ret = NULL;
    for(auto it = cur_goals.begin(); cur_goals.end() != it; it++){
        int id = *it;
        goal_node* n = (goal_node*)all_nodes[id];

        assert(get_bit(id) == 1);

        if(n->role == r){
            ret = n;
            break;
        }
    }
    return ret;
}

void propnet::clear_state(){
    for(auto it = cur_state.begin(); cur_state.end() != it; it++){
        int id = *it;
        set_bit(id,0);
    }
}

static string print_node(node* n);
static void print_wire(node* n,FILE* fp);

void propnet::propagate(const unordered_set<int>& base_set, const unordered_set<int>& does_set){
    queue<int> Q;

    for(auto it = base_set.begin(); base_set.end() != it; it++){
        Q.push(*it);
    }
    for(auto it = does_set.begin(); does_set.end() != it; it++){
        Q.push(*it);
    }

    while(Q.size() > 0){
        int cur = Q.front();
        Q.pop();

        if(get_prev_bit(cur) == get_bit(cur)) continue;
        int cur_val = get_bit(cur);

        for(int i=0;i<compac_outputs[cur].size();i++){
            int n_out_id = compac_outputs[cur][i];

            switch(compac_types[n_out_id]){
                case AND: case OR:
                {
                    //fprintf(stderr,"BEFORE: cur=%d, cur_val=%d, n_out_id=%d, get_count(n_out_id)=%d\n",
                            //cur,cur_val,n_out_id,get_count(n_out_id));
                    if(cur_val == 1){
                        inc_count(n_out_id);
                    } else {
                        dec_count(n_out_id);
                    }
                    //fprintf(stderr,"AFTER: cur=%d, cur_val=%d, n_out_id=%d, get_count(n_out_id)=%d\n",
                            //cur,cur_val,n_out_id,get_count(n_out_id));

                    break;
                }
                default:
                {
                    break;
                }
            }

            process(n_out_id);
            if(get_prev_bit(n_out_id) != get_bit(n_out_id)){
                Q.push(n_out_id);
            }
        }
        set_prev_bit(cur,cur_val);
    }
}

void propnet::process(int i){
    switch(compac_types[i]){
        case AND:
        {
            set_bit(i,get_count(i) == compac_inputs_count[i]);
            break;
        }
        case OR:
        {
            set_bit(i,get_count(i) > 0);
            break;
        }
        case NOT:
        {
            int in_id = compac_input[i];
            set_bit(i,1 - get_bit(in_id));
            break;
        }
        case LEGAL:
        {
            int in_id = compac_input[i];
            set_bit(i,get_bit(in_id));

            if(get_prev_bit(i) != get_bit(i)){
                if(get_bit(i) == 1){
                    cur_legals.insert(i);
                } else {
                    cur_legals.erase(i);
                }
            }
            break;
        }
        case NEXT:
        {
            int in_id = compac_input[i];
            set_bit(i,get_bit(in_id));

            if(get_prev_bit(i) != get_bit(i)){
                if(get_bit(i) == 1){
                    cur_nexts.insert(i);
                } else {
                    cur_nexts.erase(i);
                }
            }
            break;
        }
        case GOAL:
        {
            int in_id = compac_input[i];
            set_bit(i,get_bit(in_id));

            if(get_prev_bit(i) != get_bit(i)){
                if(get_bit(i) == 1){
                    cur_goals.insert(i);
                } else {
                    cur_goals.erase(i);
                }
            }
            break;
        }
        default:
        {
            int in_id = compac_input[i];
            if(in_id != -1){
                set_bit(i,get_bit(in_id));
            }
            break;
        }
    }
}

//===========================================================

terms* propnet::get_bases(){
    terms* ret = new terms();
    for(auto it = bases.begin(); bases.end() != it; it++){
        prop_node* cn = (prop_node*)(*it);
        ret->push_back(new gdl_term(cn->prop));
    }
    
    return ret;
}

terms* propnet::get_inputs(const string& role){
    terms* ret = new terms();
    for(auto it = does.begin(); does.end() != it; it++){
        action_node* cn = (action_node*)(*it);
        if(cn->role == role){
            ret->push_back(new gdl_term(cn->action));
        }
    }

    return ret;
}

//=========================================================

string print_node(node* n){
    stringstream ss;
    switch(n->type()){
        case BASE:
        {
            prop_node* cn = (prop_node*)n;
            ss << "(true " << cn->prop->convertS() << ")";

            break;
        }
        case NEXT:
        {
            prop_node* cn = (prop_node*)n;
            ss << "(next " << cn->prop->convertS() << ")";

            break;
        }
        case VIEW:
        {
            prop_node* cn = (prop_node*)n;
            ss << cn->prop->convertS();

            break;
        }

        case LEGAL:
        {
            action_node* cn = (action_node*)n;
            ss << "(legal " << cn->role << " " << cn->action->convertS() << ")";

            break;
        }
        case DOES:
        {
            action_node* cn = (action_node*)n;
            ss << "(does " << cn->role << " " << cn->action->convertS() << ")";

            break;
        }
        case GOAL:
        {
            goal_node* cn = (goal_node*)n;
            ss << "(goal " << cn->role << " " << cn->goal_value << ")";

            break;
        }
        case TERM:
        {
            ss << "terminal";
            break;
        }
        case AND:
        {
            ss << "and";
            break;
        }
        case OR:
        {
            ss << "or";
            break;
        }
        case NOT:
        {
            ss << "not";
            break;
        }
    }

    return ss.str();
}

static void print_wire(node* n,FILE* fp){
    for(auto it = n->inputs.begin(); n->inputs.end() != it; it++){
        node* n_in = *it;
        fprintf(fp,"\t%d <- %d\n",n->id,n_in->id);
    }
    for(auto it = n->outputs.begin(); n->outputs.end() != it; it++){
        node* n_out = *it;
        fprintf(fp,"\t%d -> %d\n",n->id,n_out->id);
    }
}

void print_wire_id(int id){
    node* n = propnet::all_nodes[id];
    print_wire(n,stderr);
}

void print_node_id(int id){
    node* n = propnet::all_nodes[id];
    fprintf(stderr,"%s\n",print_node(n).c_str());
}

void propnet::print_net(FILE* fp){
    for(int i=0;i<all_nodes.size();i++){
        node* n = all_nodes[i];
        string s = print_node(n);
        fprintf(fp,"node #%d: %s[%d]\n",n->id,s.c_str(),get_bit(n->id));

        print_wire(n,fp);

        fprintf(fp,"\n");
    }

    //set<node*> bases;
    print_bases(fp);

    //set<node*> inits; //inits is a subset of bases
    slog("propnet","inits:\n");
    for(auto it = inits.begin(); inits.end() != it; it++){
        node* n = *it;
        string s = print_node(n);
        fprintf(fp,"\tnode #%d: %s\n",n->id,s.c_str());
    }

    //set<node*> nexts;
    print_nexts(fp);

    //set<node*> does;
    slog("propnet","does:\n");
    for(auto it = does.begin(); does.end() != it; it++){
        node* n = *it;
        string s = print_node(n);
        fprintf(fp,"\tnode #%d: %s\n",n->id,s.c_str());
    }

    //set<node*> legals;
    slog("propnet","legals:\n");
    for(auto it = legals.begin(); legals.end() != it; it++){
        node* n = *it;
        string s = print_node(n);
        fprintf(fp,"\tnode #%d: %s\n",n->id,s.c_str());
    }

    //set<node*> goals;
    slog("propnet","goals:\n");
    for(auto it = goals.begin(); goals.end() != it; it++){
        node* n = *it;
        string s = print_node(n);
        fprintf(fp,"\tnode #%d: %s\n",n->id,s.c_str());
    }

    //node* term;
    slog("propnet","term:\n");
    string sterm = print_node(term);
    fprintf(fp,"\tnode #%d: %s\n",term->id,sterm.c_str());

    //set<node*> conns;
    slog("propnet","conns:\n");
    for(auto it = conns.begin(); conns.end() != it; it++){
        node* n = *it;
        string s = print_node(n);
        fprintf(fp,"\tnode #%d: %s\n",n->id,s.c_str());
    }
}

void propnet::print_bases(FILE* fp){
    slog("propnet","bases:\n");
    for(auto it = bases.begin(); bases.end() != it; it++){
        node* n = *it;
        string s = print_node(n);
        fprintf(fp,"\tnode #%d: %s\n",n->id,s.c_str());
    }
}

void propnet::print_nexts(FILE* fp){
    slog("propnet","nexts:\n");
    for(auto it = nexts.begin(); nexts.end() != it; it++){
        node* n = *it;
        string s = print_node(n);
        fprintf(fp,"\tnode #%d: %s\n",n->id,s.c_str());
    }
}

void propnet::check_consistency(){
    for(int i=0;i<all_nodes.size();i++){
        node* n = all_nodes[i];

        assert(get_bit(i) == get_prev_bit(i));

        switch(n->type()){
            case AND:
            {
                int count = 0;
                for(auto it = n->inputs.begin(); n->inputs.end() != it; it++){
                    node* n_in = *it;

                    count += get_bit(n_in->id);
                }

                assert(get_count(i) == count);
                assert(get_bit(i) == (count == n->inputs.size()));
                break;
            }
            case OR:
            {
                int count = 0;
                for(auto it = n->inputs.begin(); n->inputs.end() != it; it++){
                    node* n_in = *it;

                    count += get_bit(n_in->id);
                }

                assert(get_count(i) == count);
                assert(get_bit(i) == (count > 0));
                break;
            }
            case NOT:
            {
                node* n_in = *(n->inputs.begin());
                assert(get_bit(n_in->id) == (1 - get_bit(n->id)));
                break;
            }
            default:
            {
                if(n->inputs.size() > 0){
                    node* n_in = *(n->inputs.begin());
                    assert(get_bit(n_in->id) == get_bit(n->id));
                }
                break;
            }
        }
    }

    //check curs
    //set<node*> cur_state;
    for(auto it = bases.begin(); bases.end() != it; it++){
        node* n = *it;
        if(get_bit(n->id) == 1){
            assert(cur_state.find(n->id) != cur_state.end());
        } else {
            assert(cur_state.find(n->id) == cur_state.end());
        }
    }
    //set<node*> cur_legals;
    for(auto it = legals.begin(); legals.end() != it; it++){
        node* n = *it;
        if(get_bit(n->id) == 1){
            assert(cur_legals.find(n->id) != cur_legals.end());
        } else {
            assert(cur_legals.find(n->id) == cur_legals.end());
        }
    }
    //set<node*> cur_nexts;
    for(auto it = nexts.begin(); nexts.end() != it; it++){
        node* n = *it;
        if(get_bit(n->id) == 1){
            assert(cur_nexts.find(n->id) != cur_nexts.end());
        } else {
            assert(cur_nexts.find(n->id) == cur_nexts.end());
        }
    }
    //set<node*> cur_goals;
    for(auto it = goals.begin(); goals.end() != it; it++){
        node* n = *it;
        if(get_bit(n->id) == 1){
            assert(cur_goals.find(n->id) != cur_goals.end());
        } else {
            assert(cur_goals.find(n->id) == cur_goals.end());
        }
    }
}

//=========================================================

void propnet::init_dynamic(){
    prev_vals = new dynamic_bitset<>(all_nodes.size());
    vals = new dynamic_bitset<>(all_nodes.size());

    prev_vals->reset();
    vals->reset();

    for(auto it = conns.begin(); conns.end() != it; it++){
        node* n = *it;
        conn_counts.insert(pair<int,int>(n->id,0));
    }

    set_bit(const_true->id,1);
    set_bit(const_false->id,0);
}

void propnet::clear_dynamics(){
    delete prev_vals;
    delete vals;
}

void propnet::inc_count(int i){
    conn_counts[i]++;
}

void propnet::dec_count(int i){
    conn_counts[i]--;
}

int propnet::get_count(int i){
    return conn_counts[i];
}

void propnet::set_bit(int i,int v){
    vals->set(i,v);
}

int propnet::get_bit(int i){
    return vals->test(i);
}

int propnet::get_prev_bit(int i){
    return prev_vals->test(i);
}

void propnet::set_prev_bit(int i,int v){
    prev_vals->set(i,v);
}

propnet* propnet::duplicate(){
    propnet* ret = new propnet();
    ret->propagate_time = 0.0;

    ret->cur_state = cur_state;
    ret->cur_legals = cur_legals;
    ret->cur_nexts = cur_nexts;
    ret->cur_goals = cur_goals;

    ret->prev_vals = new dynamic_bitset<>(all_nodes.size());
    ret->vals = new dynamic_bitset<>(all_nodes.size());

    for(int i=0;i<all_nodes.size();i++){
        ret->set_bit(i,get_bit(i));
        ret->set_prev_bit(i,get_prev_bit(i));
    }

    ret->conn_counts = conn_counts;

    return ret;
}

//==========================================================

prop_node::prop_node(const gdl_ast* ast,node_type tp){
    prop = new gdl_ast(*ast);
    typ = tp;
}

prop_node::~prop_node(){
    delete prop;
}

action_node::action_node(const string& r,const gdl_ast* a,node_type t){
    role = r;
    action = new gdl_ast(*a);
    typ = t;
}

action_node::~action_node(){
    delete action;
}
