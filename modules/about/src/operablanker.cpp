/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#if !defined NO_URL_OPERA && defined SELFTEST
#include "modules/about/operablanker.h"

OP_STATUS OperaBlanker::GenerateData()
{
	RETURN_IF_ERROR(SetupURL(NO_CACHE | SKIP_BOM));
	return FinishURL();
}

#endif
