/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef COCOA_FILECHOOSER_H
#define COCOA_FILECHOOSER_H

#include "adjunct/desktop_pi/desktop_file_chooser.h"

class CocoaFileChooser : public DesktopFileChooser
{
public:
	CocoaFileChooser();
	~CocoaFileChooser();
	virtual OP_STATUS Execute(OpWindow* parent, DesktopFileChooserListener* listener, const DesktopFileChooserRequest& request);
	virtual void Cancel();

	static bool HandlingCutCopyPaste();
	static void Cut();
	static void Copy();
	static void Paste();
	static void SelectAll();
};

#endif
