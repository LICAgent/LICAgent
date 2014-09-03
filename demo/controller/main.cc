#include "yapcontroller.h"
#include "parser.h"
#include <string>
#include <stdlib.h>

using std::string;

//#define KIF_FILE "tic.kif"
//#define PL_FILE "tic.pl"
//#define COMPILED_GGP "tic.ggp"

//#define KIF_FILE "pentago.kif"
//#define PL_FILE "pentago.pl"
//#define COMPILED_GGP "pentago.ggp"

//#define KIF_FILE "buttons.kif"
//#define PL_FILE "buttons.pl"
//#define COMPILED_GGP "buttons.ggp"

//#define KIF_FILE "3-puzzle.kif"
//#define PL_FILE "3-puzzle.pl"
//#define COMPILED_GGP "3-puzzle.ggp"

#define GGP_EXTENSION "ggp.extensions.pl"

int main(int argc,char** argv){
    string KIF_FILE = argv[1];
    KIF_FILE += ".kif";
    string PL_FILE = argv[1];
    PL_FILE += ".pl";
    string COMPILED_GGP = argv[1];
    COMPILED_GGP += ".ggp";

    std::ifstream in(KIF_FILE);
    string input((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>()); //ref: the most vexing parse problem?

    gdl* game = parser::parse_game(input);
    FILE* fout = fopen(PL_FILE.c_str(),"w");
    game->print(fout);
    //game->printS(stdout);
    fclose(fout);

    controller* con = new yapcontroller(game,GGP_EXTENSION,PL_FILE.c_str(),COMPILED_GGP.c_str());
    if(!con->init()){
        fprintf(stderr,"error initiating controller!\n");

        delete con;
        delete game;
        return 1;
    }

    vector<string> roles = con->roles();
    printf("roles:\n");
    for(int i=0;i<roles.size();i++)
        printf("%s ",roles[i].c_str());
    printf("\n\n");

    srand(time(NULL));

    while(!con->terminal()){
        printf("===========================================\n");
        printf("get_current_state:\n");

        terms* cur_state = con->state();
        controller::print_terms(cur_state,stdout);
        controller::free_terms(cur_state);

        printf("====================\n");

        terms j_move;
        vector<terms*>* all_moves = con->all_moves();
        vector<terms*>::iterator it = all_moves->begin();
        while(it != all_moves->end()){
            terms* moves = *it;
            int i = rand() % moves->size();

            j_move.push_back((*moves)[i]);

            it++;
        }

        printf("====================\n");
        printf("j_move:\n");
        controller::print_terms(&j_move,stdout);
        printf("====================\n");

        con->make_move(j_move);

        controller::free_moves(all_moves);
    }

    printf("====================\n");
    printf("terminal state:\n");

    terms* cur_state = con->state();
    controller::print_terms(cur_state,stdout);
    controller::free_terms(cur_state);

    printf("====================\n");

    for(int i=0;i<roles.size();i++){
        using namespace std;

        int gv = con->goal(roles[i]);

        cout<<"(goal " << roles[i] << " " << gv << ")" << endl;
    }


    printf("deleting controller\n");
    delete con;
    delete game;
    //printf("game(%x)\n",(int)(game));
    return 0;
}
