/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LOCALE_ENUM_H
#define LOCALE_ENUM_H

/**
 * "Namespace" for locale strings. Used not to clutter the global namespace,
 * since there are a lot of these enums. The name is short since it is
 * expected to be used a lot.
 *
 * @author Peter Krefting
 * @see OpLanguageManager
 */
#ifdef LOC_STR_NAMESPACE
// If possible, make this a namespace so that Str::LocaleString can be
// forward-declared
namespace Str
{
#else
class Str
{
public:
#endif // LOC_STR_NAMESPACE
	/*
	 * Locale string identifiers.
	 *
	 * This enumeration contains all the locale strings used by Opera. These
	 * values are used by the LanguageManager to identify the strings, and
	 * are used in the dialog and menu templates to determine their
	 * location.
	 *
	 * You MUST NOT rely on the numerical equivalent of the enumeration
	 * members (the only exception is that NOT_A_STRING is equal to zero).
	 *
	 * DO NOT EDIT the .inc file directly, it is generated automatically by
	 * the operasetup.pike script from the language database file in the
	 * strings module. Any changes done locally to this file will be LOST.
	 *
	 * Platforms re-implementing the LanguageManager might also want to
	 * re-implement or change how this enumeration list is generated to suit
	 * their platform's needs. As long as the enumeration names are kept, it
	 * will be compatible.
	 */
# include "modules/locale/locale-enum.inc"

	/**
	 * Interface class for the locale string identifier.
	 *
	 * This class is used to encapsulate a locale string identifier. The sole
	 * reason for this class is a bug in Visual C++ 6.0 which require
	 * the sum of the length of the names in an enumeration not to exceed
	 * 64 kilobytes.
	 *
	 * This class is light-weight (an int) and can be allocated on the stack
	 * and passed to function calls.
	 */
	class LocaleString
	{
	public:
		// Constructors ---
		/** Construct a string from an enumeration value. */
		LocaleString(enum StringList1 id) : m_id(int(id)) {}
		/** Construct a string from an enumeration value. */
		LocaleString(enum StringList2 id) : m_id(int(id)) {}
		/** Construct a string from an enumeration value. */
		LocaleString(enum StringList3 id) : m_id(int(id)) {}
		/** Construct a string from an enumeration value. */
		LocaleString(enum StringList4 id) : m_id(int(id)) {}
		/** Construct a string from an enumeration value. */
		LocaleString(enum StringList5 id) : m_id(int(id)) {}
		/** Construct a string from an enumeration value. */
		LocaleString(enum StringList6 id) : m_id(int(id)) {}
		/** Construct a string from an enumeration value. */
		LocaleString(enum StringList7 id) : m_id(int(id)) {}
		/** Construct a string from an enumeration value. */
		LocaleString(enum StringList8 id) : m_id(int(id)) {}
		/** Construct a string from an enumeration value. */
		LocaleString(enum StringList9 id) : m_id(int(id)) {}
		/** Construct a string from an enumeration value. */
		LocaleString(enum StringList10 id) : m_id(int(id)) {}
		/** Construct a string from another string. */
		LocaleString(const LocaleString &s) : m_id(s.m_id) {}

#ifdef LOCALE_MAP_OLD_ENUMS
		/** Construct a string from an integer. Supports both old-style
		  * (unhashed) and new-style identifiers. Please avoid using this
		  * if you can. */
		explicit LocaleString(int);
#else
		/** Construct a string from an integer. Please avoid using this
		  * if you can. */
		explicit LocaleString(int id) : m_id(id) {}
#endif

#ifdef LOCALE_SET_FROM_STRING
		/** Construct a string from a string representing the name of
		  * the string (without the Str prefix).
		  */
		explicit LocaleString(const uni_char *);
		explicit LocaleString(const char *);
#endif

		// Assignments ---
		/** Assign an enumeration value. */
		inline void operator=(const enum StringList1 id) { m_id = id; }
		/** Assign an enumeration value. */
		inline void operator=(const enum StringList2 id) { m_id = id; }
		/** Assign an enumeration value. */
		inline void operator=(const enum StringList3 id) { m_id = id; }
		/** Assign an enumeration value. */
		inline void operator=(const enum StringList4 id) { m_id = id; }
		/** Assign an enumeration value. */
		inline void operator=(const enum StringList5 id) { m_id = id; }
		/** Assign an enumeration value. */
		inline void operator=(const enum StringList6 id) { m_id = id; }
		/** Assign an enumeration value. */
		inline void operator=(const enum StringList7 id) { m_id = id; }
		/** Assign an enumeration value. */
		inline void operator=(const enum StringList8 id) { m_id = id; }
		/** Assign an enumeration value. */
		inline void operator=(const enum StringList9 id) { m_id = id; }
		/** Assign an enumeration value. */
		inline void operator=(const enum StringList10 id) { m_id = id; }
		/** Assign a string. */
		inline void operator=(const LocaleString &s) { m_id = s.m_id; }

		// Comparison ---
		/** Compare to an enumeration value. */
		inline BOOL operator==(const enum StringList1 id) const
		{ return int(id) == m_id; }
		/** Compare to an enumeration value. */
		inline BOOL operator==(const enum StringList2 id) const
		{ return int(id) == m_id; }
		/** Compare to an enumeration value. */
		inline BOOL operator==(const enum StringList3 id) const
		{ return int(id) == m_id; }
		/** Compare to an enumeration value. */
		inline BOOL operator==(const enum StringList4 id) const
		{ return int(id) == m_id; }
		/** Compare to an enumeration value. */
		inline BOOL operator==(const enum StringList5 id) const
		{ return int(id) == m_id; }
		/** Compare to an enumeration value. */
		inline BOOL operator==(const enum StringList6 id) const
		{ return int(id) == m_id; }
		/** Compare to an enumeration value. */
		inline BOOL operator==(const enum StringList7 id) const
		{ return int(id) == m_id; }
		/** Compare to an enumeration value. */
		inline BOOL operator==(const enum StringList8 id) const
		{ return int(id) == m_id; }
		/** Compare to an enumeration value. */
		inline BOOL operator==(const enum StringList9 id) const
		{ return int(id) == m_id; }
		/** Compare to an enumeration value. */
		inline BOOL operator==(const enum StringList10 id) const
		{ return int(id) == m_id; }
		/** Compare to a string. */
		inline BOOL operator==(const LocaleString &s) const
		{ return s.m_id == m_id; }

		// Cast ---
		/** Convert to an integer.  */
		inline operator int() const { return m_id; }

	private:
		int m_id;
	};
};
#endif
