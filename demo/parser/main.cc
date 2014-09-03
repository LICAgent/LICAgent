#include "parser.h"
#include "message.h"
#include <iostream>
#include <string>

using std::string;
using std::cout;
using std::endl;

int main(int argc,char* argv[]){
    string KIF_FILE = string(argv[1]) + ".kif";
    string PL_FILE = string(argv[1]) + ".pl";

    std::ifstream in(KIF_FILE);
    string input((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>()); //ref: the most vexing parse problem?

    cout<<"FUCK"<<endl;
    cout<<input;
    cout<<"FUCK YEAH"<<endl<<endl;

    gdl* game = parser::parse_game(input);

    FILE* fout = fopen(PL_FILE.c_str(),"w");
    game->print(fout);
    game->printS(stdout);

    delete game;
    game = NULL;

    return 0;
}
