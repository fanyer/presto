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


#ifndef __FL_LIST_H__
#define __FL_LIST_H__

#ifdef DATASTREAM_SEQUENCE_ENABLED

#include "modules/datastream/fl_lpipe.h"


/** Control a sequence of DataStream based records
 *
 *	This class administrates reading and writing of general structured records
 *	
 *	Each element in the least can have its own application specific ID.
 *
 *	As each element is read, or written the implementation may perform required actions based on 
 *	the current position in the list, and the ID of the read/written object, such as disabling records,
 *	update or add them.
 */
class DataStream_SequenceBase : public DataStream, public Head
{
public:
	enum {
		STRUCTURE_START = DATASTREAM_SPECIAL_START_VALUE + 0, // This indicates that the first element of the record structure is about to be loaded or written
		STRUCTURE_FINISHED = DATASTREAM_SPECIAL_START_VALUE + 1, // This indicates that the last element of the record structure has been loaded or written
		BASERECORD_SPECIAL_START_VALUE,  // start new special IDs in subclasses from this value
		BASERECORD_START_VALUE = DATASTREAM_START_VALUE
	};

private:
	/** The item currently being read */
	DataStream *current_item;

	/** The current step of the reading process. Step zero is the intial step*/
	int step;

protected:
	/** 
	 *	Returns a pointer to the input stream.
	 *
	 *	Implementation may create pipes that uses the src stream 
	 *	as a source, and return the pipe. Such pipes should be careful 
	 *	about how much data they consume.
	 */
	virtual DataStream *GetInputSource(DataStream *src);

	/** 
	 *	Returns a pointer to the output stream.
	 *
	 *	Implementation may create pipes that uses the given stream as a target.
	 *
	 *	WriteActionL with the itemid STRUCTURE_FINISHED MUST flush such pipes
	 */
	virtual DataStream *GetOutputTarget(DataStream *trgt);


	/** Read or continue to read from the given source, at most up until the last_element has been read*/
	OP_STATUS ReadRecordSequenceFromStreamL(DataStream *src, DataStream *last_element);

	/** Write this sequence to the given target, optionally from or after the start element, and to and including the given last_element
	 *
	 *	@param	target	Stream to write to
	 *	@param	start_after		If TRUE start *after* the element start_element, if FALSE start_element is the first element
	 *	@param	start_element	Indicates the start point of the sequence to write, depending on the value of start_after
	 *	@param	last_element	Stop after this element has been written
	 */
	void WriteRecordSequenceL(DataStream *target, BOOL start_after, DataStream *start_element, DataStream *last_element);

public:

	/** Default constructor */
	DataStream_SequenceBase();
	virtual ~DataStream_SequenceBase();

	uint32 GetLength(){return GetAttribute(DataStream::KCalculateLength);};
	virtual uint32 GetAttribute(DataStream::DatastreamUIntAttribute attr);
	virtual OP_STATUS	PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target);
	virtual void	PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);

	DataStream *First(){return (DataStream *) Head::First();}
	DataStream *Last(){return (DataStream *) Head::Last();}


#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "DataStream_SequenceBase";}
	virtual const char *Debug_OutputFilename(){return "stream.txt";};
public:
	virtual void Debug_Write_All_Data();

#endif

};


#ifdef DATASTREAM_USE_FLEXIBLE_SEQUENCE
/** Maintains a sequence/array of objects of records
 *
 *	The sequence can be accessed as an array, searched for the existens of a 
 *	specific binary record, lists can be copied, appended, and items that also exists 
 *	in another list can be extracted into a new list.
 */
class DataStream_FlexibleSequence : public DataStream_SequenceBase
{
private:
	DataStream *currently_loading_element;

public:
	DataStream_FlexibleSequence();
	virtual ~DataStream_FlexibleSequence();

	virtual OP_STATUS PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target);
	virtual void	PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);

    void ResizeL(uint32 nlength);
	void Resize(uint32 nlength);

    /** Get the number of objects in the list */
    uint32 Count() const{return Cardinal();};

	/** Access the specified item in the sequence */
    DataStream *Item(uint32);
    const DataStream *Item(uint32) const;

	/** Does the sequence contain a binary match for the given object? */
    BOOL ContainL(DataStream &source){uint32 index=0;	return ContainL(source, index);};
    BOOL Contain(DataStream &);
	/** Does the sequence contain a binary match for the given object, and if so, which index? */
	BOOL ContainL(DataStream &source, uint32& index);
	BOOL Contain(DataStream &source, uint32& index);

	/** Copy all the elements in the source list into this list, destroying the prevous content of this list */
    void CopyL(DataStream_FlexibleSequence &source);
    void Copy(DataStream_FlexibleSequence &source);

	/** Copy selected elements in the source list into this list, replacing specific 
	 *	elements in this list, but does not extend the list.
	 *
	 *	@param	start	The first element in this list to replace
	 *	@param	source	The list to copy from
	 *	@param	start1	The first item in the source list to copy
	 *	@param	len		Maximum number of elements to copy (restricted to the minimum of the available elements in this list and the source)
	 */
    void TransferL(uint32 start, DataStream_FlexibleSequence &source,uint32 start1, uint32 len);
    void Transfer(uint32 start, DataStream_FlexibleSequence &source,uint32 start1, uint32 len);

	/** Copy the elements from ordered_list that also exists in the master_list, in the order 
	 *	(or reverse order) of their position in the ordered_list
	 *
	 *	@param	ordered_list	The ordered_version of the elements the master_list is extracted from, controls the order of the resulting list
	 *	@param	master_list		The unordered list used to select the elements from the ordered list.
	 *	@param	reverse			If TRUE, the items copied will have their order reverses compared to the corresponding items in the ordered list
	 */
    void CopyCommonL(DataStream_FlexibleSequence &ordered_list, DataStream_FlexibleSequence &master_list, BOOL reverse=FALSE);
    void CopyCommon(DataStream_FlexibleSequence &ordered_list, DataStream_FlexibleSequence &master_list, BOOL reverse=FALSE);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "DataStream_FlexibleSequence";};
#endif

};

#ifdef DATASTREAM_PIPE_ENABLED
/** Typed template DataStream_Pipe, using a DataStream object of the specified type 
 *	as a source or target
 */
template <class T> class DataStream_TypedPipe : public DataStream_Pipe
{
public:
	/** Constructor, set the pipe up to use the specified source, if necessary take ownership of the object */
	DataStream_TypedPipe(DataStream *src, BOOL take_over=TRUE):DataStream_Pipe(src, take_over){}
	virtual ~DataStream_TypedPipe(){}

protected:
	virtual DataStream *CreateRecordL(){ return OP_NEW_L(T, ());}

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "DataStream_TypedPipe";}
#endif
};

/** Typed flexible sequence of Datastream objects */
template <class T>
class DataStream_TypedSequence : public DataStream_FlexibleSequence
{
private:
	T default_item;

	DataStream_TypedPipe<T> source_pipe;

private:
    T &Item(uint32); 
    const T &Item(uint32) const;

public:
	DataStream_TypedSequence() : source_pipe(NULL){};
	virtual ~DataStream_TypedSequence();

	virtual DataStream *CreateRecordL();/*{ return new (ELeave) T;}*/
	virtual DataStream *GetInputSource(DataStream *src);/*{source_pipe.SetStreamSource(src, FALSE); return &source_pipe;}*/

	/** Assignement operator, other sequence */
    DataStream_TypedSequence &operator =(DataStream_TypedSequence &);
	/** Assignement operator, actual type */
    DataStream_TypedSequence &operator =(T &);
    
	/** Array dereferencing, read/write access */
    T &operator [](uint32 item){return Item(item);}; 

	/** Array dereferencing, read only access */
    const T &operator [](uint32 item) const{return Item(item);};     

	T *First(){return (T *) Head::First();}
	T *Last(){return (T *) Head::Last();}
};

#endif // DATASTREAM_PIPE_ENABLED
#endif // DATASTREAM_USE_FLEXIBLE_SEQUENCE

typedef DataStream_BaseRecord Structured_GenericRecord;

#endif // DATASTREAM_SEQUENCE_ENABLED

#endif
