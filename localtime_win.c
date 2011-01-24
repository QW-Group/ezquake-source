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

#include <windows.h>
#include "localtime.h"

// localtime built-in
// void GetLocalTime(SYSTEMTIME *wintime);


// local file time
int  GetFileLocalTime(char *path, SYSTEMTIME *wintime)
{
    // update time
    HANDLE hFile;
//    OFSTRUCT of;
    FILETIME ft1, ft2;

    hFile = CreateFile(path, 0, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == (HANDLE)ERROR_INVALID_HANDLE)
        return 0;
        
    GetFileTime(hFile, NULL, NULL, &ft1);
    FileTimeToLocalFileTime(&ft1, &ft2);
    FileTimeToSystemTime(&ft2, wintime);
    CloseHandle(hFile);
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
    return intcmp( t1->wSecond, t2->wSecond ) * 2
        +  intcmp( t1->wMinute, t2->wMinute ) * 4
        +  intcmp( t1->wHour,   t2->wHour   ) * 8
        +  intcmp( t1->wDay,    t2->wDay    ) * 16
        +  intcmp( t1->wMonth,  t2->wMonth  ) * 32
        +  intcmp( t1->wYear,   t2->wYear   ) * 64;
}
