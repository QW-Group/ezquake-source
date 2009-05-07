
#ifndef __EZ_LISTVIEWITEM_H__
#define __EZ_LISTVIEWITEM_H__
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

$Id: ez_listviewitem.h,v 1.55 2007-10-27 14:51:15 cokeman1982 Exp $
*/

#include "ez_controls.h"
#include "ez_label.h"

// =========================================================================================
// Listview item
// =========================================================================================

#define LISTVIEW_COLUMN_COUNT	32

typedef struct ez_listview_subitem_s
{
	char *text;			// The text that should be used in the listview column. The contents of this pointer will be COPIED not used directly.
	void *payload;		// A pointer to a payload item supplied by the user. Up to the caller to clean this up.
} ez_listview_subitem_t;

//
// Information associated with the change of an item column change.
//
typedef struct ez_listviewitem_changeinfo_s
{
	//ez_listview_subitem_t	*item;		// Contains the info about the new text of the listview column that was changed, and its user associated payload.
	ez_label_t				*item;
	int						column;		// The column of the listview that this item is on.
	void					*payload;	// The user associated payload associated with this listview item (the entire row, not just the column).
} ez_listviewitem_changeinfo_t;

typedef struct ez_listviewitem_eventcount_s
{
	int OnColumnAdded;
	int OnColumnVisibilityChanged;
	int OnColumnGapChanged;
	int OnColumnWidthChanged;
	//int OnSubItemChanged;
} ez_listviewitem_eventcount_t;

typedef struct ez_listviewitem_events_s
{
	ez_event_fp OnColumnAdded;
	ez_event_fp	OnColumnVisibilityChanged;
	ez_event_fp OnColumnGapChanged;
	ez_event_fp OnColumnWidthChanged;
	//ez_event_fp OnSubItemChanged;
} ez_listviewitem_events_t;

typedef struct ez_listviewitem_eventhandlers_s
{
	ez_eventhandler_t *OnColumnAdded;
	ez_eventhandler_t *OnColumnVisibilityChanged;
	ez_eventhandler_t *OnColumnGapChanged;
	ez_eventhandler_t *OnColumnWidthChanged;
	//ez_eventhandler_t *OnSubItemChanged;
} ez_listviewitem_eventhandlers_t;

typedef struct ez_listviewitem_s
{
	ez_control_t			super;							// The super class.
															// (we inherit a scrollpane instead of a normal control so we get scrolling also)

	ez_listviewitem_events_t		events;
	ez_listviewitem_eventhandlers_t	event_handlers;
	ez_listviewitem_eventcount_t	inherit_levels;
	ez_listviewitem_eventcount_t	override_counts;

	ez_label_t				*items[LISTVIEW_COLUMN_COUNT];			// The sub items (what's shown in the columns).
	int						item_count;								// 
	int						item_gap;								// The gap between two controls.
	int						item_widths[LISTVIEW_COLUMN_COUNT];		// The width of the sub items.
	qbool					item_visibile[LISTVIEW_COLUMN_COUNT];	// Which columns are visible?

	int						override_count;							// These are needed so that subclasses can override listview specific events.
	int						inheritance_level;
} ez_listviewitem_t;

//
// Listview item - Creates a new listview item and initializes it.
//
ez_listviewitem_t *EZ_listviewitem_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Listview item - Initializes a listview item.
//
void EZ_listviewitem_Init(ez_listviewitem_t *listviewitem, ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags);

//
// Listview item - Destroys a listview item.
//
int EZ_listviewitem_Destroy(ez_control_t *self, qbool destroy_children);

//
// Listview item - Event for when a new column was added to the listview item.
//
int EZ_listviewitem_OnColumnAdded(ez_control_t *self, void *column);

//
// Listview item - The visibility changed for a column.
//
int EZ_listviewitem_OnColumnVisibilityChanged(ez_control_t *self, void *ext_event_info);

//
// Listview item - Adds a column to the listview item.
//
void EZ_listviewitem_AddColumn(ez_listviewitem_t *self, ez_listview_subitem_t data, int width);

//
// Listview item - Lays out the control.
//
void EZ_listviewitem_LayoutControl(ez_listviewitem_t *self);

//
// Listview item - Gets a column from the listview item.
//
ez_label_t *EZ_listviewitem_GetColumn(ez_listviewitem_t *self, int column);

//
// Listview item - Sets if a column should be visible or not.
//
void EZ_listviewitem_SetColumnVisible(ez_listviewitem_t *self, int column, qbool visible);

//
// Listview item - A sub items text has changed.
//
int EZ_listviewitem_OnSubItemChanged(ez_control_t *self, void *ext_event_info);

//
// Listview item - Event for when the column gap has changed.
//
int EZ_listviewitem_OnColumnGapChanged(ez_control_t *self, void *ext_event_info);

//
// Listview item - Sets the gap between columns.
//
void EZ_listviewitem_SetColumnGap(ez_listviewitem_t *self, int gap);

//
// Listview item - Sets the column width for a given column.
//
void EZ_listviewitem_SetColumnWidth(ez_listviewitem_t *self, int column, int width);

//
// Listview item - The column width has changed for some column.
//
int EZ_listviewitem_OnColumnWidthChanged(ez_control_t *self, void *ext_event_info);

#endif // __EZ_LISTVIEWITEM_H__

