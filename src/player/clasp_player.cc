#include "clasp_player.h"
#include "gdl_simplify.h"

#include <algorithm>
#include <sstream>
using std::stringstream;
using std::endl;
using std::getline;
using std::reverse;

clasp_player::clasp_player(controller* con,gdl* g, const char* _fn,double tl,const timer* _T)
:player(con)
,game(g)
,fn(_fn)
,time_limit(tl)
,T(_T)
{}

clasp_player::~clasp_player(){
    for(int i=0;i<plan.size();i++){
        delete plan[i];
    }
}

bool clasp_player::init(){
    gdl_simplify::del_and_or(game->desc);
    string nt_lp = game->convert_lparse();

    int rl = 100;
    stringstream lp;
    lp << nt_lp;
    lp << "time(1.." << rl << ")." << endl;

    string slp = lp.str();

    fprintf(stdout,"---------------------");
    fprintf(stdout,"converted lparse:\n");
    fprintf(stdout,"%s\n",slp.c_str());

    slog("clasp_player","creating asp problem file(%s):\n",fn);
    FILE* fout = fopen(fn,"w");
    fwrite(slp.c_str(),slp.size(),sizeof(char),fout);
    fclose(fout);

    int tl = time_limit;
    if(tl <= 0) tl = 1;

    stringstream cmds;
    cmds << "timeout " << tl << " python solver.py " << fn;
    string cmd = cmds.str();

    slog("clasp_player","executing command:\n%s\n",cmd.c_str());

    stringstream result;

    char buffer[100];
    FILE* pipe = popen(cmd.c_str(),"r");
    while(fgets(buffer,sizeof(buffer),pipe)){
        result << buffer;
    }
    int retcode = fclose(pipe)/256;
    slog("clasp_player","return code:%d\n",retcode);

    fprintf(stdout,"\n");
    slog("clasp_player","result:\n%s\n",result.str().c_str());

    string planbuf;
    getline(result,planbuf);

    if('A' <= planbuf[0] && planbuf[0] <= 'Z'){
        return false;
    }

    string status;
    getline(result,status);
    if(status != "SATISFIABLE"){
        return false;
    }

    stringstream splan(planbuf);
    string smove;
    while(splan >> smove){
        gdl_term* term = parser::parse_lp(smove);
        fprintf(stderr,"term=%s\n",term->convertS().c_str());

        gdl_term* move = new gdl_term(term->ast->children[2]);
        fprintf(stderr,"move=%s\n",move->convertS().c_str());

        delete term;
        plan.push_back(move);
    }
    reverse(plan.begin(),plan.end());

    //check whether the plan actually works
    terms* init_state = con->state();

    string role = con->roles()[0];
    for(int i=0;i<plan.size();i++){
        const gdl_term* move = plan[i];

        if(!con->legal(role,*move)){
            return false;
        }

        terms* jmove = new terms();
        jmove->push_back(new gdl_term(*move));
        con->make_move(*jmove);

        controller::free_terms(jmove);
    }

    if(!con->terminal()){
        return false;
    }
    int gv = con->goal(role);
    if(gv != 100){
        return false;
    }

    con->retract(*init_state);

    controller::free_terms(init_state);

    return true;
}

void clasp_player::meta_game(const string& game_id,const string& role,int startclock,const timer* T){
    this->role = role;

    round = 0;
}

gdl_term* clasp_player::next_move(int playclock,const terms& moves,const timer* T){
    return new gdl_term(*plan[round++]);
}

mw_vec* clasp_player::next_move_dist(int playclock,const terms& moves,string& src,const timer* T){
    gdl_term* move = plan[round++];

    src = "clasp";

    mw_vec* ret = new mw_vec();
    ret->push_back(new move_weight(move,1.0,100.0));

    return ret;
}

void clasp_player::finish_game(){
    int score = con->goal(role);
    slog("clasp_player","final score:%d\n",score);
}
