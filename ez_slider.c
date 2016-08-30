
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
#include "ez_slider.h"

// =========================================================================================
// Slider
// =========================================================================================

// TODO : Slider - Show somehow that it's focused?

//
// Slider - Creates a new button and initializes it.
//
ez_slider_t *EZ_slider_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags)
{
	ez_slider_t *slider = NULL;

	// We have to have a tree to add the control to.
	if (!tree)
	{
		return NULL;
	}

	slider = (ez_slider_t *)Q_malloc(sizeof(ez_slider_t));
	memset(slider, 0, sizeof(ez_slider_t));

	EZ_slider_Init(slider, tree, parent, name, description, x, y, width, height, flags);
	
	return slider;
}

//
// Slider - Initializes a button.
//
void EZ_slider_Init(ez_slider_t *slider, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags)
{
	height = max(height, 8);

	// Initialize the inherited class first.
	EZ_control_Init(&slider->super, tree, parent, name, description, x, y, width, height, flags);

	// Initilize the button specific stuff.
	((ez_control_t *)slider)->CLASS_ID					= EZ_SLIDER_ID;
	((ez_control_t *)slider)->ext_flags					|= (flags | control_focusable | control_contained | control_resizeable);

	// Overrided control events.
	CONTROL_REGISTER_EVENT(slider, EZ_slider_OnDraw, OnDraw, ez_control_t);
	CONTROL_REGISTER_EVENT(slider, EZ_slider_OnMouseDown, OnMouseDown, ez_control_t);
	CONTROL_REGISTER_EVENT(slider, EZ_slider_OnMouseUpOutside, OnMouseUpOutside, ez_control_t);
	CONTROL_REGISTER_EVENT(slider, EZ_slider_OnMouseEvent, OnMouseEvent, ez_control_t);
	CONTROL_REGISTER_EVENT(slider, EZ_slider_OnResize, OnResize, ez_control_t);
	CONTROL_REGISTER_EVENT(slider, EZ_slider_OnKeyDown, OnKeyDown, ez_control_t);

	// Slider specific events.
	CONTROL_REGISTER_EVENT(slider, EZ_slider_OnSliderPositionChanged, OnSliderPositionChanged, ez_slider_t);
	CONTROL_REGISTER_EVENT(slider, EZ_slider_OnMaxValueChanged, OnMaxValueChanged, ez_slider_t);
	CONTROL_REGISTER_EVENT(slider, EZ_slider_OnMinValueChanged, OnMinValueChanged, ez_slider_t);
	CONTROL_REGISTER_EVENT(slider, EZ_slider_OnScaleChanged, OnScaleChanged, ez_slider_t);

	// Set default values.
	EZ_slider_SetMax(slider, 100);
	EZ_slider_SetScale(slider, 1.0);
	EZ_slider_SetPosition(slider, 0);
}

//
// Slider - Calculates the actual slider position.
//
static inline void EZ_slider_CalculateRealSliderPos(ez_slider_t *slider)
{
	int pos = slider->slider_pos - slider->min_value;

	// Calculate the real position of the slider by multiplying by the gap size between each value.
	// (Don't start drawing at the exact start cause that would overwrite the edge marker)
	slider->real_slider_pos = Q_rint((slider->scaled_char_size / 2.0) + (pos * slider->gap_size));
}

//
// Slider - Calculates the gap size between values.
//
static inline void EZ_slider_CalculateGapSize(ez_slider_t *slider)
{
	slider->range = abs(slider->max_value - slider->min_value);

	// Calculate the gap between each value in pixels (floating point so that we don't get rounding errors).
	// Don't include the first and last characters in the calculation, the slider is not allowed to move over those.
	slider->gap_size = ((float)(((ez_control_t *)slider)->width - (2 * slider->scaled_char_size)) / (float)slider->range);
}

//
// Slider - Event handler for OnSliderPositionChanged.
//
void EZ_slider_AddOnSliderPositionChanged(ez_slider_t *slider, ez_eventhandler_fp OnSliderPositionChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(slider, EZ_CONTROL_HANDLER, OnSliderPositionChanged, ez_slider_t, OnSliderPositionChanged, payload);
	CONTROL_RAISE_EVENT(NULL, slider, ez_control_t, OnEventHandlerChanged, NULL);
}

//
// Slider - Event handler for OnMaxValueChanged.
//
void EZ_slider_AddOnMaxValueChanged(ez_slider_t *slider, ez_eventhandler_fp OnMaxValueChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(slider, EZ_CONTROL_HANDLER, OnMaxValueChanged, ez_slider_t, OnMaxValueChanged, payload);
	CONTROL_RAISE_EVENT(NULL, slider, ez_control_t, OnEventHandlerChanged, NULL);
}

//
// Slider - Event handler for OnMinValueChanged.
//
void EZ_slider_AddOnMinValueChanged(ez_slider_t *slider, ez_eventhandler_fp OnMinValueChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(slider, EZ_CONTROL_HANDLER, OnMinValueChanged, ez_slider_t, OnMinValueChanged, payload);
	CONTROL_RAISE_EVENT(NULL, slider, ez_control_t, OnEventHandlerChanged, NULL);
}

//
// Slider - Event handler for OnSliderPositionChanged.
//
void EZ_slider_AddOnScaleChanged(ez_slider_t *slider, ez_eventhandler_fp OnScaleChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(slider, EZ_CONTROL_HANDLER, OnScaleChanged, ez_slider_t, OnScaleChanged, payload);
	CONTROL_RAISE_EVENT(NULL, slider, ez_control_t, OnEventHandlerChanged, NULL);
}

//
// Slider - Gets the current position of the mouse in slider scale. (This does not care if the mouse is within the control)
//
int EZ_slider_GetPositionFromMouse(ez_slider_t *slider, float mouse_x, float mouse_y)
{
	ez_control_t *self = (ez_control_t *)slider;
	int x = Q_rint(self->absolute_virtual_x + (slider->scaled_char_size / 2.0));
	return Q_rint((mouse_x - x) / slider->gap_size);
}

//
// Slider - Get the current slider position.
//
int EZ_slider_GetPosition(ez_slider_t *slider)
{
	return slider->slider_pos;
}

//
// Slider - Set the slider position.
//
void EZ_slider_SetPosition(ez_slider_t *slider, int slider_pos)
{
	clamp(slider_pos, slider->min_value, slider->max_value);

	slider->slider_pos = slider_pos;

	CONTROL_RAISE_EVENT(NULL, slider, ez_slider_t, OnSliderPositionChanged, NULL);
}

//
// Slider - Set the max slider value.
//
void EZ_slider_SetMax(ez_slider_t *slider, int max_value)
{
	slider->max_value = max_value;

	CONTROL_RAISE_EVENT(NULL, slider, ez_slider_t, OnMaxValueChanged, NULL);
}

//
// Slider - Set the max slider value.
//
void EZ_slider_SetMin(ez_slider_t *slider, int min_value)
{
	slider->min_value = min_value;

	CONTROL_RAISE_EVENT(NULL, slider, ez_slider_t, OnMinValueChanged, NULL);
}

//
// Slider - Set the scale of the slider characters.
//
void EZ_slider_SetScale(ez_slider_t *slider, float scale)
{
	slider->scale = max(0.1, scale);

	CONTROL_RAISE_EVENT(NULL, slider, ez_slider_t, OnScaleChanged, NULL);
}

//
// Slider - Scale changed.
//
int EZ_slider_OnScaleChanged(ez_control_t *self, void *ext_event_info)
{
	ez_slider_t *slider = (ez_slider_t *)self;

	// Calculate the new character size.
	slider->scaled_char_size = slider->scale * 8;

	// Refit the control to fit the slider.
	EZ_control_SetSize(self, slider->super.width + slider->scaled_char_size, slider->scaled_char_size);

	CONTROL_EVENT_HANDLER_CALL(NULL, slider, ez_slider_t, OnScaleChanged, NULL);

	return 0;
}

//
// Slider - Draw function for the slider.
//
int EZ_slider_OnDraw(ez_control_t *self, void *ext_event_info)
{
	int x, y, i;
	ez_slider_t *slider = (ez_slider_t *)self;

	EZ_control_GetDrawingPosition(self, &x, &y);

	// Draw the background.
	{
		// Left edge.
		Draw_SCharacter(x, y, 128, slider->scale);

		for (i = 1; i < Q_rint((float)(self->width - slider->scaled_char_size) / slider->scaled_char_size); i++)
		{
			Draw_SCharacter(x + (i * slider->scaled_char_size), y, 129, slider->scale);
		}

		// Right edge.
		Draw_SCharacter(x + (i * slider->scaled_char_size), y, 130, slider->scale);
	}

	// Slider.
	Draw_SCharacter(x + slider->real_slider_pos, y, 131, slider->scale);

	return 0;
}

//
// Slider - The max value changed.
//
int EZ_slider_OnMaxValueChanged(ez_control_t *self, void *ext_event_info)
{
	ez_slider_t *slider = (ez_slider_t *)self;

	// Only allow positive values greater than the min value.
	slider->max_value = max(0, max(slider->max_value, slider->min_value));

	// Calculate the gap between each slider value.
	EZ_slider_CalculateGapSize(slider);

	CONTROL_EVENT_HANDLER_CALL(NULL, slider, ez_slider_t, OnMaxValueChanged, NULL);

	return 0;
}

//
// Slider - The min value changed.
//
int EZ_slider_OnMinValueChanged(ez_control_t *self, void *ext_event_info)
{
	ez_slider_t *slider = (ez_slider_t *)self;

	// Only allow positive values less than the max value.
	slider->min_value = max(0, min(slider->max_value, slider->min_value));

	// Calculate the gap between each slider value.
	EZ_slider_CalculateGapSize(slider);

	CONTROL_EVENT_HANDLER_CALL(NULL, slider, ez_slider_t, OnMinValueChanged, NULL);

	return 0;
}

//
// Slider - The slider position changed.
//
int EZ_slider_OnSliderPositionChanged(ez_control_t *self, void *ext_event_info)
{
	ez_slider_t *slider = (ez_slider_t *)self;

	// Recalculate the drawing position.
	EZ_slider_CalculateRealSliderPos(slider);

	CONTROL_EVENT_HANDLER_CALL(NULL, slider, ez_slider_t, OnSliderPositionChanged, NULL);

	return 0;
}

//
// Slider - Jumps to the point that was clicked on the slider 
//          (Normal behavior is to make a "big step" when clicking the slider outside the handle).
//
void EZ_slider_SetJumpToClick(ez_slider_t *slider, qbool jump_to_click)
{
	SET_FLAG(slider->ext_flags, slider_jump_to_click, jump_to_click);
	CONTROL_RAISE_EVENT(NULL, (ez_control_t *)slider, ez_control_t, OnFlagsChanged, NULL);
}

//
// Slider - Mouse down event.
//
int EZ_slider_OnMouseDown(ez_control_t *self, mouse_state_t *ms)
{
	ez_slider_t *slider		= (ez_slider_t *)self;
	int big_step			= max(1, slider->range / 10);
	int slider_left_edge; 
	int slider_right_edge;
	int x, y;
	
	EZ_control_GetDrawingPosition(self, &x, &y);

	slider_left_edge	= (x + slider->real_slider_pos);
	slider_right_edge	= (x + slider->real_slider_pos + slider->scaled_char_size);

	// Super class.
	EZ_control_OnMouseDown(self, ms);

	if ((ms->x >= slider_left_edge) && (ms->x <= slider_right_edge))
	{
		slider->int_flags |= slider_dragging;
	}
	else if (slider->ext_flags & slider_jump_to_click)
	{
		// Jump to the exact position on the slider that was clicked.
		int newpos = EZ_slider_GetPositionFromMouse(slider, ms->x, ms->y);
		EZ_slider_SetPosition(slider, newpos);
	}
	else if (ms->x < slider_left_edge)
	{
		// When clicking on the slider (but not the slider handle)
		// make a "big step" instead of dragging it.
		EZ_slider_SetPosition(slider, slider->slider_pos - big_step);		
	}
	else if (ms->x > slider_right_edge)
	{
		EZ_slider_SetPosition(slider, slider->slider_pos + big_step);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnMouseDown, ms);

	return true;
}

//
// Slider - Mouse up event.
//
int EZ_slider_OnMouseUpOutside(ez_control_t *self, mouse_state_t *ms)
{
	ez_slider_t *slider = (ez_slider_t *)self;

	// Super class.
	EZ_control_OnMouseUpOutside(self, ms);

	// Not dragging anymore.
	slider->int_flags &= ~slider_dragging;

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnMouseUp, ms);

	return true;
}

//
// Slider - Handles a mouse event.
//
int EZ_slider_OnMouseEvent(ez_control_t *self, mouse_state_t *ms)
{
	// Make sure we handle all mouse events when we're over the control
	// otherwise they will fall through to controls below.
	int mouse_handled	= POINT_IN_CONTROL_RECT(self, ms->x, ms->y); 
	ez_slider_t *slider = (ez_slider_t *)self;

	// Call the super class first.
	EZ_control_OnMouseEvent(self, ms);
	
	if (slider->int_flags & slider_dragging)
	{
		int new_slider_pos = slider->min_value + Q_rint((ms->x - self->absolute_x) / slider->gap_size);
		EZ_slider_SetPosition(slider, new_slider_pos);
		mouse_handled = true;
	}
	
	// Event handler call.
	{
		int mouse_handled_tmp = false;
		CONTROL_EVENT_HANDLER_CALL(&mouse_handled_tmp, self, ez_control_t, OnMouseEvent, ms);
		mouse_handled = (mouse_handled || mouse_handled_tmp);
	}

	return mouse_handled;
}

//
// Slider - The slider was resized.
//
int EZ_slider_OnResize(ez_control_t *self, void *ext_event_info)
{
	ez_slider_t *slider = (ez_slider_t *)self;

	// Call super class.
	EZ_control_OnResize(self, NULL);

	EZ_slider_CalculateGapSize(slider);
	EZ_slider_CalculateRealSliderPos(slider);

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnResize, NULL);

	return 0;
}

//
// Slider - Key event.
//
int EZ_slider_OnKeyDown(ez_control_t *self, int key, int unichar)
{
	qbool key_handled	= false;
	ez_slider_t *slider = (ez_slider_t *)self;
	int big_step		= max(1, slider->max_value / 10);
	int step			= keydown[K_CTRL] ? big_step : 1;

	switch(key)
	{
		case K_RIGHTARROW :
		{
			EZ_slider_SetPosition(slider, slider->slider_pos + step);
			key_handled = true;
			break;
		}
		case K_LEFTARROW :
		{
			EZ_slider_SetPosition(slider, slider->slider_pos - step);
			key_handled = true;
			break;
		}
		default :
		{
			break;
		}
	}

	return key_handled;
}


