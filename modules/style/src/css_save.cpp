/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/style/css_save.h"
#include "modules/url/url2.h"

#include "modules/logdoc/savewithinline.h"
#include "modules/util/opfile/unistream.h"
#include "modules/logdoc/datasrcelm.h"
#include "modules/encodings/utility/charsetnames.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"

#include "modules/style/src/css_parser.h"

#ifdef SAVE_SUPPORT

OP_STATUS SaveCSSWithInline(URL& url, const uni_char* fname, const char* force_encoding, const char* parent_encoding, SavedUrlCache* su_cache, UnicodeFileOutputStream* out, DataSrc* src_head, Window* window, BOOL main_page)
{
	OP_STATUS ret;
	TRAP_AND_RETURN_VALUE_IF_ERROR(ret, SaveCSSWithInlineL(url, fname, force_encoding, parent_encoding, su_cache, out, src_head, window, main_page), ret);
	return OpStatus::OK;
}

static void CSSQuoteURL_L(const uni_char* url, OpString& quoted_url)
{
	OP_ASSERT(url && *url);
	uni_char* out = quoted_url.ReserveL(2*uni_strlen(url)+1);
	while (*url)
	{
		switch(*url)
		{
		case ' ':
		case '\t':
		case '(':
		case ',':
		case ')':
		case '\\':
		case '"':
		case '\'':
			*out++ = '\\';
			// fallthrough
		default:
			*out++ = *url++;
		}
	}
	*out = '\0';
}

void SaveCSSWithInlineL(URL& url, const uni_char* fname, const char* force_encoding, const char* parent_encoding, SavedUrlCache* su_cache, UnicodeFileOutputStream* out, DataSrc* src_head, Window* window, BOOL main_page)
{
	BOOL my_out = !out;
	DataSrc my_src_head;
	ANCHOR(DataSrc, my_src_head);
	OpStackAutoPtr<UnicodeFileOutputStream> out_autoptr(0);
	uni_char tmp_filename[256]; /* ARRAY OK 2009-02-12 rune */

	if (my_out)
	{
		OpFileLength file_length;
		url.GetAttribute(URL::KContentSize, &file_length, TRUE);

		unsigned short parent_encoding_id = g_charsetManager->GetCharsetIDL(parent_encoding);
		g_charsetManager->IncrementCharsetIDReference(parent_encoding_id);
		URL_DataDescriptor* desc = url.GetDescriptor(NULL, URL::KFollowRedirect, FALSE, TRUE, window, URL_UNDETERMINED_CONTENT, 0, FALSE, parent_encoding_id);
		g_charsetManager->DecrementCharsetIDReference(parent_encoding_id);

		// If CSS file is empty, just create an empty file and return
		if (file_length == 0 && !desc)
		{
			OpFile out_file; ANCHOR(OpFile, out_file);
			LEAVE_IF_ERROR(out_file.Construct(fname));
			LEAVE_IF_ERROR(out_file.Open(OPFILE_WRITE));
			LEAVE_IF_ERROR(out_file.Close());
			return;
		}
		// If file_length is > 0, and we still don't have a DataDescriptor something is wrong
		if (!desc)
			LEAVE(OpStatus::ERR);

		OpStackAutoPtr<URL_DataDescriptor> desc_autoptr(desc);

		BOOL more = TRUE;
		unsigned long data_len;
		uni_char* data_buf;

		while (more)
		{
			data_len = desc->RetrieveDataL(more);
			data_buf = (uni_char*)desc->GetBuffer();
			if (data_len > 0)
			{
				LEAVE_IF_ERROR(my_src_head.AddSrc(data_buf, UNICODE_DOWNSIZE(data_len), url, TRUE));
				desc->ConsumeData(data_len);
			}
			else
				more = FALSE;
		}
		src_head = &my_src_head;
	}

	if (!src_head->IsEmpty())
	{
		// Create a CSS_Buffer...

		CSS_Buffer css_buf;
		ANCHOR(CSS_Buffer, css_buf);

		if (!css_buf.AllocBufferArrays(src_head->GetDataSrcElmCount()))
		{
			LEAVE(OpStatus::ERR_NO_MEMORY);
		}

		DataSrcElm* src_elm = src_head->First();

		while (src_elm)
		{
			css_buf.AddBuffer(src_elm->GetSrc(), src_elm->GetSrcLen());
			src_elm = src_elm->Suc();
		}

		// ... and a CSS_Lexer.
		CSS_Lexer lexer(&css_buf);

		if (my_out)
		{
			out = OP_NEW(UnicodeFileOutputStream, ());
			if (out == NULL)
			{
				LEAVE(OpStatus::ERR_NO_MEMORY);
			}
			out_autoptr.reset(out);

			BOOL use_force_encoding = FALSE;
			if (force_encoding && *force_encoding &&
				!strni_eq(force_encoding, "AUTODETECT-", 11))
				use_force_encoding = TRUE;
			const char* charset = use_force_encoding
				? force_encoding // Window's forced encoding has priority
				: url.GetAttribute(URL::KMIME_CharSet).CStr(); // Otherwise use URL's encoding

			if (charset == NULL)
				charset = g_pcdisplay->GetDefaultEncoding();

			OP_ASSERT(charset != NULL);
			LEAVE_IF_ERROR(out->Construct(fname, charset));
		}

		YYSTYPE value;

		int next_write_buf_pos = 0;
		int prev_token = 0;
		int token = lexer.Lex(&value);

		while (token != 0)
		{
			switch (token)
			{
			case CSS_TOK_STRING:
				if (prev_token != CSS_TOK_IMPORT_SYM)
					break;
			case CSS_TOK_URI:
				{
					// URI or STRING representing URI.

					// Catch up with file output up to the point where the URI occurs.
					css_buf.WriteToStreamL(out, next_write_buf_pos, value.str.start_pos-next_write_buf_pos);

					// Get the inline url.
					URL inline_url;
					ANCHOR(URL, inline_url);
					inline_url = css_buf.GetURLL(url, value.str.start_pos, value.str.str_len);

					BOOL new_file;
					const uni_char* filename;

					// don't download unsafe URLs, instead make sure
					// they refer to the external resource
					OpString complete_url;
					ANCHOR(OpString, complete_url);
					if (!SafeTypeToSave(Markup::HTE_STYLE, inline_url))
					{
						new_file = FALSE;
						inline_url.GetAttributeL(URL::KUniName_Username_Password_NOT_FOR_UI, complete_url);
						filename = complete_url.CStr();
					}
					else
					{
						filename = su_cache->GetSavedFilename(inline_url, new_file);

						if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::SaveUseSubfolder))
							if (main_page)
							{
								uni_strncpy(tmp_filename, su_cache->GetReldirStart(), 255);
								tmp_filename[255] = 0;
								filename = tmp_filename;
							}
					}

					if (new_file)
						SaveWithInlineHelper::SaveL(inline_url, NULL, su_cache->GetPathname(), force_encoding, su_cache, window, FALSE, FALSE);

					if (filename)
					{
						// Write the filename to the stream.
						OpString css_quoted_filename; ANCHOR(OpString, css_quoted_filename);
						CSSQuoteURL_L(filename, css_quoted_filename);
						out->WriteStringL(css_quoted_filename.CStr());
					}

					// Update the position in the source file to copy from.
					next_write_buf_pos = value.str.start_pos + value.str.str_len;
				}
				break;
			default:
				break;
			}

			if (token != CSS_TOK_SPACE)
				prev_token = token;

			token = lexer.Lex(&value);
		}

		css_buf.WriteToStreamL(out, next_write_buf_pos, -1);
	}
}
#endif // SAVE_SUPPORT

#if defined STYLE_EXTRACT_URLS
OP_STATUS ExtractURLs(URL& base_url, URL& css_url, AutoDeleteHead* link_list)
{
	RETURN_IF_LEAVE(ExtractURLsL(base_url, css_url, link_list));

	return OpStatus::OK;
}

void ExtractURLsL(URL& base_url, URL& css_url, AutoDeleteHead* link_list)
{
	DataSrc src_head; ANCHOR(DataSrc, src_head);

	URL_DataDescriptor* desc = css_url.GetDescriptor(NULL, URL::KFollowRedirect, FALSE, TRUE);
	if (!desc)
		LEAVE(OpStatus::ERR);

	OpStackAutoPtr<URL_DataDescriptor> desc_autoptr(desc);

	BOOL more = TRUE;
	unsigned long data_len;
	uni_char* data_buf;

	// Build up a list of DataSrcElm containing the source
	while (more)
	{
		data_len = desc->RetrieveDataL(more);
		data_buf = (uni_char*)desc->GetBuffer();
		if (data_len > 0)
		{
			LEAVE_IF_ERROR(src_head.AddSrc(data_buf, UNICODE_DOWNSIZE(data_len), css_url, TRUE));
			desc->ConsumeData(data_len);
		}
		else
			more = FALSE;
	}

	// Extract links from source
	ExtractURLsL(css_url, &src_head, link_list);
}

OP_STATUS ExtractURLs(URL& url, DataSrc* src_head, AutoDeleteHead* link_list)
{
	RETURN_IF_LEAVE(ExtractURLsL(url, src_head, link_list));

	return OpStatus::OK;
}

void ExtractURLsL(URL& url, DataSrc* src_head, AutoDeleteHead* link_list)
{
	OP_ASSERT(src_head && link_list);

	if (!src_head->IsEmpty())
	{
		// Create a CSS_Buffer...
		CSS_Buffer css_buf;
		ANCHOR(CSS_Buffer, css_buf);

		if (!css_buf.AllocBufferArrays(src_head->GetDataSrcElmCount()))
		{
			LEAVE(OpStatus::ERR_NO_MEMORY);
		}

		DataSrcElm* src_elm = src_head->First();

		while (src_elm)
		{
			css_buf.AddBuffer(src_elm->GetSrc(), src_elm->GetSrcLen());
			src_elm = src_elm->Suc();
		}

		// ... and a CSS_Lexer.
		CSS_Lexer lexer(&css_buf);
		YYSTYPE value;
		int prev_token = 0;
		int token = lexer.Lex(&value);

		while (token != 0)
		{
			switch (token)
			{
			case CSS_TOK_STRING:
				if (prev_token != CSS_TOK_IMPORT_SYM)
					break;
			case CSS_TOK_URI:
				{
					// URI or STRING representing URI.

					// Get the inline url.
					URL inline_url;
					ANCHOR(URL, inline_url);
					inline_url = css_buf.GetURLL(url, value.str.start_pos, value.str.str_len);

					// Check that we haven't already added this URL
					BOOL already_included = FALSE;
					for (URLLink* link = (URLLink*)link_list->First(); link; link = (URLLink*)link->Suc())
					{
						URL l_url = link->url;
						if (l_url == inline_url)
						{
							already_included = TRUE;
							break;
						}
					}
					if (already_included)
						break;

					// Add it to list:
					URLLink* tmp_url = OP_NEW_L(URLLink, (inline_url));
					tmp_url->Into(link_list);

					// If it is a CSS file then recursivly parse it as well:
					if (inline_url.ContentType() == URL_CSS_CONTENT)
						ExtractURLsL(inline_url, inline_url, link_list);
				}
				break;
			default:
				break;
			}

			if (token != CSS_TOK_SPACE)
				prev_token = token;

			token = lexer.Lex(&value);
		}
	}
}
#endif // STYLE_EXTRACT_URLS
