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

void SSL_Error_Status::ForwardAlert()
{
	ErrorRaisedFlag = TRUE;
	if(!HandleAlert(status) && forwardto.get() != NULL)
		forwardto->RaiseAlert(this);
}

SSL_Error_Status::SSL_Error_Status() 
: forwardto(this, NULL), ErrorRaisedFlag(FALSE)
{
}

/* Unref YNP
SSL_Error_Status::SSL_Error_Status(const SSL_Error_Status& old)
: status(old.status), forwardto(NULL), ErrorRaisedFlag(FALSE)
{
}
*/

BOOL SSL_Error_Status::Error(SSL_Alert_Base *msg) const
{
	if (status.GetLevel() != SSL_NoError)
	{
		if(msg != NULL)
			msg->operator =(status);
		return TRUE;
	}
	
	return FALSE;
} 

OP_STATUS SSL_Error_Status::GetOPStatus()
{
	SSL_Alert msg;

	if(!Error(&msg))
		return OpStatus::OK;

	return (msg.GetDescription() == SSL_Allocation_Failure ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR);
}

void SSL_Error_Status::ResetError()
{
	status.Set(SSL_NoError, SSL_No_Description);
	ErrorRaisedFlag = FALSE;
	TRAPD(op_err, SignalActionL(RESET_ERROR_STATUS));
	OpStatus::Ignore(op_err);
} 

void SSL_Error_Status::TwoWayPointerActionL(
	TwoWayPointer_Target *action_source,
	uint32 action_val)
{
	if(action_val == RESET_ERROR_STATUS)
		ResetError();
}

BOOL SSL_Error_Status::HandleAlert(SSL_Alert_Base &)
{
	return FALSE;
}

void SSL_Error_Status::ForwardTo(SSL_Error_Status *other)
{
	forwardto = other; 
	if(ErrorRaisedFlag) 
		ForwardAlert(); 
}

#if 0
void SSL_Error_Status::ForwardToThis(SSL_Error_Status *other1, SSL_Error_Status *other2)
{
	if(other1)
		other1->ForwardTo(this);
	if(other2)
		other2->ForwardTo(this);
};
#endif

void SSL_Error_Status::ForwardToThis(SSL_Error_Status &other1, SSL_Error_Status &other2)
{
	other1.ForwardTo(this);
	other2.ForwardTo(this);
}

void SSL_Error_Status::RaiseAlert(SSL_AlertLevel lev, SSL_AlertDescription des)
{
	status.Set(lev, des);
	ForwardAlert();
}

void SSL_Error_Status::RaiseAlert(SSL_AlertLevel lev, SSL_AlertDescription des, SSL_Alert *alert)
{
	status.Set(lev, des);
	if(alert)
		alert->Set(lev, des);
	ForwardAlert();
}

void SSL_Error_Status::RaiseAlert(const SSL_Alert &msg)
{
	status = msg;
	ForwardAlert();
}

void SSL_Error_Status::RaiseAlert(const SSL_Error_Status &other)
{
	if(other.ErrorRaisedFlag)
	{
		status = other.status;
		ForwardAlert();
	}
}

void SSL_Error_Status::RaiseAlert(const SSL_Error_Status *other)
{
	if(other && other->ErrorRaisedFlag)
	{
		status = other->status;
		ForwardAlert();
	}
}

void SSL_Error_Status::RaiseAlert(const SSL_Error_Status_Base *other)
{
	RaiseAlert((SSL_Error_Status *) other);
}

void SSL_Error_Status::RaiseAlert(OP_STATUS op_err)
{
	SSL_AlertLevel lev=SSL_Internal;
	SSL_AlertDescription des = SSL_InternalError;

	switch(op_err)
	{
	case OpStatus::ERR_NO_MEMORY:
		des = SSL_Allocation_Failure;
		break;
	case OpRecStatus::RECORD_TOO_SHORT:
		des = SSL_Decode_Error;
		lev = SSL_Fatal;
		break;

	}

	RaiseAlert(lev, des);
}

SSL_Error_Status::~SSL_Error_Status()
{
}

BOOL SSL_Error_Status::Valid(SSL_Alert *msg) const
{
	return !Error(msg);
}

#endif
