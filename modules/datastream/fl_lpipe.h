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


#ifndef __FL_LPIPE_H__
#define __FL_LPIPE_H__

#ifdef DATASTREAM_PIPE_ENABLED

#include "modules/datastream/fl_stream.h"

/**
 *	This class is used to read a given number of bytes from a data source.
 *
 *	Writing to the pipeline is not limited, but not recommende.
 */
class DataStream_LengthLimitedPipe : public DataStream_Pipe
{
private:
	uint32 max_length;
	BOOL length_set;

	uint32 read_length;

public:
	DataStream_LengthLimitedPipe(DataStream *src, BOOL take_over=TRUE);
	virtual ~DataStream_LengthLimitedPipe();

	/** Specify the length of bytes to be read */
	void SetReadLength(uint32 length);

	/** Remove the length specification, and read all available data */
	void UnsetReadLength();

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

	virtual uint32 GetAttribute(DataStream::DatastreamUIntAttribute attr);
	virtual void	PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName();
#endif
};

#endif // DATASTREAM_PIPE_ENABLED

#endif // __FL_LPIPE_H__
