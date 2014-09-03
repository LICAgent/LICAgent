#include "slog.h"
#include "common.h"
#include <stdio.h>
#include <assert.h>
#include <string>

FILE* output = NULL;

std::string redir_fn = "";
bool slog_output = true;
bool slog_redirect = false;

void slog_init_params(){
    configure::get_option("SLOG_REDIRECT",&slog_redirect);
    configure::get_option("SLOG_OUTPUT",&slog_output);
    configure::get_option("SLOG_REDIRECT_FILE",&redir_fn);

    slog("slog","options:\n");
    slog("slog","SLOG_REDIRECT: %d[bool]\n",slog_redirect);
    slog("slog","SLOG_OUTPUT: %d[bool]\n",slog_output);
    slog("slog","SLOG_REDIRECT_FILE: %s\n",redir_fn.c_str());
}

static int format_output(FILE* fp, char left, char right, const char* h, const char* fmt, va_list ap){
    int ret = 0;
    if(slog_output){
        fprintf(fp,"%c%s%c:",left,h,right);
        ret = vfprintf(fp,fmt,ap);
        fflush(fp);
    }

    return ret;
}

void fslog(FILE* fp, const char* h, const char* fmt, ...){
    va_list args;
    va_start(args,fmt);
    format_output(fp,'(',')',h,fmt,args);
    va_end(args);
}

void fserr(FILE* fp, const char* h, const char* fmt, ...){
    va_list args;
    va_start(args,fmt);
    format_output(fp,'[',']',h,fmt,args);
    va_end(args);
}

static FILE* get_fptr(FILE* fp){
    if(slog_redirect){
        if(output == NULL){
            output = fopen(redir_fn.c_str(),"a");

            assert(output);
        }

        return output;
    } else {
        return fp;
    }
}

void slog(const char* h,const char* fmt, ...){
    va_list args;
    va_start(args,fmt);

    FILE* fp = get_fptr(stderr);
    format_output(fp,'(',')',h,fmt,args);

    va_end(args);

    fflush(stdout);
}

void serror(const char* h,const char* fmt, ...){
    va_list args;
    va_start(args,fmt);

    FILE* fp = get_fptr(stderr);
    format_output(fp,'[',']',h,fmt,args);

    va_end(args);
}
