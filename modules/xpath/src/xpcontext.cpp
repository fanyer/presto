/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/xpath.h"
#include "modules/xpath/src/xpapiimpl.h"
#include "modules/xpath/src/xpcontext.h"
#include "modules/xpath/src/xpdefs.h"
#include "modules/xpath/src/xpnodeset.h"
#include "modules/xpath/src/xpnodelist.h"
#include "modules/xpath/src/xpstringset.h"
#include "modules/xpath/src/xpvalue.h"
#include "modules/xpath/src/xputils.h"

#include "modules/util/str.h"
#include "modules/util/tempbuf.h"

#ifdef XPATH_DEBUG_MODE

void
XPath_Pause (XPath_Context *context)
{
  LEAVE (OpBoolean::IS_FALSE);
}

#endif // XPATH_DEBUG_MODE

XPath_ContextStateSizes::XPath_ContextStateSizes ()
  : states_count (0),
    values_count (0),
    numbers_count (0),
    buffers_count (0),
    nodes_count (0),
    nodesets_count (0),
    nodelists_count (0),
    stringsets_count (0),
    cis_count (0)
{
#ifdef XPATH_EXTENSION_SUPPORT
  variablestates_count = 0;
  functionstates_count = 0;
#endif // XPATH_EXTENSION_SUPPORT
}

XPath_GlobalContext::XPath_GlobalContext ()
  : isallocated (FALSE),
    isclean (FALSE),
#ifdef XPATH_ERRORS
    error_message (0),
    source (0),
#endif // XPATH_ERRORS
    iterations (0),
    states (0),
    values (0),
    values_count (0),
    numbers (0),
    buffers (0),
    nodes (0),
    nodesets (0),
    nodesets_count (0),
    nodelists (0),
    nodelists_count (0),
    stringsets (0),
    cis (0)
{
#ifdef XPATH_EXTENSION_SUPPORT
  variablestates = 0;
  variablestates_count = 0;
  functionstates = 0;
  functionstates_count = 0;
#endif // XPATH_EXTENSION_SUPPORT
}

void
XPath_GlobalContext::DeleteStates()
{
  OP_DELETEA (states);
  states = NULL;
  OP_DELETEA (values);
  values = NULL;
  OP_DELETEA (numbers);
  numbers = NULL;
  OP_DELETEA (buffers);
  buffers = NULL;
  OP_DELETEA (nodes);
  nodes = NULL;
  OP_DELETEA (nodesets);
  nodesets = NULL;
  OP_DELETEA (nodelists);
  nodelists = NULL;
  OP_DELETEA (stringsets);
  stringsets = NULL;
  OP_DELETEA (cis);
  cis = NULL;
#ifdef XPATH_EXTENSION_SUPPORT
  OP_DELETEA (variablestates);
  variablestates = NULL;
  OP_DELETEA (functionstates);
  functionstates = NULL;
#endif // XPATH_EXTENSION_SUPPORT
}

XPath_GlobalContext::~XPath_GlobalContext ()
{
  if (isallocated)
    {
      DeleteStates();
    }
}

void
XPath_GlobalContext::Clean (BOOL check)
{
  if (!isclean)
    {
      if (values_count != 0 || nodesets_count != 0 || nodelists_count != 0)
        {
          XPath_Context context (this, 0, 0, 0);
          unsigned index;

          for (index = 0; index < values_count; ++index)
            XPath_Value::DecRef (&context, values[index]);
          for (index = 0; index < nodesets_count; ++index)
            nodesets[index].Clear (&context);
          for (index = 0; index < nodelists_count; ++index)
            nodelists[index].Clear (&context);
        }

#ifdef XPATH_EXTENSION_SUPPORT
      if (variablestates_count != 0)
        for (unsigned index = 0; index < variablestates_count; ++index)
          OP_DELETE (variablestates[index]);
      if (functionstates_count != 0)
        for (unsigned index = 0; index < functionstates_count; ++index)
          OP_DELETE (functionstates[index]);
#endif // XPATH_EXTENSION_SUPPORT

#ifdef _DEBUG
      if (check)
        OP_ASSERT (values_cache.IsAllFree ());
#endif // _DEBUG

      values_cache.Clean ();

#ifdef _DEBUG
      if (check)
        OP_ASSERT (nodes_cache.IsAllFree ());
#endif // _DEBUG

      nodes_cache.Clean ();
      isclean = TRUE;
    }
}

OP_STATUS
XPath_GlobalContext::AllocateStates (const XPath_ContextStateSizes &sizes)
{
  isclean = FALSE;

  if (!isallocated)
    {
#define XP_ALLOCATE(what, type) \
      if (sizes.what ## _count != 0) \
        { \
          if (sizes.what ## _count > 16*1024) \
            { \
              OP_ASSERT (!"Please check the sanity of the input. Is it distorted?"); \
              return OpStatus::ERR; \
            } \
          if (!(what = OP_NEWA (type, sizes.what ## _count))) \
            { \
              DeleteStates(); \
              return OpStatus::ERR_NO_MEMORY; \
            } \
        } \
      else \
        what = 0;

#ifdef _DEBUG
#define XP_ALLOCATE_AND_RESET(what, type) \
      XP_ALLOCATE (what, type); \
      if (sizes.what ## _count != 0) \
        op_memset (what, 0xef, sizes.what ## _count * sizeof (type));
#else // _DEBUG
#define XP_ALLOCATE_AND_RESET(what, type) XP_ALLOCATE (what, type);
#endif // _DEBUG

      XP_ALLOCATE_AND_RESET (states, unsigned);
      XP_ALLOCATE (values, XPath_Value *);
      XP_ALLOCATE_AND_RESET (numbers, double);
      XP_ALLOCATE (buffers, TempBuffer);
      XP_ALLOCATE (nodes, XPath_Node);
      XP_ALLOCATE (nodesets, XPath_NodeSet);
      XP_ALLOCATE (nodelists, XPath_NodeList);
      XP_ALLOCATE (stringsets, XPath_StringSet);
      XP_ALLOCATE_AND_RESET (cis, XPath_ContextInformation);
#ifdef XPATH_EXTENSION_SUPPORT
      XP_ALLOCATE (variablestates, XPathVariable::State *);
      XP_ALLOCATE (functionstates, XPathFunction::State *);
#endif // XPATH_EXTENSION_SUPPORT

      values_count = sizes.values_count;
      nodesets_count = sizes.nodesets_count;
      nodelists_count = sizes.nodelists_count;
#ifdef XPATH_EXTENSION_SUPPORT
      variablestates_count = sizes.variablestates_count;
      functionstates_count = sizes.functionstates_count;
#endif // XPATH_EXTENSION_SUPPORT

      isallocated = TRUE;
    }

  op_memset (values, 0, values_count * sizeof *values);
#ifdef XPATH_EXTENSION_SUPPORT
  op_memset (variablestates, 0, variablestates_count * sizeof *variablestates);
  op_memset (functionstates, 0, functionstates_count * sizeof *functionstates);
#endif // XPATH_EXTENSION_SUPPORT

  return OpStatus::OK;
}

XPath_Context::XPath_Context (XPath_GlobalContext *global, XPath_Node *node, unsigned position, unsigned size)
  : global (global),
    previous (0),
    node (node),
    position (position),
    size (size),
    iterations (1)
{
  CopyStatesFromGlobal ();
  global->iterations = iterations;
}

XPath_Context::XPath_Context (XPath_Context &context)
  : global (context.global),
    previous (&context),
    node (context.node),
    position (context.position),
    size (context.size),
    iterations (context.iterations)
{
  CopyStatesFromGlobal ();
}

XPath_Context::XPath_Context (XPath_Context *context, XPath_Node *node, unsigned position, unsigned size)
  : global (context->global),
    previous (context),
    node (node),
    position (position),
    size (size),
    iterations (context->iterations)
{
  CopyStatesFromGlobal ();
}

XPath_Context::~XPath_Context ()
{
  if (previous)
    previous->iterations = iterations;
}

void
XPath_Context::CopyStatesFromGlobal ()
{
  states = global->states;
  values = global->values;
  numbers = global->numbers;
  buffers = global->buffers;
  nodes = global->nodes;
  nodesets = global->nodesets;
  nodelists = global->nodelists;
  stringsets = global->stringsets;
  cis = global->cis;

#ifdef XPATH_EXTENSION_SUPPORT
  variablestates = global->variablestates;
  functionstates = global->functionstates;
#endif // XPATH_EXTENSION_SUPPORT
}

#ifdef XPATH_ERRORS

OP_STATUS
XPath_Context::SetError (const char *reason, const XPath_Location &location, XPath_ValueType type)
{
  if (global->error_message)
    {
      const char *insertion_point = op_strstr (reason, "%s");
      OpString local, &error_message = insertion_point ? local : *global->error_message;

      OpString location_string; ANCHOR (OpString, location_string);
      if (location.IsValid ())
        {
          if (location.start == location.end)
            RETURN_IF_ERROR (location_string.AppendFormat (UNI_L ("character %u"), location.start + 1));
          else
            RETURN_IF_ERROR (location_string.AppendFormat (UNI_L ("characters %u-%u"), location.start + 1, location.end));

          if (global->source && location.start < static_cast<unsigned> (global->source->Length ()) && location.end <= static_cast<unsigned> (global->source->Length ()))
            {
              location_string.AppendL (", \"");
              location_string.AppendL (global->source->CStr () + location.start, location.end - location.start);
              location_string.AppendL ("\"");
            }
        }
      else
        RETURN_IF_ERROR (location_string.Set ("unknown location"));

      RETURN_IF_ERROR (error_message.AppendFormat (UNI_L ("evaluation error (%s): "), location_string.CStr ()));
      RETURN_IF_ERROR (error_message.Append (reason));

      if (insertion_point)
        {
          const uni_char *type_string;
          switch (type)
            {
            case XP_VALUE_NUMBER: type_string = UNI_L ("number"); break;
            case XP_VALUE_STRING: type_string = UNI_L ("string"); break;
            case XP_VALUE_BOOLEAN: type_string = UNI_L ("boolean"); break;
            default: type_string = UNI_L ("node-set"); break;
            }
          RETURN_IF_ERROR (global->error_message->AppendFormat (error_message.CStr(), type_string));
        }
    }

  return OpStatus::ERR;
}

void
XPath_Context::SignalErrorL (const char *reason, const XPath_Location &location, XPath_ValueType type)
{
  LEAVE (SetError (reason, location, type));
}

#else // XPATH_ERRORS

void
XPath_Context::SignalErrorL ()
{
  LEAVE (OpStatus::ERR);
}

#endif // XPATH_ERRORS
#endif // XPATH_SUPPORT
