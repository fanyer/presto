/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "InstallPersonabar.h"

#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/quick/widgets/OpInfobar.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/locale/src/opprefsfilelanguagemanager.h"
#include "modules/util/filefun.h"
#include "modules/skin/OpSkinManager.h"
#include "adjunct/desktop_util/string/i18n.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/quick/widgets/OpStatusField.h"

#define INSTALL_SKINS_DELAYED_HIDE		1000 * 15	// The toolbar will remain open for 15 seconds before hiding automatically

/***********************************************************************************
 **
 **	InstallPersonaBar
 **
 ***********************************************************************************/

InstallPersonaBar::InstallPersonaBar(PrefsCollectionUI::integerpref prefs_setting, PrefsCollectionUI::integerpref autoalignment_prefs_setting)
	: OpInfobar(prefs_setting, autoalignment_prefs_setting)
#ifdef PERSONA_SKIN_SUPPORT
	, m_old_persona_file(NULL)
#endif // PERSONA_SKIN_SUPPORT
	, m_old_skin_file(NULL)
	, m_transfer(NULL)
	, m_url(NULL)
	, m_skin_type(OpSkinManager::SkinUnknown)
{
	m_delayed_trigger.SetDelayedTriggerListener(this);
	m_delayed_trigger.SetTriggerDelay(INSTALL_SKINS_DELAYED_HIDE);
}

InstallPersonaBar::~InstallPersonaBar()
{
	if(m_transfer && g_transferManager)
		g_transferManager->ReleaseTransferItem(m_transfer);
#ifdef PERSONA_SKIN_SUPPORT
	OP_DELETE(m_old_persona_file);
#endif // PERSONA_SKIN_SUPPORT
	OP_DELETE(m_old_skin_file);
}

/* static */
OP_STATUS InstallPersonaBar::Construct(InstallPersonaBar** obj)
{
	OpAutoPtr<InstallPersonaBar> new_bar (OP_NEW(InstallPersonaBar, ()));
	RETURN_OOM_IF_NULL(new_bar.get());
	RETURN_IF_ERROR(new_bar->init_status);
	RETURN_IF_ERROR(new_bar->Init("Install Skin Toolbar"));

	new_bar->GetBorderSkin()->SetImage("Install Skin Toolbar Skin");

	*obj = new_bar.release();
	return OpStatus::OK;
}

void InstallPersonaBar::OnTrigger(OpDelayedTrigger* trigger)
{
	OP_ASSERT(trigger == &m_delayed_trigger);

	SetAlignment(ALIGNMENT_OFF);
}

void InstallPersonaBar::UndoSkinInstall()
{
#ifdef PERSONA_SKIN_SUPPORT
	if (!g_pcfiles->GetFile(PrefsCollectionFiles::PersonaFile)->GetSerializedName() || !m_old_persona_file->GetSerializedName())
	{
		// if NULL, we didn't have a previous file so just set it back to NULL
		g_skin_manager->SelectPersonaByFile(m_old_persona_file);
	}
	else 
	if(uni_strcmp(g_pcfiles->GetFile(PrefsCollectionFiles::PersonaFile)->GetSerializedName(),
		m_old_persona_file->GetSerializedName()) != 0)
	{
		g_skin_manager->SelectPersonaByFile(m_old_persona_file);
	}
#endif // PERSONA_SKIN_SUPPORT
	if (uni_strcmp(g_pcfiles->GetFile(PrefsCollectionFiles::ButtonSet)->GetSerializedName(),
		m_old_skin_file->GetSerializedName()) != 0)
	{
		g_skin_manager->SelectSkinByFile(m_old_skin_file);
	}
	// Delete the downloaded file.
	if (m_setupfilename.HasContent())
	{
		OpFile file;
		file.Construct(m_setupfilename.CStr());
		file.Delete();
	}
}

BOOL InstallPersonaBar::OnInputAction(OpInputAction* action)
{
	if (OpInfobar::OnInputAction(action))
		return TRUE;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_UNDO_SKIN_INSTALL:
				{
					child_action->SetEnabled(m_skin_type != OpSkinManager::SkinUnknown);
					return TRUE;
				}
				break;
				
				default:
					break;
			}
			break;
		}
		case OpInputAction::ACTION_CANCEL:
			{
				SetAlignment(ALIGNMENT_OFF);
				return TRUE;
			}
			break;

		case OpInputAction::ACTION_UNDO_SKIN_INSTALL:
			{
				UndoSkinInstall();
				SetAlignment(ALIGNMENT_OFF);
				return TRUE;
			}
			break;

		default:
			break;
	}
	return FALSE;
}

OP_STATUS InstallPersonaBar::SetURLInfo(const OpStringC & full_url, URLContentType content_type)
{
	RETURN_IF_ERROR(m_urlstring.Set(full_url));
	switch(content_type)
	{
	case URL_SKIN_CONFIGURATION_CONTENT:
		m_setuptype = OPSKIN_SETUP;		// might change after we've sniffed the file
		break;
	default:
		OP_ASSERT(!"Unsupported filetype");
		break;
	}
	return StartDownloading();
}

void InstallPersonaBar::UpdateVisibility(BOOL show_progress)
{
	OpProgressBar *progress = static_cast<OpProgressBar *>(GetWidgetByName(GetProgressFieldName()));
	if(progress)
	{
		progress->SetType(OpProgressBar::Percentage_Label);
		progress->SetHidden(!show_progress);
		progress->SetGrowValue(2);
	}
	OpStatusField* label = static_cast<OpStatusField*> (GetWidgetByName(GetStatusFieldName()));
	if(label && show_progress)
	{
		OpString text;

		g_languageManager->GetString(Str::S_DOWNLOADING_SKIN, text);
		label->SetText(text.CStr());
	}
}

void InstallPersonaBar::UpdateProgress(OpFileLength len, OpFileLength total)
{
	OpProgressBar* progress = static_cast<OpProgressBar *>(GetWidgetByName(GetProgressFieldName()));
	if(progress)
	{
		progress->SetProgress(len, total);
	}
}

OP_STATUS InstallPersonaBar::StartDownloading()
{
	OP_STATUS status = OpStatus::OK;
	BOOL already_transferring = FALSE;

	SetAlignmentAnimated(ALIGNMENT_OLD_VISIBLE);

	UpdateVisibility(TRUE);

	// Need to store the current state so that we can fallback to it if the setup is rejected by the user.
	if(m_setuptype == OPSKIN_SETUP)
	{
#ifdef PERSONA_SKIN_SUPPORT
		m_old_persona_file = OP_NEW(OpFile, ());
		if (m_old_persona_file)
		{
			RETURN_IF_LEAVE(g_pcfiles->GetFileL(PrefsCollectionFiles::PersonaFile, *m_old_persona_file));
		}
#endif // PERSONA_SKIN_SUPPORT
		m_old_skin_file = OP_NEW(OpFile, ());
		if (m_old_skin_file)
		{
			RETURN_IF_LEAVE(g_pcfiles->GetFileL(PrefsCollectionFiles::ButtonSet, *m_old_skin_file));
		}
	}
	if(m_transfer)	// redirect causes a new download
	{
		g_transferManager->ReleaseTransferItem(m_transfer);
	}

	OpTransferItem * transfer;
	RETURN_IF_ERROR(g_transferManager->GetTransferItem(&transfer, m_urlstring.CStr(), &already_transferring));

	already_transferring = already_transferring && (transfer->GetHaveSize() != 0);

	if(transfer && !already_transferring)
	{
		m_url = transfer->GetURL();

		MessageHandler *oldHandler = m_url->GetFirstMessageHandler();

		m_url->ChangeMessageHandler(oldHandler ? oldHandler : g_main_message_handler, g_main_message_handler);

		URL tmp;
		m_url->Reload(g_main_message_handler, tmp, FALSE, TRUE);

		transfer->SetTransferListener(this);

	}
	return status;
}

// copy and paste from SetupApplyDialog::SaveDownload():
OP_STATUS InstallPersonaBar::SaveDownload(OpStringC directory, OpString& setupfilename)
{
	// Retrieve suggested file name
	OpString suggested_filename;

	RETURN_IF_LEAVE(m_url->GetAttributeL(URL::KSuggestedFileName_L, suggested_filename, TRUE));

	if (suggested_filename.IsEmpty())
	{
		RETURN_IF_ERROR(suggested_filename.Set("default"));

		OpString ext;
		RETURN_IF_LEAVE(m_url->GetAttributeL(URL::KSuggestedFileNameExtension_L, ext, TRUE));
		if (ext.HasContent())
		{
			RETURN_IF_ERROR(suggested_filename.Append("."));
			RETURN_IF_ERROR(suggested_filename.Append(ext));
		}
		if (suggested_filename.IsEmpty())
		{
			return OpBoolean::IS_FALSE;
		}
	}

	// Build unique file name
	OpString filename;
	RETURN_IF_ERROR(filename.Set(directory));
	if( filename.HasContent() && filename[filename.Length()-1] != PATHSEPCHAR )
	{
		RETURN_IF_ERROR(filename.Append(PATHSEP));
	}

	RETURN_IF_ERROR(filename.Append(suggested_filename.CStr()));

	BOOL exists = FALSE;

	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename.CStr()));
	RETURN_IF_ERROR(file.Exists(exists));

	if (exists)
	{
		RETURN_IF_ERROR(CreateUniqueFilename(filename));
	}

	// Save file to disk
	URL* url = m_transfer->GetURL();
	RETURN_VALUE_IF_NULL(url, OpStatus::ERR);

	RETURN_IF_ERROR(url->LoadToFile(filename.CStr()));

	return setupfilename.Set(filename);
}

void InstallPersonaBar::RegularSetup()
{
	// do setup:
	OpString tmp_storage;
	OpStatus::Ignore(SaveDownload(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SKIN_FOLDER, tmp_storage), m_setupfilename));

	if(m_setupfilename.HasContent())
	{
		OpFile skinfile;
		RETURN_VOID_IF_ERROR(skinfile.Construct(m_setupfilename.CStr()));

		m_skin_type = g_skin_manager->GetSkinFileType(m_setupfilename);
		if(m_skin_type == OpSkinManager::SkinNormal)
		{
			OpFile dummy_persona;
			if (OpStatus::IsError(g_skin_manager->SelectSkinByFile(&skinfile)))
			{
				m_skin_type = OpSkinManager::SkinUnknown;
			}
			else 
			{
				// attempt to install an empty persona as personas should not apply 
				// when installing third party skins
				OpStatus::Ignore(g_skin_manager->SelectPersonaByFile(&dummy_persona));
			}
		}
#ifdef PERSONA_SKIN_SUPPORT
		else if(m_skin_type == OpSkinManager::SkinPersona)
		{
			if (OpStatus::IsError(g_skin_manager->SelectPersonaByFile(&skinfile)))
			{
				m_skin_type = OpSkinManager::SkinUnknown;
			}
		}
#endif // PERSONA_SKIN_SUPPORT
		g_transferManager->ReleaseTransferItem(m_transfer);

		// extra verification
		if((m_skin_type == OpSkinManager::SkinNormal && !g_skin_manager->GetCurrentSkin()) 
#ifdef PERSONA_SKIN_SUPPORT
			|| (m_skin_type == OpSkinManager::SkinPersona && !g_skin_manager->GetPersona())
#endif // PERSONA_SKIN_SUPPORT
			)
			m_skin_type = OpSkinManager::SkinUnknown;

		OpString text;

		// handle the skin error here
		if(m_skin_type == OpSkinManager::SkinUnknown)
		{
			g_languageManager->GetString(Str::S_SKIN_DOWNLOAD_FAILED, text);

			// skin was broken, roll it back
			UndoSkinInstall();
		}
		else
		{
#ifdef PERSONA_SKIN_SUPPORT
			OpStringC name = m_skin_type == OpSkinManager::SkinPersona && g_skin_manager->GetPersona() ? g_skin_manager->GetPersona()->GetSkinName() : g_skin_manager->GetCurrentSkin()->GetSkinName();
			OpStringC author = m_skin_type == OpSkinManager::SkinPersona && g_skin_manager->GetPersona() ? g_skin_manager->GetPersona()->GetSkinAuthor() : g_skin_manager->GetCurrentSkin()->GetSkinAuthor();
#else
			OpStringC name = g_skin_manager->GetCurrentSkin()->GetSkinName();
			OpStringC author = g_skin_manager->GetCurrentSkin()->GetSkinAuthor();
#endif // PERSONA_SKIN_SUPPORT

			I18n::Format(text, Str::S_SKIN_INSTALLED, name, author);
		}
		SetStatusText(text);
	}
	// start trigger to hide it again after some time
	m_delayed_trigger.InvokeTrigger(OpDelayedTrigger::INVOKE_REDELAY);
}

void InstallPersonaBar::AbortSetup()
{
	if(m_transfer)
	{
		g_transferManager->ReleaseTransferItem(m_transfer);
	}
	SetAlignmentAnimated(ALIGNMENT_OFF);
}

void InstallPersonaBar::OnProgress(OpTransferItem* transferItem, TransferStatus status)
{
	if(status == OpTransferListener::TRANSFER_DONE)
	{
		UpdateVisibility(FALSE);

		if(transferItem)
		{
			UpdateProgress(transferItem->GetSize(), transferItem->GetSize());
		}
		if(IsAThreat(transferItem))
		{
			URLInformation *urlinfo = transferItem->CreateURLInformation();

			OP_ASSERT(urlinfo);
			if(urlinfo)
			{
				INT32 result = 0;

				const uni_char* server_name = urlinfo->ServerUniName();
				OpString8 name;

				name.Set(server_name);

				SetupApplyDialog_ConfirmDialog* dialog = OP_NEW(SetupApplyDialog_ConfirmDialog, ());

				if (dialog)
				{
					dialog->Init(this, GetParentDesktopWindow(), name.CStr(), &result);
				}
				OP_DELETE(urlinfo);
				return;
			}
		}
		RegularSetup();
	}
	else if(status == OpTransferListener::TRANSFER_PROGRESS)
	{
		UpdateProgress(transferItem->GetHaveSize(), transferItem->GetSize());
	}
	else if(status == OpTransferListener::TRANSFER_FAILED || status == OpTransferListener::TRANSFER_ABORTED)
	{
		m_skin_type = OpSkinManager::SkinUnknown;

		OpString failed;
		g_languageManager->GetString(Str::S_SKIN_DOWNLOAD_FAILED, failed);

		SetStatusText(failed);

		AbortSetup();
	}
}

void InstallPersonaBar::OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to)
{

}

void InstallPersonaBar::SetTransferItem(OpTransferItem * transferItem)
{
	m_transfer = transferItem;
}

void InstallPersonaBar::OnSetupCancelled()
{
	AbortSetup();
}

void InstallPersonaBar::OnSetupAccepted()
{
	RegularSetup();
}

// Check if the content could contain security risks
// 1. Check if the content came from opera.com
BOOL InstallPersonaBar::IsAThreat(OpTransferItem* transferItem)
{
	OpAutoPtr<URLInformation> urlinfo(transferItem->CreateURLInformation());

	OP_ASSERT(urlinfo.get());
	if(urlinfo.get())
	{
		const uni_char* server_name = urlinfo->ServerUniName();

		int servername_len = server_name ? uni_strlen(server_name) : 0;
		if(servername_len >= 9)
		{
			const uni_char* t = server_name + (uni_strlen(server_name)-9);
			//check if the servername _ends_ with opera.com
			return uni_stricmp(t, "opera.com") != 0;
		}
	}
	return TRUE;
}
