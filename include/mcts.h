#ifndef __MCTS_H__
#define __MCTS_H__

#include "player.h"
#include "mcts_struct.h"
#include "slog.h"
#include "config.h"
#include "regress.h"
#include "propnet.h"
#include <pthread.h>

typedef vector<pair<terms*,terms*>  > sp_stack_type;

void* simulation_worker_thread(void* v);

class mcts_player : public player{
public:
    mcts_player(controller* con,gdl* g,double _Cp,bool multi = false)
    :player(con)
    ,root(NULL)
    ,Cp(_Cp)
    ,game(g)
    ,use_multi(multi)
    { }

    ~mcts_player(){
        clear_all_nodes();
    }

    void init_params();

    bool init();

    void meta_game(const string& game_id,const string& role,int startclock,const timer* T);
    gdl_term* next_move(int playclock,const terms& moves,const timer* T);
    mw_vec* next_move_dist(int playclock,const terms& moves,string& src,const timer* T);

    void finish_game();

    state_node_type state_node;

//retrieved from configure file
    double DP_max;

    bool use_TT;
    bool use_zobrist;

    bool profile;
    string profile_output;

    bool limit_node;
    int node_max_count;

    int prune_freq;

    bool use_UCB_TUNE;

    double mcts_gamma;
    double mcts_time_buffer;

    bool use_RAVE;
    int rave_V;
    double rave_w;
    
    bool use_REG;
    int reg_V;
    double reg_w;
    int reg_com_th;

    bool use_MAST;
    
    double dist_weight;

    int mcts_num_threads;

    double regress(mcts_node* state,int role_index);

//mcts_node related
    int mcts_node_count;
    set<mcts_node*> all_mcts_nodes;

    void clear_ref_and_mark();
    void inc_ref(mcts_node* n);
    void prune();
    void clear_all_nodes();

private:
//game tree related
    mcts_node* new_node(terms* state);
    mcts_node* lookup_node(terms* s);

    int prune_and_update(const terms&);

    double Cp;
    mcts_node* root;

//regress
    bool interm;

    struct reg_struct{
        int i;
        int gv;
        rnode* rn;
    };

    void init_regress();
    void dealloc_regress();
    vector<reg_struct> regs;

//time management
    bool has_time(double time_limit);
    void update_time_buffer(double dur);

    double time_buffer;
    const timer* T;

//entry point
    int sample(mcts_node* root,double time_limit);

//main mcts procedures
    void do_search(int playclock,const terms& moves,const timer* T);
    terms* choose_default_action(vector<terms*>* all_moves);

    mcts_node* expand(mcts_node* n,vector<int>& choice);
    mcts_node* tree_policy(mcts_node* n,vector<mcts_stack_node>& stack);
    int default_policy(controller* con,const mcts_node* n,vector<double>* outcome, sp_stack_type* sp, rave_set* rv);
    void backup(const vector<double>& outcome,const vector<mcts_stack_node>& cstack, rave_set& rv);

//threads

    void single_thread_run(mcts_node* n,vector<mcts_stack_node>& stack);

    pthread_t* threads;

    pthread_cond_t pins_cond;
    pthread_mutex_t pins_lock;
    int worker_pins;

    pthread_barrier_t wait_other;

    pthread_cond_t worker_cond;
    pthread_mutex_t worker_lock;
    const mcts_node* thread_start_node;
    int thread_terminal;

    struct thread_struct{
        mcts_player* play;
        controller* con;
        int index;

        vector<double>* outcome;
        rave_set* rv;
        sp_stack_type* pstack;
    };
    vector<thread_struct*> tss;

    void multi_thread_init();
    void multi_thread_run(mcts_node* n,const vector<mcts_stack_node>& stack);
    void multi_thread_dealloc();

//single-player enhancement
    double single_best_result;

    void single_extend(const sp_stack_type& pstack,vector<mcts_stack_node>& stack);
    void clear_single_pstack(sp_stack_type& sp);

//book keeping
    int playtime;
    int round;

//used to measure sample rate
    int meta_samples;
    double meta_dur;

//RAVE
    void init_rave_set(rave_set& rv_set);
    void free_rave_set(rave_set& rv_set);

//for MAST
    struct Qha_struct{
        Qha_struct()
        :Q(100.0)
        ,N(0)
        {}

        double Q;
        int N;
    };

    vector<unordered_map<pred_wrapper,Qha_struct* >* > Qha;
    double MAST_sigma;

    void init_MAST();
    void update_MAST(const vector<double>& outcome,const vector<mcts_stack_node>& trace,rave_set& playout_moves);
    double get_MAST_weight(int i, gdl_term* move);
    void clear_MAST();

//profile
    void profile_init();
    void profile_dealloc();
    void profile_round_begin();
    void profile_round_end(int count);

    void make_move_wrapper(controller* con,const terms& m);
    terms* state_wrapper(controller* con);
    void retract_wrapper(controller* con,const terms& s);

    int role_index;
    int n_roles;

    bool use_multi;

    gdl* game;

    friend void* simulation_worker_thread(void* v);
};

#endif
