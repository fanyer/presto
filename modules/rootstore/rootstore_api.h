/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef ROOTSTORE_API_H
#define ROOTSTORE_API_H

#if defined(_SSL_USE_OPENSSL_) && !defined(_EXTERNAL_SSL_SUPPORT_) && !defined(_CERTICOM_SSL_SUPPORT_)

class RootStore_API
{
public:
	RootStore_API(){};
	~RootStore_API(){};

	OP_STATUS InitAuthorityCerts(SSL_Options *optionsManager,uint32 ver);
};

/***********************************************************************************
**
**	Defines for OP_NEW / OP_DELETE
**
***********************************************************************************/

#ifndef OP_NEW
#define OP_NEW(obj, args) new obj args
#endif

#ifndef OP_NEWA
#define OP_NEWA(obj, count) new obj[count]
#endif

#ifndef OP_NEW_L
#define OP_NEW_L(obj, args) new (ELeave) obj args
#endif

#ifndef OP_NEWA_L
#define OP_NEWA_L(obj, count) new (ELeave) obj[count]
#endif

#ifndef OP_DELETE
#define OP_DELETE(ptr) delete ptr
#endif

#ifndef OP_DELETEA
#define OP_DELETEA(ptr) delete[] ptr
#endif

#endif
#endif // ROOTSTORE_API_H
