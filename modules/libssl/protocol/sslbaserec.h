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

#ifndef __SSLBASEREC_H
#define __SSLBASEREC_H

#if defined _NATIVE_SSL_SUPPORT_

//class SSL_Compressed;
class SSL_CipherText;
class SSL_CipherSpec;

#include "modules/libssl/base/sslprotver.h"

class SSL_Record_Base : public SSL_secure_varvector16
{
private:
	BOOL handled;
	
	void InternalInit();
	
protected:
    SSL_ProtocolVersion version;
    
	BOOL IV_field;
    
protected:
    SSL_Record_Base();
    SSL_Record_Base(const SSL_Record_Base &old);
 	
    virtual SSL_Record_Base *InitEncryptTarget() =0;
    virtual SSL_Record_Base *InitDecryptTarget(SSL_CipherSpec *cipher) =0;
	
public:
	
    virtual ~SSL_Record_Base();
    void SetType(SSL_ContentType);
    SSL_ContentType GetType() const;
    virtual void SetVersion(const SSL_ProtocolVersion &);
    const SSL_ProtocolVersion &GetVersion() const;
	
	BOOL	GetHandled(){return handled;};
	void	SetHandled(BOOL hndl){handled = hndl;};

	SSL_Record_Base *Encrypt(SSL_CipherSpec *cipher);
	SSL_Record_Base *Decrypt(SSL_CipherSpec *cipher);
	

    SSL_Record_Base &operator =(const SSL_Record_Base &old);

	void SetUseIV_Field(){IV_field = TRUE;}
    
    SSL_Record_Base *Suc() const {
		return (SSL_Record_Base *)Link::Suc();
    };       //pointer
    SSL_Record_Base *Pred() const {
		return (SSL_Record_Base *)Link::Pred();
    };       //pointer

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_Record_Base";}
#endif
};

#endif
#endif
