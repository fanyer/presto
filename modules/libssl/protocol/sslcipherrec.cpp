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

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/libssl/protocol/sslcipherrec.h"

/*
* Compressing and Decompressing structures and code are commented away
*/

#ifdef _DEBUG
//#define TST_DUMP
#endif

SSL_CipherText::SSL_CipherText() : SSL_PlainText()
{
	SetLegalMax(SSL_MAX_ENCRYPTED_LENGTH);
}

void SSL_CipherText::SetVersion(const SSL_ProtocolVersion &ver)
{
	SSL_PlainText::SetVersion(ver);
	if(version.Major() > 3 || (version.Major() == 3 && version.Minor() > 1))
		SetUseIV_Field();
}

/* Crompressing Deactivated */
/*
SSL_Compressed::SSL_Compressed()
: SSL_PlainText()
{
SetLegalMax(SSL_MAX_COMPRESSED_LENGTH);
}

SSL_Compressed::SSL_Compressed(uint16 len)
: SSL_PlainText(len)
{
SetLegalMax(SSL_MAX_COMPRESSED_LENGTH);
}

SSL_Compressed::SSL_Compressed(const SSL_Compressed &old)
: SSL_PlainText(old)
{
SetLegalMax(SSL_MAX_COMPRESSED_LENGTH);
}
*/
#endif

