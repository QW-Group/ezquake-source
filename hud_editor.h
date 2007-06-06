/**
	
	HUD Editor module

	made by jogihoogi, Feb 2007
	last edit:
	$Id: hud_editor.h,v 1.9 2007-06-06 15:34:59 cokeman1982 Exp $

*/

#ifndef __HUD_EDITOR_H__
#define __HUD_EDITOR_H__

// hud editor drawing function
void HUD_Editor_Draw(void);

// hud editor initialization
void HUD_Editor_Init(void);

// key press processing function
void HUD_Editor_Key(int, int);

//
// Should this HUD element be fully drawn or not when in align mode
// when using the HUD editor.  
//
qbool HUD_Editor_ConfirmDraw(hud_t *hud);

typedef enum
{
	hud_editmode_off,
	hud_editmode_align,
	hud_editmode_place,
	hud_editmode_move_resize,
	hud_editmode_resize,
	hud_editmode_move_lockedaxis,
	hud_editmode_hudmenu,
	hud_editmode_menu,
	hud_editmode_hoverlist,
	hud_editmode_normal
} hud_editor_mode_t;

extern hud_editor_mode_t	hud_editor_mode;
extern hud_t				*selected_hud;

#endif // __HUD_EDITOR_H__
