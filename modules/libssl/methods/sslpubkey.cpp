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
#include "modules/libssl/methods/sslpubkey.h"

#ifdef _DEBUG
//#define TST_DUMP
#endif

void SSL_PublicKeyCipher::SignVector(const SSL_varvector32 &source,SSL_varvector32 &target)
{
	EncryptVector(source,target);
}

BOOL SSL_PublicKeyCipher::VerifyVector(const SSL_varvector32 &reference, const SSL_varvector32 &signature)
{
	return Verify(reference.GetDirect(),reference.GetLength(),signature.GetDirect(),signature.GetLength());
}

#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
/*
unsigned long SSL_PublicKeyCipher::GetSmartCardNumber()
{
	return 0;
}
*/

void SSL_PublicKeyCipher::Login(SSL_secure_varvector32 &password)
{
}

BOOL SSL_PublicKeyCipher::Need_Login() const
{
	return FALSE;
}

BOOL SSL_PublicKeyCipher::Need_PIN() const
{
	return FALSE;
}

BOOL SSL_PublicKeyCipher::UseAutomatically() const
{
	return use_automatically;
}

void SSL_PublicKeyCipher::Set_UseAutomatically(BOOL flag)
{
	use_automatically=flag;
}
#endif

#endif
