/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "platforms/quix/toolkits/ToolkitLoader.h"

#include "modules/pi/OpDLL.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/prefs/prefsmanager/prefsnotifier.h"
#include "modules/dochand/winman.h"
#include "modules/skin/OpSkinManager.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "platforms/posix/posix_native_util.h"
#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/quix/environment/DesktopEnvironment.h"
#include "platforms/quix/toolkits/ToolkitLibrary.h"
#include "platforms/quix/toolkits/x11/X11ToolkitLibrary.h"
#include "platforms/unix/base/common/unixutils.h"
#include "platforms/unix/base/x11/x11_globals.h"

#define TOOLKIT_STATE_CHANGE_TIMER_INTERVAL 1000  // [ms]

ToolkitLoader* ToolkitLoader::s_toolkit_loader = 0;

ToolkitLoader::ToolkitLoader()
	: m_toolkit_dll(0)
{
}

ToolkitLoader::~ToolkitLoader()
{
	OP_DELETE(m_toolkit_dll);
}

OP_STATUS ToolkitLoader::Init(DesktopEnvironment::ToolkitType toolkit)
{
	const uni_char* library_name = GetLibraryName(toolkit);

	if (!library_name)
		return OpStatus::OK;

	RETURN_IF_ERROR(OpDLL::Create(&m_toolkit_dll));

	OpString full_name;
	RETURN_IF_ERROR(UnixUtils::FromNativeOrUTF8(g_startup_settings.binary_dir, &full_name));
	RETURN_IF_ERROR(full_name.Append(library_name));

	if (OpStatus::IsError(m_toolkit_dll->Load(full_name)))
	{
		OP_DELETE(m_toolkit_dll);
		m_toolkit_dll = 0;
	}

	m_optimer.SetTimerListener(this);
	m_optimer.Start(TOOLKIT_STATE_CHANGE_TIMER_INTERVAL); 

	return OpStatus::OK;
}

const uni_char* ToolkitLoader::GetLibraryName(DesktopEnvironment::ToolkitType toolkit)
{
	if (toolkit == DesktopEnvironment::TOOLKIT_AUTOMATIC)
		toolkit = DesktopEnvironment::GetInstance().GetPreferredToolkit();

	switch (toolkit)
	{
		case DesktopEnvironment::TOOLKIT_GTK:
			return UNI_L("liboperagtk2.so");
		case DesktopEnvironment::TOOLKIT_GTK3:
			return UNI_L("liboperagtk3.so");
		case DesktopEnvironment::TOOLKIT_QT: /* fallthrough */
		case DesktopEnvironment::TOOLKIT_KDE:
			return UNI_L("liboperakde4.so");
		case DesktopEnvironment::TOOLKIT_X11:
			return 0;
	}

	OP_ASSERT(!"Toolkit library not implemented");
	return 0;
}

ToolkitLibrary* ToolkitLoader::CreateToolkitLibrary()
{
	OpAutoPtr<ToolkitLibrary> library;

	if (m_toolkit_dll)
	{
		typedef ToolkitLibrary* (*CreateToolkitLibraryFunction)();
		CreateToolkitLibraryFunction create_toolkit_library = (CreateToolkitLibraryFunction)m_toolkit_dll->GetSymbolAddress("CreateToolkitLibrary");
		if (!create_toolkit_library)
			return 0;

		library.reset(create_toolkit_library());
	}
	else
	{
		library.reset(OP_NEW(X11ToolkitLibrary, ()));
	}
	
	if (!library.get() || !library->Init(g_x11->GetDisplay()))
		return 0;

	library->SetMainloopRunner(&m_runner);

	return library.release();
}

void ToolkitLoader::OnTimeOut(OpTimer* timer)
{
	if (timer == &m_optimer)
	{
		// Check for style change
 		if (g_toolkit_library->IsStyleChanged())
 		{
			// Let skin manager erase to toolkit base color instead of default color (0xFFC0C0C0)
			UINT32 bg = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_BACKGROUND);
			UINT32 c = 0xFF000000 | (OP_GET_R_VALUE(bg) << 16) | (OP_GET_G_VALUE(bg) << 8) | OP_GET_B_VALUE(bg);
			g_skin_manager->SetTransparentBackgroundColor(c);

 			g_input_manager->InvokeAction(OpInputAction::ACTION_RELOAD_SKIN); // DSK-276417 Update skins on the fly
			TRAPD(rc, PrefsNotifier::OnColorChangedL());   // DSK-302216 Fix color of selected text
			g_windowManager->UpdateWindowsAllDocuments();  // DSK-311301

			OpVector<DesktopWindow> list;
			if (OpStatus::IsSuccess(g_application->GetDesktopWindowCollection().GetDesktopWindows(OpTypedObject::WINDOW_TYPE_BROWSER, list)))  // DSK-326611
				for (UINT32 i=0; i<list.GetCount(); i++)
					list.Get(i)->GetOpWindow()->UpdateMenuBar();
 		}

		m_optimer.Start(TOOLKIT_STATE_CHANGE_TIMER_INTERVAL);
	}
}

OP_STATUS ToolkitLoader::Create()
{
	s_toolkit_loader = OP_NEW(ToolkitLoader, ());
	if (!s_toolkit_loader)
		return OpStatus::ERR_NO_MEMORY;

	return s_toolkit_loader->Init();
}

void ToolkitLoader::Destroy()
{
	OP_DELETE(s_toolkit_loader);
	s_toolkit_loader = 0;
}

unsigned ToolkitLoader::MainloopRunner::RunSlice()
{
	g_component_manager->GetPlatform()->ProcessEvents(0);
	return min(g_component_manager->RunSlice(), MaxWait);
}
