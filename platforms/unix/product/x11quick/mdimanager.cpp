/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#include "core/pch.h"

#ifndef VEGA_OPPAINTER_SUPPORT

#include "mdimanager.h"

#include "platforms/unix/base/x11/x11_colormanager.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_opwindow.h"


MdiManager::MdiManager(X11OpWindow *opwindow)
	: X11WindowWidget(opwindow),
	  active_child(0)
{
}

OP_STATUS MdiManager::Init(X11Widget *parent, const char *name, int flags)
{
	RETURN_IF_ERROR(X11WindowWidget::Init(parent, name, flags));
	XSetWindowBackground(display, handle,
						 g_x11_colormanager->GetPixelValue(128, 128, 128));
	return OpStatus::OK;
}

void MdiManager::Resize(int w, int h)
{
	if (active_child)
		active_child->X11Widget::Resize(w, h);

	X11WindowWidget::Resize(w, h);
}

void MdiManager::DescendantAdded(X11Widget *child)
{
	child->Move(0, 0);
	X11WindowWidget::DescendantAdded(child);
}

void MdiManager::DescendantRemoved(X11Widget *child)
{
	if (child == active_child)
		active_child = 0;
	X11WindowWidget::DescendantRemoved(child);
}

void MdiManager::ChangeActiveChild(MdiWindow *child, bool activate)
{
	if (activate)
	{
		if (active_child)
		{
			if (active_child == child)
				return; // nothing to do then

			active_child->X11Widget::Lower();
			OpWindowListener *listener =
				active_child->GetOpWindow()->GetWindowListener();
			if (listener)
				listener->OnActivate(FALSE);
		}

		int w, h;
		GetSize(&w, &h);
		child->X11Widget::SetGeometry(0, 0, w, h);
		child->HandleResize();
		child->X11Widget::Raise();
		child->GiveFocus();
	}
	else
	{
		child->X11Widget::Lower();
		child->RemoveFocus();
		if (active_child == child)
			active_child = 0;
	}

	OpWindowListener *listener = child->GetOpWindow()->GetWindowListener();
	if (listener)
	{
		listener->OnActivate(activate);
		if (activate)
			active_child = child;
	}
}


/*
  MdiWindow
*/

MdiWindow::MdiWindow(X11OpWindow *opwindow)
	: X11WindowWidget(opwindow)
{
}


OP_STATUS MdiWindow::Init(X11Widget *parent, const char *name, int flags)
{
	RETURN_IF_ERROR(X11WindowWidget::Init(parent, name, flags));
	Activate(true);
	return OpStatus::OK;
}

void MdiWindow::Activate(bool active)
{
	X11OpWindow *parentwin = (X11OpWindow *)opwindow->GetParentWindow();
	OP_ASSERT(parentwin);
	if (parentwin)
	{
		MdiManager *mdimanager = parentwin->GetMdiManager();
		OP_ASSERT(mdimanager);
		if (mdimanager)
			mdimanager->ChangeActiveChild(this, active);
	}
}

void MdiWindow::Resize(int w, int h)
{
}

void MdiWindow::Move(int x, int y)
{
}

void MdiWindow::Raise()
{
	X11Widget::Raise();
	Activate(TRUE);
}

void MdiWindow::Lower()
{
	X11Widget::Lower();
	Activate(FALSE);
}

void MdiWindow::SetWindowState(WindowState newstate)
{
}

#endif // !VEGA_OPPAINTER_SUPPORT
