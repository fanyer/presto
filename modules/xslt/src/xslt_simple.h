/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_SIMPLE_H
#define XSLT_SIMPLE_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_types.h"
#include "modules/xslt/src/xslt_string.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmllanguageparser.h"
#include "modules/xpath/xpath.h"
#include "modules/url/url2.h"

#ifdef XSLT_ERRORS
# include "modules/console/opconsoleengine.h"
#endif // XSLT_ERRORS

class XSLT_StylesheetImpl;
class XSLT_StylesheetParserImpl;
class XSLT_Element;
class XPathNamespaces;
class XPathNode;
class XPathExpression;
class XPathPattern;
class XMLParser;
class XMLTokenHandler;

class XSLT_Import
{
public:
  XSLT_Import (unsigned import_precedence, const URL &url, XSLT_ElementType type, XSLT_Import *parent);
  ~XSLT_Import ();

  XSLT_ElementType type;
  XSLT_StylesheetVersion version;
  XSLT_Import *parent;
  XSLT_Element *root_element, *current_element;

  unsigned import_precedence;
  BOOL import_allowed;
  XMLVersion xmlversion;
  XMLTokenHandler *xmltokenhandler;
  XMLLanguageParser::SourceCallback *source_callback;
  URL url;

  XSLT_Import *GetNonInclude ();
};

#ifdef XML_ERRORS

class XSLT_Location
{
public:
  enum Type
    {
      TYPE_NO_RANGE,
      TYPE_START_TAG,
      TYPE_END_TAG,
      TYPE_ATTRIBUTE,
      TYPE_PROCESSING_INSTRUCTION,
      TYPE_CHARACTER_DATA
    };

  Type type;
  URL import_url;
  XMLRange range;

  XSLT_Location ()
    : type (TYPE_NO_RANGE)
  {
  }

  XSLT_Location (XSLT_Import *import)
    : type (TYPE_NO_RANGE)
  {
    if (import)
      import_url = import->url;
  }

  XSLT_Location (Type type, XSLT_Import *import, const XMLRange &range)
    : type (type),
      range (range)
  {
    if (import)
      import_url = import->url;
  }

  XSLT_Location (const XSLT_Location &other)
    : type (other.type),
      import_url (other.import_url),
      range (other.range)
  {
  }

  void operator= (const XSLT_Location &other)
  {
    type = other.type;
    import_url = other.import_url;
    range = other.range;
  }
};

#endif // XML_ERRORS

class XSLT_XPathExpressionOrPattern
{
protected:
  XMLNamespaceDeclaration::Reference nsdeclaration;
  XSLT_String source;
  XPathExtensions *extensions;

  XSLT_XPathExpressionOrPattern (XPathExtensions *extensions, XMLNamespaceDeclaration *nsdeclaration);
  virtual ~XSLT_XPathExpressionOrPattern () {};

  XPathNamespaces *MakeNamespacesL ();

public:
  void SetSourceL (XSLT_StylesheetParserImpl *parser, const XMLCompleteNameN &attributename, const uni_char *source, unsigned source_length);
  void SetSourceL (const XSLT_String& new_source) { source.CopyL (new_source); }
  void SetNamespaceDeclaration (XSLT_StylesheetParserImpl *parser);
  void SetNamespaceDeclaration (XMLNamespaceDeclaration *new_nsdeclaration) { nsdeclaration = new_nsdeclaration; }
  const XSLT_String &GetString () const { return source; }
  const uni_char *GetSource () const { return source.GetString(); }

  BOOL IsSpecified () const { return source.IsSpecified(); }

  XMLNamespaceDeclaration *GetNamespaceDeclaration () { return nsdeclaration; }
};

class XSLT_TransformationImpl;

class XSLT_XPathExpression
  : public XSLT_XPathExpressionOrPattern
{
protected:
  XPathExpression *expression;

public:
  XSLT_XPathExpression (XPathExtensions* extensions, XMLNamespaceDeclaration *nsdeclaration);
  ~XSLT_XPathExpression ();

  XPathExpression *GetExpressionL (XSLT_MessageHandler *messagehandler);
};

class XSLT_XPathPattern
  : public XSLT_XPathExpressionOrPattern
{
protected:
  OpString *pattern_sources;
  XPathPattern **pattern_objects;
  unsigned patterns_count, patterns_disabled_count;

  void Reset ();

public:
  XSLT_XPathPattern (XPathExtensions *extensions, XMLNamespaceDeclaration *nsdeclaration);
  ~XSLT_XPathPattern ();

  void PreprocessL (XSLT_MessageHandler *messagehandler, XPathExtensions *extensions);

  const OpString &GetPatternSource (unsigned index) const { return pattern_sources[index]; }
  XPathPattern **GetPatterns () const { return pattern_objects; }
  unsigned GetPatternsCount () const { return patterns_count; }
  unsigned GetPatternsDisabledCount () const { return patterns_disabled_count; }
  void DisablePattern (unsigned index);
};

class XSLT_DecimalFormatData
{
protected:
  XSLT_DecimalFormatData *next;

public:
  XSLT_DecimalFormatData ();
  ~XSLT_DecimalFormatData ();

  void SetDefaultsL ();

  XMLExpandedName name;
  uni_char *infinity;
  uni_char *nan;

  unsigned decimal_separator;
  unsigned grouping_separator;
  unsigned minus_sign;
  unsigned percent;
  unsigned per_mille;
  unsigned zero_digit;
  unsigned digit;
  unsigned pattern_separator;

  static XSLT_DecimalFormatData *PushL (XSLT_DecimalFormatData *&existing);
  static XSLT_DecimalFormatData *Find (XSLT_DecimalFormatData *existing, const XMLExpandedName &name);
  static XSLT_DecimalFormatData *FindDefault (XSLT_DecimalFormatData *existing);
};

BOOL XSLT_CompareStrings (const uni_char *string1, unsigned string1_length, const char *string2);
unsigned XSLT_GetLength (const uni_char *string, unsigned string_length);
BOOL XSLT_GetPrefix (const uni_char *qname, const uni_char *&prefix, unsigned &prefix_length);

#ifdef XSLT_ERRORS

class XSLT_MessageHandler
{
public:
  /* Please make sure this enumeration stays compatible with the
     corresponding one in XSLT_Stylesheet::Transformation::Callback. */
  enum MessageType
    {
      MESSAGE_TYPE_ERROR,
      MESSAGE_TYPE_WARNING,
      MESSAGE_TYPE_MESSAGE
    };

  virtual OP_BOOLEAN HandleMessage (MessageType type, const uni_char *message) = 0;

protected:
  virtual ~XSLT_MessageHandler () {}
};

#ifdef XML_ERRORS
void XSLT_SignalErrorL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const char *reason, const XSLT_Location *location = 0);
void XSLT_SignalErrorL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const uni_char *reason, const XSLT_Location *location = 0);
void XSLT_SignalWarningL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const char *reason, const XSLT_Location *location = 0);
void XSLT_SignalWarningL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const uni_char *reason, const XSLT_Location *location = 0);
void XSLT_SignalMessageL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const uni_char *message, BOOL terminate, const XSLT_Location *location = 0);
#else // XML_ERRORS
void XSLT_SignalErrorL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const char *reason);
void XSLT_SignalErrorL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const uni_char *reason);
void XSLT_SignalWarningL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const char *reason);
void XSLT_SignalWarningL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const uni_char *reason);
void XSLT_SignalMessageL (XSLT_MessageHandler *messagehandler, XSLT_Element *element, const uni_char *message, BOOL terminate);
#endif // XML_ERRORS

void XSLT_SignalErrorL (XSLT_MessageHandler *messagehandler, const char *reason, const uni_char *context, URL *url);
void XSLT_SignalErrorL (XSLT_MessageHandler *messagehandler, const uni_char *reason, const uni_char *context, URL *url);

#else // XSLT_ERRORS

class XSLT_MessageHandler
{
};

#endif // XSLT_ERRORS
#endif // XSLT_SUPPORT
#endif // XSLT_SIMPLE_H
