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

#ifndef __SSLCIPHERREC_H
#define __SSLCIPHERREC_H

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/libssl/protocol/sslplainrec.h"

/* Decrompressing Deactivated */
/*
#define SSL_MAX_COMPRESSED_LENGTH 0x4400
class SSL_Compressed : public SSL_PlainText{
friend class SSL_CipherText;
public:  
SSL_Compressed();
SSL_Compressed(uint16 len);
SSL_Compressed(const SSL_Compressed &);
};
*/

#define SSL_MAX_ENCRYPTED_LENGTH 0x4800
class SSL_CipherText  :public SSL_PlainText{ 
public:
    SSL_CipherText();
    SSL_CipherText(const SSL_CipherText &);

    virtual void SetVersion(const SSL_ProtocolVersion &);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_CipherText";}
#endif
};
#endif
#endif
