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


DataStream_Pipe::DataStream_Pipe(DataStream *src, BOOL take_over)
: source(src), own_source(take_over)
{
}

DataStream_Pipe::~DataStream_Pipe()
{
	SetStreamSource(NULL);
}

void DataStream_Pipe::SetStreamSource(DataStream *src, BOOL take_over)
{
	if(source && own_source)
		OP_DELETE(source);
	source = src;
	own_source = take_over;
}

unsigned long DataStream_Pipe::ReadDataL(unsigned char *buffer, unsigned long len, DataStream::DatastreamCommitPolicy sample)
{
	DS_DEBUG_Start();
	DS_Debug_Printf0("DataStream_Pipe::ReadDataL starting\n");
	return (source ? source->ReadDataL(buffer, len, sample) : 0);
}

void DataStream_Pipe::WriteDataL(const unsigned char *buffer, unsigned long len)
{
	DS_DEBUG_Start();
	DS_Debug_Write("DataStream_Pipe::WriteDataL", "", buffer, len);
	if(source)
		source->WriteDataL(buffer, len);
}

uint32	DataStream_Pipe::GetAttribute(DataStream::DatastreamUIntAttribute attr)
{
	switch(attr)
	{
	case DataStream::KActive:
	case DataStream::KMoreData:
		return (source ? source->GetAttribute(attr) : 0);
	}
	return DataStream::GetAttribute(attr);
}

void DataStream_Pipe::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	switch(action)
	{
	case DataStream::KCommitSampledBytes:
	case DataStream::KFinishedAdding:
		if(source)
			source->PerformActionL(action, arg1, arg2);
		break;
	default: 
		DataStream::PerformActionL(action, arg1, arg2);
	}
}

/*
DataStream *DataStream_Pipe::CreateRecordL()
{
	return NULL;
}
*/

#ifdef _DATASTREAM_DEBUG_
const char *DataStream_Pipe::Debug_ClassName()
{
	return "DataStream_Pipe";
}
#endif

#endif // DATASTREAM_PIPE_ENABLED
