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

#ifndef _BASIC_AUTH_ELEMENT_H_
#define _BASIC_AUTH_ELEMENT_H_

#include "modules/auth/auth_elm.h"


class Basic_AuthElm : public AuthElm_Base {
private:
	OpString8		auth_string;
	OpString8		password;

public:
    Basic_AuthElm(unsigned short a_port, AuthScheme a_scheme, URLType a_urltype, BOOL authenticate_once = FALSE);
	OP_STATUS	Construct(OpStringC8 a_realm, OpStringC8 a_user, OpStringC8 a_passwd);
    virtual ~Basic_AuthElm();
    
    virtual OP_STATUS		GetAuthString(OpStringS8 &ret_str, URL_Rep *, HTTP_Request *http=NULL);
	virtual OP_STATUS		GetProxyConnectAuthString(OpStringS8 &ret_str, URL_Rep *, Base_request_st *request, HTTP_Request_digest_data &proxy_digest);

    virtual OP_STATUS		SetPassword(OpStringC8 a_passwd);

	virtual const char*		GetPassword() const;

private:
	OP_STATUS		MakeAuthString(OpStringC8 user, OpStringC8 passwd);
};

#endif /* _AUTH_ELEMENT_H_ */

