/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if !defined MODULES_ABOUT_OPERRORPAGE_H && defined ERROR_PAGE_SUPPORT
#define MODULES_ABOUT_OPERRORPAGE_H

#include "modules/about/opgenerateddocument.h"
#include "modules/locale/locale-enum.h"

/**
 * Generator for the inline error page.
 */
class OpErrorPage : public OpGeneratedDocument
{
public:
	/**
	 * Create and return an OpErrorPage.
	 *
	 * @param errorPage Object to generate, valid if OpStatus::OK is returned.
	 * @param url URL to write to.
	 * @param errorcode Error message code.
	 * @param ref_url URL error message corresponds to.
	 * @param internal_error
	 *   Any internal error text that is to be included verbatim in the page.
	 * @param show_search_field
	 *   If TRUE then we'll give a search field with a suggested search
	 *   to allow the user to find what couldn't be found in the first attempt. This is suitable
	 *   to use if the user was responsible for the broken input. It's not suitable if someone
	 *   else was responsible for it since that would allow an evil site to inject data into an
	 *   opera generated page.
	 * @param user_search
	 *   Text to be included in generated search form (only used if
	 *   AB_ERROR_SEARCH_FORM is defined).
	 */
	static OP_STATUS Create(OpErrorPage **errorPage, URL &url, Str::LocaleString errorcode, URL *ref_url, const OpStringC &internal_error, BOOL show_search_field, const OpStringC &user_search);

	/**
	 * Generate the error document to the specified internal URL. This will
	 * overwrite anything that has already been written to the URL, so the
	 * caller must make sure we do not overwrite anything important.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

#ifdef AB_ERROR_SEARCH_FORM
	/**
	 * Helper method that generates the markup for the search form whenever
	 * the user types in a bad url that generates an error page.
	 * 
	 * @param orig_url          url of the page that was loaded
	 * @param orig_url_length   length in chars of orig_url
	 * @param dest_url          URL object to which the generated
	 *                          data will be written
	 * @param typed_in_url      text typed by the user. Will be used
	 *                          instead of the url in the search field
	 *                          if not empty
	 * @param is_url_illegal    tells if this url is known not to be
	 *                          syntactically valid, so we skip the filtering phase
	 */
	static OP_STATUS GenerateSearchForm(const OpStringC& url_string, URL& dest_url, const uni_char* user_text, BOOL is_url_illegal);

	/**
	 * Helper function:
	 * 1 replaces password in url if any
	 * 2 blanks file urls. Those should not present a search field
	 *
	 * @param candidate The string to examine. It may or may not be a url
	 *
	 * @return OpStatus::OK on sucsess or an error code from URL::GetAttribute()
	 */
	static OP_STATUS PreprocessUrl(OpString& candidate);
#endif //AB_ERROR_SEARCH_FORM



private:
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	/**
	 * Simplified search form generator. 
	 */
	OP_STATUS GenerateSearchForm(const OpStringC& search_key);
#endif

protected:
	/**
	 * First-phase constructor for error page generator.
	 *
	 * @param url URL to write to.
	 * @param errorcode Error message code.
	 * @param ref_url URL error message corresponds to.
	 * @param show_search_field Whether to allow showing a search field or not. If FALSE -> no search field. If TRUE -> Maybe.
	 */
	OpErrorPage(URL &url, Str::LocaleString errorcode, URL *ref_url, BOOL show_search_field)
		: OpGeneratedDocument(url)
		, m_errorcode(errorcode)
		, m_ref_url(ref_url)
		, m_show_search_field(show_search_field)
		{}

	/**
	 * Second-phase constructor for error page generator.
	 *
	 * @param internal_error Internal error string, if any.
	 * @param user_search Default search text, if any.
	 */
	OP_STATUS Construct(const OpStringC &internal_error, const OpStringC &user_search);

	OpString m_internal_error;		///< Internal error message.
	Str::LocaleString m_errorcode;	///< Error message code.
	URL *m_ref_url;					///< URL error message corresponds to.
	BOOL m_show_search_field;
#ifdef AB_ERROR_SEARCH_FORM
	OpString m_user_search;			///< Pre-filled search text.
#endif
};

#endif
