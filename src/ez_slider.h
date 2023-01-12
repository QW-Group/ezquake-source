
#ifndef __EZ_SLIDER_H__
#define __EZ_SLIDER_H__
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

$Id: ez_slider.h,v 1.55 2007-10-27 14:51:15 cokeman1982 Exp $
*/

#include "ez_controls.h"

// =========================================================================================
// Slider
// =========================================================================================

typedef enum ez_slider_iflags_e
{
	slider_dragging	= (1 << 0)
} ez_slider_iflags_t;

typedef enum ez_slider_flags_e
{
	slider_jump_to_click = (1 << 0)
} ez_slider_flags_t;

typedef struct ez_slider_eventcount_s
{
	int	OnSliderPositionChanged;
	int	OnMaxValueChanged;
	int	OnMinValueChanged;
	int	OnScaleChanged;
} ez_slider_eventcount_t;

typedef struct ez_slider_events_s
{
	ez_event_fp	OnSliderPositionChanged;
	ez_event_fp	OnMaxValueChanged;
	ez_event_fp	OnMinValueChanged;
	ez_event_fp	OnScaleChanged;
} ez_slider_events_t;

typedef struct ez_slider_eventhandlers_s
{
	ez_eventhandler_t		*OnSliderPositionChanged;
	ez_eventhandler_t		*OnMaxValueChanged;
	ez_eventhandler_t		*OnMinValueChanged;
	ez_eventhandler_t		*OnScaleChanged;
} ez_slider_eventhandlers_t;

typedef struct ez_slider_s
{
	ez_control_t			super;				// The super class.

	ez_slider_events_t		events;				// Slider specific events.
	ez_slider_eventhandlers_t event_handlers;	// Slider specific event handlers.
	ez_slider_eventcount_t	inherit_levels;
	ez_slider_eventcount_t	override_counts;

	ez_slider_flags_t		ext_flags;			// External slider flags.
	ez_slider_iflags_t		int_flags;			// Slider specific internal flags.
	int						slider_pos;			// The position of the slider.
	int						real_slider_pos;	// The actual slider position in pixels.
	int						max_value;			// The max value allowed for the slider.
	int						min_value;			// The min value allowed for the slider.
	int						range;				// The number of values between the min and max values.
	float					gap_size;			// The pixel gap between each value.
	float					scale;				// The scale of the characters used for drawing the slider.
	int						scaled_char_size;	// The scaled character size in pixels after applying scale to the slider chars.

	int						override_count;		// These are needed so that subclasses can override slider specific events.
	int						inheritance_level;
} ez_slider_t;

//
// Slider - Creates a new slider and initializes it.
//
ez_slider_t *EZ_slider_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Slider - Initializes a slider.
//
void EZ_slider_Init(ez_slider_t *slider, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Slider - Event handler for OnSliderPositionChanged.
//
void EZ_slider_AddOnSliderPositionChanged(ez_slider_t *slider, ez_eventhandler_fp OnSliderPositionChanged, void *payload);

//
// Slider - Event handler for OnSliderPositionChanged.
//
void EZ_slider_AddOnScaleChanged(ez_slider_t *slider, ez_eventhandler_fp OnScaleChanged, void *payload);

//
// Slider - Event handler for OnMaxValueChanged.
//
void EZ_slider_AddOnMaxValueChanged(ez_slider_t *slider, ez_eventhandler_fp OnMaxValueChanged, void *payload);

//
// Slider - Event handler for OnMinValueChanged.
//
void EZ_slider_AddOnMinValueChanged(ez_slider_t *slider, ez_eventhandler_fp OnMinValueChanged, void *payload);

//
// Slider - Gets the current position of the mouse in slider scale. (This does not care if the mouse is within the control)
//
int EZ_slider_GetPositionFromMouse(ez_slider_t *slider, float mouse_x, float mouse_y);

//
// Slider - Get the current slider position.
//
int EZ_slider_GetPosition(ez_slider_t *slider);

//
// Slider - Set the slider position.
//
void EZ_slider_SetPosition(ez_slider_t *slider, int slider_pos);

//
// Slider - Set the max slider value.
//
void EZ_slider_SetMax(ez_slider_t *slider, int max_value);

//
// Slider - Set the max slider value.
//
void EZ_slider_SetMin(ez_slider_t *slider, int min_value);

//
// Slider - Set the scale of the slider characters.
//
void EZ_slider_SetScale(ez_slider_t *slider, float scale);

//
// Slider - Draw function for the slider.
//
int EZ_slider_OnDraw(ez_control_t *self, void *ext_event_info);

//
// Slider - The max value changed.
//
int EZ_slider_OnMaxValueChanged(ez_control_t *self, void *ext_event_info);

//
// Slider - The min value changed.
//
int EZ_slider_OnMinValueChanged(ez_control_t *self, void *ext_event_info);

//
// Slider - Scale changed.
//
int EZ_slider_OnScaleChanged(ez_control_t *self, void *ext_event_info);

//
// Slider - The slider position changed.
//
int EZ_slider_OnSliderPositionChanged(ez_control_t *self, void *ext_event_info);

//
// Slider - Mouse down event.
//
int EZ_slider_OnMouseDown(ez_control_t *self, mouse_state_t *ms);

//
// Slider - Mouse up event.
//
int EZ_slider_OnMouseUpOutside(ez_control_t *self, mouse_state_t *ms);

//
// Slider - Handles a mouse event.
//
int EZ_slider_OnMouseEvent(ez_control_t *self, mouse_state_t *ms);

//
// Slider - The slider was resized.
//
int EZ_slider_OnResize(ez_control_t *self, void *ext_event_info);

//
// Slider - Key event.
//
int EZ_slider_OnKeyDown(ez_control_t *self, int key, int unichar);

//
// Slider - Jumps to the point that was clicked on the slider 
//          (Normal behavior is to make a "big step" when clicking the slider outside the handle).
//
void EZ_slider_SetJumpToClick(ez_slider_t *slider, qbool jump_to_click);

#endif // __EZ_SLIDER_H__
