/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_OPSTRING_H
#define MODULES_UTIL_OPSTRING_H

#ifdef MEMTEST
# include "modules/memtools/mem_util.h"
#endif

class OpString8;
class OpString16;
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
		OP_ASSERT(0<=aIndex && aIndex<(int)(Length()+1));
		return iBuffer[aIndex];
	}

	/**
	  * Returns whether strings are identical
	  *
	  * @param aCStr The C-string to be compared
	  */
    BOOL operator==(const char* aCStr) const { return (Compare(aCStr)==0); }

	/**
	  * Returns whether strings are identical
	  *
	  * @param aString The string to be compared
	  */
    BOOL operator==(const OpStringC8& aString) const { return (Compare(aString)==0); }

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
	  * Note - an index is not included as the pointer itself can be indexed
	  *
	  * @param aCStr The c-string to be appended
	  * @param aLength A number of characters from the c-string to compare
					(default is all subsequent characters)
	  * @return int (<0 mean this is less than aString, 0 means they're equal,
	  *				 >0 means this is greater than aString)
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
	  * Note - an index is not included as the pointer itself can be indexed
	  *
	  * @param aCStr The c-string to be compared
	  * @param aLength A number of characters to compare from the aString
	  *				(default is all subsequent characters)
	  * @return int (<0 mean this is less than aString, 0 means they're equal,
	  *				 >0 means this is greater than aString)
	  */
	int CompareI(const char* aCStr, int aLength=KAll) const;

	/**
	  * Finds a string within this string
	  *
	  * @param aString The string to be searched for
	  * @return int the character position of the found string (or KNotFound)
	  */
	int Find(const OpStringC8& aString) const
	{
		return Find(aString.CStr());
	}

	/**
	  * Finds a c-string within this string
	  *
	  * Note - an index is not included as the pointer itself can be indexed
	  *
	  * @param aCStr The c-string to be searched for
	  * @return int the character position of the found string (or KNotFound)
	  */
	int Find(const char* aCStr) const;

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
	BOOL IsEmpty() const { return iBuffer == NULL || *iBuffer == 0; }


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
	BOOL HasContent() const { return iBuffer && *iBuffer; }

	/**
	  * Cast operator
	  *
	  * For use where a c-string pointer is expected
	  *
	  * INCLUDED TO SUPPORT EXISTING LEGACY CODE
	  *
	  * @return const char* A pointer to a c-string
	  */
	operator const char*() const { return CStr(); }

	/**
	  * Returns a sub-string
	  *
	  * @param aPos The character index at which to start
	  * @param aLength A number of characters to copy
	  * @param aErr A location in which to store a status code; if
	  *             the status code indicates an error then the
	  *             result string is (safe) garbage.
	  * @return OpString8 The sub-string
	  */
	OpString8 SubString(int aPos, int aLength=KAll, OP_STATUS* aErr = NULL) const;

protected:
	/**
	  * The data is currently kept as a null-terminated char*
	  */
	char* iBuffer;
};

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

	/**
	  * Copy constructor
	  *
	  * @param aString A string to be copied
	  */
	OpString8( const OpString8& aString);

	/**
	  * Destructor
	  */
	~OpString8() { Empty(); }

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
	  * @param aLength Number of chars to set.
	  */
	OpString8& ConstructL(const uni_char* aCStr, int aLength=KAll);

	/**
	  * Reallocates the contents of the string with another
	  *
	  * @param aString The string to be copied
	  * @return OpString8& The string (this)
	  */
	OP_STATUS Set(const OpStringC8& aString) { return Set(aString.CStr(), KAll); }
	OpString8& SetL(const OpStringC8& aString);

	/**
	  * Reallocates the contents of the string with a the contents of
	  * a c-string
	  *
	  * @param aCStr The c-string to be copied
	  * @param aLength Number of chars to set.
	  * @return OpString8& The string (this)
	  */
	OP_STATUS Set(const char* aCStr, int aLength=KAll);
	OpString8& SetL(const char* aCStr, int aLength=KAll);

	/**
	  * Reallocates the contents of the string with a the contents of
	  * a c-string
	  *
	  * @param aCStr The c-string to be copied
	  * @param aLength Number of chars to set.
	  * @return OpString8& The string (this)
	  */
	OP_STATUS Set(const uni_char* aCStr, int aLength=KAll);
	OpString8& SetL(const uni_char* aCStr, int aLength=KAll);

	/**
	  * Inserts the contents of a string (whole or sub-string) into the
	  * string at a specified character position
	  *
	  * @param aPos The character position at which to insert
	  * @param aString The string to be inserted
	  * @return OpString8& The string (this)
	  */
	OP_STATUS Insert(int aPos,const OpStringC8& aString) { return Insert(aPos, aString.CStr(), KAll); }
	OpString8& InsertL(int aPos,const OpStringC8& aString) { return InsertL(aPos, aString.CStr(), KAll); }

	/**
	  * Inserts the contents of a char*  (whole or sub-string) into the
	  * string at a specified character position
	  *
	  * Note - an index is not included as the pointer itself can be indexed
	  *
	  * @param aPos The character position at which to insert
	  * @param aCStr The c-string to be inserted
	  * @param aLength A number of characters to copy from the c-string
	  *				(default is all subsequent characters)
	  * @return OpString8& The string (this)
	  */
	OP_STATUS Insert(int aPos,const char* aCStr, int aLength=KAll);
	OpString8& InsertL(int aPos,const char* aCStr, int aLength=KAll);

	/**
	  * Appends the contents of a string (whole or sub-string) to the string
	  *
	  * @param aString The string to be appended
	  * @return OpString8& The string (this)
	  */
	OP_STATUS Append(const OpStringC8& aString) { return Append(aString.CStr(), KAll); }
	OpString8& AppendL(const OpStringC8& aString) { return AppendL(aString.CStr(), KAll); }

	/**
	  * Appends the contents of a c-string (whole or sub-string) to the string
	  *
	  * Note - an index is not included as the pointer itself can be indexed
	  *
	  * @param aCStr The c-string to be appended
	  * @param aLength A number of characters to copy from the c-string
	  *				(default is all subsequent characters)
	  * @return OpString8& The string (this)
	  */
	OP_STATUS Append(const char* aCStr, int aLength=KAll);
	OpString8& AppendL(const char* aCStr, int aLength=KAll);

	// Just for optimization when few strings are concatenated
    OP_STATUS SetConcat(const OpStringC8& str1, const OpStringC8& str2,
                        const OpStringC8& str3 = OpStringC8(NULL), const OpStringC8& str4 = OpStringC8(NULL));

	/**
	  * Sets the string to the concatenation of the supplied strings. At least two
	  * strings must be supplied, maximum is eight strings.
	  * @param str1 First string to be concatenated
	  * @param str2 Second string to be concatenated
	  * @param str3 ...
	  * @param str4 ...
	  * @param str5 ...
	  * @param str6 ...
	  * @param str7 ...
	  * @param str8 Eight and last string to be concatenated
	  * @returns OP_STATUS OpStatus::OK if the operation succeeded, OpStatus::ERR_NO_MEMORY otherwise
	  */
    OP_STATUS SetConcat(const OpStringC8& str1, const OpStringC8& str2,
                        const OpStringC8& str3, const OpStringC8& str4, const OpStringC8& str5,
                        const OpStringC8& str6 = OpStringC8(NULL), const OpStringC8& str7 = OpStringC8(NULL), const OpStringC8& str8 = OpStringC8(NULL));

	/**
	  * Extends the length of the string if necessary
	  *
	  * @param aMaxLength A new maximum length
	  * @return char* A pointer to null-terminated char*
	  */
	char* Reserve(int aMaxLength);
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

	/**
	  * Deletes a specified portion a string
	  *
	  * @param aPos The character index at which to start the delete
	  * @param aLength A number of characters to delete (default is all)
	  * @return OpString8& The string (this)
	  */
	OpString8& Delete(int aPos,int aLength=KAll);

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
	    OP_ASSERT(0<=aIndex && aIndex<(int)(Length()+1));
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
	  *
	  * @return char* A pointer to a c-string
	  */
	operator char*() { return CStr(); }
	operator const char*() const { return CStr(); }

	/**
	 * Takes over the pointer aString, without allocation.
	 */
	void TakeOver(char* aString);

	/**
	 * Appends a string from a format/vararg list.
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

protected:
	OP_STATUS Grow(int aLength);

	int iSize; // Number of allocated characters (excluding null-terminator if used)
};

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
	  * Note - an index is not included as the pointer itself can be indexed
	  *
	  * @param aCStr A 16-bit c-string
	  */
	OpStringC16(const uni_char* aCStr) { iBuffer = (uni_char*)aCStr; }

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
	uni_char operator[](int aIndex) const;

	/**
	  * Returns whether strings are identical
	  *
	  * @param aCStr The C-string to be compared
	  */
    BOOL operator==(const uni_char* aCStr) const { return (Compare(aCStr)==0); }

	/**
	  * Returns whether strings are identical
	  *
	  * @param aCStr The C-string to be compared
	  */
    BOOL operator==(const char* aCStr) const { return (Compare(aCStr)==0); }

	/**
	  * Returns whether strings are identical
	  *
	  * @param aString The string to be compared
	  */
    BOOL operator==(const OpStringC16& aString) const { return (Compare(aString)==0); }

	/**
	  * Returns whether strings are identical
	  *
	  * @param aString The string to be compared
	  */
    BOOL operator==(const OpStringC8& aString) const { return (Compare(aString)==0); }

	/**
	  * Returns TRUE if this is less than aString
	  *
	  * @param aString The string to be compared
	  */
    BOOL operator<(const OpStringC16& aString) const { return (CompareI(aString) < 0); }

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
	int Length() const;

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
	  * Note - an index is not included as the pointer itself can be indexed
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
	  * Note - an index is not included as the pointer itself can be indexed
	  *
	  * @param aCStr The c-string to be appended
	  * @param aLength A number of characters from the c-string to compare
					(default is all subsequent characters)
	  * @return int (<0 mean this is less than aString, 0 means they're equal,
	  *				 >0 means this is greater than aString)
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
	  * Note - an index is not included as the pointer itself can be indexed
	  *
	  * @param aCStr The c-string to be appended
	  * @param aLength A number of characters from the c-string to compare
					(default is all subsequent characters)
	  * @return int (<0 mean this is less than aString, 0 means they're equal,
	  *				 >0 means this is greater than aString)
	  */
	int CompareI(const uni_char* aCStr, int aLength=KAll) const;

	/**
	  * Compares the string with an 8-bit c-string (case insensitive)
	  *
	  * Note - an index is not included as the pointer itself can be indexed
	  *
	  * @param aCStr The c-string to be appended
	  * @param aLength A number of characters from the c-string to compare
					(default is all subsequent characters)
	  * @return int (<0 mean this is less than aString, 0 means they're equal,
	  *				 >0 means this is greater than aString)
	  */
	int CompareI(const char* aCStr, int aLength=KAll) const;

	/**
	  * Finds a string within this string
	  *
	  * @param aString The string to be searched for
	  * @return int the character position of the found string (or KNotFound)
	  */
	int Find(const OpStringC16& aString) const
	{
		return Find(aString.CStr());
	}

	/**
	  * Finds an 8-bit string within this string
	  *
	  * @param aString The string to be searched for
	  * @return int the character position of the found string (or KNotFound)
	  */
	int Find(const OpStringC8& aString) const
	{
		return Find(aString.CStr());
	}

	/**
	  * Finds a 16-bit c-string within this string
	  *
	  * @param aCStr The string to be searched for
	  * @return int the character position of the found string (or KNotFound)
	  */
	int Find(const uni_char* aCStr) const;

	/**
	  * Finds an 8-bit c-string within this string
	  *
	  * @param aCStr The string to be searched for
	  * @return int the character position of the found string (or KNotFound)
	  */
	int Find(const char* aCStr) const;

	/**
	  * Finds a string within this string (case insensitive)
	  *
	  * @param aString The string to be searched for
	  * @return int the character position of the found string (or KNotFound)
	  */
	int FindI(const OpStringC16& aString) const
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
	  * @param aLength A number of characters in the sub-string of aString to be searched for
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
	BOOL IsEmpty() const { return iBuffer == NULL || *iBuffer == 0; }

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
	BOOL HasContent() const {return !IsEmpty();}

	/**
	  * Cast operator
	  *
	  * For use where a c-string pointer is expected
	  *
	  * INCLUDED TO SUPPORT EXISTING LEGACY CODE
	  *
	  * @return const uni_char* A pointer to a c-string
	  */
    operator const uni_char*() const { return CStr(); }

	/**
	  * Returns a sub-string
	  *
	  * @param aPos The character index at which to start
	  * @param aLength A number of characters to copy
	  * @param aErr A location in which to store a status code; if
	  *             the status code indicates an error then the
	  *             result string is (safe) garbage.
	  * @return OpString16 The sub-string
	  */
	OpString16 SubString(int aPos, int aLength=KAll, OP_STATUS* aErr = NULL) const;
	OpString16 SubStringL(int aPos, int aLength=KAll) const;

protected:
	/**
	  * The data is currently kept as a null-terminated uni_char*
	  */
	uni_char* iBuffer;
};

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
	/**
	  * Assignment operator
	  *
	  * NEVER USE THIS.  Private to avoid it being used in client code.
	  */
	OpString16& operator=(const OpString16& aString);

public:
	/**
	  * Default constructor
	  *
	  * Creates an empty string of length 0
	  */
	OpString16()
	{
#ifdef MEMTEST
	    OOM_REPORT_UNANCHORED_OBJECT();
#endif // MEMTEST
		iSize = 0;
	}

	/**
	  * Copy constructor
	  *
	  * @param aString A string to be copied
	*/
	OpString16(const OpString16& aString);

	/**
	  * Destructor
	  */
	~OpString16() { Empty(); }

	/**
	  * Reallocates the contents of the string with another
	  *
	  * @param aString The string to be copied
	  * @return OpString16 The string (this)
	  */
	OP_STATUS Set(const OpStringC16& aString) { return Set(aString.CStr(), KAll); }
	OpString16& SetL(const OpStringC16& aString) { return SetL(aString.CStr(), KAll); }

	/**
	  * Reallocates the contents of the string with the contents
	  * of an 8-bit string
	  *
	  * @param aString The string to be copied
	  * @return OpString16 The string (this)
	  */
	OP_STATUS Set(const OpStringC8& aString) { return Set(aString.CStr(), KAll); }
	OpString16& SetL(const OpStringC8& aString) { return SetL(aString.CStr(), KAll); }

	/**
	  * Reallocates the contents of the string with a the contents of
	  * a c-string or uni_char buffer. Stops at null termination.
	  *
	  * @param aCStr The c-string to be copied
	  * @param aLength Maximum characters to copy
	  * @return OpString16 The string (this)
	  */
	OP_STATUS Set(const uni_char* aCStr, int aLength=KAll);
	OpString16& SetL(const uni_char* aCStr, int aLength=KAll);

	/**
	  * Reallocates the contents of the string with a the contents of
	  * an 8-bit c-string or char buffer. Stops at null termination.
	  *
	  * @param aCStr The c-string to be copied
	  * @param aLength Maximum characters to copy
	  * @return OpString16 The string (this)
	  */
	OP_STATUS Set(const char* aCStr, int aLength=KAll);
	OpString16& SetL(const char* aCStr, int aLength=KAll);

	/**
	  * Inserts the contents of a string (whole or sub-string) into the
	  * string at a specified character position
	  *
	  * @param aPos The character position at which to insert
	  * @param aString The string to be inserted
	  * @return OpString16 The string (this)
	  */
	OP_STATUS Insert(int aPos,const OpStringC16& aString) { return Insert(aPos, aString.CStr(), KAll); }

	/**
	  * Inserts the contents of an 8-bit string (whole or sub-string)
	  * into the string at a specified character position
	  *
	  * @param aPos The character position at which to insert
	  * @param aString The string to be inserted
	  * @return OpString16 The string (this)
	  */
	OP_STATUS Insert(int aPos,const OpStringC8& aString) { return Insert(aPos, aString.CStr(), KAll); }
	OpString16& InsertL(int aPos,const OpStringC8& aString);

	/**
	  * Inserts the contents of a char*  (whole or sub-string) into the
	  * string at a specified character position
	  *
	  * Note - an index is not included as the pointer itself can be indexed
	  *
	  * @param aPos The character position at which to insert
	  * @param aCStr The c-string to be inserted
	  * @param aLength A number of characters to copy from the c-string
	  *				(default is all subsequent characters)
	  * @return OpString16 The string (this)
	  */
	OP_STATUS Insert(int aPos,const uni_char* aCStr, int aLength=KAll);

	/**
	  * Inserts the contents of an 8-bit char*  (whole or sub-string)
	  * into the string at a specified character position
	  *
	  * Note - an index is not included as the pointer itself can be indexed
	  *
	  * @param aPos The character position at which to insert
	  * @param aCStr The c-string to be inserted
	  * @param aLength A number of characters to copy from the c-string
	  *				(default is all subsequent characters)
	  * @return OpString16 The string (this)
	  */
	OP_STATUS Insert(int aPos,const char* aCStr, int aLength=KAll);

	/**
	  * Appends the contents of a string (whole or sub-string)
	  *
	  * @param aString The string to be appended
	  * @return OpString16 The string (this)
	  */
	OP_STATUS Append(const OpStringC16& aString) { return Append(aString.CStr(), KAll); }
	OpString16& AppendL(const OpStringC16& aString) { return AppendL(aString.CStr(), KAll); }

	/**
	  * Appends the contents of an 8-bit string (whole or sub-string)
	  *
	  * @param aString The string to be appended
	  * @return OpString16 The string (this)
	  */
	OP_STATUS Append(const OpStringC8& aString) { return Append(aString.CStr(), KAll); }

	/**
	  * Appends the contents of a c-string (whole or sub-string)
	  *
	  * Note - an index is not included as the pointer itself can be indexed
	  *
	  * @param aCStr The c-string to be appended
	  * @param aLength A number of characters to copy from the c-string
	  *				(default is all subsequent characters)
	  * @return OpString16 The string (this)
	  */
	OP_STATUS Append(const uni_char* aCStr, int aLength=KAll);
	OpString16& AppendL(const uni_char* aCStr, int aLength=KAll);

	/**
	  * Appends the contents of an 8-bit c-string (whole or sub-string)
	  *
	  * Note - an index is not included as the pointer itself can be indexed
	  *
	  * @param aCStr The c-string to be appended
	  * @param aLength A number of characters to copy from the c-string
	  *				(default is all subsequent characters)
	  * @return OpString16 The string (this)
	  */
	OP_STATUS Append(const char* aCStr, int aLength=KAll);
	OpString16& AppendL(const char* aCStr, int aLength=KAll);

	/**
	  * Extends the length of the string if necessary
	  *
	  * @param aMaxLength A new maximum length
	  * @return char* A pointer to null-terminated uni_char*
	  */
	uni_char* Reserve(int aMaxLength);
	uni_char* ReserveL(int aMaxLength);

	/**
	  * @param aString A string to be overtaken
	  * @return OP_STATUS
	  */

	OP_STATUS TakeOver(OpString16& aString);

	// Just for optimization when few strings are concatenated
    OP_STATUS SetConcat(const OpStringC16& str1, const OpStringC16& str2,
                        const OpStringC16& str3 = OpStringC16(NULL), const OpStringC16& str4 = OpStringC16(NULL));

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
		OP_ASSERT(0<=aIndex && aIndex<(int)(Length()+1));
		return iBuffer[aIndex];
	}

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
	  *
	  * @return uni_char* A pointer to a c-string
	  */
	operator uni_char*() { return CStr(); }
	operator const uni_char*() const { return CStr(); }

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

	/**
	  * Deletes a specified portion a string
	  *
	  * @param aPos The character index at which to start the delete
	  * @param aLength A number of characters to delete (default is all)
	  * @return OpString16& The string (this)
	  */
	OpString16& Delete(int aPos,int aLength=KAll);

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
	 * @param format A c-style uni_char* format % string
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS AppendFormat(const uni_char* format, ...);
	OP_STATUS AppendVFormat(const uni_char* format, va_list args);

protected:
	OP_STATUS Grow(int aLength);

	int iSize; // Number of allocated characters (excluding null-terminator if used)
};

/* gcc 3 but < 3.4 can't handle deprecated inline; so separate deprecated
 * declaration from inline definition.
 */
inline int OpString16::Size() const { return iSize; }

typedef OpStringC16 OpStringC;
typedef OpString16 OpStringS16;
typedef OpString16 OpString;
typedef OpString8 OpStringS8;
typedef OpString OpStringS;

#endif // !MODULES_UTIL_OPSTRING_H
