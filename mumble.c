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
#include "mumble.h"
#ifdef __linux__
#include <sys/time.h>
#include <sys/mman.h>
#endif

#if defined (_WIN32) || defined (__linux__)

cvar_t mumble_enabled = {"mumble_enabled", "1", CVAR_NONE, OnChange_mumble_enabled};
cvar_t mumble_distance_ratio = {"mumble_distance_ratio", "0.0254"};
cvar_t mumble_debug = {"mumble_debug", "0"};

struct LinkedMem {
#ifdef _WIN32
	UINT32	uiVersion;
	DWORD	uiTick;
#else // Linux
	uint32_t uiVersion;
	uint32_t uiTick;
#endif

	float	fPosition[3];
	float	fFront[3];
	float	fTop[3];
	wchar_t	name[256];
};

struct LinkedMem *lm = NULL;
#ifdef _WIN32
static HANDLE hMapObject = NULL;
#else //Linux
static int shmfd = -1;
#endif

void Mumble_Init(void) 
{
	Cvar_SetCurrentGroup(CVAR_GROUP_SYSTEM_SETTINGS);
	Cvar_Register (&mumble_enabled);
	Cvar_Register (&mumble_distance_ratio);
	Cvar_Register (&mumble_debug);
}

void Mumble_CreateLink()
{
#ifdef _WIN32
	if (hMapObject != NULL)
	{
		ST_Printf(PRINT_FAIL,"Mumble Link already initialized.\n");
		return;
	}
	hMapObject = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"MumbleLink");
	if (hMapObject == NULL)
	{
		ST_Printf(PRINT_INFO,"Mumble initialization skipped. Mumble not running.\n");
		return;
	}

	lm = (struct LinkedMem *) MapViewOfFile(hMapObject, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(struct LinkedMem));
	if (lm == NULL) {
		CloseHandle(hMapObject);
		hMapObject = NULL;
		ST_Printf(PRINT_FAIL,"Mumble Link initialization failed.\n");
		return;
	}
#else //Linux
	char memname[256];
	snprintf(memname, 256, "/MumbleLink.%d", getuid());

	shmfd = shm_open(memname, O_RDWR, S_IRUSR | S_IWUSR);

	if(shmfd < 0)
	{
		ST_Printf(PRINT_INFO,"Mumble initialization skipped. Mumble not running.\n");
		return;
	}

	lm = (struct LinkedMem *) (mmap(NULL, sizeof(struct LinkedMem), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd,0));

	if (lm == (void *) (-1))
	{
		lm = NULL;
		ST_Printf(PRINT_FAIL,"Mumble Link initialization failed.\n");
		return;
	}
#endif

	mbstowcs(lm->name, "ezQuake", sizeof(lm->name));
	ST_Printf(PRINT_INFO,"Mumble Link initialization successful.\n");
}

void Mumble_DestroyLink()
{
#ifdef _WIN32
	if (hMapObject != NULL)
	{
		CloseHandle(hMapObject);
		hMapObject = NULL;
#else //Linux
	if (lm == (void *) (-1))
	{
		lm = NULL;
#endif
		ST_Printf(PRINT_INFO,"Mumble Link shut down.\n");
		return;
	}
	else
	{
	if (mumble_debug.integer)
		ST_Printf(PRINT_FAIL,"Mumble Link not established, unable to shut down.\n");
	}
}

void OnChange_mumble_enabled(cvar_t *var, char *value, qbool *cancel)
{
	int mumble = Q_atoi (value);

	if (mumble)
		Mumble_CreateLink();
	else
		Mumble_DestroyLink();
}

#ifndef WIN32
static const int32_t GetTickCount() 
{
	struct timeval tv;
	gettimeofday(&tv,NULL);

	return tv.tv_usec / 1000 + tv.tv_sec * 1000;
}
#endif

void updateMumble() 
{
	vec3_t  right;
	static qbool mpa_report_active = true;

	if (!mumble_enabled.integer || !lm )
		return;

	// Only activate Mumble Positional Audio if you are an active player
	if (cls.demoplayback || cl.spectator)
	{
		if (mpa_report_active)
		{
			// Setting position to 0 disables Mumble Positional Audio
			VectorClear(lm->fPosition);
			mpa_report_active = false;
			ST_Printf(PRINT_INFO,"Mumble Position Audio reporting disabled\n");
		}
		return ;
	}

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

	// If a link is lost struct is reset, reinitialize lm->name
	if(lm->name[0] != 'e')
		mbstowcs(lm->name, "ezQuake", sizeof(lm->name));

	// Required by Mumble
	lm->uiVersion = 1;
	lm->uiTick = GetTickCount();
	
	if(!mpa_report_active)
	{
		ST_Printf(PRINT_INFO,"Mumble Position Audio reporting enabled.\n");
		mpa_report_active = true;
	}
}

#endif // WIN32 && LINUX

