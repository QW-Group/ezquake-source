#include <time.h>
#include <sys/stat.h>
#include "localtime.h"

//char *tzname[2][128];
//long int timezone;
//int daylight;


// helper
void UnixtimeToWintime(SYSTEMTIME *wintime, struct tm* time)
{
    wintime->wYear      = time->tm_year + 1900;
    wintime->wMonth     = time->tm_mon + 1;
    wintime->wDay       = time->tm_mday;
    wintime->wDayOfWeek = time->tm_wday;
    wintime->wHour      = time->tm_hour;
    wintime->wMinute    = time->tm_min;
    wintime->wSecond    = time->tm_sec;
}


void GetLocalTime(SYSTEMTIME *wintime)
{
    struct tm * lintime;
    time_t t;
    
    time(&t);
    lintime = localtime(&t);

    UnixtimeToWintime(wintime, lintime);    
}


int  GetFileLocalTime(char *path, SYSTEMTIME *wintime)
{
    struct stat statbuf;
    struct tm * lintime;
    
    if (stat(path, &statbuf) != 0)
        return 0;
    lintime = localtime(&statbuf.st_mtime);

    UnixtimeToWintime(wintime, lintime);

    return 1;
}

int intcmp(int i1, int i2)
{
    if (i1 > i2)
    return 1;
    if (i1 < i2)
    return -1;
    return 0;
}

int SYSTEMTIMEcmp(const SYSTEMTIME *t1, const SYSTEMTIME *t2)
{
    return intcmp( t1->wSecond, t2->wSecond )
    +  intcmp( t1->wMinute, t2->wMinute ) * 2
    +  intcmp( t1->wHour,   t2->wHour   ) * 4
    +  intcmp( t1->wDay,    t2->wDay    ) * 8
    +  intcmp( t1->wMonth,  t2->wMonth  ) * 16
    +  intcmp( t1->wYear,   t2->wYear   ) * 32;
}
















