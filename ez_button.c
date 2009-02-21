
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
#include "ez_button.h"

#ifdef _WIN32
#pragma warning( disable : 4189 )
#endif

// =========================================================================================
// Button
// =========================================================================================

//
// Button - Recalculates and sets the position of the buttons label.
//
static void EZ_button_RecalculateLabelPosition(ez_button_t *button)
{
	ez_control_t *self				= ((ez_control_t *)button);
	ez_label_t *label				= button->text_label;
	ez_control_t *text_label_ctrl	= ((ez_control_t *)label);
//	ez_textalign_t alignment		= button->text_alignment;
	int new_x						= text_label_ctrl->x;
	int new_y						= text_label_ctrl->y;
	int new_anchor					= EZ_control_GetAnchor(text_label_ctrl);

	// TODO : Hmm, should we even bothered with a special case for text alignment? Why not just use ez_anchor_t stuff? Also remember to add support for middle_top and such.
	switch (button->text_alignment)
	{
		case top_left :
		{
			new_anchor = (anchor_left | anchor_top);
			new_x = 0;
			new_y = 0;
			break;
		}
		case top_right :
		{
			new_anchor = (anchor_right | anchor_top);
			new_x = 0;
			new_y = 0;
			break;
		}
		case top_center :
		{
			new_anchor = anchor_top;
			new_x = Q_rint((self->width  - text_label_ctrl->width) / 2.0);
			new_y = 0;
			break;
		}
		case bottom_left :
		{
			new_anchor = (anchor_left | anchor_bottom);
			new_x = 0;
			new_y = 0;
			break;
		}
		case bottom_right :
		{
			new_anchor = (anchor_right | anchor_bottom);
			new_x = 0;
			new_y = 0;
			break;
		}
		case bottom_center :
		{
			new_anchor = anchor_bottom;
			new_x = Q_rint((self->width  - text_label_ctrl->width) / 2.0);
			new_y = 0;
			break;
		}
		case middle_right :
		{
			new_anchor = anchor_right;
			new_x = 0;
			new_y = Q_rint((self->height - text_label_ctrl->height) / 2.0);
			break;
		}
		case middle_left :
		{
			new_anchor = anchor_left;
			new_x = 0;
			new_y = Q_rint((self->height - text_label_ctrl->height) / 2.0);
			break;
		}
		case middle_center :
		{
			new_anchor = (anchor_left | anchor_top);
			new_x = Q_rint((self->width  - text_label_ctrl->width) / 2.0);
			new_y = Q_rint((self->height - text_label_ctrl->height) / 2.0);
			break;
		}
		default :
			break;
	}

	if (new_anchor != EZ_control_GetAnchor(text_label_ctrl))
	{
		EZ_control_SetAnchor(text_label_ctrl, new_anchor);
	}

	if ((new_x != text_label_ctrl->x) || (new_y != text_label_ctrl->y))
	{
		EZ_control_SetPosition(text_label_ctrl, new_x, new_y);
	}
}

//
// Button - Event handler for the buttons OnTextChanged event.
// 
static int EZ_button_OnLabelTextChanged(ez_control_t *self, void *payload, void *ext_event_info)
{
	ez_button_t *button = NULL;
	
	if ((self->parent == NULL) || (self->parent->CLASS_ID != EZ_BUTTON_ID))
	{
		Sys_Error("EZ_button_OnLabelTextChanged: The buttons text labels parent isn't a button!");
	}
	
	button = (ez_button_t *)self->parent;

	EZ_button_RecalculateLabelPosition(button);

	return 0;
}

//
// Button - Creates a new button and initializes it.
//
ez_button_t *EZ_button_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags)
{
	ez_button_t *button = NULL;

	// We have to have a tree to add the control to.
	if (!tree)
	{
		return NULL;
	}

	button = (ez_button_t *)Q_malloc(sizeof(ez_button_t));
	memset(button, 0, sizeof(ez_button_t));

	EZ_button_Init(button, tree, parent, name, description, x, y, width, height, flags);
	
	return button;
}

//
// Button - Sets either the background image or color.
//
static void EZ_button_SetColorOrBackground(ez_control_t *self, mpic_t *background, byte *c)
{
	ez_button_t *button = (ez_button_t *)self;

	if (button->ext_flags & button_use_images)
	{
		self->background = background;
	}
	else
	{
		EZ_control_SetBackgroundColor(self, c[0], c[1], c[2], c[3]);
	}
}

//
// Button - Initializes a button.
//
void EZ_button_Init(ez_button_t *button, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags)
{
	// Initialize the inherited class first.
	EZ_control_Init(&button->super, tree, parent, name, description, x, y, width, height, flags);

	// Initilize the button specific stuff.
	((ez_control_t *)button)->CLASS_ID	= EZ_BUTTON_ID;

	// TODO : Make a default macro for button flags.
	((ez_control_t *)button)->ext_flags	|= (flags | control_focusable | control_contained | control_bg_tile_center | control_bg_tile_edges);

	// Override the draw function.
	CONTROL_REGISTER_EVENT(button, EZ_button_OnDraw, OnDraw, ez_control_t);
	CONTROL_REGISTER_EVENT(button, EZ_button_OnResize, OnResize, ez_control_t);
	CONTROL_REGISTER_EVENT(button, EZ_button_OnMouseClick, OnMouseClick, ez_control_t);
	CONTROL_REGISTER_EVENT(button, EZ_button_OnMouseLeave, OnMouseLeave, ez_control_t);
	CONTROL_REGISTER_EVENT(button, EZ_button_OnMouseEnter, OnMouseEnter, ez_control_t);	
	CONTROL_REGISTER_EVENT(button, EZ_button_OnMouseDown, OnMouseDown, ez_control_t);
	CONTROL_REGISTER_EVENT(button, EZ_button_OnMouseUp, OnMouseUp, ez_control_t);	

	// Button specific events.
	CONTROL_REGISTER_EVENT(button, EZ_button_OnAction, OnAction, ez_button_t);
	CONTROL_REGISTER_EVENT(button, EZ_button_OnTextAlignmentChanged, OnTextAlignmentChanged, ez_button_t);
	CONTROL_REGISTER_EVENT(button, EZ_button_OnToggled, OnToggled, ez_button_t);

	// Create the buttons text label.
	{
		button->text_label = EZ_label_Create(tree, (ez_control_t *)button, "Button text label", "", button->padding_left, button->padding_top, 1, 1, 0, 0, "");

		EZ_label_AddOnTextChanged(button->text_label, EZ_button_OnLabelTextChanged, NULL);

		// Don't allow any interaction with the label, it's just there to show text.
		EZ_label_SetReadOnly(button->text_label, true);
		EZ_label_SetTextSelectable(button->text_label, false);
		EZ_label_SetAutoSize(button->text_label, true);
		EZ_control_SetIgnoreMouse((ez_control_t *)button->text_label, true);
		EZ_control_SetFocusable((ez_control_t *)button->text_label, false);
		EZ_control_SetMovable((ez_control_t *)button->text_label, false);

		CONTROL_RAISE_EVENT(NULL, ((ez_control_t *)button), ez_control_t, OnResize, NULL);
	}

	EZ_button_SetNormalImage(button, EZ_BUTTON_DEFAULT_NORMAL_IMAGE);
	EZ_button_SetHoverImage(button, EZ_BUTTON_DEFAULT_HOVER_IMAGE);
	EZ_button_SetPressedImage(button, EZ_BUTTON_DEFAULT_PRESSED_IMAGE);
	EZ_button_SetToggledHoverImage(button, EZ_BUTTON_DEFAULT_PRESSED_HOVER_IMAGE);

	button->ext_flags |= button_use_images;

	EZ_button_SetColorOrBackground((ez_control_t *)button, button->normal_image, button->color_normal);
}

//
// Button - Destroys the button.
//
void EZ_button_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_button_t *button = (ez_button_t *)self;
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDestroy, destroy_children);

	EZ_control_Destroy(&button->super, destroy_children);

	EZ_eventhandler_Remove(button->event_handlers.OnAction, NULL, true);
	EZ_eventhandler_Remove(button->event_handlers.OnTextAlignmentChanged, NULL, true);
}

//
// Button - OnAction event handler.
//
int EZ_button_OnAction(ez_control_t *self, void *ext_event_info)
{
	ez_button_t *button = (ez_button_t *)self;
	CONTROL_EVENT_HANDLER_CALL(NULL, button, ez_button_t, OnAction, NULL);

	return 0;
}

//
// Button - OnResize event handler.
//
int EZ_button_OnResize(ez_control_t *self, void *ext_event_info)
{
	ez_button_t *button = (ez_button_t *)self;

	// Run the super class implementation.
	EZ_control_OnResize(self, NULL);

	EZ_button_RecalculateLabelPosition(button);

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnResize, NULL);

	return 0;
}

//
// Button - Use images for the button?
//
void EZ_button_SetUseImages(ez_button_t *button, qbool useimages)
{
	SET_FLAG(button->ext_flags, button_use_images, useimages);
}

//
// Button - Sets if the button is toggleable.
//
void EZ_button_SetToggleable(ez_button_t *button, qbool toggleable)
{
	SET_FLAG(button->ext_flags, button_is_toggleable, toggleable);
}

//
// Button - Gets if the button is toggled.
//
qbool EZ_button_GetIsToggled(ez_button_t *button)
{
	return (button->int_flags & button_toggled);
}

//
// Button - Set the text of the button. 
//
void EZ_button_SetText(ez_button_t *button, const char *text)
{
	EZ_label_SetText(button->text_label, text);
}

//
// Button - Set the text of the button. 
//
void EZ_button_SetTextAlignment(ez_button_t *button, ez_textalign_t text_alignment)
{
	button->text_alignment = text_alignment;

	CONTROL_RAISE_EVENT(NULL, button, ez_button_t, OnTextAlignmentChanged, NULL);
}

//
// Button - Set the event handler for the OnTextAlignmentChanged event.
//
void EZ_button_AddOnTextAlignmentChanged(ez_button_t *button, ez_eventhandler_fp OnTextAlignmentChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(button, EZ_CONTROL_HANDLER, OnTextAlignmentChanged, ez_button_t, OnTextAlignmentChanged, payload);
	CONTROL_RAISE_EVENT(NULL, button, ez_control_t, OnEventHandlerChanged, NULL);
}

// 
// Button - Sets the OnToggled event handler.
//
void EZ_button_AddOnToggled(ez_button_t *button, ez_eventhandler_fp OnToggled, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(button, EZ_CONTROL_HANDLER, OnToggled, ez_button_t, OnToggled, payload);
	CONTROL_RAISE_EVENT(NULL, button, ez_control_t, OnEventHandlerChanged, NULL);
}

//
// Button - Set the normal image for the button.
//
void EZ_button_SetNormalImage(ez_button_t *button, const char *normal_image)
{
	button->normal_image = normal_image ? Draw_CachePicSafe(normal_image, false, true) : NULL;
}

//
// Button - Set the hover image for the button.
//
void EZ_button_SetHoverImage(ez_button_t *button, const char *hover_image)
{
	button->hover_image = hover_image ? Draw_CachePicSafe(hover_image, false, true) : NULL;
}

//
// Button - Set the hover image for the button.
//
void EZ_button_SetPressedImage(ez_button_t *button, const char *pressed_image)
{
	button->pressed_image = pressed_image ? Draw_CachePicSafe(pressed_image, false, true) : NULL;
}

//
// Button - Set the hover image for the button when it is toggled.
//
void EZ_button_SetToggledHoverImage(ez_button_t *button, const char *toggled_hover_image)
{
	button->toggled_hover_image = toggled_hover_image ? Draw_CachePicSafe(toggled_hover_image, false, true) : NULL;
}

//
// Button - Sets the normal color of the button.
//
void EZ_button_SetNormalColor(ez_button_t *self, byte r, byte g, byte b, byte alpha)
{
	self->color_normal[0] = r;
	self->color_normal[1] = g;
	self->color_normal[2] = b;
	self->color_normal[3] = alpha;
}

//
// Button - Sets the pressed color of the button.
//
void EZ_button_SetPressedColor(ez_button_t *self, byte r, byte g, byte b, byte alpha)
{
	self->color_pressed[0] = r;
	self->color_pressed[1] = g;
	self->color_pressed[2] = b;
	self->color_pressed[3] = alpha;
}

//
// Button - Sets the hover color of the button.
//
void EZ_button_SetHoverColor(ez_button_t *self, byte r, byte g, byte b, byte alpha)
{
	self->color_hover[0] = r;
	self->color_hover[1] = g;
	self->color_hover[2] = b;
	self->color_hover[3] = alpha;
}

//
// Button - Sets the toggled hover color of the button.
//
void EZ_button_SetToggledHoverColor(ez_button_t *self, byte r, byte g, byte b, byte alpha)
{
	self->color_toggled_hover[0] = r;
	self->color_toggled_hover[1] = g;
	self->color_toggled_hover[2] = b;
	self->color_toggled_hover[3] = alpha;
}

//
// Button - Sets the focused color of the button.
//
void EZ_button_SetFocusedColor(ez_button_t *self, byte r, byte g, byte b, byte alpha)
{
	self->color_focused[0] = r;
	self->color_focused[1] = g;
	self->color_focused[2] = b;
	self->color_focused[3] = alpha;
}

//
// Button - Sets the OnAction event handler.
//
void EZ_button_AddOnAction(ez_button_t *self, ez_eventhandler_fp OnAction, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnAction, ez_button_t, OnAction, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged, NULL);
}

//
// Button - OnDraw event.
//
int EZ_button_OnDraw(ez_control_t *self, void *ext_event_info)
{
	ez_button_t *button = (ez_button_t *)self;

	int x, y;
	EZ_control_GetDrawingPosition(self, &x, &y);

	// Run the super class's implementation first.
	EZ_control_OnDraw(self, NULL);

	if (self->int_flags & control_focused)
	{
		if (button->ext_flags & button_use_images)
		{
		}
		else
		{
			Draw_AlphaRectangleRGB(x, y, self->width, self->height, 1, false, RGBAVECT_TO_COLOR(button->color_focused));
		}
	}

	// Draw control specifics.
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDraw, NULL);
	return 0;
}

//
// Button - OnTextAlignmentChanged event.
//
int EZ_button_OnTextAlignmentChanged(ez_control_t *self, void *ext_event_info)
{
	ez_button_t *button = (ez_button_t *)self;

	EZ_button_RecalculateLabelPosition(button);

	CONTROL_EVENT_HANDLER_CALL(NULL, button, ez_button_t, OnTextAlignmentChanged, NULL);
	return 0;
}

//
// Button - OnMouseClick event.
//
int EZ_button_OnMouseClick(ez_control_t *self, mouse_state_t *mouse_state)
{
	int mouse_handled = true;
	ez_button_t *button = (ez_button_t *)self;
	EZ_control_OnMouseClick(self, mouse_state);

	// Toggle the button.
	if (button->ext_flags & button_is_toggleable)
	{
		button->int_flags ^= button_toggled;
		CONTROL_RAISE_EVENT(NULL, button, ez_button_t, OnToggled, NULL);
	}
	else
	{
		button->int_flags &= ~button_toggled;
	}

	CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, ez_control_t, OnMouseClick, mouse_state);
	return mouse_handled;
}

//
// Button - OnToggled event. The button was toggled.
//
int EZ_button_OnToggled(ez_control_t *self, void *ext_event_info)
{
	ez_button_t *button = (ez_button_t *)self;

	// Set the background based on the new state.
	if (button->int_flags & button_toggled)
	{
		EZ_button_SetColorOrBackground(self, button->pressed_image, button->color_pressed);
	}
	else
	{
		EZ_button_SetColorOrBackground(self, button->normal_image, button->color_normal);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, button, ez_button_t, OnToggled, NULL);
	return 0;
}

//
// Button - OnMouseEnter event.
//
int EZ_button_OnMouseEnter(ez_control_t *self, mouse_state_t *mouse_state)
{
	ez_button_t *button = (ez_button_t *)self;
	EZ_control_OnMouseEnter(self, mouse_state);

	// Set the background based on the new state.
	if (button->int_flags & button_toggled)
	{
		EZ_button_SetColorOrBackground(self, button->toggled_hover_image, button->color_toggled_hover);
	}
	else
	{
		EZ_button_SetColorOrBackground(self, button->hover_image, button->color_hover);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnMouseEnter, NULL);
	return 0;
}

//
// Button - OnMouseLeave event.
//
int EZ_button_OnMouseLeave(ez_control_t *self, mouse_state_t *mouse_state)
{
	ez_button_t *button = (ez_button_t *)self;
	EZ_control_OnMouseLeave(self, mouse_state);

	// Set the background based on the new state.
	if (button->int_flags & button_toggled)
	{
		EZ_button_SetColorOrBackground(self, button->pressed_image, button->color_pressed);
	}
	else
	{
		EZ_button_SetColorOrBackground(self, button->normal_image, button->color_normal);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, button, ez_control_t, OnMouseLeave, NULL);
	return 0;
}

//
// Button - OnMouseDown event.
//
int EZ_button_OnMouseDown(ez_control_t *self, mouse_state_t *mouse_state)
{
	ez_button_t *button = (ez_button_t *)self;
	EZ_control_OnMouseDown(self, mouse_state);

	EZ_button_SetColorOrBackground(self, button->toggled_hover_image, button->color_toggled_hover);

	CONTROL_EVENT_HANDLER_CALL(NULL, button, ez_control_t, OnMouseDown, NULL);
	return 1;
}

//
// Button - OnMouseDown event.
//
int EZ_button_OnMouseUp(ez_control_t *self, mouse_state_t *mouse_state)
{
	ez_button_t *button = (ez_button_t *)self;
	EZ_control_OnMouseUp(self, mouse_state);

	if (!(button->ext_flags & button_is_toggleable))
	{
		EZ_button_SetColorOrBackground(self, button->hover_image, button->color_hover);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, button, ez_control_t, OnMouseUp, NULL);
	return 1;
}


