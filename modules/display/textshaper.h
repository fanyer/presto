/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifdef TEXTSHAPER_SUPPORT

#ifndef TEXT_SHAPER_H
#define TEXT_SHAPER_H


/** Simple text-shaper, to be used on systems with a font engine that doesn't
 * perform text-shaping on scripts that require it (such as Arabic).
 *
 * This implementation is based on the Arabic shaping classes as defined in
 * the Unicode Character Database file ArabicShaping-4.1.0.txt
 * URL:http://www.unicode.org/Public/UNIDATA/ArabicShaping.txt
 */
class TextShaper
{
public:
	/**
	 * the different joining types an arabic character can have.
	 */
	enum JoiningClass {
		/** The character can join with the previous character, but not the next */
		RIGHT_JOINING,
		/** The character can joing with the next character, but not the previous */
		LEFT_JOINING,
		/** The character can join with both the previous and the next character */
		DUAL_JOINING,
		/** No idea, unused */
		JOIN_CAUSING,
		/** The character cannot joing with other characters */
		NON_JOINING,
		/** The character does not affect joining (and thus, the surrounding
		 * characters join as if the transparent character did not exist) */
		TRANSPARENT_JOINING
							 
	};

	/**
	 * the states the text shaper can have.
	 */
	enum JoiningState {
		NOT_JOINING,  /*!< The previous character could not join (at least not to the left) */
		JOINING_LEFT,  /*!< The previous character could join to the left */
		JOINING_DUAL  /*!< The previous character could join on both sides (not necesarily
					   * used, since right joining does not affect next character) */
	};

	/**
	 * returns the joining class of an arabic character.
	 * @param ch the character from which the joining class is to be obtained.
	 * @return the joining class of the character.
	 */
	static JoiningClass GetJoiningClass(UnicodePoint ch);

	/**
	 * resets the text shaper state - call before processing a string or block of text, when using
	 * GetShapedChar to process a string one (resulting) character at the time.
	 */
	static void ResetState();

	/**
	 * returns one shaped character, processing as much of str as is necessary.
	 * @param str the string from which the shaped char is to be produced.
	 * @param len the length of the string.
	 * @param chars_read (out) the number of uni_chars consumed to produce the shaped character.
	 * the input value of this argument is ignored.
	 * @return the shaped character
	 */
	static UnicodePoint GetShapedChar(const uni_char *str, int len, int& chars_read);

	/**
	 * Transform normal text to cursive text (a string with joinable
	 * characters). For example, from the Arabic block to one of the
	 * two Arabic Presentation Forms blocks. Like this:
	 * - From: U+0627 U+0644 U+0644 U+063A U+0627 U+062A
	 * - To:   U+FE8D U+FEDF U+FEE0 U+FED0 U+FE8E U+FE95
	 *
	 * This method may also apply ligatures.
	 *
	 * @param src the source text (text to transform)
	 * @param slen the length of the source text
	 * @param dest (out) this will be set to a pointer to the transformed text.
	 * The input value of this argument is ignored. Please note that this
	 * pointer and the data pointed to may change after a subsequent call to
	 * this method.
	 * @param dlen (out) this will be set to the number of characters in the
	 * transformed text. The input value of this argument is ignored.
	 */
	static OP_STATUS Prepare(const uni_char *src, int slen, uni_char *&dest, int &dlen);

	/**
	 * Check if a string contains characters that are part of a script that
	 * needs text-shaping.
	 *
	 * @param str the string to check
	 * @param len length of 'str'
	 */
	static BOOL NeedsTransformation(const uni_char *str, int len);
	static BOOL NeedsTransformation(UnicodePoint up);

	static UnicodePoint GetIsolatedForm(UnicodePoint ch) { return GetJoinedForm(ch, 0); }
	static UnicodePoint GetInitialForm(UnicodePoint ch) { return GetJoinedForm(ch, 2); }
	static UnicodePoint GetMedialForm(UnicodePoint ch) { return GetJoinedForm(ch, 3); }
	static UnicodePoint GetFinalForm(UnicodePoint ch) { return GetJoinedForm(ch, 1); }

private:
	static int ConsumeJoiners(const uni_char* str, int len);
	static JoiningClass GetNextJoiningClass(const uni_char* str, int len);
	static UnicodePoint GetJoinedChar(UnicodePoint ch, JoiningClass next_type);

	static UnicodePoint GetJoinedForm(UnicodePoint ch, int num);
//	static int ComposeString(uni_char *dst, const uni_char *src, int len);
};

#endif // !TEXT_SHAPER_H

#endif // TEXTSHAPER_SUPPORT
