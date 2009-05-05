
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
	ez_control_t *listview_ctrl	= (ez_control_t *)listview;

	// Initialize the inherited class first.
	EZ_scrollpane_Init(&listview->super, tree, parent, name, description, x, y, width, height, flags);

	((ez_control_t *)listview)->CLASS_ID		= EZ_LISTVIEW_ID;
	((ez_control_t *)listview)->ext_flags		|= (flags | control_focusable | control_contained | control_resizeable);

}

//
// Listview - Destroys a listview.
//
int EZ_listview_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_listview_t *listview = (ez_listview_t *)self;
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDestroy, destroy_children); 

	// TODO : Remove any event handlers.

	// TODO : Cleanup listview items.

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
								0, 0, ctrl_lstview->width, self->item_height, 0);

	for (i = 0; i < subitem_count; i++)
	{
		EZ_listviewitem_AddColumn(item, sub_items[i], 30); // TODO: Hmm what default width should we use?
	}

	EZ_double_linked_list_Add(&self->items, (void *)item);
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

	if (sci < 0 || sci >= COLUMN_COUNT)
	{
		return 0;
	}

	res = strcmp(lvi1->items[sci]->text, lvi2->items[sci]->text);

	return (lv->sort_ascending) ? res : -res;
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
void EZ_listview_SortByColumn(ez_listview_t *self, int column)
{
	EZ_listview_SortByUserFunc(self, EZ_listview_ColumnCompareFunc);
}

//
// Listview - The text changed in one of the listviews items columns.
//
int EZ_listview_OnItemColumnTextChanged(ez_control_t *self, void *ext_event_info)
{
	ez_listview_changeinfo_t *changeinfo = (ez_listview_changeinfo_t *)ext_event_info;

	return 0;
}



