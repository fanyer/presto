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


// ================================= GLOBALSIGN =============================================

DECLARE_DEP_CERT_LIST(Globalsign_dep_cert)
	DECLARE_DEP_CERT_ITEM(GLOBALSIGN_ROOT2)
DECLARE_DEP_CERT_LIST_END;

DECLARE_DEPENDENCY(Globalsign_dep,
	NULL, // Keyids
	Globalsign_dep_cert, // Dependencies
	NULL,
	0,
	0
	);

DECLARE_DEP_EV_OID_LIST(Globalsign_EV_OIDs)
	DECLARE_DEP_EV_OID("1.3.6.1.4.1.4146.1.1")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(Globalsign_EV_dep,
	NULL, // Keyids
	NULL, // Dependencies
	Globalsign_EV_OIDs,
	2012,
	9
	);

