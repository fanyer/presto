/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ACCOUNT_CREATOR_H
#define ACCOUNT_CREATOR_H

#include "adjunct/m2/src/include/defs.h"

class XMLFragment;
class Account;

/**
 * @brief Easily create accounts for known mail providers
 * @author Arjan van Leeuwen
 *
 * This class reads from an xml file to get a list of known mail providers, identified
 * by email addresses. This can be used by the account wizard to allow easier setup
 * of mail accounts.
 *
 */
class AccountCreator
{
public:
	class Server;

	enum UsernameType
	{
		USERNAME_EMAIL,
		USERNAME_USER,
		USERNAME_CUSTOM,
		USERNAME_LITERAL
	};

	/** Initialize the mail providers, run before using any other function
	  */
	OP_STATUS Init();

	/** Check whether we know about a mail provider, and get server information for it
	  * @param email_address Mail address to search for
	  * @param protocol Protocol to find server for, use UNDEFINED to get preferred type
	  * @param server Where to output details
	  * @return OpStatus::OK if we found the server and could fill in details, error codes otherwise
	  */
	OP_STATUS GetServer(const OpStringC& email_address,
						AccountTypes::AccountType protocol,
						BOOL incoming,
						Server& server);

	/** Get reasonable default values for an incoming server based on an email address
	  * @param email_address Mail address to use
	  * @param protocol Protocol for defaults, or UNDEFINED
	  * @param server Where to output defaults
	  */
	OP_STATUS GetServerDefaults(const OpStringC& email_address,
								AccountTypes::AccountType protocol,
								Server& server);

	/** Create an account
	  * @param name Name for account (usually name of user)
	  * @param email_address Address to create an account for
	  * @param company_name Name of company of user
	  * @param account_name Name of the account
	  * @param incoming_server Server details for incoming data
	  * @param outgoing_server Server details for outgoing data
	  * @param engine Engine to use for creating account
	  * @param new_account The created account
	  * @return OpStatus::OK if account was successfully created, error codes otherwise
	  */
	OP_STATUS CreateAccount(const OpStringC& name,
							const OpStringC& email_address,
							const OpStringC& company_name,
							const OpStringC& account_name,
							const Server& incoming_server,
							const Server& outgoing_server,
							MessageEngine* engine,
							Account*& new_account);

	class Server
	{
	public:
		/** Constructor
		  */
		Server();

		/** Initialize a server - will set default values for this protocol
		  * @param protocol Protocol to use
		  */
		OP_STATUS Init(AccountTypes::AccountType protocol);

		/** Set the protocol to a specific type
		  * @param protocol Protocol to set
		  */
		void	  SetProtocol(AccountTypes::AccountType protocol) { m_protocol = protocol; }

		/** Set the hostname
		  */
		OP_STATUS SetHost(const OpStringC& host) { return m_host.Set(host); }

		/** if port > 0, Set the port to use to connect to hostname
		  */
		void	  SetPort(unsigned port) { if (port) m_port = port; }

		/** Set the authentication type to use on this server
		  */
		void	  SetAuthenticationType(AccountTypes::AuthenticationType auth) { m_auth = auth; }

		/** Set a username template for this server (for automatic fill-in of username)
		  */
		void	  SetUsernameType(UsernameType username_type) { m_username_type = username_type; }

		/** Set a custom username template for this server (for automatic fill-in of username)
		  */
		OP_STATUS SetCustomUsername(const OpStringC& custom) { return m_custom_username.Set(custom); }

		/** Set username for this server
		  */
		OP_STATUS SetUsername(const OpString& username);

		/** Set password for this server
		  */
		OP_STATUS SetPassword(const OpString& password) { return m_password.Set(password); }

		/** Set whether to use secure connection
		  */
		void	  SetSecure(BOOL secure) { m_secure = secure; }

		/** Set whether it's possible to use IMAP bodystructure support
		  */
		void	  SetImapBodyStructure(BOOL body_structure) { m_imap_bodystructure = body_structure; }

		/** Set whether it's possible to use IMAP IDLE
		  */
		void	  SetImapIdle(BOOL idle) { m_imap_idle = idle; }

		/** Set IMAP sent folder name for this server
		  */
		OP_STATUS		SetImapSentFoldername(const OpStringC& sent_folder) { return m_imap_sent_foldername.Set(sent_folder.CStr()); }

		/** Set whether we should leave messages on server in case of POP
		  */
		void	  SetPopLeaveMessages(BOOL leave_messages) { m_pop_leave_messages = leave_messages; }

		/** Set whether to delete messages on the server when trash is emptied
		*/
		void	  SetPopDeletePermanently(BOOL delete_permanently) { m_pop_delete_permanently = delete_permanently; }

		/** Get the protocol type
		 */
		AccountTypes::AccountType GetProtocol() const { return m_protocol; }

		/** Get the hostname
		  */
		const OpStringC& GetHostname() const { return m_host; }

		/** Get the port to use to connect to hostname
		  */
		unsigned  GetPort() const { return m_port; }

		/** Get the authentication type to use on this server
		  */
		AccountTypes::AuthenticationType GetAuthenticationType() const { return m_auth; }

		/** Get username for this server
		  */
		const OpStringC& GetUsername() const { return m_custom_username; }

		/** Get username for this server based on an email address
		  */
		OP_STATUS		 GetUsername(const OpStringC& email_address, OpString& username) const;

		/** Get password for this server
		  */
		const OpStringC& GetPassword() const { return m_password; }

		/** Get whether to use a secure connection
		  */
		BOOL	  GetSecure() const { return m_secure; }

		/** Get whether IMAP bodystructure is available for this server
		  */
		BOOL	  GetImapBodyStructure() const { return m_imap_bodystructure; }

		/** Get whether IMAP IDLE is available for this server
		  */
		BOOL	  GetImapIdle() const { return m_imap_idle; }

		/** Get imap sent folder name for this server based on an email address
		  */
		const OpStringC&	GetImapSentFoldername() const { return m_imap_sent_foldername; }

		/** Get whether to leave messages on server in case of POP
		  */
		BOOL	  GetPopLeaveMessages() const { return m_pop_leave_messages; }

		/** Get whether to delete messages on the server when trash is emptied
		*/
		BOOL	  GetPopDeletePermanently() const { return m_pop_delete_permanently; }

		/** Copy from another server
		  * @param server Server to copy
		  */
		OP_STATUS CopyFrom(const Server& server);

		/** Set account information for incoming or outgoing server from this server
		  * @param account Account to change
		  * @param incoming Whether to change incoming or outgoing server information
		  */
		OP_STATUS SetAccountInfo(Account* account, BOOL incoming);

		void SetForceSingleConnection(const OpStringC& value) { m_force_single_connection = value.Compare("true") == 0; };
		BOOL GetForceSingleConnection() const { return m_force_single_connection; }

		void EnableQRESYNC(const OpStringC& value) { m_qresync = value.Compare("true") == 0; }
		BOOL IsQRESYNCEnabled() const { return m_qresync; }

	private:
		AccountTypes::AccountType		 m_protocol;
		OpString						 m_host;
		unsigned						 m_port;
		AccountTypes::AuthenticationType m_auth;
		OpString						 m_password;
		UsernameType					 m_username_type;
		OpString						 m_custom_username;
		BOOL							 m_secure;
		BOOL							 m_imap_bodystructure;
		BOOL							 m_imap_idle;
		OpString						 m_imap_sent_foldername;
		BOOL							 m_pop_leave_messages;
		BOOL							 m_pop_delete_permanently;
		BOOL							 m_force_single_connection;
		BOOL							 m_qresync;
	};

private:
	struct MailProvider
	{
		MailProvider() : preferred_protocol(AccountTypes::UNDEFINED) {}

		OpAutoVector<OpString>		 domains;
		AccountTypes::AccountType	 preferred_protocol;
		OpAutoINT32HashTable<Server> servers;
	};

	OP_STATUS GetMailProviders(XMLFragment& fragment);
	OP_STATUS GetServer(XMLFragment& fragment, MailProvider* provider, UsernameType username_type, const OpStringC& custom_username);
	AccountTypes::AuthenticationType GetAuthenticationType(const OpStringC& auth_type);
	BOOL	  GetBoolean(const OpStringC& boolean);
	OP_STATUS GetUsernameTemplate(XMLFragment& fragment, UsernameType& username_type, OpString& custom_username);

	OpAutoVector<MailProvider>		 m_providers;		 ///< List of providers
	OpStringHashTable<MailProvider>  m_domain_providers; ///< Providers indexed by domain name
};

#endif // ACCOUNT_CREATOR_H
