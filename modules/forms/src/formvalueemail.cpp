/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/forms/formvalueemail.h"

#include "modules/forms/piforms.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/idna/idna.h"
#include "modules/unicode/unicode.h"

/* virtual */
FormValue* FormValueEmail::Clone(HTML_Element *he)
{
	FormValueText* clone = OP_NEW(FormValueEmail, ());
	if (clone)
	{
		CopyToClone(he, clone);
	}
	return clone;
}

/* static */
OP_STATUS FormValueEmail::ConvertToIDNA(uni_char *source, uni_char *target, size_t targetbufsize, BOOL is_mailaddress)
{
	TRAPD(status, IDNA::ConvertToIDNA_L(source, target, targetbufsize, is_mailaddress));
	return status;
}

/* static */
OP_STATUS FormValueEmail::ConvertFromIDNA(const char* source, uni_char *target, size_t targetbufsize, BOOL& is_mailaddress)
{
	TRAPD(status, IDNA::ConvertFromIDNA_L(source, target, targetbufsize, is_mailaddress));
	return status;
}

/* static */
OP_STATUS FormValueEmail::ConvertMailAddressesIDNA(const uni_char* in, TempBuffer* result, BOOL from_idna)
{
	const uni_char* p = in;
	while (*p)
	{
		const uni_char* at = uni_strchr(p, '@');
		if (at)
		{
			RETURN_IF_ERROR(result->Append(p, at - p + 1));
			p = at + 1;
			const uni_char* end = p;
			while (Unicode::IsAlphaOrDigit(*end) || *end == '.' || *end == '-')
			{
				end++;
			}
			if (end != p)
			{
				OP_STATUS status;
				OpString temp_storage;
				if (from_idna)
				{
					OpString8 idna_email;
					RETURN_IF_ERROR(idna_email.SetUTF8FromUTF16(p, end - p));
					int temp_storage_len = end - p + 1;
					if (!temp_storage.Reserve(temp_storage_len))
					{
						return OpStatus::ERR_NO_MEMORY;
					}
					BOOL is_mailaddress = FALSE;
					status = ConvertFromIDNA(idna_email.CStr(), temp_storage.CStr(), temp_storage_len, is_mailaddress);
				}
				else
				{
					OpString temp_source;
					RETURN_IF_ERROR(temp_source.Set(p, end - p));
					uni_char* source = temp_source.CStr();
					// The expansion in unicode -> idna is unknown, but assume it's huge to not hit the limit for anything realistic.
					int temp_storage_len = uni_strlen(source) * 15 + 1;
					if (!temp_storage.Reserve(temp_storage_len))
					{
						return OpStatus::ERR_NO_MEMORY;
					}
					status = ConvertToIDNA(source, temp_storage.CStr(), temp_storage_len, FALSE);
				}
				RETURN_IF_ERROR(status);
				RETURN_IF_ERROR(result->Append(temp_storage.CStr()));
				p = end;
			}
			const uni_char* next = p;
			while (Unicode::IsSpace(*next) || *next == ',')
			{
				next++;
			}
			if (next != p)
			{
				RETURN_IF_ERROR(result->Append(p, next - p));
			}
			p = next;
		}
		else
		{
			RETURN_IF_ERROR(result->Append(p));
			break;
		}
	}

	return OpStatus::OK;
}

/* virtual */
OP_STATUS FormValueEmail::ConvertRealValueToDisplayValue(const uni_char* real_value,
														 OpString& display_value) const
{
	if (real_value && uni_strstr(real_value, "xn--"))
	{
		// Puny-code
		TempBuffer result;
		RETURN_IF_ERROR(ConvertMailAddressesIDNA(real_value, &result, TRUE));
		return display_value.Set(result.GetStorage(), result.Length());
	}
	return OpStatus::OK;
}

/* virtual */
OP_STATUS FormValueEmail::ConvertDisplayValueToRealValue(OpString& value) const
{
	if (!value.IsEmpty())
	{
		const uni_char* p = value.CStr();
		while (*p >= '0' && *p <= '9' ||
			   *p >= 'a' && *p <= 'z' ||
			   *p >= 'A' && *p <= 'Z' ||
			   *p == '@' || *p == '.' ||
			   *p == ' ' || *p == ',')
		{
			p++;
		}
		if (*p)
		{
			// At least one strange letter. Convert the domains using IDNA and leave the rest.
			TempBuffer result;
			RETURN_IF_ERROR(ConvertMailAddressesIDNA(value.CStr(), &result, FALSE));
			return value.Set(result.GetStorage(), result.Length());
		}
	}
	return OpStatus::OK;
}

/*static*/
OP_STATUS FormValueEmail::ConstructFormValueEmail(HTML_Element* he,
												  HLDocProfile* hld_profile,
												  FormValue*& out_value)
{
	FormValueEmail* email_value = OP_NEW(FormValueEmail, ());
	if (!email_value)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	const uni_char* initial_value = he->GetStringAttr(ATTR_VALUE);
	if (initial_value)
	{
		if (OpStatus::IsError(email_value->m_text.Set(initial_value)))
		{
			OP_DELETE(email_value);
			return OpStatus::ERR_NO_MEMORY;
		}
		FilterChars(email_value->m_text);

		// Make a roundtrip around the display value to get the same behaviour regardless
		// of whether we have a widget or not.
		OpString roundtrip_value;
		OpStatus::Ignore(email_value->ConvertRealValueToDisplayValue(email_value->m_text.CStr(), roundtrip_value));
		if (roundtrip_value.CStr())
		{
			OpStatus::Ignore(email_value->ConvertDisplayValueToRealValue(roundtrip_value));
			OpStatus::Ignore(email_value->m_text.Set(roundtrip_value));
		}
	}

	out_value = email_value;
	return OpStatus::OK;
}
