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

#ifndef __SSLPHASH_H
#define __SSLPHASH_H

#ifdef _NATIVE_SSL_SUPPORT_

class SSL_Hash_Pointer : public SSL_Error_Status{
private:
    SSL_Hash *hash;
    SSL_Hash *point;
	
private:
    void RemovePointer();
    BOOL CreatePointer(const SSL_Hash *, BOOL fork, BOOL hmac=FALSE);
	void Init();
	
public:
    SSL_Hash_Pointer();
    SSL_Hash_Pointer(SSL_HashAlgorithmType alg);
    SSL_Hash_Pointer(const SSL_Hash_Pointer &);
    SSL_Hash_Pointer(const SSL_Hash *);
    virtual ~SSL_Hash_Pointer();
    
	void Set(SSL_HashAlgorithmType alg);
    BOOL SetFork(const SSL_Hash *);
    BOOL SetProduce(const SSL_Hash *);
    BOOL SetProduceHMAC(const SSL_Hash *);
	//    void SetExternal(SSL_Hash *);
    
    SSL_Hash * Ptr() const{return point;};
#ifdef WIN32
    SSL_Hash * const operator ->() const{return point;};
    SSL_Hash * const operator ->(){return point;};
    operator SSL_Hash * const()const {return hash;};
    operator SSL_Hash * const(){return hash;};
#else
    const SSL_Hash *operator ->() const{return point;};
    SSL_Hash *operator ->(){return point;};
    operator const SSL_Hash *()const {return hash;};
    operator SSL_Hash *(){return hash;};
#endif
	
    SSL_Hash_Pointer &operator =(const SSL_Hash_Pointer &);
    SSL_Hash_Pointer &operator =(const SSL_Hash *);
    SSL_Hash_Pointer &operator =(SSL_HashAlgorithmType alg){Set(alg); return *this;};
	
    virtual BOOL Valid(SSL_Alert *msg=NULL) const;
};

#endif
#endif
