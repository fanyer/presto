/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPUNIONEXPR_H
#define XPUNIONEXPR_H

#include "modules/xpath/src/xpexpr.h"

class XPath_UnionExpression
  : public XPath_Expression
{
private:
  XPath_UnionExpression (XPath_Parser *parser)
    : XPath_Expression (parser)
  {
  }

  OpVector<XPath_Producer> *producers;

public:
  static XPath_Expression *MakeL (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs);

  virtual ~XPath_UnionExpression ();

  virtual unsigned GetResultType ();
  virtual unsigned GetExpressionFlags ();

  virtual XPath_Producer *GetProducerInternalL (XPath_Parser *parser);
};

class XPath_UnionProducer
  : public XPath_Producer
{
protected:
  OpVector<XPath_Producer> *producers;
  unsigned producer_index_index, localci_index;

public:
  XPath_UnionProducer (XPath_Parser *parser, OpVector<XPath_Producer> *producers);

  virtual ~XPath_UnionProducer ();

  virtual unsigned GetProducerFlags ();

  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
  virtual XPath_Node *GetNextNodeL (XPath_Context *context);
};

#endif // XPUNIONEXPR_H
