/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#if defined _LOCALHOST_SUPPORT_ || !defined NO_FTP_SUPPORT
#include "modules/about/opfolderlisting.h"
#include "modules/util/htmlify.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/viewers/viewers.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpLocale.h"
#include "modules/formats/uri_escape.h"

#ifndef WILDCHARS
# define WILDCHARS	"*?"
#endif

OpFolderListing::~OpFolderListing()
{
	// Clean up.
	delete[] m_temp_base;
	delete[] m_htmlified_url;
	delete[] m_displayable_base_url;
	delete[] m_htmlified_displayable_base_url;
}

OP_STATUS OpFolderListing::GenerateData()
{
#ifdef _LOCALHOST_SUPPORT_
	// Prefix for local files
# ifdef HAS_COMPLEX_GLOBALS
	static const uni_char * const localhost_prefix =
		UNI_L("file://localhost/");
# else
#  define localhost_prefix UNI_L("file://localhost/")
# endif
	static const size_t localhost_prefix_len = 17;
	OP_ASSERT(uni_strlen(localhost_prefix) == localhost_prefix_len);
#endif

	const uni_char * OP_MEMORY_VAR dirend_char = UNI_L("");
#if PATHSEPCHAR != '/'
	const uni_char * OP_MEMORY_VAR displayable_dirend_char = dirend_char;
#else
# define displayable_dirend_char dirend_char
#endif

	// FIXME: OOM - not reported, not checked

	// Set attributes.
	m_url.SetAttribute(URL::KIsDirectoryListing, TRUE);

	// Get the URL and take copies of it for processing.
	OpString base;
	RETURN_IF_ERROR(m_url.GetAttribute(URL::KUniName_Username_Escaped, base));
	m_temp_base = SetNewStr(base.CStr());
	m_displayable_base_url = SetNewStr(base.CStr());
	if (!m_temp_base || !m_displayable_base_url)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// Check if this is a local file or not.
#ifdef _LOCALHOST_SUPPORT_
	OP_MEMORY_VAR bool is_local_file = false;
#else
	static const bool is_local_file = false;
#endif
#ifdef _LOCALHOST_SUPPORT_
	if (uni_strncmp(m_displayable_base_url, localhost_prefix, localhost_prefix_len) == 0)
	{
		is_local_file = true;
	}
#endif

	static const uni_char pathsepchar = '/';

	//  If wildchars, remove upto last PATHSEPCHAR/
	unsigned int PathLen = uni_strcspn(m_temp_base, UNI_L(WILDCHARS));
	if (PathLen != uni_strlen(m_temp_base))
	{
		int i;
		for (i = PathLen; i > 0 && m_temp_base[i] != PATHSEPCHAR && m_temp_base[i] != '/' && m_temp_base[i] != '\\' ; i--) {}
		m_temp_base[i] = '\0';
	}

	PathLen = uni_strcspn(m_temp_base,UNI_L("?"));
	m_temp_base[PathLen] = '\0';
	{
		// Ignore parameter portion, but only for the last element to ensure
		// proper path names for FTP and file (anyone using parameters in the
		// parent directories will just have to take their chances).
		uni_char *temp_path = uni_strrchr(m_temp_base, '/'); // don't bother looking for backslash
		if(temp_path)
		{
			uni_char *temp_param = uni_strchr(temp_path, ';');
			if(temp_param)
			{
				PathLen = temp_param - m_temp_base;
				m_temp_base[PathLen] = '\0';
			}
		}
	}

	// If the path does not end in a path separator, add one.
	if (m_temp_base[PathLen-1] != '/'
#if PATHSEPCHAR != '/'
		&& m_temp_base[PathLen-1] != PATHSEPCHAR
#endif
	   )
	{
		dirend_char = UNI_L("/");
#if PATHSEPCHAR != '/'
		if (is_local_file)
		{
			displayable_dirend_char = UNI_L(PATHSEP);
		}
		else
		{
			displayable_dirend_char = dirend_char;
		}
#endif
	}

	// Create a HTML version of the URL string.
	m_htmlified_url = HTMLify_string(m_temp_base);
	if (!m_htmlified_url)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// Transmogrify the URL to make it displayable.
	UriUnescape::ReplaceChars(m_displayable_base_url,
		(static_cast<URLType>(m_url.GetAttribute(URL::KType)) == URL_FILE ?
		 UriUnescape::LocalfileUrlUtf8 : UriUnescape::SafeUtf8));

	// Remove localhost prefix
	if (is_local_file)
		op_memmove(m_displayable_base_url, m_displayable_base_url + localhost_prefix_len, UNICODE_SIZE(uni_strlen(m_displayable_base_url + localhost_prefix_len) + 1));

#ifdef SYS_CAP_FILESYSTEM_HAS_DRIVES
	// Clean up the generated path string
	if (is_local_file)
	{
		// Replace Netscape-compatible "|" as drive letter postfix with
		// a drive separator, which we assume to be ":"
		if (m_displayable_base_url[0] &&
		    '|' == m_displayable_base_url[1])
		{
			m_displayable_base_url[1] = ':';
		}

		// Make sure drive letter is upper-cased
		if (Unicode::IsLower(m_displayable_base_url[0]))
		{
			m_displayable_base_url[0] = Unicode::ToUpper(m_displayable_base_url[0]);
		}
	}
#endif

	// Remove formatting characters
	RemoveFormattingCharacters(m_displayable_base_url);

	// Create a HTML version of the displayable URL string.
	m_htmlified_displayable_base_url = HTMLify_string(m_displayable_base_url);
	if (!m_htmlified_displayable_base_url)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// Set up the document title from the displayable URL string.
	OpString document_title;
#ifndef SYS_CAP_FILESYSTEM_HAS_DRIVES
	if (is_local_file)
	{
		// Include an initial slash for local files on file systems that do
		// not have drive letters, to avoid blank titles or titles like
		// "usr/bin".
		if (uni_strcmp(m_htmlified_displayable_base_url, localhost_prefix) == 0)
		{
			RETURN_IF_ERROR(document_title.Set("/"));
		}
		else
		{
			RETURN_IF_ERROR(document_title.SetConcat(UNI_L("/"), m_htmlified_displayable_base_url));
		}
	}
	else
#endif
	{
		RETURN_IF_ERROR(document_title.Set(m_htmlified_displayable_base_url));
	}


	// Write the document.
#ifdef _LOCALHOST_SUPPORT_
	OpString dirstyle;
	TRAP_AND_RETURN(rc, g_pcfiles->GetFileURLL(PrefsCollectionFiles::StyleDirFile, &dirstyle));
	RETURN_IF_ERROR(OpenDocument(document_title.CStr(), dirstyle.CStr()));
#else
	RETURN_IF_ERROR(OpenDocument(document_title.CStr(), NULL));
#endif

	m_url.WriteDocumentDataUniSprintf(UNI_L(" <base href=\"%s%s\">\n"), m_htmlified_url, dirend_char);

	RETURN_IF_ERROR(OpenBody(Str::S_FOLDER_LISTING_TEXT));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<h2>")));

	// Provide links to each parent directory in the hierarchy.
	size_t len_htmlified = uni_strlen(m_htmlified_url);
	size_t len_displayable = uni_strlen(m_htmlified_displayable_base_url);
	size_t idx_htmlified = 0, idx_displayable = 0;
	uni_char *protocolsuffix = NULL;
	if (NULL != (protocolsuffix = uni_strstr(m_htmlified_url, UNI_L("://"))))
	{
		uni_char *thirdslash = uni_strchr(protocolsuffix + 3, '/');
		if (thirdslash)
		{
			idx_htmlified = thirdslash - m_htmlified_url + 1;
			if(!is_local_file)
			{
				m_url.WriteDocumentDataUniSprintf(UNI_L("<a href=\"%.*s\">%.*s</a>"), idx_htmlified, m_htmlified_url, idx_htmlified, m_htmlified_url);
			}
		}
		else
		{
			idx_htmlified = static_cast<size_t>(-1);
		}
	}
	else
	{
		idx_htmlified = static_cast<size_t>(-1);
	}
	if (NULL != (protocolsuffix = uni_strstr(m_htmlified_displayable_base_url, UNI_L("://"))))
	{
		uni_char *thirdslash = uni_strchr(protocolsuffix + 3, '/');
		if (thirdslash)
		{
			idx_displayable = thirdslash - m_htmlified_displayable_base_url + 1;
		}
	}

	if (static_cast<size_t>(-1) == idx_htmlified)
	{
		m_url.WriteDocumentDataUniSprintf(UNI_L("%s%s</h2>\n\n"),
										  m_htmlified_url, displayable_dirend_char);
	}
	else
	{
#ifndef SYS_CAP_FILESYSTEM_HAS_DRIVES
		if (is_local_file)
		{
			// Add the initial slash manually, as it does not have a
			// left-hand-side component.
			m_url.WriteDocumentDataUniSprintf(UNI_L("<a href=\"%s\">/</a>"), localhost_prefix);
		}
#endif
		while (idx_htmlified < len_htmlified && idx_displayable < len_displayable)
		{
			uni_char *nextslash_htmlified = uni_strchr(m_htmlified_url + idx_htmlified, '/');
			if (!nextslash_htmlified)
			{
				nextslash_htmlified = m_htmlified_url + len_htmlified;
			}
			uni_char *nextslash_displayable =
				uni_strchr(m_htmlified_displayable_base_url + idx_displayable, pathsepchar);
			if (!nextslash_displayable)
			{
				nextslash_displayable = m_htmlified_displayable_base_url + len_displayable;
			}
			int temp_idx_htmlified = nextslash_htmlified - m_htmlified_url;
			int temp_idx_displayable = nextslash_displayable - m_htmlified_displayable_base_url;

			m_url.WriteDocumentDataUniSprintf(UNI_L("<a href=\"%.*s/\">%.*s</a>/"),
											 temp_idx_htmlified, m_htmlified_url,
											 temp_idx_displayable - idx_displayable, &m_htmlified_displayable_base_url[idx_displayable]);

			idx_htmlified = temp_idx_htmlified + 1;
			idx_displayable = temp_idx_displayable + 1;
		}
		m_url.WriteDocumentData(URL::KNormal, UNI_L("</h2>\n\n"));
	}

	// Clean up.
	delete[] m_temp_base; m_temp_base = NULL;
	delete[] m_htmlified_url; m_htmlified_url = NULL;
	delete[] m_displayable_base_url; m_displayable_base_url = NULL;
	delete[] m_htmlified_displayable_base_url; m_htmlified_displayable_base_url = NULL;

	return OpStatus::OK;
}

#ifndef NO_FTP_SUPPORT
OP_STATUS OpFolderListing::WriteWelcomeMessage(const OpStringC &message)
{
	// FIXME: OOM - not reported, not checked
	m_url.WriteDocumentData(URL::KNormal, UNI_L("<pre>"));
	m_url.WriteDocumentData(URL::KHTMLify_CreateLinks, message);
	return m_url.WriteDocumentData(URL::KNormal, UNI_L("</pre>"));
}
#endif

OP_STATUS OpFolderListing::WriteEntry(const OpStringC &url, const OpStringC &name,
	char ftptype, OpFileInfo::Mode mode, OpFileLength size, const struct tm *datetime)
{
	if (!m_list_open)
	{
		// First call: Generate the header for the data table
		m_list_open = true;
		OpString line;
		line.Reserve(128);
		line.Set("<table>\n <tr>\n  <th>");
		AppendLocaleString(&line, Str::SI_DIRECTORY_COLOUMN_NAME);
		line.Append("</th>\n  <th>");
		AppendLocaleString(&line, Str::SI_DIRECTORY_COLOUMN_TYPE);
		line.Append("</th>\n  <th>");
		AppendLocaleString(&line, Str::SI_DIRECTORY_COLOUMN_SIZE);
		line.Append("</th>\n  <th>");
		AppendLocaleString(&line, Str::SI_DIRECTORY_COLOUMN_TIME);
		line.Append("</th>\n </tr>\n");
		m_url.WriteDocumentData(URL::KNormal, line.CStr());

		// Set up some strings we need later
		RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_DIRECTORY_DIRECTORY, m_str_directory));
		RETURN_IF_ERROR(m_str_b.Set(" "));
		RETURN_IF_ERROR(AppendLocaleString(&m_str_b, Str::SI_IDSTR_BYTE));
		RETURN_IF_ERROR(m_str_kb.Set(" "));
		RETURN_IF_ERROR(AppendLocaleString(&m_str_kb, Str::SI_IDSTR_KILOBYTE));
	}

	// Make URL and file name HTML safe
	uni_char *file_name_url = HTMLify_string(url.CStr());
	uni_char *file_name_text= HTMLify_string(name.CStr());
	RemoveFormattingCharacters(file_name_text);

	const uni_char *dir_end_char = UNI_L("");	// Directory slash if needed
	const uni_char *cssclass = NULL;			// Class attribute
	uni_char filetypebuf[256] = { 0 };			// File type string /* ARRAY OK 2008-09-11 adame */
	uni_char file_length[64] = { 0 };			// File length /* ARRAY OK 2008-09-11 adame */
	uni_char file_time[128] = { 0 };			// File time string /* ARRAY OK 2008-09-11 adame */

	switch (mode)
	{
	case OpFileInfo::DIRECTORY:
		cssclass = UNI_L("class=\"dir\"");
		if (name.CStr()[name.Length() - 1] != '/')
		{
			dir_end_char = UNI_L("/");
		}
		uni_strlcpy(filetypebuf, m_str_directory.CStr(), ARRAY_SIZE(filetypebuf));
		goto setnofilelength;

	case OpFileInfo::SYMBOLIC_LINK:
		cssclass = UNI_L("class=\"sym\"");
setnofilelength:
		file_length[0] = 0;
		break;

	default:
	case OpFileInfo::FILE:
		cssclass= UNI_L("");
		{
			// Format file size
			OpFileLength ksize = (size + 1023) / 1024;

			if (ksize < 2)
			{
				if (g_oplocale->NumberFormat(file_length, ARRAY_SIZE(file_length), size, TRUE) == -1)
					file_length[0] = 0; // FIXME: this error should probably be reported instead of being silently ignored
				else
					uni_strlcat(file_length, m_str_b.CStr(), ARRAY_SIZE(file_length));
			}
			else
			{
				if (g_oplocale->NumberFormat(file_length, ARRAY_SIZE(file_length), ksize, TRUE) == -1)
					file_length[0] = 0; // FIXME: this error should probably be reported instead of being silently ignored
				else
					uni_strlcat(file_length, m_str_kb.CStr(), ARRAY_SIZE(file_length));
			}
		}
	}

	// Format datetime
	if (datetime)
	{
		g_oplocale->op_strftime(file_time, ARRAY_SIZE(file_time), UNI_L("%x %X"), datetime);
	}
	else
	{
		file_time[0] = 0;
	}

	// Format filetype string
	if (!filetypebuf[0])
	{
#ifdef OPSYSTEMINFO_GETFILETYPENAME
		if (!OpStatus::IsSuccess(g_op_system_info->GetFileTypeName(name.CStr(), filetypebuf, ARRAY_SIZE(filetypebuf)))
			|| !filetypebuf[0])
#endif
		{
			int i = name.FindLastOf('.');
			if (i == KNotFound || i == 0 || name.Length() - (i + 1) > 4)
			{
				filetypebuf[0] = 0;
			}
			else
			{
/*
				// Look up the MIME type in Opera's internal list
				const char *mime_type = GetContentTypeStringFromExt(name.DataPtr() + i + 1);
				if (mime_type)
				{
					uni_char *p = filetypebuf;
					uni_char *end = p + MIN(ARRAY_SIZE(filetypebuf) - 1, op_strlen(mime_type));
					while (p < end)
					{
						*(p ++) = static_cast<uni_char>(*reinterpret_cast<const unsigned char *>(mime_type ++));
					}
					*p = 0;
				}
				else
*/
				{
					uni_strlcpy(filetypebuf, name.DataPtr() + i + 1, ARRAY_SIZE(filetypebuf));
					uni_strupr(filetypebuf);
				}
			}
		}
	}
	uni_char *file_type = HTMLify_string(filetypebuf);
	if (!file_type)
	{
		file_type = filetypebuf;
		filetypebuf[0] = 0;
	}


	// Data setup done
	if (file_name_url && file_name_text && file_type)
	{
		// Write to the document
		m_url.WriteDocumentDataUniSprintf(
			UNI_L("\n <tr>\n  <td><a href=\"%s%s%s\" %s>%s</a></td>\n  <td>%s</td>\n  <td>%s</td>\n  <td>%s</td>\n </tr>\n"),
			file_name_url, dir_end_char,
			(ftptype == 'i' ? UNI_L(";type=i") : UNI_L("")),
			cssclass, file_name_text,
			file_type, file_length, file_time);
	}
	else
	{
		// Raise error
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}

	// Clean up
	if (file_type != filetypebuf)
	{
		delete[] file_type;
	}
	delete[] file_name_url;
	delete[] file_name_text;

	// FIXME: OOM - not reported, not checked
	return OpStatus::OK;
}

OP_STATUS OpFolderListing::EndFolderListing()
{
	// FIXME: OOM - not reported, not checked
	if (m_list_open)
	{
		m_url.WriteDocumentData(URL::KNormal, UNI_L("</table>\n"));
	}
	return CloseDocument();
}

void OpFolderListing::RemoveFormattingCharacters(uni_char *s)
{
	while (*s)
	{
		switch (Unicode::GetCharacterClass(*s))
		{
		case CC_Cc:
		case CC_Cf:
		case CC_Co:
		case CC_Cs:
			*s = NOT_A_CHARACTER;
		}
		++ s;
	}
}
#endif
