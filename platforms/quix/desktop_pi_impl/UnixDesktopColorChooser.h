/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patryk Obara (pobara)
 */

#ifndef UNIX_DESKTOP_COLOR_CHOOSER_H
#define UNIX_DESKTOP_COLOR_CHOOSER_H

#include "adjunct/desktop_pi/DesktopColorChooser.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

class ToolkitColorChooser;

class UnixDesktopColorChooser
	: public OpColorChooser
{
public:
	UnixDesktopColorChooser(ToolkitColorChooser *toolkit_chooser);

	virtual ~UnixDesktopColorChooser();

	OP_STATUS Show (COLORREF initial_color,
					OpColorChooserListener *listener,
					DesktopWindow *parent=NULL);

private:
	ToolkitColorChooser *m_chooser;

	static BOOL m_color_chooser_dialog_is_active;
};

#endif // UNIX_DESKTOP_COLOR_CHOOSER_H
