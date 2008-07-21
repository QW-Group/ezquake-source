/*
Copyright (C) 2004 VVD (vvd0@sorceforge.net).

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	$Id: log.h 636 2007-07-20 05:07:57Z disconn3ct $
*/

#ifndef __LOG_H__
#define __LOG_H__

enum {	MIN_LOG = 0, CONSOLE_LOG = 0, ERROR_LOG,  RCON_LOG,
		TELNET_LOG,  FRAG_LOG,        PLAYER_LOG, MOD_FRAG_LOG, MAX_LOG};

typedef struct log_s {
	FILE		*sv_logfile;
	char		*command;
	char		*file_name;
	char		*message_off;
	char		*message_on;
	xcommand_t	function;
	int			log_level;
} log_t;

extern	log_t	logs[MAX_LOG];
extern	cvar_t	frag_log_type;
extern	cvar_t	telnet_log_level;

//bliP: logging
void	SV_Logfile (int sv_log, qbool newlog);
void	SV_LogPlayer(client_t *cl, char *msg, int level);
//<-
void	SV_Write_Log(int sv_log, int level, char *msg);

#endif /* !__LOG_H__ */
