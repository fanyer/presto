/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef GADGET_UTILS_H
#define GADGET_UTILS_H
#ifdef GADGET_SUPPORT

#include "modules/img/image.h"

#ifdef OPERA_CONSOLE
# include "modules/console/opconsoleengine.h"
# define GADGETS_REPORT_TO_CONSOLE(a) do { OpGadgetUtils::ReportToConsole a; } while(0)
# define GADGETS_REPORT_ERROR_TO_CONSOLE(a) do { OpGadgetUtils::ReportErrorToConsole a; } while(0)
#else
# define GADGETS_REPORT_TO_CONSOLE(a) do {} while (0)
# define GADGETS_REPORT_ERROR_TO_CONSOLE(a) do {} while(0)
#endif // OPERA_CONSOLE

//////////////////////////////////////////////////////////////////////////
// OpGadgetStatus
//////////////////////////////////////////////////////////////////////////

class OpGadgetStatus : public OpStatus
{
public:
	enum GadErrEnum
	{
		ERR_INVALID_ZIP	                 = USER_ERROR + 1,  ///< The zip archive is invalid.
		ERR_WRONG_WIDGET_FORMAT          = USER_ERROR + 2,  ///< The widget configuration document is invalid.
		ERR_WRONG_NAMESPACE              = USER_ERROR + 3,  ///< The gadget's config file is not using the correct namespace. (e.g. extensions need to be w3c widgets).
		ERR_CONFIGURATION_FILE_NOT_FOUND = USER_ERROR + 4,  ///< The widget configuration document was not found.
		ERR_START_FILE_NOT_FOUND         = USER_ERROR + 5,  ///< The widget start file was not found.
		ERR_CONTENT_TYPE_NOT_SUPPORTED   = USER_ERROR + 6,  ///< The widget start file type is not supported by Opera.
		ERR_FEATURE_NOT_SUPPORTED        = USER_ERROR + 7,  ///< The feature which was specified as required is not supported.
		ERR_GADGET_TYPE_NOT_SUPPORTED    = USER_ERROR + 8,  ///< The widget type requested is not supported by this build of opera.
		ERR_BAD_PARAM                    = USER_ERROR + 9,  ///< A feature contains a badly declared parameter.

		OK_UNKNOWN_FEATURE			= USER_SUCCESS + 1,     ///< The feature which was specified as not required is not supported.
		OK_SIGNATURE_VERIFICATION_IN_PROGRESS	= USER_SUCCESS + 2    ///< The widget signature verification hasn't finished yet.
	};
};

typedef OP_STATUS OP_GADGET_STATUS;

//////////////////////////////////////////////////////////////////////////
// OpGadgetUtils
//////////////////////////////////////////////////////////////////////////

class OpGadgetUtils
{
public:
#ifdef OPERA_CONSOLE
	static void ReportErrorToConsole(const uni_char* url, const uni_char* fmt, ...);
	static void ReportToConsole(const uni_char* url, OpConsoleEngine::Severity severity, const uni_char* fmt, ...);
	static void ReportToConsole(const uni_char* url, OpConsoleEngine::Severity severity, const uni_char* fmt, va_list args);
#endif // OPERA_CONSOLE
	static BOOL IsAttrStringTrue(const uni_char* attr_str, BOOL dval = FALSE);
	static UINT32 AttrStringToI(const uni_char* attr_str, UINT32 dval = 0);
	static void AttrStringToI(const uni_char* attr_str, UINT32& result, OP_GADGET_STATUS &success);
#if PATHSEPCHAR != '/'
	/** Convert a slash separated path to be delimited by the local system's
	  * path separator. Any occurances of the local path seperator is replaced
	  * by a slash.
	  * @param pathstr Path to convert.
	  */
	static void ChangeToLocalPathSeparators(uni_char *pathstr);
#endif
	static OP_GADGET_STATUS AppendToList(OpString &string, const char *str);
	static OP_STATUS NormalizeText(const uni_char *text, TempBuffer *result, uni_char bidi_marker = 0);
	static OP_STATUS GetTextNormalized(class XMLFragment *f, TempBuffer *result, uni_char bidi_marker = 0);

	static BOOL IsSpace(uni_char c);
	static BOOL IsUniWhiteSpace(uni_char c);
	static BOOL IsValidIRI(const uni_char* str, BOOL allow_wildcard = FALSE);
};

//////////////////////////////////////////////////////////////////////////
// BufferContentProvider (Helper class for OpGadgetClass)
//////////////////////////////////////////////////////////////////////////

class BufferContentProvider : public ImageContentProvider
{
public:
	static OP_STATUS Make(BufferContentProvider*& obj, char* data, UINT32 numbytes);
	~BufferContentProvider() { OP_DELETEA(data); }

	OP_STATUS GetData(const char*& data, INT32& data_len, BOOL& more) { data = this->data; data_len = numbytes; more = FALSE; return OpStatus::OK; }
	OP_STATUS Grow() { return OpStatus::ERR; }
	void  ConsumeData(INT32 len) { loaded = TRUE; }

	void Reset() {}
	void Rewind() {}
	BOOL IsLoaded() { return TRUE; }
	INT32 ContentType() { return content_type; }
	void SetContentType(INT32 type) { content_type = type; }
	INT32 ContentSize() { return numbytes; }
	BOOL IsEqual(ImageContentProvider* content_provider) { return FALSE; }

private:
	BufferContentProvider(char* data, UINT32 numbytes) : data(data), numbytes(numbytes), content_type(0), loaded(FALSE) {}
	const char* data;
	UINT32 numbytes;
	INT32 content_type;
	BOOL loaded;
};

//////////////////////////////////////////////////////////////////////////
// OpPersistentStorageListener
//////////////////////////////////////////////////////////////////////////

class OpPersistentStorageListener
{
public:
	virtual ~OpPersistentStorageListener() { }

	virtual OP_STATUS		SetPersistentData(const uni_char* section, const uni_char* key, const uni_char* value) = 0;
	virtual const uni_char*	GetPersistentData(const uni_char* section, const uni_char* key) = 0;
	virtual OP_STATUS		DeletePersistentData(const uni_char* section, const uni_char* key) = 0;
	virtual OP_STATUS		GetPersistentDataItem(UINT32 idx, OpString& key, OpString& data) = 0;
	virtual OP_STATUS		GetStoragePath(OpString& path) = 0;
	virtual OP_STATUS		GetApplicationPath(OpString& path) { return OpStatus::ERR; }
};

#endif // GADGET_SUPPORT
#endif // !GADGET_UTILS_H
