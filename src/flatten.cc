#include "gdl_simplify.h"
#include "gdl_flatten.h"
#include "parser.h"
#include <iostream>

using namespace std;

vector<gdl_elem*> grounded_gdl;

void free_grounded_gdl();

int main(int argc,char* argv[]){
    if(argc != 2){
        cout<<"usage: flatten [kif_file]"<<endl;
        return 1;
    }

    int ret = 0;

    std::ifstream in(argv[1]);
    string input((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>()); //ref: the most vexing parse problem?

    gdl* game = parser::parse_game(input); //new
    gdl_simplify::simplify(game->desc); //simplify GDL

#ifdef GDL_FLATTEN_DEBUG
    fprintf(stderr,"simplified GDL:\n");
    game->printS(stderr);
#endif

    gdl_flatten* flattener = new gdl_flatten(game); //new

    if(!flattener->flatten(grounded_gdl)){ //new
        ret = 1;
        goto end;
    }

    flattener->printS(grounded_gdl,stdout);

end:
    free_grounded_gdl();
    delete game;
    delete flattener;

    return ret;
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
