#include "mcts.h"
#include "yapcontroller.h"
#include "propnet.h"

#define GGP_EXTENSION "./ggp.extensions.pl"
//#define STARTCLOCK 170
//#define PLAYCLOCK 40

#define STARTCLOCK 15
#define PLAYCLOCK 15

//#define STARTCLOCK 90
//#define PLAYCLOCK 45

#define BOOT_USE_PROPNET

//#define MANUAL_CHOICE

controller* con = NULL;
gdl* game = NULL;
mcts_player* play;

vector<string> roles;
int role_index;

int human_role_index;

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
        cout<<"usage: boot_demo [kif_file]"<<endl;
        return 1;
    }

    setup(argv[1]);

    terms* jm = new terms();
    while(!con->terminal()){
        terms* state = con->state();

        cout<<"------------------------------"<<endl;
        cout<<"===current state==="<<endl;
        controller::print_terms(state,stdout);

        terms* moves = con->get_move(roles[human_role_index]);
        cout<<"===available moves==="<<endl;
        //controller::print_terms(moves,stdout);
        for(int i=0;i<moves->size();i++){
            cout<<"["<<i<<"]:"<<moves->at(i)->convertS()<<endl;
        }


        int i;

        cout<<endl<<"select move:";
#ifdef MANUAL_CHOICE
        cin>>i;
        while(i > moves->size() || i < 0){
            cout<<"invalid index"<<endl;

            cout<<endl<<"select move:";
            cin>>i;
        }
#else
        i = rand() % moves->size();
#endif

        gdl_term* human_move = moves->at(i);

        timer* T = new timer();
        gdl_term* mcts_move = play->next_move(PLAYCLOCK,*jm,T);
        delete T;

        cout<<endl<<endl<<"========mcts_move========"<<endl;
        cout<<mcts_move->convertS()<<endl<<endl<<endl;

        for(int i=0;i<jm->size();i++){
            delete jm->at(i);
        }
        jm->clear();

        if(role_index == 0){
            jm->push_back(new gdl_term(*mcts_move));
            jm->push_back(new gdl_term(*human_move));
        } else {
            jm->push_back(new gdl_term(*human_move));
            jm->push_back(new gdl_term(*mcts_move));
        }

        cout<<"joint moves:"<<endl;
        controller::print_terms(jm,stdout);

        con->make_move(*jm);

        controller::free_terms(state);
        controller::free_terms(moves);
        delete mcts_move;
    }
    for(int i=0;i<jm->size();i++){
        delete jm->at(i);
    }
    jm->clear();
    delete jm;

    cout<<"====================="<<endl;
    for(int i=0;i<roles.size();i++){
        int gv = con->goal(roles[i]);

        if(i == role_index){
            cout<<"mcts goal:";
        } else {
            cout<<"your goal:";
        }

        cout<<"(goal " << roles[i] << " " << gv << ")" << endl;
    }

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

    configure::initialize(CONFIG_FILE);

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
    //game->printS(stdout);
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

    roles = con->roles();
    assert(roles.size() == 2);

    int i;
    cout<<"roles:"<<endl;
    cout<<"[0]"<<roles[0]<<endl;
    cout<<"[1]"<<roles[1]<<endl;

    cout<<"select your role:";
#ifdef MANUAL_CHOICE
    cin>>i;
#else
    i = rand() % 2;
#endif

    assert(0 <= i && i <= 1);
    role_index = 1-i;
    human_role_index = i;

    double Cp;
    configure::get_option("MCTS_CP",&Cp);

#ifdef BOOT_USE_PROPNET
    play = new mcts_player(con,game,Cp,true);
    //play = new mcts_player(con,game,Cp);
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

    cout<<"meta gaming"<<endl;

    stringstream ss;
#ifdef BOOT_USE_PROPNET
    ss << "boot_demo.prop." << fn << "." << wall_time();
#else 
    ss << "boot_demo.yap." << fn << "." << wall_time();
#endif

    game_id = ss.str();

    timer* T2 = new timer();
    play->meta_game(game_id,roles[role_index],STARTCLOCK,T2);
    delete T2;
}
