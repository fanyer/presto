/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#if !defined(ADS12) || defined(ADS12_TEMPLATING) // see end of streamtype.cpp

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/options/cipher_description.h"

SSL_CipherDescriptions::~SSL_CipherDescriptions()
{
	//OP_ASSERT(reference_count == 0); // Should never happen

#ifdef SSL_FULL_DESTRUCT
	method = NULL;
	hash = NULL;
	certhandler = NULL;
	publichandler = NULL;
	key_exchange = NULL;
#endif
}

SSL_CipherDescriptions::SSL_CipherDescriptions(
											   const SSL_CipherID &p_id,
											   const SSL_CipherMethod &p_method,
											   const SSL_CipherNames &names,
											   SSL_SecurityRating p_security_rating,    
											   SSL_AuthentiRating p_authentication,
											   BYTE		p_low_reason
											   )
 :	fullname(names.fullname), KEAname(names.KEAname),
	EncryptName(names.EncryptName), HashName(names.HashName) 
{
	InternalInit(p_id, p_method, p_security_rating, p_authentication, p_low_reason);
}

void SSL_CipherDescriptions::InternalInit(
										   const SSL_CipherID &p_id,
										   const SSL_CipherMethod &p_method,
										   SSL_SecurityRating p_security_rating,    
										   SSL_AuthentiRating p_authentication,
										   BYTE		p_low_reason
										   )
{
	//reference_count = 1;
	id = p_id;
	Init(p_method, p_security_rating, p_authentication, p_low_reason);
}


void SSL_CipherDescriptions::Init(
								   const SSL_CipherMethod &p_method,
								   SSL_SecurityRating p_security_rating,    
								   SSL_AuthentiRating p_authentication,
								   BYTE		p_low_reason
								   )
{
	
	method = p_method.method;
	hash = p_method.hash;
	
	kea_alg = p_method.kea_alg;
	sigalg = p_method.sigalg;
	KeySize = p_method.KeySize;
#ifdef _SUPPORT_TLS_1_2
	hash_tls12 = p_method.hash_tls12;
	prf_fun = p_method.prf_fun;
#endif
	// Removed name init, call Construct instead
	security_rating = p_security_rating;
	authentication = p_authentication; 
	low_reason = 	p_low_reason;
}

/* Unref YNP
SSL_CipherDescriptions::SSL_CipherDescriptions(const SSL_CipherDescriptions & old)
{
reference_count = 1;
id = old.id;
id_v2 = old.id_v2;

  method = old.method;
  hash = old.hash;
  certhandler = old.certhandler;
  publichandler = old.publichandler;
  key_exchange = old.key_exchange;
  kea_alg = old.kea_alg;
  sigalg = old.sigalg;
  US_Exportable = old.US_Exportable;
  KeySize = old.KeySize;
  ExportKeySize = old.ExportKeySize;
  fullname = old.fullname;
  KEAname = old.KEAname;
  EncryptName = old.EncryptName;
  HashName = old.HashName;
  security_rating = old.security_rating;
  authentication = old.authentication; 
  }
*/

SSL_CipherDescriptionHead::~SSL_CipherDescriptionHead()
{
	SSL_CipherDescriptions_Pointer item;

	while((item = First()) != NULL)
	{
		item->Out();
		item->Decrement_Reference();
	}
	item = NULL; // Automatic delete
}

SSL_CipherMethod::SSL_CipherMethod(
								   SSL_BulkCipherType p_method,
								   SSL_HashAlgorithmType p_hash,
								   SSL_KeyExchangeAlgorithm  p_kea_alg,
								   SSL_SignatureAlgorithm p_sigalg,
#ifdef _SUPPORT_TLS_1_2
								   SSL_HashAlgorithmType p_hash_tls12,
								   TLS_PRF_func	p_prf_fun,
#endif
								   uint16 p_KeySize
								   )
{
	method = p_method;
	hash = p_hash;
	kea_alg = p_kea_alg;
	sigalg = p_sigalg;
#ifdef _SUPPORT_TLS_1_2
	hash_tls12 = p_hash_tls12;
	prf_fun = p_prf_fun;
#endif
	KeySize = p_KeySize;
}

SSL_CipherMethod::~SSL_CipherMethod()
{
#ifdef SSL_FULL_DESTRUCT
	US_Exportable = FALSE;
	KeySize = 0;
	ExportKeySize = 0;
#endif
}

SSL_CipherNames::SSL_CipherNames(
								 const char *p_fullname,
								 const char *p_KEAname,
								 const char *p_EncryptName,
								 const char *p_HashName
								 )
 :	fullname(p_fullname), KEAname(p_KEAname),
	EncryptName(p_EncryptName), HashName(p_HashName) 
{
}

SSL_CipherNames::~SSL_CipherNames()
{
}


SSL_CipherDescriptions *SSL_Options::GetCipherDescription(const SSL_ProtocolVersion &ver, int number)
{
	SSL_CipherDescriptions *current;
	int i;

	if (ver.Major() != 3)
		return NULL;

	number ++;
	current = SystemCiphers.First();
	for(i = 0; i<number && current != NULL;)
	{
		i++;
		if(i<number)
			current = current->Suc();
	}

	return current;  
}

BOOL SSL_Options::GetCipherName(const SSL_ProtocolVersion &ver, int number, OpString &name, BOOL &selected)
{
	SSL_CipherDescriptions *current;
	//  uint16 i,count;
	
	name.Empty();
	selected = FALSE;
	
	current = GetCipherDescription(ver,number);
	if(current == NULL)
		return FALSE;
    
	if(OpStatus::IsError(name.Set( current->fullname ) ))
		return FALSE;
	
	switch  (ver.Major())
	{
    case  3 :
		{
			selected = current_security_profile->original_ciphers.Contain(current->id);
			break;
		}
	}
	return TRUE;  
}

void SSL_Options::SetCiphers(const SSL_ProtocolVersion &ver, int num, int *ilist)
{
	const SSL_CipherDescriptions *current;
	int i;
	uint16 j,major;
	
	major = ver.Major();
	switch  (major)
	{
    case  3 : current_security_profile->original_ciphers.Resize((uint16) num);
		break;
	}
	for(i = num,j=0;i>0;j++)
	{
		i--;
		current = GetCipherDescription(ver,(int) ilist[i]);
		if(current == NULL)
			continue;
		
		switch  (major)
		{
		case  3 : current_security_profile->original_ciphers[j] = current->id;
			break;
		}
	}
	switch  (major)
	{
    case  3 : current_security_profile->ciphers= current_security_profile->original_ciphers;
		break;
	}

	if(register_updates)
	{
		updated_v3_ciphers = TRUE;
	}
}

void SSL_Options::AddSystemCiphers(SSL_CipherDescriptions *(*func)(unsigned int),int count)
{ 
	int i;
	SSL_CipherDescriptions *temp;

	if(func == NULL)
		return;
	
	i = 0;
	do
	{
		temp = (*func)(i++);
		if(temp != NULL)
		{
			temp->Into(&SystemCiphers);    
			temp->Increment_Reference();
		}
	}while (temp != NULL);
}

#endif // !ADS12 or ADS12_TEMPLATING
#endif // relevant support
