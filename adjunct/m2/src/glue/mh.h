/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

// ----------------------------------------------------

#ifndef MH_H
#define MH_H

// ----------------------------------------------------

#include "adjunct/m2/src/include/defs.h"

class MessageLoop 
{
public:
    virtual ~MessageLoop() {}
	class Target
	{
	public:
        virtual ~Target() {}

		virtual OP_STATUS Receive(OpMessage message) = 0;

	};

	virtual OP_STATUS SetTarget(Target* target) = 0;

	virtual OP_STATUS Post(OpMessage message, time_t delay = 0) = 0;

	virtual OP_STATUS Send(OpMessage message) = 0;

	virtual void Release() = 0;

};

// ----------------------------------------------------

#endif // MH_H

// ----------------------------------------------------
