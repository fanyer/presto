/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef X11_UIWINDOWNOTIFIER_H
#define X11_UIWINDOWNOTIFIER_H

class UiWindowNotifier
{
public:
	virtual ~UiWindowNotifier() { }

	virtual void SetSkinElement(const char *elm) = 0;
	virtual void GetMargin(INT32 *left, INT32 *top, INT32 *right,
						   INT32 *bottom) = 0;
};

#endif // X11_UIWINDOWNOTIFIER_H
