/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLPARSER_XMLDATASOURCE_H
#define XMLPARSER_XMLDATASOURCE_H

#include "modules/url/url2.h"

class XMLBuffer;
class XMLInternalParserState;
class XMLDoctype;

class XMLDataSource
{
public:
  XMLDataSource ();
  virtual ~XMLDataSource ();

  void SetBuffer (XMLBuffer *new_buffer) { buffer = new_buffer; }
  XMLBuffer *GetBuffer () { return buffer; }

  void SetNextSource (XMLDataSource *source) { next_source = source; }
  XMLDataSource *GetNextSource () { return next_source; }

  void SetParserState (XMLInternalParserState *state) { parser_state = state; }
  XMLInternalParserState *GetParserState () { return parser_state; }

  virtual OP_STATUS Initialize ();
  virtual const uni_char *GetName ();
  virtual URL GetURL ();

  virtual const uni_char *GetData () = 0;
  virtual unsigned GetDataLength () = 0;

  virtual unsigned Consume (unsigned length);

  virtual OP_BOOLEAN Grow ();
  virtual BOOL IsAtEnd ();
  virtual BOOL IsAllSeen ();
  virtual BOOL IsModified ();

#ifdef XML_ERRORS
  virtual OP_BOOLEAN Restart ();
#endif // XML_ERRORS

  virtual BOOL SetEncoding (const uni_char *name, unsigned name_length);
  /**< Set the encoding of the data stream, after encountering an XML
       declaration with an encoding specified.  Should return TRUE if the
       encoding is supported and FALSE if it is not or if the change is
       somehow impossible (such as if the current encoding used is "utf-8"
       and the requested encoding is "utf-16", in which case the XML
       declaration requesting "utf-16" was not there at all and could not
       have requested it.

       The default implementation of this function simply returns TRUE and
       does nothing. */

protected:
  XMLBuffer *buffer;
  XMLDataSource *next_source;
  XMLInternalParserState *parser_state;
};

class XMLStringDataSource
  : public XMLDataSource
{
public:
  XMLStringDataSource (const uni_char *data, unsigned data_length);
  ~XMLStringDataSource ();

  virtual const uni_char *GetName ();

  virtual const uni_char *GetData ();
  virtual unsigned GetDataLength ();

  virtual unsigned Consume (unsigned length);
  virtual BOOL IsAllSeen ();

#ifdef XML_ERRORS
  virtual OP_BOOLEAN Restart ();
#endif // XML_ERRORS

protected:
  const uni_char *data;
  unsigned data_length;
  unsigned consumed;
};

class XMLDataSourceHandler
{
public:
  virtual ~XMLDataSourceHandler ();

  virtual OP_STATUS CreateInternalDataSource (XMLDataSource *&source, const uni_char *data, unsigned data_length) = 0;
  virtual OP_STATUS CreateExternalDataSource (XMLDataSource *&source, const uni_char *pubid, const uni_char *system, URL baseurl) = 0;
  virtual OP_STATUS DestroyDataSource (XMLDataSource *source) = 0;

  virtual XMLDoctype *GetCachedExternalSubset (const uni_char *pubid, const uni_char *system, URL baseurl);
};

#endif // XMLPARSER_XMLDATASOURCE_H
