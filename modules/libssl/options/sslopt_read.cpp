/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/formats/url_dfr.h"
#include "modules/libssl/method_disable.h"
#include "modules/libssl/auto_update_version.h"

void DeactivateCrypto();


OP_STATUS OpenFileL(OpStackAutoPtr<OpFile> &input, OpFileFolder folder, const uni_char *filename)
{
	OP_STATUS res = OpStatus::OK;

	input.reset(OP_NEW_L(OpFile, ()));

	LEAVE_IF_FATAL(res = input->Construct(filename, folder));
	RETURN_IF_ERROR(res);
	LEAVE_IF_FATAL(res = input->Open(OPFILE_READ));
	return res;
}

#ifdef UNUSED_CODE
OP_STATUS OpenOneOfFilesL(OpStackAutoPtr<OpFile> &input, int &used_name, OpFileFolder folder, const uni_char *file_1, const uni_char *file_2)
{
	OP_STATUS res;

	used_name = 0;
	res = OpStatus::ERR_FILE_NOT_FOUND;
	if(file_1)
		res = OpenFileL(input,folder, file_1);

	if(OpStatus::IsError(res) && file_2)
	{
		used_name = 1;
		res = OpenFileL(input, folder, file_2);
	}

	return res;
}
#endif // UNUSED_CODE

void LoadListFromIndexedRecordAndCopyCommonL(DataFile_Record *rec, SSL_LoadAndWritable_list &target1, SSL_LoadAndWritable_list &target2, SSL_LoadAndWritable_list &master, uint32 tag)
{
	rec->ReadRecordFromIndexedRecordL(target1, tag);

	target2.CopyCommon(master, target1,TRUE);
}

BOOL SSL_Options::ReadNewConfigFileL( BOOL &sel, BOOL &upgrade)
{
    OpStackAutoPtr<OpFile> input(NULL);
	BOOL status = FALSE;

	if(storage_folder == OPFILE_ABSOLUTE_FOLDER)
		return FALSE; // No file, use defaults

	if( OpStatus::IsError(OpenFileL(input, storage_folder,  UNI_L("opssl6.dat"))) )
		return FALSE;

	DataFile prefsfile(input.release());
	ANCHOR(DataFile, prefsfile);

	prefsfile.InitL();

	keybase_version = prefsfile.AppVersion();
	if(keybase_version == 0)
	{
		prefsfile.Close();
		return FALSE;

	}
	if(keybase_version< SSL_Options_Version_NewBase)
	{
		prefsfile.Close();
		return TRUE;
	}

#if defined LIBSSL_AUTO_UPDATE
	if(keybase_version< SSL_Options_Version)
		g_main_message_handler->PostMessage(MSG_SSL_START_AUTO_UPDATE, 0, 0);
#endif


	OpStackAutoPtr<DataFile_Record> rec(NULL);
	DataFile_Record *rec2;

	some_secure_ciphers_are_disabled = TRUE;

	while((rec2 = prefsfile.GetNextRecordL()) != NULL)
	{
		rec.reset(rec2);
		switch(rec->GetTag())
		{
		case TAG_SSL_MAIN_PREFERENCES:
			{
				rec->SetRecordSpec(prefsfile.GetRecordSpec());
				rec->IndexRecordsL();

				PasswordAging = (uint16) rec->GetIndexedRecordUIntL(TAG_SSL_PREFS_PASSWORD_AGING);

				LoadListFromIndexedRecordAndCopyCommonL(rec.get(), current_security_profile->original_ciphers,
					current_security_profile->ciphers, SystemCipherSuite, TAG_SSL_PREFS_v3_CIPHERS);
				status = TRUE;

				some_secure_ciphers_are_disabled = FALSE;
				uint16 i;
				for(i=0;i <StrongCipherSuite.Count(); i++)
				{
					if(!current_security_profile->ciphers.Contain(StrongCipherSuite[i]))
					{
						some_secure_ciphers_are_disabled = TRUE;
						break;
					}
				}

				if(keybase_version < SSL_Options_Version_9_00c)
				{
					current_security_profile->ciphers.CopyCommon(StrongCipherSuite, current_security_profile->original_ciphers, TRUE);
					current_security_profile->original_ciphers = current_security_profile->ciphers;
				}

				break;
			}
		case TAG_SSL_REPOSITORY_ITEM:
			{
				uint32 i = root_repository_list.Count();
				root_repository_list.Resize(i+1);
				if(!root_repository_list.Error())
					root_repository_list[i].AddContentL(rec.get());
				LEAVE_IF_ERROR(root_repository_list.GetOPStatus());
				break;
			}
		case TAG_SSL_UNTRUSTED_REPOSITORY_ITEM:
			{
				SSL_varvector24 temp;
				temp.AddContentL(rec.get());

				if (!untrusted_repository_list.Contain(temp))
				{
					uint32 i = untrusted_repository_list.Count();
					untrusted_repository_list.Resize(i+1);
					if(!untrusted_repository_list.Error())
						untrusted_repository_list[i].AddContentL(&temp);
					LEAVE_IF_ERROR(untrusted_repository_list.GetOPStatus());
				}
				break;
			}
		case TAG_SSL_CRL_OVERRIDE_ITEM:
			{
				rec->SetRecordSpec(prefsfile.GetRecordSpec());
				rec->IndexRecordsL();
				OpStackAutoPtr<SSL_CRL_Override_Item> item(OP_NEW_L(SSL_CRL_Override_Item, ()));

				DataFile_Record *id_rec = rec->GetIndexedRecord(TAG_SSL_CRL_OVERRIDE_ID);
				if(id_rec)
				{
					item->cert_id.AddContentL(id_rec);
					LEAVE_IF_ERROR(item->cert_id.GetOPStatus());
					rec->GetIndexedRecordStringL(TAG_SSL_CRL_OVERRIDE_URL, item->crl_url);
					if(item->crl_url.HasContent())
						item.release()->Into(&crl_override_list);
				}
				// Cleanup
				break;
			}
		case TAG_SSL_OCSP_POST_OVERRIDE_ITEM:
			{
				OpString8 temp;
				ANCHOR(OpString8, temp);
				rec->GetStringL(temp);
				if(temp.HasContent())
				{
					uint32 i = ocsp_override_list.Count();
					LEAVE_IF_ERROR(ocsp_override_list.Resize(i+1));
					ocsp_override_list[i].SetFromUTF8L(temp.CStr());
				}
				break;
			}
		case TAG_SSL_SERVER_STRICT_TRANSPORT_SECURITY_SUPPORT_ITEM:
			{
				rec->IndexRecordsL();

				OpString8 name;
				ANCHOR(OpString8, name);

				time_t expiry = (time_t) rec->GetIndexedRecordUInt64L(TAG_SSL_SERVER_STRICT_TRANSPORT_SECURITY_SUPPORT_EXPIRY);
				if(expiry && expiry <= g_timecache->CurrentTime())
					break;

				rec->GetIndexedRecordStringL(TAG_SSL_SERVER_STRICT_TRANSPORT_SECURITY_SUPPORT_SERVERNAME, name);
				if(name.IsEmpty())
					break;

				BOOL include_subdomains = rec->GetIndexedRecordBOOL(TAG_SSL_SERVER_STRICT_TRANSPORT_SECURITY_SUPPORT_INCLUDE_DOMAINS);


				ServerName *server_name = g_url_api->GetServerName(name, TRUE);
				if (!server_name)
					break;

				server_name->SetStrictTransportSecurity(TRUE, include_subdomains, expiry);

				OP_ASSERT(server_name->SupportsStrictTransportSecurity());

				break;
			}
		case TAG_SSL_SERVER_OPTIONS:
			{
				OpString8 name;
				ANCHOR(OpString8, name);
				uint16 port;
				time_t date;

				rec->IndexRecordsL();

				date = (time_t) rec->GetIndexedRecordUInt64L(TAG_SSL_SERVOPT_UNTIL);
				if(date <= g_timecache->CurrentTime())
					break;

				rec->GetIndexedRecordStringL(TAG_SSL_SERVOPT_NAME, name);
				if(name.IsEmpty())
					break;

				port = (uint16) rec->GetIndexedRecordUIntL(TAG_SSL_SERVOPT_PORT);
				if(port == 0)
					break;

				ServerName *server_name = g_url_api->GetServerName(name, TRUE);
				if(server_name == NULL)
					break;

				SSL_Port_Sessions *serv_info = server_name->GetSSLSessionHandler(port);
				if(serv_info == NULL)
					break;

				if((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & CRYPTO_METHOD_RENEG_REFUSE) != 0)
					serv_info->SetFeatureStatus(TLS_1_2_and_Extensions);
				else
				{
					DataFile_Record *ver_rec = rec->GetIndexedRecord(TAG_SSL_SERVOPT_TLSVER);
					SSL_ProtocolVersion ver;
					ANCHOR(SSL_ProtocolVersion, ver);

					if(ver.ReadRecordFromStreamL(ver_rec) !=  OpRecStatus::FINISHED)
						break;

					if(ver.Major()<3 || ver.Major() > 3 || ver.Minor()>3)
						break; // not in (3,0) to (3,3) range, inclusive. SSL v2 should always trigger a new test (when active).

					BOOL use_extensions = rec->GetIndexedRecordBOOL(TAG_SSL_SERVOPT_TLSEXT);
					OP_ASSERT(ver.Major() == 3 && ver.Minor() <=3); // Fix this code, it assumes version is 3.0 to 3.3. inclusive
					TLS_Feature_Test_Status feature_status = TLS_Untested;
					switch(ver.Minor())
					{
					case 0:
						serv_info->SetTLSDeactivated(TRUE);
						feature_status = TLS_SSL_v3_only;
						break;
					case 1:
					case 2:
						feature_status = (use_extensions ? TLS_1_0_and_Extensions : TLS_1_0_only);
						break;
					//case 2:
						//feature_status = TLS_1_1_and_Extensions;
						//break;
					case 3:
						feature_status = TLS_1_2_and_Extensions;
						break;

					}
					serv_info->SetFeatureStatus(feature_status);
				}
				if(rec->GetIndexedRecordBOOL(TAG_SSL_SERVOPT_DHE_DEPRECATE))
					serv_info->SetUseReducedDHE();
			}
			break;
		}

		rec.reset();
	}

	uni_char *host = NULL; // PREFSMAN3-FIXME: Retrieve host context
	if((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & CRYPTO_METHOD_DISABLE_SSLV3) != 0)
	{
#ifdef PREFS_WRITE
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::EnableSSL3_0, FALSE);
#endif
		Enable_SSL_V3_0 = FALSE;
	}
	else
		Enable_SSL_V3_0 = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableSSL3_0, host);

	Enable_TLS_V1_0 = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableSSL3_1, host);
	Enable_TLS_V1_1 = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableTLS1_1, host);
#ifdef _SUPPORT_TLS_1_2
	Enable_TLS_V1_2 = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableTLS1_2, host);
#endif

	SecurityEnabled = Enable_SSL_V3_0 || Enable_TLS_V1_0 || Enable_TLS_V1_1
#ifdef _SUPPORT_TLS_1_2
		|| Enable_TLS_V1_2
#endif
		;

	loaded_primary = TRUE;

	prefsfile.Close();

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	if(!g_ssl_tried_auto_updaters && (root_repository_list.Count() == 0 ||
		(root_repository_list[0].GetDirect() && 
		*root_repository_list[0].GetDirect() != AUTOUPDATE_VERSION_NUM)))
	{
		g_ssl_tried_auto_updaters = TRUE;
		g_main_message_handler->PostMessage(MSG_SSL_START_AUTO_UPDATE, 0, 0);
	}
#endif

	return status;
}

#endif // relevant support
