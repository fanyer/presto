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

#include "modules/libssl/sslbase.h"
#include "modules/util/opstrlst.h"
#include "modules/libssl/certs/certhandler.h"

OP_STATUS ParseCommonName(const OpString_list &info, OpString &title);

OP_STATUS SSL_CertificateHandler::GetCommonName(uint24 item, OpString &target)
{
	OpString_list info;

	RETURN_IF_ERROR(GetSubjectName(item, info));
	return ParseCommonName(info, target);
}

/*
SSL_CertificateHandler::~SSL_CertificateHandler()
{
}
*/

#endif // _NATIVE_SSL_SUPPORT_
