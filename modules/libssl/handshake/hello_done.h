/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_HELLODONE_H
#define LIBSSL_HELLODONE_H

#if defined _NATIVE_SSL_SUPPORT_

class SSL_Hello_Done_st : public SSL_Handshake_Message_Base
{
public:
	virtual SSL_KEA_ACTION ProcessMessage(SSL_ConnectionState *pending);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_Hello_Done_st";}
#endif
};
#endif
#endif // LIBSSL_HELLODONE_H
