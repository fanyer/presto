/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef FORMS_FORM_H_
#define FORMS_FORM_H_

#include "modules/url/url2.h"
#include "modules/forms/tempbuffer8.h"
#include "modules/forms/formmanager.h"
#include "modules/forms/formtraversal.h"

class HTML_Element;
class FramesDocument;
class OutputConverter;

class Upload_Multipart;

class Upload_Handler;
class OpGenericStringHashTable;

/**
 * The content type (encoding) string for normal submissions. Used in
 * the desktop-utils so it must remain public for now.
 */
#define FormUrlEncodedString "application/x-www-form-urlencoded"

/**
 * This says that we should be able to post multipart submits. Before, we didn't
 * allow that when file uploads were disabled but that seems wrong. Splitting the
 * _FILE_UPLOAD_SUPPORT_ temporarily into _FILE_UPLOAD_SUPPORT_ and
 * MULTIPART_POST_SUPPORT with the meaning to remove MULTIPART_POST_SUPPORT
 * (always set) when the code is stable and seems to work.
 */
#define MULTIPART_POST_SUPPORT
/**
 * Helper class for form submission related operations.
 */
class Form
	: public FormTraverser
{
private:
	/**
	 * The parent url for this submit. Used for relative URL:s.
	 */
	URL					m_parent_url;
	/**
	 * The HTML Element used to submit. A submit button or image.
	 */
	HTML_Element*		m_submit_he;
	/**
	 * The form element for the form to submit.
	 */
	HTML_Element*		m_form_he;
	/**
	 * Optional search field element, whose value will be replaced by
	 * "%s" in the submit data.
	 */
	HTML_Element*		m_search_field_he;
	/**
	 * The position of the click (x coordinate) that caused the submit.
	 * This is only used when the submit was through an image.
	 */
	int					m_xpos;
	/**
	 * The position of the click (y coordinate) that caused the submit.
	 * This is only used when the submit was through an image.
	 */
	int					m_ypos;

	/**
	 * The frames document the form belongs to.
	 */
	FramesDocument*		m_frames_doc;

	/**
	 * The charset to use. (Can we get this from m_output_converter?)
	 */
	OpString8			m_form_charset;
	/**
	 * The converter from Unicode to the specified charset.
	 */
	OutputConverter*	m_output_converter;

	/**
	 * A buffer containing (after a GetURL call) the query string of the submit.
	 */
	TempBuffer8 m_url_formatted_values;

#ifdef MULTIPART_POST_SUPPORT
	/**
	 * If file upload is performed, this will contain enough information
	 * for the url module to upload a multipart file.
	 */
	Upload_Multipart	*m_upload_data;
#endif // MULTIPART_POST_SUPPORT

	/**
	 * The different ways to submit.
	 */
	enum SubmissionFormat
	{
		/**
		 * The old, normal, default format.
		 */
		URL_ENCODED,
		/**
		 * The old file submit format.
		 */
		MULTIPART_FORM_DATA,
		/**
		 * A new format from Web Forms 2. Plain text.
		 */
		TEXT_PLAIN,
		/**
		 * If the form data shouldn't be collected at all. Used
		 * for instance with method DELETE where nothing is sent.
		 */
		SKIP_FORM_DATA
	};
	/**
	 * The current submit format.
	 */
	SubmissionFormat m_submission_format;

	/**
	 * Action type in the form.
	 */
	enum ActionType
	{
		ACTION_TYPE_OTHER,
		ACTION_TYPE_HTTP,
		ACTION_TYPE_FTP,
		ACTION_TYPE_FILE,
		ACTION_TYPE_DATA,
		ACTION_TYPE_MAILTO,
		ACTION_TYPE_JAVASCRIPT
	};

	/**
	 * Determines which charset to use for the submit. Leaves if something
	 * goes wrong.
	 *
	 * @param frames_doc The FramesDocument where the form resides.
	 * @param he
	 * @param form_charset Will contain the charset to use.
	 * @param formconverter Will contain a pointer to the
	 * character converter to use to encode data for sending.
	 * @param specify_encoding Will tell if the encoding is required to be
	 * specified in the submit or if it will be implicit.
	 */
	void	DetermineCharsetL(FramesDocument* frames_doc,
							  HTML_Element* he,
							  OpString8& form_charset,
							  OutputConverter*& formconverter,
							  BOOL& specify_encoding);

	/**
	 * Used during accept-charset calculations.
	 */
	class CharsetConverterCount
	{
	public:
		/**
		 * The converter to use.
		 */
		OutputConverter* m_converter;
		/**
		 * Count of unsupported charactes encountered.
		 */
		unsigned int m_unsupported_count;

		CharsetConverterCount() : m_converter(NULL), m_unsupported_count(0) {}
		~CharsetConverterCount();
	};


	/**
	 * Counts the number of chars in text that are not supported by
	 * the specified charset.
	 *
	 * @param converter A converter for the charset.
	 * @param text The text. Must not be NULL.
	 *
	 * @returns The number of unsupported chars.
	 */
	unsigned int UnsupportedChars(OutputConverter& converter, const uni_char* text);

	/**
	 * Adds a single value to the submit.
	 *
	 * @param url_format_values The string to grow.
	 * @param he The HTML_Element for the value.
	 * @param force_name If a certain name must be used, it should be
	 * in this string, otherwise NULL.
	 * @param force_value If a certain value must be used, it should be
	 * in this string, otherwise NULL.
	 *
	 */
	void	AddValueL(TempBuffer8& url_format_values, HTML_Element* he, const uni_char* force_name,const uni_char* force_value);

	/**
	 * Adds a single name value pair to the url_format_values string, or whatever
	 * should be used in the current submit.
	 *
	 * @param he The HTML_Element for the value.
	 * @param url_format_values The string to grow.
	 * @param name The name to use.
	 * @param value The name to use.
	 * @param verbatim_value If true, value is added without escaping.
	 *
	 * @todo Replace the use of values with something more flexible.
	 */
	void	AddNameValuePairL(HTML_Element* he,
							  TempBuffer8& url_format_values,
							  const char* name,
							  const char* value,
							  BOOL verbatim_value = FALSE);

	/**
	 * Adds a single name value pair to the values string.
	 *
	 * @param he The HTML_Element for the value.
	 * @param url_format_values The string to grow.
	 * @param name The name to use.
	 * @param value The name to use.
	 * @param verbatim_value If true, value is added without escaping.
	 *
	 * @todo Replace the use of values with something more flexible.
	 */
	void	AddNameValuePairURLEncodedL(HTML_Element* he,
										TempBuffer8& url_format_values,
										const char* name,
										const char* value,
										BOOL verbatim_value = FALSE);

	// From FormTraverser
	virtual void ToFieldValueL(OpString8 &name, const uni_char *name_str);

	virtual void AppendNameValuePairL(FormTraversalContext &context, HTML_Element *he, const char *name, const char *value, BOOL verbatim = FALSE);
	virtual void AppendUploadFilenameL(FormTraversalContext &context, HTML_Element *he, const char *name, const uni_char *filename, const char *encoded_filename);

	/**
	 * Extracts the action (URL) to use from the current form. Leaves if
	 * if fails.
	 *
	 * @param he
	 * @param is_wml_form TRUE if the form is a wml form.
	 * @param action A string that will contain the action.
	 * @param action_type An action type. Basically the scheme of the URL.
	 * @param converter The character converter used for the form data
	 */
	void	GetActionL(HTML_Element* he,
					   BOOL is_wml_form,
					   OpString& action,
					   ActionType& action_type
#ifdef _WML_SUPPORT_
					   , OutputConverter* converter
#endif // _WML_SUPPORT_
					   );

	/**
	 * Helper method for GetActionL.
	 *
	 * @param action_url The url to check.
	 * @returns the action type, or ACTION_TYPE_OTHER if not a known or interesting type.
	 */
	static ActionType ConvertURLToActionType(URL& action_url);


#ifdef SMART_ENCTYPE_PARSER
	/**
	 * Extracts the encoding from the form.
	 * Leaves if it fails miserably. Returns without setting
	 * anything if there was no enctype.
	 *
	 * @param enc_type Will contain the encoding, converted down to
	 * ASCII, with all mime attributes removed.
	 * @param char_set If the enctype contains a charset attribute that
	 * will be returned in this string.
	 */
	void	GetEncTypeL(OpString8& enc_type, OpString8& char_set);
#else
	/**
	 * Extracts the encoding from the form.
	 * Leaves if it fails miserably. Returns without setting
	 * anything if there was no enctype.
	 *
	 * @param enc_type Will contain the encoding, converted down to
	 * ASCII, with all mime attributes removed.
	 */
	void	GetEncTypeL(OpString8& enc_type);
#endif // SMART_ENCTYPE_PARSER

#ifdef _FILE_UPLOAD_SUPPORT_
	/**
	 * Upload support. Adds a file to upload.
	 *
	 * @param he
	 * @param url_format_values The string to grow.
	 * @param name The name of the form element.
	 * @param value The file name.
	 * @param encoded_value A charset converted file name.
	 * @param encoding The encoding.
	 *
	 * @todo Replace the use of values with something more flexible.
	 */
	void	AddUploadFileNamesL(HTML_Element* he,
								TempBuffer8& url_format_values,
								const char* name,
								const uni_char* value,
								const char *encoded_value,
								const char *encoding);

#endif // _FILE_UPLOAD_SUPPORT_

#ifdef MULTIPART_POST_SUPPORT
	/**
	 * Multipart support. Adds a string to a submit
	 *
	 * @param he
	 * @param name The name of the form element.
	 * @param value The string.
	 *
	 * @todo Replace the use of values with something more flexible.
	 */
	void AddUploadNameValuePairURLEncodedL(HTML_Element* he,
										   const char* name,
										   const char* value);

	/**
	 * Prepares a multipart post by creating the necessary object(s). Leaves
	 * if it fails. Can be called even if there will not be a multipart post
	 * and in that case the method doesn't do anything.
	 *
	 * @param method The method to use for the "upload".
	 * @param cnt_type The content type.
	 * @param specify_encoding TRUE is encoding must be specified.
	 */
	void	PrepareMultipartL(HTTP_Method method,
							  const char *cnt_type,
							  BOOL specify_encoding);
#endif // MULTIPART_POST_SUPPORT

	/**
	 * Counts the number of chars required to represent
	 * the string URL escaped.
	 */
	static size_t StrLenEscaped(const char* str);

	/**
	 * Adds a name and value in text/plain submit.
	 *
	 * @param he The input element.
	 * @param url_format_values The string to add the text/plain stuff to.
	 * @param name The name of the element.
	 * @param value An UTF-8 encoded string.
	 *
	 * @todo Remove values.
	 */
	void	AddNameValuePairTextPlainL(HTML_Element* he,
									   TempBuffer8& url_format_values, // XXX badly named
									   const char* name,
									   const char* value) const;

	/**
	 * The method that creates the ISINDEX submit.
	 *
	 * @returns The URL.
	 */
	URL CreateIsIndexSubmitURL_L();

	/**
	 * This is another version of the previous method that shouldn't be used.
	 */
	URL					GetURL_L(FramesDocument* frames_doc, TempBuffer8& url_format_values);

public:
	/**
	 * Constructs a Form object with parent url, form element, submit
	 * element and x and y coordinates for the click.
	 */
						Form(const URL& url, HTML_Element* fhe, HTML_Element* she, int x, int y);
	/**
	 * Destructs the form.
	 */
						~Form();

	/**
	 * Fetch the URL the submit results in. This is the method to use.
	 */
	URL					GetURL(FramesDocument* frames_doc, OP_STATUS& status);


	/**
	 * Construct an URL that is suitable for tooltip display.
	 *
	 * @param[out] display_url The url to show
	 *
	 * @param[in] doc The FramesDocument for this form. Has a default
	 * value for backwards compatibility. It's really required or the
	 * result might be wrong.
	 */
	OP_STATUS			GetDisplayURL(URL& display_url,
									  FramesDocument* doc = NULL);

	/**
	 * Returns the form element.
	 */
	HTML_Element*		GetFormHE() { return m_form_he; };

	/**
	 * Returns the query part of the URL generated by the last
	 * GetURL call. This is used by the quick UI's auto
	 * search.ini generation.
	 *
	 * @param returned_query_string The string which is going to
	 * be set to the query part of the output. The string will
	 * be encoded in the encoding set in the URL object returned
	 * from GetURL.
	 *
	 * @returns OpStatus::OK if there was a query string and it
	 * was returned in the returned_query_string parameter.
	 */
	OP_STATUS GetQueryString(OpString8& returned_query_string);

	/**
	 * Set a search field element for which the string "%s" will be
	 * used instead if its actual value when constructing the form
	 * submit data.  This is useful when automatically creating an
	 * search engine URL from a certain form element.
	 */
	void SetSearchFieldElement(HTML_Element *element) { m_search_field_he = element; }

	/**
	 * Returns the charset used to encode submitted form values.
	 */
	const char *GetCharset() { return m_form_charset.CStr(); }

	/**
	 * Convert a formenctype attribute value to its
	 * enumerated and canonical string representation.
	 */
	static const uni_char *GetFormEncTypeEnumeration(const uni_char* enctype);

	/**
	 * Convert a formmethod attribute value to its
	 * enumerated and canonical string representation.
	 */
	static const uni_char *GetFormMethodEnumeration(const uni_char* method);
};

#endif // FORMS_FORM_H_
