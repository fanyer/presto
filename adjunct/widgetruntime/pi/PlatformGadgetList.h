/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef PLATFORM_GADGET_LIST_H
#define PLATFORM_GADGET_LIST_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/GadgetInstallerContext.h"
#include "modules/util/adt/opvector.h"


/**
 * Represents a read-only list of gadgets installed in the OS.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class PlatformGadgetList
{
public:
	virtual ~PlatformGadgetList()  {}

	/**
	 * Determines if the list of gadgets installed in the OS has changed since a
	 * certain time.  If it has, an up-to-date list can be obtained from
	 * GetAll().
	 *
	 * NOTE: The implementation must be as lean as possible.  This function will
	 * potentially be invoked periodically (e.g., every 3 s or so).
	 *
	 * @param last_update_time the time when the list was known to be up-to-date
	 * 		last
	 * @return @c FALSE iff the list is up-to-date wrt @a last_update_time
	 * @see GetAll
	 */
	virtual BOOL HasChangedSince(time_t last_update_time) const = 0;

	/**
	 * Retrieves all the gadgets currently installed in the OS.
	 *
	 * For each installed gadget, a GadgetInstallerContext object is appended
	 * to @a gadget_contexts.  The vector gets ownership of the objects.
	 *
	 * @param gadget_contexts receives the GadgetInstallerContext objects
	 * @param list_globally_installed_gadgets should we get also gadgets that
	 * that are installed globally (for all users)
	 * @return status
	 * @see HasChangedSince
	 */
	virtual OP_STATUS GetAll(
			OpAutoVector<GadgetInstallerContext>& gadget_contexts,
			BOOL list_globally_installed_gadgets = FALSE) const = 0;

	/**
	 * Creates the platform-specific PlatformGadgetList implementation.
	 *
	 * @param gadget_list on success, receives a pointer to a PlatformGadgetList
	 * @return status
	 */
	static OP_STATUS Create(PlatformGadgetList** gadget_list);
};


#endif // WIDGET_RUNTIME_SUPPORT
#endif // PLATFORM_GADGET_LIST_H
