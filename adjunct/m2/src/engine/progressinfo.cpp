/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/backend.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2/src/engine/store.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"


/***********************************************************************************
 ** Constructor
 **
 ** ProgressInfo::ProgressInfo
 **
 ***********************************************************************************/
ProgressInfo::ProgressInfo()
	: m_current_action(NONE)
	, m_count(0)
	, m_total_count(0)
	, m_sub_count(0)
	, m_sub_total_count(0)
	, m_connected(FALSE)
	, m_progress_changed(FALSE)
	, m_subprogress_changed(FALSE)
{
	// Initialize messages
	g_main_message_handler->SetCallBack(this, MSG_M2_PROGRESS_CHANGED, (MH_PARAM_1)this);
	g_main_message_handler->SetCallBack(this, MSG_M2_SUBPROGRESS_CHANGED, (MH_PARAM_1)this);
}


/***********************************************************************************
 ** Destructor
 **
 ** ProgressInfo::~ProgressInfo
 **
 ***********************************************************************************/
ProgressInfo::~ProgressInfo()
{
	if (g_opera && g_main_message_handler)
		g_main_message_handler->UnsetCallBacks(this);
}


/***********************************************************************************
 ** Reset all data
 **
 ** ProgressInfo::Reset
 **
 ***********************************************************************************/
void ProgressInfo::Reset(BOOL reset_connected)
{
	BOOL status_changed = m_current_action != NONE || (reset_connected && m_connected);

	// Reset member variables
	m_current_action	= NONE;
	m_count				= 0;
	m_total_count		= 0;
	m_sub_count			= 0;
	m_sub_total_count	= 0;
	if (reset_connected)
		m_connected		= FALSE;

	if (status_changed)
	{
		// Status changed, call listeners
		for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
			m_listeners.GetNext(iterator)->OnStatusChanged(*this);
	}
}


/***********************************************************************************
 ** Set the current action for this progress keeper
 **
 ** ProgressInfo::SetCurrentAction
 ** @param current_action The action to set
 ** @param total_count How many times this action will have to be performed
 **
 ***********************************************************************************/
OP_STATUS ProgressInfo::SetCurrentAction(ProgressAction current_action, unsigned total_count)
{
	BOOL status_changed = FALSE;

	// Check if we have to end an action before setting the new action
	if (m_current_action != NONE && m_current_action != current_action)
		Reset(FALSE);
	else
		status_changed = m_current_action == NONE && current_action != NONE;

	m_current_action	= current_action;
	m_total_count		= total_count;

	AlertListeners();

	if (status_changed)
	{
		for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
			m_listeners.GetNext(iterator)->OnStatusChanged(*this);
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Set the absolute progress of current action
 **
 ** ProgressInfo::SetCurrentActionProgress
 ** @param count How many actions have already been performed
 ** @param total How many actions should be performed in total
 **
 ***********************************************************************************/
void ProgressInfo::SetCurrentActionProgress(unsigned count, unsigned total_count)
{
	m_count = count;
	m_total_count = total_count;

	AlertListeners();
}


/***********************************************************************************
 ** End the current action
 **
 ** ProgressInfo::EndCurrentAction
 ** @param end_connection Ends the connection as well
 **
 ***********************************************************************************/
OP_STATUS ProgressInfo::EndCurrentAction(BOOL end_connection)
{
	Reset(end_connection);

	AlertListeners();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Sets the 'sub progress' for an action (e.g. percentage of a message fetched)
 **
 ** ProgressInfo::SetSubProgress
 ** @param current_count Bits done
 ** @param total_count Total amount of bits to do
 **
 ***********************************************************************************/
void ProgressInfo::SetSubProgress(unsigned current_count, unsigned total_count)
{
	m_sub_count			= current_count;
	m_sub_total_count	= total_count;

	if (!m_subprogress_changed)
	{
		g_main_message_handler->PostMessage(MSG_M2_SUBPROGRESS_CHANGED, (MH_PARAM_1)this, 0, ProgressDelay);
		m_subprogress_changed = TRUE;
	}
}


/***********************************************************************************
 ** Sets whether we are connected to a server
 **
 ** ProgressInfo::SetConnected
 **
 ***********************************************************************************/
void ProgressInfo::SetConnected(BOOL connected)
{
	if (m_connected != connected)
	{
		m_connected = connected;
		AlertListeners();
	}
}


/***********************************************************************************
 ** Get a string description of the current progress status
 **
 ** ProgressInfo::GetProgressString
 ** @param progress_string Where to save the string
 **
 ***********************************************************************************/
OP_STATUS ProgressInfo::GetProgressString(OpString& progress_string) const
{
	progress_string.Empty();

	switch (m_current_action)
	{
		case NONE:
			return OpStatus::OK;
		case CONNECTING:
			return g_languageManager->GetString(Str::DI_IDSTR_M2_STATUS_CONNECTING, progress_string);
		case AUTHENTICATING:
			return g_languageManager->GetString(Str::DI_IDSTR_M2_STATUS_AUTHENTICATING, progress_string);
		case CONTACTING:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_CONTACTING, progress_string);
		case EMPTYING_TRASH:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_EMPTYING_TRASH, progress_string);
		case SENDING_MESSAGES:
			return g_languageManager->GetString(Str::DI_IDSTR_M2_STATUS_SENDING_MESSAGES, progress_string);
		case UPDATING_FEEDS:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_UPDATING_FEEDS, progress_string);
		case CHECKING_FOLDER:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_CHECKING_FOLDER, progress_string);
		case FETCHING_FOLDER_LIST:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_FETCHING_FOLDERS, progress_string);
		case CREATING_FOLDER:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_CREATING_FOLDER, progress_string);
		case DELETING_FOLDER:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_DELETING_FOLDER, progress_string);
		case RENAMING_FOLDER:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_RENAME_FOLDER, progress_string);
		case FOLDER_SUBSCRIBE:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_FOLDER_SUBSCRIPTION, progress_string);
		case FOLDER_UNSUBSCRIBE:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_FOLDER_UNSUBSCRIPTION, progress_string);
		case DELETING_MESSAGE:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_DELETING_MESSAGE, progress_string);
		case APPENDING_MESSAGE:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_APPENDING_MESSAGE, progress_string);
		case COPYING_MESSAGE:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_APPENDING_MESSAGE, progress_string);
		case STORING_FLAG:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_STORING_FLAG, progress_string);
		case SYNCHRONIZING:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_SYNCHRONIZING, progress_string);
		case FETCHING_HEADERS:
			return g_languageManager->GetString(Str::DI_IDSTR_M2_STATUS_FETCHING_HEADERS, progress_string);
		case FETCHING_MESSAGES:
			return g_languageManager->GetString(Str::DI_IDSTR_M2_STATUS_FETCHING_MESSAGES, progress_string);
		case FETCHING_GROUPS:
			return g_languageManager->GetString(Str::DI_IDSTR_M2_STATUS_FETCHING_GROUPS, progress_string);
		case LOADING_DATABASE:
			return g_languageManager->GetString(Str::S_IDSTR_M2_STATUS_LOADING_DATABASE, progress_string);
		default:
			OP_ASSERT(!"All progress actions should be handled here, there's one missing!");
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Merge a progress info, keep 'most important' progress
 **
 ** ProgressInfo::Merge
 ** @param to_merge What to merge
 **
 ***********************************************************************************/
void ProgressInfo::Merge(const ProgressInfo& to_merge)
{
	if (to_merge.m_current_action == m_current_action)
	{
		// If we are looking at the same action, simply increase the counts
		m_count				+= to_merge.m_count;
		m_total_count		+= to_merge.m_total_count;

		// The subcounts are copied over
		if (to_merge.m_sub_total_count > 0)
		{
			m_sub_count 		= to_merge.m_sub_count;
			m_sub_total_count	= to_merge.m_sub_total_count;
		}
	}
	else if (to_merge.m_current_action > m_current_action)
	{
		// This is a 'more important' action; we copy all details
		m_current_action	= to_merge.m_current_action;
		m_count				= to_merge.m_count;
		m_total_count		= to_merge.m_total_count;
		m_sub_count			= to_merge.m_sub_count;
		m_sub_total_count	= to_merge.m_sub_total_count;
	}

	// We are connected as soon as one of the merged progresses is connected
	if (to_merge.m_connected)
		m_connected			= TRUE;
}


/***********************************************************************************
 ** Alert listeners something has changed
 **
 ** ProgressInfo::AlertListeners
 **
 ***********************************************************************************/
void ProgressInfo::AlertListeners()
{
	if (!m_progress_changed)
	{
		g_main_message_handler->PostMessage(MSG_M2_PROGRESS_CHANGED, (MH_PARAM_1)this, 0, ProgressDelay);
		m_progress_changed = TRUE;
	}
}


/***********************************************************************************
 ** Listener, in case this progress keeper listens to other progress updates
 **
 ** ProgressInfo::OnProgressChanged
 **
 ***********************************************************************************/
void ProgressInfo::OnProgressChanged(const ProgressInfo& progress)
{
	AccountManager* account_mgr = MessageEngine::GetInstance()->GetAccountManager();
	if (!account_mgr)
		return;

	Reset(TRUE);

	// Check all accounts, merge progress
	for (unsigned i = 0; i < account_mgr->GetAccountCount(); i++)
	{
		Account* account = account_mgr->GetAccountByIndex(i);
		if (account)
		{
			Merge(account->GetProgress(TRUE));
			Merge(account->GetProgress(FALSE));
		}
	}

	AlertListeners();
}


/***********************************************************************************
 ** Listener, in case this progress keeper listens to other progress updates
 **
 ** ProgressInfo::OnSubProgressChanged
 **
 ***********************************************************************************/
void ProgressInfo::OnSubProgressChanged(const ProgressInfo & progress)
{
	SetSubProgress(progress.GetSubCount(), progress.GetSubTotalCount());
}


/***********************************************************************************
 ** Listener, in case this progress keeper listens to other progress updates
 **
 ** ProgressInfo::OnStatusChanged
 **
 ***********************************************************************************/
void ProgressInfo::OnStatusChanged(const ProgressInfo& progress)
{
	// Status changed, pass on
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnStatusChanged(progress);
}


/***********************************************************************************
 ** Handle incoming messages
 **
 ** ProgressInfo::HandleCallback
 **
 ***********************************************************************************/
void ProgressInfo::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (par1 != (MH_PARAM_1)this)
		return;

	switch (msg)
	{
		case MSG_M2_PROGRESS_CHANGED:
			for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
				m_listeners.GetNext(iterator)->OnProgressChanged(*this);
			m_progress_changed = FALSE;
			break;

		case MSG_M2_SUBPROGRESS_CHANGED:
			for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
				m_listeners.GetNext(iterator)->OnSubProgressChanged(*this);
			m_subprogress_changed = FALSE;
			break;

		default:
			OP_ASSERT(!"Unexpected message received");
	}
}


/***********************************************************************************
 ** Do notifications when new messages have been received
 **
 ** ProgressInfo::NotifyReceived
 **
 ***********************************************************************************/
OP_STATUS ProgressInfo::NotifyReceived(Account* account)
{
	if (!account)
		return OpStatus::ERR_NULL_POINTER;

	// Get the number of new messages received
	Index* account_index = MessageEngine::GetInstance()->GetIndexById(IndexTypes::FIRST_ACCOUNT + account->GetAccountId());
	if (!account_index)
		return OpStatus::ERR;

	unsigned count  = account_index->GetNewMessagesCount();

	// Check if we need a signal even without new messages
	if (account->ResetNeedSignal() && count == 0)
	{
		for (OpListenersIterator it(m_notification_listeners); m_notification_listeners.HasNext(it);)
			m_notification_listeners.GetNext(it)->NeedNoMessagesNotification(account);
	}

	if (count == 0)
		return OpStatus::OK;

	for (OpListenersIterator it(m_notification_listeners); m_notification_listeners.HasNext(it);)
		m_notification_listeners.GetNext(it)->NeedNewMessagesNotification(account, count);

	// Do a sound notification if necessary
	if (account->GetSoundEnabled())
	{
		OpString path;

		RETURN_IF_ERROR(account->GetSoundFile(path));
		for (OpListenersIterator it(m_notification_listeners); m_notification_listeners.HasNext(it);)
			m_notification_listeners.GetNext(it)->NeedSoundNotification(path);
	}

	return OpStatus::OK;
}

void ProgressInfo::OnMailViewActivated(MailDesktopWindow* mail_window, BOOL active)
{
	for (OpListenersIterator it(m_notification_listeners); m_notification_listeners.HasNext(it);)
		m_notification_listeners.GetNext(it)->OnMailViewActivated(mail_window, active);
}

#endif // M2_SUPPORT
