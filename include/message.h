#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <string>
#include "game.h"
#include "controller.h"
#include "config.h"

typedef enum{
    START=1,
    PLAY,
    INFO,
    PREVIEW,
    STOP,
    ABORT
} message_type;

class message{
public:
    virtual ~message(){}
    virtual message_type type() = 0;

    string game_id;
};

class start_message : public message{
public:
    message_type type(){ return START; }

    start_message(gdl* g,int sc,int pc,const string& r);

    ~start_message(){}

    gdl* game; //no need to free here, shared with the agent class
    int startclock, playclock;
    string role;

    vector<string> roles;
};

class play_message : public message{
public:
    message_type type(){ return PLAY; }

    play_message(const gdl_ast&);
    ~play_message();

    terms move;
};

class info_message : public message{
public:
    message_type type(){ return INFO; }
};

class preview_message : public message{
public:
    message_type type(){ return PREVIEW; }
};

class stop_message : public message{
public:
    message_type type(){ return STOP; }

    stop_message(const gdl_ast&);
    ~stop_message();

    terms move;
};

class abort_message : public message{
public:
    message_type type(){ return ABORT; }
};

#endif
