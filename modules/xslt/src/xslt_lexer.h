/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_LEXER_H
#define XSLT_LEXER_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_types.h"

class XMLExpandedNameN;

class XSLT_Lexer
{
public:
  static XSLT_ElementType GetElementType (const XMLExpandedNameN &name);
  static XSLT_AttributeType GetAttributeType (BOOL element_is_xslt, const XMLExpandedNameN &name);

  static const char *GetElementName (XSLT_ElementType type);
  static const char *GetAttributeName (XSLT_AttributeType type);
};

#endif // XSLT_SUPPORT
#endif // XSLT_LEXER_H
