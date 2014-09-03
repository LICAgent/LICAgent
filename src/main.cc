#include <iostream>
#include <sstream>
#include <string>
#include <microhttpd.h>
#include "agent_state.h"
#include "slog.h"
#include "common.h"

using std::stringstream;
using std::string;
using std::cout;
using std::cin;
using std::endl;
using std::exception;

static int sta[2];
static int ats[2];
static int agt_read,agt_write;

static FILE* server_in;
static FILE* server_out;

static void agent_loop(){
    agent agt;
    char control_buf[100];
    char cmd[10];
    int len;

    FILE* agent_in = fdopen(agt_read,"r");
    FILE* agent_out = fdopen(agt_write,"w");

    setvbuf(agent_out,NULL,_IONBF,BUFSIZ);
    setvbuf(agent_in,NULL,_IONBF,BUFSIZ);

    while(1){
        slog("agent","================================================\n");

        if(!readbuf(agent_in,control_buf,sizeof(control_buf))){
            serror("agent", "error when reading DATA from agent_in\n");

            serror("agent", "exiting\n");
            return;
        }

        if(sscanf(control_buf,"%s %d",cmd,&len) != 2){
            serror("agent", "error when parsing message from agent_in\n");
            serror("agent", "the errneous message is:%s[%d]\n",control_buf,strlen(control_buf));

            serror("agent", "exiting\n");
            return;
        }

        char* data = (char*)malloc(sizeof(char)*(len+10));

        if(!readbuf(agent_in,data,len+1)){
            serror("agent", "error when reading data from agent_in\n");
            free(data);

            serror("agent", "exiting\n");
            return;
        }

        bool dist = false;
        if(!strcmp(cmd,"DIST")){
            dist = true;
        }

        string ret = agt.handle_request(data,dist);

        if(ret.length() == 0){
            fprintf(agent_out,"DATA 0\n");

            serror("agent", "ERROR in agent\n");
        } else {
            fprintf(agent_out,"DATA %d\n",ret.length());
            fprintf(agent_out,"%s\n",ret.c_str());
        }
        fflush(agent_out);

        free(data);

        slog("agent","=================================================\n");
    }
}

typedef struct post_data_struct{
    char* post_data;
    int post_data_size;
    int post_data_len;
} post_data_struct;

post_data_struct* new_post_data_struct(){
    post_data_struct* ret = (post_data_struct*)malloc(sizeof(post_data_struct));
    ret->post_data = NULL;
    ret->post_data_len = ret->post_data_size = 0;

    return ret;
}

static void post_iterator(const char* data, size_t size,post_data_struct* pds){
    if(pds->post_data == NULL){
        pds->post_data_size = 1000;
        pds->post_data = (char*)malloc(pds->post_data_size);
        pds->post_data_len = 0;
    }
    if(pds->post_data_len + size + 100 > pds->post_data_size){
        pds->post_data_size = (pds->post_data_len + size)*2;
        pds->post_data = (char*)realloc(pds->post_data,pds->post_data_size);
    }

    memcpy(pds->post_data + pds->post_data_len,data,size);
    pds->post_data_len += size;
}

static void free_post_data(post_data_struct* pds){
    if(pds->post_data){
        free(pds->post_data);
    }
    free(pds);
}

static agent_state agt_state;

static int server_loop( void *cls, struct MHD_Connection *connection, const char *url,
        const char *method, const char *version,
        const char *upload_data, size_t *upload_data_size, void **con_cls) {
    (void) cls;

    struct MHD_Response *response;
    int ret = MHD_NO;

//    if (0 != strcmp (method, MHD_HTTP_METHOD_POST))
//        return MHD_NO;

    if(*con_cls == NULL){
        *con_cls = new_post_data_struct();
        return MHD_YES;
    }

    post_data_struct* pds = (post_data_struct*)(*con_cls);

    if(*upload_data_size){
        post_iterator(upload_data,*upload_data_size,pds);
        *upload_data_size = 0;
        return MHD_YES;
    }

    if(!pds->post_data){
        free_post_data(pds);
        return MHD_NO;
    }

    pds->post_data[pds->post_data_len] = 0;

    bool dist = false;
    if(MHD_lookup_connection_value(connection,MHD_HEADER_KIND,"Sentience-Dist") != NULL){
        dist = true;
    }

    string reply = agt_state.handle_request(pds->post_data,pds->post_data_len,dist);

    if(reply != ""){
        response = MHD_create_response_from_buffer (reply.length(),
                (void *) reply.c_str(),
               MHD_RESPMEM_MUST_COPY);

        //essential for the stanford game manager!
        MHD_add_response_header(response,"Content-Type","text/acl");
        MHD_add_response_header(response,"Access-Control-Allow-Origin","*");
        MHD_add_response_header(response,"Access-Control-Allow-Methods","POST, GET, OPTIONS");
        MHD_add_response_header(response,"Access-Control-Allow-Headers","Content-Type");
        MHD_add_response_header(response,"Access-Control-Allow-Age","86400");

        ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
        MHD_destroy_response (response);
    }

    free_post_data(pds);
    return ret;
}


int main(int argc,char* argv[])
{
    //check Hoard
    char* hoard = getenv("LD_PRELOAD");
    if(hoard != NULL){
        fprintf(stderr,"LD_PRELOAD=%s\n",hoard);
    } else {
        fprintf(stderr,"LD_PRELOAD=\n");
    }

    if(pipe(sta) < 0 || pipe(ats) < 0){
        fprintf(stderr,"pipe failed\n");
        perror("pipe");
        return 1;
    }

    agt_read = sta[0]; //s -> a read
    agt_write = ats[1]; //a -> s write

    pid_t pid = fork();
    if(pid < 0){
        fprintf(stderr,"fork failed\n");
        perror("fork");
        return 1;
    } else if(pid == 0){ //agent process
        close(sta[1]); //[sta] s -> a channel write
        close(ats[0]); //[ats] a -> s channel read
        
        //sleep(5);

        agent_loop();
        return 0;
    } else { //server process
        fprintf(stderr,"child pid=%d\n",pid);

        close(sta[0]); //[sta] s -> a channel read
        close(ats[1]); //[ats] a -> s channel write

        server_in = fdopen(ats[0],"r");
        server_out = fdopen(sta[1],"w");

        setvbuf(server_out,NULL,_IONBF,BUFSIZ);
        setvbuf(server_in,NULL,_IONBF,BUFSIZ);

        assert(server_in && server_out);

        agt_state.init(server_in,server_out);
    }

    struct MHD_Daemon *d;
    d = MHD_start_daemon( // MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
                         MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG | MHD_USE_POLL,
                        // MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG,
                        9147, //port
                        NULL, NULL, &server_loop, NULL, 

                        MHD_OPTION_END);

    if (d == NULL){
        fprintf(stderr,"Failed to start server\n");
        return 1;
    }
    while(getchar() != 'q'){
        sleep(5);
    }

    MHD_stop_daemon (d);
    return 0;
}

