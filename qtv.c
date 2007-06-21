/*
	Support for FTE QuakeTV automated startup and connecting

	$Id: qtv.c,v 1.8 2007-06-21 11:31:37 johnnycz Exp $
*/

#include "quakedef.h"
#include "winquake.h"
#include "qtv.h"

// ripped from FTEQTV, original name is SV_ConsistantMVDData
// return non zero if we have at least one message
int ConsistantMVDData(unsigned char *buffer, int remaining)
{
	qbool warn = true;
	int lengthofs;
	int length;
	int available = 0;
	while(1)
	{
		if (remaining < 2)
			return available;

		//buffer[0] is time

		switch (buffer[1]&dem_mask)
		{
		case dem_set:
			length = 10;
			goto gottotallength;
		case dem_multiple:
			lengthofs = 6;
			break;
		default:
			lengthofs = 2;
			break;
		}

		if (lengthofs+4 > remaining)
			return available;

		length = (buffer[lengthofs]<<0) + (buffer[lengthofs+1]<<8) + (buffer[lengthofs+2]<<16) + (buffer[lengthofs+3]<<24);

		length += lengthofs+4;
		if (length > 1400 && warn) {
			Com_Printf("Corrupt mvd\n");
			warn = false;
		}

gottotallength:
		if (remaining < length)
			return available;
		
		remaining -= length;
		available += length;
		buffer += length;
	}
}

