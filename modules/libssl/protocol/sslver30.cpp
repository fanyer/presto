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
#include "modules/libssl/protocol/sslver30.h"
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/base/sslciphspec.h"
#include "modules/libssl/methods/sslcipher.h"

#include "modules/util/cleanse.h"
#include "modules/libssl/debug/tstdump2.h"

#ifdef _DEBUG
#ifdef YNP_WORK
#define TST_DUMP
#endif
#endif


SSL_Version_3_0_MAC::SSL_Version_3_0_MAC()
{
}

/*
SSL_Version_3_0_MAC::SSL_Version_3_0_MAC(const SSL_Version_3_0_MAC *old)
: SSL_MAC(old)
{
}
*/

void SSL_Version_3_0_MAC::CalculateRecordMAC_L(const uint64 &seqence_number,
											 SSL_ProtocolVersion &version, SSL_ContentType type, 
											 SSL_varvector32 &payload, const byte *padding, uint16 p_len,
											 SSL_varvector32 &target)
{
	uint16 padlen;
	SSL_secure_varvector32 headers;
	
	switch(HashID())
	{
    case SSL_MD5 : padlen = 48;
		break;  
    case SSL_SHA : padlen = 40;
		break;  
    default : return;
	}

	headers.ForwardTo(this);
	
#ifdef TST_DUMP
	DumpTofile(secret, secret.GetLength(),"SSL 3.0 Calculating MAC : indata secret ","sslcrypt.txt"); 
#endif
	
	InitHash();
	SSL_Hash::CalculateHash(secret);
	SSL_Hash::CalculateHash(0x36,padlen);
	
	uint16 len = payload.GetLength();
	headers.WriteIntegerL(seqence_number, SSL_SIZE_UINT_64, TRUE);
	headers.WriteIntegerL((byte) type, SSL_SIZE_CONTENT_TYPE, TRUE, FALSE) ;
	headers.WriteIntegerL((uint16) len, SSL_SIZE_UINT_16, TRUE, FALSE) ;
		
#ifdef TST_DUMP
	DumpTofile(headers, headers.GetLength(),"SSL 3.0 Calculating MAC : indata headers ","sslcrypt.txt"); 
#endif
	SSL_Hash::CalculateHash(headers);
	
#ifdef TST_DUMP
	DumpTofile(payload,payload.GetLength(),"SSL 3.0 Calculating MAC : indata fragment ","sslcrypt.txt"); 
#endif
	SSL_Hash::CalculateHash(payload);
	SSL_Hash::ExtractHash(target);
	


#ifdef TST_DUMP
	DumpTofile(target,hash->Size(),"SSL 3.0 Calculating MAC : indata first hash ","sslcrypt.txt"); 
#endif
	InitHash();
	SSL_Hash::CalculateHash(secret);
	SSL_Hash::CalculateHash(0x5c,padlen);
	SSL_Hash::CalculateHash(target/*,MAC_hash->Size()*/);
	SSL_Hash::ExtractHash(target);
	
#ifdef TST_DUMP
	DumpTofile(target,hash->Size(),"SSL 3.0 Calculating MAC : output ","sslcrypt.txt"); 
#endif
}

/*
SSL_MAC *SSL_Version_3_0_MAC::ForkMAC() const
{
	return new SSL_Version_3_0_MAC(this);
}
*/

SSL_Version_3_0::SSL_Version_3_0(uint8 ver_minor)
:SSL_Version_Dependent(3,ver_minor)
{
	Init();
#ifdef TST_DUMP
	PrintfTofile("sslhhansh.txt", "SSL_Version_3_0 constructed\n");
#endif
}

void SSL_Version_3_0::Init()
{ 
}

SSL_Version_3_0::~SSL_Version_3_0()
{
}

#if 0
SSL_ProtocolVersion SSL_Version_3_0::Version() const
{
	return SSL_ProtocolVersion(3,0);
}
#endif

SSL_MAC *SSL_Version_3_0::GetMAC() const
{
	return OP_NEW(SSL_Version_3_0_MAC, ());
}

void SSL_Version_3_0::GetHandshakeHash(SSL_SignatureAlgorithm alg, SSL_secure_varvector32 &target)
{
	GetHandshakeHash(target,NULL,0);
}

void SSL_Version_3_0::GetFinishedMessage(BOOL client, SSL_varvector32 &target)
{ 
	target.SetSecure(TRUE);
	GetHandshakeHash(target , 
        (byte *) (client ? "\x43\x4c\x4e\x54" : "\x53\x52\x56\x52"),4);
}

void SSL_Version_3_0::GetHandshakeHash(SSL_secure_varvector32 &hash_target, const byte *extra_msg, uint32 len)
{
	byte *target;
	int i;
	SSL_Hash_Pointer t_hash1;
	SSL_Hash *t_hash;
	uint32 pad_len,hash_len;
    
	hash_target.Resize(SSL_MD5_LENGTH+SSL_SHA_LENGTH);
	if(hash_target.ErrorRaisedFlag || t_hash1.ErrorRaisedFlag)
		return;

	if(len==0)
		extra_msg = NULL;

	target = hash_target.GetDirect();
	t_hash1.ForwardTo(this);
	
#ifdef TST_DUMP
	PrintfTofile("sslhhash.txt","SSL v3 Extracting Finished message\n");
	DumpTofile(conn_state->session->mastersecret,conn_state->session->mastersecret.GetLength(),"SSL 3.0 HandshakeHash mastersecret","sslhhash.txt");
	DumpTofile(conn_state->session->mastersecret,conn_state->session->mastersecret.GetLength(),"SSL 3.0 HandshakeHash mastersecret","sslhash.txt");

	SSL_secure_varvector16 temphand;
	SSL_secure_varvector16 trg;
	SSL_Hash_Pointer TH;
	byte pad[48]; /* ARRAY OK 2009-04-19 yngve */

	temphand = handshake;
	temphand.Append(conn_state->session->mastersecret);
	temphand.Append(extra_msg, len);
	op_memset(pad, 0x36, 48);
	temphand.Append(pad, 48);
	TH.Set(SSL_MD5);
	TH->CompleteHash(temphand, trg);
	DumpTofile(temphand,temphand.GetLength(),"SSL 3.0 HandshakeHash input1","sslhhash.txt");
	DumpTofile(trg,trg.GetLength(),"SSL 3.0 HandshakeHash temp1","sslhhash.txt");
	temphand = conn_state->session->mastersecret;
	op_memset(pad, 0x5c, 48);
	temphand.Append(pad,48);
	temphand.Append(trg);
	TH->CompleteHash(temphand, trg);
	DumpTofile(temphand,temphand.GetLength(),"SSL 3.0 HandshakeHash input2","sslhhash.txt");
	DumpTofile(trg,trg.GetLength(),"SSL 3.0 HandshakeHash temp2","sslhhash.txt");
	
	temphand = handshake;
	temphand.Append(conn_state->session->mastersecret);
	temphand.Append(extra_msg, len);
	op_memset(pad, 0x36, 40);
	temphand.Append(pad, 40);
	TH.Set(SSL_SHA);
	TH->CompleteHash(temphand, trg);
	DumpTofile(temphand,temphand.GetLength(),"SSL 3.0 HandshakeHash input3","sslhhash.txt");
	DumpTofile(trg,trg.GetLength(),"SSL 3.0 HandshakeHash temp3","sslhhash.txt");
	temphand = conn_state->session->mastersecret;
	op_memset(pad, 0x5c, 40);
	temphand.Append(pad,40);
	temphand.Append(trg);
	TH->CompleteHash(temphand, trg);
	DumpTofile(temphand,temphand.GetLength(),"SSL 3.0 HandshakeHash input4","sslhhash.txt");
	DumpTofile(trg,trg.GetLength(),"SSL 3.0 HandshakeHash temp4","sslhhash.txt");
#endif
	
	for(i=0;i<2;i++)
	{
		if(i==0)
		{
			t_hash1 = SSL_MD5;
			pad_len = 48;
		}
		else
		{
			t_hash1 = SSL_SHA;
			pad_len = 40;
		}
		SSL_Version_Dependent::GetHandshakeHash(t_hash1);

		t_hash = t_hash1;
		hash_len = t_hash->Size();

		if(extra_msg != NULL)    
			t_hash->CalculateHash(extra_msg,len);

		t_hash->CalculateHash(conn_state->session->mastersecret);
		t_hash->CalculateHash(0x36,pad_len);
		t_hash->ExtractHash(target);
#ifdef TST_DUMP               
		DumpTofile(target,hash_len,(i ? "SSL 3.0 HandshakeHash sha temporary step" : "SSL 3.0 HandshakeHash md5 temporary step"),"sslhash.txt");
#endif
		
		t_hash->InitHash();
		t_hash->CalculateHash(conn_state->session->mastersecret);
		t_hash->CalculateHash(0x5c,pad_len);
		
		t_hash->CalculateHash(target,hash_len);  
		target = t_hash->ExtractHash(target);
#ifdef TST_DUMP               
		DumpTofile(target,hash_len,(i ? "SSL 3.0 HandshakeHash sha final step" : "SSL 3.0 HandshakeHash md5 final step"),"sslhash.txt");
#endif
	}
}

BOOL SSL_Version_3_0::PRF(SSL_varvector32 &result, uint32 len, 
						   const SSL_varvector32 &secret_seed,
						   const SSL_varvector32 &data_seed) const
{
	SSL_Hash_Pointer t_md5_1(SSL_MD5), t_sha_1(SSL_SHA);
	SSL_Hash *t_md5, *t_sha;
	uint32 made;
	uint i;
	byte letter;
	byte temp[SSL_MAX_HASH_LENGTH]; /* ARRAY OK 2009-04-22 yngve */
	byte *target;
#ifdef TST_DUMP               
	OpString8 text;
#endif
	
#ifdef TST_DUMP               
	DumpTofile(secret_seed,secret_seed.GetLength(),"SSL 3.0 PRF secret","sslkeys.txt");
	DumpTofile(data_seed,data_seed.GetLength(),"SSL 3.0 PRF seed","sslkeys.txt");
#endif

	if(t_md5_1.ErrorRaisedFlag || t_sha_1.ErrorRaisedFlag)
		return FALSE;
	
	result.SetSecure(TRUE);
	result.Resize(len + SSL_MD5_LENGTH);
	if(result.ErrorRaisedFlag)
		return FALSE;
	
	t_md5 = t_md5_1;
	t_sha = t_sha_1;
	
	letter = 'A';
	i=1;
	made = 0;
	target = result.GetDirect();
	
	while(made<len)
	{
#ifdef TST_DUMP               
		//    text.AppendFormat("SSL 3.0 PRF  inputdata string: Step %d shadata.",i);
		//    DumpTofile(letters,i,text,"sslkeys.txt");
#endif
		
		t_sha->InitHash();
		t_sha->CalculateHash(letter,i);
		t_sha->CalculateHash(secret_seed);
		t_sha->CalculateHash(data_seed);
		t_sha->ExtractHash(temp);
		
#ifdef TST_DUMP               
		text.AppendFormat("SSL 3.0 PRF  outputdata string: Step %d shadata.",i);
		DumpTofile(temp,SSL_SHA_LENGTH,text,"sslkeys.txt");
#endif
		
		t_md5->InitHash();
		t_md5->CalculateHash(secret_seed);
		t_md5->CalculateHash(temp,SSL_SHA_LENGTH);
		target = t_md5->ExtractHash(target);
		
#ifdef TST_DUMP               
		text.AppendFormat("SSL 3.0 PRF  outputdata string: Step %d md5data.",i);
		DumpTofile(target- SSL_MD5_LENGTH,SSL_MD5_LENGTH,text,"sslkeys.txt");
#endif
		
		made += SSL_MD5_LENGTH;
		letter++;
		i++;
	}
	
	OPERA_cleanse(temp,sizeof(temp));
	result.Resize(len);
	
	return !result.ErrorRaisedFlag;
}

void SSL_Version_3_0::CalculateMasterSecret(SSL_secure_varvector16 &mastersecret, const SSL_secure_varvector16 &premaster)
{
	
#ifdef TST_DUMP 
	DumpTofile(premaster,premaster.GetLength(),"SSL Master : premaster","sslkeys.txt");
#endif
	
	SSL_secure_varvector32 random;

	random.Concat(conn_state->client_random, conn_state->server_random);
	
	if(!PRF(mastersecret, SSL_MASTER_SECRET_SIZE, premaster, random))
		mastersecret.Resize(0);

#ifdef TST_DUMP 
	DumpTofile(mastersecret,SSL_MASTER_SECRET_SIZE,"SSL Calcmaster : ","sslkeys.txt");
#endif
}

const byte *SSL_Version_3_0::LoadKeyData(
										 SSL_Hash_Pointer &hashmethod,
										 const byte *secret, 
										 const SSL_varvector32 &seed,
										 SSL_GeneralCipher *cipher, BOOL calckey) const  
{ 
	if(calckey)
		return cipher->LoadKey(secret);
	else
		return cipher->LoadIV(secret);
}

void SSL_Version_3_0::CalculateKeys(const SSL_varvector16 &mastersecret,
									SSL_CipherSpec *client, SSL_CipherSpec *server)
{
	SSL_secure_varvector16 result;
	SSL_varvector32 random, random_c;
	//byte *result;
	const byte *source;
	uint32 keylen,keysize,ivsize,hashsize;
	SSL_GeneralCipher *clientcipher, *servercipher;
	
	clientcipher = client->Method;
	servercipher = server->Method;
	
	hashsize = client->MAC->Size();  
	keysize = clientcipher->KeySize();
	ivsize = clientcipher->IVSize();
	
	keylen = hashsize + keysize + ivsize;
	keylen += keylen;
	
	random.Concat(conn_state->server_random, conn_state->client_random);
	
	if(!PRF(result, keylen, mastersecret, random))
		return;

#ifdef TST_DUMP 
	DumpTofile(result,keylen,"SSL Calckeys : keyblock","sslkeys.txt");
#endif
	
	source = result.GetDirect();
	if(hashsize)
	{
		source = client->MAC->LoadSecret(source,hashsize);
		source = server->MAC->LoadSecret(source,hashsize);
	}
	
	if(keysize || ivsize)
	{
		SSL_Hash_Pointer t_md5(SSL_MD5);
		
		random_c.Concat(conn_state->client_random, conn_state->server_random);

		if(t_md5.ErrorRaisedFlag || random_c.ErrorRaisedFlag)
			return;

		if(keysize)
		{ 
			clientcipher->SetCipherDirection(SSL_Encrypt);
			servercipher->SetCipherDirection(SSL_Decrypt);
			source = LoadKeyData(t_md5, source, random_c, clientcipher, TRUE);
			source = LoadKeyData(t_md5, source, random, servercipher, TRUE);
		}
		if(ivsize)
		{
			source = LoadKeyData(t_md5, source, random_c, clientcipher, FALSE);
			source = LoadKeyData(t_md5, source, random, servercipher, FALSE);
		}
	}
}

BOOL SSL_Version_3_0::SendAlert(SSL_Alert &msg, BOOL cont) const // If necessary translate passed alert
{
	if(msg.GetLevel() == SSL_Internal)
		msg.SetLevel(SSL_Fatal);
	switch (msg.GetDescription())
	{
    case  SSL_Decryption_Failed :
		msg.Set(SSL_Fatal,SSL_Bad_Record_MAC);
		break;
	case  SSL_Bad_Record_MAC :
    case  SSL_Close_Notify :
    case  SSL_Unexpected_Message : 
    case  SSL_Decompression_Failure :
    case  SSL_Handshake_Failure :
    case  SSL_No_Certificate :
    case  SSL_Certificate_Revoked : 
    case  SSL_Certificate_Expired : 
    case  SSL_Certificate_Unknown :
    case  SSL_Illegal_Parameter :
    case  SSL_Bad_Certificate : 
    case  SSL_Unsupported_Certificate :
	case  SSL_No_Cipher_Selected:
	case SSL_Fraudulent_Certificate:
		break;
    case  SSL_Decrypt_Error :
		msg.Set(SSL_Fatal,SSL_Bad_Certificate);
		break;
		
    case  SSL_Export_Restriction :
    case  SSL_Insufficient_Security :
	case  SSL_Insufficient_Security1 :
		//case  SSL_Insufficient_Security2 :
    case  SSL_Protocol_Version_Alert :
	case SSL_NoRenegExtSupport:
		msg.Set(SSL_Fatal,SSL_Handshake_Failure);
		break;
    case  SSL_Access_Denied :
    case  SSL_Unknown_CA :
		msg.SetDescription(SSL_Certificate_Unknown);
		break;
    case  SSL_Certificate_Not_Yet_Valid :
		msg.SetDescription(SSL_Certificate_Expired);
		break;
	case SSL_Authentication_Only_Warning:
		if(cont)
			return TRUE;
		msg.Set(SSL_Fatal,SSL_Handshake_Failure);
		break;
    case  SSL_Record_Overflow :
    case  SSL_Decode_Error :
    case  SSL_InternalError :
    case  SSL_User_Canceled : 
    case  SSL_No_Renegotiation :
    case  SSL_Allocation_Failure  :
    default :
		msg.Set(SSL_Fatal,SSL_Illegal_Parameter);
		return FALSE;
	}
	return TRUE;
}


#endif
