/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl), Frode Gill (frodegill)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/util/authenticate.h"

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/util/qp.h"


/***********************************************************************************
 ** Initialization - call this function and check return value before using values
 **
 ** OpAuthenticate::Init
 ** @param auth_type Type of authentication needed
 ** @param protocol_type Protocol that's used
 ** @param username Username for authentication - will be wiped
 ** @param password Password for authentication - will be wiped
 ** @param challenge Input if authentication method will need it (e.g. challenge-response auths)
 ***********************************************************************************/
OP_STATUS OpAuthenticate::Init(AccountTypes::AuthenticationType auth_type,
							   AccountTypes::AccountType protocol_type,
							   const OpStringC& username,
							   const OpStringC& password,
							   const OpStringC8& challenge)
{
	OP_STATUS ret = OpStatus::OK;

	// Process information
	switch(auth_type)
	{
		case AccountTypes::PLAIN:
			ret = AuthPlain(username, password);
			break;
		case AccountTypes::SIMPLE:
			// TODO
			ret = OpStatus::ERR;
			break;
		case AccountTypes::GENERIC:
			// TODO
			ret = OpStatus::ERR;
			break;
		case AccountTypes::LOGIN:
			ret = AuthLogin(username, password);
			break;
		case AccountTypes::APOP:
			ret = AuthAPOP(username, password, challenge);
			break;
		case AccountTypes::CRAM_MD5:
			ret = AuthCramMD5(username, password, challenge);
			break;
		case AccountTypes::SHA1:
			// TODO
			ret = OpStatus::ERR;
			break;
		case AccountTypes::KERBEROS:
			// TODO
			ret = OpStatus::ERR;
			break;
		case AccountTypes::DIGEST_MD5:
			// TODO
			ret = OpStatus::ERR;
			break;
		case AccountTypes::PLAINTEXT:
		default:
			ret = PlainText(username, password, protocol_type != AccountTypes::POP && protocol_type != AccountTypes::IRC && protocol_type != AccountTypes::NEWS);
	}

	return ret;
}


/***********************************************************************************
 ** Encode username and password for usage in 'plain text'
 **
 ** OpAuthenticate::PlainText
 ***********************************************************************************/
OP_STATUS OpAuthenticate::PlainText(const OpStringC& username, const OpStringC& password, BOOL quote)
{
	RETURN_IF_ERROR(m_username.Set(username));
	RETURN_IF_ERROR(m_password.Set(password));

	if (quote)
	{
		OpMisc::SafeString<OpString8> unquoted_username, unquoted_password;

		unquoted_username.TakeOver(m_username);
		unquoted_password.TakeOver(m_password);

		RETURN_IF_ERROR(OpMisc::QuoteString(unquoted_username, m_username));
		RETURN_IF_ERROR(OpMisc::QuoteString(unquoted_password, m_password));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Create encoded response (contains both username and password) for AUTH PLAIN
 **
 ** OpAuthenticate::AuthPlain
 ***********************************************************************************/
OP_STATUS OpAuthenticate::AuthPlain(const OpStringC& username, const OpStringC& password)
{
	// According to RFC2595, charset must be utf-8 - encode
	int			inv_count;
	OpString	inv_chars;
	OpMisc::SafeString<OpString8>	username_utf8, password_utf8;
	OpString8	charset;

	RETURN_IF_ERROR(charset.Set("utf-8"));
	RETURN_IF_ERROR(MessageEngine::ConvertToChar8(charset, username, username_utf8, inv_count, inv_chars));
	RETURN_IF_ERROR(MessageEngine::ConvertToChar8(charset, password, password_utf8, inv_count, inv_chars));

	// Make sure encoded username and password have some content
	if (username_utf8.IsEmpty())
		RETURN_IF_ERROR(username_utf8.Set(""));
	if (password_utf8.IsEmpty())
		RETURN_IF_ERROR(password_utf8.Set(""));

	// Create unencoded response
	OpMisc::SafeString<OpString8> response;

	// Response will have '\0'-characters, use '?' first and replace later
	RETURN_IF_ERROR(response.AppendFormat("?%s?%s", username_utf8.CStr(), password_utf8.CStr()));
	int response_length = response.Length();

	response[username_utf8.Length() + 1] = '\0';
	response[0]							 = '\0';

	// Encode response into Base64
	return OpQP::Base64Encode(response.CStr(), response_length, m_response);
}


/***********************************************************************************
 ** Create encoded username and password for AUTH LOGIN
 **
 ** OpAuthenticate::AuthLogin
 ***********************************************************************************/
OP_STATUS OpAuthenticate::AuthLogin(const OpStringC& username, const OpStringC& password)
{
	OpMisc::SafeString<OpString8> unencoded_username, unencoded_password;

	RETURN_IF_ERROR(unencoded_username.Set(username));
	RETURN_IF_ERROR(unencoded_password.Set(password));

	// Username / password can't be empty
	if (unencoded_username.IsEmpty())
		RETURN_IF_ERROR(unencoded_username.Set("\"\""));
	if (unencoded_password.IsEmpty())
		RETURN_IF_ERROR(unencoded_password.Set("\"\""));

	// Encode
	RETURN_IF_ERROR(OpQP::Base64Encode(unencoded_username.CStr(), unencoded_username.Length(), m_username));
	RETURN_IF_ERROR(OpQP::Base64Encode(unencoded_password.CStr(), unencoded_password.Length(), m_password));

	return OpStatus::OK;
}


/***********************************************************************************
 ** Create encoded response for APOP (given a challenge)
 **
 ** OpAuthenticate::AuthAPOP
 ***********************************************************************************/
OP_STATUS OpAuthenticate::AuthAPOP(const OpStringC& username, const OpStringC& password, const OpStringC8& challenge)
{
	// The RFC does not define unicode passwords, convert directly to OpString8
	OpMisc::SafeString<OpString8> username8, password8, tmp_answer, tmp_md5_answer;

	RETURN_IF_ERROR(username8.Set(username));
	RETURN_IF_ERROR(password8.Set(password));

	if (username8.IsEmpty())
		RETURN_IF_ERROR(username8.Set(""));

	// Calculate checksum for challenge and password
	RETURN_IF_ERROR(tmp_answer.Set(challenge));
	RETURN_IF_ERROR(tmp_answer.Append(password8));
	RETURN_IF_ERROR(OpMisc::CalculateMD5Checksum(tmp_answer.CStr(), tmp_answer.Length(), tmp_md5_answer));

	// Create the response ("username checksum")
	return m_response.AppendFormat("%s %s", username8.CStr(), tmp_md5_answer.CStr());
}


/***********************************************************************************
 ** Create encoded response for AUTH CRAM-MD5 (given a challenge)
 **
 ** OpAuthenticate::AuthCramMD5
 ***********************************************************************************/
OP_STATUS OpAuthenticate::AuthCramMD5(const OpStringC& username, const OpStringC& password, const OpStringC8& challenge)
{
	OpString16 decodedChallenge;
	OpString8  decodedChallenge8;
	BOOL	   warn, err;
	const BYTE* dummy_stringptr = reinterpret_cast<const BYTE*>(challenge.CStr());

	// BASE64-decode the challenge - Ignore errors, do the best out of what we get
	OpStatus::Ignore(OpQP::Base64Decode(dummy_stringptr, "us-ascii", decodedChallenge, warn, err));
	RETURN_IF_ERROR(decodedChallenge8.Set(decodedChallenge.CStr()));

	// Prepare pads
	const int KEY_LENGTH = 64;
	OpMisc::SafeString<OpString8> key_ipad, key_opad, password8;

	RETURN_IF_ERROR(password8.Set(password));
	if (!key_ipad.Reserve(KEY_LENGTH) || !key_opad.Reserve(KEY_LENGTH))
		return OpStatus::ERR_NO_MEMORY;

	// Using strncpy fills remainder of pads with '\0'-characters. Otherwise, pads need no termination
	op_strncpy(key_ipad.CStr(), password8.CStr() ? password8.CStr() : "[Miffo]", KEY_LENGTH);
	op_strncpy(key_opad.CStr(), password8.CStr() ? password8.CStr() : "[Miffo]", KEY_LENGTH);

	// do XOR on pads
	int i;
	for (i = 0; i < KEY_LENGTH; i++)
	{
		key_ipad.CStr()[i] ^= 0x36;
		key_opad.CStr()[i] ^= 0x5c;
	}

	// Do MD5 checksum - part 1
	OpMisc::SafeString<OpString8> md5input;
	OpString8  md5output;

	if (!md5input.Reserve(KEY_LENGTH + decodedChallenge8.Length() + 1))
		return OpStatus::ERR_NO_MEMORY;

	op_memcpy(md5input.CStr(), key_ipad.CStr(), KEY_LENGTH);
	op_memcpy(md5input.CStr() + KEY_LENGTH, decodedChallenge8.CStr(), decodedChallenge8.Length());
	md5input.CStr()[KEY_LENGTH + decodedChallenge8.Length()] = '\0';

	RETURN_IF_ERROR(OpMisc::CalculateMD5Checksum(md5input.CStr(), KEY_LENGTH + decodedChallenge8.Length(), md5output));

	// Do MD5 checksum - part 2
	OpMisc::SafeString<OpString8> md5input2;
	OpString8  md5output2;
	BYTE*      buf;
	unsigned   buffer_len;

	// Convert the text buffer back to 16 bytes to get new input
	RETURN_IF_ERROR(HexStrToByte(md5output, buf, buffer_len));

	if (!md5input2.Reserve(KEY_LENGTH + 16))
		return OpStatus::ERR_NO_MEMORY;

	op_memcpy(md5input2.CStr(), key_opad.CStr(), KEY_LENGTH);
	op_memcpy(md5input2.CStr() + KEY_LENGTH, buf, MIN(buffer_len, 16));
	md5input2.CStr()[KEY_LENGTH + 16] = '\0';
	OP_DELETEA(buf);

	RETURN_IF_ERROR(OpMisc::CalculateMD5Checksum(md5input2.CStr(), KEY_LENGTH + 16, md5output2));

	// Prepare the Base64-encoded response
	OpMisc::SafeString<OpString8> response;

	RETURN_IF_ERROR(response.Set(username.HasContent() ? username.CStr() : UNI_L("[Miffo]")));
	RETURN_IF_ERROR(response.AppendFormat(" %s", md5output2.CStr()));

	return OpQP::Base64Encode(response.CStr(), response.Length(), m_response);
}

#endif //M2_SUPPORT
