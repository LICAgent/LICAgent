#ifndef __AGENT_H__
#define __AGENT_H__

#include <string>
#include <stdint.h>
#include "game.h"
#include "controller.h"
#include "message.h"
#include "parser.h"
#include "config.h"
#include "common.h"

using std::string;

typedef enum{
    PLAYING = 1,
    IDLE
} agent_status;

class player;

class agent{
public:
    agent()
    :game_id("")
    ,play(NULL)
    ,game(NULL)
    ,con(NULL)
    ,status(IDLE)
    {
    }

    ~agent(){
        reset();
    }

    string handle_request(const string& req,bool dist);

private:
    string handle_message(message* msg,timer* T,bool dist);

    player* select_player(start_message* msg,timer* T);
    controller* select_controller(start_message* msg,timer* T);

    bool init_game(start_message* msg,timer* T);
    void reset();

    int playclock,startclock;
    string game_id,role;
    player* play;
    gdl* game;
    controller* con;

    bool use_propnet;

    agent_status status;
};

#endif
