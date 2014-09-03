#include "mcts.h"

void mcts_player::single_thread_run(mcts_node* n,vector<mcts_stack_node>& stack){
    vector<double> outcome;

    rave_set playout_moves; //only used in single_thread_run
    sp_stack_type single_pstack; //only used in single-threade_run

    init_rave_set(playout_moves);

    default_policy(con,n,&outcome,&single_pstack,&playout_moves);

    if(n_roles == 1){
        if(outcome[0] > single_best_result){
            single_best_result = outcome[0];
            single_extend(single_pstack,stack);
        }

        clear_single_pstack(single_pstack);
    }
    backup(outcome,stack,playout_moves);

    free_rave_set(playout_moves);
}

void* simulation_worker_thread(void* v){
    mcts_player::thread_struct* ts = (mcts_player::thread_struct*)v;
    mcts_player* play = ts->play;
    controller* con = ts->con;
    int index = ts->index;

    while(1){
        bool to_end = false;

        pthread_mutex_lock(&play->worker_lock);
        while(1){
            if(play->thread_terminal){
                to_end = true;
                break;
            } else if(play->thread_start_node != NULL){
                break;
            }

            //test
            //fprintf(stderr,"worker[%d]: waiting for worker_lock\n",index);
            pthread_cond_wait(&play->worker_cond,&play->worker_lock);
        }
        //test
        //fprintf(stderr,"worker[%d], worker_pins=%d\n",index,play->worker_pins);
        pthread_mutex_unlock(&play->worker_lock);


        if(to_end){
            break;
        }

        //finally, run simulation

        //test
        //fprintf(stderr,"worker[%d]: starting simulation\n",index);
        play->default_policy(con,play->thread_start_node,ts->outcome,ts->pstack,ts->rv);
        //test
        //fprintf(stderr,"worker[%d]: simulation finished\n",index);

        pthread_mutex_lock(&play->pins_lock);
        play->worker_pins++;
        if(play->worker_pins == play->mcts_num_threads){
            //test
            //fprintf(stderr,"worker[%d]: all simulation finished\n",index);

            play->thread_start_node = NULL;
            pthread_cond_signal(&play->pins_cond);
        }
        pthread_mutex_unlock(&play->pins_lock);

        pthread_barrier_wait(&play->wait_other);
    }

    pthread_exit(NULL);
    return NULL;
}

void mcts_player::multi_thread_init(){
    pthread_mutex_init(&worker_lock,NULL);
    pthread_cond_init(&worker_cond,NULL);

    pthread_mutex_init(&pins_lock,NULL);
    pthread_cond_init(&pins_cond,NULL);

    pthread_barrier_init(&wait_other,NULL,mcts_num_threads);

    worker_pins = 0;
    thread_terminal = 0;
    thread_start_node = NULL;

    //start worker threads
    threads = (pthread_t*)malloc(sizeof(pthread_t)*mcts_num_threads);
    for(int i=0;i<mcts_num_threads;i++){
        thread_struct* ts = new thread_struct();
        ts->index = i;
        ts->play = this;
        if(i == 0){
            ts->con = this->con;
        } else {
            ts->con = ((propnet*)this->con)->duplicate();
        }
        ts->rv = NULL;
        ts->outcome = NULL;
        ts->pstack = NULL;

        tss.push_back(ts);

        pthread_create(&threads[i],NULL,simulation_worker_thread,(void*)ts);
    }
}

void mcts_player::multi_thread_dealloc(){
    //stop worker threads
    pthread_mutex_lock(&worker_lock);
    thread_terminal = 1;
    pthread_cond_broadcast(&worker_cond);
    pthread_mutex_unlock(&worker_lock);
    for(int i=0;i<mcts_num_threads;i++){
        pthread_join(threads[i],NULL);
    }

    free(threads);
    pthread_cond_destroy(&worker_cond);
    pthread_mutex_destroy(&worker_lock);
    pthread_cond_destroy(&pins_cond);
    pthread_mutex_destroy(&pins_lock);

    pthread_barrier_destroy(&wait_other);

    for(int i=0;i<tss.size();i++){
        if(i != 0) delete tss[i]->con;
        delete tss[i];
    }
}

void mcts_player::multi_thread_run(mcts_node* n,const vector<mcts_stack_node>& stack){
    pthread_mutex_lock(&worker_lock);

    thread_start_node = n;
//init ts
    for(int i=0;i<tss.size();i++){
        thread_struct* ts = tss[i];

        ts->outcome = new vector<double>();
        ts->pstack = new sp_stack_type();
        ts->rv = new rave_set();

        init_rave_set(*ts->rv);
    }
    worker_pins = 0;

    pthread_cond_broadcast(&worker_cond);
    pthread_mutex_unlock(&worker_lock);

    //test
    //fprintf(stderr,"player: waiting for all threads to finish\n");

//wait for all worker threads to finish
    pthread_mutex_lock(&pins_lock);
    while(worker_pins < mcts_num_threads){
        pthread_cond_wait(&pins_cond,&pins_lock);
    }
    thread_start_node = NULL;
    pthread_mutex_unlock(&pins_lock);

    //test
    //fprintf(stderr,"player: about to synthesize simulation results\n");

//synthesize simulation results
    for(int i=0;i<tss.size();i++){
        thread_struct* ts = tss[i];

        if(n_roles == 1 && ts->outcome->at(0) > single_best_result){
            single_best_result = ts->outcome->at(0);

            vector<mcts_stack_node> stack_copy = stack;
            single_extend(*ts->pstack,stack_copy);

            backup(*ts->outcome,stack_copy,*ts->rv);
        } else {
            backup(*ts->outcome,stack,*ts->rv);
        }

        clear_single_pstack(*ts->pstack);
        free_rave_set(*ts->rv);
        delete ts->pstack;
        delete ts->rv;
        delete ts->outcome;

        ts->rv = NULL;
        ts->outcome = NULL;
        ts->pstack = NULL;
    }
}
