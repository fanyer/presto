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
#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/libssl/sslbase.h"

void SSL_Alert_Base::Set(SSL_AlertLevel lev,SSL_AlertDescription des)
{
	SetLevel(lev);
	SetDescription(des);
}

void SSL_Alert_Base::SetLevel(SSL_AlertLevel lev)
{
	level = (::Valid(lev) ? lev : SSL_Internal);
}

#ifdef UNUSED_CODE
void SSL_Alert_Base::Set(OP_STATUS op_err)
{
	Set(SSL_Internal, (op_err == OpStatus::ERR_NO_MEMORY ? SSL_Allocation_Failure: SSL_InternalError));
}
#endif

void SSL_Alert_Base::SetDescription(SSL_AlertDescription des)
{
	description = (::Valid(des) || des == SSL_No_Description ? des : SSL_InternalError);
}

SSL_Alert_Base &SSL_Alert_Base::operator =(const SSL_Alert_Base &other)
{
	level = other.level;
	description = other.description; 
	OpStatus::Ignore(reason.Set(other.reason)); 
	return *this;
}

SSL_Alert::SSL_Alert()
{
	level.Into(this);
	description.Into(this);
}

SSL_Alert::SSL_Alert(SSL_AlertLevel lev,SSL_AlertDescription des)
: SSL_Alert_Base(lev,des)
{
	level.Into(this);
	description.Into(this);
}

#ifdef UNUSED_CODE
SSL_Alert::SSL_Alert(const SSL_Alert &old) 
: SSL_Alert_Base(old), LoadAndWritableList()
{
	AddItem(level);
	AddItem(description);
}
#endif

BOOL SSL_Alert::Valid(SSL_Alert *msg) const
{
	return (SSL_Error_Status::Valid(msg) && ::Valid(level,msg) && 
		::Valid(description,msg));
}
#endif
