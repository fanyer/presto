/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_RANDFIELD_H
#define LIBSSL_RANDFIELD_H

#if defined _NATIVE_SSL_SUPPORT_

class SSL_Random : public SSL_secure_varvector16 
{
private: 
	BOOL include_time;

public:
    SSL_Random(int size=32, BOOL use_time=TRUE);
    SSL_Random(const SSL_Random &);
	
    SSL_Random &operator =(const SSL_Random &);
    void Generate();

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_Random";}
#endif
};
#endif
#endif // LIBSSL_RANDFIELD_H
