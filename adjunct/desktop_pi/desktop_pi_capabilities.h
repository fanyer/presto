/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DESKTOP_PI_CAPABILITIES_H
# define DESKTOP_PI_CAPABILITIES_H

/** DesktopOpLowLevelFile is available */
# define DPI_CAP_DESKTOP_LOWLEVELFILE

/** DesktopSystemInfo is available */
# define DPI_CAP_DESKTOP_SYSTEMINFO

/** DesktopWindowManager is available*/
# define DPI_CAP_DESKTOP_WINDOWMANAGER

/** DesktopFileChooser is available */
# define DPI_CAP_DESKTOP_FILECHOOSER

/** One can ask about running downloaded content */
# define DPI_CAP_ALLOW_RUNNING_DOWNLOADED

/** We have a desktopbitmap with extended api */
# define DPI_CAP_DESKTOPBITMAP

/** We have DesktopOpSystemInfo with PlatformExecuteApplication */
# define DPI_CAP_PLATFORM_EXECUTE

/** We have ShallWeTryToSetOperaAsDefaultBrowser and SetAsDefaultBrowser in the DesktopOpSystemInfo api */
#define DPI_CAP_DEFAULT_BROWSER
#endif // !DESKTOP_PI_CAPABILITIES_H
