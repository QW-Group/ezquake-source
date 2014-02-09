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

#include <SDL.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hidsystem/event_status_driver.h>

struct osx_mouse_data
{
	SDL_Thread *thread;
	SDL_mutex *mouse_mutex;

	CFRunLoopRef threadrunloop;

	IOHIDManagerRef hid_manager;

	int mouse_x;
	int mouse_y;
};

struct osx_mouse_data *mdata;

static void input_callback(void *unused, IOReturn result, void *sender, IOHIDValueRef value)
{
	IOHIDElementRef elem = IOHIDValueGetElement(value);
	uint32_t page = IOHIDElementGetUsagePage(elem);
	uint32_t usage = IOHIDElementGetUsage(elem);
	uint32_t val = IOHIDValueGetIntegerValue(value);

	if (page == kHIDPage_GenericDesktop) {
		switch (usage) {
			case kHIDUsage_GD_X:
				SDL_LockMutex(mdata->mouse_mutex);
				mdata->mouse_x += val;
				SDL_UnlockMutex(mdata->mouse_mutex);
				break;
			case kHIDUsage_GD_Y:
				SDL_LockMutex(mdata->mouse_mutex);
				mdata->mouse_y += val;
				SDL_UnlockMutex(mdata->mouse_mutex);
				break;
			default:
				break;
		}
	}
}

static int OSX_Mouse_Thread(void *inarg)
{
	IOReturn tIOReturn;

	mdata->threadrunloop = CFRunLoopGetCurrent();

	mdata->hid_manager = IOHIDManagerCreate(kCFAllocatorSystemDefault, kIOHIDOptionsTypeNone);
	if (mdata->hid_manager)
	{
		IOHIDManagerSetDeviceMatching(mdata->hid_manager, NULL);
		IOHIDManagerRegisterInputValueCallback(mdata->hid_manager, input_callback, NULL);
		IOHIDManagerScheduleWithRunLoop(mdata->hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

		tIOReturn = IOHIDManagerOpen(mdata->hid_manager, kIOHIDOptionsTypeNone);
		if (tIOReturn == kIOReturnSuccess)
			CFRunLoopRun();

		IOHIDManagerUnscheduleFromRunLoop(mdata->hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

	}

	CFRelease(mdata->hid_manager);

	return 0;
}

int OSX_Mouse_Init(void)
{
	if (!mdata)
		mdata = malloc(sizeof(struct osx_mouse_data));

	if (!mdata)
		return -1;

	memset(mdata, 0, sizeof(struct osx_mouse_data));

	if ((mdata->mouse_mutex = SDL_CreateMutex()) != NULL) {
		if ((mdata->thread = SDL_CreateThread(OSX_Mouse_Thread, "OSX_Mouse_Thread", (void*)mdata)) != NULL)
			return 0;

		SDL_DestroyMutex(mdata->mouse_mutex);
	}

	free(mdata);
	mdata = NULL;

	return -1;
}

void OSX_Mouse_Shutdown(void)
{
	if (!mdata)
		return;

#warning Race conditions'r'us
	CFRunLoopStop(mdata->threadrunloop);

	SDL_WaitThread(mdata->thread, NULL);

	SDL_DestroyMutex(mdata->mouse_mutex);

	free(mdata);
	mdata = NULL;
}

void OSX_Mouse_GetMouseMovement(int *mouse_x, int *mouse_y)
{
	if (!mdata)
		return;

	SDL_LockMutex(mdata->mouse_mutex);

	*mouse_x = mdata->mouse_x;
	*mouse_y = mdata->mouse_y;

	mdata->mouse_x = 0;
	mdata->mouse_y = 0;

	SDL_UnlockMutex(mdata->mouse_mutex);
}

