/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "adjunct/desktop_pi/DesktopPopupMenuHandler.h"
#include "adjunct/widgetruntime/GadgetApplication.h"
#include "adjunct/widgetruntime/GadgetConfirmWandDialog.h"
#include "adjunct/widgetruntime/GadgetStartup.h"
#include "adjunct/widgetruntime/GadgetUtils.h"
#include "adjunct/quick/data_types/OpenURLSetting.h"
#include "adjunct/quick/windows/DesktopGadget.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "adjunct/quick/dialogs/WandMatchDialog.h"
//#include "modules/prefs/prefsmanager/collections/pc_core.h"
//#include "modules/softcore/uamanager/ua.h"

//#ifdef _MACINTOSH_		
//#include "platforms/mac/pi/desktop/MacGadgetAboutDialog.h"
//#endif // _MACINTOSH_		

GadgetApplication::GadgetApplication()
	: m_gadget_desktop_window(NULL)
	, m_input_context(NULL)
	, m_menu_handler(m_spell_check_context)
#ifdef WIDGETS_UPDATE_SUPPORT
	, m_updater(NULL)
	, m_block_app_exit(FALSE)
#endif //WIDGETS_UPDATE_SUPPORT
{
	g_wand_manager->AddListener(this);
}

GadgetApplication::~GadgetApplication()
{

	// Is NULL only if created, but not initialized.
	OP_DELETE(m_gadget_desktop_window);
	m_gadget_desktop_window = NULL;


#ifdef WIDGETS_UPDATE_SUPPORT
		g_windowCommanderManager->SetGadgetListener(NULL);
		m_update_controller.RemoveUpdateListener(this);
		OP_DELETE(m_updater);
#endif //WIDGETS_UPDATE_SUPPORT

	g_wand_manager->RemoveListener(this);

	// Save gadget persistent data.
	g_gadget_manager->SaveGadgets();

	if (NULL != m_input_context)
	{
		m_input_context->SetParentInputContext(NULL);
	}
}


OP_STATUS GadgetApplication::Start()
{
	OP_STATUS status = OpStatus::OK;

	g_desktop_global_application->OnStart();

	if (OpStatus::IsError(InitGadget()))
	{
		status = OpStatus::ERR;
		g_desktop_global_application->Exit();
	}

	return status;
}


OP_STATUS GadgetApplication::InitGadget()
{
	// On first start, load from the "requested gadget path" explicitely.  On
	// subsequent starts, load via the gadget manager.  Otherwise, the manager
	// will treat it as a different gadget each time, which breaks gadget
	// peristent data handling (among other things, probably).

	OpGadget* gadget = g_gadget_manager->GetGadget(0);
	if (NULL == gadget)
	{
		OpString gadget_path;
		RETURN_IF_ERROR(GadgetStartup::GetRequestedGadgetFilePath(gadget_path));
		if (OpStatus::IsError(g_gadget_manager->OpenGadget(gadget_path, URL_GADGET_INSTALL_CONTENT)))
		{
			OP_DELETE(m_gadget_desktop_window);
			m_gadget_desktop_window = NULL;
			OpStatus::Ignore(GadgetStartup::HandleStartupFailure(gadget_path));
			return OpStatus::ERR;
		}
	}
	else
	{
		RETURN_IF_ERROR(g_gadget_manager->OpenGadget(gadget));
	}

	// Either of the OpGadgetManager::OpenGadget() overloads should end up
	// calling GadgetApplication::UiWindowListener::CreateUiWindow().
	OP_ASSERT(NULL != m_gadget_desktop_window);

	RETURN_IF_ERROR(InitGadgetWindow());

#ifdef WIDGETS_UPDATE_SUPPORT
	if (!m_updater)
	{
		// limitation: update possiblity only once per widget run. 
		m_update_controller.AddUpdateListener(this);
		m_updater = OP_NEW(WidgetUpdater,(m_update_controller));
		g_windowCommanderManager->SetGadgetListener(&m_update_controller);
		m_gadget_desktop_window->SetUpdateController(*m_updater);	
	}
	
#endif //WIDGETS_UPDATE_SUPPORT

	return OpStatus::OK;
}


OP_STATUS GadgetApplication::InitGadgetWindow()
{
	RETURN_IF_ERROR(m_gadget_desktop_window->Init());

	RETURN_IF_ERROR(m_gadget_desktop_window->AddListener(this));

	Image gadget_icon;
	OpStatus::Ignore(GadgetUtils::GetGadgetIcon(gadget_icon, GetGadget().GetClass()));
	m_gadget_desktop_window->SetIcon(gadget_icon);

	OpStatus::Ignore(RestoreGadgetWindowSettings());
	m_gadget_desktop_window->Show(TRUE);
	m_gadget_desktop_window->Activate(TRUE);

	return OpStatus::OK;
}


void GadgetApplication::OnDesktopWindowClosing(DesktopWindow* desktop_window,
		BOOL user_initiated)
{
	if (m_gadget_desktop_window == desktop_window)
	{
#ifdef WIDGETS_UPDATE_SUPPORT
		if (!m_block_app_exit)
		{
#endif //WIDGETS_UPDATE_SUPPORT
			g_desktop_global_application->ExitStarted();

			OpStatus::Ignore(StoreGadgetWindowSettings());
#ifdef WIDGETS_UPDATE_SUPPORT
		}
#endif //WIDGETS_UPDATE_SUPPORT
		m_gadget_desktop_window->RemoveListener(this);
		m_gadget_desktop_window = NULL;
		
#ifdef WIDGETS_UPDATE_SUPPORT
		if (!m_block_app_exit)
#endif //WIDGETS_UPDATE_SUPPORT
			g_desktop_global_application->Exit();
	
	}
}


DesktopWindow* GadgetApplication::GetActiveDesktopWindow(BOOL toplevel_only)
{
	return m_gadget_desktop_window;
}


DesktopMenuHandler* GadgetApplication::GetMenuHandler()
{
	return &m_menu_handler;
}


void GadgetApplication::SetInputContext(UiContext& input_context)
{ 
	m_input_context = &input_context;
	input_context.SetParentInputContext(&m_spell_check_context);
}


OP_STATUS GadgetApplication::OnSubmit(WandInfo* info)
{
	if (info->page->CountPasswords() > 0)
	{
		OpAutoPtr<GadgetConfirmWandDialog> dialog(
				OP_NEW(GadgetConfirmWandDialog, ()));
		RETURN_OOM_IF_NULL(dialog.get());
		RETURN_IF_ERROR(dialog->Init(GetActiveDesktopWindow(TRUE), info));
		dialog.release();
	}

	return OpStatus::OK;
}

OP_STATUS GadgetApplication::OnSelectMatch(WandMatchInfo* info)
{
     WandMatchDialog* dialog = OP_NEW(WandMatchDialog, ());
     if (!dialog)
         return OpStatus::ERR_NO_MEMORY;
     OP_STATUS status = dialog->Init(GetActiveDesktopWindow(TRUE), info);
     
     if (OpStatus::IsError(status))
     {
         OP_DELETE(dialog);
         return OpStatus::ERR;       
     }   
     return OpStatus::OK;
}

OpGadget& GadgetApplication::GetGadget()
{
	return *m_gadget_desktop_window->GetOpGadget();
}

OP_STATUS GadgetApplication::RestoreGadgetWindowSettings()
{
	OP_STATUS result = OpStatus::OK;

	OP_STATUS partial_result = OpStatus::ERR;
	INT32 scale = 100;
	const uni_char* scale_str =
			GetGadget().GetPersistentData(UNI_L("last window scale"));
	if (NULL != scale_str)
	{
		errno = 0;
		INT32 parsed_scale = uni_atoi(scale_str);
		if (0 == errno)
		{
			scale = parsed_scale;
			m_gadget_desktop_window->SetScale(scale);
			partial_result = OpStatus::OK;
		}
	}
	result = OpStatus::IsSuccess(partial_result) ? result : OpStatus::ERR;

	partial_result = OpStatus::ERR;
	const uni_char* x_str =
			GetGadget().GetPersistentData(UNI_L("last window x coord"));
	const uni_char* y_str =
			GetGadget().GetPersistentData(UNI_L("last window y coord"));
	if (NULL != x_str && NULL != y_str)
	{
		errno = 0;
		INT32 x = uni_atoi(x_str);
		INT32 y = uni_atoi(y_str);
		if (0 == errno)
		{
			OpRect rect(x, y,
					GetGadget().InitialWidth() * scale / 100,
					GetGadget().InitialHeight() * scale / 100);
			m_gadget_desktop_window->FitRectToScreen(rect, FALSE);
			m_gadget_desktop_window->SetOuterPos(rect.x, rect.y);

			partial_result = OpStatus::OK;
		}
	}
	if(partial_result != OpStatus::OK)
	{
		m_gadget_desktop_window->CenterOnScreen();
	}
	result = OpStatus::IsSuccess(partial_result) ? result : OpStatus::ERR;
	
	partial_result = OpStatus::ERR;
	const uni_char* style_str =
			GetGadget().GetPersistentData(UNI_L("last window style"));
	if (NULL != style_str)
	{
		errno = 0;
		INT32 style = uni_atoi(style_str);
		if (0 == errno)
		{
			m_gadget_desktop_window->SetGadgetStyle(
					static_cast<DesktopGadget::GadgetStyle>(style));
			partial_result = OpStatus::OK;
		}
	}
	result = OpStatus::IsSuccess(partial_result) ? result : OpStatus::ERR;

	return result;
}


OP_STATUS GadgetApplication::StoreGadgetWindowSettings()
{
	INT32 x = 0;
	INT32 y = 0;
	m_gadget_desktop_window->GetOuterPos(x, y);

	OpString x_str;
	RETURN_IF_ERROR(x_str.AppendFormat(UNI_L("%d"), x));
	RETURN_IF_ERROR(GetGadget().SetPersistentData(
				UNI_L("last window x coord"), x_str.CStr()));

	OpString y_str;
	RETURN_IF_ERROR(y_str.AppendFormat(UNI_L("%d"), y));
	RETURN_IF_ERROR(GetGadget().SetPersistentData(
				UNI_L("last window y coord"), y_str.CStr()));


	UINT32 scale =  m_gadget_desktop_window->GetScale();
	OpString scale_str;
	RETURN_IF_ERROR(scale_str.AppendFormat(UNI_L("%d"), scale));
	RETURN_IF_ERROR(GetGadget().SetPersistentData(
				UNI_L("last window scale"), scale_str.CStr()));


	DesktopGadget::GadgetStyle gadget_style =
			m_gadget_desktop_window->GetGadgetStyle();
	OpString style_str;
	RETURN_IF_ERROR(style_str.AppendFormat(UNI_L("%d"), gadget_style));
	RETURN_IF_ERROR(GetGadget().SetPersistentData(
				UNI_L("last window style"), style_str.CStr()));
	
	return OpStatus::OK;
}

OpUiWindowListener* GadgetApplication::CreateUiWindowListener()
{
	return OP_NEW(UiWindowListener, (*this));
}

GadgetApplication::UiWindowListener::UiWindowListener(
		GadgetApplication& application)
	: m_application(&application)
{
}

OP_STATUS GadgetApplication::UiWindowListener::CreateUiWindow(OpWindowCommander* windowCommander,
															  OpWindowCommander* opener, UINT32 width, 
															  UINT32 height, UINT32 flags)
{
	OP_ASSERT(!"I'm not prepared for this!");
	return OpStatus::ERR;
}

OP_STATUS GadgetApplication::UiWindowListener::CreateUiWindow(
		OpWindowCommander* commander, const CreateUiWindowArgs& args)
{
	OP_ASSERT(commander != NULL);
	if (commander == NULL)
	{
		return OpStatus::ERR;
	}

	OP_ASSERT(WINDOWTYPE_WIDGET == args.type);
	if (WINDOWTYPE_WIDGET != args.type)
	{
		return OpStatus::ERR;
	}

	m_application->m_gadget_desktop_window =
			OP_NEW(DesktopGadget, (commander));
	RETURN_OOM_IF_NULL(m_application->m_gadget_desktop_window);

	return OpStatus::OK;
}

void GadgetApplication::UiWindowListener::CloseUiWindow(
		OpWindowCommander* commander)
{
	// This method body intentionally left blank.
}


#ifdef  WIDGETS_UPDATE_SUPPORT
void GadgetApplication::OnGadgetUpdateFinish(GadgetUpdateInfo* data,
											 GadgetUpdateController::GadgetUpdateResult result)
{
	switch(result)
	{
		case GadgetUpdateController::UPD_SUCCEDED:
			// update finished successfully, 
			// we can restart wgt
			m_update_controller.RemoveUpdateListener(this);
			InitGadget();
			
			break;

		case GadgetUpdateController::UPD_FATAL_ERROR:
		case GadgetUpdateController::UPD_FAILED:
		case GadgetUpdateController::UPD_DOWNLOAD_FAILED:

			break;

		default:
			OP_ASSERT(!"Unexpected index");
	}
	m_block_app_exit = FALSE;
}

void GadgetApplication::OnGadgetUpdateStarted(GadgetUpdateInfo* data)
{
	StoreGadgetWindowSettings();
	m_block_app_exit = TRUE;
	if (GetActiveDesktopWindow(TRUE))
		GetActiveDesktopWindow(TRUE)->Close();

}

#endif //WIDGETS_UPDATE_SUPPORT

BOOL GadgetApplication::OpenURL(const OpStringC &address)
{
	return g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_URL_IN_NEW_WINDOW, 0, address.CStr());
}

#endif // WIDGET_RUNTIME_SUPPORT
