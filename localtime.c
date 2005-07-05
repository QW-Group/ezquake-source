#include <time.h>
#include "localtime.h"

//char *tzname[2][128];
//long int timezone;
//int daylight;

void GetLocalTime(SYSTEMTIME *wintime)
{
    struct tm * lintime;
    time_t t;
    
    time(&t);
    lintime = localtime(&t);
    
    wintime->wYear      = lintime->tm_year;
    wintime->wMonth     = lintime->tm_mon;
    wintime->wDay       = lintime->tm_mday;
    wintime->wDayOfWeek = lintime->tm_wday;
    wintime->wHour      = lintime->tm_hour;
    wintime->wMinute    = lintime->tm_min;
    wintime->wSecond    = lintime->tm_sec;
    wintime->wMilliseconds = 0;  // FIXME..
}
