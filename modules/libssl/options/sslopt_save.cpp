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
#include "modules/libssl/method_disable.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/formats/url_dfr.h"

void SSL_Options::Save()
{
	if(storage_folder == OPFILE_ABSOLUTE_FOLDER)
		return; // No file
	TRAPD(op_err, SaveL());
	if(OpStatus::IsMemoryError(op_err))
		g_memory_manager->RaiseCondition(op_err);
}

OP_STATUS OpenWriteFileL(OpStackAutoPtr<OpFile> &output, OpFileFolder folder, const uni_char *filename)
{
	OP_STATUS res = OpStatus::ERR_FILE_NOT_FOUND;
	if(filename)
	{
		output.reset(OP_NEW_L(OpFile, ()));
		LEAVE_IF_FATAL(res = output->Construct(filename, folder));
		RETURN_IF_ERROR(res);
		LEAVE_IF_FATAL(res = output->Open(OPFILE_WRITE));
	}
	else
		output.reset(NULL);
	return res;
}

void SSL_Options::SaveL()
{
#ifdef PREFS_WRITE
	if(storage_folder == OPFILE_ABSOLUTE_FOLDER)
		return; // No file

	OpStackAutoPtr<OpFile> output(NULL);

	if(loaded_primary)
	{
		if( OpStatus::IsSuccess(OpenWriteFileL(output, storage_folder, UNI_L("opssl6.dat"))) )
		{
			DataFile prefsfile(output.release(), SSL_Options_Version, 1, 4);
			ANCHOR(DataFile, prefsfile);

			prefsfile.InitL();

			DataFile_Record prefsentry(TAG_SSL_MAIN_PREFERENCES);
			ANCHOR(DataFile_Record, prefsentry);

			prefsentry.SetRecordSpec(prefsfile.GetRecordSpec());

			prefsentry.AddRecordL(TAG_SSL_PREFS_PASSWORD_AGING,PasswordAging);

			{
				DataFile_Record ciphers1(TAG_SSL_PREFS_v3_CIPHERS);
				ciphers1.SetRecordSpec(prefsentry.GetRecordSpec());
				current_security_profile->original_ciphers.WriteRecordL(&ciphers1);
				ciphers1.WriteRecordL(&prefsentry);
			}
			
			prefsentry.WriteRecordL(&prefsfile);

			uint32 i=0, n=root_repository_list.Count();
			root_repository_list.PerformActionL(DataStream::KResetRead);

			for(i=0; i<n; i++)
			{
				do{
					DataFile_Record rep_item(TAG_SSL_REPOSITORY_ITEM);
					rep_item.SetRecordSpec(prefsentry.GetRecordSpec());
					rep_item.AddContentL(&root_repository_list[i]);
					rep_item.WriteRecordL(&prefsfile);
				}while(0);
			}

			n=untrusted_repository_list.Count();
			untrusted_repository_list.PerformActionL(DataStream::KResetRead);

			for(i=0; i<n; i++)
			{
				do{
					DataFile_Record rep_item(TAG_SSL_UNTRUSTED_REPOSITORY_ITEM);
					rep_item.SetRecordSpec(prefsentry.GetRecordSpec());
					rep_item.AddContentL(&untrusted_repository_list[i]);
					rep_item.WriteRecordL(&prefsfile);
				}while(0);
			}

			SSL_CRL_Override_Item *crl_overideitem = (SSL_CRL_Override_Item *) crl_override_list.First();
			while(crl_overideitem)
			{
				{
					DataFile_Record crl_item(TAG_SSL_CRL_OVERRIDE_ITEM);
					ANCHOR(DataFile_Record, crl_item);
					crl_item.SetRecordSpec(prefsentry.GetRecordSpec());
					DataFile_Record crl_id_item(TAG_SSL_CRL_OVERRIDE_ID);
					ANCHOR(DataFile_Record, crl_id_item);
					crl_id_item.SetRecordSpec(prefsentry.GetRecordSpec());
					crl_overideitem->cert_id.PerformActionL(DataStream::KResetRead);
					crl_id_item.AddContentL(&crl_overideitem->cert_id);
					crl_id_item.WriteRecordL(&crl_item);

					crl_item.AddRecordL(TAG_SSL_CRL_OVERRIDE_URL,crl_overideitem->crl_url.CStr());
					crl_item.WriteRecordL(&prefsfile);

				}
				crl_overideitem = (SSL_CRL_Override_Item *) crl_overideitem->Suc();
			}

			n = ocsp_override_list.Count();

			for(i=0; i<n; i++)
			{
				OpString8 temp;
				ANCHOR(OpString8, temp);
				temp.SetUTF8FromUTF16L(ocsp_override_list[i]);
				if(temp.HasContent())
				{
					DataFile_Record rep_item(TAG_SSL_OCSP_POST_OVERRIDE_ITEM);
					ANCHOR(DataFile_Record, rep_item);
					rep_item.SetRecordSpec(prefsentry.GetRecordSpec());
					rep_item.AddContentL(temp);
					rep_item.WriteRecordL(&prefsfile);
				}
			}

			time_t now = g_timecache->CurrentTime();
			ServerName *server_name = g_url_api->GetFirstServerName();

			while(server_name)
			{
				if (server_name->SupportsStrictTransportSecurity())
				{
					DataFile_Record sts_item(TAG_SSL_SERVER_STRICT_TRANSPORT_SECURITY_SUPPORT_ITEM);
					ANCHOR(DataFile_Record, sts_item);
					sts_item.SetRecordSpec(prefsfile.GetRecordSpec());
					sts_item.AddRecord64L(TAG_SSL_SERVER_STRICT_TRANSPORT_SECURITY_SUPPORT_EXPIRY, server_name->GetStrictTransportSecurityExpiry());
					sts_item.AddRecordL(TAG_SSL_SERVER_STRICT_TRANSPORT_SECURITY_SUPPORT_SERVERNAME, server_name->Name());

					if (server_name->GetStrictTransportSecurityIncludeSubdomains())
						sts_item.AddRecordL(TAG_SSL_SERVER_STRICT_TRANSPORT_SECURITY_SUPPORT_INCLUDE_DOMAINS);


					sts_item.WriteRecordL(&prefsfile);
				}
				server_name = g_url_api->GetNextServerName();
			}

			server_name = g_url_api->GetFirstServerName();
			while(server_name)
			{
				SSL_Port_Sessions *serv_info = server_name->GetFirstSSLSessionHandler();

				while(serv_info)
				{
					TLS_Feature_Test_Status feature_status = serv_info->GetFeatureStatus();

					if(feature_status > TLS_Final_stage_start &&
						feature_status < TLS_Final_stage_end &&
						serv_info->GetValidTo() > now)
					{
						DataFile_Record feature_entry(TAG_SSL_SERVER_OPTIONS);
						ANCHOR(DataFile_Record, feature_entry);

						feature_entry.SetRecordSpec(prefsfile.GetRecordSpec());
						feature_entry.AddRecord64L(TAG_SSL_SERVOPT_UNTIL,serv_info->GetValidTo());
						feature_entry.AddRecordL(TAG_SSL_SERVOPT_NAME,server_name->Name());
						feature_entry.AddRecordL(TAG_SSL_SERVOPT_PORT,serv_info->Port());

						if((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & CRYPTO_METHOD_RENEG_REFUSE) == 0)
						{
							DataFile_Record feature_ver_field(TAG_SSL_SERVOPT_TLSVER);
							ANCHOR(DataFile_Record, feature_ver_field);

							SSL_ProtocolVersion ver;
							BOOL use_extension = FALSE;
							switch(feature_status)
							{
							case TLS_SSL_v3_only:
								ver.Set(3,0);
								break;
							case TLS_1_0_and_Extensions:
								use_extension = TRUE;
							case TLS_1_0_only:
								ver.Set(3,1);
								break;
							//case TLS_1_1_and_Extensions:
							//	use_extension = TRUE;
							//	ver.Set(3,2);
							//	break;
							case TLS_1_2_and_Extensions:
								use_extension = TRUE;
								ver.Set(3,3);
								break;
							default:
								OP_ASSERT(!"Add new cases above");
							}
							feature_ver_field.SetRecordSpec(prefsfile.GetRecordSpec());
						    ver.WriteRecordL(&feature_ver_field);
						    feature_ver_field.WriteRecordL(&feature_entry);
						    if(use_extension)
							    feature_entry.AddRecordL(TAG_SSL_SERVOPT_TLSEXT);
						}

						if(serv_info->UseReducedDHE())
							feature_entry.AddRecordL(TAG_SSL_SERVOPT_DHE_DEPRECATE);

						feature_entry.WriteRecordL(&prefsfile);
					}
					serv_info = (SSL_Port_Sessions *) serv_info->Suc();
				}

				server_name = g_url_api->GetNextServerName();
			}
			prefsfile.Close();
		}
        else
        {
            output.reset();
        };
	}

	uint32 type_sel;
	for(type_sel = 1; type_sel < SSL_LOAD_ALL_STORES; type_sel <<= 1)
		SaveCertificateFileL(type_sel);
#endif
}

#endif // relevant support
