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

#include "modules/libssl/methods/sslcipher.h"

#ifdef _DEBUG
//#define TST_DUMP
#endif


uint32 SSL_Cipher::Calc_BufferSize(uint32 size)
{
	uint32 block,result;
	
	result = size;
	if(cipher_type == SSL_BlockCipher)
	{ 
		block = in_block_size;

		if(block == 0)
			block = (out_block_size >11 ? out_block_size -11 : 0);

		if(block != 0)
			result = ((size + block -1) / block) * out_block_size; 
	}
	
	return result;
}

#ifdef USE_SSL_PGP_CFB_MODE
void SSL_Cipher::CFB_Resync()
{
}

void SSL_Cipher::SetPGP_CFB_Mode(PGP_CFB_Mode_Type mode)
{
}

void SSL_Cipher::UnloadPGP_Prefix(SSL_varvector32 &buf)
{
	buf.Resize(0);
}

#endif

void SSL_Cipher::EncryptVector(const SSL_varvector32 &source, SSL_varvector32 &target)
{
	uint32 len,size;
	byte *target1;
	
	len = source.GetLength();
	size = Calc_BufferSize(len+(GetPaddingStrategy() != SSL_NO_PADDING && CipherType() == SSL_BlockCipher ? 1 : 0));
	target.Resize(size);
	if(size==0)
	{
		RaiseAlert(SSL_Fatal, SSL_InternalError);
		target.RaiseAlert(this);
	}
	
	if(target.ErrorRaisedFlag)
	{
		target.Resize(0);
		return;
	}
	
	target1 = target.GetDirect(); 
	
	InitEncrypt();
	target1 = Encrypt(source.GetDirect(),len,target1,size, target.GetLength());
	FinishEncrypt(target1,size, target.GetLength()- size);  
} 


void SSL_Cipher::DecryptVector(const SSL_varvector32 &source, SSL_varvector32 &target)
	{
	uint32 len,len1,len2;
	byte *target1;
	
	len = source.GetLength();
	
	target.Resize(len+2*InputBlockSize());
	if(InputBlockSize() == 0)
	{
		RaiseAlert(SSL_Fatal, SSL_InternalError);
		target.RaiseAlert(this);
	}
	
	if(target.ErrorRaisedFlag)
	{
		target.Resize(0);
		return;
	}
	
	target1 = target.GetDirect(); 
	
	InitDecrypt();
	target1 = Decrypt(source.GetDirect(),len,target1,len1, target.GetLength());
	FinishDecrypt(target1,len2, target.GetLength()- len1);
	
	len1+= len2;
	target.Resize(len1);
}

#ifdef USE_SSL_DECRYPT_STREAM
uint32 SSL_Cipher::DecryptStreamL(DataStream *source, DataStream *target, uint32 len)
{
	if(source == NULL || target == NULL)
		return 0;

	SSL_secure_varvector32 temp_in, temp_out;
	ANCHOR(SSL_secure_varvector32, temp_in);
	ANCHOR(SSL_secure_varvector32, temp_out);

	uint32 maxlen = (len ? len : 4096);
	uint32 read_len, dec_len;
	uint32 consumed_len = 0;
	uint32 produced_len = 0;

	temp_in.Resize(maxlen);
	if(temp_in.Error())
		LEAVE(temp_in.GetOPStatus());
	temp_out.Resize(maxlen);
	if(temp_out.Error())
		LEAVE(temp_out.GetOPStatus());

	while(source->MoreData() && (!len || consumed_len < len))
	{
		read_len = source->ReadDataL(temp_in.GetDirect(), temp_in.GetLength());

		if(!read_len)
			break;

		consumed_len += read_len;

		Decrypt(temp_in.GetDirect(), read_len, temp_out.GetDirect(), dec_len, temp_out.GetLength());
		target->WriteDataL(temp_out.GetDirect(), dec_len);

		produced_len += dec_len;

		if(read_len < maxlen)
			break;
	}

	if(!source->MoreData())
	{
		FinishDecrypt(temp_out.GetDirect(), dec_len, temp_out.GetLength());
		target->WriteDataL(temp_out.GetDirect(), dec_len);

		produced_len += dec_len;
	}

	return produced_len;
}
#endif

#ifdef USE_SSL_ENCRYPT_STREAM
uint32 SSL_Cipher::EncryptStreamL(DataStream *source, DataStream *target, uint32 len)
{
	if(target == NULL)
		return 0;

	SSL_secure_varvector32 temp_in, temp_out;
	ANCHOR(SSL_secure_varvector32, temp_in);
	ANCHOR(SSL_secure_varvector32, temp_out);

	uint32 maxlen = (len ? len : 4096);
	uint32 read_len, enc_len;
	uint32 consumed_len = 0;
	uint32 produced_len = 0;

	temp_out.Resize(maxlen);
	if(temp_out.Error())
		LEAVE(temp_out.GetOPStatus());

	if(source == NULL)
	{
		FinishEncrypt(temp_out.GetDirect(), enc_len, temp_out.GetLength());
		target->WriteDataL(temp_out.GetDirect(), enc_len);

		produced_len += enc_len;
		return produced_len;
	}
	
	temp_in.Resize(maxlen);
	if(temp_in.Error())
		LEAVE(temp_in.GetOPStatus());

	while(source->MoreData() && (!len || consumed_len < len))
	{
		read_len = source->ReadDataL(temp_in.GetDirect(), temp_in.GetLength());

		if(!read_len)
			break;

		consumed_len += read_len;

		Encrypt(temp_in.GetDirect(), read_len, temp_out.GetDirect(), enc_len, temp_out.GetLength());
		target->WriteDataL(temp_out.GetDirect(), enc_len);

		produced_len += enc_len;

		if(read_len < maxlen)
			break;
	}

	return produced_len;
}
#endif

void SSL_Cipher::SetPaddingStrategy(SSL_PADDING_strategy app_pad)
{
	appendpad = app_pad;
}

SSL_PADDING_strategy SSL_Cipher::GetPaddingStrategy()
{
	return appendpad;
}
#endif
