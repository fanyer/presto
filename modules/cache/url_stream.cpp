/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _URL_STREAM_ENABLED_

#include "modules/url/url2.h"
#include "modules/cache/url_dd.h"
#include "modules/cache/url_stream.h"
#include "modules/formats/url_dfr.h"

URL_DataStream::URL_DataStream(URL &url, BOOL get_raw_data, BOOL get_decoded_data)
 :	source_target(url), 
	reader(NULL),
	more_data(FALSE),
	m_raw(get_raw_data), 
	m_decoded(get_decoded_data)
{
}

URL_DataStream::~URL_DataStream()
{
	OP_DELETE(reader);
}

BOOL URL_DataStream::CheckReader()
{
	if(!reader)
		reader = source_target->GetDescriptor(NULL, URL::KFollowRedirect, m_raw, m_decoded);

	return (reader ? TRUE : FALSE);
}

void URL_DataStream::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	switch(action)
	{
	case DataStream::KCommitSampledBytes:
		if(arg1!= 0 && CheckReader())
			reader->ConsumeData(arg2);
		break;
	case DataStream::KFinishedAdding:
		source_target->WriteDocumentDataFinished();
		break;
	default:
		DataStream::PerformActionL(action, arg1, arg2);
	}
}

uint32	URL_DataStream::GetAttribute(DataStream::DatastreamUIntAttribute attr)
{
	switch(attr)
	{
	case DataStream::KActive:
		return more_data || reader->GetBufSize();
	case DataStream::KMoreData:
		return CheckReader();
	default: 
		return DataStream::GetAttribute(attr);
	}
}


unsigned long URL_DataStream::ReadDataL(unsigned char *buffer, unsigned long len, DataStream::DatastreamCommitPolicy sample)
{
	if(len == 0 || buffer == NULL || !CheckReader())
		return 0;

	unsigned long read_len=0;
	unsigned long copy_len=0;

	while(len > 0)
	{
		if(sample == DataStream::KReadAndCommitOnComplete && copy_len < len)
		{
			reader->Grow(len);
		}
		reader->RetrieveData(more_data);

		if(!reader->GetBuffer() || !reader->GetBufSize())
			break;

		copy_len = len;
		if(copy_len > reader->GetBufSize())
			copy_len = reader->GetBufSize();

		if(buffer)
			op_memcpy(buffer, reader->GetBuffer(), copy_len);
		if(sample == DataStream::KReadAndCommit || (sample == DataStream::KReadAndCommitOnComplete && copy_len == len))
			reader->ConsumeData(copy_len);

		if(buffer)
			buffer += copy_len;
		read_len += copy_len;
		len -= copy_len;

		if(sample == DataStream::KReadAndCommitOnComplete)
			break;
	}

	return read_len;
}

void URL_DataStream::WriteDataL(const unsigned char *buffer, unsigned long len)
{
	source_target->SetAttributeL(URL::KLoadStatus, URL_LOADING);
	source_target->WriteDocumentData(URL::KNormal, (const char *) buffer, len);
}

#ifdef FORMATS_DATAFILE_ENABLED
DataStream *URL_DataStream::CreateRecordL()
{
  return OP_NEW_L(DataFile_Record, ());
}
#endif // FORMATS_DATAFILE_ENABLED

#endif // _URL_STREAM_ENABLED_
