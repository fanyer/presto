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

// ================================= Buypass =============================================

DECLARE_DEP_EV_OID_LIST(Buypass_EV_OIDs)
        DECLARE_DEP_EV_OID("2.16.578.1.26.1.3.3")
DECLARE_DEP_EV_OID_END;

DECLARE_DEPENDENCY(Buypass_EV_dep,
        NULL, // Keyids
        NULL, // Dependencies
        Buypass_EV_OIDs,
        2013,
        5
        );
