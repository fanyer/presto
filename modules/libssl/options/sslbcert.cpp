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

#if defined _NATIVE_SSL_SUPPORT_
#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"

//#include "testmem.h"
// -1 - error or could not produce certificate chain
// 0 - no CA found
// 1 - CA in chain but no root
// 2 - CA in chain with root
// CA is considered in chain if ca_names-list is empty


int SSL_Options::BuildChain(SSL_CertificateHandler_List *certs, SSL_CertificateItem *basecert,
								SSL_DistinguishedName_list &ca_names, SSL_Alert &msg)
{
	BOOL foundca,foundroot;
	SSL_CertificateItem *item;
	SSL_DistinguishedName current_ca;
	SSL_CertificateHandler *handler;
	SSL_Certificate_indirect_list *list_item;
	
	if(!basecert)
		return -1;

	certs->certitem = FindClientCertByKey(basecert->public_key_hash);
	if(certs->certitem == NULL || certs->certitem->ErrorRaisedFlag)
		return -1;

	certs->ca_list.Clear();
	
	handler = certs->certitem->GetCertificateHandler(&msg);
	if(!handler)
		return -1;
	
	foundca = foundroot = FALSE;
	while(!foundroot)
	{
		handler->GetIssuerName(0,current_ca);
		
		item = FindTrustedCA(current_ca);
		if(item == NULL)
			break;
		
#ifdef _SSL_ENABLE_CA_CHECK_
		n = (uint16) ca_names.Count();
		if(n != 0)
		{
			for(i=0;i<n;i++)
				if(ca_names[i] == current_ca)
				{
					foundca = TRUE;
					break;
				}
		}
		else
			foundca = TRUE;
#else
		foundca = TRUE;
#endif    
		handler = item->GetCertificateHandler(&msg);
		if(!handler)
			return -1;
		
		list_item = OP_NEW(SSL_Certificate_indirect_list, ());
		if(list_item == NULL)
		{
			msg.RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
			return -1;
		}
		
		list_item->cert_item = item;
		list_item->Into(&certs->ca_list);
		
		if(handler->SelfSigned(0))
			foundroot = TRUE;
	}
	
	if(ca_names.Count() == 0)
		foundca = TRUE;
	
	return (foundca ? (foundroot ? 2 : 1) : 0);
}

#endif
