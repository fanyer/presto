/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLPARSER_XMLSTSUPPORT_H
#define XMLPARSER_XMLSTSUPPORT_H

#ifdef SELFTEST

#include "modules/xmlparser/xmlinternalparser.h"

class XMLSelftestHelper
{
public:
  enum Mode
    {
      PARSE,    // Check for well-formedness.
      PARSE_EE, // Check for well-formedness with external entities.
      VALIDATE, // Validate.
      TOKENS    // Test that produced tokens have the correct values.
    };

  enum Expected
    {
      NOT_WELL_FORMED,
      INVALID,
      NO_ERRORS
    };

  enum EndOfData
    {
      NORMAL,     ///< All data avaiable immediately.
      NO_CONSUME, ///< The data source never consumes data when asked to (XMLDataSource::Consume returns zero.)
      LEVEL_1,    ///< One character more becomes available whenever the parsers asks for more data.
      LEVEL_2,    ///< One character more becomes available every time the parser stops because it had no more data.
      LEVEL_3,    ///< One character more becomes available when the parser asks for more data after it has stopped because it had no more data.
      MAX_TOKENS  ///< Set max_tokens_per_call to 1.
    };

  XMLSelftestHelper (const char *source, unsigned source_length, const uni_char *basename, Mode mode, Expected expected, EndOfData endofdata);
  ~XMLSelftestHelper ();

  static BOOL FromFile (const char *filename, Mode mode, Expected expected, EndOfData endofdata = NORMAL);

  BOOL Parse ();

protected:
  const char *source;
  unsigned source_length;
  const uni_char *basename;
  Mode mode;
  Expected expected;
  EndOfData endofdata;
};

#ifdef XML_VALIDATING

class XMLSelftestValidityHelper
{
public:
  XMLSelftestValidityHelper (const char *source);
  ~XMLSelftestValidityHelper ();

  BOOL Parse ();
  BOOL CheckErrorsCount (unsigned count);
  BOOL CheckError (unsigned error_index, XMLInternalParser::ParseError error);
  BOOL CheckRangesCount (unsigned error_index, unsigned count);
  BOOL CheckRange (unsigned error_index, unsigned range_index, unsigned start_line, unsigned start_column, unsigned end_line, unsigned end_column);
  BOOL CheckDataCount (unsigned error_index, unsigned count);
  BOOL CheckDatum (unsigned error_index, unsigned datum_index, const uni_char *datum);

protected:
  XMLValidityReport *report;
  const char *source;
};

#endif // XML_VALIDATING
#endif // SELFTEST
#endif // XMLPARSER_XMLSTSUPPORT_H
