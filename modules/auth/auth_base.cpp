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

#ifdef _ENABLE_AUTHENTICATE

#include "modules/auth/auth_elm.h"
#include "modules/auth/auth_digest.h"

#if defined DEBUG && defined YNP_WORK 
#include "modules/olddebug/tstdump.h"

#define DEBUG_AUTH
#endif

// AuthElm_Base main class for actual authentication objects

AuthElm_Base::AuthElm_Base(unsigned short a_port, AuthScheme a_scheme, URLType a_type, BOOL authenticate_once)
	 : AuthElm(a_port, a_type, authenticate_once)
	 , scheme(a_scheme) 
	 ,	context_id(0)
{
}

OP_STATUS AuthElm_Base::Construct(OpStringC8 a_realm, OpStringC8 a_user)
{
	RETURN_IF_ERROR(realm.Set(a_realm));
	return user.Set(a_user);
}

AuthElm_Base::~AuthElm_Base()
{
}

#ifdef HTTP_DIGEST_AUTH
OP_STATUS AuthElm_Base::AddAlias(AuthElm *alias)
{
	if(alias)
	{
		AuthAlias_Ref *ref = (AuthAlias_Ref *) aliased_elements.First();
		
		while(ref)
		{
			if(ref->alias == alias)
				break;
			ref = (AuthAlias_Ref *) ref->Suc();
		}

		if(!ref)
		{
			ref = OP_NEW(AuthAlias_Ref,(alias));
			if(ref)
			{
				SetId(alias->GetId());
				ref->Into(&aliased_elements);
				return OpStatus::OK;
			}
			else
				return OpStatus::ERR_NO_MEMORY;
		}
	}
	return OpStatus::ERR;
}

void AuthElm_Base::RemoveAlias(AuthElm *alias)
{
	AuthAlias_Ref *ref = (AuthAlias_Ref *) aliased_elements.First();

	while(ref)
	{
		if(ref->alias == alias)
			break;
		ref = (AuthAlias_Ref *) ref->Suc();
	}

	if(ref)
	{
		ref->alias = NULL;
		ref->Out();
		OP_DELETE(ref);
	}
}

OP_STATUS AuthElm::Update_Authenticate(ParameterList *header)
{
	return OpStatus::OK;
}
#endif

#endif
