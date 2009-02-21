
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
#include "ez_scrollbar.h"

#ifdef _WIN32
#pragma warning( disable : 4189 )
#endif

// =========================================================================================
// Scrollbar
// =========================================================================================

//
// Scrollbar - OnMouseDown event.
//
int EZ_scrollbar_OnSliderMouseDown(ez_control_t *self, void *payload, mouse_state_t *ms)
{
	if (!self->parent || self->parent->CLASS_ID != EZ_SCROLLBAR_ID)
	{
		Sys_Error("EZ_scrollbar_OnSliderMove(): Parent of slider is not a scrollbar!");
	}
	else
	{
		ez_scrollbar_t *scrollbar = (ez_scrollbar_t *)self->parent;

		// Start sliding.
		scrollbar->int_flags |= sliding;
	}

	return true;
}

//
// Scrollbar - Generic scroll button action event handler.
//
static void EZ_scrollbar_OnScrollButtonMouseDown(ez_scrollbar_t *scrollbar, qbool back)
{
	ez_control_t *scrollbar_ctrl = (ez_control_t *)scrollbar;
	ez_control_t *scroll_target = (scrollbar->ext_flags & target_parent) ? scrollbar_ctrl->parent : scrollbar->target;

	if (!scroll_target)
	{
		return; // We have nothing to scroll.
	}

	if (scrollbar->orientation == vertical)
	{
		EZ_control_SetScrollChange(scroll_target, 0, (back ? -scrollbar->scroll_delta_y : scrollbar->scroll_delta_y));
	}
	else
	{
		EZ_control_SetScrollChange(scroll_target, (back ? -scrollbar->scroll_delta_x : scrollbar->scroll_delta_x), 0);
	}
}

//
// Scrollbar - Event handler for pressing the back button (left or up).
//
static int EZ_scrollbar_OnBackButtonMouseDown(ez_control_t *self, void *payload, mouse_state_t *ms)
{
	if (!self->parent || self->parent->CLASS_ID != EZ_SCROLLBAR_ID)
	{
		Sys_Error("EZ_scrollbar_OnBackButtonMouseDown(): Parent of back button is not a scrollbar!");
	}
	else 
	{
		ez_scrollbar_t *scrollbar = (ez_scrollbar_t *)self->parent;
		EZ_scrollbar_OnScrollButtonMouseDown(scrollbar, true);
	}

	return true;
}

//
// Scrollbar - Event handler for pressing the back button (left or up).
//
static int EZ_scrollbar_OnForwardButtonMouseDown(ez_control_t *self, void *payload, mouse_state_t *ms)
{
	if (!self->parent || self->parent->CLASS_ID != EZ_SCROLLBAR_ID)
	{
		Sys_Error("EZ_scr	ollbar_OnForwardButtonMouseDown(): Parent of forward button is not a scrollbar!");
	}
	else
	{
		ez_scrollbar_t *scrollbar = (ez_scrollbar_t *)self->parent;
		EZ_scrollbar_OnScrollButtonMouseDown(scrollbar, false);
	}

	return true;
}

//
// Scrollbar - Creates a new scrollbar and initializes it.
//
ez_scrollbar_t *EZ_scrollbar_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags)
{
	ez_scrollbar_t *scrollbar = NULL;

	// We have to have a tree to add the control to.
	if (!tree)
	{
		return NULL;
	}

	scrollbar = (ez_scrollbar_t *)Q_malloc(sizeof(ez_scrollbar_t));
	memset(scrollbar, 0, sizeof(ez_scrollbar_t));

	EZ_scrollbar_Init(scrollbar, tree, parent, name, description, x, y, width, height, flags);
	
	return scrollbar;
}

//
// Scrollbar - Initializes a scrollbar.
//
void EZ_scrollbar_Init(ez_scrollbar_t *scrollbar, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags)
{
	// Initialize the inherited class first.
	EZ_control_Init(&scrollbar->super, tree, parent, name, description, x, y, width, height, flags);

	// Initilize the button specific stuff.
	((ez_control_t *)scrollbar)->CLASS_ID	= EZ_SCROLLBAR_ID;
	((ez_control_t *)scrollbar)->ext_flags	|= (flags | control_focusable | control_contained | control_resizeable);

	// Override control events.
	CONTROL_REGISTER_EVENT(scrollbar, EZ_scrollbar_Destroy, OnDestroy, ez_control_t);
	CONTROL_REGISTER_EVENT(scrollbar, EZ_scrollbar_OnResize, OnResize, ez_control_t);
	CONTROL_REGISTER_EVENT(scrollbar, EZ_scrollbar_OnParentResize, OnParentResize, ez_control_t);
	CONTROL_REGISTER_EVENT(scrollbar, EZ_scrollbar_OnMouseEvent, OnMouseEvent, ez_control_t);
	CONTROL_REGISTER_EVENT(scrollbar, EZ_scrollbar_OnMouseDown, OnMouseDown, ez_control_t);
	CONTROL_REGISTER_EVENT(scrollbar, EZ_scrollbar_OnMouseUpOutside, OnMouseUpOutside, ez_control_t);
	CONTROL_REGISTER_EVENT(scrollbar, EZ_scrollbar_OnParentScroll, OnParentScroll, ez_control_t);

	// Scrollbar specific events.
	CONTROL_REGISTER_EVENT(scrollbar, EZ_scrollbar_OnTargetChanged, OnTargetChanged, ez_scrollbar_t);

	scrollbar->back		= EZ_button_Create(tree, (ez_control_t *)scrollbar, "Scrollbar back button", "", 0, 0, 0, 0, control_contained | control_enabled);
	scrollbar->forward	= EZ_button_Create(tree, (ez_control_t *)scrollbar, "Scrollbar forward button", "", 0, 0, 0, 0, control_contained | control_enabled);
	scrollbar->slider	= EZ_button_Create(tree, (ez_control_t *)scrollbar, "Scrollbar slider button", "", 0, 0, 0, 0, control_contained | control_enabled);
	
	EZ_control_AddOnMouseDown((ez_control_t *)scrollbar->slider, EZ_scrollbar_OnSliderMouseDown, NULL);
	EZ_control_AddOnMouseDown((ez_control_t *)scrollbar->back, EZ_scrollbar_OnBackButtonMouseDown, NULL);
	EZ_control_AddOnMouseDown((ez_control_t *)scrollbar->forward, EZ_scrollbar_OnForwardButtonMouseDown, NULL);

	scrollbar->slider_minsize = 5;
	scrollbar->scroll_delta_x = 1;
	scrollbar->scroll_delta_y = 1;

	// Listen to repeated mouse events so that we continue scrolling when
	// holding down the mouse button over the scroll arrows.
	EZ_control_SetListenToRepeatedMouseClicks((ez_control_t *)scrollbar->back, true);
	EZ_control_SetListenToRepeatedMouseClicks((ez_control_t *)scrollbar->forward, true);
	
	// TODO : Remove this test stuff.
	/*
	{
		EZ_button_SetNormalColor(scrollbar->back, 255, 125, 125, 125);
		EZ_button_SetNormalColor(scrollbar->forward, 255, 125, 125, 125);
		EZ_button_SetNormalColor(scrollbar->slider, 255, 125, 125, 125);
	}
	*/

	CONTROL_RAISE_EVENT(NULL, (ez_control_t *)scrollbar, ez_control_t, OnResize, NULL);
}

//
// Scrollbar - Destroys the scrollbar.
//
int EZ_scrollbar_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_scrollbar_t *scrollbar = (ez_scrollbar_t *)self;
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDestroy, destroy_children);

	EZ_eventhandler_Remove(scrollbar->event_handlers.OnTargetChanged, NULL, true);

	EZ_control_Destroy(self, destroy_children);

	return 0;
}

//
// Scrollbar - Calculates and sets the parents scroll position based on where the slider button is.
//
static void EZ_scrollbar_CalculateParentScrollPosition(ez_scrollbar_t *scrollbar, ez_control_t *target)
{
//	ez_control_t *self			= (ez_control_t *)scrollbar;
	ez_control_t *back_ctrl		= (ez_control_t *)scrollbar->back;
//	ez_control_t *forward_ctrl	= (ez_control_t *)scrollbar->forward;
	ez_control_t *slider_ctrl	= (ez_control_t *)scrollbar->slider;
	float scroll_ratio;

	if (!target)
	{
		return;
	}

	if (scrollbar->orientation == vertical)
	{
		scroll_ratio = (slider_ctrl->y - back_ctrl->height) / (float)scrollbar->scroll_area;
		EZ_control_SetScrollPosition(target, target->virtual_x, Q_rint(scroll_ratio * target->virtual_height));
	}
	else
	{
		scroll_ratio = (slider_ctrl->x - back_ctrl->width) / (float)scrollbar->scroll_area;
		EZ_control_SetScrollPosition(target, Q_rint(scroll_ratio * target->virtual_width), target->virtual_y);
	}
}

//
// Scrollbar - Updates the slider position of the scrollbar based on the parents scroll position.
//
static void EZ_scrollbar_UpdateSliderBasedOnTarget(ez_scrollbar_t *scrollbar, ez_control_t *target)
{
	ez_control_t *self			= (ez_control_t *)scrollbar;
	ez_control_t *back_ctrl		= (ez_control_t *)scrollbar->back;
	ez_control_t *forward_ctrl	= (ez_control_t *)scrollbar->forward;
	ez_control_t *slider_ctrl	= (ez_control_t *)scrollbar->slider;
	float scroll_ratio;

	if (!target)
	{
		return;
	}

	// Don't do anything if this is the user moving the slider using the mouse.
	if (scrollbar->int_flags & scrolling)
	{
		return;
	}

	if (scrollbar->orientation == vertical)
	{
		int new_y;

		// Find how far down on the parent control we're scrolled (percentage).
		scroll_ratio = fabs(target->virtual_y / (float)target->virtual_height);
		
		// Calculate the position of the slider by multiplying the scroll areas
		// height with the scroll ratio.
		new_y = back_ctrl->height + Q_rint(scroll_ratio * scrollbar->scroll_area);
		clamp(new_y, back_ctrl->height, (self->height - forward_ctrl->height - slider_ctrl->height));

		EZ_control_SetPosition(slider_ctrl, 0, new_y);
	}
	else
	{
		int new_x;
		scroll_ratio = fabs(target->virtual_x / (float)target->virtual_width);
		
		new_x = back_ctrl->width + Q_rint(scroll_ratio * scrollbar->scroll_area);
		clamp(new_x, back_ctrl->width, (self->width - forward_ctrl->width - slider_ctrl->width));
		
		EZ_control_SetPosition(slider_ctrl, new_x, 0);
	}
}

//
// Scrollbar - Sets if the target for the scrollbar should be it's parent, or a specified target control.
//				(The target controls OnScroll event handler will be used if it's not the parent)
//
void EZ_scrollbar_SetTargetIsParent(ez_scrollbar_t *scrollbar, qbool targetparent)
{
	SET_FLAG(scrollbar->ext_flags, target_parent, targetparent);
}

//
// Scrollbar - Set the minimum slider size.
//
void EZ_scrollbar_SetSliderMinSize(ez_scrollbar_t *scrollbar, int minsize)
{
	scrollbar->slider_minsize = max(1, minsize);
}

//
// Scrollbar - Sets the amount to scroll the parent control when pressing the scrollbars scroll buttons.
//
void EZ_scrollbar_SetScrollDelta(ez_scrollbar_t *scrollbar, int scroll_delta_x, int scroll_delta_y)
{
	scrollbar->scroll_delta_x = max(1, scroll_delta_x);
	scrollbar->scroll_delta_y = max(1, scroll_delta_y);
}

//
// Scrollbar - Set if the scrollbar should be vertical or horizontal.
//
void EZ_scrollbar_SetIsVertical(ez_scrollbar_t *scrollbar, qbool is_vertical)
{
	scrollbar->orientation = (is_vertical ? vertical : horizontal);
}

//
// Scrollbar - Calculates the size of the slider button.
//
static void EZ_scrollbar_CalculateSliderSize(ez_scrollbar_t *scrollbar, ez_control_t *target)
{
	ez_control_t *self = (ez_control_t *)scrollbar;

	if (target)
	{
		if (scrollbar->orientation == vertical)
		{
			// Get the percentage of the parent that is shown and calculate the new slider button size from that. 
			float target_height_ratio	= (target->height / (float)target->virtual_height);
			int new_slider_height		= max(scrollbar->slider_minsize, Q_rint(target_height_ratio * scrollbar->scroll_area));

			EZ_control_SetSize((ez_control_t *)scrollbar->slider, self->width, new_slider_height);
		}
		else
		{
			float target_width_ratio	= (target->width / (float)target->virtual_width);
			int new_slider_width		= max(scrollbar->slider_minsize, Q_rint(target_width_ratio * scrollbar->scroll_area));

			EZ_control_SetSize((ez_control_t *)scrollbar->slider, new_slider_width, self->height);
		}
	}
}

//
// Scrollbar - Repositions the back / forward buttons according to orientation.
//
static void EZ_scrollbar_RepositionScrollButtons(ez_scrollbar_t *scrollbar)
{
	ez_control_t *self			= (ez_control_t *)scrollbar;
	ez_control_t *back_ctrl		= (ez_control_t *)scrollbar->back;
	ez_control_t *slider_ctrl	= (ez_control_t *)scrollbar->slider;
	ez_control_t *forward_ctrl	= (ez_control_t *)scrollbar->forward;

	// Let the super class do it's thing first.
	EZ_control_OnResize(self, NULL);

	if (scrollbar->orientation == vertical)
	{
		// Up button.
		EZ_control_SetSize(back_ctrl, self->width, self->width);
		EZ_control_SetAnchor(back_ctrl, anchor_left | anchor_top | anchor_right);

		// Slider.
		EZ_control_SetPosition(slider_ctrl, 0, self->width);
		EZ_control_SetAnchor(slider_ctrl, anchor_left | anchor_right);

		// Down button.
		EZ_control_SetSize(forward_ctrl, self->width, self->width);
		EZ_control_SetAnchor(forward_ctrl, anchor_left | anchor_bottom | anchor_right);

		scrollbar->scroll_area = self->height - (forward_ctrl->height + back_ctrl->height);
	}
	else
	{
		// Left button.
		EZ_control_SetSize(back_ctrl, self->height, self->height);
		EZ_control_SetAnchor(back_ctrl, anchor_left | anchor_top | anchor_bottom);

		// Slider.
		EZ_control_SetPosition(slider_ctrl, self->height, 0);
		EZ_control_SetAnchor(slider_ctrl, anchor_top | anchor_bottom);

		// Right button.
		EZ_control_SetSize(forward_ctrl, self->height, self->height);
		EZ_control_SetAnchor(forward_ctrl, anchor_top | anchor_bottom | anchor_right);

		scrollbar->scroll_area = self->width - (forward_ctrl->width + back_ctrl->width);
	}
}

//
// Control - Event handler for when the target control gets scrolled.
//
static int EZ_scrollbar_OnTargetScroll(ez_control_t *self, void *payload, void *ext_event_info)
{
	ez_scrollbar_t *scrollbar = NULL;

	if (!payload)
	{
		Sys_Error("EZ_scrollbar_OnTargetResize(): Payload is NULL, so no scrollbar object.");
	}

	scrollbar = (ez_scrollbar_t *)payload;

	if (!(scrollbar->ext_flags & target_parent))
	{
		EZ_scrollbar_UpdateSliderBasedOnTarget(scrollbar, scrollbar->target);
	}

	return 0;
}

//
// Scrollbar - Generic function of what to do when one of the scrollbars target was resized.
//
static int EZ_scrollbar_OnTargetResize(ez_control_t *self, void *payload, void *ext_event_info)
{
	ez_scrollbar_t *scrollbar = NULL;

	if (!payload)
	{
		Sys_Error("EZ_scrollbar_OnTargetResize(): Payload is NULL, so no scrollbar object.");
	}

	scrollbar = (ez_scrollbar_t *)payload;

	if (!(scrollbar->ext_flags & target_parent))
	{
		EZ_scrollbar_CalculateSliderSize(scrollbar, scrollbar->target);
	}

	return 0;
}

//
// Scrollbar - Set the target control that the scrollbar should scroll.
//
void EZ_scrollbar_SetTarget(ez_scrollbar_t *scrollbar, ez_control_t *target)
{
	// Remove the event handlers from the old target.
	if (scrollbar->target)
	{
		EZ_eventhandler_Remove(scrollbar->target->event_handlers.OnResize, EZ_scrollbar_OnTargetResize, false);
		EZ_eventhandler_Remove(scrollbar->target->event_handlers.OnScroll, EZ_scrollbar_OnTargetScroll, false);
	}

	scrollbar->target = target;
	CONTROL_RAISE_EVENT(NULL, scrollbar, ez_scrollbar_t, OnTargetChanged, NULL);
}

//
// Scrollbar - The target of the scrollbar changed.
//
int EZ_scrollbar_OnTargetChanged(ez_control_t *self, void *ext_event_info)
{
	ez_scrollbar_t *scrollbar = (ez_scrollbar_t *)self;

	if (scrollbar->target)
	{
		EZ_control_AddOnScroll(scrollbar->target, EZ_scrollbar_OnTargetScroll, scrollbar);
		EZ_control_AddOnResize(scrollbar->target, EZ_scrollbar_OnTargetResize, scrollbar);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, scrollbar, ez_scrollbar_t, OnTargetChanged, NULL);
	return 0;
}

//
// Scrollbar - OnResize event.
//
int EZ_scrollbar_OnResize(ez_control_t *self, void *ext_event_info)
{
	ez_scrollbar_t *scrollbar = (ez_scrollbar_t *)self;

	// Let the super class do it's thing first.
	EZ_control_OnResize(self, NULL);

	// Make sure the buttons are in the correct place.
	EZ_scrollbar_RepositionScrollButtons(scrollbar);
	EZ_scrollbar_CalculateSliderSize(scrollbar, ((scrollbar->ext_flags & target_parent) ? self->parent : scrollbar->target));

	// Reset the min virtual size to 1x1 so that the 
	// scrollbar won't stop resizing when it's parent is resized.
	EZ_control_SetMinVirtualSize(self, 1, 1);

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnResize, NULL);
	return 0;
}

//
// Scrollbar - OnParentResize event.
//
int EZ_scrollbar_OnParentResize(ez_control_t *self, void *ext_event_info)
{
	ez_scrollbar_t *scrollbar = (ez_scrollbar_t *)self;

	// Let the super class do it's thing first.
	EZ_control_OnParentResize(self, NULL);

	if (scrollbar->ext_flags & target_parent)
	{
		EZ_scrollbar_CalculateSliderSize(scrollbar, self->parent);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnParentResize, NULL);
	return 0;
}

//
// Scrollbar - OnMouseDown event.
//
int EZ_scrollbar_OnMouseDown(ez_control_t *self, mouse_state_t *ms)
{
	qbool  mouse_handled = false;
//	ez_scrollbar_t *scrollbar	= (ez_scrollbar_t *)self;

	EZ_control_OnMouseDown(self, ms);

	CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, ez_control_t, OnMouseDown, ms);

	return mouse_handled;
}

//
// Scrollbar - OnMouseUpOutside event.
//
int EZ_scrollbar_OnMouseUpOutside(ez_control_t *self, mouse_state_t *ms)
{
	ez_scrollbar_t *scrollbar	= (ez_scrollbar_t *)self;
	qbool mouse_handled			= false;

	EZ_control_OnMouseUpOutside(self, ms);

	scrollbar->int_flags &= ~sliding;

	CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, ez_control_t, OnMouseUpOutside, ms);

	return false;
}

//
// Scrollbar - Mouse event.
//
int EZ_scrollbar_OnMouseEvent(ez_control_t *self, mouse_state_t *ms)
{
	ez_scrollbar_t *scrollbar		= (ez_scrollbar_t *)self;
	ez_control_t *back_ctrl			= (ez_control_t *)scrollbar->back;
	ez_control_t *forward_ctrl		= (ez_control_t *)scrollbar->forward;
	ez_control_t *slider_ctrl		= (ez_control_t *)scrollbar->slider;
	int m_delta_x					= Q_rint(ms->x - ms->x_old);
	int m_delta_y					= Q_rint(ms->y - ms->y_old);
	qbool mouse_handled				= false;
	qbool mouse_handled_tmp			= false;

	mouse_handled = EZ_control_OnMouseEvent(self, ms);

	if (!mouse_handled)
	{
		if (scrollbar->int_flags & sliding)
		{
			if (scrollbar->orientation == vertical)
			{
//				float scroll_ratio = 0;

				// Reposition the slider within the scrollbar control based on where the mouse moves.
				int new_y = slider_ctrl->y + m_delta_y;

				// Only allow moving the scroll slider in the area between the two buttons (the scroll area).
				clamp(new_y, back_ctrl->height, (self->height - forward_ctrl->height - slider_ctrl->height));
				EZ_control_SetPosition(slider_ctrl, 0, new_y);

				mouse_handled = true;
			}
			else
			{
				int new_x = slider_ctrl->x + m_delta_x;
				clamp(new_x, back_ctrl->width, (self->width - forward_ctrl->width - slider_ctrl->width));
				EZ_control_SetPosition(slider_ctrl, new_x, 0);
				mouse_handled = true;
			}

			// Make sure we don't try to set the position of the slider
			// as the parents scroll position changes, like normal.
			scrollbar->int_flags |= scrolling;

			if (scrollbar->ext_flags & target_parent)
			{
				EZ_scrollbar_CalculateParentScrollPosition(scrollbar, self->parent);
			}
			else
			{
				EZ_scrollbar_CalculateParentScrollPosition(scrollbar, scrollbar->target);
			}
			
			scrollbar->int_flags &= ~scrolling;
		}
	}

	CONTROL_EVENT_HANDLER_CALL(&mouse_handled_tmp, self, ez_control_t, OnMouseEvent, ms);
	mouse_handled = (mouse_handled | mouse_handled_tmp);

	return mouse_handled;
}

//
// Scrollbar - OnParentScroll event.
//
int EZ_scrollbar_OnParentScroll(ez_control_t *self, void *ext_event_info)
{
	ez_scrollbar_t *scrollbar = (ez_scrollbar_t *)self;

	// Super class.
	EZ_control_OnParentScroll(self, NULL);

	// Update the slider button to match the new scroll position.
	if (scrollbar->ext_flags & target_parent)
	{
		EZ_scrollbar_UpdateSliderBasedOnTarget(scrollbar, self->parent);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnParentScroll, NULL);
	return 0;
}



