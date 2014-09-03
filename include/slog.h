#ifndef __SLOG_H__
#define __SLOG_H__

#include <stdarg.h>
#include <stdio.h>
#include "config.h"

void slog_init_params();

void fslog(FILE* fp, const char* h, const char* fmt, ...);
void fserr(FILE* fp, const char* h, const char* fmt, ...);

void slog(const char* h,const char* fmt, ...);
void serror(const char* h,const char* fmt, ...);

#endif
