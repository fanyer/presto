/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef COCOA_OPCOLORCHOOSER_H
#define COCOA_OPCOLORCHOOSER_H

#include "adjunct/desktop_pi/DesktopColorChooser.h"

class CocoaOpColorChooser : public OpColorChooser
{
public:
	CocoaOpColorChooser();
	virtual ~CocoaOpColorChooser();
	virtual OP_STATUS Show(COLORREF initial_color, OpColorChooserListener* listener, DesktopWindow* parent=NULL);
private:
	id responder;
};

#endif // COCOA_OPCOLORCHOOSER_H
