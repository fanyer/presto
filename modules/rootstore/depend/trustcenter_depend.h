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

// ================================= TrustCenter =============================================

DECLARE_DEP_CERT_LIST(TrustCenter_c1_uni_dep)
	DECLARE_DEP_CERT_ITEM(TRUSTCENTER_UNIVERSIAL_1)
DECLARE_DEP_CERT_LIST_END;


/*
DECLARE_DEPENDENCY(TrustCenter_old_dep,
	NULL, // Keyids
	TrustCenter_c1_uni_dep, // Dependencies
	NULL,
	0,
	0
	);



DECLARE_DEP_CERT_LIST(TrustCenter_c2_uni_dep)
	DECLARE_DEP_CERT_ITEM(TRUSTCENTER_2048)
DECLARE_DEP_CERT_LIST_END;


DECLARE_DEPENDENCY(TrustCenter_c2_old_dep,
	NULL, // Keyids
	TrustCenter_c2_uni_dep, // Dependencies
	NULL,
	0,
	0
	);
*/

DECLARE_DEP_EV_OID_LIST(TrustCenter_EV_OIDs)
	DECLARE_DEP_EV_OID("1.2.276.0.44.1.1.1.4")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(TrustCenter_EV_dep,
	NULL, // Keyids
	NULL, // Dependencies
	TrustCenter_EV_OIDs,
	2012,
	12
	);
