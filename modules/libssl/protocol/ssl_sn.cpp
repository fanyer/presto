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

#include "modules/url/url2.h"
#include "modules/url/url_sn.h"
#include "modules/url/url_man.h"
#include "modules/url/protocols/http1.h"

#include "modules/hardcore/mem/mem_man.h"

SSL_Port_Sessions *ServerName::GetFirstSSLSessionHandler()
{
	return (SSL_Port_Sessions *) Session_data.First();
}

SSL_Port_Sessions *ServerName::GetSSLSessionHandler(unsigned short port)
{
	SSL_Port_Sessions *current;
	
	current = (SSL_Port_Sessions *) Session_data.First();
	while(current)
	{
		if(current->Port() == port)
			break;
		
		current = (SSL_Port_Sessions *) current->Suc();
	}
	
	if(!current)
	{
		current = OP_NEW(SSL_Port_Sessions, (this, port));
		if(current)
			current->Into(&Session_data);
	}
	
	return current;
}

void ServerName::SetSSLSessionIIS4(unsigned short port, BOOL is_IIS5)
{
	SSL_Port_Sessions *sess = GetSSLSessionHandler(port);
	
	if(sess)
	{
		sess->SetIsIIS4Server(TRUE);
		sess->SetIsIIS5Server(is_IIS5);
		sess->SetTLSUseSSLv3NoCert(TRUE);
	}
}

void ServerName::FreeUnusedSSLResources(BOOL all)
{
	SSL_Port_Sessions *current;
	
	current = (SSL_Port_Sessions *) Session_data.First();
	while(current)
	{
		current->FreeUnusedResources(all);
		SSL_Port_Sessions *temp = current;
		current = (SSL_Port_Sessions *) current->Suc();
		if(temp->SafeToDelete())
		{
			temp->Out();
			OP_DELETE(temp);
		}
	}
}

void ServerName::InvalidateSessionCache(BOOL url_shutdown)
{
	SSL_Port_Sessions *current;
	
	current = (SSL_Port_Sessions *) Session_data.First();
	while(current)
	{
		current->InvalidateSessionCache(url_shutdown);
		current = (SSL_Port_Sessions *) current->Suc();
	}
}

void ServerName::InvalidateSessionForCertificate(SSL_varvector24 &cert)
{
	SSL_Port_Sessions *current;
	
	current = (SSL_Port_Sessions *) Session_data.First();
	while(current)
	{
		if(current->InvalidateSessionForCertificate(cert))
		{
			HTTP_Server_Manager *manager = http_Manager->FindServer(this, current->Port(), TRUE);
			if(manager)
			{
				manager->ForceOtherConnectionsToClose(NULL);
			}
		}

		current = (SSL_Port_Sessions *) current->Suc();
	}
}

void ServerName::CheckTimeOuts(time_t curtime)
{
	SSL_Port_Sessions *current;
	
	current = (SSL_Port_Sessions *) Session_data.First();
	while(current)
	{
		current->CheckCertificateTime(curtime);
		current = (SSL_Port_Sessions *) current->Suc();
	}
}

void ServerName::ForgetCertificates()
{
	SSL_Port_Sessions *current;
	
	current = (SSL_Port_Sessions *) Session_data.First();
	while(current)
	{
		current->SetCertificate(NULL);
		current->ForgetSessionAcceptedCertificate();
		current->SetFeatureStatus(TLS_Untested);
		current = (SSL_Port_Sessions *) current->Suc();
	}
}
#endif
