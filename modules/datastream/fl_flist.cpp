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

#include "modules/datastream/fl_lib.h"

#ifdef DATASTREAM_USE_FLEXIBLE_SEQUENCE
DataStream_FlexibleSequence::DataStream_FlexibleSequence()
{
	currently_loading_element = NULL;
}

DataStream_FlexibleSequence::~DataStream_FlexibleSequence()
{
	Clear();
	OP_DELETE(currently_loading_element);
}

OP_STATUS DataStream_FlexibleSequence::PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target)
{
	if(action == DataStream::KReadRecord)
	{
		DataStream *src = src_target;

		if(src == NULL)
			return OpRecStatus::FINISHED;

		while(src->GetAttribute(DataStream::KMoreData))
		{
			if(currently_loading_element == NULL)
			{
				currently_loading_element = CreateRecordL();
				if(currently_loading_element == NULL)
					DS_LEAVE(OpRecStatus::UNSUPPORTED_FORMAT);
			}

			OP_STATUS op_err = currently_loading_element->PerformStreamActionL(DataStream::KReadRecord, src);
			if(OpRecStatus::IsError(op_err))
				return op_err;
			if(op_err != OpRecStatus::FINISHED)
				break;

			currently_loading_element->Into(this);

			currently_loading_element = NULL;
		}

		if(!(src->GetAttribute(DataStream::KMoreData) || src->GetAttribute(DataStream::KActive)))
		{
			if(currently_loading_element)
				return OpRecStatus::RECORD_TOO_SHORT;

			DataStream_FlexibleSequence::PerformActionL(DataStream::KFinishedAdding);
			return OpRecStatus::FINISHED;
		}

		return OpRecStatus::WAIT_FOR_DATA;
	}

	return DataStream_SequenceBase::PerformStreamActionL(action, src_target);
}

void DataStream_FlexibleSequence::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	if(action == DataStream::KResetRecord)
	{
		Clear();
		OP_DELETE(currently_loading_element);
	}
	DataStream_SequenceBase::PerformActionL(action, arg1, arg2);
}

DataStream *DataStream_FlexibleSequence::Item(uint32 idx)
{
	DataStream *item = First();

	if(!item || !idx)
		return item;

	//idx--;
	while(idx && item)
	{
		item = item->Suc();
		idx--;
	}

	return item;
}

const DataStream *DataStream_FlexibleSequence::Item(uint32 idx) const
{
	/* const */ DataStream *item = ((DataStream_FlexibleSequence *) this)->First();

	if(!item || !idx)
		return item;

	//idx--;
	while(idx && item)
	{
		item = item->Suc();
		idx--;
	}

	return item;
}

void DataStream_FlexibleSequence::ResizeL(uint32 nlength)
{
	uint32 current_len = Count();

	if(current_len == nlength)
		return;

	if(current_len > nlength)
	{
		DataStream *item, *next_item = Item(nlength);

		while(next_item)
		{
			item = next_item;
			next_item = item->Suc();

			item->Out();
			OP_DELETE(item);
		}
	}
	else
	{
		while(current_len < nlength)
		{
			DataStream *item = CreateRecordL();

			if(item == NULL)
				DS_LEAVE(OpRecStatus::ERR_NULL_POINTER);

			item->Into(this);
			current_len ++;
		}
	}
}

/*    
BOOL DataStream_FlexibleSequence::ContainL(DataStream &source)
{
	uint32 index=0;
	return ContainL(source, index);
}
*/

BOOL DataStream_FlexibleSequence::ContainL(DataStream &source, uint32& index)
{
	DataStream_ByteArray source_binary;
	ANCHOR(DataStream_ByteArray, source_binary);
	DataStream_ByteArray target_binary;
	ANCHOR(DataStream_ByteArray, target_binary);
	DataStream *target;
	uint32 i, len;

	index = 0;
	len = Count();
	if(!len)
		return FALSE;

	source.WriteRecordL(&source_binary);

	for(i= 0; i< len; i++)
	{
		target = Item(i);

		if(target)
		{
			target_binary.ResetRecord();
			target->WriteRecordL(&target_binary);

			if(target_binary == source_binary)
			{
				index = i;
				return TRUE;
			}
		}
	}
	
	return FALSE;
}

void DataStream_FlexibleSequence::CopyL(DataStream_FlexibleSequence &source)
{
	uint32 len;

	len = source.Count();
	ResizeL(len);

	TransferL(0,source, 0, len);
}

void DataStream_FlexibleSequence::TransferL(uint32 start, DataStream_FlexibleSequence &source,uint32 start1, uint32 trans_len)
{
	DataStream_ByteArray source_binary;
	ANCHOR(DataStream_ByteArray, source_binary);
	DataStream *source_item;
	DataStream *target_item;
	uint32 len, len_s;

	len = Count();
	if(start >= len)
		return;

	len_s = source.Count();
	if(start1 >= len_s)
		return;

	if(start1 + trans_len > len_s)
		trans_len = len_s - start1;


	for(;trans_len >0 ; trans_len --, start++, start1++)
	{
		source_item = source.Item(start1);
		source_binary.ResetRecord();
		if(source_item)
		{
			source_item->WriteRecordL(&source_binary);
		}

		target_item = Item(start);
		if(target_item)
		{
			target_item->ResetRecord();
			source_binary.ResetRead();
			target_item->ReadRecordFromStreamL(&source_binary);
		}
	}
}

void DataStream_FlexibleSequence::CopyCommonL(DataStream_FlexibleSequence &ordered_list, DataStream_FlexibleSequence &master_list, BOOL reverse)
{
	DataStream_TypedSequence<DataStream_ByteArray> master;
	ANCHOR(DataStream_FlexibleSequence, master);
	DataStream_ByteArray source_binary;
	ANCHOR(DataStream_ByteArray, source_binary);
	DataStream *source_item;
	DataStream *target_item;
	uint32 i,j, k,count, master_count;
	
	count = ordered_list.Count();
	master_count = master_list.Count();
	if(master_count == 0)
		count = 0;

	ResizeL(count);

	if(count == 0 )
		return;

	master.ResizeL(master_count);

	for(i=0;i<master_count;i++)
	{
		source_item = master_list.Item(i);
		master[i].ResetRecord();
		if(source_item)
		{
			source_item->WriteRecordL(&master[i]);
		}
	}

	for(i=j=0;i<count;i++)
	{
		source_item = ordered_list.Item(reverse ? count-1 -i : i);
		source_binary.ResetRecord();
		if(source_item)
		{
			source_item->WriteRecordL(&source_binary);
		}

		for(k=0;k<master_count;k++)
		{
			if(master[k] == source_binary)
			{
				target_item = Item(j++);
				if(target_item)
				{
					target_item->ResetRecord();
					source_binary.ResetRead();
					target_item->ReadRecordFromStreamL(&source_binary);
				}
			}
		}
	}
	ResizeL(j);
}

#endif // DATASTREAM_USE_FLEXIBLE_SEQUENCE
