
#ifndef __EZ_SCROLLPANE_H__
#define __EZ_SCROLLPANE_H__
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

$Id: ez_scrollpane.h,v 1.55 2007-10-27 14:51:15 cokeman1982 Exp $
*/

#include "ez_controls.h"
#include "ez_scrollbar.h"

// =========================================================================================
// Scrollpane
// =========================================================================================

typedef enum ez_scrollpane_flags_e
{
	always_v_scrollbar	= (1 << 0),
	always_h_scrollbar	= (1 << 1)
} ez_scrollpane_flags_t;		

typedef enum ez_scrollpane_iflags_e
{
	show_v_scrollbar		= (1 << 0),	// Should the vertical scrollbar be shown?
	show_h_scrollbar		= (1 << 1),	// Should the horizontal scrollbar be shown?
	resizing_target			= (1 << 2)	// Are we resizing the target (if we are, don't care about OnResize events, it'll cause an infinite loop).
} ez_scrollpane_iflags_t;

typedef struct ez_scrollpane_eventcount_s
{
	int OnTargetChanged;
	int OnScrollbarThicknessChanged;
} ez_scrollpane_eventcount_t;

typedef struct ez_scrollpane_events_s
{
	ez_event_fp	OnTargetChanged;
	ez_event_fp	OnTargetResize;
	ez_event_fp	OnTargetScroll;
	ez_event_fp	OnScrollbarThicknessChanged;
} ez_scrollpane_events_t;

typedef struct ez_scrollpane_eventhandlers_s
{
	ez_eventhandler_t		*OnTargetChanged;
	ez_eventhandler_t		*OnTargetResize;
	ez_eventhandler_t		*OnTargetScroll;
	ez_eventhandler_t		*OnScrollbarThicknessChanged;
} ez_scrollpane_eventhandlers_t;

typedef struct ez_scrollpane_s
{
	ez_control_t				super;				// The super class.

	ez_scrollpane_events_t		events;
	ez_scrollpane_eventhandlers_t event_handlers;
	ez_scrollpane_eventcount_t	inherit_levels;
	ez_scrollpane_eventcount_t	override_counts;

	ez_control_t				*target;			// The target of the scrollbar if the parent isn't targeted (ext_flag & target_parent).
	ez_control_t				*prev_target;		// The previous target control. We keep this to clean up when we change target.

	ez_scrollbar_t				*v_scrollbar;		// The vertical scrollbar.
	ez_scrollbar_t				*h_scrollbar;		// The horizontal scrollbar.

	int							scrollbar_thickness; // The thickness of the scrollbars.

	ez_scrollpane_flags_t		ext_flags;			// External flags for the scrollbar.
	ez_scrollpane_iflags_t		int_flags;			// The internal flags for the scrollbar.
} ez_scrollpane_t;

//
// Scrollpane - Creates a new Scrollpane and initializes it.
//
ez_scrollpane_t *EZ_scrollpane_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Scrollpane - Initializes a Scrollpane.
//
void EZ_scrollpane_Init(ez_scrollpane_t *scrollpane, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Scrollpane - Destroys the scrollpane.
//
int EZ_scrollpane_Destroy(ez_control_t *self, qbool destroy_children);

//
// Scrollpane - Set the target control of the scrollpane (the one to be scrolled).
//
void EZ_scrollpane_SetTarget(ez_scrollpane_t *scrollpane, ez_control_t *target);

//
// Scrollpane - Set the thickness of the scrollbar controls.
//
void EZ_scrollpane_SetScrollbarThickness(ez_scrollpane_t *scrollpane, int scrollbar_thickness);

//
// Scrollpane - The target control changed.
//
int EZ_scrollpane_OnTargetChanged(ez_control_t *self);

//
// Scrollpane - The scrollbar thickness changed.
//
int EZ_scrollpane_OnScrollbarThicknessChanged(ez_control_t *self);

//
// Scrollpane - OnResize event.
//
int EZ_scrollpane_OnResize(ez_control_t *self);

#endif // __EZ_SCROLLPANE_H__
