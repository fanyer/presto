/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef EXTENSION_CONTEXT_H
#define	EXTENSION_CONTEXT_H

#include "modules/inputmanager/inputcontext.h"

/**
 * The Extension feature-specific UI context.
 *
 * Currently hooked up into Opera's input action system in a somewhat clumsy
 * and definitely not scalable way.  This should improve once DSK-271123 is
 * implemented.
 */
class ExtensionContext : public OpInputContext
{
public:
	virtual const char*	GetInputContextName()
		{ return "Extension Application"; }

	virtual BOOL OnInputAction(OpInputAction* action);
};

#endif // EXTENSION_CONTEXT_H
