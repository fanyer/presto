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

// ================================= GEOTRUST =============================================

DECLARE_DEP_CERT_LIST(Geotrust_old_dep)
	DECLARE_DEP_CERT_ITEM(GEOTRUST_PRIMARY_ROOT)
DECLARE_DEP_CERT_LIST_END;

DECLARE_DEPENDENCY(GEOTRUST_DEP_old,
	NULL, // Keyids
	Geotrust_old_dep, // Dependencies
	NULL,
	0,
	0
	);

DECLARE_DEP_EV_OID_LIST(Geotrust_EV_OIDs)
	DECLARE_DEP_EV_OID("1.3.6.1.4.1.14370.1.6")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(GEOTRUST_DEP_primary,
	NULL, // Keyids
	NULL, // Dependencies
	Geotrust_EV_OIDs,
	2012,
	7
	);
