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

#ifdef HAS_OPERABLANK
#include "modules/about/operablank.h"
#include "modules/locale/locale-enum.h"

OP_STATUS OperaBlank::GenerateData()
{
	RETURN_IF_ERROR(SetupURL(NO_CACHE | SKIP_BOM));
	return FinishURL();
}

#endif
