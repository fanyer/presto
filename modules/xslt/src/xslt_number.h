/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_NUMBER_H
#define XSLT_NUMBER_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/xpath/xpath.h"
#include "modules/xslt/src/xslt_string.h"

class XSLT_StylesheetParserImpl;

class XSLT_Number
  : public XSLT_EmptyTemplateContent
{
private:
  XSLT_String value;
  XPathPattern::Count::Level level;
  XSLT_XPathPattern count, from;
  XSLT_String format;
#ifdef _DEBUG
  BOOL has_compiled;
#endif // _DEBUG
  unsigned count_pattern_index;
  unsigned count_pattern_count;
  unsigned from_pattern_index;
  unsigned from_pattern_count;

  BOOL has_grouping_separator, has_grouping_size;
  unsigned grouping_separator, grouping_size;

  void ConvertNumbersToStringInternalL (const uni_char *format_string, unsigned *numbers, unsigned number, TempBuffer &buffer) const;

public:
  XSLT_Number ();

  virtual void CompileL (XSLT_Compiler *compiler);

  virtual BOOL EndElementL (XSLT_StylesheetParserImpl *parser);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);

  void ConvertNumberToStringL (const uni_char *format_string, double number_value, TempBuffer &result) const;
  void GetCountPatternIndices (unsigned &count_pattern_index, unsigned &count_pattern_count) const;
  void GetFromPatternIndices (unsigned &from_pattern_index, unsigned &from_pattern_count) const;

  XPathPattern::Count::Level GetLevel () const { return level; }

  OP_STATUS ConvertNumbersToString (const uni_char *format_string, unsigned *number_array, unsigned number_count, TempBuffer &result) const;
};

#endif // XSLT_SUPPORT
#endif // XSLT_NUMBER_H
