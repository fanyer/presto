/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef MDE_DSMODE_H
#define MDE_DSMODE_H

#ifdef MDE_DS_MODE

#include "modules/libgogi/mde.h"

class MDE_DSMode;

/** The offscreen buffer screen used by DS mode to render a big window. */
class MDE_DSModeScreen : public MDE_Screen
{
public:
	MDE_DSModeScreen();

	void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
	void OnInvalid();

	void OutOfMemory(){isOOM = TRUE;}
	MDE_FORMAT GetFormat();
	MDE_BUFFER *LockBuffer();
	void UnlockBuffer(MDE_Region *update_region);
	
	BOOL IsOOM(){return isOOM;}
	void SetDSModeInstance(MDE_DSMode* dsm){dsMode = dsm;}
	MDE_DSMode* dsMode;
private:
	MDE_BUFFER screen;
	BOOL isOOM;
};

/** The bird and from view implementeation of ds mode. */
class MDE_DSModeView : public MDE_View
{
public:
	MDE_DSModeView();

	/** Set the DS mode screen this bird/frog view should use. */
	void SetScreen(MDE_DSModeScreen* s){dsScreen = s;}

	void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);

	/** Make this view bird (or from by settings false. */
	void SetBird(BOOL bird){isBird = bird;}
	
#ifdef MDE_SUPPORT_MOUSE
	void OnMouseDown(int x, int y, int button, int clicks);
	void OnMouseUp(int x, int y, int button);
	void OnMouseMove(int x, int y);
	bool OnMouseWheel(int delta);
#endif // MDE_SUPPORT_MOUSE
private:
	BOOL isBird;
	MDE_DSModeScreen* dsScreen;

	int grab_x;
	int grab_y;
	BOOL grabbed;
	OpPoint lastPos;
};

class MDE_DSMode : public MDE_View
{
public:
	MDE_DSMode();
	~MDE_DSMode();
	
	OP_STATUS EnableDS(BOOL en, MDE_View* win);
	OP_STATUS Resize(int w, int h);

	BOOL IsDSEnabled(){return dsEnabled;}
	void SwapViews();
	
	void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
	
	void Repaint();
	
	void InvalidateOperaArea(const MDE_RECT &r);
	void MoveFrogRect(int cx, int cy);

	BOOL GetInvalid();
	unsigned char* overview_buffer;
	int frogOffsetX;
	int frogOffsetY;
private:
	BOOL dsEnabled;
	MDE_DSModeScreen dsScreen;
	MDE_DSModeView dsBirdView;
	MDE_DSModeView dsFrogView;
};

#endif // MDE_DS_MODE

#endif // !MDE_DSMODE_H

