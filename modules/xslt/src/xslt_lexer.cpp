/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_lexer.h"
#include "modules/xmlutils/xmlnames.h"

static const char xsltelementnames[] =
  {
    'a', 'p', 'p', 'l', 'y', '-', 'i', 'm', 'p', 'o', 'r', 't', 's', 0,
    'a', 'p', 'p', 'l', 'y', '-', 't', 'e', 'm', 'p', 'l', 'a', 't', 'e', 's', 0,
    'a', 't', 't', 'r', 'i', 'b', 'u', 't', 'e', 0,
    'a', 't', 't', 'r', 'i', 'b', 'u', 't', 'e', '-', 's', 'e', 't', 0,
    'c', 'a', 'l', 'l', '-', 't', 'e', 'm', 'p', 'l', 'a', 't', 'e', 0,
    'c', 'h', 'o', 'o', 's', 'e', 0,
    'c', 'o', 'm', 'm', 'e', 'n', 't', 0,
    'c', 'o', 'p', 'y', 0,
    'c', 'o', 'p', 'y', '-', 'o', 'f', 0,
    'd', 'e', 'c', 'i', 'm', 'a', 'l', '-', 'f', 'o', 'r', 'm', 'a', 't', 0,
    'e', 'l', 'e', 'm', 'e', 'n', 't', 0,
    'f', 'a', 'l', 'l', 'b', 'a', 'c', 'k', 0,
    'f', 'o', 'r', '-', 'e', 'a', 'c', 'h', 0,
    'i', 'f', 0,
    'i', 'm', 'p', 'o', 'r', 't', 0,
    'i', 'n', 'c', 'l', 'u', 'd', 'e', 0,
    'k', 'e', 'y', 0,
    'm', 'e', 's', 's', 'a', 'g', 'e', 0,
    'n', 'a', 'm', 'e', 's', 'p', 'a', 'c', 'e', '-', 'a', 'l', 'i', 'a', 's', 0,
    'n', 'u', 'm', 'b', 'e', 'r', 0,
    'o', 't', 'h', 'e', 'r', 'w', 'i', 's', 'e', 0,
    'o', 'u', 't', 'p', 'u', 't', 0,
    'p', 'a', 'r', 'a', 'm', 0,
    'p', 'r', 'e', 's', 'e', 'r', 'v', 'e', '-', 's', 'p', 'a', 'c', 'e', 0,
    'p', 'r', 'o', 'c', 'e', 's', 's', 'i', 'n', 'g', '-', 'i', 'n', 's', 't', 'r', 'u', 'c', 't', 'i', 'o', 'n', 0,
    's', 'o', 'r', 't', 0,
    's', 't', 'r', 'i', 'p', '-', 's', 'p', 'a', 'c', 'e', 0,
    's', 't', 'y', 'l', 'e', 's', 'h', 'e', 'e', 't', 0,
    't', 'e', 'm', 'p', 'l', 'a', 't', 'e', 0,
    't', 'e', 'x', 't', 0,
    't', 'r', 'a', 'n', 's', 'f', 'o', 'r', 'm', 0,
    'v', 'a', 'l', 'u', 'e', '-', 'o', 'f', 0,
    'v', 'a', 'r', 'i', 'a', 'b', 'l', 'e', 0,
    'w', 'h', 'e', 'n', 0,
    'w', 'i', 't', 'h', '-', 'p', 'a', 'r', 'a', 'm', 0
  };

static const char xsltattributenames[] =
  {
    'N', 'a', 'N', 0,
    'c', 'a', 's', 'e', '-', 'o', 'r', 'd', 'e', 'r', 0,
    'c', 'd', 'a', 't', 'a', '-', 's', 'e', 'c', 't', 'i', 'o', 'n', '-', 'e', 'l', 'e', 'm', 'e', 'n', 't', 's', 0,
#ifdef XSLT_COPY_TO_FILE_SUPPORT
    'c', 'o', 'p', 'y', '-', 't', 'o', '-', 'f', 'i', 'l', 'e', 0,
#endif // XSLT_COPY_TO_FILE_SUPPORT
    'c', 'o', 'u', 'n', 't', 0,
    'd', 'a', 't', 'a', '-', 't', 'y', 'p', 'e', 0,
    'd', 'e', 'c', 'i', 'm', 'a', 'l', '-', 's', 'e', 'p', 'a', 'r', 'a', 't', 'o', 'r', 0,
    'd', 'i', 'g', 'i', 't', 0,
    'd', 'i', 's', 'a', 'b', 'l', 'e', '-', 'o', 'u', 't', 'p', 'u', 't', '-', 'e', 's', 'c', 'a', 'p', 'i', 'n', 'g', 0,
    'd', 'o', 'c', 't', 'y', 'p', 'e', '-', 'p', 'u', 'b', 'l', 'i', 'c', 0,
    'd', 'o', 'c', 't', 'y', 'p', 'e', '-', 's', 'y', 's', 't', 'e', 'm', 0,
    'e', 'l', 'e', 'm', 'e', 'n', 't', 's', 0,
    'e', 'n', 'c', 'o', 'd', 'i', 'n', 'g', 0,
    'e', 'x', 'c', 'l', 'u', 'd', 'e', '-', 'r', 'e', 's', 'u', 'l', 't', '-', 'p', 'r', 'e', 'f', 'i', 'x', 'e', 's', 0,
    'e', 'x', 't', 'e', 'n', 's', 'i', 'o', 'n', '-', 'e', 'l', 'e', 'm', 'e', 'n', 't', '-', 'p', 'r', 'e', 'f', 'i', 'x', 'e', 's', 0,
    'f', 'o', 'r', 'm', 'a', 't', 0,
    'f', 'r', 'o', 'm', 0,
    'g', 'r', 'o', 'u', 'p', 'i', 'n', 'g', '-', 's', 'e', 'p', 'a', 'r', 'a', 't', 'o', 'r', 0,
    'g', 'r', 'o', 'u', 'p', 'i', 'n', 'g', '-', 's', 'i', 'z', 'e', 0,
    'h', 'r', 'e', 'f', 0,
    'i', 'd', 0,
    'i', 'n', 'd', 'e', 'n', 't', 0,
    'i', 'n', 'f', 'i', 'n', 'i', 't', 'y', 0,
    'l', 'a', 'n', 'g', 0,
    'l', 'e', 'v', 'e', 'l', 0,
    'm', 'a', 't', 'c', 'h', 0,
    'm', 'e', 'd', 'i', 'a', '-', 't', 'y', 'p', 'e', 0,
    'm', 'e', 't', 'h', 'o', 'd', 0,
    'm', 'i', 'n', 'u', 's', '-', 's', 'i', 'g', 'n', 0,
    'm', 'o', 'd', 'e', 0,
    'n', 'a', 'm', 'e', 0,
    'n', 'a', 'm', 'e', 's', 'p', 'a', 'c', 'e', 0,
    'o', 'm', 'i', 't', '-', 'x', 'm', 'l', '-', 'd', 'e', 'c', 'l', 'a', 'r', 'a', 't', 'i', 'o', 'n', 0,
    'o', 'r', 'd', 'e', 'r', 0,
    'p', 'a', 't', 't', 'e', 'r', 'n', '-', 's', 'e', 'p', 'a', 'r', 'a', 't', 'o', 'r', 0,
    'p', 'e', 'r', '-', 'm', 'i', 'l', 'l', 'e', 0,
    'p', 'e', 'r', 'c', 'e', 'n', 't', 0,
    'p', 'r', 'i', 'o', 'r', 'i', 't', 'y', 0,
    'r', 'e', 's', 'u', 'l', 't', '-', 'p', 'r', 'e', 'f', 'i', 'x', 0,
    's', 'e', 'l', 'e', 'c', 't', 0,
    's', 't', 'a', 'n', 'd', 'a', 'l', 'o', 'n', 'e', 0,
    's', 't', 'y', 'l', 'e', 's', 'h', 'e', 'e', 't', '-', 'p', 'r', 'e', 'f', 'i', 'x', 0,
    't', 'e', 'r', 'm', 'i', 'n', 'a', 't', 'e', 0,
    't', 'e', 's', 't', 0,
    'u', 's', 'e', 0,
    'u', 's', 'e', '-', 'a', 't', 't', 'r', 'i', 'b', 'u', 't', 'e', '-', 's', 'e', 't', 's', 0,
    'v', 'a', 'l', 'u', 'e', 0,
    'v', 'e', 'r', 's', 'i', 'o', 'n', 0,
    'z', 'e', 'r', 'o', '-', 'd', 'i', 'g', 'i', 't', 0
  };

/* static */ XSLT_ElementType
XSLT_Lexer::GetElementType (const XMLExpandedNameN &name)
{
  if (name.IsXSLT ())
    {
      const char *xsltelementname = xsltelementnames;
      unsigned index = 0;

      while (index < XSLTE_LAST)
        {
          unsigned length = op_strlen (xsltelementname);

          if (length == name.GetLocalPartLength () && uni_strncmp (name.GetLocalPart (), xsltelementname, length) == 0)
            return (XSLT_ElementType) index;

          ++index;
          xsltelementname += length + 1;
        }

      return XSLTE_UNRECOGNIZED;
    }
  else
    return XSLTE_OTHER;
}

/* static */ XSLT_AttributeType
XSLT_Lexer::GetAttributeType (BOOL element_is_xslt, const XMLExpandedNameN &name)
{
  if (element_is_xslt && !name.GetUri () || name.IsXSLT ())
    {
      const char *xsltattributename = xsltattributenames;
      unsigned index = 0;

      while (index < XSLTA_LAST)
        {
          unsigned length = op_strlen (xsltattributename);

          if (length == name.GetLocalPartLength () && uni_strncmp (name.GetLocalPart (), xsltattributename, length) == 0)
            return (XSLT_AttributeType) index;

          ++index;
          xsltattributename += length + 1;
        }

      return XSLTA_UNRECOGNIZED;
    }
  else
    return XSLTA_OTHER;
}

/* static */ const char *
XSLT_Lexer::GetElementName (XSLT_ElementType type)
{
  switch (type)
    {
    case XSLTE_UNSUPPORTED:
      return "(unsupported extension element)";

    case XSLTE_UNRECOGNIZED:
      return "(unrecognized element in the XSLT namespace)";

    case XSLTE_OTHER:
      return "(literal result element)";

    case XSLTE_LITERAL_RESULT_TEXT_NODE:
      return "text()";

    default:
      const char *xsltelementname = xsltelementnames;
      unsigned index = type;

      while (index-- != 0)
        xsltelementname += op_strlen (xsltelementname) + 1;

      return xsltelementname;
    }
}

/* static */ const char *
XSLT_Lexer::GetAttributeName (XSLT_AttributeType type)
{
  switch (type)
    {
    case XSLTA_UNRECOGNIZED:
      return "(unrecognized attribute in the XSLT namespace)";

    case XSLTA_OTHER:
      return "(attribute in unknown namespace)";

    case XSLTA_NO_MORE_ATTRIBUTES:
      return "(internal)";

    default:
      const char *xsltattributename = xsltattributenames;
      unsigned index = type;

      while (index-- != 0)
        xsltattributename += op_strlen (xsltattributename) + 1;

      return xsltattributename;
    }
}

#endif // XSLT_SUPPORT
