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
#include "modules/libssl/protocol/sslbaserec.h"
#include "modules/libssl/base/sslciphspec.h"
#include "modules/libssl/protocol/sslmac.h"
#include "modules/libssl/methods/sslcipher.h"
#include "modules/libssl/sslrand.h"
#include "modules/libssl/debug/tstdump2.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"

#ifdef _DEBUG
#define _SSL_DEBUG
#ifdef _SSL_DEBUG
#ifdef YNP_WORK
#define TST_DUMP
#endif
#endif
#endif



SSL_Record_Base *SSL_Record_Base::Encrypt(SSL_CipherSpec *cipher)
{
	uint32 blocksize;
	if(cipher == NULL || (blocksize = cipher->Method->InputBlockSize()) == 0)
	{
		RaiseAlert(SSL_Fatal, SSL_InternalError);
		return NULL;
	}

	OpStackAutoPtr<SSL_Record_Base> encrypt_target(InitEncryptTarget());
	if(ErrorRaisedFlag)
		return NULL;

	LoadAndWritableList crypt_source;
	SSL_secure_varvector32 IV;
	SSL_secure_varvector16 payload_data;
	SSL_secure_varvector16 MAC_data;
	SSL_secure_varvector16 pad_data;

	crypt_source.ForwardTo(this);

	if(encrypt_target->IV_field && blocksize > 1)
	{
		crypt_source.AddItem(&IV);
		// Keep the IV from the previous record (option 2b from RFC 4346)
		IV.SetEnableRecord(TRUE);
		IV.FixedLoadLength(TRUE);
	
		SSL_RND(IV, blocksize);
	}

	crypt_source.AddItem(&payload_data);
	payload_data.SetExternal(GetDirect());
	payload_data.Resize(GetLength());
	payload_data.FixedLoadLength(TRUE);


	crypt_source.AddItem(&MAC_data);
	MAC_data.Resize(cipher->MAC->Size());
	MAC_data.FixedLoadLength(TRUE);


	if(ErrorRaisedFlag)
		return NULL;

	if(blocksize > 1)
	{
		uint16 plen = payload_data.GetLength() + MAC_data.GetLength();
		uint16 elen = cipher->Method->Calc_BufferSize(plen);

		uint16 paddinglength = (elen-plen);
		if(paddinglength == 0)
			paddinglength = blocksize;
		uint8 pad_char = (uint8) (paddinglength-1);

		crypt_source.AddItem(&pad_data);
		pad_data.Resize(paddinglength);
		pad_data.Blank(pad_char);
		pad_data.FixedLoadLength(TRUE);
	}

	{
		SSL_ContentType r_type;
		SSL_ProtocolVersion r_ver;

		r_type = GetType();
		r_ver = GetVersion();
		encrypt_target->SetType(r_type);
		encrypt_target->SetVersion(r_ver);

		if(MAC_data.GetLength())
		{
			SSL_TRAP_AND_RAISE_ERROR_THIS(cipher->MAC->CalculateRecordMAC_L(cipher->Sequence_number, r_ver,
				r_type, payload_data,pad_data.GetDirect(), pad_data.GetLength(), MAC_data));
		}
	}

	SSL_secure_varvector16 tempdata;

	SSL_TRAP_AND_RAISE_ERROR_THIS(crypt_source.WriteRecordL(&tempdata));
	if(ErrorRaisedFlag)
		return NULL;

	cipher->Method->EncryptVector(tempdata, *encrypt_target);

	cipher->Sequence_number++;
	if(ErrorRaisedFlag || cipher->Method->Error())
		return NULL;

	return encrypt_target.release();
}

SSL_Record_Base *SSL_Record_Base::Decrypt(SSL_CipherSpec *cipher)
{
	uint16 blocksize;
	if(cipher == NULL || (blocksize = cipher->Method->InputBlockSize()) == 0)
	{
		RaiseAlert(SSL_Fatal, SSL_InternalError);
		return NULL;
	}

	SSL_secure_varvector16 tempdata;
	tempdata.ForwardTo(this);

	UINT mac_hash_size = cipher->MAC->Size();

	// Counter measure for paper "Lucky Thirteen: Breaking the TLS and DTLS Record Protocols"
	// Sanity check the cipher text length.
	// Lucky Thirteen step 1
	if(blocksize > 1)
	{
		UINT mimmimum_cipher_length = MAX(blocksize, mac_hash_size + 1);

		if (IV_field)
			mimmimum_cipher_length += blocksize;

		if (GetLength() < mimmimum_cipher_length)
		{
			RaiseAlert(SSL_Fatal, SSL_Decryption_Failed);
			return NULL;
		}
	}

	cipher->Method->DecryptVector(*this, tempdata);
	if(ErrorRaisedFlag || cipher->Method->Error())
		return NULL;

#ifdef TST_DUMP
	DumpTofile(tempdata, tempdata.GetLength() ,"Decrypt outputdata (includes padding): ","sslcrypt.txt"); 
#endif

	OP_MEMORY_VAR uint16 plainlength = tempdata.GetLength();

	// The padding length byte as given in the stream
	UINT8 padlen = 0;
	OP_MEMORY_VAR uint16 paddinglength = 0;      
	const byte * OP_MEMORY_VAR source = tempdata.GetDirect();

	{
		BOOL length_OK = TRUE;

		if(blocksize == 1)
		{
			if(plainlength  != GetLength())
				length_OK = FALSE;
		}
		else// if(blocksize != 1)
		{
			if(plainlength % blocksize != 0)
				length_OK = FALSE;
			else if(IV_field)
			{
				if(plainlength < blocksize)
					length_OK = FALSE;
			}
		}
		if(!length_OK)
		{
			RaiseAlert(SSL_Fatal, SSL_Decryption_Failed);
			return NULL;
		}
	}

	if(IV_field && blocksize > 1)
	{
		plainlength -= blocksize;
		source += blocksize;
	}

	int original_plainlength = plainlength;

	OpStackAutoPtr<SSL_Record_Base> decrypt_target(InitDecryptTarget(cipher));
	if(ErrorRaisedFlag)
		return NULL;

	OP_MEMORY_VAR BOOL padding_failure = FALSE;

	if(blocksize > 1)
	{
		if(plainlength == 0)
		{
			RaiseAlert(SSL_Fatal, SSL_Illegal_Parameter);
			return NULL;
		}
		padlen = source[plainlength-1];

		paddinglength = padlen + 1;

		// Lucky Thirteen counter measure step 3
		if (mac_hash_size + padlen + 1 > plainlength)
		{
			for(UINT16 i = 0;i < 256; i++)
			{
				// Dummy check, to avoid timing differences.
				if(source[i] != padlen ) //dummy check
					padding_failure = TRUE;
			}

			padding_failure = TRUE;
			paddinglength = 0;
		}
		else if(version >= SSL_ProtocolVersion(3,2) && paddinglength >0)
		{
			// Lucky Thirteen counter measure step 4
			uint16 i;
			for(i= plainlength - paddinglength;i<plainlength;i++)
			{
				if(source[i] != padlen )
				{
					padding_failure = TRUE;
					paddinglength = 0;
					// Pretending the padding was OK, to counter timing attacks against the MAC.
				}
			}

			BOOL dummy = FALSE;
			// Dummy check, to avoid timing differences.
			for(UINT16 i= 0; i < 256 - padlen - 1; i++)
			{
				if(source[i] != padlen ) // dummy check
					padding_failure = dummy;
			}

		}
		plainlength -= paddinglength;
	}

	SSL_secure_varvector16 MAC_data_master;
	SSL_secure_varvector16 MAC_data_calculated;
	ForwardToThis(MAC_data_master,MAC_data_calculated);
	decrypt_target->ForwardTo(this);

	uint16 MAC_size = cipher->MAC->Size();
	uint16 read_MAC_size = cipher->MAC->Size();
	if(plainlength<MAC_size)
	{
		read_MAC_size = 0;
	}

	uint16 payload_len = plainlength - read_MAC_size;

	source = decrypt_target->Set(source, payload_len);
#ifdef TST_DUMP
	DumpTofile(*decrypt_target, decrypt_target->GetLength() ,"Decrypt outputdata (without MAC and padding): ","sslcrypt.txt"); 
#endif

	source = MAC_data_master.Set(source, read_MAC_size);

#ifdef TST_DUMP
	DumpTofile(MAC_data_master, MAC_size,"Decryptstep received MAC","sslcrypt.txt"); 
#endif

	if(ErrorRaisedFlag)
		return NULL;

	{
		SSL_ContentType r_type;
		SSL_ProtocolVersion r_ver;
		
		r_type = GetType();
		r_ver = GetVersion();
		decrypt_target->SetType(r_type);
		decrypt_target->SetVersion(r_ver);

		if(MAC_size)
		{
			SSL_TRAP_AND_RAISE_ERROR_THIS(cipher->MAC->CalculateRecordMAC_L(cipher->Sequence_number, r_ver,
				r_type, *decrypt_target.get(), source, paddinglength, MAC_data_calculated));

			if(blocksize > 1)
			{
				OP_ASSERT(payload_len == original_plainlength - padlen - 1 - read_MAC_size);
				// Extra mac check done for Lucky Thirteen counter measure step 3, 4 and 5
				// Extra dummy operations to avoid timing attacks on the padding.
				int L1 = 13 + original_plainlength - MAC_size;
				int L2 = 13 + original_plainlength - padlen - 1 - MAC_size;
				int number_of_extra_dummy_mac_compressions = (int)op_ceil((L1 - 55)/64.) - (int)op_ceil((L2 - 55)/64.);

				OP_ASSERT(L1 >= 0 && L2 >= 0 && number_of_extra_dummy_mac_compressions >= 0);

				// Add some random noise on top.
				number_of_extra_dummy_mac_compressions += g_libcrypto_random_generator->GetUint8() & 15;

				for (int i = 0; i < number_of_extra_dummy_mac_compressions; i++)
					cipher->MAC->CalculateHash(source, 1);
			}

			RaiseAlert(cipher->MAC);
			if(ErrorRaisedFlag)
				return NULL;
			
			if(plainlength<MAC_size)
			{
				RaiseAlert(SSL_Fatal, SSL_Illegal_Parameter);
				return NULL;
			}

#ifdef TST_DUMP
			DumpTofile(MAC_data_calculated, MAC_size,"Decryptstep calculated MAC : ","sslcrypt.txt"); 
			DumpTofile(MAC_data_master, MAC_size,"Decryptstep received MAC","sslcrypt.txt"); 
#endif

			OP_ASSERT(MAC_data_calculated.GetLength() == MAC_data_master.GetLength());

			BOOL mac_check_success = (MAC_data_calculated.GetLength() == MAC_data_master.GetLength());
			// Constant time mac check
			int mac_check_size = MIN(MAC_data_calculated.GetLength(), MAC_data_master.GetLength());

			for (int j = 0; j < mac_check_size; j++)
			{
				if (MAC_data_calculated.GetDirect()[j] != MAC_data_master.GetDirect()[j])
					mac_check_success = FALSE;
			}


			if (padding_failure || !mac_check_success)
				decrypt_target->RaiseAlert(SSL_Fatal, SSL_Bad_Record_MAC);
		}
	}
	cipher->Sequence_number++;

	if(ErrorRaisedFlag)
		return NULL;

	return decrypt_target.release();
}

#endif
