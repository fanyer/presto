/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/


#ifndef _URL_DFR_H_
#define _URL_DFR_H_

#define NEW_DATAFILE_CLASS

#ifdef FORMATS_DATAFILE_ENABLED

#define DATA_FILE_CURRENT_VERSION 0x00001000

#include "modules/datastream/fl_lib.h"

class DataFile_Record;
class SSL_varvector32;



class DataFile_Record : public DataStream_GenericRecord_Large
{
public:

	/** Default connstructor */
	DataFile_Record();

	/**
	 *	Constructor
	 *
	 *	@param	_tag	The specified tag number of the record
	 */
	DataFile_Record(uint32 _tag);

	/** Destructor */
	virtual ~DataFile_Record();

	/** 
	  *	Allocates a new record of the kind used in connection with the 
	  * actual stream implementation.
	  *
	  * @return  DataStream_GenericRecord_Large *	Pointer to the new record. NULL *only* if allocation failes 
	  */
	virtual DataStream *CreateRecordL();

	virtual void	PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);

	/** 
	 *	Retrieves the next available record from this datastream.
	 *  Unless the stream has no more data, it always returns a pointer to a loading record,
	 *  even if the record is unfinished. Subsequent calls to this function is necessary in that case.
	 *  As long as a record is unfinished a pointer to the record will be kept.
	 *  Destruction of the record will remove this connection
	 *
	 *  @return  DataFile_Record *	A pointer to the currently loading record, 
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
	DataFile_Record *GetNextRecordL()
		{return (DataFile_Record *) DataStream_GenericRecord_Large::GetNextRecordL();}

	/**
	 *	Adds content to the payload from a memory buffer
	 *
	 *	@param	src		pointer to buffer that will be added to the payload
	 *	@param	len		the length of the buffer being added
	 */
	void AddContentL(const byte *src, uint32 len)
		{DataStream_GenericRecord_Large::AddContentL(src, len);}

	/*
	 *	Adds content to the payload from a DataStream
	 *
	 *	@param	src		pointer to stream holding the data that will be added to the payload
	 *	@param	len		the number of bytes to be read from src and added to the payload (0 means every thing)
	 *
	 *	@return	OP_STATUS	status code. OpStatus::ERR if the specified number of bytes could not be read
	 */
	//OP_STATUS AddContentL(DataStream *src, uint32 len= 0)
	//	{return DataStream_GenericRecord_Large::AddContentL(src, len);}

	/**
	 *	Adds a record to the payload. The record will be encoded in its proper format
	 *	The record must be finished
	 *
	 *	@param	src		pointer to the record that will be added to the payload
	 */
	void AddContentL(DataStream_BaseRecord *src)
		{DataStream_GenericRecord_Large::AddContentL(src);}

	/**
	 *	Add a null terminated string to the record payload
	 *	The null termination is not stored
	 *
	 *	@param	src		Allocated Null terminated string
	 */
	void AddContentL(const char *src){
		DataStream_GenericRecord_Large::AddContentL(src);
	}


	/**
	 *	Add an 8 bit string object to the record payload
	 *	The null termination is not stored
	 *
	 *	@param	src		The string
	 */
	void AddContentL(const OpStringC8 &src){
		DataStream_GenericRecord_Large::AddContentL(src);
	}


	/**
	 *	Add an 8 bit unsigned integer to the payload
	 *
	 *	@param	src		The integer
	 */
	void AddContentL(uint8 src){
		AddContentL(&src, 1);
	}


	/**
	 *	Add a 16 bit unsigned integer to the payload
	 *
	 *	@param	src		The integer
	 */
	void AddContentL(uint16 src){
		WriteIntegerL(src, 2, spec.numbers_big_endian, FALSE, this);
	}


	/**
	 *	Add a 32 bit unsigned integer to the payload
	 *
	 *	@param	src		The integer
	 */
	void AddContentL(uint32 src){
		WriteIntegerL(src, 4, spec.numbers_big_endian, FALSE, this);
	}


#if 0
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
#endif

	/**
	 *	Return the next payload  byte
	 *
	 *	@return uint8	an 8 bit unsigned integer
	 */
	uint8 GetUInt8L(){
		uint8 buf=0;

		ReadDataL((byte *) &buf, 1);
		return buf;
	}


	/**
	 *	Return the two next payload bytes as a 16 bit unsigned integer
	 *	Storage is determined by the data specification
	 *
	 *	@return uint16	a 16 bit unsigned integer
	 */
	uint16 GetUInt16L(){
		uint32 buf=0;

		ReadIntegerL(this,buf, 2, spec.numbers_big_endian, FALSE);
		return (uint16) buf;
	};

	/**
	 *	Return the 4 next payload bytes as a 32 bit unsigned integer
	 *	Storage is determined by the data specification
	 *
	 *	@param	handle_oversized	The integer is the entire record, if it is too long, ignore the most significant bits
	 *	@return uint32	a 32 bit unsigned integer
	 */
	uint32 GetUInt32L(BOOL handle_oversized= FALSE){
		uint32 buf=0;

		ReadIntegerL(this,buf, (handle_oversized && spec.numbers_big_endian ? GetLength() : 4), spec.numbers_big_endian, FALSE);
		return (uint32) buf;
	}

	/**
	 *	Return the 8 next payload bytes as a 64 bit unsigned integer
	 *	Storage is determined by the data specification
	 *
	 *	@param	handle_oversized	The integer is the entire record, if it is too long, ignore the most significant bits
	 *	@return OpFileLength	a 64 bit unsigned integer
	 */
	OpFileLength GetUInt64L(BOOL handle_oversized= FALSE){
		OpFileLength buf=0;

		ReadInteger64L(this,buf, (handle_oversized && spec.numbers_big_endian ? GetLength() : 8), spec.numbers_big_endian, FALSE);
		return buf;
	}


	/**
	 *	Return the next payload byte
	 *
	 *	@return int8	an 8 bit signed integer
	 */
	int8 GetInt8L();

	/**
	 *	Return the two next payload bytes as a 16 bit signed integer
	 *	Storage is determined by the data specification
	 *
	 *	@return int16	an 16 bit signed integer
	 */
	int16 GetInt16L();

	/**
	 *	Return the 4 next payload bytes as a 32 bit signed integer
	 *	Storage is determined by the data specification
	 *
	 *	@return int32	an 32 bit signed integer
	 */
	int32 GetInt32L();

	/**
	 *	Get the first record from the indexed list that has the give tag id
	 *
	 *	@param	_tag		The tag id of the desired record 
	 *
	 *	@return	DataStream_GenericRecord_Large *	Pointer to the requested record. NULL is returned if it was not available
	 */
	DataFile_Record *GetIndexedRecord(uint32 _tag)
		{return (DataFile_Record *) DataStream_GenericRecord_Large::GetIndexedRecord(_tag);};

	/**
	 *	Get the first record after the record ptag from the indexed list 
	 *	that has the give tag id (or if the tag is 0, the same tag id as the prec record)
	 *
	 *	@param	p_rec	The record preceding the part of the indexed list 
	 *					that will be searched for the desired record
	 *					If this pointer is NULL, the search will start 
	 *					at the beginning of the list, and tag must be specified
	 *					If the record is not in the list 
	 *	@param	_tag	The tag id of the desired record 
	 *					If tag is 0 (default) tag assumed to be the 
	 *					same as prec->GetTag() if prec is not NULL
	 *
	 *	@return	DataStream_GenericRecord_Large *	Pointer to the requested record. NULL is returne if it was not 
	 *								available, or prec was not in the list of indexed records
	 */
	DataFile_Record *GetIndexedRecord(DataStream_GenericRecord_Large *p_rec, uint32 _tag = 0)
		{return (DataFile_Record *) DataStream_GenericRecord_Large::GetIndexedRecord(p_rec, _tag);};

	/**
	 *	Allocate and return a nullterminated 8 bit string
	 *	The string is create from the entire payload of the 
	 *	first record with the identified tag number
	 *	Caller must free string
	 *
	 *	@param	_tag	The tagnumber of the requested record
	 *	@return	char *	The allocated string
	 */
	char *GetIndexedRecordStringL(uint32 _tag);

	/**
	 *	Return an 8 bit string in the string argument
	 *	The string is create from the entire payload of the 
	 *	first record with the identified tag number
	 *
	 *	@param	_tag	The tagnumber of the requested record
	 *	@param	string	The object where the string will be stored
	 */
	void GetIndexedRecordStringL(uint32 _tag, OpStringS8 &string);

	/**
	 *	Return TRUE if the record with the given tag exist
	 *	FALSE if not.
	 *
	 *	@param	_tag	The tagnumber of the requested record
	 *	@return BOOL	TRUE if the requested record was found
	 */
	BOOL GetIndexedRecordBOOL(uint32 _tag);

	/**
	 *	Return the 4 first payload bytes in the first available 
	 *	record with the specified tag number as a 32 bit unsigned integer
	 *	Storage is determined by the data specification
	 *
	 *	@return uint32	a 32 bit unsigned integer
	 */
	unsigned long GetIndexedRecordUIntL(uint32 _tag);
	OpFileLength GetIndexedRecordUInt64L(uint32 _tag);

	/**
	 *	Return the 4 first payload bytes in the first available 
	 *	record with the specified tag number as a 32 bit signed integer
	 *	Storage is determined by the data specification
	 *
	 *	@param	_tag	The tagnumber of the requested record
	 *	@return int32	an 32 bit signed integer
	 */
	long GetIndexedRecordIntL(uint32 _tag);


	/**
	 *	Add a record with the given tag, and the string referenced in src
	 *
	 *	@param tag	the tag of the record
	 *	@param src	the string to be copied
	 */
	void AddRecordL(uint32 tag, const OpStringC8 &src); // Should be UTF8
	void AddRecordL(uint32 tag, const char *src){ // Should be UTF8
		OpStringC8 temp_src(src);
		AddRecordL(tag, temp_src);
	}


	/**
	 *	Add a record with the given tag, and the integer value
	 *
	 *	@param tag	the tag of the record
	 *	@param value the string to be copied
	 */
	void AddRecordL(uint32 tag, uint32 value);
	void AddRecord64L(uint32 tag, OpFileLength value);

	/*
	 *	Add a record with the given tag, and the integer value
	 *
	 *	@param tag	the tag of the record
	 *	@value		the string to be copied
	 */
	//OP_STATUS AddRecordL(uint32 tag, uint16 value);

	/*
	 *	Add a record with the given tag, and the integer value
	 *
	 *	@param tag	the tag of the record
	 *	@value		the string to be copied
	 */
	//OP_STATUS AddRecordL(uint32 tag, uint8 value);

	/**
	 *	Add an empty record with the given tag
	 *
	 *	@param tag	the tag of the record
	 */
	void AddRecordL(uint32 tag);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName();
#endif
};

class DataFile : public DataStream_GenericFile
{
private:
	/** The record field specifications */
	DataRecord_Spec spec;

	/** The file version number */
	uint32		file_version;

	/** The application version number */
	uint32		app_version;

public:

	/** 
	 *	Contructor for reading file 
	 *	NOTE: Init() MUST be called by the owner after the object is constructed
	 *
	 *	@param file_p	The file from which data will be read
	 */
	DataFile(OpFileDescriptor *file_p);

	/** 
	 *	Contructor for writing file
	 *	NOTE: Init() MUST be called by the owner after the object is constructed
	 *
	 *	@param file_p	The file from which data will be read
	 *	@param app_ver	The application version number
	 *	@param taglen	How many bytes will the tags be stored as
	 *	@param lenlen	How many bytes will the length of the payload be stored as
	 */
	DataFile(OpFileDescriptor *file_p,
			 uint32 app_ver, uint16 taglen, uint16 lenlen);

	/** Destructor */
	virtual ~DataFile();

	/** Returns the application version number of the file */
	uint32 AppVersion() const
			{return app_version;};

protected:

	/** 
	  *	Allocates a new record of the kind used in connection with the 
	  * actual stream implementation.
	  *
	  * @return  DataStream_GenericRecord_Large *	Pointer to the new record. NULL *only* if allocation failes 
	  */
	virtual DataStream *CreateRecordL();

public:
	/** 
	 *	Returns the fixed record specification used by the stream.
	 *  This function is only used by fixed size record formats, and only need to be defined 
	 *	for datastreams that supports this.
	 *  
	 *	@return const DataRecord_Spec *		The pointer to the record specification, if available, NULL otherwise.
	 */
	virtual const DataRecord_Spec *GetRecordSpec();

	void SetRecordSpec(const DataRecord_Spec &new_spec){spec = new_spec;};
	void SetRecordSpec(const DataRecord_Spec *new_spec){if(new_spec) SetRecordSpec(*new_spec);};

	virtual void	PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);

	/** 
	 *	Retrieves the next available record from this datastream.
	 *  Unless the stream has no more data, it always returns a pointer to a loading record,
	 *  even if the record is unfinished. Subsequent calls to this function is necessary in that case.
	 *  As long as a record is unfinished a pointer to the record will be kept.
	 *  Destruction of the record will remove this connection
	 *
	 *  @return  DataFile_Record *	A pointer to the currently loading record, 
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
	DataFile_Record *GetNextRecordL()
		{return (DataFile_Record *) DataStream::GetNextRecordL();}

	/**
	 *	Retrieves the next available record from this datastream.
	 *  Unless the stream has no more data, it always returns a pointer to a loading record,
	 *  even if the record is unfinished. Subsequent calls to this function is necessary in that case.
	 *  As long as a record is unfinished a pointer to the record will be kept.
	 *  Destruction of the record will remove this connection
	 *
	 *  @param[out] next_record		A pointer to the currently loading record,
	 *								or a new record if there is no record loading and there are
	 *								more data available or expected in the stream.
	 *								A NULL pointer is returned if a record cannot be created at the present time.
	 *								This may happen when there is not enough data in the stream to determine
	 *								the type of record, or when there is no more data in the stream.
	 *								MoreData() must be called in such cases.
	 *
	 *								The allocated record must be deleted by the caller.
	 *								The returned record may be accessed as a datastream before
	 *								it is completely loaded, assuming that the caller is able to handle
	 *								streaming data.
	 *
	 * @see GetNextRecordL() - Same function but using LEAVE instead of OP_STATUS.
	 */
	OP_STATUS GetNextRecord(DataFile_Record *&next_record)
		{ TRAPD(status, next_record = GetNextRecordL()); return status; }

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName();
#endif


};

#endif // FORMATS_DATAFILE_ENABLED

#endif
