/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/util/str/m2tokenizer.h"

#include "modules/util/alloc.h"

// ----------------------------------------------------

Tokenizer::Tokenizer() :
        m_started(0),
        m_nextIsSpecial(FALSE),
        m_whitespaceChars(NULL),
        m_specialChars(NULL),
        m_inputString(NULL),
		m_originalInputString(NULL),
        m_token(NULL),
        m_numWhitespaceChars(0),
        m_numSpecialChars(0),
		m_maxTokenSize(0)
{
}

OP_STATUS Tokenizer::Init(UINT32 maxTokenSize)
{
    m_token = OP_NEWA(char, maxTokenSize);
	m_maxTokenSize = maxTokenSize-2; // Room for null char
	return m_token ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

Tokenizer::~Tokenizer()
{
	OP_DELETEA(m_token);
	op_free(m_originalInputString);
}

void Tokenizer::SetWhitespaceChars(const char* whitespaceChars)
{
	m_whitespaceChars = whitespaceChars;
    m_numWhitespaceChars = strlen( whitespaceChars );
}

void Tokenizer::SetSpecialChars(const char* specialChars)
{
	m_specialChars = specialChars;
    m_numSpecialChars = strlen( specialChars );
}

OP_STATUS Tokenizer::SetInputString(const char* inputString)
{
	if ((m_inputString = op_strdup(inputString)) == NULL)
		return OpStatus::ERR_NO_MEMORY;
	op_free(m_originalInputString);
	m_originalInputString = m_inputString;
	m_started = FALSE;
	return OpStatus::OK;
}

BOOL Tokenizer::IsNextSpecialChar()
{
	if (m_inputString && m_specialChars)
	{
        const char x = *m_inputString;
		UINT32 i;
		for (i = 0; i < m_numSpecialChars; i++)
		{
			if ( x == m_specialChars[i])
			{
				return TRUE;
			}
		}
	}
	// Okay, no special char was matched
	return FALSE;
}

BOOL Tokenizer::IsNextWhitespaceChar(const char* nextChar)
{
	if (nextChar && m_whitespaceChars)
	{
        const char x = *nextChar;
		for (UINT32 i = 0; i < m_numWhitespaceChars; i++)
		{
			if (x == m_whitespaceChars[i])
			{
				return TRUE;
			}
		}
	}
	// Okay, no whitespace char was matched
	return FALSE;
}

void Tokenizer::Start()
{
	m_started = TRUE;
}

/*********************************************
*
* Fill the token buffer with a token
* The peek argument tells whether the
* input string should be stepped forward or not
*
**********************************************/

BOOL Tokenizer::Fill(BOOL peek)
{
	// Save the input string state for later
	char* oldInputString = m_inputString;
	BOOL endOfString = FALSE;

	if (m_inputString && *m_inputString)
	{
		// Fill the token buffer
		BOOL finished = FALSE;
		UINT32 tokenPos = 0;
		while (!finished)
		{
			if (IsNextWhitespaceChar(m_inputString))
			{
				if (tokenPos > 0)
				{
					// We've got a token in the buffer - finish it off!
					m_token[tokenPos] = 0;
					finished = TRUE;
				}
				// Step past
				m_inputString++;
			}
			else
			{
				if (IsNextSpecialChar())
				{
					if (tokenPos > 0)
					{
						// We've got a token in the buffer - finish it off!
						m_token[tokenPos] = 0;
					}
					else
					{
						// Make a new token
						*m_token = *m_inputString++;
						m_token[1] = 0;
					}
					// In all cases, finish loop.
					finished = TRUE;
				}
				else
				{
					if (*m_inputString == 0)
					{
						// End of input - return current token, but with an error code
						if (tokenPos > 0)
						{
							// We've got a token in the buffer - finish it off!
							m_token[tokenPos] = 0;
						}
						else
						{
							// Make a new token
							m_token[1] = 0;
						}
						finished = TRUE;
						endOfString = TRUE;
					}
					else if (tokenPos < m_maxTokenSize)
					// A normal char to read into token buffer
					{
						*(m_token+tokenPos++) = *m_inputString++;
					}
					else
					// No more space in token buffer, skip
					{
						m_inputString++;
					}
				}
			}
		}
	}
	else
	{
		// No input left
		*m_token = 0;
		return FALSE;
	}
	if (peek)
	// Restore the inputString
	m_inputString = oldInputString;

	return (!endOfString); // Whether we reached the end of the input when reading the token or not
}

BOOL Tokenizer::GetNextToken(char* outBuffer, UINT32 bufLen)
{
	if (!outBuffer || !bufLen)
	{
		OP_ASSERT(0);
		return FALSE;
	}

	// Fill the token buffer
	BOOL res = Fill();
	UINT32 tokenLen = strlen(m_token);
	OP_ASSERT(tokenLen <= bufLen - 1); // Room for null char
	op_memcpy(outBuffer, m_token, min(tokenLen, bufLen - 1));
	outBuffer[min(tokenLen, bufLen - 1)] = 0;
	return res;
}

BOOL Tokenizer::PeekNextToken(char* outBuffer, UINT32 bufLen)
{
	if (!outBuffer || !bufLen)
	{
		OP_ASSERT(0);
		return FALSE;
	}

	// Fill the token buffer, with peek parameter true
	BOOL res = Fill(TRUE);
	UINT32 tokenLen = strlen(m_token);
	OP_ASSERT(tokenLen <= bufLen - 1); // Room for null char
	op_memcpy(outBuffer, m_token, min(tokenLen, bufLen - 1));
	outBuffer[min(tokenLen, bufLen - 1)] = 0;
	return res;
}

OP_STATUS Tokenizer::GetNextLineLength(UINT32& pos)
{
	if (!m_inputString)
		return OpStatus::ERR_NULL_POINTER;

	char* charpos = strpbrk(m_inputString, "\r\n");
	if (charpos)
	{
		pos = charpos - m_inputString;
		return OpStatus::OK;
	}
	else
	{
		pos = 0;
		return OpStatus::ERR;
	}
}

UINT32 Tokenizer::GetConsumedChars()
{
	if (m_inputString && m_originalInputString)
		return (m_inputString - m_originalInputString);
	else
		return 0;
}

UINT32 Tokenizer::GetNextCharsLiteral(UINT32 numberOfChars, char* outBuffer)
{
	// Check how many chars are in the buffer...
	UINT32 existingNumberOfChars = strlen(m_inputString);
	UINT32 charsToGet = min(existingNumberOfChars, numberOfChars);
	op_memcpy(outBuffer, m_inputString, charsToGet);
	m_inputString += charsToGet;
	return charsToGet;
}

BOOL Tokenizer::SkipUntil(const char* patternString)
{
	if (patternString && *patternString)
	{
		char* skippedString = strstr(m_inputString, patternString);
		if (skippedString)
			m_inputString = skippedString;
		else
			// Find the end
			m_inputString = m_inputString + strlen(m_inputString);
	}
	return (*m_inputString != 0);
}

BOOL Tokenizer::SkipUntilLinebreak()
{
	char* skippedString = strpbrk(m_inputString, "\r\n");
	if (skippedString)
		m_inputString = skippedString;
	else
		// Find the end
		m_inputString = m_inputString + strlen(m_inputString);

	return (*m_inputString != 0);
}


BOOL Tokenizer::GetLiteralUntil(const char* patternString, char* outBuffer, UINT32 bufLen)
{
	if (patternString)
	{
		char* foundString = strstr(m_inputString, patternString);
		if (foundString)
		{
			UINT32 existingChars = foundString - m_inputString;
			OP_ASSERT(existingChars < bufLen); // The buffer is too short, the token will be truncated
			op_memcpy(outBuffer, m_inputString, min(existingChars, bufLen - 1));
			outBuffer[min(existingChars, bufLen - 1)] = 0;
			m_inputString = foundString;
		}
		else
			// Find the end
			m_inputString = m_inputString + strlen(m_inputString);
	}
	return (m_inputString != 0);
}


BOOL Tokenizer::Skip(size_t n_chars)
{
	m_inputString += n_chars;
	return TRUE;
}

BOOL Tokenizer::SkipNextToken()
{
	return Fill();
}

BOOL Tokenizer::HasData()
{
	return *m_inputString != 0;
}

// ----------------------------------------------------

#endif //M2_SUPPORT
