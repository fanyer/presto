/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_VARIABLE_H
#define XSLT_VARIABLE_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_templatecontent.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_tree.h"
#include "modules/xslt/src/xslt_nodelist.h"
#include "modules/xpath/xpath.h"
#include "modules/util/adt/opvector.h"

class XSLT_StylesheetImpl;
class XPathContext;
class XSLT_XPathExtensions;
class XSLT_Tree;
class XSLT_NodeList;

class XSLT_Variable
  : public XSLT_TemplateContent
{
protected:
  XMLCompleteName name;
  BOOL has_name;
  XSLT_String select;
  XSLT_Program *program;

public:
  XSLT_Variable ();
  ~XSLT_Variable ();

  const XMLCompleteName &GetName () const { return name; }

  XSLT_XPathExtensions* GetXPathExtensionsL (XSLT_XPathExtensions *previous_extensions);

  virtual void CompileL (XSLT_Compiler *compiler);

  XSLT_Program *CompileProgramL (XSLT_StylesheetImpl *stylesheet, XSLT_MessageHandler *messagehandler);

  virtual BOOL EndElementL (XSLT_StylesheetParserImpl *parser);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);

  /**
  * Returns the previos variable in scope. It might be a sibling or just a sibling to an ancestor.
  */
  XSLT_Variable *GetPrevious () { return extensions.GetPreviousVariable (); }

  void SetPrevious (XSLT_Variable *new_previous) { extensions.SetPreviousVariable (new_previous); }
};

class XSLT_VariableReference
  : public XPathVariable
{
private:
  XSLT_Variable* variable_elm;
  BOOL is_blocking;

public:
  XSLT_VariableReference (XSLT_Variable* variable_elm);

  virtual unsigned GetValueType ();
  virtual unsigned GetFlags ();
  virtual Result GetValue (XPathValue &value, XPathExtensions::Context *extensions_context, State *&state);
};

class XSLT_VariableValue
{
private:
  XSLT_VariableValue()
    : type (BOOLEAN),
      refcount (1)
  {
  }

  enum Type
    {
      NEEDS_CALCULATION,
      /**< Variable value needs to be calculated. */

      IS_BEING_CALCULATED,
      /**< Variable value is being calculated.  Attempts to read it means there
           is a circular dependency between some variables in the stylesheet. */

      STRING,
      NUMBER,
      BOOLEAN,
      TREE,
      NODESET
    };

  Type type;
  OpString string;

  union
  {
    double number;
    bool boolean;
    XSLT_Tree* tree;
    XSLT_NodeList* nodelist;
  } data;

  unsigned refcount;

public:
  static XSLT_VariableValue *MakeL ();
  static XSLT_VariableValue *MakeL (const uni_char *string_value);
  static XSLT_VariableValue *MakeL (XSLT_NodeList *nodelist);
  static XSLT_VariableValue *MakeL (double number_value);
  static XSLT_VariableValue *MakeL (BOOL boolean_value);
  static XSLT_VariableValue *MakeL (XSLT_Tree *tree);

  ~XSLT_VariableValue ();

  BOOL NeedsCalculation () { return type == NEEDS_CALCULATION; }
  BOOL IsBeingCalculated () { return type == IS_BEING_CALCULATED; }
  void SetIsBeingCalculated () { type = IS_BEING_CALCULATED; }

  OP_STATUS SetXPathValue (XPathValue &xpath_value);

  static XSLT_VariableValue *IncRef (XSLT_VariableValue *value) { if (value) ++value->refcount; return value; }
  static void DecRef (XSLT_VariableValue *value) { if (value && --value->refcount == 0) OP_DELETE (value); }
};


#endif // XSLT_SUPPORT
#endif // XSLT_VARIABLE_H
