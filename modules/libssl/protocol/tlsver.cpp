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
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/base/sslciphspec.h"
#include "modules/libssl/protocol/tlsver.h"
#include "modules/libssl/methods/sslcipher.h"

#ifdef YNP_WORK
#include "modules/libssl/debug/tstdump2.h"
#define SSL_VERIFIER
#endif

BOOL TLS_Version_1_Dependent::P_hash(SSL_varvector32 &result, uint32 len, 
							  const SSL_varvector32 &secret_seed,
							  const char *label,  //Null teminated string
							  const SSL_varvector32 &data_seed,
							  SSL_HashAlgorithmType hash) const
{
	SSL_MAC *mac;
	uint32 made,hash_len,blen,i;
	byte *target;
	SSL_secure_varvector32 seeder;
#ifdef SSL_VERIFIER
	OpString8 text;
#endif
	
	result.SetSecure(TRUE);

	mac = GetMAC();
	if(mac == NULL)
		return FALSE;
	
	mac->SetHash(hash);
	hash_len = mac->Size();
	
#ifdef SSL_VERIFIER               
	DumpTofile(secret_seed,secret_seed.GetLength(),"TLS 1.2 P_hash secret","sslkeys.txt");
	DumpTofile((byte *) label,(label ? op_strlen(label) : 0),"TLS 1.2 P_hash label","sslkeys.txt");
	DumpTofile(data_seed,data_seed.GetLength(),"TLS 1.2 P_hash seed","sslkeys.txt");
#endif
	
	blen = len + hash_len;
	result.Resize(blen);
	if(result.Error() || hash_len==0) // hash_len can be 0 only if mac->SetHash(hash); fails;
	{
		OP_DELETE(mac);
		return FALSE;
	}
	target = result.GetDirect();
	
	mac->LoadSecret(secret_seed.GetDirect(),secret_seed.GetLength());
	
	made = 0;
	i = 0;
	
	mac->InitHash();
	mac->SSL_Hash::CalculateHash(label);
	mac->SSL_Hash::CalculateHash(data_seed);
	mac->SSL_Hash::ExtractHash(seeder);
	
	while(made < len)
	{
		i++;
		if(i>1)
			mac->CompleteHash(seeder /*,hash_len*/,seeder);
		
#ifdef SSL_VERIFIER
		text.Empty();
		text.AppendFormat("TLS 1.2 P_hash  inputdata seeder: Step %d.",i);
		DumpTofile(seeder,seeder.GetLength(),text,"sslkeys.txt");
#endif
		
		mac->InitHash();
		mac->SSL_Hash::CalculateHash(seeder/*,hash_len*/);
		mac->SSL_Hash::CalculateHash(label);
		mac->SSL_Hash::CalculateHash(data_seed);
		mac->ExtractHash(target);
		
#ifdef SSL_VERIFIER               
		text.Empty();
		text.AppendFormat("TLS 1.2 P_hash  outputdata string: Step %d.",i);
		DumpTofile(target,hash_len,text,"sslkeys.txt");
#endif
		
		target += hash_len;
		made += hash_len;
	}
	

	result.Resize(len);

	OP_DELETE(mac);
	return TRUE;
}

void TLS_Version_1_Dependent::CalculateMasterSecret(SSL_secure_varvector16 &mastersecret, const SSL_secure_varvector16 &premaster)
{
	SSL_secure_varvector32 random;
	
#ifdef SSL_VERIFIER 
	DumpTofile(premaster,premaster.GetLength(),"TLS 1.0 Master : premaster","sslkeys.txt");
#endif
	
	random.Concat(conn_state->client_random, conn_state->server_random);
	if(!PRF(mastersecret, SSL_MASTER_SECRET_SIZE, premaster,"master secret", random))
		mastersecret.Resize(0);              

#ifdef SSL_VERIFIER 
	DumpTofile(mastersecret,SSL_MASTER_SECRET_SIZE,"TLS 1.0 Calcmaster : ","sslkeys.txt");
#endif
}

void TLS_Version_1_Dependent::CalculateKeys(const SSL_varvector16 &mastersecret, 
									SSL_CipherSpec *client, SSL_CipherSpec *server)
{
	SSL_secure_varvector16 result;
	SSL_varvector32 random;
	const byte *source;
	uint32 keylen,keysize,ivsize,hashsize;
	
	hashsize = client->MAC->Size();  
	keysize = client->Method->KeySize();
	ivsize = client->Method->IVSize();
	
	keylen = hashsize + keysize + ivsize;
	keylen += keylen;
	
	random.Concat(conn_state->server_random, conn_state->client_random);
	if(!PRF(result,keylen,mastersecret,"key expansion", random))
		return;              

#ifdef SSL_VERIFIER 
	DumpTofile(result,keylen,"TLS 1.X Calckeys : keyblock","sslkeys.txt");
#endif
	
	source = result.GetDirect();
	if(hashsize)
	{
		source = client->MAC->LoadSecret(source,hashsize);
		source = server->MAC->LoadSecret(source,hashsize);
	}
	
	if(keysize)
	{
		source = client->Method->LoadKey(source);
		source = server->Method->LoadKey(source);
	}
	if(ivsize)
	{
		source = client->Method->LoadIV(source);
		source = server->Method->LoadIV(source);
	}
}

BOOL TLS_Version_1_Dependent::SendAlert(SSL_Alert &msg, BOOL cont) const // If necessary translate passed alert
{
	if(msg.GetLevel() == SSL_Internal)
		msg.SetLevel(SSL_Fatal);
	switch (msg.GetDescription())
	{
    case  SSL_Decryption_Failed :
    case  SSL_Close_Notify :
	case  SSL_Bad_Record_MAC :
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
    case  SSL_Export_Restriction :
    case  SSL_Insufficient_Security : 
    case  SSL_Insufficient_Security1 : 
    case  SSL_Protocol_Version_Alert :
	case SSL_NoRenegExtSupport:
    case  SSL_Access_Denied :
    case  SSL_Unknown_CA :
    case  SSL_Record_Overflow :
    case  SSL_Decode_Error :
    case  SSL_Decrypt_Error :
    case  SSL_InternalError :
    case  SSL_User_Canceled : 
	case  SSL_No_Cipher_Selected:
    case  SSL_No_Renegotiation :
	case  SSL_Fraudulent_Certificate:
		break;
    case  SSL_Allocation_Failure  :
		msg.Set(SSL_Fatal,SSL_InternalError);
		break;
	case SSL_Authentication_Only_Warning:
		if(cont)
			return TRUE;
		msg.SetLevel(SSL_Fatal);
		break;
    default :
		msg.SetLevel(SSL_Fatal);
		return FALSE;
	}
	return TRUE;
}


void TLS_Version_1_Dependent::GetFinishedMessage(BOOL client, SSL_varvector32 &target)
{
	SSL_secure_varvector32 handshakehash;
	
	final_hash->ExtractHash(handshakehash);
	if(handshakehash.Error())
	{
		target.Resize(0); 
		return;
	}

#ifdef SSL_VERIFIER        
	PrintfTofile("sslhhash.txt", "TLS 1.2 Get %s Finished message\n", (client ? "client" : "server"));
	DumpTofile(conn_state->session->mastersecret,conn_state->session->mastersecret.GetLength(),"TLS 1.2 HandshakeHash mastersecret","sslhhash.txt");
	DumpTofile(handshake,handshake.GetLength(),"TLS 1.2 HandshakeHash handshake","sslhhash.txt");

	SSL_Hash_Pointer t_hash(version.Minor() >=3 ? SSL_SHA_256 : SSL_MD5_SHA);
	SSL_varvector32 trgt;

	t_hash->CompleteHash(handshake,trgt);
	DumpTofile(trgt,trgt.GetLength(),"TLS 1.2 HandshakeHash calculated digest result","sslhhash.txt");

	trgt.Resize(0);
	GetHandshakeHash(t_hash);
	t_hash->ExtractHash(trgt);
	DumpTofile(trgt,trgt.GetLength(),"TLS 1.2 complete HandshakeHash SHA-X calculated digest result","sslhhash.txt");
#endif
	
#ifdef SSL_VERIFIER               
	DumpTofile(handshakehash,handshakehash.GetLength(),"TLS 1.2 HandshakeHash hash result","sslhhash.txt");
#endif
	target.SetSecure(TRUE);

	if(!PRF(target, 12,conn_state->session->mastersecret, (client ? "client finished" : "server finished"),handshakehash))
		target.Resize(0);

#ifdef SSL_VERIFIER               
	DumpTofile(target,target.GetLength(),"TLS 1.2 HandshakeHash PRF result","sslhhash.txt");
#endif
}

SSL_MAC *TLS_Version_1_Dependent::GetMAC() const
{
	return OP_NEW(TLS_Version_1_MAC, ());
}

TLS_Version_1_MAC::TLS_Version_1_MAC()
{
}

/*
TLS_Version_1_MAC::TLS_Version_1_MAC(const TLS_Version_1_MAC *old)
: SSL_MAC(old)
{ 
}
*/

TLS_Version_1_MAC::~TLS_Version_1_MAC()
{
}

void TLS_Version_1_MAC::SetHash(SSL_HashAlgorithmType nhash)
{
	switch(nhash)
	{
	case SSL_MD5:
		nhash = SSL_HMAC_MD5;
		break;
	case SSL_SHA:
		nhash = SSL_HMAC_SHA;
		break;
	case SSL_SHA_256:
		nhash = SSL_HMAC_SHA_256;
		break;
	case SSL_SHA_384:
		nhash = SSL_HMAC_SHA_384;
		break;
	case SSL_SHA_512:
		nhash = SSL_HMAC_SHA_512;
		break;
	}

	SSL_MAC::SetHash(nhash);
}

const byte *TLS_Version_1_MAC::LoadSecret(const byte *secret_buffer,uint32 len)
{
	return hash->LoadSecret(secret_buffer, len);
}


void TLS_Version_1_MAC::CalculateRecordMAC_L(const uint64 &seqence_number,
											  SSL_ProtocolVersion &version, SSL_ContentType type, 
											  SSL_varvector32 &payload, const byte *padding, uint16 p_len,
											  SSL_varvector32 &target)
{
	SSL_varvector32 pad;
	ANCHOR(SSL_varvector32, pad);
	
	uint16 len = payload.GetLength();
	pad.WriteIntegerL(seqence_number, SSL_SIZE_UINT_64, TRUE);
	pad.WriteIntegerL(type, SSL_SIZE_CONTENT_TYPE, TRUE, FALSE);
	version.WriteRecordL(&pad);
	pad.WriteIntegerL(len, SSL_SIZE_UINT_16, TRUE, FALSE);

#ifdef SSL_VERIFIER
	DumpTofile(pad,pad.GetLength(),"TLS 1.0 MAC Input 1","sslcrypt.txt");
	DumpTofile(payload,payload.GetLength(),"TLS 1.0 MAC Input 2","sslcrypt.txt");
#endif
	
	InitHash();  
	SSL_Hash::CalculateHash(pad);
	SSL_Hash::CalculateHash(payload);
	
	SSL_Hash::ExtractHash(target);
#ifdef SSL_VERIFIER
	DumpTofile(target, target.GetLength(), "Extract TLS 1.0 record MAC", "sslcrypt.txt");
#endif
}

/*
SSL_MAC *TLS_Version_1_MAC::ForkMAC() const
{
	return new TLS_Version_1_MAC(this);
}
*/



#endif
