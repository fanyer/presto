/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#ifndef WINDOWSLANGUAGEMANAGER_H
#define WINDOWSLANGUAGEMANAGER_H

#include "modules/locale/src/opprefsfilelanguagemanager.h"

/**
 * Windows specialization of the locale string manager.
 *
 * Uses the platform-independent locale string manager, but falls back to
 * using the built-in resources if the string could not be loaded from the
 * external translation file.
 *
 * @author Peter Karlsson
 */
class WindowsLanguageManager: public OpPrefsFileLanguageManager
{
public:
	WindowsLanguageManager();
	virtual ~WindowsLanguageManager() {};

protected:
	virtual int GetStringInternalL(int num, OpString &s);
};


#endif // WINDOWSLANGUAGEMANAGER_H