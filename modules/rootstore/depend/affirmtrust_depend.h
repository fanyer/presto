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

// ================================= Affirmtrust =============================================

DECLARE_DEP_EV_OID_LIST(AffirmTrust_Com_EV_OIDs)
	DECLARE_DEP_EV_OID("1.3.6.1.4.1.34697.2.1")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(AffirmTrust_Com_EV_dep,
	NULL, // Keyids
	NULL, // Dependencies
	AffirmTrust_Com_EV_OIDs,
	2010,
	7
	);

DECLARE_DEP_EV_OID_LIST(AffirmTrust_Net_EV_OIDs)
	DECLARE_DEP_EV_OID("1.3.6.1.4.1.34697.2.2")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(AffirmTrust_Net_EV_dep,
	NULL, // Keyids
	NULL, // Dependencies
	AffirmTrust_Net_EV_OIDs,
	2010,
	7
	);

DECLARE_DEP_EV_OID_LIST(AffirmTrust_Premium_EV_OIDs)
	DECLARE_DEP_EV_OID("1.3.6.1.4.1.34697.2.3")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(AffirmTrust_Premium_EV_dep,
	NULL, // Keyids
	NULL, // Dependencies
	AffirmTrust_Premium_EV_OIDs,
	2010,
	7
	);
