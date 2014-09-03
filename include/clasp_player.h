#ifndef __CLASP_PLAYER_H__
#define __CLASP_PLAYER_H__

#include "player.h"

class clasp_player: public player{
public:
    clasp_player(controller* con,gdl* g, const char* _fn,double tl,const timer* _T);
    ~clasp_player();

    bool init();

    void meta_game(const string& game_id,const string& role,int startclock,const timer* T);
    gdl_term* next_move(int playclock,const terms& moves,const timer* T);
    void finish_game();

    mw_vec* next_move_dist(int playclock,const terms& moves,string& src,const timer* T);

private:
    const timer* T;
    vector<gdl_term*> plan;

    int round;

    gdl* game;
    const char* fn;
    double time_limit;
};

#endif
