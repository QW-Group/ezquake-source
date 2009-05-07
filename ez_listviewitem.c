
/*
Copyright (C) 2008 ezQuake team

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

#define VALID_COLUMN(column) ((column >= 0) && (column < self->item_count))

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
	ez_control_t *self	= (ez_control_t *)listviewitem;

	// Initialize the inherited class first.
	EZ_control_Init(&listviewitem->super, tree, parent, name, description, x, y, width, height, flags);

	((ez_control_t *)listviewitem)->CLASS_ID		= EZ_LISTVIEWITEM_ID;
	((ez_control_t *)listviewitem)->ext_flags		|= (flags | control_focusable | control_contained | control_resizeable);

	// Make all columns visible by default :)
	memset(self, true, sizeof(qbool) * sizeof(listviewitem->item_visibile));

	// Listview item specific events.
	CONTROL_REGISTER_EVENT(listviewitem, EZ_listviewitem_OnColumnAdded, OnColumnAdded, ez_listviewitem_t);
	CONTROL_REGISTER_EVENT(listviewitem, EZ_listviewitem_OnColumnVisibilityChanged, OnColumnVisibilityChanged, ez_listviewitem_t);
	CONTROL_REGISTER_EVENT(listviewitem, EZ_listviewitem_OnColumnGapChanged, OnColumnGapChanged, ez_listviewitem_t);
	CONTROL_REGISTER_EVENT(listviewitem, EZ_listviewitem_OnColumnWidthChanged, OnColumnWidthChanged, ez_listviewitem_t);
	
	//CONTROL_REGISTER_EVENT(listviewitem, EZ_listviewitem_OnSubItemChanged, OnSubItemChanged, ez_listviewitem_t);
}

//
// Listview item - Destroys a listview item.
//
int EZ_listviewitem_Destroy(ez_control_t *self, qbool destroy_children)
{
	int i;
	ez_listviewitem_t *listviewitem = (ez_listviewitem_t *)self;
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDestroy, destroy_children); 

	// TODO : Remove any event handlers.
	EZ_eventhandler_Remove(listviewitem->event_handlers.OnColumnAdded, NULL, true);
	EZ_eventhandler_Remove(listviewitem->event_handlers.OnColumnVisibilityChanged, NULL, true);
	//EZ_eventhandler_Remove(listviewitem->event_handlers.OnSubItemChanged, NULL, true);

	// Cleanup columns.
	for (i = 0; i < listviewitem->item_count; i++)
	{
		// TODO: Hmm should we raise it like this?
		CONTROL_RAISE_EVENT(NULL, listviewitem->items[i], ez_control_t, OnDestroy, true);
	}

	EZ_control_Destroy((ez_control_t *)self, destroy_children);

	return 0;
}

//
// Listview item - Adds a column to the listview item.
//
void EZ_listviewitem_AddColumn(ez_listviewitem_t *self, ez_listview_subitem_t data, int width)
{
	int column = 0;
	ez_label_t *label = NULL;
	ez_control_t *ctrl = (ez_control_t *)self;

	if (self->item_count >= sizeof(self->items))
	{
		Sys_Error("EZ_listviewitem_AddColumn: Max allowed columns %i exceeded.\n", sizeof(self->items));
	}

	// Create a new label to use as the column.
	label = EZ_label_Create(ctrl->control_tree, ctrl, "Listview item column", "", 0, 0, width, 5, 0, 0, data.text);
	EZ_control_SetPayload(ctrl, data.payload);
	EZ_control_SetAnchor(ctrl, anchor_left | anchor_top | anchor_bottom);

	// Add the item to the columns.
	self->items[self->item_count] = label;
	column = self->item_count;
	self->item_count++;

	CONTROL_RAISE_EVENT(NULL, ctrl, ez_listviewitem_t, OnColumnAdded, &column);
}

//
// Listview item - Event for when a new column was added to the listview item.
//
int EZ_listviewitem_OnColumnAdded(ez_control_t *self, void *column)
{
	ez_listviewitem_t *lvi = (ez_listviewitem_t *)self;
	EZ_listviewitem_LayoutControl(lvi);

	CONTROL_EVENT_HANDLER_CALL(NULL, lvi, ez_listviewitem_t, OnColumnAdded, column);
	// TODO: Layout the control again if some mischevaous event handler changed the size/position of the columns? :D

	return 0;
}

//
// Listview item - Lays out the control.
//
void EZ_listviewitem_LayoutControl(ez_listviewitem_t *self)
{
	int i;
	int x = 0;
	ez_control_t *ctrl		= (ez_control_t *)self;
	ez_control_t *lblctrl	= NULL;

	for (i = 0; i < self->item_count; i++)
	{
		if (!self->item_visibile[i])
			continue;

		lblctrl = (ez_control_t *)self->items[i];

		// Space out the labels in columns.
		EZ_control_SetPosition(lblctrl, x, 0);
		x += lblctrl->width + self->item_gap;

		// Make sure we reset the height if some naughty person changed it since last time.
		EZ_control_SetSize(lblctrl, lblctrl->width, ctrl->height);
	}
}

//
// Listview item - Gets a column from the listview item.
//
ez_label_t *EZ_listviewitem_GetColumn(ez_listviewitem_t *self, int column)
{
	return VALID_COLUMN(column) ? self->items[column] : NULL;
}

//
// Listview item - Sets if a column should be visible or not.
//
void EZ_listviewitem_SetColumnVisible(ez_listviewitem_t *self, int column, qbool visible)
{
	if (!VALID_COLUMN(column))
		return;

	self->item_visibile[column] = visible;
	EZ_control_SetVisible((ez_control_t *)self->items[column], visible);

	CONTROL_RAISE_EVENT(NULL, self, ez_listviewitem_t, OnColumnVisibilityChanged, (void *)column);
}

//
// Listview item - The visibility changed for a column.
//
int EZ_listviewitem_OnColumnVisibilityChanged(ez_control_t *self, void *column)
{
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_listviewitem_t, OnColumnVisibilityChanged, column);
	return 0;
}

//
// Listview item - Sets the gap between columns.
//
void EZ_listviewitem_SetColumnGap(ez_listviewitem_t *self, int gap)
{
	self->item_gap = gap;

	CONTROL_RAISE_EVENT(NULL, self, ez_listviewitem_t, OnColumnGapChanged, NULL);
}

//
// Listview item - Event for when the column gap has changed.
//
int EZ_listviewitem_OnColumnGapChanged(ez_control_t *self, void *ext_event_info)
{
	ez_listviewitem_t *lvi = (ez_listviewitem_t *)self;
	EZ_listviewitem_LayoutControl(lvi);
	
	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_listviewitem_t, OnColumnGapChanged, NULL);
	return 0;
}

//
// Listview item - Sets the column width for a given column.
//
void EZ_listviewitem_SetColumnWidth(ez_listviewitem_t *self, int column, int width)
{
	if (!VALID_COLUMN(column))
		return;

	self->item_widths[column] = width;

	CONTROL_RAISE_EVENT(NULL, self, ez_listviewitem_t, OnColumnWidthChanged, (void *)column);
}

//
// Listview item - The column width has changed for some column.
//
int EZ_listviewitem_OnColumnWidthChanged(ez_control_t *self, void *ext_event_info)
{
	ez_listviewitem_t *lvi = (ez_listviewitem_t *)self;
	int column = (int)ext_event_info;

	EZ_control_SetSize(self, lvi->item_widths[column], self->height);

	CONTROL_EVENT_HANDLER_CALL(NULL, lvi, ez_listviewitem_t, OnColumnWidthChanged, ext_event_info);
	return 0;
}

#if 0
//
// Listview item - A sub items text has changed.
//
int EZ_listviewitem_OnSubItemChanged(ez_control_t *self, void *ext_event_info)
{
	ez_listviewitem_changeinfo_t *change = (ez_listviewitem_changeinfo_t *)ext_event_info;

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_listviewitem_t, OnSubItemChanged, ext_event_info);
	return 0;
}


//
// Listview item - The text of a label in a sub item changed (event handler).
//
static int EZ_listviewitem_OnLabelTextChanged(ez_control_t *self, void *payload, void *ext_event_info)
{
	ez_label_t *label = (ez_label_t *)self;
	ez_listviewitem_changeinfo_t change;
	ez_listview_subitem_t item;

	item.payload	= self->payload;
	//item.text		= label->
	
	change.column	= (int)*self->payload; // TODO: Save the column in the label controls payload.
	change.payload	= self;
	

	CONTROL_RAISE_EVENT(NULL, self, ez_listviewitem_t, OnSubItemChanged, (void *)&change);
}
#endif

