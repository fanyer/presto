/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#ifdef _DEBUG
# include <assert.h>
# define ALWAYS_CREATE_LOG
#endif // _DEBUG

#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/desktop_util/prefs/PrefsUtils.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/modules.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/MessageDatabase/MessageDatabase.h"
#include "adjunct/m2/src/util/misc.h"
#include "adjunct/m2/src/util/qp.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslopt.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_m2.h"
#include "modules/util/opfile/unistream.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/wand/wandmanager.h"
#include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"
#include "modules/windowcommander/WritingSystem.h"

// Backends
#include "adjunct/m2/src/backend/archive/ArchiveBackend.h"
#include "adjunct/m2/src/backend/imap/imapmodule.h"
#ifdef IRC_SUPPORT
#include "adjunct/m2/src/backend/irc/irc-module.h"
#endif
#ifdef JABBER_SUPPORT
#include "adjunct/m2/src/backend/jabber/jabber-backend.h"
#endif
#include "adjunct/m2/src/backend/nntp/nntpmodule.h"
#include "adjunct/m2/src/backend/pop/popmodule.h"
#include "adjunct/m2/src/backend/rss/rssmodule.h"
#include "adjunct/m2/src/backend/smtp/smtpmodule.h"

Account::Account(MessageDatabase& database)
  : MessageBackend(database),
  m_account_id(0),
#ifdef _DEBUG
  m_is_initiated(FALSE),
#endif
  m_incoming_interface(NULL),
  m_outgoing_interface(NULL),
  m_is_temporary(FALSE),
  m_save_incoming_password(FALSE),
  m_save_outgoing_password(FALSE),
  m_need_signal(FALSE),
  m_nick_in_flux(FALSE),
  m_incoming_portnumber(0),
  m_outgoing_portnumber(0),
  m_use_secure_connection_in(FALSE),
  m_use_secure_connection_out(FALSE),
  m_use_secure_authentication(FALSE),
  m_incoming_authentication_method(AccountTypes::AUTOSELECT),
  m_outgoing_authentication_method(AccountTypes::NONE),
  m_current_incoming_auth_method(AccountTypes::AUTOSELECT), //Not saved.
  m_current_outgoing_auth_method(AccountTypes::AUTOSELECT), //Not saved.
  m_incoming_timeout(0),
  m_outgoing_timeout(0),
  m_delayed_remove_from_server(FALSE),
  m_remove_from_server_delay(604800),
  m_remove_from_server_only_if_read(FALSE),
  m_remove_from_server_only_if_complete_message(TRUE),
  m_fetch_timer(NULL),
  m_initial_poll_delay(0),
  m_poll_interval(0),
  m_max_downloadsize(0),
  m_purge_age(-1),
  m_purge_size(-1),
  m_download_bodies(FALSE),
  m_download_attachments(FALSE),
  m_leave_on_server(TRUE),
  m_server_has_spam_filter(FALSE),
  m_imap_bodystructure(TRUE),
  m_imap_idle(TRUE),
  m_permanent_delete(TRUE),
  m_queue_outgoing(FALSE),
  m_send_after_checking(FALSE),
  m_read_if_seen(FALSE),
  m_sound_enabled(FALSE),
  m_manual_check_enabled(1),
  m_add_contact_when_sending(TRUE),
  m_warn_if_empty_subject(TRUE),
  m_HTML_signature(FALSE),
  m_low_bandwidth(FALSE),
  m_fetch_max_lines(100),
  m_fetch_only_text(FALSE),
  m_force_charset(FALSE),
  m_allow_8bit_header(FALSE),
  m_allow_8bit_transfer(FALSE),
  m_allow_incoming_quotedstring_qp(TRUE),
  m_prefer_HTML_composing(Message::PREFER_TEXT_PLAIN_BUT_REPLY_WITH_SAME_FORMATTING),
  m_default_direction(AccountTypes::DIR_AUTOMATIC),
  m_linelength(76),
  m_remove_signature_on_reply(TRUE),
  m_max_quotedepth_on_reply(-1),
  m_max_quotedepth_on_quickreply(2),
  m_incoming_logfile_ptr(NULL),
  m_outgoing_logfile_ptr(NULL),
  m_offline_log(*this),
  m_sent_messages(0),
  m_default_store(g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailStoreType)),
  m_askpasswordcontext(NULL),
  m_force_single_connection(FALSE),
  m_qresync(FALSE)
{
}

Account::~Account()
{
	// Write offline log
	if (m_incoming_interface)
		OpStatus::Ignore(m_incoming_interface->WriteToOfflineLog(m_offline_log));

	OP_DELETE(m_fetch_timer); m_fetch_timer = NULL;

	if (m_incoming_interface == m_outgoing_interface)
		m_outgoing_interface = NULL;

	OP_DELETE(m_incoming_interface); m_incoming_interface = NULL;
	OP_DELETE(m_outgoing_interface); m_outgoing_interface = NULL;

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	if (m_incoming_logfile_ptr) //Should be done after deleting incoming interface
	{
		m_incoming_logfile_ptr->Close();
		glue_factory->DeleteOpFile(m_incoming_logfile_ptr);
		m_incoming_logfile_ptr = NULL;
	}

	if (m_outgoing_logfile_ptr) //Should be done after deleting outgoing interface
	{
		m_outgoing_logfile_ptr->Close();
		glue_factory->DeleteOpFile(m_outgoing_logfile_ptr);
		m_outgoing_logfile_ptr = NULL;
	}
}

OP_STATUS Account::ReadAccountSettings(PrefsFile * prefs_file, const char * ini_key, BOOL import/*=FALSE*/)
{
	OpString8 protocol;
	BOOL      keep_bodies;

	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Incoming Protocol", protocol));
	m_incoming_protocol = GetProtocolFromName(protocol);

	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Outgoing Protocol", protocol));
	m_outgoing_protocol = GetProtocolFromName(protocol);

    AccountTypes::AuthenticationType default_authentication = AccountTypes::AUTOSELECT;
	if ((m_incoming_protocol == AccountTypes::NEWS || m_incoming_protocol == AccountTypes::IRC) &&
		m_incoming_username.IsEmpty()) //Default to NONE for NNTP and IRC, except for when username and password is given
    {
        default_authentication = AccountTypes::NONE;
    }

	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Incoming Authentication Method", default_authentication, m_incoming_authentication_method));

    default_authentication = AccountTypes::NONE; //Default to NONE for outgoing backends, except for when username and password is given
	if (m_outgoing_username.HasContent())
	{
		default_authentication = AccountTypes::AUTOSELECT;
	}
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Outgoing Authentication Method", default_authentication, m_outgoing_authentication_method));

    RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Charset", m_charset));
    RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Personalization", m_personalization));
	m_personalization.Strip();
    RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Xface", m_xface));

#ifdef ALWAYS_CREATE_LOG
	OpString incoming_maillog_path, outgoing_maillog_path;

	OpString tmp_storage;
	const uni_char * mail_folder_path = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_MAIL_FOLDER, tmp_storage);
	RETURN_IF_ERROR(incoming_maillog_path.AppendFormat(UNI_L("%s%caccount%d-incoming.log"),
					mail_folder_path,
					PATHSEPCHAR,
					m_account_id));

	RETURN_IF_ERROR(outgoing_maillog_path.AppendFormat(UNI_L("%s%caccount%d-outgoing.log"),
					mail_folder_path,
					PATHSEPCHAR,
					m_account_id));
#endif

	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Temporary", 0, m_is_temporary));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Incoming Port", 0, m_incoming_portnumber));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Outgoing Port", 0, m_outgoing_portnumber));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Secure Connection In", FALSE, m_use_secure_connection_in));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Secure Connection Out", FALSE, m_use_secure_connection_out));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Secure Authentication", FALSE, m_use_secure_authentication));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Incoming Timeout", 60, m_incoming_timeout));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Outgoing Timeout", 90, m_outgoing_timeout));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Remove From Server Delay Enabled", FALSE, m_delayed_remove_from_server));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Remove From Server Delay", 604800, m_remove_from_server_delay));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Remove From Server Only If Marked As Read", FALSE, m_remove_from_server_only_if_read));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Remove From Server Only If Complete Message", TRUE, m_remove_from_server_only_if_complete_message));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Initial Poll Delay", 5, m_initial_poll_delay));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Poll Interval", 0, m_poll_interval));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Max Download Size", 0, m_max_downloadsize));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Purge Age", -1, m_purge_age));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Purge Size", -1, m_purge_size));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Download Bodies", FALSE, m_download_bodies));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Keep Bodies", TRUE, keep_bodies));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Download Attachments", TRUE, m_download_attachments));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Leave On Server", FALSE, m_leave_on_server));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Server Has Spam Filter", FALSE, m_server_has_spam_filter));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "IMAP BODYSTRUCTURE support", TRUE, m_imap_bodystructure));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "IMAP IDLE support", TRUE, m_imap_idle));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Permanent Delete", TRUE, m_permanent_delete));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Queue Outgoing", FALSE, m_queue_outgoing));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Send Queued After Check", FALSE, m_send_after_checking));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Mark Read If Seen", FALSE, m_read_if_seen));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Sound Enabled", TRUE, m_sound_enabled));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Manual Check Enabled", TRUE, m_manual_check_enabled));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Add Contact When Sending", TRUE, m_add_contact_when_sending));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Show Warning For Empty Subject", TRUE, m_warn_if_empty_subject));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Force Charset", FALSE, m_force_charset));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Allow 8Bit Headers", FALSE, m_allow_8bit_header));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Allow 8Bit Transfer", TRUE, m_allow_8bit_transfer));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Allow Incoming QuotedString QP", TRUE, m_allow_incoming_quotedstring_qp));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Prefer HTML Composing", Message::PREFER_TEXT_PLAIN_BUT_REPLY_WITH_SAME_FORMATTING, m_prefer_HTML_composing));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Linelength", 76, m_linelength));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Remove Signature On Reply", TRUE, m_remove_signature_on_reply));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Max QuoteDepth On Reply", -1, m_max_quotedepth_on_reply));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Max QuoteDepth On QuickReply", 2, m_max_quotedepth_on_quickreply));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Signature is HTML", FALSE, m_HTML_signature));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Low bandwidth mode", FALSE, m_low_bandwidth));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Fetch Max Lines", 100, m_fetch_max_lines));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Fetch Only Text", FALSE, m_fetch_only_text));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Default Direction", AccountTypes::DIR_AUTOMATIC, m_default_direction));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Default Store", keep_bodies ? g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailStoreType) : 0, m_default_store));

#ifdef ALWAYS_CREATE_LOG
	if (import)
	{
		RETURN_IF_ERROR(m_incoming_logfile.Set(incoming_maillog_path));
		RETURN_IF_ERROR(m_outgoing_logfile.Set(outgoing_maillog_path));
	}
	else
	{
		RETURN_IF_ERROR(PrefsUtils::ReadFolderPath(prefs_file,  ini_key, "Incoming Log File", m_incoming_logfile, incoming_maillog_path, OPFILE_SERIALIZED_FOLDER));
		RETURN_IF_ERROR(PrefsUtils::ReadFolderPath(prefs_file,  ini_key, "Outgoing Log File", m_outgoing_logfile, outgoing_maillog_path, OPFILE_SERIALIZED_FOLDER));
	}
#else
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Incoming Log File", m_incoming_logfile));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Outgoing Log File", m_outgoing_logfile));
#endif
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Category", m_account_category));
	if (!import)
	{
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Account Name", m_accountname));
		if (m_accountname.IsEmpty())
			m_accountname.Set(ini_key);
	}
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Incoming Servername", m_incoming_servername));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Incoming Username", m_incoming_username));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Outgoing Servername", m_outgoing_servername));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Outgoing Username", m_outgoing_username));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Real Name", m_realname));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Email", m_email));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Replyto", m_replyto));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Organization", m_organization));
	RETURN_IF_ERROR(PrefsUtils::ReadFolderPath(prefs_file,  ini_key, "Signature File", m_signature_file, UNI_L(""), OPFILE_SERIALIZED_FOLDER));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Fqdn", m_fqdn));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Sound File", m_sound_file));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Reply", m_reply_string));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Followup", m_followup_string));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Forward", m_forward_string));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Auto CC", m_auto_cc));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Auto BCC", m_auto_bcc));

	OpStatus::Ignore(m_original_fqdn.Set(m_fqdn));
    OpStatus::Ignore(OpMisc::StripWhitespaceAndQuotes(m_fqdn));
	m_incoming_servername.Strip();
	m_outgoing_servername.Strip();
	m_email.Strip();
	m_replyto.Strip();
	m_auto_cc.Strip();
	m_auto_bcc.Strip();

	{
		// The following four irc properties were previously in accounts.ini, so
		// we want to read their settings if they are present, but they are saved
		// by the backend now.
		UINT start_dcc_pool = 0;
		UINT end_dcc_pool = 0;
		BOOL private_chats_in_background = FALSE;
		OpString perform_when_connected;

		RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "DCC Portpool start", 1024, start_dcc_pool));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "DCC Portpool end", 1040, end_dcc_pool));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(prefs_file, ini_key, "Open private chat windows in background", TRUE, private_chats_in_background));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, ini_key, "Perform when connected", perform_when_connected));

		SetStartOfDccPool(start_dcc_pool);
		SetEndOfDccPool(end_dcc_pool);
		SetOpenPrivateChatsInBackground(private_chats_in_background);
		SetPerformWhenConnected(perform_when_connected);
	}

#if defined(M2_MERLIN_COMPATIBILITY) || defined(M2_KESTREL_BETA_COMPATIBILITY)
	// Merlin and Kestrel Beta allowed you to uncheck 'download bodies' for POP, a very dangerous thing
	// since this works with POP in Kestrel! See #336164
	if (m_incoming_protocol == AccountTypes::POP)
		m_download_bodies = TRUE;
#endif // M2_MERLIN_COMPATIBILITY || M2_KESTREL_BETA_COMPATIBILITY

	return OpStatus::OK;
}

OP_STATUS Account::Init(UINT16 account_id, OpString8& status)
{
    AccountManager* account_manager = (AccountManager*)MessageEngine::GetInstance()->GetAccountManager();
    if (!account_manager)
        return OpStatus::ERR_NULL_POINTER;

    if (account_id == 0)
	{
#ifdef _DEBUG
			m_is_initiated = TRUE;
#endif
			RETURN_IF_ERROR(SaveSettings(TRUE));
	}
	else
	{
		m_account_id = account_id;
	}

    PrefsFile* prefs_file = account_manager->GetPrefsFile();
    if (!prefs_file)
        return OpStatus::ERR_NULL_POINTER;

    char ini_key[13];
	op_sprintf(ini_key, "Account%u", m_account_id);

	RETURN_IF_ERROR(ReadAccountSettings(prefs_file, ini_key));

#ifdef M2_MERLIN_COMPATIBILITY
	// Update old-style passwords in accounts.ini to new-style passwords in Wand
	OpStatus::Ignore(ReadPasswordsFromAccounts(prefs_file, ini_key));
#endif // M2_MERLIN_COMPATIBILITY

#ifdef _DEBUG
	m_is_initiated = TRUE;
#endif

	// if file doesn't exist, recreate path to allow for easy moving of mail folders
	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	if (m_signature_file.HasContent())
	{
		OpFile* file = glue_factory->CreateOpFile(m_signature_file);
		if (!file)
			return OpStatus::ERR_NO_MEMORY;

		BOOL file_exists;
		if (OpStatus::IsSuccess(file->Exists(file_exists)) && !file_exists)
			m_signature_file.Empty();
		glue_factory->DeleteOpFile(file);
	}

	if (m_signature_file.IsEmpty())
    {
		OpString defaultsignature;

		RETURN_IF_ERROR(CreateSignatureFileName());

		// maybe the signature file is in the normal location, let's check before writing the default one
		OpFile* file = glue_factory->CreateOpFile(m_signature_file);
		if (!file)
			return OpStatus::ERR_NO_MEMORY;

		BOOL file_exists;
		OP_STATUS err = OpStatus::OK;
		if (OpStatus::IsSuccess(file->Exists(file_exists)) && !file_exists)
		{
			err = g_languageManager->GetString(Str::S_M2_DEFAULT_SIGNATURE, defaultsignature);
			if (OpStatus::IsSuccess(err))
				err = SetSignature(defaultsignature.CStr(), FALSE);
		}
		glue_factory->DeleteOpFile(file);
		RETURN_IF_ERROR(err);
    }

	// Regenerate file name every time to allow for dual boot or copying of Mail folder
	if (m_incoming_optionsfile.IsEmpty())
		RETURN_IF_ERROR(CreateIncomingOptionsFileName());

	// Regenerate file name every time to allow for dual boot or copying of Mail folder
	if (m_outgoing_optionsfile.IsEmpty())
		RETURN_IF_ERROR(CreateOutgoingOptionsFileName());

/*
//    PGP*      m_pgp_key;
*/
	if (GetMessageDatabase().HasFinishedLoading() && OpStatus::IsError(InitBackends()))
	{
		status.Append("Couldn't initialize backends\n");
		return OpStatus::ERR;
	}

	if (m_incoming_protocol == AccountTypes::IMAP)
	{
		// IMAP should by default sychronize changes.
		m_read_if_seen = TRUE;
	}
	else if (m_incoming_protocol == AccountTypes::IRC && !m_incoming_interface)
	{	
		// Create the IRC backend here. Otherwise the list of rooms will not be populated on startup (ref. DSK-276224)
		if (OpStatus::IsError(InitBackends()))
		{
			status.Append("Couldn't initialize backends\n");
			return OpStatus::ERR;
		}
	}

	//Make sure storage type is compatible with what backend supports
	if (m_incoming_interface && !m_incoming_interface->SupportsStorageType((AccountTypes::MboxType)m_default_store))
		m_default_store = g_pcm2->GetDefaultIntegerPref(PrefsCollectionM2::DefaultMailStoreType);

	//Make sure methods are compatible with what backend supports
	SetIncomingAuthenticationMethod(m_incoming_authentication_method);
	SetOutgoingAuthenticationMethod(m_outgoing_authentication_method);
	OnAccountAdded();

    return OpStatus::OK;
}

OP_STATUS Account::StartAutomaticFetchTimer(BOOL startup)
{
	//Start incoming poll timer
	if (m_poll_interval>0)
	{
		if (!m_fetch_timer)
		{
			m_fetch_timer = OP_NEW(OpTimer, ());
			if (!m_fetch_timer)
				return OpStatus::ERR_NO_MEMORY;

			m_fetch_timer->SetTimerListener(this);
		}

		m_fetch_timer->Start((startup && m_initial_poll_delay>0 ? m_initial_poll_delay : m_poll_interval) * 1000);
	}
	else if (m_fetch_timer)
	{
		OP_DELETE(m_fetch_timer);
		m_fetch_timer = NULL;
	}

	return OpStatus::OK;
}

OP_STATUS Account::PrepareToDie()
{
	if (m_fetch_timer)
		m_fetch_timer->Stop();

    // Let incoming backend clean up whatever it has created
    if (m_incoming_interface)
        m_incoming_interface->PrepareToDie();

    //Let outgoing backend clean up whatever it has created
    if (m_outgoing_interface)
        m_outgoing_interface->PrepareToDie();

    //Remove incoming options file
	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
    OpFile* file = glue_factory->CreateOpFile(m_incoming_optionsfile);
    if (file)
    {
        file->Delete();
    }
	glue_factory->DeleteOpFile(file);

    //Remove outgoing options file
    file = glue_factory->CreateOpFile(m_outgoing_optionsfile);
    if (file)
    {
        file->Delete();
    }
	glue_factory->DeleteOpFile(file);

	AccountManager* account_manager = (AccountManager*)MessageEngine::GetInstance()->GetAccountManager();

    //Remove signature if the account is temporary or if the signature is only used by this account
	if (m_is_temporary || (account_manager && account_manager->GetSignatureCount(m_signature_file) == 1))
    {
        file = glue_factory->CreateOpFile(m_signature_file);
        if (file)
        {
            file->Delete();
        }
		glue_factory->DeleteOpFile(file);
    }

    return OpStatus::OK;
}

OP_STATUS Account::SaveSettings(BOOL force_flush)
{
#ifdef _DEBUG
	OP_ASSERT(m_is_initiated); //NEVER save settings before they are loaded!
	if (!m_is_initiated)
		return OpStatus::ERR;
#endif

    AccountManager* account_manager = (AccountManager*)MessageEngine::GetInstance()->GetAccountManager();
    if (!account_manager)
        return OpStatus::ERR_NULL_POINTER;

    if (m_account_id == 0)
        RETURN_IF_ERROR(account_manager->AddAccount(this, TRUE));

    PrefsFile* prefs_file = account_manager->GetPrefsFile();
    if (!prefs_file)
        return OpStatus::ERR_NULL_POINTER;

	OpString8 ini_key;
	RETURN_IF_ERROR(ini_key.AppendFormat("Account%u", m_account_id));

	RETURN_IF_LEAVE(prefs_file->ClearSectionL(ini_key.CStr()));

	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Incoming Protocol", GetProtocolName(m_incoming_protocol)));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Outgoing Protocol", GetProtocolName(m_outgoing_protocol)));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Charset", m_charset));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Personalization", m_personalization));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Xface", m_xface));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Temporary", m_is_temporary));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Category", m_account_category));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Incoming Port", m_incoming_portnumber));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Outgoing Port", m_outgoing_portnumber));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Secure Connection In", m_use_secure_connection_in));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Secure Connection Out", m_use_secure_connection_out));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Secure Authentication", m_use_secure_authentication));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Incoming Authentication Method", m_incoming_authentication_method));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Outgoing Authentication Method", m_outgoing_authentication_method));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Incoming Timeout", m_incoming_timeout));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Outgoing Timeout", m_outgoing_timeout));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Remove From Server Delay Enabled", m_delayed_remove_from_server));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Remove From Server Delay", m_remove_from_server_delay));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Remove From Server Only If Marked As Read", m_remove_from_server_only_if_read));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Remove From Server Only If Complete Message", m_remove_from_server_only_if_complete_message));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Initial Poll Delay", m_initial_poll_delay));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Poll Interval", m_poll_interval));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Max Download Size", m_max_downloadsize));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Purge Age", m_purge_age));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Purge Size", m_purge_size));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Download Bodies", m_download_bodies));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Download Attachments", m_download_attachments));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Leave On Server", m_leave_on_server));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Server Has Spam Filter", m_server_has_spam_filter));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "IMAP BODYSTRUCTURE support", m_imap_bodystructure));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "IMAP IDLE support", m_imap_idle));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Permanent Delete", m_permanent_delete));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Queue Outgoing", m_queue_outgoing));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Send Queued After Check", m_send_after_checking));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Mark Read If Seen", m_read_if_seen));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Sound Enabled", m_sound_enabled));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Manual Check Enabled", m_manual_check_enabled));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Add Contact When Sending", m_add_contact_when_sending));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Show Warning For Empty Subject", m_warn_if_empty_subject));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Account Name", m_accountname));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Incoming Servername", m_incoming_servername));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Incoming Username", m_incoming_username));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Outgoing Servername", m_outgoing_servername));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Outgoing Username", m_outgoing_username));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Force Charset", m_force_charset));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Allow 8Bit Headers", m_allow_8bit_header));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Allow 8Bit Transfer", m_allow_8bit_transfer));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Allow Incoming QuotedString QP", m_allow_incoming_quotedstring_qp));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Prefer HTML Composing", m_prefer_HTML_composing));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Real Name", m_realname));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Email", m_email));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Replyto", m_replyto));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Organization", m_organization));
	if (m_signature_file.HasContent())
		RETURN_IF_ERROR(PrefsUtils::WritePrefsFileFolder(prefs_file, ini_key, "Signature File", m_signature_file, OPFILE_SERIALIZED_FOLDER));
	if (m_original_fqdn.HasContent())
		RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Fqdn", m_original_fqdn));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Sound File", m_sound_file));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Reply", m_reply_string));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Followup", m_followup_string));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Forward", m_forward_string));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Linelength", m_linelength));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Remove Signature On Reply", m_remove_signature_on_reply));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Max QuoteDepth On Reply", m_max_quotedepth_on_reply));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Max QuoteDepth On QuickReply", m_max_quotedepth_on_quickreply));
	if (m_incoming_logfile.HasContent())
		RETURN_IF_ERROR(PrefsUtils::WritePrefsFileFolder(prefs_file, ini_key, "Incoming Log File", m_incoming_logfile, OPFILE_SERIALIZED_FOLDER));
	if (m_outgoing_logfile.HasContent())
		RETURN_IF_ERROR(PrefsUtils::WritePrefsFileFolder(prefs_file, ini_key, "Outgoing Log File", m_outgoing_logfile, OPFILE_SERIALIZED_FOLDER));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Auto CC", m_auto_cc));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(prefs_file, ini_key, "Auto BCC", m_auto_bcc));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Signature is HTML", m_HTML_signature));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Low bandwidth mode", m_low_bandwidth));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Fetch Max Lines", m_fetch_max_lines));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Fetch Only Text", m_fetch_only_text));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Default Direction", m_default_direction));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(prefs_file, ini_key, "Default Store", m_default_store));

/*
//    PGP*      m_pgp_key;
*/
	if(m_incoming_interface)
	{
		RETURN_IF_ERROR(m_incoming_interface->WriteSettings());
	}
	if(m_outgoing_interface)
	{
		RETURN_IF_ERROR(m_outgoing_interface->WriteSettings());
	}
    if (force_flush)
    {
        RETURN_IF_LEAVE(prefs_file->CommitL());
    }

	return OpStatus::OK;
}

OP_STATUS Account::SetDefaults()
{
    OP_STATUS ret;
    m_incoming_servername.Empty();
    m_incoming_username.Empty();
    m_outgoing_servername.Empty();
    m_outgoing_username.Empty();
    m_charset.Empty();
    m_realname.Empty();
    m_email.Empty();
    m_replyto.Empty();
    m_organization.Empty();
    m_xface.Empty();
    m_signature_file.Empty();
    m_personalization.Empty();
    m_sound_file.Empty();
    m_incoming_optionsfile.Empty();
    m_outgoing_optionsfile.Empty();

	m_prefer_HTML_composing=Message::PREFER_TEXT_PLAIN_BUT_REPLY_WITH_SAME_FORMATTING;

	switch (m_incoming_protocol)
	{
		case AccountTypes::POP:
		{
			m_incoming_portnumber=110;
			m_use_secure_connection_in=FALSE;
			m_use_secure_authentication=FALSE;
			m_incoming_authentication_method = AccountTypes::AUTOSELECT;
			m_incoming_timeout=60;
			m_allow_8bit_transfer=FALSE;
			m_initial_poll_delay=5;
			m_poll_interval=300;
			m_max_downloadsize=0;
			m_download_bodies=TRUE;
			m_download_attachments=TRUE;

			OpString languagestring;
			m_followup_string.Empty();

			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_M2_MAIL_REPLY_STR, languagestring));
			RETURN_IF_ERROR(m_reply_string.Set(languagestring));
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_M2_MAIL_FORWARD_STR, languagestring));
			RETURN_IF_ERROR(m_forward_string.Set(languagestring));

			m_leave_on_server=TRUE;
			break;
		}
		case AccountTypes::IMAP:
		{
			m_incoming_portnumber=143;
			m_use_secure_connection_in=FALSE;
			m_use_secure_authentication=FALSE;
			m_incoming_authentication_method = AccountTypes::AUTOSELECT;
			m_incoming_timeout=60;
			m_allow_8bit_transfer=FALSE;
			m_initial_poll_delay=5;
			m_poll_interval=600;
			m_max_downloadsize=0;
			m_download_bodies=FALSE;
			m_download_attachments=TRUE;

			OpString languagestring;

			m_followup_string.Empty();
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_M2_MAIL_REPLY_STR, languagestring));
			RETURN_IF_ERROR(m_reply_string.Set(languagestring));
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_M2_MAIL_FORWARD_STR, languagestring));
			RETURN_IF_ERROR(m_forward_string.Set(languagestring));

			m_leave_on_server=FALSE;
			m_imap_bodystructure = TRUE;
			m_imap_idle = TRUE;
			break;
		}
		case AccountTypes::NEWS:
		{
			m_incoming_portnumber=119;
			m_use_secure_connection_in=FALSE;
			m_use_secure_authentication=FALSE;
			m_incoming_authentication_method = AccountTypes::NONE;
			m_incoming_timeout=30;
			m_allow_8bit_transfer=FALSE;
			m_initial_poll_delay=0;
			m_poll_interval=0;
			m_max_downloadsize=0;
			m_download_bodies=FALSE;
			m_download_attachments=TRUE;
			m_leave_on_server=TRUE;
			m_manual_check_enabled=FALSE;
			m_prefer_HTML_composing=Message::PREFER_TEXT_PLAIN;
			OpString languagestring;

			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_M2_NEWS_REPLY_STR, languagestring));
			RETURN_IF_ERROR(m_reply_string.Set(languagestring));
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_M2_NEWS_FOLLOWUP_STR, languagestring));
			RETURN_IF_ERROR(m_followup_string.Set(languagestring));
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_M2_NEWS_FORWARD_STR, languagestring));
			RETURN_IF_ERROR(m_forward_string.Set(languagestring));
			break;
		}
		case AccountTypes::RSS:
			m_initial_poll_delay=5;
        	m_poll_interval=300;
			break;
#ifdef IRC_SUPPORT
		case AccountTypes::IRC:
        	m_incoming_portnumber=6667;
			break;
#endif
#ifdef JABBER_SUPPORT
		case AccountTypes::JABBER:
			m_incoming_portnumber=5222;
			break;
#endif // JABBER_SUPPORT
	}

	if (m_outgoing_protocol == AccountTypes::SMTP)
	{
		m_outgoing_portnumber=25;
		m_use_secure_connection_out=FALSE;
		m_outgoing_authentication_method = AccountTypes::NONE;
		m_outgoing_timeout=90;
		m_allow_8bit_transfer=FALSE;
		m_queue_outgoing=FALSE;
		m_send_after_checking=FALSE;
	}

	m_purge_age=-1;
	m_purge_size=-1;
	m_linelength=76;
	m_remove_signature_on_reply=TRUE;
	m_max_quotedepth_on_reply=-1;
	m_max_quotedepth_on_quickreply=2;
	m_force_charset=FALSE;
	m_allow_8bit_header=FALSE;
	m_default_direction = AccountTypes::DIR_AUTOMATIC;

	/* Set up the default encoding for the account, which we base on the
	 * user preferred language, or, if we have no special settings for this
	 * language, the system encoding. For IRC, we always use UTF-8. */
#ifdef IRC_SUPPORT
	if (m_incoming_protocol == AccountTypes::IRC)
	{
		ret = m_charset.Set("utf-8");
	}
	else
#endif
	{
		char userlanguage[8] = {0}; /* ARRAY OK 2012-11-07 peter */ // "ll-Vvvv\0"
		const char* acceptlanguage = g_pcnet->GetAcceptLanguage();
		size_t len = op_strcspn(acceptlanguage, ",;");
		op_strlcpy(userlanguage, acceptlanguage, MIN(len + 1, ARRAY_SIZE(userlanguage)));
		WritingSystem::Script userscript = WritingSystem::FromLanguageCode(userlanguage);

		const char* encoding;
		RETURN_IF_LEAVE(encoding = g_op_system_info->GetSystemEncodingL());

		if (userscript == WritingSystem::Unknown)
		{
			userscript = WritingSystem::FromEncoding(encoding);
		}

		/* Select encoding based on language, see DSK-240864, DSK-250211, DSK-365303 */
		switch (userscript)
		{
		case WritingSystem::Japanese:
			ret = m_charset.Set("iso-2022-jp");
			break;
		case WritingSystem::ChineseSimplified:
			ret = m_charset.Set("gbk");
			break;
		case WritingSystem::ChineseTraditional:
			ret = m_charset.Set("big5");
			break;
		case WritingSystem::Korean:
			ret = m_charset.Set("euc-kr");
			break;
		case WritingSystem::LatinEastern: /* Polish, Hungarian, et.al */
			ret = m_charset.Set("utf-8");
			break;
		default:
			/* We could not determine the language, use the system encoding;
			 * in case of a windows encoding, translate to a more standard
			 * encoding; some of these should be caught by the FromEncoding()
			 * clause above, but keep them here for completeness.
			 */
			unsigned codepage;
			if (op_sscanf(encoding, "windows-%u", &codepage) == 1)
			{
				switch (codepage)
				{
				case 874:  ret = m_charset.Set("iso-8859-11"); break;
				case 932:  ret = m_charset.Set("iso-2022-jp"); break;
				case 936:  ret = m_charset.Set("gbk"); break;
				case 949:  ret = m_charset.Set("euc-kr"); break;
				case 950:  ret = m_charset.Set("big5"); break;
				case 1250: ret = m_charset.Set("iso-8859-2"); break;
				case 1251: ret = m_charset.Set("koi8-r"); break;
				case 1252: ret = m_charset.Set("iso-8859-15"); break;
				case 1253: ret = m_charset.Set("iso-8859-7"); break;
				case 1254: ret = m_charset.Set("iso-8859-9"); break;
				case 1255: ret = m_charset.Set("iso-8859-8"); break;
				case 1256: ret = m_charset.Set("iso-8859-6"); break;
				case 1257: ret = m_charset.Set("iso-8859-13"); break;
				default:   ret = m_charset.Set("utf-8"); break;
				}
			}
			else
			{
				ret = m_charset.Set(encoding);
			}
		}
	}

    return ret;
}

OP_STATUS Account::SettingsChanged(BOOL startup)
{
	if (m_incoming_interface)
		RETURN_IF_ERROR(m_incoming_interface->SettingsChanged(startup));

    if (m_outgoing_interface)
		RETURN_IF_ERROR(m_outgoing_interface->SettingsChanged(startup));

	return OpStatus::OK;
}


OP_STATUS Account::FetchMessage(message_index_t index, BOOL user_request, BOOL force_complete)
{
    if (MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->OfflineMode())
        return OpStatus::OK;

    if (!m_incoming_interface)
        return OpStatus::ERR_NULL_POINTER;

    return m_incoming_interface->FetchMessage(index, user_request, force_complete);
}

OP_STATUS Account::FetchMessages(BOOL enable_signalling)
{
    if (MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->OfflineMode())
        return OpStatus::OK;

    if (!m_incoming_interface)
        return OpStatus::ERR_NULL_POINTER;

	if(m_fetch_timer)
	{
		m_fetch_timer->Stop(); //Don't want timed fetch if user just pressed manual fetch
	}

    OP_STATUS ret = m_incoming_interface->FetchMessages(enable_signalling);

	//In case this is a manual fetch, timer should be reset
	StartAutomaticFetchTimer();

	m_need_signal |= enable_signalling;

	return ret;
}

OP_STATUS Account::SelectFolder(UINT32 index_id)
{
    if (!m_incoming_interface)
        return OpStatus::ERR_NULL_POINTER;

    return m_incoming_interface->SelectFolder(index_id);
}

OP_STATUS Account::RefreshFolder(UINT32 index_id)
{
    if (!m_incoming_interface)
        return OpStatus::ERR_NULL_POINTER;

    return m_incoming_interface->RefreshFolder(index_id);
}

OP_STATUS Account::RefreshAll()
{
    if (!m_incoming_interface)
        return OpStatus::ERR_NULL_POINTER;

    return m_incoming_interface->RefreshAll();
}

OP_STATUS Account::StopFetchingMessages()
{
    if (!m_incoming_interface)
        return OpStatus::ERR_NULL_POINTER;

    return m_incoming_interface->StopFetchingMessages();
}

OP_STATUS Account::KeywordAdded(message_gid_t message_id, const OpStringC8& keyword)
{
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return m_offline_log.KeywordAdded(message_id, keyword);
	else if (m_incoming_interface)
		return m_incoming_interface->KeywordAdded(message_id, keyword);

	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS Account::KeywordRemoved(message_gid_t message_id, const OpStringC8& keyword)
{
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return m_offline_log.KeywordRemoved(message_id, keyword);
	else if (m_incoming_interface)
		return m_incoming_interface->KeywordRemoved(message_id, keyword);

	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS Account::CloseAllConnections()
{
	// Close connection for both incoming and outgoing interfaces
	if (m_incoming_interface)
		RETURN_IF_ERROR(m_incoming_interface->Disconnect());
	if (m_outgoing_interface)
		RETURN_IF_ERROR(m_outgoing_interface->Disconnect());

	return OpStatus::OK;
}

OP_STATUS Account::StopSendingMessage()
{
    if (!m_outgoing_interface)
        return OpStatus::OK; //Nothing to do

    return m_outgoing_interface->StopSendingMessage();
}

OP_STATUS Account::AddOperaGeneratedMail(const uni_char* filename, Str::LocaleString subject_string)
{
	// Create the message.
	OpAutoPtr<Message> new_message(OP_NEW(Message, ()));
	if (new_message.get() == 0)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_message->Init(m_account_id));
	
	OpString from, subject;
	OpString8 mbs_file_content;
	OpFile welcome_mail_file;
	OpFileLength filelength;
	// Read from the mbs file in the package
	RETURN_IF_ERROR(welcome_mail_file.Construct(filename, OPFILE_STYLE_FOLDER));
	RETURN_IF_ERROR(welcome_mail_file.Open(OPFILE_READ));
	RETURN_IF_ERROR(welcome_mail_file.GetFileLength(filelength));
	mbs_file_content.Reserve(filelength);
	mbs_file_content.CStr()[filelength] = 0;
	OpFileLength bytes_read = 0;
	RETURN_IF_ERROR(welcome_mail_file.Read(mbs_file_content.CStr(), filelength, &bytes_read));

	if (g_languageManager->GetWritingDirection() == OpLanguageManager::RTL)
	{
		int body_tag = mbs_file_content.Find("<body>");
		if (body_tag != KNotFound)
		{
			RETURN_IF_ERROR(mbs_file_content.Insert(body_tag + 5, " dir=\"rtl\""));
		}
	}

	// Set the content of the file as the raw message content
	RETURN_IF_ERROR(new_message->SetRawMessage(mbs_file_content.CStr(), TRUE));

	// Set localized From and Subject headers
	g_languageManager->GetString(Str::S_WELCOME_TO_M2_FROM, from);
	g_languageManager->GetString(subject_string, subject);
	RETURN_IF_ERROR(new_message->SetFrom(from));
	RETURN_IF_ERROR(new_message->SetHeaderValue(Header::SUBJECT, subject));
	new_message->SetConfirmedNotSpam(true);

	// Add the welcome message to the account
	RETURN_IF_ERROR(Fetched(*new_message.get(), FALSE, FALSE));

	return OpStatus::OK;
}

void Account::OnAccountAdded()
{
	MessageEngine::GetInstance()->OnAccountAdded(m_account_id);

	if (m_incoming_interface)
		m_incoming_interface->OnAccountAdded();
	if (m_outgoing_interface)
		m_outgoing_interface->OnAccountAdded();
}


/***********************************************************************************
 ** Copies a message and inserts it into this account as if new
 **
 ** Account::InternalCopyMessage
 ** @param message Message to copy
 ***********************************************************************************/
OP_STATUS Account::InternalCopyMessage(Message& message)
{
	// Check for body
	BOOL has_body;
	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->HasMessageDownloadedBody(message.GetId(), has_body));

	if (has_body)
		RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->GetMessageData(message));

	// Set correct account
	message.SetAccountId(GetAccountId(), FALSE);

	// Keep only copyable flags
	message.SetAllFlags(message.GetAllFlags() & Message::CopyableFlags);

	// Insert the message into the system as a new message
	message.SetId(0);

	return Fetched(message, !has_body, FALSE);
}


OP_STATUS Account::UpdateStore(int old_version)
{
	OP_STATUS ret = OpStatus::OK;

	if(m_incoming_interface)
	{
		RETURN_IF_ERROR(m_incoming_interface->UpdateStore(old_version));
	}
	if(m_outgoing_interface)
	{
		RETURN_IF_ERROR(m_outgoing_interface->UpdateStore(old_version));
	}

	return ret;
}

OP_STATUS Account::SendMessages()
{
	Index* outbox =  GetMessageDatabase().GetIndex(IndexTypes::OUTBOX);
	Index* trash =   GetMessageDatabase().GetIndex(IndexTypes::TRASH);

	if (!outbox || !trash)
		return OpStatus::ERR;

	for (INT32SetIterator it(outbox->GetIterator()); it; it++)
	{
		message_gid_t id = it.GetData();

		// We send messages for this account that have not been deleted
		if (!trash->Contains(id))
		{
			Message message;

			if (OpStatus::IsSuccess(GetMessageDatabase().GetMessage(id, FALSE, message)) && message.GetAccountId() == m_account_id)
				SendMessage(message, FALSE);
		}
	}

	return OpStatus::OK;
}

OP_STATUS Account::PrepareToSendMessage(message_gid_t id, BOOL anonymous, Message& message)
{
	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->GetMessage(message, id));
	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->GetMessageData(message));

	if(message.GetAccountId() != GetAccountId())
		return OpStatus::ERR;

    OP_STATUS ret;
    BOOL is_resent = message.IsFlagSet(Message::IS_RESENT);
    if (!is_resent)
    {
        //Set organization-header (not available in resend)
		RETURN_IF_ERROR(message.SetHeaderValue(Header::ORGANIZATION, m_organization));

        //Set xface-header (not available in resend)
		RETURN_IF_ERROR(message.SetHeaderValue(Header::XFACE, m_xface));
    }

    //Set date-header
    RETURN_IF_ERROR(message.SetDate(0)); // 0 = ::Now

    //Check from-header
	Header::HeaderValue tmp_from;
	if ( OpStatus::IsError(ret=message.GetFrom(tmp_from, FALSE)) || tmp_from.IsEmpty() )
	{
		OP_ASSERT(0); //This should never happen!
        return ret;
	}

    //Set messageID-header, if it isn't already set
	Header::HeaderType messageid_type = message.IsFlagSet(Message::IS_RESENT) ? Header::RESENTMESSAGEID : Header::MESSAGEID;
    Header* messageid = message.GetHeader(messageid_type);
	if (!messageid || messageid->IsEmpty())
	{
		RETURN_IF_ERROR(message.SetHeaderValue(messageid_type, UNI_L(""), FALSE/*No need to set this to TRUE for an empty string*/));
		Header* messageid = message.GetHeader(messageid_type);
		if (!messageid)
			return OpStatus::ERR_NULL_POINTER;
		RETURN_IF_ERROR(messageid->GenerateNewMessageId());
	}

	//Set In-Reply-To header
	RETURN_IF_ERROR(message.GenerateInReplyToHeader());

    if (!is_resent) //Don't add Useragent-header for resent messages
    {
        OpString8 user_agent;
		RETURN_IF_ERROR(MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->GetUserAgent(user_agent));
		RETURN_IF_ERROR(message.SetHeaderValue(Header::USERAGENT, user_agent));
    }

	//Is this a usenet message?
	BOOL is_newsmessage = FALSE;
	if (m_incoming_protocol == AccountTypes::NEWS)
	{
        OpString8 tmp_value8;
		RETURN_IF_ERROR(message.GetHeaderValue(Header::NEWSGROUPS, tmp_value8));
		is_newsmessage = tmp_value8.HasContent();
	}
	message.SetFlag(Message::IS_NEWS_MESSAGE, is_newsmessage);

	// Keep original headers up to date
	RETURN_IF_ERROR(message.CopyCurrentToOriginalHeaders(FALSE));

	// Update message content
	MessageEngine::GetInstance()->GetStore()->UpdateMessage(message, TRUE);
	MessageEngine::GetInstance()->GetStore()->SetRawMessage(message);

	return OpStatus::OK;
}

OP_STATUS Account::SendMessage(Message& message, BOOL anonymous)
{
	// Validate the account_id
	if(message.GetAccountId() != GetAccountId())
		return OpStatus::ERR;

	/* NNTP is special, as it uses incoming backend to send messages, in addition to outgoing SMTP */
	if (m_incoming_protocol == AccountTypes::NEWS)
	{
		OpString8 tmp_value8;
		RETURN_IF_ERROR(message.GetHeaderValue(Header::NEWSGROUPS, tmp_value8));

		if (tmp_value8.HasContent())
		{
			RETURN_IF_ERROR(m_incoming_interface->SendMessage(message, FALSE));

			//If it does not have a mail-recipient, don't send it with smtp
			Header::HeaderValue tmp_value_to, tmp_value_cc, tmp_value_bcc;
			RETURN_IF_ERROR(message.GetTo(tmp_value_to, FALSE));
			RETURN_IF_ERROR(message.GetCc(tmp_value_cc, FALSE));
			RETURN_IF_ERROR(message.GetBcc(tmp_value_bcc, FALSE));

			if ( m_outgoing_protocol == AccountTypes::SMTP ||
				 (tmp_value_to.IsEmpty() && tmp_value_cc.IsEmpty() && tmp_value_bcc.IsEmpty()) )
			{
				return OpStatus::OK; //We're done, no need to use outgoing backend
			}

			//It has a smtp backend and a recipient. Fallthrough and send it 'as normal'
		}
    }

	if (!m_outgoing_interface)
		return OpStatus::ERR_NULL_POINTER;

    return m_outgoing_interface->SendMessage(message, anonymous);
}

OP_STATUS Account::Sent(message_gid_t uid, OP_STATUS transmit_status)
{
	m_sent_messages++;

	if (OpStatus::IsSuccess(transmit_status))
		m_incoming_interface->OnMessageSent(uid); // Save this message in sent mail folder!
	return MessageEngine::GetInstance()->Sent(this, uid, transmit_status);
}

unsigned Account::GetSentMessagesCount()
{
	unsigned sent_messages = m_sent_messages;

	m_sent_messages = 0;
	return sent_messages;
}

ProgressInfo& Account::GetProgress(BOOL incoming)
{
	if (incoming && m_incoming_interface)
	{
		return m_incoming_interface->GetProgress();
	}
	else if (!incoming && m_outgoing_interface)
	{
		return m_outgoing_interface->GetProgress();
	}
	else
	{
		static ProgressInfo info;
		return info;
	}
}

OpStringC8 Account::GetProtocolName(AccountTypes::AccountType protocol)
{
	switch (protocol)
	{
		case AccountTypes::POP:	   return "POP";
		case AccountTypes::IMAP:   return "IMAP";
		case AccountTypes::SMTP:   return "SMTP";
		case AccountTypes::NEWS:   return "NNTP";
#ifdef IRC_SUPPORT
		case AccountTypes::IRC:	   return "IRC";
#endif // IRC_SUPPORT
		case AccountTypes::MSN:	   return "MSN";
		case AccountTypes::AOL:	   return "AOL";
		case AccountTypes::ICQ:	   return "ICQ";
#ifdef JABBER_SUPPORT
		case AccountTypes::JABBER: return "JABBER";
#endif // JABBER_SUPPORT
		case AccountTypes::RSS:	   return "RSS";
		case AccountTypes::IMPORT: return "IMPORT";
#ifdef OPERAMAIL_SUPPORT
		case AccountTypes::WEB:	   return "WEB";
#endif // OPERAMAIL_SUPPORT
		case AccountTypes::ARCHIVE: return "ARCHIVE";
	}

	return NULL;
}

AccountTypes::AccountType Account::GetProtocolFromName(const OpStringC8& name)
{
	for (int i = 0; i < AccountTypes::_LAST_ACCOUNT_TYPE; i++)
	{
		AccountTypes::AccountType type = (AccountTypes::AccountType)i;

		if (name.CompareI(GetProtocolName(type)) == 0)
			return type;
	}

	return AccountTypes::UNDEFINED;
}

AccountTypes::AccountType Account::GetOutgoingForIncoming(AccountTypes::AccountType incoming)
{
	switch (incoming)
	{
		case AccountTypes::POP:
		case AccountTypes::IMAP:
		case AccountTypes::NEWS:
			return AccountTypes::SMTP;
		default:
			return AccountTypes::UNDEFINED;
	}
}

AccountTypes::AuthenticationType Account::GetCurrentIncomingAuthMethod() const
{
	if (m_incoming_authentication_method == AccountTypes::AUTOSELECT)
		return m_current_incoming_auth_method;
	else
		return m_incoming_authentication_method;
}

AccountTypes::AuthenticationType Account::GetCurrentOutgoingAuthMethod() const
{
	if (m_outgoing_authentication_method == AccountTypes::AUTOSELECT)
		return m_current_outgoing_auth_method;
	else
		return m_outgoing_authentication_method;
}

UINT32 Account::GetIncomingAuthenticationSupported() const
{
    if (!m_incoming_interface)
        return 0;

    return m_incoming_interface->GetAuthenticationSupported();
}

UINT32 Account::GetOutgoingAuthenticationSupported() const
{
    if (!m_outgoing_interface)
        return 0;

    return m_outgoing_interface->GetAuthenticationSupported();
}

OP_STATUS Account::JoinChatRoom(const OpStringC& room, const OpStringC& password, BOOL no_lookup)
{
    if (!m_incoming_interface)
        return OpStatus::ERR_NULL_POINTER;

    //Set this account as default for chat
	AccountManager* account_manager = (AccountManager*)MessageEngine::GetInstance()->GetAccountManager();
    if (account_manager)
		account_manager->SetLastUsedAccount(GetAccountId());

    return m_incoming_interface->JoinChatRoom(room, password, no_lookup);
}

OP_STATUS Account::LeaveChatRoom(const ChatInfo& room)
{
    if (!m_incoming_interface)
        return OpStatus::ERR_NULL_POINTER;

    return m_incoming_interface->LeaveChatRoom(room);
}

OP_STATUS Account::SendChatMessage(EngineTypes::ChatMessageType type, const OpStringC& message, const ChatInfo& room, const OpStringC& chatter)
{
    if (!m_incoming_interface)
        return OpStatus::ERR_NULL_POINTER;

    return m_incoming_interface->SendChatMessage(type, message, room, chatter);
}

OP_STATUS Account::SetChatProperty(const ChatInfo& room, const OpStringC& chatter, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value)
{
    if (!m_incoming_interface)
        return OpStatus::ERR_NULL_POINTER;

    return m_incoming_interface->SetChatProperty(room, chatter, property, property_string, property_value);
}

OP_STATUS Account::SendFile(const OpStringC& chatter, const OpStringC& filename)
{
	if (!m_incoming_interface)
		return OpStatus::ERR_NULL_POINTER;

	return m_incoming_interface->SendFile(chatter, filename);
}

OP_STATUS Account::FetchNewGroups()
{
    if (!m_incoming_interface)
        return OpStatus::ERR_NULL_POINTER;

    return m_incoming_interface->FetchNewGroups();
}

BOOL Account::IsChatStatusAvailable(AccountTypes::ChatStatus chat_status)
{
	if (!m_incoming_interface)
		return FALSE;

	return m_incoming_interface->IsChatStatusAvailable(chat_status);
}

AccountTypes::ChatStatus Account::GetChatStatus(BOOL& is_connecting)
{
	if (!m_incoming_interface)
	{
		is_connecting = FALSE;
		return AccountTypes::OFFLINE;
	}

	return m_incoming_interface->GetChatStatus(is_connecting);
}

OP_STATUS Account::RenameFolder(OpString& oldCompleteFolderPath, OpString& newCompleteFolderPath)
{
	Index* index = MessageEngine::GetInstance()->GetIndexer()->GetSubscribedFolderIndex(this, oldCompleteFolderPath, 0,  oldCompleteFolderPath, FALSE, FALSE);
	if(index)
	{
		RETURN_IF_ERROR(index->SetName(newCompleteFolderPath.CStr()));

		// only change the search text if the incoming interface is actually able to rename folders (IMAP is, NNTP isn't)
		// bug #260310
		if (!OpStatus::IsError(m_incoming_interface->RenameFolder(oldCompleteFolderPath, newCompleteFolderPath)))
			RETURN_IF_ERROR(index->GetSearch()->SetSearchText(newCompleteFolderPath));
	}
	return OpStatus::OK;
}

OP_STATUS Account::InsertMessage(message_gid_t message_id, index_gid_t destination_index)
{
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return m_offline_log.InsertMessage(message_id, destination_index);
	else if (m_incoming_interface)
		return m_incoming_interface->InsertMessage(message_id, destination_index);

	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS Account::RemoveMessage(message_gid_t message_id, BOOL permanently)
{
	OpINT32Vector message_ids;

	RETURN_IF_ERROR(message_ids.Add(message_id));

	return RemoveMessages(message_ids, permanently);
}

OP_STATUS Account::RemoveMessages(const OpINT32Vector& message_ids, BOOL permanently)
{
	// TODO Remove from this account's index
//	Index* account_index = MessageEngine::GetInstance()->GetIndexById(

	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return m_offline_log.RemoveMessages(message_ids, permanently);
	else if (m_incoming_interface)
		return m_incoming_interface->RemoveMessages(message_ids, permanently);

	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS Account::RemoveMessage(const OpStringC8& internet_location)
{
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return m_offline_log.RemoveMessage(internet_location);
	else if (m_incoming_interface)
		return m_incoming_interface->RemoveMessage(internet_location);

	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS Account::MoveMessages(const OpINT32Vector& message_ids, index_gid_t source_index_id, index_gid_t destination_index_id)
{
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return m_offline_log.MoveMessages(message_ids, source_index_id, destination_index_id);
	else if (m_incoming_interface)
		return m_incoming_interface->MoveMessages(message_ids, source_index_id, destination_index_id);

	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS Account::CopyMessages(const OpINT32Vector& message_ids, index_gid_t source_index_id, index_gid_t destination_index_id)
{
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return m_offline_log.CopyMessages(message_ids, source_index_id, destination_index_id);
	else if (m_incoming_interface)
		return m_incoming_interface->CopyMessages(message_ids, source_index_id, destination_index_id);

	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS Account::ReadMessages(const OpINT32Vector& message_ids, BOOL read)
{
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return m_offline_log.ReadMessages(message_ids, read);
	else if (m_incoming_interface)
		return m_incoming_interface->ReadMessages(message_ids, read);

	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS Account::FlaggedMessage(message_gid_t message_id, BOOL flagged)
{
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return m_offline_log.FlaggedMessage(message_id, flagged);
	else if (m_incoming_interface)
		return m_incoming_interface->FlaggedMessage(message_id, flagged);

	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS Account::ReplyToMessage(message_gid_t message_id)
{
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return m_offline_log.ReplyToMessage(message_id);
	else if (m_incoming_interface)
		return m_incoming_interface->ReplyToMessage(message_id);

	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS Account::EmptyTrash(BOOL done_removing_messages)
{
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return m_offline_log.EmptyTrash(done_removing_messages);
	else if (m_incoming_interface)
		return m_incoming_interface->EmptyTrash(done_removing_messages);

	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS Account::MessagesMovedFromTrash(const OpINT32Vector& message_ids)
{
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return m_offline_log.MessagesMovedFromTrash(message_ids);
	else if (m_incoming_interface)
		return m_incoming_interface->MessagesMovedFromTrash(message_ids);

	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS Account::MarkMessagesAsSpam(const OpINT32Vector& message_ids, BOOL is_spam, BOOL imap_move)
{
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return m_offline_log.MarkMessagesAsSpam(message_ids, is_spam);
	else if (m_incoming_interface)
		return m_incoming_interface->MarkMessagesAsSpam(message_ids, is_spam, imap_move);

	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS Account::Fetched(Message& message, BOOL headers_only, BOOL register_as_new)
{
	return GetMessageDatabase().AddMessage(message);
}

void Account::OnError(const OpStringC& errormessage, EngineTypes::ErrorSeverity severity)
{
	MessageEngine::GetInstance()->OnError(m_account_id, errormessage, m_accountname, severity);
}

void Account::OnError(Str::LocaleString errormessage, EngineTypes::ErrorSeverity severity)
{
	OpString  error_string;

	g_languageManager->GetString(errormessage, error_string);
	OnError(error_string, severity);
}


#ifdef IRC_SUPPORT
void Account::OnChatPropertyChanged(const ChatInfo& room, const OpStringC& chatter,
	const OpStringC& changed_by, EngineTypes::ChatProperty property,
	const OpStringC& property_string, INT32 property_value)
{
	// Notify the GUI.
	MessageEngine::GetInstance()->OnChatPropertyChanged(m_account_id, room, chatter, changed_by, property, property_string, property_value);

	if (property == EngineTypes::CHATTER_NICK)
	{
		// If this is ourselves, we save the nickname.
		if (m_incoming_username == chatter)
		{
			SetIncomingUsername(property_string);
			SaveSettings(TRUE);
		}
	}

}
#endif // IRC_SUPPORT

void Account::OnTimeOut(OpTimer* timer)
{
	if (timer == m_fetch_timer)
	{
		if (!MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->OfflineMode())
		{
			FetchMessages(FALSE);
			// Send any queued messages (send messages added to the outbox when in offline mode or
			// if the connection was dropped)
			// except when queueing on purpose
			if (!m_queue_outgoing)
			{
				MessageEngine::GetInstance()->SendMessages(m_account_id);
			}
		}

		StartAutomaticFetchTimer();
	}
}

OP_STATUS Account::ChangeNick(OpString& new_nick)
{
    if (!m_incoming_interface)
        return OpStatus::ERR_NULL_POINTER;

	return m_incoming_interface->ChangeNick(new_nick);
}


OP_STATUS Account::InitBackends()
{
	OP_ASSERT(!m_incoming_interface && !m_outgoing_interface); //InitBackend should only be called once

	if (m_incoming_protocol != AccountTypes::UNDEFINED)
		RETURN_IF_ERROR(CreateBackendObject(m_incoming_protocol, this, m_incoming_interface));

	if (m_outgoing_protocol != AccountTypes::UNDEFINED)
		RETURN_IF_ERROR(CreateBackendObject(m_outgoing_protocol, this, m_outgoing_interface));

	RETURN_IF_ERROR(SettingsChanged(TRUE));

	StartAutomaticFetchTimer(TRUE);

	return OpStatus::OK;
}

OP_STATUS Account::CreateBackendObject(AccountTypes::AccountType account_type, Account* account, MessageBackend*& backend)
{
	ProtocolBackend* new_backend = NULL;

	switch(account_type)
	{
		case AccountTypes::IMAP:
			new_backend = OP_NEW(ImapBackend, (GetMessageDatabase()));
			break;
#ifdef IRC_SUPPORT
		case AccountTypes::IRC:
			new_backend = OP_NEW(IRCBackend, (GetMessageDatabase()));
			break;
#endif // IRC_SUPPORT
#ifdef MSN_SUPPORT
		case AccountTypes::MSN:
			new_backend = OP_NEW(MSNBackend, (GetMessageDatabase()));
			break;
#endif //MSN_SUPPORT
#ifdef JABBER_SUPPORT
		case AccountTypes::JABBER:
			new_backend = OP_NEW(JabberBackend, (GetMessageDatabase()));
			break;
#endif
		case AccountTypes::NEWS:
			new_backend = OP_NEW(NntpBackend, (GetMessageDatabase()));
			break;
		case AccountTypes::POP:
			new_backend = OP_NEW(PopBackend, (GetMessageDatabase()));
			break;
		case AccountTypes::RSS:
			new_backend = OP_NEW(RSSBackend, (GetMessageDatabase()));
			break;
		case AccountTypes::SMTP:
			new_backend = OP_NEW(SmtpBackend, (GetMessageDatabase()));
			break;
		case AccountTypes::ARCHIVE:
			new_backend = OP_NEW(ArchiveBackend, (GetMessageDatabase()));
			break;
		default:
			OP_ASSERT(!"Unknown backend type?");
			backend = NULL;
			return OpStatus::ERR;
	}

	// Initialization
	if (!new_backend)
		return OpStatus::ERR_NO_MEMORY;

	backend = new_backend;
	return new_backend->Init(account);
}

OP_STATUS Account::GetFormattedEmail(OpString16& email, BOOL do_quote_pair_encode) const
{
    return Header::From::GetFormattedEmail(m_realname, m_email, UNI_L(""), email, do_quote_pair_encode);
}

OP_STATUS Account::GetSignature(OpString& signature, BOOL& isHTML) const
{
	isHTML = m_HTML_signature;
    signature.Empty();

    if (m_signature_file.IsEmpty())
        return OpStatus::OK; //Account doesn't have a signature

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
    OpFile* file = glue_factory->CreateOpFile(m_signature_file);
    if (!file)
        return OpStatus::ERR_NO_MEMORY;

    BOOL file_exists;
    OP_STATUS ret = file->Exists(file_exists);
    if (OpStatus::IsSuccess(ret) && !file_exists)
    {
		glue_factory->DeleteOpFile(file);
        return OpStatus::OK; //No signaturefile is a valid non-existing signature
    }

    UnicodeFileInputStream* file_stream = glue_factory->CreateUnicodeFileInputStream(file,UnicodeFileInputStream::TEXT_FILE);
    if (!file_stream)
    {
		glue_factory->DeleteOpFile(file);
        return OpStatus::ERR_NO_MEMORY;
    }

    int buffer_length;
    uni_char* signature_buffer;
    BOOL has_BOM;
    while (file_stream->has_more_data())
    {
        buffer_length = 512; //Length should be less than 12000 (size of buffer allocated in UnicodeFileInputStream::Construct)
        signature_buffer = file_stream->get_block(buffer_length);
        if (!signature_buffer || buffer_length==0) //Just to be safe
            break;

        has_BOM = (*signature_buffer==0xFEFF);
		if (OpStatus::IsError(ret=signature.Append(signature_buffer+(has_BOM?1:0), (buffer_length/sizeof(uni_char)-(has_BOM?1:0)))))
        {
			glue_factory->DeleteUnicodeFileInputStream(file_stream);
			glue_factory->DeleteOpFile(file);
            return ret;
        }
    }
	glue_factory->DeleteUnicodeFileInputStream(file_stream);
	glue_factory->DeleteOpFile(file);

    //Skip signature delimeter if it is at start of file
    if (!signature.IsEmpty())
    {
        uni_char* signature_start = signature.CStr();
        if (*signature_start=='\r') signature_start++;
        if (*signature_start=='\n') signature_start++;
        if (uni_strncmp(signature_start, UNI_L("-- "), 3) == 0)
        {
            signature_start += 3;
            if (*signature_start=='\r') signature_start++;
            if (*signature_start=='\n') signature_start++;
            return signature.Set(signature_start);
        }
    }
	return OpStatus::OK;
}

OP_STATUS Account::GetFQDN(OpString& fqdn)
{
	BrowserUtils* browser_utils = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();

	if (m_fqdn.IsEmpty())
		m_fqdn.Set(browser_utils->GetLocalFQDN());

	if (m_fqdn.IsEmpty())
		m_fqdn.Set(browser_utils->GetLocalHost());

	if (m_fqdn.IsEmpty())
		m_fqdn.Set("error.opera.illegal");

	m_fqdn.Strip();
	uni_char* end_of_domain = m_fqdn.CStr() ? uni_strchr(m_fqdn.CStr(), ' ') : NULL;
	if (end_of_domain)
	{
		*end_of_domain = 0;
		m_fqdn.Strip();
	}

	//Macs will often return "<machine>.local." as local hostname. This violates RFC1035.
	size_t fqdn_end_offset = m_fqdn.Length()-1;
	while (fqdn_end_offset>0 && *(m_fqdn.CStr()+fqdn_end_offset)=='.')
	{
		*(m_fqdn.CStr()+fqdn_end_offset--) = 0;
	}

	// Some windows machines will have a underscore '_' in the domain and some mail servers don't support that
	// we can then replace it with a dash instead
	for (int pos = m_fqdn.Length() - 1; pos >= 0; pos--)
	{
		if (m_fqdn[pos] == '_')
			m_fqdn[pos] = '-';
	}

	return fqdn.Set(m_fqdn);
}

int Account::GetDefaultStore(BOOL get_setting) const
{
	if (get_setting || !GetLowBandwidthMode())
		return m_default_store;
	else
		return g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailStoreType);
}

OP_STATUS Account::GetEmail(OpString8& email) const
{
	BOOL failed_imaa = FALSE;

	OP_STATUS ret = OpMisc::ConvertToIMAAAddress(m_email, TRUE, email, &failed_imaa);

	if (failed_imaa)
		ret=OpStatus::ERR_PARSING_FAILED;

	return ret;
}

OP_STATUS Account::GetIncomingServername(OpString8& servername) const
{
	return OpMisc::ConvertToIMAAAddressList(m_incoming_servername, FALSE, servername, ',');
}

OP_STATUS Account::GetIncomingServernames(OpAutoVector<OpString> &servernames) const
{
	// Split the list up into individual server names.
	StringTokenizer tokenizer(m_incoming_servername, UNI_L(","));
	while (tokenizer.HasMoreTokens())
	{
		OpAutoPtr<OpString> current_server(OP_NEW(OpString, ()));
		if (current_server.get() == 0)
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(tokenizer.NextToken(*current_server.get()));
		current_server->Strip();

		RETURN_IF_ERROR(servernames.Add(current_server.get()));
		current_server.release();
	}

	return OpStatus::OK;
}

OP_STATUS Account::GetOutgoingServername(OpString8& servername) const
{
	return OpMisc::ConvertToIMAAAddressList(m_outgoing_servername, FALSE, servername, ',');
}

OP_STATUS Account::GetIncomingUsername(OpString8& username) const
{
	return username.Set(m_incoming_username.CStr()); //IMAA is idling. Usernames can until further only be US_ASCII
}

OP_STATUS Account::GetOutgoingUsername(OpString8& username) const
{
	return username.Set(m_outgoing_username.CStr()); //IMAA is idling. Usernames can until further only be US_ASCII
}

#ifdef WAND_CAP_GANDALF_3_API
OP_STATUS Account::GetIncomingPassword(PrivacyManagerCallback* callback) const
{
	if (m_incoming_temp_login.HasContent())
	{
		callback->OnPasswordRetrieved(m_incoming_temp_login);
		return OpStatus::OK;
	}

	if (OpStatus::IsError(PrivacyManager::GetInstance()->GetPassword(WAND_OPERA_MAIL, m_incoming_servername, m_incoming_username, callback)))
		callback->OnPasswordRetrieved(0);

	return OpStatus::OK;
}

OP_STATUS Account::GetOutgoingPassword(PrivacyManagerCallback* callback) const
{
	if (m_outgoing_temp_login.HasContent())
	{
		callback->OnPasswordRetrieved(m_outgoing_temp_login);
		return OpStatus::OK;
	}

	if (OpStatus::IsError(PrivacyManager::GetInstance()->GetPassword(WAND_OPERA_MAIL, m_outgoing_servername, m_outgoing_username, callback)))
		callback->OnPasswordRetrieved(0);

	return OpStatus::OK;
}
#else
OP_STATUS Account::GetIncomingPassword(OpString& password) const
{
	if (m_incoming_temp_login.HasContent())
		return password.Set(m_incoming_temp_login);

	if (OpStatus::IsError(PrivacyManager::GetInstance()->GetPassword(WAND_OPERA_MAIL, m_incoming_servername, m_incoming_username, password)))
		password.Empty();

	return OpStatus::OK;
}

OP_STATUS Account::GetOutgoingPassword(OpString& password) const
{
	if (m_outgoing_temp_login.HasContent())
		return password.Set(m_outgoing_temp_login);

	if (OpStatus::IsError(PrivacyManager::GetInstance()->GetPassword(WAND_OPERA_MAIL, m_outgoing_servername, m_outgoing_username, password)))
		password.Empty();

	return OpStatus::OK;
}
#endif // WAND_CAP_GANDALF_3_API

OP_STATUS Account::SetAccountName(const OpStringC& accountname)
{
	if (m_accountname.Compare(accountname)==0)
		return OpStatus::OK;

    AccountManager* account_manager = (AccountManager*)MessageEngine::GetInstance()->GetAccountManager();
    if (!account_manager)
        return OpStatus::ERR_NULL_POINTER;

	OpString temp_accountname;
	RETURN_IF_ERROR(temp_accountname.Set(accountname));

	OpString temporary_string;
	g_languageManager->GetString(Str::S_ACCOUNT_NAME_TEMPORARY, temporary_string);

    int length = temp_accountname.Length();
	int temporary_string_length = temporary_string.Length();
	if (length >= temporary_string_length)
	{
		if (m_is_temporary) //Add " (temporary)" if not already present
		{
			if (temp_accountname.CStr() && uni_stricmp(temp_accountname.CStr()+length-temporary_string_length, temporary_string.CStr())!=0)
				OpStatus::Ignore(temp_accountname.Append(temporary_string));
		}
		else //Remove " (temporary)" if present
		{
			if (temp_accountname.CStr() && uni_stricmp(temp_accountname.CStr()+length-temporary_string_length, temporary_string.CStr())==0)
				*(temp_accountname.CStr()+length-temporary_string_length) = 0;
		}
	}

    UINT16 tmp_account_id;
    OpString suggested_accountname;
	int prefix = 0;
	do
	{
		RETURN_IF_ERROR(suggested_accountname.Set(temp_accountname));

		if (++prefix > 1)
		{
			RETURN_IF_ERROR(suggested_accountname.AppendFormat(UNI_L(" (%d)"), prefix));
		}
	} while ((tmp_account_id=account_manager->FindAccountId(suggested_accountname))!=0 && tmp_account_id!=m_account_id);

	Index* index= MessageEngine::GetInstance()->GetIndexer()->GetIndexById(IndexTypes::FIRST_ACCOUNT+m_account_id);
	if (index)
		index->SetName(suggested_accountname);

    return m_accountname.Set(suggested_accountname);
}

void Account::SetIsTemporary(BOOL temporary)
{
    if (m_is_temporary == temporary)
        return;

    m_is_temporary = temporary;
    OpStatus::Ignore(SetAccountName(m_accountname));
}

AccountTypes::AuthenticationType Account::SetAuthenticationMethod(BOOL incoming, AccountTypes::AuthenticationType method)
{
	AccountTypes::AuthenticationType verified_method = method;
	UINT32 available_methods = incoming ? (m_incoming_interface ? m_incoming_interface->GetAuthenticationSupported() : 0xFFFFFFFF) :
											(m_outgoing_interface ? m_outgoing_interface->GetAuthenticationSupported() : 0xFFFFFFFF);

	if ((available_methods & (1<<(int)verified_method)) == 0)
	{
		int i=-1;
		while (available_methods>0)
		{
			available_methods = available_methods>>1;
			i++;
		}
		verified_method = i<0 ? AccountTypes::NONE : (AccountTypes::AuthenticationType)i;
	}

	if (incoming)
	{
		m_incoming_authentication_method = verified_method;
		m_current_incoming_auth_method = AccountTypes::AUTOSELECT;
	}
	else
	{
		m_outgoing_authentication_method = verified_method;
		m_current_outgoing_auth_method = AccountTypes::AUTOSELECT;
	}

	return verified_method;
}

OP_STATUS Account::SetIncomingPassword(const OpStringC& password, BOOL remember)
{
	if (m_incoming_interface)
		static_cast<ProtocolBackend*>(m_incoming_interface)->ResetRetrievedPassword();

	if (remember)
	{
		m_incoming_temp_login.Empty();

		if (password.HasContent())
			return PrivacyManager::GetInstance()->SetPassword(WAND_OPERA_MAIL, m_incoming_servername, m_incoming_username, password);
		else
			return PrivacyManager::GetInstance()->DeletePassword(WAND_OPERA_MAIL, m_incoming_servername, m_incoming_username);
	}
	else
	{
		return m_incoming_temp_login.Set(password);
	}
}

OP_STATUS Account::SetOutgoingPassword(const OpStringC& password, BOOL remember)
{
	if (m_outgoing_interface)
		static_cast<ProtocolBackend*>(m_outgoing_interface)->ResetRetrievedPassword();

	if (remember)
	{
		m_outgoing_temp_login.Empty();

		if (m_outgoing_username.HasContent() && password.HasContent())
			return PrivacyManager::GetInstance()->SetPassword(WAND_OPERA_MAIL, m_outgoing_servername, m_outgoing_username, password);
		// password are identified by servername and username, if both are the same for outgoing and incoming then we shouldn't delete the outgoing
		else if (m_incoming_servername.Compare(m_outgoing_servername) || m_incoming_username.Compare(m_outgoing_username))
			return PrivacyManager::GetInstance()->DeletePassword(WAND_OPERA_MAIL, m_outgoing_servername, m_outgoing_username);
	}
	else
	{
		return m_outgoing_temp_login.Set(password);
	}

	return OpStatus::OK;
}

void Account::ResetRetrievedPassword()
{
	if (m_incoming_interface)
	{
		m_incoming_interface->ResetRetrievedPassword();
		OpStatus::Ignore(m_incoming_interface->Disconnect());
	}

	if (m_outgoing_interface)
	{
		m_outgoing_interface->ResetRetrievedPassword();
		OpStatus::Ignore(m_outgoing_interface->Disconnect());
	}
}

OP_STATUS Account::SetSignature(const OpStringC& signature, BOOL isHTML, BOOL overwrite_existing)
{
    OP_STATUS ret;
	if (m_signature_file.IsEmpty())
	{
		RETURN_IF_ERROR(CreateSignatureFileName());
    }

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
    if (!overwrite_existing)
    {
        //Check if a file already exists
        OpFile* file = glue_factory->CreateOpFile(m_signature_file);
        if (!file)
            return OpStatus::ERR_NO_MEMORY;

        BOOL file_exists;
	    ret = file->Exists(file_exists);
		glue_factory->DeleteOpFile(file);
        if (OpStatus::IsError(ret) || file_exists)
            return OpStatus::OK; //Don't overwrite existing file
    }

    UnicodeFileOutputStream* file_stream = glue_factory->CreateUnicodeFileOutputStream(m_signature_file.CStr(),"utf-8");
    if (!file_stream)
        return OpStatus::ERR;

	TRAPD(err, file_stream->WriteStringL(UNI_L("\xFEFF")); 
			   if (signature.HasContent()) file_stream->WriteStringL(signature.CStr()));

	glue_factory->DeleteUnicodeFileOutputStream(file_stream);

    m_HTML_signature = isHTML;
	//Save changes
    return SaveSettings(TRUE);
}

OP_STATUS Account::CreateIncomingOptionsFileName()
{
	if (m_incoming_optionsfile.IsEmpty())
	{
		RETURN_IF_ERROR(MailFiles::GetAccountsSettingsFilename(UNI_L("incoming"), m_account_id, m_incoming_optionsfile));
	}
	return OpStatus::OK;
}

OP_STATUS Account::CreateOutgoingOptionsFileName()
{
	if (m_outgoing_optionsfile.IsEmpty())
	{
		RETURN_IF_ERROR(MailFiles::GetAccountsSettingsFilename(UNI_L("outgoing"), m_account_id, m_outgoing_optionsfile));
	}
	return OpStatus::OK;
}

OP_STATUS Account::CreateSignatureFileName()
{
	if (m_signature_file.IsEmpty())
	{
		RETURN_IF_ERROR(MailFiles::GetAccountsSettingsFilename(UNI_L("signature"), m_account_id, m_signature_file));

        return SaveSettings(TRUE);
	}
	return OpStatus::OK;
}

OP_STATUS Account::LogIncoming(const OpStringC8& heading, const OpStringC8& text)
{
#if defined (DEBUG_OUTPUT) && defined (MSWIN)
	OpString8 dummy;
	dummy.AppendFormat("\r\n%s\r\n%s\r\n", heading.HasContent() ? heading.CStr() : "", text.HasContent() ? text.CStr() : "");
	::OutputDebugStringA(dummy.CStr());
#endif

    if (m_incoming_logfile.IsEmpty())
        return OpStatus::OK; //Nothing to do

    if (!m_incoming_logfile_ptr)
    {
        m_incoming_logfile_ptr = MessageEngine::GetInstance()->GetGlueFactory()->CreateOpFile(m_incoming_logfile);
        if (!m_incoming_logfile_ptr)
            return OpStatus::ERR_NO_MEMORY;
    }

	return DesktopFileLogger::Log(m_incoming_logfile_ptr, heading, text);
}


OP_STATUS Account::LogOutgoing(const OpStringC8& heading, const OpStringC8& text)
{
#if defined (DEBUG_OUTPUT) && defined (MSWIN)
	OpString8 dummy;
	dummy.AppendFormat("\r\n%s\r\n%s\r\n", heading.HasContent() ? heading.CStr() : "", text.HasContent() ? text.CStr() : "");
	::OutputDebugStringA(dummy.CStr());
#endif

    if (m_outgoing_logfile.IsEmpty())
        return OpStatus::OK; //Nothing to do

    if (!m_outgoing_logfile_ptr)
    {
        m_outgoing_logfile_ptr = MessageEngine::GetInstance()->GetGlueFactory()->CreateOpFile(m_outgoing_logfile);
        if (!m_outgoing_logfile_ptr)
            return OpStatus::ERR_NO_MEMORY;
    }

    return DesktopFileLogger::Log(m_outgoing_logfile_ptr, heading, text);
}

UINT Account::GetStartOfDccPool() const
{
	UINT port = 0;
	if (m_incoming_interface != 0)
		port = m_incoming_interface->GetStartOfDccPool();

	return port;
}

UINT Account::GetEndOfDccPool() const
{
	UINT port = 0;
	if (m_incoming_interface != 0)
		port = m_incoming_interface->GetEndOfDccPool();

	return port;
}

void Account::SetCanAcceptIncomingConnections(BOOL can_accept)
{
	if (m_incoming_interface != 0)
		m_incoming_interface->SetCanAcceptIncomingConnections(can_accept);
}

BOOL Account::GetCanAcceptIncomingConnections() const
{
	BOOL can_accept = FALSE;
	if (m_incoming_interface != 0)
		can_accept = m_incoming_interface->GetCanAcceptIncomingConnections();

	return can_accept;
}

void Account::SetStartOfDccPool(UINT port)
{
	if (m_incoming_interface != 0)
		m_incoming_interface->SetStartOfDccPool(port);
}

void Account::SetEndOfDccPool(UINT port)
{
	if (m_incoming_interface != 0)
		m_incoming_interface->SetEndOfDccPool(port);
}

void Account::SetOpenPrivateChatsInBackground(BOOL open_in_background)
{
	if (m_incoming_interface != 0)
		m_incoming_interface->SetOpenPrivateChatsInBackground(open_in_background);
}

BOOL Account::GetOpenPrivateChatsInBackground() const
{
	BOOL open_in_background = FALSE;
	if (m_incoming_interface != 0)
		open_in_background = m_incoming_interface->GetOpenPrivateChatsInBackground();

	return open_in_background;
}

OP_STATUS Account::GetPerformWhenConnected(OpString& commands) const
{
	if (m_incoming_interface == 0)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(m_incoming_interface->GetPerformWhenConnected(commands));
	return OpStatus::OK;
}


OP_STATUS Account::SetPerformWhenConnected(const OpStringC& commands)
{
	if (m_incoming_interface == 0)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(m_incoming_interface->SetPerformWhenConnected(commands));
	return OpStatus::OK;
}

OP_STATUS
Account::PresenceChanged(const uni_char* identity, presence_t presence)
{
	// look up identity in contact list and change status
	return OpStatus::OK;
}

OP_STATUS Account::GetAuthenticationString(AccountTypes::AuthenticationType type, OpString& string) const
{
	switch(type)
	{
	case AccountTypes::NONE:
    case AccountTypes::PLAINTEXT:
    case AccountTypes::AUTOSELECT:
		{
			Str::LocaleString title_id = Str::NOT_A_STRING;

            switch(type)
            {
            case AccountTypes::NONE:       title_id = Str::SI_SECURITY_TEXT_NONE; break;
            case AccountTypes::PLAINTEXT:  title_id = Str::D_PLAINTEXT; break;
            case AccountTypes::AUTOSELECT: title_id = Str::D_AUTHENTICATION_AUTO; break;
            }

			string.Empty();

			return g_languageManager->GetString(title_id, string);
		}

	case AccountTypes::PLAIN:		 return string.Set("AUTH PLAIN");
	case AccountTypes::SIMPLE:	 return string.Set("AUTH SIMPLE");
	case AccountTypes::GENERIC:	 return string.Set("AUTH GENERIC");
	case AccountTypes::LOGIN:		 return string.Set("AUTH LOGIN");
	case AccountTypes::APOP:		 return string.Set("APOP");
	case AccountTypes::DIGEST_MD5: return string.Set("AUTH DIGEST-MD5");
	case AccountTypes::SHA1:		 return string.Set("AUTH SHA1");
	case AccountTypes::KERBEROS:	 return string.Set("AUTH KERBEROS");
	case AccountTypes::CRAM_MD5:	 return string.Set("AUTH CRAM-MD5");
	default: string.Empty(); return OpStatus::OK;
	};
}

OP_STATUS Account::SetDefaultStore(int default_store)
{
	if (default_store != m_default_store)
	{
		m_default_store = default_store;
		RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->ChangeStorageType(m_account_id, m_default_store));
	}

	return OpStatus::OK;
}

OP_STATUS Account::ReadPasswordsFromAccounts(PrefsFile* prefs_file, const OpStringC8& section, BOOL import/*=FALSE*/)
{
	// Read passwords from accounts.ini (old-style, pre-kestrel) and convert to passwords in Wand (new-style)

	// Read encrypted passwords from accounts.ini
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, section, "Incoming Password", m_encrypted_incoming_password));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(prefs_file, section, "Outgoing Password", m_encrypted_outgoing_password));

	if (m_encrypted_incoming_password.HasContent() || m_encrypted_outgoing_password.HasContent())
	{
		BOOL paranoia_mode = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseParanoidMailpassword);
		if (paranoia_mode)
		{
			// ask password
			g_ssl_api->StartSecurityPasswordSession();
			URL url;
			if (import)
			{
				g_main_message_handler->SetCallBack(this, MSG_M2_ACCOUNT_ASK_PASSWORD_DONE, (MH_PARAM_1)this);
				SSL_dialog_config config(NULL, g_main_message_handler, MSG_M2_ACCOUNT_ASK_PASSWORD_DONE, (MH_PARAM_1)this, url);
				m_askpasswordcontext = OP_NEW(SSLSecurtityPasswordCallbackImpl, (OpSSLListener::SSLSecurityPasswordCallback::JustPassword, OpSSLListener::SSLSecurityPasswordCallback::CertificateUnlock, Str::D_M2_ACCOUNT_IMPORT_MASTER_PASSWD_TITLE, Str::D_M2_ACCOUNT_IMPORT_MASTER_PASSWD_MESSAGE, config));
			}
			else
			{
				g_main_message_handler->SetCallBack(this, MSG_M2_ACCOUNT_ASK_PASSWORD_DONE, (MH_PARAM_1)this);
				SSL_dialog_config config(NULL, g_main_message_handler, MSG_M2_ACCOUNT_ASK_PASSWORD_DONE, (MH_PARAM_1)this, url);
				m_askpasswordcontext = OP_NEW(SSLSecurtityPasswordCallbackImpl, (OpSSLListener::SSLSecurityPasswordCallback::JustPassword, OpSSLListener::SSLSecurityPasswordCallback::CertificateUnlock, Str::D_M2_ACCOUNT_UPGRADE_MASTER_PASSWD_TITLE, Str::D_M2_ACCOUNT_UPGRADE_MASTER_PASSWD_MESSAGE, config));
			}
			if(m_askpasswordcontext)
			{
				OP_STATUS op_err = m_askpasswordcontext->StartDialog();
				if(OpStatus::IsError(op_err))
				{
					OP_DELETE(m_askpasswordcontext);
					m_askpasswordcontext = NULL;
				}
			}
		}
		else
		{
			OpString incoming_plaintext, outgoing_plaintext;
			const char* obfuscation_password = "\x083\x0C4\x004\x083\x07D\x0E4\x001\x075\x005\x083\x0C8\x0FF\x0EB\x03A\x000";
			DecryptPassword(m_encrypted_incoming_password, incoming_plaintext, obfuscation_password);
			DecryptPassword(m_encrypted_outgoing_password, outgoing_plaintext, obfuscation_password);

			OpStatus::Ignore(SetIncomingPassword(incoming_plaintext));
			OpStatus::Ignore(SetOutgoingPassword(outgoing_plaintext));

		}
		// Remove old passwords
		RETURN_IF_LEAVE(prefs_file->DeleteKeyL(section.CStr(), "Incoming Password");
						prefs_file->DeleteKeyL(section.CStr(), "Outgoing Password"));
	}

	return OpStatus::OK;
}

OP_STATUS Account::DecryptPassword(const OpStringC8& encrypted_password, OpString& plaintext_password, const char * key)
{
	OP_STATUS ret;
	OP_ASSERT(key);
	plaintext_password.Empty();
	if (encrypted_password.IsEmpty()) //Empty password should decrypt into an empty password
		return OpStatus::OK;

	// Paranoia mode is handled internally so only supply a standard string if not in paranoid mode
	// I assume in paranoid mode it will use the master password

	unsigned char* encrypted_buffer;
	unsigned int buffer_len;
	if ((ret=HexStrToByte(encrypted_password, encrypted_buffer, buffer_len)) != OpStatus::OK)
		return ret;

	unsigned long plaintext_length = 0;

	g_ssl_api->StartSecurityPasswordSession();

	SSL_dialog_config config;
	unsigned char* plaintext_buf = g_ssl_api->DecryptWithSecurityPassword(ret, encrypted_buffer, buffer_len, plaintext_length, key, config);
	OP_ASSERT(ret != InstallerStatus::ERR_PASSWORD_NEEDED);
	OP_DELETEA(encrypted_buffer);

	g_ssl_api->EndSecurityPasswordSession();

	if (!plaintext_buf)
		return OpStatus::ERR;

	ret = plaintext_password.Set((const char*)plaintext_buf, plaintext_length);
	memset(plaintext_buf, 0, plaintext_length);
	OP_DELETEA(plaintext_buf);

	return ret;
}

void Account::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(((Account *)par1) == this);

	OpString incoming_plaintext;
	OpString outgoing_plaintext;

	char* key_buffer = NULL;
	const char* key = NULL;
	const char* obfuscation_password = "\x083\x0C4\x004\x083\x07D\x0E4\x001\x075\x005\x083\x0C8\x0FF\x0EB\x03A\x000";

	switch (msg)
	{
	case MSG_M2_ACCOUNT_ASK_PASSWORD_DONE:
		g_main_message_handler->UnsetCallBack(this, MSG_M2_ACCOUNT_ASK_PASSWORD_DONE, (MH_PARAM_1) this);
		if (par2 == IDOK)
		{
			OpString password;
			password.Set(m_askpasswordcontext->GetOldPassword());
			OpStatus::Ignore(password.UTF8Alloc(&key_buffer));
			key = key_buffer;
		}
		else // cancel = use obfuscation, not master password
			key = obfuscation_password;

		if (key)
		{
			DecryptPassword(m_encrypted_incoming_password, incoming_plaintext, key);
			DecryptPassword(m_encrypted_outgoing_password, outgoing_plaintext, key);
			OpStatus::Ignore(SetIncomingPassword(incoming_plaintext));
			OpStatus::Ignore(SetOutgoingPassword(outgoing_plaintext));
		}
		g_ssl_api->EndSecurityPasswordSession();
		if (key_buffer)
			OP_DELETEA(key_buffer);
		OP_DELETE(m_askpasswordcontext);
		m_askpasswordcontext = NULL;
		break;
	}
}

const char * Account::GetIcon(BOOL progress_icon)
{
	if (progress_icon && GetProgress(TRUE).GetCurrentAction() != ProgressInfo::NONE)
	{
		return "Mail Accounts Busy";
	}
	else if (m_incoming_interface && progress_icon)
	{
		return m_incoming_interface->GetIcon(progress_icon);
	}
	else
	{
		if (!progress_icon && m_incoming_protocol == AccountTypes::RSS)
			return "Mail Newsfeeds";
		else
			return MessageBackend::GetIcon(progress_icon);
	}
}
#endif //M2_SUPPORT

