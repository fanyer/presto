// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Øyvind Østlund && Julien Picalausa
//

#include "core/pch.h"


#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#include "platforms/windows/pi/WindowsOpMessageLoop.h"
#include "platforms/windows/installer/OperaInstallerUI.h"
#include "platforms/windows/installer/InstallerWizard.h"


const OperaInstallerUI::ErrorToStringMapping OperaInstallerUI::m_error_mapping [] = { 
	{OperaInstaller::INSTALLER_INIT_FOLDER_FAILED, Str::D_INSTALLER_INIT_FOLDER_FAILED},
	{OperaInstaller::INSTALLER_INIT_OPERATION_FAILED, Str::D_INSTALLER_INIT_OPERATION_FAILED},
	{OperaInstaller::INVALID_FOLDER, Str::D_INSTALLER_INIT_FOLDER_FAILED},
	{OperaInstaller::CREATE_FOLDER_FAILED, Str::D_INSTALLER_CREATE_FOLDER_FAILED},
	{OperaInstaller::CANT_GET_FOLDER_WRITE_PERMISSIONS, Str::D_INSTALLER_CANT_GET_FOLDER_WRITE_PERMISSIONS},
	{OperaInstaller::COPY_FILE_REPLACE_FAILED, Str::D_INSTALLER_COPY_FILE_REPLACE_FAILED},
	{OperaInstaller::COPY_FILE_WRITE_FAILED, Str::D_INSTALLER_COPY_FILE_WRITE_FAILED},
	{OperaInstaller::SHORTCUT_REPLACE_FAILED, Str::D_INSTALLER_SHORTCUT_REPLACE_FAILED},
	{OperaInstaller::SHORTCUT_DEPLOY_FAILED, Str::D_INSTALLER_SHORTCUT_DEPLOY_FAILED},
	{OperaInstaller::DEFAULT_PREFS_FILE_CREATE_FAILED, Str::D_INSTALLER_DEFAULT_PREFS_FILE_CREATE_FAILED},
	{OperaInstaller::DEFAULT_PREFS_FILE_COMMIT_FAILED, Str::D_INSTALLER_DEFAULT_PREFS_FILE_COMMIT_FAILED},
	{OperaInstaller::FIXED_PREFS_MAKE_COPY_FAILED, Str::D_INSTALLER_FIXED_PREFS_MAKE_COPY_FAILED},
	{OperaInstaller::FIXED_PREFS_COPY_CONTENT_FAILED, Str::D_INSTALLER_FIXED_PREFS_COPY_CONTENT_FAILED},
	{OperaInstaller::SAVE_LOG_FAILED, Str::D_INSTALLER_SAVE_LOG_FAILED},
	{OperaInstaller::DELETE_OLD_FILE_FAILED, Str::D_INSTALLER_DELETE_OLD_FILE_FAILED},
	{OperaInstaller::DELETE_OLD_SHORTCUT_FAILED, Str::D_INSTALLER_DELETE_OLD_SHORTCUT_FAILED},
	{OperaInstaller::REGISTRY_OPERATION_FAILED, Str::D_INSTALLER_REGISTRY_OPERATION_FAILED},
	{OperaInstaller::DELETE_OLD_REG_VALUE_FAILED, Str::D_INSTALLER_DELETE_OLD_REG_VALUE_FAILED},
	{OperaInstaller::DELETE_OLD_KEY_FAILED, Str::D_INSTALLER_DELETE_OLD_KEY_FAILED},
	{OperaInstaller::DELETE_OLD_LOG_FAILED, Str::D_INSTALLER_DELETE_OLD_LOG_FAILED},
	{OperaInstaller::FILES_LIST_MISSING, Str::D_INSTALLER_FILES_LIST_MISSING},
	{OperaInstaller::ELEVATION_SHELL_EXECUTE_FAILED, Str::D_INSTALLER_ELEVATION_SHELL_EXECUTE_FAILED},
	{OperaInstaller::CANT_INSTALL_SINGLE_PROFILE_WITHOUT_WRITE_ACCESS, Str::D_INSTALLER_CANT_INSTALL_SINGLE_PROFILE_WITHOUT_WRITE_ACCESS},
	{OperaInstaller::OTHER_USERS_ARE_LOGGED_ON, Str::D_INSTALLER_OTHER_USER_IS_LOGGED_ON},
	{OperaInstaller::CANT_ELEVATE_WITHOUT_UAC, Str::D_INSTALLER_CANT_ELEVATE_WITHOUT_UAC},
	{OperaInstaller::CANT_OBTAIN_WRITE_ACCESS, Str::D_INSTALLER_CANT_OBTAIN_WRITE_ACCESS},
	{OperaInstaller::MSI_UNINSTALL_FAILED, Str::D_INSTALLER_MSI_UNINSTALL_FAILED},
	{OperaInstaller::PIN_TO_TASKBAR_FAILED, Str::D_INSTALLER_PIN_TO_TASKBAR_FAILED},
	{OperaInstaller::UNPIN_FROM_TASKBAR_FAILED, Str::D_INSTALLER_UNPIN_FROM_TASKBAR_FAILED},

};

OperaInstallerUI::OperaInstallerUI(OperaInstaller* opera_installer)
	: m_opera_installer(opera_installer)
	, m_dialog(NULL)
	, m_paused(0)
{
	g_main_message_handler->SetCallBack(this, MSG_WIN_INSTALLER_NEXT_STEP, (MH_PARAM_1)this);
}

OperaInstallerUI::~OperaInstallerUI()
{
	g_main_message_handler->RemoveCallBack(this, MSG_WIN_INSTALLER_NEXT_STEP);
}

void OperaInstallerUI::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	OP_ASSERT (desktop_window == m_dialog);

	m_dialog = NULL;
	g_main_message_handler->RemoveCallBack(this, MSG_WIN_INSTALLER_NEXT_STEP);
	g_desktop_global_application->Exit();
	OP_DELETE(m_opera_installer);
}

void OperaInstallerUI::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT (msg == MSG_WIN_INSTALLER_NEXT_STEP);

	if (m_paused > 0)
		return;

	if (!m_dialog || m_dialog->IsClosing())
		return;

	OP_STATUS status = m_opera_installer->DoStep();

	if (OpStatus::IsError(status) && status != OpStatus::ERR_YIELD)
	{
		m_dialog->Close(TRUE);
		return;
	}

	if (status != OpStatus::ERR_YIELD)
	{
		m_dialog->IncProgress();
	}
	
	if (m_paused <= 0)
	{
		g_main_message_handler->PostMessage(MSG_WIN_INSTALLER_NEXT_STEP, (MH_PARAM_1)this, 0);
	}
}

OP_STATUS OperaInstallerUI::Show()
{
	OP_ASSERT(m_opera_installer && !m_dialog);

	m_dialog = OP_NEW(InstallerWizard, (m_opera_installer));
	RETURN_OOM_IF_NULL(m_dialog);
	
	RETURN_IF_ERROR(m_dialog->AddListener(this));

	if (m_opera_installer->GetOperation() == OperaInstaller::OpUninstall)
		return m_dialog->Init(InstallerWizard::INSTALLER_DLG_UNINSTALL);

	return m_dialog->Init(InstallerWizard::INSTALLER_DLG_STARTUP);
}

OP_STATUS OperaInstallerUI::ShowProgress(UINT steps_count)
{
	if (!m_dialog)
	{
		m_dialog = OP_NEW(InstallerWizard, (m_opera_installer));
		RETURN_OOM_IF_NULL(m_dialog);
		RETURN_IF_ERROR(m_dialog->AddListener(this));
		RETURN_IF_ERROR(m_dialog->Init(InstallerWizard::INSTALLER_DLG_PROGRESS_INFO));
	}
	else
	{
		m_dialog->SetCurrentPage(InstallerWizard::INSTALLER_DLG_PROGRESS_INFO);
	}

	m_dialog->SetProgressSteps(steps_count);	
	m_paused = 0;
	g_main_message_handler->PostMessage(MSG_WIN_INSTALLER_NEXT_STEP, (MH_PARAM_1)this, 0);
	
	return OpStatus::OK;
}

void OperaInstallerUI::ForbidClosing(BOOL forbid_closing)
{
	if (m_dialog)
		m_dialog->ForbidClosing(forbid_closing);
}

void OperaInstallerUI::HideAndClose()
{
	if (m_dialog)
	{
		m_dialog->Show(FALSE);
		m_dialog->Close(FALSE);
	}
	else
		g_desktop_global_application->Exit();
}

void OperaInstallerUI::Pause(BOOL pause)
{
	if (pause)
		m_paused++;
	else
		m_paused--;

	OP_ASSERT(m_paused >= 0);
	if (m_paused < 0)
		m_paused = 0;

	if (m_paused == 0 && m_opera_installer->IsRunning())
		g_main_message_handler->PostMessage(MSG_WIN_INSTALLER_NEXT_STEP, (MH_PARAM_1)this, 0);
}

OP_STATUS OperaInstallerUI::ShowLockingProcesses(const uni_char* file_name, OpVector<ProcessItem>& processes)
{
	// Show the locked file handles by process page in a new dialog (to not lose the state).
	InstallerWizard *dialog = OP_NEW(InstallerWizard, (m_opera_installer));
	RETURN_OOM_IF_NULL(dialog);
	RETURN_IF_ERROR(dialog->Init(InstallerWizard::INSTALLER_DLG_HANDLE_LOCKED));

	OP_STATUS status = dialog->ShowLockingProcesses(file_name, processes);

	if (OpStatus::IsSuccess(status))
	{
		if (m_dialog)
		{
			OpRect rect;
			m_dialog->GetRect(rect);
			m_dialog->Show(FALSE); //Hide progress dialog(previous one)
			dialog->Show(TRUE, &rect, OpWindow::RESTORED, FALSE, TRUE);
		}
		else
		{
			dialog->Show(TRUE);
		}

		OpStatus::Ignore(g_windows_message_loop->Run(static_cast<WindowsOpWindow*>(dialog->GetOpWindow())));

		if (m_dialog)
			m_dialog->Show(TRUE);
	}

	return status;
}

BOOL OperaInstallerUI::ShowError(OperaInstaller::ErrorCode error, const uni_char* item_affected, const uni_char* secondary_error_code, BOOL is_critical)
{
	Pause(TRUE);

	//Get errorcode string from error code
	Str::LocaleString str_id = Str::D_INSTALLER_GENERIC_ERROR;
	UINT32 limit = ARRAY_SIZE(m_error_mapping); 
	for ( UINT32 index = 0 ; index < limit ; index++)
	{
		if (error == m_error_mapping[index].error_code)
		{
			str_id = Str::LocaleString(m_error_mapping[index].str_code);
			break;
		}
	}
	
	//Get error message string
	OpString err_str;
	RETURN_VALUE_IF_ERROR(g_languageManager->GetString(str_id, err_str), FALSE);
	
	//Get error title  string
	OpString title;
	RETURN_VALUE_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_ERROR_TITLE, title), FALSE);
	
	//Get error code string
	OpString err_code_str;
	RETURN_VALUE_IF_ERROR(g_languageManager->GetString(Str::M_LOC_OPERA_ERROR_CODE_FORMAT, err_code_str), FALSE);
	RETURN_VALUE_IF_ERROR(err_str.Append(" "), FALSE);
	RETURN_VALUE_IF_ERROR(err_str.AppendFormat(err_code_str, error), FALSE);

	if (item_affected && (str_id == Str::D_INSTALLER_GENERIC_ERROR || (err_str.FindI("%s") == KNotFound && err_str.FindI("%d") == KNotFound)))
		RETURN_VALUE_IF_ERROR(err_str.Append(" [%s]"), FALSE);

	if (secondary_error_code)
		RETURN_VALUE_IF_ERROR(err_str.AppendFormat(" [%s]", secondary_error_code), FALSE);

	//Add affected item information into the error message.
	OpString formatted_error_msg;
	formatted_error_msg.Empty();
	if (item_affected)
	{
		if (err_str.FindI("%s") != KNotFound)
			RETURN_VALUE_IF_ERROR(formatted_error_msg.AppendFormat(err_str, item_affected), FALSE);
		else if (err_str.FindI("%d") != KNotFound)
			RETURN_VALUE_IF_ERROR(formatted_error_msg.AppendFormat(err_str, uni_atoi(item_affected)), FALSE);
	}
	else
		RETURN_VALUE_IF_ERROR(formatted_error_msg.Set(err_str), FALSE);

	OpFile* error_log;
	if (OpStatus::IsSuccess(m_opera_installer->GetErrorLog(error_log)))
		OpStatus::Ignore(error_log->WriteUTF8Line(formatted_error_msg));

	SimpleDialog* dialog = OP_NEW(SimpleDialog, ());
	if (!dialog)
		return FALSE;

	if (!is_critical)
	{
		dialog->SetOkTextID(Str::D_INSTALLER_IGNORE);
		dialog->SetCancelTextID(Str::DI_IDBTN_CLOSE);
	}
	INT32 result;

	RETURN_VALUE_IF_ERROR(dialog->Init(WINDOW_NAME_INSTALLER_ERROR, title, formatted_error_msg, m_dialog, is_critical?Dialog::TYPE_CLOSE:Dialog::TYPE_OK_CANCEL, is_critical?Dialog::IMAGE_ERROR:Dialog::IMAGE_QUESTION, TRUE, &result), FALSE);

	Pause(FALSE);

	if (is_critical)
		return FALSE;
	else
		return result == Dialog::DIALOG_RESULT_OK;

}

void OperaInstallerUI::ShowAskReboot(BOOL has_rights, BOOL &reboot)
{
	reboot = FALSE;

	SimpleDialog* dialog = OP_NEW(SimpleDialog, ());
	if (dialog)
	{
		OpString title;
		RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_REQUIRES_REBOOT_TITLE, title));

		if (has_rights)
		{
			OpString message;
			RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_REQUIRES_REBOOT, message));

			INT32 result;
			dialog->Init(WINDOW_NAME_INSTALLER_CONFIRM_REBOOT, title, message, m_dialog, Dialog::TYPE_YES_NO, Dialog::IMAGE_QUESTION, TRUE, &result);
			if (result == Dialog::DIALOG_RESULT_YES)
				reboot = TRUE;
		}
		else
		{
			OpString message;
			RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_REQUIRES_REBOOT_MANUAL, message));

			dialog->Init(WINDOW_NAME_INSTALLER_CONFIRM_REBOOT, title, message, m_dialog, Dialog::TYPE_CLOSE, Dialog::IMAGE_QUESTION, TRUE);
		}
	}
}
