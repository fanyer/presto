/*
 *  GadgetApplicationListener.h
 *  Opera
 *
 *  Created by Mateusz Berezecki on 5/13/09.
 *  Copyright 2009 Opera Software. All rights reserved.
 *
 */

#include "core/pch.h"

#ifndef GADGET_APPLICATION_LISTENER
#define GADGET_APPLICATION_LISTENER

#ifdef WIDGET_RUNTIME_SUPPORT

#include "platforms/mac/quick_support/MacApplicationListener.h"

class MacGadgetApplicationListener : public MacApplicationListener
{
	virtual void OnStart();
};

#endif
#endif
