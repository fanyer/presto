/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef UNIXGADGETLIST_H
#define UNIXGADGETLIST_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/pi/PlatformGadgetList.h"


/**
 * @author Patryk Obara
 */
class UnixGadgetList : public PlatformGadgetList
{
public:

	virtual BOOL HasChangedSince(time_t last_update_time) const;

	virtual OP_STATUS GetAll(
			OpAutoVector<GadgetInstallerContext>& gadget_contexts,
			BOOL list_globally_installed_gadgets = FALSE) const;
private:

	OP_STATUS CreateGadgetContext(const OpStringC& gadget_path,
		OpAutoPtr<GadgetInstallerContext>& context) const;

	OP_STATUS GetStatusSuffix(const OpStringC& profile_name, OpString& suffix) const;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // UNIXGADGETLIST_H
