/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef WORDSEGMENTER_H
#define WORDSEGMENTER_H

#include "modules/search_engine/Vector.h"
#ifdef USE_UNICODE_SEGMENTATION
#include "modules/unicode/unicode_segmenter.h"
#endif

/**
 * @brief Parses plain text into separate words.
 * @author Pavel Studeny <pavels@opera.com>
 *
 * Finds words in any 16-bit unicode script or mixture of scripts.
 * Emails and URLs are detected and returned both segmented to words and as a whole.
 * Special behavior for languages with no spaces between words:
 * returns single characters for Chinese and n-grams for the others
 */
class WordSegmenter : public NonCopyable
{
public:
	enum Flags
	{
		DisableNGrams = 1,       ///< bigrams and trigrams are used for spaceless scripts like hiragana, not desirable for word highlighting
		// DisableSpecialBlock would be easy to implement, if needed
		DontCopyInputString = 2, /**< The input string set with Set will be used directly, not copied.
									  The string must not be deleted before the WordSegmenter is.
									  Note that this flag prevents preprocessing of the input string (e.g. to remove &shy; characters) */
		FineSegmenting = 4       ///< Fine-grained segmenting with additional word boundaries: letter-nonletter, number-nonnumber, lower-uppercase transition
	};

	/* return values of WordSegmenter::GetCharFlags */
	enum CharFlags
	{
		BreakBefore   =   1,  ///< word can end before this character
		BreakAfter    =   2,  ///< word can end this character
		NoBreakBefore =   4,  ///< word mustn't end before this character
		NoBreakAfter  =   8,  ///< word mustn't end after this character
		AlNum         =  16,  ///< letter or number
		CJK           =  32,  ///< chinese characters (china, japan, korea)  returns 1 character
		Hiragana      =  64,  ///< japan     returns 2 characters in a sliding window
		Hangul        = 128,  ///< korea     returns 2 characters in a sliding window
		Katakana      = 256,  ///< japan     returns 2 characters in a sliding window
		Thai          = 512,  ///< thailand  returns 3 characters in a sliding window
		NoSpaceLng    = CJK | Hiragana | Hangul | Katakana | Thai
	};

	struct Word
	{
		const uni_char *ptr;
		int len;

		Word() : ptr(NULL), len(0) {}
		void Set(const uni_char *p, int l) { ptr = p; len = l; }
		void Empty()                       { ptr = NULL; len = 0; }
		BOOL IsEmpty() const               { return ptr==NULL; }
		uni_char* Extract() const          { uni_char* s=OP_NEWA(uni_char,len+1); if(s){ uni_strncpy(s,ptr,len); s[len]=0; } return s; }
		BOOL operator<(const Word &right)  { return (ptr == right.ptr && len < right.len) || ptr < right.ptr; }
	};

	WordSegmenter(unsigned flags = 0);
	~WordSegmenter();

	CHECK_RESULT(OP_STATUS Set(const OpStringC &string)) {return Set(string.CStr());}
	/**
	 * delete the old data, if any, and set a new string
	 */
	CHECK_RESULT(OP_STATUS Set(const uni_char *string));

	/**
	 * get next word from the text set with the Set function.
	 */
	void GetNextToken(Word &token);

	/**
	 * get next word from the text set with the Set function.
	 */
	CHECK_RESULT(OP_BOOLEAN GetNextToken(OpString &token));

	/**
	 * get list of all the words, the words are destructed automatically in the output Vector's destructor
	 * @return NULL on out of memory or if no string had been set
	 */
	TVector<Word> *GetTokens(void);

	/**
	 * get list of all the words, the words are destructed automatically in the output Vector's destructor
	 * @param last_is_prefix if not NULL, will be set to TRUE if the last parsed word can be considered a prefix
	 * @return NULL on out of memory or if no string had been set
	 */
	TVector<uni_char *> *Parse(BOOL *last_is_prefix = NULL);

	/**
	 * @param buf beginning of the string containing the word break
	 * @param s character after the possible word break, s == buf and *s == 0 is allowed
	 * @param fine_segmenting TRUE to achieve the same effect as flag "FineSegmenting"
	 * @return TRUE if a word break is possible before s
	 */
	static BOOL WordBreak(const uni_char *buf, const uni_char *s, BOOL fine_segmenting = FALSE);

	/**
	 * Check if a character belongs to a character class that can be a part of
	 * a word, but is invisible. E.g soft-hyphen (0xAD)
	 * @param ch character to check
	 * @return TRUE if ch is an invisible word character
	 */
	static BOOL IsInvisibleWordCharacter(const uni_char ch);

	/**
	 * Preprocess while duplicating a string to be used in the WordSegmenter.
	 * This removes IsInvisibleWordCharacter characters from the input.
	 * @param string The input string
	 * @return The preprocessed string, or NULL if out of memory
	 */
	static uni_char *PreprocessDup(const uni_char *string);

private:
	// a lot of the work is outsouced to the calling layer #ifdef USE_UNICODE_SEGMENTATION
	inline static int GetCharFlags(UnicodePoint c);

	static BOOL UniStrCompare(const void *left, const void *right);

	const uni_char *m_original_string;
	const uni_char *m_word_break;
	const uni_char *m_original_string_end;

	// Should also possibly run without unicode segmenter.
#ifdef USE_UNICODE_SEGMENTATION
	UnicodeSegmenter m_boundary_finder;
#endif
	unsigned m_flags;
};

#endif  // WORDSEGMENTER_H

