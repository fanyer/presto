/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/util/OpTypedObject.h"

/*static*/ INT32 OpTypedObject::GetUniqueID()
{
	return g_unique_id_counter ++;
}
