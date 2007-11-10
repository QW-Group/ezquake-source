
#ifndef __EZ_BUTTON_H__
#define __EZ_BUTTON_H__
/*
Copyright (C) 2007 ezQuake team

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

$Id: ez_button.h,v 1.55 2007-10-27 14:51:15 cokeman1982 Exp $
*/

#include "ez_controls.h"
#include "ez_label.h"

// =========================================================================================
// Button
// =========================================================================================

#define EZ_BUTTON_DEFAULT_NORMAL_IMAGE	"gfx/ui/button_normal"
#define EZ_BUTTON_DEFAULT_HOVER_IMAGE	"gfx/ui/button_hover"
#define EZ_BUTTON_DEFAULT_PRESSED_IMAGE	"gfx/ui/button_pressed"

typedef struct ez_button_eventcount_s
{
	int	OnAction;
	int	OnTextAlignmentChanged;
} ez_button_eventcount_t;

typedef struct ez_button_events_s
{
	ez_event_fp	OnAction;				// The event that's raised when the button is clicked / activated via a button.
	ez_event_fp	OnTextAlignmentChanged;	// Text alignment changed.
} ez_button_events_t;

typedef struct ez_button_eventhandlers_s
{
	ez_eventhandler_t		*OnAction;
	ez_eventhandler_t		*OnTextAlignmentChanged;
} ez_button_eventhandlers_t;

typedef enum ez_button_flags_e
{
	use_images	= (1 << 0),
	tile_center	= (1 << 1),
	tile_edges	= (1 << 2)
} ez_button_flags_t;

typedef enum ez_textalign_e
{
	top_left,
	top_center,
	top_right,
	middle_left,
	middle_center,
	middle_right,
	bottom_left,
	bottom_center,
	bottom_right
} ez_textalign_t;

typedef struct ez_button_s
{
	ez_control_t			super;				// The super class.

	ez_button_events_t		events;				// Specific events for the button control.
	ez_button_eventhandlers_t event_handlers;	// Specific event handlers for the button control.
	ez_button_eventcount_t	inherit_levels;
	ez_button_eventcount_t	override_counts;

	ez_label_t				*text_label;		// The buttons text label.

	mpic_t					*focused_image;		// The image for the button when it is focused.
	mpic_t					*normal_image;		// The normal image used for the button.
	mpic_t					*hover_image;		// The image that is shown for the button when the mouse is over it.
	mpic_t					*pressed_image;		// The image that is shown when the button is pressed.

	byte					color_focused[4];	// Color of the focus indicator of the button.
	byte					color_normal[4];	// The normal color of the button.
	byte					color_hover[4];		// Color when the button is hovered.
	byte					color_pressed[4];	// Color when the button is pressed.

	ez_button_flags_t		ext_flags;

	int						padding_top;
	int						padding_left;
	int						padding_right;
	int						padding_bottom;
	ez_textalign_t			text_alignment;		// Text alignment.
	clrinfo_t				focused_text_color;	// Text color when the button is focused.
	clrinfo_t				normal_text_color;	// Text color when the button is in it's normal state.
	clrinfo_t				hover_text_color;	// Text color when the button is hovered.
	clrinfo_t				pressed_text_color;	// Text color when the button is pressed.

	int						override_count;		// These are needed so that subclasses can override button specific events.
	int						inheritance_level;
} ez_button_t;

//
// Button - Creates a new button and initializes it.
//
ez_button_t *EZ_button_Create(ez_tree_t *tree, ez_control_t *parent, 
							  char *name, char *description, 
							  int x, int y, int width, int height, 
							  ez_control_flags_t flags);

//
// Button - Initializes a button.
//
void EZ_button_Init(ez_button_t *button, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Button - Destroys the button.
//
void EZ_button_Destroy(ez_control_t *self, qbool destroy_children);

//
// Button - OnTextAlignmentChanged event.
//
int EZ_button_OnTextAlignmentChanged(ez_control_t *self);

//
// Button - OnAction event handler.
//
int EZ_button_OnAction(ez_control_t *self);

//
// Button - OnResize event handler.
//
int EZ_button_OnResize(ez_control_t *self);

//
// Button - Use images for the button?
//
void EZ_button_SetUseImages(ez_button_t *button, qbool useimages);

//
// Button - Set the text of the button. 
//
void EZ_button_SetText(ez_button_t *button, const char *text);

//
// Button - Set the text of the button. 
//
void EZ_button_SetTextAlignment(ez_button_t *button, ez_textalign_t text_alignment);

//
// Button - Set if the edges of the button should be tiled or stretched.
//
void EZ_button_SetTileCenter(ez_button_t *button, qbool tileedges);

//
// Button - Set if the center of the button should be tiled or stretched.
//
void EZ_button_SetTileCenter(ez_button_t *button, qbool tilecenter);

//
// Button - Set the event handler for the OnTextAlignmentChanged event.
//
void EZ_button_AddOnTextAlignmentChanged(ez_button_t *button, ez_eventhandler_fp OnTextAlignmentChanged, void *payload);

//
// Button - Set the normal image for the button.
//
void EZ_button_SetNormalImage(ez_button_t *button, const char *normal_image);

//
// Button - Set the hover image for the button.
//
void EZ_button_SetHoverImage(ez_button_t *button, const char *hover_image);

//
// Button - Set the hover image for the button.
//
void EZ_button_SetPressedImage(ez_button_t *button, const char *pressed_image);

// 
// Button - Sets the normal color of the button.
//
void EZ_button_SetNormalColor(ez_button_t *self, byte r, byte g, byte b, byte alpha);

// 
// Button - Sets the pressed color of the button.
//
void EZ_button_SetPressedColor(ez_button_t *self, byte r, byte g, byte b, byte alpha);

// 
// Button - Sets the hover color of the button.
//
void EZ_button_SetHoverColor(ez_button_t *self, byte r, byte g, byte b, byte alpha);

// 
// Button - Sets the focused color of the button.
//
void EZ_button_SetFocusedColor(ez_button_t *self, byte r, byte g, byte b, byte alpha);

// 
// Button - Sets the OnAction event handler.
//
void EZ_button_AddOnAction(ez_button_t *self, ez_eventhandler_fp OnAction, void *payload);

//
// Button - OnDraw event.
//
int EZ_button_OnDraw(ez_control_t *self);

#endif // __EZ_BUTTON_H__
