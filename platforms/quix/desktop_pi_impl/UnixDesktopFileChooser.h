/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef UNIX_DESKTOP_FILE_CHOOSER_H
#define UNIX_DESKTOP_FILE_CHOOSER_H

#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "modules/hardcore/mh/messobj.h"
#include "platforms/quix/toolkits/ToolkitFileChooser.h"

class UnixDesktopFileChooser
	: public DesktopFileChooser
	, public ToolkitFileChooserListener
	, public MessageObject
{
public:
	UnixDesktopFileChooser(ToolkitFileChooser* toolkit_chooser);
	virtual ~UnixDesktopFileChooser();

	// From DesktopFileChooser

	virtual OP_STATUS Execute(OpWindow* parent, 
			DesktopFileChooserListener* listener, 
			const DesktopFileChooserRequest& request);

	virtual void Cancel();

	// From ToolkitFileChooserListener
	virtual bool OnSaveAsConfirm(ToolkitFileChooser* chooser);
	virtual void OnChoosingDone(ToolkitFileChooser* chooser);

	// From MessageObject
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	ToolkitFileChooser* m_chooser;
	DesktopFileChooserListener* m_listener;
};

#endif // UNIX_DESKTOP_FILE_CHOOSER_H
