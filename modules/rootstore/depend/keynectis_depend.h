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

// ================================= Keynectis =============================================

DECLARE_DEP_EV_OID_LIST(Keynectis_EV_OIDs)
	DECLARE_DEP_EV_OID("1.3.6.1.4.1.22234.2.5.2.3.1")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(Keynectis_EV_dep,
	NULL, // Keyids
	NULL, // Dependencies
	Keynectis_EV_OIDs,
	2013,
	1
	);
