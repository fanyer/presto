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

// ================================= CNNIC =============================================

DECLARE_DEP_EV_OID_LIST(CNNIC_EV_OIDs)
	DECLARE_DEP_EV_OID("1.3.6.1.4.1.29836.1.10")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(CNNIC_EV_dep,
	NULL, // Keyids
	NULL, // Dependencies
	CNNIC_EV_OIDs,
	2012,
	10
	);
