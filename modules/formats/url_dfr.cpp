/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/



#include "core/pch.h"

#ifdef FORMATS_DATAFILE_ENABLED
#include "modules/formats/url_dfr.h"

DataFile::DataFile(OpFileDescriptor *file_p)
 : DataStream_GenericFile(file_p, FALSE)
{
	file_version = 0;
	app_version = 0;
}

DataFile::DataFile(OpFileDescriptor *file_p,
				   uint32 app_ver, uint16 taglen, uint16 lenlen)
 : DataStream_GenericFile(file_p, TRUE)
{
	file_version = DATA_FILE_CURRENT_VERSION;
	app_version = app_ver;
	spec.idtag_len = taglen;
	spec.tag_big_endian = TRUE;
	spec.tag_MSB_detection = TRUE;
	spec.length_len = lenlen;
	spec.length_big_endian = TRUE;
}

DataFile::~DataFile()
{
}

void DataFile::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	switch(action)
	{
	case DataStream::KInternal_InitRead:
		{
			OP_STATUS op_err;
			uint32 temp_int;

			op_err = ReadIntegerL(this,file_version, 4, TRUE, FALSE);
			if(op_err == OpRecStatus::FINISHED)
			{
				op_err = ReadIntegerL(this,app_version, 4, TRUE, FALSE);
				if(op_err == OpRecStatus::FINISHED)
				{
					op_err = ReadIntegerL(this, temp_int, 2, TRUE, FALSE);
					if(op_err == OpRecStatus::FINISHED)
					{
						spec.idtag_len = (uint16) temp_int;
						op_err = ReadIntegerL(this, temp_int , 2, TRUE, FALSE);
						if(op_err == OpRecStatus::FINISHED)
						{
							spec.length_len = (uint16) temp_int;
							if (!spec.idtag_len && !spec.length_len)
								op_err = OpRecStatus::ILLEGAL_FORMAT;
						}
					}
				}
			}
			if(op_err != OpRecStatus::FINISHED)
			{
				ErrorClose();
				if(!OpRecStatus::IsError(op_err))
					op_err = OpRecStatus::ERR;
				LEAVE(op_err);
			}
			break;
		}
	case DataStream::KInternal_InitWrite:
		WriteIntegerL(file_version, 4, TRUE, FALSE, this);
		WriteIntegerL(app_version, 4, TRUE, FALSE, this);
		WriteIntegerL(spec.idtag_len, 2, TRUE, FALSE, this);
		WriteIntegerL(spec.length_len, 2, TRUE, FALSE, this);
		break;
	default: 
		DataStream_GenericFile::PerformActionL(action, arg1, arg2);
	}
}

const DataRecord_Spec *DataFile::GetRecordSpec()
{
	return &spec;
}

DataStream *DataFile::CreateRecordL()
{
	DataFile_Record *rec = OP_NEW_L(DataFile_Record, ());

	rec->SetRecordSpec(GetRecordSpec());

	return rec;
}

DataFile_Record::DataFile_Record()
{
}

DataFile_Record::DataFile_Record(uint32 _tag)
{
	SetTag(_tag);
}

DataFile_Record::~DataFile_Record()
{
}

DataStream *DataFile_Record::CreateRecordL()
{
	DataFile_Record *rec = OP_NEW_L(DataFile_Record, ());

	rec->SetRecordSpec(GetRecordSpec());

	return rec;
}

void DataFile_Record::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	DataStream_GenericRecord_Large::PerformActionL(action, arg1, arg2);

	if(action == DataStream::KSetTag && (arg1 & MSB_VALUE) != 0)
		UsePayload(FALSE);
}

/*
void DataFile_Record::AddContentL(const char *src)
{
	DataStream_GenericRecord_Large::AddContentL(src);
}

void DataFile_Record::AddContentL(OpStringC8 &src)
{
	DataStream_GenericRecord_Large::AddContentL(src);
}

void DataFile_Record::AddContentL(uint8 src)
{
	AddContentL(&src, 1);
}

void DataFile_Record::AddContentL(uint16 src)
{
	WriteIntegerL(src, 2, spec.numbers_big_endian, FALSE, this);
}

void DataFile_Record::AddContentL(uint32 src)
{
	WriteIntegerL(src, 4, spec.numbers_big_endian, FALSE, this);
}

char *DataFile_Record::GetStringL()
{
	return DataStream_GenericRecord_Large::GetStringL();
}

void DataFile_Record::GetStringL(OpStringS8 &string)
{
	DataStream_GenericRecord_Large::GetStringL(string);
}
*/

#if 0
uint8 DataFile_Record::GetUInt8L()
{
	uint8 buf=0;

	ReadDataL((byte *) &buf, 1);
	return buf;
}

uint16 DataFile_Record::GetUInt16L()
{
	uint32 buf=0;

	ReadIntegerL(this,buf, 2, spec.numbers_big_endian, FALSE);
	return (uint16) buf;
}
#endif

/*
uint32 DataFile_Record::GetUInt32L(BOOL handle_oversized)
{
	uint32 buf=0;

	ReadIntegerL(this,buf, (handle_oversized && spec.numbers_big_endian ? GetLength() : 4), spec.numbers_big_endian, FALSE);
	return (uint32) buf;
}
*/

#if 0
int8 DataFile_Record::GetInt8L()
{
	uint8 buf=0;

	ReadDataL((byte *) &buf, 1);
	return (int8) buf;
}

int16 DataFile_Record::GetInt16L()
{
	uint32 buf=0;

	ReadIntegerL(this,buf, 2, spec.numbers_big_endian, FALSE);
	return (int16) buf;
}
#endif

int32 DataFile_Record::GetInt32L()
{
	uint32 buf=0;

	ReadIntegerL(this,buf, 4, spec.numbers_big_endian, FALSE);
	return (int32) buf;
}

#if 0
char *DataFile_Record::GetIndexedRecordStringL(uint32 _tag)
{
	DataFile_Record *rec = GetIndexedRecord(_tag);

	if(!rec)
		return NULL;

	rec->ResetRead();

	return rec->GetStringL();
}
#endif

void DataFile_Record::GetIndexedRecordStringL(uint32 _tag, OpStringS8 &string)
{
	string.Empty();

	DataFile_Record *rec = GetIndexedRecord(_tag);

	if(!rec)
		return;

	rec->ResetRead();
	rec->GetStringL(string);
}

BOOL DataFile_Record::GetIndexedRecordBOOL(uint32 _tag)
{
	DataFile_Record *rec = GetIndexedRecord(_tag);

	return (rec ? TRUE : FALSE);
}

unsigned long DataFile_Record::GetIndexedRecordUIntL(uint32 _tag)
{
	DataFile_Record *rec = GetIndexedRecord(_tag);

	if(!rec)
		return 0;

	rec->ResetRead();

	return rec->GetUInt32L(TRUE);
}

OpFileLength DataFile_Record::GetIndexedRecordUInt64L(uint32 _tag)
{
	DataFile_Record *rec = GetIndexedRecord(_tag);

	if(!rec)
		return 0;

	rec->ResetRead();

	return rec->GetUInt64L(TRUE);
}

long DataFile_Record::GetIndexedRecordIntL(uint32 _tag)
{
	DataFile_Record *rec = GetIndexedRecord(_tag);

	if(!rec)
		return 0;

	rec->ResetRead();

	return rec->GetInt32L();
}

void DataFile_Record::AddRecordL(uint32 tag, const OpStringC8 &src)
{
#if 0
	DataFile_Record temp_record(tag);
	ANCHOR(DataFile_Record, temp_record);

	temp_record.SetRecordSpec(GetRecordSpec());
	temp_record.AddContentL(src);

	temp_record.WriteRecordL(this);
#else
	if(spec.enable_tag)
		WriteIntegerL(tag, spec.idtag_len, spec.tag_big_endian, spec.tag_MSB_detection, this);
	uint32 len = src.Length();
	if(spec.enable_length)
		WriteIntegerL(len, spec.length_len, spec.length_big_endian, FALSE, this);
	if(len)
		AddContentL((const unsigned char *) src.CStr(), len);
#endif
}

void DataFile_Record::AddRecordL(uint32 tag, uint32 value)
{
#if 0
	DataFile_Record temp_record(tag);
	ANCHOR(DataFile_Record, temp_record);

	temp_record.SetRecordSpec(GetRecordSpec());
	temp_record.AddContentL(value);

	temp_record.WriteRecordL(this);
#else
	if(spec.enable_tag)
		WriteIntegerL(tag, spec.idtag_len, spec.tag_big_endian, spec.tag_MSB_detection, this);
	if(spec.enable_length)
		WriteIntegerL(4, spec.length_len, spec.length_big_endian, FALSE, this);
	WriteIntegerL(value, 4, spec.numbers_big_endian, FALSE, this);
#endif
}

void DataFile_Record::AddRecord64L(uint32 tag, OpFileLength value)
{
	if(spec.enable_tag)
		WriteIntegerL(tag, spec.idtag_len, spec.tag_big_endian, spec.tag_MSB_detection, this);
	if(spec.enable_length)
		WriteIntegerL(8, spec.length_len, spec.length_big_endian, FALSE, this);
	WriteInteger64L(value, 8, spec.numbers_big_endian, FALSE, this);
}

/*
OP_STATUS DataFile_Record::AddRecordL(uint32 tag, uint16 value)
{
	DataFile_Record temp_record(tag);
	ANCHOR(DataFile_Record, temp_record);

	temp_record.SetRecordSpec(GetRecordSpec());
	temp_record.AddContentL(value);

	return AddContentL(&temp_record);
}

OP_STATUS DataFile_Record::AddRecordL(uint32 tag, uint8 value)
{
	DataFile_Record temp_record(tag);
	ANCHOR(DataFile_Record, temp_record);

	temp_record.SetRecordSpec(GetRecordSpec());
	temp_record.AddContentL(value);

	return AddContentL(&temp_record);
}
*/

void DataFile_Record::AddRecordL(uint32 tag)
{
#if 0
	DataFile_Record temp_record(tag);
	ANCHOR(DataFile_Record, temp_record);

	temp_record.SetRecordSpec(GetRecordSpec());

	temp_record.WriteRecordL(this);
#else
	if(spec.enable_tag)
		WriteIntegerL(tag, spec.idtag_len, spec.tag_big_endian, spec.tag_MSB_detection, this);
	if((!spec.tag_MSB_detection && spec.enable_length) ||
		(spec.tag_MSB_detection && (tag & MSB_VALUE) == 0) )
		WriteIntegerL(0, spec.length_len, spec.length_big_endian, FALSE, this);
#endif
}

#ifdef _DATASTREAM_DEBUG_
const char *DataFile::Debug_ClassName()
{
	return "DataFile";
}

const char *DataFile_Record::Debug_ClassName()
{
	return "DataFile_Record";
}
#endif

#endif // FORMATS_DATAFILE_ENABLED
