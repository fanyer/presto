/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 * 
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved. 
 * 
 * This file is part of the Opera web browser.  It may not be distributed 
 * under any circumstances. 
 */ 

#include "core/pch.h"

#ifdef PLUGIN_AUTO_INSTALL

#include "adjunct/autoupdate/updatablefile.h"
#include "adjunct/autoupdate/updatableresource.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/quick/controller/PluginInstallController.h"
#include "adjunct/quick/controller/PluginEULAController.h"
#include "adjunct/quick/managers/PluginInstallManager.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickCheckBox.h"
#include "adjunct/quick_toolkit/widgets/QuickPagingLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickButtonStrip.h"
#include "adjunct/quick_toolkit/widgets/QuickRichTextLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickMultilineLabel.h"
#include "adjunct/widgetruntime/pi/PlatformGadgetUtils.h"

#ifdef MSWIN
# include "platforms/windows/utils/authorization.h"
#endif

// This will be used for the "Plugin Install Goto Page" action in the plugin install dialog for plugins
// that were not recognized via autoupdate, but their mimetype is known, i.e. http://redir.opera.com/plugins/?application/x-director
static const uni_char* PIM_PLUGIN_INFO_URL = UNI_L("http://redir.opera.com/plugins/?");
// Same as above if the mimetype is empty
static const uni_char* PIM_FALLBACK_INFO_URL = UNI_L("http://www.opera.com/docs/plugins/");

PluginInstallDialog::PluginInstallDialog():
	m_plugin_item(NULL),
	m_eula_checkbox(NULL),
	m_eula_label(NULL),
	m_general_info_label(NULL),
	m_current_page_id(PIDP_LAST),
	m_pages(NULL),
	m_plugin_can_auto_install(false),
	m_dialog_mode(PIM_DM_NONE)
{
}

PluginInstallDialog::~PluginInstallDialog()
{
	OpStatus::Ignore(g_plugin_install_manager->RemoveListener(this));
	if (m_eula_label)
		OpStatus::Ignore(m_eula_label->RemoveOpWidgetListener(*this));
	if (m_eula_checkbox)
		OpStatus::Ignore(m_eula_checkbox->RemoveOpWidgetListener(*this));
	SetListener(NULL);
}

void PluginInstallDialog::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Plugin Install Dialog"));
	ReadWidgetsL();

	OP_ASSERT(m_eula_checkbox);
	OP_ASSERT(m_eula_label);

	SetListener(this);
	LEAVE_IF_ERROR(m_eula_label->AddOpWidgetListener(*this));
	LEAVE_IF_ERROR(m_eula_checkbox->AddOpWidgetListener(*this));
	LEAVE_IF_ERROR(g_plugin_install_manager->AddListener(this));

	QuickBinder* binder = GetBinder();
	LEAVE_IF_ERROR(binder->Connect(*m_eula_checkbox, m_can_install_auto));
	LEAVE_IF_ERROR(binder->EnableIf(*GetButton("button_start_auto"), m_can_install_auto));
	LEAVE_IF_ERROR(binder->EnableIf(*GetButton("button_start_manual"), m_can_install_manual));

	LEAVE_IF_ERROR(SwitchPage(PIDP_REDIRECT));
	LEAVE_IF_ERROR(ChangeButtons());
}

void PluginInstallDialog::ReadWidgetsL()
{
	OP_ASSERT(m_dialog);
	OP_ASSERT(m_dialog->GetWidgetCollection());

	m_eula_label = m_dialog->GetWidgetCollection()->GetL<QuickMultilineLabel>("accept_eula_label_welcome");
	m_eula_checkbox = m_dialog->GetWidgetCollection()->GetL<QuickCheckBox>("accept_eula_checkbox_welcome");
	m_pages = m_dialog->GetWidgetCollection()->GetL<QuickPagingLayout>("main_layout");
	m_general_info_label = m_dialog->GetWidgetCollection()->GetL<QuickMultilineLabel>("info_label_general_info");
}

OP_STATUS PluginInstallDialog::ConfigureDialogContent(PIM_DialogMode mode, const OpStringC& mime_type, const OpStringC& plugin_content_url)
{
	RETURN_IF_ERROR(m_mime_type.Set(mime_type));
	RETURN_IF_ERROR(m_plugin_content_url.Set(plugin_content_url));
	m_plugin_item = g_plugin_install_manager->GetItemForMimeType(m_mime_type);
	m_dialog_mode = mode;

	if (mode == PIM_DM_INSTALL || mode == PIM_DM_NEEDS_DOWNLOADING || mode == PIM_DM_IS_DOWNLOADING)
		if (mime_type.IsEmpty() || !m_plugin_item || !m_plugin_item->GetIsReadyForInstall())
			m_dialog_mode = PIM_DM_REDIRECT;

	switch (m_dialog_mode)
	{
	case PIM_DM_INSTALL:
		RETURN_IF_ERROR(ReadPluginProperties());
		RETURN_IF_ERROR(InitWidgetsWithPluginName());
		RETURN_IF_ERROR(SwitchPage(PIDP_WELCOME));
		break;
	case PIM_DM_NEEDS_DOWNLOADING:
		RETURN_IF_ERROR(ReadPluginProperties());
		RETURN_IF_ERROR(InitWidgetsWithPluginName());
		RETURN_IF_ERROR(SwitchPage(PIDP_NEEDS_DOWNLOADING));
		break;
	case PIM_DM_IS_DOWNLOADING:
		RETURN_IF_ERROR(ReadPluginProperties());
		RETURN_IF_ERROR(InitWidgetsWithPluginName());
		RETURN_IF_ERROR(SwitchPage(PIDP_IS_DOWNLOADING));
		break;
	case PIM_DM_REDIRECT:
		RETURN_IF_ERROR(InitWidgetsWithoutPluginName());
		RETURN_IF_ERROR(SwitchPage(PIDP_REDIRECT));
		break;
	default:
		OP_ASSERT(!"Unknown dialog mode!");
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(SetTitle());
	m_can_install_auto.Set(false);
	RETURN_IF_ERROR(ChangeButtons());

	return OpStatus::OK;
}

OP_STATUS PluginInstallDialog::SetTitle()
{
	OpString title;
	switch (m_dialog_mode)
	{
	case PIM_DM_INSTALL:
	case PIM_DM_IS_DOWNLOADING:
	case PIM_DM_NEEDS_DOWNLOADING:
		OP_ASSERT(m_plugin_item);
		RETURN_IF_ERROR(m_plugin_item->GetStringWithPluginName(Str::S_PLUGIN_AUTO_INSTALL_DIALOG_TITLE_AVAILABLE, title));
		break;
	case PIM_DM_REDIRECT:
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_PLUGIN_AUTO_INSTALL_DIALOG_TITLE_NOT_AVAILABLE, title));
		break;
	case PIM_DM_NONE:
	default:
		OP_ASSERT(!"Unknown dialog mode");
		return OpStatus::ERR;
	}
	RETURN_IF_ERROR(m_dialog->SetTitle(title));
	m_dialog->OnContentsChanged();

	return OpStatus::OK;
}

OP_STATUS PluginInstallDialog::ReadPluginProperties()
{
	if (m_mime_type.HasContent())
	{
		if (m_plugin_item && m_plugin_item->GetResource())
		{
			RETURN_IF_ERROR(m_plugin_item->GetResource()->GetAttrValue(URA_PLUGIN_NAME, m_plugin_name));
			RETURN_IF_ERROR(m_plugin_item->GetResource()->GetAttrValue(URA_EULA_URL, m_eula_url));
			RETURN_IF_ERROR(m_plugin_item->GetResource()->GetAttrValue(URA_INSTALLS_SILENTLY, m_plugin_can_auto_install));

			return OpStatus::OK;
		}
	}
	return OpStatus::ERR;
}

OP_STATUS PluginInstallDialog::InitWidgetsWithPluginName()
{
	RETURN_IF_ERROR(InitStringForWidget("plugin_installer_name_label_welcome", Str::S_PLUGIN_AUTO_INSTALL_DIALOG_TITLE_AVAILABLE));
	RETURN_IF_ERROR(InitStringForWidget("plugin_installer_name_label_installing_auto", Str::S_PLUGIN_AUTO_INSTALL_DIALOG_TITLE_AVAILABLE));
	RETURN_IF_ERROR(InitStringForWidget("plugin_installer_name_label_general_info", Str::S_PLUGIN_AUTO_INSTALL_DIALOG_TITLE_AVAILABLE));
	
	RETURN_IF_ERROR(InitStringForWidget("accept_eula_label_welcome", Str::S_PLUGIN_AUTO_INSTALL_I_AGREE_TO_TOS));
	RETURN_IF_ERROR(InitStringForWidget("info_label_installing_auto", Str::S_PLUGIN_AUTO_INSTALL_INSTALLING_AUTO));

	OpString view_tos_string, final_tos_string;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_PLUGIN_AUTO_INSTALL_VIEW_TOS, view_tos_string));
	QuickRichTextLabel* rich_text_label = m_dialog->GetWidgetCollection()->Get<QuickRichTextLabel>("plugin_eula_label_welcome");
	RETURN_VALUE_IF_NULL(rich_text_label, OpStatus::ERR);
	RETURN_IF_ERROR(final_tos_string.AppendFormat(UNI_L("<a href='opera:/action/Plugin Install Show Eula'>%s</a>"), view_tos_string.CStr()));
	RETURN_IF_ERROR(rich_text_label->SetText(final_tos_string));

#ifdef MSWIN
	if (PIM_DM_INSTALL == m_dialog_mode)
	{
		Image uac_shield;
		OpStatus::Ignore(static_cast<DesktopOpSystemInfo*>(g_op_system_info)->GetImage(DesktopOpSystemInfo::PLATFORM_IMAGE_SHIELD, uac_shield));

		GetButton("button_start_auto")->GetOpWidget()->GetForegroundSkin()->SetBitmapImage(uac_shield, FALSE);
		GetButton("button_start_auto")->GetOpWidget()->SetButtonStyle(OpButton::STYLE_IMAGE_AND_TEXT_CENTER);

		GetButton("button_start_manual")->GetOpWidget()->GetForegroundSkin()->SetBitmapImage(uac_shield, FALSE);
		GetButton("button_start_manual")->GetOpWidget()->SetButtonStyle(OpButton::STYLE_IMAGE_AND_TEXT_CENTER);
	}
#endif // MSWIN

	return OpStatus::OK;
}

OP_STATUS PluginInstallDialog::InitWidgetsWithoutPluginName()
{
	RETURN_IF_ERROR(InitStringForWidget("plugin_installer_name_label_general_info", Str::S_PLUGIN_AUTO_INSTALL_DIALOG_TITLE_NOT_AVAILABLE));
	return OpStatus::OK;
}

OP_STATUS PluginInstallDialog::InitStringForWidget(const OpStringC8& widget_name, Str::LocaleString string_id)
{
	QuickMultilineLabel* multiline_label = NULL;
	QuickLabel* label = m_dialog->GetWidgetCollection()->Get<QuickLabel>(widget_name);

	if (NULL == label)
	{
		multiline_label = m_dialog->GetWidgetCollection()->Get<QuickMultilineLabel>(widget_name);
		if (NULL == multiline_label)
		{
			OP_ASSERT(!"Couldn't fetch label from dialog");
			return OpStatus::ERR;
		}
	}

	OpString label_string;
	if (!m_plugin_item || OpStatus::IsError(m_plugin_item->GetStringWithPluginName(string_id, label_string)))
		RETURN_IF_ERROR(g_languageManager->GetString(string_id, label_string));

	if (label)
		RETURN_IF_ERROR(label->SetText(label_string));
	else
		RETURN_IF_ERROR(multiline_label->SetText(label_string));

	return OpStatus::OK;
}

BOOL PluginInstallDialog::CanHandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
	case OpInputAction::ACTION_OK:
	case OpInputAction::ACTION_CANCEL:
	case OpInputAction::ACTION_PLUGIN_INSTALL_GOTO_PAGE:
	case OpInputAction::ACTION_PLUGIN_INSTALL_OPEN_EXTERNALLY:
	case OpInputAction::ACTION_PLUGIN_INSTALL_START_AUTO:
	case OpInputAction::ACTION_PLUGIN_INSTALL_START_MANUAL:
	case OpInputAction::ACTION_PLUGIN_INSTALL_SHOW_EULA:
	case OpInputAction::ACTION_PLUGIN_INSTALL_START_DOWNLOAD:
		return TRUE;
	};
	return FALSE;
}

BOOL PluginInstallDialog::DisablesAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
	case OpInputAction::ACTION_PLUGIN_INSTALL_START_AUTO:
		return !m_can_install_auto.Get();
	case OpInputAction::ACTION_PLUGIN_INSTALL_START_MANUAL:
		if (PIDP_AUTO_INSTALL_FAILED != m_current_page_id && PIDP_NO_AUTOINSTALL != m_current_page_id)
			return TRUE;
		break;
	};
	return FALSE;
}

OP_STATUS PluginInstallDialog::GoToWebPage()
{
	// This method opens the help page for installing the given plugin in the current Opera browser in case we're running
	// as a browser or in the opera.exe associated with the running widget runtime instance in case we're running as
	// a widget.

	OpString info_url;

	if (m_mime_type.HasContent())
	{
		RETURN_IF_ERROR(info_url.Set(PIM_PLUGIN_INFO_URL));
		RETURN_IF_ERROR(info_url.Append(m_mime_type));
	}
	else
	{
		RETURN_IF_ERROR(info_url.Set(PIM_FALLBACK_INFO_URL));
	}

	// DSK-337761 - always open the info page from within Opera, don't use the system default browser since
	// the default browser might just happen to be Internet Explorer that might just install an ActiveX
	// version of the needed plugin and that would result in the plugin still not being available in Opera.
	if (g_application->IsBrowserStarted())
	{
		// Use the standard open URL mechanism in case we're the Opera.exe
		if (!g_application->OpenURL(info_url, NO, YES, NO))
			return OpStatus::ERR;
	}
	else
	{
#ifdef _UNIX_DESKTOP_
		OP_ASSERT(!"Not implemented!");
#else
		// Use the Opera.exe associated with this widget runtime instance in case we're not a browser.
		OpString opera_exe_path;
		RETURN_IF_ERROR(PlatformGadgetUtils::GetOperaExecPath(opera_exe_path));
		RETURN_IF_ERROR(g_op_system_info->ExecuteApplication(opera_exe_path.CStr(), info_url.CStr()));
#endif // _UNIX_DESKTOP_
	}

	return OpStatus::OK;
}

OP_STATUS PluginInstallDialog::HandleAction(OpInputAction* action)
{
	OpString plugin_content_url_string;

	switch (action->GetAction())
	{
	case OpInputAction::ACTION_CANCEL:
		if (m_current_page_id == PIDP_INSTALLING_AUTO)
			OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_INSTALL_CANCEL_REQUESTED, m_mime_type));

		CloseDialog();
		return OpStatus::OK;
	case OpInputAction::ACTION_PLUGIN_INSTALL_SHOW_EULA:
		RETURN_IF_ERROR(ShowEULADialog());
		CloseDialog(TRUE);
		return OpStatus::OK;
	case OpInputAction::ACTION_PLUGIN_INSTALL_START_AUTO:
		RETURN_IF_ERROR(SwitchPage(PIDP_INSTALLING_AUTO));
		if (m_plugin_can_auto_install)
		{
			if (OpStatus::IsError(g_plugin_install_manager->Notify(PIM_AUTO_INSTALL_REQUESTED, m_mime_type)))
				RETURN_IF_ERROR(SwitchPage(PIDP_AUTO_INSTALL_FAILED));
		}
		else
			RETURN_IF_ERROR(SwitchPage(PIDP_NO_AUTOINSTALL));
		return OpStatus::OK;
	case OpInputAction::ACTION_PLUGIN_INSTALL_START_MANUAL:
		RETURN_IF_ERROR(SwitchPage(PIDP_INSTALLING_MANUAL));
		if (OpStatus::IsError(g_plugin_install_manager->Notify(PIM_MANUAL_INSTALL_REQUESTED, m_mime_type)))
			RETURN_IF_ERROR(SwitchPage(PIDP_MANUAL_INSTALL_FAILED));
		else if (m_plugin_item->GetIsItFlash())
			CloseDialog();
		return OpStatus::OK;
	case OpInputAction::ACTION_PLUGIN_INSTALL_START_DOWNLOAD:
		RETURN_IF_ERROR(g_plugin_install_manager->Notify(PIM_PLUGIN_DOWNLOAD_REQUESTED, m_mime_type));
		CloseDialog();
		return OpStatus::OK;
	case OpInputAction::ACTION_PLUGIN_INSTALL_GOTO_PAGE:
		// We're handling the "Go to Webpage" button that is intented to give the user the best help available on-line for
		// the given plugin-mime type.
		RETURN_IF_ERROR(GoToWebPage());
		CloseDialog();
		return OpStatus::OK;
		break;
	case OpInputAction::ACTION_PLUGIN_INSTALL_OPEN_EXTERNALLY:
		// We are handling DSK-330597 here.
		// User have just requested to try opening the plugin content in an external application, this will trigger the download dialog,
		// that will hopefully offer some application to play the plugin content. If not so, then user can save the content to local storage
		// should he/she wish so.

		// Get the URL string that will be used to trigger the download dialog
		OP_ASSERT(m_plugin_content_url.HasContent());

		// We're using the standard system browser here, we most probably don't need to use Opera specifically for this action.
		if (g_application->OpenURL(m_plugin_content_url, NO, YES, NO))
		{
			CloseDialog();
			return OpStatus::OK;
		}
		break;		
	default:
		OP_ASSERT(!"Add new page ID here!");
		break;
	}

	return OpStatus::ERR;
}

OP_STATUS PluginInstallDialog::SwitchPage(PluginInstallDialog::PID_Page page_id)
{
	OpString8 new_page_name;
	OpString label_text;

	m_can_install_auto.Set(false);
	m_can_install_manual.Set(false);

	switch (page_id)
	{
	case PIDP_WELCOME:
		RETURN_IF_ERROR(new_page_name.Set("welcome_page"));
		break;
	case PIDP_INSTALLING_AUTO:
		RETURN_IF_ERROR(new_page_name.Set("installing_auto_page"));
		break;
	case PIDP_NO_AUTOINSTALL:
		OP_ASSERT(m_plugin_item);
		m_can_install_manual.Set(true);
		RETURN_IF_ERROR(new_page_name.Set("general_info_page"));
		RETURN_IF_ERROR(m_plugin_item->GetStringWithPluginName(Str::D_PLUGIN_AUTO_INSTALL_DIALOG_NO_AUTOINSTALL, label_text));
		RETURN_IF_ERROR(m_general_info_label->SetText(label_text));
		break;
	case PIDP_INSTALLING_MANUAL:
		OP_ASSERT(m_plugin_item);
		RETURN_IF_ERROR(new_page_name.Set("general_info_page"));
		RETURN_IF_ERROR(m_plugin_item->GetStringWithPluginName(Str::S_PLUGIN_AUTO_INSTALL_INSTALLING_MANUAL, label_text));
		RETURN_IF_ERROR(m_general_info_label->SetText(label_text));
		break;
	case PIDP_AUTO_INSTALL_FAILED:
		OP_ASSERT(m_plugin_item);
		m_can_install_manual.Set(true);
		RETURN_IF_ERROR(new_page_name.Set("general_info_page"));
		RETURN_IF_ERROR(m_plugin_item->GetStringWithPluginName(Str::D_PLUGIN_AUTO_INSTALL_DIALOG_AUTOINSTALL_FAILED, label_text));
		RETURN_IF_ERROR(m_general_info_label->SetText(label_text));
		break;
	case PIDP_MANUAL_INSTALL_OK:
		OP_ASSERT(m_plugin_item);
		RETURN_IF_ERROR(new_page_name.Set("general_info_page"));
		RETURN_IF_ERROR(m_plugin_item->GetStringWithPluginName(Str::D_PLUGIN_AUTO_INSTALL_DIALOG_MANUAL_INSTALL_FINISHED, label_text));
		RETURN_IF_ERROR(m_general_info_label->SetText(label_text));
		break;
	case PIDP_MANUAL_INSTALL_FAILED:
		OP_ASSERT(m_plugin_item);
		RETURN_IF_ERROR(new_page_name.Set("general_info_page"));
		RETURN_IF_ERROR(m_plugin_item->GetStringWithPluginName(Str::D_PLUGIN_AUTO_INSTALL_DIALOG_INSTALL_FAILED, label_text));
		RETURN_IF_ERROR(m_general_info_label->SetText(label_text));
		break;
	case PIDP_NEEDS_DOWNLOADING:
		OP_ASSERT(m_plugin_item);
		RETURN_IF_ERROR(new_page_name.Set("general_info_page"));
		RETURN_IF_ERROR(m_plugin_item->GetStringWithPluginName(Str::S_PLUGIN_INSTALL_DIALOG_NEEDS_DOWNLOADING, label_text));
		RETURN_IF_ERROR(m_general_info_label->SetText(label_text));
		break;
	case PIDP_IS_DOWNLOADING:
		OP_ASSERT(m_plugin_item);
		RETURN_IF_ERROR(new_page_name.Set("general_info_page"));
		RETURN_IF_ERROR(m_plugin_item->GetStringWithPluginName(Str::S_PLUGIN_INSTALL_DIALOG_IS_DOWNLOADING, label_text));
		RETURN_IF_ERROR(m_general_info_label->SetText(label_text));
		break;
	case PIDP_REDIRECT:
		RETURN_IF_ERROR(new_page_name.Set("general_info_page"));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::D_PLUGIN_AUTO_INSTALL_DIALOG_REDIRECT, label_text));
		RETURN_IF_ERROR(m_general_info_label->SetText(label_text));
		break;
	case PIDP_XP_NON_ADMIN:
		OP_ASSERT(m_plugin_item);
		RETURN_IF_ERROR(new_page_name.Set("general_info_page"));
		RETURN_IF_ERROR(m_plugin_item->GetStringWithPluginName(Str::D_PLUGIN_AUTO_INSTALL_XP_NON_ADMIN, label_text));
		RETURN_IF_ERROR(m_general_info_label->SetText(label_text));
		break;
	default:
		OP_ASSERT(!"Unknown page ID!");
		return OpStatus::ERR;
	}

	QuickWidget* new_widget = m_dialog->GetWidgetCollection()->Get<QuickWidget>(new_page_name);
	if (NULL == new_widget)
	{
		OP_ASSERT(!"Could not find the page widget!");
		return OpStatus::ERR;
	}

	m_current_page_id = page_id;
	RETURN_IF_ERROR(m_pages->GoToPage(new_widget));

	RETURN_IF_ERROR(ChangeButtons());

	return OpStatus::OK;
}

OP_STATUS PluginInstallDialog::ShowButton(const char* button_name, bool show)
{
	QuickButton* button = GetButton(button_name);
	RETURN_VALUE_IF_NULL(button, OpStatus::ERR);

	if (show)
		button->Show();
	else
		button->Hide();

	return OpStatus::OK;
}

QuickButton* PluginInstallDialog::GetButton(const char* button_name)
{
	OP_ASSERT(m_dialog);
	OP_ASSERT(m_dialog->GetWidgetCollection());

	QuickButton* ret = m_dialog->GetWidgetCollection()->Get<QuickButton>(button_name);
	OP_ASSERT(ret);
	return ret;
}

OP_STATUS PluginInstallDialog::HideAllButtons()
{
	const char* BUTTON_NAMES[] = {
		"button_start_auto",
		"button_start_manual",
		"button_start_download",
		"button_goto_page",
		"button_open_externally",
		"button_close",
		"button_cancel"
	};

	for (size_t i = 0; i < ARRAY_SIZE(BUTTON_NAMES); i++)
		RETURN_IF_ERROR(ShowButton(BUTTON_NAMES[i], false));

	return OpStatus::OK;
}

OP_STATUS PluginInstallDialog::ChangeButtons()
{
	RETURN_IF_ERROR(HideAllButtons());

	switch (m_current_page_id)
	{
	case PIDP_WELCOME:
		ShowButton("button_start_auto", true);
		ShowButton("button_cancel", true);
		break;
	case PIDP_INSTALLING_AUTO:
		ShowButton("button_start_auto", true);
		ShowButton("button_cancel", true);
		break;
	case PIDP_AUTO_INSTALL_FAILED:
		ShowButton("button_start_manual", true);
		ShowButton("button_close", true);
		break;
	case PIDP_NO_AUTOINSTALL:
		ShowButton("button_start_manual", true);
		ShowButton("button_close", true);
		break;
	case PIDP_INSTALLING_MANUAL:
		ShowButton("button_start_manual", true);
		ShowButton("button_close", true);
		break;
	case PIDP_MANUAL_INSTALL_OK:
		ShowButton("button_close", true);
		break;
	case PIDP_MANUAL_INSTALL_FAILED:
		ShowButton("button_start_manual", true);
		ShowButton("button_cancel", true);
		break;
	case PIDP_NEEDS_DOWNLOADING:
		ShowButton("button_start_download", true);
		ShowButton("button_close", true);
		break;
	case PIDP_IS_DOWNLOADING:
		ShowButton("button_close", true);
		break;
	case PIDP_REDIRECT:
		ShowButton("button_goto_page", true);
		ShowButton("button_cancel", true);
		if (m_plugin_content_url.HasContent())
			ShowButton("button_open_externally", true);
		break;
	case PIDP_XP_NON_ADMIN:
		ShowButton("button_start_auto", true);
		ShowButton("button_close", true);
		break;
	default:
		OP_ASSERT(!"Add new page ID here");
		break;
	}
	
	return OpStatus::OK;
}

void PluginInstallDialog::OnChange(OpWidget *widget, BOOL)
{
	// DSK-351871
	if (m_eula_checkbox && widget && widget == m_eula_checkbox->GetOpWidget())
		g_input_manager->UpdateAllInputStates(TRUE);
}

void PluginInstallDialog::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget == m_eula_label->GetOpWidget() && button == MOUSE_BUTTON_1 && !down)
	{
		m_can_install_auto.Set(!m_can_install_auto.Get());
		// DSK-351871
		g_input_manager->UpdateAllInputStates(TRUE);
	}
}

void PluginInstallDialog::OnDialogClosing(DialogContext* context)
{
}

void PluginInstallDialog::OnPluginInstalledOK(const OpStringC& a_mime_type)
{
	if (m_mime_type.CompareI(a_mime_type) != 0)
		return;

	switch (m_current_page_id)
	{
	case PIDP_INSTALLING_AUTO:
		CloseDialog();
		break;
	case PIDP_INSTALLING_MANUAL:
		OpStatus::Ignore(SwitchPage(PIDP_MANUAL_INSTALL_OK));
		break;
	default:
		OP_ASSERT(!"Should not be here");
		break;
	}
}

void PluginInstallDialog::OnPluginInstallFailed(const OpStringC& a_mime_type)
{
	if (!m_plugin_item)
	{
		OP_ASSERT(!"Should never happen");
		return;
	}

	if (m_mime_type.CompareI(a_mime_type) != 0)
		return;

	INT32 shell_exit_code = 0;

	if (m_plugin_item->GetResource() && m_plugin_item->GetResource()->GetAsyncProcessRunner())
		shell_exit_code = m_plugin_item->GetResource()->GetAsyncProcessRunner()->GetShellExitCode();

	// Adobe requests that we show a separate error page for this specific error, that is "Need admin rights".
	// Need admin rights on XP                 = 1012
	// Need admin rights Vista                 = 1022
	// Permissions file creation error         = 1031
	if (m_plugin_item->GetIsItFlash() && ((shell_exit_code == 1012) || (shell_exit_code == 1031) || (shell_exit_code == 1022)))
	{
		//IPDG_XP_NON_ADMIN
		SwitchPage(PIDP_XP_NON_ADMIN);
		return;
	}

	switch (m_current_page_id)
	{
	case PIDP_INSTALLING_AUTO:
		OpStatus::Ignore(SwitchPage(PIDP_AUTO_INSTALL_FAILED));
		break;
	case PIDP_INSTALLING_MANUAL:
		OpStatus::Ignore(SwitchPage(PIDP_MANUAL_INSTALL_FAILED));
		break;
	default:
		OP_ASSERT(!"Should not be here");
		break;
	}
}

OP_STATUS PluginInstallDialog::ShowEULADialog()
{
	OpAutoPtr<PluginEULADialog> eula_dialog_guard;
	PluginEULADialog* eula_dialog = OP_NEW(PluginEULADialog, ());
	RETURN_OOM_IF_NULL(eula_dialog);

	eula_dialog_guard = eula_dialog;
	RETURN_IF_ERROR(eula_dialog->ShowDialog(g_application->GetActiveDesktopWindow()));
	eula_dialog_guard.release();

	RETURN_IF_ERROR(eula_dialog->StartLoadingEULA(m_mime_type, m_plugin_content_url));

	return OpStatus::OK;
}

void PluginInstallDialog::CloseDialog(bool closing_for_eula)
{
	if (m_dialog && m_dialog->GetDesktopWindow() && !m_dialog->GetDesktopWindow()->IsClosing())
	{
		m_dialog->GetDesktopWindow()->Close();

		/*
		There is a hack for showing the download dialog over the EULA dialog in case a PDF link has been
		clicked in the EULA shown in the EULA dialog.
		The hack requires notifying the plugin install manager that ALL the plugin install related dialogs have been closed, however
		the notification cannot occur when the install dialog is closed to make room for the EULA dialog.
		*/
		if (!closing_for_eula)
			OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_INSTALL_DIALOG_CLOSED, m_mime_type));
	}
}

#endif // PLUGIN_AUTO_INSTALL
