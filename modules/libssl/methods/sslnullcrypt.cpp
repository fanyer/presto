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
#include "modules/libssl/methods/sslnull.h"

void SSL_Null_Cipher::InitEncrypt()
{
}

byte *SSL_Null_Cipher::Encrypt(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen)
{
	len1 = len;
	op_memcpy((void *) target, (const void *) source, (size_t) len);
	return target+len;
}

byte *SSL_Null_Cipher::FinishEncrypt(byte *target, uint32 &len1,uint32 bufferlen)
{
	len1 = 0;
	return target;
}

void SSL_Null_Cipher::InitDecrypt()
{
}

byte *SSL_Null_Cipher::Decrypt(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen) 
{
	len1 = len;
	op_memcpy((void *) target, (const void *) source, (size_t) len);
	return target+len;
} 

byte *SSL_Null_Cipher::FinishDecrypt(byte *target, uint32 &len1,uint32 bufferlen)
{
	len1 = 0;
	return target;
}

void SSL_Null_Cipher::SetCipherDirection(SSL_CipherDirection dir)
{
}

const byte *SSL_Null_Cipher::LoadKey(const byte *source)
{
	return source;
}

const byte *SSL_Null_Cipher::LoadIV(const byte *source)
{
	return source;
}

/* Unref YNP
BOOL SSL_Null_Cipher::Valid(SSL_Alert *msg) const   
{ 
return TRUE;
}
*/

void SSL_Null_Cipher::BytesToKey(SSL_HashAlgorithmType, 
								 const SSL_varvector32 &string,  const SSL_varvector32 &salt, int count)
{
}

/*
SSL_GeneralCipher *SSL_Null_Cipher::Fork() const
{
return new SSL_Null_Cipher;
} */

#endif
