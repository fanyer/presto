/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 * 
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved. 
 * 
 * This file is part of the Opera web browser.  It may not be distributed 
 * under any circumstances. 
 */ 

#include "core/pch.h"
#include "adjunct/quick/controller/PluginEULAController.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickIcon.h"
#include "adjunct/quick_toolkit/widgets/QuickBrowserView.h"
#include "adjunct/quick/managers/PluginInstallManager.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/autoupdate/updatableresource.h"
#include "adjunct/autoupdate/updatablefile.h"

#ifdef PLUGIN_AUTO_INSTALL

PluginEULADialog::PluginEULADialog():
	m_loading_icon(NULL),
	m_browser_view(NULL),
	m_license_label(NULL),
	m_item(NULL),
	m_eula_failed(false)
{
}

void PluginEULADialog::InitL()
{
	SetListener(this);
	LEAVE_IF_ERROR(SetDialog("Plugin EULA Dialog"));

	m_loading_icon = m_dialog->GetWidgetCollection()->GetL<QuickIcon>("plugin_eula_loading_icon");

	QuickBrowserView* quick_eula_browser_view = m_dialog->GetWidgetCollection()->GetL<QuickBrowserView>("plugin_eula_browser");
	m_browser_view = quick_eula_browser_view->GetOpBrowserView();

	m_license_label = m_dialog->GetWidgetCollection()->GetL<QuickLabel>("plugin_eula_license_label");

	LEAVE_IF_ERROR(m_browser_view->AddListener(this));

	m_eula_failed = false;
}

OP_STATUS PluginEULADialog::StartLoadingEULA(const OpStringC& mime_type, const OpStringC& plugin_content_url)
{
	if (mime_type.IsEmpty())
		return OpStatus::ERR;

	RETURN_IF_ERROR(m_mime_type.Set(mime_type));
	RETURN_IF_ERROR(m_plugin_content_url.Set(plugin_content_url));

	m_item = g_plugin_install_manager->GetItemForMimeType(mime_type);
	RETURN_VALUE_IF_NULL(m_item, OpStatus::ERR);
	OP_ASSERT(m_item->GetResource());

	OpString eula_url, title_string;
	RETURN_IF_ERROR(m_item->GetResource()->GetAttrValue(URA_EULA_URL, eula_url));
	RETURN_IF_ERROR(m_item->GetStringWithPluginName(Str::S_PLUGIN_AUTO_INSTALL_DIALOG_TITLE_AVAILABLE, title_string));

	QuickLabel* quick_title_label = m_dialog->GetWidgetCollection()->Get<QuickLabel>("plugin_eula_title_label");
	RETURN_VALUE_IF_NULL(quick_title_label, OpStatus::ERR);
	RETURN_IF_ERROR(quick_title_label->SetText(title_string));

	if (eula_url.IsEmpty())
		return OpStatus::ERR;

	OpWindowCommander* window_commander = m_browser_view->GetWindowCommander();
	RETURN_VALUE_IF_NULL(window_commander, OpStatus::ERR);

	// DSK-336850 - Plugin Install wizard: link to plugin EULA appears in browsing history
	window_commander->DisableGlobalHistory();

	// DSK-336843 - "Resizer" appears next to the scrollbars suggesting that EULA viewport can be resized
	// The resizer is OpResizeCorner and in order to disable it we need to change the window type.
	// Should there appear any problems related to changing the type, this needs to be handled in a different way,
	// perhaps by a core patch introducing a new window type for the OpBrowserView widget, or by allowing to control
	// the OpResizeCorner visibility better (see vis_dev.cpp: "corner->SetVisibility(h_on && v_on)").
	window_commander->GetWindow()->SetType(WIN_TYPE_BRAND_VIEW);

	// DSK-336847 - TAB navigation loops inside EULA
	// Don't allow the OpBrowserView to take the focus, it's more important to have tab stops on the dialog 
	// controls - checkbox and button(s).
	m_browser_view->SetTabStop(FALSE);
	m_browser_view->SetEnabled(FALSE);

	BOOL loading_started = window_commander->OpenURL(eula_url.CStr(), FALSE);
	if (FALSE == loading_started)
		OnLoadingFailed();

	return OpStatus::OK;
}

void PluginEULADialog::OnPageUrlChanged(OpWindowCommander* commander, const uni_char* url)
{
	if (m_eula_failed)
		commander->Stop();
}

void PluginEULADialog::OnPageStartLoading(OpWindowCommander* commander)
{
	if (m_eula_failed)
		commander->Stop();
}


OP_STATUS PluginEULADialog::OnLoadingFailed()
{
	OP_ASSERT(m_item);
	OpString label_string;

	m_loading_icon->Hide();
	m_browser_view->RequestDimmed(true);
	m_browser_view->SetEnabled(false);
	m_browser_view->SetSuppressFocusEvents(true);
	m_eula_failed = true;

	RETURN_IF_ERROR(m_item->GetStringWithPluginName(Str::D_PLUGIN_AUTO_INSTALL_DIALOG_EULA_FAILED, label_string));
	RETURN_IF_ERROR(m_license_label->SetText(label_string));

	return OpStatus::OK;
}

OP_STATUS PluginEULADialog::OnLoadingOK()
{
	OP_ASSERT(m_item);
	OpString label_string;

	m_loading_icon->Hide();

	RETURN_IF_ERROR(m_item->GetStringWithPluginName(Str::S_PLUGIN_AUTO_INSTALL_VIEWING_TOS, label_string));
	RETURN_IF_ERROR(m_license_label->SetText(label_string));

	return OpStatus::OK;
}

void PluginEULADialog::OnPageLoadingFailed(OpWindowCommander* commander, const uni_char* url)
{
	OpStatus::Ignore(OnLoadingFailed());
}

void PluginEULADialog::OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user)
{
	if (commander->HttpResponseIs200())
		OpStatus::Ignore(OnLoadingOK());
	else
		OpStatus::Ignore(OnLoadingFailed());
}

BOOL PluginEULADialog::CanHandleAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_OK)
		return TRUE;
	return FALSE;
}

BOOL PluginEULADialog::DisablesAction(OpInputAction* action)
{
	return FALSE;
}

OP_STATUS PluginEULADialog::HandleAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_OK)
		if (m_dialog && m_dialog->GetDesktopWindow() && !m_dialog->GetDesktopWindow()->IsClosing())
		{
			m_dialog->GetDesktopWindow()->Close();
			return OpStatus::OK;
		}
	return OpStatus::ERR;
}

void PluginEULADialog::OnDialogClosing(DialogContext* context)
{
	SetListener(NULL);
	g_plugin_install_manager->Notify(PIM_INSTALL_DIALOG_REQUESTED, m_mime_type, NULL, NULL, NULL, m_plugin_content_url);

	if (m_browser_view)
		m_browser_view->RemoveListener(this);
}

#endif // PLUGIN_AUTO_INSTALL
