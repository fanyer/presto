/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPLITERALEXPR_H
#define XPLITERALEXPR_H

#include "modules/xpath/src/xpexpr.h"

class XPath_LiteralExpression
{
private:
  XPath_LiteralExpression ();

public:
  static XPath_Expression *MakeL (XPath_Parser *parser, double number);
  static XPath_Expression *MakeL (XPath_Parser *parser, BOOL boolean);
  static XPath_Expression *MakeL (XPath_Parser *parser, const uni_char *string, unsigned string_length);
};

#endif // XPLITERALEXPR_H
