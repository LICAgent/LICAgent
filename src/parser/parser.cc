#include "parser.h"
#include <sstream>

using std::stringstream;

extern gdl_ast* g_game;
extern int yyparse(void* scanner);
extern void reset_parser(FILE* fp,void* scanner);
extern int yylex_init(void** scanner);
extern int yylex_destroy(void* scanner);

gdl_ast* parser::parse_ast(const string& str){
    char* input =strdup(str.c_str());
    int len = str.size();

    gdl_ast* ret = NULL;
    FILE* sstream = fmemopen(input, len, "r");

    void* scanner = NULL;
    yylex_init(&scanner);
    reset_parser(sstream,scanner);
    if(yyparse(scanner) != 0){
        serror("parser","parsing error\n");
        return NULL;
    }
    yylex_destroy(scanner);

    fclose(sstream);
    free(input);

    ret = g_game;
    return ret;
}

void print_stream(stringstream& stream){
    fprintf(stderr,"%s\n",stream.str().c_str());
}

string get_token(stringstream& stream){
    string ret = "";
    while(stream.good()){
        char c = stream.get();

        if(c == '(' || c == ')' || c == ','){
            if(ret == ""){
                ret += c;
            } else {
                stream.unget();
            }

            break;
        }

        ret += c;
    }

    return ret;
}

void unget_token(stringstream& stream,string s){
    for(int i=0;i<s.size();i++)
        stream.unget();
}

string peak_token(stringstream& stream){
    string ret = get_token(stream);
    unget_token(stream,ret);

    return ret;
}

static gdl_ast* top(stringstream& stream){
    string pred = get_token(stream);

    string next = peak_token(stream);
    if(next != "("){
        return new gdl_ast(pred.c_str());
    }

    gdl_ast* ret = new gdl_ast();
    ret->children.push_back(new gdl_ast(pred.c_str()));

    assert(get_token(stream) == "(");
    while(peak_token(stream) != ")"){
        gdl_ast* sub = top(stream);

        ret->children.push_back(sub);

        if(peak_token(stream) != ")"){
            assert(get_token(stream) == ",");
        }
    }
    assert(get_token(stream) == ")");

    return ret;
}

gdl_term* parser::parse_lp(const string& str){ //we assume that all works are grounded
    stringstream stream(str);
    gdl_ast* ast = top(stream);
    assert(ast);

    gdl_ast* counter = ast->children[ast->children.size() - 1];
    ast->children.pop_back();
    delete counter;

    gdl_term* ret = new gdl_term(ast);
    delete ast;

    return ret;
}

message* parser::parse_message(const string& msg){
    using namespace std;

    gdl_ast* o_ast = parse_ast(msg);
    assert(o_ast->children.size() == 1);

    gdl_ast* ast = o_ast->children[0];
    message* ret = NULL;
    
    string cmd = ast->children[0]->sym();
    //slog("parser","cmd=%s\n",cmd.c_str());

    if(cmd == "info"){
        ret = new info_message();
    } else if(cmd == "preview"){
        ret = new preview_message();
    } else if(cmd == "stop"){
        ret = new stop_message(*(ast->children[2]));
        ret->game_id = ast->children[1]->sym();
    } else if(cmd == "play"){
        ret = new play_message(*(ast->children[2]));
        ret->game_id = ast->children[1]->sym();
    } else if(cmd == "abort"){
        ret = new abort_message();
        ret->game_id = ast->children[1]->sym();
    } else if(cmd == "start"){
        gdl* game = new gdl(ast->children[3]);
        int startclock = atoi(ast->children[4]->sym());
        int playclock = atoi(ast->children[5]->sym());
        const char* role = ast->children[2]->sym();
        ret = new start_message(game,startclock,playclock,role);
        ret->game_id = ast->children[1]->sym();
    }

    delete o_ast;
    return ret;
}

//for testing purposes
gdl* parser::parse_game(const string& msg){
    gdl_ast* ast = parse_ast(msg);
    gdl* ret = new gdl(ast);

    delete ast;
    return ret;
}
