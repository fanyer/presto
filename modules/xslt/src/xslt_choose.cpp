/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_choose.h"
#include "modules/xslt/src/xslt_iforwhen.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/util/adt/opvector.h"

XSLT_Choose::XSLT_Choose ()
  : has_when (FALSE),
    has_otherwise (FALSE)
{
}

/* virtual */ void
XSLT_Choose::CompileL (XSLT_Compiler *compiler)
{
  OpINT32Vector true_jumps;

  unsigned index = 0;
  // First all |when|s
  for (;index < children_count; ++index)
    {
      XSLT_TemplateContent *child = children[index];

      if (child->GetType () == XSLTE_WHEN)
        {
          unsigned true_target = static_cast<XSLT_IfOrWhen*>(child)->CompileConditionalCodeL(compiler);
          OP_ASSERT(true_target > 0 && true_target == (unsigned)(INT32)true_target);
          LEAVE_IF_ERROR(true_jumps.Add((INT32)true_target));
        }
      else if (child->GetType () == XSLTE_OTHERWISE)
        break;
    }

  // Then the |otherwise|
  for (;index < children_count; ++index)
  {
    XSLT_TemplateContent *child = children[index];
    if (child->GetType () == XSLTE_OTHERWISE)
      {
        child->CompileL(compiler);
        break;
      }
  }

  for (unsigned block = 0; block < true_jumps.GetCount(); block++)
    {
      unsigned true_target = (unsigned)true_jumps.Get(block);
      compiler->SetJumpDestination(true_target);
    }

}

/* virtual */ XSLT_Element *
XSLT_Choose::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element)
{
  if (type == XSLTE_WHEN || type == XSLTE_OTHERWISE)
    {
      if (type == XSLTE_WHEN)
        if (has_otherwise)
          {
            parser->SignalErrorL ("xsl:when after xsl:otherwise in xsl:choose");
            ignore_element = TRUE;
            return this;
          }
        else
          has_when = TRUE;
      else if (!has_when)
        {
          parser->SignalErrorL ("xsl:otherwise before xsl:when in xsl:choose");
          ignore_element = TRUE;
          return this;
        }
      else if (has_otherwise)
        {
          parser->SignalErrorL ("too many xsl:otherwise in xsl:choose");
          ignore_element = TRUE;
          return this;
        }
      else
        has_otherwise = TRUE;

      return XSLT_TemplateContent::StartElementL (parser, type, name, ignore_element);
    }
  else
    {
      parser->SignalErrorL ("unexpected element");
      ignore_element = TRUE;
      return this;
    }
}

/* virtual */ BOOL
XSLT_Choose::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  if (parser)
    if (!has_when)
      SignalErrorL (parser, "xsl:choose without xsl:when");

  return FALSE;
}

/* virtual */ void
XSLT_Choose::AddCharacterDataL (XSLT_StylesheetParserImpl *parser, XSLT_StylesheetParser::CharacterDataType type, const uni_char *value, unsigned value_length)
{
  XSLT_Element::AddCharacterDataL (parser, type, value, value_length);
}

#endif // XSLT_SUPPORT
