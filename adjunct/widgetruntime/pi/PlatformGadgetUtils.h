/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef PLATFORM_GADGET_UTILS_H
#define	PLATFORM_GADGET_UTILS_H

#ifdef WIDGET_RUNTIME_SUPPORT

struct GadgetInstallerContext;

namespace PlatformGadgetUtils
{
	/**
	 * Obtains the profile name to be used by a gadget.
	 *
	 * @param gadget_path the path to the gadget's GADGET_CONFIGURATION_FILE
	 * @param profile_name receives the gadget profile name
	 * @return status
	 */
	OP_STATUS GetGadgetProfileName(const OpStringC& gadget_path, 
			OpString& profile_name);

	/** 
	 * Function should return absolute path which will point to opera's 
	 * executable file.
	 *
	 * @param exec receives the absolute path to opera's executable file
	 * @return Error status
	 */
	OP_STATUS GetOperaExecPath(OpString& exec);

	/**
	 * Gets the implied path to the gadget path.  This is the path as implied by
	 * general start-up conditions, as opposed to a path requested explicitely
	 * via a command line switch, etc.
	 *
	 * @param path receives the implied gadget file path
	 * @return an error code indicates there is no implied gadget file path
	 */
	OP_STATUS GetImpliedGadgetFilePath(OpString& path);

	/**
	 * Converts a string into a normalized one (in place).
	 * Intended to use with gadget names; a normalized name can be used to build
	 * file paths, registry entries, etc.
	 *
	 * FIXME: This function has too many responsibilities! We should divide
	 * it at least into NormalizeToFileName and NormalizeToRegistryKey.
	 *
	 * @param[in,out] name the name to be normalized
	 */
    void Normalize(OpString& name);

	/**
	 * Returns absolute path of a file.
	 *
	 * @param file_path relative path to file
	 * @param absolute_path absolute path to given file
	 * @return status
	 */
	 OP_STATUS GetAbsolutePath(IN const OpString& file_path, 
		OUT OpString& absolute_path);
 
	/**
	 * Launches the installer of gadget from wgt file under given path.
	 *
	 * @param path Path to wgt file for install
	 * @return status
	 */
	OP_STATUS InstallGadget(const OpStringC& path);
	
	/**
	 * Launches the gadget described by a GadgetInstallerContext.
	 *
	 * @param context describes the gadget
	 * @return status
	 */
	OP_STATUS ExecuteGadget(const GadgetInstallerContext& context);

	/**
	 * Uninstalls the gadget described by a GadgetInstallerContext.
	 *
	 * @param context describes the gadget
	 * @return status
	 */
	OP_STATUS UninstallGadget(const GadgetInstallerContext& context);

#ifdef WIDGETS_UPDATE_SUPPORT
	/**
	 * Updates the gadget in the system described by a GadgetInstallerContext.
	 * should remove old widget files and replace them with new pointed in installer context
	 *
	 * @param context describes the gadget
	 * @return status
	 */
	OP_STATUS UpdateGadget(const GadgetInstallerContext& context);
#endif //WIDGETS_UPDATE_SUPPORT
};

#endif // WIDGET_RUNTIME_SUPPORT

#endif // !PLATFORM_GADGET_UTILS_H
