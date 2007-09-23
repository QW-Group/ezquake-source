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

$Id: ez_controls.h,v 1.30 2007-09-23 18:00:42 dkure Exp $
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
	struct ez_control_s		*root;				// The control tree.
	ez_dllist_node_t		*focused_node;		// The node of focused control (from the tablist). 
	ez_double_linked_list_t	drawlist;			// A list with the controls ordered in their drawing order.
	ez_double_linked_list_t	tablist;			// A list with the controls ordered in their tabbing order.
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
// Control Tree - Draws a control tree.
// 
void EZ_tree_Draw(ez_tree_t *tree);

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

// =========================================================================================
// Control
// =========================================================================================

#define CONTROL_ENABLED				(1 << 0)	// Is the control usable?
#define CONTROL_MOVABLE				(1 << 1)	// Can the control be moved?
#define CONTROL_MOVING				(1 << 2)	// Is the control in the process of being moved?
#define CONTROL_FOCUSABLE			(1 << 3)	// Can the control be given focus?
#define CONTROL_FOCUSED				(1 << 4)	// Is the control currently in focus?
#define CONTROL_RESIZE_H			(1 << 5)	// Is the control resizeable horizontally?
#define CONTROL_RESIZE_V			(1 << 6)	// Is the control resizeable vertically?
#define CONTROL_RESIZING_RIGHT		(1 << 7)	// Are we resizing the control to the right?
#define CONTROL_RESIZING_LEFT		(1 << 8)	// Are we resizing the control to the left?
#define CONTROL_RESIZING_TOP		(1 << 9)	// Resizing upwards.
#define CONTROL_RESIZING_BOTTOM		(1 << 10)	// Resizing downards.
#define CONTROL_CONTAINED			(1 << 11)	// Is the control contained within it's parent or can it go outside its edges?
#define CONTROL_CLICKED				(1 << 12)	// Is the control being clicked? (If the mouse button is released outside the control a click event isn't raised).
#define CONTROL_VISIBLE				(1 << 13)	// Is the control visible?
#define CONTROL_MOUSE_OVER			(1 << 14)	// Is the mouse over the control?
#define CONTROL_SCROLLABLE			(1 << 15)	// Is the control scrollable?
#define CONTROL_RESIZEABLE			(1 << 16)	// Is the control resizeable?

//
// Control - Function pointer types.
//
typedef int (*ez_control_handler_fp) (struct ez_control_s *self);
typedef int (*ez_control_mouse_handler_fp) (struct ez_control_s *self, mouse_state_t *mouse_state);
typedef int (*ez_control_key_handler_fp) (struct ez_control_s *self, int key, int unichar, qbool down);
typedef int (*ez_control_keyspecific_handler_fp) (struct ez_control_s *self, int key, int unichar);
typedef int (*ez_control_destroy_handler_fp) (struct ez_control_s *self, qbool destroy_children);

#define CONTROL_IS_CONTAINED(self) (self->parent && (self->flags & CONTROL_CONTAINED))
#define MOUSE_OUTSIDE_PARENT_GENERIC(ctrl, mouse_state, axis, h)									\
	(ctrl->parent																					\
	&& (((int) mouse_state->axis <= ctrl->parent->absolute_##axis)									\
	|| ((int) mouse_state->axis >= (ctrl->parent->absolute_##axis + ctrl->parent->h))))				\

#define MOUSE_OUTSIDE_PARENT_X(ctrl, mouse_state) MOUSE_OUTSIDE_PARENT_GENERIC(ctrl, mouse_state, x, width)
#define MOUSE_OUTSIDE_PARENT_Y(ctrl, mouse_state) MOUSE_OUTSIDE_PARENT_GENERIC(ctrl, mouse_state, y, height)

#define CONTROL_EVENT_HANDLER(name, ctrl, eventhandler) (ctrl->name.eventhandler)

//
// Raises an event. (See the CONTROL_EVENT_HANDLER_CALL for a more detailed explination of how this works).
//
#ifdef __INTEL_COMPILER

#define CONTROL_RAISE_EVENT(retval, ctrl, eventroot, eventhandler, ...)										\
{																											\
	int temp = 0;																							\
	int *p = (int *)retval;																					\
	((eventroot *)ctrl)->override_count = ((eventroot *)ctrl)->inheritance_level;							\
	if(!CONTROL_EVENT_HANDLER(events, ctrl, eventhandler))													\
		Sys_Error("CONTROL_RAISE_EVENT : "#ctrl" ("#eventroot") has no event function for "#eventhandler);	\
	temp = CONTROL_EVENT_HANDLER(events, (ctrl), eventhandler)((ez_control_t *)(ctrl), __VA_ARGS__);		\
	if(p) (*p) = temp;																						\
}																																																						

#else

#define CONTROL_RAISE_EVENT(retval, ctrl, eventroot, eventhandler, ...)										\
{																											\
	int temp = 0;																							\
	int *p = (int *)retval;																					\
	((eventroot *)ctrl)->override_count = ((eventroot *)ctrl)->inheritance_level;							\
	if(!CONTROL_EVENT_HANDLER(events, ctrl, eventhandler))													\
		Sys_Error("CONTROL_RAISE_EVENT : "#ctrl" ("#eventroot") has no event function for "#eventhandler);	\
	temp = CONTROL_EVENT_HANDLER(events, (ctrl), eventhandler)((ez_control_t *)(ctrl), ##__VA_ARGS__);		\
	if(p) (*p) = temp;																						\
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

#define CONTROL_EVENT_HANDLER_CALL(retval, ctrl, eventroot, eventhandler, ...)											\
{																														\
	int *p = (int *)retval, temp = 0;																					\
	if(CONTROL_EVENT_HANDLER(event_handlers, (ctrl), eventhandler))														\
	{																													\
		if(((eventroot *)ctrl)->override_count == 0)																	\
			temp = CONTROL_EVENT_HANDLER(event_handlers, (ctrl), eventhandler)((ez_control_t *)ctrl, __VA_ARGS__);		\
		else																											\
			((eventroot *)ctrl)->override_count--;																		\
	}																													\
	if(p) (*p) = temp;																									\
}																														\

#else

#define CONTROL_EVENT_HANDLER_CALL(retval, ctrl, eventroot, eventhandler, ...)											\
{																														\
	int *p = (int *)retval, temp = 0;																					\
	if(CONTROL_EVENT_HANDLER(event_handlers, (ctrl), eventhandler))														\
	{																													\
		if(((eventroot *)ctrl)->override_count == 0)																	\
			temp = CONTROL_EVENT_HANDLER(event_handlers, (ctrl), eventhandler)((ez_control_t *)ctrl, ##__VA_ARGS__);	\
		else																											\
			((eventroot *)ctrl)->override_count--;																		\
	}																													\
	if(p) (*p) = temp;																									\
}

#endif // __INTEL_COMPILER

#define EZ_CONTROL_INHERITANCE_LEVEL	0
#define EZ_CLASS_ID						0

typedef struct ez_control_events_s
{
	ez_control_mouse_handler_fp			OnMouseEvent;
	ez_control_mouse_handler_fp			OnMouseClick;
	ez_control_mouse_handler_fp			OnMouseUp;
	ez_control_mouse_handler_fp			OnMouseUpOutside;
	ez_control_mouse_handler_fp			OnMouseDown;
	ez_control_mouse_handler_fp			OnMouseHover;
	ez_control_mouse_handler_fp			OnMouseEnter;
	ez_control_mouse_handler_fp			OnMouseLeave;
	ez_control_key_handler_fp			OnKeyEvent;
	ez_control_keyspecific_handler_fp	OnKeyDown;
	ez_control_keyspecific_handler_fp	OnKeyUp;
	ez_control_handler_fp				OnLayoutChildren;
	ez_control_handler_fp				OnDraw;
	ez_control_destroy_handler_fp		OnDestroy;
	ez_control_handler_fp				OnMove;
	ez_control_handler_fp				OnScroll;
	ez_control_handler_fp				OnResize;
	ez_control_handler_fp				OnParentResize;
	ez_control_handler_fp				OnGotFocus;
	ez_control_handler_fp				OnLostFocus;
	ez_control_handler_fp				OnVirtualResize;
	ez_control_handler_fp				OnMinVirtualResize;
} ez_control_events_t;

typedef enum ez_anchor_e
{
	anchor_none		= (1 << 0),
	anchor_left		= (1 << 1), 
	anchor_right	= (1 << 2),
	anchor_top		= (1 << 3),
	anchor_bottom	= (1 << 4)
} ez_anchor_t;

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

	ez_anchor_t				anchor_flags;			// Which parts of it's parent the control is anchored to.

	int						resize_handle_thickness;// The thickness of the resize handles on the sides of the control.

	int						draw_order;				// The order the control is drawn in.
	int						tab_order;				// The tab number of the control.
	
	int						flags;					// Flags defining such as visibility, focusability, and manipulation
													// being performed on the control.

	byte					background_color[4];	// The background color of the control RGBA.
	mpic_t					*background;			// The background picture.
	float					opacity;				// The opacity of the control.

	mouse_state_t			prev_mouse_state;		// The last mouse event that was passed on to this control.

	ez_control_events_t		events;					// The base reaction for events. Is only set at initialization.
	ez_control_events_t		event_handlers;			// Can be set by the user of the class to react to events.

	struct ez_control_s		*parent;				// The parent of the control. Only the root node has no parent.
	ez_double_linked_list_t	children;				// List of children belonging to the control.

	struct ez_tree_s		*control_tree;			// The control tree the control belongs to.

	int						override_count;			// The current number of times the event handler needs to be called before it's executed.
	int						inheritance_level;		// The class's level of inheritance. Control -> SubControl -> SubSubControl
													// SubSubControl would have a value of 2.
													// !!! This is class specific, should never be changed except in init function !!!
} ez_control_t;

//
// Control - Creates a new control and initializes it.
//
ez_control_t *EZ_control_Create(ez_tree_t *tree, ez_control_t *parent, 
							  char *name, char *description, 
							  int x, int y, int width, int height, 
							  char *background_name, int flags);

//
// Control - Initializes a control and adds it to the specified control tree.
//

//
// Control - Initializes a control.
//
void EZ_control_Init(ez_control_t *control, ez_tree_t *tree, ez_control_t *parent, 
							  char *name, char *description, 
							  int x, int y, int width, int height, 
							  char *background_name, int flags);

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
// Control - Sets the OnDestroy event handler.
//
void EZ_control_SetOnDestroy(ez_control_t *self, ez_control_destroy_handler_fp OnDestroy);

//
// Control - Sets the OnLayoutChildren event handler.
//
void EZ_control_SetOnLayoutChildren(ez_control_t *self, ez_control_handler_fp OnLayoutChildren);

//
// Control - Sets the OnMove event handler.
//
void EZ_control_SetOnMove(ez_control_t *self, ez_control_handler_fp OnMove);

//
// Control - Sets the OnResize event handler.
//
void EZ_control_SetOnResize(ez_control_t *self, ez_control_handler_fp OnResize);

//
// Control - Sets the OnParentResize event handler.
//
void EZ_control_SetOnParentResize(ez_control_t *self, ez_control_handler_fp OnParentResize);

//
// Control - Sets the OnVirtualResize event handler.
//
void EZ_control_SetOnVirtualResize(ez_control_t *self, ez_control_handler_fp OnVirtualResize);

//
// Control - Sets the OnMinVirtualResize event handler.
//
void EZ_control_SetOnMinVirtualResize(ez_control_t *self, ez_control_handler_fp OnMinVirtualResize);

//
// Control - Sets the OnKeyEvent event handler.
//
void EZ_control_SetOnKeyEvent(ez_control_t *self, ez_control_key_handler_fp OnKeyEvent);

//
// Control - Sets the OnLostFocus event handler.
//
void EZ_control_SetOnLostFocus(ez_control_t *self, ez_control_handler_fp OnLostFocus);

//
// Control - Sets the OnGotFocus event handler.
//
void EZ_control_SetOnGotFocus(ez_control_t *self, ez_control_handler_fp OnGotFocus);

//
// Control - Sets the OnMouseHover event handler.
//
void EZ_control_SetOnMouseHover(ez_control_t *self, ez_control_mouse_handler_fp OnMouseHover);

//
// Control - Sets the OnMouseLeave event handler.
//
void EZ_control_SetOnMouseLeave(ez_control_t *self, ez_control_mouse_handler_fp OnMouseLeave);

//
// Control - Sets the OnMouseEnter event handler.
//
void EZ_control_SetOnMouseEnter(ez_control_t *self, ez_control_mouse_handler_fp OnMouseEnter);

//
// Control - Sets the OnMouseClick event handler.
//
void EZ_control_SetOnMouseClick(ez_control_t *self, ez_control_mouse_handler_fp OnMouseClick);

//
// Control - Sets the OnMouseEvent event handler.
//
void EZ_control_SetOnMouseUp(ez_control_t *self, ez_control_mouse_handler_fp OnMouseUp);

//
// Control - Sets the OnMouseUp event handler.
//
void EZ_control_SetOnMouseUp(ez_control_t *self, ez_control_mouse_handler_fp OnMouseUp);

//
// Control - Sets the OnMouseDown event handler.
//
void EZ_control_SetOnMouseDown(ez_control_t *self, ez_control_mouse_handler_fp OnMouseDown);

//
// Control - Sets the OnMouseEvent event handler.
//
void EZ_control_SetOnMouseEvent(ez_control_t *self, ez_control_mouse_handler_fp OnMouseEvent);

//
// Control - Sets the OnDraw event handler.
//
void EZ_control_SetOnDraw(ez_control_t *self, ez_control_handler_fp OnDraw);

//
// Control - Set color of a control.
//
void EZ_control_SetBackgroundColor(ez_control_t *self, byte r, byte g, byte b, byte alpha);

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
// Control - Convenient function for changing the scroll position by a specified amount.
//
void EZ_control_SetScrollChange(ez_control_t *self, int delta_scroll_x, int delta_scroll_y);

//
// Control - The control was resized.
//
int EZ_control_OnResize(ez_control_t *self);

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

#define EZ_LABEL_INHERITANCE_LEVEL		1
#define EZ_LABEL_ID						2

#define LABEL_LARGEFONT		(1 << 0)	// Use the large font.
#define LABEL_AUTOSIZE		(1 << 1)	// Autosize the label to fit the text (if it's not wrapped).
#define LABEL_AUTOELLIPSIS	(1 << 2)	// Show ... at the end of the text if it doesn't fit within the label.
#define LABEL_WRAPTEXT		(1 << 3)	// Wrap the text to fit inside the label.
#define LABEL_SELECTING		(1 << 4)	// Are we selecting text?

#define LABEL_MAX_WRAPS		512
#define LABEL_LINE_SIZE		1024

// Is any text selected in the label?
#define LABEL_TEXT_SELECTED(label) ((label->select_start > -1) && (label->select_end > - 1) && abs(label->select_end - label->select_start) > 0)

typedef struct ez_label_events_s
{
	ez_control_handler_fp	OnTextChanged;			// Event raised when the text in the label is changed.
	ez_control_handler_fp	OnCaretMoved;			// The caret was moved.
	ez_control_handler_fp	OnTextScaleChanged;		// The scale of the text changed.
} ez_label_events_t;

typedef struct ez_label_textpos_s
{
	int index;
	int row;
	int col;
} ez_label_textpos_t;

typedef struct ez_label_s
{
	ez_control_t		super;

	ez_label_events_t	events;						// Specific events for the label control.
	ez_label_events_t	event_handlers;				// Specific event handlers for the label control.
	char				*text;						// The text to be shown in the label.
	int					text_length;				// The length of the label text (excluding \0).
	int					text_flags;					// Flags for how the text in the label is formatted.
	ez_label_textpos_t	wordwraps[LABEL_MAX_WRAPS];	// Positions in the text where line breaks are located.
	clrinfo_t			color;						// The text color of the label.
	float				scale;						// The scale of the text in the label.
	int					scaled_char_size;			// The actual size of the characters in the text.
	int					select_start;				// At what index the currently selected text starts at (this can be greater than select_end).
	int					select_end;					// The end index of the selected text.
	ez_label_textpos_t	caret_pos;					// The position of the caret (index / row / column).
	int					num_rows;					// The current number of rows in the label.
	int					num_cols;					// The current number of cols (for the longest row).

	int					override_count;				// These are needed so that subclasses can override label specific events.
	int					inheritance_level;
} ez_label_t;

ez_label_t *EZ_label_Create(ez_tree_t *tree, ez_control_t *parent, 
							  char *name, char *description, 
							  int x, int y, int width, int height, 
							  char *background_name, 
							  int flags, int text_flags,
							  char *text, clrinfo_t text_color);

void EZ_label_Init(ez_label_t *label, ez_tree_t *tree, ez_control_t *parent, 
				  char *name, char *description, 
				  int x, int y, int width, int height, 
				  char *background_name, 
				  int flags, int text_flags,
				  char *text, clrinfo_t text_color);

//
// Label - Destroys a label control.
//
int EZ_label_Destroy(ez_control_t *self, qbool destroy_children);

//
// Label - Sets the event handler for the OnTextChanged event.
//
void EZ_label_SetOnTextChanged(ez_label_t *label, ez_control_handler_fp OnTextChanged);

//
// Label - Sets the event handler for the OnTextScaleChanged event.
//
void EZ_label_SetOnTextScaleChanged(ez_label_t *label, ez_control_handler_fp OnTextScaleChanged);

//
// Label - Sets the event handler for the OnCaretMoved event.
//
void EZ_label_SetOnTextOnCaretMoved(ez_label_t *label, ez_control_handler_fp OnCaretMoved);

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
// Label - Appends text at the given position in the text.
//
void EZ_label_AppendText(ez_label_t *label, int position, const char *append_text);

//
// Label - Set the caret position.
//
void EZ_label_SetCaretPosition(ez_label_t *label, int caret_pos);

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

#define EZ_BUTTON_INHERITANCE_LEVEL		1
#define EZ_BUTTON_ID					1

typedef struct ez_button_events_s
{
	ez_control_handler_fp	OnAction;			// The event that's raised when the button is clicked / activated via a button.
} ez_button_events_t;

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
	ez_button_events_t		event_handlers;		// Specific event handlers for the button control.

	mpic_t					*focused_image;		// The image for the button when it is focused.
	mpic_t					*normal_image;		// The normal image used for the button.
	mpic_t					*hover_image;		// The image that is shown for the button when the mouse is over it.
	mpic_t					*pressed_image;		// The image that is shown when the button is pressed.

	byte					color_focused[4];	// Color of the focus indicator of the button.
	byte					color_normal[4];	// The normal color of the button.
	byte					color_hover[4];		// Color when the button is hovered.
	byte					color_pressed[4];	// Color when the button is pressed.

	int						padding_top;
	int						padding_left;
	int						padding_right;
	int						padding_bottom;
	char					*text;				// The text for the button.
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
							  char *background_name, char *hover_image, char *pressed_image,
							  int flags);

//
// Button - Initializes a button.
//
void EZ_button_Init(ez_button_t *button, ez_tree_t *tree, ez_control_t *parent, 
							  char *name, char *description, 
							  int x, int y, int width, int height, 
							  char *background_name, char *hover_image, char *pressed_image,
							  int flags);

//
// Button - Destroys the button.
//
void EZ_button_Destroy(ez_control_t *self, qbool destroy_children);

//
// Button - OnAction event handler.
//
int EZ_button_OnAction(ez_control_t *self);

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
void EZ_button_SetOnAction(ez_button_t *self, ez_control_handler_fp OnAction);

//
// Button - OnDraw event.
//
int EZ_button_OnDraw(ez_control_t *self);

// =========================================================================================
// Slider
// =========================================================================================

#define EZ_SLIDER_INHERITANCE_LEVEL		1
#define EZ_SLIDER_ID					2

#define SLIDER_DRAGGING					(1 << 0)

typedef struct ez_slider_events_s
{
	ez_control_handler_fp	OnSliderPositionChanged;
	ez_control_handler_fp	OnMaxValueChanged;
	ez_control_handler_fp	OnScaleChanged;
} ez_slider_events_t;

typedef struct ez_slider_s
{
	ez_control_t			super;				// The super class.

	ez_slider_events_t		events;
	ez_slider_events_t		event_handlers;

	int						slider_flags;		// Slider specific flags.
	int						slider_pos;			// The position of the slider.
	int						real_slider_pos;	// The actual slider position in pixels.
	int						max_value;			// The max value allowed for the slider.
	int						min_value;			// The min value allowed for the slider.
	float					gap_size;			// The pixel gap between each value.
	float					scale;				// The scale of the characters used for drawing the slider.
	int						scaled_char_size;	// The scaled character size in pixels after applying scale to the slider chars.

	int						override_count;		// These are needed so that subclasses can override slider specific events.
	int						inheritance_level;
} ez_slider_t;

//
// Slider - Creates a new button and initializes it.
//
ez_slider_t *EZ_slider_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  char *background_name,
							  int flags);

//
// Slider - Initializes a button.
//
void EZ_slider_Init(ez_slider_t *slider, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  char *background_name,
							  int flags);

//
// Slider - Event handler for OnSliderPositionChanged.
//
void EZ_slider_SetOnSliderPositionChanged(ez_slider_t *slider, ez_control_handler_fp OnSliderPositionChanged);

//
// Slider - Event handler for OnSliderPositionChanged.
//
void EZ_slider_SetOnScaleChanged(ez_slider_t *slider, ez_control_handler_fp OnScaleChanged);

//
// Slider - Event handler for OnMaxValueChanged.
//
void EZ_slider_SetOnMaxValueChanged(ez_slider_t *slider, ez_control_handler_fp OnMaxValueChanged);

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

#endif // __EZ_CONTROLS_H__

