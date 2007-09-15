
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

$Id: ez_controls.c,v 1.34 2007-09-15 21:51:11 cokeman1982 Exp $
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
	qbool contained = control->flags & CONTROL_CONTAINED;

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

		// Make sure that the control draws only within it's bounds.
		Draw_EnableScissor(payload->bound_left, payload->bound_right, payload->bound_top, payload->bound_bottom);

		// Raise the draw event for the control.
		CONTROL_RAISE_EVENT(NULL, payload, OnDraw);

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
		CONTROL_RAISE_EVENT(&mouse_handled, payload, OnMouseEvent, ms);

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
	// ez_control_t *payload = NULL;

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
			// ez_control_t *ha = (ez_control_t *)node_iter->payload;
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
qbool EZ_tree_KeyEvent(ez_tree_t *tree, int key, int unichar)
{
	int key_handled = false;

	if (!tree)
	{
		Sys_Error("EZ_tree_KeyEvent(): NULL control tree specified.\n");
	}

	if (tree->root)
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
			}
		}

		if (!key_handled)
		{
			CONTROL_RAISE_EVENT(&key_handled, tree->root, OnKeyEvent, key, unichar);
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
	if (self->flags & CONTROL_SCROLLABLE)
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
							  char *background_name, int flags)
{
	static int order = 0;

	control->CLASS_ID = EZ_CLASS_ID;

	control->control_tree = tree;
	control->name = name;
	control->description = description;
	control->flags = flags | CONTROL_ENABLED | CONTROL_VISIBLE;
	control->anchor_flags = anchor_none;

	// Default to containing a child within it's parent
	// if the parent is being contained by it's parent.
	if (parent && parent->flags & CONTROL_CONTAINED)
	{
		control->flags |= CONTROL_CONTAINED;
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
	control->events.OnMouseEnter		= EZ_control_OnMouseEnter;
	control->events.OnMouseLeave		= EZ_control_OnMouseLeave;
	control->events.OnMouseHover		= EZ_control_OnMouseHover;
	control->events.OnDestroy			= EZ_control_Destroy;
	control->events.OnDraw				= EZ_control_OnDraw;
	control->events.OnGotFocus			= EZ_control_OnGotFocus;
	control->events.OnKeyEvent			= EZ_control_OnKeyEvent;
	control->events.OnLostFocus			= EZ_control_OnLostFocus;
	control->events.OnLayoutChildren	= EZ_control_OnLayoutChildren;
	control->events.OnMove				= EZ_control_OnMove;
	control->events.OnScroll			= EZ_control_OnScroll;
	control->events.OnResize			= EZ_control_OnResize;
	control->events.OnParentResize		= EZ_control_OnParentResize;

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
	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnDestroy, destroy_children);

	iter = self->children.head;

	// Destroy the children!
	while(iter)
	{
		if(destroy_children)
		{
			// Destroy the child!
			CONTROL_RAISE_EVENT(NULL, ((ez_control_t *)iter->payload), OnDestroy, destroy_children);
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
	if(!(self->flags & CONTROL_FOCUSABLE) || !(self->flags & CONTROL_ENABLED))
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
		payload->flags &= ~CONTROL_FOCUSED;
		tree->focused_node = NULL;

		// Reset all manipulation flags.
		payload->flags &= ~(CONTROL_CLICKED | CONTROL_MOVING | CONTROL_RESIZING_LEFT | CONTROL_RESIZING_RIGHT | CONTROL_RESIZING_BOTTOM | CONTROL_RESIZING_TOP);

		// Raise event for losing the focus.
		CONTROL_RAISE_EVENT(NULL, payload, OnLostFocus);
	}

	// Set the new focus.
	self->flags |= CONTROL_FOCUSED;
	tree->focused_node = node;

	// Raise event for getting focus.
	CONTROL_RAISE_EVENT(NULL, self, OnGotFocus);

	return true;
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
	CONTROL_RAISE_EVENT(NULL, self, OnMove);
	CONTROL_RAISE_EVENT(NULL, self, OnParentResize);
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

	CONTROL_RAISE_EVENT(NULL, self, OnResize);
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
	CONTROL_RAISE_EVENT(NULL, self, OnMove);
}

//
// Control - Sets the part of the control that should be shown if it's scrollable.
//
void EZ_control_SetScrollPosition(ez_control_t *self, int scroll_x, int scroll_y)
{
	// Only scroll scrollable controls.
	if (!(self->flags & CONTROL_SCROLLABLE))
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
 
	CONTROL_RAISE_EVENT(NULL, self, OnScroll);
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
	// TODO : Raise a changed virtual size event.
	//CONTROL_RAISE_EVENT(NULL, self, OnMove);
}

//
// Control - Set the min virtual size for the control, the control size is not allowed to be larger than this.
//
void EZ_control_SetMinVirtualSize(ez_control_t *self, int min_virtual_width, int min_virtual_height)
{
	self->virtual_width_min = max(0, min_virtual_width);
	self->virtual_height_min = max(0, min_virtual_height);
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
	if(!(self->flags & CONTROL_FOCUSED))
	{
		return 0;
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnGotFocus);

	return 0;
}

//
// Control - The control lost focus.
//
int EZ_control_OnLostFocus(ez_control_t *self)
{
	if (self->flags & CONTROL_FOCUSED)
	{
		return 0;
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnLostFocus);

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
		CONTROL_RAISE_EVENT(NULL, payload, OnParentResize);
		iter = iter->next;
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnResize);

	return 0;
}

//
// Control - The controls parent was resized.
//
int EZ_control_OnParentResize(ez_control_t *self)
{
	if (self->parent && (self->flags & CONTROL_RESIZEABLE))
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

	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnParentResize);

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
	CONTROL_RAISE_EVENT(NULL, self, OnScroll);

	// Tell the children we've moved.
	while(iter)
	{
		payload = (ez_control_t *)iter->payload;
		CONTROL_RAISE_EVENT(NULL, payload, OnMove);
		iter = iter->next;
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnMove);

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
		CONTROL_RAISE_EVENT(NULL, payload, OnMove);
		iter = iter->next;
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnScroll);

	return 0;
}

//
// Control - Layouts children.
//
int EZ_control_OnLayoutChildren(ez_control_t *self)
{
	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnLayoutChildren);
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
			((self->flags & CONTROL_MOVING) ? "M" : " "),
			((self->flags & CONTROL_FOCUSED) ? "F" : " "),
			((self->flags & CONTROL_CLICKED) ? "C" : " "),
			((self->flags & CONTROL_RESIZING_LEFT) ? "R" : " ")
			));

	// Draw control specifics.
	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnDraw);

	return 0;
}

//
// Control - Key event.
//
int EZ_control_OnKeyEvent(ez_control_t *self, int key, int unichar)
{
	int key_handled = false;
	int key_handled_tmp = false;

	ez_control_t *payload = NULL;
	ez_dllist_node_t *iter = self->children.head;

	CONTROL_EVENT_HANDLER_CALL(&key_handled, self, OnKeyEvent, key, unichar);

	if (key_handled)
	{
		return true;
	}

	// Tell the children of the key event.
	while (iter)
	{
		payload = (ez_control_t *)iter->payload;
		CONTROL_RAISE_EVENT(&key_handled_tmp, payload, OnKeyEvent, key, unichar);

		key_handled = (key_handled || key_handled_tmp);

		iter = iter->next;
	}

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
int EZ_control_OnMouseEvent(ez_control_t *self, mouse_state_t *ms)
{
	mouse_state_t *old_ms = &self->prev_mouse_state;
	qbool mouse_inside = false;
	qbool prev_mouse_inside = false;
	qbool mouse_inside_parent = false;
	qbool prev_mouse_inside_parent = false;
	qbool is_contained = CONTROL_IS_CONTAINED(self);
	int mouse_handled = false;
	int mouse_delta_x = 0;
	int mouse_delta_y = 0;

	if (!ms)
	{
		Sys_Error("EZ_control_OnMouseEvent(): mouse_state_t NULL\n");
	}

	mouse_delta_x = Q_rint(ms->x_old - ms->x);
	mouse_delta_y = Q_rint(ms->y_old - ms->y);

	mouse_inside = POINT_IN_RECTANGLE(ms->x, ms->y, self->absolute_x, self->absolute_y, self->width, self->height);
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
			CONTROL_RAISE_EVENT(&mouse_handled, self, OnMouseEnter, ms);
		}
		else
		{
			// We're hovering the control.
			CONTROL_RAISE_EVENT(&mouse_handled, self, OnMouseHover, ms);
		}

		if (ms->button_down && (ms->button_down != old_ms->button_down))
		{
			// Mouse down.
			CONTROL_RAISE_EVENT(&mouse_handled, self, OnMouseDown, ms);
		}

		if (ms->button_up && (ms->button_up != old_ms->button_up))
		{
			// Mouse up.
			CONTROL_RAISE_EVENT(&mouse_handled, self, OnMouseUp, ms);
		}
	}
	else if((!is_contained && prev_mouse_inside) || (is_contained && !prev_mouse_inside_parent))
	{
		// Mouse leave.
		CONTROL_RAISE_EVENT(&mouse_handled, self, OnMouseLeave, ms);
	}

	// Make sure we remove the click flag always when releasing the button
	// not just when we're hovering above the control.
	if (ms->button_up && (ms->button_up != old_ms->button_up))
	{
		self->flags &= ~CONTROL_CLICKED;
	}

	// TODO : Move these to new methods.

	// Check for moving and resizing.
	if ((self->flags & CONTROL_RESIZING_LEFT)
	 || (self->flags & CONTROL_RESIZING_RIGHT)
	 || (self->flags & CONTROL_RESIZING_TOP)
	 || (self->flags & CONTROL_RESIZING_BOTTOM))
	{
		// These can be combined to grab the corners for resizing.

		// Resize by width.
		if (self->flags & CONTROL_RESIZING_LEFT)
		{
			EZ_control_ResizeByDirection(self, ms, mouse_delta_x, mouse_delta_y, RESIZE_LEFT);
			mouse_handled = true;
		}
		else if (self->flags & CONTROL_RESIZING_RIGHT)
		{
			EZ_control_ResizeByDirection(self, ms, mouse_delta_x, mouse_delta_y, RESIZE_RIGHT);
			mouse_handled = true;
		}

		// Resize by height.
		if (self->flags & CONTROL_RESIZING_TOP)
		{
			EZ_control_ResizeByDirection(self, ms, mouse_delta_x, mouse_delta_y, RESIZE_UP);
			mouse_handled = true;
		}
		else if (self->flags & CONTROL_RESIZING_BOTTOM)
		{
			EZ_control_ResizeByDirection(self, ms, mouse_delta_x, mouse_delta_y, RESIZE_DOWN);
			mouse_handled = true;
		}
	}
	else if (self->flags & CONTROL_MOVING)
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

	// Let any event handler run if the mouse event wasn't handled.
	// TODO: Should probably always be allowed to run, just not overwrite mouse_handled.
	if (!mouse_handled)
	{
		CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, OnMouseEvent, ms);
	}

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
	CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, OnMouseClick, mouse_state);
	return mouse_handled;
}

//
// Control - The mouse entered the controls bounds.
//
int EZ_control_OnMouseEnter(ez_control_t *self, mouse_state_t *mouse_state)
{
	int mouse_handled = false;
	self->flags |= CONTROL_MOUSE_OVER;
	CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, OnMouseEnter, mouse_state);
	return mouse_handled;
}

//
// Control - The mouse left the controls bounds.
//
int EZ_control_OnMouseLeave(ez_control_t *self, mouse_state_t *mouse_state)
{
	int mouse_handled = false;

	// Stop moving since the mouse is outside the control.
	self->flags &= ~(CONTROL_MOVING | CONTROL_MOUSE_OVER);

	CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, OnMouseLeave, mouse_state);
	return mouse_handled;
}

//
// Control - A mouse button was released within the bounds of the control.
//
int EZ_control_OnMouseUp(ez_control_t *self, mouse_state_t *mouse_state)
{
	int mouse_handled = false;

	// Stop moving / resizing.
	self->flags &= ~(CONTROL_MOVING | CONTROL_RESIZING_BOTTOM | CONTROL_RESIZING_LEFT | CONTROL_RESIZING_RIGHT | CONTROL_RESIZING_TOP);

	// Raise a click event.
	if (self->flags & CONTROL_CLICKED)
	{
		self->flags &= ~CONTROL_CLICKED;
		CONTROL_RAISE_EVENT(&mouse_handled, self, OnMouseClick, mouse_state);
	}

	// Call event handler.
	CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, OnMouseUp, mouse_state);

	return mouse_handled;
}

//
// Control - A mouse button was pressed within the bounds of the control.
//
int EZ_control_OnMouseDown(ez_control_t *self, mouse_state_t *ms)
{
	int mouse_handled = false;

	if (!(self->flags & CONTROL_ENABLED))
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

	if (!(self->flags & CONTROL_FOCUSED))
	{
		return false;
	}

	// Check if the mouse is at the edges of the control, and turn on the correct reszie mode if it is.
	if (((self->flags & CONTROL_RESIZE_H) || (self->flags & CONTROL_RESIZE_V)) && (ms->button_down == 1))
	{
		if (self->flags & CONTROL_RESIZE_H)
		{
			// Left side of the control.
			if (POINT_IN_RECTANGLE(ms->x, ms->y,
				self->absolute_x, self->absolute_y,
				self->resize_handle_thickness, self->height))
			{
				self->flags |= CONTROL_RESIZING_LEFT;
				mouse_handled = true;
			}

			// Right side of the control.
			if (POINT_IN_RECTANGLE(ms->x, ms->y,
				self->absolute_x + self->width - self->resize_handle_thickness, self->absolute_y,
				self->resize_handle_thickness, self->height))
			{
				self->flags |= CONTROL_RESIZING_RIGHT;
				mouse_handled = true;
			}
		}

		if (self->flags & CONTROL_RESIZE_V)
		{
			// Top of the control.
			if (POINT_IN_RECTANGLE(ms->x, ms->y,
				self->absolute_x, self->absolute_y,
				self->width, self->resize_handle_thickness))
			{
				self->flags |= CONTROL_RESIZING_TOP;
				mouse_handled = true;
			}

			// Bottom of the control.
			if (POINT_IN_RECTANGLE(ms->x, ms->y,
				self->absolute_x, self->absolute_y + self->height - self->resize_handle_thickness,
				self->width, self->resize_handle_thickness))
			{
				self->flags |= CONTROL_RESIZING_BOTTOM;
				mouse_handled = true;
			}
		}
	}

	// The control is being moved.
	if ((self->flags & CONTROL_MOVABLE) && (ms->button_down == 1))
	{
		self->flags |= CONTROL_MOVING;
		mouse_handled = true;
	}

	if (ms->button_down)
	{
		self->flags |= CONTROL_CLICKED;
		mouse_handled = true;
	}

	/*
	TODO : Fix OnMouseClick
	if(!mouse_handled)
	{
		CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, OnMouseClick, mouse_state);
	}
	*/

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
	CONTROL_EVENT_HANDLER_CALL(&wheel_handled, self, OnMouseWheel, mouse_state);
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
		CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, OnMouseHover, mouse_state);
	}

	return mouse_handled;
}

// =========================================================================================
// Label
// =========================================================================================

//
// Label - Creates a label control and initializes it.
//
ez_label_t *EZ_label_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  char *background_name,
							  int flags, int text_flags,
							  char *text, clrinfo_t text_color)
{
	ez_label_t *label = NULL;

	// We have to have a tree to add the control to.
	if (!tree)
	{
		return NULL;
	}

	label = (ez_label_t *)Q_malloc(sizeof(ez_label_t));
	memset(label, 0, sizeof(ez_label_t));

	EZ_label_Init(label, tree, parent, name, description, x, y, width, height, background_name, flags, text_flags, text, text_color);
	
	return label;
}

//
// Label - Initializes a label control.
//
void EZ_label_Init(ez_label_t *label, ez_tree_t *tree, ez_control_t *parent,
				  char *name, char *description,
				  int x, int y, int width, int height,
				  char *background_name,
				  int flags, int text_flags,
				  char *text, clrinfo_t text_color)
{
	// Initialize the inherited class first.
	EZ_control_Init(&label->super, tree, parent, name, description, x, y, width, height, background_name, flags);

	label->super.CLASS_ID				= EZ_LABEL_ID;
	label->super.inheritance_level		= EZ_LABEL_INHERITANCE_LEVEL;

	label->super.events.OnDraw			= EZ_label_OnDraw;
	label->super.events.OnKeyEvent		= EZ_label_OnKeyEvent;
	label->super.events.OnMouseDown		= EZ_label_OnMouseDown;
	label->super.events.OnMouseUp		= EZ_label_OnMouseUp;
	label->super.events.OnMouseHover	= EZ_label_OnMouseHover;
	label->super.events.OnResize		= EZ_label_OnResize;
	label->super.events.OnDestroy		= EZ_label_Destroy;

	label->super.flags	|= CONTROL_CONTAINED;
	label->scale		= 1.0;
	label->text			= NULL;
	label->text_flags	|= text_flags;
	label->color		= text_color;
	label->select_start = -1;
	label->select_end	= -1;

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
// Label - Calculates where in the label text that the wordwraps will be done.
//
static void EZ_label_CalculateWordwraps(ez_label_t *label)
{
	int i = 0;
	int current_index = -1;
	int char_size			= (label->text_flags & LABEL_LARGEFONT) ? 64 : 8;
	int scaled_char_size	= Q_rint(char_size * label->scale);

	if (label->text_flags & LABEL_WRAPTEXT)
	{	
		// Wordwrap the string to the virtual size of the control and save the
		// indexes where each row ends in an array.
		while ((i < LABEL_MAX_WRAPS) && Util_GetNextWordwrapString(label->text, NULL, current_index + 1, &current_index, LABEL_LINE_SIZE, label->super.virtual_width, scaled_char_size))
		{
			label->wordwraps[i] = current_index;
			i++;
		}

		label->wordwraps[i] = -1;
	}
	else
	{
		while (label->text[current_index])
		{
			if (label->text[current_index] == '\n')
			{
				label->wordwraps[i] = current_index;
				i++;
			}
			
			current_index++;
		}

		label->wordwraps[i] = -1;
	}
}

//
// Label - Set the text scale for the label.
//
void EZ_label_SetTextScale(ez_label_t *label, float scale)
{
	label->scale = scale;

	// We need to recalculate the wordwrap stuff since the size changed.
	EZ_label_CalculateWordwraps(label);
}

//
// Label - Sets the text color of a label.
//
void EZ_label_SetTextColor(ez_label_t *label, byte r, byte g, byte b, byte alpha)
{
	label->color.c = RGBA_TO_COLOR(r, g, b, alpha);
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
// Label - Sets the text of a label.
//
void EZ_label_SetText(ez_label_t *label, const char *text)
{
	int text_len = strlen(text) + 1;

	Q_free(label->text);

	if (text)
	{
		label->text = Q_calloc(text_len, sizeof(char));
		memset(label->text, 0, text_len * sizeof(char));
		strlcpy(label->text, text, text_len);
	}

	// Calculate where to wrap the text.
	EZ_label_CalculateWordwraps(label);
}

//
// Label - Happens when the control has resized.
//
int EZ_label_OnResize(ez_control_t *self)
{
	ez_label_t *label	= (ez_label_t *)self;
	
	// Let the super class try first.
	EZ_control_OnResize(self);

	// Calculate where to wrap the text.
	EZ_label_CalculateWordwraps(label);

	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnResize);

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
	int char_size			= (label->text_flags & LABEL_LARGEFONT) ? 64 : 8;
	int scaled_char_size	= char_size * label->scale;
	int last_index			= -1;
	int curr_row			= 0;
	int curr_col			= 0;
	color_t selection_color	= RGBA_TO_COLOR(178, 0, 255, 125);
	color_t caret_color		= RGBA_TO_COLOR(255, 0, 0, 125);

	// Let the super class draw first.
	EZ_control_OnDraw(self);

	// Get the position we're drawing at.
	EZ_control_GetDrawingPosition(self, &x, &y);

	// Find any newlines and draw line by line.
	for (i = 0; i <= strlen(label->text); i++)
	{
		// Draw selection markers.
		{
			if (((label->select_start > -1) && (label->select_end > -1)	// Is something selected at all?
				&& (label->select_end != label->select_start))			// Only highlight if we have at least 1 char selected.
				&&
				(((i >= label->select_start) && (i < label->select_end))
				|| (i >= label->select_end) && (i < label->select_start))
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

			if (i == label->caret_pos)
			{
				// Draw the caret.
				Draw_AlphaFillRGB(
					x + (curr_col * scaled_char_size), 
					y + (curr_row * scaled_char_size), 
					max(5, Q_rint(scaled_char_size * 0.1)), scaled_char_size, 
					caret_color);
			}
		}

		if (i == label->wordwraps[curr_row] || label->text[i] == '\0')
		{
			// We found the end of a line, copy the contents of the line to the line buffer.
			snprintf(line, min(LABEL_LINE_SIZE, (i - last_index) + 1), "%s", (label->text + last_index + 1));
			last_index = i;	// Skip the newline character

			if (label->text_flags & LABEL_LARGEFONT)
			{
				Draw_BigString(x, y + (curr_row * scaled_char_size), line, &label->color, 1, label->scale, 1, 0);
			}
			else
			{
				Draw_SColoredString(x, y + (curr_row * scaled_char_size), str2wcs(line), &label->color, 1, false, label->scale);
			}

			curr_row++;
			curr_col = 0;
		}
		else
		{
			curr_col++;
		}
	}

	Draw_String(x + (self->width / 2), y + (self->height / 2), va("%i %i (%i %i) %i", label->row_clicked, label->col_clicked, label->select_start, label->select_end, label->caret_pos));

	{
		char tmp[1024];

		int sublen = abs(label->select_end - label->select_start);
		if (sublen > 0)
		{
			snprintf(tmp, sublen + 1, "%s", label->text + min(label->select_start, label->select_end));
			Draw_String(x + (self->width / 2), y + (self->height / 2) + 8, tmp);
		}
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnDraw);

	return 0;
}

//
// Label - PRIVATE - Finds at what text index the mouse was clicked.
//
static int EZ_label_FindMouseTextIndex(ez_label_t *label, mouse_state_t *ms)
{
	// TODO : Support big font gaps.
	int x, y, i;
	int row, col;						// The row and column that the mouse is over.
	int cur_row				= 0;		// Current row index.
	int cur_col				= 0;		// Current column index.
	int char_size			= (label->text_flags & LABEL_LARGEFONT) ? 64 : 8;
	int scaled_char_size	= Q_rint(char_size * label->scale);
	int text_len			= strlen(label->text);

	// Get the position we're drawing at.
	EZ_control_GetDrawingPosition((ez_control_t *)label, &x, &y);

	// Find the row and column where the mouse is.
	row = ((ms->y - y) / scaled_char_size);
	col = ((ms->x - x) / scaled_char_size);

	label->row_clicked = row;
	label->col_clicked = col;

	// Find which index in the string the row an column corresponds to.
	for (i = 0; i <= text_len; i++)
	{
		if ((cur_row < LABEL_MAX_WRAPS) && (i == label->wordwraps[cur_row]))
		{
			// A newline was found, go to the next row.
			cur_row++;

			// Skip the new line.
			i++;

			// Don't continue if the current row is past the
			// row the mouse is on. Then we'll use the last index
			// on the previous row.
			if (cur_row > row)
			{
				return i - 1;
			}
			
			// We're on a new line so restart the column count.
			cur_col = 0;
		}

		// If both the current row and column match with the row and column
		// the mouse is in, it means we've found the text index.
		if ((cur_row == row) && (cur_col == col))
		{
			// We found the index, stop looking.
			return i;
		}

		cur_col++;
	}

	if (row > cur_row)
	{
		return text_len;
	}

	return -1;
}

//
// Label - Set the caret position.
//
void EZ_label_SetCaretPosition(ez_label_t *label, int caret_pos)
{
	label->caret_pos = caret_pos;
}

//
// Label - Moves the caret up or down.
//
static void EZ_label_MoveCaretVertically(ez_label_t *label, qbool up)
{
	int i			= 0;
	int curr_row	= 0;
	int curr_col	= 0;
	int caret_row	= 0;
	int caret_col	= 0;
	qbool found		= false;
	int text_len	= strlen(label->text);

	for (i = 0; i < text_len; i++)
	{
		if (found)
		{
			if (!up)
			{
				if ((curr_row == caret_row + 1) && (curr_col == caret_col))
				{
					label->caret_pos = i;
					break;
				}
			}
			else
			{
				if ((curr_row == caret_row - 1) && (curr_col == caret_col))
				{
					label->caret_pos = i;
					break;
				}
			}
		}

		if (label->caret_pos == i)
		{
			// We found the current row and column.
			caret_row = curr_row;
			caret_col = curr_col;
			
			// Restart from the beginning, we need to find the row above.
			if (up && !found)
			{
				i = -1;
				curr_row = 0;
				curr_col = 0;
			}

			found = true;

			// Do the restart.
			if (up)
			{
				continue;
			}
		}

		if (label->wordwraps[curr_row] == i)
		{
			// New line.
			curr_row++;
			curr_col = 0;
		}
		else
		{
			curr_col++;
		}
	}
}

//
// Label - Key event.
//
int EZ_label_OnKeyEvent(ez_control_t *self, int key, int unichar)
{
	ez_label_t *label	= (ez_label_t *)self;
	int key_handled		= false;
	int text_len		= strlen(label->text);
	int caret_delta		= 0;

	key_handled = EZ_control_OnKeyEvent(self, key, unichar);

	if (key_handled)
	{
		return true;
	}

	key_handled = true;

	switch (key)
	{
		case K_LEFTARROW :
		{
			label->caret_pos--;
			break;
		}
		case K_RIGHTARROW :
		{
			label->caret_pos++;
			break;
		}
		case K_UPARROW :
		{
			EZ_label_MoveCaretVertically(label, true);
			break;
		}
		case K_DOWNARROW :
		{
			EZ_label_MoveCaretVertically(label, false);
			break;
		}
		default :
		{
			key_handled = false;
			break;
		}
	}

	clamp(label->caret_pos, 0, text_len);

	CONTROL_EVENT_HANDLER_CALL(&key_handled, self, OnKeyEvent, key, unichar);
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
	if (label->text_flags & LABEL_SELECTING)
	{
		label->select_end = EZ_label_FindMouseTextIndex(label, ms);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnMouseHover, ms);

	return mouse_handled;
}

//
// Label - Handles the mouse down event.
//
int EZ_label_OnMouseDown(ez_control_t *self, mouse_state_t *ms)
{
	qbool mouse_handled = false;
	ez_label_t *label = (ez_label_t *)self;

	// Call the super class first.
	mouse_handled = EZ_control_OnMouseDown(self, ms);

	// Find at what index in the text the mouse was clicked.
	label->select_start = EZ_label_FindMouseTextIndex(label, ms);

	// Reset the caret and selection end since we just started a new select / caret positioning.
	label->select_end = -1;
	label->caret_pos = -1;

	// We just started to select some text.
	label->text_flags |= LABEL_SELECTING;

	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnMouseDown, ms);

	return mouse_handled;
}

//
// Label - Handles the mouse up event.
//
int EZ_label_OnMouseUp(ez_control_t *self, mouse_state_t *ms)
{
	qbool mouse_handled = false;
	ez_label_t *label = (ez_label_t *)self;

	// Call the super class first.
	mouse_handled = EZ_control_OnMouseUp(self, ms);

	// Find at what index in the text where the mouse was released.
	if ((self->flags & CONTROL_FOCUSED) && (label->text_flags & LABEL_SELECTING))
	{
		label->select_end = EZ_label_FindMouseTextIndex(label, ms);
		label->caret_pos = label->select_end;
	}
	else
	{
		label->select_start	= -1;
		label->select_end	= -1;
		label->caret_pos	= -1;
	}

	// We've stopped selecting.
	label->text_flags &= ~LABEL_SELECTING;

	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnMouseUp, ms);

	return mouse_handled;
}

// =========================================================================================
// Button
// =========================================================================================

//
// Button - Creates a new button and initializes it.
//
ez_button_t *EZ_button_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  char *background_name, char *hover_image, char *pressed_image,
							  int flags)
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
							  int flags)
{
	// Initialize the inherited class first.
	EZ_control_Init(&button->super, tree, parent, name, description, x, y, width, height, background_name, flags);

	// Initilize the button specific stuff.
	button->super.CLASS_ID = EZ_BUTTON_ID;
	button->super.inheritance_level = EZ_BUTTON_INHERITANCE_LEVEL;

	button->super.flags |= (flags | CONTROL_FOCUSABLE | CONTROL_CONTAINED);

	// Override the draw function.
	button->super.events.OnDraw = EZ_button_OnDraw;

	// Button specific events.
	button->events.OnAction = EZ_button_OnAction;

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
	CONTROL_EVENT_HANDLER_CALL(NULL, button, OnAction);

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
	// qbool mouse_inside = 0;
	ez_button_t *button = (ez_button_t *)self;

	int x, y;
	EZ_control_GetDrawingPosition(self, &x, &y);

	// Run the parents implementation first.
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

	if (self->flags & CONTROL_CLICKED)
	{
		Draw_AlphaFillRGB(x, y, self->width, self->height, RGBAVECT_TO_COLOR(button->color_pressed));
	}

	if (self->flags & CONTROL_MOUSE_OVER)
	{
		if (self->flags & CONTROL_CLICKED)
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

	if (self->flags & CONTROL_FOCUSED)
	{
		Draw_AlphaRectangleRGB(x, y, self->width, self->height, 1, false, RGBAVECT_TO_COLOR(button->color_focused));
		//Draw_ColoredString3(self->absolute_x, self->absolute_y, button->text, button->focused_text_color, 1, 0);
	}

	// Draw control specifics.
	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnDraw);

	return 0;
}


