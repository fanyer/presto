/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPLOCATIONPATHEXPR_H
#define XPLOCATIONPATHEXPR_H

#include "modules/xpath/src/xpexpr.h"

class XPath_LocationPath;

class XPath_LocationPathExpression
  : public XPath_Expression
{
protected:
  XPath_Producer *producer;

public:
  XPath_LocationPathExpression (XPath_Parser *parser, XPath_Producer *producer);
  virtual ~XPath_LocationPathExpression ();

  virtual unsigned GetResultType ();
  virtual unsigned GetExpressionFlags ();

  virtual XPath_Producer *GetProducerInternalL (XPath_Parser *parser);
};

#endif // XPLOCATIONPATHEXPR_H
