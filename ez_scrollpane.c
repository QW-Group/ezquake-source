
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

$Id: ez_scrollpane.c,v 1.78 2007/10/27 14:51:15 cokeman1982 Exp $
*/

#include "quakedef.h"
#include "keys.h"
#include "utils.h"
#include "common_draw.h"
#include "ez_controls.h"
#include "ez_scrollpane.h"

#ifdef _WIN32
#pragma warning( disable : 4189 )
#endif

// =========================================================================================
// Scrollpane
// =========================================================================================

//
// Scrollpane - Adjusts the scrollpanes target to fit in between the scrollbars.
//
static void EZ_scrollpane_AdjustTargetSize(ez_scrollpane_t *scrollpane)
{
	ez_control_t *scrollpane_ctrl	= (ez_control_t *)scrollpane;

	qbool show_v	= (scrollpane->ext_flags & always_h_scrollbar) || (scrollpane->int_flags & show_v_scrollbar);
	qbool show_h	= (scrollpane->ext_flags & always_v_scrollbar) || (scrollpane->int_flags & show_h_scrollbar);
	int rh_size_v	= (scrollpane_ctrl->ext_flags & control_resize_v ? scrollpane_ctrl->resize_handle_thickness : 0); 
	int rh_size_h	= (scrollpane_ctrl->ext_flags & control_resize_h ? scrollpane_ctrl->resize_handle_thickness : 0); 

	// Make sure we don't listen to the targets OnResize event when resizing it ourselves
	// or we'll get a stack overflow due to an infinite loop :S
	scrollpane->int_flags |= resizing_target;
	
	EZ_control_SetSize(scrollpane->target, 
						(scrollpane_ctrl->width  - (show_v ? scrollpane->scrollbar_thickness : 0) - rh_size_h),
						(scrollpane_ctrl->height - (show_h ? scrollpane->scrollbar_thickness : 0) - rh_size_v));

	scrollpane->int_flags &= ~resizing_target;
}

//
// Scrollpane - Resize the scrollbars based on the targets state.
//
static void EZ_scrollpane_ResizeScrollbars(ez_scrollpane_t *scrollpane)
{
	ez_control_t *scrollpane_ctrl	= (ez_control_t *)scrollpane;
	ez_control_t *v_scroll_ctrl		= (ez_control_t *)scrollpane->v_scrollbar;
	ez_control_t *h_scroll_ctrl		= (ez_control_t *)scrollpane->h_scrollbar;

//	qbool size_changed	= false;
	qbool show_v		= (scrollpane->ext_flags & always_h_scrollbar) || (scrollpane->int_flags & show_v_scrollbar);
	qbool show_h		= (scrollpane->ext_flags & always_v_scrollbar) || (scrollpane->int_flags & show_h_scrollbar);
	int rh_size_v		= (scrollpane_ctrl->ext_flags & control_resize_v ? scrollpane_ctrl->resize_handle_thickness : 0); 
	int rh_size_h		= (scrollpane_ctrl->ext_flags & control_resize_h ? scrollpane_ctrl->resize_handle_thickness : 0); 

	EZ_control_SetVisible((ez_control_t *)scrollpane->v_scrollbar, show_v);
	EZ_control_SetVisible((ez_control_t *)scrollpane->h_scrollbar, show_h);

	// Resize the scrollbars depending on if both are shown or not.
	EZ_control_SetSize(v_scroll_ctrl,
						scrollpane->scrollbar_thickness, 
						scrollpane_ctrl->height - (show_h ? scrollpane->scrollbar_thickness : 0) - (2 * rh_size_v));

	EZ_control_SetSize(h_scroll_ctrl,
						scrollpane_ctrl->width - (show_v ? scrollpane->scrollbar_thickness : 0) - (2 * rh_size_h), 
						scrollpane->scrollbar_thickness);

	// Resize the target control so that it doesn't overlap with the scrollbars.
	if (scrollpane->target)
	{
		// Since this resize operation is going to raise a new OnResize event on the target control
		// which in turn calls this function again we only change the size of the target to fit
		// within the scrollbars when the scrollbars actually changed size. Otherwise we'd get an infinite recursion.
		EZ_scrollpane_AdjustTargetSize(scrollpane);
	}
}

//
// Scrollpane - Determine if the scrollbars should be visible based on the target controls state.
//
static void EZ_scrollpane_DetermineScrollbarVisibility(ez_scrollpane_t *scrollpane)
{
	SET_FLAG(scrollpane->int_flags, show_v_scrollbar, (scrollpane->target->height <= scrollpane->target->virtual_height_min));
	SET_FLAG(scrollpane->int_flags, show_h_scrollbar, (scrollpane->target->width <= scrollpane->target->virtual_width_min));
}

// TODO : Add an event handler for handling when the target control changes it's min virtual size, we might need to show/hide the scrollbars if that happens.

//
// Scrollpane - Target changed it's virtual size.
//
static int EZ_scrollpane_OnTargetVirtualResize(ez_control_t *self, void *payload, void *ext_event_info)
{
	ez_scrollpane_t *scrollpane = NULL;

	if (!self->parent || (self->parent->CLASS_ID != EZ_SCROLLPANE_ID))
	{
		Sys_Error("EZ_scrollpane_OnTargetVirtualResize(): Target parent is not a scrollpane!.\n");
	}

	scrollpane = (ez_scrollpane_t *)self->parent;

	// Check if we should show/hide the scrollbars.
	EZ_scrollpane_DetermineScrollbarVisibility(scrollpane);

	return 0;
}

//
// Scrollpane - Creates a new Scrollpane and initializes it.
//
ez_scrollpane_t *EZ_scrollpane_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags)
{
	ez_scrollpane_t *scrollpane = NULL;

	// We have to have a tree to add the control to.
	if (!tree)
	{
		return NULL;
	}

	scrollpane = (ez_scrollpane_t *)Q_malloc(sizeof(ez_scrollpane_t));
	memset(scrollpane, 0, sizeof(ez_scrollpane_t));

	EZ_scrollpane_Init(scrollpane, tree, parent, name, description, x, y, width, height, flags);
	
	return scrollpane;
}

//
// Scrollpane - Initializes a Scrollpane.
//
void EZ_scrollpane_Init(ez_scrollpane_t *scrollpane, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags)
{
	int rh_size_h = 0;
	int rh_size_v = 0;
	ez_control_t *scrollpane_ctrl = (ez_control_t *)scrollpane;

	// Initialize the inherited class first.
	EZ_control_Init(&scrollpane->super, tree, parent, name, description, x, y, width, height, flags);

	// Initilize the button specific stuff.
	((ez_control_t *)scrollpane)->CLASS_ID	= EZ_SCROLLPANE_ID;
	((ez_control_t *)scrollpane)->ext_flags	|= (flags | control_focusable | control_contained | control_resizeable);

	// Override control events.
	CONTROL_REGISTER_EVENT(scrollpane, EZ_scrollpane_OnResize, OnResize, ez_control_t);

	// Scrollpane specific events.
	CONTROL_REGISTER_EVENT(scrollpane, EZ_scrollpane_OnTargetChanged, OnTargetChanged, ez_scrollpane_t);
	CONTROL_REGISTER_EVENT(scrollpane, EZ_scrollpane_OnScrollbarThicknessChanged, OnScrollbarThicknessChanged, ez_scrollpane_t);

	scrollpane->scrollbar_thickness = 10;

	rh_size_h = (scrollpane_ctrl->ext_flags & control_resize_h) ? scrollpane_ctrl->resize_handle_thickness : 0;
	rh_size_v = (scrollpane_ctrl->ext_flags & control_resize_v) ? scrollpane_ctrl->resize_handle_thickness : 0;

	EZ_control_SetMinVirtualSize(scrollpane_ctrl, 1, 1);

	scrollpane->int_flags |= (show_h_scrollbar | show_v_scrollbar);

	// Create vertical scrollbar.
	{
		scrollpane->v_scrollbar = EZ_scrollbar_Create(tree, scrollpane_ctrl, "Vertical scrollbar", "", 0, 0, 10, 10, control_anchor_viewport);
		EZ_control_SetVisible((ez_control_t *)scrollpane->v_scrollbar, true);
		EZ_control_SetPosition((ez_control_t *)scrollpane->v_scrollbar, -rh_size_h, rh_size_v);
		EZ_control_SetAnchor((ez_control_t *)scrollpane->v_scrollbar, (anchor_top | anchor_bottom | anchor_right));

		EZ_scrollbar_SetTargetIsParent(scrollpane->v_scrollbar, false);
 
		EZ_control_AddChild(scrollpane_ctrl, (ez_control_t *)scrollpane->v_scrollbar);
	}

	// Create horizontal scrollbar.
	{
		scrollpane->h_scrollbar = EZ_scrollbar_Create(tree, (ez_control_t *)scrollpane, "Horizontal scrollbar", "", 0, 0, 10, 10, control_anchor_viewport);
	
		EZ_control_SetVisible((ez_control_t *)scrollpane->h_scrollbar, true);
		EZ_control_SetPosition((ez_control_t *)scrollpane->h_scrollbar, rh_size_h, -rh_size_v);
		EZ_control_SetAnchor((ez_control_t *)scrollpane->h_scrollbar, (anchor_left | anchor_bottom | anchor_right));

		EZ_scrollbar_SetTargetIsParent(scrollpane->h_scrollbar, false);
		EZ_scrollbar_SetIsVertical(scrollpane->h_scrollbar, false);

		EZ_control_AddChild(scrollpane_ctrl, (ez_control_t *)scrollpane->h_scrollbar);
	}

	EZ_scrollpane_ResizeScrollbars(scrollpane);
}

//
// Scrollpane - Destroys the scrollpane.
//
int EZ_scrollpane_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_scrollpane_t *scrollpane = (ez_scrollpane_t *)self;
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDestroy, destroy_children);

	EZ_eventhandler_Remove(scrollpane->event_handlers.OnScrollbarThicknessChanged, NULL, true);
	EZ_eventhandler_Remove(scrollpane->event_handlers.OnTargetChanged, NULL, true);

	EZ_control_Destroy(self, destroy_children);

	return 0;
}

//
// Scrollpane - Always show the vertical scrollbar.
//
void EZ_scrollpane_SetAlwaysShowVerticalScrollbar(ez_scrollpane_t *scrollpane, qbool always_show)
{
	SET_FLAG(scrollpane->ext_flags, always_v_scrollbar, always_show);
}

//
// Scrollpane - Always show the horizontal scrollbar.
//
void EZ_scrollpane_SetAlwaysShowHorizontalScrollbar(ez_scrollpane_t *scrollpane, qbool always_show)
{
	SET_FLAG(scrollpane->ext_flags, always_h_scrollbar, always_show);
}

//
// Scrollpane - Set the target control of the scrollpane (the one to be scrolled).
//
void EZ_scrollpane_SetTarget(ez_scrollpane_t *scrollpane, ez_control_t *target)
{
	scrollpane->prev_target = scrollpane->target;
	scrollpane->target = target;
	CONTROL_RAISE_EVENT(NULL, scrollpane, ez_scrollpane_t, OnTargetChanged, NULL);
}

//
// Scrollpane - Set the thickness of the scrollbar controls.
//
void EZ_scrollpane_SetScrollbarThickness(ez_scrollpane_t *scrollpane, int scrollbar_thickness)
{
	scrollpane->scrollbar_thickness = scrollbar_thickness;
	CONTROL_RAISE_EVENT(NULL, scrollpane, ez_scrollpane_t, OnScrollbarThicknessChanged, NULL);
}

//
// Scrollpane - OnResize event.
//
int EZ_scrollpane_OnResize(ez_control_t *self, void *ext_event_info)
{
	ez_scrollpane_t *scrollpane = (ez_scrollpane_t *)self;

	EZ_control_OnResize(self, NULL);

	// Make sure we don't listen to resizing events of the target that we raised
	// ourselves, it causes an infinite loop.
	if (!(scrollpane->int_flags & resizing_target))
	{
		// Resize the scrollbars and target based on the state of the target.
		EZ_scrollpane_ResizeScrollbars(scrollpane);
	}
	
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnResize, NULL);
	return 0;
}

//
// Scrollpane - The target control changed.
//
int EZ_scrollpane_OnTargetChanged(ez_control_t *self, void *ext_event_info)
{
	ez_scrollpane_t *scrollpane = (ez_scrollpane_t *)self;

	// Clean up the old target.
	if (scrollpane->prev_target)
	{
		EZ_eventhandler_Remove(scrollpane->prev_target->event_handlers.OnVirtualResize, EZ_scrollpane_OnTargetVirtualResize, false);
		EZ_control_RemoveChild(self, scrollpane->target);
		EZ_control_SetMovesParent(scrollpane->prev_target, false);
	}

	// Set the new target for the scrollbars so they know what to scroll.
	EZ_scrollbar_SetTarget(scrollpane->v_scrollbar, scrollpane->target);
	EZ_scrollbar_SetTarget(scrollpane->h_scrollbar, scrollpane->target);

	if (scrollpane->target)
	{
		// Subscribe to the targets resize and scroll events.
		EZ_control_AddOnVirtualResize(scrollpane->target, EZ_scrollpane_OnTargetVirtualResize, scrollpane);
		EZ_control_AddChild(self, scrollpane->target);

		EZ_control_SetScrollable(scrollpane->target, true);
		EZ_control_SetMovable(scrollpane->target, true);
		EZ_control_SetResizeableBoth(scrollpane->target, true);
		EZ_control_SetResizeable(scrollpane->target, true);

		// Reposition the target inside the scrollpane.
		EZ_control_SetPosition(scrollpane->target, 0, 0);
		EZ_control_SetSize(scrollpane->target, (self->width - scrollpane->scrollbar_thickness), (self->height - scrollpane->scrollbar_thickness));
		EZ_control_SetAnchor(scrollpane->target, (anchor_left | anchor_right | anchor_top | anchor_bottom));

		// Make sure the target is drawn infront of the scrollpane.
		EZ_control_SetDrawOrder(scrollpane->target, ((ez_control_t *)scrollpane)->draw_order + 1, true);

		// When moving the target move the scrollpane with it
		// and don't allow moving the target inside of the scrollpane.
		EZ_control_SetMovesParent(scrollpane->target, true);

		// Resize the scrollbars / target to fit properly.
		EZ_scrollpane_DetermineScrollbarVisibility(scrollpane);
		EZ_scrollpane_ResizeScrollbars(scrollpane);
		EZ_scrollpane_AdjustTargetSize(scrollpane);

		CONTROL_RAISE_EVENT(NULL, (ez_control_t *)scrollpane->target, ez_control_t, OnResize, NULL);
		CONTROL_RAISE_EVENT(NULL, (ez_control_t *)scrollpane->target, ez_control_t, OnMove, NULL);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, scrollpane, ez_scrollpane_t, OnTargetChanged, NULL);
	return 0;
}

//
// Scrollpane - The scrollbar thickness changed.
//
int EZ_scrollpane_OnScrollbarThicknessChanged(ez_control_t *self, void *ext_event_info)
{
	ez_scrollpane_t *scrollpane = (ez_scrollpane_t *)self;

	// Resize the scrollbars and target with the new scrollbar thickness.
	EZ_scrollpane_ResizeScrollbars(scrollpane);

	CONTROL_EVENT_HANDLER_CALL(NULL, scrollpane, ez_scrollpane_t, OnScrollbarThicknessChanged, NULL);
	return 0;
}


