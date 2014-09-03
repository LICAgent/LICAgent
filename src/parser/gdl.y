%{
#include "game.h"
#include "gdl_parse.hh"
#include <deque>
#include <string.h>
#include <assert.h>

using std::deque;

gdl_ast* g_game;

/*void rec_free(deque<gdl_ast*>* lt);*/

extern int yylex ( YYSTYPE * lvalp, yyscan_t scanner );
int yyerror(yyscan_t scanner, const char *msg);

%}

%define api.pure
%lex-param   { yyscan_t scanner }
%parse-param { yyscan_t scanner }

%union{
    gdl_ast* t;
    deque<gdl_ast*>* lt;
    char* sym;
}

%code requires{
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
    typedef void *yyscan_t;
#endif

#define YYMAXDEPTH 1000000
}

%token LPAREN
%token RPAREN
%token ATOM

%type <t> expr ATOM program
%type <lt> expr_list 

%%

program : expr_list
{
    $$ = g_game = new gdl_ast($1);
    delete $1;
}

expr : LPAREN expr_list RPAREN
{
    $$ = new gdl_ast($2);
    delete $2;
}
     | ATOM
{
    $$ = new gdl_ast(yylval.sym);
};

expr_list : expr expr_list
{
    $$ = $2;
    $$->push_front($1);
}
          | expr
{
    $$ = new deque<gdl_ast*>();
    $$->push_back($1);
};

%%

int yyerror(yyscan_t scanner, const char *s){
    fprintf (stderr, "bison/flex error: %s\n", s);
    return 0;
}

extern void reset_lexer(FILE*, void*);

void reset_parser(FILE* fp,void* scanner){
    reset_lexer(fp,scanner);
    /*if(g_game)*/
        /*delete g_game;*/
}

/*void rec_free(deque<gdl_ast*>* lt){*/
    /*deque<gdl_ast*>::iterator it = lt->begin();*/
    /*while(it != lt->end()){*/
        /*delete *it;*/
        /*it++;*/
    /*}*/

    /*delete lt;*/
/*}*/
