/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef ES_JSON_LITE_LEXER_H
#define ES_JSON_LITE_LEXER_H

#ifdef ES_JSON_LITE

// OpValueVector is a vector that contains objects not pointer to objects
// It should be moved out of scope and into util, but given that this
// implementation in ecmascript is highly temporary, I will keep this
// "private" include file.
#include "modules/protobuf/src/opvaluevector.h"
class TempBuffer;
class ByteBuffer;
class OpString8;

class OpJSONLexer
{
public:
	class Input
	{
	public:
		Input() : len(-1), in(NULL), cur(NULL), end(NULL) {}
		OP_STATUS Construct(const uni_char *_in, int _len)
		{
			OP_ASSERT(_in != NULL);
			in = _in;
			len = _len;
			cur = in;
			end = in + len;
			return OpStatus::OK;
		}

		struct State
		{
			State(const uni_char *cur=NULL) : cur(cur) {}
			const uni_char *cur;
		};

		OP_STATUS LA(unsigned int num, uni_char &value) const;
		OP_STATUS Consume(unsigned int num = 1);

		OP_STATUS Match(const uni_char c);
		OP_STATUS Match(const uni_char *str, int len);
		OP_STATUS MatchRange(const uni_char from, const uni_char to);

		OP_STATUS Extract(const State &start, const State &end, const uni_char *&data, int &data_len);
		OP_STATUS Extract(const State &start, const State &end, OpString &data);

		State GetState();

	private:
		int             len;
		const uni_char *in;
		const uni_char *cur;
		const uni_char *end;
	};

	class Lexer
	{
	public:
		Lexer(int tab_size = 8)
			: input(NULL), tab_size(tab_size), pos(1, 0, 0)
		{}
		OP_STATUS Construct(Input *inp)
		{
			input = inp;
			return OpStatus::OK;
		}

		enum Token
		{
			  T_NONE = 0
			, T_WS
			, T_COMMA
			, T_BRACKET_START
			, T_BRACKET_END
			, T_CURLY_BRACKET_START
			, T_CURLY_BRACKET_END
			, T_INTEGER
			, T_REAL
			, T_FRACTION
			, T_EXPONENT
			, T_STRING
			, T_NULL
			, T_TRUE
			, T_FALSE
			, T_COLON
		};

		struct Position
		{
			Position() : line(-1), column(-1), offset(-1) {}
			Position(int line, int column, int offset)
				: line(line), column(column), offset(offset)
			{}
			int line;
			int column;
			int offset;
		};

		struct MatchResult
		{
			MatchResult() : token(T_NONE) {}
			MatchResult(Token t, const Input::State &start, const Input::State &end) : token(t), start(start), end(end) {}
			Token token;
			Input::State start, end;
			Position pos_start, pos_end;
		};

		struct Error
		{
			Error() : token(T_NONE), line(1), column(0), offset(0) {}
			Token token;
			int line;
			int column;
			int offset;
			OpString description;
		};

		OP_STATUS Match(const uni_char c);
		OP_STATUS Match(const uni_char *str, int len);
		OP_STATUS MatchRange(const uni_char from, const uni_char to);
		OP_STATUS NextToken(MatchResult &m);
		OP_STATUS LT(unsigned int num, MatchResult &res);
		OP_STATUS LT(unsigned int num, Token &t);
		OP_STATUS Match(Token t);

		OP_STATUS Extract(const MatchResult &start, const uni_char *&data, int &data_len);
		OP_STATUS Extract(const MatchResult &start, OpString &data);

		const Error &GetError() const { return error; }
		void SetErrorPosition(const Position &position);
		OP_STATUS SetError(const uni_char *description);
		OP_STATUS SetError(const uni_char *description, const MatchResult &res);

	private:
		const uni_char *ErrorExpectedToken(Token t);
		const uni_char *ErrorParsingToken(Token t);

		OP_STATUS mWS();
		OP_STATUS mComma();
		OP_STATUS mBracketStart();
		OP_STATUS mBracketEnd();
		OP_STATUS mCurlyBracketStart();
		OP_STATUS mCurlyBracketEnd();
		OP_STATUS mNull();
		OP_STATUS mTrue();
		OP_STATUS mFalse();
		OP_STATUS mNumber(BOOL &is_real);
		OP_STATUS mFraction();
		OP_STATUS mExponent();
		OP_STATUS mDigit();
		OP_STATUS mString();
		OP_STATUS mColon();

	private:
		OP_STATUS NextTokenUncached(MatchResult &m);

		Input *input;
		OpValueVector<MatchResult> lookahead;
		int tab_size;
		Position pos;
		Error error;
	};

	class Parser
	{
	public:
		Parser(Lexer &lexer) : lexer(lexer) {}

		OP_STATUS ParseSignedInteger(INT32 &value);
		OP_STATUS ParseUnsignedInteger(UINT32 &value);
		OP_STATUS ParseDouble(double &value);
		OP_STATUS ParseString(OpString &str);

		const Lexer &GetLexer() const { return lexer; }
		Lexer &GetLexer() { return lexer; }

	private:
		OP_STATUS ExtractInteger(OpString &str);

		Lexer &lexer;
	};

	static OP_STATUS DumpDouble(ByteBuffer &out, double value);
	static OP_STATUS DumpSignedLong(ByteBuffer &out, long value);
	static OP_STATUS DumpUnsignedLong(ByteBuffer &out, unsigned long value);
	static OP_STATUS DumpCString(ByteBuffer &out, const char *string);
	static OP_STATUS DumpUniString(ByteBuffer &out, const uni_char *string);
	static OP_STATUS DumpBytes(ByteBuffer &out, const ByteBuffer &in);
};

#endif // ES_JSON_LITE

#endif // ES_JSON_LITE_LEXER_H
