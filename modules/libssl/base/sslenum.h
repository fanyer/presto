/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _SSLENUM_H_	// {
#define _SSLENUM_H_

#include "modules/url/url_enum.h"
#include "modules/libssl/base/sslenum2.h"

// Certificate warn status flags

// warn/deny status
#define SSL_CERT_NO_WARN		0
#define SSL_CERT_WARN			1
#define SSL_CERT_DENY			2
#define SSL_CERT_WARN_MASK		3

// expiration errors
#define SSL_CERT_EXPIRED		4
#define SSL_CERT_NOT_YET_VALID	8

// Unknown issuer
#define SSL_CERT_NO_ISSUER		16
#define SSL_CERT_UNKNOWN_ROOT	32

// Low encryption level
#define SSL_CERT_LOW_ENCRYPTION	64
#define SSL_CERT_DIS_CIPH_LOW_ENCRYPTION	128
#define SSL_CERT_ANONYMOUS_CONNECTION	256
#define SSL_CERT_AUTHENTICATION_ONLY 512

// Servername and certificate name mismatch
#define SSL_CERT_NAME_MISMATCH	1024

// Validation failed
#define SSL_CERT_INVALID		2048

#define SSL_CERT_INCORRECT_ISSUER	4096

#define SSL_CERT_LOW_SEC_REASON_START 8192
#define SSL_CERT_LOW_SEC_REASON_SHIFT 12
#define SSL_CERT_LOW_SEC_REASON_MASK (SECURITY_LOW_REASON_MASK << SSL_CERT_LOW_SEC_REASON_SHIFT)

#define SSL_CERT_NEXT_VALUE  (SSL_CERT_LOW_SEC_REASON_START << SECURITY_LOW_REASON_BITS) 

#define SSL_CERT_UNUSED			SSL_CERT_NEXT_VALUE
#define SSL_INVALID_CRL			(SSL_CERT_NEXT_VALUE << 1)
#define SSL_NO_CRL				(SSL_CERT_NEXT_VALUE << 2)
#define SSL_FROM_STORE			(SSL_CERT_NEXT_VALUE << 3)
#define SSL_PRESHIPPED			(SSL_CERT_NEXT_VALUE << 4)

#if !defined LIBOPEAY_DISABLE_MD5_PARTIAL
#define SSL_USED_MD5		(SSL_CERT_NEXT_VALUE << 5)
#endif
#if !defined LIBOPEAY_DISABLE_SHA1_PARTIAL
#define SSL_USED_SHA1			(SSL_CERT_NEXT_VALUE << 6)
#endif

#define SSL_NO_RENEG_EXTSUPPORT (SSL_CERT_NEXT_VALUE << 7)


#define SSL_CERT_INFO_FLAGS     (SSL_FROM_STORE | SSL_PRESHIPPED)

// Hash lengths
#define SSL_MD5_LENGTH 16
#define SSL_SHA_LENGTH 20 
#define SSL_SHA_224_LENGTH 24 
#define SSL_SHA_256_LENGTH 32
#define SSL_SHA_384_LENGTH 48 
#define SSL_SHA_512_LENGTH 64 

#define SSL_MAX_HASH_LENGTH SSL_SHA_512_LENGTH


enum SSL_ContentType {
	SSL_ChangeCipherSpec   = 0x14,
	SSL_AlertMessage       = 0x15,
	SSL_Handshake          = 0x16,
	SSL_Application			 = 0x17,
	SSL_ContentError       = 0x1FF,
	SSL_PerformChangeCipher = 0x1000, // special type, only used internally
	SSL_Warning_Alert_Message = 0x1015, // special type, only used internally // downgrades to SSL_AlertMessage
#ifdef LIBSSL_ENABLE_SSL_FALSE_START
	SSL_Handshake_False_Start_Application = 0x2000 // special type, which sends application data early according to ssl false start specification.
#endif // LIBSSL_ENABLE_SSL_FALSE_START
};

enum SSL_AlertDescription {
	SSL_Close_Notify = 0,
	SSL_Unexpected_Message = 0x0a,
	SSL_Bad_Record_MAC = 0x14, 
	SSL_Decryption_Failed = 0x15,
	SSL_Record_Overflow = 0x16,
	SSL_Decompression_Failure = 0x1e,
	SSL_Handshake_Failure = 0x28,
	SSL_No_Certificate = 0x29,
	SSL_Bad_Certificate = 0x2a,
	SSL_Unsupported_Certificate =0x2b,
	SSL_Certificate_Revoked = 0x2c,
	SSL_Certificate_Expired = 0x2d,
	SSL_Certificate_Unknown = 0x2e,
	SSL_Illegal_Parameter = 0x2f,
	SSL_Unknown_CA = 0x30,
	SSL_Access_Denied =0x31,
	SSL_Decode_Error = 0x32,
	SSL_Decrypt_Error = 0x33,
	SSL_Export_Restriction = 0x3c,
	SSL_Protocol_Version_Alert = 0x46,
	SSL_Insufficient_Security = 0x47,
	SSL_InternalError = 0x50,
	SSL_User_Canceled = 0x5a,
	SSL_No_Renegotiation = 0x64,
	//SSL_UnSupported_Extension = 0x6E,		// Reserved in RFC. Defined in case we use it later
	//SSL_Certificate_Unobtainable = 0x6F,	// Reserved in RFC. Defined in case we use it later
	SSL_Unrecognized_Name = 0x70,
	SSL_Bad_Certificate_Status_Response = 0x71,
	SSL_Bad_Certificate_Hash_Value = 0x72,
	SSL_Certificate_Verify_Error = 0x22a,     // Devolve to badcertificate
	SSL_Certificate_Purpose_Invalid = 0x32a,  // Devolve to badcertificate
	SSL_Certificate_OCSP_error= 0x42a,    // Devolve to badcertificate
	SSL_Certificate_OCSP_Verify_failed = 0x52a,    // Devolve to badcertificate
	SSL_Certificate_CRL_Verify_failed = 0x62a,    // Devolve to badcertificate
	SSL_Not_TLS_Server = 0x228,  // Devolve to handshake failure
	SSL_Restart_Handshake = 0x328,			// Devolve to SSL_Handshake_Failure
	SSL_Renego_PatchUnPatch = 0x428,		// Devolve to SSL_Handshake_Failure
	SSL_Fraudulent_Certificate = 0x12c,     // Devolve to SSL_Certificate_Revoked
	SSL_Certificate_Not_Yet_Valid = 0x12d, // Devolve to SSL_Certificate_Expired
	SSL_Unknown_CA_WithAIA_URL = 0x130,	// Devolve to SSL_Unknown_CA
	SSL_Unknown_CA_RequestingUpdate = 0x230,	// Devolve to SSL_Unknown_CA
	SSL_Unknown_CA_RequestingExternalUpdate = 0x330, 	// Devolve to SSL_Unknown_CA
	SSL_Untrusted_Cert_RequestingUpdate = 0x231,	// Devolve to SSL_Access_Denied
	SSL_NoRenegExtSupport = 0x146,				// Devolve to SSL_Protocol_Version_Alert
	SSL_Insufficient_Security1 = 0x147,  // Devolve to insufficient security // too weak DHE key
	//SSL_Insufficient_Security2 = 0x247,  // Devolve to insufficient security
	SSL_Allocation_Failure =0x150,    // Devolve to internal error
	SSL_No_Cipher_Selected = 0x250,   // Devolve to internal error
	SSL_Components_Lacking = 0x350,   // Devolve to internal error
	SSL_Security_Disabled = 0x450,   // Devolve to internal error
	SSL_Bad_PIN = 0x550,				// Devolve to internal error
	SSL_Smartcard_Open = 0x650,			// Devolve to internal error
	SSL_Smartcard_Failed = 0x750,		// Devolve to internal error
	SSL_Unloaded_CRLs_Or_OCSP = 0x850,			// Devolve to internal error
	SSL_WaitForRepository = 0x950,		// Devolve to internal error
	SSL_Authentication_Only_Warning = 0x15a, // Devolve to User Canceled
	SSL_No_Description = 0x1ff,
}; 

enum SSL_AlertLevel {
	SSL_NoError = 0, 
	SSL_Warning = 1, 
	SSL_Fatal   = 2,
	SSL_Internal= 0x1FF
};  

enum SSL_CompressionMethod {
	SSL_Null_Compression = 0,
	SSL_Illegal_Compression
};

enum SSL_CipherType {
	SSL_StreamCipher, 
	SSL_BlockCipher
};

// CFB and CFB enums downgrade on request for ID
enum SSL_BulkCipherType {
	SSL_NoCipher,
	SSL_RC4, 
	SSL_RC4_256, 
	SSL_RC2, 
	SSL_DES,
	SSL_3DES,
	SSL_IDEA,
	SSL_BLOWFISH_CBC,
	SSL_AES,
	SSL_CAST5,
	SSL_AES_128_CBC,
	SSL_AES_256_CBC,
	SSL_3DES_CFB,
	SSL_AES_128_CFB,
	SSL_AES_192_CFB,
	SSL_AES_256_CFB,
	SSL_CAST5_CFB,
	SSL_RSA,
	SSL_DH,
	SSL_DSA,
	SSL_ElGamal
};

enum SSL_HandShakeType {
	SSL_Hello_Request = 0, 
	SSL_Client_Hello = 1, 
	SSL_Server_Hello=2,
	SSL_Certificate = 11, 
	SSL_Server_Key_Exchange=12, 
	SSL_CertificateRequest= 13, 
	SSL_Server_Hello_Done=14, 
	SSL_Certificate_Verify=15,
	SSL_Client_Key_Exchange=16, 
	SSL_Finished=20, 

	TLS_CertificateStatus = 22,
	TLS_NextProtocol=67,

	SSL_NONE =0x1ff
};


enum SSL_Handshake_Action {
	Handshake_No_Action,
	Handshake_Handle_Message,
	Handshake_Send_Message,
	Handshake_ChangeCipher,
	Handshake_ChangeCipher2,
	Handshake_Send_No_Certificate,
	Handshake_PrepareFinished,
	Handshake_Completed,
	Handshake_Restart,
	Handshake_Handle_Error,
	/*Handshake_Next_Protocol,*/
#ifdef LIBSSL_ENABLE_SSL_FALSE_START
	Handshake_False_Start_Send_Application_Data
#endif // LIBSSL_ENABLE_SSL_FALSE_START
};

enum SSL_Handshake_Status {
	Handshake_Undecided,
	Handshake_Ignore,
	Handshake_Will_Not_Receive,
	Handshake_Will_Not_Send,
	Handshake_Expected,
	Handshake_MustReceive,
	Handshake_Received,
	Handshake_Will_Send,
	Handshake_Sent,
	Handshake_Block_Messages
};

enum SSL_SessionUpdate
{
	Session_No_Update,
	Session_Resume_Session,
	Session_New_Session,
	Session_New_Anonymous_Session,
	Session_TLS_ExpectCertificateStatus,
	Session_Server_Done_Received,
	Session_Certificate_Configured,
	Session_No_Certificate,
	Session_Changed_Server_Cipher,
	Session_Finished_Confirmed
};


enum SSL_KEA_ACTION {
	SSL_KEA_No_Action,
	SSL_KEA_Verify_Certificate,
	SSL_KEA_Application_Process_Certificate_Request,
	SSL_KEA_Application_Delay_Certificate_Request, // Used during testing
	SSL_KEA_Handle_Message,
	SSL_KEA_Handle_Errors,
	SSL_KEA_Wait_For_User,
	SSL_KEA_Wait_For_KeyExchange,
	SSL_KEA_Resume_Session,
	SSL_KEA_Prepare_Premaster,
	SSL_KEA_Prepare_Keys,
	SSL_KEA_Commit_Session,
	SSL_KEA_Finish_Handshake,
	SSL_KEA_FullConnectionRestart,
};

enum SSL_KeyExchangeAlgorithm {
	SSL_NULL_KEA, 
	SSL_RSA_KEA, 
	SSL_RSA_EXPORT_KEA, 
	SSL_Diffie_Hellman_KEA, 
	SSL_Ephemeral_Diffie_Hellman_KEA, 
	SSL_Anonymous_Diffie_Hellman_KEA, 
	SSL_Illegal_KEA
};

enum SSL_SignatureAlgorithm {
	SSL_Anonymous_sign=0,
	SSL_RSA_sign=1,
	SSL_DSA_sign=2,
	SSL_ECDSA= 3,
	SSL_Signature_max=0xff,
	SSL_Illegal_sign=0x1ff,
	SSL_RSA_MD5_SHA_sign, 
	SSL_RSA_MD5,
	SSL_RSA_SHA,
	SSL_RSA_SHA_224,
	SSL_RSA_SHA_256,
	SSL_RSA_SHA_384,
	SSL_RSA_SHA_512,
};

enum SSL_ClientCertificateType {
	SSL_rsa_sign = 1,
	SSL_dss_sign = 2,
	SSL_rsa_fixed_dh = 3,
	SSL_dss_fixed_dh = 4,
	SSL_rsa_ephemeral_dh  = 5,
	SSL_dss_ephemeral_dh = 6,
	SSL_Illegal_Cert_Type = 0x1ff
}; 

enum SSL_PublicValueEncoding {
	SSL_implicit, SSL_explicit
};

enum SSL_ENCRYPTMODE{
	SSL_RECORD_PLAIN = 0x00,
	SSL_RECORD_COMPRESSED = 0x01,
	SSL_RECORD_ENCRYPTED_UNCOMPRESSED = 0x02, 
	SSL_RECORD_ENCRYPTED_COMPRESSED  = 0x03  
};

enum SSL_Certificate_Request_Format {
	SSL_Netscape_SPKAC,
	SSL_PKCS10
};

enum SSL_CertificatePurpose {
	SSL_Purpose_Any,
	SSL_Purpose_Client_Certificate,
	SSL_Purpose_Server_Certificate,
	SSL_Purpose_Object_Signing
};

enum EPKCS12InstStatus {
	NOT_PKCS12, 
	PKCS12_SUCCESS, 
	PKCS12_FAILED, 
	PKCS12_ABORTED, 
	PKCS12_NOKEY, 
	PKCS12_WRONGKEY
};

enum SSL_SecurityRating {
	No_Security = SECURITY_STATE_NONE,
	Insecure    = SECURITY_STATE_NONE,
	Vulnerable  = SECURITY_STATE_LOW,
	SomeSecurity= SECURITY_STATE_HALF,
	Secure      = SECURITY_STATE_FULL
};


enum LengthOrData_Only {Length_Only, Data_Only};


enum SSL_AuthentiRating {No_Authentication, Authenticated};

// Enum value the same as the openssl/rsa.h padding value
enum SSL_PADDING_strategy {
	SSL_NO_PADDING = 3, 
	SSL_PKCS5_PADDING = 5,
	/*, SSL_V3_PADDING = 2,*/
	SSL_PKCS1_PADDING = 1, // standard padding
};


enum SSL_ExpirationType {
	SSL_NotExpired,
	SSL_NotYetValid,
	SSL_Expired
};


enum TLS_CertificateStatusType {
	TLS_CST_OCSP = 1,
	TLS_CST_MaxValue = 255
};

/*
 * What is the highest version and featureset tolerated by the server, based on testing, or which version/featureset are we currently testing?
 *
 * In case of a failure during testing the stated next test version will be used in the next attempt.

 * The ssl version testing starts with TLS_Test_1_2_Extensions, and goes down if there is a failure. 
 * Once successful, the corresponding final value is stored.
 */
enum TLS_Feature_Test_Status {
	TLS_Untested,             // initial state
	TLS_Test_SSLv3_only, // Only if original test TLS_Test_1_0 timed out, if this one also times out move to TLS_SSL_v2_only
				// Failure -> TLS_Version_not_supported		If this fails it is not possible to connect to the server using any supported version of SSL or TLS.
	TLS_Test_1_0,				// Test with at most TLS 1.0, without extensions 
				// Failure -> TLS_Test_SSLv3_only	In case of failure  to complete negotiation
	TLS_Test_with_Extensions_start,
	TLS_Test_1_0_Extensions,	// Test with TLS 1.0 and Extensions
				// Failure -> TLS_Test_1_0	In case of failure  to complete negotiation
	//TLS_Test_1_1_Extensions,	// Test with TLS 1.1 and Extensions
				// Failure -> TLS_Test_1_0_Extensions	In case of failure  to complete negotiation
	TLS_Test_1_2_Extensions,	// Test with TLS 1.2 and Extensions
				// Failure -> TLS_Test_1_0_Extensions	In case of failure  to complete negotiation
	TLS_Test_with_Extensions_end,

	TLS_Final_stage_start,
	// All finals below this line
	TLS_Version_not_supported,
	TLS_SSL_v3_only,			// Final stage // Use SSL v3 only
	TLS_1_0_only,				// Final stage // Use at most TLS 1.0 w/o extensions
	TLS_Final_extensions_supported,  // All configurations below support extensions
	TLS_1_0_and_Extensions,		// Final stage // Use up to TLS 1.0 with extensions
	//TLS_1_1_and_Extensions,		// Final stage // Use up to TLS 1.1 with extensions
	TLS_1_2_and_Extensions,		// Final stage // Use up to TLS 1.2 with extensions
	// All finals above this line
	TLS_Final_stage_end,

	// Mappings
	TLS_TestMaxVersion = TLS_Test_1_2_Extensions,	// Test with highest version available
};

enum SSL_Certinfo_mode {
	SSL_Cert_XML,
	SSL_Cert_XML_Subject,
	SSL_Cert_XML_Issuer,
	SSL_Cert_XML_General,
	SSL_Cert_XML_end,
	SSL_Cert_Text,
	SSL_Cert_Text_Subject,
	SSL_Cert_Text_Issuer,
	SSL_Cert_Text_General,
	SSL_Cert_Text_ValidFrom,
	SSL_Cert_Text_ValidTo,
	SSL_Cert_Text_SerialNumber,
	SSL_Cert_Text_end

};

SSL_HashAlgorithmType SignAlgToHash(SSL_SignatureAlgorithm sig_alg);
SSL_BulkCipherType SignAlgToCipher(SSL_SignatureAlgorithm);
SSL_SignatureAlgorithm SignAlgToBasicSignAlg(SSL_SignatureAlgorithm sig_alg);

enum SSL_CipherDirection {
	SSL_Encrypt,
	SSL_Decrypt
};

enum SSL_ConfirmedMode_enum {
	SSL_CONFIRMED, // Search mode
	NO_INTERACTION,
	SESSION_CONFIRMED,
	PERMANENTLY_CONFIRMED,
	APPLET_SESSION_CONFIRMED,
	USER_REJECTED
};

#ifdef _SSLUINT_H_

#define SSL_SIZE_CONTENT_TYPE 1


class SSL_Alert;

BOOL Valid(SSL_AlertLevel,SSL_Alert *msg=NULL);

inline uint16 SSL_Size(SSL_ContentType){return 1;}
BOOL Valid(SSL_ContentType met, SSL_Alert *msg = NULL);

BOOL Valid(SSL_AlertDescription, SSL_Alert *msg = NULL);
#endif

#endif	// } _SSLENUM_H_
