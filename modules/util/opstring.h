/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_UTIL_OPSTRING_H
#define MODULES_UTIL_OPSTRING_H

#include "modules/util/opswap.h"

#ifdef MEMTEST
# include "modules/memtools/mem_util.h"
#endif

#include "modules/opdata/OpData.h"
#include "modules/opdata/UniString.h"

class OpString8;
class OpString16;
class OpStringC16;
class InputConverter;
class OutputConverter;

typedef uni_char OpChar;

enum
{
/**
  * Definition - all characters (one meaning of standard c++ npos)
  */
	KAll = -1,
/**
  * Definition - another meaning of standard c++ npos
  */
	KNotFound = -1
};

/*************************************************************************

	class OpStringC8
*/

/**
 * 8-bit constant string class
 */

class OpStringC8
{
public:
	/**
	  * Static - returns the number of characters in a given c-string
	  *
	  * @param aCStr A c-string
	  * @return int The length
	  */
	static int Length(const char* aCStr) { return aCStr ? op_strlen(aCStr) : 0; }

public:
	/**
	  * Blank constructor
	  */
	OpStringC8() : iBuffer(NULL) {}

	/**
	  * C-string constructor
	  *
	  * @param aCStr A C-string to be copied; the underlying data
	  *				 are not copied (the OpString refers to the
	  *				 C-string).
	  */
	OpStringC8(const char* aCStr)
	{
#ifdef MEMTEST
		OOM_REPORT_UNANCHORED_OBJECT();
#endif // MEMTEST
		iBuffer = const_cast<char*>(aCStr);
	}

	/**
	  * Destructor
	  */
	~OpStringC8()
	{
#ifdef MEMTEST
		oom_delete_unanchored_object(this);
#endif // MEMTEST
	}

	/**
	  * Returns the character at a specified index
	  *
	  * @param aIndex An index into the string
	  */
	char operator[](int aIndex) const
	{
		OP_ASSERT(0 <= aIndex && aIndex <= Length());
		return iBuffer[aIndex];
	}
	char operator[](unsigned int aIndex) const
	{
		OP_ASSERT(0 <= (int)aIndex && (int)aIndex <= Length());
		return iBuffer[aIndex];
	}

	/**
	  * Returns whether strings are identical
	  *
	  * @param aCStr The C-string to be compared
	  */
	op_force_inline BOOL operator==(const char* aCStr) const { return (Compare(aCStr)==0); }

	/**
	  * Returns whether strings are identical
	  *
	  * @param aString The string to be compared
	  */
	op_force_inline BOOL operator==(const OpStringC8& aString) const { return (Compare(aString)==0); }

	/**
	  * Returns whether strings are not identical
	  *
	  * @param aCStr The C-string to be compared
	  */
    op_force_inline BOOL operator!=(const char* aCStr) const { return !operator==(aCStr); }

	/**
	  * Returns whether strings are not identical
	  *
	  * @param aString The string to be compared
	  */
    op_force_inline BOOL operator!=(const OpStringC8& aString) const { return !operator==(aString); }

	/**
	  * Returns a pointer to the text data
	  *
	  * @return const char* A pointer to the (constant) data
	  */
	const char* DataPtr() const { return iBuffer; }

	/**
	  * Returns a c-string equivalent
	  *
	  * @return const char* A pointer to a c-string representation of the string
	  */
	const char* CStr() const { return DataPtr(); }

	/**
	  * Returns the number of characters in the string
	  *
	  * @return int The length
	  */
	int Length() const { return iBuffer ? op_strlen(iBuffer) : 0; }

	/**
	  * Compares the string with another specified string
	  *
	  * @param aString The string to be compared
	  * @return int (<0 mean this is less than aString, 0 means they're equal,
	  *				 >0 means this is greater than aString)
	  */
	int Compare(const OpStringC8& aString) const
	{
		return Compare(aString.CStr(), KAll);
	}

	/**
	  * Compares the string with a c-string
	  *
	  * @param aCStr The c-string to be appended
	  * @param aLength A number of characters from the c-string to compare
					(default is all subsequent characters)
	  * @return int (<0 mean this is less than the c-string, 0 means they're equal,
	  *				 >0 means this is greater than the c-string)
	  */
	int Compare(const char* aCStr, int aLength=KAll) const;

	/**
	  * Compares the string with another specified string (case-insensitive)
	  *
	  * Note, this function simply maps to strcmp
	  *
	  * @param aString The string to be compared
	  * @return int (<0 mean this is less than aString, 0 means they're equal,
	  *				 >0 means this is greater than aString)
	  */
	int CompareI(const OpStringC8& aString) const
	{
		return CompareI(aString.CStr(), KAll);
	}

	/**
	  * Compares the string with a c-string (case-insensitive)
	  *
	  * Note, this function simply maps to strcmp
	  *
	  * @param aCStr The c-string to be compared
	  * @param aLength A number of characters to compare from the c-string
	  *				(default is all subsequent characters)
	  * @return int (<0 mean this is less than the c-string, 0 means they're equal,
	  *				 >0 means this is greater than the c-string)
	  */
	int CompareI(const char* aCStr, int aLength=KAll) const;

	/**
	  * Finds a string within this string
	  *
	  * @param aString The string to be searched for
	  * @return int the character position of the found string (or KNotFound)
	  */
	op_force_inline int Find(const OpStringC8& aString) const
	{
		return Find(aString.CStr());
	}

	/**
	  * Finds a c-string within this string
	  *
	  * @param aCStr The c-string to be searched for
	  * @param aStartIndex index from where to start looking for the needle, defaults to 0
	  * @return int the character position of the found string (or KNotFound)
	  */
	int Find(const char* aCStr, int aStartIndex = 0) const;

	/**
	  * Finds a string within this string
	  *
	  * @param aCStr The c-string to be searched for
	  * @param aStartIndex index from where to start looking for the needle, defaults to 0
	  * @return int the character position of the found string (or KNotFound)
	  */
	int FindI(const char* aCStr, int aStartIndex = 0) const;

	/**
	  * Finds the first occurrence of character within the string
	  *
	  * Provides the functionality of the c-library function
	  * strchr
	  *
	  * @param aChar A character to locate
	  * @return int the position of the found char (or KNotFound)
	  */
	int FindFirstOf(char aChar) const;

	/**
	  * Finds the last occurrence of character within the string
	  *
	  * Provides the functionality of the c-library function
	  * strrchr
	  *
	  * @param aChar A character to locate
	  * @return int the position of the found char (or KNotFound)
	  */
	int FindLastOf(char aChar) const;

	/**
	  * Finds the length of the initial segment of the string which
	  * consists entirely of characters from the specified string
	  *
	  * Provides the functionality of the c-library function strspn.
	  *
	  * @param aCharsString The string of characters from which
	  *						the substring must be composed
	  * @return int the length of the found string.
	  */
	int SpanOf(const OpStringC8& aCharsString) const;

	/**
	  * Finds the length of the initial segment of the string which
	  * consists entirely of characters not in the specified string
	  *
	  * Provides the functionality of the c-library function strcspn.
	  *
	  * @param aCharsString The string of characters from which
	  *						the substring must not be composed
	  * @return int the length of the found string.
	  */
	int SpanNotOf(const OpStringC8& aCharsString) const;

	/**
	  * Finds the first occurrence within the string of any of a
	  * string of specified character
	  *
	  * Provides the functionality of the c-library function strpbrk
	  * (but returns index within string, rather than pointer into
	  * string, on success; it is thus distinguishable from SpanNotOf
	  * only on failure).
	  *
	  * @param aCharsString The string of character any of
	  *						which will satisfy the search
	  * @param idx Starting index to search from
	  * @return int the position of the found character (or KNotFound)
	  */
	int FindFirstOf(const OpStringC8& aCharsString, int idx = 0) const;

	/**
	  * Checks empty string
	  *
	  * @return BOOL Returns TRUE if string length == 0, FALSE otherwise
	  */
	op_force_inline BOOL IsEmpty() const { return iBuffer == NULL || *iBuffer == 0; }


	/**
	  * Checks for content. Reverse of IsEmpty, and thus not really
	  * needed, but normally one wants to check if string actually
	  * has content and code reading
	  *
	  * if (string.HasContent()) is just so much more readable than
	  * if (!string.IsEmpty())
	  *
	  * @return BOOL Returns TRUE if string length > 0, FALSE otherwise
	  */
	op_force_inline BOOL HasContent() const { return iBuffer && *iBuffer; }

	/**
	  * Cast operator
	  *
	  * For use where a c-string pointer is expected
	  *
	  * INCLUDED TO SUPPORT EXISTING LEGACY CODE
	  * @deprecated Use .CStr() overtly !
	  *
	  * @return const char* A pointer to a c-string
	  */
	DEPRECATED(operator const char*() const); // inline after class

	/**
	  * Returns a sub-string
	  *
	  * @param aPos The character index at which to start
	  * @return OpStringC8 The sub-string
	  */
	OpStringC8 SubString(unsigned int aPos) const { OP_ASSERT(static_cast<int>(aPos)<=Length()); OpStringC8 substring(iBuffer + aPos); return substring; }

	/**
	  * Deprecated sub-string functionality
	  * Use instead for equivalent functionality: newstring.Set(oldstring.SubString(aPos).CStr(), aLength);
	  */
	DEPRECATED(OpString8 SubString(unsigned int aPos, int aLength, OP_STATUS* aErr = NULL) const);

	/**
	 * Append this string to an OpData object.
	 *
	 * Since this is supposed to be a _constant_ string, we use OpData's
	 * AppendConstData() which can use the given data without copying.
	 * We may have to change this into AppendCopyData(), because OpString
	 * is misdesigned and abused, and the string may end up being less
	 * "constant" than anticipated, which will cause problems for OpData.
	 *
	 * @param d The OpData object to which this string should be appended.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	op_force_inline OP_STATUS AppendToOpData(OpData& d) const
	{
		return HasContent()
			? d.AppendConstData(iBuffer, Length())
			: OpStatus::OK;
	}

protected:
	/**
	  * The data is currently kept as a null-terminated char*
	  */
	char* iBuffer;
};

inline OpStringC8::operator const char*() const { return CStr(); }

/*************************************************************************

	class OpString8
*/

/**
 * 8-bit changeable string class.
 *
 * Note that since this class is derived from OpStringC8 which do not
 * have a virtual destructor, data allocated by an instance of this
 * class will not be freed if freed through a pointer to an OpStringC8
 * object.
 */
class OpString8 : public OpStringC8
{
private:
	/**
	  * Assignment operator
	  *
	  * NEVER USE THIS.  Private to avoid it being used in client code.
	  */
	OpString8& operator=(const OpString8& aString);
public:
	/**
	  * Default constructor
	  *
	  * Creates an empty string of length 0
	  */
	OpString8()
	{
#ifdef MEMTEST
		OOM_REPORT_UNANCHORED_OBJECT();
#endif // MEMTEST
		iSize = 0;
	}

	/** Pillaging constructor
	 *
	 * IMPORTANT: This method is DANGEROUS, as it STEALS the data from the given
	 * object. Yes, that's right, the object passed to this method will be EMPTY
	 * when this method returns. This is why it's no longer declared const.
	 *
	 * @param aString A string to be "copied" (i.e. pillaged).
	 */
	OpString8(OpString8& aString);

	/**
	  * Destructor
	  */
	op_force_inline ~OpString8() { OP_DELETEA(iBuffer); }

	/**
	  * Second-phase constructor
	  *
	  * Creates an OpString from a C string
	  *
	  * Note - an index is not included as the pointer itself can be indexed
	  *
	  * @param aCStr A c-style char*
	  * @param aLength A number of characters to copy (default is all subsequent characters)
	  */
	OpString8& ConstructL(const char* aCStr, int aLength=KAll);

	/**
	  * Second-phase constructor
	  *
	  * Creates an 8-bit OpString8 from a unicode char*
	  *
	  * Note - INFORMATION WILL BE LOST
	  *
	  * @param aCStr A c-style unicode char*
	  * @param aLength Number of chars to copy.
	  */
	OpString8& ConstructL(const uni_char* aCStr, int aLength=KAll);

	/** Copies the contents of another string.
	 *
	 * @param aString The string to be copied
	 * @param aLength Number of chars to copy, or KAll (the default) to copy the
	 *  whole string.
	 * @return Some variants (which may OOM) return OP_STATUS; the others (which
	 *  may LEAVE) return *this.
	 */
	OP_STATUS Set(const OpStringC8& aString, int aLength=KAll)
		{ return Set(aString.CStr(), MIN(aLength, aString.Length())); }
	/**
	 * @overload
	 */
	OP_STATUS Set(const char* aCStr, int aLength=KAll);
	/**
	 * @overload
	 */
	OpString8& SetL(const OpStringC8& aString, int aLength=KAll);
	/**
	 * @overload
	 */
	OpString8& SetL(const char* aCStr, int aLength=KAll);

	/** Copy Unicode contents from a C-style string or another string object.
	 *
	 * @note This method simply down-codes the Unicode; it should only be used
	 * if the Unicode string is in fact a plain ASCII string.
	 *
	 * @param aCStr The string to be copied.
	 * @param aLength Number of chars to set or KAll (the default) to copy the whole string.
	 * @return Some variants (which may OOM) return OP_STATUS; the others (which
	 *  may LEAVE) return *this.
	 */
	OP_STATUS Set(const uni_char* aCStr, int aLength=KAll);
	/**
	 * @overload
	 */
	OP_STATUS Set(const OpStringC16 &aCStr, int aLength=KAll);
	/**
	 * @overload
	 */
	OpString8& SetL(const uni_char* aCStr, int aLength=KAll);
	/**
	 * @overload
	 */
	OpString8& SetL(const OpStringC16 &aCStr, int aLength=KAll);

	/** Insert content from a C-string or a string object
	 *
	 * @param aPos The index in which after which to insert the new content.
	 * @param aString The string to be inserted.
	 * @param aLength Number of bytes to copy from aString, or KAll (the
	 *  default) to copy the whole string.
	 * @return Some variants (which may OOM) return OP_STATUS; the others (which
	 *  may LEAVE) return *this.
	 */
	OP_STATUS Insert(int aPos,const OpStringC8& aString, int aLength=KAll)
		{ return Insert(aPos, aString.CStr(), MIN(aLength, aString.Length())); }
	/**
	 * @overload
	 */
	OP_STATUS Insert(int aPos,const char* aCStr, int aLength=KAll);
	/**
	 * @overload
	 */
	OpString8& InsertL(int aPos,const OpStringC8& aString, int aLength=KAll)
		{ return InsertL(aPos, aString.CStr(), MIN(aLength, aString.Length())); }
	/**
	 * @overload
	 */
	OpString8& InsertL(int aPos, const char* aCStr, int aLength=KAll);

	/** Append content from a C-string or a string object.
	 *
	 * @param aString The string to be appended
	 * @param aLength Number of bytes to copy from aString, or KAll (the
	 *  default) to copy the whole string.
	 * @return Some variants (which may OOM) return OP_STATUS; the others (which
	 *  may LEAVE) return *this.
	 */
	OP_STATUS Append(const OpStringC8& aString, int aLength=KAll)
		{ return Append(aString.CStr(), MIN(aLength, aString.Length())); }
	/**
	 * @overload
	 */
	OP_STATUS Append(const char* aCStr, int aLength=KAll);
	/**
	 * @overload
	 */
	OpString8& AppendL(const OpStringC8& aString, int aLength=KAll)
		{ return AppendL(aString.CStr(), MIN(aLength, aString.Length())); }
	/**
	 * @overload
	 */
	OpString8& AppendL(const char* aCStr, int aLength=KAll);

	/** Concatenate several strings.
	 *
	 * Up to eight strings may be supplied.
	 *
	 * @return See OpStatus; may OOM.
	 */
	OP_STATUS SetConcat(
		const OpStringC8& str1, const OpStringC8& str2,
		const OpStringC8& str3, const OpStringC8& str4,
		const OpStringC8& str5,
		const OpStringC8& str6 = OpStringC8(NULL),
		const OpStringC8& str7 = OpStringC8(NULL),
		const OpStringC8& str8 = OpStringC8(NULL));
	/**
	 * @overload
	 */
	OP_STATUS SetConcat(
		const OpStringC8& str1, const OpStringC8& str2,
		const OpStringC8& str3 = OpStringC8(NULL),
		const OpStringC8& str4 = OpStringC8(NULL));

	/** Convert a string from utf-16 to utf-8 (with allocation)
	 *
	 * NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE!
	 *
	 * This will make the OpString8 contain a UTF-8 string - whose length,
	 * measured in bytes, is not the number of characters it encodes.
	 *
	 * NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE!
	 *
	 * An OpString8 containing UTF-8 can not be converted to an OpString16 using
	 * Set(); OpString16::SetFromUTF8() is needed.
	 *
	 * NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE! NOTA BENE!
	 *
	 * @param aUTF16str A UTF-16 encoded string
	 * @param aLength Number of UTF-16 characters to convert
	 * @return Some variants (which may OOM) return OP_STATUS; the others (which
	 *  may LEAVE) return *this.
	 */
	OP_STATUS SetUTF8FromUTF16(const uni_char* aUTF16str, int aLength = KAll);
	/**
	 * @overload
	 */
	OP_STATUS SetUTF8FromUTF16(const OpStringC16 &aUTF16str, int aLength = KAll);
	/**
	 * @overload
	 */
	OpString8& SetUTF8FromUTF16L(const uni_char* aUTF16str, int aLength = KAll);
	/**
	 * @overload
	 */
	OpString8& SetUTF8FromUTF16L(const OpStringC16 &aUTF16str, int aLength = KAll);

	/** Extends the length of the buffer if necessary
	 *
	 * Specify the size you need the OpString to be for what you're about to do
	 * to it.  This can be used either to obtain a modifiable buffer in which to
	 * do what you will "by hand" or simply, when you plan to make lots of
	 * changes to the string, to ensure that it performs one big allocation up
	 * front instead of lots of smaller ones - this can save a lot of heap
	 * traffic.
	 *
	 * @param aMaxLength Space desired.
	 * @return The resulting .CStr(), on success.  One variant returns @c NULL
	 * on OOM, the other \c LEAVE()s.
	 */
	char* Reserve(int aMaxLength);
	/**
	 * @overload
	 */
	char* ReserveL(int aMaxLength);

	/**
	  * Wipes the contents of the string (replaces each char with 0)
	  * but doesn't alter any other aspect
	  *
	  * Used for security purposes
	  */
	void Wipe();

	/**
	  * Returns the number of allocated characters (excluding null-terminator if used)
	  *
	  * @return int The number of characters allocated
	  */
	int Capacity() const { return iSize; }

	/** Discards part of the string.
	 *
	 * @param aPos The character index at which to start the delete
	 * @param aLength A number of characters to delete (default, KAll, means: to
	 *  end of string).
	 * @return OpString8& The string (this)
	 */
	OpString8& Delete(int aPos, int aLength=KAll);

	/**
	  * Strips leading and/or trailing whitespace from string
	  * @param leading Whether to strip leading whitespace
	  * @param trailing Whether to strip trailing whitespace
	  * @return OpString8& The string (this)
	  */
	OpString8& Strip(BOOL leading = TRUE, BOOL trailing = TRUE);

	/**
	  * Overtakes another string without allocation, the overtaken string doesn't
	  * contain any data after the operation
	  * @param aString The string to be overtaken
	  * @returns OP_STATUS Result of the operation
	  */

	OP_STATUS TakeOver(OpString8& aString);

	/**
	  * Returns the char at a specified index
	  *
	  * @param aIndex An index into the string
	  * @return char& The character at the position specified
	  */
	char& operator[](int aIndex)
	{
		OP_ASSERT(0 <= aIndex && aIndex <= Capacity());
		return iBuffer[aIndex];
	}
	char& operator[](unsigned int aIndex)
	{
		OP_ASSERT(0 <= (int)aIndex && (int)aIndex <= Capacity());
		return iBuffer[aIndex];
	}
	char operator[](int aIndex) const
	{
		OP_ASSERT(0 <= aIndex && aIndex <= Capacity());
		return iBuffer[aIndex];
	}
	char operator[](unsigned int aIndex) const
	{
		OP_ASSERT(0 <= (int)aIndex && (int)aIndex <= Capacity());
		return iBuffer[aIndex];
	}

	/**
	  * Returns a pointer to the text data
	  *
	  * @return char* A pointer to the data
	  */
	char* DataPtr() { return iBuffer; }
	const char* DataPtr() const { return iBuffer; }

	/**
	  * Returns a c-string equivalent
	  *
	  * @return char* A c-string
	  */
	char* CStr() { return DataPtr(); }
	const char* CStr() const { return DataPtr(); }

	/**
	  * Cast operator
	  *
	  * For use where a c-string pointer is expected
	  *
	  * INCLUDED TO SUPPORT EXISTING LEGACY CODE
	  * @deprecated Use .CStr() overtly !
	  *
	  * @return char* A pointer to a c-string
	  */
	DEPRECATED(operator char*()); // inline after class
	DEPRECATED(operator const char*() const); // inline after class

	/**
	 * Takes over the pointer aString, without allocation.
	 *
	 * IMPORTANT: OpString assumes that the given string was allocated with
	 * OP_NEWA(), and will deallocate it with OP_DELETEA() when no longer
	 * needed.
	 */
	void TakeOver(char* aString);

	/**
	 * Appends a string from a format/vararg list.
	 *
	 * The output is the one defined by C99 sprintf. Note that
	 * for floating point numbers, the output will be locale dependant.
	 *
	 * @param format A c-style char* format % string
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS AppendFormat(const char* format, ...);
	OP_STATUS AppendVFormat(const char* format, va_list args);

	void Empty();

	/**
	  * Makes string uppercase
	  *
	  */
	void MakeUpper();

	/**
	 * Replaces all occurrences of needle by subject, from left to right
	 *
	 * @param needle       what to look for, that will be replaced.
	 *                     If NULL or empty, call does nothing
	 * @param subject      the new stuff that will replace subject, can be NULL,
	 *                     and if so will be treated as an empty string
	 * @param occurrences  the function will replace at most
	 *                     number_of_occurrences times the needle string.
	 *                     Use value -1 for all
	 */
	OP_STATUS ReplaceAll(const char* needle, const char* subject, int occurrences = -1);

	/**
	 * Append this string to an OpData object.
	 *
	 * Since the data owned by this string may change after this call, we
	 * must _copy_ the data into the OpData object. Hence, we use OpData's
	 * AppendCopyData() method.
	 *
	 * @param d The OpData object to which this string should be appended.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	op_force_inline OP_STATUS AppendToOpData(OpData& d) const
	{
		return HasContent()
			? d.AppendCopyData(iBuffer, Length())
			: OpStatus::OK;
	}

	/**
	 * Append the released contents of this string to an OpData object.
	 *
	 * Append this string to the given OpData object, transferring ownership
	 * of the string data in the process. This means that this OpString can
	 * no longer refer to the string data, so this object will be empty upon
	 * a successful return.
	 *
	 * The data transferred to the OpData object is assumed to have been
	 * allocated with OP_NEWA(), so we will instruct OpData to use
	 * OP_DELETEA() for deallocating the data when no longer needed.
	 *
	 * @param d The OpData object to which this string should be appended.
	 * @retval OpStatus::OK on success. This string will be empty.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM. This string will be unchanged.
	 */
	op_force_inline OP_STATUS ReleaseToOpData(OpData& d)
	{
		if (HasContent())
		{
			RETURN_IF_ERROR(d.AppendRawData(
				OPDATA_DEALLOC_OP_DELETEA, iBuffer, Length(),
				iSize ? static_cast<size_t>(iSize + 1) : OpDataUnknownLength));
			iBuffer = NULL;
			iSize = 0;
		}
		return OpStatus::OK;
	}

	/**
	 * Copy the given OpData into this OpString object.
	 *
	 * On success this object will refer to a copy of the data in \c d, and
	 * OpStatus::OK will be returned. Otherwise, OpStatus::ERR_NO_MEMORY is
	 * returned, and this string remains unchanged.
	 *
	 * @param d The OpData object to copy from.
	 * @retval OpStatus::OK on success. This string holds a copy of \c d.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM. This string will be unchanged.
	 */
	op_force_inline OP_STATUS SetFromOpData(const OpData& d)
	{
		char *newBuffer = OP_NEWA(char, d.Length() + 1);
		RETURN_OOM_IF_NULL(newBuffer);
		size_t copied = d.CopyInto(newBuffer, d.Length());
		OP_ASSERT(copied == d.Length());
		(void) copied;
		newBuffer[d.Length()] = '\0';

		Empty();
		iBuffer = newBuffer;
		iSize = d.Length();
		return OpStatus::OK;
	}

protected:
	OP_STATUS Grow(int aLength);

	int iSize; // Number of allocated characters (excluding null-terminator if used)

	friend int SetToEncodingL(OpString8 *target, const char *aEncoding, const uni_char *aUTF16str, int aLength);
	friend int SetToEncodingL(OpString8 *target, const OpStringC8 &aEncoding, const OpStringC16 &aUTF16str, int aLength);
	friend int SetToEncodingL(OpString8 *target, OutputConverter *aEncoder, const uni_char *aUTF16str, int aLength);
	friend int SetToEncodingL(OpString8 *target, OutputConverter *aEncoder, const OpStringC16 &aUTF16str, int aLength);
};

inline OpString8::operator char*() { return CStr(); }
inline OpString8::operator const char*() const { return CStr(); }

/*************************************************************************

    class OpStringC16
*/

/**
 * 16-bit constant string class
 */

class OpStringC16
{
public:
	/**
	  * Static - returns the number of characters in a given c-string
	  *
	  * @param aCStr A c-string
	  * @return int The length
	  */
	static int Length(const char* aCStr);

	/**
	  * Static - returns the number of characters in a given c-string
	  *
	  * @param aCStr A c-string
	  * @return int The length
	  */
	static int Length(const uni_char* aCStr);

public:
	/**
	  * Blank constructor
	  */
	OpStringC16()
	{
#ifdef MEMTEST
		OOM_REPORT_UNANCHORED_OBJECT();
#endif // MEMTEST
		iBuffer = NULL;
	}

	/**
	  * C-string constructor
	  *
	  * @param aCStr A 16-bit c-string
	  */
	OpStringC16(const uni_char* aCStr) { iBuffer = const_cast<uni_char*>(aCStr); }

	/**
	  * Destructor
	  */
	~OpStringC16()
	{
#ifdef MEMTEST
		oom_delete_unanchored_object(this);
#endif // MEMTEST
	}

	/**
	  * Returns the character at a specified index
	  *
	  * @param aIndex An index into the string
	  */
	uni_char operator[](int aIndex) const
	{
		OP_ASSERT(0 <= aIndex && aIndex <= Length());
		return iBuffer[aIndex];
	}
#ifdef UTIL_OPERATOR_SUPPORT
	uni_char operator[](unsigned int aIndex) const
	{
		OP_ASSERT(0 <= (int)aIndex && (int)aIndex <= Length());
		return iBuffer[aIndex];
	}
#endif // UTIL_OPERATOR_SUPPORT
	/**
	  * Returns whether strings are identical
	  *
	  * @param aCStr The C-string to be compared
	  */
	op_force_inline BOOL operator==(const uni_char* aCStr) const { return (Compare(aCStr)==0); }

	/**
	  * Returns whether strings are identical
	  *
	  * @param aCStr The C-string to be compared
	  */
	op_force_inline BOOL operator==(const char* aCStr) const { return (Compare(aCStr)==0); }

	/**
	  * Returns whether strings are identical
	  *
	  * @param aString The string to be compared
	  */
	op_force_inline BOOL operator==(const OpStringC16& aString) const { return (Compare(aString)==0); }

	/**
	  * Returns whether strings are identical
	  *
	  * @param aString The string to be compared
	  */
	op_force_inline BOOL operator==(const OpStringC8& aString) const { return (Compare(aString)==0); }

	/**
	  * Returns whether strings are not identical
	  *
	  * @param aCStr The C-string to be compared
	  */
    op_force_inline BOOL operator!=(const uni_char* aCStr) const { return !operator==(aCStr); }

	/**
	  * Returns whether strings are not identical
	  *
	  * @param aCStr The C-string to be compared
	  */
    op_force_inline BOOL operator!=(const char* aCStr) const { return !operator==(aCStr); }

	/**
	  * Returns whether strings are not identical
	  *
	  * @param aString The string to be compared
	  */
    op_force_inline BOOL operator!=(const OpStringC16& aString) const { return !operator==(aString); }

	/**
	  * Returns whether strings are not identical
	  *
	  * @param aString The string to be compared
	  */
    op_force_inline BOOL operator!=(const OpStringC8& aString) const { return !operator==(aString); }

	/**
	  * Returns TRUE if this is less than aString
	  *
	  * @param aString The string to be compared
	  */
	op_force_inline BOOL operator<(const OpStringC16& aString) const { return (CompareI(aString) < 0); }

	/**
	  * Returns a pointer to the text data
	  *
	  * @return const uni_char* A pointer to the (constant) data
	  */
	const uni_char* DataPtr() const { return iBuffer; }

	/**
	  * Returns a c-string equivalent
	  *
	  * @return const uni_char* A pointer to a c-string representation of the string
	  */
	const uni_char* CStr() const { return DataPtr(); }

	/**
	  * Returns the number of characters in the string
	  *
	  * @return int The length
	  */
	int Length() const { return iBuffer == NULL ? 0 : uni_strlen(iBuffer); }

	/**
	  * Allocates and returns a UTF-8 equivalent.
	  *
	  * @param result A location in which to store the result.  Unless an
	  *               error occurs the result is a freshly allocated UTF-8
	  *               string representing this string.  This string MUST be
	  *               de-allocated with delete[].  The return value is NULL
	  *               if this is a NULL string.
	  * @return A freshly allocated UTF-8 string representing this string.
	  *         This string MUST be de-allocated with delete[].
	  *         The return value is NULL if this is a NULL string.
	  */
	OP_STATUS UTF8Alloc( char **result ) const;
	char *UTF8AllocL() const;

	/**
	  * Copies a UTF-8 equivalent to a pre-allocated buffer.
	  *
	  * @param buf  Buffer to hold result value.
	  * @param size Size of buffer, if -1 no copying is done, only the needed
	  *             size is calculated.
	  * @return Number of bytes (that would be) copied (includes trailing null).
	  */
	int UTF8(char *buf, int size = -1) const;

	/**
	  * Compares the string with another specified string
	  *
	  * @param aString The string to be compared
	  * @return int (<0 mean this is less than aString, 0 means they're equal,
	  *				 >0 means this is greater than aString)
	  */
	int Compare(const OpStringC16& aString) const
	{
		return Compare(aString.CStr(),KAll);
	}

	/**
	  * Compares the string with an 8-bit string
	  *
	  * @param aString The string to be compared
	  * @return int (<0 mean this is less than aString, 0 means they're equal,
	  *				 >0 means this is greater than aString)
	  */
	int Compare(const OpStringC8& aString) const
	{
		return Compare(aString.CStr(),KAll);
	}

	/**
	  * Compares the string with a 16-bit c-string
	  *
	  * @param aCStr The c-string to be appended
	  * @param aLength A number of characters from the c-string to compare
					(default is all subsequent characters)
	  * @return int (<0 mean this is less than aString, 0 means they're equal,
	  *				 >0 means this is greater than aString)
	  */
	int Compare(const uni_char* aCStr, int aLength=KAll) const;

	/**
	  * Compares the string with an 8-bit c-string
	  *
	  * @param aCStr The c-string to be appended
	  * @param aLength A number of characters from the c-string to compare
					(default is all subsequent characters)
	  * @return int (<0 mean this is less than the c-string, 0 means they're equal,
	  *				 >0 means this is greater than the c-string)
	  */
	int Compare(const char* aCStr, int aLength=KAll) const;

	/**
	  * Compares the string with another specified string (case insensitive)
	  *
	  * @param aString The string to be compared
	  * @return int (<0 mean this is less than aString, 0 means they're equal,
	  *				 >0 means this is greater than aString)
	  */
	int CompareI(const OpStringC16& aString) const
	{
		return CompareI(aString.CStr(),KAll);
	}

	/**
	  * Compares the string with an 8-bit string (case insensitive)
	  *
	  * @param aString The string to be compared
	  * @return int (<0 mean this is less than aString, 0 means they're equal,
	  *				 >0 means this is greater than aString)
	  */
	int CompareI(const OpStringC8& aString) const
	{
		return CompareI(aString.CStr(),KAll);
	}

	/**
	  * Compares the string with a 16-bit c-string (case insensitive)
	  *
	  * @param aCStr The c-string to be appended
	  * @param aLength A number of characters from the c-string to compare
					(default is all subsequent characters)
	  * @return int (<0 mean this is less than the c-string, 0 means they're equal,
	  *				 >0 means this is greater than the c-string)
	  */
	int CompareI(const uni_char* aCStr, int aLength=KAll) const;

	/**
	  * Compares the string with an 8-bit c-string (case insensitive)
	  *
	  * @param aCStr The c-string to be appended
	  * @param aLength A number of characters from the c-string to compare
					(default is all subsequent characters)
	  * @return int (<0 mean this is less than the c-string, 0 means they're equal,
	  *				 >0 means this is greater than the c-string)
	  */
	int CompareI(const char* aCStr, int aLength=KAll) const;

	/**
	  * Finds a string within this string
	  *
	  * @param aString The string to be searched for
	  * @return int the character position of the found string (or KNotFound)
	  */
	op_force_inline int Find(const OpStringC16& aString) const
	{
		return Find(aString.CStr());
	}

	/**
	  * Finds an 8-bit string within this string
	  *
	  * @param aString The string to be searched for
	  * @return int the character position of the found string (or KNotFound)
	  */
	op_force_inline int Find(const OpStringC8& aString) const
	{
		return Find(aString.CStr());
	}

	/**
	  * Finds a 16-bit c-string within this string
	  *
	  * @param aCStr The string to be searched for
	  * @param aStartIndex index from where to start looking for the needle, defaults to 0
	  * @return int the character position of the found string (or KNotFound)
	  */
	int Find(const uni_char* aCStr, int aStartIndex = 0) const;

	/**
	  * Finds an 8-bit c-string within this string
	  *
	  * @param aCStr The string to be searched for
	  * @param aStartIndex index from where to start looking for the needle, defaults to 0
	  * @return int the character position of the found string (or KNotFound)
	  */
	int Find(const char* aCStr, int aStartIndex = 0) const;

	/**
	  * Finds a string within this string (case insensitive)
	  *
	  * @param aString The string to be searched for
	  * @return int the character position of the found string (or KNotFound)
	  */
	op_force_inline int FindI(const OpStringC16& aString) const
	{
		return FindI(aString.CStr());
	}

	/**
	  * Finds an 8-bit string within this string (case insensitive)
	  *
	  * @param aString The string to be searched for
	  * @return int the character position of the found string (or KNotFound)
	  */
	int FindI(const OpStringC8& aString) const;

	/**
	  * Finds a 16-bit c-string within this string (case insensitive)
	  *
	  * @param aCStr The string to be searched for
	  * @return int the character position of the found string (or KNotFound)
	  */
	int FindI(const uni_char* aCStr) const;

	/**
	  * Finds an 8-bit c-string within this string (case insensitive)
	  *
	  * @param aCStr The string to be searched for
	  * @return int the character position of the found string (or KNotFound)
	  */
	int FindI(const char* aCStr) const;

	/**
	  * Finds an 8-bit c-string within this string (case insensitive)
	  * @deprecated Use the one-parameter version instead.
	  *
	  * @param aCStr The string to be searched for
	  * @param aLength A number of characters in the sub-string of the c-string to be searched for
	  *				(default is all subsequent characters)
	  *        PARAMETER IS DEPRECATED AND DOES NOT WORK
	  * @return int the character position of the found string (or KNotFound)
	  */
	int DEPRECATED(FindI(const char* aCStr, int aLength) const);

	/**
	  * Finds the first occurrence of character within the string
	  *
	  * Provides the functionality of the c-library function
	  * strchr
	  *
	  * @param aChar A character to locate
	  * @return int the position of the found char (or KNotFound)
	  */
	int FindFirstOf(uni_char aChar) const;

	/**
	  * Finds the first occurrence within the string of any of a
	  * string of specified character
	  *
	  * Provides the functionality of the c-library function strpbrk
	  * (but returns index within string, rather than pointer into
	  * string, on success; it is thus distinguishable from SpanNotOf
	  * only on failure).
	  *
	  * @param aCharsString The string of character any of
	  *						which will satisfy the search
	  * @param idx Starting index to search from
	  * @return int the position of the found character (or KNotFound)
	  */
	int FindFirstOf(const OpStringC16& aCharsString, int idx = 0) const;

	/**
	  * Finds the last occurrence of character within the string.
	  *
	  * Provides the functionality of the c-library function
	  * strrchr.
	  *
	  * @param aChar A character to locate
	  * @return int the position of the found char (or KNotFound)
	  */
	int FindLastOf(uni_char aChar) const;

	/**
	  * Finds the length of the initial segment of the string which
	  * consists entirely of characters from the specified string
	  *
	  * Provides the functionality of the c-library function strspn.
	  *
	  * @param aCharsString The string of characters from which
	  *						the substring must be composed
	  * @return int the length of the found string.
	  */
	int SpanOf(const OpStringC16& aCharsString) const;

	/**
	  * Finds the first occurrence within the string of any character
	  * that appears in a specified string.
	  *
	  * Provides the functionality of the c-library function strpbrk
	  * (but returns index within string, rather than pointer into
	  * string, on success; it is thus distinguishable from SpanNotOf
	  * only on failure).
	  *
	  * @param aString The string of characters any of
	  *	               which will satisfy the search.
	  * @return int the position of the found character (or KNotFound)
	  */
	int FindFirstOf(const OpString8& aString) const;

	/**
	  * Checks empty string
	  *
	  * @return BOOL Returns TRUE if string length == 0, FALSE otherwise
	  */
	op_force_inline BOOL IsEmpty() const { return iBuffer == NULL || *iBuffer == 0; }

	/**
	  * Checks for content. Reverse of IsEmpty, and thus not really
	  * needed, but normally one wants to check if string actually
	  * has content and code reading
	  *
	  * if (string.HasContent()) is just so much more readable than
	  * if (!string.IsEmpty())
	  *
	  * @return BOOL Returns TRUE if string length > 0, FALSE otherwise
	  */
	op_force_inline BOOL HasContent() const {return !IsEmpty();}

	/**
	  * Cast operator
	  *
	  * For use where a c-string pointer is expected
	  *
	  * INCLUDED TO SUPPORT EXISTING LEGACY CODE
	  * @deprecated Use .CStr() overtly !
	  *
	  * @return const uni_char* A pointer to a c-string
	  */
	DEPRECATED(operator const uni_char*() const); // inline after class

	/**
	  * Returns a sub-string
	  *
	  * @param aPos The character index at which to start
	  * @return OpStringC8 The sub-string
	  */
	OpStringC16 SubString(unsigned int aPos) const { OP_ASSERT(static_cast<int>(aPos)<=Length()); OpStringC16 substring(iBuffer + aPos); return substring; }

	/**
	  * Deprecated sub-string functionality
	  * Use instead for equivalent functionality: newstring.Set(oldstring.SubString(aPos).CStr(), aLength);
	  */
	DEPRECATED(OpString16 SubString(unsigned int aPos, int aLength, OP_STATUS* aErr = NULL) const);
	DEPRECATED(OpString16 SubStringL(unsigned int aPos, int aLength=KAll) const);

	/**
	 * Append this string to a UniString object.
	 *
	 * Since this is supposed to be a _constant_ string, we use UniString's
	 * AppendConstData() which can use the given data without copying.
	 * We may have to change this into AppendCopyData(), because OpString
	 * is misdesigned and abused, and the string may end up being less
	 * "constant" than anticipated, which will cause problems for UniString.
	 *
	 * @param d The UniString object to which this string should be appended.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	op_force_inline OP_STATUS AppendToUniString(UniString& d) const
	{
		return HasContent()
			? d.AppendConstData(iBuffer, Length())
			: OpStatus::OK;
	}

protected:
	/**
	  * The data is currently kept as a null-terminated uni_char*
	  */
	uni_char* iBuffer;
};

inline OpStringC16::operator const uni_char*() const { return CStr(); }

/*************************************************************************

    class OpString16
*/

/**
 * 16-bit changeable string class.
 *
 * Note that since this class is derived from OpStringC16 which do not
 * have a virtual destructor, data allocated by an instance of this
 * class will not be freed if freed through a pointer to an
 * OpStringC16 object.
 */
class OpString16 : public OpStringC16
{
private:
	/** Suppressed assignment operator
	 *
	 * NEVER USE THIS.  Private to avoid it being used in client code.
	 */
	OpString16& operator=(const OpString16& aString);

public:
	/** Default constructor
	 *
	 * Creates an empty string of length 0.
	 */
	OpString16()
	{
#ifdef MEMTEST
		OOM_REPORT_UNANCHORED_OBJECT();
#endif // MEMTEST
		iSize = 0;
	}

	/** Pillaging constructor.
	 *
	 * IMPORTANT: This method is DANGEROUS, as it STEALS the data from the given
	 * object. Yes, that's right, the object passed to this method will be EMPTY
	 * when this method returns. This is why it's no longer declared const.
	 *
	 * @param aString A string to be "copied" (i.e. pillaged).
	 */
	OpString16(OpString16& aString);

	/**
	  * Destructor
	  */
	op_force_inline ~OpString16() { OP_DELETEA(iBuffer); }

	/** Set to a copy of another string.
	 *
	 * @param aString The string to be copied
	 * @param aLength Length of data to copy from aString; defaults to KAll,
	 *  meaning to copy the whole string.
	 * @return Some variants (which may OOM) return OP_STATUS; the others (which
	 *  may LEAVE) return *this.
	 */
	OP_STATUS Set(const OpStringC16& aString, int aLength=KAll)
		{ return Set(aString.CStr(), MIN(aLength, aString.Length())); }
	/**
	 * @overload
	 */
	OP_STATUS Set(const uni_char* aCStr, int aLength=KAll);
	/**
	 * @overload
	 */
	OpString16& SetL(const OpStringC16& aString, int aLength=KAll)
		{ return SetL(aString.CStr(), MIN(aLength, aString.Length())); }
	/**
	 * @overload
	 */
	OpString16& SetL(const uni_char* aCStr, int aLength=KAll);

	/** Set from an 8-bit string.
	 *
	 * @param aString The string to be copied
	 * @param aLength Length of data to copy from aString; defaults to KAll,
	 *  meaning to copy the whole string.
	 * @return Some variants (which may OOM) return OP_STATUS; the others (which
	 *  may LEAVE) return *this.
	 */
	OP_STATUS Set(const OpStringC8& aString, int aLength=KAll)
		{ return Set(aString.CStr(), MIN(aLength, aString.Length())); }
	/**
	 * @overload
	 */
	OP_STATUS Set(const char* aCStr, int aLength=KAll);
	/**
	 * @overload
	 */
	OpString16& SetL(const OpStringC8& aString, int aLength=KAll)
		{ return SetL(aString.CStr(), MIN(aLength, aString.Length())); }
	/**
	 * @overload
	 */
	OpString16& SetL(const char* aCStr, int aLength=KAll);

	/** Set from a UTF-8 string.
	 *
	 * If the UTF-8 string ends with an incomplete character, that character
	 * will not be included.  Illegal character sequences will yield replacement
	 * characters.
	 *
	 * @param aUTF8str The UTF-8 string to be copied
	 * @param aLength Number of charcaters to copy
	 * @return Some variants (which may OOM) return OP_STATUS; the others (which
	 *  may LEAVE) return *this.
	 */
	OP_STATUS SetFromUTF8(const char* aUTF8str, int aLength=KAll);
	/**
	 * @overload
	 */
	OpString16& SetFromUTF8L(const char* aUTF8str, int aLength=KAll);
	/**
	 * @overload
	 */
	OP_STATUS SetFromUTF8(const OpStringC8 &aUTF8str, int aLength=KAll);
	/**
	 * @overload
	 */
	OpString16& SetFromUTF8L(const OpStringC8 &aUTF8str, int aLength=KAll);

	/** Insert content from a Unicode C-style string or string object.
	 *
	 * @param aPos The character position at which to insert
	 * @param aString The string to be inserted
	 * @param aLength Number of bytes to copy from aString, or KAll (the
	 *  default) to copy the whole string.
	 * @return See OpStatus: may OOM.
	 */
	OP_STATUS Insert(int aPos,const OpStringC16& aString, int aLength=KAll)
		{ return Insert(aPos, aString.CStr(), MIN(aLength, aString.Length())); }
	/**
	 * @overload
	 */
	OP_STATUS Insert(int aPos,const uni_char* aCStr, int aLength=KAll);

	/** Inserts content from an 8-bit C-string or string object.
	 *
	 * @param aPos The character position at which to insert.
	 * @param aString The string to be inserted.
	 * @param aLength Number of bytes to copy from aString, or KAll (the
	 *  default) to copy the whole string.
	 * @return Some variants (which may OOM) return OP_STATUS; the other (which
	 *  may LEAVE) return *this.
	 */
	OP_STATUS Insert(int aPos,const OpStringC8& aString, int aLength=KAll)
		{ return Insert(aPos, aString.CStr(), MIN(aLength, aString.Length())); }
	/**
	 * @overload
	 */
	OpString16& InsertL(int aPos,const OpStringC8& aString, int aLength=KAll);
	/**
	 * @overload
	 */
	OP_STATUS Insert(int aPos,const char* aCStr, int aLength=KAll);
	/**
	 * @overload
	 */

	/** Append content from a Unicode C-style string or string object.
	 *
	 * @param aString The string to be appended
	 * @param aLength Number of bytes to copy from aString, or KAll (the
	 *  default) to copy the whole string.
	 * @return Some variants (which may OOM) return OP_STATUS; the others (which
	 *  may LEAVE) return *this.
	 */
	OP_STATUS Append(const OpStringC16& aString, int aLength=KAll)
		{ return Append(aString.CStr(), MIN(aLength, aString.Length())); }
	/**
	 * @overload
	 */
	OP_STATUS Append(const uni_char* aCStr, int aLength=KAll);
	/**
	 * @overload
	 */
	OpString16& AppendL(const OpStringC16& aString, int aLength=KAll)
		{ return AppendL(aString.CStr(), MIN(aLength, aString.Length())); }
	/**
	 * @overload
	 */
	OpString16& AppendL(const uni_char* aCStr, int aLength=KAll);

	/** Appends content from an 8-bit C-string or string object.
	 *
	 * @param aString The string to be appended
	 * @param aLength Number of bytes to copy from aString, or KAll (the
	 *  default) to copy the whole string.
	 * @return Some variants (which may OOM) return OP_STATUS; the other (which
	 *  may LEAVE) return *this.
	 */
	OP_STATUS Append(const OpStringC8& aString, int aLength=KAll)
		{ return Append(aString.CStr(), MIN(aLength, aString.Length())); }
	/**
	 * @overload
	 */
	OP_STATUS Append(const char* aCStr, int aLength=KAll);
	/**
	 * @overload
	 */
	OpString16& AppendL(const char* aCStr, int aLength=KAll);

	/** Extends the length of the buffer if necessary
	 *
	 * Specify the size you need the OpString to be for what you're about to do
	 * to it.  This can be used either to obtain a modifiable buffer in which to
	 * do what you will "by hand" or simply, when you plan to make lots of
	 * changes to the string, to ensure that it performs one big allocation up
	 * front instead of lots of smaller ones - this can save a lot of heap
	 * traffic.
	 *
	 * @param aMaxLength Space desired.
	 * @return The resulting .CStr(), on success.  One variant returns @c NULL
	 * on OOM, the other \c LEAVE()s.
	 */
	uni_char* Reserve(int aMaxLength);
	/**
	 * @overload
	 */
	uni_char* ReserveL(int aMaxLength);

	/**
	  * @param aString A string to be overtaken
	  * @return OP_STATUS
	  */

	OP_STATUS TakeOver(OpString16& aString);

	/**
	 * Takes over the pointer aString, without allocation.
	 *
	 * IMPORTANT: OpString assumes that the given string was allocated with
	 * OP_NEWA(), and will deallocate it with OP_DELETEA() when no longer
	 * needed.
	 */
	void TakeOver(uni_char *aString);

	/** Concatenate several strings.
	 *
	 * Up to four strings may be supplied.
	 *
	 * @return See OpStatus; may OOM.
	 */
	OP_STATUS SetConcat(
		const OpStringC16& str1, const OpStringC16& str2,
		const OpStringC16& str3 = OpStringC16(NULL),
		const OpStringC16& str4 = OpStringC16(NULL));

	/**
	  * Wipes the contents of the string (replaces each char with 0)
	  * but doesn't alter any other aspect
	  *
	  * Used for security purposes
	  */
	void Wipe();

	/**
	  * Returns the number of allocated characters (excluding null-terminator if used)
	  *
	  * @return int The number of characters allocated
	  */
	int Capacity() const { return iSize; }

	/**
	  * Returns the number of allocated bytes (excluding null-terminator if used)
	  *
	  * DEPRECATED - FOR BACKWARD COMPATIBILITY WITH AUniCharPtr
	  *
	  * Use Capacity() instead
	  *
	  * @return int A number of bytes
	  */
	int DEPRECATED(Size() const); // inlined below

	/**
	  * Returns the char at a specified index
	  *
	  * @param aIndex An index into the string
	  * @return uni_char& The character at the position specified
	  */
	uni_char& operator[](int aIndex)
	{
		OP_ASSERT(0 <= aIndex && aIndex <= Capacity());
		return iBuffer[aIndex];
	}
#ifdef UTIL_OPERATOR_SUPPORT
	uni_char& operator[](unsigned int aIndex)
	{
		OP_ASSERT(0 <= (int)aIndex && (int)aIndex <= Capacity());
		return iBuffer[aIndex];
	}
#endif // UTIL_OPERATOR_SUPPORT
	uni_char operator[](int aIndex) const
	{
		OP_ASSERT(0 <= aIndex && aIndex <= Capacity());
		return iBuffer[aIndex];
	}
#ifdef UTIL_OPERATOR_SUPPORT
	uni_char operator[](unsigned int aIndex) const
	{
		OP_ASSERT(0 <= (int)aIndex && (int)aIndex <= Capacity());
		return iBuffer[aIndex];
	}
#endif // UTIL_OPERATOR_SUPPORT

	/**
	  * Returns a pointer to the text data
	  *
	  * @return uni_char* A pointer to the data
	  */
	const uni_char* DataPtr() const { return iBuffer; }
	uni_char* DataPtr() { return iBuffer; }

	/**
	  * Returns a c-string equivalent
	  *
	  * @return uni_char* A c-string
	  */
	uni_char* CStr() { return DataPtr(); }
	const uni_char* CStr() const { return DataPtr(); }

	/**
	  * Cast operator
	  *
	  * For use where a c-string pointer is expected
	  *
	  * INCLUDED TO SUPPORT EXISTING LEGACY CODE
	  * @deprecated Use .CStr() overtly !
	  *
	  * @return uni_char* A pointer to a c-string
	  */
	DEPRECATED(operator uni_char*()); // inline after class body
	DEPRECATED(operator const uni_char*() const); // inline after class body

	void Empty();

	/**
	  * Makes string lowercase
	  *
	  */
	void MakeLower();

	/**
	  * Makes string uppercase
	  *
	  */
	void MakeUpper();

	/** Discards part of the string.
	 *
	 * @param aPos The character index at which to start the delete
	 * @param aLength A number of characters to delete (default, KAll, means: to
	 *  end of string).
	 * @return OpString16& The string (this)
	 */
	OpString16& Delete(int aPos, int aLength=KAll);

	/**
	  * Strips leading and/or trailing whitespace from string
	  * @param leading Whether to strip leading whitespace
	  * @param trailing Whether to strip trailing whitespace
	  * @return OpString16& The string (this)
	  */
	OpString16& Strip(BOOL leading = TRUE, BOOL trailing = TRUE);

	/**
	 * Appends a string from a format/vararg list.
	 *
	 * If the format string is char*, it is assumed to use UTF-8 encoding (being
	 * plain ASCII is compatible with this assumption).
	 *
	 * The output is the one defined by C99 sprintf.  Note that, for floating
	 * point numbers, the output will depend on locale.
	 *
	 * @param format A sprintf-format string (either UTF-8 or UTF-16)
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS AppendFormat(const uni_char* format, ...);
	/**
	 * @overload
	 */
	OP_STATUS AppendFormat(const char* format, ...);

	/**
	 * Appends a string from a format/vararg list.
	 *
	 * As for AppendFormat (q.v.) but the parameters to be formatted in the
	 * format string are supplied via a va_list.
	 *
	 * @param format A sprintf-format string (either UTF-8 or UTF-16)
	 * @param args A va_list obtained using va_start; don't forget to va_end it after use.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS AppendVFormat(const uni_char* format, va_list args);
	/**
	 * @overload
	 */
	OP_STATUS AppendVFormat(const char* format, va_list args);

	/**
	 * Replaces all occurrences of needle by subject, from left to right
	 *
	 * @param needle       what to look for, that will be replaced.
	 *                     If NULL or empty, call does nothing
	 * @param subject      the new stuff that will replace subject, can be NULL,
	 *                     and if so will be treated as an empty string
	 * @param occurrences  the function will replace at most
	 *                     number_of_occurrences times the needle string.
	 *                     Use value -1 for all
	 */
	OP_STATUS ReplaceAll(const uni_char* needle, const uni_char* subject, int occurrences = -1);

	/**
	 * Append this string to a UniString object.
	 *
	 * Since the data owned by this string may change after this call, we
	 * must _copy_ the data into the UniString object. Hence, we use
	 * UniString's AppendCopyData() method.
	 *
	 * @param d The UniString object to which this string should be appended.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	op_force_inline OP_STATUS AppendToUniString(UniString& d) const
	{
		return HasContent()
			? d.AppendCopyData(iBuffer, Length())
			: OpStatus::OK;
	}

	/**
	 * Append the released contents of this string to a UniString object.
	 *
	 * Append this string to the given UniString object, transferring
	 * ownership of the string data in the process. This means that this
	 * OpString can no longer refer to the string data, so this object will
	 * be empty upon a successful return.
	 *
	 * The data transferred to the UniString object is assumed to have been
	 * allocated with OP_NEWA(), so we will instruct UniString to use
	 * OP_DELETEA() for deallocating the data when no longer needed.
	 *
	 * @param d The UniString object to which this string should be appended.
	 * @retval OpStatus::OK on success. This string will be empty.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM. This string will be unchanged.
	 */
	op_force_inline OP_STATUS ReleaseToUniString(UniString& d)
	{
		if (HasContent())
		{
			RETURN_IF_ERROR(d.AppendRawData(
				OPDATA_DEALLOC_OP_DELETEA, iBuffer, Length(),
				iSize ? static_cast<size_t>(iSize + 1) : OpDataUnknownLength));
			iBuffer = NULL;
			iSize = 0;
		}
		return OpStatus::OK;
	}

	/**
	 * Copy the given UniString into this OpString object.
	 *
	 * On success this object will refer to a copy of the data in \c d, and
	 * OpStatus::OK will be returned. Otherwise, OpStatus::ERR_NO_MEMORY is
	 * returned, and this string remains unchanged.
	 *
	 * @param d The UniString object to copy from.
	 * @retval OpStatus::OK on success. This string holds a copy of \c d.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM. This string will be unchanged.
	 */
	op_force_inline OP_STATUS SetFromUniString(const UniString& d)
	{
		uni_char *newBuffer = OP_NEWA(uni_char, d.Length() + 1);
		RETURN_OOM_IF_NULL(newBuffer);
		size_t copied = d.CopyInto(newBuffer, d.Length());
		OP_ASSERT(copied == d.Length());
		(void) copied;
		newBuffer[d.Length()] = '\0';

		Empty();
		iBuffer = newBuffer;
		iSize = d.Length();
		return OpStatus::OK;
	}

protected:
	OP_STATUS Grow(int aLength);

	int iSize; // Number of allocated characters (excluding null-terminator if used)

	friend OP_STATUS SetFromEncoding(OpString16 *target, const char *aEncoding, const void *aEncodedStr, int aByteLength, int* aInvalidInputs);
	friend OP_STATUS SetFromEncoding(OpString16 *target, const OpStringC8 &aEncoding, const void *aEncodedStr, int aByteLength, int* aInvalidInputs);
	friend int SetFromEncodingL(OpString16 *target, const char *aEncoding, const void *aEncodedStr, int aByteLength);
	friend int SetFromEncodingL(OpString16 *target, const OpStringC8 &aEncoding, const void *aEncodedStr, int aByteLength);
	friend OP_STATUS SetFromEncoding(OpString16 *target, InputConverter *aDecoder, const void *aEncodedStr, int aByteLength, int* aInvalidInputs);
	friend int SetFromEncodingL(OpString16 *target, InputConverter *aDecoder, const void *aEncodedStr, int aByteLength);
};

inline OpString16::operator uni_char*() { return CStr(); }
inline OpString16::operator const uni_char*() const { return CStr(); }

inline OP_STATUS OpString8::Set(const OpStringC16 &aCStr, int aLength){return Set(aCStr.CStr());}
inline OpString8& OpString8::SetL(const OpStringC16 &aCStr, int aLength){return SetL(aCStr.CStr());};
inline OP_STATUS OpString8::SetUTF8FromUTF16(const OpStringC16 &aUTF16str, int aLength /*= KAll*/)
{
	return SetUTF8FromUTF16(aUTF16str.CStr(), aLength);
}

inline OpString8& OpString8::SetUTF8FromUTF16L(const OpStringC16 &aUTF16str, int aLength /*= KAll*/)
{
	return SetUTF8FromUTF16L(aUTF16str.CStr(), aLength);
}

inline OpString16& OpString16::SetFromUTF8L(const OpStringC8 &aUTF8str, int aLength /*=KAll */)
{
	return SetFromUTF8L(aUTF8str.CStr(), aLength);
}

inline OP_STATUS OpString16::SetFromUTF8(const OpStringC8 &aUTF8str, int aLength /*=KAll */)
{
	return SetFromUTF8(aUTF8str.CStr(), aLength);
}

/* gcc 3 but < 3.4 can't handle deprecated inline; so separate deprecated
 * declaration from inline definition.
 */
inline int OpString16::Size() const { return iSize; }

template <> void op_swap(OpString8& x, OpString8& y);
template <> void op_swap(OpString16& x, OpString16& y);

typedef OpStringC16 OpStringC;
typedef OpString16 OpStringS16;
typedef OpString16 OpString;
typedef OpString8 OpStringS8;
typedef OpString OpStringS;

/**
 * Utility class to call Empty() on a
 * OpString when deleted.
 */
template<typename T>
class AutoEmptyOpStringT
{
	T* m_str;
	BOOL m_do_wipe;
public:
	AutoEmptyOpStringT(T* str, BOOL do_wipe = TRUE) :
		m_str(str), m_do_wipe(do_wipe){}
	~AutoEmptyOpStringT()
	{ if (m_str){ if (m_do_wipe) m_str->Wipe(); m_str->Empty(); } }
};

typedef AutoEmptyOpStringT<OpString> AutoEmptyOpString;
typedef AutoEmptyOpStringT<OpString8> AutoEmptyOpString8;
typedef AutoEmptyOpStringT<OpString16> AutoEmptyOpString16;

#endif // !MODULES_UTIL_OPSTRING_H
