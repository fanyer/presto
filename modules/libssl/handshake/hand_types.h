/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_HANDTYPES_H
#define LIBSSL_HANDTYPES_H

#if defined _NATIVE_SSL_SUPPORT_

typedef DataStream_IntegralType<SSL_CompressionMethod,1> Loadable_SSL_CompressionMethod;
typedef SSL_LoadAndWriteableListType<Loadable_SSL_CompressionMethod, 1, 0xff> SSL_varvectorCompress;
typedef SSL_varvector16_list SSL_DistinguishedName_list;
typedef DataStream_IntegralType<SSL_ClientCertificateType, 1> Loadable_SSL_ClientCertificateType;
typedef SSL_LoadAndWriteableListType<Loadable_SSL_ClientCertificateType, 1, 0xff> SSL_ClientCertificateType_list;
typedef SSL_LoadAndWriteableListType<SSL_varvector8, 1, 0xff> SSL_NextProtocols;

#endif
#endif // LIBSSL_HANDTYPES_H
