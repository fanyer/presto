/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef SELFTEST

#include "modules/xmlparser/xmlstsupport.h"
#include "modules/xmlparser/xmldatasource.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/util/opautoptr.h"
#include "modules/util/opstring.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/tempbuf.h"
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/encodings/decoders/utf16-decoder.h"
#include "modules/encodings/decoders/iso-8859-1-decoder.h"

static OP_STATUS
XMLSelftestDecode (const char *data, unsigned data_length, OpString &storage)
{
  InputConverter *converter = 0;

  if (data_length >= 2 && ((unsigned char *) data)[0] == 0xfe && ((unsigned char *) data)[1] == 0xff)
    {
      converter = OP_NEW(UTF16BEtoUTF16Converter, ());
      data += 2;
      data_length -= 2;
    }
  else if (data_length >= 2 && ((unsigned char *) data)[0] == 0xff && ((unsigned char *) data)[1] == 0xfe)
    {
      converter = OP_NEW(UTF16LEtoUTF16Converter, ());
      data += 2;
      data_length -= 2;
    }
  else if (data_length >= 5 && op_strncmp (data, "<?xml", 5) == 0)
    {
      const char *encoding = 0, *end = 0;

      for (unsigned index = 0; index < data_length; ++index)
        {
          if (!encoding && data_length - index >= 8 && op_memcmp (data + index, "encoding", 8) == 0)
            encoding = data + index;

          if (data_length - index >= 2 && op_memcmp (data + index, "?>", 2) == 0)
            {
              end = data + index;
              break;
            }
        }

      if (!end)
        end = data + data_length;

      if (encoding && end && encoding < end)
        {
          while (encoding != end && *encoding != '=')
            ++encoding;

          while (encoding != end && *encoding != '"' && *encoding != '\'')
            ++encoding;

          if (encoding != end)
            {
              char delimiter = *encoding;

              ++encoding;

              const char *encoding_start = encoding;

              while (encoding != end && *encoding != delimiter)
                ++encoding;

              if (encoding != end)
                {
                  OpString8 encoding_string;

                  RETURN_IF_ERROR (encoding_string.Set (encoding_start, encoding - encoding_start));

                  if (encoding_string.CompareI("ISO-8859-1") == 0)
                    converter = OP_NEW(ISOLatin1toUTF16Converter, ());
                  else
                    {
                      OpStatus::Ignore (InputConverter::CreateCharConverter(encoding_string.CStr(), &converter));
                    }
                }
            }
        }
    }

  if (!converter)
    /* Fall back to UTF-8.  Either it is the right one or the XMLDecl was
       invalid and we're just interested in reporting that. */
    OpStatus::Ignore (InputConverter::CreateCharConverter("UTF-8", &converter));

  if (!converter)
    /* Probably OOM, but who cares? */
    return OpStatus::ERR;

  uni_char buffer[8192]; /* ARRAY OK jl 2008-02-07 */

  int read, written = converter->Convert (data, data_length, buffer, sizeof buffer, &read);

  OP_DELETE(converter);

  if (read != (int) data_length || written == -1 || written == sizeof buffer)
    return OpStatus::ERR;

  return storage.Set (buffer, UNICODE_DOWNSIZE (written));
}

static OP_STATUS
XMLSelftestTranslate (const uni_char *data, unsigned data_length, OpString &storage)
{
  TempBuffer buffer;
  const uni_char *ptr = data, *ptr_end = ptr + data_length;

  while (ptr != ptr_end)
    if (ptr[0] == 'U' && ptr[1] == '+')
      {
        unsigned index = 2, character = 0;

        while (XMLInternalParser::IsHexDigit (ptr[index]))
          {
            if (ptr[index] >= '0' && ptr[index] <= '9')
              character = (character << 4) + ptr[index] - '0';
            else
              character = (character << 4) + ptr[index] - 'A' + 10;

            ++index;
          }

        ptr += index;

        if (character < 0x10000)
          RETURN_IF_ERROR (buffer.Append ((uni_char) character));
        else
          {
            character -= 0x10000;
            RETURN_IF_ERROR (buffer.Append ((uni_char) (0xd800 | (character >> 10))));
            RETURN_IF_ERROR (buffer.Append ((uni_char) (0xdc00 | (character & 0x3ff))));
          }
      }
    else
      {
        RETURN_IF_ERROR (buffer.Append (*ptr));
        ++ptr;
      }

  return storage.Set (buffer.GetStorage ());
}

static OP_STATUS
XMLSelftestTranslateInput (const uni_char *data, unsigned data_length, OpString &storage)
{
  TempBuffer buffer;

  const uni_char *ptr = data, *ptr_end = ptr + data_length;

  while (ptr != ptr_end)
    {
      const uni_char *skip_start = ptr;

      while (ptr != ptr_end && *ptr != '{')
        ++ptr;

      if (skip_start != ptr)
        RETURN_IF_ERROR (buffer.Append (skip_start, ptr - skip_start));

      if (ptr != ptr_end)
        {
          const uni_char *translate_start = ptr;

          while (ptr != ptr_end && *ptr != '}')
            ++ptr;

          OpString translated;

          RETURN_IF_ERROR (XMLSelftestTranslate (translate_start + 1, ptr - translate_start - 1, translated));
          RETURN_IF_ERROR (buffer.Append (translated.CStr ()));

          ++ptr;
        }
    }

  return storage.Set (buffer.GetStorage ());
}

class XMLSelftestTokenHandler
  : public XMLTokenHandler
{
public:
  XMLSelftestTokenHandler (XMLInternalParser *parser, BOOL test)
    : parser (parser),
      test (test),
      test_text (FALSE),
      test_attribute (FALSE),
      pick_up_test (FALSE),
      pick_up_expected (FALSE)
    {
    }

  virtual Result HandleToken (XMLToken &token)
    {
      if (test)
        {
          if (!test_text && !test_attribute)
            {
              if (token.GetType () == XMLToken::TYPE_STag)
                if (XMLInternalParser::CompareStrings (token.GetName ().GetLocalPart (), token.GetName ().GetLocalPartLength (), UNI_L ("test-text")))
                  test_text = TRUE;
                else if (XMLInternalParser::CompareStrings (token.GetName ().GetLocalPart (), token.GetName ().GetLocalPartLength (), UNI_L ("test-attribute")))
                  test_attribute = TRUE;
            }
          else if ((token.GetType () == XMLToken::TYPE_STag || token.GetType () == XMLToken::TYPE_EmptyElemTag) && XMLInternalParser::CompareStrings (token.GetName ().GetLocalPart (), token.GetName ().GetLocalPartLength (), UNI_L ("test")))
            if (test_text)
              pick_up_test = TRUE;
            else
              {
                if (XMLToken::Attribute *attribute = token.GetAttribute (UNI_L ("attribute")))
                  {
                    const uni_char *value = attribute->GetValue ();
                    test_out.Set (value, attribute->GetValueLength ());
                  }
              }
          else if ((token.GetType () == XMLToken::TYPE_STag || token.GetType () == XMLToken::TYPE_EmptyElemTag) && XMLInternalParser::CompareStrings (token.GetName ().GetLocalPart (), token.GetName ().GetLocalPartLength (), UNI_L ("expected")))
            if (test_text)
              pick_up_expected = TRUE;
            else
              {
                if (XMLToken::Attribute *attribute = token.GetAttribute (UNI_L ("attribute")))
                  {
                    const uni_char *value = attribute->GetValue ();
                    expected_out.Set (value, attribute->GetValueLength ());
                  }
              }
          else if (test_text && token.GetType () == XMLToken::TYPE_Text)
            if (pick_up_test)
              {
                uni_char *literal = parser->GetLiteralAllocatedValue ();
                test_out.Set (literal);
                OP_DELETEA(literal);
                pick_up_test = FALSE;
              }
            else if (pick_up_expected)
              {
                uni_char *literal = parser->GetLiteralAllocatedValue ();
                XMLSelftestTranslate (literal, uni_strlen (literal), expected_out);
                OP_DELETEA(literal);
                pick_up_expected = FALSE;
              }
        }

      return RESULT_OK;
    }

  BOOL Check ()
    {
      const uni_char *test = test_out.CStr (), *expected = expected_out.CStr ();
      return uni_strcmp (test ? test : UNI_L (""), expected ? expected : UNI_L ("")) == 0;
    }

protected:
  XMLInternalParser *parser;
  BOOL test, test_text, test_attribute, pick_up_test, pick_up_expected;
  OpString test_out, expected_out;
};

class XMLSelftestStringDataSource
  : public XMLStringDataSource
{
public:
  XMLSelftestStringDataSource (OpString *data, XMLSelftestHelper::EndOfData endofdata = XMLSelftestHelper::NORMAL)
    : XMLStringDataSource (data->CStr () ? data->CStr () : UNI_L (""), data->Length ()),
      data (data),
      endofdata (endofdata),
      hidden (endofdata >= XMLSelftestHelper::LEVEL_1 && endofdata <= XMLSelftestHelper::LEVEL_3 && data_length != 0 ? data_length - 1 : 0),
      gray (0),
      want_more (FALSE)
    {
    }

  virtual ~XMLSelftestStringDataSource ()
    {
      OP_DELETE(data);
    }

  virtual unsigned GetDataLength ()
    {
      return XMLStringDataSource::GetDataLength () - hidden;
    }

  virtual unsigned Consume (unsigned length)
    {
      if (endofdata == XMLSelftestHelper::NO_CONSUME)
        return 0;
      else
        {
          OP_ASSERT (data_length - hidden >= length);
          return XMLStringDataSource::Consume (length);
        }
    }

  virtual OP_BOOLEAN Grow ()
    {
      if (hidden != 0)
        {
          if (endofdata == XMLSelftestHelper::LEVEL_1)
            {
              --hidden;

              SwapBuffer ();
              return OpBoolean::IS_TRUE;
            }
          else if (endofdata == XMLSelftestHelper::LEVEL_3 && gray != 0)
            {
              hidden -= gray;
              gray = 0;

              SwapBuffer ();
              return OpBoolean::IS_TRUE;
            }
          else
            {
              want_more = TRUE;
              return OpBoolean::IS_FALSE;
            }
        }
      else
        return OpBoolean::IS_FALSE;
    }

  virtual BOOL IsAtEnd ()
    {
      return hidden == 0;
    }

  virtual BOOL IsAllSeen ()
    {
      return hidden == 0;
    }

  void MoreData ()
    {
      if (want_more && hidden != 0)
        if (endofdata == XMLSelftestHelper::LEVEL_2)
          {
            --hidden;
            SwapBuffer ();
          }
        else if (endofdata == XMLSelftestHelper::LEVEL_3)
          ++gray;
    }

  void SwapBuffer ()
    {
      OpString temporary; ANCHOR(OpString, temporary);

      temporary.TakeOver (*data);
      data->SetL (temporary);
      XMLStringDataSource::data = data->CStr ();

      op_memset ((void *) temporary.CStr (), 0, temporary.Length ());
    }

protected:
  OpString *data;
  XMLSelftestHelper::EndOfData endofdata;
  unsigned hidden, gray;
  BOOL want_more;
};

class XMLSelftestDataSourceHandler
  : public XMLDataSourceHandler
{
public:
  XMLSelftestDataSourceHandler (XMLDataSource *base, const uni_char *basename = 0, XMLSelftestHelper::EndOfData endofdata = XMLSelftestHelper::NORMAL)
    : base (base),
      current (base),
      basename (basename),
      endofdata (endofdata)
    {
    }

  virtual OP_STATUS CreateInternalDataSource (XMLDataSource *&source, const uni_char *data, unsigned data_length)
    {
      current = source = OP_NEW(XMLStringDataSource, (data, data_length));
      return source ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
    }

  virtual OP_STATUS CreateExternalDataSource (XMLDataSource *&source, const uni_char *pubid, const uni_char *system, URL baseurl)
    {
      source = 0;

      if (basename)
        {
          OpString buffer;

          RETURN_IF_ERROR (buffer.Set (basename));
          RETURN_IF_ERROR (buffer.Append (system));

          OpFile file;

          RETURN_IF_ERROR (file.Construct (buffer.CStr ()));

          if (OpStatus::IsError (file.Open (OPFILE_READ)))
            return OpStatus::OK;

          char data[8192]; /* ARRAY OK jl 2008-02-07 */
          OpFileLength read;

          RETURN_IF_ERROR (file.Read (data, sizeof data, &read));

          if (!file.Eof ())
            return OpStatus::ERR;

          OpAutoPtr<OpString> datastring (OP_NEW(OpString, ()));
          if (!datastring.get ())
            return OpStatus::ERR_NO_MEMORY;

          RETURN_IF_ERROR (XMLSelftestDecode (data, (unsigned) read, *datastring.get ()));

          current = source = OP_NEW(XMLSelftestStringDataSource, (datastring.get (), endofdata));

          if (source)
            {
              datastring.release ();
              return OpStatus::OK;
            }
          else
            return OpStatus::ERR_NO_MEMORY;
        }
      else
        {
          source = 0;
          return OpStatus::OK;
        }
    }

  virtual OP_STATUS DestroyDataSource (XMLDataSource *source)
    {
      current = source->GetNextSource ();
      OP_DELETE(source);
      return OpStatus::OK;
    }

  XMLDataSource *GetCurrent ()
    {
      return current;
    }

protected:
  XMLDataSource *base, *current;
  const uni_char *basename;
  XMLSelftestHelper::EndOfData endofdata;
};

XMLSelftestHelper::XMLSelftestHelper (const char *source, unsigned source_length, const uni_char *basename, Mode mode, Expected expected, XMLSelftestHelper::EndOfData endofdata)
  : source (source),
    source_length (source_length),
    basename (basename),
    mode (mode),
    expected (expected),
    endofdata (endofdata)
{
}

XMLSelftestHelper::~XMLSelftestHelper ()
{
}

//#define XML_STOP_AT_SELFTEST
#ifdef XML_STOP_AT_SELFTEST

static BOOL
XMLEndsWith (const char *filename, const char *fragment)
{
  return op_strlen (filename) >= op_strlen (fragment) && op_strcmp (filename + op_strlen (filename) - op_strlen (fragment), fragment) == 0;
}

#endif // XML_STOP_AT_SELFTEST

/* static */ BOOL
XMLSelftestHelper::FromFile (const char *filename, Mode mode, Expected expected, EndOfData endofdata)
{
  OpString buffer;
  OpFile file;

  if (!OpStatus::IsSuccess (buffer.Set (filename)))
    return FALSE;

  if (!OpStatus::IsSuccess (file.Construct (buffer.CStr ())))
    return FALSE;

  if (!OpStatus::IsSuccess (file.Open (OPFILE_READ)))
    return FALSE;

  char data[8192]; /* ARRAY OK jl 2008-02-07 */
  OpFileLength read;

  if (!OpStatus::IsSuccess (file.Read (data, sizeof data, &read)))
    return FALSE;

  if (!file.Eof ())
    return FALSE;

  data[read] = 0;

  int pos = buffer.FindLastOf (PATHSEPCHAR);
  buffer.Delete (pos + 1);

  XMLSelftestHelper helper (data, (unsigned) read, buffer.CStr (), mode, expected, endofdata);

#ifdef XML_STOP_AT_SELFTEST
  // This can be used to stop at a specific test.  Modify "--filename--"
  // and the conditions after and set a breakpoint.
  if (XMLEndsWith (filename, "--filename--") && mode == PARSE && endofdata == NORMAL)
    int stop_here = 0;
#endif // XML_STOP_AT_SELFTEST

  return helper.Parse ();
}

BOOL
XMLSelftestHelper::Parse ()
{
  OpString *buffer = OP_NEW(OpString, ());

  if (!buffer || !OpStatus::IsSuccess (XMLSelftestDecode (source, source_length, *buffer)))
    {
      OP_DELETE(buffer);
      return FALSE;
    }

  if (mode == TOKENS)
    {
      OpString *temporary = OP_NEW(OpString, ());

      if (!temporary || !OpStatus::IsSuccess (XMLSelftestTranslateInput (buffer->CStr (), buffer->Length (), *temporary)))
        {
    	  OP_DELETE(temporary);
    	  OP_DELETE(buffer);
          return FALSE;
        }

      OP_DELETE(buffer);
      buffer = temporary;
    }

  XMLParser::Configuration configuration;
  XMLSelftestStringDataSource datasource (buffer, endofdata);
  XMLSelftestDataSourceHandler datasourcehandler (&datasource, basename, endofdata);
  XMLInternalParser parser (0, &configuration);  // declare parser last (hence desctructed first) since it will use the datasourcehandler during destruction
  XMLSelftestTokenHandler tokenhandler (&parser, mode == TOKENS);

  parser.SetTokenHandler (&tokenhandler, FALSE);
  parser.SetDataSourceHandler (&datasourcehandler, FALSE);
  parser.SetXmlValidating (mode == VALIDATE, TRUE);
  parser.SetEntityReferenceTokens (FALSE);
  parser.SetLoadExternalEntities (mode != PARSE);
  if (OpStatus::IsError(parser.Initialize (&datasource)))
	return FALSE;

  if (endofdata == MAX_TOKENS)
    parser.SetMaxTokensPerCall (1);

  XMLInternalParser::ParseResult result;

  do
    {
      XMLSelftestStringDataSource *current = (XMLSelftestStringDataSource *) datasourcehandler.GetCurrent ();
      result = parser.Parse (current);

      if (result == XMLInternalParser::PARSE_RESULT_OK && current == datasourcehandler.GetCurrent ())
        {
    	  OP_STATUS stat;
    	  TRAP_AND_RETURN_VALUE_IF_ERROR(stat, current->MoreData(), FALSE);
        }
    }
  while (result == XMLInternalParser::PARSE_RESULT_OK);

  if (mode == TOKENS)
    return result == XMLInternalParser::PARSE_RESULT_FINISHED && tokenhandler.Check ();
  else if (expected == NO_ERRORS)
    return result == XMLInternalParser::PARSE_RESULT_FINISHED;
  else if (result == XMLInternalParser::PARSE_RESULT_ERROR)
    {
      XMLInternalParser::ParseError error;
      unsigned line, column, character;

      parser.GetLastError (error, line, column, character);

      if (expected == INVALID)
        return error >= XMLInternalParser::VALIDITY_ERROR_FIRST;
      else
        return mode == VALIDATE || error < XMLInternalParser::VALIDITY_ERROR_FIRST;
    }
  else
    return FALSE;
}

#ifdef XML_VALIDATING

XMLSelftestValidityHelper::XMLSelftestValidityHelper (const char *source)
  : report (0),
    source (source)
{
}

XMLSelftestValidityHelper::~XMLSelftestValidityHelper ()
{
  OP_DELETE(report);
}

BOOL
XMLSelftestValidityHelper::Parse ()
{
  OpString buffer;
  buffer.SetFromUTF8 (source);

  XMLParser::Configuration configuration;
  XMLStringDataSource datasource (buffer.CStr (), buffer.Length ());
  XMLSelftestDataSourceHandler datasourcehandler (&datasource);
  XMLInternalParser parser (0, &configuration);
  XMLSelftestTokenHandler tokenhandler (&parser, FALSE);

  parser.SetTokenHandler (&tokenhandler, FALSE);
  parser.SetDataSourceHandler (&datasourcehandler, FALSE);
  parser.SetXmlValidating (TRUE, FALSE);
  parser.SetEntityReferenceTokens (FALSE);
  parser.Initialize (&datasource);

  XMLInternalParser::ParseResult result;

  do
    result = parser.Parse (datasourcehandler.GetCurrent ());
  while (result == XMLInternalParser::PARSE_RESULT_OK);

  OP_DELETE(parser.GetDoctype ());

  if (result != XMLInternalParser::PARSE_RESULT_FINISHED)
    return FALSE;

  report = parser.GetValidityReport ();
  return TRUE;
}

BOOL
XMLSelftestValidityHelper::CheckErrorsCount (unsigned count)
{
  return report->GetErrorsCount () == count;
}

BOOL
XMLSelftestValidityHelper::CheckError (unsigned error_index, XMLInternalParser::ParseError expected_error)
{
  XMLInternalParser::ParseError error;
  XMLRange *ranges;
  unsigned ranges_count;
  const uni_char **data;
  unsigned data_count;
  report->GetError (error_index, error, ranges, ranges_count, data, data_count);

  return error == expected_error;
}

BOOL
XMLSelftestValidityHelper::CheckRangesCount (unsigned error_index, unsigned count)
{
  XMLInternalParser::ParseError error;
  XMLRange *ranges;
  unsigned ranges_count;
  const uni_char **data;
  unsigned data_count;
  report->GetError (error_index, error, ranges, ranges_count, data, data_count);

  return ranges_count == count;
}

BOOL
XMLSelftestValidityHelper::CheckRange (unsigned error_index, unsigned range_index, unsigned start_line, unsigned start_column, unsigned end_line, unsigned end_column)
{
  XMLInternalParser::ParseError error;
  XMLRange *ranges;
  unsigned ranges_count;
  const uni_char **data;
  unsigned data_count;
  report->GetError (error_index, error, ranges, ranges_count, data, data_count);

  return (ranges[range_index].start.line == start_line &&
          ranges[range_index].start.column == start_column &&
          ranges[range_index].end.line == end_line &&
          ranges[range_index].end.column == end_column);
}

BOOL
XMLSelftestValidityHelper::CheckDataCount (unsigned error_index, unsigned count)
{
  XMLInternalParser::ParseError error;
  XMLRange *ranges;
  unsigned ranges_count;
  const uni_char **data;
  unsigned data_count;
  report->GetError (error_index, error, ranges, ranges_count, data, data_count);

  return data_count == count;
}

BOOL
XMLSelftestValidityHelper::CheckDatum (unsigned error_index, unsigned datum_index, const uni_char *datum)
{
  XMLInternalParser::ParseError error;
  XMLRange *ranges;
  unsigned ranges_count;
  const uni_char **data;
  unsigned data_count;
  report->GetError (error_index, error, ranges, ranges_count, data, data_count);

  return uni_strcmp (data[datum_index], datum) == 0;
}

#endif // XML_VALIDATING
#endif // SELFTEST
