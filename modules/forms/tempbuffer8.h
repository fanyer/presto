/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FORMS_TEMPBUFFER8_H
#define FORMS_TEMPBUFFER8_H

/**
 * We spend some time clearing our buffer before deleting it to
 * decrease the risk that a password gets available through
 * some means. (on Yngve's request). This can be done with memory
 * scanning trojans and we want to make it harder for them.
 *
 * See for instance the discussion in
 * http://my.opera.com/forums/showthread.php?s=&threadid=78808
 */
#define CLEAR_PASSWD_FROM_MEMORY


/**
 * This is a scaled down version of the TempBuffer changed to use 8
 * bit chars instead of uni_char. Its intended use is for encoding
 * data that will be transported in the query part of an url and the
 * data must already be encoded in an 8-bit encoding.
 */
class TempBuffer8
{
public:
	TempBuffer8() : m_storage(NULL), m_storage_size(0), m_str_len(0) {}
	~TempBuffer8() { DeleteBuffer(); }

	/**
	 * Can be wrong if someone outside this code has changed the data.
	 */
	size_t Length() { return m_str_len; }

	/**
	 * Returs true if this buffer has the empty string.
	 */
	BOOL IsEmpty() { return m_str_len == 0; }

	/**
	 * Returns the text accumulated in the tempbuffer.
	 *
	 * Will not return NULL.
	 */
	const char* GetStorage() { return m_storage ? m_storage : ""; }

	/**
	 * Append the string to the end of the buffer, expanding it if needed.
	 *
	 * If the second argument 'length' is not UINT_MAX, then a prefix of
	 * at most length characters is taken from str and appended to the
	 * buffer.
	 */
	OP_STATUS Append(const char *str, unsigned length = UINT_MAX);

	/**
	 * Append the string to the end of the buffer, expanding it if needed.
	 *
	 * If the second argument 'length' is not UINT_MAX, then a prefix of
	 * at most length characters is taken from str and appended to the
	 * buffer.
	 */
	void AppendL(const char* str, unsigned length = UINT_MAX) { LEAVE_IF_ERROR(Append(str, length)); }

	/**
	 * Appends and escapes the text.
	 * Note If using StrLenURLEncoded to calculate length, make sure the parameters correspond
	 *
	 * @param val  text to URL encode
	 * @param use_plus_for_space  Will encode spaces as a plus sign if TRUE,
	 *                            otherwise they will be encoded as %20.
	 * @param is_form_submit Only escape the set of characters suitable for form submit
	 */
	OP_STATUS AppendURLEncoded(const char* val, BOOL use_plus_for_space=TRUE, BOOL is_form_submit=FALSE);

private:
	/** The actual buffer */
	char* m_storage;
	/** The size of the buffer */
	size_t m_storage_size;
	/** The current length of the string in the buffer */
	size_t m_str_len;

	OP_STATUS EnsureSize(size_t new_size);

#ifdef CLEAR_PASSWD_FROM_MEMORY
	/**
	 * Fills the memory with zeroes and then frees it.
	 */
	void DeleteBuffer();
#else
	/**
	 * Frees the memory.
	 */
	void DeleteBuffer() { delete[] m_storage; }
#endif // CLEAR_PASSWD_FROM_MEMORY


	/**
	 * Helper method to calculate how long a string will be after url encoding.
	 * Note If using calculated length with AppendURLEncoded, make sure the parameters correspond
	 *
	 * @param str The string to check. Not NULL.
	 * @param use_plus_for_space  Will encode spaces as a plus sign if TRUE,
	 *                            otherwise they will be encoded as %20
	 * @param is_form_submit Only escape the set of characters suitable for form submit
	 *
	 * @returns the length of an url encoded string.
	 */
	static size_t StrLenURLEncoded(const char* str, BOOL use_plus_for_space, BOOL is_form_submit);
};

#endif // FORMS_TEMPBUFFER8_H
