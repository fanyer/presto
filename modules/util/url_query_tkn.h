/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _MODULES_UTIL_URL_QUERY_TKN_H_
#define _MODULES_UTIL_URL_QUERY_TKN_H_

#ifdef URL_QUERY_TOKENIZER_SUPPORT

/**
 * Utility class that iterates over a string with the typical
 * format found in the query arguments in a url:
 *   a=b&c&d=e
 * Bonus points because it does not modify the original script,
 * nor does it allocate memory.
 * 
 * Example usage:
 * 
 * UrlQueryTokenizer itr(string);
 * 
 * while(itr.HasContents())
 * {
 * 	UseName(itr.GetName(), itr.GetNameLength();
 * 	UseName(itr.GetValue(), itr.GetValueLength();
 * 	itr.MoveNext();
 * }
 */
class UrlQueryTokenizer
{
	const uni_char* m_str;
	const uni_char* m_next_amp;
	const uni_char* m_next_equal;
	
	unsigned m_name_length;
	unsigned m_value_length;
	
	static inline const uni_char* nvl(const uni_char* a, const uni_char* b){ return a ? a : b; }
	
public:
	
	/**
	 * Default ctor. After being called, object is ready to be used.
	 */
	UrlQueryTokenizer(const uni_char* subject) { Set(subject); }
	
	/**
	 * Resets the iterator to the beginning of the string
	 * 
	 * @param subject      new string to be iterated upon
	 */
	void Set(const uni_char* subject);
	
	/**
	 * Move to next name/value pair
	 */
	void MoveNext();
	
	/**
	 * Tells if iterator still has pairs to process
	 */
	inline BOOL HasContent() const { return m_str && *m_str; }
	
	/**
	 * Gets the name part of the name/value pair in
	 * the current iterator position
	 */
	inline const uni_char* GetName() const { return m_str; }

	/**
	 * Gets the name length of the name/value pair in
	 * the current iterator position
	 */
	inline unsigned GetNameLength() const { return m_name_length; }

	/**
	 * Gets the value part of the name/value pair in
	 * the current iterator position
	 */
	inline const uni_char* GetValue() const { return m_next_equal; }

	/**
	 * Gets the value length of the name/value pair in
	 * the current iterator position
	 */
	inline unsigned GetValueLength() const { return m_value_length; }
};

#endif //#ifdef URL_QUERY_TOKENIZER_SUPPORT

#endif // _MODULES_UTIL_URL_QUERY_TKN_H_
