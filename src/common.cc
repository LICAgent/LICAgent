#include "common.h"
#include "slog.h"
#include <sys/time.h>
#include <stdlib.h>
#include <sstream>

double rand1(){
    return (double)rand()/(double)RAND_MAX;
}

uint64_t wall_time(){
    struct timeval ts;

    gettimeofday(&ts,NULL);

    uint64_t ret = ts.tv_sec * 1000;
    ret += ts.tv_usec / 1000;

    return ret;
}

timer::timer(uint64_t t){
    start_time = t;
}

timer::timer(){
    start_time = wall_time();
}

timer::~timer(){}

double timer::time_elapsed() const{
    uint64_t now = wall_time();
    return (now - start_time) / 1000.0;
}

int char_to_int(const char* s){
    std::stringstream ss;
    int ret;

    ss << s;
    ss >> ret;

    return ret;
}

int readbuf(FILE* fp,char* buf,size_t n){
    //while(buf[0] == '\n' || strlen(buf) < n){ //new line
    //if(getline(&buf,&len,fp) == -1) return 0;
    
    int i = 0;

    char c = fgetc(fp);
    if(c == '\n') c = fgetc(fp);
    if(c == EOF) return 0;

    buf[i++] = c;
    while((c = fgetc(fp)) != '\n'){
        if(c == EOF)
            return 0;

        buf[i++] = c;
    }
    buf[i] = 0;

    return 1;
}

typedef std::pair<string,option_type*> option_item;
map<string,option_type*> configure::option_map;

void configure::initialize(const char* config){
    dealloc();

    fprintf(stderr,"configure file:%s\n",config);

    FILE* fp = fopen(config,"r");
    if(!fp){
        serror("common","cannot open configure file '%s'\n",config);
        exit(1);
    }

    char* buf = (char*)malloc(100);

    while(fgets(buf,100,fp)){
        if(buf[0] == '#') continue;

        int len = strlen(buf);

        assert(buf[len-1] == '\n');

        if(len <= 1 || buf[0] == '#'){ //comment or blank line
            continue;
        }

        string type,key;
        std::stringstream stream(buf);
        stream >> type >> key;

        if(type == "bool"){
            string val;
            stream >> val;

            if(val == "false"){
                option_map.insert(option_item(key, new option_type((bool)false)));
            } else {
                option_map.insert(option_item(key, new option_type((bool)true)));
            }

            assert(option_map[key]->contains<bool>());
        } else if(type == "double"){
            double val;
            stream >> val;

            option_map.insert(option_item(key, new option_type((double)val)));

            assert(option_map[key]->contains<double>());
        } else if(type == "string"){
            string val;
            stream >> val;

            option_map.insert(option_item(key, new option_type(val)));

            assert(option_map[key]->contains<string>());
        } else if(type == "int"){
            int val;
            stream >> val;

            option_map.insert(option_item(key, new option_type((int)val)));

            assert(option_map[key]->contains<int>());
        }
    }

    free(buf);
}

void configure::print_current_config(){
    for(auto it = option_map.begin(); it != option_map.end(); it++){
        string key = it->first;
        option_type* val = it->second;

        slog("common","key = %s -> ",key.c_str());
        if(val->contains<bool>()){
            if(val->get<bool>()){
                slog("common","value = true[bool]\n");
            } else {
                slog("common","value = false[bool]\n");
            }
        } else if(val->contains<double>()){
            slog("common","value = %lf[double]\n",val->get<double>());
        } else if(val->contains<int>()){
            slog("common","value = %d[int]\n",val->get<int>());
        } else if(val->contains<string>()){
            slog("common","value = %s[string]\n",val->get<string>().c_str());
        }
    }
}

void configure::get_option(const char* key,void* v){
    assert(v != NULL);

    if(option_map.find(key) == option_map.end()){
        get_default(key,v);
        return;
    }

    option_type* val = option_map[key];
    if(val->contains<bool>()){
        *((bool*)v) = val->get<bool>();
    } else if(val->contains<double>()){
        *((double*)v) = val->get<double>();
    } else if(val->contains<int>()){
        *((int*)v) = val->get<int>();
    } else if(val->contains<string>()){
        *((string*)v) = val->get<string>();
    }
}

void configure::dealloc(){
    for(auto it = option_map.begin(); it != option_map.end(); it++){
        delete it->second;
    }
    option_map.clear();
}

void configure::get_default(const char* key,void* val){
    // default values
}
