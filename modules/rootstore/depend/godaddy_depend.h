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

// ================================= GODADDY =============================================

DECLARE_DEP_CERT_LIST(Godaddy_dep_cert)
	DECLARE_DEP_CERT_ITEM(STARFIELD_ISSUING_CA)
	DECLARE_DEP_CERT_ITEM_COND(GoDaddy_Class2, 2,0)
	DECLARE_DEP_CERT_ITEM_COND(StarField_Class2, 2,0)
DECLARE_DEP_CERT_LIST_END;

DECLARE_DEPENDENCY(Godaddy_dep,
	NULL, // Keyids
	Godaddy_dep_cert, // Dependencies
	NULL,
	0,
	0
	);


DECLARE_DEP_CERT_LIST(Godaddy_dep_cert1)
	DECLARE_DEP_CERT_ITEM(VALICERT_CLASS_2_CA)
DECLARE_DEP_CERT_LIST_END;

DECLARE_DEPENDENCY(Godaddy_dep1,
	NULL, // Keyids
	Godaddy_dep_cert1, // Dependencies
	NULL,
	0,
	0
	);


DECLARE_DEP_EV_OID_LIST(Godaddy_EV_OIDs)
	DECLARE_DEP_EV_OID("2.16.840.1.114413.1.7.23.3")
DECLARE_DEP_EV_OID_END;

DECLARE_DEP_EV_OID_LIST(Godaddy_Starfield_EV_OIDs)
	DECLARE_DEP_EV_OID("2.16.840.1.114414.1.7.23.3")
DECLARE_DEP_EV_OID_END;

DECLARE_DEP_EV_OID_LIST(Godaddy_Starfield_Services_EV_OIDs)
	DECLARE_DEP_EV_OID("2.16.840.1.114414.1.7.24.3")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(Godaddy_EVdep,
	NULL, // Keyids
	NULL, // Dependencies
	Godaddy_EV_OIDs,
	2012,
	11
	);

DECLARE_DEPENDENCY(Godaddy_EVdep2,
	NULL, // Keyids
	NULL, // Dependencies
	Godaddy_Starfield_EV_OIDs,
	2012,
	11
	);

	
DECLARE_DEPENDENCY(Godaddy_EVdep3,
	NULL, // Keyids
	NULL, // Dependencies
	Godaddy_Starfield_Services_EV_OIDs,
	2012,
	11
	);
