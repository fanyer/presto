/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Johannes Hoff / Jan Borsodi
**
*/

#include "core/pch.h"

#ifdef PROTOBUF_SUPPORT

#ifdef PROTOBUF_JSON_SUPPORT

#include "modules/protobuf/src/json_tools.h"

#include "modules/protobuf/src/protobuf_utils.h"
#include "modules/util/tempbuf.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/util/opstring.h"
#include "modules/formats/encoder.h"
#include "modules/stdlib/include/double_format.h"

/* OpScopeJSON::Input */

OP_STATUS
OpScopeJSON::Input::LA(unsigned int num, uni_char &value) const
{
	OP_ASSERT(cur != NULL);
	OP_ASSERT(num > 0);
	const uni_char *pos = cur + num - 1;
	if (pos >= end)
		return OpStatus::ERR;
	value = *pos;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Input::Consume(unsigned int num)
{
	OP_ASSERT(cur != NULL);
	const uni_char *pos = cur + num;
	if (pos > end)
		return OpStatus::ERR;
	cur = pos;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Input::Match(const uni_char c)
{
	OP_ASSERT(cur != NULL);
	uni_char tmp;
	RETURN_IF_ERROR(LA(1, tmp));
	if (tmp != c)
		return OpStatus::ERR;
	++cur;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Input::Match(const uni_char *str, int len)
{
	OP_ASSERT(cur != NULL);
	OP_ASSERT(len > 0);
	const uni_char *pos = cur;
	const uni_char *strend = pos + len;
	if (strend > end)
		return OpStatus::ERR;
	for (; pos != strend; ++pos)
	{
		if (*pos != *str++)
			return OpStatus::ERR;
	}
	cur += len;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Input::MatchRange(const uni_char from, const uni_char to)
{
	OP_ASSERT(cur != NULL);
	uni_char tmp;
	RETURN_IF_ERROR(LA(1, tmp));
	if (tmp < from || tmp > to)
		return OpStatus::ERR;
	++cur;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Input::Extract(const State &start, const State &end_state, const uni_char *&data, int &data_len)
{
	OP_ASSERT(cur != NULL);
	if (start.cur < in || start.cur > end || end_state.cur < in || end_state.cur > end || start.cur > end_state.cur)
		return OpStatus::ERR;
	data = start.cur;
	data_len = end_state.cur - data;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Input::Extract(const State &start, const State &end_state, OpString &str)
{
	OP_ASSERT(cur != NULL);
	const uni_char *data;
	int data_len;
	RETURN_IF_ERROR(Extract(start, end_state, data, data_len));
	RETURN_IF_ERROR(str.Set(data, data_len));
	return OpStatus::OK;
}

OpScopeJSON::Input::State
OpScopeJSON::Input::GetState()
{
	OP_ASSERT(cur != NULL);
	return State(cur);
}

/* OpScopeJSON::Lexer */

void
OpScopeJSON::Lexer::SetErrorPosition(const Position &position)
{
	error.line   = position.line;
	error.column = position.column;
	error.offset = position.offset;
}

OP_STATUS
OpScopeJSON::Lexer::SetError(const uni_char *description)
{
	RETURN_IF_ERROR(error.description.Set(description));
	error.line = pos.line;
	error.column = pos.column;
	error.offset = pos.offset;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::SetError(const uni_char *description, const MatchResult &result)
{
	RETURN_IF_ERROR(error.description.Set(description));
	error.line = result.pos_start.line;
	error.column = result.pos_start.column;
	error.offset = result.pos_start.offset;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::Match(const uni_char c)
{
	RETURN_IF_ERROR(input->Match(c));
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::Match(const uni_char *str, int len)
{
	RETURN_IF_ERROR(input->Match(str, len));
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::MatchRange(const uni_char from, const uni_char to)
{
	RETURN_IF_ERROR(input->MatchRange(from, to));
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::Extract(const MatchResult &start, const uni_char *&data, int &data_len)
{
	return input->Extract(start.start, start.end, data, data_len);
}

OP_STATUS
OpScopeJSON::Lexer::Extract(const MatchResult &start, OpString &data)
{
	return input->Extract(start.start, start.end, data);
}

OP_STATUS
OpScopeJSON::Lexer::NextToken(MatchResult &m)
{
	if (lookahead.GetCount() > 0)
	{
		m = lookahead.Get(0);
		lookahead.Remove(0);
		return OpStatus::OK;
	}
	return NextTokenUncached(m);
}

OP_STATUS
OpScopeJSON::Lexer::LT(unsigned int num, Token &t)
{
	MatchResult res;
	RETURN_IF_ERROR(LT(num, res));
	t = res.token;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::LT(unsigned int num, MatchResult &r)
{
	OP_ASSERT(num > 0);
	if (num == 0)
		return OpStatus::ERR_PARSING_FAILED;
	if (num <= lookahead.GetCount())
	{
		r = lookahead.Get(num - 1);
		return OpStatus::OK;
	}
	int count = num - lookahead.GetCount();

	// Get remaining tokens
	for (int i = 0; i < count; ++i)
	{
		RETURN_IF_ERROR(NextTokenUncached(r));
		RETURN_IF_ERROR(lookahead.Add(r));
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::Match(Token t)
{
	MatchResult tmp;
	RETURN_IF_ERROR(NextToken(tmp));
	if (tmp.token != t)
	{
		SetError(ErrorExpectedToken(t), tmp);
		error.token = t;
		return OpStatus::ERR_PARSING_FAILED;
	}
	return OpStatus::OK;
}

const uni_char *
OpScopeJSON::Lexer::ErrorExpectedToken(Token t)
{
	switch (t)
	{
	case TOK_WS:
		return UNI_L("Expected whitespace");
	case TOK_COMMA:
		return UNI_L("Expected comma");
	case TOK_BRACKET_START:
		return UNI_L("Expected opening square bracket");
	case TOK_BRACKET_END:
		return UNI_L("Expected closing square bracket");
	case TOK_CURLY_BRACKET_START:
		return UNI_L("Expected opening curly bracket");
	case TOK_CURLY_BRACKET_END:
		return UNI_L("Expected closing curly bracket");
	case TOK_INTEGER:
		return UNI_L("Expected integer value");
	case TOK_REAL:
		return UNI_L("Expected real number");
	case TOK_FRACTION:
		return UNI_L("Expected decimal fraction for number");
	case TOK_EXPONENT:
		return UNI_L("Expected exponent for number");
	case TOK_STRING:
		return UNI_L("Expected a string");
	case TOK_NULL:
		return UNI_L("Expected a 'null' value");
	case TOK_TRUE:
		return UNI_L("Expected a boolean 'true'");
	case TOK_FALSE:
		return UNI_L("Expected a boolean 'false'");
	default:
		return UNI_L("Unknown token error");
	}
}

const uni_char *
OpScopeJSON::Lexer::ErrorParsingToken(Token t)
{
	switch (t)
	{
	case TOK_WS:
		return UNI_L("Error while parsing whitespace");
	case TOK_COMMA:
		return UNI_L("Error while parsing comma");
	case TOK_BRACKET_START:
		return UNI_L("Error while parsing opening square bracket");
	case TOK_BRACKET_END:
		return UNI_L("Error while parsing closing square bracket");
	case TOK_CURLY_BRACKET_START:
		return UNI_L("Error while parsing opening curly bracket");
	case TOK_CURLY_BRACKET_END:
		return UNI_L("Error while parsing closing curly bracket");
	case TOK_INTEGER:
		return UNI_L("Error while parsing integer");
	case TOK_REAL:
		return UNI_L("Error while parsing real number");
	case TOK_FRACTION:
		return UNI_L("Error while parsing decimal fraction");
	case TOK_EXPONENT:
		return UNI_L("Error while parsing exponent");
	case TOK_STRING:
		return UNI_L("Error while parsing string");
	case TOK_NULL:
		return UNI_L("Error while parsing 'null' value");
	case TOK_TRUE:
		return UNI_L("Error while parsing boolean 'true'");
	case TOK_FALSE:
		return UNI_L("Error while parsing boolean 'false'");
	default:
		return UNI_L("Unknown parser error");
	}
}

OP_STATUS
OpScopeJSON::Lexer::NextTokenUncached(MatchResult &m)
{
	uni_char la1;
	Input::State start_of_token = input->GetState();
	m.pos_start = pos;
	while (1)
	{
		OP_STATUS status = input->LA(1, la1);
		if (OpStatus::IsError(status))
		{
			SetError(UNI_L("Unexpected end-of-file"));
			return status;
		}
		status = OpStatus::OK;
		Token token = TOK_NONE;
		switch (la1)
		{
		case '[':
			status = mBracketStart();
			token = TOK_BRACKET_START;
			break;
		case ']':
			status = mBracketEnd();
			token = TOK_BRACKET_END;
			break;
		case '{':
			status = mCurlyBracketStart();
			token = TOK_CURLY_BRACKET_START;
			break;
		case '}':
			status = mCurlyBracketEnd();
			token = TOK_CURLY_BRACKET_END;
			break;
		case '-':
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			{
				BOOL is_real = FALSE;
				status = mNumber(is_real);
				token = TOK_INTEGER;
				if (is_real)
					token = TOK_REAL;
			}
			break;
		case ',':
			status = mComma();
			token = TOK_COMMA;
			break;
		case 'n': // null
			status = mNull();
			token = TOK_NULL;
			break;
		case 't': // true
			status = mTrue();
			token = TOK_TRUE;
			break;
		case 'f': // false
			status = mFalse();
			token = TOK_FALSE;
			break;
		case '"':
			status = mString();
			token = TOK_STRING;
			break;
		case '\t': case '\n': case '\r': case ' ':
			status = mWS();
			token = TOK_WS;
			start_of_token = input->GetState();
			m.pos_start = pos;
			continue; // Skip token
		default:
			status = OpStatus::ERR_PARSING_FAILED;
		};
		if (status != OpStatus::OK)
		{
			if (error.description.Length() == 0)
				SetError(ErrorParsingToken(token));
			else
				SetErrorPosition(pos);
			error.token = token;
			return status;
		}
		OP_ASSERT(token != 0);
		m.token = token;
		m.start = start_of_token;
		m.end = input->GetState();
		m.pos_end = pos;

		break;
	}
	return OpStatus::OK;
}
OP_STATUS
OpScopeJSON::Lexer::mComma()
{
	RETURN_IF_ERROR(Match(','));
	pos.column++;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::mWS()
{
	int cnt = 0;
	uni_char tmp;
	OP_STATUS status;
	while (1)
	{
		status = input->LA(1, tmp);
		if (status != OpStatus::OK)
			return (cnt >= 1) ? OpStatus::OK : status;
		switch (tmp)
		{
		case '\t':
			pos.column += tab_size - (pos.column % tab_size);
			input->Consume();
			break;
		case '\n':
			++pos.line;
			pos.column = 0;
			input->Consume(1);
			break;
		case '\r':
			++pos.line;
			pos.column = 0;
			status = input->LA(2, tmp);
			if (status == OpStatus::OK && tmp == '\n')
				input->Consume(2);
			else
				input->Consume(1);
			break;
		case ' ':
			++pos.column;
			input->Consume();
			break;
		default:
			if (cnt >= 1)
				return OpStatus::OK;
			return OpStatus::ERR_PARSING_FAILED;
		}
		++cnt;
	}
	return OpStatus::ERR_PARSING_FAILED;
}

OP_STATUS
OpScopeJSON::Lexer::mBracketStart()
{
	RETURN_IF_ERROR(Match('['));
	pos.column++;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::mBracketEnd()
{
	RETURN_IF_ERROR(Match(']'));
	pos.column++;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::mCurlyBracketStart()
{
	RETURN_IF_ERROR(Match('{'));
	pos.column++;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::mCurlyBracketEnd()
{
	RETURN_IF_ERROR(Match('}'));
	pos.column++;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::mNull()
{
	OP_STATUS status = Match(UNI_L("null"), 4);
	if (OpStatus::IsError(status))
	{
		SetError(UNI_L("Expected 'null' value"));
		return status;
	}
	pos.column += 4;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::mTrue()
{
	OP_STATUS status = Match(UNI_L("true"), 4);
	if (OpStatus::IsError(status))
	{
		SetError(UNI_L("Expected boolean 'true' value"));
		return status;
	}
	pos.column += 4;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::mFalse()
{
	OP_STATUS status = Match(UNI_L("false"), 5);
	if (OpStatus::IsError(status))
	{
		SetError(UNI_L("Expected boolean 'false' value"));
		return status;
	}
	pos.column += 5;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::mNumber(BOOL &is_real)
{
	Input::State start = input->GetState();
	RETURN_IF_ERROR(mNumberInternal(is_real));

	// Check if we matched -0, this cannot be represented as an integer so change it to a real
	OpString data;
	RETURN_IF_ERROR(input->Extract(start, input->GetState(), data));
	if (data.Compare("-0") == 0)
		is_real = TRUE;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Lexer::mNumberInternal(BOOL &is_real)
{
	int cnt = 0;
	uni_char tmp;
	RETURN_IF_ERROR(input->LA(1, tmp));
	if (tmp == '-')
	{
		RETURN_IF_ERROR(input->Consume());
		++pos.column;
	}
	else if (tmp == '0')
	{
		RETURN_IF_ERROR(input->Consume());
		++pos.column;
		++cnt;
	}
	OP_STATUS status;
	is_real = FALSE;
	while (1)
	{
		status = input->LA(1, tmp);
		if (status != OpStatus::OK)
		{
			if (cnt >= 1)
				return OpStatus::OK;
			else
			{
				SetError(UNI_L("Unexpected end-of-file"));
				return status;
			}
		}
		switch (tmp)
		{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			input->Consume();
			break;
		case '.':
			is_real = TRUE;
			RETURN_IF_ERROR(mFraction());
			status = input->LA(1, tmp);
			if (status != OpStatus::OK)
				return (cnt >= 1) ? OpStatus::OK : status;
			if (tmp != 'e' && tmp != 'E')
				return OpStatus::OK;
			// else: continue to exponent checking
		case 'e':
		case 'E':
			is_real = TRUE;
			RETURN_IF_ERROR(mExponent());
			return OpStatus::OK;
		default:
			if (cnt >= 1)
				return OpStatus::OK;
			SetError(UNI_L("Expected decimal digit"));
			return OpStatus::ERR_PARSING_FAILED;
		}
		++cnt;
		pos.column++;
	}
	return OpStatus::ERR_PARSING_FAILED;
}

OP_STATUS
OpScopeJSON::Lexer::mFraction()
{
	uni_char tmp;
	RETURN_IF_ERROR(input->Match('.'));
	pos.column++;
	int cnt = 0;
	while (1)
	{
		OP_STATUS status = input->LA(1, tmp);
		if (status != OpStatus::OK)
		{
			if (cnt >= 1)
				return OpStatus::OK;
			else
			{
				SetError(UNI_L("Expected decimal digit after decimal point"));
				return status;
			}
		}
		switch (tmp)
		{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			input->Consume();
			break;
		default:
			if (cnt >= 1)
				return OpStatus::OK;
			SetError(UNI_L("Expected decimal digit after decimal point"));
			return OpStatus::ERR_PARSING_FAILED;
		}
		++cnt;
		pos.column++;
	}
	return OpStatus::ERR_PARSING_FAILED;
}

OP_STATUS
OpScopeJSON::Lexer::mExponent()
{
	uni_char tmp;
	OP_STATUS status = input->LA(1, tmp);
	if (status != OpStatus::OK)
		return status;
	if (tmp != 'e' && tmp != 'E')
		return OpStatus::ERR_PARSING_FAILED;

	RETURN_IF_ERROR(input->Consume());
	pos.column++;
	status = input->LA(1, tmp);
	if (OpStatus::IsError(status))
	{
		SetError(UNI_L("Unexpected end-of-file"));
		return status;
	}
	if (tmp == '-' || tmp == '+')
	{
		RETURN_IF_ERROR(input->Consume());
		pos.column++;
	}
	int cnt = 0;
	while (1)
	{
		OP_STATUS status = input->LA(1, tmp);
		if (status != OpStatus::OK)
		{
			if (cnt >= 1)
				return OpStatus::OK;
			else
			{
				SetError(UNI_L("Expected decimal digit after exponent"));
				return status;
			}
		}
		switch (tmp)
		{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			input->Consume();
			break;
		default:
			if (cnt >= 1)
				return OpStatus::OK;
			SetError(UNI_L("Expected decimal digit after exponent"));
			return OpStatus::ERR_PARSING_FAILED;
		}
		++cnt;
		pos.column++;
	}
	return OpStatus::ERR_PARSING_FAILED;
}

OP_STATUS
OpScopeJSON::Lexer::mString()
{
	RETURN_IF_ERROR(input->Match('"'));
	pos.column += 1;
	uni_char tmp;
	while (1)
	{
		OP_STATUS status = input->LA(1, tmp);
		if (OpStatus::IsError(status))
		{
			SetError(UNI_L("Unexpected end-of-file"));
			return status;
		}
		if (tmp == '\\')
		{
			status = input->LA(2, tmp);
			if (OpStatus::IsError(status))
			{
				pos.column += 1;
				SetError(UNI_L("Expected double quote"));
				return status;
			}
			switch (tmp)
			{
			case 'b':  case 't': case 'n':
			case 'f':  case 'r': case '"':
			case '\'': case '/': case '\\':
				// Handle escaped character
				pos.column += 2;
				RETURN_IF_ERROR(input->Consume(2));
				continue;
			case 'u':
			{
				RETURN_IF_ERROR(input->Consume(2));
				pos.column += 2;
				// Unicode character requires four hex digits.
				for (int i = 0; i < 4; ++i)
				{
					RETURN_IF_ERROR(input->LA(1, tmp));
					if ((tmp >= '0' && tmp <= '9') ||
						(tmp >= 'A' && tmp <= 'F') ||
						(tmp >= 'a' && tmp <= 'f'))
						RETURN_IF_ERROR(input->Consume());
					else
					{
						SetError(UNI_L("Expected hexadecimal digit for Unicode code point"));
						return OpStatus::ERR_PARSING_FAILED;
					}
					pos.column++;
				}
				continue;
			}
			default:
				pos.column += 1;
				SetError(UNI_L("Expected escape character sequence \\b, \\t, \\n, \\f, \\r, \\\", \\/, \\\\ or \\uxxxx"));
				return OpStatus::ERR_PARSING_FAILED;
			}
		}
		else if (tmp <= 0x1f) // We do not allow special characters
		{
			SetError(UNI_L("Special characters (0x00 - 0x1F) not allowed in JSON strings"));
			return OpStatus::ERR_PARSING_FAILED;
		}
		else if (tmp == '"')
		{
			pos.column++;
			return input->Consume(); // End of string, consume and return
		}
		else // Else we allow the character, UTF-8 decoding will be handled later on
		{
			RETURN_IF_ERROR(input->Consume());
			pos.column++;
		}
	}
	return OpStatus::ERR_PARSING_FAILED;
}

/* OpScopeJSON::Parser */

OP_STATUS
OpScopeJSON::Parser::ParseSignedInteger(INT32 &value)
{
	OpString int_str;
	RETURN_IF_ERROR(ExtractInteger(int_str));

	const long int32_max = 0x7fffffff;
	const long int32_min = -int32_max - 1;

	long number;
	ConvFunction<long> f(uni_strtol);
	RETURN_IF_ERROR(OpProtobufUtils::ParseLong(int_str.DataPtr(), f, int32_min, int32_max, number));
	value = (INT32)number;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Parser::ParseUnsignedInteger(UINT32 &value)
{
	OpString int_str;
	RETURN_IF_ERROR(ExtractInteger(int_str));

	const unsigned long uint32_max = 0xffffffffU;
	const unsigned long uint32_min = 0;

	unsigned long number;
	ConvFunction<unsigned long> f(uni_strtoul);
	RETURN_IF_ERROR(OpProtobufUtils::ParseLong(int_str.DataPtr(), f, uint32_min, uint32_max, number, '-'));
	value = (UINT32)number;
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Parser::ExtractInteger(OpString &str)
{
	OpScopeJSON::Lexer::MatchResult res;
	RETURN_IF_ERROR(lexer.NextToken(res));
	if (res.token != OpScopeJSON::Lexer::TOK_INTEGER)
	{
		lexer.SetError(UNI_L("Expected integer"), res);
		return OpStatus::ERR_PARSING_FAILED;
	}

	return lexer.Extract(res, str);
}

OP_STATUS
OpScopeJSON::Parser::ParseDouble(double &value)
{
	OpScopeJSON::Lexer::MatchResult res;
	RETURN_IF_ERROR(lexer.NextToken(res));
	if (res.token != OpScopeJSON::Lexer::TOK_INTEGER && res.token != OpScopeJSON::Lexer::TOK_REAL)
	{
		lexer.SetError(UNI_L("Expected number or string"), res);
		return OpStatus::ERR_PARSING_FAILED;
	}

	OpString float_str;
	RETURN_IF_ERROR(lexer.Extract(res, float_str));
	uni_char *endptr;
	value = uni_strtod(float_str.DataPtr(), &endptr);
	if (*endptr != '\0')
	{
		lexer.SetError(UNI_L("Failed to parse entire number"));
		return OpStatus::ERR_PARSING_FAILED;
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeJSON::Parser::ParseString(OpString &str)
{
	OpScopeJSON::Lexer::MatchResult res;
	RETURN_IF_ERROR(lexer.NextToken(res));
	if (res.token != OpScopeJSON::Lexer::TOK_STRING)
	{
		lexer.SetError(UNI_L("Expected string"), res);
		return OpStatus::ERR_PARSING_FAILED;
	}

	const uni_char *buf;
	int len;
	RETURN_IF_ERROR(lexer.Extract(res, buf, len));

	// skip the doublequotes
	buf++;
	len -= 2;

	uni_char tmp;
	const uni_char *cur = buf;
	const uni_char *end = buf + len;

	unsigned int needed = 0;

	// Count the number of uni_chars needed.
	while (cur < end)
	{
		const uni_char *last = cur;
		for (; *cur != '\\' && cur < end; ++cur) {};
		needed += (cur - last);
		if (cur == end)
			break;

		if (cur[0] != '\\')
			++cur;
		else if(cur[0] == '\\' && cur[1] != 'u')
			cur += 2;
		else // cur[1] == 'u'
			cur += 6;

		++needed;
	}

	uni_char *storage = OP_NEWA(uni_char, needed+1); // +1 for '\0'
	RETURN_OOM_IF_NULL(storage);

	uni_char *dst = storage;
	cur = buf;
	end = buf + len;

	while (1)
	{
		const uni_char *last = cur;
		for (; *cur != '\\' && cur < end; ++cur) {};
		if (last != cur)
		{
			op_memcpy(dst, last, (cur - last)*sizeof(uni_char));
			dst += (cur - last);
		}
		if (cur == end)
			break;

		tmp = cur[1];
		switch (tmp)
		{
		case 'b':  tmp = '\b'; break;
		case 't':  tmp = '\t'; break;
		case 'n':  tmp = '\n'; break;
		case 'f':  tmp = '\f'; break;
		case 'r':  tmp = '\r'; break;
		case '"':  tmp = '"';  break;
		case '\'': tmp = '\''; break;
		case '/':  tmp = '/';  break;
		case '\\': tmp = '\\'; break;
		case 'u':
		{
			// Unicode character requires four hex digits.
			uni_char c = 0;
			for (int i = 0; i < 4; ++i)
			{
				tmp = cur[i+2];
				if (tmp >= '0' && tmp <= '9')
					c = c*16 + (tmp - '0');
				else if (tmp >= 'A' && tmp <= 'F')
					c = c*16 + (tmp - 'A' + 10);
				else if (tmp >= 'a' && tmp <= 'f')
					c = c*16 + (tmp - 'a' + 10);
				else
				{
					OP_DELETEA(storage);
					return OpStatus::ERR_PARSING_FAILED;
				}
			}
			*dst = c;
			++dst;
			cur += 6;
			continue;
		}
		default:
			OP_DELETEA(storage);
			return OpStatus::ERR_PARSING_FAILED;
		}
		*dst = tmp;
		++dst;
		cur += 2;
	}

	*dst = '\0';

	str.TakeOver(storage);

	return OpStatus::OK;
}

/* Static methods */

/*static*/
OP_STATUS
OpScopeJSON::DumpDouble(ByteBuffer &out, double value)
{
	// NaN and +-Infinite are not allowed in JSON
	OP_ASSERT(op_isfinite(value));
	if (op_isnan(value) || op_isinf(value))
		return OpStatus::ERR_NOT_SUPPORTED;
	char buf[ DECIMALS_FOR_128_BITS + 1 ]; // ARRAY OK 2008-10-17 jborsodi
	if(OpDoubleFormat::ToString(buf, value) == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return out.AppendString(buf);
}

/*static*/
OP_STATUS
OpScopeJSON::DumpSignedLong(ByteBuffer &out, long value)
{
	char buf[ DECIMALS_FOR_128_BITS + 1 ]; // ARRAY OK 2008-10-17 jborsodi
	op_itoa(value, buf, 10);
	return out.AppendString(buf);
}

/*static*/
OP_STATUS
OpScopeJSON::DumpUnsignedLong(ByteBuffer &out, unsigned long value)
{
	char buf[ DECIMALS_FOR_128_BITS + 1 ]; // ARRAY OK 2008-10-17 jborsodi
	OpProtobufUtils::uitoa(value, buf, 10);
	return out.AppendString(buf);
}

/*static*/
OP_STATUS
OpScopeJSON::DumpCString(ByteBuffer &out, const char *string, size_t len)
{
	if (!string || string[0] == '\0')
	{
		RETURN_IF_ERROR(out.AppendString("\"\""));
		return OpStatus::OK;
	}
	const char *iter;
	size_t new_size = 0;
	RETURN_IF_ERROR(out.Append1('"'));
	char escaped[2] = {'\\', 0};
	char uni_escape[7] = {'\\', 'u', '0', '0', 0, 0, 0};
	if (len == ~0u)
		len = op_strlen(string);
	size_t i = 0;
	for (iter = string; i < len; ++iter, ++i)
	{
		switch (*iter)
		{
			case '"':
			case '\\':
				if (new_size)
					RETURN_IF_ERROR(out.AppendBytes(iter - new_size, new_size));
				RETURN_IF_ERROR(out.Append1('\\'));
				new_size = 1;
				continue;
			case '\b': escaped[1] = 'b'; break;
			case '\f': escaped[1] = 'f'; break;
			case '\n': escaped[1] = 'n'; break;
			case '\r': escaped[1] = 'r'; break;
			case '\t': escaped[1] = 't'; break;
			default:
				if (*iter <= 0x1f)
				{
					if (new_size)
						RETURN_IF_ERROR(out.AppendBytes(iter - new_size, new_size));
					if (*iter < 0x10)
					{
						uni_escape[4] = '0';
						op_itoa(*iter, &uni_escape[5], 16);
					}
					else
						op_itoa(*iter, &uni_escape[4], 16);
					RETURN_IF_ERROR(out.AppendBytes(uni_escape, 6));
					new_size = 0;
					continue;
				}
				++new_size;
				continue;
		}
		if (new_size)
			RETURN_IF_ERROR(out.AppendBytes(iter - new_size, new_size));
		RETURN_IF_ERROR(out.AppendBytes(escaped, 2));
		new_size = 0;
	}
	if (new_size)
		RETURN_IF_ERROR(out.AppendBytes(iter - new_size, new_size));
	return out.Append1('"');
}

/*static*/
OP_STATUS
OpScopeJSON::DumpUniString(ByteBuffer &out, const uni_char *string, size_t len)
{
	if (!string || string[0] == '\0')
	{
		RETURN_IF_ERROR(out.AppendString("\"\""));
		return OpStatus::OK;
	}
	const uni_char *iter;
	size_t new_size = 0;
	RETURN_IF_ERROR(out.Append1('"'));
	char escaped[2] = {'\\', 0};
	char uni_escape[7] = {'\\', 'u', '0', '0', 0, 0, 0};
	if (len == ~0u)
		len = uni_strlen(string);
	size_t i = 0;
	for (iter = string; i < len; ++iter, ++i)
	{
		switch (*iter)
		{
			case '"':
			case '\\':
				if (new_size)
					RETURN_IF_ERROR(OpProtobufUtils::Convert(out, iter - new_size, new_size));
				RETURN_IF_ERROR(out.Append1('\\'));
				new_size = 1;
				continue;
			case '\b': escaped[1] = 'b'; break;
			case '\f': escaped[1] = 'f'; break;
			case '\n': escaped[1] = 'n'; break;
			case '\r': escaped[1] = 'r'; break;
			case '\t': escaped[1] = 't'; break;
			default:
				if (*iter <= 0x1f || (*iter >= 0x80 && *iter <= 0x9f))
				{
					if (new_size)
						RETURN_IF_ERROR(OpProtobufUtils::Convert(out, iter - new_size, new_size));
					if (*iter < 0x10)
					{
						uni_escape[4] = '0';
						op_itoa(*iter, &uni_escape[5], 16);
					}
					else
						op_itoa(*iter, &uni_escape[4], 16);
					RETURN_IF_ERROR(out.AppendBytes(uni_escape, 6));
					new_size = 0;
					continue;
				}
				++new_size;
				continue;
		}
		if (new_size)
			RETURN_IF_ERROR(OpProtobufUtils::Convert(out, iter - new_size, new_size));
		RETURN_IF_ERROR(out.AppendBytes(escaped, 2));
		new_size = 0;
	}
	if (new_size)
		RETURN_IF_ERROR(OpProtobufUtils::Convert(out, iter - new_size, new_size));
	return out.Append1('"');
}

/*static*/
OP_STATUS
OpScopeJSON::DumpString(ByteBuffer &out, OpProtobufStringChunkRange range)
{
	if (range.IsEmpty())
	{
		RETURN_IF_ERROR(out.AppendString("\"\""));
		return OpStatus::OK;
	}
	const uni_char *iter;
	RETURN_IF_ERROR(out.Append1('"'));
	char escaped[2] = {'\\', 0};
	char uni_escape[7] = {'\\', 'u', '0', '0', 0, 0, 0};
	for (; !range.IsEmpty(); range.PopFront())
	{
		OpProtobufStringChunk chunk = range.Front();
		const uni_char *string = chunk.GetString();
		size_t len = chunk.GetLength();
		size_t new_size = 0;

		size_t i = 0;
		for (iter = string; i < len; ++iter, ++i)
		{
			switch (*iter)
			{
			case '"':
			case '\\':
				if (new_size)
					RETURN_IF_ERROR(OpProtobufUtils::Convert(out, iter - new_size, new_size));
				RETURN_IF_ERROR(out.Append1('\\'));
				new_size = 1;
				continue;
			case '\b': escaped[1] = 'b'; break;
			case '\f': escaped[1] = 'f'; break;
			case '\n': escaped[1] = 'n'; break;
			case '\r': escaped[1] = 'r'; break;
			case '\t': escaped[1] = 't'; break;
			default:
				if (*iter <= 0x1f || (*iter >= 0x80 && *iter <= 0x9f))
				{
					if (new_size)
						RETURN_IF_ERROR(OpProtobufUtils::Convert(out, iter - new_size, new_size));
					if (*iter < 0x10)
					{
						uni_escape[4] = '0';
						op_itoa(*iter, &uni_escape[5], 16);
					}
					else
						op_itoa(*iter, &uni_escape[4], 16);
					RETURN_IF_ERROR(out.AppendBytes(uni_escape, 6));
					new_size = 0;
					continue;
				}
				++new_size;
				continue;
			}
			if (new_size)
				RETURN_IF_ERROR(OpProtobufUtils::Convert(out, iter - new_size, new_size));
			RETURN_IF_ERROR(out.AppendBytes(escaped, 2));
			new_size = 0;
		}
		if (new_size)
			RETURN_IF_ERROR(OpProtobufUtils::Convert(out, iter - new_size, new_size));
	}
	return out.Append1('"');
}

OP_STATUS
OpScopeJSON::DumpBytes(ByteBuffer &out, const ByteBuffer &in)
{
	int sourcelen = in.Length();
	OpHeapArrayAnchor<char> source(in.Copy());
	if (source.Get() == NULL)
		return OpStatus::ERR_NO_MEMORY;
	char *target = NULL;
	int targetlen = 0;
	MIME_Encode_Error err = MIME_Encode_SetStr(target, targetlen, source.Get(), sourcelen, "UTF-8", GEN_BASE64_ONELINE);
	if (err != MIME_NO_ERROR)
	{
		OP_DELETEA(target);
		return OpStatus::ERR;
	}
	OP_STATUS status = DumpCString(out, target, targetlen);
	OP_DELETEA(target);
	return status;
}

/*static*/
OP_STATUS
OpScopeJSON::DumpBytes(ByteBuffer &out, OpProtobufDataChunkRange range)
{
	int sourcelen = range.Length();
	OpHeapArrayAnchor<char> source(OP_NEWA(char, sourcelen));
	RETURN_OOM_IF_NULL(source.Get());

	OpProtobufContigousDataOutputRange out_range(source.Get(), sourcelen);
	OpProtobufDataOutputAdapter adapter(&out_range);
	RETURN_IF_ERROR(adapter.Put(range));

	char *target = NULL;
	int targetlen = 0;
	MIME_Encode_Error err = MIME_Encode_SetStr(target, targetlen, source.Get(), sourcelen, "UTF-8", GEN_BASE64_ONELINE);
	if (err != MIME_NO_ERROR)
	{
		OP_DELETEA(target);
		return OpStatus::ERR;
	}
	OP_STATUS status = DumpCString(out, target, targetlen);
	OP_DELETEA(target);
	return status;
}

#endif // PROTOBUF_JSON_SUPPORT

#endif // PROTOBUF_SUPPORT
