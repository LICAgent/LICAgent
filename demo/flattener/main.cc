#include "gdl_flatten.h"
#include "gdl_simplify.h"
#include "parser.h"
#include <iostream>

using namespace std;

vector<gdl_elem*> grounded_gdl;

void free_grounded_gdl();

int main(int argc,char* argv[]){
    if(argc != 2){
        cout<<"usage: propnet_demo [kif_file]"<<endl;
        return 1;
    }

    std::ifstream in(argv[1]);
    string input((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>()); //ref: the most vexing parse problem?

    gdl* game = parser::parse_game(input);
    //gdl_simplify::simplify(game); //simplify GDL

    gdl_flatten* flattener = new gdl_flatten(game);

    assert(flattener->flatten(grounded_gdl,30));

    slog("flattener","===================\n");
    slog("flattener","expanded gdl:\n");
    flattener->print(grounded_gdl);

    free_grounded_gdl();
    delete game;
    delete flattener;
    return 0;
}

void free_grounded_gdl(){
    for(auto it = grounded_gdl.begin(); grounded_gdl.end() != it;it++){
        gdl_elem* elem = *it;
        if(elem->contains<gdl_term*>()){
            gdl_term* t = elem->get<gdl_term*>();
            delete t;
        } else if(elem->contains<gdl_rule*>()){
            gdl_rule* r = elem->get<gdl_rule*>();
            delete r;
        }

        delete elem;
    }
}

