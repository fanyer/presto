/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef WINDOWS_GADGET_LIST_ADAPTER_H
#define WINDOWS_GADGET_LIST_ADAPTER_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/pi/PlatformGadgetList.h"


/**
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class WindowsGadgetListAdapter : public PlatformGadgetList
{
public:
	virtual BOOL HasChangedSince(time_t last_update_time) const;

	virtual OP_STATUS GetAll(
			OpAutoVector<GadgetInstallerContext>& gadget_contexts,
			BOOL list_globally_installed_gadgets = FALSE) const;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // WINDOWS_GADGET_LIST_ADAPTER_H
