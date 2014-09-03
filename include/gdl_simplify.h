#ifndef __GDL_SIMPLIFY__
#define __GDL_SIMPLIFY__

#include "game.h"

class gdl_simplify{
public:
    static void simplify(vector<gdl_elem*>& g);
    static void del_and_or(vector<gdl_elem*>& g);
    static void var_constrain(vector<gdl_elem*>& g);
};

#endif
