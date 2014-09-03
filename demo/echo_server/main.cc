#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sstream>

using std::string;
using std::stringstream;

static string reply_header;

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
    free(pds->post_data);
    free(pds);
}

static int server_loop( void *cls, struct MHD_Connection *connection, const char *url,
        const char *method, const char *version,
        const char *upload_data, size_t *upload_data_size, void **con_cls) {
    (void) cls;

    struct MHD_Response *response;
    int ret = MHD_NO;

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
    pds->post_data[pds->post_data_len] = 0;

    string reply = reply_header;

    reply += "hobo\t";

    sleep(20);

    response = MHD_create_response_from_buffer(reply.length(),
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

    fprintf(stderr,"reply:%s\n",reply.c_str());

    free_post_data(pds);
    return ret;
}


int main(int argc,char* argv[])
{
    int port;
    stringstream stream;
    stream << argv[1];
    stream >> port;

    reply_header = argv[1];

    fprintf(stderr,"port=%d\n",port);

    struct MHD_Daemon *d;
    d = MHD_start_daemon( MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
                         //MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG | MHD_USE_POLL,
                        // MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG,
                        port, //port
                        NULL, NULL, &server_loop, NULL, 

                        MHD_OPTION_END);

    if (d == NULL){
        fprintf(stderr,"Failed to start echo_server\n");
        return 1;
    }

    while(getchar() != 'q'){
        sleep(5);
    }

    MHD_stop_daemon (d);
    return 0;
}
