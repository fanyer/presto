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

#include "modules/encodings/utility/charsetnames.h"
#include "modules/encodings/utility/createinputconverter.h"
#include "modules/encodings/utility/opstring-encodings.h"
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/encodings/decoders/identityconverter.h"
#include "modules/encodings/detector/charsetdetector.h"
#include "modules/dochand/win.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/security_manager/include/security_manager.h"

BOOL needsUnicodeConversion(URLContentType content)
{
	switch (content)
	{
	// Pure-text content types:
	case URL_HTML_CONTENT:
#ifdef PREFS_DOWNLOAD
	case URL_PREFS_CONTENT:
#endif
#ifdef SVG_SUPPORT
	case URL_SVG_CONTENT:
#endif
	case URL_XML_CONTENT:
	case URL_WML_CONTENT:
	case URL_PAC_CONTENT:
	case URL_TEXT_CONTENT:
	case URL_CSS_CONTENT:
#ifdef APPLICATION_CACHE_SUPPORT
	case URL_MANIFEST_CONTENT:
#endif
#ifdef JAD_SUPPORT
	case URL_JAD_CONTENT:
#endif // JAD_SUPPORT
#ifdef _ISHORTCUT_SUPPORT
	case URL_ISHORTCUT_CONTENT:
#endif
	case URL_X_JAVASCRIPT:
		return TRUE;
	}

	// Anything else we want to pass through unaltered:
	return FALSE;
}

InputConverter* createInputConverter(
	URL_DataDescriptor* url_data_desc,
	const char* buf, unsigned long len,
	Window *window, unsigned short parent_charset,
	BOOL allowUTF7 /* = FALSE */)
{
	URL url = url_data_desc->GetURL();
	URLContentType content =
		url_data_desc->GetContentType() != URL_UNDETERMINED_CONTENT
			? url_data_desc->GetContentType()
			: url.ContentType();

	/* First catch resources of any types we do not need to convert
	 * to UTF-16. In most cases, these types should be blocked by our
	 * caller; but, if they are not, just set up an IdentityConverter,
	 * which will just copy the input. */
	if (!needsUnicodeConversion(content))
	{
		InputConverter* iconv = OP_NEW(IdentityConverter, ());
		if (iconv && OpStatus::IsError(iconv->Construct())) // Failed to Construct()
		{
			OP_DELETE(iconv);
			iconv = NULL;
		}
		return iconv;
	}

	/* Okay, this must be converted into Unicode. This is the order of
	 * priority used when determining which encoding to use:
	 *
	 * 1. Forced encoding set in the View - Encoding menu
	 *    (not for internally generated documents)
	 * 2. Any BOM (Byte Order Mark) found in the document.
	 * 3. Information from HTTP header
	 *    (includes <meta> tag found by the parser code)
	 * 4. Encoding determined by examining the content
	 *    (data type dependent; <meta>, @charset, etc.)
	 * 5. Any preferred encoding as defined by documents linking here
	 *    (charset attribute of <a>, <link> and <script>), or,
	 *    for CSS and scripts, the encoding of the parent document.
	 *    These are passed through the parent_charset argument.
	 * 6. Encoding autodetect from the binary data stream.
	 *
	 * and if all these fail to find anything, the fallback encoding is used.
	 */

	/* 1. Check if we have overridden the document character set via the
	 * View menu, but not for generated documents. */
	const char *charset = NULL, *force_charset = NULL;
	if (!window) window = url_data_desc->GetDocumentWindow();

	if (!url.GetAttribute(URL::KIsGeneratedByOpera))
	{
		if (window)
		{
			// Get window specific encoding, but only for normal windows.
			if (window->IsNormalWindow()
#ifdef ALLOW_ENCODING_OVERRIDE_IN_MAIL
				|| window->GetType() == WIN_TYPE_MAIL_VIEW
#endif
			   )
			{
				charset = force_charset = window->GetForceEncoding();
			}
		}
		else
		{
			// No window, get default.
			charset = force_charset = g_pcdisplay->GetForceEncoding();
		}

		if (charset && *charset && strni_eq(charset, "AUTODETECT-", 11))
			charset = NULL;
	}

	if (!charset || !*charset)
	{
		// 2. Check for a BOM (CORE-39204)
		charset = CharsetDetector::GetUTFEncodingFromBOM(buf, len);
		if (charset && *charset)
		{
			/* Any encoding found here is canon. Set it explicitly to
			 * prevent autodetection from slowing down loading. */
			OpStatus::Ignore(url.SetAttribute(URL::KMIME_CharSet, charset));
		}
	}

	if (!charset || !*charset)
	{
		// 3. Get the HTTP or <META> information
		charset =
			url_data_desc->GetCharsetID() != 0
				? g_charsetManager->GetCanonicalCharsetFromID(url_data_desc->GetCharsetID())
				: url.GetAttribute(URL::KMIME_CharSet).CStr();

		// Check if this is an auto-detected charset
		const char *used_autodetect = url.GetAttribute(URL::KAutodetectCharSet).CStr();
		const char *current_autodetect = force_charset;
		if (!current_autodetect || !*current_autodetect)
			current_autodetect = "AUTODETECT";
		if (charset && used_autodetect && *used_autodetect &&
			op_strcmp(used_autodetect, current_autodetect) != 0)
		{
			/* We autodetect this with another force setting, so forget
			 * about it. */
			charset = NULL;
			/* We can safely ignore the return value here, since setting a
			 * OpString to NULL cannot cause an OOM condition. */
			OpStatus::Ignore(url.SetAttribute(URL::KMIME_CharSet, NULL));
			OpStatus::Ignore(url.SetAttribute(URL::KAutodetectCharSet, NULL));
		}
	}

	BOOL auto_detect_tried = FALSE;
	BOOL content_detect_tried = FALSE;
	BOOL inherit_tried = FALSE;
	InputConverter *converter = NULL;
	OP_STATUS rc;

	do
	{
		if ((!charset || !*charset) && !content_detect_tried)
		{
			/* 4. The user has not forced an encoding, and the HTTP header
			 * did not specify any. We need to figure the encoding out
			 * ourselves.
			 *
			 * Unless we already tried that, look at what is already
			 * loaded of the document to find encoding information
			 * there. Done for known data types. */
			switch (content)
			{
			case URL_CSS_CONTENT:
				charset = CharsetDetector::GetCSSEncoding(buf, len);
				break;

#ifdef PREFS_DOWNLOAD
			case URL_PREFS_CONTENT:
#endif
#ifdef SVG_SUPPORT
			case URL_SVG_CONTENT:
#endif
			case URL_XML_CONTENT:
			case URL_WML_CONTENT:
				charset = CharsetDetector::GetXMLEncoding(buf, len);
				break;

			case URL_HTML_CONTENT:
				charset = CharsetDetector::GetHTMLEncoding(buf, len);
				break;

			case URL_X_JAVASCRIPT:
				charset = CharsetDetector::GetJSEncoding(buf, len);
				break;
			}

			/* Make sure we do not try again if we do another iteration
			 * (the case where we do not understand the encoding
			 * specified in the document). */
			content_detect_tried = TRUE;

			if (charset && *charset)
			{
				/* Any encoding found here is canon. Set it explicitly to
				 * prevent autodetection from slowing down loading. */
				OpStatus::Ignore(url.SetAttribute(URL::KMIME_CharSet, charset));
			}
		}

		if ((!charset || !*charset) && !inherit_tried)
		{
			/* 5. If we still were unable to find the encoding, try to
			 * inherit the parent encoding passed. This should be the
			 * encoding that was actually used by the parent document
			 * where this makes sense, or an encoding suggested by it,
			 * for instance by way of a CHARSET attribute on a SCRIPT
			 * or LINK tag. */
			if (parent_charset != 0)
			{
				const char *parent_charset_string =
					g_charsetManager->GetCanonicalCharsetFromID(parent_charset);
				/* If UTF-16 was not detected by content detection, then it
				 * should not be used. */
				if (parent_charset_string && !strni_eq(parent_charset_string, "UTF-16", 6))
				{
					charset = parent_charset_string;
				}
			}
			inherit_tried = TRUE;
		}

		if (!charset || !*charset)
		{
			/* 6. We did not find any encoding information in the document
			 * (or we don't know how to get it), so let's see if we can
			 * autodetect the encoding by looking at the byte stream. */
			const ServerName *server =
				static_cast<const ServerName *>(url.GetAttribute(URL::KServerName, (void *) NULL));

			const char *parent_charset_string =
				g_charsetManager->GetCanonicalCharsetFromID(parent_charset);
			CharsetDetector detect(
				server ? server->Name() : NULL,
				window,
				NULL,
				NULL,
				10,
				FALSE,
				parent_charset_string);
			detect.PeekAtBuffer(buf, len);
			charset = detect.GetDetectedCharset();

			/* Autodetection is the last resort, so if we have performed
			 * it, we will not do another iteration. */
			auto_detect_tried = TRUE;
		}

		if (charset)
		{
			/* We now have a name of an encoding to use. Try to create a
			 * converter for this encoding. */
			rc = InputConverter::CreateCharConverter(charset, &converter);
			if (OpStatus::IsMemoryError(rc))
				g_memory_manager->RaiseCondition(rc);

			/* If we couldn't create a converter, forget about the
			 * encoding, which will give autodetection a chance,
			 * if we haven't tried it already. */
			if (!converter)
			{
				charset = NULL;
			}
		}
	} while (!converter && !auto_detect_tried);

	if (converter == NULL)
	{
		/* Still no luck, so we use the fallback. Acecording to RFC 2616
		 * (HTTP/1.1) section 3.7.1, the default for any text MIME type
		 * is ISO 8859-1. However, we let the users specify their own
		 * defaults for the types that do not specify their own default
		 * encoding.
		 */

		switch (content)
		{
		case URL_HTML_CONTENT:
		case URL_CSS_CONTENT:
		case URL_X_JAVASCRIPT:
		case URL_TEXT_CONTENT:
#ifdef _ISHORTCUT_SUPPORT
		case URL_ISHORTCUT_CONTENT:
#endif
			/* For text data types (plain text, HTML, CSS, Javascript,
			 * but not XML, see below) we use the encoding specified
			 * in the configuration as the fallback encoding. */
			charset = g_pcdisplay->GetDefaultEncoding();
			break;

#ifdef PREFS_DOWNLOAD
		case URL_PREFS_CONTENT:
#endif
		case URL_XML_CONTENT:
		case URL_SVG_CONTENT:
			/* The XML specification overrides the HTTP specification,
			 * stating that an undeclared document is UTF-8 encoded. */
			charset = "utf-8";
			break;

		case URL_WML_CONTENT:
			/* From the WML spec, it should inherit the properties
			 * from XML and use UTF-8. However, current practice seems
			 * to be latin-1, so we let it fall through. */
		case URL_PAC_CONTENT:
		default:
			/* For any other content type, we use ISO 8859-1 as per
			 * the HTTP specification. */
			charset = "iso-8859-1";
		}

		rc = InputConverter::CreateCharConverter(charset, &converter);
		if (OpStatus::IsMemoryError(rc))
			g_memory_manager->RaiseCondition(rc);

		if (converter == NULL)
		{
			/* We can't create our fallback either. This is BAD! Someone
			 * probably stole our encoding.bin. Try creating a plain
			 * iso-8559-1 decoder.
			 *
			 * We ignore the status value since we return NULL on error. */
			rc = InputConverter::CreateCharConverter("iso-8859-1", &converter);
			if (OpStatus::IsMemoryError(rc))
				g_memory_manager->RaiseCondition(rc);
		}
	}

	if (converter && !allowUTF7 && strni_eq(converter->GetCharacterSet(), "utf-7", 5))
	{
		OP_DELETE(converter);
		converter = NULL;
	}

	return converter;
}

/**
 * Small helper function;
 * Try to encode, and return true if
 * it was successful.
 */
bool tryConvertWithEncodingOnString(
	const char*	encoding,
	OpString8*  input,
	OpString&	result_string)
{
	if(encoding != 0 && *encoding != 0)
	{
		int invalidChars = 0;
		if(OpStatus::IsSuccess(SetFromEncoding(&result_string, encoding, input->CStr(), input->Length(), &invalidChars)))
		{
			return invalidChars == 0;
		}
	}
	return false;
}

OP_STATUS guessEncodingFromContextAndConvertString(
	OpString8* original_suggested_input,
	OpString8* original_suggested_input_charset,
	const char* document_charset,
	Window*		window,
	ServerName* server_name,
	const char* referrer_url_charset,
	const char* default_encoding,
	const char* system_encoding,
	OpString&	result_string)
{

	const char* encoding;
	BOOL detectedCharsetIsNotUTF8 = FALSE;

	// #1: Try suggested charset
	if(tryConvertWithEncodingOnString(
		original_suggested_input_charset->CStr(),
		original_suggested_input,
		result_string))
	{
		return OpStatus::OK;
	}

	// #2: Try document charset
	if(original_suggested_input_charset &&
		tryConvertWithEncodingOnString(
		document_charset,
		original_suggested_input,
		result_string))
	{
		return OpStatus::OK;
	}

	// #3: Try force encoding from window if its not AUTODETECT-
	if(window != 0)
	{
		encoding = window->GetForceEncoding();
		if(encoding != 0 && !strni_eq(encoding, "AUTODETECT-", 11))
		{
			if(tryConvertWithEncodingOnString(
				encoding,
				original_suggested_input,
				result_string))
			{
				return OpStatus::OK;
			}
		}
	}

	// #4: Try CharsetDetector on original_suggested_input
	if(server_name != 0)
	{
		CharsetDetector detect(
			server_name->GetNameComponent(0),
			window,
			NULL,
			NULL,
			10,
			FALSE,
			referrer_url_charset);
		detect.PeekAtBuffer(original_suggested_input->CStr(), original_suggested_input->Length());
		if(tryConvertWithEncodingOnString(
			detect.GetDetectedCharset(),
			original_suggested_input,
			result_string))
		{
			return OpStatus::OK;
		}
		detectedCharsetIsNotUTF8 = detect.GetDetectedCharsetIsNotUTF8();
	}

	// #5: If possibly UTF-8; try UTF-8
	// We do hardcoded utf-8 test early because of two reasons:
	//	-A: utf-8 is stricter than many other encodings (other encodings accept
	//		utf-8 input while utf-8-encoding often dont accept strings with other encodings)
	//	-B: utf-8 is common and therefore simply works for many cases.
	//
	//	This simply works much better than having the hardcoded utf-8 encoding at the end.
	if(!detectedCharsetIsNotUTF8 &&
		tryConvertWithEncodingOnString(
		"utf-8",
		original_suggested_input,
		result_string))
	{
		return OpStatus::OK;
	}

	// #6: Try system encoding
	// This lets us catch seldom cornercases where user and machine is
	// inside the same encoding area
	if(tryConvertWithEncodingOnString(
		system_encoding,
		original_suggested_input,
		result_string))
	{
		return OpStatus::OK;
	}

	// #7: Try default encoding
	// This lets us catch seldom cornercases where user and machine is
	// inside the same encoding area
	if(tryConvertWithEncodingOnString(
		default_encoding,
		original_suggested_input,
		result_string))
	{
		return OpStatus::OK;
	}

	// #8: Try "iso-8859-1"
	// When everything else fails, lets try iso-8859-1
	if(tryConvertWithEncodingOnString(
		"iso-8859-1",
		original_suggested_input,
		result_string))
	{
		return OpStatus::OK;
	}

	// We did not manage to convert it, lets return an error
	return OpStatus::ERR;
}
