/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_ATTRIBUTEVALUETEMPLATE_H
#define XSLT_ATTRIBUTEVALUETEMPLATE_H

#ifdef XSLT_SUPPORT

class XSLT_StylesheetParserImpl;
class XSLT_XPathExpression;
class XSLT_Element;
class XSLT_String;
class XSLT_Compiler;

class XSLT_AttributeValueTemplate
{
public:
  static void CompileL (XSLT_Compiler *compiler, XSLT_Element *element, const XSLT_String &source);

  static BOOL NeedsCompiledCode (const XSLT_String &source);
  /**< Returns FALSE if the attribute value template contains no expression
       parts, and thus is really just a constant string. */

  static const uni_char *UnescapeL (const XSLT_String &source, TempBuffer &buffer);
  /**< Unescapes {{ or }} in 'source'.  It can only be called on strings that
       NeedsCompiledCode() returns FALSE for. The returned string is generated into 'buffer' if
       any characters needed unescaping, otherwise buffer is not used and
       'source.GetString()' is returned directly. */
};

#endif // XSLT_SUPPORT
#endif // XSLT_ATTRIBUTEVALUETEMPLATE_H
