/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */
#include "core/pch.h"

#if defined(CRYPTO_ENCRYPTED_FILE_SUPPORT)

#include "modules/libcrypto/include/OpEncryptedFile.h"
#include "modules/libcrypto/include/CryptoHash.h"
#include "modules/libcrypto/include/CryptoStreamEncryptionCFB.h"
#include "modules/libcrypto/include/CryptoSymmetricAlgorithm.h"
#include "modules/util/opfile/opfile.h"
#include "modules/stdlib/util/opdate.h"

#define CORRECT_KEY_CHECK "CORRECTKEYCHECK:"

/* Encryption format
 * 	
 * Since we use AES in CFB mode, the file encryption needs an Initialization vector IV. 
 * 
 * The IV is stored in the first n bytes as plain text, where n is the block size of the cipher. The next n bytes is an encryption of CORRECT_KEY_CHECK defined above. The rest of 
 * the file is normal encrypted text.
 * 
 * Formally:
 * 
 * Let IV=[iv_0, ..., iv_(n-1)] be the IV vector, P=[p_0,p_1...p_(l-1)] be the plain text, and Let C=[ c_0, c_1, ...,c_(l+(n*2-1))] be the resulting cipher text,
 * where iv_i, p_i and c_i are bytes. 
 * 
 * E_k is the AES encryption algorithm, and k is the key. 
 *  
 * C = [iv_0,iv_1,...,iv_(n-1), E_k(CORRECT_KEY_CHECK), E_k(p_0,p_1,....,p_(l-1)) ]
 * 
 */


OpEncryptedFile::OpEncryptedFile()
	: m_file(NULL)
	, m_stream_cipher(NULL)
	, m_key(NULL)
	, m_internal_buffer(NULL)
	, m_internal_buffer_size(0)
	, m_iv_buf(NULL)
	, m_temp_buf(NULL)
	, m_first_append(FALSE)
	, m_first_append_block_ptr(0)
	, m_serialized(FALSE)
	, m_file_position_changed(FALSE)
{
}

OpEncryptedFile::~OpEncryptedFile()
{
	op_memset(m_key, 0, m_stream_cipher->GetKeySize()); /* for security */
	
	OP_DELETE(m_file);	
	OP_DELETE(m_stream_cipher);	
	OP_DELETEA(m_internal_buffer);
	
	OP_DELETEA(m_key);
	OP_DELETEA(m_temp_buf);
	OP_DELETEA(m_iv_buf);
}

/* static */ OP_STATUS OpEncryptedFile::Create(OpLowLevelFile** new_file, const uni_char* path, const UINT8 *key, int key_length, BOOL serialized)
{
	if (!new_file || !path || !key || key_length <= 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	*new_file = NULL; 
	OpStackAutoPtr<OpEncryptedFile> temp_file(OP_NEW(OpEncryptedFile, ()));
	if (!temp_file.get())
		return OpStatus::ERR_NO_MEMORY;
	
	RETURN_IF_ERROR(OpLowLevelFile::Create(&temp_file->m_file, path, serialized));
	
	CryptoSymmetricAlgorithm *alg = CryptoSymmetricAlgorithm::CreateAES(key_length);
	if (!alg || !(temp_file->m_stream_cipher = CryptoStreamEncryptionCFB::Create(alg)))
	{
		OP_DELETE(alg);
		return OpStatus::ERR_NO_MEMORY;
	}

	temp_file->m_key = OP_NEWA(byte, key_length);
	if (temp_file->m_key == NULL)
		return OpStatus::ERR_NO_MEMORY;

	op_memcpy(temp_file->m_key, key, key_length);
	
	temp_file->m_stream_cipher->SetKey(key);
	if ((temp_file->m_iv_buf = OP_NEWA(byte, temp_file->m_stream_cipher->GetBlockSize())) == NULL)
		return OpStatus::ERR_NO_MEMORY;
		
	temp_file->m_serialized = serialized;
	
	*new_file = temp_file.release();
	return OpStatus::OK;
}

/* virtual */OP_STATUS OpEncryptedFile::GetFileInfo(OpFileInfo* info)
{
	return m_file->GetFileInfo(info);
}

/* virtual */OP_STATUS OpEncryptedFile::Open(int mode)
{
	if (
		(mode & (OPFILE_WRITE | OPFILE_READ)) == (OPFILE_WRITE | OPFILE_READ) ||
		(mode & (OPFILE_APPEND | OPFILE_READ)) == (OPFILE_APPEND | OPFILE_READ)
		)
	{
		OP_ASSERT(!"Encrypted files cannot be read and written at the same time"); // FixMe		
		return OpStatus::ERR;
	}
	
	mode &= ~OPFILE_TEXT; // OPFILE_TEXT mode does not work with encryption
	RETURN_IF_ERROR(m_file->Open(mode));

	OP_STATUS stat = OpenEncryptedFile2ndPhase(mode);
	if (OpStatus::IsError(stat))
		m_file->Close();	// never keep the file open when returning an error
	return stat;
}

OP_STATUS OpEncryptedFile::OpenEncryptedFile2ndPhase(int mode)
{
	unsigned int block_size = static_cast<unsigned int>(m_stream_cipher->GetBlockSize());
		
	BOOL exists = FALSE;
	RETURN_IF_ERROR(OpStatus::IsSuccess(m_file->Exists(&exists)));
	if (mode & OPFILE_READ)
	{
		if (exists == FALSE)
			return OpStatus::OK;

		OpFileLength length;
		RETURN_IF_ERROR(m_file->GetFileLength(&length));
		if (length < 2 * block_size)
			return OpStatus::ERR;		
		
		OpFileLength length_read;		
		/* Read up the iv (iv is not encrypted)*/
		RETURN_IF_ERROR(m_file->Read(m_iv_buf, block_size, &length_read));
		if (length_read != block_size)
			return OpStatus::ERR;
		m_stream_cipher->SetIV(m_iv_buf);
	}
				
	if ((mode & OPFILE_APPEND) && exists)
	{
		OpFile get_iv_file;
		RETURN_IF_ERROR(get_iv_file.Construct(m_file->GetFullPath()));
		RETURN_IF_ERROR(get_iv_file.Open(OPFILE_READ));
		
		OpFileLength length;
		RETURN_IF_ERROR(get_iv_file.GetFileLength(length));
		if (length < 2*block_size)
			return OpStatus::ERR;

		length -= block_size;
		
		RETURN_IF_ERROR(EnsureBufferSize(2 * block_size));
		OpFileLength mult_pos = (length/block_size) * block_size;
		
		if ((m_temp_buf = OP_NEWA(byte, block_size)) == NULL)
			return OpStatus::ERR_NO_MEMORY;

		op_memset(m_temp_buf, 0, block_size);
		OpFileLength read;
		/* Read up the first previous full cipher text, and use that as IV state */
		if (mult_pos >= block_size)
		{
			m_first_append = TRUE;
			m_first_append_block_ptr = length - mult_pos;
			RETURN_IF_ERROR(get_iv_file.SetFilePos(mult_pos /* - block_size */, SEEK_FROM_START));
			RETURN_IF_ERROR(get_iv_file.Read(m_iv_buf, block_size, &read));
			OP_ASSERT(read == block_size);
			m_stream_cipher->SetIV(m_iv_buf);			
			RETURN_IF_ERROR(get_iv_file.Read(m_temp_buf, length - mult_pos, &read));
		}
		else
		{
			m_first_append = TRUE;
			m_first_append_block_ptr = length % block_size;
			RETURN_IF_ERROR(get_iv_file.Read(m_iv_buf, block_size, &read)); /* The IV is the first block_size bytes in the file */
			m_stream_cipher->SetIV(m_iv_buf);

			RETURN_IF_ERROR(get_iv_file.Read(m_temp_buf, length % block_size, &read));			
		}
		m_stream_cipher->SetKey(m_key);
		RETURN_IF_ERROR(get_iv_file.Close());
	}
	const char *encrypted_check = CORRECT_KEY_CHECK;

	if (mode & (OPFILE_READ))
	{
		char *check_str = OP_NEWA(char, op_strlen(encrypted_check));
		if (check_str == NULL)
			return OpStatus::ERR_NO_MEMORY;
		
		ANCHOR_ARRAY(char, check_str);
		OpFileLength length_read;
		RETURN_IF_ERROR(Read(check_str, op_strlen(encrypted_check), &length_read));
		if (length_read != (OpFileLength)op_strlen(encrypted_check))
			return OpStatus::ERR;
		
		if (op_strncmp(check_str, encrypted_check, op_strlen(encrypted_check)))
			return OpStatus::ERR_NO_ACCESS;
	}

	if (
			(mode & ( OPFILE_WRITE)) || 
			((mode | OPFILE_APPEND) && exists == FALSE)
		)
	{
		// Calculate an IV (Doesn't have to be very random)
		CryptoHash *hasher;
		hasher = CryptoHash::CreateSHA1();
		if (hasher == NULL)
			return OpStatus::ERR_NO_MEMORY;
		
		unsigned char hash_buffer[20]; /* ARRAY OK 2008-11-10 haavardm */
		
		OP_ASSERT(hasher->Size() == 20);
		OP_ASSERT(sizeof(hash_buffer) == hasher->Size() && static_cast<unsigned int>(hasher->Size()) >= block_size);		
		const uni_char *path = m_file->GetFullPath();
		
		hasher->InitHash();
		hasher->CalculateHash(reinterpret_cast<const UINT8 *>(path), sizeof(uni_char)*uni_strlen(path));
		hasher->CalculateHash(reinterpret_cast<const UINT8 *>(this), sizeof(this));
		double gmt_unix_time = OpDate::GetCurrentUTCTime();
		hasher->CalculateHash(reinterpret_cast<const UINT8 *>(&gmt_unix_time), sizeof(double));
		hasher->ExtractHash(hash_buffer);
		OP_DELETE(hasher);
		
		op_memcpy(m_iv_buf, hash_buffer, block_size);
		// Write IV as plain text, only use the first block_size bytes of the hash_buffer. 
		RETURN_IF_ERROR(m_file->Write(hash_buffer, block_size));
		m_stream_cipher->SetIV(m_iv_buf);
		
		RETURN_IF_ERROR(Write(encrypted_check, op_strlen(encrypted_check)));
	}
	
		
	return OpStatus::OK;
}

/* virtual */ OP_STATUS OpEncryptedFile::GetFilePos(OpFileLength* pos) const
{
	if (!pos)
		return OpStatus::ERR_OUT_OF_RANGE;

	OpFileLength real_length;

	RETURN_IF_ERROR(m_file->GetFilePos(&real_length));

	OpFileLength checklen = static_cast<OpFileLength>(op_strlen(CORRECT_KEY_CHECK) + m_stream_cipher->GetBlockSize());

	if (real_length < checklen)
	{
		return OpStatus::ERR;
	}

	*pos = real_length - checklen;

	return OpStatus::OK;
}

/* virtual */OP_STATUS OpEncryptedFile::SetFilePos(OpFileLength pos, OpSeekMode seek_mode)
{
	OpFileLength current_pos;
	RETURN_IF_ERROR(GetFilePos(&current_pos));
	if (current_pos == pos)
		return OpStatus::OK;
	
	OpFileLength file_length;
	RETURN_IF_ERROR(GetFileLength(&file_length));	
	if (file_length < pos)
		return OpStatus::ERR;
	
	OpFileLength block_size = m_stream_cipher->GetBlockSize();
	
	OpFileLength block_position = (pos/block_size) * block_size;
	
	RETURN_IF_ERROR(m_file->SetFilePos(op_strlen(CORRECT_KEY_CHECK) + block_position, seek_mode));
	m_file_position_changed = TRUE;
	
	OpFileLength length;
	RETURN_IF_ERROR(m_file->GetFileLength(&length));
	if (length - pos < block_size)
		return OpStatus::ERR;		
	
	OpFileLength length_read;		
	/* Read up the iv (iv is the previous encrypted block) */
	RETURN_IF_ERROR(m_file->Read(m_iv_buf, block_size, &length_read));
	if (length_read != block_size)
		return OpStatus::ERR;
	
	m_stream_cipher->SetIV(m_iv_buf);	
	
	/* Skip the first pos - block_position bytes */
	OP_ASSERT(block_size <= CRYPTO_MAX_CIPHER_BLOCK_SIZE);
	UINT8 temp_buf2[CRYPTO_MAX_CIPHER_BLOCK_SIZE];

	return Read(temp_buf2, pos - block_position, &length_read);
	
}

/* virtual */OP_STATUS OpEncryptedFile::GetFileLength(OpFileLength* len) const
{
	if (!len)
		return OpStatus::ERR_OUT_OF_RANGE;

	OpFileLength real_length;
	
	RETURN_IF_ERROR(m_file->GetFileLength(&real_length));
	OpFileLength checklen = static_cast<OpFileLength>(op_strlen(CORRECT_KEY_CHECK) + m_stream_cipher->GetBlockSize()); 
	
	if (real_length < checklen)
	{
		return OpStatus::ERR;
	}
	
	*len = real_length - checklen;

	return OpStatus::OK; 
}


/* virtual */OP_STATUS OpEncryptedFile::SetFileLength(OpFileLength len)
{ 
	OP_ASSERT(!"Dont use");
	return m_file->SetFileLength(len + static_cast<OpFileLength>(op_strlen(CORRECT_KEY_CHECK) + m_stream_cipher->GetBlockSize()));
}

/* virtual */OP_STATUS OpEncryptedFile::Write(const void* data, OpFileLength len)
{
	if (m_file_position_changed)
	{
		OP_ASSERT(!"SetFilePos cannot be used when writing files, file will be destroyed, and will cause security problems");
		return OpStatus::ERR;
	}
	
	if (len == 0)
		return OpStatus::OK;

	if (!data || len <= 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	unsigned int block_size = m_stream_cipher->GetBlockSize();
	RETURN_IF_ERROR(EnsureBufferSize(len + block_size));

	if (m_first_append)
	{
		OP_ASSERT(m_temp_buf != NULL);
		OP_ASSERT(m_first_append_block_ptr <= block_size);

		if (m_first_append_block_ptr > block_size)
			return OpStatus::ERR;

		/* calculate correct state from cipher text on last block in file, and the data written */
		OpFileLength rest_block_len = block_size - m_first_append_block_ptr;
		OpFileLength rest_data_len = len > rest_block_len ? len - rest_block_len : 0;
		
		OP_ASSERT(block_size <= CRYPTO_MAX_CIPHER_BLOCK_SIZE);
		UINT8 temp_buf2[CRYPTO_MAX_CIPHER_BLOCK_SIZE];

		op_memset(temp_buf2, 0, block_size);
		op_memcpy(temp_buf2 + m_first_append_block_ptr, data, (size_t) MIN(rest_block_len, len));
		
		OpFileLength encrypt_length =  len < rest_block_len ? m_first_append_block_ptr + len : block_size;
		m_stream_cipher->Encrypt(temp_buf2, m_internal_buffer, (int) encrypt_length);
		
		op_memmove(m_internal_buffer, m_internal_buffer + m_first_append_block_ptr, (size_t) MIN(rest_block_len, len)); 
		
		op_memcpy(m_temp_buf + m_first_append_block_ptr, m_internal_buffer, (size_t) MIN(rest_block_len, len));
		if (rest_data_len > 0)
		{
			m_stream_cipher->SetIV(m_temp_buf);
			m_first_append = FALSE;
			m_stream_cipher->Encrypt(static_cast<const byte*>(data) + rest_block_len, m_internal_buffer + rest_block_len, (int) rest_data_len);
		}
		else
		{
			m_stream_cipher->SetIV(m_iv_buf);
			m_first_append_block_ptr += len; 
		}
	}
	else
		m_stream_cipher->Encrypt(static_cast<const byte*>(data), m_internal_buffer, (int) len);
	
	
	return m_file->Write(m_internal_buffer, len);
}

/* virtual */OP_STATUS OpEncryptedFile::Read(void* data, OpFileLength len, OpFileLength* bytes_read)
{
	if (len == 0)
		return OpStatus::OK;

	if (!data || len <= 0)
		return OpStatus::ERR_OUT_OF_RANGE;
	if (!bytes_read)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(EnsureBufferSize(len + m_stream_cipher->GetBlockSize())); 

	RETURN_IF_ERROR(m_file->Read(m_internal_buffer, len, bytes_read));
	m_stream_cipher->Decrypt(m_internal_buffer, static_cast<byte*>(data), (int) *bytes_read);
	
	return OpStatus::OK;
}

/* virtual */OP_STATUS OpEncryptedFile::ReadLine(char** data)
{
	OP_ASSERT(!"Current version does not support reading line");
	return OpStatus::ERR;
}

OpLowLevelFile* OpEncryptedFile::CreateCopy()
{
	OpStackAutoPtr<OpEncryptedFile> temp_file(OP_NEW(OpEncryptedFile, ()));
	
	if (!temp_file.get())
		return NULL;
	
	const uni_char *path =  m_file->GetFullPath();
	if (OpStatus::IsError(OpLowLevelFile::Create(&temp_file->m_file, path, m_serialized)))
		return NULL;

	CryptoSymmetricAlgorithm *alg = CryptoSymmetricAlgorithm::CreateAES(m_stream_cipher->GetKeySize());
	if (!alg || !(temp_file->m_stream_cipher = CryptoStreamEncryptionCFB::Create(alg)))
	{
		OP_DELETE(alg);
		return NULL;
	}		
	
	if ((temp_file->m_key = OP_NEWA(byte, m_stream_cipher->GetKeySize())) == NULL)
		return NULL;

	op_memcpy(temp_file->m_key, m_key, m_stream_cipher->GetKeySize());
	
	temp_file->m_stream_cipher->SetKey(m_key);
	
	unsigned int block_size = temp_file->m_stream_cipher->GetBlockSize();
		
	if ((temp_file->m_iv_buf = OP_NEWA(byte, block_size)) == NULL)
		return NULL;
	
	CryptoHash *hasher;
	if ((hasher = CryptoHash::CreateSHA1()) == NULL)
		return NULL;
	
	unsigned char hash_buffer[20]; /* ARRAY OK 2009-06-18 alexeik */
	
	OP_ASSERT(hasher->Size() == 20);
	OP_ASSERT(sizeof(hash_buffer) == hasher->Size() && static_cast<unsigned int>(hasher->Size()) >= block_size);
	
	hasher->InitHash();
	hasher->CalculateHash(reinterpret_cast<const UINT8 *>(path), sizeof(uni_char)*uni_strlen(path));
	hasher->CalculateHash(reinterpret_cast<const UINT8 *>(this), sizeof(this));
	double gmt_unix_time = OpDate::GetCurrentUTCTime();
	hasher->CalculateHash(reinterpret_cast<const UINT8 *>(&gmt_unix_time), sizeof(double));
	
	hasher->ExtractHash(hash_buffer);
	op_memcpy(temp_file->m_iv_buf, hash_buffer, block_size);
	
	OP_DELETE(hasher);
	
	temp_file->m_stream_cipher->SetIV(temp_file->m_iv_buf);
	
	
	temp_file->m_internal_buffer_size =  m_internal_buffer_size;

	if (m_internal_buffer && (temp_file->m_internal_buffer = OP_NEWA(byte, (size_t) m_internal_buffer_size)) == NULL)
		return NULL;
	
	if (m_temp_buf && (temp_file->m_temp_buf = OP_NEWA(byte, block_size)) == NULL)
		return NULL;
	
	if (temp_file->m_internal_buffer)
		op_memcpy(temp_file->m_internal_buffer, m_internal_buffer, (size_t) m_internal_buffer_size);
	
	if (temp_file->m_iv_buf)
		op_memcpy(temp_file->m_iv_buf, m_iv_buf, block_size);
	
	if (temp_file->m_temp_buf)
		op_memcpy(temp_file->m_temp_buf, m_temp_buf, block_size);
	
	
	temp_file->m_first_append = m_first_append; 
	temp_file->m_first_append_block_ptr = m_first_append_block_ptr;
	
	temp_file->m_serialized = m_serialized;	
	return temp_file.release();	
}

OpLowLevelFile* OpEncryptedFile::CreateTempFile(const uni_char* prefix)
{
	if (!prefix)
		return NULL;

	OpLowLevelFile *new_file;
	OpLowLevelFile *child_file = m_file->CreateTempFile(prefix);
	if (child_file == NULL)
		return NULL;
	
	if (OpStatus::IsError(OpEncryptedFile::Create(&new_file, child_file->GetFullPath(), m_key, m_stream_cipher->GetKeySize(), m_serialized)))
	{
		OP_DELETE(child_file);
		return NULL;
	}
	
	OP_DELETE(static_cast<OpEncryptedFile*>(new_file)->m_file);
	
	static_cast<OpEncryptedFile*>(new_file)->m_file = child_file;
	
	return new_file;
}

OP_STATUS OpEncryptedFile::EnsureBufferSize(OpFileLength size)
{
	if (m_internal_buffer_size < size)
	{
		OP_DELETEA(m_internal_buffer);
		m_internal_buffer = OP_NEWA(byte, (size_t) size);
		
		if (m_internal_buffer == NULL)
		{
			m_internal_buffer_size = 0;
			return OpStatus::ERR_NO_MEMORY;
		}
		else
		{
			m_internal_buffer_size = size;
		}
	}
	
	return OpStatus::OK;
}

#endif // CRYPTO_ENCRYPTED_FILE_SUPPORT
