/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_unsupported.h"

#include "modules/xslt/src/xslt_compiler.h"

/* virtual */ void
XSLT_Unsupported::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_EXTENSION_ELEMENT_PREFIXES:
      if (GetType () == XSLTE_UNSUPPORTED)
        {
          parser->SetStringL (extension_elements, name, value, value_length);
          break;
        }

    case XSLTA_NO_MORE_ATTRIBUTES:
    case XSLTA_OTHER:
      break;

    default:
      XSLT_Element::AddAttributeL (parser, type, name, value, value_length);
    }
}

/* virtual */ void
XSLT_Unsupported::CompileL (XSLT_Compiler *compiler)
{
  for (unsigned index = 0; index < children_count; ++index)
    if (children[index]->GetType () == XSLTE_FALLBACK)
      children[index]->CompileL(compiler);
}

#endif // XSLT_SUPPORT
