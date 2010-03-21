#include <time.h>
#include <sys/stat.h>
#include "localtime.h"

//char *tzname[2][128];
//long int timezone;
//int daylight;


// helper
void UnixtimeToWintime(SYSTEMTIME *wintime, struct tm* time)
{
    if (wintime != NULL)
    {
        wintime->wYear      = time->tm_year + 1900;
        wintime->wMonth     = time->tm_mon + 1;
        wintime->wDay       = time->tm_mday;
        wintime->wDayOfWeek = time->tm_wday;
        wintime->wHour      = time->tm_hour;
        wintime->wMinute    = time->tm_min;
        wintime->wSecond    = time->tm_sec;
    }
}

//
// Convert DOS time (used in zip-files) to Win format
// Dos time format:
//                24                16                 8                 0
// +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
// |Y|Y|Y|Y|Y|Y|Y|M| |M|M|M|D|D|D|D|D| |h|h|h|h|h|m|m|m| |m|m|m|s|s|s|s|s|
// +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
//  \___________/\________/\_________/ \________/\____________/\_________/
//    year        month       day      hour       minute        second
// The year is stored as an offset from 1980. Seconds are stored in two-second
// increments. (So if the "second" value is 15, it actually represents 
// 30 seconds.)
// Source: http://blogs.msdn.com/oldnewthing/archive/2003/09/05/54806.aspx
//
void DostimeToWintime(SYSTEMTIME *wintime, unsigned long dostime)
{
	if (wintime != NULL)
	{
		wintime->wYear      = ((dostime & 0xFE000000) >> 25) + 1980;
		wintime->wMonth     = (dostime & 0x01E00000) >> 21;
		wintime->wDay       = (dostime & 0x1F0000) >> 16;
		wintime->wDayOfWeek = 0; // Don't waste time trying to find out day of week
		wintime->wHour      = (dostime & 0xF800) >> 11;
		wintime->wMinute    = (dostime & 0x07E0) >> 5;
		wintime->wSecond    = (dostime & 0x1F) * 2;
	}
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
















