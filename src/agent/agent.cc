#include "agent.h"
#include "random_player.h"
#include "mcts.h"
#include "clasp_player.h"
#include "yapcontroller.h"
#include "propnet.h"
#include <sys/time.h>
#include <sstream>
#include <iostream>
#include <stdlib.h>
#include "common.h"

using std::stringstream;

player* agent::select_player(start_message* msg,timer* T){
    bool try_clasp_player = false;
    configure::get_option("TRY_CLASP_PLAYER",&try_clasp_player);
    slog("agent","TRY_CLASP_PLAYER:%d\n",try_clasp_player);

    if(msg->roles.size() == 1 && try_clasp_player){
        string cl_path;
        configure::get_option("CLASP_FILE_DIR",&cl_path);
        slog("agent","CLASP_FILE_DIR:%s\n",cl_path.c_str());

        cl_path += msg->game_id;
        cl_path += ".lp";

        double tl = 3.0;
        configure::get_option("CLASP_TIME_LIMIT",&tl);
        slog("agent","CLASP_TIME_LIMIT:%lf\n",tl);

        timer* T2 = new timer();

        clasp_player* ret = new clasp_player(con,msg->game,cl_path.c_str(),tl,T2);
        if(ret && ret->init()){
            delete T2;
            return ret;
        }

        slog("agent","failed to init clasp_player\n\n");

        if(ret){
            delete ret;
        }
        delete T2;
    }

    double Cp;
    bool multi_thread;
    configure::get_option("MCTS_CP",&Cp);
    configure::get_option("PROPNET_MULTITHREAD",&multi_thread);
    slog("agent","MCTS_CP:%lf\n",Cp);
    slog("agent","use_propnet:%d[bool]\n",use_propnet);
    slog("agent","multi_thread:%d[bool]\n",multi_thread);

    player* ret = new mcts_player(con,game,Cp,use_propnet && multi_thread);
    if(ret && ret->init()){
        return ret;
    }

    if(ret)
        delete ret;
    return NULL;
}

const char* ext = "ggp.extensions.pl";
const char* path = "./games/";

controller* agent::select_controller(start_message* msg,timer* T){
    using namespace std;
    controller* ret = NULL;

    use_propnet = false;
    
    stringstream spath;
    spath << path << wall_time() / 1000 << ".";

    string pl = spath.str();
    pl += msg->game_id + ".pl";
    string compiled = spath.str();
    compiled += msg->game_id + ".ggp";
    string kif_file = spath.str();
    kif_file += msg->game_id + ".kif";

    int timeout = 10;
    configure::get_option("PROPNET_FLATTEN_TIMEOUT",&timeout);
    slog("agent","PROPNET_FLATTEN_TIMEOUT:%d\n",timeout);

#ifdef AGENT_USE_PROPNET
    if(msg->startclock >= timeout*2){
#else
    if(false){
#endif
        slog("agent","trying to construct propnet\n");

        FILE* fkif = fopen(kif_file.c_str(),"w"); //dump game description to file
        msg->game->printS(fkif);
        fclose(fkif);

        propnet_factory* propfac = new propnet_factory(game,timeout);
        ret = propfac->create_propnet(kif_file.c_str());
        if(ret && ret->init()){
            use_propnet = true;
            delete propfac;

            return ret;
        }

        if(ret){
            delete ret;
        }
        delete propfac;

        slog("agent","failed to construct propnet\n");
    }

    FILE* fpl = fopen(pl.c_str(),"w");
    msg->game->print(fpl);
    fclose(fpl);
    ret = new yapcontroller(msg->game,ext,pl.c_str(),compiled.c_str());

    slog("agent","yap controller initialized\n");

    if(ret && ret->init()){
        return ret;
    }

    if(ret)
        delete ret;
    return NULL;
}

string agent::handle_request(const string& req,bool dist){
    timer* T = new timer();

    message* msg = parser::parse_message(req);
    string ret = handle_message(msg,T,dist);
    
    delete T;
    delete msg;
    return ret;
}

string agent::handle_message(message* msg, timer* T, bool dist){
    using namespace std;

    string ret = "";

    switch(msg->type()){
        case INFO: //handled by agent_state now
        {
            ret = "";
            break;
        }
        case START:
        {
            if(status != IDLE){
                serror("agent","ignoring start message, agent not idle\n");
                break;
            }

            start_message* cmsg = (start_message*)msg;
            if(!init_game(cmsg,T)){
                reset();
                break;
            }

            slog("agent","meta gaming\n");

            play->meta_game(cmsg->game_id,cmsg->role,startclock,T);
            ret = "ready";

            break;
        }
        case PLAY:
        {
            if(status != PLAYING){
                serror("agent","ignoring play message, agent not playing\n");
                break;
            }

            play_message* cmsg = (play_message*)msg;
            if(cmsg->move.size() > 0){
                con->make_move(cmsg->move);
            }

            slog("agent","cmsg->move:\n");
            controller::print_terms(&cmsg->move,stdout);
            slog("agent","\n\n");

            if(!dist){
                gdl_term* move = play->next_move(playclock,cmsg->move,T);
                ret = move->convertS();
                delete move;
            } else {
                string src;
                mw_vec* mws = play->next_move_dist(playclock,cmsg->move,src,T);
                ret = move_weight::convertS(mws,src);

                move_weight::free_vec(mws);
            }
            break;
        }
        case PREVIEW:
        {
            serror("agent","ignoring preview message, not supported yet\n");
            break;
        }
        case STOP:
        {
            if(status != PLAYING){
                serror("agent","ignoring stop message, agent not playing\n");
                break;
            }

            stop_message* cmsg = (stop_message*)msg;
            con->make_move(cmsg->move); // the final state

            play->finish_game();
            reset();

            ret = "done";
            break;
        }
        case ABORT:
        {
            if(status != PLAYING){
                serror("agent","ignoring abort message, agent not playing\n");
                break;
            }

            play->finish_game();
            reset();

            ret = "aborted";
            break;
        }
    }

    return ret;
}

bool agent::init_game(start_message* msg,timer* T){
    configure::initialize(CONFIG_FILE);
    slog_init_params();

    playclock = msg->playclock;
    startclock = msg->startclock;
    game = msg->game;
    status = PLAYING;
    role = msg->role;

    con = select_controller(msg,T);
    if(con == NULL){ //error in initializing controller
        return false;
    }

    play = select_player(msg,T);
    if(play == NULL){ //error in initializing player
        return false;
    }

    srand(time(NULL));

    return true;
}

void agent::reset(){
    if(play)
        delete play;
    if(game)
        delete game;
    if(con)
        delete con;

    play = NULL;
    game = NULL;
    con = NULL;
    role = "";

    status = IDLE;

    configure::dealloc();
}
