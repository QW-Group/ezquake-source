
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

}

//
// Listview - Removes an item with a specific payload.
//
void EZ_listview_RemoveItemByPayload(ez_listview_t *self, void *payload)
{

}

//
// Listview - Sorts the listview items by a specified column.
//
void EZ_listview_SortByColumn(ez_listview_t *self, int column)
{

}

//
// Listview - Sorts the list view by a user supplied function.
//
void EZ_listview_SortByUserFunc(ez_listview_t *self, PtFuncCompare compare_function)
{
}

//
// Listview - The text changed in one of the listviews items columns.
//
int EZ_listview_OnItemColumnTextChanged(ez_control_t *self, void *ext_event_info)
{
	ez_listview_changeinfo_t *changeinfo = (ez_listview_changeinfo_t *)ext_event_info;

	return 0;
}



