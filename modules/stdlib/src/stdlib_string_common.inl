/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file stdlib_string_common.inl Common string declarations.

   This file is included several times by stdlib_string.cpp, to produce
   various 'char' and 'uni_char' versions.   This file is parameterized
   in several ways:

      - the HAVE_WHATEVER macros determine whether anything is defined,
        for a key 'whatever'
      - WHATEVER_NAME is the name of whatever is defined
      - CHAR_TYPE is the base type of the first string argument, and
        often of the second string argument
      - UCHAR_TYPE is the unsigned variant of CHAR_TYPE
      - CHAR_UNICODE is defined if the first string argument type is
        UTF-16, and undefined if it is ASCII
      - CHAR2_TYPE is the base type of the second string argument, for
        some functions
      - UCHAR2_TYPE is the unsigned variant of CHAR_TYPE
      - CHAR2_UNICODE is defined if the second string argument type is
        UTF-16, and undefined if it is ASCII
      - CHARPARAM_TYPE is the name of the type used to receive character
        parameters; for the op_ functions this is 'int' in keeping with
        the C standard 

   IMPLEMENTATION NOTE: I'm using op_tolower() for case conversion here rather
   than Unicode::ToLower, since the platform may provide its own op_tolower
   functionality.

   */

#ifndef HAVE_STRLEN

size_t STRLEN_NAME(const CHAR_TYPE* s)
{
	OP_ASSERT( s != NULL );

	const CHAR_TYPE *t = s;
	while (*t++ != 0)
		;
	return t - s - 1;
}

#endif // HAVE_STRLEN

#ifndef HAVE_STRCPY

CHAR_TYPE * STRCPY_NAME(CHAR_TYPE *dest, const CHAR_TYPE *src)
{
	// Note, the C Standard requires dest to be non-NULL.  It is not known why we
	// allow dest to be NULL here, or whether Opera needs it.

	OP_ASSERT(dest != NULL && src != NULL);
	if (dest == NULL)
		return NULL;

	CHAR_TYPE *dest0 = dest;

	while ((*dest++ = *src++) != 0)
		;
	return dest0;
}

#endif // HAVE_STRCPY

#ifndef HAVE_STRNCPY

/* PERFORMANCE NOTE: Spec violation: strncpy() should zero-fill the rest of 
   the string */

CHAR_TYPE * STRNCPY_NAME(CHAR_TYPE *dest, const CHAR_TYPE *src, size_t n)
{
	OP_ASSERT( dest != NULL && src != NULL );

	CHAR_TYPE *dest0 = dest;

	while (n != 0 && *src != 0)
	{
		*dest++ = *src++;
		--n;
	}

	if (n != 0)
		*dest = 0;

	return dest0;
}

#endif // HAVE_STRNCPY

#ifndef HAVE_STRLCPY

size_t STRLCPY_NAME(CHAR_TYPE *dest, const CHAR2_TYPE *src, size_t dest_size)
{
	size_t srclen = 0;
	if (dest_size > 0)
	{
		dest_size --; // Leave space for nul
		while (*src && dest_size)
		{
			OP_ASSERT(CHAR_TYPE(*src) == *src || !"Unicode-to-ASCII conversion is lossy");
			*(dest ++) = CHAR_TYPE(*(src ++));
			dest_size --;
			srclen ++;
		}
		// Always nul-terminate
		*dest = 0;
	}
	while (*(src ++))
	{
		srclen ++;
	}
	return srclen;
}

#endif // HAVE_STRLCPY

#ifndef HAVE_STRCAT

CHAR_TYPE * STRCAT_NAME(CHAR_TYPE *dest, const CHAR_TYPE *src)
{
	OP_ASSERT( dest != NULL && src != NULL );

	CHAR_TYPE *dest0 = dest;

	while (*dest != 0)
		dest++;

	while ((*dest++ = *src++) != 0)
		;

	return dest0;
}

#endif // HAVE_STRCAT

#ifndef HAVE_STRNCAT

CHAR_TYPE * STRNCAT_NAME(CHAR_TYPE *dest, const CHAR_TYPE *src, size_t n)
{
	OP_ASSERT( dest != NULL && src != NULL );

	CHAR_TYPE *dest0 = dest;

	while (*dest != 0)
		dest++;

	while (n > 0 && *src != 0)
	{
		*dest++ = *src++;
		--n;
	}

	/* We are required to add a trailing nul, even when n is zero, which
	   means that the n passed as parameter has to be one less than the
	   remaining space in dest (C99 7.21.3.2.2)  */

	*dest = 0;

	return dest0;
}

#endif // HAVE_STRNCAT

#ifndef HAVE_STRLCAT

size_t STRLCAT_NAME(CHAR_TYPE * dest, const CHAR_TYPE * src, size_t dest_size)
{
	//OP_ASSERT(dest != NULL && src != NULL);

	CHAR_TYPE *d = dest;

	if (dest_size > 0)
	{
		while (*dest)
			dest++;

		if (dest_size > size_t(dest - d + 1))
		{
			dest_size -= dest - d + 1;
			while (*src && dest_size-- > 0)
				(*dest++) = (*src++);
			*dest = 0;
		}
	}

	const CHAR_TYPE *s = src;
	while (*src)
		src++;

	return dest - d + src - s;
}

#endif // HAVE_STRLCAT

#ifndef HAVE_STRCMP

int STRCMP_NAME(const CHAR_TYPE *s1_0, const CHAR2_TYPE *s2_0)
{
	OP_ASSERT( s1_0 != NULL && s2_0 != NULL );

	const UCHAR_TYPE *s1 = (const UCHAR_TYPE *)s1_0;
	const UCHAR2_TYPE *s2 = (const UCHAR2_TYPE *)s2_0;

	while (*s1 && *s1 == *s2)
		++s1, ++s2;
	return (int)*s1 - (int)*s2;
}

#endif // HAVE_STRCMP

#ifndef HAVE_STRNCMP

int STRNCMP_NAME(const CHAR_TYPE *s1_0, const CHAR2_TYPE *s2_0, size_t n)
{
	OP_ASSERT( s1_0 != NULL && s2_0 != NULL );

	const UCHAR_TYPE *s1 = (const UCHAR_TYPE *)s1_0;
	const UCHAR2_TYPE *s2 = (const UCHAR2_TYPE *)s2_0;

	while (n > 0 && *s1 && *s1 == *s2)
		++s1, ++s2, --n;
	return n > 0 ? (int)*s1 - (int)*s2 : 0;
}

#endif // HAVE_STRNCMP

#ifndef HAVE_STRICMP

int STRICMP_NAME(const CHAR_TYPE *s1_0, const CHAR2_TYPE *s2_0)
{
	OP_ASSERT( s1_0 != NULL && s2_0 != NULL );

	const UCHAR_TYPE *s1 = (const UCHAR_TYPE *)s1_0;
	const UCHAR2_TYPE *s2 = (const UCHAR2_TYPE *)s2_0;

	while (*s1 && *s2 && (*s1 == *s2 || op_tolower(*s1) == op_tolower(*s2)))
		++s1, ++s2;
	return op_tolower(*s1) - op_tolower(*s2);
}

#endif // HAVE_STRICMP

#ifndef HAVE_STRNICMP

int STRNICMP_NAME(const CHAR_TYPE *s1_0, const CHAR2_TYPE *s2_0, size_t n)
{
	OP_ASSERT( s1_0 != NULL && s2_0 != NULL );

	const UCHAR_TYPE *s1 = (const UCHAR_TYPE *)s1_0;
	const UCHAR2_TYPE *s2 = (const UCHAR2_TYPE *)s2_0;

	while (n > 0 && *s1 && *s2 && (*s1 == *s2 || op_tolower(*s1) == op_tolower(*s2)))
		++s1, ++s2, --n;
	return n > 0 ? op_tolower(*s1) - op_tolower(*s2) : 0;
}

#endif // HAVE_STRNICMP

#ifndef HAVE_STRDUP

CHAR_TYPE * STRDUP_NAME(const CHAR_TYPE* s)
{
	OP_ASSERT( s != NULL );

#ifdef HAVE_LTH_MALLOC
	LTH_MALLOC_SITE(site, MALLOC_TAG_MALLOC|MALLOC_TAG_STRDUP);
	CHAR_TYPE *p = reinterpret_cast<CHAR_TYPE *>(internal_malloc((strlib_strlen(s) + 1) * sizeof(CHAR_TYPE), site));
#else
	CHAR_TYPE *p = reinterpret_cast<CHAR_TYPE *>(op_malloc((strlib_strlen(s) + 1) * sizeof(CHAR_TYPE)));
#endif
	if (p != NULL)
		strlib_strcpy(p, s);
	return p;
}

#endif // HAVE_STRDUP

#ifndef HAVE_STRCHR

CHAR_TYPE * STRCHR_NAME(const CHAR_TYPE *s, CHARPARAM_TYPE c0)
{
	OP_ASSERT( s != NULL );

	CHAR_TYPE c = (CHAR_TYPE)c0;

	while (*s && *s != c)
		++s;
	return *s == c ? const_cast<CHAR_TYPE *>(s) : NULL;
}

#endif // HAVE_STRCHR

#ifndef HAVE_STRRCHR

CHAR_TYPE * STRRCHR_NAME(const CHAR_TYPE * s, CHARPARAM_TYPE c0)
{
	OP_ASSERT( s != NULL );

	CHAR_TYPE c = (CHAR_TYPE)c0;
	const CHAR_TYPE * p = NULL;

	do
	{
		if (*s == c)
			p = s;
	} while (*s++ != 0);
	return const_cast<CHAR_TYPE *>(p);
}

#endif // HAVE_STRRCHR

#ifndef HAVE_STRSTR

CHAR_TYPE * STRSTR_NAME(const CHAR_TYPE * haystack, const CHAR2_TYPE * needle0)
{
	OP_ASSERT( haystack != NULL && needle0 != NULL );

	if (!*needle0)
		return (CHAR_TYPE *) haystack;
	if (*haystack)
	{
		const UCHAR2_TYPE *needle = (const UCHAR2_TYPE *)needle0;
		do
		{
			if (*needle == *((UCHAR_TYPE*)haystack))
			{
				// Continue scanning if we find a match
				++ needle;

				// Return pointer to beginning of string when we have scanned to
				// the end of needle.
				if (!*needle)
					return (CHAR_TYPE *) haystack - ((const CHAR2_TYPE *) needle - needle0) + 1;
			}
			else if (needle != (const UCHAR2_TYPE *) needle0)
			{
				// Else backtrack to the start of the current substring.
				haystack -= ((const CHAR2_TYPE *) needle - needle0);
				needle = (const UCHAR2_TYPE *) needle0;
			}

			++ haystack;
		} while (*haystack);
	}

	return NULL;
}

#endif // HAVE_STRSTR

#ifndef HAVE_STRISTR

const CHAR_TYPE *STRISTR_NAME(const CHAR_TYPE * haystack0, const CHAR2_TYPE * needle0)
{
	const UCHAR_TYPE *haystack = (UCHAR_TYPE*)haystack0;
	const UCHAR2_TYPE *needle = (UCHAR2_TYPE*)needle0;
	int h = 0, n, first, i;
	int first_caseless;

	OP_ASSERT(haystack && needle);

	n = *needle;
	if (!n)
		return haystack0;

	// Cache lower-case version of the first character in needle
	first = op_tolower(n);
	first_caseless = (first == op_toupper(n));

	do
	{
		// Scan until we find the first letter of the needle
		if (first_caseless)
		{
			haystack = (UCHAR_TYPE*) STRCHR_NAME((CHAR_TYPE*)haystack, (CHARPARAM_TYPE)first);
			if (!haystack)
				return NULL; // Reached end of input
		}
		else
		{
			while ((h = *haystack) != 0 && first != op_tolower(h))
				++haystack;
			if (!h)
				return NULL; // Reached end of input
		}

		// After the first letter, match haystack and needle letter for letter
		i = 1;
		while ((n = needle[i]) != 0 && (h = haystack[i]) != 0 && op_tolower(h) == op_tolower(n))
			++i;

		if (!n)
			return (CHAR_TYPE*)haystack; // Found a match
		if (!h)
			return NULL; // Reached end of input

		haystack++;
	}
	while (1);
}

#endif // HAVE_STRISTR

#ifndef HAVE_STRSPN

/** Calculate length of prefix containing only characters from accept. */

size_t STRSPN_NAME(const CHAR_TYPE *string, const CHAR_TYPE *accept)
{
	OP_ASSERT( string != NULL && accept != NULL );

	int count = 0;
	bool found;

	while (*string)
	{
		found = false;
		for (const CHAR_TYPE *c = accept; *c && !found; c ++)
		{
			// Continue if we find a character from accept
			if (*c == *string)
				found = true;
		}

		// Return if we didn't find it
		if (!found)
			return count;

		count ++; string ++;
	}

	return count;
}

#endif // HAVE_STRSPN

#ifndef HAVE_STRCSPN

/** Calculate length of prefix not containing characters from reject. */
size_t STRCSPN_NAME(const CHAR_TYPE *string, const CHAR_TYPE *reject)
{
	OP_ASSERT( string != NULL && reject != NULL );

	int count = 0;

	while (*string)
	{
		for (const CHAR_TYPE *c = reject; *c; c ++)
		{
			// Exit if we find a character from reject
			if (*c == *string)
				return count;
		}

		count ++; string ++;
	}

	return count;
}

#endif // HAVE_STRCSPN

#ifndef HAVE_STRPBRK

CHAR_TYPE * STRPBRK_NAME(const CHAR_TYPE *str, const CHAR_TYPE *charset)
{
	OP_ASSERT( str != NULL && charset != NULL );

	for (; *str; ++str)
		for (const CHAR_TYPE* tmp = charset; *tmp; ++tmp)
			if (*str == *tmp)
				return (CHAR_TYPE *)str;
	return NULL;
}

#endif // HAVE_STRPBRK

#ifndef HAVE_CONVERTING_STRDUP

CHAR2_TYPE * CONVERTING_STRDUP_NAME(const CHAR_TYPE *s)
{
	if (s == NULL) 
		return NULL;

	CHAR2_TYPE *copy = (CHAR2_TYPE*) op_malloc((strlib_strlen(s) + 1) * sizeof(CHAR2_TYPE));
	if (copy == NULL)
		return NULL;
	CHAR2_TYPE *copyptr = copy;
	while (*s)
	{
		OP_ASSERT(((UCHAR_TYPE) *s) == ((CHAR2_TYPE) *(UCHAR_TYPE *) s) || !"Unicode-to-ASCII conversion is lossy");
		*copyptr++ = (CHAR2_TYPE)*(UCHAR_TYPE*)s++;
	}

	*copyptr = 0;
	return copy;
}

#endif // HAVE_CONVERTING_STRDUP

#ifndef HAVE_STR_EQ

BOOL STR_EQ_NAME(const CHAR_TYPE * str1, const CHAR2_TYPE *str2)
{
	while (*str1 && *str1 == *(const UCHAR2_TYPE*)str2)
		str1++, str2++;
	return *str1 == *(const UCHAR2_TYPE*)str2;
}

#endif

#ifndef HAVE_STRREV

/** Reverse a Unicode string */
void STRREV_NAME(CHAR_TYPE *buffer)
{
    CHAR_TYPE *end = buffer;

#ifdef CHAR_UNICODE
	CHAR_TYPE *str = buffer;
	bool have_surrogate = false;
#endif
	
    while (*end)
	{
#ifdef CHAR_UNICODE
		if (Unicode::IsSurrogate(*end))
			have_surrogate = true;
#endif
        ++end;
	}

    while (--end > buffer)
    {
        CHAR_TYPE temp = *end;

        *end = *buffer;
        *buffer++ = temp;
    }

#ifdef CHAR_UNICODE
	if (have_surrogate)
	{
		// Fix up surrogate pairs
		while (*str)
		{
			if (Unicode::IsLowSurrogate(*str) && Unicode::IsHighSurrogate(*(str + 1)))
			{
				CHAR_TYPE high_surrogate = *(str + 1);
				*(str + 1) = *str;
				*(str ++) = high_surrogate;
			}
			++ str;
		}
	}
#endif
}

#endif // HAVE_STRREV

#ifndef HAVE_STRCASE

void STRUPR_NAME(CHAR_TYPE* buffer)
{
    while (*buffer)
    {
        *buffer = op_toupper(*(UCHAR_TYPE*)buffer);
        ++buffer;
    }
}

void STRLWR_NAME(CHAR_TYPE* buffer)
{
    while (*buffer)
    {
        *buffer = op_tolower(*(UCHAR_TYPE*)buffer);
        ++buffer;
    }
}

#endif // HAVE_STRCASE
