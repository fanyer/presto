/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#include "core/pch.h"

#if CACHE_CONTAINERS_ENTRIES>0
#include "modules/cache/cache_container.h"






int CacheContainer::GetEntryIndexByID(UINT8 id)
{
	//CheckInvariants();
	
	for(UINT32 i=0; i<num_entries; i++)
	{
		if(entries[i].GetID()==id)
			return i;
	}
	
	return -1;
}
	
BOOL CacheContainer::AddEntry(UINT32 size, UINT8 &id, void *host_pointer)
{
	//CheckInvariants();
	
	if(next_id>=256 || num_entries>=CACHE_CONTAINERS_ENTRIES || total_size+size>CACHE_CONTAINERS_CONTAINER_LIMIT)
		return FALSE;
		
	if(num_entries==0)
	{
		if(!host_pointer)
			domain_pointer=NULL;
		else
			domain_pointer=host_pointer;
	}
	else
	{
		// UIf the first entry is without domain, every entry could enter here
		if(domain_pointer!=host_pointer)
				return FALSE;
	}
		
	id=next_id++;
	entries[num_entries].SetRecord(id, NULL, size);
	
	total_size+=size;
	num_entries++;
	
	modified=TRUE;
	
	CheckInvariants();
	
	return TRUE;
}

BOOL CacheContainer::RemoveEntryByID(UINT8 id)
{
	//CheckInvariants();
	
	int index=GetEntryIndexByID(id);
	
	if(index<0)
		return FALSE;
		
	// Remove and compact the entries
	num_entries--;
	total_size-=entries[index].GetSize();
	
	entries[index].Reset();
	
	if(index<(int)num_entries)
	{
		ContainerEntry temp=entries[index];
		
		for(UINT32 i=index; i<num_entries; i++)
			entries[i]=entries[i+1];
		
		entries[num_entries]=temp;
	}
	
	modified=TRUE;
	
	CheckInvariants();
	
	return TRUE;
}

BOOL CacheContainer::UpdatePointerByID(UINT8 id, const unsigned char *new_ptr, UINT16 new_size)
{
	OP_ASSERT(new_ptr);
	
	//CheckInvariants();
	
	int index=GetEntryIndexByID(id);
	
	if(index<0)
		return FALSE;
		
	if(total_size-entries[index].GetSize()+new_size>CACHE_CONTAINERS_CONTAINER_LIMIT)
		return FALSE;
		
	total_size=total_size-entries[index].GetSize()+new_size;
		
	entries[index].SetPointer(new_ptr, new_size);
	
	modified=TRUE;
	
	CheckInvariants();
	
	return TRUE;
}

BOOL CacheContainer::GetEntryByID(UINT8 id, const unsigned char *&ptr, UINT16 &size)
{
	//CheckInvariants();
	
	for(UINT32 i=0; i<num_entries; i++)
	{
		if(entries[i].GetID()==id)
		{
			ptr=entries[i].GetPointer();
			size=entries[i].GetSize();
			
			return TRUE;
		}
	}
	
	return FALSE;
}

void CacheContainer::Reset()
{
	for(UINT32 i=0; i<num_entries; i++)
		entries[i].Reset();
		
	num_entries=0;
	next_id=1;
	total_size=0;
	domain_pointer=NULL;
	file_name.Empty();
	modified=FALSE;
}

OP_STATUS CacheContainer::WriteToStream(SimpleStreamWriter *writer)
{
	OP_ASSERT(writer);
	
	//CheckInvariants();
	
	if(!writer)
		return OpStatus::ERR_NULL_POINTER;
		
	OP_ASSERT(num_entries<256);
	UINT32 i;
		
	RETURN_IF_ERROR(writer->Write8(CACHE_CONTAINERS_BYTE_SIGN));
	RETURN_IF_ERROR(writer->Write8(num_entries));
	
	// header
	for(i=0; i<num_entries; i++)
	{
		RETURN_IF_ERROR(writer->Write8(entries[i].GetID()));
		RETURN_IF_ERROR(writer->Write16(entries[i].GetSize()));
	}
	
	// Data
	for(i=0; i<num_entries; i++)
		RETURN_IF_ERROR(writer->WriteBuf((void *)entries[i].GetPointer(), entries[i].GetSize()));
	
	writer->FlushBuffer(TRUE);
	
	CheckInvariants();
	
	return OpStatus::OK;
}

OP_STATUS CacheContainer::ReadFromStream(SimpleStreamReader *reader)
{
	OP_ASSERT(reader);
	OP_ASSERT(!modified);
	
	//CheckInvariants();
	
	if(!reader)
		return OpStatus::ERR_NULL_POINTER;
		
	OP_ASSERT(num_entries==0 && total_size==0);
	UINT8 sign;
	
	if(num_entries>0)
		return OpStatus::ERR;
		
	sign=reader->Read8();
	
	if(sign!=CACHE_CONTAINERS_BYTE_SIGN)
		return OpStatus::ERR;
		
	UINT32 expected_entries=reader->Read8();
	UINT32 i;
	
	if(expected_entries>CACHE_CONTAINERS_ENTRIES)
		return OpStatus::ERR;
		
	total_size=0;
	next_id=0;
	num_entries=0;
	
	UINT8 entries_id[CACHE_CONTAINERS_ENTRIES];
	UINT16 entries_size[CACHE_CONTAINERS_ENTRIES];
	
	// header
	for(i=0; i<expected_entries; i++)
	{
		entries_id[i]=(UINT8)reader->Read8();
		entries_size[i]=(UINT16)reader->Read16();
	}
	
	// Data
	for(i=0; i<expected_entries; i++)
	{
		if(entries_id[i] && entries_size[i])
		{
			unsigned char *ptr=OP_NEWA(unsigned char, entries_size[i]);
			
			if(!ptr)
				return OpStatus::ERR_NO_MEMORY;
			
			OP_STATUS ops=reader->ReadBuf((void *)ptr, entries_size[i]);
			
			if(OpStatus::IsError(ops))
			{
				OP_DELETEA(ptr);
				
				return ops;
			}
			
			//ExternalPointer extp(ptr, TRUE, TRUE);
			
			entries[i].SetRecord(entries_id[i], ptr, entries_size[i]);
			
			total_size+=entries_size[i];
			num_entries++;
			if(entries_id[i]>=next_id)
				next_id=entries_id[i]+1;
		}
	}
	
	modified=FALSE; // Modified means that the content in memory is different than the disk, so it is not modified
	
	CheckInvariants();
	
	return OpStatus::OK;
}

#endif // CACHE_CONTAINERS_ENTRIES
