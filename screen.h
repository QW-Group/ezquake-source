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

/**
  $Id: screen.h,v 1.10 2007-09-26 21:51:33 tonik Exp $
  
  Common declarations for modules associated with drawing on screen
  cl_screen.c, sbar.c
*/

#ifndef EZQUAKE_SCREEN_HEADER
#define EZQUAKE_SCREEN_HEADER

#define		SCR_NEED_CONSOLE_BACKGROUND		(cls.state < ca_active && !cl.intermission)

void SCR_Init (void);

#define UPDATESCREEN_MULTIVIEW   1
#define UPDATESCREEN_POSTPROCESS 2
#define UPDATESCREEN_3D_ONLY     4
#define UPDATESCREEN_2D_ONLY     8

qbool SCR_UpdateScreen (void);
qbool SCR_UpdateScreenPrePlayerView (void);
void SCR_UpdateScreenPlayerView(int flags);
void SCR_UpdateScreenHudOnly(void);
void SCR_UpdateScreenPostPlayerView (void);

void SCR_UpdateWholeScreen (void);
void SCR_AutoScreenshot(char *matchname);
qbool SCR_TakingAutoScreenshot(void);

void SCR_CenterPrint(const char *str);
void SCR_CenterPrint_Clear(void);
void SCR_CenterPrint_Init(void);
void SCR_CenterString_Draw(void);

extern	float		scr_con_current;
extern	float		scr_conlines;		// lines of console to display

extern	int			scr_fullupdate;	// set to 0 to force full redraw
extern	int			sb_lines;	// sbar.c !!

extern	int			clearnotify;	// set to 0 whenever notify text is drawn
extern	qbool		scr_disabled_for_loading;

extern	cvar_t		scr_viewsize;

// only the refresh window will be updated unless these variables are flagged 
extern	int			scr_copytop;
extern	int			scr_copyeverything;

extern	qbool		scr_skipupdate;
extern	qbool		block_drawing;

// QW262 HUD
typedef char* (*Hud_Func)(void);

#define		HUD_CVAR		1
#define		HUD_FUNC		2
#define		HUD_STRING		4
#define		HUD_BLINK_F		8
#define		HUD_BLINK_B		16
#define		HUD_IMAGE		32
#define		HUD_ENABLED		512

typedef struct hud_element_s {
	struct hud_element_s*	next;
	char					*name;
	unsigned				flags;
	signed char				coords[4]; // pos_type, x, y, bg
	unsigned				width;
	float					blink;
	void*					contents;
	//int					charset;
	float					alpha;
	char					*f_hover, *f_button;
	unsigned				scr_width, scr_height;
} hud_element_t;

void Hud_262Init (void);
void Hud_262LoadOnFirstStart(void);
void Hud_262CatchStringsOnLoad(char *line);

qbool Hud_ElementExists(const char* name);

// Flash & Conc for TF
extern qbool	concussioned;
extern qbool flashed;

#define	ELEMENT_X_COORD(var)	((var##_x.value < 0) ? vid.width - strlen(str) * 8 + 8 * var##_x.value: 8 * var##_x.value)
#define	ELEMENT_Y_COORD(var)	((var##_y.value < 0) ? vid.height - sb_lines + 8 * var##_y.value : 8 * var##_y.value)

void SCR_OnChangeMVHudPos(cvar_t *var, char *newval, qbool *cancel);
void SCR_TileClear(void);
void SCR_RestoreAutoID(void);
void SCR_SaveAutoID(void);
void SCR_SetupAutoID(void);
void SCR_RegisterAutoIDCvars(void);
void SCR_DrawAutoID(void);
void SCR_DrawAntilagIndicators(void);
void SShot_RegisterCvars(void);
qbool SCR_TakingAutoScreenshot(void);
void SCR_CheckAutoScreenshot(void);

// the current position of the mouse pointer
extern double cursor_x, cursor_y;

// kazik, HUD, incremented every screen update, never reset
extern  int         host_screenupdatecount;

// scr_teaminfo

#define TEAMINFO_SHOWSELF() ((scr_teaminfo.integer == 1) && (scr_teaminfo_show_self.integer >= 1 && cls.mvdplayback))
#define TEAMINFO_NICKLEN 5
#define FONTWIDTH (8)

/*do not show player if no info about him during this time, affect us if we lagging too*/
#define TI_TIMEOUT (5)

typedef struct ti_player_s {

	int 		client;

	vec3_t		org;
	int			items;
	int			health;
	int			armor;
	char		nick[TEAMINFO_NICKLEN]; // yeah, nick is nick, must be short
	double		time; // when we recive last update about this player, so we can guess disconnects and etc

} ti_player_t;

char *SCR_GetWeaponShortNameByFlag (int flag);

//Must be called whenever vid changes
void SCR_CalcRefdef(void);
void SCR_DrawMultiviewIndividualElements(void);

// end - scr_teaminfo

#endif // EZQUAKE_SCREEN_HEADER
