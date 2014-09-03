#include "agent_state.h"

agent_state::agent_state(){
    playing = false;
    game_id = "";
    thinking = false;
}

agent_state::~agent_state(){
    pthread_mutex_destroy(&lock);
}

void agent_state::init(FILE* fin,FILE* fout){
    server_in = fin;
    server_out = fout;

    assert( pthread_mutex_init(&lock,NULL) == 0 );
}

string agent_state::handle_request(const char* data, int len, bool dist){
    string req = data;
    message* msg = parser::parse_message(req);

    string ret = "";

    fprintf(stderr,"\n\n##########################\n");
    slog("agent_state","data:\n");
    fprintf(stderr,"%s\n",data);
    fprintf(stderr,"##########################\n\n\n");

    switch(msg->type()){
        case INFO:
        {
            pthread_mutex_lock(&lock);
            if(playing){
                ret = "((status busy))";
            } else {
                ret = "((status available))";
            }
            pthread_mutex_unlock(&lock);

            break;
        }
        case PREVIEW: //ignore for now
        {
            ret = "ready";

            break;
        }
        case START:
        {
            bool endb = false;

            pthread_mutex_lock(&lock);
            {
                if(!playing){ //already playing game, cannot start a new one
                    playing = true;
                    game_id = msg->game_id;

                    thinking = true;
                } else {
                    endb = true;
                }
            }
            pthread_mutex_unlock(&lock);

            if(endb){
                ret = "((status busy))";
                goto end;
            }

            ret = communicate_with_agent(data,len,dist);

            pthread_mutex_lock(&lock);
            thinking = false;
            pthread_mutex_unlock(&lock);

            break;
        }
        case PLAY:
        {
            bool endb = false;

            pthread_mutex_lock(&lock);
            {
                if(!playing){ //not playing game, ignoring play message;
                    ret = "ERROR";
                    endb = true;
                } else if(msg->game_id != game_id){ //not the correct game id, ignoring message
                    ret = "((status busy))";
                    endb = true;
                } else if(thinking){
                    ret = "((status busy))";
                    endb = true;
                } else {
                    thinking = true;
                }
            }
            pthread_mutex_unlock(&lock);

            if(endb){
                goto end;
            }

            ret = communicate_with_agent(data,len,dist);

            pthread_mutex_lock(&lock);
            thinking = false;
            pthread_mutex_unlock(&lock);

            break;
        }
        case STOP: case ABORT:
        {
            bool endb = false;
            pthread_mutex_lock(&lock);
            {
                if(!playing){ //not playing game, ignoring play message;
                    ret = "ERROR";
                    endb = true;
                //} else if(msg->game_id != game_id){ //not the correct game id, ignoring message
                    //ret = "((status busy))";
                    //endb = true;
                }
            }
            pthread_mutex_unlock(&lock);

            if(endb){
                goto end;
            }

            ret = communicate_with_agent(data,len,dist);

            pthread_mutex_lock(&lock);
            {
                thinking = false;
                playing = false;
                game_id = "";
            }
            pthread_mutex_unlock(&lock);

            break;
        }
    }

end:
    if(msg->type() == START){
        start_message* cmsg = (start_message*)msg;
        delete cmsg->game;
    }
    delete msg;

    fprintf(stderr,"\n\n##########################\n");
    slog("agent_state","response:\n");
    fprintf(stderr,"%s\n",ret.c_str());
    fprintf(stderr,"##########################\n\n\n");


    return ret;
}


string agent_state::communicate_with_agent(const char* post_data,int post_data_len,bool dist){
    if(!dist){
        fprintf(server_out,"DATA %d\n",post_data_len);
    } else {
        fprintf(server_out,"DIST %d\n",post_data_len);
    }
    fprintf(server_out,"%s\n",post_data);
    fflush(server_out);

    char control_buf[100];
    char cmd[10];
    int len;

    if(!readbuf(server_in,control_buf,sizeof(controller))){
        serror("agent_state", "error when reading DATA from server_in\n");

        serror("agent_state", "exiting\n");
        exit(1);
    }

    serror("agent_state","control_buf:%s[%d]\n",control_buf,strlen(control_buf));

    if(sscanf(control_buf,"%s %d",cmd,&len) != 2){
        serror("agent_state", "error when parsing message from server_in\n");
        serror("agent_state", "the errneous message is:%s\n",control_buf);
        
        serror("agent_state", "exiting\n");
        exit(1);
    }

    char* reply = (char*)malloc(sizeof(char)*(len+10));
    if(!readbuf(server_in,reply,len+1)){
        serror("agent_state", "error when reading reply from server_in\n");
        free(reply);

        serror("agent_state", "exiting\n");
        exit(1);
    }

    serror("agent_state","------\n");
    serror("agent_state","reply:%s[%d]\n",reply,strlen(reply));
    serror("agent_state","------\n\n");

    string ret = reply;

    free(reply);

    return ret;
}
