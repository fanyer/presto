/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef SSLEAY_EXT_CERTREPOSITORY_H
#define SSLEAY_EXT_CERTREPOSITORY_H

#ifdef _NATIVE_SSL_SUPPORT_
#ifdef SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY

#include "modules/libopeay/openssl/x509.h"
#include "modules/libopeay/openssl/err.h"
#include "modules/libssl/ext_certrepository.h"

/** The baseclass for OpenSSL based external certificate repositories 
 *
 *	Platforms will implement a class using the APIs of this class
 *
 *	The implementations MUST implement a reasonable timeout in case they 
 *	access remote sources for the certificates. In case of a timeout the
 *	implementation MUST disable further remote lookups for the failed 
 *	certificates for some time afterwards to allow the connection to continue;
 *	the suspension period should be at least 60 seconds.
 *
 *	To register the implementation, call the method Register(), to unregister call Unregister()
 *	Caller retains ownership of the implementation, and destruction automatically unsubscribes the implementation
 *
 *	Note regarding Error handling: In case of an unhandled error the Lookup function MUST
 *	return -1 and ERR_peek_error() != 0.
 *
 *	Errors can be investigated using ERR_peek_error(), and popped off the stack using ERR_get_error()
 *	The macros ERR_GET_LIB(), ERR_GET_FUNC() and ERR_GET_REASON() can be used to check the source and 
 *	reason of the error. The lib, func and reason IDs depend on the actual function.
 *
 *	Implementations that get errors from other sources than OpenSSL MUST make sure that an error
 *	is reported to the caller using the call 
 *			ERR_PUT_error(ERR_LIB_USER, 1,1,__FILE__,__LINE__);
 *
 *	Implementations SHOULD check regularly that ERR_peek_error() == 0, and return -1 if it isn't 0
 */
class OpenSSL_External_Repository : public SSL_External_CertRepository_Base
{
public:
#ifdef SELFTEST
	// Selftest flags to be set by implementations, can also be read and set by selftest
	/** TRUE if there has been a call to the Lookup function */
	BOOL accessed; 
	/** TRUE if the Lookup function returned a certificate */
	BOOL returned_cert;
#endif

	OpenSSL_External_Repository(): SSL_External_CertRepository_Base(SSL_External_CertRepository_Base::OpenSSL)
#ifdef SELFTEST
			, accessed(FALSE), returned_cert(FALSE)
#endif
			{}

	/** This function has the same API and meaning as the LookupFunction in OpenSSL
	 *
	 *	The implementation should avoid updating ctx and x
	 *
	 *  Return values are 
	 *		0: No certificate found
	 *		<0: Some kind of error, use ERR_put_error to indicate exact reason. If no error 
	 *			has been reported during the verification, this code is assumed to mean that an 
	 *			asynchronous lookup has been initiated, and the caller will wait for 
	 *			SignalLookupCompleted to be called.
	 *		>0: Success, issuer returned,
	 *
	 *	Regarding asynchronous lookups:
	 *		*	Multiple calls may be made for the same certificate. This MUST NOT result in multiple 
	 *			online lookups.
	 *		*	In cases where a certificate is known, but not stored locally and an online lookup fails,
	 *			the implementation must remember this, and not attempt another attempt for a certain 
	 *			amount of time to be specified by the implementation, but more than 60 seconds is recommended.
	 *
	 *	@param	issuer	Pointer to the pointer which to assign the allocated X509.  
	 *					Reference counting is used, and is assumed to include the value stored in *issuer.
	 *	@param	ctx		The ceriticate store context.
	 *	@param	x		The certificate whose issuer we are trying to find.
	 */
	virtual int LookupCertificate(X509 **issuer, X509_STORE_CTX *ctx, X509 *x)=0;

	/** This utility function converts the provided binary array string to an
	 *	OpenSSL X509 object.
	 *
	 *	@param	candidate		Pointer to an unsigned char array containing the DER encoded 
	 *							certificate to be checked. Contains candidate_len bytes.
	 *	@param	candidate_len	Length of the candidate certificate's binary representation.
	 *
	 *	@return X509 *	Pointer to the created X509 object. If NULL, there was an error, check ERR_get_error or 
	 *					ERR_peek_error for further information about what went wrong. Free using X509_free()
	 */
	static X509 *ConvertBinaryToX509(const unsigned char *candidate, size_t candidate_len){
		OP_ASSERT(candidate && candidate_len);
		
		// Create an X509 object from the candidate item
		return d2i_X509(NULL, (const unsigned char **) &candidate, candidate_len);
	}

	/** This utility function converts the provided X509 object to an allocated binary array string
	 *
	 *	@param	candidate		Rreference to a pointer where the pointer to an allocated array of unsigned chars 
	 *							containing the DER encoded certificate will be stored. Contains candidate_len bytes.
	 *							Allocated using OPENSSL_malloc(), Free using OPENSSL_free()
	 *	@param	candidate_len	Reference to location where the length of the candidate certificate's binary representation will be stored
	 *
	 *	@return BOOL	TRUE if the candidate pointer contains a valid pointer to a buffer. If FALSE there was an error, check ERR_get_error or 
	 *					ERR_peek_error for further information about what went wrong.
	 */
	static BOOL ConvertX509ToBinary(X509*x, unsigned char *&candidate, size_t &candidate_len){
		OP_ASSERT(x);

		candidate = NULL;
		candidate_len = 0;
		
		candidate_len = i2d_X509(x, &candidate);
		return (candidate != NULL);
	}

	/** This utility function checks the provided candidate certificate to discover if it issued
	 *	the certificate x, and if so returns TRUE
	 *
	 *	@param	ctx		The ceriticate store context.
	 *	@param	x		The certificate whose issuer we are trying to find.
	 *	@param	cand	Pointer to the X509 object being checked
	 */
	static BOOL CheckACandidate(X509_STORE_CTX *ctx, X509 *x, X509 *cand){
		if (!cand)
			return FALSE;
			
		OP_ASSERT(x && ctx);

		// Is this an issuer of the certificate being checked?
		if(ctx->check_issued(ctx, x, cand))
		{
			return TRUE;
		}

		return FALSE;
	}

	/** This utility function checks the provided binary array string to discover if it issued
	 *	the certificate x, and if so returns the resulting X509 object in issuer
	 *
	 *  Return values are 
	 *		0: No certificate found
	 *		<0: Some kind of error, use ERR_put_error to indicate exact reason. 
	 *		>0: Success, issuer returned
	 *
	 *	@param	issuer	Pointer to the pointer which to assign the allocated X509.  
	 *					Reference counting is used, and is assumed to include the value stored in *issuer.
	 *	@param	ctx		The ceriticate store context.
	 *	@param	x		The certificate whose issuer we are trying to find.
	 *	@param	candidate		Pointer to an unsigned char array containing the DER encoded 
	 *							certificate to be checked. Contains candidate_len bytes.
	 *	@param	candidate_len	Length of the candidate certificate's binary representation.
	 */
	static int CheckCandidate(X509 **issuer, X509_STORE_CTX *ctx, X509 *x, const unsigned char *candidate, size_t candidate_len){
		if (!candidate || !candidate_len)
			return 0; // No certificate
			
		OP_ASSERT(x && issuer);
		OP_ASSERT(!ERR_peek_error());

		// Create an X509 object from the candidate item
		X509 *cand = ConvertBinaryToX509(candidate, candidate_len);

		if(ERR_peek_error())
			return -1; // error

		if(cand)
		{
			// Is this an issuer of the certificate being checked?
			if(CheckACandidate(ctx, x, cand))
			{
				*issuer = cand;
				// If so return it
				return 1;
			}
			// else delete the object
			X509_free(cand);
		}
		return 0;
	}
};


#ifdef SSL_API_OPENSSL_REGISTER_CUSTOM_ROOT_TABLE

#include "modules/libssl/repository_item.h"

/** Implementation of OpenSSL_External_Repository that allows an application 
 *	to register a list/array of trusted certificates.
 *
 *	The certificate are X509 certificates, encoded as binary DER bytearrays.
 *
 *	Usage: Call OpenSSL_External_Trusted_Repository_Table::Create to allocate and construct 
 *	the table. 
 *
 *	To register the implementation, call the method Register(), to unregister call Unregister()
 *	Caller retains ownership of the implementation, and destruction automatically unsubscribes the implementation
 *
 *	The structure declaration of the elements can be found in "modules/libssl/repository_item.h"
 */
class OpenSSL_External_Trusted_Repository_Table : public OpenSSL_External_Repository
{
private:
	/** Pointer to the array of certificates */
	External_RepositoryItem *table;

	/** Number of items in the certificate array */
	unsigned int table_len;

protected:
	/** Constuctor. Can only be called by implementations 
	 *	
	 *	@param	tbl_list	Pointer to the certificate array. Caller retains ownership, 
	 *						but MUST NOT delete the array unless this object has been destroyed
	 *	@param	tbl_len		Number of items in the array
	 */
	OpenSSL_External_Trusted_Repository_Table(External_RepositoryItem *tbl_list,  unsigned int tbl_len) : table(tbl_list), table_len(tbl_len){}

public:
	/** Create function. Allocates and initializes an OpenSSL_External_Trusted_Repository_Table object
	 *	
	 *	@param	new_repository	Reference to pointer where the pointer to the created object will be stored.
	 *	@param	tbl_list	Pointer to the certificate array. Caller retains ownership, 
	 *						but MUST NOT delete the array unless this object has been destroyed
	 *	@param	tbl_len		Number of items in the array
	 */
	static OP_STATUS Create(OpenSSL_External_Trusted_Repository_Table *&new_repository, External_RepositoryItem *tbl_list,  unsigned int tbl_len){
		new_repository = NULL;
		OpenSSL_External_Trusted_Repository_Table * tbl = OP_NEW(OpenSSL_External_Trusted_Repository_Table, (tbl_list, tbl_len));
		RETURN_OOM_IF_NULL(tbl);

		new_repository = tbl;
		return OpStatus::OK;
	};

	virtual int LookupCertificate(X509 **issuer, X509_STORE_CTX *ctx, X509 *x){
#ifdef SELFTEST
		accessed = TRUE;
#endif
		if (!table || !table_len)
			return 0; // No certificate
			
		OP_ASSERT(ctx && x && issuer);
		OP_ASSERT(!ERR_peek_error());

		unsigned int items_left = table_len;
		External_RepositoryItem *item = table;

		while(items_left>0)
		{
			int ret = CheckCandidate(issuer, ctx, x, item->certificate, item->certificate_len);

			if(ret != 0)
			{
#ifdef SELFTEST
				if(ret>0)
					returned_cert = TRUE;
#endif
				return ret; // Non-zero means either error or that we found a certificate
			}
			// if not found try the next one
			items_left--;
			item++;
		}
		return 0;
	}
};
#endif // SSL_API_OPENSSL_REGISTER_CUSTOM_ROOT_TABLE

#endif // SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
#endif	// _NATIVE_SSL_SUPPORT_

#endif /* SSLEAY_EXT_CERTREPOSITORY_H */
