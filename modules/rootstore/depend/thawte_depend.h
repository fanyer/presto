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

// ================================= VERISIGN/Thawte =============================================

DECLARE_DEP_CERT_LIST(Thawte_old_dep)
	DECLARE_DEP_CERT_ITEM(THAWTE_PRIMARY_ROOT)
DECLARE_DEP_CERT_LIST_END;

DECLARE_DEPENDENCY(THAWTE_DEP_old,
	NULL, // Keyids
	Thawte_old_dep, // Dependencies
	NULL,
	0,
	0
	);

DECLARE_DEP_EV_OID_LIST(Thawte_EV_OIDs)
	DECLARE_DEP_EV_OID("2.16.840.1.113733.1.7.48.1")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(THAWTE_DEP_primary,
	NULL, // Keyids
	NULL, // Dependencies
	Thawte_EV_OIDs,
	2012,
	7
	);
