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

#ifndef __SSLALERT_H
#define __SSLALERT_H

#ifdef _NATIVE_SSL_SUPPORT_
class SSL_Alert : public SSL_Alert_Base, public LoadAndWritableList
{
public:
	
    SSL_Alert();   
    SSL_Alert(const SSL_Alert &);   
    SSL_Alert(SSL_AlertLevel,SSL_AlertDescription);   

    virtual BOOL Valid(SSL_Alert *msg=NULL) const;
	
    SSL_Alert &operator =(const SSL_Alert &other){SSL_Alert_Base::operator =(other); return *this;};

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_Alert";}
#endif

	virtual OP_STATUS SetReason(const OpStringC &text){return SSL_Alert_Base::SetReason(text);}
	virtual OP_STATUS SetReason(Str::LocaleString str){return SSL_Alert_Base::SetReason(str);}
};
#endif
#endif
