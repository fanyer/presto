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

// ================================= COMODO =============================================

DECLARE_DEP_CERT_LIST(Comodo_dep_cert)
	DECLARE_DEP_CERT_ITEM(COMODO_EV_ROOT)
DECLARE_DEP_CERT_LIST_END;

DECLARE_DEPENDENCY(Comodo_dep,
	NULL, // Keyids
	Comodo_dep_cert, // Dependencies
	NULL,
	0,
	0
	);

DECLARE_DEP_EV_OID_LIST(Comodo_EV_OIDs)
	DECLARE_DEP_EV_OID("1.3.6.1.4.1.6449.1.2.1.5.1")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(Comodo_EV_dep,
	NULL, // Keyids
	NULL, // Dependencies
	Comodo_EV_OIDs,
	2012,
	12
	);

DECLARE_DEPENDENCY(Comodo_EV_dep2,
	NULL, // Keyids
	Comodo_dep_cert, // Dependencies
	Comodo_EV_OIDs,
	2012,
	12
	);
