/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/util/OpLineParser.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

OP_STATUS OpLineParser::GetNextToken(OpString& token)
{
	FindFirstCharacter();
	const uni_char* start = m_current_pos;
	return token.Set(start, FindLastCharacter(','));
}

OP_STATUS OpLineParser::GetNextToken8(OpString8& token8)
{
	OpString token;
	RETURN_IF_ERROR(GetNextToken(token));
	return token8.Set(token.CStr());
}

OP_STATUS OpLineParser::GetNextTokenWithoutTrailingDigits(OpString8& token)
{
	RETURN_IF_ERROR(GetNextToken8(token));

	if (token.HasContent())
	{
		char *last = token.CStr() + token.Length() - 1;
		while (last >= token.CStr() && op_isdigit(*last))
			*last-- = 0;
	}

	return OpStatus::OK;
}

OP_STATUS OpLineParser::GetNextString(OpString& string)
{
	FindFirstCharacter();
	if (*m_current_pos == '"')
	{
		m_current_pos++;
		const uni_char* start = m_current_pos;
		RETURN_IF_ERROR(string.Set(start, FindLastCharacter('"', FALSE)));
		FindLastCharacter(',');
		return OpStatus::OK;
	}
	else
	{
		return GetNextToken(string);
	}
}

OP_STATUS OpLineParser::GetNextLanguageString(OpString& language_string, Str::LocaleString* id)
{
	FindFirstCharacter();

	INT32 value = 0;

	if (*m_current_pos == '"' || (!uni_isdigit(*m_current_pos) && *m_current_pos != '-'))
	{
#ifdef LOCALE_SET_FROM_STRING
		BOOL define = FALSE;

		if (*m_current_pos != '"')
			define = TRUE;
#endif // LOCALE_SET_FROM_STRING

		RETURN_IF_ERROR(GetNextString(language_string));

#ifdef LOCALE_SET_FROM_STRING
		OpString loaded_string;

		Str::LocaleString locale_string(language_string.CStr());

		// Probably a define so just try to load it straight out
		if (define)
		{
			RETURN_IF_ERROR(g_languageManager->GetString(locale_string, loaded_string));
		}

		// If this didn't load which could happen when the OpLineParser is used for plain strings
		// instead of lines to parse, which can happen!! Then just assume it's a string
		if (loaded_string.IsEmpty())
#endif // LOCALE_SET_FROM_STRING
		{
			uni_char* string = language_string.CStr();
			int len = language_string.Length() + 1;

			// Converts \n to a real new line
			int new_len = 0;
			for (int i=0; i<len; i++, new_len++)
			{
				if (string[i] == '\\' && i+1 < len)
				{
					if(string[i+1] == 'n')
					{
						string[new_len] = 0xd;
						i ++;
					}
					else
						string[new_len] = '\\';
				}
				else if (new_len != i)
					string[new_len] = string[i];
			}
		}
#ifdef LOCALE_SET_FROM_STRING
		else
		{
			RETURN_IF_ERROR(language_string.Set(loaded_string.CStr()));
			value = locale_string;
		}
#endif // LOCALE_SET_FROM_STRING
	}
	else
	{
		RETURN_IF_ERROR(GetNextValue(value));
		RETURN_IF_ERROR(g_languageManager->GetString((Str::LocaleString) value, language_string));
	}

	if (id)
	{
		*id = (Str::LocaleString) value;
		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

OP_STATUS OpLineParser::GetNextStringOrValue(OpString& string, INT32& value)
{
	FindFirstCharacter();

	if (*m_current_pos == '"')
	{
		return GetNextString(string);
	}
	else
	{
		return GetNextValue(value);
	}
}

OP_STATUS OpLineParser::GetNextValue(INT32& value)
{
	FindFirstCharacter();
	if (*m_current_pos == '#')
	{
		OpStringC color = m_current_pos;

		INT32 r, g, b;

		if (color.Length() >= 7 && 3 == uni_sscanf(color.CStr(), UNI_L("#%2x%2x%2x"), &r, &g, &b))
		{
			value = OP_RGB(r, g, b);
		}
		else
		{
			value = 0;
		}
	}
	else if (*m_current_pos)
	{
		value = uni_atoi(m_current_pos);
	}
	FindLastCharacter(',');
	return OpStatus::OK;
}

void OpLineParser::FindFirstCharacter()
{
	while (uni_isspace(*m_current_pos))
	{
		m_current_pos++;
	}
}

INT32 OpLineParser::FindLastCharacter(uni_char end_character, BOOL trim_space)
{
	const uni_char* start = m_current_pos;

	BOOL within_quotes = FALSE;

	while (*m_current_pos)
	{
		if (end_character != '"' && *m_current_pos == '"')
		{
			within_quotes = !within_quotes;
		}
		else if (!within_quotes && *m_current_pos == end_character)
		{
			break;
		}

		m_current_pos++;
	}

	INT32 len = m_current_pos - start;

	if (trim_space)
	{
		while (len > 0 && uni_isspace(start[len - 1]))
			len--;
	}

	if (*m_current_pos)
	{
		m_current_pos++;
	}

	return len;
}

/***********************************************************************************
**
**	OpLineString
**
***********************************************************************************/

OP_STATUS OpLineString::WriteToken8(const char* token8)
{
	OpString token;
	RETURN_IF_ERROR(token.Set(token8));
	return WriteToken(token.CStr());
}

OP_STATUS OpLineString::WriteToken(const uni_char* token)
{
	OP_STATUS err = OpStatus::OK;
	if (token && *token)
	{
		AppendNeededCommas();
		err = m_string.Append(token);
	}

	m_commas_needed++;
	return err;
}

OP_STATUS OpLineString::WriteTokenWithTrailingDigits(const char* token, INT32 value)
{
	OP_STATUS err = OpStatus::OK;
	if (token && *token)
	{
		AppendNeededCommas();
		m_string.Append(token);
		uni_char buffer[32]; /* ARRAY OK 2009-04-03 adame */
		uni_itoa(value, buffer, 10);
		err = m_string.Append(buffer);
	}

	m_commas_needed++;
	return err;
}

OP_STATUS OpLineString::WriteString(const uni_char* string)
{
	if (string && *string)
	{
		AppendNeededCommas();
		RETURN_IF_ERROR(m_string.AppendFormat(UNI_L("\"%s\""), string));
	}

	m_commas_needed++;
	return OpStatus::OK;
}

OP_STATUS OpLineString::WriteString8(const char* string8)
{
	OpString string;
	RETURN_IF_ERROR(string.Set(string8));
	return WriteString(string.CStr());
}

OP_STATUS OpLineString::WriteValue(INT32 value, BOOL treat_zero_as_empty)
{
	if (value || !treat_zero_as_empty)
	{
		AppendNeededCommas();
		uni_char buffer[32]; /* ARRAY OK 2009-04-03 adame */
		uni_itoa(value, buffer, 10);
		RETURN_IF_ERROR(m_string.Append(buffer));
	}

	m_commas_needed++;
	return OpStatus::OK;
}

OP_STATUS OpLineString::WriteSeparator(const char separator)
{
	m_commas_needed = 0;
	return m_string.AppendFormat(UNI_L(" %c "), separator);
}

OP_STATUS OpLineString::AppendNeededCommas()
{
	while (m_commas_needed > 0)
	{
		RETURN_IF_ERROR(m_string.Append(UNI_L(", ")));
		m_commas_needed--;
	}
	return OpStatus::OK;
}
