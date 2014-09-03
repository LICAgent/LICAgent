#ifndef __AGENT_STATE_H__
#define __AGENT_STATE_H__

#include "agent.h"
#include <string>
#include <vector>
#include <stdio.h>
#include <pthread.h>

using std::vector;
using std::string;

class agent_state{
public:
    agent_state();
    ~agent_state();

    void init(FILE* fin,FILE* fout);

    string handle_request(const char* data,int len,bool dist);

private:
    string communicate_with_agent(const char* post_data,int post_data_len,bool dist);
    
    FILE* server_in;
    FILE* server_out;

    pthread_mutex_t lock;
    bool thinking;

    bool playing;
    string game_id;
};

#endif
