/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef WINDOWS_GADGET_UTILS_H
#define WINDOWS_GADGET_UTILS_H

#ifdef WIDGET_RUNTIME_SUPPORT

/**
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class WindowsGadgetUtils
{
public:
	/**
	 * Look for GADGET_CONFIGURATION_FILE in the file tree under a possible
	 * gadget installation directory.
	 *
	 * @param dir_path a directory potentially being a root of a gadget
	 * 		installation
	 * @param gadget_file_path on success, receives the path to the installed
	 * 		gadget's GADGET_CONFIGURATION_FILE
	 * @return status
	 */
	static OP_STATUS FindGadgetFile(const OpStringC& dir_path,
			OpString& gadget_file_path);

	/**
	 * Builds the path to the executable to be used to start an installed
	 * gadget.
	 *
	 * @param context the gadget context
	 * @param starter_path receives the gadget starter path
	 * @return status
	 */
	static OP_STATUS MakeGadgetStarterPath(
			const GadgetInstallerContext& context, OpString& starter_path);

	/**
	 * Builds the path to the gadget uninstaller script.
	 *
	 * @param context the gadget context
	 * @param uninstaller_path receives the path to the gadget uninstaller
	 * 		script
	 * @return status
	 */
	static OP_STATUS MakeGadgetUninstallerPath(
			const GadgetInstallerContext& context, OpString& uninstaller_path);

	/**
	 * Uninstalls widgets from the system
	 *
	 * @param context the gadget context
	 * @param uninstall_for_update switches uninstall script to prepare widgets 
	 *			installation dir for update install
	 * 	
	 * @return status
	 */
	static OP_STATUS Uninstall(
		const GadgetInstallerContext& context, BOOL update_uninstall = FALSE);

	/**
	* Reads widget log file
	* 
	* @param widget_installation_path path to widget installations folder
	* @param file_content readed file content
	*/
	static OP_STATUS ReadWidgetLogFile(const OpString& widget_installation_path, OpString& file_content);

	/**
	* Keeps information from install.log file, which will not be restored during the update
	* 
	* @param preserved_list information that will have to be added to install.log file after update
	* @param file_content content of the uninstall log file
	*/
	static OP_STATUS PreserveLogInformation(OpString& preserved_list,OpString& file_content);

	/**
	* During update widget is partly uninstalled. Also partly install.log file is recreated as a result of partly update-install.
	* This funciton helps prevent some information from beeing lost for regular uninstall (exe and lnk files). 
	*/
	static OP_STATUS RestoreLogInformation(const OpString& widget_installation_path,const OpString& preserved_list);
};

#endif // WIDGET_RUNTIME_SUPPORT

#endif // WINDOWS_GADGET_UTILS_H
