#include "core/pch.h"

#ifdef M2_SUPPORT

#include "quote.h"
#include "adjunct/m2/src/engine/engine.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"


OpQuoteBuffer::OpQuoteBuffer(OpString* destination, unsigned int buffer_size, unsigned int rollback_size)
: m_tmp_buffer(OP_NEWA(uni_char, buffer_size+1)),
  m_tmp_buffer_pos(0),
  m_destination(destination),
  m_destination_length(destination ? destination->Length() : 0),
  m_buffer_size(buffer_size),
  m_rollback_size(rollback_size < buffer_size ? rollback_size : (buffer_size - 1))
{
    m_tmp_buffer[0] = m_tmp_buffer[m_buffer_size] = 0;
}


OpQuoteBuffer::~OpQuoteBuffer()
{
    if (m_tmp_buffer)
    {
        m_tmp_buffer[m_tmp_buffer_pos]=0;
        m_destination->Append(m_tmp_buffer); //Append the remaining of m_tmp_buffer
        OP_DELETEA(m_tmp_buffer);
    }
}


OP_STATUS OpQuoteBuffer::Append(uni_char append_char)
{
    if (!m_tmp_buffer)
        return OpStatus::ERR_NO_MEMORY;

    m_tmp_buffer[m_tmp_buffer_pos++]=append_char;
    if (m_tmp_buffer_pos==m_buffer_size)
    {
        int append_length = m_buffer_size-m_rollback_size;
        uni_char cTmp = m_tmp_buffer[append_length]; //Remember this char, so we can set it back after having zero-terminated it
        m_tmp_buffer[append_length]=0;

        //Allocate bulk of memory if needed
        if ((m_destination_length+append_length) >= m_destination->Capacity())
        {
            if (m_destination->Reserve((m_destination_length+append_length)*2) == NULL) //Make room for twice the stringlength
                return OpStatus::ERR_NO_MEMORY;
        }

        //Append string
        m_destination->Append(m_tmp_buffer);
        m_destination_length += append_length;

        m_tmp_buffer[append_length]=cTmp;
        memmove(m_tmp_buffer, m_tmp_buffer+append_length, UNICODE_SIZE(m_rollback_size));
        m_tmp_buffer_pos=m_rollback_size;
    }
    return OpStatus::OK;
}

OP_STATUS OpQuoteBuffer::Append(const OpStringC& append_string)
{
	return Append(append_string.CStr(), append_string.Length());
}

OP_STATUS OpQuoteBuffer::Append(const uni_char* append_string_ptr, int length)
{
	OP_STATUS ret = OpStatus::OK;
	int i;
	for (i=0; i<length && ret==OpStatus::OK; i++)
	{
		ret = Append(*(append_string_ptr+i));
	}
	return ret;
}

OP_STATUS OpQuoteBuffer::Delete(unsigned int delete_count)
{
    if (delete_count>m_tmp_buffer_pos)
        return OpStatus::ERR_OUT_OF_RANGE;

    m_tmp_buffer_pos-=delete_count;
    return OpStatus::OK;
}


uni_char OpQuoteBuffer::Peek(unsigned int back_count) const
{
    return back_count>=m_tmp_buffer_pos?(uni_char)0:m_tmp_buffer[m_tmp_buffer_pos-back_count-1];
}



OpQuote::OpQuote(BOOL is_flowed, BOOL has_delsp, BOOL should_autowrap, unsigned int max_line_length, unsigned int max_quote_depth)
: m_max_line_length(max_line_length),
  m_max_quote_depth(max_quote_depth),
  m_is_flowed(is_flowed),
  m_has_delsp(has_delsp),
  m_should_autowrap(should_autowrap)
{
	if (!m_is_flowed)
		m_has_delsp = FALSE;

    if (m_max_line_length==0)
        m_max_line_length = 76;

    if (m_max_quote_depth>=m_max_line_length) //This should be rare...
        m_max_quote_depth=m_max_line_length-1;
}


OpQuote::~OpQuote()
{
}


OP_STATUS OpQuote::QuoteAndWrapText(OpString& quoted_and_wrapped_text, const OpStringC& original_text, uni_char quote_char)
{
    OP_STATUS ret;
    OpString quoted_text;
    quoted_and_wrapped_text.Empty();
	if (original_text.IsEmpty())
		return OpStatus::OK;

    ret = QuoteText(quoted_text, original_text, quote_char);

    if (ret==OpStatus::OK)
	{
		if (m_should_autowrap)
		{
			ret = WrapText(quoted_and_wrapped_text, quoted_text, TRUE /*there shouldn't be any nonquoted lines */);
		}
		else
		{
			ret = quoted_and_wrapped_text.Set(quoted_text);
		}
	}

    if (ret!=OpStatus::OK)
        quoted_and_wrapped_text.Empty();

    return ret;
}


OP_STATUS OpQuote::QuoteText(OpString& quoted_text, const OpStringC& original_text, uni_char quote_char)
{
    OP_STATUS ret = OpStatus::OK;
    quoted_text.Empty();
    const uni_char* original_text_cstr = original_text.CStr();
    unsigned int original_text_len = uni_strlen(original_text_cstr);
    unsigned int original_text_pos = 0;

    //Allocate buffer used to cache OpString::Append (prevent fragmentation)
    OpQuoteBuffer quote_buffer(&quoted_text, 1024, 3); //Buffer should keep 3 uni_char: reflow-space, CR and LF
    ret = quote_buffer.Validate();
    if (ret!=OpStatus::OK)
        return ret;

	OpString snip_text;

	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_STRIPPED_QUOTE, snip_text));
	RETURN_IF_ERROR(snip_text.Append(UNI_L("\r\n")));

    // Quote the text, line by line
	unsigned current_quote_depth;
    BOOL found_sig=FALSE, purging=FALSE;
    while (ret==OpStatus::OK && !found_sig && original_text_pos<=original_text_len)
    {
        if ((original_text_pos+3)<=original_text_len &&
            original_text_cstr[original_text_pos]=='-' &&
            original_text_cstr[original_text_pos+1]=='-' &&
            original_text_cstr[original_text_pos+2]==' ' &&
            (original_text_cstr[original_text_pos+3]=='\r' || original_text_cstr[original_text_pos+3]=='\n'))
        {
            found_sig=TRUE;
            break;
        }

		current_quote_depth = 0;
		while (original_text_cstr[original_text_pos+current_quote_depth]=='>') current_quote_depth++;

		if (current_quote_depth>=m_max_quote_depth && !purging)
		{
			ret = quote_buffer.Append(snip_text);
		}

		purging = current_quote_depth>=m_max_quote_depth;

		if (ret==OpStatus::OK)
		{
			if (purging) //Skip the current line of text
			{
				while (original_text_pos<=original_text_len && original_text_cstr[original_text_pos]!='\r' && original_text_cstr[original_text_pos]!='\n') original_text_pos++; //Skip line
				if (original_text_pos<=original_text_len && original_text_cstr[original_text_pos]=='\r') original_text_pos++; //Skip CR
				if (original_text_pos<=original_text_len && original_text_cstr[original_text_pos]=='\n') original_text_pos++; //Skip LF
			}
			else //Append this line to the buffer
			{
				if (quote_char)
				{
					ret = quote_buffer.Append(quote_char);

					if (ret==OpStatus::OK && original_text_cstr[original_text_pos]!='>' && original_text_cstr[original_text_pos]!='\r' && original_text_cstr[original_text_pos]!='\n')
					{
						ret = quote_buffer.Append(' ');
					}
				}

				if (ret==OpStatus::OK && original_text_pos<=original_text_len)
				{
					uni_char* line_end = uni_strpbrk((uni_char*)original_text_cstr+original_text_pos, UNI_L("\r\n"));
					if (!line_end)
						line_end = const_cast<uni_char*>(original_text_cstr+original_text_len);

					int tmp_text_len = line_end - (original_text_cstr+original_text_pos);
					while ((!m_is_flowed || !m_should_autowrap) && tmp_text_len>0 && *(original_text_cstr+original_text_pos+tmp_text_len-1)==' ') tmp_text_len--;
					if (tmp_text_len>0)
					{
						ret = quote_buffer.Append(original_text_cstr+original_text_pos, tmp_text_len);

						if (ret==OpStatus::OK && m_should_autowrap && m_is_flowed && !m_has_delsp && *(line_end-1)==' ') //If it was flowed, without extra delsp-char, append one. Don't worry about signature, we won't get here if current line is the delimiter
						{
							ret = quote_buffer.Append(' ');
						}
					}
					original_text_pos = line_end-original_text_cstr;
					if (*(original_text_cstr+original_text_pos)==0) original_text_pos++; //Quick hack to terminate at EOF
				}

				if (ret==OpStatus::OK && original_text_pos<=original_text_len && original_text_cstr[original_text_pos]=='\r')
				{
					ret = quote_buffer.Append(original_text_cstr[original_text_pos++]);
				}
				if (ret==OpStatus::OK && original_text_pos<=original_text_len && original_text_cstr[original_text_pos]=='\n')
				{
					ret = quote_buffer.Append(original_text_cstr[original_text_pos++]);
				}
			}
		}
    }

	// We have added delsp characters where necessary
	m_has_delsp = m_is_flowed;

    return ret;
}

OP_STATUS OpQuote::VerifyBufferCapacity(OpString& buffer, uni_char*& current_position, int needed, int grow_size) const
{
	int used = current_position-buffer.CStr();
	if (used < 0)
	{
		OP_ASSERT(0);
		return OpStatus::ERR;
	}

	if (needed <= (buffer.Capacity()-used))
		return OpStatus::OK;

	if (!buffer.Reserve(buffer.Capacity()+needed+grow_size))
		return OpStatus::ERR_NO_MEMORY;

	current_position = const_cast<uni_char*>(buffer.CStr())+used;
	return OpStatus::OK;
}

OP_STATUS OpQuote::WrapText(OpString& wrapped_text, const OpStringC& original_text, BOOL strip_nonquoted_flows)
{
	wrapped_text.Empty();
	if (original_text.IsEmpty())
		return OpStatus::OK; //Nothing to do

	const int buffer_grow = 100+original_text.Length()/256; //Counter of how many characters we have to spare in the destination buffer (we need to realloc if this is negative)
	if (!wrapped_text.Reserve(original_text.Length()+buffer_grow+1))
		return OpStatus::ERR_NO_MEMORY;

	uni_char* source = const_cast<uni_char*>(original_text.CStr());
	uni_char* destination = const_cast<uni_char*>(wrapped_text.CStr());

	// Optimization: Allocate variables used in the loop here to avoid unneeded allocation/deallocation
	OP_STATUS ret;
	int i, temp_length;
	uni_char* source_fragment_start = NULL;
	uni_char* source_fragment_end;
	uni_char current_char;
	BOOL start_of_destination_line;
	BOOL line_is_signature;
	int skipped_spaces;
	int destination_line_bytes;      //Number of bytes copied to the current destination line. Must not exceed 998
	int destination_line_halfwidths; //Logical width of text on line. Count CJK full-width as twice the width of latin characters. Should not exceed m_max_line_length
	int current_quote = 0, next_quote;
	BOOL needs_flow_char;

	//While-loop expects source to point to text after quote-levels. Adjust pointer and count quotes before we loop
	while (*source=='>') {source++; current_quote++;}
	if (current_quote>0 && *source==' ') source++;

	while (source && *source)
	{
		/*
		** Start a new destination line
		*/
		destination_line_bytes = destination_line_halfwidths = 0;

		//Insert quote-chars
		if (current_quote > 0)
		{
			if ((ret=VerifyBufferCapacity(wrapped_text, destination, current_quote+1, buffer_grow)) != OpStatus::OK)
				return ret;

			for (i=0; i<current_quote; i++)	*(destination++) = '>'; //current_quote will usually be small, so a memset would probably not be faster
			*(destination++) = ' ';
			destination_line_bytes = destination_line_halfwidths = current_quote + 1; //All of these are half-width
		}

		next_quote = current_quote;
		start_of_destination_line = TRUE;
		while (source && *source)
		{
			/*
			** Find fragment
			*/
			source_fragment_start = source_fragment_end = source;

			//Skip whitespace at start of fragment
			while (*source_fragment_end==' ')
			{
				source_fragment_end++;
				destination_line_halfwidths++;
			}

			line_is_signature = FALSE;

			//Find end of fragment
			if (start_of_destination_line && current_quote==0 && //Signature?
				*source_fragment_start=='-' && *(source_fragment_start+1)=='-' && *(source_fragment_start+2)==' ' && (*(source_fragment_start+3)=='\r' || *(source_fragment_start+3)=='\n'))
			{
				source_fragment_end = source_fragment_start+3;
				destination_line_halfwidths = 3;
				line_is_signature = TRUE;
			}
			else
			{
				while ((current_char = *source_fragment_end) != 0)
				{
					//Are we at EOL/EOF?
					if (current_char=='\r' ||
						current_char=='\n')
					{
						break;
					}

					destination_line_halfwidths += ((current_char>=0x3000 && current_char<=0xD7A3) ||
													(current_char>=0xF900 && current_char<=0xFAFF) ||
													(current_char>=0xFF01 && current_char<=0xFF64)) ? 2 : 1; //Hack to identify full-width characters

					// We want to break *after* SP, not before
					if (*source_fragment_end==0 || *++source_fragment_end==' ')
					{
						break;
					}
				}
			}

			skipped_spaces = 0;
			if (source_fragment_end!=source_fragment_start)
			{
				//Include whitespace after fragment
				while (*source_fragment_end==' ')
				{
					source_fragment_end++;
					destination_line_halfwidths++;
				}

				//We're at start of next fragment. Go one back (two if delsp=yes)(destination_line_halfwidths has not been adjusted for the last character, so no need to adjust that)
				source_fragment_end--;
				if (*source_fragment_end==' ' &&
					(*(source_fragment_end+1)=='\r' || *(source_fragment_end+1)=='\n' || *(source_fragment_end+1)==0)) //Space at end of line?
				{
					if (current_quote==0 && !line_is_signature)
					{
						while (*source_fragment_end==' ')
						{
							source_fragment_end--;
							destination_line_halfwidths--;
							skipped_spaces++;
						}
					}
					else if (source_fragment_end!=source_fragment_start && !line_is_signature && m_has_delsp)
					{
						source_fragment_end--;
						destination_line_halfwidths--;
						skipped_spaces = 1;
					}
				}
			}
			else //Blank line, end-of-line, etc. Test for reflow
			{
				//Find next fragment and quote-level on next line
				next_quote = 0;
				source = source_fragment_end;
				if (*source=='\r') source++;
				if (*source=='\n') source++;
				while (*source=='>') {source++; next_quote++;}
				if (*source==' ' && next_quote>0) source++;

				BOOL signature = ((destination-wrapped_text.CStr())>=3 &&
								  *(destination-1)==' ' &&
								  *(destination-2)=='-' &&
								  *(destination-3)=='-' &&
								  ((destination-wrapped_text.CStr())==3 || (*(destination-4)=='\r' || *(destination-4)=='\n')));

				//Strip whitespace at end of unquoted lines if requested, and always when quote-level changes
				if (!signature &&
					((current_quote==0 && strip_nonquoted_flows) || current_quote!=next_quote || start_of_destination_line))
				{
					while (destination>wrapped_text.CStr() && *(destination-1)==' ') destination--;
				}

				if (!signature &&
					destination>wrapped_text.CStr() && *(source_fragment_end-1)==' ' && next_quote==current_quote && current_quote>0) //Softwrap. Reflow!
				{
					continue;
				}
				else //Hardwrapped. End the destination line
				{
					break;
				}
			}

			//Copy fragment?
			if (start_of_destination_line ||
				(destination_line_halfwidths+1) <= (int)m_max_line_length) //We have to add 1, in case this line will need to flow
			{
				if (start_of_destination_line)
				{
					//(RFC2646, §4.1, #1)
					if ((destination_line_bytes + (source_fragment_end-source_fragment_start+1) + 1) > 998)
					{
						source_fragment_end = source_fragment_start+(998-1-destination_line_bytes);
					}

					//(RFC2646, §4.1, #3)
					if (current_quote==0 && (*source_fragment_start==' ' || uni_strni_eq((const uni_char*)source_fragment_start, "FROM ", 5)))
					{
						if ((ret=VerifyBufferCapacity(wrapped_text, destination, 1, buffer_grow)) != OpStatus::OK)
							return ret;

						*destination++ = ' ';
					}
				}

				temp_length = source_fragment_end-source_fragment_start+1;
				if ((ret=VerifyBufferCapacity(wrapped_text, destination, temp_length, buffer_grow)) != OpStatus::OK)
					return ret;

				op_memcpy(destination, source_fragment_start, temp_length * sizeof(uni_char));
				destination += temp_length;
				destination_line_bytes += temp_length;
				source = source_fragment_end+1+skipped_spaces;
				start_of_destination_line = FALSE;
			}
			else //we need to flow
			{
				break;
			}
		}

		/*
		** End the destination line
		*/

        //We get here for 8 different reasons and expected behaviour:

        //1. Unquoted line where we have [atom][space][CRLF or EOF]                    Wrap (Trim user-inserted space)
        //2. Quoted line where we have [atom][space][CRLF or EOF]                      Flow, we have the flow char
        //3. Unquoted line where we have [atom][CRLF or EOF]                           Wrap
        //4. Quoted line where we have [atom][CRLF or EOF]                             Wrap, we do not have the flow char
        //5. Unquoted line where we have [atom][space][next atom]                      Flow, user wants a long line
        //6. Quoted line where we have [atom][space][next atom]                        Flow, content was a long line
        //7. Unquoted line where we have [first ~998 part of atom][next part of atom]  Flow, user wrote a long word
        //8. Quoted line where we have [first ~998 part of atom][next part of atom]    Flow, content was a long word

		//source_fragment_start points to first character of last [atom] (or CRLF/EOF)
		//source points to \0 if EOF

		if (*source || destination==wrapped_text.CStr())
		{
			needs_flow_char = TRUE;
			if (!*source_fragment_start || *source_fragment_start=='\r' || *source_fragment_start=='\n') //Handle 1-4 on the list above
			{
				if (current_quote==0 || (destination!=wrapped_text.CStr() && *(destination-1)!=' ')) //Handle 1, 3 and 4 on the list above
					needs_flow_char = FALSE;
			}

			//Don't flow across quote-levels
			if (current_quote != next_quote)
			{
				current_quote = next_quote;
				needs_flow_char = FALSE;
			}

			//Insert hard or soft linefeed
			if ((ret=VerifyBufferCapacity(wrapped_text, destination, 3, buffer_grow)) != OpStatus::OK)
				return ret;

			if (needs_flow_char)
			{
				*destination++ = ' ';
			}

			*destination++ = '\r';
			*destination++ = '\n';
		}
		start_of_destination_line = TRUE;
	}

	//Terminate string
	if ((ret=VerifyBufferCapacity(wrapped_text, destination, 1, 1)) != OpStatus::OK)
		return ret;
	*destination++ = 0;

	return OpStatus::OK;
}

OP_STATUS OpQuote::UnwrapText(OpString& unwrapped_text, const OpStringC& original_text)
{
	unwrapped_text.Empty();
	if (original_text.IsEmpty())
		return OpStatus::OK; //Nothing to do

	const int buffer_grow = 100;
	if (!unwrapped_text.Reserve(original_text.Length()+buffer_grow+1))
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret;
	uni_char* line_start = const_cast<uni_char*>(original_text.CStr());
	uni_char* line_end;
	uni_char* destination = const_cast<uni_char*>(unwrapped_text.CStr());
	BOOL last_line_flowed = FALSE;
	int delsp_offset;
	while (line_start && *line_start)
	{
		line_end = uni_strpbrk(line_start, UNI_L("\r\n"));

		//If we go from flowed to wrapped state, make sure the linefeed is inserted
		if (*line_start=='>' && last_line_flowed)
		{
			if ((ret=VerifyBufferCapacity(unwrapped_text, destination, 2, buffer_grow)) != OpStatus::OK) //We will write two characters not present in the original string
				return ret;

			*destination++ = '\r';
			*destination++ = '\n';
		}

		if (!line_end) //We're at the end of the text.
		{
			if (*line_start==' ') line_start++; //Skip space-stuffing

			if ((ret=VerifyBufferCapacity(unwrapped_text, destination, uni_strlen(line_start), buffer_grow)) != OpStatus::OK)
				return ret;

			uni_strcpy(destination, line_start);
			return OpStatus::OK;
		}
		else //New line. Append the line to the end of destination buffer
		{
			if (line_end == line_start)
			{
				last_line_flowed = FALSE;
			}
			else if (*line_start=='>') //Quoted content should not be unwrapped
			{
				if ((ret=VerifyBufferCapacity(unwrapped_text, destination, (line_end-line_start), buffer_grow)) != OpStatus::OK)
					return ret;

				op_memcpy(destination, line_start, (line_end-line_start) * sizeof(uni_char));
				destination += (line_end-line_start);
				line_start=line_end;
				last_line_flowed = FALSE; //In this function, we should never unflow quoted lines
			}
			else
			{
				delsp_offset = 0;

				if (*line_start==' ') line_start++; //Skip space-stuffing

				if (line_end>line_start && *(line_end-1)==' ' &&
					!((line_end-line_start)==3 && uni_strncmp(line_start, UNI_L("-- "), 3)==0)) //Signature should not unwrap
				{
					last_line_flowed = TRUE;
					if (m_has_delsp)
						delsp_offset = 1;
				}
				else
				{
					last_line_flowed = FALSE;
				}

				if ((ret=VerifyBufferCapacity(unwrapped_text, destination, (line_end-line_start)-delsp_offset, buffer_grow)) != OpStatus::OK)
					return ret;

				op_memcpy(destination, line_start, ((line_end-line_start)-delsp_offset) * sizeof(uni_char));
				destination += (line_end-line_start)-delsp_offset;

				if (last_line_flowed) //If we have unwrapped lines, skip whitespace and linefeed
				{
					if (*line_end=='\r') line_end++;
					if (*line_end=='\n') line_end++;
				}

				line_start=line_end;
			}

			if ((ret=VerifyBufferCapacity(unwrapped_text, destination, 2, buffer_grow)) != OpStatus::OK)
				return ret;

			if (*line_start=='\r')
				*destination++ = *line_start++;

			if (*line_start=='\n')
				*destination++ = *line_start++;
		}
	}

	if ((ret=VerifyBufferCapacity(unwrapped_text, destination, 1, 1)) != OpStatus::OK) //We will write one character not counted in the original string
		return ret;

	*destination = 0; //Just to be safe. It could get here without zero-terminator

	return OpStatus::OK;
}

#endif //M2_SUPPORT
