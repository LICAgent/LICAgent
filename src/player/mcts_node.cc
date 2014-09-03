#include "mcts.h"
#include "slog.h"
#include <cmath>

bool mcts_node::same_pointer_flag = false;

mcts_node::mcts_node(terms* s,mcts_player* pl){
    state = controller::copy_terms(s);
    play = pl;

    ref = 0;

    play->mcts_node_count++;
    play->all_mcts_nodes.insert(this);

    if(play->use_TT){
        play->state_node.insert(state_node_pair(state_key(state),this));
    }
}

mcts_node::~mcts_node(){
    if(play->use_TT){
        play->state_node.erase(state_key(state));
    }

    controller::free_terms(state);
    controller::free_moves(moves);

    edge_set::iterator it = parents.begin();
    while(it != parents.end()){
        const mcts_node_edge& edge = *it;
        if(play->all_mcts_nodes.find(edge.node) != play->all_mcts_nodes.end()){ //so that the node is not deleted
            mcts_node_edge parent_to_child(this,edge.choices);
            edge.node->children.erase(parent_to_child);
        }

        it++;
    }

    play->mcts_node_count--;
}

double mcts_node::get_Q(int i,int j){
    return edges[i][j].Q;
}

double mcts_node::get_Q2(int i,int j){
    return edges[i][j].Q2;
}

double mcts_node::get_best(int i,int j){
    return edges[i][j].bestQ;
}

static double rave_beta(int Nij,int V){
    if(Nij > V) return 0;

    return (V - Nij)/(double)V;
}

static double reg_beta(int Nij,int V){
    return rave_beta(Nij,V);
}

double mcts_node::get_Q_(int i,int j){
    int Nij = get_N(i,j);
    double Qij = get_Q(i,j);

    double beta = 0.0;
    double rQij = 0.0;
    if(play->use_RAVE){
        beta = rave_beta(Nij,play->rave_V);
        rQij = edges[i][j].raveQ;
    }

    double beta2 = 0.0;
    double regQ = 0.0;
    if(play->use_REG){
        beta2 = reg_beta(Nij,play->reg_V);
        regQ = edges[i][j].regQ;
    }

    //double old_beta = beta;
    //double old_beta2 = beta2;

    if(play->use_RAVE && play->use_REG){
        double wbeta = beta*play->rave_w + beta2*play->reg_w;
        if(wbeta > 0.0){
            beta = beta*play->rave_w / wbeta;
            beta2 = beta2*play->reg_w / wbeta;
        }
    }

    double ret = beta*rQij + beta2*regQ + (1-beta-beta2)*Qij;
    assert(std::isfinite(ret));
    return ret;
}

int mcts_node::get_N(int i,int j){
    return edges[i][j].N;
}

double mcts_node::get_G(int i,int j){
    return edges[i][j].sgamma;
}

gdl_term* mcts_node::get_move(int i,int j){
    return moves->at(i)->at(j);
}

terms* mcts_node::get_imove(int i){
    return moves->at(i);
}

void mcts_node::updateQ(int i,int j,double v,double gamma){
    double Qij = edges[i][j].Q;
    double Q2ij = edges[i][j].Q2;
    int sGij = edges[i][j].sgamma;

    Qij = (Qij*sGij + gamma*v)/(sGij+gamma);
    Q2ij = (Q2ij*sGij + gamma*v*v)/(sGij+gamma);

    if(v > edges[i][j].bestQ){
        edges[i][j].bestQ = v;
    }

    edges[i][j].Q = Qij;
    edges[i][j].Q2 = Q2ij;
    edges[i][j].sgamma += gamma;

    if(edges[i][j].N == 0){
        expanded_count[i]++;
    }

    edges[i][j].N++;
}

void mcts_node::update_raveQ(int i,int j,double v,double gamma){
    double rQij = edges[i][j].raveQ;
    double rGij = edges[i][j].rgamma;

    rQij = (rQij*rGij+ gamma*v)/(rGij+gamma);

    edges[i][j].raveQ = rQij;
    edges[i][j].rgamma += gamma;
}

void mcts_node::init_joint_choice(int i,int n,vector<int>& p){
    if(i >= n){
        joint_choices.insert(p);

        return;
    }

    for(int j=0;j<edges[i].size();j++){
        p.push_back(j);
        init_joint_choice(i+1,n,p);
        p.pop_back();
    }
}

void mcts_node::init(controller* con){
    using namespace std;

    moves = con->all_moves();
    terminal = con->terminal();
    if(terminal){
        vector<string> roles = con->roles();
        for(int i=0;i<roles.size();i++){
            gvs.push_back(con->goal(roles[i]));
        }
    }

    int i,j;
    int n = moves->size();

    for(i=0;i<n;i++){
        edges.push_back(vector<edge_info>());

        for(j=0;j<moves->at(i)->size();j++){
            edges[i].push_back(edge_info());
        }
    }

    all_joint_moves = 1;
    for(int i=0;i<n;i++){
        all_joint_moves *= edges[i].size();
        expanded_count.push_back(0);
    }

    if(all_joint_moves > 100){
        use_joint_choices = false;
    } else {
        use_joint_choices = true;
        vector<int> p;
        init_joint_choice(0,n,p); //joint moves
    }
}

bool mcts_node::is_expanded(){
    if(use_joint_choices){
        return joint_choices.size() == 0; //joint moves
    } else {
        for(int i=0;i<edges.size();i++){
            if(edges[i].size() > expanded_count[i]){
                return false;
            }
        }
    }

    return true;
}

void mcts_node::fill_regress(mcts_node* child,const vector<int>& choices){
    for(int i=0;i<edges.size();i++){
        int j = choices[i];

        edges[i][j].use_reg = true;
        edges[i][j].regQ = play->regress(child,i);

        //test
        //fprintf(stderr,"^^^^^^^^^^^^^^^^^^^^^^\n");
        //fprintf(stderr,"regress result:\n");
        //fprintf(stderr,"state:\n");
        //controller::print_terms(child->state,stderr);
        //fprintf(stderr,"regQ = %lf\n",edges[i][j].regQ);
        //fprintf(stderr,"^^^^^^^^^^^^^^^^^^^^^^\n\n");
    }
}

void mcts_node::expand(mcts_node* child,const vector<int>& choices){
    mcts_node_edge child_to_parent(this,choices);
    mcts_node_edge parent_to_child(child,choices);

    this->children.insert(parent_to_child);
    child->parents.insert(child_to_parent);

    if(use_joint_choices){
        joint_choices.erase(choices); //joint moves
    }

    if(play->use_REG){
        fill_regress(child,choices);
    }
}

double mcts_node::get_score(int i,int j,int Nisum,double Cp){ //simply assume the best goal value is 100
    int Nij;

    double Qij,Q2ij;
    double Q_ij;

    Nij = get_N(i,j);

    Qij = get_Q(i,j);
    Q2ij = get_Q2(i,j);
    Q_ij = get_Q_(i,j);

    double ret = 0.0;
    if(play->use_UCB_TUNE){
        double upperBound = (Q2ij/10000.0) - (Qij/100.0)*(Qij/100.0) + sqrt(2*log(Nisum)/Nij);
        ret = Q_ij/100.0 + Cp*sqrt((log(Nisum)/Nij)*std::min(upperBound,0.25));
    } else {
        ret = Q_ij/100.0 + Cp*sqrt(2.0*log(Nisum)/Nij);
    }
    assert(std::isfinite(ret));

    return ret;
}

int mcts_node::UCB1(int i,double Cp){
    int retj = -1;
    double bestscore = -10000.0; //-oo

    int j;

    int Nisum = 0;
    for(j=0;j<edges[i].size();j++){
        Nisum += get_N(i,j);
    }
    
    if(Nisum == 0){ //nothing explored
        retj = rand() % edges[i].size();
    } else {
        vector<int> unexplored;
        vector<int> ties;
        
        for(j=0;j<edges[i].size();j++){
            int Nij = get_N(i,j);
            if(Nij != 0){
                double score = get_score(i,j,Nisum,Cp);

                if(score >= bestscore){
                    if(score == bestscore){
                        ties.push_back(j);
                    } else if(score > bestscore){
                        bestscore = score;

                        ties.clear();
                        ties.push_back(j);
                    }
                }
            } else {
                unexplored.push_back(j);
            }
        }

        if(unexplored.size() > 0){
            retj = unexplored[rand() % unexplored.size()];
        } else {
            if(ties.size() != 0){
                retj = ties[rand() % ties.size()];
            } else { //@TODO: for now
                retj = rand() % edges[i].size(); 
            }
        }
    }

    return retj;
}

void mcts_node::UCB_select(vector<int>& choice,double Cp){
    if(use_joint_choices){
        if(!is_expanded()){ //joint moves
            choice = *(joint_choices.begin()); //@TODO: how to select a random element in this case from a set in O(1)?

            return;
        }

        for(int i=0;i<edges.size();i++){
            int t = UCB1(i,Cp);
            choice.push_back(t);
        }
    } else {
        for(int i=0;i<edges.size();i++){
            int selj = -1;

            if(expanded_count[i] < edges[i].size()){
                vector<int> unexplored;

                for(int j=0;j<edges[i].size();j++){
                    int Nij = get_N(i,j);
                    if(Nij == 0){
                        unexplored.push_back(j);
                    }
                }

                if(unexplored.size() > 0){
                    selj = unexplored[rand() % unexplored.size()];
                } else { //@NOTE: this should never happen!
                    selj = UCB1(i,Cp);
                }
            } else {
                selj = UCB1(i,Cp);
            }

            assert(selj != -1);
            choice.push_back(selj);
        }
    }
}

terms* mcts_node::move_from_choice(const vector<int>& choice){
    terms* ret = new terms();
    for(int i=0;i<choice.size();i++){
        gdl_term* imove = (*moves)[i]->at(choice[i]);

        ret->push_back(new gdl_term(*imove));
    }

    return ret;
}

gdl_term* mcts_node::choose_best(int i){
    int retj = -1;
    vector<int> ties;

    if(edges.size() == 1){
        for(int j=0;j<edges[i].size();j++){
            int Nij = get_N(i,j);

            if(Nij != 0){
                double bQij = get_best(i,j);
                double bQirj = -1.0;
                if(retj != -1){
                    bQirj = get_best(i,retj);
                }

                if(retj == -1 || bQirj <= bQij){
                    if(retj != -1 && bQirj == bQij){
                        ties.push_back(j);
                    } else if(retj == -1 || bQirj < bQij){
                        retj = j;
                        ties.clear();
                        ties.push_back(retj);
                    }
                }

                double Q_ij = get_Q_(i,j);
                slog("mcts_node","move(best) :%s(%lf)(%lf)[%d]\n",moves->at(i)->at(j)->convertS().c_str(),bQij,Q_ij,Nij);
            }
        }

        if(ties.size() > 0)
            retj = ties[rand() % ties.size()];
    } else {
        for(int j=0;j<edges[i].size();j++){
            int Nij = get_N(i,j);

            if(Nij != 0){
                double Q_ij = get_Q_(i,j);
                double Q_irj = -1.0;
                if(retj != -1){
                    Q_irj = get_Q_(i,retj);
                }

                if(retj == -1 || Q_irj <= Q_ij){
                    if(retj != -1 && Q_irj == Q_ij){
                        ties.push_back(j);
                    } else if(retj == -1 || Q_irj < Q_ij){
                        retj = j;
                        ties.clear();
                        ties.push_back(retj);
                    }
                }

                double Qij = get_Q(i,j);
                slog("mcts_node","move:%s(%lf)(%lf)[%d]\n",moves->at(i)->at(j)->convertS().c_str(),Q_ij,Qij,Nij);
            }
        }

        if(ties.size() > 0)
            retj = ties[rand() % ties.size()];
    }

    if(retj == -1){
        retj = rand() % edges[i].size();
    }

    gdl_term* ret = moves->at(i)->at(retj);

    return new gdl_term(*ret);
}

mcts_node* mcts_node::get_child(const vector<int>& choices){ //@TODO: can we improve the efficiency of this operation?
    for(edge_set::const_iterator it = children.begin(); children.end() != it; it++){
        const mcts_node_edge& e = *it;

        if(std::equal_to<vector<int> >()(choices,e.choices)){
            return e.node;
        }
    }

    return NULL;
}


int mcts_node::choice_from_move(const terms* move,vector<int>& choices){
    choices.clear();
    for(int i=0;i<move->size();i++){
        gdl_term* imove = move->at(i);

        for(int j=0;j < moves->at(i)->size();j++){
            gdl_term* m = moves->at(i)->at(j);

            //fprintf(stderr,"m:%s\n",m->convertS().c_str());

            if(*m == *imove){
                choices.push_back(j);
                break; //sometimes the actions in moves are duplicated (presumably due to implementations of YAP Prolog, thus causing problems
            }
        }
        //fprintf(stderr,"\n");
    }

    if(choices.size() != move->size()){
        serror("mcts_node","out of sync with game server!\n");
        choices.clear();

        assert(choices.size() == move->size()); //@NOTE: out of sync! we simply let it crash!

        return 0;
    }

    return 1;
}

void mcts_node::update_stats(const vector<int>& choice,const vector<double>& outcome,double gamma){
    for(int i=0;i<choice.size();i++){
        int j = choice[i];
        updateQ(i,j,outcome[i],gamma);
    }
}

void mcts_node::update_rave(const vector<int>& choice,const vector<double>& outcome,double gamma, const rave_set& playout_moves){
    for(int i=0;i<edges.size();i++){
        for(int j=0;j<edges[i].size();j++){
            gdl_term* move = get_move(i,j);
            pred_wrapper pw(move);

            if(playout_moves[i]->find(pw) != playout_moves[i]->end()){
                update_raveQ(i,j,outcome[i],gamma);
            }
        }
    }
}

//==================================================

bool state_key::use_zobrist_hash = false;
map<string, int> state_key::zobrist_tab;

bool state_key::init_zobrist(terms* bases){
    if(bases->size() == 0){
        use_zobrist_hash = false;
        zobrist_tab.clear();

        slog("mcts_node","not using zobrist\n");

        return false;
    }

    use_zobrist_hash = true;
    set<int> Zs;
    for(int i=0;i<bases->size();i++){
        gdl_term* t = bases->at(i);

        int Z = rand();
        while(Zs.find(Z) != Zs.end()){
            Z = rand();
        }
        Zs.insert(Z);

        zobrist_tab.insert(pair<string,int>(t->convertS(),Z));
    }

    slog("mcts_node","using zobrist\n");

    return true;
}

void state_key::dealloc_zobrist(){
    slog("mcts_node","deallocing zobrist\n");

    use_zobrist_hash = false;
    zobrist_tab.clear();
}

namespace std{
    bool equal_to<state_key>::operator()(const state_key& k1,const state_key& k2) const{
        terms* t1 = k1.state;
        terms* t2 = k2.state;

        if(mcts_node::same_pointer_flag){
            return t1 == t2;
        }

        if(t1 == t2) return true;

        if(t1->size() != t2->size()) return false;

        if(state_key::use_zobrist_hash){
            set<int> Zs1;

            for(int i=0;i<t1->size();i++){
                string key = t1->at(i)->convertS();
#ifdef MCTS_CHECK
                assert(state_key::zobrist_tab.find(key) != state_key::zobrist_tab.end());
#endif
                int Z = state_key::zobrist_tab[key];

                Zs1.insert(Z);
            }

            for(int i=0;i<t2->size();i++){
                string key = t2->at(i)->convertS();
#ifdef MCTS_CHECK
                assert(state_key::zobrist_tab.find(key) != state_key::zobrist_tab.end());
#endif
                int Z = state_key::zobrist_tab[key];

                if(Zs1.find(Z) == Zs1.end()){
                    return false;
                }
            }

            return true;
        }

        for(int i=0;i<t1->size();i++){
            int j;
            for(j=0;j<t2->size();j++){
                gdl_term* gt1 = t1->at(i);
                gdl_term* gt2 = t2->at(j);

                string s1 = gt1->convertS();
                string s2 = gt2->convertS();


                if(*(t1->at(i)) == *(t2->at(j))) break;
            }
            if(j == t2->size()){
                return false;
            }
        }

        return true;
    }

    std::size_t hash<state_key>::operator()(const state_key& k) const{
        if(state_key::use_zobrist_hash){
            terms* ts = k.state;

            size_t ret = 0;
            for(int i=0;i<ts->size();i++){
                string key = ts->at(i)->convertS();
#ifdef MCTS_CHECK
                assert(state_key::zobrist_tab.find(key) != state_key::zobrist_tab.end());
#endif

                int Z = state_key::zobrist_tab[key];

                ret ^= Z;
            }

            return ret;
        }

        terms* t = k.state;
        size_t ret = 0;
        for(int i=0;i<t->size();i++){
            ret ^= t->at(i)->hash();
        }

        return ret;
    }
};

