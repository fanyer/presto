/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/util/opfile/opfile.h"
#include "modules/pi/system/OpFolderLister.h"

#include "modules/util/zipload.h"

#ifdef CRYPTO_ENCRYPTED_FILE_SUPPORT 
#include "modules/libcrypto/include/OpEncryptedFile.h"
#endif

#include "modules/probetools/probepoints.h"

// OpFileDescriptor

OP_STATUS
OpFileDescriptor::WriteUTF8Line(const OpStringC& data)
{
	char* utf8;
	RETURN_IF_ERROR(data.UTF8Alloc(&utf8));
	int len = op_strlen(utf8);
	utf8[len] = '\n'; // utf8 is no longer NULL-terminated, but that's OK
	OP_STATUS result = Write(utf8, len+1);
	OP_DELETEA(utf8);
	return result;
}

OP_STATUS
OpFileDescriptor::ReadUTF8Line(OpString& str)
{
	OpString8 utf8;
	RETURN_IF_ERROR(ReadLine(utf8));
	return str.SetFromUTF8(utf8.CStr());
}

OP_STATUS
OpFileDescriptor::WriteShort(short data)
{
	char buf[13]; // ARRAY OK 2009-03-03 adame
	op_snprintf(buf, 13, "%d\n", data);
	return Write(buf, op_strlen(buf));
}

OP_STATUS
OpFileDescriptor::ReadShort(short& data)
{
	OpString str;
	RETURN_IF_ERROR(ReadUTF8Line(str));
	if (str.HasContent())
	{
		data = uni_atoi(str.CStr());
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS
OpFileDescriptor::ReadBinLong(long& value)
{
	unsigned char c[4]; /* ARRAY OK 2009-04-24 johanh */
	OpFileLength bytes_read;

	RETURN_IF_ERROR(Read(c, 4, &bytes_read));
	if (bytes_read < 4)
	{
		return OpStatus::ERR;
	}

	INT32 val = ((c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3]);
	value = val;
	return OpStatus::OK;
}

OP_STATUS
OpFileDescriptor::ReadBinShort(short& value)
{
	unsigned char c[2]; /* ARRAY OK 2009-04-24 johanh */
	OpFileLength bytes_read;

	RETURN_IF_ERROR(Read(c, 2, &bytes_read));
	if (bytes_read < 2)
	{
		return OpStatus::ERR;
	}

	value = ((c[0] << 8) | c[1]);
	return OpStatus::OK;
}

OP_STATUS
OpFileDescriptor::ReadBinByte(char& value)
{
	OpFileLength bytes_read;
	RETURN_IF_ERROR(Read(&value, 1, &bytes_read));
	if (bytes_read < 1)
	{
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

OP_STATUS
OpFileDescriptor::WriteBinLong(long data)
{
	unsigned char c[4]; /* ARRAY OK 2009-04-24 johanh */

	c[0] = (unsigned char)(data >> 24);
	c[1] = (unsigned char)(data >> 16);
	c[2] = (unsigned char)(data >> 8);
	c[3] = (unsigned char)(data);

	return Write(c, 4);
}

OP_STATUS
OpFileDescriptor::WriteBinLongLH(long data)
{
	unsigned char c[4]; /* ARRAY OK 2009-04-24 johanh */

	// write data in a long LOW-HIGH format
	c[0] = (unsigned char)(data);
	c[1] = (unsigned char)(data >> 8);
	c[2] = (unsigned char)(data >> 16);
	c[3] = (unsigned char)(data >> 24);

	return Write(c, 4);
}

OP_STATUS
OpFileDescriptor::WriteBinShort(short data)
{
	unsigned char c[2]; /* ARRAY OK 2009-04-24 johanh */

	c[0] = (unsigned char)(data >> 8);
	c[1] = (unsigned char)(data);

	return Write(c, 2);
}

OP_STATUS
OpFileDescriptor::WriteBinShortLH(short data)
{
	unsigned char c[2]; /* ARRAY OK 2009-04-24 johanh */

	c[0] = (unsigned char)(data);
	c[1] = (unsigned char)(data >> 8);

	return Write(c, 2);
}

OP_STATUS
OpFileDescriptor::WriteBinByte(char data)
{
	return Write(&data, 1);
}

// OpFile

OpFile::~OpFile()
{
	OP_DELETE(m_file);
}

OP_STATUS
OpFile::Construct(const OpStringC& path, OpFileFolder folder, int flags)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_CONSTRUCT);

	OP_DELETE(m_file);
	m_file = NULL;
	BOOL serialized = folder == OPFILE_SERIALIZED_FOLDER;
	OpString full_path;

	RETURN_IF_ERROR(ConstructPath(full_path, path.CStr(), folder));
	OP_ASSERT(full_path.CStr());
	if (full_path.CStr() == NULL)
		return OpStatus::ERR;

#ifdef UTIL_CONVERT_TO_BACKSLASH
	uni_char* p = full_path.CStr();
	while (*p)
	{
		if (*p == '/')
			*p = '\\';
		++p;
	}
#endif // UTIL_CONVERT_TO_BACKSLASH

#ifdef ZIPFILE_DIRECT_ACCESS_SUPPORT
	if (g_opera && (flags & OPFILE_FLAGS_ASSUME_ZIP || OpZipFolder::MaybeZipArchive(full_path.CStr())))
	{
		OpAutoPtr<OpLowLevelFile> file;
#ifdef UTIL_ZIP_CACHE
		// First, check if we already have a matching zip file in cache.
		OpZip *zip = NULL;
		size_t key_len;
		if (g_zipcache && OpStatus::IsSuccess(g_zipcache->SearchData(full_path.CStr(), key_len, &zip)) && zip)
		{
			// We know there's already a zip file open for some prefix of full_path.
			// We can therefore skip calling OpLowLevelFile::Exists() below, and
			// go straight to creating the appropriate ZipFolder object.
		}
		else
#endif // UTIL_ZIP_CACHE
		// Check if NOT inside a zip archive, but in fact exists on disk
		{
			OpLowLevelFile *diskfile;
			BOOL exists;

			RETURN_IF_ERROR(OpLowLevelFile::Create(&diskfile, full_path.CStr(), serialized));
			file = diskfile;
			RETURN_IF_ERROR(file->Exists(&exists));
			if (exists)
			{
				m_file = file.release();
				return OpStatus::OK;
			}
		}

		// Try to create a OpZipFolder file that transcends the zip file
		{
			OP_STATUS res;
			OpZipFolder *zipfolder = NULL;

			res = OpZipFolder::Create(&zipfolder, full_path.CStr(), flags);
			if (OpStatus::IsSuccess(res))
			{
				m_file = zipfolder;
				return OpStatus::OK;
			}
			OP_DELETE(zipfolder);
		}

		// The path might only look like a zip path (i.e. folder named "sth.zip"),
		// or point to a non-existing object. Use the file object we created
		// above, if any.
		if ((m_file = file.release()) != NULL)
		{
			return OpStatus::OK;
		}
	}
#endif // ZIPFILE_DIRECT_ACCESS_SUPPORT

	return OpLowLevelFile::Create(&m_file, full_path.CStr(), serialized);
}

#ifdef CRYPTO_ENCRYPTED_FILE_SUPPORT 
OP_STATUS
OpFile::ConstructEncrypted(const uni_char* path, const byte *key, unsigned int key_length, OpFileFolder folder)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_CONSTRUCTENCRYPTED);

	OP_DELETE(m_file);
	m_file = NULL;

	OpString full_path;
	RETURN_IF_ERROR(ConstructPath(full_path, path, folder));
	OP_ASSERT(full_path.CStr());
	if (full_path.CStr() == NULL)
		return OpStatus::ERR;

#ifdef UTIL_CONVERT_TO_BACKSLASH
	uni_char* p = full_path.CStr();
	while (*p)
	{
		if (*p == '/')
			*p = '\\';
		++p;
	}
#endif // UTIL_CONVERT_TO_BACKSLASH
	
	BOOL serialized = folder == OPFILE_SERIALIZED_FOLDER;
	return OpEncryptedFile::Create(&m_file, full_path.CStr(), key, key_length, serialized);
}
#endif // CRYPTO_ENCRYPTED_FILE_SUPPORT

OP_STATUS
OpFile::ConstructPath(OpString& result, const uni_char* path, OpFileFolder folder)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_CONSTRUCTPATH);

	if (folder != OPFILE_ABSOLUTE_FOLDER && folder != OPFILE_SERIALIZED_FOLDER)
	{
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(folder, result));
	}

	return result.Append(path);
}

OpFileType
OpFile::Type() const
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_TYPE);
	return OPFILE; 
}

BOOL
OpFile::IsWritable() const
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_ISWRITABLE);
	return m_file && m_file->IsWritable(); 
}

OP_STATUS
OpFile::Open(int mode)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_OPEN);
	return m_file ? m_file->Open(mode) : OpStatus::ERR; 
}

BOOL
OpFile::IsOpen() const
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_ISOPEN);
	return m_file && m_file->IsOpen();
}

OP_STATUS
OpFile::Close()
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_CLOSE);
	return m_file ? m_file->Close() : OpStatus::OK; 
}

BOOL
OpFile::Eof() const
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_EOF);
	return m_file->Eof(); 
}

OP_STATUS
OpFile::Exists(BOOL& exists) const
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_EXISTS);

	if (m_file)
	{
		return m_file->Exists(&exists);
	}

	exists = FALSE;
	return OpStatus::OK;
}

BOOL OpFile::ExistsL() const
{
	BOOL exists;
	LEAVE_IF_ERROR(Exists(exists));
	return exists;
}

BOOL OpFile::ExistsL(const OpStringC& path)
{
	ANCHORD(OpFile, file);
	LEAVE_IF_ERROR(file.Construct(path));
	return file.ExistsL();
}

OP_STATUS
OpFile::GetFilePos(OpFileLength& pos) const
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_GETFILEPOS);
	return m_file->GetFilePos(&pos);
}

OP_STATUS
OpFile::SetFilePos(OpFileLength pos, OpSeekMode seek_mode)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_SETFILEPOS);
	return m_file->SetFilePos(pos, seek_mode);
}

OP_STATUS
OpFile::GetFileLength(OpFileLength& len) const
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_GETFILELENGTH);
	return m_file->GetFileLength(&len);
}

OP_STATUS
OpFile::SetFileLength(OpFileLength len)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_SETFILELENGTH);
	return m_file->SetFileLength(len);
}

OP_STATUS
OpFile::Write(const void* data, OpFileLength len)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_WRITE);
	return m_file->Write(data, len);
}

OP_STATUS
OpFile::Read(void* data, OpFileLength len, OpFileLength* bytes_read)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_READ);
	return m_file->Read(data, len, bytes_read);
}

OP_STATUS
OpFile::ReadLine(OpString8& str)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_READLINE);

	char* data;
	RETURN_IF_ERROR(m_file->ReadLine(&data));
	str.TakeOver(data);
	return OpStatus::OK;
}

OpFileDescriptor*
OpFile::CreateCopy() const
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_CREATECOPY);

	OpFile* result = OP_NEW(OpFile, ());
	if (result && m_file)
	{
		result->m_file = m_file->CreateCopy();
		if (result->m_file == NULL)
		{
			OP_DELETE(result);
			return NULL;
		}
	}

	return result;
}

OP_STATUS
OpFile::SafeClose()
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_SAFECLOSE);
	return m_file->SafeClose();
}

OP_STATUS
OpFile::SafeReplace(OpFile* source)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_SAFEREPLACE);
	return source ? m_file->SafeReplace(source->m_file) : OpStatus::ERR_NULL_POINTER;
}

OP_STATUS
OpFile::Copy(const OpFile* copy)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_COPY);

	OpLowLevelFile* new_low = NULL;
	if (copy->m_file)
	{
		new_low = copy->m_file->CreateCopy();
		if (!new_low)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	OP_DELETE(m_file);
	m_file = new_low;

	return OpStatus::OK;
}

OP_STATUS
OpFile::Delete(BOOL recursive)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_DELETE);

	BOOL exists;
	RETURN_IF_ERROR(Exists(exists));
	if (!exists)
		return OpStatus::OK;

	OpFileInfo::Mode mode;
	RETURN_IF_ERROR(GetMode(mode));

#ifdef DIRECTORY_SEARCH_SUPPORT
	if (mode == OpFileInfo::DIRECTORY && recursive)
	{
		OpFolderLister* lister = GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*"), GetFullPath());
		if (!lister)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS err = OpStatus::OK;
		while (lister->Next())
		{
			const uni_char* name = lister->GetFileName();
			if (uni_str_eq(name, ".") || uni_str_eq(name, ".."))
				continue;

			const uni_char* path = lister->GetFullPath();
			OpFile file;
			if (OpStatus::IsError(err = file.Construct(path)))
				break;
			if (OpStatus::IsError(err = file.Delete(recursive)))
				break;
		}
		OP_DELETE(lister);

		if (OpStatus::IsError(err))
			return err;
	}
#endif // DIRECTORY_SEARCH_SUPPORT

#ifdef UTIL_ZIP_CACHE
	OpZip* zip;
	ZipCache *zcache = g_opera->util_module.m_zipcache;
	if (zcache && OpStatus::IsSuccess(zcache->GetData(GetFullPath(), &zip)))
		zcache->FlushIfUnused(zip);
#endif

	return m_file->Delete();
}

OP_STATUS
OpFile::CopyContents(OpFile* file, BOOL fail_if_exists)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_COPYCONTENTS);

	if (fail_if_exists)
	{
		BOOL exists;
		RETURN_IF_ERROR(m_file->Exists(&exists));
		if (exists)
		{
			return OpStatus::ERR;
		}
	}

	return m_file->CopyContents(file->m_file);
}

#ifdef DIRECTORY_SEARCH_SUPPORT

OpFolderLister*
OpFile::GetFolderLister(OpFileFolder folder, const uni_char* pattern, const uni_char* path)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_GETFOLDERLIST);

	OpString full_path;
	if (OpStatus::IsError(ConstructPath(full_path, path, folder))) return NULL;

	OpFolderLister* lister;
	OP_STATUS rc;
#ifdef ZIPFILE_DIRECT_ACCESS_SUPPORT
	if (OpStatus::IsError(OpZipFolderLister::Create(&lister))) return NULL;
	rc = lister->Construct(full_path.CStr(), pattern);
	if (OpStatus::IsError(rc))
#endif
	{
#ifdef ZIPFILE_DIRECT_ACCESS_SUPPORT
		// Probably not in a zip file, then
		OP_DELETE(lister);
#endif
		if (OpStatus::IsError(OpFolderLister::Create(&lister))) return NULL;
		rc = lister->Construct(full_path.CStr(), pattern);
	}
	if (OpStatus::IsError(rc))
	{
		OP_DELETE(lister);
		return NULL;
	}

	return lister;
}

#endif // DIRECTORY_SEARCH_SUPPORT

const uni_char* 
OpFile::GetFullPath() const 
{ 
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_GETFULLPATH);

	return m_file ? m_file->GetFullPath() : NULL; 
}

const uni_char* 
OpFile::GetSerializedName() const { 
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_GETSERIALIZEDNAME);

	return m_file ? m_file->GetSerializedName() : NULL; 
}

OP_STATUS
OpFile::GetLocalizedPath(OpString *localized_path) const
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_GETLOCALIZEDPATH);

	if (OpStatus::IsError(m_file->GetLocalizedPath(localized_path)))
		return localized_path->Set(GetFullPath());
	return OpStatus::OK;
}

OP_STATUS
OpFile::MakeDirectory()
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_MAKEDIRECTORY);
	return m_file->MakeDirectory();
}

const uni_char*
OpFile::GetName() const
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_GETNAME);

	BOOL first = TRUE;
	const uni_char* p = GetFullPath();
	if (!p)
		return NULL;

	uni_char c = *p;
	const uni_char* d = p++;
	while ('\0' != c)									// find last slash or backslash...
	{
		if(first && ':' == c)							// take u:/dev/con:/ into account
		{
			d = p;
			first = FALSE;
		}
		else if(PATHSEPCHAR == c)
		{
			first = FALSE;
			d = p;
		}
		c = *p++;
	}
	return(d);
}

OP_STATUS
OpFile::GetDirectory(OpString &directory)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_GETDIRECTORY);

	const uni_char* path = GetFullPath();
	const uni_char* name = GetName();

	if (name && name != path)
	{
		return directory.Set(path, name-path);
	}
	else if (path)
	{
		return directory.Set(path);
	}

	directory.Empty();
	return OpStatus::OK;
}

OP_STATUS
OpFile::GetLastModified(time_t& value) const
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_GETLASTMODIFIED);

	OpFileInfo info;
	info.last_modified = 0;
	info.flags = OpFileInfo::LAST_MODIFIED;
	if (m_file)
	{
		RETURN_IF_ERROR(m_file->GetFileInfo(&info));
	}

	value = info.last_modified;
	return OpStatus::OK;
}

OpFileInfo::Mode
OpFile::GetMode() const
{	
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_GETMODE1);

	OpFileInfo info;
	info.mode = OpFileInfo::FILE;
	info.flags = OpFileInfo::MODE;
	if (m_file)
	{
		m_file->GetFileInfo(&info);
	}

	return info.mode;
}

OP_STATUS
OpFile::GetMode(OpFileInfo::Mode &mode) const
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_GETMODE2);

	if (m_file)
	{
		OpFileInfo info;
		info.mode = OpFileInfo::FILE;
		info.flags = OpFileInfo::MODE;
		RETURN_IF_ERROR(m_file->GetFileInfo(&info));
		mode = info.mode;
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS
OpFile::Flush()
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_FLUSH);
	OP_ASSERT(m_file && "Whoever just called OpFile::Flush is a naughty naughty boy!");
	return m_file ? m_file->Flush() : OpStatus::ERR;
}

#define OPFILE_PRINT_BUFFER_SIZE 2048

OP_STATUS
OpFile::Print(const char* format, ...)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_PRINT);

	OP_STATUS result = OpStatus::OK;

	if (m_file)
	{
		va_list arglist;
		va_start(arglist, format);

		char buffer[OPFILE_PRINT_BUFFER_SIZE]; /* ARRAY OK 2009-04-24 johanh */
		int count = op_vsnprintf(buffer, OPFILE_PRINT_BUFFER_SIZE-1, format, arglist);

		if (count > 0 && count < OPFILE_PRINT_BUFFER_SIZE)
		{
			buffer[count] = 0;
			result = m_file->Write(buffer, count);
		}
		else
		{
			result = OpStatus::ERR;
		}

		va_end(arglist);
	}

	return result;
}

#ifdef UTIL_ASYNC_FILE_OP
void OpFile::SetListener (OpFileListener *listener)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_SETLISTENER);

	m_listener = listener;
	if (m_file)
		m_file->SetListener(m_listener ? this : NULL);
}

OP_STATUS OpFile::ReadAsync(void* data, OpFileLength len, OpFileLength abs_pos /* = FILE_LENGTH_NONE */)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_READASYNC);

	if (!m_file)
		return OpStatus::ERR;
	OP_ASSERT(m_listener);
	if (!m_listener)
		return OpStatus::ERR_NULL_POINTER;
	return m_file->ReadAsync(data, len, abs_pos);
}

OP_STATUS OpFile::WriteAsync(const void* data, OpFileLength len, OpFileLength abs_pos /* = FILE_LENGTH_NONE */)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_WRITEASYNC);

	if (!m_file)
		return OpStatus::ERR;
	OP_ASSERT(m_listener);
	if (!m_listener)
		return OpStatus::ERR_NULL_POINTER;
	return m_file->WriteAsync(data, len, abs_pos);
}

OP_STATUS OpFile::DeleteAsync()
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_DELETEASYNC);

	if (!m_file)
		return OpStatus::ERR;
	OP_ASSERT(m_listener);
	if (!m_listener)
		return OpStatus::ERR_NULL_POINTER;
	return m_file->DeleteAsync();
}

OP_STATUS OpFile::FlushAsync()
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_FLUSHASYNC);

	if (!m_file)
		return OpStatus::ERR;
	OP_ASSERT(m_listener);
	if (!m_listener)
		return OpStatus::ERR_NULL_POINTER;
	return m_file->FlushAsync();
}

OP_STATUS
OpFile::Sync()
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_SYNC);
	return m_file ? m_file->Sync() : OpStatus::OK;
}

BOOL
OpFile::IsAsyncInProgress()
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_ISASYNCINPROGRESS);
	return m_file ? m_file->IsAsyncInProgress() : FALSE;
}

// Implementation of OpLowLevelFileListener
void OpFile::OnDataRead(OpLowLevelFile* lowlevel_file, OP_STATUS result, OpFileLength len)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_ONDATAREAD);

	OP_ASSERT(m_listener);
	OP_ASSERT(lowlevel_file == m_file);
	m_listener->OnDataRead(this, result, len);
}

void OpFile::OnDataWritten(OpLowLevelFile* lowlevel_file, OP_STATUS result, OpFileLength len)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_ONDATAWRITTEN);

	OP_ASSERT(m_listener);
	OP_ASSERT(lowlevel_file == m_file);
	m_listener->OnDataWritten(this, result, len);
}

void OpFile::OnDeleted(OpLowLevelFile* lowlevel_file, OP_STATUS result)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_ONDELETE);

	OP_ASSERT(m_listener);
	OP_ASSERT(lowlevel_file == m_file);
	m_listener->OnDeleted(this, result);
}

void OpFile::OnFlushed(OpLowLevelFile* lowlevel_file, OP_STATUS result)
{
	OP_PROBE2(OP_PROBE_UTIL_OP_FILE_ONFLUSHED);

	OP_ASSERT(m_listener);
	OP_ASSERT(lowlevel_file == m_file);
	m_listener->OnFlushed(this, result);
}
#endif // UTIL_ASYNC_FILE_OP

