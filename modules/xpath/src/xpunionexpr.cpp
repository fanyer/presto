/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpunionexpr.h"
#include "modules/xpath/src/xpvalue.h"
#include "modules/xpath/src/xpnodeset.h"
#include "modules/xpath/src/xpcontext.h"
#include "modules/xpath/src/xplocationpathexpr.h"
#include "modules/xpath/src/xpparser.h"
#include "modules/xpath/src/xpstep.h"

/* static */ XPath_Expression *
XPath_UnionExpression::MakeL (XPath_Parser *parser, XPath_Expression *lhs, XPath_Expression *rhs)
{
  XPath_UnionExpression *expression = 0;
  XPath_Producer *lhs_producer = 0, *rhs_producer = 0;
  OP_STATUS error = OpStatus::OK;

  if (lhs->HasFlag (FLAG_UNION))
    {
      expression = (XPath_UnionExpression *) lhs;
      lhs = 0;
    }
  else if (rhs->HasFlag (FLAG_UNION))
    {
      expression = (XPath_UnionExpression *) rhs;
      rhs = 0;
    }
  else
    {
      expression = OP_NEW (XPath_UnionExpression, (parser));
      if (!expression || !(expression->producers = OP_NEW (OpVector<XPath_Producer>, ())))
        goto exit_oom;
    }

  if (rhs && rhs->HasFlag (FLAG_UNION))
    {
      XPath_UnionExpression *rhs_union = (XPath_UnionExpression *) rhs;

      for (unsigned index = 0, count = rhs_union->producers->GetCount (); index < count; ++index)
        if (OpStatus::IsMemoryError (expression->producers->Add (rhs_union->producers->Get (index))))
          goto exit_oom;

      rhs_union->producers->Clear ();

      OP_DELETE (rhs);
      rhs = 0;
    }

  /* If 'lhs' or 'rhs' are non-NULL, they must be non-union expression that we
     want to convert into producers and add to 'expression->producers'. */

  if (lhs)
    {
      lhs_producer = XPath_Expression::GetProducerL (parser, lhs);

      if (!lhs_producer)
        {
          /* FIXME: Error message. */
          error = OpStatus::ERR;
          goto exit_error;
        }

      lhs = 0;

      if (OpStatus::IsMemoryError (expression->producers->Add (lhs_producer)))
        goto exit_oom;
      else
        lhs_producer = 0;
    }

  if (rhs)
    {
      rhs_producer = XPath_Expression::GetProducerL (parser, rhs);

      if (!rhs_producer)
        {
          /* FIXME: Error message. */
          error = OpStatus::ERR;
          goto exit_error;
        }

      rhs = 0;

      if (OpStatus::IsMemoryError (expression->producers->Add (rhs_producer)))
        goto exit_oom;
    }

  return expression;

 exit_oom:
  error = OpStatus::ERR_NO_MEMORY;

 exit_error:
  /* Anything we shouldn't delete will have been set to 0 already. */
  OP_DELETE (expression);
  OP_DELETE (lhs);
  OP_DELETE (rhs);
  OP_DELETE (lhs_producer);
  OP_DELETE (rhs_producer);

  LEAVE (error);

  /* Keep compiler happy.  Not reached, obviously. */
  return 0;
}

/* virtual */
XPath_UnionExpression::~XPath_UnionExpression ()
{
  if (producers)
    {
      producers->DeleteAll ();
      OP_DELETE (producers);
    }
}

/* virtual */ unsigned
XPath_UnionExpression::GetResultType ()
{
  return XP_VALUE_NODESET;
}

/* virtual */ unsigned
XPath_UnionExpression::GetExpressionFlags ()
{
  unsigned flags = FLAG_PRODUCER;

  for (unsigned index = 0, count = producers->GetCount (); index < count; ++index)
    flags |= (producers->Get (index)->GetExpressionFlags ()) & MASK_INHERITED;

  return flags;
}

/* virtual */ XPath_Producer *
XPath_UnionExpression::GetProducerInternalL (XPath_Parser *parser)
{
  XPath_UnionProducer *producer = OP_NEW_L (XPath_UnionProducer, (parser, producers));

  /* Now owned by 'producer'. */
  producers = 0;

  return producer;
}

XPath_UnionProducer::XPath_UnionProducer (XPath_Parser *parser, OpVector<XPath_Producer> *producers)
  : producers (producers),
    producer_index_index (parser->GetStateIndex ()),
    localci_index (parser->GetContextInformationIndex ())
{
  ci_index = localci_index;
}

/* virtual */
XPath_UnionProducer::~XPath_UnionProducer ()
{
  if (producers)
    {
      producers->DeleteAll ();
      OP_DELETE (producers);
    }
}

static BOOL
XPath_IsMutuallyExclusive (XPath_Producer *producer1, XPath_Producer *producer2)
{
  if (XPath_Step::NodeTest::IsMutuallyExclusive (static_cast<XPath_Step::NodeTest *> (producer1->GetPrevious (XPath_Producer::PREVIOUS_NODETEST, TRUE)),
                                                 static_cast<XPath_Step::NodeTest *> (producer2->GetPrevious (XPath_Producer::PREVIOUS_NODETEST, TRUE))) ||
      XPath_Step::Axis::IsMutuallyExclusive (static_cast<XPath_Step::Axis *> (producer1->GetPrevious (XPath_Producer::PREVIOUS_AXIS, TRUE)),
                                             static_cast<XPath_Step::Axis *> (producer2->GetPrevious (XPath_Producer::PREVIOUS_AXIS, TRUE))))
    return TRUE;
  else
    return FALSE;
}

/* virtual */ unsigned
XPath_UnionProducer::GetProducerFlags ()
{
  unsigned flags = 0;

  BOOL has_no_duplicates = TRUE;

  for (unsigned outer = 0; outer < producers->GetCount (); ++outer)
    {
      XPath_Producer *producer = producers->Get (outer);

      flags |= producer->GetProducerFlags () & (MASK_NEEDS | FLAG_BLOCKING);

      for (unsigned inner = 0; inner < outer; ++inner)
        if (!XPath_IsMutuallyExclusive (producers->Get (inner), producer))
          {
            has_no_duplicates = FALSE;
            break;
          }
    }

  if (has_no_duplicates)
    flags |= FLAG_NO_DUPLICATES;

  return flags;
}

/* virtual */ BOOL
XPath_UnionProducer::Reset (XPath_Context *context, BOOL local_context_only)
{
  for (unsigned index = 0, count = producers->GetCount (); index < count; ++index)
    producers->Get (index)->Reset (context);

  context->states[producer_index_index] = 0;
  return FALSE;
}

/* virtual */ XPath_Node *
XPath_UnionProducer::GetNextNodeL (XPath_Context *context)
{
  unsigned &producer_index = context->states[producer_index_index];

  while (producer_index < producers->GetCount ())
    {
      XPath_Producer *producer = producers->Get (producer_index);
      if (XPath_Node *node = producer->GetNextNodeL (context))
        return node;
      else
        ++producer_index;
    }

  return 0;
}

#endif // XPATH_SUPPORT
