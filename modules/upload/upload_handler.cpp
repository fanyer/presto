/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef HAS_SET_HTTP_DATA
#include "modules/hardcore/mem/mem_man.h"

#include "modules/upload/upload.h"
#include "modules/formats/encoder.h"
#include "modules/formats/base64_decode.h"

#include "modules/olddebug/tstdump.h"

#include "modules/util/cleanse.h"

Upload_Handler::Upload_Handler()
{
	internal_buffer = NULL;
	buffer_len = buffer_pos = buffer_size = 0;
	buffer_min_len = 0;
	last_block = FALSE;
	calculated_payload_size = 0;
	current_line_len = 0;

	encoding = ENCODING_AUTODETECT;
}

Upload_Handler::~Upload_Handler()
{
	FreeBuffer();
}

void Upload_Handler::InitL(MIME_Encoding enc, const char **header_names)
{
	encoding = enc;
	Upload_Base::InitL(header_names);
}

OpFileLength Upload_Handler::CalculateLength()
{
	return calculated_payload_size + Upload_Base::CalculateLength();
}

#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
unsigned char *Upload_Handler::Encode7Or8Bit(unsigned char *target, uint32 &remaining_len, BOOL more)
{
	const unsigned char *current = internal_buffer;
	unsigned char token;
	
	while(remaining_len>0 && buffer_pos < buffer_len)
	{
		token = *(current++);
		if(token == '\r' || token == '\n')
		{
			if(remaining_len < 2)
				break;

			if(token == '\r')
			{
				if(buffer_pos +1 >= buffer_len && more)
					break; // we will have to wait until the next time
			
				if(buffer_pos +1 < buffer_len && *current == '\n')
				{
					current++;
					buffer_pos ++;
				}
			}
			
			*(target++) = '\r';
			token = '\n';
			remaining_len --;
		}

		buffer_pos ++;
		*(target++) = token;
		remaining_len --;
	}
	return target;
}
#endif // URL_UPLOAD_MAILENCODING_SUPPORT

#ifdef URL_UPLOAD_QP_SUPPORT
unsigned char *Upload_Handler::EncodeQP(unsigned char *target, uint32 &remaining_len, BOOL more)
{
	const unsigned char *current = internal_buffer;
	unsigned char token;
	const char * const hexchars = GetHexChars();
	
	while(remaining_len>6 && buffer_pos < buffer_len)
	{
		token = *(current++);
		switch(token)
		{
		case '\n':
		case '\r':
			if(token == '\r')
			{
				if(buffer_pos +1 >= buffer_len && more)
					break; // we will have to wait until the next time
				
				if(buffer_pos +1 < buffer_len && *current == '\n')
				{
					current++;
					buffer_pos ++;
				}
			}
			
			*(target++) = '\r';
			*(target++) = '\n';

			remaining_len -=2;
			current_line_len = 0;
			break;
		case '\t':
		case ' ':
			{
				BOOL add_softlinefeed = FALSE;
				
				if(buffer_pos+1 < buffer_len)
				{
					unsigned char token1 = *current;
					
					if(token1 == '\r' || token1 == '\n')
					{
						add_softlinefeed = TRUE;						
					}
				}
				else if(!more)
				{
					add_softlinefeed = TRUE;
				}
				else
					return target;
				
				*(target++) = token;
				remaining_len --;
				current_line_len ++;
				if(add_softlinefeed)
				{
					*(target++) = '=';
					*(target++) = '\r';
					*(target++) = '\n';
					
					remaining_len -= 3;
					current_line_len =0;
				}
				break;
			}
			
		default:
			if(NeedQPEscape(token, 0))
			{
				*(target++) = '=';
				*(target++) = hexchars[(token>>4) & 0x0f];
				*(target++) = hexchars[token & 0x0f];
				
				remaining_len -= 3;
				current_line_len += 3;
			}
			else
			{
				*(target++) = token;
				remaining_len --;
				current_line_len ++;
			}
		}
		
		buffer_pos ++;
		if(current_line_len >= 72)
		{
			// soft breaks line at 72, might have waited until 76
			*(target++) = '=';
			*(target++) = '\r';
			*(target++) = '\n';
			
			remaining_len -= 3;
			current_line_len = 0;
		}
	}

	return target;
}
#endif // URL_UPLOAD_QP_SUPPORT

#ifdef URL_UPLOAD_BASE64_SUPPORT
unsigned char *Upload_Handler::EncodeBase64(unsigned char *target, uint32 &remaining_len, BOOL more)
{
	unsigned char *src = internal_buffer + buffer_pos;

	while (buffer_pos < (more ? buffer_len-3 : buffer_len))
	{
		BOOL append_crlf = (current_line_len >= UPLOAD_BASE64_LINE_LEN-4)
						|| (buffer_pos >= buffer_len-3 && !more);

		if (remaining_len < (uint32) (append_crlf ? 6 : 4))
			break;

		uint32 bits1, bits2, bits3, bits4;
		unsigned char tmp;

		tmp = *(src++); buffer_pos++;
		bits1 = (tmp >> 2) & 0x3f;
		bits2 = (tmp << 4) & 0x30;

		if (buffer_pos < buffer_len)
		{
			tmp = *(src++); buffer_pos++;
			bits2 |= (tmp >> 4) & 0x0f;
			bits3 = (tmp << 2) & 0x3c;
		}
		else
			bits3 = 64;

		if (buffer_pos < buffer_len)
		{
			tmp = *(src++); buffer_pos++;
			bits3 |= (tmp >> 6) & 0x03;
			bits4 = tmp & 0x3f;
		}
		else
			bits4 = 64;

		*(target++) = Base64_Encoding_chars[bits1];
		*(target++) = Base64_Encoding_chars[bits2];
		*(target++) = Base64_Encoding_chars[bits3];
		*(target++) = Base64_Encoding_chars[bits4];

		remaining_len -= 4;

		if(append_crlf)
		{
			*(target++) = '\r';
			*(target++) = '\n';
			current_line_len = 0;
			remaining_len -= 2;
		}
		else
			current_line_len += 4;
	}
	return target;
}
#endif // URL_UPLOAD_BASE64_SUPPORT

unsigned char *Upload_Handler::OutputContent(unsigned char *target, uint32 &remaining_len, BOOL &done)
{
	done = FALSE;
	
	target = Upload_Base::OutputContent(target, remaining_len, done);
	
	if(!done)
		return target;
	
	uint32 portion_len;
	BOOL more;
	
	if(encoding == ENCODING_NONE || encoding == ENCODING_BINARY)
	{
		more = FALSE;
		
		portion_len = GetNextContentBlock(target, remaining_len, more);

		done = !more;
		target += portion_len;
		remaining_len -= portion_len;

		return target;
	}

#if defined URL_UPLOAD_MAILENCODING_SUPPORT || defined URL_UPLOAD_QP_SUPPORT || defined URL_UPLOAD_BASE64_SUPPORT
	CheckInternalBufferL(0);

	uint32 prev_remaining_len = 0;
	more = TRUE;
	while(more && prev_remaining_len != remaining_len)
	{
		more = FALSE;
		if(!last_block)
		{
			uint32 retrieved =GetNextContentBlock(internal_buffer+buffer_len, buffer_size-buffer_len, more);
			
			buffer_len += retrieved;
			last_block = !more;
		}

		prev_remaining_len = remaining_len;
		switch(encoding)
		{
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
		case ENCODING_7BIT:
		case ENCODING_8BIT:
			target = Encode7Or8Bit(target, remaining_len, more);
			break;
#endif
#ifdef URL_UPLOAD_QP_SUPPORT
		case ENCODING_QUOTED_PRINTABLE:
			target = EncodeQP(target, remaining_len, more);
			break;
#endif
#ifdef URL_UPLOAD_BASE64_SUPPORT
		case ENCODING_BASE64:
			target = EncodeBase64(target, remaining_len, more);
			break;
#endif
		default:

			OP_ASSERT(0); // encoding illegal
		}

		if(buffer_pos < buffer_len)
		{
			buffer_len -= buffer_pos;
			op_memmove(internal_buffer, internal_buffer + buffer_pos, buffer_len);

		}
		else
			buffer_len = 0;
		buffer_pos = 0;
	}

	done = (last_block && buffer_len == 0 ? TRUE : FALSE);

#else
	done = TRUE;
	OP_ASSERT(0); // encoding illegal
#endif

	return target;
}

uint32 Upload_Handler::PrepareL(Boundary_List &boundaries, Upload_Transfer_Mode transfer_restrictions)
{
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
	BOOL detect_encoding = FALSE;
#endif
	BOOL calculate_size = FALSE;
	BOOL check_boundary = FALSE;
	OpFileLength payload_len;

	payload_len = PayloadLength();

	if(BodyNeedScan() /* && (transfer_restrictions != UPLOAD_BINARY_NO_CONVERSION ||
			payload_len < 4*1024*1024)*/)
	{
		check_boundary = (boundaries.Cardinal() != 0);

#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
		if(transfer_restrictions != UPLOAD_BINARY_NO_CONVERSION && 
			charset.CompareI("iso-2022-jp") == 0)
		{
			encoding = ENCODING_7BIT;
		}
#endif
#if defined(URL_UPLOAD_MAILENCODING_SUPPORT) && defined(URL_UPLOAD_BASE64_SUPPORT)
		else
#endif
#ifdef URL_UPLOAD_BASE64_SUPPORT
		if(transfer_restrictions != UPLOAD_BINARY_NO_CONVERSION && 
			encoding != ENCODING_BASE64 &&
			payload_len >= 4*1024*1024)
		{
			encoding = ENCODING_BASE64;
		}
#endif

		switch(encoding)
		{
		case ENCODING_AUTODETECT:
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
			if(transfer_restrictions == UPLOAD_7BIT_TRANSFER || transfer_restrictions == UPLOAD_8BIT_TRANSFER)
				detect_encoding = TRUE;
#endif
			break;
#ifdef URL_UPLOAD_BASE64_SUPPORT
		case ENCODING_BASE64:
			check_boundary = FALSE;
#endif
#ifdef URL_UPLOAD_QP_SUPPORT 
		case ENCODING_QUOTED_PRINTABLE:
#endif
#if defined URL_UPLOAD_BASE64_SUPPORT || defined URL_UPLOAD_QP_SUPPORT
			calculate_size = TRUE;
			break;
#endif
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
		case ENCODING_7BIT:
		case ENCODING_8BIT:
#endif
		case ENCODING_NONE:
		case ENCODING_BINARY:
		default:
			break;
		}

#ifdef UPLOAD_NO_BODY_SCAN
		// URL_UPLOAD_BASE64_SUPPORT must be defined
		check_boundary = FALSE; // Trust the 1 : 12 401 769 434 657 526 912 139 264 randomness of the boundary
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
		if (detect_encoding && transfer_restrictions == UPLOAD_8BIT_TRANSFER)
		{
			encoding = ENCODING_8BIT; // Assume content does not have long lines or the receiver can handle it
			detect_encoding = FALSE;
		}
#endif
		if (0
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
			|| detect_encoding
#endif
#ifdef URL_UPLOAD_QP_SUPPORT
			|| encoding == ENCODING_QUOTED_PRINTABLE
#endif
			)
		{
			encoding = ENCODING_BASE64; // Always safe, but 33% bigger
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
			detect_encoding = FALSE;
#endif
			calculate_size = TRUE;
		}
#endif
	}

#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
	uint32 QP_escapes = 0;
	uint32 QP_soft_line_escapes = 0;
	uint32 QP_equals_escapes = 0; // Count '=' separately, QP not needed unless other chars need QP
	uint32 linefeed_escapes = 0;
	uint32 max_line_len = 0;
#endif


	if(check_boundary 
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
		|| detect_encoding 
#endif
#ifdef URL_UPLOAD_QP_SUPPORT 
		|| (calculate_size && encoding == ENCODING_QUOTED_PRINTABLE)
#endif
		|| DecodeToCalculateLength())
	{
		CheckInternalBufferL(payload_len);

		ResetContent();

		BOOL more = TRUE;
		uint32 portion_len =0, unconsumed = 0;
		payload_len = 0;
		current_line_len = 0;

#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
		uint32 QP_line_len = 0;
#endif

#ifdef DEBUG_SCAN
		
		time_t start_time = op_time(NULL);
		
		uint32 bytes_processed = 0;
		
		PrintfTofile("upload.txt", "Starting scan at %lu boundary= %s\n", start_time, boundaries.GetBoundaryL().CStr());
		
		
#endif

		while(more)
		{
			portion_len = GetNextContentBlock(internal_buffer+unconsumed, buffer_size-unconsumed, more);
			payload_len += portion_len;

			portion_len += unconsumed;
			uint32 committed = portion_len;

#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
			if(detect_encoding)
			{
				unsigned char *pos = internal_buffer + unconsumed;
				committed = unconsumed;
				unsigned char token;

				while(committed < portion_len)
				{
					token = *(pos++);

					switch(token)
					{
					case '\r':
					case '\n':
						if(token == '\n')
						{
							linefeed_escapes ++;
							QP_line_len =0;
							if(current_line_len > max_line_len)
								max_line_len = current_line_len;
							current_line_len = 0;
							committed ++;
						}
						else if(committed+1 < portion_len)
						{
							unsigned char token1 = *pos;

							if(token1 == '\n')
							{
								committed ++;
								pos ++;
							}
							else
							{
								linefeed_escapes ++;
							}
							committed ++;
							QP_line_len = 0;
							if(current_line_len > max_line_len)
								max_line_len = current_line_len;
							current_line_len = 0;
						}
						else
							goto force_end_of_scan;
						break;
					case '\t':
					case ' ':
						if(committed+1 < portion_len)
						{
							unsigned char token1 = *pos;

							QP_line_len ++;
							current_line_len ++;
							if(token1 == '\r' || token1 == '\n')
							{
								// Adding "=\r\n" 
								QP_soft_line_escapes += 3;
								QP_line_len =0;
							}
							committed ++;
						}
						else if(!more)
						{
							// Adding "=\r\n" 
							QP_soft_line_escapes += 3;
							QP_line_len = 0;
							current_line_len ++;
							committed ++;
						}
						else
							goto force_end_of_scan;
						break;

					default:
						if(token == 0)
						{
							encoding = ENCODING_BASE64;
							check_boundary = FALSE;
							detect_encoding = FALSE;
							calculate_size = TRUE;

							goto force_end_of_scan;
						}
						else if (NeedQPEscape(token, 0))
						{
							if (token == '=')
								QP_equals_escapes +=2;
							else
								QP_escapes +=2;
							QP_line_len +=2;
						}
						committed ++;
						QP_line_len ++;
						current_line_len ++;
					}

					if(QP_line_len >= 72)
					{
						// soft breaks line at 72, might have waited until 76
						QP_soft_line_escapes += 3;
						QP_line_len = 0;
					}
				}
force_end_of_scan:;

				if(detect_encoding && QP_escapes && transfer_restrictions == UPLOAD_7BIT_TRANSFER && (QP_escapes + linefeed_escapes + QP_soft_line_escapes + QP_equals_escapes) *3 > payload_len)
				{
					// Do not QP encode if it increases the length more than 1/3
					encoding = ENCODING_BASE64;
					check_boundary = FALSE;
					detect_encoding = FALSE;
					calculate_size = TRUE;
				}
			}
#endif // URL_UPLOAD_MAILENCODING_SUPPORT

			unconsumed = 0;
			if(check_boundary)
			{
				uint32 committed2 = 0;

				if(boundaries.ScanForBoundaryL(internal_buffer, portion_len,committed2) == Boundary_Match)
				{
					LEAVE(Upload_Error_Status::ERR_FOUND_BOUNDARY_PREFIX);
				}

#ifdef DEBUG_SCAN
				
				bytes_processed += committed2;
				
#endif
 
 				if(committed2 < committed)
					committed = committed2;
			}

			if(more && committed < portion_len)
			{
				unconsumed = portion_len - committed;
				op_memmove(internal_buffer, internal_buffer + committed, unconsumed);
			}
		}
	}

#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
	if(detect_encoding)
	{
		BOOL use_QP = FALSE;

		if((QP_escapes && transfer_restrictions == UPLOAD_7BIT_TRANSFER) || max_line_len >998)
		{
			use_QP = TRUE;
			QP_escapes += linefeed_escapes +QP_soft_line_escapes + QP_equals_escapes;
			linefeed_escapes = 0;
		}
		if(use_QP)
		{
			calculate_size = TRUE;
			if(QP_escapes *3 > payload_len)
			{
				// Do not QP encode if it increases the length more than 1/3
				encoding = ENCODING_BASE64;
				check_boundary = FALSE;
				detect_encoding = FALSE;
			}
			else
				encoding = ENCODING_QUOTED_PRINTABLE;
		}
		else
			encoding = (transfer_restrictions == UPLOAD_7BIT_TRANSFER ? ENCODING_7BIT : ENCODING_8BIT);
	}
	else
#endif // URL_UPLOAD_MAILENCODING_SUPPORT
	if(encoding == ENCODING_AUTODETECT)
	{
		encoding = ENCODING_NONE;
	}
 

	if(calculate_size)
	{
		if(encoding == ENCODING_BASE64)
			calculated_payload_size = ((payload_len + 2)/3)*4 /*Base64*/ + (payload_len/((UPLOAD_BASE64_LINE_LEN * 3)/ 4))*2 /*CRLFs every UPLOAD_BASE64_LINE_LEN (64) encoded bytes, i.e. every UPLOAD_BASE64_LINE_LEN*3/4 (48) unencoded bytes */;
		else
		{
			calculated_payload_size = payload_len;
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
			calculated_payload_size += QP_escapes + linefeed_escapes;
#endif // URL_UPLOAD_MAILENCODING_SUPPORT
		}
	}
	else
	{
		calculated_payload_size = payload_len;
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
		if (encoding == ENCODING_7BIT || encoding == ENCODING_8BIT)
			calculated_payload_size += linefeed_escapes;
#endif // URL_UPLOAD_MAILENCODING_SUPPORT
	}

#if defined URL_UPLOAD_MAILENCODING_SUPPORT || defined URL_UPLOAD_QP_SUPPORT || defined URL_UPLOAD_BASE64_SUPPORT
	const char *encoding_string = NULL;

	switch(encoding)
	{
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
	case ENCODING_7BIT:
		encoding_string = "7bit";
		break;
	case ENCODING_8BIT:
		encoding_string = "8bit";
		break;
#endif
#ifdef URL_UPLOAD_QP_SUPPORT 
	case ENCODING_QUOTED_PRINTABLE:
		encoding_string = "Quoted-Printable";
		break;
#endif
#ifdef URL_UPLOAD_BASE64_SUPPORT
	case ENCODING_BASE64:
		encoding_string = "Base64";
		break;
#endif
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
	case ENCODING_BINARY:
		encoding_string = "binary";
		break;
#endif
	}

	if(encoding_string)
		AccessHeaders().AddParameterL("Content-Transfer-Encoding", NULL, encoding_string);
#endif

	current_line_len = 0;

	return Upload_Base::PrepareL(boundaries, transfer_restrictions);
}

void Upload_Handler::ResetL()
{
	Upload_Base::ResetL();
	current_line_len = 0;
	last_block = FALSE;
	buffer_pos = 0;
	buffer_len = 0;
}

void Upload_Handler::Release()
{
	FreeBuffer();
}

void Upload_Handler::SetMinimumBufferSize(uint32 size)
{
	if(size > buffer_min_len)
		buffer_min_len = size;
}

void Upload_Handler::ResetContent()
{
}

void Upload_Handler::FreeBuffer()
{
	if(internal_buffer)
		OPERA_cleanse_heap(internal_buffer, buffer_size);

	OP_DELETEA(internal_buffer);
	internal_buffer = NULL;
	buffer_len = buffer_pos = buffer_size = 0;
}

void Upload_Handler::CheckInternalBufferL(OpFileLength max_needed)
{
	uint32 len = buffer_min_len;

	if(len < 32*1024)
		len = 32*1024;

	if(max_needed < len)
		len =  max_needed > 1024 ? (uint32) max_needed : 1024;

	if(len <= buffer_size)
		return;

	unsigned char *temp_internal_buffer = OP_NEWA_L(unsigned char, len);
	uint32	temp_buffer_len = 0; 

	if(internal_buffer && buffer_len && buffer_pos < buffer_len)
	{
		temp_buffer_len = buffer_len-buffer_pos;
		op_memcpy(temp_internal_buffer, internal_buffer + buffer_pos, temp_buffer_len);
	}

	FreeBuffer();

	internal_buffer = temp_internal_buffer;
	buffer_size = len;
	buffer_len = temp_buffer_len;
	// buffer_pos = 0 not necessary
}

#ifdef UPLOAD_PAYLOAD_ACTION_SUPPORT
void Upload_Handler::ProcessPayloadActionL()
{
	ResetContent();
	CheckInternalBufferL(0);

	BOOL more = TRUE;
	while(more)
	{
		more = FALSE;
		uint32 retrieved =GetNextContentBlock(internal_buffer, buffer_size, more);
			
		PerformPayloadActionL(internal_buffer, retrieved);
	}
}

void Upload_Handler::PerformPayloadActionL(unsigned char *src, uint32 src_len)
{
}
#endif

#endif // HTTP_data
