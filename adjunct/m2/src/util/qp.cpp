#include "core/pch.h"

#ifdef M2_SUPPORT

#include "qp.h"

#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/header.h"
#include "adjunct/m2/src/util/inputconvertermanager.h"

InputConverterManager* OpQP::s_input_converter_manager = 0;

const char BASE64_ALPHABET[] = {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"}; //RFC2045, p24

void OpQP::SetInputConverterManager(InputConverterManager* input_converter_manager)
{
	s_input_converter_manager = input_converter_manager;
}

InputConverterManager& OpQP::GetInputConverterManager()
{
	if (s_input_converter_manager)
		return *s_input_converter_manager;
	else
		return MessageEngine::GetInstance()->GetInputConverter();
}

UINT8 OpQP::BitsNeeded(const OpStringC16& buffer, int length)
{
    UINT8 bits_needed = 7;
	if (buffer.IsEmpty())
		return bits_needed;

    uni_char* start = (uni_char*)buffer.CStr();
    uni_char* tmp = start;
    while (*tmp && (length==KAll || (tmp-start)<length))
    {
        if (*(tmp) > 255)
        {
            return 16;
        }
        else if ((*(tmp)>127) && bits_needed==7)
        {
            bits_needed = 8;
        }
        tmp++;
    }
    return bits_needed;
}


void OpQP::SkipWhitespaceBeforeEncodedWord(IO const char** src) //RFC2047, p11
{
    if (!src || !*src)
        return; //Nothing to do

    const char* tmp = *src;
    while (*tmp)
    {
        if (*tmp==' ')
        {
            tmp++;
        }
        else if (*tmp=='\r' && *(tmp+1)=='\n' && *(tmp+2)==' ')
        {
            tmp+=3;
        }
		else if (*tmp=='\n' && *(tmp+1)==' ')
		{
			tmp+=2;
		}
        else if (*tmp=='=' && *(tmp+1)=='?')
        {
            break; //We have whitespace and a possible encoded word.
        }
        else
        {
            tmp=NULL; //We don't have whitespace and a possible encoded word, so nothing should be skipped
            break;
        }
    }

    if (tmp)
        *src = tmp;
}


UINT8 OpQP::BitsNeeded(const OpStringC8& buffer, int length)
{
    unsigned char* start = (unsigned char*)buffer.CStr();
	if (buffer.IsEmpty())
		return 7;

    unsigned char* tmp = start;
    while (*tmp && (length==KAll || (tmp-start)<length))
    {
        if (*(tmp++) > 127)
            return 8;
    }
    return 7;
}


BOOL OpQP::LooksLikeQPEncodedWord(const OpStringC16& buffer, int& start_offset, int& end_offset)
{
	start_offset = end_offset = 0;
    if (buffer.IsEmpty())
        return FALSE;

    const uni_char* temp = buffer.CStr();
    const uni_char* start_qp = uni_strstr(temp, UNI_L("=?"));
    const uni_char* end_qp;
    while (start_qp)
    {
        for (end_qp=start_qp+2; end_qp<(start_qp+76); end_qp++)
        {
            if (*end_qp==0 || *end_qp==' ' || *end_qp>=127)
                break;

            if (*end_qp=='?' && *(end_qp+1)=='=')
			{
				start_offset = start_qp-buffer.CStr();
				end_offset = end_qp-buffer.CStr()+1;
                return TRUE;
			}

            if (*end_qp=='=' && *(end_qp+1)=='\r' && *(end_qp+2)=='\n')
			{
				start_offset = start_qp-buffer.CStr();
				end_offset = end_qp-buffer.CStr();
                return TRUE;
			}
        }
        start_qp = uni_strstr(start_qp+2, UNI_L("=?"));
    }

    return FALSE;
}


OP_STATUS OpQP::AppendText(const OpStringC8& charset, OpString16& destination, const char* source, int length, BOOL accept_illegal_characters)
{
	if (*source==0)
		return OpStatus::OK;

	// Do the conversion
	return GetInputConverterManager().ConvertToChar16(charset, source, destination, accept_illegal_characters, TRUE, length);
}


OP_STATUS OpQP::StartNewLine(OpString8& buffer, const OpStringC8& charset, const OpStringC8& type, UINT8& chars_used, BOOL first_line)
{
    OP_STATUS ret;

    //Optimization. Grow once, instead of growing in each Append
    if (!buffer.Reserve(buffer.Length()+5+2+charset.Length()+1+1+1+1)) //Buffer + '?=\r\n ' + '=?' + charset + '?' + type(always one char) + '?' + \0
        return OpStatus::ERR_NO_MEMORY;

    if (!first_line && !buffer.IsEmpty())
    {
        if ((ret=buffer.Append("?=\r\n ")) != OpStatus::OK)
            return ret;
    }

    if ((ret=buffer.Append("=?")) != OpStatus::OK)
        return ret;

    if ((ret=buffer.Append(charset)) != OpStatus::OK)
        return ret;

    if ((ret=buffer.Append("?")) != OpStatus::OK)
        return ret;
	if ((ret=buffer.Append(type.CStr(), 1)) != OpStatus::OK)
        return ret;
    if ((ret=buffer.Append("?")) != OpStatus::OK)
        return ret;

    chars_used = (UINT8)(2 + charset.Length() + 1 + 1 + 1);
    return OpStatus::OK;
}


const char* OpQP::StartOfEncodedWord(const char* buffer, BOOL allow_quotedstring_qp, const char** start_of_iso2022)
{
	if (!buffer)
		return NULL;

	if (start_of_iso2022)
		*start_of_iso2022 = NULL;

	//Find "=?", but not inside DQUOTE (according to RFC2047, �5 (3))
	BOOL in_dquote = FALSE;

	while (TRUE)
	{
		switch (*buffer)
		{
			case '\0':
				// End of string
				return NULL;

			case '=':
				// Check if this is a qp start
				if (*(buffer + 1) == '?' && (!in_dquote || allow_quotedstring_qp))
					return buffer;
				break;

			case 27:
				if (!op_strncmp(buffer + 1, "$B", 2) &&
					start_of_iso2022 &&
					(!in_dquote || allow_quotedstring_qp))
				{
					*start_of_iso2022 = buffer;
					return buffer;
				}
				break;

			case '"':
				in_dquote = !in_dquote;
				break;

			case '\\':
				// Skip quote-special
				if (*(buffer + 1))
					buffer++;
				break;
		}
		buffer++;
	}

	return NULL;
}


//See RFC2045, page23, for info about the BASE64 encoding
void OpQP::Base64Encode(const char* source_3bytes, UINT8 bytes_to_convert, char* destination_4bytes)
{
	const unsigned char* source_3bytes_unsigned = reinterpret_cast<const unsigned char*>(source_3bytes);
	UINT32 base64_value = 0;

	base64_value	 |= source_3bytes_unsigned[0] << 16;
	if (bytes_to_convert>1)
		base64_value |= source_3bytes_unsigned[1] << 8;
	if (bytes_to_convert>2)
		base64_value |= source_3bytes_unsigned[2];

    destination_4bytes[0] = BASE64_ALPHABET[base64_value>>18];
    destination_4bytes[1] = BASE64_ALPHABET[(base64_value>>12) & 0x3F];
    destination_4bytes[2] = bytes_to_convert>1?BASE64_ALPHABET[(base64_value>>6) & 0x3F]:'=';
    destination_4bytes[3] = bytes_to_convert>2?BASE64_ALPHABET[base64_value & 0x3F]:'=';
}

OP_STATUS OpQP::Base64LineEncode(const uni_char* source, OpString8& encoded, const OpStringC8& charset)
{
	OutputConverter* conv_temp = 0;
	RETURN_IF_ERROR(OutputConverter::CreateCharConverter(charset.CStr(), &conv_temp));
	OpAutoPtr<OutputConverter> conv (conv_temp);

    OpString8 temp_buffer;
    if (!temp_buffer.Reserve(75+1)) //According to RFC2047, an encoded line must not exceed 75 characters
    {
        return OpStatus::ERR_NO_MEMORY;
    }

	//Buffer needs to be terminated to avoid problems in OpString::Append
	if (temp_buffer.Capacity())
	{
		*(temp_buffer.CStr()+temp_buffer.Capacity()-1) = 0;
	}

	//How long is the escape sequence?
	UINT8 converter_escape_length = (UINT8)(conv->LongestSelfContainedSequenceForCharacter());
	UINT8 chars_used = 0;
	UINT8 line_capacity;

    int read = 0;
    const char* src = reinterpret_cast<const char*>(source);

	//Variables used in Base64-encoder-loop
	int l;
	const char* buffer_ptr;
	char  tmp_base64string[5]; tmp_base64string[4]=0;
	UINT8 bytes_to_convert;
	BOOL first_line = TRUE;

    int src_len = source ? UNICODE_SIZE(uni_strlen(source)) : 0;
    while (src_len > 0)
    {
		RETURN_IF_ERROR(StartNewLine(encoded, charset, "B", chars_used, first_line)); //RFC2047, p5
		first_line = FALSE;

		//How many chars do we have room for?
		line_capacity = (UINT8)((75 - chars_used - 2)*3/4);

		l = conv->Convert(src, src_len, (char*)temp_buffer.CStr(), line_capacity-converter_escape_length, &read);
        src += read;
        src_len -= read;
		l += conv->ReturnToInitialState((char*)temp_buffer.CStr()+l);

		//Optimization. Grow once, instead of growing in each Append
		if (!encoded.Reserve(encoded.Length()+(((l/3)+1)*4)+2+1)) //Buffer + number of 3-bytes * 4 bytes + '?=' + \0
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		buffer_ptr = temp_buffer.CStr();
		while (l>0)
		{
			bytes_to_convert = l<3 ? l : 3;
			Base64Encode(buffer_ptr, l, tmp_base64string);
			l -= bytes_to_convert;
			buffer_ptr += bytes_to_convert;

			RETURN_IF_ERROR(encoded.Append(tmp_base64string, 4));
		}

        if (!read) //We have data in the buffer that will not be converted. For now, we bail out with an OK
            break;
    }

    return encoded.Append("?=");
}

OP_STATUS OpQP::Base64Encode(const OpStringC16& source, OpString8& encoded, OpString8& charset, BOOL add_controlchars, BOOL is_subject)
{
    encoded.Empty();
    if (source.IsEmpty())
        return OpStatus::OK;

	int subject_offset = 0;
	if (is_subject && add_controlchars)
	{
		if (source.CompareI("RE: ", 4) == 0)
		{
			encoded.Set(source.CStr(), 4);
			subject_offset = 4;
		}
		else if (source.CompareI("FWD: ", 5) == 0)
		{
			encoded.Set(source.CStr(), 5);
			subject_offset = 5;
		}
	}

    OP_STATUS ret;
    OpString8 source8;
    if ((ret=MessageEngine::ConvertToBestChar8(charset, FALSE, source, source8)) != OpStatus::OK)
        return ret;

	if (add_controlchars)
		return Base64LineEncode(source.CStr()+subject_offset, encoded, charset);
    else
        return Base64Encode(source8.CStr(), source8.Length(), encoded);
}

OP_STATUS OpQP::Base64Encode(const char* source, int length, OpString8& encoded)
{
    encoded.Empty();
    if (!source || length==0)
        return OpStatus::OK;

	char  tmp_base64string[5]; tmp_base64string[4]=0;
    int   bytes_left = length;
	UINT8 bytes_to_convert;

	//Optimization. Grow once, instead of growing in each Append
	if (!encoded.Reserve((((bytes_left/3)+1)*4)+1)) //number of 3-bytes * 4 bytes + \0
		return OpStatus::ERR_NO_MEMORY;

    OP_STATUS ret;
	while (bytes_left>0)
    {
		bytes_to_convert = bytes_left<3 ? bytes_left : 3;
		Base64Encode(source, bytes_to_convert, tmp_base64string);
        bytes_left -= bytes_to_convert;
		source += bytes_to_convert;

        if ((ret=encoded.Append(tmp_base64string, 4)) != OpStatus::OK)
            return ret;
    }
    return OpStatus::OK;
}


//See RFC2045, page18, for info about the Quoted-Printable encoding
BOOL OpQP::QPEncode(IN char source, OUT char* destination_2bytes)
{
	BOOL encode = source < 32 || source >= 127;

	if (!encode)
	{
		switch(source)
		{
			case '=':
			case '?':
			case '_':
        	//SHOULD quote these, for reliable transport through EBCDIC gateways
			case '!':
			case '\"':
			case '#':
			case '$':
			case '@':
			case '[':
			case '\\':
			case ']':
			case '^':
			case '`':
			case '{':
			case '|':
			case '}':
			case '~':
			case '<':
			case '>':
			case ';':
			case ':':
			case ',':
			case '.':
			//To avoid problems with comments, we also quote these
			case '(':
			case ')':
				encode = TRUE;
		}
	}

	if (encode)
    {
        destination_2bytes[0]=(((source&0xf0)>>4) >= 10) ? (char)('A'+((source&0xf0)>>4)-10) : (char)('0'+((source&0xf0)>>4));
        destination_2bytes[1]=((source&0x0f) >= 10) ? (char)('A'+(source&0x0f)-10) : (char)('0'+(source&0x0f));
		return TRUE;
    }
	return FALSE;
}

OP_STATUS OpQP::QPLineEncode(const uni_char* source, OpString8& encoded, OpString8& charset)
{
	OutputConverter* conv_temp = 0;
	RETURN_IF_ERROR(OutputConverter::CreateCharConverter(charset.CStr(), &conv_temp));
	OpAutoPtr<OutputConverter> conv (conv_temp);

    OpString8 temp_buffer;
    if (!temp_buffer.Reserve(75+1)) //According to RFC2047, an encoded line must not exceed 75 characters
	{
        return OpStatus::ERR_NO_MEMORY;
    }

	//Buffer needs to be terminated to avoid problems in OpString::Append
	if (temp_buffer.Capacity())
	{
		*(temp_buffer.CStr()+temp_buffer.Capacity()-1) = 0;
	}

	//How much do we need to reserve for the escape sequence?
	UINT8 reserved_escape_length = (UINT8)((conv->LongestSelfContainedSequenceForCharacter()+1)*3);
	UINT8 chars_used = 0;
	UINT8 line_capacity;

    int read = 0;
    const char* src = reinterpret_cast<const char*>(source);

	//Variables used in QP-encoder-loop
	int l;
	char* buffer_ptr;
	char tmp_qpstring[4]; tmp_qpstring[3]=0;
    tmp_qpstring[0]='=';
	BOOL first_line = TRUE;

    int src_len = source ? UNICODE_SIZE(uni_strlen(source)) : 0;
    while (src_len > 0)
    {
        if ((encoded.Length()+75) >= encoded.Capacity())
        {
            if (!encoded.Reserve((int)(encoded.Capacity()*1.5)+75))
			{
                return OpStatus::ERR_NO_MEMORY;
			}
        }

		RETURN_IF_ERROR(StartNewLine(encoded, charset, "Q", chars_used, first_line)); //RFC2047, p5
		first_line = FALSE;

		//How many encoded chars do we _at least_ have room for?
		line_capacity = (UINT8)((75 - chars_used - reserved_escape_length - 2)/3);

		l = conv->Convert(src, src_len, (char*)temp_buffer.CStr(), line_capacity, &read);
        src += read;
        src_len -= read;
        if (!read) //We have data in the buffer that will not be converted. For now, we bail out with an OK
            break;

		buffer_ptr = (char*)temp_buffer.CStr();
		while (l>0)
		{
			if (QPEncode(*buffer_ptr, &tmp_qpstring[1]))
			{
				RETURN_IF_ERROR(encoded.Append(tmp_qpstring, 3));

				chars_used += 3;
			}
			else //Valid to be represented as us-ascii character
			{
				RETURN_IF_ERROR(encoded.Append(*buffer_ptr==32 ? "_" : buffer_ptr, 1)); //According to RFC2047, 4.2.2, SPACE may be represented by a '_' for enhanced readability

				chars_used++;
			}
			l--;
			buffer_ptr++;
		}

		// Do we have room for more chars?
		while (src_len>0 && ((75 - chars_used - reserved_escape_length - 2)/3) > 6)
		{
			l = conv->Convert(src, src_len, (char*)temp_buffer.CStr(), 4, &read);  // UTF-8 can expand to 4 chars
			if (read==0 && l==0) //Couldn't process more data
				break;

			src += read;
			src_len -= read;

			buffer_ptr = (char*)temp_buffer.CStr();
			while (l>0)
			{
				if (QPEncode(*buffer_ptr, &tmp_qpstring[1]))
				{
					RETURN_IF_ERROR(encoded.Append(tmp_qpstring, 3));

					chars_used += 3;
				}
				else //Valid to be represented as us-ascii character
				{
					RETURN_IF_ERROR(encoded.Append(*buffer_ptr==32 ? "_" : buffer_ptr, 1)); //According to RFC2047, 4.2.2, SPACE may be represented by a '_' for enhanced readability

					chars_used++;
				}
				l--;
				buffer_ptr++;
			}
		}

		//Insert the escape sequence if needed
		if (conv->ReturnToInitialState(NULL) != 0)
		{
			l = conv->ReturnToInitialState((char*)temp_buffer.CStr());
			buffer_ptr = (char*)temp_buffer.CStr();
			while (l>0)
			{
				if (QPEncode(*buffer_ptr, &tmp_qpstring[1]))
				{
					RETURN_IF_ERROR(encoded.Append(tmp_qpstring, 3));

					chars_used += 3;
				}
				else //Valid to be represented as us-ascii character
				{
					RETURN_IF_ERROR(encoded.Append(*buffer_ptr==32 ? "_" : buffer_ptr, 1)); //According to RFC2047, 4.2.2, SPACE may be represented by a '_' for enhanced readability

					chars_used++;
				}
				l--;
				buffer_ptr++;
			}
		}
    }

    return encoded.Append("?=");
}

//See RFC2045, page18, for info about the Quoted-Printable encoding
OP_STATUS OpQP::QPEncode(const OpStringC16& source, OpString8& encoded, OpString8& charset, BOOL add_controlchars, BOOL is_subject)
{
    encoded.Empty();

	int subject_offset = 0;
	if (is_subject && add_controlchars && source.CStr())
	{
		if (source.CompareI("RE: ", 4) == 0)
		{
			encoded.Set(source.CStr(), 4);
			subject_offset = 4;
		}
		else if (source.CompareI("FWD: ", 5) == 0)
		{
			encoded.Set(source.CStr(), 5);
			subject_offset = 5;
		}
	}

    OP_STATUS ret;
    OpString8 source8;
    if ((ret=MessageEngine::ConvertToBestChar8(charset, FALSE, source, source8)) != OpStatus::OK)
        return ret;

	if (add_controlchars)
		return QPLineEncode(source.CStr()+subject_offset, encoded, charset);

    char  tmp_qpstring[4]; tmp_qpstring[3]=0;
    tmp_qpstring[0]='=';
    int encoded_length = 0;

    char* tmp_src = source8.CStr();
    while (*tmp_src)
    {
        if (encoded_length+5 >= encoded.Capacity()) //Worst case: encoded + '=xx' + '?='
        {
            if (!encoded.Reserve((int)(encoded.Capacity()*1.5)+50))
                return OpStatus::ERR_NO_MEMORY;
        }

		if (QPEncode(*tmp_src, &tmp_qpstring[1]))
        {
            if ((ret=encoded.Append(tmp_qpstring, 3)) != OpStatus::OK)
                return ret;

            encoded_length += 3;
        }
        else //Valid to be represented as us-ascii character
        {
            if ((ret=encoded.Append(*tmp_src==32?"_":tmp_src, 1)) != OpStatus::OK) //According to RFC2047, 4.2.2, SPACE may be represented by a '_' for enhanced readability
                return ret;

            encoded_length++;
        }

        tmp_src++;
    }
    return OpStatus::OK;
}


OP_STATUS OpQP::Encode(const OpStringC16& source, OpString8& encoded, OpString8& charset, BOOL allow_8bit, BOOL is_subject, BOOL check_for_lookalikes)
{
    encoded.Empty();

    if (source.IsEmpty())
        return OpStatus::OK;

    BOOL needs_qp_encoding = FALSE;
    UINT8 bits_needed = BitsNeeded(source);
	int qp_lookalike_start=0, qp_lookalike_end=0;
    if (bits_needed==7 || (bits_needed==8 && allow_8bit))
    {
        if (!check_for_lookalikes || (needs_qp_encoding=LooksLikeQPEncodedWord(source, qp_lookalike_start, qp_lookalike_end))==FALSE)
        {
			return encoded.Set(source.CStr()); //No need to encode 7-bit (ascii-only) (or 8-bit, when allowed) strings
        }
		OP_ASSERT(qp_lookalike_start==0 && qp_lookalike_end==0);
    }

    if (bits_needed<=8 || needs_qp_encoding)
        return QPEncode(source, encoded, charset, TRUE, is_subject);
    else if (bits_needed==16)
        return Base64Encode(source, encoded, charset, TRUE, is_subject);
    else
        return OpStatus::ERR;
}


OP_STATUS OpQP::Base64Decode(const BYTE*& src, const OpStringC8& charset, OpString16& decoded,
                             BOOL& warning_found, BOOL& error_found, BOOL encoded_word)
{
    warning_found = error_found = FALSE;

    if (!src || !src)
        return OpStatus::ERR_NULL_POINTER;

    if (!*src)
        return OpStatus::OK; //Nothing to do

    //Allocate temp_buffer, used to store decoded char8 before sent to charconverter
    int src_len = (int)op_strlen(reinterpret_cast<const char*>(src));
    int temp_buffer_size = (src_len+3)/4*3;
    if (temp_buffer_size > 65536)
        temp_buffer_size = 65536; // Big is good. Beware of charset conversion error at the boundary.
    int temp_buffer_index = 0;
    BYTE* temp_buffer = OP_NEWA(BYTE, temp_buffer_size);
    if (!temp_buffer)
        return OpStatus::ERR_NO_MEMORY;

    //Create base64 alphabet, see RFC2045, p24
    UINT8 base64_map[256];
	int i;
    for (i=0; i<256; i++) base64_map[i]=255; //255 is used as a "not a base64-char" character
    for (i='A'; i<='Z'; i++) base64_map[i]=(UINT8)(i-'A');
    for (i='a'; i<='z'; i++) base64_map[i]=(UINT8)(i-'a'+26);
    for (i='0'; i<='9'; i++) base64_map[i]=(UINT8)(i-'0'+52);
    base64_map[(unsigned char)'+']=62;
    base64_map[(unsigned char)'/']=63;
    base64_map[(unsigned char)'=']=64; //Padding, and end of QP
    base64_map[(unsigned char)'?']=65; //End of QP

	UINT8 base64_pos = 0;

    OP_STATUS ret;
    while (*src)
    {
        // Do the common case as fast as possible
        UINT8 b0,b1,b2,b3;
        if (base64_pos == 0 && temp_buffer_size-temp_buffer_index >= 3 &&
            ((b0 = base64_map[src[0]]) < 64) &&
            ((b1 = base64_map[src[1]]) < 64) &&
            ((b2 = base64_map[src[2]]) < 64) &&
            ((b3 = base64_map[src[3]]) < 64))
        {
            int output = (b0<<18)+(b1<<12)+(b2<<6)+b3;
            BYTE* cur_buf = &temp_buffer[temp_buffer_index];
            cur_buf[0] = (BYTE)(output>>16);
            cur_buf[1] = (BYTE)(output>>8);
            cur_buf[2] = (BYTE)(output);
            temp_buffer_index += 3;
            src += 4;
            continue;
        }

		UINT8 mapped_bits = base64_map[*src];
		(src)++;

        if (encoded_word && mapped_bits==65 && *src=='=') //We're at the end of the encoded-word
        {
            (src)++; //Skip '='

            //Convert rest of buffer
            ret = AppendText(charset, decoded, reinterpret_cast<char*>(temp_buffer), temp_buffer_index);
            OP_DELETEA(temp_buffer);
            return ret;
        }
        else if (mapped_bits<64) //Legal base64 character
        {
			if (temp_buffer_index >= temp_buffer_size - 1)
			{
				if (OpStatus::IsError(ret = AppendText(charset, decoded, reinterpret_cast<char*>(temp_buffer), temp_buffer_index)))
            	{
					OP_DELETEA(temp_buffer);
					return ret;
            	}

				temp_buffer[0] = temp_buffer[temp_buffer_size - 1];
				temp_buffer_index = 0;
			}

			BYTE* cur_buf = &temp_buffer[temp_buffer_index];

			switch (base64_pos)
			{
				case 0:
					cur_buf[0] = mapped_bits << 2;
					base64_pos++;
					break;
				case 1:
					cur_buf[0] |= mapped_bits >> 4;
					cur_buf[1] = mapped_bits << 4;
					temp_buffer_index++;
					base64_pos++;
					break;
				case 2:
					cur_buf[0] |= mapped_bits >> 2;
					cur_buf[1] = mapped_bits << 6;
					temp_buffer_index++;
					base64_pos++;
					break;
				default:
					cur_buf[0] |= mapped_bits;
					temp_buffer_index++;
					base64_pos = 0;
					break;
			}
        }
        else if (mapped_bits!=64) //We don't want to do anything for the padding '='
        {
            error_found = TRUE;
        }
    }

    src = NULL; //If we get to this line, we're done (if not, the code would have bailed out in the "=?"-test.

    //Convert buffer
	ret = AppendText(charset, decoded, reinterpret_cast<char*>(temp_buffer), temp_buffer_index);
    OP_DELETEA(temp_buffer);
    return ret;
}

OP_STATUS OpQP::QPDecode(const BYTE*& src, const OpStringC8& charset, OpString16& decoded,
                              BOOL& warning_found, BOOL& error_found, BOOL encoded_word)
{
    warning_found = error_found = FALSE;

    if (!src)
        return OpStatus::ERR_NULL_POINTER;

    if (!*src)
        return OpStatus::OK; //Nothing to do

    //Allocate temp_buffer, used to store decoded char8 before sent to charconverter
    int temp_buffer_size = (int)op_strlen(reinterpret_cast<const char*>(src));
    if (temp_buffer_size > 65536)
        temp_buffer_size = 65536; // Big is good. Beware of charset conversion error at the boundary.
    int temp_buffer_index = 0;
    OpString8 temp_buffer_string;
    BYTE* temp_buffer = (BYTE*)temp_buffer_string.Reserve(temp_buffer_size);
    if (!temp_buffer)
        return OpStatus::ERR_NO_MEMORY;

    const BYTE* tmp;
    BYTE qp_value;
    while (*src)
    {
        // Do the common case as fast as possible
        BYTE ch = *src;
        if (temp_buffer_index < temp_buffer_size-1 &&
            ch>=32 && ch!='=' && ch!='_' && ch!='?' && ch!=127)
        {
            temp_buffer[temp_buffer_index++] = ch; // non-special character can be appended directly
            src++;
            continue;
        }

        if (encoded_word && *src=='?' && *(src+1)=='=') //We're at the end of the encoded-word
        {
            src += 2; //Skip "?="

            //Convert rest of buffer
			return AppendText(charset, decoded, reinterpret_cast<char*>(temp_buffer), temp_buffer_index);
        }
        else if ( (*src<27 && *src!='\t' && *src!='\r' && *src!='\n') || //Invalid control character?
				  (*src==127) ||
                  //(*src>=128) || //According to RFC2045, �6.7, decimal values greater than 126 MUST not appear in encoded text, but a robust implementation might exclude (or accept) them. Eudora sends 8-bit characters, so I choose not to exclude it.
                  ((*src=='\t' || *src==' ') && (*(src+1)=='\r' || *(src+1)=='\n')) ) //Whitespace at end-of-line?
        {
            error_found = TRUE;
            (src)++;
        }
        else if (*src=='_') //Special-case: QP-encoded whitespace
        {
            (src)++;
            temp_buffer[temp_buffer_index++]=' ';
        }
        else if (*src=='=') //Encoded char, or soft linebreak
        {
            tmp = src+1;
            while (*tmp==' ') tmp++; //Skip any whitespace possibly added by MTA at EOL
            if (*tmp=='\r' && *(tmp+1)=='\n')
            {
                tmp += 2; //Skip CRLF
                while (*tmp==' ') tmp++; //Skip spacestuffing
                src = tmp;
            }
            else if ( ((*tmp>='A' && *tmp<='F') || (*tmp>='0' && *tmp<='9') || (*tmp>='a' && *tmp<='f')) &&
                      ((*(tmp+1)>='A' && *(tmp+1)<='F') || (*(tmp+1)>='0' && *(tmp+1)<='9') || (*(tmp+1)>='a' && *(tmp+1)<='f')) )
            {
                qp_value = 0;
                if (*tmp>='A' && *tmp<='F') qp_value |= (10+(*tmp-'A'))<<4;
                else if (*tmp>='0' && *tmp<='9') qp_value |= (*tmp-'0')<<4;
                else if (*tmp>='a' && *tmp<='f') //A robust implementation should allow lower-case hex-digits
                {
                    warning_found =TRUE;
                    qp_value |= (10+(*tmp-'a'))<<4;
                }
                tmp++;
                if (*tmp>='A' && *tmp<='F') qp_value |= 10+(*tmp-'A');
                else if (*tmp>='0' && *tmp<='9') qp_value |= *tmp-'0';
                else if (*tmp>='a' && *tmp<='f') //A robust implementation should allow lower-case hex-digits
                {
                    warning_found =TRUE;
                    qp_value |= 10+(*tmp-'a');
                }

                temp_buffer[temp_buffer_index++]=qp_value;

                src = tmp+1; //We have already increased tmp 2 chars relative to old src
            }
            else //Invalid character detected
            {
                error_found = TRUE;

                if (temp_buffer_index >= temp_buffer_size - 2)
                {
					RETURN_IF_ERROR(AppendText(charset, decoded, reinterpret_cast<char*>(temp_buffer), temp_buffer_index));
                	temp_buffer_index = 0;
                }

                temp_buffer[temp_buffer_index++]=*(src)++;
                temp_buffer[temp_buffer_index++]=*(src)++;
            }
        }
        else
        {
            temp_buffer[temp_buffer_index++]=*(src)++;//US-ASCII character can be appended directly
        }

        if (temp_buffer_index >= temp_buffer_size)
        {
			RETURN_IF_ERROR(AppendText(charset, decoded, reinterpret_cast<char*>(temp_buffer), temp_buffer_index));
        	temp_buffer_index = 0;
        }
    }

    src = NULL; //If we get to this line, we're done (if not, the code would have bailed out in the "=?"-test.

    //Convert buffer
	return AppendText(charset, decoded, reinterpret_cast<char*>(temp_buffer), temp_buffer_index);
}


OP_STATUS OpQP::UnknownDecode(IO const char** src, IN const OpStringC8& charset, OpString16& decoded)
{
    if (!src || !*src)
        return OpStatus::ERR_NULL_POINTER;

    if (!**src)
        return OpStatus::OK; //Nothing to do

    OP_STATUS ret;
    const char* tmp = *src;
    while (*tmp)
    {
        if (*tmp==' ')
        {
            if ((ret=AppendText(charset, decoded, *src, tmp-(*src))) != OpStatus::OK)
                return ret;

            *src = tmp;
            return OpStatus::OK;
        }
        else if (*tmp=='?' && *(tmp+1)=='=')
        {
            if ((ret=AppendText(charset, decoded, *src, tmp-(*src))) != OpStatus::OK)
                return ret;

            *src = tmp+2; //Skip "?="
            return OpStatus::OK;
        }
        tmp++;
    }

    if ((ret=AppendText(charset, decoded, *src)) != OpStatus::OK)
        return ret;
    *src = NULL; //We're done

    return OpStatus::OK;
}


OP_STATUS OpQP::Decode(const OpStringC8& source, OpString16& decoded, const OpStringC8& preferred_charset,
                       BOOL& warning_found, BOOL& error_found, BOOL allow_quotedstring_qp, BOOL is_rfc822)
{
    warning_found = error_found = FALSE;
    decoded.Empty();

    if (source.IsEmpty())
        return OpStatus::OK;

    const char* src = source.CStr();
    const char* tmp;
	const char* start_iso2022;

	OpString rfc822_atom;
	OpString* decode_destination = is_rfc822 ? &rfc822_atom : &decoded;
	BOOL in_quoted_string = FALSE;

    OP_STATUS ret;
    OpString8 charset;
    while (src)
    {
		tmp = StartOfEncodedWord(src, allow_quotedstring_qp, &start_iso2022); //Find "=?" or [ESC]$B
		if (in_quoted_string && (!tmp || src != tmp || start_iso2022)) // "end quotes" necessary
		{
			const char* has_following_quote_char = strchr(src, '\"');
			const char* has_following_address = strchr(src, ',');
			// only append a quote if there are none in the string, at least not before a ','
			if (!has_following_quote_char || has_following_address && has_following_quote_char > has_following_address)
			{
				decoded.Append(UNI_L("\""));
				in_quoted_string = FALSE;
			}
		}

        if (!tmp) //No coding found (rest of text is US-ASCII, or at least compatible with given charset)
            return AppendText(preferred_charset, decoded, src);

        if (src != tmp) //Append any non-encoded text directly to the decoded buffer
        {
			for (const char* quotechar = strchr(src, '\"'); quotechar && quotechar < tmp; quotechar = strchr(quotechar+1, '\"'))
				in_quoted_string = !in_quoted_string;

			RETURN_IF_ERROR(AppendText(preferred_charset, decoded, src, tmp-src));
        }

		if (start_iso2022)
		{
            if ((ret=AppendText("iso-2022-jp", decoded, start_iso2022, KAll, TRUE)) != OpStatus::OK)
                return ret;

			src = NULL; //We are done, as we expect a iso-2022-jp-encoded string to not mix with QP or Base64-encoded words (in that case, QP or Base64 would be used exclusivly)
		}
		else
		{
			src = tmp+2; //Skip "=?"
			tmp = strchr(src, '?');
			if (!tmp) //Syntax error. Append the rest of the string and return
			{
				return AppendText(preferred_charset, decoded, src);
			}

			if ((ret=charset.Set(src, tmp-src)) != OpStatus::OK)
				return ret;

			src = tmp+1; //Skip "?"

			char enc_char = *src;

			do
			{
				if (enc_char && *(src+1)=='?')
				{
					src += 2;
					enc_char &= 0xDF;
					if (enc_char == 'B') //Base64 encoding
					{
						if ((ret=Base64Decode(reinterpret_cast<const BYTE*&>(src), charset, *decode_destination, warning_found, error_found)) != OpStatus::OK)
							return ret;

						break;
					}
					else if (enc_char == 'Q') //QP encoding
					{
						if ((ret=QPDecode(reinterpret_cast<const BYTE*&>(src), charset, *decode_destination, warning_found, error_found)) != OpStatus::OK)
							return ret;

						break;
					}
				}
				//Unknown encoding
				warning_found = TRUE; //Unknown encoding counts as a warning
				if ((ret=UnknownDecode(&src, charset, *decode_destination)) != OpStatus::OK)
					return ret;
			}
			while (0);
		}

		if (is_rfc822 && rfc822_atom.HasContent())
		{
			for (uni_char* quotechar = uni_strchr(rfc822_atom.CStr(), '\"'); quotechar && *quotechar; quotechar = uni_strchr(quotechar+1, '\"'))
				in_quoted_string = !in_quoted_string;
			if(!in_quoted_string && Header::From::NeedQuotes(rfc822_atom, TRUE))
			{
				RETURN_IF_ERROR(rfc822_atom.Insert(0, UNI_L("\"")));
				in_quoted_string = TRUE;
			}

			RETURN_IF_ERROR(decoded.Append(rfc822_atom));
			rfc822_atom.Empty();
		}

		if(src && *src == '\"')
			src++; // Do not append quotes, we'll add these ourselves if necessary

        SkipWhitespaceBeforeEncodedWord(&src);
    }

	if(in_quoted_string)
		decoded.Append(UNI_L("\""));

    return OpStatus::OK;
}

#endif //M2_SUPPORT
