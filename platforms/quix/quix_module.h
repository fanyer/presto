/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUIX_MODULE_H
#define QUIX_MODULE_H

#include "modules/hardcore/opera/module.h"

class ToolkitLibrary;
class UnixWidgetPainter;

class QuixModule : public OperaModule
{
public:
	QuixModule();

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	inline ToolkitLibrary* GetToolkitLibrary() { return m_toolkit_library; }
	inline UnixWidgetPainter* GetWidgetPainter() { return m_widget_painter; }

private:
	ToolkitLibrary* m_toolkit_library;
	UnixWidgetPainter* m_widget_painter;
};

#define g_toolkit_library (g_opera->quix_module.GetToolkitLibrary())
#define g_unix_widget_painter (g_opera->quix_module.GetWidgetPainter())

#ifdef _UNIX_DESKTOP_
# define QUIX_MODULE_REQUIRED
#endif // _UNIX_DESKTOP_

#endif // QUIX_MODULE_H
