/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef __MDIMANAGER_H__
#define __MDIMANAGER_H__

#include "platforms/unix/base/x11/x11_windowwidget.h"


class MdiManager : public X11WindowWidget
{
private:
	class MdiWindow *active_child;

public:
	MdiManager(X11OpWindow *opwindow);
	OP_STATUS Init(X11Widget *parent, const char *name=0, int flags=0);

	void Resize(int w, int h);
	void DescendantAdded(X11Widget *child);
	void DescendantRemoved(X11Widget *child);
	void ChangeActiveChild(MdiWindow *child, bool activate);
};


class MdiWindow : public X11WindowWidget
{
public:
	MdiWindow(X11OpWindow *opwindow);
	OP_STATUS Init(X11Widget *parent, const char *name=0, int flags=0);
	void Activate(bool active);
	void Resize(int w, int h);
	void Move(int x, int y);
	void Raise();
	void Lower();
	void SetWindowState(WindowState newstate);
};

#endif // __X11_WINDOWWIDGET_H__
