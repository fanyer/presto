/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#include "modules/libssl/sslbase.h"

#if defined(WIN32)
#  pragma warning( disable : 4660 )	// Disable duplicate template instantiation warnings in MSVC++ 6.0
#endif

template <class T>
DataStream *SSL_Stream_TypedPipe<T>::CreateRecordL()
{
	T* item= OP_NEW_L(T, ());

	if(GetForwardErrorToTarget())
		GetForwardErrorToTarget()->ForwardToThis(item);

	return item;
}

template <class T>
SSL_Stream_TypedSequence<T>::~SSL_Stream_TypedSequence()
{
}

template <class T>
DataStream *SSL_Stream_TypedSequence<T>::GetInputSource(DataStream *src)
{
    source_pipe.SetStreamSource(src, FALSE); return &source_pipe;
}

template <class T>
T &SSL_Stream_TypedSequence<T>::Item(uint32 idx)
{
	T* item = (T *) DataStream_FlexibleSequence::Item(idx);
	return(item ? *item : default_item);
}

template <class T>
const T &SSL_Stream_TypedSequence<T>::Item(uint32 idx) const
{
	const T* item = (const T*) DataStream_FlexibleSequence::Item(idx);
	return(item ? *item : default_item);
}

template <class T>
DataStream *SSL_Stream_TypedSequence<T>::CreateRecordL()
{
	T* item= OP_NEW_L(T, ());

	ForwardToThis(item);

	return item;
}

template<class T>
void SSL_Stream_TypedSequence<T>::Set(SSL_Stream_TypedSequence<T> &source)
{
	SSL_TRAP_AND_RAISE_ERROR_THIS(CopyL(source));
}

template<class T>
void SSL_Stream_TypedSequence<T>::Set(const T &source)
{
	Resize(1);
	Item(0) = source;
}

template<class T>
void SSL_Stream_TypedSequence<T>::Resize(uint32 nlength)
{
	SSL_TRAP_AND_RAISE_ERROR_THIS(ResizeL(nlength));
}

template<class T>
BOOL SSL_Stream_TypedSequence<T>::Contain(T &src)
{
	OP_MEMORY_VAR BOOL ret = FALSE;
	SSL_TRAP_AND_RAISE_ERROR_THIS(ret = ContainL(src));

	return ret;
}

template<class T>
BOOL SSL_Stream_TypedSequence<T>::Contain(T &source, uint32& index)
{
	OP_MEMORY_VAR BOOL ret = FALSE;
	SSL_TRAP_AND_RAISE_ERROR_THIS(ret = ContainL(source, index));

	return ret;
}

template<class T>
void SSL_Stream_TypedSequence<T>::Copy(SSL_Stream_TypedSequence<T> &source)
{
	SSL_TRAP_AND_RAISE_ERROR_THIS(CopyL(source));
}

template<class T>
void SSL_Stream_TypedSequence<T>::Transfer(uint32 start, SSL_Stream_TypedSequence<T> &source,uint32 start1, uint32 len)
{
	SSL_TRAP_AND_RAISE_ERROR_THIS(TransferL(start, source, start1, len));
}

template<class T>
void SSL_Stream_TypedSequence<T>::CopyCommon(SSL_Stream_TypedSequence<T> &ordered_list, SSL_Stream_TypedSequence<T> &master_list, BOOL reverse)
{
	SSL_TRAP_AND_RAISE_ERROR_THIS(CopyCommonL(ordered_list, master_list, reverse));
}

#include "modules/libssl/handshake/hand_types.h"
#include "modules/libssl/handshake/cipherid.h"
#include "modules/libssl/handshake/tlssighash.h"

template class SSL_Stream_TypedSequence<SSL_CipherID>;
template class SSL_Stream_TypedSequence<Loadable_SSL_ClientCertificateType>;
template class SSL_Stream_TypedSequence<SSL_varvector8>;
template class SSL_Stream_TypedSequence<SSL_varvector24>;
template class SSL_Stream_TypedSequence<SSL_varvector16>;
template class SSL_Stream_TypedSequence<Loadable_SSL_CompressionMethod>;
#ifdef _SUPPORT_TLS_1_2
template class SSL_Stream_TypedSequence<TLS_SigAndHash>;
#endif // _SUPPORT_TLS_1_2

#include "modules/libssl/handshake/tls_ext.h"
template class SSL_Stream_TypedSequence<TLS_ExtensionItem>;
template class SSL_Stream_TypedSequence<TLS_ServerNameItem>;


#ifdef ADS12
#define ADS12_TEMPLATING
#include "modules/libssl/handshake/tlssighash.cpp"
#include "modules/libssl/handshake/certreq.cpp"
#include "modules/libssl/base/loadwritelist_t.cpp"
#include "modules/libssl/external/openssl/certhand1.cpp"
#include "modules/libssl/options/ciphers.cpp"
#include "modules/libssl/options/sslopt_init.cpp"
#include "modules/libssl/keyex/sslkeyex.cpp"
#undef ADS12_TEMPLATING
#endif // ADS12

#endif
