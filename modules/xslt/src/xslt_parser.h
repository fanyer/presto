/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_PARSER_H
#define XSLT_PARSER_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/xslt.h"
#include "modules/xslt/src/xslt_types.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_xpathextensions.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlparser.h"

#include "modules/url/url2.h"

class XSLT_StylesheetParserImpl;
class XSLT_Template;
class XSLT_TemplateContent;
class XSLT_Import;
class XSLT_Sort;
class XSLT_Key;
class XSLT_Variable;
//class XPathContext;
class XMLParser;
class XSLT_XPathExtensions;

class XSLT_Element
{
protected:
  XSLT_ElementType type;
  XSLT_Element *parent;
  XMLNamespaceDeclaration::Reference nsdeclaration;
  URL baseurl;
  BOOL is_inserted;

#ifdef XSLT_ERRORS
  XMLCompleteName name;

#ifdef XML_ERRORS
  XSLT_Location location;
#endif // XML_ERRORS
#endif // XSLT_ERRORS

  XSLT_XPathExtensions extensions;

public:
  XSLT_Element ();
  virtual ~XSLT_Element ();

  void SetType (XSLT_ElementType new_type) { type = new_type; }
  XSLT_ElementType GetType () const { return type; }

  void SetParent (XSLT_Element *new_parent) { parent = new_parent; }
  XSLT_Element *GetParent () { return parent; }
  const XSLT_Element *GetParent () const { return parent; }

  void SetBaseURL (URL url) { baseurl = url; }
  URL GetBaseURL () { return baseurl; }

  void SetNamespaceDeclaration (XMLNamespaceDeclaration *new_nsdeclaration) { nsdeclaration = new_nsdeclaration; }

  XMLVersion GetXMLVersion ();
  XMLNamespaceDeclaration *GetNamespaceDeclaration () { return nsdeclaration; }
  XSLT_XPathExtensions* GetXPathExtensions() { return &extensions; }

  void SetIsInserted () { is_inserted = TRUE; }
  BOOL GetIsInserted () { return is_inserted; }

#ifdef XSLT_ERRORS
  void SetNameL (const XMLCompleteNameN &name0) { name.SetL (name0); }
  const XMLCompleteName &GetName () { return name; }
#endif // XSLT_ERRORS

#if defined XSLT_ERRORS && defined XML_ERRORS
  void SetLocation (const XSLT_Location &new_location) { location = new_location; }
#endif // XSLT_ERRORS && XML_ERRORS

  void SignalErrorL (XSLT_MessageHandler *messagehandler, const char *reason);
  void SignalErrorL (XSLT_MessageHandler *messagehandler, const uni_char *reason);
#ifdef XSLT_ERRORS
  void SignalMessageL (XSLT_MessageHandler *messagehandler, const uni_char *message, BOOL terminate);
#endif // XSLT_ERRORS

  BOOL IsExcludedNamespace (const uni_char *uri);
  BOOL IsExtensionNamespace (const uni_char *uri);

  virtual XSLT_Element *StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element) = 0;
  virtual BOOL EndElementL (XSLT_StylesheetParserImpl *parser) = 0;
  virtual void AddCharacterDataL (XSLT_StylesheetParserImpl *parser, XSLT_StylesheetParser::CharacterDataType type, const uni_char *value, unsigned value_length);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);

  virtual BOOL IsTemplateContent () { return FALSE; }
};

class XSLT_StylesheetParserImpl
  : public XSLT_StylesheetParser,
    public XSLT_MessageHandler
{
protected:
  XSLT_StylesheetParser::Callback *callback;
  XSLT_StylesheetImpl *stylesheet;

  BOOL current_stylesheet, ignored_element, parsing_failed;
  XSLT_Element *current_element;

  XSLT_Import *base_import, *current_import, *next_import;
  unsigned import_index;

#ifdef XSLT_ERRORS
  XMLCompleteName current_elementname;

#ifdef XML_ERRORS
  XSLT_Location::Type current_location_type;
#endif // XML_ERRORS

  virtual OP_BOOLEAN HandleMessage (XSLT_MessageHandler::MessageType type, const uni_char *message);
#endif // XSLT_ERRORS

  TempBuffer character_data;

  unsigned depth;

  void StartEntityL (const URL &url, const XMLDocumentInformation &docinfo, BOOL entity_reference);
  void StartElementL (const XMLCompleteNameN &completename, BOOL fragment_start, BOOL &ignore_element);
  void AddAttributeL (const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length, BOOL specified, BOOL id);
  void StartContentL ();
  void AddCharacterDataL (CharacterDataType type, const uni_char *value, unsigned value_length);
  void FlushCharacterDataL ();
  void AddProcessingInstructionL (const uni_char *target, unsigned target_length, const uni_char *data, unsigned data_length);
  void EndElementL (BOOL &block, BOOL &finished);
  void EndEntityL ();
  void XMLErrorL ();

public:
  XSLT_StylesheetParserImpl (XSLT_StylesheetParser::Callback *callback);
  virtual ~XSLT_StylesheetParserImpl ();

  OP_STATUS Construct ();

  virtual OP_STATUS StartEntity (const URL &url, const XMLDocumentInformation &docinfo, BOOL entity_reference);
  virtual OP_STATUS StartElement (const XMLCompleteNameN &completename, BOOL fragment_start, BOOL &ignore_element);
  virtual OP_STATUS AddAttribute (const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length, BOOL specified, BOOL id);
  virtual OP_STATUS StartContent ();
  virtual OP_STATUS AddCharacterData (CharacterDataType type, const uni_char *value, unsigned value_length);
  virtual OP_STATUS AddProcessingInstruction (const uni_char *target, unsigned target_length, const uni_char *data, unsigned data_length);
  virtual OP_STATUS EndElement (BOOL &block, BOOL &finished);
  virtual void SetSourceCallback(XMLLanguageParser::SourceCallback *source_callback);
  virtual OP_STATUS EndEntity ();
  virtual OP_STATUS XMLError ();
  virtual OP_STATUS GetStylesheet (XSLT_Stylesheet *&stylesheet);

  XSLT_StylesheetImpl *GetStylesheet () { return stylesheet; }

  const uni_char *GetNamespaceURI (const uni_char *prefix);
  XMLNamespaceDeclaration *GetNamespaceDeclaration ();

  void AddVariableL (XSLT_Element *scope, XSLT_Variable *variable);
  void EndVariableScopeL (XSLT_Element *scope);

  void SetQNameAttributeL (const uni_char *value, unsigned value_length, BOOL use_default, XMLExpandedName *expandedname, XMLCompleteName *completename);

  void HandleImportOrIncludeL (const uni_char *href, XSLT_ElementType type);

  XMLVersion GetCurrentXMLVersion ();
  XMLWhitespaceHandling GetCurrentWhitespaceHandling();

  XSLT_Import *GetCurrentImport () { return current_import; }

  void AddWhiteSpaceControlL (const uni_char *elements, unsigned elements_length, BOOL strip);
  void AddCDATASectionElementsL (const uni_char *elements, unsigned elements_length);
  void AddNamespaceAliasL (const uni_char *stylesheet_prefix, const uni_char *result_prefix);

  void SetStringL (XSLT_String &string, const XMLCompleteNameN &attributename, const uni_char *value, unsigned value_length);

  void SignalErrorL (const char *reason);

#if defined XSLT_ERRORS && defined XML_ERRORS
  XSLT_Location GetCurrentLocation ();
#endif // XSLT_ERRORS && XML_ERRORS

  unsigned AllocateNewImportIndex ();
};

#endif // XSLT_SUPPORT
#endif // XSLT_PARSER_H
