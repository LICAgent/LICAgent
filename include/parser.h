#ifndef __PARSER_H__
#define __PARSER_H__

#include "message.h"
#include "slog.h"
#include "config.h"

class parser{
public:
    static message* parse_message(const string& msg);

    static gdl_ast* parse_ast(const string& str);
    static gdl_term* parse_lp(const string& str);

    //for testing purposes
    static gdl* parse_game(const string& msg);
};

#endif
