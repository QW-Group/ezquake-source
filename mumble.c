/*
Copyright (C) 2009 ezQuake Development Team

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

*/

#include "quakedef.h"

#ifdef _WIN32

cvar_t mumble_enabled = {"mumble_enabled", "1"};
cvar_t mumble_distance_ratio = {"mumble_distance_ratio", "0.0254"};
cvar_t mumble_debug = {"mumble_debug", "0"};

struct LinkedMem {
	UINT32	uiVersion;
	DWORD	uiTick;

	float	fPosition[3];
	float	fFront[3];
	float	fTop[3];
	wchar_t	name[256];
};

struct LinkedMem *lm = NULL;
static HANDLE hMapObject = NULL;

void Mumble_Init(void) 
{
	Cvar_SetCurrentGroup(CVAR_GROUP_NO_GROUP);
	Cvar_Register (&mumble_enabled);
	Cvar_Register (&mumble_distance_ratio);
	Cvar_Register (&mumble_debug);

	hMapObject = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"MumbleLink");
	if (hMapObject == NULL)
	{
		ST_Printf(PRINT_FAIL,"Mumble initialization failed.\n");
		return;
	}
	//struct Linkedmem;
	lm = (struct LinkedMem *) MapViewOfFile(hMapObject, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(struct LinkedMem));
	if (lm == NULL) {
		CloseHandle(hMapObject);
		hMapObject = NULL;
		ST_Printf(PRINT_FAIL,"Mumble object initialization failed.\n");
		return;
	}
	wcscpy_s(lm->name, 256, L"ezQuake");

	ST_Printf(PRINT_INFO,"Mumble initialization successful.\n");
	return;
}

void updateMumble() {

	vec3_t  right;

	if (!mumble_enabled.integer)
		return;

	if (cls.demoplayback || cl.spectator)
		return ;

	if (!lm)
		return;

	// Set player origin for Mumble
	VectorCopy(cl.simorg , lm->fPosition);

	// Set distance ratio for Mumble distance calculation, qw units to meters
	lm->fPosition[0] *= mumble_distance_ratio.value;
	lm->fPosition[1] *= mumble_distance_ratio.value;
	// Above + convert from right-handed coordinates to left-handed
	lm->fPosition[2] *= -1 * mumble_distance_ratio.value;

	// Convert viewangle to normalized vectors
	AngleVectors(cl.viewangles,lm->fFront, right, lm->fTop);

	// Convert from right-handed coordinates to left-handed
	lm->fFront[2] *= -1;
	lm->fTop[2] *= -1;

	if (mumble_debug.integer)
		Com_Printf("X=%f Y=%f Z=%f\n",lm->fPosition[0],lm->fPosition[1],lm->fPosition[2]);

	// Required by Mumble
	lm->uiVersion = 1;
	lm->uiTick = GetTickCount();
}

#endif