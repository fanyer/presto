/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef WINDOWS_GADGET_LIST_H
#define WINDOWS_GADGET_LIST_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/GadgetInstallerContext.h"

/**
 * Represents a read-only list of gadgets installed in the OS.  
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class WindowsGadgetList
{
public:
	/**
	 * Obtains the default instance.
	 *
	 * @return the default WindowsGadgetList instance
	 */
	static WindowsGadgetList& GetDefaultInstance();

	/**
	 * Initializes the object without actually generating any list yet.
	 *
	 * The optional parameters are probably only useful for testing.
	 *
	 * @param list_reg_key_name the name of the registry key containing
	 * 		information on installed applications
	 * @param gadget_path_prefix what the installation path read from the
	 * 		registry should be prefixed with to obtain the actual installation
	 * 		path
	 * @return status
	 */
	OP_STATUS Init(
			const OpStringC& list_reg_key_name = DEFAULT_LIST_REG_KEY_NAME,
			const OpStringC& gadget_path_prefix = OpStringC(NULL));

	/**
	 * Obtains the name of the registry key that this list is based on.
	 *
	 * @return the name of the registry key that this list is based on
	 */
	const OpStringC& GetListRegKeyName() const;

	/**
	 * Obtains the name of the registry key value used to mark a registry key as
	 * an Opera Widget key.
	 *
	 * @return the name of the registry key marker value
	 */
	static OpStringC GetRegKeyMarkerValueName();

	/**
	 * Determines the time when the OS application list contents have changed
	 * last.  This _may_ mean one or more gadgets have been installed or
	 * uninstalled and, consequently, the last list obtained from Read() is no
	 * longer up-to-date.
	 *
	 * @param mtime receives the OS list modification time
	 * @return status
	 */
	OP_STATUS GetModificationTime(time_t& mtime) const;

	/**
	 * Retrieves the list of gadgets installed in the OS.
	 *
	 * For each gadget installed, a GadgetInstalerContext instance is appended
	 * to the vector.  The vector gets the ownership of the instances.
	 *
	 * @param gadget_contexts is filled with information on gadget installations
	 * @return status
	 */
	OP_STATUS Read(OpAutoVector<GadgetInstallerContext>& gadget_contexts) const;

private:
	/**
	 * @param app_key_name the name of the reg key describing the application
	 * @param context if the application is a gadget, receives a new
	 * 		GadgetInstallerContext instance describing the gadget.  Receives
	 * 		@c NULL otherwise.
	 * @return status
	 */
	OP_STATUS CreateGadgetContext(const OpStringC& app_key_name,
			OpAutoPtr<GadgetInstallerContext>& context) const;

	OP_STATUS ReadAppInfo(const OpStringC& app_key_name,
			OpString& display_name, OpString& install_dir_path) const;

	static time_t ToTimeT(const FILETIME& filetime);

	static const uni_char DEFAULT_LIST_REG_KEY_NAME[];
	static const uni_char REG_KEY_MARKER_VALUE_NAME[];

	OpString m_list_reg_key_name;
	OpString m_gadget_path_prefix;
};

#endif // WIDGET_RUNTIME_SUPPORT

#endif // WINDOWS_GADGET_LIST_H
