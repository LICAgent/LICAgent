%{

#include <deque>
#include "game.h"
using std::deque;

#include "gdl_parse.hh"
#include <assert.h>

static void strlocase(char* s){
    while(*s){
        *s = tolower(*s);
        s++;
    }
}

%}

%option noyywrap

%option reentrant bison-bridge

%%

[a-zA-Z_\-<=0-9?\.\+]+ { strlocase(yytext); yylval->sym = yytext; return ATOM; }
"(" { return LPAREN; }
")" { return RPAREN; }

;.* {}
\n {}
. {}

%%

void reset_lexer(FILE* fp,void* scanner){
    yyrestart(fp,scanner);
    /*assert(fp == yyin);*/
}
