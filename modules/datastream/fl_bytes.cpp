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

#ifdef DATASTREAM_BYTEARRAY_ENABLED

#include "modules/datastream/fl_lib.h"

#include "modules/util/cleanse.h"


DataStream_ByteArray::DataStream_ByteArray()
{
	SetMicroBuffer(micro_payload, sizeof(micro_payload));
}

DataStream_ByteArray::DataStream_ByteArray(unsigned char *buf, uint32 len)
: DataStream_ByteArray_Base(buf, len)
{
	SetMicroBuffer(micro_payload, sizeof(micro_payload));
}

DataStream_ByteArray::~DataStream_ByteArray()
{
	if(g_ByteArray_Cleanse_Policy == DataStream_Sensitive_Array)
		OPERA_cleanse_heap(micro_payload, sizeof(micro_payload));
	else
		op_memset(micro_payload, 0, sizeof(micro_payload));
}

DataStream_ByteArray_Base::buffer_item::buffer_item()
:payload(NULL),length(0), next(NULL)
{
}

DataStream_ByteArray_Base::buffer_item::~buffer_item()
{
	Clear();
}

void DataStream_ByteArray_Base::buffer_item::Clear()
{
	OpStatus::Ignore(Resize(0));
	while(next)
	{
		buffer_item *item = next;

		next = item->next;
		item->next = NULL;
		OP_DELETE(item);
	}
	next = NULL;
}

OP_STATUS DataStream_ByteArray_Base::buffer_item::Set(const unsigned char *data1, uint32 len1, const unsigned char *data2, uint32 len2)
{
	OP_ASSERT(payload == NULL);

	if(!data1)
		len1 = 0;
	else if(len1 == 0)
		data1 = NULL;
	if(!data2)
		len2 = 0;
	else if(len2 == 0)
		data2 = NULL;

	uint32 len= len1 +len2;

	if(len == 0)
		return OpRecStatus::ERR_OUT_OF_RANGE;

	RETURN_IF_ERROR(Resize(len));

	if(data1)
	{
		op_memcpy(payload, data1, len1);
	}
	if(data2)
	{
		op_memcpy(payload+len1, data2, len2);
	}
	length = len;

	return OpRecStatus::OK;
}

OP_STATUS DataStream_ByteArray_Base::buffer_item::Resize(uint32 len)
{
	if (len!=0 && len == length)
		return OpStatus::OK;

	unsigned char *new_payload = NULL;
	
	if(len > 0)
	{
		new_payload = OP_NEWA(unsigned char, len);
		if(new_payload == NULL)
			return OpRecStatus::ERR_NO_MEMORY;

		if(payload)
			op_memcpy(new_payload, payload, (len < length ? len : length));

		if(len > length)
			op_memset(new_payload+length , 0 , len - length);
	}

	if(payload)
	{
		if(g_ByteArray_Cleanse_Policy == DataStream_Sensitive_Array)
			OPERA_cleanse_heap(payload, length);
		else
			op_memset(payload, 0, length);

		OP_DELETEA(payload);
	}

	payload = new_payload;
	length = len;

	return OpStatus::OK;
}



DataStream_ByteArray_Base::DataStream_ByteArray_Base()
{
	InternalInit(NULL, 0);
}

DataStream_ByteArray_Base::DataStream_ByteArray_Base(unsigned char *buf, uint32 len)
{
	InternalInit(buf, len);
}

void DataStream_ByteArray_Base::InternalInit(unsigned char *buf, uint32 len)
{
	micro_payload = NULL;
	micro_payload_length = 0;
	payload_length = 0;
	max_payload_length = 0;
	payload_mode = NO_PAYLOAD;
	small_payload_length = 0;
	payload_sequence = NULL;
	payload_sequence_end = NULL;
	read_item = 0;
	read_pos = 0;
	write_pos = 0;
	need_direct_access = FALSE;
	fixed_size = FALSE;
	is_a_segment = FALSE;
	is_a_fifo = FALSE;
	dont_alloc_extra = FALSE;
	is_sensitive = (g_ByteArray_Cleanse_Policy == DataStream_Sensitive_Array ? TRUE : FALSE);

	if(buf)
	{
		SetNeedDirectAccess(TRUE);
		SetFixedSize(TRUE);
		SetExternalPayload(buf);
		payload_length = max_payload_length = len;
	}
}

void DataStream_ByteArray_Base::SetMicroBuffer(unsigned char *micro_buf, uint32 mbuf_len)
{
	if(micro_payload != NULL || micro_buf == NULL || mbuf_len == 0)
		return;

	micro_payload = micro_buf;
	micro_payload_length = mbuf_len;
}


DataStream_ByteArray_Base::~DataStream_ByteArray_Base()
{
	micro_payload = NULL;
	micro_payload_length = 0;
	if(payload_mode == MICRO_PAYLOAD)
	{
		payload_mode = NO_PAYLOAD;
		small_payload.payload = NULL;
		max_payload_length = payload_length = 0;
	}

	ClearPayload();
}

void DataStream_ByteArray_Base::ClearPayload()
{
	switch(payload_mode)
	{
	case MICRO_PAYLOAD:
		if(is_sensitive || g_ByteArray_Cleanse_Policy == DataStream_Sensitive_Array)
			OPERA_cleanse_heap(micro_payload, micro_payload_length);
		else
			op_memset(micro_payload, 0, micro_payload_length);
		break;
		
	case EXTERNAL_PAYLOAD:
		/* Owner must clear this memory, it may point into constant memory */
		small_payload.Release();
		break;
		
	case LARGE_PAYLOAD:
		{
			DataStream_Conditional_Sensitive_ByteArray(large, is_sensitive);
			OP_DELETE(payload_sequence); // Automatic delete of chain
			read_item = payload_sequence = payload_sequence_end = NULL;
			payload_length = 0;
			small_payload_length = 0;
		}
		// Fall through to small payload
	case SMALL_PAYLOAD:
	case EXTERNAL_PAYLOAD_OWN:
		{
			DataStream_Conditional_Sensitive_ByteArray(small, is_sensitive);
			small_payload.Resize(0);
		}
		break;

	}

	read_pos = 0;
	read_item = NULL;
	payload_length = 0;
	max_payload_length = 0;
	write_pos = 0;
	payload_mode = NO_PAYLOAD;
}

void DataStream_ByteArray_Base::SetExternalPayload(unsigned char *src, BOOL take_over, uint32 len)
{
	if(payload_mode != NO_PAYLOAD)
		ClearPayload();
	
	if(src == NULL)
	{
		small_payload.payload = NULL;
		payload_mode = NO_PAYLOAD;
		return;
	}

	small_payload.payload = src;
	payload_mode = (take_over ? EXTERNAL_PAYLOAD_OWN : EXTERNAL_PAYLOAD);
	read_pos = 0;
	read_item = NULL;
	payload_length = (len ? len : max_payload_length);
	write_pos = 0;
}

OP_STATUS DataStream_ByteArray_Base::Resize(uint32 _len, BOOL alloc, BOOL set_length)
{
	if(max_payload_length && _len == max_payload_length)
		return OpStatus::OK; // No need to do anything;
	
	if(_len == 0)
	{
		ClearPayload();
		max_payload_length = payload_length = 0;
		return  OpStatus::OK;
	}

	switch(payload_mode)
	{
	case EXTERNAL_PAYLOAD:
		/* Clean up must be done by owner, This pointer MAY point into constant memory*/
		break;
	case EXTERNAL_PAYLOAD_OWN:
		if(_len > payload_length)
		{
			op_memset(small_payload.payload + payload_length, 0, _len - payload_length);
			break;
		}
		break;
	default:
		if(_len < payload_length)
		{
			op_memset((payload_mode != MICRO_PAYLOAD ? small_payload.payload : micro_payload) + _len, 0, payload_length - _len);
		}
		else if(alloc && (payload_mode != MICRO_PAYLOAD || _len > micro_payload_length))
		{
			RETURN_IF_ERROR(InternalResize(SMALL_PAYLOAD, _len));
		}
	}

	if(fixed_size)
	{
		payload_length = _len;
		max_payload_length = payload_length;
	}
	else
	{
		max_payload_length = 0;
		if(set_length || payload_length > _len )
			payload_length  = _len;
	}
	if(write_pos > payload_length)
		write_pos = payload_length;
	if(read_pos > payload_length)
		read_pos = payload_length;

	return OpStatus::OK;
}

OP_STATUS DataStream_ByteArray_Base::InternalResize(en_payload_mode new_mode, uint32 new_len)
{
	OP_ASSERT(new_mode != NO_PAYLOAD); // Not supporte, use ClearPayload();
	OP_ASSERT(payload_mode != LARGE_PAYLOAD || new_mode == LARGE_PAYLOAD); // Large payload may not be changed to smaller payload

	switch(new_mode)
	{
	case EXTERNAL_PAYLOAD:
	case EXTERNAL_PAYLOAD_OWN:
	case MICRO_PAYLOAD:
		break; // do nothing
	case SMALL_PAYLOAD:
		{
			DataStream_Conditional_Sensitive_ByteArrayL(small, is_sensitive);
			RETURN_IF_ERROR(small_payload.Resize(new_len));
			if(payload_mode == MICRO_PAYLOAD)
				op_memcpy(small_payload.payload, micro_payload, (payload_length< new_len ? payload_length : new_len));
		}
		break;
	case LARGE_PAYLOAD:
		{
			DataStream_Conditional_Sensitive_ByteArrayL(large, is_sensitive);
			small_payload_length  = payload_length;
			read_item = NULL;
			RETURN_IF_ERROR(small_payload.Resize(FL_MAX_SMALL_PAYLOAD));
			if(payload_mode == MICRO_PAYLOAD)
				op_memcpy(small_payload.payload, micro_payload, (payload_length< new_len ? payload_length : new_len));
		}
		break;
	}
	payload_mode = new_mode;
	return  OpStatus::OK;
}

unsigned char *DataStream_ByteArray_Base::GetDirectPayload()
{
	return (need_direct_access ? (payload_mode != MICRO_PAYLOAD ? small_payload.payload : micro_payload) + (is_a_fifo ? read_pos : 0) : NULL);
}

const unsigned char *DataStream_ByteArray_Base::GetDirectPayload() const
{
	return (need_direct_access ? (payload_mode != MICRO_PAYLOAD ? small_payload.payload : micro_payload) + (is_a_fifo ? read_pos : 0) : NULL);
}

unsigned char *DataStream_ByteArray_Base::ReleaseL()
{
	if(!need_direct_access)
		DS_LEAVE(OpRecStatus::ERR);

	if(payload_mode == MICRO_PAYLOAD)
	{
		LEAVE_IF_ERROR(small_payload.Set(micro_payload, payload_length));
		op_memset(micro_payload, 0, payload_length);
	}

	unsigned char *ret = small_payload.Release();

	payload_mode = NO_PAYLOAD;
	ClearPayload();

	return ret;
}

BOOL DataStream_ByteArray_Base::SetReadPos(uint32 _pos)
{
	if(payload_mode == NO_PAYLOAD || 
		(payload_mode == SMALL_PAYLOAD && small_payload.payload == NULL))
		return FALSE;

	if(payload_mode == LARGE_PAYLOAD)
	{
		if(payload_length - small_payload_length <= _pos)
		{
			// In the current scratch buffer
			read_item = NULL;
			// If past end, position just past end
			read_pos = (payload_length <=_pos ? small_payload_length : _pos - (payload_length - small_payload_length));
			return TRUE;
		}

		buffer_item *item = payload_sequence;

		while(_pos > 0 && item)
		{
			if(_pos < item->length)
				break;
			_pos -= item->length;
			item = item->next;
		}

		OP_ASSERT(item);
		read_item = item;
		read_pos = _pos;
	}
	else
		read_pos = (_pos <= payload_length ? _pos : payload_length);

	return TRUE;
}

BOOL DataStream_ByteArray_Base::SetWritePos(uint32 _pos)
{
	OP_ASSERT(need_direct_access);

	if(_pos != 0 && 
		(payload_mode == NO_PAYLOAD || 
		(payload_mode == SMALL_PAYLOAD && small_payload.payload == NULL) || 
			_pos > payload_length))
		return FALSE;


	if(payload_mode != LARGE_PAYLOAD) // Large payloads always append
	{
		write_pos = _pos;
	}
	return TRUE;
}

void DataStream_ByteArray_Base::ExportDataL(DataStream *dst)
{
	DS_DEBUG_Start();

	unsigned long read_len = 0;

	switch(payload_mode)
	{
	case MICRO_PAYLOAD:
	case SMALL_PAYLOAD:
	case EXTERNAL_PAYLOAD:
	case EXTERNAL_PAYLOAD_OWN:
		{
			unsigned char *r_buffer = (payload_mode == MICRO_PAYLOAD ? micro_payload : small_payload.payload) + read_pos;
			read_len = payload_length - read_pos;
			dst->WriteDataL(r_buffer, read_len);
			DS_Debug_Write("DataStream_ByteArray_Base::ExportDataL", "Export data", r_buffer, read_len);

			{
				read_pos += read_len; // commit position;
				if(is_a_fifo && read_pos > 0)
				{
					if(read_pos > 32 || read_pos == write_pos)
					{
						if(small_payload.payload || micro_payload)
						{
							OP_ASSERT(read_pos <= write_pos);
							unsigned char *move_buffer = small_payload.payload ? small_payload.payload : micro_payload;
							op_memmove(move_buffer, move_buffer+read_pos, write_pos-read_pos);
						}
						write_pos -= read_pos;
						payload_length = write_pos;
						read_pos = 0;
					}
				}
			}
		}
		break;
	case LARGE_PAYLOAD:
		{
			if(read_item)
			{
				OP_ASSERT(read_item->payload);
				OP_ASSERT(read_item->length > read_pos);

				dst->WriteDataL(read_item->payload + read_pos, read_item->length-read_pos);
				DS_Debug_Write("DataStream_ByteArray_Base::ExportDataL", "Export data", read_item->payload + read_pos, read_item->length-read_pos);
				read_item = read_item->next;
				read_pos = 0;
			}

			while(read_item)
			{
				OP_ASSERT(read_item->payload);

				dst->WriteDataL(read_item->payload, read_item->length);
				DS_Debug_Write("DataStream_ByteArray_Base::ExportDataL", "Export data", read_item->payload + read_pos, read_item->length-read_pos);
				read_item = read_item->next;
			}

			if(small_payload_length)
			{
				OP_ASSERT(small_payload.payload);
				dst->WriteDataL(small_payload.payload+ read_pos, small_payload_length-read_pos);
				
				DS_Debug_Write("DataStream_ByteArray_Base::ExportDataL", "Export data", small_payload.payload+ read_pos, small_payload_length-read_pos);
				read_pos = small_payload_length;
			}
		}
	}
}

unsigned long DataStream_ByteArray_Base::ReadDataL(unsigned char *buffer, unsigned long len, DataStream::DatastreamCommitPolicy commit)
{
	DS_DEBUG_Start();

	unsigned long read_len = 0;

	/*if(buffer == NULL)
		return read_len;*/

	switch(payload_mode)
	{
	case MICRO_PAYLOAD:
	case SMALL_PAYLOAD:
	case EXTERNAL_PAYLOAD:
	case EXTERNAL_PAYLOAD_OWN:
		{
			read_len = len;
			if(read_len + read_pos > payload_length)
			{
				if((unsigned long) read_pos >= payload_length)
				{
					read_len = 0;
					break;
				}
				
				read_len = payload_length- read_pos;
			}

			if(buffer != NULL)
			{
				unsigned char *r_buffer = (payload_mode == MICRO_PAYLOAD ? micro_payload : small_payload.payload) + read_pos;

				switch(read_len)
				{
				default:
					op_memcpy(buffer, r_buffer , read_len);
					break;
				case 4:
					*(buffer++) = *(r_buffer++);
				case 3:
					*(buffer++) = *(r_buffer++);
				case 2:
					*(buffer++) = *(r_buffer++);
				case 1:
					*(buffer++) = *(r_buffer++);
				// !!NOTE!!: No new lines below this line
				}
			}
			if(commit == KReadAndCommit || (commit == KReadAndCommitOnComplete && read_len == len))
			{
				read_pos += read_len; // commit position;
				if(is_a_fifo && read_pos > 0)
				{
					if(read_pos > 32 || read_pos == write_pos)
					{
						if(small_payload.payload || micro_payload)
						{
							OP_ASSERT(read_pos <= write_pos);
							unsigned char *move_buffer = small_payload.payload ? small_payload.payload : micro_payload;
							op_memmove(move_buffer, move_buffer+read_pos, write_pos-read_pos);
						}
						write_pos -= read_pos;
						payload_length = write_pos;
						read_pos = 0;
					}
				}
			}
		}
		break;
	case LARGE_PAYLOAD:
		{
			size_t buf_len;
			buffer_item *t_read_item = read_item;
			uint32 t_read_pos = read_pos;

			while(read_len < len && t_read_item)
			{
				OP_ASSERT(t_read_item->payload);
				OP_ASSERT(t_read_item->length > t_read_pos);

				buf_len = t_read_item->length - t_read_pos;
				if(buf_len > len - read_len)
					buf_len = len - read_len;

				op_memcpy(buffer+read_len, t_read_item->payload + t_read_pos, buf_len);

				read_len += buf_len;
				t_read_pos += buf_len;

				if(t_read_pos >= t_read_item->length)
				{
					t_read_item = t_read_item->next;
					t_read_pos = 0;
				}
			}

			if(read_len < len && small_payload_length)
			{
				OP_ASSERT(small_payload.payload);

				buf_len = small_payload_length-t_read_pos;
				if(buf_len > len - read_len)
					buf_len = len - read_len;

				op_memcpy(buffer+read_len, small_payload.payload+ t_read_pos, buf_len);

				read_len += buf_len;
				t_read_pos += buf_len;
			}

			if(commit == KReadAndCommit || (commit == KReadAndCommitOnComplete && read_len == len))
			{
				OP_ASSERT(!is_a_fifo);
				// Commit position
				read_item = t_read_item;
				read_pos = t_read_pos;
			}
		}
	}

	DS_Debug_Write("DataStream_ByteArray_Base::InternalReadDataL", (commit ?  "Read data" : "Sampled Data"), buffer, read_len);
	return read_len;
}

void DataStream_ByteArray_Base::WriteDataL(const unsigned char *buffer, unsigned long len)
{
	DS_DEBUG_Start();

	DS_Debug_Write("DataStream_ByteArray_Base::WriteDataL", "Writing data", buffer, len);
	if(buffer == NULL || len == 0)
		return;

	// Switch payload mode if necessary
	switch(payload_mode)
	{
	case NO_PAYLOAD:
		{
			payload_length= 0;
			if(micro_payload && len <= micro_payload_length)
				payload_mode = MICRO_PAYLOAD;
			else if(len <= FL_MAX_SMALL_PAYLOAD || need_direct_access)
				payload_mode = SMALL_PAYLOAD;
			else
			{
				payload_mode = LARGE_PAYLOAD;
				OP_DELETE(payload_sequence); // Just to be on the safe side
				payload_sequence = payload_sequence_end = NULL;
			}
		}
		break;
	case MICRO_PAYLOAD :
		{
			unsigned long new_len = write_pos + len;
			if(new_len <= micro_payload_length || (max_payload_length && max_payload_length <= new_len && max_payload_length <= micro_payload_length ))
				break;
			else if(need_direct_access || new_len <= FL_MAX_SMALL_PAYLOAD)
			{
				// do complete concatenation
				uint32 new_small_max_len = (dont_alloc_extra ? new_len :
							(need_direct_access || new_len < FL_MAX_SMALL_PAYLOAD - 1024 ? new_len + 1024 : FL_MAX_SMALL_PAYLOAD));

				LEAVE_IF_ERROR(InternalResize(SMALL_PAYLOAD, new_small_max_len));
				break;
			}   
		}
		// else continue to next
	case SMALL_PAYLOAD:
		{
			unsigned long new_len = write_pos + len;
			if(!need_direct_access && small_payload.length < new_len && new_len > FL_MAX_SMALL_PAYLOAD)
			{
				// Just to be on the safe side
				OP_DELETE(payload_sequence); 
				payload_sequence = payload_sequence_end = NULL;

				LEAVE_IF_ERROR(InternalResize(LARGE_PAYLOAD, payload_length));
			}
		}
		break;
	}

	if(payload_mode != LARGE_PAYLOAD && max_payload_length && write_pos + len > max_payload_length)
		len = max_payload_length - write_pos;

	{
		byte *w_buffer = NULL;

		// Perform addition of data
		switch(payload_mode)
		{
		case MICRO_PAYLOAD :
			{
				w_buffer = micro_payload+write_pos;
			}
			break;
			
		case SMALL_PAYLOAD:
			{
				unsigned long new_len = write_pos + len;
				
				if(new_len > small_payload.length)
				{
					uint32 new_small_max_len = (dont_alloc_extra ? new_len :
							(need_direct_access || new_len < FL_MAX_SMALL_PAYLOAD - 1024 ? new_len + 1024 : FL_MAX_SMALL_PAYLOAD));
					
					LEAVE_IF_ERROR(InternalResize(SMALL_PAYLOAD, new_small_max_len));
				}
			}
			
			// Continue with buffer copy
		case EXTERNAL_PAYLOAD:
		case EXTERNAL_PAYLOAD_OWN:
			{
				w_buffer = small_payload.payload +write_pos;
			}
			break;
		case LARGE_PAYLOAD:
			{
				unsigned long new_len = small_payload_length + len;
				if(new_len >= small_payload.length)
				{
					OpStackAutoPtr<buffer_item> new_item(OP_NEW_L(buffer_item, ()));

					LEAVE_IF_ERROR(new_item->Set(small_payload.payload, small_payload_length, buffer, len));

					if(!payload_sequence)
						payload_sequence = payload_sequence_end = new_item.release();
					else
					{
						OP_ASSERT(payload_sequence_end);
						payload_sequence_end->next = new_item.get();
						payload_sequence_end = new_item.release();
					}
					if(read_item == NULL)
						read_item = payload_sequence_end; // position is the same
					small_payload_length = 0;
					payload_length += len;
					return;
				}
				LEAVE_IF_ERROR(small_payload.Resize(payload_length > 8*FL_MAX_SMALL_PAYLOAD ? 4*FL_MAX_SMALL_PAYLOAD: FL_MAX_SMALL_PAYLOAD));
				w_buffer = small_payload.payload + small_payload_length;
				OP_ASSERT(small_payload_length + len <= small_payload.length);
			}
			break;
		}

		if(w_buffer == NULL)
			return;

		switch(len)
		{
		default:
			op_memcpy(w_buffer, buffer , len);
			break;
		case 4:
			*(w_buffer++) = *(buffer++);
		case 3:
			*(w_buffer++) = *(buffer++);
		case 2:
			*(w_buffer++) = *(buffer++);
		case 1:
			*(w_buffer++) = *(buffer++);
		}
		
		if(payload_mode != LARGE_PAYLOAD)
		{
			write_pos += len;
			if(write_pos > payload_length)
				payload_length = write_pos;
		}
		else
		{
			payload_length += len;
			small_payload_length += len;
		}
	}
}

#ifdef DATASTREAM_USE_FLEXIBLE_SEQUENCE
int DataStream_ByteArray_Base::operator ==(const DataStream_ByteArray_Base &other) const 
{
	if(payload_length != other.payload_length)
		return FALSE;

	if(payload_length == 0)
		return TRUE;

	if(payload_mode != LARGE_PAYLOAD && other.payload_mode !=  LARGE_PAYLOAD)
	{
		const byte *this_src = (payload_mode == MICRO_PAYLOAD ? micro_payload : small_payload.payload);
		const byte *other_src = (other.payload_mode == MICRO_PAYLOAD ? other.micro_payload : other.small_payload.payload);

		return (op_memcmp(this_src, other_src, payload_length) == 0);
	}

	{
		// Always a large payload
		const DataStream_ByteArray_Base *src1 = (payload_mode == LARGE_PAYLOAD || other.payload_mode != LARGE_PAYLOAD ? this : &other);

		// Either a small or a large payload
		const DataStream_ByteArray_Base *src2 = (src1 == this ? &other : this);

		buffer_item *item1 = src1->payload_sequence;
		buffer_item *item2 = src2->payload_sequence;
		size_t read_pos1 = 0;
		size_t read_pos2 = 0;
		const byte *srcbuf_small2 = src2->payload_mode == MICRO_PAYLOAD ? src2->micro_payload : src2->small_payload.payload;
		uint32 compare_len=0;

		OP_ASSERT(src2->payload_mode == LARGE_PAYLOAD || srcbuf_small2 != NULL);

		while(item1)
		{
			uint32 compare_len = item1->length - read_pos1;
			if(item2)
			{
				if(compare_len > item2->length - read_pos2)
					compare_len = item2->length - read_pos2;
				OP_ASSERT(compare_len);

				if(op_memcmp(item1->payload + read_pos1, item2->payload + read_pos2, compare_len) != 0)
					return FALSE;

				read_pos2 += compare_len;
				if(read_pos2 >= item2->length)
				{
					item2 = item2->next;
					read_pos2 = 0;
				}
			}
			else
			{
				OP_ASSERT(compare_len);
				if(op_memcmp(item1->payload + read_pos1, srcbuf_small2 + read_pos2, compare_len) != 0)
					return FALSE;

				read_pos2 += compare_len;
			}

			read_pos1 += compare_len;
			if(read_pos1 >= item1->length)
			{
				item1 = item1->next;
				read_pos1 = 0;
			}
		}

		while(item2)
		{
			OP_ASSERT(src1->small_payload.payload);

			compare_len = item2->length - read_pos2;
			OP_ASSERT(compare_len);

			if(op_memcmp(src1->small_payload.payload + read_pos1, item2->payload + read_pos2, compare_len) != 0)
				return FALSE;

			read_pos1 += compare_len;
			read_pos2 += compare_len;
			if(read_pos2 >= item2->length)
			{
				item2 = item2->next;
				read_pos2 = 0;
			}
		}

		if(src1->small_payload_length < read_pos1)
		{
			compare_len = src1->small_payload_length - read_pos1;

			if(op_memcmp(src1->small_payload.payload + read_pos1, srcbuf_small2 + read_pos2, compare_len) != 0)
				return FALSE;

			read_pos1 += compare_len;
			read_pos2 += compare_len;
		}
		
		OP_ASSERT ((src2->payload_mode != LARGE_PAYLOAD && read_pos2 == payload_length) ||
			(src2->payload_mode == LARGE_PAYLOAD && small_payload_length == read_pos2));  // Curious! Not matching lengths after all?

		return TRUE;
	}
}
#endif

void DataStream_ByteArray_Base::AddContentL(const byte *src, uint32 len)
{
	DS_DEBUG_Start();

	DS_Debug_Write("DataStream_ByteArray_Base::AddContentL", "byte array", src, len);
	WriteDataL(src, len);
}

uint32	DataStream_ByteArray_Base::GetAttribute(DataStream::DatastreamUIntAttribute attr)
{
	switch(attr)
	{
	case DataStream::KCalculateLength:
		return GetLength();
	case DataStream::KActive:
		return is_a_segment;
	case DataStream::KFinished:
		return (!max_payload_length || payload_length == max_payload_length);
	case DataStream::KMoreData:
		switch(payload_mode)
		{
		case MICRO_PAYLOAD:
		case SMALL_PAYLOAD:
		case EXTERNAL_PAYLOAD:
		case EXTERNAL_PAYLOAD_OWN:
			return ((unsigned long) read_pos < payload_length ? TRUE : FALSE);
			break;
		case LARGE_PAYLOAD:
			if(read_item)
				return TRUE;

			if(read_pos < small_payload_length)
				return TRUE;
		}

		return FALSE;
	default:
		return DataStream::GetAttribute(attr);
	}
}


#ifdef DATASTREAM_READ_RECORDS
OP_STATUS DataStream_ByteArray_Base::PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target)
{
	if(src_target == NULL)
		LEAVE(OpRecStatus::ERR_NULL_POINTER);

	switch(action)
	{
	case DataStream::KReadRecord:
		{
			if(src_target == NULL)
				LEAVE(OpRecStatus::ERR_NULL_POINTER);
			
			DataStream *source = src_target;

			if(!source)
				return OpRecStatus::ERR;
			
			if(max_payload_length == 0)
			{
				AddContentL(source);
				if(source->MoreData() || source->Active())
					return OpRecStatus::OK;
				
				return OpRecStatus::FINISHED;
			}
			
			uint32 rest_len = max_payload_length - write_pos;
			if(rest_len == 0)
				return OpRecStatus::FINISHED;
			
			AddContentL(source, rest_len, rest_len/20);
			
			if(write_pos < max_payload_length)
			{
				if(source->Active())
					return OpRecStatus::OK;
				
				return OpRecStatus::RECORD_TOO_SHORT;
			}
		}

		return OpRecStatus::FINISHED;

	case DataStream::KWriteRecord:
		{
			DS_DEBUG_Start(); 
			if(src_target == NULL)
				LEAVE(OpRecStatus::ERR_NULL_POINTER);

			DataStream *target = src_target;
			
			switch(payload_mode)
			{
			case MICRO_PAYLOAD:
			case SMALL_PAYLOAD:
			case EXTERNAL_PAYLOAD:
			case EXTERNAL_PAYLOAD_OWN:
				{
					DS_Debug_Write("DataStream_ByteArray_Base::WriteRecordL", "Writing content", (payload_mode == MICRO_PAYLOAD ? micro_payload : small_payload.payload), payload_length);
					target->WriteDataL((payload_mode == MICRO_PAYLOAD ? micro_payload : small_payload.payload), payload_length);
				}
				break;
			case LARGE_PAYLOAD:
				{
					buffer_item *item = payload_sequence;

					while(item)
					{
						DS_Debug_Write("DataStream_ByteArray_Base::WriteRecordL", "Writing content block", item->payload, item->length);
						target->WriteDataL(item->payload, item->length);
						item = item->next;
					}

					if(small_payload.payload)
					{
						DS_Debug_Write("DataStream_ByteArray_Base::WriteRecordL", "Writing content block", small_payload.payload, small_payload_length);
						target->WriteDataL(small_payload.payload, small_payload_length);
					}
				}
			}
		}
		return OpRecStatus::FINISHED;
	}

	return DataStream::PerformStreamActionL(action, src_target);
}
#endif

#ifdef DATASTREAM_ARRAY_ADD_CHAR_CONTENT
void DataStream_ByteArray_Base::AddContentL(const char *src)
{
	DS_DEBUG_Start();

	if(src == NULL)
		return;

	DS_Debug_Printf1("DataStream_ByteArray_Base::AddContenL: %s\n", src);

	AddContentL((const unsigned char *) src, op_strlen(src));
}

void DataStream_ByteArray_Base::AddContentL(OpStringC8 &src)
{
	DS_DEBUG_Start();

	DS_Debug_Printf1("DataStream_ByteArray_Base::AddContenL: %s\n", src.CStr());
	AddContentL((const unsigned char *) src.CStr(), src.Length());
}
#endif

#if 0
char *DataStream_ByteArray_Base::GetStringL()
{
	DS_DEBUG_Start();

	unsigned long len = GetLength();
	
	if(len == 0)
		return NULL;
	
	char *string = OP_NEWA_L(char, len+1);

	unsigned long len1 = ReadDataL((unsigned char *) string, len);
	string[len1] = '\0';
	//MAKE_DOUBLEBYTE((char *) string);

	DS_Debug_Printf1("DataStream_ByteArray_Base::GetStringL(char *): Retrieved: %s\n", string);

	return string;
}
#endif

void DataStream_ByteArray_Base::GetStringL(OpStringS8 &string)
{
	DS_DEBUG_Start();

	uint32 len = GetLength();

#ifdef WIN16
	if(len > 65530)
		len = 65530;
#endif
	string.Empty();
	if(len == 0)
		return;

	char *str = string.ReserveL(len+1);

	if(len > 0 && !ReadDataL((byte *) str, len))
	{
		string.Empty();
		return;
	}

	str[len] = '\0';

	DS_Debug_Printf1("DataStream_ByteArray_Base::GetStringL: Retrieved: %s\n", string.CStr());
}

void DataStream_ByteArray_Base::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	switch(action)
	{
	case DataStream::KCommitSampledBytes:
		{
			DS_DEBUG_Start();
			uint32 len= arg1;
			
			DS_Debug_Printf1("DataStream_ByteArray_Base::CommitSampledBytesL %u bytes\n", len);
			switch(payload_mode)
			{
			case MICRO_PAYLOAD:
			case SMALL_PAYLOAD:
			case EXTERNAL_PAYLOAD:
			case EXTERNAL_PAYLOAD_OWN:
				{
					//if((unsigned long) read_pos >= payload_length)
					//	break;
					
					read_pos += len;
					if((unsigned long) read_pos > payload_length)
						read_pos = payload_length;

					if(is_a_fifo && read_pos > 0)
					{
						if(small_payload.payload)
						{
							OP_ASSERT(read_pos <= write_pos);
							op_memmove(small_payload.payload, small_payload.payload+read_pos, write_pos-read_pos);
						}
						write_pos -= read_pos;
						payload_length = write_pos;
						read_pos = 0;
					}
				}
				break;
			case LARGE_PAYLOAD:
				OP_ASSERT(!is_a_fifo);
				while(read_item && len)
				{
					uint32 left = read_item->length - read_pos;

					if(len >= left)
					{
						read_pos = 0;
						read_item = read_item->next;
						len-= left;
					}
					else
					{
						read_pos += len;
						len = 0;
					}
				}

				if(len && !read_item && small_payload.payload && small_payload_length)
				{
					uint32 left = small_payload_length - read_pos;

					if(len >= left)
						read_pos = small_payload_length;
					else
						read_pos += len;
				}
				break;
			}
			break;
		}
	case DataStream::KFinishedAdding:
		if(payload_mode == LARGE_PAYLOAD && small_payload_length)
		{
			OpStackAutoPtr<buffer_item> new_item(OP_NEW_L(buffer_item, ()));

			LEAVE_IF_ERROR(new_item->Set(small_payload.payload, small_payload_length));

			if(!payload_sequence)
				payload_sequence = payload_sequence_end = new_item.release();
			else
			{
				OP_ASSERT(payload_sequence_end);
				payload_sequence_end->next = new_item.get();
				payload_sequence_end = new_item.release();
			}
			if(read_item == NULL)
				read_item = payload_sequence_end; // position is the same
			small_payload_length = 0;
			small_payload.Resize(0);
		}
		else if(payload_mode == SMALL_PAYLOAD && small_payload.length > payload_length + 128 )
		{
			LEAVE_IF_ERROR(InternalResize(SMALL_PAYLOAD,payload_length));
		}
		break;
	case DataStream::KResetRead:
		read_pos = 0;
		read_item = payload_sequence;
		break;
	case DataStream::KResetRecord:
		if(payload_mode == EXTERNAL_PAYLOAD || payload_mode == EXTERNAL_PAYLOAD_OWN || max_payload_length)
		{
			SetWritePos(0);
		}
		else
			ClearPayload();
		break;
	default: 
		DataStream::PerformActionL(action, arg1, arg2);
	}
}


/*
uint32 DataStream_ByteArray_Base::GetLength() const
{
	return payload_length;
}
*/

#ifdef _DATASTREAM_DEBUG_
void DataStream_ByteArray_Base::Debug_Write_All_Data()
{
	DS_DEBUG_Start();

	DS_Debug_Printf0("DataStream_ByteArray Starting dump\n");
	switch(payload_mode)
	{
	case MICRO_PAYLOAD:
		DS_Debug_Write("DataStream_ByteArray_Base", "micro payload", micro_payload , payload_length);
		break;
	case SMALL_PAYLOAD:
		DS_Debug_Write("DataStream_ByteArray_Base", "small payload", small_payload.payload, payload_length);
		break;
	case EXTERNAL_PAYLOAD:
	case EXTERNAL_PAYLOAD_OWN:
		DS_Debug_Write("DataStream_ByteArray_Base", "external payload", small_payload.payload, payload_length);
		break;
	case LARGE_PAYLOAD:
		{
			buffer_item *item = payload_sequence;

			while(item)
			{
				DS_Debug_Write("DataStream_ByteArray_Base", "Large Payload", item->payload, item->length);
				item = item->next;
			}

			if(small_payload.payload)
			{
				DS_Debug_Write("DataStream_ByteArray_Base::WriteRecordL", "Writing content block", small_payload.payload, small_payload_length);
			}
		}
	}
	DS_Debug_Printf0("DataStream_ByteArray_Base Ending dump\n");
}
#endif

#endif // DATASTREAM_BYTEARRAY_ENABLED
