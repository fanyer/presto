/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** This file describe a class used to asynchronously export a cache storage
**
** Luca Venturi
**
*/

#include "core/pch.h"

#ifdef PI_ASYNC_FILE_OP
#include "modules/cache/cache_exporter.h"
#include "modules/util/opfile/opfile.h"
#include "modules/url/url_ds.h"
#include "modules/cache/url_cs.h"
#include "modules/pi/OpSystemInfo.h"

CacheAsyncExporter::CacheAsyncExporter(AsyncExporterListener *progress_listener, URL_Rep *rep, BOOL safe_mode, BOOL delete_if_error, UINT32 param):
		url_rep(rep), pos(FILE_LENGTH_NONE), len(0), safe(safe_mode), user_param(param), delete_on_error(delete_if_error),
		listener(progress_listener), input(NULL), output(NULL), buf(NULL)
{
	OP_ASSERT(listener && url_rep);
	OP_ASSERT(url_rep->GetDataStorage() && url_rep->GetDataStorage()->GetCacheStorage());

	url_rep->IncUsed(1);
}

OP_STATUS CacheAsyncExporter::StartExport(const OpStringC &file_name)
{
	RETURN_IF_ERROR(name.Set(file_name));

	if(name.IsEmpty())
		return OpStatus::ERR;

	OP_ASSERT(pos==FILE_LENGTH_NONE);

	if(!listener || !url_rep)
		return OpStatus::ERR_NULL_POINTER;

	if(pos!=FILE_LENGTH_NONE)
		return OpStatus::ERR_OUT_OF_RANGE;

	buf = OP_NEWA(UINT8, STREAM_BUF_SIZE);

	RETURN_OOM_IF_NULL(buf);

	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_CACHE_EXPORT, (MH_PARAM_1)this));

	if(!g_main_message_handler->PostMessage(MSG_CACHE_EXPORT, (MH_PARAM_1)this, 0))
		return OpStatus::ERR;

	return OpStatus::OK;
}

void CacheAsyncExporter::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT((CacheAsyncExporter *)par1 == this);

	OP_STATUS ops=OpStatus::OK;

	if(pos==FILE_LENGTH_NONE) // First message
	{
		OP_ASSERT(!input && !output);
		Cache_Storage *cs=url_rep->GetDataStorage()->GetCacheStorage();

		pos=0;
		len=0;

		// Try to open the input file
		if(!safe)
			input=cs->OpenCacheFileForExportAsReadOnly(ops);

		if(input)
			input->GetFileLength(len);

		if(!len)
			url_rep->GetAttribute(URL::KContentSize, &len);

		// First notification, to be able to show something to the user before starting synchronous operations
		if(listener)
			listener->NotifyStart(len, url_rep, name, user_param);

		// First progress notification
		if(listener)
			listener->NotifyProgress(0, len, url_rep, name, user_param);

		// If the problem is that the file is still in memory because it has not been saved, save it synchronously then continue asynchronously
		if(!input && !safe && !cs->IsEmbedded() && !cs->IsEmbeddable() && !cs->GetContainerID())
		{
			OpFileFolder folder;
			OpStringC cache_file=url_rep->GetDataStorage()->GetCacheStorage()->FileName(folder);

			if(cache_file.IsEmpty())
			{
				url_rep->GetDataStorage()->GetCacheStorage()->Flush(); // Save the file
				url_rep->GetDataStorage()->GetCacheStorage()->CloseFile();
			}

			// The problem is that files become embedded or containers after Flush... so it needs to be checked again (strictly speaking, the first check is useless)
			if(!cs->IsEmbedded() && !cs->GetContainerID())
			{
				input=url_rep->GetDataStorage()->GetCacheStorage()->OpenCacheFileForExportAsReadOnly(ops);

				if(input) // Fast operations are now possible
				{
					input->GetFileLength(len);

					// Repeat first progress notification
					if(listener)
						listener->NotifyProgress(0, len, url_rep, name, user_param);
				}
			}
		}

		//// If it was impossible to open the second file, switch to synchronous

		// "Safe", synchronous export (there are cases where OpenCacheFileForExportAsReadOnly() can still refuse to open the file)
		if(safe || !input)
		{
			ops=(OP_STATUS)url_rep->SaveAsFile(name);

			if(OpStatus::IsSuccess(ops))
				pos=len;
		}
		else
		{
			if(OpStatus::IsError(ops) || !input ||
			   !(output=OP_NEW(OpFile, ())) ||
			   OpStatus::IsError(ops=output->Construct(name)) ||
			   OpStatus::IsError(ops=output->Open(OPFILE_WRITE)))
			{
				NotifyEndAndDeleteThis(OpStatus::ERR_NO_MEMORY);  // The destructor will delete the pointers

				return;
			}

		}
	}

	if(!safe)
	{
		// Not "safe", asynchronous export
		double start=g_op_time_info->GetTimeUTC();
		double cur_time=start;
		OpFileLength bytes_read=1;

		while(OpStatus::IsSuccess(ops) && pos<len && cur_time<start+100 && bytes_read>0)
		{
			if(OpStatus::IsSuccess(ops=input->Read(buf, STREAM_BUF_SIZE, &bytes_read)) &&
							bytes_read>0)
			{
				ops=output->Write(buf, bytes_read);
			}

			if(bytes_read>0 && bytes_read!=FILE_LENGTH_NONE)
				pos+=bytes_read;

			cur_time=g_op_time_info->GetTimeUTC();
			//cur_time=start+100;  // Simulation for selftests
		}

		if(bytes_read==0 || bytes_read==FILE_LENGTH_NONE)
			ops=OpStatus::ERR;
	}

	OP_ASSERT(pos<=len);

	if(listener)
		listener->NotifyProgress(pos, len, url_rep, name, user_param);

	if(pos>=len)  // File exported
	{
		OP_ASSERT(ops==OpStatus::OK);

		NotifyEndAndDeleteThis(OpStatus::OK);
	}
	else if(OpStatus::IsError(ops))	  // Error
		NotifyEndAndDeleteThis(ops);
	else
	{
		OP_ASSERT(!safe);

		// Post a message, the export will continue
		if(!g_main_message_handler->PostMessage(MSG_CACHE_EXPORT, (MH_PARAM_1)this, 0))
			NotifyEndAndDeleteThis(OpStatus::ERR); // Message not posted ==> Stop everything or we will leak memory and file handles
	}
}

void CacheAsyncExporter::NotifyEndAndDeleteThis(OP_STATUS ops)
{
	OP_ASSERT(listener);

	// Close the files, to reduce a bit the resource usage before notifying that the export is finished
	if(input)
		OpStatus::Ignore(input->Close());
	if(output)
	{
		OpStatus::Ignore(output->Close());

		if(delete_on_error && OpStatus::IsError(ops))
			OpStatus::Ignore(output->Delete());
	}

	if(listener)
		listener->NotifyEnd(ops, pos, len, url_rep, name, user_param);

	OP_DELETE(this);
}

CacheAsyncExporter::~CacheAsyncExporter()
{
	g_main_message_handler->UnsetCallBacks(this);

	if(input)
		input->Close();
	if(output)
		output->Close();

	OP_DELETE(input);
	OP_DELETE(output);

	url_rep->DecUsed(1);

	OP_DELETEA(buf);
}
#endif // CACHE_EXPORTER_H
