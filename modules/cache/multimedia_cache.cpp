/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Luca Venturi
 */
#include "core/pch.h"
#include "modules/cache/multimedia_cache.h"

#ifdef MULTIMEDIA_CACHE_SUPPORT

OP_STATUS MultimediaCacheFile::ConstructPrivate(UINT8 suggested_flags, OpFileLength suggested_max_file_size, UINT16 suggested_max_segments)
{
	OP_ASSERT(segments.GetCount()==0);
	
	BOOL exists=FALSE;
	
	RETURN_IF_ERROR(sfrw.Exists(exists));
	
	// If the file header is invalid, truncate it
	if(exists)
	{
		OpFileLength len;
		
		RETURN_IF_ERROR(sfrw.GetFileLength(len));
		
		if(	len<MMCACHE_HEADER_SIZE ||		// Too short for the header
			sfrw.Read8()!='O' ||	// No Opera Multimedia Cache File
			sfrw.Read8()!='M' ||
			sfrw.Read8()!='C' ||
			sfrw.Read8()!='F' ||
			sfrw.Read8()!=1)		// No Version 1
		{
			RETURN_IF_ERROR(sfrw.Truncate());
			exists=FALSE;
		}
	}
	
	if(exists)	// Read the header
	{
		// Read the remaining part of the header
		#ifdef DEBUG_ENABLE_OPASSERT
			OpFileLength pos;
			OP_ASSERT(OpStatus::IsSuccess(sfrw.GetReadFilePos(pos)) && pos==5);
		#endif
		header_flags=sfrw.Read8();
		max_size=sfrw.Read64();
		max_segments=sfrw.Read16();
		segments.DeleteAll();
		
		#ifdef DEBUG_ENABLE_OPASSERT
			OP_ASSERT(OpStatus::IsSuccess(sfrw.GetReadFilePos(pos)) &&
					  pos==MMCACHE_HEADER_SIZE);
		#endif
		
		// Read the segments informations from the disk
		RETURN_IF_ERROR(LoadAllSegments(FALSE));
		
		#ifdef DEBUG_ENABLE_OPASSERT
			OP_ASSERT(OpStatus::IsSuccess(sfrw.GetReadFilePos(pos)) &&
					  pos==GetFullHeaderLength());
		#endif
	}
	else		// Write the header
	{
		#ifdef DEBUG_ENABLE_OPASSERT
			OpFileLength pos;
			OP_ASSERT(OpStatus::IsSuccess(sfrw.GetWriteFilePos(pos)) && pos==0);
		#endif
		
		header_flags=suggested_flags;
		
		RETURN_IF_ERROR(WriteInitialHeader());
		RETURN_IF_ERROR(sfrw.Write64(suggested_max_file_size));		// Max File Size
		RETURN_IF_ERROR(sfrw.Write16(suggested_max_segments));			// Max number of segments
		
		#ifdef DEBUG_ENABLE_OPASSERT
			OP_ASSERT(OpStatus::IsSuccess(sfrw.GetWriteFilePos(pos)) &&
					  pos==MMCACHE_HEADER_SIZE);
		#endif
		
		max_size=suggested_max_file_size;
		max_segments=suggested_max_segments;
		segments.DeleteAll();
		
		if(!max_size && ! header_flags & MMCACHE_HEADER_64_BITS)
			max_size=2*1024*1024;//UINT_MAX-65536
			
		OP_ASSERT(max_size<=UINT_MAX-65536 || (header_flags & MMCACHE_HEADER_64_BITS));
		
		// Write the segment informations to the disk
		RETURN_IF_ERROR(WriteAllSegments(FALSE, FALSE));
	}
	
	CheckInvariants();
	
	return OpStatus::OK;
}

OP_STATUS MultimediaCacheFile::ConstructFile(const uni_char* path, OpFileFolder folder, OpFileLength suggested_max_file_size, UINT16 suggested_max_segments)
{
	OP_ASSERT(!ram);

	RETURN_IF_ERROR(sfrw.ConstructFile(path, folder, FALSE));
	
	ram=FALSE;

	OP_NEW_DBG("MultimediaCacheFile::ConstructFile", "multimedia_cache.write");
	OP_DBG((UNI_L("*** File %s constructed in path %d"), path, (int)folder));

	return ConstructPrivate(MMCACHE_HEADER_64_BITS, suggested_max_file_size, suggested_max_segments);
}


OP_STATUS MultimediaCacheFile::ConstructMemory(OpFileLength suggested_max_file_size, UINT16 suggested_max_segments)
{
	OP_ASSERT(!ram);
	OP_ASSERT(suggested_max_file_size>0);

	if(!suggested_max_file_size)
		return OpStatus::ERR_OUT_OF_RANGE;

	// Check that there are not strange side effects in converting to UINT32
	OP_ASSERT(suggested_max_file_size+MMCACHE_HEADER_SIZE+suggested_max_segments*GetSegmentHeaderLength() + STREAM_BUF_SIZE == (OpFileLength)((UINT32)(suggested_max_file_size+MMCACHE_HEADER_SIZE+suggested_max_segments*GetSegmentHeaderLength() + STREAM_BUF_SIZE)));

	// Allocate enough memory for the requested size plus the header + STREAM_BUF_SIZE, because of a bug in streaming that can make the file a bit too big
	RETURN_IF_ERROR(sfrw.ConstructMemory((UINT32)(suggested_max_file_size+MMCACHE_HEADER_SIZE+suggested_max_segments*GetSegmentHeaderLength() + STREAM_BUF_SIZE)));
	
	ram=TRUE;

	return ConstructPrivate(MMCACHE_HEADER_64_BITS, suggested_max_file_size, suggested_max_segments);
}

OP_STATUS MultimediaCacheFile::WriteInitialHeader()
{
	RETURN_IF_ERROR(sfrw.SetWriteFilePos(0));

	RETURN_IF_ERROR(sfrw.Write8('O'));			// Sign
	RETURN_IF_ERROR(sfrw.Write8('M'));
	RETURN_IF_ERROR(sfrw.Write8('C'));
	RETURN_IF_ERROR(sfrw.Write8('F'));
	RETURN_IF_ERROR(sfrw.Write8(1));			// Version
	RETURN_IF_ERROR(sfrw.Write8(header_flags));	// Flags

	return OpStatus::OK;
}
OP_STATUS MultimediaCacheFile::LoadAllSegments(BOOL reposition)
{
	OP_ASSERT(segments.GetCount()==0);
	OP_ASSERT(max_segments>0);
	
	if(reposition)
		RETURN_IF_ERROR(sfrw.SetReadFilePos(MMCACHE_HEADER_SIZE));
	else
	{
		OpFileLength pos;
		
		RETURN_IF_ERROR(sfrw.GetReadFilePos(pos));
		OP_ASSERT(pos==MMCACHE_HEADER_SIZE);
		
		if(pos!=MMCACHE_HEADER_SIZE)
			return OpStatus::ERR_NOT_SUPPORTED;
	}
		
	if(segments.GetCount()!=0)
		return OpStatus::ERR_NOT_SUPPORTED;
		
	UINT8 guard_bytes=(header_flags & MMCACHE_HEADER_GUARDS)?1:0;
	OpFileLength header_full_size=GetSegmentPos(max_segments);
	OpFileLength content_virtual_position=header_full_size;  // Length of the content contained in the segments so far (used to calculate the physical position in the disk)

	cached_size=0;

	// Load the segment informations from the file
	for(UINT16 cur_segment=0; cur_segment<max_segments; cur_segment++)
	{
		// Read the segment values
		OpFileLength content_start;
		OpFileLength content_len;
		OpFileLength file_start=content_virtual_position+(guard_bytes);
		UINT8 flags;
		
		if(header_flags & MMCACHE_HEADER_64_BITS)
		{
			content_start=sfrw.Read64();
			content_len=sfrw.Read64();
		}
		else
		{
			content_start=sfrw.Read32();
			content_len=sfrw.Read32();
		}
		
		flags=sfrw.Read8();
		
		if(content_len==0) // This situation can happen for a crash or because the segments are finished
		{
			if(flags & MMCACHE_SEGMENT_NEW) // Crash: content lasts till the end of the file
			{
				OpFileLength file_len;

				RETURN_IF_ERROR(sfrw.GetFileLength(file_len));

				content_len=file_len-file_start;	// Content lasts till the end of the file
				// Don't remove this flag... it is usefull
				if(!content_len)
				{
					// Jump to the end of the header
					RETURN_IF_ERROR(sfrw.SetReadFilePos(header_full_size));
				
					break;  // Crash after updating the segment but before writing
				}
			}
			else	// This segment is after the last one
			{
				// Jump to the end of the header
				RETURN_IF_ERROR(sfrw.SetReadFilePos(header_full_size));
				
				break;
			}
		}
		
		cached_size+=content_len;
		
		// Create the segment and add it to the list
		MultimediaSegment *seg=OP_NEW(MultimediaSegment, ());
		
		if(!seg)
			return OpStatus::ERR_NO_MEMORY;
			
		OP_STATUS ops;

		if( OpStatus::IsError(ops=seg->SetSegment(file_start, content_start, content_len, flags)) || 
			OpStatus::IsError(ops=segments.Add(seg)))
		{
			OP_DELETE(seg);
			
			return ops;
		}

		if(flags & MMCACHE_SEGMENT_DIRTY)
			seg->to_be_discarded=TRUE; // Basically this segment cannot be used
		
		content_virtual_position+=guard_bytes+content_len;
	}
	
	OP_ASSERT(max_segments>=segments.GetCount());
	
	#ifdef DEBUG_ENABLE_OPASSERT
		OpFileLength pos;
		OpFileLength file_len;
		OP_ASSERT(OpStatus::IsSuccess(sfrw.GetReadFilePos(pos)) &&
				  OpStatus::IsSuccess(sfrw.GetFileLength(file_len)) &&
				  // file_len == content_virtual_position &&
				  pos==header_full_size);
	#endif
	
	return OpStatus::OK;
}

OP_STATUS MultimediaCacheFile::CloseAll()
{
	RETURN_IF_ERROR(WriteInitialHeader());
	RETURN_IF_ERROR(WriteAllSegments(TRUE, TRUE));
	RETURN_IF_ERROR(sfrw.Close());
	
	segments.DeleteAll();
	cached_size=0;
	
	return OpStatus::OK;
}

OP_STATUS MultimediaCacheFile::DeleteContent()
{
	RETURN_IF_ERROR(sfrw.Truncate());

	segments.DeleteAll();
	cached_size=0;

	RETURN_IF_ERROR(WriteInitialHeader());
	RETURN_IF_ERROR(WriteAllSegments(TRUE, TRUE));
	
	return OpStatus::OK;
}

void MultimediaCacheFile::GetOptimisticFullRange(OpFileLength &start, OpFileLength &end) const
{
	start=end=0;

	if(!GetSegmentsInUse())
		return;
	
	for(UINT32 i=0, num=GetSegmentsInUse(); i<num; i++)
	{
		MultimediaSegment *seg=segments.Get(i);
		
		OP_ASSERT(seg);
		
		if(seg)
		{
			if(i==0 || seg->GetContentStart()<start)
				start=seg->GetContentStart();
			if(i==0 || seg->GetContentEnd()>end)
				end=seg->GetContentEnd();
		}
	}
}

OP_STATUS MultimediaCacheFile::WriteAllSegments(BOOL reposition, BOOL update_header)
{
	OP_ASSERT(segments.GetCount()<=max_segments);
	OP_ASSERT(max_segments>0);
	OP_ASSERT(!update_header || reposition);  // update_header requires reposition

	CheckInvariants();
	
	if(reposition)
	{
		if(update_header)
		{
			RETURN_IF_ERROR(sfrw.SetWriteFilePos(MMCACHE_HEADER_SIZE-10));
			RETURN_IF_ERROR(sfrw.Write64(max_size));
			RETURN_IF_ERROR(sfrw.Write16(max_segments));
			
			OpFileLength pos;
		
			RETURN_IF_ERROR(sfrw.GetWriteFilePos(pos));
			OP_ASSERT(pos==MMCACHE_HEADER_SIZE);
			
			if(pos!=MMCACHE_HEADER_SIZE)
				return OpStatus::ERR_NOT_SUPPORTED;
		}
		else
			RETURN_IF_ERROR(sfrw.SetWriteFilePos(MMCACHE_HEADER_SIZE));
	}
	else
	{
		OpFileLength pos;
		
		RETURN_IF_ERROR(sfrw.GetWriteFilePos(pos));
		OP_ASSERT(pos==MMCACHE_HEADER_SIZE);
		
		if(pos!=MMCACHE_HEADER_SIZE)
			return OpStatus::ERR_NOT_SUPPORTED;
	}
		
	//if(segments.GetCount()!=max_segments)
	//	return OpStatus::ERR_NOT_SUPPORTED;

	//UINT8 guard_bytes=(header_flags & MMCACHE_HEADER_GUARDS)?1:0;
#ifdef DEBUG_ENABLE_OPASSERT
	OpFileLength header_full_size=(header_flags & MMCACHE_HEADER_64_BITS)?
		MMCACHE_HEADER_SIZE+17*max_segments:MMCACHE_HEADER_SIZE+9*max_segments;
#endif

	// Write the segment informations to the file
	UINT16 cur_segment=0;
	
	for(; cur_segment<segments.GetCount(); cur_segment++)
	{
		MultimediaSegment *seg=segments.Get(cur_segment);
		OpFileLength content_start=0;
		OpFileLength content_len=0;
		UINT8 flags=0;  
	
		if(seg)
		{
			OP_ASSERT(!seg->empty_space || seg->IsDirty());

			content_start=seg->GetContentStart();
			content_len=seg->GetContentLength()+seg->empty_space;
			flags=seg->GetFlags();
		}
		
		// Write a segment header
		RETURN_IF_ERROR(MultimediaSegment::DirectWriteHeader(sfrw, header_flags, content_start, content_len, flags));
	}
	
	// Write the segment informations to the file
	for(; cur_segment<max_segments; cur_segment++)
	{
		// Write a dummy segment header
		RETURN_IF_ERROR(MultimediaSegment::DirectWriteHeader(sfrw, header_flags, 0, 0, 0));
	}
	
	//OP_ASSERT(max_segments==segments.GetCount());
	
	#ifdef DEBUG_ENABLE_OPASSERT
		OpFileLength pos;
		OP_ASSERT(OpStatus::IsSuccess(sfrw.GetWriteFilePos(pos)) && pos == header_full_size);
	#endif
	
	CheckInvariants();
	
	return OpStatus::OK;	
}

OpFileLength MultimediaCacheFile::GetAvailableSpace()
{
	OpFileLength avail_space=0;

	if(max_size>0)
		avail_space=max_size-cached_size;
	else
		avail_space=2047*1024*1024L-cached_size; // A limit of around 2 GB is simulated
	
	if(streaming)
	{
		for(UINT32 i=segments.GetCount(); i>0; i--)
		{
			MultimediaSegment *seg=segments.Get(i-1);

			if(seg)
				avail_space+=seg->empty_space;
		}
	}

	return avail_space;
}

#ifdef DEBUG_ENABLE_OPASSERT
void MultimediaCacheFile::CheckInvariants()
{
	OP_ASSERT(GetSegmentsInUse()<=max_segments);
	OpFileLength size=0;
	int num_new_segments=0;
	int num_dirty_segments=0;
	OpFileLength empty_space=0;

	if(!streaming)
		OP_ASSERT(consume_policy==CONSUME_NONE);

	for(UINT32 i=0, num=GetSegmentsInUse(); i<num; i++)
	{
		MultimediaSegment *seg=segments.Get(i);
		
		OP_ASSERT(seg);
		
		if(seg)
		{
			seg->CheckInvariants();
			size+=seg->GetContentLength();
			empty_space+=seg->empty_space;
			if(seg->IsNew())
				num_new_segments++;
			if(seg->IsDirty())
				num_dirty_segments++;

			if(seg->reserve_segment)
			{
				OP_ASSERT(seg->reserve_segment->file_offset+seg->reserve_segment->content_len+seg->reserve_segment->empty_space==seg->file_offset);
			}
		}
	}
	
	OP_ASSERT(size==cached_size || streaming);
	OP_ASSERT(size+empty_space==cached_size);
	OP_ASSERT(num_new_segments==0 || num_new_segments==1);
	OP_ASSERT(max_size==0 || max_size>=cached_size);

	if(streaming && deep_streaming_check)
		OP_ASSERT(segments.GetCount()<=2);

	if(streaming)
		OP_ASSERT(max_size>0); // That's the concept of streaming!  :-)
}
#endif // DEBUG_ENABLE_OPASSERT

OP_STATUS MultimediaCacheFile::WriteContent(OpFileLength content_position, const void *buf, UINT32 original_len, UINT32 &written_bytes)
{
	UINT32 len=original_len;
	OpFileLength total_empty_space=0;

	if(streaming && consume_policy==CONSUME_ON_WRITE)
	{
		UINT32 consumed_bytes=0;

		total_empty_space=GetAvailableSpace();

		if(total_empty_space<original_len)
		{
			RETURN_IF_ERROR(AutoConsume(original_len-(UINT32)total_empty_space, consumed_bytes));
			len=(UINT32)(total_empty_space+consumed_bytes);

			OP_ASSERT(GetAvailableSpace()==len);
		}
	}

	RETURN_IF_ERROR(WriteContentKernel(content_position, buf, len, written_bytes));

	// While streaming, in some situations, the content needs to be written partly in the Master segment and partly in the Reserve, but
	// WriteContentKernel() can only write to a single segment.
	// So if not everything has been written, a second write is attempted, to spread the content in both the segments
	if(streaming && written_bytes<original_len)
	{
		UINT32 bytes_temp;

		if(OpStatus::IsSuccess(WriteContentKernel(content_position+written_bytes, ((UINT8 *)buf)+written_bytes, len-written_bytes, bytes_temp)))
			written_bytes+=bytes_temp;
	}

  return OpStatus::OK;
}

OP_STATUS MultimediaCacheFile::WriteContentKernel(OpFileLength content_position, const void *buf, UINT32 len, UINT32 &written_bytes)
{
	if(!sfrw.IsFileValid())
		return OpStatus::ERR_NOT_SUPPORTED;
		
	OP_ASSERT(content_position != FILE_LENGTH_NONE);
	OP_ASSERT(buf);
	OP_ASSERT(len>0);
	
	if(!buf)
		return OpStatus::ERR_NULL_POINTER;
	
	MultimediaSegment *segment=NULL;
	OpFileLength file_pos;
	
	written_bytes=0;
	
	if(len==0)
		return OpStatus::OK;
	
	RETURN_IF_ERROR(GetWriteSegmentByContentPosition(segment, content_position, file_pos, TRUE));
	
	OP_ASSERT(segment);
	OP_ASSERT(file_pos>=MMCACHE_HEADER_SIZE);
	
	if(!segment)
		return OpStatus::ERR_NULL_POINTER;

	OpFileLength empty_space=(streaming)?segment->empty_space:0;
	OpFileLength extension_bytes=empty_space;
	BOOL last_segment=GetSegmentsInUse() && segment==segments.Get(GetSegmentsInUse()-1);

	RETURN_IF_ERROR(WriteContentDirect(file_pos, buf, len, empty_space, written_bytes, last_segment));
	
	segment->AddContent(written_bytes, extension_bytes);  // Also update the empty space and cache_dize of the segment

	if(segment->reserve_segment)
	{
		// The reserve segment should have no content in this case (just created to allow streaming later)
		// If it's not the case, the content of the reserve is dropped to avoid corruption

		OP_ASSERT(segment->reserve_segment->GetContentLength()==0);
		//OP_ASSERT(!segment->empty_space); /* It can happen in legitimate cases */

		segment->reserve_segment->empty_space+=segment->reserve_segment->content_len;
		segment->reserve_segment->content_len=0;
		segment->reserve_segment->content_start=segment->content_start+segment->content_len+segment->empty_space; // The reserve points to the complete end of the segment, empty space included
	}
	
	OP_ASSERT(!empty_space || 
		(empty_space>=written_bytes || (last_segment && extension_bytes>0 && extension_bytes<=max_size-cached_size)));

	// Compute Empty space and cached size
	cached_size+=extension_bytes; // If the empty space is used, the cache_size does not change
	
	CheckInvariants();
	
	return OpStatus::OK;
}

#ifdef SELFTEST
OP_STATUS MultimediaCacheFile::DebugAddSegment(MultimediaSegment *seg)
{
	OP_ASSERT(seg);

	if(!seg)
		return OpStatus::ERR_NO_MEMORY;
	
	// Check that the segment is not yet in the array and that there is no overlapping
	if(max_segments)
	{
		OpAutoVector<MultimediaSegment> covered;

		RETURN_IF_ERROR(GetUnsortedCoverage(covered, seg->GetContentStart(), seg->GetContentLength()));

		OP_ASSERT(covered.GetCount()==0);
	}

	// Add to the array
	RETURN_IF_ERROR(segments.Add(seg));
	
	OP_ASSERT(segments.Get(segments.GetCount()-1)==seg);
	
	if(max_segments<segments.GetCount())
		max_segments=segments.GetCount();
		
	cached_size+=seg->GetContentLength();

	if(seg->IsDirty())
		seg->to_be_discarded=TRUE; // Simulate a load
	
	CheckInvariants();
	
	return OpStatus::OK;
}

OpFileLength MultimediaCacheFile::DebugGetSegmentLength(int index)
{
	return segments.Get(index)->GetContentLength();
}

OpFileLength MultimediaCacheFile::DebugGetSegmentEmptySpace(int index)
{
	return segments.Get(index)->empty_space;
}

OpFileLength MultimediaCacheFile::DebugGetSegmentContentStart(int index)
{
	return segments.Get(index)->GetContentStart();
}


#endif // SELFTEST

OP_STATUS MultimediaCacheFile::WriteContentDirect(OpFileLength file_position, const void *buf, UINT32 size, OpFileLength usable_empty_space, UINT32 &written_bytes, BOOL last_segment)
{
	written_bytes=0;
	
	if(max_size)
	{
		OP_ASSERT(!usable_empty_space || streaming);

		if(cached_size>=max_size && !usable_empty_space) // It's ok... if cached_size>max_size, size is supposed to have been reduced too late...
			return OpStatus::ERR_OUT_OF_RANGE;
			//OpStatus::OK;   
		
		if(usable_empty_space)
		{
			UINT32 size_available;

			if(last_segment)
				size_available=(UINT32)((max_size-cached_size) + usable_empty_space);
			else
				size_available=(UINT32)usable_empty_space;

			if(size>size_available)
				size=size_available;
		}
		else if(cached_size+size>max_size)
			size=(UINT32)(max_size-cached_size);
			
		OP_ASSERT(size>0);
	}
	
	RETURN_IF_ERROR(sfrw.SetWriteFilePos(file_position));
	RETURN_IF_ERROR(sfrw.WriteBuf(buf, size));
	
	written_bytes=size;
	
	return OpStatus::OK;
}

void MultimediaSegment::AddContent(OpFileLength size, OpFileLength &extension_bytes)
{
	content_len+=size;
	
	if(size>empty_space)
	{
		extension_bytes=size-empty_space;
		empty_space=0;
	}
	else
	{
		extension_bytes=0;
		empty_space-=size;
	}
} 

BOOL MultimediaSegment::ContainsContentBeginning(OpFileLength start, OpFileLength &bytes_available, OpFileLength &file_pos)
{
	if(start>=GetContentStart() && start<GetContentEnd())
	{
		bytes_available=GetContentEnd()-start;
		file_pos=GetFileOffset()+start-GetContentStart();
		OP_ASSERT(file_pos+bytes_available==GetFileOffset()+GetContentLength());
		
		return TRUE;
	}
	
	return FALSE;
}

BOOL MultimediaSegment::ContainsPartialContent(OpFileLength requested_start, OpFileLength requested_len, OpFileLength &available_start, OpFileLength &available_len, OpFileLength &file_pos)
{
	OpFileLength requested_end;

	if(requested_len==FILE_LENGTH_NONE)
	{
		if(requested_start>=GetContentEnd())
			return FALSE;

		requested_end=GetContentEnd();
	}
	else
		requested_end=requested_start+requested_len;
	
	if( (requested_start   >= GetContentStart() && requested_start   < GetContentEnd()) || // Requested start included in the segment
		(requested_end-1   >= GetContentStart() && requested_end-1   < GetContentEnd()) || // Requested end included in the segment
		(GetContentStart() >= requested_start   && GetContentStart() < requested_end  ) || // Segment start included in the request
		(GetContentEnd()-1 >= requested_start   && GetContentEnd()-1 < requested_end  ))   // Segment end included in the request
	{
		OpFileLength start=(requested_start<GetContentStart())?GetContentStart():requested_start;
		OpFileLength end=(requested_end<GetContentEnd())?requested_end:GetContentEnd();
		
		OP_ASSERT(end>=start);
		OP_ASSERT(start>=GetContentStart());
		OP_ASSERT(end<=GetContentEnd());
		OP_ASSERT(end>=start);
		
		available_start=start;
		available_len=end-start;
		file_pos=file_offset+start-GetContentStart();
		
		return TRUE;
	}
	
	return FALSE;
}

BOOL MultimediaSegment::CanAppendContent(OpFileLength start, OpFileLength &file_pos)
{
	if(start==GetContentEnd())
	{
		file_pos=GetFileOffset()+GetContentLength();
		
		return TRUE;
	}
	
	return FALSE;
}
	
OP_STATUS MultimediaCacheFile::GetWriteSegmentByContentPosition(MultimediaSegment *&segment, OpFileLength content_position, OpFileLength &file_pos, BOOL update_disk)
{
	OP_ASSERT(content_position != FILE_LENGTH_NONE);
	
	/* The write can happen:
	 * - on the last segment (preferred, also to let the file grow)
	 * - on a segment with empty space (if streaming is allowed)
	 * - on a new segment */
	
	// Check the last segment
	if(GetSegmentsInUse()>0 && 
	  (max_size==0 || cached_size<max_size))  // This segment cannot have priority if the disk is full
	{
		UINT16 index=GetSegmentsInUse()-1;
		MultimediaSegment *seg=segments.Get(index);
		
		OP_ASSERT(seg);
		
		if(seg)
		{
			// Try to append
			if(seg->CanAppendContent(content_position, file_pos))
			{
				OP_ASSERT(segments.Get(index)==seg);
				
				if(!seg->IsNew()) // Mark the segment as new
				{
					seg->SetNew(TRUE);
					
					if(update_disk)
						RETURN_IF_ERROR(seg->UpdateDisk(header_flags, sfrw, GetSegmentPos(index)));
				}
				
				segment=seg;
				
				return OpStatus::OK;
			}
			else if(seg->IsNew())  // If the segment is new, close it!
			{
				seg->SetNew(FALSE);
					
				if(update_disk)
					RETURN_IF_ERROR(seg->UpdateDisk(header_flags, sfrw, GetSegmentPos(index)));
			}
		}
	}

	BOOL could_append_but_full=FALSE;

	// Check if it can be "streamed" on a segment with empty space
	// Priority is given to the master segment (with upper index), not to the reserve, even if this should no longer be a requirement
	if(streaming)
	{
		for(int i=GetSegmentsInUse(); i>0; i--)
		{
			MultimediaSegment *seg=segments.Get(i-1);
			
			OP_ASSERT(seg);
			
			if(seg && seg->CanAppendContent(content_position, file_pos))
			{
				if(seg->empty_space) // Check if the segment can contain at least one more byte
				{
					segment=seg;
					
					return OpStatus::OK;
				}
				else
					could_append_but_full=TRUE; // Segment Full. Keep searching...
			}
		}
	}

	// If it can be contained in a segment already written (also the last one!), return an error! Overwrite is not supported.
	for(int i=GetSegmentsInUse(); i>0; i--)
	{
		MultimediaSegment *seg=segments.Get(i-1);
		OpFileLength bytes_available;
		OpFileLength pos;
		
		OP_ASSERT(seg);
		
		if(seg && seg->ContainsContentBeginning(content_position, bytes_available, pos))
			return OpStatus::ERR_NOT_SUPPORTED;
	}
	
	/* New segment required */
	
	// !!! WARNING: this condition is met when there is a seek on write, while streaming ==> Drop everything!!!
	// This has to be performed because while streaming all the size is pre allocated, and it is not possible to create new segments to
	// contain a write in a different position than expected
	if(streaming && auto_delete_on_streaming && !could_append_but_full && GetSegmentsInUse())
		RETURN_IF_ERROR(DeleteContent());

	// Check if we are over the maximum number of segments
	if(GetSegmentsInUse()>=max_segments)
		return OpStatus::ERR_NOT_SUPPORTED;

	// Prevent the useless creation of a new segment, also because the write will fail triggering a bug
	if(max_size>0 && cached_size>=max_size)
		return OpStatus::ERR_OUT_OF_RANGE;
	
	MultimediaSegment *seg=OP_NEW(MultimediaSegment, ());
	OpFileLength len;
		
	if(!seg)
		return OpStatus::ERR_NO_MEMORY;
		
	OP_STATUS ops;
	
	if( OpStatus::IsError(ops=sfrw.GetFileLength(len)) || 
		OpStatus::IsError(ops=seg->SetSegment(len, content_position, 0, MMCACHE_SEGMENT_NEW)) || 
		OpStatus::IsError(ops=segments.Add(seg)))
	{
		OP_DELETE(seg);
		
		return ops;
	}
	
	UINT16 index=GetSegmentsInUse()-1;
	OP_ASSERT(segments.Get(index)==seg);

	// In case of streaming, the segment is created as using all the space available, which simplify several problems
	if(!index && streaming && max_size>0)
	{
		OP_ASSERT(cached_size==0);

		seg->content_len=0;
		seg->empty_space=max_size;
		seg->SetDirty(TRUE);
		cached_size=max_size;
	}
	
	if(update_disk)
		RETURN_IF_ERROR(seg->UpdateDisk(header_flags, sfrw, GetSegmentPos(index)));
		
	segment=seg;
	file_pos=segment->GetFileOffset();
		
	return OpStatus::OK;
}

OP_STATUS MultimediaCacheFile::RetrieveFromEmptySpace(OpFileLength start, OpFileLength &bytes_available, OpFileLength &file_pos)
{
	OP_ASSERT(enable_empty_space_recover);

	if(!enable_empty_space_recover)
		return OpStatus::ERR_NOT_SUPPORTED;
	// Look for the segment that, in its reserve, can contain this range
	// Please note that we assume that the segment itself cannot contains it
	// We can visualize the two segments (master and reserve) in this way:
	//
	// || Reserve Segment               || Master Segment     ||
	// || More Data  | Empty Space      || Data               ||
	//
	// Example: 256 bytes stream cache, downloaded from 0 to 272, 64 byte read, 16 "lost", 48 still in Empty Space
	// || 256... 272 | 16... 63         || 64..255           ||
	//
	// So we are looking to see if start is incuded in the empty space
	// It has to be understood that the Empty Space contains the Data that have been consumed by the, master segments, but that are
	// still available on the disk. So they logically start before the beginning of the master segment
	for(UINT32 i=GetSegmentsInUse(); i>0; i--)
	{
		MultimediaSegment *seg=segments.Get(i-1);
		
		OP_ASSERT(seg && !seg->ContainsContentBeginning(start, bytes_available, file_pos));

		if( seg && seg->EmptySpaceContains(start))
		{
			bytes_available=seg->content_start-start;  // The beginning of the master is the end of the "virtual empty segment"
			file_pos=seg->file_offset-bytes_available;

			// Check that it is inside the reserve segment (in the empty spcae) and before the master segment
			OP_ASSERT(file_pos>=seg->reserve_segment->file_offset+seg->reserve_segment->content_len && 
				      file_pos<seg->file_offset &&
					  file_pos<seg->reserve_segment->file_offset+seg->reserve_segment->content_len+seg->reserve_segment->empty_space);
			
			return OpStatus::OK;
		}
	}

	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

BOOL MultimediaSegment::EmptySpaceContains(OpFileLength content_position)
{
	return reserve_segment && 
		   content_start>content_position && 
		   content_start - reserve_segment->empty_space <= content_position &&
		   reserve_segment->empty_space>0;
}

OP_STATUS MultimediaCacheFile::ReadContent(OpFileLength content_position, void *buf, UINT32 size, UINT32 &read_bytes)
{
	CheckInvariants();
	
	if(!sfrw.IsFileValid())
		return OpStatus::ERR_NOT_SUPPORTED;
		
	MultimediaSegment *found_seg=NULL;
	OpFileLength bytes_available=0;
	OpFileLength file_pos=0;
	UINT32 seg_index=0;
	
	read_bytes=0;
	
	// Look for the segment that contains the position
	for(UINT32 i=GetSegmentsInUse(); i>0 && !found_seg; i--)
	{
		MultimediaSegment *seg=segments.Get(i-1);
		
		OP_ASSERT(seg);
		
		if(seg && seg->ContainsContentBeginning(content_position, bytes_available, file_pos))
		{
			found_seg=seg;
			seg_index=i-1;
		}
	}
	
	if(!found_seg)
	{
		if(streaming && enable_empty_space_recover) // Check if the content is stilla vailable in the reserve segment
			RETURN_IF_ERROR(RetrieveFromEmptySpace(content_position, bytes_available, file_pos));
		else
			return OpStatus::ERR_NO_SUCH_RESOURCE;
	}
		
	OpFileLength bytes_to_read=(bytes_available<size)?bytes_available:size;
	
	RETURN_IF_ERROR(sfrw.SetReadFilePos(file_pos));
	RETURN_IF_ERROR(sfrw.ReadBuf(buf, (UINT32) bytes_to_read));
	
	OP_ASSERT(bytes_to_read<UINT_MAX);
	
	read_bytes=(UINT32)bytes_to_read;

	// If the bytes are retrieved from the empty space, there is no need to consume them, as at the moment this would require a 
	// second call to retrieve the bytes in the master segment
	// found_seg then it's NULL, and it's fine
	if(streaming && found_seg && consume_policy==CONSUME_ON_READ)
	{
		OpFileLength skip=content_position-found_seg->content_start; // Bytes to skip

		OP_ASSERT(found_seg->content_start<=content_position);
		OP_ASSERT(skip==0 || (skip>0 && skip<=found_seg->content_len));
		OP_ASSERT(found_seg->content_len>=read_bytes+skip);

		// If a seek read is detected (skip>0), the bytes before the new position are dropped.
		// This is unavoidable, else we would need to create a new segment to contain the bytes that have been
		// skipped but not consumed. This potentially waste bandwidth, but it also solves many problems.
		// Otherwise, ConsumeBytes() should be somewhat exposed to URL, and called explicitly by the media module

		// The problem is that if the skip affect the reserve segment, we also need to skip all the bytes in the master,
		// which will trigger a reorganization...
		// For semplicity, we apply a shortcut only valid for 2 indexes

		OP_ASSERT(!deep_streaming_check || GetSegmentsInUse()<=2);

		if(!found_seg->reserve_segment && GetSegmentsInUse()==2) // Check if this segment is the reserve
		{
			OP_ASSERT(seg_index==0 || seg_index==1);

			int master_seg_index=1-seg_index;
			MultimediaSegment *master_seg=segments.Get(master_seg_index);

			OP_ASSERT(master_seg && master_seg!=found_seg);
			OP_ASSERT(master_seg->reserve_segment == found_seg);
			OP_ASSERT(master_seg->content_start < found_seg->content_start && master_seg->content_start+master_seg->content_len < content_position);
			RETURN_IF_ERROR(ConsumeBytes(master_seg_index, master_seg->content_start, (UINT32)(master_seg->content_len)));

			OP_ASSERT(!deep_streaming_check || GetSegmentsInUse()<=2);

			// Shortcut: consuming bytes will promote the reserve to master
			OP_ASSERT(master_seg->reserve_segment == found_seg);
			OP_ASSERT(!found_seg->content_len && !found_seg->empty_space);
			OP_ASSERT(master_seg->content_start < content_position && master_seg->content_start+master_seg->content_len>=content_position);
			OP_ASSERT(segments.Get(master_seg_index)==master_seg);

			RETURN_IF_ERROR(ConsumeBytes(master_seg_index, master_seg->content_start, (UINT32)(content_position-master_seg->content_start)));

			OP_ASSERT(!deep_streaming_check || GetSegmentsInUse()<=2);
		}
		else // This segment is the master
			RETURN_IF_ERROR(ConsumeBytes(seg_index, found_seg->content_start, (UINT32)(read_bytes+skip)));
	}
	
	CheckInvariants(); 
	
	return OpStatus::OK;
}

OP_STATUS MultimediaCacheFile::ConsumeBytes(int seg_index, OpFileLength content_position, UINT32 bytes_to_consume)
{
	MultimediaSegment *seg=segments.Get(seg_index);
	OP_ASSERT(seg);

	if(!seg)
		return OpStatus::ERR_NULL_POINTER;

	OP_ASSERT(content_position>=seg->content_start && content_position+bytes_to_consume<=seg->content_start+seg->content_len);

	// Mark the segment as dirty, updating also the status on the disk
	if(!seg->IsDirty())
	{
		seg->SetDirty(TRUE);

		RETURN_IF_ERROR(seg->UpdateDisk(header_flags, sfrw, GetSegmentPos(seg_index)));
		seg->SetNew(FALSE);
	}

	OP_ASSERT(seg->IsDirty());

	// Check if there is already a reserve segment
	if(!seg->reserve_segment)
	{
		seg->reserve_segment=OP_NEW(MultimediaSegment, ());
		
		if(!seg->reserve_segment)
			return OpStatus::ERR_NO_MEMORY;
			
		OP_STATUS ops;
		
		// The reserve segment point at the beginning of the segment itself, and it's empty
		if( OpStatus::IsError(ops=seg->reserve_segment->SetSegment(seg->file_offset, seg->content_start+seg->content_len+seg->empty_space, 0, MMCACHE_SEGMENT_DIRTY)) || 
			OpStatus::IsError(ops=segments.Add(seg->reserve_segment)))
		{
			OP_DELETE(seg->reserve_segment);
			
			return ops;
		}

		// Shortcut only valid because we force the use of two segments
		int reserve_seg_index=segments.GetCount()-1;

		// Checks that the shortcut is valid
		OP_ASSERT(segments.Get(reserve_seg_index)==seg->reserve_segment);

		// Swap the segments, to have the master after the reserve (even if this should no longer be a concern)
		segments.Replace(seg_index, seg->reserve_segment);
		segments.Replace(reserve_seg_index, seg);

		// Swap the indexes
		int t_index=seg_index;

		seg_index=reserve_seg_index;
		reserve_seg_index=t_index;
		
		OP_ASSERT(seg_index>reserve_seg_index);

		// Update the situation on the disk
		RETURN_IF_ERROR(seg->reserve_segment->UpdateDisk(header_flags, sfrw, GetSegmentPos(reserve_seg_index)));
		RETURN_IF_ERROR(seg->UpdateDisk(header_flags, sfrw, GetSegmentPos(seg_index)));

		CheckInvariants();
	}

	OpFileLength dumped_bytes=0;

	RETURN_IF_ERROR(seg->ConsumeBytes(content_position, bytes_to_consume, dumped_bytes));

	CheckInvariants();

	OP_ASSERT(dumped_bytes<=cached_size);

	// If the master segment is empty, the reserve is promoted to master
	if(!seg->content_len)
	{
	#ifdef DEBUG_ENABLE_OPASSERT
		OpFileLength aggregated_space=seg->content_len+seg->empty_space+seg->reserve_segment->content_len+seg->reserve_segment->empty_space;
	#endif

		// Fill the master segment with the content of the reserve
		// WARNING: it is supposed to work even if the Reserve segment has a reserve segment by itself

		//seg->content_start=seg->reserve_segment->content_start;
		seg->content_len=seg->reserve_segment->content_len;
		seg->file_offset=seg->reserve_segment->file_offset;
		seg->empty_space+=seg->reserve_segment->empty_space;  // Preserve the empty space
		// Reset the reserve segment, so it is like when it has been created
		seg->reserve_segment->content_start=seg->content_start+seg->content_len+seg->empty_space; // The reserve points to the complete end of the segment, empty space included
		seg->reserve_segment->content_len=0;
		seg->reserve_segment->empty_space=0;

	#ifdef DEBUG_ENABLE_OPASSERT
		OpFileLength aggregated_space2=seg->content_len+seg->empty_space+seg->reserve_segment->content_len+seg->reserve_segment->empty_space;

		OP_ASSERT(aggregated_space2 == aggregated_space);
		OP_ASSERT(aggregated_space2 == seg->content_len+seg->empty_space);
	#endif

		seg->CheckInvariants();
		seg->reserve_segment->CheckInvariants();
	}

	CheckInvariants();

	return OpStatus::OK;
}

int MultimediaCacheFile::FindSegmentForAutoConsume()
{
	MultimediaSegment *found_seg=NULL;
	int found_seg_index=-1;

	for(UINT32 i=0, num=GetSegmentsInUse(); i<num; i++)
	{
		MultimediaSegment *seg=segments.Get(i);

		if(seg && seg->content_len>0 && (!found_seg || seg->content_start < found_seg->content_start))
		{
			found_seg=seg;
			found_seg_index=i;
		}
	}

	return found_seg_index;
}

OP_STATUS MultimediaCacheFile::AutoConsume(UINT32 bytes_to_consume, UINT32 &bytes_consumed)
{
	bytes_consumed=0;

	OP_ASSERT(streaming);

	if(!streaming)
		return OpStatus::ERR_NOT_SUPPORTED;

	do
	{
		int seg_index=FindSegmentForAutoConsume();

		if(seg_index<0)
			break;

		MultimediaSegment *seg=segments.Get(seg_index);
		UINT32 bytes_to_consume_now=(UINT32)((seg->content_len>bytes_to_consume-bytes_consumed)?bytes_to_consume-bytes_consumed:seg->content_len);

		RETURN_IF_ERROR(ConsumeBytes(seg_index, seg->content_start, bytes_to_consume_now));

		bytes_consumed+=bytes_to_consume_now;
	}
	while(bytes_consumed<bytes_to_consume);

	return OpStatus::OK;
}

OP_STATUS MultimediaCacheFile::GetUnsortedCoverage(OpAutoVector<MultimediaSegment> &out_segments, OpFileLength start, OpFileLength len)
{
	OP_ASSERT(start != FILE_LENGTH_NONE);
	OP_ASSERT(len>0 || len==FILE_LENGTH_NONE);
	OP_ASSERT(out_segments.GetCount()==0);
	
	if(start==FILE_LENGTH_NONE)
		return OpStatus::ERR_OUT_OF_RANGE;
	
	// Look for the segments in the range requested
	for(UINT32 i=0, num=GetSegmentsInUse(); i<num; i++)
	{
		MultimediaSegment *seg=segments.Get(i);
		OpFileLength available_start=0;
		OpFileLength available_len=0;
		OpFileLength file_pos=0;
		
		OP_ASSERT(seg);
		
		if(seg && !seg->HasToBeDiscarded() && seg->ContainsPartialContent(start, len, available_start, available_len, file_pos))
		{
			MultimediaSegment *new_seg=OP_NEW(MultimediaSegment, (file_pos, available_start, available_len));
			
			if(!new_seg)
				return OpStatus::ERR_NO_MEMORY;
				
			OP_STATUS ops=out_segments.Add(new_seg);
			
			if(OpStatus::IsError(ops))
			{
				OP_DELETE(new_seg);
				
				return ops;
			}
		}
	}
	
	CheckInvariants();
	
	return OpStatus::OK;
}

OP_STATUS MultimediaCacheFile::GetSortedCoverage(OpAutoVector<MultimediaSegment> &segments, OpFileLength start, OpFileLength len, BOOL merge)
{
	RETURN_IF_ERROR(GetUnsortedCoverage(segments, start, len)); // This call also check the parameters
	
	// Shell sort on GetContentStart()
	UINT32 num=segments.GetCount();
	UINT32 inc=(num+1)/2;

	while(inc > 0)
	{
		for(UINT32 i = inc; i<num; i++)
		{
			MultimediaSegment *temp=segments.Get(i);
			MultimediaSegment *cmp_seg=NULL;
			OpFileLength cur_start=(temp)?temp->GetContentStart():0;
			UINT32 j=i;
			
			OP_ASSERT(temp);
			
			while(j >= inc && (cmp_seg=segments.Get(j - inc))!= NULL && cmp_seg->GetContentStart() > cur_start)
			{
				segments.Replace(j, cmp_seg);
				j = j - inc;
			}
			OP_ASSERT(cmp_seg);
			
			segments.Replace(j, temp);
		}
		inc = (UINT32 ) ((inc+1) / 2.2);
	}
	
	// Merge
	if(merge)
	{
		for(UINT32 i=num; i>1; i--)
		{
			MultimediaSegment *cur=segments.Get(i-1);
			MultimediaSegment *prev=segments.Get(i-2);
			
			OP_ASSERT(cur && prev && cur->GetContentStart()>prev->GetContentStart());
			
			if(cur && prev && cur->GetContentStart()==prev->GetContentEnd())
			{
				prev->SetLength(prev->GetContentLength() + cur->GetContentLength());
				segments.Remove(i-1);
				OP_DELETE(cur);
			}
		}
	}
	
	CheckInvariants();
	
	return OpStatus::OK;
}

OP_STATUS MultimediaCacheFile::GetMissingCoverage(OpAutoVector<StorageSegment> &missing, OpFileLength start, OpFileLength len)
{
	OpAutoVector<MultimediaSegment> covered;  // Segment available

	// This call also check the parameters
	// Merge or no merge is the same (from a functional POV), as the segments are sorted.
	// But I expect no merge to be slightly faster
	RETURN_IF_ERROR(GetSortedCoverage(covered, start, len, FALSE));

	OpFileLength cur=start;
	OpFileLength end=(len==FILE_LENGTH_NONE)?FILE_LENGTH_NONE:start+len;	

	// Go through the ordered segments, marking what is missing
	for(int i=0, segs=covered.GetCount(); i<segs && (cur<end ||len==FILE_LENGTH_NONE); i++)
	{
		OP_ASSERT(covered.Get(i));

		if(covered.Get(i))
		{
			if(covered.Get(i)->GetContentStart()<=cur)
			{
				OP_ASSERT(covered.Get(i)->GetContentStart()==cur || i==0);  // Only the first segment could start before the range
				OP_ASSERT(covered.Get(i)->GetContentEnd()>cur);  // The real end (the last byte is excluded) cannot be before the segment

				// Even if the assert fails, this is not causing problems to the method, but it means problems in other parts

				if(covered.Get(i)->GetContentEnd()>cur)  // Just to be on the safe side...
					cur=covered.Get(i)->GetContentEnd();
			}
			else
			{
				OP_ASSERT(covered.Get(i)->GetContentEnd()<end || len==FILE_LENGTH_NONE); // Granted by GetSortedCoverage()

				StorageSegment *seg=OP_NEW(StorageSegment, ());

				if(!seg)
					return OpStatus::ERR_NO_MEMORY;

				RETURN_IF_ERROR(missing.Add(seg));

				OP_ASSERT(covered.Get(i)->GetContentStart()>cur);  // No other cases are possible

				seg->content_start=cur;
				seg->content_length=covered.Get(i)->GetContentStart()-cur;
				cur=covered.Get(i)->GetContentEnd();

				OP_ASSERT(cur<end || i==segs-1 || len==FILE_LENGTH_NONE);
			}
		}
	} // end for

	// Add the last segment, if it has not been covered
	if(cur<end && len!=FILE_LENGTH_NONE)
	{
		StorageSegment *seg=OP_NEW(StorageSegment, ());

		if(!seg)
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(missing.Add(seg));

		seg->content_start=cur;
		seg->content_length=end-cur; 
	}

	CheckInvariants();
	
	return OpStatus::OK;
}

void MultimediaCacheFile::GetPartialCoverage(OpFileLength position, BOOL &available, OpFileLength &length, BOOL multiple_segments)
{
	OP_ASSERT(position != FILE_LENGTH_NONE);
	
	length=0;
	available = FALSE;
	
	if(position==FILE_LENGTH_NONE)
		return;
		
	MultimediaSegment *closest_seg=NULL;
	int num_seg=0;
	
	do
	{
		available=FALSE;

		// Look for the segment that contain the position
		for(UINT32 i=0, num=GetSegmentsInUse(); i<num; i++)
		{
			MultimediaSegment *seg=segments.Get(i);

			if(seg && !seg->HasToBeDiscarded())
			{
				// Check if it is the right segment
				if(seg->GetContentStart()<=position && seg->GetContentEnd()>position)
				{
					available=TRUE;
					length+=seg->GetContentEnd()-position;

					if(!multiple_segments)
						return;

					position=seg->GetContentEnd(); // Look for the next segment
					num_seg++;
					break;
				}
				else if(enable_empty_space_recover && seg->EmptySpaceContains(position)) // Check empty space in reserve segment
				{
					OP_ASSERT(seg->GetContentStart()>position);
					available=TRUE;
					length+=seg->GetContentStart()-position;

					if(!multiple_segments)
						return;

					position=seg->GetContentStart(); // Look for the next segment
					num_seg++;
					break;
				}
				
				// Check if it is the closest segment (after the position)
				if(seg->GetContentStart()>position &&
				    seg->GetContentLength()>0 &&
					(!closest_seg || seg->GetContentStart()<closest_seg->GetContentStart()))
				   closest_seg=seg;
			}
		}
	}
	while(available);
	
	available=num_seg>0;

	if(!available && closest_seg)
	{
		length=closest_seg->GetContentStart()-position;

		// Recover bytes from empty spacec
		if(enable_empty_space_recover && closest_seg->reserve_segment)
		{
			OP_ASSERT(closest_seg->reserve_segment->empty_space<length);  // Else it should have been marked as available

			if(closest_seg->reserve_segment->empty_space<length)
				length-=closest_seg->reserve_segment->empty_space;
		}
	}
}

#ifdef SELFTEST
int MultimediaCacheFile::DebugGetNewSegments()
{
	int num_new_segments=0;

	for(UINT32 i=0; i<GetSegmentsInUse(); i++)
	{
		MultimediaSegment *seg=segments.Get(i);
		
		if(seg && seg->IsNew())
			num_new_segments++;
	}
	
	return num_new_segments;
}

int MultimediaCacheFile::DebugGetDirtySegments()
{
	int num_dirty_segments=0;

	for(UINT32 i=0; i<GetSegmentsInUse(); i++)
	{
		MultimediaSegment *seg=segments.Get(i);
		
		if(seg && seg->IsDirty())
			num_dirty_segments++;
	}
	
	return num_dirty_segments;
}
#endif // SELFTEST

OP_STATUS MultimediaSegment::UpdateDisk(UINT8 header_flags, SimpleFileReadWrite &sfrw, UINT32 header_pos)
{
	RETURN_IF_ERROR(sfrw.SetWriteFilePos(header_pos));
	
	return MultimediaSegment::DirectWriteHeader(sfrw, header_flags, content_start, content_len, flags);
}

OP_STATUS MultimediaSegment::DirectWriteHeader(SimpleFileReadWrite &sfrw, UINT8 header_flags, OpFileLength start, OpFileLength len, UINT8 segment_flags)
{
	if(header_flags & MMCACHE_HEADER_64_BITS)
	{
		RETURN_IF_ERROR(sfrw.Write64(start));
		RETURN_IF_ERROR(sfrw.Write64(len));
	}
	else
	{
		RETURN_IF_ERROR(sfrw.Write32((UINT32)start));
		RETURN_IF_ERROR(sfrw.Write32((UINT32)len));
	}
	RETURN_IF_ERROR(sfrw.Write8(segment_flags));
	
	return OpStatus::OK;
}

OP_STATUS MultimediaSegment::SetSegment(OpFileLength file_start, OpFileLength segment_start, OpFileLength segment_len, UINT8 segment_flags)
{
	content_start=segment_start;
	content_len=segment_len;
	file_offset=file_start;
	flags=segment_flags;
	CheckInvariants();
	
	// Some controls
	if(content_start == FILE_LENGTH_NONE)
	{
		content_start=0;
		flags=MMCACHE_SEGMENT_DIRTY;  // Disable segment
	}
	
	if(file_offset == FILE_LENGTH_NONE)
	{
		file_offset=0;
		flags=MMCACHE_SEGMENT_DIRTY;  // Disable segment, but in this case it is not enough

		return OpStatus::ERR_OUT_OF_RANGE;
	}
	
	if(content_len == FILE_LENGTH_NONE)
	{
		content_len=0;
		flags=MMCACHE_SEGMENT_DIRTY;  // Disable segment, but in this case it is not enough

		return OpStatus::ERR_OUT_OF_RANGE;
	}
	
	OP_ASSERT(content_len>0 || (segment_flags & MMCACHE_SEGMENT_NEW) || (segment_flags & MMCACHE_SEGMENT_DIRTY) );
	
	return OpStatus::OK;	
}


OP_STATUS MultimediaSegment::ConsumeBytes(OpFileLength content_position, UINT32 bytes_to_consume, OpFileLength &dumped_bytes)
{
	OP_ASSERT(reserve_segment);
	OP_ASSERT(content_position>=content_start);
	OP_ASSERT(content_position<content_start+content_len);

	if(!reserve_segment)
		return OpStatus::ERR_NULL_POINTER;
	if(content_position<content_start || content_position>=content_start+content_len)
		return OpStatus::ERR_OUT_OF_RANGE;

	CheckInvariants(); // Check that the reserve segment is coherent

	// WARNING: if a read occurs in the middle of the segment, all the previous content is discarded

	// Dump the bytes from the current segment
	dumped_bytes=content_position-content_start+bytes_to_consume;
	content_start=content_position+dumped_bytes;
	content_len-=dumped_bytes;
	file_offset+=dumped_bytes;

	// Add empty space to the reserve segment
	reserve_segment->empty_space+=dumped_bytes;

	CheckInvariants(); // Check that the reserve segment is coherent

	return OpStatus::OK;
}

#ifdef DEBUG_ENABLE_OPASSERT
void MultimediaSegment::CheckInvariants()
{
		OP_ASSERT(content_start!=FILE_LENGTH_NONE && content_len!=FILE_LENGTH_NONE && file_offset>=MMCACHE_HEADER_SIZE && ( (flags&3) == flags));

		if(reserve_segment)
		{
			OP_ASSERT(reserve_segment->file_offset+reserve_segment->content_len+reserve_segment->empty_space==file_offset);
			OP_ASSERT(reserve_segment->content_start==content_start+content_len+empty_space);
		}

		if(HasToBeDiscarded())
			OP_ASSERT(IsDirty());
}
#endif // DEBUG_ENABLE_OPASSERT


BOOL MultimediaCacheFileDescriptor::Eof() const
{
	OP_ASSERT(mcf);
	
	if(!mcf)
		return TRUE;
		
	OpFileLength start;
	OpFileLength end;
	
	mcf->GetOptimisticFullRange(start, end);
	
	return read_pos>=end;
}


OP_STATUS MultimediaCacheFileDescriptor::Write(const void* data, OpFileLength len)
{
	OP_ASSERT(mcf);
	
	if(!mcf)
		return OpStatus::ERR_NULL_POINTER;
		
	OP_ASSERT(!read_only);
	
	if(read_only)
		return OpStatus::ERR_NOT_SUPPORTED;
		
	OP_ASSERT(len<=UINT_MAX);
	OP_ASSERT(write_pos!=FILE_LENGTH_NONE);
	
	UINT32 written_bytes;
	 
	RETURN_IF_ERROR(mcf->WriteContent(write_pos, data, (UINT32)len, written_bytes));
	
	OP_NEW_DBG("MultimediaCacheFileDescriptor::Write", "multimedia_cache.write");
	
	if(written_bytes!=len)
	{
		OP_DBG((UNI_L("*** Error Written %d bytes (instead of %d) on position %d"), (UINT32)written_bytes, (UINT32)len, (UINT32)write_pos));
		
		return OpStatus::ERR;
	}
		
	OP_DBG((UNI_L("*** Written %d bytes on position %d"), (UINT32)written_bytes, (UINT32)write_pos));
		
	write_pos+=len;
	
	return OpStatus::OK;
}

void MultimediaCacheFileDescriptor::SetReadPosition(OpFileLength pos)
{
	OP_NEW_DBG("MultimediaCacheFileDescriptor::SetReadPosition", "multimedia_cache.read");
	OP_DBG((UNI_L("*** Read position set to %d"), (UINT32)pos));
	
	read_pos=pos;
}

void MultimediaCacheFileDescriptor::SetWritePosition(OpFileLength pos)
{
	OP_NEW_DBG("MultimediaCacheFileDescriptor::SetWritePosition", "multimedia_cache.write");
	OP_DBG((UNI_L("*** Write position set to %d"), (UINT32)pos));
	
	write_pos=pos;
}

OP_STATUS MultimediaCacheFileDescriptor::Read(void* data, OpFileLength len, OpFileLength* bytes_read)
{
	if(!mcf)
		return OpStatus::ERR_NULL_POINTER;
		
	OP_ASSERT(len<=UINT_MAX);
	
	UINT32 bytes_read32=0;
	UINT32 bytes_temp=1;

	OP_NEW_DBG("MultimediaCacheFileDescriptor::Read", "multimedia_cache.read");

	if(bytes_read)
		*bytes_read=0;
	 
	// Read multiple times, because the segment can be finished but the content can continue in another one...
	while(bytes_read32<len && bytes_temp)
	{
		OP_STATUS ops=mcf->ReadContent(read_pos+bytes_read32, ((unsigned char *)data)+bytes_read32, (UINT32)len-bytes_read32, bytes_temp);
		
		if(OpStatus::IsError(ops) && !bytes_read32)  // No bytes with an error, it's usually a real error
		{
			if(ops==OpStatus::ERR_NO_SUCH_RESOURCE)
			{
				OP_DBG((UNI_L("*** Read (ok, but stopped for ERR_NO_SUCH_RESOURCE) %d bytes (instead of %d) from position %d"), (UINT32)bytes_read32, (UINT32)len, (UINT32)read_pos));

				return OpStatus::OK; // Allow request for content not cached
			}

			OP_DBG((UNI_L("*** Error read %d bytes (instead of %d) from position %d"), (UINT32)bytes_read32, (UINT32)len, (UINT32)read_pos));

			return ops;
		}
			
		bytes_read32+=bytes_temp;
	}
	
	OP_DBG((UNI_L("*** Read %d bytes from position %d"), (UINT32)bytes_read32, (UINT32)read_pos));

	if(bytes_read)
		*bytes_read=bytes_read32;
		
	read_pos+=len;
	
	return OpStatus::OK;
}

MultimediaCacheFileDescriptor *MultimediaCacheFile::CreateFileDescriptor(int mode)
{
	MultimediaCacheFileDescriptor *desc=OP_NEW(MultimediaCacheFileDescriptor, (this));
	
	if(!desc)
		return NULL;
		
	if(mode & OPFILE_APPEND)
	{
		MultimediaSegment *seg=GetLastSegment();
		
		if(seg)
			desc->SetWritePosition(seg->GetContentEnd());
	}
	else if(mode==OPFILE_READ)
		desc->SetReadOnly(TRUE);
	
	return desc;
}

OP_STATUS MultimediaCacheFileDescriptor::GetFileLength(OpFileLength& len) const
{
	len=0;
	
	if(!mcf)
		return OpStatus::ERR_NULL_POINTER;
	
	OP_STATUS ops=mcf->sfrw.GetFileLength(len);
	
	if(OpStatus::IsSuccess(ops))
	{
		len-=mcf->GetFullHeaderLength();
		
		OP_ASSERT(len != FILE_LENGTH_NONE);
		
		if(len == FILE_LENGTH_NONE)
		{
			len=0;
			
			return OpStatus::ERR;
		}
	}
	
	return ops;
}

#endif // MULTIMEDIA_CACHE_SUPPORT

