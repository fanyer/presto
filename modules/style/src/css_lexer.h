/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_LEXER_H
#define CSS_LEXER_H

#include "modules/hardcore/unicode/unicode.h"
#include "modules/style/src/css_buffer.h"

typedef struct
{
	short token;
	union
	{
		struct
		{
			int first;
			int last;
		} int_pair;
		struct
		{
			unsigned int start_pos;
			unsigned int str_len;
		} str;
		struct {
			unsigned int start_pos;
			unsigned int str_len;
			double number;
		} number;
		struct {
			unsigned int start_pos;
			unsigned int str_len;
			int integer;
		} integer;
		long color;
		bool boolean;
		short shortint;
		struct {
			int steps;
			bool start;
		} steps;
	} value;
} PropertyValue;

#include "modules/style/src/css_grammar.h"

#define CSS_BRACKET_STACK_SIZE 8

class CSS_Lexer
{
public:
	CSS_Lexer(CSS_Buffer* css_buffer, BOOL parse_col = FALSE);
	virtual ~CSS_Lexer();

	int Lex(YYSTYPE* ret_value) { value=ret_value; return ParseToken(); }

	void StoreBlockLevel(int lookahead);

	/** Error recovery. Synchronize on next end-of-block or semi-colon on stored_block_level, or EOF. */
	int RecoverBlockOrSemi(int sync_token);

	/** Syntax error in declaration. Synchronize on next semi-colon, end of declaration block, or EOF. */
	int RecoverDeclaration(int sync_token);

	/** Syntax error in media query. Synchronize on next comma, start of block, or EOF. */
	int RecoverMediaQuery(int sync_token);

	/** Tell the lexer we're parsing a media query list. This method is used for parsing
		a single media query and a media query list for dom operations. */
	void ForceInMediaRule() { in_media_rule = TRUE; }

	/** Return TRUE if we're currently parsing a media query or a media query list. */
	BOOL InMediaRule() const { return in_media_rule; }

	/** Return TRUE if we're currently parsing an @supports rule. */
	BOOL InSupportsRule() const { return in_supports_rule; }

	void UnGetEOF()
	{
		returned_eof_tok = FALSE;
#ifdef CSS_ERROR_SUPPORT
		input_truncated |= TRUNC_SEMI;
#endif // CSS_ERROR_SUPPORT
	}

#ifdef CSS_ERROR_SUPPORT
	enum TruncatedBits {
		TRUNC_NONE = 0,
		TRUNC_STRING = 1,
		TRUNC_PAREN = 2,
		TRUNC_BLOCK = 4,
		TRUNC_SEMI = 8
	};

	int GetCurrentLineNo() const { return line_no; }
	uni_char* GetCurrentLineString(int max_len);
	int GetCurrentLinePos() const { return in_buffer->CurrentPos() - line_start; }
	unsigned char GetTruncationBits() const { return input_truncated; }
#endif // CSS_ERROR_SUPPORT

private:
	/** If parse_color is TRUE, do a fast path for color parsing */
	int ParseToken();

	BOOL LookAheadIsEscape(int n=0);
	BOOL LookAheadIsNMChar(int n=0);
	BOOL LookAheadIsNMStart(int n=0);
	BOOL LookAheadStr(const char* str);

	void EatNMChars();
	void EatWhiteSpaces();

	int GetNMChar();
	int GetEscape();
	int GetNextCharAsToken();

	int ParseNumExpr();
	int ParseUnit();
	int ParseImportantSymbol();
	int ParseAtSymbol();
	int ParseHash();
	void ParseComment();
	int ParseString();
	int ParseIdent();

	int MatchString();
	void MatchURL();

	BOOL MatchNMChars(const char* chars);

	void PushBracket(uni_char ch)
	{
		if (block_level < 0)
			block_level = 0;
		if (block_level < CSS_BRACKET_STACK_SIZE)
			bracket_stack[block_level] = ch;
		if (ch == '(')
			unmatched_paren = TRUE;
		block_level++;
	}

	void PopBracket(uni_char ch)
	{
		if (block_level-- > 0 && block_level < CSS_BRACKET_STACK_SIZE)
		{
			uni_char opening_bracket = bracket_stack[block_level];
			if (!(opening_bracket == '{' && ch == '}' ||
				  opening_bracket == '(' && ch == ')' ||
				  opening_bracket == '[' && ch == ']'))
				block_level++;
		}
	}

	YYSTYPE* value;
	BOOL in_media_rule;
	BOOL in_supports_rule;
	BOOL returned_eof_tok;
	BOOL unmatched_paren;
	BOOL expect_nfunc;
	BOOL parse_color;
	int block_level;
	int stored_block_level;

	CSS_Buffer* in_buffer;

	uni_char bracket_stack[CSS_BRACKET_STACK_SIZE]; /* ARRAY OK 2009-05-13 davve */

#ifdef CSS_ERROR_SUPPORT
	// Current line number of the source.
	int line_no;
	int line_start;
	unsigned char input_truncated;
#endif // CSS_ERROR_SUPPORT
};

#endif // CSS_LEXER_H
