/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve N. Pettersen
 */
#ifndef _AUTH_HANDLER_H
#define _AUTH_HANDLER_H

#if defined(SELFTEST) 

class SimpleAuthenticationListener 
	: public BasicWindowListener
{
protected:
	BOOL proxy;
	OpString username;
	OpString password;

	BOOL sent;

public:
	SimpleAuthenticationListener (URL_DocSelfTest_Manager *manager, OpLoadingListener *fallback)
		: BasicWindowListener(manager, fallback), proxy(FALSE), sent(FALSE) {}

	virtual ~SimpleAuthenticationListener(){}

	OP_STATUS SetCredentials(BOOL a_proxy, const OpStringC &user, const OpStringC &pass){proxy = a_proxy; RETURN_IF_ERROR(username.Set(user));RETURN_IF_ERROR(password.Set(pass)); return OpStatus::OK;}

	BOOL CredentialSent(){return sent;}

	BOOL Authenticate(OpAuthenticationCallback* callback)
	{
		if(callback == NULL)
			return FALSE;

		if(username.IsEmpty() && password.IsEmpty())
			return FALSE;

		switch (callback->GetType())
		{
		case OpAuthenticationCallback::PROXY_AUTHENTICATION_NEEDED:
		case OpAuthenticationCallback::PROXY_AUTHENTICATION_WRONG:
			if(!proxy)
				return FALSE;
			break;

		case OpAuthenticationCallback::AUTHENTICATION_NEEDED:
		case OpAuthenticationCallback::AUTHENTICATION_WRONG:
			if(proxy)
				return FALSE;
			break;

		default:
			OP_ASSERT(!"Unsupported authentication type");
			return FALSE;
		}

		if(sent)
			return FALSE;

		callback->Authenticate(username.CStr(), password.CStr());

		sent = TRUE;

		return TRUE;
	}

	virtual void OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback)
	{
		if (Authenticate(callback))
			return;
		else
		{
			callback->CancelAuthentication();
			URL empty;
			ReportFailure(empty,"Authentication failed");
		}
	}
};


#endif // SELFTEST 
#endif // _SSL_WAIT_UPDATE_H
