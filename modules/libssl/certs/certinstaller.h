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


#ifndef SSL_CERTINST_H
#define SSL_CERTINST_H

#if defined _NATIVE_SSL_SUPPORT_ && defined USE_SSL_CERTINSTALLER

#include "modules/libssl/certs/certinst_base.h"
#include "modules/libssl/handshake/asn1certlist.h"

class SSL_CertificateHandler;
struct SSL_dialog_config;


class SSL_Certificate_Installer : public SSL_Certificate_Installer_Base
{
protected:
	SSL_varvector32 original_data;
	BOOL decoded_data;
	SSL_ASN1Cert_list original_cert;

	SSL_secure_varvector32 private_key;
	SSL_varvector16 pub_key_hash;
	uint16 bits;
	SSL_BulkCipherType type;

	OpString8 password_import_key;


	SSL_CertificateHandler *certificate;
	SSL_Options *optionsManager;
	BOOL created_manager;

	SSL_CertificateStore store;
	OpString suggested_name;
	BOOL warn_before_use;
	BOOL forbid_use;
	BOOL overwrite_exisiting;
	BOOL set_preshipped;

	BOOL is_pem;

private:

	BOOL finished_basic_install;
	BOOL basic_install_success;

	BOOL external_options;

	struct Install_errors : public Link
	{
	public:
		Str::LocaleString message;
		OpString info;

		Install_errors(): message(Str::S_NOT_A_STRING){}
	};

	AutoDeleteHead  install_messages; // List of Install_errors

public:


	SSL_Certificate_Installer();
	virtual ~SSL_Certificate_Installer();

	virtual OP_STATUS StartInstallation();
	virtual BOOL Finished();
	virtual BOOL InstallSuccess();
	virtual Str::LocaleString ErrorStrings(OpString &info);
	virtual OP_STATUS SetImportPassword(const char *pass);

	OP_STATUS Construct(URL &source, const SSL_Certificate_Installer_flags &install_flags, SSL_Options *optManager=NULL);
	OP_STATUS Construct(SSL_varvector32 &source,  const SSL_Certificate_Installer_flags &install_flags, SSL_Options *optionsManager= NULL);
	OP_STATUS Construct(DataStream_ByteArray_Base &source,  const SSL_Certificate_Installer_flags &install_flags, SSL_Options *optionsManager= NULL);

	void SetSuggestedName(const OpStringC &name){OpStatus::Ignore(suggested_name.Set(name));}

	void Raise_SetPreshippedFlag(BOOL a_preshipped){set_preshipped = a_preshipped;}

protected:

	OP_STATUS AddErrorString(Str::LocaleString  msg, const OpStringC &info);

	/** This call decodes the original DER data, and in case of PKCS #12 files the password 
	 *	must have been supplied in password_import_key before calling this function, 
	 *	if the password is not accepted InstallerStatus::ERR_PASSWORD_NEEDED is returned,
	 *	and the application should call the function again after providing the password
	 *
	 *	If InstallerStatus::VERIFYING_CERT is returned, then CheckClientCert must be called separately
	 */
	OP_STATUS PrepareInstallation();

	/** Will verify the certificate and return InstallerStatus::OK if successful,
	 *	and InstallerStatus::VERIFYING_CERT if asynchronous verification is used; 
	 *	verification MUST NOT open a dialog as that will be done later.
	 *	default implementation is synchrounous and returns either with OK or a failure
	 */
	virtual OP_STATUS VerifyCertificate();

	/** Check client certificate key, will add key if no certificate is present, then report completed */
	OP_STATUS CheckClientCert();

	
	/** This call performs the final installation of the certificate and the (optional) private key
	 *	A security password is needed for storing the private key, and this password must be stored in 
	 *	password_database before calling the function. If the password is not accepted the error code 
	 *  InstallerStatus::ERR_PASSWORD_NEEDED is returned and the application should call the function 
	 *  again after providing the password
	 */
	OP_STATUS PerformInstallation(SSL_dialog_config &config);

	/** This performs the last steps of the installation, such as committing the options manager, if necessary */
	void InstallationLastStep();

	/** Performs any preprocessing steps on the given certificate data, before
	 * binding the current installer instance with the result of the preprocessing.
	 *
	 * The default implementation checks for PEM-encoded content and isolates it if found,
	 * leaving the source data untouched otherwise.
	 * 
	 * @param[in] source the input data to preprocess.
	 * @param[in] target the target container to write to.
	 *
	 * @return OpStatus::OK if successful, or the appropriate member of the OpStatus enum otherwise.
	 */
	virtual OP_STATUS PreprocessVector(SSL_varvector32& source, SSL_varvector32& target);
};

#endif // USE_SSL_CERTINSTALLER

#endif
