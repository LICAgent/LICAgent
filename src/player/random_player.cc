#include "random_player.h"

void random_player::meta_game(const string& game_id,const string& role,int startclock,const timer* T){
    this->role = role;
    this->game_id = game_id;
}

gdl_term* random_player::next_move(int playclock,const terms& moves,const timer* T){
    this->T = T;

    terms* move = con->get_move(role);

    //printf("state:\n");
    //terms* state = con->state();
    //controller::print_terms(state,stdout);
    //controller::free_terms(state);
    //printf("\n\n\n");

    //printf("moves:\n");
    //controller::print_terms(move,stdout);
    //printf("\n\n\n");

    int i = rand() % move->size();
    gdl_term* ret = new gdl_term(*((*move)[i]));

    controller::free_terms(move);
    return ret;
}

mw_vec* random_player::next_move_dist(int playclock,const terms& moves,string& src,const timer* T){
    this->T = T;

    terms* move = con->get_move(role);
    mw_vec* ret = new mw_vec();
    for(int i=0;i<move->size();i++){
        ret->push_back(new move_weight(move->at(i),1.0,50.0)); //the neutral guess
    }

    controller::free_terms(move);

    src = "mcts";
    return ret;
}

void random_player::finish_game(){}
