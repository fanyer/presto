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
#include "modules/formats/encoder.h"

#include "modules/olddebug/tstdump.h"
#include "modules/encodings/utility/opstring-encodings.h"

#ifdef URL_UPLOAD_QP_SUPPORT
void Header_QuotedPrintable_Parameter::InitL(const OpStringC8 &p_name, const OpStringC8 &p_value, const OpStringC8 &charset, Header_Encoding encoding, BOOL quote_if_not_encoded)
{
	const char *actual_value= p_value.CStr();
	OpString8 encoded_value;
	ANCHOR(OpString8, encoded_value);

	BOOL quote_flag = quote_if_not_encoded;

	if(actual_value && *actual_value)
	{
		do{
			int input_len = (int)op_strlen(actual_value);
			if(encoding != ENCODING_QUOTED_PRINTABLE && encoding != ENCODING_BASE64)
			{
				// Meaning ENCODING_AUTODETECT

				OpStringC8 charset2(charset);
				Header_RFC2231_Parameter::RFC2231_SpecialCharacter specials_type;
				Header_RFC2231_Parameter::CheckRFC2231Special(p_value, specials_type, charset2);

				if ((specials_type == Header_RFC2231_Parameter::RFC2231_QUOTABLE ||
					 specials_type == Header_RFC2231_Parameter::RFC2231_NOT_SPECIAL) && charset2.IsEmpty())
				{
					// Similar to Header_RFC2231_Parameter, don't encode simple parameters that can be quoted instead
					quote_flag = quote_flag || specials_type == Header_RFC2231_Parameter::RFC2231_QUOTABLE;
					break;
				}

				int escapes = CountQPEscapes(actual_value, input_len);
				
				if(escapes)
				{
					if(input_len + escapes*2 > ((input_len +2)/3) * 4)
						encoding = ENCODING_BASE64;
					else
						encoding = ENCODING_QUOTED_PRINTABLE;
				}
				else
					break;
			}
			
			Mime_EncodeTypes qp_enc = MAIL_QP_E;
			const char *qp_base = "?Q?";
			
			if(encoding == ENCODING_BASE64)
			{
				qp_enc = GEN_BASE64_ONELINE;
				qp_base = "?B?";
			}
			
			char *qp_temp = NULL;
			int qp_temp_len = 0;
			
			MIME_Encode_Error err;
			
			err = MIME_Encode_SetStr(qp_temp, qp_temp_len, actual_value, input_len, NULL, qp_enc);
			ANCHOR_ARRAY(char, qp_temp);

			if(err != MIME_NO_ERROR) LEAVE(OpStatus::ERR);
			if(qp_temp == NULL)	LEAVE(OpStatus::ERR_NO_MEMORY);
			
			LEAVE_IF_ERROR(encoded_value.SetConcat("=?", charset, qp_base, qp_temp, "?="));
			
			actual_value = encoded_value.CStr();
			
			quote_flag = FALSE;
		}while(0);
	}
	
	Header_Parameter::InitL(p_name, actual_value, quote_flag);
}

void Header_QuotedPrintable_Parameter::InitL(const OpStringC8 &p_name, const OpStringC16 &p_value, const OpStringC8 &charset, Header_Encoding encoding, BOOL quote_if_not_encoded)
{
	OpString8 p_value2;
	ANCHOR(OpString8, p_value2);

	SetToEncodingL(&p_value2, (charset.HasContent() && charset.CompareI("utf-16") != 0 ? charset.CStr() : "utf-8"), p_value.CStr());
	InitL(p_name, p_value2, charset, encoding, quote_if_not_encoded);
}
#endif

#endif // HTTP_data
