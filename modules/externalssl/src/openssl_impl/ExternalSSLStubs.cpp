/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file ExternalSSLStubs.cpp
 *
 * Implementation stubs of External SSL specific functions.
 * They are only needed for successful linking.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */
#include "core/pch.h"

#ifdef EXTERNAL_SSL_OPENSSL_IMPLEMENTATION

#include "modules/pi/network/OpCertificateManager.h"
OP_STATUS OpCertificateManager::Get(OpCertificateManager** cert_manager, OpCertificateManager::CertificateType type) { return OpStatus::ERR_NOT_SUPPORTED; }

#endif // EXTERNAL_SSL_OPENSSL_IMPLEMENTATION
