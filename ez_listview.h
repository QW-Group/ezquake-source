
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

$Id: ez_window.h,v 1.55 2007-10-27 14:51:15 cokeman1982 Exp $
*/

#include "ez_controls.h"
#include "ez_scrollpane.h"

// =========================================================================================
// Listview
// =========================================================================================

typedef struct ez_listview_s
{
	ez_scrollpane_t			super;				// The super class
												// (we inherit a scrollpane instead of a normal control so we get scrolling also)

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

#endif // __EZ_LISTVIEW_H__

