/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef ROOTSTORE_MODULE_REQUIRED

#include "modules/rootstore/rootstore_api.h"

#ifdef ROOTSTORE_SIGNKEY
#include "modules/rootstore/auto_update_versions.h"

#include AUTOUPDATE_PUBKEY_INCLUDE
#endif

RootstoreModule::RootstoreModule()
: m_root_store_api(NULL)
#ifdef ROOTSTORE_SIGNKEY
	, m_rootstore_sign_pubkey(AUTOUPDATE_CERTNAME)
	, m_rootstore_sign_pubkey_len(sizeof(AUTOUPDATE_CERTNAME))
#endif
{
}

void RootstoreModule::InitL(const OperaInitInfo& info)
{
	m_root_store_api = OP_NEW_L(RootStore_API, ());
}

void RootstoreModule::Destroy()
{
	OP_DELETE(m_root_store_api);
	m_root_store_api = NULL;
}

#endif // ROOTSTORE_MODULE_REQUIRED
