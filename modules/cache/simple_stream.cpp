/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Luca Venturi
**
*/
#include "core/pch.h"

#if defined(CACHE_FAST_INDEX) || CACHE_CONTAINERS_ENTRIES>0

#include "modules/cache/simple_stream.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opmemfile.h"
#include "modules/util/opfile/opsafefile.h"
#include "modules/datastream/fl_base.h"




#define _STD_READ_CHECK(dim, def)					\
	if(available-read<dim) RefillBuffer();			\
	if(available<dim)	return def;					\

UINT64 SimpleStreamReader::Read64(UINT64 def)
{
	_STD_READ_CHECK(8, def);
		
	UINT64 u=(((UINT64)buf[read])<<56) | (((UINT64)buf[read+1])<<48) | (((UINT64)buf[read+2])<<40) | ((UINT64)buf[read+3]<<32) |
	(((UINT64)buf[read+4])<<24) | (((UINT64)buf[read+5])<<16) | (((UINT64)buf[read+6])<<8) | ((UINT64)buf[read+7]);
	
	IncRead(8);
	
	return u;
}


UINT32 SimpleStreamReader::Read32(UINT32 def)
{
	_STD_READ_CHECK(4, def);
		
	UINT32 u=(((UINT32)buf[read])<<24) | (((UINT32)buf[read+1])<<16) | (((UINT32)buf[read+2])<<8) | ((UINT32)buf[read+3]);
	
	IncRead(4);
	
	return u;
}

UINT32 SimpleStreamReader::Read16(UINT32 def)
{
	_STD_READ_CHECK(2, def);
		
	UINT32 u=(((UINT32)buf[read])<<8) | ((UINT32)buf[read+1]);
	
	IncRead(2);
	
	return u;
}

UINT32 SimpleStreamReader::Read8(UINT32 def)
{
	_STD_READ_CHECK(1, def);
		
	UINT32 u=((UINT32)buf[read]);
	
	IncRead(1);
	
	return u;
}

UINT64 SimpleStreamReader::Read64()
{
	_STD_READ_CHECK(8, 0);
		
	UINT64 u=(((UINT64)buf[read])<<56) | (((UINT64)buf[read+1])<<48) | (((UINT64)buf[read+2])<<40) | ((UINT64)buf[read+3]<<32) |
	(((UINT64)buf[read+4])<<24) | (((UINT64)buf[read+5])<<16) | (((UINT64)buf[read+6])<<8) | ((UINT64)buf[read+7]);
	
	IncRead(8);
	
	return u;
}


UINT32 SimpleStreamReader::Read32()
{
	_STD_READ_CHECK(4, 0);
		
	UINT32 u=(((UINT32)buf[read])<<24) | (((UINT32)buf[read+1])<<16) | (((UINT32)buf[read+2])<<8) | ((UINT32)buf[read+3]);
	
	IncRead(4);
	
	return u;
}

UINT32 SimpleStreamReader::Read16()
{
	_STD_READ_CHECK(2, 0);
		
	UINT32 u=(((UINT32)buf[read])<<8) | ((UINT32)buf[read+1]);
	
	IncRead(2);
	
	return u;
}

UINT32 SimpleStreamReader::Read8()
{
	_STD_READ_CHECK(1, 0);
		
	UINT32 u=((UINT32)buf[read]);
	
	IncRead(1);
	
	return u;
}

time_t SimpleStreamReader::ReadTime(int len)
{
	OP_ASSERT(sizeof(time_t)==8 || sizeof(time_t)==4);
	OP_ASSERT(len==8 || len==4);
	
	if(len==8)
		return (time_t)Read64();
	else if(len==4)
		return Read32();
		
	return 0;
}

OP_STATUS SimpleStreamReader::ReadString(OpString8 *str, UINT32 len)
{
	OP_ASSERT(str);
	
	if(!str)
		return OpStatus::ERR_NO_MEMORY;
	
	SimpleStreamReader::EmptyString(str);
		
	if((UINT32)str->Capacity()<=len && !str->Reserve(len))
		return OpStatus::ERR_NO_MEMORY;
		
	int written=0;
	char *dest=str->CStr();
	
	// Copy the string in the buffer; this is supposed to work also with UTF8...
	while(len>0)
	{
		if(available<=read/*(UINT32)len*/)
			RETURN_IF_ERROR(RefillBuffer());
		
		int l=(available-read<(UINT32)len)?available-read:len;
		
		op_memcpy(dest+written, buf+read, l);
		//RETURN_IF_ERROR(str->Append((char *)(buf+read), l));
		len-=l;
		written+=l;
		IncRead(l);
	}
	
	dest[written]=0;
	
	
	return OpStatus::OK;
}

OP_STATUS SimpleStreamReader::ReadString(OpString *str, UINT32 len)
{
	OP_ASSERT(str);
	
	if(!str)
		return OpStatus::ERR_NO_MEMORY;
	
	SimpleStreamReader::EmptyString(str);
		
	OpString8 temp;
	ANCHOR(OpString8, temp);
	RETURN_IF_ERROR(ReadString(&temp, len));

	return str->SetFromUTF8(temp.CStr());
}

OP_STATUS SimpleStreamReader::ReadBuf(void *dest, UINT32 len)
{
	OP_ASSERT(dest);
	
	if(!dest)
		return OpStatus::ERR_NO_MEMORY;
	
	while(len)
	{
		OP_ASSERT(available>=read);
		
		if(available<=read)
			RETURN_IF_ERROR(RefillBuffer());
		
		int l=(available-read<(UINT32)len)?(int)(available-read):len;
		
		if(l<=0)
			return OpStatus::ERR_OUT_OF_RANGE;   // Not enough bytes

		op_memcpy(dest, buf+read, l);
		len-=l;
		IncRead(l);
		dest=((unsigned char *)dest)+l;
	}
	
	return OpStatus::OK;
}

/// Skip the requested number of bytes
OP_STATUS SimpleStreamReader::Skip(UINT32 len)
{
	while(len)
	{
		OP_ASSERT(available>=read);
		
		if(available<=read)
			RETURN_IF_ERROR(RefillBuffer());
		
		int l=(available-read<(UINT32)len)?(int)(available-read):len;
		
		len-=l;
		IncRead(l);
	}
	
	return OpStatus::OK;
}

int SimpleStreamReader::CompactBuffer()
{
	OP_ASSERT(buf);
	OP_ASSERT(read<=available);
	//OP_ASSERT(available-read<8);   // It is supposed to move only some bytes
	
	int keep=available-read;
	
	// Move the bytes available but not read at the beginning of the buffer
	if(keep>0)
		op_memcpy(buf, buf+read, keep);
	else
		keep=0;
	
	read=0;
	available=keep;
	
	return keep;
}


OP_STATUS SimpleBufferReader::RefillBuffer()
{
	OP_ASSERT(buf && "Has the reader been Constructed?");
	
	if(!buf)
		return OpStatus::ERR_NO_MEMORY;
		
	int keep=CompactBuffer();
	
	int len=(STREAM_BUF_SIZE-keep)<size-cur?STREAM_BUF_SIZE-keep:size-cur;
	
    op_memcpy(buf+keep, data+cur, len);
    cur+=len;
	available+=(int)len;
	OP_ASSERT(available<=STREAM_BUF_SIZE);
	
	return OpStatus::OK;	
}

OP_STATUS SimpleBufferWriter::FlushBuffer(BOOL finished)
{
	OP_ASSERT(buf);
	
	if(!buf)
		return OpStatus::ERR_NO_MEMORY;
		
	OP_ASSERT(cur+written<=size);
	
	if(cur+written>size)
		return OpStatus::ERR;
		
    op_memcpy(data+cur, buf, written);
    cur+=written;
	written=0;
	
	return OpStatus::OK;	
}


OP_STATUS OpConfigFileReader::Construct(const uni_char* path, OpFileFolder folder)
{
	RETURN_IF_ERROR(SimpleFileReader::Construct(path, folder));
	
	return ReadHeader();
}

OP_STATUS OpConfigFileReader::Construct(OpFile *data_file, BOOL manage_file)
{
	RETURN_IF_ERROR(SimpleFileReader::Construct(data_file, manage_file));
	
	return ReadHeader();
}

OP_STATUS OpConfigFileReader::ReadHeader()
{
	header.file_version=Read32();
	header.app_version=Read32();
	header.tag_len=Read16();
	header.length_len=Read16();
	read_tag_fn=NULL;
	read_len_fn=NULL;
	
	OP_ASSERT(header.file_version==0x1000);	// Of course it could change...
	OP_ASSERT(header.app_version==0x20000);	// Of course it could change...
	OP_ASSERT(header.tag_len==1 || header.tag_len==2 || header.tag_len==4);		// It should be 1
	OP_ASSERT(header.length_len==1 || header.length_len==2 || header.length_len==4);			// It should be 2
	
	if(!header.file_version || !header.app_version || !header.tag_len || !header.length_len)
		return OpStatus::ERR;
	
	if(header.tag_len==1)
	{
		read_tag_fn=&OpConfigFileReader::Read8;
		msb=0x80;
	}
	else if(header.tag_len==2)
	{
		read_tag_fn=&OpConfigFileReader::Read16;
		msb=0x8000;
	}
	else if(header.tag_len==4)
	{
		read_tag_fn=&OpConfigFileReader::Read32;
		msb=0x80000000;
	}
	else
	{
		OP_ASSERT(FALSE);
		
		return OpStatus::ERR;
	}
	
	if(header.length_len==1)
		read_len_fn=&OpConfigFileReader::Read8;
	else if(header.length_len==2)
		read_len_fn=&OpConfigFileReader::Read16;
	else if(header.length_len==4)
		read_len_fn=&OpConfigFileReader::Read32;
	else
	{
		OP_ASSERT(FALSE);
		
		return OpStatus::ERR;
	}
	
	record_start=total_read;
		
	return OpStatus::OK;
}

OP_STATUS OpConfigFileReader::ReadNextRecord(UINT32 &record_tag, int &len)
{
	OP_ASSERT(total_read<=record_start+record_len);
	
	// Skip the remaining values
	if(record_len && total_read<record_start+record_len)
		Skip((record_start+record_len)-total_read);
	
	OP_ASSERT(read_tag_fn);
	
	record_tag=(this->*read_tag_fn)();
	
	/*if(!record_tag)
	{
		len=-1;
		
		return OpStatus::OK;
	}*/
	
	OP_ASSERT(read_len_fn);
	
	record_len=len=(this->*read_len_fn)();

	if(!len && available<header.length_len) // File finished...
	{
		len=-1;
		
		#ifdef _DEBUG
			UINT32 av=available;
			
			RefillBuffer();
			
			OP_ASSERT(av==available);
		#endif
		
		return OpStatus::ERR;
	}
		
	record_start=total_read;
	
	OP_ASSERT(total_read<=record_start+record_len);
	
	return OpStatus::OK;
}

OP_STATUS OpConfigFileReader::ReadNextValue(UINT32 &value_tag, int &len)
{
	// Check if the record is finished
	OP_ASSERT(total_read<=record_start+record_len && read_tag_fn);
	
	if(total_read>=record_start+record_len)
	{
		len=-1;
		value_tag=0;
		
		if(total_read==record_start+record_len)
			return OpStatus::OK;
			
		return OpStatus::ERR;
	}
	
	
	value_tag=(this->*read_tag_fn)();
	
	if((value_tag) & msb)
	{
		len=0;
		value_tag=(value_tag^msb) | MSB_VALUE;
	}
	else
	{
		// Value Length
		OP_ASSERT(read_len_fn);
	
		len=(this->*read_len_fn)();
		
		if(!len && available<header.length_len) // File finished...
		{
			len=-1;
			
			#ifdef _DEBUG
				UINT32 av=available;
				
				RefillBuffer();
				
				OP_ASSERT(av==available);
			#endif
			
			return OpStatus::ERR;
		}
	}
	
	return OpStatus::OK;
}

OP_STATUS SimpleFileReader::Construct(OpFileDescriptor *data_file, BOOL manage_file)
{
	OP_ASSERT(!file && data_file);
	
	file=data_file;
	take_over=manage_file;
	
	return SimpleStreamReader::Construct();
}

OP_STATUS SimpleFileReader::Construct(const uni_char* path, OpFileFolder folder)
{
	OP_ASSERT(!file && path);
	
	if(file)
		return OpStatus::ERR_NOT_SUPPORTED;
	
	file=OP_NEW(OpFile, ());
	
	if(!file)
		return OpStatus::ERR_NO_MEMORY;
	
	RETURN_IF_ERROR(SimpleStreamReader::Construct());
	RETURN_IF_ERROR(((OpFile *)file)->Construct(path, folder));
	take_over=TRUE;
	
	return file->Open(OPFILE_READ);
}

OP_STATUS SimpleFileReader::GetFileLength(OpFileLength& len) const
{
	return file->GetFileLength(len);
}

OP_STATUS SimpleFileReader::RefillBuffer()
{
	int keep=CompactBuffer();
	
	OpFileLength bytes_read;
#ifdef _DEBUG
	OpFileLength pos_before=0;
	OpFileLength pos_after=0;
	
	OP_ASSERT(OpStatus::IsSuccess(GetFilePos(pos_before)));
#endif
	
	RETURN_IF_ERROR(file->Read(buf+keep, STREAM_BUF_SIZE-keep, &bytes_read));
	available+=(int)bytes_read;
	OP_ASSERT(available<=STREAM_BUF_SIZE);
	
#ifdef _DEBUG
	OP_ASSERT(OpStatus::IsSuccess(GetFilePos(pos_after)));
	OP_ASSERT(pos_after>=pos_before);
	OP_ASSERT(pos_after==pos_before + bytes_read);
#endif
	
	if(!bytes_read && file->Eof())
		return OpStatus::ERR;
	
	return OpStatus::OK;	
}

OP_STATUS SimpleFileReader::Close()
{
	if(file)
	{
		OP_STATUS ops=OpStatus::OK;
		
		//DS_Debug_Printf1("DataStream_GenericFile::Close Closing file %s\n", make_singlebyte_in_tempbuffer(file->GetFullPath(), uni_strlen(file->GetFullPath())));
		if(file->Type() == OPSAFEFILE)
			ops=((OpSafeFile *)file)->SafeClose();
		else
			ops=file->Close();
			
		if(take_over)
			OP_DELETE(file);
		file = NULL;
		
		RETURN_IF_ERROR(ops);
	}
	
	return ClearBuffer();
}

OP_STATUS SimpleFileWriter::Construct(OpFileDescriptor *data_file, BOOL manage_file)
{
	OP_ASSERT(!file && data_file);
	
	file=data_file;
	take_over=manage_file;
	
	return SimpleStreamWriter::Construct();
}

OP_STATUS SimpleFileWriter::Construct(const uni_char* path, OpFileFolder folder, BOOL safe_file)
{
	OP_ASSERT(path);
	
	if(!file)
	{
		if(safe_file)
			file=OP_NEW(OpSafeFile, ());
		else
			file=OP_NEW(OpFile, ());
	}
	
	if(!file)
		return OpStatus::ERR_NO_MEMORY;
	
	RETURN_IF_ERROR(SimpleStreamWriter::Construct());
	RETURN_IF_ERROR(((OpFile *)file)->Construct(path, folder));
	take_over=TRUE;
	
	return file->Open(OPFILE_WRITE);
}

OP_STATUS SimpleFileWriter::GetFileLengthUnsafe(OpFileLength& len) const
{
	return file->GetFileLength(len);
}

OP_STATUS SimpleFileWriter::FlushBuffer(BOOL finished)
{
	//OP_ASSERT(buf && file);
	
	if(file)
	{
		if(written && buf)
			RETURN_IF_ERROR(file->Write(buf, written));
		
		written=0;  // if buf is NULL, written should be 0. This should also avoid a theoretical infinite recursion
		
		if(finished)
		{
			OP_ASSERT(!written);
			
			RETURN_IF_ERROR(Close());
		}
	}
	
	DebugCleanBuffer();
	
	if(!buf)
		return OpStatus::ERR_NO_MEMORY;
	
	return OpStatus::OK;	
}

SimpleFileWriter::~SimpleFileWriter() 
{ 
    OP_STATUS res = Close(); 
    
    // If call to Close() fails the file isn't closed and deallocated so
    // we explicitly call CloseFile().
    if (OpStatus::IsError(res))
        CloseFile(FALSE);
}

OP_STATUS SimpleFileWriter::Close(BOOL flush_buffer)
{
    if(written && flush_buffer)
        return FlushBuffer(TRUE);
  
    CloseFile(FALSE);

    return OpStatus::OK;
}

OP_STATUS SimpleFileWriter::CloseFile(BOOL flush_buffer)
{
	if(written && flush_buffer)
		return FlushBuffer(TRUE);
		
	OP_STATUS ops=OpStatus::OK;
	
	if(file)
	{
		//DS_Debug_Printf1("DataStream_GenericFile::Close Closing file %s\n", make_singlebyte_in_tempbuffer(file->GetFullPath(), uni_strlen(file->GetFullPath())));
		if(file->Type() == OPSAFEFILE)
			ops=((OpSafeFile *)file)->SafeClose();
		else
			ops=file->Close();
			
		if(take_over)
			OP_DELETE(file);
		file = NULL;
	}

	return ops;
}

OP_STATUS DiskCacheEntry::ReadValues(OpConfigFileReader *cache, UINT32 tag)
{
	UINT32 value_tag=0;
	int value_len=0;
	
	OP_ASSERT(cache);
	OP_ASSERT(tag==TAG_CACHE_FILE_ENTRY || tag==TAG_VISITED_FILE_ENTRY);
	
	record_tag=tag;
	
	if(!cache)
		return OpStatus::ERR_NULL_POINTER;
	
	Reset();
	
	//while(OpStatus::IsSuccess(cache->ReadNextValue(value_tag, value_len)) && value_len>=0)
	while(TRUE)
	{
		RETURN_IF_ERROR(cache->ReadNextValue(value_tag, value_len));
		
		if(value_len<0)
			break;
			
		switch(value_tag)
		{
			case 0: // It happens...
				RETURN_IF_ERROR(cache->Skip(value_len));
				break;
				
			case TAG_LAST_VISITED:
			{
				last_visited=(time_t)cache->ReadInt64(value_len);
				break;
			}
			
			case TAG_URL_NAME:
			{
				RETURN_IF_ERROR(cache->ReadString(&url, value_len));
				break;
			}
			
			case TAG_LOCALTIME_LOADED:
			{
				local_time=(time_t)cache->ReadInt64(value_len);
				break;
			}
			
			case TAG_STATUS:
			{
				status=cache->ReadInt(value_len);
				break;
			}

			case TAG_CACHE_LOADED_FROM_NETTYPE:
			{
				loaded_from_net_type=cache->ReadInt(value_len);
				break;
			}
			
			case TAG_CONTENT_SIZE:
			{
				content_size=cache->ReadInt64(value_len);
				break;
			}
			
			case TAG_CACHE_CONTAINER_INDEX:
			{
				container_id=cache->ReadInt(value_len);
				break;
			}
			
			case TAG_CONTENT_TYPE:
			{
				RETURN_IF_ERROR(cache->ReadString(&content_type, value_len));
				break;
			}
			
			case TAG_CHARSET:
			{
				RETURN_IF_ERROR(cache->ReadString(&charset, value_len));
				break;
			}
			
			case TAG_HTTP_PROTOCOL_DATA:
			{
				http.ReadValues(cache, this, value_len);
				break;
			}
			
			case TAG_CACHE_FILENAME:
			{
				RETURN_IF_ERROR(cache->ReadString(&file_name, value_len));
				
				break;
			}
			
			case TAG_HTTP_KEEP_EXPIRES:
			{
				expiry=(time_t)cache->ReadInt64(value_len);
				break;
			}
			
			case TAG_RELATIVE_ENTRY:
			{
				UINT32 end=cache->GetRecordPosition()+value_len;
			    OP_STATUS ops=OpStatus::ERR_NO_MEMORY;
			    
				while(cache->GetRecordPosition()<end)
				{
					RelativeEntry *rel=OP_NEW(RelativeEntry, ());
					if(!rel
						|| !OpStatus::IsSuccess(ops=rel->ReadValues(cache, value_len))
						|| !OpStatus::IsSuccess(ops=ar_relative_vec.Add(rel)))
					{
						OP_DELETE(rel);
						
						return ops;
					}
				}
				
				OP_ASSERT(cache->GetRecordPosition()==end);
				
				/*for(UINT32 i=0; i<GetRelativeLinksCount(); i++)
				{
					RelativeEntry* r=GetRelativeLink(i);
					
					printf("%s: %d\n", r->GetName()->CStr(), (UINT32)r->GetLastVisited());
				}*/
				
				
				break;
			}
			
			case TAG_DOWNLOAD_FTP_MDTM_DATE:
			{
				RETURN_IF_ERROR(cache->ReadString(&ftp_mdtm_date, value_len));
				break;
			}
			
			case TAG_ASSOCIATED_FILES:
			{
				assoc_files=cache->ReadInt(value_len);
				break;
			}
			
			case TAG_DOWNLOAD_RESUMABLE_STATUS:
			{
				resumable=(URL_Resumable_Status)cache->ReadInt(value_len, Resumable_Unknown);
				
				OP_ASSERT(resumable==Resumable_Unknown || resumable==Not_Resumable || resumable==Probably_Resumable);
				break;
			}
			
			case TAG_CACHE_EMBEDDED_CONTENT:
			{
				OP_ASSERT(embedded_content_size_reserved && embedded_content_size_reserved>=(UINT32)value_len);
				OP_ASSERT(embedded_content);
				OP_ASSERT(!embedded_content_size);
				
				if(embedded_content && embedded_content_size_reserved && embedded_content_size_reserved>=(UINT32)value_len)
				{
					RETURN_IF_ERROR(cache->ReadBuf(embedded_content, value_len));
					embedded_content_size=value_len;
					OP_ASSERT(embedded_content_size<=CACHE_SMALL_FILES_SIZE);
				}
				else
					RETURN_IF_ERROR(cache->Skip(value_len));
					
				break;
			}
			
			case TAG_FORM_REQUEST:			// BOOLEAN
				form_query=TRUE;
				break;

			case TAG_CACHE_SERVER_CONTENT_TYPE:	// STRING
				RETURN_IF_ERROR(cache->ReadString(&server_content_type, value_len));
				break;
				
			case TAG_CACHE_NEVER_FLUSH:		// BOOLEAN
				never_flush=TRUE;
				break;
				
			case TAG_CACHE_DOWNLOAD_FILE:	// BOOLEAN
				download=TRUE;
				break;
				
			case TAG_CACHE_ALWAYS_VERIFY:	// BOOLEAN
				always_verify=TRUE;
				break;
				
			case TAG_CACHE_MULTIMEDIA:	// BOOLEAN
				multimedia=TRUE;
				break;
				
			case TAG_CACHE_CONTENT_TYPE_WAS_NULL:	// BOOLEAN
				content_type_was_null=TRUE;
				break;

			case TAG_CACHE_MUST_VALIDATE:	// BOOLEAN
				must_validate=TRUE;
				break;
				
			case TAG_CACHE_DYNATTR_FLAG_ITEM:
			case TAG_CACHE_DYNATTR_FLAG_ITEM_Long:
			case TAG_CACHE_DYNATTR_INT_ITEM:
			case TAG_CACHE_DYNATTR_INT_ITEM_Long:
			case TAG_CACHE_DYNATTR_STRING_ITEM:
			case TAG_CACHE_DYNATTR_STRING_ITEM_Long:
			case TAG_CACHE_DYNATTR_URL_ITEM:
			case TAG_CACHE_DYNATTR_URL_ITEM_Long:
				RETURN_IF_ERROR(ReadDynamicAttribute(value_tag, value_len, cache));
				break;
			
			default:
			{
				OP_ASSERT(FALSE);
				RETURN_IF_ERROR(cache->Skip(value_len));
			}
		}
	}
	
	return OpStatus::OK;
}

OP_STATUS DiskCacheEntry::ReadDynamicAttribute(UINT32 tag, int len, OpConfigFileReader *cache)
{
	OP_ASSERT(cache);
	OP_ASSERT(	tag==TAG_CACHE_DYNATTR_FLAG_ITEM || tag==TAG_CACHE_DYNATTR_FLAG_ITEM_Long ||
				tag==TAG_CACHE_DYNATTR_INT_ITEM || tag==TAG_CACHE_DYNATTR_INT_ITEM_Long ||
				tag==TAG_CACHE_DYNATTR_STRING_ITEM || tag==TAG_CACHE_DYNATTR_STRING_ITEM_Long ||
				tag==TAG_CACHE_DYNATTR_URL_ITEM || tag==TAG_CACHE_DYNATTR_URL_ITEM_Long);

	StreamDynamicAttribute *att=NULL;
	UINT32 mod_id;
	UINT32 tag_id;
	BOOL long_form=(tag==TAG_CACHE_DYNATTR_FLAG_ITEM_Long || tag==TAG_CACHE_DYNATTR_INT_ITEM_Long ||
					tag==TAG_CACHE_DYNATTR_STRING_ITEM_Long || tag==TAG_CACHE_DYNATTR_URL_ITEM_Long);
	
	// Read module and tag
	if(long_form)
	{
		mod_id=cache->Read16();
		tag_id=cache->Read16();
	}
	else
	{
		mod_id=cache->Read8();
		tag_id=cache->Read8();
	}
	
	// Create the object with the value
	if(tag==TAG_CACHE_DYNATTR_FLAG_ITEM || tag==TAG_CACHE_DYNATTR_FLAG_ITEM_Long)
		att=OP_NEW(StreamDynamicAttribute, (tag, mod_id, tag_id));
	else if(tag==TAG_CACHE_DYNATTR_INT_ITEM || tag==TAG_CACHE_DYNATTR_INT_ITEM_Long)
		att=OP_NEW(StreamDynamicAttributeInt, (tag, mod_id, tag_id, cache->Read32()));
	else
	{
		StreamDynamicAttributeString *att_str=OP_NEW(StreamDynamicAttributeString, (tag, mod_id, tag_id));
		
		if(att_str)
		{
			int len_str=len-((long_form)?4:2);
			
			RETURN_IF_ERROR(cache->ReadString(att_str->GetOpString8(), len_str));
			att=att_str;
		}
	}
	
	if(!att)
		return OpStatus::ERR_NO_MEMORY;
		
	return ar_dynamic_att.Add(att);
}

BOOL DiskCacheEntry::GetIntValueByTag(UINT32 tag_id, UINT32 &value)
{
	switch(tag_id)
	{
		case TAG_ASSOCIATED_FILES:
			value=assoc_files;
			return TRUE;
			
		case TAG_STATUS:
			value=status;
			return TRUE;

		case TAG_CACHE_LOADED_FROM_NETTYPE:
			value=loaded_from_net_type;
			return TRUE;
			
		case TAG_CACHE_ALWAYS_VERIFY:	// BOOLEAN
			value=always_verify;
			return TRUE;
			
		case TAG_CACHE_MULTIMEDIA:		// BOOLEAN
			value=multimedia;
			return TRUE;
			
		case TAG_CACHE_MUST_VALIDATE:	// BOOLEAN
			value=must_validate;
			return TRUE;
			
		case TAG_FORM_REQUEST:			// BOOLEAN
			value=form_query;
			return TRUE;
			
		case TAG_CACHE_DOWNLOAD_FILE:	// BOOLEAN
			value=download;
			return TRUE;
			
		case TAG_CACHE_NEVER_FLUSH:		// BOOLEAN
			value=never_flush;
			return TRUE;
			
		default:
			OP_ASSERT(FALSE);
			
			return FALSE;
	}
}

BOOL DiskCacheEntry::GetStringPointerByTag(UINT32 tag_id, OpString8 **str)
{
	OP_ASSERT(str);
	
	if(!str)
		return FALSE;
		
	switch(tag_id)
	{
		case TAG_CONTENT_TYPE:
			*str=&content_type;
			return TRUE;
			
		case TAG_CHARSET:
			*str=&charset;
			return TRUE;
			
		case TAG_URL_NAME:
			*str=&url;
			return TRUE;
			
		case TAG_CACHE_FILENAME:
			//*str=&file_name;
			OP_ASSERT(FALSE);
			return FALSE;
			
		case TAG_DOWNLOAD_FTP_MDTM_DATE:
			*str=&ftp_mdtm_date;
			return TRUE;
			
		default:
			OP_ASSERT(FALSE);
			
			return FALSE;
	}
}

OP_STATUS DiskCacheEntry::AddDynamicAttributeFromDataStorage(UINT32 mod_id, UINT32 tag_id, UINT32 short_tag_version, void *ptr, UINT32 len)
{
	OP_ASSERT(short_tag_version==TAG_CACHE_DYNATTR_FLAG_ITEM ||
		short_tag_version==TAG_CACHE_DYNATTR_INT_ITEM ||
		short_tag_version==TAG_CACHE_DYNATTR_STRING_ITEM ||
		short_tag_version==TAG_CACHE_DYNATTR_URL_ITEM);
	
	if( short_tag_version!=TAG_CACHE_DYNATTR_FLAG_ITEM &&
		short_tag_version!=TAG_CACHE_DYNATTR_INT_ITEM &&
		short_tag_version!=TAG_CACHE_DYNATTR_STRING_ITEM &&
		short_tag_version!=TAG_CACHE_DYNATTR_URL_ITEM)
		return OpStatus::ERR_OUT_OF_RANGE;
	
	BOOL longform = (mod_id > 255 ||tag_id > 255);
	UINT32 tag=short_tag_version;
				
	// Use the real tag, not always the short version
	if(longform)
	{
		if(short_tag_version==TAG_CACHE_DYNATTR_FLAG_ITEM)
			tag=TAG_CACHE_DYNATTR_FLAG_ITEM_Long;
		else if(short_tag_version==TAG_CACHE_DYNATTR_INT_ITEM)
			tag=TAG_CACHE_DYNATTR_INT_ITEM_Long;
		else if(short_tag_version==TAG_CACHE_DYNATTR_STRING_ITEM)
			tag=TAG_CACHE_DYNATTR_STRING_ITEM_Long;
		else if(short_tag_version==TAG_CACHE_DYNATTR_URL_ITEM)
			tag=TAG_CACHE_DYNATTR_URL_ITEM_Long;
	}
	
	// Empirical check
	OP_ASSERT(tag==short_tag_version || tag==short_tag_version+1);
	
	// Create the attribute of the right type
	StreamDynamicAttribute *att=NULL;
	OP_STATUS ops=OpStatus::OK;
	
	if(short_tag_version==TAG_CACHE_DYNATTR_FLAG_ITEM)
	{
		OP_ASSERT(!ptr && !len);
		
		if(ptr || len)
			return OpStatus::ERR_OUT_OF_RANGE;
		
		att=OP_NEW(StreamDynamicAttribute, (tag, mod_id, tag_id));
	}
	else if(short_tag_version==TAG_CACHE_DYNATTR_INT_ITEM)
	{
		OP_ASSERT(ptr && len==4);
		
		if(!ptr || len!=4)
			return OpStatus::ERR_OUT_OF_RANGE;
			
		att=OP_NEW(StreamDynamicAttributeInt, (tag, mod_id, tag_id, *((UINT32 *)ptr)));
	}
	else if(short_tag_version==TAG_CACHE_DYNATTR_STRING_ITEM || short_tag_version==TAG_CACHE_DYNATTR_URL_ITEM)
	{
		OP_ASSERT(ptr && op_strlen((const char *)ptr)==len);
		
		if(!ptr)
			return OpStatus::ERR_OUT_OF_RANGE;
			
		att=OP_NEW(StreamDynamicAttributeString, (tag, mod_id, tag_id));
		
		if(att)
			ops=((StreamDynamicAttributeString *)att)->Construct((const char *)ptr);
	}
	
	// Add to the Disk Cache Entry
	if(!att)
		return OpStatus::ERR_NO_MEMORY;
		
	if(OpStatus::IsError(ops) || OpStatus::IsError(ops=AddDynamicAttribute(att)))
		OP_DELETE(att);
		
	return ops;
}


OP_STATUS HTTPCacheEntry::AddLinkRelation(OpStringC8* val)
{
	if(!val)
		return OpStatus::ERR_NULL_POINTER;
		
	OpString8 *str=OP_NEW(OpString8, ());
	
	if(!str)
		return OpStatus::ERR_NO_MEMORY;
		
	OP_STATUS ops;
	
	if(OpStatus::IsError(ops=str->Set(val->CStr())) ||
		OpStatus::IsError(ops=ar_link_rel.Add(str)))
		OP_DELETE(str);
	
	return ops;
}


OP_STATUS HTTPCacheEntry::ReadValues(OpConfigFileReader *cache, DiskCacheEntry *entry, UINT32 record_len)
{
	UINT32 value_tag=0;
	int value_len=0;
	
	OP_ASSERT(cache);
	
	if(!cache)
		return OpStatus::ERR_NULL_POINTER;
	
	Reset();
	
	UINT32 start=cache->GetRecordPosition();
	UINT32 end=start+record_len;
	
	while(cache->GetRecordPosition()<end)
	{
		RETURN_IF_ERROR(cache->ReadNextValue(value_tag, value_len));
		
		if(value_len<0)
			break;
		
		data_available=TRUE;
		
		switch(value_tag)
		{
			case TAG_HTTP_RESPONSE:
				response_code=cache->ReadInt(value_len);
				break;
				
			case TAG_HTTP_AGE:
				age=cache->ReadInt(value_len);
				break;
				
			case TAG_USERAGENT_ID:
				agent_id=cache->ReadInt(value_len);
				break;
				
			case TAG_USERAGENT_VER_ID:
				agent_ver_id=cache->ReadInt(value_len);
				break;
			
			case TAG_HTTP_REFRESH_INT:
				refresh_int=cache->ReadInt(value_len);
				break;
				
				
			case TAG_HTTP_KEEP_LOAD_DATE:
				RETURN_IF_ERROR(cache->ReadString(&keep_load_date, value_len));
				break;
				
			case TAG_HTTP_KEEP_LAST_MODIFIED:
				RETURN_IF_ERROR(cache->ReadString(&keep_last_modified, value_len));
				break;
				
			case TAG_HTTP_KEEP_ENCODING:
				RETURN_IF_ERROR(cache->ReadString(&keep_encoding, value_len));
				break;
				
			case TAG_HTTP_KEEP_ENTITY_TAG:
				RETURN_IF_ERROR(cache->ReadString(&keep_entity, value_len));
				break;
				
			case TAG_HTTP_MOVED_TO_URL:
				RETURN_IF_ERROR(cache->ReadString(&moved_to, value_len));
				break;
				
			case TAG_HTTP_RESPONSE_TEXT:
				RETURN_IF_ERROR(cache->ReadString(&response_text, value_len));
				break;
				
			case TAG_HTTP_REFRESH_URL:
				RETURN_IF_ERROR(cache->ReadString(&refresh_url, value_len));
				break;
				
			case TAG_HTTP_CONTENT_LOCATION:
				RETURN_IF_ERROR(cache->ReadString(&location, value_len));
				break;
				
			case TAG_HTTP_CONTENT_LANGUAGE:
				RETURN_IF_ERROR(cache->ReadString(&language, value_len));
				break;
				
			case TAG_HTTP_SUGGESTED_NAME:
				RETURN_IF_ERROR(cache->ReadString(&suggested, value_len));
				break;
			
			case TAG_HTTP_LINK_RELATION:
			{
			    OpString8 *str=OP_NEW(OpString8, ());
			    OP_STATUS ops=OpStatus::ERR_NO_MEMORY;
			    
			    if(!str 
					|| !OpStatus::IsSuccess(ops=cache->ReadString(str, value_len))
					|| !OpStatus::IsSuccess(ops=ar_link_rel.Add(str)))
			    {
					OP_DELETE(str);
					
					return ops;
			    }
				break;
			}
			
			case TAG_HTTP_RESPONSE_HEADERS:
			{
				UINT32 end=cache->GetRecordPosition()+value_len;
			    OP_STATUS ops=OpStatus::ERR_NO_MEMORY;
			    
			    OP_ASSERT(headers_num==0);
			    
			    while(headers_num<HEADER_DEF_SIZE && cache->GetRecordPosition()<end)
			    {
					RETURN_IF_ERROR(ar_headers_def[headers_num].ReadValues(cache));
					headers_num++;
			    }
			    
				while(cache->GetRecordPosition()<end)
				{
					HTTPHeaderEntry *header=OP_NEW(HTTPHeaderEntry, ());
					if(!header
						|| !OpStatus::IsSuccess(ops=header->ReadValues(cache))
						|| !OpStatus::IsSuccess(ops=ar_headers_vec.Add(header)))
					{
						OP_DELETE(header);
						
						return ops;
					}
				}
				
				OP_ASSERT(cache->GetRecordPosition()==end);
				
				/*for(UINT32 i=0; i<GetHeadersCount(); i++)
				{
					HTTPHeaderEntry* h=GetHeader(i);
					
					printf("%s: %s\n", h->GetName()->CStr(), h->GetValue()->CStr());
				}
				
				static int max=0;
				static int sum=0;
				static int num=0;
	
				sum+=GetHeadersCount();
				num++;
				if(GetHeadersCount()>max)
				{
					max=GetHeadersCount();
					printf("New maximum: %d - mean:%f\n", max, ((float)sum/num));
				}*/
				
				break;
			}
			
			//////////////////// Unsupported tags
			////// (used in older versions of Opera?), written here just to avoid assertions...
			
			// Unused
			case TAG_HTTP_KEEP_MIME_TYPE:
				RETURN_IF_ERROR(cache->Skip(value_len));
				break;
				
			// Wrong place...
			case TAG_HTTP_KEEP_EXPIRES:
			{
				RETURN_IF_ERROR(cache->Skip(value_len));
				
				break;
			}
			
			case 0x68:
				break;
				
			default:
			{
				OP_ASSERT(FALSE);
				RETURN_IF_ERROR(cache->Skip(value_len));
			}
		}
		
		OP_ASSERT(cache->GetRecordPosition()-start<=record_len);
	}
	
	OP_ASSERT(cache->GetRecordPosition()-start==record_len);
	
	return OpStatus::OK;
}


OP_STATUS HTTPCacheEntry::AddHeader(const OpStringC8 *name, const OpStringC8 *value)
{
	if(headers_num<HEADER_DEF_SIZE)
	{
		RETURN_IF_ERROR(ar_headers_def[headers_num].SetName(name));
		RETURN_IF_ERROR(ar_headers_def[headers_num].SetValue(value));
		headers_num++;
		
		return OpStatus::OK;
	}
	
	OP_STATUS ops=OpStatus::ERR_NO_MEMORY;
	HTTPHeaderEntry *header=OP_NEW(HTTPHeaderEntry, ());
	
	if(!header
		|| !OpStatus::IsSuccess(ops=header->SetName(name))
		|| !OpStatus::IsSuccess(ops=header->SetValue(value))
		|| !OpStatus::IsSuccess(ops=ar_headers_vec.Add(header)))
	{
		OP_DELETE(header);
		
		return ops;
	}
	
	return OpStatus::OK;
}
			
BOOL HTTPCacheEntry::GetIntValueByTag(UINT32 tag_id, UINT32 &value)
{
	switch(tag_id)
	{
		case TAG_HTTP_RESPONSE:
			value=response_code;
			return TRUE;
			
		case TAG_HTTP_AGE:
			value=age;
			return TRUE;
		
		case TAG_USERAGENT_ID:
			value=agent_id;
			return TRUE;
			
		case TAG_USERAGENT_VER_ID:
			value=agent_ver_id;
			return TRUE;
			
		case TAG_HTTP_REFRESH_INT:
			value=refresh_int;
			return TRUE;
			
		default:
			OP_ASSERT(FALSE);
			
			return FALSE;
	}
}

BOOL HTTPCacheEntry::GetStringPointerByTag(UINT32 tag_id, OpString8 **str)
{
	OP_ASSERT(str);
	
	if(!str)
		return FALSE;
		
	switch(tag_id)
	{
		case TAG_HTTP_KEEP_LOAD_DATE:
			*str=&keep_load_date;
			return TRUE;
			
		case TAG_HTTP_KEEP_LAST_MODIFIED:
			*str=&keep_last_modified;
			return TRUE;
			
		case TAG_HTTP_KEEP_ENCODING:
			*str=&keep_encoding;
			return TRUE;
			
		case TAG_HTTP_KEEP_ENTITY_TAG:
			*str=&keep_entity;
			return TRUE;
			
		case TAG_HTTP_MOVED_TO_URL:
			*str=&moved_to;
			return TRUE;
			
		case TAG_HTTP_RESPONSE_TEXT:
			*str=&response_text;
			return TRUE;
			
		case TAG_HTTP_REFRESH_URL:
			*str=&refresh_url;
			return TRUE;
			
		case TAG_HTTP_CONTENT_LOCATION:
			*str=&location;
			return TRUE;
			
		case TAG_HTTP_CONTENT_LANGUAGE:
			*str=&language;
			return TRUE;
			
		case TAG_HTTP_SUGGESTED_NAME:
			//*str=&suggested;
			OP_ASSERT(FALSE);
			return FALSE;
			
		case TAG_HTTP_LINK_RELATION:
			OP_ASSERT(FALSE); // Non-sense: it is an array
			*str=ar_link_rel.Get(0);
			return TRUE;
			
		default:
			OP_ASSERT(FALSE);
			
			return FALSE;
	}
}

OP_STATUS HTTPHeaderEntry::ReadValues(OpConfigFileReader *cache)
{
	UINT32 value_tag=0;
	int value_len=0;
	
	OP_ASSERT(cache);
	
	if(!cache)
		return OpStatus::ERR_NULL_POINTER;
	
	Reset();
	
	UINT32 start=cache->GetRecordPosition();
	UINT32 record_len=1; // The first record is supposed to be
	
	while(cache->GetRecordPosition()-start<record_len)
	{
		RETURN_IF_ERROR(cache->ReadNextValue(value_tag, value_len));
		
		if(value_len<0)
			break;
			
		data_available=TRUE;
		
		switch(value_tag)
		{
			case TAG_HTTP_RESPONSE_ENTRY:
				record_len=value_len;		// Just update the record len and the start
				start=cache->GetRecordPosition();
				break;
				
			case TAG_HTTP_RESPONSE_HEADER_NAME:
				OP_ASSERT(name.IsEmpty());
				RETURN_IF_ERROR(cache->ReadString(&name, value_len));
				break;
				
			case TAG_HTTP_RESPONSE_HEADER_VALUE:
				OP_ASSERT(value.IsEmpty());
				RETURN_IF_ERROR(cache->ReadString(&value, value_len));
				break;
				
			default:
			{
				OP_ASSERT(FALSE);
				RETURN_IF_ERROR(cache->Skip(value_len));
			}
		}
		
		OP_ASSERT(record_len>1);		
		OP_ASSERT(cache->GetRecordPosition()-start<=record_len);
	}
	
	OP_ASSERT(cache->GetRecordPosition()-start==record_len);
	
	return OpStatus::OK;
}

BOOL HTTPHeaderEntry::GetStringPointerByTag(UINT32 tag_id, OpString8 **str)
{
OP_ASSERT(str);
	
	if(!str)
		return FALSE;
		
	switch(tag_id)
	{
		case TAG_HTTP_RESPONSE_HEADER_NAME:
			*str=&name;
			return TRUE;
			
		case TAG_HTTP_RESPONSE_HEADER_VALUE:
			*str=&value;
			return TRUE;
			
		default:
			OP_ASSERT(FALSE);
			
			return FALSE;
	}
}

OP_STATUS RelativeEntry::ReadValues(OpConfigFileReader *cache, UINT32 record_len)
{
	UINT32 value_tag=0;
	int value_len=0;
	
	OP_ASSERT(cache);
	
	if(!cache)
		return OpStatus::ERR_NULL_POINTER;
	
	Reset();
	
	UINT32 start=cache->GetRecordPosition();
	
	while(cache->GetRecordPosition()-start<record_len)
	{
		RETURN_IF_ERROR(cache->ReadNextValue(value_tag, value_len));
		
		if(value_len<0)
			break;
		
		if(value_len>0)
		{
			switch(value_tag)
			{
				case TAG_RELATIVE_NAME:
					OP_ASSERT(name.IsEmpty());
					RETURN_IF_ERROR(cache->ReadString(&name, value_len));
					break;
					
				case TAG_RELATIVE_VISITED:
					OP_ASSERT(!visited);
					visited=(time_t)cache->ReadInt64(value_len);
					break;
					
				default:
				{
					OP_ASSERT(FALSE);
					RETURN_IF_ERROR(cache->Skip(value_len));
				}
			}
		}
		
		OP_ASSERT(cache->GetRecordPosition()-start<=record_len);
	}
	
	OP_ASSERT(cache->GetRecordPosition()-start==record_len);
	
	return OpStatus::OK;
}


#define _STD_WRITE_CHECK(dim)														\
	if(STREAM_BUF_SIZE-written<dim) RETURN_IF_ERROR(FlushBuffer(FALSE));			\
	OP_ASSERT(STREAM_BUF_SIZE-written>=dim);


OP_STATUS SimpleStreamWriter::Write64(UINT64 val)
{
	_STD_WRITE_CHECK(8);
	
	buf[written+7]=(unsigned char) val;
	buf[written+6]=(unsigned char) (val>>8);
	buf[written+5]=(unsigned char) (val>>16);
	buf[written+4]=(unsigned char) (val>>24);
	buf[written+3]=(unsigned char) (val>>32);
	buf[written+2]=(unsigned char) (val>>40);
	buf[written+1]=(unsigned char) (val>>48);
	buf[written]=(unsigned char) (val>>56);
	
	IncWrite(8);
	
	return OpStatus::OK;
}

OP_STATUS SimpleStreamWriter::Write32(UINT32 val)
{
	_STD_WRITE_CHECK(4);
	
	buf[written+3]=(unsigned char) val;
	buf[written+2]=(unsigned char) (val>>8);
	buf[written+1]=(unsigned char) (val>>16);
	buf[written]=(unsigned char) (val>>24);
	
	IncWrite(4);
	
	return OpStatus::OK;
}

OP_STATUS SimpleStreamWriter::Write16(UINT32 val)
{
	_STD_WRITE_CHECK(2);
	
	buf[written+1]=(unsigned char) val;
	buf[written]=(unsigned char) (val>>8);
	
	IncWrite(2);
	
	return OpStatus::OK;
}

OP_STATUS SimpleStreamWriter::Write8(UINT32 val)
{
	_STD_WRITE_CHECK(1);
	
	buf[written]=(unsigned char) val;
	
	IncWrite(1);
	
	return OpStatus::OK;
}


OP_STATUS SimpleStreamWriter::WriteString(OpString8 *val, int length_len)
{
	if(!val)
		return OpStatus::ERR_NULL_POINTER;
		
	OP_ASSERT(length_len==1 || length_len==2);
	int str_len=val->Length();
	
	if(length_len==2)
		RETURN_IF_ERROR(Write16(str_len));
	else if(length_len==1)
		RETURN_IF_ERROR(Write8(str_len));
	else if(length_len==4)
		RETURN_IF_ERROR(Write32(str_len));
	else
		return OpStatus::ERR;
	
	return WriteBuf(val->CStr(), str_len);
}

OP_STATUS SimpleStreamWriter::WriteBuf(const void *src, UINT32 buf_len)
{
	OP_ASSERT(src);
	
	if(!src)
		return OpStatus::ERR_NO_MEMORY;
	
	while(buf_len)
	{
		OP_ASSERT(written<=STREAM_BUF_SIZE);
		
		if(written>=STREAM_BUF_SIZE)
			RETURN_IF_ERROR(FlushBuffer(FALSE));
		
		int l=(STREAM_BUF_SIZE-written<(UINT32)buf_len)?(int)(STREAM_BUF_SIZE-written):buf_len;
		
		OP_ASSERT(l>0);
		
		if(l<=0)
			return OpStatus::ERR;
			
		op_memcpy(buf+written, src, l);
		buf_len-=l;
		IncWrite(l);
		src=((unsigned char *)src)+l;
	}
	
	return OpStatus::OK;
}

/*OP_STATUS SimpleFileReader::GetLogicalFilePositionAndAlign(OpFileLength& pos)
{
	UINT32 remaining=ClearBuffer();
	OpFileLength physical_pos, logical_pos;
	
	RETURN_IF_ERROR(GetFilePos(physical_pos));
	
	OP_ASSERT(physical_pos>=remaining);
	
	if(physical_pos>remaining)
		logical_pos=physical_pos-remaining;
	else
		logical_pos=0;
		
	RETURN_IF_ERROR(SetFilePos(logical_pos));
		
	pos=logical_pos;
	
	return OpStatus::OK;
}*/

OP_STATUS SimpleFileReadWrite::ConstructFile(const uni_char* path, OpFileFolder folder, BOOL safe)
{
	OP_ASSERT(!file && path);
	OP_ASSERT(last_operation==OperationNone);
	
	if(file)
		return OpStatus::ERR_NOT_SUPPORTED;

	if(safe)
		file=OP_NEW(OpSafeFile, ());
	else
		file=OP_NEW(OpFile, ());

	if(!file)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(((OpFile *)file)->Construct(path, folder));

	return ConstructPrivate();
}


/// RAM File adapted to the uses of the cache, as OpMemFile is not exactly what we need here
/// The file starts empty, and it can extend till the limit provided by the buffer
/// This is a partial implementation, just good enough for the cache
/// This file:
///	* Starts empty
/// * Write and read positions are the same
/// * cannot grow past its limit
class OpMemFileCache: public OpFileDescriptor
{
	/// TRUE if the file is opened
	BOOL open;
	/// TRUE if the file is read only 
	BOOL readonly;
	/// Length of the file
	OpFileLength file_len;
	/// Buffer with the data
	UINT8 *buf;
	/// Size of the buffer
	OpFileLength buf_size;
	/// Current position
	OpFileLength file_pos;
	/// TRUE if the file exists
	BOOL file_exists;
	/// TRUE if the memory is owned by this object
	BOOL take_over;

public:
	OpMemFileCache(): open(FALSE), readonly(FALSE), file_len(0), buf(NULL), buf_size(0), file_pos(0), file_exists(FALSE), take_over(FALSE) {}
	virtual ~OpMemFileCache() { OP_DELETEA(buf); }
	static OpMemFileCache* Create(UINT32 size)
	{
		UINT8 *buf=OP_NEWA(UINT8, size);
		
		if(!buf)
			return NULL;

		OpMemFileCache *p=new OpMemFileCache();

		if(p)
		{
			p->buf=buf;
			p->buf_size=size;
			p->file_len=size;
			p->take_over=TRUE;
		}
		else
			OP_DELETEA(buf);

		return p;
	}
	static OpMemFileCache* Create(UINT8 *buf, UINT32 size, BOOL take_over)
	{
		if(!buf)
			return NULL;

		OpMemFileCache *p=new OpMemFileCache();

		if(p)
		{
			p->buf=buf;
			p->file_len=size;
			p->buf_size=size;
			p->take_over=take_over;
		}

		return p;
	}

	/**
	 * Retrieves the type.
	 *
	 * @return OPFILE, OPMEMFILE or OPSAFEFILE.
	 */
	virtual OpFileType Type() const { return OPMEMFILE; }

	virtual BOOL IsWritable() const { return !readonly; }
	virtual OP_STATUS Open(int mode) { readonly = ! ((mode & OPFILE_WRITE) || (mode & OPFILE_APPEND)); if(mode & OPFILE_WRITE) file_len=0; open=TRUE; if(!readonly) file_exists=TRUE; return OpStatus::OK; }
	virtual BOOL IsOpen() const { return open; }
	virtual OP_STATUS Close() { open=FALSE; return OpStatus::OK; };
	virtual BOOL Eof() const { return file_pos==file_len; }
	virtual OP_STATUS Exists(BOOL& exists) const { exists=file_exists; return OpStatus::OK; };
	virtual OP_STATUS GetFilePos(OpFileLength& pos) const { pos=file_pos; return OpStatus::OK; };
	virtual OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode=SEEK_FROM_START) 
	{
		if(seek_mode==SEEK_FROM_START)
			file_pos=pos;
		else if(seek_mode==SEEK_FROM_CURRENT)
			file_pos+=pos;
		else
			file_pos=file_len-pos;

		if(file_pos>file_len)
			file_pos=file_len;

		return OpStatus::OK;
	};

	virtual OP_STATUS GetFileLength(OpFileLength& len) const { len=file_len; return OpStatus::OK; };
	virtual OP_STATUS Write(const void* data, OpFileLength len)
	{
		if(!open || readonly)
			return OpStatus::ERR;

		OpFileLength real_len=(len<=buf_size-file_pos)?len:buf_size-file_pos;

		if(!real_len || file_pos>=buf_size || real_len!=len)
			return OpStatus::ERR_NO_DISK;

		op_memcpy(buf+file_pos, data, (UINT32)len);
		file_pos+=len;
		if(file_pos>file_len)
			file_len=file_pos;

		OP_ASSERT(file_pos<=file_len);

		return OpStatus::OK;
	}

	virtual OP_STATUS Read(void* data, OpFileLength len, OpFileLength* bytes_read=NULL)
	{
		if(!open)
			return OpStatus::ERR;

		OpFileLength real_len=(len<=file_len-file_pos)?len:file_len-file_pos;

		if(!real_len || file_pos>=file_len)
			return OpStatus::ERR_NO_DISK;

		op_memcpy(data, buf+file_pos, (UINT32)real_len);
		file_pos+=real_len;
		if(bytes_read)
			*bytes_read=(OpFileLength)real_len;

		OP_ASSERT(file_pos<=file_len);

		return OpStatus::OK;
	}
	virtual OP_STATUS ReadLine(OpString8& str) { OP_ASSERT(!"NOT IMPLEMENTED!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OpFileDescriptor* CreateCopy() const { OP_ASSERT(!"NOT IMPLEMENTED!"); return NULL; }
};

OP_STATUS SimpleFileReadWrite::ConstructMemory(UINT32 size)
{
	OP_ASSERT(!file && size>0);
	OP_ASSERT(last_operation==OperationNone);
	
	if(file || size<1)
		return OpStatus::ERR_NOT_SUPPORTED;

	file=OpMemFileCache::Create(size);

	if(!file)
		return OpStatus::ERR_NO_MEMORY;

	return ConstructPrivate();
}

OP_STATUS SimpleFileReadWrite::ConstructPrivate()
{
	BOOL exists;
	
	RETURN_IF_ERROR(file->Exists(exists));
	
	if(exists)
		RETURN_IF_ERROR(file->Open(OPFILE_UPDATE));
	else
		RETURN_IF_ERROR(file->Open(OPFILE_OVERWRITE));

	RETURN_IF_ERROR(file->SetFilePos(0, SEEK_FROM_START));

	last_operation=OperationNone;
	read_pos=0;
	write_pos=0;

	OP_STATUS ops=OpStatus::OK;

	// Tolerate multiple constructions
	if(!sfw)
		sfw=new SimpleFileWriter(NULL);
	if(!sfr)
		sfr=new SimpleFileReader(NULL);

	if(!sfw || !sfr)
		ops=OpStatus::ERR_NO_MEMORY;
		
	if(OpStatus::IsError(ops) ||
	   OpStatus::IsError(ops=((SimpleFileWriter *)sfw)->Construct(file, FALSE)) ||
	   OpStatus::IsError(ops=((SimpleFileReader *)sfr)->Construct(file, FALSE)))
	{
		OP_DELETE(sfw);
		OP_DELETE(sfr);
		sfw=NULL;
		sfr=NULL;

		return ops;
	}

	//RETURN_IF_ERROR(file->GetFileLength(length));
	
	return OpStatus::OK;
}

OP_STATUS SimpleFileReadWrite::Truncate()
{
	OP_ASSERT(file);
	
	if(!file)
		return OpStatus::ERR_NULL_POINTER;
	
	RETURN_IF_ERROR(file->Close());
	RETURN_IF_ERROR(file->Open(OPFILE_OVERWRITE));
	
	last_operation=OperationNone;
	read_pos=0;
	write_pos=0;
	sfr->total_read=0;
	sfr->snapshot_read=0;
	sfw->total_written=0;
	sfw->snapshot_written=0;
	
	#ifdef DEBUG_ENABLE_OPASSERT
		OpFileLength len;
		
		OP_ASSERT(OpStatus::IsSuccess(GetFileLength(len)));
		
		OP_ASSERT(len==0);
	#endif
	
	return OpStatus::OK;
}

OP_STATUS SimpleFileReadWrite::Close()
{
	// OP_ASSERT(file);
	
	if(sfw)
		sfw->FlushBuffer(FALSE);
	if(sfr)
		sfr->ClearBuffer();
	OP_STATUS ops=OpStatus::OK;

	if(file)
	{	
	if(file->Type() == OPSAFEFILE)
		ops=((OpSafeFile *)file)->SafeClose();
	else
		ops=file->Close();
	}

	if(sfr)
		sfr->file=NULL;
	if(sfw)
		sfw->Close(TRUE);
	
	OP_DELETE(file); // Deleted by sfw
	file=NULL;
	
	last_operation=OperationNone;
	read_pos=write_pos=0;
	if(sfr)
		sfr->TakeSnapshot();
	if(sfw)
		sfw->TakeSnapshot();
	
	return ops;
}


OP_STATUS SimpleFileReadWrite::SwitchToRead(BOOL force_clear_buffer)
{
	if(!sfw || !sfr)
		return OpStatus::ERR_NULL_POINTER;

	if(last_operation==OperationWrite || last_operation==OperationSeek || force_clear_buffer)
	{
		sfw->FlushBuffer(FALSE);
		sfr->ClearBuffer();
		RETURN_IF_ERROR(sfw->GetFilePos(write_pos));
		RETURN_IF_ERROR(sfr->SetFilePos(read_pos));
		sfr->TakeSnapshot();
		sfw->TakeSnapshot();
	}
		
	last_operation=OperationRead;
	
	return OpStatus::OK;
}

OP_STATUS SimpleFileReadWrite::SetWriteFilePos(OpFileLength pos)
{
	if(!sfw)
		return OpStatus::ERR_NULL_POINTER;

	if(read_only)
		return OpStatus::ERR_NOT_SUPPORTED;
		
	if(last_operation==OperationWrite && pos==write_pos+sfw->GetSnapshotBytes())
		return OpStatus::OK;
		
	RETURN_IF_ERROR(sfw->FlushBuffer(FALSE));
	write_pos=pos;
	last_operation=OperationSeek;

	return OpStatus::OK;
}

OP_STATUS SimpleFileReadWrite::SwitchToWrite(BOOL force_clear_buffer)
{
	if(!sfw || !sfr)
		return OpStatus::ERR_NULL_POINTER;

	if(read_only)
		return OpStatus::ERR_NOT_SUPPORTED;
		
	if(last_operation==OperationRead || last_operation==OperationSeek  || force_clear_buffer)
	{
		read_pos+=sfr->TakeSnapshot();
		//RETURN_IF_ERROR(sfr->GetLogicalFilePositionAndAlign(read_pos));
		RETURN_IF_ERROR(sfw->SetFilePos(write_pos));
		sfw->TakeSnapshot();
		sfr->TakeSnapshot();
	}
	
	last_operation=OperationWrite;
	
	return OpStatus::OK;
}


OP_STATUS SimpleFileReadWrite::GetFileLength(OpFileLength& len)
{
	RETURN_IF_ERROR(SwitchToRead(FALSE));
	OP_STATUS ops=sfr->GetFileLength(len);
	
#ifdef _DEBUG
	OpFileLength len2;
	
	if(sfw)
	{
		sfw->GetFileLengthUnsafe(len2);
	
		OP_ASSERT(len==len2);
	}
#endif
	
	return ops;
}


OP_STATUS OpConfigFileWriter::WriteDynamicAttributeTag(UINT32 tag, UINT16 mod_id, UINT16 tag_id, void *ptr, UINT32 len)
{
	bool long_form=(tag==TAG_CACHE_DYNATTR_FLAG_ITEM_Long || tag==TAG_CACHE_DYNATTR_INT_ITEM_Long || tag==TAG_CACHE_DYNATTR_STRING_ITEM_Long || tag==TAG_CACHE_DYNATTR_URL_ITEM_Long);
	
	OP_ASSERT(	((tag==TAG_CACHE_DYNATTR_FLAG_ITEM || tag==TAG_CACHE_DYNATTR_FLAG_ITEM_Long) && len==0) ||
				((tag==TAG_CACHE_DYNATTR_INT_ITEM || tag==TAG_CACHE_DYNATTR_INT_ITEM_Long) && len==4) ||
				(tag==TAG_CACHE_DYNATTR_STRING_ITEM || tag==TAG_CACHE_DYNATTR_STRING_ITEM_Long || 
				 tag==TAG_CACHE_DYNATTR_URL_ITEM || tag==TAG_CACHE_DYNATTR_URL_ITEM_Long));
	OP_ASSERT(ptr!=NULL || len==0);
	
	if(!ptr)
		len=0;
		
	UINT32 tag_len=(long_form)?4:2+len;
	
	RETURN_IF_ERROR((this->*write_tag_fn)(tag));
	RETURN_IF_ERROR((this->*write_len_fn)(tag_len));
	
	if(long_form)
	{
		RETURN_IF_ERROR(Write16(mod_id));
		RETURN_IF_ERROR(Write16(tag_id));
	}
	else
	{
		RETURN_IF_ERROR(Write8(mod_id));
		RETURN_IF_ERROR(Write8(tag_id));
	}
	
	// A buf write could have been performed here, instead
	switch(tag)
	{
		// Flags are not written
		case TAG_CACHE_DYNATTR_FLAG_ITEM:
		case TAG_CACHE_DYNATTR_FLAG_ITEM_Long:
			return OpStatus::OK;
			
		// 32 bits
		case TAG_CACHE_DYNATTR_INT_ITEM:
		case TAG_CACHE_DYNATTR_INT_ITEM_Long:
			return Write32(*((UINT32 *)ptr));
			
		default:
			return WriteBuf(ptr, (int)len);
	}
}
/// Write a dynamic attribute tag
OP_STATUS OpConfigFileWriter::WriteDynamicAttributeTag(StreamDynamicAttribute *att)
{
	OP_ASSERT(att);
	
	if(!att)
		return OpStatus::ERR_NULL_POINTER;
	
	return WriteDynamicAttributeTag(att->GetTag(), att->GetModuleID(), att->GetTagID(), att->GetPointer(), att->GetLength());
}


OP_STATUS OpConfigFileWriter::InitHeader(int tag_len, int length_len)
{
	header.app_version=0x20000;
	header.file_version=0x1000;
	header.tag_len=tag_len;
	header.length_len=length_len;
	
	if(tag_len==1)
	{
		write_tag_fn=&OpConfigFileWriter::Write8;
		msb=0x80;
	}
	else if(tag_len==2)
	{
		write_tag_fn=&OpConfigFileWriter::Write16;
		msb=0x8000;
	}
	else if(tag_len==4)
	{
		write_tag_fn=&OpConfigFileWriter::Write32;
		msb=0x80000000;
	}
	else
	{
		OP_ASSERT(FALSE);
		
		return OpStatus::ERR;
	}
	
	if(length_len==1)
		write_len_fn=&OpConfigFileWriter::Write8;
	else if(length_len==2)
		write_len_fn=&OpConfigFileWriter::Write16;
	else if(length_len==4)
		write_len_fn=&OpConfigFileWriter::Write32;
	else
	{
		OP_ASSERT(FALSE);
		
		return OpStatus::ERR;
	}
	
	return OpStatus::OK;
}


/// Write the header to the file
OP_STATUS OpConfigFileWriter::WriteHeader()
{
	OP_ASSERT(written==0);
	
	OP_ASSERT(header.file_version==0x1000);	// Of course it could change...
	OP_ASSERT(header.app_version==0x20000);	// Of course it could change...
	OP_ASSERT(header.tag_len==1 || header.tag_len==2 || header.tag_len==4);		// It should be 1
	OP_ASSERT(header.length_len==1 || header.length_len==2 || header.length_len==4);			// It should be 2
	
	if(written || !header.file_version || !header.app_version || !header.tag_len || !header.length_len)
		return OpStatus::ERR;
		
	RETURN_IF_ERROR(Write32(header.file_version));
	RETURN_IF_ERROR(Write32(header.app_version));
	RETURN_IF_ERROR(Write16(header.tag_len));
	RETURN_IF_ERROR(Write16(header.length_len));
	
	OP_ASSERT(written==12);
	
	return OpStatus::OK;
}

OP_STATUS OpConfigFileWriter::Construct(const uni_char* path, OpFileFolder folder, int tag_len, int length_len, BOOL safe_file)
{
	RETURN_IF_ERROR(SimpleFileWriter::Construct(path, folder, safe_file));
	RETURN_IF_ERROR(InitHeader(tag_len, length_len));
	
	return WriteHeader();
}

OP_STATUS OpConfigFileWriter::Construct(OpFile *data_file, int tag_len, int length_len, BOOL manage_file)
{
	RETURN_IF_ERROR(SimpleFileWriter::Construct(data_file, manage_file));
	RETURN_IF_ERROR(InitHeader(tag_len, length_len));
	
	return WriteHeader();
}
	
/// Write a tag
// @param tag Tag ID
// @param record_len Length of the whole record
OP_STATUS OpConfigFileWriter::WriteRecord(UINT32 record_tag, UINT32 record_len)
{
	RETURN_IF_ERROR((this->*write_tag_fn)(record_tag));
	
	return (this->*write_len_fn)(record_len);
}

OP_STATUS OpConfigFileWriter::Close(char *next_file_name)
{
	//if(!file)
	//	return OpStatus::OK;
	
	if(next_file_name)
	{
		OP_ASSERT(op_strlen(next_file_name)==5);
		
		int len=op_strlen(next_file_name);
		
		RETURN_IF_ERROR(WriteRecord(TAG_CACHE_LASTFILENAME_ENTRY, len));
		if(len)
			RETURN_IF_ERROR(WriteBuf(next_file_name, len));
	}
	
	RETURN_IF_ERROR(FlushBuffer(TRUE));
	
	return SimpleFileWriter::Close();
}

void DiskCacheEntry::SetIntValueByTag(UINT32 tag_id, UINT32 value)
{
	switch(tag_id)
	{
		case TAG_ASSOCIATED_FILES:
			assoc_files=value;
			break;
			
		case TAG_STATUS:
			status=value;
			break;

		case TAG_CACHE_LOADED_FROM_NETTYPE:
			loaded_from_net_type=value;
			break;
			
		case TAG_CACHE_ALWAYS_VERIFY:	// BOOLEAN
			always_verify=(value)?TRUE:FALSE;
			break;
			
		case TAG_CACHE_MULTIMEDIA:		// BOOLEAN
			multimedia=(value)?TRUE:FALSE;
			break;
			
		case TAG_CACHE_MUST_VALIDATE:	// BOOLEAN
			must_validate=(value)?TRUE:FALSE;
			break;
			
		case TAG_FORM_REQUEST:			// BOOLEAN
			form_query=(value)?TRUE:FALSE;
			break;
			
		case TAG_CACHE_DOWNLOAD_FILE:	// BOOLEAN
			download=(value)?TRUE:FALSE;
			break;
			
		case TAG_CACHE_NEVER_FLUSH:		// BOOLEAN
			never_flush=(value)?TRUE:FALSE;
			break;
			
		default:
			OP_ASSERT(FALSE);
	}
}

OP_STATUS DiskCacheEntry::SetStringValueByTag(UINT32 tag_id, const OpStringC8 *str)
{
	OP_ASSERT(str);
	
	if(!str)
		return OpStatus::ERR_NULL_POINTER;
		
	switch(tag_id)
	{
		case TAG_CONTENT_TYPE:
			return content_type.Set(str->CStr());
			
		case TAG_CHARSET:
			return charset.Set(str->CStr());
			
		case TAG_URL_NAME:
			return url.Set(str->CStr());
			
		case TAG_DOWNLOAD_FTP_MDTM_DATE:
			return ftp_mdtm_date.Set(str->CStr());
			
		case TAG_CACHE_FILENAME:
			OP_ASSERT(FALSE);
			return OpStatus::ERR_NOT_SUPPORTED;
			
		default:
			OP_ASSERT(FALSE);
			
			return OpStatus::ERR_NO_SUCH_RESOURCE;
	}
}


////////////////////////////////////////////////////////////////////////////
/////// ComputeRecordLength() and WriteValues MUST BE SYNCHRONIZED
////////////////////////////////////////////////////////////////////////////
OP_STATUS DiskCacheEntry::ComputeRecordLength(OpConfigFileHeader *header, int &len, int &http_len, int &headers_len, UINT32 tag, BOOL embedded, UINT32 &len_rel)
{
	OP_ASSERT(header);
	
	len=0;
	
	if(!header)
		return OpStatus::ERR;
		
	len+=GetBoolLen(header, form_query);
	len+=GetBoolLen(header, always_verify);
	len+=GetBoolLen(header, multimedia);
	len+=GetBoolLen(header, content_type_was_null);
	len+=GetBoolLen(header, must_validate);
	
	#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
		len+=GetBoolLen(header, never_flush);
	#endif
	
	if(tag==URL_CACHE_USERFILE)
	{
		len+=GetBoolLen(header, download);
		len+=GetUINT32Len(header, 1);	// TAG_DOWNLOAD_RESUMABLE_STATUS
		len+=GetStringLen(header, &ftp_mdtm_date, FALSE);
	}
	
	len+=GetUINT64Len(header, 1);	// TAG_CONTENT_SIZE
	len+=GetUINT32Len(header, 1);	// TAG_STATUS
	len+=GetUINT32Len(header, 1);	// TAG_CACHE_LOADED_FROM_NETTYPE
	len+=GetTime32Len(header, 1);	// TAG_LAST_VISITED
	#ifdef URL_ENABLE_ASSOCIATED_FILES
		if(assoc_files)
			len+=GetTime32Len(header, 1);	// TAG_ASSOCIATED_FILES
	#endif

	len+=GetTime64Len(header, 2);   // TAG_HTTP_KEEP_EXPIRES, TAG_LOCALTIME_LOADED
	
	if(container_id)
		len+=GetUINT8Len(header, 1);
	
#ifndef SEARCH_ENGINE_CACHE
	len+=GetStringLen(header, &url, FALSE);
	len+=GetStringLen(header, &file_name, FALSE);
#endif
	
	len+=GetStringLen(header, &content_type, FALSE);
	len+=GetStringLen(header, &charset, FALSE);
	if(embedded && embedded_content_size)
		len+=GetBufferLen(header, embedded_content, embedded_content_size);

	len+=GetStringLen(header, &server_content_type, FALSE);

	http_len=0;
	
	RETURN_IF_ERROR(http.ComputeRecordLength(header, http_len, headers_len));
	len+=http_len; // Data
	len+=header->GetTagLength()+header->GetLengthLength(); // Header
	
	// Used to put a maximum on the relative entries
	len_rel=0;
	
	// Length of the relative links
	for(UINT32 i=0; i<ar_relative_vec.GetCount() && len_rel<RELATIVE_MAX_SIZE; i++)
	{
		int l=0;
		
		RelativeEntry *rel_entry=ar_relative_vec.Get(i);
		
		if(rel_entry)
		{
			RETURN_IF_ERROR(rel_entry->ComputeRecordLength(header, l));
		
			len_rel+=header->GetTagLength()+header->GetLengthLength()+l;
			len+=header->GetTagLength()+header->GetLengthLength()+l;
		}
	}
	
	
	// Length of the dynamic attributes
	for(UINT32 j=0, num_att=ar_dynamic_att.GetCount(); j<num_att; j++)
	{
		StreamDynamicAttribute *att=GetDynamicAttribute(j);
		
		len+=(int)(header->GetTagLength()+header->GetLengthLength()+att->ComputeRecordLength());
	}
	
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////
/////// ComputeRecordLength() and WriteValues MUST BE SYNCHRONIZED
////////////////////////////////////////////////////////////////////////////
OP_STATUS HTTPCacheEntry::ComputeRecordLength(OpConfigFileHeader *header, int &len, int &headers_len)
{
	OP_ASSERT(header);
	
	len=0;//header->GetTagLength()+header->GetLengthLength();   // Tag and length for HTTP records
	
	if(!header)
		return OpStatus::ERR;
		
	len+=GetUINT32Len(header, 6); // TAG_USERAGENT_ID, TAG_USERAGENT_VER_ID, TAG_HTTP_RESPONSE, TAG_HTTP_AGE, TAG_USERAGENT_ID, TAG_USERAGENT_VER_ID
	
	if(!refresh_url.IsEmpty())
		len+=GetUINT32Len(header, 1);
	
	len+=GetStringLen(header, &keep_load_date, FALSE);
	len+=GetStringLen(header, &keep_last_modified, FALSE);
	len+=GetStringLen(header, &keep_encoding, FALSE);
	len+=GetStringLen(header, &keep_entity, FALSE);
	len+=GetStringLen(header, &moved_to, FALSE);
	len+=GetStringLen(header, &response_text, FALSE);
	len+=GetStringLen(header, &refresh_url, FALSE);
	len+=GetStringLen(header, &location, FALSE);
	len+=GetStringLen(header, &language, FALSE);
	len+=GetStringLen(header, &suggested, FALSE);

	for(UINT32 i=0; i<ar_link_rel.GetCount(); i++)
		len+=GetStringLen(header, ar_link_rel.Get(i), FALSE);

	headers_len=0;
	UINT32 headers=GetHeadersCount();

	int temp=len;

	for(UINT32 j=0; j<headers; j++)
	{
		int l=0;

		HTTPHeaderEntry *h=GetHeader(j);

		if(h)
		{
			GetHeader(j)->ComputeRecordLength(header, l);
			len+=l+header->GetTagLength()+header->GetLengthLength();  // Single header "overhead"
		}
	}

	headers_len=len-temp;

	if(headers)
		len+=header->GetTagLength()+header->GetLengthLength();    // Record "overhead"

	return OpStatus::OK;
}

OP_STATUS DiskCacheEntry::AddRelativeLink(OpStringC8 *name, time_t visited)
{
	RelativeEntry *entry=OP_NEW(RelativeEntry, ());

	if(!entry)
		return OpStatus::ERR_NO_MEMORY;
		
	OP_STATUS ops;
	
	if( OpStatus::IsError(ops=entry->SetValues(name, visited)) ||
		OpStatus::IsError(ops=ar_relative_vec.Add(entry)))
		OP_DELETE(entry);
	
	return ops;
}

////////////////////////////////////////////////////////////////////////////
/////// ComputeRecordLength() and WriteValues MUST BE SYNCHRONIZED
////////////////////////////////////////////////////////////////////////////
OP_STATUS DiskCacheEntry::WriteValues(OpConfigFileWriter *cache, UINT32 tag, BOOL embedded, BOOL &url_refused)
{
	OP_ASSERT(cache && tag==0x01);

	url_refused=FALSE;

	if(!cache)
		return OpStatus::ERR_NULL_POINTER;

	int len, http_len, headers_len;

	if(!embedded && file_name.IsEmpty())
		return OpStatus::OK;

	UINT32 len_rel_computed;

	RETURN_IF_ERROR(ComputeRecordLength(&(cache->header), len, http_len, headers_len, tag, embedded, len_rel_computed));

	// It can happen that Opera has URLs bigger than 64 KB, possibly for big headers or for data:// URLs.
	// They can't be saved as we normally use 16 bits for the length of the cache records, so if still save them, the cache
	// index will become corrupted.
	// To avoid this, at the moment, we simply skip them without writing them into the index.
	// For more details or ideas for solutions that would enable the cache to save them: CORE-39758
	if(len>=65530)
	{
		url_refused=TRUE;

		return OpStatus::OK;
	}

	RETURN_IF_ERROR(cache->WriteRecord(tag, len));
	
#ifdef _DEBUG
	int start_pos=cache->GetBytesWritten();
#endif
	
	// Write the main data
#ifndef SEARCH_ENGINE_CACHE
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_URL_NAME, &url, FALSE));
#endif
	
	RETURN_IF_ERROR(cache->WriteTime32Tag(TAG_LAST_VISITED, last_visited));
	
	RETURN_IF_ERROR(cache->WriteBoolTag(TAG_FORM_REQUEST, form_query));
	
	//// Relatives ///
	// Used to put a maximum on the relative entries
	UINT32 len_rel=0;
	UINT32 start_rel=cache->GetBytesWritten();
	UINT32 end_rel=start_rel;
	
	for(int rel=0, rel_num=ar_relative_vec.GetCount(); rel<rel_num && len_rel<RELATIVE_MAX_SIZE; rel++)
	{
		RelativeEntry *entry=ar_relative_vec.Get(rel);
		
		if(entry)
		{
			RETURN_IF_ERROR(entry->WriteValues(cache));
			
			end_rel=cache->GetBytesWritten();
			len_rel+=end_rel-start_rel;
			start_rel=end_rel;
		}
	}
	
	OP_ASSERT(len_rel_computed==len_rel);
	
	RETURN_IF_ERROR(cache->Write32Tag(TAG_STATUS, status));
	RETURN_IF_ERROR(cache->Write32Tag(TAG_CACHE_LOADED_FROM_NETTYPE, loaded_from_net_type));
	
	RETURN_IF_ERROR(cache->WriteBoolTag(TAG_CACHE_ALWAYS_VERIFY, always_verify));
	RETURN_IF_ERROR(cache->WriteBoolTag(TAG_CACHE_MULTIMEDIA, multimedia));
	RETURN_IF_ERROR(cache->WriteBoolTag(TAG_CACHE_CONTENT_TYPE_WAS_NULL, content_type_was_null));
	RETURN_IF_ERROR(cache->WriteBoolTag(TAG_CACHE_MUST_VALIDATE, must_validate));
	
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_CONTENT_TYPE, &content_type, FALSE));
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_CHARSET, &charset, FALSE));
	RETURN_IF_ERROR(cache->Write64Tag(TAG_CONTENT_SIZE, content_size));
	if(container_id)
		RETURN_IF_ERROR(cache->Write8Tag(TAG_CACHE_CONTAINER_INDEX, container_id));
	RETURN_IF_ERROR(cache->WriteTime64Tag(TAG_LOCALTIME_LOADED, local_time));
	
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_CACHE_SERVER_CONTENT_TYPE, &server_content_type, FALSE));

#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	RETURN_IF_ERROR(cache->WriteBoolTag(TAG_CACHE_NEVER_FLUSH, never_flush));
#endif

#ifdef URL_ENABLE_ASSOCIATED_FILES
	if(assoc_files)
		RETURN_IF_ERROR(cache->WriteTime32Tag(TAG_ASSOCIATED_FILES, assoc_files));
#endif
	
	RETURN_IF_ERROR(cache->WriteTime64Tag(TAG_HTTP_KEEP_EXPIRES, expiry));
	
	// Write the HTTP Record
	#ifdef _DEBUG
		UINT32 start_http=cache->GetBytesWritten();
	#endif
	RETURN_IF_ERROR(http.WriteValues(cache, http_len, headers_len));
	
	#ifdef _DEBUG
		OP_ASSERT(!http_len || (UINT32)http_len+cache->header.GetTagLength()+cache->header.GetLengthLength() == cache->GetBytesWritten()-start_http);
	#endif
	
	if(tag==URL_CACHE_USERFILE)
	{
		RETURN_IF_ERROR(cache->WriteBoolTag(TAG_CACHE_DOWNLOAD_FILE, download));
		RETURN_IF_ERROR(cache->Write32Tag(TAG_DOWNLOAD_RESUMABLE_STATUS, resumable));
		RETURN_IF_ERROR(cache->WriteStringTag(TAG_DOWNLOAD_FTP_MDTM_DATE, &ftp_mdtm_date, FALSE));
	}
#ifndef SEARCH_ENGINE_CACHE
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_CACHE_FILENAME, &file_name, FALSE)); // Embedded files does not have a name
#endif

	OP_ASSERT((!embedded && embedded_content_size==0) || (embedded && embedded_content_size>0));
	
	if(embedded && embedded_content_size>0)
		RETURN_IF_ERROR(cache->WriteBufTag(TAG_CACHE_EMBEDDED_CONTENT, embedded_content, embedded_content_size));
	
	//RETURN_IF_ERROR(cache->Write32Tag(TAG_CONTENT_SIZE, content_size));
	//RETURN_IF_ERROR(cache->Write32Tag(TAG_LOCALTIME_LOADED, content_size));
	//;
	
	// Add Dynamic attributes
	for(int dyn=0, num_dyn=ar_dynamic_att.GetCount(); dyn<num_dyn; dyn++)
	{
		StreamDynamicAttribute *att=ar_dynamic_att.Get(dyn);
		
		if(att)
			RETURN_IF_ERROR(cache->WriteDynamicAttributeTag(att));
	}
	
	
#ifdef _DEBUG
	int new_pos=cache->GetBytesWritten();
	
	OP_ASSERT(new_pos==start_pos+len);
#endif	
	
	return OpStatus::OK;
}

OP_STATUS DiskCacheEntry::ReserveSpaceForEmbeddedContent(UINT32 size)
{
	OP_ASSERT(!embedded_content && !embedded_content_size);
	OP_ASSERT(size<=CACHE_SMALL_FILES_SIZE);
	
	embedded_content_size=0;
	embedded_content_size_reserved=0;
	
	if(embedded_content)
		OP_DELETEA(embedded_content);
	
	embedded_content=OP_NEWA(unsigned char, size);
	
	if(!embedded_content)
		return OpStatus::ERR_NO_MEMORY;
		
	embedded_content_size_reserved=size;
		
	return OpStatus::OK;
}


void HTTPCacheEntry::SetIntValueByTag(UINT32 tag_id, UINT32 value)
{
	switch(tag_id)
	{
		case TAG_HTTP_RESPONSE:
			response_code=value;
			break;
			
		case TAG_HTTP_AGE:
			age=value;
			break;
		
		case TAG_USERAGENT_ID:
			agent_id=value;
			break;
			
		case TAG_USERAGENT_VER_ID:
			agent_ver_id=value;
			break;
			
		case TAG_HTTP_REFRESH_INT:
			refresh_int=value;
			break;
			
		default:
			OP_ASSERT(FALSE);
	}
}

OP_STATUS HTTPCacheEntry::SetStringValueByTag(UINT32 tag_id, const OpStringC8 *str)
{
	OP_ASSERT(str);
	
	if(!str)
		return OpStatus::ERR_NULL_POINTER;
		
	switch(tag_id)
	{
		case TAG_HTTP_KEEP_LOAD_DATE:
			return keep_load_date.Set(str->CStr());
			
		case TAG_HTTP_KEEP_LAST_MODIFIED:
			return keep_last_modified.Set(str->CStr());
			
		case TAG_HTTP_KEEP_ENCODING:
			return keep_encoding.Set(str->CStr());
			
		case TAG_HTTP_KEEP_ENTITY_TAG:
			return keep_entity.Set(str->CStr());
			
		case TAG_HTTP_MOVED_TO_URL:
			return moved_to.Set(str->CStr());
			
		case TAG_HTTP_RESPONSE_TEXT:
			return response_text.Set(str->CStr());
			
		case TAG_HTTP_REFRESH_URL:
			return refresh_url.Set(str->CStr());
			
		case TAG_HTTP_CONTENT_LOCATION:
			return location.Set(str->CStr());
			
		case TAG_HTTP_CONTENT_LANGUAGE:
			return language.Set(str->CStr());
			
		default:
			OP_ASSERT(FALSE);
			
			return OpStatus::ERR_NO_SUCH_RESOURCE;
	}
}
////////////////////////////////////////////////////////////////////////////
/////// ComputeRecordLength() and WriteValues MUST BE SYNCHRONIZED
////////////////////////////////////////////////////////////////////////////
OP_STATUS HTTPCacheEntry::WriteValues(OpConfigFileWriter *cache, int expected_len, int headers_len)
{
	OP_ASSERT(cache);
	#ifdef _DEBUG
		int l=0, l2;
		
		ComputeRecordLength(&cache->header, l, l2);
		OP_ASSERT(expected_len==l);
	#endif
	
	if(!cache)
		return OpStatus::ERR_NULL_POINTER;
		
	RETURN_IF_ERROR(cache->WriteRecord(TAG_HTTP_PROTOCOL_DATA, expected_len));
#ifdef _DEBUG
	int start_pos=cache->GetBytesWritten();
#endif
	
	RETURN_IF_ERROR(cache->Write32Tag(TAG_USERAGENT_ID, agent_id));
	RETURN_IF_ERROR(cache->Write32Tag(TAG_USERAGENT_VER_ID, agent_ver_id));
	RETURN_IF_ERROR(cache->Write32Tag(TAG_HTTP_RESPONSE, response_code));
	RETURN_IF_ERROR(cache->Write32Tag(TAG_HTTP_AGE, age));
	RETURN_IF_ERROR(cache->Write32Tag(TAG_USERAGENT_ID, agent_id));
	RETURN_IF_ERROR(cache->Write32Tag(TAG_USERAGENT_VER_ID, agent_ver_id));
	
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_HTTP_KEEP_LOAD_DATE, &keep_load_date, FALSE));
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_HTTP_KEEP_LAST_MODIFIED, &keep_last_modified, FALSE));
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_HTTP_KEEP_ENCODING, &keep_encoding, FALSE));
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_HTTP_KEEP_ENTITY_TAG, &keep_entity, FALSE));
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_HTTP_MOVED_TO_URL, &moved_to, FALSE));
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_HTTP_RESPONSE_TEXT, &response_text, FALSE));
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_HTTP_REFRESH_URL, &refresh_url, FALSE));
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_HTTP_CONTENT_LOCATION, &location, FALSE));
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_HTTP_CONTENT_LANGUAGE, &language, FALSE));
	
	if(!refresh_url.IsEmpty())
		RETURN_IF_ERROR(cache->Write32Tag(TAG_HTTP_REFRESH_INT, refresh_int));

		RETURN_IF_ERROR(cache->WriteStringTag(TAG_HTTP_SUGGESTED_NAME, &suggested, FALSE));
	
	for(UINT32 i=0; i<ar_link_rel.GetCount(); i++)
		RETURN_IF_ERROR(cache->WriteStringTag(TAG_HTTP_LINK_RELATION, ar_link_rel.Get(i), FALSE));

	UINT32 headers=GetHeadersCount();
	
	if(headers>0)
	{
		cache->WriteRecord(TAG_HTTP_RESPONSE_HEADERS, headers_len);
		 
		for(UINT32 j=0; j<headers; j++)
		{
			HTTPHeaderEntry *h=GetHeader(j);
			
			if(h)
			  RETURN_IF_ERROR(h->WriteValues(cache));
		}
	}

#ifdef _DEBUG
	int new_pos=cache->GetBytesWritten();
	
	OP_ASSERT(new_pos==start_pos+expected_len);
#endif	
	
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////
/////// ComputeRecordLength() and WriteValues MUST BE SYNCHRONIZED
////////////////////////////////////////////////////////////////////////////
OP_STATUS HTTPHeaderEntry::WriteValues(OpConfigFileWriter *cache)
{
	OpConfigFileHeader *header=&cache->header;
	
	RETURN_IF_ERROR(cache->WriteRecord(TAG_HTTP_RESPONSE_ENTRY, GetStringLen(header, &name, TRUE)+GetStringLen(header, &value, TRUE)));
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_HTTP_RESPONSE_HEADER_NAME, &name, TRUE));
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_HTTP_RESPONSE_HEADER_VALUE, &value, TRUE));
	
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////
/////// ComputeRecordLength() and WriteValues MUST BE SYNCHRONIZED
////////////////////////////////////////////////////////////////////////////
OP_STATUS RelativeEntry::WriteValues(OpConfigFileWriter *cache)
{
	OpConfigFileHeader *header=&cache->header;
	
	RETURN_IF_ERROR(cache->WriteRecord(TAG_RELATIVE_ENTRY, GetStringLen(header, &name, TRUE)+GetTime32Len(header, 1)));
	RETURN_IF_ERROR(cache->WriteStringTag(TAG_RELATIVE_NAME, &name, TRUE));
	RETURN_IF_ERROR(cache->WriteTime32Tag(TAG_RELATIVE_VISITED, visited));
	
	return OpStatus::OK;
}

OP_STATUS RelativeEntry::SetValues(OpStringC8 *rel_name, time_t rel_visited)
{
	if(!rel_name)
		return OpStatus::ERR_NULL_POINTER;
		
	RETURN_IF_ERROR(name.Set(rel_name->CStr()));
	visited=rel_visited;
	
	return OpStatus::OK;
}
#endif // CACHE_FAST_INDEX
