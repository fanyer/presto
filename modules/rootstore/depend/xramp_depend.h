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

// ================================= XRamp/Trustwave =============================================

DECLARE_DEP_EV_OID_LIST(XRamp_EV_OIDs)
	DECLARE_DEP_EV_OID("2.16.840.1.114404.1.1.2.4.1")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(XRamp_EV_dep,
	NULL, // Keyids
	NULL, // Dependencies
	XRamp_EV_OIDs,
	2013,
	2
	);
