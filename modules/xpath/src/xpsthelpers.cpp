/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef XPATH_SUPPORT
#ifdef SELFTEST

#include "modules/xpath/xpath.h"
#include "modules/xpath/src/xputils.h"

#include "modules/doc/frm_doc.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/util/tempbuf.h"

#define XP_INSIST(result, expr) do result = (expr); while (result == OpBoolean::IS_FALSE)

#define STRING_RETURN(txt) do { test_result = txt; goto test_finished; } while (0)

const char *XPath_TestNumber (FramesDocument *doc, const char *source, double expected)
{
  const char *test_result = "ok";

  TempBuffer buffer; buffer.Append (source);

  XPathNamespaces *namespaces = 0;
  XPathExpression *expression = 0;
  XPathExpression::Evaluate *evaluate = 0;
  XPathNode *node = 0;
  XMLTreeAccessor *tree = 0;
  XMLTreeAccessor::Node *treenode = 0;
  XPathExpression::ExpressionData data;

  if (!doc || !doc->GetDocRoot ())
    STRING_RETURN ("no document or no root element");

  if (doc->GetLogicalDocument ()->CreateXMLTreeAccessor (tree, treenode) != OpStatus::OK)
    STRING_RETURN ("LogicalDocument::CreateXMLTreeAccessor failed");

  if (XPathNamespaces::Make (namespaces, buffer.GetStorage ()) != OpStatus::OK)
    STRING_RETURN ("XPathNamespaces::Make failed");

  data.source = buffer.GetStorage ();
  data.namespaces = namespaces;

  if (XPathExpression::Make (expression, data) != OpStatus::OK)
    STRING_RETURN ("XPathExpression::Make failed");

  if (XPathExpression::Evaluate::Make (evaluate, expression) != OpStatus::OK)
    STRING_RETURN ("XPathExpression::Evaluate::Make failed");

  if (XPathNode::Make (node, tree, treenode) != OpStatus::OK)
    STRING_RETURN ("XPathNode::Make failed");

  evaluate->SetContext (node);

  unsigned type;

  OP_BOOLEAN result1;
  XP_INSIST (result1, evaluate->CheckResultType (type));

  if (result1 != OpBoolean::IS_TRUE)
    STRING_RETURN ("evaluate->CheckResultType failed");

  if (type != XPathExpression::Evaluate::PRIMITIVE_NUMBER)
    STRING_RETURN ("result not a number");

  double result;

  OP_BOOLEAN result2;
  XP_INSIST (result2, evaluate->GetNumberResult (result));

  if (result2 != OpBoolean::IS_TRUE)
    STRING_RETURN ("evaluate->GetNumberResult failed");

  if (!op_isnan (result) != !op_isnan (expected) || !op_isnan (result) && (result != expected || 1 / result != 1 / expected))
    {
      char *buffer = (char *) g_memory_manager->GetTempBuf ();
      op_sprintf (buffer, "wrong result; got '%f', expected '%f'", result, expected);
      test_result = buffer;
    }

 test_finished:
  XPathExpression::Evaluate::Free (evaluate);
  XPathExpression::Free (expression);
  XPathNamespaces::Free (namespaces);
  LogicalDocument::FreeXMLTreeAccessor (tree);

  return test_result;
}

const char *XPath_TestString (FramesDocument *doc, const char *source, const char *expected)
{
  const char *test_result = "ok";

  TempBuffer buffer_source; buffer_source.Append (source);
  TempBuffer buffer_expected; buffer_expected.Append (expected);

  XPathNamespaces *namespaces = 0;
  XPathExpression *expression = 0;
  XPathExpression::Evaluate *evaluate = 0;
  XPathNode *node = 0;
  XMLTreeAccessor *tree = 0;
  XMLTreeAccessor::Node *treenode = 0;
  XPathExpression::ExpressionData data;

  if (!doc || !doc->GetDocRoot ())
    STRING_RETURN ("no document or no root element");

  if (doc->GetLogicalDocument ()->CreateXMLTreeAccessor (tree, treenode) != OpStatus::OK)
    STRING_RETURN ("LogicalDocument::CreateXMLTreeAccessor failed");

  if (XPathNamespaces::Make (namespaces, buffer_source.GetStorage ()) != OpStatus::OK)
    STRING_RETURN ("XPathNamespaces::Make failed");

  data.source = buffer_source.GetStorage ();
  data.namespaces = namespaces;

  if (XPathExpression::Make (expression, data) != OpStatus::OK)
    STRING_RETURN ("XPathExpression::Make failed");

  if (XPathExpression::Evaluate::Make (evaluate, expression) != OpStatus::OK)
    STRING_RETURN ("XPathExpression::Evaluate::Make failed");

  if (XPathNode::Make (node, tree, treenode) != OpStatus::OK)
    STRING_RETURN ("XPathNode::Make failed");

  evaluate->SetContext (node);

  unsigned type;

  OP_BOOLEAN result1;
  XP_INSIST (result1, evaluate->CheckResultType (type));

  if (result1 != OpBoolean::IS_TRUE)
    STRING_RETURN ("evaluate->CheckResultType failed");

  if (type != XPathExpression::Evaluate::PRIMITIVE_STRING)
    STRING_RETURN ("result not a string");

  const uni_char *result;

  OP_BOOLEAN result2;
  XP_INSIST (result2, evaluate->GetStringResult (result));

  if (result2 != OpBoolean::IS_TRUE)
    STRING_RETURN ("evaluate->GetStringResult failed");

  if (uni_strcmp (result, buffer_expected.GetStorage ()) != 0)
    {
      TempBuffer value; value.Append (result);
      make_singlebyte_in_place ((char *) value.GetStorage ());

      char *buffer = (char *) g_memory_manager->GetTempBuf ();
      op_sprintf (buffer, "wrong result; got '%s', expected '%s'", (char *) value.GetStorage (), expected);
      test_result = buffer;
    }

 test_finished:
  XPathExpression::Evaluate::Free (evaluate);
  XPathExpression::Free (expression);
  XPathNamespaces::Free (namespaces);
  LogicalDocument::FreeXMLTreeAccessor (tree);

  return test_result;
}

const char *XPath_TestBoolean (FramesDocument *doc, const char *source, BOOL expected)
{
  const char *test_result = "ok";

  TempBuffer buffer; buffer.Append (source);

  XPathNamespaces *namespaces = 0;
  XPathExpression *expression = 0;
  XPathExpression::Evaluate *evaluate = 0;
  XPathNode *node = 0;
  XMLTreeAccessor *tree = 0;
  XMLTreeAccessor::Node *treenode = 0;
  XPathExpression::ExpressionData data;

  BOOL result;

  if (!doc || !doc->GetDocRoot ())
    STRING_RETURN ("no document or no root element");

  if (doc->GetLogicalDocument ()->CreateXMLTreeAccessor (tree, treenode) != OpStatus::OK)
    STRING_RETURN ("LogicalDocument::CreateXMLTreeAccessor failed");

  if (XPathNamespaces::Make (namespaces, buffer.GetStorage ()) != OpStatus::OK)
    STRING_RETURN ("XPathNamespaces::Make failed");

  data.source = buffer.GetStorage ();
  data.namespaces = namespaces;

  if (XPathExpression::Make (expression, data) != OpStatus::OK)
    STRING_RETURN ("XPathExpression::Make failed");

  if (XPathExpression::Evaluate::Make (evaluate, expression) != OpStatus::OK)
    STRING_RETURN ("XPathExpression::Evaluate::Make failed");

  if (XPathNode::Make (node, tree, treenode) != OpStatus::OK)
    STRING_RETURN ("XPathNode::Make failed");

  evaluate->SetContext (node);

  unsigned type;

  OP_BOOLEAN result1;
  XP_INSIST (result1, evaluate->CheckResultType (type));

  if (result1 != OpBoolean::IS_TRUE)
    STRING_RETURN ("evaluate->CheckResultType failed");

  if (type != XPathExpression::Evaluate::PRIMITIVE_BOOLEAN)
    STRING_RETURN ("result not a boolean");

  OP_BOOLEAN result2;
  XP_INSIST (result2, evaluate->GetBooleanResult (result));

  if (result2 != OpBoolean::IS_TRUE)
    STRING_RETURN ("evaluate->GetBooleanResult failed");

  if (!result != !expected)
    {
      char *buffer = (char *) g_memory_manager->GetTempBuf ();
      op_sprintf (buffer, "wrong result; got '%s', expected '%s'", result ? "true" : "false", expected ? "true" : "false");
      test_result = buffer;
    }

 test_finished:
  XPathExpression::Evaluate::Free (evaluate);
  XPathExpression::Free (expression);
  XPathNamespaces::Free (namespaces);
  LogicalDocument::FreeXMLTreeAccessor (tree);

  return test_result;
}

#ifdef XPATH_EXTENSION_SUPPORT

class XPath_TestVariable
{
public:
  enum Type
    {
      TYPE_INVALID,
      TYPE_NUMBER,
      TYPE_BOOLEAN,
      TYPE_STRING,
      TYPE_NODESET_ORDERED,
      TYPE_NODESET_UNORDERED,
      TYPE_NODESET_ORDERED_DUPLICATES,
      TYPE_NODESET_UNORDERED_DUPLICATES
    };

  XPath_TestVariable ()
    : type (TYPE_INVALID),
      nodes (0),
      nodes_count (0)
  {
  }

  ~XPath_TestVariable ()
  {
    for (unsigned index = 0; index < nodes_count; ++index)
      XPathNode::Free (nodes[index]);
    OP_DELETEA (nodes);
  }

  unsigned GetValueType ();
  XPathVariable::Result GetValue (XPathValue &value, XPathExtensions::Context *extensions_context, XPathVariable::State *&state);

  Type type;
  double number;
  BOOL boolean;
  OpString string;
  XPathNode **nodes;
  unsigned nodes_count;
};

class XPath_TestVariableWrapper
  : public XPathVariable
{
private:
  XPath_TestVariable *actual;

public:
  XPath_TestVariableWrapper (XPath_TestVariable *actual)
    : actual (actual)
  {
  }

  virtual unsigned GetValueType () { return actual->GetValueType (); }
  virtual Result GetValue (XPathValue &value, XPathExtensions::Context *extensions_context, State *&state) { return actual->GetValue (value, extensions_context, state); }
};

class XPath_TestExtensions
  : public XPathExtensions
{
public:
  virtual OP_STATUS GetFunction (XPathFunction *&function, const XMLExpandedName &name);
  virtual OP_STATUS GetVariable (XPathVariable *&variable, const XMLExpandedName &name);

  XPath_TestVariable testv;
};

/* virtual */ OP_STATUS
XPath_TestExtensions::GetFunction (XPathFunction *&function, const XMLExpandedName &name)
{
  return OpStatus::ERR;
}

/* virtual */ OP_STATUS
XPath_TestExtensions::GetVariable (XPathVariable *&variable, const XMLExpandedName &name)
{
  if (name == XMLExpandedName (UNI_L ("testv")))
    {
      variable = OP_NEW (XPath_TestVariableWrapper, (&testv));
      return variable ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
    }
  else
    return OpStatus::ERR;
}

/* virtual */ unsigned
XPath_TestVariable::GetValueType ()
{
  switch (type)
    {
    case TYPE_NUMBER:
      return XPathVariable::TYPE_NUMBER;

    case TYPE_BOOLEAN:
      return XPathVariable::TYPE_BOOLEAN;

    case TYPE_STRING:
      return XPathVariable::TYPE_STRING;

    case TYPE_NODESET_ORDERED:
    case TYPE_NODESET_UNORDERED:
    case TYPE_NODESET_ORDERED_DUPLICATES:
    case TYPE_NODESET_UNORDERED_DUPLICATES:
      return XPathVariable::TYPE_NODESET;

    default:
      return XPathVariable::TYPE_ANY;
    }
}

/* virtual */ XPathVariable::Result
XPath_TestVariable::GetValue (XPathValue &value, XPathExtensions::Context *extensions_context, XPathVariable::State *&state)
{
#define RETURN_IF_ERRORX(status) do { if (OpStatus::IsMemoryError (status)) return XPathVariable::RESULT_OOM; } while (0)
  switch (type)
    {
    case TYPE_NUMBER:
      value.SetNumber (number);
      break;

    case TYPE_BOOLEAN:
      value.SetBoolean (boolean);
      break;

    case TYPE_STRING:
      RETURN_IF_ERRORX (value.SetString (string.CStr ()));
      break;

    case TYPE_NODESET_ORDERED:
    case TYPE_NODESET_UNORDERED:
    case TYPE_NODESET_ORDERED_DUPLICATES:
    case TYPE_NODESET_UNORDERED_DUPLICATES:
      RETURN_IF_ERRORX (value.SetNodeSet (type == TYPE_NODESET_ORDERED || type == TYPE_NODESET_ORDERED_DUPLICATES, type == TYPE_NODESET_ORDERED_DUPLICATES || type == TYPE_NODESET_UNORDERED_DUPLICATES));

      XPathValue::AddNodeStatus status;
      for (unsigned index = 0; index < nodes_count; ++index)
        {
          XPathNode *copy;
          RETURN_IF_ERRORX (XPathNode::MakeCopy (copy, nodes[index]));
          RETURN_IF_ERRORX (value.AddNode (copy, status));
        }
    }
#undef RETURN_IF_ERRORX

  return XPathVariable::RESULT_FINISHED;
}

const char *XPath_TestBooleanWithParam (FramesDocument *doc, const char *source, BOOL expected, const char *param)
{
  const char *test_result = "ok";

  XPath_TestExtensions extensions;

  TempBuffer buffer; buffer.Append (param);

  XPathNamespaces *namespaces = 0;
  XPathExpression *expression = 0;
  XPathExpression::Evaluate *evaluate = 0;
  XPathNode *node = 0;
  XMLTreeAccessor *tree = 0;
  XMLTreeAccessor::Node *treenode = 0;
  XPathExpression::ExpressionData data;

  BOOL result;

  if (!doc || !doc->GetDocRoot ())
    STRING_RETURN ("no document or no root element");

  if (doc->GetLogicalDocument ()->CreateXMLTreeAccessor (tree, treenode) != OpStatus::OK)
    STRING_RETURN ("LogicalDocument::CreateXMLTreeAccessor failed");

  if (XPathNamespaces::Make (namespaces, buffer.GetStorage ()) != OpStatus::OK)
    STRING_RETURN ("XPathNamespaces::Make failed");

  data.source = buffer.GetStorage ();
  data.namespaces = namespaces;
  data.extensions = &extensions;

  if (XPathExpression::Make (expression, data) != OpStatus::OK)
    STRING_RETURN ("XPathExpression::Make failed");

  if (XPathExpression::Evaluate::Make (evaluate, expression) != OpStatus::OK)
    STRING_RETURN ("XPathExpression::Evaluate::Make failed");

  if (XPathNode::Make (node, tree, treenode) != OpStatus::OK)
    STRING_RETURN ("XPathNode::Make failed");

  evaluate->SetContext (node);
  evaluate->SetRequestedType (XPathExpression::Evaluate::PRIMITIVE_NUMBER, XPathExpression::Evaluate::PRIMITIVE_BOOLEAN, XPathExpression::Evaluate::PRIMITIVE_STRING, XPathExpression::Evaluate::NODESET_SNAPSHOT | XPathExpression::Evaluate::NODESET_FLAG_ORDERED);

  unsigned type;

  OP_BOOLEAN result1;
  XP_INSIST (result1, evaluate->CheckResultType (type));

  if (result1 != OpBoolean::IS_TRUE)
    STRING_RETURN ("evaluate->CheckResultType failed");

  const uni_char *string;
  OP_BOOLEAN result2;

  switch (type)
    {
    case XPathExpression::Evaluate::PRIMITIVE_NUMBER:
      extensions.testv.type = XPath_TestVariable::TYPE_NUMBER;
      XP_INSIST (result2, evaluate->GetNumberResult (extensions.testv.number));
      if (result2 != OpBoolean::IS_TRUE)
        STRING_RETURN ("evaluate->GetNumberResult failed");
      break;

    case XPathExpression::Evaluate::PRIMITIVE_BOOLEAN:
      extensions.testv.type = XPath_TestVariable::TYPE_BOOLEAN;
      XP_INSIST (result2, evaluate->GetBooleanResult (extensions.testv.boolean));
      if (result2 != OpBoolean::IS_TRUE)
        STRING_RETURN ("evaluate->GetBooleanResult failed");
      break;

    case XPathExpression::Evaluate::PRIMITIVE_STRING:
      extensions.testv.type = XPath_TestVariable::TYPE_STRING;
      XP_INSIST (result2, evaluate->GetStringResult (string));
      if (result2 != OpBoolean::IS_TRUE)
        STRING_RETURN ("evaluate->GetStringResult failed");
      if (extensions.testv.string.Set (string) != OpStatus::OK)
        STRING_RETURN ("OpString::Set failed");
      break;

    case XPathExpression::Evaluate::NODESET_SNAPSHOT | XPathExpression::Evaluate::NODESET_FLAG_ORDERED:
      extensions.testv.type = XPath_TestVariable::TYPE_NODESET_ORDERED;
      XP_INSIST (result2, evaluate->GetNodesCount (extensions.testv.nodes_count));
      if (result2 != OpBoolean::IS_TRUE)
        STRING_RETURN ("evaluate->GetNodesCount failed");
      extensions.testv.nodes = OP_NEWA (XPathNode *, extensions.testv.nodes_count);
      if (!extensions.testv.nodes)
        STRING_RETURN ("allocation of 'XPathNode *' failed");
      op_memset (extensions.testv.nodes, 0, extensions.testv.nodes_count * sizeof *extensions.testv.nodes);
      for (unsigned index = 0; index < extensions.testv.nodes_count; ++index)
        {
          XPathNode *node;
          if (evaluate->GetNode (node, index) != OpStatus::OK)
            STRING_RETURN ("evaluate->GetNode failed");
          if (XPathNode::MakeCopy (extensions.testv.nodes[index], node) != OpStatus::OK)
            STRING_RETURN ("XPathNode::MakeCopy failed");
        }
      break;
    }

  XPathExpression::Evaluate::Free (evaluate); evaluate = 0;
  XPathExpression::Free (expression); expression = 0;
  XPathNamespaces::Free (namespaces); namespaces = 0;

  buffer.Clear ();
  buffer.Append (source);

  if (XPathNamespaces::Make (namespaces, buffer.GetStorage ()) != OpStatus::OK)
    STRING_RETURN ("XPathNamespaces::Make failed");

  data.source = buffer.GetStorage ();
  data.namespaces = namespaces;
  data.extensions = &extensions;

  if (XPathExpression::Make (expression, data) != OpStatus::OK)
    STRING_RETURN ("XPathExpression::Make failed");

  if (XPathExpression::Evaluate::Make (evaluate, expression) != OpStatus::OK)
    STRING_RETURN ("XPathExpression::Evaluate::Make failed");

  if (XPathNode::Make (node, tree, treenode) != OpStatus::OK)
    STRING_RETURN ("XPathNode::Make failed");

  evaluate->SetContext (node);

  OP_BOOLEAN result3;
  XP_INSIST (result3, evaluate->CheckResultType (type));

  if (result3 != OpBoolean::IS_TRUE)
    STRING_RETURN ("evaluate->CheckResultType failed");

  if (type != XPathExpression::Evaluate::PRIMITIVE_BOOLEAN)
    STRING_RETURN ("result not a boolean");

  OP_BOOLEAN result4;
  XP_INSIST (result4, evaluate->GetBooleanResult (result));

  if (result4 != OpBoolean::IS_TRUE)
    STRING_RETURN ("evaluate->GetBooleanResult failed");

  if (!result != !expected)
    {
      char *buffer = (char *) g_memory_manager->GetTempBuf ();
      op_sprintf (buffer, "wrong result; got '%s', expected '%s'", result ? "true" : "false", expected ? "true" : "false");
      test_result = buffer;
    }

 test_finished:
  XPathExpression::Evaluate::Free (evaluate);
  XPathExpression::Free (expression);
  XPathNamespaces::Free (namespaces);
  LogicalDocument::FreeXMLTreeAccessor (tree);

  return test_result;
}

#endif // XPATH_EXTENSION_SUPPORT

HTML_Element *XPath_FindElementByIndex (FramesDocument *doc, int index)
{
  HTML_Element *element = doc->GetDocRoot ();

  while (element)
    {
      if (element->Type() != HE_DOCTYPE)
        if (index == 0)
          break;
        else
          --index;
      element = element->NextActual();
    }

  return element;
}

const char *XPath_TestNodeSet (FramesDocument *doc, const char *source, XPathNode *contextNode, int *expectedIndeces, BOOL ordered)
{
  const char *test_result = "ok";

  TempBuffer buffer; buffer.Append (source);

  XPathNamespaces *namespaces = 0;
  XPathExpression *expression = 0;
  XPathExpression::Evaluate *evaluate = 0;
  XMLTreeAccessor *tree = contextNode->GetTreeAccessor ();
  XPathExpression::ExpressionData data;

  if (!doc || !doc->GetDocRoot ())
    STRING_RETURN ("no document or no root element");

  if (XPathNamespaces::Make (namespaces, buffer.GetStorage ()) != OpStatus::OK)
    STRING_RETURN ("XPathNamespaces::Make failed");

  data.source = buffer.GetStorage ();
  data.namespaces = namespaces;

  if (XPathExpression::Make (expression, data) != OpStatus::OK)
    STRING_RETURN ("XPathExpression::Make failed");

  if (XPathExpression::Evaluate::Make (evaluate, expression) != OpStatus::OK)
    STRING_RETURN ("XPathExpression::Evaluate::Make failed");

  unsigned requestedType;

  requestedType = ordered ? XPathExpression::Evaluate::NODESET_ITERATOR | XPathExpression::Evaluate::NODESET_FLAG_ORDERED : XPathExpression::Evaluate::NODESET_ITERATOR;

  evaluate->SetContext (contextNode);
  evaluate->SetRequestedType (requestedType);

  unsigned type;
  unsigned index;

  OP_BOOLEAN result1;
  XP_INSIST (result1, evaluate->CheckResultType (type));

  if (result1 != OpBoolean::IS_TRUE)
    STRING_RETURN ("evaluate->CheckResultType failed");

  if (type != requestedType)
    STRING_RETURN ("result not of the requested type");

  XPathNode *found;

  for (index = 0; expectedIndeces[index] != -1;)
    {
      OP_BOOLEAN result2;
      XP_INSIST (result2, evaluate->GetNextNode (found));

      if (result2 != OpBoolean::IS_TRUE)
        STRING_RETURN ("evaluate->GetNextNode failed");

      if (!found)
        STRING_RETURN ("too few nodes in result");

      XPathNode::Type foundType = found->GetType ();
      HTML_Element *foundElement = LogicalDocument::GetNodeAsElement (tree, found->GetNode ());
      BOOL same = FALSE;

      if (expectedIndeces[index] == -2)
        same = XPathNode::IsSameNode (contextNode, found);

      if (foundType == XPathNode::PI_NODE && uni_str_eq (foundElement->GetStringAttr (ATTR_TARGET), "xml"))
        continue;

      if (!same && expectedIndeces[index] >= 0)
        {
          HTML_Element *expectedElement = XPath_FindElementByIndex (doc, expectedIndeces[index]);

          if (!expectedElement)
            STRING_RETURN ("expected node not found");

          if (foundType == XPathNode::ATTRIBUTE_NODE || foundType == XPathNode::NAMESPACE_NODE || expectedElement != foundElement)
            STRING_RETURN ("result node was not expected");
        }

       ++index;
    }

  OP_BOOLEAN result2;
  XP_INSIST (result2, evaluate->GetNextNode (found));

  if (result2 != OpBoolean::IS_TRUE)
    STRING_RETURN ("result->GetNextNode failed");

  if (found)
    STRING_RETURN ("too many nodes in result");

 test_finished:
  XPathExpression::Evaluate::Free (evaluate);
  XPathExpression::Free (expression);
  XPathNamespaces::Free (namespaces);

  return test_result;
}

const char *XPath_TestNodeSet (FramesDocument *doc, const char *expression, int contextNodeIndex, int *expectedIndeces, BOOL ordered)
{
  const char *test_result = "ok";

  HTML_Element *contextElement = XPath_FindElementByIndex (doc, contextNodeIndex);

  XMLTreeAccessor *tree = 0;
  XMLTreeAccessor::Node *treenode = 0;

  if (!contextElement)
    STRING_RETURN ("contextNode not found");

  if (doc->GetLogicalDocument ()->CreateXMLTreeAccessor (tree, treenode, contextElement) != OpStatus::OK)
    STRING_RETURN ("LogicalDocument::CreateXMLTreeAccessor failed");

  XPathNode *contextNode;

  if (XPathNode::Make (contextNode, tree, treenode) != OpStatus::OK)
    STRING_RETURN ("XPathNode::Make failed");

  test_result = XPath_TestNodeSet (doc, expression, contextNode, expectedIndeces, ordered);

 test_finished:
  LogicalDocument::FreeXMLTreeAccessor (tree);
  return test_result;
}

const char *XPath_TestNodeSetAttribute (FramesDocument *doc, const char *expression, int contextNodeIndex, const char *localpart, const char *uri, int *expectedIndeces, BOOL ordered)
{
  const char *test_result = "ok";

  HTML_Element *contextElement = XPath_FindElementByIndex (doc, contextNodeIndex);

  XMLTreeAccessor *tree = 0;
  XMLTreeAccessor::Node *treenode = 0;

  TempBuffer buffer_localpart; buffer_localpart.Append (localpart);
  TempBuffer buffer_uri; if (uri) buffer_uri.Append (uri);

  if (!contextElement)
    STRING_RETURN ("contextNode not found");

  if (doc->GetLogicalDocument ()->CreateXMLTreeAccessor (tree, treenode, contextElement) != OpStatus::OK)
    STRING_RETURN ("LogicalDocument::CreateXMLTreeAccessor failed");

  XPathNode *contextNode;

  if (XPathNode::MakeAttribute (contextNode, tree, treenode, XMLExpandedName (buffer_uri.GetStorage (), buffer_localpart.GetStorage ())) != OpStatus::OK)
    STRING_RETURN ("XPathNode::MakeAttribute failed");

  test_result = XPath_TestNodeSet (doc, expression, contextNode, expectedIndeces, ordered);

 test_finished:
  LogicalDocument::FreeXMLTreeAccessor (tree);
  return test_result;
}

const char *XPath_TestNodeSetNamespace (FramesDocument *doc, const char *expression, int contextNodeIndex, const char *prefix, const char *uri, int *expectedIndeces, BOOL ordered)
{
  const char *test_result = "ok";

  HTML_Element *contextElement = XPath_FindElementByIndex (doc, contextNodeIndex);

  TempBuffer buffer_prefix; buffer_prefix.Append (prefix);
  TempBuffer buffer_uri; buffer_uri.Append (uri);

  XMLTreeAccessor *tree = 0;
  XMLTreeAccessor::Node *treenode = 0;

  if (!contextElement)
    STRING_RETURN ("contextNode not found");

  if (doc->GetLogicalDocument ()->CreateXMLTreeAccessor (tree, treenode, contextElement) != OpStatus::OK)
    STRING_RETURN ("LogicalDocument::CreateXMLTreeAccessor failed");

  XPathNode *contextNode;

  if (XPathNode::MakeNamespace (contextNode, tree, treenode, buffer_prefix.GetStorage (), buffer_uri.GetStorage ()) != OpStatus::OK)
    STRING_RETURN ("XPathNode::MakeNamespace failed");

  test_result = XPath_TestNodeSet (doc, expression, contextNode, expectedIndeces, ordered);

 test_finished:
  LogicalDocument::FreeXMLTreeAccessor (tree);
  return test_result;
}

#ifdef XPATH_PATTERN_SUPPORT

const char *XPath_TestPattern (FramesDocument *doc, const char *patternsrc, const char *nodessrc, BOOL match_expected)
{
  const char *test_result = "ok";

  TempBuffer buffer;

  XPathExpression::ExpressionData expression_data;
  XPathPattern::PatternData pattern_data;

  XPathNamespaces *namespaces = 0;
  XPathPattern *pattern = 0;
  XPathExpression *expression = 0;
  XPathExpression::Evaluate *evaluate = 0;
  XPathNode *node = 0;
  XMLTreeAccessor *tree = 0;
  XMLTreeAccessor::Node *treenode = 0;
  XPathPatternContext *pattern_context = 0;

  buffer.Append (patternsrc);

  if (XPathNamespaces::Make (namespaces, buffer.GetStorage ()) != OpStatus::OK)
    STRING_RETURN ("XPathNamespaces::Make failed");

  pattern_data.source = buffer.GetStorage ();
  pattern_data.namespaces = namespaces;

  if (XPathPattern::Make (pattern, pattern_data))
    STRING_RETURN ("XPathPattern::Make failed");

  XPathNamespaces::Free (namespaces); namespaces = 0;

  buffer.Clear ();
  buffer.Append (nodessrc);

  if (!doc || !doc->GetDocRoot ())
    STRING_RETURN ("no document or no root element");

  if (doc->GetLogicalDocument ()->CreateXMLTreeAccessor (tree, treenode) != OpStatus::OK)
    STRING_RETURN ("LogicalDocument::CreateXMLTreeAccessor failed");

  if (XPathNamespaces::Make (namespaces, buffer.GetStorage ()) != OpStatus::OK)
    STRING_RETURN ("XPathNamespaces::Make failed");

  expression_data.source = buffer.GetStorage ();
  expression_data.namespaces = namespaces;

  if (XPathExpression::Make (expression, expression_data) != OpStatus::OK)
    STRING_RETURN ("XPathExpression::Make failed");

  XPathNamespaces::Free (namespaces); namespaces = 0;

  if (XPathExpression::Evaluate::Make (evaluate, expression) != OpStatus::OK)
    STRING_RETURN ("XPathExpression::Evaluate::Make failed");

  if (XPathNode::Make (node, tree, treenode) != OpStatus::OK)
    STRING_RETURN ("XPathNode::Make failed");

  evaluate->SetContext (node);

  unsigned type;

  OP_BOOLEAN result1;
  XP_INSIST (result1, evaluate->CheckResultType (type));

  if (result1 != OpBoolean::IS_TRUE)
    STRING_RETURN ("evaluate->CheckResultType failed");

  if (type != XPathExpression::Evaluate::NODESET_ITERATOR)
    STRING_RETURN ("result not a node-set");

  OP_BOOLEAN result2;
  XP_INSIST (result2, evaluate->GetNextNode (node));

  if (result2 != OpBoolean::IS_TRUE)
    STRING_RETURN ("evaluate->GetNextNode failed");

  if (!node)
    STRING_RETURN ("empty node-set to match against");

  if (XPathPatternContext::Make (pattern_context) != OpStatus::OK)
    STRING_RETURN ("XPathPatternContext::Make failed");

  do
    {
      if (XPathNode::MakeCopy (node, node) != OpStatus::OK)
        STRING_RETURN ("XPathNode::MakeCopy failed");

      XPathPattern::Match *match;

      if (XPathPattern::Match::Make (match, pattern_context, node, &pattern, 1) != OpStatus::OK)
        STRING_RETURN ("XPathPattern::Match::Make failed");

      XPathPattern *matched;

      OP_BOOLEAN result3;
      XP_INSIST (result3, match->GetMatchedPattern (matched));

      if (result3 != OpBoolean::IS_TRUE)
        STRING_RETURN ("match->GetMatchedPattern failed");

      XPathPattern::Match::Free (match);

      if (!(matched == pattern) != !match_expected)
        STRING_RETURN ("unexpected match result");

      OP_BOOLEAN result4;
      XP_INSIST (result4, evaluate->GetNextNode (node));

      if (result4 != OpBoolean::IS_TRUE)
        STRING_RETURN ("evaluate->GetNextNode failed");
    }
  while (node);

 test_finished:
  XPathPatternContext::Free (pattern_context);
  XPathExpression::Evaluate::Free (evaluate);
  XPathExpression::Free (expression);
  XPathPattern::Free (pattern);
  XPathNamespaces::Free (namespaces);
  LogicalDocument::FreeXMLTreeAccessor (tree);

  return test_result;
}

#ifdef XPATH_NODE_PROFILE_SUPPORT

const char *XPath_TestProfileVerdict (const char *expressionsrc, const char *patternsrc, XPathPattern::ProfileVerdict expected)
{
  TempBuffer expressionsrc_buffer; expressionsrc_buffer.Append (expressionsrc);
  TempBuffer patternsrc_buffer; patternsrc_buffer.Append (patternsrc);

  XPathExpression::ExpressionData expressiondata;
  XPathPattern::PatternData patterndata;

  XPathNamespaces *namespaces = 0;
  XPathExpression *expression = 0;
  XPathNodeProfile *nodeprofile = 0;
  XPathPattern *pattern = 0;
  XPathPattern::ProfileVerdict verdict;

  const char *result = "ok";

#define RETURN(s) do { result = s; goto return_now; } while (0)
#define CHECK(e) do { OP_STATUS s = (e); if (OpStatus::IsMemoryError (s)) RETURN ("OOM"); else if (OpStatus::IsError (s)) RETURN ("FAILED: " #e); } while (0)

  CHECK (XPathNamespaces::Make (namespaces));
  CHECK (namespaces->Add (UNI_L ("prefix1"), UNI_L ("uri1")));
  CHECK (namespaces->Add (UNI_L ("prefix2"), UNI_L ("uri2")));

  expressiondata.source = expressionsrc_buffer.GetStorage ();
  expressiondata.namespaces = namespaces;

  CHECK (XPathExpression::Make (expression, expressiondata));
  CHECK (expression->GetNodeProfile (nodeprofile));

  if (nodeprofile)
    {
      patterndata.source = patternsrc_buffer.GetStorage ();
      patterndata.namespaces = namespaces;

      CHECK (XPathPattern::Make (pattern, patterndata));

      verdict = pattern->GetProfileVerdict (nodeprofile);
    }
  else
    verdict = XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES;

  if (verdict != expected)
    switch (verdict)
      {
      case XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES:
        if (expected == XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES)
          RETURN ("result was 'will match', expected 'can match'");
        else
          RETURN ("result was 'will match', expected 'does not match'");
      case XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES:
        if (expected == XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES)
          RETURN ("result was 'can match', expected 'will match'");
        else
          RETURN ("result was 'can match', expected 'does not match'");
      case XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES:
        if (expected == XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES)
          RETURN ("result was 'does not match', expected 'will match'");
        else
          RETURN ("result was 'does not match', expected 'can match'");
      }

#undef RETURN
#undef CHECK

 return_now:
  XPathPattern::Free (pattern);
  XPathExpression::Free (expression);
  XPathNamespaces::Free (namespaces);
  return result;
}

#endif // XPATH_NODE_PROFILE_SUPPORT
#endif // XPATH_PATTERN_SUPPORT

double XPath_Zero ()
{
  return 0.;
}

#endif // SELFTEST
#endif // XPATH_SUPPORT
