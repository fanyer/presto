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

// ================================= CERTUM =============================================

/*
static DEFCERT_certificate_data * const Certum_dep_main[] = {
	Certum_Level1,
	Certum_Level2,
	Certum_Level3,
	Certum_Level4,
	NULL
};

static DEFCERT_dependency Certum_dep_top = {
	NULL, // Keyids
	Certum_dep_main, // Dependencies
	NULL,
	0,
	0
};
*/

DECLARE_DEP_CERT_LIST(Certum_dep_sub)
	DECLARE_DEP_CERT_ITEM(Certum_Level5)
DECLARE_DEP_CERT_LIST_END;

DECLARE_DEPENDENCY (Certum_dep_other,	
	NULL, // Keyids
	Certum_dep_sub, // Dependencies
	NULL,
	0,
	0
	);

DECLARE_DEP_EV_OID_LIST(Certum_EV_OIDs)
        DECLARE_DEP_EV_OID("1.2.616.1.113527.2.5.1.1")
DECLARE_DEP_EV_OID_END;


DECLARE_DEPENDENCY (Certum_dep_EV,	
	NULL, // Keyids
	NULL, // Dependencies
	Certum_EV_OIDs,
	2012,
	7
	);

