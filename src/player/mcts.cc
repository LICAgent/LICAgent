#include "mcts.h"
#include <math.h>
#include <dlib/rand.h>

#ifdef MCTS_COLLECT_ERROR
bool err1_flag;
FILE* err_output = NULL;
#endif


//=========================================================

void mcts_player::meta_game(const string &game_id,const string& role,int startclock,const timer* T){
    using namespace std;

    this->T = T;

    this->game_id = game_id;
    this->role = role;

    // finding role_index
    vector<string> all_roles = con->roles();
    role_index = -1;
    for(int i=0;i<all_roles.size();i++){
        if(role == all_roles[i]){
            role_index = i;
            break;
        }
    }
    n_roles = all_roles.size();

    if(use_TT){
        if(use_zobrist){
            //try to init zobrist hashing
            terms* bases = con->get_bases();
            state_key::init_zobrist(bases);
            controller::free_terms(bases);
        } else {
            state_key::use_zobrist_hash = false;
        }

        state_node.reserve(10000); //pre-alloc buckets
    }

    if(use_REG){
        init_regress();
    }
    if(use_MAST){
        init_MAST();
    }

    slog("mcts_player","use_multi: %d[bool]\n",use_multi);

    //initialize root
    slog("mcts_player","initializing roots\n");
    terms* state = state_wrapper(con);
    root = new_node(state);
    controller::free_terms(state);

#ifdef MCTS_COLLECT_ERROR
    err1_flag = false;
    err_output = fopen("err.txt","a");
#endif

    single_best_result = 0.0;

    slog("mcts_player","Cp=%lf\n",Cp);

    // sample till we run out of time
    slog("mcts_player","start sampling\n");
    round = 0;
    playtime = startclock;

    if(use_multi){
        slog("mcts_player","disabling profile for multi-thread case\n");
        profile = false;
    }

    if(profile){
        profile_init();
        profile_round_begin();

        if(use_multi){
            propnet* pcon = (propnet*)con;
            pcon->begin_bench();
        }
    }

    if(use_multi){
        multi_thread_init();
    }

    double st = T->time_elapsed();
    meta_samples = sample(root,startclock);
    meta_dur = T->time_elapsed() - st;

    if(profile){
        profile_round_end(meta_samples);

        if(use_multi){
            propnet* pcon = (propnet*)con;
            pcon->end_bench();
        }
    }

    slog("mcts_player","done sampling\n");
}

void mcts_player::do_search(int playclock,const terms& moves,const timer* T){
    using namespace std;

    this->T = T;

    slog("mcts_player","in next move\n");

    if(moves.size() > 0){
        if(!prune_and_update(moves)){ //out of sync
            return;
        }
    }

    if(profile){
        profile_round_begin();

        if(use_multi){
            propnet* pcon = (propnet*)con;
            pcon->begin_bench();
        }
    }

    playtime = playclock;
    round++;
    int count = sample(root,playclock);

    if(profile){
        profile_round_end(count);

        if(use_multi){
            propnet* pcon = (propnet*)con;
            pcon->end_bench();
        }
    }
}

gdl_term* mcts_player::next_move(int playclock,const terms& moves,const timer* T){
    do_search(playclock,moves,T);

    return root->choose_best(role_index);
}

mw_vec* mcts_node::get_mws(int i){
    mw_vec* ret = new mw_vec();

    for(int j=0;j<edges[i].size();j++){
        gdl_term* move = get_move(i,j);
        
        double Q = 0.0;
        if(edges.size()== 1){
            Q = get_best(i,j);
        } else {
            Q = get_Q_(i,j);
        }

        double w = get_G(i,j) * play->dist_weight;

        move_weight* mw = new move_weight(move,w,Q);

        ret->push_back(mw);

        fprintf(stderr,"(mv): move=%s, w=%lf, Q=%lf\n",move->convertS().c_str(),w,Q);
    }

    return ret;
}

mw_vec* mcts_player::next_move_dist(int playclock,const terms& moves,string& src,const timer* T){
    do_search(playclock,moves,T);

    if(n_roles > 1){
        src = "mcts";
    } else {
        src = "single";
    }

    return root->get_mws(role_index);
}

void mcts_player::finish_game(){
    int gv = con->goal(con->roles()[role_index]);

    slog("mcts_player","final goal value:%d\n",gv);

    clear_all_nodes();
    root = NULL;

    if(use_zobrist){
        state_key::dealloc_zobrist();
    }

    if(profile){
        profile_dealloc();
    }

    dealloc_regress();
    clear_MAST();

    if(use_multi){
        multi_thread_dealloc();
    }

#ifdef MCTS_COLLECT_ERROR
    fclose(err_output);
#endif
}

//=========================================================

int mcts_player::sample(mcts_node* node,double time_limit){
    using namespace std;

    int count = 0;
    time_buffer = mcts_time_buffer;

    slog("mcts_player","time_limit=%lf, round=%d\n",time_limit, round);

    double start_time = T->time_elapsed();

#ifdef MCTS_DEBUG
    slog("mcts_player","start sampling state:\n");
    controller::print_terms(node->state,stdout);
#endif

    while(has_time(time_limit)){
        double t1 = T->time_elapsed();

        vector<mcts_stack_node> trace;
        mcts_node* n = tree_policy(node,trace);
        if(use_multi){
            multi_thread_run(n,trace);
        } else {
            single_thread_run(n,trace);
        }

        double dur = T->time_elapsed() - t1;

#ifdef MCTS_DEBUG
        slog("mcts_player","dur=%lf, count=%d. time_elapsed=%lf, time_buffer=%lf\n", dur, count, T->time_elapsed(),time_buffer);
#endif

        if(use_multi){
            count += mcts_num_threads;
        } else {
            count++;
        }

        update_time_buffer(dur);
    }

    retract_wrapper(con,*(node->state));

    double dur = T->time_elapsed() - start_time;
    slog("mcts_player","simulated %d samples, node count=%d, duration of sample=%lf, total time_elapsed=%lf\n",count, mcts_node_count, dur, T->time_elapsed());
    if(n_roles == 1){
        slog("mcts_player","single_best_result=%lf\n",single_best_result);
    }

    return count;
}

//=========================================================

void mcts_player::init_params(){
    configure::get_option("MCTS_DPmax",&DP_max);
    configure::get_option("MCTS_USE_TT",&use_TT);
    configure::get_option("MCTS_USE_ZOBRIST",&use_zobrist);
    configure::get_option("MCTS_PROFILE",&profile);
    configure::get_option("MCTS_PROFILE_OUTPUT",&profile_output);
    configure::get_option("MCTS_LIMIT_NODE",&limit_node);
    configure::get_option("MCTS_NODE_MAX_COUNT",&node_max_count);

    configure::get_option("MCTS_TIME_BUFFER",&mcts_time_buffer);
    configure::get_option("MCTS_GAMMA",&mcts_gamma);

    configure::get_option("MCTS_PRUNE_FREQ",&prune_freq);

    //in mcts_node
    configure::get_option("MCTS_USE_UCB_TUNE",&use_UCB_TUNE);

    configure::get_option("MCTS_USE_RAVE",&use_RAVE);
    configure::get_option("MCTS_RAVE_THRESHOLD",&rave_V);
    configure::get_option("MCTS_RAVE_WEIGHT",&rave_w);
    
    configure::get_option("MCTS_USE_REGRESS",&use_REG);
    configure::get_option("MCTS_REG_THRESHOLD",&reg_V);
    configure::get_option("MCTS_REG_WEIGHT",&reg_w);
    configure::get_option("MCTS_REG_COM_THRESHOLD",&reg_com_th);

    configure::get_option("MCTS_USE_MAST",&use_MAST);
    configure::get_option("MCTS_MAST_SIGMA",&MAST_sigma);

    configure::get_option("MCTS_DIST_WEIGHT",&dist_weight);

    configure::get_option("MCTS_NUM_THREADS",&mcts_num_threads);

    slog("mcts_player","options:\n");
    slog("mcts_player","MCTS_DPmax: %lf\n",DP_max);
    slog("mcts_player","MCTS_USE_TT: %d[bool]\n",use_TT);
    slog("mcts_player","MCTS_USE_ZOBRIST: %d[bool]\n",use_zobrist);
    slog("mcts_player","MCTS_PROFILE: %d[bool]\n",profile);
    slog("mcts_player","MCTS_PROFILE_OUTPUT: %s\n",profile_output.c_str());
    slog("mcts_player","MCTS_LIMIT_NODE: %d[bool]\n",limit_node);
    slog("mcts_player","MCTS_NODE_MAX_COUNT: %d\n",node_max_count);

    slog("mcts_player","MCTS_PRUNE_FREQ: %d\n",prune_freq);

    slog("mcts_player","MCTS_TIME_BUFFER: %lf\n",mcts_time_buffer);
    slog("mcts_player","MCTS_GAMMA: %lf\n",mcts_gamma);

    slog("mcts_node","MCTS_USE_UCB_TUNE: %d[bool]\n",use_UCB_TUNE);

    slog("mcts_player","MCTS_USE_RAVE: %d[bool]\n",use_RAVE);
    slog("mcts_player","MCTS_RAVE_THRESHOLD:%d\n",rave_V);
    slog("mcts_player","MCTS_RAVE_WEIGHT:%lf\n",rave_w);
    
    slog("mcts_player","MCTS_USE_REGRESS: %d[bool]\n",use_REG);
    slog("mcts_player","MCTS_REG_THRESHOLD:%d\n",reg_V);
    slog("mcts_player","MCTS_REG_WEIGHT:%lf\n",reg_w);
    slog("mcts_player","MCTS_REG_COM_THRESHOLD:%d\n",reg_com_th);

    slog("mcts_player","MCTS_USE_MAST:%d[bool]\n",use_MAST);
    slog("mcts_player","MCTS_MAST_SIGMA:%lf\n",MAST_sigma);

    slog("mcts_player","MCTS_DIST_WEIGHT:%lf\n",dist_weight);

    slog("mcts_player","MCTS_NUM_THREADS:%d\n",mcts_num_threads);
}

bool mcts_player::init(){
    //default values;
    DP_max = 0.2;

    use_TT = true;
    use_zobrist = false;

    profile = false;
    profile_output = "";

    limit_node = false;
    node_max_count = 3000000;

    prune_freq = 1;
    
    use_UCB_TUNE = true;

    mcts_gamma = 1.0;
    mcts_time_buffer = 3;

    use_RAVE = false;
    rave_V = 50;
    rave_w = 0.5;
    
    use_REG = false;
    reg_V = 50;
    reg_w = 0.5;
    reg_com_th = 130;

    use_MAST = false;
    MAST_sigma = 3.0;
    mcts_num_threads = 2;

    init_params();

    mcts_node_count = 0;

    return true;
}
//=======================================================

