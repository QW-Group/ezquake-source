/*

Copyright (C) 2001-2002       A Nourai

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef __LOGGING_H_

#define __LOGGING_H_

qboolean Log_IsLogging(void);
void Log_Init(void);
void Log_Shutdown(void);
void Log_Write(char *s);

void Log_AutoLogging_StopMatch(void);
void Log_AutoLogging_CancelMatch(void);
void Log_AutoLogging_StartMatch(char *logname);
qboolean Log_AutoLogging_Status(void);
void Log_AutoLogging_SaveMatch(void);

extern cvar_t log_readable;

#endif
