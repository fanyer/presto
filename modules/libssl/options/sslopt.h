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

// SSL options

#ifndef SSLOPT_H
#define SSLOPT_H

#ifdef _NATIVE_SSL_SUPPORT_

class SSL_KeyExchange;
class SSL_GeneralCipher;
class SSLSecurtityPasswordCallbackImpl;
struct SSL_dialog_config;
class ObfuscatedPassword;

#include "modules/util/opstrlst.h"
#include "modules/libssl/base/sslprotver.h"

#include "modules/rootstore/rootstore_version.h"

#include "modules/libssl/options/optenums.h"
#include "modules/libssl/options/certitem.h"
#include "modules/libssl/options/secprofile.h"
#include "modules/libssl/options/accept_item.h"
#include "modules/libssl/certs/ssl_snlist.h"
#include "modules/libssl/certs/verifyinfo.h"
#include "modules/libssl/certs/crlitem.h"
#include "modules/libssl/certs/certhandler.h"
#include "modules/libssl/options/cipher_description.h"
#include "modules/libssl/options/auto_retrieve.h"
#include "modules/libssl/options/url_retrieve.h"
#include "modules/libssl/handshake/cipherid.h"
#include "modules/locale/locale-enum.h"

class SSL_Options : public OpPrefsListener
{
	friend class SSL_API;
	friend class SSL_KeyExchange;
	friend class SSL_CertificateVerifier;
	friend class SSL_ConnectionState;
	friend class SSL;
	friend class SSL_Record_Layer;
	friend class SSL_Auto_Root_Updater;
	friend class SSLEAY_CertificateHandler;
	friend class LibsslModule;
	friend class SSL_Certificate_Installer;
	friend class SSL_XML_Updater;
	friend class SSL_ProtocolVersion;
private:
    int referencecount;

	BOOL initializing;
	OpFileFolder storage_folder;

    SSL_Security_ProfileList_Pointer current_security_profile,default_security_profile;
    SSL_Security_ProfileList_Pointer security_profile;
    
    SSL_CertificateHead System_ClientCerts;
    SSL_CertificateHead System_TrustedCAs;
    SSL_CertificateHead System_IntermediateCAs;
    SSL_CertificateHead System_TrustedSites;
    SSL_CertificateHead System_UnTrustedSites;
    
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	SSL_Head	AutoRetrieved_certs; // Failed requests, the successful ones will be in the CA database
	SSL_Head	AutoRetrieved_untrustedcerts; // Failed requests, the successful ones will be in the Untrusted database
#endif
	SSL_Head	AutoRetrieved_urls; // Failed requests, the successful ones will be in the CRL cache
    
    SSL_CipherDescriptionHead SystemCiphers;
    SSL_CipherSuites SystemCipherSuite;

	SSL_CipherSuites StrongCipherSuite;
   	BOOL some_secure_ciphers_are_disabled;

	SSL_CipherSuites DHE_Reduced_CipherSuite;

    BOOL Enable_SSL_V3_0;
#define Enable_SSL_V3_1 Enable_TLS_V1_0
    BOOL Enable_TLS_V1_0;
    BOOL Enable_TLS_V1_1;
#ifdef _SUPPORT_TLS_1_2
	BOOL Enable_TLS_V1_2;
#endif
    
    BOOL SecurityEnabled,ComponentsLacking;
        
    uint16 PasswordAging;
	uint16 PasswordLockCount;
    
    SSL_secure_varvector32 SystemPartPassword;
    SSL_secure_varvector32 SystemPartPasswordSalt;
    SSL_secure_varvector32 SystemCompletePassword;
    uint32 keybase_version;
    uint32 usercert_base_version;
	time_t SystemPasswordVerifiedLast;
    
	SSL_GeneralCipher *obfuscation_cipher;
    SSL_secure_varvector32 obfuscated_SystemCompletePassword;
	SSL_secure_varvector32 obfuscator_iv;

	SSL_varvector24_list	root_repository_list;
	SSL_varvector24_list	untrusted_repository_list;

	AutoDeleteHead		crl_override_list; /* SSL_CRL_Override_Item */
	OpString_list		ocsp_override_list;
    
    BOOL loaded_primary, loaded_cacerts, loaded_intermediate_cacerts, loaded_usercerts;
	BOOL loaded_trusted_serves, loaded_untrusted_certs;

    BOOL updated_protocols, updated_v2_ciphers, updated_v3_ciphers;
	BOOL updated_password_aging, updated_password, updated_external_items;
	BOOL updated_repository;
	BOOL register_updates;

#ifdef SSL_DH_SUPPORT  
    //SSL_ServerDHParams ServerDHParam;
#endif // SSL_DH_SUPPORT

	SSLSecurtityPasswordCallbackImpl *ask_security_password;

private:
	BOOL ReadNewConfigFileL(BOOL &sel, BOOL &upgrade);

	BOOL ReadNewCertFileL(const uni_char *filename, SSL_CertificateStore store, uint32 recordtag, SSL_CertificateHead *base, BOOL &upgrade);
	void SaveCertificateFileL(uint32 sel);

	void InternalInit(OpFileFolder folder);
	void InternalDestruct();

	SSL_CertificateHead *GetCertificateStore(SSL_CertificateStore store);
	SSL_CertificateHead *MapSelectionToStore(uint32 sel, BOOL *&loaded_flag, const uni_char *&filename, SSL_CertificateStore &store, uint32	&recordtag);
	SSL_CertificateHead *MapToStore(SSL_CertificateStore store, BOOL *&loaded_flag, const uni_char *&filename, uint32	&recordtag);
	
public:

    int dec_reference(){ referencecount --; return referencecount;};
    int inc_reference(){ referencecount ++; return referencecount;};
	OP_STATUS Init(SSL_CertificateStore store);

	/*
	 * @param  sel which stores to load. These are bit values
	 * 		   		so different stores can be combined using "|" operator.
	 *				These stores are available:
	 *
	 *  			SSL_LOAD_CLIENT_STORE
	 *				SSL_LOAD_INTERMEDIATE_CA_STORE
	 *				SSL_LOAD_CA_STORE
	 *				SSL_LOAD_TRUSTED_STORE
	 *				SSL_LOAD_UNTRUSTED_STORE
	 *
	 *  			SSL_LOAD_ALL_STORES
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR for generic errors.
	 **/

    OP_STATUS Init(int sel = 0);
    OP_STATUS InitL(int sel = 0);
    void Save();
    void SaveL();

    /**
     * OOM fix: Init must be called after constructed.
     * Init may return ERR_NO_MEMORY.
     */

private:
    SSL_Options(OpFileFolder folder);
public:
    ~SSL_Options();

    /* From OpPrefsListener */
    virtual void PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);

	void PartialShutdownCertificate();

	void	Set_RegisterChanges(BOOL val);
	BOOL	Get_RegisterChanges(){return register_updates;};
	BOOL	LoadUpdates(SSL_Options *updateManager);

	/* For feature testing check if a feature to be tested have its protocol versions enabled, always returns TRUE for final features, as this state reflects what the server is able to handle */
    BOOL IsTestFeatureAvailable(TLS_Feature_Test_Status protocol);

	/* Check if a given protocol is enabled in preferences */
    BOOL IsEnabled(UINT8 major, UINT8 minor);

    SSL_Security_ProfileList *FindSecurityProfile(ServerName *server, unsigned short port);
    SSL_CipherDescriptions *GetCipherDescription(const SSL_CipherID &);
    SSL_CipherDescriptions *GetCipherDescription(const SSL_ProtocolVersion &ver, int i);
    
    SSL_PublicKeyCipher *FindPrivateKey(SSL_CertificateHandler_List *, SSL_Alert &msg);
    
    BOOL GetCipherName(const SSL_ProtocolVersion &ver, int i, OpString &name, BOOL &selected);
    void SetCiphers(const SSL_ProtocolVersion &ver, int num, int *ilist);
	
	SSL_CertificateItem *Find_Certificate(SSL_CertificateStore store,int n);
	
	// SSL_CertificateItem *Find_Server_Certificate(int n);
	// SSL_CertificateItem *Find_Client_Certificate(int n);

	/** Check if we should encrypt client certificates.
	 *
	 * We encrypt if user has set the master password.
	 *
	 * @return TRUE if client certificate encryption is on.
	 */
	BOOL EncryptClientCertificates() const;
    BOOL Get_Certificate_title(SSL_CertificateStore store, int n, OpString &title);
    BOOL Get_CertificateSubjectName(SSL_CertificateStore store, int n, OpString_list &target);
    BOOL Get_CertificateIssuerName(SSL_CertificateStore store, int n, OpString_list &target);
    BOOL Get_Certificate(SSL_CertificateStore store, int n, 
		URL	&info,
		BOOL &warn_if_used, BOOL &deny_if_used,
		Str::LocaleString description=Str::NOT_A_STRING, SSL_Certinfo_mode info_type= SSL_Cert_XML);
    void Remove_Certificate(SSL_CertificateStore store, int n);
    void Set_Certificate(SSL_CertificateStore store, int n, BOOL warn_if_used, BOOL deny_if_used);

    int BuildChain(SSL_CertificateHandler_List *certs, SSL_CertificateItem *basecert,
					SSL_DistinguishedName_list &ca_names, SSL_Alert &msg);

	BOOL	GetCanFetchIssuerID(SSL_varvector24 &issuer_id);
	void	SetHaveCheckedIssuerID(SSL_varvector24 &issuer_id);
	BOOL	GetHaveCheckedURL(URL &url);
	void	SetHaveCheckedURL(URL &url, time_t expire_time);
	BOOL	GetCanFetchUntrustedID(SSL_varvector24 &subject_id, BOOL &in_list);
	void	SetHaveCheckedUntrustedID(SSL_varvector24 &subject_id);

    SSL_CertificateItem *FindTrustedCA(const SSL_DistinguishedName &name, SSL_CertificateItem *after = NULL);
    SSL_CertificateItem *FindClientCertByKey(const SSL_varvector16 &keyhash);
    SSL_CertificateItem *FindTrustedServer(const SSL_ASN1Cert &cert, ServerName *server, unsigned short port, CertAcceptMode mode=CertAccept_Site);
    SSL_CertificateItem *FindUnTrustedCert(const SSL_ASN1Cert &cert);
    SSL_CertificateItem *AddTrustedServer(SSL_ASN1Cert &cert, ServerName *server, unsigned short port, BOOL expired, CertAcceptMode mode);
    SSL_CertificateItem *AddUnTrustedCert(SSL_ASN1Cert &cert);
	SSL_CertificateItem *Find_Certificate(SSL_CertificateStore store, const SSL_DistinguishedName &name, SSL_CertificateItem *after = NULL);
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	SSL_CertificateItem *Find_CertificateByID(SSL_CertificateStore store, const SSL_varvector32 &name, SSL_CertificateItem *after = NULL);
#endif
	void AddCertificate(SSL_CertificateStore store, SSL_CertificateItem *new_item);

    int DecryptWithPassword(const SSL_varvector32 &encrypted, const SSL_varvector32 &encrypted_salt,
		SSL_secure_varvector32 &decrypted, const SSL_secure_varvector32 &password,
		const char *plainseed);
    void EncryptWithPassword(const SSL_secure_varvector32 &plain, 
		SSL_varvector32 &encrypted, SSL_varvector32 &encrypted_salt,
		const SSL_secure_varvector32 &password, const char *plainseed_text);
#if defined (LIBSSL_SECURITY_PASSWD) 
	void StartSecurityPasswordSession();
	void EndSecurityPasswordSession();

	OP_STATUS DecryptPrivateKey(int cert_number, SSL_secure_varvector32 &decryptedkey, SSL_dialog_config &config);
	OP_STATUS DecryptPrivateKey(SSL_varvector32 &encryptedkey, SSL_varvector32 &encryptedkey_salt, SSL_secure_varvector32 &decryptedkey, SSL_PublicKeyCipher *key, SSL_dialog_config &config);
	OP_STATUS DecryptData(const SSL_varvector32 &encrypteddata, const SSL_varvector32 &encrypteddata_salt, SSL_secure_varvector32 &decrypted_data,
							  const char* pwd, SSL_dialog_config &config);
	OP_STATUS EncryptData(SSL_secure_varvector32 &plain_data, SSL_varvector32 &encrypteddata, SSL_varvector32 &encrypteddata_salt,
							  const char* pwd, SSL_dialog_config &config);
	OP_STATUS GetPassword(SSL_dialog_config &config);
	SSL_PublicKeyCipher *FindPrivateKey(OP_STATUS &op_err, SSL_CertificateHandler_List *cert, SSL_secure_varvector32 &decryptedkey, SSL_Alert &msg, SSL_dialog_config &config);
#ifndef SSL_DISABLE_CLIENT_CERTIFICATE_INSTALLATION
	OP_STATUS AddPrivateKey(SSL_BulkCipherType type, uint32 bits, SSL_secure_varvector32 &privkey, 
							SSL_varvector16 &pub_key_hash, const OpStringC &url_arg, SSL_dialog_config &config);
#endif
#endif

#ifdef LIBOPEAY_PKCS12_SUPPORT
	BOOL ExportPKCS12_Key_and_Certificate(const OpStringC &filename, SSL_secure_varvector32 &priv_key, int n, const OpStringC8 &password);
#endif
	OP_STATUS Export_Certificate(const OpStringC &filename, SSL_CertificateStore store, int n, BOOL multiple, BOOL save_as_pem, BOOL full_pkcs7);

	void Obfuscate();
	void DeObfuscate(SSL_secure_varvector32 &target);
	void ClearObfuscated();

	void UseSecurityPassword(const char *passwd);
    void CheckPasswordAging();
    BOOL CheckPassword(const SSL_secure_varvector32 &password);
    void ClearSystemCompletePassword();


	void RemoveSensitiveData();

	/*
    const char *GetCipherName(SSL_version_cipher_list version, uint16 number);
    BOOL ResizeCipherList(SSL_version_cipher_list version, uint16 number); 
    BOOL UpdateCipherList(SSL_version_cipher_list version, uint16 number,const char *);
	*/

	/**
	 * Checks whether a security password has been set.
	 *
	 * @return TRUE if a password has been set, FALSE otherwise.
	 */
	BOOL IsSecurityPasswordSet() const
	{
		return SystemCompletePassword.GetLength() > 0;
	}

	/// @return the number of Trusted Certification Authorities
	UINT32 GetNumberTrustedCAs() { return System_TrustedCAs.Cardinal(); }
	/// @return the number of Intermediate Certification Authorities
	UINT32 GetNumberIntermediateCAs() { return System_IntermediateCAs.Cardinal(); }
	/// @return TRUE if SSL V3.0 is enabled
	BOOL IsSSLv30Enabled() { return Enable_SSL_V3_0; }

private:
	BOOL ReEncryptDataBase(const ObfuscatedPassword *old_password, const ObfuscatedPassword *new_password);
    BOOL ReEncryptDataBase(SSL_secure_varvector32 &old_complete_password, SSL_secure_varvector32 &new_complete_password);

    void AddSystemCiphers(SSL_CipherDescriptions *(*func)(unsigned int),int); 
	void ConfigureSystemCiphers();	

	friend class CryptoMasterPasswordHandler;
	friend class Options_AskPasswordContext;
};

BOOL CheckPasswordPolicy(const SSL_secure_varvector32 &password);

#include "modules/libssl/options/ssltags.h"

#endif	// _NATIVE_SSL_SUPPORT_

#endif /* SSLOPT_H */
