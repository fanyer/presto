/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef HAS_SET_HTTP_DATA
#include "modules/upload/upload.h"
#include "modules/hardcore/mem/mem_man.h"

#include "modules/olddebug/tstdump.h"
#include "modules/formats/uri_escape.h"
#include "modules/encodings/utility/opstring-encodings.h"

#ifdef URL_UPLOAD_RFC_2231_SUPPORT

Header_RFC2231_Parameter::Header_RFC2231_Parameter()
 : Header_Parameter_Collection(SEPARATOR_SEMICOLON_NEWLINE)
{
}

Header_RFC2231_Parameter::RFC2231_SpecialCharacter Header_RFC2231_Parameter::IsRFC2231Special(unsigned char token)
{
	if(token < 0x20 || token >= 0x80)
		return RFC2231_ESCAPABLE;
	
	switch(token)
	{
	case ' ':
	case '*':
	case '\'':
	case '%':
	case '(':
	case ')':
	case '<':
	case '>':
	case '@':
	case ',':
	case ';':
	case ':':
	case '\\':
	case '\"':
	case '/':
	case '[':
	case ']':
	case '?':
	case '=':
		return RFC2231_QUOTABLE;
	}
	
	return RFC2231_NOT_SPECIAL;
}

void Header_RFC2231_Parameter::CheckRFC2231Special(const OpStringC8 &p_value, RFC2231_SpecialCharacter &specials_type, OpStringC8 &charset)
{
	specials_type = RFC2231_NOT_SPECIAL;
	const unsigned char *pos = (const unsigned char *) p_value.CStr();
	unsigned char token;

	while ((token = *(pos++)) != 0)
	{
		RFC2231_SpecialCharacter tmp = IsRFC2231Special(token);
		if (tmp > specials_type)
		{
			specials_type = tmp;
			if (specials_type == RFC2231_ESCAPABLE)
				break; // No need to go on
		}
	}

	if (specials_type == RFC2231_NOT_SPECIAL || specials_type == RFC2231_QUOTABLE)
	{
		if (charset.CompareI("US-ASCII") == 0 || charset.CompareI("UTF-8") == 0)
			charset = NULL;
	}
}

void Header_RFC2231_Parameter::InitL(const OpStringC8 &p_name, const OpStringC8 &p_value, const OpStringC8 &charset1)
{
	RFC2231_SpecialCharacter specials_type;
	const unsigned char *pos;
	OpStringC8 charset(charset1);

	Clear(); // Clear existing parameters
	CheckRFC2231Special(p_value, specials_type, charset);

	int val_len = p_value.Length();
	BOOL convert = (specials_type != RFC2231_NOT_SPECIAL || !charset.IsEmpty() || val_len > 60);

	if(convert)
	{
		char *temp_name = (char *) g_memory_manager->GetTempBuf();
		char *temp_value = (char *) g_memory_manager->GetTempBuf2();

		if((unsigned int) p_name.Length()+20 > g_memory_manager->GetTempBufLen()) LEAVE(OpStatus::ERR_OUT_OF_RANGE);

		if((specials_type == RFC2231_QUOTABLE || specials_type == RFC2231_NOT_SPECIAL) && charset.IsEmpty())
		{
			if(val_len <= 60)
			{
				AddParameterL(p_name, p_value, TRUE);
			}
			else
			{
				int item = 0;
				int len = val_len;
				
				pos = (const unsigned char*) p_value.CStr();
				while(len > 0)
				{
					op_snprintf(temp_name, g_memory_manager->GetTempBufLen(), "%s*%d", p_name.CStr(), item);
					item++;
					
					int copy_len = 40;
					
					if(copy_len > len || len <60)
						copy_len = len;
					
					op_strlcpy(temp_value, (const char *) pos, copy_len+1);
					
					len -= copy_len;
					pos += copy_len;
					
					AddParameterL(temp_name, temp_value, TRUE);
				}
			}
		}
		else
		{
			int item = 0;
			int len = val_len;
			int copy_len;
			int consumed;

			if((unsigned int) charset.Length()+20 > g_memory_manager->GetTempBuf2Len()) LEAVE(OpStatus::ERR_OUT_OF_RANGE);

			pos = (const unsigned char*) p_value.CStr();
			while(len > 0)
			{
				copy_len = 0;

				if(item == 0)
				{
					copy_len += op_snprintf(temp_value, g_memory_manager->GetTempBuf2Len(), "%s''", charset.CStr());
					temp_value[g_memory_manager->GetTempBuf2Len()-1] = 0;
				}

				copy_len += UriEscape::Escape(temp_value+copy_len, 40-copy_len, (const char*)pos, len, UriEscape::RFC2231Unsafe, &consumed);
				len -= consumed;
				pos += consumed;
				temp_value[copy_len] = '\0';

				// if the value fits in a single argument, skip the item part
				op_snprintf(temp_name, g_memory_manager->GetTempBufLen(), (item == 0 && len == 0 ? "%s*" : "%s*%d*"), p_name.CStr(), item);
				item++;
				
				AddParameterL(temp_name, temp_value);
			}
		}
	}
	else
		AddParameterL(p_name, p_value);
}

void Header_RFC2231_Parameter::InitL(const OpStringC8 &p_name, const OpStringC16 &p_value, const OpStringC8 &charset)
{
	OpString8 p_value2;
	ANCHOR(OpString8, p_value2);

	SetToEncodingL(&p_value2, (charset.HasContent() && charset.CompareI("utf-16") != 0 ? charset.CStr() : "utf-8"), p_value.CStr());
	InitL(p_name, p_value2, (charset.HasContent() && charset.CompareI("utf-16") != 0 ? charset.CStr() : "utf-8"));
}
#endif // URL_UPLOAD_RFC_2231_SUPPORT

#endif // HTTP_data
