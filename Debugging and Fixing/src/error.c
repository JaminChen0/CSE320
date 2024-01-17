/*
 * Error handling routines
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
int errors;
int warnings;
int dbflag = 1;

void fatal(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "\nFatal error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
    exit(0);
}

void error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "\nError: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
    errors++;
}

void warning(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "\nWarning: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
    warnings++;
}

void debug(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    if(!dbflag) return;
    fprintf(stderr, "\nDebug: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}
