/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _WINDOWS_SHORTCUT_H_
#define _WINDOWS_SHORTCUT_H_

#include "modules/img/image.h"

struct DesktopShortcutInfo;


/**
 * Implements shortcut creation functionality.
 */
class WindowsShortcut
{
public:
	WindowsShortcut();

	/**
	 * Initialization.
	 *
	 * @param shortcut_info provides all the required shortcut data
	 * @return status
	 */
	OP_STATUS Init(const DesktopShortcutInfo& shortcut_info);

	/**
	 * Alternative initialization.  Allows overriding of the path to the shortcut
	 * file.  
	 *
	 * @param shortcut_info provides all the required shortcut data
	 * @param shortcut_path overrides the shortcut file path implied by
	 * 		@a shortcut_info.GetDestination()
	 * @return status
	 */
	OP_STATUS Init(const DesktopShortcutInfo& shortcut_info,
			const OpStringC& shortcut_path);

	/**
	 * @return the absolute path to the shortcut file created by this instance
	 */
	OpStringC GetShortcutFilePath() const;

	/**
	 * Creates the actual shortcut file.
	 *
	 * @retun status
	 */
	OP_STATUS Deploy();

	/**
	 *Returns informations about the last error occuring in Deploy()
	 */
	void GetErrData(HRESULT &hr, UINT &location) {hr = m_hr; location = m_err_loc;}

	/**
	 * Checks if application is pinned to taskbar.
	 *
	 * @param app_abs_path is absolute path of an executable
	 * @param is_pinned sets to TRUE if application is pinned. FALSE otherwise.
	 * @return status
	 */
	static OP_STATUS CheckIfApplicationIsPinned(const uni_char *app_abs_path, BOOL &is_pinned);

	/**
	 * PinToTaskbar
	 *
	 * @param appp_path is absolute path of an executable.
	 * @return status
	 */
	static OP_STATUS PinToTaskbar(const uni_char *app_path) { return ExecuteContextmenuVerb(app_path, "taskbarpin"); }

	/**
	 * UnPinFromTaskbar
	 *
	 * @param app_path is absolute path of an executable
	 * @return status
	 */
	static OP_STATUS UnPinFromTaskbar(const uni_char *app_path) { return ExecuteContextmenuVerb(app_path, "taskbarunpin"); }

private:
	OP_STATUS MakePaths();

	/**
	 * ExecuteContextmenuVerb
	 *
	 * @param appp_path absolute path of an executable.
	 * @param verb indicates action to be invoked. Examples are: taskbarpin, 
	 *             taskbarunpin, startpin, startunpin
	 * @return status
	 */
	static OP_STATUS ExecuteContextmenuVerb(const uni_char *app_path, const char *verb);

	/**
	 * GetPathLinkIsPointingTo retrives the target path pointed by a link.
	 *
	 * @param link_file absolute path of a link file.
	 * @param app_path contains the target path if fetched successfully from the link file.
	 * @return status
	 */
	static OP_STATUS GetPathLinkIsPointingTo(const OpString &link_file, OpString &app_path); 

	OP_STATUS GetTaskbarShortcutName(const uni_char * app_path, const uni_char* filename);

	const DesktopShortcutInfo* m_shortcut_info;
	OpString m_shortcut_path;

	//Stores informations about the last actual error happening
	//in Deploy, if any
	HRESULT m_hr;
	UINT m_err_loc;
};


#endif //_WINDOWS_SHORTCUT_H_
