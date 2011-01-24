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

#ifndef __LOCALTIME_H__
#define __LOCALTIME_H__

#ifdef _WIN32

#include <windows.h> // FIXME: we should include it only at winquake.h

#else

#include <time.h>

typedef struct SYSTEMTIME_s
{
    int wYear;
    int wMonth;
    int wDay;
    int wDayOfWeek;
    int wHour;
    int wMinute;
    int wSecond;
    int wMilliseconds;
} SYSTEMTIME;

void GetLocalTime(SYSTEMTIME *);

#endif


int  GetFileLocalTime(char *, SYSTEMTIME *);
void UnixtimeToWintime(SYSTEMTIME *, struct tm *);
int  SYSTEMTIMEcmp(const SYSTEMTIME *, const SYSTEMTIME *);

#endif // __LOCALTIME_H__








