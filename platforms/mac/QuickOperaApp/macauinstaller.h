/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#ifndef _MACAUINSTALLER_H_INCLUDED_
#define _MACAUINSTALLER_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

// Include system.h to get all the defines we are using
#include "platforms/mac/system.h"

#include "adjunct/autoupdate/updater/pi/auinstaller.h"

class MacAUInstaller : public AUInstaller
{
public:
	/**
	 * Installs a package
	 *
	 * @param install_path  Full path to the installation
	 * @param package_file  Full path of package to install
	 *
	 * @return AUF_Status.
	 */
	virtual AUInstaller::AUI_Status Install(uni_char *install_path, uni_char *package_file);

	/**
	 * Checks if the user has the priviledges to do the
	 * install
	 *
	 * @param install_path  Full path to the installation
	 *
	 * @return True they can.
	 */
	virtual BOOL HasInstallPrivileges(uni_char *install_path);

	/**
	 * Dialog show to check if the installation is to be applied at this startup
	 *
	 * @param caption  Text on the dialog caption
	 * @param message  Text on the dialog message
	 * @param yes_text Text on the dialog yes button
	 * @param no_text  Text on the dialog no button
	 *
	 * @return True they can.
	 */
	virtual BOOL ShowStartDialog(uni_char *caption, uni_char *message, uni_char *yes_text, uni_char *no_text);

private:
	// Runs a command line function
	AUInstaller::AUI_Status RunCmd(const char *cmd_format, const char *src, const char *dest);
	AUInstaller::AUI_Status RunPrivilegedCmd(AuthorizationRef authRef, const char *cmd, const char *args, const char *src = NULL, const char *dest = NULL);

};

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

#endif // _MACAUINSTALLER_H_INCLUDED_
