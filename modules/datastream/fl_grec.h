/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _FL_GREC_H_
#define _FL_GREC_H_

#ifdef DATASTREAM_SEQUENCE_ENABLED
/** DataStream_GenericRecord
 *	Contains the record's id and payload
 *
 *	This implementation is only intended to handle records on the form
 *
 *		tag
 *		[payload length]
 *		[payload]
 *
 *	payload length (and payload is optional)
 * 
 *  Presently limited to 32 bit tag-id and record payload length.
 *  Implementations may however override tag-id and length binary representations
 *  Unless they reimplement payload storage they cannot expand payload capacity.
 *
 *  Payload operations are only on binary arrays or Generic records
 *	Type specific operations must be added by the specific implementations.
 *
 *	Payload storage is 3-tier, with conditonal storage in a 32 byte, 16KB and 
 *  "unlimited" storage, depending on the size of the payload;
 *  The reason for this is to reduce allocation overhead (in MemoryHandler), and
 *  to reduce allocation of unnecessary memory (MemoryHandler allocates large blocks)
 */
template <class T> class DataStream_GenericRecord : public DataStream_BaseRecord
{
	friend class DataStream;

protected:
	T payload;


public:

	/** provide direct access to the payload record */
	T &AccessPayloadRecord(){return payload;}

	/**
	 *	Set the provided external buffer as the actual buffer for storage
	 *
	 *	@param  src		pointer to external buffer
	 *	@param	take_over	if TRUE, ClearPayload() will use "delete []" to free the memory
	 *	@param	len		Length of buffer. If zero, it is not set, and maximum is set as the current length
	 */
	void	SetExternalPayload(unsigned char *src, BOOL take_over = FALSE, uint32 len = 0)
		{payload.SetExternalPayload(src, take_over, len);}


	/**
	 *	Resize payload buffer to _len bytes, reallocate if necessary
	 *	External buffers will not be reallocated. It is assumed that they are *always* large enough
	 *
	 *	@param	_len	New length of buffer
	 */
	void ResizeL(uint32 _len, BOOL alloc = FALSE, BOOL set_length=FALSE)
		{payload.ResizeL(_len, alloc, set_length);}

	/**
	 *	Get Direct access to the buffer used to store the payload
	 *
	 *	@return		Pointer to byte 0 in the payload
	 */
	unsigned char *GetDirectPayload()
		{return payload.GetDirectPayload();}

	const unsigned char *GetDirectPayload() const
		{return payload.GetDirectPayload();}

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
	void SetNeedDirectAccess(BOOL flag){payload.SetNeedDirectAccess(flag);}

	/**
	 *  Sets the flag for fixed size. When this is TRUE, the ResizeL set size is the maximum used
	 *  Only valid for need_direct_access == TRUE
	 *
	 *	The default value of the flag is FALSE
	 *
	 *	@param	flag	The new fixed size flag value.
	 */
	void SetFixedSize(BOOL flag){payload.SetFixedSize(flag);}

	/**
	 *  Sets the flag for the buffer being only a part of the content
	 *
	 *	The default value of the flag is FALSE
	 *
	 *	@param	flag	The segment flag value.
	 */
	void SetIsASegment(BOOL flag){payload.SetIsASegment(flag);}

	/**
	 *  Sets the flag for the buffer being kept to the 
	 *	currently required size, by not allocating extra bytes
	 *
	 *	The default value of the flag is FALSE
	 *
	 *	@param	flag	The new flag value.
	 */
	void SetDontAllocExtra(BOOL flag){payload.SetDontAllocExtra(flag);}

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
	unsigned char *ReleaseL()
		{return payload.ReleaseL();}

	/**
	 *	Set the reading position inside the payload buffer
	 *  Only valid for need_direct_access == TRUE
	 *
	 *	@param	_pos	Position in buffer, base 0. Must be less than length of payload
	 *
	 *	@return	BOOL	TRUE if position was valid, otherwise FALSE
	 */
	BOOL SetReadPos(uint32 _pos)
		{return payload.SetReadPos(_pos);}

	/** Return the current reading position
	 *  Only valid for need_direct_access == TRUE
	 */
	uint32 GetReadPos() const
		{return payload.GetReadPos();}

	/**
	 *	Set the writing position inside the payload buffer
	 *
	 *	@param	_pos	Position in buffer, base 0. Must be less than length of payload
	 *
	 *	@return	BOOL	TRUE if position was valid, otherwise FALSE
	 */
	BOOL SetWritePos(uint32 _pos)
		{return payload.SetWritePos(_pos);}

	/** Return the current reading position
	 *  Only valid for need_direct_access == TRUE
	 */
	uint32 GetWritePos() const
		{return payload.GetWritePos();}

	uint32 GetLength() const{return payload.GetLength();}

public:

	/** Constructor */
	DataStream_GenericRecord(){
		payload.Into(this);
	}


	/** Destructor */
	//~DataStream_GenericRecord();

	/**
	 *	Controls whether or not the payload length (and the payload) is written 
	 *	to the binary version of the record, or read from the binary stream
	 *
	 *	@param use_payload	If this parameter is set to FALSE only the tag value 
	 *						of this record is written to the binary stream, or read form the stream
	 */
	void UsePayload(BOOL use_payload)
			{payload.SetEnableRecord(use_payload);} 
	
	/**
	 *	Adds content to the payload from a memory buffer
	 *
	 *	@param	src		pointer to buffer that will be added to the payload
	 *	@param	len		the length of the buffer being added
	 */
	void AddContentL(const byte *src, uint32 len){payload.AddContentL(src, len);}


	/**
	 *	Adds content to the payload from a DataStream
	 *
	 *	@param	src		pointer to stream holding the data that will be added to the payload
	 *	@param	len		the number of bytes to be read from src and added to the payload (0 means every thing)
	 */
	void AddContentL(DataStream *src, uint32 len= 0)
			{DataStream::AddContentL(src,len);}

	/**
	 *	Add a null terminated string to the record payload
	 *	The null termination is not stored
	 *
	 *	@param	src		Allocated Null terminated string
	 */
	void AddContentL(const char *src)
		{payload.AddContentL((const unsigned char *) src, (src ? op_strlen(src): 0));}

	/**
	 *	Add an 8 bit string object to the record payload
	 *	The null termination is not stored
	 *
	 *	@param	src		The string
	 */
	void AddContentL(const OpStringC8 &src)
		{payload.AddContentL((const unsigned char *) src.CStr(), src.Length());}

	/**
	 *	Allocate and return a nullterminated 8 bit string
	 *	The string is created from the entire payload 
	 *	of the record.
	 *	Caller must free string
	 *
	 *	@return	char *	The allocated string
	 */
	char *GetStringL();
#if 0
		{return payload.GetStringL();}
#endif

	/**
	 *	Return an 8bit string in the string argument
	 *	The string is created from the entire payload 
	 *	of the record
	 *
	 *	@param	string	The object where the string will be stored
	 */
	void GetStringL(OpStringS8 &string)
		{payload.GetStringL(string);}

	/** 
	 *	Release the stream that this record is reading from. 
	 *	The argument strm must match the current stream
	 *	The record will also be finished (aborted)
	 *
	 *	@param	strm		pointer to the stream that is calling. Must match the current stream
	 */
	void ReleaseStream(DataStream *strm);

	virtual void PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0)
	{
		switch(action)
		{
		case DataStream::KReadAction:
			{
				int record_item_id = (int) arg2;

				if(record_item_id == DataStream_BaseRecord::RECORD_LENGTH && length.GetEnabledRecord())
				{
					payload.SetFixedSize(TRUE);
					payload.ResizeL(length->GetValue());
				}
			}
			break;
		case DataStream::KCommitSampledBytes:
		case DataStream::KFinishedAdding:
			payload.PerformActionL(action, arg1, arg2);
			return;
		}
		DataStream_BaseRecord::PerformActionL(action, arg1, arg2);
	}


	/** 
	 *	Read up to the len next bytes from the stream, updasting position
	 *
	 *	@param	buffer	pointer to the location where the bytes will be stored
	 *					Must be at least len bytes
	 *	@param	len		Number of bytes to read
	 *
	 *  @return unsigned long	Number of bytes actually read, may be less than specified in len
	 */
	virtual unsigned long ReadDataL(unsigned char *buffer, unsigned long len, DataStream::DatastreamCommitPolicy sample = DataStream::KReadAndCommit){
		return payload.ReadDataL(buffer, len, sample);
	}


	/** 
	 *	Write the len bytes in buffer to the stream
	 *
	 *	@param	buffer 	pointer to the location where the bytes are stored
	 *					Must be at least len bytes
	 *	@param	len		Number of bytes to write
	 */
	virtual void	WriteDataL(const unsigned char *buffer, unsigned long len){
		payload.WriteDataL(buffer, len);
	}

	virtual uint32		GetAttribute(DataStream::DatastreamUIntAttribute attr){
				if(attr >= DataStream::Datastream_payload_start && attr <= DataStream::Datastream_payload_end)
					return payload.GetAttribute(attr);
				return DataStream_BaseRecord::GetAttribute(attr);
			};

	DataStream_GenericRecord *Suc(){return (DataStream_GenericRecord *) Link::Suc();}
	DataStream_GenericRecord *Pred(){return (DataStream_GenericRecord *) Link::Pred();}

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "DataStream_GenericRecord";}
#endif

};

#endif // DATASTREAM_SEQUENCE_ENABLED

#endif // _FL_REC_H_
