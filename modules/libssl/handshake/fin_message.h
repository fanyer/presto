/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_FINMESS_H
#define LIBSSL_FINMESS_H

#if defined _NATIVE_SSL_SUPPORT_
#include "modules/libssl/handshake/vectormess.h"

class SSL_Finished_st : public SSL_VectorMessage 
{
public:
	virtual void SetUpMessageL(SSL_ConnectionState *pending);
	virtual SSL_KEA_ACTION ProcessMessage(SSL_ConnectionState *pending);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_Finished_st";}
#endif
};

#endif
#endif // LIBSSL_FINMESS_H
