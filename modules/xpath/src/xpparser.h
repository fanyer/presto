/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPPARSER_H
#define XPPARSER_H

#include "modules/xpath/src/xpdefs.h"
#include "modules/xpath/src/xplexer.h"

#include "modules/util/adt/opvector.h"

class XPath_Expression;
class XPath_LocationPath;
class XPath_Namespaces;
class XPath_Producer;
class XPath_ContextStateSizes;

#ifdef XPATH_EXTENSION_SUPPORT
class XPathExtensions;
class XPathVariable;
class XPath_VariableReader;
#endif // XPATH_EXTENSION_SUPPORT

#ifdef XPATH_PATTERN_SUPPORT
class XPath_Pattern;
#endif // XPATH_PATTERN_SUPPORT

#ifdef XPATH_ERRORS
# define XPATH_PARSE_ERROR(message, location) parser->ParseErrorL (message, location, XPath_Token ())
# define XPATH_PARSE_ERROR_DATA(message, location, data) parser->ParseErrorL (message, location, XPath_Token (XP_TOKEN_INVALID, data, uni_strlen (data)))
# define XPATH_COMPILATION_ERROR(message, location) parser->CompilationErrorL (message, location, 0)
# define XPATH_COMPILATION_ERROR_NAME(message, location) parser->CompilationErrorL (message, location, parser->GetCurrentName ())
#else // XPATH_ERRORS
# define XPATH_PARSE_ERROR(message, location) LEAVE (OpStatus::ERR)
# define XPATH_PARSE_ERROR_DATA(message, location, data) LEAVE (OpStatus::ERR)
# define XPATH_COMPILATION_ERROR(message, location) LEAVE (OpStatus::ERR)
# define XPATH_COMPILATION_ERROR_NAME(message, location) LEAVE (OpStatus::ERR)
#endif // XPATH_ERRORS

class XPath_Parser
{
protected:
  XPath_Lexer lexer;
  XPath_Namespaces *namespaces;
  OpVector<XPath_Expression> expressions;
  unsigned expressions_skip;

  int state_counter, value_counter, number_counter, buffer_counter, node_counter, nodeset_counter, nodelist_counter, stringset_counter, ci_counter;

#ifdef XPATH_EXTENSION_SUPPORT
  int variablestate_counter, functionstate_counter;
  XPathExtensions *extensions;
  XPath_VariableReader *readers;
#endif // XPATH_EXTENSION_SUPPORT

#ifdef XPATH_ERRORS
  OpString *error_message_storage;

  unsigned current_start, current_end;
  XMLExpandedName current_name;
#endif // XPATH_ERRORS

  void ResolveQNameL (TempBuffer &buffer, XMLExpandedName &name, const XPath_Token &token);

  BOOL GetIsEmpty ();
  unsigned GetCount ();

  void PushExpressionL (XPath_Expression *expr);
  void PushBinaryExpressionL (XPath_ExpressionType type);
  void PushProducerExpressionL (XPath_ExpressionType type);
  void PushNegateExpressionL (XPath_Expression *expr);
  void PushLocationPathExpressionL (XPath_Producer *producer);
  void PushVariableReferenceExpressionL (const XPath_Token &token);
  void PushLiteralExpressionL (const XPath_Token &token);
  void PushNumberExpressionL (const XPath_Token &token);
  void PushFunctionCallExpressionL (const XPath_Token &token, unsigned argc);

  XPath_Expression *PopExpression ();
  XPath_Expression *LastExpression ();

  XPath_Expression *ParseExpressionL (XPath_ExpressionType type = XP_EXPR_OR, BOOL top_expr = FALSE);
  XPath_Producer *ParseLocationPathL (XPath_Producer *base_producer, BOOL is_descendants = FALSE);

  void Reset ();

public:
  XPath_Parser (XPath_Namespaces *namespaces, const uni_char *expression);
  ~XPath_Parser ();

#ifdef XPATH_EXTENSION_SUPPORT
  void SetExtensions (XPathExtensions *new_extensions) { extensions = new_extensions; }
  XPathExtensions *GetExtensions () { return extensions; }

  XPath_VariableReader *GetVariableReaderL (const XMLExpandedName &name);

  XPath_VariableReader *TakeVariableReaders ();
#endif // XPATH_EXTENSION_SUPPORT

#ifdef XPATH_ERRORS
  void SetErrorMessageStorage (OpString *new_error_message_storage) { error_message_storage = new_error_message_storage; lexer.error_message_storage = new_error_message_storage; }

  void ParseErrorL (const char *message, const XPath_Location &location, const XPath_Token &token);
  void CompilationErrorL (const char *message, const XPath_Location &location, const XMLExpandedName *name);

  XPath_Location GetCurrentLocation ();
  const XMLExpandedName *GetCurrentName ();
#endif // XPATH_ERRORS

  XPath_Expression *ParseToPrimitiveL ();
  /**< Parse expression and return an XPath_Expression object suitable for
       evaluating to a primitive type (number and string directly or via
       conversion, and boolean if the intrinsic type of the expression is
       number or string.) */

  XPath_Producer *ParseToProducerL (BOOL ordered, BOOL no_duplicates);
  /**< Parse expression and return an XPath_Producer object suitable for
       evaluating to a node-set (or a single node) or a boolean (as in
       "non-empty node-set".)  If the intrinsic type of the expression is a
       primitive type, null is returned; nodes cannot be produced from such an
       expression. */

  void CopyStateSizes (XPath_ContextStateSizes &state_sizes);

#ifdef XPATH_PATTERN_SUPPORT
  XPath_Pattern *ParsePatternL (float &priority);
#endif // XPATH_PATTERN_SUPPORT

  unsigned GetStateIndex () { return (unsigned) ++state_counter; }
  unsigned GetValueIndex () { return (unsigned) ++value_counter; }
  unsigned GetNumberIndex () { return (unsigned) ++number_counter; }
  unsigned GetBufferIndex () { return (unsigned) ++buffer_counter; }
  unsigned GetNodeIndex () { return (unsigned) ++node_counter; }
  unsigned GetNodeSetIndex () { return (unsigned) ++nodeset_counter; }
  unsigned GetNodeListIndex () { return (unsigned) ++nodelist_counter; }
  unsigned GetStringSetIndex () { return (unsigned) ++stringset_counter; }
  unsigned GetContextInformationIndex () { return (unsigned) ++ci_counter; }

#ifdef XPATH_EXTENSION_SUPPORT
  unsigned GetVariableStateIndex () { return (unsigned) ++variablestate_counter; }
  unsigned GetFunctionStateIndex () { return (unsigned) ++functionstate_counter; }
#endif // XPATH_EXTENSION_SUPPORT
};

#endif // XPPARSER_H
