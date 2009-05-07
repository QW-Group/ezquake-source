
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

$Id: ez_listview.c,v 1.78 2007/10/27 14:51:15 cokeman1982 Exp $
*/

#include "quakedef.h"
#include "keys.h"
#include "utils.h"
#include "common_draw.h"
#include "ez_listview.h"

#ifdef _WIN32
#pragma warning( disable : 4189 )
#endif

//
// Listview - Lays out the control.
//
static void EZ_listview_Layout(ez_listview_t *self)
{
	ez_dllist_node_t *it = (self->sort_ascending ? self->items.head : self->items.tail);
	ez_control_t *lvi = NULL;
	int y = 0;

	// Make sure the header hasn't moved.
	EZ_control_SetPosition((ez_control_t *)self->header, 0, 0);
	y += ((ez_control_t *)self->header)->height + self->row_gap;

	// Position all the listview items according to the sort order.
	while ((self->sort_ascending ? it->next : it->previous))
	{
		lvi = (ez_control_t *)it->payload;
		
		EZ_control_SetPosition(lvi, 0, y);
		y += lvi->height + self->row_gap;

		it = (self->sort_ascending ? it->next : it->previous);
	}
}

//
// Listview - On Mouse Click event handler for when a header label is clicked.
//
static int EZ_listview_OnHeaderMouseClick(ez_control_t *self, void *payload, mouse_state_t *ms)
{
	int column = (int)payload;
	ez_listview_t *listview = (ez_listview_t *)self;

	// Change the sorting.
	listview->sort_ascending = !listview->sort_ascending;
	EZ_listview_SetSortColumn(listview, column);

	// Sort!
	EZ_listview_SortByColumn(listview);

	EZ_listview_Layout(listview);

	return 0;
}

//
// Listview - Creates a new listview and initializes it.
//
ez_listview_t *EZ_listview_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags)
{
	ez_listview_t *listview = NULL;

	// We have to have a tree to add the control to.
	if (!tree)
	{
		return NULL;
	}

	listview = (ez_listview_t *)Q_malloc(sizeof(ez_listview_t));
	memset(listview, 0, sizeof(ez_listview_t));

	EZ_listview_Init(listview, tree, parent, name, description, x, y, width, height, flags);
	
	return listview;
}

//
// Listview - Initializes a listview.
//
void EZ_listview_Init(ez_listview_t *listview, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags)
{
	int i;
	ez_listview_subitem_t subitem;
	ez_control_t *listview_ctrl	= (ez_control_t *)listview;

	// Initialize the inherited class first.
	EZ_scrollpane_Init(&listview->super, tree, parent, name, description, x, y, width, height, flags);

	((ez_control_t *)listview)->CLASS_ID		= EZ_LISTVIEW_ID;
	((ez_control_t *)listview)->ext_flags		|= (flags | control_focusable | control_contained | control_resizeable);

	// Overridden events.
	CONTROL_REGISTER_EVENT(listview_ctrl, EZ_listview_OnResize, OnResize, ez_control_t);

	// Listview specific events.
	CONTROL_REGISTER_EVENT(listview, EZ_listview_OnItemAdded, OnItemAdded, ez_listview_t);
	CONTROL_REGISTER_EVENT(listview, EZ_listview_OnRowGapChanged, OnRowGapChanged, ez_listview_t);
	CONTROL_REGISTER_EVENT(listview, EZ_listview_OnColumnGapChanged, OnColumnGapChanged, ez_listview_t);
	CONTROL_REGISTER_EVENT(listview, EZ_listview_OnColumnWidthChanged, OnColumnWidthChanged, ez_listview_t);
	CONTROL_REGISTER_EVENT(listview, EZ_listview_OnRowHeightChanged, OnRowHeightChanged, ez_listview_t);

	// Create the header listviewitem and add subitems to it and set their text to "" to start with.
	{
		ez_label_t *curlabel = NULL;
		listview->header = EZ_listviewitem_Create(listview_ctrl->control_tree, listview_ctrl, "List view header", "", 
									0, 0, listview_ctrl->width, 8, 0);

		subitem.payload = NULL;
		subitem.text = "";

		// Init the labels in the header list view item.
		for (i = 0; i < LISTVIEW_COLUMN_COUNT; i++)
		{
			EZ_listviewitem_AddColumn(listview->header, subitem, 30);
			curlabel = listview->header->items[i];

			// Pass the column index as payload (so we know what column to sort by when a header is clicked).
			EZ_control_AddOnMouseClick((ez_control_t *)curlabel, (void *)i, EZ_listview_OnHeaderMouseClick);
			
			EZ_label_SetReadOnly(curlabel, true);
			EZ_label_SetAutoSize(curlabel, false);
			EZ_label_SetTextSelectable(curlabel, false);
		}

		// TODO: Add ez_control_t's that will act as resize handles between the column headers.
	}
}

//
// Listview - Cleans up a listview item when it is removed in a range.
//
static void EZ_listview_CleanupRangeItem(void *payload)
{
	if (payload != NULL)
	{
		ez_listviewitem_t *item = (ez_listviewitem_t *)payload;
		EZ_listviewitem_Destroy((ez_control_t *)item, true);
	}
}

//
// Listview - Destroys a listview.
//
int EZ_listview_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_listview_t *listview = (ez_listview_t *)self;
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDestroy, destroy_children); 

	// TODO : Remove any event handlers.

	// Cleanup listview items.
	EZ_double_linked_list_RemoveRange(&listview->items, 0, listview->items.count - 1, EZ_listview_CleanupRangeItem);
	
	EZ_control_Destroy(self, destroy_children);

	return 0;
}

//
// Listview - Adds a listview item to the listview. Expects an array of column items as argument (and a count of how many).
//
void EZ_listview_AddItem(ez_listview_t *self, const ez_listview_subitem_t *sub_items, int subitem_count, void *payload)
{
	int i;
	ez_control_t *ctrl_lstview = (ez_control_t *)self;
	ez_listviewitem_t *item;
	
	item = EZ_listviewitem_Create(ctrl_lstview->control_tree, ctrl_lstview, "List view item", "", 
								0, 0, ctrl_lstview->width, self->row_height, 0);

	for (i = 0; i < subitem_count; i++)
	{
		EZ_listviewitem_AddColumn(item, sub_items[i], 30); // TODO: Hmm what default width should we use?
	}

	EZ_double_linked_list_Add(&self->items, (void *)item);

	CONTROL_RAISE_EVENT(NULL, self, ez_listview_t, OnItemAdded, NULL)
}

//
// Listview - An item was added to the listview.
//
int EZ_listview_OnItemAdded(ez_control_t *self, void *ext_event_info)
{
	ez_listview_t *listview = (ez_listview_t *)self;

	// TODO: Maybe we should allow you to supress layout while adding a bunch of items.
	EZ_listview_Layout(listview);

	CONTROL_EVENT_HANDLER_CALL(NULL, listview, ez_listview_t, OnItemAdded, NULL);

	return 0;
}

//
// Listview - Set the text of the header.
//
void EZ_listview_SetHeaderText(ez_listview_t *self, int column, const char *text)
{
	if (column < 0 || column >= LISTVIEW_COLUMN_COUNT)
	{
		return;
	}

	EZ_label_SetText(self->header->items[column], text);
}

//
// Listview - Removes an item at a specific index.
//
void EZ_listview_RemoveItemByIndex(ez_listview_t *self, int index)
{
	ez_listviewitem_t *item = EZ_double_linked_list_RemoveByIndex(&self->items, index);
	
	if (item != NULL)
	{
		EZ_listviewitem_Destroy((ez_control_t *)item, true);
	}
}

//
// Listview - Removes an item with a specific payload.
//
void EZ_listview_RemoveItemByPayload(ez_listview_t *self, void *payload)
{
	ez_listviewitem_t *item = EZ_double_linked_list_RemoveByPayload(&self->items, payload);
	
	if (item != NULL)
	{
		EZ_listviewitem_Destroy((ez_control_t *)item, true);
	}
}

//
// Listview - Removes a range of items from a listview.
//
void EZ_listview_RemoveRange(ez_listview_t *self, int start, int end)
{
	EZ_double_linked_list_RemoveRange(&self->items, start, end, EZ_listview_CleanupRangeItem);
}

//
// Listview - Compares two columns to each other (based on the sort column set in the listview object they are related with).
//
static int EZ_listview_ColumnCompareFunc(const void *it1, const void *it2)
{
	int sci;
	int res = 0;
	ez_listviewitem_t *lvi1 = GET_LISTVIEW_ITEM(it1);
	ez_listviewitem_t *lvi2 = GET_LISTVIEW_ITEM(it2);
	ez_listview_t *lv = (ez_listview_t *)lvi1->super.parent; // We assume the item has a listview as parent, otherwise something is wrong.

	sci = lv->sort_column_index;

	if (sci < 0 || sci >= LISTVIEW_COLUMN_COUNT)
	{
		return 0;
	}

	res = strcmp(lvi1->items[sci]->text, lvi2->items[sci]->text);

	return res; // (lv->sort_ascending) ? res : -res; // HMM! Nevermind this, since we're using a double linked list to store the listview items we can just layout them from tail -> head instead of doing a sort :)
}

//
// Listview - Sorts the list view by a user supplied function.
//
void EZ_listview_SortByUserFunc(ez_listview_t *self, PtFuncCompare compare_function)
{
	EZ_double_linked_list_Sort(&self->items, EZ_listview_ColumnCompareFunc);
}

//
// Listview - Sorts the listview items by a specified column.
//
void EZ_listview_SortByColumn(ez_listview_t *self)
{
	EZ_listview_SortByUserFunc(self, EZ_listview_ColumnCompareFunc);
}

//
// Listview - Set the column to sort by.
//
void EZ_listview_SetSortColumn(ez_listview_t *self, int column)
{
	self->sort_column_index = column;
}

//
// Listview - The text changed in one of the listviews items columns.
//
int EZ_listview_OnItemColumnTextChanged(ez_control_t *self, void *ext_event_info)
{
	ez_listview_changeinfo_t *changeinfo = (ez_listview_changeinfo_t *)ext_event_info;

	return 0;
}

//
// Listview - OnResize event handler.
//
int EZ_listview_OnResize(ez_control_t *self, void *ext_event_info)
{
	ez_listview_t *listview = (ez_listview_t *)self;

	// Run the super class implementation.
	EZ_scrollpane_OnResize(self, ext_event_info);

	//EZ_listview_Layout(listview);

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnResize, NULL);

	return 0;
}

//
// Listview - Sets the row gap size of the listview.
//
void EZ_listview_SetRowGap(ez_listview_t *self, int gap)
{
	self->row_gap = gap;
	EZ_listview_Layout(self);

	CONTROL_RAISE_EVENT(NULL, self, ez_listview_t, OnRowGapChanged, NULL);
}

//
// Listview - The row gap size has changed.
//
int EZ_listview_OnRowGapChanged(ez_control_t *self, void *ext_event_info)
{
	ez_listview_t *listview = (ez_listview_t *)self;

	EZ_listview_Layout(listview);

	CONTROL_EVENT_HANDLER_CALL(NULL, listview, ez_listview_t, OnRowGapChanged, ext_event_info);

	return 0;
}

//
// Listview - Sets the column gap size of the listview.
//
void EZ_listview_SetColumnGap(ez_listview_t *self, int gap)
{
	self->col_gap = gap;
	CONTROL_RAISE_EVENT(NULL, self, ez_listview_t, OnRowGapChanged, NULL);
}

//
// Listview - The column gap size has changed.
//
int EZ_listview_OnColumnGapChanged(ez_control_t *self, void *ext_event_info)
{
	ez_listview_t *listview = (ez_listview_t *)self;
	ez_dllist_node_t *it = listview->items.head;
	ez_listviewitem_t *lvi = NULL;

	// Update all listview items to know about the new gap size.
	if (listview->items.count > 0)
	{
		while (it->next)
		{
			lvi = (ez_listviewitem_t *)it->payload;

			EZ_listviewitem_SetColumnGap(lvi, listview->col_gap);

			it = it->next;
		}
	}

	EZ_listview_Layout(listview);

	CONTROL_EVENT_HANDLER_CALL(NULL, listview, ez_listview_t, OnColumnGapChanged, ext_event_info);

	return 0;
}

//
// Listview - Set the width of a column.
//
void EZ_listview_SetColumnWidth(ez_listview_t *self, int column, int width)
{
	ez_listview_t *listview = (ez_listview_t *)self;

	if (column < 0 || column >= LISTVIEW_COLUMN_COUNT)
		return;

	self->col_widths[column] = width;

	CONTROL_RAISE_EVENT(NULL, listview, ez_listview_t, OnColumnWidthChanged, (void *)column);
}

//
// Listview - The width of a column has changed.
//
int EZ_listview_OnColumnWidthChanged(ez_control_t *self, void *ext_event_info)
{
	ez_listview_t *listview = (ez_listview_t *)self;
	ez_dllist_node_t *it = listview->items.head;
	ez_listviewitem_t *lvi = NULL;
	int column = (int)ext_event_info;

	if (listview->items.count > 0)
	{
		while (it->next)
		{
			lvi = (ez_listviewitem_t *)it->payload;
			EZ_listviewitem_SetColumnWidth(lvi, column, listview->col_widths[column]);
			it = it->next;
		}
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, listview, ez_listview_t, OnColumnWidthChanged, ext_event_info);
	return 0;
}

//
// Listview - Set the width of a column.
//
void EZ_listview_SetRowHeight(ez_listview_t *self, int row_height)
{
	self->row_height = row_height;

	CONTROL_RAISE_EVENT(NULL, self, ez_listview_t, OnRowHeightChanged, NULL);
}

//
// Listview - The height of the rows has changed.
//
int EZ_listview_OnRowHeightChanged(ez_control_t *self, void *ext_event_info)
{
	ez_listview_t *listview = (ez_listview_t *)self;
	ez_dllist_node_t *it = listview->items.head;
	ez_control_t *lvi = NULL;

	if (listview->items.count > 0)
	{
		while (it->next)
		{
			lvi = (ez_control_t *)it->payload;
			EZ_control_SetSize(lvi, lvi->width, lvi->height);
			it = it->next;
		}
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, listview, ez_listview_t, OnRowHeightChanged, ext_event_info);
	return 0;
}


