#ifndef __EZ_CONTROLS_H__
#define __EZ_CONTROLS_H__

#define POINT_IN_RECTANGLE(p_x, p_y, r_x, r_y, r_width, r_height) ((p_x >= r_x) && (p_y >= r_y) && (p_x <= (r_x + r_width)) && (p_y <= (r_y + r_height)))

#define CONTROL_ENABLED				(1 << 0)	// Is the control usable?
#define CONTROL_MOVABLE				(1 << 1)	// Can the control be moved?
#define CONTROL_RESIZABLE			(1 << 2)	// Can the control be resized?
#define CONTROL_MOVING				(1 << 3)	// Is the control in the process of being moved?
#define CONTROL_RESIZING			(1 << 4)	// Is the control in the process of being resized?
#define CONTROL_FOCUSABLE			(1 << 5)	// Can the control be given focus?
#define CONTROL_FOCUSED				(1 << 6)	// Is the control currently in focus?
#define CONTROL_RESIZE_HORIZONTAL	(1 << 7)	// Is the control resizeable horizontally?
#define CONTROL_RESIZE_VERTICAL		(1 << 8)	// Is the control resizeable vertically?
#define CONTROL_CONTAINED			(1 << 9)	// Is the control contained within it's parent or can it go outside its edges?
#define CONTROL_CLICKED				(1 << 10)	// Is the control being clicked? (If the mouse button is released outside the control a click event isn't raised).

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
// Control Tree - Orders the draw list based on the draw order property.
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
qbool EZ_tree_KeyEvent(ez_tree_t *tree, int key, int unichar);

//
// Tree Control - Finds any orphans and adds them to the root control.
//
void EZ_tree_UnOrphanizeChildren(ez_tree_t *tree);

// =========================================================================================
// Control
// =========================================================================================

//
// Control - Function pointer types.
//
typedef int (*ez_control_handler_fp) (struct ez_control_s *self);
typedef int (*ez_control_mouse_handler_fp) (struct ez_control_s *self, mouse_state_t *mouse_state);
typedef int (*ez_control_key_handler_fp) (struct ez_control_s *self, int key, int unichar);
typedef int (*ez_control_destroy_handler_fp) (struct ez_control_s *self, qbool destroy_children);

#define CONTROL_EVENT_HANDLER(name, ctrl, eventhandler) (ctrl##->##name.##eventhandler)

//
// Raises an event.
//
#define CONTROL_RAISE_EVENT(retval, ctrl, eventhandler, ...)										\
{																									\
	int temp = 0;																					\
	int *p = (int *)retval;																			\
	(ctrl)->override_count = (ctrl)->inheritance_level;												\
	if(CONTROL_EVENT_HANDLER(events, ctrl, eventhandler))											\
		temp = CONTROL_EVENT_HANDLER(events, (ctrl), eventhandler)((ctrl), __VA_ARGS__);			\
	if(p) (*p) = temp;																				\
}																									\

//
// Calls a event handler function.
//
#define CONTROL_EVENT_HANDLER_CALL(retval, ctrl, eventhandler, ...)									\
{																									\
	int *p = (int *)retval, temp = 0;																\
	if(CONTROL_EVENT_HANDLER(event_handlers, (ctrl), eventhandler))									\
	{																								\
		if(ctrl->override_count == 0)																\
			temp = CONTROL_EVENT_HANDLER(event_handlers, (ctrl), eventhandler)(ctrl, __VA_ARGS__);	\
		else																						\
			ctrl->override_count--;																	\
	}																								\
	if(p) (*p) = temp;																				\
}																									\

#define EZ_CONTROL_INHERITANCE_LEVEL	0

typedef struct ez_control_events_s
{
	ez_control_mouse_handler_fp		OnMouseEvent;
	ez_control_mouse_handler_fp		OnMouseClick;
	ez_control_mouse_handler_fp		OnMouseUp;
	ez_control_mouse_handler_fp		OnMouseDown;
	ez_control_mouse_handler_fp		OnMouseHover;
	ez_control_mouse_handler_fp		OnMouseEnter;
	ez_control_mouse_handler_fp		OnMouseLeave;
	ez_control_key_handler_fp		OnKeyEvent;
	ez_control_handler_fp			OnLayoutChildren;
	ez_control_handler_fp			OnDraw;
	ez_control_destroy_handler_fp	OnDestroy;
	ez_control_handler_fp			OnMove;
	ez_control_handler_fp			OnResize;
	ez_control_handler_fp			OnGotFocus;
	ez_control_handler_fp			OnLostFocus;
} ez_control_events_t;

typedef struct ez_control_s
{
	char				*name;
	char				*description;
	int	 				x;
	int					y;
	int 				width;
	int					height;
	
	int					width_max;
	int					width_min;
	int					height_max;
	int					height_min;

	int					absolute_x;						// The absolute screen coordinates for the control.
	int					absolute_y;	

	int					draw_order;						// The order the control is drawn in.
	int					tab_order;						// The tab number of the control.
	
	int					flags;

	byte				background_color[4];
	mpic_t				*background;

	mouse_state_t		prev_mouse_state;

	ez_control_events_t			events;					// The base reaction for events. Is only set at initialization.
	ez_control_events_t			event_handlers;			// Can be set by the user of the class to react to events.

	struct ez_control_s			*parent;				// The parent of the control. Only the root node has no parent.
	ez_double_linked_list_t		children;				// List of children belonging to the control.

	struct ez_tree_s			*control_tree;			// The control tree the control belongs to.

	int							override_count;			// The current number of times the event handler needs to be called before it's executed.
	int							inheritance_level;		// The class's level of inheritance. Control -> SubControl -> SubSubControl
														// SubSubControl would have a value of 2.
														// !!! This is class specific, should never be changed !!!
} ez_control_t;

//
// Control - Initializes a control.
//
ez_control_t *EZ_control_Init(ez_tree_t *tree, ez_control_t *parent, 
							  char *name, char *description, 
							  int x, int y, int width, int height, 
							  char *background_name, int flags);

//
// Control - Destroys a specified control.
//
int EZ_control_Destroy(ez_control_t *self, qbool destroy_children);

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
// Control - The control was resized.
//
int EZ_control_OnResize(ez_control_t *self);

//
// Control - Layouts children.
//
int EZ_control_OnLayoutChildren(ez_control_t *self);

//
// Control - Draws the control.
//
int EZ_control_OnDraw(ez_control_t *self);

//
// Control - Key event.
//
int EZ_control_OnKeyEvent(ez_control_t *self, int key, int unichar);

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

#endif // __EZ_CONTROLS_H__

