/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/style/src/css_lexer.h"
#include "modules/style/src/css_parser.h"
#include "modules/util/opfile/unistream.h"

static inline BOOL css_is_nmstart(uni_char c)
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 128)
		return TRUE;
	else
		return FALSE;
}

static inline BOOL css_is_nmchar(uni_char c)
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c >= 128)
		return TRUE;
	else
		return FALSE;
}

static inline BOOL css_is_decimal_digit(uni_char c)
{
	if (c >= '0' && c <= '9')
		return TRUE;
	else
		return FALSE;
}

static inline BOOL css_is_space(uni_char c)
{
	return (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f');
}

static inline unsigned int css_str_to_uint(const uni_char* num_str)
{
	unsigned int ret = 0;

	while (*num_str)
	{
		if (ret > (UINT_MAX / 10) - 10)
			return UINT_MAX;
		ret = 10*ret + (*num_str++ - '0');
	}

	return ret;
}

CSS_Lexer::CSS_Lexer(CSS_Buffer* css_buffer, BOOL parse_col) :
	value(NULL),
	in_media_rule(FALSE),
	in_supports_rule(FALSE),
	returned_eof_tok(FALSE),
	unmatched_paren(FALSE),
	expect_nfunc(FALSE),
	parse_color(parse_col),
	block_level(0),
	stored_block_level(0)
#ifdef CSS_ERROR_SUPPORT
	, line_no(1)
	, line_start(0)
	, input_truncated(TRUNC_NONE)
#endif // CSS_ERROR_SUPPORT
{
	in_buffer = css_buffer;
}

CSS_Lexer::~CSS_Lexer()
{
}

#ifdef CSS_ERROR_SUPPORT
uni_char* CSS_Lexer::GetCurrentLineString(int max_len)
{
	int start_pos = line_start;
	int cur_pos = in_buffer->CurrentPos();
	if (cur_pos - line_start > max_len)
		start_pos = cur_pos - max_len;
	return in_buffer->GetLineString(start_pos, max_len);
}
#endif // CSS_ERROR_SUPPORT

void CSS_Lexer::EatNMChars()
{
	while (LookAheadIsNMChar())
		GetNMChar();
}

int CSS_Lexer::GetNMChar()
{
	int ret;
	if (in_buffer->LookAhead() == '\\')
		ret = GetEscape();
	else
		ret = in_buffer->GetNextChar();

	if (ret >= 'A' && ret <= 'Z')
		ret += 0x20;
	return ret;
}

int CSS_Lexer::GetEscape()
{
	OP_ASSERT(in_buffer->LookAhead() == '\\' && in_buffer->BytesLeft() > 1);

	// Consume '\\'
	in_buffer->GetNextChar();

	if (uni_isxdigit(in_buffer->LookAhead()))
	{
		int ret = 0;
		for (int i=0; i<6 && in_buffer->BytesLeft()>0; i++)
		{
			if (uni_isxdigit(in_buffer->LookAhead()))
			{
				uni_char c = in_buffer->GetNextChar();
				ret *= 0x10;
				if (c > '9')
					ret += (c & 0xf) + 9;
				else
					ret += c - '0';
			}
			else
				break;
		}
		if (Unicode::IsSurrogate(ret))
			ret = 0xFFFD;
		if (in_buffer->BytesLeft() > 0)
		{
			switch (in_buffer->LookAhead())
			{
			case '\r':
				if (in_buffer->LookAhead(1) == '\n')
					in_buffer->GetNextChar();
			case ' ':
			case '\t':
			case '\n':
			case '\f':
				in_buffer->GetNextChar();
				break;
			default:
				break;
			}
		}
		return ret;
	}
	else
		return in_buffer->GetNextChar();
}

BOOL CSS_Lexer::MatchNMChars(const char* chars)
{
	while (*chars && LookAheadIsNMChar() && GetNMChar() == *chars)
		chars++;
	return *chars == 0;
}

int CSS_Lexer::GetNextCharAsToken()
{
	int start = in_buffer->CurrentPos();
	uni_char ch = in_buffer->GetNextChar();

	if (value && in_supports_rule &&
		(ch == '(' || ch == ')'))
		// Save the parenthesis positions so we can extract the supports_declaration_condition
		value->str.start_pos = start;

	switch (ch)
	{
	case '{':
		if (in_media_rule && block_level == stored_block_level)
			in_media_rule = FALSE;
		else if (in_supports_rule && block_level == stored_block_level)
			in_supports_rule = FALSE;
		// fall through
	case '(':
	case '[':
		PushBracket(ch);
		break;
	case ')':
		unmatched_paren = FALSE;
		expect_nfunc = FALSE;
		// fall through
	case '}':
	case ']':
		PopBracket(ch);
		break;
	case ';':
		if (in_media_rule && block_level <= stored_block_level)
			in_media_rule = FALSE;
		else if (in_supports_rule && block_level <= stored_block_level)
			in_supports_rule = FALSE;
		break;
	case ':':
	case '.':
	case ',':
	case '/':
	case '+':
	case '-':
	case '>':
	case '*':
	case '=':
	case '|':
	case '$':
	case '~':
	case '#':
		break;
	default:
		return CSS_TOK_UNRECOGNISED_CHAR;
	}
	return ch;
}

void CSS_Lexer::EatWhiteSpaces()
{
	while (in_buffer->BytesLeft() > 0 && css_is_space(in_buffer->LookAhead()))
	{
#ifdef CSS_ERROR_SUPPORT
		int ws = in_buffer->GetNextChar();
		if (ws == '\n')
		{
			line_no++;
			line_start = in_buffer->CurrentPos();
		}
		else if (ws == '\r')
		{
			line_no++;
			if (in_buffer->BytesLeft() > 0 && in_buffer->LookAhead() == '\n')
				in_buffer->GetNextChar();
			line_start = in_buffer->CurrentPos();
		}
#else // CSS_ERROR_SUPPORT
		in_buffer->GetNextChar();
#endif // CSS_ERROR_SUPPORT
	}
}

int CSS_Lexer::ParseToken()
{
	if (parse_color && in_buffer->BytesLeft() > 0)
	{
		// fast path for parsing color

		// "#"{name}
		if (in_buffer->LookAhead() == '#')
		{
			return ParseHash();
		}

		// {ident}
		if (LookAheadIsNMStart())
		{
			return ParseIdent();
		}
		// if not, do the slow path
	}

	// Ignore comments
	// \/\*[^*]*\*+([^/][^*]*\*+)*\/
	while (in_buffer->LookAhead() == '/' && in_buffer->LookAhead(1) == '*')
		ParseComment();

	if (in_buffer->BytesLeft() == 0)
	{
		if (returned_eof_tok)
			return 0;
		else if (unmatched_paren)
		{
			PopBracket(')');
			unmatched_paren = FALSE;
#ifdef CSS_ERROR_SUPPORT
			input_truncated |= TRUNC_PAREN;
#endif // CSS_ERROR_SUPPORT
			return ')';
		}
		else if (block_level > 0)
		{
			block_level--;
#ifdef CSS_ERROR_SUPPORT
			input_truncated |= TRUNC_BLOCK;
#endif // CSS_ERROR_SUPPORT
			return '}';
		}
		else
		{
			returned_eof_tok = TRUE;
			return CSS_TOK_EOF;
		}
	}

	uni_char lookahead = in_buffer->LookAhead();

	// [ \t\r\n\f]+
	if (css_is_space(lookahead))
	{
		EatWhiteSpaces();
		return CSS_TOK_SPACE;
	}

	// "<!--"
	if (lookahead == '<')
	{
		if (LookAheadStr("<!--"))
		{
			in_buffer->EatChars(4);
			return CSS_TOK_CDO;
		}
		else
			return GetNextCharAsToken();
	}

	// "-->"
	if (lookahead == '-')
	{
		if (!expect_nfunc && LookAheadIsNMStart(1))
			return ParseIdent();
		else if (LookAheadStr("-->"))
		{
			in_buffer->EatChars(3);
			return CSS_TOK_CDC;
		}
		else
			return GetNextCharAsToken();
	}

	// "~="
	if (lookahead == '~')
	{
		if (in_buffer->LookAhead(1) == '=')
		{
			in_buffer->EatChars(2);
			return CSS_TOK_INCLUDES;
		}
		else
			return GetNextCharAsToken();
	}

	// "|="
	if (lookahead == '|')
	{
		if (in_buffer->LookAhead(1) == '=')
		{
			in_buffer->EatChars(2);
			return CSS_TOK_DASHMATCH;
		}
		else
			return GetNextCharAsToken();
	}

	// "^="
	if (lookahead == '^')
	{
		if (in_buffer->LookAhead(1) == '=')
		{
			in_buffer->EatChars(2);
			return CSS_TOK_PREFIXMATCH;
		}
		else
			return GetNextCharAsToken();
	}

	// "$="
	if (lookahead == '$')
	{
		if (in_buffer->LookAhead(1) == '=')
		{
			in_buffer->EatChars(2);
			return CSS_TOK_SUFFIXMATCH;
		}
		else
			return GetNextCharAsToken();
	}

	// "*="
	if (lookahead == '*')
	{
		if (in_buffer->LookAhead(1) == '=')
		{
			in_buffer->EatChars(2);
			return CSS_TOK_SUBSTRMATCH;
		}
		else
			return GetNextCharAsToken();
	}

	// {string}
	if (lookahead == '"' || lookahead == '\'')
	{
		return ParseString();
	}

	// U\+{range}
	// U\+{h}{1,6}-{h}{1,6}
	if (lookahead == 'U' && in_buffer->LookAhead(1) == '+'
		&& (uni_isxdigit(in_buffer->LookAhead(2)) || in_buffer->LookAhead(2) == '?'))
	{
		uni_char char_str[8]; /* ARRAY OK 2009-02-12 rune */

		in_buffer->EatChars(2);
		int i=0;
		while (uni_isxdigit(in_buffer->LookAhead(i)) && i<6)
			i++;

		if (in_buffer->LookAhead(i) == '-' && uni_isxdigit(in_buffer->LookAhead(i+1)))
		{
			// this is a range with '<start>-<end>'
			in_buffer->GetNChars(char_str, i);
			char_str[i] = 0;
			if (value)
				value->int_pair.first = uni_strtol(char_str, 0, 16);
			in_buffer->GetNextChar();
			i = 0;
			while (uni_isxdigit(in_buffer->LookAhead(i)) && i<6)
				i++;
			in_buffer->GetNChars(char_str, i);
			char_str[i] = 0;
			if (value)
				value->int_pair.last = uni_strtol(char_str, 0, 16);
		}
		else if (in_buffer->LookAhead(i) == '?' && i<6)
		{
			// this is a range with ? wildcard(s)
			int i_q = i, n;

			while (i < 6 && in_buffer->LookAhead(i) == '?')
				i++;
			in_buffer->GetNChars(char_str, i);
			char_str[i] = 0;
			for (n=i_q; n<i; n++)
				char_str[n]='0';
			if (value)
				value->int_pair.first = uni_strtol(char_str, 0, 16);
			for (n=i_q; n<i; n++)
				char_str[n]='f';
			if (value)
				value->int_pair.last = uni_strtol(char_str, 0, 16);
		}
		else
		{
			// this is a single character
			in_buffer->GetNChars(char_str, i);
			char_str[i] = 0;
			if (value)
				value->int_pair.first =
				value->int_pair.last = uni_strtol(char_str, 0, 16);
		}
		return CSS_TOK_UNICODERANGE;
	}

	// {ident}
	if (LookAheadIsNMStart())
	{
		return ParseIdent();
	}

	// "#"{name}
	if (lookahead == '#')
	{
		return ParseHash();
	}

	// "@import"
	// "@page"
	// "@media"
	// "@font-face"
	// "@charset"
	// "@namespace"
	// "@"{ident}
	// "@keyframes"
	if (lookahead == '@')
	{
		if (in_buffer->BytesLeft() > 1 && (LookAheadIsNMStart(1) || in_buffer->LookAhead(1) == '-' && LookAheadIsNMStart(2)))
			return ParseAtSymbol();
		else
			return GetNextCharAsToken();
	}

	// "!"{w}"important"
	if (lookahead == '!')
	{
		return ParseImportantSymbol();
	}

	// {num}
	if (lookahead >= '0' && lookahead <= '9' ||
		lookahead == '.' && css_is_decimal_digit(in_buffer->LookAhead(1)))
	{
		return ParseNumExpr();
	}

	// LookAhead character does not match any token rule.
	return GetNextCharAsToken();
}

#define NUM_BUF_LEN 32

int CSS_Lexer::ParseNumExpr()
{
	if (value)
		value->number.start_pos = in_buffer->CurrentPos();

	BOOL leading_zeros = FALSE;

	while (in_buffer->LookAhead() == '0')
	{
		in_buffer->EatChars(1);
		leading_zeros = TRUE;
	}

	uni_char num_str[NUM_BUF_LEN]; /* ARRAY OK 2009-02-12 rune */
	uni_char* tmp = num_str;
	int count = 0;

	while (css_is_decimal_digit(in_buffer->LookAhead()))
	{
		uni_char c = in_buffer->GetNextChar();

		if (++count < NUM_BUF_LEN)
			*tmp++ = c;
	}

	BOOL is_integer = TRUE;

	if (in_buffer->LookAhead() == '.' && css_is_decimal_digit(in_buffer->LookAhead(1)))
	{
		is_integer = FALSE;

		uni_char c = in_buffer->GetNextChar();

		if (++count < NUM_BUF_LEN)
			*tmp++ = c;

		while (css_is_decimal_digit(in_buffer->LookAhead()))
		{
			c = in_buffer->GetNextChar();

			if (++count < NUM_BUF_LEN)
				*tmp++ = c;
		}
	}
	else if (leading_zeros && count == 0)
		*tmp++ = '0';

	*tmp = 0;

	int tok;

	if (in_buffer->LookAhead() == '%')
	{
		in_buffer->EatChars(1);
		tok = CSS_TOK_PERCENTAGE;
	}
	else if (expect_nfunc && is_integer && in_buffer->LookAhead() == 'n')
	{
		in_buffer->EatChars(1);
		tok = CSS_TOK_N;
	}
	else if (LookAheadIsNMStart())
	{
		if (value)
			value->number.number = (float)uni_atof(num_str);

		tok = ParseUnit();
	}
	else
	{
		tok = is_integer ? CSS_TOK_INTEGER : CSS_TOK_NUMBER;
	}

	if (value)
	{
		value->number.str_len = in_buffer->CurrentPos() - value->number.start_pos;

		if (tok == CSS_TOK_PERCENTAGE || tok == CSS_TOK_NUMBER)
			value->number.number = (float)uni_atof(num_str);
		else if (tok == CSS_TOK_N || tok == CSS_TOK_INTEGER)
			value->uinteger.integer = css_str_to_uint(num_str);
	}

	return tok;
}

int CSS_Lexer::ParseUnit()
{
	int tok = CSS_TOK_DIMEN;
	double factor = 0;

	switch (GetNMChar())
	{
	case 'c':
		if (LookAheadIsNMChar() && GetNMChar() == 'm')
			tok = CSS_TOK_LENGTH_CM;
		break;
	case 'e':
		if (LookAheadIsNMChar())
		{
			int c = GetNMChar();
			if (c == 'm')
				tok = CSS_TOK_EMS;
			else if (c == 'x')
				tok = CSS_TOK_EXS;
		}
		break;
	case 'd':
		if (LookAheadIsNMChar())
		{
			int c1 = GetNMChar();
			if (LookAheadIsNMChar())
			{
				int c2 = GetNMChar();
				if (c1 == 'e' && c2 == 'g')
				{
					tok = CSS_TOK_ANGLE;
					factor = M_PI/180.0;
				}
				else if (c1 == 'p')
				{
					if (c2 == 'i')
						tok = CSS_TOK_DPI;
					else if (c2 == 'c' && LookAheadIsNMChar() && GetNMChar() == 'm')
						tok = CSS_TOK_DPCM;
					else if (c2 == 'p' && LookAheadIsNMChar() && GetNMChar() == 'x')
						tok = CSS_TOK_DPPX;
				}
			}
		}
		break;
	case 'g':
		if (LookAheadIsNMChar() && GetNMChar() == 'r' &&
			LookAheadIsNMChar() && GetNMChar() == 'a' &&
			LookAheadIsNMChar() && GetNMChar() == 'd')
		{
			tok = CSS_TOK_ANGLE;
			factor = M_PI/200.0;
		}
		break;
	case 'h':
		if (LookAheadIsNMChar() && GetNMChar() == 'z')
			tok = CSS_TOK_FREQ;
		break;
	case 'i':
		if (LookAheadIsNMChar() && GetNMChar() == 'n')
			tok = CSS_TOK_LENGTH_IN;
		break;
	case 'k':
		if (LookAheadIsNMChar() && GetNMChar() == 'h' &&
			LookAheadIsNMChar() && GetNMChar() == 'z')
		{
			tok = CSS_TOK_FREQ;
			factor = 1000.0;
		}
		break;
	case 'm':
		if (LookAheadIsNMChar())
		{
			int c = GetNMChar();
			if (c == 'm')
				tok = CSS_TOK_LENGTH_MM;
			else if (c == 's')
			{
				tok = CSS_TOK_TIME;
				factor = 0.001;
			}
		}
		break;
	case 'p':
		if (LookAheadIsNMChar())
		{
			switch (GetNMChar())
			{
			case 'x':
				tok = CSS_TOK_LENGTH_PX;
				break;
			case 't':
				tok = CSS_TOK_LENGTH_PT;
				break;
			case 'c':
				tok = CSS_TOK_LENGTH_PC;
				break;
			}
		}
		break;
	case 'r':
		if (LookAheadIsNMChar())
		{
			int c = GetNMChar();
			if (c == 'a' && LookAheadIsNMChar() && GetNMChar() == 'd')
				tok = CSS_TOK_ANGLE;
			else if (c == 'e' && LookAheadIsNMChar() && GetNMChar() == 'm')
				tok = CSS_TOK_REMS;
		}
		break;
	case 's':
		tok = CSS_TOK_TIME;
		break;
	case 't':
		if (LookAheadIsNMChar() && GetNMChar() == 'u' &&
			LookAheadIsNMChar() && GetNMChar() == 'r' &&
			LookAheadIsNMChar() && GetNMChar() == 'n')
		{
			tok = CSS_TOK_ANGLE;
			factor = 2*M_PI;
		}
		break;
	}

	if (LookAheadIsNMChar())
	{
		tok = CSS_TOK_DIMEN;
		EatNMChars();
	}

	if (factor != 0 && tok != CSS_TOK_DIMEN)
	{
		value->number.number *= factor;
	}

	return tok;
}

int CSS_Lexer::ParseImportantSymbol()
{
	OP_ASSERT(in_buffer->LookAhead() == '!');

	if (in_buffer->BytesLeft() > 1)
	{
#ifdef CSS_ERROR_SUPPORT
		int stored_line_no = line_no;
		int stored_line_start = line_start;
#endif // CSS_ERROR_SUPPORT
		in_buffer->PushState();
		in_buffer->GetNextChar();
		EatWhiteSpaces();
		if (LookAheadStr("important"))
		{
			in_buffer->PopState();
			in_buffer->EatChars(9);
			return CSS_TOK_IMPORTANT_SYM;
		}
		else
		{
			in_buffer->RestoreState();
#ifdef CSS_ERROR_SUPPORT
			line_no = stored_line_no;
			line_start = stored_line_start;
#endif // CSS_ERROR_SUPPORT
		}
	}
	return GetNextCharAsToken();
}

int CSS_Lexer::ParseAtSymbol()
{
	OP_ASSERT(in_buffer->BytesLeft() > 1);
	// Eat '@'
	in_buffer->GetNextChar();

	int tok = CSS_TOK_ATKEYWORD;

	switch (GetNMChar())
	{
	case '-':
		if (MatchNMChars("o-viewport"))
			tok = CSS_TOK_VIEWPORT_SYM;
		break;
	case 'c':
		if (MatchNMChars("harset"))
			tok = CSS_TOK_CHARSET_SYM;
		break;
	case 'f':
		if (MatchNMChars("ont-face"))
			tok = CSS_TOK_FONT_FACE_SYM;
		break;
	case 'i':
		if (MatchNMChars("mport"))
			tok = CSS_TOK_IMPORT_SYM;
		break;
#ifdef CSS_ANIMATIONS
	case 'k':
		if (MatchNMChars("eyframes"))
			tok = CSS_TOK_KEYFRAMES_SYM;
#endif // CSS_ANIMATIONS
	case 'm':
		if (MatchNMChars("edia"))
			tok = CSS_TOK_MEDIA_SYM;
		break;
	case 'n':
		if (MatchNMChars("amespace"))
			tok = CSS_TOK_NAMESPACE_SYM;
		break;
	case 'p':
		if (MatchNMChars("age"))
			tok = CSS_TOK_PAGE_SYM;
		break;
	case 's':
		if (MatchNMChars("upports"))
			tok = CSS_TOK_SUPPORTS_SYM;
		break;
	}

	if (LookAheadIsNMChar())
	{
		tok = CSS_TOK_ATKEYWORD;
		EatNMChars();
	}
	else if (tok == CSS_TOK_MEDIA_SYM || tok == CSS_TOK_IMPORT_SYM)
	{
		in_media_rule = TRUE;
	}
	else if (tok == CSS_TOK_SUPPORTS_SYM)
	{
		in_supports_rule = TRUE;
	}

	return tok;
}

int CSS_Lexer::ParseHash()
{
	OP_ASSERT(in_buffer->LookAhead() == '#');

	if (in_buffer->BytesLeft() > 1 && LookAheadIsNMChar(1))
	{
		in_buffer->GetNextChar();
		int start = in_buffer->CurrentPos();

		EatNMChars();

		// copy string into YYSTYPE
		if (value)
		{
			value->str.start_pos = start;
			value->str.str_len = in_buffer->CurrentPos()-start;
		}
		return CSS_TOK_HASH;
	}
	else
		return GetNextCharAsToken();
}

void CSS_Lexer::ParseComment()
{
	in_buffer->GetNextChar();
	in_buffer->GetNextChar();

	while (in_buffer->BytesLeft() > 1)
	{
		if (in_buffer->LookAhead() == '*' && in_buffer->LookAhead(1) == '/')
		{
			in_buffer->GetNextChar();
			in_buffer->GetNextChar();
			return;
		}
		else
		{
			// Use EatWhiteSpaces to update line_no correctly.
			if (css_is_space(in_buffer->LookAhead()))
				EatWhiteSpaces();
			else
				in_buffer->GetNextChar();
		}
	}

	if (in_buffer->BytesLeft() > 0)
		in_buffer->GetNextChar();
}

BOOL CSS_Lexer::LookAheadStr(const char* str)
{
	int i=0;

	while (*str)
	{
		uni_char cmp_char = in_buffer->LookAhead(i++);
		if (cmp_char >= 'A' && cmp_char <= 'Z')
			cmp_char += 0x20;
		if (cmp_char != *str++)
			return FALSE;
	}
	return TRUE;
}

int CSS_Lexer::ParseString()
{
	int string_start = in_buffer->CurrentPos() + 1;
	int delimiter = in_buffer->LookAhead();
	int matched = MatchString();

	if (matched)
	{
		if (value)
		{
			int string_len = in_buffer->CurrentPos()-string_start;
			if (in_buffer->BytesLeft() != 0 || in_buffer->LastChar() == delimiter)
			{
				if (string_len > 0)
					string_len--;
			}
#ifdef CSS_ERROR_SUPPORT
			else
				input_truncated |= TRUNC_STRING;
#endif // CSS_ERROR_SUPPORT

			value->str.start_pos = string_start;
			value->str.str_len = string_len;
		}
		return CSS_TOK_STRING;
	}
	else
	{
		return CSS_TOK_STRING_TRUNC;
	}
}

int CSS_Lexer::MatchString()
{
	uni_char delimiter = in_buffer->GetNextChar();
	uni_char look_ahead = 0;

	while (in_buffer->BytesLeft() > 0 && (look_ahead = in_buffer->GetNextChar()) != delimiter)
	{
		if (look_ahead == '\\')
		{
			if (in_buffer->LookAhead() == '\r')
			{
#ifdef CSS_ERROR_SUPPORT
				line_no++;
#endif // CSS_ERROR_SUPPORT
				if (in_buffer->LookAhead(1) == '\n')
					in_buffer->EatChars(2);
				else
					in_buffer->EatChars(1);
#ifdef CSS_ERROR_SUPPORT
				line_start = in_buffer->CurrentPos();
#endif // CSS_ERROR_SUPPORT
			}
			else if (in_buffer->LookAhead() == '\n'
#ifdef CSS_ERROR_SUPPORT
				&& ++line_no && ((line_start = in_buffer->CurrentPos()), TRUE)
#endif // CSS_ERROR_SUPPORT
				|| in_buffer->LookAhead() == '\f')
				in_buffer->EatChars(1);
			else
			{
				in_buffer->UnGetChar();
				GetEscape();
			}
		}
		else if (look_ahead == '\n' || look_ahead == '\r' || look_ahead == '\f')
			break;
	}

	return (look_ahead == delimiter || in_buffer->BytesLeft() == 0);
}

int CSS_Lexer::ParseIdent()
{
	int start = in_buffer->CurrentPos();
	int tok = CSS_TOK_IDENT;

	switch (GetNMChar())
	{
	case 'a':
		//and
		if ((InMediaRule() || InSupportsRule()) && MatchNMChars("nd"))
			tok = CSS_TOK_AND;
		//attr
		else if (MatchNMChars("ttr"))
			tok = CSS_TOK_ATTR_FUNC;
		break;
	case 'c':
		if (LookAheadIsNMChar())
		{
			int c = GetNMChar();
			if (c == 'u' && LookAheadIsNMChar())
			{
				c = GetNMChar();
				//cue
				if (c == 'e')
					tok = CSS_TOK_CUE_FUNC;
				//cubic-bezier
				else if (c == 'b' && MatchNMChars("ic-bezier"))
					tok = CSS_TOK_CUBIC_BEZ_FUNC;
			}
			else if (c == 'o' && MatchNMChars("unter"))
			{
				if (LookAheadIsNMChar())
				{
					//counters
					if (GetNMChar() == 's')
						tok = CSS_TOK_COUNTERS_FUNC;
				}
				//counter
				else
					tok = CSS_TOK_COUNTER_FUNC;
			}
		}
		break;
	case 'd':
		//dashboard-region
		if (MatchNMChars("ashboard-region"))
			tok = CSS_TOK_DASHBOARD_REGION_FUNC;
		break;
	case 'f':
		//format
		if (MatchNMChars("ormat"))
			tok = CSS_TOK_FORMAT_FUNC;
		break;
	case 'h':
		if (MatchNMChars("sl"))
		{
			//hsla
			if (LookAheadIsNMChar())
			{
				if (GetNMChar() == 'a')
					tok = CSS_TOK_HSLA_FUNC;
			}
			//hsl
			else
				tok = CSS_TOK_HSL_FUNC;
		}
		break;
	case 'l':
		if (LookAheadIsNMChar())
		{
			int c = GetNMChar();
			//lang
			if (c == 'a' && MatchNMChars("ng"))
				tok = CSS_TOK_LANG_FUNC;
			//local
			else if (c == 'o' && MatchNMChars("cal"))
				tok = CSS_TOK_LOCAL_FUNC;
			//linear-gradient
			else if (c == 'i' && MatchNMChars("near-gradient"))
				tok = CSS_TOK_LINEAR_GRADIENT_FUNC;
		}
		break;

	case 'm':
		//matrix
		if (MatchNMChars("atrix"))
		{
			if (value)
				value->shortint = CSS_VALUE_matrix;

			tok = CSS_TOK_TRANSFORM_FUNC;
		}

		break;

	case 'n':
		if (expect_nfunc)
		{
			if (value)
				value->integer.integer = 1;
			return CSS_TOK_N;
		}
		if (LookAheadIsNMChar())
		{
			int c = GetNMChar();
			//not
			if (c == 'o')
			{
				if (LookAheadIsNMChar() && GetNMChar() == 't')
					tok = CSS_TOK_NOT_FUNC;
			}
			else if (c == 't' && MatchNMChars("h-") && LookAheadIsNMChar())
			{
				switch (GetNMChar())
				{
				//nth-child
				case 'c':
					if (MatchNMChars("hild"))
						tok = CSS_TOK_NTH_CHILD_FUNC;
					break;
				//nth-of-type
				case 'o':
					if (MatchNMChars("f-type"))
						tok = CSS_TOK_NTH_OF_TYPE_FUNC;
					break;
				case 'l':
					if (MatchNMChars("ast-") && LookAheadIsNMChar())
					{
						int c = GetNMChar();
						//nth-last-child
						if (c == 'c' && MatchNMChars("hild"))
							tok = CSS_TOK_NTH_LAST_CHILD_FUNC;
						//nth-last-of-type
						else if (c == 'o' && MatchNMChars("f-type"))
							tok = CSS_TOK_NTH_LAST_OF_TYPE_FUNC;
					}
					break;
				}
			}
		}
		break;

	case 'o':
		if (LookAheadIsNMChar())
		{
			int c = GetNMChar();
			//only
			if (InMediaRule() && c == 'n' && MatchNMChars("ly"))
				tok = CSS_TOK_ONLY;
			//or
			else if (InSupportsRule() && c == 'r')
				tok = CSS_TOK_OR;
		}
		break;
	case 'r':
		if (LookAheadIsNMChar())
		{
			int c = GetNMChar();
			//rgb
			if (c == 'g' && LookAheadIsNMChar() && GetNMChar() == 'b')
			{
				if (LookAheadIsNMChar())
				{
					if (GetNMChar() == 'a')
						tok = CSS_TOK_RGBA_FUNC;
				}
				else
					tok = CSS_TOK_RGB_FUNC;
			}
			//rect
			else if (c == 'e' && LookAheadIsNMChar())
			{
				c = GetNMChar();
				if (c == 'c' && MatchNMChars("t"))
					tok = CSS_TOK_RECT_FUNC;
				else if ( c == 'p' && MatchNMChars("eating-") && LookAheadIsNMChar())
				{
					c = GetNMChar();
					//repeating-linear-gradient
					if (c == 'l' && MatchNMChars("inear-gradient"))
						tok = CSS_TOK_REPEATING_LINEAR_GRADIENT_FUNC;
					//repeating-radial-gradient
					else if (c == 'r' && MatchNMChars("adial-gradient"))
						tok = CSS_TOK_REPEATING_RADIAL_GRADIENT_FUNC;
				}
			}
			//rotate
			else if (c == 'o' && MatchNMChars("tate"))
			{
				if (value)
					value->shortint = CSS_VALUE_rotate;
				tok = CSS_TOK_TRANSFORM_FUNC;
			}
			//radial-gradient
			else if (c == 'a' && MatchNMChars("dial-gradient"))
				tok = CSS_TOK_RADIAL_GRADIENT_FUNC;
		}
		break;

	case 's':
		if (LookAheadIsNMChar())
		{
			int c = GetNMChar();
			//skew
			if (c == 'k' && MatchNMChars("ew"))
			{
				if (LookAheadIsNMChar())
				{
					c = GetNMChar();
					if (value)
						if (c == 'x')
							value->shortint = CSS_VALUE_skewX;
						else if (c == 'y')
							value->shortint = CSS_VALUE_skewY;
				}
				else if (value)
					value->shortint = CSS_VALUE_skew;

				tok = CSS_TOK_TRANSFORM_FUNC;
			}
			//scale
			else if (c == 'c' && MatchNMChars("ale"))
			{
				if (LookAheadIsNMChar())
				{
					c = GetNMChar();
					if (value)
						if (c == 'x')
							value->shortint = CSS_VALUE_scaleX;
						else if (c == 'y')
							value->shortint = CSS_VALUE_scaleY;
				}
				else if (value)
					value->shortint = CSS_VALUE_scale;

				tok = CSS_TOK_TRANSFORM_FUNC;
			}
			else if (c == 't' && MatchNMChars("eps"))
				tok = CSS_TOK_STEPS_FUNC;
		}
		break;

	case 't':
		//translate
		if (MatchNMChars("ranslate"))
		{
			if (LookAheadIsNMChar())
			{
				int c = GetNMChar();
				if (value)
					if (c == 'x')
						value->shortint = CSS_VALUE_translateX;
					else if (c == 'y')
						value->shortint = CSS_VALUE_translateY;
			}
			else if (value)
				value->shortint = CSS_VALUE_translate;

			tok = CSS_TOK_TRANSFORM_FUNC;
		}

		break;

	case 'u':
		// "url("{w}{string}{w}")"
		if (MatchNMChars("rl") && in_buffer->LookAhead() == '(')
		{
			int string_start=0;
			int string_len=0;
#ifdef CSS_ERROR_SUPPORT
			int stored_line_no = line_no;
			int stored_line_start = line_start;
#endif // CSS_ERROR_SUPPORT
			in_buffer->PushState();
			in_buffer->EatChars(1);
			EatWhiteSpaces();
			if (in_buffer->LookAhead() == '"' || in_buffer->LookAhead() == '\'')
			{
				string_start = in_buffer->CurrentPos() + 1;
				MatchString();
				string_len = in_buffer->CurrentPos() - string_start - 1;
			}
			else
			{
				string_start = in_buffer->CurrentPos();
				MatchURL();
				string_len = in_buffer->CurrentPos()-string_start;
			}

			EatWhiteSpaces();

			if (in_buffer->LookAhead() == ')')
			{
				in_buffer->GetNextChar();
				in_buffer->PopState();

				if (value)
				{
					value->str.start_pos = string_start;
					value->str.str_len = string_len;
				}
				return CSS_TOK_URI;
			}
			else
			{
				in_buffer->RestoreState();
#ifdef CSS_ERROR_SUPPORT
				line_no = stored_line_no;
				line_start = stored_line_start;
#endif // CSS_ERROR_SUPPORT
			}
		}
		break;
	case '-':
		if (LookAheadIsNMChar())
		{
			switch (GetNMChar())
			{
			case 'o':
				if (MatchNMChars("-") && LookAheadIsNMChar())
				{
					int c = GetNMChar();
					//-o-skin
					if (c == 's')
					{
						if (MatchNMChars("kin"))
							tok = CSS_TOK_SKIN_FUNC;
					}
					//-o-double-rainbow
					else if (c == 'd')
					{
						if (MatchNMChars("ouble-rainbow"))
							tok = CSS_TOK_DOUBLE_RAINBOW_FUNC;
					}
					//-o-integer
					else if (c == 'i')
					{
						if (MatchNMChars("nteger"))
							tok = CSS_TOK_INTEGER_FUNC;
					}
					else if (c == 'l')
						switch (GetNMChar())
						{
						case 'a':
							//-o-language-string
							if (MatchNMChars("nguage-string"))
								tok = CSS_TOK_LANGUAGE_STRING_FUNC;
							break;
						case 'i':
							//-o-linear-gradient
							if (MatchNMChars("near-gradient"))
								tok = CSS_TOK_O_LINEAR_GRADIENT_FUNC;
							break;
						}
				}
				break;
			case 'w':
				//-webkit-linear-gradient
				if (MatchNMChars("ebkit-linear-gradient"))
				{
					tok = CSS_TOK_WEBKIT_LINEAR_GRADIENT_FUNC;
					break;
				}
			}
		}
		break;
	}

	if (LookAheadIsNMChar())
	{
		tok = CSS_TOK_IDENT;
		EatNMChars();
	}

	BOOL is_func = !(tok == CSS_TOK_IDENT || tok == CSS_TOK_AND || tok == CSS_TOK_ONLY || tok == CSS_TOK_OR || tok == CSS_TOK_NOT);
	if (in_buffer->LookAhead() == '(')
	{
		in_buffer->GetNextChar();
		PushBracket('(');
		if (!is_func)
			tok = CSS_TOK_FUNCTION;
		else
		{
			switch (tok)
			{
			case CSS_TOK_NTH_CHILD_FUNC:
			case CSS_TOK_NTH_OF_TYPE_FUNC:
			case CSS_TOK_NTH_LAST_CHILD_FUNC:
			case CSS_TOK_NTH_LAST_OF_TYPE_FUNC:
				expect_nfunc = TRUE;
			}
		}
	}
	else
	{
		if (tok == CSS_TOK_NOT_FUNC && (InMediaRule() || InSupportsRule()))
			tok = CSS_TOK_NOT;
		else if (is_func)
			tok = CSS_TOK_IDENT;

		// copy string into YYSTYPE for CSS_TOK_IDENT
		if (value && tok == CSS_TOK_IDENT)
		{
			value->str.start_pos = start;
			value->str.str_len = in_buffer->CurrentPos() - start;
		}
	}

	return tok;
}

BOOL CSS_Lexer::LookAheadIsEscape(int n)
{
	if (in_buffer->LookAhead(n) != '\\')
		return FALSE;
	else
	{
		uni_char peek = in_buffer->LookAhead(n+1);
		switch (peek)
		{
		case '\n':
		case '\r':
		case '\f':
		case 0:
			return FALSE;
		default:
			return TRUE;
		}
	}
}

BOOL CSS_Lexer::LookAheadIsNMChar(int n)
{
	return (css_is_nmchar(in_buffer->LookAhead(n)) || LookAheadIsEscape(n));
}

BOOL CSS_Lexer::LookAheadIsNMStart(int n)
{
	return (css_is_nmstart(in_buffer->LookAhead(n)) || LookAheadIsEscape(n));
}

void CSS_Lexer::MatchURL()
{
	// ([!#$%&*-~]|{non-ascii}|{escape})*

	while (in_buffer->BytesLeft() && ((in_buffer->LookAhead() >= '*' && in_buffer->LookAhead() <= '~') ||
									   in_buffer->LookAhead() == '!' ||
									   in_buffer->LookAhead() == '#' ||
									   in_buffer->LookAhead() == '$' ||
									   in_buffer->LookAhead() == '%' ||
									   in_buffer->LookAhead() == '&' ||
									   in_buffer->LookAhead() >= 128))
	{
		if (LookAheadIsEscape())
			GetEscape();
		else
			in_buffer->GetNextChar();
	}
}

static inline void
STORE_BLOCK_LOOKAHEAD_ASSERT(int stack_top, int lookahead)
{
	// All FUNC tokens (which push a '(' on the bracket stack) should be listed
	// below. If this assert triggers when lookahead is a FUNC token not listed
	// below, add it.
	OP_ASSERT(stack_top != '(' ||
			  lookahead == '(' ||
			  lookahead == CSS_TOK_FUNCTION ||
			  lookahead == CSS_TOK_RGB_FUNC ||
			  lookahead == CSS_TOK_HSL_FUNC ||
			  lookahead == CSS_TOK_NOT_FUNC ||
			  lookahead == CSS_TOK_RGBA_FUNC ||
			  lookahead == CSS_TOK_HSLA_FUNC ||
			  lookahead == CSS_TOK_LINEAR_GRADIENT_FUNC ||
			  lookahead == CSS_TOK_REPEATING_LINEAR_GRADIENT_FUNC ||
			  lookahead == CSS_TOK_RADIAL_GRADIENT_FUNC ||
			  lookahead == CSS_TOK_REPEATING_RADIAL_GRADIENT_FUNC ||
			  lookahead == CSS_TOK_DOUBLE_RAINBOW_FUNC ||
			  lookahead == CSS_TOK_ATTR_FUNC ||
			  lookahead == CSS_TOK_TRANSFORM_FUNC ||
			  lookahead == CSS_TOK_COUNTER_FUNC ||
			  lookahead == CSS_TOK_COUNTERS_FUNC ||
			  lookahead == CSS_TOK_RECT_FUNC ||
			  lookahead == CSS_TOK_DASHBOARD_REGION_FUNC ||
			  lookahead == CSS_TOK_LANG_FUNC ||
			  lookahead == CSS_TOK_SKIN_FUNC ||
			  lookahead == CSS_TOK_LANGUAGE_STRING_FUNC ||
			  lookahead == CSS_TOK_NTH_CHILD_FUNC ||
			  lookahead == CSS_TOK_NTH_OF_TYPE_FUNC ||
			  lookahead == CSS_TOK_NTH_LAST_CHILD_FUNC ||
			  lookahead == CSS_TOK_NTH_LAST_OF_TYPE_FUNC ||
			  lookahead == CSS_TOK_FORMAT_FUNC ||
			  lookahead == CSS_TOK_LOCAL_FUNC ||
			  lookahead == CSS_TOK_CUBIC_BEZ_FUNC ||
			  lookahead == CSS_TOK_INTEGER_FUNC ||
			  lookahead == CSS_TOK_STEPS_FUNC);
}

void CSS_Lexer::StoreBlockLevel(int lookahead)
{
	if (block_level < 0)
		stored_block_level = 0;
	else
	{
		stored_block_level = block_level;
		if (block_level > 0)
		{
			// If the lookahead has been pushed to the bracket stack, move one
			// level up since that was were the start of block bracket was
			// stored. If the lookahead is a FUNC token, a '(' has been pushed
			// but it won't match the token. Assume it's a FUNC token and let
			// the assert make sure that's what happened.
			if (bracket_stack[block_level-1] == lookahead || bracket_stack[block_level-1] == '(')
			{
				STORE_BLOCK_LOOKAHEAD_ASSERT(bracket_stack[block_level-1], lookahead);
				--stored_block_level;
			}
		}

		// Make sure we're storing a block level for a block, not different type of brackets.
		OP_ASSERT(stored_block_level == 0 || stored_block_level <= CSS_BRACKET_STACK_SIZE && bracket_stack[stored_block_level-1] == '{');
	}
}

int CSS_Lexer::RecoverBlockOrSemi(int sync_token)
{
	// Error recovery. Synchronize on next end-of-block or semi-colon on stored_block_level, or EOF.
	int tok = sync_token;

	while (tok != CSS_TOK_EOF)
	{
		if (block_level < 0)
		{
			block_level = 0;
			/* Negative block_level with a '}' token means that we didn't find
			   a matching bracket. Make sure we don't recognize this as a
			   balanced block at the right level below. */
			if (tok == '}')
				tok = CSS_TOK_UNRECOGNISED_CHAR;
		}

		if (stored_block_level > block_level || block_level == stored_block_level && (tok == '}' || tok == ';'))
			break;

		tok = ParseToken();
	}

	// Return the '}' token if the block level < stored_block_level. That means we've reached
	// the end of the block outside of the rule we're recovering from and therefore need re-emit
	// the token for the outer block production in the grammar.
	OP_ASSERT(block_level == stored_block_level || tok == '}');
	return (stored_block_level > block_level) ? tok : 0;
}

int CSS_Lexer::RecoverDeclaration(int sync_token)
{
	// Syntax error in declaration. Synchronize on next semi-colon or end of declaration block.
	int tok = sync_token;
	while (!(	block_level == stored_block_level && tok == ';'
			||	stored_block_level > block_level
			||	tok == CSS_TOK_EOF))
	{
		tok = ParseToken();
	}
	// Return last token which will be either ';', '}', or CSS_TOK_EOF.
	// They are only used as synchronization tokens and need to be re-emitted by the parser
	// to match the grammar productions for lists of declarations and declaration blocks.
	OP_ASSERT(tok == ';' || tok == '}' || tok == CSS_TOK_EOF);
	return tok;
}

int CSS_Lexer::RecoverMediaQuery(int sync_token)
{
	// Syntax error in media query. Synchronize on next comma, start of block, or EOF.
	int tok = sync_token;
	if (block_level < 0)
		/* Negative block_level with a '}' token means that we didn't find
		   a matching bracket. Make sure we don't recognize this as a
		   balanced block at the right level below. */
		block_level = 0;

	while (!(	block_level == stored_block_level && tok == ','
			||	block_level == stored_block_level && tok == ';' && !InMediaRule()
			||	block_level == stored_block_level+1 && tok == '{' && !InMediaRule()
			||	stored_block_level > block_level
			||	tok == CSS_TOK_EOF))
	{
		tok = ParseToken();
		if (block_level < 0)
		{
			/* Negative block_level with a '}' token means that we didn't find
			   a matching bracket. Make sure we don't recognize this as a
			   balanced block at the right level below. */
			block_level = 0;
			OP_ASSERT(tok == '}');
		}
	}
	return tok;
}
