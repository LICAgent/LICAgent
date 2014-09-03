#ifndef __PLAYER_H__
#define __PLAYER_H__

#include "agent.h"
#include "game.h"
#include "config.h"
#include "common.h"

class agent;

class player{
public:
    player(controller* c)
    :con(c)
    {}

    virtual ~player(){}

    virtual bool init() = 0;

    virtual void meta_game(const string& game_id,const string& role,int startclock,const timer* T) = 0;
    virtual gdl_term* next_move(int playclock,const terms& moves,const timer* T) = 0;
    virtual mw_vec* next_move_dist(int playclock,const terms& moves,string& src,const timer* T) = 0;

    virtual void finish_game() = 0;

    controller* con;
    string role;
    string game_id;
};

#endif
