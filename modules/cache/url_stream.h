/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __URL_STREAM_H__
#define __URL_STREAM_H__

#ifdef _URL_STREAM_ENABLED_
#include "modules/datastream/fl_lib.h"

class URL_DataStream : public DataStream
{
private:
	URL_InUse source_target;

	URL_DataDescriptor *reader;
	BOOL more_data;

	BOOL m_raw;
	BOOL m_decoded;

public:
	URL_DataStream(URL &url, BOOL get_raw_data=FALSE, BOOL get_decoded_data=TRUE);
	virtual ~URL_DataStream();

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
	 *	Write the len bytes in buffer to the stream
	 *
	 *	@param	buffer 	pointer to the location where the bytes are stored
	 *					Must be at least len bytes
	 *	@param	len		Number of bytes to write
	 */
	virtual void	WriteDataL(const unsigned char *buffer, unsigned long len);

	/** 
	  *	Allocates a new record of the kind used in connection with the 
	  * actual stream implementation.
	  *
	  * @return  DataStream_GenericRecord *	Pointer to the new record. NULL *only* if allocation failes 
	  */
#ifdef FORMATS_DATAFILE_ENABLED
	virtual DataStream *CreateRecordL();
#endif // FORMATS_DATAFILE_ENABLED

	virtual uint32 GetAttribute(DataStream::DatastreamUIntAttribute attr);
	virtual void PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);

private:

	/** Creates reader if necessary, returns TRUE if a reader is present*/
	BOOL CheckReader();


#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "URL_DataStream";}
#endif
};

#endif // _URL_STREAM_ENABLED_

#endif
