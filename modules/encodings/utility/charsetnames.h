/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1999-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CHARSETNAMES_H
#define CHARSETNAMES_H

/** The global singleton CharsetManager instance. */
#define g_charsetManager (g_opera->encodings_module.GetCharsetManager())

#ifndef ENCODINGS_REGTEST
# include "modules/util/opstring.h"
# include "modules/util/adt/opvector.h"
#endif

/**
 * Class for handling charset identifiers.
 */
class CharsetManager
{
public:
	// -- Constructor and desctructor -----------
	CharsetManager();
	~CharsetManager();

	/**
	 * Second-phase constructor. Requires that TableManager has been
	 * properly constructed (if ENCODINGS_HAVE_TABLE_DRIVEN is defined).
	 */
	void ConstructL();

	// -- Class methods -------------------------
	/**
	 * Get the canonical name of a character encoding. This function checks
	 * the given charset identifier against a table of known character sets
	 * derived from the IANA character set registry, and returns the
	 * "preferred MIME name" of the encoding. If the encoding name is
	 * unknown, NULL is returned.
	 *
	 * We try to be lenient in the exact spelling of the given encoding
	 * identifier. More specifically, we follow the rules laid out in
	 * section 8.2.2.2 of HTML5: When comparing the given string to a
	 * registered charset name/alias, we are case-insensitive, in additions
	 * to ignoring all whitespace and punctuation characters in ASCII.
	 * See http://www.w3.org/html/wg/html5/#character0 (as of 2008-05-23)
	 * for more info.
	 *
	 * @param charset Encoding identifier.
	 * @param charset_len Optional. Length of above encoding identifier.
	 *                    Defaults to strlen(charset).
	 * @return Canonical name of encoding, NULL if unknown.
	 */
	const char *GetCanonicalCharsetName(const char *charset, long charset_len = -1);

#ifdef ENCODINGS_HAVE_MIB_SUPPORT
	/**
	 * Retrieves the IANA assigned charset name for a MIBenum value. If the
	 * encoding is unknown to Opera, or the value is invalid, NULL is
	 * returned.
	 *
	 * @param mibenum MIBenum value to look up.
	 * @return Canonical name of encoding, NULL if unknown.
	 */
	const char *GetCharsetNameFromMIBenum(int mibenum);
#endif

	// -- Instance methods ----------------------
#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
	/**
	 * Get the list of charsets supported by this version of Opera. The
	 * list will only contain canonical names. The list is static and will
	 * never be deallocated. The final pointer in the array is NULL.
	 *
	 * Returns NULL if there are no charset names and on OOM.
	 */
	inline const char *const *GetCharsetNames()
	{
		return m_all_supported_charsets;
	}
#endif // ENCODINGS_HAVE_TABLE_DRIVEN

#ifndef ENCODINGS_REGTEST
	/**
	 * Retrieve a unique numeric charset identifier (charsetID) from a
	 * charset tag. If the tag has not been seen before, a new unique
	 * number is created, up to a maximum of ENCODINGS_CHARSET_CACHE_SIZE.
	 * If charset is a NULL pointer, charsetID will be 0, and
	 * OpStatus::ERR_NULL_POINTER is returned. The value 0 is never
	 * assigned to a valid tag.
	 *
	 * When the maximum number of encodings have been reached, unreferenced
	 * ids will be reused. If that is not possible, the number for the
	 * corresponding canonical charset will be used instead, if available.
	 * In both cases OpStatus::OK is returned.
	 *
	 * If there are no reusable charset IDs, and a corresponding canonical
	 * charset cannot be found, charsetID is set to 0, and
	 * OpStatus::ERR_OUT_OF_RANGE is returned.
	 *
	 * If the number is to be retained, IncrementCharsetIDReference() must
	 * be called (followed by DecrementCharsetIDReference() when
	 * appropriate)
	 *
	 * @param charset Verbatim charset string, converted to lower case.
	 * @param charsetID A unique identifier for this charset is stored here.
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM,
	 *         otherwise OpStatus::ERR_NULL_POINTER or
	 *         OpStatus::ERR_OUT_OF_RANGE as described above.
	 */
	OP_STATUS GetCharsetID(const char *charset, unsigned short *charsetID);

	/**
	 * Retrieve a unique numeric charset identifier from a charset tag. If
	 * the tag has not been seen before, a new unique number is created, up
	 * to a maximum of ENCODINGS_CHARSET_CACHE_SIZE. If charset is a NULL
	 * pointer, 0 will be returned. The value 0 is never assigned to a valid
	 * tag.
	 *
	 * When the maximum number of encodings have been reached, unreferenced
	 * ids will be reused. If that is not possible, the number for the
	 * corresponding canonical charset will be returned, if available, or 0.
	 *
	 * If the number is to be retained, IncrementCharsetIDReference() must
	 * be called (followed by DecrementCharsetIDReference() when
	 * appropriate)
	 *
	 * @param charset Verbatim charset string, converted to lower case.
	 * @return A unique identifier for this charset.
	 */
	unsigned short GetCharsetIDL(const char *charset);

	/**
	 * Retrieve a string charset identifier from the numeric identifier
	 * returned by GetCharsetIDL().
	 *
	 * @param id A charset identifier as returned from GetCharsetIDL().
	 * @return The string corresponding to this id, or NULL if invalid.
	 */
	const char *GetCharsetFromID(unsigned short id);

	/**
	 * Retrieve a canonical charset identifier from the numeric identifier
	 * returned by GetCharsetIDL().
	 *
	 * @param id A unique identifier.
	 * @return The canonical charset corresponding to this id, or NULL if
	 * none is available.
	 */
	const char *GetCanonicalCharsetFromID(unsigned short id);

	/**
	 * Increment the reference counter for a charset identifier. If this is
	 * not called after GetCharsetIDL() is called, the returned id may be
	 * invalidated by the next call to GetCharsetIDL().
	 *
	 * @param id A charset identifier as returned from GetCharsetIDL().
	 */
	void IncrementCharsetIDReference(unsigned short id);

	/**
	 * Decrement the reference counter for a charset identifier. The holder
	 * of the identifier may not use it after it has called this method.
	 *
	 * @param id A charset identifier as returned from GetCharsetIDL().
	 */
	void DecrementCharsetIDReference(unsigned short id);
#endif // !ENCODINGS_REGTEST

private:
	static size_t Canonize(const char *input, size_t input_len, char *output, size_t max_output_len);
	static int CharsetCompare(const void *, const void *);
#ifdef ENCODINGS_HAVE_MIB_SUPPORT
	static int MIBCompare(const void *key, const void *entry);
#endif

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
	const char **m_all_supported_charsets;	///< Table for GetCharsetNames().
#endif

#ifndef ENCODINGS_REGTEST
	class CharsetInfo
	{
	public:
		/** Construct a CharsetInfo record for a canonical charset
		 *
		 * A canonical CharsetInfo record is always present in the list
		 * of CharsetInfo objects (m_charset_info), and refers to a
		 * canonical charset (whose charset name is const char * (i.e.
		 * always present, and therefore not op_free()d on destruction).
		 *
		 * @param canonical_charset Charset name.
		 * @return A new CharsetInfo record corresponding to the given
		 *         canonical charset name. NULL on OOM.
		 */
		static CharsetInfo *createConstantRecord (const char *canonical_charset);

		/** Construct a CharsetInfo record for a regular charset
		 *
		 * A regular CharsetInfo record may be removed from the list
		 * when its refcount is 0. The refcount starts out at 0, so if
		 * any longevity is required, users must immediately call
		 * IncrementCharsetIDReference() after this record has been put
		 * in the list (m_charset_info).
		 *
		 * A regular CharsetInfo refers to a non-canonical charset.
		 * The CharsetInfo object creates a private copy of the given
		 * charset.
		 *
		 * @param charset Charset name (non-canonical).
		 * @return A new CharsetInfo record corresponding to the given
		 *         non-canonical charset name. NULL on OOM.
		 */
		 static CharsetInfo *createRegularRecord (const char *charset);

		~CharsetInfo()
		{
			// Delete the charset, unless it is a constant string.
			if (isConstant()) return;
			OP_ASSERT(m_ref_count == 0);
			op_free(m_charset);
		}

		const char *getCharset () const
		{
			return m_charset;
		}

		const char *getCanonicalCharset () const
		{
#ifdef ENC_CSETMAN_STORE_CANONICAL
			return m_canonical_charset;
#else
			return g_charsetManager->GetCanonicalCharsetName(m_charset);
#endif
		}

		BOOL isConstant() const
		{
			return m_ref_count == REF_COUNT_CONSTANT;
		}

		int getRefCount() const
		{
			return m_ref_count;
		}

		void incRefCount()
		{
			if (isConstant()) return;
			++m_ref_count;
		}

		void decRefCount()
		{
			if (isConstant()) return;
			OP_ASSERT(m_ref_count > 0);
			--m_ref_count;
		}

	private:
		/** Magic value for reference counter to indicate an internal constant value */
		enum { REF_COUNT_CONSTANT = -1 };

		/** Constructor for internal use only.
		 *
		 * External callers should use factory functions above, instead.
		 */
		CharsetInfo(char *charset, const char *canonical_charset, int ref_count)
		: m_charset(charset),
#ifdef ENC_CSETMAN_STORE_CANONICAL
		  m_canonical_charset(canonical_charset),
#endif
		  m_ref_count(ref_count)
		{}

		char *m_charset; // Charset name (constant if m_ref_count == REF_COUNT_CONSTANT)
#ifdef ENC_CSETMAN_STORE_CANONICAL
		const char *m_canonical_charset; // Canonical charset name
#endif
		int m_ref_count; // Reference count.
	};
	friend class CharsetInfo; // Needed to fix stupid M$ VC++ v6.0 error

	OpAutoVector<CharsetInfo> m_charset_info;
#endif // !ENCODINGS_REGTEST

	// Global variables
#ifdef ENCODINGS_HAVE_MIB_SUPPORT
	struct mibentry
	{
		short mib;
		const char *charset;
	};
#endif // ENCODINGS_HAVE_MIB_SUPPORT

#ifdef HAS_COMPLEX_GLOBALS
	// Standard, compile-time generated, lists.
	/** List of known IANA charset tags and their aliases. */
	static const char * const m_charset_aliases[];

# ifdef ENCODINGS_HAVE_MIB_SUPPORT
	/** Mapping from MIB enum to IANA charset tags. */
	static const mibentry m_mib[];
# endif

#else
	// Run-time generated lists.
	/** List of known IANA charset tags and their aliases. */
	const char **m_charset_aliases;
	/** Initialize m_charset_aliases. */
	BOOL InitCharsetAliasesL();

# ifdef ENCODINGS_HAVE_MIB_SUPPORT
	/** Mapping from MIB enum to IANA charset tags. */
	mibentry *m_mib;
	/** Initialize m_mib. */
	BOOL InitMIBEntriesL();
# endif
#endif
};

#endif // CHARSETNAMES_H
