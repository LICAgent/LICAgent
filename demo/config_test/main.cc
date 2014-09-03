#include "common.h"
#include "slog.h"

int main(void){
    configure::initialize("sen.config");
    configure::print_current_config();

    string sval;
    int ival;
    double dval;
    bool bval;

    configure::get_option("AGENT_USE_PROPNET",&bval);
    configure::get_option("MCTS_PROFILE_OUTPUT",&sval);
    configure::get_option("MCTS_NODE_MAX_COUNT",&ival);
    configure::get_option("TEST_DOUBLE",&dval);

    fprintf(stdout,"\n\n\n");
    slog("config","string: %s\n",sval.c_str());
    slog("config","int: %d\n",ival);
    slog("config","double: %lf\n",dval);
    slog("config","bool: %d\n",bval);

    return 0;
}
