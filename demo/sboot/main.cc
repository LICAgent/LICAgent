#include <dlib/svm.h>
#include "mcts.h"
#include "clasp_player.h"
#include "yapcontroller.h"
#include "propnet.h"

#define GGP_EXTENSION "./ggp.extensions.pl"
//#define STARTCLOCK 60
//#define PLAYCLOCK 15

#define STARTCLOCK 20
#define PLAYCLOCK 20

//#define BOOT_USE_PROPNET

//#define MANUAL_CHOICE

controller* con = NULL;
gdl* game = NULL;
player* play = NULL;

string role;
string game_id;

void setup(const char* fn);

int main(int argc,char* argv[]){
    using namespace std;

    //check Hoard
    char* hoard = getenv("LD_PRELOAD");
    if(hoard != NULL){
        fprintf(stderr,"LD_PRELOAD=%s\n",hoard);
    } else {
        fprintf(stderr,"LD_PRELOAD=\n");
    }

    if(argc != 2){
        cout<<"usage: sboot_demo [kif_file]"<<endl;
        return 1;
    }

    setup(argv[1]);

    terms* jm = new terms();
    while(!con->terminal()){
        terms* state = con->state();

        cout<<"------------------------------"<<endl;
        cout<<"===current state==="<<endl;
        controller::print_terms(state,stdout);

        timer* T = new timer();
        gdl_term* mcts_move = play->next_move(PLAYCLOCK,*jm,T);
        delete T;

        cout<<endl<<endl<<"========mcts_move========"<<endl;
        cout<<mcts_move->convertS()<<endl<<endl<<endl;

        for(int i=0;i<jm->size();i++){
            delete jm->at(i);
        }
        jm->clear();

        jm->push_back(new gdl_term(*mcts_move));

        cout<<"joint moves:"<<endl;
        controller::print_terms(jm,stdout);

        con->make_move(*jm);

        controller::free_terms(state);
        delete mcts_move;
    }
    for(int i=0;i<jm->size();i++){
        delete jm->at(i);
    }
    jm->clear();
    delete jm;

    cout<<"====================="<<endl;
    int gv = con->goal(role);

    cout<<"mcts goal:";
    cout<<"(goal " << role << " " << gv << ")" << endl;

    play->finish_game();

    configure::dealloc();

    delete con;
    delete game;
    delete play;

#ifdef BOOT_USE_PROPNET
    propnet::clear_propnet();
#endif

    return 0;
}


void setup(const char* fn){
    using namespace std;

    configure::initialize("sen.config");

    string KIF_FILE = fn;
    KIF_FILE += ".kif";
    string PL_FILE = fn;
    PL_FILE += ".pl";
    string COMPILED_GGP = fn;
    COMPILED_GGP += ".ggp";

    std::ifstream in(KIF_FILE);
    string input((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());

    game = parser::parse_game(input);
    FILE* fout = fopen(PL_FILE.c_str(),"w");
    game->print(fout);
    game->printS(stdout);
    fclose(fout);

#ifndef BOOT_USE_PROPNET
    con = new yapcontroller(game,GGP_EXTENSION,PL_FILE.c_str(),COMPILED_GGP.c_str());
    if(!con->init()){
        fprintf(stderr,"error initiating controller!\n");

        delete con;
        delete game;

        exit(1);
    }
#else
    int timeout = 10;
    configure::get_option("PROPNET_FLATTEN_TIMEOUT",&timeout);
    propnet_factory* propfac = new propnet_factory(game,timeout);
    con = propfac->create_propnet(KIF_FILE.c_str());
    assert(con && con->init());

    delete propfac;
#endif

    vector<string> roles = con->roles();
    assert(roles.size() == 1);
    role = roles[0];

    stringstream ss;
    ss << "boot_demo." << wall_time();
    game_id = ss.str();

    bool try_clasp_player = false;
    configure::get_option("TRY_CLASP_PLAYER",&try_clasp_player);
    bool success = false;

    if(try_clasp_player){
        string cl_path;
        configure::get_option("CLASP_FILE_DIR",&cl_path);
        cl_path += game_id;
        cl_path += ".lp";

        double tl;
        configure::get_option("CLASP_TIME_LIMIT",&tl);

        timer* T2 = new timer();

        play = new clasp_player(con,game,cl_path.c_str(),tl,T2);
        if(play&& play->init()){
            fprintf(stderr,"------- successfully initialized clasp_player!\n\n");

            success = true;
        } else {
            fprintf(stderr,"------- failed to init clasp_player!\n\n");

            if(play){
                delete play;
                play = NULL;
            }
        }

        delete T2;
    }

    if(!success){
        double Cp;
        configure::get_option("MCTS_CP",&Cp);

#ifdef BOOT_USE_PROPNET
        play = new mcts_player(con,game,Cp,true);
#else
        play = new mcts_player(con,game,Cp);
#endif
        if(!play->init()){
            fprintf(stderr,"player initialization failed\n");

            delete con;
            delete game;
            delete play;

            exit(1);
        }
    }

    cout<<"meta gaming"<<endl;


    timer* T2 = new timer();
    play->meta_game(game_id,roles[0],STARTCLOCK,T2);
    delete T2;
}
