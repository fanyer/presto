/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; coding: iso-8859-1 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/util/str.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#ifdef FORMATS_URI_ESCAPE_SUPPORT
#include "modules/formats/uri_escape.h"
#endif

#ifdef HAVE_LTH_MALLOC
# include "modules/memtools/happy-malloc.h"

# define STRDUP_ALLOC_LOCATION() LTH_MALLOC_SITE(site, MALLOC_TAG_ANEW|MALLOC_TAG_STRDUP)
# define SETSTR_ALLOC_LOCATION() LTH_MALLOC_SITE(site, MALLOC_TAG_ANEW|MALLOC_TAG_SETSTR)
# define NEW_CHAR(x)             (char*)internal_malloc(x, site)
# define NEW_UNI_CHAR(x)         (uni_char*)internal_malloc((x)*sizeof(uni_char), site)
#else // HAVE_LTH_MALLOC
# define STRDUP_ALLOC_LOCATION()
# define SETSTR_ALLOC_LOCATION()
# define NEW_CHAR(x)             OP_NEWA(char, x)
# define NEW_UNI_CHAR(x)         OP_NEWA(uni_char, x)
#endif // HAVE_LTH_MALLOC

OP_STATUS StrToOpFileLength(const char* nptr, OpFileLength *file_length,  int base, char** endptr)
{
#ifdef _DEBUG
	int n = 0;
	op_sscanf ("0","%d",&n); // Just a hack to trick the compiler to NOT give a warning
	OpFileLength value = (OpFileLength)n; // value = 0;

# ifdef OPFILE_LENGTH_IS_SIGNED
	value--;
	if (value > 0)
		OP_ASSERT(!"Platform claims that OpFileLength is signed while it is unsigned!");
# else
	value--;
	if (!(value > 0))
		OP_ASSERT(!"Platform claims that OpFileLength is unsigned while it is signed!");
# endif
#endif // _DEBUG

#ifdef OPFILE_LENGTH_IS_SIGNED
	OP_ASSERT
	(
		"FILE_LENGTH_MAX does not have correct size" &&
		(
			(sizeof(OpFileLength) == 8 && FILE_LENGTH_MAX == ((((OpFileLength)1) << 62 ) - 1  + ((OpFileLength)1 << 62))) ||
			(sizeof(OpFileLength) == 4 && FILE_LENGTH_MAX ==  (((OpFileLength)1) << 31) - 1)
		)
	 );
#else
	OP_ASSERT(FILE_LENGTH_MAX ==  ~((OpFileLength)0) && "FILE_LENGTH_MAX does not have correct size");
#endif

	*file_length = 0;
    OpFileLength answer = 0;

    if (base == 1 || base < 0 || base > 36)
	{
    	return OpStatus::ERR;
	}
    else if (nptr)
	{
        BOOL representable = FALSE;

		/* Leading space and sign */

		while (op_isspace(*nptr))
			nptr++;

		/* Base cleanup */

		if (base == 0)
		{
			base = 10;
			if (nptr[0] == '0')
			{
				if (nptr[1] == 'x' || nptr[1] == 'X')
				{
					nptr += 2;
					base = 16;
				}
				else
					base = 8;
			}
		}
		else if (base == 16)
		{
			if (nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X'))
				nptr += 2;
		}

		/* Consume input */
        const OpFileLength realbig = FILE_LENGTH_MAX / base;
        for (;;)
		{
			const char c = *nptr;

			int dig = -1;
			if (c >= '0' && c <= '9')
				dig = c - '0';
			else if (c >= 'A' && c <= 'Z')
				dig = (c - 'A') + 10;
			else if (c >= 'a' && c <= 'z')
				dig = (c - 'a') + 10;

            if (dig < 0 || dig >= base)
				break;

			if (answer > realbig || (answer *= base) > FILE_LENGTH_MAX - dig)
			{
				return OpStatus::ERR_OUT_OF_RANGE;

			}

			answer += dig;

			representable = TRUE;

			nptr++;
        }

    	if (!representable)
    		return OpStatus::ERR;

    }


    if (endptr != 0)
		*(const char **)endptr = nptr;

    *file_length = answer;
    return OpStatus::OK;
}

UnicodePoint ConvertHexToUnicode(int hex_start, int hex_end, int& hex_pos, const uni_char* hex_string)
{
	UnicodePoint charcode = 0, shift = 0;
	hex_pos = hex_end;

	while (hex_pos > hex_start)
	{
		int digit = op_tolower(hex_string[hex_pos-1]);

		if (digit >= '0' && digit <= '9')
			digit -= '0';
		else if (digit >= 'a' && digit <= 'f')
			digit -= 'a' - 10;
		else break;

		charcode += digit << shift;
		shift += 4;
		hex_pos--;

		/* The highest codepoint in Unicode is U+10FFFF, so stop if we have
		 * U+10000 or higher, or more than six digits
		 */
		if (shift > 24 || charcode >= 0x10000)
			break;
	}
	if (charcode > 0x10FFFF)
	{
		// For instance 200000; skip the last two digits
		charcode &= 0xFFFF;
		hex_pos += 2;
	}

	return charcode;
}

char *stripdup( const char *str)
{
	STRDUP_ALLOC_LOCATION();

	if (!str)
		return NULL;

	int cbResLen = 0;
	const char *szStartPtr = str;

	//	Make sure we have some characters to work with
	if (*str)
	{
		const char *szEndPtr = szStartPtr + op_strlen(szStartPtr) - 1;

		//	Find start for result
		while( *szStartPtr && op_isspace((unsigned char) *szStartPtr))
			++ szStartPtr;

		//	Find end for result
		while( (szEndPtr>szStartPtr) && op_isspace((unsigned char) *szEndPtr))
			-- szEndPtr;

		//	Calc size of result
		cbResLen = szEndPtr - szStartPtr + 1;
	}

	if (cbResLen < 0)
		return NULL;	// should not happen

	char* szResult = NEW_CHAR(cbResLen+1);
	if (szResult == NULL)
		return NULL;
	op_memcpy(szResult, szStartPtr, cbResLen);
	szResult[cbResLen] = '\0';

	return szResult;
}

uni_char* uni_stripdup(const uni_char *str)
{
	STRDUP_ALLOC_LOCATION();

	if (!str)
		return NULL;

	int cbResLen = 0;
	const uni_char *szStartPtr = str;

	//	Make sure we have some characters to work with
	if (*str)
	{
		const uni_char *szEndPtr = szStartPtr + uni_strlen(szStartPtr) - 1;

		//	Find start for result
		while( *szStartPtr && uni_isspace(*szStartPtr))
			++ szStartPtr;

		//	Find end for result
		while( (szEndPtr>szStartPtr) && uni_isspace(*szEndPtr))
			-- szEndPtr;

		//	Calc size of result
		cbResLen = szEndPtr - szStartPtr + 1;
	}

	if (cbResLen < 0)
		return NULL;	// should not happen

	uni_char* szResult = NEW_UNI_CHAR(cbResLen+1);
	if (szResult == NULL)
		return NULL;
	op_memcpy(szResult, szStartPtr, UNICODE_SIZE(cbResLen));
	szResult[cbResLen] = 0;

	return szResult;
}

char *StrMultiCat(char *target,  const char *source1, const char *source2, const char *source3,
				  const char *source4, const char *source5, const char *source6, const char *source7)
{
	if(target)
	{
		if(source1)
			op_strcat(target,source1);
		if(source2)
			op_strcat(target,source2);
		if(source3)
			op_strcat(target,source3);
		if(source4)
			op_strcat(target,source4);
		if(source5)
			op_strcat(target,source5);
		if(source6)
			op_strcat(target,source6);
		if(source7)
			op_strcat(target,source7);
	}
	return target;
}

char *StrMultiCat(char *target,  const char *source1, const char *source2, const char *source3)
{
	if(target)
	{
		if(source1)
			op_strcat(target,source1);
		if(source2)
			op_strcat(target,source2);
		if(source3)
			op_strcat(target,source3);
	}
	return target;
}

uni_char *StrMultiCat(uni_char *target,  const uni_char *source1, const uni_char *source2, const uni_char *source3)
{
	if(target)
	{
		if(source1)
			uni_strcat(target,source1);
		if(source2)
			uni_strcat(target,source2);
		if(source3)
			uni_strcat(target,source3);
	}
	return target;
}

uni_char *StrMultiCat(uni_char *target,        const uni_char *source1, const uni_char *source2, const uni_char *source3,
                      const uni_char *source4, const uni_char *source5, const uni_char *source6, const uni_char *source7)
{
	if(target)
	{
		if(source1)
			uni_strcat(target,source1);
		if(source2)
			uni_strcat(target,source2);
		if(source3)
			uni_strcat(target,source3);
		if(source4)
			uni_strcat(target,source4);
		if(source5)
			uni_strcat(target,source5);
		if(source6)
			uni_strcat(target,source6);
		if(source7)
			uni_strcat(target,source7);
	}
	return target;
}

int StrCatSprintf(char *target,  const char *format, ... )
{
	va_list marker;
	int ret;
	
	va_start(marker,format);
	ret = op_vsprintf(target + op_strlen(target),format,marker);
	va_end(marker);
	return ret;
}

int StrCatSnprintf(char *target, unsigned int max_len, const char *format, ... )
{
	va_list marker;
	int ret;
	
	va_start(marker,format);
	ret = op_vsnprintf(target + op_strlen(target), max_len, format,marker);
	va_end(marker);
	return ret;
}

#ifdef UTIL_STRCATSPRINTFUNI
int StrCatSprintf(uni_char *target,  const uni_char *format, ... )
{
	va_list marker;
	int ret;
	
	va_start(marker,format);
	ret = uni_vsprintf(target + uni_strlen(target),format,marker);
	va_end(marker);
	return ret;
}

int StrCatSnprintf(uni_char *target, unsigned int max_len, const uni_char *format, ... )
{
	va_list marker;
	int ret;
	
	va_start(marker,format);
	ret = uni_vsnprintf(target + uni_strlen(target),max_len, format,marker);
	va_end(marker);
	return ret;
}
#endif // UTIL_STRCATSPRINTFUNI

OP_STATUS UniSetStr(uni_char* &str, const uni_char* val)
{
	SETSTR_ALLOC_LOCATION();

	OP_DELETEA(str);
	str = NULL;

	if (val)
	{
		str = NEW_UNI_CHAR(uni_strlen(val)+1);
		if (str == NULL)
			return OpStatus::ERR_NO_MEMORY;
		uni_strcpy(str, val);
	}
	return OpStatus::OK;
}

OP_STATUS UniSetStrN(uni_char* &str, const uni_char* val, int len)
{
	SETSTR_ALLOC_LOCATION();

	OP_DELETEA(str);
	str = NULL;

	if (val && len > 0)
	{
		str = NEW_UNI_CHAR(len+1);
		if (str == NULL)
			return OpStatus::ERR_NO_MEMORY;
		uni_strncpy(str, val, len);
		str[len] = 0;
	}
	return OpStatus::OK;
}

OP_STATUS UniSetStr_NotEmpty(uni_char *&target, const uni_char *source, int *len)
{
	SETSTR_ALLOC_LOCATION();

	OP_DELETEA(target);

	int source_len = (source) ? uni_strlen(source) : 0;

	target = NEW_UNI_CHAR(source_len+1);
	if (target == NULL)
		return OpStatus::ERR_NO_MEMORY;
	if (source_len)
		uni_strlcpy(target, source, source_len + 1);
	else
		target[0] = '\0';
	
	if (len) *len = source_len;
	return OpStatus::OK;
}

uni_char* UniSetNewStr(const uni_char* val)
{
	SETSTR_ALLOC_LOCATION();

	uni_char *str = NULL;
	if (val)
	{
		str = NEW_UNI_CHAR(uni_strlen(val)+1);
		if (str == NULL)
			return NULL;
		uni_strcpy(str, val);
	}
	return str;
}

uni_char* UniSetNewStrN(const uni_char* val, int len)
{
	SETSTR_ALLOC_LOCATION();

	uni_char *str = NULL;
	if (val)
	{
		str = NEW_UNI_CHAR(len+1);
		if (str == NULL)
			return NULL;
		uni_strncpy(str, val, len);
		str[len] = 0;
	}
	return str;
}

uni_char *UniSetNewStr_NotEmpty(const uni_char* source)
{
	SETSTR_ALLOC_LOCATION();

	uni_char *str = NULL;

	int source_len = (source) ? uni_strlen(source) : 0;

	str = NEW_UNI_CHAR(source_len+1);
	if (str == NULL)
		return NULL;
	if (source_len)
		uni_strlcpy(str, source, source_len+1);
	else
		str[0] = '\0';
	return str;
}

OP_STATUS SetStr(char* &str, const char* val)
{
	SETSTR_ALLOC_LOCATION();

	OP_DELETEA(str);
	str = NULL;

	if (val)
	{
		str = NEW_CHAR(op_strlen(val)+1);
		if (str == NULL)
			return OpStatus::ERR_NO_MEMORY;
		op_strcpy(str, val);
	}
	return OpStatus::OK;
}

OP_STATUS SetStr(uni_char* &str, const char* val)
{
	SETSTR_ALLOC_LOCATION();

	OP_DELETEA(str);
	str = NULL;

	if (val)
	{
		int valLen = op_strlen(val);
		int strLen = valLen + 1;

		str = NEW_UNI_CHAR(strLen);
		if (str == NULL)
			return OpStatus::ERR_NO_MEMORY;
		make_doublebyte_in_buffer(val, valLen, str, strLen);
	}
	return OpStatus::OK;
}

OP_STATUS SetStr(uni_char* &str, const uni_char* val)
{
	SETSTR_ALLOC_LOCATION();

	OP_DELETEA(str);
	str = NULL;

	if (val)
	{
		str = NEW_UNI_CHAR(uni_strlen(val)+1);
		if (str == NULL)
			return OpStatus::ERR_NO_MEMORY;
		uni_strcpy(str, val);
	}
	return OpStatus::OK;
}

OP_STATUS SetStr(char* &str, const char* val, int len)
{
	SETSTR_ALLOC_LOCATION();

	OP_DELETEA(str);
	str = NULL;

	if (val && len > 0)
	{
		str = NEW_CHAR(len+1);
		if (str == NULL)
			return OpStatus::ERR_NO_MEMORY;
		op_strncpy(str, val, len);
		str[len] = 0;
	}
	return OpStatus::OK;
}

char *SetNewStr(const char* val)
{
	SETSTR_ALLOC_LOCATION();

	char *str = NULL;
	if (val)
	{
		str = NEW_CHAR(op_strlen(val)+1);
		if (str == NULL)
			return NULL;
		op_strcpy(str, val);
	}
	return str;
}

uni_char *SetNewStr(const uni_char* val)
{
	SETSTR_ALLOC_LOCATION();

	uni_char *str = NULL;
	if (val)
	{
		str = NEW_UNI_CHAR(uni_strlen(val)+1);
		if (str == NULL)
			return NULL;
		uni_strcpy(str, val);
	}
	return str;
}

char *SetNewStr_NotEmpty(const char* source)
{
	SETSTR_ALLOC_LOCATION();

	char *str = NULL;

	int source_len = (source) ? op_strlen(source) : 0;

	str = NEW_CHAR(source_len+1);
	if (str == NULL)
		return NULL;
	if (source_len)
		op_strlcpy(str, source, source_len+1);
	else
		str[0] = '\0';
	return str;
}

#ifdef UTIL_SET_NEW_CAT_STR
char* SetNewCatStr(const char* val,const char* val1,const char* val2,const char* val3, const char* val4, const char* val5, const char* val6)
{
	SETSTR_ALLOC_LOCATION();

	char *str = NULL;
	int nlen = 0;
	if(val)
		nlen +=	op_strlen(val);
	if(val1)
		nlen +=	op_strlen(val1);
	if(val2)
		nlen +=	op_strlen(val2);
	if(val3)
		nlen +=	op_strlen(val3);
	if(val4)
		nlen += op_strlen(val4);
	if(val5)
		nlen += op_strlen(val5);
	if(val6)
		nlen += op_strlen(val6);

	str = NEW_CHAR(nlen+1);
	if (str == NULL)
		return NULL;
	str[0] = 0;

	return StrMultiCat(str, val, val1, val2, val3, val4, val5, val6);
}

uni_char* SetNewCatStr(const uni_char* val, const uni_char* val1, const uni_char* val2, const uni_char* val3, const uni_char* val4)
{
	SETSTR_ALLOC_LOCATION();

	uni_char *str = NULL;
	int nlen = 0;
	if(val)
		nlen +=	uni_strlen(val);
	if(val1)
		nlen +=	uni_strlen(val1);
	if(val2)
		nlen +=	uni_strlen(val2);
	if(val3)
		nlen +=	uni_strlen(val3);
	if(val4)
		nlen += uni_strlen(val4);

	str = NEW_UNI_CHAR(nlen+1);
	if (str == NULL)
		return NULL;
	str[0] = 0;

	return StrMultiCat(str, val, val1, val2, val3, val4);
}
#endif // UTIL_SET_NEW_CAT_STR

OP_STATUS SetStr_NotEmpty(char *&target, const char *source, int *len)
{
	SETSTR_ALLOC_LOCATION();

	OP_DELETEA(target);

	int source_len = (source) ? op_strlen(source) : 0;

	target = NEW_CHAR(source_len+1);
	if (!target)
		return OpStatus::ERR_NO_MEMORY;
	if (source_len)
		op_strlcpy(target, source, source_len+1);
	else
		target[source_len] = '\0';
	
	if (len) *len = source_len;
	return OpStatus::OK;
}

BOOL MatchExpr(const uni_char* name, const uni_char* expr, BOOL complete_match)
{
	if (name && expr)
	{
		const uni_char* name_tmp = name;
		uni_char* expr_tmp = (uni_char*) expr;
		while (*expr_tmp)
		{
			uni_char* tmp = uni_strchr(expr_tmp, '*');
			if (tmp == expr_tmp)
			{
				expr_tmp++;
				uni_char* tmp2 = uni_strchr(expr_tmp, '*');

				while(tmp2 && ((tmp2) == expr_tmp)) //check if this was two asterix' together
				{
					uni_char* tmp3 = tmp2+1;
					tmp2 = uni_strchr(tmp3, '*');
				}

				if (tmp2)
					*tmp2 = '\0';

				const uni_char* match = uni_stristr(name_tmp, expr_tmp);

				if (tmp2)
					*tmp2 = '*';

				while (match && *match)
				{
					if (!tmp2)
						return (!complete_match || !*expr_tmp || !match[uni_strlen(expr_tmp)]);

					if (tmp2 && MatchExpr(match+(tmp2-expr_tmp), tmp2,complete_match))
						return TRUE;

					name_tmp = match+1;
					*tmp2 = '\0';
					match = uni_stristr(name_tmp, expr_tmp);
					*tmp2 = '*';
				}

				return FALSE;
			}

			unsigned int len = 0;
			if (tmp)
				len = tmp-expr_tmp;
			else
				len = uni_strlen(expr_tmp);

			if (uni_strnicmp(name_tmp, expr_tmp, len) != 0)
				return FALSE;

			name_tmp += len;
			expr_tmp += len;
		}

		if (!complete_match || !*name_tmp)
			return TRUE;
	}
	return FALSE;
}

const unsigned char* strnstr(const unsigned char* string, int length, const unsigned char* find)
{
	int findlen = op_strlen((const char*) find);

	while (findlen <= length)
	{
		if (op_strncmp((const char*) string, (const char*) find, findlen) == 0)
			return string;

		++string;
		--length;
	}
	return NULL;
}

uni_char* StrReplaceChars( uni_char *szArg, uni_char chToFind, uni_char chReplace)
{
	if (IsStr(szArg))
	{
		uni_char *pch = szArg;
		while (*pch)
		{
			if (*pch == chToFind)
				*pch = chReplace;
			++ pch;
		}
	}
	return szArg;
}


#ifdef UTIL_STRTRIMCHARS

//	DG-2000-01-20
//	___________________________________________________________________________

//	StrTrimChars
//	___________________________________________________________________________
//
uni_char *StrTrimChars
(
	uni_char			*pszArg,
	const uni_char		*pszCharSet,
	OPSTR_TRIMTYPE		trimKind
)
{
	//
	//	Param sanity check
	//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	//	verify trimKind
	if (! ((trimKind > OPSTR_TRIMKIND_UNUSED_FIRST_ENUM && trimKind < OPSTR_TRIMKIND_UNUSED_LAST_ENUM)))
		return pszArg;
	
	//	Make sure we have some characters to work with
	if (!IsStr(pszArg) || !IsStr(pszCharSet))
		return pszArg;

	if (trimKind==OPSTR_TRIM_ALL)
	{
		uni_char *pszDest = pszArg;
		uni_char *pszNext = pszArg;
		int finalLen = uni_strlen( pszArg);
		while (*pszNext)
		{
			if (uni_strchr( pszCharSet, *pszNext))
				-- finalLen;
			else
			{
				*pszDest = *pszNext;
				++ pszDest;
			}
			++ pszNext;
		}
		pszArg[finalLen] = 0;
	}
	else
	{
		//
		//	OPSTR_TRIM_LEFT / OPSTR_TRIM_RIGHT
		//	Find the start and end pointers
		//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		uni_char	*pszStart	= pszArg;
		uni_char	*pszEnd		= StrEnd(pszArg);
	
		//	Find START for result
		if (trimKind & OPSTR_TRIM_LEFT)
		{
			while ( *pszStart && uni_strchr( pszCharSet, *pszStart))
				++ pszStart;
		}

		//	Find END for result
		if (trimKind & OPSTR_TRIM_RIGHT)
		{
			while ( (pszEnd>pszStart) && uni_strchr( pszCharSet, *pszEnd))
				-- pszEnd;
		}

		//
		//	Make result
		//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		long cbResLen = UNICODE_SIZE(pszEnd - pszStart + 1);
		if (cbResLen >= 0)
		{
			op_memmove( pszArg, pszStart, cbResLen);
			pszArg[UNICODE_DOWNSIZE(cbResLen)] = '\0';
		}
	}

	return pszArg;
}

#endif // UTIL_STRTRIMCHARS

#ifdef UTIL_POINTER_OVERLAP
BOOL IsPointerOverlapping( const void* ptr1Start, size_t lenPtr1, const void* ptr2Start, size_t lenPtr2)
{
	
	if( !ptr1Start || !ptr2Start)	//	NULL pointers never overlap
		return FALSE;

	if (ptr1Start == ptr2Start && lenPtr1 == 0 && lenPtr2 == 0)
		return TRUE;

	const BYTE* ptr1End = (const BYTE *)ptr1Start + lenPtr1 - 1;
	const BYTE* ptr2End = (const BYTE *)ptr2Start + lenPtr2 - 1;

	//
	//	the entire ptr1 has to be on one size of ptr2
	//
	BOOL fOverlap = !((ptr1End < ptr2Start) ||  (ptr1Start > ptr2End));
	return fOverlap;
}
#endif // UTIL_POINTER_OVERLAP

#ifdef UTIL_STRTRIMCHARS
uni_char* StrTrimLeftStrings
(
	uni_char*		pszInp,
	const uni_char*	arrayOfStringsToRemove[],
	int				nItemsInArrayOfStringsToRemove,
	BOOL			fCaseSensitive
)
{
	if (IsStr(pszInp))
	{
		if( arrayOfStringsToRemove && nItemsInArrayOfStringsToRemove)
		{
			uni_char* pszInpStart = pszInp;
			int lenInp = uni_strlen( pszInpStart);

			//
			//	For each needle
			//
			for (int i=0; lenInp>0 && i<nItemsInArrayOfStringsToRemove; i++)
			{
				const uni_char *pszNeedle = arrayOfStringsToRemove[i];
				if (pszNeedle)
				{
					//
					//	Check len
					//
					int lenNeedle = uni_strlen( pszNeedle);
					if (lenNeedle>0 && lenNeedle<lenInp)
					{
						//
						//	Check actual string
						//
						BOOL fMatch = FALSE;
						if (fCaseSensitive)
							fMatch = uni_strncmp(pszInpStart, pszNeedle, lenNeedle) == 0;
						else
							fMatch = uni_strnicmp(pszInpStart, pszNeedle, lenNeedle) == 0;

						//
						//	Match
						//
						if (fMatch)
						{
							pszInpStart += lenNeedle;
							lenInp -= lenNeedle;
							OP_ASSERT (lenInp>=0);
						}
					}
				}
			}

			//
			//	Make result.
			//	Note - pointer overlap
			//
			op_memmove( pszInp, pszInpStart, UNICODE_SIZE(lenInp+1));
		}
	}
	return pszInp;
}
#endif // UTIL_STRTRIMCHARS

#ifdef UTIL_MYUNISTRTOK
uni_char *MyUniStrTok(uni_char *str, const uni_char *dividers, int &pos, BOOL &done)
{
    if (!str || !dividers)
    {
        done = TRUE;
        return 0;
    }
    uni_char *sub = str + pos;
    int i;
	int len = uni_strlen(dividers);
    while (str[pos])
    {
        for (i = 0; i < len; i++)
            if (str[pos] == dividers[i])
            {
                str[pos++] = 0;
                return sub;
            }
        pos ++;
    }
    if (sub < str + pos)
        return sub;
    done = TRUE;
    return 0;
}
#endif // UTIL_MYUNISTRTOK

#define ARG_CHAR '%'   // character to use as argument indicator
#define LONG_LENGTH 12 // longest possible int32 is -2147383648 (11 chars+\0)

int shuffle_arg(uni_char *out, size_t len, const uni_char *format, const uni_char *first, const uni_char *second, const uni_char *third)
{
	int m = 0;
	int n = 0;

	const uni_char* args[3] = {first,second,third}; // ARRAY OK 2011-05-31 markuso

	// do exception handling

	if (!format || !out || !len)
		return FALSE;

	uni_char* end = out + len - 1; // last writeable char

	while (TRUE)
	{
		// copy text as long as not argument indicator
		while (TRUE)
		{
			if (out == end)
				goto done;

			m = *format++;
			if (m == ARG_CHAR)
				break;
			if (m == '\0')
				goto done;
			*out++ = m;
		}

		n = *format++;
		// if argument indicator at end of string then end all
		if (n == '\0')
		{
			goto done;
		}
		// if legal argument then get argument and print it
		else if ('1' <= n && n <= '3' && args[n-'1'])
		{
			const uni_char* my = args[n-'1'];
			// copy the argument string
			while (*my)
			{
				if (out == end)
					goto done;
				*out++ = *my++;
			}

		}
		// if argument indicator twice, then echo actual character
		else if (n == ARG_CHAR)
		{
			if (out == end)
				goto done;
			*out++ = n;
		}
		// if argument not recognized, then print both characters as is
		else
		{
			if (out == end)
				goto done;
			*out++ = m;

			if (out == end)
				goto done;
			*out++ = n;
		}
	}

done:

	*out = '\0';
	return TRUE;
}

// one string
int uni_snprintf_s(uni_char *out, size_t len, const uni_char *format, const uni_char *first)
{
	return shuffle_arg(out, len, format, first, NULL, NULL);
}

// two strings
int uni_snprintf_ss(uni_char *out, size_t len, const uni_char *format, const uni_char *first, const uni_char *second) {
	return( shuffle_arg(out, len, format, first, second, NULL) );
}

// three strings
int uni_snprintf_sss(uni_char *out, size_t len, const uni_char *format, const uni_char *first, const uni_char *second, const uni_char *third)
{
	return shuffle_arg(out, len, format, first, second, third);
}

// string, then integer
int uni_snprintf_si(uni_char *out, size_t len, const uni_char *format, const uni_char *first, int two) {

	uni_char second[LONG_LENGTH]; /* ARRAY OK 2009-04-24 johanh */

	uni_sprintf(second, UNI_L("%d"), two);

	return( shuffle_arg(out, len, format, first, second, NULL) );
}

#ifdef UTIL_CHECK_KEYWORD_INDEX
int CheckKeywordsIndex(const char *string, const KeywordIndex *keys, int count)
{
	if (!keys || count == 0)
		return -1;

	if(!string || count == 1)
		return keys[0].index;

	int left = 1;
	int right = count -1;

	do{
		if(left == right)
		{
			if(op_stricmp(string, keys[left].keyword) == 0)
				return keys[left].index;
			break;
		}
		else
		{
			int i = (left + right)>>1; // divide by 2

			int res = op_stricmp(string, keys[i].keyword);

			if(res == 0)
				return keys[i].index;
			else if (res < 0)
				right = i-1;
			else
				left = i+1;
		}
	}while(left <= right);

	return keys[0].index;
}
#endif // UTIL_CHECK_KEYWORD_INDEX

#ifdef UTIL_START_KEYWORD_CASE
int CheckStartsWithKeywordIndexCase(const char *string, const KeywordIndex *keys, int count, int &offset)
{
	int i;

	for(i =1; i< count; i++)
	{
		if(op_strncmp(string, keys[i].keyword,op_strlen(keys[i].keyword)) == 0)
		{
			offset = op_strlen(keys[i].keyword);
			return  keys[i].index;
		}
	}
	return  keys[0].index;
}
#endif // UTIL_START_KEYWORD_CASE

#ifdef UTIL_CHECK_KEYWORD_INDEX
int CheckStartsWithKeywordIndex(const char *string, const KeywordIndex *keys, int count)
{
	if (!keys)
		return -1;

	if (string)
	{
		for(int i=1; i<count; i++)
		{
			if(op_strnicmp(string, keys[i].keyword,op_strlen(keys[i].keyword)) == 0)
				return keys[i].index;
		}
	}
	return keys[0].index;
}
#endif // UTIL_CHECK_KEYWORD_INDEX

#ifdef UTIL_CHECK_KEYWORD_INDEX
int CheckKeywordsIndex(const char *string, int len, const KeywordIndex *keys, int count)
{
	if (!keys || count == 0)
		return -1;	//	*** DG-1999-11-19

	if(!string || count == 1)
		return keys[0].index;

	int left = 1;
	int right = count -1;

	do{
		if(left == right)
		{
			if(op_strnicmp(string, keys[left].keyword,len) == 0)
				return keys[left].index;
			break;
		}
		else
		{
			int i = (left + right)>>1; // divide by 2

			int res = op_strnicmp(string, keys[i].keyword,len);

			if(res == 0)
				return keys[i].index;
			else if (res < 0)
				right = i-1;
			else
				left = i+1;
		}
	}while(left <= right);

	return keys[0].index;
}
#endif // UTIL_CHECK_KEYWORD_INDEX

#ifdef UTIL_FIND_FILE_EXT
const uni_char *FindFileExtension(const uni_char *name, UINT* len)
{
	if (name && *name)
    {
		// Need to do this because we might have http://server url's without path

		const uni_char *fileext = uni_strstr(name, UNI_L("://"));

		if (fileext)
			fileext += 3;
		else
			fileext = name;

		uni_char *qp = (uni_char *) uni_strchr(fileext, '?');
		if (qp)
		{
			// Cannot use uni_strrchr since there might be slashes
			// and dots after the question mark.
			uni_char *tmp, *dot = NULL;
			for (tmp = qp; tmp > fileext; tmp --)
			{
				if ('.' == *tmp)
				{
					dot = tmp;
				}
				else if ('/' == *tmp)
				{
					break;
				}
			}
			if (dot)
			{
				if (len)
					*len = qp - (dot + 1);
				return dot + 1;
			}
		}
		else
		{
			const uni_char *tmp = (uni_char *) uni_strrchr(fileext, '/');
			fileext = tmp ? tmp : fileext;
			if (fileext)
			{
				fileext = (uni_char *) uni_strrchr(fileext, '.');
				if (fileext)
				{
					if (len)
						*len = uni_strlen(fileext+1);
					return fileext + 1;
				}
			}
		}
    }

	if (len)
		*len = 0;
	return NULL;
}
#endif // UTIL_FIND_FILE_EXT

#ifdef UTIL_METRIC_STRING_TO_INT
int MetricStringToInt(const char *str)
{
    int len = str ? op_strlen(str) : 0;

	if (!str || !len || len > 5)
        return 0;

    int i = 0;
    int val = 0;
    int j = 0;

    while (i < len)
    {
		if (op_isdigit((unsigned char) str[i]))
            val = val * 10 + str[i] - '0';
		else
			if (op_isspace((unsigned char) str[i]) && val != 0)
        		if (val > 500)
            		return 50000;
        		else
            		return val * 100;
			else
				if (str[i] == '.' || str[i] == ',')
				{
					i ++;
					break;
				}

        i ++;
    }

    val = val * 100;

    while(i < len)
    {
		if (op_isdigit((unsigned char) str[i]))
        {
            if (j == 0)
                val = val + 10 * (str[i] - '0');
            else
				if (j == 1)
					val = val + (str[i] - '0');
            j++;
            if (j == 2)
                break;
        }
        else
            return val;
        i++;
    }
    return val;
}
#endif // UTIL_METRIC_STRING_TO_INT

#ifdef UTIL_EXTRACT_FILE_NAME
void ExtractFileName(const uni_char *orgname, uni_char *extracted, int extracted_buffer_size, uni_char *formatted_ext, int formatted_buffer_size)
{
	// Clear output buffers
    extracted[0] = 0;
    formatted_ext[0] = 0;

	// Find start of file name
	const uni_char *filename_p = uni_strchr(orgname,':');
	filename_p = (filename_p == NULL ? orgname : filename_p + 1);

	const uni_char *next_fragment_p;
	do
	{
		next_fragment_p = uni_strpbrk(filename_p, UNI_L(";?/\\"));
		if (next_fragment_p == NULL)
			break;
		if (*next_fragment_p == '/' || *next_fragment_p == '\\')
			filename_p = next_fragment_p + 1;
	} while (*next_fragment_p != '?' && *next_fragment_p != ';');

	if (filename_p)
	{
		// Calculate filename length. Default is the number of characters
		// left in the string.
		int len = uni_strlen(filename_p);

		// If we have a query part ("?" or ";"), we restrict the length
		if (next_fragment_p != NULL)
			len = (next_fragment_p - filename_p);

		// Make sure we fit the buffer
		len = MIN(len, extracted_buffer_size - 1);
	    uni_strncpy(extracted, filename_p, len); // Exact buffer copying (=strncpy)
		extracted[len] = 0;
	}

	// Find file name extension
    uni_char *ext_p = uni_strrchr(extracted, '.');
    if (ext_p)
    {
        ext_p[0] = 0;
        ext_p++;
#ifdef UNIX
		if (ext_p == extracted)
		{
			ext_p = NULL;
		}
		else
#endif // UNIX
		{
			if (uni_strlen(ext_p) > 254)
			{
				ext_p[254] = 0;
			}
	
			// Replace "JPE" extension with jpg
			if (uni_strni_eq(ext_p, "JPE", 4))
				uni_strcpy(ext_p, UNI_L("jpg"));
		}
    }

	// Reconstruct file name if we have either a file name or an extension
	// component
    if (*extracted || ext_p)
    {
		// Temporary buffer to work in
		int max_len = MAX(extracted_buffer_size, formatted_buffer_size);
		uni_char *tmp_ext = OP_NEWA(uni_char, max_len);

		if (tmp_ext)
		{
			// Store extension, if we have one
			if (ext_p)
			{
				uni_strlcpy(tmp_ext, ext_p, max_len); //Store extension
			}
			else
			{
				*tmp_ext=0;
			}

			// If we have room to append extension, append it
            if (uni_strlen(extracted) < static_cast<size_t>(extracted_buffer_size - 1))
			{
                if(*tmp_ext) // If there is an extention to append
                {
                    uni_strlcat(extracted, UNI_L("."), extracted_buffer_size);
                    uni_strlcat(extracted, tmp_ext, extracted_buffer_size);
				}
			}

			extracted[extracted_buffer_size - 1] = 0; //Terminate string, just to be sure

			uni_snprintf(formatted_ext, formatted_buffer_size, UNI_L("*.%s%c*.%s%c%c"), tmp_ext, 0, tmp_ext, 0, 0);
		}
		OP_DELETEA(tmp_ext);
    }
}
#endif // UTIL_EXTRACT_FILE_NAME

#ifdef FORMATS_URI_ESCAPE_SUPPORT

char GetEscapedChar(const char c1, const char c2)
{
	return UriUnescape::Decode(c1, c2);
}

void ReplaceEscapedChars(char* request, int& len, BOOL replace_all)
{
	UriUnescape::ReplaceChars(request, len, replace_all ? UriUnescape::All : UriUnescape::NonCtrlAndEsc);
}

void ReplaceEscapedChars(uni_char* request, int& len, BOOL replace_all)
{
	UriUnescape::ReplaceChars(request, len, replace_all ? UriUnescape::All : UriUnescape::NonCtrlAndEsc);
}

/* Replace %xx-escaped URL inline, where %xx codes represent a UTF-8 encoded
 * URL, or a iso-8859-1 encoded URL */
void ReplaceEscapedCharsUTF8(uni_char* request, int& len, OPSTR_FILETYPE filetype, BOOL replace_all /* =FALSE */)
{
	int unescape_flags;
	switch (filetype)
	{
	case OPSTR_LOCALFILE:
		unescape_flags = UriUnescape::LocalfileUtf8;
		break;
	case OPSTR_LOCALFILE_URL:
		unescape_flags = UriUnescape::LocalfileUrlUtf8;
		break;
	case OPSTR_MAIL:
		unescape_flags = UriUnescape::MailUtf8;
		break;
	case OPSTR_MAIL_URL_BODY:
		unescape_flags = UriUnescape::MailUrlBodyUtf8;
		break;
	case OPSTR_OTHER:
	default:
		unescape_flags = UriUnescape::SafeUtf8;
		break;
	}
	if (replace_all)
		unescape_flags &= ~UriUnescape::ExceptUnsafeHigh;
	UriUnescape::ReplaceChars(request, len, unescape_flags);
}

#else // FORMATS_URI_ESCAPE_SUPPORT

char GetEscapedChar(const char c1, const char c2)
{
	// Bug#256294: Make sure your input is valid before calling this function.
	OP_ASSERT(op_isxdigit(c1));
	OP_ASSERT(op_isxdigit(c2));
	char cc1 = (c1 | 0x20) - '0';
	if (cc1 > 9) cc1 -= 0x27;
	char cc2 = (c2 | 0x20) - '0';
	if (cc2 > 9) cc2 -= 0x27;
	return (cc1 << 4) | cc2;
}

void ReplaceEscapedChars(char* request, int& len, BOOL replace_all)
{
	int new_len = 0;
	for (int i=0; i<len; i++, new_len++)
	{
		// Unescape escaped characters, but only if they are actual
		// hexadecimal characters
		if (request[i] == '%' && i + 2 < len &&
			uni_isxdigit(request[i + 1]) && uni_isxdigit(request[i + 2]))
		{
			unsigned char token = GetEscapedChar(request[i+1], request[i+2]);
			if(token >= 0x20 || token == 0x1b || replace_all) // Allow escape, but no other control characters
			{
				request[new_len] = token;
				i += 2;
			}
			else
				request[new_len] = '%';
		}
		else if (new_len != i)
			request[new_len] = request[i];
	}
	len = new_len;
}

void ReplaceEscapedChars(uni_char* request, int& len, BOOL replace_all)
{
	int new_len = 0;
	for (int i=0; i<len; i++, new_len++)
	{
		// Unescape escaped characters, but only if they are actual
		// hexadecimal characters
		if (request[i] == '%' && i + 2 < len &&
			uni_isxdigit(request[i + 1]) && uni_isxdigit(request[i + 2]))
		{
			unsigned char token = GetEscapedChar(char(request[i+1]), char(request[i+2]));
			if(token >= 0x20 || token == 0x1b || replace_all) // Allow escape, but no other control characters
			{
				request[new_len] = token;
				i += 2;
			}
			else
				request[new_len] = '%';
		}
		else if (new_len != i)
			request[new_len] = request[i];
	}
	len = new_len;
}

/* Replace %xx-escaped URL inline, where %xx codes represent a UTF-8 encoded
* URL, or a iso-8859-1 encoded URL */
void ReplaceEscapedCharsUTF8(uni_char* request, int& len, OPSTR_FILETYPE filetype, BOOL replace_all /* =FALSE */)
{
	BOOL use_utf8 = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseUTF8Urls) != 0;

	int new_len = 0;
	BOOL in_query = FALSE;
	for (int i=0; i<len; i++, new_len++)
	{
		if (request[i] == '%' && i+2 < len && !in_query &&
			uni_isxdigit(request[i+1]) && uni_isxdigit(request[i+2]))
		{
			unsigned char first = (unsigned char)
				GetEscapedChar(char(request[i+1]), char(request[i+2]));

			if ((use_utf8 || filetype==OPSTR_LOCALFILE || filetype == OPSTR_LOCALFILE_URL) && (first & 0x80))
			{
				// Check for possible UTF-8
				if (i+5 < len && (first & 0xE0) == 0xC0 && '%' == request[i+3] &&
					uni_isxdigit(request[i+1]) && uni_isxdigit(request[i+2]) &&
					uni_isxdigit(request[i+4]) && uni_isxdigit(request[i+5]))
				{
					unsigned char second = (unsigned char)
						GetEscapedChar(char(request[i+4]), char(request[i+5]));
					if ((second & 0xC0) == 0x80)
					{
						uni_char decoded = ((first & 0x1F) << 6) |
							(second & 0x3F);
						// Only decode as UTF-8 if it is proper UTF-8
						if (decoded >= 0x80)
						{
							request[new_len] = ((first & 0x1F) << 6) |
								(second & 0x3F);
							i += 5;
						}
						else
							request[new_len] = '%';
					}
					else
						request[new_len] = '%';
				}
				else if (i+8 < len && (first & 0xF0) == 0xE0 && '%' == request[i+3] && '%' == request[i+6] &&
						 uni_isxdigit(request[i+1]) && uni_isxdigit(request[i+2]) &&
						 uni_isxdigit(request[i+4]) && uni_isxdigit(request[i+5]) &&
						 uni_isxdigit(request[i+7]) && uni_isxdigit(request[i+8]))
				{
					unsigned char second = (unsigned char)
						GetEscapedChar(char(request[i+4]), char(request[i+5]));
					unsigned char third  = (unsigned char)
						GetEscapedChar(char(request[i+7]), char(request[i+8]));
					if ((second & 0xC0) == 0x80 && (third & 0xC0) == 0x80)
					{
						uni_char decoded = ((first & 0x1F) << 12) |
							((second & 0x3F) << 6) |
							(third & 0x3F);
						// Only decode as UTF-8 if it is proper UTF-8
						if (decoded >= 0x800 &&
							(replace_all ||
							 decoded != 0x115F &&  decoded != 0x1160 && decoded != 0x3164 &&// Filler characters that should not be displayed unencoded due to spoofing issues
							 decoded != 0x202E && // Right to left override should not be displayed. See bug 319181.
							 !(decoded >= 0x2000 && decoded <= 0x200B) && decoded != 0x3000// Spaces
							 )
							)
						{
							request[new_len] = decoded;
							i += 8;
						}
						else
							request[new_len] = '%';
					}
					else
						request[new_len] = '%';
				}
				else
					request[new_len] = '%';
			}
			else
			{
				if(filetype == OPSTR_OTHER)
				{
					switch(first)
					{
					default:
						if (first > 0x20 && first <= 0x7F)
						{
							request[new_len] = first;
							i += 2;
							break;
						}
					case 0x20: // space
					case ';':
					case '/':
					case '?':
					case ':':
					case '@':
					case '&':
					case '=':
					case '+':
					case '$':
					case ',':
					case '<':
					case '>':
					case '#':
					case '%':
					case '\0':
					case '{':
					case '[':
					case ']':
					case '}':
					case '|':
					case '\\':
					case '^':
					case '\"':
						request[new_len] = '%';
					}
				}
				else if( (first>=0x20 && (filetype==OPSTR_LOCALFILE || filetype == OPSTR_LOCALFILE_URL)) ||
						 ((first>=0x20 || op_isspace(first)) && filetype == OPSTR_MAIL) ||
						 filetype == OPSTR_MAIL_URL_BODY )
				{
					request[new_len] = first;
					i += 2;
				}
				else
					request[new_len] = '%';
			}
		}
#if PATHSEPCHAR != '/'
		else if ('/' == request[i] && filetype==OPSTR_LOCALFILE)
		{
			request[new_len] = PATHSEPCHAR;
		}
#endif
#ifdef SYS_CAP_FILESYSTEM_HAS_DRIVES
		else if ('|' == request[i] && filetype==OPSTR_LOCALFILE)
		{
			request[new_len] = ':';
		}
#endif
		else if (new_len != i)
		{
			request[new_len] = request[i];

			// Stop decoding %xx escapes when finding a query marker (bug#49350)
			if ('?' == request[new_len] && filetype != OPSTR_LOCALFILE && filetype != OPSTR_LOCALFILE_URL)
				in_query = TRUE;
		}
		else if ('?' == request[new_len] && filetype != OPSTR_LOCALFILE && filetype != OPSTR_LOCALFILE_URL)
		{
			// Stop decoding %xx escapes when finding a query marker (bug#49350)
			in_query = TRUE;
		}
	}
	len = new_len;
}

#endif // FORMATS_URI_ESCAPE_SUPPORT

//
//	===============================================================================================
//

#ifdef UTIL_REMOVE_CHARS
OP_STATUS RemoveChars(OpString& str, const OpStringC& charstoremove)
{
	OpString fixedstring;
	RETURN_IF_ERROR(fixedstring.Set(str));

	uni_char *p1 = fixedstring.DataPtr();
	uni_char *p2 = p1;
	uni_char *rp = const_cast<uni_char*>(charstoremove.DataPtr());
	int rlength  = charstoremove.Length();

	if( rlength > 1 && p1)
	{
		for(; *p1; p1++)
		{
			int keep = 1;
			for(int j=0; j<rlength; j++)
				if( *p1 == rp[j] )
					keep = 0;
			if( keep )
				*(p2++) = *p1;
		}
		*p2 = 0;
	}
	else if( rlength && p1)
	{
		uni_char remove_char = rp[0];
		for(; *p1; p1++)
			if( *p1 != remove_char )
				*(p2++) = *p1;
			*p2 = 0;
	}

	return str.Set(fixedstring);
}
#endif // UTIL_REMOVE_CHARS

#ifdef UTIL_UNI_ISSTRPREFIX_ESC
#ifdef FORMATS_URI_ESCAPE_SUPPORT

BOOL uni_isstrprefix_esc(const uni_char *prefix_esc, const uni_char *string2_esc)
{
	return UriUnescape::isstrprefix(prefix_esc, string2_esc, UriUnescape::All);
}

#else // FORMATS_URI_ESCAPE_SUPPORT

BOOL uni_isstrprefix_esc(const uni_char *prefix_esc, const uni_char *string2_esc)
{
	uni_char token1;
	uni_char token2;
	while(*prefix_esc && *string2_esc)
	{
		token1 = *(prefix_esc++);
		if (token1 == '%')
		{
			uni_char val1 = *(prefix_esc++);
			if(!val1)
				return TRUE;

			uni_char val2 = *(prefix_esc++);

			if( val2 == 0)
			   return TRUE;

			token1 = 0;

			if(val1 >= '0' && val1 <='9')
				token1 = (val1 & 0xf);
			else if(val1 >= 'A' && val1 <= 'F')
				token1 = (val1 -'A')+0x0A;
			else if(val1 >= 'a' && val1 <= 'f')
				token1 = (val1 -'a')+0x0A;

			token1 <<= 4;

			if(val2 >= '0' && val2 <='9')
				token1 |= (val2 & 0xf);
			else if(val2 >= 'A' && val2 <= 'F')
				token1 |= (val2 -'A')+0x0A;
			else if(val2 >= 'a' && val2 <= 'f')
				token1 |= (val2 -'a')+0x0A;
		}
		token2 = *(string2_esc++);
		if (token2 == '%')
		{
			uni_char val1 = *(string2_esc++);
			if(!val1)
				return FALSE;

			uni_char val2 = *(string2_esc++);

			if( val2 == 0)
			   return FALSE;

			token2 = 0;

			if(val1 >= '0' && val1 <='9')
				token2 |= (val1 & 0xf);
			else if(val1 >= 'A' && val1 <= 'F')
				token2 |= (val1 -'A')+0x0A;
			else if(val1 >= 'a' && val1 <= 'f')
				token2 |= (val1 -'a')+0x0A;

			token2 <<= 4;

			if(val2 >= '0' && val2 <='9')
				token2 |= (val2 & 0xf);
			else if(val2 >= 'A' && val2 <= 'F')
				token2 |= (val2 -'A')+0x0A;
			else if(val2 >= 'a' && val2 <= 'f')
				token2 |= (val2 -'a')+0x0A;
		}

		if(token1!= token2)
			return FALSE;
	}
	return (*prefix_esc ? FALSE : TRUE);
}

#endif // FORMATS_URI_ESCAPE_SUPPORT
#endif // UTIL_UNI_ISSTRPREFIX_ESC

BOOL
IsWhiteSpaceOnly(const uni_char* txt)
{
	for (; *txt; txt++)
		if (!uni_collapsing_sp(*txt))
			return FALSE;

	return TRUE;
}
