/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/encodings/decoders/inputconverter.h"
#ifndef ENCODINGS_REGTEST
# include "modules/encodings/utility/charsetnames.h"
# include "modules/encodings/decoders/inputconverter.h"
# include "modules/encodings/decoders/identityconverter.h"
# include "modules/encodings/decoders/iso-8859-1-decoder.h"
# include "modules/encodings/decoders/utf7-decoder.h"
# include "modules/encodings/decoders/utf8-decoder.h"
# include "modules/encodings/decoders/utf16-decoder.h"
# ifdef ENCODINGS_HAVE_TABLE_DRIVEN
#  include "modules/encodings/decoders/big5-decoder.h"
#  include "modules/encodings/decoders/big5hkscs-decoder.h"
#  include "modules/encodings/decoders/euc-jp-decoder.h"
#  include "modules/encodings/decoders/euc-kr-decoder.h"
#  include "modules/encodings/decoders/euc-tw-decoder.h"
#  include "modules/encodings/decoders/gbk-decoder.h"
#  include "modules/encodings/decoders/hz-decoder.h"
#  include "modules/encodings/decoders/iso-2022-cn-decoder.h"
#  include "modules/encodings/decoders/iso-2022-jp-decoder.h"
#  include "modules/encodings/decoders/sbcs-decoder.h"
#  include "modules/encodings/decoders/sjis-decoder.h"
# endif
#endif // !ENCODINGS_REGTEST

#if !defined ENCODINGS_REGTEST && defined ENCODINGS_HAVE_TABLE_DRIVEN
/**
 * Create conversion table for converting into UTF-16. Internal helper
 * function for InputConverter::CreateCharConverter(). Requires that the
 * specified encoding already is in canonized form. The input parameter
 * may not be NULL.
 *
 * If you are implementing your own conversion system (that is, have
 * FEATURE_TABLEMANAGER disabled, you will need to implement this
 * method on your own. Please note that there are several decoders that
 * are available without the TableManager that you may want to make use
 * of in your own implementation.
 *
 * See the the documentation of the two CreateCharConverter() methods
 * for details on the return values from this method. None of the
 * parameters can be NULL.
 */
OP_STATUS InputConverter::CreateCharConverter_real(const char *realname,
                                                   InputConverter **obj,
                                                   BOOL allowUTF7 /* = FALSE */)
{
	// Create conversion table converting to UTF-16 (Unicode)
	OpTableManager *tm = g_table_manager; // cache pointer
	if (strni_eq(realname, "UTF-8", 6))
	{
		*obj = OP_NEW(UTF8toUTF16Converter, ());
	}
	else if (strni_eq(realname, "ISO-8859-1", 11) ||
	         strni_eq(realname, "US-ASCII", 9))
	{
		// Most iso-8859-1 pages are actually windows-1252
		if (tm && tm->Has("windows-1252"))
		{
			*obj = OP_NEW(SingleBytetoUTF16Converter, ("windows-1252"));
		}
		else
		{
			*obj = OP_NEW(ISOLatin1toUTF16Converter, ());
		}
	}
	else if (strni_eq(realname, "UTF-16", 6))
	{
		if (strni_eq(realname, "UTF-16BE", 9))
		{
			*obj = OP_NEW(UTF16BEtoUTF16Converter, ());
		}
		else if (strni_eq(realname, "UTF-16LE", 9))
		{
			*obj = OP_NEW(UTF16LEtoUTF16Converter, ());
		}
		else
		{
			// Handles both the generic utf-16 (with BOM); defaults to big
			// endian if no BOM is found
			*obj = OP_NEW(UTF16toUTF16Converter, ());
		}
	}
# ifdef ENCODINGS_HAVE_UTF7
	else if (strni_eq(realname, "UTF-7", 6))
	{
		if (!allowUTF7) return OpStatus::ERR_NOT_SUPPORTED;
		*obj = OP_NEW(UTF7toUTF16Converter, (UTF7toUTF16Converter::STANDARD));
	}
# endif // ENCODINGS_HAVE_UTF7
	else if (strni_eq(realname, "X-USER-DEFINED", 14))
	{
		// CORE-14632: Treat this as a byte stream, just converting each
		// eight-bit value to a sixteen-bit value.
		*obj = OP_NEW(ISOLatin1toUTF16Converter, (TRUE));
	}
	else if (tm)
	{
		// These converters require access to the data tables
# ifdef STRICT_HTML5
		if (strni_eq(realname, "ISO-8859-9", 11) && tm->Has("windows-1254"))
		{
			// Most iso-8859-9 pages are actually windows-1254
			// according to HTML5 section 8.2.2.2
			*obj = OP_NEW(SingleBytetoUTF16Converter, ("windows-1254"));
		}
		else
# endif // STRICT_HTML5
		if (tm->Has(realname))
		{
			*obj = OP_NEW(SingleBytetoUTF16Converter, (realname));
		}
		else if (strni_eq(realname, "ISO-8859-8-I", 13))
		{
			*obj = OP_NEW(SingleBytetoUTF16Converter, ("iso-8859-8-i", "iso-8859-8"));
		}
		else if (strni_eq(realname, "ISO-8859-6-I", 13))
		{
			*obj = OP_NEW(SingleBytetoUTF16Converter, ("iso-8859-6-i", "iso-8859-6"));
		}
# ifdef ENCODINGS_HAVE_CHINESE
		else if (strni_eq(realname, "ISO-2022-CN", 12))
		{
			*obj = OP_NEW(ISO2022toUTF16Converter, (ISO2022toUTF16Converter::ISO2022CN));
		}
		else if (strni_eq(realname, "BIG5", 5))
		{
			*obj = OP_NEW(Big5toUTF16Converter, ());
		}
		else if (strni_eq(realname, "BIG5-HKSCS", 11))
		{
			*obj = OP_NEW(Big5HKSCStoUTF16Converter, ());
		}
		else if (strni_eq(realname, "HZ-GB-2312", 11))
		{
			*obj = OP_NEW(HZtoUTF16Converter, ());
		}
		else if (strni_eq(realname, "GBK", 4))
		{
			// Bug#23903: Use GBK converter for EUC-CN/GB2312. GBK is supposed
			// to be a superset of EUC-CN/GB2312 anyway
			*obj = OP_NEW(GBKtoUTF16Converter, (GBKtoUTF16Converter::GBK));
		}
		else if (strni_eq(realname, "GB18030", 7))
		{
			*obj = OP_NEW(GBKtoUTF16Converter, (GBKtoUTF16Converter::GB18030));
		}
		else if (strni_eq(realname, "EUC-TW", 7))
		{
			*obj = OP_NEW(EUCTWtoUTF16Converter, ());
		}
# endif // ENCODINGS_HAVE_CHINESE
# ifdef ENCODINGS_HAVE_JAPANESE
		// ISO 2022-JP decoder can also decode ISO 2022-JP-1
		else if (strni_eq(realname, "ISO-2022-JP", 12))
		{
			*obj = OP_NEW(ISO2022JPtoUTF16Converter, (ISO2022JPtoUTF16Converter::ISO_2022_JP));
		}
		else if (strni_eq(realname, "ISO-2022-JP-1", 14))
		{
			*obj = OP_NEW(ISO2022JPtoUTF16Converter, (ISO2022JPtoUTF16Converter::ISO_2022_JP_1));
		}
		else if (strni_eq(realname, "SHIFT_JIS", 10))
		{
			*obj = OP_NEW(SJIStoUTF16Converter, ());
		}
		else if (strni_eq(realname, "EUC-JP", 7))
		{
			*obj = OP_NEW(EUCJPtoUTF16Converter, ());
		}
# endif // ENCODINGS_HAVE_JAPANESE
# ifdef ENCODINGS_HAVE_KOREAN
		else if (strni_eq(realname, "ISO-2022-KR", 12))
		{
			*obj = OP_NEW(ISO2022toUTF16Converter, (ISO2022toUTF16Converter::ISO2022KR));
		}
		else if (strni_eq(realname, "EUC-KR", 7))
		{
			*obj = OP_NEW(EUCKRtoUTF16Converter, ());
		}
# endif // ENCODINGS_HAVE_KOREAN
		else
		{
			// We know this encoding, but cannot create a decoder for it.
			// This usually means that someone has stolen our data tables,
			// or that it has been disabled using the feature system.
			return OpStatus::ERR_NOT_SUPPORTED;
		}
	}

	if (!*obj) return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret = (*obj)->Construct();
	if (!OpStatus::IsSuccess(ret)) {
		OP_DELETE(*obj);
		*obj = NULL;
	}
	return ret;
}
#endif // !ENCODINGS_REGTEST && ENCODINGS_HAVE_TABLE_DRIVEN

#ifndef ENCODINGS_REGTEST
/* static */
OP_STATUS InputConverter::CreateCharConverter(const char *charset,
                                              InputConverter **obj,
                                              BOOL allowUTF7 /* = FALSE */)
{
	if (!obj)
		return OpStatus::ERR_NULL_POINTER;

	// Bug#193696: Make sure the output pointer is NULL if this call fails.
	*obj = NULL;

	const char *canonized = g_charsetManager->GetCanonicalCharsetName(charset);
	if (canonized)
		return CreateCharConverter_real(canonized, obj, allowUTF7);
	else
		return OpStatus::ERR_OUT_OF_RANGE;
}

# ifdef ENCODINGS_HAVE_MIB_SUPPORT
/* static */
OP_STATUS InputConverter::CreateCharConverter(int mibenum,
                                              InputConverter **obj,
                                              BOOL allowUTF7 /* = FALSE */)
{
	if (!obj)
		return OpStatus::ERR_NULL_POINTER;

	// Bug#193696: Make sure the output pointer is NULL if this call fails.
	*obj = NULL;

	const char *charset = g_charsetManager->GetCharsetNameFromMIBenum(mibenum);
	if (charset)
		return CreateCharConverter_real(charset, obj, allowUTF7);
	else
		return OpStatus::ERR_OUT_OF_RANGE;
}
# endif // ENCODINGS_HAVE_MIB_SUPPORT

/* static */
OP_STATUS InputConverter::CreateCharConverterFromID(unsigned short id,
                                                    InputConverter **obj,
                                                    BOOL allowUTF7 /* = FALSE */)
{
	if (!obj)
		return OpStatus::ERR_NULL_POINTER;

	// Bug#193696: Make sure the output pointer is NULL if this call fails.
	*obj = NULL;

	const char *canonical = g_charsetManager->GetCanonicalCharsetFromID(id);
	if (!canonical)
		return OpStatus::ERR_OUT_OF_RANGE;

	return CreateCharConverter_real(canonical, obj, allowUTF7);
}
#endif // !ENCODINGS_REGTEST

const char *InputConverter::GetDestinationCharacterSet()
{
	return "utf-16";
}
