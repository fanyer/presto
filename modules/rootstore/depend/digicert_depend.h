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

// ================================= Digicert =============================================

DECLARE_DEP_CERT_LIST(Digicert_dep_cert)
	DECLARE_DEP_CERT_ITEM(ENTRUST_EV_ROOT) // Cross signed by entrust's EV root
DECLARE_DEP_CERT_LIST_END;

DECLARE_DEP_EV_OID_LIST(Digicert_EV_OIDs)
	DECLARE_DEP_EV_OID("2.16.840.1.114412.2.1")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(Digicert_EV_dep,
	NULL, // Keyids
	Digicert_dep_cert, // Dependencies
	Digicert_EV_OIDs,
	2012,
	7
	);
