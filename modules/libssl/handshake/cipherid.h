/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_CIPHERID_H
#define LIBSSL_CIPHERID_H

#if defined _NATIVE_SSL_SUPPORT_

class SSL_CipherID : public DataStream_SourceBuffer
{ 
private:
    uint8 id[2];
    
public:
    SSL_CipherID();
    SSL_CipherID(const SSL_CipherID &);
    SSL_CipherID(uint8 a,uint8 b);
    SSL_CipherID &operator =(const SSL_CipherID &);
    
    void Set(uint8 a,uint8 b){id[0] = a; id[1]=b;};
    void Get(uint8 &a,uint8 &b)  const{ a = id[0], b = id[1];};

    int operator ==(const SSL_CipherID &) const;
    int operator !=(const SSL_CipherID &other) const{return !operator ==(other);};

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_CipherID";}
#endif
};

typedef SSL_LoadAndWriteableListType<SSL_CipherID, 2, 0xffff> SSL_CipherSuites;

#endif
#endif // LIBSSL_CIPHERID_H
