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

// ================================= ENTRUST =============================================


/*
DECLARE_DEP_CERT_LIST(Entrust_dep_cert)
	DECLARE_DEP_CERT_ITEM(ENTRUST_EV_ROOT)
DECLARE_DEP_CERT_LIST_END;

DECLARE_DEPENDENCY(Entrust_dep,
	NULL, // Keyids
	Entrust_dep_cert, // Dependencies
	NULL,
	0,
	0
	);
*/

DECLARE_DEP_CERT_LIST(Entrust_dep_cert2)
	DECLARE_DEP_CERT_ITEM_COND(TRUSTWAVE_STCA_CERT_2, 2, 0)
DECLARE_DEP_CERT_LIST_END;

DECLARE_DEPENDENCY(Entrust_dep2,
	NULL, // Keyids
	Entrust_dep_cert2, // Dependencies
	NULL,
	0,
	0
	);

DECLARE_DEP_EV_OID_LIST(Entrust_EV_OIDs)
	DECLARE_DEP_EV_OID("2.16.840.1.114028.10.1.2")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(Entrust_EV_dep,
	NULL, // Keyids
	NULL, // Dependencies
	Entrust_EV_OIDs,
	2012,
	7
	);
