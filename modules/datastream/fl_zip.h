/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef __FL_ZIP_H__
#define __FL_ZIP_H__

#ifdef DATASTREAM_DECOMPRESSION_ENABLED

#include "modules/zlib/zlib.h"

enum DataStream_Compress_alg {
	DataStream_Compress_Unknown,
	DataStream_Deflate,
	DataStream_Deflate_No_Header, //<<<< No zlib 2 byte header when sending, either is accepted on receiving
	//DataStream_Gzip,
};

class DataStream_Compression : public DataStream_Pipe
{
private:
	DataStream_Compress_alg alg;
	BOOL compressing;
	BOOL flushed;

	z_stream stream;

	DataStream_ByteArray work_buffer;
	DataStream_ByteArray work_buffer1;
	
	DataStream_FIFO_Stream decompressed_buffer;

private:

	void CompressionStepL(const unsigned char *buffer, unsigned long len, int flush);

public:

	DataStream_Compression(DataStream_Compress_alg _alg, BOOL writing, DataStream *src_trg, BOOL take_over=TRUE);
	virtual ~DataStream_Compression	();

	void InitL();

	virtual unsigned long ReadDataL(unsigned char *buffer, unsigned long len, DataStream::DatastreamCommitPolicy sample = DataStream::KReadAndCommit);
	virtual void WriteDataL(const unsigned char *buffer, unsigned long len);

	virtual uint32 GetAttribute(DataStream::DatastreamUIntAttribute attr);
	virtual void PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2);

	/** Flush compression, without terminating compression */
	void FlushL();
};

#endif
#endif
