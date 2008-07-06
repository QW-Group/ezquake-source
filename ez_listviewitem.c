
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

$Id: ez_listviewitem.c,v 1.78 2007/10/27 14:51:15 cokeman1982 Exp $
*/

#include "quakedef.h"
#include "keys.h"
#include "utils.h"
#include "common_draw.h"
#include "ez_listviewitem.h"

//
// Listview item - Creates a new listview item and initializes it.
//
ez_listviewitem_t *EZ_listviewitem_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags)
{
	ez_listviewitem_t *listviewitem = NULL;

	// We have to have a tree to add the control to.
	if (!tree)
	{
		return NULL;
	}

	listviewitem = (ez_listviewitem_t *)Q_malloc(sizeof(ez_listviewitem_t));
	memset(listviewitem, 0, sizeof(ez_listviewitem_t));

	EZ_listviewitem_Init(listviewitem, tree, parent, name, description, x, y, width, height, flags);
	
	return listviewitem;
}

//
// Listview item - Initializes a listview item.
//
void EZ_listviewitem_Init(ez_listviewitem_t *listviewitem, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags)
{
	ez_control_t *listviewitem_ctrl	= (ez_control_t *)listviewitem;

	// Initialize the inherited class first.
	EZ_control_Init(&listviewitem->super, tree, parent, name, description, x, y, width, height, flags);

	((ez_control_t *)listviewitem)->CLASS_ID		= EZ_LISTVIEWITEM_ID;
	((ez_control_t *)listviewitem)->ext_flags		|= (flags | control_focusable | control_contained | control_resizeable);

}

//
// Listview item - Destroys a listview item.
//
int EZ_listviewitem_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_listviewitem_t *listviewitem = (ez_listviewitem_t *)self;
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDestroy, destroy_children); 

	// TODO : Remove any event handlers.

	// TODO : Cleanup columns.

	EZ_control_Destroy(self, destroy_children);

	return 0;
}

//
// Listview item - Adds a column to the listview item.
//
void EZ_listviewitem_AddColumn(ez_listviewitem_t *self, ez_listview_subitem_t data)
{
}

//
// Listview item - Sets the specified column index to use the supplied data (which will be copied).
//
void EZ_listviewitem_SetColumn(ez_listviewitem_t *self, ez_listview_subitem_t data, int column)
{
}

//
// Listview item - Sets if a column should be visible or not.
//
void EZ_listviewitem_SetColumnVisible(ez_listviewitem_t *self, int column, qbool visible)
{
}

//
// Listview item - A sub items text has changed.
//
int EZ_listviewitem_OnSubItemChanged(ez_control_t *self, void *ext_event_info)
{
	ez_listviewitem_changeinfo_t *change = (ez_listviewitem_changeinfo_t *)ext_event_info;

	return 0;
}


