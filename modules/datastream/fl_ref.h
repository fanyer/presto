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


#ifndef __FL_REF_H__
#define __FL_REF_H__

#ifdef DATASTREAM_REFERENCE_ENABLED

/** 
 *  Makes it possible to let a baseclass use object to use unknown 
 *  implementations for a common puporse, without resorting to virtual API's
 *  Does not destroy the target during destruction 
 *
 *	API functions remap directly to the target object, unless specified otherwise
 */
class DataStream_StreamReference : public DataStream
{
private:
	DataStream *target;

public:
	/** Constructor
	 *
	 *	Sets the specified trgt object as the current reference target
	 *
	 *	@param trgt	The object to reference
	 */
	DataStream_StreamReference(DataStream *trgt);

	/** Destructor. Does not detroy the target */
	virtual ~DataStream_StreamReference();

	/** This object will from now point on the trgt object instead of the current one
	 *	Current object is not deleted
	 *
	 *	@param trgt	The object to reference
	 */
	void SetReference(DataStream *trgt){target = trgt;}

	/** Return the pointer to the currently referenced object
	 *
	 *	@return DataStream	Pointer to the referenced object
	 */
	DataStream *GetReference(){return target;}
	const DataStream *GetReference() const{return target;}

	virtual OP_STATUS PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target);

	virtual unsigned long ReadDataL(unsigned char *buffer, unsigned long len, DataStream::DatastreamCommitPolicy sample=DataStream::KReadAndCommit);
	virtual void	WriteDataL(const unsigned char *buffer, unsigned long len);
	virtual DataStream *CreateRecordL();
	virtual uint32 GetAttribute(DataStream::DatastreamUIntAttribute attr);
	virtual void	PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);

	virtual void SetItemID(int id){PerformActionL(DataStream::KSetItemID, id);}

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "DataStream_StreamReference";}
	virtual const char *Debug_OutputFilename(){return "stream.txt";};
public:
	virtual void Debug_Write_All_Data();
#endif
};


/** Typed template used to reference specific types, forbidding references to other DataStream classes */
template <class T> class DataStream_Reference : public DataStream_StreamReference
{
public:
	/** Constructor
	 *
	 *	Sets the specified trgt object as the current reference target
	 *
	 *	@param ref	The object to reference
	 */
	DataStream_Reference(T *ref): DataStream_StreamReference(ref){};

	/** Destructor. Does not detroy the target */
	virtual ~DataStream_Reference(){};

	/** This object will from now point on the trgt object instead of the current one
	 *	Current object is not deleted
	 *
	 *	@param trgt	The object to reference
	 */
	void SetReference(T *trgt){DataStream_StreamReference::SetReference(trgt);}

	/** Return the pointer to the currently referenced object
	 *
	 *	@return DataStream	Pointer to the referenced object
	 */
	T *GetReference(){return (T *) DataStream_StreamReference::GetReference();}
	const T *GetReference() const{return (const T *) DataStream_StreamReference::GetReference();}

	/** Assignment Operator, same as SetReference */
	DataStream_Reference &operator =(T *ref){ DataStream_StreamReference::SetReference(ref); return *this;}

	/** Type conversion, Same as GetReference*/
	operator T *(){return (T *)  DataStream_StreamReference::GetReference();}
	operator const T *()const{return (const T *)  DataStream_StreamReference::GetReference();}

	/** Derefenecing opertor, Same as object.GetReference()->function() */
	T *operator ->(){return (T *)  DataStream_StreamReference::GetReference();}
	const T *operator ->()const{return (const T *)  DataStream_StreamReference::GetReference();}

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "DataStream_Reference";}
#endif
};

#endif // DATASTREAM_REFERENCE_ENABLED

#endif // __FL_REF_H__
