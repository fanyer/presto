/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_HARDCORE_MH_MESSOBJ_H
#define MODULES_HARDCORE_MH_MESSOBJ_H

typedef INTPTR MH_PARAM_1;
typedef INTPTR MH_PARAM_2;

#include "modules/hardcore/mh/messages.h"

class MessageObject
{
public:
	virtual			~MessageObject() { OP_ASSERT(!m_is_listening); }

	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2) = 0;

#ifdef DEBUG_ENABLE_OPASSERT
	MessageObject() : m_is_listening(FALSE) {}
	BOOL m_is_listening;
#endif // DEBUG_ENABLE_OPASSERT
};

#endif // !MODULES_HARDCORE_MH_MESSOBJ_H
