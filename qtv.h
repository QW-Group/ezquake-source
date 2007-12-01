
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

#define QTV_EZQUAKE_EXT_DOWNLOAD (1<<0)		// well, this is not just download, but also different connection process
#define QTV_EZQUAKE_EXT_SETINFO  (1<<1)		// does't qtv server/client support setinfo

#define QTV_EZQUAKE_EXT_NUM ( QTV_EZQUAKE_EXT_DOWNLOAD | QTV_EZQUAKE_EXT_SETINFO )

// }

// this just can't be done as macro, so I wrote function
char *QTV_CL_HEADER(float qtv_ver, int qtv_ezquake_ext);

// qqshka: Its all messy.
// For example ezquake (and FTE?) expect maximum message is MSG_BUF_SIZE == 8192 with mvd header which have not fixed size,
// however fuhquake uses less msg size as I recall.
// mvd header max size is 10 bytes.
// 
// MAX_MVD_SIZE - max size of single mvd message _WITHOUT_ header
#define	MAX_MVD_SIZE			(MSG_BUF_SIZE - 100)

//======================================

extern		cvar_t	qtv_buffertime;
extern		cvar_t	qtv_chatprefix;
extern		cvar_t  qtv_adjustbuffer;
extern		cvar_t  qtv_adjustminspeed;
extern		cvar_t  qtv_adjustmaxspeed;
extern		cvar_t  qtv_adjustlowstart;
extern		cvar_t  qtv_adjusthighstart;

void		QTV_Init(void);

//======================================

#define		dem_mask	(7)

int			ConsistantMVDDataEx(unsigned char *buffer, int remaining, int *ms);
int			ConsistantMVDData(unsigned char *buffer, int remaining);

//======================================
// qtv clc list
//

#define qtv_clc_stringcmd    1

//======================================

void		QTV_Say_f (void);
void		QTV_Cmd_ForwardToServer (void);
void		QTV_Cl_ForwardToServer_f (void);
void		QTV_Cmd_Printf(int qtv_ext, char *fmt, ...);

#endif // __QTV_H__

