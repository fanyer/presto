/* -*- Mode: c++; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_XPATHEXTENSIONS_H
#define XSLT_XPATHEXTENSIONS_H

#ifdef XSLT_SUPPORT

#include "modules/xpath/xpath.h"
#include "modules/xslt/src/xslt_functions.h"

class XSLT_Variable;
class XSLT_Element;

class XSLT_XPathExtensions
  : public XPathExtensions
{
public:
  XSLT_XPathExtensions (XSLT_Element *element, BOOL no_variables = FALSE)
    : element (element),
      previous_variable (NULL),
      no_variables (no_variables)
  {
  }

  virtual OP_STATUS GetVariable (XPathVariable *&variable, const XMLExpandedName &name);
  virtual OP_STATUS GetFunction (XPathFunction *&function, const XMLExpandedName &name);

  void SetPreviousVariable (XSLT_Variable* variable) { previous_variable = variable; }

  XMLNamespaceDeclaration *GetNamespaceDeclaration ();
  XSLT_StylesheetImpl *GetStylesheet ();
  XSLT_Variable *GetPreviousVariable () { return previous_variable; }

private:
  XSLT_Element *element;
  XSLT_Variable *previous_variable; // The chain of XSLT_Variable elements that produce the current scope
  BOOL no_variables;

  void CalculatePreviousVariable();
};

#endif // XSLT_SUPPORT
#endif // XSLT_XPATHEXTENSIONS_H
