/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#include "modules/search_engine/FileWrapper.h"

FileWrapper::FileWrapper(OpLowLevelFile* wrapped_file, BOOL transfer_ownership)
	: m_f(wrapped_file),
	  m_owns_file(transfer_ownership),
	  m_mode(0)
{
}

FileWrapper::~FileWrapper()
{
	if (m_owns_file)
		OP_DELETE(m_f);
}

OP_STATUS FileWrapper::GetFileInfo(OpFileInfo* info)
{
	return m_f->GetFileInfo(info);
}

const uni_char* FileWrapper::GetFullPath() const
{
	return m_f->GetFullPath();
}

const uni_char* FileWrapper::GetSerializedName() const
{
	return m_f->GetSerializedName();
}

OP_STATUS FileWrapper::GetLocalizedPath(OpString *localized_path) const
{
	return m_f->GetLocalizedPath(localized_path);
}

BOOL FileWrapper::IsWritable() const
{
	return m_f->IsWritable();
}

OP_STATUS FileWrapper::Open(int mode)
{
	m_mode = mode;
	OP_STATUS status =  m_f->Open(mode);
	if (OpStatus::IsError(status))
		m_mode = 0;
	return status;
}

BOOL FileWrapper::IsOpen() const
{
	return m_f->IsOpen();
}

OP_STATUS FileWrapper::Close()
{
	return m_f->Close();
}

OP_STATUS FileWrapper::MakeDirectory()
{
	return m_f->MakeDirectory();
}

#ifdef SUPPORT_OPFILEINFO_CHANGE
OP_STATUS FileWrapper::ChangeFileInfo(const OpFileInfoChange* changes)
{
	return m_f->ChangeFileInfo(changes);
}
#endif

BOOL FileWrapper::Eof() const
{
	return m_f->Eof();
}

OP_STATUS FileWrapper::Exists(BOOL* exists) const
{
	return m_f->Exists(exists);
}

OP_STATUS FileWrapper::GetFilePos(OpFileLength* pos) const
{
	return m_f->GetFilePos(pos);
}

OP_STATUS FileWrapper::SetFilePos(OpFileLength pos, OpSeekMode mode /* = SEEK_FROM_START */)
{
	return m_f->SetFilePos(pos, mode);
}

OP_STATUS FileWrapper::GetFileLength(OpFileLength* len) const
{
	return m_f->GetFilePos(len);
}

OP_STATUS FileWrapper::Write(const void* data, OpFileLength len)
{
	return m_f->Write(data, len);
}

OP_STATUS FileWrapper::Read(void* data, OpFileLength len, OpFileLength* bytes_read)
{
	return m_f->Read(data, len, bytes_read);
}

OP_STATUS FileWrapper::ReadLine(char** data)
{
	return m_f->ReadLine(data);
}

OpLowLevelFile* FileWrapper::CreateCopy()
{
	return m_f->CreateCopy();
}

OpLowLevelFile* FileWrapper::CreateTempFile(const uni_char* prefix)
{
	return m_f->CreateTempFile(prefix);
}

OP_STATUS FileWrapper::CopyContents(const OpLowLevelFile* copy)
{
	return m_f->CopyContents(copy);
}

OP_STATUS FileWrapper::SafeClose()
{
	return m_f->SafeClose();
}

OP_STATUS FileWrapper::SafeReplace(OpLowLevelFile* new_file)
{
	return m_f->SafeReplace(new_file);
}

OP_STATUS FileWrapper::Delete()
{
	return m_f->Delete();
}

OP_STATUS FileWrapper::Flush()
{
	return m_f->Flush();
}

OP_STATUS FileWrapper::SetFileLength(OpFileLength len)
{
	return m_f->SetFileLength(len);
}

#ifdef PI_ASYNC_FILE_OP
void FileWrapper::SetListener(OpLowLevelFileListener* listener)
{
	OP_ASSERT(!"Not implemented");
}

OP_STATUS FileWrapper::ReadAsync(void* data, OpFileLength len, OpFileLength abs_pos)
{
	OP_ASSERT(!"Not implemented");
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS FileWrapper::WriteAsync(const void* data, OpFileLength len, OpFileLength abs_pos)
{
	OP_ASSERT(!"Not implemented");
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS FileWrapper::DeleteAsync()
{
	OP_ASSERT(!"Not implemented");
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS FileWrapper::FlushAsync()
{
	OP_ASSERT(!"Not implemented");
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS FileWrapper::Sync()
{
	OP_ASSERT(!"Not implemented");
	return OpStatus::ERR_NOT_SUPPORTED;
}

BOOL FileWrapper::IsAsyncInProgress()
{
	OP_ASSERT(!"Not implemented");
	return OpStatus::ERR_NOT_SUPPORTED;
}
#endif // PI_ASYNC_FILE_OP

