/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 * 
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved. 
 * 
 * This file is part of the Opera web browser.  It may not be distributed 
 * under any circumstances. 
 */ 

#include "core/pch.h"
#include "adjunct/quick/Application.h"
#include "modules/locale/oplanguagemanager.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick/widgets/OpPluginMissingBar.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/managers/PluginInstallManager.h"
#include "adjunct/autoupdate/updatablefile.h"
#include "adjunct/quick/widgets/OpProgressField.h"

#ifdef PLUGIN_AUTO_INSTALL

DEFINE_CONSTRUCT(OpPluginMissingBar)

OpPluginMissingBar::OpPluginMissingBar()
    : m_is_loading(FALSE)
	, m_show_when_loaded(FALSE)
	, m_show_time(0)
	, m_current_state(PIMIS_Unknown)
{
	SetWrapping(OpBar::WRAPPING_OFF);
	SetShrinkToFit(TRUE);
	SetStandardToolbar(FALSE);
	SetButtonType(OpButton::TYPE_TOOLBAR);
	GetBorderSkin()->SetImage("Plugin Missing Toolbar Skin");

	SetName("Plugin Missing Toolbar");
}

OpPluginMissingBar::~OpPluginMissingBar()
{
}

OP_STATUS OpPluginMissingBar::SetMimeType(const OpStringC& mime_type)
{
	if (mime_type.IsEmpty())
		return OpStatus::ERR;

	RETURN_IF_ERROR(m_mime_type.Set(mime_type));

	PIM_PluginItem* item = g_plugin_install_manager->GetItemForMimeType(m_mime_type);
	OP_ASSERT(item);
	RETURN_VALUE_IF_NULL(item, OpStatus::ERR);
	RETURN_IF_ERROR(item->GetResource()->GetAttrValue(URA_PLUGIN_NAME, m_plugin_name));

	OpProgressField* progress_field = NULL;
	OpWidget* current_widget = GetFirstChild();
	while (current_widget)
	{
		if (current_widget->GetType() == OpTypedObject::WIDGET_TYPE_PROGRESS_FIELD)
		{
			progress_field = (OpProgressField*)current_widget;
			break;
		}
		current_widget = current_widget->GetNextSibling();
	}
	RETURN_VALUE_IF_NULL(progress_field, OpStatus::ERR);
	progress_field->SetPluginMimeType(mime_type);

	RETURN_IF_ERROR(ConfigureCurrentLayout(item->GetCurrentState()));
	return OpStatus::OK;
}

void OpPluginMissingBar::OnLayout(BOOL compute_size_only, INT32 available_width, INT32 available_height, INT32& used_width, INT32& used_height)
{
	// DSK-343045 - we need to restore the toolbar content after it has been reset by the widget framework.
	ConfigureCurrentLayout(m_current_state, true);
	OpInfobar::OnLayout(compute_size_only, available_width, available_height, used_width, used_height);
}

OP_STATUS OpPluginMissingBar::ConfigureCurrentLayout(PIM_ItemState new_state, bool do_not_layout)
{
	/**
	 * DO NOT use any calls that might result in a call to OnLayout() in case do_not_layout is TRUE.
	 */
	if (m_mime_type.IsEmpty())
		return OpStatus::ERR;

	if (!do_not_layout)
	{
		// No need to relayout to the same state, but ONLY when the toolbar has not been reset by the widget framework (DSK-343045).
		if (m_current_state == new_state)
			return OpStatus::OK;

		Hide();
	}

	m_current_state = new_state;

	OpLabel* main_label = (OpLabel*)GetWidgetByNameInSubtree("MainLabel");
	OpButton* download_button = (OpButton*)GetWidgetByNameInSubtree("DownloadBtn");
	OpButton* install_button = (OpButton*)GetWidgetByNameInSubtree("InstallBtn");
	OpButton* cancel_download_button = (OpButton*)GetWidgetByNameInSubtree("CancelDownloadButton");
	OpButton* dont_ask_button = (OpButton*)GetWidgetByNameInSubtree("DontAskBtn");
	OpButton* retry_download_button = (OpButton*)GetWidgetByNameInSubtree("RetryDownloadBtn");
	OpButton* reload_button = (OpButton*)GetWidgetByNameInSubtree("ReloadBtn");
	OpProgressField* progress_field = (OpProgressField*)GetWidgetByType(OpTypedObject::WIDGET_TYPE_PROGRESS_FIELD, FALSE);

	RETURN_VALUE_IF_NULL(main_label, OpStatus::ERR);
	RETURN_VALUE_IF_NULL(download_button, OpStatus::ERR);
	RETURN_VALUE_IF_NULL(install_button, OpStatus::ERR);
	RETURN_VALUE_IF_NULL(cancel_download_button, OpStatus::ERR);
	RETURN_VALUE_IF_NULL(dont_ask_button, OpStatus::ERR);
	RETURN_VALUE_IF_NULL(retry_download_button, OpStatus::ERR);
	RETURN_VALUE_IF_NULL(reload_button, OpStatus::ERR);
	RETURN_VALUE_IF_NULL(progress_field, OpStatus::ERR);

	// Choose the string for the label depending on the current plugin item state.
	OpString label_string, final_string;
	Str::LocaleString string_id = Str::NOT_A_STRING;
	switch (m_current_state)
	{
	case PIMIS_Available:
		string_id = Str::S_PLUGIN_TOOLBAR_NEEDS_DOWNLOAD;
		break;
	case PIMIS_Downloading:
		string_id = Str::S_PLUGIN_TOOLBAR_IS_DOWNLOADING;
		break;
	case PIMIS_Downloaded:
		string_id = Str::S_PLUGIN_TOOLBAR_READY_TO_INSTALL;
		break;
	case PIMIS_DownloadFailed:
		string_id = Str::S_PLUGIN_TOOLBAL_DOWNLOAD_FAILED;
		break;
	case PIMIS_Installed:
		string_id = Str::S_PLUGIN_TOOLBAR_INSTALLED_RELOAD_NOW;
		break;
	default:
		OP_ASSERT(!"Unknown state");
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(g_languageManager->GetString(string_id, label_string));
	RETURN_IF_ERROR(final_string.AppendFormat(label_string, m_plugin_name.CStr()));
	RETURN_IF_ERROR(main_label->SetText(final_string.CStr()));

	// Show/hide buttons depending on the current state.
	switch (m_current_state)
	{
	case PIMIS_Available:
		download_button->SetHidden(false);
		install_button->SetHidden(true);
		cancel_download_button->SetHidden(true);
		dont_ask_button->SetHidden(false);
		retry_download_button->SetHidden(true);
		reload_button->SetHidden(true);
		progress_field->SetHidden(true);
		break;
	case PIMIS_Downloading:
		download_button->SetHidden(true);
		install_button->SetHidden(true);
		cancel_download_button->SetHidden(false);
		dont_ask_button->SetHidden(true);
		retry_download_button->SetHidden(true);
		reload_button->SetHidden(true);
		progress_field->SetHidden(false);
		break;
	case PIMIS_Downloaded:
		download_button->SetHidden(true);
		install_button->SetHidden(false);
		cancel_download_button->SetHidden(true);
		dont_ask_button->SetHidden(true);
		retry_download_button->SetHidden(true);
		reload_button->SetHidden(true);
		progress_field->SetHidden(true);
		break;
	case PIMIS_DownloadFailed:
		download_button->SetHidden(true);
		install_button->SetHidden(true);
		cancel_download_button->SetHidden(true);
		dont_ask_button->SetHidden(true);
		retry_download_button->SetHidden(false);
		reload_button->SetHidden(true);
		progress_field->SetHidden(true);
		break;
	case PIMIS_Installed:
		download_button->SetHidden(true);
		install_button->SetHidden(true);
		cancel_download_button->SetHidden(true);
		dont_ask_button->SetHidden(true);
		retry_download_button->SetHidden(true);
		reload_button->SetHidden(false);
		progress_field->SetHidden(true);
		break;
	default:
		OP_ASSERT(!"Unknown state!");
		return OpStatus::ERR;
	}

	if (!do_not_layout)
	{
		Show();
		InvalidateAll();
	}

	return OpStatus::OK;
}

BOOL OpPluginMissingBar::Show()
{
	if (m_mime_type.IsEmpty())
		return FALSE;

	m_show_time = g_op_time_info->GetRuntimeMS();
    return OpInfobar::Show();
}

BOOL OpPluginMissingBar::Hide()
{
	if (GetAlignment() == ALIGNMENT_OFF)
		return FALSE;

	g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PAGE);
	return OpInfobar::Hide();
}

BOOL OpPluginMissingBar::OnInputAction(OpInputAction* action)
{
    switch (action->GetAction())
    {
		case OpInputAction::ACTION_PLUGIN_INSTALL_RETRY_DOWNLOAD:
        case OpInputAction::ACTION_PLUGIN_INSTALL_START_DOWNLOAD:
			OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_PLUGIN_DOWNLOAD_REQUESTED, m_mime_type));
			return TRUE;
		case OpInputAction::ACTION_PLUGIN_INSTALL_SHOW_DIALOG:
			OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_INSTALL_DIALOG_REQUESTED, m_mime_type));
			return TRUE;
		case OpInputAction::ACTION_PLUGIN_INSTALL_CANCEL_DOWNLOAD:
			OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_PLUGIN_DOWNLOAD_CANCEL_REQUESTED, m_mime_type));
			return TRUE;
		case OpInputAction::ACTION_PLUGIN_INSTALL_RELOAD_PAGE:
			OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_RELOAD_ACTIVE_WINDOW, m_mime_type));
			Hide();
			return TRUE;
		case OpInputAction::ACTION_PLUGIN_INSTALL_DONT_SHOW_TOOLBAR:
			OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_SET_DONT_SHOW, m_mime_type));
			Hide();
			return TRUE;
        case OpInputAction::ACTION_CANCEL:
            Hide();
			return TRUE;
    };
    return OpToolbar::OnInputAction(action);
}

void OpPluginMissingBar::OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url)
{
	if(g_op_time_info->GetRuntimeMS() - m_show_time < 500)
		return;

	Hide();
}

void OpPluginMissingBar::OnStartLoading(DocumentDesktopWindow* document_window)
{
	m_is_loading = TRUE;
}

void OpPluginMissingBar::OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus, BOOL was_stopped_by_user)
{
	if(m_show_when_loaded)
	{
		m_show_when_loaded = FALSE;
		Show();
	}
	m_is_loading = FALSE;
}

void OpPluginMissingBar::ShowWhenLoaded()
{
	if(!m_is_loading)
	{
		m_show_when_loaded = FALSE;
		Show();
	}
	else
		m_show_when_loaded = TRUE;
}

void OpPluginMissingBar::OnPluginResolved(const OpStringC& mime_type)
{
	// Ignore
}

void OpPluginMissingBar::OnPluginAvailable(const OpStringC& mime_type)
{
	if (m_mime_type.CompareI(mime_type))
		return;

	if (OpStatus::IsError(ConfigureCurrentLayout(PIMIS_Available)))
		Hide();
}

void OpPluginMissingBar::OnPluginDownloadStarted(const OpStringC& mime_type)
{
	if (m_mime_type.CompareI(mime_type))
		return;

	if (OpStatus::IsError(ConfigureCurrentLayout(PIMIS_Downloading)))
		Hide();
}

void OpPluginMissingBar::OnPluginDownloadCancelled(const OpStringC& mime_type)
{
	if (m_mime_type.CompareI(mime_type))
		return;

	if (OpStatus::IsError(ConfigureCurrentLayout(PIMIS_Available)))
		Hide();
}

void OpPluginMissingBar::OnPluginDownloaded(const OpStringC& mime_type)
{
	if (m_mime_type.CompareI(mime_type))
		return;

	if (OpStatus::IsError(ConfigureCurrentLayout(PIMIS_Downloaded)))
		Hide();
}

void OpPluginMissingBar::OnPluginInstalledOK(const OpStringC& mime_type)
{
	if (m_mime_type.CompareI(mime_type))
		return;

	if (OpStatus::IsError(ConfigureCurrentLayout(PIMIS_Installed)))
		Hide();
}

void OpPluginMissingBar::OnPluginInstallFailed(const OpStringC& mime_type)
{
	// Ignore
}

void OpPluginMissingBar::OnPluginsRefreshed()
{
	// Ignore
}

void OpPluginMissingBar::OnFileDownloadProgress(const OpStringC& mime_type, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long estimate)
{
	// Ignore
	// The OpProgressField handles the notifications on its own
}

void OpPluginMissingBar::OnFileDownloadDone(const OpStringC& mime_type, OpFileLength total_size)
{
	if (m_mime_type.CompareI(mime_type))
		return;

	if (OpStatus::IsError(ConfigureCurrentLayout(PIMIS_Downloaded)))
		Hide();
}

void OpPluginMissingBar::OnFileDownloadFailed(const OpStringC& mime_type)
{
	if (m_mime_type.CompareI(mime_type))
		return;

	if (OpStatus::IsError(ConfigureCurrentLayout(PIMIS_DownloadFailed)))
		Hide();
}

void OpPluginMissingBar::OnFileDownloadAborted(const OpStringC& mime_type)
{
	if (m_mime_type.CompareI(mime_type))
		return;

	if (OpStatus::IsError(ConfigureCurrentLayout(PIMIS_Available)))
		Hide();
}

#endif // PLUGIN_AUTO_INSTALL
