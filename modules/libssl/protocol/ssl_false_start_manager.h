/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SSL_FALSE_START_BLACK_LIST_H_
#define SSL_FALSE_START_BLACK_LIST_H_

#ifdef LIBSSL_ENABLE_SSL_FALSE_START

// Defines the OpFileFolder for SSL_FALSE_START_BLACK_LIST_FILE_NAME
#define SSL_FALSE_START_BLACK_LIST_FOLDER OPFILE_RESOURCES_FOLDER

#define SSL_FALSE_START_BLACK_LIST_FILE_NAME "ssl_false_start_blacklist.txt"

/* Key exchange algorithm white list.
 *
 * https://tools.ietf.org/html/draft-bmoeller-tls-falsestart-00#section-6.2
 *
 * The list given in the specification is a minimum secure white list. The list
 * given in specification only contains algorithms with forward security.
 * However, applications can for efficiency add other key exchange algorithms.
 * We add SSL_RSA_KEA as it is widely used and considered secure.
 *
 *
 * This list will never grow big, so we define it simply as a boolean expression. */
#define SSL_FALSE_START_KEY_EXCHANGE_WHITELIST \
		(\
				current_key_exchange_algorithm == SSL_RSA_KEA || \
				current_key_exchange_algorithm == SSL_Ephemeral_Diffie_Hellman_KEA ||  \
				current_key_exchange_algorithm == SSL_Diffie_Hellman_KEA \
		)

class SSL_ConnectionState;

/**
 * Manager that controls the usage of SSL false start.
 *
 * https://tools.ietf.org/html/draft-bmoeller-tls-falsestart-00
 *
 * Before we can use ssl false start on a connection, certain
 * conditions must be satisfied.
 *
 * The encryption methods must be strong, and the server
 * must not be in the black list read up from file BLACK_LIST_FILE_NAME
 *
 */
class SSLFalseStartManager
{
public:
	/**
	 * Creates the SSL false start manager.
	 *
	 * Reads up the back list BLACK_LIST_FILE_NAME, which
	 * contains servers that cannot handle ssl false start.
	 */
	static OP_STATUS Make(SSLFalseStartManager *&manager);

	/**
	 * Checks if we can use false start on this connection.
	 *
	 * https://tools.ietf.org/html/draft-bmoeller-tls-falsestart-00
	 *
	 * @return 	FALSE if ssl false start is turned off via preferences,
	 *          FALSE if symmetric cipher and key exchange method is not white listed,
	 *          FALSE if client certificate is present and the signing method is not white listed,
	 *          FALSE is the server is blacklisted, meaning it doesn't handle false start,
	 *          TRUE otherwise.
	 */
	BOOL ConnectionApprovedForSSLFalseStart(SSL_ConnectionState *connection);

	virtual ~SSLFalseStartManager(){}

private:
	/**
	 * Checks server black list for SSL false start.
	 *
	 * Not all servers handles SSL false start.
	 *
	 * Will first test domain, and then domain:port
	 * That means that if a domain is listed without port
	 * the whole domain is blocked. If port is represented,
	 * only the domain:port will be blocked
	 *
	 * @return TRUE if server is black listed.
	 */
	BOOL IsBlackListed(const char* server, int port = 443);

	/**
	 * Check if the cipher suite for this connection
	 * is approved for SSL false start.
	 *
	 * Uses a white list to prevent future ciphers to be
	 * approved by default.
	 *
	 * Implements 6.1 and 6.2 in
	 * https://tools.ietf.org/html/draft-bmoeller-tls-falsestart-00#section-6.1
	 *
	 * @return TRUE if cipher suite is approved.
	 */
	BOOL SSLFalseStartCipherSuitesApproved(SSL_ConnectionState *connection);

	/**
	 * Load blacklist from path defined by
	 * <SSL_FALSE_START_BLACK_LIST_FOLDER>/<SSL_FALSE_START_BLACK_LIST_FILE_NAME>
	 *
	 * where SSL_FALSE_START_BLACK_LIST_FOLDER is an OpFolder as defined
	 * in the top of this file.
	 *
	 * @return OpStatus::OK,
	 *         OpStatus::FILE_NOT_FOUND,
	 *         OpStatus::ERR for generic error.
	 */
	OP_STATUS LoadBlackList();

	SSLFalseStartManager() : m_file_loaded(FALSE) {};

	OpAutoString8HashTable<OpString8> m_black_list;

	BOOL m_file_loaded;
};
#endif // LIBSSL_ENABLE_SSL_FALSE_START

#endif // SSL_FALSE_START_BLACK_LIST_H_
