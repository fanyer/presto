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

#ifdef DATASTREAM_REFERENCE_ENABLED

#include "modules/datastream/fl_lib.h"

DataStream_StreamReference::DataStream_StreamReference(DataStream *trgt)
{
	target = trgt;
	//OP_ASSERT(target);
}

DataStream_StreamReference::~DataStream_StreamReference()
{
	target = NULL; // No delete
}

OP_STATUS DataStream_StreamReference::PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target)
{
	switch(action)
	{
	case DataStream::KReadRecord:
	case DataStream::KWriteRecord:
		return (target ? target->PerformStreamActionL(action, src_target) : OpRecStatus::FINISHED);
	}

	return DataStream::PerformStreamActionL(action, src_target);
}

unsigned long DataStream_StreamReference::ReadDataL(unsigned char *buffer, unsigned long len, DataStream::DatastreamCommitPolicy sample)
{
	return (target ? target->ReadDataL(buffer, len, sample) : 0);
}

void	DataStream_StreamReference::WriteDataL(const unsigned char *buffer, unsigned long len)
{
	if(target) 
		target->WriteDataL(buffer, len);
}

DataStream *DataStream_StreamReference::CreateRecordL()
{
	return (target ? target->CreateRecordL() : NULL);
}

uint32	DataStream_StreamReference::GetAttribute(DataStream::DatastreamUIntAttribute attr)
{
	if(target)
		return target->GetAttribute(attr);

	return DataStream::GetAttribute(attr);
}

void DataStream_StreamReference::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	switch(action)
	{
	case DataStream::KCommitSampledBytes:
	case DataStream::KFinishedAdding:
	case DataStream::KReadAction:
	case DataStream::KResetRead:
	case DataStream::KResetRecord:
	case DataStream::KSetItemID:
	case DataStream::KWriteAction:
		if(target)
			target->PerformActionL(action, arg1, arg2);
		break;
	default: 
		DataStream::PerformActionL(action, arg1, arg2);
	}
}

#ifdef _DATASTREAM_DEBUG_
void DataStream_StreamReference::Debug_Write_All_Data()
{
	DS_DEBUG_Start();

	DS_Debug_Printf0("DataStream_StreamReference Starting dump\n");
	if(target)
		target->Debug_Write_All_Data();
	DS_Debug_Printf0("DataStream_StreamReference Ending dump\n");
}
#endif

#endif // DATASTREAM_REFERENCE_ENABLED
