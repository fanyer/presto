/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/


#include "core/pch.h"


#include "adjunct/autoupdate_gadgets/ExtensionsUpdater.h"
#include "adjunct/quick/extensions/ExtensionInstaller.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/models/ServerWhiteList.h"

#include "modules/gadgets/OpGadgetManager.h"


ExtensionUpdater::ExtensionUpdater(GadgetUpdateController& controller):
	m_update_data(NULL),
	m_controller(&controller)
{
	m_controller->AddUpdateListener(this);
}

ExtensionUpdater::~ExtensionUpdater()
{
	m_controller->RemoveUpdateListener(this);
}

void ExtensionUpdater::OnGadgetUpdateAvailable(GadgetUpdateInfo* data, BOOL visible)
{
	if (!visible)
		return;

	OP_ASSERT(!m_update_data);
	if (data && !data->gadget_class->IsExtension())
		return;

	m_update_data = data;

	const uni_char* update_url_str = data->gadget_class->GetGadgetUpdateUrl();
	if (!update_url_str || !*update_url_str)
	{
		OP_ASSERT(!"This should never happen");
		m_controller->RejectCurrentUpdate();
	}
	else
	{
		m_controller->AllowCurrentUpdate();
	}
}

OP_STATUS ExtensionUpdater::CreateUpdateUrlFile(GadgetUpdateInfo* data)
{
	OpGadget* gadget = g_gadget_manager->FindGadget(GADGET_FIND_BY_GADGET_ID, data->gadget_class->GetGadgetId());
	RETURN_VALUE_IF_NULL(gadget, OpStatus::ERR);
	OpString path;
	RETURN_IF_ERROR(g_desktop_gadget_manager->GetUpdateUrlFileName(*gadget, path));
	OpFile file;
	RETURN_IF_ERROR(file.Construct(path));
	RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
	RETURN_IF_ERROR(file.WriteUTF8Line(data->src));
	return OpStatus::OK;
}

void  ExtensionUpdater::OnGadgetUpdateDownloaded(GadgetUpdateInfo* data,
											OpStringC& update_source)
{
	if (m_update_data == data)
	{		
		OP_STATUS err = g_desktop_extensions_manager->UpdateExtension(data,update_source);
		if (OpStatus::IsSuccess(err))
		{
			err = CreateUpdateUrlFile(data);
		}
		m_controller->GadgetUpdateFinished(err);
	}
}

void ExtensionUpdater::OnGadgetUpdateFinish(GadgetUpdateInfo* data,
									  GadgetUpdateController::GadgetUpdateResult result)
{
	if (data && m_update_data == data)
	{
		m_update_data = NULL;	
	}

	if (m_controller->GetQueueLength() == 1) // if  == 1, then we processing the last one. 
	{
		g_desktop_extensions_manager->ShowUpdateNotification();
	}
}
