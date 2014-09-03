#include "yapcontroller.h"
#include "Yap/YapInterface.h"
#include <sstream>
#include <iostream>
#include <unordered_set>
#include "slog.h"

using std::stringstream;
using std::cout;
using std::endl;
using std::unordered_set;

static YAP_Term make_term(gdl_ast* t);
static gdl_ast* build_ast(YAP_Term term);

yapcontroller::yapcontroller(gdl* game,const char* ext,const char* pl,const char* compiled){
    //find roles
    gdl_elem* elem;
    vector<gdl_elem*>::iterator it = game->desc.begin();
    while(it != game->desc.end()){
        elem = *it;
        if(elem->contains<gdl_term*>()){
            gdl_term* term = elem->get<gdl_term*>();
            gdl_ast* ast = term->ast;

            if(ast->type() == AST_LIST && ast->children.size() == 2 && !strcmp(ast->children[0]->sym(),"role")){
                _roles.push_back(ast->children[1]->sym());
            }
        }

        it++;
    }

    //compile pl file
    stringstream cmd;
    cmd<<"yap -g \"compile(['" << ext << "', '" << pl << "']), save_program('" << compiled <<  "'), halt.\"";
    fprintf(stderr,"cmd:%s\n",cmd.str().c_str());

    if(system(cmd.str().c_str()) != 0){
        fprintf(stderr,"yap compilation failed!\n");
        //WTF?
    }

    _compiled = compiled;
    _game = game;
}

yapcontroller::~yapcontroller(){ }

bool yapcontroller::init(){
    using namespace std;

    if(!controller::init()){
        return false;
    }

    //slog("yapcontroller","initializing YAP\n");

    char* save = strdup(_compiled.c_str());

    //slog("yapcontroller","save=%s\n",save);

#ifdef YAP_USE_FASTINIT
    //cout<<"calling YAP_FastInit"<<endl;

    if(YAP_FastInit(save) == YAP_BOOT_ERROR){
        fprintf(stderr,"YAP_FastInit error, exiting\n");
        return false;
    }
#else
    YAP_init_args initArgs;
    memset(&initArgs,0,sizeof(initArgs));

    initArgs.SavedState = save;
    initArgs.HeapSize = 0;
    initArgs.StackSize = 0;
    initArgs.TrailSize = 0;
    initArgs.NumberWorkers = 1;
    initArgs.SchedulerLoop = 0;
    initArgs.DelayedReleaseLoad = 0;
    initArgs.Argc = 1;
    char* argv[] = {"yap",""};
    initArgs.Argv = argv;

    cout<<"calling YAP_Init"<<endl;

    if ( YAP_Init( &initArgs ) == YAP_BOOT_ERROR ){
        fprintf(stderr,"YAP_Init error, exiting\n");
        if(initArgs.ErrorCause)
            fprintf(stderr,"Error cause:%s\n",initArgs.ErrorCause);

        return false;
    }
#endif

    free(save);

    YAP_Reset();
    prove_init_state();

    return true;
}

int yapcontroller::prove_init_state(){
    int okay = YAP_RunGoal( YAP_MkAtomTerm( YAP_LookupAtom("state_initialize")  ) ) != 0;
    YAP_Reset();   
    return okay;
}

bool yapcontroller::terminal(){
    int okay = YAP_RunGoal( YAP_MkAtomTerm( YAP_LookupAtom("state_is_terminal")  ) ) != 0;
    YAP_Reset();   
    return okay;
}

int yapcontroller::goal(const string& role){
	// Prolog call: ?- state_goal( Player, Value ).
    YAP_Functor yapFnct = YAP_MkFunctor( YAP_LookupAtom("state_goal"), 2 );
    YAP_Term player = YAP_MkAtomTerm(YAP_LookupAtom(role.c_str()));
	YAP_Term args[2];
	args[0] = player;
	args[1] = YAP_MkVarTerm();
	
    YAP_Term yapGoal = YAP_MkApplTerm( yapFnct, 2, &args[0] );
    long slot = YAP_InitSlot( yapGoal );
	int okay = YAP_RunGoal( yapGoal );
    yapGoal = YAP_GetFromSlot(slot);

	int goal = -1;
    if( okay != 0 ) 
		goal = YAP_IntOfTerm(YAP_ArgOfTerm( 2, yapGoal ));

	YAP_Reset();
    YAP_RecoverSlots(1);

	return goal;
}

bool yapcontroller::legal(const string& role, const gdl_term& move){
	// Prolog call: ?- legal( role, move).
    YAP_Functor yapFnct = YAP_MkFunctor( YAP_LookupAtom("legal"), 2 );
    YAP_Term player = YAP_MkAtomTerm(YAP_LookupAtom(role.c_str()));
    YAP_Term tmove = make_term(move.ast);
	YAP_Term args[2];

	args[0] = player;
	args[1] = tmove;

    YAP_Term yapGoal = YAP_MkApplTerm( yapFnct, 2, &args[0] );
	int okay = YAP_RunGoal( yapGoal );
	YAP_Reset();
    
	return okay;
}

terms* yapcontroller::state(){
    YAP_Functor yapFnct = YAP_MkFunctor( YAP_LookupAtom("state"), 1 );
    
    YAP_Term yapGoal = YAP_MkNewApplTerm( yapFnct, 1 );
    long slot = YAP_InitSlot( yapGoal );
    int okay = YAP_RunGoal( yapGoal );
    yapGoal = YAP_GetFromSlot(slot);

    terms* state = new terms();

    gdl_ast* c = NULL;
    while ( okay != 0 ) 
    {
        c = build_ast( YAP_ArgOfTerm( 1, yapGoal ) );
        state->push_back(new gdl_term(c));
        delete c;

        okay = YAP_RestartGoal();
    }

    YAP_Reset();
    YAP_RecoverSlots(1);

    return state;
}

terms* yapcontroller::get_move(const string& role){
    YAP_Functor yapFnct = YAP_MkFunctor( YAP_LookupAtom("state_gen_moves"), 2 );

    YAP_Term player = YAP_MkAtomTerm(YAP_LookupAtom(role.c_str()));
    YAP_Term* args = NULL;
    
    args = (YAP_Term*)malloc(sizeof(YAP_Term)*(2+2));
    args[0] = player;
    args[1] = YAP_MkVarTerm();

    YAP_Term yapGoal = YAP_MkApplTerm( yapFnct, 2, args );
    free(args);

    long slot = YAP_InitSlot(yapGoal);
    int okay = YAP_RunGoal( yapGoal );

    gdl_ast* c = NULL;

    terms* moves = new terms();
    unordered_set<pred_wrapper>* uset = new unordered_set<pred_wrapper>();

    YAP_Term resAction;

    while ( okay != 0 ) {
        yapGoal = YAP_GetFromSlot(slot);
        resAction = YAP_ArgOfTerm( 2, yapGoal );

        c = build_ast(resAction);
        if(c==NULL) {
            fprintf(stderr,"NULL move\n");
            break;
        }
        
        gdl_term* term = new gdl_term(c);

        if(uset->find(pred_wrapper(term)) == uset->end()){
            moves->push_back(term);
            uset->insert(pred_wrapper(term));
        } else {
            delete term;
        }

        delete c;
        okay = YAP_RestartGoal();
    }

    delete uset;
    
    YAP_Reset();
    YAP_RecoverSlots(1);

    return moves;
}

int yapcontroller::make_move(const terms& moves){
    // Prolog call: ?- state_make_sim_moves( [ [<p>,<m>], ... ] ).
    YAP_Functor fnct = YAP_MkFunctor( YAP_LookupAtom("state_make_sim_moves"), 1 );
    
    int okay = 0;
    int num_roles = moves.size();
    int n;

    if(num_roles <= 0 || num_roles != _roles.size()){
        return 0;
    }

    YAP_Term* args = (YAP_Term*)malloc(sizeof(YAP_Term)*(num_roles+10));
    for(n = 0 ; n < num_roles; ++n) {
        YAP_Term player = YAP_MkAtomTerm(YAP_LookupAtom(_roles[n].c_str()));
        YAP_Term move = make_term(moves[n]->ast);

        args[n] = YAP_MkPairTerm(player, YAP_MkPairTerm(move, YAP_MkAtomTerm( YAP_FullLookupAtom( "[]" ))));
    }

    YAP_Term arg = YAP_MkAtomTerm( YAP_FullLookupAtom( "[]" ) );
    for ( n = (num_roles - 1) ; n >= 0; --n ) {
        arg = YAP_MkPairTerm( args[n], arg);
    }

    YAP_Term yapGoal = YAP_MkApplTerm( fnct, 1, &arg );
    okay = YAP_RunGoal( yapGoal );

    YAP_Reset();
    free(args);
    return okay;
}

int yapcontroller::retract(const terms& s){
    // Prolog call: ?- state_retract_move( <clause_list> ).
    YAP_Functor yapFnct = YAP_MkFunctor( YAP_LookupAtom("state_retract_move"), 1 );

    int num_args = s.size();
    int n;
    int okay = 0;
    YAP_Term* args = (YAP_Term*)malloc(sizeof(YAP_Term)*(num_args+10));

    if(num_args < 0){
        return 0;
    }

    for(n = 0 ; n < num_args ; ++n)
    {
        YAP_Term move = make_term(s[n]->ast);
        args[n] = move;
    }

    YAP_Term arg = YAP_MkAtomTerm( YAP_FullLookupAtom( "[]" ) );
    for ( n = (num_args - 1) ; n >= 0; --n ) {
        arg = YAP_MkPairTerm( args[n], arg);
    }
    YAP_Term yapGoal = YAP_MkApplTerm( yapFnct, 1, &arg );
    okay = YAP_RunGoal( yapGoal );
    assert(okay);

    YAP_Reset();
    free(args);

    return okay;
}

terms* yapcontroller::get_bases(){
    YAP_Functor yapFnct = YAP_MkFunctor( YAP_LookupAtom("base"), 1 );
    
    YAP_Term yapGoal = YAP_MkNewApplTerm( yapFnct, 1 );
    long slot = YAP_InitSlot( yapGoal );
    int okay = YAP_RunGoal( yapGoal );
    yapGoal = YAP_GetFromSlot(slot);

    terms* ret = new terms();

    gdl_ast* c = NULL;
    while ( okay != 0 ) 
    {
        c = build_ast( YAP_ArgOfTerm( 1, yapGoal ) );
        ret->push_back(new gdl_term(c));
        delete c;

        okay = YAP_RestartGoal();
    }

    YAP_Reset();
    YAP_RecoverSlots(1);

    return ret;
}

terms* yapcontroller::get_inputs(const string& role){
    YAP_Functor yapFnct = YAP_MkFunctor( YAP_LookupAtom("input"), 2 );

    YAP_Term player = YAP_MkAtomTerm(YAP_LookupAtom(role.c_str()));
    YAP_Term* args = NULL;
    
    args = (YAP_Term*)malloc(sizeof(YAP_Term)*(2+2));
    args[0] = player;
    args[1] = YAP_MkVarTerm();

    YAP_Term yapGoal = YAP_MkApplTerm( yapFnct, 2, args );
    free(args);

    long slot = YAP_InitSlot(yapGoal);
    int okay = YAP_RunGoal( yapGoal );

    gdl_ast* c = NULL;
    terms* ret = new terms();

    YAP_Term resAction;

    while ( okay != 0 ) {
        yapGoal = YAP_GetFromSlot(slot);
        resAction = YAP_ArgOfTerm( 2, yapGoal );

        c = build_ast(resAction);
        if(c==NULL) {
            fprintf(stderr,"NULL move\n");
            break;
        }
        
        gdl_term* term = new gdl_term(c);
        ret->push_back(term);
        delete c;

        okay = YAP_RestartGoal();
    }
    
    YAP_Reset();
    YAP_RecoverSlots(1);

    return ret;
}

static YAP_Term make_term(gdl_ast* t){
    if(t->type() == AST_ATOM){
        if(isdigit(t->sym()[0])){
            int i = atoi(t->sym());
            return YAP_MkIntTerm(i);
        } else {
            return YAP_MkAtomTerm(YAP_LookupAtom(t->sym()));
        }
    }
		
    YAP_Term term;
    int arity = t->children.size() - 1;
    YAP_Functor fnct = YAP_MkFunctor(YAP_LookupAtom(t->children[0]->sym()), arity);

    vector<gdl_ast*>::iterator it = t->children.begin();
    it++; //start from the second term

    YAP_Term* args = NULL;

    if(arity)
    {
        int i;
        args = (YAP_Term*)malloc(sizeof(YAP_Term)*(arity+10));

        for (i=0; i<arity; i++){
            args[i] = make_term(*it);
            it++;
        }
        term = YAP_MkApplTerm(fnct, arity, args);

        free(args);
    }
    else
        term = YAP_MkApplTerm(fnct, arity, NULL);
	
    return term;
}

static gdl_ast* build_ast(YAP_Term term){
    gdl_ast* c = NULL;

    if(YAP_IsAtomTerm(term)){
        c = new gdl_ast(YAP_AtomName(YAP_AtomOfTerm(term)));
    } else if(YAP_IsIntTerm(term)){
        stringstream buffer;
        buffer << (int)(YAP_IntOfTerm( term ));

        c = new gdl_ast(buffer.str().c_str());
    } else {
        YAP_Functor fnct =  YAP_FunctorOfTerm( term );
        const char* name = YAP_AtomName( YAP_NameOfFunctor( fnct ) );
        
        deque<gdl_ast*> lt;
        lt.push_back(new gdl_ast(name));

        int arity = YAP_ArityOfFunctor(fnct);
        int i;
        for(i = 1; i <= arity; i++) {
            stringstream buffer;
            YAP_Term temp = YAP_ArgOfTerm( i, term );

            if ( YAP_IsIntTerm( temp ) ) {
                buffer << (int)(YAP_IntOfTerm( temp ));
            }
            else if ( YAP_IsFloatTerm( temp ) ) {
                buffer << (float)YAP_FloatOfTerm( temp );
            }
            else if ( YAP_IsAtomTerm( temp ) ) {
                buffer << YAP_AtomName( YAP_AtomOfTerm( temp ) );
            }
            else if(YAP_IsApplTerm(temp)) {
                YAP_Functor fnctInner =  YAP_FunctorOfTerm( temp );
                name = YAP_AtomName( YAP_NameOfFunctor( fnctInner ) );

                deque<gdl_ast*> ltInner;
                ltInner.push_back(new gdl_ast(name));

                int argsizeInner = YAP_ArityOfFunctor(fnctInner);
                for(int n = 1; n <= argsizeInner; ++n)
                {
                    stringstream buffer;

                    YAP_Term tempInner = YAP_ArgOfTerm( n, temp );
                    if ( YAP_IsIntTerm( tempInner ) ) {
                        buffer << (int)(YAP_IntOfTerm( tempInner ));
                    }
                    else if ( YAP_IsFloatTerm( tempInner ) ) {
                        buffer << (float)YAP_FloatOfTerm( tempInner );
                    }
                    else if ( YAP_IsAtomTerm( tempInner ) ) {
                        buffer << YAP_AtomName( YAP_AtomOfTerm( tempInner ) );
                    }
                    else {
                        fprintf(stderr,"unexpected term type!\n");
                        return NULL;
                    }

                    ltInner.push_back(new gdl_ast(buffer.str().c_str()));
                }

                lt.push_back(new gdl_ast(&ltInner));

                continue;
            } else {
                fprintf(stderr,"unknown term type!\n");
                return NULL;
            }

            lt.push_back(new gdl_ast(buffer.str().c_str()));
        }

        c = new gdl_ast(&lt);
    }

    return c;
}
