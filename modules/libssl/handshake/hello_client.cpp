/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/pi/OpSystemInfo.h"

#include "modules/libssl/sslbase.h"
#include "modules/libssl/handshake/sslhand3.h"
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/sslrand.h"
#include "modules/libssl/handshake/hello_client.h"
#include "modules/libssl/handshake/tlssighash.h"
#include "modules/libssl/handshake/tls_ext.h"

#include "modules/util/cleanse.h"

SSL_Client_Hello_st::SSL_Client_Hello_st()
{
	InternalInit();
}
void SSL_Client_Hello_st::InternalInit()
{
	session_id.ForceVectorLengthSize(1, 0xff); // Force it to use 8 bit length. Done to avoid adding another template
	AddItem(client_version);
	AddItem(random);
	AddItem(session_id);
	AddItem(ciphersuites);
	AddItem(compression_methods);
}

SSL_Client_Hello_st::SSL_Client_Hello_st(BOOL dontadd_fields)
{
	// Don't add fields 
}

static const SSL_SignatureAlgorithm supported_signatures_list[]=
{
	SSL_RSA_SHA_256,
	SSL_RSA_SHA,
	SSL_RSA_MD5,
	SSL_DSA_sign
};

// TODO: replace with actual values
#define TLS_RENEGO_PROTECTION_MAJOR_ID 0x00
#define TLS_RENEGO_PROTECTION_MINOR_ID 0xFF

void SSL_Client_Hello_st::SetUpMessageL(SSL_ConnectionState *pending)
{
	SSL_SessionStateRecordList *session;

	if(pending == NULL /*|| security_profile == NULL*/)
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
		return;
	}

	client_version = pending->sent_version;
	
	random.Generate();
	pending->client_random.ResetRecord();
	random.WriteRecordL(&pending->client_random);

	session = pending->session;
	//if(client_version.Major() >= 3)
	{
		TLS_Feature_Test_Status feature_status = pending->server_info->GetFeatureStatus();

		// set ciphers
		uint32 count = pending->sent_ciphers.Count();
		uint32 start=0;
		// SCSV = Signalling Cipher Suite Value
		// Don't send SCSV when server is known to be extension tolerant, as a 
		// renego patched server MUST support extensions, sending SCVS is therefore unnecessary
		BOOL send_scsv = !(feature_status >= TLS_Final_extensions_supported && feature_status <= TLS_Final_stage_end) &&
					!(feature_status >= TLS_Test_with_Extensions_start && feature_status <= TLS_Test_with_Extensions_end);
		ciphersuites.Resize(count +(send_scsv ? 1 /* SCSV */ : 0));
		if(send_scsv)
		{
			// Add SCSV value
			// TODO: Optional when known patched server, or when strict mode. Check if spec requires extension.
			ciphersuites[start++].Set(TLS_RENEGO_PROTECTION_MAJOR_ID,TLS_RENEGO_PROTECTION_MINOR_ID); // special cipher
		}
		uint32 index=0;
		if(session != NULL && pending->sent_ciphers.Contain(session->used_cipher, index) && index >0)
		{
			// move used cipher to front
			ciphersuites[start++] = pending->sent_ciphers[index];
		}
		else
			index = count;
		ciphersuites.Transfer(start, pending->sent_ciphers,0,index);
		start += index;
		if(index +1 < count)
			ciphersuites.Transfer(start, pending->sent_ciphers,index+1, count -index-1);

		compression_methods = pending->sent_compression;

		BOOL use_extensions = FALSE;

		switch(feature_status)
		{
		case TLS_1_0_and_Extensions:
		//case TLS_1_1_and_Extensions:
		case TLS_1_2_and_Extensions:
		case TLS_Test_1_0_Extensions:
		//case TLS_Test_1_1_Extensions:
		case TLS_Test_1_2_Extensions:
			use_extensions = TRUE;
			break;
		}

		if(use_extensions)
		{
			TLS_ExtensionList ext_list;
			ANCHOR(SSL_LoadAndWritable_list, ext_list);
			int i;
			OpStringC8 name(pending->server_info->GetServerName()->Name());
			if(name.HasContent() && name[0] != '[' && name.SpanOf("0123456789.") != name.Length())
			{
				TLS_ServerNameList namelist;
				ANCHOR(SSL_LoadAndWritable_list, namelist);
				
				namelist.Resize(1);
				namelist[0].SetHostName(name);
				
				i = ext_list.Count();
				ext_list.Resize(i+1);
				if(namelist.Valid() && ext_list.Valid() )
				{
					ext_list[i].extension_id = TLS_Ext_ServerName;
					namelist.WriteRecordL(&ext_list[i].extension_data);
					
				}
			}
			{
				TLS_RenegotiationInfo reneg_info;
				ANCHOR(TLS_RenegotiationInfo, reneg_info);

				reneg_info.renegotiated_connection = pending->last_client_finished;

				i = ext_list.Count();
				ext_list.Resize(i+1);
				if(reneg_info.Valid() && ext_list.Valid() )
				{
					ext_list[i].extension_id = TLS_Ext_RenegotiationInfo;
					reneg_info.WriteRecordL(&ext_list[i].extension_data);
				}
			}

#ifndef TLS_NO_CERTSTATUS_EXTENSION
			{
				TLS_CertificateStatusRequest ocsp_item;
				ANCHOR(TLS_CertificateStatusRequest, ocsp_item);

				ocsp_item.ConstructL(pending);

				i = ext_list.Count();
				ext_list.Resize(i+1);
				if(ocsp_item.Valid() && ext_list.Valid() )
				{
					ext_list[i].extension_id = TLS_Ext_StatusRequest;
					ocsp_item.WriteRecordL(&ext_list[i].extension_data);
				}
			}
#endif
#ifdef _SUPPORT_TLS_1_2
			if(client_version.Compare(3,3) >= 0)
			{
				TLS_SignatureAlgList algs;
				algs.Set(supported_signatures_list, ARRAY_SIZE(supported_signatures_list));

				i = ext_list.Count();
				ext_list.Resize(i+1);
				if(algs.Valid() && ext_list.Valid() )
				{
					ext_list[i].extension_id = TLS_Ext_SignatureList;
					algs.WriteRecordL(&ext_list[i].extension_data);
				}
			}
#endif
			// Renegotiations on the same connection MUST NOT include the next_protocol_negotiation extension.
			// pending_connstate->last_client_finished it is always empty for new connections, non-empty for renegotiation
			if (pending->last_client_finished.GetLength() == 0)
			{
				i = ext_list.Count();
				ext_list.Resize(i+1);
				ext_list[i].extension_id = TLS_Ext_NextProtocol;
			}

			if(ext_list.Valid() && ext_list.Count() > 0)
			{
				ext_list.WriteRecordL(&extensions);
				AddItem(extensions);
			}
		}
	}

	if(session)
	{
		session_id = session->sessionID;
		session->last_used = g_timecache->CurrentTime();
	}
}
#endif // relevant support
