/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/desktop_util/mail/mailto.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/formats/uri_escape.h"

namespace MailtoConstants
{
	const char * const AttributeStrings[] =
	{
		/* MAILTO_TO      */ "to",
		/* MAILTO_CC      */ "cc",
		/* MAILTO_BCC     */ "bcc",
		/* MAILTO_SUBJECT */ "subject",
		/* MAILTO_BODY    */ "body"
	};
};

/***********************************************************************************
 **
 **
 ** MailTo::Init
 ***********************************************************************************/
OP_STATUS MailTo::Init(const char * url)
{
	RETURN_IF_ERROR(m_raw_mailto.Set(url));
	return RawToAttributes();
}

/***********************************************************************************
 **
 **
 ** MailTo::Init
 ***********************************************************************************/
OP_STATUS MailTo::Init(const OpStringC& to, const OpStringC& cc, const OpStringC& bcc, const OpStringC& subject, const OpStringC& body)
{
	RETURN_IF_ERROR(m_values[MAILTO_TO].Set(to));
	RETURN_IF_ERROR(m_values[MAILTO_CC].Set(cc));
	RETURN_IF_ERROR(m_values[MAILTO_BCC].Set(bcc));
	RETURN_IF_ERROR(m_values[MAILTO_SUBJECT].Set(subject));
	RETURN_IF_ERROR(m_values[MAILTO_BODY].Set(body));

	return AttributesToRaw();
}


/***********************************************************************************
 **
 **
 ** MailTo::RawToAttributes
 ***********************************************************************************/
OP_STATUS MailTo::RawToAttributes()
{
	if (m_raw_mailto.Compare("mailto:", 7))
		return OpStatus::ERR;

	// We are parsing a mailto: address. It's supposed to look like this:
	// mailto:[tovalue][?attribute=value(&attribute=value)*]

	// parse tovalue
	const char* value     = m_raw_mailto.CStr() + 7;
	const char* next_attr = op_strchr(m_raw_mailto.CStr(), '?'); // First attribute starts with '?'

	DecodeValue(value, next_attr, m_values[MAILTO_TO]);

	// parse attribute=value pairs
	while (next_attr)
	{
		const char* attr = next_attr + 1;

		// Get next attribute: all remaining attributes start with '&'
		next_attr = op_strchr(attr, '&');

		// Get value for attribute
		const char* value = op_strchr(attr, '=');
		if (!value++)
			continue;

		// Compare to known attributes
		for (unsigned i = 0; i < MAILTO_ATTR_COUNT; i++)
		{
			if (!op_strnicmp(MailtoConstants::AttributeStrings[i], attr, (value - 1) - attr))
			{
				// for all headers except Subject we concatenate the values
				if (i != MAILTO_SUBJECT && m_values[i].HasContent())
				{
					OpString decoded;
					RETURN_IF_ERROR(DecodeValue(value, next_attr, decoded));
					// don't concatenate if it's empty
					if (*value)
					{
						if (i == MAILTO_BODY)
							RETURN_IF_ERROR(m_values[i].Append("\r\n"));
						else
							RETURN_IF_ERROR(m_values[i].Append(","));

						RETURN_IF_ERROR(m_values[i].Append(decoded));
					}
				}
				else
				{
					RETURN_IF_ERROR(DecodeValue(value, next_attr, m_values[i]));
				}

				break;
			}
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** MailTo::AttributesToUrl
 ***********************************************************************************/
OP_STATUS MailTo::AttributesToRaw()
{
	OpString8 url;
	RETURN_IF_ERROR(url.Set("mailto:"));

	// First attribute will be prefixed with '?'
	char attr_prefix = '?';

	for (unsigned i = 0; i < MAILTO_ATTR_COUNT; i++)
	{
		if (m_values[i].IsEmpty())
			continue;

		OpString8 encoded;
		RETURN_IF_ERROR(EncodeValue(m_values[i], encoded));

		if (i == MAILTO_TO)
		{
			// Special case: to is without an attribute name
			RETURN_IF_ERROR(url.Append(encoded));
		}
		else
		{
			// Attribute name and value
			RETURN_IF_ERROR(url.AppendFormat("%c%s=%s", attr_prefix,
								MailtoConstants::AttributeStrings[i], encoded.CStr()));
			// All following attributes will be prefixed with '&'
			attr_prefix = '&';
		}
	}

	return m_raw_mailto.Set(url.CStr());
}


/***********************************************************************************
 **
 **
 ** MailTo::EncodeMailtoValue
 ***********************************************************************************/
OP_STATUS MailTo::EncodeValue(const OpStringC& value, OpString8& encoded) const
{
	if (value.IsEmpty())
	{
		encoded.Empty();
		return OpStatus::OK;
	}

	// Convert to UTF-8
	OpString8 utf8_value;
	RETURN_IF_ERROR(utf8_value.SetUTF8FromUTF16(value.CStr()));

	// Escape value
	OpString8 uri_escaped;
	RETURN_IF_ERROR(StringUtils::EscapeURIAttributeValue(utf8_value, uri_escaped));
	
	if(!encoded.Reserve(uri_escaped.Length()*3+1))  //Worst case
		return OpStatus::ERR_NO_MEMORY;

	UriEscape::Escape(encoded.CStr(), uri_escaped.CStr(), UriEscape::StandardUnsafe);

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** MailTo::DecodeValue
 ***********************************************************************************/
OP_STATUS MailTo::DecodeValue(const char* value, const char* value_end, OpString& decoded) const
{
	int len = value_end ? value_end - value : op_strlen(value);
	RETURN_IF_ERROR(decoded.Set(value, len));

	// Decode (special chars and UTF-8)
	UriUnescape::ReplaceChars(decoded.CStr(), len, UriUnescape::AllUtf8 | UriUnescape::ExceptNbsp);
	decoded.CStr()[len] = 0;

	return OpStatus::OK;
}
