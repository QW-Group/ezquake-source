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

#define QTVPREBUFFERTIME (qtv_prebuffertime.value > 0 ? qtv_prebuffertime.value : bound(0.1, qtv_buffertime.value, 10))
#define QTVBUFFERTIME bound(0.1, qtv_buffertime.value, 30)

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

// qqshka: It's all messy.
// For example ezquake (and FTE?) expect maximum message is MSG_BUF_SIZE == 8192 with mvd header which have not fixed size,
// however fuhquake uses less msg size as I recall.
// mvd header max size is 10 bytes.
// 
// MAX_MVD_SIZE - max size of single mvd message _WITHOUT_ header
#define	MAX_MVD_SIZE			(MSG_BUF_SIZE - 100)
//======================================

typedef enum {
	QUL_NONE = 0,	//
	QUL_ADD,		// user joined
	QUL_CHANGE,		// user changed something like name or something
	QUL_DEL,		// user dropped
	QUL_INIT		// user init
} qtvuserlist_t;

typedef struct qtvuser_s {
	int					id;								// unique user id
	char				name[MAX_INFO_KEY];				// client name, well must be unique too

	struct qtvuser_s	*next;							// next qtvuser_s struct in our list
} qtvuser_t;

void		QTV_FreeUserList(void);
void		Parse_QtvUserList(char *s);

qbool		QTV_FindBestNick (const char *nick, char *result, size_t result_len);

//======================================

extern cvar_t qtv_buffertime;
extern cvar_t qtv_prebuffertime;
extern cvar_t qtv_chatprefix;
extern cvar_t qtv_gamechatprefix;
extern cvar_t qtv_skipchained;
extern cvar_t qtv_adjustbuffer;
extern cvar_t qtv_adjustminspeed;
extern cvar_t qtv_adjustmaxspeed;
extern cvar_t qtv_adjustlowstart;
extern cvar_t qtv_adjusthighstart;
extern cvar_t qtv_allow_pause;

extern cvar_t qtv_event_join;
extern cvar_t qtv_event_leave;
extern cvar_t qtv_event_changename;

void QTV_Init(void);

//======================================

#define		dem_mask	(7)

int			ConsistantMVDDataEx(unsigned char *buffer, int remaining, int *ms, int max_packets);
int			ConsistantMVDData(unsigned char *buffer, int remaining, int max_packets);

//======================================
// qtv clc list
//

#define qtv_clc_stringcmd    1

//======================================

void		QTV_ForwardToServerEx (qbool skip_if_no_params, qbool use_first_argument);
void		QTV_Say_f (void);
void		QTV_Cmd_ForwardToServer (void);
void		QTV_Cl_ForwardToServer_f (void);
void		QTV_Cmd_Printf(int qtv_ext, char *fmt, ...);

#endif // __QTV_H__

