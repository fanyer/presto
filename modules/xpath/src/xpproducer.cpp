/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpproducer.h"
#include "modules/xpath/src/xputils.h"
#include "modules/xpath/src/xpparser.h"
#include "modules/xpath/src/xpvalue.h"
#include "modules/xpath/src/xpexpr.h"
#include "modules/xpath/src/xpunknown.h"
#include "modules/xpath/src/xpstep.h"
#include "modules/xpath/src/xpstringset.h"

XPath_ContextInformation::XPath_ContextInformation ()
{
  Reset ();
}

void
XPath_ContextInformation::Reset ()
{
  position = size = 0;
}

XPath_Producer::XPath_Producer ()
  /* An out-of-range index that should be unlikely to wrap around and actually be accessible. */
  : ci_index (~0u / sizeof(unsigned) / sizeof(void *))
{
}

/* virtual */
XPath_Producer::~XPath_Producer ()
{
}

OP_BOOLEAN
XPath_Producer::GetNextNode (XPath_Node *&node, XPath_Context *context)
{
  TRAPD (status, node = GetNextNodeL (context));
  return status == OpStatus::OK ? OpBoolean::IS_TRUE : status;
}

/* virtual */ XPath_Producer::Type
XPath_Producer::GetProducerType ()
{
  return TYPE_OTHER;
}

/* virtual */ unsigned
XPath_Producer::GetProducerFlags ()
{
  /**< This is true for most producers, I suppose. */
  return FLAG_DOCUMENT_ORDER | FLAG_NO_DUPLICATES;
}

static unsigned
XPath_CalculateLocationPathComplexity (XPath_Producer *producer)
{
  XPath_Producer *context_node = producer->GetPrevious (XPath_Producer::PREVIOUS_CONTEXT_NODE, TRUE);

  /* You know, "how long is a string?" */
  unsigned string_length = 0;

  while (producer && producer != context_node)
    {
      XPath_Step::Axis *axis = static_cast<XPath_Step::Axis *> (producer->GetPrevious (XPath_Producer::PREVIOUS_AXIS, TRUE));
      XPath_Step::NodeTest *nodetest = static_cast<XPath_Step::NodeTest *> (producer->GetPrevious (XPath_Producer::PREVIOUS_NODETEST, TRUE));
      XPath_Step::Predicate *predicate = static_cast<XPath_Step::Predicate *> (producer->GetPrevious (XPath_Producer::PREVIOUS_PREDICATE, TRUE));

      while (producer && producer != context_node)
        {
          if (producer == axis)
            {
              switch (axis->GetAxis ())
                {
                case XP_AXIS_PARENT:
                case XP_AXIS_SELF:
                case XP_AXIS_ATTRIBUTE:
                  /* The parent and self axes are single node, and the attribute axis is
                     at least usually very limited, and if there is a full name test,
                     also single node. */
                  break;

                case XP_AXIS_ANCESTOR:
                case XP_AXIS_ANCESTOR_OR_SELF:
                case XP_AXIS_CHILD:
                case XP_AXIS_PRECEDING_SIBLING:
                case XP_AXIS_FOLLOWING_SIBLING:
                case XP_AXIS_NAMESPACE:
                  /* More uncertain, but at least not crazy expensive. */
                  ++string_length;
                  break;

                default:
                  /* Crazy expensive.  :-) */
                  return XPath_Expression::FLAG_COMPLEXITY_COMPLEX;
                }

              producer = producer->GetPrevious (XPath_Producer::PREVIOUS_ANY, FALSE);
              break;
            }
          else if (producer == nodetest)
            nodetest = static_cast<XPath_Step::NodeTest *> (nodetest->GetPrevious (XPath_Producer::PREVIOUS_NODETEST, FALSE));
          else if (producer == predicate)
            {
              unsigned predicate_flags = predicate->GetPredicateExpressionFlags ();

              if ((predicate_flags & XPath_Expression::MASK_COMPLEXITY) != XPath_Expression::COMPLEXITY_TRIVIAL ||
                  (predicate_flags & (XPath_Expression::FLAG_CONTEXT_SIZE | XPath_Expression::FLAG_CONTEXT_POSITION)) != 0)
                return XPath_Expression::FLAG_COMPLEXITY_COMPLEX;
              else
                ++string_length;

              predicate = static_cast<XPath_Step::Predicate *> (predicate->GetPrevious (XPath_Producer::PREVIOUS_PREDICATE, FALSE));
            }
          else
            /* Unknown producer => unknown complexity. */
            return XPath_Expression::FLAG_COMPLEXITY_COMPLEX;

          producer = producer->GetPrevious (XPath_Producer::PREVIOUS_ANY, FALSE);
        }
    }

  /* These conditions aren't picked arbitrarily, I promise. */
  if (string_length > 5)
    return XPath_Expression::FLAG_COMPLEXITY_COMPLEX;
  else if (string_length > 0)
    return XPath_Expression::FLAG_COMPLEXITY_SIMPLE;
  else
    return 0;
}

unsigned
XPath_Producer::GetExpressionFlags ()
{
  unsigned expression_flags = XPath_CalculateLocationPathComplexity (this), producer_flags = GetProducerFlags ();

  if ((producer_flags & FLAG_CONTEXT_SIZE) != 0)
    expression_flags |= XPath_Expression::FLAG_CONTEXT_SIZE;
  if ((producer_flags & FLAG_CONTEXT_POSITION) != 0)
    expression_flags |= XPath_Expression::FLAG_CONTEXT_POSITION;
  if ((producer_flags & FLAG_BLOCKING) != 0)
    expression_flags |= XPath_Expression::FLAG_BLOCKING;

  return expression_flags;
}

XPath_Producer *
XPath_Producer::GetPrevious (What what, BOOL include_self)
{
  /* Calling 'GetPrevious (PREVIOUS_ANY, TRUE)' would always return this
     producer, and that is fairly pointless.  However, if the 'what' or
     'include_self' arguments in the call are not known compile-time, and it
     would be convenient to make a call like this, this assertion can just be
     removed. */
  OP_ASSERT (what != PREVIOUS_ANY || !include_self);

  return GetPreviousInternal (what, include_self);
}

/* virtual */ void
XPath_Producer::OptimizeL (unsigned hints)
{
}

/* virtual */ BOOL
XPath_Producer::TransformL (XPath_Parser *parser, Transform transform, TransformData &data)
{
  return FALSE;
}

/* static */ XPath_Producer *
XPath_Producer::EnsureFlagsL (XPath_Parser *parser, XPath_Producer *producer, unsigned flags)
{
  unsigned producer_flags = producer->GetProducerFlags ();

#ifdef XPATH_EXTENSION_SUPPORT
  if ((producer_flags & FLAG_UNKNOWN) != 0 && static_cast<XPath_Unknown *> (producer)->GetUnknownType () == XPath_Unknown::UNKNOWN_VARIABLE_REFERENCE)
    static_cast<XPath_Unknown *> (producer)->SetProducerFlags (flags);
  else
#endif // XPATH_EXTENSION_SUPPORT
    {
      XPath_Producer *wrapper = producer;

      if ((flags & FLAG_SINGLE_NODE) != 0 && (producer_flags & FLAG_SINGLE_NODE) == 0)
        wrapper = OP_NEW (XPath_SingleNodeProducer, (parser, producer, flags & MASK_ORDER));
      else
        {
          BOOL change_order = (flags & MASK_ORDER) != 0 && (flags & MASK_ORDER) != (producer_flags & MASK_ORDER);
          BOOL remove_duplicates = (flags & (FLAG_NO_DUPLICATES | FLAG_KNOWN_SIZE)) != 0 && (producer_flags & FLAG_NO_DUPLICATES) == 0;
          BOOL count = (flags & FLAG_KNOWN_SIZE) != 0 && (producer_flags & FLAG_KNOWN_SIZE) == 0;

          if ((flags & MASK_ORDER) == 0)
            flags |= FLAG_DOCUMENT_ORDER;

          if (remove_duplicates)
            if (!change_order && !count)
              /* We need to remove duplicates, but not change order.  A filter
                 that uses a node-set to filter out duplicates "incrementally"
                 is the best.  We end up collecting a complete node-set
                 (unless interrupted,) but can begin returning nodes right
                 away. */
              wrapper = OP_NEW (XPath_NodeSetFilter, (parser, producer, flags));
            else
              {
                /* We need to remove duplicates and change order or
                   count.  A collector that uses a node-set to filter
                   out duplicates will do, but since we need to sort
                   or reverse it, we can't begin returning nodes until
                   we've collected the full node-set. */
                wrapper = OP_NEW (XPath_NodeSetCollector, (parser, producer, flags));
                flags &= ~FLAG_LOCAL_CONTEXT_POSITION;
              }
          else if (change_order || count)
            {
              /* We need a different order than that which the
                 producer produces, but needn't worry about
                 duplicates.  Collecting a node-list is cheaper than
                 collecting a node-set, so we do that instead. */
              wrapper = OP_NEW (XPath_NodeListCollector, (parser, producer, flags));
              flags &= ~FLAG_LOCAL_CONTEXT_POSITION;
            }
        }

      if (!wrapper)
        {
          if ((flags & FLAG_SECONDARY_WRAPPER) == 0)
            OP_DELETE (producer);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }

      producer = wrapper;
    }

  if ((flags & FLAG_LOCAL_CONTEXT_POSITION) != 0)
    {
      XPath_Producer *wrapper = OP_NEW (XPath_LocalContextPositionProducer, (parser, producer, flags));

      if (!wrapper)
        {
          if ((flags & FLAG_SECONDARY_WRAPPER) == 0)
            OP_DELETE (producer);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }

      producer = wrapper;
    }

  return producer;
}

/* virtual */ XPath_Producer *
XPath_Producer::GetPreviousInternal (What what, BOOL include_self)
{
  return what == PREVIOUS_ANY && include_self ? this : 0;
}

XPath_Producer *
XPath_Producer::CallGetPreviousInternalOnOther (XPath_Producer *other, What what, BOOL include_self)
{
  return other->GetPreviousInternal (what, include_self);
}

XPath_EmptyProducer::XPath_EmptyProducer (XPath_Parser *parser)
  : localci_index (parser->GetContextInformationIndex ())
{
  ci_index = localci_index;
}

/* virtual */ BOOL
XPath_EmptyProducer::Reset (XPath_Context *context, BOOL local_context_only)
{
  return FALSE;
}

/* virtual */ XPath_Node *
XPath_EmptyProducer::GetNextNodeL (XPath_Context *context)
{
  return 0;
}

/* virtual */ unsigned
XPath_EmptyProducer::GetProducerFlags ()
{
  return FLAG_DOCUMENT_ORDER | FLAG_NO_DUPLICATES | FLAG_KNOWN_SIZE;
}

XPath_InitialContextProducer::XPath_InitialContextProducer (XPath_Parser *parser, BOOL absolute)
  : absolute (absolute),
    state_index (parser->GetStateIndex ()),
    node_index (parser->GetNodeIndex ()),
    localci_index (parser->GetContextInformationIndex ())
{
  ci_index = localci_index;
}

/* virtual */ BOOL
XPath_InitialContextProducer::Reset (XPath_Context *context, BOOL local_context_only)
{
  if (!local_context_only)
    {
      context->states[state_index] = 0;

      if (absolute)
        {
          XMLTreeAccessor *tree = context->node->tree;
          XMLTreeAccessor::Node *treenode = tree->GetRoot ();

          context->nodes[node_index].Set (tree, treenode);
        }
      else
        context->nodes[node_index].CopyL (context->node);

      XPath_ContextInformation &ci = context->cis[localci_index];
      ci.position = context->position;
      ci.size = context->size;
    }

  return FALSE;
}

/* virtual */ XPath_Node *
XPath_InitialContextProducer::GetNextNodeL (XPath_Context *context)
{
  unsigned &state = context->states[state_index];

  if (state == 0)
    {
      state = 1;
      return XPath_Node::IncRef (&context->nodes[node_index]);
    }
  else
    return 0;
}

/* virtual */ unsigned
XPath_InitialContextProducer::GetContextSizeL (XPath_Context *context)
{
  return 1;
}

/* virtual */ unsigned
XPath_InitialContextProducer::GetProducerFlags ()
{
  return FLAG_DOCUMENT_ORDER | FLAG_NO_DUPLICATES | FLAG_KNOWN_SIZE | FLAG_SINGLE_NODE;
}

/* virtual */ XPath_Producer *
XPath_InitialContextProducer::GetPreviousInternal (What what, BOOL include_self)
{
  if (include_self && (what == PREVIOUS_ANY || what == PREVIOUS_CONTEXT_NODE))
    return this;
  else
    return 0;
}

XPath_ChainProducer::XPath_ChainProducer (XPath_Producer *producer)
  : producer (producer),
    owns_producer (TRUE)
{
  if (producer)
    ci_index = producer->ci_index;
}

/* virtual */
XPath_ChainProducer::~XPath_ChainProducer ()
{
  if (owns_producer)
    OP_DELETE (producer);
}

/* virtual */ BOOL
XPath_ChainProducer::Reset (XPath_Context *context, BOOL local_context_only)
{
  return producer->Reset (context, local_context_only);
}

/* virtual */ XPath_Node *
XPath_ChainProducer::GetNextNodeL (XPath_Context *context)
{
  return producer->GetNextNodeL (context);
}

/* virtual */ unsigned
XPath_ChainProducer::GetProducerFlags ()
{
  /* Inherit everything that is inheritable. */
  return producer->GetProducerFlags () & MASK_INHERITED;
}

/* virtual */ void
XPath_ChainProducer::OptimizeL (unsigned hints)
{
  producer->OptimizeL (hints);
}

/* virtual */ BOOL
XPath_ChainProducer::TransformL (XPath_Parser *parser, Transform transform, TransformData &data)
{
#ifdef XPATH_NODE_PROFILE_SUPPORT
  if (transform == TRANSFORM_NODE_PROFILE)
    return producer->TransformL (parser, transform, data);
  else
#endif // XPATH_NODE_PROFILE_SUPPORT
    return FALSE;
}

/* virtual */ XPath_Producer *
XPath_ChainProducer::GetPreviousInternal (What what, BOOL include_self)
{
  return what == PREVIOUS_ANY && include_self ? this : CallGetPreviousInternalOnOther (producer, what, TRUE);
}

XPath_LocalContextPositionProducer::XPath_LocalContextPositionProducer (XPath_Parser *parser, XPath_Producer *producer, unsigned flags)
  : XPath_ChainProducer (producer),
    localci_index (parser->GetContextInformationIndex ())
{
  ci_index = localci_index;

  if ((flags & FLAG_SECONDARY_WRAPPER) != 0)
    owns_producer = FALSE;
}

/* virtual */ BOOL
XPath_LocalContextPositionProducer::Reset (XPath_Context *context, BOOL local_context_only)
{
  if (!local_context_only)
    {
      context->cis[localci_index].Reset ();
      return producer->Reset (context, local_context_only);
    }
  else
    return FALSE;
}

XPath_SingleNodeProducer::XPath_SingleNodeProducer (XPath_Parser *parser, XPath_Producer *producer, unsigned flags)
  : XPath_ChainProducer (producer),
    flags (flags),
    state_index (parser->GetStateIndex ()),
    node_index (parser->GetNodeIndex ()),
    localci_index (parser->GetContextInformationIndex ())
{
  ci_index = localci_index;
}

/* virtual */ BOOL
XPath_SingleNodeProducer::Reset (XPath_Context *context, BOOL local_context_only)
{
  if (!local_context_only)
    {
      context->states[state_index] = 0;
      context->nodes[node_index].Reset ();
      context->cis[localci_index].Reset ();

      return XPath_ChainProducer::Reset (context, local_context_only);
    }
  else
    return FALSE;
}

/* virtual */ XPath_Node *
XPath_SingleNodeProducer::GetNextNodeL (XPath_Context *context)
{
  unsigned &state = context->states[state_index];

  if (state == 1)
    {
      XP_SEQUENCE_POINT (context);
      return 0;
    }

  XPath_Node &previous = context->nodes[node_index];

  if ((flags & MASK_ORDER) == 0)
    {
      /* No particular order requested, just return whatever node we find
         first. */

      XPath_Node *node = producer->GetNextNodeL (context);

      context->cis[localci_index].size = node != 0;
      state = 1;

      return node;
    }
  else
    {
      /* First or last node in document order requested.  Read all nodes,
         storing each that precedes (or succeeds) the previous one stored, and
         then return the one stored. */

      while (XPath_Node *node = producer->GetNextNodeL (context))
        {
          if (!previous.IsValid () || !XPath_Node::Precedes (node, &previous) == !((flags & FLAG_DOCUMENT_ORDER) != 0))
            previous.CopyL (node);
          XPath_Node::DecRef (context, node);
        }

      context->cis[localci_index].size = !!previous.IsValid ();
      state = 1;

      return previous.IsValid () ? XPath_Node::IncRef (&previous) : 0;
    }
}

/* virtual */ unsigned
XPath_SingleNodeProducer::GetProducerFlags ()
{
  /* Might be tempting to say FLAG_KNOWN_SIZE here, but we really don't know
     that we'll find any node to return.  And which order we say is of course
     irrelevant, since we return a single node, so we say no order. */
  return (flags & MASK_ORDER) | FLAG_SINGLE_NODE;
}

XPath_Filter::XPath_Filter (XPath_Producer *producer)
  : XPath_ChainProducer (producer)
{
}

/* virtual */ XPath_Node *
XPath_Filter::GetNextNodeL (XPath_Context *context)
{
  while (XPath_Node *node = producer->GetNextNodeL (context))
    {
      if (MatchL (context, node))
        return node;
      else
        XPath_Node::DecRef (context, node);

      XP_SEQUENCE_POINT (context);
    }

  return 0;
}

/* virtual */ unsigned
XPath_Filter::GetProducerFlags ()
{
  /* Inherit everything but FLAG_KNOWN_SIZE.  We're filtering nodes out, so
     we're changing the size, and we most likely don't know exactly how. */
  return XPath_ChainProducer::GetProducerFlags () & ~FLAG_KNOWN_SIZE;
}

#ifdef XPATH_NODE_PROFILE_SUPPORT

/* virtual */ BOOL
XPath_Filter::TransformL (XPath_Parser *parser, Transform transform, TransformData &data)
{
  if (XPath_ChainProducer::TransformL (parser, transform, data))
    {
      if (transform == TRANSFORM_NODE_PROFILE)
        data.nodeprofile.filtered = 1;
      return TRUE;
    }
  else
    return FALSE;
}

#endif // XPATH_NODE_PROFILE_SUPPORT

XPath_NodeListCollector::XPath_NodeListCollector (XPath_Parser *parser, XPath_Producer *producer, unsigned flags)
  : XPath_ChainProducer (producer),
    sort (FALSE),
    reverse (FALSE),
    state_index (parser->GetStateIndex ()),
    nodelist_index (parser->GetNodeListIndex ()),
    localci_index (parser->GetContextInformationIndex ())
{
  unsigned requested_order = flags & MASK_ORDER, producer_order = producer->GetProducerFlags () & MASK_ORDER;

  if (requested_order && !producer_order)
    {
      sort = TRUE;
      producer_order = FLAG_DOCUMENT_ORDER;
    }

  if (requested_order && requested_order != producer_order)
    reverse = TRUE;

  ci_index = localci_index;

  if ((flags & FLAG_SECONDARY_WRAPPER) != 0)
    owns_producer = FALSE;
}

/* virtual */ BOOL
XPath_NodeListCollector::Reset (XPath_Context *context, BOOL local_context_only)
{
  context->states[state_index] = (sort ? 0 : 2);
  context->nodelists[nodelist_index].Clear (context);
  context->cis[localci_index].Reset ();
  return XPath_ChainProducer::Reset (context, local_context_only);
}

/* virtual */ XPath_Node *
XPath_NodeListCollector::GetNextNodeL (XPath_Context *context)
{
  unsigned &state = context->states[state_index];
  XPath_NodeList &nodelist = context->nodelists[nodelist_index];
  XPath_ContextInformation &localci = context->cis[localci_index];

  /* States: state & 1 == 0: document order or unknown order so far
             state & 1 == 1: reverse document order so far

             state & 2 == 0: ordered
             state & 2 == 2: unordered (or no sorting requested)

             state & 4 == 0: fetching nodes
             state & 4 == 4: all nodes fetched

             state / 4: index of next node to return */

  /* Still fetching; can't have begun returning nodes. */
  OP_ASSERT ((state & 4) == 4 || state <= 3);

  if ((state & 4) == 0)
    {
      unsigned state_local = state;

      while (XPath_Node *node = producer->GetNextNodeL (context))
        {
          /* "Worst case" (when nodes come in order) we'll make as many calls
             to XPath_Node::Precedes as in the best case when sorting.  If
             nodes don't come in order, we might give up and end up doing
             fewer calls here, but then they'll all have been in vain. */

          if (state_local < 2)
            switch (nodelist.GetCount ())
              {
              case 0:
                /* First node.  Nothing to do. */
                break;

              case 1:
                /* Second node.  Determine order. */
                if (XPath_Node::Precedes (node, nodelist.Get (0)))
                  /* Second node precedes first: reverse document order. */
                  state = state_local = 1;
                break;

              default:
                /* Third+ node.  Check if it is out of order. */
                if (!XPath_Node::Precedes (node, nodelist.GetLast ()) == !(state_local == 0))
                  /* Not the expected order: node list needs to be sorted
                     later. */
                  state = state_local = 2;
              }

          nodelist.AddL (context, node);
          XPath_Node::DecRef (context, node);

          XP_SEQUENCE_POINT (context);
        }

      if (sort && state_local == 2)
        nodelist.SortL ();

      state |= 4;
      localci.size = nodelist.GetCount ();

      XP_SEQUENCE_POINT (context);
    }

  unsigned index = state >> 3;

  if (index < nodelist.GetCount ())
    {
      state = ((index + 1) << 3) | (state & 7);
      return XPath_Node::IncRef (nodelist.Get (index, ((state & 1) == 0) == reverse));
    }

  return 0;
}

/* virtual */ unsigned
XPath_NodeListCollector::GetContextSizeL (XPath_Context *context)
{
  return context->nodelists[nodelist_index].GetCount ();
}

/* virtual */ unsigned
XPath_NodeListCollector::GetProducerFlags ()
{
  /* Inherit everything, and introduce or reverse order if we're sorting or
     reversing. */

  unsigned flags = producer->GetProducerFlags () & MASK_INHERITED;

  if (sort)
    flags = (flags & ~MASK_ORDER) | FLAG_DOCUMENT_ORDER;

  if (reverse)
    {
      /* It seems odd to be reversing if we didn't have a specific order to
         begin with. */
      OP_ASSERT ((flags & MASK_ORDER) != 0);

      flags = flags ^ MASK_ORDER;
    }

  return flags;
}

XPath_NodeSetCollector::XPath_NodeSetCollector (XPath_Parser *parser, XPath_Producer *producer, unsigned flags)
  : XPath_ChainProducer (producer),
    sort (FALSE),
    reverse (FALSE),
    state_index (parser->GetStateIndex ()),
    nodeset_index (parser->GetNodeSetIndex ()),
    localci_index (parser->GetContextInformationIndex ())
{
  unsigned requested_order = flags & MASK_ORDER, producer_order = producer->GetProducerFlags () & MASK_ORDER;

  if (requested_order && !producer_order)
    {
      sort = TRUE;
      producer_order = FLAG_DOCUMENT_ORDER;
    }

  if (requested_order != producer_order)
    reverse = TRUE;

  ci_index = localci_index;

  if ((flags & FLAG_SECONDARY_WRAPPER) != 0)
    owns_producer = FALSE;
}

/* virtual */ BOOL
XPath_NodeSetCollector::Reset (XPath_Context *context, BOOL local_context_only)
{
  context->states[state_index] = 0;
  context->nodesets[nodeset_index].Clear (context);
  context->cis[localci_index].Reset ();
  return XPath_ChainProducer::Reset (context, local_context_only);
}

/* virtual */ XPath_Node *
XPath_NodeSetCollector::GetNextNodeL (XPath_Context *context)
{
  unsigned &state = context->states[state_index];
  XPath_NodeSet &nodeset = context->nodesets[nodeset_index];
  XPath_ContextInformation &localci = context->cis[localci_index];

  if (state == 0)
    {
      while (XPath_Node *node = producer->GetNextNodeL (context))
        {
          nodeset.AddL (context, node);
          XPath_Node::DecRef (context, node);

          XP_SEQUENCE_POINT (context);
        }

      if (sort)
        nodeset.SortL ();

      state = 1;
      localci.size = nodeset.GetCount ();

      XP_SEQUENCE_POINT (context);
    }

  unsigned index = state;

  if (index - 1 < nodeset.GetCount ())
    {
      state = index + 1;
      return XPath_Node::IncRef (nodeset.Get (index - 1, reverse));
    }

  return 0;
}

/* virtual */ unsigned
XPath_NodeSetCollector::GetContextSizeL (XPath_Context *context)
{
  return context->nodesets[nodeset_index].GetCount ();
}

/* virtual */ unsigned
XPath_NodeSetCollector::GetProducerFlags ()
{
  /* Inherit everything, and introduce or reverse order if we're sorting or
     reversing. */

  unsigned flags = producer->GetProducerFlags () & MASK_INHERITED;

  /* We're collecting a set, so we'll automatically filter out duplicates. */
  flags |= FLAG_NO_DUPLICATES;

  if (sort)
    flags = (flags & ~MASK_ORDER) | FLAG_DOCUMENT_ORDER;

  if (reverse)
    {
      /* It seems odd to be reversing if we didn't have a specific order to
         begin with. */
      OP_ASSERT ((flags & MASK_ORDER) != 0);

      flags = (flags & ~MASK_ORDER) | ~(flags & MASK_ORDER);
    }

  return flags;
}

XPath_NodeSetFilter::XPath_NodeSetFilter (XPath_Parser *parser, XPath_Producer *producer, unsigned flags)
  : XPath_Filter (producer),
    nodeset_index (parser->GetNodeSetIndex ())
{
  if ((flags & FLAG_SECONDARY_WRAPPER) != 0)
    owns_producer = FALSE;
}

/* virtual */ BOOL
XPath_NodeSetFilter::Reset (XPath_Context *context, BOOL local_context_only)
{
  context->nodesets[nodeset_index].Clear (context);
  return XPath_Filter::Reset (context, local_context_only);
}

/* virtual */ BOOL
XPath_NodeSetFilter::MatchL (XPath_Context *context, XPath_Node *node)
{
  return context->nodesets[nodeset_index].AddL (context, node);
}

#endif // XPATH_SUPPORT
