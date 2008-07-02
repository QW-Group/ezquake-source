
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

$Id: ez_window.c,v 1.78 2007/10/27 14:51:15 cokeman1982 Exp $
*/

#include "quakedef.h"
#include "keys.h"
#include "utils.h"
#include "common_draw.h"
#include "ez_listview.h"

//
// Listview - Creates a new slider and initializes it.
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

	// Initilize the button specific stuff.
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

	EZ_control_Destroy(self, destroy_children);

	// TODO : Remove any event handlers.

	return 0;
}
