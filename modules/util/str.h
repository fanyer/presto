/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_UTIL_STR_H
#define MODULES_UTIL_STR_H

typedef enum
{
	OPSTR_TRIMKIND_UNUSED_FIRST_ENUM	= 0,
	OPSTR_TRIM_LEFT						= 0x01,		//	0001
	OPSTR_TRIM_RIGHT					= 0x02,		//	0010
	OPSTR_TRIM_RIGHT_AND_LEFT			= 0x03,		//	0011
	OPSTR_TRIM_ALL						= 0x07,		//	0111
	OPSTR_TRIMKIND_UNUSED_LAST_ENUM
} OPSTR_TRIMTYPE;

typedef enum
{
	OPSTR_CASE_SENSITIVE,
	OPSTR_CASE_IGNORE
} OPSTR_CASECOMPARETYPE;

typedef enum
{
	OPSTR_LOCALFILE,
	OPSTR_LOCALFILE_URL,
	OPSTR_MAIL,
	OPSTR_MAIL_URL_BODY, // Used when decoding mailto (subject and body fields)
	OPSTR_OTHER
} OPSTR_FILETYPE;

#ifndef MAXSTRSIZE										// because conflict with error.h <JB>
#define MAXSTRSIZE(sz)		(sizeof(sz)/sizeof(sz[0]))  // counting characters (not bytes)
#endif
#define MAXSTRLEN(sz)		(MAXSTRSIZE(sz)-1)          // counting characters (not bytes)

inline uni_char*		StrEnd( uni_char* sz)		{ return sz ? sz+uni_strlen(sz) : NULL; }
inline char*			StrEnd( char* sz)			{ return sz ? sz+op_strlen(sz) : NULL; }
inline const char*		StrEnd( const char* sz)		{ return sz ? sz+op_strlen(sz) : NULL; }
inline const uni_char*	StrEnd( const uni_char* sz)	{ return sz ? sz+uni_strlen(sz) : NULL; }

inline BOOL	IsStr(const char *str)		{ return str && *str; }
inline BOOL	IsStr(const uni_char *str)	{ return str && *str; }

#ifdef UTIL_POINTER_OVERLAP
/** Return TRUE if two pointers with a given size overlaps. */
BOOL IsPointerOverlapping( const void* ptr1Start, size_t lenPtr1,
			   const void* ptr2Start, size_t lenPtr2);
#endif // UTIL_POINTER_OVERLAP


/**
 * Convert a string to OpFileLength according to a given base.
 *
 * The string may begin with an arbitrary amount of white space (as determined
 * by op_isspace()). First subsequent non digit character stops parsing.
 * Example: " \t13tusen200" becomes 13.
 *
 * Note that the function will never return negative values, as we cannot we sure
 * OpFileLength is unsigned. Thus the function will not accept signed number
 * (neither with a + or -).
 *
 * If there is an overflow, OpStatus::ERR_OUT_OF_RANGE is returned. If no number
 * was found OpStatus::ERR is returned.
 *
 * Maximum value is defined by FILE_LENGTH_MAX given by in system.h in platform.
 *
 *
 * @param nptr			A pointer to the string containing a number and
						terminated by any non-digit (e.g. '\0').
 * @param file_length	(out) Converted number is stored here.
 * @param base			The base of the number. If 0, the base is autodetected
 *                       from the string ("0x" prefix -> base 16,
 *						"0" prefix -> base 8, otherwise base 10 is assumed).
 * @param endptr		(out) A pointer to the last digit in the converted number.
 * 						It is only set when returning OpStatus::OK
 *

 *
 *
 * @return  OpStatus::OK or OpStatus::ERR if no number was found,
 * 		    OpStatus::ERR_OUT_OF_RANGE if number could not be represented as
 * 			an unsigned number in OpFileLength
 */
OP_STATUS StrToOpFileLength(const char* nptr, OpFileLength *file_length,  int base = 10, char** endptr = 0);

/**
 * Convert a string of hexadecimal characters to a Unicode code point value.
 * Scans backwards in hex_string, starting at hex_end - 1 and going no further
 * back than hex_start, to find the longest string of hexadecimal characters
 * describing Unicode code point. Only values in the range of valid Unicode
 * code points (0x0 - 0x10FFFF) are considered.
 *
 * @param hex_start The earliest position in hex_string to look at.
 * @param hex_end The first character after the hexadecimal string.
 * @param[out] hex_pos Set to offset of the first character forming the
 *   output character.
 * @param hex_string A string of characters.
 * @return The Unicode code point, or 0 if nothing was found.
 */
UnicodePoint ConvertHexToUnicode(int hex_start, int hex_end, int& hex_pos, const uni_char* hex_string);

/**
 *	Allocate a duplicate of "str" and strip trailing and leading
 *  whitespace. Returns this new string.
 */
char* stripdup(const char* str);

/**
 * Allocate a duplicate of "str" and strip trailing and leading
 * whitespace. Returns this new string.
 */
uni_char* uni_stripdup(const uni_char* str);

/**
 * Create a concatenated string from one or more source strings.
 * The target string must be big enough to fit all the source strings.
 * Any NULL source strings are be ignored.
 */
char *StrMultiCat(char* target, const char* source1, const char* source2, const char* source3,
				  const char* source4, const char* source5 = NULL, const char* source6 = NULL, const char* source7 = NULL);

/** @overload */
char *StrMultiCat(char* target, const char* source1, const char* source2 = NULL, const char* source3 = NULL);

/** @overload */
uni_char *StrMultiCat(uni_char* target, const uni_char* source1, const uni_char* source2 = NULL, const uni_char* source3 = NULL);
/** @overload */
uni_char *StrMultiCat(uni_char* target, const uni_char* source1, const uni_char* source2, const uni_char* source3,
                      const uni_char* source4, const uni_char* source5 = NULL, const uni_char* source6 = NULL, const uni_char* source7 = NULL);

/** Do a sprintf() that appends to the current string.
  * The target string must be big enough to fit the resulting string.
  * @deprecated Use OpString::AppendFormat() instead.
  */
int DEPRECATED(StrCatSprintf(char* target, const char* format, ...));

/**
 * Do a snprintf() that appends to the target string.
 *
 * @param target the string written to.
 *
 * @param max_len the the maximum number characters written to target including
 * the trailing zero.
 *
 * @param format A printf-style format-string; subsequent parameters provide the
 * data it formats.
 *
 * @return The number of characters written, excluding the trailing zero; or the
 * number of characters that would have been written if enough space had been
 * available.
 */
int StrCatSnprintf(char *target, unsigned int max_len, const char *format, ... );
#ifdef UTIL_STRCATSPRINTFUNI
/**
 * @overload
 */
int StrCatSnprintf(uni_char *target, unsigned int max_len, const uni_char *format, ... );
/**
 * @overload
 * @deprecated
 */
int DEPRECATED(StrCatSprintf(uni_char *target,  const uni_char *format, ...));
#endif // UTIL_STRCATSPRINTFUNI

/** Set str to the contents of val, allocating a new pointer and
  * deallocating any current string. If val is NULL, str is deallocated.
  * Deletes the old string and sets pointer to NULL and returns
  * ERR_NO_MEMORY on OOM. */
OP_STATUS UniSetStr(uni_char*& str, const uni_char* val);

/** Set str to the first len characters of val, allocating a new
  * pointer and deallocating any current string. If val is NULL,
  * str is deallocated. Deletes the old string and sets pointer
  * to NULL and returns ERR_NO_MEMORY on OOM. */
OP_STATUS UniSetStrN(uni_char*& str, const uni_char* val, int len);

/** Set str to the contents of val, allocating a new pointer and
  * deallocating any current string. If val is NULL, str will be an
  * empty string of length 1. Deletes the old string and sets pointer
  * to NULL and returns ERR_NO_MEMORY on OOM. */
OP_STATUS UniSetStr_NotEmpty(uni_char*& target, const uni_char* source, int *source_len = NULL);

/** Create a copy of the source string. Behaves like uni_strdup()
  * but uses new[] instead of op_malloc(). If source is NULL,
  * returns NULL. Returns NULL on OOM. */
uni_char* UniSetNewStr(const uni_char* source);

/** Create a copy of the first len characters of the string source.
  * If val is NULL, return NULL. Returns NULL on OOM. */
uni_char* UniSetNewStrN(const uni_char* val, int len);

/** Create a copy of the first len characters of the string source.
  * If val is NULL, return an empty string. Returns NULL on OOM. */
uni_char* UniSetNewStr_NotEmpty(const uni_char* source);

/** Set str to the contents of val, allocating a new pointer and
  * deallocating any current string. If val is NULL, str is deallocated.
  * Deletes the old string and sets pointer to NULL and returns
  * ERR_NO_MEMORY on OOM. */
OP_STATUS SetStr(char*& str, const char* val);

/** Set str to the contents of val, allocating a new pointer and
  * deallocating any current string. If val is NULL, str is deallocated.
  * Deletes the old string, sets pointer to NULL and LEAVEs on OOM. */
inline void SetStrL(char*& str, const char* val)
{
	LEAVE_IF_ERROR(SetStr(str,val));
}

/** @overload */
OP_STATUS SetStr(uni_char* &str, const char* val);
/** @overload */
inline void SetStrL(uni_char*& str, const char* val)
{
	LEAVE_IF_ERROR(SetStr(str,val));
}

/** @overload */
OP_STATUS SetStr(uni_char* &str, const uni_char* val);
/** @overload */
inline void SetStrL(uni_char*& str, const uni_char* val)
{
	LEAVE_IF_ERROR(SetStr(str,val));
}

/** Set str to the first len characters of val, allocating a new
  * pointer and deallocating any current string. If val is NULL,
  * str is deallocated. Deletes the old string and sets pointer
  * to NULL and returns ERR_NO_MEMORY on OOM. */
OP_STATUS SetStr(char*& str, const char* val, int len);

/** Set str to the first len characters of val, allocating a new
  * pointer and deallocating any current string. If val is NULL,
  * str is deallocated. Deletes the old string and sets pointer
  * to NULL and LEAVEs on OOM. */
inline void SetStrL(char*& str, const char* val, int len)
{
	LEAVE_IF_ERROR(SetStr(str,val,len));
}

/** Set target to the first source_len characters of source,
  * allocating a new pointer and deallocating any current string.
  * If source is NULL, str will be an empty string of length 1.
  * Deletes the old string and sets pointer to NULL and returns
  * ERR_NO_MEMORY on OOM. */
OP_STATUS SetStr_NotEmpty(char*& target, const char* source, int *source_len = NULL);

/** Create a copy of the source string. Behaves like uni_strdup()
  * but uses new[] instead of op_malloc(). If source is NULL,
  * returns NULL. Returns NULL on OOM. */
char *SetNewStr(const char* val);

/** @overload */
uni_char *SetNewStr(const uni_char* val);

/** Allocate a new string, and copy source to it. Source must be
  * null-terminated.
  * If source is a NULL pointer, an empty string allocated and returned.
  * Returns NULL on OOM. */
char *SetNewStr_NotEmpty(const char* source);

#ifdef UTIL_SET_NEW_CAT_STR
/** Create a concatenated string from one or more source strings.
  * Allocates a target string just big enough to fit all the source
  * strings. Any NULL source strings are be ignored. */
char* SetNewCatStr(const char* val = NULL, const char* val1 = NULL, const char* val2 = NULL, const char* val3 = NULL, const char* val4 = NULL, const char* val5 = NULL, const char* val6 = NULL);
/** @overload */
uni_char* SetNewCatStr(const uni_char* val = NULL, const uni_char* val1 = NULL, const uni_char* val2 = NULL, const uni_char* val3 = NULL, const uni_char* val4 = NULL);
#endif // UTIL_SET_NEW_CAT_STR

BOOL MatchExpr(const uni_char* name, const uni_char* expr, BOOL complete_match = FALSE);

const unsigned char* strnstr(const unsigned char* string, int length, const unsigned char* find);

#ifdef UTIL_STRTRIMCHARS
/** Remove all leading strings in pszInp that is found in
  * arrayOfStringsToRemove.
  *	Returns a pointer to the resulting string (pszInp).
  */
uni_char* StrTrimLeftStrings
(
	uni_char*		pszInp,
	const uni_char*	arrayOfStringsToRemove[],
	int				nItemsInArrayOfStringsToRemove,
	BOOL			fCaseSensitive
);
#endif // UTIL_STRTRIMCHARS

/** Replace all occurances of chToFind in pszArg with chReplace. */
uni_char* StrReplaceChars	( uni_char *pszArg, uni_char chToFind, uni_char chReplace);

#ifdef UTIL_STRTRIMCHARS
uni_char *StrTrimChars
(
	uni_char			*pszArg,
	const uni_char		*pszCharSet,
	OPSTR_TRIMTYPE		trimKind
);

inline	uni_char*	StrTrimLeft				(uni_char *pszArg, const uni_char *pszCharSet) { return StrTrimChars( pszArg, pszCharSet, OPSTR_TRIM_LEFT);};
inline	uni_char*	StrTrimRight			(uni_char *pszArg, const uni_char *pszCharSet) { return StrTrimChars( pszArg, pszCharSet, OPSTR_TRIM_RIGHT);};
inline	uni_char*	StrTrimLeftAndRight		(uni_char *pszArg, const uni_char *pszCharSet) { return StrTrimChars( pszArg, pszCharSet, OPSTR_TRIM_RIGHT_AND_LEFT);};
#endif // UTIL_STRTRIMCHARS


inline int			StrLen					(const uni_char *psz) {return psz ? uni_strlen(psz) : 0 ;}

#ifdef UTIL_MYUNISTRTOK
uni_char *MyUniStrTok(uni_char *str, const uni_char *dividers, int &pos, BOOL &done);
#endif // UTIL_MYUNISTRTOK

/** Write formatted string with variable order arguments. For use with
  * language strings. Where there once was "Use %s to do %s with %s",
  * you can now do "Use %1 to do %2 with %3". this is particularily
  * useful languages with different syntax.
  *
  * Example: to do this for a string which has a string as argument 1
  * and an integer as argument 2, replace the "uni_snprintf" statement
  * in your code with "uni_snprintf_si" (s=string, i=integer).
  * Then put %1 and %2 in place of %s and %i in the language string.
  *
  * If you need new combinations of arguments, write new functions that
  * call shuffle_arg().
  */
int uni_snprintf_si (uni_char *out, size_t len, const uni_char *format, const uni_char *first,       int       two                          );

/** Write formatted string with variable order arguments. For use with
  * language strings. Where there once was "Use %s to do %s with %s",
  * you can now do "Use %1 to do %2 with %3". this is particularily
  * useful languages with different syntax.
  *
  * Example: to do this for a string which has two strings as arguments
  * replace the "uni_snprintf" statement in your code with
  * "uni_snprintf_ss" (s=string). Then put %1 and %2 in place of the two
  * %s in the language string.
  *
  * If you need new combinations of arguments, write new functions that
  * call shuffle_arg().
  */
int uni_snprintf_s  (uni_char *out, size_t len, const uni_char *format, const uni_char *first);
int uni_snprintf_ss (uni_char *out, size_t len, const uni_char *format, const uni_char *first, const uni_char *second                       );
int uni_snprintf_sss(uni_char *out, size_t len, const uni_char *format, const uni_char *first, const uni_char *second, const uni_char *third);

#if defined UTIL_CHECK_KEYWORD_INDEX || defined UTIL_START_KEYWORD_CASE
// !!! ALL LISTS MUST BE SORTED !!! Unless specified otherwise, in alphabetical order
// The FIRST item in the array contains the DEFAULT value
struct KeywordIndex
{
	const char *keyword;
	int index;
};
#endif // UTIL_CHECK_KEYWORD_INDEX || UTIL_START_KEYWORD_CASE

#ifdef UTIL_CHECK_KEYWORD_INDEX
// !!! ALL LISTS MUST BE SORTED !!!
/**
 * CheckKeywordIndex
 *
 * Return the index of the keyword found in string.
 *
 * @param string the string to search for keyword.
 * @param keys the keywords with associated index. Must be sorted
 * according to the keywords.
 * @param count the number of keywords
 * @return the index of the match or if no match the first index.
 */
int CheckKeywordsIndex(const char *string, const KeywordIndex *keys, int count);
/** @overload */
int CheckKeywordsIndex(const char *string, int len, const KeywordIndex *keys, int count);

/**
 * Return the index of the keyword found in string.
 * The keyword list does not need to be sorted, with the exception that
 * keywords that are prefixes of other strings must come after every
 * string they are prefixes of.
 * @see CheckKeywordsIndex
 */
int CheckStartsWithKeywordIndex(const char *string, const KeywordIndex *keys, int count);
#endif // UTIL_CHECK_KEYWORD_INDEX

#ifdef UTIL_START_KEYWORD_CASE
/**
 * Case sensitive version of CheckStartsWithKeywordIndex().
 * @return the number of matching bytes in offset */
int CheckStartsWithKeywordIndexCase(const char *string, const KeywordIndex *keys, int count, int &offset);
#endif // UTIL_START_KEYWORD_CASE

#ifdef UTIL_FIND_FILE_EXT
// Will return length in 'len', if it is non-NULL
const uni_char *FindFileExtension(const uni_char *name, UINT* len = NULL);
#endif // UTIL_FIND_FILE_EXT

#ifdef UTIL_METRIC_STRING_TO_INT
int MetricStringToInt(const char *str);
#endif // UTIL_METRIC_STRING_TO_INT

#ifdef UTIL_EXTRACT_FILE_NAME
/**
 * Extract the file name and glob pattern from a URL.
 *
 * @param orgname Input: The URL to extract the file name from.
 * @param extracted Output: Buffer to write the extracted file name to.
 * @param extracted_buffer_size Input: Size of extracted_buffer_size, in uni_chars.
 * @param formatted_ext
 *   Output: Buffer to write a file name glob pattern into. This will be
 *   stored twice, with a delimiting nul character, provided that the buffer
 *   is big enough.
 * @param  formatted_buffer_size Input: Size of formatted_ext, in uni_chars.
 */
void ExtractFileName(const uni_char *orgname, uni_char *extracted, int extracted_buffer_size, uni_char *formatted_ext, int formatted_buffer_size);
#endif // UTIL_EXTRACT_FILE_NAME

#ifdef UTIL_REMOVE_CHARS
OP_STATUS RemoveChars(OpString& str, const OpStringC& charstoremove);
#endif // UTIL_REMOVE_CHARS

/**
 * Check if character string consists entirely of uni_collapsing_sp characters.
 *
 * @param txt Nul-terminated character string to check.
 * @return TRUE if only uni_collapsing_sp character were found in txt.
 */
BOOL IsWhiteSpaceOnly(const uni_char* txt);

#endif // !MODULES_UTIL_STR_H
