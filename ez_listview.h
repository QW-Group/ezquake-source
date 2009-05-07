
#ifndef __EZ_LISTVIEW_H__
#define __EZ_LISTVIEW_H__
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

$Id: ez_listviewitem.h,v 1.55 2007-10-27 14:51:15 cokeman1982 Exp $
*/

#include "ez_controls.h"
#include "ez_scrollpane.h"
#include "ez_listviewitem.h"

// =========================================================================================
// Listview
// =========================================================================================

//
// Information associated with the change of an item column change.
//
typedef struct ez_listview_changeinfo_s
{
	ez_listview_subitem_t *item;		// Contains the info about the new text of the listview column that was changed, and its user associated payload.
	int row;							// The row of the listview that this item is on.
	int column;							// The column of the listview that this item is on.
	void *payload;						// The user associated payload associated with this listview item (the entire row, not just the column).
} ez_listview_changeinfo_t;

typedef struct ez_listview_eventcount_s
{
	int OnItemAdded;
	int OnRowGapChanged;
	int OnColumnGapChanged;
	int OnColumnWidthChanged;
	int OnRowHeightChanged;
} ez_listview_eventcount_t;

typedef struct ez_listview_events_s
{
	ez_event_fp OnItemAdded;			// When an item is added to the listview.
	ez_event_fp OnRowGapChanged;		// The row gap has changed.
	ez_event_fp OnColumnGapChanged;		// The column gap has changed.
	ez_event_fp OnColumnWidthChanged;	// The width of a column has changed.
	ez_event_fp OnRowHeightChanged;		// The height of the rows has been changed.
} ez_listview_events_t;

typedef struct ez_listview_eventhandlers_s
{
	ez_eventhandler_t	*OnItemAdded;
	ez_eventhandler_t	*OnRowGapChanged;
	ez_eventhandler_t	*OnColumnGapChanged;
	ez_eventhandler_t	*OnColumnWidthChanged;
	ez_eventhandler_t	*OnRowHeightChanged;
} ez_listview_eventhandlers_t;

typedef struct ez_listview_s
{
	ez_scrollpane_t			super;				// The super class
												// (we inherit a scrollpane instead of a normal control so we get scrolling also)

	ez_listview_events_t	events;				// Specific events for the listview control.
	ez_listview_eventhandlers_t event_handlers;	// Specific event handlers for the listview control.
	ez_listview_eventcount_t inherit_levels;
	ez_listview_eventcount_t override_counts;

	ez_listviewitem_t		*header;			// The header row control of the listview.
	ez_double_linked_list_t	items;				// The listview items.
	int						row_height;			// The height of the listview rows.
	int						col_widths[LISTVIEW_COLUMN_COUNT];	// The widths of the  items.
	int						col_gap;			// The gap to use between columns.
	int						row_gap;			// The gap to use between rows.

	int						sort_column_index;	// The index of the column that we should sort by.
	qbool					sort_ascending;		// Sort ascending or descending when sorting by column?

	int						override_count;		// These are needed so that subclasses can override listview specific events.
	int						inheritance_level;
} ez_listview_t;

//
// Listview - Creates a new listview and initializes it.
//
ez_listview_t *EZ_listview_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Listview - Initializes a listview.
//
void EZ_listview_Init(ez_listview_t *listview, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Listview - Destroys a listview.
//
int EZ_listview_Destroy(ez_control_t *self, qbool destroy_children);

//
// Listview - Adds a listview item to the listview. Expects an array of column items as argument (and a count of how many).
//
void EZ_listview_AddItem(ez_listview_t *self, const ez_listview_subitem_t *sub_items, int subitem_count, void *payload);

//
// Listview - Removes an item at a specific index.
//
void EZ_listview_RemoveItemByIndex(ez_listview_t *self, int index);

//
// Listview - Removes an item with a specific payload.
//
void EZ_listview_RemoveItemByPayload(ez_listview_t *self, void *payload);

//
// Listview - Removes a range of items from a listview.
//
void EZ_listview_RemoveRange(ez_listview_t *self, int start, int end);

// 
// Convenience macro for getting the listview items from a void pointer in comparison functions.
//
#define GET_LISTVIEW_ITEM(lnode) ((ez_listviewitem_t *)( (ez_dllist_node_t *)(lnode))->payload)

//
// Listview - Sorts the list view by a user supplied function. (The listview items will have the listview as a parent).
//
void EZ_listview_SortByUserFunc(ez_listview_t *self, PtFuncCompare compare_function);

//
// Listview - Sorts the listview items by a specified column.
//
void EZ_listview_SortByColumn(ez_listview_t *self);

//
// Listview - Set the column to sort by.
//
void EZ_listview_SetSortColumn(ez_listview_t *self, int column);

//
// Listview - The text changed in one of the listviews items columns.
//
int EZ_listview_OnItemColumnTextChanged(ez_control_t *self, void *ext_event_info);

//
// Listview - Set the text of the header.
//
void EZ_listview_SetHeaderText(ez_listview_t *self, int column, const char *text);

//
// Listview - On Resize event handler.
//
int EZ_listview_OnResize(ez_control_t *self, void *ext_event_info);

//
// Listview - An item was added to the listview.
//
int EZ_listview_OnItemAdded(ez_control_t *self, void *ext_event_info);

//
// Listview - Sets the column gap size of the listview.
//
void EZ_listview_SetColumnGap(ez_listview_t *self, int gap);

//
// Listview - Sets the row gap size of the listview.
//
void EZ_listview_SetRowGap(ez_listview_t *self, int gap);

//
// Listview - The column gap size has changed.
//
int EZ_listview_OnColumnGapChanged(ez_control_t *self, void *ext_event_info);

//
// Listview - The row gap size has changed.
//
int EZ_listview_OnRowGapChanged(ez_control_t *self, void *ext_event_info);

//
// Listview - Set the width of a column.
//
void EZ_listview_SetColumnWidth(ez_listview_t *self, int column, int width);

//
// Listview - The width of a column has changed.
//
int EZ_listview_OnColumnWidthChanged(ez_control_t *self, void *ext_event_info);

//
// Listview - Set the width of a column.
//
void EZ_listview_SetRowHeight(ez_listview_t *self, int row_height);

//
// Listview - The height of the rows has changed.
//
int EZ_listview_OnRowHeightChanged(ez_control_t *self, void *ext_event_info);

#endif // __EZ_LISTVIEW_H__

