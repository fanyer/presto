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

#ifdef DATASTREAM_GENERIC_FILE_ENABLED

#include "modules/hardcore/mem/mem_man.h"

#include "modules/datastream/fl_lib.h"
#include "modules/util/opfile/opsafefile.h"

DataStream_GenericFile::DataStream_GenericFile(
								 OpFileDescriptor *file_p,
								 BOOL _write,
								 BOOL _take_over)
 : file(file_p)
{
	scan_buffer.SetIsFIFO();

	writing = _write;
	take_over = _take_over;
	BOOL existed_1 = FALSE;
	if (file)
	{
		OpStatus::Ignore(file->Exists(existed_1));
	}
	existed = existed_1;

	//DS_Debug_Printf1("DataStream_GenericFile::DataStream_GenericFile Opening file %s\n", (file ? make_singlebyte_in_tempbuffer(file->GetFullPath(), uni_strlen(file->GetFullPath())) : "(no file)"));
}

DataStream_GenericFile::~DataStream_GenericFile()
{
	Close();
}

void DataStream_GenericFile::Close()
{
	PerformActionL(writing ? DataStream::KInternal_FinalWrite: DataStream::KInternal_FinalRead);

	//ClearBuffer();

	if(file)
	{
		//DS_Debug_Printf1("DataStream_GenericFile::Close Closing file %s\n", make_singlebyte_in_tempbuffer(file->GetFullPath(), uni_strlen(file->GetFullPath())));
		if(file->Type() == OPSAFEFILE)
		{
			OpSafeFile *safe_file = (OpSafeFile *) file;

			safe_file->SafeClose();
		}
		else
			file->Close();
		if(take_over)
			OP_DELETE(file);
		file = NULL;
	}
}

void DataStream_GenericFile::ErrorClose()
{
	if(file)
	{
		//DS_Debug_Printf1("DataStream_GenericFile::Close Closing file %s\n", make_singlebyte_in_tempbuffer(file->GetFullPath(), uni_strlen(file->GetFullPath())));
		file->Close();
		if(take_over)
			OP_DELETE(file);
		file = NULL;
	}
}

uint32	DataStream_GenericFile::GetAttribute(DataStream::DatastreamUIntAttribute attr)
{
	switch(attr)
	{
	case DataStream::KActive:
		{
			if(file == NULL)
				return FALSE;

			unsigned long len = 0;

			TRAPD(op_err, len = ReadDataL(NULL, 1, DataStream::KSampleOnly));
			return OpStatus::IsSuccess(op_err) && (len /*|| !file->Eof()*/);
		}
	case DataStream::KMoreData:
		{
			if(scan_buffer.GetAttribute(DataStream::KMoreData))
				return TRUE;

			if(file == NULL)
				return FALSE;

			unsigned long len = 0;

			TRAPD(op_err, len = ReadDataL(NULL, 1, DataStream::KSampleOnly));
			return OpStatus::IsSuccess(op_err) && (len /*|| !file->Eof()*/);
		}
	default:
		return DataStream::GetAttribute(attr);
	}
}

void DataStream_GenericFile::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	switch(action)
	{
	case DataStream::KCommitSampledBytes:
		{
			DS_DEBUG_Start();
			
			DS_Debug_Printf1("DataStream_GenericFile::CommitSampledBytesL %u bytes\n", arg1);
			
			scan_buffer.PerformActionL(DataStream::KCommitSampledBytes, arg1);
		}
		break;
	case DataStream::KInternal_InitRead:
	case DataStream::KInternal_InitWrite:
	case DataStream::KInternal_FinalRead:
	case DataStream::KInternal_FinalWrite:
		// No action
		break;
	case DataStream::KInit:
		PerformActionL((writing ? DataStream::KInternal_InitWrite: DataStream::KInternal_InitRead), arg1, arg2);
		break;
	default: 
		DataStream::PerformActionL(action, arg1, arg2);
	}
}

#ifdef DATASTREAM_ENABLE_FILE_OPENED
BOOL DataStream_GenericFile::Opened()
{
	return(file && file->IsOpen());
}
#endif

/*
void DataStream_GenericFile::ClearBuffer()
{
}
*/

unsigned long DataStream_GenericFile::ReadFromFile(unsigned char *buffer, unsigned long len)
{
	DS_DEBUG_Start();

	OpFileLength read_len = 0;

	if(len ==0)
		return 0;

	if(file)
	{
		OpStatus::Ignore(file->Read(buffer, len, &read_len));
	}
	//if(read_len != len && !scan_buffer.MoreData())
	//	Close();

	DS_Debug_Write("DataStream_GenericFile::ReadFromFile", "read data", buffer, (unsigned long) read_len);
	return (unsigned long) read_len;
}

unsigned long DataStream_GenericFile::ReadDataL(unsigned char *buffer, unsigned long len, DataStream::DatastreamCommitPolicy sample)
{
	DS_DEBUG_Start();

	if (len == 0)
		return 0;

	if(file && len > scan_buffer.GetLength())
	{
		byte *read_buffer = (byte *) g_memory_manager->GetTempBuf2k();
		uint32 s_len = g_memory_manager->GetTempBuf2kLen();
		
		while(scan_buffer.GetLength() < len)
		{
			unsigned long did_read_len = ReadFromFile(read_buffer, s_len);
			
			scan_buffer.WriteDataL(read_buffer, did_read_len);
			
			if(did_read_len < s_len)
				break;
		}
		
		if(!file || file->Eof())
			scan_buffer.PerformActionL(DataStream::KFinishedAdding);
	}

	uint32 read_len = scan_buffer.ReadDataL(buffer, len, sample);

	DS_Debug_Write("DataStream_GenericFile::ReadDataL", "read data", buffer, read_len);

	return read_len;
}

void DataStream_GenericFile::ExportDataL(DataStream *dst)
{
	dst->AddContentL(this);
}

void DataStream_GenericFile::WriteDataL(const unsigned char *buffer, unsigned long len)
{
	DS_DEBUG_Start();
	unsigned long write_len = 0;

	DS_Debug_Write("DataStream_GenericFile::WriteDataL", "writing data", buffer, len);
	write_len = (file && OpStatus::IsSuccess(file->Write(buffer, len)) ? len : 0);
	if(write_len != len)
		ErrorClose();
}

#ifdef DATASTREAM_ENABLE_FILE_ADD_STREAM
void DataStream_GenericFile::AddContentL(DataStream *src)
{
	DS_DEBUG_Start();

	DS_Debug_Printf0("DataStream_GenericFile::AddContentL: Adding record\n");
	src->WriteRecordL(this);
	DS_Debug_Printf0("DataStream_GenericFile::AddContentL: Added record\n");
}
#endif

#ifdef _DATASTREAM_DEBUG_
const char *DataStream_GenericFile::Debug_ClassName()
{
	return "DataStream_GenericFile";
}
#endif

#endif // DATASTREAM_GENERIC_FILE_ENABLED
