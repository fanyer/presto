/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_attributevaluetemplate.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xslt/src/xslt_string.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/util/tempbuf.h"

/* static */ void
XSLT_AttributeValueTemplate::CompileL (XSLT_Compiler *compiler, XSLT_Element *element, const XSLT_String &source)
{
  const uni_char *string = source.GetString ();
  unsigned length = source.GetLength ();

  unsigned start, index = 0;
  TempBuffer buffer; ANCHOR (TempBuffer, buffer);

  BOOL first = TRUE;

  while (index < length)
    {
      /* Parse plain text item. */
      while (index < length)
        {
          start = index;

          while (index < length && string[index] != '{' && string[index] != '}')
            ++index;

          BOOL escaped_brace = index < length - 1 && string[index] == string[index + 1];

          if (escaped_brace)
            ++index;

          if (escaped_brace || start < index)
            buffer.AppendL (string + start, index - start);

          if (escaped_brace)
            ++index;
          else if (string[index] == '{')
            break;
          else if (string[index] == '}')
            {
              XSLT_ADD_INSTRUCTION_WITH_ARGUMENT_EXTERNAL (XSLT_Instruction::IC_ERROR, reinterpret_cast<UINTPTR> ("invalid attribute value template"), element);
              return;
            }
        }

      if (buffer.Length () != 0)
        {
          unsigned string_index = compiler->AddStringL (buffer.GetStorage ());
          buffer.Clear ();

          if (first && index == length)
            XSLT_ADD_INSTRUCTION_WITH_ARGUMENT_EXTERNAL (XSLT_Instruction::IC_SET_STRING, string_index, element);
          else
            XSLT_ADD_INSTRUCTION_WITH_ARGUMENT_EXTERNAL (XSLT_Instruction::IC_APPEND_STRING, string_index, element);
        }

      first = FALSE;

      /* Parse expression item. */
      if (index < length)
        {
          start = ++index;

          uni_char ch;

          while (index < length && (ch = string[index]) != '}')
            {
              ++index;

              if (ch == '"' || ch == '\'')
                {
                  while (index < length && string[index++] != ch)
                    {
                      // Scan for the ending quote char
                    }
                }
            }

          if (index == length || index == start)
            {
              XSLT_ADD_INSTRUCTION_WITH_ARGUMENT_EXTERNAL (XSLT_Instruction::IC_ERROR, reinterpret_cast<UINTPTR> ("invalid attribute value template"), element);
              return;
            }

          XSLT_String expression; ANCHOR (XSLT_String, expression);

          expression.SetSubstringOfL (source, start, index - start);

          unsigned idx = compiler->AddExpressionL (expression, element->GetXPathExtensions (), element->GetNamespaceDeclaration ());

          XSLT_ADD_INSTRUCTION_WITH_ARGUMENT_EXTERNAL (XSLT_Instruction::IC_EVALUATE_TO_STRING, idx, element);

          ++index;
        }
    }
}

/* static */ BOOL
XSLT_AttributeValueTemplate::NeedsCompiledCode (const XSLT_String &source)
{
  const uni_char *string = source.GetString ();

  if (string)
    while (*string)
      {
        if (string[0] == '{' || string[0] == '}')
          if (string[0] == string[1])
            ++string;
          else
            return TRUE;

        ++string;
      }

  return FALSE;
}

/* static */ const uni_char *
XSLT_AttributeValueTemplate::UnescapeL (const XSLT_String &source, TempBuffer &buffer)
{
  OP_ASSERT (!NeedsCompiledCode (source));
  const uni_char *string = source.GetString (), *start = string, *ptr = start;
  if (ptr)
    while (*ptr)
      if (ptr[0] == '{' || ptr[0] == '}')
        {
          buffer.AppendL (start, ++ptr - start);
          start = ++ptr;
        }
      else
        ++ptr;

  if (start != string)
    {
      buffer.AppendL (start, ptr - start);
      return buffer.GetStorage ();
    }
  else
    return string;
}

#endif // XSLT_SUPPORT
