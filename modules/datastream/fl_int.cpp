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

#ifdef DATASTREAM_UINT_ENABLED

#include "modules/datastream/fl_lib.h"


DataStream_UIntBase::DataStream_UIntBase(uint32 sz, uint32 val)
{
	read_data = FALSE;
	size = sz;
	big_endian = TRUE;
	MSB_detection = FALSE;
	value = val;
}

OP_STATUS DataStream_UIntBase::PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target)
{
	switch(action)
	{
	case DataStream::KReadRecord:
		{
			read_data = FALSE;
			OP_STATUS op_err;
			if(integer_buffer.get() != NULL)
			{
				OP_ASSERT(integer_buffer->GetLength() < 16);
				op_err = integer_buffer->PerformStreamActionL(DataStream::KReadRecord, src_target);
				if(op_err != OpRecStatus::FINISHED)
					return op_err;
				
				src_target = integer_buffer.get();
			}
			op_err = ReadIntegerL(src_target, value, size, big_endian, MSB_detection);
			if(op_err == OpRecStatus::FINISHED)
			{
				integer_buffer.reset();
				read_data = TRUE;
			}
			else
			{
				OP_ASSERT(integer_buffer.get() == NULL);

				integer_buffer.reset(OP_NEW_L(DataStream_ByteArray, ()));
				integer_buffer->SetNeedDirectAccess(TRUE);
				integer_buffer->SetFixedSize(TRUE);
				integer_buffer->ResizeL(size);

				op_err = integer_buffer->PerformStreamActionL(DataStream::KReadRecord, src_target);
				OP_ASSERT(op_err != OpRecStatus::FINISHED);
			}
			
			return op_err;
		}
	case DataStream::KWriteRecord:
		WriteIntegerL(value, size, big_endian, MSB_detection, src_target);
		return OpRecStatus::FINISHED;
	}

	return DataStream::PerformStreamActionL(action, src_target);
}

uint32	DataStream_UIntBase::GetAttribute(DataStream::DatastreamUIntAttribute attr)
{
	switch(attr)
	{
	case DataStream::KFinished:
		return read_data;
	case DataStream::KCalculateLength:
		return size;
	}
	
	return DataStream::GetAttribute(attr);
}

#ifdef _DATASTREAM_DEBUG_
void DataStream_UIntBase::Debug_Write_All_Data()
{
	DS_DEBUG_Start();

	DS_Debug_Printf("DataStream_UIntBase %u bytes integer value %u\n", size, value);
	//DataStream::Debug_Write_All_Data();
}
#endif

#endif // DATASTREAM_UINT_ENABLED
