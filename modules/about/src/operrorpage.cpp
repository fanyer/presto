/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef ERROR_PAGE_SUPPORT
#include "modules/about/operrorpage.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/util/htmlify.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_api.h"

#ifdef AB_ERROR_SEARCH_FORM
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#endif // AB_ERROR_SEARCH_FORM

#if defined(AB_ERROR_SEARCH_FORM) || defined(DOC_SEARCH_SUGGESTIONS_SUPPORT)
static OpSearchProviderListener::SearchProviderInfo* GetSearchProvider()
{
	OpSearchProviderListener* listener = g_windowCommanderManager->GetSearchProviderListener();
	OpSearchProviderListener::SearchProviderInfo* info = listener ? listener->OnRequestSearchProviderInfo(OpSearchProviderListener::REQUEST_ERROR_PAGE) : NULL;
	return info;
}

static OP_STATUS expand_search_string(URL& dest_url, OpSearchProviderListener::SearchProviderInfo* info, OpString &url, int query_begin, OpString &formatted_search_key);
#endif // AB_ERROR_SEARCH_FORM || DOC_SEARCH_SUGGESTIONS_SUPPORT

#if !defined ABOUT_PLATFORM_IMPLEMENTS_OPERRORPAGE
/* static */
OP_STATUS OpErrorPage::Create(OpErrorPage **errorPage, URL &url, Str::LocaleString errorcode, URL *ref_url, const OpStringC &internal_error, BOOL show_search_field, const OpStringC &user_text)
{
	OP_ASSERT(errorPage);

	// Call the first-phase constructor
	OpErrorPage *newPage = OP_NEW(OpErrorPage, (url, errorcode, ref_url, show_search_field));

	if (!newPage)
		return OpStatus::ERR_NO_MEMORY;

	// Call the second-phase constructor
	OP_STATUS rc;
	if (OpStatus::IsSuccess(rc = newPage->Construct(internal_error, user_text)))
	{
		*errorPage = newPage;
		return OpStatus::OK;
	}

	OP_DELETE(newPage);
	return rc;
}
#endif // ABOUT_PLATFORM_IMPLEMENTS_OPERRORPAGE

OP_STATUS OpErrorPage::Construct(const OpStringC &internal_error, const OpStringC &user_search)
{
#ifdef AB_ERROR_SEARCH_FORM
	RETURN_IF_ERROR(m_user_search.Set(user_search));
#endif
	return m_internal_error.Set(internal_error);
}

OP_STATUS OpErrorPage::GenerateData()
{
#if defined(DOC_SEARCH_SUGGESTIONS_SUPPORT) || defined(AB_ERROR_SEARCH_FORM)
	BOOL show_search_field = m_show_search_field && g_pccore->GetIntegerPref(PrefsCollectionCore::EnableSearchFieldErrorPage);
#endif

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	// Spec: Show simplified page layout if 1: The search field is to be shown and 2: There is a search url
	if (show_search_field)
	{
		OpSearchProviderListener::SearchProviderInfo* info = GetSearchProvider();
		if (info)
		{
			OpStringC search_url(info->GetUrl());
			if (search_url.HasContent())
			{
				// We depend on script support
				RETURN_IF_ERROR(m_url.SetAttribute(URL::KForceAllowScripts, TRUE));
				// Search engines will normally not accept a request with a referrer as a request that generates revenue.
				// The error page itself is the referer for a click or search from the page so we must turn it off.
				RETURN_IF_ERROR(m_url.SetAttribute(URL::KDontUseAsReferrer, TRUE));

				OpString part1, part2;

				OpString title, description, search_with, learn_more, learn_more_url_string;
				RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ERROR_PAGE_TITLE, title));
				RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ERROR_PAGE_DESCRIPTION, description));
				RETURN_IF_ERROR(g_languageManager->GetString(Str::S_SEARCH_FIELD_SEARCH_WITH, search_with));
				RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ERRORPAGE_SEARCH_LEARN_MORE, learn_more));
				RETURN_IF_ERROR(learn_more_url_string.Set("http://help.opera.com/errorpage/"));

				// Before search field

				// Add suggestion code only if search engine can deal with it, and only if the string
				// doesn't start with https (https -> might be sensitive data in the string).
				if (info->ProvidesSuggestions() && m_user_search.CStr() && !uni_strni_eq(m_user_search.CStr(), "https://", 8))
				{
					RETURN_IF_ERROR(part1.Append("<span id=searchdata style='display:none'>"));
					RETURN_IF_ERROR(AppendHTMLified(&part1, m_user_search.CStr(), m_user_search.Length()));
					RETURN_IF_ERROR(part1.Append("</span>"));
					RETURN_IF_ERROR(part1.Append("\n<script type=\"text/javascript\">\nwindow.searchSuggest(document.getElementById('searchdata').textContent);\n"));
					RETURN_IF_ERROR(part1.Append("function setSearchSuggestions(event) { document.getElementById(\"searchContainer\").innerHTML = event.content;}\n"));
					RETURN_IF_ERROR(part1.Append("window.addEventListener(\"SearchSuggestionsEvent\", setSearchSuggestions, false);</script>\n"));
				}

				RETURN_IF_ERROR(part1.Append("<p>"));
				RETURN_IF_ERROR(AppendHTMLified(&part1, description, description.Length()));
				RETURN_IF_ERROR(part1.Append("</p>\n<h2>"));
				if (info->GetIconUrl())
				{
					RETURN_IF_ERROR(part1.Append("<img src=\""));
					RETURN_IF_ERROR(AppendHTMLified(&part1, info->GetIconUrl(), uni_strlen(info->GetIconUrl())));
					RETURN_IF_ERROR(part1.Append("\">"));
				}

				RETURN_IF_ERROR(search_with.ReplaceAll(UNI_L("%1"), info->GetName(), 1));
				RETURN_IF_ERROR(AppendHTMLified(&part1, search_with, search_with.Length()));
				RETURN_IF_ERROR(part1.Append("</h2>\n"));

				// After search field
				RETURN_IF_ERROR(part2.Append("\n<div id=\"searchContainer\"></div>\n"));

				// Learn more link
				RETURN_IF_ERROR(part2.Append("\n<div id=\"learnmore\">\n<a href=\""));
				RETURN_IF_ERROR(part2.Append(learn_more_url_string));
				RETURN_IF_ERROR(part2.Append("\">"));
				RETURN_IF_ERROR(part2.Append(learn_more));
				RETURN_IF_ERROR(part2.Append("</a>\n</div>\n"));

				// Make content
				RETURN_IF_ERROR(OpenDocument(Str::S_LOADINGFAILED_PAGE_TITLE, PrefsCollectionFiles::StyleErrorFile));
				RETURN_IF_ERROR(OpenBody(m_errorcode == Str::NOT_A_STRING ? Str::S_ERROR_PAGE_TITLE : m_errorcode));

				if(m_internal_error.HasContent())
				{
					OpString show_details;
					RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ERROR_PAGE_SHOW_DETAILS, show_details));
					RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<input type=\"checkbox\" id=\"showdetails\"><label for=\"showdetails\">")));
					RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, show_details.CStr()));
					RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</label>\n\n<p class=\"errorinformation\">")));
					RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, m_internal_error.CStr()));
					RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</p>")));
				}

				RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, part1.CStr()));
				RETURN_IF_ERROR(GenerateSearchForm(m_user_search));
				RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, part2.CStr()));
				return CloseDocument();
			}
		}
		show_search_field = FALSE; // We have an error condition. Do not show search field below
	}
#endif // DOC_SEARCH_SUGGESTIONS_SUPPORT


#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::S_LOADINGFAILED_PAGE_TITLE, PrefsCollectionFiles::StyleErrorFile, FALSE));
#else
	RETURN_IF_ERROR(OpenDocument(Str::S_LOADINGFAILED_PAGE_TITLE, FALSE));
#endif

	/* Start document body */
	Str::LocaleString heading = m_errorcode;
	if (Str::NOT_A_STRING == heading)
	{
		heading = Str::S_LOADINGFAILED_PAGE_TITLE;
	}
	RETURN_IF_ERROR(OpenBody(heading));

	/* Generic error description goes here */
	OpString header_template;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_LOADINGFAILED_PAGE_HELP_HEADER, header_template));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<p>")));
	OpString url_string;
	if (m_ref_url)
	{
		OpString errormessage;
		RETURN_IF_ERROR(m_ref_url->GetAttribute(URL::KUniName, url_string));
		int url_string_length = url_string.Length();
		errormessage.Reserve(url_string_length + 32);
		RETURN_IF_ERROR(errormessage.Set(UNI_L("<cite><a href=\"")));
		RETURN_IF_ERROR(AppendHTMLified(&errormessage, url_string, url_string_length));
		RETURN_IF_ERROR(errormessage.Append(UNI_L("\">")));
		RETURN_IF_ERROR(AppendHTMLified(&errormessage, url_string, url_string_length));
		RETURN_IF_ERROR(errormessage.Append(UNI_L("</a></cite>")));
		m_url.WriteDocumentDataUniSprintf(header_template.CStr(), errormessage.CStr());
	}
	else
	{
		m_url.WriteDocumentDataUniSprintf(header_template.CStr(), UNI_L(""));
	}
	
	
	/* Add the specific error message */
	if (!m_internal_error.IsEmpty())
	{
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</p>\n<p>")));

		// Trim any trailing newlines
		uni_char *p = m_internal_error.DataPtr();
		uni_char *end = p + uni_strlen(p) - 1;
		while ((*end == '\r' || *end == '\n') && end >= p)
		{
			*(end --) = 0;
		}
		
		// The message can be multi-line, so split it with one line per
		// paragraph.
		uni_char *lf;
		while (NULL != (lf = uni_strpbrk(p, UNI_L("\r\n"))))
		{
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, p, lf - p));
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</p>\n<p>")));

			// Fold multiple line breaks.
			p = lf + 1;
			while ('\r' == *p || '\n' == *p)
			{
				++ p;
			}
		}
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, p));
	}
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</p>\n")));

	/* Now add some generic help information */
	if (!g_url_api->OfflineLoadable(static_cast<URLType>(m_url.GetAttribute(URL::KType))))
	{
		/* Generic help for network resources */
		RETURN_IF_ERROR(OpenUnorderedList());
		RETURN_IF_ERROR(AddListItem(Str::S_LOADINGFAILED_PAGE_HELP_NONOPERA));
#ifdef AB_ERROR_DESKTOP_HELP
		RETURN_IF_ERROR(AddListItem(Str::S_LOADINGFAILED_PAGE_HELP_OTHERSW));
		RETURN_IF_ERROR(AddListItem(Str::S_LOADINGFAILED_PAGE_HELP_FIREWALL));
		RETURN_IF_ERROR(AddListItem(Str::S_LOADINGFAILED_PAGE_HELP_PROXY));
#endif
#ifdef AB_ERROR_SYMBIAN_HELP
		if (m_url.Type() == URL_HTTPS && m_errorcode == Str::S_SSL_FATAL_ERROR_MESSAGE)
		{
			RETURN_IF_ERROR(AddListItem(Str::S_LOADINGFAILED_PAGE_HELP_SYMBIAN_CERT));
			const ServerName *server_name = reinterpret_cast<const ServerName *>(m_ref_url->GetAttribute(URL::KServerName, NULL));
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, server_name->UniName()));
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</p>\n")));
		}
#endif
		RETURN_IF_ERROR(CloseList());
	}

#ifdef AB_ERROR_SEARCH_FORM
	if (show_search_field)
		RETURN_IF_ERROR(GenerateSearchForm(url_string, m_url, m_user_search.CStr(), FALSE));
#endif // AB_ERROR_SEARCH_FORM

#ifdef AB_ERROR_HELP_LINK
	if (m_errorcode != Str::SI_ERR_NETWORK_PROBLEM)
	{
		/* And some help links */
		RETURN_IF_ERROR(Heading(Str::S_LOADINGFAILED_PAGE_HELP_NEEDHELP));
		RETURN_IF_ERROR(OpenUnorderedList());
		RETURN_IF_ERROR(AddListItem(Str::S_LOADINGFAILED_PAGE_HELP_HELPFILES));
		RETURN_IF_ERROR(AddListItem(Str::S_LOADINGFAILED_PAGE_HELP_SUPPORT));
		RETURN_IF_ERROR(CloseList());
	}
#endif

	/* Finish document */
	return CloseDocument();
}

#ifdef AB_ERROR_SEARCH_FORM

#include "modules/formats/uri_escape.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/util/url_query_tkn.h"

#define SEARCH_TERMS_PLACEHOLDER UNI_L("%s")
#define NUMBER_OF_HITS_PLACEHOLDER UNI_L("%i")

static BOOL IsStringEmpty(const uni_char* s) { return s == NULL || *s == 0; }

// static
OP_STATUS OpErrorPage::PreprocessUrl(OpString& candidate)
{
	URL url = g_url_api->GetURL(candidate);
	if (url.Type() == URL_FILE)
	{
		candidate.Empty();
		return OpStatus::OK;
	}

	OpString password;
	RETURN_IF_ERROR(url.GetAttribute(URL::KPassWord, password));
	if (password.HasContent())
	{
		RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, candidate));
	}
	return OpStatus::OK;
}

OP_STATUS OpErrorPage::GenerateSearchForm(const OpStringC& url_string, URL& dest_url, const uni_char* user_text, BOOL is_url_illegal)
{
	if (!g_pccore->GetIntegerPref(PrefsCollectionCore::EnableSearchFieldErrorPage))
		return OpStatus::OK;

	OpSearchProviderListener::SearchProviderInfo* info = GetSearchProvider();
	OpSearchProviderListener::SearchProviderInfoAnchor info_anchor(info);
	if (!info)
		return OpStatus::OK;

	OpString url;
	RETURN_IF_ERROR(url.Set(info->GetUrl()));
	if (url.IsEmpty())
		return OpStatus::OK;

	// Determine search key. We prefer user-supplied text
	OpString search_key;
	RETURN_IF_ERROR(search_key.Set(IsStringEmpty(user_text) ? url_string.CStr() : user_text));
	int search_key_length = search_key.Length();
	if (search_key_length == 0)
		return OpStatus::OK;

	OpString formatted_search_key;
	RETURN_OOM_IF_NULL(formatted_search_key.Reserve(search_key_length+1));
	// ReplaceWhitespace will produce a string of at most the same size.
	ReplaceWhitespace(search_key.CStr(), search_key_length, formatted_search_key.CStr(), search_key_length);

	if (!is_url_illegal)
	{
		RETURN_IF_ERROR(PreprocessUrl(formatted_search_key));
		if (formatted_search_key.IsEmpty())
			return OpStatus::OK;
	}

	// "Search for "%1" with %2"
	OpString str_search_for_with;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_LOADINGFAILED_PAGE_SEARCH_SUGGESTION, str_search_for_with));
	RETURN_IF_ERROR(str_search_for_with.ReplaceAll(UNI_L("%1"), info->GetName(), 1));

	OpString hits_string;
	int num_hits = info->GetNumberOfSearchHits();
	if (num_hits > 0)
		RETURN_IF_ERROR(hits_string.AppendFormat(UNI_L("%d"), num_hits));
	RETURN_IF_ERROR(url.ReplaceAll(NUMBER_OF_HITS_PLACEHOLDER, hits_string.CStr() ? hits_string.CStr() : UNI_L("")));

	// this var is used to strip the query part away from the url, else it'll get dropped if submitting a GET form
	int query_begin = url.FindFirstOf('?');
	BOOL placeholder_in_value = FALSE;
	{
		OpStringC se_query(info->IsPost() ? info->GetQuery() : query_begin != KNotFound ? url.CStr()+query_begin : UNI_L(""));
		int first_placeholder = se_query.Find(SEARCH_TERMS_PLACEHOLDER);
		for (; first_placeholder >= 0; first_placeholder--)
		{
			if (se_query[first_placeholder] == '=')
			{
				placeholder_in_value = TRUE;
				break;
			}
			if (se_query[first_placeholder] == '&')
				break;
		}
	}

	if (!placeholder_in_value)
	{
		// Search by showing a single link

		// 1) normalize whitespace in string (done above)
		// 2) url encode everything but replace spaces with +

		OpString encoded_search_key;
		unsigned encoded_search_key_length = UriEscape::GetEscapedLength(formatted_search_key.CStr(), UriEscape::AllUnsafe | UriEscape::UsePlusForSpace);
		RETURN_OOM_IF_NULL(encoded_search_key.Reserve(encoded_search_key_length));
		UriEscape::Escape(encoded_search_key.CStr(), formatted_search_key.CStr(), UriEscape::AllUnsafe | UriEscape::UsePlusForSpace);

		if (info->IsPost() && !IsStringEmpty(info->GetQuery()))
		{
			uni_char c = query_begin == KNotFound ? '?' : '&';
			RETURN_IF_ERROR(url.Append(&c,1));
			RETURN_IF_ERROR(url.Append(info->GetQuery()));
		}

		RETURN_IF_ERROR(url.ReplaceAll(SEARCH_TERMS_PLACEHOLDER, encoded_search_key.CStr()));
		RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("<p class=\"search\"><a href=\"")));
		RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KHTMLify, url.CStr()));
		RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("\">")));
		RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KHTMLify, str_search_for_with.CStr()));

		const uni_char* se_icon = info->GetIconUrl();
		if (!IsStringEmpty(se_icon))
		{
			if (uni_strchr(se_icon, (uni_char)':') != NULL)
			{
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L(" <span class=\"icon\" style=\"background-image:url('")));
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KHTMLify, se_icon));
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("')\"></span>\n</a></p>\n")));
			}
			else
			{
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L(" <span class=\"icon\" style=\"background:-o-skin('")));
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KHTMLify, se_icon));
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("')\"></span>\n</a></p>\n")));
			}
		}
		else
		{
			RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("</a></p>\n")));
		}
	}
	else
	{
		// Search using a form element

		OpString str_search_caption;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_SEARCH_INTERNET_BUTTON_TEXT, str_search_caption));
		RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("<p class=\"search\">\n")));
		RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KHTMLify, str_search_for_with.CStr()));

		RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("</p>\n<form class=\"search\" name=\"search\" method=\"")));
		RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, info->IsPost() ? 	UNI_L("POST") : UNI_L("GET")));
		RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("\" action=\"")));
		RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KHTMLify, url.CStr(), !info->IsPost() && query_begin != KNotFound ? query_begin : KAll));

		if (!IsStringEmpty(info->GetEncoding()))
		{
			RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("\" accept-charset=\"")));
			RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KHTMLify, info->GetEncoding()));
		}
		RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("\">\n<p class=\"search\">")));

		const uni_char* se_icon = info->GetIconUrl();
		if (!IsStringEmpty(se_icon))
		{
			if (uni_strchr(se_icon, (uni_char)':') != NULL)
			{
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L(" <span class=\"icon\" style=\"background-image:url('")));
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KHTMLify, se_icon));
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("')\"></span>\n")));
			}
			else
			{
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L(" <span class=\"icon\" style=\"background:-o-skin('")));
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KHTMLify, se_icon));
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("')\"></span>\n")));
			}
		}

		RETURN_IF_ERROR(expand_search_string(dest_url, info, url, query_begin, formatted_search_key));

		RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("<input value=\"")));
		RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KHTMLify, str_search_caption.CStr()));
		RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("\" type=\"submit\">\n</p></form>\n")));
	}

	return OpStatus::OK;
}
#endif // AB_ERROR_SEARCH_FORM

#if defined(AB_ERROR_SEARCH_FORM) || defined(DOC_SEARCH_SUGGESTIONS_SUPPORT)

// expand the variables in the search string in dest_url
static OP_STATUS expand_search_string(URL& dest_url, OpSearchProviderListener::SearchProviderInfo* info, OpString &url, int query_begin, OpString &formatted_search_key)
{
	// cycle just to prevent repeated code, 1st iteration is the reminder of the url, 2nd the query string
	// all url arguments will be added as hidden
	for (unsigned nn = 0; nn < 2; nn++)
	{
		if (nn == 0 && (info->IsPost() || query_begin == KNotFound)) continue;
		if (nn == 1 && !info->IsPost()) continue;
		UrlQueryTokenizer tokens(nn == 0 ? url.CStr() + query_begin + 1 : info->GetQuery());
		while (tokens.HasContent())
		{
			const uni_char* value = tokens.GetValue();
			if (value && tokens.GetValueLength() != 0)
			{
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("<input name=\"")));
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KHTMLify, tokens.GetName(), tokens.GetNameLength()));
				RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("\" value=\"")));

				value = uni_strstr(value, SEARCH_TERMS_PLACEHOLDER);
				if (value != NULL && value < tokens.GetValue() + tokens.GetValueLength())
				{
					OpString value_replaced;
					RETURN_IF_ERROR(value_replaced.Set(tokens.GetValue(), tokens.GetValueLength()));
					RETURN_IF_ERROR(value_replaced.ReplaceAll(SEARCH_TERMS_PLACEHOLDER, formatted_search_key.CStr()));
					RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KHTMLify, value_replaced));
					RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("\" tabindex=\"1\" type=\"text\">\n")));
				}
				else
				{
					RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KHTMLify, tokens.GetValue(), tokens.GetValueLength()));
					RETURN_IF_ERROR(dest_url.WriteDocumentData(URL::KNormal, UNI_L("\" type=\"hidden\">\n")));
				}
			}

			tokens.MoveNext();
		}
	}

return OpStatus::OK;
}

#endif // AB_ERROR_SEARCH_FORM || DOC_SEARCH_SUGGESTIONS_SUPPORT


#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
OP_STATUS OpErrorPage::GenerateSearchForm(const OpStringC& search_key)
{
	OpSearchProviderListener::SearchProviderInfo* info = GetSearchProvider();
	OpSearchProviderListener::SearchProviderInfoAnchor info_anchor(info);
	if(!info)
		return OpStatus::OK;

	OpString button_text;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_SEARCH_INTERNET_BUTTON_TEXT, button_text));

	int search_key_length = search_key.Length();

	OpString formatted_search_key;
	RETURN_OOM_IF_NULL(formatted_search_key.Reserve(search_key_length+1));
	// ReplaceWhitespace will produce a string of at most the same size.
	ReplaceWhitespace(search_key.CStr(), search_key_length, formatted_search_key.CStr(), search_key_length);

	OpString url;
	RETURN_IF_ERROR(url.Set(info->GetUrl()));

	// Fill in number of hits.
	OpString hits_string;
	int num_hits = info->GetNumberOfSearchHits();
	if (num_hits > 0)
		RETURN_IF_ERROR(hits_string.AppendFormat(UNI_L("%d"), num_hits));
	RETURN_IF_ERROR(url.ReplaceAll(NUMBER_OF_HITS_PLACEHOLDER, hits_string.CStr() ? hits_string.CStr() : UNI_L("")));

	int query_begin = url.FindFirstOf('?');
	BOOL placeholder_in_value = FALSE;
	{
		OpStringC query(info->IsPost() ? info->GetQuery() : query_begin != KNotFound ? url.CStr() + query_begin : UNI_L(""));
		int first_placeholder = query.Find(SEARCH_TERMS_PLACEHOLDER);
		for (; first_placeholder >= 0; first_placeholder--)
		{
			if (query[first_placeholder] == '=')
			{
				placeholder_in_value = TRUE;
				break;
			}
			if (query[first_placeholder] == '&')
				break;
		}
	}

	if (!placeholder_in_value)
	{
		// We do not support link based searching in this search box, but do not treat it as a fatal error
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<form class=\"search\" name=\"search\" method=\"")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, info->IsPost() ? UNI_L("POST") : UNI_L("GET")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("\" action=\"")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, url.CStr(), !info->IsPost() && query_begin != KNotFound ? query_begin : KAll));

	if (!IsStringEmpty(info->GetEncoding()))
	{
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("\" accept-charset=\"")));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, info->GetEncoding()));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("\"")));
	}
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L(" onSubmit=\"window.onSearch(false)\">\n")));

	RETURN_IF_ERROR(expand_search_string(m_url, info, url, query_begin, formatted_search_key));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<button tabindex=\"2\" type=\"submit\">")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, button_text.CStr()));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</button>\n")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</form>\n")));
	return OpStatus::OK;
}
#endif // DOC_SEARCH_SUGGESTIONS_SUPPORT


#endif
