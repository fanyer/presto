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

#ifndef _PRE_AUTH_ELEMENT_H_
#define _PRE_AUTH_ELEMENT_H_

#include "modules/auth/auth_elm.h"

#ifdef _USE_PREAUTHENTICATION_ 
class AuthenticationCookie : public Link
{
  private:
    OpString8		realm;
	AuthScheme		scheme;
	unsigned short	port;
	URLType			urltype;
	URL_CONTEXT_ID	context_id;

  public:
  	AuthenticationCookie(unsigned short pport, AuthScheme pscheme, URLType typ
					  , URL_CONTEXT_ID id
		);
	OP_STATUS		Construct(OpStringC8 rlm);

  	virtual ~AuthenticationCookie();
  	const char *	Realm(){return realm.CStr();};
  	AuthScheme 		Scheme(){return scheme;};
	unsigned 		Port(){return port;};
	URLType			Type(){return urltype;};
	URL_CONTEXT_ID	ContextID(){return context_id;}

	AuthenticationCookie *Pred(){return (AuthenticationCookie *) Link::Pred();};
	AuthenticationCookie *Suc(){return (AuthenticationCookie *) Link::Suc();};
};
#endif



#endif /* _AUTH_ELEMENT_H_ */

