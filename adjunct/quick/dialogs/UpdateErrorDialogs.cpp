/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/quick/dialogs/UpdateErrorDialogs.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"

#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**  Constructor. Initialize Values and some strings.
**
**	UpdateErrorDialog::UpdateErrorDialog
**  @param error
**
***********************************************************************************/
UpdateErrorDialog::UpdateErrorDialog(AutoUpdateError error) :
	m_error(error)
{
	g_languageManager->GetString(Str::D_UPDATE_RESUME, m_resume_text);
	g_languageManager->GetString(Str::D_UPDATE_MANUAL_INSTALL, m_install_manually_text);
}


/***********************************************************************************
**  Destructor
**
**	UpdateErrorDialog::~UpdateErrorDialog
**
***********************************************************************************/
UpdateErrorDialog::~UpdateErrorDialog()
{
}


/***********************************************************************************
**  
**	UpdateErrorDialog::OnInit
**
***********************************************************************************/
void UpdateErrorDialog::OnInit()
{
	OpLabel* error_label = (OpLabel*)GetWidgetByName("update_error_label");
	if (error_label)
	{
		error_label->SetWrap(TRUE);

		OpString error_info, error_type, tmp;

		g_languageManager->GetString(Str::D_UPDATE_ERROR_TEXT, error_info);
		switch (m_error)
		{
		case AUConnectionError:
			{
				g_languageManager->GetString(Str::D_UPDATE_ERROR_TYPE_CONNECTION, error_type);
				break;
			}
		case AUSaveError:
			{
				g_languageManager->GetString(Str::D_UPDATE_ERROR_TYPE_SAVING, error_type);
				break;
			}
		case AUValidationError:
			{
				g_languageManager->GetString(Str::D_UPDATE_ERROR_TYPE_VALIDATION, error_type);
				break;
			}
		case AUInternalError:
		default:
			{
				g_languageManager->GetString(Str::D_UPDATE_ERROR_TYPE_INTERNAL, error_type);
				break;
			}
		}
		tmp.AppendFormat(error_info.CStr(), error_type.CStr());
		tmp.Append(UNI_L("\n"));
		g_languageManager->GetString(Str::D_UPDATE_UNABLE_TO_RESUME, error_info);
		tmp.Append(error_info);

		error_label->SetText(tmp.CStr());
	}
}


/***********************************************************************************
**  
**	UpdateErrorDialog::OnOk
**  @return
**
***********************************************************************************/
UINT32 UpdateErrorDialog::OnOk() // retry
{
	g_autoupdate_manager->DownloadUpdate();
	return 0;
}


/***********************************************************************************
**  
**	UpdateErrorDialog::OnCancel
**
***********************************************************************************/
void UpdateErrorDialog::OnCancel() // manually
{
	g_autoupdate_manager->CancelUpdate();
	
	// Then go to the download page.
	g_autoupdate_manager->GoToDownloadPage();
}


/***********************************************************************************
**  
**	UpdateErrorDialog::OnYNCCancel
**
***********************************************************************************/
void UpdateErrorDialog::OnYNCCancel() // cancel
{
	g_autoupdate_manager->CancelUpdate();
}


/***********************************************************************************
**  Constructor
**
**	UpdateErrorResumingDialog::UpdateErrorResumingDialog
**  @param	error
**  @param	seconds_until_retry
**
***********************************************************************************/
UpdateErrorResumingDialog::UpdateErrorResumingDialog(UpdateErrorContext* context)
{
	OP_ASSERT(context);

	m_context.error = context->error;
	m_context.seconds_until_retry = context->seconds_until_retry;

	g_autoupdate_manager->AddListener(this);

	g_languageManager->GetString(Str::D_UPDATE_DOWNLOAD_INSTALL, m_resume_now_text);
//	SetResumingInfo(context->seconds_until_retry); // TODO: put that in OnInit() or after that
}


/***********************************************************************************
**  Destructor
**  
**	UpdateErrorResumingDialog::~UpdateErrorResumingDialog
**
***********************************************************************************/
UpdateErrorResumingDialog::~UpdateErrorResumingDialog()
{
	g_autoupdate_manager->RemoveListener(this);
}


/***********************************************************************************
**  
**	UpdateErrorResumingDialog::OnInit
**
***********************************************************************************/
void UpdateErrorResumingDialog::OnInit()
{
	OpLabel* error_label = (OpLabel*)GetWidgetByName("update_error_label");
	if (error_label)
	{
		error_label->SetWrap(TRUE);

		OpString error_info, error_type, tmp;

		g_languageManager->GetString(Str::D_UPDATE_ERROR_TEXT, error_info);
		switch (m_context.error)
		{
		case AUConnectionError:
			{
				g_languageManager->GetString(Str::D_UPDATE_ERROR_TYPE_CONNECTION, error_type);
				break;
			}
		case AUSaveError:
			{
				g_languageManager->GetString(Str::D_UPDATE_ERROR_TYPE_SAVING, error_type);
				break;
			}
		case AUValidationError:
			{
				g_languageManager->GetString(Str::D_UPDATE_ERROR_TYPE_VALIDATION, error_type);
				break;
			}
		case AUInternalError:
		default:
			{
				g_languageManager->GetString(Str::D_UPDATE_ERROR_TYPE_INTERNAL, error_type);
				break;
			}
		}
		tmp.AppendFormat(error_info.CStr(), error_type.CStr());

		error_label->SetText(tmp.CStr());
	}
}


/***********************************************************************************
**  
**	UpdateErrorResumingDialog::OnOk
**  @return
**
***********************************************************************************/
UINT32 UpdateErrorResumingDialog::OnOk()
{
	g_autoupdate_manager->DownloadUpdate();
	return 0;
}


/***********************************************************************************
**  
**	UpdateErrorResumingDialog::OnCancel
**
***********************************************************************************/
void UpdateErrorResumingDialog::OnCancel()
{
	g_autoupdate_manager->CancelUpdate();
}


/***********************************************************************************
**  
**	UpdateErrorResumingDialog::SetResumingInfo
**  @param seconds_until_retry
**
***********************************************************************************/
void UpdateErrorResumingDialog::SetResumingInfo(INT32 seconds_until_retry)
{
	OpLabel* resuming_label = (OpLabel*)GetWidgetByName("update_resuming_label");
	if (resuming_label)
	{
		if (seconds_until_retry > 0)
		{
			OpString resuming_str, tmp_str;
			g_languageManager->GetString(Str::D_UPDATE_RESUMING_SECONDS, tmp_str);
			resuming_str.AppendFormat(tmp_str.CStr(), seconds_until_retry);

			resuming_label->SetText(resuming_str);
		}
		else
		{
			resuming_label->SetText(UNI_L(""));
		}
	}
}

#endif // AUTO_UPDATE_SUPPORT
