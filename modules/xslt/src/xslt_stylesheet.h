/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_STYLESHEET_H
#define XSLT_STYLESHEET_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/xslt.h"
#include "modules/xslt/src/xslt_types.h"
#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_compiler.h"
#include "modules/xslt/src/xslt_program.h"
#include "modules/xslt/src/xslt_functions.h"
#include "modules/xslt/src/xslt_nodelist.h"
#include "modules/xslt/src/xslt_namespacecollection.h"
#include "modules/xpath/xpath.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/OpHashTable.h"

class XSLT_Template;
class XMLExpandedName;
class XSLT_Key;
class XSLT_AttributeSet;
class XSLT_DecimalFormatData;
class XSLT_TemplatePatterns;
class XSLT_Program;
class XSLT_ApplyTemplatesProgram;
class XSLT_StylesheetImpl;

#ifdef XSLT_USE_NODE_PROFILES
class XPathNodeProfile;
#else // XSLT_USE_NODE_PROFILES
typedef void XPathNodeProfile;
#endif // XSLT_USE_NODE_PROFILES

static void XSLT_DeleteString (const uni_char *string)
{
  uni_char *non_const = const_cast<uni_char *> (string);
  OP_DELETEA (non_const);
}

/**
 * We have to own some strings, sometimes, so this extension keeps track of what is owned.
 */
struct XSLT_OutputSpecificationInternal
  : public XSLT_Stylesheet::OutputSpecification
{
  XSLT_OutputSpecificationInternal ()
    : indent (FALSE),
      owns_version (FALSE),
      owns_encoding (FALSE),
      owns_doctype_public_id (FALSE),
      owns_doctype_system_id (FALSE),
      owns_media_type (FALSE)
  {
  }

  ~XSLT_OutputSpecificationInternal ()
  {
    if (owns_version)
      XSLT_DeleteString (version);
    if (owns_encoding)
      XSLT_DeleteString (encoding);
    if (owns_doctype_public_id)
      XSLT_DeleteString (doctype_public_id);
    if (owns_doctype_system_id)
      XSLT_DeleteString (doctype_system_id);
    if (owns_media_type)
      XSLT_DeleteString (media_type);
  }

  BOOL indent;

  BOOL owns_version;
  BOOL owns_encoding;
  BOOL owns_doctype_public_id;
  BOOL owns_doctype_system_id;
  BOOL owns_media_type;
};

class XSLT_StylesheetElement
  : public XSLT_Element
{
private:
  XSLT_StylesheetImpl *stylesheet;
  XSLT_Import *import;
  XSLT_NamespaceCollection excluded_namespaces, extension_namespaces;

  virtual XSLT_Element *StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element);
  virtual BOOL EndElementL (XSLT_StylesheetParserImpl *parser);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);

public:
  XSLT_StylesheetElement (XSLT_StylesheetImpl *stylesheet, XSLT_Import *import)
    : stylesheet (stylesheet),
      import (import),
      excluded_namespaces (XSLT_NamespaceCollection::TYPE_EXCLUDED_RESULT_PREFIXES),
      extension_namespaces (XSLT_NamespaceCollection::TYPE_EXTENSION_ELEMENT_PREFIXES)
  {
  }

  virtual ~XSLT_StylesheetElement ();

  XSLT_StylesheetImpl *GetStylesheet () { return stylesheet; }
  XSLT_Import *GetImport () { return import; }
  XSLT_NamespaceCollection *GetExcludedNamespaces () { return &excluded_namespaces; }
  XSLT_NamespaceCollection *GetExtensionNamespaces () { return &extension_namespaces; }
};

class XSLT_StylesheetImpl
  : public XSLT_Stylesheet
{
protected:
  XSLT_StylesheetElement **elements;
  unsigned elements_count, elements_total;
  XSLT_Template **templates;
  unsigned templates_count, templates_total;
  XSLT_Key *keys;
  XSLT_AttributeSet *attribute_sets, *attribute_sets_used;
  XSLT_OutputMethod output_method;
  XSLT_DecimalFormatData *decimal_formats;
  XSLT_NamespaceCollection excluded_namespaces, extension_namespaces;
#ifdef XSLT_COPY_TO_FILE_SUPPORT
  OpString copy_to_file;
#endif // XSLT_COPY_TO_FILE_SUPPORT

  OpVector<XSLT_ApplyTemplatesProgram> apply_templates_programs;
#ifdef XSLT_USE_NODE_PROFILES
  XPathNodeProfile *text_nodeprofile;
  XPathNodeProfile *comment_nodeprofile;
#endif // XSLT_USE_NODE_PROFILES

  XSLT_Program pre_calculations_program; // Computes variable values
  XSLT_Program key_program; // Program where keys can register interesting nodes
  XSLT_Program *root_program; // Points to either of apply_templates_program and pre_calculations_program
  XSLT_Variable* last_variable_in_root; // Head of a reverse chain of variables owned by the stylesheet
  XSLT_Variable* currently_compiled_global_variable;
  BOOL compilation_complete; // If the compilation finished successfully
  BOOL compilation_started_once; // If the compilation has started once

  class WhiteSpaceControl
  {
  protected:
    unsigned index, import_precedence;
    WhiteSpaceControl *next;

  public:
    WhiteSpaceControl (XSLT_Import *import, unsigned index);
    ~WhiteSpaceControl ();

    BOOL FindMatch (unsigned &import_index, unsigned &index, float &priority, const XMLExpandedNameN &name);

    XMLCompleteName nametest;

    static void Push (WhiteSpaceControl *&existing, WhiteSpaceControl *wsc);
  } *strip_space, *preserve_space;

  unsigned whitespace_control_index;
  XMLExpandedName::HashFunctions xmlexpandedname_hash_functions;

  class NameTable
    : public OpHashTable
  {
  public:
    NameTable (OpHashFunctions *hash_functions)
      : OpHashTable (hash_functions)
    {
    }

  private:
    virtual void Delete (void *data);
  };

  NameTable strip_space_table;
  NameTable preserve_space_table;

  class CDATASectionElement
  {
  protected:
    XMLExpandedName name;
    CDATASectionElement *next;

    CDATASectionElement ()
      : next (0)
    {
    }

  public:
    ~CDATASectionElement ()
    {
      OP_DELETE (next);
    }

    BOOL Find (const XMLExpandedName &sought) { return name == sought || next && next->Find (sought); }

    static void PushL (CDATASectionElement *&existing, const XMLExpandedNameN &name);
  } *cdata_section_elements;

  class NamespaceAlias
  {
  protected:
    NamespaceAlias *next;

  public:
    NamespaceAlias ();
    ~NamespaceAlias ();

    void ProcessL (XMLCompleteName &name);

    uni_char *stylesheet_prefix, *stylesheet_uri, *result_prefix, *result_uri;

    static void Push (NamespaceAlias *&existing, NamespaceAlias *alias);
  } *namespace_aliases;

  XSLT_OutputSpecificationInternal output_specification;

  void CompileApplyTemplatesProgramL (XSLT_ApplyTemplatesProgram &apply_templates_program, XSLT_MessageHandler *messagehandler);
  void CompileRootProgramL (XSLT_StylesheetParserImpl *parser);
  void FinishL (XSLT_StylesheetParserImpl *parser);

public:
  XSLT_StylesheetImpl ();
  ~XSLT_StylesheetImpl ();

  virtual const OutputSpecification *GetOutputSpecification ();
  virtual BOOL ShouldStripWhitespaceIn (const XMLExpandedName &name);
  virtual OP_STATUS StartTransformation (Transformation *&transformation, const Input &input, OutputForm outputform);

  OP_STATUS Finish (XSLT_StylesheetParserImpl *parser);

  void AddStylesheetElementL (XSLT_StylesheetElement *element);
  void AddTemplateL (XSLT_Template *tmplate);

  XSLT_Template **GetTemplates () { return templates; }
  unsigned GetTemplatesCount () { return templates_count; }

  XSLT_Program *GetApplyTemplatesProgramL (const XMLExpandedName *mode = 0, XSLT_Import *imported_into = 0, XPathNodeProfile *nodeprofile = 0, BOOL compile = FALSE, XSLT_MessageHandler *messagehandler = 0);
#ifdef XSLT_USE_NODE_PROFILES
  XSLT_Program *GetApplyTemplatesProgramL (const XMLExpandedName *mode, XPathNode::Type nodetype, BOOL compile = FALSE, XSLT_MessageHandler *messagehandler = 0);
#endif // XSLT_USE_NODE_PROFILES
  XSLT_Program *GetRootProgram () { return root_program; }

  void AddKey (XSLT_Key *key);
  XSLT_Key *FindKey (const XMLExpandedName &name);

  XSLT_Variable *GetLastVariable () { return last_variable_in_root; }
  void SetLastVariable(XSLT_Variable* variable) { last_variable_in_root = variable; }

  void AddAttributeSet (XSLT_AttributeSet *attribute_set);
  XSLT_AttributeSet *FindAttributeSet (const XMLExpandedName &name);
  XSLT_AttributeSet *GetUsedAttributeSet () { return attribute_sets_used; }
  void SetUsedAttributeSet (XSLT_AttributeSet *attribute_set) { attribute_sets_used = attribute_set; }

  void AddVariable (XSLT_Variable *variable);

  XSLT_StylesheetVersion GetVersion ();

  void SetOutputMethod (XSLT_OutputMethod new_output_method);
  XSLT_OutputMethod GetOutputMethod () { return output_method; }

  void AddWhiteSpaceControlL (XSLT_Import *import, const XMLCompleteNameN &nametest, BOOL strip);
  BOOL StripWhiteSpace (const XMLExpandedName &name);
  BOOL HasStripSpaceElements () { return strip_space != 0; }

  void AddCDATASectionElementL (const XMLCompleteNameN &name);
  BOOL UseCDATASection (const XMLExpandedName &name);

  void AddNamespaceAliasL (XSLT_Import *import, const uni_char *stylesheet_prefix, const uni_char *stylesheet_uri, const uni_char *result_prefix, const uni_char *result_uri);
  void ProcessNamespaceAliasesL (XMLCompleteName &name);

  XSLT_DecimalFormatData *AddDecimalFormat ();
  XSLT_DecimalFormatData *FindDecimalFormat (const XMLExpandedName &name);
  XSLT_DecimalFormatData *FindDefaultDecimalFormat ();

  XSLT_NamespaceCollection *GetExcludedNamespaces () { return &excluded_namespaces; }
  XSLT_NamespaceCollection *GetExtensionNamespaces () { return &extension_namespaces; }

  void CompileTemplatesL (XSLT_StylesheetParserImpl *parser);

  XSLT_OutputSpecificationInternal &GetOutputSpecificationInternal () { return output_specification; }

#ifdef XSLT_COPY_TO_FILE_SUPPORT
  void SetCopyToFileL (const uni_char *value, unsigned value_length) { copy_to_file.SetL (value, value_length); }
  const OpString &GetCopyToFile () { return copy_to_file; }
#endif // XSLT_COPY_TO_FILE_SUPPORT

  void RenumberImportPrecedence (unsigned old_index, unsigned new_index);
};

class XSLT_VariableValue;

class XSLT_ParameterValue
  : public XSLT_Stylesheet::Input::Parameter::Value
{
public:
  XSLT_ParameterValue()
    : type (NONE)
  {
  }

  virtual OP_STATUS AddNodeToList (XPathNode *value);

  XSLT_ParameterValue *CopyL ();
  XSLT_VariableValue *MakeVariableValueL ();

  enum Type
    {
      NONE,
      STRING,
      NUMBER,
      BOOLEAN,
      NODELIST
    };

  Type type;

  OpString string;
  BOOL boolean;
  double number;
  XSLT_NodeList nodelist;
};

#endif // XSLT_SUPPORT
#endif // XSLT_STYLESHEET_H
