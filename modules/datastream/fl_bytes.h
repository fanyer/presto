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


#ifndef __FL_BYTES_H__
#define __FL_BYTES_H__

#ifdef DATASTREAM_BYTEARRAY_ENABLED

#include "modules/datastream/fl_base.h"


/** Binary buffer DataStream implementation
 *
 *  Payload operations are only on binary arrays or Generic records
 *	Type specific operations must be added by the specific implementations.
 *
 *	Payload storage is 2 or 3-tier, with conditonal storage in an optional 
 *	buffer specified by the implementation , 16KB and 
 *  "unlimited" storage, depending on the size of the payload;
 *  The reason for this is to reduce allocation overhead (in MemoryHandler), and
 *  to reduce allocation of unnecessary memory (MemoryHandler allocates large blocks)
 *
 *
 *	Buffers can be owned externally, or be taken over by the object.
 */
class DataStream_ByteArray_Base : public DataStream
{
private:
	struct buffer_item {
		/** payload buffer */
		unsigned char *payload;
		/** Lengt of buffer */
		uint32 length;
		/** Next item in the chain */
		buffer_item *next;

		buffer_item();
		~buffer_item();

		/** Clean up, and delete all items after it in the chain */
		void Clear();
		/** Set the buffer with up to two buffers that are concatenated
		 *
		 *	@param	data1	First buffer
		 *	@param	len1	Length of first buffer
		 *	@param	data2	second buffer
		 *	@param	len2	Length of second buffer
		 *
		 *	@return			Status of operation
		 */
		OP_STATUS Set(const unsigned char *data1, uint32 len1, const unsigned char *data2=NULL, uint32 len2=0);

		/** Resize the buffer
		 *
		 *  @param	len	New length
		 */
		OP_STATUS Resize(uint32 len);

		/** Release the buffer to the caller; caller takes ownership */
		unsigned char *Release(){unsigned char *ret = payload; payload = NULL; length = 0; return ret;}
	};

private:
	/**	The current record payload length*/
	uint32 payload_length;

	/** The maximum length of the payload, set by ResizeL(), 0 means as long as necessary */
	uint32 max_payload_length;

	/** What kind of poayload storage is used */
	enum en_payload_mode {NO_PAYLOAD, MICRO_PAYLOAD, SMALL_PAYLOAD, LARGE_PAYLOAD, EXTERNAL_PAYLOAD, EXTERNAL_PAYLOAD_OWN} payload_mode;

	/** The payload buffer for small records, owned by implementation class */
	unsigned char *micro_payload;

	/** Length of the micro_payload buffer */
	uint32 micro_payload_length;

	/** The payload buffer for medium sized records */
	buffer_item small_payload; // OLD:! max FL_MAX_SMALL_PAYLOAD bytes: NEW: no limitation, may also be external

	/** The length of the current small_payload; only used during large payload mode */
	uint32 small_payload_length;

	/** The payload buffer linked listsequence for large records */
	buffer_item *payload_sequence;

	/** The last item in the sequence */
	buffer_item *payload_sequence_end;

	/** The position in the current block (or buffer) presently being read */
	uint32 read_pos;

	/** Which block is presently being read in the large payload buffer */
	buffer_item *read_item;

	/** The position in the current block (or buffer) presently being written to */
	uint32 write_pos;

	/** If this is TRUE, the object needs direct memory access to the payload. Default FALSE */
	UINT32	need_direct_access:1;

	/** If this is TRUE, the object has been given a fixed size, that must not be changed. Default FALSE */
	UINT32	fixed_size:1;

	/** If this is TRUE, the object only contains a part of the content, there may be more later. Default FALSE */
	UINT32	is_a_segment:1;

	/** If this is TRUE, the buffer is a FIFO sequence, when committing the remaining contents of the buffer is moved to the beginning */
	UINT32  is_a_fifo:1;

	/** Do not allocate extra size for this buffer */
	UINT32	dont_alloc_extra:1;

	/** Is this array sensitive? */
	UINT32	is_sensitive:1;

private:
	/** Used to delete the payload */
	void ClearPayload();

	OP_STATUS InternalResize(en_payload_mode new_mode, uint32 new_len);
	void InternalInit(unsigned char *buf, uint32 len);

protected:
	//virtual DataStream *CreateRecordL();

	/** Sets the micro buffer made available from an implementation 
	 *	Can only be set *once*, further call will fail silently.
	 *
	 *	@param	micro_buf	pointer to the buffer that can be used. 
	 *						The buffer MUST exist until the object is destroyed
	 *						DataStream_ByteArray_Base destructor WILL NOT access the buffer
	 *
	 *	@param	mbuf_len	Length of thebuffer
	 */
	void SetMicroBuffer(unsigned char *micro_buf, uint32 mbuf_len);

public:
	/** Constructor */
	DataStream_ByteArray_Base();

	/** Constructor. Intialize with externally owned array, of specified length
	 *
	 *	@param	buf		Externally owned buffer which will be used by this object
	 *	@len	len		Maximum length of the buffer
	 */
	DataStream_ByteArray_Base(unsigned char *buf, uint32 len);

	virtual ~DataStream_ByteArray_Base();

	uint32 GetLength() const{return (is_a_fifo ? payload_length - read_pos : payload_length);};

	/** Return 0 if the capacity is not limited */
	uint32 Capacity(){return max_payload_length ? max_payload_length : (payload_mode == SMALL_PAYLOAD ? small_payload.length : payload_length);}


	/**
	 *	Set the provided external buffer as the actual buffer for storage
	 *
	 *	@param  src		pointer to external buffer
	 *	@param	take_over	if TRUE, ClearPayload() will use "delete []" to free the memory
	 *	@param	len		Length of buffer. If zero, it is not set, and maximum is set as the current length
	 */
	void	SetExternalPayload(unsigned char *src, BOOL take_over = FALSE, uint32 len = 0);

	/**
	 *	Resize payload buffer to _len bytes, reallocate if necessary
	 *	External buffers will not be reallocated. It is assumed that they are *always* large enough
	 *
	 *	@param	_len	New length of buffer
	 */
	OP_STATUS Resize(uint32 _len, BOOL alloc = FALSE, BOOL set_length=FALSE);
	void ResizeL(uint32 _len, BOOL alloc = FALSE, BOOL set_length=FALSE){LEAVE_IF_ERROR(Resize(_len, alloc, set_length));}


	/**
	 *	Get Direct access to the buffer used to store the payload
	 *
	 *	@return		Pointer to byte 0 in the payload
	 */
	unsigned char *GetDirectPayload();
	const unsigned char *GetDirectPayload() const;

	/** 
	 *	release the contained buffer, caller takes ownership of the release buffer, 
	 *	and MUST delete the buffer after use.
	 *
	 *	Only allowed if need_direct_access == TRUE
	 *
	 *	NOTE: In case the buffer has been set using SetExternalPayload() with take_over == FALSE 
	 *	the callers of SetExternalPayload() and release() MUST agree on how to delete the buffer
	 *
	 *	May have to allocated memory, if micro_payload is used, and may LEAVE in such cases
	 */
	unsigned char *ReleaseL();

	/**
	 *  Sets the flag for direct access is needed. This disables the use of 
	 *	MemoryHandler (Large payload), and the size limitations on small payloads,
	 *	and enables the use of the ResizeL and GetDirectPayload functions
	 *
	 *	The default value of the flag is FALSE
	 *
	 *	This function MUST only be used during construction of an object, and 
	 *	before any allocation has been made. Otherwise, the results are undetermined.
	 *
	 *	@param	flag	The new direct access flag value.
	 */
	void SetNeedDirectAccess(BOOL flag){need_direct_access = (flag ? TRUE : FALSE);}

	/**
	 *  Sets the flag for fixed size. When this is TRUE, the ResizeL set size is the maximum used
	 *  Only valid for need_direct_access == TRUE
	 *
	 *	The default value of the flag is FALSE
	 *
	 *	@param	flag	The new fixed size flag value.
	 */
	void SetFixedSize(BOOL flag){fixed_size = (flag ? TRUE : FALSE); max_payload_length = (fixed_size ? payload_length : 0);}

	/** Enables FIFO mode. Implies SetNeedDirecAccess */
	void SetIsFIFO(){is_a_fifo = TRUE; need_direct_access = TRUE;}

	/** Is this a FIFO buffer? */
	BOOL GetIsFIFO(){return is_a_fifo;}

	/**
	 *  Sets the flag for the buffer being only a part of the content
	 *
	 *	The default value of the flag is FALSE
	 *
	 *	@param	flag	The segment flag value.
	 */
	void SetIsASegment(BOOL flag){is_a_segment = (flag ? TRUE : FALSE);}

	/**
	 *  Sets the flag for the buffer being kept to the 
	 *	currently required size, by not allocating extra bytes
	 *
	 *	The default value of the flag is FALSE
	 *
	 *	@param	flag	The new flag value.
	 */
	void SetDontAllocExtra(BOOL flag){dont_alloc_extra = (flag ? TRUE : FALSE);}

	/** Is this array sensitive? */
	BOOL GetIsSensitive() const{return is_sensitive;}

	/** Set this array as sensitive (default TRUE) */
	void SetIsSensitive(BOOL flag){is_sensitive = (flag ? TRUE: FALSE);}

	/**
	 *	Set the reading position inside the payload buffer
	 *  Only valid for need_direct_access == TRUE
	 *
	 *	@param	_pos	Position in buffer, base 0. Must be less than length of payload
	 *
	 *	@return	BOOL	TRUE if position was valid, otherwise FALSE
	 */
	BOOL SetReadPos(uint32 _pos);

	/** Return the current reading position
	 *  Only valid for need_direct_access == TRUE
	 */
	uint32 GetReadPos() const{return read_pos;}

	/**
	 *	Set the writing position inside the payload buffer
	 *
	 *	@param	_pos	Position in buffer, base 0. Must be less than length of payload
	 *
	 *	@return	BOOL	TRUE if position was valid, otherwise FALSE
	 */
	BOOL SetWritePos(uint32 _pos);

	/** Return the current reading position
	 *  Only valid for need_direct_access == TRUE
	 */
	uint32 GetWritePos() const{return write_pos;}

	/**
	 *	Adds content to the payload from a memory buffer
	 *
	 *	@param	src		pointer to buffer that will be added to the payload
	 *	@param	len		the length of the buffer being added
	 */
	void AddContentL(const byte *src, uint32 len);

	/**
	 *	Adds content to the payload from a DataStream
	 *
	 *	@param	src		pointer to stream holding the data that will be added to the payload
	 *	@param	len		the number of bytes to be read from src and added to the payload (0 means every thing)
	 */
	uint32 AddContentL(DataStream *src, uint32 len= 0, uint32 buffer_len=0)
			{return DataStream::AddContentL(src,len, buffer_len);}

	/**
	 *	Add a null terminated string to the record payload
	 *	The null termination is not stored
	 *
	 *	@param	src		Allocated Null terminated string
	 */
	void AddContentL(const char *src);

	/**
	 *	Add an 8 bit string object to the record payload
	 *	The null termination is not stored
	 *
	 *	@param	src		The string
	 */
	void AddContentL(OpStringC8 &src);

	/**
	 *	Allocate and return a nullterminated 8 bit string
	 *	The string is created from the entire payload 
	 *	of the record.
	 *	Caller must free string
	 *
	 *	@return	char *	The allocated string
	 */
	char *GetStringL();

	/**
	 *	Return an 8bit string in the string argument
	 *	The string is created from the entire payload 
	 *	of the record
	 *
	 *	@param	string	The object where the string will be stored
	 */
	void GetStringL(OpStringS8 &string);

	/** 
	 *	Read up to the len next bytes from the stream, updasting position
	 *
	 *	@param	buffer	pointer to the location where the bytes will be stored
	 *					Must be at least len bytes
	 *	@param	len		Number of bytes to read
	 *
	 *  @return unsigned long	Number of bytes actually read, may be less than specified in len
	 */
	virtual unsigned long ReadDataL(unsigned char *buffer, unsigned long len, DataStream::DatastreamCommitPolicy sample=DataStream::KReadAndCommit);


	/** 
	 *	Read into the destination datastream
	 *
	 *	@param	dst 	pointer to the destination datastream
	 */
	virtual void	ExportDataL(DataStream *dst);

	/** 
	 *	Write the len bytes in buffer to the stream
	 *
	 *	@param	buffer 	pointer to the location where the bytes are stored
	 *					Must be at least len bytes
	 *	@param	len		Number of bytes to write
	 */
	virtual void	WriteDataL(const unsigned char *buffer, unsigned long len);

	virtual uint32		GetAttribute(DataStream::DatastreamUIntAttribute attr);
#ifdef DATASTREAM_READ_RECORDS
	virtual OP_STATUS	PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target);
#endif
	virtual void		PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);

	int operator ==(const DataStream_ByteArray_Base &other) const;

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "DataStream_ByteArray";}
	virtual const char *Debug_OutputFilename(){return "stream.txt";};
public:
	virtual void Debug_Write_All_Data();
#endif
};

/** Binary buffer DataStream implementation with 128 byte internal unallocated buffer
 *
 *  Payload operations are only on binary arrays or Generic records
 *	Type specific operations must be added by the specific implementations.
 *
 *	This is an implementation of the DataStream_ByteArray_Base class, and provides 
 *	the object with 128 byte internal buffer, removing the need for allocation for small
 *	records,
 *
 *	Buffers can be owned externally, or be taken over by the object.
 */
class DataStream_ByteArray : public DataStream_ByteArray_Base
{
private:
	/** The payload buffer for small records */
	unsigned char micro_payload[FL_MAX_MICRO_PAYLOAD]; // for small records, to avoid allocation /* ARRAY OK 2009-04-22 yngve */

public:
	/** Constructor */
	DataStream_ByteArray();

	/** Constructor. Intialize with externally owned array, of specified length
	 *
	 *	@param	buf		Externally owned buffer which will be used by this object
	 *	@len	len		Maximum length of the buffer
	 */
	DataStream_ByteArray(unsigned char *buf, uint32 len);
 
	/** Destructor */
	virtual ~DataStream_ByteArray();
};

typedef DataStream_ByteArray_Base DataStream_SourceBuffer;
typedef DataStream_ByteArray_Base DataStream_FIFO_Stream;

/** A FIFO stream with a buffer for temporary storage while loading data from a non-stream source 
 *
 *	Usage: 
 *		- Construct with the desired buffer size (default and minimum 1024 bytes)
 *		- Use GetBuffer and GetBufLen to access the buffer, and load data into this buffer
 *		- Call AddBuffer with the loaded length to commit to the FIFO
 *		- Data can also be added with ordinary WriteDataL calls
 *		- Load stream content as usual, and use FinishedAddingL to indicate the end of the stream.
 */
class Datastream_FIFO_Stream_With_Buffer : public DataStream_ByteArray_Base
{
private:
	DataStream_ByteArray_Base temp_buffer;
public:
	/** Constructor */
	Datastream_FIFO_Stream_With_Buffer(){};

	/** Construct the buffer 
	 *
	 *  @param buf_size		Buffersize in bytes. Default value is 1024; which is also the minimum size
	 *
	 *	LEAVES in case of an OOM
	 */
	void ConstructL(uint32 buf_size=1024){
					SetIsFIFO();
					SetIsASegment(TRUE);
					temp_buffer.SetNeedDirectAccess(TRUE);
					temp_buffer.SetFixedSize(TRUE);

					temp_buffer.ResizeL((buf_size>1024?buf_size: 1024), TRUE); // Need to alloc or our GetDirectPayload() will return NULL
				}

	/** Get the pointer to the input buffer */
	unsigned char *GetBuffer(){return temp_buffer.GetDirectPayload();}

	/** Get the length of the input buffer */
	uint32  GetBufLen(){return temp_buffer.Capacity();}

	/** After adding data to the buffer, use this call to commit it into the FIFO stream
	 *
	 *	@param len	Number of bytes that was added to the buffer; max allowed size is the length of the buffer
	 */
	OP_STATUS AddBuffer(uint32 a_len){
				OP_MEMORY_VAR uint32 len = a_len; 
				if (len > temp_buffer.Capacity())
					len = temp_buffer.Capacity(); 
				TRAPD(op_err, WriteDataL(temp_buffer.GetDirectPayload(), len)); 
				return op_err;
			}
};

class DataStream_ByteArray_CleansePolicyL : public CleanupItem // Most frequently used with leave functions
{
private:
	DataStream_ByteArray_CleansePolicy_values previous_policy;

	DataStream_ByteArray_CleansePolicyL(const DataStream_ByteArray_CleansePolicyL &); // Non-existent. MUST NOT be used

public:
	DataStream_ByteArray_CleansePolicyL(DataStream_ByteArray_CleansePolicy_values pol)
	{
		previous_policy = g_ByteArray_Cleanse_Policy;
		g_ByteArray_Cleanse_Policy = pol;
	}

	~DataStream_ByteArray_CleansePolicyL()
	{
		g_ByteArray_Cleanse_Policy = previous_policy;
	}
};

class DataStream_ByteArray_CleansePolicy
{
private:
	DataStream_ByteArray_CleansePolicy_values previous_policy;

	DataStream_ByteArray_CleansePolicy(const DataStream_ByteArray_CleansePolicy &); // Non-existent. MUST NOT be used

public:
	DataStream_ByteArray_CleansePolicy(DataStream_ByteArray_CleansePolicy_values pol)
	{
		previous_policy = g_ByteArray_Cleanse_Policy;
		g_ByteArray_Cleanse_Policy = pol;
	}

	~DataStream_ByteArray_CleansePolicy()
	{
		g_ByteArray_Cleanse_Policy = previous_policy;
	}
};

#define DataStream_Sensitive_ByteArray(name)  DataStream_ByteArray_CleansePolicy policy_##name (DataStream_Sensitive_Array)
#define DataStream_NonSensitive_ByteArray(name)  DataStream_ByteArray_CleansePolicy policy_##name (DataStream_NonSensitive_Array)
#define DataStream_Conditional_Sensitive_ByteArray(name, sensitive)  DataStream_ByteArray_CleansePolicy policy_##name ((sensitive) ? DataStream_Sensitive_Array : DataStream_NonSensitive_Array )

#define DataStream_Sensitive_ByteArrayL(name)  DataStream_ByteArray_CleansePolicyL policy_##name (DataStream_Sensitive_Array)
#define DataStream_NonSensitive_ByteArrayL(name)  DataStream_ByteArray_CleansePolicyL policy_##name (DataStream_NonSensitive_Array)
#define DataStream_Conditional_Sensitive_ByteArrayL(name, sensitive)  DataStream_ByteArray_CleansePolicyL policy_##name ((sensitive) ? DataStream_Sensitive_Array : DataStream_NonSensitive_Array )

#endif // DATASTREAM_BYTEARRAY_ENABLED
#endif // __FL_BYTES_H__

