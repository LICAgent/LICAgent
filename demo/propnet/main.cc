#include "propnet.h"
#include "propnet_factory.h"
#include "yapcontroller.h"
#include "mcts.h"
#include "parser.h"
#include <iostream>
#include <stdlib.h>
using namespace std;

#define GGP_EXTENSION "ggp.extensions.pl"

gdl* game = NULL;
propnet* propcon = NULL;
yapcontroller* yapcon = NULL;

void setup(char* fn);
void random_simulation(int N);

int main(int argc,char** argv){
    if(argc != 2){
        cout<<"usage: propnet_demo [kif_file]"<<endl;
        return 1;
    }

    setup(argv[1]);

    vector<string> roles = propcon->roles();
    printf("roles:\n");
    for(int i=0;i<roles.size();i++)
        printf("%s ",roles[i].c_str());
    printf("\n\n");

    //srand(time(NULL));
    srand(10);
    //srand(10);

    fprintf(stderr,"testing functional equivalence of yapcontroller and propnet\n");
    random_simulation(4);

    delete yapcon;
    delete propcon;
    delete game;

    return 0;
}

void setup(char* fn){
    string KIF_FILE = fn;
    KIF_FILE += ".kif";
    string PL_FILE = fn;
    PL_FILE += ".pl";
    string COMPILED_GGP = fn;
    COMPILED_GGP += ".ggp";

    std::ifstream in(KIF_FILE);
    string input((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>()); //ref: the most vexing parse problem?

    game = parser::parse_game(input);
    FILE* fout = fopen(PL_FILE.c_str(),"w");
    game->print(fout);
    //game->printS(stdout);
    fclose(fout);

    yapcon = new yapcontroller(game,GGP_EXTENSION,PL_FILE.c_str(),COMPILED_GGP.c_str());
    assert(yapcon->init());

    timer* T = new timer();

    propnet_factory* propfac = new propnet_factory(game,20);
    propcon = propfac->create_propnet(KIF_FILE.c_str());
    assert(propcon && propcon->init());

    delete propfac;

    double dur = T->time_elapsed();
    delete T;
    fprintf(stderr,"propnet construction time:%lf\n",dur);

    //propcon->print_net(stderr);
}

static void assert_state_equal(){
    terms* prop_state = propcon->state();
    terms* yap_state = yapcon->state();

    std::equal_to<state_key> eq;
    if(!eq(state_key(prop_state),state_key(yap_state))){
        slog("propnet_demo","prop_state:\n");
        controller::print_terms(prop_state,stderr);
        
        slog("propnet_demo","yap_state:\n");
        controller::print_terms(yap_state,stderr);

        assert(false);
    }

    controller::free_terms(yap_state);
    controller::free_terms(prop_state);
}

static void assert_all_moves_equal(){
    vector<terms*>* prop_all_moves = propcon->all_moves();
    vector<terms*>* yap_all_moves = yapcon->all_moves();

    assert(prop_all_moves->size() == yap_all_moves->size());
    int n = prop_all_moves->size();
    for(int i=0;i<n;i++){
        terms* prop_imoves = prop_all_moves->at(i);
        terms* yap_imoves = yap_all_moves->at(i);

        std::equal_to<state_key> eq;
        if(!eq(state_key(prop_imoves),state_key(yap_imoves))){
            slog("propnet_demo","prop_imoves:\n");
            controller::print_terms(prop_imoves,stderr);
            
            slog("propnet_demo","yap_imoves:\n");
            controller::print_terms(yap_imoves,stderr);

            assert(false);
        }
    }

    controller::free_moves(prop_all_moves);
    controller::free_moves(yap_all_moves);
}

void random_simulation(int N){
    terms* init_state = propcon->state();
    assert_state_equal();

    vector<string> roles = propcon->roles();

    for(int dum=0;dum<N;dum++){
        int round = 0;

        fprintf(stderr,"=====================\n");
        while(!propcon->terminal()){
            fprintf(stderr,"round=%d\n",++round);

            assert(!propcon->terminal() && !propcon->terminal());

            vector<terms*>* all_moves = propcon->all_moves();
            //assert_all_moves_equal();

            terms* move = new terms();

            for(int i=0;i<all_moves->size();i++){
                terms* imoves = all_moves->at(i);

                for(int j=0;j<imoves->size();j++){
                    assert(yapcon->legal(roles[i],*imoves->at(j)));
                    assert(propcon->legal(roles[i],*imoves->at(j)));
                }

                gdl_term* t = imoves->at(rand() % imoves->size());
                move->push_back(new gdl_term(*t));
            }

            //fprintf(stderr,"joint move:\n");
            //controller::print_terms(move,stderr);

            propcon->make_move(*move);
            yapcon->make_move(*move);

            assert_state_equal();

            controller::free_terms(move);
            controller::free_moves(all_moves);
        }
        assert(propcon->terminal() && yapcon->terminal());

        for(int i=0;i<roles.size();i++){
            assert(propcon->goal(roles[i]) == yapcon->goal(roles[i]));
        }

        propcon->retract(*init_state);
        yapcon->retract(*init_state);

        assert_state_equal();
    }

    controller::free_terms(init_state);
}
