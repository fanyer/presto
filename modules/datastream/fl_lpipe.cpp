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


#include "core/pch.h"

#ifdef DATASTREAM_PIPE_ENABLED

#include "modules/datastream/fl_lib.h"

DataStream_LengthLimitedPipe::DataStream_LengthLimitedPipe(DataStream *src, BOOL take_over)
: DataStream_Pipe(src, take_over), max_length(0), length_set(FALSE), read_length(0)
{
}

DataStream_LengthLimitedPipe::~DataStream_LengthLimitedPipe()
{
}

void DataStream_LengthLimitedPipe::SetReadLength(uint32 length)
{
	max_length = length ;
	length_set = TRUE;
	read_length = 0;
}

#if 0
void DataStream_LengthLimitedPipe::UnsetReadLength()
{
	max_length = 0 ;
	length_set = FALSE;
	read_length = 0;
}
#endif

unsigned long DataStream_LengthLimitedPipe::ReadDataL(unsigned char *buffer, unsigned long len, DataStream::DatastreamCommitPolicy sample)
{
	unsigned long sample_len;

	sample_len = len;
	if(length_set && read_length +sample_len >max_length)
		sample_len = max_length -read_length;

	DS_DEBUG_Start();
	DS_Debug_Printf0("DataStream_LengthLimitedPipe::ReadDataL starting\n");
	
	unsigned long actually_read = DataStream_Pipe::ReadDataL(buffer, sample_len, sample);
	if(sample == DataStream::KReadAndCommit || (sample == DataStream::KReadAndCommitOnComplete && len == actually_read))
	{
		read_length += actually_read;

		if(read_length < max_length && !(DataStream_Pipe::GetAttribute(DataStream::KMoreData) || DataStream_Pipe::GetAttribute(DataStream::KActive)))
			LEAVE(OpRecStatus::RECORD_TOO_SHORT);
	}
	return actually_read;
}

uint32	DataStream_LengthLimitedPipe::GetAttribute(DataStream::DatastreamUIntAttribute attr)
{
	switch(attr)
	{
	case DataStream::KActive:
	case DataStream::KMoreData:
		if(length_set && read_length >= max_length)
			return FALSE;
		break;
	}
	return DataStream_Pipe::GetAttribute(attr);
}

void DataStream_LengthLimitedPipe::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	switch(action)
	{
	case DataStream::KCommitSampledBytes:
		{
			uint32 sample_len;
			
			sample_len = arg1;
			if(length_set && read_length +sample_len >max_length)
				sample_len = max_length -read_length;
			read_length += sample_len;
			arg1 = sample_len;
		}
		// fall-through
	default: 
		DataStream_Pipe::PerformActionL(action, arg1, arg2);
	}
}


#ifdef _DATASTREAM_DEBUG_
const char *DataStream_LengthLimitedPipe::Debug_ClassName()
{
	return "DataStream_LengthLimitedPipe";
}
#endif

#endif // DATASTREAM_PIPE_ENABLED
