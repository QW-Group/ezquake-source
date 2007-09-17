/**

	HUD Editor module

	made by Cokeman, Feb 2007
	last edit:
	$Id: hud_editor.h,v 1.15 2007-09-17 11:12:29 cokeman1982 Exp $

*/

#ifndef __HUD_EDITOR_H__
#define __HUD_EDITOR_H__

// hud editor drawing function
void HUD_Editor_Draw(void);

// hud editor initialization
void HUD_Editor_Init(void);

// Mouse processing.
qbool HUD_Editor_MouseEvent (mouse_state_t *ms);

// Key press processing function.
void HUD_Editor_Key(int key, int unichar, qbool down);

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
