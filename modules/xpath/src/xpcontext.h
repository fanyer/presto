/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPCONTEXT_H
#define XPCONTEXT_H

#include "modules/xpath/xpath.h"
#include "modules/xpath/src/xpitems.h"

#include "modules/util/adt/opvector.h"
#include "modules/util/OpHashTable.h"

#ifdef XPATH_DEBUG_MODE
# define XP_SEQUENCE_POINT(context) do { if (--context->iterations == 0) XPath_Pause (context); } while (0)
extern void XPath_Pause (XPath_Context *context);
#else // XPATH_DEBUG_MODE
# define XP_SEQUENCE_POINT(context)
#endif // XPATH_DEBUG_MODE

class XPath_Node;
class XPath_NodeSet;
class XPath_NodeList;
class XPath_Context;
class XPath_Expression;
class XPath_StringSet;

class XPath_ContextInformation
{
public:
  XPath_ContextInformation ();

  unsigned position;
  unsigned size;

  void Reset ();
};

class XPath_ContextStateSizes
{
public:
  XPath_ContextStateSizes ();

  unsigned states_count;
  unsigned values_count;
  unsigned numbers_count;
  unsigned buffers_count;
  unsigned nodes_count;
  unsigned nodesets_count;
  unsigned nodelists_count;
  unsigned stringsets_count;
  unsigned cis_count;
#ifdef XPATH_EXTENSION_SUPPORT
  unsigned variablestates_count;
  unsigned functionstates_count;
#endif // XPATH_EXTENSION_SUPPORT
};

#ifdef XPATH_PATTERN_SUPPORT
class XPath_PatternContext;
#endif // XPATH_PATTERN_SUPPORT

class XPath_GlobalContext
{
public:
  XPath_GlobalContext ();
  ~XPath_GlobalContext ();

  void Clean (BOOL check);

  void InitializeCoreFunctionsL ();

#ifdef XPATH_PATTERN_SUPPORT
  XPath_PatternContext *pattern_context;
#endif // XPATH_PATTERN_SUPPORT
#ifdef XPATH_EXTENSION_SUPPORT
  XPathExtensions *extensions;
  XPathExtensions::Context *extensions_context;
#endif // XPATH_EXTENSION_SUPPORT
  XPath_Values values_cache;
  XPath_Nodes nodes_cache;
  BOOL isallocated, isclean;
  TempBuffer buffer;
#ifdef XPATH_ERRORS
  OpString *error_message;
  const OpString *source;
#endif // XPATH_ERRORS

  void DeleteStates();
  OP_STATUS AllocateStates (const XPath_ContextStateSizes &sizes);

  unsigned iterations;
  unsigned *states;
  XPath_Value **values; unsigned values_count;
  double *numbers;
  TempBuffer *buffers;
  XPath_Node *nodes;
  XPath_NodeSet *nodesets; unsigned nodesets_count;
  XPath_NodeList *nodelists; unsigned nodelists_count;
  XPath_StringSet *stringsets;
  XPath_ContextInformation *cis;
#ifdef XPATH_EXTENSION_SUPPORT
  XPathVariable::State **variablestates; unsigned variablestates_count;
  XPathFunction::State **functionstates; unsigned functionstates_count;
#endif // XPATH_EXTENSION_SUPPORT
};

class XPath_Context
{
public:
  XPath_Context (XPath_GlobalContext *global, XPath_Node *node, unsigned position, unsigned size);
  XPath_Context (XPath_Context &context);
  XPath_Context (XPath_Context *context, XPath_Node *node, unsigned position, unsigned size);
  ~XPath_Context ();

  XPath_GlobalContext *global;
  XPath_Context *previous;
  XPath_Node *node;
  unsigned position, size;

  TempBuffer *GetTempBuffer () { return &global->buffer; }

  void CopyStatesFromGlobal ();

  unsigned iterations;
  unsigned *states;
  XPath_Value **values;
  double *numbers;
  TempBuffer *buffers;
  XPath_Node *nodes;
  XPath_NodeSet *nodesets;
  XPath_NodeList *nodelists;
  XPath_StringSet *stringsets;
  XPath_ContextInformation *cis;
#ifdef XPATH_EXTENSION_SUPPORT
  XPathVariable::State **variablestates;
  XPathFunction::State **functionstates;
#endif // XPATH_EXTENSION_SUPPORT

#ifdef XPATH_ERRORS
# define XPATH_EVALUATION_ERROR(reason, expression) context->SignalErrorL (reason, expression->location)
# define XPATH_EVALUATION_ERROR_WITH_TYPE(reason, expression, type) context->SignalErrorL (reason, expression->location, type)
  OP_STATUS SetError (const char *reason, const XPath_Location &location, XPath_ValueType type = XP_VALUE_INVALID);
  void SignalErrorL (const char *reason, const XPath_Location &location, XPath_ValueType type = XP_VALUE_INVALID);
#else // XPATH_ERRORS
# define XPATH_EVALUATION_ERROR(reason, expression) context->SignalErrorL ()
# define XPATH_EVALUATION_ERROR_WITH_TYPE(reason, expression, type) context->SignalErrorL ()
  void SignalErrorL ();
#endif // XPATH_ERRORS
};

#endif // XPCONTEXT_H
