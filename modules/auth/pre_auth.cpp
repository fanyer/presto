/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
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

#include "modules/hardcore/mem/mem_man.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/util/handy.h"
#include "modules/upload/upload.h"

#include "modules/url/url_rep.h"
#include "modules/url/url_sn.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/auth/pre_auth.h"
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/protocols/http_req2.h"
#include "modules/cookies/url_cm.h"

#if defined DEBUG && defined YNP_WORK 
#include "modules/olddebug/tstdump.h"

#define DEBUG_AUTH
#endif

int UU_encode (unsigned char* bufin, unsigned int nbytes, char* bufcoded);
#ifdef _ENABLE_AUTHENTICATE

#ifndef CLEAR_PASSWD_FROM_MEMORY
#error "For security reasons FEATURE_CLEAR_PASSWORDS *MUST* be YES when HTTP Authentication is used"
#endif


#ifdef _USE_PREAUTHENTICATION_
// pth must not start with '/'
CookiePath* CookiePath::GetAuthenticationPath(const char* pth, BOOL create)
{
	if (pth && *pth)
	{
		char* tmp = (char*) op_strchr(pth, '/');
		OP_MEMORY_VAR char save_c = 0;
		if (tmp)
		{
			save_c = *tmp;
			*tmp = '\0';
		}

		OP_MEMORY_VAR int test = 1;
		CookiePath* OP_MEMORY_VAR cp = LastChild();

		while (cp && test>0)
		{
			test = cp->PathPart().Compare(pth);
			if (test > 0)
				cp = cp->Pred();
		}

		CookiePath* OP_MEMORY_VAR  next_cp = cp;
		if (test != 0 || !cp)
		{
			if (!create)
			{
				if (tmp)
					*tmp = save_c;

				return this;
			}

			OP_STATUS op_err = OpStatus::OK;
			next_cp = CookiePath::Create(pth, op_err);
			
			if(OpStatus::IsError(op_err))
			{
				if(tmp)
					*tmp = save_c;
				return NULL;
			}

			if (cp)
				next_cp->Follow(cp);
			else if (FirstChild())
				next_cp->Precede(FirstChild());
			else
				next_cp->Under(this);
		}

		if (tmp)
			*tmp = save_c;

		const char* next_path = (tmp) ? tmp+1 : 0;
		CookiePath* return_cp = next_cp->GetAuthenticationPath(next_path, create);
		if (!return_cp)
			{
				delete (char *) next_path; /* FIXME: THIS LOOKS ODD ! haavardm */
				return NULL;
			}

		return return_cp;
	}
	else
		return this;
}

AuthenticationCookie::AuthenticationCookie(unsigned short pport, AuthScheme pscheme, URLType typ, URL_CONTEXT_ID id)
{
	port = pport;
	scheme = pscheme;
	urltype = typ;
	context_id = id;
}

OP_STATUS AuthenticationCookie::Construct(OpStringC8 rlm)
{
	return realm.Set(rlm);
}

AuthenticationCookie::~AuthenticationCookie()
{
}

AuthenticationCookie *CookiePath::GetAuthenticationCookie(unsigned short port,AuthScheme scheme, URLType typ, URL_CONTEXT_ID id)
{
	AuthenticationCookie* ck = (AuthenticationCookie*) cookie_list.First();
	while (ck)
	{
		AuthenticationCookie* next_ck = ck->Suc();
		
		if( (scheme == AUTH_SCHEME_HTTP_UNKNOWN || 
			(ck->Scheme() & ~AUTH_SCHEME_HTTP_PROXY) == scheme
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
				|| (((scheme & AUTH_SCHEME_HTTP_MASK) == AUTH_SCHEME_HTTP_NTLM_NEGOTIATE) &&
					(((ck->Scheme() & AUTH_SCHEME_HTTP_MASK) == AUTH_SCHEME_HTTP_NEGOTIATE) ||
					((ck->Scheme() & AUTH_SCHEME_HTTP_MASK) == AUTH_SCHEME_HTTP_NTLM)) &&
					(ck->Scheme() & AUTH_SCHEME_HTTP_PROXY) == (scheme &AUTH_SCHEME_HTTP_PROXY)
				)
#endif
			) &&
			ck->Port() == port && ck->Type() == typ
			&& ck->ContextID() == id
			)
			break;
		ck = next_ck;
	}

	return ck;
}


AuthenticationCookie*
CookiePath::GetAuthenticationCookie(unsigned short pport, OpStringC8 rlm,
									AuthScheme pscheme, URLType typ,
									BOOL create
									, URL_CONTEXT_ID id
									)
{
	OpStackAutoPtr<AuthenticationCookie>
		ck(GetAuthenticationCookie(pport,pscheme,typ
		, id
		));
	OP_STATUS op_err;

	if (!create)
		return ck.release();

	if (ck.get())
	{
		ck->Out();
		ck.reset();
	}
	TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, ck.reset(OP_NEW_L(AuthenticationCookie, (pport, pscheme, typ, id))), NULL);

	if(ck.get())
	{
		op_err = ck->Construct(rlm);
		if(OpStatus::IsSuccess(op_err))
			ck->Into(&cookie_list);
		else
			ck.reset();
	}
	return ck.release();
}
#endif

#endif
