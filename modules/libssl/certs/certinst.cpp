/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#ifdef USE_SSL_CERTINSTALLER
#include "modules/libssl/sslbase.h"

#include "modules/libssl/sslrand.h"
#include "modules/libssl/ssl_api.h"

#include "modules/url/url2.h"
#include "modules/cache/url_stream.h"

#include "modules/libssl/certs/certinstaller.h"
#include "modules/libssl/ui/certinst.h"

#include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"

#include "modules/util/opautoptr.h"

OP_STATUS ExtractCertificates(SSL_varvector32 &input, const OpStringC8 &pkcs12_password,
							  SSL_ASN1Cert_list &certificate_list, SSL_secure_varvector32 &private_key,
							  SSL_varvector16 &pub_key_hash, uint16 &bits, SSL_BulkCipherType &type);
BOOL load_PEM_certificates2(SSL_varvector32 &data_source, SSL_varvector32 &pem_content);
OP_STATUS ParseCommonName(const OpString_list &info, OpString &title);

SSL_Certificate_Installer_Base *SSL_API::CreateCertificateInstallerL(URL &source, const SSL_Certificate_Installer_flags &install_flags, SSL_dialog_config *config, SSL_Options *optManager)
{
	if(config == NULL)
	{
		// Non-interactive
		SSL_Certificate_Installer *result = OP_NEW_L(SSL_Certificate_Installer, ());
		OP_STATUS op_err = result->Construct(source, install_flags, optManager);
		if(OpStatus::IsError(op_err))
		{
			OP_DELETE(result);
			LEAVE(op_err);
		}
		return result;
	}
	else
	{
		// Interactive
		SSL_Interactive_Certificate_Installer *result = OP_NEW_L(SSL_Interactive_Certificate_Installer, ());
		OP_STATUS op_err = result->Construct(source, install_flags, *config, optManager);
		if(OpStatus::IsError(op_err))
		{
			OP_DELETE(result);
			LEAVE(op_err);
		}
		return result;
	}
}

SSL_Certificate_Installer_Base *SSL_API::CreateCertificateInstallerL(SSL_varvector32 &source, const SSL_Certificate_Installer_flags &install_flags, SSL_dialog_config *config, SSL_Options *optManager)
{
	if(config == NULL)
	{
		// Non-interactive
		SSL_Certificate_Installer *result = OP_NEW_L(SSL_Certificate_Installer, ());
		OP_STATUS op_err = result->Construct(source, install_flags, optManager);
		if(OpStatus::IsError(op_err))
		{
			OP_DELETE(result);
			LEAVE(op_err);
		}
		return result;
	}
	else
	{
		// Interactive
		SSL_Interactive_Certificate_Installer *result = OP_NEW_L(SSL_Interactive_Certificate_Installer, ());
		OP_STATUS op_err = result->Construct(source, install_flags, *config, optManager);
		if(OpStatus::IsError(op_err))
		{
			OP_DELETE(result);
			LEAVE(op_err);
		}
		return result;
	}
}

SSL_Certificate_Installer::SSL_Certificate_Installer()
: decoded_data(FALSE),
  bits(0), type(SSL_RSA),
  certificate(NULL),
  optionsManager(NULL),
  created_manager(FALSE),
  store(SSL_Unknown_Store), 
  warn_before_use(TRUE),
  forbid_use(TRUE),
  overwrite_exisiting(FALSE),
  set_preshipped(FALSE),
  is_pem(FALSE),
  finished_basic_install(FALSE),
  basic_install_success(FALSE),
  external_options(FALSE)
{
}

SSL_Certificate_Installer::~SSL_Certificate_Installer()
{
	OP_DELETE(certificate);
	certificate = NULL;

	if(optionsManager && optionsManager->dec_reference() == 0)
		OP_DELETE(optionsManager);
	optionsManager = NULL;

	password_import_key.Wipe();
}

OP_STATUS SSL_Certificate_Installer::StartInstallation()
{
	finished_basic_install = FALSE;
	basic_install_success = FALSE;
	OP_STATUS op_err;
	op_err = PrepareInstallation();
	if(OpStatus::IsSuccess(op_err))
	{
		SSL_dialog_config config; // Empty, no action
		op_err = PerformInstallation(config);
	}

	if(OpStatus::IsError(op_err))
	{
		finished_basic_install = TRUE;
		basic_install_success = FALSE;
		return op_err;
	}

	basic_install_success = TRUE;
	InstallationLastStep();
	finished_basic_install = TRUE;

	return InstallerStatus::INSTALL_FINISHED;
}

BOOL SSL_Certificate_Installer::Finished()
{
	return finished_basic_install;
}

BOOL SSL_Certificate_Installer::InstallSuccess()
{
	return basic_install_success;
}

Str::LocaleString SSL_Certificate_Installer::ErrorStrings(OpString &info)
{
	info.Empty();

	Install_errors *message = (Install_errors *) install_messages.First();

	if(!message)
		return Str::S_NOT_A_STRING;

	Str::LocaleString msg = message->message;
	info.TakeOver(message->info);

	message->Out();
	OP_DELETE(message);

	return msg;
}

OP_STATUS SSL_Certificate_Installer::Construct(URL &source, const SSL_Certificate_Installer_flags &inst_flags, SSL_Options *optManager)
{
	SSL_varvector32 data;

	// Load the certificate
	URL_DataStream source_stream(source, TRUE);

	TRAPD(op_err, data.AddContentL(&source_stream));
	if(OpStatus::IsError(op_err))
	{
		basic_install_success = FALSE;
		finished_basic_install = TRUE;
		return op_err;
	}

	if(data.ErrorRaisedFlag || data.GetLength() == 0)
	{
		//AddErrorString(Str::SI_MSG_SECURE_ILLEGAL_FORMAT, NULL);
		basic_install_success = FALSE;
		finished_basic_install = TRUE;
		return OpStatus::ERR_PARSING_FAILED;
	}

	return Construct(data, inst_flags, optManager);
}

OP_STATUS SSL_Certificate_Installer::Construct(DataStream_ByteArray_Base &source1, const SSL_Certificate_Installer_flags &inst_flags, SSL_Options *optManager)
{
	SSL_varvector32 data;

	data.Set(source1);
	RETURN_IF_ERROR(data.GetOPStatus());

	return Construct(data, inst_flags, optManager);
}

OP_STATUS SSL_Certificate_Installer::PreprocessVector(SSL_varvector32& source, SSL_varvector32& target)
{
	target.Resize(0);

	uint32 source_length = source.GetLength();

	if (source_length == 0)
		return OpStatus::ERR;

	// The PEM parser expects to have a trailing '\0' in the
	// buffer, however it might not be present - and it can
	// not be added to the original SSL_varvector32 instance
	// if its payload has been set from elsewhere.  Therefore
	// we clone it so we can append the trailing '\0' ourselves
	// and be sure that string-related functions will not bring
	// the process down with a crash.

	SSL_varvector32 clone;

	clone.Set(source);
	RETURN_IF_ERROR(clone.GetOPStatus());
	clone.Append("\0", 1);
	RETURN_IF_ERROR(clone.GetOPStatus());

	// Detect PEM source
	uint32 pos = 0;
	byte *data = clone.GetDirect();
	BOOL pem_found = FALSE;

	while (pos +10 <= source_length)
	{
		if(!op_isspace(data[pos]))
		{
			if(op_strnicmp((char *) data+pos, "-----BEGIN",10) == 0)
			{
				if (!load_PEM_certificates2(clone, target))
					return OpStatus::ERR_PARSING_FAILED;

				if (target.GetLength() == 0)
					return OpStatus::ERR_PARSING_FAILED;

				pem_found = TRUE;

				// Stop after the first certificate

				break;
			}
		}

		pos ++;
	}

	if (!pem_found)
	{
		target.Set(source);
		RETURN_IF_ERROR(target.GetOPStatus());
	}

	return OpStatus::OK;
}

OP_STATUS SSL_Certificate_Installer::Construct(SSL_varvector32& source, const SSL_Certificate_Installer_flags &inst_flags, SSL_Options *optManager)
{
	if(optManager != NULL)
	{
		optionsManager = optManager;
		optionsManager->inc_reference();
		external_options = TRUE;
	}
	else
	{
		optionsManager = g_ssl_api->CreateSecurityManager(TRUE);
		if(optionsManager == NULL)
			return OpStatus::ERR_NO_MEMORY;

		external_options = FALSE;
	}

	OP_STATUS status = PreprocessVector(source, original_data);
	if (OpStatus::IsError(status))
	{
		original_data.Resize(0);
		basic_install_success = FALSE;
		finished_basic_install = TRUE;
		return status;
	}

	store = inst_flags.store;
	warn_before_use = inst_flags.warn_before_use;
	forbid_use = inst_flags.forbid_use;

	return OpStatus::OK;
}

OP_STATUS SSL_Certificate_Installer::AddErrorString(Str::LocaleString  msg, const OpStringC &info)
{
	if(msg == Str::S_NOT_A_STRING)
		return OpStatus::OK;

	Install_errors *message = OP_NEW(Install_errors, ());
	if(message == NULL)
		return OpStatus::ERR_NO_MEMORY;

	message->message = msg;
	OpStatus::Ignore(message->info.Set(info)); // OOM usually not serious in this case

	message->Into(&install_messages);

	return OpStatus::OK;
}

OP_STATUS SSL_Certificate_Installer::PrepareInstallation()
{
	OP_STATUS op_err = ExtractCertificates(original_data, password_import_key, original_cert, private_key, pub_key_hash, bits, type);
	if(OpStatus::IsError(op_err))
	{
		switch(op_err)
		{
		case InstallerStatus::ERR_UNSUPPORTED_KEY_ENCRYPTION :
			AddErrorString(Str::S_MSG_SECURE_INSTALL_UNSUPPORTED_ENCRYPTION, NULL);
			return op_err;
		case InstallerStatus::ERR_PASSWORD_NEEDED:
#ifndef SSL_DISABLE_CLIENT_CERTIFICATE_INSTALLATION
			return  InstallerStatus::ERR_PASSWORD_NEEDED;
#else
			op_err = InstallerStatus::ERR_INSTALL_FAILED;
#endif
			// Fall through if support is disabled
		default  :
			AddErrorString(Str::SI_MSG_SECURE_INSTALL_FAILED, NULL);
			return  op_err;
		}
	}

	if(original_cert.Count() == 0)
		return  private_key.GetLength() ? OpStatus::OK : InstallerStatus::ERR_PARSING_FAILED;

	if(private_key.GetLength() != 0)
		store = SSL_ClientStore;

	op_err = VerifyCertificate();
	RETURN_IF_ERROR(op_err);
	if(op_err == InstallerStatus::VERIFYING_CERT)
		return op_err;

	return CheckClientCert();
}

OP_STATUS SSL_Certificate_Installer::VerifyCertificate()
{
	uint32 i, count;

	certificate = g_ssl_api->CreateCertificateHandler();
	if(certificate == NULL)
		return OpStatus::ERR_NO_MEMORY;

	certificate->LoadCertificate(original_cert);

	SSL_Alert msg;
	if(!certificate->VerifySignatures((store == SSL_ClientStore ? SSL_Purpose_Client_Certificate : SSL_Purpose_Any), &msg
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
		, NULL
#endif // LIBSSL_ENABLE_CRL_SUPPORT
		, optionsManager, TRUE // The certificate may be new. No reason to trigger an error, so this is a "standalone" verification
		))
	{
		switch(msg.GetDescription())
		{
		case SSL_Certificate_Expired:
			AddErrorString(Str::SI_MSG_HTTP_SSL_Certificate_Expired, NULL);
			return  InstallerStatus::ERR_PARSING_FAILED;
		default  :
			AddErrorString(Str::SI_MSG_SECURE_INSTALL_FAILED, NULL);
			return  InstallerStatus::ERR_PARSING_FAILED;
		}
	}

	// Check that the certificate chain is correctly ordered so that the user does not have to
	// parse the chain, and that there is only one chain, not multiple chains.
	SSL_ASN1Cert_list val_certs;
	certificate->GetValidatedCertificateChain(val_certs);
	if(val_certs.Error())
	{
		return val_certs.GetOPStatus();
	}

	count = certificate->CertificateCount();
	if(count >val_certs.Count())
	{
		AddErrorString(Str::SI_MSG_SECURE_INVALID_CHAIN, NULL);
		return  InstallerStatus::ERR_PARSING_FAILED;
	}
	OP_ASSERT(val_certs.Count() > 0);

	for(i = 0; i< count; i++)
	{
		if(val_certs[i] != original_cert[i])
		{
			AddErrorString(Str::SI_MSG_SECURE_INVALID_CHAIN, NULL);
			return  InstallerStatus::ERR_PARSING_FAILED;
		}
	}

	SSL_CertificateHandler *val_certificate = g_ssl_api->CreateCertificateHandler();
	if(val_certificate == NULL)
		return OpStatus::ERR_NO_MEMORY;

	val_certificate->LoadCertificate(val_certs);

	if(!val_certificate->SelfSigned(count-1))
	{
		// Incomplete chain
		// TO-DO: Action
	}

	OP_DELETE(val_certificate);

	return InstallerStatus::OK;
}

OP_STATUS SSL_Certificate_Installer::CheckClientCert()
{
	SSL_varvector16 key_hash;

	if(store == SSL_ClientOrCA_Store)
	{
		certificate->GetPublicKeyHash(0,key_hash);

		RETURN_IF_ERROR(optionsManager->Init(SSL_ClientStore));
		SSL_CertificateItem *clientcert = optionsManager->FindClientCertByKey(key_hash);
		store = (clientcert == NULL ? SSL_CA_Store : SSL_ClientStore);
	}

#ifdef SSL_DISABLE_CLIENT_CERTIFICATE_INSTALLATION
	if(store == SSL_ClientStore)
	{
		AddErrorString(Str::SI_MSG_SECURE_INSTALL_FAILED, NULL);
		return InstallerStatus::ERR_INSTALL_FAILED;
	}
#else // !SSL_DISABLE_CLIENT_CERTIFICATE_INSTALLATION
	if(store == SSL_ClientStore && private_key.GetLength() == 0)
	{
		certificate->GetPublicKeyHash(0,key_hash);

		RETURN_IF_ERROR(optionsManager->Init(SSL_ClientStore));
		SSL_CertificateItem *clientcert = optionsManager->FindClientCertByKey(key_hash);
		if(clientcert == NULL)
		{
			AddErrorString(Str::SI_MSG_SECURE_NO_KEY, NULL);
			return  InstallerStatus::ERR_PARSING_FAILED;
		}

		if(clientcert->certificate.GetLength() != 0)
		{
			if(clientcert->certificate != original_cert[0])
			{
				AddErrorString(Str::SI_MSG_SECURE_DIFFERENTCLIENT, NULL);
				return  InstallerStatus::ERR_PARSING_FAILED;
			}
			return InstallerStatus::INSTALL_FINISHED;
		}
	}
#endif // SSL_DISABLE_CLIENT_CERTIFICATE_INSTALLATION

	return InstallerStatus::OK;
}

OP_STATUS SSL_Certificate_Installer::PerformInstallation(SSL_dialog_config &config)
{
#ifndef SSL_DISABLE_CLIENT_CERTIFICATE_INSTALLATION
	if(private_key.GetLength() != 0)
	{
		RETURN_IF_ERROR(optionsManager->Init(SSL_ClientStore));
		RETURN_IF_ERROR(optionsManager->AddPrivateKey(type, bits, private_key, pub_key_hash, (const uni_char *) NULL, config));
	}
#endif

	SSL_varvector16 key_hash, key_hash2;
	SSL_DistinguishedName subject, issuer, issuer2;
	OpString_list info;

	uint24 count = original_cert.Count();
	if(count)
	{
		SSL_CertificateStore current_store = store;

		uint24 i;
		for(i = 0; i< count; i++)
		{
			if(current_store == SSL_ClientStore)
			{
				RETURN_IF_ERROR(optionsManager->Init(SSL_ClientStore));
				certificate->GetPublicKeyHash(0,key_hash);

				SSL_CertificateItem *clientcert = optionsManager->FindClientCertByKey(key_hash);
				if(clientcert == NULL || clientcert->certificate.GetLength() != 0)
				{
					if (clientcert && (clientcert->certificate.GetLength() != 0))
						return InstallerStatus::ERR_CERTIFICATE_ALREADY_PRESENT;

					return InstallerStatus::ERR_INSTALL_FAILED;
				}

				clientcert->certificatetype = certificate->CertificateType(0);
				clientcert->certificate = original_cert[0];
				certificate->GetSubjectName(0,clientcert->name);
				clientcert->cert_title.Empty();
				if (
					OpStatus::IsError(certificate->GetSubjectName(0,info)) ||
					OpStatus::IsError(ParseCommonName(info, clientcert->cert_title) ))
				{
					return InstallerStatus::ERR_INSTALL_FAILED;
				}

				if(optionsManager->register_updates && clientcert->cert_status != Cert_Inserted)
					clientcert->cert_status = Cert_Updated;
				current_store = SSL_CA_Store;
			}
			else
			{
				certificate->GetSubjectName(i, subject);

				SSL_CertificateStore use_store = current_store;
				if(use_store == SSL_CA_Store && !certificate->SelfSigned(i))
					use_store = SSL_IntermediateCAStore;

				RETURN_IF_ERROR(optionsManager->Init(use_store));
				SSL_CertificateItem *last_item = NULL;
				SSL_CertificateItem *cert_item;
				do{
					cert_item = optionsManager->Find_Certificate(use_store, subject, last_item);
					if(cert_item == NULL && use_store != current_store)
						cert_item = optionsManager->Find_Certificate(current_store, subject, last_item);
					last_item = cert_item;

					if(cert_item)
					{
						SSL_CertificateHandler *handler = cert_item->GetCertificateHandler();

						if(handler)
						{
							certificate->GetIssuerName(i, issuer);
							handler->GetIssuerName(i, issuer2);
							if(issuer != issuer2)
								continue;

							certificate->GetPublicKeyHash(i,key_hash);
							handler->GetPublicKeyHash(i,key_hash2);

							if(key_hash != key_hash2)
								continue;

							if(!overwrite_exisiting)
								break; // Do not overwrite exisiting

							// Delete the old object; commit is not able to override the old.
							if(optionsManager->register_updates)
							{
								cert_item->cert_status = Cert_Deleted;
							
								OP_DELETE(cert_item->handler);
								cert_item->handler = NULL;
							}
							else
							{
								cert_item->Out();
								OP_DELETE(cert_item);
							}
							cert_item = NULL;
						}
						else
							continue;
					}

					if(cert_item == NULL)
					{
						cert_item = OP_NEW(SSL_CertificateItem, ());
						if(cert_item == NULL)
						{
							return InstallerStatus::ERR_INSTALL_FAILED;
						}
						if(optionsManager->register_updates)
							cert_item->cert_status = Cert_Inserted;
					}
					else if(optionsManager->register_updates)
						cert_item->cert_status = Cert_Updated;
					cert_item->certificate = original_cert[i];

					cert_item->certificatetype = certificate->CertificateType(i);
					cert_item->WarnIfUsed = (i != count-1 || warn_before_use);
					cert_item->DenyIfUsed = (i == count-1 && forbid_use);
					cert_item->cert_title.Set(suggested_name);
					if(set_preshipped)
						cert_item->PreShipped = TRUE;

					OP_STATUS ParseCommonName(const OpString_list &info, OpString &title);
					certificate->GetSubjectName(i,cert_item->name);
					if (
						OpStatus::IsError(certificate->GetSubjectName(i,info)) ||
						(cert_item->cert_title.IsEmpty() && OpStatus::IsError(ParseCommonName(info, cert_item->cert_title))) )
					{
						if(!cert_item->InList())
							OP_DELETE(cert_item);
						return InstallerStatus::ERR_INSTALL_FAILED;
					}

					if(!cert_item->InList())
						optionsManager->AddCertificate(use_store, cert_item);
					break;
				}while(cert_item != NULL);
			}
		}
	}
	return InstallerStatus::OK;
}

void SSL_Certificate_Installer::InstallationLastStep()
{
	if(InstallSuccess() && !external_options)
		g_ssl_api->CommitOptionsManager(optionsManager);
}

OP_STATUS SSL_Certificate_Installer::SetImportPassword(const char *pass)
{
	return password_import_key.Set(pass);
}

#endif // USE_SSL_CERTINSTALLER
#endif // relevant support
