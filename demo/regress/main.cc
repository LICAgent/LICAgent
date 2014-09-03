#include "parser.h"
#include "gdl_flatten.h"
#include "regress.h"
#include "mcts.h"
#include "mcts_struct.h"
#include "yapcontroller.h"

controller* con = NULL;
gdl* game = NULL;

#define GGP_EXTENSION "ggp.extensions.pl"

void random_simulation(int N,unordered_set<state_key>& uset);
void setup(char* fn);

int main(int argc,char* argv[]){
    setup(argv[1]);

    gdl_flatten* flatten = new gdl_flatten(game); //new
    domains_type* domains = flatten->build_and_get_domain();

    //flatten->print_pi_inits();

    vector<string> roles;
    for(auto it = game->desc.begin(); it != game->desc.end(); it++){
        gdl_elem* elem = *it;
        if(elem->contains<gdl_term*>()){
            gdl_term* t = elem->get<gdl_term*>();
            if(t->get_pred() == "role"){
                roles.push_back(t->ast->children[1]->convertS());
            }
        }
    }

    pred_index goal_value("goal",2);

    unordered_set<state_key> sim_states;

    fprintf(stdout,"=====================================================\n");
    fprintf(stdout,"%s\n",argv[1]);
    fprintf(stdout,"dur\tNstates\tfreq(states/sec)\tcomplexity\tavg_state_size\n");
    fflush(stdout);

    unordered_set<string>* gv_dom = domains->at(goal_value);
    for(int i=0;i<roles.size();i++){
        string role = roles[i];
        for(auto gv_it = gv_dom->begin(); gv_it != gv_dom->end(); gv_it++){
            string gv = *gv_it;

            gdl_ast* ast = new gdl_ast();
            ast->children.push_back(new gdl_ast("goal"));
            ast->children.push_back(new gdl_ast(role.c_str()));
            ast->children.push_back(new gdl_ast(gv.c_str()));

            gdl_term* goal_term = new gdl_term(ast);

            //slog("regress_demo","beginning to regress goal_term:%s\n",goal_term->convertS().c_str());

            timer* T = new timer();
            rnode* result = regress::regress_term(game,domains,goal_term,T,2.0);
            delete T;

            if(!result){
                serror("regress_demo","\nfailed to regress term %s\n\n",goal_term->convertS().c_str());
            } else {
                gdl_term* term = result->compose();

                fprintf(stderr,"\n");
                slog("regress_demo","result of regressing term %s\n",goal_term->convertS().c_str());
                fprintf(stdout,"===========================\n");
                fprintf(stdout,"%s\n",term->convertS().c_str());
                fprintf(stdout,"===========================\n\n");

                if(sim_states.size() == 0){
                    random_simulation(20,sim_states);
                }

                //fprintf(stderr,"sim_states.size() = %d\n",sim_states.size());
                double avg_state_size = 0.0;
                int N_state_size = 0;

                timer* T = new timer();
                int count = 0;
                for(auto it = sim_states.begin(); sim_states.end() != it; it++){
                    terms* state = it->state;
                    (void)result->eval(state);

                    avg_state_size = (avg_state_size*N_state_size + state->size())/(N_state_size + 1);
                    N_state_size++;

                    count++;
                    if(count % 10 == 0){
                        //fprintf(stderr,"count=%d\n",count);
                    }
                }
                double dur = T->time_elapsed();
                int Nstates = sim_states.size();

                fprintf(stdout,"%.3lf\t%d\t%.3lf\t%d\t%lf\n",dur,Nstates,Nstates/dur,result->gauge_com(),avg_state_size);

                delete T;

                delete term;
                delete result;
            }

            delete goal_term;
            delete ast;
        }
    }

    for(auto it = sim_states.begin(); sim_states.end() != it; it++){
        terms* state = it->state;
        controller::free_terms(state);
    }
    sim_states.clear();

    delete flatten;
    delete game;
    delete con;

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
    string input((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());

    game = parser::parse_game(input);
    FILE* fout = fopen(PL_FILE.c_str(),"w");
    game->print(fout);
    fclose(fout);

    con = new yapcontroller(game,GGP_EXTENSION,PL_FILE.c_str(),COMPILED_GGP.c_str());
    assert(con->init());
}

void random_simulation(int N,unordered_set<state_key>& uset){
    terms* init_state = con->state();

    for(int dum=0;dum<N;dum++){
        //fprintf(stderr,"sim run #%d\n",dum);

        while(!con->terminal()){
            terms* state = con->state();
            if(uset.find(state_key(state)) == uset.end()){
                uset.insert(state_key(state));
            } else {
                controller::free_terms(state);
            }

            vector<terms*>* all_moves = con->all_moves();
            terms* move = new terms();

            for(int i=0;i<all_moves->size();i++){
                terms* imoves = all_moves->at(i);

                gdl_term* t = imoves->at(rand() % imoves->size());
                move->push_back(new gdl_term(*t));
            }

            con->make_move(*move);

            controller::free_terms(move);
            controller::free_moves(all_moves);
        }

        con->retract(*init_state);
    }

    controller::free_terms(init_state);
}
