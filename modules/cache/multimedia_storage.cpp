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

#ifdef MULTIMEDIA_CACHE_SUPPORT
#ifndef RAMCACHE_ONLY

#include "modules/url/url2.h"
#include "modules/url/url_ds.h"
#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/cache/url_cs.h"
#include "modules/cache/multimedia_cache.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"



Multimedia_Storage::Multimedia_Storage(URL_Rep *url_rep):
Persistent_Storage(url_rep)
{
#ifdef CACHE_URL_RANGE_INTEGRATION
	enable_loader=TRUE;
#endif// CACHE_URL_RANGE_INTEGRATION
	mcf=NULL;
#if (CACHE_SMALL_FILES_SIZE>0 || CACHE_CONTAINERS_ENTRIES>0)
	plain_file=TRUE; /*No embedding or containers in these files  :-)*/
#endif
	SetForceSave(TRUE); /* To set Write Position every time a new segment is downloaded */
}

BOOL Multimedia_Storage::IsStreamRequired(URL_Rep *rep, BOOL &ram_stream)
{
	ram_stream = FALSE;

#ifdef MULTIMEDIA_CACHE_STREAM
	URLType utype = (URLType)rep->GetAttribute(URL::KType);

	// Streaming data: or file: doesn't make any sense
	if (utype == URL_DATA || utype == URL_FILE)
		return FALSE;

	// HTTPS usually is in RAM, but not always
	if (utype == URL_HTTPS &&
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
		!rep->GetAttribute(URL::KNeverFlush) &&
#endif
		!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheHTTPS))
	{
		ram_stream = TRUE;

		return TRUE;
	}

	BOOL disk_cache_enabled = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheToDisk);
	BOOL stream_always = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MultimediaStreamAlways);

	if(disk_cache_enabled && !rep->GetAttribute(URL::KCachePolicy_NoStore) && !stream_always)
		return FALSE;

	// Streaming
	if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MultimediaStreamRAM))
		ram_stream = TRUE;

	return TRUE;
#else
	return FALSE;
#endif
}


OP_STATUS Multimedia_Storage::Construct(const OpStringC &afilename, BOOL force_ram_stream)
{
	OP_ASSERT(afilename.HasContent());
	
	if(!mcf) // Create and construct the multimedia file
	{
		if(!afilename.HasContent())
			return OpStatus::ERR_OUT_OF_RANGE;
			
		RETURN_OOM_IF_NULL(mcf=OP_NEW(MultimediaCacheFile, ()));
		
		OP_STATUS ops=OpStatus::ERR;

// Stream support requires the media module to import API_MULTIMEDIA_CACHE_STREAM
#ifdef MULTIMEDIA_CACHE_STREAM
		BOOL ram_stream;
		BOOL stream = IsStreamRequired(url, ram_stream);

		OP_ASSERT( !(ram_stream && !stream) );

		if(!stream)
			ops=mcf->ConstructFile(afilename.CStr(), folder);  // Normal case, multi segment
		else
		{
			// Streaming
			int stream_size=g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MultimediaStreamSize);

			// To be on the safe side, when streaming the URL should be unique
			urlManager->MakeUnique(url);

			OP_ASSERT(stream_size);

			if(!stream_size)
				stream_size=512; // Default: 512 KB, just to avoid a failure

			// Stream on RAM
			if(!ram_stream)
				ops=mcf->ConstructFile(afilename.CStr(), folder, stream_size*1024);  // Stream on disk
			else
				ops=mcf->ConstructMemory(stream_size*1024);  // Stream on RAM

			// By default, we stream with "consume on write"
			// Check Multimedia_cache.h or the documentation for more information
			mcf->ActivateStreaming(CONSUME_ON_WRITE);
		}
#else
		ops=mcf->ConstructFile(afilename.CStr(), folder);  // Normal case, multi segment
#endif // MULTIMEDIA_CACHE_STREAM
		
		if(OpStatus::IsError(ops))
		{
			OP_DELETE(mcf);
			mcf=NULL;
			
			return ops;
		}
		
		if(!filename.HasContent())
			RETURN_IF_ERROR(filename.Set(afilename));
			
		OP_ASSERT(!filename.Compare(afilename));
	}
	
	return OpStatus::OK;
}

OP_STATUS Multimedia_Storage::Construct(FileName_Store &filenames, OpFileFolder folder, const OpStringC &afilename, OpFileLength file_len)
{
	OP_ASSERT(!mcf);
	
	RETURN_IF_ERROR(Persistent_Storage::Construct(filenames, folder, afilename, file_len));
	
	if(!mcf) // Create and construct the multimedia file
	{
		RETURN_OOM_IF_NULL(mcf=OP_NEW(MultimediaCacheFile, ()));
		
		OP_STATUS ops=mcf->ConstructFile(afilename.CStr(), folder); 
		
		if(OpStatus::IsError(ops))
		{
			OP_DELETE(mcf);
			mcf=NULL;
			
			return ops;
		}
	}
	
	return OpStatus::OK;
}


Multimedia_Storage* Multimedia_Storage::Create(URL_Rep *url_rep, const OpStringC &afilename, BOOL force_ram_stream)
{
	Multimedia_Storage* ms = OP_NEW(Multimedia_Storage, (url_rep));
	if (!ms)
		return NULL;
		
	if (afilename.HasContent() && OpStatus::IsError(ms->Construct(afilename, force_ram_stream)))
	{
			OP_DELETE(ms);
			return NULL;
	}
	return ms;
}

Multimedia_Storage* Multimedia_Storage::Create(URL_Rep *url_rep, FileName_Store &filenames, OpFileFolder folder, const OpStringC &afilename, OpFileLength file_len)
{
	Multimedia_Storage* ms = OP_NEW(Multimedia_Storage, (url_rep));
	if (!ms)
		return NULL;
	if (OpStatus::IsError(ms->Construct(filenames, folder, afilename, file_len)))
	{
		OP_DELETE(ms);
		return NULL;
	}
	return ms;
}

OpFileDescriptor* Multimedia_Storage::CreateAndOpenFile(const OpStringC &filename, OpFileFolder folder, OpFileOpenMode mode, OP_STATUS &op_err, int flags)
{
	OP_ASSERT(mode==OPFILE_READ || mode==(OPFILE_APPEND | OPFILE_READ) || mode==OPFILE_WRITE);
	
	op_err=OpStatus::OK;
	
	if(!mcf)
	{
		mcf=OP_NEW(MultimediaCacheFile, ());
		
		if(mcf)
		{
			if(OpStatus::IsError(op_err=mcf->ConstructFile(filename.CStr(), folder, flags)))
			{
				OP_DELETE(mcf);
				mcf=NULL;
			}
		}
		else
			op_err=OpStatus::ERR_NO_MEMORY;
	}
	
	OpFileDescriptor *desc=(mcf)?mcf->CreateFileDescriptor(mode):NULL;
	
	if(!desc && !OpStatus::IsError(op_err))
		op_err=OpStatus::ERR;
		
	return desc;
}

OP_STATUS Multimedia_Storage::GetUnsortedCoverage(OpAutoVector<StorageSegment> &out_segments, OpFileLength start, OpFileLength len)
{
	OP_ASSERT(mcf);
	
	if(!mcf)
		return OpStatus::ERR_NULL_POINTER;
	
	Flush();  // Important to provide access to the latest bytes, to not advertise something different than the disk
	
	OpAutoVector<MultimediaSegment> segments;
	
	RETURN_IF_ERROR(mcf->GetUnsortedCoverage(segments, start, len));
	
	return ConvertSegments(out_segments, segments);
	
}

OP_STATUS Multimedia_Storage::GetSortedCoverage(OpAutoVector<StorageSegment> &out_segments, OpFileLength start, OpFileLength len, BOOL merge)
{
	OP_ASSERT(mcf);
	
	if(!mcf)
		return OpStatus::ERR_NULL_POINTER;
		
	Flush();  // Important to provide access to the latest bytes, to not advertise something different than the disk
		
	OpAutoVector<MultimediaSegment> segments;
	
	RETURN_IF_ERROR(mcf->GetSortedCoverage(segments, start, len, merge));
	
	return ConvertSegments(out_segments, segments);
}

OP_STATUS Multimedia_Storage::GetMissingCoverage(OpAutoVector<StorageSegment> &missing, OpFileLength start, OpFileLength len)
{
	OP_ASSERT(mcf);
	
	if(!mcf)
		return OpStatus::ERR_NULL_POINTER;
		
	Flush();  // Important to provide access to the latest bytes, to not advertise something different than the disk
		
	return mcf->GetMissingCoverage(missing, start, len);
}

void Multimedia_Storage::GetPartialCoverage(OpFileLength position, BOOL &available, OpFileLength &length, BOOL multiple_segments)
{
	OP_ASSERT(mcf);
	
	if(!mcf)
	{
		available=FALSE;
		length=0;

		return;
	}
		
	mcf->GetPartialCoverage(position, available, length, multiple_segments);
}

OpFileLength Multimedia_Storage::EstimateContentAvailable()
{
	OpFileLength len=0;
	BOOL available;

	if(!cache_file)
		return 0;

	GetPartialCoverage(((MultimediaCacheFileDescriptor *)cache_file)->GetReadPosition(), available, len, TRUE);

	if(available)
		return len;

	return 0;
}

OP_STATUS Multimedia_Storage::ConvertSegments(OpAutoVector<StorageSegment> &out_segments, OpAutoVector<MultimediaSegment> &in_segments)
{
	UINT32 in_len=in_segments.GetCount();
	UINT32 out_len=out_segments.GetCount();

	for(UINT32 i=0; i<in_len; i++)
	{
		MultimediaSegment *mseg=in_segments.Get(i);
		
		OP_ASSERT(mseg);
		
		if(mseg && mseg->GetContentLength() > 0)
		{
			StorageSegment *sseg=NULL;

			if(i<out_len)
				sseg=out_segments.Get(i); // Tries to recycle the segment

			if(!sseg) // Recycling failed: create a new segment
			{
				sseg=OP_NEW(StorageSegment, ());

				RETURN_OOM_IF_NULL(sseg);

				OP_STATUS ops;

				if(i<out_len)
				{	// The segment spot was NULL, so we fill it
					OP_ASSERT(out_segments.GetCount()>i);
					OP_ASSERT(out_segments.Get(i)==NULL);

					ops=out_segments.Replace(i, sseg);
				}
				else // We are past the end of the segments
				{
					OP_ASSERT(out_segments.GetCount()==i);

					ops=out_segments.Add(sseg);
				}

				if(OpStatus::IsError(ops))
				{
					OP_DELETE(sseg);
					
					return ops;
				}

				OP_ASSERT(out_segments.GetCount()>=i);
			}
			
			sseg->content_start=mseg->GetContentStart();
			sseg->content_length=mseg->GetContentLength();
		}
	}

	OP_ASSERT(out_segments.GetCount()>=in_segments.GetCount());
	OP_ASSERT(out_segments.GetCount()>=out_len);
	OP_ASSERT(in_segments.GetCount()>=in_len);

	// Delete segments not needed
	while(out_segments.GetCount()>in_len)
		out_segments.Delete(in_len);


	return OpStatus::OK;
}

BOOL Multimedia_Storage::Flush()
{
	BOOL b=Persistent_Storage::Flush();
	
	if(mcf)
		mcf->FlushBuffer(); // TO DO: does an error should change the return code?
	
	save_position=FILE_LENGTH_NONE;
	
	return b;
}

OP_STATUS Multimedia_Storage::SetWritePosition(OpFileDescriptor *file, OpFileLength start_position)
{
	OP_ASSERT(file && start_position != FILE_LENGTH_NONE);
	
	if(!file)
		return OpStatus::ERR_NULL_POINTER;
		
	if(start_position == FILE_LENGTH_NONE)  // APPEND
		return OpStatus::OK;
		
	((MultimediaCacheFileDescriptor *)file)->SetWritePosition(start_position);
	
	return OpStatus::OK;
}

OP_STATUS Multimedia_Storage::CheckFilename()
{
	RETURN_IF_ERROR(File_Storage::CheckFilename());
	
	return Construct(filename);
}

Multimedia_Storage::~Multimedia_Storage() { OP_DELETE(mcf); }

void Multimedia_Storage::GetCacheInfo(BOOL &streaming, BOOL &ram, BOOL &embedded_storage)
{
	if(mcf)
	{
		streaming=mcf->IsStreaming();
		ram=mcf->IsInRAM();
	}
	else
	{
		streaming=FALSE;
		ram=FALSE;
	}

	embedded_storage=FALSE;
}

BOOL Multimedia_Storage::IsExportAllowed()
{
	OpAutoVector<StorageSegment> missing;
	OpFileLength size=0;

	url->GetAttribute(URL::KContentSize, &size);

	if(size==0)
		size=FILE_LENGTH_NONE;  // 0 sounds suspicious, so we will assume that if we have some content, the last byte already downloaded is the end of the file

	OP_STATUS ops=GetMissingCoverage(missing, 0, size);

	return OpStatus::IsSuccess(ops) && missing.GetCount()==0;
}

void Multimedia_Storage::SetFinished(BOOL force)
{
	OP_NEW_DBG("Multimedia_Storage::SetFinished()", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - cache_content: %d - ctx id: %d - read_only: %d - info.completed: %d - IsDiskContentAvailable(): %d \n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, (UINT32) cache_content.GetLength(), url->GetContextId(), read_only, GetFinished(), IsDiskContentAvailable()));

	Flush();
	Cache_Storage::SetFinished();
	SetInfoCompleted(TRUE);
}

#ifdef CACHE_URL_RANGE_INTEGRATION
BOOL Multimedia_Storage::CustomizeLoadDocument(MessageHandler* mh, const URL& referer_url, const URL_LoadPolicy &loadpolicy, CommState &comm_state)
{
	if(!enable_loader)
		return FALSE;

	OP_ASSERT(url && url->GetDataStorage());

	OpFileLength range_start=0;
	OpFileLength range_end=0;
	url->GetAttribute(URL::KHTTPRangeStart, &range_start);
	url->GetAttribute(URL::KHTTPRangeEnd, &range_end);

	if((range_start != FILE_LENGTH_NONE && range_start>0) || (range_end != FILE_LENGTH_NONE && range_end>0) || (range_start==0 && range_end==FILE_LENGTH_NONE)) // Valid range
	{
		enable_loader=FALSE;
		comm_state = loader.StartLoadDocument(range_start, range_end, url, mh, referer_url, loadpolicy);
		enable_loader=TRUE;

		url->SetAttribute(URL::KHTTPRangeStart, &range_start);
		url->SetAttribute(URL::KHTTPRangeEnd, &range_end);

		return TRUE;
	}
	return FALSE;
}

/// Start the loading process
CommState MultimediaLoader::StartLoadDocument(OpFileLength range_start, OpFileLength range_end, const URL_Rep *rep, MessageHandler* mh, const URL& referer_url, const URL_LoadPolicy &policy)
{
	OP_ASSERT(rep);

	load_url = URL(const_cast<URL_Rep *>(rep), (const char*)NULL);

	OP_ASSERT(load_url.GetRep() && load_url.GetRep()==rep && load_url.GetRep()->GetDataStorage() && load_url.GetRep()->GetDataStorage()->GetCacheStorage());

	if(load_policy.GetReloadPolicy()!=URL_Reload_Full && start!=FILE_LENGTH_NONE)
	{
		start = range_start;
		end = range_end;
		load_mh = mh;
		load_referer_url = referer_url;
		load_policy = policy;

		BOOL available;
		OpFileLength length=0;
		OpFileLength new_start=start; // default: requested range
		OpFileLength new_end=end; // default: requested range

		// Check what's available
		load_url.GetRep()->GetDataStorage()->GetCacheStorage()->GetPartialCoverage(start, available, length, TRUE);

		if(available) // The beginning of the file is cached: ask to download the rest
		{
			// Download everything but the first segment cached.
			// FIXME: a better approach should check for other parts in cache, avoiding double loading
			new_start+=length;

			if(new_start>new_end) // Completely in cache: simply send a message
			{
				mh->PostMessage(MSG_URL_DATA_LOADED, rep->GetID(), FALSE);

				return COMM_LOADING;
			}
		}
		else
		{
			OpAutoVector<StorageSegment>segments;

			// This can probably be done quicker, with a GetPartialCoverageBack() method
			if(OpStatus::IsError(load_url.GetRep()->GetDataStorage()->GetCacheStorage()->GetSortedCoverage(segments, new_start, new_end!=FILE_LENGTH_NONE ? (new_end - new_start): INT_MAX, TRUE)))
				return COMM_REQUEST_FAILED;

			// Partially cached: download everything but the last segment cached
			// FIXME: a better approach should check for other parts in cache, avoiding double loading
			if(segments.GetCount()>0)
			{
				StorageSegment *seg=segments.Get(segments.GetCount()-1);

				new_end=seg->content_start;

				OP_ASSERT(new_end>start && new_end<=new_end);
			}

			load_url.SetAttribute(URL::KHTTPRangeStart, &new_start);
			load_url.SetAttribute(URL::KHTTPRangeEnd, &new_end);
		}
	}

	return load_url.LoadDocument(load_mh, load_referer_url, load_policy);
}


void MultimediaLoader::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
}
#endif // CACHE_URL_RANGE_INTEGRATION
#endif // !RAMCACHE_ONLY

#endif // MULTIMEDIA_CACHE_SUPPORT
