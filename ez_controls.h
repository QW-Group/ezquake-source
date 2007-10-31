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
// Add item to double linked list.
//
void EZ_double_linked_list_Add(ez_double_linked_list_t *list, void *payload);

//
// Finds a given node based on the specified payload.
//
ez_dllist_node_t *EZ_double_linked_list_FindByPayload(ez_double_linked_list_t *list, void *payload);

//
// Removes an item from a linked list by it's payload.
//
void *EZ_double_linked_list_RemoveByPayload(ez_double_linked_list_t *list, void *payload);

//
// Removes the first occurance of the item from double linked list and returns it's payload.
//
void *EZ_double_linked_list_Remove(ez_double_linked_list_t *list, ez_dllist_node_t *item);

//
// Double Linked List - Orders a list.
//
void EZ_double_linked_list_Order(ez_double_linked_list_t *list, PtFuncCompare compare_function);

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
// Control Tree - Destroys a tree. Will not free the memory for the tree.
//
void EZ_tree_Destroy(ez_tree_t *tree);

// =========================================================================================
// Event handler 
// =========================================================================================

//
// Event function pointer types.
//
typedef int (*ez_event_fp) (struct ez_control_s *self);
typedef int (*ez_mouse_event_fp) (struct ez_control_s *self, mouse_state_t *mouse_state);
typedef int (*ez_key_event_fp) (struct ez_control_s *self, int key, int unichar, qbool down);
typedef int (*ez_keyspecific_event_fp) (struct ez_control_s *self, int key, int unichar);
typedef int (*ez_destroy_event_fp) (struct ez_control_s *self, qbool destroy_children);

//
// Event handlers function pointer types (same as event function types, except they have a payload also).
//
typedef int (*ez_eventhandler_fp) (struct ez_control_s *self, void *payload);
typedef int (*ez_mouse_eventhandler_fp) (struct ez_control_s *self, void *payload, mouse_state_t *mouse_state);
typedef int (*ez_key_eventhandler_fp) (struct ez_control_s *self, void *payload, int key, int unichar, qbool down);
typedef int (*ez_keyspecific_eventhandler_fp) (struct ez_control_s *self, void *payload, int key, int unichar);
typedef int (*ez_destroy_eventhandler_fp) (struct ez_control_s *self, void *payload, qbool destroy_children);

#define EZ_CONTROL_HANDLER			0
#define EZ_CONTROL_MOUSE_HANDLER	1
#define EZ_CONTROL_KEY_HANDLER		2
#define EZ_CONTROL_KEYSP_HANDLER	3
#define EZ_CONTROL_DESTROY_HANDLER	4

typedef union ez_eventhandlerfunction_u
{
	ez_eventhandler_fp				normal;
	ez_mouse_eventhandler_fp		mouse;
	ez_key_eventhandler_fp			key;
	ez_keyspecific_eventhandler_fp	key_sp;
	ez_destroy_eventhandler_fp		destroy;
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

#define EZ_CONTROL_ID 0

// TODO : Redo these structs so that we have one struct containing {eventcount, event, evethandler} and then a struct with all the different events based on that struct, so that we don't need 592898 places for the OnEvent names.
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
	control_listen_repeat_mouse	= (1 << 12)		// Should the control listen to repeated mouse button events?
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
	int						CLASS_ID;				// An ID unique for this class, this is set at initilization
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
	float					opacity;				// The opacity of the control.

	ez_control_events_t			events;				// The base reaction for events. Is only set at initialization.
	ez_control_eventhandlers_t	event_handlers;
	ez_control_eventcount_t		inherit_levels;		// The number of times each event has been overriden. 
													// (Countdown before executing the event handler for the event, after all
													// event implementations in the inheritance chain have been run).
	ez_control_eventcount_t override_counts;		// This gets resetted each time a event is raised by CONTROL_RAISE_EVENT to the
													// inheritance level for the event in question.

	struct ez_control_s		*parent;				// The parent of the control. Only the root node has no parent.
	ez_double_linked_list_t	children;				// List of children belonging to the control.

	struct ez_tree_s		*control_tree;			// The control tree the control belongs to.

	mouse_state_t			prev_mouse_state;		// The last mouse event that was passed on to this control.
	double					mouse_repeat_delay;		// The time to wait before raising a new mouse click event for this control (if control_listen_repeat_mouse is set).
} ez_control_t;

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

//
// Control - Initializes a control.
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
// Control - Set color of a control.
//
void EZ_control_SetBackgroundColor(ez_control_t *self, byte r, byte g, byte b, byte alpha);

//
// Control - Sets the tab order of a control.
//
void EZ_control_SetDrawOrder(ez_control_t *self, int draw_order, qbool update_children);

//
// Control - Sets the size of a control.
//
void EZ_control_SetSize(ez_control_t *self, int width, int height);

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
int EZ_control_OnEventHandlerChanged(ez_control_t *self);

//
// Control - The control got focus.
//
int EZ_control_OnGotFocus(ez_control_t *self);

//
// Control - The control lost focus.
//
int EZ_control_OnLostFocus(ez_control_t *self);

//
// Control - The control was moved.
//
int EZ_control_OnMove(ez_control_t *self);

//
// Control - On scroll event.
//
int EZ_control_OnScroll(ez_control_t *self);

//
// Control - On parent scroll event.
//
int EZ_control_OnParentScroll(ez_control_t *self);

//
// Control - Convenient function for changing the scroll position by a specified amount.
//
void EZ_control_SetScrollChange(ez_control_t *self, int delta_scroll_x, int delta_scroll_y);

//
// Control - The control was resized.
//
int EZ_control_OnResize(ez_control_t *self);

//
// Control - The anchoring for the control changed.
//
int EZ_control_OnAnchorChanged(ez_control_t *self);

//
// Control - The controls parent was resized.
//
int EZ_control_OnParentResize(ez_control_t *self);

//
// Control - The minimum virtual size has changed for the control.
//
int EZ_control_OnMinVirtualResize(ez_control_t *self);

//
// Label - The virtual size of the control has changed.
//
int EZ_control_OnVirtualResize(ez_control_t *self);

//
// Control - Layouts children.
//
int EZ_control_OnLayoutChildren(ez_control_t *self);

//
// Control - Visibility changed.
//
int EZ_control_OnVisibilityChanged(ez_control_t *self);

//
// Label - The flags for the control changed.
//
int EZ_control_OnFlagsChanged(ez_control_t *self);

//
// Control - OnResizeHandleThicknessChanged event.
//
int EZ_control_OnResizeHandleThicknessChanged(ez_control_t *self);

//
// Control - Draws the control.
//
int EZ_control_OnDraw(ez_control_t *self);

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
// NOTICE! When extending this event you need to make sure that you need to tell the framework
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

// =========================================================================================
// Label
// =========================================================================================

#define EZ_LABEL_ID 2

typedef enum ez_label_flags_e
{
	label_largefont		= (1 << 0),		// Use the large font.
	label_autosize		= (1 << 1),		// Autosize the label to fit the text (if it's not wrapped).
	label_autoellipsis	= (1 << 2),		// Show ... at the end of the text if it doesn't fit within the label.
	label_wraptext		= (1 << 3),		// Wrap the text to fit inside the label.
	label_selectable	= (1 << 4),		// Is text selectable?
	label_readonly		= (1 << 5)		// Is input allowed?
} ez_label_flags_t;

#define LABEL_DEFAULT_FLAGS	(label_wraptext | label_selectable)

typedef enum ez_label_iflags_e
{
	label_selecting		= (1 << 0)		// Are we selecting text?
} ez_label_iflags_t;

#define LABEL_MAX_WRAPS		512
#define LABEL_LINE_SIZE		1024

// Is any text selected in the label?
#define LABEL_TEXT_SELECTED(label) ((label->select_start > -1) && (label->select_end > - 1) && abs(label->select_end - label->select_start) > 0)

typedef struct ez_label_eventcount_s
{
	int	OnTextChanged;
	int	OnCaretMoved;
	int	OnTextScaleChanged;
	int	OnTextFlagsChanged;
} ez_label_eventcount_t;

typedef struct ez_label_events_s
{
	ez_event_fp	OnTextChanged;			// Event raised when the text in the label is changed.
	ez_event_fp	OnCaretMoved;			// The caret was moved.
	ez_event_fp	OnTextScaleChanged;		// The scale of the text changed.
	ez_event_fp	OnTextFlagsChanged;		// The text flags changed.
} ez_label_events_t;

typedef struct ez_label_eventhandlers_s
{
	ez_eventhandler_t		*OnTextChanged;
	ez_eventhandler_t		*OnCaretMoved;
	ez_eventhandler_t		*OnTextScaleChanged;
	ez_eventhandler_t		*OnTextFlagsChanged;
} ez_label_eventhandlers_t;

typedef struct ez_label_textpos_s
{
	int index;
	int row;
	int col;
} ez_label_textpos_t;

typedef struct ez_label_s
{
	ez_control_t			super;						// Super class.

	ez_label_events_t		events;						// Specific events for the label control.
	ez_label_eventhandlers_t event_handlers;			// Specific event handlers for the label control.
	ez_label_eventcount_t	inherit_levels;
	ez_label_eventcount_t	override_counts;

	char					*text;						// The text to be shown in the label.
	int						text_length;				// The length of the label text (excluding \0).
	
	ez_label_flags_t		ext_flags;					// External flags for how the text in the label is formatted.
	ez_label_iflags_t		int_flags;					// Internal flags for the current state of the label.

	ez_label_textpos_t		wordwraps[LABEL_MAX_WRAPS];	// Positions in the text where line breaks are located.
	clrinfo_t				color;						// The text color of the label. // TODO : Make this a pointer?
	float					scale;						// The scale of the text in the label.
	int						scaled_char_size;			// The actual size of the characters in the text.
	int						select_start;				// At what index the currently selected text starts at 
														// (this can be greater than select_end).
	int						select_end;					// The end index of the selected text.
	ez_label_textpos_t		caret_pos;					// The position of the caret (index / row / column).
	int						num_rows;					// The current number of rows in the label.
	int						num_cols;					// The current number of cols (for the longest row).
} ez_label_t;

ez_label_t *EZ_label_Create(ez_tree_t *tree, ez_control_t *parent, 
							  char *name, char *description, 
							  int x, int y, int width, int height,
							  ez_control_flags_t flags, ez_label_flags_t text_flags,
							  char *text);

void EZ_label_Init(ez_label_t *label, ez_tree_t *tree, ez_control_t *parent, 
				  char *name, char *description, 
				  int x, int y, int width, int height,
				  ez_control_flags_t flags, ez_label_flags_t text_flags,
				  char *text);

//
// Label - Destroys a label control.
//
int EZ_label_Destroy(ez_control_t *self, qbool destroy_children);

//
// Label - Sets if the label should use the large charset or the normal one.
//
void EZ_label_SetLargeFont(ez_label_t *label, qbool large_font);

//
// Label - Sets if the label should autosize itself to fit the text in the label.
//
void EZ_label_SetAutoSize(ez_label_t *label, qbool auto_size);

//
// Label - Sets if the label should automatically add "..." at the end of the string when it doesn't fit in the label.
//
void EZ_label_SetAutoEllipsis(ez_label_t *label, qbool auto_ellipsis);

//
// Label - Sets if the text in the label should wrap to fit the width of the label.
//
void EZ_label_SetWrapText(ez_label_t *label, qbool wrap_text);

//
// Label - Sets the text flags for the label.
//
void EZ_label_SetTextFlags(ez_label_t *label, ez_label_flags_t flags);

//
// Label - Sets if the label should be read only.
//
void EZ_label_SetReadOnly(ez_label_t *label, qbool read_only);

//
// Label - Sets if the text in the label should be selectable.
//
void EZ_label_SetTextSelectable(ez_label_t *label, qbool selectable);

//
// Label - Sets the event handler for the OnTextChanged event.
//
void EZ_label_AddOnTextChanged(ez_label_t *label, ez_eventhandler_fp OnTextChanged, void *payload);

//
// Label - Sets the event handler for the OnTextScaleChanged event.
//
void EZ_label_AddOnTextScaleChanged(ez_label_t *label, ez_eventhandler_fp OnTextScaleChanged, void *payload);

//
// Label - Sets the event handler for the OnCaretMoved event.
//
void EZ_label_AddOnTextOnCaretMoved(ez_label_t *label, ez_eventhandler_fp OnCaretMoved, void *payload);

//
// Label - Hides the caret.
//
void EZ_label_HideCaret(ez_label_t *label);

//
// Label - Gets the size of the selected text.
//
int EZ_label_GetSelectedTextSize(ez_label_t *label);

//
// Label - Gets the selected text.
//
void EZ_label_GetSelectedText(ez_label_t *label, char *target, int target_size);

//
// Label - Deselects the text in the label.
//
void EZ_label_DeselectText(ez_label_t *label);

//
// Label - Set the text scale for the label.
//
void EZ_label_SetTextScale(ez_label_t *label, float scale);

//
// Label - Sets the color of the text in the label.
//
void EZ_label_SetTextColor(ez_label_t *self, byte r, byte g, byte b, byte alpha);

//
// Label - Sets the text of the label (will be reallocated and copied to the heap).
//
void EZ_label_SetText(ez_label_t *self, const char *text);

//
// Label - Gets the text flags for the label.
//
ez_label_flags_t EZ_label_GetTextFlags(ez_label_t *label);

//
// Label - Sets the text flags for the label.
//
void EZ_label_SetTextFlags(ez_label_t *label, ez_label_flags_t flags);

//
// Label - Appends text at the given position in the text.
//
void EZ_label_AppendText(ez_label_t *label, int position, const char *append_text);

//
// Label - Set the caret position.
//
void EZ_label_SetCaretPosition(ez_label_t *label, int caret_pos);

//
// Label - The text flags for the label changed.
//
int EZ_label_OnTextFlagsChanged(ez_control_t *self);

//
// Label - The scale of the text changed.
//
int EZ_label_OnTextScaleChanged(ez_control_t *self);

//
// Label - Happens when the control has resized.
//
int EZ_label_OnResize(ez_control_t *self);

//
// Label - On Draw.
//
int EZ_label_OnDraw(ez_control_t *label);

//
// Label - The caret was moved.
//
int EZ_label_OnCaretMoved(ez_control_t *self);

//
// Label - The text changed in the label.
//
int EZ_label_OnTextChanged(ez_control_t *self);

//
// Label - Key down event.
//
int EZ_label_OnKeyDown(ez_control_t *self, int key, int unichar);

//
// Label - Key up event.
//
int EZ_label_OnKeyUp(ez_control_t *self, int key, int unichar);

//
// Label - Key event.
//
int EZ_label_OnKeyEvent(ez_control_t *self, int key, int unichar, qbool down);

//
// Label - Handles the mouse down event.
//
int EZ_label_OnMouseDown(ez_control_t *self, mouse_state_t *ms);

//
// Label - Handles the mouse up event.
//
int EZ_label_OnMouseUp(ez_control_t *self, mouse_state_t *ms);

//
// Label - The mouse is hovering within the bounds of the label.
//
int EZ_label_OnMouseHover(ez_control_t *self, mouse_state_t *mouse_state);

// =========================================================================================
// Button
// =========================================================================================

#define EZ_BUTTON_ID	1

#define EZ_BUTTON_DEFAULT_NORMAL_IMAGE	"gfx/ui/button_normal"
#define EZ_BUTTON_DEFAULT_HOVER_IMAGE	"gfx/ui/button_hover"
#define EZ_BUTTON_DEFAULT_PRESSED_IMAGE	"gfx/ui/button_pressed"

typedef struct ez_button_eventcount_s
{
	int	OnAction;
	int	OnTextAlignmentChanged;
} ez_button_eventcount_t;

typedef struct ez_button_events_s
{
	ez_event_fp	OnAction;				// The event that's raised when the button is clicked / activated via a button.
	ez_event_fp	OnTextAlignmentChanged;	// Text alignment changed.
} ez_button_events_t;

typedef struct ez_button_eventhandlers_s
{
	ez_eventhandler_t		*OnAction;
	ez_eventhandler_t		*OnTextAlignmentChanged;
} ez_button_eventhandlers_t;

typedef enum ez_button_flags_e
{
	use_images	= (1 << 0),
	tile_center	= (1 << 1),
	tile_edges	= (1 << 2)
} ez_button_flags_t;

typedef enum ez_textalign_e
{
	top_left,
	top_center,
	top_right,
	middle_left,
	middle_center,
	middle_right,
	bottom_left,
	bottom_center,
	bottom_right
} ez_textalign_t;

typedef struct ez_button_s
{
	ez_control_t			super;				// The super class.

	ez_button_events_t		events;				// Specific events for the button control.
	ez_button_eventhandlers_t event_handlers;	// Specific event handlers for the button control.
	ez_button_eventcount_t	inherit_levels;
	ez_button_eventcount_t	override_counts;

	ez_label_t				*text_label;		// The buttons text label.

	mpic_t					*focused_image;		// The image for the button when it is focused.
	mpic_t					*normal_image;		// The normal image used for the button.
	mpic_t					*hover_image;		// The image that is shown for the button when the mouse is over it.
	mpic_t					*pressed_image;		// The image that is shown when the button is pressed.

	byte					color_focused[4];	// Color of the focus indicator of the button.
	byte					color_normal[4];	// The normal color of the button.
	byte					color_hover[4];		// Color when the button is hovered.
	byte					color_pressed[4];	// Color when the button is pressed.

	ez_button_flags_t		ext_flags;

	int						padding_top;
	int						padding_left;
	int						padding_right;
	int						padding_bottom;
	ez_textalign_t			text_alignment;		// Text alignment.
	clrinfo_t				focused_text_color;	// Text color when the button is focused.
	clrinfo_t				normal_text_color;	// Text color when the button is in it's normal state.
	clrinfo_t				hover_text_color;	// Text color when the button is hovered.
	clrinfo_t				pressed_text_color;	// Text color when the button is pressed.

	int						override_count;		// These are needed so that subclasses can override button specific events.
	int						inheritance_level;
} ez_button_t;

//
// Button - Creates a new button and initializes it.
//
ez_button_t *EZ_button_Create(ez_tree_t *tree, ez_control_t *parent, 
							  char *name, char *description, 
							  int x, int y, int width, int height, 
							  ez_control_flags_t flags);

//
// Button - Initializes a button.
//
void EZ_button_Init(ez_button_t *button, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Button - Destroys the button.
//
void EZ_button_Destroy(ez_control_t *self, qbool destroy_children);

//
// Button - OnTextAlignmentChanged event.
//
int EZ_button_OnTextAlignmentChanged(ez_control_t *self);

//
// Button - OnAction event handler.
//
int EZ_button_OnAction(ez_control_t *self);

//
// Button - OnResize event handler.
//
int EZ_button_OnResize(ez_control_t *self);

//
// Button - Use images for the button?
//
void EZ_button_SetUseImages(ez_button_t *button, qbool useimages);

//
// Button - Set the text of the button. 
//
void EZ_button_SetText(ez_button_t *button, const char *text);

//
// Button - Set the text of the button. 
//
void EZ_button_SetTextAlignment(ez_button_t *button, ez_textalign_t text_alignment);

//
// Button - Set if the edges of the button should be tiled or stretched.
//
void EZ_button_SetTileCenter(ez_button_t *button, qbool tileedges);

//
// Button - Set if the center of the button should be tiled or stretched.
//
void EZ_button_SetTileCenter(ez_button_t *button, qbool tilecenter);

//
// Button - Set the event handler for the OnTextAlignmentChanged event.
//
void EZ_button_AddOnTextAlignmentChanged(ez_button_t *button, ez_eventhandler_fp OnTextAlignmentChanged, void *payload);

//
// Button - Set the normal image for the button.
//
void EZ_button_SetNormalImage(ez_button_t *button, const char *normal_image);

//
// Button - Set the hover image for the button.
//
void EZ_button_SetHoverImage(ez_button_t *button, const char *hover_image);

//
// Button - Set the hover image for the button.
//
void EZ_button_SetPressedImage(ez_button_t *button, const char *pressed_image);

// 
// Button - Sets the normal color of the button.
//
void EZ_button_SetNormalColor(ez_button_t *self, byte r, byte g, byte b, byte alpha);

// 
// Button - Sets the pressed color of the button.
//
void EZ_button_SetPressedColor(ez_button_t *self, byte r, byte g, byte b, byte alpha);

// 
// Button - Sets the hover color of the button.
//
void EZ_button_SetHoverColor(ez_button_t *self, byte r, byte g, byte b, byte alpha);

// 
// Button - Sets the focused color of the button.
//
void EZ_button_SetFocusedColor(ez_button_t *self, byte r, byte g, byte b, byte alpha);

// 
// Button - Sets the OnAction event handler.
//
void EZ_button_AddOnAction(ez_button_t *self, ez_eventhandler_fp OnAction, void *payload);

//
// Button - OnDraw event.
//
int EZ_button_OnDraw(ez_control_t *self);

// =========================================================================================
// Slider
// =========================================================================================

#define EZ_SLIDER_ID					2

typedef enum ez_slider_iflags_e
{
	slider_dragging	= (1 << 0)
} ez_slider_iflags_t;

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

	ez_slider_iflags_t		int_flags;			// Slider specific flags.
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
int EZ_slider_OnDraw(ez_control_t *self);

//
// Slider - The max value changed.
//
int EZ_slider_OnMaxValueChanged(ez_control_t *self);

//
// Slider - The min value changed.
//
int EZ_slider_OnMinValueChanged(ez_control_t *self);

//
// Slider - Scale changed.
//
int EZ_slider_OnScaleChanged(ez_control_t *self);

//
// Slider - The slider position changed.
//
int EZ_slider_OnSliderPositionChanged(ez_control_t *self);

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
int EZ_slider_OnResize(ez_control_t *self);

//
// Slider - Key event.
//
int EZ_slider_OnKeyDown(ez_control_t *self, int key, int unichar);

// =========================================================================================
// Scrollbar
// =========================================================================================

#define EZ_SCROLLBAR_ID 3

typedef enum ez_orientation_s
{
	vertical	= 0,
	horizontal	= 1
} ez_orientation_t;

typedef enum ez_scrollbar_flags_e
{
	target_parent	= (1 << 0)
} ez_scrollbar_flags_t;

typedef enum ez_scrollbar_iflags_e
{
	sliding			= (1 << 0),
	scrolling		= (1 << 1)
} ez_scrollbar_iflags_t;

typedef struct ez_scrollbar_eventcount_s
{
	int OnTargetChanged;
} ez_scrollbar_eventcount_t;

typedef struct ez_scrollbar_events_s
{
	ez_event_fp	OnTargetChanged;
} ez_scrollbar_events_t;

typedef struct ez_scrollbar_eventhandlers_s
{
	ez_eventhandler_t		*OnTargetChanged;
} ez_scrollbar_eventhandlers_t;

typedef struct ez_scrollbar_s
{
	ez_control_t				super;				// The super class.

	ez_scrollbar_events_t		events;
	ez_scrollbar_eventhandlers_t event_handlers;
	ez_scrollbar_eventcount_t	inherit_levels;
	ez_scrollbar_eventcount_t	override_counts;

	ez_button_t					*back;				// Up / left button depending on the scrollbars orientation.
	ez_button_t					*slider;			// The slider button.
	ez_button_t					*forward;			// Down / right button.
	
	ez_orientation_t			orientation;		// The orientation of the scrollbar, vertical / horizontal.
	int							scroll_area;		// The width or height (depending on orientation) of the area between
													// the forward/back buttons. That is, the area you can move the slider in.

	int							slider_minsize;		// The minimum size of the slider button.
	int							scroll_delta_x;		// How much should the scrollbar scroll it's parent when the scroll buttons are pressed?
	int							scroll_delta_y;

	ez_control_t				*target;			// The target of the scrollbar if the parent isn't targeted (ext_flag & target_parent).

	ez_scrollbar_flags_t		ext_flags;			// External flags for the scrollbar.
	ez_scrollbar_iflags_t		int_flags;			// The internal flags for the scrollbar.

	int							override_count;		// These are needed so that subclasses can override slider specific events.
	int							inheritance_level;
} ez_scrollbar_t;

//
// Scrollbar - Creates a new scrollbar and initializes it.
//
ez_scrollbar_t *EZ_scrollbar_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Scrollbar - Initializes a scrollbar.
//
void EZ_scrollbar_Init(ez_scrollbar_t *scrollbar, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Scrollbar - Destroys the scrollbar.
//
int EZ_scrollbar_Destroy(ez_control_t *self, qbool destroy_children);

//
// Scrollbar - Set the target control that the scrollbar should scroll.
//
void EZ_scrollbar_SetTarget(ez_scrollbar_t *scrollbar, ez_control_t *target);

//
// Scrollbar - Set if the scrollbar should be vertical or horizontal.
//
void EZ_scrollbar_SetIsVertical(ez_scrollbar_t *scrollbar, qbool is_vertical);

//
// Scrollbar - Sets if the target for the scrollbar should be it's parent, or a specified target control.
//				(The target controls OnScroll event handler will be used if it's not the parent)
//
void EZ_scrollbar_SetTargetIsParent(ez_scrollbar_t *scrollbar, qbool targetparent);

//
// Scrollbar - The target of the scrollbar changed.
//
int EZ_scrollbar_OnTargetChanged(ez_control_t *self);

//
// Scrollbar - OnResize event.
//
int EZ_scrollbar_OnResize(ez_control_t *self);

//
// Scrollbar - OnParentResize event.
//
int EZ_scrollbar_OnParentResize(ez_control_t *self);

//
// Scrollbar - OnMouseDown event.
//
int EZ_scrollbar_OnMouseDown(ez_control_t *self, mouse_state_t *ms);

//
// Scrollbar - OnMouseUpOutside event.
//
int EZ_scrollbar_OnMouseUpOutside(ez_control_t *self, mouse_state_t *ms);

//
// Scrollbar - Mouse event.
//
int EZ_scrollbar_OnMouseEvent(ez_control_t *self, mouse_state_t *ms);

//
// Scrollbar - OnParentScroll event.
//
int EZ_scrollbar_OnParentScroll(ez_control_t *self);

// =========================================================================================
// Scrollpane
// =========================================================================================

#define EZ_SCROLLPANE_ID 4

typedef enum ez_scrollpane_flags_e
{
	always_v_scrollbar	= (1 << 0),
	always_h_scrollbar	= (1 << 1)
} ez_scrollpane_flags_t;		

typedef enum ez_scrollpane_iflags_e
{
	show_v_scrollbar		= (1 << 0),
	show_h_scrollbar		= (1 << 1)
} ez_scrollpane_iflags_t;

typedef struct ez_scrollpane_eventcount_s
{
	int OnTargetChanged;
	int OnScrollbarThicknessChanged;
} ez_scrollpane_eventcount_t;

typedef struct ez_scrollpane_events_s
{
	ez_event_fp	OnTargetChanged;
	ez_event_fp	OnTargetResize;
	ez_event_fp	OnTargetScroll;
	ez_event_fp	OnScrollbarThicknessChanged;
} ez_scrollpane_events_t;

typedef struct ez_scrollpane_eventhandlers_s
{
	ez_eventhandler_t		*OnTargetChanged;
	ez_eventhandler_t		*OnTargetResize;
	ez_eventhandler_t		*OnTargetScroll;
	ez_eventhandler_t		*OnScrollbarThicknessChanged;
} ez_scrollpane_eventhandlers_t;

typedef struct ez_scrollpane_s
{
	ez_control_t				super;				// The super class.

	ez_scrollpane_events_t		events;
	ez_scrollpane_eventhandlers_t event_handlers;
	ez_scrollpane_eventcount_t	inherit_levels;
	ez_scrollpane_eventcount_t	override_counts;

	ez_control_t				*target;			// The target of the scrollbar if the parent isn't targeted (ext_flag & target_parent).
	ez_control_t				*prev_target;		// The previous target control. We keep this to clean up when we change target.

	ez_scrollbar_t				*v_scrollbar;		// The vertical scrollbar.
	ez_scrollbar_t				*h_scrollbar;		// The horizontal scrollbar.

	int							scrollbar_thickness; // The thickness of the scrollbars.

	ez_scrollpane_flags_t		ext_flags;			// External flags for the scrollbar.
	ez_scrollpane_iflags_t		int_flags;			// The internal flags for the scrollbar.

	int							override_count;		// These are needed so that subclasses can override slider specific events.
	int							inheritance_level;
} ez_scrollpane_t;

//
// Scrollpane - Creates a new Scrollpane and initializes it.
//
ez_scrollpane_t *EZ_scrollpane_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Scrollpane - Initializes a Scrollpane.
//
void EZ_scrollpane_Init(ez_scrollpane_t *scrollpane, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Scrollpane - Destroys the scrollpane.
//
int EZ_scrollpane_Destroy(ez_control_t *self, qbool destroy_children);

//
// Scrollpane - Set the target control of the scrollpane (the one to be scrolled).
//
void EZ_scrollpane_SetTarget(ez_scrollpane_t *scrollpane, ez_control_t *target);

//
// Scrollpane - Set the thickness of the scrollbar controls.
//
void EZ_scrollpane_SetScrollbarThickness(ez_scrollpane_t *scrollpane, int scrollbar_thickness);

//
// Scrollpane - The target control changed.
//
int EZ_scrollpane_OnTargetChanged(ez_control_t *self);

//
// Scrollpane - The scrollbar thickness changed.
//
int EZ_scrollpane_OnScrollbarThicknessChanged(ez_control_t *self);

//
// Scrollpane - OnResize event.
//
int EZ_scrollpane_OnResize(ez_control_t *self);

#endif // __EZ_CONTROLS_H__	



