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

#ifndef _FL_BASE_H_
#define _FL_BASE_H_

#ifdef DATASTREAM_BASE_ENABLED

#include "modules/util/simset.h"
#ifdef _SSL_SUPPORT_
#include "modules/libssl/ssluint.h"
#endif

#ifdef _DEBUG
#ifdef YNP_WORK
//#define _DATASTREAM_DEBUG_
#endif
#endif


#define FL_MAX_MICRO_PAYLOAD 16
#define FL_MAX_SMALL_PAYLOAD (16*1024)
#ifndef MSB_VALUE
#define MSB_VALUE 0x80000000
#endif

#if defined _SSL_SUPPORT_
class SSL_varvector32;
#endif

/** Structure used to specify fixed size record formats */
struct DataRecord_Spec
{
	/**	Flag for turning on/off the tag_id field */
	uint32	enable_tag:1;

	/** length in bytes of record field tags */
	uint32 idtag_len : 8;

	/** The tag is stored in big endian format*/
	uint32	tag_big_endian:1;

	/**	The MSB of the tag is stored as the MSB of the binary format, and vice versa */
	uint32	tag_MSB_detection:1;

	/**	Flag for turning on/off the length field */
	uint32	enable_length:1;

	/** length in bytes of record field payload length */
	uint32 length_len:8 ;

	/** The length is stored in big endian format*/
	uint32	length_big_endian:1;

	/** The general numbers of the format are stored in a big endian format*/
	uint32	numbers_big_endian:1;

	/** Constructor */
	DataRecord_Spec();
	DataRecord_Spec(BOOL e_tag, uint16 tag_len, BOOL tag_be, BOOL tag_MSB, BOOL e_len, uint16 len_len, BOOL len_be, BOOL num_be);

#ifdef UNUSED_CODE
	/** Assignement operator */
	DataRecord_Spec &operator =(const DataRecord_Spec &other);
#endif
};

/** An extension of the OpStatus class and its enums */
class OpRecStatus : public OpStatus
{
public:
	enum 
	{
		/** Record finished */
		FINISHED = USER_SUCCESS + 0,
		/** We've reached the end of the stream */
		END_OF_STREAM = USER_SUCCESS +1,
		/** Waiting for more data */
		WAIT_FOR_DATA = USER_SUCCESS +2,
		/** PGP key is in pending list */
		KEY_IN_PENDING = USER_SUCCESS+3,
		/** Need a password */
		NEED_PASSWORD = USER_SUCCESS+4,

		/** The available data in the record is too short, could not complete record */
		RECORD_TOO_SHORT = USER_ERROR + 0,
		/** Record contained an illegal sequence of data*/
		ILLEGAL_FORMAT = USER_ERROR+1,
		/** The format of the stream is not supported */
		UNSUPPORTED_FORMAT = USER_ERROR+2,
		/** Could not decrypt data */
		DECRYPTION_FAILED = USER_ERROR+3,
		/** CRC check did not match calculated value */
		CRC_CHECK_FAILED =  USER_ERROR+4,
		/** Method unsupported */
		UNSUPPORTED_METHOD = USER_ERROR+5,
		/** Wrong password */
		WRONG_PASSWORD = USER_ERROR+6,
		/** Signature is not using a valid format*/
		ILLEGAL_SIGNATURE = USER_ERROR+7,
		/** The key has been revoked */
		KEY_REVOKED = USER_ERROR+8,
		/** Signature could not be verified */
		VERIFICATION_FAILURE = USER_ERROR+9,
		/** Can't find certificate of signer */
		UNKNOWN_SIGNER =  USER_ERROR+10,
		/** The signature will be valid in the future, according to the local clock. Is the computer's time correct? */
		SIGNATURE_NOT_YET_VALID =  USER_ERROR+11,
		/** The validity period of the signature has expired */
		SIGNATURE_EXPIRED =  USER_ERROR+12,
		/** The key will be valid in the future, according to the local clock. Is the computer's time correct? */
		KEY_NOT_YET_VALID =  USER_ERROR+13,
		/** The validity period of the key has expired */
		KEY_EXPIRED =  USER_ERROR+14,
		/** Could not creat the signature */
		SIGNING_FAILED = USER_ERROR+15,
		/** Have not verified the signature (yet) */
		NOT_VERIFIED = USER_ERROR+16,
		/** Unknown key */
		UNKNOWN_KEY = USER_ERROR+17,
		/** Could not find the specified record in the sequence*/
		RECORD_NOT_FOUND = USER_ERROR+18,

		/** During decompression, the format was incorrect */
		DECOMPRESSION_FAILED  = USER_ERROR+19,


		/** First free error in this class */
		REC_STATUS_ERROR
	};
};

class DataStream_BaseRecord;

/* DataStream: Used to retrieve data from an 
 * unspecified data source
 */
class DataStream : public Link
{
	friend class DataStream_StreamReference;
private:
#ifdef DATASTREAM_READ_RECORDS
	/** The currently loading record. Always unfinished */
	DataStream *current_loading_record;
#endif

	/** Item identficator id. Default value DataStream::GenericRecord */
	int item_id;

	/** In structured records: is this record enabled */
	BOOL	enabled_record;

public:
	/** These values are used to identify special record items for
	 *	DataStream_BaseRecord::ReadActionL and DataStream_BaseRecord::WriteActionL()
	 */
	enum {
		GENERIC_RECORD = -1, // Generic record, no specific tasks
		DATASTREAM_START_VALUE = 0,  // Start subclass IDs from this value
		DATASTREAM_SPECIAL_START_VALUE = -4096 // Startvalue for special record types
	};

	enum DatastreamUIntAttribute {
		Datastream_record_start,

			/** Uint32	Calculate the length of this object */
			KCalculateLength,

			/** BOOL	Has the record been completely loaded? */
			KFinished,

		Datastream_record_end,
		Datastream_payload_start,

			/** BOOL	Is this stream open and active */
			KActive,
			/** uint32	Object ID */
			KID,
			/** BOOL	Is there more data available? */
			KMoreData,

		Datastream_payload_end,
		Datastream_uint_max_item
	};

	enum DatastreamAction {
		/** Commit a number of sampled bytes from the stream, arg1 = number of bytes to commit */
		KCommitSampledBytes,
		/** Finished adding data to the stream. No arguments*/
		KFinishedAdding,
		/** Initialize stream. No arguments */
		KInit,
		/** Internal command for Initialize stream for reading. No arguments*/
		KInternal_InitRead,
		/** Internal command for Initialize stream for reading. No arguments*/
		KInternal_InitWrite,
		/** Internal command for Initialize stream for reading. No arguments. MUST NOT LEAVE*/
		KInternal_FinalRead,
		/** Internal command for Initialize stream for reading. No arguments. MUST NOT LEAVE*/
		KInternal_FinalWrite,
		/** Action when a record has been read arg1 = step_num,  arg2 = record_id */
		KReadAction,
		/** Rewind for a new reading. No arguments. MUST NOT LEAVE*/
		KResetRead,
		/** Reset the record. No arguments. MUST NOT LEAVE */
		KResetRecord,
		/** Set the item's record ID. arg1 = item ID. MUST NOT LEAVE*/
		KSetItemID,
		/** Set the item's record tag. arg1 = tag ID. MUST NOT LEAVE*/
		KSetTag,
		/** Action when a record has been written arg1 = step_num,  arg2 = record_id */
		KWriteAction,
		/** Implementation specific action */
		KUserAction1,
		/** Implementation specific action */
		KUserAction2,
		/** Implementation specific action */
		KUserAction3,
		/** Implementation specific action */
		KUserAction4,
		/** Implementation specific action */
		KUserAction5,
		/** Implementation specific action */
		KUserAction6,
		/** Implementation specific action */
		KUserAction7,
		/** Implementation specific action */
		KUserAction8,
		/** Implementation specific action */
		KUserAction9,
		/** Implementation specific action */
		KUserAction10,
		Datastream_action_max_item
	};

	enum DatastreamStreamAction {
		/** Read the record */
		KReadRecord,
		/** Read the header of the record */
		KReadRecordHeader,
		/** Write the record */
		KWriteRecord,
		/** Write the header of the record */
		KWriteRecordHeader,
		/** Write the payload of the record */
		KWriteRecordPayload,

		Datastream_stream_action_max_item
	};

	enum DatastreamCommitPolicy {
		/** Read the data and Commit */
		KReadAndCommit = 0,
		/** Read the data, but do not commit */
		KSampleOnly = 1,
		/** Read the data, but only commit if all was read. */
		KReadAndCommitOnComplete
	};

public:

	/** Constructor */
	DataStream();

	/** Destructor */
	virtual ~DataStream();

	/** SampleData: look at the len next bytes in the stream without actually reading them
	 *
	 *	@param	buffer	pointer to the location where the bytes will be stored
	 *					Must be at least len bytes
	 *	@param	len     Number of bytes to sample
	 *
	 *  @return unsigned long Number of bytes actually sampled, may be less than specified in len
	 */
	unsigned long SampleDataL(unsigned char *buffer, unsigned long len){return ReadDataL(buffer, len, DataStream::KSampleOnly);}

	/** CommitSampledBytes : update position in stream by skipping the len next bytes 
	 *
	 *	@param	len		Number of bytes to skip.
	 */
	void CommitSampledBytesL(unsigned long len){PerformActionL(DataStream::KCommitSampledBytes, len);}

	/** 
	 *	Read up to the len next bytes from the stream, updasting position
	 *
	 *	@param	buffer	pointer to the location where the bytes will be stored
	 *					Must be at least len bytes
	 *	@param	len		Number of bytes to read
	 *
	 *  @return unsigned long	Number of bytes actually read, may be less than specified in len
	 */
	virtual unsigned long ReadDataL(unsigned char *buffer, unsigned long len, DataStream::DatastreamCommitPolicy sample = DataStream::KReadAndCommit);

	/** 
	 *	Read into the destination datastream
	 *
	 *	@param	dst 	pointer to the destination datastream
	 */
	virtual void	ExportDataL(DataStream *dst) {}

	/** 
	 *	Write the len bytes in buffer to the stream
	 *
	 *	@param	buffer 	pointer to the location where the bytes are stored
	 *					Must be at least len bytes
	 *	@param	len		Number of bytes to write
	 */
	virtual void	WriteDataL(const unsigned char *buffer, unsigned long len);

	/**
	 *	Adds content to the payload from a DataStream
	 *
	 *	@param	src		pointer to stream holding the data that will be added to the payload
	 *	@param	len		the number of bytes to be read from src and added to the payload (0 means every thing)
	 *
	 *	@return			Number of bytes read from the stream
	 */
	uint32 AddContentL(DataStream *src, uint32 len= 0, uint32 buffer_len = 0);
#if defined _SSL_SUPPORT_ 
	uint32 AddContentL(SSL_varvector32 *src, uint32 len= 0);
#endif

	/**
	 *	MoreData : Is there more data available, or expected (if the stream is currently exhausted)?

	 *	@return	BOOL	TRUE if there is presently more data available, or if the stream is not yet terminated
	 */
	BOOL	MoreData(){return !!GetAttribute(DataStream::KMoreData);};

	/**	Finish output generation. Flush any pending data
	 *	NOTE: File streams MUST NOT close.
	 */
	void FinishedAddingL(){PerformActionL(DataStream::KFinishedAdding);}

#ifdef DATASTREAM_READ_RECORDS
	/** 
	 *	Retrieves the next available record from this datastream.
	 *
	 *	If a record has been sampled, this record will be returned, and the caller takes ownership of the record
	 *
	 *  Unless the stream has no more data, it always returns a pointer to a loading record,
	 *  even if the record is unfinished. Subsequent calls to this function is necessary in that case.
	 *  As long as a record is unfinished a pointer to the record will be kept.
	 *  Destruction of the record will remove this connection
	 *
	 *	Note: Unfinished records will be deleted on destruction of this object
	 *
	 *  @return  DataStream_GenericRecord *	A pointer to the currently loading record, 
	 *					or a new record if there is no record loading and there are 
	 *					more data available or expected in the stream.
	 *					A NULL pointer is returned if a record cannot be created at the present time.
	 *					This may happen when there is not enough data in the stream to determine
	 *					the type of record, or when there is no more data in the stream.
	 *					MoreData() must be called in such cases.
	 *
	 *					The allocated record must be deleted by the caller.
	 *					The returned record may be accessed as a datastream before 
	 *					it is completely loaded, assuming that the caller is able to handle 
	 *					streaming data.
	 */
	DataStream *GetNextRecordL();

	/** 
	 *	Retrieves the next available record from this datastream, but does not release the record to the caller.
	 *
	 *	To take ownership of the returned record the caller MUST call GetNextRecordL() 
	 *
	 *	NOTE: SampleNextRecord WILL NOT try to load a new record as long as a finished, sampled record 
	 *	has not been removed by a call to GetNextRecord()
	 *
	 *  Unless the stream has no more data, it always returns a pointer to a loading record,
	 *  even if the record is unfinished. Subsequent calls to this function is necessary in that case.
	 *  As long as a record is unfinished a pointer to the record will be kept.
	 *  Destruction of the record will remove this connection
	 *
	 *	To determine if the record is finished loading call it's Finished() function
	 *
	 *	Note: Finished Records that have not been retrieved with GetNextRecordL will be deleted on destruction of this object
	 *
	 *  @return  DataStream_GenericRecord *	A pointer to the currently loading record, 
	 *					or a new record if there is no record loading and there are 
	 *					more data available or expected in the stream.
	 *					A NULL pointer is returned if a record cannot be created at the present time.
	 *					This may happen when there is not enough data in the stream to determine
	 *					the type of record, or when there is no more data in the stream.
	 *					MoreData() must be called in such cases.
	 *
	 *					The allocated record MUST NOT be deleted by the caller unless GetNextRecordL has been called first.
	 *					The returned record may be accessed as a datastream before 
	 *					it is completely loaded, assuming that the caller is able to handle 
	 *					streaming data.
	 */
	DataStream *SampleNextRecordL();

private:
	
	/** 
	 *	This function actually creates and reads the next record, and is called by GetNextRecordL and
	 *	SampleNextRecordL.
	 *
	 *  Unless the stream has no more data, it always returns a pointer in current_loading_record to a loading record,
	 *  even if the record is unfinished. Subsequent calls to this function is necessary in that case.
	 *  As long as a record is unfinished a pointer to the record will be kept.
	 *  Destruction of the record will remove this connection
	 *
	 *	To determine if the record is finished loading call it's Finished() function
	 *
	 *	This function updates the current_loading_record as specified in the 
	 */
	void ReadNextRecordL();

protected:

	/** 
	  *	Allocates a new record of the kind used in connection with the 
	  * actual stream implementation.
	  *
	  * @return  DataStream_GenericRecord *	Pointer to the new record. NULL *only* if allocation failes 
	  */
	virtual DataStream *CreateRecordL();

protected:

	/**
	 *	Write the given integer of the record to the target stream according 
	 *	to the given specifications assosiated
	 *
	 *	@param	value		The value to be written
	 *	@param	len			The number of bytes in which to write the integer
	 *	@param	big_endian	If TRUE, write the integer in the bigendian format
	 *	@param	MSB_detection	If TRUE, move the MSB of the integer to the MSB of the stored integer
	 *	@param	target			The stream to write the contents to
	 */
	void	WriteIntegerL(uint32 value, uint32 len, BOOL big_endian, BOOL MSB_detection,  DataStream *target);
	void	WriteInteger64L(OpFileLength value, uint32 len, BOOL big_endian, BOOL MSB_detection,  DataStream *target);
public:
	void	WriteIntegerL(uint32 value, uint32 len, BOOL big_endian, BOOL MSB_detection){
		WriteIntegerL(value, len, big_endian, MSB_detection, this);
	}
	void	WriteInteger64L(OpFileLength value, uint32 len, BOOL big_endian, BOOL MSB_detection){
		WriteInteger64L(value, len, big_endian, MSB_detection, this);
	}
#ifdef _SSL_SUPPORT_
protected:
	void	WriteIntegerL(uint64 value, uint32 len, BOOL big_endian,  DataStream *target);
public:
	void WriteIntegerL(uint64 value, uint32 len, BOOL big_endian){
		WriteIntegerL(value, len, big_endian, this);
	}
#endif

protected:

	/**
	 *	Read the given integer of the record from the source stream according 
	 *	to the given specifications assosiated
	 *
	 *	@param	src		The stream to read the contents from
	 *	@param	value	Reference to the location of the integer's storage
	 *	@param	len		The number of bytes from which to read the integer
	 *	@param	big_endian		If TRUE, read the integer in the bigendian format
	 *	@param	MSB_detection	If TRUE, move the MSB of the stored integer to the MSB of the integer returned
	 *
	 *	@return	OP_STATUS	status code (OpRecStatus::FINISHED if the value has beenfully loaded)
	 */
	OP_STATUS	ReadIntegerL(DataStream *src, uint32 &value, uint32 len, BOOL big_endian, BOOL MSB_detection, BOOL sample_only=FALSE);
	OP_STATUS	ReadInteger64L(DataStream *src, OpFileLength &value, uint32 len, BOOL big_endian, BOOL MSB_detection, BOOL sample_only=FALSE);
public:
	OP_STATUS	ReadIntegerL(uint32 &value, uint32 len, BOOL big_endian, BOOL MSB_detection, BOOL sample_only=FALSE, BOOL handle_oversized = FALSE){
		return ReadIntegerL(this, value, len, big_endian, MSB_detection, sample_only);
	}
#endif // DATASTREAM_READ_RECORDS

	virtual uint32		GetAttribute(DataStream::DatastreamUIntAttribute attr);

	/** 
	 *  Perform the given action on the stream, using the optional parameters
	 *
	 *	NOTE: Some of the actions MUST NOT LEAVE, and can be used without using TRAPs 
	 *
	 *	@param	action	which action to take. determines which arguments are used, if any
	 *	@param	arg1	first argument. meaning specified by the action enum
	 *	@param	arg1	second argument. meaning specified by the action enum
	 */
	virtual void	PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);

#ifdef DATASTREAM_READ_RECORDS
	virtual OP_STATUS	PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target);

public:
	/**
	 *	Signals that the current step, having loaded the identified record_item_id, is completed,
	 *	and that the special actions depending on that object can now be performed.
	 * 
	 *	The last step is signaled by DataStream_BaseRecord::STRUCTURE_FINISHED, step number is irrelevant.
	 *
	 *	Subclasses MUST ALWAYS call the baseclass implementation of ReadActionL. Call sequence is usually not significant.
	 */
	void ReadActionL(uint32 step, int record_item_id){PerformActionL(DataStream::KReadAction, step, (uint32) record_item_id);}


	/**
	 *	Signals that the current step, having written the identified record_item_id, is completed,
	 *	and that the special actions depending on that object can now be performed.
	 * 
	 *	Subclasses MUST ALWAYS call the baseclass implementation of ReadActionL. Call sequence is usually not significant.
	 */
	void WriteActionL(uint32 step, int record_item_id){PerformActionL(DataStream::KWriteAction, step, (uint32) record_item_id);}

public:
	/** 
	 *	Returns the fixed record specification used by the stream.
	 *  This function is only used by fixed size record formats, and only need to be defined 
	 *	for datastreams that supports this.
	 *  
	 *	@return const DataRecord_Spec *		The pointer to the record specification, if available, NULL otherwise.
	 */
	virtual const DataRecord_Spec *GetRecordSpec();

	/** 
	 *	Release the current record. The argument rec must match the current record pointer
	 *  NOTE: THIS WILL ALWAYS CAUSE PROBLEMS UNLESS THE STREAM IS TO BE CLOSED
	 *
	 *	@param	rec		pointer to the record that is calling. Must match the current record
	 */
	void ReleaseRecord(DataStream *rec, BOOL finished= FALSE);
#endif  // DATASTREAM_READ_RECORDS

public:
	/**
	 *  Return TRUE if the stream is open and active.
	 */
	BOOL Active(){return !!GetAttribute(DataStream::KActive);};

	/** 
	 *	Sets a specific item ID for this object 
	 *
	 *  These values are used to identify special record items for
	 *	DataStream_BaseRecord::ReadActionL and DataStream_BaseRecord::WriteActionL()
	 *
	 *	If not set, the default value is DataStream::GenericRecord
	 */
	virtual void SetItemID(int id){item_id =id;}

	/** 
	 *	Retrieves the specified item ID for this object.
	 *
	 *  These values are used to identify special record items for
	 *	DataStream_BaseRecord::ReadActionL and DataStream_BaseRecord::WriteActionL()
	 *
	 *	If not set, the default value is DataStream::GenericRecord
	 */
	uint32 GetItemID(){return GetAttribute(DataStream::KID);};

#ifdef DATASTREAM_READ_RECORDS
	/**
	 *	Read data from the stream to construct this record
	 *
	 *	@param	src		Source stream from which content will be read.
	 *	@return	OP_STATUS	OpStatus::OK if everything went well. 
	 *			Check Finished() to determine if the record is completely loaded
	 */
	OP_STATUS ReadRecordFromStreamL(DataStream *src)   {return PerformStreamActionL(DataStream::KReadRecord,src);}

#if defined _SSL_SUPPORT_
	/**
	 *	Read only the header portion of this record from the stream
	 *
	 *	@param	src		Source stream from which content will be read.
	 *	@return	OP_STATUS	OpStatus::OK if everything went well. 
	 *			Check Finished() to determine if the record is completely loaded
	 */
	OP_STATUS ReadRecordHeaderFromStreamL(DataStream *src)  {return PerformStreamActionL(DataStream::KReadRecordHeader,src);}

#endif
#endif

	/**
	 *	Enable/Disable record
	 */
	void SetEnableRecord(BOOL flag){enabled_record = flag;}

	/**
	 *	Get Record Enabled flag
	 */
	BOOL GetEnabledRecord() const{return enabled_record;}

	/**	Clear payload and tag id */
	void ResetRecord(){PerformActionL(DataStream::KResetRecord);/* Does not LEAVE */}

	/**	Prepare to read the payload from the start.*/
	void ResetRead(){PerformActionL(DataStream::KResetRead);/* Does not LEAVE */};

#ifdef DATASTREAM_READ_RECORDS
	/**
	 *	Write the record to the target stream
	 *
	 *	@param	target	The stream to write the contents to
	 */
	void WriteRecordL(DataStream *target){OpStatus::Ignore(PerformStreamActionL(DataStream::KWriteRecord,target));}

#if defined _SSL_SUPPORT_
	/**
	 *	Write the header of the record to the target stream
	 *
	 *	@param	target	The stream to write the contents to
	 */
	void WriteRecordHeaderL(DataStream *target) {OpStatus::Ignore(PerformStreamActionL(DataStream::KWriteRecordHeader,target));}


	/**
	 *	Write the payload of the record to the target stream
	 *
	 *	@param	target	The stream to write the contents to
	 */
	void WriteRecordPayloadL(DataStream *target) {OpStatus::Ignore(PerformStreamActionL(DataStream::KWriteRecordPayload,target));}
#endif
#endif



	/** 
	 *	Returns TRUE if the record has been completely loaded
	 *
	 *	@return	BOOL	TRUE if record has been loaded
	 */
	BOOL Finished(){return !!GetAttribute(DataStream::KFinished);};

	/**
	 *	Calculate the length of this stream/record 
	 *
	 *	@return	uint32	the length of the stream.
	 */
	uint32 CalculateLength(){return GetAttribute(DataStream::KCalculateLength);};


	DataStream *Suc(){return (DataStream *) Link::Suc();}
	DataStream *Pred(){return (DataStream *) Link::Pred();}

#ifdef _DATASTREAM_DEBUG_
private:
	static int DS_debug_level;
	friend class DataStream_Debug_Counter;
protected:
	virtual const char *Debug_ClassName();
	virtual const char *Debug_OutputFilename();

	/** DS_Debug_Write can be used without ifdefs, a macro is defined in nondebug compiles */
	void DS_Debug_Write(const char *function_name, const char *Description, const unsigned char *source, unsigned int len);
	/** DS_Debug_Write cannot be used directly without ifdefs, DS_Debug_PrintfN has been defined to allow use without ifdefs*/
	void DS_Debug_Printf(const char *format, ...);

public:
	virtual void Debug_Write_All_Data();

	// Use these to avoid ifdef's around DS_Debug_Printf
#define DS_Debug_Printf0(f) DS_Debug_Printf(f)
#define DS_Debug_Printf1(f,x) DS_Debug_Printf(f,x)
#define DS_Debug_Printf2(f,x,y) DS_Debug_Printf(f,x,y)
#define DS_Debug_Printf3(f,x,y,z) DS_Debug_Printf(f,x,y,z)
#define DS_Debug_Printf4(f,x,y,z,v) DS_Debug_Printf(f,x,y,z,v)
#define DS_Debug_Printf5(f,x,y,z,v,w) DS_Debug_Printf(f,x,y,z,v,w)
#define DS_Debug_WriteObject(s) s.Debug_Write_All_Data();
#else

public:
	void Debug_Write_All_Data(){};

#define DS_Debug_Write(n,s,l,D)
#define DS_Debug_Printf0(f)
#define DS_Debug_Printf1(f,x)
#define DS_Debug_Printf2(f,x,y)
#define DS_Debug_Printf3(f,x,y,z)
#define DS_Debug_Printf4(f,x,y,z,v)
#define DS_Debug_Printf5(f,x,y,z,v,w)
#define DS_Debug_WriteObject(s)
#endif
};

#endif // DATASTREAM_BASE_ENABLED

#endif // _FL_BASE_H_
