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

#ifndef _EXTERNAL_DIGEST_API_H_
#define _EXTERNAL_DIGEST_API_H_

#if defined _NATIVE_SSL_SUPPORT_ && defined EXTERNAL_DIGEST_API

#include "modules/libssl/sslenum.h"
#include "modules/libssl/external/ext_digest/external_operations.h"

#define EXTERNAL_DIGEST_ID_START 0x1000
#define EXTERNAL_IS_SESS_DIGEST(alg) (alg >= EXTERNAL_DIGEST_ID_START && ( ((int) alg) & 0x01) != 0)
#define EXTERNAL_PERFORM_INIT(hash, op, param) hash->PerformInitOperation(op, param);

class SSL_Hash;

class OpExternalDigestStatus : public OpStatus
{
public:
	enum {
		ERR_INVALID_NAME	=	USER_ERROR+1,
		ERR_INVALID_DOMAIN	=	USER_ERROR+2,
		ERR_INVALID_SPEC	=	USER_ERROR+3,
		ERR_ALGORITHM_EXIST =	USER_ERROR+4,
		ERR_ALG_NOT_FOUND	=	USER_ERROR+5
	};
};

/** Base class for implementations of externally specified digest methods */
class External_Digest_Handler : public Link, public OpReferenceCounter
{
private:
	OpStringS8 method_name;

	int parameter_uid;

	int domain_count;
	struct External_digest_domain_item{
		BOOL full_match;
		int len;
		OpStringS8 domain;

		External_digest_domain_item(){full_match = TRUE;}
	} *domains;

	int method_id;
	SSL_HashAlgorithmType algorithm_id;

public:
	External_Digest_Handler();
	virtual ~External_Digest_Handler();

	/** Construct the digest handler, also checks the validity of the domain list */
	OP_STATUS Construct(const char *meth_name, const char **domains_in, int count, int param_uid);

	/** Create a new hash object based on this specificication
	 *
	 *	@paran	digest	the created object is returned in the referenced pointer
	 */
	virtual OP_STATUS	GetDigest(SSL_Hash *& digest)=0;


	/**	Get the credentials (if any) for the given target from the external digest owner
	 *	Using Type 1 parameters
	 *
	 *	@param	retrieved_credentials	If the credentials are updated this 
	 *									parameter contain TRUE, and if not, FALSE
	 *	@param	username		The username is returned in this parameter
	 *	@param	password		The password is returned in this parameter
	 *	@param	parameter		Type 1 digest authentication structure. This gives the 
	 *							Credential Manager a context for the request.
	 */
	virtual OP_STATUS	GetCredentials(BOOL &retrieved_credentials, OpString &username, OpString &password, const void *parameter)=0;

	/** Get the name of the method */
	const OpStringC8 &Name()const{return method_name;}

	/** Is this domain in the accepted list for this specification */
	BOOL	MatchDomain(const OpStringC8 &host);

	/** Get the Method ID of this digest method */
	int		GetMethodID() const {return method_id;}

	SSL_HashAlgorithmType	GetAlgID() const {return algorithm_id;}

	External_Digest_Handler *Suc(){return (External_Digest_Handler *) Link::Suc();}
	External_Digest_Handler *Pred(){return (External_Digest_Handler *) Link::Pred();}
};


/** Get a handler for the given digest method
 *
 *	@param	name	Name of the requested method
 *	@param	hostname	Host for which the method will be used
 *	@param	op_err	The error code is returned here
 */
External_Digest_Handler *GetExternalDigestMethod(const OpStringC8 &name, const OpStringC8 &hostname, OP_STATUS &op_err);

/** Get a handler for the given digest method
 *
 *	@param	method_id	ID of the requested method
 */
External_Digest_Handler *GetExternalDigestMethod(int method_id);

/** Get a handler for the given digest method
 *
 *	@param	digest	Algorithm ID of the requested method
 */
SSL_Hash *GetExternalDigestMethod(SSL_HashAlgorithmType digest, OP_STATUS &op_err);

/** Add an external digest handler to the list. Will fail if there is a matching method already */
OP_STATUS AddExternalDigestMethod(External_Digest_Handler *spec);

void RemoveExternalDigestMethod(const OpStringC8 &name);

#else
#define EXTERNAL_PERFORM_INIT(hash, op, param) 
#endif

#endif // _EXTERNAL_DIGEST_API_H_

