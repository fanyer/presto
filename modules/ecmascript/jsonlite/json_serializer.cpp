/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef ES_JSON_LITE

#include "modules/ecmascript/json_serializer.h"

JSONSerializer::JSONSerializer(TempBuffer &out)
	: buffer(out)
	, add_comma(FALSE)
{}

/* virtual */ OP_STATUS
JSONSerializer::EnterArray()
{
	RETURN_IF_ERROR(AddComma());
	RETURN_IF_ERROR(buffer.Append(UNI_L("[")));
	add_comma = FALSE;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
JSONSerializer::LeaveArray()
{
	RETURN_IF_ERROR(buffer.Append(UNI_L("]")));
	add_comma = TRUE;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
JSONSerializer::EnterObject()
{
	RETURN_IF_ERROR(AddComma());
	RETURN_IF_ERROR(buffer.Append(UNI_L("{")));
	add_comma = FALSE;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
JSONSerializer::LeaveObject()
{
	RETURN_IF_ERROR(buffer.Append(UNI_L("}")));
	add_comma = TRUE;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
JSONSerializer::AttributeName(const OpString& str)
{
	RETURN_IF_ERROR(String(str));
	RETURN_IF_ERROR(buffer.Append(UNI_L(":")));
	add_comma = FALSE;
	return OpStatus::OK;
}

OP_STATUS
JSONSerializer::AttributeName(const uni_char *str)
{
	OpString opstr;
	RETURN_IF_ERROR(opstr.Set(str));
	return AttributeName(opstr);
}

OP_STATUS
JSONSerializer::AddComma() // if needed
{
	if (add_comma)
		RETURN_IF_ERROR(buffer.Append(UNI_L(",")));
	else
		add_comma = TRUE;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
JSONSerializer::String(const OpString& str)
{
	RETURN_IF_ERROR(AddComma());
	RETURN_IF_ERROR(buffer.Append(UNI_L("\"")));

	BOOL escape = FALSE;
	if (str.CStr())
		for (const uni_char *s = str.CStr(); *s; s++)
			if (*s < 0x23 || *s == '\\' || *s == 0x7f)
			{
				escape = TRUE;
				break;
			}

	if (!escape)
		RETURN_IF_ERROR(buffer.Append(str.CStr()));
	else
	{
		// Keep this code in line with JSON_Appender::AppendStringEscaped().
		const uni_char* s = str.CStr();
		int str_len = str.Length();
		uni_char subst_buff[8]; // ARRAY OK 2012-01-04 tjamroszczak
		const uni_char* subst;
		unsigned int subst_len;
		for (const uni_char* ends = s + str_len; s < ends; ++s)
		{
			switch(*s)
			{
				case '\b': subst = UNI_L("\\b");  subst_len = 2; break;
				case '\t': subst = UNI_L("\\t");  subst_len = 2; break;
				case '\n': subst = UNI_L("\\n");  subst_len = 2; break;
				case '\f': subst = UNI_L("\\f");  subst_len = 2; break;
				case '\r': subst = UNI_L("\\r");  subst_len = 2; break;
				case '\\': subst = UNI_L("\\\\"); subst_len = 2; break;
				case '"' : subst = UNI_L("\\\""); subst_len = 2; break;
				default:
					if (*s < 0x20)
					{
				case '\x7f':
						uni_sprintf(subst_buff, UNI_L("\\u%04x"), (unsigned)*s);
						subst = subst_buff;
						subst_len = 6;
					}
					else
					{
						RETURN_IF_ERROR(buffer.Append(*s));
						continue;
					}
			}
			RETURN_IF_ERROR(buffer.Append(subst, subst_len));
		}
	}

	RETURN_IF_ERROR(buffer.Append(UNI_L("\"")));
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
JSONSerializer::PlainString(const OpString& str)
{
	return PlainString(str.CStr());
}

OP_STATUS
JSONSerializer::String(const uni_char *str)
{
	OpString opstr;
	RETURN_IF_ERROR(opstr.Set(str));
	return String(opstr);
}

OP_STATUS
JSONSerializer::PlainString(const uni_char *str)
{
	return buffer.Append(str);
}

/* virtual */ OP_STATUS
JSONSerializer::Number(double num)
{
	if (op_isintegral(num))
	{
		if (num >= static_cast<double>(INT_MIN) && num <= static_cast<double>(INT_MAX))
			return Int(static_cast<int>(num));
		else if (num >= 0 && num <= static_cast<double>(UINT_MAX))
			return UnsignedInt(static_cast<unsigned int>(num));
	}

	RETURN_IF_ERROR(AddComma());
	RETURN_IF_ERROR(buffer.AppendDouble(num));
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
JSONSerializer::Int(int num)
{
	RETURN_IF_ERROR(AddComma());
	RETURN_IF_ERROR(buffer.AppendLong(num));
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
JSONSerializer::UnsignedInt(unsigned int num)
{
	RETURN_IF_ERROR(AddComma());
	RETURN_IF_ERROR(buffer.AppendUnsignedLong(num));
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
JSONSerializer::Bool(BOOL val)
{
	RETURN_IF_ERROR(AddComma());
	return buffer.Append(val ? UNI_L("true") : UNI_L("false"));
}

/* virtual */ OP_STATUS
JSONSerializer::Null()
{
	RETURN_IF_ERROR(AddComma());
	return buffer.Append(UNI_L("null"));
}

#endif // ES_JSON_LITE
