/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#include "modules/about/opgenerateddocument.h"
#include "modules/util/opstring.h"
#include "modules/util/htmlify.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/tempbuf.h"

OpGeneratedDocument::~OpGeneratedDocument()
{
}

OP_STATUS OpGeneratedDocument::SetupURL(unsigned int options)
{
	bool allow_disk_cache = !(options & NO_CACHE);
	bool add_bom = !(options & SKIP_BOM);

	// Set up MIME type and required attributes
	RETURN_IF_ERROR(m_url.SetAttribute(URL::KMIME_ForceContentType, "text/html;charset=utf-16"));
	RETURN_IF_ERROR(m_url.SetAttribute(URL::KCachePolicy_NoStore, !allow_disk_cache));

	if (m_frame_options_deny)
		RETURN_IF_ERROR(m_url.SetAttribute(URL::KHTTPFrameOptions, "deny"));

	// Write a BOM, if not omitted
	return  add_bom ? m_url.WriteDocumentData(URL::KNormal, UNI_L("\xFEFF")) : OpStatus::OK;
}

OP_STATUS OpGeneratedDocument::OpenDocument(
	Str::LocaleString title
#ifdef _LOCALHOST_SUPPORT_
	, PrefsCollectionFiles::filepref stylesheet
#endif
	, BOOL allow_disk_cache
	)
{
	OpString titlestring;
	if (title != Str::NOT_A_STRING)
	{
		RETURN_IF_ERROR(g_languageManager->GetString(title, titlestring));
	}

#ifdef _LOCALHOST_SUPPORT_
	OpString styleurl;
	if (stylesheet != PrefsCollectionFiles::DummyLastFilePref)
	{
		TRAP_AND_RETURN(rc, g_pcfiles->GetFileURLL(stylesheet, &styleurl));
	}

	return OpenDocument(titlestring.CStr(), styleurl.CStr(), allow_disk_cache);
#else
	return OpenDocument(titlestring.CStr(), NULL, allow_disk_cache);
#endif
}

OP_STATUS OpGeneratedDocument::OpenDocument(const uni_char *title, const uni_char *stylesheet, BOOL allow_disk_cache)
{
	// Initial setup
	RETURN_IF_ERROR(SetupURL(allow_disk_cache ? 0 : NO_CACHE));

	// Write document header
	const char *htmlns = NULL;
	OpString line;
	line.Reserve(512);
	switch (m_doctype)
	{
	default:
		OP_ASSERT(!"Invalid DOCTYPE");
	case HTML5:
		RETURN_IF_ERROR(line.Set("<!DOCTYPE HTML>\n"));
		break;

	case XHTML5:
		RETURN_IF_ERROR(line.Set(
			"<?xml version=\"1.0\" encoding=\"utf-16\"?>\n"
			"<!DOCTYPE HTML>\n"));
		break;

	case XHTMLMobile1_2:
		RETURN_IF_ERROR(line.Set(
			"<!DOCTYPE html PUBLIC \"-//WAPFORUM//DTD XHTML Mobile 1.2//EN\" "
			"\"http://www.openmobilealliance.org/tech/DTD/xhtml-mobile12.dtd\">"));
		break;
	}

	// HTML element, with DIR attribute
	m_rtl = g_languageManager->GetWritingDirection() == OpLanguageManager::RTL;
	RETURN_IF_ERROR(line.Append("<html dir=\""));
	RETURN_IF_ERROR(line.Append(m_rtl ? "rtl" : "ltr"));
	RETURN_IF_ERROR(line.Append("\""));

	if (htmlns)
	{
		OP_ASSERT(*htmlns == ' '); // Must start with space
		line.Append(htmlns);
	}
	RETURN_IF_ERROR(line.Append(">\n<head>\n"));
	if (title && *title)
	{
		RETURN_IF_ERROR(line.Append(" <title>"));
		RETURN_IF_ERROR(line.Append(title));
		RETURN_IF_ERROR(line.Append("</title>\n"));
	}
	if (stylesheet && *stylesheet)
	{
		RETURN_IF_ERROR(
			line.AppendFormat(UNI_L(" <link rel=\"stylesheet\" href=\"%s\" media=\"screen,projection,tv,handheld,print,speech\"%s>\n"),
				stylesheet, (IsXML() ? UNI_L("/") : UNI_L(""))));
	}
	RETURN_IF_ERROR(line.Append(" <meta name=\"viewport\" content=\"width=device-width\""));
	if (IsXML()) RETURN_IF_ERROR(line.Append("/"));
	RETURN_IF_ERROR(line.Append(">\n"));

	return m_url.WriteDocumentData(URL::KNormal, line);
}

OP_STATUS OpGeneratedDocument::OpenBody()
{
	return OpenBody(Str::NOT_A_STRING, NULL);
}

OP_STATUS OpGeneratedDocument::OpenBody(Str::LocaleString header, const uni_char *id)
{
	OpString line;
	line.Reserve(128);
	RETURN_IF_ERROR(line.Set("</head>\n<body"));
	if (id && *id)
	{
		RETURN_IF_ERROR(line.AppendFormat(UNI_L(" id=\"%s\""), id));
	}
	RETURN_IF_ERROR(line.Append(">"));
	if (header != Str::NOT_A_STRING)
	{
		RETURN_IF_ERROR(line.Append("\n<h1>"));
		RETURN_IF_ERROR(AppendLocaleString(&line, header));
		RETURN_IF_ERROR(line.Append("</h1>\n"));
	}
	return m_url.WriteDocumentData(URL::KNormal, line.CStr());
}

OP_STATUS OpGeneratedDocument::CloseDocument()
{
#ifdef LOCALE_CONTEXTS
	g_languageManager->SetContext(0);
#endif

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</body></html>")));
	return FinishURL();
}

OP_STATUS OpGeneratedDocument::FinishURL()
{
	m_url.WriteDocumentDataFinished();
	RETURN_IF_ERROR(m_url.SetAttribute(URL::KCacheType, URL_CACHE_TEMP));
	/* The setting of the "is-generated" flag to TRUE must happen after
	   WriteDocumentData() has written to m_url and a cache storage
	   object has been allocated. This is a suitable spot; see CORE-43302. */
	RETURN_IF_ERROR(m_url.SetAttribute(URL::KIsGeneratedByOpera, TRUE));
	return OpStatus::OK;
}

OP_STATUS OpGeneratedDocument::Heading(Str::LocaleString str)
{
	OpString line;
	line.Reserve(64);
	RETURN_IF_ERROR(line.Set("<h2>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, str));
	RETURN_IF_ERROR(line.Append("</h2>\n"));
	return m_url.WriteDocumentData(URL::KNormal, line.CStr());
}

OP_STATUS OpGeneratedDocument::OpenUnorderedList()
{
	return m_url.WriteDocumentData(URL::KNormal, UNI_L("<ul>\n"));
}

OP_STATUS OpGeneratedDocument::AddListItem(Str::LocaleString str)
{
	OpString line;
	line.Reserve(256);
	line.Set(" <li>");
	AppendLocaleString(&line, str);
	line.Append("</li>\n");
	return m_url.WriteDocumentData(URL::KNormal, line.CStr());
}

OP_STATUS OpGeneratedDocument::CloseList()
{
	return m_url.WriteDocumentData(URL::KNormal, UNI_L("</ul>\n"));
}

#ifdef USE_ABOUT_TEMPLATES
OP_STATUS OpGeneratedDocument::GenerateDataFromTemplate(PrefsCollectionFiles::filepref template_file, BOOL allow_disk_cache)
{
	// Initial setup
	RETURN_IF_ERROR(m_url.SetAttribute(URL::KMIME_ForceContentType, "text/html;charset=utf-8"));
	RETURN_IF_ERROR(m_url.SetAttribute(URL::KCachePolicy_NoStore, !allow_disk_cache));

	// Read file and copy to URL
	OpFile file;

	RETURN_IF_LEAVE(g_pcfiles->GetFileL(template_file, file));
	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	OpString8		   buffer;
	OpFileLength	   bytes_read;
	const OpFileLength block_length = 1024;

	if (!buffer.Reserve(block_length))
		return OpStatus::ERR_NO_MEMORY;

	do
	{
		RETURN_IF_ERROR(file.Read(buffer.CStr(), block_length, &bytes_read));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, buffer, bytes_read));
	} while (bytes_read > 0);

	// Close URL
	return FinishURL();
}
#endif // USE_ABOUT_TEMPLATES

OP_STATUS OpGeneratedDocument::GeneratedByOpera()
{
	OpString line;
	line.Reserve(128);
	RETURN_IF_ERROR(line.Set("<address>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_GENERATED_BY_OPERA));
	RETURN_IF_ERROR(line.Append("</address>\n"));
	return m_url.WriteDocumentData(URL::KNormal, line);
}

/* static */
OP_STATUS OpGeneratedDocument::AppendLocaleString(OpString *s, Str::LocaleString id)
{
	OpString tr;
	RETURN_IF_ERROR(g_languageManager->GetString(id, tr));
	OP_ASSERT(tr.HasContent()); // Language string missing - check LNG file
	return s->Append(tr);
}

/* static */
OP_STATUS OpGeneratedDocument::AppendHTMLified(TempBuffer *s, OpStringC string, unsigned string_length)
{
	uni_char *htmlified = HTMLify_string(string.CStr(), string_length, TRUE);

	if (!htmlified)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = s->Append(htmlified);

	delete[] htmlified;

	return status;
}

/* static */
OP_STATUS OpGeneratedDocument::AppendHTMLified(OpString *s, OpStringC string, unsigned string_length)
{
	uni_char *htmlified = HTMLify_string(string.CStr(), string_length, TRUE);

	if (!htmlified)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = s->Append(htmlified);

	delete[] htmlified;

	return status;
}

/* static */
OP_STATUS OpGeneratedDocument::AppendHTMLified(OpString *s, const char *str, unsigned len)
{
	char *htmlified = HTMLify_string(str, len, TRUE);

	if (!htmlified)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = s->Append(htmlified);

	delete[] htmlified;

	return status;
}

/* static */
OP_STATUS OpGeneratedDocument::JSSafeLocaleString(OpString *s, Str::LocaleString id)
{
	OpString tr;
	RETURN_IF_ERROR(g_languageManager->GetString(id, tr));
	OP_ASSERT(tr.HasContent()); // Language string missing - check LNG file

	uni_char *p = tr.CStr(), *q = p - 1;
	if (p)
	{
		while ((q = uni_strpbrk(q + 1, UNI_L("\\/\"'"))) != NULL)
		{
			// Quote backslash, slash and quotes
			RETURN_IF_ERROR(s->Append(p, q - p));
			RETURN_IF_ERROR(s->Append("\\"));
			p = q;
		}
		return s->Append(p);
	}
	else
	{
		s->Empty();
		return OpStatus::OK;
	}
}

/* static */
void OpGeneratedDocument::StripHTMLTags(uni_char* string, unsigned length)
{
	enum { MAIN = 0, TAG, TAG_ACCEPT, ATTRIBUTE, ATTRIBUTE_ACCEPT, COMMENT, PROCINST } state = MAIN;
	unsigned src_index = 0;
	unsigned dst_index = 0;
	uni_char attr_quote = ' ';
	BOOL skip_this;
	while (src_index < length)
	{
		skip_this = FALSE;
		switch (state)
		{
		case MAIN:
			if (string[src_index] == '<')
			{
				if (src_index == length-1)
					break;
				if (uni_isalpha(string[src_index + 1]) || string[src_index + 1] == '/')
				{
					// Accept A tag.
					if (src_index < length - 2 && uni_tolower(string[src_index + 1]) == 'a' && !uni_isalpha(string[src_index + 2]))
						state = TAG_ACCEPT;
					else if (src_index < length - 3 && string[src_index + 1] == '/' && uni_tolower(string[src_index + 2]) == 'a' && !uni_isalpha(string[src_index + 3]))
						state = TAG_ACCEPT;
					// Replace BR tag with one space.
					else
					{
						if (src_index < length - 3 && uni_tolower(string[src_index + 1]) == 'b' && uni_tolower(string[src_index + 2]) == 'r' && !uni_isalpha(string[src_index + 3]))
							string[dst_index++] = ' ';
						state = TAG;
					}
				}
				else if (string[src_index + 1] == '?')
				{
					state = PROCINST;
				}
				else if (src_index < length - 3 && string[src_index + 1] == '!' && string[src_index + 2] == '-' && string[src_index + 3] == '-')
				{
					state = COMMENT;
				}
			}
			break;

		case TAG:
		case TAG_ACCEPT:
			if (string[src_index] == '>')
			{
				skip_this = state == TAG;
				state = MAIN;
			}
			else if (src_index < length - 2 && uni_isalpha(string[src_index]) && string[src_index + 1] == '=')
			{
				if (string[src_index + 2] == '\'' || string[src_index + 2] == '\"')
				{
					attr_quote = string[src_index + 2];
					state = state == TAG_ACCEPT ? ATTRIBUTE_ACCEPT : ATTRIBUTE;
				}
				else if (!uni_isspace(string[src_index + 2]))
				{
					attr_quote = ' ';
					state = state == TAG_ACCEPT ? ATTRIBUTE_ACCEPT : ATTRIBUTE;
				}
			}

			if (state == TAG_ACCEPT && src_index < length - 2 && uni_isspace(string[src_index]) &&
				uni_tolower(string[src_index + 1]) == 'o' && uni_tolower(string[src_index + 2]) == 'n')
			{
				// Strip onevent="" attributes.
				string[src_index + 1] = 'x';
			}
			else if (state == TAG_ACCEPT && src_index < length - 6 && uni_isspace(string[src_index]) &&
					uni_strncmp(string + src_index + 1, UNI_L("style"), 5) == 0 && !uni_isalpha(string[src_index + 6]))
				{
					// Strip style="" attributes.
					string[src_index + 1] = 'x';
				}
			else if (state == ATTRIBUTE_ACCEPT && src_index < length - (attr_quote == ' ' ? 10 : 11) &&
				uni_strncmp(string + src_index + 2 + (attr_quote == ' ' ? 0 : 1), UNI_L("javascript"), 10) == 0)
			{
				// Strip javascript: urls.
				string[src_index + 2 + (attr_quote == ' ' ? 0 : 1)] = 'x';
			}
			break;

		case ATTRIBUTE:
		case ATTRIBUTE_ACCEPT:
			if (string[src_index] == attr_quote)
				state = state == ATTRIBUTE ? TAG : TAG_ACCEPT;
			else if (attr_quote == ' ' && uni_isalpha(string[src_index]))
				state = state == ATTRIBUTE ? TAG : TAG_ACCEPT;
			break;

		case COMMENT:
			if (src_index < length - 2 && string[src_index] == '-' && string[src_index + 1] == '-' && string[src_index + 2] == '>')
			{
				src_index += 2;
				skip_this = TRUE;
				state = MAIN;
			}
			else if (src_index < length - 3 && string[src_index] == '-' && string[src_index + 1] == '-' && string[src_index + 2] == '!' && string[src_index + 3] == '>')
			{
				src_index += 3;
				skip_this = TRUE;
				state = MAIN;
			}
			break;

		case PROCINST:
			if (src_index < length - 1 && string[src_index] == '?' && string[src_index + 1] == '>')
			{
				src_index += 1;
				skip_this = TRUE;
				state = MAIN;
			}
			break;
		}

		if (src_index < length && !skip_this && (state == MAIN || state == TAG_ACCEPT || state == ATTRIBUTE_ACCEPT))
			string[dst_index++] = string[src_index++];
		else
			src_index++;
	}
	if (dst_index < length)
		string[dst_index] = 0;
}
