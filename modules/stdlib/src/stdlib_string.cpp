/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file stdlib_string.cpp String functions.

    Functions common to the 'char' and 'uni_char' types are defined
	in stdlib_string_common.inl; that file is included several times,
	with proper #defines for function names and types, at the end 
    of this file.  (If you don't understand how this works then you
    need to read the code once more.)  */

#include "core/pch.h"

#ifdef HAVE_LTH_MALLOC
#  include "modules/memtools/happy-malloc.h"
#endif

/* These are abstractions that are used mostly by the parameterized code in 
   stdlib_string_common.inl */

static inline size_t strlib_strlen(const char* s) { return op_strlen(s); }
static inline size_t strlib_strlen(const uni_char* s) { return uni_strlen(s); }
static inline char*  strlib_strcpy(char* dest, const char* src) { return op_strcpy(dest, src); }
static inline uni_char* strlib_strcpy(uni_char* dest, const uni_char* src) { return uni_strcpy(dest, src); }

/* For these functions we have overloaded versions, and we can only map one
   of them to the system libraries */
#ifdef HAVE_UNI_STRSTR
uni_char* uni_strstr(const uni_char* haystack, const uni_char* needle)
{
	return const_cast<uni_char *>(real_uni_strstr(haystack, needle));
}
#endif

#ifdef HAVE_UNI_STRISTR
const uni_char* uni_stristr(const uni_char* haystack, const uni_char* needle)
{
	return real_uni_stristr(haystack, needle);
}
#endif

#ifdef HAVE_UNI_STRCMP
int uni_strcmp(const uni_char* str1, const uni_char* str2)
{
	return real_uni_strcmp(str1, str2);
}
#endif

#ifdef HAVE_UNI_STRICMP
int uni_stricmp(const uni_char* str1, const uni_char* str2)
{
	return real_uni_stricmp(str1, str2);
}
#endif

#ifdef HAVE_UNI_STRNCMP
int uni_strncmp(const uni_char* str1, const uni_char* str2, size_t n)
{
	return real_uni_strncmp(str1, str2, n);
}
#endif

#ifdef HAVE_UNI_STRNICMP
int uni_strnicmp(const uni_char* str1, const uni_char* str2, size_t n)
{
	return real_uni_strnicmp(str1, str2, n);
}
#endif


/* These functions do not exist as parametrized versions */
#ifndef HAVE_UNI_STRTOK
uni_char *uni_strtok(uni_char * s, const uni_char * delim)
{
	uni_char **strtok_global_pp = &g_opera->stdlib_module.m_uni_strtok_datum;

	if (!s)
		s = *strtok_global_pp;	// use previous pointer

	if (!*s)
	{
		*strtok_global_pp = s;  // in case the string was empty initially
		return NULL;			// the end has come
	}

	// may be in an area composed of delims, if so, skip those
	while (*s)
	{
		const uni_char *delims = delim;

		while (*delims && *s != *delims)
			delims++;
		if (!*delims)
			break;				// stop at first non-delim
		s++;
	}

	// now we should be at the start of a token
	uni_char *token = s;

	while (*s)
	{
		const uni_char *delims = delim;

		while (*delims && *s != *delims)
			delims++;
		if (*delims)
			break;				// stop at first delim
		s++;
	}

	// now we've found the end of the token
	if (*s)
	{
		(*s) = 0;
		*strtok_global_pp = s + 1;
	}
	else
		*strtok_global_pp = s;

	if (token == s)
		return NULL;			// empty token
	else
		return token;

}
#endif // HAVE_UNI_STRTOK

// Deprecate???
// Why not use op_strnicmp?  (With suitable optimizations?)
//
// Warning: 7bit! (suitable for html-tags and attributes only, more or less)
BOOL strni_eq(const char * str1, const char *str2, size_t len)
{
	while (len-- && *str1)
	{
		char c = *str1;

		if (c >= 'a' && c <= 'z')
		{
			if (*str2 != (c & 0xDF))
				return FALSE;
		}
		else if (*str2 != c)
			return FALSE;

		str1++, str2++;
	}

	return !*str2 || len == size_t(-1);
}


BOOL uni_stri_eq_upper(const uni_char* str1, const char* str2)
{
	const unsigned char *unsigned_str2 = (const unsigned char *)str2;
	while ( *str1 )
	{
		UnicodePoint c1 = *str1;
		OP_ASSERT( Unicode::ToUpper(*unsigned_str2) == *unsigned_str2 );
		if( c1 != *unsigned_str2 && Unicode::ToUpper(c1) != *unsigned_str2 )
			return FALSE;
		++str1;
		++unsigned_str2;
	}
	return (*unsigned_str2 == 0);
}

BOOL uni_stri_eq_lower(const uni_char* str1, const char* str2)
{
	const unsigned char *unsigned_str2 = (const unsigned char *)str2;
	while ( *str1 )
	{
		UnicodePoint c1 = *str1;
		OP_ASSERT( Unicode::ToLower(*unsigned_str2) == *unsigned_str2 );
		if( c1 != *unsigned_str2 && Unicode::ToLower(c1) != *unsigned_str2 )
			return FALSE;
		++str1;
		++unsigned_str2;
	}
	return (*unsigned_str2 == 0);
}

BOOL uni_strni_eq_lower(const uni_char* str1, const char* str2, size_t len)
{
	const unsigned char *unsigned_str2 = (const unsigned char *)str2;

	while (len-- && *str1)
	{
		UnicodePoint c1 = *str1;
		OP_ASSERT( Unicode::ToLower(*unsigned_str2) == *unsigned_str2 );
		if( c1 != *unsigned_str2 && Unicode::ToLower( c1 ) != *unsigned_str2 )
			return FALSE;
		++str1, ++unsigned_str2;
		if (*str1 == 0 && *unsigned_str2 != 0 && len >= 1)
			return FALSE;
	}
	return !*unsigned_str2 || len == size_t(-1);
}

BOOL uni_strni_eq_lower(const uni_char* str1, const uni_char* str2, size_t len)
{
	while (len-- && *str1)
	{
		UnicodePoint c1 = *str1;
		UnicodePoint c2 = *str2;
		OP_ASSERT( Unicode::ToLower(c2) == c2 );
		if( c1 != c2 && Unicode::ToLower( c1 ) != c2 )
			return FALSE;
		++str1, ++str2;
		if (*str1 == 0 && *str2 != 0 && len >= 1)
			return FALSE;
	}
	return !*str2 || len == size_t(-1);
}


BOOL uni_strni_eq_upper(const uni_char * str1, const char *str2, size_t len)
{
	while (len-- && *str1)
	{
		UnicodePoint c1 = *str1;
		OP_ASSERT( Unicode::ToUpper(*(unsigned char*)str2) == *(unsigned char*)str2 );
		if( c1 != *(unsigned char *)str2 && Unicode::ToUpper( c1 ) != *(unsigned char*)str2 )
			return FALSE;
		++str1, ++str2;
		if (*str1 == 0 && *(unsigned char*)str2 != 0 && len >= 1)
			return FALSE;
	}
	return !*str2 || len == size_t(-1);
}

BOOL uni_strni_eq_upper(const uni_char * str1, const uni_char *str2, size_t len)
{
	while (len-- && *str1)
	{
		UnicodePoint c1 = *str1;
		UnicodePoint c2 = *str2;
		OP_ASSERT( Unicode::ToUpper(c2) == c2 );
		if( c2 != c1 && Unicode::ToUpper( c1 ) != c2 )
			return FALSE;
		++str1, ++str2;
		if (*str1 == 0 && *str2 != 0 && len >= 1)
			return FALSE;
	}
	return !*str2 || len == size_t(-1);
}

BOOL uni_strni_eq_lower_ascii(const uni_char* str1, const uni_char* str2, size_t len)
{
	while (len-- && *str1)
	{
		UnicodePoint c1 = *str1;
		UnicodePoint c2 = *str2;
		OP_ASSERT( c2 < 127 );
		OP_ASSERT( Unicode::ToLower(c2) == c2 );
		if( c1 != c2 && ( c1 < 'A' || c1 > 'Z' || c1 + 32 != c2 ))
			return FALSE;
		OP_ASSERT( Unicode::ToLower(c1) == c2 );
		++str1, ++str2;
		if (*str1 == 0 && *str2 != 0 && len >= 1)
			return FALSE;
	}
	return !*str2 || len == size_t(-1);
}

/* Functions that operate on 'char' */

extern "C" {

#define CHAR_TYPE char
#define CHAR2_TYPE char					// Used for second parameter of some functions
#define UCHAR_TYPE unsigned char
#define UCHAR2_TYPE unsigned char		// Used for second parameter of some functions
#define CHARPARAM_TYPE int				// Used when a character is passed through 'int' in the stdlib
#undef CHAR_UNICODE
#undef CHAR2_UNICODE
	
#define STRLEN_NAME op_strlen
#define STRCPY_NAME op_strcpy
#define STRNCPY_NAME op_strncpy
#define STRLCPY_NAME op_strlcpy
#define STRCAT_NAME op_strcat
#define STRNCAT_NAME op_strncat
#define STRLCAT_NAME op_strlcat
#define STRCMP_NAME op_strcmp
#define STRNCMP_NAME op_strncmp
#define STRICMP_NAME op_stricmp
#define STRNICMP_NAME op_strnicmp
#define STRDUP_NAME op_lowlevel_strdup
#define STRCHR_NAME op_strchr
#define STRRCHR_NAME op_strrchr
#define STRSTR_NAME op_strstr
#define STRISTR_NAME op_stristr
#define STRSPN_NAME op_strspn
#define STRCSPN_NAME op_strcspn
#define STRPBRK_NAME op_strpbrk
#define STRREV_NAME op_strrev
#define STRUPR_NAME op_strupr
#define STRLWR_NAME op_strlwr

// Remove support for APIs not imported
#ifndef STDLIB_STRREV
# undef HAVE_STRREV
# define HAVE_STRREV
#endif
	
#define HAVE_CONVERTING_STRDUP	// not part of the op_ set
#define HAVE_STR_EQ				// not part of the op_ set

#include "modules/stdlib/src/stdlib_string_common.inl"

} // extern "C"


/* Functions that operate on 'uni_char' */

#undef CHAR_TYPE
#undef CHAR2_TYPE
#define CHAR_TYPE uni_char
#define CHAR2_TYPE uni_char

#undef UCHAR_TYPE
#undef UCHAR2_TYPE
#define UCHAR_TYPE uni_char
#define UCHAR2_TYPE uni_char

#undef CHARPARAM_TYPE 
#define CHARPARAM_TYPE uni_char

#undef CHAR_UNICODE
#define CHAR_UNICODE
#undef CHAR2_UNICODE
#define CHAR2_UNICODE

#undef STRLEN_NAME
#undef STRCPY_NAME
#undef STRNCPY_NAME
#undef STRLCPY_NAME
#undef STRCAT_NAME
#undef STRNCAT_NAME
#undef STRLCAT_NAME
#undef STRCMP_NAME
#undef STRNCMP_NAME
#undef STRDUP_NAME
#undef STRCHR_NAME
#undef STRICMP_NAME
#undef STRNICMP_NAME
#undef STRRCHR_NAME
#undef STRSTR_NAME
#undef STRISTR_NAME
#undef STRSPN_NAME
#undef STRCSPN_NAME
#undef STRPBRK_NAME
#undef STRREV_NAME

#define STRLEN_NAME uni_strlen
#define STRCPY_NAME uni_strcpy
#define STRNCPY_NAME uni_strncpy
#define STRLCPY_NAME uni_strlcpy
#define STRCAT_NAME uni_strcat
#define STRNCAT_NAME uni_strncat
#define STRLCAT_NAME uni_strlcat
#define STRCMP_NAME uni_strcmp
#define STRNCMP_NAME uni_strncmp
#define STRICMP_NAME uni_stricmp
#define STRNICMP_NAME uni_strnicmp
#define STRDUP_NAME uni_lowlevel_strdup
#define STRCHR_NAME uni_strchr
#define STRRCHR_NAME uni_strrchr
#define STRSTR_NAME uni_strstr
#define STRISTR_NAME uni_stristr
#define STRSPN_NAME uni_strspn
#define STRCSPN_NAME uni_strcspn
#define STRPBRK_NAME uni_strpbrk
#define STR_EQ_NAME uni_str_eq
#define STRREV_NAME uni_strrev

/* Reset have variables using the UNI versions */
#undef HAVE_STRLEN
#ifdef HAVE_UNI_STRLEN
# define HAVE_STRLEN
#endif

#undef HAVE_STRCPY
#ifdef HAVE_UNI_STRCPY
# define HAVE_STRCPY
#endif

#undef HAVE_STRNCPY
#ifdef HAVE_UNI_STRNCPY
# define HAVE_STRNCPY
#endif

#undef HAVE_STRLCPY
#ifdef HAVE_UNI_STRLCPY
# define HAVE_STRLCPY
#endif

#undef HAVE_STRCAT
#ifdef HAVE_UNI_STRCAT
# define HAVE_STRCAT
#endif

#undef HAVE_STRNCAT
#ifdef HAVE_UNI_STRNCAT
# define HAVE_STRNCAT
#endif

#undef HAVE_STRLCAT
#ifdef HAVE_UNI_STRLCAT
# define HAVE_STRLCAT
#endif

#undef HAVE_STRCMP
#ifdef HAVE_UNI_STRCMP
# define HAVE_STRCMP
#endif

#undef HAVE_STRNCMP
#ifdef HAVE_UNI_STRNCMP
# define HAVE_STRNCMP
#endif

#undef HAVE_STRICMP
#ifdef HAVE_UNI_STRICMP
# define HAVE_STRICMP
#endif

#undef HAVE_STRNICMP
#ifdef HAVE_UNI_STRNICMP
# define HAVE_STRNICMP
#endif

#undef HAVE_STRDUP
#ifdef HAVE_UNI_STRDUP
# define HAVE_STRDUP
#endif

#undef HAVE_STRCHR
#ifdef HAVE_UNI_STRCHR
# define HAVE_STRCHR
#endif

#undef HAVE_STRRCHR
#ifdef HAVE_UNI_STRRCHR
# define HAVE_STRRCHR
#endif

#undef HAVE_STRSTR
#ifdef HAVE_UNI_STRSTR
# define HAVE_STRSTR
#endif

#undef HAVE_STRISTR
#ifdef HAVE_UNI_STRISTR
# define HAVE_STRISTR
#endif

#undef HAVE_STRSPN
#ifdef HAVE_UNI_STRSPN
# define HAVE_STRSPN
#endif

#undef HAVE_STRCSPN
#ifdef HAVE_UNI_STRCSPN
# define HAVE_STRCSPN
#endif

#undef HAVE_STRPBRK
#ifdef HAVE_UNI_STRPBRK
# define HAVE_STRPBRK
#endif

#undef HAVE_STR_EQ

#undef HAVE_STRREV
#ifdef HAVE_UNI_STRREV
# define HAVE_STRREV
#endif

#define HAVE_STRCASE

// Remove support for APIs not imported
#ifndef STDLIB_STRREV
# undef HAVE_STRREV
# define HAVE_STRREV
#endif

#include "modules/stdlib/src/stdlib_string_common.inl"


/* Functions that operate on 'uni_char' on the first parameter and 
   'char' on the second parameter or return type.  Note these all
   use uni_whatever names */

#undef CHAR_TYPE
#undef CHAR2_TYPE
#define CHAR_TYPE uni_char
#define CHAR2_TYPE char

#undef UCHAR_TYPE
#undef UCHAR2_TYPE
#define UCHAR_TYPE uni_char
#define UCHAR2_TYPE unsigned char

#undef CHAR_UNICODE
#define CHAR_UNICODE
#undef CHAR2_UNICODE

#undef HAVE_STRLEN
#undef HAVE_STRCPY
#undef HAVE_STRNCPY
#undef HAVE_STRLCPY
#undef HAVE_STRCAT
#undef HAVE_STRNCAT
#undef HAVE_STRLCAT
#undef HAVE_STRCMP
#undef HAVE_STRNCMP
#undef HAVE_STRICMP
#undef HAVE_STRNICMP
#undef HAVE_STRDUP
#undef HAVE_STRCHR
#undef HAVE_STRRCHR
#undef HAVE_STRSTR
#undef HAVE_STRISTR
#undef HAVE_STRSPN
#undef HAVE_STRCSPN
#undef HAVE_STRPBRK
#undef HAVE_STR_EQ
#undef HAVE_STRREV

#define HAVE_STRLEN
#define HAVE_STRCPY
#define HAVE_STRNCPY
#define HAVE_STRLCPY
#define HAVE_STRCAT
#define HAVE_STRNCAT
#define HAVE_STRLCAT
#define HAVE_STRDUP
#define HAVE_STRCHR
#define HAVE_STRRCHR
#define HAVE_STRSPN
#define HAVE_STRCSPN
#define HAVE_STRPBRK
#define HAVE_STRREV

#undef HAVE_CONVERTING_STRDUP

#define CONVERTING_STRDUP_NAME uni_down_strdup

#include "modules/stdlib/src/stdlib_string_common.inl"


/* Functions that operate on 'char' on the first parameter and 
   'uni_char' on the second parameter or return type.  Note these
   all use uni_whatever names */

#undef CHAR_TYPE
#undef CHAR2_TYPE
#define CHAR_TYPE char
#define CHAR2_TYPE uni_char

#undef UCHAR_TYPE
#undef UCHAR2_TYPE
#define UCHAR_TYPE unsigned char
#define UCHAR2_TYPE uni_char

#undef CHAR_UNICODE
#undef CHAR2_UNICODE
#define CHAR2_UNICODE

#define HAVE_STRLEN
#define HAVE_STRCPY
#define HAVE_STRNCPY
#define HAVE_STRCAT
#define HAVE_STRNCAT
#define HAVE_STRLCAT
#define HAVE_STRCMP
#define HAVE_STRNCMP
#define HAVE_STRICMP
#define HAVE_STRNICMP
#define HAVE_STRDUP
#define HAVE_STRCHR
#define HAVE_STRRCHR
#define HAVE_STRSTR
#define HAVE_STRISTR
#define HAVE_STRSPN
#define HAVE_STRCSPN
#define HAVE_STRPBRK
#define HAVE_STR_EQ

#undef CONVERTING_STRDUP_NAME 
#define CONVERTING_STRDUP_NAME uni_up_strdup

#undef HAVE_STRLCPY
#undef STRLCPY_NAME
#define STRLCPY_NAME uni_cstrlcpy

#include "modules/stdlib/src/stdlib_string_common.inl"

extern "C"
{

#undef op_isdigit
#undef op_isxdigit
#undef op_isspace
#undef op_isupper
#undef op_islower
#undef op_isalnum
#undef op_isalpha
#undef op_iscntrl
#undef op_ispunct

int op_isdigit(int c) { return uni_isdigit(c); }
int op_isxdigit(int c) { return uni_isxdigit(c); }
int op_isspace(int c) { return uni_isspace(c); }
int op_isupper(int c) { return uni_isupper(c); }
int op_islower(int c) { return uni_islower(c); }
int op_isalnum(int c) { return uni_isalnum(c); }
int op_isalpha(int c) { return uni_isalpha(c); }
int op_iscntrl(int c) { return uni_iscntrl(c); }
int op_ispunct(int c) { return uni_ispunct(c); }

int op_isprint(int c)
{
	// FIXME: This is a halfway decent kludge, but we really need something better.
	return c < 256 && !op_iscntrl(c);
}

}
