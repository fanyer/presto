/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl), Frode Gill (frodegill)
 */

#ifndef _AUTHENTICATE_H_
#define _AUTHENTICATE_H_

#include "adjunct/m2/src/include/defs.h"

#include "adjunct/m2/src/util/misc.h"

/** @brief Manages sensitive information
  *
  * Retrieves login information (username, password) for a backend
  * and safely deletes it when the object is destroyed.
  */
class OpAuthenticate
{
	public:
		/** Initialization - call this function and check return value before using values
		  * @param auth_type Type of authentication needed
		  * @param protocol_type Protocol that's used
		  * @param username Username for authentication - will be wiped
		  * @param password Password for authentication - will be wiped
		  * @param challenge Input if authentication method will need it (e.g. challenge-response auths)
		  */
		OP_STATUS Init(AccountTypes::AuthenticationType auth_type,
					   AccountTypes::AccountType protocol_type,
					   const OpStringC& username,
					   const OpStringC& password,
					   const OpStringC8& challenge = NULL);

		/** Get processed username (for applicable authentication types)
		  */
		const OpStringC8& GetUsername() { return m_username; }

		/** Get processed password (for applicable authentication types)
		  */
		const OpStringC8& GetPassword() { return m_password; }

		/** Get processed response (for applicable authentication types, i.e. those that require a challenge)
		  */
		const OpStringC8& GetResponse() { return m_response; }

	private:


		OP_STATUS PlainText(const OpStringC& username, const OpStringC& password, BOOL quote);
		OP_STATUS AuthPlain(const OpStringC& username, const OpStringC& password);
		OP_STATUS AuthLogin(const OpStringC& username, const OpStringC& password);
		OP_STATUS AuthAPOP(const OpStringC& username, const OpStringC& password, const OpStringC8& challenge);
		OP_STATUS AuthCramMD5(const OpStringC& username, const OpStringC& password, const OpStringC8& challenge);

		OpMisc::SafeString<OpString8> m_username;
		OpMisc::SafeString<OpString8> m_password;
		OpMisc::SafeString<OpString8> m_response;
};

#endif
