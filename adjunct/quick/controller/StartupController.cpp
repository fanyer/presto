/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/controller/StartupController.h"

#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "adjunct/quick/models/ExtensionsModel.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/QuickCheckBox.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickRadioButton.h"
#include "adjunct/quick_toolkit/widgets/QuickTreeView.h"

#include "modules/gadgets/OpGadgetManager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/prefs/prefsmanager/collections/pc_opera_account.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"


StartupController::StartupController():
	m_widgets(0),
	m_restart_gadgets(true),
	m_restart_gadgets_enabled(true)
{
}

void StartupController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Startup Dialog"));
	m_widgets = m_dialog->GetWidgetCollection();
	LEAVE_IF_ERROR(InitOptions());
	LEAVE_IF_ERROR(InitTreeView());
}

OP_STATUS StartupController::InitOptions()
{
	//Preference binders for Radiobutton widgets.
	RETURN_IF_ERROR(GetBinder()->Connect("Continue_lasttime_radio", *g_pcui,PrefsCollectionUI::WindowRecoveryStrategy, Restore_AutoSavedWindows));
	RETURN_IF_ERROR(GetBinder()->Connect("Continue_session_radio", *g_pcui, PrefsCollectionUI::WindowRecoveryStrategy, Restore_RegularStartup));
	RETURN_IF_ERROR(GetBinder()->Connect("Start_homepage_radio", *g_pcui, PrefsCollectionUI::WindowRecoveryStrategy, Restore_Homepage));
	RETURN_IF_ERROR(GetBinder()->Connect("Start_nowindows_radio", *g_pcui,PrefsCollectionUI::WindowRecoveryStrategy, Restore_NoWindows));

	BOOL permanent_homepage = FALSE;
#ifdef PERMANENT_HOMEPAGE_SUPPORT
	permanent_homepage = (1 == g_pcui->GetIntegerPref(PrefsCollectionUI::PermanentHomepage));
#endif
	if ((!permanent_homepage) && (!g_pcui->GetIntegerPref(PrefsCollectionUI::Running)))
	{
		RETURN_IF_ERROR(GetBinder()->Connect("Continue_session_radio", *g_pcui, PrefsCollectionUI::StartupType, STARTUP_HISTORY));
		RETURN_IF_ERROR(GetBinder()->Connect("Start_homepage_radio", *g_pcui, PrefsCollectionUI::StartupType, STARTUP_HOME));
		RETURN_IF_ERROR(GetBinder()->Connect("Start_nowindows_radio", *g_pcui,PrefsCollectionUI::StartupType, STARTUP_NOWIN));
		RETURN_IF_ERROR(GetBinder()->Connect("Continue_lasttime_radio", *g_pcui,PrefsCollectionUI::StartupType, STARTUP_CONTINUE));
	}

	//Binding Checkbox in the button-strip
	RETURN_IF_ERROR(GetBinder()->Connect("checkbox_Default", m_dont_show_again));
	if (g_pcui->GetIntegerPref(PrefsCollectionUI::Running))
		m_widgets->Get<QuickWidget>("checkbox_Default")->Hide();

	//Binding the enable state of Continue_lasttime_radio, Continue_session_radio
	RETURN_IF_ERROR(GetBinder()->EnableIf("Continue_lasttime_radio", m_have_autosave));
	RETURN_IF_ERROR(GetBinder()->EnableIf("Continue_session_radio", m_have_sessions));

	//Binding the enable state and the value of restart_unite_checkbox
	RETURN_IF_ERROR(GetBinder()->Connect("restart_gadgets_checkbox", m_restart_gadgets));
	RETURN_IF_ERROR(GetBinder()->EnableIf("restart_gadgets_checkbox", m_restart_gadgets_enabled));

#ifdef WEBSERVER_SUPPORT
	if (g_pcui->GetIntegerPref(PrefsCollectionUI::EnableUnite) != 0)
	{
		QuickCheckBox* restart_gadgets_checkbox = m_widgets->Get<QuickCheckBox>("restart_gadgets_checkbox");
		RETURN_IF_ERROR(restart_gadgets_checkbox->SetText(Str::D_WEBSERVER_RESTART_SERVICES));
	}
#endif // WEBSERVER_SUPPORT

	return OpStatus::OK;
}

OP_STATUS StartupController::InitTreeView()
{
	STARTUPTYPE startupType =
		STARTUPTYPE(g_pcui->GetIntegerPref(PrefsCollectionUI::StartupType));

	PrefsFile* defaultsessionfile = NULL;
	OpSession* OP_MEMORY_VAR tempdefsession = NULL;
	OpFile winstore;
	TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::WindowsStorageFile, winstore));
	if (OpStatus::IsSuccess(rc))
	{
		defaultsessionfile = OP_NEW(PrefsFile, (PREFS_INI));
		if (defaultsessionfile)
		{
			TRAPD(err,defaultsessionfile->ConstructL());
			TRAP(err,defaultsessionfile->SetFileL(&winstore));
			tempdefsession = OP_NEW(OpSession, ());
			if (tempdefsession)
				TRAP(err,tempdefsession->InitL(defaultsessionfile));
			else
				OP_DELETE(defaultsessionfile);
		}
	}

	if (g_pcui->GetIntegerPref(PrefsCollectionUI::Running) &&
		g_pcui->GetIntegerPref(PrefsCollectionUI::ShowProblemDlg))
	{
		startupType = STARTUP_CONTINUE;
	}
	OP_MEMORY_VAR OP_STATUS err;
	TRAP(err, g_session_manager->ScanSessionFolderL());
	OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ? Fails when unable to create folder.

	OP_MEMORY_VAR INT32 defaultpos = -1;
	for (OP_MEMORY_VAR UINT32 i = 0; i < g_session_manager->GetSessionCount(); i++)
	{
		OpStringC name;
		TRAP(err, name = g_session_manager->GetSessionNameL(i));
		INT32 sort_order = 1;

		OpString auto_saved;
		g_languageManager->GetString(Str::S_PREVIOUS_SESSION_AUTOSAVED, auto_saved);

		if (name.CompareI(UNI_L("autosave"))== 0)
		{
			m_have_autosave.Set(true);

			// if autosave is the only one there, don't list it

			if (g_session_manager->GetSessionCount() == 1)
			{
				break;
			}
			else
			{
				name = auto_saved;
				sort_order = 0;
			}
		}

		m_have_sessions.Set(true);

		m_sessions_model.AddItem(name.CStr(), "Session", sort_order);

		if(tempdefsession)
		{
			OpSession* session;
			TRAPD(err, session = g_session_manager->ReadSessionL(i));
			if (OpStatus::IsSuccess(err))
			{
				if(*session == (const OpSession*)tempdefsession)
				{
					defaultpos = i;
				}

				OP_DELETE(session);
			}
		}
	}

	OP_DELETE(tempdefsession);
	tempdefsession=NULL;

	if (!m_have_sessions.Get())
	{
		if (startupType == STARTUP_HISTORY)
			startupType = STARTUP_CONTINUE;
	}
	else
	{
		OpTreeView* sessions = m_widgets->Get<QuickTreeView>("Sessions_treeview")->GetOpWidget();

		sessions->SetCheckableColumn(0);
		sessions->SetShowColumnHeaders(FALSE);
		sessions->SetTreeModel(&m_sessions_model);
		sessions->Sort(0, TRUE);
		if( defaultpos != -1 )
		{
			sessions->ToggleItem(defaultpos);
			sessions->SetSelectedItem(defaultpos);
		}
		else
		{
			sessions->SetSelectedItem(0);
		}
		sessions->StopPendingOnChange();

	}

	if (!m_have_autosave.Get())
	{
		if (startupType == STARTUP_CONTINUE)
			startupType = STARTUP_HOME;
	}

	switch (startupType)
	{
	    case STARTUP_CONTINUE:
			m_widgets->Get<QuickRadioButton>("Continue_lasttime_radio")->GetOpWidget()->SetValue(true); break;
	    case STARTUP_HISTORY:
			m_widgets->Get<QuickRadioButton>("Continue_session_radio")->GetOpWidget()->SetValue(true); break;
	    case STARTUP_HOME:
		 	m_widgets->Get<QuickRadioButton>("Start_homepage_radio")->GetOpWidget()->SetValue(true); break;
	    case STARTUP_NOWIN:
			m_widgets->Get<QuickRadioButton>("Start_nowindows_radio")->GetOpWidget()->SetValue(true); break;
	}

	if (!g_pcui->GetIntegerPref(PrefsCollectionUI::AllowEmptyWorkspace))
		RETURN_IF_ERROR(SetWidgetText<QuickRadioButton>("Start_nowindows_radio", Str::S_START_BLANK_PAGE));

	BOOL extensions_startup_requested = FALSE;
	for (UINT i = 0; i < g_gadget_manager->NumGadgets(); ++i)
	{
		OpGadget* gadget = g_gadget_manager->GetGadget(i);
		OP_ASSERT(gadget);
		OpGadgetClass* gadget_class = gadget->GetClass();
		OP_ASSERT(gadget_class);

		if (gadget_class->IsExtension())
		{
			if (gadget->GetPersistentData(GADGET_ENABLE_ON_STARTUP) != NULL)
			{
				extensions_startup_requested  = TRUE;
				break;
			}
		}
	}

	BOOL unite_startup_requested = FALSE;
	if(g_pcui->GetIntegerPref(PrefsCollectionUI::EnableUnite))
	{
		if (g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverEnable)
#ifdef PREFS_CAP_ACCOUNT_SAVE_PASSWORD
			&& g_pcopera_account->GetIntegerPref(PrefsCollectionOperaAccount::SavePassword)
#endif // PREFS_CAP_ACCOUNT_SAVE_PASSWORD
			)
			{
				unite_startup_requested = true;
			}
	}

	if (extensions_startup_requested || unite_startup_requested)
	{
		m_restart_gadgets.Set(true);
		m_restart_gadgets_enabled.Set(true);
	}
	else
	{
		m_restart_gadgets.Set(false);
		m_restart_gadgets_enabled.Set(false);
	}

	return OpStatus::OK;
}

void StartupController::OnOk()
{
	QuickTreeView* treeview = m_widgets->Get<QuickTreeView>("Sessions_treeview");
	OpTreeView* sessions = treeview->GetOpWidget();
	OpINT32Vector* checked_sessions;
	checked_sessions = OP_NEW(OpINT32Vector, ());

	if (checked_sessions)
	{
		for (INT32 i = 0; i < m_sessions_model.GetItemCount(); i++)
		{
			if (sessions->IsItemChecked(i))
			{
				INT32 new_int = sessions->GetModelPos(i);
				if (new_int)
					checked_sessions->Add(new_int);
			}
		}
	}

	OP_STATUS err;
	if(g_pcui->GetIntegerPref(PrefsCollectionUI::Running))
	{
		TRAP(err,g_session_manager->BackupAutosaveSessionL());
	}

	if (!g_pcui->GetIntegerPref(PrefsCollectionUI::Running) ||
		!g_pcui->GetIntegerPref(PrefsCollectionUI::ShowProblemDlg))
	{
		TRAP(err,g_pcui->WriteIntegerL(PrefsCollectionUI::ShowStartupDialog, !m_dont_show_again.Get()));
	}

#ifdef WEBSERVER_SUPPORT
	TRAP(err,g_pcui->WriteIntegerL(PrefsCollectionUI::RestartUniteServicesAfterCrash, m_restart_gadgets.Get()));
	if (!m_restart_gadgets.Get())
		TRAP(err, g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::WebserverEnable, m_restart_gadgets.Get()));
#endif // WEBSERVER_SUPPORT

	TRAP(err, g_prefsManager->CommitL());
	g_main_message_handler->PostMessage(MSG_QUICK_APPLICATION_START_CONTINUE, (MH_PARAM_1)checked_sessions, 0);

	return OkCancelDialogContext::OnOk();
}

void StartupController::OnCancel()
{
	g_desktop_global_application->Exit();
}
