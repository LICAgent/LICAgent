#include "message.h"

static void fill_terms(terms& move,const gdl_ast& ast){
    vector<gdl_ast*>::const_iterator it = ast.children.begin();
    while(it != ast.children.end()){
        gdl_ast* c = *it;
        move.push_back(new gdl_term(c));

        it++;
    }
}

static void free_terms(terms& move){
    terms_iter it = move.begin();
    while(it != move.end()){
        delete *it;

        it++;
    }
}

start_message::start_message(gdl* g,int sc,int pc,const string& r)
:game(g),
startclock(sc),
playclock(pc),
role(r)
{
    for(int i=0;i<g->desc.size();i++){
        gdl_elem* elem = g->desc[i];
        if(elem->contains<gdl_term*>()){
            gdl_term* t = elem->get<gdl_term*>();
            if(t->get_pred() == "role"){
                roles.push_back(t->ast->children[1]->convertS());
            }
        }
    }
}

play_message::play_message(const gdl_ast& ast){
    if(ast.type() == AST_ATOM){
        assert(!strcmp(ast.sym(),"nil"));

        return;
    }

    fill_terms(move,ast);
}

play_message::~play_message(){
    free_terms(move);
}

stop_message::stop_message(const gdl_ast& ast){
    if(ast.type() == AST_ATOM && !strcmp(ast.sym(),"nil")){
        return;
    }

    if(ast.type() == AST_ATOM){
        move.push_back(new gdl_term(&ast));
    } else {
        fill_terms(move,ast);
    }
}

stop_message::~stop_message(){
    free_terms(move);
}
