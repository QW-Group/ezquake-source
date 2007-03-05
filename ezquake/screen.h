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
// screen.h

#define		SCR_NEED_CONSOLE_BACKGROUND		(cls.state < ca_active && !cl.intermission)

void SCR_Init (void);

void SCR_UpdateScreen (void);
void SCR_UpdateWholeScreen (void);
void SCR_AutoScreenshot(char *matchname);

void SCR_CenterPrint (char *str);

extern	float		scr_con_current;
extern	float		scr_conlines;		// lines of console to display

extern	int			scr_fullupdate;	// set to 0 to force full redraw
extern	int			sb_lines;

extern	int			clearnotify;	// set to 0 whenever notify text is drawn
extern	qbool	scr_disabled_for_loading;

extern	cvar_t		scr_viewsize;

// only the refresh window will be updated unless these variables are flagged 
extern	int			scr_copytop;
extern	int			scr_copyeverything;

qbool	scr_skipupdate;

qbool	block_drawing;

// QW262 HUD -->
typedef char* (*Hud_Func)();

typedef struct hud_element_s {
	struct hud_element_s*	next;
	char					*name;
	unsigned				flags;
	signed char				coords[4]; // pos_type, x, y, bg
	unsigned				width;
	float					blink;
	void*					contents;
	int					charset;
	float					alpha;
	char					*f_hover, *f_button;
	unsigned				scr_width, scr_height;
} hud_element_t;

#define		HUD_CVAR		1
#define		HUD_FUNC		2
#define		HUD_STRING		4
#define		HUD_BLINK_F		8
#define		HUD_BLINK_B		16
#define		HUD_IMAGE		32

#define		HUD_ENABLED		512
// <-- QW262 HUD

void Hud_262Init (void);
hud_element_t *Hud_FindElement(char *name);

// Flash & Conc for TF
extern qbool	concussioned;
extern qbool flashed;

typedef struct mouse_state_s {
	double x;	// current mouse pointer horisontal position
	double y;	// current mouse pointer vertical position
	double x_old;	// previous mouse pointer positions
	double y_old;
} mouse_state_t;
