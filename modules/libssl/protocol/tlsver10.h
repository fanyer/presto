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

#ifndef TLSVER10_H
#define TLSVER10_H

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/libssl/protocol/tlsver.h"

class TLS_Version_1_0 : public TLS_Version_1_Dependent
{
private:
	void Init();
    TLS_Version_1_0(const TLS_Version_1_0 *);

private:
	void Init(const TLS_Version_1_0 *);
    BOOL PRF(SSL_varvector32 &result, uint32 len, 
		const SSL_varvector32 &secret_seed,
		const char *label,  //Null teminated string
		const SSL_varvector32 &data_seed) const;

public:
    TLS_Version_1_0(uint8 minor_ver = 1);
    virtual ~TLS_Version_1_0();

#ifndef TLS_NO_CERTSTATUS_EXTENSION
	virtual void SessionUpdate(SSL_SessionUpdate state);
#endif
};
#endif
#endif /* TLSVER10_H */
