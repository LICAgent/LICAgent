#include "yapcontroller.h"
#include "parser.h"
#include <curl/curl.h>
#include <microhttpd.h>
#include <pthread.h>
#include <stdio.h>
#include <map>
#include <sstream>

using std::map;
using std::pair;
using std::stringstream;

#define CONNECT_TIMEOUT 2
#define NORMAL_MSG_TIMEOUT 0

vector<string> urls;

pthread_mutex_t game_info_lock;
struct{
    string game_id;

    int startclock;
    int playclock;

    int g_clasp_i;
} game_info ;

struct global_data{
    pthread_mutex_t* pin_lock;
    pthread_cond_t* pin_cond;

    int pins; // counter for threads
    map<int,string> responses;
};

struct thread_param_struct{
    int index;
    int timeout;

    const string* postdata;

    global_data* global;
};

struct write_data_param{
    int index;

    global_data* global;

    string data;
    int content_length;
};

void inc_pin(global_data* global);

size_t write_header(char* buffer, size_t size, size_t nmemb, void* userp){
    write_data_param* wdp = (write_data_param*)userp;

    int len = nmemb*size;
    char* key = NULL;
    stringstream content;

    char* line = (char*)malloc(len+10);
    memcpy(line,buffer,len);
    line[len] = 0;

    if(strstr(line,"HTTP") == line){
        goto end;
    }
    if(!isalpha(line[0])){
        goto end;
    }

    int i;
    for(i=0;i<len;i++){
        if(line[i] == ':') break;
    }

    key = (char*)malloc(i+10);
    memcpy(key,line,i);
    key[i] = 0;

    if(strcasecmp(key,"Content-Length") != 0){
        goto end;
    }

    content.str(line+i+1);
    content >> wdp->content_length;

    fprintf(stderr,"index=%d, wdp->content_length=%d\n",wdp->index,wdp->content_length);

end:
    //fprintf(stderr,"%s\n",line);

    free(line);
    if(key){
        free(key);
    }

    return len;
}

size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp){
    write_data_param* wdp = (write_data_param*)userp;
    int index = wdp->index;
    global_data* global = wdp->global;

    char* cbuf = (char*)calloc(nmemb,size+10);
    int len = nmemb*size;
    memcpy(cbuf,buffer,len);
    cbuf[len] = 0;

    wdp->data += cbuf;
    assert(wdp->data.size() <= wdp->content_length);

    if(wdp->data.size() == wdp->content_length){
        fprintf(stderr,"index=%d, data=%s[%d]\n",index,wdp->data.c_str(),wdp->data.size());

        global->responses.insert(std::pair<int,string>(index,wdp->data));
        inc_pin(global);

        delete wdp;
    }

    free(cbuf);
    return len;
}

void inc_pin(global_data* global){
    pthread_mutex_lock(global->pin_lock);
    global->pins++;

    if(global->pins == urls.size()){
        pthread_cond_signal(global->pin_cond);
    }

    pthread_mutex_unlock(global->pin_lock);
}

void* post_to_target(void* t){
    thread_param_struct* tps = (thread_param_struct*)t;
    int index = tps->index;
    const string* postdata = tps->postdata;
    global_data* global = tps->global;
    int timeout = tps->timeout;

    //fprintf(stderr,"[in_thread]tps #%d: 0x%x\n",index,(int)tps);

    delete tps;

    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();

    if(curl){
        curl_easy_setopt(curl, CURLOPT_URL , urls[index].c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS , postdata->c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE , postdata->size());

        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT , CONNECT_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);

        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

        write_data_param* wdp = new write_data_param();
        wdp->index = index;
        wdp->global = global;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,(void*)wdp); //the fourth argument to userp
        curl_easy_setopt(curl, CURLOPT_HEADERDATA,(void*)wdp);

        //setup headers
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: text/acl");
        headers = curl_slist_append(headers, "Sentience-Dist: true");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK){
            fprintf(stderr, "#%d -> curl_easy_perform() failed: %s\n",index,curl_easy_strerror(res));

            delete wdp;

            inc_pin(global);
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } else {
        inc_pin(global);
    }

    pthread_exit(NULL);

    return NULL;
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
    free(pds->post_data);
    free(pds);
}

static string merge_play_results(const vector<string>& results);

static int post_and_agg(const char* data, MHD_Connection* connection){
    string req = data;
    message* msg = parser::parse_message(req);

    switch(msg->type()){
        case INFO: case PREVIEW: case PLAY:
        {
            break;
        }
        case START:
        {
            start_message* cmsg = (start_message*)msg;

            pthread_mutex_lock(&game_info_lock);
            if(game_info.game_id == ""){
                game_info.game_id = cmsg->game_id;
                game_info.startclock = cmsg->startclock;
                game_info.playclock = cmsg->playclock;

                game_info.g_clasp_i = -1;
            }
            pthread_mutex_unlock(&game_info_lock);

            break;
        }
        case STOP: case ABORT:
        {
            pthread_mutex_lock(&game_info_lock);
            if(msg->game_id == game_info.game_id){
                game_info.game_id = "";
                game_info.startclock = game_info.playclock = 0;
                game_info.g_clasp_i = -1;
            }
            pthread_mutex_unlock(&game_info_lock);

            break;
        }
    }

    pthread_mutex_t pin_lock;
    pthread_cond_t pin_cond;

    pthread_mutex_init(&pin_lock,NULL);
    pthread_cond_init(&pin_cond,NULL);

    global_data* global = new global_data();
    global->pin_lock = &pin_lock;
    global->pin_cond = &pin_cond;
    global->pins = 0;

    pthread_t* threads = (pthread_t*)malloc(urls.size() * sizeof(pthread_t));
    for(int i=0;i<urls.size();i++){
        thread_param_struct* tps = new thread_param_struct(); //this is free'd in post_to_target
        tps->index = i;
        tps->postdata = &req;
        tps->global = global;

        tps->timeout = NORMAL_MSG_TIMEOUT;

        pthread_mutex_lock(&game_info_lock);
        if(msg->type() == START){
            tps->timeout = game_info.startclock;
        } else if(msg->type() == PLAY){
            tps->timeout = game_info.playclock;
        }
        pthread_mutex_unlock(&game_info_lock);

        //fprintf(stderr,"tps #%d: 0x%x\n",i,(int)tps);

        pthread_create(&threads[i],NULL,post_to_target,(void*)tps);
    }

    pthread_mutex_lock(&pin_lock);
    while(global->pins < urls.size()){
        pthread_cond_wait(&pin_cond,&pin_lock);
    }
    pthread_mutex_unlock(&pin_lock);

    for(int i=0;i<urls.size();i++){
        pthread_join(threads[i],NULL);
    }
    free(threads);

    fprintf(stderr,"\n\n");

    string reply;
    int ret = MHD_NO;

    if(global->responses.size() > 0){
        //aggregate all responses
        if(msg->type() != PLAY){
            reply = global->responses.begin()->second;

            //@TODO: pick response based on message (see playerchecker)
            fprintf(stderr,"picking the first response:%s\n",reply.c_str());

            for(auto it = global->responses.begin(); it != global->responses.end(); it++){
                fprintf(stderr,"url:%s\n",urls[it->first].c_str());
                fprintf(stderr,"response:%s\n",it->second.c_str());
                fprintf(stderr,"\n");
            }
        } else {
            vector<string> values;
            for(auto it = global->responses.begin(); it != global->responses.end(); it++){
                values.push_back(it->second);
            }

            reply = merge_play_results(values);
        }
    } else {
        reply = "ERROR";
    }

    struct MHD_Response *response;

    fprintf(stderr,"reply: %s\n",reply.c_str());

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

    fprintf(stderr,"ret = %d, MHD_YES = %d, MHD_NO = %d\n",ret,MHD_YES,MHD_NO);

    if(msg->type() == START){
        start_message* cmsg = (start_message*)msg;
        delete cmsg->game;
    }

    delete msg;
    delete global;
    return ret;
}

static int agg_loop( void *cls, struct MHD_Connection *connection, const char *url,
        const char *method, const char *version,
        const char *upload_data, size_t *upload_data_size, void **con_cls) {
    (void) cls;

    int ret = MHD_NO;

    if (0 != strcmp (method, MHD_HTTP_METHOD_POST))
        return MHD_NO;

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

    fprintf(stderr,"=====================================\n");

    ret = post_and_agg(pds->post_data,connection);

    free_post_data(pds);
    return ret;
}

int main(int argc,char* argv[]){
    char buf[100];
    FILE* furl = fopen("urls.txt","r");
    while(fgets(buf,sizeof(buf),furl)){
        assert(buf[strlen(buf)-1] == '\n');
        buf[strlen(buf)-1] = 0;

        urls.push_back(buf);

        fprintf(stderr,"url:%s\n",buf);
    }
    fprintf(stderr,"\n\n");

    pthread_mutex_init(&game_info_lock,NULL);
    game_info.game_id = "";
    game_info.startclock = 0;
    game_info.playclock = 0;

    struct MHD_Daemon *d;
    d = MHD_start_daemon(// MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
                         MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG | MHD_USE_POLL,
                        // MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG,
                        31022, //port
                        NULL, NULL, &agg_loop, NULL, 

                        MHD_OPTION_END);

    if (d == NULL){
        fprintf(stderr,"Failed to muxin server\n");
        return 1;
    }

    while(getchar() != 'q'){
        sleep(5);
    }

    MHD_stop_daemon (d);
    return 0;
}

static void merge_two_mcts_mw_vec(mw_vec* L,const mw_vec* R){
    //it is possible that some node is out of sync but others are not
    //assert(L->size() == R->size());

    for(int i=0;i<L->size();i++){ //L & R might not be in the same order
        for(int j=0;j<R->size();j++){
            gdl_term* tL = L->at(i)->t;
            const gdl_term* tR = R->at(j)->t;

            if(*tL == *tR){
                move_weight* mw1 = L->at(i);
                const move_weight* mw2 = L->at(j);

                double w = mw1->w + mw2->w;
                double Q = 0.0;
                if(w > 0.0){
                    Q = (mw1->Q*mw1->w + mw2->Q*mw2->w) / w;
                }

                mw1->w = w;
                mw1->Q = Q;
            }
        }
    }
}

//protocol:
//
//(<SOURCE = clasp|mcts|single>)
//(move_1 <weight(float)> <utility(float)>)
//(move_2 <weight(float)> <utility(float)>)
//...
//(move_n <weight(float)> <utility(float)>)

//when SOURCE=single, ignore weight and simple choose the move with the biggest utility
//if a single clasp result is present, use this as the answer
//otherwise, do a weighted average and count the move with the largest expected utility

static string merge_play_results(const vector<string>& results){
    vector<mw_vec*> all_mw_vecs;
    vector<string> srcs;

    string ret;

    bool mcts = false;
    bool single = false;
    bool clasp = false;
    map<int,int> clasp_map; // from index of results -> index of all_mw_vecs

    bool busy = false;

    for(int i=0;i<results.size();i++){
        string src;
        mw_vec* vec = move_weight::parse_vec(results[i],src); //new

        if(vec){
            if(src == "clasp"){
                clasp = true;
                int j = srcs.size();

                clasp_map.insert(pair<int,int>(i,j));
            } else if(src == "single"){
                single = true;
            } else if(src == "mcts"){
                mcts = true;
            }

            all_mw_vecs.push_back(vec);
            srcs.push_back(src);
        } else {
            fprintf(stderr,"ERROR in merge_play_results with results:%s\n",results[i].c_str());

            if(results[i] == "((status busy))"){
                busy = true;
            }
        }
    }

    if(all_mw_vecs.size() == 0){
        if(busy) return "((status busy))";

        return "ERROR";
    }

    if(clasp){
        int final_cj = -1;

        auto fi = clasp_map.begin();
        int ci = fi->first;
        int cj = fi->second;

        pthread_mutex_lock(&game_info_lock);
        if(game_info.g_clasp_i == -1){
            game_info.g_clasp_i = ci;
            final_cj = cj;
        } else {
            if(clasp_map.find(game_info.g_clasp_i) != clasp_map.end()){
                final_cj = clasp_map[game_info.g_clasp_i];
            } else { //@NOTE: only a guess, hopefully we don't have to use this one, this relies on the assumption that all clasp instances produce the same plan
                final_cj = cj;
                game_info.g_clasp_i = ci;
            }
        }
        pthread_mutex_unlock(&game_info_lock);

        ret = move_weight::best_move_in_vec(all_mw_vecs[final_cj])->convertS();
    } else if(single){
        assert(!mcts);

        double Q = -1.0;
        for(int i=0;i<all_mw_vecs.size();i++){
            mw_vec* mws = all_mw_vecs[i];

            double cQ;
            string cret = move_weight::best_move_in_vec(mws,&cQ)->convertS();

            if(cQ > Q){
                ret = cret;
                Q = cQ;
            }
        }

        slog("muxin","best single value:%lf\n",Q);
    } else {
        mw_vec* overall = NULL;

        for(int i=0;i<all_mw_vecs.size();i++){
            mw_vec* mws = all_mw_vecs[i];

            if(overall == NULL){
                overall = mws;
            } else {
                merge_two_mcts_mw_vec(overall,mws);
            }
        }

        if(!overall){
            ret = "ERROR";
        } else {
            double Q;
            ret = move_weight::best_move_in_vec(overall,&Q)->convertS();

            slog("muxin","best move value:%lf\n",Q);
        }
    }

    for(int i=0;i<all_mw_vecs.size();i++){
        move_weight::free_vec(all_mw_vecs[i]);
    }

    return ret;
}
