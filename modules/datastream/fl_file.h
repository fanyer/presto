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

#ifndef _FL_FILE_H_
#define _FL_FILE_H_

#ifdef DATASTREAM_GENERIC_FILE_ENABLED

#include "modules/util/opfile/opfile.h"

/** 
 *	DataStream_GenericFile: DataStream implementation for standard files
 *  This implementation handles all general input and output.
 *  Special handling must be added by subclasses.
 *
 *	This class produces files of the generic binary format used in Opera's 
 *	Cache, cookie and security databases.
 */
class DataStream_GenericFile : public DataStream
{
	/** the source, or destination fil */   
	OpFileDescriptor *file;
	UINT writing:1;
	UINT existed:1;
	UINT take_over:1;

	DataStream_FIFO_Stream scan_buffer;

public:
	/** 
	 *	Called after allocation and construction by the owner of 
	 *	the object before any operations are conducted
	 *
	 *	This function may be implemented by subclasses.
	 *	Such implementations MUST initialize any objects needed by InitRead 
	 *	or InitWrite before calling the base class implementation of Init() .
	 *	They MUST NOT call InitRead of InitWrite explicitly.
	 */
	void InitL(){PerformActionL(DataStream::KInit);}
	OP_STATUS Init() { TRAPD(status, InitL()); return status; }

public:

	/**
	 *	Constructor (OpFileDescriptor)
	 *
	 *	@param	file_p	reference to the file to be used.
	 *					The file is taken over by the object, unless _take_over 
	 *					is FALSE (file will be closed in any case)
	 *					The file must be open for reading, unless _write is TRUE
	 *	@param	_write	TRUE if the file is opened for writing
	 *	@param	_take_over	TRUE (default) if this object takes ownership of the file
	 */
	DataStream_GenericFile(OpFileDescriptor *file_p, BOOL _write = FALSE, BOOL _take_over=TRUE);

	/** Destructor */
	virtual ~DataStream_GenericFile();

	/** Close the file */
	void Close();

	/** Close in case of error */
	void ErrorClose();

#ifdef DATASTREAM_ENABLE_FILE_OPENED
	/** 
	 *	Is the file valid? 
	 *
	 *	@return	BOOL returns TRUE if the file is open
	 *
	 *	Requires caller module to import API_DATASTREAM_FILE_OPENED
	 */
	BOOL Opened();
#endif // DATASTREAM_ENABLE_FILE_OPENED

	/** 
	 *	Read up to the len next bytes from the stream, updating position
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

	/**
	 *	Adds a record to the payload. The record will be encoded in its proper format
	 *	The record must be finished
	 *
	 *	@param	src		pointer to the record that will be added to the payload
	 *
	 *	Requires calling module to import API_DATASTREAM_FILE_ADD_STREAM_CONTENT
	 */
	void AddContentL(DataStream *src);

	virtual uint32 GetAttribute(DataStream::DatastreamUIntAttribute attr);
	virtual void	PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);
private:

	/** 
	 *	Read up to the len next bytes from the file, updating position
	 *
	 *	@param	buffer	pointer to the location where the bytes will be stored
	 *					Must be at least len bytes
	 *	@param	len		Number of bytes to read
	 *
	 *  @return unsigned long	Number of bytes actually read, may be less than specified in len
	 */
	unsigned long ReadFromFile(unsigned char *buffer, unsigned long len);

	/** Clear and free internal buffers */
	//void ClearBuffer();

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName();
#endif

};
#endif // DATASTREAM_GENERIC_FILE_ENABLED

#endif // _FL_FILE_H_
