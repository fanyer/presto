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

// ================================= Secom =============================================

DECLARE_DEP_CERT_LIST(Secom_dep_cert)
	DECLARE_DEP_CERT_ITEM_COND(SECOM_EV_ROOT, 2, 0)
DECLARE_DEP_CERT_LIST_END;

DECLARE_DEPENDENCY(Secom_Cert_dep,
	NULL, // Keyids
	Secom_dep_cert, // Dependencies
	NULL,
	0,
	0
	);


DECLARE_DEP_EV_OID_LIST(Secom_EV_OIDs)
	DECLARE_DEP_EV_OID("1.2.392.200091.100.721.1")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(Secom_EV_dep,
	NULL, // Keyids
	NULL, // Dependencies
	Secom_EV_OIDs,
	2012,
	12
	);
