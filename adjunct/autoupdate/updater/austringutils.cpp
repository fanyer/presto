#include "core/pch.h"

#include "adjunct/autoupdate/updater/austringutils.h"

//////////////////////////////////////////////////////////////////////////////////////////////////

size_t u_strlen(const uni_char* s)
{
	const uni_char* t = s;
	while (*t++ != 0) {}
	return t - s - 1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

uni_char* u_strcpy(uni_char* dest, const uni_char* src)
{
	if (dest == NULL)
		return NULL;
	
	uni_char* dest0 = dest;
	
	while((*dest++ = *src++) != 0) {}
	return dest0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

uni_char* u_strncpy(uni_char* dest, const uni_char* src, size_t n)
{
	uni_char *dest0 = dest;
	
	while (n != 0 && *src != 0)
	{
		*dest++ = *src++;
		--n;
	}
	
	if (n != 0)
		*dest = 0;
	
	return dest0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

uni_char* u_strchr(const uni_char* s, uni_char c0)
{
	uni_char c = c0;
	
	while(*s && *s != c)
	{
		++s;
	}
	return *s == c ? const_cast<uni_char *>(s) : NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Finds the rightmost c in s.
// Neither s nor c should or can be NULL.
// Will return the address to the rightmost c in s if found, or NULL if not found.
uni_char* u_strrchr(const uni_char* s, uni_char c)
{
	uni_char* s2 = const_cast<uni_char *>(s) + u_strlen(s);
	
	while(s != s2 && *s2 != c)
	{
		--s2;
	}
	return *s2 == c ? s2 : NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

int u_strncmp(const uni_char *s1_0, const uni_char *s2_0, size_t n)
{
	const uni_char *s1 = (const uni_char *)s1_0;
	const uni_char *s2 = (const uni_char *)s2_0;
	
	while (n > 0 && *s1 && *s1 == *s2)
		++s1, ++s2, --n;
	return n > 0 ? (int)*s1 - (int)*s2 : 0;
}

/////////////////////////////////////////////////////////////////////////

uni_char* u_strcat(uni_char* dest, const uni_char* src)
{
	uni_char* dest0 = dest;
	
	while (*dest != 0)
		dest++;
	
	while ((*dest++ = *src++) != 0)
		;
	
	return dest0;
}

/////////////////////////////////////////////////////////////////////////

UINT32 u_str_to_uint32(const uni_char* str)
{
	uni_char* number_start = const_cast<uni_char*>(str);
	int str_length = u_strlen(str);
	uni_char* number_end = number_start;
	for(int i = 0; i < str_length; ++i)
	{
		if(number_start[i] < '0' || number_start[i] > '9')
			break;
		++number_end;
	}
	UINT32 result = 0;
	int base = 1;
	for(uni_char* digit = number_end - 1; digit >= number_start; --digit)
	{
		result += (*digit - UNI_L('0')) * base;
		base *= 10;
	}
	return result;
}

/////////////////////////////////////////////////////////////////////////

OP_STATUS u_uint32_to_str(uni_char* str, size_t count, int number)
{
	uni_char tmp_buff[12];
	uni_char* ptr = tmp_buff;

	--count; // save one char space for \0

	do
	{
		*ptr = number % 10 + UNI_L('0');
		number -= number % 10;
		if(number > 9)
			++ptr;
	} while(number /= 10);

	*(ptr + 1) = UNI_L('\0');

	size_t chars_copied = 0;
	for(int i = u_strlen(tmp_buff) - 1; i >= 0; --i, ++chars_copied)
	{
		if(chars_copied > count)
		{
			--chars_copied;
			break;
		}
		str[chars_copied] = tmp_buff[i];
	}
	str[chars_copied] = UNI_L('\0');

	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////
