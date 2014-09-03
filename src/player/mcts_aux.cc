#include "mcts.h"
#include <queue>
#include <sstream>
#include "mcts_struct.h"

using std::queue;
using std::stringstream;

//MCTS_PROFILE
static double make_move_time = 0.0;
static double retract_time = 0.0;
static double state_time = 0.0;
static double lookup_time = 0.0;
static double new_node_time = 0.0;

static double tot_make_move_time = 0.0;
static double tot_retract_time = 0.0;
static double tot_state_time = 0.0;
static double tot_lookup_time = 0.0;
static double tot_new_node_time = 0.0;

FILE* stat_output = NULL;
//MCTS_PROFILE

#ifdef MCTS_DEBUG
bool tictactoe_legal_state(terms* s);
#endif

//===================================================================

mcts_node* mcts_player::new_node(terms* state){
    double t = 0.0;
    if(profile){
        t = T->time_elapsed();
    }

    retract_wrapper(con,*state);
    mcts_node* ret = new mcts_node(state,this);
    ret->init(con);

    if(profile){
        new_node_time += T->time_elapsed() - t;
    }
    
    return ret;
}

mcts_node* mcts_player::lookup_node(terms* state){ // only in use when MCTS_USE_TT is defined
    double t = 0.0;
    if(profile){
        t = T->time_elapsed();
    }

    mcts_node* ret = NULL;

    state_node_type::iterator result = state_node.find(state_key(state));
    if(result == state_node.end()){
        ret = new_node(state);
    } else {
        ret = result->second;
    }

    if(profile){
        lookup_time += T->time_elapsed() - t;
    }

    return ret;
}

int mcts_player::prune_and_update(const terms& move){
    using namespace std;

    slog("mcts_player","move:\n");
    controller::print_terms(&move,stdout);
    slog("mcts_player","\n");
    
    slog("mcts_player", "pruning\n");
    slog("mcts_player", "number of nodes before:%d\n", mcts_node_count);

    vector<int> choices;
    if(!root->choice_from_move(&move,choices)){
        return 0;
    }

    mcts_node* new_root = NULL;

    if(use_TT){
        terms* s = state_wrapper(con);
        new_root = lookup_node(s);
        controller::free_terms(s);
    } else {
        new_root = root->get_child(choices);
        if(new_root == NULL){
            terms* state = state_wrapper(con);
            new_root = new_node(state);
            controller::free_terms(state);
        }
    }

    root->expand(new_root,choices);
    root = new_root;

//prune
    if(round > 0 && round % prune_freq == 0){
        double prune_st = T->time_elapsed();

        clear_ref_and_mark();
        inc_ref(new_root);
        prune();

        double prune_dur = T->time_elapsed() - prune_st;

        slog("mcts_player", "prune_dur:%lf\n", prune_dur);

        slog("mcts_player", "done pruning\n");
        slog("mcts_player", "number of nodes after:%d\n", mcts_node_count);
    }

    return 1;
}

bool mcts_player::has_time(double time_limit){
    double dur = T->time_elapsed();
    return dur + time_buffer < time_limit;
}

void mcts_player::update_time_buffer(double dur){
    if(dur + 0.5 > time_buffer){
        time_buffer = dur+0.5;
        //slog("mcts_player","updated time_buffer to %lf\n",dur+0.5);
    }
}

//===================================================================

void mcts_player::profile_init(){
    tot_make_move_time = 0;
    tot_state_time = 0;
    tot_retract_time = 0;
    tot_lookup_time = 0;
    tot_new_node_time = 0;

    stat_output = fopen(profile_output.c_str(),"a");
    fprintf(stat_output,"----------------------------------\n");
    fprintf(stat_output,"game_id:%s\n",game_id.c_str());
    fprintf(stat_output,"make_move_time\tretract_time\tstate_time\tlookup_time\tnew_node_time\tsamples\n");
    fflush(stat_output);
}

void mcts_player::profile_dealloc(){
    fprintf(stat_output,"%lf\t%lf\t%lf\t%lf\t%lf\t%d\n",tot_make_move_time,tot_retract_time,tot_state_time,tot_lookup_time,tot_new_node_time,0);
    fclose(stat_output);
}

void mcts_player::profile_round_begin(){
    make_move_time = 0;
    retract_time = 0.0;
    state_time = 0.0;
    lookup_time = 0.0;
    new_node_time = 0.0;
}

void mcts_player::profile_round_end(int count){
    tot_make_move_time += make_move_time;
    tot_state_time += state_time;
    tot_retract_time += retract_time;
    tot_lookup_time += lookup_time;
    tot_new_node_time += new_node_time;

    fprintf(stat_output,"%lf\t%lf\t%lf\t%lf\t%lf\t%d\n",make_move_time,retract_time,state_time,lookup_time,new_node_time,count);
    fflush(stat_output);
}

void mcts_player::make_move_wrapper(controller* con,const terms& m){
    double t = 0.0;
    if(profile){
        t = T->time_elapsed();
    }

    con->make_move(m);

    if(profile){
        make_move_time += T->time_elapsed() - t;
    }

}

terms* mcts_player::state_wrapper(controller* con){
    terms* ret = NULL;

    double t = 0.0;
    if(profile){
        t = T->time_elapsed();
    }

    ret = con->state();

    if(profile){
        state_time += T->time_elapsed() - t;
    }

    return ret;
}

void mcts_player::retract_wrapper(controller* con,const terms& s){
    double t = 0.0;
    if(profile){
        t = T->time_elapsed();
    }

    con->retract(s);

    if(profile){
        retract_time += T->time_elapsed() - t;
    }

}

//=============================================================

void mcts_player::clear_ref_and_mark(){
    set<mcts_node*>::iterator it = all_mcts_nodes.begin();

    while(it != all_mcts_nodes.end()){
        mcts_node* n = *it;

        n->ref = 0;
        n->mark = false;
        it++;
    }
}

void mcts_player::inc_ref(mcts_node* n){
    queue<mcts_node*> Q;
    set<mcts_node*> inQ;

    Q.push(n);
    inQ.insert(n);

    while(Q.size() > 0){
        mcts_node* cur = Q.front();
        Q.pop();
        inQ.erase(cur);

        cur->mark = true;
        cur->ref++;

        for(auto it = cur->children.begin(); it != cur->children.end(); it++){
            const mcts_node_edge& e = *it;
            mcts_node* nn = e.node;

            if(!nn->mark){
                if(inQ.find(nn) == inQ.end()){
                    Q.push(nn);
                    inQ.insert(nn);
                }
            }
        }
    }
}

void mcts_player::prune(){
    using namespace std;

    set<mcts_node*>::iterator it = all_mcts_nodes.begin();
    vector<mcts_node*> ref0;

    while(it != all_mcts_nodes.end()){
        mcts_node* n = *it;
        if(n->ref == 0)
            ref0.push_back(n);

        n->ref = 0;
        it++;
    }

    mcts_node::same_pointer_flag = true;
    for(int i=0;i<ref0.size();i++){
        all_mcts_nodes.erase(ref0[i]);

        delete ref0[i];
    }
    mcts_node::same_pointer_flag = false;
}

void mcts_player::clear_all_nodes(){
    while(all_mcts_nodes.size() > 0){
        mcts_node* n = *(all_mcts_nodes.begin());
        all_mcts_nodes.erase(n);

        delete n;
    }
}

//================================================================

void mcts_player::init_regress(){
    interm = false;

    gdl_flatten* flatten = new gdl_flatten(game); //new
    domains_type* domains = flatten->build_and_get_domain();

    pred_index goal_value("goal",2);
    unordered_set<string>* gv_dom = domains->at(goal_value);

    vector<string> roles;

    double time_limit = std::max(5.0 / gv_dom->size(),0.5);
    bool fail = false;

    //if(gv_dom->size() > 5){ //maybe we should just use intermediate goal values as heuristic?
        //slog("mcts_player","using intermediate goal values as heuristic\n");
        //interm = true;

        //goto end;
    //}

    roles = con->roles();

    for(int i=0;i<roles.size();i++){
        string role = roles[i];
        for(auto gv_it = gv_dom->begin(); gv_it != gv_dom->end(); gv_it++){
            string gv = *gv_it;

            if(gv == "0") continue;

            gdl_ast* ast = new gdl_ast();
            ast->children.push_back(new gdl_ast("goal"));
            ast->children.push_back(new gdl_ast(role.c_str()));
            ast->children.push_back(new gdl_ast(gv.c_str()));

            gdl_term* goal_term = new gdl_term(ast);

            timer* T = new timer();
            rnode* result = regress::regress_term(game,domains,goal_term,T,time_limit);
            delete T;

            if(!result){
                slog("mcts_player","failed to regress term %s\n",goal_term->convertS().c_str());
                if(gv == "100"){ //failed to regress the most important goal term
                    fail = true;

                    delete goal_term;
                    delete ast;
                    break;
                }
            } else if (result->gauge_com() > reg_com_th){
                slog("mcts_player","term %s too large\n",goal_term->convertS().c_str());
                delete result;

                if(gv == "100"){
                    fail = true;

                    delete goal_term;
                    delete ast;
                    break;
                }
            } else {
                stringstream ss;
                ss << gv;

                reg_struct rs;
                rs.i = i;
                ss >> rs.gv;
                rs.rn = result;

                regs.push_back(rs);
            }

            delete goal_term;
            delete ast;
        }
    }

    if(fail){
        use_REG = false;
        serror("mcts_player","failed to regress key goal term, not using REG\n");
        dealloc_regress();

        goto end;
    }

end:
    delete flatten;
}

double mcts_player::regress(mcts_node* node,int role_index){
    terms* state = node->state;
    if(node->terminal){
        double ret = node->gvs[role_index];
        if(ret < 0.0) ret = 0.0;

        return ret;
    }

    if(interm){
        terms* prev_state = state_wrapper(con);
        retract_wrapper(con,*state);

        vector<string> roles = con->roles();
        int gv = con->goal(roles[role_index]);
        if(gv == -1){
            gv = 0;
        }

        retract_wrapper(con,*prev_state);
        controller::free_terms(prev_state);

        return (double)gv;
    }

    double ret = 0.0;
    double sgv = 0.0;
    for(int i=0;i<regs.size();i++){
        if(regs[i].i != role_index) continue;

        ret += regs[i].gv * regs[i].rn->eval(state);
        sgv += regs[i].gv;
    }
    ret /= sgv;
    ret *= 100.0;

    return ret;
}

void mcts_player::dealloc_regress(){
    for(int i=0;i<regs.size();i++){
        delete regs[i].rn;
    }
    regs.clear();
}

//==============================================================


void mcts_player::init_MAST(){
    vector<string> roles = con->roles();

    for(int i=0;i<roles.size();i++){
        terms* input_ri = con->get_inputs(roles[i]);

        if(input_ri->size() == 0){
            use_MAST = false;
            serror("mcts_player","no input clauses in gdl files, failed to initialize MAST\n");

            controller::free_terms(input_ri);

            clear_MAST();
            break;
        }

        unordered_map<pred_wrapper,Qha_struct* >* uset = new unordered_map<pred_wrapper,Qha_struct* >();
        for(int j=0;j<input_ri->size();j++){
            Qha_struct* Qs = new Qha_struct();

            gdl_term* t = input_ri->at(j);
            pred_wrapper pw(new gdl_term(*t));

            uset->insert(pair<pred_wrapper,Qha_struct*>(pw,Qs));
        }
        Qha.push_back(uset);

        controller::free_terms(input_ri);
    }
}

void mcts_player::update_MAST(const vector<double>& outcome,const vector<mcts_stack_node>& trace,rave_set& playout_moves){
    for(int ti=0;ti<trace.size();ti++){
        terms* move = trace[ti].node->move_from_choice(trace[ti].choice);

        for(int i=0;i<n_roles;i++){
            gdl_term* t = move->at(i);
            pred_wrapper pw(new gdl_term(*t));

            if(playout_moves[i]->find(pw) == playout_moves[i]->end()){
                playout_moves[i]->insert(pw);
            } else {
                delete pw.t;
            }
        }

        controller::free_terms(move);
    }
    
    for(int i=0;i<n_roles;i++){
        for(auto it = playout_moves[i]->begin(); it != playout_moves[i]->end(); it++){
            gdl_term* t = it->t;
            pred_wrapper pw(t);

            auto it2 = Qha[i]->find(pw);
            Qha_struct* qs = it2->second;
            qs->Q = (qs->Q*qs->N + outcome[i])/(qs->N + 1.0);
            qs->N++;
        }
    }
}

static double MAST_curve(double x,double sigma){
    return exp(x/(100.0*sigma));
}

double mcts_player::get_MAST_weight(int i, gdl_term* move){
    pred_wrapper pw(move);
    auto it = Qha[i]->find(pw);
    Qha_struct* qs = it->second;

    double Q = qs->Q;

    return MAST_curve(Q,MAST_sigma);
}

void mcts_player::clear_MAST(){
    for(int i=0;i<Qha.size();i++){
        unordered_map<pred_wrapper,Qha_struct* >* uset = Qha[i];

        for(auto it = uset->begin(); uset->end() != it; it++){
            delete it->first.t;
            delete it->second;
        }
        delete uset;
    }
    Qha.clear();
}

void mcts_player::clear_single_pstack(sp_stack_type& sp){
    for(int i=0;i<sp.size();i++){
        controller::free_terms(sp[i].first); //state
        controller::free_terms(sp[i].second); //move
    }

    sp.clear();
}

void mcts_player::init_rave_set(rave_set& rv_set){
    for(int i=0;i<n_roles;i++){
        rv_set.push_back(new unordered_set<pred_wrapper>());
    }
}

void mcts_player::free_rave_set(rave_set& rv_set){
    for(int i=0;i<rv_set.size();i++){
        unordered_set<pred_wrapper>* uset = rv_set[i];
        while(uset->size() > 0){
            auto it = uset->begin();
            pred_wrapper pw = *it;

            uset->erase(it);
            delete pw.t;
        }
        delete uset;
    }

    rv_set.clear();
}

//===============================================================

#ifdef MCTS_DEBUG
bool tictactoe_legal_state(terms* state){
    //controller::print_terms(state,stdout);

    if(state->size() != 10) return false;

    bool xcontrol;
    int xpieces,opieces,bpieces;
    int ncontrol;

    bpieces = xpieces = opieces = ncontrol = 0;
    for(int i=0;i<state->size();i++){
        gdl_term* t = state->at(i);
        pred_form pf(t);
        gdl_ast* p = t->ast;

        if(pf.pred == "control"){
            ncontrol++;
            if(!strcmp(p->children[1]->sym(),"xplayer")){
                xcontrol = true;
            } else if(!strcmp(p->children[1]->sym(),"oplayer")){
                xcontrol = false;
            } else {
                assert(false);
            }
        } else {
            if(!strcmp(p->children[3]->sym(),"x")){
                xpieces++;
            } else if(!strcmp(p->children[3]->sym(),"o")){
                opieces++;
            } else if(!strcmp(p->children[3]->sym(),"b")){
                bpieces++;
            } else {
                assert(false);
            }
        }
    }

    assert(xpieces + opieces + bpieces == 9);
    assert(ncontrol == 1);

    if(xcontrol){
        assert(xpieces == opieces);
    } else {
        assert(xpieces == opieces+1);
    }

    return true;
}
#endif
