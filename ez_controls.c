
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
		//EZ_control_OnDraw((ez_control_t *)iter->payload);
		if(payload->on_draw)
		{
			payload->on_draw(payload);
		}

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
	ez_dllist_node_t *iter = tree->drawlist.head;

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
// Control Tree - Key event.
//
qbool EZ_tree_KeyEvent(ez_tree_t *tree, int key, qbool down)
{
	if(tree->root)
	{
		return EZ_control_OnKeyEvent(tree->root, key, down);
	}

	return false;
}

//
// Control - Initializes a control.
//
ez_control_t *EZ_control_Init(char *name, char *description, int x, int y, int width, int height, char *background_name, int flags)
{
	ez_control_t *control = (ez_control_t *)Q_malloc(sizeof(ez_control_t));
	
	control->name = name;
	control->description = description;
	control->flags = flags;

	EZ_control_SetSize(control, x, y, width, height);
	
	if(background_name)
	{
		control->background = Draw_CachePicSafe(background_name, false, true);
	}

	return control;
}

//
// Control - Destroys a specified control.
//
void EZ_control_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_dllist_node_t *iter = self->children.head;
	ez_dllist_node_t *temp = iter;

	// Destroy the children! 
	while(iter)
	{
		if(destroy_children)
		{
			// Destroy the child.
			EZ_control_Destroy((ez_control_t *)iter->payload, destroy_children);
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
		self->on_destroy(self);
	}
}

//
// Control - Finds any orphans and adds them to the root control.
//
void EZ_control_UnOrphanizeChildren(ez_control_t *root_node, ez_tree_t *tree)
{
	ez_control_t *payload;
	ez_dllist_node_t *iter = tree->drawlist.head;

	while(iter)
	{
		payload = (ez_control_t *)iter->payload;
		
		if(!payload->parent)
		{
			EZ_double_linked_list_Add(&root_node->children, payload);
		}

		iter = iter->next;
	}
}

//
// Control - Sets the size of a control.
//
void EZ_control_SetSize(ez_control_t *self, int x, int y, int width, int height)
{
	self->x = x;
	self->y = y;
	self->width = width;
	self->height = height;
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
	if(self->parent)
	{
		EZ_double_linked_list_RemoveByPayload(&self->parent->children, child);
	}

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
void EZ_control_OnMove(ez_control_t *self, int x, int y)
{
	ez_control_t *payload = NULL;
	ez_dllist_node_t *iter = self->children.head;

	while(iter)
	{
		payload = (ez_control_t *)iter->payload;
		//EZ_control_OnMove(payload);

		if(self->on_move)
		{
			self->on_move(payload);
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
	if(down)
	{
		EZ_control_OnKeyDown(self, key);
	}
	else
	{
		EZ_control_OnKeyUp(self, key);
	}

	if(self->on_key)
	{
		self->on_key(self, key, down);
	}
}

//
// Control - Key released.
//
qbool EZ_control_OnKeyUp(ez_control_t *self, int key)
{
	if(self->on_key_up)
	{
		self->on_key_up(self, key);
	}
}

//
// Control - Key pressed.
//
qbool EZ_control_OnKeyDown(ez_control_t *self, int key)
{
	if(self->on_key_down)
	{
		self->on_key_down(self, key);
	}
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

	/*
	// Give any children the mouse info if it's inside of them.
	if(!mouse_handled)
	{
		ez_control_t *payload;
		ez_dllist_node_t *iter = &self->children.head;

		while(iter)
		{
			payload = (ez_control_t *)iter->payload;

			if(POINT_IN_RECTANGLE(ms->x, ms->y, payload->x, payload->y, payload->width, payload->height))
			{
				mouse_handled = EZ_control_OnMouseEvent(payload, ms);
			}

			if(mouse_handled)
			{
					break;
			}

			iter = iter->next;
		}
	}
	*/

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
		self->on_mouse_enter(self, mouse_state);
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
		self->on_mouse_leave(self, mouse_state);
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




