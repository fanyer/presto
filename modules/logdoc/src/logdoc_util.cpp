/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/logdoc/htm_ldoc.h"

#include "modules/logdoc/logdoc_util.h"

#include "modules/doc/loadinlineelm.h"
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/formats/uri_escape.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/logdoc/html5parser.h"
#include "modules/logdoc/logdoc.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#ifdef PICTOGRAM_SUPPORT
# include "modules/logdoc/src/picturlconverter.h"
#endif // PICTOGRAM_SUPPORT
#include "modules/logdoc/attr_val.h"
#include "modules/logdoc/data/entities_len.h"
#include "modules/probetools/probepoints.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/url/url_man.h"
#include "modules/encodings/utility/opstring-encodings.h"

const int AttrvalueNameMaxSize = 14;

/**
 * This denotes where ATTR_value_name-s of a particular length begin.
 * So, at 34 those attribute values of length 5 start and such
 */
static const short ATTR_value_idx[] =
{
	0,	// start idx size 0
	0,	// start idx size 1
	0,	// start idx size 2
	3,	// start idx size 3
	15,	// start idx size 4
	34,	// start idx size 5
	46,	// start idx size 6
	63,	// start idx size 7
	67,	// start idx size 8
	74,	// start idx size 9
	78,	// start idx size 10
	78,	// start idx size 11
	78,	// start idx size 12
	78,	// start idx size 13
	78,	// start idx size 14
	79	// start idx size 15
};

/// Table of attribute values.
CONST_ARRAY(ATTR_value_name, char*)
	CONST_ENTRY("NO"), // pos 0
	CONST_ENTRY("EN"),
	CONST_ENTRY("UP"),
	CONST_ENTRY("PUT"), // pos 3
	CONST_ENTRY("GET"),
	CONST_ENTRY("YES"),
	CONST_ENTRY("ALL"),
	CONST_ENTRY("TOP"),
	CONST_ENTRY("LTR"),
	CONST_ENTRY("RTL"),
	CONST_ENTRY("BOX"),
	CONST_ENTRY("LHS"),
	CONST_ENTRY("RHS"),
	CONST_ENTRY("TEL"),
	CONST_ENTRY("URL"),
	CONST_ENTRY("AUTO"), // pos 15
	CONST_ENTRY("DISC"),
	CONST_ENTRY("POST"),
	CONST_ENTRY("TEXT"),
	CONST_ENTRY("FILE"),
	CONST_ENTRY("CIRC"),
	CONST_ENTRY("POLY"),
	CONST_ENTRY("LEFT"),
	CONST_ENTRY("HIDE"),
	CONST_ENTRY("SHOW"),
	CONST_ENTRY("VOID"),
	CONST_ENTRY("NONE"),
	CONST_ENTRY("ROWS"),
	CONST_ENTRY("COLS"),
	CONST_ENTRY("DOWN"),
	CONST_ENTRY("BOTH"),
	CONST_ENTRY("DATE"),
	CONST_ENTRY("WEEK"),
	CONST_ENTRY("TIME"),
	CONST_ENTRY("COLOR"), // pos 34
	CONST_ENTRY("RESET"),
	CONST_ENTRY("IMAGE"),
	CONST_ENTRY("RIGHT"),
	CONST_ENTRY("FALSE"),
	CONST_ENTRY("RADIO"),
	CONST_ENTRY("ABOVE"),
	CONST_ENTRY("BELOW"),
	CONST_ENTRY("SLIDE"),
	CONST_ENTRY("EMAIL"),
	CONST_ENTRY("RANGE"),
	CONST_ENTRY("MONTH"),
	CONST_ENTRY("BUTTON"), // pos 46
	CONST_ENTRY("BOTTOM"),
	CONST_ENTRY("CIRCLE"),
	CONST_ENTRY("PIXELS"),
	CONST_ENTRY("SEARCH"),
	CONST_ENTRY("SQUARE"),
	CONST_ENTRY("SUBMIT"),
	CONST_ENTRY("MIDDLE"),
	CONST_ENTRY("CENTER"),
	CONST_ENTRY("HIDDEN"),
	CONST_ENTRY("SILENT"),
	CONST_ENTRY("HSIDES"),
	CONST_ENTRY("VSIDES"),
	CONST_ENTRY("BORDER"),
	CONST_ENTRY("GROUPS"),
	CONST_ENTRY("SCROLL"),
	CONST_ENTRY("NUMBER"),
	CONST_ENTRY("DEFAULT"), // pos 63
	CONST_ENTRY("JUSTIFY"),
	CONST_ENTRY("POLYGON"),
	CONST_ENTRY("INHERIT"),
	CONST_ENTRY("FILEOPEN"), // pos 67
	CONST_ENTRY("BASELINE"),
	CONST_ENTRY("RELATIVE"),
	CONST_ENTRY("CHECKBOX"),
	CONST_ENTRY("PASSWORD"),
	CONST_ENTRY("INFINITE"),
	CONST_ENTRY("DATETIME"),
	CONST_ENTRY("ABSBOTTOM"), // pos 74
	CONST_ENTRY("ABSMIDDLE"),
	CONST_ENTRY("MOUSEOVER"),
	CONST_ENTRY("ALTERNATE"),
	CONST_ENTRY("DATETIME-LOCAL") // pos 78
CONST_END(ATTR_value_name)

/// Numeric values for each attribute string. Offset must match the string table.
static const BYTE ATTR_value_tok[] =
{
	ATTR_VALUE_no,
	ATTR_VALUE_en,
	ATTR_VALUE_up,
	ATTR_VALUE_put,
	ATTR_VALUE_get,
	ATTR_VALUE_yes,
	ATTR_VALUE_all,
	ATTR_VALUE_top,
	ATTR_VALUE_ltr,
	ATTR_VALUE_rtl,
	ATTR_VALUE_box,
	ATTR_VALUE_lhs,
	ATTR_VALUE_rhs,
	ATTR_VALUE_tel,
	ATTR_VALUE_url,
	ATTR_VALUE_auto,
	ATTR_VALUE_disc,
	ATTR_VALUE_post,
	ATTR_VALUE_text,
	ATTR_VALUE_file,
	ATTR_VALUE_circ,
	ATTR_VALUE_poly,
	ATTR_VALUE_left,
	ATTR_VALUE_hide,
	ATTR_VALUE_show,
	ATTR_VALUE_void,
	ATTR_VALUE_none,
	ATTR_VALUE_rows,
	ATTR_VALUE_cols,
	ATTR_VALUE_down,
	ATTR_VALUE_both,
	ATTR_VALUE_date,
	ATTR_VALUE_week,
	ATTR_VALUE_time,
	ATTR_VALUE_color,
	ATTR_VALUE_reset,
	ATTR_VALUE_image,
	ATTR_VALUE_right,
	ATTR_VALUE_false,
	ATTR_VALUE_radio,
	ATTR_VALUE_above,
	ATTR_VALUE_below,
	ATTR_VALUE_slide,
	ATTR_VALUE_email,
	ATTR_VALUE_range,
	ATTR_VALUE_month,
	ATTR_VALUE_button,
	ATTR_VALUE_bottom,
	ATTR_VALUE_circle,
	ATTR_VALUE_pixels,
	ATTR_VALUE_search,
	ATTR_VALUE_square,
	ATTR_VALUE_submit,
	ATTR_VALUE_middle,
	ATTR_VALUE_center,
	ATTR_VALUE_hidden,
	ATTR_VALUE_silent,
	ATTR_VALUE_hsides,
	ATTR_VALUE_vsides,
	ATTR_VALUE_border,
	ATTR_VALUE_groups,
	ATTR_VALUE_scroll,
	ATTR_VALUE_number,
	ATTR_VALUE_default,
	ATTR_VALUE_justify,
	ATTR_VALUE_polygon,
	ATTR_VALUE_inherit,
	ATTR_VALUE_fileopen,
	ATTR_VALUE_baseline,
	ATTR_VALUE_relative,
	ATTR_VALUE_checkbox,
	ATTR_VALUE_password,
	ATTR_VALUE_infinite,
	ATTR_VALUE_datetime,
	ATTR_VALUE_absbottom,
	ATTR_VALUE_absmiddle,
	ATTR_VALUE_mouseover,
	ATTR_VALUE_alternate,
	ATTR_VALUE_datetime_local
};

BYTE
ATTR_GetKeyword(const uni_char* str, int len)
{
	if (len <= AttrvalueNameMaxSize)
	{
		short end_idx = ATTR_value_idx[len+1];
		for (short i=ATTR_value_idx[len]; i<end_idx; i++)
			if (uni_strni_eq(str, ATTR_value_name[i], len) != 0)
				return ATTR_value_tok[i];
	}
	return 0;
}

int
SetLoop(const uni_char* str, UINT len)
{
	int lloop;
	if (str && len)
	{
		BYTE loop_val = ATTR_GetKeyword(str, len);
		if (loop_val == ATTR_VALUE_infinite)
			lloop = LoopInfinite;
		else
			lloop = uni_atoi(str);
	}
	else
		lloop = LoopInfinite;

	if (lloop > LoopInfinite)
		lloop = LoopInfinite;

	return lloop;
}

short
GetFirstFontNumber(const uni_char* face_list, int face_list_len, WritingSystem::Script script)
{
	uni_char* face_start = const_cast<uni_char*>(face_list); // casting to be able to do the temp termination
	const uni_char* end = face_list + face_list_len;
	uni_char* next_face;

	while (face_start < end)
	{
		while (face_start < end && HTML5Parser::IsHTML5WhiteSpace(*face_start) || *face_start == ',')
			face_start++;

		next_face = face_start;
		while (next_face < end && *next_face != ',')
			next_face++;

		int len = next_face - face_start;
		while (len && HTML5Parser::IsHTML5WhiteSpace(face_start[len - 1]))
			len--;

		if (len)
		{
			uni_char save_c = face_start[len];
			face_start[len] = '\0'; // temporarily terminate the string

			int font_num;
			StyleManager::GenericFont generic_font = styleManager->GetGenericFont(face_start);
			if (generic_font != StyleManager::UNKNOWN)
				font_num = styleManager->GetGenericFontNumber(generic_font, script);
			else
				font_num = styleManager->GetFontNumber(face_start);

			face_start[len] = save_c; // remove temporary termination
			if (font_num >= 0 && styleManager->HasFont(font_num))
				return font_num;
		}

		face_start = next_face;
	}

	return -1; // font_num;
}

BOOL
WhiteSpaceOnly(const uni_char* txt, int len)
{
	for (; len--; txt++)
	{
		if (*txt != ' ' && !uni_iscntrl(*txt))
			return FALSE;
	}
	return TRUE;
}

BOOL
WhiteSpaceOnly(const uni_char* txt)
{
	for (; *txt; txt++)
	{
		if (*txt != ' ' && !uni_iscntrl(*txt))
			return FALSE;
	}
	return TRUE;
}

void
ReplaceWhitespace(const uni_char* src_txt, int src_txt_len,
				  uni_char* target_txt, int target_txt_len,
				  BOOL preserve_whitespace_type)
{
	// don't put anything in the string if we don't have anything
	if (src_txt_len)
	{
		BOOL skip_ws = FALSE;

		for (int in = 0; in < src_txt_len; ++in)
		{
			uni_char c = src_txt[in];

			if (uni_isspace(c))
			{
				if (!skip_ws)
				{
					*(target_txt++) = preserve_whitespace_type ? c : ' ';
					skip_ws = TRUE;
				}
			}
			else
			{
				*(target_txt++) = c;
				skip_ws = FALSE;
			}
		}
	}

	// We always assume there's an extra byte at the end of the string
	*target_txt = 0;
}

#include "modules/logdoc/data/entities.inl"

/**
 * Look up an HTML character entity reference by name.
 *
 * @param esc_seq The character entity (without leading ampersand) to look up.
 * @param len Length of character entity string, in uni_chars.
 * @return the referenced character.
 */
inline uni_char
GetEscapeChar(const uni_char* esc_seq, int len)
{
	if (len <= AMP_ESC_MAX_SIZE)
	{
		const char * const *end = AMP_ESC + AMP_ESC_idx[len + 1];
		for (const char * const *p = AMP_ESC + AMP_ESC_idx[len]; p < end; ++ p)
		{
			int n = 0;
			while (n < len && (*p)[n] == esc_seq[n])
				n++;
			if (n == len)
				return AMP_ESC_char[p - AMP_ESC];
		}
	}

	return 0;
}

void RemoveTabs(uni_char* txt)
{
	uni_char* dst = txt;
	while (*txt)
	{
		if (*txt != '\t')
		{
			if (dst != txt)
			{
				*dst = *txt;
			}
			dst++;
		}
		txt++;
	}
	*dst = '\0';
}

OP_BOOLEAN
ReplaceAttributeEntities(HtmlAttrEntry* hae)
{
	BOOL has_ampersand = FALSE;
	for (unsigned i = 0; i < hae->value_len; i++)
	{
		if (hae->value[i] == '&')
		{
			has_ampersand = TRUE;
			break;
		}
	}

	if (has_ampersand)
	{
		uni_char* output = UniSetNewStrN(hae->value, hae->value_len);
		if (!output)
			return OpStatus::ERR_NO_MEMORY;

		hae->value_len = ReplaceEscapes(output, hae->value_len, FALSE, FALSE, FALSE);
		hae->value = output;
		return OpBoolean::IS_TRUE; // We replaced hae->value with a new string
	}
	return OpBoolean::IS_FALSE;
}

int
ReplaceEscapes(uni_char* txt, int txt_len,
			   BOOL require_termination,
			   BOOL remove_tabs,
			   BOOL treat_nbsp_as_space)
{
    if (!txt || txt_len <= 0)
        return 0;

	const unsigned short *win1252 = g_opera->logdoc_module.GetWin1252Table();

	int scan_idx = 0;
    uni_char* replace = txt;

	while (scan_idx < txt_len)
    {
	    uni_char* scan = txt + scan_idx;
        if (*scan == '&')
		{
            unsigned int char_no = 32; // the referenced character number
            uni_char* esc_end = scan;

			if (*(scan + 1) == '#')
			{
				// Look for numeric reference
                if ((*(scan + 2) == 'x' || *(scan + 2) == 'X') && uni_isxdigit(scan[3]))
				{ // Hexadecimal
                    esc_end = scan + 3;

                    while (uni_isxdigit(*esc_end))
                        esc_end++;

                    uni_char* dummy;
                    char_no = uni_strtol(scan + 3, &dummy, 16);
                }
				else if (Unicode::IsDigit(scan[2]))
				{ // Decimal
                    esc_end = scan + 2;

                    while (uni_isdigit(*esc_end))
                        esc_end++;

                    char_no = uni_atoi(scan + 2);
                }
                else // no number after the &# stuff should not be converted
                {
                    *replace++ = *scan++;
                    *replace++ = *scan; // skip both characters
					scan_idx += 2; // update the index when scan is updated
                    continue;
                }

                if (char_no == 0 || char_no > 0x10FFFF)
				{
					// Nul character or non-Unicode character is replaced
					// with U+FFFD.
					char_no = 0xFFFD;
                }

                if (*esc_end == ';') esc_end++;
			}
			else
			{
				// Look for character entity

				esc_end = scan + 1;

				// Supported entities are made up of alphanumerical
				// characters which are all ASCII.
				while (uni_isalnum(*esc_end) && *esc_end < 128)
					esc_end++;

				char_no = GetEscapeChar(scan + 1, esc_end - scan - 1);

				if (*esc_end == ';')
					esc_end++;
				else if (char_no == '<' || char_no == '>' || char_no == '&' || char_no == '"')
				{
				    // We always accept these because MSIE does, regardless of require_termination
					// setting
				}
				else if (require_termination || char_no > 255)
				{
					// We're less tolerant against errors with non latin-1 chars regardless
					// of the require_termination setting. This makes us compatible with MSIE.
					char_no = 0; // We required this to be terminated and it is not.
				}
			}


            if (char_no)
                scan_idx += (esc_end - scan);

			if(char_no == 9 && remove_tabs)
				char_no = 0; // skip tabs if remove_tabs is true (for URLs)
            else
                if (char_no == NBSP_CHAR && treat_nbsp_as_space)
                    char_no = ' ';

            // encode and insert character
			if (char_no >= 128 && char_no < 160)
			{
				// Treat numerical entities in the [128,160) range as
				// Windows-1252. Although not according to the standard,
				// this is what is used today.
				*replace++ = win1252 ? win1252[char_no - 0x80]
				                     : NOT_A_CHARACTER;
			}
			else if (char_no > 0 && char_no < 0x10000)
			{
				*replace++ = char_no;
			}
			else if (char_no >= 0x10000)
			{
				// a character outside the BMP was referenced, so we must
				// encode it using surrogates (grrrmph!) the procedure is
				// described in RFC 2781.
				char_no -= 0x10000;
				*replace++ = 0xD800 | (char_no >> 10);
				*replace++ = 0xDC00 | (char_no & 0x03FF);
			}
			else
			{
				*replace++ = *scan;
				scan_idx++;
			}
        } // if(*scan == '&')
        else
		{
            *replace++ = *scan;
			scan_idx++;
		}
    } // while(*scan)
    *replace = 0;

	return replace - txt;
}

int
ReplaceEscapes(uni_char* txt,
			   BOOL require_termination,
			   BOOL remove_tabs/*=FALSE*/,
			   BOOL treat_nbsp_as_space/*=FALSE*/)
{
	return ReplaceEscapes(txt, txt ? uni_strlen(txt) : 0, require_termination, remove_tabs, treat_nbsp_as_space);
}

void
EncodeFormsData(const char *charset,
				uni_char *url,
				size_t startidx_arg,
				unsigned int &len,
				unsigned int maxlen)
{
	// May be clobbered by longjmp (TRAP on Unix)
	OP_MEMORY_VAR size_t startidx = startidx_arg;

	// Check number of characters we need to encode; we start at the first
	// character of the forms data section, and end when we hit a fragment
	// identifier (bug#227310)
	uni_char * OP_MEMORY_VAR form_start = url + startidx;
	uni_char * OP_MEMORY_VAR fragment = uni_strchr(form_start, '#');
	if (fragment == form_start)
		return;
	if (!fragment)
		fragment = form_start + uni_strlen(form_start);

	// Create a reverse converter for this character set
	OutputConverter *converter;
	if (OpStatus::IsError(OutputConverter::CreateCharConverter(charset, &converter, TRUE)))
		return;

	// Convert forms data section
	OpString8 encoded;
	TRAPD(err, SetToEncodingL(&encoded, converter, form_start, (fragment ? fragment - form_start : int(KAll))));
	OP_DELETE(converter);
	if(err) //if a error happens we set the len to 0 and return.
	{
		len = 0;
		return;

	}
	else {
		unsigned char *buffer = reinterpret_cast<unsigned char *>(encoded.DataPtr());
		unsigned char *end = buffer + encoded.Length();
		size_t fragment_length = fragment ? uni_strlen(fragment) : 0;

		if (fragment_length)
		{
			// We need to calculate how much the forms data section grew, so
			// that we can move the fragment identifier into place
			uni_char *new_fragment = fragment;

			// First it grew because of the encoding into some 8-bit charset (commonly utf-8)
			int start_len = fragment - form_start;
			int current_len = end - buffer;
			int growth = current_len - start_len;
			new_fragment += growth;

			// Then it grows because we url encode some chars
			for (unsigned char *p = buffer; p < end; p ++)
			{
				if (*p & 0x80)
				{
					// Each character needing escape shifts the fragment by
					// two characters
					new_fragment += 2;
				}
			}

			// Copy the fragment up in memory
			if (new_fragment != fragment)
			{
				unsigned int position = new_fragment - url;
				if (position < maxlen)
				{
					fragment_length = MIN(fragment_length, maxlen - position);
					op_memmove(new_fragment, fragment, UNICODE_SIZE(fragment_length));
				}
				else
					fragment_length = 0;
			}
		}

		// Forget about old formsdata part
		len = startidx;

		// Copy our converted data back to the URL, escaping as needed
		len += UriEscape::Escape(form_start, (int)(maxlen-len), (char*)buffer, end-buffer, UriEscape::Range_80_ff);

		// Add the length of the fragment identifier, if any
		len += fragment_length;
	}
}

static BOOL IsSupposedToBeScheme(uni_char* scheme, int len = -1 /* -1 means - figure it out by yourself */)
{
	OP_ASSERT(scheme);

	if (len == -1)
	{
		uni_char *colon = uni_strchr(scheme, ':');
		if (!colon)
			return FALSE;
		len = (colon - scheme);
	}

	OP_ASSERT(scheme[len] == ':');

	uni_char sch[12]; /* ARRAY OK 2010-12-15 adame */
	int sch_idx = 0;

	for (int cnt = 0; cnt <= len && sch_idx < 11; ++cnt)
	{
		if (scheme[cnt] != '\t' &&
			scheme[cnt] != '\n' &&
			scheme[cnt] != '\r')
		{
			sch[sch_idx++] = scheme[cnt];
		}
	}

	sch[sch_idx] = 0;

	return (uni_stri_eq(sch, "JAVASCRIPT:") ||
			uni_stri_eq(sch, "FILE:") ||
			uni_stri_eq(sch, "MAILTO:") ||
			uni_stri_eq(sch, "DATA:"));
}

static uni_char*
CleanUrlName(uni_char *url_name, unsigned int &len,
			 unsigned int maxlen,
			 HLDocProfile *hld_profile)
{
	const uni_char *colon = uni_strchr(url_name, ':');
	const char *charset = hld_profile ? hld_profile->GetCharacterSet() : "UTF-8";

	// remove \r and \n and \t...
	const uni_char *src, *end;
	uni_char *copy;
	src = copy = url_name;

	// ...but not from the scheme part
	BOOL scheme_detected = FALSE;
	if (colon)
	{
		scheme_detected = IsSupposedToBeScheme(url_name, colon - url_name);
		if (scheme_detected)
		{
			src = copy = url_name + (colon - url_name + 1);
			len -= (colon - url_name + 1);
		}
	}

	end = src + len;
	while (src < end)
	{
		if (*src != '\r' && *src != '\n' && *src != '\t')
			*(copy ++) = *src;
		++ src;
	}

	len = copy - url_name;
	*copy = '\0';

	// Bug#123416: If the page is ISO 2022-JP, there is a chance that we
	// have links encoded in a JIS-Roman block, which would mean that
	// tilde and backslash would be replaced with overline and the yen
	// symbol. If we find these symbols in the URL, we change them back.
	if (charset && 0 == op_strcmp(charset, "iso-2022-jp"))
	{
		StrReplaceChars(url_name, 0x203E, 0x007E); // Overline -> Tilde
		StrReplaceChars(url_name, 0x00A5, 0x005C); // Yen -> Backslash
	}

	// Recode international characters in URLs. We only do this for
	// network URLs (URLs with a protocol that is followed by a server
	// name).
	//
	// Compare bug#51646 and bug#189961.

	const uni_char *colonslashslash = NULL;
	uni_char *forms_data = uni_strchr(url_name, '?');
	if (colon)
	{
		if (forms_data && colon > forms_data)
		{
			colon = NULL;
		}
		else
		{
			colonslashslash = uni_strstr(colon, UNI_L("://"));
		}

		if (!colonslashslash && colon)
		{
			// Check if relative url
			const uni_char *temp_str = url_name;
			while (*temp_str && *temp_str != ':' && !scheme_detected)
			{
				if (!uni_isalnum(*temp_str) &&
					*temp_str != '+' &&
					*temp_str != '-' &&
					*temp_str != '.')
				{
					colon = NULL;
					break;
				}
				temp_str++;
			}
		}
	}
	if ((!colon || (colon && colon == colonslashslash))
#ifdef LU_KDDI_DEVICE_PROTOCOL
		|| 0 == uni_strncmp(url_name, UNI_L("device:"), 7)
#endif // LU_KDDI_DEVICE_PROTOCOL
		)
	{
		// This is a network path, or a path relative to the current
		// document.
		//
		// If the URL contains a question mark, treat everything after it as
		// GET form data, and %-encode it using the document's character
		// set.
		//
		// If not using UTF-8 encoded URLs, we encode the entire server-part
		// of the URL in the document encoding.

		// Fetch the URL of the parent document. We need that for
		// site-specific configuration of UTF-8 encoding of URLs to work.
		BOOL use_utf8 = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseUTF8Urls, hld_profile ? hld_profile->GetURL()->GetServerName() : NULL);
#ifdef _LOCALHOST_SUPPORT_
		if (!use_utf8)
		{
			OP_ASSERT(hld_profile); // or use_utf8 could never have
									// been set to FALSE
			// Bug#197260: Local file URLs should always be UTF-8 encoded, no
			// matter what the preference setting is.
			URL *base_url = hld_profile->BaseURL(); /* <base> URL for relative URL */
			OP_ASSERT(base_url);
			URLType parent_type = base_url ? base_url->Type() : URL_NULL_TYPE;
			if ((parent_type == URL_FILE && !colon /* relative file URL */) ||
				(uni_strni_eq_upper(url_name, "FILE:", 5) /* absolute file URL */))
			{
				use_utf8 = TRUE;
			}
		}
#endif // _LOCALHOST_SUPPORT_

		if (use_utf8)
		{
			// Locate the forms data part of the URL (if any).
			if (forms_data)
			{
				++ forms_data; // Skip question mark
			}
		}
		else
		{
			if (colonslashslash)
			{
				// Locate the server-part of the URL.
				forms_data = (uni_char*)uni_strchr(colonslashslash + 3, '/');
				if (forms_data)
				{
					++ forms_data; // Skip slash
				}
			}
			else
			{
				// There is no server-part since this is a relative URL.
				// Encode the lot.
				forms_data = url_name;
			}
		}

		// URL encode if we found something to URL encode
		if (forms_data && *forms_data)
		{
			size_t idx = forms_data - url_name;
			url_name[len] = '\0';

			// URL encode
			if(idx < len)
			{
				EncodeFormsData(charset, url_name, idx, len, maxlen);
			}
		}
	}

	OP_ASSERT(len <= maxlen);
	if (len == maxlen)
	{
		// Truncate to make room for a null char
		// FIXME: Let the function work with a dynamic buffer instead of a static one.
		len--;
	}
	url_name[len] = '\0';

	return url_name;
}

void
CleanStringForDisplay(uni_char* string)
{
	// Initially true to remove leading whitespace
	BOOL in_whitespace = TRUE;

	int j = 0;
	for (int i = 0; string[i]; i++)
	{
		BOOL next_is_whitespace = IsHTML5Space(string[i]);
		if (in_whitespace && next_is_whitespace)
			continue;
		in_whitespace = next_is_whitespace;

		if (in_whitespace)
			string[j++] = ' '; // Replace all whitespace with SPACE
		else
			string[j++] = string[i];
	}

	if (j > 0 && in_whitespace) // strip trailing whitespace
		j--;
	string[j] = '\0';
}


void
CleanCDATA(uni_char* url_name, unsigned int &len)
{
	uni_char *tmp = url_name;

	// remove leading white spaces
	while (len > 0 && (uni_isspace(*tmp) || *tmp == '\r' || *tmp == '\t' || *tmp == '\n'))
	{
		tmp++;
		len--;
	}

	// remove trailing white spaces
	while (len > 0 && (uni_isspace(*(tmp+len-1)) || *(tmp+len-1) == '\r' || *(tmp+len-1) == '\t' || *(tmp+len-1) == '\n'))
		len--;

	int j = 0;
	for (unsigned int i=0; i<len; i++)
	{
		if (tmp[i] != '\n') // ignore \n
		{
			if (tmp[i] == '\r' || tmp[i] == '\t') // replace \n and \t with space
				url_name[j++] = ' ';
			else
				url_name[j++] = tmp[i];
		}
	}

	len = j;
	url_name[j] = '\0';
}

int
HexToInt(const uni_char* str, int len, BOOL strict)
{
	int val = 0;
	for (int i = 0; i < len; i++)
	{
		val *= 16;
		if (str[i] >= '0' && str[i] <= '9')
			val += static_cast<int>(str[i] - '0');
		else if (str[i] >= 'A' && str[i] <= 'F')
			val += static_cast<int>(str[i] - 'A' + 10);
		else if (str[i] >= 'a' && str[i] <= 'f')
			val += static_cast<int>(str[i] - 'a' + 10);
        else if (strict)
            return -1;
	}
	return val;
}

long
GetColorVal(const uni_char* coltxt, int collen, BOOL check_color_name)
{
	/*
	 * Color parsing according to HTML5 on rules for parsing legacy color value.
	 */
	while (collen && IsHTML5Space(*coltxt))
	{
		++coltxt;
		--collen;
	}
	while (collen && IsHTML5Space(coltxt[collen-1]))
		--collen;

	if (collen == 0 || (collen == 1 && *coltxt == '#'))
		return USE_DEFAULT_COLOR;

	if (check_color_name && uni_isalpha(*coltxt))
	{
		// Reject the keyword 'transparent'.
		if (collen == 11 && uni_strni_eq(coltxt, "transparent", 11))
			return USE_DEFAULT_COLOR;

		// try predefined colors
		COLORREF coltmp = HTM_Lex::GetColIdxByName(coltxt, collen);
		if (coltmp != USE_DEFAULT_COLOR) // Added this to get rgbcolors without '#' (and not a colorname) work...
			return HTM_Lex::GetColValByIndex(coltmp);
	}

	uni_char rgb[129];  // ARRAY OK 2011-06-15 tommyn

	if (collen == 4
		&& *coltxt == '#'
		&& HexToInt(coltxt + 1, 1, TRUE) >= 0
		&& HexToInt(coltxt + 2, 1, TRUE) >= 0
		&& HexToInt(coltxt + 3, 1, TRUE) >= 0)
	{
		rgb[5] = coltxt[3];
		rgb[4] = coltxt[3];
		rgb[3] = coltxt[2];
		rgb[2] = coltxt[2];
		rgb[1] = coltxt[1];
		rgb[0] = coltxt[1];
	}
	else
	{
		if (collen > 128)
			collen = 128;
		if (*coltxt == '#')
		{
			--collen;
			++coltxt;
		}
		op_memcpy(rgb, coltxt, UNICODE_SIZE(collen));

		for (int i = 0; i < collen; ++i)
			if (HexToInt(rgb + i, 1, TRUE) < 0)
				rgb[i] = '0';
		for (; collen % 3 != 0; ++collen)
			rgb[collen] = '0';

		if (collen > 6)
		{
			unsigned int component = collen / 3;
			uni_char *r = rgb;
			uni_char *g = r + component;
			uni_char *b = g + component;
			if (component > 8)
			{
				const unsigned int offset = component - 8;
				r += offset;
				g += offset;
				b += offset;
				component = 8;
			}
			while (component > 2 && *r == '0' && *g == '0' && *b == '0')
			{
				--component;
				++r;
				++g;
				++b;
			}
			rgb[0] = r[0];
			rgb[1] = r[1];
			rgb[2] = g[0];
			rgb[3] = g[1];
			rgb[4] = b[0];
			rgb[5] = b[1];
		}
		else if (collen == 3)
		{
			rgb[5] = rgb[2];
			rgb[4] = '0';
			rgb[3] = rgb[1];
			rgb[2] = '0';
			rgb[1] = rgb[0];
			rgb[0] = '0';
		}
	}

	return OP_RGB(static_cast<BYTE>(HexToInt(rgb    , 2, FALSE)),
				  static_cast<BYTE>(HexToInt(rgb + 2, 2, FALSE)),
				  static_cast<BYTE>(HexToInt(rgb + 4, 2, FALSE)));
}

/**
 * Convert backslash to slash in URLs, like IE and NS.
 * (stighal)
 *
 * @param url_name Pointer to a URL string.
 * @param len Length of url_name.
 */
inline void
Backslash2Slash(uni_char *url_name, UINT len)
{
    for(UINT i = 0; i < len; i++)
	{
        if( url_name[i] == '\\' )
            url_name[i] = '/';
		else if( url_name[i] == '?' )
			break;
	}
}

static URL*
CreateUrlInternal(const uni_char* value, UINT value_len,
				  URL* base_url,
				  HLDocProfile *hld_profile,
				  BOOL check_for_internal_img,
				  BOOL accept_empty
#ifdef WEB_TURBO_MODE
				  , BOOL set_context_id/*=FALSE*/
#endif // WEB_TURBO_MODE
				  )
{
	OP_PROBE4(OP_PROBE_LOGDOC_CREATEURLFROMSTRING);

	// remove leading whitespace
	while (value_len && uni_isspace(value[0]))
	{
		value++;
		value_len--;
	}

	if (value_len == 0 && !accept_empty) // lets not bother to do anything if we have no string
	{
		URL *ret_url = OP_NEW(URL, ());
		return ret_url;
	}

	unsigned int len = value_len;
	AutoTempBuffer uni_tmp_buf;
	if (OpStatus::IsMemoryError(uni_tmp_buf.GetBuf()->Append(value, value_len)))
		return NULL;

	uni_char *uni_tmp = uni_tmp_buf.GetBuf()->GetStorage();
	size_t max_len = uni_tmp_buf.GetBuf()->GetCapacity();

	// Fix forms data and stuff. Also remove any HTML escapes, unless we
	// are in a X[HT]ML document, in which case they will be unescaped
	// already.
	uni_tmp = CleanUrlName(uni_tmp, len, max_len, hld_profile);

	BOOL is_file_url = uni_strni_eq(uni_tmp, "FILE:", 5);
	BOOL is_data_url =
#ifdef SUPPORT_DATA_URL
		uni_strni_eq(uni_tmp, "DATA:", 5);
#else
		FALSE;
#endif
	BOOL is_mailto_url = uni_strni_eq(uni_tmp, "MAILTO:", 7);
	BOOL is_javascript_url =
		uni_strni_eq(uni_tmp, "JAVASCRIPT:", 11); // don't escape a javascript url !!!

	BOOL invalid_scheme = FALSE;
	if ((!is_file_url && !is_data_url && !is_mailto_url && !is_javascript_url) && IsSupposedToBeScheme(uni_tmp))
	{
#ifdef LOGDOC_IGNORE_SCHEMES_WITH_TABS_OR_NEW_LINES
		URL *ret_url = OP_NEW(URL, ());
		return ret_url;
#else // LOGDOC_IGNORE_SCHEMAS_WITH_TABS_OR_NEW_LINES
		invalid_scheme = TRUE;
#endif // LOGDOC_IGNORE_SCHEMAS_WITH_TABS_OR_NEW_LINES
	}

	// Bug#191273: Convert backslash in URLs to a forward slash, but only
	// in quirks mode.
	if (hld_profile && !hld_profile->IsInStrictMode()
		&& !is_file_url && !is_data_url && !is_javascript_url)
		Backslash2Slash(uni_tmp, len);

	uni_char *rel_start = 0;
	uni_char *url_start = uni_tmp;
	if (!is_javascript_url && !is_data_url && !is_mailto_url)
	{
		rel_start = uni_strchr(uni_tmp, '#');
		if(rel_start)
		{
			*(rel_start++) = '\0';
		}
	}

	URL* url = NULL;

#ifdef PICTOGRAM_SUPPORT
	//URL copy_url;
	if (check_for_internal_img)
	{
		if (uni_str_eq(url_start, "pict:"))
		{
			OpString file_url;
			OP_STATUS rc = PictUrlConverter::GetLocalUrl(url_start, uni_strlen(url_start), file_url);
			if (OpStatus::IsError(rc))
				return NULL;

			if (!file_url.IsEmpty())
			{
				url = OP_NEW(URL, ());
				if (url) *url = g_url_api->GetURL(file_url.CStr());
				return url;
			}
		}
	}
#endif // PICTOGRAM_SUPPORT

	URL_CONTEXT_ID context_id = 0;
#ifdef WEB_TURBO_MODE
	context_id = (set_context_id && hld_profile) ? hld_profile->GetLogicalDocument()->GetCurrentContextId() : 0;
#endif // WEB_TURBO_MODE

	if (invalid_scheme)
	{
		URL temp_url = urlManager->GenerateInvalidHostPageUrl(uni_tmp, context_id
#ifdef ERROR_PAGE_SUPPORT
															  , OpIllegalHostPage::KIllegalCharacter
#endif // ERROR_PAGE_SUPPORT
			);
		url = OP_NEW(URL, (temp_url));
		return url;
	}

	URL *doc_url = hld_profile ? hld_profile->GetURL() : NULL;
	if (base_url && doc_url)
	{
		OP_PROBE4(OP_PROBE_LOGDOC_CREATEURLFROMSTRING2);

		URL use_base = *base_url;
#ifdef WEB_TURBO_MODE
		if (set_context_id && use_base.GetContextId() != context_id)
		{
			const OpStringC8 base_str
				= base_url->GetAttribute(URL::KName_With_Fragment_Username_Password_NOT_FOR_UI, URL::KNoRedirect);
			use_base = g_url_api->GetURL(base_str.CStr(), context_id);
		}
#endif // WEB_TURBO_MODE

		if (!is_javascript_url && !is_mailto_url)
		{
			if (!url_start || !*url_start)
			{
				if (!rel_start)
					url = OP_NEW(URL, (use_base));
				else
					url = OP_NEW(URL, (use_base, rel_start));
			}
			else if (doc_url->GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).Compare(url_start) == 0
					 && (!doc_url->GetAttribute(URL::KUnique) || rel_start))
			{
#ifdef WEB_TURBO_MODE
				if (set_context_id && doc_url->GetContextId() != context_id)
				{
					const OpStringC8 base_str = doc_url->GetAttribute(URL::KName_With_Fragment_Username_Password_NOT_FOR_UI, URL::KNoRedirect);
					use_base = g_url_api->GetURL(base_str.CStr(), context_id);
				}
				else
#endif // WEB_TURBO_MODE
					use_base = *doc_url;

				url = OP_NEW(URL, (use_base, rel_start));
			}
			else
			{
				URL tmp_url = g_url_api->GetURL(use_base, url_start, rel_start);
				url = OP_NEW(URL, (tmp_url));
			}
		}
		else
		{
			URL tmp_url = g_url_api->GetURL(use_base, url_start, rel_start, is_javascript_url);
			url = OP_NEW(URL, (tmp_url));
		}
	}
	else if (base_url)
	{
		URL use_base = *base_url;
#ifdef WEB_TURBO_MODE
		if (set_context_id && use_base.GetContextId() != context_id)
		{
			const OpStringC8 base_str = base_url->GetAttribute(URL::KName_With_Fragment_Username_Password_NOT_FOR_UI, URL::KNoRedirect);
			use_base = g_url_api->GetURL(base_str.CStr(), context_id);
		}
#endif // WEB_TURBO_MODE

		URL tmp_url = g_url_api->GetURL(use_base, url_start, rel_start, is_javascript_url);
		url = OP_NEW(URL, (tmp_url));
	}
	else
	{
#ifdef WEB_TURBO_MODE
		URL tmp_url = g_url_api->GetURL(url_start, rel_start, is_javascript_url, context_id);
#else // WEB_TURBO_MODE
		URL tmp_url = g_url_api->GetURL(url_start, rel_start, is_javascript_url);
#endif // WEB_TURBO_MODE
		url = OP_NEW(URL, (tmp_url));
	}

	return url;
}

URL*
SetUrlAttr(const uni_char* value, UINT value_len,
		   URL* base_url,
		   HLDocProfile *hld_profile,
		   BOOL check_for_internal_img,
		   BOOL accept_empty)
{
	return CreateUrlInternal(value, value_len, base_url, hld_profile, check_for_internal_img, accept_empty
#ifdef WEB_TURBO_MODE
		, FALSE
#endif // WEB_TURBO_MODE
		);
}

URL*
CreateUrlFromString(const uni_char* value, UINT value_len,
					URL* base_url,
					HLDocProfile *hld_profile,
					BOOL check_for_internal_img,
					BOOL accept_empty
#ifdef WEB_TURBO_MODE
					,BOOL set_context_id/*=FALSE*/
#endif // WEB_TURBO_MODE
					)
{
	return CreateUrlInternal(value, value_len, base_url, hld_profile, check_for_internal_img, accept_empty
#ifdef WEB_TURBO_MODE
		, set_context_id
#endif // WEB_TURBO_MODE
		);
}

BOOL
SetTabIndexAttr(const uni_char *value, int value_len, HTML_Element *elm, long& return_value)
{
	if (value_len && value_len < 20)
	{
		uni_char number[20]; /* ARRAY OK 2009-02-05 stighal */
		op_memcpy(number, value, sizeof(uni_char) * value_len);
		number[value_len] = '\0';

		uni_char* end_ptr;
		long num_val = uni_strtol(number, &end_ptr, 10);
		if (end_ptr == number + value_len)
		{
			// Correct number
			if (num_val > INT_MAX)
				num_val = INT_MAX;

			return_value = num_val;
			return TRUE;
		}
	}

	return FALSE;
}

uni_char*
SetStringAttr(const uni_char* value, int value_len,
			  BOOL replace_ws)
{
	uni_char* str = UniSetNewStrN(value, value_len); // FIXME:REPORT_MEMMAN-KILSMO

	// OOM check
	if (str)
	{
		if (replace_ws)
		{
			// remove leading whitespace
			while (value_len && uni_isspace(value[0]))
			{
				value++;
				value_len--;
			}
			// remove trailing whitespace
			while (value_len && uni_isspace(value[value_len - 1]))
				value_len--;

			ReplaceWhitespace(value, value_len, str, value_len);
		}
	}

	return str;
}

/**
 * @deprecated Only has three arguments now
 */
uni_char* SetStringAttr(const uni_char* value, int value_len, BOOL replace_ws, BOOL unused1, BOOL unused2)
{
	return SetStringAttr(value, value_len, replace_ws);
}

uni_char*
SetStringAttrUTF8Escaped(const uni_char *value, int value_len,
						 HLDocProfile *hld_profile)
{
	uni_char *uni_tmp = reinterpret_cast<uni_char *>(g_memory_manager->GetTempBuf());
	unsigned int uni_tmp_len = UNICODE_DOWNSIZE(g_memory_manager->GetTempBufLen()) - 1;

	// Crop URL length to the size of the buffer
	unsigned int len =
		(uni_tmp_len <= static_cast<unsigned int>(value_len))
			? uni_tmp_len
			: static_cast<unsigned int>(value_len);
	if (len)
		uni_strncpy(uni_tmp, value, len);
	uni_tmp[len] = '\0';

	uni_tmp = CleanUrlName(uni_tmp, len, uni_tmp_len, hld_profile);
	return SetStringAttr(uni_tmp, len, FALSE);
}

const uni_char*
GetCurrentBaseTarget(HTML_Element* from_he)
{
	HTML_Element* he = from_he;
	while (he)
	{
		const uni_char* btarget = he->GetBase_Target();
		if (btarget)
			return btarget;
		he = he->Prev();
	}
	return NULL;
}

BOOL
IsHtmlInlineElm(HTML_ElementType type, HLDocProfile *hld_profile)
{
	OP_ASSERT(hld_profile);

	switch (type)
	{
	case HE_TEXT:
	case HE_TEXTGROUP:
	case HE_COMMENT:
	case HE_PROCINST:
	case HE_DOCTYPE:
	case HE_ENTITY:
	case HE_ENTITYREF:
	case HE_UNKNOWN:
	case HE_A:
	case HE_IMG:
	case HE_BR:
	case HE_I:
	case HE_B:
	case HE_TT:
	case HE_FONT:
	case HE_SMALL:
	case HE_BIG:
	case HE_STRIKE:
	case HE_S:
	case HE_U:
	case HE_EMBED:
#ifdef MEDIA_HTML_SUPPORT
	case HE_VIDEO:
#endif // MEDIA_HTML_SUPPORT
#ifdef CANVAS_SUPPORT
	case HE_CANVAS:
#endif
	case HE_METER:
	case HE_PROGRESS:
	case HE_SUB:
	case HE_SUP:
	case HE_CITE:
	case HE_EM:
	case HE_STRONG:
	case HE_KBD:
	case HE_DFN:
	case HE_VAR:
	case HE_CODE:
	case HE_SAMP:
	case HE_NOBR:
	case HE_WBR:
	case HE_INPUT:
	case HE_SELECT:
	case HE_OPTION:
	case HE_TEXTAREA:
	case HE_LABEL:
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
	case HE_KEYGEN:
#endif
	//case HE_TABLE:
	case HE_NOEMBED:
	case HE_MAP:
	case HE_AREA:
	case HE_SPAN:
	case HE_MARK:
	case HE_SCRIPT:
	case HE_OPTGROUP:
	case HE_INS: // <ins> and <del> are special cases who are sometimes blocks and sometimes inlines. See bug 216970
	case HE_DEL:
		return TRUE;

	case HE_NOSCRIPT:
		return hld_profile->GetESEnabled();

	default:
		return FALSE;
	}
}

AutoTempBuffer::AutoTempBuffer()
{
	m_buf = g_opera->logdoc_module.GetTempBuffer();
}

AutoTempBuffer::~AutoTempBuffer()
{
	g_opera->logdoc_module.ReleaseTempBuffer(m_buf);
}

unsigned short int GetSuggestedCharsetId(HTML_Element *html_element, HLDocProfile *hld_profile, LoadInlineElm *elm)
{
	unsigned short int parent_charset_id = 0;
	uni_char *charset_attr = reinterpret_cast<uni_char *>(html_element->GetAttr(ATTR_CHARSET, ITEM_TYPE_STRING, NULL));
	if (charset_attr && *charset_attr)
	{
		OpString8 charset_utf8;
		if (OpStatus::IsError(charset_utf8.SetUTF8FromUTF16(charset_attr)))
			return 0;
		if (OpStatus::IsError(g_charsetManager->GetCharsetID(charset_utf8.CStr(), &parent_charset_id)))
			return 0;
	}
	else
	{
		if (OpStatus::IsError(g_charsetManager->GetCharsetID(hld_profile->GetCharacterSet(), &parent_charset_id)))
			return 0;
	}

	if (parent_charset_id)
	{
		g_charsetManager->IncrementCharsetIDReference(parent_charset_id);
		const char *parent_charset_string = g_charsetManager->GetCanonicalCharsetFromID(parent_charset_id);

		// Perform security check
		BOOL allowed = FALSE;
		OP_STATUS rc = OpStatus::OK;
		if (parent_charset_string)
		{
			OpSecurityContext
				source(*hld_profile->GetURL(), hld_profile->GetCharacterSet() ? hld_profile->GetCharacterSet() : ""),
				target(*elm->GetUrl(), parent_charset_string, TRUE);

			rc = OpSecurityManager::CheckSecurity(
				OpSecurityManager::DOC_SET_PREFERRED_CHARSET,
				source, target, allowed);
		}
		g_charsetManager->DecrementCharsetIDReference(parent_charset_id);

		if (OpStatus::IsError(rc) || !allowed)
			return 0;

		// FIXME: Log to console when blocking attribute?
	}

	return parent_charset_id;
}
