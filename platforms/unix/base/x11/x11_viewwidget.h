/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef __X11_VIEWWIDGET_H__
#define __X11_VIEWWIDGET_H__

#include "x11_widget.h"


class X11ViewWidget : public X11Widget
{
private:
	class X11OpView *opview;
	bool pending_leave;
	static int last_mouse_x, last_mouse_y;

	void HandleAllExposures(XEvent *event=0);

public:
	X11ViewWidget(X11OpView *opview);
	~X11ViewWidget();

	virtual void UpdateAll(bool now=false);
	virtual bool HandleEvent(XEvent *event);

	void ToViewCoords(int *x, int *y);
	void FromViewCoords(int *x, int *y);

	// Implementation of X11CbObject:
	virtual void HandleCallBack(INTPTR ev);

	static bool IsButtonPressed(int num);
	static void GetLastMouseEventPos(int *x, int *y);
};

#endif // __X11_VIEWWIDGET_H__
