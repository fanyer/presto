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

// ================================= QuoVadis =============================================

DECLARE_DEP_CERT_LIST(QuoVadis_dep_cert)
	DECLARE_DEP_CERT_ITEM(QV_ROOT_CA2)
DECLARE_DEP_CERT_LIST_END;

DECLARE_DEPENDENCY(QuoVadis_dep,
	NULL, // Keyids
	QuoVadis_dep_cert, // Dependencies
	NULL,
	0,
	0
	);

DECLARE_DEP_EV_OID_LIST(QuoVadis_EV_OIDs)
	DECLARE_DEP_EV_OID("1.3.6.1.4.1.8024.0.2.100.1.2")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(QuoVadis_EV_dep,
	NULL, // Keyids
	NULL, // Dependencies
	QuoVadis_EV_OIDs,
	2013,
	7
	);
