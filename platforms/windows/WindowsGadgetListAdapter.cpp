/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "platforms/windows/WindowsGadgetListAdapter.h"
#include "platforms/windows/WindowsGadgetList.h"

BOOL WindowsGadgetListAdapter::HasChangedSince(time_t last_update_time) const
{
	time_t mod_time = -1;
	RETURN_VALUE_IF_ERROR(WindowsGadgetList::GetDefaultInstance()
			.GetModificationTime(mod_time), FALSE);
	OP_ASSERT(-1 != mod_time);
	return difftime(mod_time, last_update_time) > 0;
}


OP_STATUS WindowsGadgetListAdapter::GetAll(
		OpAutoVector<GadgetInstallerContext>& gadget_contexts,
		BOOL list_globally_installed_gadgets) const
{
	return WindowsGadgetList::GetDefaultInstance().Read(gadget_contexts);
}


OP_STATUS PlatformGadgetList::Create(PlatformGadgetList** gadget_list)
{
	OP_ASSERT(NULL != gadget_list);
	if (NULL == gadget_list)
	{
		return OpStatus::ERR;
	}

	*gadget_list = OP_NEW(WindowsGadgetListAdapter, ());
	RETURN_OOM_IF_NULL(*gadget_list);

	return OpStatus::OK;
}

#endif // WIDGET_RUNTIME_SUPPORT
