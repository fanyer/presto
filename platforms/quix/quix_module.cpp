/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "platforms/quix/quix_module.h"

#include "platforms/quix/toolkits/ToolkitLoader.h"
#include "platforms/quix/toolkits/ToolkitLibrary.h"
#include "platforms/quix/skin/UnixWidgetPainter.h"
#include "modules/prefs/prefsmanager/prefsnotifier.h"

QuixModule::QuixModule()
  : m_toolkit_library(0)
  , m_widget_painter(0)
{
}

void QuixModule::InitL(const OperaInitInfo& info)
{
	LEAVE_IF_ERROR(ToolkitLoader::Create());

	m_toolkit_library = ToolkitLoader::GetInstance()->CreateToolkitLibrary();
	if (!m_toolkit_library)
		LEAVE(OpStatus::ERR);

	PrefsNotifier::OnFontChangedL();
	PrefsNotifier::OnColorChangedL();

	ToolkitWidgetPainter* toolkit_painter = m_toolkit_library->GetWidgetPainter();
	if (toolkit_painter)
		m_widget_painter = OP_NEW_L(UnixWidgetPainter, (toolkit_painter));
}

void QuixModule::Destroy()
{
	OP_DELETE(m_toolkit_library);
	m_toolkit_library = 0;
	// Do not delete m_widget_painter here. It gets handed over to other 
	// code that takes ownership (bug #DSK-293755)
}
