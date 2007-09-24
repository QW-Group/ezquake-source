
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

$Id: ez_controls.c,v 1.51 2007-09-24 23:25:00 cokeman1982 Exp $
*/

#include "quakedef.h"
#include "keys.h"
#include "utils.h"
#include "ez_controls.h"

// =========================================================================================
// Double Linked List
// =========================================================================================

//
// Add item to double linked list.
//
void EZ_double_linked_list_Add(ez_double_linked_list_t *list, void *payload)
{
	ez_dllist_node_t *item = (ez_dllist_node_t *)Q_malloc(sizeof(ez_dllist_node_t));
	memset(item, 0, sizeof(ez_dllist_node_t));

	item->payload = payload;
	item->next = NULL;

	// Add the item to the start of the list if it's empty
	// otherwise add it to the tail.
	if(!list->head)
	{
		list->head = item;
		item->next = NULL;
		item->previous = NULL;
	}
	else
	{
		item->previous = list->tail;
	}

	// Make sure the previous item knows who we are.
	if(item->previous)
	{
		item->previous->next = item;
	}

	list->tail = item;
	list->count++;
}

//
// Finds a given node based on the specified payload.
//
ez_dllist_node_t *EZ_double_linked_list_FindByPayload(ez_double_linked_list_t *list, void *payload)
{
	ez_dllist_node_t *iter = list->head;

	while(iter)
	{
		if(iter->payload == payload)
		{
			return iter;
		}

		iter = iter->next;
	}

	return NULL;
}

//
// Removes an item from a linked list by it's payload.
//
void *EZ_double_linked_list_RemoveByPayload(ez_double_linked_list_t *list, void *payload)
{
	ez_dllist_node_t *node = EZ_double_linked_list_FindByPayload(list, payload);
	return EZ_double_linked_list_Remove(list, node);
}

//
// Removes the first occurance of the item from double linked list and returns it's payload.
//
void *EZ_double_linked_list_Remove(ez_double_linked_list_t *list, ez_dllist_node_t *item)
{
	void *payload = item->payload;

	item->previous->next = item->next;
	item->next->previous = item->previous;

	list->count--;

	Q_free(item);

	return payload;
}

typedef void * PVOID;

//
// Double Linked List - Orders a list.
//
void EZ_double_linked_list_Order(ez_double_linked_list_t *list, PtFuncCompare compare_function)
{
	int i = 0;
	ez_dllist_node_t *iter = NULL;
	PVOID **items = (PVOID **)Q_calloc(list->count, sizeof(PVOID *));

	iter = list->head;

	while(iter)
	{
		items[i] = iter->payload;
		i++;

		iter = iter->next;
	}

	qsort(items, list->count, sizeof(PVOID *), compare_function);

	iter = list->head;

	for(i = 0; i < list->count; i++)
	{
		iter->payload = items[i];
		iter = iter->next;
	}

	Q_free(items);
}

// =========================================================================================
// Control Tree
// =========================================================================================

//
// Control tree -
// Sets the drawing bounds for a control and then calls the function
// recursivly on all it's children. These bounds are used to restrict the drawing
// of all children that should be contained within it's parent, and their children
// to within the bounds of the parent.
//
static void EZ_tree_SetDrawBounds(ez_control_t *control)
{
	ez_dllist_node_t *iter = NULL;
	ez_control_t *child = NULL;
	ez_control_t *p = control->parent;
	qbool contained = control->ext_flags & control_contained;

	// Calculate the controls bounds.
	int top		= control->absolute_y;
	int bottom	= control->absolute_y + control->height;
	int left	= control->absolute_x;
	int right	= control->absolute_x + control->width;

	// If the control has a parent (and should be contained within it's parent), 
	// set the corresponding bound to the parents bound (ex. button), 
	// otherwise use the drawing area of the control as bounds (ex. windows).
	control->bound_top		= (p && contained && (top	 < p->bound_top))		? (p->bound_top)	: top;
	control->bound_bottom	= (p && contained && (bottom > p->bound_bottom))	? (p->bound_bottom)	: bottom;
	control->bound_left		= (p && contained && (left	 < p->bound_left))		? (p->bound_left)	: left;
	control->bound_right	= (p && contained && (right	 > p->bound_right))		? (p->bound_right)	: right;

	// Make sure that the left bounds isn't greater than the right bounds and so on.
	// This would lead to controls being visible in a few incorrect cases.
	clamp(control->bound_top, 0, control->bound_bottom);
	clamp(control->bound_bottom, control->bound_top, vid.height);
	clamp(control->bound_left, 0, control->bound_right);
	clamp(control->bound_right, control->bound_left, vid.width);

	// Calculate the bounds for the children.
	for (iter = control->children.head; iter; iter = iter->next)
	{
		child = (ez_control_t *)iter->payload;

		// TODO : Probably should some better check for infinte loop here also.
		if (child == control)
		{
			Sys_Error("EZ_tree_SetDrawBounds(): Infinite loop, child is its own parent.\n");
		}

		EZ_tree_SetDrawBounds(child);
	}
}

//
// Control Tree - Draws a control tree.
//
void EZ_tree_Draw(ez_tree_t *tree)
{
	ez_control_t *payload = NULL;
	ez_dllist_node_t *iter = tree->drawlist.head;

	if (!tree->root)
	{
		return;
	}

	// Calculate the drawing bounds for all the controls in the control tree.
	EZ_tree_SetDrawBounds(tree->root);

	while (iter)
	{
		payload = (ez_control_t *)iter->payload;

		Draw_AlphaRectangleRGB(
			payload->absolute_virtual_x, 
			payload->absolute_virtual_y, 
			payload->virtual_width, 
			payload->virtual_height, 
			1, false, RGBA_TO_COLOR(255, 0, 0, 125));

		// TODO : Remove this test stuff.
		if (!strcmp(payload->name, "label"))
		{
			Draw_String(payload->absolute_virtual_x, payload->absolute_virtual_y - 10, 
				va("avx: %i avy: %i vx: %i vy %i", 
				payload->absolute_virtual_x, payload->absolute_virtual_y, payload->virtual_x, payload->virtual_y));
		}

		// Make sure that the control draws only within it's bounds.
		Draw_EnableScissor(payload->bound_left, payload->bound_right, payload->bound_top, payload->bound_bottom);

		// Raise the draw event for the control.
		CONTROL_RAISE_EVENT(NULL, payload, ez_control_t, OnDraw);

		// Reset the drawing bounds to the entire screen after the control has been drawn.
		Draw_DisableScissor();

		iter = iter->next;
	}
}

//
// Control Tree - Dispatches a mouse event to a control tree.
//
qbool EZ_tree_MouseEvent(ez_tree_t *tree, mouse_state_t *ms)
{
	int mouse_handled = false;
	ez_control_t *payload = NULL;
	ez_dllist_node_t *iter = NULL;

	if (!tree)
	{
		Sys_Error("EZ_tree_MouseEvent: NULL tree reference.\n");
	}

	// Propagate the mouse event in the opposite order that we drew
	// the controls (Since they are drawn from back to front), so
	// that the foremost control gets it first.
	for (iter = tree->drawlist.tail; iter; iter = iter->previous)
	{
		payload = (ez_control_t *)iter->payload;

		// Notify the control of the mouse event.
		CONTROL_RAISE_EVENT(&mouse_handled, payload, ez_control_t, OnMouseEvent, ms);

		if (mouse_handled)
		{
			return mouse_handled;
		}
	}

	return mouse_handled;
}

//
// Control Tree - Moves the focus to the next control in the control tree.
//
void EZ_tree_ChangeFocus(ez_tree_t *tree, qbool next_control)
{
	qbool found = false;
	ez_dllist_node_t *node_iter = NULL;

	if(tree->focused_node)
	{
		node_iter = (next_control) ? tree->focused_node->next : tree->focused_node->previous;

		// Find the next control that can be focused.
		while(node_iter && !found)
		{
			// ez_control_t *ha = (ez_control_t *)node_iter->payload;
			found = EZ_control_SetFocusByNode((ez_control_t *)node_iter->payload, node_iter);
			node_iter = (next_control) ? node_iter->next : node_iter->previous;
		}
	}

	// We haven't found a focusable control yet,
	// or there was no focused control to start with.
	// So search for one from the start/end of the tab list
	// (depending on what direction we're searching in)
	if(!found || !tree->focused_node)
	{
		node_iter = (next_control) ? tree->tablist.head : tree->tablist.tail;

		// Find the next control that can be focused.
		while(node_iter && !found)
		{
			found = EZ_control_SetFocusByNode((ez_control_t *)node_iter->payload, node_iter);
			node_iter = (next_control) ? node_iter->next : node_iter->previous;
		}
	}

	// There is nothing to focus on.
	if(!found)
	{
		tree->focused_node = NULL;
	}
}

//
// Control Tree - Key event.
//
qbool EZ_tree_KeyEvent(ez_tree_t *tree, int key, int unichar, qbool down)
{
	int key_handled = false;
	ez_control_t *payload = NULL;
	ez_dllist_node_t *iter = NULL;

	if (!tree)
	{
		Sys_Error("EZ_tree_KeyEvent(): NULL control tree specified.\n");
	}

	if (tree->root && down)
	{
		switch (key)
		{
			case 'k' :
				key_handled = true;
				break;
			case K_TAB :
			{
				// Focus on the next focusable control (TAB)
				// or the previous (Shift + TAB)
				EZ_tree_ChangeFocus(tree, !isShiftDown());
				key_handled = true;
				break;
			}
		}
	}

	// Find the focused control and send the key events to it.
	for (iter = tree->drawlist.head; iter; iter = iter->next)
	{
		payload = (ez_control_t *)iter->payload;

		// Notify the control of the key event.
		if (payload->int_flags & control_focused)
		{
			CONTROL_RAISE_EVENT(&key_handled, payload, ez_control_t, OnKeyEvent, key, unichar, down);
			break;
		}
	}

	return key_handled;
}

//
// Control Tree - Finds any orphans and adds them to the root control.
//
void EZ_tree_UnOrphanizeChildren(ez_tree_t *tree)
{
	ez_control_t *payload = NULL;
	ez_dllist_node_t *iter = NULL;

	if(!tree)
	{
		assert(!"EZ_control_UnOrphanizeChildren: No control tree specified.\n");
		return;
	}

	iter = tree->drawlist.head;

	while(iter)
	{
		payload = (ez_control_t *)iter->payload;

		if(!payload->parent && payload != tree->root)
		{
			// The control is an orphan, and not the root control.
			EZ_double_linked_list_Add(&tree->root->children, payload);
		}
		else if(!tree->root)
		{
			// There was no root (should never happen).
			tree->root = payload;
		}

		iter = iter->next;
	}
}

//
// Control Tree - Order function for the draw list based on the draw_order property.
//
static int EZ_tree_DrawOrderFunc(const void *val1, const void *val2)
{
	const ez_control_t **control1 = (const ez_control_t **)val1;
	const ez_control_t **control2 = (const ez_control_t **)val2;

	return ((*control1)->draw_order - (*control2)->draw_order);
}

//
// Control Tree - Orders the draw list based on the draw order property.
//
void EZ_tree_OrderDrawList(ez_tree_t *tree)
{
	EZ_double_linked_list_Order(&tree->drawlist, EZ_tree_DrawOrderFunc);
}

//
// Control Tree - Tree Order function for the tab list based on the tab_order property.
//
static int EZ_tree_TabOrderFunc(const void *val1, const void *val2)
{
	const ez_control_t **control1 = (const ez_control_t **)val1;
	const ez_control_t **control2 = (const ez_control_t **)val2;

	return ((*control1)->tab_order - (*control2)->tab_order);
}

//
// Control Tree - Orders the tab list based on the tab order property.
//
void EZ_tree_OrderTabList(ez_tree_t *tree)
{
	EZ_double_linked_list_Order(&tree->tablist, EZ_tree_TabOrderFunc);
}

// =========================================================================================
// Control
// =========================================================================================

//
// Control - Creates a new control and initializes it.
//
ez_control_t *EZ_control_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  char *background_name, int flags)
{
	ez_control_t *control = NULL;

	// We have to have a tree to add the control to.
	if(!tree)
	{
		return NULL;
	}

	control = (ez_control_t *)Q_malloc(sizeof(ez_control_t));
	memset(control, 0, sizeof(ez_control_t));

	EZ_control_Init(control, tree, parent, name, description, x, y, width, height, background_name, flags);

	return control;
}

//
// Control - 
// Returns the screen position of the control. This will be different for a scrollable window
// since it's drawing position differs from the windows actual position on screen.
//
void EZ_control_GetDrawingPosition(ez_control_t *self, int *x, int *y)
{
	if (self->ext_flags & control_scrollable)
	{
		(*x) = self->absolute_virtual_x;
		(*y) = self->absolute_virtual_y;
	}
	else
	{
		(*x) = self->absolute_x;
		(*y) = self->absolute_y;
	}
}

//
// Control - Initializes a control and adds it to the specified control tree.
//
void EZ_control_Init(ez_control_t *control, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  char *background_name, ez_control_flags_t flags)
{
	static int order = 0;

	control->CLASS_ID = EZ_CLASS_ID;

	control->control_tree	= tree;
	control->name			= name;
	control->description	= description;
	control->ext_flags		= flags | control_enabled | control_visible;
	control->anchor_flags	= anchor_none;

	// Default to containing a child within it's parent
	// if the parent is being contained by it's parent.
	if (parent && parent->ext_flags & control_contained)
	{
		control->ext_flags |= control_contained;
	}

	control->draw_order = order++;

	control->resize_handle_thickness = 5;
	control->width_max	= vid.conwidth;
	control->height_max = vid.conheight;
	control->width_min	= 5;
	control->height_min = 5;

	// This should NEVER be changed.
	control->inheritance_level = EZ_CONTROL_INHERITANCE_LEVEL;

	// Setup the default event functions, none of these should ever be NULL.
	control->events.OnMouseEvent		= EZ_control_OnMouseEvent;
	control->events.OnMouseClick		= EZ_control_OnMouseClick;
	control->events.OnMouseDown			= EZ_control_OnMouseDown;
	control->events.OnMouseUp			= EZ_control_OnMouseUp;
	control->events.OnMouseUpOutside	= EZ_control_OnMouseUpOutside;
	control->events.OnMouseEnter		= EZ_control_OnMouseEnter;
	control->events.OnMouseLeave		= EZ_control_OnMouseLeave;
	control->events.OnMouseHover		= EZ_control_OnMouseHover;
	control->events.OnDestroy			= EZ_control_Destroy;
	control->events.OnDraw				= EZ_control_OnDraw;
	control->events.OnGotFocus			= EZ_control_OnGotFocus;
	control->events.OnKeyEvent			= EZ_control_OnKeyEvent;
	control->events.OnKeyDown			= EZ_control_OnKeyDown;
	control->events.OnKeyUp				= EZ_control_OnKeyUp;
	control->events.OnLostFocus			= EZ_control_OnLostFocus;
	control->events.OnLayoutChildren	= EZ_control_OnLayoutChildren;
	control->events.OnMove				= EZ_control_OnMove;
	control->events.OnScroll			= EZ_control_OnScroll;
	control->events.OnResize			= EZ_control_OnResize;
	control->events.OnParentResize		= EZ_control_OnParentResize;
	control->events.OnMinVirtualResize	= EZ_control_OnMinVirtualResize;
	control->events.OnVirtualResize		= EZ_control_OnVirtualResize;
	control->events.OnFlagsChanged		= EZ_control_OnFlagsChanged;

	// Load the background image.
	if(background_name)
	{
		control->background = Draw_CachePicSafe(background_name, false, true);
	}

	// Add the control to the control tree.
	if(!tree->root)
	{
		// The first control will be the root.
		tree->root = control;
	}
	else if(!parent)
	{
		// No parent was given so make the control a root child.
		EZ_control_AddChild(tree->root, control);
	}
	else
	{
		// Add the control to the specified parent.
		EZ_control_AddChild(parent, control);
	}

	// Add the control to the draw and tab list.
	EZ_double_linked_list_Add(&tree->drawlist, (void *)control);
	EZ_double_linked_list_Add(&tree->tablist, (void *)control);

	// Order the lists.
	EZ_tree_OrderDrawList(tree);
	EZ_tree_OrderTabList(tree);

	// Position and size of the control.
	control->prev_width				= width;
	control->prev_height			= height;
	control->prev_virtual_width		= width;
	control->prev_virtual_height	= height;

	EZ_control_SetVirtualSize(control, width, height);
	EZ_control_SetMinVirtualSize(control, width, height);
	EZ_control_SetPosition(control, x, y);
	EZ_control_SetSize(control, width, height);
}

//
// Control - Destroys a specified control.
//
int EZ_control_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_dllist_node_t *iter = NULL;
	ez_dllist_node_t *temp = NULL;

	// Nothing to destroy :(
	if(!self)
	{
		return 0;
	}

	if(!self->control_tree)
	{
		// Very bad!
		char *msg = "EZ_control_Destroy(): tried to destroy control without a tree.\n";
		assert(!msg);
		Sys_Error(msg);
		return 0;
	}

	// Cleanup any specifics this control may have.
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDestroy, destroy_children);

	iter = self->children.head;

	// Destroy the children!
	while(iter)
	{
		if(destroy_children)
		{
			// Destroy the child!
			CONTROL_RAISE_EVENT(NULL, ((ez_control_t *)iter->payload), ez_control_t, OnDestroy, destroy_children);
		}
		else
		{
			// The child becomes an orphan :~/
			((ez_control_t *)iter->payload)->parent = NULL;
		}

		temp = iter;
		iter = iter->next;

		// Remove the child from the list.
		EZ_double_linked_list_Remove(&self->children, temp);
	}

	Q_free(self);

	return 0;
}

//
// Control - Sets the OnDestroy event handler.
//
void EZ_control_SetOnDestroy(ez_control_t *self, ez_control_destroy_handler_fp OnDestroy)
{
	self->event_handlers.OnDestroy = OnDestroy;
}

//
// Control - Sets the OnFlagsChanged event handler.
//
void EZ_control_SetOnFlagsChanged(ez_control_t *self, ez_control_handler_fp OnFlagsChanged)
{
	self->event_handlers.OnFlagsChanged = OnFlagsChanged;
}

//
// Control - Sets the OnLayoutChildren event handler.
//
void EZ_control_SetOnLayoutChildren(ez_control_t *self, ez_control_handler_fp OnLayoutChildren)
{
	self->event_handlers.OnLayoutChildren = OnLayoutChildren;
}

//
// Control - Sets the OnMove event handler.
//
void EZ_control_SetOnMove(ez_control_t *self, ez_control_handler_fp OnMove)
{
	self->event_handlers.OnMove = OnMove;
}

//
// Control - Sets the OnScroll event handler.
//
void EZ_control_SetOnScroll(ez_control_t *self, ez_control_handler_fp OnScroll)
{
	self->event_handlers.OnScroll = OnScroll;
}

//
// Control - Sets the OnResize event handler.
//
void EZ_control_SetOnResize(ez_control_t *self, ez_control_handler_fp OnResize)
{
	self->event_handlers.OnResize = OnResize;
}

//
// Control - Sets the OnParentResize event handler.
//
void EZ_control_SetOnParentResize(ez_control_t *self, ez_control_handler_fp OnParentResize)
{
	self->event_handlers.OnParentResize = OnParentResize;
}

//
// Control - Sets the OnMinVirtualResize event handler.
//
void EZ_control_SetOnMinVirtualResize(ez_control_t *self, ez_control_handler_fp OnMinVirtualResize)
{
	self->event_handlers.OnMinVirtualResize = OnMinVirtualResize;
}

//
// Control - Sets the OnVirtualResize event handler.
//
void EZ_control_SetOnVirtualResize(ez_control_t *self, ez_control_handler_fp OnVirtualResize)
{
	self->event_handlers.OnVirtualResize = OnVirtualResize;
}

//
// Control - Sets the OnKeyEvent event handler.
//
void EZ_control_SetOnKeyEvent(ez_control_t *self, ez_control_key_handler_fp OnKeyEvent)
{
	self->event_handlers.OnKeyEvent = OnKeyEvent;
}

//
// Control - Sets the OnLostFocus event handler.
//
void EZ_control_SetOnLostFocus(ez_control_t *self, ez_control_handler_fp OnLostFocus)
{
	self->event_handlers.OnLostFocus = OnLostFocus;
}

//
// Control - Sets the OnGotFocus event handler.
//
void EZ_control_SetOnGotFocus(ez_control_t *self, ez_control_handler_fp OnGotFocus)
{
	self->event_handlers.OnGotFocus = OnGotFocus;
}

//
// Control - Sets the OnMouseHover event handler.
//
void EZ_control_SetOnMouseHover(ez_control_t *self, ez_control_mouse_handler_fp OnMouseHover)
{
	self->event_handlers.OnMouseHover = OnMouseHover;
}

//
// Control - Sets the OnMouseLeave event handler.
//
void EZ_control_SetOnMouseLeave(ez_control_t *self, ez_control_mouse_handler_fp OnMouseLeave)
{
	self->event_handlers.OnMouseLeave = OnMouseLeave;
}

//
// Control - Sets the OnMouseEnter event handler.
//
void EZ_control_SetOnMouseEnter(ez_control_t *self, ez_control_mouse_handler_fp OnMouseEnter)
{
	self->event_handlers.OnMouseEnter = OnMouseEnter;
}

//
// Control - Sets the OnMouseClick event handler.
//
void EZ_control_SetOnMouseClick(ez_control_t *self, ez_control_mouse_handler_fp OnMouseClick)
{
	self->event_handlers.OnMouseClick = OnMouseClick;
}

//
// Control - Sets the OnMouseUp event handler.
//
void EZ_control_SetOnMouseUp(ez_control_t *self, ez_control_mouse_handler_fp OnMouseUp)
{
	self->event_handlers.OnMouseUp = OnMouseUp;
}

//
// Control - Sets the OnMouseUpOutside event handler.
//
void EZ_control_SetOnMouseUpOutside(ez_control_t *self, ez_control_mouse_handler_fp OnMouseUpOutside)
{
	self->event_handlers.OnMouseUpOutside = OnMouseUpOutside;
}

//
// Control - Sets the OnMouseDown event handler.
//
void EZ_control_SetOnMouseDown(ez_control_t *self, ez_control_mouse_handler_fp OnMouseDown)
{
	self->event_handlers.OnMouseDown = OnMouseDown;
}

//
// Control - Sets the OnMouseEvent event handler.
//
void EZ_control_SetOnMouseEvent(ez_control_t *self, ez_control_mouse_handler_fp OnMouseEvent)
{
	self->event_handlers.OnMouseEvent = OnMouseEvent;
}

//
// Control - Sets the OnDraw event handler.
//
void EZ_control_SetOnDraw(ez_control_t *self, ez_control_handler_fp OnDraw)
{
	self->event_handlers.OnDraw = OnDraw;
}

//
// Control - Focuses on a control.
//
qbool EZ_control_SetFocus(ez_control_t *self)
{
	return EZ_control_SetFocusByNode(self, EZ_double_linked_list_FindByPayload(&self->control_tree->tablist, self));
}

//
// Control - Focuses on a control associated with a specified node from the tab list.
//
qbool EZ_control_SetFocusByNode(ez_control_t *self, ez_dllist_node_t *node)
{
	ez_tree_t *tree = NULL;

	if(!self || !node->payload)
	{
		Sys_Error("EZ_control_SetFocus(): Cannot focus on a NULL control.\n");
	}

	// The nodes payload and the control must be the same.
	if(self != node->payload)
	{
		return false;
	}

	// We can't focus on this control.
	if(!(self->ext_flags & control_focusable) || !(self->ext_flags & control_enabled))
	{
		return false;
	}

	tree = self->control_tree;

	// Steal the focus from the currently focused control.
	if(tree->focused_node)
	{
		ez_control_t *payload = NULL;

		if(!tree->focused_node->payload)
		{
			Sys_Error("EZ_control_SetFocus(): Focused node has a NULL payload.\n");
		}

		payload = (ez_control_t *)tree->focused_node->payload;

		// We're trying to focus on the already focused node.
		if(payload == self)
		{
			return true;
		}

		// Remove the focus flag and set the focus node to NULL.
		payload->int_flags &= ~control_focused;
		tree->focused_node = NULL;

		// Reset all manipulation flags.
		payload->int_flags &= ~(control_clicked | control_moving | control_resizing_left | control_resizing_right | control_resizing_bottom | control_resizing_top);

		// Raise event for losing the focus.
		CONTROL_RAISE_EVENT(NULL, payload, ez_control_t, OnLostFocus);
	}

	// Set the new focus.
	self->int_flags |= control_focused;
	tree->focused_node = node;

	// Raise event for getting focus.
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnGotFocus);

	return true;
}

//
// Control - Sets whetever the control is contained within the bounds of it's parent or not, or is allowed to draw outside it.
//
void EZ_control_SetContained(ez_control_t *self, qbool contained)
{
	SET_FLAG(self->ext_flags, control_contained, contained);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnFlagsChanged);
}

//
// Control - Sets whetever the control is enabled or not.
//
void EZ_control_SetEnabled(ez_control_t *self, qbool enabled)
{
	// TODO : Make sure input isn't allowed if a control isn't enabled.
	SET_FLAG(self->ext_flags, control_enabled, enabled);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnFlagsChanged);
}

//
// Control - Sets whetever the control is movable.
//
void EZ_control_SetMovable(ez_control_t *self, qbool movable)
{
	SET_FLAG(self->ext_flags, control_movable, movable);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnFlagsChanged);
}

//
// Control - Sets whetever the control is focusable.
//
void EZ_control_SetFocusable(ez_control_t *self, qbool focusable)
{
	SET_FLAG(self->ext_flags, control_focusable, focusable);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnFlagsChanged);
}

//
// Control - Sets whetever the control is resizeable horizontally by the user.
//
void EZ_control_SetResizeableHorizontally(ez_control_t *self, qbool resize_horizontally)
{
	SET_FLAG(self->ext_flags, control_resize_h, resize_horizontally);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnFlagsChanged);
}

//
// Control - Sets whetever the control is resizeable both horizontally and vertically by the user.
//
void EZ_control_SetResizeableBoth(ez_control_t *self, qbool resize)
{
	SET_FLAG(self->ext_flags, (control_resize_h | control_resize_v), resize);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnFlagsChanged);
}

//
// Control - Sets whetever the control is resizeable at all, not just by the user.
//
void EZ_control_SetResizeable(ez_control_t *self, qbool resize_vertically)
{
	// TODO : Is it confusing having this resizeable?
	SET_FLAG(self->ext_flags, control_resize_v, resize_vertically);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnFlagsChanged);
}

//
// Control - Sets whetever the control is resizeable vertically by the user.
//
void EZ_control_SetResizeableVertically(ez_control_t *self, qbool resize_vertically)
{
	SET_FLAG(self->ext_flags, control_resize_v, resize_vertically);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnFlagsChanged);
}

//
// Control - Sets whetever the control is visible or not.
//
void EZ_control_SetVisible(ez_control_t *self, qbool visible)
{
	SET_FLAG(self->ext_flags, control_visible, visible);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnFlagsChanged);
}

//
// Control - Sets whetever the control is scrollable or not.
//
void EZ_control_SetScrollable(ez_control_t *self, qbool scrollable)
{
	SET_FLAG(self->ext_flags, control_scrollable, scrollable);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnFlagsChanged);
}

//
// Control - Sets the external flags of the control.
//
void EZ_control_SetFlags(ez_control_t *self, ez_control_flags_t flags)
{
	self->ext_flags = flags;
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnFlagsChanged);
}

//
// Control - Sets the external flags of the control.
//
ez_control_flags_t EZ_control_GetFlags(ez_control_t *self)
{
	return self->ext_flags;
}

//
// Control - Sets the tab order of a control.
//
void EZ_control_SetTabOrder(ez_control_t *self, int tab_order)
{
	self->tab_order = tab_order;
	EZ_tree_OrderTabList(self->control_tree);
}

//
// Control - Sets the anchoring of the control to it's parent.
//
void EZ_control_SetAnchor(ez_control_t *self, ez_anchor_t anchor_flags)
{
	self->anchor_flags = anchor_flags;

	// Make sure the control is repositioned correctly.
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnMove);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnParentResize);
}

//
// Control - Sets the tab order of a control.
//
void EZ_control_SetDrawOrder(ez_control_t *self, int draw_order)
{
	self->draw_order = draw_order;
	EZ_tree_OrderTabList(self->control_tree);
}

//
// Control - Sets the size of a control.
//
void EZ_control_SetSize(ez_control_t *self, int width, int height)
{
	self->prev_width = self->width;
	self->prev_height = self->height;

	self->width  = width;
	self->height = height;

	clamp(self->width, self->width_min, self->width_max);
	clamp(self->height, self->height_min, self->height_max);

	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnResize);
}

//
// Control - Set the max size for the control.
//
void EZ_control_SetMaxSize(ez_control_t *self, int max_width, int max_height)
{
	self->width_max = max(0, max_width);
	self->height_max = max(0, max_height);

	// Do we need to change the size of the control to fit?
	if ((self->width > self->width_max) || (self->height > self->height_max))
	{
		EZ_control_SetSize(self, min(self->width, self->width_max), min(self->height, self->height_max));
	}
}

//
// Control - Set the min size for the control.
//
void EZ_control_SetMinSize(ez_control_t *self, int min_width, int min_height)
{
	self->width_max = max(0, min_width);
	self->height_max = max(0, min_height);

	// Do we need to change the size of the control to fit?
	if ((self->width < self->width_min) || (self->height < self->height_min))
	{
		EZ_control_SetSize(self, max(self->width, self->width_min), max(self->height, self->height_min));
	}
}

//
// Control - Set color of a control.
//
void EZ_control_SetBackgroundColor(ez_control_t *self, byte r, byte g, byte b, byte alpha)
{
	self->background_color[0] = r;
	self->background_color[1] = g;
	self->background_color[2] = b;
	self->background_color[3] = alpha;
}

//
// Control - Sets the position of a control, relative to it's parent.
//
void EZ_control_SetPosition(ez_control_t *self, int x, int y)
{
	// Set the new relative position.
	self->x = x;
	self->y = y;

	// Raise the event that we have moved.
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnMove);
}

//
// Control - Sets the part of the control that should be shown if it's scrollable.
//
void EZ_control_SetScrollPosition(ez_control_t *self, int scroll_x, int scroll_y)
{
	// Only scroll scrollable controls.
	if (!(self->ext_flags & control_scrollable))
	{
		return;
	}

	// Don't allow scrolling outside the scroll area.
	if ((scroll_x > (self->virtual_width - self->width)) 
	 || (scroll_y > (self->virtual_height - self->height))
	 || (scroll_x < 0)
	 || (scroll_y < 0))
	{
		return;
	}

	self->virtual_x = scroll_x;
	self->virtual_y = scroll_y;
 
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnScroll);
}

//
// Control - Convenient function for changing the scroll position by a specified amount.
//
void EZ_control_SetScrollChange(ez_control_t *self, int delta_scroll_x, int delta_scroll_y)
{
	EZ_control_SetScrollPosition(self, (self->virtual_x + delta_scroll_x), (self->virtual_y + delta_scroll_y));
}

//
// Control - Sets the virtual size of the control (this area can be scrolled around in).
//
void EZ_control_SetVirtualSize(ez_control_t *self, int virtual_width, int virtual_height)
{
	self->prev_virtual_width = self->virtual_width;
	self->prev_virtual_height = self->virtual_height;

	// Set the new virtual size of the control (cannot be smaller than the current size of the control).
	self->virtual_width = max(virtual_width, self->virtual_width_min);
	self->virtual_height = max(virtual_height, self->virtual_height_min);

	self->virtual_width = max(self->virtual_width, self->width + self->virtual_x);
	self->virtual_height = max(self->virtual_height, self->height + self->virtual_y);

	// Raise the event that we have changed the virtual size of the control.
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnVirtualResize);
}

//
// Control - Set the min virtual size for the control, the control size is not allowed to be larger than this.
//
void EZ_control_SetMinVirtualSize(ez_control_t *self, int min_virtual_width, int min_virtual_height)
{
	self->virtual_width_min = max(0, min_virtual_width);
	self->virtual_height_min = max(0, min_virtual_height);

	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnMinVirtualResize);
}

//
// Control - Returns true if this control is the root control.
//
qbool EZ_control_IsRoot(ez_control_t *self)
{
	return (self->parent == NULL);
}

//
// Control - Adds a child to the control.
//
void EZ_control_AddChild(ez_control_t *self, ez_control_t *child)
{
	// Remove the control from it's current parent.
	if (child->parent)
	{
		EZ_double_linked_list_RemoveByPayload(&child->parent->children, child);
	}
 
	// Set the new parent of the child.
	child->parent = self;

	// Add the child to the new parents list of children.
	EZ_double_linked_list_Add(&self->children, child);
}

//
// Control - Remove a child from the control. Returns a reference to the child that was removed.
//
ez_control_t *EZ_control_RemoveChild(ez_control_t *self, ez_control_t *child)
{
	return (ez_control_t *)EZ_double_linked_list_RemoveByPayload(&self->children, child);
}

//
// Control - The control got focus.
//
int EZ_control_OnGotFocus(ez_control_t *self)
{
	if(!(self->int_flags & control_focused))
	{
		return 0;
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnGotFocus);

	return 0;
}

//
// Control - The control lost focus.
//
int EZ_control_OnLostFocus(ez_control_t *self)
{
	if (self->int_flags & control_focused)
	{
		return 0;
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnLostFocus);

	return 0;
}

//
// Control - The control was resized.
//
int EZ_control_OnResize(ez_control_t *self)
{
	// Calculate how much the width has changed so that we can
	// compensate by scrolling.
	int scroll_change_x = (self->prev_width - self->width);
	int scroll_change_y = (self->prev_height - self->height);

	ez_control_t *payload = NULL;
	ez_dllist_node_t *iter = self->children.head;

	// Make sure the virtual size never is smaller than the normal size of the control
	// and that it's never smaller than the virtual max size.
	EZ_control_SetVirtualSize(self, self->width, self->height);

	// First scroll the window so that we use the current virtual space fully
	// before expanding it. That is, only start expanding when the:
	// REAL_SIZE > VIRTUAL_MIN_SIZE
	if (scroll_change_x < 0)
	{
		EZ_control_SetScrollChange(self, scroll_change_x, 0);
	}

	if (scroll_change_y < 0)
	{
		EZ_control_SetScrollChange(self, 0, scroll_change_y);
	}

	// Tell the children we've resized.
	while(iter)
	{
		payload = (ez_control_t *)iter->payload;
		CONTROL_RAISE_EVENT(NULL, payload, ez_control_t, OnParentResize);
		iter = iter->next;
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnResize);

	return 0;
}

//
// Control - The controls parent was resized.
//
int EZ_control_OnParentResize(ez_control_t *self)
{
	if (self->parent && (self->ext_flags & control_resizeable))
	{
		int width = self->width;
		int height = self->height;

		if ((self->anchor_flags & (anchor_left | anchor_right)) == (anchor_left | anchor_right))
		{
			// The position of the right side of the control relative to the parents right side.
			int x_from_right = self->parent->prev_virtual_width - (self->x + self->width);
			
			// Set the new width so that the right side of the control is
			// still the same distance from the right side of the parent.
			width = self->parent->virtual_width - (self->x + x_from_right);
		}

		if ((self->anchor_flags & (anchor_top | anchor_bottom)) == (anchor_top | anchor_bottom))
		{
			int x_from_bottom = self->parent->prev_virtual_height - (self->x + self->height);
			height = self->parent->virtual_height - (self->x + x_from_bottom);
		}

		// Set the new size if it changed.
		if (self->width != width || self->height != height)
		{
			EZ_control_SetSize(self, width, height);
		}
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnParentResize);

	return 0;
}

//
// Control - The minimum virtual size has changed for the control.
//
int EZ_control_OnMinVirtualResize(ez_control_t *self)
{
	self->virtual_width		= max(self->virtual_width_min, self->virtual_width);
	self->virtual_height	= max(self->virtual_height_min, self->virtual_height);

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnMinVirtualResize);

	return 0;
}

//
// Label - The flags for the control changed.
//
int EZ_control_OnFlagsChanged(ez_control_t *self)
{
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnFlagsChanged);

	return 0;
}

//
// Label - The virtual size of the control has changed.
//
int EZ_control_OnVirtualResize(ez_control_t *self)
{
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnVirtualResize);

	return 0;
}

//
// Control - The control was moved.
//
int EZ_control_OnMove(ez_control_t *self)
{
	ez_control_t *payload = NULL;
	ez_dllist_node_t *iter = self->children.head;

	// Update the absolute screen position based on the parents position.
	self->absolute_x = (self->parent ? self->parent->absolute_virtual_x : 0) + self->x;
	self->absolute_y = (self->parent ? self->parent->absolute_virtual_y : 0) + self->y;

	if (self->parent)
	{
		// If the control has a parent, position it in relation to it
		// and the way it's anchored to it.

		// If we're anchored to both the left and right part of the parent we position
		// based on the parents left pos 
		// (We will stretch to the right if the control is resizable and if the parent is resized).
		if ((self->anchor_flags & anchor_left) && !(self->anchor_flags & anchor_right))
		{
			self->absolute_x = self->parent->absolute_virtual_x + self->x;
		}
		else if ((self->anchor_flags & anchor_right) && !(self->anchor_flags & anchor_left))
		{
			self->absolute_x = self->parent->absolute_virtual_x + self->parent->virtual_width + (self->x - self->width);
		}

		if ((self->anchor_flags & anchor_top) && !(self->anchor_flags & anchor_bottom))
		{
			self->absolute_y = self->parent->absolute_virtual_y + self->y;
		}
		else if ((self->anchor_flags & anchor_bottom) && !(self->anchor_flags & anchor_top))
		{
			self->absolute_y = self->parent->absolute_virtual_y + self->parent->virtual_height + (self->y - self->height);
		}
	}
	else
	{
		// No parent, position based on screen coordinates.
		self->absolute_x = self->x;
		self->absolute_y = self->y;
	}

	// We need to move the virtual area of the control also.
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnScroll);

	// Tell the children we've moved.
	while(iter)
	{
		payload = (ez_control_t *)iter->payload;
		CONTROL_RAISE_EVENT(NULL, payload, ez_control_t, OnMove);
		iter = iter->next;
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnMove);

	return 0;
}

//
// Control - On scroll event.
//
int EZ_control_OnScroll(ez_control_t *self)
{
	ez_control_t *payload = NULL;
	ez_dllist_node_t *iter = self->children.head;

	self->absolute_virtual_x = self->absolute_x - self->virtual_x;
	self->absolute_virtual_y = self->absolute_y - self->virtual_y;

	// Tell the children we've scrolled.
	while(iter)
	{
		payload = (ez_control_t *)iter->payload;
		CONTROL_RAISE_EVENT(NULL, payload, ez_control_t, OnMove);
		iter = iter->next;
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnScroll);

	return 0;
}

//
// Control - Layouts children.
//
int EZ_control_OnLayoutChildren(ez_control_t *self)
{
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnLayoutChildren);
	return 0;
}

//
// Control - Draws the control.
//
int EZ_control_OnDraw(ez_control_t *self)
{
	int x, y;
	EZ_control_GetDrawingPosition(self, &x, &y);

	if (self->background_color[3] > 0)
	{
		Draw_AlphaRectangleRGB(self->absolute_x, self->absolute_y, self->width, self->height, 1, true, RGBAVECT_TO_COLOR(self->background_color));
	}

	Draw_String(x, y, va("%s%s%s%s",
			((self->int_flags & control_moving) ? "M" : " "),
			((self->int_flags & control_focused) ? "F" : " "),
			((self->int_flags & control_clicked) ? "C" : " "),
			((self->int_flags & control_resizing_left) ? "R" : " ")
			));

	// Draw control specifics.
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDraw);

	return 0;
}

//
// Control - Key down event.
//
int EZ_control_OnKeyDown(ez_control_t *self, int key, int unichar)
{
	int key_handled = false;

	CONTROL_EVENT_HANDLER_CALL(&key_handled, self, ez_control_t, OnKeyDown, key, unichar);

	return key_handled;
}

//
// Control - Key up event.
//
int EZ_control_OnKeyUp(ez_control_t *self, int key, int unichar)
{
	int key_handled = false;

	CONTROL_EVENT_HANDLER_CALL(&key_handled, self, ez_control_t, OnKeyUp, key, unichar);

	return key_handled;
}

//
// Control - Key event.
//
int EZ_control_OnKeyEvent(ez_control_t *self, int key, int unichar, qbool down)
{
	int key_handled			= false;
	ez_control_t *payload	= NULL;
	ez_dllist_node_t *iter	= self->children.head;

	if (down)
	{
		CONTROL_RAISE_EVENT(&key_handled, self, ez_control_t, OnKeyDown, key, unichar);
	}
	else
	{
		CONTROL_RAISE_EVENT(&key_handled, self, ez_control_t, OnKeyUp, key, unichar);
	}

	if (key_handled)
	{
		return true;
	}

	CONTROL_EVENT_HANDLER_CALL(&key_handled, self, ez_control_t, OnKeyEvent, key, unichar, down);

	// Tell the children of the key event.
	/*while (iter)
	{
		payload = (ez_control_t *)iter->payload;
		CONTROL_RAISE_EVENT(&key_handled_tmp, payload, ez_control_t, OnKeyEvent, key, unichar, down);

		// FIXME: How do we decide what control that gets the key events?
		key_handled = (key_handled || key_handled_tmp);

		iter = iter->next;
	}*/

	return key_handled;
}

typedef enum
{
	RESIZE_LEFT		= (1 << 0),
	RESIZE_RIGHT	= (1 << 1),
	RESIZE_UP		= (1 << 2),
	RESIZE_DOWN		= (1 << 3)
} resize_direction_t;

//
// Control - Resizes the control by moving the left corner.
//
static void EZ_control_ResizeByDirection(ez_control_t *self, mouse_state_t *ms, int delta_x, int delta_y, resize_direction_t direction)
{
	int width = self->width;
	int height = self->height;
	int x = self->x;
	int y = self->y;
	qbool resizing_width = (direction & RESIZE_LEFT) || (direction & RESIZE_RIGHT);
	qbool resizing_height = (direction & RESIZE_UP) || (direction & RESIZE_DOWN);

	if (resizing_width)
	{
		// Set the new width based on how much the mouse has moved
		// keeping it within the allowed bounds.
		width = self->width + ((direction & RESIZE_LEFT) ? delta_x : -delta_x);
		clamp(width, self->width_min, self->width_max);

		// Move the control to counteract the resizing when resizing to the left.
		x = self->x + ((direction & RESIZE_LEFT) ? (self->width - width) : 0);
	}

	if (resizing_height)
	{
		height = self->height + ((direction & RESIZE_UP) ? delta_y : -delta_y);
		clamp(height, self->height_min, self->height_max);
		y = self->y + ((direction & RESIZE_UP) ? (self->height - height) : 0);
	}

	// If the child should be contained within it's parent the
	// mouse should stay within the parent also when resizing the control.
	if (ms && CONTROL_IS_CONTAINED(self))
	{
		if (resizing_width && MOUSE_OUTSIDE_PARENT_X(self, ms))
		{
			ms->x = ms->x_old;
			x = self->x;
			width = self->width;
		}

		if (resizing_height && MOUSE_OUTSIDE_PARENT_Y(self, ms))
		{
			ms->y = ms->y_old;
			y = self->y;
			height = self->height;
		}
	}

	EZ_control_SetSize(self, width, height);
	EZ_control_SetPosition(self, x, y);
}

//
// Control -
// The initial mouse event is handled by this, and then raises more specialized event handlers
// based on the new mouse state.
// 
// NOTICE! When extending this event you need to make sure that you need to tell the framework
// that you've handled all mouse events that happened within the controls bounds in your own
// implementation by returning true whenever the mouse is inside the control.
// This can easily be done with the following macro: MOUSE_INSIDE_CONTROL(self, ms); 
// If this is not done, all mouse events will "fall through" to controls below.
//
int EZ_control_OnMouseEvent(ez_control_t *self, mouse_state_t *ms)
{
	mouse_state_t *old_ms			= &self->prev_mouse_state;
	qbool mouse_inside				= false;
	qbool prev_mouse_inside			= false;
	qbool mouse_inside_parent		= false;
	qbool prev_mouse_inside_parent	= false;
	qbool is_contained				= CONTROL_IS_CONTAINED(self);
	int mouse_handled_tmp			= false;
	int mouse_handled				= false;
	int mouse_delta_x				= 0;
	int mouse_delta_y				= 0;

	if (!ms)
	{
		Sys_Error("EZ_control_OnMouseEvent(): mouse_state_t NULL\n");
	}

	mouse_delta_x = Q_rint(ms->x_old - ms->x);
	mouse_delta_y = Q_rint(ms->y_old - ms->y);

	mouse_inside = MOUSE_INSIDE_CONTROL(self, ms);
	prev_mouse_inside = POINT_IN_RECTANGLE(ms->x_old, ms->y_old, self->absolute_x, self->absolute_y, self->width, self->height);

	// If the control is contained within it's parent,
	// the mouse must be within the parents bounds also for
	// the control to generate any mouse events.
	if (is_contained)
	{
		int p_x = self->parent->absolute_x;
		int p_y = self->parent->absolute_y;
		int p_w = self->parent->width;
		int p_h = self->parent->height;
		mouse_inside_parent = POINT_IN_RECTANGLE(ms->x, ms->y, p_x, p_y, p_w, p_h);
		prev_mouse_inside_parent = POINT_IN_RECTANGLE(old_ms->x, old_ms->y, p_x, p_y, p_w, p_h);
	}

	// Raise more specific events.
	//
	// For contained controls (parts of the control outside of the parent are not drawn)
	// we only want to trigger mouse events when the mouse click is both inside the control and the parent.
	if ((!is_contained && mouse_inside) || (is_contained && mouse_inside && mouse_inside_parent))
	{
		if (!prev_mouse_inside)
		{
			// Were we inside of the control last time? Otherwise we've just entered it.
			CONTROL_RAISE_EVENT(&mouse_handled_tmp, self, ez_control_t, OnMouseEnter, ms);
		}
		else
		{
			// We're hovering the control.
			CONTROL_RAISE_EVENT(&mouse_handled_tmp, self, ez_control_t, OnMouseHover, ms);
		}

		mouse_handled = (mouse_handled || mouse_handled_tmp);

		if (ms->button_down && (ms->button_down != old_ms->button_down))
		{
			// Mouse down.
			CONTROL_RAISE_EVENT(&mouse_handled_tmp, self, ez_control_t, OnMouseDown, ms);
			mouse_handled = (mouse_handled || mouse_handled_tmp);
		}

		if (ms->button_up && (ms->button_up != old_ms->button_up))
		{
			// Mouse up.
			CONTROL_RAISE_EVENT(&mouse_handled_tmp, self, ez_control_t, OnMouseUp, ms);
			mouse_handled = (mouse_handled || mouse_handled_tmp);
		}
	}
	else if((!is_contained && prev_mouse_inside) || (is_contained && !prev_mouse_inside_parent))
	{
		// Mouse leave.
		CONTROL_RAISE_EVENT(&mouse_handled_tmp, self, ez_control_t, OnMouseLeave, ms);
		mouse_handled = (mouse_handled || mouse_handled_tmp);
	}

	// Send a mouse up event to controls if the mouse is outside also.
	if (ms->button_up && (ms->button_up != old_ms->button_up))
	{
		// Mouse up.
		CONTROL_RAISE_EVENT(&mouse_handled_tmp, self, ez_control_t, OnMouseUpOutside, ms);
		mouse_handled = (mouse_handled || mouse_handled_tmp);
	}

	// Make sure we remove the click flag always when releasing the button
	// not just when we're hovering above the control.
	if (ms->button_up && (ms->button_up != old_ms->button_up))
	{
		self->int_flags &= ~control_clicked;
	}

	// TODO : Move these to new methods.

	if (!mouse_handled)
	{
		// Check for moving and resizing.
		if ((self->int_flags & control_resizing_left)
		 || (self->int_flags & control_resizing_right)
		 || (self->int_flags & control_resizing_top)
		 || (self->int_flags & control_resizing_bottom))
		{
			// These can be combined to grab the corners for resizing.

			// Resize by width.
			if (self->int_flags & control_resizing_left)
			{
				EZ_control_ResizeByDirection(self, ms, mouse_delta_x, mouse_delta_y, RESIZE_LEFT);
				mouse_handled = true;
			}
			else if (self->int_flags & control_resizing_right)
			{
				EZ_control_ResizeByDirection(self, ms, mouse_delta_x, mouse_delta_y, RESIZE_RIGHT);
				mouse_handled = true;
			}

			// Resize by height.
			if (self->int_flags & control_resizing_top)
			{
				EZ_control_ResizeByDirection(self, ms, mouse_delta_x, mouse_delta_y, RESIZE_UP);
				mouse_handled = true;
			}
			else if (self->int_flags & control_resizing_bottom)
			{
				EZ_control_ResizeByDirection(self, ms, mouse_delta_x, mouse_delta_y, RESIZE_DOWN);
				mouse_handled = true;
			}
		}
		else if (self->int_flags & control_moving)
		{
			// Root control will be moved relative to the screen,
			// others relative to their parent.
			int x = self->x + Q_rint(ms->x - ms->x_old);
			int y = self->y + Q_rint(ms->y - ms->y_old);

			// Should the control be contained within it's parent?
			// Then don't allow the mouse to move outside the parent
			// while moving the control.
			if (CONTROL_IS_CONTAINED(self))
			{
				if (MOUSE_OUTSIDE_PARENT_X(self, ms))
				{
					ms->x = ms->x_old;
					x = self->x;
					mouse_handled = true;
				}

				if (MOUSE_OUTSIDE_PARENT_Y(self, ms))
				{
					ms->y = ms->y_old;
					y = self->y;
					mouse_handled = true;
				}
			}

			EZ_control_SetPosition(self, x, y);
		}
	}

	// Let any event handler run if the mouse event wasn't handled.
	CONTROL_EVENT_HANDLER_CALL(&mouse_handled_tmp, self, ez_control_t, OnMouseEvent, ms);
	mouse_handled = (mouse_handled || mouse_handled_tmp);

	// Save the mouse state for the next time we check.
	self->prev_mouse_state = *ms;

	return mouse_handled;
}

//
// Control - The mouse was pressed and then released within the bounds of the control.
//
int EZ_control_OnMouseClick(ez_control_t *self, mouse_state_t *mouse_state)
{
	int mouse_handled = false;
	CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, ez_control_t, OnMouseClick, mouse_state);
	return mouse_handled;
}

//
// Control - The mouse entered the controls bounds.
//
int EZ_control_OnMouseEnter(ez_control_t *self, mouse_state_t *mouse_state)
{
	int mouse_handled = false;
	self->int_flags |= control_mouse_over;
	CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, ez_control_t, OnMouseEnter, mouse_state);
	return mouse_handled;
}

//
// Control - The mouse left the controls bounds.
//
int EZ_control_OnMouseLeave(ez_control_t *self, mouse_state_t *mouse_state)
{
	int mouse_handled = false;

	// Stop moving since the mouse is outside the control.
	self->int_flags &= ~(control_moving | control_mouse_over);

	CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, ez_control_t, OnMouseLeave, mouse_state);
	return mouse_handled;
}

//
// Control - A mouse button was released either inside or outside of the control.
//
int EZ_control_OnMouseUpOutside(ez_control_t *self, mouse_state_t *ms)
{
	int mouse_handled_tmp	= false;
	int mouse_handled		= false;

	// Stop moving / resizing.
	self->int_flags &= ~(control_moving | control_resizing_left | control_resizing_right | control_resizing_top | control_resizing_bottom);

	// Call event handler.
	CONTROL_EVENT_HANDLER_CALL(&mouse_handled_tmp, self, ez_control_t, OnMouseUpOutside, ms);
	mouse_handled = (mouse_handled || mouse_handled_tmp);

	return mouse_handled;
}

//
// Control - A mouse button was released within the bounds of the control.
//
int EZ_control_OnMouseUp(ez_control_t *self, mouse_state_t *mouse_state)
{
	int mouse_handled_tmp	= false;
	int mouse_handled		= false;

	// Stop moving / resizing.
	self->int_flags &= ~(control_moving | control_resizing_left | control_resizing_right | control_resizing_top | control_resizing_bottom);

	// Raise a click event.
	if (self->int_flags & control_clicked)
	{
		self->int_flags &= ~control_clicked;
		CONTROL_RAISE_EVENT(&mouse_handled_tmp, self, ez_control_t, OnMouseClick, mouse_state);
		mouse_handled = (mouse_handled || mouse_handled_tmp);
	}

	// Call event handler.
	CONTROL_EVENT_HANDLER_CALL(&mouse_handled_tmp, self, ez_control_t, OnMouseUp, mouse_state);
	mouse_handled = (mouse_handled || mouse_handled_tmp);

	return mouse_handled;
}

//
// Control - A mouse button was pressed within the bounds of the control.
//
int EZ_control_OnMouseDown(ez_control_t *self, mouse_state_t *ms)
{
	int mouse_handled_tmp	= false;
	int mouse_handled		= false;

	if (!(self->ext_flags & control_enabled))
	{
		return false;
	}

	// Make sure the current control is focused.
	if (self->control_tree->focused_node)
	{
		// If the focused node isn't this one, refocus.
		if (self->control_tree->focused_node->payload != self)
		{
			mouse_handled = EZ_control_SetFocus(self);
		}
	}
	else
	{
		// If there's no focused node, focus on this one.
		mouse_handled = EZ_control_SetFocus(self);
	}

	if (self->int_flags & control_focused)
	{
		// Check if the mouse is at the edges of the control, and turn on the correct reszie mode if it is.
		if (((self->ext_flags & control_resize_h) || (self->ext_flags & control_resize_v)) && (ms->button_down == 1))
		{
			if (self->ext_flags & control_resize_h)
			{
				// Left side of the control.
				if (POINT_IN_RECTANGLE(ms->x, ms->y,
					self->absolute_x, self->absolute_y,
					self->resize_handle_thickness, self->height))
				{
					self->int_flags |= control_resizing_left;
					mouse_handled = true;
				}

				// Right side of the control.
				if (POINT_IN_RECTANGLE(ms->x, ms->y,
					self->absolute_x + self->width - self->resize_handle_thickness, self->absolute_y,
					self->resize_handle_thickness, self->height))
				{
					self->int_flags |= control_resizing_right;
					mouse_handled = true;
				}
			}

			if (self->ext_flags & control_resize_v)
			{
				// Top of the control.
				if (POINT_IN_RECTANGLE(ms->x, ms->y,
					self->absolute_x, self->absolute_y,
					self->width, self->resize_handle_thickness))
				{
					self->int_flags |= control_resizing_top;
					mouse_handled = true;
				}

				// Bottom of the control.
				if (POINT_IN_RECTANGLE(ms->x, ms->y,
					self->absolute_x, self->absolute_y + self->height - self->resize_handle_thickness,
					self->width, self->resize_handle_thickness))
				{
					self->int_flags |= control_resizing_bottom;
					mouse_handled = true;
				}
			}
		}

		// The control is being moved.
		if ((self->ext_flags & control_movable) && (ms->button_down == 1))
		{
			self->int_flags |= control_moving;
			mouse_handled = true;
		}

		if (ms->button_down)
		{
			self->int_flags |= control_clicked;
			mouse_handled = true;
		}
	}

	/*
	TODO : Fix OnMouseClick
	if(!mouse_handled)
	{
		CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, OnMouseClick, mouse_state);
	}
	*/

	CONTROL_EVENT_HANDLER_CALL(&mouse_handled_tmp, self, ez_control_t, OnMouseDown, ms);
	mouse_handled = (mouse_handled || mouse_handled_tmp);

	return mouse_handled;
}

/*
// TODO : Add support for mouse wheel.
//
// Control - The mouse wheel was triggered within the bounds of the control.
//
int EZ_control_OnMouseWheel(ez_control_t *self, mouse_state_t *mouse_state)
{
	int wheel_handled = false;
	CONTROL_EVENT_HANDLER_CALL(&wheel_handled, self, ez_control_t, OnMouseWheel, mouse_state);
	return wheel_handled;
}
*/

//
// Control - The mouse is hovering within the bounds of the control.
//
int EZ_control_OnMouseHover(ez_control_t *self, mouse_state_t *mouse_state)
{
	int mouse_handled = false;

	if (!mouse_handled)
	{
		CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, ez_control_t, OnMouseHover, mouse_state);
	}

	return mouse_handled;
}

// =========================================================================================
// Label
// =========================================================================================

// TODO : Enable making a label read only / non-selectable text.

//
// Label - Creates a label control and initializes it.
//
ez_label_t *EZ_label_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  char *background_name,
							  ez_control_flags_t flags, ez_label_flags_t text_flags,
							  char *text)
{
	ez_label_t *label = NULL;

	// We have to have a tree to add the control to.
	if (!tree)
	{
		return NULL;
	}

	label = (ez_label_t *)Q_malloc(sizeof(ez_label_t));
	memset(label, 0, sizeof(ez_label_t));

	EZ_label_Init(label, tree, parent, name, description, x, y, width, height, background_name, flags, text_flags, text);
	
	return label;
}

//
// Label - Initializes a label control.
//
void EZ_label_Init(ez_label_t *label, ez_tree_t *tree, ez_control_t *parent,
				  char *name, char *description,
				  int x, int y, int width, int height,
				  char *background_name,
				  ez_control_flags_t flags, ez_label_flags_t text_flags,
				  char *text)
{
	// Initialize the inherited class first.
	EZ_control_Init(&label->super, tree, parent, name, description, x, y, width, height, background_name, flags);

	label->super.CLASS_ID				= EZ_LABEL_ID;

	// Overriden events.
	label->super.inheritance_level					= EZ_LABEL_INHERITANCE_LEVEL;
	((ez_control_t *)label)->events.OnDraw			= EZ_label_OnDraw;
	((ez_control_t *)label)->events.OnKeyDown		= EZ_label_OnKeyDown;
	((ez_control_t *)label)->events.OnKeyUp			= EZ_label_OnKeyUp;
	((ez_control_t *)label)->events.OnMouseDown		= EZ_label_OnMouseDown;
	((ez_control_t *)label)->events.OnMouseUp		= EZ_label_OnMouseUp;
	((ez_control_t *)label)->events.OnMouseHover	= EZ_label_OnMouseHover;
	((ez_control_t *)label)->events.OnResize		= EZ_label_OnResize;
	((ez_control_t *)label)->events.OnDestroy		= EZ_label_Destroy;

	// Label specific events.
	label->inheritance_level						= 0;
	label->events.OnTextChanged						= EZ_label_OnTextChanged;
	label->events.OnCaretMoved						= EZ_label_OnCaretMoved;
	label->events.OnTextScaleChanged				= EZ_label_OnTextScaleChanged;
	label->events.OnTextFlagsChanged				= EZ_label_OnTextFlagsChanged;

	((ez_control_t *)label)->ext_flags				|= control_contained;

	EZ_label_SetTextFlags(label, text_flags | label_selectable);
	EZ_label_SetTextScale(label, 1.0);
	EZ_label_DeselectText(label);

	if (text)
	{
		EZ_label_SetText(label, text);
	}
}

//
// Label - Destroys a label control.
//
int EZ_label_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_label_t *label = (ez_label_t *)self;

	Q_free(label->text);

	// FIXME: Can we just free a part like this here? How about children, will they be properly destroyed?
	return EZ_control_Destroy(&label->super, destroy_children);
}

//
// Label - Sets the event handler for the OnTextChanged event.
//
void EZ_label_SetOnTextChanged(ez_label_t *label, ez_control_handler_fp OnTextChanged)
{
	label->event_handlers.OnTextChanged = OnTextChanged;
}

//
// Label - Sets the event handler for the OnTextScaleChanged event.
//
void EZ_label_SetOnTextScaleChanged(ez_label_t *label, ez_control_handler_fp OnTextScaleChanged)
{
	label->event_handlers.OnTextScaleChanged = OnTextScaleChanged;
}

//
// Label - Sets the event handler for the OnCaretMoved event.
//
void EZ_label_SetOnTextOnCaretMoved(ez_label_t *label, ez_control_handler_fp OnCaretMoved)
{
	label->event_handlers.OnCaretMoved = OnCaretMoved;
}

//
// Label - Calculates where in the label text that the wordwraps will be done.
//
static void EZ_label_CalculateWordwraps(ez_label_t *label)
{
	int i					= 0;
	int current_index		= -1;
	int last_index			= -1;
	int current_col			= 0;
	int scaled_char_size	= label->scaled_char_size;

	label->num_rows			= 1;
	label->num_cols			= 0;

	if (label->text && (label->ext_flags & label_wraptext))
	{	
		// Wordwrap the string to the virtual size of the control and save the
		// indexes where each row ends in an array.
		while ((i < LABEL_MAX_WRAPS) && Util_GetNextWordwrapString(label->text, NULL, current_index + 1, &current_index, LABEL_LINE_SIZE, label->super.virtual_width, scaled_char_size))
		{
			label->wordwraps[i].index = current_index;
			label->wordwraps[i].col = current_index - last_index;
			label->wordwraps[i].row = label->num_rows - 1;
			i++;

			// Find the number of rows and columns.
			label->num_cols = max(label->num_cols, label->wordwraps[i].col);
			label->num_rows++;

			last_index = current_index;
		}
	}
	else if (label->text)
	{
		// Normal non-wrapped text, still save new line locations.

		while ((i < LABEL_MAX_WRAPS) && label->text[current_index])
		{
			if (label->text[current_index] == '\n')
			{
				label->wordwraps[i].index = current_index;
				label->wordwraps[i].col = current_index - last_index;
				label->wordwraps[i].row = label->num_rows - 1;
				i++;

				// Find the number of rows and columns.
				label->num_cols = max(label->num_cols, label->wordwraps[i].col);
				label->num_rows++;
			}
			
			current_index++;
		}
	}

	// Save the row/col information for the last row also.
	if (label->text)
	{
		label->wordwraps[i].index	= -1;
		label->wordwraps[i].row		= label->num_rows - 1;
		label->wordwraps[i].col		= label->text_length - label->wordwraps[max(0, i - 1)].index;
		label->num_cols				= max(label->num_cols, label->wordwraps[i].col);
	}
	else
	{
		// No text.
		label->wordwraps[0].index	= -1;
		label->wordwraps[0].row		= -1;
		label->wordwraps[0].col		= -1;
		label->num_cols				= 0;
		label->num_rows				= 0;
	}

	// Change the virtual height of the control to fit the text when wrapping.
	if (label->ext_flags & label_wraptext)
	{
		EZ_control_SetMinVirtualSize((ez_control_t *)label, label->super.virtual_width_min, scaled_char_size * (label->num_rows + 1));
	}
	else
	{
		EZ_control_SetMinVirtualSize((ez_control_t *)label, scaled_char_size * (label->num_cols + 1), scaled_char_size * (label->num_rows + 1));
	}
}

//
// Label - Sets if the label should use the large charset or the normal one.
//
void EZ_label_SetLargeFont(ez_label_t *label, qbool large_font)
{
	SET_FLAG(label->ext_flags, label_largefont, large_font);
	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextFlagsChanged);
}

//
// Label - Sets if the label should autosize itself to fit the text in the label.
//
void EZ_label_SetAutoSize(ez_label_t *label, qbool auto_size)
{
	SET_FLAG(label->ext_flags, label_autosize, auto_size);
	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextFlagsChanged);
}

//
// Label - Sets if the label should automatically add "..." at the end of the string when it doesn't fit in the label.
//
void EZ_label_SetAutoEllipsis(ez_label_t *label, qbool auto_ellipsis)
{
	SET_FLAG(label->ext_flags, label_autoellipsis, auto_ellipsis);
	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextFlagsChanged);
}

//
// Label - Sets if the text in the label should wrap to fit the width of the label.
//
void EZ_label_SetWrapText(ez_label_t *label, qbool wrap_text)
{
	SET_FLAG(label->ext_flags, label_wraptext, wrap_text);
	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextFlagsChanged);
}

//
// Label - Sets if the text in the label should be selectable.
//
void EZ_label_SetTextSelectable(ez_label_t *label, qbool selectable)
{
	SET_FLAG(label->ext_flags, label_selectable, selectable);
	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextFlagsChanged);
}

//
// Label - Sets if the label should be read only.
//
void EZ_label_SetReadOnly(ez_label_t *label, qbool read_only)
{
	SET_FLAG(label->ext_flags, label_readonly, read_only);
	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextFlagsChanged);
}

//
// Label - Sets the text flags for the label.
//
void EZ_label_SetTextFlags(ez_label_t *label, ez_label_flags_t flags)
{
	label->ext_flags = flags;
	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextFlagsChanged);
}

//
// Label - Gets the text flags for the label.
//
ez_label_flags_t EZ_label_GetTextFlags(ez_label_t *label)
{
	return label->ext_flags;
}

//
// Label - Set the text scale for the label.
//
void EZ_label_SetTextScale(ez_label_t *label, float scale)
{
	label->scale = scale;

	CONTROL_RAISE_EVENT(NULL, label, ez_control_t, OnTextScaleChanged);
}

//
// Label - Sets the text color of a label.
//
void EZ_label_SetTextColor(ez_label_t *label, byte r, byte g, byte b, byte alpha)
{
	label->color.c = RGBA_TO_COLOR(r, g, b, alpha);
}

//
// Label - Hides the caret.
//
void EZ_label_HideCaret(ez_label_t *label)
{
	label->caret_pos.index = -1;
}

//
// Label - Deselects the text in the label.
//
void EZ_label_DeselectText(ez_label_t *label)
{
	label->int_flags	&= ~label_selecting;
	label->select_start = -1;
	label->select_end	= -1;
}

//
// Label - Gets the size of the selected text.
//
int EZ_label_GetSelectedTextSize(ez_label_t *label)
{
	if ((label->select_start > -1) && (label->select_end > -1))
	{
		return abs(label->select_end - label->select_start) + 1;
	}

	return 0;
}

//
// Label - Gets the selected text.
//
void EZ_label_GetSelectedText(ez_label_t *label, char *target, int target_size)
{
	if ((label->select_start > -1) && (label->select_end > -1))
	{
		int sublen = abs(label->select_end - label->select_start) + 1;
		
		if (sublen > 0)
		{
			snprintf(target, min(sublen, target_size), "%s", label->text + min(label->select_start, label->select_end));
		}
	}
}

//
// Label - Appends text at the given position in the text.
//
void EZ_label_AppendText(ez_label_t *label, int position, const char *append_text)
{
	int text_len		= label->text_length;
	int append_text_len	= strlen(append_text);

	clamp(position, 0, text_len);

	if (!label->text)
	{
		return;
	}

	// Reallocate the string to fit the new text.
	label->text = (char *)Q_realloc((void *)label->text, (text_len + append_text_len + 1) * sizeof(char));

	// Move the part of the string after the specified position.
	memmove(
		label->text + position + append_text_len,		// Destination, make room for the append text.
		label->text + position,							// Move everything after the specified position.
		(text_len + 1 - position) * sizeof(char));

	// Copy the append text into the newly created space in the string.
	memcpy(label->text + position, append_text, append_text_len * sizeof(char));

	CONTROL_RAISE_EVENT(NULL, label, ez_control_t, OnTextChanged);
}

//
// Label - Removes text between two given indexes.
//
void EZ_label_RemoveText(ez_label_t *label, int start_index, int end_index)
{
	int text_len = label->text_length;
	clamp(start_index, 0, text_len);
	clamp(end_index, start_index, text_len);

	if (!label->text)
	{
		return;
	}

	memmove(
		label->text + start_index,					// Destination.
		label->text + end_index,					// Source.
		(text_len - end_index + 1) * sizeof(char));	// Size.
	
	label->text = Q_realloc(label->text, (strlen(label->text) + 1) * sizeof(char));

	CONTROL_RAISE_EVENT(NULL, label, ez_control_t, OnTextChanged);
}

//
// Label - Sets the text of a label.
//
void EZ_label_SetText(ez_label_t *label, const char *text)
{
	int text_len = strlen(text) + 1;

	Q_free(label->text);

	if (text)
	{
		label->text = Q_calloc(text_len, sizeof(char));
		strlcpy(label->text, text, text_len);
	}

	CONTROL_RAISE_EVENT(NULL, label, ez_control_t, OnTextChanged);
}

//
// Label - The text flags for the label changed.
//
int EZ_label_OnTextFlagsChanged(ez_control_t *self)
{
	ez_label_t *label = (ez_label_t *)self;

	// Deselect anything selected and hide the caret.
	if (!(label->ext_flags & label_selectable))
	{
		EZ_label_HideCaret(label);
		EZ_label_DeselectText(label);
	}
	
	// Make sure we recalculate the char size if we changed to large font.
	if (label->ext_flags & label_largefont)
	{
		CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextScaleChanged);
	}

	// The label will autosize on a text change, so trigger an event for that.
	if (label->ext_flags & label_autosize)
	{
		CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextChanged);
	}

	// Recalculate the line ends.
	if (label->ext_flags & label_wraptext)
	{
		CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnResize);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, label, ez_label_t, OnTextFlagsChanged);

	return 0;
}

//
// Label - The scale of the text changed.
//
int EZ_label_OnTextScaleChanged(ez_control_t *self)
{
	ez_label_t *label		= (ez_label_t *)self;
	int char_size			= (label->ext_flags & label_largefont) ? 64 : 8;

	label->scaled_char_size	= Q_rint(char_size * label->scale);
	
	// We need to recalculate the wordwrap stuff since the size changed.
	EZ_label_CalculateWordwraps(label);

	CONTROL_EVENT_HANDLER_CALL(NULL, label, ez_control_t, OnTextScaleChanged);

	return 0;
}

//
// Label - Happens when the control has resized.
//
int EZ_label_OnResize(ez_control_t *self)
{
	ez_label_t *label = (ez_label_t *)self;
	
	// Let the super class try first.
	EZ_control_OnResize(self);

	// Calculate where to wrap the text.
	EZ_label_CalculateWordwraps(label);

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnResize);

	return 0;
}

//
// Label - Draws a label control.
//
int EZ_label_OnDraw(ez_control_t *self)
{
	int x, y, i = 0;
	char line[LABEL_LINE_SIZE];
	ez_label_t *label		= (ez_label_t *)self;
	int scaled_char_size	= label->scaled_char_size;
	int last_index			= -1;
	int curr_row			= 0;
	int curr_col			= 0;
	clrinfo_t text_color	= {RGBA_TO_COLOR(255, 255, 255, 255), 0}; // TODO : Set this in the struct instead.
	color_t selection_color	= RGBA_TO_COLOR(178, 0, 255, 125);
	color_t caret_color		= RGBA_TO_COLOR(255, 0, 0, 125);

	// Let the super class draw first.
	EZ_control_OnDraw(self);

	// Get the position we're drawing at.
	EZ_control_GetDrawingPosition(self, &x, &y);

	// Find any newlines and draw line by line.
	for (i = 0; i <= label->text_length; i++)
	{
		// Draw selection markers.
		{
			if (((label->select_start > -1) && (label->select_end > -1)	// Is something selected at all?
				&& (label->select_end != label->select_start))			// Only highlight if we have at least 1 char selected.
				&&
					(
					((i >= label->select_start) && (i < label->select_end))
					|| ((i >= label->select_end) && (i < label->select_start))
					)
				)
			{
				// If this is one of the selected letters, draw a selection thingie behind it.
				// TODO : Make this an XOR drawing ontop of the text instead?
				Draw_AlphaFillRGB(
					x + (curr_col * scaled_char_size), 
					y + (curr_row * scaled_char_size), 
					scaled_char_size, scaled_char_size, 
					selection_color);
			}

			if (i == label->caret_pos.index)
			{
				// Draw the caret.
				Draw_AlphaFillRGB(
					x + (curr_col * scaled_char_size), 
					y + (curr_row * scaled_char_size), 
					max(5, Q_rint(scaled_char_size * 0.1)), scaled_char_size, 
					caret_color);
			}
		}

		if (i == label->wordwraps[curr_row].index || label->text[i] == '\0')
		{
			// We found the end of a line, copy the contents of the line to the line buffer.
			snprintf(line, min(LABEL_LINE_SIZE, (i - last_index) + 1), "%s", (label->text + last_index + 1));
			last_index = i;	// Skip the newline character

			if (label->ext_flags & label_largefont)
			{
				Draw_BigString(x, y + (curr_row * scaled_char_size), line, &text_color, 1, label->scale, 1, 0);
			}
			else
			{
				Draw_SColoredString(x, y + (curr_row * scaled_char_size), str2wcs(line), &text_color, 1, false, label->scale);
			}

			curr_row++;
			curr_col = 0;
		}
		else
		{
			curr_col++;
		}
	}

	// TODO : Remove this test stuff.
	{
		char tmp[1024];

		int sublen = abs(label->select_end - label->select_start);
		if (sublen > 0)
		{
			snprintf(tmp, sublen + 1, "%s", label->text + min(label->select_start, label->select_end));
			Draw_String(x + (self->width / 2), y + (self->height / 2) + 8, tmp);
		}
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDraw);

	return 0;
}

//
// Label - Finds the row and column for a specified index. The proper wordwrap indexes must've been calculated first.
//
static void EZ_label_FindRowColumnByIndex(ez_label_t *label, int index, int *row, int *column)
{
	int i = 0;

	// Find the row the index is on.
	if (row)
	{
		// Find the wordwrap that's closest to the index but still larger
		// that will be the end of the row that the index is on.
		while ((label->wordwraps[i].index != -1) && (label->wordwraps[i].index < index))
		{
			i++;
		}

		// Save the row. (The last row as -1 as index, but still has the correct row saved).
		(*row) = label->wordwraps[i].row;
	}

	if (column)
	{
		// We don't have the last rows end index saved explicitly, so get it here.
		int row_end_index = (label->wordwraps[i].index < 0) ? strlen(label->text) : label->wordwraps[i].index;

		// The column at the end of the row minus the difference between
		// the given index and the index at the end of the row gives us
		// what column the index is in.
		(*column) = label->wordwraps[i].col - (row_end_index - index);
	}
}

//
// Label - Finds the index in the label text at the specified row and column.
//
static int EZ_label_FindIndexByRowColumn(ez_label_t *label, int row, int column)
{
	int row_end_index = 0;

	// Make sure we don't do anythin stupid.
	if ((row < 0) || (row > (label->num_rows - 1)) || (column < 0)) // || (label->wordwraps[row].index < 0))
	{
		return -1;
	}

	// We don't have the last rows end index saved explicitly, so get it here.
	row_end_index = (label->wordwraps[row].index < 0) ? strlen(label->text) : label->wordwraps[row].index;

	if (column > label->wordwraps[row].col)
	{
		// There is no such column on the specified row
		// return the index of the last character on that row instead.
		return row_end_index;
	}

	// Return the index of the specified column.
	return  row_end_index - (label->wordwraps[row].col - column);
}

//
// Label - Finds at what text index the mouse was clicked.
//
static int EZ_label_FindMouseTextIndex(ez_label_t *label, mouse_state_t *ms)
{
	// TODO : Support big font gaps?
	int x, y;
	int row, col;

	// Get the position we're drawing at.
	EZ_control_GetDrawingPosition((ez_control_t *)label, &x, &y);

	// Find the row and column where the mouse is.
	row = ((ms->y - y) / label->scaled_char_size);
	col = ((ms->x - x) / label->scaled_char_size);

	// Give the last index if the mouse is past the last row.
	if (row > (label->num_rows - 1))
	{
		return label->text_length;
	}

	return EZ_label_FindIndexByRowColumn(label, row, col) + 1;
}

//
// Label - Set the caret position.
//
void EZ_label_SetCaretPosition(ez_label_t *label, int caret_pos)
{
	if (label->ext_flags & label_selectable)
	{
		label->caret_pos.index = caret_pos;
		clamp(label->caret_pos.index, -1, label->text_length);
	}
	else
	{
		label->caret_pos.index = -1;
		EZ_label_DeselectText(label);
	}

	CONTROL_RAISE_EVENT(NULL, label, ez_control_t, OnCaretMoved);
}

//
// Label - Moves the caret up or down.
//
static void EZ_label_MoveCaretVertically(ez_label_t *label, int amount)
{
	// Find the index of the character above/below that position,
	// and set the caret to that position, if there is no char
	// exactly above that point, jump to the end of the line
	// of the previous/next row instead.

	int new_caret_index = EZ_label_FindIndexByRowColumn(label, max(0, label->caret_pos.row + amount), label->caret_pos.col);
	
	if (new_caret_index >= 0)
	{
		EZ_label_SetCaretPosition(label, new_caret_index);
	}
}

//
// Label - The caret was moved.
//
int EZ_label_OnCaretMoved(ez_control_t *self)
{
	ez_label_t *label		= (ez_label_t *)self;
	int scaled_char_size	= label->scaled_char_size;
	int row_visible_start	= 0;
	int row_visible_end		= 0;
	int	col_visible_start	= 0;
	int col_visible_end		= 0;
	int prev_caret_row		= label->caret_pos.row;
	int prev_caret_col		= label->caret_pos.col;
	int new_scroll_x		= self->virtual_x;
	int new_scroll_y		= self->virtual_y;
	int num_visible_rows	= Q_rint((float)self->height / scaled_char_size - 1);	// The number of currently visible rows.
	int num_visible_cols	= Q_rint((float)self->width / scaled_char_size - 1);

	// Find the new row and column of the caret.
	EZ_label_FindRowColumnByIndex(label, label->caret_pos.index, &label->caret_pos.row, &label->caret_pos.col);

	row_visible_start	= (self->virtual_y / scaled_char_size);
	row_visible_end		= (row_visible_start + num_visible_rows);

	// Vertical scrolling.
	if (label->caret_pos.row <= row_visible_start)
	{
		// Caret at the top of the visible part of the text.
		new_scroll_y = label->caret_pos.row * scaled_char_size;
	}
	else if (prev_caret_row >= label->caret_pos.row)
	{
		// Caret in the middle.
	}
	else if (label->caret_pos.row >= (row_visible_end - 1))
	{
		// Caret at the bottom of the visible part.
		new_scroll_y = (label->caret_pos.row - num_visible_rows) * scaled_char_size;

		// Special case if it's the last line. Make sure we're allowed to
		// see the entire row, by scrolling a bit extra at the end.
		if ((label->caret_pos.row == (label->num_rows - 1)) && (new_scroll_y < label->super.virtual_height_min))
		{
			new_scroll_y += Q_rint(0.2 * scaled_char_size);
		}
	}

	// Only scroll horizontally if the text is not wrapped.
	if (!(label->ext_flags & label_wraptext))
	{
		col_visible_start	= (self->virtual_x / scaled_char_size);
		col_visible_end		= (col_visible_start + num_visible_cols);

		if (label->caret_pos.col <= (col_visible_start + 1))
		{
			// At the start of the visible text (columns).
			new_scroll_x = (label->caret_pos.col - 1) * scaled_char_size;
		}
		else if (prev_caret_col >= label->caret_pos.col)
		{
			// In the middle of the visible text.
		}
		else if (label->caret_pos.col >= (col_visible_end - 1))
		{
			// At the end of the visible text.
			new_scroll_x = (label->caret_pos.col - num_visible_cols) * scaled_char_size;
		}
	}

	// Only set a new scroll if it changed.
	if ((new_scroll_x != self->virtual_x) || (new_scroll_y != self->virtual_y))
	{
		EZ_control_SetScrollPosition(self, new_scroll_x, new_scroll_y);
	}

	// Select while holding down shift.
	if (isShiftDown() && (label->select_start > -1))
	{
		label->int_flags |= label_selecting;
		label->select_end = label->caret_pos.index;
	}

	// Deselect any selected text if we're not in selection mode.
	if (!(label->int_flags & label_selecting))
	{
		EZ_label_DeselectText(label);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, label, ez_label_t, OnCaretMoved);

	return 0;
}

//
// Label - The text changed in the label.
//
int EZ_label_OnTextChanged(ez_control_t *self)
{
	ez_label_t *label = (ez_label_t *)self;

	// Make sure we have the correct text length saved.
	label->text_length = label->text ? strlen(label->text) : 0;

	// Find the new places to wrap on.
	EZ_label_CalculateWordwraps(label);

	if (label->ext_flags & label_autosize)
	{
		// Only resize after the number of rows if we're wrapping the text.
		if (label->ext_flags & label_wraptext)
		{
			EZ_control_SetSize(self, self->width, (label->scaled_char_size * label->num_rows));
		}
		else
		{
			EZ_control_SetSize(self, (label->scaled_char_size * label->num_cols), (label->scaled_char_size * label->num_rows));
		}
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, label, ez_label_t, OnTextChanged);

	return 0;
}

//
// Label - Handle a page up/dn key press.
//
static void EZ_label_PageUpDnKeyDown(ez_label_t *label, int key)
{
	int num_visible_rows = Q_rint((float)label->super.height / label->scaled_char_size);

	switch (key)
	{
		case K_PGUP :
		{
			EZ_label_MoveCaretVertically(label, -num_visible_rows);
			break;
		}
		case K_PGDN :
		{
			EZ_label_MoveCaretVertically(label, num_visible_rows);
			break;
		}
		default :
		{
			break;
		}
	}
}

//
// Label - Handle a arrow key press.
//
static void EZ_label_ArrowKeyDown(ez_label_t *label, int key)
{
	switch(key)
	{
		case K_LEFTARROW :
		{
			EZ_label_SetCaretPosition(label, max(0, label->caret_pos.index - 1));
			break;
		}
		case K_RIGHTARROW :
		{
			EZ_label_SetCaretPosition(label, label->caret_pos.index + 1);
			break;
		}
		case K_UPARROW :
		{
			EZ_label_MoveCaretVertically(label, -1);
			break;
		}
		case K_DOWNARROW :
		{
			EZ_label_MoveCaretVertically(label, 1);
			break;
		}
		default :
		{
			break;
		}
	}
}

//
// Label - Handle a HOME/END key press.
//
static void EZ_label_EndHomeKeyDown(ez_label_t *label, int key)
{
	switch(key)
	{
		case K_HOME :
		{
			if (isCtrlDown())
			{
				// Move the caret to the start of the text.
				EZ_label_SetCaretPosition(label, 0);
			}
			else
			{
				// Move the caret to the start of the current line.
				EZ_label_SetCaretPosition(label, label->caret_pos.index - label->caret_pos.col + 1);
			}
			break;
		}
		case K_END :
		{
			if (isCtrlDown())
			{
				// Move the caret to the end of the text.
				EZ_label_SetCaretPosition(label, label->text_length);
			}
			else
			{
				// Move the caret to the end of the line.
				int i = 0;
				int line_end_index = 0;

				// Find the last index of the current row.
				while ((label->wordwraps[i].index > 0) && (label->wordwraps[i].index < label->caret_pos.index))
				{
					i++;
				}

				// Special case for last line.
				line_end_index = (label->wordwraps[i].index < 0) ? label->text_length : label->wordwraps[i].index;
				EZ_label_SetCaretPosition(label, line_end_index);
			}
			break;
		}
		default :
		{
			break;
		}
	}
}

//
// Label - Handle backspace/delete key presses.
//
static void EZ_label_BackspaceDeleteKeyDown(ez_label_t *label, int key)
{
	// Don't allow deleting if read only.
	if (label->ext_flags & label_readonly)
	{
		return;
	}

	if (LABEL_TEXT_SELECTED(label))
	{
		// Find the start and end of the selected text.
		int start = (label->select_start < label->select_end) ? label->select_start : label->select_end;
		int end	  = (label->select_start < label->select_end) ? label->select_end : label->select_start;

		// Remove a selected chunk of text.				
		EZ_label_RemoveText(label, start, end);
		
		// When deleting we need to move the cursor so that
		// it's at the "same point" in the text afterwards.
		if (label->select_start < label->select_end)
		{
			EZ_label_SetCaretPosition(label, label->select_start);
		}

		// Deselect what we just deleted.
		EZ_label_DeselectText(label);
	}
	else if (key == K_BACKSPACE)
	{
		// Remove a single character.
		EZ_label_RemoveText(label, label->caret_pos.index - 1, label->caret_pos.index);
		EZ_label_SetCaretPosition(label, label->caret_pos.index - 1);
	}
	else if (key == K_DEL)
	{
		EZ_label_RemoveText(label, label->caret_pos.index, label->caret_pos.index + 1);
	}
}

//
// Label - Handles Ctrl combination key presses.
//
static void EZ_label_CtrlComboKeyDown(ez_label_t *label, int key)
{
	char c = (char)key;

	switch (c)
	{
		case 'c' :
		case 'C' :
		{
			if (LABEL_TEXT_SELECTED(label))
			{
				// CTRL + C (Copy to clipboard).
				int selected_len = EZ_label_GetSelectedTextSize(label) + 1;
				char *selected_text = (char *)Q_malloc(selected_len * sizeof(char));

				EZ_label_GetSelectedText(label, selected_text, selected_len);

				CopyToClipboard(selected_text);
				break;
			}
		}
		case 'v' :
		case 'V' :
		{
			// CTRL + V (Paste from clipboard).
			char *pasted_text = ReadFromClipboard();
			EZ_label_AppendText(label, label->caret_pos.index, pasted_text);
			break;
		}
		case 'a' :
		case 'A' :
		{
			// CTRL + A (Select all).
			label->select_start = 0;
			label->select_end	= label->text_length;
			label->int_flags	|= label_selecting;
			EZ_label_SetCaretPosition(label, label->select_end);
			label->int_flags	&= ~label_selecting;
			break;
		}
		default :
		{
			break;
		}
	}
}

//
// Label - Handles input key was presses.
//
static void EZ_label_InputKeyDown(ez_label_t *label, int key)
{
	char c = (char)key;

	// Don't allow input if read only.
	if (label->ext_flags & label_readonly)
	{
		return;
	}

	// User typing text into the label.
	// TODO : Add support for input of all quake chars
	if ((label->caret_pos.index >= 0) 
		&& (	
				((c >= 'a') && (c <= 'z'))
				||	
				((c >= 'A') && (c <= 'Z'))
			)
		)
	{
		char char_str[2];				

		// First remove any selected text.
		if (LABEL_TEXT_SELECTED(label))
		{
			int start = (label->select_start < label->select_end) ? label->select_start : label->select_end;
			int end	  = (label->select_start < label->select_end) ? label->select_end : label->select_start;

			EZ_label_RemoveText(label, start, end);

			// Make sure the caret stays in the same place.
			EZ_label_SetCaretPosition(label, (label->select_start < label->select_end) ? label->select_start : label->select_end);
		}

		// Turn off selecting mode, since we don't want to be selecting
		// when typing capital letters for instance.
		EZ_label_DeselectText(label);

		// Add the text.
		snprintf(char_str, 2, "%c", (char)key);
		EZ_label_AppendText(label, label->caret_pos.index, char_str);
		EZ_label_SetCaretPosition(label, label->caret_pos.index + 1);
	}
}

//
// Label - Key down event.
//
int EZ_label_OnKeyDown(ez_control_t *self, int key, int unichar)
{
	ez_label_t *label	= (ez_label_t *)self;
	int key_handled		= false;

	key_handled = EZ_control_OnKeyDown(self, key, unichar);

	if (key_handled)
	{
		return true;
	}

	key_handled = true;

	// Only handle keys if text is selectable.
	if (label->ext_flags & label_selectable)
	{
		// Key down.
		switch (key)
		{
			case K_LEFTARROW :
			case K_RIGHTARROW :
			case K_UPARROW :
			case K_DOWNARROW :
			{
				EZ_label_ArrowKeyDown(label, key);
				break;
			}
			case K_PGDN :
			case K_PGUP :
			{
				EZ_label_PageUpDnKeyDown(label, key);
				break;
			}
			case K_HOME :
			case K_END :
			{
				EZ_label_EndHomeKeyDown(label, key);
				break;
			}
			case K_TAB :
			{
				break;
			}
			case K_LSHIFT :
			case K_RSHIFT :
			case K_SHIFT :
			{
				// Start selecting.
				label->select_start = label->caret_pos.index;
				label->int_flags |= label_selecting;
				break;
			}
			case K_RCTRL :
			case K_LCTRL :
			case K_CTRL :
			{
				break;
			}
			case K_DEL :
			case K_BACKSPACE :
			{
				EZ_label_BackspaceDeleteKeyDown(label, key);
				break;
			}
			default :
			{
				if (isCtrlDown())
				{
					EZ_label_CtrlComboKeyDown(label, key);
				}
				else
				{
					EZ_label_InputKeyDown(label, key);
				}
				break;
			}
		}
	}

	CONTROL_EVENT_HANDLER_CALL(&key_handled, self, ez_control_t, OnKeyDown, key, unichar);
	return key_handled;
}

//
// Label - Key up event.
//
int EZ_label_OnKeyUp(ez_control_t *self, int key, int unichar)
{
	ez_label_t *label	= (ez_label_t *)self;
	int key_handled		= false;

	key_handled = EZ_control_OnKeyUp(self, key, unichar);

	if (key_handled)
	{
		return true;
	}

	key_handled = true;

	// Key up.
	switch (key)
	{
		case K_SHIFT :
		{
			label->int_flags &= ~label_selecting;
			break;
		}
	}

	return key_handled;
}

//
// Label - The mouse is hovering within the bounds of the label.
//
int EZ_label_OnMouseHover(ez_control_t *self, mouse_state_t *ms)
{
	qbool mouse_handled = false;
	ez_label_t *label = (ez_label_t *)self;

	// Call the super class first.
	mouse_handled = EZ_control_OnMouseHover(self, ms);

	// Find at what index in the text where the mouse is currently over.
	if (label->int_flags & label_selecting)
	{
		label->select_end = EZ_label_FindMouseTextIndex(label, ms);
		EZ_label_SetCaretPosition(label, label->select_end);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnMouseHover, ms);

	return mouse_handled;
}

//
// Label - Handles the mouse down event.
//
int EZ_label_OnMouseDown(ez_control_t *self, mouse_state_t *ms)
{
	int mouse_handled = false;
	ez_label_t *label = (ez_label_t *)self;

	// Call the super class first.
	mouse_handled = EZ_control_OnMouseDown(self, ms);

	// Find at what index in the text the mouse was clicked.
	label->select_start = EZ_label_FindMouseTextIndex(label, ms);

	// Reset the caret and selection end since we just started a new select / caret positioning.
	label->select_end = -1;
	EZ_label_HideCaret(label);

	// We just started to select some text.
	label->int_flags |= label_selecting;

	CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, ez_control_t, OnMouseDown, ms);

	return true;
}

//
// Label - Handles the mouse up event.
//
int EZ_label_OnMouseUp(ez_control_t *self, mouse_state_t *ms)
{
	int mouse_handled = false;
	ez_label_t *label = (ez_label_t *)self;

	// Call the super class first.
	mouse_handled = EZ_control_OnMouseUp(self, ms);

	// Find at what index in the text where the mouse was released.
	if ((self->int_flags & control_focused) && (label->int_flags & label_selecting))
	{
		label->select_end = EZ_label_FindMouseTextIndex(label, ms);
		
		EZ_label_SetCaretPosition(label, label->select_end);
	}
	else
	{
		label->select_start		= -1;
		label->select_end		= -1;
		EZ_label_SetCaretPosition(label, -1);
	}

	// We've stopped selecting.
	label->int_flags &= ~label_selecting;

	CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, ez_control_t, OnMouseUp, ms);

	return mouse_handled;
}

// =========================================================================================
// Button
// =========================================================================================

// TODO : Add a label as text on the button.

//
// Button - Creates a new button and initializes it.
//
ez_button_t *EZ_button_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  char *background_name, char *hover_image, char *pressed_image,
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

	EZ_button_Init(button, tree, parent, name, description, x, y, width, height, background_name, hover_image, pressed_image, flags);
	
	return button;
}

//
// Button - Initializes a button.
//
void EZ_button_Init(ez_button_t *button, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  char *background_name, char *hover_image, char *pressed_image,
							  ez_control_flags_t flags)
{
	// Initialize the inherited class first.
	EZ_control_Init(&button->super, tree, parent, name, description, x, y, width, height, background_name, flags);

	// Initilize the button specific stuff.
	((ez_control_t *)button)->CLASS_ID			= EZ_BUTTON_ID;

	// TODO : Make a default macro for button flags.
	((ez_control_t *)button)->ext_flags			|= (flags | control_focusable | control_contained);

	// Override the draw function.
	((ez_control_t *)button)->inheritance_level = EZ_BUTTON_INHERITANCE_LEVEL;
	((ez_control_t *)button)->events.OnDraw		= EZ_button_OnDraw;

	// Button specific events.
	button->inheritance_level					= 0;
	button->events.OnAction						= EZ_button_OnAction;

	// TODO : Set proper flags / size for the label. Associate a function with the label text changing (and center it or whatever on that).
	button->text_label = EZ_label_Create(tree, parent, "Button text label", "", 0, 0, 1, 1, NULL, 0, 0, "");

	// TODO : Load button images.
}

//
// Button - Destroys the button.
//
void EZ_button_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_button_t *button = (ez_button_t *)self;

	// TODO: Cleanup button images?

	// FIXME: Can we just free a part like this here? How about children, will they be properly destroyed?
	EZ_control_Destroy(&button->super, destroy_children);
}

//
// Button - OnAction event handler.
//
int EZ_button_OnAction(ez_control_t *self)
{
	ez_button_t *button = (ez_button_t *)self;
	CONTROL_EVENT_HANDLER_CALL(NULL, button, ez_button_t, OnAction);

	return 0;
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
void EZ_button_SetOnAction(ez_button_t *self, ez_control_handler_fp OnAction)
{
	self->event_handlers.OnAction = OnAction;
}

//
// Button - OnDraw event.
//
int EZ_button_OnDraw(ez_control_t *self)
{
	int text_x = 0;
	int text_y = 0;
	int text_len = 0;
	ez_button_t *button = (ez_button_t *)self;

	int x, y;
	EZ_control_GetDrawingPosition(self, &x, &y);

	// Run the super class's implementation first.
	EZ_control_OnDraw(self);

	if (button->text)
	{
		text_len = strlen(button->text);
	}

	switch (button->text_alignment)
	{
		case top_left :
			text_x = button->padding_left;
			text_y = button->padding_top;
			break;
		case top_center :
			text_x = button->padding_left;
			text_y = button->padding_top;
			break;
		case top_right :
			text_x = self->width - (text_len * 8) - button->padding_right;
			text_y = button->padding_top;
			break;
		case middle_left :
			text_x = button->padding_left;
			//text_y = button-
			break;
		default:
			break;
		// unhandled
		case middle_center:
		case middle_right:
		case bottom_left:
		case bottom_center:
		case bottom_right:
			break;
	}

	if (self->int_flags & control_clicked)
	{
		Draw_AlphaFillRGB(x, y, self->width, self->height, RGBAVECT_TO_COLOR(button->color_pressed));
	}

	if (self->int_flags & control_mouse_over)
	{
		if (self->int_flags & control_clicked)
		{
			Draw_AlphaFillRGB(x, y, self->width, self->height, RGBAVECT_TO_COLOR(button->color_pressed));
		}
		else
		{
			Draw_AlphaFillRGB(x, y, self->width, self->height, RGBAVECT_TO_COLOR(button->color_hover));
		}
	}
	else
	{
		Draw_AlphaFillRGB(x, y, self->width, self->height, RGBAVECT_TO_COLOR(button->color_normal));
	}

	if (self->int_flags & control_focused)
	{
		Draw_AlphaRectangleRGB(x, y, self->width, self->height, 1, false, RGBAVECT_TO_COLOR(button->color_focused));
		//Draw_ColoredString3(self->absolute_x, self->absolute_y, button->text, button->focused_text_color, 1, 0);
	}

	// Draw control specifics.
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDraw);

	return 0;
}

// =========================================================================================
// Slider
// =========================================================================================

// TODO : Slider - Add key support. Show somehow that it's focused?

//
// Slider - Creates a new button and initializes it.
//
ez_slider_t *EZ_slider_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  char *background_name,
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

	EZ_slider_Init(slider, tree, parent, name, description, x, y, width, height, background_name, flags);
	
	return slider;
}

//
// Slider - Initializes a button.
//
void EZ_slider_Init(ez_slider_t *slider, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  char *background_name,
							  ez_control_flags_t flags)
{
	height = max(height, 8);

	// Initialize the inherited class first.
	EZ_control_Init(&slider->super, tree, parent, name, description, x, y, width, height, background_name, flags);

	// Initilize the button specific stuff.
	((ez_control_t *)slider)->CLASS_ID					= EZ_SLIDER_ID;
	((ez_control_t *)slider)->ext_flags					|= (flags | control_focusable | control_contained | control_resizeable);

	// Override the draw function.
	((ez_control_t *)slider)->inheritance_level			= EZ_SLIDER_INHERITANCE_LEVEL;
	((ez_control_t *)slider)->events.OnDraw				= EZ_slider_OnDraw;
	((ez_control_t *)slider)->events.OnMouseDown		= EZ_slider_OnMouseDown;
	((ez_control_t *)slider)->events.OnMouseUpOutside	= EZ_slider_OnMouseUpOutside;
	((ez_control_t *)slider)->events.OnMouseEvent		= EZ_slider_OnMouseEvent;
	((ez_control_t *)slider)->events.OnResize			= EZ_slider_OnResize;
	((ez_control_t *)slider)->events.OnKeyDown			= EZ_slider_OnKeyDown;

	// Button specific events.
	slider->inheritance_level							= 0;
	slider->events.OnSliderPositionChanged				= EZ_slider_OnSliderPositionChanged;
	slider->events.OnMaxValueChanged					= EZ_slider_OnMaxValueChanged;
	slider->events.OnMinValueChanged					= EZ_slider_OnMinValueChanged;
	slider->events.OnScaleChanged						= EZ_slider_OnScaleChanged;

	// Set default values.
	EZ_slider_SetMax(slider, 100);
	EZ_slider_SetScale(slider, 1.0);
	EZ_slider_SetPosition(slider, 0);
}

//
// Slider - Calculates the actual slider position.
//
__inline void EZ_slider_CalculateRealSliderPos(ez_slider_t *slider)
{
	int pos = slider->slider_pos - slider->min_value;

	// Calculate the real position of the slider by multiplying by the gap size between each value.
	// (Don't start drawing at the exact start cause that would overwrite the edge marker)
	slider->real_slider_pos = Q_rint((slider->scaled_char_size / 2.0) + (pos * slider->gap_size));
}

//
// Slider - Calculates the gap size between values.
//
__inline void EZ_slider_CalculateGapSize(ez_slider_t *slider)
{
	slider->range = abs(slider->max_value - slider->min_value);

	// Calculate the gap between each value in pixels (floating point so that we don't get rounding errors).
	// Don't include the first and last characters in the calculation, the slider is not allowed to move over those.
	slider->gap_size = ((float)(((ez_control_t *)slider)->width - (2 * slider->scaled_char_size)) / (float)slider->range);
}

//
// Slider - Event handler for OnSliderPositionChanged.
//
void EZ_slider_SetOnSliderPositionChanged(ez_slider_t *slider, ez_control_handler_fp OnSliderPositionChanged)
{
	slider->event_handlers.OnSliderPositionChanged = OnSliderPositionChanged;
}

//
// Slider - Event handler for OnMaxValueChanged.
//
void EZ_slider_SetOnMaxValueChanged(ez_slider_t *slider, ez_control_handler_fp OnMaxValueChanged)
{
	slider->event_handlers.OnMaxValueChanged = OnMaxValueChanged;
}

//
// Slider - Event handler for OnMinValueChanged.
//
void EZ_slider_SetOnMinValueChanged(ez_slider_t *slider, ez_control_handler_fp OnMinValueChanged)
{
	slider->event_handlers.OnMinValueChanged = OnMinValueChanged;
}

//
// Slider - Event handler for OnSliderPositionChanged.
//
void EZ_slider_SetOnScaleChanged(ez_slider_t *slider, ez_control_handler_fp OnScaleChanged)
{
	slider->event_handlers.OnScaleChanged = OnScaleChanged;
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

	CONTROL_RAISE_EVENT(NULL, slider, ez_control_t, OnSliderPositionChanged);
}

//
// Slider - Set the max slider value.
//
void EZ_slider_SetMax(ez_slider_t *slider, int max_value)
{
	slider->max_value = max_value;

	CONTROL_RAISE_EVENT(NULL, slider, ez_slider_t, OnMaxValueChanged);
}

//
// Slider - Set the max slider value.
//
void EZ_slider_SetMin(ez_slider_t *slider, int min_value)
{
	slider->min_value = min_value;

	CONTROL_RAISE_EVENT(NULL, slider, ez_slider_t, OnMinValueChanged);
}

//
// Slider - Set the scale of the slider characters.
//
void EZ_slider_SetScale(ez_slider_t *slider, float scale)
{
	slider->scale = max(0.1, scale);

	CONTROL_RAISE_EVENT(NULL, slider, ez_slider_t, OnScaleChanged);
}

//
// Slider - Scale changed.
//
int EZ_slider_OnScaleChanged(ez_control_t *self)
{
	ez_slider_t *slider = (ez_slider_t *)self;

	// Calculate the new character size.
	slider->scaled_char_size = slider->scale * 8;

	// Refit the control to fit the slider.
	EZ_control_SetSize(self, slider->super.width + slider->scaled_char_size, slider->scaled_char_size);

	CONTROL_EVENT_HANDLER_CALL(NULL, slider, ez_slider_t, OnScaleChanged);

	return 0;
}

//
// Slider - Draw function for the slider.
//
int EZ_slider_OnDraw(ez_control_t *self)
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
int EZ_slider_OnMaxValueChanged(ez_control_t *self)
{
	ez_slider_t *slider = (ez_slider_t *)self;

	// Only allow positive values greater than the min value.
	slider->max_value = max(0, max(slider->max_value, slider->min_value));

	// Calculate the gap between each slider value.
	EZ_slider_CalculateGapSize(slider);

	CONTROL_EVENT_HANDLER_CALL(NULL, slider, ez_slider_t, OnMaxValueChanged);

	return 0;
}

//
// Slider - The min value changed.
//
int EZ_slider_OnMinValueChanged(ez_control_t *self)
{
	ez_slider_t *slider = (ez_slider_t *)self;

	// Only allow positive values less than the max value.
	slider->min_value = max(0, min(slider->max_value, slider->min_value));

	// Calculate the gap between each slider value.
	EZ_slider_CalculateGapSize(slider);

	CONTROL_EVENT_HANDLER_CALL(NULL, slider, ez_slider_t, OnMinValueChanged);

	return 0;
}

//
// Slider - The slider position changed.
//
int EZ_slider_OnSliderPositionChanged(ez_control_t *self)
{
	ez_slider_t *slider = (ez_slider_t *)self;

	// Recalculate the drawing position.
	EZ_slider_CalculateRealSliderPos(slider);

	CONTROL_EVENT_HANDLER_CALL(NULL, slider, ez_slider_t, OnSliderPositionChanged);

	return 0;
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
	else if (ms->x < slider_left_edge)
	{
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
	int mouse_handled	= MOUSE_INSIDE_CONTROL(self, ms); 
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
int EZ_slider_OnResize(ez_control_t *self)
{
	ez_slider_t *slider = (ez_slider_t *)self;

	// Call super class.
	EZ_control_OnResize(self);

	EZ_slider_CalculateGapSize(slider);
	EZ_slider_CalculateRealSliderPos(slider);

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnResize);

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
	int step			= isCtrlDown() ? big_step : 1;

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


