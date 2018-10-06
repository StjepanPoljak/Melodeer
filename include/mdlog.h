#ifndef MDLOG

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <stdbool.h>

#include <sys/stat.h>
#include <sys/types.h>

#define  SEC_PER_DAY     86400
#define  SEC_PER_HOUR    3600
#define  SEC_PER_MIN     60

void    MD__log                 (const char *string, ...);
void    MDLOG__dynamic          (const char *string, ...);
void    MDLOG__dynamic_reset    ();
void    MDLOG__reset_log        ();
void    MDLOG__remove_last_line ();

#endif

#define MDLOG
