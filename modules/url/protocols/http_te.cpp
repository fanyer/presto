/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/hardcore/mem/mem_man.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/util/simset.h"
#include "modules/util/handy.h"

#include "modules/url/url2.h"
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/connman.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/protocols/http_req2.h"
#include "modules/url/protocols/http_te.h"

#ifdef _HTTP_COMPRESS

BOOL HTTP_Request::AddTransferDecodingL(HTTP_compression mth)
{
	HTTP_Transfer_Decoding *decoder = HTTP_Transfer_Decoding::Create(mth);

	LEAVE_IF_NULL(decoder);

	decoder->Into(&TE_decoding);

	return !decoder->Error();
}

void URL_DataDescriptor::SetupContentDecodingL(const char *header)
{
	if(!header)
		LEAVE(OpStatus::ERR);
	
	ParameterList params(KeywordIndex_HTTP_General_Parameters); 
	ANCHOR(ParameterList, params);
	
	params.SetValueL((char *) header, PARAM_SEP_COMMA| PARAM_NO_ASSIGN | PARAM_ONLY_SEP);

	Parameters *encoding = params.First();

	while(encoding)
	{
		ParameterList *current_encoding = encoding->GetParametersL(PARAM_SEP_SEMICOLON, KeywordIndex_HTTP_General_Parameters);
		Parameters *name = (current_encoding ? current_encoding->First() : NULL);
		
		if(name)
		{
			HTTP_compression mth;
			
			switch(mth = (HTTP_compression) name->GetNameID())
			{
			case HTTP_Deflate:
			case HTTP_Compress:
			case HTTP_Gzip:
				{
					HTTP_Transfer_Decoding *decoder = HTTP_Transfer_Decoding::Create(mth);
					if(decoder)
						decoder->Into(&CE_decoding);
					else
					{
						g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
						LEAVE(OpStatus::ERR_NO_MEMORY);
					}
				}
				break;

			default: break; // nothing to do for plaintext
			}
		}
		encoding = encoding->Suc();
	}
}

HTTP_Transfer_Decoding::HTTP_Transfer_Decoding()
{

	buffer_len = 0;
	buffer_used = 0;
	
	state.next_in = Z_NULL;
	state.avail_in = 0;
	state.total_in = 0;
	state.next_out = Z_NULL;
	state.avail_out = 0;
	state.total_out = 0;
	state.msg = NULL;
	state.state  = Z_NULL;
	state.zalloc = Z_NULL;
	state.zfree = NULL;
	state.opaque = Z_NULL;

	deflate_first_byte = TRUE;
	first_read = TRUE;
#ifndef URL_USE_ZLIB_FOR_GZIP
	decompress_step  = NULL;
	decompress_final = NULL;
#endif

}

OP_STATUS HTTP_Transfer_Decoding::Construct(HTTP_compression mth)
{
	OP_STATUS status = OpStatus::OK;

	int result;
	method = mth;

	switch(method)
	{
	case HTTP_Deflate:
#ifdef URL_USE_ZLIB_FOR_GZIP
	case HTTP_Gzip:
#endif
		if((result = inflateInit2(&state, MAX_WBITS + 32 /* enable zlib or gzip decompression */)) != Z_OK)
		{
			finished = TRUE;
			error = TRUE;
			if (result == Z_MEM_ERROR)
			{
				status = OpStatus::ERR_NO_MEMORY;
			}
			break;
		}
#ifndef URL_USE_ZLIB_FOR_GZIP
		decompress_step  = inflate;
		decompress_final = inflateEnd;
#endif
		break;
#ifndef URL_USE_ZLIB_FOR_GZIP
	case HTTP_Gzip:
		ResetGzip();
		inflateInit2(&state, -MAX_WBITS);
		decompress_step  = inflate;
		decompress_final = inflateEnd;
		
		break;
#endif
#ifdef URL_USE_ZLIB_FOR_GZIP
	case HTTP_Identity:
		break;
#endif
	case HTTP_Compress:
	default:
		error = TRUE;
		finished = TRUE;
	}

	if(!finished)
	{
		buffer_len = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize) *1024;
		buffer_used = 0;
		buffer = OP_NEWA(unsigned char, buffer_len);
		if(!buffer)
		{
			finished = TRUE;
			status = OpStatus::ERR_NO_MEMORY;
		}
	}
	else
		buffer = NULL;

	return status;
}

HTTP_Transfer_Decoding* HTTP_Transfer_Decoding::Create(HTTP_compression mth)
{
	HTTP_Transfer_Decoding* hte = OP_NEW(HTTP_Transfer_Decoding, ());
	if (!hte)
		return NULL;
	OP_STATUS status = hte->Construct(mth);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(hte);
		return NULL;
	}
	return hte;
}


HTTP_Transfer_Decoding::~HTTP_Transfer_Decoding()
{
	switch(method)
	{
	case HTTP_Deflate:
	case HTTP_Gzip:
		inflateEnd(&state);
		break;
	}

	if(buffer)
		OP_DELETEA(buffer);

	buffer = 0;
}

static const unsigned char deflate_dummyheader[2] = {0x78, 0x9c}; /* dummy deflate header, used if deflate files do not start properly */
static const unsigned char gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */

unsigned int HTTP_Transfer_Decoding::DoDecodingStep(char *buf, unsigned int blen)
{
	unsigned int buf_ret = 0;

	if(!finished /* && buffer_used > 0*/)
	{
#ifdef URL_USE_ZLIB_FOR_GZIP
		if (method == HTTP_Identity)
		{
			buf_ret = buffer_used;
			if(buf_ret > blen)
				buf_ret = blen;

			op_memcpy(buf, buffer, buf_ret);
			if(buffer_used > buf_ret)
				op_memmove(buffer, buffer+buf_ret, buffer_used - buf_ret);
			buffer_used -= buf_ret;
			if(source_is_finished && buffer_used == 0)
				finished = TRUE;
			return buf_ret;
		}
#endif
#ifndef URL_USE_ZLIB_FOR_GZIP
		if(method == HTTP_Deflate && deflate_first_byte)
		{
			if(buffer_used >= 2 && buffer[0] == gz_magic[0] && buffer[1] == gz_magic[1])
			{
				method = HTTP_Gzip;
				ResetGzip();
				inflateEnd(&state);
				inflateInit2(&state, -MAX_WBITS);
				decompress_step  = inflate;
				decompress_final = inflateEnd;
			}
		}

		if(method == HTTP_Gzip)
		{
			if(gzip_state.reading_final_crc)
			{
				unsigned int used = DoGzipFinalStep(buffer, buffer_used);

				if(used)
				{
					if(used < buffer_used)
						op_memmove(buffer, buffer + used, (buffer_used - used));
					buffer_used -= used;
				}
				if(gzip_state.reading_final_crc && !source_is_finished)
					return buf_ret;
				if(buffer_used == 0 && source_is_finished)
				{
					finished = TRUE;
					return buf_ret;
				}
			}
			if(!gzip_state.read_header)
			{
				if(source_is_finished && buffer_used == 0)
				{
					finished = TRUE;
					return buf_ret;
				}
				unsigned int used = DoGzipFirstStep(buffer, buffer_used);

				if(used)
				{
					if(used < buffer_used)
						op_memmove(buffer, buffer + used, (buffer_used - used));
					buffer_used -= used;
				}
			}
			if(!gzip_state.read_header)
				return buf_ret;
			if(gzip_state.transparent)
			{
				buf_ret = (buffer_used > blen ? blen : buffer_used);
				op_memcpy(buf, buffer, buf_ret);
				if(buffer_used > buf_ret)
					op_memmove(buffer, buffer + buf_ret, (buffer_used - buf_ret));
				buffer_used -= buf_ret;
				if(source_is_finished && buffer_used == 0)
					finished = TRUE;
				return buf_ret;
			}
		}
#endif

		state.next_out = (unsigned char *) buf;
		state.avail_out = blen;

		if(method == HTTP_Deflate && deflate_first_byte)
		{
			if(buffer_used >= 2 || source_is_finished)
			{
				BOOL use_dummy_header = FALSE;
				if(buffer_used >= 2)
				{
					if(buffer_used >= 2 && buffer[0] == gz_magic[0] && buffer[1] == gz_magic[1])
					{
						method = HTTP_Gzip;
					}
					else if(((((unsigned long) buffer[0]) << 8) + buffer[1]) % 31 != 0)
					{
						use_dummy_header = TRUE;
					}
					else
					{
						state.next_in = buffer;
						state.avail_in = 2;
						
						int result = inflate(&state, Z_SYNC_FLUSH);
						if(result != Z_OK)
						{
							if(result == Z_DATA_ERROR)
							{
								inflateEnd(&state);
								result = inflateInit(&state);
							}

							if(result != Z_OK && result != Z_STREAM_END)
							{
								finished = TRUE;
								error = TRUE;
								if (result == Z_MEM_ERROR)
									g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
							}
							
							if(error)
								return buf_ret;
						
							// because too many sites are sending illegal content, use fake header (may break unknown formats)
							if(result == Z_OK)
								use_dummy_header = TRUE;
						}
						else
						{
							// header successfully read
							buffer_used-=2;
							op_memmove(buffer, buffer+2, buffer_used);
						}
					}
				}

				if(use_dummy_header)
				{
					state.next_in = (unsigned char *) deflate_dummyheader;
					state.avail_in = sizeof(deflate_dummyheader);

					int result = inflate(&state, Z_SYNC_FLUSH);
					if(result != Z_OK)
					{
						finished = TRUE;
						if(result != Z_STREAM_END)
						{
							error = TRUE;
							if (result == Z_MEM_ERROR)
								g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
						}
						
						if(error)
							return buf_ret;
					}
				}
				deflate_first_byte = FALSE;
			}
			else
				return 0;
		}

		state.next_in = buffer;
		state.avail_in = buffer_used;
		BOOL data_available = (buffer_used > 0);

#ifdef _DEBUG_TE
	DumpTofile(buffer,buffer_used, "Decompress buffered Data", "http.txt");
#endif
		
		int result = inflate(&state, Z_SYNC_FLUSH);
		buf_ret = blen - state.avail_out;

		if(result == Z_OK && buf_ret == 0 && source_is_finished && blen > 1024)
		{
			result = inflate(&state, Z_FINISH);
			finished = TRUE;
		}
#ifdef URL_USE_ZLIB_FOR_GZIP
		else if(result == Z_DATA_ERROR && first_read && buf_ret == 0)
		{
			// Bad encoding detected at start of content, change to identity mode
			inflateEnd(&state);
			method = HTTP_Identity;
			return DoDecodingStep(buf, blen); // Redo from start to avoid returning 0 bytes here
		}
#endif

		if(state.avail_in)
		{
			if(state.avail_in != buffer_used)
			{
				op_memmove(buffer, state.next_in, state.avail_in);
				buffer_used = state.avail_in;
			}
		}
		else
			buffer_used = 0;

#ifndef URL_USE_ZLIB_FOR_GZIP
		if(method == HTTP_Gzip && buf_ret)
			gzip_state.crc = crc32(gzip_state.crc, (unsigned char *) buf, buf_ret);
#endif

		if(result == Z_BUF_ERROR)
		{
			if(source_is_finished)
				finished = TRUE;
		}
		else if(result != Z_OK)
		{
			finished = TRUE;
			if(result != Z_STREAM_END)
			{
				error = TRUE;
				if (result == Z_DATA_ERROR && state.total_out>0)
					can_error_be_hidden = TRUE; // hide error from the client code if managed to decode something (see CORE-42991)
				if (result == Z_MEM_ERROR)
					g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			}
		}
		if(data_available)
			first_read = FALSE;

		if(finished)
		{
#ifndef URL_USE_ZLIB_FOR_GZIP
			if(method != HTTP_Gzip || error)
				return buf_ret;

			// GZip final
			finished = FALSE;
			gzip_state.reading_final_crc = TRUE;
			gzip_state.file_crc = 0;
			gzip_state.remaining_bytes = 8;
			unsigned int used = DoGzipFinalStep(buffer, buffer_used);

			if(used)
			{
				if(used < buffer_used)
					op_memmove(buffer, buffer + used, (buffer_used - used));
				buffer_used -= used;
			}
			if(gzip_state.reading_final_crc)
				return buf_ret;
			if(source_is_finished && buffer_used == 0)
				finished = TRUE;
#endif
			return buf_ret;
		}
	}

	//if(source_is_finished && buffer_used == 0)
	//	finished = TRUE;

	return buf_ret;
}


#ifndef URL_USE_ZLIB_FOR_GZIP
/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

void HTTP_Transfer_Decoding::ResetGzip()
{
	gzip_state.read_header = FALSE;
	gzip_state.transparent= FALSE;
	gzip_state.read_magic= FALSE;
	gzip_state.read_method= FALSE;
	gzip_state.read_flag= FALSE;
	gzip_state.read_discarded= FALSE;
	gzip_state.read_extra_len_lo= FALSE;
	gzip_state.read_extra_len_hi= FALSE;
	gzip_state.read_extra= FALSE;
	gzip_state.read_name= FALSE;
	gzip_state.read_comment= FALSE;
	gzip_state.read_crc_1= FALSE;
	gzip_state.read_crc_2= FALSE;
	gzip_state.reading_final_crc = FALSE;
	gzip_state.remaining_bytes = 2;
	gzip_state.crc = 0;
	gzip_state.file_crc = 0;
}

unsigned int HTTP_Transfer_Decoding::DoGzipFirstStep(unsigned char *buf, unsigned int blen)
{
	if(gzip_state.read_header)
		return 0;

	unsigned int pos=0;

	while(!gzip_state.read_header && pos < blen)
	{
		if(!gzip_state.read_magic)
		{
			if(blen - pos < 2)
			{
				if(source_is_finished)
					gzip_state.read_header = gzip_state.transparent = TRUE;
				return pos;
			}
			gzip_state.read_magic = TRUE;
			if(buf[pos] != gz_magic[0] || buf[pos+1] != gz_magic[1])
			{
				gzip_state.read_header = gzip_state.transparent = TRUE;
				return pos;
			}
			pos ++;
		}
		else if(!gzip_state.read_method)
		{
			gzip_state.read_method = TRUE;
			BYTE meth = buf[pos];
			if(meth != Z_DEFLATED)
			{
				finished = error = TRUE;
				return pos;
			}
		}
		else if(!gzip_state.read_flag)
		{
			gzip_state.read_flag = TRUE;
			BYTE flag = buf[pos];
			if((flag & RESERVED) != 0)
			{
				finished = error = TRUE;
				return pos;
			}
			if((flag & EXTRA_FIELD) == 0)
				gzip_state.read_extra = gzip_state.read_extra_len_lo = gzip_state.read_extra_len_hi = TRUE;
			if((flag & ORIG_NAME) == 0)
				gzip_state.read_name = TRUE;
			if((flag & COMMENT) == 0)
				gzip_state.read_comment = TRUE;
			if((flag & HEAD_CRC) == 0)
				gzip_state.read_crc_1 = gzip_state.read_crc_2 = TRUE;
			gzip_state.remaining_bytes = 6;
		}
		else if(!gzip_state.read_discarded)
		{
			gzip_state.remaining_bytes--;
			if(gzip_state.remaining_bytes == 0)
				gzip_state.read_discarded = TRUE;
		}
		else if(!gzip_state.read_extra_len_lo)
		{
			gzip_state.read_extra_len_lo = TRUE;
			gzip_state.remaining_bytes = buf[pos];
		}
		else if(!gzip_state.read_extra_len_hi)
		{
			gzip_state.read_extra_len_hi = TRUE;
			gzip_state.remaining_bytes += ((unsigned int) buf[pos]) << 8;
		}
		else if(!gzip_state.read_extra)
		{
			gzip_state.remaining_bytes--;
			if(gzip_state.remaining_bytes == 0)
				gzip_state.read_extra = TRUE;
		}
		else if(!gzip_state.read_name)
		{
			if(buf[pos] == 0)
				gzip_state.read_name = TRUE;
		}
		else if(!gzip_state.read_comment)
		{
			if(buf[pos] == 0)
				gzip_state.read_comment = TRUE;
		}
		else if(!gzip_state.read_crc_1)
			gzip_state.read_crc_1 = TRUE;
		else if(!gzip_state.read_crc_2)
		{
			gzip_state.read_crc_2 = TRUE;
		}
		else
		{
			gzip_state.read_header = TRUE;
			gzip_state.file_crc = crc32(0L, Z_NULL, 0);
			break;
		}

		pos++;
	}

	return pos;
}

unsigned int HTTP_Transfer_Decoding::DoGzipFinalStep(unsigned char *buf, unsigned int blen)
{
	unsigned pos = 0;

	while(pos < blen && gzip_state.reading_final_crc)
	{
		if(gzip_state.remaining_bytes >4)
			gzip_state.file_crc |= ((unsigned long) buf[pos]) << ((8-gzip_state.remaining_bytes)*8);
		gzip_state.remaining_bytes--;
		pos++;
		if(!gzip_state.remaining_bytes)
		{
			gzip_state.reading_final_crc = FALSE;
			if(gzip_state.file_crc && gzip_state.file_crc !=  gzip_state.crc)
				error = finished = TRUE;
			ResetGzip();
			// *** CHECK RETURN VALUE
			inflateReset(&state);
		}
	}			
	return pos;
}
#endif

unsigned int HTTP_Transfer_Decoding::ReadData(char *buf, unsigned int blen, HTTP_1_1 *conn, BOOL &read_from_connection)
{
	read_from_connection = FALSE;
	if(finished || error || buf == NULL || blen == 0)
	{
		if(conn)
		{
			Data_Decoder::ReadData((char *) buffer + buffer_used,buffer_len - buffer_used, conn, read_from_connection);
		}

		return 0;
	}

	unsigned int buf_ret = DoDecodingStep(buf,blen);
	if(buf_ret == blen || finished)
		return buf_ret;

	unsigned int read_data; 
	
	read_data = Data_Decoder::ReadData((char *) buffer + buffer_used,buffer_len - buffer_used, conn, read_from_connection);
	buffer_used += read_data;

	buf_ret += DoDecodingStep(buf + buf_ret,blen - buf_ret);

	return buf_ret;
}


unsigned int HTTP_Transfer_Decoding::ReadData(char *buf, unsigned int blen, 
					URL_DataDescriptor *desc, BOOL &read_storage, BOOL &more)
{
	more = FALSE;
	read_storage = FALSE;
	if(finished || error || buf == NULL || blen == 0)
	{
		if(desc)
		{
			Data_Decoder::ReadData((char *) buffer + buffer_used,buffer_len - buffer_used, desc, read_storage, more);
		}

		return 0;
	}

	unsigned int buf_ret = DoDecodingStep(buf,blen);
	if(buf_ret == blen || finished)
	{
		more = !finished;
		return buf_ret;
	}

	unsigned int read_data; 
	
	read_data = Data_Decoder::ReadData((char *) buffer + buffer_used,
									   buffer_len - buffer_used,
									   desc, read_storage, more);
	buffer_used += read_data;

	buf_ret += DoDecodingStep(buf + buf_ret,blen - buf_ret);

	more = ((more || !finished) && !error);
	return buf_ret;
}

#endif // _HTTP_COMPRESS

Data_Decoder::Data_Decoder()
{
	source_is_finished = FALSE;
	finished = FALSE;
	error = FALSE;
	can_error_be_hidden = FALSE;
}

Data_Decoder::~Data_Decoder()
{
}


BOOL Data_Decoder::Error()
{
	if(error)
		return TRUE;
	Data_Decoder *next = (Data_Decoder *) Suc();
	return next && next->Error();
}

BOOL Data_Decoder::CanErrorBeHidden()
{
	return can_error_be_hidden;
}

unsigned int Data_Decoder::ReadData(char *buf, unsigned int blen, HTTP_1_1 *conn, BOOL &read_from_connection)
{
	read_from_connection = FALSE;
	if(finished || error || buf == NULL || blen == 0)
	{
		if(conn)
		{
			if(buf)
				conn->ReadData(buf, blen);
			read_from_connection = TRUE;
		}

		return 0;
	}

	Data_Decoder *source = (Data_Decoder *) Suc();
	unsigned int read_data = 0;
	if(source)
	{
		if(!source->Finished())
		{
			read_data = source->ReadData(buf, blen, conn, read_from_connection);
		}
		source_is_finished = source->Finished();
		can_error_be_hidden = source->CanErrorBeHidden();
	}
	else if(conn)
	{
		read_data = conn->ReadData(buf, blen);
		read_from_connection = TRUE;
	}
	else
		source_is_finished = TRUE;

#ifdef _DEBUG_TE
	DumpTofile((unsigned char *) buf, read_data, "Decompress reading Data", "http.txt");
#endif

	return read_data;
}

unsigned int Data_Decoder::ReadData(char *buf, unsigned int blen, URL_DataDescriptor *desc, BOOL &read_storage, BOOL &more)
{
	Data_Decoder *source = (Data_Decoder *) Suc();
	BOOL override_source_is_finished = FALSE; // used to override setting source_is_finished to FALSE (when using datadescriptiors without MessageHandlers)
	unsigned int read_data = 0;
	if(source)
	{
		if(!source->Finished())
		{
			read_data = source->ReadData(buf, blen,
										 desc, read_storage, more);
		}
		source_is_finished = source->Finished();
		can_error_be_hidden = source->CanErrorBeHidden();
	}
	else if(desc)
	{
		read_data = desc->RetrieveData(more);
		if(!more && !desc->HasMessageHandler() && !desc->FinishedLoading())
			override_source_is_finished = TRUE;

		if(more || !override_source_is_finished)
			source_is_finished = !more;
		if(read_data > blen)
		{
			read_data = blen;
			source_is_finished = FALSE;
			more = TRUE;
		}
		if(read_data)
		{
			op_memcpy(buf, desc->GetBuffer(), read_data);
			desc->ConsumeData(read_data);
		}
		read_storage = TRUE;

	}
	else 
		source_is_finished = TRUE;

#ifdef _DEBUG_TE
	DumpTofile((unsigned char *) buf, read_data, "Decompress reading Data", "http.txt");
#endif

	if(!read_storage && !source_is_finished)
		more = TRUE;

	return read_data;
}
