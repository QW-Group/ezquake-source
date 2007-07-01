
// qtv.h - qtv related stuff

#ifndef __QTV_H__
#define __QTV_H__

//======================================

extern		cvar_t	qtv_buffertime;

void		QTV_Init(void);

//======================================

#define		dem_mask	(7)

int			ConsistantMVDData(unsigned char *buffer, int remaining);

//======================================
// qtv clc list
//

#define qtv_clc_stringcmd    1

//======================================

void		QTV_Say_f (void);

#endif // __QTV_H__

