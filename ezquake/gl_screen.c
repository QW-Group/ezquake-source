/*
Copyright (C) 1996-1997 Id Software, Inc.

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

// screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"
#include "gl_local.h"
#include "image.h"
#include "keys.h"
#include "menu.h"
#include "sbar.h"
#include "sound.h"
#include <time.h>

#include "gl_image.h"
#include "utils.h"

#include "teamplay.h"

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Com_Printf ();

net 
turn off messages option

the refresh is always rendered, unless the console is full screen


console is:
	notify lines
	half
	full
	

*/

int                     glx, gly, glwidth, glheight;

// only the refresh window will be updated unless these variables are flagged 
int                     scr_copytop;
int                     scr_copyeverything;

float           scr_con_current;
float           scr_conlines;           // lines of console to display

float           oldscreensize, oldfov, oldsbar;
cvar_t          scr_viewsize = {"viewsize","100",CVAR_ARCHIVE};
cvar_t          scr_fov = {"fov","90",CVAR_ARCHIVE}; // 10 - 170
cvar_t          scr_consize = {"scr_consize","0.75"};
cvar_t          scr_conspeed = {"scr_conspeed","1000"};
cvar_t          scr_centertime = {"scr_centertime","2"};
cvar_t          scr_showram = {"showram","1"};
cvar_t          scr_showturtle = {"showturtle","0"};
cvar_t          scr_showpause = {"showpause","1"};
cvar_t          scr_printspeed = {"scr_printspeed","8"};
cvar_t			scr_allowsnap = {"scr_allowsnap", "1"};

cvar_t			scr_clock = {"cl_clock","0"};
cvar_t			scr_clock_x = {"cl_clock_x","0"};
cvar_t			scr_clock_y = {"cl_clock_y","-1"};


cvar_t			scr_democlock = {"cl_democlock","0"};
cvar_t			scr_democlock_x = {"cl_democlock_x","0"};
cvar_t			scr_democlock_y = {"cl_democlock_y","-2"};


cvar_t			show_speed = {"show_speed","0"};
cvar_t			scr_sshot_type			= {"scr_sshot_type","tga"};			
cvar_t			scr_sshot_apply_palette	= {"scr_sshot_apply_palette","1"};	
cvar_t          show_fps = {"show_fps", "0"};

cvar_t          gl_triplebuffer = {"gl_triplebuffer", "1",CVAR_ARCHIVE};

cvar_t			scr_autoid	= {"scr_autoid", "0"};

qboolean        scr_initialized;                // ready to draw

mpic_t          *scr_ram;
mpic_t          *scr_net; 
mpic_t          *scr_turtle;

int				scr_fullupdate;

int				clearconsole;
int				clearnotify;

viddef_t        vid;                            // global video state

vrect_t         scr_vrect;

qboolean        scr_skipupdate;

qboolean        scr_disabled_for_loading;
qboolean        scr_drawloading;
float           scr_disabled_time;

qboolean        block_drawing;

void SCR_ScreenShot_f (void);
extern char *COM_FileExtension (char *in);	
void SCR_DrawStatusOp(void); 

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char            scr_centerstring[1024];
float           scr_centertime_start;   // for slow victory printing
float           scr_centertime_off;
int                     scr_center_lines;
int                     scr_erase_lines;
int                     scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	Q_strncpyz (scr_centerstring, str, sizeof(scr_centerstring));
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

// count the number of lines for centering
	scr_center_lines = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}

void SCR_DrawCenterString (void)
{
	char    *start;
	int             l;
	int             j;
	int             x, y;
	int             remaining;

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.height*0.35;
	else
		y = 48;

	do      
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
		{
			Draw_Character (x, y, start[j]);        
			if (!remaining--)
				return;
		}
			
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;                // skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= cls.frametime;
	
	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
	float   a;
	float   x;
	
	if (fov_x < 1 || fov_x > 179)
		Sys_Error ("Bad fov: %f", fov_x);
	
	x = width / tan(fov_x/360*M_PI);
	
	a = atan (height/x);
	
	a = a*360/M_PI;
	
	return a;
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void SCR_CalcRefdef (void)
{
	float           size;
	int             h;
	qboolean		full = false;


	scr_fullupdate = 0;             // force a background redraw
	vid.recalc_refdef = 0;

// force the status bar to redraw
	Sbar_Changed ();

//========================================
	
// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set (&scr_viewsize, "30");
	if (scr_viewsize.value > 120)
		Cvar_Set (&scr_viewsize, "120");

// bound field of view
	if (scr_fov.value < 10)
		Cvar_Set (&scr_fov, "10");
	if (scr_fov.value > 170)
		Cvar_Set (&scr_fov, "170");

// intermission is always full screen   
	if (cl.intermission)
		size = 120;
	else
		size = scr_viewsize.value;

	if (size >= 120)
		sb_lines = 0;           // no status bar at all
	else if (size >= 110)
		sb_lines = 24;          // no inventory
	else
		sb_lines = 24+16+8;

	if (scr_viewsize.value >= 100.0) {
		full = true;
		size = 100.0;
	} else
		size = scr_viewsize.value;
	if (cl.intermission)
	{
		full = true;
		size = 100.0;
		sb_lines = 0;
	}
	size /= 100.0;

	if (!cl_sbar.value && full)
		h = vid.height;
	else
		h = vid.height - sb_lines;

	r_refdef.vrect.width = vid.width * size;
	if (r_refdef.vrect.width < 96)
	{
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96;      // min for icons
	}

	r_refdef.vrect.height = vid.height * size;
	if (cl_sbar.value || !full) {
  		if (r_refdef.vrect.height > vid.height - sb_lines)
  			r_refdef.vrect.height = vid.height - sb_lines;
	} else if (r_refdef.vrect.height > vid.height)
			r_refdef.vrect.height = vid.height;
	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width)/2;
	if (full)
		r_refdef.vrect.y = 0;
	else 
		r_refdef.vrect.y = (h - r_refdef.vrect.height)/2;

	r_refdef.fov_x = scr_fov.value;
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

	scr_vrect = r_refdef.vrect;
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	Cvar_SetValue (&scr_viewsize,scr_viewsize.value+10);
	vid.recalc_refdef = 1;
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	Cvar_SetValue (&scr_viewsize,scr_viewsize.value-10);
	vid.recalc_refdef = 1;
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_VIEW);
	Cvar_Register (&scr_fov);
	Cvar_Register (&scr_viewsize);

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&scr_consize);
	Cvar_Register (&scr_conspeed);
	Cvar_Register (&scr_printspeed);

	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	Cvar_Register (&gl_triplebuffer);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&scr_showram);
	Cvar_Register (&scr_showturtle);
	Cvar_Register (&scr_showpause);
	Cvar_Register (&scr_centertime);

	Cvar_Register (&scr_clock_x);
	Cvar_Register (&scr_clock_y);
	Cvar_Register (&scr_clock);


	Cvar_Register (&scr_democlock_x);
	Cvar_Register (&scr_democlock_y);
	Cvar_Register (&scr_democlock);

	Cvar_Register (&scr_autoid);


	Cvar_Register (&show_speed);
	Cvar_Register (&show_fps);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREENSHOTS);
	Cvar_Register (&scr_sshot_type);			
	Cvar_Register (&scr_sshot_apply_palette);	
	Cvar_Register (&scr_allowsnap);

	Cvar_ResetCurrentGroup();

// register our commands
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);

	scr_ram = Draw_CacheWadPic ("ram");
	scr_net = Draw_CacheWadPic ("net");
	scr_turtle = Draw_CacheWadPic ("turtle");

	scr_initialized = true;
}



/*
==============
SCR_DrawRam
==============
*/
void SCR_DrawRam (void)
{
	if (!scr_showram.value)
		return;

	if (!r_cache_thrash)
		return;

	Draw_Pic (scr_vrect.x+32, scr_vrect.y, scr_ram);
}

/*
==============
SCR_DrawTurtle
==============
*/
void SCR_DrawTurtle (void)
{
	static int      count;
	
	if (!scr_showturtle.value)
		return;

	if (cls.frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_Pic (scr_vrect.x, scr_vrect.y, scr_turtle);
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < UPDATE_BACKUP-1)
		return;
	if (cls.demoplayback)
		return;

	Draw_Pic (scr_vrect.x+64, scr_vrect.y, scr_net);
}

void SCR_DrawFPS (void) {
	static double lastframetime;
	double t;
	extern int fps_count;
	static float lastfps;
	int x, y;
	char st[80];

	if (!show_fps.value)
		return;

	t = Sys_DoubleTime();
	if ((t - lastframetime) >= 1.0) {
		lastfps = fps_count / (t - lastframetime);
		fps_count = 0;
		lastframetime = t;
	}

	if (cl_multiview.value && cls.mvdplayback) {
		sprintf(st, "%3.1f FPS", (lastfps + 0.05)/nNumViews);
	}
	else {
		sprintf(st, "%3.1f FPS", lastfps + 0.05);
	}  

	x = vid.width - strlen(st) * 8 - 8;
	y = vid.height - sb_lines - 8;
//	Draw_TileClear(x, y, strlen(st) * 8, 8);
	Draw_String(x, y, st);
}


void SCR_DrawSpeed (void)
{
	int x, y;
	char st[80];
	vec3_t vel;
	float speed;
	static float maxspeed = 0;
	static float display_speed = -1;
	static double lastrealtime = 0;

	if (!show_speed.value)
		return;

	if (lastrealtime > cls.realtime)
	{
		lastrealtime = 0;
		display_speed = -1;
		maxspeed = 0;
	}

	if (show_speed.value == 2) {
		VectorCopy (cl.simvel, vel);	// predicted velocity
	} else
		VectorCopy (cl.frames[cl.validsequence & UPDATE_MASK].playerstate[cl.playernum].velocity, vel);
	vel[2] = 0;
	speed = VectorLength(vel);

	if (speed > maxspeed)
		maxspeed = speed;

	if (display_speed >= 0)
	{
		sprintf(st, "%3d", (int)display_speed);
		x = vid.width - strlen(st) * 8 - 8;
		y = 8;
		Draw_String(x, y, st);
	}

	if (cls.realtime - lastrealtime >= 0.1)
	{
		lastrealtime = cls.realtime;
		display_speed = maxspeed;
		maxspeed = 0;
	}
}

void SCR_DrawClock (void) {
	char	str[80] = {0};

	if (!scr_clock.value)
		return;

	if (scr_clock.value == 2) {
		time_t		t;
		struct tm	*ptm;

		time (&t);
		if (!(ptm = localtime (&t)))
			strcpy (str, "#bad date#");
		else
			strftime (str, sizeof(str)-1, "%H:%M:%S", ptm);
	} else {
		Q_strncpyz (str, SecondsToHourString(cls.realtime), sizeof(str));
	}

	if (scr_clock_y.value < 0)
		Draw_String (8 * scr_clock_x.value, vid.height - sb_lines + 8 * scr_clock_y.value, str);
	else
		Draw_String (8 * scr_clock_x.value, 8 * scr_clock_y.value, str);
}


void SCR_DrawDemoClock (void) {
	char	str[80];

	if (!cls.demoplayback || !scr_democlock.value)
		return;
	// START shaman RFE 1024658
	// Q_strncpyz (str, SecondsToHourString(cls.realtime - cls.demostarttime), sizeof(str));
	Q_strncpyz (str, SecondsToMinutesString(cls.realtime - cls.demostarttime), sizeof(str));
	// END shaman RFE 1024658

	if (scr_democlock_y.value < 0)
		Draw_String (8 * scr_democlock_x.value, vid.height - sb_lines + 8 * scr_democlock_y.value, str);
	else
		Draw_String (8 * scr_democlock_x.value, 8 * scr_democlock_y.value, str);
}


/*
==============
DrawPause
==============
*/
void SCR_DrawPause (void)
{
	mpic_t  *pic;

	if (!scr_showpause.value)               // turn off for screenshots
		return;

	if (!cl.paused)
		return;

	pic = Draw_CachePic ("gfx/pause.lmp");
	Draw_Pic ( (vid.width - pic->width)/2, 
		(vid.height - 48 - pic->height)/2, pic);
}



/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	mpic_t  *pic;

	if (!scr_drawloading)
		return;
		
	pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_Pic ( (vid.width - pic->width)/2, 
		(vid.height - 48 - pic->height)/2, pic);
}



//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void) {

	Con_CheckResize ();

	if (scr_drawloading)
		return;         // never a console with loading plaque
	
// decide on the height of the console
	if (cls.state != ca_active && !cl.intermission)	{
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	} else if (key_dest == key_console) {
		scr_conlines = vid.height * scr_consize.value;
		if (scr_conlines < 30)
			scr_conlines = 30;
		if (scr_conlines > vid.height - 10)
			scr_conlines = vid.height - 10;
	}
	else {
		scr_conlines = 0;				// none visible
	}

	if (scr_conlines < scr_con_current)	{
		scr_con_current -= scr_conspeed.value * cls.trueframetime * vid.height / 320;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;
	} else if (scr_conlines > scr_con_current) {
		scr_con_current += scr_conspeed.value * cls.trueframetime * vid.height / 320;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (clearconsole++ < vid.numpages)
		Sbar_Changed ();
	else
		con_notifylines = 0;
}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	if (scr_con_current)
	{
		scr_copyeverything = 1;
		Con_DrawConsole (scr_con_current);
		clearconsole = 0;
	}
	else
	{
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();      // only draw notify in game
	}
}

/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 

typedef struct _TargaHeader {
	unsigned char   id_length, colormap_type, image_type;
	unsigned short  colormap_index, colormap_length;
	unsigned char   colormap_size;
	unsigned short  x_origin, y_origin, width, height;
	unsigned char   pixel_size, attributes;
} TargaHeader;


void screenshot(char *name) {
	qboolean png, tga, jpg, ok;
	byte *buffer;
	char *ext;

	ok = png = tga = jpg = false;
	name = (*name == '/') ? name + 1 : name;
	ext = COM_FileExtension(name);

	if (!Q_strcasecmp(ext, "tga"))
		tga = true;

#ifdef WITH_PNG
	else if (!Q_strcasecmp(ext, "png"))
		png = true;
#endif

#ifdef WITH_JPEG
	else if (!Q_strcasecmp(ext, "jpg"))
		jpg = true;
#endif

#ifdef WITH_PNG
	else if (!Q_strcasecmp(scr_sshot_type.string, "png"))
		png = true;
#endif

#ifdef WITH_JPEG
	else if (!Q_strcasecmp(scr_sshot_type.string, "jpg") || !Q_strcasecmp(scr_sshot_type.string, "jpeg"))
		jpg = true;
#endif

	else
		tga = true;

	buffer = Q_Malloc (glwidth * glheight * 3);
	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer); 
	Image_ApplyGamma(buffer, glwidth * glheight * 3);

	if (tga) {
		COM_ForceExtension (name, ".tga");
		ok = Image_WriteTGA(name, buffer, glwidth, glheight);
	}

#ifdef WITH_PNG
	else if (png) {
		COM_ForceExtension (name, ".png");
		ok = Image_WritePNG(name, image_png_compression_level.value, buffer, glwidth, glheight);
	}
#endif

#ifdef WITH_JPEG
	else if (jpg) {
		COM_ForceExtension (name, ".jpg");
		ok = Image_WriteJPEG(name, image_jpeg_quality_level.value, buffer, glwidth, glheight);
	}
#endif

	free(buffer);
	Com_Printf ("%s %s\n", ok ? "Wrote" : "Couldn't write", name);
}

/* 
================== 
SCR_ScreenShot_f
================== 
*/
void SCR_ScreenShot_f (void) {
	char	name[MAX_OSPATH], ext[4];
	int		i;
	FILE	*f;

	if (Cmd_Argc() == 2)
		Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
	else if (Cmd_Argc() == 1){
		// find a file name to save it to 
		if (!Q_strcasecmp(scr_sshot_type.string, "png"))
			Q_strncpyz(ext, "png", 4);
		else if (!Q_strcasecmp(scr_sshot_type.string, "jpeg") || !Q_strcasecmp(scr_sshot_type.string, "jpg"))
			Q_strncpyz(ext, "jpg", 4);
		else
			Q_strncpyz(ext, "tga", 4);

		for (i = 0; i < 999; i++) {
			Q_snprintfz(name, sizeof(name), "ezquake%03i.%s", i, ext);
			if (!(f = fopen (va("%s/%s", cls.gamedir, name), "rb")))
				break;  // file doesn't exist
			fclose(f);
		}
		if (i == 1000) {
			Com_Printf ("Error: Cannot create more than 1000 screenshots\n");
			return;
		}
	} else {
		Com_Printf("Usage: %s [filename]", Cmd_Argv(0));
		return;
	}
	screenshot(va("%s/%s", cls.gamedirfile, name));
}

/* 
============== 
WritePCXfile 
============== 
*/ 
void WritePCXfile (char *filename, byte *data, int width, int height, int rowbytes, byte *palette, qboolean upload) {
	int		i, j, length;
	pcx_t	*pcx;
	byte		*pack;
	  
	pcx = Hunk_TempAlloc (width * height * 2 + 1000);
	if (pcx == NULL) {
		Com_Printf ("WritePCXfile: not enough memory\n");
		return;
	}
 
	pcx->manufacturer = 0x0a;	// PCX id
	pcx->version = 5;			// 256 color
 	pcx->encoding = 1;			// uncompressed
	pcx->bits_per_pixel = 8;	// 256 color
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = LittleShort((short)(width-1));
	pcx->ymax = LittleShort((short)(height-1));
	pcx->hres = LittleShort((short)width);
	pcx->vres = LittleShort((short)height);
	memset (pcx->palette,0,sizeof(pcx->palette));
	pcx->color_planes = 1;		// chunky image
	pcx->bytes_per_line = LittleShort((short)width);
	pcx->palette_type = LittleShort(2);		// not a grey scale
	memset (pcx->filler,0,sizeof(pcx->filler));

// pack the image
	pack = &pcx->data;

	data += rowbytes * (height - 1);

	for (i = 0 ; i<height ; i++) {
		for (j=0 ; j<width ; j++) {
			if ( (*data & 0xc0) != 0xc0)
				*pack++ = *data++;
			else {
				*pack++ = 0xc1;
				*pack++ = *data++;
			}
		}

		data += rowbytes - width;
		data -= rowbytes * 2;
	}
			
// write the palette
	*pack++ = 0x0c;	// palette ID byte
	for (i=0 ; i < 768 ; i++)
		*pack++ = *palette++;
		
// write output file 
	length = pack - (byte *) pcx;

	if (upload) {
		CL_StartUpload((void *) pcx, length);
	} else {
		if (!COM_WriteFile (filename, pcx, length))
			Com_Printf("Couldn't save screenshot %s\n", filename);
	}
}

/*
Find closest color in the palette for named color
*/
int MipColor(int r, int g, int b)
{
	int i;
	float dist;
	int best;
	float bestdist;
	int r1, g1, b1;
	static int lr = -1, lg = -1, lb = -1;
	static int lastbest;

	if (r == lr && g == lg && b == lb)
		return lastbest;

	bestdist = 256*256*3;

	for (i = 0; i < 256; i++) {
		r1 = host_basepal[i*3] - r;
		g1 = host_basepal[i*3+1] - g;
		b1 = host_basepal[i*3+2] - b;
		dist = r1*r1 + g1*g1 + b1*b1;
		if (dist < bestdist) {
			bestdist = dist;
			best = i;
		}
	}
	lr = r; lg = g; lb = b;
	lastbest = best;
	return best;
}

// from gl_draw.c
extern byte		*draw_chars;				// 8*8 graphic characters

void SCR_DrawCharToSnap (int num, byte *dest, int width)
{
	int		row, col;
	byte	*source;
	int		drawline;
	int		x;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if (source[x])
				dest[x] = source[x];
			else
				dest[x] = 98;
		source += 128;
		dest -= width;
	}

}

void SCR_DrawStringToSnap (const char *s, byte *buf, int x, int y, int width)
{
	byte *dest;
	const unsigned char *p;

	dest = buf + ((y * width) + x);

	p = (const unsigned char *)s;
	while (*p) {
		SCR_DrawCharToSnap(*p++, dest, width);
		dest += 8;
	}
}


/* 
================== 
SCR_RSShot_f
================== 
*/  
void SCR_RSShot_f (void) { 
	int     x, y;
	unsigned char		*src, *dest;
	unsigned char		*newbuf;
	int w, h;
	int dx, dy, dex, dey, nx;
	int r, b, g;
	int count;
	float fracw, frach;
	char st[80];
	time_t now;
	extern cvar_t name;

	if (CL_IsUploading())
		return; // already one pending

	if (cls.state < ca_onserver)
		return; // gotta be connected

	if (!scr_allowsnap.value) { 
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd); 
		SZ_Print (&cls.netchan.message, "snap\n"); 
		Com_Printf ("Refusing remote screen shot request.\n"); 
		return; 
	} 

	Com_Printf ("Remote screen shot requested.\n");

// save the pcx file 
	newbuf = Q_Malloc (glheight * glwidth * 3);

	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, newbuf);

	w = (vid.width < RSSHOT_WIDTH) ? glwidth : RSSHOT_WIDTH;
	h = (vid.height < RSSHOT_HEIGHT) ? glheight : RSSHOT_HEIGHT;

	fracw = (float)glwidth / (float)w;
	frach = (float)glheight / (float)h;

	for (y = 0; y < h; y++) {
		dest = newbuf + (w*3 * y);

		for (x = 0; x < w; x++) {
			r = g = b = 0;

			dx = x * fracw;
			dex = (x + 1) * fracw;
			if (dex == dx) dex++; // at least one
			dy = y * frach;
			dey = (y + 1) * frach;
			if (dey == dy) dey++; // at least one

			count = 0;
			for (/* */; dy < dey; dy++) {
				src = newbuf + (glwidth * 3 * dy) + dx * 3;
				for (nx = dx; nx < dex; nx++) {
					r += *src++;
					g += *src++;
					b += *src++;
					count++;
				}
			}
			r /= count;
			g /= count;
			b /= count;
			*dest++ = r;
			*dest++ = g;
			*dest++ = b;
		}
	}

	// convert to eight bit
	for (y = 0; y < h; y++) {
		src = newbuf + (w * 3 * y);
		dest = newbuf + (w * y);

		for (x = 0; x < w; x++) {
			*dest++ = MipColor(src[0], src[1], src[2]);
			src += 3;
		}
	}

	time(&now);
	strcpy(st, ctime(&now));
	st[strlen(st) - 1] = 0;
	SCR_DrawStringToSnap (st, newbuf, w - strlen(st)*8, h - 1, w);

	Q_strncpyz (st, cls.servername, sizeof(st));
	SCR_DrawStringToSnap (st, newbuf, w - strlen(st)*8, h - 11, w);

	Q_strncpyz (st, name.string, sizeof(st));
	SCR_DrawStringToSnap (st, newbuf, w - strlen(st)*8, h - 21, w);

	WritePCXfile (NULL, newbuf, w, h, w, host_basepal, true);

	free(newbuf);

	Com_Printf ("Sending shot to server...\n");
} 

int qglProject (float objx, float objy, float objz, float *model, float *proj, int *view, float* winx, float* winy, float* winz) {
	float in[4], out[4];
	int i;

	in[0] = objx; in[1] = objy; in[2] = objz; in[3] = 1.0;


	for (i = 0; i < 4; i++)
		out[i] = in[0] * model[0 * 4 + i] + in[1] * model[1 * 4 + i] + in[2] * model[2 * 4 + i] + in[3] * model[3 * 4 + i];


	for (i = 0; i < 4; i++)
		in[i] =	out[0] * proj[0 * 4 + i] + out[1] * proj[1 * 4 + i] + out[2] * proj[2 * 4 + i] + out[3] * proj[3 * 4 + i];

	if (!in[3])
		return 0;

	VectorScale(in, 1 / in[3], in);


	*winx = view[0] + (1 + in[0]) * view[2] / 2;
	*winy = view[1] + (1 + in[1]) * view[3] / 2;
	*winz = (1 + in[2]) / 2;

	return 1;
}

typedef struct player_autoid_s {
	float x, y;
	player_info_t *player;
} autoid_player_t;

static autoid_player_t autoids[MAX_CLIENTS];
static int autoid_count;

#define ISDEAD(i) ((i) >= 41 && (i) <= 102)

void SCR_SetupAutoID (void) {
	int j, view[4], tracknum = -1;
	float model[16], project[16], winz, *origin;
	player_state_t *state;
	player_info_t *info;
	item_vis_t visitem;
	autoid_player_t *id;
	extern cvar_t v_viewheight;
	extern int cl_playerindex, cl_h_playerindex;

	autoid_count = 0;

	if (!scr_autoid.value)
		return;

	if (cls.state != ca_active || !cl.validsequence)
		return;

	if (!cls.demoplayback && !cl.spectator)
		return;

	glGetFloatv(GL_MODELVIEW_MATRIX, model);
	glGetFloatv(GL_PROJECTION_MATRIX, project);
	glGetIntegerv(GL_VIEWPORT, view);

	if (cl.spectator)
		tracknum = Cam_TrackNum();


	VectorCopy(vpn, visitem.forward);
	VectorCopy(vright, visitem.right);
	VectorCopy(vup, visitem.up);
	VectorCopy(r_origin, visitem.vieworg);

	state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate;
	info = cl.players;

	for (j = 0; j < MAX_CLIENTS; j++, info++, state++) {
		if (state->messagenum != cl.parsecount || j == cl.playernum || j == tracknum || info->spectator)
			continue;

		if ((state->modelindex == cl_playerindex && ISDEAD(state->frame)) || state->modelindex == cl_h_playerindex)
			continue;

		if (R_CullSphere(state->origin, 0))
			continue;


		VectorCopy (state->origin, visitem.entorg);
		visitem.entorg[2] += 27;
		VectorSubtract (visitem.entorg, visitem.vieworg, visitem.dir);
		visitem.dist = DotProduct (visitem.dir, visitem.forward);
		visitem.radius = 25;

		if (!TP_IsItemVisible(&visitem))
			continue;

		id = &autoids[autoid_count];
		id->player = info;
		origin = state->origin;
		if (qglProject(origin[0], origin[1], origin[2] + 28, model, project, view, &id->x, &id->y, &winz))
			autoid_count++;
	}
}

void SCR_DrawAutoID (void) {
	int i, x, y;

	if (!scr_autoid.value)
		return;

	if (!cls.demoplayback && !cl.spectator)
		return;

	for (i = 0; i < autoid_count; i++) {
		x =  autoids[i].x * vid.width / glwidth;
		y =  (glheight - autoids[i].y) * vid.height / glheight;
		Draw_String(x - strlen(autoids[i].player->name) * 4, y - 8, autoids[i].player->name);
	}
}

//=============================================================================


void SCR_TileClear (void)
{
	if (cls.state != ca_active && cl.intermission) {
		Draw_TileClear (0, 0, vid.width, vid.height);
		return;
	}

	if (r_refdef.vrect.x > 0) {
		// left
		Draw_TileClear (0, 0, r_refdef.vrect.x, vid.height - sb_lines);
		// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0, 
			vid.width - (r_refdef.vrect.x + r_refdef.vrect.width), 
			vid.height - sb_lines);
	}
	if (r_refdef.vrect.y > 0) {
		// top
		Draw_TileClear (r_refdef.vrect.x, 0, r_refdef.vrect.width, 
			r_refdef.vrect.y);
	}
	if (r_refdef.vrect.y + r_refdef.vrect.height < vid.height - sb_lines) {
		// bottom
		Draw_TileClear (r_refdef.vrect.x,
			r_refdef.vrect.y + r_refdef.vrect.height, 
			r_refdef.vrect.width, 
			vid.height - sb_lines - 
			(r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void SCR_UpdateScreen (void) {
	if (block_drawing)
		return;

	vid.numpages = 2 + gl_triplebuffer.value;

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (scr_disabled_for_loading)
	{
		if (cls.realtime - scr_disabled_time > 60)
		{
			scr_disabled_for_loading = false;
			Com_Printf ("load failed.\n");
		}
		else
			return;
	}

	if (!scr_initialized || !con_initialized)
		return;                         // not initialized yet


	if (oldsbar != cl_sbar.value) {
		oldsbar = cl_sbar.value;
		vid.recalc_refdef = true;
	}

	//
	// determine size of refresh window
	//
	if (oldfov != scr_fov.value)
	{
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}

	if (oldscreensize != scr_viewsize.value)
	{
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

	if (v_contrast.value > 1 && !vid_hwgamma_enabled) {
		// scr_fullupdate = true;
		Sbar_Changed ();
	}

	if (cl_multiview.value)
		SCR_CalcRefdef(); 

//
// do 3D refresh drawing, and then update the screen
//
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);

	SCR_SetUpToDrawConsole ();
	
	V_RenderView ();

	SCR_SetupAutoID ();

	GL_Set2D ();

	R_PolyBlend ();

	//
	// draw any areas not covered by the refresh
	//
	SCR_TileClear ();

	if (r_netgraph.value)
		R_NetGraph ();

	if (scr_drawloading)
	{
		SCR_DrawLoading ();
		Sbar_Draw ();
	}
	else
	{
		if (cl.intermission == 1 && key_dest != key_menu)
		{
			Sbar_IntermissionOverlay ();
			Con_ClearNotify ();
		}
		else if (cl.intermission == 2 && key_dest != key_menu)
		{
			Sbar_FinaleOverlay ();
			SCR_CheckDrawCenterString ();
			Con_ClearNotify ();
		}
		
		if (cls.state == ca_active)
		{
			SCR_DrawRam ();
			SCR_DrawNet ();
			SCR_DrawTurtle ();
			SCR_DrawPause ();
			SCR_DrawAutoID ();
			if (!cl.intermission) {
				if (key_dest != key_menu)
					Draw_Crosshair ();
				SCR_CheckDrawCenterString ();
				SCR_DrawSpeed ();
				SCR_DrawClock ();
			
				SCR_DrawDemoClock ();
			
				SCR_DrawFPS ();

				if (cl_multiview.value && cls.mvdplayback)
					SCR_DrawStatusOp();

				if (cl_multiview.value == 2 && cls.mvdplayback) {
					if (cl_mvinset.value && CURRVIEW == 1)
						Sbar_Draw ();
					else
						true;
				} else {
					Sbar_Draw();
				} 

			}
		}
	
		SCR_DrawConsole ();	
		M_Draw ();
	}

	R_BrightenScreen ();

	V_UpdatePalette ();

	GL_EndRendering ();
}

void SCR_DrawStatusOp(void) {
	int xb, yb, xc, yc, xd, yd;
	char strng[80];
	char weapons[40];
	char w1,w2;
	char sAmmo[3];
	char pups[4];
	char armor;
	char name[16];
	byte c, c2;
	extern cvar_t cl_multiview;
	extern cvar_t cl_mvinsetcrosshair;
	extern cvar_t cl_mvinsethud;
	int i;

	if (!cl_multiview.value && !cls.mvdplayback)
		return;

	if (cl.stats[STAT_ACTIVEWEAPON] & IT_LIGHTNING || cl.stats[STAT_ACTIVEWEAPON] & IT_SUPER_LIGHTNING) {
		w1='l'; w2='g';
	} else if (cl.stats[STAT_ACTIVEWEAPON] & IT_ROCKET_LAUNCHER) {
		w1='r'; w2='l';
	} else if (cl.stats[STAT_ACTIVEWEAPON] & IT_GRENADE_LAUNCHER) {
		w1='g'; w2='l';
	} else if (cl.stats[STAT_ACTIVEWEAPON] & IT_SUPER_NAILGUN) {
		w1='s'; w2='n';
	} else if (cl.stats[STAT_ACTIVEWEAPON] & IT_NAILGUN) {
		w1='n'; w2='g';
	} else if (cl.stats[STAT_ACTIVEWEAPON] & IT_SUPER_SHOTGUN) {
		w1='s'; w2='s';
	} else if (cl.stats[STAT_ACTIVEWEAPON] & IT_SHOTGUN) {
		w1='s'; w2='g';
	} else if (cl.stats[STAT_ACTIVEWEAPON] & IT_AXE) {
		w1='a'; w2='x';
	}
	w1 |= 128; w2 |= 128;

	pups[0] = pups[1] = pups[2] = ' ';
	pups[3] = '\0';

	if (cl.stats[STAT_ITEMS] & IT_QUAD) {
		pups[0] = 'Q';
		pups[0] |= 128;
	}
	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) {
		pups[1] = 'R';
		pups[1] |= 128;
	}
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		pups[2] = 'P';
		pups[2] |= 128;
	}

	strng[0]='\0';

	c = (byte) 128;
	c2 = (byte) 0;

	for (i=0;i<8;i++) {
		weapons[i]=' ';
		weapons[8] = '\0';
	}

	if (cl.stats[STAT_ITEMS] & IT_AXE)
		weapons[0] = '1' - '0' + 0x12;
	if (cl.stats[STAT_ITEMS] & IT_SHOTGUN)
		weapons[1] = '2' - '0' + 0x12;
	if (cl.stats[STAT_ITEMS] & IT_SUPER_SHOTGUN)
		weapons[2] = '3' - '0' + 0x12;
	if (cl.stats[STAT_ITEMS] & IT_NAILGUN)
		weapons[3] = '4' - '0' + 0x12;
	if (cl.stats[STAT_ITEMS] & IT_SUPER_NAILGUN)
		weapons[4] = '5' - '0' + 0x12;
	if (cl.stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER)
		weapons[5] = '6' - '0' + 0x12;
	if (cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER)
		weapons[6] = '7' - '0' + 0x12;
	if (cl.stats[STAT_ITEMS] & IT_SUPER_LIGHTNING || cl.stats[STAT_ITEMS] & IT_LIGHTNING)
		weapons[7] = '8' - '0' + 0x12;

	armor = ' ';
	if (cl.stats[STAT_ITEMS] & IT_ARMOR1) {
		armor='g';
		armor |= 128;
	}
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR2) {
		armor='y';
		armor |= 128;
	}
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR3) {
		armor='r';
		armor |= 128;
	}
	
	strcpy(name,cl.players[nPlayernum].name);


	if (strcmp(cl.players[nPlayernum].name,"") && !cl.players[nPlayernum].spectator) {
		if (cl.players[nPlayernum].stats[STAT_HEALTH] <= 0 && cl_multiview.value == 2 && cl_mvinset.value) {
			sprintf(sAmmo, "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%.5s   %s %c%c:%-3s",name,
										"dead   ",
										w1,w2,
										sAmmo);
		}
		else if (cl.players[nPlayernum].stats[STAT_HEALTH] <= 0 && vid.width < 512) {
			sprintf(sAmmo, "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%.2s  %s %c%c:%-3s",name,
										"dead   ",
										w1,w2,
										sAmmo);
		}
		else if (cl.players[nPlayernum].stats[STAT_HEALTH] <= 0) {
			sprintf(sAmmo, "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%s   %s %c%c:%-3s",name,
										"dead   ",
										w1,w2,
										sAmmo);
		}

		else if (cl_multiview.value == 2 && cl_mvinset.value && CURRVIEW == 1) {
			sprintf(sAmmo, "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%s %.5s  %c%03d %03d %c%c:%-3s",pups,
												name,
												armor,
												cl.players[nPlayernum].stats[STAT_ARMOR],
												cl.players[nPlayernum].stats[STAT_HEALTH],
												w1,w2,
												sAmmo);
		} 
		else if (cl_multiview.value && vid.width < 512) {
			sprintf(sAmmo, "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%s %.2s %c%03d %03d %c%c:%-3s",pups,
												name,
												armor,
												cl.players[nPlayernum].stats[STAT_ARMOR],
												cl.players[nPlayernum].stats[STAT_HEALTH],
												w1,w2,
												sAmmo);
		}
		
		else {
			sprintf(sAmmo, "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%s %s  %c%03d %03d %c%c:%-3s",pups,
												name,
												armor,
												cl.players[nPlayernum].stats[STAT_ARMOR],
												cl.players[nPlayernum].stats[STAT_HEALTH],
												w1,w2,
												sAmmo);
		}
	}

	// placement
	if (cl_multiview.value == 1) {
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width/2;
			yc = vid.height/2;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
	}

	else if (cl_multiview.value == 2) {
		if (!cl_mvinset.value) {
		if (CURRVIEW == 2) { // top
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height/2 - sb_lines - 16;
			xc = vid.width/2;
			yc = vid.height/4;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height/2 - sb_lines - 8;
		}
		else if (CURRVIEW == 1) { // bottom
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width/2;
			yc = vid.height/2 + vid.height/4;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
		} else if (cl_mvinset.value) {
		if (CURRVIEW == 2) { // main
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height/2 - sb_lines - 16;
			xc = -2;
			yc = -2;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height/2 - sb_lines - 8;
		}
		else if (CURRVIEW == 1) { // top right
			xb = vid.width - strlen(strng) * 8; // hud
			yb = vid.height/3 - 16;
			if (cl_mvinsetcrosshair.value) {
				xc = vid.width - (double)(vid.width/3)/2-2;
				yc = (vid.height/3)/2;
			} else { 
				xc=yc=-2; 
			}
			xd = vid.width - strlen(weapons) * 8 - 70; // weapons
			yd = vid.height/3 - 8;
		}
		}
		
	}
	else if (cl_multiview.value == 3) {
		if (CURRVIEW == 2) { // top
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height/2 - sb_lines - 16;
			xc = vid.width/2;
			yc = vid.height/4;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height/2 - sb_lines - 8;
		}
		else if (CURRVIEW == 3) { // bl
			xb = vid.width - (vid.width/2)- strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width/4;
			yc = vid.height/2 + vid.height/4;
			xd = vid.width - (vid.width/2)- strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
		else if (CURRVIEW == 1) { // br
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width/2 + vid.width/4;
			yc = vid.height/2 + vid.height/4;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
	}
	else if (cl_multiview.value == 4) {
		if (CURRVIEW == 2) { // tl
			xb = vid.width - (vid.width/2)- strlen(strng) * 8 - 12;
			yb = vid.height/2 - sb_lines - 16;
			xc = vid.width/4;
			yc = vid.height/4;
			xd = vid.width - (vid.width/2)- strlen(weapons) * 8 - 84;
			yd = vid.height/2 - sb_lines - 8;
		}
		else if (CURRVIEW == 3) { // tr
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height/2 - sb_lines - 16;
			xc = vid.width/2 + vid.width/4;
			yc = vid.height/4;
			xd = vid.width - strlen(weapons)-25 * 8 - 8;
			yd = vid.height - sb_lines - (vid.height/1.9);
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height/2 - sb_lines - 8;
		}
		else if (CURRVIEW == 4) { // bl
			xb = vid.width - (vid.width/2)- strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width/4;
			yc = vid.height/2 + vid.height/4;
			xd = vid.width - (vid.width/2)- strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
		else if (CURRVIEW == 1) { // br
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width/2 + vid.width/4;
			yc = vid.height/2 + vid.height/4;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
	}
	if (cl_multiview.value == 2 && cl_mvinset.value)
		memcpy(cl.stats, cl.players[nTrack1duel].stats, sizeof(cl.stats));

	// fill the void
	if (cl_sbar.value && vid.width > 512 && cl_multiview.value==2 && cl_mvinset.value && cl_mvinsethud.value && cl_mvdisplayhud.value)
		Draw_Fill(vid.width/3*2,vid.height/3,vid.width, -1*sb_lines/3-1,c2);

	// crosshair
	if (nCrosshairExit && cl_sbar.value) {
		if (vid.width<=512)
			Draw_Fill(xc,((vid.height/3)-sb_lines/3)/2,1,1,c);
		else
			Draw_Fill(xc,((vid.height/3)-sb_lines/3)/2,2,2,c);
	}
	else if (nCrosshairExit) {
		if (vid.width<=512)
			Draw_Fill(xc,yc,1,1,c);
		else
			Draw_Fill(xc,yc,2,2,c);
	}

	// hud info
	if (cl_mvdisplayhud.value && !cl_mvinset.value && cl_multiview.value == 2 
		|| cl_mvdisplayhud.value && cl_multiview.value != 2)
		Draw_String(xb,yb,strng);
	else if (cl_multiview.value == 2 && cl_mvdisplayhud.value && CURRVIEW == 1 && vid.width > 512 && cl_mvinsethud.value)
		Draw_String(xb,yb,strng);

	// weapons
	if (cl_mvdisplayhud.value && !cl_mvinset.value && cl_multiview.value == 2 
		|| cl_mvdisplayhud.value && cl_multiview.value != 2)
		Draw_String(xd,yd,weapons);
	else if (cl_multiview.value == 2 && cl_mvdisplayhud.value && CURRVIEW == 1 && vid.width > 512  && cl_mvinsethud.value)
		Draw_String(xd,yd,weapons);

	// borders
	if (cl_multiview.value == 2 && !cl_mvinset.value) {
		Draw_Fill(0,vid.height/2,vid.width,1,c2);
	}
	else if (cl_multiview.value == 2 && cl_mvinset.value) {
		if (vid.width <= 512 && cl_sbar.value) {
			Draw_Fill(vid.width/3*2,vid.height/3-sb_lines/3,vid.width/3*2,1,c2);
			Draw_Fill(vid.width/3*2,0,1,vid.height/3-sb_lines/3,c2);
		} else if (vid.width>512 && cl_sbar.value && !cl_mvinsethud.value || vid.width>512 && cl_sbar.value && !cl_mvdisplayhud.value) {
			Draw_Fill(vid.width/3*2,vid.height/3-sb_lines/3+1,vid.width/3*2,1,c2);
			Draw_Fill(vid.width/3*2,0,1,vid.height/3-sb_lines/3,c2);
		}
		else {
			Draw_Fill(vid.width/3*2,vid.height/3,vid.width/3*2,1,c2);
			Draw_Fill(vid.width/3*2,0,1,vid.height/3,c2);
		}
	}
	else if (cl_multiview.value == 3) {
		Draw_Fill(vid.width/2,vid.height/2,1,vid.height/2,c2);
		Draw_Fill(0,vid.height/2,vid.width,1,c2);
	}
	else if (cl_multiview.value == 4) {
		Draw_Fill(vid.width/2,0,1,vid.height,c2);
		Draw_Fill(0,vid.height/2,vid.width,1,c2);
	}
}
