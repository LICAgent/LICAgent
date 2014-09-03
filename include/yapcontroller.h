#ifndef __YAPCONTROLLER_H__
#define __YAPCONTROLLER_H__

#include "controller.h"
#include "config.h"

class yapcontroller : public controller{
public:
    yapcontroller(gdl* game,const char* ext,const char* pl,const char* compiled);
    ~yapcontroller();

    bool init();

    bool terminal();
    int goal(const string& role);
    bool legal(const string& role, const gdl_term& move);

    vector<string> roles(){ return _roles; }
    terms* state();
    terms* get_move(const string& role);

    int make_move(const terms&);
    int retract(const terms&);

    terms* get_bases();
    terms* get_inputs(const string& role);

    int prove_init_state();

    string _compiled;
    gdl* _game;
    vector<string> _roles;
};

#endif
