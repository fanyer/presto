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
#if !defined(ADS12) || defined(ADS12_TEMPLATING) // see end of streamtype.cpp
#include "modules/libssl/sslbase.h"

#if defined(WIN32)
#  pragma warning( disable : 4660 )	// Disable duplicate template instantiation warnings in MSVC++ 6.0
#endif

#if 0
template<class T, int len_size, int max_len>
SSL_LoadAndWriteableListType<T, len_size, max_len>::SSL_LoadAndWriteableListType(SSL_LoadAndWriteableListType &other)
: SSL_LoadAndWritable_list(full_payload, len_size, max_len)
{
	AddItem(full_payload);
	full_payload = other.full_payload;
}
#endif

template<class T, int len_size, int max_len>
SSL_LoadAndWriteableListType<T, len_size, max_len>::~SSL_LoadAndWriteableListType()
{
}

#include "modules/libssl/handshake/hand_types.h"
#include "modules/libssl/handshake/cipherid.h"
#include "modules/libssl/handshake/tlssighash.h"

template class SSL_LoadAndWriteableListType<SSL_CipherID, 2, 0xffff>;
template class SSL_LoadAndWriteableListType<SSL_varvector16, 2, 0xffff>;
template class SSL_LoadAndWriteableListType<SSL_varvector24, 3, 0xffffff>;
template class SSL_LoadAndWriteableListType<Loadable_SSL_ClientCertificateType, 1, 0xff> ;
template class SSL_LoadAndWriteableListType<Loadable_SSL_CompressionMethod, 1, 0xff> ;
template class SSL_LoadAndWriteableListType<SSL_varvector8, 1, 0xff>;
#ifdef _SUPPORT_TLS_1_2
template class SSL_LoadAndWriteableListType<TLS_SigAndHash, 2, 0xffff>;
//template class  SSL_LoadAndWriteableListType<Loadable_SSL_Hashtype, 1,0xff>;
#endif

#include "modules/libssl/handshake/tls_ext.h"

template class SSL_LoadAndWriteableListType<TLS_ExtensionItem, 2, 0xffff>;
template class SSL_LoadAndWriteableListType<TLS_ServerNameItem, 2, 0xffff>;

#endif // !ADS12 or ADS12_TEMPLATING

#endif // _NATIVE_SSL_SUPPORT_
