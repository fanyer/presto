/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPPREDICATEEXPR_H
#define XPPREDICATEEXPR_H

#include "modules/xpath/src/xpexpr.h"

class XPath_PredicateExpression
{
public:
  static XPath_Expression *MakeL (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs);
};

#endif // XPPREDICATEEXPR_H
