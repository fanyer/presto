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


#ifndef __FL_INT_H__
#define __FL_INT_H__

#ifdef DATASTREAM_UINT_ENABLED

#include "modules/datastream/fl_base.h"
class DataStream_ByteArray;


/** General unsigned int record handler
 *	This class can be used to create record elements of a specified length (in bytes), 
 *	endinanness, and MSB detectionpolicy.
 *
 *	Maximum size of integers is currently limited to 32 bit, but longer integers can be 
 *	created or read, but higher bits (except the MSB) will be nulled.
 *
 *	Default:	Big-endian, no MSB detection
 */
class DataStream_UIntBase : public DataStream
{
private:
	uint32 read_data:1; 
protected:

	uint32 size:8;
	uint32 big_endian:1;
	uint32 MSB_detection:1;
	uint32 value;

	OpAutoPtr<DataStream_ByteArray> integer_buffer;

public:
	/** Contructor 
	 *
	 *	Constructs an unsigned integer with the given size and value
	 *
	 *	@param	sz	Size (in bytes) of the integer
	 *	@param	val Initial value of the integer
	 */
	DataStream_UIntBase(uint32 sz, uint32 val=0);
	//virtual DataStream *CreateRecordL();

public:

	/** Update the value of the integer
	 *
	 *	@param	val New value of the integer
	 */
	void SetValue(uint32 val){value = val;}

	/** Get the value of this integer
	 *
	 *	@return uint32 The current value of the contained integer
	 */
	uint32 GetValue() const{return value;}

	/**	Update the size of the integer
	 *
	 *	@param	sz	The new size (in bytes) of the integer
	 */
	void SetSize(uint32 sz){size = sz;}

	/** Get the size of the integer value
	 *
	 *	@return uint32	The size (in bytes) of the contained integer
	 */
	uint32 GetLength() const{return size;}

	/**	Set the endianness to be used when reading and writing the value
	 *
	 *	@param	big_end		If TRUE the value will be written in bigendian form, otherwise in little endian form
	 */
	void SetBigEndian(BOOL big_end){big_endian = big_end;}

	/** Get the endianness of the format used for this integer
	 *
	 *	@return	BOOL	If TRUE the value will be written in bigendian form, otherwise in little endian form
	 */
	BOOL GetBigEndian() const{return big_endian;}

	/**	Set the MSBpolicy to be used when reading and writing the value
	 *
	 *	@param	msb_det		If TRUE the value will be written in with the MSB bit written in the most 
	 *						significant byte of the output, and during read the MSB of the most significant
	 *						byte read will be copied to the MSB of the contained integer.
	 */
	void SetMSBDetection(BOOL msb_det){MSB_detection = msb_det;}

	/**	Get the MSBpolicy to be used when reading and writing the value
	 *
	 *	@return	BOOL		If TRUE the value will be written in with the MSB bit written in the most 
	 *						significant byte of the output, and during read the MSB of the most significant
	 *						byte read will be copied to the MSB of the contained integer.
	 */
	BOOL GetMSBDetection() const{return MSB_detection;}

	virtual OP_STATUS PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target);

	void	SetReadData(BOOL flag){read_data = flag;}

	virtual uint32 GetAttribute(DataStream::DatastreamUIntAttribute attr);
#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "DataStream_UIntBase";}
	virtual const char *Debug_OutputFilename(){return "stream.txt";};
public:
	virtual void Debug_Write_All_Data();
#endif
};

#endif // DATASTREAM_UINT_ENABLED

#endif // __FL_INT_H__
