/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef X11_SPECIFIC_ACTIONS_H
#define X11_SPECIFIC_ACTIONS_H

class X11SpecificActions
{
public:
	static bool Handle(class OpInputAction *action, class Window *win);
};

#endif // X11_SPECIFIC_ACTIONS_H
