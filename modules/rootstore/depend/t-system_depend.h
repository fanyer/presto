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

// ================================= T-System =============================================

DECLARE_DEP_CERT_LIST(T_System_old_dep)
	DECLARE_DEP_CERT_ITEM(T_SYSTEM_GLOBAL_ROOT_C2)
	DECLARE_DEP_CERT_ITEM(T_SYSTEM_GLOBAL_ROOT_C3)
DECLARE_DEP_CERT_LIST_END;


DECLARE_DEP_EV_OID_LIST(T_System_EV_OIDs)
	DECLARE_DEP_EV_OID("1.3.6.1.4.1.7879.13.24.1")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(T_System_EV_dep,
	NULL, // Keyids
	T_System_old_dep, // Dependencies
	T_System_EV_OIDs,
	2012,
	10
	);

DECLARE_DEPENDENCY(T_System_EV_dep2,
	NULL, // Keyids
	NULL, // Dependencies
	T_System_EV_OIDs,
	2012,
	10
	);
