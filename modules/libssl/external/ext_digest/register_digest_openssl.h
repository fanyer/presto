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

#ifndef _REGISTER_DIGEST_H_
#define _REGISTER_DIGEST_H_

#include "modules/libopeay/openssl/e_os.h"
#include "modules/libopeay/openssl/ossl_typ.h"

#include "external_operations.h"

/** Callback used by Opera to initialize a digest context, ctx, for a given operation 
 *	using the specified parameters in params
 *
 *	@param	ctx			OpenSSL 0.9.7 digest context, contains a context initialized by EVP_DigestInit
 *	@param	operation	Identifies digest operation to be started. Is one of the OP_DIGEST_OPERATION values
 *  @param	params		Parameters. The actual type of the object params is pointing to 
 *						defined by the algorithm registration's parameter_uid argument
 *
 *	@return zero(0) if sucess, otherwise a negative unspecified error code. Errors are always fatal.
 */
typedef int (*OpDigestParamInitCB)(EVP_MD_CTX *ctx, int operation, const void *params);

/** Callback into Opera's code used to set a string that will be owned by Opera
 *	
 *	@param	target		Pointer to pointer of a null-terminated string that will be updated with 
 *						the pointer to the new string. If the target location is non-NULL the memory 
 *						will be deleted.
 *	@param	source		Pointer to caller's non-NULL string that will be copies
 *
 *	@return zero(0) if sucess, otherwise a negative unspecified error code. Errors are always fatal.
 */
typedef int (*OpSetStringCB)(char **target, const char *source);

/**	Callback used by Opera to get the credentials for the context identified by the params
 *	The credentials are returned by calling the set_string_cb callback to copy them into  
 *	strings whose pointers are returned via the pointers pointed to by the credentials arguments
 *	
 *	@param	credential_name			Pointer to the pointer in which the username will be returned (UTF8 is assumed).
 *	@param	credential_password		Pointer to the pointer in which the password will be returned (UTF8 is assumed).
 *	@param	params					Parameters. The actual type of the object params is pointing to 
 *									defined by the algorithm registration's parameter_uid argument
 *	@param	set_string_cb			Callback to Opera used to create a copy of the credentials in 
 *									the credential parameters
 *
 *	@return			0	: Ask User
 *					1	: Credentials returned
 *					-1	: set_string_cb callback failed
 *					other negative codes : unspecified failure
 */
typedef int (*OpDigestGetCredentialsCB)(char**credential_name, char**credential_password, const void *params, OpSetStringCB set_string_cb);

/** Function pointer to OpenSSL's ERR_put_error function, returned to the caller that registers a digest method */
typedef void (*OpDigestPutError)(int lib, int func,int reason
#ifndef OPENSSL_NO_ERR
								 ,const char *file,int line
#endif
								 );

/** Method used to register a Digest method with Opera for the given name and the
 *	OpenSSL 0.9.7 compatible method specification  for the specified domains.
 *
 *	Caller can specify callbacks for method-specific username/password and/or initialization operations.
 *	
 *	This function automatically defines a alg-sess algorithm for RFC 2617 Digest Authentication
 *
 *
 *	The registration will fail if the method's name is empty or matches a previously 
 *	registered method, or no method specification is specified, or is missing functions
 *
 *
 *	@param		method_name		nullterminated string giving the name of the method, Names ending in "-sess" are forbidden.
 *	@param		method_spec		Pointer to an OpenSSL 0.9.7 message digest specification object
 *								This object MUST exist for the entire time the application is running
 *	@param		get_credentials	Callback to caller's code that returns the caller's credentials for the 
 *								current servername, or indicates that Opera must ask the user.
 *	@param		parameter_init	Callback to caller's code that perform required initialization before 
 *								the specified operations 
 *	@param		parameter_uid	Specifies a uniques ID that identifies the format of parameter_init's
 *								and get_credentials's params arguments.
 *	@param		domains			Pointer to a list of domain_count (at least one) pointers to null-terminated 
 *								strings that identifies which servers and domains the specified method can 
 *								be used for.
 *
 *								The check is case-insensitive.
 *
 *								Names must be IDNA (RFC 3490) compatible, and punycode encoded, if necessary.
 *
 *								- Non-dotted names and IP-addresses (IPv4 and IPv6) MUST match the servername
 *								- Names with internal dots ('.') but no leading dot MUST match the servername
 *								- Names with internal dots and a leading dot can either match the servername 
 *								with the part after the dot, or match the servername's end with the whole name, 
 *								including the leading dot.
 *	@param		domain_count	Number of names in the domain list.
 *	@param		put_error		If non-NULL: Pointer to a function pointer where Opera will put a pointer to 
 *								OpenSSL's 	ERR_put_error function, which caller can use to register errors 
 *								when the method_spec's functions are used. The call MUST NOT be used during 
 *								other operations.
 *
 *	@return		0	: Success
 *				-1	: Already have an algorithm with that name
 *				-2  : Invalid name
 *				-3	: Invalid method spec
 *				-4	: Invalid domain specification
 *					other negative codes : unspecified failure
 */
int RegisterDigestAuthenticationOpenSSLMethod(
               const char *method_name,
               const EVP_MD *method_spec,
			   OpDigestGetCredentialsCB	get_credentials,
               OpDigestParamInitCB parameter_init,
               int   parameter_uid,
               const char ** domains,
               int domain_count,
			   OpDigestPutError *put_error
     );

/** Unregisters a registered digest method 
 *	Both the name and method specification must match the registered method
 *
 *	NOTE: This will only prevent the method from being found after this call
 *	Existing use of the method will continue.
 *
 *	@params		method_name		nullterminated string giving the name of the method
 *	@params		method_spec		Pointer to an OpenSSL 0.9.7 message digest specification object
 */
void UnRegisterDigestAuthenticationOpenSSLMethod(
               const char *method_name
	);


#endif
