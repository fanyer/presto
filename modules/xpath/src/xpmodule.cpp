/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xmlutils/xmlnames.h"

/* virtual */ void
XpathModule::InitL (const OperaInitInfo &info)
{
  xmlexpandedname_hash_functions = OP_NEW_L (XMLExpandedName::HashFunctions, ());
}

/* virtual */ void
XpathModule::Destroy ()
{
  OP_DELETE (xmlexpandedname_hash_functions);
}

#endif // XPATH_SUPPORT
