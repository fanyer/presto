/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/util/accountcreator.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/prefs/prefsmanager/collections/pc_m2.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"


/////////////////////////////////////////////////////////////////////////////////////
//                                AccountCreator
/////////////////////////////////////////////////////////////////////////////////////


/***********************************************************************************
 ** Initialize the mail providers, run before using any other function
 **
 ** AccountCreator::Init
 ***********************************************************************************/
OP_STATUS AccountCreator::Init()
{
	OpFile providers_file;

	// Open the providers file for reading
	RETURN_IF_ERROR(providers_file.Construct(UNI_L("mailproviders.xml"), OPFILE_INI_FOLDER));
	RETURN_IF_ERROR(providers_file.Open(OPFILE_READ));

	// Parse XML file
	XMLFragment fragment;
	RETURN_IF_ERROR(fragment.Parse(&providers_file));

	RETURN_IF_ERROR(providers_file.Close());

	// Do something with parsed data
	return GetMailProviders(fragment);
}


/***********************************************************************************
 ** Check whether we know about a mail provider, and get server information for it
 **
 ** AccountCreator::GetServer
 ** @param email_address Mail address to search for
 ** @param protocol Protocol to find server for
 ** @param server Where to output details
 ** @return OpStatus::OK if we found the server and could fill in details, error
 **         codes otherwise
 ***********************************************************************************/
OP_STATUS AccountCreator::GetServer(const OpStringC& email_address,
									AccountTypes::AccountType protocol,
									BOOL incoming,
									Server& server)
{
	int domainpos = email_address.FindLastOf('@');
	if (domainpos == KNotFound)
		return OpStatus::ERR;

	// Find mail provider
	MailProvider* provider;
	RETURN_IF_ERROR(m_domain_providers.GetData(email_address.CStr() + domainpos + 1, &provider));

	// Check for preferred protocol
	if (protocol == AccountTypes::UNDEFINED)
	{
		if (incoming)
			protocol = provider->preferred_protocol;
		else
			protocol = Account::GetOutgoingForIncoming(provider->preferred_protocol);
	}

	// Find server
	Server* found_server;
	RETURN_IF_ERROR(provider->servers.GetData(protocol, &found_server));
	RETURN_IF_ERROR(server.CopyFrom(*found_server));

	// Find username
	OpString username;
	RETURN_IF_ERROR(server.GetUsername(email_address, username));
	return server.SetUsername(username);
}


/***********************************************************************************
 ** Get reasonable default values for an incoming server based on an email address
 **
 ** AccountCreator::GetServerDefaults
 ** @param email_address Mail address to use
 ** @param protocol Protocol for defaults, or UNDEFINED
 ** @param server Where to output defaults
 ***********************************************************************************/
OP_STATUS AccountCreator::GetServerDefaults(const OpStringC& email_address,
											AccountTypes::AccountType protocol,
											Server& server)
{
	if (email_address.IsEmpty())
		return OpStatus::OK;

	// Initialize server
	RETURN_IF_ERROR(server.Init(protocol));

	// Get user and domain from email address
	OpString user, domain;
	if (!user.Reserve(email_address.Length()) || !domain.Reserve(email_address.Length()))
		return OpStatus::ERR_NO_MEMORY;

	uni_sscanf(email_address.CStr(), UNI_L("%[^@]@%s"), user.CStr(), domain.CStr());

	// Guess username
	RETURN_IF_ERROR(server.SetUsername(user));

	// Guess server name
	if (domain.HasContent())
	{
		OpString mailserver;

		RETURN_IF_ERROR(mailserver.AppendFormat(UNI_L("mail.%s"), domain.CStr()));
		RETURN_IF_ERROR(server.SetHost(mailserver));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Create an account
 **
 ** AccountCreator::CreateAccount
 ** @param name Name for account (usually name of user)
 ** @param email_address Address to create an account for
 ** @param company_name Name of company of user
 ** @param account_name Name of the account
 ** @param incoming_server Server details for incoming data
 ** @param outgoing_server Server details for outgoing data
 ** @return OpStatus::OK if account was successfully created, error codes otherwise
 ***********************************************************************************/
OP_STATUS AccountCreator::CreateAccount(const OpStringC& name,
										const OpStringC& email_address,
										const OpStringC& company_name,
										const OpStringC& account_name,
										const Server& incoming_server,
										const Server& outgoing_server,
										MessageEngine* engine,
									    Account*& new_account)
{
	BOOL	 first_account = !engine->GetAccountManager()->GetMailNewsAccountCount();

	{
		// Make sure this pointer can only be used inside this block
		OpAutoPtr<Account> account(engine->CreateAccount());
		if (!account.get())
			return OpStatus::ERR_NO_MEMORY;

		// Start by using sensible defaults for the protocol
		account->SetIncomingProtocol(incoming_server.GetProtocol());
		account->SetOutgoingProtocol(outgoing_server.GetProtocol());
		RETURN_IF_ERROR(account->SetDefaults());

		// Fill in account meta-information
		RETURN_IF_ERROR(account->SetRealname	(name));
		RETURN_IF_ERROR(account->SetOrganization(company_name));
		RETURN_IF_ERROR(account->SetEmail		(email_address));
		RETURN_IF_ERROR(account->SetAccountName	(account_name));

		// Fill in incoming server details
		RETURN_IF_ERROR(account->SetIncomingServername			(incoming_server.GetHostname()));
						account->SetIncomingPort				(incoming_server.GetPort());
		RETURN_IF_ERROR(account->SetIncomingUsername			(incoming_server.GetUsername()));
		RETURN_IF_ERROR(account->SetIncomingPassword			(incoming_server.GetPassword()));
						account->SetSaveIncomingPassword		(incoming_server.GetPassword().HasContent());
						account->SetIncomingAuthenticationMethod(incoming_server.GetAuthenticationType());
						account->SetUseSecureConnectionIn		(incoming_server.GetSecure());
						account->SetLeaveOnServer				(incoming_server.GetPopLeaveMessages());
						account->SetPermanentDelete				(incoming_server.GetPopDeletePermanently());
						account->SetImapBodyStructure			(incoming_server.GetImapBodyStructure());
						account->SetImapIdle					(incoming_server.GetImapIdle());
						account->SetForceSingleConnection		(incoming_server.GetForceSingleConnection());
						account->EnableQRESYNC					(incoming_server.IsQRESYNCEnabled());

		// Fill in outgoing server details
		RETURN_IF_ERROR(account->SetOutgoingServername			(outgoing_server.GetHostname()));
						account->SetOutgoingPort				(outgoing_server.GetPort());
		RETURN_IF_ERROR(account->SetOutgoingUsername			(outgoing_server.GetUsername()));
		RETURN_IF_ERROR(account->SetOutgoingPassword			(outgoing_server.GetPassword()));
						account->SetSaveOutgoingPassword		(outgoing_server.GetPassword().HasContent());
						account->SetOutgoingAuthenticationMethod(outgoing_server.GetAuthenticationType());
						account->SetUseSecureConnectionOut		(outgoing_server.GetSecure());

		// Initialize the account
		RETURN_IF_ERROR(account->Init());

		// we can now set up the sent folder path
		if (incoming_server.GetImapSentFoldername().HasContent())
			account->SetFolderPath(AccountTypes::FOLDER_SENT, incoming_server.GetImapSentFoldername());

		new_account = account.release();
	}

	// If this is the first account and not an IRC account, add an introduction email
	if	(first_account && new_account->GetIncomingProtocol() != AccountTypes::IRC)
	{
		new_account->AddIntroductionMail();
#ifdef DU_CAP_PREFS
		time_t now = g_timecache->CurrentTime();
		TRAPD(err, g_pcm2->WriteIntegerL(PrefsCollectionM2::LastDatabaseCheck, now));
#endif // DU_CAP_PREFS
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Get all mail providers out of a parsed XML fragment
 **
 ** AccountCreator::GetMailProviders
 ***********************************************************************************/
OP_STATUS AccountCreator::GetMailProviders(XMLFragment & fragment)
{
	// Parse providers
	if (fragment.EnterElement(UNI_L("providers")))
	{
		while (fragment.EnterElement(UNI_L("provider")))
		{
			OpAutoPtr<MailProvider> provider(OP_NEW(MailProvider, ()));
			if (!provider.get())
				return OpStatus::ERR_NO_MEMORY;

			// Parse domains
			if (fragment.EnterElement(UNI_L("domains")))
			{
				while (fragment.EnterElement(UNI_L("domain")))
				{
					OpAutoPtr<OpString> domain(OP_NEW(OpString, ()));
					if (!domain.get())
						return OpStatus::ERR_NO_MEMORY;

					RETURN_IF_ERROR(domain->Set(fragment.GetText()));
					RETURN_IF_ERROR(provider->domains.Add(domain.get()));
					domain.release();

					fragment.LeaveElement();
				}
				fragment.LeaveElement();
			}

			// Parse username
			OpString     custom_username;
			UsernameType username_type = USERNAME_USER;

			if (fragment.EnterElement(UNI_L("username")))
			{
				RETURN_IF_ERROR(GetUsernameTemplate(fragment, username_type, custom_username));

				fragment.LeaveElement();
			}

			// Parse preferred account type
			if (fragment.EnterElement(UNI_L("preferred")))
			{
				OpString8 preferred;
				RETURN_IF_ERROR(preferred.Set(fragment.GetText()));
				provider->preferred_protocol = Account::GetProtocolFromName(preferred);

				fragment.LeaveElement();
			}

			// Parse servers
			if (fragment.EnterElement(UNI_L("servers")))
			{
				while (fragment.EnterElement(UNI_L("server")))
				{
					RETURN_IF_ERROR(GetServer(fragment, provider.get(), username_type, custom_username));

					fragment.LeaveElement();
				}
				fragment.LeaveElement();
			}

			// Add fully parsed provider to indexes
			RETURN_IF_ERROR(m_providers.Add(provider.get()));
			MailProvider* new_provider = provider.release();

			for (unsigned i = 0; i < new_provider->domains.GetCount(); i++)
				RETURN_IF_ERROR(m_domain_providers.Add(new_provider->domains.Get(i)->CStr(), new_provider));

			fragment.LeaveElement();
		}

		fragment.LeaveElement();
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Get server information out of an XML fragment, and add server to provider
 **
 ** AccountCreator::GetServer
 ***********************************************************************************/
OP_STATUS AccountCreator::GetServer(XMLFragment& fragment, MailProvider* provider, UsernameType username_type, const OpStringC& custom_username)
{
	OpAutoPtr<Server> server(OP_NEW(Server, ()));
	if (!server.get())
		return OpStatus::ERR_NO_MEMORY;

	// Get protocol
	OpString8 protocol;

	RETURN_IF_ERROR(protocol.Set(fragment.GetAttribute(UNI_L("protocol"))));
	RETURN_IF_ERROR(server->Init(Account::GetProtocolFromName(protocol)));

	// If there's no preferred protocol, the first one is preferred
	if (provider->preferred_protocol == AccountTypes::UNDEFINED)
		provider->preferred_protocol = server->GetProtocol();

	// Get hostname
	if (fragment.EnterElement(UNI_L("host")))
	{
		RETURN_IF_ERROR(server->SetHost(fragment.GetText()));
		fragment.LeaveElement();
	}

	// Get port
	if (fragment.EnterElement(UNI_L("port")))
	{
		unsigned port;

		if (uni_sscanf(fragment.GetText(), UNI_L("%u"), &port) == 1)
			server->SetPort(port);
		fragment.LeaveElement();
	}

	// Get authentication type
	if (fragment.EnterElement(UNI_L("auth")))
	{
		server->SetAuthenticationType(GetAuthenticationType(fragment.GetText()));
		fragment.LeaveElement();
	}

	// Get user (if there, use default otherwise)
	if (fragment.EnterElement(UNI_L("username")))
	{
		OpString new_custom_username;

		RETURN_IF_ERROR(GetUsernameTemplate(fragment, username_type, new_custom_username));
		server->SetUsernameType(username_type);
		RETURN_IF_ERROR(server->SetCustomUsername(new_custom_username));
		fragment.LeaveElement();
	}
	else
	{
		server->SetUsernameType(username_type);
		RETURN_IF_ERROR(server->SetCustomUsername(custom_username));
	}

	// Get secure status
	if (fragment.EnterElement(UNI_L("secure")))
	{
		server->SetSecure(GetBoolean(fragment.GetText()));
		fragment.LeaveElement();
	}

	// Get connection type
	if (fragment.EnterElement(UNI_L("single_connection")))
	{
		server->SetForceSingleConnection(fragment.GetText());
		fragment.LeaveElement();
	}

	if (fragment.EnterElement(UNI_L("qresync")))
	{
		server->EnableQRESYNC(fragment.GetText());
		fragment.LeaveElement();
	}

	// Get IMAP-specific options
	if (fragment.EnterElement(UNI_L("imap")))
	{
		if (fragment.EnterElement(UNI_L("bodystructure")))
		{
			server->SetImapBodyStructure(GetBoolean(fragment.GetText()));
			fragment.LeaveElement();
		}

		if (fragment.EnterElement(UNI_L("idle")))
		{
			server->SetImapIdle(GetBoolean(fragment.GetText()));
			fragment.LeaveElement();
		}

		if (fragment.EnterElement(UNI_L("sent_folder")))
		{
			server->SetImapSentFoldername(fragment.GetText());
			fragment.LeaveElement();
		}

		fragment.LeaveElement();
	}

	// Add parsed server to provider
	RETURN_IF_ERROR(provider->servers.Add(server->GetProtocol(), server.get()));
	server.release();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Get authentication type based on string input
 **
 ** AccountCreator::GetAuthenticationType
 ***********************************************************************************/
AccountTypes::AuthenticationType AccountCreator::GetAuthenticationType(const OpStringC& auth_type)
{
	if (auth_type.Compare("auto") == 0)
		return AccountTypes::AUTOSELECT;
	else if (auth_type.Compare("none") == 0)
		return AccountTypes::NONE;
	else if (auth_type.Compare("plaintext") == 0)
		return AccountTypes::PLAINTEXT;
	else if (auth_type.Compare("plain") == 0)
		return AccountTypes::PLAIN;
	else if (auth_type.Compare("simple") == 0)
		return AccountTypes::SIMPLE;
	else if (auth_type.Compare("generic") == 0)
		return AccountTypes::GENERIC;
	else if (auth_type.Compare("login") == 0)
		return AccountTypes::LOGIN;
	else if (auth_type.Compare("apop") == 0)
		return AccountTypes::APOP;
	else if (auth_type.Compare("cram-md5") == 0)
		return AccountTypes::CRAM_MD5;
	else if (auth_type.Compare("sha1") == 0)
		return AccountTypes::SHA1;
	else if (auth_type.Compare("kerberos") == 0)
		return AccountTypes::KERBEROS;
	else if (auth_type.Compare("digest-md5") == 0)
		return AccountTypes::DIGEST_MD5;
	else
		return AccountTypes::NONE;
}


/***********************************************************************************
 ** Get a boolean value based on string input ('yes' or 'no')
 **
 ** AccountCreator::GetBoolean
 ***********************************************************************************/
BOOL AccountCreator::GetBoolean(const OpStringC& boolean)
{
	return boolean.Compare("yes") == 0;
}


/***********************************************************************************
 ** Parse a <username> tag
 **
 ** AccountCreator::GetUsernameTemplate
 ***********************************************************************************/
OP_STATUS AccountCreator::GetUsernameTemplate(XMLFragment& fragment, UsernameType& username_type, OpString& custom_username)
{
	OpStringC username_type_str = fragment.GetAttribute(UNI_L("use"));

	if (username_type_str.Compare("email") == 0)
		username_type = USERNAME_EMAIL;
	else if (username_type_str.Compare("user") == 0)
		username_type = USERNAME_USER;
	else if (username_type_str.Compare("custom") == 0)
	{
		username_type = USERNAME_CUSTOM;
		RETURN_IF_ERROR(custom_username.Set(fragment.GetText()));
	}

	return OpStatus::OK;
}


/////////////////////////////////////////////////////////////////////////////////////
//                           AccountCreator::Server
/////////////////////////////////////////////////////////////////////////////////////


/***********************************************************************************
 ** Constructor
 **
 ** AccountCreator::Server::Server
 ***********************************************************************************/
AccountCreator::Server::Server()
  : m_protocol(AccountTypes::UNDEFINED)
  , m_port(0)
  , m_auth(AccountTypes::NONE)
  , m_username_type(USERNAME_USER)
  , m_secure(FALSE)
  , m_imap_bodystructure(TRUE)
  , m_imap_idle(TRUE)
  , m_pop_leave_messages(TRUE)
  , m_pop_delete_permanently(TRUE)
  , m_force_single_connection(FALSE)
  , m_qresync(FALSE)
{
}


/***********************************************************************************
 ** Initialize a server, set default options
 **
 ** AccountCreator::Server::Init
 ** @param protocol Protocol to use
 ***********************************************************************************/
OP_STATUS AccountCreator::Server::Init(AccountTypes::AccountType protocol)
{
	SetProtocol(protocol);

	switch (protocol)
	{
		case AccountTypes::POP:
			m_port				 = 110;
			m_pop_leave_messages = TRUE;
			m_pop_delete_permanently = TRUE;
			m_auth				 = AccountTypes::AUTOSELECT;
			break;
		case AccountTypes::IMAP:
			m_port				 = 143;
			m_pop_leave_messages = FALSE;
			m_imap_bodystructure = TRUE;
			m_imap_idle			 = TRUE;
			m_auth				 = AccountTypes::AUTOSELECT;
			break;
		case AccountTypes::SMTP:
			m_port				 = 25;
			m_auth				 = AccountTypes::AUTOSELECT;
			break;
		case AccountTypes::NEWS:
			m_port				 = 119;
			m_auth				 = AccountTypes::NONE;
			break;
		case AccountTypes::IRC:
			m_port				 = 6667;
			m_auth				 = AccountTypes::NONE;
			break;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Set username for this server
 **
 ** AccountCreator::Server::SetUsername
 ***********************************************************************************/
OP_STATUS AccountCreator::Server::SetUsername(const OpString& username)
{
	m_username_type = USERNAME_LITERAL;
	return m_custom_username.Set(username);
}


/***********************************************************************************
 ** Get username for this server based on an email address
 **
 ** AccountCreator::Server::GetUsername
 ***********************************************************************************/
OP_STATUS AccountCreator::Server::GetUsername(const OpStringC& email_address, OpString& username) const
{
	switch (m_username_type)
	{
		case USERNAME_EMAIL:
			return username.Set(email_address);
		case USERNAME_USER:
			return username.Set(email_address.CStr(), email_address.FindLastOf('@'));
		case USERNAME_CUSTOM:
		{
			OpString user;
			OpString domain;

			// Determine user and domain part of email address
			if (email_address.HasContent())
			{
				if (!user.Reserve(email_address.Length()) || !domain.Reserve(email_address.Length()))
					return OpStatus::ERR_NO_MEMORY;

				uni_sscanf(email_address.CStr(), UNI_L("%[^@]@%s"), user.CStr(), domain.CStr());
			}

			RETURN_IF_ERROR(username.Set(m_custom_username));

			// Replace the three possible replacement characters: $u (user), $d (domain) and $e (email)
			RETURN_IF_ERROR(StringUtils::Replace(username, UNI_L("$u"), user));
			RETURN_IF_ERROR(StringUtils::Replace(username, UNI_L("$d"), domain));
			RETURN_IF_ERROR(StringUtils::Replace(username, UNI_L("$e"), email_address));

			return OpStatus::OK;
		}
		case USERNAME_LITERAL:
			return username.Set(m_custom_username);
		default:
			return OpStatus::ERR;
	}
}


/***********************************************************************************
 ** Copy from another server
 **
 ** AccountCreator::Server::CopyFrom
 ** @param server Server to copy
 ***********************************************************************************/
OP_STATUS AccountCreator::Server::CopyFrom(const Server& server)
{
	// Copy all member variables
	m_protocol			 = server.m_protocol;
	m_port				 = server.m_port;
	m_auth				 = server.m_auth;
	m_secure			 = server.m_secure;
	m_imap_bodystructure = server.m_imap_bodystructure;
	m_imap_idle			 = server.m_imap_idle;
	m_pop_leave_messages = server.m_pop_leave_messages;
	m_username_type		 = server.m_username_type;
	m_force_single_connection = server.m_force_single_connection;
	m_qresync			 = server.m_qresync;

	RETURN_IF_ERROR(m_host					.Set(server.m_host.CStr()));
	RETURN_IF_ERROR(m_custom_username		.Set(server.m_custom_username.CStr()));
	RETURN_IF_ERROR(m_password				.Set(server.m_password.CStr()));
	RETURN_IF_ERROR(m_imap_sent_foldername	.Set(server.m_imap_sent_foldername.CStr()));

	return OpStatus::OK;
}

#endif // M2_SUPPORT
