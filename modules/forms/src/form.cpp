/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/forms/form.h"

#include "modules/forms/formsenum.h"
#include "modules/forms/src/formiterator.h"
#include "modules/forms/formmanager.h"

#include "modules/url/url2.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/dochand/win.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/docman.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/forms/piforms.h"
#include "modules/util/str.h"
#include "modules/formats/argsplit.h"
#include "modules/upload/upload.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"

#include "modules/encodings/charconverter.h"
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/hardcore/unicode/unicode.h"
#include "modules/util/opstring.h"
#include "modules/encodings/utility/opstring-encodings.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/windowcommander/src/WindowCommander.h"

#include "modules/forms/formvalue.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/formvalueradiocheck.h"

#ifdef _WML_SUPPORT_
# include "modules/logdoc/wml.h"
#endif // _WML_SUPPORT_

#ifdef FORMS_KEYGEN_SUPPORT
# include "modules/forms/formvaluekeygen.h"
#endif // FORMS_KEYGEN_SUPPORT

#include "modules/util/htmlify.h"

#if defined _FILE_UPLOAD_SUPPORT_ && !defined MULTIPART_POST_SUPPORT
# error Inconsistent defines
#endif // MULTIPART_POST_SUPPORT

#ifdef MULTIPART_POST_SUPPORT

void Form::AddUploadNameValuePairURLEncodedL(HTML_Element* he, const char* name, const char* value)
{
	OpStackAutoPtr<Upload_OpString8> element(OP_NEW_L(Upload_OpString8, ()));

	element->InitL(value, NULL, NULL);

	element->SetContentDispositionL("form-data");
	element->SetContentDispositionParameterL("name",name, TRUE);

	m_upload_data->AddElement(element.release());
}

#endif // MULTIPART_POST_SUPPORT

#ifdef _FILE_UPLOAD_SUPPORT_

void Form::AddUploadFileNamesL(HTML_Element* he, TempBuffer8& url_format_values, const char* name, const uni_char* value, const char *encoded_value, const char *encoding)
{
	// upload data has a value if we decided that we want to submit the file contents
	// otherwise we send the file name
	if (m_upload_data == NULL)
	{
		// Remove the path from the filename when submitting with GET. Sending
		// the path reveals more information than necessary (for
		// instance the username on the computer).
		BOOL quoted = FALSE;
		if (encoded_value)
		{
			const char* last_sep = op_strrchr(encoded_value, PATHSEPCHAR);
			if (last_sep)
			{
				quoted = (*encoded_value == '\"');
				encoded_value = last_sep + 1;
				if (quoted)
				{
					// XXX Ugly hack to temporarily remove the trailing quote
					int len = op_strlen(encoded_value);
					if (len)
					{
						((char*)encoded_value)[len-1] = '\0';
					}
					else
					{
						quoted = FALSE;
					}
				}
			}
		}
		AddNameValuePairL(he, url_format_values, name, encoded_value);
		// Restore the quote if we removed it
		if (quoted)
		{
			((char*)encoded_value)[op_strlen(encoded_value)] = '\"';
		}
		return;
	}

	Upload_Multipart *file_container = m_upload_data;

	// Ignore part of RFC 1867 point 3.3 stating: "If multiple files are selected,
	// they should be transferred together using the multipart/mixed format." and
	// RFC 2388 4.2.: "If the value of a form field is a set of files rather than a single
	// file, that value can be transferred together using the "multipart/mixed" format.
	// in order to work around PHP bugs #50338 and #47789.
	// Instead wrap only once in "multipart/form-data".

	if (!value || !*value)
	{
		OpStackAutoPtr<Upload_OpString8> element(OP_NEW_L(Upload_OpString8, ()));

		element->InitL(NULL, NULL, NULL);

		element->SetContentDispositionL("form-data");
		element->SetContentDispositionParameterL("name", name, TRUE);
		element->SetContentDispositionParameterL("filename", "\"\"");

		file_container->AddElement(element.release());
	}
	else
	{
		OpStackAutoPtr<Upload_URL> element(OP_NEW_L(Upload_URL, ()));
		element->InitL(value, NULL, "form-data", name, encoding);
		file_container->AddElement(element.release());
	}
}

#endif // _FILE_UPLOAD_SUPPORT_

void Form::AddNameValuePairL(HTML_Element* he, TempBuffer8& url_format_values, const char* name, const char* value, BOOL verbatim_value)
{
	// The two formats below (text/plain and URL encoded) handles the
	// _charset_ parameter in their special way so we look if it is
	// that parameter and changes value if it is.
	OpString8 encoded_charset;
	ANCHOR(OpString8, encoded_charset);
	if (he->Type() == HE_INPUT && he->GetInputType() == INPUT_HIDDEN && op_strcmp(name, "_charset_") == 0)
	{
		// This is a magic variable, specified in the web forms 2 specification
		// Need to convert from ASCII to the wanted charset
		OpString unicode_charset;
		ANCHOR(OpString, unicode_charset);
		unicode_charset.SetL(m_form_charset); // This expands from ASCII to UTF16
		SetToEncodingL(&encoded_charset, m_output_converter, unicode_charset.CStr());
		value = encoded_charset.CStr();
	}

	if (m_submission_format == TEXT_PLAIN)
	{
		AddNameValuePairTextPlainL(he, url_format_values, name, value);
	}
	else
	{
		AddNameValuePairURLEncodedL(he, url_format_values, name, value, verbatim_value);
	}
}


void Form::AddNameValuePairTextPlainL(HTML_Element* he, TempBuffer8& url_format_values, const char* name, const char* value) const
{
	// name == "foo" and value == "bar" -> foo=bar with a trailing CRLF

	url_format_values.AppendL(name);
	url_format_values.AppendL("=");
	if (value)
	{
		url_format_values.AppendL(value);
	}
	url_format_values.AppendL("\r\n");
}


void Form::AddNameValuePairURLEncodedL(HTML_Element* he, TempBuffer8& url_format_values, const char* name, const char* value, BOOL verbatim_value)
{
#ifdef MULTIPART_POST_SUPPORT
	if (m_upload_data != NULL)
	{
		AddUploadNameValuePairURLEncodedL(he, name,value);
		return;
	}
#endif // MULTIPART_POST_SUPPORT

	if (url_format_values.Length() > 0)
	{
		LEAVE_IF_ERROR(url_format_values.Append("&"));
	}

	if (name)
	{
		LEAVE_IF_ERROR(url_format_values.AppendURLEncoded(name, TRUE, TRUE));
	}

	LEAVE_IF_ERROR(url_format_values.Append("="));

	if (value)
	{
		if (verbatim_value)
		{
			LEAVE_IF_ERROR(url_format_values.Append(value));
		}
		else
		{
			LEAVE_IF_ERROR(url_format_values.AppendURLEncoded(value, TRUE, TRUE));
		}
	}
}

Form::Form(const URL& url, HTML_Element* fhe, HTML_Element* she, int x, int y)
  : m_parent_url(url),
	m_submit_he(she),
	m_form_he(fhe),
	m_search_field_he(NULL),
	m_xpos(x),
	m_ypos(y),
	m_frames_doc(NULL),
	m_output_converter(NULL)
#ifdef MULTIPART_POST_SUPPORT
	, m_upload_data(NULL)
#endif // MULTIPART_POST_SUPPORT
{
}

Form::~Form()
{
#ifdef MULTIPART_POST_SUPPORT
	OP_DELETE(m_upload_data);
#endif // MULTIPART_POST_SUPPORT

	OP_DELETE(m_output_converter);
}


void Form::AddValueL(TempBuffer8& url_format_values, HTML_Element* he, const uni_char* force_name, const uni_char* force_value)
{
	FormTraversalContext context(m_frames_doc, m_submit_he, m_search_field_he);
	context.force_name = force_name;
	context.force_value = force_value;
	context.output_buffer = &url_format_values;
	context.xpos = m_xpos;
	context.ypos = m_ypos;

	return FormTraversal::TraverseElementL(context, this, he);
}

/* virtual */
void Form::ToFieldValueL(OpString8 &result, const uni_char *name)
{
	SetToEncodingL(&result, m_output_converter, name);
	if (!result.CStr())
	{
		// Can happen if name == "" and maybe in some other case
		result.SetL("");
	}
}

/* virtual */
void Form::AppendNameValuePairL(FormTraversalContext &context, HTML_Element *he, const char *name, const char *value, BOOL verbatim)
{
	AddNameValuePairL(he, *context.output_buffer, name, value, verbatim);
}

/* virtual */
void Form::AppendUploadFilenameL(FormTraversalContext &context, HTML_Element *he, const char *name, const uni_char *file_name, const char *encoded_file_name)
{
#ifdef _FILE_UPLOAD_SUPPORT_
	AddUploadFileNamesL(he, *context.output_buffer, name, file_name, encoded_file_name, m_output_converter->GetDestinationCharacterSet());
#endif // _FILE_UPLOAD_SUPPORT_
}


Form::CharsetConverterCount::~CharsetConverterCount()
{
	OP_DELETE(m_converter);
}

unsigned int Form::UnsupportedChars(OutputConverter& converter, const uni_char* text)
{
	unsigned unsupported_count = 0;
	size_t text_len = uni_strlen(text);

	// The count is done during conversion
	char temp_out_buf[64]; // ARRAY OK bratell 2009-01-26
	const uni_char* text_p = text;
	const uni_char* end_p = text + text_len;
	int read_count;
	while (text_p < end_p)
	{
		(void)converter.Convert(text_p, UNICODE_SIZE(text_len - (text_p-text)),
			temp_out_buf, sizeof(temp_out_buf), &read_count);
		text_p += UNICODE_DOWNSIZE(read_count);
	}

	OP_ASSERT(static_cast<size_t>(converter.LongestSelfContainedSequenceForCharacter()) < sizeof(temp_out_buf));
	converter.ReturnToInitialState(temp_out_buf);
	unsupported_count = converter.GetNumberOfInvalid();
	converter.Reset();

	return unsupported_count;
}

void Form::DetermineCharsetL(FramesDocument* frames_doc, HTML_Element* he,
							 OpString8& form_charset,
							 OutputConverter*& formconverter,
							 BOOL& specify_encoding )
{
	// Figure out which character set to use for the form
	const uni_char *accept_charset = he->GetAcceptCharset();

	// We might have gotten a charset from some other source already
	// Maybe from the enctype. Maybe hard coded.

	// Did the <form> tag include the accept-charset attribute?
	if (form_charset.IsEmpty() && accept_charset && *accept_charset)
	{
		UniParameterList charset_list;
		ANCHOR(UniParameterList, charset_list);
		charset_list.SetValueL(accept_charset, PARAM_SEP_COMMA | PARAM_SEP_WHITESPACE | PARAM_NO_ASSIGN);
		// Count the number of character sets in accept-charset
		// and try to find if the form accepts UTF-8. If so, use it.
		BOOL found_utf8 = FALSE;
		UINT charset_count = charset_list.Cardinal();
		UniParameters* charset_it = charset_list.First();
		while (charset_it && !found_utf8)
		{
			const uni_char* charset_name = charset_it->Name();
			found_utf8 = uni_stri_eq(charset_name, "UTF-8");
			charset_it = charset_it->Suc();
		}

		// If we have UTF-8 we skip all expensive calculation and use it.
		if (found_utf8)
		{
			form_charset.SetL("utf-8");
		}
		else if (charset_count > 0)
		{
			if (1 == charset_count)
			{
				// Form accepts only one charset; use it
				form_charset.SetL(charset_list.First()->Name());
			}
			else // More than one charset
			{
				// If accept-charset gets into wider use, then this is
				// a draft for an implementation that tries to find
				// the character set that can encode the highest
				// number of characters possible from the form.

				// Split the character sets specified up into an array

				CharsetConverterCount* charsets = OP_NEWA_L(CharsetConverterCount, charset_count);
				ANCHOR_ARRAY(CharsetConverterCount, charsets);

				UniParameters* charset_obj = charset_list.First();
				unsigned int i; // Repeat after me: MSVC++ is not ANSI C++
				for (i = 0; i < charset_count; i++, charset_obj = charset_obj->Suc())
				{
					const uni_char* uni_charset_name = charset_obj->Name();
					OP_STATUS status =
						OutputConverter::CreateCharConverter(uni_charset_name, &charsets[i].m_converter);
					if (OpStatus::IsMemoryError(status))
					{
						LEAVE(status);
					}
				}

				FormIterator form_iterator(frames_doc, he);
#ifdef _PLUGIN_SUPPORT_
				form_iterator.SetIncludeObjectElements();
#endif // _PLUGIN_SUPPORT_

				OpString value_buf;
				while (HTML_Element* form_elm = form_iterator.GetNext())
				{
					const uni_char* form_element_data = NULL;
					OpString form_value_text;
#ifdef _PLUGIN_SUPPORT_
					if (form_elm->Type() == HE_OBJECT)
					{
						OpStatus::Ignore(FormManager::GetPluginFormValue(form_elm, form_value_text));
					}
					else
#endif // _PLUGIN_SUPPORT_
					{
						FormValue* form_value = form_elm->GetFormValue();
						if (form_value->GetType() == FormValue::VALUE_LIST_SELECTION)
						{
							FormValueList* list_value = FormValueList::GetAs(form_value);
							OpAutoVector<OpString> values;
							ANCHOR(OpAutoVector<OpString>, values);

							LEAVE_IF_ERROR(list_value->GetAllValues(form_elm, values));

							int value_count = values.GetCount();
							int total_value_size = 0;
							for (int j = 0; j < value_count; j++)
							{
								total_value_size += values.Get(j)->Length();
							}
							form_value_text.ReserveL(total_value_size + 1);
							for (int k = 0; k < value_count; k++)
							{
								form_value_text.Append(*values.Get(k));
							}
						}
						else if (form_value->HasMeaningfulTextRepresentation())
						{
							LEAVE_IF_ERROR(form_value->GetValueAsText(form_elm, form_value_text));
						}
					}
					form_element_data = form_value_text.CStr();

					// Count number of unsupported chars for each charset
					if  (form_element_data)
					{
						for (i = 0; i < charset_count; i ++)
						{
							if (charsets[i].m_converter)
							{
								charsets[i].m_unsupported_count += UnsupportedChars(*charsets[i].m_converter, form_element_data);
							}
						}
					}
				}

				// From the weight table; select
				//  a) any charset that supports all characters
				//  b) the charset that supports the most characters

				int candidate = 0;
				for (i = 0; i < charset_count; i ++)
				{
					// Did we manage to convert the specified string to a converter?
					if (charsets[i].m_converter)
					{
						if (charsets[i].m_unsupported_count == 0)
						{
							candidate = i;
							break;
						}
						if (charsets[candidate].m_converter == NULL ||
							charsets[i].m_unsupported_count < charsets[candidate].m_unsupported_count)
						{
							candidate = i;
						}
					}
				}

				if (charsets[candidate].m_converter)
				{
					form_charset.SetL(charsets[candidate].m_converter->GetDestinationCharacterSet());
				}
			}
		}
	}

	// Fall back to the document encoding
	const char *doc_charset = frames_doc->GetHLDocProfile()->GetCharacterSet();

	// ...unless it was overridden from the menu
	const char *forced_charset = frames_doc->GetWindow()->GetForceEncoding();
	if (forced_charset && *forced_charset)
	{
		if (strni_eq(forced_charset, "AUTODETECT", 10))
		{
			// Ignore autodetect settings, the autodetected value should
			// have been fed back into the doc_charset variable anyway.
			forced_charset = NULL;
		}
		else
		{
			// Bug#177075: If the window has a forced encoding, it should
			// override the document encoding as seen from the form.
			doc_charset = forced_charset;
		}
	}

	// Documents decoded as iso-8859-1 might really
	// be us-ascii. If they are, we do not want to allow any
	// iso-8859-1 characters in the output.
	if (doc_charset)
	{
		if (op_strcmp(doc_charset, "iso-8859-1") == 0 && !(forced_charset && *forced_charset))
		{
			const char *url_charset = frames_doc->GetURL().GetAttribute(URL::KMIME_CharSet).CStr();
			if (url_charset && *url_charset)
			{
				const char *canonic_url_charset =
					g_charsetManager->GetCanonicalCharsetName(url_charset);
				if (canonic_url_charset && canonic_url_charset != url_charset)
				{
					// Only use the URL charset if we actually understand what it is
					doc_charset = canonic_url_charset;
				}
			}
		}
	}
	else
	{
		// doc_charset == NULL, the page was script generated.
		// windows-1252 is a good default. Used to be Latin-1 but
		// HTML5 changed the world. Or rather MSIE did.
		doc_charset = "windows-1252";
	}

	// Canonicalize the form charset to make sure we have something usable. If
	// it is too strange, we fallback to the doc charset.
	const char *form_charset_to_use = NULL;
	if (!form_charset.IsEmpty())
	{
		form_charset_to_use =
			g_charsetManager->GetCanonicalCharsetName(form_charset.CStr());
	}

	if (!form_charset_to_use)
	{
		// Okay, the form did not specify a character set (or an invalid
		// one), so instead we use the document character set.

		// Note that if the character set we get here is invalid, then we will
		// get a windows-1252 encoder automatically.
		form_charset_to_use = doc_charset;
	}

	if (op_strcmp(form_charset_to_use, "iso-8859-1") == 0)
	{
		// Since HTML5 (or MSIE's dominance) iso-8859-1 on the web
		// has come to mean windows-1252
		form_charset_to_use = "windows-1252";
	}


	// Now that we have determined what character set to use, create
	// a converter object that we can pass to the AddValue method.
	OP_STATUS status = OutputConverter::CreateCharConverter(form_charset_to_use, &formconverter);
	if (status == OpStatus::ERR_NOT_SUPPORTED)
	{
		// Encoding not supported by this build, use UTF-8 instead.
		status = OutputConverter::CreateCharConverter("utf-8", &formconverter);
	}
	if (OpStatus::IsError(status))
	{
		LEAVE(status);
	}

	// Check what character set we actually got (if tables are missing,
	// we might get something else than what we requested) or if the name
	// we used wasn't the canonical name.
	form_charset.SetL(formconverter->GetDestinationCharacterSet());

	// If our form encoder uses another character set than the actual
	// document we need to specify that in our HTTP headers. Otherwise
	// we should avoid it. There are scripts
	// out there that can't cope with a charset in the content type.
#ifdef FORMS_FORCE_SPECIFIED_CHARSET
	specify_encoding = TRUE;
#else
	if (form_charset.CompareI(doc_charset) != 0)
	{
		specify_encoding = TRUE;
	}
#endif
}

#ifdef MULTIPART_POST_SUPPORT

void Form::PrepareMultipartL( HTTP_Method method, const char *cnt_type, BOOL specify_encoding )
{
	OP_DELETE(m_upload_data);
	m_upload_data = NULL;

	// I excluded GET requests for this one (stighal)
	if (method != HTTP_METHOD_GET && cnt_type && op_stricmp(cnt_type, "multipart/form-data") == 0 )
	{
		m_upload_data = OP_NEW_L(Upload_Multipart, ());

		m_upload_data->InitL(cnt_type);
		if (specify_encoding)
		{
			m_upload_data->SetCharsetL(m_form_charset.CStr());
		}
	}
}

#endif // MULTIPART_POST_SUPPORT

/* static */
Form::ActionType Form::ConvertURLToActionType(URL& action_url)
{
	// Should probably skip this method and use URLType directly...
	ActionType action_type = ACTION_TYPE_OTHER; // Fallback

	if (!action_url.IsEmpty())
	{
		const URLType action_url_type[] =
		{
			URL_JAVASCRIPT,
			URL_MAILTO,
			URL_DATA,
			URL_HTTP,
			URL_FILE,
			URL_FTP,
		};
		const ActionType types[] =
		{
			ACTION_TYPE_JAVASCRIPT,
			ACTION_TYPE_MAILTO,
			ACTION_TYPE_DATA,
			ACTION_TYPE_HTTP,
			ACTION_TYPE_FILE,
			ACTION_TYPE_FTP,
		};

		OP_ASSERT(sizeof(action_url_type)/sizeof(*action_url_type) == sizeof(types)/sizeof(*types));

		const size_t action_url_type_count = sizeof(action_url_type)/(sizeof(*action_url_type));
		URLType url_type = action_url.Type();
		for (unsigned int i = 0; i < action_url_type_count; i++)
		{
			if (url_type == action_url_type[i])
			{
				action_type = types[i];
				break;
			}
		}
	}
	return action_type;
}

void Form::GetActionL( HTML_Element* he, BOOL is_wml_form, OpString& action, ActionType& action_type
#ifdef _WML_SUPPORT_
					   , OutputConverter* converter
#endif // _WML_SUPPORT_
					  )
{
	URL action_url;
	LogicalDocument* logdoc = m_frames_doc ? m_frames_doc->GetLogicalDocument() : NULL;
#ifdef _WML_SUPPORT_
	if (is_wml_form)
    {
        const uni_char *tmp_str = he->GetStringAttr(WA_HREF, NS_IDX_WML);

        if (tmp_str)
		{
			if (m_frames_doc)
			{
				WML_Context *wc = m_frames_doc->GetDocManager()->WMLGetContext();
				uni_char *tmp_buf = (uni_char *) g_memory_manager->GetTempBuf();

				wc->SubstituteVars(tmp_str, uni_strlen(tmp_str),
								   tmp_buf, UNICODE_DOWNSIZE(g_memory_manager->GetTempBufLen()),
								   TRUE
								   , converter
								   );

				action.SetL(tmp_buf);
			}
			else
			{
				action.SetL(tmp_str); // No frames_doc, i.e. only get a quick approximation for tooltip display
			}
		}
		else
			action.SetL(UNI_L(""));

		action_url = he->ResolveUrl(action.CStr(), logdoc, ATTR_HREF);
    }
	else
#endif // _WML_SUPPORT_
	{
		HTML_AttrType action_attr = ATTR_ACTION;
		if (m_submit_he && m_submit_he->HasAttr(ATTR_FORMACTION))
		{
			he = m_submit_he; // Use the submit "button" instead
			action_attr = ATTR_FORMACTION;
		}
#if 0 // In WF2 the action attribute is no longer required. This breaks the orbit page in bug 188894
		if (m_frames_doc && m_frames_doc->GetHLDocProfile()->IsXml() && !he->HasAttr(action_attr))
		{
			// The action attribute is required in XHTML. See for instance bug 188894
			// where we submitted when other browsers didn't.
			LEAVE(OpStatus::ERR);
		}
#endif // 0
#if 1
		URL* resolved_action = he->GetUrlAttr(action_attr, NS_IDX_HTML, logdoc);
		if (resolved_action && !resolved_action->IsEmpty())
		{
			// XXX Why send it around as a string? Should use the normal URL methods, though that complicates things for the wml case
			resolved_action->GetAttributeL(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, action);
			action_url = *resolved_action;
		}
#else
		action.SetL(he->GetStringAttr(action_attr));
#endif // 1
	}

	action_type = ConvertURLToActionType(action_url);
}

void Form::GetEncTypeL(OpString8& enc_type
#ifdef SMART_ENCTYPE_PARSER
					   , OpString8& char_set
#endif // SMART_ENCTYPE_PARSER
					   )
{
	const uni_char* encoding = NULL;
	// First look at the submit "button"
	if (m_submit_he)
	{
		encoding = m_submit_he->GetStringAttr(ATTR_FORMENCTYPE);
	}

	// Look at the form if the submit button didn't have the information
	if (!encoding)
	{
		encoding = m_form_he->GetEncType();
	}

	// We can parse enctype="multipart/form-data; charset=utf-8" as
	// enctype="multipart/form-data" and charset being UTF-8, but the
	// other browsers don't so we save some footprint and is more
	// compatible by doing it the simple way. The code is left here
	// for now in case we change our minds.
#ifdef SMART_ENCTYPE_PARSER
	if (encoding)
	{
		UniParameterList encoding_parts;
		ANCHOR(UniParameterList, encoding_parts);
		encoding_parts.SetValueL(encoding, PARAM_SEP_SEMICOLON | PARAM_HAS_RFC2231_VALUES);
		UniParameters* encoding_obj = encoding_parts.First();
		if (!encoding_obj)
		{
			return;
		}

		encoding = encoding_obj->Name();
		if (encoding)
		{
			// We strip down to ASCII since enc type should be ASCII anyway.
			enc_type.SetL(encoding);
		}

		encoding_obj = encoding_obj->Suc();
		for (;encoding_obj; encoding_obj = encoding_obj->Suc())
		{
			const uni_char* param_name = encoding_obj->Name();

			if (param_name)
			{
				const uni_char* param_value = encoding_obj->Value();
				if (uni_str_eq(param_name, "charset"))
				{
					char_set.SetL(param_value);
				}
			}
		}
	}
#else // SMART_ENCTYPE_PARSER
	if (encoding)
	{
		// We strip down to ASCII since enc type should be ASCII anyway.
		enc_type.SetL(encoding);
	}
#endif // SMART_ENCTYPE_PARSER

	// enc_type is case insensitive so we make it lowercase to
	// simplify comparisons.
	if (!enc_type.IsEmpty())
	{
		op_strlwr(enc_type.CStr());
	}
}

OP_STATUS Form::GetDisplayURL(URL& url, FramesDocument* frames_doc /* = NULL */)
{
	m_frames_doc = frames_doc;
	BOOL is_wml_form = FALSE;

#ifdef _WML_SUPPORT_ // stighal {
	is_wml_form = m_form_he->IsMatchingType(WE_GO, NS_WML);
#endif // _WML_SUPPORT_ } stighal
	OpString action;
	ActionType action_type;
#ifdef _WML_SUPPORT_
	TRAPD(status, GetActionL(m_form_he, is_wml_form, action, action_type, NULL));
#else // _WML_SUPPORT_
	TRAPD(status, GetActionL(m_form_he, is_wml_form, action, action_type));
#endif // _WML_SUPPORT_
	if (OpStatus::IsSuccess(status))
	{
		url = g_url_api->GetURL(m_parent_url, action.CStr());
	}
	return status;
}

URL Form::GetURL(FramesDocument* frames_doc, OP_STATUS& status)
{
	URL result;

	OP_ASSERT(m_url_formatted_values.IsEmpty()); // Can only call GetURL once for a given Form object
	TRAPD(r,(result = GetURL_L(frames_doc, m_url_formatted_values)));

	status = r;
	return result;
}


URL Form::CreateIsIndexSubmitURL_L()
{
	// Find the input
	OP_ASSERT(m_form_he);
	OP_ASSERT(m_form_he->Type() == HE_ISINDEX);
	HTML_Element* input_elm = m_form_he->FirstChild();
	while (input_elm && !(input_elm->IsMatchingType(HE_INPUT, NS_HTML) && input_elm->GetInputType() == INPUT_TEXT))
	{
		input_elm = input_elm->Suc();
	}
	if (!input_elm)
	{
		// The HE_ISINDEX was probably created by script or manipulated by script since if
		// it had come from the HTML parser there would have been a form here.
		// And HE_ISINDEX should only work when parsed with the HTML parser
		// so let's just abort.
		return URL();
	}

	OP_ASSERT(input_elm->Type() == HE_INPUT);
	OP_ASSERT(input_elm->GetInputType() == INPUT_TEXT);

	OpString8 query_string;
	ANCHOR(OpString8, query_string);

	// Put the value into encoded_value in latin-1
	OpString unicode_value;
	ANCHOR(OpString, unicode_value);
	FormValue* form_value = input_elm->GetFormValue();
	LEAVE_IF_ERROR(form_value->GetValueAsText(input_elm, unicode_value));
	char* value = query_string.ReserveL(UNICODE_SIZE(unicode_value.Length()+3));

	*(value++) = 1; // Dummy. Two chars to get the right unichar alignment
	*(value++) = '?';

	if (!unicode_value.IsEmpty())
	{
		uni_strcpy((uni_char*)value, unicode_value.CStr());
	}
	else
	{
		*(uni_char*)value = '\0';
	}

	make_singlebyte_in_place(value); // Cut -> latin-1

	// Replace sequences of whitespace with a single '+'
	char* write_p = value;
	const char* read_p = value;
	BOOL skip_ws = TRUE;
	while (*read_p)
	{
		if (uni_isspace(static_cast<unsigned char>(*read_p)))
		{
			if (!skip_ws)
			{
				*(write_p++) = '+';
				skip_ws = TRUE;
			}
		}
		else
		{
			*(write_p++) = *read_p;
			skip_ws = FALSE;
		}
		read_p++;
	}

	if (skip_ws && write_p != value)
		write_p--;  // remove trailing '+'

	*write_p = '\0';

	// Build a query from it. Skip the first byte which is a dummy byte, see above
	const char* query_string_p = query_string.CStr()+1;

	URL isindex_url = g_url_api->GetURL(m_parent_url, query_string_p);
	ANCHOR(URL, isindex_url);
	isindex_url.SetAttributeL(URL::KHTTPIsFormsRequest, TRUE);

	return isindex_url;
}


URL Form::GetURL_L(FramesDocument* frames_doc, TempBuffer8& url_format_values)
{

	OP_ASSERT(frames_doc);
	OP_ASSERT(!m_frames_doc);
	m_frames_doc = frames_doc;
	OP_ASSERT(m_frames_doc->GetHLDocProfile()); // Must be a FramesDocument with a document in it.

	HTML_Element* he = m_form_he;
	if (frames_doc == NULL || he == NULL)
		return URL();

	if (he->Type() == HE_ISINDEX)
	{
		return CreateIsIndexSubmitURL_L();
	}

	BOOL is_wml_form = FALSE;

#ifdef _WML_SUPPORT_ // stighal {
	is_wml_form = he->IsMatchingType(WE_GO, NS_WML);
#endif // _WML_SUPPORT_ } stighal

	if (he->Type() != HE_FORM && !is_wml_form)
	{
		return URL();
	}

	// Determine method (POST/GET/...)
	HTTP_Method method;
	if (m_submit_he && m_submit_he->HasAttr(ATTR_FORMMETHOD))
	{
		// Can't use GetNumAttr since we want to have a non-0 default value
		void* val = m_submit_he->GetAttr(ATTR_FORMMETHOD, ITEM_TYPE_NUM, reinterpret_cast<void*>(HTTP_METHOD_GET));
		method = static_cast<HTTP_Method>(reinterpret_cast<INTPTR>(val));
	}
	else
	{
		method = he->GetMethod();
	}

 	// These are used in the call to FileUploadSetDataL() when
	// _FILE_UPLOAD_SUPPORT_ is defined. [pere 2001-06-08]
	BOOL specify_encoding = FALSE;

	// Try to figure out which character set to actually use for the <form>
	DetermineCharsetL(frames_doc, he, m_form_charset, m_output_converter, specify_encoding);

	// Determine the action string ("url")
	OpString action;
	ANCHOR(OpString,action);

	ActionType action_type;
#ifdef _WML_SUPPORT_
	GetActionL( he, is_wml_form, action, action_type, m_output_converter );
#else // _WML_SUPPORT_
	GetActionL( he, is_wml_form, action, action_type );
#endif // _WML_SUPPORT_

	if (action_type == ACTION_TYPE_JAVASCRIPT || action_type == ACTION_TYPE_DATA)
	{
		// Nothing we'll do will make a difference so just return the action URL
		OP_ASSERT(!action.IsEmpty());
		return g_url_api->GetURL(m_parent_url, action.CStr());
	}

	OpString8 enc_type;
	ANCHOR(OpString8, enc_type);
#ifdef SMART_ENCTYPE_PARSER
	 GetEncTypeL(enc_type, m_form_charset);
#else
	 GetEncTypeL(enc_type);
#endif // SMART_ENCTYPE_PARSER
	const char* encoding = enc_type.CStr();

	// Patch method
	if (action_type == ACTION_TYPE_MAILTO)
	{
		if (method == HTTP_METHOD_PUT || method == HTTP_METHOD_DELETE)
		{
			method = HTTP_METHOD_POST;
		}
	}
	else if (action_type == ACTION_TYPE_DATA)
	{
		if (method == HTTP_METHOD_DELETE)
		{
			method = HTTP_METHOD_POST;
		}
	}
	else if (action_type == ACTION_TYPE_FTP)
	{
		if (method == HTTP_METHOD_POST)
		{
			method = HTTP_METHOD_PUT;
		}
	}

	// Decide submission format
	m_submission_format = URL_ENCODED; // fallback
	// GET always becomes URL_ENCODED
	if (encoding)
	{
		// we first select the natural format and then "patch" it according
		// to action_type and method
		if (method == HTTP_METHOD_POST && op_strcmp(encoding, "multipart/form-data") == 0)
		{
			m_submission_format = MULTIPART_FORM_DATA;
		}
		else if (op_strcmp(encoding, "text/plain") == 0)
		{
			m_submission_format = TEXT_PLAIN;
		}
		else
		{
			// Strange or default encoding. Since we are going to fall back to
			// URL_ENCODED, we make sure that enc_type gets the
			// correct value
			enc_type.Empty(); // Drop the strange/bad enc_type.
			encoding = NULL;
		}
	}

	// Patch submission method (if user specified a strange combination)
	switch(action_type)
	{
	case ACTION_TYPE_HTTP:
		{
			if (method == HTTP_METHOD_GET)
			{
				m_submission_format = URL_ENCODED;
			}
			else if (method == HTTP_METHOD_DELETE)
			{
				m_submission_format = SKIP_FORM_DATA;
			}
		}
		break;
#if 0
	case ACTION_TYPE_JAVASCRIPT:
		{
			if (method == HTTP_METHOD_GET)
			{
				m_submission_format = SKIP_FORM_DATA;
			}
			// These should have been patched away when method was patched above
			OP_ASSERT(method != HTTP_METHOD_DELETE && method != HTTP_METHOD_PUT);
		}
		break;
#endif
	case ACTION_TYPE_FTP:
		{
			if (method == HTTP_METHOD_GET)
			{
				m_submission_format = SKIP_FORM_DATA;
			}
			else if (method == HTTP_METHOD_DELETE)
			{
				m_submission_format = SKIP_FORM_DATA;
			}
		}
		break;
	case ACTION_TYPE_FILE:
		{
			// XXX Security?
			if (method == HTTP_METHOD_DELETE)
			{
				m_submission_format = SKIP_FORM_DATA;
			}
		}
		break;
	default:
		OP_ASSERT(action_type != ACTION_TYPE_JAVASCRIPT &&
			action_type != ACTION_TYPE_DATA); // handled earlier
		// No patch necessary
	}
#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
	// Convert characters outside the target encoding as HTML entities. This
	// was something MSIE started with and Mozilla has adapted. It has
	// many problems but we do it to be compatible with other web browsers.
	BOOL use_entity_encoding = FALSE;
	// Never use entity encoding for text/plain
	if (m_submission_format != TEXT_PLAIN)
	{
		use_entity_encoding = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::UseEntitiesInForms);
	}
	m_output_converter->EnableEntityEncoding(use_entity_encoding);
#endif // ENCODINGS_HAVE_ENTITY_ENCODING

	BOOL have_password = FALSE;

#ifdef MULTIPART_POST_SUPPORT
	PrepareMultipartL(method, encoding, specify_encoding);
#endif // MULTIPART_POST_SUPPORT

	// BEGIN add values to form
#ifdef _WML_SUPPORT_
	if (is_wml_form)
	{
		WML_Context *wc = frames_doc->GetDocManager()->WMLGetContext();
		WMLNewTaskElm *wml_task = wc->GetTaskByElement(he);

		if (wml_task)
		{
			HTML_Element *last_found = NULL;
			const uni_char *name_str = NULL;
			const uni_char *value_str = NULL;

			wml_task->GetFirstExtVar(&last_found, &name_str, &value_str);
			if (last_found)
			{
				int name_buf_len    = UNICODE_DOWNSIZE(g_memory_manager->GetTempBufLen());
				int value_buf_len   = UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2Len());
				uni_char *name_buf  = (uni_char *) g_memory_manager->GetTempBuf();
				uni_char *value_buf = (uni_char *) g_memory_manager->GetTempBuf2();

				while (last_found)
				{
					if (!name_str)
					{
						wml_task->GetNextExtVar(&last_found, &name_str, &value_str);
						continue;
					}

					wc->SubstituteVars(name_str, uni_strlen(name_str), name_buf, name_buf_len, FALSE, m_output_converter);
					if (value_str)
					{
						wc->SubstituteVars(value_str, uni_strlen(value_str), value_buf, value_buf_len, FALSE, m_output_converter);
					}

					AddValueL(url_format_values, he, name_buf, value_buf);

					wml_task->GetNextExtVar(&last_found, &name_str, &value_str);
				}

				wc->SetStatusOff(WS_NOSECWARN); // generate a form secure warning -- since we are submitting data
			}
			else
				wc->SetStatusOn(WS_NOSECWARN); // suppress form-warning if <go>-element have no <postfield>/<setvar>
		}
	}
	else
#endif // _WML_SUPPORT_
	{
		// Iterate over everything in the form, adding to the submit data when
		// something that should be added to the submit data is found.
		if (m_submission_format != SKIP_FORM_DATA)
		{
			FormTraversalContext context(m_frames_doc, m_submit_he, m_search_field_he);
			context.xpos = m_xpos;
			context.ypos = m_ypos;
			context.output_buffer = &url_format_values;
			FormTraversal::TraverseL(context, this, he, &have_password);
		}
	}
	// END add values to form

#ifdef FORMS_LIMIT_FORM_SIZE
	if (!url_format_values.IsEmpty())
	{
		int max_bytes = method == HTTP_METHOD_GET ? g_pcdoc->GetIntegerPref(PrefsCollectionDoc::MaximumBytesFormGet)
			: g_pcdoc->GetIntegerPref(PrefsCollectionDoc::MaximumBytesFormPost);
		int bytes_used = url_format_values.Length();
		if (bytes_used > max_bytes)
		{
			OpString msg;
			if (OpStatus::IsSuccess(g_languageManager->GetString(Str::S_FORM_TOO_LARGE, msg)))
			{
				WindowCommander *wic = frames_doc->GetWindow()->GetWindowCommander();
				wic->GetDocumentListener()->OnGenericError(wic,
					frames_doc->GetURL().GetAttribute(URL::KUniName_Username_Password_Hidden).CStr(),
					msg.CStr());
			}
			return URL();
		}
	}
#endif // FORMS_LIMIT_FORM_SIZE

	URL action_url;
	ANCHOR(URL, action_url);

	OpString8 rel_name;
	ANCHOR(OpString8,rel_name);

	if (!action.IsEmpty())
	{
		action_url = g_url_api->GetURL(m_parent_url, action.CStr());

		OP_ASSERT(action_type != ACTION_TYPE_JAVASCRIPT); // Handled earlier

		int rel_index = action.FindFirstOf('#');
		if (rel_index != KNotFound)
		{
			rel_name.SetL(action.CStr() + rel_index + 1);
		}
	}

	if (action_url.IsEmpty())
	{
		action_url = m_parent_url;
	}

	OpString8 query_string;
	ANCHOR(OpString8, query_string);
#ifdef _WML_SUPPORT_
	if (is_wml_form && !url_format_values.IsEmpty() && method == HTTP_METHOD_GET)
	{
		// In WML we should append the post data to the current query.
		int query_start = action.FindFirstOf('?');
		int current_query_len = 0;
		if (query_start >= 0)
		{
			current_query_len = action.Length() - query_start - 1; // excluding '?'
		}

		int needed_len = current_query_len + url_format_values.Length() + 2;
		char* query_str_p = query_string.ReserveL(needed_len);

		*query_str_p++ = '?';

		if (current_query_len > 0)
		{
			// // FIXME, this does some kind of conversion from UTF16 to
			// ASCII but it's most certainly not right. It's important
			// though that the length doesn't change
			const uni_char* src = action.CStr() + query_start + 1;
			while (*src)
			{
				char c = *src > 127 ? '?' : static_cast<char>(*src); // Keep ASCII, replace the rest with '?'
				*query_str_p++ = c;
				src++;
			}
			*query_str_p++ = '&';
		}

		op_strcpy(query_str_p, url_format_values.GetStorage());
	}
	else
#endif //_WML_SUPPORT_
		if (method == HTTP_METHOD_GET &&
		m_submission_format == URL_ENCODED &&
		action_type != ACTION_TYPE_MAILTO && /* action_type != ACTION_TYPE_JAVASCRIPT &&  */
		!(is_wml_form && url_format_values.IsEmpty()))
	{
		OP_ASSERT(action_type != ACTION_TYPE_JAVASCRIPT); // Handled earlier
		char* query_str_p = query_string.ReserveL(url_format_values.Length()+2);
		*query_str_p = '?';
		op_strcpy(query_str_p + 1, url_format_values.GetStorage());
	}
	else
	{
		query_string.SetL("");
	}
	URL f_url = action_url;
	ANCHOR(URL,f_url);
	if (!query_string.IsEmpty() || !rel_name.IsEmpty() || method != HTTP_METHOD_POST)
	{
		OP_ASSERT(action_type != ACTION_TYPE_JAVASCRIPT); // g_url_api->GetURL doesn't support Javascript base urls.

		f_url = g_url_api->GetURL(action_url, query_string.CStr(), rel_name.CStr(), method == HTTP_METHOD_POST);
	}
	else if (method == HTTP_METHOD_POST)
	{
		OP_ASSERT(rel_name.IsEmpty());
		// These must be made unique and this is the simplest way to do that
		f_url = g_url_api->GetURL(action_url, UNI_L(""), TRUE);
	}

#ifdef _DEBUG
	if (method == HTTP_METHOD_POST)
	{
		OP_ASSERT(f_url.GetAttribute(URL::KUnique));
	}
	else
	{
		// OP_ASSERT(!f_url.GetAttribute(URL::KUnique));
	}
#endif // _DEBUG

#ifdef IMODE_EXTENSIONS
	// utn support
	const uni_char *attr_value = m_form_he->GetAttrValue(UNI_L("UTN"));
	if (attr_value && uni_stri_eq(attr_value, "TRUE"))
	{
		f_url.SetUseUtn(TRUE);
	}
#endif //IMODE_EXTENSIONS

	if (method == HTTP_METHOD_GET)
	{
		f_url.SetAttributeL(URL::KHTTPIsFormsRequest, TRUE);
	}

	f_url.SetAttributeL(URL::KHavePassword, have_password);

	if (method == HTTP_METHOD_POST)
	{
		switch (f_url.Type())
		{
		case URL_HTTP:
		case URL_HTTPS:
			{
				f_url.SetHTTP_Method(HTTP_METHOD_POST);

#ifdef MULTIPART_POST_SUPPORT
				if (m_upload_data != NULL)
				{
					m_upload_data->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION);
					f_url.SetHTTP_Data(m_upload_data, TRUE); // The URL owns m_upload_data now
					m_upload_data = NULL;
				}
				else
#endif	// MULTIPART_POST_SUPPORT
				{
					// It can be empty if the form was empty
					const char* post_data = url_format_values.GetStorage();
					LEAVE_IF_ERROR(f_url.SetHTTP_Data(post_data, TRUE));
				}


#ifdef _FILE_UPLOAD_SUPPORT_
				if (encoding && *encoding)
				{
					LEAVE_IF_ERROR(f_url.SetHTTP_ContentType(encoding));
				}
				else
#endif	// _FILE_UPLOAD_SUPPORT_
				{
					LEAVE_IF_ERROR(f_url.SetHTTP_ContentType(FormUrlEncodedString));
				}

				OpString8 form_cnt_type;	ANCHOR(OpString8,form_cnt_type);
				if (m_submission_format == TEXT_PLAIN)
				{
					form_cnt_type.SetL("text/plain");
				}
				else
				{
					if (specify_encoding)
					{
						OpString8 tmp_str;			ANCHOR(OpString8,tmp_str);
						form_cnt_type.SetL("application/x-www-form-urlencoded; charset=");
						tmp_str.SetL(m_form_charset.CStr());
						form_cnt_type.AppendL(tmp_str);
					}
					else
					{
						form_cnt_type.SetL(FormUrlEncodedString);
					}
				}
				LEAVE_IF_ERROR(f_url.SetHTTP_ContentType(form_cnt_type.CStr())); // XXX This is already done 20 lines up

				f_url.SetAttributeL(URL::KHTTPIsFormsRequest, TRUE);
			}

			break;

		case URL_MAILTO:
			{
				f_url.SetAttributeL(URL::KHTTPIsFormsRequest, TRUE); // Same as for HTTP

				OpString8 mailto;
				ANCHOR(OpString8, mailto);

				const char* url_str = f_url.GetAttribute(URL::KName).CStr(); // pwd in a mail url?
				if (op_strlen(url_str) > 7) // "mailto:"
				{
					mailto.AppendL(url_str + 7); // Everything after "mailto:"
					LEAVE_IF_ERROR(f_url.SetMailTo(mailto.CStr()));
				}

				LEAVE_IF_ERROR(f_url.SetMailSubject("Form posted from Opera"));
#ifdef _FILE_UPLOAD_SUPPORT_
				if (encoding)
				{
					LEAVE_IF_ERROR(f_url.SetHTTP_ContentType(encoding));
				}
				else
#endif	// _FILE_UPLOAD_SUPPORT_
				{
					LEAVE_IF_ERROR(f_url.SetHTTP_ContentType(FormUrlEncodedString));
				}

#ifdef MULTIPART_POST_SUPPORT
				if (m_upload_data != NULL)
				{
					m_upload_data->PrepareUploadL(UPLOAD_7BIT_TRANSFER);
					f_url.SetMailData(m_upload_data); // The URL owns m_upload_data now
					m_upload_data = NULL;
				}
				else
#endif	// MULTIPART_POST_SUPPORT
				{
					LEAVE_IF_ERROR(f_url.SetMailData(url_format_values.GetStorage()));
				}
			}

			break;

#ifdef GADGET_SUPPORT
		case URL_WIDGET:
			f_url.SetAttributeL(URL::KHTTPIsFormsRequest, TRUE);
			break;
#endif // GADGET_SUPPORT

			default:
				break;
		}
	}

	return f_url;

}

OP_STATUS Form::GetQueryString(OpString8& returned_query_string)
{
	return returned_query_string.Set(m_url_formatted_values.GetStorage());
}

/* static */ const uni_char*
Form::GetFormEncTypeEnumeration(const uni_char* enctype)
{
	if (uni_stri_eq(enctype, "text/plain"))
	{
		return UNI_L("text/plain");
	}
	else if (uni_stri_eq(enctype, "multipart/form-data"))
	{
		return UNI_L("multipart/form-data");
	}
	else
	{
		return UNI_L("application/x-www-form-urlencoded");
	}
}

/* static */ const uni_char*
Form::GetFormMethodEnumeration(const uni_char* method)
{
	if (uni_stri_eq(method, "POST"))
	{
		return UNI_L("post");
	}
	else if (uni_stri_eq(method, "DIALOG"))
	{
		return UNI_L("dialog");
	}
	else
	{
		return UNI_L("get");
	}
}
