/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#ifndef __SSLPUBKEY_H
#define __SSLPUBKEY_H

#ifdef _NATIVE_SSL_SUPPORT_
#include "modules/libssl/methods/sslcipher.h"

class SSL_PublicKeyCipher: public SSL_Cipher
{
protected:
    BOOL use_privatekey;
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	BOOL use_automatically;
#endif

public:
	
	SSL_PublicKeyCipher():use_privatekey(FALSE)
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
		, use_automatically(FALSE)
#endif
		{cipher_type = SSL_BlockCipher;}

	/** Enables/Disables the use of a Private Key */
    void SetUsePrivate(BOOL stat){use_privatekey = stat;}

	/** Are we using the private key? */
	BOOL UsingPrivate() const { return use_privatekey;}

	/** Verify that the signature string matches the locally generated reference digest string
	 *	@param	reference	Locally generated digest of content to be verified
	 *	@param signature	bytestring containing the signature to be checked, encrypted with private key
	 *
	 *	@return	TRUE if the signature verified correctly
	 */
    virtual BOOL Verify(const byte *reference,uint32 len, const byte *signature, uint32) =0; 

	/** Sign the source string with this private key and store the result in the target
	 *
	 *	@param	source	The generated digest to be signed (len byte long)
	 *	@param	target	Pointer to buffer where the result is to be stored
	 *	@param	result_len	The length of the completed signature stored in target is returne here
	 *	@param	bufferlen	Length of the target buffer, must be at least as long as the private key encryption result
	 *	@return Pointer to signature in target, NULL if operation failed
	 */
    virtual byte *Sign(const byte *source,uint32 len,byte *target, uint32 &result_len, uint32 bufferlen) =0;

#ifdef USE_SSL_ASN1_SIGNING
	/** Sign the source string with this private key and store the result in the target, encode using PKCS #1.5 format
	 *
	 *	@param	source	The generated digest to be signed (len byte long)
	 *	@param	target	Pointer to buffer where the result is to be stored
	 *	@param	result_len	The length of the completed signature stored in target is returne here
	 *	@param	bufferlen	Length of the target buffer, must be at least as long as the private key encryption result
	 *	@return Pointer to signature in target, NULL if operation failed
	 */

	virtual byte *SignASN1(SSL_Hash *reference, byte *target, uint32 &, uint32 bufferlen) =0;
	virtual byte *SignASN1(SSL_HashAlgorithmType alg, SSL_varvector32 &reference, byte *signature, uint32 &len, uint32 bufferlen)=0;

	/** Verify that the PKCS #1.5 encoded signature string matches the locally generated reference digest string
	 *	@param	reference	Locally generated digest of content to be verified
	 *	@param signature	bytestring containing the signature to be checked, encrypted with private key
	 *
	 *	@return	TRUE if the signature verified correctly
	 */
    virtual BOOL VerifyASN1(SSL_Hash *reference, const byte *signature, uint32) =0; 
#endif

	/** Sign the source string with this private key and store the result in the target
	 *
	 *	@param	source	The generated digest to be signed (len byte long)
	 *	@param	target	Pointer to buffer where the result is to be stored
	 *	@param	result_len	The length of the completed signature stored in target is returne here
	 *	@param	bufferlen	Length of the target buffer, must be at least as long as the private key encryption result
	 *	@return Pointer to signature in target, NULL if operation failed
	 */
    void SignVector(const SSL_varvector32 &source,SSL_varvector32 &target);
	/** Verify that the signature string matches the locally generated reference digest string
	 *	@param	reference	Locally generated digest of content to be verified
	 *	@param signature	bytestring containing the signature to be checked, encrypted with private key
	 *
	 *	@return	TRUE if the signature verified correctly
	 */
    BOOL VerifyVector(const SSL_varvector32 &reference, const SSL_varvector32 &signature); 
	
	// Public key Id's
	/*
	*  RSA
	*       public   0   modulator
	*       public   1   public exponent;
	*       private  0   private exponent;
	*		private  1   secret prime p
	*		private  2   secret prime q
	*		private  3   secret u the multiplicative inverse of p mod q
	*
	*  DSA
	*       public   0   p : prime
	*       public   1   q : 160 bit prime;
	*       public   2   g : generator
	*       public   3   y : public key
	*       private  0   x : private key
	*
	*  DH
	*       public   0   p (dh_p): prime
	*       public   1   g (dh_g) : generator
	*       public   2   Y (dh_Yx) : own generated public key
	*       public   3   Y (dh_Yy) : received public key
	*       private  0   x (x) : private random value
	*       private  1   generated common, shared value produced from received public key and x
	*/

	/** Length of key in bits */
    virtual uint32 KeyBitsLength() const=0;   

	/** Load a specific public key parameter (listed above) into this key */
    virtual void LoadPublicKey(uint16, const SSL_varvector16 &) =0;

#ifdef _SMART_CARD_API_
	virtual unsigned long GetSmartCardNumber(); 
	enum SSL_KeyPurpose {
		SSL_EncryptKey,
		SSL_Sign_Key,
		SSL_KeyNegotiate
	};
	virtual void UnloadCertificate(SSL_varvector32 &cert){cert.Resize(0)};
	virtual void SelectKeyPurpose(SSL_KeyPurpose purp){}
#endif

#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	/** Set the login password/Pincode for the external key *
	 *	the password is encoded in UTF-8
	 */
	virtual void Login(SSL_secure_varvector32 &password);

	/** Does this key need a login (as separate from a PIN code) */
	virtual BOOL Need_Login() const;

	/** Does this key need the user to enter a PIN code through a provider device 
	 *	specific entry path? 
	 */
	virtual BOOL Need_PIN() const;

	/** Returns TRUE if the certificate associated with this key must be used without prompting the user 
	 *	This should be used with care, since this may affect the user's experience of the transaction, and 
	 *	could conceivably override the certificate the user really should use on a site.
	 */
	virtual BOOL UseAutomatically() const;

	/** Set the flag for use automatically */
	virtual void Set_UseAutomatically(BOOL flag);
#endif
	
	/** Load an element of a private key into this key handler */
    virtual void LoadPrivateKey(uint16, const SSL_varvector16 &) =0;
	/** Size of a public key element */
    virtual uint16 PublicSize(uint16) const =0;   
	/** Extract a public key element */
    virtual void UnLoadPublicKey(uint16, SSL_varvector16 &) =0;
	/** Extract a private key element. Use with care */
    virtual void UnLoadPrivateKey(uint16, SSL_varvector16 &) =0;
#ifdef SSL_PUBKEY_DETAILS
	/** Number of public key elements */
    virtual uint16 PublicCount() const =0;   
	/** Number of generated public key elements */
    virtual uint16 GeneratedPublicCount() const =0;   
	/** Number of private key elements */
    virtual uint16 PrivateCount() const =0;   
	/** Number of generated private key elements */
    virtual uint16 GeneratedPrivateCount() const =0;   
	/** length in bits of a private key element */
    virtual uint16 PrivateSize(uint16) const =0;   
#endif
#ifdef SSL_DH_SUPPORT 
	/** Start generation of a generated public key element */
    virtual void ProduceGeneratedPublicKeys() =0;
	/** Start generation of a generated private key element */
    virtual void ProduceGeneratedPrivateKeys() =0;
#endif
	/** Copy the state of this entire key handler and return the result */
    virtual SSL_PublicKeyCipher *Fork() const=0; 
	
	/** Load a complete key, private or public. The key must be stored in the 
	 *	DER format required for such keys in certificates
	 */
    virtual void LoadAllKeys(SSL_varvector32 &)=0;   // Vector of application specific content
}; 

#define SSL_DER_EncodedKey USHRT_MAX

#define SSL_RSA_private_key 0
#define SSL_RSA_public_key_mod 0
#define SSL_RSA_public_key_exp 1

#define SSL_DSA_private_key 0
#define SSL_DSA_public_prime 0
#define SSL_DSA_public_prime1 1
#define SSL_DSA_public_generator 2
#define SSL_DSA_public_key 3

#define SSL_DH_private_random 0
#define SSL_DH_private_common 1
#define SSL_DH_public_prime 0
#define SSL_DH_public_generator 1
#define SSL_DH_gen_public_key 2 
#define SSL_DH_recv_public_key 3
#define SSL_MAX_EXPORTABLE_RSA_KEY 512

#endif
#endif
