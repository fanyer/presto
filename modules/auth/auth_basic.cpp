/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#ifdef _ENABLE_AUTHENTICATE

#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/auth/auth_basic.h"

#include "modules/util/cleanse.h"


#if defined DEBUG && defined YNP_WORK 

#include "modules/olddebug/tstdump.h"

#define DEBUG_AUTH
#endif

int UU_encode (unsigned char* bufin, unsigned int nbytes, char* bufcoded);
OP_STATUS GenerateBasicAuthenticationString(OpString8 &credential, OpStringC8 _user, OpStringC8 passwd);

#ifndef CLEAR_PASSWD_FROM_MEMORY
#error "For security reasons FEATURE_CLEAR_PASSWORDS *MUST* be YES when HTTP Authentication is used"
#endif


// Basic_AuthElm for Basic (simple) Authentication, also used for FTP and News

Basic_AuthElm::Basic_AuthElm(unsigned short a_port, AuthScheme a_scheme, URLType a_type, BOOL authenticate_once)
	: AuthElm_Base(a_port, a_scheme, a_type, authenticate_once)
{
}

OP_STATUS Basic_AuthElm::Construct(OpStringC8 a_realm, OpStringC8 a_user, OpStringC8 a_passwd)
{
	RETURN_IF_ERROR(AuthElm_Base::Construct(a_realm, a_user));
	return Basic_AuthElm::SetPassword(a_passwd);
}

Basic_AuthElm::~Basic_AuthElm()
{
	auth_string.Wipe();
}

OP_STATUS Basic_AuthElm::SetPassword(OpStringC8 a_passwd)
{
	RETURN_IF_ERROR(password.Set(a_passwd));
	auth_string.Wipe();
	auth_string.Empty();

	if(Basic_AuthElm::GetScheme() & (AUTH_SCHEME_FTP))
		return auth_string.Set(a_passwd);

	return MakeAuthString(OpStringC8(Basic_AuthElm::GetUser()), a_passwd);
}

OP_STATUS Basic_AuthElm::MakeAuthString(OpStringC8 _user, OpStringC8 passwd)
{
    // encode "user:passwd"

	return GenerateBasicAuthenticationString(auth_string, _user, passwd);
}


OP_STATUS Basic_AuthElm::GetAuthString(OpStringS8 &ret_str, URL_Rep *url, HTTP_Request *http)
{
	return ret_str.Set(auth_string);
}

OP_STATUS Basic_AuthElm::GetProxyConnectAuthString(OpStringS8 &ret_str, URL_Rep *, Base_request_st *request, HTTP_Request_digest_data &proxy_digest)
{
	return ret_str.Set(auth_string);
}

const char* Basic_AuthElm::GetPassword() const
{
	return password.CStr();
}

/** 
 *	Creates a string containing "Basic " followed by "username:password" Base64 encoded
 *  Uses  all three normal MemoryManager TempBufs.
 *
 *	@param	credential	Contains the result of the conversion
 *	@param	_user		The Username part of the credential
 *	@param	_passwd		The Password part of the credential
 */
OP_STATUS GenerateBasicAuthenticationString(OpString8 &credential, OpStringC8 _user, OpStringC8 passwd)
{
	OP_STATUS op_err = OpStatus::OK;

	credential.Wipe();
	credential.Empty();

    if (!_user.CStr() ||
		((unsigned int) (_user.Length()+passwd.Length() + 2) >
			g_memory_manager->GetTempBufLen()))
		return OpStatus::OK;

    char *user_passwd = (char*)g_memory_manager->GetTempBuf();
	user_passwd[0] = '\0';
	StrMultiCat(user_passwd, _user.CStr(),":",passwd.CStr()); //  19/01/98 YNP
    int user_passwd_len = op_strlen(user_passwd);         //  19/01/98 YNP

    int auth_str_len = ((user_passwd_len+3)*4)/3 + 1;
    char *auth_str = (char*)g_memory_manager->GetTempBuf2();
    if (auth_str_len < (int)g_memory_manager->GetTempBuf2Len())
	{
		UU_encode((unsigned char*)user_passwd, user_passwd_len, auth_str);

		char *auth_final = credential.Reserve(auth_str_len + 6);
		if(auth_final != NULL)
		{
			*auth_final = '\0';
			StrMultiCat(credential.CStr(), "Basic ",auth_str);
		}
		else
			op_err = OpStatus::ERR_NO_MEMORY;

		OPERA_cleanse_heap(auth_str, auth_str_len);
	}
	else
		auth_str = NULL;

	OPERA_cleanse_heap(user_passwd, user_passwd_len);

    return op_err;
}



// The following code is taken from libwww
static const char six2pr[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','+','/'
};

/*--- function UU_encode -----------------------------------------------
 *
 *   Encode a single line of binary data to a standard format that
 *   uses only printing ASCII characters (but takes up 33% more bytes).
 *
 *    Entry    bufin    points to a buffer of bytes.  If nbytes is not
 *                      a multiple of three, then the byte just beyond
 *                      the last byte in the buffer must be 0.
 *             nbytes   is the number of bytes in that buffer.
 *                      This cannot be more than 48.
 *             bufcoded points to an output buffer.  Be sure that this
 *                      can hold at least 1 + (4*nbytes)/3 characters.
 *
 *    Exit     bufcoded contains the coded line.  The first 4*nbytes/3 bytes
 *                      contain printing ASCII characters representing
 *                      those binary bytes. This may include one or
 *                      two '=' characters used as padding at the end.
 *                      The last byte is a zero byte.
 *             Returns the number of ASCII characters in "bufcoded".
 */
int UU_encode (unsigned char* bufin, unsigned int nbytes, char* bufcoded)
{
/* ENC is the basic 1 character encoding function to make a char printing */
#define ENC(c) six2pr[c]

    register char *outptr = bufcoded;
    unsigned int i;

    for (i=0; i<nbytes; i += 3) {
        *(outptr++) = ENC(*bufin >> 2);            /* c1 */
        *(outptr++) = ENC(((*bufin << 4) & 060) | ((bufin[1] >> 4) & 017)); /*c2*/
        *(outptr++) = ENC(((bufin[1] << 2) & 074) | ((bufin[2] >> 6) & 03));/*c3*/
        *(outptr++) = ENC(bufin[2] & 077);         /* c4 */

        bufin += 3;
    }

    /* If nbytes was not a multiple of 3, then we have encoded too
    * many characters.  Adjust appropriately.
    */
    if(i == nbytes+1) {
        /* There were only 2 bytes in that last group */
        outptr[-1] = '=';
    } else if(i == nbytes+2) {
        /* There was only 1 byte in that last group */
        outptr[-1] = '=';
        outptr[-2] = '=';
    }
    *outptr = '\0';
    return(outptr - bufcoded);
}

#endif
