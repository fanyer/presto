/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef INSTALLED_GADGET_LIST_H
#define INSTALLED_GADGET_LIST_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/pi/PlatformGadgetList.h"


/**
 * An efficient and always up-to-date view of the list of gadgets installed in
 * the OS.
 *
 * This is a self-updating cache-like wrapper for PlatformGadgetList.
 *
 * The most glaring difference between using this class and using
 * PlatformGadgetList directly is that, in the former case, one relinquishes
 * control over when exactly the potentially heavy PlatformGadgetList::GetAll()
 * method is called.
 *
 * @see PlatformGadgetList
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class InstalledGadgetList
{
public:
	InstalledGadgetList();

	/**
	 * Initializes the list using an internal PlatformGadgetList instance.
	 *
	 * @return status
	 */
	OP_STATUS Init();

	/**
	 * Initializes the list using an external PlatformGadgetList instance.
	 * Probably only useful for testing.
	 *
	 * @param platform_list an external instance, but ownership is transferred
	 * @return status
	 */
	OP_STATUS Init(PlatformGadgetList* platform_list);

	/**
	 * Determines if a gadget identified by a display name is installed in the
	 * OS.
	 *
	 * @param gadget_display_name the gadget display name
	 * @return @c TRUE iff a gadget identified by @a gadget_display_name is
	 * 		installed in the OS
	 */
	BOOL Contains(const OpStringC& gadget_display_name) const;

	unsigned GetCount() const { return m_cache.Get().GetCount(); }

private:
	InstalledGadgetList(const InstalledGadgetList&);
	InstalledGadgetList& operator=(const InstalledGadgetList&);

	class Cache
	{
	public:
		Cache();
		OP_STATUS Init(PlatformGadgetList* platform_list);
		OpAutoVector<GadgetInstallerContext>& Get();
	private:
		OpAutoPtr<PlatformGadgetList> m_platform_list;
		time_t m_timestamp;
		OpAutoVector<GadgetInstallerContext> m_gadget_infos;
	};

	mutable Cache m_cache;
};


#endif // WIDGET_RUNTIME_SUPPORT
#endif // INSTALLED_GADGET_LIST_H
