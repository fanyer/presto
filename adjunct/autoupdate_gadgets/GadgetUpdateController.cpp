/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/


#include "core/pch.h"

#ifdef GADGET_UPDATE_SUPPORT

#include "adjunct/quick/Application.h"
#include "adjunct/desktop_pi/DesktopGlobalApplication.h"

#include "adjunct/autoupdate_gadgets/GadgetUpdateController.h"

#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/windows/DesktopGadget.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/widgetruntime/GadgetApplication.h"
#include "adjunct/widgetruntime/GadgetUpdateDialog.h"

#include "modules/gadgets/OpGadgetManager.h"
#include "modules/util/path.h"
#include "modules/util/opautoptr.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#define GADGETS_UPDATE_TIMEOUT_MIN	86400		//1 day
#define GADGETS_UPDATE_TIMEOUT_MAX	2592000	    //30 days
#define GADGETS_UPDATE_TASKNAME "Gadgets Autoupdate"


GadgetUpdateController::GadgetUpdateController():  
	m_download_obj(NULL),
	m_update_result(UPD_FAILED)
{
}

void GadgetUpdateController::StartScheduler()
{
	int update_enabled = g_pcui->GetIntegerPref(PrefsCollectionUI::LevelOfUpdateAutomation);
	if (update_enabled == 0)
		return;

	int interval = g_pcui->GetIntegerPref(PrefsCollectionUI::UpdateCheckIntervalGadgets);

	if (interval > GADGETS_UPDATE_TIMEOUT_MAX)
		interval  = GADGETS_UPDATE_TIMEOUT_MAX;
	else if (interval < GADGETS_UPDATE_TIMEOUT_MIN)
		interval = GADGETS_UPDATE_TIMEOUT_MIN;

	m_update_sheduler.SetListener(this);
	m_update_sheduler.InitTask(GADGETS_UPDATE_TASKNAME);
	m_update_sheduler.AddScheduledTask(interval);
}

GadgetUpdateController::~GadgetUpdateController()
{
	// in case worked didn't call GadgetUpdateFinished

	if (m_download_obj)
	{
		OP_DELETE(m_download_obj);
		m_download_obj = NULL;
	}
	if (m_update_file_path.Length())
	{
		OpFile downloaded_file;
		if (OpStatus::IsSuccess((downloaded_file.Construct(m_update_file_path))))
		{
			downloaded_file.Delete();
			downloaded_file.Close();
		}
	}
}

void GadgetUpdateController::OnGadgetUpdateReady(const OpGadgetUpdateData& data)
{
 
	if (data.src == NULL || 
		data.src[0] == 0 || 
		data.gadget_class == NULL)
	{
		BroadcastGadgetUpdateFinish(NULL,UPD_FAILED);
		return;
	}

	const uni_char* g_ver = data.gadget_class->GetGadgetVersion();
	if (g_ver && data.version)
	{
		if (uni_strcmp(data.version, g_ver) == 0)
		{
			BroadcastGadgetUpdateFinish(NULL, UPD_FAILED);
			return;
		}
	}

	GadgetUpdateInfo* update_info = OP_NEW(GadgetUpdateInfo,());
	if (update_info== NULL)
	{
		BroadcastGadgetUpdateFinish(NULL,UPD_FAILED);
		return;
	}

	update_info->src.Set(data.src);
	update_info->details.Set(data.details);
	update_info->gadget_class = data.gadget_class;

	m_update_queue.Add(update_info);
	if (m_update_queue.GetCount() == 1)
	{
		BroadcastUpdateAvailable(m_update_queue.Get(0),TRUE);
	}
}

void GadgetUpdateController::OnGadgetUpdateError(const OpGadgetErrorData& data)
{
	BroadcastGadgetUpdateFinish(NULL,UPD_FAILED);
}

void GadgetUpdateController::GadgetUpdateFinished(OP_STATUS status)
{

	BroadcastGadgetUpdateFinish(
		m_update_queue.Get(0),OpStatus::IsSuccess(status) ?  UPD_SUCCEDED  : m_update_result);

	OpFile downloaded_file;
	if (OpStatus::IsSuccess((downloaded_file.Construct(m_update_file_path))))
	{
		downloaded_file.Delete();
		downloaded_file.Close();
	}

	if (m_download_obj)
	{
		OP_DELETE(m_download_obj);
		m_download_obj = NULL;
	}
	m_update_file_path.Delete(0);
	ProcessNextUpdate();
	
}

void GadgetUpdateController::ProcessNextUpdate()
{
	m_update_queue.Delete(0,1);

	if (m_update_queue.GetCount()>0)
	{
		BroadcastUpdateAvailable(m_update_queue.Get(0),TRUE);
	}
}


void GadgetUpdateController::RejectCurrentUpdate()
{
	BroadcastUpdateAvailable(m_update_queue.Get(0),FALSE);
	BroadcastGadgetUpdateFinish(m_update_queue.Get(0),UPD_REJECTED_BY_USER);
	ProcessNextUpdate();
}

void GadgetUpdateController::AllowCurrentUpdate()
{

	if (OpStatus::IsError(OnUpdatePossibilityConfirmed()))
	{
		BroadcastGadgetUpdateFinish(m_update_queue.Get(0),UPD_FAILED);
		ProcessNextUpdate();
	}	
}


OP_STATUS GadgetUpdateController::OnUpdatePossibilityConfirmed()
{
	RETURN_OOM_IF_NULL(
		m_download_obj = OP_NEW(SimpleDownloadItem, (*g_main_message_handler,*this)));
	OpAutoPtr<SimpleDownloadItem> auto_ptr(m_download_obj);
	RETURN_IF_ERROR(m_download_obj->Init(m_update_queue.Get(0)->src));
	auto_ptr.release();
	return OpStatus::OK;
}

void GadgetUpdateController::DownloadSucceeded(const OpStringC& widget_path)
{
	if (OpStatus::IsError(m_update_file_path.Set(widget_path.CStr())))
	{
		GadgetUpdateFinished(OpStatus::ERR);
		return;
	}

	if (!g_gadget_manager->IsThisAGadgetPath(m_update_file_path))
	{
		GadgetUpdateFinished(OpStatus::ERR);
		return;
	}
	// at this level, we are starting to close gadget app
	// and replace its files 
	m_update_result = UPD_FATAL_ERROR;

	BroadcastPreGadgetUpdate(m_update_queue.Get(0));
	BroadcastUpdateDownloaded(m_update_queue.Get(0),m_update_file_path);
}


void GadgetUpdateController::DownloadFailed()
{
	m_update_result = UPD_DOWNLOAD_FAILED;
	GadgetUpdateFinished(OpStatus::ERR);
}


void GadgetUpdateController::OnTaskTimeOut(OpScheduledTask* task)
{
	if (m_update_queue.GetCount()>0)
	{
		// if previous update is still in progress, skip further requests
		return;
	}
	g_desktop_extensions_manager->CheckForUpdate();
}

void GadgetUpdateController::BroadcastUpdateAvailable(GadgetUpdateInfo* data,BOOL available)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnGadgetUpdateAvailable(data,available);
	}
}

void GadgetUpdateController::BroadcastGadgetUpdateFinish(GadgetUpdateInfo* data,GadgetUpdateController::GadgetUpdateResult result)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnGadgetUpdateFinish(data, result);
	}
}

void GadgetUpdateController::BroadcastPreGadgetUpdate(GadgetUpdateInfo* data)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnGadgetUpdateStarted(data);
	}
}

void GadgetUpdateController::BroadcastUpdateDownloaded(GadgetUpdateInfo* data,OpStringC &path)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnGadgetUpdateDownloaded(data,path);
	}
}

#endif //GADGET_UPDATE_SUPPORT
