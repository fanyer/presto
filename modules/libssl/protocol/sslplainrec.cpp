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
#include "modules/libssl/protocol/sslplainrec.h"
#include "modules/libssl/protocol/sslcipherrec.h"

#ifdef _DEBUG
//#define TST_DUMP
#endif

SSL_PlainText::SSL_PlainText()
{
	Init();
	SetIsASegment(TRUE);
}

void SSL_PlainText::Init()
{
	SetLegalMax(SSL_MAX_PLAIN_LENGTH);
	spec.enable_tag = TRUE;
	spec.idtag_len = 1;

	version.Follow(&tag);	  // Action 2;
}

SSL_PlainText::~SSL_PlainText()
{
}

void SSL_PlainText::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	SSL_Record_Base::PerformActionL(action, arg1, arg2);

	if(action == DataStream::KReadAction &&
		arg2 == DataStream_BaseRecord::RECORD_LENGTH &&
		GetLength() > GetLegalMaxLength())
	{
		RaiseAlert(SSL_Fatal,SSL_Illegal_Parameter);
	}
}

SSL_Record_Base *SSL_PlainText::InitEncryptTarget()
{		
	SSL_Record_Base *temp;
	
	temp = OP_NEW(SSL_CipherText, ());
	if(temp == NULL)
		RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
	// Should really be in TLS class, but this is easier.
	if(version.Minor() > 1)
		temp->SetUseIV_Field();

	return temp;
}

SSL_Record_Base *SSL_PlainText::InitDecryptTarget(SSL_CipherSpec *cipher)
{
	SSL_Record_Base *temp;
	
	temp = OP_NEW(SSL_PlainText, ());
	if(temp == NULL)
		RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
	
	return temp;
}


/* Crompressing Deactivated */
/*
void SSL_PlainText::StartPartialCompress()
{ 
if(compress_target != NULL)
delete compress_target;
compress_target = new SSL_Compressed;
if(compress_target == NULL)
RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
epos = ppos = 0;
}

SSL_Record_Base *SSL_PlainText::PartialCompress(SSL_CipherSpec *method,uint32)
{
SSL_Record_Base *ret;

ret = NULL;
if (compress_target != NULL){
compress_target->SetType(GetType());
compress_target->SetVersion(GetVersion());

switch(method->compression){
case SSL_Null_Compression :
default : compress_target->SSL_varvector16::operator =(*this);
break;
}
ret = compress_target;
compress_target = NULL;    
} 
return ret;
}

void SSL_PlainText::StartPartialDecompress()
{
if(decryption_target != NULL)
delete decryption_target;
decryption_target = new SSL_PlainText;
if(decryption_target == NULL)
RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
epos = ppos = 0;
}

SSL_Record_Base *SSL_PlainText::PartialDecompress(SSL_CipherSpec *method,uint32)
{
SSL_Record_Base *ret;

ret = NULL;
if (decryption_target != NULL){
decryption_target->SetType(GetType());
decryption_target->SetVersion(GetVersion());

switch(method->compression){
case SSL_Null_Compression :
default : decryption_target->SSL_varvector16::operator =(*this);
break;
}
ret = decryption_target;
decryption_target = NULL;    
} 

return ret;
}
*/

#endif

