#ifndef __EZ_CONTROLS_H__
#define __EZ_CONTROLS_H__

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
typedef int (*ez_control_key_handler_fp) (struct ez_control_s *self, int key, int unichar);
typedef int (*ez_control_destroy_handler_fp) (struct ez_control_s *self, qbool destroy_children);

#define CONTROL_IS_CONTAINED(self) (self->parent && (self->flags & CONTROL_CONTAINED))
#define MOUSE_OUTSIDE_PARENT_GENERIC(ctrl, mouse_state, axis, h)									\
	(ctrl->parent																					\
	&& (((int) mouse_state->axis <= ctrl->parent->absolute_##axis)							\
	|| ((int) mouse_state->axis >= (ctrl->parent->absolute_##axis + ctrl->parent->h))))	\

#define MOUSE_OUTSIDE_PARENT_X(ctrl, mouse_state) MOUSE_OUTSIDE_PARENT_GENERIC(ctrl, mouse_state, x, width)
#define MOUSE_OUTSIDE_PARENT_Y(ctrl, mouse_state) MOUSE_OUTSIDE_PARENT_GENERIC(ctrl, mouse_state, y, height)

#define CONTROL_EVENT_HANDLER(name, ctrl, eventhandler) (ctrl->name.eventhandler)

//
// Raises an event.
//
#ifdef __INTEL_COMPILER

#define CONTROL_RAISE_EVENT(retval, ctrl, eventhandler, ...)										\
{																									\
	int temp = 0;																					\
	int *p = (int *)retval;																			\
	((ez_control_t *)ctrl)->override_count = ((ez_control_t *)ctrl)->inheritance_level;				\
	if(CONTROL_EVENT_HANDLER(events, ctrl, eventhandler))											\
	temp = CONTROL_EVENT_HANDLER(events, (ctrl), eventhandler)((ctrl), __VA_ARGS__);				\
	if(p) (*p) = temp;																				\
}																									\

#else

#define CONTROL_RAISE_EVENT(retval, ctrl, eventhandler, ...)										\
{																									\
	int temp = 0;																					\
	int *p = (int *)retval;																			\
	((ez_control_t *)ctrl)->override_count = ((ez_control_t *)ctrl)->inheritance_level;				\
	if(CONTROL_EVENT_HANDLER(events, ctrl, eventhandler))											\
	temp = CONTROL_EVENT_HANDLER(events, (ctrl), eventhandler)((ctrl), ##__VA_ARGS__);				\
	if(p) (*p) = temp;																				\
}																									\

#endif

//
// Calls a event handler function.
//
#ifdef __INTEL_COMPILER

#define CONTROL_EVENT_HANDLER_CALL(retval, ctrl, eventhandler, ...)														\
{																														\
	int *p = (int *)retval, temp = 0;																					\
	if(CONTROL_EVENT_HANDLER(event_handlers, (ctrl), eventhandler))														\
	{																													\
		if(((ez_control_t *)ctrl)->override_count == 0)																	\
			temp = CONTROL_EVENT_HANDLER(event_handlers, (ctrl), eventhandler)((ez_control_t *)ctrl, __VA_ARGS__);		\
		else																											\
			((ez_control_t *)ctrl)->override_count--;																	\
	}																													\
	if(p) (*p) = temp;																									\
}																														\

#else

#define CONTROL_EVENT_HANDLER_CALL(retval, ctrl, eventhandler, ...)														\
{																														\
	int *p = (int *)retval, temp = 0;																					\
	if(CONTROL_EVENT_HANDLER(event_handlers, (ctrl), eventhandler))														\
	{																													\
		if(((ez_control_t *)ctrl)->override_count == 0)																	\
		temp = CONTROL_EVENT_HANDLER(event_handlers, (ctrl), eventhandler)((ez_control_t *)ctrl, ##__VA_ARGS__);		\
		else																											\
			((ez_control_t *)ctrl)->override_count--;																	\
}																														\
	if(p) (*p) = temp;																									\
}																														\

#endif

//
// Validates a method call -
// Should be called first in all method calls for inherited classes to make sure
// the call isn't attempted on an object 
//
#define CONTROL_VALIDATE_CALL(ctrl, id, funcname) if( ctrl##->CLASS_ID < id ) Sys_Error("EZ_controls: Cannot pass a less specific object as an argument to %s\n", funcname)

#define EZ_CONTROL_INHERITANCE_LEVEL	0
#define EZ_CLASS_ID						0

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
	ez_control_handler_fp			OnScroll;
	ez_control_handler_fp			OnResize;
	ez_control_handler_fp			OnParentResize;
	ez_control_handler_fp			OnGotFocus;
	ez_control_handler_fp			OnLostFocus;
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
	int					CLASS_ID;						// An ID unique for this class, this is set at initilization
														// and should never be changed after that.
	char				*name;							// The name of the control.
	char				*description;					// A short description of the control.
	
	int	 				x;								// Relative position to it's parent.
	int					y;

	int					prev_x;							// Previous position.
	int					prev_y;

	int					absolute_x;						// The absolute screen coordinates for the control.
	int					absolute_y;	
	
	int 				width;							// Size.
	int					height;
	
	int					prev_width;						// Previous size.
	int					prev_height;

	int					virtual_x;						// The relative position in the virtual window that the "real" window is showing.
	int					virtual_y;

	int					absolute_virtual_x;				// The absolute position of the virtual window on the screen.
	int					absolute_virtual_y;

	int					virtual_width;					// The virtual size of the control (scrollable area).
	int					virtual_height;

	int					prev_virtual_width;				// The previous virtual size of the control.
	int					prev_virtual_height;

	int					virtual_width_min;				// The virtual min size for the control.
	int					virtual_height_min;
	
	int					width_max;						// Max/min sizes for the control.
	int					width_min;
	int					height_max;
	int					height_min;

	int					bound_top;						// The bounds the control is allowed to draw within.
	int					bound_left;
	int					bound_right;
	int					bound_bottom;

	ez_anchor_t			anchor_flags;					// Which parts of it's parent the control is anchored to.

	int					resize_handle_thickness;		// The thickness of the resize handles on the sides of the control.

	int					draw_order;						// The order the control is drawn in.
	int					tab_order;						// The tab number of the control.
	
	int					flags;							// Flags defining such as visibility, focusability, and manipulation
														// being performed on the control.

	byte				background_color[4];			// The background color of the control RGBA.
	mpic_t				*background;					// The background picture.
	float				opacity;						// The opacity of the control.

	mouse_state_t		prev_mouse_state;				// The last mouse event that was passed on to this control.

	ez_control_events_t			events;					// The base reaction for events. Is only set at initialization.
	ez_control_events_t			event_handlers;			// Can be set by the user of the class to react to events.

	struct ez_control_s			*parent;				// The parent of the control. Only the root node has no parent.
	ez_double_linked_list_t		children;				// List of children belonging to the control.

	struct ez_tree_s			*control_tree;			// The control tree the control belongs to.

	int							override_count;			// The current number of times the event handler needs to be called before it's executed.
	int							inheritance_level;		// The class's level of inheritance. Control -> SubControl -> SubSubControl
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

typedef struct ez_label_s
{
	ez_control_t	super;

	char			*text;						// The text to be shown in the label.
	int				text_flags;					// Flags for how the text in the label is formatted.
	int				wordwraps[LABEL_MAX_WRAPS];	// Indexes for where the text has been wordwrapped.
	clrinfo_t		color;						// The text color of the label.
	float			scale;						// The scale of the text in the label.
	int				select_start;				// At what index the currently selected text starts at (from the beginning of the string).
	int				select_end;					// The end index of the selected text.
	int				caret_pos;					// The position of the caret.
	int				row_clicked;				// The row that was clicked.
	int				col_clicked;				// The column that was clicked.
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
// Label - Gets the size of the selected text.
//
int EZ_label_GetSelectedTextSize(ez_label_t *label);

//
// Label - Gets the selected text.
//
void EZ_label_GetSelectedText(ez_label_t *label, char *target, int target_size);

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
// Label - Happens when the control has resized.
//
int EZ_label_OnResize(ez_control_t *self);

//
// Label - On Draw.
//
int EZ_label_OnDraw(ez_control_t *label);

//
// Label - Key event.
//
int EZ_label_OnKeyEvent(ez_control_t *self, int key, int unichar);

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


#endif // __EZ_CONTROLS_H__

