/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SAVE_SUPPORT

#include "modules/logdoc/savewithinline.h"
#include "modules/logdoc/inline_finder.h"

#include "modules/dochand/win.h"
#include "modules/doc/frm_doc.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/logdoc/html_att.h"
#include "modules/logdoc/link.h"
#include "modules/logdoc/datasrcelm.h"
#include "modules/logdoc/src/html5/html5tokenizer.h"
#include "modules/style/css_save.h"
#include "modules/url/url2.h"
#include "modules/url/url_api.h"
#include "modules/url/url_tools.h"
#include "modules/upload/upload.h"
#include "modules/util/path.h"
#include "modules/util/opfile/unistream.h"
#include "modules/util/adt/opvector.h"
#include "modules/formats/uri_escape.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/windowcommander/src/WindowCommander.h"

/**
 * The buffer |filename_buf| must have room for at least 14 chars (8.4 plus terminating NULL)
 */
void ComposeInlineFilename(uni_char* filename_buf, uni_char* filename_start, const uni_char* inline_url_name, URLContentType cnt_type)
{
	if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::SaveUseSubfolder))
		*(filename_start -1) = '/';
	else
		*filename_start = '\0';

	int flen = 0;
	uni_char *nstart = (uni_char*) uni_strrchr(inline_url_name, '/');
	if (nstart)
	{
		uni_char* tmp = ++nstart;
		while (op_isalpha((unsigned char) *tmp) || op_isdigit((unsigned char) *tmp) || *tmp == '-' || *tmp == '_') // 01/04/98 YNP
			tmp++;

		flen = tmp - nstart;
		if (flen > 8)
			flen = 8;
		uni_strncpy(filename_start, nstart, flen);
	}

	while (flen < 8)
		filename_start[flen++] = '0';

	filename_start[flen++] = '.';

	switch (cnt_type)
	{
	case URL_XML_CONTENT:
		uni_strcpy(filename_start + flen, UNI_L("xml"));
		break;
#ifdef _WML_SUPPORT_
	case URL_WML_CONTENT:
		uni_strcpy(filename_start + flen, UNI_L("wml"));
		break;
	case URL_WBMP_CONTENT:
		uni_strcpy(filename_start + flen, UNI_L("wbmp"));
		break;
#endif
#ifdef SVG_SUPPORT
	case URL_SVG_CONTENT:
		uni_strcpy(filename_start + flen, UNI_L("svg"));
		break;
#endif
	case URL_HTML_CONTENT:
		uni_strcpy(filename_start + flen, UNI_L("htm"));
		break;
#ifdef _PNG_SUPPORT_
	case URL_PNG_CONTENT:
		uni_strcpy(filename_start + flen, UNI_L("png"));
		break;
#endif
#ifdef HAS_ATVEF_SUPPORT
	case URL_TV_CONTENT:
		// This is a phrank... but it might come in handy?
		uni_strcpy(filename_start + flen, UNI_L("tv"));
		break;
#endif // HAS_ATVEF_SUPPORT
	case URL_GIF_CONTENT:
		uni_strcpy(filename_start + flen, UNI_L("gif"));
		break;
#ifdef _JPG_SUPPORT_
	case URL_JPG_CONTENT:
		uni_strcpy(filename_start + flen, UNI_L("jpg"));
		break;
#endif // _JPG_SUPPORT_
#ifdef _BMP_SUPPORT_
	case URL_BMP_CONTENT:
		uni_strcpy(filename_start + flen, UNI_L("bmp"));
		break;
	case URL_WEBP_CONTENT:
		uni_strcpy(filename_start + flen, UNI_L("webp"));
		break;
#endif // _BMP_SUPPORT_
#ifdef _XBM_SUPPORT_
	case URL_XBM_CONTENT:
		uni_strcpy(filename_start + flen, UNI_L("xbm"));
		break;
#endif // _XBM_SUPPORT_
	case URL_X_JAVASCRIPT:
		uni_strcpy(filename_start + flen, UNI_L("js"));
		break;
	case URL_CSS_CONTENT:
		uni_strcpy(filename_start + flen, UNI_L("css"));
		break;
	default:
		{
			uni_char *fileext = (uni_char*) uni_strrchr(inline_url_name, '.');
			if (fileext && uni_strlen(fileext + 1) <= 3)
				uni_strcpy(filename_start + flen, fileext + 1);
			else
				uni_strcpy(filename_start + flen, UNI_L("txt"));
		}
		break;
	}

	BOOL exists = FALSE;
	do
	{
		OpFile *f = OP_NEW(OpFile, ());
		if (f)
		{
			if (OpStatus::IsSuccess(f->Construct(filename_buf)) &&
				OpStatus::IsSuccess(f->Exists(exists)) &&
				exists)
			{
				IncFileString(filename_start, 7, 0, 7);
			}
			OP_DELETE(f);
		}
	} while (exists);
}

SavedUrlCache::SavedUrlCache(const uni_char* fname) : m_used(0)
{
	uni_strncpy(m_filename_buf, fname, FILENAME_BUF_SIZE);
	m_filename_buf[FILENAME_BUF_SIZE - 1] = 0;

	if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::SaveUseSubfolder))
	{
		m_reldir_start = uni_strrchr(m_filename_buf, PATHSEPCHAR);
		if (m_reldir_start)
			m_reldir_start++;
		else
			m_reldir_start = m_filename_buf;

		m_filename_start = uni_strrchr(m_filename_buf, '.');

		if (NULL == m_filename_start || m_filename_start < m_reldir_start || m_filename_start == m_filename_buf)
			m_filename_start = m_filename_buf + uni_strlen(fname);

		int possible_additional_filename = MAX (0, FILENAME_BUF_SIZE - (m_filename_start - m_filename_buf));

		uni_strncpy(m_filename_start, UNI_L("_files"), possible_additional_filename);

		m_filename_start += MIN (6, possible_additional_filename);
	}
	else
		m_filename_start = uni_strrchr(m_filename_buf, PATHSEPCHAR);

	if (m_filename_start)
		m_filename_start++;
	else
		*m_filename_buf = '\0';
}

BOOL HasBlacklistedExtension(uni_char* filename)
{
	// blacklist the following extensions from automatic saving
	const uni_char* blacklisted_extensions[10];
	blacklisted_extensions[0] = UNI_L("bat");
	blacklisted_extensions[1] = UNI_L("com");
	blacklisted_extensions[2] = UNI_L("exe");
	blacklisted_extensions[3] = UNI_L("inf");  // windows package information file
	blacklisted_extensions[4] = UNI_L("lnk");  // windows shortcut
	blacklisted_extensions[5] = UNI_L("msi");  // windows installer
	blacklisted_extensions[6] = UNI_L("pif");  // windows program information file
	blacklisted_extensions[7] = UNI_L("reg");  // windows reg entry
	blacklisted_extensions[8] = UNI_L("scr");  // windows screensaver
	blacklisted_extensions[9] = UNI_L("vbs");  // VB script

	if (!filename)
		return FALSE;

	uni_char* extension = uni_strrchr(filename, '.');
	if (!extension || !*(++extension))  // no dot or dot is last character
		return FALSE;

	for (UINT i = 0; i < ARRAY_SIZE(blacklisted_extensions); i++)
		if (uni_strcmp(extension, blacklisted_extensions[i]) == 0)
			return TRUE;

	return FALSE;
}

BOOL SafeTypeToSave(Markup::Type element_type, URL content)
{
	// Don't change this list without checking security implications. Ref bug: 145253, 208192
	URLContentType content_type = content.ContentType();

	switch (element_type)
	{
	case Markup::HTE_LINK:
	case Markup::HTE_SCRIPT:
	case Markup::HTE_PROCINST:
		if (content_type != URL_UNKNOWN_CONTENT && content_type != URL_UNDETERMINED_CONTENT)
			return TRUE;
		break;
	case Markup::HTE_STYLE:
		if (content_type == URL_CSS_CONTENT)
			return TRUE;
		// falltrough to allow all image types in CSS
	case Markup::HTE_IMG:
	case Markup::HTE_BODY:
	case Markup::HTE_TABLE:
	case Markup::HTE_TR:
	case Markup::HTE_TH:
	case Markup::HTE_TD:
	case Markup::HTE_INPUT:
		{
			switch (content_type)
			{
			case URL_WBMP_CONTENT:
			case URL_SVG_CONTENT:
			case URL_PNG_CONTENT:
			case URL_GIF_CONTENT:
			case URL_JPG_CONTENT:
			case URL_BMP_CONTENT:
			case URL_WEBP_CONTENT:
			case URL_XBM_CONTENT:
#ifdef ICO_SUPPORT
			case URL_ICO_CONTENT:
#endif //ICO_SUPPORT
				return TRUE;
			default:
				return FALSE;
			}
		}
		break;
	case Markup::HTE_OBJECT:
	case Markup::HTE_EMBED:
	case Markup::HTE_APPLET:
	case Markup::HTE_FRAME:
	case Markup::HTE_IFRAME:
		// Disallow some filenames
		OpString suggested_filename;
		TRAPD(status, content.GetAttribute(URL::KSuggestedFileName_L, suggested_filename));
		if (OpStatus::IsError(status))
			return TRUE;

		return !HasBlacklistedExtension(suggested_filename.CStr());

		break;
	}

	return FALSE;
}

const uni_char*
SavedUrlCache::GetSavedFilename(const URL& url, BOOL& new_filename)
{
	new_filename = FALSE;

	for (int i = 0; i < m_used; i++)
	{
		if (m_array[i].url == url)
		{
			uni_strcpy(m_filename_start, m_array[i].name);
			return m_array[i].name;
		}
	}

	if (m_used == 1024)
		return NULL;

	ComposeInlineFilename(m_filename_buf, m_filename_start, url.GetAttribute(URL::KUniName).CStr(), url.ContentType());

	m_array[m_used].url = url;

	// m_filename_start is always less than 13 bytes !!!
	OP_ASSERT(uni_strlen(m_filename_start) < 13);

	uni_strcpy(m_array[m_used].name, m_filename_start);

	new_filename = TRUE;
	return m_array[m_used++].name;
}

static void GetCharacterEncodingL(OpString8& charset, URL& url, Window* window, const char* force_encoding=NULL)
{
	charset.Empty();
	if (force_encoding && *force_encoding &&
	    !strni_eq(force_encoding, "AUTODETECT-", 11))
	{
		// Window's forced encoding is used if defined,
		// and not an autodetection setting.
		charset.SetL(force_encoding);
	}
	else
	{
		// If that fails, get encoding from content
		OpStackAutoPtr<URL_DataDescriptor> url_dd (url.GetDescriptor(NULL, TRUE, TRUE, TRUE, window));
		if (!url_dd.get())
			LEAVE(OpStatus::ERR_NO_MEMORY);
		LEAVE_IF_ERROR(url_dd->Init(FALSE, NULL));

		BOOL more = FALSE;
#ifdef OOM_SAFE_API
		url_dd->RetrieveDataL(more);
#else
		url_dd->RetrieveData(more);
#endif //OOM_SAFE_API
		const char *dd_charset = url_dd->GetCharacterSet();
		if (dd_charset)
			charset.SetL(dd_charset);
	}

	if (charset.IsEmpty())
	{
		// Otherwise use URL's encoding, if available.
		const char *mime_charset = url.GetAttribute(URL::KMIME_CharSet).CStr();
		if (mime_charset)
			charset.SetL(mime_charset);
	}

	// And if *that* failed, use the default encoding
	if (charset.IsEmpty())
		charset.SetL(g_pcdisplay->GetDefaultEncoding());
}

/**
 * FIXME: Should use the new escaping api when it is in place.
 *
 * @param src The string to escape
 * @param dst Where to put the escaped string
 */
static void EscapeFilename(const uni_char *src, OpString& dst)
{
	LEAVE_IF_ERROR(UriEscape::AppendEscaped(dst, src, UriEscape::AllUnsafe & ~UriEscape::Slash));
}

static void WriteQuotedHTMLValuesL(const uni_char *value, unsigned length, UnicodeOutputStream *buf, BOOL in_attr, BOOL escape_quote=TRUE)
{
	for (const uni_char *c = value; length && *c; c++, length--)
	{
		switch (*c)
		{
		case '&':
			buf->WriteStringL(UNI_L("&amp;"), 5);
			break;
		case 0xa0:
			buf->WriteStringL(UNI_L("&nbsp;"), 6);
			break;
		case '"':
			if (in_attr && escape_quote)
				buf->WriteStringL(UNI_L("&quot;"), 6);
			else
				buf->WriteStringL(c, 1);
			break;
		case '<':
			if (!in_attr)
				buf->WriteStringL(UNI_L("&lt;"), 4);
			else
				buf->WriteStringL(c, 1);
			break;
		case '>':
			if (!in_attr)
				buf->WriteStringL(UNI_L("&gt;"), 4);
			else
				buf->WriteStringL(c, 1);
			break;
		default:
			buf->WriteStringL(c, 1);
		}
	}
}

static void WriteQuotedCharacterDataL(const HTML5StringBuffer& data, UnicodeOutputStream *buf)
{
	HTML5StringBuffer::ContentIterator iter(data); ANCHOR(HTML5StringBuffer::ContentIterator, iter);

	unsigned length;
	const uni_char* data_str;
	while ((data_str = iter.GetNext(length)) != NULL)
		WriteQuotedHTMLValuesL(data_str, length, buf, FALSE, FALSE);
}

static void WriteQuotedAttributesL(HTML5TokenWrapper& token, UnicodeOutputStream* buf)
{
	for (unsigned i = 0; i < token.GetAttrCount(); i++)
	{
		HTML5TokenBuffer *name;
		uni_char* value;
		unsigned value_len;
		BOOL must_delete = token.GetAttributeL(i, name, value, value_len);
		ANCHOR_ARRAY(uni_char, value);
		if (!must_delete)
			ANCHOR_ARRAY_RELEASE(value);

		buf->WriteStringL(UNI_L(" "), 1);
		buf->WriteStringL(name->GetBuffer());
		if (value)
		{
			buf->WriteStringL(UNI_L("=\""), 2);
			WriteQuotedHTMLValuesL(value, value_len, buf, TRUE, TRUE);
			buf->WriteStringL(UNI_L("\""), 1);
		}
	}
}

static void ReplaceURLAttrL(HTML5TokenWrapper& token,
		const uni_char* attr_name,
		URL& current_url,
		URL& use_base_url,
		HLDocProfile* hld_profile,
		SavedUrlCache* su_cache,
		BOOL main_page,
		Window* window,
		BOOL frames_only)
{
	unsigned attr_idx;
	uni_char* attr_value = token.GetUnescapedAttributeValueL(attr_name, &attr_idx); ANCHOR_ARRAY(uni_char, attr_value);

	if (attr_value)
	{
		URL base_url = use_base_url; ANCHOR(URL, base_url);

		unsigned codebase_attr_idx;
		if (token.GetElementType() == Markup::HTE_OBJECT && token.HasAttribute(UNI_L("codebase"), codebase_attr_idx))
		{
			// need to resolve relative to codebase
			uni_char* codebase = token.GetUnescapedAttributeValueL(UNI_L("codebase")); ANCHOR_ARRAY(uni_char, codebase);
			base_url = g_url_api->GetURL(use_base_url, codebase);
		}

		OpStackAutoPtr<URL> inline_url(SetUrlAttr(attr_value, uni_strlen(attr_value), &base_url, hld_profile, FALSE, FALSE));
		if (!inline_url.get())
			LEAVE(OpStatus::ERR_NO_MEMORY);

		URLStatus inline_url_stat = inline_url->Status(TRUE);

		if (inline_url_stat == URL_LOADED)
		{
			if (token.GetElementType() == Markup::HTE_OBJECT) //we need to save codebase as '.'
			{
				unsigned codebase_attr_idx;
				if (token.HasAttribute(UNI_L("codebase"), codebase_attr_idx))
					token.SetAttributeValueL(codebase_attr_idx, UNI_L("."));
			}

			URLContentType content_type = inline_url->ContentType();
			BOOL save_to_file = SafeTypeToSave(token.GetElementType(), *inline_url.get());

			// if we're only saving frames, filter out other types
			if (save_to_file && frames_only)
			{
				switch (content_type)
				{
					case URL_XML_CONTENT:
#ifdef _WML_SUPPORT_
					case URL_WML_CONTENT:
#endif
					case URL_HTML_CONTENT:
					case URL_X_JAVASCRIPT:
					case URL_TEXT_CONTENT:
					case URL_CSS_CONTENT:
						save_to_file = TRUE;
						break;
					default:
						save_to_file = FALSE;
						break;
				}
			}

			OpString url_string;
			const uni_char* filename = NULL;
			uni_char tmp_filename[255];  // ARRAY OK arneh 2011-03-11

			if (save_to_file)
			{
				// Need to create a file and save it.  First generate a filename for it
				BOOL new_file = FALSE;
				if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::SaveUseSubfolder))
				{
					filename = su_cache->GetSavedFilename(*inline_url.get(), new_file);
					if (main_page)
					{
						uni_strncpy(tmp_filename, su_cache->GetReldirStart(), ARRAY_SIZE(tmp_filename));
						tmp_filename[ARRAY_SIZE(tmp_filename) - 1] = '\0';
						filename = tmp_filename;
					}
				}
				else
					filename = su_cache->GetSavedFilename(*inline_url.get(), new_file);

				if (new_file && !(*inline_url.get() == current_url))  // save the file
				{
					TRAPD(status, SaveWithInlineHelper::SaveL(*inline_url.get(), NULL, su_cache->GetPathname(), NULL, su_cache, window, frames_only, FALSE));
					if (OpStatus::IsMemoryError(status))
						LEAVE(status);
				}
			}
			else  // If we're not saving, just keep a URL to the resource
			{
				inline_url->GetAttributeL(URL::KUniName_Username_Password_NOT_FOR_UI, url_string);
				filename = url_string.CStr();
			}

			// replace the attribute so that it points where the resource is now
			if (filename)
			{
				if (save_to_file)
				{
					OpString escaped_filename; ANCHOR(OpString, escaped_filename);
					EscapeFilename(filename, escaped_filename);
					token.SetAttributeValueL(attr_idx, escaped_filename.CStr());
				}
				else if (!uni_str_eq(filename, attr_value))
					token.SetAttributeValueL(attr_idx, filename);
			}
		}
	}
}

OP_STATUS
SaveWithInlineHelper::Save(URL& url, FramesDocument *displayed_doc, const uni_char* fname, const char *force_encoding, Window *window, BOOL frames_only)
{
	TRAPD(oom_status, SaveWithInlineHelper::SaveL(url, displayed_doc, fname, force_encoding, window, frames_only));
	return oom_status;
}

void
SaveWithInlineHelper::SaveL(URL& url, FramesDocument *displayed_doc, const uni_char* fname, const char *force_encoding, Window *window, BOOL frames_only)
{
	OpStackAutoPtr<SavedUrlCache> su_cache (OP_NEW_L(SavedUrlCache, (fname)));

	if (*su_cache->GetPathname())
		SaveWithInlineHelper::SaveL(url, displayed_doc, fname, force_encoding, su_cache.get(), window, frames_only);
}


OP_STATUS
SaveWithInlineHelper::Save(URL& url, FramesDocument *displayed_doc, const uni_char* fname,
							 const char *force_encoding,
							 SavedUrlCache* su_cache,
							 Window *window,
							 BOOL frames_only)
{
	TRAPD(oom_status, SaveL(url, displayed_doc, fname, force_encoding, su_cache, window, frames_only, TRUE));
	return oom_status;
}

void
SaveWithInlineHelper::SaveL(URL& url, FramesDocument *displayed_doc, const uni_char* fname,
							 const char *force_encoding,
							 SavedUrlCache* su_cache,
							 Window *window,
							 BOOL frames_only,
							 BOOL main_page)
{
	if (url.Status(TRUE) == URL_LOADING || !fname)
	{
		LEAVE(OpStatus::ERR);
	}

	URL tempurl = url.GetAttribute(URL::KMovedToURL, TRUE);
	if (!tempurl.IsEmpty())
		url = tempurl;

	const char *suggested_encoding = NULL;
	if (displayed_doc && displayed_doc->GetHLDocProfile())
		suggested_encoding = displayed_doc->GetHLDocProfile()->GetCharacterSet();

	BOOL is_xml = FALSE;
	URLContentType content_type = url.ContentType();

	// First the types we just save directly to file without parsing for links
	switch (content_type)
	{
	case URL_XML_CONTENT:
	case URL_SVG_CONTENT:
	case URL_WML_CONTENT:
		is_xml = TRUE;
		// fall through
	case URL_HTML_CONTENT:
		// proceed with HTML parser tokenization
		break;

	case URL_CSS_CONTENT:
		SaveCSSWithInlineL(url, fname, force_encoding, suggested_encoding, su_cache, NULL, NULL, window, main_page);
		return;

	default:
		{
			unsigned long err;
			err = url.SaveAsFile(fname);
			if(err)
			{
				OpString msg;
				if (OpStatus::IsSuccess(g_languageManager->GetString(ConvertUrlStatusToLocaleString(err), msg)))
				{
					WindowCommander *wic(window->GetWindowCommander());
					wic->GetDocumentListener()->OnGenericError(wic,
						url.GetAttribute(URL::KUniName_Username_Password_Hidden).CStr(),
						msg.CStr(), url.GetAttribute(URL::KInternalErrorString).CStr());
				}
				LEAVE(ConvertUrlStatusToOpStatus(err));
			}
			return;
		}
	}

	unsigned int suggested_encoding_id = g_charsetManager->GetCharsetIDL(suggested_encoding);
	g_charsetManager->IncrementCharsetIDReference(suggested_encoding_id);
	OpStackAutoPtr<URL_DataDescriptor> url_data_desc(url.GetDescriptor(NULL, TRUE, FALSE, TRUE, window, URL_UNDETERMINED_CONTENT, 0, FALSE, suggested_encoding_id));
	g_charsetManager->DecrementCharsetIDReference(suggested_encoding_id);
	if (!url_data_desc.get())
		LEAVE(OpStatus::ERR);

	OpStackAutoPtr<UnicodeFileOutputStream> out(OP_NEW_L(UnicodeFileOutputStream, ()));
	// Always write as UTF-8
	LEAVE_IF_ERROR(out->Construct(fname, "UTF-8"));

	OpString8 charset; ANCHOR(OpString8, charset);
	GetCharacterEncodingL(charset, url, window, force_encoding);

	URL use_base_url = url;
	DataSrc style_src_head; ANCHOR(DataSrc, style_src_head);

	BOOL more;
	url_data_desc->RetrieveDataL(more);

	uni_char* data_buf = (uni_char*)url_data_desc->GetBuffer();
	unsigned long data_len = url_data_desc->GetBufSize();

	OpStackAutoPtr<LogicalDocument> dummy_logdoc (OP_NEW_L(LogicalDocument, (NULL)));
	HLDocProfile* dummy_profile = dummy_logdoc->GetHLDocProfile();

	dummy_profile->SetURL(&url);
	if (!charset.IsEmpty())
		dummy_profile->SetCharacterSet(charset.CStr());

	HTML5Tokenizer tokenizer(NULL, TRUE); ANCHOR(HTML5Tokenizer, tokenizer);
	HTML5TokenWrapper token; ANCHOR(HTML5TokenWrapper, token);
	token.InitializeL();

#define RESET_SUSPEND Markup::HTE_DOC_ROOT
	Markup::Type suspend_until_endtag = RESET_SUSPEND;

	BOOL has_written_meta_contenttype = FALSE;

	while (data_buf && UNICODE_DOWNSIZE(data_len) > 0)
	{
		unsigned uni_len = UNICODE_DOWNSIZE(data_len);
		tokenizer.AddDataL(data_buf, uni_len, !more, FALSE, FALSE);
		url_data_desc->ConsumeData(data_len);

		while (token.GetType() != HTML5Token::ENDOFFILE)
		{
			token.ResetWrapper();
			TRAPD(status, tokenizer.GetNextTokenL(token));
			if (status == HTML5ParserStatus::NEED_MORE_DATA)
				break;

			LEAVE_IF_ERROR(status);

			switch (token.GetType())
			{
			case HTML5Token::DOCTYPE:
				{
					dummy_logdoc->SetDoctype(token.GetNameStr(), token.GetPublicIdentifier(), token.GetSystemIdentifier());

					out->WriteStringL(UNI_L("<!DOCTYPE"), 9);

					if (token.GetNameStr())
					{
						out->WriteStringL(UNI_L(" "), 1);
						out->WriteStringL(token.GetNameStr());
					}

					if (token.GetPublicIdentifier())
					{
						out->WriteStringL(UNI_L(" PUBLIC \""), 9);
						WriteQuotedHTMLValuesL(token.GetPublicIdentifier(), UINT_MAX, out.get(), TRUE, FALSE);
						out->WriteStringL(UNI_L("\""), 1);
					}

					if (token.GetSystemIdentifier())
					{
						if (token.GetPublicIdentifier())
							out->WriteStringL(UNI_L(" \""), 2);
						else
							out->WriteStringL(UNI_L(" SYSTEM \""), 9);
						WriteQuotedHTMLValuesL(token.GetSystemIdentifier(), UINT_MAX, out.get(), TRUE, FALSE);
						out->WriteStringL(UNI_L("\""), 1);
					}

					out->WriteStringL(UNI_L(">"), 1);
				}
				break;

			case HTML5Token::START_TAG:
				{
					Markup::Type type = g_html5_name_mapper->GetTypeFromTokenBuffer(token.GetName());
					token.SetElementType(type);
					BOOL supress_output = FALSE;

					if (!has_written_meta_contenttype && content_type == URL_HTML_CONTENT)
					{
						// we'll insert a meta element with content type and charset before the first element which isn't HTML or HEAD
						if (type != Markup::HTE_HTML && Markup::HTE_HEAD)
						{
							out->WriteStringL(UNI_L("\n<meta http-equiv=\"Content-Type\" content=\"text/html;charset=UTF-8\">\n"));
							has_written_meta_contenttype = TRUE;
						}
					}

					switch (type)
					{
					case Markup::HTE_SCRIPT:
						ReplaceURLAttrL(token, UNI_L("src"), url, use_base_url, dummy_profile, su_cache, main_page, window, frames_only);

						// Make sure the script data is tokenized correctly as character data:
						if (!is_xml || !token.IsSelfClosing())
						{
							tokenizer.SetState(HTML5Tokenizer::SCRIPT_DATA);
							suspend_until_endtag = type;
						}
						break;

					case Markup::HTE_STYLE:
						// Make sure the style data is tokenized correctly as character data:
						tokenizer.SetState(HTML5Tokenizer::RAWTEXT);
						suspend_until_endtag = type;
						break;

					case Markup::HTE_BASE:
						{
							// make sure we resolve URLs relative to the base URL
							uni_char* base_str = token.GetUnescapedAttributeValueL(UNI_L("href"));  ANCHOR_ARRAY(uni_char, base_str);
							if (base_str)
								use_base_url = g_url_api->GetURL(use_base_url, base_str);
							// don't write the base URL to the file, or relative URLs won't work
							supress_output = TRUE;
						}
						break;

					case Markup::HTE_META:
						{
							uni_char* meta_str = token.GetUnescapedAttributeValueL(UNI_L("http-equiv"));  ANCHOR_ARRAY(uni_char, meta_str);
							if (meta_str)
							{
								// we add our own content-type, so ignore the original one
								if (uni_strni_eq(meta_str, "content-type", 12) && meta_str[12] == 0)
									supress_output = TRUE;
							}
						}
						break;

					case Markup::HTE_FRAME:
						if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FramesEnabled))
							break;
						// fall through

					case Markup::HTE_IFRAME:
					case Markup::HTE_APPLET:
					case Markup::HTE_EMBED:
					case Markup::HTE_IMG:
					case Markup::HTE_INPUT:
						ReplaceURLAttrL(token, UNI_L("src"), url, use_base_url, dummy_profile, su_cache, main_page, window, frames_only);
						break;

					case Markup::HTE_BODY:
					case Markup::HTE_TABLE:
					case Markup::HTE_TR:
					case Markup::HTE_TH:
					case Markup::HTE_TD:
						ReplaceURLAttrL(token, UNI_L("background"), url, use_base_url, dummy_profile, su_cache, main_page, window, frames_only);
						break;

					case Markup::HTE_OBJECT:
						ReplaceURLAttrL(token, UNI_L("data"), url, use_base_url, dummy_profile, su_cache, main_page, window, frames_only);
						break;

					case Markup::HTE_LINK:
						{
							uni_char* rel = token.GetUnescapedAttributeValueL(UNI_L("rel"));

							if (rel)
							{
								// We only care about stylesheets and favicons.
								unsigned kinds = LinkElement::MatchKind(rel);
								OP_DELETEA(rel);

								if (kinds & (LINK_TYPE_STYLESHEET | LINK_TYPE_ICON))
									ReplaceURLAttrL(token, UNI_L("href"), url, use_base_url, dummy_profile, su_cache, main_page, window, frames_only);
							}
						}
						break;

					case Markup::HTE_A:
					case Markup::HTE_AREA:
					case Markup::HTE_FORM:
						{
							// need to rewrite these URLs, in case they are relative
							unsigned attr_idx;
							const uni_char* attr = (type == Markup::HTE_FORM) ? UNI_L("action") : UNI_L("href");
							uni_char* href = token.GetUnescapedAttributeValueL(attr, &attr_idx);  ANCHOR_ARRAY(uni_char, href);

							if (href)
							{
								URL link_url = g_url_api->GetURL(use_base_url, href);
								const uni_char* link_name = link_url.GetAttribute(URL::KUniName_With_Fragment).CStr();
								if (link_name && !uni_str_eq(link_name, href))
									token.SetAttributeValueL(attr_idx, link_name);
							}
						}
						break;
					}

					if (!supress_output)
					{
						// Write the start token
						out->WriteStringL(UNI_L("<"), 1);
						out->WriteStringL(token.GetNameStr());

						WriteQuotedAttributesL(token, out.get());

						if (is_xml && token.IsSelfClosing())
							out->WriteStringL(UNI_L("/>"), 2);
						else
							out->WriteStringL(UNI_L(">"), 1);
					}
				}
				break;

			case HTML5Token::END_TAG:
				if (suspend_until_endtag == Markup::HTE_STYLE)
				{
					// Parse the script data for links (will also write it to the file)
					SaveCSSWithInlineL(url, fname, force_encoding, suggested_encoding, su_cache, out.get(), &style_src_head, window, main_page);
					style_src_head.DeleteAll();
					suspend_until_endtag = RESET_SUSPEND;
				}
				else
				{
					Markup::Type type = g_html5_name_mapper->GetTypeFromTokenBuffer(token.GetName());
					if (suspend_until_endtag == type)
						suspend_until_endtag = RESET_SUSPEND;
				}

				out->WriteStringL(UNI_L("</"), 2);
				out->WriteStringL(token.GetNameStr());
				out->WriteStringL(UNI_L(">"), 1);
				break;

			case HTML5Token::PROCESSING_INSTRUCTION:
				out->WriteStringL(UNI_L("<?"), 2);
				out->WriteStringL(token.GetNameStr());

				// Replace URL of xml-stylesheet
				if (token.GetName()->IsEqualTo(UNI_L("xml-stylesheet"), 14))
				{
					token.SetElementType(Markup::HTE_PROCINST);
					ReplaceURLAttrL(token, UNI_L("href"), url, use_base_url, dummy_profile, su_cache, main_page, window, frames_only);
				}

				WriteQuotedAttributesL(token, out.get());

				out->WriteStringL(UNI_L("?>"), 2);
				break;

			case HTML5Token::COMMENT:
				{
					out->WriteStringL(UNI_L("<!--"), 4);

					// Write comment without escaping
					HTML5StringBuffer::ContentIterator iter(token.GetData()); ANCHOR(HTML5StringBuffer::ContentIterator, iter);
					unsigned length;
					const uni_char* data_str;
					while ((data_str = iter.GetNext(length)) != NULL)
						out->WriteStringL(data_str, length);

					out->WriteStringL(UNI_L("-->"), 3);
				}
				break;

			case HTML5Token::CHARACTER:
				if (suspend_until_endtag == Markup::HTE_STYLE)
				{
					// Add script data to style_src, so that we can parse it for links later
					HTML5StringBuffer::ContentIterator style_content(token.GetData());
					ANCHOR(HTML5StringBuffer::ContentIterator, style_content);

					const uni_char* data; unsigned length;
					while ((data = style_content.GetNext(length)) != NULL)
						LEAVE_IF_ERROR(style_src_head.AddSrc(data, length, url));
				}
				else if (suspend_until_endtag == Markup::HTE_SCRIPT)
				{
					HTML5StringBuffer::ContentIterator iter(token.GetData()); ANCHOR(HTML5StringBuffer::ContentIterator, iter);

					unsigned length;
					const uni_char* data_str;
					while ((data_str = iter.GetNext(length)) != NULL)
						out->WriteStringL(data_str, length);
				}
				else if (suspend_until_endtag)
					break;
				else
					WriteQuotedCharacterDataL(token.GetData(), out.get());

				break;
			}
		}

		if (!more)
			break;

		url_data_desc->RetrieveDataL(more);

		data_buf = (uni_char*)url_data_desc->GetBuffer();
		data_len = url_data_desc->GetBufSize();
	}

	out->WriteStringL(UNI_L("\n<!-- This document saved from "));
	out->WriteStringL(url.GetAttribute(URL::KUniName).CStr());
	out->WriteStringL(UNI_L(" -->\n"), 5);

	LEAVE_IF_ERROR(out->Close());
}

#undef RESET_SUSPEND

class AutoDeleteStringSet : public OpGenericStringHashTable
{
	public:
		AutoDeleteStringSet() : OpGenericStringHashTable(TRUE) {}
		virtual ~AutoDeleteStringSet() { this->DeleteAll(); }
		/// Will take over ownership of key
		OP_STATUS Add(uni_char* key) { return OpGenericStringHashTable::Add(key, key); }  // use key both for key and data, since we need only get the data pointer when deleting
		void Delete(void* data) { OP_DELETEA((uni_char*)data); }
};


#ifdef MHTML_ARCHIVE_SAVE_SUPPORT
// function to go through a list of links from css files (generated by style module)
// and add each link to MIME file.
static void AddLinksToArchiveL(Head* links, AutoDeleteStringSet* included_urls,
							   Head* to_process, Upload_Multipart* archive)
{
	for (URLLink* link = (URLLink*)links->First(); link; link = (URLLink*)link->Suc())
	{
		URL inline_url = link->url; ANCHOR(URL, inline_url);

		// Don't add again files we've already included
		uni_char* url_str = UniSetNewStr(inline_url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped).CStr());
		if(!url_str)
			LEAVE(OpStatus::ERR_NO_MEMORY);
		if(!SafeTypeToSave(Markup::HTE_STYLE, inline_url) || included_urls->Contains(url_str) || inline_url.Status(TRUE) != URL_LOADED)
		{
			OP_DELETEA(url_str);
			continue;
		}
		included_urls->AddL(url_str, url_str);

		// Add this URL
		if (inline_url.ContentType() != URL_HTML_CONTENT
			&& inline_url.ContentType() != URL_XML_CONTENT
			&& inline_url.ContentType() != URL_WML_CONTENT
			&& inline_url.ContentType() != URL_SVG_CONTENT
		)
		{
			// add as a mime part
			OpStackAutoPtr<Upload_URL> body (OP_NEW_L(Upload_URL, ()));
			body->InitL(inline_url, NULL); //, NULL, ENCODING_BASE64);
			body->SetForceFileName(FALSE);
			body->AccessHeaders().AddParameterL("Content-Location", inline_url.GetAttribute(URL::KName_Username_Password_Hidden_Escaped));
			body->AccessHeaders().ClearAndAddParameterL("Content-Disposition", "inline");
			archive->AddElement(body.release());
		}
		else // add to list of files to parse for links
		{
			URLLink* tmp_url = OP_NEW_L(URLLink, (inline_url));
			tmp_url->Into(to_process);
		}
	}
}

static void GenerateContentIdL(OpString8 &res, const char* string_seed, BOOL mail_message)
{
	// Format: "<op.mhtml.[time-in-ms].[generated-string]@[ip-address]>"
	if (mail_message)
	{
		res.AppendL("<op.");
	}
	else
	{
		res.AppendL("<op.mhtml.");
	}
	double current_time = OpDate::GetCurrentUTCTime();
	LEAVE_IF_ERROR(res.AppendFormat("%.0f", current_time));

	// Append a string generated from string_seed, but only containing hex-digits
	union {
		unsigned char c[8]; // ARRAY OK stighal 2009-05-07
		double d;
		UINT32 i[2];
	} gen_str;
	gen_str.d = current_time;
	UINT i = 0;
	while (string_seed && *string_seed)
	{
		gen_str.c[i++ %8] ^= *string_seed;
		string_seed++;
	}
	LEAVE_IF_ERROR(res.AppendFormat(".%08x%08x", gen_str.i[0], gen_str.i[1]));

	// Append ip, to make it more likely for id to be unique to this host
	res.AppendL("@");
	OpString ip;
	OP_STATUS err = g_op_system_info->GetSystemIp(ip);
	if (err == OpStatus::OK)
	{
		OpString8 ip8;
		ip8.SetUTF8FromUTF16L(ip.CStr());
		res.AppendL(ip8);
	}
	else if (err == OpStatus::ERR_NO_MEMORY)
		LEAVE(err);
	else // If unable to find a hostname use 'localhost'
		res.AppendL("localhost");

	res.AppendL(">");
}

class LinkProcessor : public InlineFinder::LinkHandler
{
	URL* m_containing_url;
	Upload_Multipart* m_archive;
	Head* m_to_process;
	AutoDeleteStringSet* m_included_urls;
	BOOL m_mail_message;

public:
	LinkProcessor(URL* containing_url, Upload_Multipart* archive, Head* to_process, AutoDeleteStringSet* included_urls, BOOL mail_message) :
	  m_containing_url(containing_url), m_archive(archive), m_to_process(to_process), m_included_urls(included_urls), m_mail_message(mail_message) {}

	void LinkFoundL(URL& link_url, Markup::Type type, HTML5TokenWrapper *token);
	void LinkListFoundL(Head& links)
	  { AddLinksToArchiveL(&links, m_included_urls, m_to_process, m_archive); }
};

void LinkProcessor::LinkFoundL(URL& url, Markup::Type type, HTML5TokenWrapper *token)
{
 	if (url.GetAttribute(URL::KLoadStatus, TRUE) != URL_LOADED)
		return;

	if (m_included_urls->Contains(url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped).CStr()))
		return;

	// we should only inline URL's that are of the content id type
	if (m_mail_message && ((URLType) url.GetAttribute(URL::KType,TRUE) != URL_CONTENT_ID) && !g_mime_module.HasContentID(url))
		return;

	// Most files linked to through FRAME, IMG etc. (but NOT A,
	// which we ignore) we just want to include directly. But
	// files containing more links have to be parsed for more
	// files to include
	switch (url.ContentType())
	{
	case URL_HTML_CONTENT:
	case URL_XML_CONTENT:
	case URL_WML_CONTENT:
	case URL_SVG_CONTENT:
		{
			URLLink* tmp_url = OP_NEW_L(URLLink, (url));
			tmp_url->Into(m_to_process);
			break;
		}
	case URL_CSS_CONTENT:
		{
#if defined STYLE_EXTRACT_URLS || !defined NO_SAVE_SUPPORT   // do this until the EXTRACT_URLS API is in style module
			// Have to search CSS files for URLs to also include
			AutoDeleteHead css_links; ANCHOR(AutoDeleteHead, css_links);

			OP_STATUS status = ExtractURLs(*m_containing_url, url, &css_links);

			if (status == OpStatus::OK)
				AddLinksToArchiveL(&css_links, m_included_urls, m_to_process, m_archive);
			else if (status == OpStatus::ERR_NO_MEMORY)  // ignore other errors, just continue saving what we can
				LEAVE(status);
#endif
		}
		// CSS falls through and saves the file itself as other files
	default:
		{
			// add as a mime part
			OpStackAutoPtr<Upload_URL> body (OP_NEW_L(Upload_URL, ()));
			body->InitL(url, NULL); //, NULL, ENCODING_BASE64);
			body->SetForceFileName(!m_mail_message);
			body->AccessHeaders().ClearAndAddParameterL("Content-Disposition", "inline");
			if (!m_mail_message)
			{
				body->AccessHeaders().AddParameterL("Content-Location", url.GetAttribute(URL::KName_Username_Password_Hidden_Escaped));
			}
			else
			{
				OpString8 content_id;
				if (g_mime_module.HasContentID(url))
				{
					g_mime_module.GetContentID(url, content_id);
				}
				else
				{
					url.GetAttribute(URL::KName_Username_Password_Hidden, content_id, URL::KFollowRedirect);
					content_id.Delete(0,4);
					content_id.Insert(0,"<");
					content_id.Append(">");
				}
				body->AccessHeaders().AddParameterL("Content-ID",content_id);
			}

			m_archive->AddElement(body.release());
			break;
		}
	}

	uni_char* url_str = UniSetNewStr(url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped).CStr());
    if (!url_str)
    	LEAVE(OpStatus::ERR_NO_MEMORY);

	OP_STATUS status;
	if ((status = m_included_urls->Add(url_str)) != OpStatus::OK)
	{
		OP_DELETEA(url_str);
		LEAVE(status);
	}
}


OP_STATUS
SaveAsArchiveHelper::Save(URL& url, const uni_char* fname, Window *window)
{
    TRAPD(oom_status, SaveAsArchiveHelper::SaveL(url, fname, window, 0, NULL, NULL));

	return oom_status;
}


OP_STATUS
SaveAsArchiveHelper::SaveAndReturnSize(URL& url, const uni_char* fname, Window *window, unsigned int max_size,
                          unsigned int* page_size, unsigned int* saved_size)
{
    TRAPD(oom_status, SaveAsArchiveHelper::SaveL(url, fname, window, max_size, page_size, saved_size));

	return oom_status;
}

void
SaveAsArchiveHelper::GetArchiveL(URL& top_url, Upload_Multipart& archive, Window *window, BOOL mail_message)
{
	if (top_url.Status(TRUE) == URL_LOADING)
	{
		LEAVE(OpStatus::ERR);
	}

	URL tempurl = top_url.GetAttribute(URL::KMovedToURL, TRUE);
	if (!tempurl.IsEmpty())
		top_url = tempurl;

	archive.InitL("multipart/related");
	if (!mail_message)
	{
		archive.AccessHeaders().AddParameterL("Content-Location", top_url.GetAttribute(URL::KName_Username_Password_Hidden_Escaped));
	}
	// tell archive to start with main document (the file being viewed when saving)
	OpString8 main_content_id;
	GenerateContentIdL(main_content_id, top_url.GetAttribute(URL::KName_Username_Password_Hidden_Escaped).CStr(), mail_message);
	// 	workaround for DSK-241318, several mail clients like Thunderbird and Yahoo! can't handle the start parameter
	if (!mail_message)
	{
		archive.AccessHeaders().AddParameterL("Content-Type", "start", main_content_id.CStr());
	}

	// Add title as Subject of mime-message
#ifdef URL_UPLOAD_QP_SUPPORT
	const uni_char* title = NULL;
	TempBuffer title_buffer; ANCHOR(TempBuffer, title_buffer);
	if (window && window->GetCurrentDoc())
		title = window->GetCurrentDoc()->Title(&title_buffer);
	if (title)
	{
		OpString8 encoding; ANCHOR(OpString8, encoding);
		encoding.SetL("utf-8");
		archive.AccessHeaders().AddQuotedPrintableParameterL("Subject", OpStringC8(), title, encoding, ENCODING_QUOTED_PRINTABLE);
	}
#endif // URL_UPLOAD_QP_SUPPORT

	// Keep a hash of all URL's included, so the same file isn't included several
	// times. Uses the same string as both key and value. This because only
	// values are deleted, so to make sure all the strings are deleted they
	// have to be values.
	AutoDeleteStringSet included_urls; ANCHOR(AutoDeleteStringSet, included_urls);

	URLContentType content_type = top_url.ContentType();
	// most types of files are simply saved without any more processing
	if (content_type != URL_HTML_CONTENT
		&& content_type != URL_XML_CONTENT
#ifdef _WML_SUPPORT_
		&& content_type != URL_WML_CONTENT
#endif
#ifdef SVG_SUPPORT
		&& content_type != URL_SVG_CONTENT
#endif
		)
	{
		// add url as a mime part
		OpStackAutoPtr<Upload_URL> body (OP_NEW_L(Upload_URL, ()));
		body->SetForceFileName(!mail_message);
		uni_char* url_str = UniSetNewStr(top_url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped).CStr());
		included_urls.AddL(url_str, url_str);

		OpString name;
		top_url.GetAttribute(URL::KName_Username_Password_Hidden, name, URL::KFollowRedirect);

		body->InitL(top_url,name);
		body->AccessHeaders().AddParameterL("Content-ID", main_content_id);
		if (!mail_message)
		{
			body->AccessHeaders().ClearAndAddParameterL("Content-Disposition", "inline");
			body->AccessHeaders().AddParameterL("Content-Location", top_url.GetAttribute(URL::KName_Username_Password_Hidden_Escaped));
		}

		archive.AddElement(body.release());
	}
	else  // Now we're left with HTML, XML, WML or SVG. Has to be parsed for links
	{
		// Make a queue (list) of all URLs to process
		AutoDeleteHead to_process; ANCHOR(AutoDeleteHead, to_process);
		URLLink* tmp_url = OP_NEW_L(URLLink, (top_url));
		tmp_url->Into(&to_process);
		uni_char* url_str = UniSetNewStr(top_url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped).CStr());
		if(!url_str)
			LEAVE(OpStatus::ERR_NO_MEMORY);
		included_urls.AddL(url_str, url_str);

		// Process all URLs in queue
		while(!to_process.Empty())
		{
			tmp_url = (URLLink*)to_process.First();
			URL url = tmp_url->url; ANCHOR(URL, url);
			tmp_url->Out();
			OP_DELETE(tmp_url);

			// add main document as mime part
			OpStackAutoPtr<Upload_URL> main_body (OP_NEW_L(Upload_URL, ()));

			if (mail_message)
				main_body->InitL(url, NULL, NULL, ENCODING_QUOTED_PRINTABLE);
			else
				main_body->InitL(url, NULL);

			main_body->SetForceFileName(!mail_message);
			if(!main_content_id.IsEmpty())
			{
				main_body->AccessHeaders().AddParameterL("Content-ID", main_content_id);
				main_content_id.Empty(); // use main content-id only for first document
			}

			if (!mail_message)
			{
				main_body->AccessHeaders().ClearAndAddParameterL("Content-Disposition", "inline");
				main_body->AccessHeaders().AddParameterL("Content-Location", url.GetAttribute(URL::KName_Username_Password_Hidden_Escaped));
			}
			else
			{
				main_body->AccessHeaders().ClearHeader("Content-Disposition");
			}
			archive.AddElement(main_body.release());

			LinkProcessor processor(&url, &archive, &to_process, &included_urls, mail_message);
			TRAPD(status, InlineFinder::ParseLoadedURLForInlinesL(url, &processor, window, TRUE));
			if (OpStatus::IsMemoryError(status))
				LEAVE(status);
		}
	}
}

void
SaveAsArchiveHelper::SaveL(URL& top_url, const uni_char* fname, Window *window,
						   unsigned int max_size/*= 0*/,
                           unsigned int* page_size/*= NULL*/,
						   unsigned int* saved_size/*= NULL*/)
{
	if (!fname)
	{
		LEAVE(OpStatus::ERR);
	}

	// If we're trying to save something which already is a MHTML file then just save the file directly
	if (top_url.GetAttribute(URL::KCacheType) == URL_CACHE_MHTML)
	{
		if (top_url.PrepareForViewing(FALSE) != 0 || top_url.SaveAsFile(fname) != 0)
			LEAVE(OpStatus::ERR);

		return;
	}

	Upload_Multipart archive;
	ANCHOR(Upload_Multipart, archive);

	TRAPD(status,GetArchiveL(top_url, archive, window));

	if (status != OpStatus::OK)
	{
		LEAVE(status);
	}

	if (max_size != 0) SortArchive(archive);

	// save it all to file:
	archive.PrepareUploadL(UPLOAD_8BIT_TRANSFER);
	archive.ResetL();

	if (max_size != 0)
	{
		if (page_size)
			*page_size = static_cast<unsigned>(archive.CalculateLength());

		while (archive.CalculateLength() > max_size)
		{
			Upload_Base* item = archive.Last();
			item->Out();
			delete item;
			if (archive.Empty())
			{
				LEAVE(OpStatus::ERR);
			}
		}

		if (saved_size)
			*saved_size = static_cast<unsigned>(archive.CalculateLength());
	}

	OpFile out_file; ANCHOR(OpFile, out_file);
	LEAVE_IF_ERROR(out_file.Construct(fname));
	LEAVE_IF_ERROR(out_file.Open(OPFILE_WRITE));

	// get a buffer to use:
	uint32 headers_len = archive.CalculateHeaderLength();
	const unsigned int BUFFER_SIZE = headers_len > 4096 ? headers_len : 4096;
	unsigned char* buffer = OP_NEWA(unsigned char, BUFFER_SIZE);
	if(!buffer)
	{
		out_file.Close();
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}
	ANCHOR_ARRAY(unsigned char, buffer);
	BOOL done = FALSE;

	// write header:
	do {
		uint32 buffer_space = BUFFER_SIZE;
		unsigned char* buf_end = archive.OutputHeaders(buffer, buffer_space, done);
		status = out_file.Write(buffer, buf_end-buffer);
		if(status != OpStatus::OK || buf_end==buffer)
		{
			out_file.Close();
			LEAVE(OpStatus::ERR);
		}
	} while(!done);
	// write all content:
	do{
		uint32 buffer_space=BUFFER_SIZE;
		unsigned char* buf_end = archive.OutputContent(buffer, buffer_space, done);
		status = out_file.Write(buffer, buf_end-buffer);
		if(status != OpStatus::OK || buf_end == buffer)
		{
			out_file.Close();
			LEAVE(OpStatus::ERR);
		}
	} while(!done);

	out_file.Close();
}

void SaveAsArchiveHelper::SortArchive(Upload_Multipart &archive)
{
	if (archive.Empty() || !archive.First()->Suc()
        || !archive.First()->Suc()->Suc())
		return;

	Head markup_files;
	Head svg_files;
	Head css_files;
	Head other_files;

	Upload_URL *current;
	// We always want the root document first, and we get it that way, so leave it in the archive
	// For everything else, sort them in different list according to their priority, and then put everthing back together.
	// O(N) in time, O(1) in space, can can't go OOM

	while ((current = static_cast<Upload_URL*>(archive.First()->Suc())) != NULL) // I am not too happy about casting, but it is the only way to get access to GetSrcContentType();
	{
		current->Out();
		switch (current->GetSrcContentType())
		{
		case URL_HTML_CONTENT:
		case URL_XML_CONTENT:
#ifdef _WML_SUPPORT_
		case URL_WML_CONTENT:
#endif /* _WML_SUPPORT_ */
			current->Into(&markup_files);
			break;
#ifdef SVG_SUPPORT
		case URL_SVG_CONTENT:
			current->Into(&svg_files);
			break;
#endif /* SVG_SUPPORT */
		case URL_CSS_CONTENT:
			current->Into(&css_files);
			break;
		default:
			current->Into(&other_files);
			break;
		}
	}

	archive.Append(&markup_files);
	archive.Append(&svg_files);
	archive.Append(&css_files);
	archive.Append(&other_files);
}

#endif // MHTML_ARCHIVE_SAVE_SUPPORT
#endif // SAVE_SUPPORT
