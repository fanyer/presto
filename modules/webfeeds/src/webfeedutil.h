// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef WEBFEEDUTIL_H_
#define WEBFEEDUTIL_H_

#ifdef WEBFEEDS_BACKEND_SUPPORT

class URL;
class FormValue;

/** Namespace for Utility functions used by the webfeeds module */
class WebFeedUtil {
public:
	/** Decode Base64 data.
	 \param input				Input in base 64.
	 \param	decoded				A pointer where a buffer with the result will be put.
	 							This is a allocated buffer, caller is responsible for calling delete[].
	 \param decoded_length		Length of decoded buffer.
	 \return				 OpStatus::ERR_NO_MEMORY if OOM.
							 OpStatus::ERR if input cannot be decoded (most likely because it is not 
							 valid base64)
							 Otherwise OpStatus::OK. */
	static OP_STATUS DecodeBase64(const OpStringC& input, unsigned char*& decoded, UINT& decoded_length);

#ifdef UNUSED
	/** Encode a string as Base64
	 \param input         A pointer to data to encode. Might be binary.
	 \param input_length  Length of the data in first parameter.
	 \param encoded       OpString where the base64 encoded result will be written.
	 \param append        If TRUE then this function will append to the OpString in previous parameter,
	                      if FALSE it will replace the content of the string. */
	static OP_STATUS EncodeBase64(const char* input, UINT input_length, OpString& encoded, BOOL append = FALSE);
#endif // UNUSED

#ifdef OLD_FEED_DISPLAY
	/** Return a human readable representation of the date
	 *  Caller must delete[] string returned
	 *  returns NULL on OOM */
	static uni_char* TimeToString(double date);

	/** Return a human readable representation of the time in short format.
	 *  If the time is the same date, then only a hh:mm timestamp will be returned,
	 *  otherwise only the date and month.
	 *  Caller must delete[] string returned
	 *  returns NULL on OOM */
	static uni_char* TimeToShortString(double date, BOOL no_break_space=FALSE);
#endif // OLD_FEED_DISPLAY

	/** Check if string contains HTML. This is because RSS contains no
	    type information, so content might be plain text or HTML. We check
	    if content contains legal HTML tags or legal character referances
	    (e.g. &amp;), and assume it to be HTML if it does.
	 		\param content		String to check for HTML content
			\param[out] contains_html	True if content is non-empty and contains 
	  									legal HTML tag(s) */
	static BOOL ContentContainsHTML(const uni_char* content);

	static OP_STATUS ParseAuthorInformation(const OpStringC& input, OpString& email, OpString& name);
	static OP_STATUS UnescapeContent(const OpStringC& content, OpString& unescaped);
	static OP_STATUS Replace(OpString& replace_in, const OpStringC& replace_what, const OpStringC& replace_with);
	static int Find(const OpStringC& string, const OpStringC& search_for, const UINT start_index = 0);

	/** Removes all tags from string, leaving just content.  If there are any stray > in content
	 *  they are replaced by &gt; */
	static OP_STATUS StripTags(const uni_char* input, OpString& stripped_output, const uni_char* strip_element = NULL, BOOL remove_content=FALSE);
#ifdef OLD_FEED_DISPLAY
	static OP_STATUS EscapeHTML(const uni_char* input, OpString& escaped_output);
#endif // OLD_FEED_DISPLAY

	static OP_STATUS RelativeToAbsoluteURL(OpString& resolved_url, const OpStringC& relative_url, const OpStringC& base_url);

#ifdef OLD_FEED_DISPLAY
#ifdef WEBFEEDS_DISPLAY_SUPPORT
	static void	WriteGeneratedPageHeaderL(URL& out_url, const uni_char* title);
	static void WriteGeneratedPageFooterL(URL& out_url);
#endif // WEBFEEDS_DISPLAY_SUPPORT
#endif // OLD_FEED_DISPLAY
};

class DOM_Environment;
class ES_Object;
/**
 * Used to keep track of the scripting objects connected to various feeds content.
 */
class EnvToESObjectHash : private OpHashTable
{
public:
	EnvToESObjectHash() : OpHashTable() {}

	OP_STATUS Add(const DOM_Environment* key, ES_Object* data) { return OpHashTable::Add(key, data); }
	OP_STATUS Remove(const DOM_Environment* key, ES_Object** data) { return OpHashTable::Remove(key, reinterpret_cast<void **>(data)); }

	OP_STATUS GetData(const DOM_Environment* key, ES_Object** data) const { return OpHashTable::GetData(key, reinterpret_cast<void **>(data)); }
};

#endif // WEBFEEDS_BACKEND_SUPPORT
#endif /*WEBFEEDUTIL_H_*/
