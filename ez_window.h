
#ifndef __EZ_WINDOW_H__
#define __EZ_WINDOW_H__
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

$Id: ez_window.h,v 1.55 2007-10-27 14:51:15 cokeman1982 Exp $
*/

#include "ez_controls.h"
#include "ez_scrollpane.h"

// =========================================================================================
// Window
// =========================================================================================

typedef struct ez_window_s
{
	ez_control_t			super;				// The super class.

	/*
	ez_slider_events_t		events;				// Slider specific events.
	ez_slider_eventhandlers_t event_handlers;	// Slider specific event handlers.
	ez_slider_eventcount_t	inherit_levels;
	ez_slider_eventcount_t	override_counts;*/

	ez_scrollpane_t			*scrollpane;		// The scrollpane for the window to enable scrolling.
	ez_control_t			*window_area;		// The window area where child controls are placed, also the target of the scrollpane.

	ez_control_t			*titlebar;			// The titlebar for the window.
	ez_button_t				*close_button;		// The close button in the upper right corner.
} ez_window_t;

//
// Window - Creates a new slider and initializes it.
//
ez_window_t *EZ_window_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Window - Initializes a slider.
//
void EZ_window_Init(ez_window_t *window, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Window - Destroys a window.
//
int EZ_window_Destroy(ez_control_t *self, qbool destroy_children);

//
// Window - Adds a child control to the window.
//
void EZ_window_AddChild(ez_window_t *window, ez_control_t *child);

//
// Window - Set window area virtual size (The part where you can put controls in the window).
//
void EZ_window_SetWindowAreaMinVirtualSize(ez_window_t *window, int min_virtual_width, int min_virtual_height);

#endif // __EZ_WINDOW_H__

