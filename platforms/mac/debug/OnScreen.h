// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// line 2	// File: OnScreen.h
// line 3	// For debugging purposes in Opera, outputs stuff directly to the screen
// line 4	// Can be torn off easily

#ifndef _ONSCREEN_H_	// {
#define _ONSCREEN_H_

enum
{
	kOnScreenBlack,
	kOnScreenBlue,
	kOnScreenRed,
	kOnScreenMagenta,
	kOnScreenGreen,
	kOnScreenCyan,
	kOnScreenYellow,
	kOnScreenWhite			/*,
	kOnScreenDarkBlack,
	kOnScreenDarkBlue,
	kOnScreenDarkRed,
	kOnScreenDarkMagenta,
	kOnScreenDarkGreen,
	kOnScreenDarkCyan,
	kOnScreenDarkYellow,
	kOnScreenDarkWhite		*/
};


////////////////////////////////////////////////////////////////////////////////////////////
// Prototypes:
////////////////////////////////////////////////////////////////////////////////////////////

void DecOnScreen(int x, int y, int fgcolor, int bkcolor, long number);
void Dec3OnScreen(int x, int y, int fgcolor, int bkcolor, long number);
void Dec5OnScreen(int x, int y, int fgcolor, int bkcolor, long value);
void Dec8OnScreen(int x, int y, int fgcolor, int bkcolor, long value);
void Str8OnScreen(int x, int y, int fgcolor, int bkcolor, char *format, ...);
void PutPixel(int x, int y, int color);
void Put4Pixel(int x, int y, int color);
void LockupMark();

#endif	// } _ONSCREEN_H_
