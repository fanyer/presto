/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

// ================================= SwissSign =============================================

DECLARE_DEP_EV_OID_LIST(SwissSign_EV_OIDs)
	DECLARE_DEP_EV_OID("2.16.756.1.89.1.2.1.1")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(SwissSign_EV_dep,
	NULL, // Keyids
	NULL, // Dependencies
	SwissSign_EV_OIDs,
	2012,
	9
	);
