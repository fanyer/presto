// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 2006-2011 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"
#include "modules/webfeeds/src/webfeedutil.h"

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/locale/oplanguagemanager.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/formats/base64_decode.h"
#include "modules/forms/formvalue.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/url/url_man.h"


//****************************************************************************
//
//	WebFeedUtil
//
//****************************************************************************

#ifdef UNUSED
OP_STATUS WebFeedUtil::EncodeBase64(const char* input, UINT input_length, OpString& encoded, BOOL append)
{
	char* temp_buf = NULL;
	int targetlen = 0;
	// MIME_Encode_SetStr allocates a buffer of appropriate size for temp_buf, has to be deallocated
	MIME_Encode_Error err = MIME_Encode_SetStr(temp_buf, targetlen, input, input_length, NULL, GEN_BASE64_ONELINE);

	if (err == MIME_NO_ERROR)
	{
		OP_STATUS str_err = OpStatus::OK;
		if (append)
			str_err = encoded.Append(temp_buf, targetlen);
		else
			str_err = encoded.Set(temp_buf, targetlen);

		OP_DELETEA(temp_buf);
		return str_err;
	}
	else
	{
		OP_DELETEA(temp_buf);
		return OpStatus::ERR;
	}
}
#endif // UNUSED

OP_STATUS WebFeedUtil::DecodeBase64(const OpStringC& input, unsigned char*& decoded, UINT& decoded_length)
{
	// convert to 8 bit
	OpString8 encoded;
	RETURN_IF_ERROR(encoded.Set(input.CStr()));
	unsigned long length = encoded.Length();
	OP_ASSERT(length);

	// decoded base64 data is 3/4 the size of encoded. add one for
	// round-off errors. Might waste a little space, since base64 often
	// has padding, and no efford is made to calculate size without padding.
	unsigned long decode_buffer_length = (length * 3 / 4) + 1;

	// allocate decoding buffer
	decoded = OP_NEWA(unsigned char, decode_buffer_length);
	if (!decoded)
		return OpStatus::ERR_NO_MEMORY;

	// decode
	BOOL warning = FALSE;
	unsigned long offset = 0;

	decoded_length = GeneralDecodeBase64((const unsigned char*)encoded.CStr(), length, offset, decoded, warning, decode_buffer_length);
	OP_ASSERT(decoded_length <= decode_buffer_length);
	if (warning || offset != length)
	{
		OP_DELETEA(decoded);
		decoded = NULL;
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

#ifdef OLD_FEED_DISPLAY

#define DOUBLE2INT32(x)				((INT32)(UINT32)(x))
#if defined HAS_COMPLEX_GLOBALS
// Use a proper table when possible
#  define ST_HEADER_UNI(name) static const uni_char * const name[] = {
#  define ST_ENTRY(name,value) value,
#  define ST_FOOTER_UNI 0 };
#  define ST_REF(name,x) name[x]
#else
// Otherwise use a function that switches on the index
#  define ST_HEADER_UNI(name) static const uni_char * name(int x) { switch (x) {
#  define ST_ENTRY(name,value) case name : return value;
#  define ST_FOOTER_UNI default: OP_ASSERT(0); return UNI_L("***"); }; }
#  define ST_REF(name,x) name(x)
#endif

ST_HEADER_UNI(months)
    ST_ENTRY(0,UNI_L("January"))
    ST_ENTRY(1,UNI_L("February"))
    ST_ENTRY(2,UNI_L("March"))
    ST_ENTRY(3,UNI_L("April"))
    ST_ENTRY(4,UNI_L("May"))
    ST_ENTRY(5,UNI_L("June"))
    ST_ENTRY(6,UNI_L("July"))
    ST_ENTRY(7,UNI_L("August"))
    ST_ENTRY(8,UNI_L("September"))
    ST_ENTRY(9,UNI_L("October"))
    ST_ENTRY(10,UNI_L("November"))
    ST_ENTRY(11,UNI_L("December"))
ST_FOOTER_UNI

#define MONTHS(x) ST_REF(months,x)

ST_HEADER_UNI(days)
    ST_ENTRY(0,UNI_L("Sunday"))
    ST_ENTRY(1,UNI_L("Monday"))
    ST_ENTRY(2,UNI_L("Tuesday"))
    ST_ENTRY(3,UNI_L("Wednesday"))
    ST_ENTRY(4,UNI_L("Thursday"))
    ST_ENTRY(5,UNI_L("Friday"))
    ST_ENTRY(6,UNI_L("Saturday"))
ST_FOOTER_UNI

#define DAYS(x) ST_REF(days,x)

uni_char* WebFeedUtil::TimeToString(double time)
{
	const UINT string_length = 127;
    uni_char* string = OP_NEWA(uni_char, string_length+1);
    if (!string)
    	return NULL;

    double localtime = OpDate::LocalTime(time);
	localtime += OpDate::DaylightSavingTA(localtime);

    if (uni_snprintf(string, string_length,
					 UNI_L("%02d:%02d, %.3s %02d %.3s %04d"),
                     OpDate::HourFromTime(localtime),
                     OpDate::MinFromTime(localtime),
                     DAYS(OpDate::WeekDay(localtime)),
                     OpDate::DateFromTime(localtime),
                     MONTHS(OpDate::MonthFromTime(localtime)),
                     OpDate::YearFromTime(localtime)
					 ) < 0)
		{
			OP_DELETEA(string);
	        return NULL;
		}

	string[string_length] = 0;

    return string;
}

uni_char* WebFeedUtil::TimeToShortString(double time, BOOL no_break_space)
{
	const UINT string_length = 31;
	uni_char* string = OP_NEWA(uni_char, string_length+1);
	if (!string)
		return NULL;

    double current_time = OpDate::GetCurrentUTCTime();
	double diff = current_time - time;
    double localtime = OpDate::LocalTime(time);
    localtime += OpDate::DaylightSavingTA(localtime);
    int written = 0;
	// Less than 24 hours since time? Use only hour and minute
	if (diff >= 0 && diff < (1000 * 60 * 60 * 24))  // Opdate stores time in milliseconds
	    written = uni_snprintf(string, string_length,
	    						UNI_L("%02d:%02d"),
		    					OpDate::HourFromTime(localtime),
	                     		OpDate::MinFromTime(localtime));
	else // use date and month
		written = uni_snprintf(string, string_length,
	    						UNI_L("%.3s%s%d"),
						 		MONTHS(OpDate::MonthFromTime(localtime)),
						 		(no_break_space ? UNI_L("&nbsp;") : UNI_L(" ")),
	                     		OpDate::DateFromTime(localtime));

	if (written < 0)  // sprintf failed
    {
    	OP_DELETEA(string);
    	return NULL;
    }
	else
	{
		string[string_length] = 0;
	    return string;
	}
}
#endif // OLD_FEED_DISPLAY

static inline BOOL only_latin_alpha(const uni_char* string, unsigned length)
{
	while (length && *string)
	{
		if (!op_isalpha(*string))
			return FALSE;

		length--;
		string++;
	}

	return TRUE;
}

static inline BOOL only_hexnumber(const uni_char* string, unsigned length)
{
	if (*string != '#')
		return FALSE;

	string++;
	length--;

	while (length && *string)
	{
		if (!op_isxdigit(*string))
			return FALSE;

		length--;
		string++;
	}

	return TRUE;
}

BOOL WebFeedUtil::ContentContainsHTML(const uni_char* content)
{
	if (!content || !*content)
		return FALSE;

	const uni_char* cur_char = content;

	const uni_char* tag_name_begin = NULL;       // index of tag start
	const uni_char* character_ref_begin = NULL;  // index of escape sequence

	while (*cur_char)
	{
		// check if contains legal HTML tags
		if (tag_name_begin)
		{
			if (cur_char == tag_name_begin && *cur_char == '/') // ending tag, but still check if it's a legal HTML tag
				++tag_name_begin;

			if (*cur_char == '>' || *cur_char == '/' || uni_isspace(*cur_char))
			{
				const int tag_name_length = cur_char - tag_name_begin;

				Markup::Type tag = HTM_Lex::GetTag(tag_name_begin, tag_name_length);
				if (Markup::IsRealElement(tag) && tag != Markup::HTE_UNKNOWN)  // found a valid HTML tag
					return TRUE;

				tag_name_begin = NULL;
			}
		}
		else if (character_ref_begin != 0 &&
			uni_strncmp(cur_char, UNI_L(";"), 1) == 0)  // possible end of character reference
		{
			// Check if this is a valid character reference.

			const int character_ref_length = cur_char - character_ref_begin;

			if (only_latin_alpha(character_ref_begin, character_ref_length)
				|| only_hexnumber(character_ref_begin, character_ref_length))
			{
				return TRUE;
			}

			character_ref_begin = NULL;
		}
		else if (uni_strncmp(cur_char, UNI_L("<"), 1) == 0)  // start of tag
		{
			tag_name_begin = cur_char + 1;
			character_ref_begin = NULL;
		}
		else if (uni_strncmp(cur_char, UNI_L("&"), 1) == 0)  // start of HTML escape
		{
			character_ref_begin = cur_char + 1;
		}

		++cur_char;
	}

	return FALSE;
}


OP_STATUS WebFeedUtil::StripTags(const uni_char* input, OpString& stripped_output, const uni_char* strip_element, BOOL remove_content)
{
	if (!input)
		return OpStatus::ERR_NULL_POINTER;

	stripped_output.Empty();
	const uni_char* c = input;
	const uni_char* last_start = c;
	UINT nesting_level = 0;
	while (TRUE)
	{
		// find next < and copy all content from last > (or begining)
		while (*c && *c != '<')
			c++;

		UINT length = c-last_start;
		if (length > 0 && (!remove_content || nesting_level == 0))
			RETURN_IF_ERROR(stripped_output.Append(last_start, length));
		last_start = c;

		if (!*c)
			break;

		BOOL is_endtag = FALSE;
		const uni_char* start_tag = c;
		c++;  // skip <
		if (*c == '/')
		{
			is_endtag = TRUE;
			c++;
		}

		while (*c && uni_isspace(*c))  // skip whitespace
			c++;

		const uni_char* tag_start = c;
		while (*c && uni_isalnum(*c))  // get tag name
			c++;

		OpString tag_name;
		if (tag_start != c)
		{
			RETURN_IF_ERROR(tag_name.Set(tag_start, c-tag_start));
			if (strip_element && tag_name.Compare(strip_element) == 0)
			{
				if (!is_endtag)
					nesting_level++;
				else if(nesting_level > 0)
					nesting_level--;
			}
		}

		// find end of tag and discard tag content
		while (*c && *c != '>')
		{
			if (*c == '\'')  // skip quoted section
			{
				c++;
				while (*c && *c != '\'')
					c++;
			}

			if (*c == '"')  // skip quoted section
			{
				c++;
				while (*c && *c != '"')
					c++;
			}

			if (*c)
				c++;
		}

		if (!*c)
		{
			OP_ASSERT(c > start_tag);
			// mismatched tags, include content of mismatched tag, as it probably
			// wasn't meant as a tag. But escape <
			OpString escaped;
			RETURN_IF_ERROR(escaped.Append(start_tag, c-start_tag));
			RETURN_IF_ERROR(WebFeedUtil::Replace(escaped, UNI_L("<"), UNI_L("&lt;")));
			RETURN_IF_ERROR(stripped_output.Append(escaped));

			break;
		}

		if (!strip_element || tag_name.Compare(strip_element) == 0 || (remove_content && nesting_level > 0))
			last_start = c+1;  // discards tag content
	}

	return OpStatus::OK;
}


OP_STATUS WebFeedUtil::Replace(OpString &replace_in,
	const OpStringC& replace_what, const OpStringC& replace_with)
{
	const int replace_what_length = replace_what.Length();
	const int replace_with_length = replace_with.Length();

	int find_pos = WebFeedUtil::Find(replace_in, replace_what, 0);
	while (find_pos != KNotFound)
	{
		replace_in.Delete(find_pos, replace_what_length);
		RETURN_IF_ERROR(replace_in.Insert(find_pos, replace_with));

		find_pos = WebFeedUtil::Find(replace_in, replace_what,
			find_pos + replace_with_length);
	}

	return OpStatus::OK;
}



int WebFeedUtil::Find(const OpStringC& find_in, const OpStringC& find_what,
	UINT find_from)
{
	OP_ASSERT(find_from <= UINT(find_in.Length()));
	const OpStringC& input = find_in.CStr() + find_from;

	// Search for the first occurance.
	int find_pos = input.Find(find_what);

	// Adjust find_pos with respect to what we just cut away.
	if (find_pos != KNotFound)
		find_pos += find_from;

	return find_pos;
}

#ifdef OLD_FEED_DISPLAY
OP_STATUS WebFeedUtil::EscapeHTML(const uni_char* input, OpString& escaped)
{
	RETURN_IF_ERROR(escaped.Set(input));

	RETURN_IF_ERROR(Replace(escaped, UNI_L("&"), UNI_L("&amp;")));
	RETURN_IF_ERROR(Replace(escaped, UNI_L("<"), UNI_L("&lt;")));
	RETURN_IF_ERROR(Replace(escaped, UNI_L(">"), UNI_L("&gt;")));
	RETURN_IF_ERROR(Replace(escaped, UNI_L("\""), UNI_L("&quot;")));
	RETURN_IF_ERROR(Replace(escaped, UNI_L("'"), UNI_L("&apos;")));

	return OpStatus::OK;
}
#endif // OLD_FEED_DISPLAY

OP_STATUS WebFeedUtil::RelativeToAbsoluteURL(OpString& absolute_uri,
	const OpStringC& relative_uri, const OpStringC& base_uri)
{
	// First we check if this allready is a relative uri.
	URL url_rel = urlManager->GetURL(relative_uri.CStr());
	if (url_rel.IsEmpty())
		return OpStatus::ERR;

	if (url_rel.GetAttribute(URL::KHaveServerName))
	{
		// RETURN_IF_ERROR(absolute_uri.Set(url_rel.UniName()));
		RETURN_IF_ERROR(url_rel.GetAttribute(URL::KUniName_With_Fragment_Username, absolute_uri));
	}
	else
	{
		// Combine the base uri with the relative uri.
		URL url_base = urlManager->GetURL(base_uri.CStr());
		if (url_base.IsEmpty())
			return OpStatus::ERR;

		URL url_abs = urlManager->GetURL(url_base, relative_uri.CStr());
		if (url_abs.IsEmpty())
			return OpStatus::ERR;

		RETURN_IF_ERROR(url_abs.GetAttribute(URL::KUniName_Username_Password_Hidden, absolute_uri));
	}

	return OpStatus::OK;
}

#ifdef WEBFEEDS_DISPLAY_SUPPORT
# ifdef OLD_FEED_DISPLAY
inline void AddHTMLL(URL& out_url, const uni_char* data)
{
	LEAVE_IF_ERROR(out_url.WriteDocumentData(URL::KNormal, data));
}

void WebFeedUtil::WriteGeneratedPageHeaderL(URL& out_url, const uni_char* title)
{
	// Get address of style file:
	OpString styleurl; ANCHOR(OpString, styleurl);
	g_pcfiles->GetFileURLL(PrefsCollectionFiles::StyleWebFeedsDisplay, &styleurl);

	// Write headers
	OpString direction;  ANCHOR(OpString, direction);
	switch (g_languageManager->GetWritingDirection)
	{
	case OpLanguageManager::LTR:
	default:
		direction.SetL("ltr");
		break;

	case OpLanguageManager::RTL:
		direction.SetL("rtl");
		break;
	}

	AddHTMLL(out_url, UNI_L("\xFEFF"));  // Byte order mark
	AddHTMLL(out_url, UNI_L("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\n"));
	out_url.WriteDocumentDataUniSprintf(UNI_L("<html dir=\"%s\" lang=\"%s\">\n<head>\n"), direction.CStr(), g_languageManager->GetLanguage().CStr());
	out_url.WriteDocumentDataUniSprintf(UNI_L("<title>%s</title>\n"), title);   // TODO: localize text
	AddHTMLL(out_url, UNI_L("<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-16\">\n"));
	out_url.WriteDocumentDataUniSprintf(UNI_L("<link rel=\"stylesheet\" href=\"%s\" type=\"text/css\" media=\"screen,projection,tv,handheld,speech\">\n"), styleurl.CStr());
	AddHTMLL(out_url, UNI_L("</head>\n<body>\n"));
}

void WebFeedUtil::WriteGeneratedPageFooterL(URL& out_url)
{
	// Finish up page:
	AddHTMLL(out_url, UNI_L("</body>\n</html>\n"));
	out_url.SetAttribute(URL::KForceContentType, URL_HTML_CONTENT);
	out_url.SetAttribute(URL::KCachePolicy_MustRevalidate, TRUE);
	out_url.SetIsGeneratedByOpera();
	out_url.WriteDocumentDataFinished();
	out_url.ForceStatus(URL_LOADED);
}
#ifdef WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI

void WebFeedUtil::CreateOptionsL(const char** options, UINT options_length, UINT default_option, OpString& result)
{
	result.Empty();

	for (UINT i=0; i<options_length; i++)
	{
		if (i == default_option)
			result.AppendL("<option selected>");
		else
			result.AppendL("<option>");
		result.AppendL(options[i]);
	}
}

OP_STATUS WebFeedUtil::GetFormValueAsUINT(FormValue* value, HTML_Element* helm, UINT& result)
{
	if (value->GetType() != FormValue::VALUE_TEXT)
		return OpStatus::ERR;

	OpString text;
	RETURN_IF_ERROR(value->GetValueAsText(helm, text));
	text.Strip();
	UINT length = text.Length();

	if (length < 1)
		return OpStatus::ERR;

	const uni_char *text_buf = text.CStr();
	for (UINT i=0; i < length; i++)
		if (!uni_isdigit(text_buf[i]))
			return OpStatus::ERR;

	result = uni_atoi(text_buf);

	return OpStatus::OK;
}
#endif // WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI
#endif // OLD_FEED_DISPLAY
#endif // WEBFEEDS_DISPLAY_SUPPORT
#endif // WEBFEEDS_BACKEND_SUPPORT
