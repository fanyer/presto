/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */


#include "core/pch.h"

#include "adjunct/quick/extensions/ExtensionSpeedDialHandler.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/widgets/OpSpeedDialView.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"

ExtensionSpeedDialHandler::ExtensionSpeedDialHandler()
	: m_lock(false)
{
}

void ExtensionSpeedDialHandler::OnExtensionDisabled(const ExtensionsModelItem& model_item)
{
	if (m_lock)
		return;
	AutoLock lock(m_lock);

	if (model_item.IsTemporary())
		return;

	const INT32 pos = FindSpeedDial(*model_item.GetGadget());
	if (pos >= 0 && !model_item.EnableOnStartup())
		OpStatus::Ignore(g_speeddial_manager->RemoveSpeedDial(pos));
}

void ExtensionSpeedDialHandler::OnExtensionInstalled(const OpGadget& extension,const OpStringC& source)
{
	OP_NEW_DBG("ExtensionSpeedDialHandler::OnExtensionInstalled", "extensions");
	OP_DBG(("wuid = ") << const_cast<OpGadget&>(extension).GetIdentifier());
	OP_DBG(("path = ") << const_cast<OpGadget&>(extension).GetGadgetPath());
	OP_DBG(("temporary = ") << 
		g_desktop_extensions_manager->IsExtensionTemporary(const_cast<OpGadget&>(extension).GetIdentifier()));

	if (g_desktop_extensions_manager->IsExtensionTemporary(const_cast<OpGadget&>(extension).GetIdentifier()))
		return;

	if (g_desktop_extensions_manager->IsSilentInstall())
		return;

	if (ShouldShowInSpeedDial(extension))
		ShowNewExtension(extension);
}

void ExtensionSpeedDialHandler::OnExtensionEnabled(const ExtensionsModelItem& model_item)
{	
	OP_NEW_DBG("ExtensionSpeedDialHandler::OnExtensionEnabled", "extensions");
	OP_DBG(("wuid = ") << model_item.GetExtensionId());
	OP_DBG(("path = ") << model_item.GetExtensionPath());
	OP_DBG(("m_lock = ") << m_lock);
	OP_DBG(("temporary = ") << model_item.IsTemporary());

	if (m_lock)
		return;
	AutoLock lock(m_lock);

	if (model_item.IsTemporary())
		return;

	if (g_speeddial_manager->GetTotalCount() == 0)
		return;  

	if (!ShouldShowInSpeedDial(*model_item.GetGadget()))
		return;

	SpeedDialData extension;
	extension.SetURL(model_item.GetGadget()->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_URL));
	extension.SetTitle(model_item.GetGadget()->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_TITLE),FALSE);
	extension.SetExtensionID(model_item.GetGadget()->GetIdentifier());

	int pos = -1;

	// Dev mode extensions always get a new dial.
	// This code it temporary, needed to replace SD-extensions mockup. Mockup is made  
	// as a Link sync result of SD-extension. When adding cell via add dialog, from plus button
	// we should first replace SD-extension mockup, if one exist (DSK-337088)
	//
	// That code should be removed, if full extension sync will be implemented (DSK-335264)
	BOOL dev_mode = FALSE;
	if (OpStatus::IsError(ExtensionUtils::IsDevModeExtension(*model_item.GetGadgetClass(), dev_mode))
			|| !dev_mode)
	{
		INT32 pos_checker = FindSpeedDial(*model_item.GetGadget());
		if (pos_checker > -1)
		{							
			pos = pos_checker;
		}
	}

	// Replace if already exists, add to end otherwise.
	if (pos < 0)
		pos = g_speeddial_manager->GetTotalCount() - 1;
	else
	{
		const DesktopSpeedDial* dsd = g_speeddial_manager->GetSpeedDial(pos);
		extension.SetUniqueID(dsd->GetUniqueID());
	}
	OpStatus::Ignore(g_speeddial_manager->ReplaceSpeedDial(pos, &extension));
}

void ExtensionSpeedDialHandler::OnSpeedDialAdded(const DesktopSpeedDial& entry)
{
	OP_NEW_DBG("ExtensionSpeedDialHandler::OnSpeedDialAdded", "extensions");
	OP_DBG(("entry = ") << entry);
	OP_DBG(("m_lock = ") << m_lock);
	OP_DBG(("temporary = ") << g_desktop_extensions_manager->IsExtensionTemporary(entry.GetExtensionWUID()));

	if (m_lock)
		return;
	AutoLock lock(m_lock);

	if (g_desktop_extensions_manager->IsExtensionTemporary(entry.GetExtensionWUID()))
		return;

	if (entry.GetExtensionWUID().HasContent())
		g_desktop_extensions_manager->EnableExtension(entry.GetExtensionWUID());
}

void ExtensionSpeedDialHandler::OnSpeedDialRemoving(const DesktopSpeedDial& entry)
{
	OP_NEW_DBG("ExtensionSpeedDialHandler::OnSpeedDialRemoving", "extensions");
	OP_DBG(("entry = ") << entry);
	OP_DBG(("m_lock = ") << m_lock);
	OP_DBG(("temporary = ") << g_desktop_extensions_manager->IsExtensionTemporary(entry.GetExtensionWUID()));

	if (m_lock)
		return;
	AutoLock lock(m_lock);

	if (g_desktop_extensions_manager->IsExtensionTemporary(entry.GetExtensionWUID()))
		return;

	if (entry.GetExtensionWUID().HasContent())
		g_desktop_extensions_manager->UninstallExtension(entry.GetExtensionWUID());
}

void ExtensionSpeedDialHandler::OnSpeedDialReplaced(const DesktopSpeedDial& old_entry,
		const DesktopSpeedDial& new_entry)
{
	OnSpeedDialRemoving(old_entry);
	OnSpeedDialAdded(new_entry);
}

BOOL ExtensionSpeedDialHandler::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_SHOW_SPEEDDIAL_EXTENSION_OPTIONS:
				{
					const DesktopSpeedDial* entry = g_speeddial_manager->GetSpeedDial(*child_action);
					if (entry != NULL)
					{
						OP_ASSERT(entry->GetExtensionWUID().HasContent());
						child_action->SetEnabled(g_desktop_extensions_manager->CanShowOptionsPage(
									entry->GetExtensionWUID()));
					}
					return TRUE;
				}
			}
			break;
		}
		
		case OpInputAction::ACTION_DISABLE_SPEEDDIAL_EXTENSION:
			const DesktopSpeedDial* entry = g_speeddial_manager->GetSpeedDial(*action);
			if (entry != NULL)
			{
				OP_ASSERT(entry->GetExtensionWUID().HasContent());
				OpStatus::Ignore(g_desktop_extensions_manager->DisableExtension(
							entry->GetExtensionWUID()));
			}
			return TRUE;
	}
	return FALSE;
}

OP_STATUS ExtensionSpeedDialHandler::AddSpeedDial(OpGadget& gadget)
{
	if (!g_speeddial_manager->GetTotalCount())
	{
		return OpStatus::ERR;
	}
	SpeedDialData extension;
	RETURN_IF_ERROR(extension.SetURL(gadget.GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_URL)));
	RETURN_IF_ERROR(extension.SetTitle(gadget.GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_TITLE), FALSE));
	RETURN_IF_ERROR(extension.SetExtensionID(gadget.GetIdentifier()));
	RETURN_IF_ERROR(g_speeddial_manager->InsertSpeedDial(g_speeddial_manager->GetTotalCount() - 1, &extension));
	return OpStatus::OK;
}

INT32 ExtensionSpeedDialHandler::FindSpeedDial(const OpGadget& extension)
{
	// Match by WUID first.
	for (unsigned i = 0; i < g_speeddial_manager->GetTotalCount(); ++i)
	{
		const DesktopSpeedDial* sd = g_speeddial_manager->GetSpeedDial(i);
		if (sd->GetExtensionWUID() == const_cast<OpGadget&>(extension).GetIdentifier())
			return i;
	}


	// Match by the global ID next.
	for (unsigned i = 0; i < g_speeddial_manager->GetTotalCount(); ++i)
	{
		const DesktopSpeedDial* sd = g_speeddial_manager->GetSpeedDial(i);
		if (sd->GetExtensionWUID().HasContent())
			continue;

		OpStringC gid = sd->GetExtensionID();
		// Only successful if config.xml actually specifies an ID.
		if (gid.HasContent() && gid == const_cast<OpGadget&>(extension).GetGadgetId())
			return i;
	}

	return -1;
}

void ExtensionSpeedDialHandler::AnimateInSpeedDial(const OpGadget& gadget)
{
	if (ShouldShowInSpeedDial(gadget))
	{
		ShowNewExtension(gadget);
	}
}

bool ExtensionSpeedDialHandler::ShouldShowInSpeedDial(const OpGadget& extension)
{
	return ExtensionUtils::RequestsSpeedDialWindow(*const_cast<OpGadget&>(extension).GetClass())
			&& OpStringC(const_cast<OpGadget&>(extension).GetGadgetId()).HasContent();
}

void ExtensionSpeedDialHandler::ShowNewExtension(const OpGadget& extension)
{
	if (!g_application || !g_application->GetActiveBrowserDesktopWindow())
	{
		return;
	}

	switch (g_speeddial_manager->GetState())
	{
		case SpeedDialManager::Shown:
		case SpeedDialManager::ReadOnly:
			break;
		default:
			return;
	}

	DocumentDesktopWindow* sd_window = NULL;
	DocumentView* doc_view = NULL;

	// Find the Speed Dial window that was focused last.
	OpWorkspace* workspace = g_application->GetActiveBrowserDesktopWindow()->GetWorkspace();
	for (INT32 i = 0; i < workspace->GetDesktopWindowCount(); ++i)
	{
		DesktopWindow* window = workspace->GetDesktopWindowFromStack(i);
		if (window->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
		{
			DocumentDesktopWindow* document_window = static_cast<DocumentDesktopWindow*>(window);
			doc_view = document_window->GetDocumentViewFromType(DocumentView::DOCUMENT_TYPE_SPEED_DIAL);
			if (doc_view)
			{
				sd_window = document_window;
				break;
			}
		}
	}

	if (sd_window == NULL)
	{
		// Need a new Speed Dial window.
		g_application->GetBrowserDesktopWindow(g_application->IsSDI(), FALSE, TRUE, &sd_window);
		OP_ASSERT(sd_window != NULL);
	}

	const INT32 pos = FindSpeedDial(extension);
	OP_ASSERT(pos >= 0);

	sd_window->Activate();

	if (doc_view)
		static_cast<OpSpeedDialView*>(doc_view->GetOpWidget())->ShowThumbnail(pos);
}
