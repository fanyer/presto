/* Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include <ctype.h>

#include "header.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/util/misc.h"
#include "adjunct/m2/src/util/qp.h"
#include "modules/locale/oplanguagemanager.h"

#if defined(WIN32) && !defined(WIN_CE)
#include <sys/timeb.h>
#endif

#define DEFAULT_CHARSET_STR "iso-8859-1"

// #define ASSERT_ON_EMPTY_ADDRESS

static struct
{
	const uni_char* tz;
	char offset;		// Offset from UTC (GMT)
}
TimeZones[] =
{
	// European

	{UNI_L("UTC"),	0},	// Universal Standard Time
	{UNI_L("GMT"),	0}, 	// Greenwich Mean Time, as UTC
	{UNI_L("BST"),	1}, 	// British Summer Time, as UTC+1 hour
	{UNI_L("IST"),	1}, 	// Irish Summer Time, as UTC+1 hour
	{UNI_L("WET"),	0}, 	// Western Europe Time, as UTC
	{UNI_L("WEST"),1},	// Western Europe Summer Time, as UTC+1 hour
	{UNI_L("CET"),	1}, 	// Central Europe Time, as UTC+1
	{UNI_L("CEST"),2}, 	// Central Europe Summer Time, as UTC+2
	{UNI_L("EET"),	2}, 	// Eastern Europe Time, as UTC+2
	{UNI_L("EEST"),3},	// Eastern Europe Summer Time, as UTC+3
	{UNI_L("MSK"),	3}, 	// Moscow Time, as UTC+3
	{UNI_L("MSD"),	4}, 	// Moscow Summer Time, as UTC+4

	// US and Canada

	{UNI_L("AST"),	-4},		// Atlantic Standard Time, as UTC-4 hours
	{UNI_L("ADT"),	-3},		// Atlantic Daylight Time, as UTC-3 hours
	{UNI_L("EST"),	-5},		// Eastern Standard Time, as UTC-5 hours
	{UNI_L("EDT"),	-4},		// Eastern Daylight Saving Time, as UTC-4 hours
	{UNI_L("CST"),	-6},		// Central Standard Time, as UTC-6 hours
	{UNI_L("CDT"),	-5},		// Central Daylight Saving Time, as UTC-5 hours
	{UNI_L("MST"),	-7},		// Mountain Standard Time, as UTC-7 hours
	{UNI_L("MDT"),	-6},		// Mountain Daylight Saving Time, as UTC-6 hours
	{UNI_L("PST"),	-8},		// Pacific Standard Time, as UTC-8 hours
	{UNI_L("PDT"),	-7},		// Pacific Daylight Saving Time, as UTC-7 hours
	{UNI_L("HST"),	-10},	// Hawaiian Standard Time, as UTC-10 hours
	{UNI_L("HDT"),	-9},		// Hawaiian Daylight Saving Time, as UTC-9 hours
	{UNI_L("AKST"),-9},		// Alaska Standard Time, as UTC-9 hours
	{UNI_L("AKDT"),-8},		// Alaska Standard Daylight Saving Time, as UTC-8 hours

	// Australia
//	{UNI_L("WST"),	8},		// Western Standard Time, as UTC+8 hours
//	{UNI_L("CST"),	9.5},	// Central Standard Time, as UTC+9.5 hours
//	{UNI_L("EST"),	10},		// Eastern Standard/Summer Time, as UTC+10 hours (+11 hours during summer time)

	// Timezone letter abbrevations
	{UNI_L("Y"),	-12},
	{UNI_L("X"),	-11},
	{UNI_L("W"),	-10},
	{UNI_L("V"),	-9},
	{UNI_L("U"),	-8},
	{UNI_L("T"),	-7},
	{UNI_L("S"),	-6},
	{UNI_L("R"),	-5},
	{UNI_L("Q"),	-4},
	{UNI_L("P"),	-3},
	{UNI_L("O"),	-2},
	{UNI_L("N"),	-1},
	{UNI_L("Z"),	0},
	{UNI_L("A"),	1},
	{UNI_L("B"),	2},
	{UNI_L("C"),	3},
	{UNI_L("D"),	4},
	{UNI_L("E"),	5},
	{UNI_L("F"),	6},
	{UNI_L("G"),	7},
	{UNI_L("H"),	8},
	{UNI_L("I"),	9},
	{UNI_L("K"),	10},
	{UNI_L("L"),	11},
	{UNI_L("M"),	12},
};

static const uni_char* monthNames[12] = {UNI_L("Jan"), UNI_L("Feb"), UNI_L("Mar"), UNI_L("Apr"), UNI_L("May"), UNI_L("Jun"), UNI_L("Jul"), UNI_L("Aug"), UNI_L("Sep"), UNI_L("Oct"), UNI_L("Nov"), UNI_L("Dec")};

static const uni_char* dayNames[7] = {UNI_L("Sun"), UNI_L("Mon"), UNI_L("Tue"), UNI_L("Wed"), UNI_L("Thu"), UNI_L("Fri"), UNI_L("Sat")}; //Do not change these to uppercase!

OP_STATUS Header::HeaderValue::Set(const uni_char* value, int aLength)
{
	RETURN_IF_ERROR(m_value_buffer.Set(value, aLength));
	iBuffer = m_value_buffer.CStr();
	return OpStatus::OK; 
}

OP_STATUS Header::HeaderValue::Append(const uni_char* string)
{
	if (HasContent() && iBuffer != m_value_buffer.CStr())
		RETURN_IF_ERROR(m_value_buffer.Set(iBuffer));
	RETURN_IF_ERROR(m_value_buffer.Append(string));
	iBuffer = m_value_buffer.CStr();
	return OpStatus::OK;
}

Header::From::From(const Header::From& src) : Link()
{
	OP_STATUS ret = OpStatus::OK;

	if (OpStatus::IsSuccess(ret))
		ret = m_name.Set(src.m_name);

	if (OpStatus::IsSuccess(ret))
		ret = m_address_localpart.Set(src.m_address_localpart);

	if (OpStatus::IsSuccess(ret))
		ret = m_address_domain.Set(src.m_address_domain);

	if (OpStatus::IsSuccess(ret))
		ret = m_address.Set(src.m_address);

	if (OpStatus::IsSuccess(ret))
		ret = m_comment.Set(src.m_comment);

	if (OpStatus::IsSuccess(ret))
		ret = m_group.Set(src.m_group);

	if (OpStatus::IsError(ret))
	{
		m_name.Empty();
		m_address_localpart.Empty();
		m_address_domain.Empty();
		m_address.Empty();
		m_comment.Empty();
		m_group.Empty();
	}
}

Header::From& Header::From::operator=(const Header::From& src)
{
    OP_STATUS ret = OpStatus::OK;

	if (OpStatus::IsSuccess(ret))
		ret = m_name.Set(src.m_name);

	if (OpStatus::IsSuccess(ret))
		ret = m_address_localpart.Set(src.m_address_localpart);

	if (OpStatus::IsSuccess(ret))
		ret = m_address_domain.Set(src.m_address_domain);

	if (OpStatus::IsSuccess(ret))
		ret = m_address.Set(src.m_address);

	if (OpStatus::IsSuccess(ret))
		ret = m_comment.Set(src.m_comment);

	if (OpStatus::IsSuccess(ret))
		ret = m_group.Set(src.m_group);

	if (OpStatus::IsError(ret))
	{
		m_name.Empty();
		m_address_localpart.Empty();
		m_address_domain.Empty();
		m_address.Empty();
		m_comment.Empty();
		m_group.Empty();
	}

    return *this;
}

BOOL Header::From::NeedQuotes(const OpStringC16& name, BOOL quote_qp, BOOL dot_atom_legal)
{
	if (name.IsEmpty())
		return FALSE;

	// atext				= ALPHA / DIGIT / "!" / "#" / "$" / "%" / "&" / "'" / "*" / "+" /
	//					  "-" / "/" / "=" / "?" / "^" / "_" / "`" / "{" / "|" / "}" / "~"
	//
	// ASCII-ranges : 33, 35-39, 42-43, 45, 47-57, 61, 63, 65-90 , 94-126
	// !ASCII-ranges: 0-32, 34, 40-41, 44, 46, 58-60, 62, 64, 91-93, 127

	const uni_char* string = name.CStr();
	BOOL in_quote = FALSE;
	while (*string)
	{
		switch (*string)
		{
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 14:
			case 15:
			case 16:
			case 17:
			case 18:
			case 19:
			case 20:
			case 21:
			case 22:
			case 23:
			case 24:
			case 25:
			case 26:
			case 27:
			case 28:
			case 29:
			case 30:
			case 31:
			case 32:
			case 40:
			case 41:
			case 44:
			case 58:
			case 59:
			case 60:
			case 62:
			case 64:
			case 91:
			case 93:
			case 127:
				// These characters always need quotes
				if (!in_quote)
					return TRUE;
				break;
			case 13:
				// '\r' (CR), skip if this is folded whitespace, needs quote otherwise
				if (*(string + 1) == '\n' && (*(string + 2) == ' ' || *(string + 2) == '\t'))
					string += 2;
				else if(!in_quote)
					return TRUE;
				break;
			case 34:
				// '"' character, this is a quote
				in_quote = !in_quote;
				break;
			case 46:
				// '.' character
				if (!in_quote && !dot_atom_legal)
					return TRUE;
				break;
			case 61:
				// '=' character, check for QP lookalike
				if (quote_qp && !in_quote && *(string + 1) == '?')
					return TRUE;
				break;
			case 92:
				// '\' character, skip quoted-char
				string++;
				break;
			default:
				break;
		}

		string++;
	}
	return FALSE;
}

OP_STATUS Header::From::GetAddress(const OpStringC16& address_localpart, const OpStringC16& address_domain, OpString16& address)
{
	BOOL      quotes        = NeedQuotes(address_localpart, FALSE, TRUE);
	int       local_length  = address_localpart.Length();
	int       domain_length = address_domain.Length();
	int       total_length  = local_length + domain_length + (quotes ? 2 : 0) + (domain_length ? 1 : 0);
	uni_char* string        = address.Reserve(total_length);

	if (!string)
		return OpStatus::ERR_NO_MEMORY;

	// Copy the local part
	if (quotes)
		*string++ = 0x22; // '"'
	op_memcpy(string, address_localpart.CStr(), UNICODE_SIZE(local_length)); // NULL-terminated below
	string += local_length;
	if (quotes)
		*string++ = 0x22; // '"'

	// Copy the domain part
	if (domain_length)
	{
		*string++ = 0x40; // '@'
		op_memcpy(string, address_domain.CStr(), UNICODE_SIZE(domain_length)); // NULL-terminated below
		string += domain_length;
	}

	// NULL-terminate string
	*string = 0;

	return OpStatus::OK;
}

OP_STATUS Header::From::GetIMAAAddress(OpString8& imaa_address, BOOL* failed_imaa) const
{
	return ConvertToIMAAAddress(m_address_localpart, m_address_domain, imaa_address, failed_imaa);
}

OP_STATUS Header::From::SetAddress(const OpStringC16& address_localpart, const OpStringC16& address_domain)
{
	//We don't have true IMAA yet. Expect all localparts to be us-ascii
	if (address_localpart.HasContent())
	{
		const uni_char* address_localpart_ptr = address_localpart.CStr();
		while (address_localpart_ptr && *address_localpart_ptr)
		{
			if (*address_localpart_ptr<32 || *address_localpart_ptr>127)
				return OpStatus::ERR_PARSING_FAILED;;

			address_localpart_ptr++;
		}
	}

	OP_STATUS ret;
	if ((ret=m_address_localpart.Set(address_localpart))!=OpStatus::OK)
		return ret;

	if (ret==OpStatus::OK && m_address_localpart.HasContent())
		ret = OpMisc::StripQuotes(m_address_localpart);

	if (address_domain.HasContent() && uni_strni_eq(address_domain.CStr(), "XN--", 4))
	{
		OpString8 imaa_address_domain;
		OpString16 dummy_localpart;
		if ((ret=imaa_address_domain.Set(address_domain.CStr()))!=OpStatus::OK ||
			(ret=ConvertFromIMAAAddress(imaa_address_domain, dummy_localpart, m_address_domain))!=OpStatus::OK)
		{
			return ret;
		}
	}
	else
	{
		if ((ret=m_address_domain.Set(address_domain))!=OpStatus::OK)
			return ret;
	}

	return GetAddress(m_address_localpart, m_address_domain, m_address);
}

OP_STATUS Header::From::SetIMAAAddress(const OpStringC8& imaa_address)
{
	RETURN_IF_ERROR(ConvertFromIMAAAddress(imaa_address, m_address_localpart, m_address_domain));
	return GetAddress(m_address_localpart, m_address_domain, m_address);
}

int Header::From::CompareAddress(const OpStringC16& address) const //Not IMAA-safe!!!
{
	return m_address.Compare(address);
}

int Header::From::CompareAddress(const OpStringC16& address_localpart, const OpStringC16& address_domain) const
{
	OpString8 imaa_address_domain8;
	OpString16 imaa_address_domain16;
	if (address_domain.HasContent() && uni_strni_eq(address_domain.CStr(), "XN--", 4))
	{
		OpString16 dummy_localpart;
		OpStatus::Ignore(imaa_address_domain8.Set(address_domain.CStr()));
		OpStatus::Ignore(ConvertFromIMAAAddress(imaa_address_domain8, dummy_localpart, imaa_address_domain16));
	}

	int compare;
	if (address_localpart.HasContent() && *address_localpart.CStr()=='"')
	{
		OpString stripped_address_localpart;
		OpStatus::Ignore(stripped_address_localpart.Set(address_localpart));
		OpStatus::Ignore(OpMisc::StripQuotes(stripped_address_localpart));
		compare = m_address_localpart.Compare(stripped_address_localpart);
	}
	else
	{
		compare = m_address_localpart.Compare(address_localpart);
	}

	if (compare==0)
	{
		if (imaa_address_domain16.HasContent())
		{
			compare = m_address_domain.Compare(imaa_address_domain16);
		}
		else
		{
			compare = m_address_domain.Compare(address_domain);
		}
	}
	return compare;
}

OP_STATUS Header::From::ConvertFromIMAAAddress(const OpStringC8& imaa_address, OpString16& address_localpart, OpString16& address_domain)
{
	return OpMisc::ConvertFromIMAAAddress(imaa_address, address_localpart, address_domain);
}

OP_STATUS Header::From::ConvertToIMAAAddress(const OpStringC16& address_localpart, const OpStringC16& address_domain, OpString8& imaa_address, BOOL* failed_imaa)
{
	return OpMisc::ConvertToIMAAAddress(address_localpart, address_domain, imaa_address, failed_imaa);
}

OP_STATUS Header::From::GetFormattedEmail(const OpStringC16& name, const OpStringC16& address, const OpStringC16& comment, OpString16& result, BOOL do_quote_pair_encode, BOOL quote_qp, BOOL quote_name_atoms)
{
	OP_STATUS ret;
	OpString address_localpart;
	OpString address_domain;
	if ((ret=OpMisc::SplitMailAddress(address, address_localpart, address_domain)) != OpStatus::OK)
		return ret;

	return GetFormattedEmail(name, address_localpart, address_domain, comment, result, do_quote_pair_encode, quote_qp, quote_name_atoms);
}

OP_STATUS Header::From::GetFormattedEmail(const OpStringC16& name,
										  const OpStringC16& address_localpart,
										  const OpStringC16& address_domain,
										  const OpStringC16& comment,
										  OpString16& result,
										  BOOL do_quote_pair_encode,
										  BOOL quote_qp,
										  BOOL quote_name_atoms)
{
	OpString16         tmp_name, tmp_address, tmp_comment, quotechar_encoded_name;
	const OpStringC16* actual_name    = &name;
	const OpStringC16* actual_comment = &comment;

	// Do name encodings if necessary
	if (name.HasContent())
	{
		if (do_quote_pair_encode)
		{
			RETURN_IF_ERROR(quotechar_encoded_name.Set(name));
			RETURN_IF_ERROR(OpMisc::EncodeQuotePair(quotechar_encoded_name, TRUE));
			actual_name = &quotechar_encoded_name;
		}

		if (quote_name_atoms ? NeedQuotes(*actual_name, quote_qp) : FALSE)
		{
			// need to add quotes
			int       tmp_name_length = actual_name->Length();
			uni_char* name_ptr        = tmp_name.Reserve(tmp_name_length + 2);
			if (!name_ptr)
				return OpStatus::ERR_NO_MEMORY;

			*name_ptr++ = 0x22; // '"'
			op_memcpy(name_ptr, actual_name->CStr(), UNICODE_SIZE(tmp_name_length)); // NULL-terminated below
			name_ptr += tmp_name_length;
			*name_ptr++ = 0x22; // '"'
			*name_ptr = 0;

			actual_name = &tmp_name;
		}
	}

	// Generate address
	if (address_localpart.HasContent() || address_domain.HasContent())
		RETURN_IF_ERROR(GetAddress(address_localpart, address_domain, tmp_address));

	// Do comment encodings if necessary
	if (comment.HasContent())
	{
		OP_ASSERT(*comment.CStr()=='('); //comment is supposed to include the needed '('s and ')'s

		if (do_quote_pair_encode)
		{
			RETURN_IF_ERROR(tmp_comment.Set(comment));
			RETURN_IF_ERROR(OpMisc::EncodeQuotePair(tmp_comment));
			actual_comment = &tmp_comment;
		}
	}

	//Use angle-brackets if we have name or comment, or if localpart of address is quote-string
	BOOL need_brackets = (actual_name->HasContent() || actual_comment->HasContent() || (tmp_address.HasContent() && tmp_address[0] == '"'));

	// Merge the strings
	int       name_length    = actual_name->Length();
	int       address_length = tmp_address.Length();
	int       comment_length = actual_comment->Length();
	uni_char* result_ptr     = result.Reserve((name_length ? name_length + 1 : 0) +
												address_length + (need_brackets ? 2 : 0) +
												(comment_length ? comment_length + 1 : 0));

	if (!result_ptr)
		return OpStatus::ERR_NO_MEMORY;

	// Merge name
	if (name_length)
	{
		op_memcpy(result_ptr, actual_name->CStr(), UNICODE_SIZE(name_length)); // NULL-terminated below
		result_ptr += name_length;
		*result_ptr++ = 0x20; // ' '
	}

	// Merge address
	if (need_brackets)
		*result_ptr++ = 0x3c; // '<'

	op_memcpy(result_ptr, tmp_address.CStr(), UNICODE_SIZE(address_length)); // NULL-terminated below
	result_ptr += address_length;

	if (need_brackets)
		*result_ptr++ = 0x3e; // '>'

	// Merge comment
	if (comment_length)
	{
		*result_ptr++ = 0x20; // ' '
		op_memcpy(result_ptr, actual_comment->CStr(), UNICODE_SIZE(comment_length)); // NULL-terminated below
		result_ptr += comment_length;
	}

	// NULL-terminate string
	*result_ptr = 0;

	return OpStatus::OK;
}

OP_STATUS Header::From::GetFormattedEmail(OpString8& encoding, const OpStringC16& name, const OpStringC16& address, const OpStringC16& comment, BOOL allow_8bit, OpString8& result, BOOL do_quote_pair_encode, BOOL quote_name_atoms)
{
	OP_STATUS ret;
	OpString address_localpart;
	OpString address_domain;
	if ((ret=OpMisc::SplitMailAddress(address, address_localpart, address_domain)) != OpStatus::OK)
		return ret;

	return GetFormattedEmail(encoding, name, address_localpart, address_domain, comment, allow_8bit, result, do_quote_pair_encode, quote_name_atoms);
}

OP_STATUS Header::From::GetFormattedEmail(OpString8& encoding,
										  const OpStringC16& name,
										  const OpStringC16& address_localpart,
										  const OpStringC16& address_domain,
										  const OpStringC16& comment,
										  BOOL allow_8bit,
										  OpString8& result,
										  BOOL do_quote_pair_encode,
										  BOOL quote_name_atoms)
{
	// Encode name
	OpString16 quotechar_encoded_name;
	OpString16 qp_encoded_name;
	OpString8  qp_encoded_name8;

	RETURN_IF_ERROR(quotechar_encoded_name.Set(name));
	if (do_quote_pair_encode)
		RETURN_IF_ERROR(OpMisc::EncodeQuotePair(quotechar_encoded_name, TRUE));
	RETURN_IF_ERROR(OpQP::Encode(quotechar_encoded_name, qp_encoded_name8, encoding, allow_8bit));
	RETURN_IF_ERROR(qp_encoded_name.Set(qp_encoded_name8));



	//Encode address
	OpString16 imaa_encoded_address;
	OpString8 imaa_encoded_address8;
	BOOL imaa_failed = FALSE;
	if (NeedQuotes(address_localpart, FALSE, TRUE))
	{
		OpString16 quoted_address_localpart;
		RETURN_IF_ERROR(quoted_address_localpart.Set("\""));
		RETURN_IF_ERROR(quoted_address_localpart.Append(address_localpart));
		RETURN_IF_ERROR(quoted_address_localpart.Append("\""));
		RETURN_IF_ERROR(ConvertToIMAAAddress(quoted_address_localpart, address_domain, imaa_encoded_address8, &imaa_failed));
	}
	else
	{
		RETURN_IF_ERROR(ConvertToIMAAAddress(address_localpart, address_domain, imaa_encoded_address8, &imaa_failed));
	}
	if (imaa_failed)
		return OpStatus::ERR_PARSING_FAILED;

	RETURN_IF_ERROR(imaa_encoded_address.Set(imaa_encoded_address8));

	//Encode comment
	OpString16 quotechar_encoded_comment, qp_encoded_comment;
	OpString8 qp_encoded_comment8;

	RETURN_IF_ERROR(quotechar_encoded_comment.Set(comment));
	if (do_quote_pair_encode)
		RETURN_IF_ERROR(OpMisc::EncodeQuotePair(quotechar_encoded_comment, TRUE));

	if (quotechar_encoded_comment.HasContent())
	{
		OpString quotechar_encoded_comment_atom16;
		OpString8 quotechar_encoded_comment_atom8;
		const uni_char* quotechar_encoded_comment_atom_start = OpMisc::uni_rfc822_strchr(quotechar_encoded_comment.CStr(), '(');
		const uni_char* quotechar_encoded_comment_atom_end;
		while (quotechar_encoded_comment_atom_start && *quotechar_encoded_comment_atom_start)
		{
			quotechar_encoded_comment_atom_end = OpMisc::uni_rfc822_strchr(quotechar_encoded_comment_atom_start, ')');
			if (quotechar_encoded_comment_atom_end && quotechar_encoded_comment_atom_end>(quotechar_encoded_comment_atom_start+1))
			{
				RETURN_IF_ERROR(quotechar_encoded_comment_atom16.Set(quotechar_encoded_comment_atom_start+1, quotechar_encoded_comment_atom_end-quotechar_encoded_comment_atom_start-1));
				RETURN_IF_ERROR(OpQP::Encode(quotechar_encoded_comment_atom16, quotechar_encoded_comment_atom8, encoding, allow_8bit));
				if (qp_encoded_comment.HasContent())
					RETURN_IF_ERROR(qp_encoded_comment.Append(" "));
				RETURN_IF_ERROR(qp_encoded_comment.Append("("));
				RETURN_IF_ERROR(qp_encoded_comment.Append(quotechar_encoded_comment_atom8));
				RETURN_IF_ERROR(qp_encoded_comment.Append(")"));
			}
			quotechar_encoded_comment_atom_start = OpMisc::uni_rfc822_strchr(quotechar_encoded_comment_atom_end, '(');
		}
	}

	OpString16 result16;
	RETURN_IF_ERROR(GetFormattedEmail(qp_encoded_name, imaa_encoded_address, qp_encoded_comment, result16, FALSE, FALSE /*QP-codes are our*/, quote_name_atoms));

	return result.Set(result16.CStr());
}

OP_STATUS Header::From::GetFormattedEmail(OpString16& result, BOOL do_quote_pair_encode, BOOL quote_name_atoms) const
{
    return GetFormattedEmail(m_name, m_address_localpart, m_address_domain, m_comment, result, do_quote_pair_encode, TRUE, quote_name_atoms);
}

OP_STATUS Header::From::GetFormattedEmail(OpString8& encoding, BOOL allow_8bit, OpString8& result, BOOL do_quote_pair_encode, BOOL quote_name_atoms) const
{
	return GetFormattedEmail(encoding, m_name, m_address_localpart, m_address_domain, m_comment, allow_8bit, result, do_quote_pair_encode, quote_name_atoms);
}

OP_STATUS Header::From::GetCommentStringWithoutCchar(const OpStringC& comment, OpString& result, BOOL only_first_level)
{
	result.Empty();
	if (comment.IsEmpty())
		return OpStatus::OK; //Nothing to do

	if (result.Reserve(comment.Length()+1) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	int level = 0;
	uni_char source_char;
	const uni_char* source_ptr = comment.CStr();
	uni_char* destination_ptr = const_cast<uni_char*>(result.CStr());
	while (source_ptr && destination_ptr && (source_char=*source_ptr)!=0)
	{
		if (source_char=='(' && (level==0 || !only_first_level))
		{
			level++;
		}
		else if (source_char==')' && (level==1 || !only_first_level))
		{
			level--;

			if (*(source_ptr+1)=='(') //If we have two comments immediately after each other, seperate them by a whitespace
			{
				*(destination_ptr++) = ' ';
			}
		}
		else
		{
			*(destination_ptr++) = source_char;
		}

		source_ptr++;
	}

	*(destination_ptr) = 0;
	return OpStatus::OK;
}



Header::Header(Message* message, HeaderType type)
: Link(),
  m_type(type),
  m_message(message),
  m_time_utc(0) //DateHeaders
{
    m_allow_8bit = message ? message->IsFlagSet(Message::ALLOW_8BIT) : FALSE;
    m_allow_incoming_quotedstring_qp = message ? message->IsFlagSet(Message::ALLOW_QUOTESTRING_QP) : FALSE;
    m_is_outgoing = message ? message->IsFlagSet(Message::IS_OUTGOING) : TRUE;
}

Header::~Header()
{
    ClearAddressList();
	ClearStringList();
}

Header::Header(Header& src) : Link()
{
	if (CopyValuesFrom(src) != OpStatus::OK)
		return;

    if ( (m_charset.Set(src.m_charset) != OpStatus::OK) ||
         (m_headername.Set(src.m_headername) != OpStatus::OK) )
    {
        m_type = ERR_NO_MEMORY;
        m_value.Empty();
        ClearAddressList();
        ClearStringList();
        return;
    }
    m_type = src.m_type;
    m_message = src.m_message;
    m_allow_8bit = src.m_allow_8bit;
    m_allow_incoming_quotedstring_qp = src.m_allow_incoming_quotedstring_qp;
    m_is_outgoing = src.m_is_outgoing;
}

Header& Header::operator=(Header& src)
{
	if (CopyValuesFrom(src) != OpStatus::OK)
		return *this;

    if ( (m_charset.Set(src.m_charset) != OpStatus::OK) ||
         (m_headername.Set(src.m_headername) != OpStatus::OK) )
    {
        m_type = ERR_NO_MEMORY;
        m_value.Empty();
        ClearAddressList();
        ClearStringList();
        return *this;
    }
    m_type = src.m_type;
    m_message = src.m_message;
    m_allow_8bit = src.m_allow_8bit;
    m_allow_incoming_quotedstring_qp = src.m_allow_incoming_quotedstring_qp;
    m_is_outgoing = src.m_is_outgoing;
    return *this;
}

OP_STATUS Header::CopyValuesFrom(Header& src)
{
	OP_STATUS ret;
    if ((ret=m_value.Set(src.m_value)) != OpStatus::OK)
    {
        m_type = ERR_NO_MEMORY;
        m_value.Empty();
        return ret;
    }
    m_time_utc = src.m_time_utc;
    ClearAddressList();
    ClearStringList();
    const From* src_from = src.GetFirstAddress();
    From* dest_from;
    while (src_from)
    {
        dest_from = OP_NEW(From, ());
        if (!dest_from)
        {
            m_type = ERR_NO_MEMORY;
            m_value.Empty();
            ClearAddressList();
            return OpStatus::ERR_NO_MEMORY;
        }

#ifdef ASSERT_ON_EMPTY_ADDRESS
        OP_ASSERT(src_from->HasAddress()); //I guess this should not happen. If it does, ask yourself why
#endif
        *dest_from = *src_from;

        dest_from->Into(&m_address_list);
        src_from = static_cast<From*>(src_from->Suc());
    }

	UINT32 i=0;
	OpString8* src_string_item;
	while ((src_string_item=src.m_string_list.Get(i++))!=NULL)
	{
		OpString8* dst_string_item = OP_NEW(OpString8, ());
		if (!dst_string_item)
			ret = OpStatus::ERR_NO_MEMORY;

		if (!dst_string_item ||
			(ret=dst_string_item->Set(*src_string_item))!=OpStatus::OK ||
			(ret=m_string_list.Add(dst_string_item))!=OpStatus::OK)
		{
            m_type = ERR_NO_MEMORY;
            m_value.Empty();
            ClearAddressList();
            ClearStringList();
            return ret;
		}
	}
	return OpStatus::OK;
}

Header::HeaderType Header::GetType(const OpStringC8& header_name)
{
  const char* header_name_str = header_name.CStr();
  OP_ASSERT(header_name_str);
  if (!header_name_str || *header_name_str==0)
      return UNKNOWN;

  int header_length = header_name.Length();

#define CHAR(X) (X-'A')

  switch( (header_name.CStr()[0]-'A') & 0x1f )
    // To get the range from 0 (a) to 0x19 (z)
    // Doing this makes the switch way faster. Blame gcc.
  {
    case CHAR('A'):
      if (header_length==12 && strni_eq(header_name_str, "ALSO-CONTROL", 12)) return ALSOCONTROL;
      if (header_length==8 && strni_eq(header_name_str, "APPROVED", 8)) return APPROVED;
      if (header_length==7 && strni_eq(header_name_str, "ARCHIVE", 7)) return ARCHIVE;
      if (header_length==13 && strni_eq(header_name_str, "ARTICLE-NAMES", 13)) return ARTICLENAMES;
      if (header_length==15 && strni_eq(header_name_str, "ARTICLE-UPDATES", 15)) return ARTICLEUPDATES;
      return UNKNOWN;

    case CHAR('B'):
      if (header_length==3 && strni_eq(header_name_str, "BCC", 3)) return BCC;
      return UNKNOWN;

    case CHAR('C'):
      if (header_length==2 && strni_eq(header_name_str, "CC", 2)) return CC;
      if (header_length==19 && strni_eq(header_name_str, "CONTENT-DISPOSITION", 19)) return CONTENTDISPOSITION;
      if (header_length==25 && strni_eq(header_name_str, "CONTENT-TRANSFER-ENCODING", 25)) return CONTENTTRANSFERENCODING;
      if (header_length==12 && strni_eq(header_name_str, "CONTENT-TYPE", 12)) return CONTENTTYPE;
      if (header_length==7 && strni_eq(header_name_str, "CONTROL", 7)) return CONTROL;
      if (header_length==8 && strni_eq(header_name_str, "COMMENTS", 8)) return COMMENTS;
      if (header_length==13 && strni_eq(header_name_str, "COMPLAINTS-TO", 13)) return COMPLAINTSTO;
      return UNKNOWN;

    case CHAR('D'):
      if (header_length==4 && strni_eq(header_name_str, "DATE", 4)) return DATE;
      if (header_length==12 && strni_eq(header_name_str, "DISTRIBUTION", 12)) return DISTRIBUTION;
      return UNKNOWN;

    case CHAR('E'):
      if (header_length==9 && strni_eq(header_name_str, "ENCRYPTED", 9)) return ENCRYPTED;
      if (header_length==7 && strni_eq(header_name_str, "EXPIRES", 7)) return EXPIRES;
      return UNKNOWN;

    case CHAR('F'):
      if (header_length==4 && strni_eq(header_name_str, "FROM", 4)) return FROM;
      if (header_length==11 && strni_eq(header_name_str, "FOLLOWUP-TO", 11)) return FOLLOWUPTO;
      return UNKNOWN;

    case CHAR('G'):
      return UNKNOWN;

    case CHAR('H'):
      return UNKNOWN;

    case CHAR('I'):
      if (header_length==11 && strni_eq(header_name_str, "IN-REPLY-TO", 11)) return INREPLYTO;
      if (header_length==13 && strni_eq(header_name_str, "INJECTOR-INFO", 13)) return INJECTORINFO;
      return UNKNOWN;

    case CHAR('J'):
      return UNKNOWN;

    case CHAR('K'):
      if (header_length==8 && strni_eq(header_name_str, "KEYWORDS", 8)) return KEYWORDS;
      return UNKNOWN;

    case CHAR('L'):
      if (header_length==5 && strni_eq(header_name_str, "LINES", 5)) return LINES;
      if (header_length==7 && strni_eq(header_name_str, "LIST-ID", 7)) return LISTID;
	  if (header_length==9 && strni_eq(header_name_str, "LIST-POST", 9)) return LISTPOST;
      return UNKNOWN;

    case CHAR('M'):
      if (header_length==12 && strni_eq(header_name_str, "MAILING-LIST", 12)) return MAILINGLIST;
      if (header_length==10 && strni_eq(header_name_str, "MESSAGE-ID", 10)) return MESSAGEID;
      if (header_length==12 && strni_eq(header_name_str, "MIME-VERSION", 12)) return MIMEVERSION;
      if (header_length==14 && strni_eq(header_name_str, "MAIL-COPIES-TO", 14)) return MAILCOPIESTO;
      return UNKNOWN;

    case CHAR('N'):
      if (header_length==10 && strni_eq(header_name_str, "NEWSGROUPS", 10)) return NEWSGROUPS;
      return UNKNOWN;

    case CHAR('O'):
      if (header_length==13 && strni_eq(header_name_str, "OPERA-AUTO-CC", 13)) return OPERAAUTOCC;
      if (header_length==14 && strni_eq(header_name_str, "OPERA-AUTO-BCC", 14)) return OPERAAUTOBCC;
      if (header_length==14 && strni_eq(header_name_str, "OPERA-RECEIVED", 14)) return OPERARECEIVED;
      if (header_length==12 && strni_eq(header_name_str, "ORGANIZATION", 12)) return ORGANIZATION;
      return UNKNOWN;

    case CHAR('P'):
      if (header_length==17 && strni_eq(header_name_str, "POSTED-AND-MAILED", 17)) return POSTEDANDMAILED;
      return UNKNOWN;

    case CHAR('Q'):
      return UNKNOWN;

    case CHAR('R'):
      if (header_length==8 && strni_eq(header_name_str, "RECEIVED", 8)) return RECEIVED;
      if (header_length==10 && strni_eq(header_name_str, "REFERENCES", 10)) return REFERENCES;
      if (header_length==8 && strni_eq(header_name_str, "REPLY-TO", 8)) return REPLYTO;
      if (header_length==11 && strni_eq(header_name_str, "RETURN-PATH", 11)) return RETURNPATH;
      if (header_length==11 && strni_eq(header_name_str, "RESENT-DATE", 11)) return RESENTDATE;
      if (header_length==11 && strni_eq(header_name_str, "RESENT-FROM", 11)) return RESENTFROM;
      if (header_length==17 && strni_eq(header_name_str, "RESENT-MESSAGE-ID", 17)) return RESENTMESSAGEID;
      if (header_length==15 && strni_eq(header_name_str, "RESENT-REPLY-TO", 15)) return RESENTREPLYTO;
      if (header_length==13 && strni_eq(header_name_str, "RESENT-SENDER", 13)) return RESENTSENDER;
      if (header_length==9 && strni_eq(header_name_str, "RESENT-TO", 9)) return RESENTTO;
      if (header_length==10 && strni_eq(header_name_str, "RESENT-BCC", 10)) return RESENTBCC;
      if (header_length==9 && strni_eq(header_name_str, "RESENT-CC", 9)) return RESENTCC;
      return UNKNOWN;

    case CHAR('S'):
      if (header_length==7 && strni_eq(header_name_str, "SUBJECT", 7)) return SUBJECT;
      if (header_length==6 && strni_eq(header_name_str, "STATUS", 6)) return STATUS;
      if (header_length==6 && strni_eq(header_name_str, "SENDER", 6)) return SENDER;
      if (header_length==8 && strni_eq(header_name_str, "SEE-ALSO", 8)) return SEEALSO;
      if (header_length==7 && strni_eq(header_name_str, "SUMMARY", 7)) return SUMMARY;
      if (header_length==10 && strni_eq(header_name_str, "SUPERSEDES", 10)) return SUPERSEDES;
      return UNKNOWN;

    case CHAR('T'):
      if (header_length==2 && strni_eq(header_name_str, "TO", 2)) return TO;
      return UNKNOWN;

    case CHAR('U'):
      if (header_length==10 && strni_eq(header_name_str, "USER-AGENT", 10)) return USERAGENT;
      return UNKNOWN;

    case CHAR('V'):
      return UNKNOWN;

    case CHAR('W'):
      return UNKNOWN;

    case CHAR('X'):
	  if (header_length==10 && strni_eq(header_name_str, "X-PRIORITY", 10)) return XPRIORITY;
      if (header_length==6 && strni_eq(header_name_str, "X-FACE", 6)) return XFACE;
	  if (header_length==8 && strni_eq(header_name_str,"X-MAILER", 8)) return XMAILER;
	  if (header_length==14 && strni_eq(header_name_str,"X-MAILING-LIST", 14)) return XMAILINGLIST;
      if (header_length==4 && strni_eq(header_name_str, "XREF", 4)) return XREF;
      return UNKNOWN;

    default:
      return UNKNOWN;
  }
#undef CHAR
}

OP_STATUS Header::DetachFromMessage()
{
    if (!m_message)
        return OpStatus::OK; //Already detached

    m_is_outgoing = m_message->IsFlagSet(Message::IS_OUTGOING);
    return m_message->GetCharset(m_charset);
}

OP_STATUS  Header::GetName(HeaderType type, OpString8& header_name)
{
    switch(type)
    {
    case ALSOCONTROL:     return header_name.Set("Also-Control");
    case APPROVED:        return header_name.Set("Approved");
    case ARCHIVE:         return header_name.Set("Archive");
    case ARTICLENAMES:    return header_name.Set("Article-Names");
    case ARTICLEUPDATES:  return header_name.Set("Article-Updates");
    case BCC:             return header_name.Set("Bcc");
    case CC:              return header_name.Set("Cc");
    case COMMENTS:        return header_name.Set("Comments");
    case COMPLAINTSTO:    return header_name.Set("Complaints-To");
    case CONTENTDISPOSITION: return header_name.Set("Content-Disposition");
    case CONTENTTRANSFERENCODING: return header_name.Set("Content-Transfer-Encoding");
    case CONTENTTYPE:     return header_name.Set("Content-Type");
    case CONTROL:         return header_name.Set("Control");
    case DATE:            return header_name.Set("Date");
    case DISTRIBUTION:    return header_name.Set("Distribution");
    case ENCRYPTED:       return header_name.Set("Encrypted");
    case EXPIRES:         return header_name.Set("Expires");
    case FOLLOWUPTO:      return header_name.Set("Followup-To");
    case FROM:            return header_name.Set("From");
    case INJECTORINFO:    return header_name.Set("Injector-Info");
    case INREPLYTO:       return header_name.Set("In-Reply-To");
    case KEYWORDS:        return header_name.Set("Keywords");
    case LINES:           return header_name.Set("Lines");
    case LISTID:          return header_name.Set("List-Id");
	case LISTPOST:        return header_name.Set("List-Post");
    case MAILCOPIESTO:    return header_name.Set("Mail-Copies-To");
    case MESSAGEID:       return header_name.Set("Message-ID");
	case MAILINGLIST:     return header_name.Set("Mailing-List");
    case MIMEVERSION:     return header_name.Set("MIME-Version");
    case NEWSGROUPS:      return header_name.Set("Newsgroups");
    case OPERAAUTOBCC:	  return header_name.Set("Opera-Auto-Bcc");
    case OPERAAUTOCC:	  return header_name.Set("Opera-Auto-Cc");
    case OPERARECEIVED:   return header_name.Set("Opera-Received");
    case ORGANIZATION:    return header_name.Set("Organization");
    case POSTEDANDMAILED: return header_name.Set("Posted-And-Mailed");
    case RECEIVED:        return header_name.Set("Received");
    case REFERENCES:      return header_name.Set("References");
    case REPLYTO:         return header_name.Set("Reply-To");
    case RESENTBCC:       return header_name.Set("Resent-bcc");
    case RESENTCC:        return header_name.Set("Resent-cc");
    case RESENTDATE:      return header_name.Set("Resent-Date");
    case RESENTFROM:      return header_name.Set("Resent-From");
    case RESENTMESSAGEID: return header_name.Set("Resent-Message-ID");
    case RESENTREPLYTO:   return header_name.Set("Resent-Reply-To");
    case RESENTSENDER:    return header_name.Set("Resent-Sender");
    case RESENTTO:        return header_name.Set("Resent-To");
    case RETURNPATH:      return header_name.Set("Return-path");
    case SEEALSO:         return header_name.Set("See-Also");
    case SENDER:          return header_name.Set("Sender");
    case STATUS:          return header_name.Set("Status");
    case SUBJECT:         return header_name.Set("Subject");
    case SUMMARY:         return header_name.Set("Summary");
    case SUPERSEDES:      return header_name.Set("Supersedes");
    case TO:              return header_name.Set("To");
    case USERAGENT:       return header_name.Set("User-Agent");
    case XFACE:           return header_name.Set("X-Face");
	case XMAILER:		  return header_name.Set("X-Mailer");
	case XMAILINGLIST:    return header_name.Set("X-Mailing-List");
	case XPRIORITY:		  return header_name.Set("X-Priority");
    case XREF:            return header_name.Set("Xref");
    case UNKNOWN: //Fallthrough, this must be handled outside of static function
    default: header_name.Empty(); return OpStatus::OK;
    };
}

Header::HeaderType Header::GetTypeForResend(Header::HeaderType type)
{
	switch (type)
	{
		case BCC:
			return RESENTBCC;
		case CC:
			return RESENTCC;
		case DATE:
			return RESENTDATE;
		case FROM:
			return RESENTFROM;
		case MESSAGEID:
			return RESENTMESSAGEID;
		case REPLYTO:
			return RESENTREPLYTO;
		case TO:
			return RESENTTO;
	}

	return type;
}

OP_STATUS Header::GetName(OpString8& header_name) const
{
	if (m_type==UNKNOWN)
		return header_name.Set(m_headername);
	else
		return GetName(m_type, header_name);
}

OP_STATUS Header::GetTranslatedName(OpString16& translated_name) const
{
	Str::LocaleString name_id = Str::NOT_A_STRING;

	switch(m_type)
	{
		case Header::BCC:
			name_id = Str::SI_MIME_TRANSLATE_BCC;
			break;
		case Header::CC:
			name_id = Str::SI_MIME_TRANSLATE_CC;
			break;
		case DATE:
			name_id = Str::SI_MIME_TRANSLATE_DATE;
			break;
		case FOLLOWUPTO:
			name_id = Str::S_MIME_TRANSLATE_FOLLOW_UP_TO;
			break;
		case FROM:
			name_id = Str::SI_MIME_TRANSLATE_FROM;
			break;
		case NEWSGROUPS:
			name_id = Str::S_MIME_TRANSLATE_NEWSGROUPS;
			break;
		case ORGANIZATION:
			name_id = Str::S_MIME_TRANSLATE_ORGANIZATION;
			break;
		case REPLYTO:
			name_id = Str::S_MIME_TRANSLATE_REPLY_TO;
			break;
		case RESENTFROM:
			name_id = Str::S_MIME_TRANSLATE_RESENT_FROM;
			break;
		case SUBJECT:
			name_id = Str::SI_MIME_TRANSLATE_SUBJECT;
			break;
		case TO:
			name_id = Str::SI_MIME_TRANSLATE_TO;
			break;
		default:
			break;
	}

	if(name_id != Str::NOT_A_STRING)
	{
		return g_languageManager->GetString(name_id, translated_name);
	}
	else
	{
		OpString8 name;
		RETURN_IF_ERROR(GetName(name));
		return translated_name.Set(name.CStr());
	}
}

BOOL Header::IsInternalHeader(HeaderType type)
{
    switch (type)
    {
    case OPERAAUTOCC: //Fallthrough
    case OPERAAUTOBCC:
    case OPERARECEIVED:  return TRUE;
    default: return FALSE;
    }
}

OP_STATUS Header::GetStringByIndexFromStart(OpString8& string, UINT16 index_from_start) const
{
	OpString8* string_ptr = m_string_list.Get(index_from_start);
	if (!string_ptr)
	{
		string.Empty();
		return OpStatus::OK;
	}

	return string.Set(*string_ptr);
}

OP_STATUS Header::GetStringByIndexFromEnd(OpString8& string, UINT16 index_from_end) const
{
	return GetStringByIndexFromStart(string, (index_from_end==USHRT_MAX) ? 0 : m_string_list.GetCount()-index_from_end-1);
}

INT16 Header::FindStringIndex(const OpStringC8& string) const
{
	UINT32 i=0;
	OpString8* string_item;
	while ((string_item=m_string_list.Get(i++))!=NULL)
	{
		if (string_item->Compare(string)==0)
			return (i-1); //Sub one, it has already been incremented
	}
	return -1;
}

OP_STATUS Header::FoldHeader(OpString8& folded_header, const OpStringC8& unfolded_header) const //RFC2822, paragraph 2.2.3
{
	if (!HeaderSupportsFolding())
		return folded_header.Set(unfolded_header);

	folded_header.Empty();
	BOOL is_folded;

	// First try 'unforced' folding at 78 characters
	RETURN_IF_ERROR(FoldHeader(folded_header, unfolded_header, 78, FALSE, is_folded));
	if (!is_folded) // This didn't work, try forced folding at 998 characters
	{
		folded_header.Empty();
		RETURN_IF_ERROR(FoldHeader(folded_header, unfolded_header, 998, TRUE, is_folded));
	}

	return OpStatus::OK;
}

OP_STATUS Header::FoldHeader(OpString8& folded_header, const OpStringC8& unfolded_header, UINT32 max_line_length, BOOL force, BOOL& is_folded) const
{
	is_folded = FALSE;
	char* current_pos = const_cast<char*>(unfolded_header.CStr());
	char * white_space_pos;

	const char* name_end = current_pos ? strchr(current_pos, ':') : NULL;
	int name_length = name_end ? name_end-unfolded_header.CStr()+1 : 0;
	int remaining = unfolded_header.Length();

	BOOL forced_fold;
	OP_STATUS ret;
	while (remaining>0)
	{
		if (remaining <= (int)max_line_length)
		{
			is_folded = TRUE;
			return folded_header.Append(current_pos);
		}

		forced_fold = FALSE;
		char* fold_pos = current_pos + max_line_length;
		while (!op_isspace(*fold_pos) && fold_pos>current_pos) fold_pos--;
		if (fold_pos==current_pos || (folded_header.IsEmpty() && fold_pos<=(current_pos+name_length+1))) //No place to fold? Force one
		{
			if (force)
			{
				forced_fold = TRUE;
				fold_pos = current_pos + max_line_length;
				while (*fold_pos != ',' && fold_pos>current_pos) fold_pos--; //Minimize damage: try to split at ','
				if (fold_pos==current_pos || (folded_header.IsEmpty() && fold_pos<=(current_pos+name_length+1))) //Still no place to fold? Force fold, and cross fingers
				{
					fold_pos = current_pos + max_line_length;
					while (*fold_pos=='\r' || *fold_pos=='\n') fold_pos--; //Just to be safe
				}
			}
			else
			{
				is_folded = FALSE;
				return OpStatus::OK;
			}
		}

		if ((ret=folded_header.Append(current_pos, fold_pos-current_pos))!=OpStatus::OK)
			return ret;
		if (fold_pos > current_pos)
		{
			// find extent of whitespace and replace with "\r\n "
			white_space_pos = folded_header.CStr() + folded_header.Length() - 1;
			while (op_isspace(*white_space_pos) && white_space_pos >folded_header.CStr()) white_space_pos--;
			if (white_space_pos < folded_header.CStr() + folded_header.Length() - 1)
				folded_header.Delete(white_space_pos + 1 - folded_header.CStr(), KAll); // +1 because we need to step back one..
			RETURN_IF_ERROR(folded_header.Append("\r\n "));

		}

		if (forced_fold && (ret = folded_header.Append(" "))!=OpStatus::OK )
			return ret;

		// Then we move forward the extent of whitespace
		white_space_pos = fold_pos;
		while (op_isspace(*white_space_pos) && white_space_pos < current_pos + remaining) white_space_pos++;
		fold_pos = white_space_pos;

		remaining -= fold_pos - current_pos;
		current_pos = fold_pos;
	}

	return OpStatus::OK;
}

OP_STATUS Header::UnfoldHeader(OpString16& unfolded_header, const OpStringC16& folded_header) const
{
	unfolded_header.Empty();
	if (folded_header.IsEmpty())
		return OpStatus::OK;

	//Skip linefeeds at start of line (it shouldn't be there in the first place)
	uni_char* folded_header_start = const_cast<uni_char*>(folded_header.CStr());
	while (*folded_header_start=='\r' || *folded_header_start=='\n') folded_header_start++;

	if (!HeaderSupportsFolding())
	{
		int header_length = uni_strlen(folded_header_start);
		while (header_length>0 && (*(folded_header_start+header_length-1)=='\r' || *(folded_header_start+header_length-1)=='\n')) header_length--;
		return unfolded_header.Set(folded_header_start, header_length);
	}

	OP_STATUS ret;
	uni_char* current_pos = folded_header_start;
	uni_char* start_fold_pos;
	uni_char* end_fold_pos;
	while (current_pos && *current_pos)
	{
		start_fold_pos = uni_strpbrk(current_pos, UNI_L("\r\n"));
		if (!start_fold_pos)
		{
			return unfolded_header.Append(current_pos);
		}

		end_fold_pos = start_fold_pos;
		if (*end_fold_pos=='\r') end_fold_pos++;
		if (*end_fold_pos=='\n') end_fold_pos++;
		if (*end_fold_pos)
		{
			if (*end_fold_pos=='\t') //Folded with TAB. Use space instead
			{
				*end_fold_pos = ' ';
			}
			else if (*end_fold_pos!=' ')
			{
				start_fold_pos = end_fold_pos; //This line did not fold. Include CRLF
			}
		}

		if (start_fold_pos!=current_pos &&	//Line had content
			start_fold_pos!=end_fold_pos && //Line has a fold
			(*(start_fold_pos-1)==' ' || *(start_fold_pos-1)=='\t')) //Previous line ended in WSP
		{
			while (*end_fold_pos==' ' || *end_fold_pos=='\t') end_fold_pos++;
		}

		if ((ret=unfolded_header.Append(current_pos, start_fold_pos-current_pos)) != OpStatus::OK)
			return ret;

		current_pos = end_fold_pos;
	}

	return OpStatus::OK;
}

OP_STATUS Header::UnfoldHeader(OpString8& unfolded_header, const OpStringC8& folded_header) const
{
	unfolded_header.Empty();
	if (folded_header.IsEmpty())
		return OpStatus::OK;

	//Skip linefeeds at start of line (it shouldn't be there in the first place)
	char* folded_header_start = const_cast<char*>(folded_header.CStr());
	while (*folded_header_start=='\r' || *folded_header_start=='\n') folded_header_start++;

	if (!HeaderSupportsFolding())
	{
		int header_length = strlen(folded_header_start);
		while (header_length>0 && (*(folded_header_start+header_length-1)=='\r' || *(folded_header_start+header_length-1)=='\n')) header_length--;
		return unfolded_header.Set(folded_header_start, header_length);
	}

	OP_STATUS ret;
	char* current_pos = folded_header_start;
	char* start_fold_pos;
	char* end_fold_pos;
	while (current_pos && *current_pos)
	{
		start_fold_pos = strpbrk(current_pos, "\r\n");
		if (!start_fold_pos)
		{
			return unfolded_header.Append(current_pos);
		}

		end_fold_pos = start_fold_pos;
		if (*end_fold_pos=='\r') end_fold_pos++;
		if (*end_fold_pos=='\n') end_fold_pos++;
		if (*end_fold_pos)
		{
			if (*end_fold_pos=='\t') //Folded with TAB. Use space instead
			{
				*end_fold_pos = ' ';
			}
			else if (*end_fold_pos!=' ')
			{
				start_fold_pos = end_fold_pos; //This line did not fold. Include CRLF
			}
		}

		if (start_fold_pos!=current_pos &&	//Line had content
			start_fold_pos!=end_fold_pos && //Line has a fold
			(*(start_fold_pos-1)==' ' || *(start_fold_pos-1)=='\t')) //Previous line ended in WSP
		{
			while (*end_fold_pos==' ' || *end_fold_pos=='\t') end_fold_pos++;
		}

		if ((ret=unfolded_header.Append(current_pos, start_fold_pos-current_pos)) != OpStatus::OK)
			return ret;

		current_pos = end_fold_pos;
	}

	return OpStatus::OK;
}

OP_STATUS Header::GetCharset(OpString8& charset) const
{
    OP_STATUS ret;
    if (m_message)
    {
        if ((ret=m_message->GetCharset(charset)) != OpStatus::OK)
            return ret;
    }
    else
    {
        if ((ret=charset.Set(m_charset)) != OpStatus::OK)
            return ret;
    }

    if (charset.IsEmpty())
    {
        return charset.Set(DEFAULT_CHARSET_STR);
    }
    return OpStatus::OK;
}


BOOL Header::HeaderSupportsQuotePair() const
{
	//The items that are commented out in this switch, I have simply not bothered to look up if they supports quote-pair or not...
	switch(m_type)
	{
//	case ALSOCONTROL:,     // so1036 - "Also-Control"
	case APPROVED:
//	case ARCHIVE,         // d-ietf - "Archive"
//	case ARTICLENAMES,    // so1036 - "Article-Names"
//	case ARTICLEUPDATES,  // so1036 - "Article-Updates"
	case BCC:
	case CC:
//	case COMPLAINTSTO,    // d-ietf - "Complaints-To"
//	case CONTENTDISPOSITION, // 2046 - "Content-Disposition"
//	case CONTENTTRANSFERENCODING, // 2046 - "Content-Transfer-Encoding"
//	case CONTENTTYPE,     //   2046 - "Content-Type"
//	case CONTROL,         //    850 - "Control"
//	case DISTRIBUTION,    //    850 - "Distribution"
//	case ENCRYPTED,       //    822 - "Encrypted"
//	case FOLLOWUPTO,      //    850 - "Followup-To"         NewsgroupsHeader
	case FROM:
	case INREPLYTO:
//	case INJECTORINFO,    // d-ietf - "Injector-Info"
//	case LINES,           //   1036 - "Lines"
//	case LISTID,          //   2919 - "List-Id"
//	case MAILCOPIESTO,    // d-ietf - "Mail-Copies-To"
	case MESSAGEID:
//	case MIMEVERSION,     //   2046 - "MIME-Version"
//	case NEWSGROUPS,      //    850 - "Newsgroups"          NewsgroupsHeader, DO NOT FOLD!
//	case ORGANIZATION,    //    850 - "Organization"
//	case POSTEDANDMAILED, // d-ietf - "Posted-And-Mailed"
	case OPERAAUTOCC:
	case OPERAAUTOBCC:
	case REFERENCES:
	case REPLYTO:
	case RESENTBCC:
	case RESENTCC:
	case RESENTFROM:
	case RESENTMESSAGEID:
	case RESENTREPLYTO:
	case RESENTSENDER:
	case RESENTTO:
//	case RETURNPATH,      //    822 - "Return-path"
//	case SEEALSO,         // so1036 - "See-Also"
	case SENDER:
//	case STATUS,
//	case SUMMARY,         //   1036 - "Summary"
//	case SUPERSEDES,      // so1036 - "Supersedes"
	case TO:
//	case USERAGENT,       // d-ietf - "User-Agent"
//	case XFACE,
//	case XMAILER,
//	case XREF,            //   1036 - "Xref"
		return TRUE;
	default: return FALSE;
	}
}

BOOL Header::HeaderSupportsFolding() const
{
	//The items that are commented out in this switch, I have simply not bothered to look up if they supports folding or not...
	switch(m_type)
	{
	case APPROVED:
//	case ARCHIVE,         // d-ietf - "Archive"
//	case ARTICLENAMES,    // so1036 - "Article-Names"
//	case ARTICLEUPDATES,  // so1036 - "Article-Updates"
	case BCC:
	case CC:
	case COMMENTS:
//	case COMPLAINTSTO,    // d-ietf - "Complaints-To"
	case CONTENTTYPE:
//	case CONTROL,         //    850 - "Control"
	case DATE:
//	case DISTRIBUTION,    //    850 - "Distribution"
//	case ENCRYPTED,       //    822 - "Encrypted"
	case EXPIRES:
	case FROM:
	case INREPLYTO:
//	case INJECTORINFO,    // d-ietf - "Injector-Info"
//	case KEYWORDS,        //   2822 - "Keywords"								'"Keywords:" phrase *("," phrase) CRLF'
//	case LINES,           //   1036 - "Lines"
//	case LISTID,          //   2919 - "List-Id"
//	case MAILCOPIESTO,    // d-ietf - "Mail-Copies-To"
	case MESSAGEID:
//	case MIMEVERSION,     //   2046 - "MIME-Version"
	case OPERAAUTOCC:
	case OPERAAUTOBCC:
	case OPERARECEIVED:
//	case ORGANIZATION,    //    850 - "Organization"
//	case POSTEDANDMAILED, // d-ietf - "Posted-And-Mailed"
	case RECEIVED:
	case REFERENCES:
	case REPLYTO:
	case RESENTBCC:
	case RESENTCC:
	case RESENTDATE:
	case RESENTFROM:
	case RESENTMESSAGEID:
	case RESENTREPLYTO:
	case RESENTSENDER:
	case RESENTTO:
	case RETURNPATH:
//	case SEEALSO,         // so1036 - "See-Also"
	case SENDER:
//	case STATUS,
	case SUBJECT:
//	case SUMMARY,         //   1036 - "Summary"
//	case SUPERSEDES,      // so1036 - "Supersedes"
	case TO:
//	case USERAGENT,       // d-ietf - "User-Agent"
//	case XFACE,
//	case XMAILER,
//	case XREF,            //   1036 - "Xref"
		return TRUE;
	default: return FALSE;
	}
}
/*
OP_STATUS Header::Parse()
{
	OP_ASSERT(0); //Todo: Code here
	return OpStatus::ERR;
}
*/
OP_STATUS Header::GetNameAndValue(OpString8& header_string)
{
    OP_STATUS ret;

	OpString8 unfolded_header_string;
    OpString8 header_value;

    if ((ret=GetName(unfolded_header_string))!=OpStatus::OK ||
		(ret=GetValue(header_value, HeaderSupportsQuotePair())) != OpStatus::OK) //We want quoted_pair esacped sequences in GetNameAndValue!
	{
        return ret;
	}

    //Optimization: Make sure header_string has room for seperator and value, to avoid two calls to Grow()
    if (!unfolded_header_string.Reserve(unfolded_header_string.Length()+2+header_value.Length()+1))
        return OpStatus::ERR_NO_MEMORY;

    if ((ret=unfolded_header_string.Append(": "))!=OpStatus::OK ||
		(ret=unfolded_header_string.Append(header_value))!=OpStatus::OK)
	{
		return ret;
	}

	return FoldHeader(header_string, unfolded_header_string);
}

OP_STATUS Header::GetValue(OpString8& header_value, BOOL do_quote_pair_encode)
{
	HeaderValue header_value16;
	OpString8 charset;

	//QuotePair will be done after QP-encoding (except for AddressHeaders(), which needs to do it now)
	RETURN_IF_ERROR(GetValue(header_value16, IsAddressHeader(), TRUE));

	RETURN_IF_ERROR(GetCharset(charset));
	RETURN_IF_ERROR(OpQP::Encode(header_value16, header_value, charset, m_allow_8bit, IsSubjectHeader(), FALSE /*We have already taken care of QP-lookalikes*/));

	OP_ASSERT(!do_quote_pair_encode || HeaderSupportsQuotePair());

	if (do_quote_pair_encode && HeaderSupportsQuotePair() && !IsAddressHeader())
		RETURN_IF_ERROR(OpMisc::EncodeQuotePair(header_value, TRUE));

	return OpStatus::OK;
}

OP_STATUS Header::GetValue(HeaderValue& header_value, BOOL do_quote_pair_encode, BOOL qp_encode_atoms)
{
	OpString8 charset;

	// Make sure the header value is empty
	header_value.Empty();

	if (qp_encode_atoms)
		RETURN_IF_ERROR(GetCharset(charset));

	if (IsDateHeader())
	{
		OP_ASSERT(m_address_list.Empty());
		OP_ASSERT(m_string_list.GetCount()==0);
		OP_ASSERT(m_value.IsEmpty());

		BrowserUtils* browser_utils = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
		time_t time_utc = m_time_utc;
        if (!time_utc) // 0 = ::Now
			time_utc = browser_utils->CurrentTime();

		int tzmin = browser_utils->CurrentTimezone()/60;

        struct tm* ltime = localtime(&time_utc);
		if (!ltime)
			return OpStatus::ERR;

        uni_char* buffer = header_value.m_value_buffer.Reserve(33);
        if (!buffer)
            return OpStatus::ERR_NO_MEMORY;

		if (ltime->tm_isdst > 0)
			tzmin -= 60;

		bool isplus = FALSE;
		if (tzmin<0)
		{
			tzmin = -tzmin;			//abs is inefficient for ints
			isplus = TRUE;
		}
		int tz_hour = tzmin / 60;
		int tz_minute = tzmin - tz_hour * 60;

        uni_sprintf(buffer, UNI_L("%s, %02d %s %04d %02d:%02d:%02d %c%02d%02d"), //Do not change this format-string!
					dayNames[ltime->tm_wday], ltime->tm_mday, monthNames[ltime->tm_mon], (ltime->tm_year+1900),
					ltime->tm_hour, ltime->tm_min, ltime->tm_sec, isplus?'+':'-', tz_hour, tz_minute);

		header_value.iBuffer = buffer;
        return OpStatus::OK;
	}
	else if (IsAddressHeader())
	{
		OP_ASSERT(m_time_utc==0);
		OP_ASSERT(m_string_list.GetCount()==0);
		OP_ASSERT(m_value.IsEmpty());

		//Clear tag on all items
		From* address_item = static_cast<From*>(m_address_list.First());
		while (address_item)
		{
			address_item->m_tagged = 0;
			address_item = static_cast<From*>(address_item->Suc());
		}

		OpString8 formatted_email8;
		OpString16 formatted_email16;
		OpString group;
		BOOL added_separator = TRUE; //No need for separator until we have added one item
		const From* first_item_of_group = NULL;
		address_item = static_cast<From*>(m_address_list.First());
		while (address_item)
		{
			if (address_item->m_tagged==0 && //Only process items not already processed
				(group.IsEmpty() || address_item->CompareGroup(group)==0)) //If we're in a group, only process groups items
			{
				if (group.IsEmpty() && address_item->HasGroup()) //Is this item first item in a group?
				{
					RETURN_IF_ERROR(group.Set(address_item->GetGroup()));

#ifdef ASSERT_ON_EMPTY_ADDRESS
					OP_ASSERT(group.HasContent());
#endif
					if (group.HasContent() && header_value.m_value_buffer.HasContent())
 						RETURN_IF_ERROR(header_value.m_value_buffer.Append(", "));

					if (do_quote_pair_encode)
						RETURN_IF_ERROR(OpMisc::EncodeQuotePair(group));
					RETURN_IF_ERROR(header_value.m_value_buffer.Append(group));
					RETURN_IF_ERROR(header_value.m_value_buffer.Append(":"));

					added_separator = TRUE;
					first_item_of_group = address_item;
				}

				if (qp_encode_atoms)
				{
					RETURN_IF_ERROR(address_item->GetFormattedEmail(charset, m_allow_8bit, formatted_email8, do_quote_pair_encode));
					RETURN_IF_ERROR(formatted_email16.Set(formatted_email8));
				}
				else
				{
					RETURN_IF_ERROR(address_item->GetFormattedEmail(formatted_email16, do_quote_pair_encode));
				}

#ifdef ASSERT_ON_EMPTY_ADDRESS
				OP_ASSERT(formatted_email16.HasContent());
#endif
				if (formatted_email16.HasContent())
				{
					if (!added_separator && header_value.m_value_buffer.HasContent())
						RETURN_IF_ERROR(header_value.m_value_buffer.Append(", "));

					RETURN_IF_ERROR(header_value.m_value_buffer.Append(formatted_email16));

					added_separator = FALSE;
				}

				address_item->m_tagged = 1; //We have processed this item
			}

			address_item = static_cast<From*>(address_item->Suc());

			if (address_item==NULL && first_item_of_group!=NULL)
			{
				RETURN_IF_ERROR(header_value.m_value_buffer.Append(";"));

				added_separator = FALSE;
				address_item = static_cast<From*>(first_item_of_group->Suc());
				first_item_of_group = NULL;
				group.Empty();
			}
		}

		header_value.iBuffer = header_value.m_value_buffer.CStr();

		return OpStatus::OK;
	}
	else if (IsMessageIdHeader() || IsReferencesHeader())
	{
		OP_ASSERT(m_time_utc==0);
		OP_ASSERT(m_address_list.Empty());
		OP_ASSERT(m_value.IsEmpty());

		if (m_string_list.GetCount()==0) //Nothing to do
			return OpStatus::OK;

		OpString8 header_value8;

		//First insert first item, and then as many as we can fit of the last items
		OpString8* string_ptr = m_string_list.Get(0);
		UINT32 i = m_string_list.GetCount();
		int insert_position = 0;
		for (UINT32 count = 0; count < 20 && string_ptr; count++)
		{
			//msg-id must start with '<'
			if (*(string_ptr->CStr())!='<')
				RETURN_IF_ERROR(string_ptr->Insert(0, "<"));

			//msg-id must end with '>'
			if (*(string_ptr->CStr()+string_ptr->Length()-1)!='>')
				RETURN_IF_ERROR(string_ptr->Append(">"));

			int strlength = string_ptr->Length();

			// Add value
			RETURN_IF_ERROR(header_value8.Insert(insert_position, string_ptr->CStr(), strlength));

			//Seperate atoms if this is not the first item on this line, fold
			if (insert_position != 0)
				RETURN_IF_ERROR(header_value8.Insert(insert_position, " ", 1));

			//MessageId should have only one msg-id (and first item is already inserted)
			if (IsMessageIdHeader() || i==1)
			{
				break;
			}
			else
			{
				if (insert_position==0)
					insert_position = strlength;

				string_ptr = m_string_list.Get(--i);
			}
		}

		RETURN_IF_ERROR(header_value.m_value_buffer.Set(header_value8));
		header_value.iBuffer = header_value.m_value_buffer.CStr();

		return OpStatus::OK;
	}
	else if (IsNewsgroupsHeader())
	{
		OP_ASSERT(m_time_utc==0);
		OP_ASSERT(m_address_list.Empty());
		OP_ASSERT(m_value.IsEmpty());

		if (m_string_list.GetCount()==0) //Nothing to do
			return OpStatus::OK;

		OpString8* string_ptr;
		UINT32 i;
		for (i=0; i<m_string_list.GetCount(); i++)
		{
			string_ptr = m_string_list.Get(i);
			if (!string_ptr)
				continue;

			//Separate atoms
			if (header_value.m_value_buffer.HasContent())
				RETURN_IF_ERROR(header_value.m_value_buffer.Append(","));
			RETURN_IF_ERROR(header_value.m_value_buffer.Append(*string_ptr));
		}

		header_value.iBuffer = header_value.m_value_buffer.CStr();

		return OpStatus::OK;
	}
	else
	{
		OP_ASSERT(m_time_utc==0);
		OP_ASSERT(m_address_list.Empty());
		OP_ASSERT(m_string_list.GetCount()==0);
		OP_ASSERT(!do_quote_pair_encode || HeaderSupportsQuotePair());

		if (do_quote_pair_encode && HeaderSupportsQuotePair())
		{
			RETURN_IF_ERROR(header_value.m_value_buffer.Set(m_value));
			RETURN_IF_ERROR(OpMisc::EncodeQuotePair(header_value.m_value_buffer, TRUE));
			header_value.iBuffer = header_value.m_value_buffer.CStr();
		}
		else
		{
			header_value.iBuffer = m_value.CStr();
		}

		return OpStatus::OK;
	}
}


OP_STATUS Header::SetValue(const OpStringC8& header_value, BOOL do_quote_pair_decode, int* parseerror_start, int* parseerror_end)
{
	BOOL parsererror_possible = TRUE;
	if (parseerror_start)
		*parseerror_start=0;

	if (parseerror_end)
		*parseerror_end=0;

	m_time_utc = 0;
	ClearAddressList();
	ClearStringList();

	if (header_value.IsEmpty())
	{
		m_value.Empty();
		return OpStatus::OK;
	}

	// This is for avoiding freezing in really big headers like eg. TO: header with 2000 mail addresses
	// Should really be done in a better way
	const char* header_value_ptr = header_value.CStr();

	if (header_value_ptr)
	{
		for (; *header_value_ptr && header_value_ptr - header_value.CStr() < 10000; header_value_ptr++) {}
		
		if (*header_value_ptr)
		{
			OpString8 copied_value;
			RETURN_IF_ERROR(copied_value.Set(header_value.CStr(), header_value_ptr - header_value.CStr()));
			return SetValue(copied_value, do_quote_pair_decode, parseerror_start, parseerror_end);
		}
	}

	if (IsAddressHeader())
	{
		return SetAddresses(header_value, parseerror_start, parseerror_end);
	}

    OP_STATUS ret;
	OpString8 quotepair_decoded_value;
	if (do_quote_pair_decode && HeaderSupportsQuotePair())
	{
		if ((ret=quotepair_decoded_value.Set(header_value))!=OpStatus::OK ||
			(ret=OpMisc::DecodeQuotePair(quotepair_decoded_value))!=OpStatus::OK)
		{
			return ret;
		}
		parsererror_possible &= quotepair_decoded_value.Compare(header_value)==0;
	}

	OpString8 corrected_header_value;
	if (quotepair_decoded_value.HasContent())
	{
		ret = UnfoldHeader(corrected_header_value, quotepair_decoded_value);
		parsererror_possible &= corrected_header_value.Compare(quotepair_decoded_value)==0;
	}
	else
	{
		ret = UnfoldHeader(corrected_header_value, header_value);
		parsererror_possible &= corrected_header_value.Compare(header_value)==0;
	}
	if (ret != OpStatus::OK)
		return ret;

	OpString8 charset;
	OpString16 header_value16;
	BOOL warning_found, error_found;
	if ((ret=GetCharset(charset))!=OpStatus::OK ||
		(ret=OpQP::Decode(corrected_header_value, header_value16, charset, warning_found, error_found, m_allow_incoming_quotedstring_qp))!=OpStatus::OK)
	{
		return ret;
	}
	parsererror_possible &= header_value16.Compare(corrected_header_value);
	return SetValue(header_value16, FALSE, parsererror_possible?parseerror_start:NULL, parsererror_possible?parseerror_end:NULL);
}

OP_STATUS Header::SetValue(const OpStringC16& header_value, BOOL do_quote_pair_decode, int* parseerror_start, int* parseerror_end)
{
	BOOL parsererror_possible = TRUE;
	if (parseerror_start)
		*parseerror_start=0;

	if (parseerror_end)
		*parseerror_end=0;

	m_time_utc = 0;
	ClearAddressList();
	ClearStringList();

	// This is for avoiding freezing in really big headers like eg. TO: header with 2000 mail addresses
	// Should really be done in a better way
	const uni_char* header_value_ptr = header_value.CStr();

	if (header_value_ptr)
	{
		for (; *header_value_ptr && header_value_ptr - header_value.CStr() < 10000; header_value_ptr++) {}
		
		if (*header_value_ptr)
		{
			OpString16 copied_value;
			RETURN_IF_ERROR(copied_value.Set(header_value.CStr(), header_value_ptr - header_value.CStr()));
			return SetValue(copied_value, do_quote_pair_decode, parseerror_start, parseerror_end);
		}
	}

    OP_STATUS ret;
	OpString quotepair_decoded_value;
	if (do_quote_pair_decode && HeaderSupportsQuotePair() && !IsAddressHeader()) //Address-headers will have to do this within AddAddress
	{
		if ((ret=quotepair_decoded_value.Set(header_value))!=OpStatus::OK ||
			(ret=OpMisc::DecodeQuotePair(quotepair_decoded_value))!=OpStatus::OK)
		{
			return ret;
		}
		parsererror_possible &= quotepair_decoded_value.Compare(header_value)==0;
	}

	if (quotepair_decoded_value.HasContent())
	{
		ret = UnfoldHeader(m_value, quotepair_decoded_value);
		parsererror_possible &= m_value.Compare(quotepair_decoded_value)==0;
	}
	else
	{
		ret = UnfoldHeader(m_value, header_value);
		parsererror_possible &= m_value.Compare(header_value)==0;
	}
	if (ret != OpStatus::OK)
		return ret;

    if (IsDateHeader())
	{
        //If parsing fails, it will return 19700101 000000 UTC
        tm time_data;
        memset(&time_data, 0, sizeof(tm));

        const uni_char* date_buffer = m_value.CStr();
        if (!date_buffer)
		{
			m_value.Empty();
            return OpStatus::OK;
		}

        const uni_char* temp_ptr = NULL;
        const uni_char* temp_temp_ptr = OpMisc::uni_rfc822_strchr(date_buffer, ','); //Find dayname date separator
        while (temp_temp_ptr) //Find the last occurrence of ','
        {
            temp_ptr = temp_temp_ptr;
            temp_temp_ptr = OpMisc::uni_rfc822_strchr(temp_temp_ptr+1, ',');
        }

        temp_temp_ptr = OpMisc::uni_rfc822_strchr(temp_ptr?temp_ptr:date_buffer, ';'); //Find date separator
        while (temp_temp_ptr) //Find the last occurrence of ';'
        {
            temp_ptr = temp_temp_ptr;
            temp_temp_ptr = OpMisc::uni_rfc822_strchr(temp_temp_ptr+1, ';');
        }

        if (temp_ptr == NULL)
            temp_ptr = date_buffer; //date_buffer start equals text start
        else
            temp_ptr++; //Skip ',' (or ';')

        while(*temp_ptr && (*temp_ptr<'0' || *temp_ptr>'9')) temp_ptr++; //Some clients send the date as "Tue 19 Nov 2002 16:43:49". In this case (without a ','), temp_ptr points to the start of the string, and skipping only spaces isn't enough

        time_data.tm_mday = uni_atoi(temp_ptr); //Parse day

		if (time_data.tm_mday>=1900) //ISO-8601
		{
			size_t i;
			//Parse date
			if (time_data.tm_mday>9999) //YYYYMMDD instead of YYYY-MM-DD?
			{
				time_data.tm_mday = 0; //Reset this, it wasn't the date anyway
				for (i=0; i<4; i++) time_data.tm_year=time_data.tm_year*10+*(temp_ptr++)-'0';
				for (i=0; i<2; i++) if (*temp_ptr) time_data.tm_mon=time_data.tm_mon*10+*(temp_ptr++)-'0';
				for (i=0; i<2; i++) if (*temp_ptr) time_data.tm_mday=time_data.tm_mday*10+*(temp_ptr++)-'0';
			}
			else
			{
				time_data.tm_year = time_data.tm_mday - 1900;
				while(*temp_ptr>='0' && *temp_ptr<='9') temp_ptr++; //Go over the digits
				while(*temp_ptr && (*temp_ptr<'0' || *temp_ptr>'9')) temp_ptr++;

				time_data.tm_mon = uni_atoi(temp_ptr) - 1; //Parse month
				while(*temp_ptr>='0' && *temp_ptr<='9') temp_ptr++; //Go over the digits
				while(*temp_ptr && (*temp_ptr<'0' || *temp_ptr>'9')) temp_ptr++;

				time_data.tm_mday = uni_atoi(temp_ptr); //Parse date
				while(*temp_ptr>='0' && *temp_ptr<='9') temp_ptr++;
			}

			BOOL is_timezone = FALSE;
			while (*temp_ptr && (*temp_ptr<'0' || *temp_ptr>'9') && (is_timezone=(*temp_ptr!='T' && IsStartOfTimezone(temp_ptr)))==FALSE) temp_ptr++; //Skip whitespace, T is for Time, not the military timezone Tango

			//Parse time
			if (!is_timezone)
			{
				time_data.tm_hour = uni_atoi(temp_ptr); //Parse hours
				if (time_data.tm_hour>99)
				{
					time_data.tm_hour = 0;
					for (i=0; i<2; i++) time_data.tm_hour=time_data.tm_hour*10+*(temp_ptr++)-'0';

					if (!IsStartOfTimezone(temp_ptr))
						for (i=0; i<2; i++) time_data.tm_min=time_data.tm_min*10+*(temp_ptr++)-'0';

					if (!IsStartOfTimezone(temp_ptr))
						for (i=0; i<2; i++) time_data.tm_sec=time_data.tm_sec*10+*(temp_ptr++)-'0';
				}
				else
				{
					while(*temp_ptr>='0' && *temp_ptr<='9') temp_ptr++; //Skip hours, already parsed
					while(*temp_ptr && (*temp_ptr<'0' || *temp_ptr>'9') && (is_timezone=IsStartOfTimezone(temp_ptr))==FALSE) temp_ptr++;

					if (!is_timezone)
					{
						time_data.tm_min = uni_atoi(temp_ptr); //Parse minutes
						while(*temp_ptr>='0' && *temp_ptr<='9') temp_ptr++;
						while(*temp_ptr && (*temp_ptr<'0' || *temp_ptr>'9') && (is_timezone=IsStartOfTimezone(temp_ptr))==FALSE) temp_ptr++;
					}

					if (!is_timezone)
					{
						time_data.tm_sec = uni_atoi(temp_ptr); //Parse seconds
						while(*temp_ptr>='0' && *temp_ptr<='9') temp_ptr++;
					}
				}
			}

			//Find start of timezone
			while(*temp_ptr && !IsStartOfTimezone(temp_ptr)) temp_ptr++;
		}
		else //RFC822
		{
			while(*temp_ptr>='0' && *temp_ptr<='9') temp_ptr++; //Go over the digits and over the following spaces
			while(*temp_ptr && ((*temp_ptr<'0' || *temp_ptr>'9') && !uni_isalpha(*temp_ptr))) temp_ptr++; //The standard says 'day month year', but Lotus sends out 'day-month-year', so I would also expect 'day/month-year' etc being sent.

			time_data.tm_mon = 0;
			while(time_data.tm_mon != 12) //Parse the month name
			{
				if(uni_strnicmp(temp_ptr, &monthNames[time_data.tm_mon][0], 3) == 0)
					break;
				time_data.tm_mon++;
			}

			if(time_data.tm_mon == 12) //If 12 then we have an invalid date-value
			{
	//            OP_ASSERT(0); //Removed this assert, as it happens all the time with mail sent from Lotus Notes
				m_value.Empty();
				return OpStatus::OK;
			}

			temp_ptr += 3; //Go after the month and find end of spaces
			while(*temp_ptr && ((*temp_ptr<'0' || *temp_ptr>'9') && !uni_isalpha(*temp_ptr))) temp_ptr++;

			time_data.tm_year = uni_atoi(temp_ptr); //Get the year

			int temp_count = 0;
			while(temp_ptr[temp_count]>='0' && temp_ptr[temp_count]<='9') temp_count++; //Count the digits and if they are 4 then sub 1900

			if (temp_count == 4)
				time_data.tm_year -= 1900;
			else if (temp_count == 2)
			{
				if (time_data.tm_year < 50)
					time_data.tm_year += 100;
			}
			else
			{
				OP_ASSERT(0);
				m_value.Empty();
				return OpStatus::OK;
			}

			temp_ptr += temp_count; //Go to next char and find end of the spaces
			while(*temp_ptr == ' ') temp_ptr ++;

			time_data.tm_hour = uni_atoi(temp_ptr); //Parse the hour value
			temp_ptr += 3;

			time_data.tm_min = uni_atoi(temp_ptr); //Parse the minutes value
			temp_ptr += 2;

			if(*temp_ptr == ':') //Parse second if available
			{
				time_data.tm_sec = uni_atoi(temp_ptr + 1);
				temp_ptr += 3;
			}

			while(*temp_ptr == ' ') temp_ptr ++;
		}

        m_time_utc = LocaltimetoUTC(&time_data, temp_ptr);
		m_value.Empty();
        return OpStatus::OK;
	}
	else if (IsAddressHeader())
	{
		m_value.Strip(FALSE, TRUE);
		ret = AddAddresses(m_value, do_quote_pair_decode, parseerror_start, parseerror_end);
		m_value.Empty();
		return ret;
	}
	else if (IsMessageIdHeader() || IsReferencesHeader())
	{
		if (m_value.IsEmpty())
			return OpStatus::OK;

		const uni_char* message_id_start = OpMisc::uni_rfc822_strchr(m_value.CStr(), '<');
		const uni_char* message_id_end;
		while (message_id_start)
		{
			message_id_end = OpMisc::uni_rfc822_strchr(message_id_start, '>');
			if (message_id_end)
			{
				OpString8 message_id;
				if ((ret=message_id.Set(message_id_start, message_id_end-message_id_start+1))!=OpStatus::OK ||
					(ret=AddMessageId(message_id))!=OpStatus::OK)
				{
					if (parsererror_possible && parseerror_start)
						*parseerror_start = message_id_start-m_value.CStr();

					if (parsererror_possible && parseerror_end)
						*parseerror_end = message_id_end+1-m_value.CStr();

					m_value.Empty();
					return ret;
				}
			}

			message_id_start = message_id_end ? OpMisc::uni_rfc822_strchr(message_id_end, '<') : NULL;
		}

		m_value.Empty();
		return OpStatus::OK;
	}
	else if (IsNewsgroupsHeader())
	{
		if (m_value.IsEmpty())
			return OpStatus::OK;

		const uni_char* newsgroup_start = m_value.CStr();
		const uni_char* newsgroup_end;
		while (newsgroup_start)
		{
			while (*newsgroup_start==' ' || *newsgroup_start==',') newsgroup_start++;
			if (*newsgroup_start==0) //Reached end of string
				break;

			newsgroup_end = uni_strchr(newsgroup_start, ',');
			if (!newsgroup_end)
			{
				newsgroup_end = newsgroup_start + uni_strlen(newsgroup_start) - 1;
			}

			while ((*newsgroup_end==',' || *newsgroup_end==' ') && newsgroup_end>newsgroup_start) newsgroup_end--;

			OpString8 newsgroup;
			if ((ret=newsgroup.Set(newsgroup_start, newsgroup_end-newsgroup_start+1))!=OpStatus::OK ||
				(ret=AddNewsgroup(newsgroup))!=OpStatus::OK)
			{
				if (parsererror_possible && parseerror_start)
					*parseerror_start = newsgroup_start-m_value.CStr();

				if (parsererror_possible && parseerror_end)
					*parseerror_end = newsgroup_end+1-m_value.CStr();

				m_value.Empty();
				return ret;
			}

			newsgroup_start = newsgroup_end+1;
		}

		m_value.Empty();
		return OpStatus::OK;
	}
	else //Accept value as-is
	{
		return OpStatus::OK;
	}
}

OP_STATUS Header::SetValue(UINT32 value)
{
	OP_ASSERT(m_time_utc==0 && m_address_list.Empty());
	m_time_utc = 0;
	ClearAddressList();
	ClearStringList();

    uni_char* buffer = m_value.Reserve(15);
    if (!buffer)
         return OpStatus::ERR_NO_MEMORY;

    uni_sprintf(buffer, UNI_L("%d"), value);
    return OpStatus::OK;
}

void Header::SetDateValue(time_t time_utc)
{
	OP_ASSERT(m_value.IsEmpty() && m_address_list.Empty());
	m_value.Empty();
	ClearAddressList();
	ClearStringList();

	if ((m_time_utc=time_utc) == 0) //Make sure we set a valid time (not 19700101)
	{
		m_time_utc = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->CurrentTime();
	}
}

time_t Header::LocaltimetoUTC(struct tm* time, const uni_char* tz, BOOL inverse) const
{
	if (!tz)
		return (time_t)-1; //According to documentation of mktime

	if (*tz)
    {
        int th=0, tm=0;

		int relative_to_greenwich = (*tz=='-') ? -1 :
									((*tz=='+') ? 1 : 0);
		if (relative_to_greenwich != 0)
		{
			tz++; //Skip '+' or '-'

			size_t tz_len = uni_strlen(tz);
			if (tz_len>=2)
			{
				th = (*tz-'0')*10 + (*(tz+1)-'0');
			}

			size_t hour_time_separators = tz_len>=3 && (*(tz+2)<'0' || *(tz+2)>'9') ? 1 : 0;
			if (tz_len>=(4+hour_time_separators))
			{
				tm = (*(tz+2+hour_time_separators)-'0')*10 + (*(tz+3+hour_time_separators)-'0');
			}

			time->tm_hour -= relative_to_greenwich*th;
			time->tm_min -= relative_to_greenwich*tm;
		}
		else
		{
			for (size_t i = 0; i < sizeof(TimeZones)/sizeof(TimeZones[0]); i++)
			{
				if (uni_strcmp(tz, TimeZones[i].tz)==0)
				{
					time->tm_hour -= TimeZones[i].offset;
					break;
				}
			}
		}

		/*
		 * mktime(time) does an adjustment for local timezone. Do the opposite of the adjustment first
		 * so we get an unadjusted time
		 */
		long timezone = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->CurrentTimezone()/60;
        time->tm_min -= (inverse ? -timezone : timezone);
	}
	return mktime(time);
}

BOOL Header::IsStartOfTimezone(const uni_char* string) const
{
	if (!string)
		return FALSE;

	if (*string>='0' && *string<='9') //We most likely have the start of a numeric timezone
		return TRUE;

	if (*string=='+' || *string=='-')
		return TRUE; //We most likely have the start of a numeric timezone

	size_t i;
	for (i=0; i<sizeof(TimeZones)/sizeof(TimeZones[0]); i++)
	{
		if (uni_strcmp(string, TimeZones[i].tz)==0)
			return TRUE; //We have a alphanumeric timezone
	}

	return FALSE;
}

OP_STATUS Header::AddAddress(const OpStringC16& name, const OpStringC16& address, const OpStringC16& comment, const OpStringC16& group)
{
#ifdef ASSERT_ON_EMPTY_ADDRESS
	OP_ASSERT(address.HasContent());
#endif
	if (address.IsEmpty())
		return OpStatus::OK;

	OP_STATUS ret;
	OpString16 address_without_brackets;
	BOOL use_address_without_brackets_string = FALSE;
	if (*address.CStr()=='<') //Remove angle-brackets from address
	{
		const uni_char* address_end = address.CStr()+address.Length()-1;
		if (*address_end=='>')
		{
			if ((ret=address_without_brackets.Set(address.CStr()+1, address.Length()-2)) != OpStatus::OK)
				return ret;

			use_address_without_brackets_string = TRUE;
		}
	}

	OpString address_localpart;
	OpString address_domain;
	if (use_address_without_brackets_string)
	{
		ret = OpMisc::SplitMailAddress(address_without_brackets, address_localpart, address_domain);
	}
	else
	{
		ret = OpMisc::SplitMailAddress(address, address_localpart, address_domain);
	}
	if (ret != OpStatus::OK)
		return ret;

	return AddAddress(name, address_localpart, address_domain, comment, group);
}

OP_STATUS Header::AddAddress(const OpStringC16& name, const OpStringC16& address_localpart, const OpStringC16& address_domain, const OpStringC16& comment, const OpStringC16& group)
{
	OP_ASSERT(IsAddressHeader());
	if (!IsAddressHeader())
		return OpStatus::ERR;

	// Inline Header::AddAddress(const From& address) to avoid the assignment operator which does string duplication
	OpAutoPtr<From> from (OP_NEW(From, ()));
	RETURN_OOM_IF_NULL(from.get());

#ifdef ASSERT_ON_EMPTY_ADDRESS
	OP_ASSERT(address_localpart.HasContent());
#endif
	RETURN_IF_ERROR(from->SetName(name));
	RETURN_IF_ERROR(from->SetAddress(address_localpart, address_domain));
	RETURN_IF_ERROR(from->SetComment(comment));
	RETURN_IF_ERROR(from->SetGroup(group));

	from.release()->Into(&m_address_list);

	return OpStatus::OK;
}

OP_STATUS Header::AddAddress(const From& address)
{
	OP_ASSERT(IsAddressHeader());
	if (!IsAddressHeader())
		return OpStatus::ERR;

	//Make a new copy
	From* new_address = OP_NEW(From, ());
	if (!new_address)
		return OpStatus::ERR_NO_MEMORY;

	*new_address = address;

    new_address->Into(&m_address_list);

	return OpStatus::OK;
}

OP_STATUS Header::AddAddresses(const OpStringC8& addresses, int* parseerror_start, int* parseerror_end)
{
	BOOL parsererror_possible = TRUE;
	if (parseerror_start)
		*parseerror_start=0;

	if (parseerror_end)
		*parseerror_end=0;

	if (addresses.IsEmpty())
		return OpStatus::OK;

	//addresses should be an RFC2822 'address-list'
    OP_STATUS ret;
	OpString8 charset;
	BOOL warning_found, error_found;
	OpString8 quotepair_decoded_addresses;
	OpString addresses16;
	OpString address;

	if ((ret=GetCharset(charset))!=OpStatus::OK ||
		(ret=OpQP::Decode(addresses, addresses16, charset, warning_found, error_found, m_allow_incoming_quotedstring_qp, TRUE))!=OpStatus::OK)
	{
		return ret;
	}
	parsererror_possible &= addresses16.Compare(addresses)==0;

	addresses16.Strip();
	return AddAddresses(addresses16, TRUE, parsererror_possible?parseerror_start:NULL, parsererror_possible?parseerror_end:NULL);
}

OP_STATUS Header::AddAddresses(const OpStringC16& addresses, BOOL do_quote_pair_decode, int* parseerror_start, int* parseerror_end)
{
	if (parseerror_start)
		*parseerror_start=0;

	if (parseerror_end)
		*parseerror_end=0;

	if (addresses.IsEmpty())
		return OpStatus::OK;

	//addresses should be a unicode, quotepair-decoded version of RFC2822-compatible 'address-list'
    OP_STATUS ret;
	const uni_char* address_start = addresses.CStr();
	const uni_char* address_end_group;
	const uni_char* address_end_address;
	const uni_char* address_end;
	OpString address;
	OpString group;
	BOOL in_group = FALSE;

	while (address_start)
	{
		while (*address_start==' ') address_start++;
		if (!*address_start)
			break;

		address_end_group = OpMisc::uni_rfc822_strchr(address_start, in_group ? ';' : ':');

		// Try and find the comma that marks the end of the address
		address_end_address = OpMisc::uni_rfc822_strchr(address_start, ',');
		// If nothing is found set it to the end of the string
		if (!address_end_address) 
			address_end_address=address_start+uni_strlen(address_start);

		// If not in a group then the address separator could also be a semi-colon. So if there is a 
		// semi-colon closer to the start of the address than the comma, then take that as the separator
		if (!in_group)
		{
			const uni_char* semicolon = OpMisc::uni_rfc822_strchr(address_start, ';');
			// Only if we find a semi colon should we both to check if it's closer
			if (semicolon)
				address_end_address = min(address_end_address, semicolon);
		}

		address_end = (address_end_group && address_end_group<address_end_address) ? address_end_group : address_end_address;

		if (!in_group && address_end_group==address_end)
		{
			if ((ret=group.Set(address_start, address_end_group-address_start)) != OpStatus::OK)
				return ret;

			group.Strip();
			if (do_quote_pair_decode && (ret=OpMisc::DecodeQuotePair(group))!=OpStatus::OK)
				return ret;

			in_group = TRUE;
			address_start = address_end_group+1;
			address_end = OpMisc::uni_rfc822_strchr(address_start, ';');
			if (!address_end || address_end_address<address_end) address_end=address_end_address;
		}

		//We have an address!
		if ((ret=address.Set(address_start, address_end-address_start))!=OpStatus::OK ||
			(ret=AddAddress(address, group, do_quote_pair_decode))!=OpStatus::OK)
		{
			if (parseerror_start)
				*parseerror_start = address_start-addresses.CStr();

			if (parseerror_end)
				*parseerror_end = address_end-addresses.CStr();

			return ret;
		}

		if (address_end_group==address_end && *address_end_group==';')
		{
			in_group = FALSE;
			group.Empty();
		}

		address_start = (*address_end_address) ? address_end_address+1 : NULL; //Bail out after last address
	}
	return OpStatus::OK;
}
	//address-list		= (address *("," address)) / obs-addr-list
	//address			= mailbox / group
	//obs-addr-list		= 1*([address] [CFWS] "," [CFWS]) [address]
	//mailbox			= name-addr / addr-spec
	//group				= display-name ":" [mailbox-list / CFWS] ";" [CFWS]
	//name-addr			= [display-name] angle-addr
	//addr-spec			= local-part "@" domain
	//display-name		= phrase
	//mailbox-list		= (mailbox *("," mailbox)) / obs-mbox-list
	//angle-addr		= [CFWS] "<" addr-spec ">" [CFWS] / obs-angle-addr
	//local-part		= dot-atom / quoted-string / obs-local-part
	//domain			= dot-atom / domain-literal / obs-domain
	//phrase			= 1*word / obs-phrase
	//obs-mbox-list		= 1*([mailbox] [CFWS] "," [CFWS]) [mailbox]
	//obs-angle-addr	= [CFWS] "<" [obs-route] addr-spec ">" [CFWS]
	//dot-atom			= [CFWS] dot-atom-text [CFWS]
	//quoted-string		= [CFWS] DQUOTE *([FWS] qcontent) [FWS] DQUOTE [CFWS]
	//obs-local-part	= word *("." word)
	//domain-literal	= [CFWS] "[" *([FWS] dcontent) [FWS] "]" [CFWS]
	//obs-domain		= atom *("." atom)
	//word				= atom / quoted-string
	//obs-phrase		= word *(word / "." / CFWS)
	//obs-route			= [CFWS] obs-domain-list ":" [CFWS]
	//dot-atom-text		= 1*atext *("." 1*atext)
	//qcontent			= qtext / quoted-pair
	//dcontent			= dtext / quoted-pair
	//atom				= [CFWS] 1*atext [CFWS]
	//obs-domain-list	= "@" domain *(*(CFWS / "," ) [CFWS] "@" domain)
	//atext				= ALPHA / DIGIT / "!" / "#" / "$" / "%" / "&" / "'" / "*" / "+" /
	//					  "-" / "/" / "=" / "?" / "^" / "_" / "`" / "{" / "|" / "}" / "~"
	//qtext				= NO-WS-CTL / %d33 / %d35-91 / %d93-126
	//quoted-pair		= ("\" text) / obs-qp
	//text				= %d1-9 / %d11 / %d12 / %d14-127 / obs-text
	//obs-qp			= "\" (%d0-127)
	//obs-text			= *LF *CR *(obs-char *LF *CR)
	//obs-char			= %d0-9 / %d11 / %d12 / %d14-127
OP_STATUS Header::AddAddress(const OpStringC16& rfc822_address, const OpStringC16& rfc822_group, BOOL do_quote_pair_decode) //This is only available as 8-bit, as it expects raw 8-bit RFC822-compatible input
{
	if (rfc822_address.IsEmpty())
		return OpStatus::OK;

	OpString name, mailaddress, comment;

	//rfc822_address should be an RFC2822 'mailbox'
	const uni_char* start_of_atom = rfc822_address.CStr();
	const uni_char* start_of_cfws = NULL;

	OP_STATUS ret;
	OpString atom, comment_atom;
	BOOL is_address;
	while (TRUE)
	{
		if (start_of_atom && *start_of_atom) //Don't iterate on start_of_atom, as we want to append the very last atom, too
		{
			comment_atom.Empty();
			start_of_atom = OpMisc::uni_rfc822_skipCFWS(start_of_atom, comment_atom);
			if (comment_atom.HasContent())
			{
				if (comment.HasContent() && (ret=comment.Append(" "))!=OpStatus::OK)
					return ret;

				if ((ret=comment.Append(comment_atom)) != OpStatus::OK)
					return ret;
			}

			start_of_cfws = OpMisc::uni_rfc822_skip_until_CFWS(start_of_atom);
			
			// if we didn't detect a name, maybe there wasn't any spacing between them?
			if (!*start_of_cfws)
			{
				// look for a '<', that might be the character splitting the name and the address
				const uni_char* first_angle_bracket = uni_strchr(start_of_atom,'<');
				if (first_angle_bracket && first_angle_bracket < start_of_cfws
					&& first_angle_bracket != start_of_atom)
				{
					// if there is a '<' before the end, we should split here
					start_of_cfws = first_angle_bracket;
				}
			}
		}

		if (atom.HasContent()) //If we have parsed an atom, store it now
		{
			is_address = ((!start_of_atom || *start_of_atom==0) && mailaddress.IsEmpty()) || //last atom is most likely mailaddress
						 (*(atom.CStr())=='<' && *(atom.CStr()+atom.Length())=='>') || //angle-addr
						 OpMisc::uni_rfc822_strchr(atom.CStr(), '@'); //addr-spec

			if (is_address) //This is (at least according to the RFC most likely) an address
			{
				if (mailaddress.HasContent()) //Move what was considered an address, into name
				{
					if (name.HasContent() && (ret=name.Append(" "))!=OpStatus::OK)
						return ret;

					if ((ret=name.Append(mailaddress)) != OpStatus::OK)
						return ret;
				}

				if ((ret=OpMisc::StripQuotes(atom))!=OpStatus::OK ||
					(ret=mailaddress.Set(atom))!=OpStatus::OK)
				{
					return ret;
				}
			}
			else
			{
				if (name.HasContent() && (ret=name.Append(" "))!=OpStatus::OK)
					return ret;

				if ((ret=OpMisc::StripQuotes(atom))!=OpStatus::OK ||
					(ret=name.Append(atom)) != OpStatus::OK)
					return ret;
			}
		}

		if (!start_of_atom || *start_of_atom==0)
			break;

		if ((ret=atom.Set(start_of_atom, start_of_cfws-start_of_atom)) != OpStatus::OK)
			return ret;

		start_of_atom = start_of_cfws;
	}

	if (name.HasContent() && mailaddress.IsEmpty())
	{
		if ((ret=mailaddress.Set(name)) != OpStatus::OK)
			return ret;

		name.Empty();
	}

	if (comment.HasContent() && name.IsEmpty())
	{
		if ((ret=Header::From::GetCommentStringWithoutCchar(comment, name)) != OpStatus::OK)
			return ret;

		comment.Empty();
	}

	if (do_quote_pair_decode)
	{
		if ((ret=OpMisc::DecodeQuotePair(name))!=OpStatus::OK ||
			(ret=OpMisc::DecodeQuotePair(mailaddress))!=OpStatus::OK ||
			(ret=OpMisc::DecodeQuotePair(comment))!=OpStatus::OK)
		{
			return ret;
		}
	}
	//mailaddress should have stripped angle-brachets here, but AddAddress wil do it anyways

	return AddAddress(name, mailaddress, comment, rfc822_group);
}

OP_STATUS Header::DeleteAddress(const OpStringC16& name /*Can be empty */, const OpStringC16& address)
{
    if (IsAddressHeader())
    {
		OP_STATUS ret;
#ifdef ASSERT_ON_EMPTY_ADDRESS
		OP_ASSERT(address.HasContent());
#endif

        const From* from = GetFirstAddress();
        while (from)
        {
            if (from->CompareAddress(address)==0 && (name.IsEmpty() || from->CompareName(name)==0))
            {
                From* from_to_delete = const_cast<From*>(from);
				from = (From*)(from->Suc());
                if ((ret=DeleteAddress(from_to_delete)) != OpStatus::OK)
					return ret;
            }
        }
		return OpStatus::OK;
    }
    OP_ASSERT(0);
    return OpStatus::ERR;
}

OP_STATUS Header::DeleteAddress(From* address)
{
    if (IsAddressHeader())
    {
		if (!address)
			return OpStatus::ERR_NULL_POINTER;

		address->Out();
		OP_DELETE(address);

		return OpStatus::OK;
	}
    OP_ASSERT(0);
    return OpStatus::ERR;
}

OP_STATUS Header::AddReplyPrefix()
{
    if (IsSubjectHeader())
    {
        if (m_value.HasContent() && uni_strnicmp(m_value.CStr(), UNI_L("Re: "), 4)==0)
        {
			op_memcpy(m_value.CStr(), UNI_L("Re: "), 4 * sizeof(uni_char));
			return OpStatus::OK;
        }
        else
		{
            return m_value.Insert(0, UNI_L("Re: "));
		}
    }
    OP_ASSERT(0);
    return OpStatus::ERR;
}

OP_STATUS Header::AddForwardPrefix()
{
    if (IsSubjectHeader())
    {
        if (m_value.HasContent() && uni_strnicmp(m_value.CStr(), UNI_L("Fwd: "), 5)==0)
        {
			op_memcpy(m_value.CStr(), UNI_L("Fwd: "), 5 * sizeof(uni_char));
			return OpStatus::OK;
        }
        else
		{
            return m_value.Insert(0, UNI_L("Fwd: "));
		}
    }
    OP_ASSERT(0);
    return OpStatus::ERR;
}

OP_STATUS Header::GetValueWithoutPrefix(OpString16& header_value)
{
    if (IsSubjectHeader())
    {
        const uni_char* start = m_value.CStr();
        if (!start)
        {
            header_value.Empty();
            return OpStatus::OK;
        }

        BOOL done = FALSE;
        while (!done)
        {
            done = TRUE;
            if (uni_strnicmp(start, UNI_L("Re: "), 4)==0)
            {
                start+=4;
                done = FALSE;
            }
            if (uni_strnicmp(start, UNI_L("Fwd: "), 5)==0)
            {
                start+=5;
                done = FALSE;
            }
        }
        return header_value.Set(start);
    }
    OP_ASSERT(0);
    return OpStatus::ERR;
}

OP_STATUS Header::GenerateNewMessageId()
{
    if (IsMessageIdHeader())
    {
        //Find account
        if (!m_message)
            return OpStatus::ERR_NULL_POINTER;

        Account* account = m_message->GetAccountPtr();
        if (!account)
            return OpStatus::ERR_NULL_POINTER;

        //Get settings from account
        OP_STATUS ret;
		OpString  fqdn;
        OpString8 fqdn8;
        if ((ret=account->GetFQDN(fqdn)) != OpStatus::OK)
            return ret;

		if ((ret=OpMisc::ConvertToIMAAAddress(UNI_L(""), fqdn, fqdn8)) != OpStatus::OK)
			if ((ret=fqdn8.Set("error.opera.illegal")) != OpStatus::OK)
				return ret;

        if (fqdn8.IsEmpty())
            return OpStatus::ERR;

        OpString8 personalization;
        if ((ret=account->GetPersonalizationString(personalization)) != OpStatus::OK)
            return ret;

        // Message-ID format: <op.[seconds since 1970][millisecond|random][md5 of From:|random][personalization]@fqdn8>
        time_t tmp_time = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->CurrentTime();

        UINT32 time_hash = (UINT32)tmp_time; //Seconds since 1970
        UINT16 ext_time_hash;                  //milliseconds
#if defined(WIN32) && !defined(WIN_CE)
        ext_time_hash = (UINT16)(g_op_time_info->GetTimeUTC())&0x03FF;
#else
        srand(time_hash);
        ext_time_hash = (UINT16)(rand()&0x03FF); //Just generate a random millisecond
#endif
		OpString8* value8 = OP_NEW(OpString8, ());
		if (!value8)
			return OpStatus::ERR_NO_MEMORY;

        if ((ret=value8->Set("<op.")) != OpStatus::OK)
            return ret;

        char base36_time[9];
        BYTE tmp_base36;
		int i;
        for (i=0; i<8; i++)
        {
            if (i>1)
            {
                tmp_base36 = (BYTE)(time_hash%36);
                time_hash/=36;
            }
            else
            {
                tmp_base36 = (BYTE)(ext_time_hash%36);
                ext_time_hash/=36;
            }
            base36_time[7-i] = (tmp_base36<26)?(char)('a'+tmp_base36):(char)('0'+(tmp_base36-26));
        }
        base36_time[8]=0;
        if ((ret=value8->Append(base36_time)) != OpStatus::OK)
            return ret;

        UINT32 extra_uid = 0;
        if (m_message)
        {
            OpString8 from;
            if ((ret=m_message->GetHeaderValue(Header::FROM, from)) != OpStatus::OK)
                return ret;
            if (from.HasContent())
            {
                OpString8 from_md5;
                if ((ret=OpMisc::CalculateMD5Checksum(from.CStr(), from.Length(), from_md5)) != OpStatus::OK)
                    return ret;

                if (from_md5.Length()>=8)
                {
                    char* buffer = (char*)(from_md5.CStr());
                    buffer[8]=0;
					unsigned int dummy_int;
                    sscanf(buffer, "%x", &dummy_int);
					extra_uid = dummy_int;
                }
            }
        }
        if (!extra_uid)
        {
            extra_uid = rand();
        }
        for (i=0; i<6; i++)
        {
            tmp_base36 = (BYTE)(extra_uid%36);
            extra_uid/=36;
            base36_time[5-i] = (tmp_base36<26)?(char)('a'+tmp_base36):(char)('0'+(tmp_base36-26));
        }
        base36_time[6]=0;

        //Optimization: Make sure value8 has room for seperators and values, to avoid multiple calls to Grow()
        if (!value8->Reserve(value8->Length()+strlen(base36_time)+personalization.Length()+1+fqdn8.Length()+1+1))
            return OpStatus::ERR_NO_MEMORY;

        if ((ret=value8->Append(base36_time))!=OpStatus::OK ||
			(ret=value8->Append(personalization))!=OpStatus::OK ||
			(ret=value8->Append("@"))!=OpStatus::OK ||
			(ret=value8->Append(fqdn8))!=OpStatus::OK ||
			(ret=value8->Append(">"))!=OpStatus::OK)
		{
			return ret;
		}

		ClearStringList();
		return m_string_list.Add(value8);
    }
    OP_ASSERT(0);
    return OpStatus::ERR;
}

OP_STATUS Header::GetMessageId(OpString8& message_id, INT16 index_from_end) const //index_from_end is 0 for last element, 1 for the last-but-one, etc.  (-1 for first element)
{
    if (IsReferencesHeader() || IsMessageIdHeader())
    {
		return GetStringByIndexFromEnd(message_id, index_from_end);
    }
    OP_ASSERT(0);
    return OpStatus::ERR;
}

OP_STATUS Header::AddMessageId(const OpStringC8& message_id)
{
    if (IsReferencesHeader() || IsMessageIdHeader())
    {
        const char* buffer = message_id.CStr();
        if (!buffer)
            return OpStatus::OK;

        const char* buffer_start = strchr(buffer, '<');
        const char* buffer_end = strchr(buffer, '>');
        if (!buffer_start || !buffer_end || (buffer_start>buffer_end))
            return OpStatus::ERR;

        OP_STATUS ret;
		OpString8* message_id_item = OP_NEW(OpString8, ());
		if (!message_id_item)
			return OpStatus::ERR_NO_MEMORY;

		if ((ret=message_id_item->Set(buffer_start, buffer_end-buffer_start+1)) != OpStatus::OK)
		{
			OP_DELETE(message_id_item);
			return ret;
		}

		if (message_id_item->IsEmpty())
		{
			OP_DELETE(message_id_item);
			return OpStatus::ERR_OUT_OF_RANGE;
		}

        if (FindStringIndex(*message_id_item)>=0)
		{
			OP_DELETE(message_id_item);
			return OpStatus::OK; //Already exists in list
		}

		return m_string_list.Add(message_id_item);
    }
    OP_ASSERT(0);
    return OpStatus::ERR;
}

OP_STATUS Header::GetNewsgroup(OpString8& newsgroup, UINT16 index_from_start)
{
	if (IsNewsgroupsHeader())
	{
		return GetStringByIndexFromStart(newsgroup, index_from_start);
	}
	OP_ASSERT(0);
	return OpStatus::ERR;
}


OP_STATUS Header::AddNewsgroup(const OpStringC8& newsgroup)
{
	if (IsNewsgroupsHeader())
	{
		if (newsgroup.IsEmpty())
			return OpStatus::ERR_OUT_OF_RANGE;

		OP_STATUS ret;
		OpString8* newsgroup_item = OP_NEW(OpString8, ());
		if (!newsgroup_item)
			return OpStatus::ERR_NO_MEMORY;

		if ((ret=newsgroup_item->Set(newsgroup)) != OpStatus::OK)
		{
			OP_DELETE(newsgroup_item);
			return ret;
		}

		if (FindStringIndex(*newsgroup_item)>=0)
		{
			OP_DELETE(newsgroup_item);
			return OpStatus::OK; //Already exists in list
		}

		return m_string_list.Add(newsgroup_item);
	}
	OP_ASSERT(0);
	return OpStatus::ERR;
}


OP_STATUS Header::RemoveNewsgroup(const OpStringC8& newsgroup)
{
	if (IsNewsgroupsHeader())
	{
		if (newsgroup.IsEmpty())
			return OpStatus::ERR_OUT_OF_RANGE;

		INT16 index = FindStringIndex(newsgroup);
		if (index>=0) //newsgroup found
		{
			m_string_list.Delete(index);
		}
		return OpStatus::OK;
	}
	OP_ASSERT(0);
	return OpStatus::ERR;
}


OP_STATUS Header::SetName(const OpStringC8& header_name)
{
    if (m_type!=UNKNOWN)
    {
        OP_ASSERT(0);
        return OpStatus::ERR;
    }

    if ((m_message && m_message->IsFlagSet(Message::IS_OUTGOING)) || (!m_message && m_is_outgoing))
    {
        if (!OpMisc::StringConsistsOf(header_name, OpMisc::ALPHA|OpMisc::DIGIT, "-"))
            return OpStatus::ERR;
    }

    return m_headername.Set(header_name);
}

#endif //M2_SUPPORT
