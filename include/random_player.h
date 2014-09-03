#ifndef __RANDOM_PLAYER_H__
#define __RANDOM_PLAYER_H__

#include "player.h"
#include "config.h"

class random_player : public player{
public:
    random_player(controller* con)
    :player(con)
    {}

    ~random_player(){}

    bool init(){ return true; }

    void meta_game(const string& game_id,const string& role,int startclock,const timer* T);
    gdl_term* next_move(int playclock,const terms& moves,const timer* T);
    mw_vec* next_move_dist(int playclock,const terms& moves,string& src,const timer* T);
    void finish_game();

private:
    const timer* T;
};

#endif
