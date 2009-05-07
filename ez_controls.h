#ifndef __EZ_CONTROLS_H__
#define __EZ_CONTROLS_H__
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

$Id: ez_controls.h,v 1.55 2007-10-27 14:51:15 cokeman1982 Exp $
*/

//
// Description
//
// Basic design princilples
// ------------------------
// The base design principle for the framework is a "light" version of Object Orientated Programming (OOP).
// That is, all controls are regarded as objects that has methods associated with them that are used
// to manipulate them and change their inner state.
//
// Implementation
// --------------
// Each "class" in the framework consists of a struct and a set of methods, with the following name schemes:
// Struct: ez_nameofobject_t
// Method: EZ_name_of_object_NameOfMethod(...)
// 
// All methods that are associated with a particular object ALWAYS has a pointer to the object's struct type as the first
// argument of the function, that is:
// EZ_nameofobject_NameOfMethod(ez_nameofobject_t *self, ...);
//
// This means that "self" in this case can be regarded as the keyword "this" usually used in OOP languages.
// This way only objects that are of the type ez_nameofobject_t are allowed to run the method. A NULL pointer
// must never be passed to a method.
//
// Each object has the following types of methods:
//
// - Create method:
//   The create method allocates the memory of the object and then passes it on to the Init method.
//
// - Init method:
//   The init method initializes all the variables of the object and sets all the function pointers
//   for the different methods.
//
// - Set methods: 
//   Used to set properties of the object, such as changing the inner state of the object struct.
//
// - Get methods: 
//   Gets inner state from the object.
//
// - Event implementations: 
//   This is a core implementation for the object, such as OnDraw, that draws the control which will be 
//   called on all objects by the framework. An object must have an implementation for all of the events
//   that are associated with it. An event is raised using CONTROL_RAISE_EVENT when something happens
//   inside of the object, such as the width changes or a key is pressed.
//   (See the comment for the CONTROL_RAISE_EVENT macro for more detail on how this works).
//
// - Event handlers: 
//   These are optional functions that are set by the user of the framework, these can be set for each of 
//   the events described above. For instance, this can be used for running some specified function when 
//   a button is pressed. These are only run after all event implementations for that particular event
//   has been run in the class tree (see inheritance).
//   (See the comment for CONTROL_EVENT_HANDLER_CALL for more detail on how this works).
//
// The events and event handlers associated with an object are specified in a separate struct named
// "ez_nameofobject_events_t", which contains a set of function pointers to the different events associated
// with the object type. Each object then has two variables of this struct type in it named "events"
// and "event_handlers". All the function pointers in the events variable has to be set in the objects
// init function, but the function pointers in the "event_handlers" variable doesn't need to be set at all,
// they're set by the user of the object when needed.
//
// NOTICE! It is important that the contents of the object structs are NEVER changed directly, but always
// to the methods specified for the object. This is an important principle in OOP programming, hiding
// internal information from the outside world. Changing struct information directly might lead to crucial
// events not being raised which in turn puts the object in an undefined state.
//
// Inheritance
// -----------
// An object in the framework can inherit functionality from other objects, and override / reimplement their
// event methods (actually, more like extending than overriding to be precise). This is achieved by creating
// a new object, and at the beginning of the objects struct we place the object of the type we want to inherit from.
// That is:
//
// typedef struct ez_object2_s
// {
//		ez_object1_t super;
//		...
// } ez_object2_t;
//
// It is now possible to cast ez_object2_t to ez_object1_t, and use it with any object1 methods:
// ez_object2_t obj2 = EZ_object2_Create(...);
// EZ_object1_SomeObject1Method((ez_object1_t *)obj2);
//
// To be able to call all the methods of a particular event implementation involving inheritance, and the only
// run the event handler associated with that event ONCE at the end. That is, first let the control change it's
// internal state, and then finally signaling the outside world. The macros CONTROL_RAISE_EVENT and 
// CONTROL_EVENT_HANDLER_CALL are used, see the comment for them below for more detailed information on how they work.
//
// Each object struct contains a CLASS_ID used to identify what type of object it is, this should be unique for
// each object type. This is set at init of the control, and should never be changed after that.
//
// Control objects (ez_control_t / EZ_control_*)
// ---------------------------------------------
// Building on the principles described above the base object for all controls in the UI framework is defined
// with the ez_control_t struct and EZ_control_* methods. This object implements all the base functionality that
// are needed for a control, such as:
// - Mouse handling
//   * Each control gets an initial event whenever the mouse state changes in some way, 
//     mouse moved, or a button was pressed.
//   * The event is processed and if the mouse was inside the control and a button was pressed for instance
//     a more detailed event is raised, targeted only to that control. Examples of this:
//     OnMouseDown, OnMouseUp, OnMouseHover, OnMouseEnter, OnMouseLeave...
// - Key events
// - Moving
// - Resizing
// - Scrolling
//   * Each control has a visible area, and a "virtual" area, which can be scrolled within.
//
// The ez_control_t object also contains a list of children, sub-controls that are placed inside of that control
//
// All controls then inherits from this object, and extends the events it has defined, or adds new events.
//
// Control tree
// ------------
// To tie all these controls together, a root object exists called ez_tree_t. This tree has a pointer to
// the root control of the tree, which all of the other controls are contained within (this control usually
// has the same size as the screen). It also contains two lists, one is the draw list which holds the
// order in which the controls should be drawn in. The other is the tab list, which holds the order that
// the controls can be "tabbed" between (if the control is focusable).
//
// All instances of a control MUST be associated with a control tree that is passed to the creation function
// of the control.
//
// All input is sent to the control tree, which in turn propagates them to the controls that should have it.
// The control tree is also responsible for raising the OnDraw event for all the controls (in the order
// specified in it's draw list).
//
// There can be any number of control trees running at the same time, but the intended practice is to
// only use one at any given time.
//
// -- Cokeman 2007
//

#define POINT_IN_RECTANGLE(p_x, p_y, r_x, r_y, r_width, r_height) ((p_x >= r_x) && (p_y >= r_y) && (p_x <= (r_x + r_width)) && (p_y <= (r_y + r_height)))
#define POINT_X_IN_BOUNDS(p_x, left, right)	((p_x >= left) && (p_x <= right))
#define POINT_Y_IN_BOUNDS(p_y, top, bottom)	((p_y >= top)  && (p_y <= bottom))
#define POINT_IN_BOUNDS(p_x, p_y, left, right, top, bottom)	(POINT_X_IN_BOUNDS(p_x, left, right) && POINT_Y_IN_BOUNDS(p_y, top, bottom))

#define POINT_X_IN_CONTROL_DRAWBOUNDS(ctrl, p_x) (POINT_X_IN_BOUNDS(p_x, (ctrl)->bound_left, (ctrl)->bound_right))
#define POINT_Y_IN_CONTROL_DRAWBOUNDS(ctrl, p_y) (POINT_Y_IN_BOUNDS(p_y, (ctrl)->bound_top, (ctrl)->bound_bottom))
#define POINT_IN_CONTROL_DRAWBOUNDS(ctrl, p_x, p_y) (POINT_X_IN_CONTROL_DRAWBOUNDS(ctrl, p_x) && POINT_Y_IN_CONTROL_DRAWBOUNDS(ctrl, p_y))
#define POINT_IN_CONTROL_RECT(ctrl, p_x, p_y) POINT_IN_RECTANGLE(p_x, p_y, (ctrl)->absolute_x, (ctrl)->absolute_y, (ctrl)->width, (ctrl)->height)

#define SET_FLAG(flag_var, flag, on) ((flag_var) = ((on) ? ((flag_var) | (flag)) : ((flag_var) & ~(flag))))

typedef enum ez_control_id_e
{
	EZ_CONTROL_ID,
	EZ_BUTTON_ID,
	EZ_LABEL_ID,
	EZ_SLIDER_ID,
	EZ_SCROLLBAR_ID,
	EZ_SCROLLPANE_ID,
	EZ_WINDOW_ID,
	EZ_LISTVIEW_ID,
	EZ_LISTVIEWITEM_ID
} ez_control_id_t;

// =========================================================================================
// Double Linked List
// =========================================================================================

// 
// Double Linked List - Function pointer types.
//
typedef int (* PtFuncCompare)(const void *, const void *);

typedef struct ez_dllist_node_s
{
	struct ez_dllist_node_s *next;
	struct ez_dllist_node_s *previous;
	void *payload;
} ez_dllist_node_t;

typedef struct ez_double_linked_list_s
{
	int count;
	ez_dllist_node_t *head;
	ez_dllist_node_t *tail;
} ez_double_linked_list_t;

//
// Double Linked List - Add item to double linked list.
//
void EZ_double_linked_list_Add(ez_double_linked_list_t *list, void *payload);

//
// Double Linked List - Finds a given node based on the specified payload.
//
ez_dllist_node_t *EZ_double_linked_list_FindByPayload(const ez_double_linked_list_t *list, const void *payload);

//
// Double Linked List - Find a item by its index.
//
ez_dllist_node_t *EZ_double_linked_list_FindByIndex(const ez_double_linked_list_t *list, int index);

//
// Double Linked List - Removes an item from a linked list by its index.
//
void *EZ_double_linked_list_RemoveByIndex(ez_double_linked_list_t *list, int index);

//
// Double Linked List - Removes an item from a linked list by its payload.
//
void *EZ_double_linked_list_RemoveByPayload(ez_double_linked_list_t *list, void *payload);

//
// Double Linked List - A function pointer to a cleanup function that is run on each item when removing a range of items.
//
typedef void (*ez_removerange_cleanup_function_t)(void *payload);

//
// Double Linked List - Removes a range of items in the list. 
//						A cleanup function needs to be supplied so that the payload of the item is also cleaned up.
//
void EZ_double_linked_list_RemoveRange(ez_double_linked_list_t *list, int start, int end, ez_removerange_cleanup_function_t func);

//
// Double Linked List - Removes the first occurance of the item from double linked list and returns its payload.
//
void *EZ_double_linked_list_Remove(ez_double_linked_list_t *list, ez_dllist_node_t *item);

//
// Double Linked List - Orders a list.
//
void EZ_double_linked_list_Sort(ez_double_linked_list_t *list, PtFuncCompare compare_function);

// =========================================================================================
// Control Tree
// =========================================================================================

typedef struct ez_tree_s
{
	struct ez_control_s		*root;					// The control tree.
	ez_dllist_node_t		*focused_node;			// The node of focused control (from the tablist). 
	ez_double_linked_list_t	drawlist;				// A list with the controls ordered in their drawing order.
	ez_double_linked_list_t	tablist;				// A list with the controls ordered in their tabbing order.

	mouse_state_t			prev_mouse_state;		// The last mouse state we received.
	double					mouse_pressed_time[9];	// How long the mouse buttons have been pressed. (Used for repeating mouse click events).
} ez_tree_t;

//
// Control Tree - Orders the tab list based on the tab order property.
//
void EZ_tree_OrderTabList(ez_tree_t *tree);

//
// Control Tree - Orders the draw list based on the draw order property.
//
void EZ_tree_OrderDrawList(ez_tree_t *tree);

//
// Control Tree - Needs to be called every frame to keep the tree alive.
//
void EZ_tree_EventLoop(ez_tree_t *tree);

//
// Control Tree - Dispatches a mouse event to a control tree.
//
qbool EZ_tree_MouseEvent(ez_tree_t *tree, mouse_state_t *ms);

//
// Control Tree - Key event.
//
qbool EZ_tree_KeyEvent(ez_tree_t *tree, int key, int unichar, qbool down);

//
// Tree Control - Finds any orphans and adds them to the root control.
//
void EZ_tree_UnOrphanizeChildren(ez_tree_t *tree);

//
// Control Tree - Refreshes the position of all controls in the tree.
//
void EZ_tree_Refresh(ez_tree_t *tree);

//
// Control Tree - Destroys a tree. Will not free the memory for the tree.
//
void EZ_tree_Destroy(ez_tree_t *tree);

// =========================================================================================
// Event handler 
// =========================================================================================

//
// Event function pointer types.
//
typedef int (*ez_event_fp) (struct ez_control_s *self, void *ext_event_info);
typedef int (*ez_mouse_event_fp) (struct ez_control_s *self, mouse_state_t *mouse_state);
typedef int (*ez_key_event_fp) (struct ez_control_s *self, int key, int unichar, qbool down);
typedef int (*ez_keyspecific_event_fp) (struct ez_control_s *self, int key, int unichar);
typedef int (*ez_destroy_event_fp) (struct ez_control_s *self, qbool destroy_children);
typedef int (*ez_child_event_fp) (struct ez_control_s *self, struct ez_control_s *child);

//
// Event handlers function pointer types (same as event function types, except they have a payload also).
//
typedef int (*ez_eventhandler_fp) (struct ez_control_s *self, void *payload, void *ext_info);
typedef int (*ez_mouse_eventhandler_fp) (struct ez_control_s *self, void *payload, mouse_state_t *mouse_state);
typedef int (*ez_key_eventhandler_fp) (struct ez_control_s *self, void *payload, int key, int unichar, qbool down);
typedef int (*ez_keyspecific_eventhandler_fp) (struct ez_control_s *self, void *payload, int key, int unichar);
typedef int (*ez_destroy_eventhandler_fp) (struct ez_control_s *self, void *payload, qbool destroy_children);
typedef int (*ez_child_eventhandler_event_fp) (struct ez_control_s *self, void *payload, struct ez_control_s *child);

#define EZ_CONTROL_HANDLER			0
#define EZ_CONTROL_MOUSE_HANDLER	1
#define EZ_CONTROL_KEY_HANDLER		2
#define EZ_CONTROL_KEYSP_HANDLER	3
#define EZ_CONTROL_DESTROY_HANDLER	4
#define EZ_CONTROL_CHILD_HANDLER	5

typedef union ez_eventhandlerfunction_u
{
	ez_eventhandler_fp				normal;
	ez_mouse_eventhandler_fp		mouse;
	ez_key_eventhandler_fp			key;
	ez_keyspecific_eventhandler_fp	key_sp;
	ez_destroy_eventhandler_fp		destroy;
	ez_child_eventhandler_event_fp	child;
} ez_eventhandlerfunction_t;

typedef struct ez_eventhandler_s
{
	int							function_type;
	ez_eventhandlerfunction_t	function;
	void						*payload;
	struct ez_eventhandler_s	*next;
} ez_eventhandler_t;

#define CONTROL_ADD_EVENTHANDLER(ctrl, func_type, eventfunc, eventroot, eventname, payload)	\
{																							\
	ez_eventhandler_t *e = EZ_eventhandler_Create((void *)eventfunc, func_type, payload);	\
	eventroot *c = (eventroot *)ctrl;														\
	if (c->event_handlers.eventname)														\
		e->next = c->event_handlers.eventname;												\
	c->event_handlers.eventname = e;														\
}

#define CONTROL_REMOVE_EVENTHANDLER(ctrl, func, eventroot, eventname)			\
{																				\
	eventroot *c = (eventroot *)ctrl;											\
	EZ_eventhandler_Remove(c->event_handlers.eventname, func, false);			\
}

//
// Eventhandler - Goes through the list of events and removes the one with the specified function.
//
void EZ_eventhandler_Remove(ez_eventhandler_t *eventhandler, void *event_func, qbool all);

//
// Eventhandler - Creates a eventhandler.
//
ez_eventhandler_t *EZ_eventhandler_Create(void *event_func, int func_type, void *payload);

// =========================================================================================
// Control
// =========================================================================================

#define CONTROL_IS_CONTAINED(self) (self->parent && (self->ext_flags & control_contained))

//
// Raises an event. (See the CONTROL_EVENT_HANDLER_CALL for a more detailed explination of how this works).
//
#ifdef __INTEL_COMPILER

#define CONTROL_RAISE_EVENT(retval, ctrl, eventroot, eventhandler, ...)											\
{																												\
	int temp = 0;																								\
	int *p = (int *)retval;																						\
	eventroot *c = (eventroot *)(ctrl);																			\
	c->override_counts.eventhandler = c->inherit_levels.eventhandler;											\
	if(!c->events.eventhandler)																					\
		Sys_Error("CONTROL_RAISE_EVENT : "#ctrl" ("#eventroot") has no event function for "#eventhandler);		\
	temp = c->events.eventhandler((ez_control_t *)(ctrl), __VA_ARGS__);											\
	if(p) (*p) = temp;																							\
}																																																	

#else

#define CONTROL_RAISE_EVENT(retval, ctrl, eventroot, eventhandler, ...)											\
{																												\
	int temp = 0;																								\
	int *p = (int *)retval;																						\
	eventroot *c = (eventroot *)(ctrl);																			\
	c->override_counts.eventhandler = c->inherit_levels.eventhandler;											\
	if(!c->events.eventhandler)																					\
		Sys_Error("CONTROL_RAISE_EVENT : "#ctrl" ("#eventroot") has no event function for "#eventhandler);		\
	temp = c->events.eventhandler((ez_control_t *)(ctrl), ##__VA_ARGS__);										\
	if(p) (*p) = temp;																							\
}

#endif // __INTEL_COMPILER

//
// Calls the event handler associated with an event after all controls have run their implementation of the event.
//
// Example: OnMouseUp defined in ez_control_t
// 1. Run EZ_control_OnMouseUp
// 2. Run EZ_label_OnMouseUp (Set in ctrl->events.OnMouseUp, which in turn MUST call EZ_control_OnMouseUp
//    the first thing it does).
// 3. Run any specified event handler associated with the OnMouseUp event. ctrl->event_handlers.OnMouseUp
//
// This works by having an "inheritance level" and "override count" in each control struct:
// Inheritance level:
// ------------------
// ez_control_t = 0
// ez_label_t	= 1
//
// When an initial event is raised using the CONTROL_RAISE_EVENT macro, the "override count" for the struct 
// is reset to the value of the "inheritance level" (which is always the same). Then each time that
// the CONTROL_EVENT_HANDLER_CALL is called, first in the ez_control_t implementation of the event
// and then in the ez_label_t implementation, the "override count" is decremented, and when it reaches
// ZERO the event handler (if there's one set) will be run.
//
// If a control such as ez_label_t implements new events OnTextChanged for example, any class inheriting
// from ez_label_t will have to have a inheritance level relative to ez_label_t also.
// Example:
// ez_control_t inheritance level:			0				1				2
// ez_label_t	inheritance level:			-				0				1
// Inheritance:							ez_control_t <- ez_label_t <- ez_crazylabel_t
//
// ez_crazylabel_t *crazylabel = CREATE(...);
// ((ez_control_t *)crazylabel)->inheritance_level = 2; // Relative to ez_control_t
// ((ez_label_t *)  crazylabel)->inheritance_level = 1;	// Relative to ez_label_t
//
// So if I now want to override OnTextChanged specified in ez_label_t for ez_crazylabel_t, the 
// ez_label_t inheritance level is used by the macro. And if I override OnMouseUp, the ez_control_t
// inheritance level is used instead.
//
// retval		= An integer pointer to the var that should receive the return value of the event handler.
// ctrl			= The control struct to call the event handler on.
// eventroot	= The name of the control struct that has the "root implementation" of the specified event.
// eventhandler = The name of the event that should be called.
//
#ifdef __INTEL_COMPILER

#define CONTROL_EVENT_HANDLER_CALL(retval, ctrl, eventroot, evnthndler, ...)				\
{																							\
	int *p = (int *)retval, temp = 0;														\
	eventroot *c = (eventroot *)ctrl;														\
	if (c->event_handlers.evnthndler)														\
	{																						\
		if (c->override_counts.evnthndler == 1)												\
		{																					\
			ez_eventhandler_t *e = c->event_handlers.evnthndler;							\
			while (e)																		\
			{																				\
				EZ_eventhandler_Exec(e, (ez_control_t *)ctrl, __VA_ARGS__);					\
				e = e->next;																\
			}																				\
		}																					\
		else																				\
			c->override_counts.evnthndler--;												\
	}																						\
	if(p) (*p) = temp;																		\
}


#else

#define CONTROL_EVENT_HANDLER_CALL(retval, ctrl, eventroot, evnthndler, ...)				\
{																							\
	int *p = (int *)retval, temp = 0;														\
	eventroot *c = (eventroot *)ctrl;														\
	if (c->event_handlers.evnthndler)														\
	{																						\
		if (c->override_counts.evnthndler == 1)												\
		{																					\
			ez_eventhandler_t *e = c->event_handlers.evnthndler;							\
			while (e)																		\
			{																				\
				EZ_eventhandler_Exec(e, (ez_control_t *)ctrl, ##__VA_ARGS__);				\
				e = e->next;																\
			}																				\
		}																					\
		else																				\
			c->override_counts.evnthndler--;												\
	}																						\
	if(p) (*p) = temp;																		\
}

#endif // __INTEL_COMPILER

//
// Registers a event function an event and sets it's inheritance level.
// ctrl			= The control that the event function should be registered to.
// eventfunc	= The function implementing the event. (Ex. EZ_control_OnMouseDown)
// eventname	= The name of the event. (Ex. OnMouseDown)
// eventroot	= The control that initially defined this event. (Ex. ez_control_t)
//
#define CONTROL_REGISTER_EVENT(ctrl, eventfunc, eventname, eventroot)	\
{																		\
	((eventroot *)ctrl)->events.eventname = eventfunc;					\
	((eventroot *)ctrl)->inherit_levels.eventname++;					\
}

typedef struct ez_control_eventcount_s
{
	int	OnMouseEvent;
	int	OnMouseClick;
	int	OnMouseUp;
	int	OnMouseUpOutside;
	int	OnMouseDown;
	int	OnMouseHover;
	int	OnMouseEnter;
	int	OnMouseLeave;
	int	OnKeyEvent;
	int	OnKeyDown;
	int	OnKeyUp;
	int	OnLayoutChildren;
	int	OnDraw;
	int	OnDestroy;
	int	OnMove;
	int	OnScroll;
	int	OnParentScroll;
	int	OnResize;
	int	OnParentResize;
	int	OnGotFocus;
	int	OnLostFocus;
	int	OnVirtualResize;
	int	OnMinVirtualResize;
	int	OnFlagsChanged;
	int OnResizeHandleThicknessChanged;
	int OnEventHandlerChanged;
	int OnAnchorChanged;
	int OnVisibilityChanged;
	int OnOpacityChanged;
	int OnChildMoved;
	int OnChildResize;
} ez_control_eventcount_t;

typedef struct ez_control_events_s
{
	ez_mouse_event_fp			OnMouseEvent;
	ez_mouse_event_fp			OnMouseClick;
	ez_mouse_event_fp			OnMouseUp;
	ez_mouse_event_fp			OnMouseUpOutside;
	ez_mouse_event_fp			OnMouseDown;
	ez_mouse_event_fp			OnMouseHover;
	ez_mouse_event_fp			OnMouseEnter;
	ez_mouse_event_fp			OnMouseLeave;
	ez_key_event_fp				OnKeyEvent;
	ez_keyspecific_event_fp		OnKeyDown;
	ez_keyspecific_event_fp		OnKeyUp;
	ez_event_fp					OnLayoutChildren;
	ez_event_fp					OnDraw;
	ez_destroy_event_fp			OnDestroy;
	ez_event_fp					OnMove;
	ez_event_fp					OnScroll;
	ez_event_fp					OnParentScroll;
	ez_event_fp					OnResize;
	ez_event_fp					OnParentResize;
	ez_event_fp					OnGotFocus;
	ez_event_fp					OnLostFocus;
	ez_event_fp					OnVirtualResize;
	ez_event_fp					OnMinVirtualResize;
	ez_event_fp					OnFlagsChanged;
	ez_event_fp					OnResizeHandleThicknessChanged;
	ez_event_fp					OnEventHandlerChanged;
	ez_event_fp					OnAnchorChanged;
	ez_event_fp					OnVisibilityChanged;
	ez_event_fp					OnOpacityChanged;
	ez_child_event_fp			OnChildMoved;
	ez_child_event_fp			OnChildResize;
} ez_control_events_t;

typedef struct ez_control_eventhandlers_s
{					
	ez_eventhandler_t	*OnMouseEvent;
	ez_eventhandler_t	*OnMouseClick;
	ez_eventhandler_t	*OnMouseUp;
	ez_eventhandler_t	*OnMouseUpOutside;
	ez_eventhandler_t	*OnMouseDown;
	ez_eventhandler_t	*OnMouseHover;
	ez_eventhandler_t	*OnMouseEnter;
	ez_eventhandler_t	*OnMouseLeave;
	ez_eventhandler_t	*OnKeyEvent;
	ez_eventhandler_t	*OnKeyDown;
	ez_eventhandler_t	*OnKeyUp;
	ez_eventhandler_t	*OnLayoutChildren;
	ez_eventhandler_t	*OnDraw;
	ez_eventhandler_t	*OnDestroy;
	ez_eventhandler_t	*OnMove;
	ez_eventhandler_t	*OnScroll;
	ez_eventhandler_t	*OnParentScroll;
	ez_eventhandler_t	*OnResize;
	ez_eventhandler_t	*OnParentResize;
	ez_eventhandler_t	*OnGotFocus;
	ez_eventhandler_t	*OnLostFocus;
	ez_eventhandler_t	*OnVirtualResize;
	ez_eventhandler_t	*OnMinVirtualResize;
	ez_eventhandler_t	*OnFlagsChanged;
	ez_eventhandler_t	*OnResizeHandleThicknessChanged;
	ez_eventhandler_t	*OnEventHandlerChanged;
	ez_eventhandler_t	*OnAnchorChanged;
	ez_eventhandler_t	*OnVisibilityChanged;
	ez_eventhandler_t	*OnOpacityChanged;
	ez_eventhandler_t	*OnChildMoved;
	ez_eventhandler_t	*OnChildResize;
} ez_control_eventhandlers_t;

typedef enum ez_anchor_e
{
	anchor_none		= (1 << 0),
	anchor_left		= (1 << 1), 
	anchor_right	= (1 << 2),
	anchor_top		= (1 << 3),
	anchor_bottom	= (1 << 4)
} ez_anchor_t;

//
// Control - External flags.
//
typedef enum ez_control_flags_e
{
	control_enabled				= (1 << 0),		// Is the control usable?
	control_movable				= (1 << 1),		// Can the control be moved?
	control_focusable			= (1 << 2),		// Can the control be given focus?
	control_resize_h			= (1 << 3),		// Is the control resizeable horizontally?
	control_resize_v			= (1 << 4),		// Is the control resizeable vertically?
	control_resizeable			= (1 << 5),		// Is the control resizeable at all, not just by the user? // TODO : Should this be external?
	control_contained			= (1 << 6),		// Is the control contained within it's parent or can it go outside its edges?
	control_visible				= (1 << 7),		// Is the control visible?
	control_scrollable			= (1 << 8),		// Is the control scrollable
	control_ignore_mouse		= (1 << 9),		// Should the control ignore mouse input?
	control_anchor_viewport		= (1 << 10),	// Anchor to the visible edges of the controls instead of the virtual edges.
	control_move_parent			= (1 << 11),	// When moving this control, should we also move it's parent?
	control_listen_repeat_mouse	= (1 << 12),	// Should the control listen to repeated mouse button events?
	control_bg_tile_center		= (1 << 13),	// Tile the center of the background image (otherwise it will be stretched).
	control_bg_tile_edges		= (1 << 14)		// Tile the edges of the background image.
} ez_control_flags_t;

#define DEFAULT_CONTROL_FLAGS	(control_enabled | control_focusable | control_contained | control_scrollable | control_visible)

//
// Control - Internal flags.
//
typedef enum ez_control_iflags_e
{
	control_focused				= (1 << 0),		// Is the control currently in focus?
	control_moving				= (1 << 1),		// Is the control in the process of being moved?
	control_resizing_left		= (1 << 2),		// Are we resizing the control to the left?
	control_resizing_right		= (1 << 3),		// Are we resizing the control to the right?
	control_resizing_top		= (1 << 4),		// Resizing upwards.
	control_resizing_bottom		= (1 << 5),		// Resizing downards.
	control_clicked				= (1 << 6),		// Is the control being clicked? (If the mouse button is released outside the control a click event isn't raised).
	control_mouse_over			= (1 << 7),		// Is the mouse over the control?
	control_update_anchorgap	= (1 << 8),		// Update the anchor gap when resizing the control?
	control_hidden_by_parent	= (1 << 9)		// Is the control hidden cause it's parent is not visible?
} ez_control_iflags_t;

typedef struct ez_control_s
{
	ez_control_id_t			CLASS_ID;				// An ID unique for this class, this is set at initilization
													// and should never be changed after that.
	char					*name;					// The name of the control.
	char					*description;			// A short description of the control.
	
	int	 					x;						// Relative position to it's parent.
	int						y;

	int						prev_x;					// Previous position.
	int						prev_y;

	int						absolute_x;				// The absolute screen coordinates for the control.
	int						absolute_y;	

	int						prev_absolute_x;		// Previous absolute position.
	int						prev_absolute_y;
	
	int 					width;					// Size.
	int						height;
	
	int						prev_width;				// Previous size.
	int						prev_height;

	int						virtual_x;				// The relative position in the virtual window that the "real" window is showing.
	int						virtual_y;

	int						absolute_virtual_x;		// The absolute position of the virtual window on the screen.
	int						absolute_virtual_y;

	int						virtual_width;			// The virtual size of the control (scrollable area).
	int						virtual_height;

	int						prev_virtual_width;		// The previous virtual size of the control.
	int						prev_virtual_height;

	int						virtual_width_min;		// The virtual min size for the control.
	int						virtual_height_min;
	
	int						width_max;				// Max/min sizes for the control.
	int						width_min;
	int						height_max;
	int						height_min;

	int						bound_top;				// The bounds the control is allowed to draw within.
	int						bound_left;
	int						bound_right;
	int						bound_bottom;

	int						left_edge_gap;			// The gap to the different edges, used for anchoring
	int						right_edge_gap;			// if the parent controls size becomes less than the
	int						top_edge_gap;			// childs min size, we need to keep track of how far
	int						bottom_edge_gap;		// from the edge we were.

	ez_anchor_t				anchor_flags;			// Which parts of it's parent the control is anchored to.

	int						resize_handle_thickness;// The thickness of the resize handles on the sides of the control.

	int						draw_order;				// The order the control is drawn in.
	int						tab_order;				// The tab number of the control.
	
	ez_control_flags_t		ext_flags;				// External flags that decide how the control should behave. Movable, resizeable and such.
	ez_control_iflags_t		int_flags;				// Internal flags that holds the current state of the control, moving, resizing and so on.

	byte					background_color[4];	// The background color of the control RGBA.
	mpic_t					*background;			// The background picture.
	float					bg_edge_size_ratio;		// The percentage of the width of the background picture to use for drawing the edges 
													// of the background when drawn tiled. Value range 0.0 - 1.0
	float					bg_opacity;				// The opacity of the control.
	float					opacity;				// My own opacity only.
	float					overall_opacity;		// The overall opacity of the control (including my parents opacity).

	float					x_percent;				// The x position in percentage based on the parents width.
	float					y_percent;				// The y position in percentage based on the parents height.

	void					*payload;				// A pointer to some user specified data associated with the control. Up to the caller to clean this up.

	ez_control_events_t			events;				// The base reaction for events. Is only set at initialization.
	ez_control_eventhandlers_t	event_handlers;
	ez_control_eventcount_t		inherit_levels;		// The number of times each event has been overriden. 
													// (Countdown before executing the event handler for the event, after all
													// event implementations in the inheritance chain have been run).
	ez_control_eventcount_t override_counts;		// This is reset each time an event is raised by CONTROL_RAISE_EVENT to the
													// inheritance level for the event in question.

	qbool					initializing;			// Is the control initializing?

	struct ez_control_s		*parent;				// The parent of the control. Only the root node has no parent.
	ez_double_linked_list_t	children;				// List of children belonging to the control.

	struct ez_tree_s		*control_tree;			// The control tree the control belongs to.

	mouse_state_t			prev_mouse_state;		// The previous mouse event that was passed on to this control.
	double					mouse_repeat_delay;		// The time to wait before raising a new mouse click event for this control (if control_listen_repeat_mouse is set).
} ez_control_t;

//
// Eventhandler - Execute an event handler. (We declare this here cause ez_control_t hasn't been defined above)
//
void EZ_eventhandler_Exec(ez_eventhandler_t *event_handler, ez_control_t *ctrl, ...);

//
// Control - Creates a new control and initializes it.
//
ez_control_t *EZ_control_Create(ez_tree_t *tree, ez_control_t *parent, 
							  char *name, char *description, 
							  int x, int y, int width, int height, 
							  int flags);

//
// Control - Initializes a control and adds it to the specified control tree.
//
void EZ_control_Init(ez_control_t *control, ez_tree_t *tree, ez_control_t *parent, 
							  char *name, char *description, 
							  int x, int y, int width, int height, 
							  ez_control_flags_t flags);

//
// Control - Destroys a specified control.
//
int EZ_control_Destroy(ez_control_t *self, qbool destroy_children);

//
// Control - 
// Returns the screen position of the control. This will be different for a scrollable window
// since it's drawing position differs from the windows actual position on screen.
//
void EZ_control_GetDrawingPosition(ez_control_t *self, int *x, int *y);

//
// Control - Gets the anchor flags.
//
ez_anchor_t EZ_control_GetAnchor(ez_control_t *self);

//
// Control - Sets the external flags of the control.
//
ez_control_flags_t EZ_control_GetFlags(ez_control_t *self);

//
// Control - Sets the external flags of the control.
//
void EZ_control_SetFlags(ez_control_t *self, ez_control_flags_t flags);

//
// Control - Set the thickness of the resize handles (if any).
//
void EZ_control_SetResizeHandleThickness(ez_control_t *self, int thickness);

//
// Control - Sets whetever the control is scrollable or not.
//
void EZ_control_SetScrollable(ez_control_t *self, qbool scrollable);

//
// Control - Sets if the control should move it's parent when it moves itself.
//
void EZ_control_SetMovesParent(ez_control_t *self, qbool moves_parent);

//
// Control - Sets whetever the control is visible or not.
//
void EZ_control_SetVisible(ez_control_t *self, qbool visible);

//
// Control - Sets whetever the control is resizeable vertically by the user.
//
void EZ_control_SetResizeableVertically(ez_control_t *self, qbool resize_vertically);

//
// Control - Sets whetever the control is resizeable at all, not just by the user.
//
void EZ_control_SetResizeable(ez_control_t *self, qbool resizeable);

//
// Control - Sets whetever the control is resizeable both horizontally and vertically by the user.
//
void EZ_control_SetResizeableBoth(ez_control_t *self, qbool resize);

//
// Control - Sets whetever the control is resizeable horizontally by the user.
//
void EZ_control_SetResizeableHorizontally(ez_control_t *self, qbool resize_horizontally);

//
// Control - Sets whetever the control is focusable.
//
void EZ_control_SetFocusable(ez_control_t *self, qbool focusable);

//
// Control - Sets whetever the control is movable.
//
void EZ_control_SetMovable(ez_control_t *self, qbool movable);

//
// Control - Sets whetever the control is enabled or not.
//
void EZ_control_SetEnabled(ez_control_t *self, qbool enabled);

//
// Control - Set the background image for the control.
//
void EZ_control_SetBackgroundImage(ez_control_t *self, const char *background_path);

//
// Control - Set the opacity for the background image for the control.
//
void EZ_control_SetBackgroundImageOpacity(ez_control_t *self, float opacity);

//
// Control - Set how much percentage of the width of the background image that should be used when drawing the edges of the control.
//
void EZ_control_SetBackgroundImageEdgePercentage(ez_control_t *self, int percentage);

//
// Control - Set if the center of the button should be tiled or stretched.
//
void EZ_control_SetBackgroundTileCenter(ez_control_t *self, qbool tilecenter);

//
// Control - Set if the edges of the button should be tiled or stretched.
//
void EZ_control_SetBackgroundTileEdges(ez_control_t *self, qbool tileedges);

//
// Control - Sets whetever the control is contained within the bounds of it's parent or not, or is allowed to draw outside it.
//
void EZ_control_SetContained(ez_control_t *self, qbool contained);

//
// Control - Sets whetever the control should care about mouse input or not.
//
void EZ_control_SetIgnoreMouse(ez_control_t *self, qbool ignore_mouse);

//
// Control - Listen to repeated mouse click events when holding down a mouse button. 
//           The delay between events is set using EZ_control_SetRepeatMouseClickDelay(...)
//
void EZ_control_SetListenToRepeatedMouseClicks(ez_control_t *self, qbool listen_repeat);

//
// Control - Sets the amount of time to wait between each new mouse click event
//           when holding down the mouse over a control.
//
void EZ_control_SetRepeatMouseClickDelay(ez_control_t *self, double delay);

//
// Control - Sets the OnDestroy event handler.
//
void EZ_control_AddOnDestroy(ez_control_t *self, ez_destroy_eventhandler_fp OnDestroy, void *payload);

//
// Control - Sets the OnLayoutChildren event handler.
//
void EZ_control_AddOnLayoutChildren(ez_control_t *self, ez_eventhandler_fp OnLayoutChildren, void *payload);

//
// Control - Sets the OnMove event handler.
//
void EZ_control_AddOnMove(ez_control_t *self, ez_eventhandler_fp OnMove, void *payload);

//
// Control - Sets the OnScroll event handler.
//
void EZ_control_AddOnScroll(ez_control_t *self, ez_eventhandler_fp OnScroll, void *payload);

//
// Control - Sets the OnResize event handler.
//
void EZ_control_AddOnResize(ez_control_t *self, ez_eventhandler_fp OnResize, void *payload);

//
// Control - Sets the OnParentResize event handler.
//
void EZ_control_AddOnParentResize(ez_control_t *self, ez_eventhandler_fp OnParentResize, void *payload);

//
// Control - Sets the OnVirtualResize event handler.
//
void EZ_control_AddOnVirtualResize(ez_control_t *self, ez_eventhandler_fp OnVirtualResize, void *payload);

//
// Control - Sets the OnMinVirtualResize event handler.
//
void EZ_control_AddOnMinVirtualResize(ez_control_t *self, ez_eventhandler_fp OnMinVirtualResize, void *payload);

//
// Control - Sets the OnFlagsChanged event handler.
//
void EZ_control_AddOnFlagsChanged(ez_control_t *self, ez_eventhandler_fp OnFlagsChanged, void *payload);

//
// Control - Sets the OnKeyEvent event handler.
//
void EZ_control_AddOnKeyEvent(ez_control_t *self, ez_key_eventhandler_fp OnKeyEvent, void *payload);

//
// Control - Sets the OnLostFocus event handler.
//
void EZ_control_AddOnLostFocus(ez_control_t *self, ez_eventhandler_fp OnLostFocus, void *payload);

//
// Control - Sets the OnGotFocus event handler.
//
void EZ_control_AddOnGotFocus(ez_control_t *self, ez_eventhandler_fp OnGotFocus, void *payload);

//
// Control - Sets the OnMouseHover event handler.
//
void EZ_control_AddOnMouseHover(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseHover, void *payload);

//
// Control - Sets the OnMouseLeave event handler.
//
void EZ_control_AddOnMouseLeave(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseLeave, void *payload);

//
// Control - Sets the OnMouseEnter event handler.
//
void EZ_control_AddOnMouseEnter(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseEnter, void *payload);

//
// Control - Sets the OnMouseClick event handler.
//
void EZ_control_AddOnMouseClick(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseClick, void *payload);

//
// Control - Sets the OnMouseEvent event handler.
//
void EZ_control_AddOnMouseUp(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseUp, void *payload);

//
// Control - Sets the OnMouseUp event handler.
//
void EZ_control_AddOnMouseUp(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseUp, void *payload);

//
// Control - Sets the OnMouseDown event handler.
//
void EZ_control_AddOnMouseDown(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseDown, void *payload);

//
// Control - Sets the OnMouseEvent event handler.
//
void EZ_control_AddOnMouseEvent(ez_control_t *self, ez_mouse_eventhandler_fp OnMouseEvent, void *payload);

//
// Control - Sets the OnDraw event handler.
//
void EZ_control_AddOnDraw(ez_control_t *self, ez_eventhandler_fp OnDraw, void *payload);

//
// Control - Set the event handler for the OnEventHandlerChanged event.
//
void EZ_control_AddOnEventHandlerChanged(ez_control_t *self, ez_eventhandler_fp OnEventHandlerChanged, void *payload);

//
// Control - Sets the OnResizeHandleThicknessChanged event handler.
//
void EZ_control_AddOnResizeHandleThicknessChanged(ez_control_t *self, ez_eventhandler_fp OnResizeHandleThicknessChanged, void *payload);

//
// Control - Sets the OnChildMoved event handler.
//
void EZ_control_AddOnChildMoved(ez_control_t *self, ez_eventhandler_fp OnChildMoved, void *payload);

//
// Control - Sets the OnChildResize event handler.
//
void EZ_control_AddOnChildResize(ez_control_t *self, ez_eventhandler_fp OnChildResize, void *payload);

//
// Control - Set color of a control.
//
void EZ_control_SetBackgroundColor(ez_control_t *self, byte r, byte g, byte b, byte alpha);

//
// Control - Sets the tab order of a control.
//
void EZ_control_SetDrawOrder(ez_control_t *self, int draw_order, qbool update_children);

//
// Control - Sets the payload for the control.
//
void EZ_control_SetPayload(ez_control_t *self, void *payload);

//
// Control - Sets the size of a control.
//
void EZ_control_SetSize(ez_control_t *self, int width, int height);

//
// Control - Set the max size for the control.
//
void EZ_control_SetMaxSize(ez_control_t *self, int max_width, int max_height);

//
// Control - Set the min size for the control.
//
void EZ_control_SetMinSize(ez_control_t *self, int min_width, int min_height);

//
// Control - Sets the position of a control.
//
void EZ_control_SetPosition(ez_control_t *self, int x, int y);

//
// Control - Sets the virtual size of the control (this area can be scrolled around in).
//
void EZ_control_SetVirtualSize(ez_control_t *self, int virtual_width, int virtual_height);

//
// Control - Set the min virtual size for the control, the control size is not allowed to be larger than this.
//
void EZ_control_SetMinVirtualSize(ez_control_t *self, int min_virtual_width, int min_virtual_height);

//
// Control - Sets the part of the control that should be shown if it's scrollable.
//
void EZ_control_SetScrollPosition(ez_control_t *self, int scroll_x, int scroll_y);

//
// Control - Sets the anchoring of the control to it's parent.
//           !!! NOTE !!! 
//           If you set an anchoring to a single edge, make sure you do this
//           after the parent has its final size, otherwise you might get some
//           goofy behavior.
//           !!! NOTE !!!
//
void EZ_control_SetAnchor(ez_control_t *self, ez_anchor_t anchor_flags);

//
// Control - Sets the tab order of a control.
//
void EZ_control_SetTabOrder(ez_control_t *self, int tab_order);

//
// Control - Focuses on a control.
//
qbool EZ_control_SetFocus(ez_control_t *self);

//
// Control - Focuses on a control associated with a specified node from the tab list.
//
qbool EZ_control_SetFocusByNode(ez_control_t *self, ez_dllist_node_t *node);

//
// Control - Returns true if this control is the root control.
//
qbool EZ_control_IsRoot(ez_control_t *self);

//
// Control - Adds a child to the control.
//
void EZ_control_AddChild(ez_control_t *self, ez_control_t *child);

//
// Control - Remove a child from the control. Returns a reference to the child that was removed.
//
ez_control_t *EZ_control_RemoveChild(ez_control_t *self, ez_control_t *child);

// =========================================================================================
// Control - Base Event Handlers (Any inheriting controls can have their own implementation)
// =========================================================================================

//
// Control - Event for when a new event handler is set for an event.
//
int EZ_control_OnEventHandlerChanged(ez_control_t *self, void *ext_event_info);

//
// Control - The control got focus.
//
int EZ_control_OnGotFocus(ez_control_t *self, void *ext_event_info);

//
// Control - The control lost focus.
//
int EZ_control_OnLostFocus(ez_control_t *self, void *ext_event_info);

//
// Control - The control was moved.
//
int EZ_control_OnMove(ez_control_t *self, void *ext_event_info);

//
// Control - On scroll event.
//
int EZ_control_OnScroll(ez_control_t *self, void *ext_event_info);

//
// Control - On parent scroll event.
//
int EZ_control_OnParentScroll(ez_control_t *self, void *ext_event_info);

//
// Control - Convenient function for changing the scroll position by a specified amount.
//
void EZ_control_SetScrollChange(ez_control_t *self, int delta_scroll_x, int delta_scroll_y);

//
// Control - The control was resized.
//
int EZ_control_OnResize(ez_control_t *self, void *ext_event_info);

//
// Control - The anchoring for the control changed.
//
int EZ_control_OnAnchorChanged(ez_control_t *self, void *ext_event_info);

//
// Control - The controls parent was resized.
//
int EZ_control_OnParentResize(ez_control_t *self, void *ext_event_info);

//
// Control - The minimum virtual size has changed for the control.
//
int EZ_control_OnMinVirtualResize(ez_control_t *self, void *ext_event_info);

//
// Label - The virtual size of the control has changed.
//
int EZ_control_OnVirtualResize(ez_control_t *self, void *ext_event_info);

//
// Control - Layouts children.
//
int EZ_control_OnLayoutChildren(ez_control_t *self, void *ext_event_info);

//
// Control - Visibility changed.
//
int EZ_control_OnVisibilityChanged(ez_control_t *self, void *ext_event_info);

//
// Label - The flags for the control changed.
//
int EZ_control_OnFlagsChanged(ez_control_t *self, void *ext_event_info);

//
// Control - OnResizeHandleThicknessChanged event.
//
int EZ_control_OnResizeHandleThicknessChanged(ez_control_t *self, void *ext_event_info);

//
// Control - Draws the control.
//
int EZ_control_OnDraw(ez_control_t *self, void *ext_event_info);

//
// Control - Key down event.
//
int EZ_control_OnKeyDown(ez_control_t *self, int key, int unichar);

//
// Control - Key up event.
//
int EZ_control_OnKeyUp(ez_control_t *self, int key, int unichar);

//
// Control - Key event.
//
int EZ_control_OnKeyEvent(ez_control_t *self, int key, int unichar, qbool down);

//
// Control -
// The initial mouse event is handled by this, and then raises more specialized event handlers
// based on the new mouse state.
// 
// NOTICE! When extending this event you need to make sure that you tell the framework
// that you've handled all mouse events that happened within the controls bounds in your own
// implementation by returning true whenever the mouse is inside the control.
// This can easily be done with the following macro: MOUSE_INSIDE_CONTROL(self, ms); 
// If this is not done, all mouse events will "fall through" to controls below.
//
int EZ_control_OnMouseEvent(ez_control_t *self, mouse_state_t *ms);

//
// Control - The mouse was pressed and then released within the bounds of the control.
//
int EZ_control_OnMouseClick(ez_control_t *self, mouse_state_t *mouse_state);

//
// Control - The mouse entered the controls bounds.
//
int EZ_control_OnMouseEnter(ez_control_t *self, mouse_state_t *mouse_state);

//
// Control - The mouse left the controls bounds.
//
int EZ_control_OnMouseLeave(ez_control_t *self, mouse_state_t *mouse_state);

//
// Control - A mouse button was released within the bounds of the control.
//
int EZ_control_OnMouseUp(ez_control_t *self, mouse_state_t *mouse_state);

//
// Control - A mouse button was released either inside or outside of the control.
//
int EZ_control_OnMouseUpOutside(ez_control_t *self, mouse_state_t *ms);

//
// Control - A mouse button was pressed within the bounds of the control.
//
int EZ_control_OnMouseDown(ez_control_t *self, mouse_state_t *mouse_state);

//
// Control - The mouse wheel was triggered within the bounds of the control.
//
int EZ_control_OnMouseWheel(ez_control_t *self, mouse_state_t *mouse_state);

//
// Control - The mouse is hovering within the bounds of the control.
//
int EZ_control_OnMouseHover(ez_control_t *self, mouse_state_t *mouse_state);

//
// Control - Sets the overall opacity of the control (including its children).
//
void EZ_control_SetOpacity(ez_control_t *self, float opacity);

//
// Control - The opacity of the control has just changed.
//
int EZ_control_OnOpacityChanged(ez_control_t *self, void *ext_event_info);

//
// Control - A child control has been moved.
//
int EZ_control_OnChildMoved(ez_control_t *self, ez_control_t *child);

//
// Control - A child control has been resized.
//
int EZ_control_OnChildResize(ez_control_t *self, ez_control_t *child);

#endif // __EZ_CONTROLS_H__	



