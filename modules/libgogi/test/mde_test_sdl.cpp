/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include "../mde.h"

#define IMAGE_WIDTH	 800
#define IMAGE_HEIGHT 600

void InitTest(MDE_Screen *mdescreen);
void ShutdownTest();
void PulseTest(MDE_Screen *mdescreen);

class SDLMdeScreen : public MDE_Screen
{
public:
	SDLMdeScreen();
	~SDLMdeScreen() {}

	// == Inherit MDE_View ========================================
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);

	void OutOfMemory();
	MDE_FORMAT GetFormat();
	MDE_BUFFER *LockBuffer();
	void UnlockBuffer(MDE_Region *update_region);
	SDL_Surface *sdl_screen;
private:
	MDE_BUFFER screen;
};

SDLMdeScreen::SDLMdeScreen()
{
	Init(IMAGE_WIDTH, IMAGE_HEIGHT);
}

void SDLMdeScreen::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	MDE_SetColor(MDE_RGB(69, 81, 120), screen);
	MDE_DrawRectFill(rect, screen);
}

void SDLMdeScreen::OutOfMemory()
{
	printf("out of memory.\n");
}

MDE_FORMAT SDLMdeScreen::GetFormat()
{
  return MDE_FORMAT_BGRA32;
}

MDE_BUFFER *SDLMdeScreen::LockBuffer()
{
	MDE_InitializeBuffer(IMAGE_WIDTH, IMAGE_HEIGHT, sdl_screen->pitch, MDE_FORMAT_BGRA32, sdl_screen->pixels, NULL, &screen);
	return &screen;
}

#define MAX_RECTS 1000
SDL_Rect sdlrects[MAX_RECTS];
void SDLMdeScreen::UnlockBuffer(MDE_Region *update_region)
{
	int num_rects;
	num_rects = update_region->num_rects;
	if (SDL_MUSTLOCK(sdl_screen))
		SDL_UnlockSurface(sdl_screen);
	if (num_rects > MAX_RECTS)
		num_rects = MAX_RECTS;
	for (int i = 0; i < num_rects; ++i)
	{
		sdlrects[i].x = update_region->rects[i].x;
		sdlrects[i].y = update_region->rects[i].y;
		sdlrects[i].w = update_region->rects[i].w;
		sdlrects[i].h = update_region->rects[i].h;
	}
	SDL_UpdateRects(sdl_screen, num_rects, sdlrects);
	if (SDL_MUSTLOCK(sdl_screen))
		SDL_LockSurface(sdl_screen);
}


int
main (int argc, char *argv[])
{
	SDLMdeScreen mdescreen;
	SDL_Surface *screen;
	
	bool running;
	if (SDL_Init(SDL_INIT_VIDEO) == -1)
	{
		fprintf(stderr, "Unable to initialize SDL: %s.\n", SDL_GetError());
		exit(-1);
	}
    screen = SDL_SetVideoMode(IMAGE_WIDTH, IMAGE_HEIGHT, 32, SDL_SWSURFACE);
    if (screen == NULL)
	{
        fprintf(stderr, "Couldn't set %dx%dx32 video mode: %s\n",
                        IMAGE_WIDTH, IMAGE_HEIGHT, SDL_GetError());
        exit(1);
    }
	// keep the surface locked until we need to update it..
	if (SDL_MUSTLOCK(screen))
		SDL_LockSurface(screen);

	

	mdescreen.sdl_screen = screen;

	InitTest(&mdescreen);

	// poll event and send the events to the gogi api
	running = true;
	while (running)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_MOUSEMOTION:
				mdescreen.TrigMouseMove((int)event.motion.x, (int)event.motion.y);
				break;
			case SDL_MOUSEBUTTONUP:
				// TODO: check event.button.state (pressed/released)
				switch (event.button.button)
				{
				case SDL_BUTTON_LEFT:
					mdescreen.TrigMouseUp((int)event.button.x, (int)event.button.y, 1);
					break;
				case SDL_BUTTON_RIGHT:
					mdescreen.TrigMouseUp((int)event.button.x, (int)event.button.y, 2);
					break;
				case SDL_BUTTON_MIDDLE:
					mdescreen.TrigMouseUp((int)event.button.x, (int)event.button.y, 3);
					break;
				case SDL_BUTTON_WHEELUP:
					mdescreen.TrigMouseWheel((int)event.button.x, (int)event.button.y, -1, true, SHIFTKEY_NONE);
					break;
				case SDL_BUTTON_WHEELDOWN:
					mdescreen.TrigMouseWheel((int)event.button.x, (int)event.button.y, 1, true, SHIFTKEY_NONE);
					break;
				default:
					break;
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				switch (event.button.button)
				{
				case SDL_BUTTON_LEFT:
					mdescreen.TrigMouseDown((int)event.button.x, (int)event.button.y, 1, 1);
					break;
				case SDL_BUTTON_RIGHT:
					mdescreen.TrigMouseDown((int)event.button.x, (int)event.button.y, 2, 1);
					break;
				case SDL_BUTTON_MIDDLE:
					mdescreen.TrigMouseDown((int)event.button.x, (int)event.button.y, 3, 1);
					break;
				default:
					break;
				}
				break;
			case SDL_QUIT:
				running = false;
				break;
			default:
				break;
			}
		}
		PulseTest(&mdescreen);
		mdescreen.Validate(true);
		SDL_Delay(10);
	}

  

	ShutdownTest();
	
	if (SDL_MUSTLOCK(screen))
		SDL_UnlockSurface(screen);
	// shutdown sdl
	SDL_Quit();
	
	return 0;
}

