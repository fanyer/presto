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

#ifdef DATASTREAM_BASE_ENABLED

#include "modules/hardcore/mem/mem_man.h"

#include "modules/datastream/fl_lib.h"

DataStream::DataStream()
: 
#ifdef DATASTREAM_READ_RECORDS
	current_loading_record(NULL), 
#endif
	item_id(DataStream::GENERIC_RECORD),
	enabled_record(TRUE)
{
}

DataStream::~DataStream()
{
#ifdef DATASTREAM_READ_RECORDS
	if(current_loading_record)
	{
		current_loading_record->FinishedAddingL(); // Abort loading;
		OP_DELETE(current_loading_record);
	}

	current_loading_record = NULL; // Deleted by owner
#endif

	if(InList())
		Out();
}

#ifdef DATASTREAM_READ_RECORDS
const DataRecord_Spec *DataStream::GetRecordSpec()
{
	return NULL;
}


void DataStream::ReleaseRecord(DataStream *rec, BOOL finished)
{
	if(rec && current_loading_record == rec)
	{
		if(!finished)
			rec->FinishedAddingL();
		current_loading_record = NULL;
	}
}

DataStream *DataStream::GetNextRecordL()
{
	DS_DEBUG_Start();
	DataStream *temp_record;

	DS_Debug_Printf0("DataStream::GetNextRecordL: step 1\n");
	ReadNextRecordL();

	DS_Debug_Printf0("DataStream::GetNextRecordL: step 2\n");

	temp_record = current_loading_record;

	if(current_loading_record && current_loading_record->Finished())
	{
		ReleaseRecord(current_loading_record, TRUE);
	}


	return temp_record;
}

#ifdef DATASTREAM_SAMPLE_NEXT_RECORD
DataStream *DataStream::SampleNextRecordL()
{
	DS_DEBUG_Start();
	DS_Debug_Printf0("DataStream::SampleNextRecordL: step 1\n");
	ReadNextRecordL();
	DS_Debug_Printf0("DataStream::SampleNextRecordL: step 2\n");

	return current_loading_record;
}
#endif

void DataStream::ReadNextRecordL()
{
	DS_DEBUG_Start();
	DS_Debug_Printf0("DataStream::ReadNextRecordL: step 1\n");
	if(current_loading_record && current_loading_record->Finished())
	{
		DS_Debug_Printf0("DataStream::ReadNextRecordL: return 1\n");
		return;
	}

	if(MoreData())
	{
		if(current_loading_record == NULL)
		{
			current_loading_record = CreateRecordL();
			if(current_loading_record == NULL)
				return;
		}

		DS_Debug_Printf0("DataStream::ReadNextRecordL: step 2\n");
		// The status of current_loadint_record is manually checked later. Therefore we can safely ignore OpStatus.
		OpStatus::Ignore(current_loading_record->ReadRecordFromStreamL(this)); 
		DS_Debug_Printf0("DataStream::ReadNextRecordL: step 3\n");

		if(current_loading_record->Finished())
		{
			DS_Debug_Printf0("DataStream::ReadNextRecordL: return 2\n");
			return;
		}
	}

	if(!MoreData() && !Active() && current_loading_record)
	{
		DS_Debug_Printf0("DataStream::ReadNextRecordL: step 4\n");
		DataStream *temp = current_loading_record;
		ReleaseRecord(current_loading_record);
		OP_DELETE(temp);
	}
	DS_Debug_Printf0("DataStream::ReadNextRecordL: return 3\n");

}

void DataStream::WriteIntegerL(uint32 value, uint32 len, BOOL big_endian, BOOL MSB_detection,  DataStream *target)
{
	DS_DEBUG_Start();

	if(target == NULL)
		return;

	DS_Debug_Printf4("DataStream::WriteIntegerL Write integer %u (%u bytes, %s %s)\n", value, len, (big_endian? "BigEndian" : "LittleEndian"), (MSB_detection ? "With MSBDEtection" : ""));
	unsigned char *tempbuf = (unsigned char *) g_memory_manager->GetTempBuf2()+1; // creating space in front and after  (boundschecker range)
	uint32 tlen = g_memory_manager->GetTempBuf2Len()-2; // creating space in front and after  (boundschecker range)

	if(tlen < len)
		DS_LEAVE(OpRecStatus::ERR);

	BOOL MSB_set = FALSE;

	if(MSB_detection)
	{
		MSB_set = ((value & MSB_VALUE) != 0 ? TRUE : FALSE);
		value &= ~MSB_VALUE;
	}

	//memset(tempbuf, 0, len);
	unsigned char *LSB_pos = tempbuf; 
	uint32 i;

	for(i=len;i>0;i--)
		*(LSB_pos++) = 0;

	LSB_pos = tempbuf + (big_endian ? len -1 : 0); 
	
	if(big_endian)
	{
		while(value > 0)
		{
			(*LSB_pos--) = (unsigned char) (value & 0xff);
			value >>= 8;
		}
	}
	else
	{
		while(value > 0)
		{
			*(LSB_pos++) = (unsigned char) (value & 0xff);
			value >>= 8;
		}
	}

	if(MSB_set)
		tempbuf[big_endian ? 0 : len -1] |= 0x80;

	target->WriteDataL(tempbuf, len);
}

void DataStream::WriteInteger64L(OpFileLength value, uint32 len, BOOL big_endian, BOOL MSB_detection,  DataStream *target)
{
	DS_DEBUG_Start();

	if(target == NULL)
		return;

	DS_Debug_Printf5("DataStream::WriteIntegerL Write integer %x:%x (%u bytes, %s %s)\n", (uint32) ((value>>32) &0xffffffff), (uint32) (value &0xffffffff), len, (big_endian? "BigEndian" : "LittleEndian"), (MSB_detection ? "With MSBDEtection" : ""));
	unsigned char *tempbuf = (unsigned char *) g_memory_manager->GetTempBuf2()+1; // creating space in front and after  (boundschecker range)
	uint32 tlen = g_memory_manager->GetTempBuf2Len()-2; // creating space in front and after  (boundschecker range)

	if(tlen < len)
		DS_LEAVE(OpRecStatus::ERR);

	BOOL MSB_set = FALSE;

	if(MSB_detection)
	{
		MSB_set = ((value & MSB_VALUE) != 0 ? TRUE : FALSE);
		value &= ~MSB_VALUE;
	}

	//memset(tempbuf, 0, len);
	unsigned char *LSB_pos = tempbuf; 
	uint32 i;

	for(i=len;i>0;i--)
		*(LSB_pos++) = 0;

	LSB_pos = tempbuf + (big_endian ? len -1 : 0); 
	
	if(big_endian)
	{
		while(value > 0)
		{
			(*LSB_pos--) = (unsigned char) (value & 0xff);
			value >>= 8;
		}
	}
	else
	{
		while(value > 0)
		{
			*(LSB_pos++) = (unsigned char) (value & 0xff);
			value >>= 8;
		}
	}

	if(MSB_set)
		tempbuf[big_endian ? 0 : len -1] |= 0x80;

	target->WriteDataL(tempbuf, len);
}

#if defined _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_
void DataStream::WriteIntegerL(uint64 value, uint32 len, BOOL big_endian, DataStream *target)
{
	DS_DEBUG_Start();

	if(target == NULL)
		return;

	DS_Debug_Printf3("DataStream::WriteIntegerL Write integer %u (%u bytes, %s)\n", (unsigned int) (value & 0xffffffff), len, (big_endian? "BigEndian" : "LittleEndian"));
	unsigned char *tempbuf = (unsigned char *) g_memory_manager->GetTempBuf2()+1;// creating space in front and after (boundschecker range)
	uint32 tlen = g_memory_manager->GetTempBuf2Len()-2;// creating space in front and after (boundschecker range )

	if(tlen < len)
		DS_LEAVE(OpRecStatus::ERR);

	//memset(tempbuf, 0, len);
	unsigned char *LSB_pos = tempbuf;
	uint32 i;

	for(i=len;i>0;i--)
		*(LSB_pos++) = 0;

	LSB_pos = tempbuf + (big_endian ? len -1 : 0);
	uint64 zero;

	zero = 0;
	
	if(big_endian)
	{
		while(value != zero)
		{
			(*LSB_pos--) = (unsigned char) (value & 0xff);
			value = value >> 8;
		}
	}
	else
	{
		while(value != zero)
		{
			*(LSB_pos++) = (unsigned char) (value & 0xff);
			value = value >> 8;
		}
	}

	target->WriteDataL(tempbuf, len);
}
#endif // _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_

OP_STATUS DataStream::ReadIntegerL(DataStream *src, uint32 &value, uint32 len, BOOL big_endian, BOOL MSB_detection, BOOL sample_only)
{
	DS_DEBUG_Start();
	if(src == NULL)
		return OpRecStatus::OK;


	unsigned char *tempbuf = (unsigned char *) g_memory_manager->GetTempBuf2(); // creating space in front and after  (boundschecker range)
	uint32 tlen = g_memory_manager->GetTempBuf2Len(); // creating space in front and after  (boundschecker range)
	uint32 rlen=0;

	if(tlen < len)
		return OpRecStatus::ERR;

	rlen = src->ReadDataL(tempbuf, len, (sample_only ? DataStream::KSampleOnly : DataStream::KReadAndCommitOnComplete));

	if(rlen < len)
	{
		DS_Debug_Printf2("DataStream::ReadIntegerL not enough data. Need %u bytes, got %u\n", len, rlen);
		return (src->Active() ? OpRecStatus::OK : OpRecStatus::ERR);
	}

	unsigned char *MSB_pos = tempbuf + (big_endian ? 0 : len -1);

	BOOL MSB_set = FALSE;
	if(MSB_detection)
	{
		MSB_set = ((((*MSB_pos) &0x80) != 0) ?  TRUE : FALSE);
		(*MSB_pos) &= ~0x80;
	}

	value = 0;

	if(big_endian)
	{
		uint32 i;
		for(i = len; i>0; i--)
		{
			value <<= 8;
			value |= *(MSB_pos++);
		}
	}
	else
	{
		uint32 i;
		for(i = len; i>0; i--)
		{
			value <<= 8;
			value |= *(MSB_pos--);
		}
	}

	if(MSB_set)
		value |= MSB_VALUE;

	DS_Debug_Printf3("DataStream::ReadIntegerL Read integer %u/%x (%d bytes)\n", value, value, len);
	return OpRecStatus::FINISHED;

}

OP_STATUS DataStream::ReadInteger64L(DataStream *src, OpFileLength &value, uint32 len, BOOL big_endian, BOOL MSB_detection, BOOL sample_only)
{
	DS_DEBUG_Start();
	if(src == NULL)
		return OpRecStatus::OK;


	unsigned char *tempbuf = (unsigned char *) g_memory_manager->GetTempBuf2(); // creating space in front and after  (boundschecker range)
	uint32 tlen = g_memory_manager->GetTempBuf2Len(); // creating space in front and after  (boundschecker range)
	uint32 rlen=0;

	if(tlen < len)
		return OpRecStatus::ERR;

	rlen = src->ReadDataL(tempbuf, len, (sample_only ? DataStream::KSampleOnly : DataStream::KReadAndCommitOnComplete));

	if(rlen < len)
	{
		DS_Debug_Printf2("DataStream::ReadIntegerL not enough data. Need %u bytes, got %u\n", len, rlen);
		return (src->Active() ? OpRecStatus::OK : OpRecStatus::ERR);
	}

	unsigned char *MSB_pos = tempbuf + (big_endian ? 0 : len -1);

	BOOL MSB_set = FALSE;
	if(MSB_detection)
	{
		MSB_set = ((((*MSB_pos) &0x80) != 0) ?  TRUE : FALSE);
		(*MSB_pos) &= ~0x80;
	}

	value = 0;

	if(big_endian)
	{
		uint32 i;
		for(i = len; i>0; i--)
		{
			value <<= 8;
			value |= *(MSB_pos++);
		}
	}
	else
	{
		uint32 i;
		for(i = len; i>0; i--)
		{
			value <<= 8;
			value |= *(MSB_pos--);
		}
	}

	if(MSB_set)
		value |= MSB_VALUE;

	DS_Debug_Printf3("DataStream::ReadIntegerL Read integer %u/%x (%d bytes)\n", value, value, len);
	return OpRecStatus::FINISHED;

}
#endif // DATASTREAM_READ_RECORDS

uint32 DataStream::AddContentL(DataStream *src, uint32 len, uint32 buffer_len)
{
	DS_DEBUG_Start();
	uint32 r_len = 0;

	if(src == NULL) 
		return 0;

	DS_Debug_Printf0("DataStream::AddContentL reading from stream\n");
	unsigned char *buffer = (unsigned char *) g_memory_manager->GetTempBuf2k();
	unsigned long buf_len = g_memory_manager->GetTempBuf2kLen();

	unsigned char *allocated_buffer = NULL;

	if(buffer_len > buf_len*2)
	{
		allocated_buffer = OP_NEWA(unsigned char, buffer_len);
		if(allocated_buffer)
		{
			buffer = allocated_buffer;
			buf_len = buffer_len;
		}
	}
	ANCHOR_ARRAY(unsigned char, allocated_buffer);

	while(src->MoreData() && (len == 0 || r_len < len))
	{
		unsigned long got_len;
		
		got_len = src->ReadDataL(buffer, (len && len - r_len < buf_len ? len - r_len : buf_len));

		if(got_len == 0)
			break; // Just in case the stream is still unfinished and waiting for data 

		DS_Debug_Write("DataStream::AddContentL", "Writing to this stream", buffer, got_len);
		WriteDataL(buffer, got_len);
		r_len += got_len;
	}
	DS_Debug_Printf0("DataStream::AddContentL finished reading from stream\n");

	ANCHOR_ARRAY_DELETE(allocated_buffer);

	return r_len;
}

unsigned long DataStream::ReadDataL(unsigned char *buffer, unsigned long len, DataStream::DatastreamCommitPolicy sample)
{
	return 0;
}

void DataStream::WriteDataL(const unsigned char *buffer, unsigned long len)
{
}

#ifdef DATASTREAM_READ_RECORDS
OP_STATUS DataStream::PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target)
{
#if defined _SSL_SUPPORT_
	if(action == DataStream::KWriteRecordPayload)
		return PerformStreamActionL(DataStream::KWriteRecord,src_target);
#endif

	// no structure
	return OpRecStatus::FINISHED;
}
#endif

uint32	DataStream::GetAttribute(DataStream::DatastreamUIntAttribute attr)
{
	switch(attr)
	{
	case DataStream::KCalculateLength:
		return 0;
	case DataStream::KID:
		return item_id;
	case DataStream::KFinished:
		return TRUE;
	case DataStream::KActive:
	case DataStream::KMoreData:
	default:
		return FALSE;
	}
}

void DataStream::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	switch(action)
	{
	case DataStream::KReadAction:
#ifdef DATASTREAM_SEQUENCE_ENABLED
		if((int) arg2 == DataStream_SequenceBase::STRUCTURE_START)
		{
			PerformActionL(DataStream::KResetRecord);
		}
#endif
		break;
	case DataStream::KSetItemID:
		item_id = arg1;
		break;
	}
}


#ifdef DATASTREAM_READ_RECORDS
DataStream *DataStream::CreateRecordL()
{
	return NULL;
}
#endif // DATASTREAM_READ_RECORDS

#endif // DATASTREAM_BASE_ENABLED

