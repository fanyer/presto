/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1995-2004
 */

#ifndef ECMASCRIPT_PRIVATE_H
#define ECMASCRIPT_PRIVATE_H

#define ES_LAST_PRIVATESLOT				65535					// Do not change this

#define ES_PRIVATE_runtime				ES_LAST_PRIVATESLOT+1	// reference from a host-object to its runtime reaper
#define ES_PRIVATE_keepalive			ES_LAST_PRIVATESLOT+2	// keepalive counter starts with this one, it must be last

#endif // ECMASCRIPT_PRIVATE_H
