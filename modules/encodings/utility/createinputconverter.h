/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef CREATEINPUTCONVERTER_H
#define CREATEINPUTCONVERTER_H

class URL_DataDescriptor;
class Window;
class InputConverter;
class URL_Rep;
class ServerName;

/**
 * This function creates the appropriate InputConverter object for this
 * particular URL_DataDescriptor instance.
 *
 * Using information from the data descriptor and the contents of the
 * URL (given in the buf and len parameters) this function creates and
 * returns a CharConverter object that can convert the contents of the
 * URL to the internal UTF-16 encoding.
 *
 * If the contents of the URL is not text, an IdentityConverter that
 * does not change the contents will be returned.
 *
 * @param url_data_desc Data descriptor associated with the document.
 * @param buf Buffert containing binary data read from the URL.
 * @param len Length of buf, in bytes.
 * @param window Pointer to window where this URL is loading.
 * @param parent_charset Character encoding passed from the parent
 *   resource. Should either be the actual encoding of the enclosing
 *   resource, or an encoding recommended by it, for instance as set
 *   by a CHARSET attribute in a SCRIPT tag. Pass 0 if no such
 *   contextual information is available.
 * @param allowUTF7 If FALSE (the default), NULL will be returned instead of a
 *                  UTF-7 converter. See bug #313069 for more information.
 * @return Appropriate InputConverter, or NULL if no applicable converter could
 *         be instantiated (e.g. because of OOM)
 */
InputConverter *createInputConverter(
	URL_DataDescriptor *url_data_desc,
	const char *buf, unsigned long len,
	Window *window, unsigned short parent_charset,
	BOOL allowUTF7 = FALSE);

/**
 * This function converts a string to internal format based
 * on a set of 
 *
 * We try encodings in the following order:
 *		#1: Try suggested charset
 *		#2: Try document charset
 *		#3: Try force encoding from window if its not AUTODETECT-
 *		#4: Try CharsetDetector on original_suggested_input
 *		#5: If possibly UTF-8; try UTF-8
 *		#6: Try system encoding
 *		#7: Try default encoding
 *		#8: Try "iso-8859-1"
 *		#9: Return OpStatus:ERR if all fails
 *
 * @param original_suggested_name			The original suggested input
 * @param original_suggested_name_charset	The original suggested charset
 * @param document_charset					The charset of the document
 * @param window							The window, used to find force_encoding + used by CharsetDetector
 * @param server_name						The ServerName object of the url
 * @param referrer_url_charset				The charset of the referrer url
 * @param default_encoding					If UTF8 isnt default, we use this as default encoding
 * @param result_string						A string with the final decoded string
 * @return OK if convertion went ok. ERR if we could not convert.
 */
OP_STATUS guessEncodingFromContextAndConvertString(
	OpString8* original_suggested_input,
	OpString8* original_suggested_input_charset,
	const char* document_charset,
	Window* window, 
	ServerName* server_name,
	const char* referrer_url_charset,
	const char* default_encoding,
	const char* system_encoding,
	OpString&	result_string);

/**
 * Check if the document has a textual content type that needs to
 * be converted to UTF-16 internally.
 *
 * @param content Media type of the document.
 * @return TRUE if the document needs to be converted to UTF-16.
 */
BOOL needsUnicodeConversion(URLContentType content);

#endif
