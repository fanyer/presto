/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC
#ifdef SELFTEST

#include "modules/util/str.h"
#include "modules/util/opstring.h"

#include "modules/sync/sync_factory.h"
#include "modules/sync/sync_transport.h"
#include "modules/sync/testcases/sync_ST_transport.h"
#include "modules/sync/testcases/sync_ST_factory.h"

ST_SyncFactory::ST_SyncFactory()
{
}

ST_SyncFactory::~ST_SyncFactory()
{
}

OP_STATUS ST_SyncFactory::CreateTransportProtocol(OpSyncTransportProtocol** protocol, OpInternalSyncListener* listener)
{
	OP_ASSERT(protocol);
	if (!protocol)
		return OpStatus::ERR_NULL_POINTER;

	*protocol = OP_NEW(ST_SyncTransportProtocol, (this, listener));
	return *protocol ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

#endif // SELFTEST
#endif // SUPPORT_DATA_SYNC
