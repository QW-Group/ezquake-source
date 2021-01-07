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

static SDL_Thread *thread;
static SDL_mutex *mouse_mutex;

static SDL_mutex *start_mutex;
static SDL_cond *start_cond;

static IOHIDManagerRef hid_manager;
static CFRunLoopRef runloop;

static int mouse_x;
static int mouse_y;

static void input_callback(void *unused, IOReturn result, void *sender, IOHIDValueRef value)
{
	IOHIDElementRef elem = IOHIDValueGetElement(value);
	uint32_t page = IOHIDElementGetUsagePage(elem);
	uint32_t usage = IOHIDElementGetUsage(elem);
	uint32_t val = IOHIDValueGetIntegerValue(value);

	if (page == kHIDPage_GenericDesktop) {
		switch (usage) {
			case kHIDUsage_GD_X:
				SDL_LockMutex(mouse_mutex);
				mouse_x += val;
				SDL_UnlockMutex(mouse_mutex);
				break;
			case kHIDUsage_GD_Y:
				SDL_LockMutex(mouse_mutex);
				mouse_y += val;
				SDL_UnlockMutex(mouse_mutex);
				break;
			default:
				break;
		}
	}
}

static int OSX_Mouse_Thread(void *inarg)
{
	SDL_LockMutex(start_mutex);

	hid_manager = IOHIDManagerCreate(kCFAllocatorSystemDefault, kIOHIDOptionsTypeNone);

	IOHIDManagerSetDeviceMatching(hid_manager, NULL);
	IOHIDManagerRegisterInputValueCallback(hid_manager, input_callback, NULL);
	IOHIDManagerScheduleWithRunLoop(hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

	// This may fail if the process running does not have 'Input Monitoring' permissions granted.
	if (IOHIDManagerOpen(hid_manager, kIOHIDOptionsTypeNone) != kIOReturnSuccess) {
		IOHIDManagerUnscheduleFromRunLoop(hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

		CFRelease(hid_manager);
		hid_manager = NULL;

		SDL_CondSignal(start_cond);
		SDL_UnlockMutex(start_mutex);

		return -1;
	}

	runloop = CFRunLoopGetCurrent();

	SDL_CondSignal(start_cond);
	SDL_UnlockMutex(start_mutex);

	CFRunLoopRun();

	IOHIDManagerUnscheduleFromRunLoop(hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

	CFRelease(hid_manager);
	hid_manager = NULL;

	return 0;
}

int OSX_Mouse_Init(void)
{
	mouse_x = 0;
	mouse_y = 0;

	start_mutex = SDL_CreateMutex();
	start_cond = SDL_CreateCond();

	SDL_LockMutex(start_mutex);

	mouse_mutex = SDL_CreateMutex();
	thread = SDL_CreateThread(OSX_Mouse_Thread, "OSX_Mouse_Thread", NULL);

	SDL_CondWait(start_cond, start_mutex);
	SDL_UnlockMutex(start_mutex);

	SDL_DestroyMutex(start_mutex);
	start_mutex = NULL;

	SDL_DestroyCond(start_cond);
	start_cond = NULL;

	if (!runloop)
	{
		SDL_WaitThread(thread, NULL);
		thread = NULL;

		SDL_DestroyMutex(mouse_mutex);
		mouse_mutex = NULL;

		return -1;
	}

	return 0;
}

void OSX_Mouse_Shutdown(void)
{
	if (!runloop)
		return;

	CFRunLoopStop(runloop);
	runloop = NULL;

	SDL_WaitThread(thread, NULL);
	thread = NULL;

	SDL_DestroyMutex(mouse_mutex);
	mouse_mutex = NULL;
}

void OSX_Mouse_GetMouseMovement(int *m_x, int *m_y)
{
	SDL_LockMutex(mouse_mutex);

	*m_x = mouse_x;
	*m_y = mouse_y;

	mouse_x = 0;
	mouse_y = 0;

	SDL_UnlockMutex(mouse_mutex);
}
