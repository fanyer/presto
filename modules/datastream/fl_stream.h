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

#ifndef __FL_STREAM_H__
#define __FL_STREAM_H__

#ifdef DATASTREAM_PIPE_ENABLED

/** 
 *	This class is intended to be used as part of a processing pipeline
 *	The default action of the members is to read directly from the source stream
 *	Subclasses may add further processing before calling the functions of this class
 */
class DataStream_Pipe : public DataStream
{
protected:
	DataStream *source;
	BOOL own_source;

public:

	/** 
	 *	Constructor 
	 *
	 *	@param	src		Pointer to source stream object
	 *	@param	take_ower	TRUE if this object now owns the source stream, and 
	 */
	DataStream_Pipe(DataStream *src, BOOL take_over=TRUE);

	/** Destructor */
	virtual ~DataStream_Pipe();

	/**
	 *	Change the source stream. This should ONLY be done if the source is not known 
	 *	when the pipe was initialized.
	 *
	 *	@param	src		Pointer to source stream object
	 *	@param	take_ower	TRUE if this object now owns the source stream, and 
	 */
	void SetStreamSource(DataStream *src, BOOL take_over=TRUE);

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
	 *	Write the len bytes in buffer to the stream
	 *
	 *	@param	buffer 	pointer to the location where the bytes are stored
	 *					Must be at least len bytes
	 *	@param	len		Number of bytes to write
	 */
	virtual void	WriteDataL(const unsigned char *buffer, unsigned long len);

	virtual uint32 GetAttribute(DataStream::DatastreamUIntAttribute attr);
	virtual void	PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);
protected:

	/** Use the piped source directly, e.g. in case normal pipe processes must be suspended */
	DataStream *AccessStreamSource(){ return source;}

	/** 
	  *	Allocates a new record of the kind used in connection with the 
	  * actual stream implementation.
	  *
	  * @return  DataStream_GenericRecord *	Pointer to the new record. NULL *only* if allocation failes 
	  */
	//virtual DataStream *CreateRecordL();

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName();
#endif

};

#endif // DATASTREAM_PIPE_ENABLED

#endif
