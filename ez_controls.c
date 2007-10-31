
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

$Id: ez_controls.c,v 1.78 2007/10/27 14:51:15 cokeman1982 Exp $
*/

#include "quakedef.h"
#include "keys.h"
#include "utils.h"
#include "common_draw.h"
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

	if (item->previous)
	{
		item->previous->next = item->next;
	}
	else
	{
		// We removed the first item, so make sure we still have a head.
		list->head = item->next;
	}

	if (item->next)
	{
		item->next->previous = item->previous;
	}
	else if (item->previous)
	{
		// We removed the last item, so make sure we have a tail.
		list->tail = item->previous;
	}

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

	int p_bound_top		= p ? p->bound_top		: 0;
	int p_bound_bottom	= p ? p->bound_bottom	: 0;
	int p_bound_left	= p ? p->bound_left		: 0;
	int p_bound_right	= p ? p->bound_right	: 0;

	// Change the bounds so that we don't draw over our parents resize handles.
	if (p)
	{
		if (p->ext_flags & control_resize_h)
		{
			p_bound_right	-= p->resize_handle_thickness;
			p_bound_left	+= p->resize_handle_thickness;
		}

		if (p->ext_flags & control_resize_v)
		{
			p_bound_bottom	-= p->resize_handle_thickness;
			p_bound_top		+= p->resize_handle_thickness;
		}
	}

	// If the control has a parent (and should be contained within it's parent), 
	// set the corresponding bound to the parents bound (ex. button), 
	// otherwise use the drawing area of the control as bounds (ex. windows).
	control->bound_top		= (p && contained && (top	 < p_bound_top))		? (p_bound_top)		: top;
	control->bound_bottom	= (p && contained && (bottom > p_bound_bottom))		? (p_bound_bottom)	: bottom;
	control->bound_left		= (p && contained && (left	 < p_bound_left))		? (p_bound_left)	: left;
	control->bound_right	= (p && contained && (right	 > p_bound_right))		? (p_bound_right)	: right;

	// Make sure that the left bounds isn't greater than the right bounds and so on.
	// This would lead to controls being visible in a few incorrect cases.
	clamp(control->bound_top, 0, max(0, control->bound_bottom));
	clamp(control->bound_bottom, control->bound_top, vid.conheight);
	clamp(control->bound_left, 0, max(0, control->bound_right));
	clamp(control->bound_right, control->bound_left, vid.conwidth);

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
static void EZ_tree_Draw(ez_tree_t *tree)
{
	ez_control_t *payload = NULL;
	ez_dllist_node_t *iter = tree->drawlist.head;

	while (iter)
	{
		payload = (ez_control_t *)iter->payload;

		/*
		Draw_AlphaRectangleRGB(
			payload->absolute_virtual_x, 
			payload->absolute_virtual_y, 
			payload->virtual_width, 
			payload->virtual_height, 
			1, false, RGBA_TO_COLOR(255, 0, 0, 125));
			*/

		// Don't draw the invisible controls.
		if (!(payload->ext_flags & control_visible) || (payload->int_flags & control_hidden_by_parent))
		{
			iter = iter->next;
			continue;
		}

		// TODO : Remove this test stuff.
		/*
		if (!strcmp(payload->name, "label"))
		{
			Draw_String(payload->absolute_virtual_x, payload->absolute_virtual_y - 10, 
				va("avx: %i avy: %i vx: %i vy %i", 
				payload->absolute_virtual_x, payload->absolute_virtual_y, payload->virtual_x, payload->virtual_y));
		}

		if (!strcasecmp(payload->name, "Child 1"))
		{
			Draw_String(payload->absolute_virtual_x, payload->absolute_virtual_y - 10, 
				va("vw: %i vh: %i w: %i h %i", 
				payload->virtual_width, payload->virtual_height, payload->width, payload->height));
		}
		*/

		/*
		if (!strcasecmp(payload->name, "button"))
		{
			Draw_AlphaLineRGB(payload->bound_left, 0, payload->bound_left, vid.conheight, 1, RGBA_TO_COLOR(255, 0, 0, 255));
			Draw_AlphaLineRGB(payload->bound_right, 0, payload->bound_right, vid.conheight, 1, RGBA_TO_COLOR(255, 0, 0, 255));
		}
		*/

		if (!strcasecmp(payload->name, "Vertical scrollbar"))
		{
			Draw_String(payload->absolute_virtual_x, payload->absolute_virtual_y - 10, 
				va("b: %i r: %i", payload->bottom_edge_gap, payload->right_edge_gap));
		}

		// Bugfix: Make sure we don't even bother trying to draw something that is completly offscreen
		// it will cause a weird flickering bug because of glScissor.
		if ((payload->bound_left == payload->bound_right) || (payload->bound_top == payload->bound_bottom))
		{
			iter = iter->next;
			continue;
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
// Control Tree - Checks how long mouse buttons have been pressed.
//
static void EZ_tree_RaiseRepeatedMouseButtonEvents(ez_tree_t *tree)
{
	if (tree->focused_node && tree->focused_node->payload)
	{
		ez_control_t *control = (ez_control_t *)tree->focused_node->payload;

		// Notify controls that are listening to repeat mouse events 
		// if the control's set delay has been reached.
		if (control->ext_flags & control_listen_repeat_mouse)
		{
			int i;
			double now			= Sys_DoubleTime();
			mouse_state_t ms	= tree->prev_mouse_state;
			ms.button_up		= 0;
			
			// Go through all the mouse buttons and compare the time since
			// the mouse buttons where pressed with the controls delay time.
			for (i = 1; i <= 8; i++)
			{
				// Only repeat the mouse click if the button is already down
				// and the repeat isn't set to be all the time.
				if (ms.buttons[i]
				 &&	(control->mouse_repeat_delay > 0.0) && (tree->mouse_pressed_time[i] > 0.0)
				 && ((now - tree->mouse_pressed_time[i]) >= control->mouse_repeat_delay))
				{
					ms.button_down = i;
					CONTROL_RAISE_EVENT(NULL, control, ez_control_t, OnMouseEvent, &ms);
				}
			}
		}
	}
}

//
// Control Tree - Needs to be called every frame to keep the tree alive.
//
void EZ_tree_EventLoop(ez_tree_t *tree)
{
	if (!tree->root)
	{
		return;
	}

	// Check if it's time to raise any repeat mouse events for controls listening to those.
	EZ_tree_RaiseRepeatedMouseButtonEvents(tree);

	// Calculate the drawing bounds for all the controls in the control tree.
	EZ_tree_SetDrawBounds(tree->root);

	EZ_tree_Draw(tree);
}

//
// Control Tree - Dispatches a mouse event to a control tree.
//
qbool EZ_tree_MouseEvent(ez_tree_t *tree, mouse_state_t *ms)
{
	int mouse_handled = false;
	ez_control_t *control = NULL;
	ez_dllist_node_t *iter = NULL;

	if (!tree)
	{
		Sys_Error("EZ_tree_MouseEvent: NULL tree reference.\n");
	}

	// Save the time that the specified button was last pressed.
	if (ms->button_down || ms->button_up)
	{
		// Set the time for the pressed button.
		if (ms->button_down)
		{
			tree->mouse_pressed_time[ms->button_down] = Sys_DoubleTime();
		}
		else if (ms->button_up)
		{
			tree->mouse_pressed_time[ms->button_up] = 0.0;
		}
	}

	// Save the mouse state so that it can be used when raising
	// repeated mouse click events for controls that wants them.
	tree->prev_mouse_state = *ms;

	// Propagate the mouse event in the opposite order that we drew
	// the controls (Since they are drawn from back to front), so
	// that the foremost control gets it first.
	for (iter = tree->drawlist.tail; iter; iter = iter->previous)
	{
		control = (ez_control_t *)iter->payload;

		// Notify the control of the mouse event.
		CONTROL_RAISE_EVENT(&mouse_handled, control, ez_control_t, OnMouseEvent, ms);

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

	// Send key events to the focused control.
	if (tree->focused_node && tree->focused_node->payload)
	{
		CONTROL_RAISE_EVENT(&key_handled, (ez_control_t *)tree->focused_node->payload, ez_control_t, OnKeyEvent, key, unichar, down);
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
// Control Tree - Destroys a tree. Will not free the memory for the tree.
//
void EZ_tree_Destroy(ez_tree_t *tree)
{
	ez_dllist_node_t *iter = NULL;

	if (!tree)
	{
		return;
	}

	EZ_control_Destroy(tree->root, true);

	memset(tree, 0, sizeof(ez_tree_t));
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
							  int flags)
{
	ez_control_t *control = NULL;

	// We have to have a tree to add the control to.
	if(!tree)
	{
		return NULL;
	}

	control = (ez_control_t *)Q_malloc(sizeof(ez_control_t));
	memset(control, 0, sizeof(ez_control_t));

	EZ_control_Init(control, tree, parent, name, description, x, y, width, height, flags);

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
// Eventhandler - Creates a eventhandler.
//
ez_eventhandler_t *EZ_eventhandler_Create(void *event_func, int func_type, void *payload)
{
	ez_eventhandler_t *e	= Q_calloc(1, sizeof(ez_eventhandler_t));
	e->function_type		= func_type;
	e->payload				= payload;

	switch(func_type)
	{
		case EZ_CONTROL_HANDLER :
			e->function.normal = (ez_eventhandler_fp)event_func;
			break;
		case EZ_CONTROL_MOUSE_HANDLER :
			e->function.mouse = (ez_mouse_eventhandler_fp)event_func;
			break;
		case EZ_CONTROL_KEY_HANDLER :
			e->function.key = (ez_key_eventhandler_fp)event_func;
			break;
		case EZ_CONTROL_KEYSP_HANDLER :
			e->function.key_sp = (ez_keyspecific_eventhandler_fp)event_func;
			break;
		case EZ_CONTROL_DESTROY_HANDLER :
			e->function.destroy = (ez_destroy_eventhandler_fp)event_func;
			break;
		default :
			e->function.normal = NULL;
			break;
	}

	e->next	= NULL;

	return e;
}	

//
// Eventhandler - Goes through the list of events and removes the one with the specified function.
//
void EZ_eventhandler_Remove(ez_eventhandler_t *eventhandler, void *event_func, qbool all)
{
	ez_eventhandler_t *it = eventhandler;
	ez_eventhandler_t *prev = NULL;

	while (it)
	{
		if (all)
		{
			prev = prev->next;
			Q_free(it);
			it = prev;
		}
		else 
		{
			if (event_func && ((void *)it->function.normal == (void *)event_func))
			{
				if (prev)
				{
					prev->next = it->next;
				}

				Q_free(it);

				return;
			}

			prev = it;
			it = it->next;
		}
	}
}

//
// Eventhandler - Execute an event handler.
//
void EZ_eventhandler_Exec(ez_eventhandler_t *event_handler, ez_control_t *ctrl, ...)
{
	va_list argptr;
	int ft = event_handler->function_type;
	void *payload = event_handler->payload;
	ez_eventhandlerfunction_t *et = &event_handler->function;

	va_start(argptr, ctrl);

	// Pass on the proper amount of arguments, depending on the type of function.
	if (ft == EZ_CONTROL_HANDLER)
	{
		et->normal(ctrl, payload);
	}
	else if (ft == EZ_CONTROL_MOUSE_HANDLER)
	{
		et->mouse(ctrl, payload, va_arg(argptr, mouse_state_t *));	
	}
	else if (ft == EZ_CONTROL_KEY_HANDLER)
	{
		et->key(ctrl, payload, va_arg(argptr, int), va_arg(argptr, int), va_arg(argptr, qbool));
	}
	else if (ft == EZ_CONTROL_KEYSP_HANDLER)
	{
		et->key_sp(ctrl, payload, va_arg(argptr, int), va_arg(argptr, int));
	}
	else if (ft == EZ_CONTROL_DESTROY_HANDLER)	
	{
		et->destroy(ctrl, payload, va_arg(argptr, qbool));
	}

	va_end(argptr);
}

//
// Control - Initializes a control and adds it to the specified control tree.
//
void EZ_control_Init(ez_control_t *control, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height, 
							  ez_control_flags_t flags)
{
	static int order		= 0;

	control->CLASS_ID		= EZ_CONTROL_ID;

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

	control->draw_order					= order++;

	control->resize_handle_thickness	= 5;
	control->width_max					= vid.conwidth;
	control->height_max					= vid.conheight;
	control->width_min					= 5;
	control->height_min					= 5;

	// Setup the default event functions, none of these should ever be NULL.
	CONTROL_REGISTER_EVENT(control, EZ_control_OnMouseEvent, OnMouseEvent, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnMouseClick, OnMouseClick, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnMouseDown,	OnMouseDown, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnMouseUp, OnMouseUp, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnMouseUpOutside, OnMouseUpOutside, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnMouseEnter, OnMouseEnter, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnMouseLeave, OnMouseLeave, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnMouseHover, OnMouseHover, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_Destroy, OnDestroy, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnDraw, OnDraw, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnGotFocus, OnGotFocus, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnKeyEvent, OnKeyEvent, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnKeyDown, OnKeyDown, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnKeyUp, OnKeyUp, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnLostFocus, OnLostFocus, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnLayoutChildren, OnLayoutChildren, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnMove, OnMove, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnScroll, OnScroll, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnParentScroll, OnParentScroll, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnResize, OnResize, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnParentResize, OnParentResize, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnMinVirtualResize, OnMinVirtualResize, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnVirtualResize, OnVirtualResize, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnFlagsChanged, OnFlagsChanged, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnEventHandlerChanged, OnEventHandlerChanged, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnAnchorChanged, OnAnchorChanged, ez_control_t);
	CONTROL_REGISTER_EVENT(control, EZ_control_OnVisibilityChanged, OnVisibilityChanged, ez_control_t);

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
	control->width					= width;
	control->height					= height;
	control->virtual_width			= width;
	control->virtual_height			= height;
	control->prev_width				= width;
	control->prev_height			= height;
	control->prev_virtual_width		= width;
	control->prev_virtual_height	= height;

	control->int_flags |= control_update_anchorgap;

	EZ_control_SetVirtualSize(control, width, height);
	EZ_control_SetMinVirtualSize(control, width, height);
	EZ_control_SetPosition(control, x, y);
	EZ_control_SetSize(control, width, height);

	// Set a default delay for raising new mouse click events.
	EZ_control_SetRepeatMouseClickDelay(control, 0.2);
}

//
// Control - Destroys a specified control.
//
int EZ_control_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_dllist_node_t *iter = NULL;
	ez_dllist_node_t *temp = NULL;

	// Nothing to destroy :(
	if (!self)
	{
		return 0;
	}

	if (!self->control_tree)
	{
		// Very bad!
		Sys_Error("EZ_control_Destroy(): tried to destroy control without a tree.\n");
		return 0;
	}

	// Cleanup any specifics this control may have.
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDestroy, destroy_children);

	iter = self->children.head;

	// Destroy the children!
	while (iter)
	{
		if (destroy_children)
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

	EZ_eventhandler_Remove(self->event_handlers.OnDestroy, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnDraw, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnEventHandlerChanged, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnFlagsChanged, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnGotFocus, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnKeyDown, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnKeyEvent, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnKeyUp, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnLayoutChildren, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnLostFocus, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnMinVirtualResize, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnMouseClick, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnMouseDown, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnMouseEnter, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnMouseEvent, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnMouseHover, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnMouseLeave, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnMouseUp, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnMouseUpOutside, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnMove, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnParentResize, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnParentScroll, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnResize, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnResizeHandleThicknessChanged, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnScroll, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnVirtualResize, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnAnchorChanged, NULL, true);
	EZ_eventhandler_Remove(self->event_handlers.OnVisibilityChanged, NULL, true);

	Q_free(self);

	return 0;
}

//
// Control - Sets the OnDestroy event handler.
//
void EZ_control_AddOnDestroy(ez_control_t *self, ez_destroy_eventhandler_fp OnDestroy, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_DESTROY_HANDLER, OnDestroy, ez_control_t, OnDestroy, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnFlagsChanged event handler.
//
void EZ_control_AddOnFlagsChanged(ez_control_t *self, ez_eventhandler_fp OnFlagsChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnFlagsChanged, ez_control_t, OnFlagsChanged, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnLayoutChildren event handler.
//
void EZ_control_AddOnLayoutChildren(ez_control_t *self, ez_eventhandler_fp OnLayoutChildren, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnLayoutChildren, ez_control_t, OnLayoutChildren, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnMove event handler.
//
void EZ_control_AddOnMove(ez_control_t *self, ez_eventhandler_fp OnMove, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnMove, ez_control_t, OnMove, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnScroll event handler.
//
void EZ_control_AddOnScroll(ez_control_t *self, ez_eventhandler_fp OnScroll, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnScroll, ez_control_t, OnScroll, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnResize event handler.
//
void EZ_control_AddOnResize(ez_control_t *self, ez_eventhandler_fp OnResize, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnResize, ez_control_t, OnResize, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnParentResize event handler.
//
void EZ_control_AddOnParentResize(ez_control_t *self, ez_eventhandler_fp OnParentResize, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnParentResize, ez_control_t, OnParentResize, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnMinVirtualResize event handler.
//
void EZ_control_AddOnMinVirtualResize(ez_control_t *self, ez_eventhandler_fp OnMinVirtualResize, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnMinVirtualResize, ez_control_t, OnMinVirtualResize, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnVirtualResize event handler.
//
void EZ_control_AddOnVirtualResize(ez_control_t *self, ez_eventhandler_fp OnVirtualResize, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnVirtualResize, ez_control_t, OnVirtualResize, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnKeyEvent event handler.
//
void EZ_control_AddOnKeyEvent(ez_control_t *self, ez_key_eventhandler_fp OnKeyEvent, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_KEY_HANDLER, OnKeyEvent, ez_control_t, OnKeyEvent, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnLostFocus event handler.
//
void EZ_control_AddOnLostFocus(ez_control_t *self, ez_eventhandler_fp OnLostFocus, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnLostFocus, ez_control_t, OnLostFocus, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnGotFocus event handler.
//
void EZ_control_AddOnGotFocus(ez_control_t *self, ez_eventhandler_fp OnGotFocus, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnGotFocus, ez_control_t, OnGotFocus, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnMouseHover event handler.
//
void EZ_control_AddOnMouseHover(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseHover, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_MOUSE_HANDLER, OnMouseHover, ez_control_t, OnMouseHover, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnMouseLeave event handler.
//
void EZ_control_AddOnMouseLeave(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseLeave, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_MOUSE_HANDLER, OnMouseLeave, ez_control_t, OnMouseLeave, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnMouseEnter event handler.
//
void EZ_control_AddOnMouseEnter(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseEnter, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_MOUSE_HANDLER, OnMouseEnter, ez_control_t, OnMouseEnter, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnMouseClick event handler.
//
void EZ_control_AddOnMouseClick(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseClick, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_MOUSE_HANDLER, OnMouseClick, ez_control_t, OnMouseClick, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnMouseUp event handler.
//
void EZ_control_AddOnMouseUp(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseUp, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_MOUSE_HANDLER, OnMouseUp, ez_control_t, OnMouseUp, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnMouseUpOutside event handler.
//
void EZ_control_AddOnMouseUpOutside(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseUpOutside, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_MOUSE_HANDLER, OnMouseUpOutside, ez_control_t, OnMouseUpOutside, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnMouseDown event handler.
//
void EZ_control_AddOnMouseDown(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseDown, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_MOUSE_HANDLER, OnMouseDown, ez_control_t, OnMouseDown, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnMouseEvent event handler.
//
void EZ_control_AddOnMouseEvent(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseEvent, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_MOUSE_HANDLER, OnMouseEvent, ez_control_t, OnMouseEvent, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnDraw event handler.
//
void EZ_control_AddOnDraw(ez_control_t *self, ez_eventhandler_fp OnDraw, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnDraw, ez_control_t, OnDraw, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Set the event handler for the OnEventHandlerChanged event.
//
void EZ_control_AddOnEventHandlerChanged(ez_control_t *self, ez_eventhandler_fp OnEventHandlerChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnEventHandlerChanged, ez_control_t, OnEventHandlerChanged, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Control - Sets the OnResizeHandleThicknessChanged event handler.
//
void EZ_control_AddOnResizeHandleThicknessChanged(ez_control_t *self, ez_eventhandler_fp OnResizeHandleThicknessChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnResizeHandleThicknessChanged, ez_control_t, OnResizeHandleThicknessChanged, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
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
// Control - Set the background image for the control.
//
void EZ_control_SetBackgroundImage(ez_control_t *self, const char *background_path)
{
	self->background = background_path ? Draw_CachePicSafe(background_path, false, true) : NULL;
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
void EZ_control_SetResizeable(ez_control_t *self, qbool resizeable)
{
	// TODO : Is it confusing having this resizeable?
	SET_FLAG(self->ext_flags, control_resizeable, resizeable);
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

	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnVisibilityChanged);
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
// Control - Sets if the control should move it's parent when it moves itself.
//
void EZ_control_SetMovesParent(ez_control_t *self, qbool moves_parent)
{
	SET_FLAG(self->ext_flags, control_move_parent, moves_parent);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnFlagsChanged);
}

//
// Control - Sets whetever the control should care about mouse input or not.
//
void EZ_control_SetIgnoreMouse(ez_control_t *self, qbool ignore_mouse)
{
	SET_FLAG(self->ext_flags, control_ignore_mouse, ignore_mouse);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnFlagsChanged);
}

//
// Control - Listen to repeated mouse click events when holding down a mouse button. 
//           The delay between events is set using EZ_control_SetRepeatMouseClickDelay(...)
//
void EZ_control_SetListenToRepeatedMouseClicks(ez_control_t *self, qbool listen_repeat)
{
	SET_FLAG(self->ext_flags, control_listen_repeat_mouse, listen_repeat);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnFlagsChanged);
}

//
// Control - Sets the amount of time to wait between each new mouse click event
//           when holding down the mouse over a control.
//
void EZ_control_SetRepeatMouseClickDelay(ez_control_t *self, double delay)
{
	self->mouse_repeat_delay = delay;
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
// Control - Gets the anchor flags.
//
ez_anchor_t EZ_control_GetAnchor(ez_control_t *self)
{
	return self->anchor_flags;
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
// Control - Updates the anchor gaps between the controls edges and it's parent 
//           (the distance to maintain from the parent when anchored to opposit edges).
//
static void EZ_control_UpdateAnchorGap(ez_control_t *self)
{
	if (!(self->int_flags & control_update_anchorgap))
	{
		return;
	}
	else if (self->parent)
	{
		qbool anchor_viewport	= self->ext_flags & control_anchor_viewport;
		int p_height			= anchor_viewport ? self->parent->height : self->parent->virtual_height;
		int p_width				= anchor_viewport ? self->parent->width  : self->parent->virtual_width;
		
		if ((self->anchor_flags & (anchor_bottom | anchor_top)) == (anchor_bottom | anchor_top))
		{
			self->top_edge_gap		= self->y;
			self->bottom_edge_gap	= p_height - (self->y + self->height);
		}

		if ((self->anchor_flags & (anchor_left | anchor_right)) == (anchor_left | anchor_right))
		{
			self->left_edge_gap		= self->x;
			self->right_edge_gap	= p_width - (self->y + self->height);
		}
	}
}

//
// Control - Sets the anchoring of the control to it's parent.
//
void EZ_control_SetAnchor(ez_control_t *self, ez_anchor_t anchor_flags)
{
	self->anchor_flags = anchor_flags;

	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnAnchorChanged);
}

//
// Control - Sets the tab order of a control.
//
void EZ_control_SetDrawOrder(ez_control_t *self, int draw_order, qbool update_children)
{
	// TODO : Is this the best way to change the draw order for the children?
	if (update_children && self->children.count > 0)
	{
		ez_dllist_node_t *it	= self->children.head;
		ez_control_t *child		= (ez_control_t *)it->payload;
		int draw_order_delta	= draw_order - self->draw_order;

		while (it)
		{
			EZ_control_SetDrawOrder(child, draw_order + draw_order_delta, update_children);
			it = it->next;
		}
	}

	self->draw_order = draw_order;

	// TODO : Force teh user to do this explicitly? Because it will be run a lot of times if there's many children. 
	EZ_tree_OrderDrawList(self->control_tree);
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

	clamp(self->height, self->height_min, self->height_max);
	clamp(self->width, self->width_min, self->width_max);

	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnResize);
}

//
// Control - Set the thickness of the resize handles (if any).
//
void EZ_control_SetResizeHandleThickness(ez_control_t *self, int thickness)
{
	self->resize_handle_thickness = thickness;
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
// Control - The anchoring for the control changed.
//
int EZ_control_OnAnchorChanged(ez_control_t *self)
{
	// Calculate the gaps to the parents edges (used for anchoring, see OnParentResize for detailed explination).
	EZ_control_UpdateAnchorGap(self);

	// Make sure the control is repositioned correctly.
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnMove);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnParentResize);

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnAnchorChanged);
	return 0;
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

	// Calculate the gaps to the parents edges (used for anchoring, see OnParentResize for detailed explination).
	EZ_control_UpdateAnchorGap(self);

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
		ez_control_t *p			= self->parent;
		int x					= self->x;
		int y					= self->y;
		int new_width			= self->width;
		int new_height			= self->height;
		qbool anchor_viewport	= (self->ext_flags & control_anchor_viewport);
		int parent_prev_width	= anchor_viewport ? self->parent->prev_width	: self->parent->prev_virtual_width;
		int parent_prev_height	= anchor_viewport ? self->parent->prev_height	: self->parent->prev_virtual_height;
		int parent_width		= anchor_viewport ? self->parent->width			: self->parent->virtual_width; 
		int parent_height		= anchor_viewport ? self->parent->height		: self->parent->virtual_height;

		if ((self->anchor_flags & (anchor_left | anchor_right)) == (anchor_left | anchor_right))
		{			
			// Set the new width so that the right side of the control is
			// still the same distance from the right side of the parent.
			new_width = parent_width - (x + self->right_edge_gap);
		}

		if ((self->anchor_flags & (anchor_top | anchor_bottom)) == (anchor_top | anchor_bottom))
		{
			new_height = parent_height - (y + self->bottom_edge_gap);
		}

		// Set the new size if it changed.
		if ((self->width != new_width) || (self->height != new_height))
		{
			// When the control is anchored to two opposit edges we want to stretch it
			// when it's parent is resized. If the parent becomes either
			// smaller or larger than the controls max/min size when doing this, it will
			// make us stop resizing the child. And since we want the child to

			// We need special behaviour when resizing a control that is anchored to two
			// opposit edges of it's parent (it should be stretched). A child control is placed
			// inside of a parent, and it's given a size and anchoring points. When resizing the
			// parent, we want to maintain the distance between the parents edges and the childs edges,
			// that is, if the childs right edge was 10 pixels from it's parents right edge before
			// we resized the parent, we want it to be the same afterwards also. We achieve this by
			// resizing the child (stretching it) to maintain the same gap. 
			//
			// This all works fine if the child is allowed to have any size (even negative), since it will
			// always maintain the same gap between the edges. The problem arises when you introduce the 
			// possibility for a control to have a min/max size, in this case we need to stop updating 
			// the gap when the min/max size has been reached, and remember the last used gap size until
			// the parent goes back to a size that let's its child keep the correct edge gap without 
			// violating its size bounds again. 
			//
			// We do this by only updating the gap size either when setting new anchoring points,
			// or when someone explicitly changes the size of the child control. Therefore any resize of the child
			// control that is triggered by its parent being resized doesn't produce a new gap size.
			self->int_flags &= ~control_update_anchorgap;
			
			EZ_control_SetSize(self, new_width, new_height);

			self->int_flags |= control_update_anchorgap;
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
// Control - Hide a control if its parent is hidden. We don't unset control_visible for this
//			because we want to let any child inside of a control to decide if it should be
//			visible or not on it's own, if we just set control_visible here we'd overwrite
//			the childs visibility.
//
static void EZ_control_SetHiddenByParent(ez_control_t *self, qbool hidden)
{
	ez_dllist_node_t *iter	= self->children.head;
	ez_control_t *child		= NULL;

	SET_FLAG(self->int_flags, control_hidden_by_parent, hidden);

	// Show or hide our children also.
	while(iter)
	{
		child = (ez_control_t *)iter->payload;
		EZ_control_SetHiddenByParent(child, hidden);
		iter = iter->next;
	}
}

//
// Control - Visibility changed.
//
int EZ_control_OnVisibilityChanged(ez_control_t *self)
{
	// Hide any children.
	EZ_control_SetHiddenByParent(self, !(self->ext_flags & control_visible));

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnVisibilityChanged);
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
// Control - OnResizeHandleThicknessChanged event.
//
int EZ_control_OnResizeHandleThicknessChanged(ez_control_t *self)
{
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnResizeHandleThicknessChanged);
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
	ez_control_t *child		= NULL;
	ez_dllist_node_t *iter	= self->children.head;
	qbool anchor_viewport	= (self->ext_flags & control_anchor_viewport);
	int parent_x			= 0;
	int parent_y			= 0;

	self->prev_absolute_x = self->absolute_x;
	self->prev_absolute_y = self->absolute_y;

	// Get the position of the area we're anchoring to. Normal behaviour is to anchor to the virtual area.
	if (self->parent)
	{
		parent_x = anchor_viewport ? self->parent->absolute_x : self->parent->absolute_virtual_x;
		parent_y = anchor_viewport ? self->parent->absolute_y : self->parent->absolute_virtual_y;
	}

	// Update the absolute screen position based on the parents position.
	self->absolute_x = (self->parent ? parent_x : 0) + self->x;
	self->absolute_y = (self->parent ? parent_y : 0) + self->y;

	if (self->parent)
	{
		// If the control has a parent, position it in relation to it
		// and the way it's anchored to it.
		int parent_prev_width	= anchor_viewport ? self->parent->prev_width	: self->parent->prev_virtual_width;
		int parent_prev_height	= anchor_viewport ? self->parent->prev_height	: self->parent->prev_virtual_height;
		int parent_width		= anchor_viewport ? self->parent->width			: self->parent->virtual_width; 
		int parent_height		= anchor_viewport ? self->parent->height		: self->parent->virtual_height;

		// If we're anchored to both the left and right part of the parent we position
		// based on the parents left pos 
		// (We will stretch to the right if the control is resizable and if the parent is resized).
		if ((self->anchor_flags & anchor_left) && !(self->anchor_flags & anchor_right))
		{
			self->absolute_x = parent_x + self->x;
		}
		else if ((self->anchor_flags & anchor_right) && !(self->anchor_flags & anchor_left))
		{
			self->absolute_x = parent_x + parent_width + (self->x - self->width);
		}

		if ((self->anchor_flags & anchor_top) && !(self->anchor_flags & anchor_bottom))
		{
			self->absolute_y = parent_y + self->y;
		}
		else if ((self->anchor_flags & anchor_bottom) && !(self->anchor_flags & anchor_top))
		{
			self->absolute_y = parent_y + parent_height + (self->y - self->height);
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
		child = (ez_control_t *)iter->payload;
		CONTROL_RAISE_EVENT(NULL, child, ez_control_t, OnMove);
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
	ez_control_t *child = NULL;
	ez_dllist_node_t *iter = self->children.head;

	self->absolute_virtual_x = self->absolute_x - self->virtual_x;
	self->absolute_virtual_y = self->absolute_y - self->virtual_y;

	// Tell the children we've scrolled. And make them recalculate their
	// absolute position by telling them they've moved.
	while(iter)
	{
		child = (ez_control_t *)iter->payload;
		CONTROL_RAISE_EVENT(NULL, child, ez_control_t, OnMove);
		CONTROL_RAISE_EVENT(NULL, child, ez_control_t, OnParentScroll);
		iter = iter->next;
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnScroll);

	return 0;
}

//
// Control - On parent scroll event.
//
int EZ_control_OnParentScroll(ez_control_t *self)
{
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnParentScroll);
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
// Control - Event for when a new event handler is set for an event.
//
int EZ_control_OnEventHandlerChanged(ez_control_t *self)
{
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnEventHandlerChanged);
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

	// TODO : Remove this test stuff.
	/*
	Draw_String(x, y, va("%s%s%s%s",
			((self->int_flags & control_moving) ? "M" : " "),
			((self->int_flags & control_focused) ? "F" : " "),
			((self->int_flags & control_clicked) ? "C" : " "),
			((self->int_flags & control_resizing_left) ? "R" : " ")
			));
	*/

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
	int width				= self->width;
	int height				= self->height;
	int x					= self->x;
	int y					= self->y;
	qbool resizing_width	= (direction & RESIZE_LEFT) || (direction & RESIZE_RIGHT);
	qbool resizing_height	= (direction & RESIZE_UP) || (direction & RESIZE_DOWN);

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
		if (resizing_width && !POINT_X_IN_CONTROL_DRAWBOUNDS(self->parent, ms->x))
		{
			ms->x = ms->x_old;
			x = self->x;
			width = self->width;
		}

		if (resizing_height && !POINT_Y_IN_CONTROL_DRAWBOUNDS(self->parent, ms->y))
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

	// Ignore the mouse?
	if (self->ext_flags & control_ignore_mouse)
	{
		return false;
	}

	mouse_delta_x = Q_rint(ms->x_old - ms->x);
	mouse_delta_y = Q_rint(ms->y_old - ms->y);

	mouse_inside		= POINT_IN_CONTROL_DRAWBOUNDS(self, ms->x, ms->y);
	prev_mouse_inside	= POINT_IN_CONTROL_DRAWBOUNDS(self, ms->x_old, ms->y_old);

	// Raise more specific events.
	if (mouse_inside)
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

		if (ms->button_down)
		{
			// Mouse down.
			CONTROL_RAISE_EVENT(&mouse_handled_tmp, self, ez_control_t, OnMouseDown, ms);
			mouse_handled = (mouse_handled || mouse_handled_tmp);
		}

		if (ms->button_up)
		{
			// Mouse up.
			CONTROL_RAISE_EVENT(&mouse_handled_tmp, self, ez_control_t, OnMouseUp, ms);
			mouse_handled = (mouse_handled || mouse_handled_tmp);
		}
	}
	else if (prev_mouse_inside)
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
			int m_delta_x = Q_rint(ms->x - ms->x_old);
			int m_delta_y = Q_rint(ms->y - ms->y_old);
			int x = self->x + m_delta_x;
			int y = self->y + m_delta_y;

			// Should the control be contained within it's parent?
			// Then don't allow the mouse to move outside the parent
			// while moving the control.
			if (CONTROL_IS_CONTAINED(self))
			{
				if (!POINT_X_IN_CONTROL_DRAWBOUNDS(self->parent, ms->x))
				{
					ms->x = ms->x_old;
					x = self->x;
					mouse_handled = true;
				}

				if (!POINT_Y_IN_CONTROL_DRAWBOUNDS(self->parent, ms->y))
				{
					ms->y = ms->y_old;
					y = self->y;
					mouse_handled = true;
				}
			}

			if (self->parent && (self->ext_flags & control_move_parent))
			{
				// Move the parent instead of just the control, the parent will in turn move the control.
				// TODO : Do we need to keep the mouse inside the parents parent here also?
				EZ_control_SetPosition(self->parent, (self->parent->x + m_delta_x), (self->parent->y + m_delta_y));
			}
			else
			{
				EZ_control_SetPosition(self, x, y);
			}

			mouse_handled = true;
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
	self->int_flags &= ~(/*control_moving |*/ control_mouse_over);

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

//
// Label - Creates a label control and initializes it.
//
ez_label_t *EZ_label_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
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

	EZ_label_Init(label, tree, parent, name, description, x, y, width, height, flags, text_flags, text);
	
	return label;
}

//
// Label - Initializes a label control.
//
void EZ_label_Init(ez_label_t *label, ez_tree_t *tree, ez_control_t *parent,
				  char *name, char *description,
				  int x, int y, int width, int height,
				  ez_control_flags_t flags, ez_label_flags_t text_flags,
				  char *text)
{
	// Initialize the inherited class first.
	EZ_control_Init(&label->super, tree, parent, name, description, x, y, width, height, flags);

	label->super.CLASS_ID				= EZ_LABEL_ID;

	// Overriden events.
	CONTROL_REGISTER_EVENT(label, EZ_label_OnDraw, OnDraw, ez_control_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnKeyDown, OnKeyDown, ez_control_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnKeyUp, OnKeyUp, ez_control_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnMouseDown, OnMouseDown, ez_control_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnMouseUp, OnMouseUp, ez_control_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnMouseHover, OnMouseHover, ez_control_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnResize, OnResize, ez_control_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_Destroy, OnDestroy, ez_control_t);

	// Label specific events.
	CONTROL_REGISTER_EVENT(label, EZ_label_OnTextChanged, OnTextChanged, ez_label_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnCaretMoved, OnCaretMoved, ez_label_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnTextScaleChanged, OnTextScaleChanged, ez_label_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnTextFlagsChanged, OnTextFlagsChanged, ez_label_t);

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

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDestroy, destroy_children);

	Q_free(label->text);

	// TODO: Can we just free a part like this here? How about children, will they be properly destroyed?
	EZ_control_Destroy(&label->super, destroy_children);

	EZ_eventhandler_Remove(label->event_handlers.OnCaretMoved, NULL, true);
	EZ_eventhandler_Remove(label->event_handlers.OnTextChanged, NULL, true);
	EZ_eventhandler_Remove(label->event_handlers.OnTextFlagsChanged, NULL, true);
	EZ_eventhandler_Remove(label->event_handlers.OnTextScaleChanged, NULL, true);

	return 0;
}

//
// Label - Sets the event handler for the OnTextChanged event.
//
void EZ_label_AddOnTextChanged(ez_label_t *label, ez_eventhandler_fp OnTextChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(label, EZ_CONTROL_HANDLER, OnTextChanged, ez_label_t, OnTextChanged, payload);
	CONTROL_RAISE_EVENT(NULL, label, ez_control_t, OnEventHandlerChanged);
}

//
// Label - Sets the event handler for the OnTextScaleChanged event.
//
void EZ_label_AddOnTextScaleChanged(ez_label_t *label, ez_eventhandler_fp OnTextScaleChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(label, EZ_CONTROL_HANDLER, OnTextScaleChanged, ez_label_t, OnTextScaleChanged, payload);
	CONTROL_RAISE_EVENT(NULL, label, ez_control_t, OnEventHandlerChanged);
}

//
// Label - Sets the event handler for the OnCaretMoved event.
//
void EZ_label_AddOnTextOnCaretMoved(ez_label_t *label, ez_eventhandler_fp OnCaretMoved, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(label, EZ_CONTROL_HANDLER, OnCaretMoved, ez_label_t, OnCaretMoved, payload);
	CONTROL_RAISE_EVENT(NULL, label, ez_control_t, OnEventHandlerChanged);
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

	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextScaleChanged);
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

	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextChanged);
}

//
// Label - Removes text between two given indexes.
//
void EZ_label_RemoveText(ez_label_t *label, int start_index, int end_index)
{
	clamp(start_index, 0, label->text_length);
	clamp(end_index, start_index, label->text_length);

	if (!label->text)
	{
		return;
	}

	memmove(
		label->text + start_index,								// Destination.
		label->text + end_index,								// Source.
		(label->text_length - end_index + 1) * sizeof(char));	// Size.
	
	label->text = Q_realloc(label->text, (strlen(label->text) + 1) * sizeof(char));

	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextChanged);
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

	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextChanged);
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

	CONTROL_EVENT_HANDLER_CALL(NULL, label, ez_label_t, OnTextScaleChanged);

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
		if (label->ext_flags & label_selectable)
		{
			if (((label->select_start > -1) && (label->select_end > -1)			// Is something selected at all?
				&& (label->select_end != label->select_start))					// Only highlight if we have at least 1 char selected.
				&& (((i >= label->select_start) && (i < label->select_end))		// Is this index selected?
					|| ((i >= label->select_end) && (i < label->select_start)))
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

	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnCaretMoved);
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
// Label - Gets the next word boundary.
//
static int EZ_label_GetNextWordBoundary(ez_label_t *label, int cur_pos, qbool forward)
{
	char *txt	= label->text;
	int dir		= forward ? 1 : -1;		

	// Make sure we're not out of bounds.
	clamp(cur_pos, 0, label->text_length);

	// Eat all the whitespaces.
	while ((cur_pos >= 0) && (cur_pos <= label->text_length) && ((txt[cur_pos] == ' ') || (txt[cur_pos] == '\n')))
	{
		cur_pos += dir;
	}
	
	// Find the next word boundary.
	while ((cur_pos >= 0) && (cur_pos <= label->text_length) && (txt[cur_pos] != ' ') && (txt[cur_pos] != '\n'))
	{
		cur_pos += dir;
	}

	return clamp(cur_pos, 0, label->text_length);
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
			if (isCtrlDown())
			{
				EZ_label_SetCaretPosition(label, EZ_label_GetNextWordBoundary(label, label->caret_pos.index, false));
			}
			else
			{
				EZ_label_SetCaretPosition(label, max(0, label->caret_pos.index - 1));
			}
			break;
		}
		case K_RIGHTARROW :
		{
			if (isCtrlDown())
			{
				EZ_label_SetCaretPosition(label, EZ_label_GetNextWordBoundary(label, label->caret_pos.index, true));
			}
			else
			{
				EZ_label_SetCaretPosition(label, label->caret_pos.index + 1);
			}
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

//
// Button - Recalculates and sets the position of the buttons label.
//
static void EZ_button_RecalculateLabelPosition(ez_button_t *button)
{
	ez_control_t *self				= ((ez_control_t *)button);
	ez_label_t *label				= button->text_label;
	ez_control_t *text_label_ctrl	= ((ez_control_t *)label);
	ez_textalign_t alignment		= button->text_alignment;
	int new_x						= text_label_ctrl->x;
	int new_y						= text_label_ctrl->y;
	int new_anchor					= EZ_control_GetAnchor(text_label_ctrl);

	// TODO : Hmm, should we even bothered with a special case for text alignment? Why not juse use ez_anchor_t stuff? Also remember to add support for middle_top and such.
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
static int EZ_button_OnLabelTextChanged(ez_control_t *self, void *payload)
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
	((ez_control_t *)button)->ext_flags	|= (flags | control_focusable | control_contained);

	// Override the draw function.
	CONTROL_REGISTER_EVENT(button, EZ_button_OnDraw, OnDraw, ez_control_t);
	CONTROL_REGISTER_EVENT(button, EZ_button_OnResize, OnResize, ez_control_t);

	// Button specific events.
	CONTROL_REGISTER_EVENT(button, EZ_button_OnAction, OnAction, ez_button_t);
	CONTROL_REGISTER_EVENT(button, EZ_button_OnTextAlignmentChanged, OnTextAlignmentChanged, ez_button_t);

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

		CONTROL_RAISE_EVENT(NULL, ((ez_control_t *)button), ez_control_t, OnResize);
	}

	EZ_button_SetNormalImage(button, EZ_BUTTON_DEFAULT_NORMAL_IMAGE);
	EZ_button_SetHoverImage(button, EZ_BUTTON_DEFAULT_HOVER_IMAGE);
	EZ_button_SetPressedImage(button, EZ_BUTTON_DEFAULT_PRESSED_IMAGE);

	button->ext_flags |= use_images | tile_edges | tile_center;
}

//
// Button - Destroys the button.
//
void EZ_button_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_button_t *button = (ez_button_t *)self;

	// TODO : Remove event handlers.
	// TODO : Can we just free a part like this here? How about children, will they be properly destroyed?
	EZ_control_Destroy(&button->super, destroy_children);

	EZ_eventhandler_Remove(button->event_handlers.OnAction, NULL, true);
	EZ_eventhandler_Remove(button->event_handlers.OnTextAlignmentChanged, NULL, true);
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
// Button - OnResize event handler.
//
int EZ_button_OnResize(ez_control_t *self)
{
	ez_button_t *button = (ez_button_t *)self;

	// Run the super class implementation.
	EZ_control_OnResize(self);

	EZ_button_RecalculateLabelPosition(button);

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnResize);

	return 0;
}

//
// Button - Use images for the button?
//
void EZ_button_SetUseImages(ez_button_t *button, qbool useimages)
{
	SET_FLAG(button->ext_flags, use_images, useimages);
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

	CONTROL_RAISE_EVENT(NULL, button, ez_button_t, OnTextAlignmentChanged);
}

//
// Button - Set the event handler for the OnTextAlignmentChanged event.
//
void EZ_button_AddOnTextAlignmentChanged(ez_button_t *button, ez_eventhandler_fp OnTextAlignmentChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(button, EZ_CONTROL_HANDLER, OnTextAlignmentChanged, ez_button_t, OnTextAlignmentChanged, payload);
	CONTROL_RAISE_EVENT(NULL, button, ez_control_t, OnEventHandlerChanged);
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
// Button - Set if the center of the button should be tiled or stretched.
//
void EZ_button_SetTileCenter(ez_button_t *button, qbool tilecenter)
{
	SET_FLAG(button->ext_flags, tile_center, tilecenter);
}

//
// Button - Set if the edges of the button should be tiled or stretched.
//
void EZ_button_SetTileEdges(ez_button_t *button, qbool tileedges)
{
	SET_FLAG(button->ext_flags, tile_edges, tileedges);
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
void EZ_button_AddOnAction(ez_button_t *self, ez_eventhandler_fp OnAction, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(self, EZ_CONTROL_HANDLER, OnAction, ez_button_t, OnAction, payload);
	CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnEventHandlerChanged);
}

//
// Button - Draw a button image.
//
static void EZ_button_DrawButtonImage(ez_button_t *button, mpic_t *pic)
{
	ez_control_t *self	= (ez_control_t *)button;

	int edge_size;		// The number of pixel from the edge of the texture 
						// to use when drawing the buttons edges.
	int edge_size2;
	int x, y;

	if (!pic)
	{
		return;
	}

	edge_size = Q_rint(0.1 * pic->width);
	edge_size2 = (2 * edge_size);

	EZ_control_GetDrawingPosition(self, &x, &y);

	// Center background.
	if (button->ext_flags & tile_center)
	{
		// Tiled.
		Draw_SubPicTiled((x + edge_size), (y + edge_size), 
			(self->width - edge_size2), (self->height - edge_size2),
			pic, 
			edge_size, edge_size, 
			(pic->width - edge_size2), (pic->height - edge_size2),
			1.0);
	}
	else
	{
		// Stretch the image.
		Draw_FitAlphaSubPic((x + edge_size), (y + edge_size), (self->width - edge_size2), (self->height - edge_size2), 
			pic, edge_size, edge_size, (pic->width - edge_size2), (pic->height - edge_size2), 1.0);
	}

	if (button->ext_flags & tile_edges)
	{
		// Tiled.

		// Top center.
		Draw_SubPicTiled((x + edge_size), y, (self->width - edge_size2), edge_size, 
						pic, edge_size, 0, (pic->width - edge_size2), edge_size, 1.0);

		// Bottom center.
		Draw_SubPicTiled((x + edge_size), (y + self->height - edge_size), (self->width - edge_size2), edge_size, 
						pic, edge_size, (pic->height - edge_size), (pic->width - edge_size2), edge_size, 1.0);
		
		// Left center.
		Draw_SubPicTiled(x, (y + edge_size), edge_size, (self->height - edge_size2), 
						pic, 0, edge_size, edge_size, (pic->height - edge_size2), 1.0);

		// Right center.
		Draw_SubPicTiled((x + self->width - edge_size), (y + edge_size), edge_size, (self->height - edge_size2), 
						pic, (pic->width - edge_size), edge_size, edge_size, (pic->height - edge_size2), 1.0);
	}
	else
	{
		// Stretched.

		// Top center.
		Draw_FitAlphaSubPic((x + edge_size), y, (self->width - edge_size), edge_size, 
							pic, edge_size, 0, (pic->width - edge_size2), edge_size, 1.0);

		// Bottom center.
		Draw_FitAlphaSubPic((x + edge_size), (y + self->height - edge_size), (self->width - edge_size), edge_size, 
							pic, edge_size, (pic->height - edge_size), (pic->width - edge_size2), edge_size, 1.0);

		// Left center.
		Draw_FitAlphaSubPic(x, (y + edge_size), edge_size, (self->height - edge_size2), 
							pic, 0, edge_size, edge_size, (pic->height - edge_size2), 1.0);

		// Right center.
		Draw_FitAlphaSubPic((x + self->width - edge_size), (y + edge_size), edge_size, (self->height - edge_size2), 
							pic, (pic->width - edge_size), edge_size, edge_size, (pic->height - edge_size2), 1.0);
	}

	// Top left corner.
	Draw_SSubPic(x, y, pic, 0, 0, edge_size, edge_size, 1.0);

	// Top right corner.
	Draw_SSubPic((x + self->width - edge_size), y, pic, (pic->width - edge_size), 0, edge_size, edge_size, 1.0);

	// Bottom left corner.
	Draw_SSubPic(x, (y + self->height - edge_size), pic, 0, (pic->height - edge_size), edge_size, edge_size, 1.0);

	// Bottom right corner.
	Draw_SSubPic((x + self->width - edge_size), (y + self->height - edge_size), 
				pic, 
				(pic->width - edge_size), (pic->height - edge_size), 
				edge_size, edge_size, 1.0);
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
	qbool useimages = (button->ext_flags & use_images);
	EZ_control_GetDrawingPosition(self, &x, &y);

	// Run the super class's implementation first.
	EZ_control_OnDraw(self);

	if (self->int_flags & control_mouse_over)
	{
		if (self->int_flags & control_clicked)
		{
			if (useimages)
			{
				EZ_button_DrawButtonImage(button, button->pressed_image);
			}
			else
			{
				Draw_AlphaFillRGB(x, y, self->width, self->height, RGBAVECT_TO_COLOR(button->color_pressed));
			}
		}
		else
		{
			if (useimages)
			{
				EZ_button_DrawButtonImage(button, button->hover_image);
			}
			else
			{
				Draw_AlphaFillRGB(x, y, self->width, self->height, RGBAVECT_TO_COLOR(button->color_hover));
			}
		}
	}
	else
	{
		if (useimages)
		{
			EZ_button_DrawButtonImage(button, button->normal_image);
		}
		else
		{
			Draw_AlphaFillRGB(x, y, self->width, self->height, RGBAVECT_TO_COLOR(button->color_normal));
		}
	}

	if (self->int_flags & control_focused)
	{
		if (useimages)
		{
		}
		else
		{
			Draw_AlphaRectangleRGB(x, y, self->width, self->height, 1, false, RGBAVECT_TO_COLOR(button->color_focused));
		}
	}

	// Draw control specifics.
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDraw);
	return 0;
}

//
// Button - OnTextAlignmentChanged event.
//
int EZ_button_OnTextAlignmentChanged(ez_control_t *self)
{
	ez_button_t *button = (ez_button_t *)self;

	EZ_button_RecalculateLabelPosition(button);

	CONTROL_EVENT_HANDLER_CALL(NULL, button, ez_button_t, OnTextAlignmentChanged);
	return 0;
}

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
void EZ_slider_AddOnSliderPositionChanged(ez_slider_t *slider, ez_eventhandler_fp OnSliderPositionChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(slider, EZ_CONTROL_HANDLER, OnSliderPositionChanged, ez_slider_t, OnSliderPositionChanged, payload);
	CONTROL_RAISE_EVENT(NULL, slider, ez_control_t, OnEventHandlerChanged);
}

//
// Slider - Event handler for OnMaxValueChanged.
//
void EZ_slider_AddOnMaxValueChanged(ez_slider_t *slider, ez_eventhandler_fp OnMaxValueChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(slider, EZ_CONTROL_HANDLER, OnMaxValueChanged, ez_slider_t, OnMaxValueChanged, payload);
	CONTROL_RAISE_EVENT(NULL, slider, ez_control_t, OnEventHandlerChanged);
}

//
// Slider - Event handler for OnMinValueChanged.
//
void EZ_slider_AddOnMinValueChanged(ez_slider_t *slider, ez_eventhandler_fp OnMinValueChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(slider, EZ_CONTROL_HANDLER, OnMinValueChanged, ez_slider_t, OnMinValueChanged, payload);
	CONTROL_RAISE_EVENT(NULL, slider, ez_control_t, OnEventHandlerChanged);
}

//
// Slider - Event handler for OnSliderPositionChanged.
//
void EZ_slider_AddOnScaleChanged(ez_slider_t *slider, ez_eventhandler_fp OnScaleChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(slider, EZ_CONTROL_HANDLER, OnScaleChanged, ez_slider_t, OnScaleChanged, payload);
	CONTROL_RAISE_EVENT(NULL, slider, ez_control_t, OnEventHandlerChanged);
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

	CONTROL_RAISE_EVENT(NULL, slider, ez_slider_t, OnSliderPositionChanged);
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

	CONTROL_RAISE_EVENT(NULL, (ez_control_t *)scrollbar, ez_control_t, OnResize);
}

//
// Scrollbar - Destroys the scrollbar.
//
int EZ_scrollbar_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_scrollbar_t *scrollbar = (ez_scrollbar_t *)self;
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDestroy, destroy_children);

	EZ_control_Destroy(self, destroy_children);

	EZ_eventhandler_Remove(scrollbar->event_handlers.OnTargetChanged, NULL, true);

	return 0;
}

//
// Scrollbar - Calculates and sets the parents scroll position based on where the slider button is.
//
static void EZ_scrollbar_CalculateParentScrollPosition(ez_scrollbar_t *scrollbar, ez_control_t *target)
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
	EZ_control_OnResize(self);

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
static int EZ_scrollbar_OnTargetScroll(ez_control_t *self, void *payload)
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
static int EZ_scrollbar_OnTargetResize(ez_control_t *self, void *payload)
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
	CONTROL_RAISE_EVENT(NULL, scrollbar, ez_scrollbar_t, OnTargetChanged);
}

//
// Scrollbar - The target of the scrollbar changed.
//
int EZ_scrollbar_OnTargetChanged(ez_control_t *self)
{
	ez_scrollbar_t *scrollbar = (ez_scrollbar_t *)self;

	if (scrollbar->target)
	{
		EZ_control_AddOnScroll(scrollbar->target, EZ_scrollbar_OnTargetScroll, scrollbar);
		EZ_control_AddOnResize(scrollbar->target, EZ_scrollbar_OnTargetResize, scrollbar);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, scrollbar, ez_scrollbar_t, OnTargetChanged);
	return 0;
}

//
// Scrollbar - OnResize event.
//
int EZ_scrollbar_OnResize(ez_control_t *self)
{
	ez_scrollbar_t *scrollbar = (ez_scrollbar_t *)self;

	// Let the super class do it's thing first.
	EZ_control_OnResize(self);

	// Make sure the buttons are in the correct place.
	EZ_scrollbar_RepositionScrollButtons(scrollbar);
	EZ_scrollbar_CalculateSliderSize(scrollbar, ((scrollbar->ext_flags & target_parent) ? self->parent : scrollbar->target));

	// Reset the min virtual size to 1x1 so that the 
	// scrollbar won't stop resizing when it's parent is resized.
	EZ_control_SetMinVirtualSize(self, 1, 1);

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnResize);
	return 0;
}

//
// Scrollbar - OnParentResize event.
//
int EZ_scrollbar_OnParentResize(ez_control_t *self)
{
	ez_scrollbar_t *scrollbar = (ez_scrollbar_t *)self;

	// Let the super class do it's thing first.
	EZ_control_OnParentResize(self);

	if (scrollbar->ext_flags & target_parent)
	{
		EZ_scrollbar_CalculateSliderSize(scrollbar, self->parent);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnParentResize);
	return 0;
}

//
// Scrollbar - OnMouseDown event.
//
int EZ_scrollbar_OnMouseDown(ez_control_t *self, mouse_state_t *ms)
{
	qbool  mouse_handled = false;
	ez_scrollbar_t *scrollbar	= (ez_scrollbar_t *)self;

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
				float scroll_ratio = 0;

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
int EZ_scrollbar_OnParentScroll(ez_control_t *self)
{
	ez_scrollbar_t *scrollbar = (ez_scrollbar_t *)self;

	// Super class.
	EZ_control_OnParentScroll(self);

	// Update the slider button to match the new scroll position.
	if (scrollbar->ext_flags & target_parent)
	{
		EZ_scrollbar_UpdateSliderBasedOnTarget(scrollbar, self->parent);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnParentScroll);
	return 0;
}

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

	EZ_control_SetSize(scrollpane->target, 
						(scrollpane_ctrl->width  - (show_v ? scrollpane->scrollbar_thickness : 0) - rh_size_h),
						(scrollpane_ctrl->height - (show_h ? scrollpane->scrollbar_thickness : 0) - rh_size_v));
}

//
// Scrollpane - Resize the scrollbars based on the targets state.
//
static void EZ_scrollpane_ResizeScrollbars(ez_scrollpane_t *scrollpane)
{
	ez_control_t *scrollpane_ctrl	= (ez_control_t *)scrollpane;
	ez_control_t *v_scroll_ctrl		= (ez_control_t *)scrollpane->v_scrollbar;
	ez_control_t *h_scroll_ctrl		= (ez_control_t *)scrollpane->h_scrollbar;

	qbool size_changed	= false;
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

	size_changed = (h_scroll_ctrl->prev_width != h_scroll_ctrl->width) || (h_scroll_ctrl->prev_height < h_scroll_ctrl->height) 
				|| (v_scroll_ctrl->prev_width != v_scroll_ctrl->width) || (v_scroll_ctrl->prev_height < v_scroll_ctrl->height);

	// Resize the target control so that it doesn't overlap with the scrollbars.
	if ((scrollpane->target && size_changed))
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

	EZ_scrollpane_ResizeScrollbars(scrollpane);
}

//
// Scrollpane - Target changed it's virtual size.
//
static int EZ_scrollpane_OnTargetVirtualResize(ez_control_t *self, void *payload)
{
	ez_scrollpane_t *scrollpane = NULL;

	if (!self->parent || (self->parent->CLASS_ID != EZ_SCROLLPANE_ID))
	{
		Sys_Error("EZ_scrollpane_OnTargetVirtualResize(): Target parent is not a scrollpane!.\n");
	}

	scrollpane = (ez_scrollpane_t *)self->parent;
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

	EZ_control_Destroy(self, destroy_children);

	EZ_eventhandler_Remove(scrollpane->event_handlers.OnScrollbarThicknessChanged, NULL, true);
	EZ_eventhandler_Remove(scrollpane->event_handlers.OnTargetChanged, NULL, true);

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
	CONTROL_RAISE_EVENT(NULL, scrollpane, ez_scrollpane_t, OnTargetChanged);
}

//
// Scrollpane - Set the thickness of the scrollbar controls.
//
void EZ_scrollpane_SetScrollbarThickness(ez_scrollpane_t *scrollpane, int scrollbar_thickness)
{
	scrollpane->scrollbar_thickness = scrollbar_thickness;
	CONTROL_RAISE_EVENT(NULL, scrollpane, ez_scrollpane_t, OnScrollbarThicknessChanged);
}

//
// Scrollpane - OnResize event.
//
int EZ_scrollpane_OnResize(ez_control_t *self)
{
	ez_scrollpane_t *scrollpane = (ez_scrollpane_t *)self;

	EZ_control_OnResize(self);

	// Resize the scrollbars and target based on the state of the target.
	EZ_scrollpane_ResizeScrollbars(scrollpane);

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnResize);
	return 0;
}

//
// Scrollpane - The target control changed.
//
int EZ_scrollpane_OnTargetChanged(ez_control_t *self)
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

		// Reposition the target inside the scrollpane.
		EZ_control_SetPosition(scrollpane->target, 0, 0);
		EZ_control_SetSize(scrollpane->target, self->width - scrollpane->scrollbar_thickness, self->height - scrollpane->scrollbar_thickness);
		EZ_control_SetAnchor(scrollpane->target, (anchor_left | anchor_right | anchor_top | anchor_bottom));

		// Make sure the target is drawn infront of the scrollpane.
		EZ_control_SetDrawOrder(scrollpane->target, ((ez_control_t *)scrollpane)->draw_order + 1, true);

		// When moving the target move the scrollpane with it
		// and don't allow moving the target inside of the scrollpane.
		EZ_control_SetMovesParent(scrollpane->target, true);

		// Resize the scrollbars / target to fit properly.
		EZ_scrollpane_ResizeScrollbars(scrollpane);
		EZ_scrollpane_AdjustTargetSize(scrollpane);

		CONTROL_RAISE_EVENT(NULL, (ez_control_t *)scrollpane->target, ez_control_t, OnResize);
		CONTROL_RAISE_EVENT(NULL, (ez_control_t *)scrollpane->target, ez_control_t, OnScroll);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, scrollpane, ez_scrollpane_t, OnTargetChanged);
	return 0;
}

//
// Scrollpane - The scrollbar thickness changed.
//
int EZ_scrollpane_OnScrollbarThicknessChanged(ez_control_t *self)
{
	ez_scrollpane_t *scrollpane = (ez_scrollpane_t *)self;

	// Resize the scrollbars and target with the new scrollbar thickness.
	EZ_scrollpane_ResizeScrollbars(scrollpane);

	CONTROL_EVENT_HANDLER_CALL(NULL, scrollpane, ez_scrollpane_t, OnScrollbarThicknessChanged);
	return 0;
}

// TODO : Add a checkbox control.
// TODO : Add a radiobox control.
// TODO : Add a window control.


