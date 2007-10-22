
// qtv.h - qtv related stuff

#ifndef __QTV_H__
#define __QTV_H__

//======================================

#define QTVBUFFERTIME bound(0.1, qtv_buffertime.value, 10)

#define QTV_PLAYBACK		2			// cls.mvdplayback == QTV_PLAYBACK if QTV playback
#define QTV_VERSION			"1.3"		// we are support up to this QTV version
#define QTV_VER_1_2			1.2			// download/chat was added at this point in QTV
#define QTV_VER_1_3			1.3			// setinfo was added at this point in QTV

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
void		QTV_Cmd_Printf(float min_version, char *fmt, ...);

#endif // __QTV_H__

