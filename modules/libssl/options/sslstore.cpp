/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/formats/url_dfr.h"

#ifdef WAND_SUPPORT
#include "modules/wand/wandmanager.h"
#endif

#include "modules/rootstore/rootstore_api.h"

SSL_CertificateHead *SSL_Options::MapSelectionToStore(uint32 sel, BOOL *&loaded_flag, const uni_char *&filename, SSL_CertificateStore &store, uint32 &recordtag)
{
	loaded_flag = NULL;
	filename = NULL;
	store = SSL_Unknown_Store;
	recordtag = 0;


	switch(sel)
	{
	case SSL_LOAD_CA_STORE:
		store = SSL_CA_Store;
		break;

	case SSL_LOAD_INTERMEDIATE_CA_STORE:
		store = SSL_IntermediateCAStore;
		break;

	case SSL_LOAD_CLIENT_STORE:
		store = SSL_ClientStore;
		break;

	case SSL_LOAD_TRUSTED_STORE:
		store = SSL_TrustedSites;
		break;
	case SSL_LOAD_UNTRUSTED_STORE:
		store = SSL_UntrustedSites;
		break;
	}

	return MapToStore(store, loaded_flag, filename, recordtag);
}

SSL_CertificateHead *SSL_Options::MapToStore(SSL_CertificateStore store, BOOL *&loaded_flag, const uni_char *&filename, uint32 &recordtag)
{
	loaded_flag = NULL;
	filename = NULL;

	recordtag = 0;

	switch(store)
	{
	case SSL_CA_Store:
		loaded_flag = &loaded_cacerts;
		filename = UNI_L("opcacrt6.dat");
		recordtag = TAG_SSL_CACERTIFICATE_RECORD;
		break;

	case SSL_IntermediateCAStore:
		loaded_flag = &loaded_intermediate_cacerts;
		filename = UNI_L("opicacrt6.dat");
		recordtag = TAG_SSL_CACERTIFICATE_RECORD;
		break;

	case SSL_ClientStore:
		loaded_flag = &loaded_usercerts;
		filename = UNI_L("opcert6.dat");
		recordtag = TAG_SSL_USER_CERTIFICATE_RECORD;
		break;

	case SSL_TrustedSites:
		loaded_flag = &loaded_trusted_serves;
		filename = UNI_L("optrust.dat");
		recordtag = TAG_SSL_TRUSTED_CERTIFICATE_RECORD;
		break;
	case SSL_UntrustedSites:
		loaded_flag = &loaded_untrusted_certs;
		filename = UNI_L("opuntrust.dat");
		recordtag = TAG_SSL_UNTRUSTED_CERTIFICATE_RECORD;
		break;
	}

	return GetCertificateStore(store);;
}

SSL_CertificateHead *SSL_Options::GetCertificateStore(SSL_CertificateStore store)
{
	switch(store)
	{
	case SSL_CA_Store :
		return &System_TrustedCAs;
	case SSL_IntermediateCAStore:
		return &System_IntermediateCAs;
	case SSL_ClientStore :
		return &System_ClientCerts;
	case SSL_TrustedSites :
		return &System_TrustedSites;
	case SSL_UntrustedSites :
		return &System_UnTrustedSites;
	}
	return NULL;
}

OP_STATUS OpenFileL(OpStackAutoPtr<OpFile> &input, OpFileFolder folder, const uni_char *filename);

BOOL SSL_Options::ReadNewCertFileL(const uni_char *filename, SSL_CertificateStore store, uint32 recordtag, SSL_CertificateHead *base, BOOL &upgrade)
{
	if(storage_folder == OPFILE_ABSOLUTE_FOLDER)
		return FALSE; // No file, use defaults

	OpStackAutoPtr<SSL_CertificateItem> current_cert(NULL);
	OpStackAutoPtr<OpFile> input(NULL);

	if( OpStatus::IsError(OpenFileL(input, storage_folder, filename)) )
		return FALSE;

	DataFile prefsfile(input.release());
	ANCHOR(DataFile, prefsfile);
	OpStackAutoPtr<DataFile_Record> rec(NULL);
	DataFile_Record *rec2;
	BOOL read_systempassword = FALSE;
	BOOL status = TRUE;

	prefsfile.InitL();

	keybase_version = prefsfile.AppVersion();
	if(keybase_version == 0)
	{
		prefsfile.Close();
		return FALSE;

	}
	if(keybase_version< SSL_Options_Version_NewBase)
		goto finish;
	if((keybase_version & SSL_Options_Mask) < (SSL_Options_Version &  SSL_Options_Mask))
		goto finish;
#if defined LIBSSL_AUTO_UPDATE
	if(store == SSL_CA_Store && keybase_version< SSL_Options_Version)
		g_main_message_handler->PostMessage(MSG_SSL_START_AUTO_UPDATE, 0, 0);
#endif

	while((rec2 = prefsfile.GetNextRecordL()) != NULL)
	{
		rec.reset(rec2);

		rec->SetRecordSpec(prefsfile.GetRecordSpec());

		if(store == SSL_ClientStore && !read_systempassword && rec->GetTag() == TAG_SSL_USER_PASSWORD_RECORD)
		{
			rec->IndexRecordsL();

			SystemPartPasswordSalt.Resize(0);
			SystemPartPassword.Resize(0);

			read_systempassword =
				(rec->ReadContentFromIndexedRecordL(SystemPartPasswordSalt, TAG_SSL_PASSWD_MAINSALT) == OpRecStatus::FINISHED) &&
				(rec->ReadContentFromIndexedRecordL(SystemPartPassword, TAG_SSL_PASSWD_MAINBLOCK) == OpRecStatus::FINISHED);

			if(!read_systempassword || !(SystemPartPassword.GetLength() >= 128 && SystemPartPasswordSalt.GetLength() >= 8))
			{
				read_systempassword = FALSE;
				SystemPartPasswordSalt.Resize(0);
				SystemPartPassword.Resize(0);
			}
		}
		else if(rec->GetTag() == recordtag)
		{
			rec->IndexRecordsL();

			current_cert.reset(OP_NEW_L(SSL_CertificateItem, ()));

			if(current_cert.get() == NULL)
				break;

			status = TRUE;

			current_cert->certificatetype = (SSL_ClientCertificateType) rec->GetIndexedRecordUIntL(TAG_SSL_CERT_TYPE);
			if(current_cert->certificatetype == 0)
				status = FALSE;

			if(status)
			{
				OpString8 title;
				ANCHOR(OpString8, title);
				rec->GetIndexedRecordStringL(TAG_SSL_CERT_TITLE, title);

				if(title.HasContent())
					current_cert->cert_title.SetFromUTF8L(title.CStr());
				else
					status = FALSE;
			}

			status = status &&
				(rec->ReadContentFromIndexedRecordL(current_cert->name, TAG_SSL_CERT_NAME) == OpRecStatus::FINISHED) &&
				(rec->ReadContentFromIndexedRecordL(current_cert->certificate, TAG_SSL_CERT_CERTIFICATE) == OpRecStatus::FINISHED);

			switch(store)
			{
			case SSL_ClientStore:
			{
				status = status &&
					(rec->ReadContentFromIndexedRecordL(current_cert->private_key, TAG_SSL_USERCERT_PRIVATEKEY) == OpRecStatus::FINISHED) &&
					(rec->ReadContentFromIndexedRecordL(current_cert->public_key_hash, TAG_SSL_USERCERT_PUBLICKEY_HASH) == OpRecStatus::FINISHED);

				OP_STATUS found = rec->ReadContentFromIndexedRecordL(current_cert->private_key_salt, TAG_SSL_USERCERT_PRIVATEKEY_SALT);
				status = status && (found == OpRecStatus::FINISHED ||found ==  OpRecStatus::RECORD_NOT_FOUND);
			}
				break;
			case SSL_IntermediateCAStore:
			case SSL_CA_Store:
				current_cert->WarnIfUsed = rec->GetIndexedRecordBOOL(TAG_SSL_CACERT_WARN);
				current_cert->DenyIfUsed = rec->GetIndexedRecordBOOL(TAG_SSL_CACERT_DENY);
				current_cert->PreShipped = rec->GetIndexedRecordBOOL(TAG_SSL_CACERT_PRESHIPPED);
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
				{
					DataFile_Record *pol_rec = rec->GetIndexedRecord(TAG_SSL_EV_OIDS);
					if(pol_rec && status)
						current_cert->ev_policies.AddContentL(pol_rec);
				}
#endif
				break;
			case SSL_TrustedSites:
				{
					OpString8 servername;
					ANCHOR(OpString8, servername);

					rec->GetIndexedRecordStringL(TAG_SSL_TRUSTED_SERVER_NAME, servername);
					current_cert->trusted_for_name = g_url_api->GetServerName(servername, TRUE);
					current_cert->trusted_for_port = (unsigned short) rec->GetIndexedRecordUIntL(TAG_SSL_TRUSTED_SERVER_PORT);
					current_cert->trusted_until = (time_t) rec->GetIndexedRecordUInt64L(TAG_SSL_TRUSTED_SERVER_UNTIL);

					current_cert->accepted_for_kind = (rec->GetIndexedRecordBOOL(TAG_SSL_TRUSTED_FOR_APPLET) ? CertAccept_Applet : CertAccept_Site);

					if(current_cert->trusted_for_name == NULL || (current_cert->trusted_for_port == 0 && current_cert->accepted_for_kind != CertAccept_Applet))
						status = FALSE;
					break;
				}
			}

			BOOL client_with_no_key = (store == SSL_ClientStore && current_cert->private_key.GetLength() == 0);
			if(client_with_no_key)
				upgrade = TRUE; // Trigger save 

			if(status && !current_cert->Error() &&
				 // make sure only entries with a private key is included in the client certificate repository. See bug CORE-26199
				!client_with_no_key)
			{
				current_cert->Into(base);
				current_cert.release();
			}
			else
			{
				current_cert.reset();
			}

#ifdef SSL_TST_DUMP
			{
				uint32 lcount;

				lcount = base->Cardinal();

				DumpTofile((byte *) &lcount,sizeof(lcount),"Initloading certificate current count","sslinit.txt");
			}
#endif
		}
	}

	rec.reset();

finish:;

	prefsfile.Close();

	switch(store)
	{
	case SSL_ClientStore:
		usercert_base_version = keybase_version;
		loaded_usercerts = TRUE;
		break;
	case SSL_CA_Store:
		loaded_cacerts = TRUE;
#if !defined LIBSSL_AUTO_UPDATE_ROOTS
		if(keybase_version < SSL_Options_Version)
#endif
		{
			g_root_store_api->InitAuthorityCerts(this, keybase_version);
			upgrade = TRUE;
		}
		keybase_version =  SSL_Options_Version;
		break;
	case SSL_IntermediateCAStore:
		loaded_intermediate_cacerts = TRUE;
		break;
	case SSL_TrustedSites:
		loaded_trusted_serves = TRUE;
		break;

	case SSL_UntrustedSites:
		loaded_untrusted_certs = TRUE;
		break;
	}
	keybase_version =  SSL_Options_Version;

	return TRUE;
}


#ifdef PREFS_WRITE
OP_STATUS OpenWriteFileL(OpStackAutoPtr<OpFile> &output, OpFileFolder folder, const uni_char *filename);

void SSL_Options::SaveCertificateFileL(uint32 sel)
{
	if(storage_folder == OPFILE_ABSOLUTE_FOLDER)
		return; // No file;

	BOOL *loaded_flag = NULL;
	const uni_char *filename = NULL;
	SSL_CertificateHead *base = NULL;
	SSL_CertificateStore store = SSL_Unknown_Store;
	uint32 recordtag = 0;

	base = MapSelectionToStore(sel, loaded_flag, filename, store, recordtag);

	if(!base || !loaded_flag || !(*loaded_flag))
		return;


	OpStackAutoPtr<OpFile> output(NULL);
	SSL_CertificateItem *current_cert;

	if( OpStatus::IsSuccess(OpenWriteFileL(output, storage_folder, filename)) )
	{
		DataFile prefsfile(output.release(),SSL_Options_Version, 1, 4);
		ANCHOR(DataFile, prefsfile);

		prefsfile.InitL();

		switch(store)
		{
		case SSL_ClientStore:
			if (SystemPartPassword.GetLength())
			{
				DataFile_Record passwd_entry(TAG_SSL_USER_PASSWORD_RECORD);
				ANCHOR(DataFile_Record, passwd_entry);

				passwd_entry.SetRecordSpec(prefsfile.GetRecordSpec());

				SystemPartPassword.WriteRecordL(&passwd_entry);
				SystemPartPasswordSalt.WriteRecordL(&passwd_entry);

				passwd_entry.WriteRecordL(&prefsfile);
			}
			break;
		}

		current_cert = base->First();
		while (current_cert != NULL)
		{
			OpStackAutoPtr<DataFile_Record> rec(NULL);

			rec.reset(OP_NEW_L(DataFile_Record, (recordtag)));

			rec->SetRecordSpec(prefsfile.GetRecordSpec());

			rec->AddRecordL(TAG_SSL_CERT_TYPE, (uint16) current_cert->certificatetype);

			{
				DataFile_Record cert_entry(TAG_SSL_CERT_TITLE);
				cert_entry.SetRecordSpec(prefsfile.GetRecordSpec());

				char *tmp = current_cert->cert_title.UTF8AllocL();
				cert_entry.AddContentL(tmp);
				OP_DELETEA(tmp);
				cert_entry.WriteRecordL(rec.get());
			}
			{
				DataFile_Record cert_entry(TAG_SSL_CERT_NAME);
				cert_entry.SetRecordSpec(prefsfile.GetRecordSpec());
				current_cert->name.WriteRecordPayloadL(&cert_entry);
				cert_entry.WriteRecordL(rec.get());
			}
			{
				DataFile_Record cert_entry(TAG_SSL_CERT_CERTIFICATE);
				cert_entry.SetRecordSpec(prefsfile.GetRecordSpec());
				current_cert->certificate.WriteRecordPayloadL(&cert_entry);
				cert_entry.WriteRecordL(rec.get());
			}

			switch(store)
			{
			case SSL_ClientStore:
				{
					DataFile_Record cert_entry(TAG_SSL_USERCERT_PRIVATEKEY);
					cert_entry.SetRecordSpec(prefsfile.GetRecordSpec());
					current_cert->private_key.WriteRecordPayloadL(&cert_entry);
					cert_entry.WriteRecordL(rec.get());
				}
				if (current_cert->private_key_salt.GetLength() > 0)
				{
					DataFile_Record cert_entry(TAG_SSL_USERCERT_PRIVATEKEY_SALT);
					cert_entry.SetRecordSpec(prefsfile.GetRecordSpec());
					current_cert->private_key_salt.WriteRecordPayloadL(&cert_entry);
					cert_entry.WriteRecordL(rec.get());
				}
				{
					OP_ASSERT(current_cert->public_key_hash.GetLength() > 0);
					DataFile_Record cert_entry(TAG_SSL_USERCERT_PUBLICKEY_HASH);
					cert_entry.SetRecordSpec(prefsfile.GetRecordSpec());
					current_cert->public_key_hash.WriteRecordPayloadL(&cert_entry);
					cert_entry.WriteRecordL(rec.get());
				}
				break;
			case SSL_CA_Store:
				{
					if(current_cert->WarnIfUsed)
						rec->AddRecordL(TAG_SSL_CACERT_WARN);
					if(current_cert->DenyIfUsed)
						rec->AddRecordL(TAG_SSL_CACERT_DENY);
					if(current_cert->PreShipped)
						rec->AddRecordL(TAG_SSL_CACERT_PRESHIPPED);
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
					if(current_cert->ev_policies.GetLength())
					{
						DataFile_Record cert_entry(TAG_SSL_EV_OIDS);
						cert_entry.SetRecordSpec(prefsfile.GetRecordSpec());
						current_cert->ev_policies.WriteRecordPayloadL(&cert_entry);
						cert_entry.WriteRecordL(rec.get());
					}
#endif
				}
				break;
			case SSL_TrustedSites:
				{
					DataFile_Record cert_entry(TAG_SSL_TRUSTED_SERVER_NAME);
					cert_entry.SetRecordSpec(prefsfile.GetRecordSpec());
					cert_entry.AddContentL(current_cert->trusted_for_name->Name());
					cert_entry.WriteRecordL(rec.get());

					rec->AddRecordL(TAG_SSL_TRUSTED_SERVER_PORT, current_cert->trusted_for_port);
					rec->AddRecord64L(TAG_SSL_TRUSTED_SERVER_UNTIL, current_cert->trusted_until);
					if(current_cert->accepted_for_kind == CertAccept_Applet)
						rec->AddRecordL(TAG_SSL_TRUSTED_FOR_APPLET);
				}
				break;
			}
			rec->WriteRecordL(&prefsfile);

			rec.reset();

			current_cert = current_cert->Suc();
		}

		prefsfile.Close();
	}
	else
	{
		output.reset();
	};

	if(store == SSL_ClientStore && updated_external_items)
	{
#ifdef WAND_SUPPORT
		g_wand_manager->CommitPreliminaryDataItems();
#endif
		updated_external_items = FALSE;
	}
}
#endif // PREFS_WRITE

#endif // SSL_SUPPORT
