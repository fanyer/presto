/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file ExternalSSLLibrary.h
 *
 * External SSL Library object.
 *
 * Main purposes of this class:
 * 1) Initinialization/uninitialization of the External SSL Library.
 * 2) Storing External SSL Library global data.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef EXTERNAL_SSL_LIBRARY_H
#define EXTERNAL_SSL_LIBRARY_H


class ExternalSSLLibrary
{
public:
	static ExternalSSLLibrary* CreateL();
	virtual ~ExternalSSLLibrary() {}

public:
	virtual void InitL(const OperaInitInfo& info) = 0;
	virtual void Destroy() = 0;
	virtual void* GetGlobalData() = 0;
};

#endif // EXTERNAL_SSL_LIBRARY_H
