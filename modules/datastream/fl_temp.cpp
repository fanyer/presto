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

#include "core/pch.h"

#include "modules/datastream/fl_lib.h"

#ifdef DATASTREAM_USE_FLEXIBLE_SEQUENCE
template <class T>
T &DataStream_TypedSequence<T>::Item(uint32 idx)
{
	T* item = (T*) DataStream_FlexibleSequence::Item(idx);
	return(item ? *item : default_item);
}

template <class T>
const T &DataStream_TypedSequence<T>::Item(uint32 idx) const
{
	const T* item = (const T*) DataStream_FlexibleSequence::Item(idx);
	return(item ? *item : default_item);
}

template<class T>
DataStream_TypedSequence<T> &DataStream_TypedSequence<T>::operator =(DataStream_TypedSequence<T> &source)
{
	TRAPD(op_err, CopyL(source));
	OpStatus::Ignore(op_err);

	return *this;
}

template<class T>
DataStream_TypedSequence<T> &DataStream_TypedSequence<T>::operator =(T &source)
{
	OP_STATUS op_err;

	TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, ResizeL(1), *this);

	DataStream_ByteArray source_binary;

	TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, source.WriteRecordL(&source_binary), *this);
	Item(0).ResetRecord();
	TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, Item(0).ReadRecordFromStreamL(&source_binary), *this);
	return *this;
}

template <class T>
DataStream_TypedSequence<T>::~DataStream_TypedSequence(){}

template <class T>
DataStream *DataStream_TypedSequence<T>::CreateRecordL(){ return OP_NEW_L(T, ());}

template <class T>
DataStream *DataStream_TypedSequence<T>::GetInputSource(DataStream *src){source_pipe.SetStreamSource(src, FALSE); return &source_pipe;}

#endif // DATASTREAM_USE_FLEXIBLE_SEQUENCE


/****************************************************/
// Template instantiations 
/****************************************************/

#ifdef DATASTREAM_USE_FLEXIBLE_SEQUENCE
template class DataStream_TypedSequence<class DataStream_ByteArray>;
#endif

#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
#include "modules/libssl/libssl_integral.inc"
#endif

#ifdef _SUPPORT_OPENPGP_

#include "modules/libopenpgp/pgp_integral.inc"
#endif

/*
template class DataStream_IntegralType<uint8>;
template class DataStream_IntegralType<uint16>;
*/

