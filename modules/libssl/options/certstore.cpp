/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/methods/sslpubkey.h"

#include "modules/libssl/debug/tstdump2.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/libssl/options/passwd_internal.h"


#ifndef USE_SSL_CERTINSTALLER
// Present implementation takes the from the listelement,
// Later Add filefunctionality
SSL_PublicKeyCipher *SSL_Options::FindPrivateKey(SSL_CertificateHandler_List *cert, SSL_Alert &msg)
{
	SSL_PublicKeyCipher *key;
	SSL_Alert	 		new_msg;

	msg = new_msg;
	
	if(cert == NULL || cert->certitem == NULL)
		return NULL;

	// Retrieves an updated entry if password has been changed
	{
		SSL_CertificateItem *certitem = cert->certitem;
		SSL_CertificateItem *current_cert = System_ClientCerts.First();
		while(current_cert)
		{
			if(current_cert->cert_title.Compare(certitem->cert_title) == 0 &&
				current_cert->certificate == certitem->certificate &&
				current_cert->public_key_hash == certitem->public_key_hash)
				break;
			current_cert = current_cert->Suc();
		}

		if(current_cert == NULL)
		{
			msg.Set(SSL_Internal, SSL_InternalError);
			return NULL;
		}

		if(current_cert->private_key != certitem->private_key || 
			current_cert->private_key_salt != certitem->private_key_salt)
		{
			OP_DELETE(cert->certitem);
			certitem = OP_NEW(SSL_CertificateItem, (*current_cert));
			if(certitem == NULL || certitem->ErrorRaisedFlag)
			{
				if(certitem)
					certitem->Error(&msg);
				else
					msg.Set(SSL_Internal, SSL_Allocation_Failure);
				OP_DELETE(certitem);
				return NULL;
			}
			
			 cert->certitem = certitem;
		}

	}

	SSL_CertificateHandler *certhandler; //;= cert->certitem->handler;

	certhandler = cert->certitem->GetCertificateHandler(&msg);
	if(!certhandler)
		return NULL;

	key = certhandler->GetCipher(0);
	if(key == NULL)
	{
		msg.Set(SSL_Internal, SSL_Allocation_Failure);
		return NULL;
	}
	
	DecryptPrivateKey(cert->certitem->private_key, cert->certitem->private_key_salt,key);
	
	if(key->ErrorRaisedFlag)
	{
		key->Error(&msg);
		OP_DELETE(key);
		key = NULL;
	}
	
	return key;
}
#endif

#ifdef USE_SSL_CERTINSTALLER
SSL_PublicKeyCipher *SSL_Options::FindPrivateKey(OP_STATUS &op_err, SSL_CertificateHandler_List *cert, SSL_secure_varvector32 &decrypted_private_key, SSL_Alert &msg, SSL_dialog_config &config)
{
	SSL_Alert	 		new_msg;

	op_err = OpStatus::OK;
	msg = new_msg;
	
	if(cert == NULL || cert->certitem == NULL)
		return NULL;

	// Retrieves an updated entry if password has been changed
	{
		SSL_CertificateItem *certitem = cert->certitem;
		SSL_CertificateItem *current_cert = System_ClientCerts.First();
		while(current_cert)
		{
			if(current_cert->cert_title.Compare(certitem->cert_title) == 0 &&
				current_cert->certificate == certitem->certificate &&
				current_cert->public_key_hash == certitem->public_key_hash)
				break;
			current_cert = current_cert->Suc();
		}

		if(current_cert == NULL)
		{
			msg.Set(SSL_Internal, SSL_InternalError);
			return NULL;
		}

		if(current_cert->private_key != certitem->private_key || 
			current_cert->private_key_salt != certitem->private_key_salt)
		{
			OP_DELETE(cert->certitem);
			certitem = OP_NEW(SSL_CertificateItem, (*current_cert));
			if(certitem == NULL || certitem->ErrorRaisedFlag)
			{
				if(certitem)
					certitem->Error(&msg);
				else
					msg.Set(SSL_Internal, SSL_Allocation_Failure);
				OP_DELETE(certitem);
				return NULL;
			}
			
			 cert->certitem = certitem;
		}

	}

	SSL_CertificateHandler *certhandler; //;= cert->certitem->handler;

	certhandler = cert->certitem->GetCertificateHandler(&msg);
	if(!certhandler)
		return NULL;

	OpAutoPtr<SSL_PublicKeyCipher> key(certhandler->GetCipher(0));

	if(key.get() == NULL)
	{
		msg.Set(SSL_Internal, SSL_Allocation_Failure);
		return NULL;
	}

	// if private_key_salt == 0, the certificate is not encrypted.
	if (cert->certitem->private_key_salt.GetLength() == 0)
	{
		decrypted_private_key.Set(cert->certitem->private_key);
		key->LoadAllKeys(cert->certitem->private_key);

		if (EncryptClientCertificates())
		{
			// If the certificate does not have salt, but certificate encryption is enabled
			// it means that the certificate is not encrypted, but the user wish to. We encrypt the certificate.

			op_err = GetPassword(config);

			if (OpStatus::IsSuccess(op_err))
			{
				EncryptWithPassword(decrypted_private_key, cert->certitem->private_key, cert->certitem->private_key_salt,
				                    SystemCompletePassword, GetPrivateKeyverificationString());
			}
		}
	}
	else
	{
		// If the certificate had a salt, it means that the private key is
		// encrypted, and must be decrypted.

		op_err = DecryptPrivateKey(cert->certitem->private_key, cert->certitem->private_key_salt, decrypted_private_key, key.get(), config);
	}

	if(key->ErrorRaisedFlag)
	{
		key->Error(&msg);
		return NULL;
	}
	else if(decrypted_private_key.ErrorRaisedFlag)
	{
		decrypted_private_key.Error(&msg);
		return NULL;
	}

	return key.release();
}
#endif


SSL_CertificateItem *SSL_Options::Find_Certificate(SSL_CertificateStore store,int n)
{      
	SSL_CertificateItem *item;
	int i;
    
	Init(store);  
	SSL_CertificateHead *cert_store = GetCertificateStore(store);
	if(!cert_store)
		return NULL;

	item = cert_store->First();
	i = 0;
	while(item != NULL)
	{
		if(item->cert_status != Cert_Deleted)
		{
			if(i == n)
				break;
			i++;
		}
		item = item->Suc();
	}
	return item;    
}

OP_STATUS ParseCommonName(const OpString_list &info, OpString &title)
{
    const OpString *temptitle;
	
	title.Empty();
    if(info.Count() >4)
    {
		temptitle = &info[0];
		if(temptitle->Length() == 0)
		{
			temptitle = &info[1];
			if(temptitle->Length() == 0 && info[2].Length() > 0)
			{
				temptitle = &info[2];
				if(info[3].Length() > 0)
				{
					RETURN_IF_ERROR(title.Append(*temptitle));  
					RETURN_IF_ERROR(title.Append(UNI_L(", "))); 
					temptitle = &info[3];
				}
			}
		}
		RETURN_IF_ERROR(title.Append(*temptitle));  
    }
	return OpStatus::OK;
}

BOOL SSL_Options::EncryptClientCertificates() const
{
	// We only encrypt the certificate if the master password is set.
	return SystemPartPassword.GetLength() > 0 ? TRUE : FALSE;
}

BOOL SSL_Options::Get_Certificate_title(SSL_CertificateStore store, int n, OpString &title)
{
	OpString_list info;
	SSL_CertificateItem *item;
	
	item = Find_Certificate(store, n);
	if(item == NULL)
		return FALSE;
    
	//  temptitle = &item->cert_title;
	if(OpStatus::IsError(title.Append(item->cert_title)))
		return FALSE;
	if(item->cert_title.Length () == 0)
	{
		SSL_CertificateHandler *cert = item->GetCertificateHandler();
		if(cert)
			return OpStatus::IsSuccess(cert->GetCommonName(0, title));
	}
	return TRUE;
}

BOOL SSL_Options::Get_Certificate(SSL_CertificateStore store, int n, 
								  URL &info, 
								  BOOL &warn_if_used, BOOL &deny_if_used,
								  Str::LocaleString description, SSL_Certinfo_mode info_type)
{
	SSL_CertificateHandler *cert;
	SSL_CertificateItem *item;
	
	Init(store);  

	item = Find_Certificate(store, n);
	if(item == NULL)
		return FALSE;

	cert = item->GetCertificateHandler();

	if(cert == NULL)
		return FALSE;

	if(store == SSL_ClientStore)
	{
		warn_if_used = deny_if_used = FALSE;
	}
	else
	{
		warn_if_used = item->WarnIfUsed;
		deny_if_used = item->DenyIfUsed;
	}
	
	return OpStatus::IsSuccess(cert->GetCertificateInfo(0,info, description, info_type));
}

BOOL SSL_Options::Get_CertificateIssuerName(SSL_CertificateStore store, int n, OpString_list &target)
{
	SSL_CertificateItem *item;
	SSL_CertificateHandler *cert;
	
	target.Resize(0);
	item = Find_Certificate(store, n);
	if(item == NULL) 
		return FALSE;
	cert = item->GetCertificateHandler();
	if(cert == NULL)
		return FALSE;
	
	cert->GetIssuerName(0,target);
	return TRUE;
}

void SSL_Options::Set_Certificate(SSL_CertificateStore store, int n, BOOL warn_if_used, BOOL deny_if_used)
{
	SSL_CertificateItem *item;
	
	if(store == SSL_CA_Store)
	{
		item = Find_Certificate(store, n);
		if(item != NULL)
		{
			item->WarnIfUsed = warn_if_used;
			item->DenyIfUsed = deny_if_used;
			if(register_updates)
				item->cert_status = Cert_Updated;
		}
	}
}

void SSL_Options::Remove_Certificate(SSL_CertificateStore store, int n)
{
	SSL_CertificateItem *item;
	
	item = Find_Certificate(store, n);
	if(item != NULL)
	{
		if(register_updates)
			item->cert_status = Cert_Deleted;
		else
			OP_DELETE(item);
	}
}

SSL_CertificateItem *SSL_Options::FindTrustedServer(const SSL_ASN1Cert &cert, ServerName *server, unsigned short port, CertAcceptMode mode)
{
	SSL_CertificateItem *item;
	item = System_TrustedSites.First();

	while (item != NULL)
	{
		if( item->cert_status != Cert_Deleted &&
			item->trusted_for_name == server && 
			item->trusted_for_port == port && 
			item->accepted_for_kind == mode &&
			(cert.GetLength()== 0 || item->certificate == cert))
			break;
		item = item->Suc();
	}
	
	return item;
}

SSL_CertificateItem *SSL_Options::FindUnTrustedCert(const SSL_ASN1Cert &cert)
{
	SSL_CertificateItem *item;
	item = System_UnTrustedSites.First();

	while (item != NULL)
	{
		if(item->certificate == cert)
			break;
		item = item->Suc();
	}
	
	return item;
}

SSL_CertificateItem *SSL_Options::AddTrustedServer(SSL_ASN1Cert &cert, ServerName *server, unsigned short port, BOOL expired, CertAcceptMode mode)
{
	if(!server || cert.GetLength() == 0)
		return NULL;

	if(port == 0 && mode != CertAccept_Applet)
		port = 443;

	SSL_ASN1Cert empty;
	SSL_CertificateItem *item;

	do
	{
		item = FindTrustedServer(empty, server, port, mode);
		if(item)
		{
			if(register_updates)
				item->cert_status = Cert_Deleted;
			else
				OP_DELETE(item);
		}
	}while(item);


	OpStackAutoPtr<SSL_CertificateItem> new_item(OP_NEW(SSL_CertificateItem, ()));

	OpStackAutoPtr<SSL_CertificateHandler> cert_handler(g_ssl_api->CreateCertificateHandler());
	if(!new_item.get() || !cert_handler.get())
		return NULL;

	cert_handler->LoadCertificate(cert);
	if(cert_handler->Error())
		return NULL;

	OpString cert_title;
	RETURN_VALUE_IF_ERROR(cert_handler->GetCommonName(0, cert_title), NULL);

	RETURN_VALUE_IF_ERROR(new_item->cert_title.AppendFormat(UNI_L("%s:%u %s"),server->UniName(), port, cert_title.CStr()), NULL) ;
	cert_handler->GetSubjectName(0,new_item->name);
	new_item->certificate = cert;

	if(new_item->Error())
		return NULL;
	new_item->trusted_for_name = server;
	new_item->trusted_for_port = port;
	new_item->trusted_until = (expired ?  g_timecache->CurrentTime()+90*24*3600 : 0);
	new_item->certificatetype = cert_handler->CertificateType(0);
	new_item->cert_status = Cert_Inserted;
	new_item->accepted_for_kind= mode;

	new_item->Into(&System_TrustedSites);
	return new_item.release();
}

SSL_CertificateItem *SSL_Options::AddUnTrustedCert(SSL_ASN1Cert &cert)
{
	if(cert.GetLength() == 0)
		return NULL;

	SSL_CertificateItem *item = FindUnTrustedCert(cert);
	if(item)
		return item;

	OpStackAutoPtr<SSL_CertificateItem> new_item(OP_NEW(SSL_CertificateItem, ()));

	OpStackAutoPtr<SSL_CertificateHandler> cert_handler(g_ssl_api->CreateCertificateHandler());
	if(!new_item.get() || !cert_handler.get())
		return NULL;

	cert_handler->LoadCertificate(cert);
	if(cert_handler->Error())
		return NULL;

	OpString cert_title;
	RETURN_VALUE_IF_ERROR(cert_handler->GetCommonName(0, new_item->cert_title), NULL);
	cert_handler->GetSubjectName(0,new_item->name);
	new_item->certificate = cert;
	new_item->certificatetype = cert_handler->CertificateType(0);

	if(new_item->Error())
		return NULL;

	new_item->cert_status = Cert_Inserted;

	new_item->Into(&System_UnTrustedSites);
	return new_item.release();
}

SSL_CertificateItem *SSL_Options::FindTrustedCA(const SSL_DistinguishedName &name, SSL_CertificateItem *after)
{
	SSL_CertificateItem *item = NULL;
	if(!after || System_TrustedCAs.HasLink(after))
		item = Find_Certificate(SSL_CA_Store, name, after);
	if(!item)
		item = Find_Certificate(SSL_IntermediateCAStore, name, after);
	
	return item;
}


SSL_CertificateItem *SSL_Options::FindClientCertByKey(const SSL_varvector16 &keyhash)
{
	SSL_CertificateItem *item;
	
	item = System_ClientCerts.First();
	while (item != NULL)
	{
		if(item->public_key_hash == keyhash)
			break;
		item = item->Suc();
	}
	
	return item;
}

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
SSL_CertificateItem *SSL_Options::Find_CertificateByID(SSL_CertificateStore store, const SSL_varvector32 &a_cert_id, SSL_CertificateItem *after)
{
	SSL_CertificateItem *item;

	if(a_cert_id.GetLength() == 0)
		return NULL;
	
	SSL_CertificateHead *cert_store = GetCertificateStore(store);
	if(!cert_store)
		return NULL;

	item = cert_store->First();
	if(after)
	{
		while (item != NULL)
		{
			if(item == after)
				break;
			item = item->Suc();
		}

		item = (item ? item->Suc() :  cert_store->First());
	}
	while (item != NULL)
	{
		if(item->cert_id.GetLength() == 0 && item->GetCertificateHandler())
			item->GetCertificateHandler()->GetSubjectID(0, item->cert_id, (store == SSL_UntrustedSites));

		if(item->cert_id == a_cert_id)
			break;
		item = item->Suc();
	}

	return item;
}
#endif

SSL_CertificateItem *SSL_Options::Find_Certificate(SSL_CertificateStore store, const SSL_DistinguishedName &name, SSL_CertificateItem *after)
{
	SSL_CertificateItem *item;
	
	SSL_CertificateHead *cert_store = GetCertificateStore(store);
	if(!cert_store)
		return NULL;

	item = cert_store->First();
	if(after)
	{
		while (item != NULL)
		{
			if(item == after)
				break;
			item = item->Suc();
		}

		item = (item ? item->Suc() :  cert_store->First());
	}
	while (item != NULL)
	{
		if(item->name == name)
			break;
		item = item->Suc();
	}
	
	return item;
}

void SSL_Options::AddCertificate(SSL_CertificateStore store, SSL_CertificateItem *new_item)
{
	if(!new_item)
		return;
	if(new_item->InList())
		new_item->Out();
	
	SSL_CertificateHead *cert_store = GetCertificateStore(store);
	if(!cert_store)
	{
		OP_DELETE(new_item);
		return;
	}

	new_item->Into(cert_store);
}

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
BOOL SSL_Options::GetCanFetchIssuerID(SSL_varvector24 &issuer_id)
{
	if(issuer_id.GetLength() == 0)
		return FALSE;

#ifdef YNP_WORK

	uint32 i,j,n = root_repository_list.Count();

	SSL_varvector24 temp;
	OpString8 issuer_id_string;
	uint32 id_len= issuer_id.GetLength();
	unsigned char *id_bytes = issuer_id.GetDirect();

	char *id_string = issuer_id_string.Reserve(((id_len+1)/2)*5+10);
	RETURN_OOM_IF_NULL(id_string);

	for(i=0;i<id_len; i+=2, id_string+=5)
		op_sprintf(id_string, (i+2 < id_len ? "%.2X%.2X_" : "%.2X%.2X.xml"), id_bytes[i], id_bytes[i+1]);

	PrintfTofile("reposearch.txt", "Searching for ID %s among:\n", issuer_id_string.CStr());

	for(j = 0;j<n;j++)
	{
		temp = root_repository_list[j];
		id_len= temp.GetLength();
		id_bytes = temp.GetDirect();

		id_string = issuer_id_string.DataPtr();
		*id_string = '\0';

		for(i=0;i<id_len; i+=2, id_string+=5)
			op_sprintf(id_string, (i+2 < id_len ? "%.2X%.2X_" : "%.2X%.2X.xml"), id_bytes[i], id_bytes[i+1]);
		PrintfTofile("reposearch.txt", "   %d: %s\n", j, issuer_id_string.CStr());
	}
	PrintfTofile("reposearch.txt", "-----------------\n");
#endif

	if(!root_repository_list.Contain(issuer_id))
		return FALSE;

	SSL_AutoRetrieve_Item *item;
	SSL_AutoRetrieve_Item *next_item = (SSL_AutoRetrieve_Item *) AutoRetrieved_certs.First();
	while(next_item)
	{
		item = next_item;
		next_item = next_item->Suc();
		if(item->last_checked+ (24*60*60) <= g_timecache->CurrentTime())
		{
			item->Out();
			OP_DELETE(item);
			continue;
		}
		if(item->issuer_id == issuer_id)
			return FALSE;
	}

	return TRUE;
}

void SSL_Options::SetHaveCheckedIssuerID(SSL_varvector24 &issuer_id)
{
	if(GetCanFetchIssuerID(issuer_id))
	{
		SSL_AutoRetrieve_Item *item = OP_NEW(SSL_AutoRetrieve_Item, ());

		if(item == NULL)
			return; // OOM is not a real problem

		item->issuer_id = issuer_id;
		if(item->issuer_id.Error())
		{
			OP_DELETE(item);
			return; // OOM is not a real problem
		}

		item->last_checked = g_timecache->CurrentTime();
		item->Into(&AutoRetrieved_certs);
	}
}

BOOL SSL_Options::GetCanFetchUntrustedID(SSL_varvector24 &subject_id, BOOL &in_list)
{
	in_list = FALSE;
	if(subject_id.GetLength() == 0)
		return FALSE;

#ifdef YNP_WORK

	uint32 i,j,n = untrusted_repository_list.Count();

	SSL_varvector24 temp;
	OpString8 subject_id_string;
	uint32 id_len= subject_id.GetLength();
	unsigned char *id_bytes = subject_id.GetDirect();

	char *id_string = subject_id_string.Reserve(((id_len+1)/2)*5+10);
	RETURN_OOM_IF_NULL(id_string);

	for(i=0;i<id_len; i+=2, id_string+=5)
		op_sprintf(id_string, (i+2 < id_len ? "%.2X%.2X_" : "%.2X%.2X.xml"), id_bytes[i], id_bytes[i+1]);

	PrintfTofile("reposearch.txt", "Searching for ID %s among:\n", subject_id_string.CStr());

	for(j = 0;j<n;j++)
	{
		temp = untrusted_repository_list[j];
		id_len= temp.GetLength();
		id_bytes = temp.GetDirect();

		id_string = subject_id_string.DataPtr();
		*id_string = '\0';

		for(i=0;i<id_len; i+=2, id_string+=5)
			op_sprintf(id_string, (i+2 < id_len ? "%.2X%.2X_" : "%.2X%.2X.xml"), id_bytes[i], id_bytes[i+1]);
		PrintfTofile("reposearch.txt", "   %d: %s\n", j, subject_id_string.CStr());
	}
	PrintfTofile("reposearch.txt", "-----------------\n");
#endif

	if(!untrusted_repository_list.Contain(subject_id))
		return FALSE;

	in_list = TRUE;

	SSL_AutoRetrieve_Item *item;
	SSL_AutoRetrieve_Item *next_item = (SSL_AutoRetrieve_Item *) AutoRetrieved_untrustedcerts.First();
	while(next_item)
	{
		item = next_item;
		next_item = next_item->Suc();
		if(item->last_checked+ (24*60*60) <= g_timecache->CurrentTime())
		{
			item->Out();
			delete item;
			continue;
		}
		if(item->issuer_id == subject_id)
			return FALSE;
	}

	return TRUE;
}

void SSL_Options::SetHaveCheckedUntrustedID(SSL_varvector24 &subject_id)
{
	BOOL in_list = FALSE; // ignored
	if(GetCanFetchUntrustedID(subject_id, in_list))
	{
		SSL_AutoRetrieve_Item *item = new SSL_AutoRetrieve_Item;

		if(item == NULL)
			return; // OOM is not a real problem

		item->issuer_id = subject_id;
		if(item->issuer_id.Error())
		{
			delete item;
			return; // OOM is not a real problem
		}

		item->last_checked = g_timecache->CurrentTime();
		item->Into(&AutoRetrieved_untrustedcerts);
	}
}
#endif // LIBSSL_AUTO_UPDATE_ROOTS

BOOL SSL_Options::GetHaveCheckedURL(URL &url)
{
	if(url.IsEmpty())
		return FALSE;

	SSL_URLRetrieve_Item *item;
	SSL_URLRetrieve_Item *next_item = (SSL_URLRetrieve_Item *) AutoRetrieved_urls.First();
	while(next_item)
	{
		item = next_item;
		next_item = next_item->Suc();
		if(item->expire<= g_timecache->CurrentTime())
		{
			item->Out();
			OP_DELETE(item);
			continue;
		}
		if(item->url == url)
			return TRUE;
	}

	return FALSE;
}

void SSL_Options::SetHaveCheckedURL(URL &url, time_t expire_time)
{
	if(!GetHaveCheckedURL(url))
	{
		if (g_opera->libssl_module.m_has_run_intermediate_shutdown)
			return;

		SSL_URLRetrieve_Item *item = OP_NEW(SSL_URLRetrieve_Item, (url, g_timecache->CurrentTime()+expire_time));

		if(item == NULL)
			return; // OOM is not a real problem

		item->Into(&AutoRetrieved_urls);
	}
}

#endif // relevant support
