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

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/method_disable.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/url/url2.h"
#include "modules/url/url_sn.h"
#include "modules/url/url_man.h"
#include "modules/url/protocols/http1.h"

#include "modules/hardcore/mem/mem_man.h"

#include "modules/libssl/methods/sslpubkey.h"
#include "modules/libssl/smartcard/smckey.h"
#include "modules/libssl/protocol/op_ssl.h"

SSL_Port_Sessions::SSL_Port_Sessions(ServerName *server, unsigned short _port)
{
	server_name = server;
	port = _port;
	
	known_to_support_version.Set(3,1);
	if((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & CRYPTO_METHOD_RENEG_REFUSE) != 0)
		feature_status = TLS_1_2_and_Extensions;
	else
		feature_status = TLS_Untested;

	previous_successfull_feature_status = feature_status;

	valid_until = 0;
	is_iis_4 = FALSE;
	is_iis_5 = FALSE;
	tls_force_inactive = FALSE;
	tls_use_ssl3_nocert = FALSE;
	use_dhe_reduced = FALSE;
	
	Certificate_last_used = 0;
	last_certificate = NULL;
}

SSL_Port_Sessions::~SSL_Port_Sessions()
{
	OP_DELETE(last_certificate);
	last_certificate = NULL;
	sessioncache.Clear();
}

void SSL_Port_Sessions::FreeUnusedResources(BOOL all)
{
	if(all)
		InvalidateSessionCache();

	SSL_SessionStateRecordList *session,*oldsession;
	
	session = sessioncache.First();
	while(session != NULL)
	{
#if SSL_SESSION_IDLE_EXPIRATION_TIME
		if(session->is_resumable && 
			session->connections == 0 &&
			session->last_used < g_timecache->CurrentTime()-(SSL_SESSION_IDLE_EXPIRATION_TIME*60))
		{
			session->is_resumable = FALSE;
		}
#endif

		oldsession = session;
		session = session->Suc();
		if(!oldsession->is_resumable && oldsession->connections == 0)
		{
			OP_DELETE(oldsession);
		}
	}
}

void SSL_Port_Sessions::BroadCastSessionNegotiatedEvent(BOOL success)
{
	for (OtlList<SSL*>::Iterator itr = connections_using_this_session.Begin(); itr != connections_using_this_session.End(); ++itr)
		(*itr)->SessionNegotiatedContinueHandshake(success);
}

BOOL SSL_Port_Sessions::SafeToDelete()
{
	if(Get_Reference_Count() != 0 ||
		!sessioncache.Empty() || 
		(feature_status > TLS_Final_stage_start &&
		feature_status < TLS_Final_stage_end)) 
		return FALSE; // Don't delete if there are active sessions or the TLS feature status is in final mode

	return TRUE;
}

void SSL_Port_Sessions::AddAcceptedCertificate(SSL_AcceptCert_Item *new_item)
{
	new_item->Into(&Session_Accepted_Certificates);
}

SSL_AcceptCert_Item *SSL_Port_Sessions::LookForAcceptedCertificate(const SSL_varvector24 &certificate, SSL_ConfirmedMode_enum mode)
{
	SSL_AcceptCert_Item *item = (SSL_AcceptCert_Item *) Session_Accepted_Certificates.First();

	while(item)
	{
		if(item->certificate == certificate && 
			(mode == item->confirm_mode || 
				(mode == SSL_CONFIRMED && item->confirm_mode != APPLET_SESSION_CONFIRMED)))
			break;

		item = item->Suc();
	}

	return item;
}

BOOL SSL_Port_Sessions::HasCertificate()
{
	return (Certificate_last_used && last_certificate ? TRUE : FALSE);
}

void SSL_Port_Sessions::CheckCertificateTime(time_t cur_time)
{
	time_t minutes_before_expire = 5; // default value

#ifdef _SSL_USE_SMARTCARD_
	if(Certificate_last_used && last_certificate && last_certificate->IsExternalCert)
	{
		minutes_before_expire = prefsManager->GetIntegerPrefDirect(PrefsManager::SmartcardCertificateRememberTimeout);
		if(minutes_before_expire< 0 || minutes_before_expire > 5)
			minutes_before_expire = 5;
	}
#endif

	if(Certificate_last_used && Certificate_last_used + minutes_before_expire*60 < cur_time)	
	{
		OP_DELETE(last_certificate);
		last_certificate = NULL;
		Certificate_last_used = 0;
	}
}

SSL_CertificateHandler_List *SSL_Port_Sessions::GetCertificate()
{
	if(!Certificate_last_used || !last_certificate)
		return NULL;
	
	SSL_CertificateHandler_List *certs = OP_NEW(SSL_CertificateHandler_List, ());
	
	if(certs)
	{
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
		if(last_certificate->IsExternalCert)
		{
			OpStringC dummy;
			SSL_ASN1Cert_list tempcert;
			OpAutoPtr<SSL_PublicKeyCipher> pkey(NULL);

			tempcert = last_certificate->certificate;
			if(last_certificate->external_key)
				pkey.reset(last_certificate->external_key->Fork());
			if(pkey.get() != NULL)
			{
				certs->external_key_item = OP_NEW(SSL_Certificate_And_Key_List, (dummy,dummy, tempcert, pkey.get()));
				if(certs->external_key_item != NULL)
					pkey.release();

				if(certs->external_key_item == NULL || certs->external_key_item->Error())
				{
					OP_DELETE(certs);
					return NULL;
				}
			}
			else
			{
				OP_DELETE(certs);
				return NULL;
			}
		}
		else
#endif
		{
			SSL_Alert msg;
			SSL_DistinguishedName_list namelist;
			int status = g_securityManager->BuildChain(certs, last_certificate, namelist, msg);
			if(msg.ErrorRaisedFlag || status <0)
			{
				OP_DELETE(certs);
				return NULL;
			}
		}
		Certificate_last_used  = g_timecache->CurrentTime();

	}
	return certs;
}

void SSL_Port_Sessions::SetCertificate(SSL_CertificateHandler_List *cert)
{
	if(last_certificate)
	{
		OP_DELETE(last_certificate);
		last_certificate = NULL;
	}
	Certificate_last_used = 0;
	
	if(!cert)
		return;
	
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	if(cert->external_key_item)
	{
		last_certificate = OP_NEW(SSL_CertificateItem, ());

		if(last_certificate)
		{
			last_certificate->certificate = cert->external_key_item->DERCertificate()[0];
			last_certificate->IsExternalCert = TRUE;
			last_certificate->external_key = cert->external_key_item->GetKey();
			if(last_certificate->external_key == NULL)
				last_certificate->RaiseAlert(OpStatus::ERR_NO_MEMORY);
		}

		Certificate_last_used  = g_timecache->CurrentTime();
	}
	else 
#endif
	if(cert->certitem)
	{
		last_certificate = OP_NEW(SSL_CertificateItem, (*cert->certitem));
		Certificate_last_used  = g_timecache->CurrentTime();
	}
}

SSL_SessionStateRecordList *SSL_Port_Sessions::FindSessionRecord()
{
	SSL_SessionStateRecordList *present;
	
	present = sessioncache.First();
	
	while(present != NULL)
	{
		if(present->is_resumable)
			break;

		SSL_SessionStateRecordList *temp = present;
		present = present->Suc();
		RemoveSessionRecord(temp);
	}
	
	return present;
}

void SSL_Port_Sessions::RemoveSessionRecord(SSL_SessionStateRecordList *target)
{
	if(target && target->connections == 0)
	{
		OP_DELETE(target);
	}
}

void SSL_Port_Sessions::AddSessionRecord(SSL_SessionStateRecordList *target)
{
	SSL_SessionStateRecordList *temp;
	
	if(target == NULL || target->InList())
		return;
	
	temp = FindSessionRecord();
	
	if(temp != NULL)
	{
		temp->is_resumable = FALSE;
		RemoveSessionRecord(temp);
	}
	target->Into(&sessioncache);  
}

void SSL_Port_Sessions::InvalidateSessionCache(BOOL url_shutdown)
{
	SSL_SessionStateRecordList *session,*oldsession;
	
	valid_until = 0;
	if((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & CRYPTO_METHOD_RENEG_REFUSE) != 0)
		feature_status = TLS_1_2_and_Extensions;
	else
		feature_status = TLS_Untested;

	previous_successfull_feature_status = feature_status;

	tls_force_inactive = FALSE;
	session = sessioncache.First();
	while(session != NULL)
	{
		session->is_resumable = FALSE;
		oldsession = session;
		session = session->Suc();
		if(oldsession->connections == 0)
		{
			//oldsession->connections--;
			//oldsession->Out();
			OP_DELETE(oldsession);
		}
		else if (url_shutdown)
			oldsession->DestroySessionInformation();
	}
}

BOOL  SSL_Port_Sessions::InvalidateSessionForCertificate(SSL_varvector24 &cert)
{
	SSL_SessionStateRecordList *session,*oldsession;
	BOOL ret = FALSE;
	
	session = sessioncache.First();
	while(session != NULL)
	{
		if(session->Client_Certificate.Count() && session->Client_Certificate[0] == cert)
		{
			ret = TRUE;
			session->is_resumable = FALSE;
			oldsession = session;
			session = session->Suc();
			if(oldsession->connections == 0)
			{
				//oldsession->connections--;
				//oldsession->Out();
				OP_DELETE(oldsession);
			}
		}
		else
			session = session->Suc();
	}

	return ret;
}
#endif
