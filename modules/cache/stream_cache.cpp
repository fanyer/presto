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

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/url/url2.h"
#include "modules/url/url_rep.h"
#include "modules/cache/url_cs.h"
#include "modules/cache/cache_common.h"


StreamCache_Storage::StreamCache_Storage(URL_Rep *url)
: Cache_Storage(url)
{
	local_desc = 0;
	descriptor_created = FALSE;
	expecting_subdescriptor = FALSE;
		
	max_buffersize = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize)*1024;
	content_loaded = 0;
		
	if(max_buffersize < MAX_MEMORY_CONTENT_SIZE)
		 max_buffersize = MAX_MEMORY_CONTENT_SIZE;

	max_buffersize *= 4;
}

StreamCache_Storage::~StreamCache_Storage()
{
	OP_DELETE(local_desc);
}

void StreamCache_Storage::ConstructL(Cache_Storage * initial_storage, OpStringS8 &content_encoding)
{
	TakeOverContentEncoding(content_encoding);
	local_desc = Cache_Storage::GetDescriptor(NULL, TRUE,FALSE);
	if (local_desc == 0)
		LEAVE(OpStatus::ERR_NO_MEMORY);
	if (initial_storage!=0)
	{
		OpStringS8 tmp_ce;
		ANCHOR(OpStringS8, tmp_ce);
		tmp_ce.SetL(initial_storage->GetContentEncoding());
		TakeOverContentEncoding(tmp_ce);

		LEAVE_IF_ERROR(AddDataFromCacheStorage(initial_storage));
	};
}

OP_STATUS StreamCache_Storage::StoreData(const unsigned char *buffer, unsigned long buf_len, OpFileLength start_position)
{
	URL_DataDescriptor *temp = local_desc;
	OP_STATUS op_err = OpStatus::OK;

	if(!temp)
	{
		// Always Construct() before StoreData()!
		OP_ASSERT(0);
		return OpStatus::ERR;
	}

	if(temp)
	{
		unsigned long new_buflen = temp->GetBufSize() + buf_len;

		if(new_buflen > (max_buffersize *4) ||
			temp->Grow(new_buflen) < new_buflen)
		{
			url->HandleError(ERR_SSL_ERROR_HANDLED);
			return OpStatus::ERR_NO_MEMORY;
		}

		TRAP(op_err,temp->AddContentL(NULL, buffer, buf_len));
		content_loaded += buf_len;
	}

	return op_err;
}

unsigned long StreamCache_Storage::RetrieveData(URL_DataDescriptor *desc,BOOL &more)
{
	OP_MEMORY_VAR int addlen = local_desc->GetBufSize();
	OP_MEMORY_VAR unsigned long added=1;
	while (added > 0 && addlen > 0)
	{
		int new_len = desc->GetBufSize() + addlen;
		desc->Grow(new_len);
		TRAPD(op_err, added = desc->AddContentL(NULL, (const unsigned char *) local_desc->GetBuffer(), addlen));
		OpStatus::Ignore(op_err);
		local_desc->ConsumeData(added);
		addlen = local_desc->GetBufSize();
	};
	more = !GetFinished()&& ((URLStatus) url->GetAttribute(URL::KLoadStatus) == URL_LOADING);

	return (desc ? desc->GetBufSize() : 0);
}

URL_DataDescriptor *StreamCache_Storage::GetDescriptor(MessageHandler *mh, BOOL get_raw_data, BOOL get_decoded_data, Window *window, 
													   URLContentType override_content_type, unsigned short override_charset_id, BOOL get_original_content, unsigned short parent_charset_id)
{
	if (expecting_subdescriptor && get_raw_data && !get_decoded_data)
	{
		expecting_subdescriptor = FALSE;
		URL_DataDescriptor * ret_desc = Cache_Storage::GetDescriptor(mh, TRUE,FALSE, window, override_content_type, override_charset_id, get_original_content, parent_charset_id);
		return ret_desc;
	};

	if (descriptor_created)
		return 0;

	descriptor_created = TRUE;
	// FIXME: This will cause us to expect a request for a
	// subdescriptor in some cases when we're not going to get one.
	// The only side effect of that is that it will be possible to
	// obtain two descriptors to the same data.  Which means that the
	// data on both descriptors will probably be corrupt...
	expecting_subdescriptor = !get_raw_data || get_decoded_data;
	URL_DataDescriptor * ret_desc = Cache_Storage::GetDescriptor(mh, get_raw_data, get_decoded_data, window, override_content_type, override_charset_id, get_original_content, parent_charset_id);
	return ret_desc;
}


OP_STATUS StreamCache_Storage::AddDataFromCacheStorage(Cache_Storage * newdata)
{
	URL_DataDescriptor * data = newdata->GetDescriptor(NULL, TRUE, FALSE);
	if (data == 0)
		return OpStatus::ERR; // Or does this mean no data, i.e. OpStatus::OK?

	URL_DataDescriptor * target = local_desc;
	if (target==0)
		return OpStatus::ERR;

	BOOL more = TRUE;
	data->RetrieveData(more);
	while (data->GetBufSize()>0 || more==TRUE)
	{
		unsigned int new_len = target->GetBufSize() + data->GetBufSize();
		if (target->Grow(new_len) < new_len)
		{
			OP_DELETE(data);
			return OpStatus::ERR_NO_MEMORY;
		};
		unsigned long added = target->AddContentL(NULL, (unsigned char *) data->GetBuffer(), data->GetBufSize());
		if (added==0)
		{
			// Argh!  But let's at least avoid an eternal loop....
			OP_DELETE(data);
			return OpStatus::ERR;
		};
		content_loaded += added;
		data->ConsumeData(added);
		data->RetrieveData(more);
	};

	OP_DELETE(data);
	return OpStatus::OK;
}
