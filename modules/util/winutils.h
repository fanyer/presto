/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file winutils.h
  * Functions working with windows */

#ifndef MODULES_UTIL_WINUTILS_H
#define MODULES_UTIL_WINUTILS_H

class Window;

/**
 * Open home url in window.
 *
 * Note: On QUICK platforms a HomepageDialog will be triggered
 * if no home url can be found rather than returning an error.
 *
 * @param window The window to open the home url in, if not homeable
 *               a new window will be created/selected.
 * @param always Open the url even if the url is already loaded in the
 *               window.
 * @param check_if_expired
 * @return OpStatus::OK if the home url got loaded, OpStatus::ERR if no
 *         home url could be found. OpStatus::ERR_NO_MEMORY on memory
 *         errors.
 */
OP_STATUS GetWindowHome(Window* window, BOOL always, BOOL check_if_expired);

#ifdef _ISHORTCUT_SUPPORT
/**
 * Open an Internet Shortcut (*.url) file and go to the link it points
 * to.
 * @param filename Internet Shortcut file.
 * @param curWin Window to open the URL in.
 * @return Status of the operation. Will leave on OOM.
 */
OP_STATUS ReadParseFollowInternetShortcutL(uni_char* filename, Window* curWin);
#endif //_ISHORTCUT_SUPPORT

#endif // !MODULES_UTIL_WINUTILS_H
