#ifndef __EZ_CONTROLS_H__
#define __EZ_CONTROLS_H__

#define POINT_IN_RECTANGLE(p_x, p_y, r_x, r_y, r_width, r_height) ((p_x >= r_x) && (p_y >= r_y) && (p_x <= (r_x + r_width)) && (p_y <= (r_y + r_height)))

#define CONTROL_ENABLED		(1 << 0)
#define CONTROL_TABBABLE	(1 << 1)
#define CONTROL_MOVABLE		(1 << 2)
#define CONTROL_RESIZABLE	(1 << 3)
#define CONTROL_MOVING		(1 << 4)
#define CONTROL_RESIZING	(1 << 5)
#define CONTROL_FOCUSABLE	(1 << 6)
#define CONTROL_FOCUSED		(1 << 7)

//
// Control - Function pointer types.
//
typedef void (*ez_control_handler_fp) (struct ez_control_s *self);
typedef qbool (*ez_control_mouse_handler_fp) (struct ez_control_s *self, mouse_state_t *mouse_state);
typedef qbool (*ez_control_key_handler_fp) (struct ez_control_s *self, int key, qbool down);
typedef qbool (*ez_control_keychange_handler_fp) (struct ez_control_s *self, int key);
typedef void (*ez_control_move_handler_fp) (struct ez_control_s *self, int parent_abs_x, int parent_abs_y);
typedef void (*ez_control_destroy_handler_fp) (struct ez_tree_s *tree, struct ez_control_s *self, qbool destroy_children);

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

typedef struct ez_control_s
{
	char				*name;
	char				*description;
	int	 				x;
	int					y;
	int 				width;
	int					height;

	int					absolute_x;		// The absolute screen coordinates for the HUD element.
	int					absolute_y;	

	int					draw_order;		// The order the control is drawn in.
	int					tab_order;		// The tab number of the control.
	
	int					flags;

	byte				background_frame[4];
	mpic_t				*background;

	mouse_state_t		prev_mouse_state;
	
	ez_control_mouse_handler_fp		on_mouse;
	ez_control_mouse_handler_fp		on_mouse_click;
	ez_control_mouse_handler_fp		on_mouse_up;
	ez_control_mouse_handler_fp		on_mouse_down;
	ez_control_mouse_handler_fp		on_mouse_hover;
	ez_control_mouse_handler_fp		on_mouse_enter;
	ez_control_mouse_handler_fp		on_mouse_leave;

	ez_control_key_handler_fp		on_key;
	ez_control_keychange_handler_fp on_key_up;
	ez_control_keychange_handler_fp on_key_down;

	ez_control_handler_fp			on_draw;

	ez_control_destroy_handler_fp	on_destroy;

	ez_control_move_handler_fp		on_move;

	ez_control_handler_fp			on_got_focus;
	ez_control_handler_fp			on_lost_focus;
	ez_control_handler_fp			on_layout_children;

	struct ez_control_s				*parent;
	ez_double_linked_list_t			children;
} ez_control_t;

typedef struct ez_tree_s
{
	ez_control_t			*root;				// The control tree.
	ez_dllist_node_t		*focused_node;		// The node of focused control (from the tablist). 
	ez_double_linked_list_t	drawlist;			// A list with the controls ordered in their drawing order.
	ez_double_linked_list_t	tablist;			// A list with the controls ordered in their tabbing order.
} ez_tree_t;

//
// Add item to double linked list.
//
void EZ_double_linked_list_Add(ez_double_linked_list_t *list, void *payload);

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
qbool EZ_tree_KeyEvent(ez_tree_t *tree, int key, qbool down);

//
// Tree Control - Finds any orphans and adds them to the root control.
//
void EZ_tree_UnOrphanizeChildren(ez_tree_t *tree);

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
void EZ_control_Destroy(ez_tree_t *tree, ez_control_t *self, qbool destroy_children);

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
void EZ_control_OnGotFocus(ez_control_t *self);

//
// Control - The control lost focus.
//
void EZ_control_OnLostFocus(ez_control_t *self);

//
// Control - The control was moved.
//
void EZ_control_OnMove(ez_control_t *self, int x, int y);

//
// Control - Layouts children.
//
void EZ_control_LayoutChildren(ez_control_t *self);

//
// Control - Draws the control.
//
void EZ_control_OnDraw(ez_control_t *self);

//
// Control - Key event.
//
qbool EZ_control_OnKeyEvent(ez_control_t *self, int key, qbool down);

//
// Control - Key released.
//
qbool EZ_control_OnKeyUp(ez_control_t *self, int key);

//
// Control - Key pressed.
//
qbool EZ_control_OnKeyDown(ez_control_t *self, int key);

//
// Control -
// The initial mouse event is handled by this, and then raises more specialized event handlers
// based on the new mouse state.
//
qbool EZ_control_OnMouseEvent(ez_control_t *self, mouse_state_t *ms);

//
// Control - The mouse was pressed and then released within the bounds of the control.
//
qbool EZ_control_OnMouseClick(ez_control_t *self, mouse_state_t *mouse_state);

//
// Control - The mouse entered the controls bounds.
//
qbool EZ_control_OnMouseEnter(ez_control_t *self, mouse_state_t *mouse_state);

//
// Control - The mouse left the controls bounds.
//
qbool EZ_control_OnMouseLeave(ez_control_t *self, mouse_state_t *mouse_state);

//
// Control - A mouse button was released within the bounds of the control.
//
qbool EZ_control_OnMouseUp(ez_control_t *self, mouse_state_t *mouse_state);

//
// Control - A mouse button was pressed within the bounds of the control.
//
qbool EZ_control_OnMouseDown(ez_control_t *self, mouse_state_t *mouse_state);

//
// Control - The mouse wheel was triggered within the bounds of the control.
//
qbool EZ_control_OnMouseWheel(ez_control_t *self, mouse_state_t *mouse_state);

//
// Control - The mouse is hovering within the bounds of the control.
//
qbool EZ_control_OnMouseHover(ez_control_t *self, mouse_state_t *mouse_state);

#endif // __EZ_CONTROLS_H__

