/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_EDIT_COMMON_H
#define OP_EDIT_COMMON_H

// This header declares functions, variables and types shared between the
// various edit control-related files (OpEdit.cpp, OpMultiEdit.cpp,
// OpTextCollection.cpp). Definitions appear in OpEdit.cpp.

// Recognizes non-ASCII whitespace characters
BOOL IsWhitespaceChar(uni_char c);

// A CharRecognizer has a method is() that returns TRUE for characters
// belonging to a particular class (word character, whitespace, punctuation,
// etc.). This determines regions and boundaries for text in edit boxes.  An
// older solution used function pointers instead of a class hierarchy and
// virtual functions, but that caused problems with BREW.
class CharRecognizer
{
public:
	virtual BOOL is(uni_char c) = 0;
};

#define N_CHAR_RECOGNIZERS 3

class WordCharRecognizer : public CharRecognizer
{
public:
	virtual BOOL is(uni_char c)
	{
		return uni_isalnum(c);
	}
};

class WhitespaceCharRecognizer : public CharRecognizer
{
public:
	virtual BOOL is(uni_char c)
	{
		return IsWhitespaceChar(c);
	}
};

// Non-whitespace characters that act as word delimiters
class WordDelimiterCharRecognizer : public CharRecognizer
{
public:
	virtual BOOL is(uni_char c)
	{
		return !(uni_isspace(c) || uni_isalnum(c));
	}
};

// Returns a CharRecognizer that will recognize characters belonging to the
// same class as 'c'. See the implementation for instructions on how to add a
// new character class.
CharRecognizer *GetCharTypeRecognizer(uni_char c);

// Returns the delta from start to the beginning of the word in the specified
// direction
INT32 SeekWord(const uni_char* str, INT32 start, INT32 step, INT32 max_len);

// These functions find the edges of regions consisting of characters
// belonging to the same class (where two characters are of the same class if
// a sequence of such characters should be considered a unit when
// double-clicking, ctrl-stepping, etc.). They return the position of the
// next/previous character in a different class, or the start or end of the
// line if no such character is found. If 'stay_within_region' is true,
// PrevCharRegion will return the position of the first character in the
// region instead of the character preceding it.
INT32 NextCharRegion(const uni_char* str, INT32 start, INT32 max_len = 1000000);
INT32 PrevCharRegion(const uni_char* str, INT32 start, BOOL stay_within_region);

/**
 The direction of a selection
 */
enum SELECTION_DIRECTION {
	SELECTION_DIRECTION_FORWARD,		///< Forward selection, meaning the caret is at the front of the selection.
	SELECTION_DIRECTION_BACKWARD,		///< Backward selection, meaning the caret is at the back of the selection.
	SELECTION_DIRECTION_NONE			///< No selection direction, meaning the direction of the selection haven't been determined yet.
};
#ifdef RANGESELECT_FROM_EDGE
#define SELECTION_DIRECTION_DEFAULT SELECTION_DIRECTION_NONE ///< The default selection direction.
#else
#define SELECTION_DIRECTION_DEFAULT SELECTION_DIRECTION_FORWARD ///< The default selection direction.
#endif // RANGESELECT_FROM_EDGE

#endif // OP_EDIT_COMMON_H
