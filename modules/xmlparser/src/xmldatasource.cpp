/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/xmlparser/xmlcommon.h"
#include "modules/xmlparser/xmldatasource.h"
#include "modules/xmlparser/xmlbuffer.h"
#include "modules/xmlparser/xmlinternalparser.h"

XMLDataSource::XMLDataSource ()
  : buffer (0),
    next_source (0),
    parser_state (0)
{
  XML_OBJECT_CREATED (XMLDataSource);
}

/* virtual */
XMLDataSource::~XMLDataSource ()
{
  OP_DELETE(buffer);
  OP_DELETE(parser_state);

  XML_OBJECT_DESTROYED (XMLDataSource);
}

OP_STATUS
XMLDataSource::Initialize ()
{
  return OpStatus::OK;
}

/* virtual */ const uni_char *
XMLDataSource::GetName ()
{
  return UNI_L ("unknown");
}

/* virtual */ URL
XMLDataSource::GetURL ()
{
  if (next_source)
    return next_source->GetURL ();
  else
    return URL ();
}

/* virtual */ unsigned
XMLDataSource::Consume (unsigned length)
{
  return 0;
}

/* virtual */ OP_BOOLEAN
XMLDataSource::Grow ()
{
  return OpBoolean::IS_FALSE;
}

/* virtual */ BOOL
XMLDataSource::IsAtEnd ()
{
  return TRUE;
}

/* virtual */ BOOL
XMLDataSource::IsAllSeen ()
{
  return FALSE;
}

/* virtual */ BOOL
XMLDataSource::IsModified ()
{
  return FALSE;
}

#ifdef XML_ERRORS

/* virtual */ OP_BOOLEAN
XMLDataSource::Restart ()
{
  return OpBoolean::IS_FALSE;
}

#endif // XML_ERRORS

/* virtual */ BOOL
XMLDataSource::SetEncoding (const uni_char *name, unsigned name_length)
{
  return TRUE;
}

XMLStringDataSource::XMLStringDataSource (const uni_char *data, unsigned data_length)
  : data (data),
    data_length (data_length),
    consumed (0)
{
}

/* virtual */
XMLStringDataSource::~XMLStringDataSource ()
{
}

/* virtual */ const uni_char *
XMLStringDataSource::GetName ()
{
  return UNI_L ("string data");
}

/* virtual */ const uni_char *
XMLStringDataSource::GetData ()
{
  return data + consumed;
}

/* virtual */ unsigned
XMLStringDataSource::GetDataLength ()
{
  return data_length - consumed;
}

/* virtual */ unsigned
XMLStringDataSource::Consume (unsigned length)
{
  consumed += length;
  return length;
}

/* virtual */ BOOL
XMLStringDataSource::IsAllSeen ()
{
  return TRUE;
}

#ifdef XML_ERRORS

/* virtual */ OP_BOOLEAN
XMLStringDataSource::Restart ()
{
  consumed = 0;
  return OpBoolean::IS_TRUE;
}

#endif // XML_ERRORS

/* virtual */
XMLDataSourceHandler::~XMLDataSourceHandler ()
{
}

/* virtual */
XMLDoctype *
XMLDataSourceHandler::GetCachedExternalSubset (const uni_char *pubid, const uni_char *system, URL baseurl)
{
  return 0;
}
