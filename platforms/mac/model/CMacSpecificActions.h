/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MACSPECIFIC_ACTIONS
#define MACSPECIFIC_ACTIONS

class OpInputAction;
class Window;

class CMacSpecificActions
{
public:
	static BOOL Handle(OpInputAction* aAction, Window* aWindow);
	static BOOL HandlePrint(OpInputAction* aAction);
};

#endif //!MACSPECIFIC_ACTIONS
