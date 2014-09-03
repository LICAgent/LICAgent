#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>
#include <vector>
#include <dlib/type_safe_union.h>
#include <map>

using std::string;
using std::map;

double rand1();

uint64_t wall_time();

class timer{
public:
    timer(uint64_t t);
    timer();
    ~timer();

    double time_elapsed() const; //in seconds

private:
    uint64_t start_time;
};

template<typename T>
struct vector2d{
    typedef std::vector<std::vector<T> > type;
};

int readbuf(FILE* fp,char* buf,size_t n);

typedef dlib::type_safe_union<string,double,bool,int> option_type;

int char_to_int(const char* s);

class configure{
public:
    static void initialize(const char* config = NULL);
    static void get_option(const char* key,void* val);

    static void print_current_config();

    static void dealloc();
private:
    static void get_default(const char* key,void* val);
    static map<string,option_type*> option_map;
};

#endif
