
#include "quakedef.h"

#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#endif // GLQUAKE

#include "keys.h"
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

//
// Double Linked List - Orders a list.
//
void EZ_double_linked_list_Order(ez_double_linked_list_t *list, PtFuncCompare compare_function)
{
	int i = 0;
	void *payload = NULL;
	ez_dllist_node_t *iter = NULL;
	void **items = (void **)Q_calloc(list->count, sizeof(void *));

	iter = list->head;
	
	while(iter)
	{
		items[i] = iter->payload;
		i++;

		iter = iter->next;
	}

	qsort(items, list->count, sizeof(void *), compare_function);

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
// Control Tree - Draws a control tree.
// 
void EZ_tree_Draw(ez_tree_t *tree)
{
	ez_control_t *payload = NULL;
	ez_dllist_node_t *iter = tree->drawlist.head;

	while(iter)
	{
		payload = (ez_control_t *)iter->payload;
		CONTROL_RAISE_EVENT(NULL, payload, OnDraw);
		
		iter = iter->next;
	}
}

//
// Control Tree - Dispatches a mouse event to a control tree.
//
int EZ_tree_MouseEvent(ez_tree_t *tree, mouse_state_t *ms)
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
	ez_control_t *payload = NULL;

	// FIXME: Not working properly

	if(tree->focused_node)
	{
		node_iter = (next_control) ? tree->focused_node->next : tree->focused_node->previous;

		// Find the next control that can be focused.
		while(node_iter && !found)
		{
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
	const ez_control_t *control1 = (ez_control_t *)val1;
	const ez_control_t *control2 = (ez_control_t *)val2;

	return (control2->draw_order - control1->draw_order);
}

//
// Control Tree - Orders the draw list based on the draw order property.
//
void EZ_tree_OrderDrawList(ez_tree_t *tree)
{
	EZ_double_linked_list_Order(&tree->drawlist, EZ_tree_DrawOrderFunc);
}

//
// Control - Tree Order function for the tab list based on the tab_order property.
//
static int EZ_tree_TabOrderFunc(const void *val1, const void *val2)
{
	const ez_control_t *control1 = (ez_control_t *)val1;
	const ez_control_t *control2 = (ez_control_t *)val2;

	return (control1->tab_order - control2->tab_order);
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
// Control - Initializes a control and adds it to the specified control tree.
//
ez_control_t *EZ_control_Init(ez_tree_t *tree, ez_control_t *parent, 
							  char *name, char *description, 
							  int x, int y, int width, int height, 
							  char *background_name, int flags)
{
	static int order = 0;
	ez_control_t *control = NULL;
	
	// We have to have a tree to add the control to.
	if(!tree)
	{
		return NULL;
	}
	
	control = (ez_control_t *)Q_malloc(sizeof(ez_control_t));

	control->control_tree = tree;
	control->name = name;
	control->description = description;
	control->flags = flags | CONTROL_ENABLED;
	control->draw_order = order++;

	control->resize_handle_thickness = 5;
	control->width_max = vid.conwidth;
	control->height_max = vid.conheight;
	control->width_min = 5;
	control->height_min = 5;

	// This should NEVER be changed.
	control->inheritance_level = EZ_CONTROL_INHERITANCE_LEVEL; 

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
	control->events.OnResize			= EZ_control_OnResize;
	
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

	EZ_double_linked_list_Add(&tree->drawlist, (void *)control);
	EZ_double_linked_list_Add(&tree->tablist, (void *)control);

	EZ_control_SetPosition(control, x, y);
	EZ_control_SetSize(control, width, height);

	return control;
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

	iter = self->children.head;

	// Destroy the children! 
	while(iter)
	{
		if(destroy_children)
		{
			// Destroy the child!
			//EZ_control_Destroy((ez_control_t *)iter->payload, destroy_children);
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

	// Cleanup any specifics this control may have.
	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnDestroy, destroy_children);

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
	self->width = width;
	self->height = height;

	// EZ_control_OnResize(self);
	CONTROL_RAISE_EVENT(NULL, self, OnResize);
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
// Control - Sets the position of a control.
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
	if(child->parent)
	{
		EZ_double_linked_list_RemoveByPayload(&child->parent->children, child);
	}

	child->parent = self;

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
	if(self->flags & CONTROL_FOCUSED)
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
	

	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnResize);

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
	self->absolute_x = (self->parent ? self->parent->absolute_x : 0) + self->x;
	self->absolute_y = (self->parent ? self->parent->absolute_y : 0) + self->y;

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
	#ifdef GLQUAKE

	// Make sure parts located outside the parent aren't drawn
	// when the control is contained within it's parent.
	if (self->parent && (self->flags & CONTROL_CONTAINED))
	{
		ez_control_t *p = self->parent;
		glEnable(GL_SCISSOR_TEST);
		glScissor(p->absolute_x, glheight - (p->absolute_y + p->height), p->width, p->height);
	}

	if (self->background_color[3] > 0.0)
	{
		Draw_AlphaRectangleRGB(self->absolute_x, self->absolute_y, self->width, self->height, 
			self->background_color[0] / 255.0,
			self->background_color[1] / 255.0,
			self->background_color[2] / 255.0,
			1, true, 
			self->background_color[3] / 255.0);

		Draw_String(self->absolute_x, self->absolute_y, va("%s%s%s%s", 
			((self->flags & CONTROL_MOVING) ? "M" : " "), 
			((self->flags & CONTROL_FOCUSED) ? "F" : " "), 
			((self->flags & CONTROL_CLICKED) ? "C" : " "), 
			((self->flags & CONTROL_RESIZING_LEFT) ? "R" : " ") 
			));
	}

	#else // SOFTWARE
	if (self->parent 
		&& (self->absolute_x >= self->parent->absolute_x) 
		&& (self->absolute_y >= self->parent->absolute_y)
		&& ((self->absolute_x + self->width) <= (self->parent->absolute_x + self->parent->width)
		&& ((self->absolute_y + self->height) <= (self->parent->absolute_y + self->parent->height)
	{
		// TODO: Draw the control normally in software.
	}
	else if (self->parent)
	{
		// Only draw the outline of the control when it's not completly inside
		// it's parent, since drawing only half of the control in software is not supported.
		Draw_Fill(self->absolute_x, self->absolute_y, self->width, 1, 0);
		Draw_Fill(self->absolute_x, self->absolute_y, 1, self->height, 0);
		Draw_Fill(self->absolute_x + self->width, self->absolute_y, 1, self->height, 0);
		Draw_Fill(self->absolute_x, self->absolute_y + self->height, self->width, 1, 0);
	}
	#endif // GLQUAKE

	// Draw control specifics.
	CONTROL_EVENT_HANDLER_CALL(NULL, self, OnDraw);

	#ifdef GLQUAKE
	glDisable(GL_SCISSOR_TEST);
	#endif // GLQUAKE

	return 0;
}



//
// Control - Key event.
//
int EZ_control_OnKeyEvent(ez_control_t *self, int key, int unichar)
{
	int key_handled = false;
	CONTROL_EVENT_HANDLER_CALL(&key_handled, self, OnKeyEvent, key, unichar);
	return key_handled;
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

	if (!ms)
	{
		Sys_Error("EZ_control_OnMouseEvent(): mouse_state_t NULL\n");
	}

	mouse_inside = POINT_IN_RECTANGLE(ms->x, ms->y, self->absolute_x, self->absolute_y, self->width, self->height);
	prev_mouse_inside = !POINT_IN_RECTANGLE(ms->x_old, ms->y_old, self->absolute_x, self->absolute_y, self->width, self->height);

	// If the control contained within it's parent,
	// the mouse must be within the parents bounds also for
	// the control to generate any mouse events.
	if (is_contained)
	{
		int p_x = self->parent->absolute_x;
		int p_y = self->parent->absolute_y;
		int p_w = self->parent->width;
		int p_h = self->parent->height;
		mouse_inside_parent = POINT_IN_RECTANGLE(ms->x, ms->y, p_x, p_y, p_w, p_h);
		prev_mouse_inside_parent =  !POINT_IN_RECTANGLE(ms->x_old, ms->y_old, p_x, p_y, p_w, p_h);
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
			CONTROL_RAISE_EVENT(&mouse_handled, self, OnMouseDown, ms);
		}
		
		if (ms->button_up && (ms->button_up != old_ms->button_up))
		{
			CONTROL_RAISE_EVENT(&mouse_handled, self, OnMouseUp, ms);
		}
	}
	else if((!is_contained && prev_mouse_inside) || (is_contained && prev_mouse_inside_parent))
	{
		CONTROL_RAISE_EVENT(&mouse_handled, self, OnMouseLeave, ms);
	}

	// TODO : Move these to new methods.

	if (self->flags & CONTROL_RESIZING_LEFT)
	{
		int mouse_delta = Round(ms->x_old - ms->x);
		int width = 0;
		int x = 0;

		// Set the new width based on how much the mouse has moved
		// keeping it within the allowed bounds.
		width = self->width + mouse_delta;
		clamp(width, self->width_min, self->width_max);

		// Move the control to counter act the resizing.
		x = self->x + (self->width - width);

		if (CONTROL_IS_CONTAINED(self))
		{
			if (MOUSE_OUTSIDE_PARENT_X(self, ms)) 
			{
				ms->x = ms->x_old;
				x = self->x;
				width = self->width;
			}
		}

		EZ_control_SetSize(self, width, self->height);
		EZ_control_SetPosition(self, x, self->y);

		mouse_handled = true;
	}
	else if (self->flags & CONTROL_MOVING)
	{
		// Root control will be moved relative to the screen,
		// others relative to their parent.
		int x = self->x + (ms->x - ms->x_old);
		int y = self->y + (ms->y - ms->y_old);

		// Should the control be contained within it's parent?
		// Then don't allow the mouse to move outside the parent
		// while moving the control.
		if (CONTROL_IS_CONTAINED(self))
		{
			if (MOUSE_OUTSIDE_PARENT_X(self, ms)) 
			{
				ms->x = ms->x_old;
				x = self->x;
			}
			
			if (MOUSE_OUTSIDE_PARENT_Y(self, ms))
			{
				ms->y = ms->y_old;
				y = self->y;
			}
		}

		mouse_handled = true;
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
	self->flags &= ~CONTROL_MOVING;

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
		CONTROL_RAISE_EVENT(NULL, self, OnMouseClick, mouse_state);
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




