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

#ifndef __SSLALERT_BASE_H
#define __SSLALERT_BASE_H

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/datastream/fl_tint.h"
#include "modules/locale/locale-enum.h"

class SSL_Alert_Base{
protected:
    DataStream_IntegralType<SSL_AlertLevel,1> level;
    DataStream_IntegralType<SSL_AlertDescription,1> description;
	OpString	reason;
	
public:
    SSL_Alert_Base(){description = SSL_No_Description;	level = SSL_NoError;}
    SSL_Alert_Base(SSL_AlertLevel lev,SSL_AlertDescription des){Set(lev,des);}
    SSL_Alert_Base(const SSL_Alert_Base &old){description = old.description; level = old.level;};
    
    void Get(SSL_AlertLevel &lev,SSL_AlertDescription &des)const {lev=static_cast<SSL_AlertLevel>(level);des=static_cast<SSL_AlertDescription>(description);};
    SSL_AlertLevel GetLevel() const{return level;};
    SSL_AlertDescription GetDescription() const{return description;};
    void Set(SSL_AlertLevel,SSL_AlertDescription);
	void Set(OP_STATUS op_err);
    void SetLevel(SSL_AlertLevel);
    void SetDescription(SSL_AlertDescription);
    
	OP_STATUS SetReason(const OpStringC &text){return reason.Set(text);}
	OP_STATUS SetReason(Str::LocaleString str){return SetLangString(str, reason);}
	const OpStringC &GetReason() const{return reason;};
    
    SSL_Alert_Base &operator =(const SSL_Alert_Base &other);
};
#endif
#endif
