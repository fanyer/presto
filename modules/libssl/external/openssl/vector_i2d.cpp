/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#if defined(_NATIVE_SSL_SUPPORT_)
#include "modules/libssl/sslbase.h"

BOOL i2d_Vector(int (*i2d)(void *, unsigned char **), void *source, SSL_varvector32 &target)
{
	target.Resize(0);

	if(source == NULL || i2d == NULL)
	{
		return TRUE;
	}

	int len = i2d(source, NULL);
	if(len == 0)
		return FALSE;

	target.Resize(len);
	if(target.ErrorRaisedFlag)
		return FALSE;

	unsigned char *data = target.GetDirect();
	if(i2d(source, &data) == len)
		return TRUE;

	target.Resize(0);
	return FALSE;
}

void *d2i_Vector(void *(*d2i)(void *, unsigned char **, long), void *target, SSL_varvector32 &source)
{
	if(source.GetLength() == 0 || d2i == NULL)
	{
		return NULL;
	}

	unsigned char *data = source.GetDirect();

	return d2i(target, &data, source.GetLength());
}

#endif
