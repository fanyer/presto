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

void DataStream_BaseRecord::IndexRecordsL()
{
	ResetRead();
	payload_records_index.Clear();

	while(MoreData())
	{
		DataStream_BaseRecord *next_record = (DataStream_BaseRecord *) GetNextRecordL();

		if(next_record == NULL || !next_record->Finished())
		{
			// OOM safe: next_record is owned by this object, and is only released when it is finished
			break;
		}

		next_record->Into(&payload_records_index);
	}
}

DataStream_BaseRecord *DataStream_BaseRecord::GetIndexedRecord(uint32 _tag)
{
	DataStream_BaseRecord *current_record;

	current_record = (DataStream_BaseRecord *) payload_records_index.First();
	while(current_record && current_record->GetTag() != _tag)
		current_record = (DataStream_BaseRecord *) current_record->Suc();

	return current_record;
}

DataStream_BaseRecord *DataStream_BaseRecord::GetIndexedRecord(DataStream_BaseRecord *p_rec, uint32 _tag)
{
	DataStream_BaseRecord *current_record;

	current_record = (DataStream_BaseRecord *) payload_records_index.First();
	while(current_record && current_record != p_rec)
		current_record = (DataStream_BaseRecord *) current_record->Suc();

	if(current_record)
		current_record = (DataStream_BaseRecord *) current_record->Suc();

	while(current_record && current_record->GetTag() != _tag)
		current_record = (DataStream_BaseRecord *) current_record->Suc();

	return current_record;
}

OP_STATUS DataStream_BaseRecord::ReadRecordFromIndexedRecordL(DataStream *target, uint32 tag, BOOL content_only)
{
	if(target == NULL)
		DS_LEAVE(OpRecStatus::ERR_NULL_POINTER);

	DataStream_BaseRecord *rec1 = GetIndexedRecord(tag);
	if(rec1 == NULL)
		return OpRecStatus::RECORD_NOT_FOUND;

	if(content_only)
	{
		target->AddContentL(rec1);
		return OpRecStatus::FINISHED;
	}

	return target->ReadRecordFromStreamL(rec1);
}

DataStream_BaseRecord::DataStream_BaseRecord()
:	read_tag_field(FALSE),read_length_field(FALSE),
	default_tag(4), default_length(4),
	payload_source(NULL),
	tag(&default_tag),
	length(&default_length)
{
	default_tag.SetItemID(DataStream_BaseRecord::RECORD_TAG);
	default_length.SetItemID(DataStream_BaseRecord::RECORD_LENGTH);
	tag.Into(this);
	length.Into(this);
}

const DataRecord_Spec *DataStream_BaseRecord::GetRecordSpec()
{
	return &spec;
}	

#ifdef DATASTREAM_USE_SWAP_TAG_LEN_FUNC
void DataStream_BaseRecord::SwapTagAndLength()
{
	OP_ASSERT(tag.InList());
	OP_ASSERT(length.InList());

	if(tag.Suc() == &length)
	{
		length.Out();
		length.Precede(&tag);
	}
	else if(tag.Pred() == &length)
	{
		length.Out();
		length.Follow(&tag);
	}
	else
	{
		DataStream *len_suc = length.Suc();
		length.Out();
		length.Follow(&tag);
		tag.Out();
		if(len_suc)
			tag.Precede(len_suc);
		else
			tag.Into(this);
	}
}
#endif

#ifdef DATASTREAM_USE_ADV_TAG_LEN_FUNC
void DataStream_BaseRecord::RegisterTag(DataStream_UIntBase *app_tag)
{
	if(app_tag)
		app_tag->SetItemID(DataStream_BaseRecord::RECORD_TAG);
	tag.SetReference(app_tag ? app_tag : &default_tag);
}

void DataStream_BaseRecord::RegisterLength(DataStream_UIntBase *app_length)
{
	if(app_length)
		app_length->SetItemID(DataStream_BaseRecord::RECORD_LENGTH);
	length.SetReference(app_length ? app_length : &default_length);
}
#endif // DATASTREAM_USE_ADV_TAG_LEN_FUNC

void DataStream_BaseRecord::SetRecordSpec(const DataRecord_Spec &new_spec)
{
	spec = new_spec;

	default_tag.SetSize(spec.idtag_len);
	default_tag.SetBigEndian(spec.tag_big_endian);
	default_tag.SetMSBDetection(spec.tag_MSB_detection);

	default_length.SetSize(spec.length_len);
	default_length.SetBigEndian(spec.length_big_endian);

	tag.SetEnableRecord(spec.enable_tag);
	length.SetEnableRecord(spec.enable_length);
}

/*
DataStream *DataStream_BaseRecord::CreateRecordL()
{
	return NULL;
}
*/

void DataStream_BaseRecord::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	switch(action)
	{
	case DataStream::KSetTag:
		tag->SetValue(arg1);
		break;
	case DataStream::KWriteAction:
	case DataStream::KReadAction:
		{
			uint32 step = arg1;
			int record_item_id = (int) arg2;
			
			if(record_item_id == DataStream_SequenceBase::STRUCTURE_START)
			{
				const DataRecord_Spec *spec = GetRecordSpec();
				
				tag.SetEnableRecord(TRUE);
				//tag.SetItemID(DataStream_BaseRecord::RECORD_TAG);
				length.SetEnableRecord(TRUE);
				//length.SetItemID(DataStream_BaseRecord::RECORD_LENGTH);
				if(action == DataStream::KReadAction)
				{
					read_tag_field = FALSE;
					read_length_field= FALSE;
				}
				else // if(action == DataStream::KWriteAction)
				{
					length->SetValue(CalculatePayloadLength());
					
					DS_DEBUG_Start();
#ifdef _DATASTREAM_DEBUG_
					if(tag.GetEnabledRecord())
						DS_Debug_Printf2("Initiating write record Tag %d (%x)\n", tag->GetValue(), tag->GetValue());
					if(length.GetEnabledRecord())
						DS_Debug_Printf2("Initiating write record Length %d (%x)\n", length->GetValue(), length->GetValue());
#endif
				}
				
				if(spec)
				{
					tag.SetEnableRecord(spec->enable_tag);
					if(spec->enable_tag)
					{
						default_tag.SetSize(spec->idtag_len);
						default_tag.SetBigEndian(spec->tag_big_endian);
						default_tag.SetMSBDetection(spec->tag_MSB_detection);
					}
					
					length.SetEnableRecord(spec->enable_length);
					if(spec->enable_length)
					{
						default_length.SetSize(spec->length_len);
						default_length.SetBigEndian(spec->length_big_endian);
					}
					
					if(action == DataStream::KReadAction)
					{
						read_tag_field = !spec->enable_tag;
						read_length_field= !spec->enable_length;
					}
					else if(spec->tag_MSB_detection && (tag->GetValue() & MSB_VALUE) != 0)
					{
						length.SetEnableRecord(FALSE);
					}
				}
			}

			DataStream_SequenceBase::PerformActionL(action, step, record_item_id);
			switch(record_item_id)
			{
			case DataStream_SequenceBase::STRUCTURE_START:
				if(action == DataStream::KWriteAction)
				{
					length->SetValue(CalculatePayloadLength());
					
					DS_DEBUG_Start();
#ifdef _DATASTREAM_DEBUG_
					if(tag.GetEnabledRecord())
						DS_Debug_Printf2("Initiating write record Tag %d (%x)\n", tag->GetValue(), tag->GetValue());
					if(length.GetEnabledRecord())
						DS_Debug_Printf2("Initiating write record Length %d (%x)\n", length->GetValue(), length->GetValue());
#endif
				}
				break;

			case DataStream_BaseRecord::RECORD_TAG:
				if(action == DataStream::KReadAction && tag.GetEnabledRecord())
				{
					DS_DEBUG_Start();
					DS_Debug_Printf2("Read Tag %d (%x)\n", tag->GetValue(), tag->GetValue());
					
					read_tag_field = TRUE;
					
					if(spec.tag_MSB_detection && (tag->GetValue() & MSB_VALUE) != 0)
					{
						length.SetEnableRecord(FALSE);
						read_length_field = TRUE;
						payload_source.SetReadLength(0);
					}
					
				}
				break;
			case DataStream_BaseRecord::RECORD_LENGTH:
				if(action == DataStream::KReadAction && length.GetEnabledRecord())
				{
					DS_DEBUG_Start();
					DS_Debug_Printf2("Read Length %d (%x)\n", length->GetValue(), length->GetValue());
					
					read_length_field = TRUE;
					payload_source.SetReadLength(length->GetValue());
				}
				break;
			}
		}
		return;
	}
	DataStream_SequenceBase::PerformActionL(action, arg1, arg2);
}

void DataStream_BaseRecord::SetTag(uint32 tag_number)
{
	tag->SetValue(tag_number);
}

DataStream *DataStream_BaseRecord::GetInputSource(DataStream *src)
{
	if(read_length_field)
	{
		payload_source.SetStreamSource(DataStream_SequenceBase::GetInputSource(src), FALSE);
		return &payload_source;
	}

	return DataStream_SequenceBase::GetInputSource(src);
}

uint32 DataStream_BaseRecord::CalculatePayloadLength()
{
	uint32 len = 0;

	OP_ASSERT(length.InList());

	DataStream *item = length.Suc();

	while(item)
	{
		if(item->GetEnabledRecord())
			len += item->CalculateLength();
		item = item->Suc();
	}

	return len;
}

uint32	DataStream_BaseRecord::GetAttribute(DataStream::DatastreamUIntAttribute attr)
{
	if(attr == DataStream::KFinished)
		return (DataStream_SequenceBase::GetAttribute(attr) && !payload_source.GetAttribute(DataStream::KMoreData));
	
	return DataStream_SequenceBase::GetAttribute(attr);
}

#if defined _SSL_SUPPORT_
OP_STATUS DataStream_BaseRecord::PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target)
{
	switch(action)
	{
	case DataStream::KReadRecordHeader:
		{
			DataStream *item = GetLastHeaderElement();

			return (item ? ReadRecordSequenceFromStreamL(src_target, item) : OpRecStatus::FINISHED);
		}
	case DataStream::KWriteRecordHeader:
	case DataStream::KWriteRecordPayload:
		{
			DataStream *last_item = GetLastHeaderElement();
			DataStream *first_item = NULL;

			if(action == DataStream::KWriteRecordPayload)
			{
				first_item = last_item;
				last_item = NULL;
			}
			
			if(action == DataStream::KWriteRecordPayload || last_item) // header write only of there is a header => Last_item != NULL, always write for payload
				WriteRecordSequenceL(src_target, TRUE, first_item, last_item);
			return OpRecStatus::FINISHED;
		}
	}

	return DataStream_SequenceBase::PerformStreamActionL(action, src_target);

}
#endif

DataStream *DataStream_BaseRecord::GetLastHeaderElement()
{
	DataStream *item = (tag.InList() ? &tag : NULL);

	if(!item || (length.InList() && !length.Precedes(item)))
		item = (length.InList() ? &length : NULL);

	return item;
}

#endif
