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

#ifndef __SSLPLAINREC_H
#define __SSLPLAINREC_H

#if defined _NATIVE_SSL_SUPPORT_

#define SSL_MAX_PLAIN_LENGTH 0x4000

#include "modules/libssl/protocol/sslbaserec.h"

class SSL_PlainText : public SSL_Record_Base{
protected:
    friend class SSL_CipherText;
	
private:
    void Init();
    
protected:
    virtual SSL_Record_Base *InitEncryptTarget();
    virtual SSL_Record_Base *InitDecryptTarget(SSL_CipherSpec *cipher);
	
public:  
    SSL_PlainText();
    SSL_PlainText(const SSL_PlainText &);    
    virtual ~SSL_PlainText();
    SSL_PlainText &operator =(const SSL_PlainText &);  

	virtual void	PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);
	
	
public:

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_PlainText";}
#endif
};

#endif
#endif
