#include "propnet_factory.h"
#include "slog.h"
#include "parser.h"
#include <stack>
#include <algorithm>
#include <iterator>

//#define PROPNET_FACTORY_CHECK_CONSISTENCY
// @TODO: DE_MORGAN transformation fails on 3pffa, find out why
//#define USE_DEMORGAN 

using std::stack;
using std::pair;

propnet_factory::propnet_factory(gdl* g,int sc)
:game(g)
,flat_game(NULL)
,setup_clock(sc)
{}

propnet_factory::~propnet_factory(){
    if(flat_game)
        delete flat_game;
}

vector<node*> propnet::all_nodes;
unordered_map<pred_wrapper, node*> propnet::pred_map;
set<node*> propnet::bases;
set<node*> propnet::inits;
set<node*> propnet::nexts;
set<node*> propnet::does;
set<node*> propnet::legals;
set<node*> propnet::goals;
node* propnet::term = NULL;
set<node*> propnet::conns;
node* propnet::const_true = NULL;
node* propnet::const_false = NULL;
vector<string> propnet::_roles;
vector2d<int>::type propnet::compac_outputs;
vector<int> propnet::compac_inputs_count;
vector<int> propnet::compac_input;
vector<node_type> propnet::compac_types;
unordered_set<int> propnet::marked_legals;

propnet* propnet_factory::create_propnet(const char* kif){
    if(!flat_game){
        timer* T = new timer();
        bool ret = flatten(kif);
        double dur = T->time_elapsed();
        delete T;

        fprintf(stderr,"flatten duration:%lf\n",dur);

        if(!ret){
            serror("propnet_factory","failed to flatten kif:%s\n",kif);
            return NULL;
        }
    }

    propnet::clear_propnet();

    if(!build_net()){
        serror("propnet_factory","failed to build_net for kif:%s\n",kif);
        return NULL;
    }

    propnet* ret = new propnet();
    return ret;
}

bool propnet_factory::flatten(const char* kif){
    //using timeout to limit flatten time
    stringstream scmd;
    scmd << "timeout " << setup_clock << " ./flatten " << kif;
    string cmd = scmd.str();

    fprintf(stderr,"executing command:%s\n",cmd.c_str());

    stringstream result;
    char buffer[100];
    FILE* pipe = popen(cmd.c_str(),"r");
    while(fgets(buffer,sizeof(buffer),pipe)){
        result << buffer;
    }

    int retcode = fclose(pipe)/256;
    slog("propnet_factory","return code:%d\n",retcode);

    if(retcode != 0){
        return false;
    }

    flat_game = parser::parse_game(result.str());
    if(!flat_game){
        return false;
    }

    return true;
}

static bool special_pred(const string& pred){
    return pred == "base" || pred == "init" || pred == "input"
        || pred == "legal" || pred == "next" || pred == "goal"
        || pred == "terminal";
}

static bool special_pred1(const string& pred){
    return pred == "base" || pred == "init" || pred == "input";
}

bool propnet_factory::build_net(){
    add_const_true_and_false();

    for(int di=0;di<flat_game->desc.size();di++){
        gdl_elem* elem = flat_game->desc[di];
        if(elem->contains<gdl_term*>()){
            gdl_term* t = elem->get<gdl_term*>();

            add_node(t);
        } else if(elem->contains<gdl_rule*>()){
            gdl_rule* r = elem->get<gdl_rule*>();

            add_node(r->head);
            for(int i=0;i<r->tails.size();i++){
                if(r->tails[i]->get_pred() == "distinct") continue;

                add_node(r->tails[i]);
            }
        }
    }


    timer* T = new timer();

    //adding wires to nodes
    for(int di=0;di<flat_game->desc.size();di++){
        gdl_elem* elem = flat_game->desc[di];

        if(elem->contains<gdl_rule*>()){
            gdl_rule* r = elem->get<gdl_rule*>();

            if(special_pred1(r->head->get_pred())){
                continue;
            }

            node* head_node = get_node(r->head);
            vector<node*> tail_nodes;
            for(int i=0;i<r->tails.size();i++){
                gdl_term* tail = r->tails[i];

                if(tail->get_pred() == "distinct") continue;

                if(tail->get_pred() == "not"){
                    tail_nodes.push_back(not_in_tail(tail));
                } else {
                    tail_nodes.push_back(get_node(tail));
                }
            }

            and_node* and_bundle = new_and_node();
            for(int i=0;i<tail_nodes.size();i++){
                and_bundle->inputs.insert(tail_nodes[i]);
                tail_nodes[i]->outputs.insert(and_bundle);
            }

            or_node* or_bundle = NULL;
            if(head_node->inputs.size() == 0){
                or_bundle = new_or_node();

                or_bundle->outputs.insert(head_node);
                head_node->inputs.insert(or_bundle);
            } else {
                or_bundle = (or_node*)*(head_node->inputs.begin());
            }

            or_bundle->inputs.insert(and_bundle);
            and_bundle->outputs.insert(or_bundle);
        } else if(elem->contains<gdl_term*>()){
            gdl_term* t = elem->get<gdl_term*>();

            string pred = t->get_pred();
            if(pred == "role" || pred == "base" || pred == "input" || pred == "init"){
                continue;
            }

            node* n = get_node(t);

            or_node* or_bundle = NULL;
            if(n->inputs.size() == 0){
                or_bundle = new_or_node();

                or_bundle->outputs.insert(n);
                n->inputs.insert(or_bundle);
            } else {
                or_bundle = (or_node*)*(n->inputs.begin());
            }

            propnet::const_true->outputs.insert(or_bundle);
            or_bundle->inputs.insert(propnet::const_true);
        }
    }

    double dur = T->time_elapsed();
    delete T;
    fprintf(stderr,"adding wire time:%lf\n",dur);


    T = new timer();
    simplify();
    dur = T->time_elapsed();
    delete T;
    fprintf(stderr,"simplify time:%lf\n",dur);

#ifdef PROPNET_FACTORY_CHECK_CONSISTENCY
    T = new timer();
    if(!check_consistency()){
        serror("propnet_factory","check_consistency failed!\n");
        return false;
    }
    dur = T->time_elapsed();
    delete T;
    fprintf(stderr,"first check_consistency time:%lf\n",dur);
#endif
    
    propnet::handle_isolated_nodes();

#ifdef PROPNET_FACTORY_CHECK_CONSISTENCY
    T = new timer();
    if(!check_consistency()){
        serror("propnet_factory","check_consistency failed!\n");
        return false;
    }
    dur = T->time_elapsed();
    delete T;
    fprintf(stderr,"second check_consistency time:%lf\n",dur);
#endif

    build_compac();

    mark_legal_nodes();

    return true;
}

void propnet_factory::build_compac(){
    for(int i=0;i<propnet::all_nodes.size(); i++){
        node* n = propnet::all_nodes[i];

        //building vector2d<int>::type compac_outputs;
        propnet::compac_outputs.push_back(vector<int>());
        for(auto it = n->outputs.begin(); n->outputs.end() != it; it++){
            node* n_out = *it;
            propnet::compac_outputs[i].push_back(n_out->id);
        }

        //building vector<int> compac_inputs_count;
        propnet::compac_inputs_count.push_back(n->inputs.size());

        //building vector<int> compac_input;
        if(n->inputs.size() == 1){
            node* source = *(n->inputs.begin());
            propnet::compac_input.push_back(source->id);
        } else {
            propnet::compac_input.push_back(-1);
        }

        //building vector<node_type> compac_types;
        propnet::compac_types.push_back(n->type());
    }
}

static bool ast_is_prop_node(const gdl_ast* ast,node_type* typ){
    string pred = ast->get_pred();
    bool ret = false;

    if(pred == "base" || pred == "init" || pred == "true"){
        ret = true;
        *typ = BASE;
    } else if(pred == "next"){
        ret = true;
        *typ = NEXT;
    }

    return ret;
}

static gdl_term* base_pred_from_ast(const gdl_ast* ast){
    if(ast->get_pred() == "next"){
        return new gdl_term(ast);
    }

    gdl_ast* nt = new gdl_ast();
    nt->children.push_back(new gdl_ast("true"));
    nt->children.push_back(new gdl_ast(*ast->children[1]));

    gdl_term* ret = new gdl_term(nt);
    
    delete nt;
    return ret;
}

static bool ast_is_action_node(const gdl_ast* ast,node_type* typ){
    string pred = ast->get_pred();
    bool ret = false;

    if(pred == "legal"){
        ret = true;
        *typ = LEGAL;
    } else if(pred == "input" || pred == "does"){
        ret = true;
        *typ = DOES;
    }

    return ret;
}

static gdl_term* action_pred_from_ast(const gdl_ast* ast){
    string pred = ast->get_pred();

    if(pred == "legal" || pred == "does"){
        return new gdl_term(ast);
    }

    gdl_term* ret = new gdl_term(ast);
    ret->ast->children[0]->set_sym("does");

    return ret;
}

node* propnet_factory::add_node(const gdl_term* t){
    const gdl_ast* ast = t->ast;
    if(ast->get_pred() == "not"){
        ast = ast->children[1];
    }

    node* n = NULL;
    bool new_node = false;
    node_type ntype = BASE; //just to silence gcc warning
    if(ast_is_prop_node(ast,&ntype)){ //BASE & NEXT only
        gdl_term* nt = base_pred_from_ast(ast); //new
        pred_wrapper pw(nt);

        if(propnet::pred_map.find(pw) == propnet::pred_map.end()){
            prop_node* cn = new prop_node(ast->children[1],ntype); //new
            new_node = true;
            n = cn;

            propnet::pred_map.insert(pair<pred_wrapper,node*>(pw,n));

            //update special subsets
            if(n->type() == BASE){
                propnet::bases.insert(n);
            } else if(n->type() == NEXT){
                propnet::nexts.insert(n);
            }
        } else {
            n = propnet::pred_map[pw];

            delete nt;
        }

        if(ast->get_pred() == "init"){
            propnet::inits.insert(n);
        }
    } else if(ast_is_action_node(ast,&ntype)){
        gdl_term* nt = action_pred_from_ast(ast); //new
        pred_wrapper pw(nt);

        if(propnet::pred_map.find(pw) == propnet::pred_map.end()){
            string role = ast->children[1]->sym();
            action_node* cn = new action_node(ast->children[1]->sym(),ast->children[2],ntype); //new
            new_node = true;
            n = cn;

            propnet::pred_map.insert(pair<pred_wrapper,node*>(pw,n));

            //update special subsets
            if(n->type() == LEGAL){
                propnet::legals.insert(n);
            } else if(n->type() == DOES){
                propnet::does.insert(n);
            }
        } else {
            n = propnet::pred_map[pw];

            delete nt;
        }
    } else if(ast->get_pred() == "goal"){
        gdl_term* nt = new gdl_term(ast); //new
        pred_wrapper pw(nt);

        if(propnet::pred_map.find(pw) == propnet::pred_map.end()){
            string role = ast->children[1]->sym();
            int gv = char_to_int(ast->children[2]->sym());

            goal_node* cn = new goal_node(role,gv); //new
            new_node = true;
            n = cn;

            propnet::pred_map.insert(pair<pred_wrapper,node*>(pw,n));

            //update special subsets
            propnet::goals.insert(n);
        } else {
            n = propnet::pred_map[pw];

            delete nt;
        }
    } else if(ast->get_pred() == "terminal"){
        gdl_term* nt = new gdl_term(ast); //new
        pred_wrapper pw(nt);

        if(propnet::pred_map.find(pw) == propnet::pred_map.end()){
            prop_node* cn = new prop_node(ast,TERM); //new
            new_node = true;
            n = cn;

            propnet::pred_map.insert(pair<pred_wrapper,node*>(pw,n));

            //update special subsets
            propnet::term = n;
        } else {
            n = propnet::pred_map[pw];

            delete nt;
        }
    } else { //ordinary view proposition
        gdl_term* nt = new gdl_term(ast); //new
        pred_wrapper pw(nt);

        if(propnet::pred_map.find(pw) == propnet::pred_map.end()){
            prop_node* cn = new prop_node(ast,VIEW); //new
            new_node = true;
            n = cn;

            propnet::pred_map.insert(pair<pred_wrapper,node*>(pw,n));
        } else {
            n = propnet::pred_map[pw];

            delete nt;
        }

        if(ast->get_pred() == "role"){
            string role = ast->children[1]->sym();
            if(std::find(propnet::_roles.begin(), propnet::_roles.end(),role) == propnet::_roles.end()){
                propnet::_roles.push_back(role);
            }
        }
    }

    if(new_node){
        n->id = propnet::all_nodes.size();
        propnet::all_nodes.push_back(n);
    }

    return n;
}

node* propnet_factory::get_node(const gdl_term* t){
    return add_node(t);
}

node* propnet_factory::not_in_tail(const gdl_term* t){
    assert(!strcmp(t->ast->children[0]->sym(),"not"));

    const gdl_ast* pred_ast = t->ast->children[1];
    gdl_term* tmp = new gdl_term(pred_ast);
    node* pred_node = get_node(tmp);

    not_node* neg = new_not_node();
    neg->inputs.insert(pred_node);
    pred_node->outputs.insert(neg);

    delete tmp;
    return neg;
}

and_node* propnet_factory::new_and_node(){
    and_node* ret = new and_node();
    ret->id = propnet::all_nodes.size();
    propnet::all_nodes.push_back(ret);
    propnet::conns.insert(ret);

    return ret;
}

or_node* propnet_factory::new_or_node(){
    or_node* ret = new or_node();
    ret->id = propnet::all_nodes.size();
    propnet::all_nodes.push_back(ret);
    propnet::conns.insert(ret);

    return ret;
}

not_node* propnet_factory::new_not_node(){
    not_node* ret = new not_node();
    ret->id = propnet::all_nodes.size();
    propnet::all_nodes.push_back(ret);
    propnet::conns.insert(ret);

    return ret;
}

void propnet_factory::add_const_true_and_false(){
    gdl_ast* true_ast = new gdl_ast("z_true_z"); //new
    gdl_ast* false_ast = new gdl_ast("z_false_z"); //new

    propnet::const_true = new prop_node(true_ast,VIEW);
    propnet::const_false = new prop_node(false_ast,VIEW);

    propnet::const_true->id = propnet::all_nodes.size();
    propnet::all_nodes.push_back(propnet::const_true);

    propnet::const_false->id = propnet::all_nodes.size();
    propnet::all_nodes.push_back(propnet::const_false);

    delete true_ast;
    delete false_ast;
}

void propnet_factory::rewrite_id(){
    for(int i=0;i<propnet::all_nodes.size();i++){
        node* n = propnet::all_nodes[i];
        n->id = i;
    }
}

void propnet_factory::simplify(){
//simplify connectives
    int change = 1;
    while(change){
        change = 0;

        for(auto it = propnet::conns.begin(); propnet::conns.end() != it; it++){
            node* n = *it;
            if(n->inputs.size() == 0 && n->outputs.size() == 0) continue;

            switch(n->type()){
                case AND: case OR:
                {
                    if(n->inputs.size() == 1){ //erase redundant AND & OR
                        change = 1;

                        node* source = *(n->inputs.begin());

                        source->outputs.erase(n);
                        n->inputs.clear();

                        for(auto it = n->outputs.begin(); n->outputs.end() != it; it++){
                            node* nn = *it;
                            nn->inputs.erase(n);

                            nn->inputs.insert(source);
                            source->outputs.insert(nn);
                        }
                        n->outputs.clear();
                    } else {
                        if(n->type() == OR){
#ifndef USE_DEMORGAN
                            continue;
#endif

                            if(n->inputs.size() <= 1) continue;

                            bool all_inputs_are_and = true;
                            for(auto it = n->inputs.begin(); n->inputs.end() != it; it++){
                                node* n_in = *it;
                                if(n_in->type() != AND){
                                    all_inputs_are_and = false;
                                    break;
                                }
                            }

                            if(all_inputs_are_and){ //de morgan's law
                                node* first_input = *(n->inputs.begin());
                                set<node*> common_inputs = first_input->inputs;

                                bool has_common_inputs = true;
                                for(auto it = n->inputs.begin(); n->inputs.end() != it; it++){
                                    node* n_in = *it;

                                    set<node*> result;
                                    std::set_intersection(common_inputs.begin(),common_inputs.end(),
                                                          n_in->inputs.begin(),n_in->inputs.end(),
                                                          std::inserter(result,result.begin()));

                                    if(result.size() == 0){
                                        has_common_inputs = false;
                                        break;
                                    }
                                    common_inputs = result;
                                }

                                if(has_common_inputs){
                                    fprintf(stderr,"DE MORGAN!\n");

                                    for(auto it = n->inputs.begin(); n->inputs.end() != it; it++){
                                        node* n_in = *it;

                                        set<node*> result;
                                        std::set_difference(n_in->inputs.begin(),n_in->inputs.end(),
                                                            common_inputs.begin(),common_inputs.end(),
                                                            std::inserter(result,result.begin()));

                                        n_in->inputs = result;

                                        for(auto it2 = common_inputs.begin(); common_inputs.end() != it2; it2++){
                                            node* common_in = *it2;
                                            common_in->outputs.erase(n_in);
                                        }
                                    }

                                    node* common_output = NULL;
                                    if(common_inputs.size() == 1){
                                        change = 1;

                                        common_output = *(common_inputs.begin());
                                    } else { //more than one common_inputs
                                        //change = 1;
                                        continue;

                                        node* common_and = new_and_node();

                                        for(auto it = common_inputs.begin(); common_inputs.end() != it; it++){
                                            node* common_in = *it;

                                            common_in->outputs.insert(common_and);
                                            common_and->inputs.insert(common_in);
                                        }

                                        common_output = common_and;
                                    }

                                    node* new_and = new_and_node();
                                    new_and->inputs.insert(common_output);
                                    common_output->outputs.insert(new_and);

                                    for(auto it = n->outputs.begin(); n->outputs.end() != it; it++){
                                        node* n_out = *it;
                                        n_out->inputs.erase(n);
                                    }
                                    set<node*> n_outputs = n->outputs;
                                    n->outputs.clear();

                                    new_and->inputs.insert(n);
                                    n->outputs.insert(new_and);

                                    for(auto it = n_outputs.begin(); n_outputs.end() != it; it++){
                                        node* n_out = *it;
                                        new_and->outputs.insert(n_out);
                                        n_out->inputs.insert(new_and);
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
                default:
                {
                    //collapse multiple NOT
                    set<node*> not_outputs;
                    for(auto it = n->outputs.begin();n->outputs.end() != it; it++){
                        node* n_out = *it;
                        if(n_out->type() == NOT){
                            not_outputs.insert(n_out);
                        }
                    }
                    if(not_outputs.size() > 1){
                        change = 1;

                        set<node*> other_outputs;
                        std::set_difference(n->outputs.begin(),n->outputs.end(),
                                            not_outputs.begin(),not_outputs.end(),
                                            std::inserter(other_outputs,other_outputs.end()));
                        n->outputs = other_outputs;

                        node* single_not = *(not_outputs.begin());
                        not_outputs.erase(single_not);
                        for(auto it = not_outputs.begin(); not_outputs.end() != it; it++){
                            node* n2 = *it;
                            single_not->outputs.insert(n2->outputs.begin(),n2->outputs.end());

                            n2->inputs.clear();
                            n2->outputs.clear();
                        }
                        n->outputs.insert(single_not);
                    }
                    break;
                }
            }
        }
    }

    vector<node*> conn_nodes;
    for(int i=0;i<propnet::all_nodes.size();i++){
        node* n = propnet::all_nodes[i];
        if(propnet::conns.find(n) != propnet::conns.end()){
            if(n->inputs.size() == 0 && n->outputs.size() == 0){
                propnet::conns.erase(n);

                delete n;
            } else {
                conn_nodes.push_back(n);
            }
        } else {
            conn_nodes.push_back(n);
        }
    }

    propnet::all_nodes = conn_nodes;
    rewrite_id();
}

void propnet_factory::mark_legal_nodes(){ //assume all id's are fixed now
    stack<int> Q;

    set<int> term_and_goals;
    set<int> marked;

    //test
    //for(auto it = propnet::goals.begin(); propnet::goals.end() != it; it++){
        //int id = (*it)->id;
        //fprintf(stderr,"goal node[%d]:",id);
        //print_node_id(id);
    //}

    for(int i=0;i<propnet::all_nodes.size();i++){
        node* n = propnet::all_nodes[i];
        //test
        //fprintf(stderr,"[%d]:",n->id);
        //print_node_id(n->id);

        if( (n == propnet::term) || (propnet::goals.find(n) != propnet::goals.end())){
            //test
            //fprintf(stderr,"[%d]: got in!\n",n->id);

            marked.insert(n->id);
            term_and_goals.insert(n->id);
            Q.push(n->id);
        }
    }

    while(Q.size() > 0){
        int cur = Q.top();
        Q.pop();

        node* n = propnet::all_nodes[cur];
        for(auto it = n->inputs.begin(); n->inputs.end() != it; it++){
            node* n_in = *it;
            if(marked.find(n_in->id) == marked.end()){
                marked.insert(n_in->id);
                Q.push(n_in->id);
            }
        }
        //for(auto it = n->outputs.begin(); n->outputs.end() != it; it++){
            //node* n_out = *it;
            //if(marked.find(n_out->id) == marked.end()){
                //marked.insert(n_out->id);
                //Q.push(n_out->id);
            //}
        //}

        switch(n->type()){
            case BASE:
            {
                node* next_n = propnet::nexts_from_base((prop_node*)n);
                if(next_n){
                    if(marked.find(next_n->id) == marked.end()){
                        marked.insert(next_n->id);
                        Q.push(next_n->id);
                    }
                }
                break;
            }
            case DOES:
            {
                node* legal_n = propnet::legals_from_does((action_node*)n);
                if(legal_n){
                    if(marked.find(legal_n->id) == marked.end()){
                        marked.insert(legal_n->id);
                        Q.push(legal_n->id);
                    }
                }
                break;
            }
            default:
            {
                break;
            }
        }
    }

    //test
    fprintf(stderr,"relevant legals:\n");
    for(auto it = propnet::legals.begin(); propnet::legals.end() != it; it++){
        node* n = *it;
        if(marked.find(n->id) != marked.end()){
            propnet::marked_legals.insert(n->id);

            //test
            print_node_id(n->id);
        }
    }
}

bool check_node_consistency(node* n){
    for(auto it = n->inputs.begin(); n->inputs.end() != it; it++){
        node* n_in = *it;
        if(n_in->outputs.find(n) == n_in->outputs.end()){
            assert(false);
            return false;
        }
    }
    for(auto it = n->outputs.begin(); n->outputs.end() != it; it++){
        node* n_out = *it;
        if(n_out->inputs.find(n) == n_out->inputs.end()){
            assert(false);
            return false;
        }
    }

    return true;
}

bool propnet_factory::check_consistency(){
    for(int i=0;i<propnet::all_nodes.size();i++){
        node* n = propnet::all_nodes[i];

        if(!check_node_consistency(n)){
            assert(false);
            return false;
        }

        if(propnet::conns.find(n) == propnet::conns.end()){
            if(n->inputs.size() > 1){
                assert(false);
                return false;
            }
        }

        if(n->type() == NOT){
            if(n->inputs.size() != 1){
                assert(false);
                return false;
            }
        }
    }

    for(int i=0;i<propnet::all_nodes.size();i++){
        node* ni = propnet::all_nodes[i];
        for(int j=i+1;j<propnet::all_nodes.size();j++){
            node* nj = propnet::all_nodes[j];

            if(ni == nj){
                assert(false);
                return false;
            }
        }
    }

    if(propnet::const_true->inputs.size() != 0){
        assert(false);
        return false;
    }
    if(propnet::const_false->inputs.size() != 0){
        assert(false);
        return false;
    }

    return true;
}

bool propnet_factory::topo_sort(){
    vector<node*> topo_order;

    stack<node*> S;

    vector<int> indeg(propnet::all_nodes.size(),0);
    for(int i=0;i<propnet::all_nodes.size();i++){
        node* n = propnet::all_nodes[i];
        indeg[i] = n->inputs.size();

        if(indeg[i] == 0){
            S.push(n);
        }
    }

    while(S.size() > 0){
        node* cur = S.top();
        S.pop();
        topo_order.push_back(cur);

        for(auto it = cur->outputs.begin(); cur->outputs.end() != it; it++){
            node* suc = *it;
            indeg[suc->id]--;

            if(indeg[suc->id] <= 0){
                S.push(suc);
            }
        }
    }

    if(topo_order.size() < propnet::all_nodes.size()) return false;

    propnet::all_nodes = topo_order;
    rewrite_id();

    return true;
}

bool propnet_factory::check_order_consistency(){
    for(int i=0;i<propnet::all_nodes.size();i++){
        node* n = propnet::all_nodes[i];

        for(auto it = n->inputs.begin(); n->inputs.end() != it; it++){
            node* n_in = *it;
            if(n_in->id >= n->id)
                return false;
        }
        for(auto it = n->outputs.begin(); n->outputs.end() != it; it++){
            node* n_out = *it;
            if(n_out->id <= n->id)
                return false;
        }
    }

    return true;
}
