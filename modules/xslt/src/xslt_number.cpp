/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_number.h"
#include "modules/xslt/src/xslt_attributevaluetemplate.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xslt/src/xslt_xpathextensions.h"
#include "modules/xslt/src/xslt_utils.h"
#include "modules/unicode/unicode.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/layout/numbers.h"

XSLT_Number::XSLT_Number ()
  : level (XPathPattern::Count::LEVEL_SINGLE),
    count (GetXPathExtensions (), NULL), // nsdeclaration set in EndElement
    from (GetXPathExtensions (), NULL), // nsdeclaration set in EndElement
#ifdef _DEBUG
    has_compiled (FALSE),
#endif // _DEBUG
    has_grouping_separator (FALSE),
    has_grouping_size (FALSE)
{
}

static BOOL IsAlphanumeric (const uni_char *ch, unsigned &length)
{
  unsigned codepoint = *ch++;

  if (codepoint >= 0xd800 && codepoint <= 0xdbff && *ch >= 0xdc00 && *ch <= 0xdfff)
    codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (unsigned) *ch - 0xdc00;

  if (codepoint < 0x10000)
    {
      length = 1;
      CharacterClass cc;
      cc = Unicode::GetCharacterClass (codepoint);
      return cc >= CC_Ll && cc <= CC_Lu || cc >= CC_Nd && cc <= CC_No;
    }
  else
    {
      length = 2;
      // FIXME: not correct.
      return TRUE;
    }
}

static const unsigned XSLT_decimalDigitZero[] =
  {
    0x0030, // DIGIT ZERO
    0x0660, // ARABIC-INDIC DIGIT ZERO
    0x06F0, // EXTENDED ARABIC-INDIC DIGIT ZERO
    0x0966, // DEVANAGARI DIGIT ZERO
    0x09E6, // BENGALI DIGIT ZERO
    0x0A66, // GURMUKHI DIGIT ZERO
    0x0AE6, // GUJARATI DIGIT ZERO
    0x0B66, // ORIYA DIGIT ZERO
    0x0C66, // TELUGU DIGIT ZERO
    0x0CE6, // KANNADA DIGIT ZERO
    0x0D66, // MALAYALAM DIGIT ZERO
    0x0E50, // THAI DIGIT ZERO
    0x0ED0, // LAO DIGIT ZERO
    0x0F20, // TIBETAN DIGIT ZERO
    0x1040, // MYANMAR DIGIT ZERO
    0x17E0, // KHMER DIGIT ZERO
    0x1810, // MONGOLIAN DIGIT ZERO
    0x1946, // LIMBU DIGIT ZERO
    0xFF10, // FULLWIDTH DIGIT ZERO
    0x104A0, // OSMANYA DIGIT ZERO
    0x1D7CE, // MATHEMATICAL BOLD DIGIT ZERO
    0x1D7D8, // MATHEMATICAL DOUBLE-STRUCK DIGIT ZERO
    0x1D7E2, // MATHEMATICAL SANS-SERIF DIGIT ZERO
    0x1D7EC, // MATHEMATICAL SANS-SERIF BOLD DIGIT ZERO
    0x1D7F6, // MATHEMATICAL MONOSPACE DIGIT ZERO
    0
  };

static BOOL
XSLT_IsDecimalValue (const uni_char *&digit, unsigned &digit_length, unsigned value, unsigned &zero_codepoint)
{
  unsigned ch1 = *digit, ch2 = 0;
  unsigned consumed = 1;

  if (ch1 >= 0xd800 && ch1 <= 0xdbff && digit_length != 1)
    ch2 = *(digit + 1);

  unsigned codepoint;

  if (ch2 < 0xdc00 || ch2 > 0xdfff)
    codepoint = ch1;
  else
    {
      codepoint = 0x10000 + ((ch1 - 0xd800) << 10) + ch2 - 0xdc00;
      consumed = 2;
    }

  if (codepoint < value || zero_codepoint != 0 && zero_codepoint != codepoint - value)
    return FALSE;

  zero_codepoint = codepoint - value;

  const unsigned *zero_codepoints = XSLT_decimalDigitZero;

  while (*zero_codepoints)
    if (*zero_codepoints == zero_codepoint)
      {
        digit += consumed;
        digit_length -= consumed;

        return TRUE;
      }
    else
      ++zero_codepoints;

  zero_codepoint = 0;
  return FALSE;
}

static void
XSLT_AppendChar (uni_char *&destination, unsigned codepoint)
{
  if (codepoint >= 0x10000)
    {
      codepoint -= 0x10000;

      *destination++ = 0xd800 + (codepoint >> 10);
      *destination++ = 0xdc00 + (codepoint & 0x3ff);
    }
  else
    *destination++ = codepoint;
}

static void
XSLT_FormatDecimalValue (unsigned zero_codepoint, const uni_char *source, uni_char *destination, BOOL has_grouping, unsigned grouping_separator, unsigned grouping_size)
{
  unsigned index = uni_strlen (source);

  while (*source)
    {
      XSLT_AppendChar (destination, zero_codepoint + *source++ - '0');

      if (has_grouping && *source && (--index % grouping_size) == 0)
        XSLT_AppendChar (destination, grouping_separator);
    }

  *destination = 0;
}

static void
XSLT_AppendAlphabeticL (TempBuffer &buffer, const uni_char base, unsigned number)
{
  unsigned more_significant_digits = number / 26, least_significant_digit = number % 26;

  if (more_significant_digits != 0)
    XSLT_AppendAlphabeticL (buffer, base, more_significant_digits - 1);

  buffer.AppendL (base + least_significant_digit);
}

/* virtual */ BOOL
XSLT_Number::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  if (parser)
    {
      count.SetNamespaceDeclaration (parser);
      from.SetNamespaceDeclaration (parser);
    }

  return FALSE;
}

/* virtual */ void
XSLT_Number::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *attrvalue, unsigned attrvalue_length)
{
  float grouping_size_float;

  switch (type)
    {
    case XSLTA_COUNT:
      count.SetSourceL (parser, name, attrvalue, attrvalue_length);
      break;

    case XSLTA_FORMAT:
      parser->SetStringL (format, name, attrvalue, attrvalue_length);
      break;

    case XSLTA_FROM:
      from.SetSourceL (parser, name, attrvalue, attrvalue_length);
      break;

    case XSLTA_LEVEL:
      if (XSLT_CompareStrings (attrvalue, attrvalue_length, "multiple"))
        level = XPathPattern::Count::LEVEL_MULTIPLE;
      else if (XSLT_CompareStrings (attrvalue, attrvalue_length, "any"))
        level = XPathPattern::Count::LEVEL_ANY;
      break;

    case XSLTA_VALUE:
      parser->SetStringL (value, name, attrvalue, attrvalue_length);
      break;

    case XSLTA_GROUPING_SEPARATOR:
      if (attrvalue_length > 0)
        {
          grouping_separator = XMLUtils::GetNextCharacter (attrvalue, attrvalue_length);
          has_grouping_separator = TRUE;
        }
      break;

    case XSLTA_GROUPING_SIZE:
      if (XSLT_Utils::ParseFloatL (grouping_size_float, attrvalue, attrvalue_length) && op_isintegral (grouping_size_float) && grouping_size_float > 0.f && grouping_size_float < (float) UINT_MAX)
        {
          has_grouping_size = TRUE;
          grouping_size = (unsigned) grouping_size_float;
        }
      break;

    default:
      XSLT_TemplateContent::AddAttributeL (parser, type, name, attrvalue, attrvalue_length);
    }
}

/* virtual */ void
XSLT_Number::CompileL (XSLT_Compiler *compiler)
{
  if (format.IsSpecified())
    XSLT_AttributeValueTemplate::CompileL(compiler, this, format);

  if (value.IsSpecified())
    {
      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_EVALUATE_TO_NUMBER, compiler->AddExpressionL (value, GetXPathExtensions (), GetNamespaceDeclaration ()));
      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_ADD_FORMATTED_NUMBER, reinterpret_cast<UINTPTR> (this));
    }
  else
    {
      XPathExtensions *extensions = GetXPathExtensions ();

      count.PreprocessL (compiler->GetMessageHandler (), extensions);
      count_pattern_count = count.GetPatternsCount ();
      if (count_pattern_count)
        count_pattern_index = compiler->AddPatternsL (count.GetPatterns (), count_pattern_count) & 0xffff;
      else
        count_pattern_index = 0;

      from.PreprocessL (compiler->GetMessageHandler (), extensions);
      from_pattern_count = from.GetPatternsCount ();
      if (from_pattern_count)
        from_pattern_index = compiler->AddPatternsL (from.GetPatterns (), from_pattern_count) & 0xffff;
      else
        from_pattern_index = 0;

      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT (XSLT_Instruction::IC_COUNT_PATTERNS_AND_ADD, reinterpret_cast<UINTPTR> (this));
    }
}

void
XSLT_Number::ConvertNumberToStringL (const uni_char *format_string, double number_value, TempBuffer &result) const
{
  unsigned number = (unsigned) number_value;

  ConvertNumbersToStringInternalL (format_string, &number, 1, result);
}

OP_STATUS
XSLT_Number::ConvertNumbersToString (const uni_char *format_string, unsigned *number_array, unsigned number_count, TempBuffer &result) const
{
  TRAPD (status, ConvertNumbersToStringInternalL (format_string, number_array, number_count, result));
  return status;
}

void
XSLT_Number::ConvertNumbersToStringInternalL (const uni_char *format_string, unsigned *numbers, unsigned number, TempBuffer &result) const
{
  if (number == 0)
    return;

  if (format.IsSpecified ())
    {
      if (!format_string)
        format_string = UNI_L ("");
    }
  else
    format_string = UNI_L ("1");

  const uni_char *formattoken = UNI_L ("1"), *separatortoken = UNI_L ("."), *previous_separatortoken = separatortoken;
  unsigned formattoken_length = 1, separatortoken_length = 1, previous_separatortoken_length = separatortoken_length;

  unsigned ch_length;
  while (*format_string && !IsAlphanumeric (format_string, ch_length))
    result.AppendL (format_string, ch_length), format_string += ch_length;

  for (unsigned index = 0; index < number; ++index)
    {
      unsigned local = numbers[index];

      BOOL found_formattoken;

      if (*format_string)
        {
          found_formattoken = TRUE;

          formattoken = format_string;
          formattoken_length = 0;

          while (*format_string && IsAlphanumeric (format_string, ch_length))
            formattoken_length += ch_length, format_string += ch_length;
        }
      else
        found_formattoken = FALSE;

      if (index != 0)
        if (found_formattoken)
          {
            result.AppendL (separatortoken, separatortoken_length);
            previous_separatortoken = separatortoken;
            previous_separatortoken_length = separatortoken_length;
          }
        else
          result.AppendL (previous_separatortoken, previous_separatortoken_length);

    again:
      unsigned zero_codepoint = 0, pad = 0;

      while (XSLT_IsDecimalValue (formattoken, formattoken_length, 0, zero_codepoint))
        ++pad;

      if (XSLT_IsDecimalValue (formattoken, formattoken_length, 1, zero_codepoint) && formattoken_length == 0)
        {
          ++pad;

          uni_char source1[16]; /* ARRAY OK jl 2008-02-07 */
          uni_snprintf (source1, 16, UNI_L ("%u"), local);
          source1[15] = 0;

          TempBuffer source2, destination; ANCHOR (TempBuffer, source2); ANCHOR (TempBuffer, destination);

          unsigned length = uni_strlen (source1);

          if (pad > length)
            {
              pad -= length;

              while (pad-- > 0)
                source2.AppendL ('0');
            }

          source2.AppendL (source1);

          /* Every digit could become a surrogate pair, and between every
             digit there might be a grouping separator, which also could be a
             surrogate pair. */
          destination.ExpandL (source2.Length () * 4 + 1);
          destination.SetCachedLengthPolicy (TempBuffer::UNTRUSTED);

          XSLT_FormatDecimalValue (zero_codepoint, source2.GetStorage (), destination.GetStorage (), has_grouping_separator && has_grouping_size, grouping_separator, grouping_size);

          result.AppendL (destination.GetStorage ());
        }
      else if (pad > 0)
        goto invalid;
      else if (Unicode::ToUpper (*formattoken) == 'A' && formattoken_length == 1)
        {
          if (local != 0)
            XSLT_AppendAlphabeticL (result, *formattoken, local - 1);
        }
      else if (Unicode::ToUpper (*formattoken) == 'I' && formattoken_length == 1)
        {
          if (local != 0)
            {
              int prev_len = result.Length ();
              result.ExpandL (prev_len + 256);
              int length = MakeRomanStr ((int) local, result.GetStorage () + prev_len, 255, *formattoken == 'I');
              result.SetCachedLengthPolicy(TempBuffer::UNTRUSTED);
              result.GetStorage ()[prev_len + length] = 0;
            }
        }
      else
        {
        invalid:
          formattoken = UNI_L ("1");
          formattoken_length = 1;
          goto again;
        }

      if (found_formattoken)
        if (*format_string)
          {
            separatortoken = format_string;
            separatortoken_length = 0;

            while (*format_string && !IsAlphanumeric (format_string, ch_length))
              separatortoken_length += ch_length, format_string += ch_length;
          }
    }

  if (!*format_string)
    {
      if (previous_separatortoken != separatortoken)
        result.AppendL (separatortoken, separatortoken_length);
    }
  else
    {
      while (*format_string)
        ++format_string;

      // FIXME: this is wrong.
      while (!IsAlphanumeric (format_string - 1, ch_length))
        format_string -= ch_length;

      result.AppendL (format_string);
    }
}

void
XSLT_Number::GetCountPatternIndices(unsigned &out_count_pattern_index, unsigned &out_count_pattern_count) const
{
  out_count_pattern_index = count_pattern_index;
  out_count_pattern_count = count_pattern_count;
}

void
XSLT_Number::GetFromPatternIndices(unsigned &out_from_pattern_index, unsigned &out_from_pattern_count) const
{
  out_from_pattern_index = from_pattern_index;
  out_from_pattern_count = from_pattern_count;
}

#endif // XSLT_SUPPORT
