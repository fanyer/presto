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
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/methods/sslphash.h"
#include "modules/libssl/methods/sslnull.h"

void Cleanup_HashPointer()
{
	OP_DELETE(g_SSL_Hash_Pointer_NullHash);
	g_SSL_Hash_Pointer_NullHash = NULL;
}

void SSL_Hash_Pointer::Init()
{
	hash = NULL;
    if (g_SSL_Hash_Pointer_NullHash==NULL)
	{
        g_SSL_Hash_Pointer_NullHash = OP_NEW(SSL_Null_Hash, ());
		if(g_SSL_Hash_Pointer_NullHash == NULL)
			RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
	}
    point = g_SSL_Hash_Pointer_NullHash;
	
}

SSL_Hash_Pointer::SSL_Hash_Pointer()
{
	Init();
}

SSL_Hash_Pointer::SSL_Hash_Pointer(SSL_HashAlgorithmType alg)
: SSL_Error_Status()
{
	Init();
	Set(alg);
}

/*
SSL_Hash_Pointer::SSL_Hash_Pointer(const SSL_Hash_Pointer &old)
: SSL_Error_Status()
{
	Init();
	CreatePointer(old.hash, TRUE);
}
*/

/*
SSL_Hash_Pointer::SSL_Hash_Pointer(const SSL_Hash *old)
{
	Init();
	CreatePointer(old , TRUE);
}
*/

SSL_Hash_Pointer::~SSL_Hash_Pointer()
{
	RemovePointer();
}

void SSL_Hash_Pointer::RemovePointer()
{
	OP_DELETE(hash);
	hash = NULL;
	point = g_SSL_Hash_Pointer_NullHash;
}

void SSL_Hash_Pointer::Set(SSL_HashAlgorithmType alg)
{
	RemovePointer();

	OP_STATUS op_err = OpStatus::OK;
	hash = g_ssl_api->CreateMessageDigest(alg, op_err);
	if(OpStatus::IsError(op_err))
	{
		RaiseAlert(op_err);
		return;
	}
	point = hash;
	point->ForwardTo(this);
}


BOOL SSL_Hash_Pointer::CreatePointer(const SSL_Hash *org, BOOL fork, BOOL hmac)
{ 
	RemovePointer();
	if(org)
	{
		if(fork)
			hash = org->Fork();
		else
		{
			Set(org->HashID());
			return !Error(); // Already configured
		}
		
		if(hash == NULL)
		{
			RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
			return FALSE;
		}
		else
		{
			point = hash;
			point->ForwardTo(this);
		}
	}
	return TRUE;
}

// Unref YNP
#if 0
BOOL SSL_Hash_Pointer::SetFork(const SSL_Hash *org)
{ 
	return CreatePointer(org,TRUE);
}
#endif 

#if 0
BOOL SSL_Hash_Pointer::SetProduce(const SSL_Hash *org)
{
	return CreatePointer(org,FALSE);
}

BOOL SSL_Hash_Pointer::SetProduceHMAC(const SSL_Hash *org)
{
	return CreatePointer(org,FALSE, TRUE);
}
#endif

SSL_Hash_Pointer &SSL_Hash_Pointer::operator =(const SSL_Hash_Pointer &old)
{
	CreatePointer(old.hash ,TRUE);
	return *this;
}

SSL_Hash_Pointer &SSL_Hash_Pointer::operator =(const SSL_Hash *org)
{
	CreatePointer(org ,TRUE);
	return *this;
}

BOOL SSL_Hash_Pointer::Valid(SSL_Alert *msg)  const
{
	if(Error(msg))
		return FALSE;
	
	if(hash != NULL)
	{
		return (point == hash && hash->Valid(msg));
	}
	else if(point != g_SSL_Hash_Pointer_NullHash || point == NULL)
	{
		if(msg)
			msg->Set(SSL_Internal, SSL_InternalError);
		
		return FALSE;
	}
	return TRUE;
}
#endif
