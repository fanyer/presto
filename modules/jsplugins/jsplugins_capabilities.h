/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef JSPLUGINS_CAPABILITIES_H
#define JSPLUGINS_CAPABILITIES_H

/* Supports ES_CAP_PROPERTY_TRANSLATION and the new EcmaScript_Object::{Get,Put}{Index,Name}() signatures. */
#define JSPLUGINS_CAP_NEW_GETNAME

/* 2008-07-03 JS_Plugin_Context has BeforeUnload function. */
#define JSPLUGINS_CAP_HAS_BEFOREUNLOAD

/* 2008-09-18 JS_Plugin_Context has PutName function. */
#define JSPLUGINS_CAP_HAS_PUTNAME

#endif // JSPLUGINS_CAPABILITIES_H
