/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_CHCIPHER_H
#define LIBSSL_CHCIPHER_H

#if defined _NATIVE_SSL_SUPPORT_
class SSL_ChangeCipherSpec_st: public LoadAndWritableList
{
public:
    SSL_ChangeCipherSpec_st();
	virtual void	PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_ChangeCipherSpec_st";}
#endif
};
#endif

#endif // LIBSSL_CHCIPHER_H
