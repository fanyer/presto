/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef SSLVARL1_H
#define SSLVARL1_H

#ifdef _NATIVE_SSL_SUPPORT_

typedef LoadAndWritableList SSL_Handshake_Message_Base;

class SSL_LoadAndWritable_list: public SSL_Handshake_Message_Base{
private:
	DataStream_FlexibleSequence &payload;

	uint32 max_length;

private:
    SSL_LoadAndWritable_list(const SSL_LoadAndWritable_list &old); // NonExistent
    SSL_LoadAndWritable_list &operator =(const SSL_LoadAndWritable_list &old); // NonExistent

protected:
    uint32 MaxLength() const{return max_length;};

    SSL_LoadAndWritable_list(DataStream_FlexibleSequence &payld, uint32 len_size, uint32 max_len);
public:
    virtual ~SSL_LoadAndWritable_list();

    void Resize(uint32 nlength);

    uint32 Count() const{return payload.Count();};
    DataStream *Item(uint32 idx){return payload.Item(idx);}
    const DataStream *Item(uint32 idx) const{return payload.Item(idx);};

    BOOL Contain(DataStream &);
	BOOL Contain(DataStream &source, uint32& index);
    void Copy(SSL_LoadAndWritable_list &source);
    void Transfer(uint32 start, SSL_LoadAndWritable_list &source,uint32 start1, uint32 len);
    void CopyCommon(SSL_LoadAndWritable_list &ordered_list, SSL_LoadAndWritable_list &master_list, BOOL reverse=FALSE);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_LoadAndWritable_list";}
#endif
};

template <class T>
class SSL_Stream_TypedPipe : public DataStream_Pipe, public SSL_Error_Status
{
public:
	SSL_Stream_TypedPipe(DataStream *src, BOOL take_over=TRUE):DataStream_Pipe(src, take_over){}
	virtual ~SSL_Stream_TypedPipe(){}

protected:
	virtual DataStream *CreateRecordL();

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_Stream_TypedPipe";}
#endif
};

template <class T>
class SSL_Stream_TypedSequence : public DataStream_FlexibleSequence, public SSL_Error_Status
{
private:
	T default_item;

	SSL_Stream_TypedPipe<T> source_pipe;

private:
    T &Item(uint32);
    const T &Item(uint32) const;

public:
	SSL_Stream_TypedSequence() : source_pipe(NULL){source_pipe.ForwardTo(this); ForwardToThis(default_item);};
	virtual ~SSL_Stream_TypedSequence();

	virtual DataStream *CreateRecordL();
	virtual DataStream *GetInputSource(DataStream *src);

    SSL_Stream_TypedSequence &operator =(SSL_Stream_TypedSequence &src){Set(src); return *this;}
    SSL_Stream_TypedSequence &operator =(const T &src){Set(src); return *this;}

    void Set(SSL_Stream_TypedSequence &);
    void Set(const T &);

    T &operator [](uint32 item){return Item(item);};
    const T &operator [](uint32 item) const{return Item(item);};

	T *First(){return (T *) Head::First();}
	T *Last(){return (T *) Head::Last();}

    void Resize(uint32 nlength);
    BOOL Contain(T &);
	BOOL Contain(T &source, uint32& index);
    void Copy(SSL_Stream_TypedSequence &source);
    void Transfer(uint32 start, SSL_Stream_TypedSequence &source,uint32 start1, uint32 len);
    void CopyCommon(SSL_Stream_TypedSequence &ordered_list, SSL_Stream_TypedSequence &master_list, BOOL reverse=FALSE);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_Stream_TypedSequence";}
#endif
};


/**
 *  Variable list template
 *
 *  @param	T	Class derived from SSL_LoadAndWritable
 *
 *	@param	len_size	Number of bytes to use in length field
 *
 *	@param	max_size	maximum number of bytes allowed in representation
 *						Must be < 2^(len_size * 8)
 *
 *
 *  Put implementation of the templates on the form
 *
 *		template class SSL_LoadAndWriteableListType<SSL_LoadAndWritable, 1, 255>;
 *
 *  in the file sslvarl1.cpp
 */
template<class T, int len_size, int max_len>
class SSL_LoadAndWriteableListType : public SSL_LoadAndWritable_list
{
private:
	SSL_Stream_TypedSequence<T> full_payload;

private:
    T &Item(uint32 idx){return full_payload[idx];}
    const T &Item(uint32 idx) const{return full_payload[idx];}

public:
    SSL_LoadAndWriteableListType()
        : SSL_LoadAndWritable_list(full_payload, len_size, max_len) {
        AddItem(full_payload);
    }
    //SSL_LoadAndWriteableListType(SSL_LoadAndWriteableListType &other);
	virtual ~SSL_LoadAndWriteableListType();

    SSL_LoadAndWriteableListType &operator =(SSL_LoadAndWriteableListType &src){Set(src); return *this;}
    SSL_LoadAndWriteableListType &operator =(const T &src){Set(src); return *this;}

	void Set(SSL_LoadAndWriteableListType &src){full_payload.Set(src.full_payload);}
	void Set(const T &src){full_payload.Set(src);}

    T &operator [](uint32 item){return Item(item);};
    const T &operator [](uint32 item) const{return Item(item);};

    void Resize(uint32 nlength){full_payload.Resize(nlength);}
    BOOL Contain(T &src){return full_payload.Contain(src);}
	BOOL Contain(T &source, uint32& index){return full_payload.Contain(source, index);}
    void Copy(SSL_LoadAndWriteableListType &source){full_payload.Copy(source.full_payload);}
    void Transfer(uint32 start, SSL_LoadAndWriteableListType &source,uint32 start1, uint32 len){full_payload.Transfer(start, source.full_payload, start1, len);}
    void CopyCommon(SSL_LoadAndWriteableListType &ordered_list, SSL_LoadAndWriteableListType &master_list, BOOL reverse=FALSE){full_payload.CopyCommon(ordered_list.full_payload, master_list.full_payload, reverse);}


#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_LoadAndWriteableListType";}
#endif
};


typedef SSL_LoadAndWriteableListType<SSL_varvector16, 2, 0xffff> SSL_varvector16_list;
typedef SSL_LoadAndWriteableListType<SSL_varvector24, 3, 0xffffff> SSL_varvector24_list;

#endif
#endif // SSLVARL1_H
