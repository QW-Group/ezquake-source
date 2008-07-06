
#ifndef __EZ_SCROLLBAR_H__
#define __EZ_SCROLLBAR_H__
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

$Id: ez_scrollbar.h,v 1.55 2007-10-27 14:51:15 cokeman1982 Exp $
*/

#include "ez_controls.h"
#include "ez_button.h"

// =========================================================================================
// Scrollbar
// =========================================================================================

typedef enum ez_orientation_s
{
	vertical	= 0,
	horizontal	= 1
} ez_orientation_t;

typedef enum ez_scrollbar_flags_e
{
	target_parent	= (1 << 0)
} ez_scrollbar_flags_t;

typedef enum ez_scrollbar_iflags_e
{
	sliding			= (1 << 0),
	scrolling		= (1 << 1)
} ez_scrollbar_iflags_t;

typedef struct ez_scrollbar_eventcount_s
{
	int OnTargetChanged;
} ez_scrollbar_eventcount_t;

typedef struct ez_scrollbar_events_s
{
	ez_event_fp	OnTargetChanged;
} ez_scrollbar_events_t;

typedef struct ez_scrollbar_eventhandlers_s
{
	ez_eventhandler_t		*OnTargetChanged;
} ez_scrollbar_eventhandlers_t;

typedef struct ez_scrollbar_s
{
	ez_control_t				super;				// The super class.

	ez_scrollbar_events_t		events;
	ez_scrollbar_eventhandlers_t event_handlers;
	ez_scrollbar_eventcount_t	inherit_levels;
	ez_scrollbar_eventcount_t	override_counts;

	ez_button_t					*back;				// Up / left button depending on the scrollbars orientation.
	ez_button_t					*slider;			// The slider button.
	ez_button_t					*forward;			// Down / right button.
	
	ez_orientation_t			orientation;		// The orientation of the scrollbar, vertical / horizontal.
	int							scroll_area;		// The width or height (depending on orientation) of the area between
													// the forward/back buttons. That is, the area you can move the slider in.

	int							slider_minsize;		// The minimum size of the slider button.
	int							scroll_delta_x;		// How much should the scrollbar scroll it's parent when the scroll buttons are pressed?
	int							scroll_delta_y;

	ez_control_t				*target;			// The target of the scrollbar if the parent isn't targeted (ext_flag & target_parent).

	ez_scrollbar_flags_t		ext_flags;			// External flags for the scrollbar.
	ez_scrollbar_iflags_t		int_flags;			// The internal flags for the scrollbar.
} ez_scrollbar_t;

//
// Scrollbar - Creates a new scrollbar and initializes it.
//
ez_scrollbar_t *EZ_scrollbar_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Scrollbar - Initializes a scrollbar.
//
void EZ_scrollbar_Init(ez_scrollbar_t *scrollbar, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Scrollbar - Destroys the scrollbar.
//
int EZ_scrollbar_Destroy(ez_control_t *self, qbool destroy_children);

//
// Scrollbar - Set the target control that the scrollbar should scroll.
//
void EZ_scrollbar_SetTarget(ez_scrollbar_t *scrollbar, ez_control_t *target);

//
// Scrollbar - Set if the scrollbar should be vertical or horizontal.
//
void EZ_scrollbar_SetIsVertical(ez_scrollbar_t *scrollbar, qbool is_vertical);

//
// Scrollbar - Sets if the target for the scrollbar should be it's parent, or a specified target control.
//				(The target controls OnScroll event handler will be used if it's not the parent)
//
void EZ_scrollbar_SetTargetIsParent(ez_scrollbar_t *scrollbar, qbool targetparent);

//
// Scrollbar - The target of the scrollbar changed.
//
int EZ_scrollbar_OnTargetChanged(ez_control_t *self, void *ext_event_info);

//
// Scrollbar - OnResize event.
//
int EZ_scrollbar_OnResize(ez_control_t *self, void *ext_event_info);

//
// Scrollbar - OnParentResize event.
//
int EZ_scrollbar_OnParentResize(ez_control_t *self, void *ext_event_info);

//
// Scrollbar - OnMouseDown event.
//
int EZ_scrollbar_OnMouseDown(ez_control_t *self, mouse_state_t *ms);

//
// Scrollbar - OnMouseUpOutside event.
//
int EZ_scrollbar_OnMouseUpOutside(ez_control_t *self, mouse_state_t *ms);

//
// Scrollbar - Mouse event.
//
int EZ_scrollbar_OnMouseEvent(ez_control_t *self, mouse_state_t *ms);

//
// Scrollbar - OnParentScroll event.
//
int EZ_scrollbar_OnParentScroll(ez_control_t *self, void *ext_event_info);

#endif // __EZ_SCROLLBAR_H__
