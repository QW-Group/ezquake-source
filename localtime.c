/*
Copyright (C) 2011 ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

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
