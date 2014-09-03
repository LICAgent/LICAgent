#include "mcts.h"

mcts_node* mcts_player::expand(mcts_node* n,vector<int>& choice){
    mcts_node* child = NULL;

    n->UCB_select(choice,Cp);
    child = n->get_child(choice);

    if(child){
        return child;
    }

    terms* move = n->move_from_choice(choice);
    con->retract_and_move(*(n->state),*move);
    controller::free_terms(move);

    terms* s = state_wrapper(con);
    if(use_TT){
        child = lookup_node(s);
    } else {
        child = new_node(s);
    }
    controller::free_terms(s);

    n->expand(child,choice);

    return child;
}

mcts_node* mcts_player::tree_policy(mcts_node* n,vector<mcts_stack_node>& stack){
    mcts_node* cur = n;
    mcts_node* next = NULL;

    int depth = 0;

    while(!cur->terminal &&  cur->is_expanded()){
#ifdef MCTS_DEBUG
        slog("mcts_player","tree_policy, cur state:\n");
        controller::print_terms(cur->state,stdout);
        slog("mcts_player","\n\n");
#endif

        mcts_stack_node sn;
        sn.node = cur;
        next = expand(cur,sn.choice);
        stack.push_back(sn);

        cur = next;
        depth++;
    }

    if(!cur->terminal && 
       ((limit_node && mcts_node_count < node_max_count) ||
        (!limit_node))){

        mcts_stack_node sn;
        sn.node = cur;
        next = expand(cur,sn.choice);
        stack.push_back(sn);

        cur = next;
    }

    return cur; 
}

terms* mcts_player::choose_default_action(vector<terms*>* all_moves){
    terms* move = new terms();

    for(int i=0;i<all_moves->size();i++){
        terms* imoves = all_moves->at(i);

        gdl_term* t = NULL;
        if(use_MAST){
            int j;
            double sw = 0.0;
            vector<double> ws;
            for(j=0;j<imoves->size();j++){
                double w = get_MAST_weight(i,imoves->at(j));
                ws.push_back(w);
                sw += w;
            }

            double p = rand1();

            for(j=0;j<imoves->size()-1;j++){
                if(ws[j]/sw <= p && p < ws[j+1]/sw){
                    break;
                }
            }

            t = imoves->at(j);
        } else {
            t = imoves->at(rand() % imoves->size());
        }

        move->push_back(new gdl_term(*t));
    }

    return move;
}

int mcts_player::default_policy(controller* tcon,const mcts_node* n,vector<double>* outcome, sp_stack_type* sp, rave_set* rv){
    using namespace std;

    retract_wrapper(tcon,*(n->state));

    while(!tcon->terminal()){
        vector<terms*>* all_moves = tcon->all_moves();

        terms* move = choose_default_action(all_moves);

        if(use_RAVE || use_MAST){
            for(int i=0;i<move->size();i++){
                gdl_term* t = move->at(i);
                pred_wrapper pw(new gdl_term(*t));

                if(rv->at(i)->find(pw) == rv->at(i)->end()){
                    rv->at(i)->insert(pw);
                } else {
                    delete pw.t;
                }
            }
        }

        if(n_roles == 1){
            terms* cur_state = state_wrapper(tcon);
            terms* cur_move = controller::copy_terms(move);
            sp->push_back(pair<terms*,terms*>(cur_state, cur_move));
        }

        make_move_wrapper(tcon,*move);

        controller::free_terms(move);
        controller::free_moves(all_moves);
    }

    vector<string> roles = tcon->roles();
    for(int i=0;i<n_roles;i++){
        int gv = tcon->goal(roles[i]);

        if(gv == -1){ //work around

#ifdef MCTS_COLLECT_ERROR
            if(!err1_flag){
                err1_flag = true;

                fprintf(err_output,"[%s]:",game_id.c_str());
                fprintf(err_output,"invalid goal value\n");
            }
#endif

            gv = 0;
        }
        //assert(gv != -1);

        outcome->push_back(gv);
    }

    //test
    //fprintf(stderr,"default_policy finished\n");

    return 1;
}

void mcts_player::backup(const vector<double>& outcome,const vector<mcts_stack_node>& cstack, rave_set& rv){
    vector<mcts_stack_node> stack = cstack;
    double gamma = mcts_gamma;

    while(stack.size() > 0){
        mcts_stack_node sn = stack.back();
        stack.pop_back();

        mcts_node* par = sn.node;
        par->update_stats(sn.choice,outcome,gamma);
        if(use_RAVE){
            par->update_rave(sn.choice,outcome,gamma,rv);
        }

        gamma *= mcts_gamma;
    }

    if(use_MAST){
        update_MAST(outcome,stack,rv);
    }
}

void mcts_player::single_extend(const sp_stack_type& pstack,vector<mcts_stack_node>& stack){
    for(int i=0;i<pstack.size();i++){
        terms* state = pstack[i].first;
        terms* move = pstack[i].second;

        mcts_node* node = lookup_node(state);

        mcts_stack_node sn;
        sn.node = node;
        node->choice_from_move(move,sn.choice);

        stack.push_back(sn);
    }
}
