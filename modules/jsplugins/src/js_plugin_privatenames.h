/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef JSPLUGIN_PRIVATENAMES_H
#define JSPLUGIN_PRIVATENAMES_H

/* The enum below is used for the ES_Runtime::GetPrivate and ES_Runtime::PutPrivate
   interface -- storing pointers in the ES_Object so that they are visible to the
   garbage collector, but unavailable to scripts.

   There is no significance to the names here -- this is just a pool of identifiers,
   all different.

   There is no significance to the numbers here, either, so the names should be kept
   alphabetical for easy modification and lookup by humans.

   And enumerators cost nothing, so keep this file clean of #ifdef-s, please. */

enum JS_Plugin_PrivateName
{
	JS_PLUGIN_PRIVATE_JSPLUGIN = ES_PRIVATENAME_JSPLUGIN,
	JS_PLUGIN_PRIVATE_nativeObject,

	JS_PLUGIN_PRIVATE_LASTNAME
};

#endif // JSPLUGIN_PRIVATENAMES_H
