/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_xpathextensions.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xslt/src/xslt_variable.h"

/* virtual */ OP_STATUS
XSLT_XPathExtensions::GetFunction (XPathFunction *&function, const XMLExpandedName &name)
{
  if (name == XMLExpandedName ((const uni_char *) 0, UNI_L ("system-property")))
    function = OP_NEW (XSLT_Functions::SystemProperty, ());
  else if (name == XMLExpandedName ((const uni_char *) 0, UNI_L ("key")))
    function = OP_NEW (XSLT_Functions::Key, (GetStylesheet ()));
  else if (name == XMLExpandedName ((const uni_char *) 0, UNI_L ("element-available")))
    function = OP_NEW (XSLT_Functions::ElementOrFunctionAvailable, (TRUE));
  else if (name == XMLExpandedName ((const uni_char *) 0, UNI_L ("function-available")))
    function = OP_NEW (XSLT_Functions::ElementOrFunctionAvailable, (FALSE));
  else if (name == XMLExpandedName ((const uni_char *) 0, UNI_L ("format-number")))
    function = OP_NEW (XSLT_Functions::FormatNumber, (GetStylesheet ()));
  else if (name == XMLExpandedName ((const uni_char *) 0, UNI_L ("current")))
    function = OP_NEW (XSLT_Functions::Current, ());
  else if (name == XMLExpandedName ((const uni_char *) 0, UNI_L ("generate-id")))
    function = OP_NEW (XSLT_Functions::GenerateID, ());
  else if (name == XMLExpandedName ((const uni_char *) 0, UNI_L ("unparsed-entity-uri")))
    function = OP_NEW (XSLT_Functions::UnparsedEntityUri, ());
  else if (name == XMLExpandedName ((const uni_char *) 0, UNI_L ("document")))
    function = OP_NEW (XSLT_Functions::Document, (element->GetBaseURL ()));
  else if (name == XMLExpandedName (UNI_L ("http://exslt.org/common"), UNI_L ("node-set")))
    function = OP_NEW (XSLT_Functions::NodeSet, ());
  else
    return OpStatus::ERR;

  if (!function)
    return OpStatus::ERR_NO_MEMORY;

  static_cast<XSLT_Function *> (function)->nsdeclaration = GetNamespaceDeclaration ();
  return OpStatus::OK;
}

/* virtual */ OP_STATUS
XSLT_XPathExtensions::GetVariable (XPathVariable *&variable0, const XMLExpandedName &name)
{
  if (!no_variables)
    {
      XSLT_StylesheetImpl *stylesheet = GetStylesheet ();
      XSLT_Variable *variable = previous_variable;
      BOOL in_root_scope = FALSE;

      if (!variable)
        {
          in_root_scope = TRUE;
          variable = stylesheet->GetLastVariable ();
        }

      while (variable)
        {
          if (static_cast<const XMLExpandedName &> (variable->GetName ()) == name)
            {
              variable0 = OP_NEW (XSLT_VariableReference, (variable));
              return variable0 ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
            }

          variable = variable->GetXPathExtensions ()->GetPreviousVariable ();

          if (!variable && !in_root_scope)
            {
              in_root_scope = TRUE;
              variable = stylesheet->GetLastVariable ();
            }
        }
    }

  return OpStatus::ERR;
}

XMLNamespaceDeclaration *
XSLT_XPathExtensions::GetNamespaceDeclaration ()
{
  return element->GetNamespaceDeclaration();
}

XSLT_StylesheetImpl *
XSLT_XPathExtensions::GetStylesheet ()
{
  XSLT_Element* root = element;
  while (root->GetParent())
    root = root->GetParent();
  return static_cast<XSLT_StylesheetElement *> (root)->GetStylesheet ();
}

#endif // XSLT_SUPPORT
