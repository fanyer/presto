/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifndef NO_CARBON
#include "platforms/mac/pi/MacOpColorChooser.h"

OP_STATUS OpColorChooser::Create(OpColorChooser** new_object)
{
	*new_object = new MacOpColorChooser;
	if (*new_object == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

OP_STATUS MacOpColorChooser::Show(COLORREF initial_color, OpColorChooserListener* listener, DesktopWindow* parent/*=NULL*/)
{
	Point pt = {0,0};
	unsigned char r,g,b;
	RGBColor oldColor;
	RGBColor newColor;
	r = initial_color & 0x000000FF;
	g = (initial_color & 0x0000FF00) >> 8;
	b = (initial_color & 0x00FF0000) >> 16;
	oldColor.red	= r | (r << 8);
	oldColor.green	= g | (g << 8);
	oldColor.blue	= b | (b << 8);
	if (GetColor(pt, "\p", &oldColor, &newColor))
	{
		r = newColor.red >> 8;
		g = newColor.green >> 8;
		b = newColor.blue >> 8;
		listener->OnColorSelected(r | (g << 8) | (b << 16));
	}
	return OpStatus::OK;
}
#endif // NO_CARBON

