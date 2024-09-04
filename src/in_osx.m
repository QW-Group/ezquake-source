/*
Copyright (C) 2011 Florian Zwoch
Copyright (C) 2011 Mark Olsen

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
*/

#import <SDL.h>
#import <GameController/GameController.h>
#import "common.h"

static id mouseAddedObserver = nil;
static id mouseRemovedObserver = nil;
static SDL_mutex *mouse_mutex = NULL;
static float mouse_x = 0;
static float mouse_y = 0;

static void OSX_RegisterMouseMovedHandler(GCMouse *mouse)
{
	mouse.mouseInput.mouseMovedHandler = ^(GCMouseInput *input, float deltaX, float deltaY) {
		SDL_LockMutex(mouse_mutex);
		mouse_x += deltaX;
		mouse_y += deltaY;
		SDL_UnlockMutex(mouse_mutex);
	};
}

static void OSX_CreateMouseObserver(void)
{
	mouseAddedObserver = [[NSNotificationCenter defaultCenter]
					 addObserverForName:GCMouseDidConnectNotification
								 object:nil
								  queue:nil
							 usingBlock:^(NSNotification *note) {
								 OSX_RegisterMouseMovedHandler(note.object);
							 }
	];
	mouseRemovedObserver = [[NSNotificationCenter defaultCenter]
			addObserverForName:GCMouseDidDisconnectNotification
						object:nil
						 queue:nil
					usingBlock:^(NSNotification *note) {
						GCMouse *mouse = note.object;
						mouse.mouseInput.mouseMovedHandler = nil;
					}
	];
}

int OSX_Mouse_Init(void)
{
	mouse_x = 0;
	mouse_y = 0;

	if (mouse_mutex != NULL) {
		return 0;
	}

	mouse_mutex = SDL_CreateMutex();
	OSX_CreateMouseObserver();

	return 0;
}

void OSX_Mouse_Shutdown(void)
{
	if (mouse_mutex == NULL) {
		return;
	}

	[[NSNotificationCenter defaultCenter] removeObserver:mouseAddedObserver];
	mouseAddedObserver = nil;
	[[NSNotificationCenter defaultCenter] removeObserver:mouseRemovedObserver];
	mouseRemovedObserver = nil;

	SDL_DestroyMutex(mouse_mutex);
	mouse_mutex = NULL;
}

void OSX_Mouse_GetMouseMovement(int *m_x, int *m_y)
{
	SDL_LockMutex(mouse_mutex);

	*m_x = (int)mouse_x;
	*m_y = (int)mouse_y;

	mouse_x = 0;
	mouse_y = 0;

	SDL_UnlockMutex(mouse_mutex);
}
