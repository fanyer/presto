/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Bartek Przybylski
 */
#include "modules/hardcore/base/op_types.h"
#include "modules/util/opstring.h"
#include "modules/util/adt/opvector.h"
#include "platforms/posix/posix_module.h"
#include "modules/pi/system/OpFolderLister.h"
#include "modules/pi/system/OpLowLevelFile.h"
#include <pwd.h>
#define BOOL NSBOOL
#import <Cocoa/Cocoa.h>
#undef BOOL

#define RETURN_IF_ERROR_CLEANUP(x, y) do { OP_STATUS err = x; if (OpStatus::IsError(err)) { OP_DELETE(y); return err; }} while(0)

static OpAutoVector<OpString>* exceptions_vector = NULL;
static OpAutoVector<OpString>* temporary_exceptions_vector = NULL;

enum ExceptionType {
	PERMANENT,
	TEMPORARY,
	NOT_AN_EXCEPTION
};

OP_STATUS StripFileProtocol(const OpString* url, OpString** path)
{
	*path = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(*path);
	RETURN_IF_ERROR_CLEANUP((*path)->Set(*url), *path);
	if (url->CompareI("file://localhost", 16) != 0)
	{
		if ((*path)->Compare("//", 2) == 0)
			(*path)->Set((*path)->SubString(1));
		return OpStatus::OK;
	}

	RETURN_IF_ERROR_CLEANUP((*path)->ReplaceAll(UNI_L("file://localhost"), NULL, 1), *path);

	return OpStatus::OK;
}

OP_STATUS AddTemporaryException(const OpString* exception)
{
	if (temporary_exceptions_vector == NULL)
	{
		temporary_exceptions_vector = OP_NEW(OpAutoVector<OpString>, ());
		RETURN_OOM_IF_NULL(temporary_exceptions_vector);
	}
	OpString* path;
	RETURN_IF_ERROR(StripFileProtocol(exception, &path));

	// dont duplicate entries
	for (UINT32 i = 0; i < temporary_exceptions_vector->GetCount(); ++i)
		if (uni_strcmp(path->CStr(), temporary_exceptions_vector->Get(i)->CStr()) == 0)
			return OpStatus::OK;

	OpString* str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR_CLEANUP(str->Append(path->CStr()), str);
	RETURN_IF_ERROR_CLEANUP(temporary_exceptions_vector->Add(str), str);
	OP_DELETE(path);
	return OpStatus::OK;
}

ExceptionType IsException(const uni_char* path)
{
	for (UINT32 i = 0; i < exceptions_vector->GetCount(); ++i)
		if (uni_strnicmp(path, exceptions_vector->Get(i)->CStr(), exceptions_vector->Get(i)->Length()) == 0)
			return PERMANENT;
	for (UINT32 i = 0; i < temporary_exceptions_vector->GetCount(); ++i)
		if (uni_strnicmp(path, temporary_exceptions_vector->Get(i)->CStr(), temporary_exceptions_vector->Get(i)->Length()) == 0)
			return TEMPORARY;
	// hack for extensions, allow in addon patch resolving
	if (path[0] != '/') return TEMPORARY;
	return NOT_AN_EXCEPTION;
}

OP_STATUS InitiateExceptionsVector()
{
	temporary_exceptions_vector = OP_NEW(OpAutoVector<OpString>, ());
	RETURN_OOM_IF_NULL(temporary_exceptions_vector);

	exceptions_vector = OP_NEW(OpAutoVector<OpString>, ());
	RETURN_OOM_IF_NULL(exceptions_vector);

	// sandbox
	OpString* str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR_CLEANUP(str->SetFromUTF8([NSHomeDirectory() UTF8String]), str);
	RETURN_IF_ERROR_CLEANUP(exceptions_vector->Add(str), str);

	// main library directory
	str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR_CLEANUP(str->SetFromUTF8("/Library"), str);
	RETURN_IF_ERROR_CLEANUP(exceptions_vector->Add(str), str);

	// main library directory
	str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR_CLEANUP(str->SetFromUTF8("/System"), str);
	RETURN_IF_ERROR_CLEANUP(exceptions_vector->Add(str), str);

	// own bundle
	str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR_CLEANUP(str->SetFromUTF8([[[NSBundle mainBundle] bundlePath] UTF8String]), str);
	RETURN_IF_ERROR_CLEANUP(exceptions_vector->Add(str), str);

	// downloads dir
	const char* home_path = getpwuid(getuid())->pw_dir;
	str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR_CLEANUP(str->SetFromUTF8(home_path), str);
	RETURN_IF_ERROR_CLEANUP(str->Append("/Downloads"), str);
	RETURN_IF_ERROR_CLEANUP(exceptions_vector->Add(str), str);

	// photos dir
	str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR_CLEANUP(str->SetFromUTF8(home_path), str);
	RETURN_IF_ERROR_CLEANUP(str->Append("/Pictures"), str);
	RETURN_IF_ERROR_CLEANUP(exceptions_vector->Add(str), str);

	// movies dir
	str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR_CLEANUP(str->SetFromUTF8(home_path), str);
	RETURN_IF_ERROR_CLEANUP(str->Append("/Movies"), str);
	RETURN_IF_ERROR_CLEANUP(exceptions_vector->Add(str), str);

	// widgets
	str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR_CLEANUP(str->Set("widget:"), str);
	RETURN_IF_ERROR_CLEANUP(exceptions_vector->Add(str), str);

	// music dir
	str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR_CLEANUP(str->SetFromUTF8(home_path), str);
	RETURN_IF_ERROR_CLEANUP(str->Append("/Music"), str);
	RETURN_IF_ERROR_CLEANUP(exceptions_vector->Add(str), str);

	// relative and special paths needed for startup
	str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR_CLEANUP(str->SetFromUTF8("{"), str);
	RETURN_IF_ERROR_CLEANUP(exceptions_vector->Add(str), str);

	str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR_CLEANUP(str->SetFromUTF8("../"), str);
	RETURN_IF_ERROR_CLEANUP(exceptions_vector->Add(str), str);

	str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR_CLEANUP(str->SetFromUTF8("./"), str);
	RETURN_IF_ERROR_CLEANUP(exceptions_vector->Add(str), str);

	return OpStatus::OK;
}

class SandboxedFolderLister : public OpFolderLister
{
public:
	SandboxedFolderLister()
		: m_exception_type(NOT_AN_EXCEPTION),
		  m_folder_lister(NULL)
	{ }

	virtual ~SandboxedFolderLister(){
		delete m_folder_lister;
	}

	OP_STATUS Construct(const uni_char* path, const uni_char* pattern)
	{
		RETURN_IF_ERROR(PosixModule::CreateRawLister(&m_folder_lister));

		m_exception_type = ::IsException(path);

		if (IsException())
			return m_folder_lister->Construct(path, pattern);
		return OpStatus::OK;
	}

	BOOL Next()
	{
		return IsException() ? m_folder_lister->Next() : FALSE;
	}

	const uni_char* GetFileName() const
	{
		return IsException() ? m_folder_lister->GetFileName() : NULL;
	}

	const uni_char* GetFullPath() const
	{
		return IsException() ? m_folder_lister->GetFullPath() : NULL;
	}

	OpFileLength GetFileLength() const
	{
		return IsException() ? m_folder_lister->GetFileLength() : 0;
	}

	BOOL IsFolder() const
	{
		return IsException() ? m_folder_lister->IsFolder() : FALSE;
	}

private:
	inline BOOL IsException() const
	{
		return m_exception_type != NOT_AN_EXCEPTION;
	}

	ExceptionType m_exception_type;
	OpFolderLister* m_folder_lister;
};

class SandboxedFile : public OpLowLevelFile
{
public:
	virtual ~SandboxedFile(){}

	virtual const uni_char* GetFullPath() const { return NULL; }
	virtual const uni_char* GetSerializedName() const { return NULL; }
	virtual OpLowLevelFile* CreateTempFile(const uni_char* prefix) { return NULL; }
	virtual OpLowLevelFile* CreateCopy() { return NULL; }

	virtual OP_STATUS GetFileInfo(OpFileInfo* info) { return OpStatus::ERR; }
	virtual OP_STATUS GetFileLength(OpFileLength* len) const { return OpStatus::ERR_NO_ACCESS; }
	virtual BOOL IsWritable() const { return FALSE; }
	virtual OP_STATUS Exists(BOOL* exists) const { return OpStatus::ERR_NO_ACCESS; }
#ifdef SUPPORT_OPFILEINFO_CHANGE
	virtual OP_STATUS ChangeFileInfo(const OpFileInfoChange* changes) { return OpStatus::ERR; }
#endif

	virtual OP_STATUS GetLocalizedPath(OpString *localized_path) const { return OpStatus::ERR; }

	virtual OP_STATUS CopyContents(const OpLowLevelFile* source) { return OpStatus::ERR_NO_ACCESS; }
	virtual OP_STATUS SafeReplace(OpLowLevelFile* source) { return OpStatus::ERR_NO_ACCESS; }
	virtual OP_STATUS Delete() { return OpStatus::ERR; }
	virtual OP_STATUS MakeDirectory() { return OpStatus::ERR_NO_ACCESS; }

	virtual BOOL IsOpen() const { return FALSE; }
	virtual OP_STATUS Open(int mode) { return OpStatus::ERR_NO_ACCESS; }
	virtual OP_STATUS Close() { return OpStatus::OK; }
	virtual OP_STATUS SafeClose() { return OpStatus::OK; }

	virtual BOOL Eof() const { return TRUE; }
	virtual OP_STATUS Read(void* data, OpFileLength len, OpFileLength* bytes_read) { return OpStatus::ERR_NO_ACCESS; }
	virtual OP_STATUS ReadLine(char** data) { return OpStatus::ERR; }
	virtual OP_STATUS GetFilePos(OpFileLength*) const { return OpStatus::ERR; }
	virtual OP_STATUS SetFilePos(OpFileLength, OpSeekMode) { return OpStatus::ERR; }
	virtual OP_STATUS SetFileLength(OpFileLength len) { return OpStatus::ERR; }
	virtual OP_STATUS Write(const void* data, OpFileLength len) { return OpStatus::ERR; }
	virtual OP_STATUS Flush() { return  OpStatus::ERR; }

#ifdef PI_ASYNC_FILE_OP
	virtual void SetListener (OpLowLevelFileListener *listener) {}
	virtual OP_STATUS ReadAsync(void* data, OpFileLength len,
								OpFileLength abs_pos = FILE_LENGTH_NONE) { return OpStatus::ERR; }
	virtual OP_STATUS WriteAsync(const void* data, OpFileLength len,
								 OpFileLength abs_pos = FILE_LENGTH_NONE) { return OpStatus::ERR; }
	virtual OP_STATUS DeleteAsync() { return OpStatus::ERR; }
	virtual OP_STATUS FlushAsync() { return OpStatus::ERR; }
	virtual BOOL IsAsyncInProgress() { return FALSE; }
	virtual OP_STATUS Sync() { return OpStatus::OK; }
#endif // PI_ASYNC_FILE_OP
};

const char* GetContainerPath() {
	return [NSHomeDirectory() UTF8String];
}

BOOL IsSandboxed()
{
	static BOOL is_sandboxed = strcmp(getpwuid(getuid())->pw_dir, [NSHomeDirectory() UTF8String]) != 0;
	return is_sandboxed;
}

OP_STATUS OpFolderLister::Create(OpFolderLister** new_lister)
{
	if (!exceptions_vector) RETURN_IF_ERROR(::InitiateExceptionsVector());

	if (!IsSandboxed())
		return PosixModule::CreateRawLister(new_lister);

	*new_lister = OP_NEW(SandboxedFolderLister, ());
	RETURN_OOM_IF_NULL(*new_lister);
	return OpStatus::OK;
}

OP_STATUS OpLowLevelFile::Create(OpLowLevelFile** new_file, const uni_char* path, BOOL serialized)
{
	if (!exceptions_vector) RETURN_IF_ERROR(::InitiateExceptionsVector());

	if (!IsSandboxed())
		return PosixModule::CreateRawFile(new_file, path, serialized);

	OpString str;
	RETURN_VALUE_IF_ERROR(str.Append(path), NOT_AN_EXCEPTION);

	OpString *stripped_path;
	RETURN_IF_ERROR(StripFileProtocol(&str, &stripped_path));
	OP_STATUS ret_status = OpStatus::OK;;

	switch (IsException(stripped_path->CStr()))
	{
		case TEMPORARY:
		case PERMANENT:
			ret_status = PosixModule::CreateRawFile(new_file, path, serialized);
			break;
		case NOT_AN_EXCEPTION:
			*new_file = OP_NEW(SandboxedFile, ());
			if (!*new_file)
				ret_status = OpStatus::ERR_NO_MEMORY;
			break;
		default:
			ret_status = OpStatus::ERR;
	}
	OP_DELETE(stripped_path);
	return ret_status;
}

#undef RETURN_IF_ERROR_CLEANUP
