/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPUNKNOWN_H
#define XPUNKNOWN_H

#ifdef XPATH_EXTENSION_SUPPORT

#include "modules/xpath/src/xpexpr.h"
#include "modules/xpath/src/xpproducer.h"

class XPath_VariableReader;

/** Expression with unknown result type.  Sometimes needs to be special
    treated as such.  (But in most cases you can just convert it to a producer
    or wrap it in a conversion expression.) */
class XPath_Unknown
  : public XPath_Expression,
    public XPath_Producer
{
protected:
  XPath_Unknown (XPath_Parser *parser)
    : XPath_Expression (parser)
  {
  }

public:
  static XPath_Unknown *MakeL (XPath_Parser *parser, XPath_VariableReader *reader);
  static XPath_Unknown *MakeL (XPath_Parser *parser, XPathFunction *function, XPath_Expression **arguments, unsigned arguments_count);

  virtual unsigned GetResultType ();
  virtual unsigned GetExpressionFlags ();
  virtual unsigned GetProducerFlags ();

  enum UnknownType
    {
      UNKNOWN_VARIABLE_REFERENCE,
      UNKNOWN_FUNCTION_CALL
    };

  virtual UnknownType GetUnknownType () = 0;
  virtual unsigned GetExternalType () = 0;

  virtual XPath_ValueType GetActualResultTypeL (XPath_Context *context, BOOL initial) = 0;
  virtual XPath_Value *EvaluateToValueL (XPath_Context *context, BOOL initial) = 0;

  virtual void SetProducerFlags (unsigned producer_flags) = 0;
  /**< Used by XPath_Producer::EnsureFlagsL.  Normally it would just wrap a
       producer to ensure the flags, but since we don't know the properties of
       ourselves as a producer, we'd need to be very conservative, inducing
       extra costs. */
};

class XPathVariable;

class XPath_VariableReader
{
private:
  XMLExpandedName name;
  XPathVariable *variable;
  unsigned state_index, variablestate_index, value_index;
  XPath_VariableReader *next;

  XPath_VariableReader (XPath_Parser *parser, XPathVariable *variable);

public:
  static XPath_VariableReader *MakeL (XPath_Parser *parser, const XMLExpandedName &name, XPathVariable *variable);

  ~XPath_VariableReader ();

  const XMLExpandedName &GetName () { return name; }
  XPathVariable *GetVariable () { return variable; }
  XPath_VariableReader *GetNext () { return next; }
  void SetNext (XPath_VariableReader *next0) { next = next0; }

  void Start (XPath_Context *context);
  void Finish (XPath_Context *context);
  void GetValueL (XPath_Value *&value, unsigned *&state, XPath_Context *context, XPath_Expression *caller, BOOL full);
};

#endif // XPATH_EXTENSION_SUPPORT
#endif // XPUNKNOWN_H
