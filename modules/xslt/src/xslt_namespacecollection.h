/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_EXCLUDERESULTPREFIX_H
#define XSLT_EXCLUDERESULTPREFIX_H

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_string.h"
# include "modules/util/OpHashTable.h"

class XSLT_StylesheetParserImpl;
class XSLT_Element;

class XSLT_NamespaceCollection
  : public XSLT_String
{
public:
  enum Type { TYPE_EXCLUDED_RESULT_PREFIXES, TYPE_EXTENSION_ELEMENT_PREFIXES };

private:
  Type type;
  OpAutoStringHashTable<OpString> uris;

public:
  XSLT_NamespaceCollection (Type type)
    : type (type)
  {
  }

  void FinishL (XSLT_StylesheetParserImpl *parser, XSLT_Element *element);

  BOOL ContainsURI (const uni_char *uri) { return uris.Contains (uri); }
};

#endif // XSLT_SUPPORT
#endif // XSLT_OUTPUT_H
