/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPPRODUCER_H
#define XPPRODUCER_H

#include "modules/xpath/src/xpdefs.h"
#include "modules/xpath/src/xpcontext.h"
#include "modules/xpath/src/xpnode.h"
#include "modules/xpath/src/xpnodeset.h"
#include "modules/xpath/src/xpnodelist.h"

#include "modules/util/OpHashTable.h"

class XPath_Parser;
class XPath_XMLTreeAccessorFilter;

class XPath_Producer
{
public:
  XPath_Producer ();

  virtual ~XPath_Producer ();

  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only = FALSE) = 0;
  /**< Reset a chain of producers.  The context data is typically ignored by
       all but the first producer, typically an
       XPath_InitialContextProducer. */

  virtual XPath_Node *GetNextNodeL (XPath_Context *context) = 0;
  /**< Produce a new node.  If NULL is returned, there were no more nodes. */

  OP_BOOLEAN GetNextNode (XPath_Node *&node, XPath_Context *context);
  /**< TRAP:s a call to GetNextNodeL. */

  enum Type
    {
      TYPE_SINGLE,
      TYPE_AXIS,
      TYPE_NODETEST,
      TYPE_PREDICATE,
      TYPE_OTHER
    };

  virtual Type GetProducerType ();

  enum Flags
    {
      FLAG_DOCUMENT_ORDER = 1,
      /**< Produced nodes are in document order. */

      FLAG_REVERSE_DOCUMENT_ORDER = 2,
      /**< Produced nodes are in reverse document order. */

      FLAG_NO_DUPLICATES = 4,
      /**< Guaranteed not to produce the same node twice. */

      FLAG_KNOWN_SIZE = 8,
      /**< Has known context size. */

      FLAG_SINGLE_NODE = 16,
      /**< The producer produces only a single node.  When combined with
           FLAG_DOCUMENT_ORDER, means first node in document order.  When
           combined with FLAG_REVERSE_DOCUMENT_ORDER means last node in
           document order. */

      FLAG_CONTEXT_SIZE = 32,
      /**< Needs context size. */

      FLAG_CONTEXT_POSITION = 64,
      /**< Needs context position. */

      FLAG_SECONDARY_WRAPPER = 128,
      /**< Signal to EnsureFlagsL() that the wrapper created is "secondary,"
           meaning it doesn't own the wrapped producer.  Also means that
           EnsureFlagsL() won't deallocate the to-be-wrapped producer on
           failure. */

      FLAG_UNKNOWN = 256,
      /**< Instance of XPath_Unknown. */

      FLAG_BLOCKING = 512,

      FLAG_LOCAL_CONTEXT_POSITION = 1024,
      /**< Used in EnsureFlagsL to request a producer that establishes
           its own "context position" scope that only the caller will
           modify. */

      MASK_ORDER = FLAG_DOCUMENT_ORDER | FLAG_REVERSE_DOCUMENT_ORDER,
      /**< Either document order or reverse document order. */

      MASK_NEEDS = FLAG_CONTEXT_SIZE | FLAG_CONTEXT_POSITION,
      /**< Whether the producer needs context size or position. */

      MASK_INHERITED = ~(FLAG_SECONDARY_WRAPPER | FLAG_UNKNOWN)
    };

  virtual unsigned GetProducerFlags ();

  unsigned GetExpressionFlags ();
  /**< Translates producer flags to corresponding expression flags. */

  enum What
    {
      PREVIOUS_ANY,
      /**< Previous producer. */
      PREVIOUS_CONTEXT_NODE,
      /**< Previous initial context producer. */
      PREVIOUS_AXIS,
      /**< Previous producer that is an XPath_Step::Axis. */
      PREVIOUS_NODETEST,
      /**< Previous producer that is an XPath_Step::NodeTest. */
      PREVIOUS_PREDICATE
      /**< Previous producer that is an XPath_Step::Predicate. */
    };

  XPath_Producer *GetPrevious (What what = PREVIOUS_ANY, BOOL include_self = FALSE);
  /**< Never returns this producer. */

  enum Hint
    {
      HINT_CONTEXT_DEPENDENT_PREDICATE = 1
      /**< There's a context size or position (but not node!) dependent
           predicate between here and the previous XPath_Step::Axis.  Set by
           the predicate in question, and cleared by XPath_Step::Axis when
           calling its base producer.  Certain simplifications of a location
           path can be performed if no such predicates exist, such as

             descendant-or-self::node()/child::<nodetest>

           to

             descendant::<nodetest>

           and

             descendant::node()/descendant::<nodetest>

           to

             descendant::node()/child::<nodetest>

           The former simplification is important because the first step is
           very common (it is what the "//" abbreviation expands to) and the
           location path unmodified returns a sequence of nodes where
           duplicates occur and that is unordered if those duplicates are
           filtered out. */
    };

  virtual void OptimizeL (unsigned hints);

  enum Transform
    {
      TRANSFORM_XMLTREEACCESSOR_FILTER,
      /**< Updates TransformData::filter.  Typically handled by simple
           XPath_Filter implementations.  Not destructive. */

      TRANSFORM_ATTRIBUTE_NAME,
      /**< Updates TransformData::name.  Handled by XPath_SingleAttribute. */

#ifdef XPATH_NODE_PROFILE_SUPPORT
      TRANSFORM_NODE_PROFILE,
      /**< Sets TransformData::nodeprofile to an allocated array of filters
           matching ALL nodes that this producer can return and returns TRUE,
           if that is possible.  Otherwise, returns FALSE.  Not destructive.
           The actual filters referenced from the array are owned by the
           producer (directly or indirectly) and the array is owned by the
           caller. */
#endif // XPATH_NODE_PROFILE_SUPPORT

      TRANSFORM_REVERSE_AXIS,
      /**< If the nearest axis can be reversed, it is, and TRUE is
           returned.  Otherwise, FALSE is returned. */

      LAST_TRANSFORM
    };

  union TransformData
  {
  public:
    /* For TRANSFORM_XMLTREEACCESSOR_FILTER: */
    struct Filter
    {
      XPath_XMLTreeAccessorFilter *filter;
      bool partial;
    } filter;

    /* For TRANSFORM_ATTRIBUTE_NAME: */
    const XMLExpandedName *name;

#ifdef XPATH_NODE_PROFILE_SUPPORT
    /* For TRANSFORM_NODE_PROFILE: */
    struct NodeProfile
    {
      XPath_XMLTreeAccessorFilter **filters;
      unsigned filters_count:28;
      unsigned includes_regulars:1;
      unsigned includes_roots:1;
      unsigned includes_attributes:1;
      unsigned includes_namespaces:1;
      unsigned filtered:1;
    } nodeprofile;
#endif // XPATH_NODE_PROFILE_SUPPORT
  };

  virtual BOOL TransformL (XPath_Parser *parser, Transform transform, TransformData &data);

  unsigned ci_index;

  static XPath_Producer *EnsureFlagsL (XPath_Parser *parser, XPath_Producer *producer, unsigned flags);
  /**< Returns a producer that produces the same nodes as 'producer'
       and whose GetProducerFlags() method returns 'flags' (or better.) */

protected:
  virtual XPath_Producer *GetPreviousInternal (What what, BOOL include_self);

  XPath_Producer *CallGetPreviousInternalOnOther (XPath_Producer *other, What what, BOOL include_self);
};

class XPath_EmptyProducer
  : public XPath_Producer
{
protected:
  unsigned localci_index;

public:
  XPath_EmptyProducer (XPath_Parser *parser);

  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
  virtual XPath_Node *GetNextNodeL (XPath_Context *context);
  virtual unsigned GetProducerFlags ();
};

class XPath_InitialContextProducer
  : public XPath_Producer
{
protected:
  BOOL absolute;

  unsigned state_index, node_index, localci_index;

public:
  XPath_InitialContextProducer (XPath_Parser *parser, BOOL absolute);

  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
  /**< Releases the previous context data, and copies 'data'. */

  virtual XPath_Node *GetNextNodeL (XPath_Context *context);
  virtual unsigned GetContextSizeL (XPath_Context *context);

  virtual unsigned GetProducerFlags ();

protected:
  virtual XPath_Producer *GetPreviousInternal (What what, BOOL include_self);
};

class XPath_ChainProducer
  : public XPath_Producer
{
protected:
  XPath_Producer *producer;
  BOOL owns_producer;

public:
  XPath_ChainProducer (XPath_Producer *producer);

  virtual ~XPath_ChainProducer ();

  void ResetProducer () { producer = 0; }

  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
  /**< Default implementation simply propagates the call to the previous
       producer. */

  virtual XPath_Node *GetNextNodeL (XPath_Context *context);

  virtual unsigned GetProducerFlags ();
  /**< Default implementation simply propagates the call to the previous
       producer. */

  virtual void OptimizeL (unsigned hints);
  /**< Default implementation simply propagates the call to the previous
       producer. */

  virtual BOOL TransformL (XPath_Parser *parser, Transform transform, TransformData &data);
  /**< Default implementation propagates the call to the previous producer if
       'transform == TRANSFORM_NODE_PROFILE'. */

protected:
  virtual XPath_Producer *GetPreviousInternal (What what, BOOL include_self);
  /**< Default implementation simply propagates the call to the
       previous producer. */
};

class XPath_LocalContextPositionProducer
  : public XPath_ChainProducer
{
protected:
  unsigned localci_index;

public:
  XPath_LocalContextPositionProducer (XPath_Parser *parser, XPath_Producer *producer, unsigned flags);

  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
};

class XPath_SingleNodeProducer
  : public XPath_ChainProducer
{
protected:
  unsigned flags, state_index, node_index, localci_index;

public:
  XPath_SingleNodeProducer (XPath_Parser *parser, XPath_Producer *producer, unsigned flags);

  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
  virtual XPath_Node *GetNextNodeL (XPath_Context *context);
  virtual unsigned GetProducerFlags ();
};

class XPath_Filter
  : public XPath_ChainProducer
{
public:
  XPath_Filter (XPath_Producer *producer);

  virtual XPath_Node *GetNextNodeL (XPath_Context *context);

  virtual BOOL MatchL (XPath_Context *context, XPath_Node *node) = 0;

  virtual unsigned GetProducerFlags ();
  /**< Default implementation propagates the calls to the previous
       producer but removes FLAG_KNOWN_SIZE, since we typically don't
       how many of the filtered nodes the filter will match. */

#ifdef XPATH_NODE_PROFILE_SUPPORT
  virtual BOOL TransformL (XPath_Parser *parser, Transform transform, TransformData &data);
#endif // XPATH_NODE_PROFILE_SUPPORT
};

class XPath_NodeListCollector
  : public XPath_ChainProducer
{
protected:
  BOOL sort, reverse;

public:
  XPath_NodeListCollector (XPath_Parser *parser, XPath_Producer *producer, unsigned flags);

  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
  virtual XPath_Node *GetNextNodeL (XPath_Context *context);
  virtual unsigned GetContextSizeL (XPath_Context *context);

  virtual unsigned GetProducerFlags ();

  void CollectL (XPath_Context *context);

  unsigned state_index, nodelist_index, localci_index;
};

class XPath_NodeSetCollector
  : public XPath_ChainProducer
{
protected:
  BOOL sort, reverse;

public:
  XPath_NodeSetCollector (XPath_Parser *parser, XPath_Producer *producer, unsigned flags);

  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
  virtual XPath_Node *GetNextNodeL (XPath_Context *context);
  virtual unsigned GetContextSizeL (XPath_Context *context);

  virtual unsigned GetProducerFlags ();

  void CollectL (XPath_Context *context);

  unsigned state_index, nodeset_index, localci_index;
};

class XPath_NodeSetFilter
  : public XPath_Filter
{
private:
  unsigned nodeset_index;

public:
  XPath_NodeSetFilter (XPath_Parser *parser, XPath_Producer *producer, unsigned flags);

  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
  virtual BOOL MatchL (XPath_Context *context, XPath_Node *node);
};

#endif // XPPRODUCER_H
