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

#ifdef DATASTREAM_DECOMPRESSION_ENABLED

#include "modules/datastream/fl_lib.h"
#include "modules/datastream/fl_zip.h"

#include "modules/libssl/debug/tstdump2.h"

#define FL_ZIP_BUFFER_SIZE 4096

DataStream_Compression::DataStream_Compression(DataStream_Compress_alg _alg, BOOL writing, DataStream *src_trg, BOOL take_over)
 : DataStream_Pipe(src_trg, take_over), alg(_alg), compressing(writing)
{
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	stream.avail_in = 0;
	stream.next_in = NULL;

	stream.avail_out = 0;
	stream.next_out = NULL;

	stream.total_in = 0;
	stream.total_out = 0;
	stream.state  = Z_NULL;
	stream.msg = NULL;

	work_buffer.SetIsFIFO();
	decompressed_buffer.SetIsFIFO();
}

DataStream_Compression::~DataStream_Compression	()
{
	if(compressing)
		deflateEnd(&stream);
#ifdef DATASTREAM_COMPRESSION_ENABLED
	else
		inflateEnd(&stream);
#endif
}

void DataStream_Compression::InitL()
{
	int ret = Z_OK;

	if(compressing)
	{
		switch(alg)
		{
		case DataStream_Deflate:
		case DataStream_Deflate_No_Header:
			ret = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
			break;
		default:
			LEAVE(OpStatus::ERR_OUT_OF_RANGE);
		}
	}
#ifdef DATASTREAM_COMPRESSION_ENABLED
	else
	{
		switch(alg)
		{
		case DataStream_Deflate:
		case DataStream_Deflate_No_Header:
			ret = inflateInit(&stream);
			break;
		default:
			LEAVE(OpStatus::ERR_OUT_OF_RANGE);
		}
	}
#endif

	if(ret != Z_OK)
		LEAVE(ret == Z_MEM_ERROR ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR);

	work_buffer1.SetNeedDirectAccess(TRUE);
	work_buffer1.ResizeL(FL_ZIP_BUFFER_SIZE,TRUE, TRUE);
}

static const unsigned char deflate_dummyheader[2] = {0x78, 0x9c}; /* dummy deflate header, used if deflate files do not start properly */

unsigned long DataStream_Compression::ReadDataL(unsigned char *buffer, unsigned long len, DataStream::DatastreamCommitPolicy sample)
{
	if(compressing)
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);

	work_buffer.AddContentL(AccessStreamSource(), (FL_ZIP_BUFFER_SIZE-512 > work_buffer.GetLength() ? 
										FL_ZIP_BUFFER_SIZE-work_buffer.GetLength() : 512)) ;
	while(decompressed_buffer.GetLength() < len)
	{
		int ret = Z_OK;
		BOOL data_produced = FALSE;

		stream.next_in = work_buffer.GetDirectPayload();
		stream.avail_in = work_buffer.GetLength();
		if(stream.total_in == 0 && stream.next_in && ((((unsigned long) stream.next_in[0]) << 8) + stream.next_in[1]) % 31 != 0)
		{
			stream.next_in = (byte *) deflate_dummyheader;
			stream.avail_in = sizeof(deflate_dummyheader);
			stream.next_out = work_buffer1.GetDirectPayload();
			stream.avail_out = work_buffer1.GetLength();
			ret = inflate(&stream, Z_SYNC_FLUSH);
			
			if(ret != Z_OK && ret != Z_STREAM_END)
				LEAVE(ret == Z_MEM_ERROR ? (OP_STATUS) OpRecStatus::ERR_NO_MEMORY : (OP_STATUS) OpRecStatus::DECOMPRESSION_FAILED);
			continue;
		}

		while(stream.avail_in>0 && decompressed_buffer.GetLength() < len)
		{
			stream.next_out = work_buffer1.GetDirectPayload();
			stream.avail_out = work_buffer1.GetLength();

			ret = inflate(&stream, Z_SYNC_FLUSH);
			
			if(ret != Z_OK && ret != Z_STREAM_END)
				LEAVE(ret == Z_MEM_ERROR ? (OP_STATUS) OpRecStatus::ERR_NO_MEMORY : (OP_STATUS) OpRecStatus::DECOMPRESSION_FAILED);
			
			if(work_buffer1.GetLength() != stream.avail_out)
			{
				data_produced = TRUE;
				decompressed_buffer.WriteDataL(work_buffer1.GetDirectPayload(), work_buffer1.GetLength() - stream.avail_out);
#if 0
				DumpTofile(work_buffer1.GetDirectPayload(), work_buffer1.GetLength() - stream.avail_out, "Plaintext block (post-decompression)", "winsck.txt");
#endif
			}

			if(ret == Z_STREAM_END)
			{
				if(!stream.avail_in && !DataStream_Pipe::GetAttribute(DataStream::KMoreData))
					break;
				// if more data reset decompression, and continue;
				inflateReset(&stream);
			}
		}

		if(!data_produced && work_buffer.GetLength() == stream.avail_in) 
			break;
		
		work_buffer.CommitSampledBytesL(work_buffer.GetLength()-stream.avail_in);
		
		if(decompressed_buffer.GetLength() >= len)
			break;

		work_buffer.AddContentL(AccessStreamSource(), (FL_ZIP_BUFFER_SIZE-512 > work_buffer.GetLength() ? 
											FL_ZIP_BUFFER_SIZE-work_buffer.GetLength() : 512)) ;
	}

	return decompressed_buffer.ReadDataL(buffer, len, sample);
}

#ifdef DATASTREAM_COMPRESSION_ENABLED
void DataStream_Compression::CompressionStepL(const unsigned char *buffer, unsigned long len, int flush)
{
	OP_ASSERT(buffer);
#if 0
	DumpTofile(buffer, len, "Plaintext block (pre-compression)", "winsck.txt");
#endif

	stream.next_in = (unsigned char *) buffer;
	stream.avail_in = len;

	do
	{
		uint32 offset = (alg != DataStream_Deflate_No_Header || stream.total_out ? 0 : 2);
		stream.next_out = work_buffer1.GetDirectPayload();
		stream.avail_out = work_buffer1.GetLength();

		int ret = deflate(&stream, flush);
		
		if(ret != Z_OK)
			LEAVE(ret == Z_MEM_ERROR ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR);
		
		DataStream_Pipe::WriteDataL(work_buffer1.GetDirectPayload()+offset, work_buffer1.GetLength() - stream.avail_out - offset);
	}
	while(stream.avail_in>0 || stream.avail_out == 0);
}
#endif

void DataStream_Compression::WriteDataL(const unsigned char *buffer, unsigned long len)
{
#ifdef DATASTREAM_COMPRESSION_ENABLED
	if(!compressing || buffer == NULL || len == 0)
		return;

	CompressionStepL(buffer, len, Z_NO_FLUSH);
#endif
}

uint32 DataStream_Compression::GetAttribute(DataStream::DatastreamUIntAttribute attr)
{
	switch(attr)
	{
	case DataStream::KMoreData:
		if(decompressed_buffer.MoreData())
			return TRUE;
	}

	return DataStream_Pipe::GetAttribute(attr);
}

void DataStream_Compression::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
#ifdef DATASTREAM_COMPRESSION_ENABLED
	switch(action)
	{
	case DataStream::KFinishedAdding:
		if(!compressing)
			return;

		CompressionStepL((byte *) g_memory_manager->GetTempBuf(), 0, Z_FINISH);
	}
#endif
		
	DataStream_Pipe::PerformActionL(action, arg1, arg2);
}

void DataStream_Compression::FlushL()
{
#ifdef DATASTREAM_COMPRESSION_ENABLED
	if(!compressing)
		return;

	CompressionStepL((byte *) g_memory_manager->GetTempBuf(), 0, Z_SYNC_FLUSH);
#endif
}


#endif
