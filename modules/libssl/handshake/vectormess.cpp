/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/pi/OpSystemInfo.h"

#include "modules/libssl/sslbase.h"
#include "modules/libssl/sslrand.h"
#include "modules/libssl/handshake/vectormess.h"

#include "modules/util/cleanse.h"
  
SSL_VectorMessage::SSL_VectorMessage()
{
	AddItem(Message);
	Message.FixedLoadLength(TRUE);
}

/* Unref YNP
SSL_VectorMessage::SSL_VectorMessage(const SSL_VectorMessage &old)
{
Message.ForwardTo(this);
Message = old.Message;
Message.FixedLoadLength(TRUE);
}
*/

SSL_VectorMessage16::SSL_VectorMessage16()
{
	AddItem(Message);
	Message.FixedLoadLength(TRUE);
}

#endif // relevant support
