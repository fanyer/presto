/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_VECTORMESS_H
#define LIBSSL_VECTORMESS_H

#if defined _NATIVE_SSL_SUPPORT_

class SSL_VectorMessage: public SSL_Handshake_Message_Base
{
public:
    SSL_varvector24 Message;
	
public:
    SSL_VectorMessage();  
    SSL_VectorMessage(const SSL_VectorMessage &);  

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_VectorMessage";}
#endif
};

class SSL_VectorMessage16: public SSL_Handshake_Message_Base
{ 
public:
    SSL_varvector16 Message;
	
public:
    SSL_VectorMessage16();  
    SSL_VectorMessage16(const SSL_VectorMessage16 &);  

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_VectorMessage16";}
#endif
};

#endif
#endif // LIBSSL_VECTORMESS_H
