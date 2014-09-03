#ifndef __GAME_H__
#define __GAME_H__

#include <string>
#include <vector>
#include <deque>
#include <set>
#include <dlib/type_safe_union.h>
#include "config.h"

using std::set;
using std::string;
using std::deque;
using std::vector;

typedef enum{
    AST_ATOM = 1,
    AST_LIST,
} gdl_ast_type;

class gdl_ast{
public:
    gdl_ast();

    gdl_ast(const char*);
    gdl_ast(const deque<gdl_ast*>*);

    gdl_ast(const gdl_ast&);

    ~gdl_ast();

    gdl_ast_type type() const{ return t; }

    unsigned int hash() const;

    const char* sym() const{ return _sym.c_str(); }
    void set_sym(const string& s){ _sym = s; }

    bool operator==(const gdl_ast&) const;
    bool operator!=(const gdl_ast& t) const { return !(*this == t); }

    string get_pred() const;

    bool grounded() const;

    string convert_lp() const;

    string convert() const;
    string convertS() const;

    vector<gdl_ast*> children;
    string _sym;

    gdl_ast_type t;
};

class gdl_term{
public:
    gdl_term(const gdl_ast*);
    gdl_term(const gdl_term&);
    ~gdl_term();

    bool operator==(const gdl_term&) const;
    bool operator!=(const gdl_term& t) const{ return !(*this == t); }

    bool grounded() const{ return ast->grounded(); }

    const char* sym(){ return ast->sym(); }

    string get_pred() const;
    int get_arity() const;

    string convert_lp() const{ return ast->convert_lp(); }
    string convertS() const;
    string convert() const;
    unsigned int hash() const;

    gdl_ast* ast;
};

class gdl_rule{
public:
    gdl_rule(const gdl_ast*);
    gdl_rule(const gdl_rule&);
    ~gdl_rule();

    string convertS() const;
    string convert() const;
    string convert_lp() const;
    
    unsigned int hash() const;

    bool grounded() const{
        if(!head->grounded()) return false;
        for(auto it = tails.begin();tails.end() != it;it++){
            if(!(*it)->grounded()) return false;
        }

        return true;
    }

    gdl_term* head;
    vector<gdl_term*> tails;
};

typedef dlib::type_safe_union<gdl_term*, gdl_rule*> gdl_elem;

class gdl{
public:
    gdl(const gdl_ast*);
    ~gdl();

    void print(FILE* fp);
    void printS(FILE* fp);


    string lparse_ast(const gdl_ast* t);
    string lparse_rule_tail(const gdl_ast* t);
    string lparse_rule(const gdl_rule* t);

    string convert_lparse();
    void compute_tpreds();
    set<string> tpreds;

    vector<gdl_elem*> desc;
};

struct pred_wrapper{
    pred_wrapper(gdl_term* _t)
    :t(_t)
    {}
    ~pred_wrapper(){}

    gdl_term* t;
};

struct rule_wrapper{
    //rule_wrapper(gdl_rule* _r){ r = new gdl_rule(*_r); }
    //~rule_wrapper(){ delete r; }
    rule_wrapper(gdl_rule* _r)
    :r(_r)
    {}
    ~rule_wrapper(){}

    gdl_rule* r;
};

struct pred_index{
    pred_index(string _pred,int _index)
    :pred(_pred)
    ,index(_index)
    { }

    string pred;
    int index;
};

namespace std{
    template<>
    class hash<pred_index>{
    public:
        size_t operator()(const pred_index& p) const;
    };

    template<>
    class equal_to<pred_index>{
    public:
        bool operator()(const pred_index& p1,const pred_index& p2) const;
    };
};


struct pred_form{
    pred_form(gdl_term* _t);

    pred_form(const pred_form& pf){
        t = pf.t;
        nvars = pf.nvars;
    }
    ~pred_form();

    static gdl_term* transform(const gdl_term*);

    gdl_term* t;
    int nvars;
};

namespace std{
    template<>
    class hash<rule_wrapper>{
    public:
        size_t operator()(const rule_wrapper& r) const{
            size_t ret = r.r->head->hash();

            auto tl = r.r->tails.begin();
            while(tl != r.r->tails.end()){
                ret ^= (*tl)->hash();
                tl++;
            }

            return ret;
        }
    };

    template<>
    class equal_to<rule_wrapper>{
    public:
        bool operator()(const rule_wrapper& rr1,const rule_wrapper& rr2) const{
            const gdl_rule* r1 = rr1.r;
            const gdl_rule* r2 = rr2.r;

            if(*(r1->head) != *(r2->head)) return false;
            if(r1->tails.size() != r2->tails.size()) return false;

            for(unsigned int i=0;i<r1->tails.size();i++){ //should we consider an unordered comparison?
                if(*(r1->tails[i]) != *(r2->tails[i])) return false;
            }

            return true;
        }
    };

    template<>
    class hash<pred_wrapper>{
    public:
        size_t operator()(const pred_wrapper& p) const{
            return p.t->hash();
        }
    };

    template<>
    class equal_to<pred_wrapper>{
    public:
        bool operator()(const pred_wrapper& p1,const pred_wrapper& p2) const{
            return *(p1.t) == *(p2.t);
        }
    };

    template<>
    class hash<pred_form>{
    public:
        size_t operator()(const pred_form& pf) const;
    };

    template<>
    class equal_to<pred_form>{
    public:
        bool operator()(const pred_form& pf1, const pred_form& pf2) const;
    };
};

struct move_weight;
typedef vector<move_weight*> mw_vec;

struct move_weight{
    move_weight(const gdl_term* _t,double _w,double _Q){
        w = _w;
        t = new gdl_term(*_t);
        Q = _Q;
    }

    ~move_weight(){
        delete t;
    }

    static string convertS(const mw_vec* mws, const string& src);
    static mw_vec* parse_vec(const string& s, string& src); 

    static void free_vec(mw_vec* mws);
    static const gdl_term* best_move_in_vec(const mw_vec* mws, double* max=NULL);

    gdl_term* t;
    double w;
    double Q;
};

#endif
