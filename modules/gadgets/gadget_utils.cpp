/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef GADGET_SUPPORT
#include "modules/gadgets/gadget_utils.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/util/tempbuf.h"
#include "modules/url/url_api.h"
#include "modules/url/url2.h"

#ifdef OPERA_CONSOLE
# include "modules/console/opconsoleengine.h"
#endif // OPERA_CONSOLE

#ifdef OPERA_CONSOLE
/* static */ void
OpGadgetUtils::ReportErrorToConsole(const uni_char* url, const uni_char* fmt, ...)
{
	// Fetch any variable length parameters
	va_list args;
	va_start(args, fmt);
	ReportToConsole(url, OpConsoleEngine::Error, fmt, args);
	va_end(args);
}

/* static */ void
OpGadgetUtils::ReportToConsole(const uni_char* url, OpConsoleEngine::Severity severity, const uni_char* fmt, ...)
{
	// Fetch any variable length parameters
	va_list args;
	va_start(args, fmt);
	ReportToConsole(url, severity, fmt, args);
	va_end(args);
}

/* static */ void
OpGadgetUtils::ReportToConsole(const uni_char* url, OpConsoleEngine::Severity severity, const uni_char* fmt, va_list args)
{
	if (!g_console->IsLogging())
		return;

	OpConsoleEngine::Message cmessage(OpConsoleEngine::Gadget, severity);

	cmessage.message.AppendVFormat(fmt, args);

	if (url && *url)
	{
		// The passed "url" string here is often a reference to a local file;
		// convert it to a string by creating a URL object and getting the
		// parsed URL back.
		URL msgurl = g_url_api->GetURL(url);
		OpStatus::Ignore(msgurl.GetAttribute(URL::KUniName, cmessage.url));
	}

	TRAPD(rc, g_console->PostMessageL(&cmessage));
	OpStatus::Ignore(rc);
}

#endif // OPERA_CONSOLE

BOOL
OpGadgetUtils::IsAttrStringTrue(const uni_char* attr_str, BOOL dval)
{
	if (!attr_str)
		return dval;
	else if (uni_strcmp(attr_str, "yes") == 0)
		return TRUE;
	else if (uni_strcmp(attr_str, "true") == 0)
		return TRUE;
	else
		return FALSE;
}

/* static */ UINT32
OpGadgetUtils::AttrStringToI(const uni_char* attr_str, UINT32 dval)
{
	UINT32 result;
	OP_STATUS success;
	AttrStringToI(attr_str, result, success);
	if (OpStatus::IsSuccess(success))
		return result;
	else
		return dval;
}

/* static */ void
OpGadgetUtils::AttrStringToI(const uni_char* attr_str, UINT32& result, OP_STATUS &success)
{
	success = OpStatus::ERR;

	if (!attr_str || !*attr_str)
		return;

	// Skip past leading whitespace
	while (IsSpace(*attr_str))
	{
		attr_str++;
		if (*attr_str == 0)
			return;
	}
	
	BOOL found_numbers = FALSE;
	UINT32 val = 0;
	while (*attr_str)
	{
		if (*attr_str >= '0' && *attr_str <= '9')
		{
			found_numbers = TRUE;
			val *= 10;
			val += *attr_str - 0x30;
		}
		else
			break;

		attr_str++;
	}

	if (!found_numbers)
		return;

	result = val;
	success = OpStatus::OK;
}

#if PATHSEPCHAR != '/'
/* static */ void
OpGadgetUtils::ChangeToLocalPathSeparators(uni_char *pathstr)
{
	if (!pathstr)
		return;

	while (*pathstr)
	{
		if ('/' == *pathstr)
			*pathstr = PATHSEPCHAR;
		else if (PATHSEPCHAR == *pathstr)
			*pathstr = '/';

		++ pathstr;
	}
}
#endif

/* static */ OP_STATUS
OpGadgetUtils::AppendToList(OpString &string, const char *str)
{
	if (string.Length() > 0)
		RETURN_IF_ERROR(string.Append(" "));

	return string.Append(str);
}

/* static */ BOOL
OpGadgetUtils::IsSpace(uni_char c)
{
	switch (c)
	{
	case 0x0009:
	case 0x000a:
	case 0x000b:
	case 0x000c:
	case 0x000d:
	case 0x0020:
		return TRUE;
	default:
		return FALSE;
	}
}

/* static */ BOOL
OpGadgetUtils::IsUniWhiteSpace(uni_char c)
{
	switch (c)
	{
	case 0x0009:
	case 0x000a:
	case 0x000b:
	case 0x000c:
	case 0x000d:
	case 0x0020:
	case 0x0085:
	case 0x00a0:
	case 0x1680:
	case 0x180e:
	case 0x2000:
	case 0x2001:
	case 0x2002:
	case 0x2003:
	case 0x2004:
	case 0x2005:
	case 0x2006:
	case 0x2007:
	case 0x2008:
	case 0x2009:
	case 0x200a:
	case 0x2028:
	case 0x2029:
	case 0x202f:
	case 0x205f:
	case 0x3000:
		return TRUE;
	default:
		return FALSE;
	}
}

/* static */ OP_STATUS
OpGadgetUtils::NormalizeText(const uni_char *text, TempBuffer *result, uni_char bidi_marker)
{
	if (!text)
		return OpStatus::OK;

	if (bidi_marker)
		RETURN_IF_ERROR(result->Append(bidi_marker));

	BOOL last_char_was_whitespace = FALSE;
	BOOL first = TRUE;
	while (*text)
	{
		if (IsUniWhiteSpace(*text))
		{
			last_char_was_whitespace = TRUE;
			text++;
		}
		else
		{
			if (last_char_was_whitespace && *text != 0x0000 && !first)
				RETURN_IF_ERROR(result->Append(0x0020));
			RETURN_IF_ERROR(result->Append(*text));
			last_char_was_whitespace = FALSE;
			first = FALSE;
			text++;
		}
	}

	if (bidi_marker)
		RETURN_IF_ERROR(result->Append(0x202c)); // bidi PDE

	return OpStatus::OK;
}

/* static */ OP_STATUS
OpGadgetUtils::GetTextNormalized(XMLFragment *f, TempBuffer *result, uni_char bidi_marker /* = 0 */)
{
	OP_ASSERT(f);
	OP_ASSERT(result);

	TempBuffer temp;

	RETURN_IF_ERROR(f->GetAllText(temp));

	if (temp.GetCapacity() == 0)
		return OpStatus::OK;

	const uni_char *s = temp.GetStorage();
	return NormalizeText(s, result, bidi_marker);
}

/* static */ OP_STATUS
BufferContentProvider::Make(BufferContentProvider*& obj, char* data, UINT32 numbytes)
{
	obj = OP_NEW(BufferContentProvider, (data, numbytes));
	if (!obj)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

/* static */ BOOL
OpGadgetUtils::IsValidIRI(const uni_char* str, BOOL allow_wildcard)
{
	return str && (                                                         // If str == then of course it is invalid.
		(allow_wildcard && str[0] == '*' && str[1] == 0) ||                 // Either it is "*" ...
		(uni_strchr(str, ':') > str && g_url_api->GetURL(str).IsValid())    // ... or it is an URI.
		);
}

#endif // GADGET_SUPPORT
