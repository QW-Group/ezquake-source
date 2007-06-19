
#include "quakedef.h"
#include "keys.h"
#include "draw.h"
//#include "string.h"
#include "ez_controls.h"

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
// Removes an item from a linked list by it's payload.
//
void *EZ_double_linked_list_RemoveByPayload(ez_double_linked_list_t *list, void *payload)
{
	ez_dllist_node_t *iter = list->head;

	while(iter)
	{
		if(iter->payload == payload)
		{
			return EZ_double_linked_list_Remove(list, iter);
		}

		iter = iter->next;
	}

	return NULL;
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
// Control Tree - Draws a control tree.
// 
void EZ_tree_Draw(ez_tree_t *tree)
{
	ez_control_t *payload = NULL;
	ez_dllist_node_t *iter = tree->drawlist.head;

	while(iter)
	{
		payload = (ez_control_t *)iter->payload;
		EZ_control_OnDraw(payload);
		
		iter = iter->next;
	}
}

//
// Control Tree - Dispatches a mouse event to a control tree.
//
qbool EZ_tree_MouseEvent(ez_tree_t *tree, mouse_state_t *ms)
{
	qbool mouse_handled = false;
	ez_control_t *payload = NULL;
	ez_dllist_node_t *iter = NULL;
	
	if(!tree)
	{
		assert(!"EZ_tree_MouseEvent: NULL tree reference.\n");
		return false;
	}
	
	iter = tree->drawlist.head;

	while(iter)
	{
		payload = (ez_control_t *)iter->payload;

		if(POINT_IN_RECTANGLE(ms->x, ms->y, payload->x, payload->y, payload->width, payload->height)
			&& payload->on_mouse)
		{
			//mouse_handled = EZ_control_OnMouseEvent(payload, ms);
			mouse_handled = payload->on_mouse(payload, ms);
		}

		if(mouse_handled)
		{
			return mouse_handled;
		}

		iter = iter->next;
	}

	return mouse_handled;
}

//
// Control Tree - Moves the focus to the next control in the control tree.
//
void EZ_tree_FocusNext(ez_tree_t *tree)
{
	qbool found = false;
	ez_control_t *payload = NULL;

	if(tree->focused_node && tree->focused_node->payload)
	{
		// Remove the focus flag from the currently focused control.
		payload = (ez_control_t *)tree->focused_node->payload;
		payload->flags &= ~CONTROL_FOCUSED;

		// Find the next focusable control.
		// (Search from the current focused control to the end of the tab list).
		tree->focused_node = tree->focused_node->next;
		while(tree->focused_node)
		{
			payload = (ez_control_t *)tree->focused_node->payload;

			if(payload->flags & CONTROL_FOCUSABLE)
			{
				found = true;
				break;
			}

			tree->focused_node = tree->focused_node->next;
		} 
	}
	
	// We haven't found a focusable control yet, 
	// or there was no focused control to start with.
	// So search for one from the start of the tab list.
	if(!found || !tree->focused_node)
	{
		tree->focused_node = tree->tablist.head;

		while(tree->focused_node)
		{
			payload = (ez_control_t *)tree->focused_node->payload;
			
			if(payload->flags & CONTROL_FOCUSABLE)
			{
				found = true;
				break;
			}

			tree->focused_node = tree->focused_node->next;
		}
	}

	// We found something new to focus on, so set the appropriate flags.
	if(tree->focused_node)
	{
		// All nodes have to have a control associated with it, otherwise something is wrong!
		if(!tree->focused_node->payload)
		{
			Sys_Error("EZ_tree_FocusNext(): Focused node has no control associated with it.\n");
		}

		((ez_control_t *)tree->focused_node->payload)->flags |= CONTROL_FOCUSED;
	}
}

//
// Control Tree - Key event.
//
qbool EZ_tree_KeyEvent(ez_tree_t *tree, int key, qbool down)
{
	if(tree && tree->root)
	{
		switch(key)
		{
			case K_TAB :
			{
				EZ_tree_FocusNext(tree);
				return true;
			}			
		}

		return EZ_control_OnKeyEvent(tree->root, key, down);
	}

	return false;
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
// Order function for the draw list based on the draw_order property.
//
static int EZ_tree_DrawOrderFunc(const void *val1, const void *val2)
{
	const ez_control_t *control1 = (ez_control_t *)val1;
	const ez_control_t *control2 = (ez_control_t *)val2;

	return (control1->draw_order - control2->draw_order);
}

//
// Control Tree - Orders the draw list based on the draw order property.
//
void EZ_tree_OrderDrawList(ez_tree_t *tree)
{
	EZ_double_linked_list_Order(&tree->drawlist, EZ_tree_DrawOrderFunc);
}

//
// Order function for the tab list based on the tab_order property.
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

	control->name = name;
	control->description = description;
	control->flags = flags;
	control->draw_order = order++;
	
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
void EZ_control_Destroy(ez_tree_t *tree, ez_control_t *self, qbool destroy_children)
{
	ez_dllist_node_t *iter = NULL; 
	ez_dllist_node_t *temp = NULL;

	if(!tree)
	{
		// Very bad!
		char *msg = "EZ_control_Destroy, tried to destroy control without a tree.\n";
		assert(!msg);
		Sys_Error(msg);
		return;
	}

	// Nothing to destroy :(
	if(!self)
	{
		return;
	}

	iter = self->children.head;

	// Destroy the children! 
	while(iter)
	{
		if(destroy_children)
		{
			// Destroy the child!
			EZ_control_Destroy(tree, (ez_control_t *)iter->payload, destroy_children);
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
	if(self->on_destroy)
	{
		self->on_destroy(tree, self, destroy_children);
	}

	Q_free(self);
}

//
// Control - Sets the tab order of a control.
//
void EZ_control_SetTabOrder(ez_tree_t *tree, ez_control_t *self, int tab_order)
{
	self->tab_order = tab_order;

	EZ_tree_OrderTabList(tree);
}

//
// Control - Sets the tab order of a control.
//
void EZ_control_SetDrawOrder(ez_tree_t *tree, ez_control_t *self, int draw_order)
{
	self->draw_order = draw_order;

	EZ_tree_OrderTabList(tree);
}

//
// Control - Sets the size of a control.
//
void EZ_control_SetSize(ez_control_t *self, int width, int height)
{
	self->width = width;
	self->height = height;
}

//
// Control - Set color of a control.
//
void EZ_control_SetBackgroundColor(ez_control_t *self, byte r, byte g, byte b, byte alpha)
{
	self->background_frame[0] = r;
	self->background_frame[1] = g;
	self->background_frame[2] = b;
	self->background_frame[3] = alpha;
}

//
// Control - Sets the position of a control.
//
void EZ_control_SetPosition(ez_control_t *self, int x, int y)
{
	self->x = x;
	self->y = y;

	if(self->parent)
	{
		/*
		self->absolute_x = self->parent->absolute_x + self->x;
		self->absolute_y = self->parent->absolute_y + self->y;
		*/

		// We moved ourselves, so send my parents position
		EZ_control_OnMove(self, self->parent->absolute_x, self->parent->absolute_y);
	}
	else
	{
		EZ_control_OnMove(self, 0, 0);
	}
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

// =========================================================================================
// Control - Base Event Handlers (Any inheriting controls can have their own implementation)
// =========================================================================================

//
// Control - The control got focus.
//
void EZ_control_OnGotFocus(ez_control_t *self)
{
	if(self->on_got_focus)
	{
		self->on_got_focus(self);
	}
}

//
// Control - The control lost focus.
//
void EZ_control_OnLostFocus(ez_control_t *self)
{
	if(self->on_lost_focus)
	{
		self->on_lost_focus(self);
	}
}

//
// Control - The control was moved.
//
void EZ_control_OnMove(ez_control_t *self, int parent_abs_x, int parent_abs_y)
{
	ez_control_t *payload = NULL;
	ez_dllist_node_t *iter = self->children.head;

	// Update the absolute screen position based on the parents position.
	self->absolute_x = parent_abs_x + self->x;
	self->absolute_y = parent_abs_y + self->y;

	while(iter)
	{
		payload = (ez_control_t *)iter->payload;
		EZ_control_OnMove(payload, self->absolute_x, self->absolute_y);

		if(self->on_move)
		{
			self->on_move(payload, self->absolute_x, self->absolute_y);
		}

		iter = iter->next;
	}
}

//
// Control - Layouts children.
//
void EZ_control_LayoutChildren(ez_control_t *self)
{
	if(self->on_layout_children)
	{
		self->on_layout_children(self);
	}
}

//
// Control - Draws the control.
//
void EZ_control_OnDraw(ez_control_t *self)
{
	#ifdef GLQUAKE
	if(self->background_frame[3] > 0.0)
	{
		Draw_AlphaRectangleRGB(self->absolute_x, self->absolute_y, self->width, self->height, 
			self->background_frame[0] / 255.0,
			self->background_frame[1] / 255.0,
			self->background_frame[2] / 255.0,
			1, true, 
			self->background_frame[3] / 255.0);
	}

	#else	
	if(self->parent 
		&& (self->absolute_x >= self->parent->absolute_x) 
		&& (self->absolute_y >= self->parent->absolute_y)
		&& ((self->absolute_x + self->width) <= (self->parent->absolute_x + self->parent->width)
		&& ((self->absolute_y + self->height) <= (self->parent->absolute_y + self->parent->height)
	{
		// TODO: Draw the control normally in software.
	}
	else
	{
		// Only draw the outline of the control when it's not completly inside
		// it's parent, since drawing only half of the control in software is not supported.
		Draw_Fill(self->absolute_x, self->absolute_y, self->width, 1, 0);
		Draw_Fill(self->absolute_x, self->absolute_y, 1, self->height, 0);
		Draw_Fill(self->absolute_x + self->width, self->absolute_y, 1, self->height, 0);
		Draw_Fill(self->absolute_x, self->absolute_y + self->height, self->width, 1, 0);
	}
	#endif

	// Draw control specifics.
	if(self->on_draw)
	{
		self->on_draw(self);
	}
}



//
// Control - Key event.
//
qbool EZ_control_OnKeyEvent(ez_control_t *self, int key, qbool down)
{
	qbool key_handled = false;

	if(down)
	{
		key_handled = EZ_control_OnKeyDown(self, key);
	}
	else
	{
		key_handled = EZ_control_OnKeyUp(self, key);
	}

	if(self->on_key)
	{
		key_handled = self->on_key(self, key, down);
	}

	return key_handled;
}

//
// Control - Key released.
//
qbool EZ_control_OnKeyUp(ez_control_t *self, int key)
{
	if(self->on_key_up)
	{
		return self->on_key_up(self, key);
	}

	return false;
}

//
// Control - Key pressed.
//
qbool EZ_control_OnKeyDown(ez_control_t *self, int key)
{
	if(self->on_key_down)
	{
		return self->on_key_down(self, key);
	}

	return false;
}

#define POINT_IN_RECTANGLE(p_x, p_y, r_x, r_y, r_width, r_height) ((p_x >= r_x) && (p_y >= r_y) && (p_x <= (r_x + r_width)) && (p_y <= (r_y + r_height)))

//
// Control -
// The initial mouse event is handled by this, and then raises more specialized event handlers
// based on the new mouse state.
//
qbool EZ_control_OnMouseEvent(ez_control_t *self, mouse_state_t *ms)
{
	mouse_state_t *old_ms = &self->prev_mouse_state;
	qbool mouse_inside = false;
	qbool prev_mouse_inside = false;
	qbool mouse_handled = false;

	if(!ms)
	{
		return mouse_handled;
	}

	mouse_inside = POINT_IN_RECTANGLE(ms->x, ms->y, self->x, self->y, self->width, self->height);
	prev_mouse_inside = !POINT_IN_RECTANGLE(old_ms->x, old_ms->y, self->x, self->y, self->width, self->height);

	if(mouse_inside)
	{
		if(prev_mouse_inside)
		{
			// Were we inside of the control last time? Otherwise we've just entered it.
			EZ_control_OnMouseEnter(self, ms);
		}
		else
		{
			// We're hovering the control.
			EZ_control_OnMouseHover(self, ms);
		}

		if(ms->button_down != old_ms->button_down)
		{
			mouse_handled = EZ_control_OnMouseDown(self, ms);
		}
		
		if(ms->button_up != old_ms->button_up)
		{
			mouse_handled = EZ_control_OnMouseUp(self, ms);
		}
	}
	else if(prev_mouse_inside)
	{
		if(!mouse_inside)
		{
			mouse_handled = EZ_control_OnMouseLeave(self, ms);
		}
	}

	if(self->on_mouse)
	{
		mouse_handled = self->on_mouse(self, ms);
	}

	// Save the mouse state for the next time we check.
	self->prev_mouse_state = *ms;

	return mouse_handled;
}

//
// Control - The mouse was pressed and then released within the bounds of the control.
//
qbool EZ_control_OnMouseClick(ez_control_t *self, mouse_state_t *mouse_state)
{
	return false;
}

//
// Control - The mouse entered the controls bounds.
//
qbool EZ_control_OnMouseEnter(ez_control_t *self, mouse_state_t *mouse_state)
{
	if(self->on_mouse_enter)
	{
		return self->on_mouse_enter(self, mouse_state);
	}

	return false;
}

//
// Control - The mouse left the controls bounds.
//
qbool EZ_control_OnMouseLeave(ez_control_t *self, mouse_state_t *mouse_state)
{
	if(self->on_mouse_leave)
	{
		return self->on_mouse_leave(self, mouse_state);
	}

	return false;
}

//
// Control - A mouse button was released within the bounds of the control.
//
qbool EZ_control_OnMouseUp(ez_control_t *self, mouse_state_t *mouse_state)
{
	// Stop moving.
	self->flags &= ~CONTROL_MOVING;

	if(self->on_mouse_up)
	{
		return self->on_mouse_up(self, mouse_state);
	}

	return false;
}

//
// Control - A mouse button was pressed within the bounds of the control.
//
qbool EZ_control_OnMouseDown(ez_control_t *self, mouse_state_t *mouse_state)
{
	if(!(self->flags & CONTROL_ENABLED))
	{
		return false;
	}
	
	// The control is being moved.
	if(!(self->flags & CONTROL_MOVABLE) && mouse_state->button_down == 1)
	{
		self->flags |= CONTROL_MOVING;
	}

	if(self->on_mouse_down)
	{
		return self->on_mouse_down(self, mouse_state);
	}

	return false;
}

//
// Control - The mouse wheel was triggered within the bounds of the control.
//
qbool EZ_control_OnMouseWheel(ez_control_t *self, mouse_state_t *mouse_state)
{
	return false;
}

//
// Control - The mouse is hovering within the bounds of the control.
//
qbool EZ_control_OnMouseHover(ez_control_t *self, mouse_state_t *mouse_state)
{
	if(self->on_mouse_hover)
	{
		return self->on_mouse_hover(self, mouse_state);
	}

	return false;
}




