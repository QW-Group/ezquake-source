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

// qtv.h - qtv related stuff

#ifndef __QTV_H__
#define __QTV_H__

//======================================

#define QTVBUFFERTIME bound(0.1, qtv_buffertime.value, 10)

#define QTV_PLAYBACK		2			// cls.mvdplayback == QTV_PLAYBACK if QTV playback

//======================================

#define QTV_VERSION			1.0f		// we are support up to this QTV version

// { QTV_EZQUAKE_EXT

#define QTV_EZQUAKE_EXT		"QTV_EZQUAKE_EXT"

#define QTV_EZQUAKE_EXT_DOWNLOAD	(1<<0)		// well, this is not just download, but also different connection process
#define QTV_EZQUAKE_EXT_SETINFO		(1<<1)		// does't qtv server/client support setinfo
#define QTV_EZQUAKE_EXT_QTVUSERLIST	(1<<2)		// does't qtv server/client support qtvuserlist command

#define QTV_EZQUAKE_EXT_NUM ( QTV_EZQUAKE_EXT_DOWNLOAD | QTV_EZQUAKE_EXT_SETINFO | QTV_EZQUAKE_EXT_QTVUSERLIST )

// }

// this just can't be done as macro, so I wrote function
char *QTV_CL_HEADER(float qtv_ver, int qtv_ezquake_ext);

//======================================

void		QTV_FreeUserList(void);
void		Parse_QtvUserList(char *s);

qbool		QTV_FindBestNick (const char *nick, char *result, size_t result_len);

//======================================

extern		cvar_t	qtv_buffertime;
extern		cvar_t	qtv_chatprefix;
extern		cvar_t	qtv_gamechatprefix;
extern		cvar_t	qtv_skipchained;
extern		cvar_t  qtv_adjustbuffer;
extern		cvar_t  qtv_adjustminspeed;
extern		cvar_t  qtv_adjustmaxspeed;
extern		cvar_t  qtv_adjustlowstart;
extern		cvar_t  qtv_adjusthighstart;

extern		cvar_t  qtv_event_join;
extern		cvar_t  qtv_event_leave;
extern		cvar_t  qtv_event_changename;

void		QTV_Init(void);

//======================================

#define		dem_mask	(7)

int			ConsistantMVDDataEx(unsigned char *buffer, int remaining, int *ms);
int			ConsistantMVDData(unsigned char *buffer, int remaining);

//======================================
// qtv clc list
//

#define		qtv_clc_stringcmd    1

//======================================

void		QTV_Say_f (void);
void		QTV_Cmd_ForwardToServer (void);
void		QTV_Cl_ForwardToServer_f (void);
void		QTV_Cmd_Printf(int qtv_ext, char *fmt, ...);

#endif // __QTV_H__

