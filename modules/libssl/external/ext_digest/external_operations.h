/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _EXTERNAL_OPERATIONS_H_
#define _EXTERNAL_OPERATIONS_H_

/** Unspecified operation */
#define OP_DIGEST_OPERATION_UNSPECIFIED		0
/** The object will be used to perform a RFC 2617 Digest Authentication H(A1) operation or 
 *	the H(user:realm:passwd) part of the session H(A1) operation that will be completed 
 *	by the OP_DIGEST_OPERATION_AUTH_H_SESS operation
 */
#define OP_DIGEST_OPERATION_AUTH_H_A1		1
/** The object will be used to perform a RFC 2617 Digest Authentication H(A2) operation */
#define OP_DIGEST_OPERATION_AUTH_H_A2		2
/** The object will be used to perform a RFC 2617 Digest Authentication KD(secret, data) operation */
#define OP_DIGEST_OPERATION_AUTH_KD			3
/** The object will be used to perform a RFC 2617 Digest Authentication session H(A1) calculation */
#define OP_DIGEST_OPERATION_AUTH_H_SESS		4
/** The object will be used to perform a RFC 2617 Digest Authentication H(entity-body) part of the qop=auth-int H(A2) operation */
#define OP_DIGEST_OPERATION_AUTH_H_ENTITY	5
/** The object will be used to perform a RFC 2617 Digest Authentication KD(secret, data) operation */
#define OP_DIGEST_OPERATION_AUTH_RESP_KD	6
/** The object will be used to perform a RFC 2617 Digest Authentication H(A2) response auth operation */
#define OP_DIGEST_OPERATION_AUTH_RESP_H_A2	7


/** Params type 1 is OpParamsType_1 */
#define OP_PARAMS_TYPE_1 1

/** Parameter struct for type 1 parameter object to perameter_init and get_credentials
 *	Please NOTE: Not all information in this structure will be available at each call.
 *	The minimum present will be what will be used in the operation being started.
 *
 *	!!! The strings being reference in this structure MUST NOT be modified. !!!
 */
struct OpParamsStruct1
{
	/** Digest URI parameter */
	const char *digest_uri;
	/** Server name (target for authentication) */
	const char *servername;
	/** Port number for request (target for authentication)*/
	unsigned int port_number;
	/** HTTP method used */
	const char *method;
	
	/** Realm parameter */
	const char *realm;

	/** Username */
	const char *username;

	/** Password */
	const char *password;

	/** Nonce value */
	const char *nonce;

	/** Nonce counter value */
	unsigned int nc_value;

	/** CNonce value */
	const char *cnonce;

	/** Qop value */
	const char *qop_value;
	
	/** opaque value */
	const char *opaque;
};

/** Type definition for parameter */
typedef OpParamsStruct1 *OpParamsType_1;

#endif /* _EXTERNAL_OPERATIONS_H_ */
