/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/parser/svgtokenizer.h"
#include "modules/xmlutils/xmlutils.h"

void
SVGTokenizer::StringRules::Init(BOOL aq, BOOL ws, BOOL cs,
								BOOL ss, uni_char ec)
{
	allow_quoting = aq;
	wsp_separator = ws;
	comma_separator = cs;
	semicolon_separator = ss;
	end_condition = ec;
}

void
SVGTokenizer::Reset(const uni_char *is, unsigned is_length)
{
    state.rest_string = input_string = is;
    state.rest_string_length = input_string_length = is_length;
	state.Shift();
}

BOOL
SVGTokenizer::ScanNumber(double& d)
{
	State number_state = state;
    unsigned num_length = number_state.MatchNumber();
    if (num_length == 0)
    {
		return FALSE;
    }

	// We have a number
	state = number_state;

    // This is a bottleneck in SVG parsing. Anything that can make
    // this faster will improve general svg performance.
	//
	// When we get here we know that we have a legal number of length
    // num_length. Therefore we can skip past uni_strntod and its
    // sanity checks and go directly to op_strtod. But still,
    // op_strtod is slow and we call it alot.
    char buf[100]; /* ARRAY OK 2009-01-28 ed */
    char* buf_p;
    if (num_length < (int)sizeof(buf))
    {
		buf_p = buf;
    }
    else
    {
		buf_p = OP_NEWA(char, num_length+1);
		if (!buf_p)
		{
			return FALSE; // OOM
		}
    }

	// We know that current_char + rest_string for num_length is an
	// ascii letters: digits, '.', 'e', '+' or '-'.
	const uni_char *str = state.rest_string - 1;
    for (unsigned i = 0; i < num_length; i++)
    {
		buf_p[i] = static_cast<char>(str[i]);
    }
    buf_p[num_length] = '\0';
    char* endptr = NULL;
    d = op_strtod(buf_p, &endptr);
    OP_ASSERT(((endptr - buf_p) > 0 &&
			   (unsigned)(endptr - buf_p) == num_length) ||
			  !"Our number parser has a bug so that it accepts numbers that op_strtod can't handle");
    if (buf_p != buf)
    {
		OP_DELETEA(buf_p);
    }

    state.MoveAscii(num_length);
    return TRUE;
}

unsigned
SVGTokenizer::State::Shift()
{
	return (current_char = (rest_string_length > 0) ?
			XMLUtils::GetNextCharacter(rest_string, rest_string_length) :'\0');
}

unsigned
SVGTokenizer::State::Shift(unsigned &counter)
{
	counter++;
	return Shift();
}

void
SVGTokenizer::State::MoveAscii(unsigned offset)
{
	// We can assume ascii in current_char and (offset - 1) chars into
	// rest_string.
	//
	// We move rest_string past the end of the known ascii block and
	// shift in the next char.

	if (offset > 0)
	{
		rest_string = (rest_string - 1) + offset;
		rest_string_length -= (offset - 1);
		Shift();
	}
}

void
SVGTokenizer::State::EatWsp()
{
    while (XMLUtils::IsSpace(current_char) && Shift())
    {
		/* empty */
	}
}

void
SVGTokenizer::State::EatWspSeparator(char separator)
{
	EatWsp();
	Scan(separator);
	EatWsp();
}

unsigned
SVGTokenizer::State::ScanHexDigits(unsigned& v)
{
	unsigned num_digits = 0;
	while (XMLUtils::IsHexDigit(current_char))
	{
		unsigned c = uni_toupper(current_char);
		Shift(num_digits);

		v <<= 4;
		v |= (uni_isalpha(c) ? c - 'A' + 10 : c - '0');
	}
	return num_digits;
}

unsigned
SVGTokenizer::State::MatchNumber()
{
    BOOL legal_number = FALSE;

	if (current_char > 127 || current_char == 0)
		return 0;

	const uni_char *start = rest_string - 1;
	const uni_char *p = start;

    if (*p == '-' || *p == '+')
    {
		p++;
    }

    if (*p >= '0' && *p <= '9')
    {
		p++;

		legal_number = TRUE;
		while (*p >= '0' && *p <= '9')
		{
			p++;
		}
    }

    if (*p == '.')
    {
		p++;
    }

    if (*p >= '0' && *p <= '9')
    {
		p++;
		legal_number = TRUE;
		while (*p >= '0' && *p <= '9')
		{
			p++;
		}
    }

    if (legal_number)
    {
		if (*p == 'e' || *p == 'E')
		{
 			const uni_char *t = p;

			t++;

			if(*t == '+' || *t == '-')
				t++;

			const uni_char *np = t;

			while(*t >= '0' && *t <= '9')
				t++;

			// Check if the 'e[+-]?' is part of the number
			if ((t - np) > 0 /* \d+ */)
			{
				p = t;
			}
		}

		return p - start;
    }
    else
    {
		return 0;
    }
}

BOOL
SVGTokenizer::State::Scan(char c)
{
	OP_ASSERT(c >= 0);
	if (current_char == (unsigned)c)
    {
		Shift();
		return TRUE;
    }
    else
    {
		return FALSE;
    }
}

BOOL
SVGTokenizer::State::Scan(const char *pattern)
{
	// Since pattern is a single byte string, we can simply compare
	// them byte for byte.

	if (rest_string_length == 0 || current_char > 127)
		return FALSE;

	unsigned n = rest_string_length + 1;
	const uni_char *c = rest_string - 1;
	const char *p = pattern;
	while(*p && n && *p == *c)
	{
		p++, c++, n--;
	}

	if (!*p)
	{
		rest_string_length -= (p - pattern - 1);
		rest_string = c;
		Shift();
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL
SVGTokenizer::State::ScanAlphabetic()
{
	if (Unicode::IsAlpha(current_char))
	{
		Shift();
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

const uni_char*
SVGTokenizer::State::CurrentString() const
{
	if (current_char < 0x10000)
		return rest_string - 1;
	else
		return rest_string - 2;
}

unsigned
SVGTokenizer::State::CurrentStringLength() const
{
	if (current_char < 0x10000)
		return rest_string_length + 1;
	else
		return rest_string_length + 2;
}

OP_STATUS
SVGTokenizer::ReturnStatus(OP_STATUS status)
{
	EatWsp();

    if (OpStatus::IsMemoryError(status))
    {
		return OpStatus::ERR_NO_MEMORY;
    }
    else if (OpStatus::IsError(status) || !IsEnd())
    {
		return OpSVGStatus::ATTRIBUTE_ERROR;
    }
    else
    {
		return OpStatus::OK;
    }
}

BOOL
SVGTokenizer::ScanURL(const uni_char *&url_string, unsigned &url_string_length)
{
	State saved_state = state;
	if (Scan("url("))
	{
		url_string_length = 0;
		unsigned c = CurrentCharacter();
		if (c == '"' || c == '\'')
		{
			StringRules rules;
			rules.Init(/* allow_quoting = */ TRUE,
					   /* wsp_separator = */ FALSE,
					   /* comma_separator = */ FALSE,
					   /* semicolon_separator = */ FALSE,
					   /* end_condition = */ ')');
			ScanString(rules, url_string, url_string_length);
		}
		else
		{
			ScanURI(url_string, url_string_length);
		}

		if (Scan(')') && url_string_length > 0)
		{
			return TRUE;
		}
		else
		{
			state = saved_state;
			return FALSE;
		}
	}
	else
	{
		return FALSE;
	}
}

BOOL
SVGTokenizer::ScanURI(const uni_char *&uri, unsigned &uri_length)
{
    uri = CurrentString();
    uri_length = 0;
    unsigned c = CurrentCharacter();

    // URLs are recognized as in the style module (css_lexer.cpp)
    // except for the hex-style escaping of which the SVG
    // specification says nothing (1.1)
    while  ( c && ((c>='*' && c<='~') ||
				   c=='!' || c=='#' ||
				   c=='$' || c=='%' ||
				   c=='&' ||
				   c>=128))
    {
		if (c == '\\')
		{
			c = Shift(uri_length);
			if (c >= ' ' && c <= '~' || c >= 128)
				c = Shift(uri_length);
		}
		c = Shift(uri_length);
    }

	return uri_length > 0;
}

BOOL
SVGTokenizer::ScanString(StringRules &rules, const uni_char *&sub_string,
						 unsigned &sub_string_length)
{
	State saved_state = state;
	EatWsp();

	if (IsEnd())
	{
		state = saved_state;
		return FALSE;
	}

	unsigned c = CurrentCharacter();

	unsigned delimiter = '\0';
	BOOL quote_mode = ShouldQuote(rules, c);
	if (quote_mode)
	{
		delimiter = c;
		c = Shift();
	}

	sub_string = CurrentString();
	sub_string_length = 0;

	BOOL found_delimiter = FALSE;
	BOOL found_string = FALSE;
	unsigned wsp_length = 0;
	while (!found_string)
	{
		if (c == '\0' ||
			(!quote_mode && c == rules.end_condition))
		{
			found_string = TRUE;
			if (quote_mode)
			{
				state = saved_state;
				return FALSE;
			}
		}
		else if (c == delimiter)
		{
			if (quote_mode)
			{
				c = Shift();
				sub_string_length += wsp_length;
				wsp_length = 0;
				quote_mode = FALSE;
				found_delimiter = TRUE;
			}
			else
			{
				state = saved_state;
				return FALSE;
			}
		}
		else if (!quote_mode && ShouldSeparate(rules, c))
		{
			do { c = Shift();	} while(ShouldSeparate(rules, c));
			found_string = TRUE;
		}
		else if (!found_delimiter && quote_mode && ShouldEscape(rules, c))
		{
			c = Shift(sub_string_length);
			if ((c >= ' ' && c <= '~') || c>=128)
			{
				c = Shift(sub_string_length);
			}
		}
		else if (!quote_mode && XMLUtils::IsSpace(c))
		{
			// possible trailing whitespace
			c = Shift();
			wsp_length++;
		}
		else if (!found_delimiter)
		{
			sub_string_length += wsp_length;
			wsp_length = 0;
			c = Shift(sub_string_length);
		}
		else
		{
			// text past delimiter
			state = saved_state;
			return FALSE;
		}
	}

	return (sub_string_length > 0);
}

/* static */ BOOL
SVGTokenizer::ShouldSeparate(StringRules &r, unsigned c)
{
	return r.wsp_separator && XMLUtils::IsSpace(c) ||
		r.comma_separator && c == ',' ||
		r.semicolon_separator && c == ';';
}

SVGTokenizer::State::State() :
	current_char(0),
	rest_string(NULL),
	rest_string_length(0)
{
}

#endif // SVG_SUPPORT

