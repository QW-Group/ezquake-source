
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

$Id: ez_window.c,v 1.78 2007/10/27 14:51:15 cokeman1982 Exp $
*/

#include "quakedef.h"
#include "keys.h"
#include "utils.h"
#include "common_draw.h"
#include "ez_window.h"

// =========================================================================================
// Window
// =========================================================================================

//
// Window - Creates a new slider and initializes it.
//
ez_window_t *EZ_window_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags)
{
	ez_window_t *window = NULL;

	// We have to have a tree to add the control to.
	if (!tree)
	{
		return NULL;
	}

	window = (ez_window_t *)Q_malloc(sizeof(ez_window_t));
	memset(window, 0, sizeof(ez_window_t));

	EZ_window_Init(window, tree, parent, name, description, x, y, width, height, flags);
	
	return window;
}

//
// Window - Initializes a slider.
//
void EZ_window_Init(ez_window_t *window, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags)
{
	ez_control_t *window_ctrl		= (ez_control_t *)window;
	ez_control_t *titlebar_ctrl		= NULL;
	ez_control_t *scrollpane_ctrl	= NULL;
	int rh_size						= 0;

	// Initialize the inherited class first.
	EZ_control_Init(&window->super, tree, parent, name, description, x, y, width, height, flags);

	// Initilize the button specific stuff.
	((ez_control_t *)window)->CLASS_ID		= EZ_WINDOW_ID;
	((ez_control_t *)window)->ext_flags		|= (flags | control_focusable | control_contained | control_resizeable);

	rh_size = window_ctrl->resize_handle_thickness;

	// Overrided control events.
	//CONTROL_REGISTER_EVENT(slider, EZ_slider_OnDraw, OnDraw, ez_control_t);

	// Title bar.
	{
		// Set the tilebar to move it's parent.
		window->titlebar = EZ_control_Create(tree, window_ctrl, "Window titlebar", NULL, rh_size, rh_size, window_ctrl->width, 15, 
			(control_focusable | control_move_parent | control_movable | control_resizeable | control_enabled | control_anchor_viewport));

		titlebar_ctrl = (ez_control_t *)window->titlebar;

		// Set the size to fit within the resize handles.
		EZ_control_SetSize(titlebar_ctrl, (window_ctrl->width - (2 * rh_size)), 15);
		//EZ_control_SetMinVirtualSize(titlebar_ctrl, 1, 1);
		EZ_control_SetAnchor(titlebar_ctrl, (anchor_left | anchor_top | anchor_right));

		EZ_control_SetDrawOrder(titlebar_ctrl, window_ctrl->draw_order + 1, true);
		EZ_control_SetBackgroundColor(titlebar_ctrl, 100, 0, 0, 100);

		// Close button.
		{
			#define CLOSE_BUTTON_EDGE_GAP	2
			int cb_sidelength = 0;
			ez_control_t *close_ctrl = NULL;
			window->close_button = EZ_button_Create(tree, titlebar_ctrl, "Close button", NULL, 
													0, 0, 10, 10, control_enabled);

			close_ctrl = (ez_control_t *)window->close_button;

			cb_sidelength = (titlebar_ctrl->height - (2 * CLOSE_BUTTON_EDGE_GAP));

			// Position the close button CLOSE_BUTTON_EDGE_GAP number of pixels from the edge
			// of the titlebar, and size it accordingly.
			EZ_control_SetPosition(close_ctrl, -CLOSE_BUTTON_EDGE_GAP, CLOSE_BUTTON_EDGE_GAP);
			EZ_control_SetSize(close_ctrl, cb_sidelength, cb_sidelength);
			EZ_control_SetAnchor(close_ctrl, (anchor_top | anchor_right));
		}
	}

	// Scrollpane.
	{
		window->scrollpane = EZ_scrollpane_Create(tree, window_ctrl, "Window scrollpane", NULL, rh_size, (rh_size + titlebar_ctrl->height), 10, 10, 0);
		scrollpane_ctrl = (ez_control_t *)window->scrollpane;

		// Size the scrollpane to fit inside the window control.
		EZ_control_SetSize(scrollpane_ctrl, 
			(window_ctrl->width - (2 * rh_size)), 
			(window_ctrl->height - (titlebar_ctrl->height + (2 * rh_size))));

		EZ_control_SetAnchor(scrollpane_ctrl, (anchor_left | anchor_right | anchor_bottom | anchor_top));

		EZ_control_SetBackgroundColor(scrollpane_ctrl, 255, 0, 50, 50);

		// Window area.
		{
			window->window_area = EZ_control_Create(tree, scrollpane_ctrl, "Window area", NULL, 0, 0, 10, 10, (control_scrollable | control_enabled | control_resizeable));

			// Set the window area as the target of the scrollpane 
			// (this will make it a child of the scrollpane, so don't bother to add it as a child to the window itself).
			EZ_scrollpane_SetTarget(window->scrollpane, window->window_area);
			EZ_control_SetBackgroundColor(window->window_area, 0, 50, 50, 50);
			EZ_control_SetDrawOrder(window->window_area, scrollpane_ctrl->draw_order + 1, true);
		}
	}
}

//
// Window - Destroys a window.
//
int EZ_window_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_window_t *window = (ez_window_t *)self;
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDestroy, destroy_children);

	EZ_control_Destroy(self, destroy_children);

	// TODO : Remove any event handlers.

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDestroy, destroy_children);
	return 0;
}

//
// Window - Set window area virtual size (The part where you can put controls in the window).
//
void EZ_window_SetWindowAreaMinVirtualSize(ez_window_t *window, int min_virtual_width, int min_virtual_height)
{
	EZ_control_SetMinVirtualSize(window->window_area, min_virtual_width, min_virtual_height);
}

//
// Window - Adds a child control to the window.
//
void EZ_window_AddChild(ez_window_t *window, ez_control_t *child)
{
	if (!window->scrollpane->target)
	{
		Sys_Error("EZ_window_AddChild(): Window scrollpane has a NULL target.\n");
	}

	EZ_control_AddChild(window->window_area, child);
	EZ_control_SetDrawOrder(child, window->window_area->draw_order + 1, true);
}


