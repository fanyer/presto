/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#ifndef X11_DIALOG_H
#define X11_DIALOG_H

#include "platforms/unix/base/x11/x11_windowwidget.h"


class X11Dialog : public X11WindowWidget
{
private:
	bool m_modal;

public:
	X11Dialog(X11OpWindow *opwindow, bool modal)
		: X11WindowWidget(opwindow), m_modal(modal) {}

	OP_STATUS Init(X11Widget *parent, const char *name, int flags);

	void HandleCallBack(INTPTR ev);
	
	// X11Widget
	bool IsDialog() const { return true; }

	/** This method does the necessary preparations for showing a
	 * dialog.
	 *
	 * Any dialog that inherits from X11Dialog will perform these
	 * operations during X11Dialog::Init().  Any dialog that is not an
	 * X11Dialog will need to explicitly call this function before
	 * being displayed.
	 *
	 * This method will perform some clean-up actions to eliminate
	 * state that could interfere with proper running of the dialog,
	 * such as closing pop-up windows and releasing grabs.
	 *
	 * @param stack_modal Set this to true if the dialog will run to
	 * completion before returning from the function that invoked it.
	 * When stack_modal is set to true, this method will clean up
	 * state that is usually cleaned up during the return to the
	 * message loop.  For example, a mouse click event will usually
	 * capture the mouse until the mouse release event has completed.
	 * When stack_modal is true, this method will release the mouse.
	 */
	static void PrepareForShowingDialog(bool stack_modal);
};

#endif // X11_DIALOG_H
