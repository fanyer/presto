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

#ifdef DATASTREAM_SEQUENCE_ENABLED

#include "modules/datastream/fl_lib.h"

DataStream_SequenceBase::DataStream_SequenceBase()
:	current_item (NULL), step(0)
{
}

DataStream_SequenceBase::~DataStream_SequenceBase()
{
	RemoveAll();
}

void DataStream_SequenceBase::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	switch(action)
	{
	case DataStream::KResetRead:
	case DataStream::KResetRecord:
		{
			step = 0;
			current_item = NULL;
			
			DataStream *item = First();
			
			while(item)
			{
				item->PerformActionL(action,arg1, arg2);
				item = item->Suc();
			}
		}
		break;
	case DataStream::KFinishedAdding:
		{
			DataStream *item = First();
			
			while(item)
			{
				if(item->GetEnabledRecord() && !item->Finished())
					item->PerformActionL(DataStream::KFinishedAdding);
				item = item->Suc();
			}
		}
		break;
	case  DataStream::KWriteAction:
		if(arg2 == (uint32) DataStream_SequenceBase::STRUCTURE_START)
		{
			DataStream *item = First();
			while(item)
			{
				item->PerformActionL(DataStream::KWriteAction, 0, (uint32) DataStream_SequenceBase::STRUCTURE_START);
				item = item->Suc();
			}
		}
		break;
	}
	DataStream::PerformActionL(action, arg1, arg2);
}

OP_STATUS DataStream_SequenceBase::PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target)
{
	switch(action)
	{
	case DataStream::KReadRecord:
		return ReadRecordSequenceFromStreamL(src_target, NULL);
	case DataStream::KWriteRecord:
		WriteRecordSequenceL(src_target, FALSE, NULL, NULL);
		return OpRecStatus::FINISHED;
	}

	return DataStream::PerformStreamActionL(action, src_target);
}

OP_STATUS DataStream_SequenceBase::ReadRecordSequenceFromStreamL(DataStream *src, DataStream *last_element)
{
	OP_STATUS op_err;

	DS_Debug_Printf1("DataStream_SequenceBase::ReadStructureRecordsFromStreamL: Step %u Starting to process structure\n", step);

	if(current_item == NULL)
	{
		step = 0;
		ReadActionL(step++, DataStream_SequenceBase::STRUCTURE_START);
		current_item = First();
	}
	
	while(current_item)
	{
		if(current_item->GetEnabledRecord())
		{
			op_err = current_item->ReadRecordFromStreamL(GetInputSource(src));
			if(op_err == OpRecStatus::OK || op_err == OpRecStatus::WAIT_FOR_DATA)
				return op_err;
			else if(op_err != OpRecStatus::FINISHED)
			{
					DS_LEAVE(OpRecStatus::IsError(op_err) ? op_err : OpRecStatus::RECORD_TOO_SHORT);
			}
			DS_Debug_Printf1("DataStream_SequenceBase::ReadStructureRecordsFromStreamL:  Step %u read item\n", step);
		}
		else
		{
			DS_Debug_Printf1("DataStream_SequenceBase::ReadStructureRecordsFromStreamL:  Step %u bypassed disabled entry\n", step);
		}

		ReadActionL(step++, current_item->GetItemID());

		BOOL is_last_element = (current_item == last_element ? TRUE : FALSE);

		current_item = current_item->Suc();

		if(current_item && is_last_element)
			return OpRecStatus::FINISHED;
	}
	ReadActionL(step, DataStream_SequenceBase::STRUCTURE_FINISHED);

	return OpRecStatus::FINISHED;
}

/*
uint32 DataStream_SequenceBase::GetLength()
{
	return GetAttribute(DataStream::KCalculateLength);
}
*/

void DataStream_SequenceBase::WriteRecordSequenceL(DataStream *target, BOOL start_after, DataStream *start_with_this, DataStream *stop_after_this)
{
	DS_DEBUG_Start();
	OP_ASSERT(target != this);

	int stepw = 0;
	DataStream *item = First();

	DS_Debug_Printf0("------------ Starting to Write Record ------------\n");
	if(start_with_this)
	{
		// Fast Forward
		while(item)
		{
			if(item == start_with_this && !start_after)
				break;

			if(item == stop_after_this)
				return;

			item = item->Suc();
			stepw++;

			if(item && start_with_this == item->Pred())
				break;
		}
	}

	WriteActionL(stepw++, (item && item->Pred() ? item->Pred()->GetItemID() : (uint32) DataStream_SequenceBase::STRUCTURE_START));
	while(item)
	{
		if(item->GetEnabledRecord())
			item->WriteRecordL(GetOutputTarget(target));

		WriteActionL(stepw++, item->GetItemID());

		BOOL is_last_element = (item == stop_after_this ? TRUE : FALSE);

		item = item->Suc();

		if(item && is_last_element)
		{
			DS_Debug_Printf0("------------ Finished Writing Record (1) ------------\n");
			return;
		}
	}
	WriteActionL(stepw++, DataStream_SequenceBase::STRUCTURE_FINISHED);

	DS_Debug_Printf0("------------ Finished Writing Record ------------\n");
}

uint32	DataStream_SequenceBase::GetAttribute(DataStream::DatastreamUIntAttribute attr)
{
	switch(attr)
	{
	case DataStream::KFinished:
		return !current_item; // Also returns finsihed on non-started items
	case DataStream::KCalculateLength:
		{
			uint32 len = 0;
			
			DataStream *item = First();
			while(item)
			{
				if(item->GetEnabledRecord())
					len += item->CalculateLength();
				item = item->Suc();
			}
		
			return len;
		}
	}
	
	return DataStream::GetAttribute(attr);
}

DataStream *DataStream_SequenceBase::GetInputSource(DataStream *src)
{
	return src;
}

DataStream *DataStream_SequenceBase::GetOutputTarget(DataStream *trgt)
{
	return trgt;
}


#ifdef _DATASTREAM_DEBUG_
void DataStream_SequenceBase::Debug_Write_All_Data()
{
	DS_DEBUG_Start();
	DS_Debug_Printf0("DataStream_BaseRecord::Debug_Write_All_Data Starting dump\n");
	DataStream *item = First();

	while(item)
	{
		DS_Debug_Printf0("--------- Start item -------\n");
		item->Debug_Write_All_Data();
		DS_Debug_Printf0("--------- End item   -------\n");
		item = item->Suc();
	}
	DS_Debug_Printf0("DataStream_BaseRecord::Debug_Write_All_Data Ending dump\n");
}
#endif

#endif // DATASTREAM_SEQUENCE_ENABLED
