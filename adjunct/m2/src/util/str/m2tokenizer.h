// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef M2TOKENIZER_H
#define M2TOKENIZER_H

// ----------------------------------------------------

#include "string.h"
#include "adjunct/m2/src/include/defs.h"
//ee #include "backend/compat/UIutils.h"

// ----------------------------------------------------

class Tokenizer {

// This tokenizer is a light-weight utility built for simplify parsing of 
// various protocols and files, when a full-blown, state of the art lexer is unnecessarily heavy. 
//
// After instansiation, the tokenizer must be fed:
// * a maximal token size (in Init)
// * an input string buffer to tokenize (in SetInputString)
// * a set of whitespace chars that separate tokens (in SetWhitespaceChars)
// * a set of special chars, that is a token in themselves (in SetSpecialChars)
// 
// After this a token is normally read by allocating a buffer and 
// feed this, and its size, to GetNextToken that fills out the buffer
// and consumes all chars until the next token begins. 
// If everything goes allright, TRUE is returned.
// 
// If the input string buffer is emptied during the operation, FALSE is returned. 
//
// If the token is longer than the buffer lenght, it will be truncated to fit, but the
// tokenizer will consume the whole token nevertheless. An assertion will be trigged to
// alert the programmer of the problem, but the function will return TRUE. 
// (If you want to read tokens of a variable and considerable size, and can't make do with the
// GetNextCharsLiteral function, ask me for an improvement.) 
// 
// The above return values and the buffer handling also apply for all the other Get functions.
// 

public:
	Tokenizer();
	~Tokenizer();

	OP_STATUS Init(UINT32 maxTokenSize);

	void SetWhitespaceChars(const char* whitespaceChars);
	void SetSpecialChars(const char* specialChars);
	OP_STATUS SetInputString(const char* inputString);

	BOOL IsNextSpecialChar();
	
	BOOL GetNextToken(char* outBuffer, UINT32 bufLen);
	BOOL PeekNextToken(char* outBuffer, UINT32 bufLen);
	BOOL SkipNextToken();

	BOOL Skip(size_t n_chars);

	// Returns FALSE if the string is not found. 
	BOOL SkipUntil(const char* patternString);
	BOOL SkipUntilLinebreak();

	BOOL GetLiteralUntil(const char* patternString, char* outBuffer, UINT32 bufLen);

	OP_STATUS GetNextLineLength(UINT32& pos);

	/**
	 * Get string of specified length.
	 *
	 * @param outBuffer Buffer to fill
	 * @param numberOfChars Number of characters wanted
	 *
	 * @return Number of characters really returned
	 */
	UINT32 GetNextCharsLiteral(UINT32 numberOfChars, char* outBuffer);
	
	UINT32 GetConsumedChars(); // How many chars have been consumed since the last SetInputString

	BOOL HasData();

private:
    Tokenizer(const Tokenizer&);
	Tokenizer& operator =(const Tokenizer&);

    BOOL IsNextWhitespaceChar(const char*);

	void Start();

	// Fills the token buffer with a token.
	// Returns FALSE if the end of the input is reached 
	BOOL Fill(BOOL peek = FALSE);

	bool m_started;
	bool m_nextIsSpecial;
	const char* m_whitespaceChars;
	const char* m_specialChars;
	char* m_inputString;		 // The input string pointer
	char* m_originalInputString; // Pointer to the original input string
	char* m_token;
	unsigned int m_numWhitespaceChars;
	unsigned int m_numSpecialChars;
	unsigned int m_maxTokenSize;
};

// ----------------------------------------------------
#endif // M2TOKENIZER_H
