/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "ExtensionsModel.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/quick/managers/CommandLineManager.h"

#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/gadgets/OpGadgetClass.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/dochand/win.h"


ExtensionsModel::~ExtensionsModel()
{
	OpStatus::Ignore(DisableAllExtensions());
	OpStatus::Ignore(DestroyDeletedExtensions());
}

OP_STATUS ExtensionsModel::Init()
{
	Refresh();

	return OpStatus::OK;
}

void ExtensionsModel::StartAutoStartServices()
{
	BOOL clean_startup_list = FALSE;

	if (!g_pcui->GetIntegerPref(PrefsCollectionUI::RestartUniteServicesAfterCrash))
	{
		WindowRecoveryStrategy startupType = WindowRecoveryStrategy(
				g_pcui->GetIntegerPref(PrefsCollectionUI::WindowRecoveryStrategy));
		if (startupType== Restore_NoWindows)
		{
			clean_startup_list = TRUE;
		}
		if (g_pcui->GetIntegerPref(PrefsCollectionUI::ShowStartupDialog)
		 ||	(g_pcui->GetIntegerPref(PrefsCollectionUI::Running)
			 &&	g_pcui->GetIntegerPref(PrefsCollectionUI::ShowProblemDlg)
			 && !CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NoSession)))
		{
			clean_startup_list = TRUE;
		}
		g_pcui->WriteIntegerL(PrefsCollectionUI::RestartUniteServicesAfterCrash, TRUE);
	}

	for (StringHashIterator<ExtensionsModelItem> it(m_extensions); it; it++)
	{
		ExtensionsModelItem* model_item = it.GetData();
		if (clean_startup_list)
		{
			model_item->GetGadget()->SetPersistentData(GADGET_ENABLE_ON_STARTUP,(const uni_char*) NULL);
		}
		else
		{
			if (model_item->EnableOnStartup())
				EnableExtension(model_item->GetExtensionId());
		}
	}
}


int ExtensionsModel::CompareItems(const ExtensionsModelItem** a, const ExtensionsModelItem** b)
{
	OpStringC name_a = (*a)->GetGadgetClass()->GetAttribute(WIDGET_NAME_TEXT);
	OpStringC name_b = (*b)->GetGadgetClass()->GetAttribute(WIDGET_NAME_TEXT);
	return name_b.Compare(name_a);
}

OP_STATUS ExtensionsModel::Refresh()
{
	// Sort by name in descending order. Only extensions.

	ExtensionsModel::RefreshInfo refresh_info;
	OpAutoVector<ExtensionsModelItem> extensions;

	for (unsigned i = 0; i < g_gadget_manager->NumGadgets(); ++i)
	{
		OpGadget* gadget = g_gadget_manager->GetGadget(i);

		if (!gadget->IsExtension())
			continue;

		OpAutoPtr<ExtensionsModelItem> item_ptr(OP_NEW(ExtensionsModelItem, (*gadget)));
		RETURN_OOM_IF_NULL(item_ptr.get());
		RETURN_IF_ERROR(item_ptr->Init());
		RETURN_IF_ERROR(extensions.Add(item_ptr.get()));
		ExtensionsModelItem* item = item_ptr.release();

		BOOL dev_mode = FALSE;
		RETURN_IF_ERROR(ExtensionUtils::IsDevModeExtension(*gadget->GetClass(), dev_mode));
		item->SetIsInDevMode(dev_mode);

		if (item->IsDeleted())
			continue;

		if (dev_mode)
			refresh_info.m_dev_count++;
		else
			refresh_info.m_normal_count++;
	}
	extensions.QSort(CompareItems);

	NotifyAboutBeforeRefresh(refresh_info);

	// Remove old model items.
	m_extensions.DeleteAll();

	// Push to listeners.

	for (unsigned i = extensions.GetCount(); i > 0; --i)
	{
		OpAutoPtr<ExtensionsModelItem> item_ptr(extensions.Remove(i - 1));
		RETURN_IF_ERROR(m_extensions.Add(item_ptr->GetExtensionId(), item_ptr.get()));
		ExtensionsModelItem* item = item_ptr.release();

		// Keep extensions marked for deletion to ourselves.
		if (item->IsDeleted())
			continue;

		if (item->IsInDevMode())
			NotifyAboutDeveloperExtensionAdded(*item);
		else
			NotifyAboutNormalExtensionAdded(*item);
	}

	NotifyAboutAfterRefresh();

	return OpStatus::OK;
}

OP_STATUS ExtensionsModel::AddListener(Listener* listener)
{
	if (!listener)
		return OpStatus::ERR;

	return m_listeners.Add(listener);
}

OP_STATUS ExtensionsModel::RemoveListener(Listener* listener)
{
	return m_listeners.Remove(listener);
}

OP_STATUS ExtensionsModel::UninstallExtension(const OpStringC& extension_id)
{
	ExtensionsModelItem* item = FindExtensionById(extension_id);
	OP_ASSERT(item);
	if (!item)
	{
		return OpStatus::ERR;
	}

	return UninstallExtension(item);
}

OP_STATUS ExtensionsModel::UninstallExtension(ExtensionsModelItem* item)
{
	// Before triggering uninstall update on listeners, disable
	// extension.

	RETURN_IF_ERROR(DisableExtension(item,TRUE));

	// Call listeners before removing the model item. It will
	// be still available for them.

	NotifyAboutUninstall(*item);

	RETURN_IF_ERROR(item->SetDeleted(TRUE));
	Refresh();

	return OpStatus::OK;
}

OP_STATUS ExtensionsModel::ReloadExtension(const OpStringC& extension_id)
{
	ExtensionsModelItem* item = FindExtensionById(extension_id);
	OP_ASSERT(item && item->GetGadget());
	if (!item || !item->GetGadget())
	{
		return OpStatus::ERR;
	}

	item->GetGadget()->Reload();

	// in this case we dont want to notify listeners about
	// reload, if we would do that, then speeddial would
	// remove it from list and change order of extensions
	DisableExtension(item, TRUE, item->IsInDevMode());
	EnableExtension(item);

	return OpStatus::OK;
}


OP_STATUS ExtensionsModel::UpdateAvailable(const OpStringC& extension_id)
{
	ExtensionsModelItem* item = FindExtensionById(extension_id);
	OP_ASSERT(item && item->GetGadget());
	if (!item || !item->GetGadget())
	{
		return OpStatus::ERR;
	}
	item->SetIsUpdateAvailable(TRUE);

	NotifyAboutUpdateAvailable(*item);

	return OpStatus::OK;
}

OP_STATUS ExtensionsModel::UpdateExtendedName(const OpStringC& extension_id,const OpStringC& name)
{
	ExtensionsModelItem* item = FindExtensionById(extension_id);
	OP_ASSERT(item && item->GetGadget());
	if (!item || !item->GetGadget())
	{
		return OpStatus::ERR;
	}
	RETURN_IF_ERROR(item->SetExtendedName(name));
	NotifyAboutExtendedNameUpdate(*item);

	return OpStatus::OK;
}

OP_STATUS ExtensionsModel::UpdateFinished(const OpStringC& extension_id)
{
	ExtensionsModelItem* item = FindExtensionById(extension_id);
	OP_ASSERT(item && item->GetGadget());
	if (!item || !item->GetGadget())
	{
		return OpStatus::ERR;
	}
	item->SetIsUpdateAvailable(FALSE);

	NotifyAboutUpdateAvailable(*item);

	return OpStatus::OK;
}

OP_STATUS ExtensionsModel::EnableExtension(const OpStringC& extension_id)
{
	OP_NEW_DBG("ExtensionsModel::EnableExtension", "extensions");
	OP_DBG(("wuid = ") << extension_id);

	ExtensionsModelItem* item = FindExtensionById(extension_id);
	OP_ASSERT(item && item->GetGadget());
	if (!item || !item->GetGadget()) 
	{
		return OpStatus::ERR;
	}

	if (item->IsDeleted())
	{
		OP_DBG(("resurrecting extension"));
		RETURN_IF_ERROR(item->SetDeleted(FALSE));
		Refresh();
		item = FindExtensionById(extension_id);
		OP_ASSERT(item);
	}

	return EnableExtension(item);
}

OP_STATUS ExtensionsModel::EnableExtension(ExtensionsModelItem* item)
{
	OP_ASSERT(item && item->GetGadget());

	if (item->GetGadget()->GetWindow())
		return OpStatus::OK; // extension is running.

	RETURN_IF_ERROR(g_gadget_manager->OpenGadget(item->GetGadget()));

	item->SetEnableOnStartup(TRUE);

	NotifyAboutEnable(*item);

	return OpStatus::OK;
}

OP_STATUS ExtensionsModel::DisableAllExtensions()
{
	for (StringHashIterator<ExtensionsModelItem> it(m_extensions); it; it++)
	{
		RETURN_IF_ERROR(DisableExtension(it.GetData()));
	}

	return OpStatus::OK;
}

OP_STATUS ExtensionsModel::DisableExtension(const OpStringC& extension_id, 
		BOOL user_requested, BOOL notify_listeners)
{
	ExtensionsModelItem* item = FindExtensionById(extension_id);
	OP_ASSERT(item && item->GetGadget());
	if (!item || !item->GetGadget()) 
	{
		return OpStatus::ERR;
	}

	return DisableExtension(item, user_requested, notify_listeners);
}

OP_STATUS ExtensionsModel::DisableExtension(ExtensionsModelItem* item,
		BOOL user_requested, BOOL notify_listeners)
{
	RETURN_IF_ERROR(CloseExtensionWindows(item->GetGadget()));	

	// Set that this should not be run on next startup.
	if (user_requested)
	{
		item->SetEnableOnStartup(FALSE);
	}

	// Notify listeners.
	if (notify_listeners)
		NotifyAboutDisable(*item);

	return OpStatus::OK;
}

BOOL ExtensionsModel::IsEnabled(const OpStringC& extension_id)
{
	ExtensionsModelItem* model_item = FindExtensionById(extension_id);
	OP_ASSERT(model_item);
	if (!model_item)
	{
		return FALSE;
	}

	return model_item->IsEnabled();
}

BOOL ExtensionsModel::CanShowOptionsPage(const OpStringC& extension_id)
{
	ExtensionsModelItem* model_item = FindExtensionById(extension_id);
	OP_ASSERT(model_item);
	if (!model_item)
	{
		return FALSE;
	}

	return model_item->GetGadget()->HasOptionsPage() && model_item->IsEnabled();
}

OP_STATUS ExtensionsModel::OpenExtensionOptionsPage(
		const OpStringC& extension_id)
{
	ExtensionsModelItem* model_item = FindExtensionById(extension_id);
	OP_ASSERT(model_item);
	if (!model_item)
	{
		return FALSE;
	}

	return model_item->GetGadget()->OpenExtensionOptionsPage();
}

ExtensionsModelItem* ExtensionsModel::FindExtensionById(const OpStringC& extension_id) const
{
	if (extension_id.IsEmpty())
		return NULL;

	ExtensionsModelItem* item = NULL;
	RETURN_VALUE_IF_ERROR(
			m_extensions.GetData(extension_id, &item), NULL);

	return item;
}

OP_STATUS ExtensionsModel::OpenExtensionUrl(const OpStringC& url,const OpStringC& extension_id)
{
	ExtensionsModelItem* item = FindExtensionById(extension_id);
	if (!item || !item->GetGadget())	
		return OpStatus::ERR;

	OpUiWindowListener::CreateUiWindowArgs args;
	args.regular.scrollbars = true;
	args.regular.toolbars = true;
	args.regular.focus_document = true;
	args.regular.user_initiated = true;
	args.opener = item->GetGadget()->GetWindow() ? item->GetGadget()->GetWindow()->GetWindowCommander() : NULL;
	args.type = OpUiWindowListener::WINDOWTYPE_REGULAR;

	return item->GetGadget()->CreateNewUiExtensionWindow(args, url.CStr());
}

ExtensionButtonComposite* ExtensionsModel::GetButton(ExtensionId id)
{
	ExtensionButtonComposite *button = NULL;
	if (OpStatus::IsError(m_extension_buttons.GetData(INT32(id), &button)))
	{
		RETURN_VALUE_IF_ERROR(m_zombie_buttons.GetData(INT32(id), &button), NULL);
	}
	OP_ASSERT(button);
	return button;
}

OP_STATUS ExtensionsModel::CreateButton(ExtensionId id)
{
	if (m_extension_buttons.Contains(INT32(id)))
		return OpStatus::ERR;
	
	ExtensionButtonComposite *button = OP_NEW(ExtensionButtonComposite, ());
	RETURN_OOM_IF_NULL(button);

	if (OpStatus::IsError(m_extension_buttons.Add(INT32(id), button)))
	{
		OP_DELETE(button);
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

OP_STATUS ExtensionsModel::DeleteButton(ExtensionId id)
{
	ExtensionButtonComposite *button = NULL;
	RETURN_IF_ERROR(m_extension_buttons.Remove(INT32(id), &button));

	// actual destruction is delayed until all associated UI elements are destroyed
	// (UI buttons are usually destroyed from ClassicApplication::SyncSettings() which
	//  is called some after extension is uninstalled)
	if (button->HasComponents())
	{
		if (OpStatus::IsSuccess(m_zombie_buttons.Add(INT32(id), button)))
			return OpStatus::OK;
	}

	OP_DELETE(button);
	return OpStatus::OK;
}

void ExtensionsModel::OnButtonRemovedFromComposite(INT32 id)
{
	ExtensionButtonComposite *button = NULL;
	if (OpStatus::IsSuccess(m_zombie_buttons.GetData(id, &button)))
	{
		if (!button->HasComponents())
		{
			if (OpStatus::IsSuccess(m_zombie_buttons.Remove(id, &button)))
			{
				OP_DELETE(button);
			}
		}
	}
}

OP_STATUS ExtensionsModel::GetAllSpeedDialExtensions(OpVector<OpGadget>& extensions)
{
	for (StringHashIterator<ExtensionsModelItem> it(m_extensions); it; it++)
	{
		ExtensionsModelItem* item = it.GetData();
		if (ExtensionUtils::RequestsSpeedDialWindow(*(item->GetGadgetClass())))
		{
			RETURN_IF_ERROR(extensions.Add(item->GetGadget()));
		}
	}
	return OpStatus::OK;
}

void ExtensionsModel::ReloadDevModeExtensions()
{
	bool refresh = false;
	for (StringHashIterator<ExtensionsModelItem> it(m_extensions); it; it++)
	{
		ExtensionsModelItem* item = it.GetData();
		if (!item ||
			!item->IsInDevMode() || 
			!item->GetGadget())
		{
			continue;
		}

		item->GetGadget()->Reload();

		if (item->IsEnabled())
		{
			DisableExtension(item, TRUE);
			EnableExtension(item);
		}

		refresh = true;
	}

	if (refresh) Refresh();
}

OP_STATUS ExtensionsModel::CloseExtensionWindows(OpGadget* extension)
{
	OP_ASSERT(extension);
	if (!extension)
	{
		return OpStatus::ERR;
	}

	if (extension->IsRunning())
	{
		Window* gadget_window = extension->GetWindow();
		if (gadget_window != NULL)
		{
			RETURN_IF_ERROR(g_gadget_manager->CloseWindowPlease(gadget_window));
			gadget_window->Close();
			if (extension->IsExtension())
			{
				extension->SetIsClosing(FALSE);
				extension->SetIsRunning(FALSE);
			}
			
			// Notify CORE that we want to close the window now, after call to 
			// OnUiWindowClosing gadget_window will return incorrect pointers,
			// that's why we need to remember them before this call.

			OpWindow* wnd = gadget_window->GetOpWindow();
			WindowCommander* commander = gadget_window->GetWindowCommander();
			commander->OnUiWindowClosing();
			g_windowCommanderManager->ReleaseWindowCommander(commander);
			OP_DELETE(wnd);
		}
		else
		{
			return OpStatus::ERR;
		}
	}

	return OpStatus::OK;
}

void ExtensionsModel::NotifyAboutBeforeRefresh(const ExtensionsModel::RefreshInfo& info)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it);)
	{
		m_listeners.GetNext(it)->OnBeforeExtensionsModelRefresh(info);
	}
}

void ExtensionsModel::NotifyAboutAfterRefresh()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it);)
	{
		m_listeners.GetNext(it)->OnAfterExtensionsModelRefresh();
	}
}

void ExtensionsModel::NotifyAboutNormalExtensionAdded(const ExtensionsModelItem& item)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it);)
	{
		m_listeners.GetNext(it)->OnNormalExtensionAdded(item);
	}
}

void ExtensionsModel::NotifyAboutDeveloperExtensionAdded(const ExtensionsModelItem& item)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it);)
	{
		m_listeners.GetNext(it)->OnDeveloperExtensionAdded(item);
	}
}

void ExtensionsModel::NotifyAboutUninstall(const ExtensionsModelItem& item)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it);)
	{
		m_listeners.GetNext(it)->OnExtensionUninstall(item);
	}
}

void ExtensionsModel::NotifyAboutEnable(const ExtensionsModelItem& item)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it);)
	{
		m_listeners.GetNext(it)->OnExtensionEnabled(item);
	}
}

void ExtensionsModel::NotifyAboutDisable(const ExtensionsModelItem& item)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it);)
	{
		m_listeners.GetNext(it)->OnExtensionDisabled(item);
	}
}

void ExtensionsModel::NotifyAboutUpdateAvailable(const ExtensionsModelItem& item)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it);)
	{
		m_listeners.GetNext(it)->OnExtensionUpdateAvailable(item);
	}
}


void ExtensionsModel::NotifyAboutExtendedNameUpdate(const ExtensionsModelItem& item)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it);)
	{
		m_listeners.GetNext(it)->OnExtensionExtendedNameUpdate(item);
	}
}

OP_STATUS ExtensionsModel::DestroyDeletedExtensions()
{
	for (StringHashIterator<ExtensionsModelItem> it(m_extensions); it; it++)
	{
		ExtensionsModelItem* item = it.GetData();
		if (item->IsDeleted())
			RETURN_IF_ERROR(g_gadget_manager->DestroyGadget(item->GetGadget()));
	}

	return OpStatus::OK;
}
