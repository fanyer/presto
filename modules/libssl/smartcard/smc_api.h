/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file describes the generic Smartcard API used by the Opera web browser.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

/** WARNING!! This file is under development and may be changed without notice */

/** SSL_SmartCardSession : Application dependent, opaque,  descriptor */

typedef void *SSL_SmartCardSession;

/** SSL_No_Session : Used to check for error conditions */

#define SSL_No_Session (SSL_SmartCardSession) NULL

/** 
 * SSL_SmartCardUnit : Application dependent, opaque,  identifies smartcard unit
 * only used when opening new session. If this ID is required by the API, 
 * it must be included in the session ID
 */

typedef void *SSL_SmartCardUnit;


/**
 * SC_API_PaddingStrategy : What kind of padding is to be used during encryption
 * or is expected during decryption
 */

typedef int SC_API_PaddingStrategy

/** Use the default padding method for the given algorithm */

#define PAD_DEFAULT			0

/** Use PKCS 1 Padding for RSA encryption */

#define PAD_RSA_PKCS1		0

/** Use PKCS 1 Padding for RSA encryption, but use 8 0x03 bytes 
 * in the leftmost part of the padded string (used in SSL v2/TLS 1.0)
 */

#define PAD_RSA_SSL_V23		1


/** 
 * SC_API_KEYUSAGE what type of operations can a given key in a card be used for? 
 * The value is a OR of the SC_API_KEYUSAGE_* defines
 */

typedef int SC_API_KEYUSAGE

/** SC_API_KEYUSAGE_ANY : no usage has been defined, any use permitted */

#define SC_API_KEYUSAGE_ANY					0x00

/** SC_API_KEYUSAGE_KEYNEGOTIATION : Key can be used for keynegotiation */

#define SC_API_KEYUSAGE_KEYNEGOTIATION		0x01

/** SC_API_KEYUSAGE_AUTHENTICATION : Key can be used for authentication */

#define SC_API_KEYUSAGE_AUTHENTICATION		0x02

/** SC_API_KEYUSAGE_SIGNING : Key can be used for digital signatures */

#define SC_API_KEYUSAGE_SIGNING				0x04



/**
 * SmartCardAPI : Pointers to the functions reqired to handle the Smartcard 
 *
 * NOTE: The API handler MUST be able to determine if the card in the reader 
 * has been replaced after the session was started, and if so, fail any 
 * operation for that session afterwards.
 */

struct SmartCardAPI
{
	/** 
	 * Identifies the exact smart card unit, used if the same 
	 * open_session API function can be used for multiple readers
	 */

	SSL_SmartCardUnit card_unit;

	/**
	 * Opens a new Smartcard Session
	 *
	 * Argument:
	 *
	 *   unit :			The opaque smartcard API specific identifier, used 
	 *					to identify  the card to use. This is the same value 
	 *					stored in the API structure.
	 *
	 * Return Value:
	 *
	 *   An opaque smartcard session identificator.
	 *
	 *   In case of a session could not be started SSL_No_Session is returned and
	 *   card_get_last_error will return the relevant error code.
	 */

	SSL_SmartCardSession	(*open_session)(SSL_SmartCardUnit unit);



	/**
	 * Checks if a card is present for this session.
	 *
	 * Argument:
	 *
	 *   session :		The opaque smartcard session identifier returned when 
	 *					opening a session.
	 *   
	 * Return Value:
	 *
	 *   The number of keys available for the card inserted.
	 *
	 *   This function return zero if no card is present, or a problem occured.
	 *   card_get_last_error will return the error code in case of a failure.
	 */

	int						(*card_present)(SSL_SmartCardSession session);



	/**
	 * do_encrypt_operation		Performs an encrypt operation using the selected
	 * key in the card.
	 *
	 * Arguments:
	 *
	 *   session :		The opaque smartcard session identifier returned when 
	 *					opening a session.
	 *
	 *   key_selected : If the card have more than one key, 
	 *					this selects which key to use. Range 0..n-1
	 *
	 *   use_privkey :	If non-zero, use the private to encrypt
	 *
	 *   pad :			Select the padding strategy used
	 *
	 *   source :		Pointer to a buffer of src_len bytes which will 
	 *					be encrypted using the selected key
	 *
	 *   src_len :		length in bytes of the source buffer
	 *
	 *   target :		Pointer to the destination buffer, buffer must be at
	 *					least bufferlen bytes long.
	 *
	 *   lout :			Pointer to an unsigned int where the number of bytes
	 *					actually stored in the target buffer is stored
	 *
	 *   bufferlen :	Maximum number of bytes to be stored in the target buffer
	 *					The target buffer must be able to hold the entire result from
	 *					the operation
	 *
	 *
	 * Return Value:
	 *
	 *   Non-zero if the encryption succeded, Zero in case of failure.
	 *   card_get_last_error will return the error code in case of a failure.
	 */

	BOOL					(*do_encrypt_operation)(
									SSL_SmartCardSession session,
									int key_selected,
									int use_privkey, 
									SC_API_PaddingStrategy pad, 
									const unsigned char *source, 
									unsigned int src_len, 
									unsigned char *target, 
									unsigned int *lout	, 
									unsigned int bufferlen
								);



	/**
	 * do_decrypt_operation		Performs a decrypt operation using the selected
	 * key in the card.
	 *
	 * Arguments:
	 *
	 *   session :		The opaque smartcard session identifier returned when 
	 *					opening a session.
	 *
	 *   key_selected : If the card have more than one key, 
	 *					this selects which key to use. Range 0..n-1
	 *
	 *   use_privkey :	If non-zero, use the private to encrypt
	 *
	 *   pad :			Select the padding strategy used
	 *
	 *   source :		Pointer to a buffer of src_len bytes which will 
	 *					be decrypted using the selected key
	 *
	 *   src_len :		length in bytes of the source buffer
	 *
	 *   target :		Pointer to the destination buffer, buffer must be at
	 *					least bufferlen bytes long.
	 *
	 *   lout :			Pointer to an unsigned int where the number of bytes
	 *					actually stored in the target buffer is stored
	 *
	 *   bufferlen :	Maximum number of bytes to be stored in the target buffer
	 *					The target buffer must be able to hold the entire result from
	 *					the operation
	 *
	 *
	 * Return Value:
	 *
	 *   Non-zero if the decryption succeded, Zero in case of failure.
	 *   card_get_last_error will return the error code in case of a failure.
	 */
	
	BOOL					(*do_decrypt_operation)(
									SSL_SmartCardSession session,
									int key_selected,
									int use_privkey, 
									SC_API_PaddingStrategy pad, 
									const unsigned char *source,
									unsigned int len,
									unsigned char *target,  
									unsigned int *lout, 
									unsigned int bufferlen
								);

	

	/**
	 * do_sign_operation		Performs a signing operation using the selected
	 * key in the card.
	 *
	 * Arguments:
	 *
	 *   session :		The opaque smartcard session identifier returned when 
	 *					opening a session.
	 *
	 *   key_selected : If the card have more than one key, 
	 *					this selects which key to use. Range 0..n-1
	 *
	 *   pad :			Select the padding strategy used
	 *
	 *   source :		Pointer to a buffer of src_len bytes which will 
	 *					be signed using the selected key
	 *
	 *   src_len :		length in bytes of the source buffer
	 *
	 *   target :		Pointer to the destination buffer, buffer must be at
	 *					least bufferlen bytes long.
	 *
	 *   lout :			Pointer to an unsigned int where the number of bytes
	 *					actually stored in the target buffer is stored
	 *
	 *   bufferlen :	Maximum number of bytes to be stored in the target buffer
	 *					The target buffer must be able to hold the entire result from
	 *					the operation
	 *
	 *
	 * Return Value:
	 *
	 *   Non-zero if the signing was successful, Zero in case of failure.
	 *   card_get_last_error will return the error code in case of a failure.
	 */

	BOOL					(*do_sign_operation)(
									SSL_SmartCardSession session,
									int key_selected,
									const unsigned char *source,
									unsigned int  src_len,
									unsigned char *target,
									unsigned int  *lout, 
									unsigned int  bufferlen
								);




	/**
	 * do_verify_operation		Performs a verify operation using the selected
	 * key in the card.
	 *
	 * Arguments:
	 *
	 *   session :		The opaque smartcard session identifier returned when 
	 *					opening a session.
	 *
	 *   key_selected : If the card have more than one key, 
	 *					this selects which key to use. Range 0..n-1
	 *
	 *   pad :			Select the padding strategy used
	 *
	 *   source :		Pointer to a buffer of src_len bytes which will 
	 *					used to verify the result stored in reference, using 
	 *					the selected key
	 *
	 *   src_len :		length in bytes of the source buffer
	 *
	 *   reference :	Pointer to a buffer of ref_len bytes which will 
	 *					be checked against the encrypted content stored 
	 *					in source.
	 *
	 *   ref_len :		length in bytes of the reference buffer
	 *
	 *
	 * Return Value:
	 *
	 *   Non-zero if the verification succeded, Zero in case of failure.
	 *   card_get_last_error will return the error code in case of a failure.
	 */

	int						(*do_verify_operation)(
									SSL_SmartCardSession session,
									int key_selected,
									const unsigned char *source,
									unsigned int src_len,
									unsigned char *reference,
									unsigned int *ref_len
								);



	/**
	 * card_get_id : Retrieve a unique, Human readable, text identifying the
	 * smart card presently in the smartcard reader.
	 *
	 * This information may be used to ask the user for a specific, 
	 * previously identified card.
	 *
	 * Recommendation: The text SHOULD include the following:
	 *		1: A manufacturer/issuer name displayed physically on the card
	 *		2: The cards serial number, or other uniquely identifying 
	 *			information printed visibly on the card
	 *
	 * Arguments
	 *
	 *   session :		The opaque smartcard session identifier returned when 
	 *					opening a session.
	 *
	 *   name :			Pointer to a character buffer where a null-terminated 
	 *					string is to be stored. The string (including null-termination)
	 *					can be no longer than namelen bytes.
	 *
	 *   namelen :		The maximum number of bytes that can be stored in the 
	 *					name buffer, including the null termination of the string
	 *
	 *
	 * Return value:
	 *
	 *   Non-zero :		The unique name of the card is stored in the name buffer.
	 *   Zero :			An error occured, card_get_last_error will identify the error
	 *
	 */

	int						(*card_get_id)(
											SSL_SmartCardSession session,
											char *name,
											unsigned int namelen
										);

	/**
	 * card_get_public_key : Retrieve the DER encoded public key. The structure used MUST be 
	 * the SubjectPublicKeyInfo from X509/PKIX:
	 *
	 *				SubjectPublicKeyInfo ::= SEQUENCE {
	 *					algorithm   AlgorithmIdentifier,
	 *					subjectPublicKey BIT STRING}
	 *
	 * Arguments:
	 *
	 *   session :		The opaque smartcard session identifier returned when 
	 *					opening a session.
	 *
	 *   key_selected : If the card have more than one key, 
	 *					this selects which key to use. Range 0..n-1
	 *
	 *   keydata :		Pointer to the location where the pointer of the buffer allocated (using
	 *					the card_mem_alloc API function) by the API function. The allocated buffer 
	 *					is used to store the extracted public key. The allocated buffer must be 
	 *					at least as long as the value stored in the location key_length points to.
	 *					The allocated memory will be freed by the caller (using the card_mem_free 
	 *					API function).
	 *
	 *   key_length :	Pointer to an unsigned int where the length of the buffer holding the
	 *					extracted public key is stored.
	 *
	 * If an error occurs the keydata pointer MUST be set to NULL, and card_get_last_error must 
	 * return the error code. In case of an error key_length is undefined.
	 *
	 */

	void				(*card_get_public_key)(
											SSL_SmartCardSession session,
											int key_selected,
											unsigned char **keydata,
											unsigned int *key_length
										);


	/**
	 * card_get_certficate : Retrieve the DER encoded certificate for the selected key.
	 * The returned array SHOULD include all signer's certificate if they are stored
	 * in the certificate. If multiple certifcates are used, they MUST be encoded as a 
	 * SEQUENCE OF X509_Certificate
	 *
	 * Arguments:
	 *
	 *   session :		The opaque smartcard session identifier returned when 
	 *					opening a session.
	 *
	 *   key_selected : If the card have more than one key, 
	 *					this selects which key to use. Range 0..n-1
	 *
	 *   certdata :		Pointer to the location where the pointer of the buffer allocated by the 
	 *					API function. The allocated buffer is used to store the extracted certificate. 
	 *					The allocated buffer must be at least as long as the value stored in the 
	 *					location key_length points to. The allocated memory will be freed by the 
	 *					caller (using the card_mem_free API function).
	 *
	 *   cert_length :	Pointer to an unsigned int where the length of the buffer holding the
	 *					extracted certficate is stored.
	 *
	 * If an error occurs the certdata pointer MUST be set to NULL, and card_get_last_error must 
	 * return the error code. In case of an error cert_length is undefined.
	 *
	 */

	void				(*card_get_certificate)(
											SSL_SmartCardSession session,
											int key_selected,
											unsigned char **certdata,
											unsigned int *cert_length
										);



	/**
	 * card_get_last_error : Returns the error code last encountered by the 
	 * smartcard driver for the given session, translated into errorcodes based on the TLS protocol 
	 * error codes described in RFC 2246. The caller determines if the error is
	 * fatal for the ongoing operation, or not.
	 *
	 * Argument
	 *
	 *   session :		The opaque smartcard session identifier returned when 
	 *					opening a session.
	 *
	 *   error_string :	Pointer to a const char pointer which can be used to 
	 *					provide a human readable error message. The buffer used 
	 *					for this message is maintained by the API function, and is 
	 *					not manipulated by the caller. If the caller wish to keep 
	 *					the string, it must allocate memory and copy the buffer contents
	 *					immediately, as the buffer must be assumed to be overwritten
	 *					by the next call to any API function for the *same* session.
	 *
	 *					If this argument is not used, the pointer is NULL. If the API function
	 *					do not use it must set the value pointed to by a non-NULL argument 
	 *					to NULL.
	 *
	 *					The string MUST be null-terminated.
	 *
	 * Return Value:
	 *
	 * A numeric value identifying the error in the range 0 - 65535.
	 *
	 * The TLS error codes are defined in the range of 0 to 256, but an application may
	 * use error codes of larger values by using error codes that are (mod 256) of the 
	 * desired TLS error codes on the form
	 *
	 *    actual_code = TLS_code + (internal_code * 256) 
	 *
	 *	internal_code values less than 192 are reserved. Values larger than 255 MUST NOT be used.
	 *
     *  The following details the TLS_code values presently defined 
	 *  in RFC 2246
	 *				RFC 2246              The TLS Protocol Version 1.0          January 1999
	 *				Dierks & Allen              Standards Track                    [Page 24]
	 *						enum {
	 *							close_notify(0),
	 *							unexpected_message(10),
	 *							bad_record_mac(20),
	 *							decryption_failed(21),
	 *							record_overflow(22),
	 *							decompression_failure(30),
	 *							handshake_failure(40),
	 *							bad_certificate(42),
	 *							unsupported_certificate(43),
	 *							certificate_revoked(44),
	 *							certificate_expired(45),
	 *							certificate_unknown(46),
	 *							illegal_parameter(47),
	 *							unknown_ca(48),
	 *							access_denied(49),
	 *							decode_error(50),
	 *							decrypt_error(51),
	 *							export_restriction(60),
	 *							protocol_version(70),
	 *							insufficient_security(71),
	 *							internal_error(80),
	 *							user_canceled(90),
	 *							no_renegotiation(100),
	 *							(255)
	 *						} AlertDescription;
	 *
	 */


	unsigned short		(*card_get_last_error)(
											SSL_SmartCardSession session,
											char **keydata
										);


										
	/**
	 * card_get_attributes: This function returns a number of attributes 
	 * of the smartcard presently used by the session.
	 *
	 * NOTE the algorithm used can be retrieved by decoding the algorithm identifier 
	 * from the public key structure returned by the card_get_public_key API function.
	 *
	 * Arguments:
	 *
	 *   session :		The opaque smartcard session identifier returned when 
	 *					opening a session.
	 *
	 *   key_selected : If the card have more than one key, 
	 *					this selects which key to use. Range 0..n-1
	 *
	 *   key_usage :		Pointer to a variable where the usage area(s) of the
	 *						selected key is stored.
	 *						If this argument is NULL pointer no value is returned.
	 *
	 *   public_key_bits :	pointer to a variable that will contain the number of
	 *						bits in the cards public key
	 *						If this argument is NULL pointer no value is returned.
	 *
	 *   encrypt_block_size_in_bytes:	pointer to a variable that will contain the maximum 
	 *						number of bytes allowed in a single encryption.
	 *						If this argument is NULL pointer no value is returned.
	 *
	 *   encrypt_block_size_out_bytes:	pointer to a variable that will contain the maximum 
	 *						number of bytes generated by a single encryption.
	 *						If this argument is NULL pointer no value is returned.
	 *
	 *
	 * Return Value:
	 *
	 *    Non-zero: The function succeded, and the values returned are valid.
	 *    zero:		The call failed, and card_get_last_error will identify the error.
	 */

	int					(*card_get_attributes)(
											SSL_SmartCardSession session,
											int key_selected,
											SC_API_KEYUSAGE *key_usage,
											unsigned int *public_key_bits,
											unsigned int *encrypt_block_size_in_bytes,
											unsigned int *encrypt_block_size_out_bytes
										);



	/**
	 * card_mem_free: function used by the caller to free memory allocated by the API-functions
	 *
	 * Argument:
	 *
	 *   ptr :				Pointer to the memory that is being freed.
	 *
	 */

	void				(*card_mem_free)(
											void *ptr
										);

};


