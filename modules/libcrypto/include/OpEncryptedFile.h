/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#ifndef ENCRYPTED_FILE_H_
#define ENCRYPTED_FILE_H_

#if defined(CRYPTO_ENCRYPTED_FILE_SUPPORT)

#include "modules/pi/system/OpLowLevelFile.h"
#include "modules/forms/tempbuffer8.h"

// Turn on this api with API_CRYPTO_ENCRYPTED_FILE_SUPPORT. Don't use this class directly, use OpFile::ConstructEncrypted() from util/opfile/opfile.h.
class CryptoStreamEncryptionCFB;

class OpEncryptedFile: public OpLowLevelFile
{
public:	
	static OP_STATUS Create(OpLowLevelFile** new_file, const uni_char* path, const UINT8 *key, int key_length = 16, BOOL serialized=FALSE);

	OpEncryptedFile();
	virtual ~OpEncryptedFile();
	
	virtual OP_STATUS GetFileInfo(OpFileInfo* info);
#ifdef SUPPORT_OPFILEINFO_CHANGE	
	virtual OP_STATUS ChangeFileInfo(const OpFileInfoChange* changes) { return m_file->ChangeFileInfo(changes);}
#endif	
	virtual const uni_char* GetFullPath() const { return m_file->GetFullPath(); }
	virtual const uni_char* GetSerializedName() const { return m_file->GetSerializedName(); }
	virtual OP_STATUS GetLocalizedPath(OpString *localized_path) const { return m_file->GetLocalizedPath(localized_path); }	
	virtual BOOL IsWritable() const { return m_file->IsWritable(); }
	virtual OP_STATUS Open(int mode);
	virtual BOOL IsOpen() const { return m_file->IsOpen(); };
	virtual OP_STATUS Close(){ return m_file->Close(); };

	virtual OP_STATUS MakeDirectory() { return OpStatus::ERR;}
	virtual BOOL Eof() const { return m_file->Eof(); }
	virtual OP_STATUS Exists(BOOL* exists) const { return m_file->Exists(exists); }

	virtual OP_STATUS GetFilePos(OpFileLength* pos) const;
	virtual OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode=SEEK_FROM_START);
	virtual OP_STATUS GetFileLength(OpFileLength* len) const;
	virtual OP_STATUS SetFileLength(OpFileLength len);
	virtual OP_STATUS Write(const void* data, OpFileLength len); 
	virtual OP_STATUS Read(void* data, OpFileLength len, OpFileLength* bytes_read);
	virtual OP_STATUS ReadLine(char** data);

	
	virtual OpLowLevelFile* CreateCopy();
	virtual OpLowLevelFile* CreateTempFile(const uni_char* prefix);
	virtual OP_STATUS CopyContents(const OpLowLevelFile* source){ return m_file->CopyContents(source); }
	virtual OP_STATUS SafeClose(){ return m_file->SafeClose(); }
	virtual OP_STATUS SafeReplace(OpLowLevelFile* source){ return m_file->SafeReplace(source); }
	virtual OP_STATUS Delete(){ return m_file->Delete(); }
	virtual OP_STATUS Flush(){ return m_file->Flush(); }

#ifdef PI_ASYNC_FILE_OP
	virtual void SetListener(OpLowLevelFileListener *listener) {}

	virtual OP_STATUS ReadAsync(void* data, OpFileLength len, OpFileLength abs_pos = FILE_LENGTH_NONE) { return OpStatus::ERR; }

	virtual OP_STATUS WriteAsync(const void* data, OpFileLength len, OpFileLength abs_pos = FILE_LENGTH_NONE) { return OpStatus::ERR; }

	virtual OP_STATUS DeleteAsync() { return OpStatus::ERR; }

	virtual OP_STATUS FlushAsync() { return OpStatus::ERR; }

	virtual OP_STATUS Sync() { return OpStatus::ERR; }

	virtual BOOL IsAsyncInProgress() { return FALSE; }
#endif // PI_ASYNC_FILE_OP
	
private:
	OP_STATUS EnsureBufferSize(OpFileLength size); /* Will delete content if too small */
	
	
private:
	OP_STATUS OpenEncryptedFile2ndPhase(int mode);

	OpLowLevelFile* m_file;
	CryptoStreamEncryptionCFB *m_stream_cipher;
	
	UINT8 *m_key;
	UINT8 *m_internal_buffer;
	OpFileLength m_internal_buffer_size;
	
	UINT8 *m_iv_buf;
	UINT8 *m_temp_buf; /* Preallocated work buffer */
	
	BOOL m_first_append;
	OpFileLength m_first_append_block_ptr;
	
	BOOL m_serialized;
	
	BOOL m_file_position_changed;
};

#endif // CRYPTO_ENCRYPTED_FILE_SUPPORT

#endif /* ENCRYPTED_FILE_H_ */
