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

// ================================= VERISIGN =============================================

DECLARE_DEP_CERT_LIST(Versign_g1_c3_old_dep)
	DECLARE_DEP_CERT_ITEM(VERISIGN_G5_CLASS3)
	DECLARE_DEP_CERT_ITEM(VERISIGN_SVR_INTL_2)
DECLARE_DEP_CERT_LIST_END;

DECLARE_DEPENDENCY_RANGE(VERISIGN_DEP_g1_c3_old,
	NULL, // Keyids
	Versign_g1_c3_old_dep, // Dependencies
	NULL,
	0,
	0,
	2,
	2
	);

DECLARE_DEPENDENCY_RANGE(VERISIGN_DEP_g1_c3_intermediates,
	NULL, // Keyids
	NULL, // Dependencies
	NULL,
	0,
	0,
	3,
	0
	);

DECLARE_DEP_CERT_LIST(Versign_g1_c3_dep)
	DECLARE_DEP_CERT_ITEM(VERISIGN_G5_CLASS3)
	DECLARE_DEP_CERT_ITEM(VERISIGN_INTERMEDIATE_2009_CSIG)
	DECLARE_DEP_CERT_ITEM(VERISIGN_INTERMEDIATE_2009_EV1024)
	DECLARE_DEP_CERT_ITEM(VERISIGN_INTERMEDIATE_2009_OFX)
	DECLARE_DEP_CERT_ITEM(VERISIGN_INTERMEDIATE_2009_SS1024)
	DECLARE_DEP_CERT_ITEM(VERISIGN_INTERMEDIATE_2009_SSCA)
	DECLARE_DEP_CERT_ITEM(VERISIGN_SVR_INTL_2)
DECLARE_DEP_CERT_LIST_END;

DECLARE_DEPENDENCY_RANGE(VERISIGN_DEP_g1_c3,
	NULL, // Keyids
	Versign_g1_c3_dep, // Dependencies
	NULL,
	0,
	0,
	3,
	0
	);

DECLARE_DEP_EV_OID_LIST(Verisign_EV_OIDs)
	DECLARE_DEP_EV_OID("2.16.840.1.113733.1.7.23.6")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(VERISIGN_DEP_g5_c3,
	NULL, // Keyids
	NULL, // Dependencies
	Verisign_EV_OIDs,
	2012	,
	7
	);
