/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPSTEP_H
#define XPSTEP_H

#include "modules/xpath/src/xpdefs.h"
#include "modules/xpath/src/xpcontext.h"
#include "modules/xpath/src/xpnode.h"
#include "modules/xpath/src/xpexpr.h"
#include "modules/xpath/src/xpunknown.h"

class XPath_LocationPath;

extern BOOL
XPath_CompareNames (const XMLExpandedName &wild, const XMLExpandedName &actual, BOOL case_sensitive);

class XPath_XMLTreeAccessorFilter
{
public:
  XPath_XMLTreeAccessorFilter ();

  void SetFilter (XMLTreeAccessor *tree);

  static XMLExpandedName AdjustNameForFilter (const XMLExpandedName &name);

  enum
    {
      FLAG_HAS_NODETYPE = 1,
      FLAG_HAS_ELEMENT_NAME = 2,
      FLAG_HAS_ATTRIBUTE_NAME = 4,
      FLAG_HAS_ATTRIBUTE_VALUE = 8
    };

  unsigned flags, element_name_data1, element_name_data2, attribute_name_data1, attribute_name_data2;
  XMLTreeAccessor::NodeType nodetype;
  XMLExpandedName element_name;
  XMLExpandedName attribute_name;
  OpString attribute_value;
};

class XPath_Step
{
public:
  class Axis
    : public XPath_ChainProducer
  {
  protected:
    Axis (XPath_Parser *parser, XPath_Producer *producer, XPath_Axis axis);

    XPath_Axis axis;
    XPath_XMLTreeAccessorFilter filter;
    BOOL manual_context_reset;
    /**< If TRUE, GetNextNodeL() will return 0 when the current context "ends"
         until ResetLocalContext() is called (unless the current context node
         produced no nodes, in which case new context nodes will be fetched
         until one is found that produces at least one node.)  The default,
         FALSE, means new context nodes are fetched automatically and
         GetNextNodeL() returns all the nodes that will ever be produced in
         one uninterrupted sequence. */
    BOOL reversed;
    /**< If TRUE, GetNextNodeL() will return its nodes in reverse order
         (locally per context node, that is.)  This is not supported for all
         axes. */
    BOOL skip_descendants;
    /**< Skip descendants of returned nodes.  Used on descendant axis followed
         by another descendant axis (with no context size of position
         dependendat predicates) to avoid double traversal of nodes. */

    unsigned index_index, count_index, skip_descendants_index, node_index, related_index, localci_index;

    BOOL HasOverlappingAxes (BOOL &disordered);
    BOOL IsNodeIncluded (XPath_Node *node);

  public:
    static Axis *MakeL (XPath_Parser *parser, XPath_Producer *producer, XPath_Axis axis);

    /* From XPath_Producer. */
    virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
    virtual XPath_Node *GetNextNodeL (XPath_Context *context);
    virtual void SetManualLocalContextReset (BOOL value);
    virtual unsigned GetProducerFlags ();
    virtual void OptimizeL (unsigned hints);
    virtual BOOL TransformL (XPath_Parser *parser, Transform transform, TransformData &data);

    XPath_Axis GetAxis () { return axis; }
    Axis *GetPreviousAxis () { return static_cast<Axis *> (GetPrevious (PREVIOUS_AXIS, FALSE)); }

    XPath_XMLTreeAccessorFilter *GetFilter () { return &filter; }

    static BOOL IsMutuallyExclusive (Axis *axis1, Axis *axis2);

    static XPath_Producer *GoAway (Axis *axis);

  protected:
    virtual XPath_Producer *GetPreviousInternal (What what, BOOL include_self);
  };

  class NodeTest
    : public XPath_Filter
  {
  protected:
    XPath_NodeTestType type;

  public:
    NodeTest (XPath_Producer *producer, XPath_NodeTestType type);

    XPath_NodeTestType GetType () { return type; }

    /* From XPath_Filter. */
    virtual BOOL MatchL (XPath_Context *context, XPath_Node *node) = 0;

    static XPath_NodeType GetMatchedNodeType (NodeTest *nodetest);
    static BOOL IsMutuallyExclusive (NodeTest *nodetest1, NodeTest *nodetest2);

  protected:
    virtual XPath_Producer *GetPreviousInternal (What what, BOOL include_self);
  };

  class Predicate
    : public XPath_ChainProducer
  {
  protected:
    Predicate (XPath_Parser *parser, XPath_Producer *producer, XPath_Expression *expression);

    XPath_Expression *expression;
    unsigned state_index, node_index, localci_index;
    BOOL is_predicate_expression;

    Axis *GetAxis () { return is_predicate_expression ? 0 : static_cast<Axis *> (producer->GetPrevious (PREVIOUS_AXIS, TRUE)); }

  public:
    static Predicate *MakeL (XPath_Parser *parser, XPath_Producer *producer, XPath_Expression *expression, BOOL is_predicate_expression);

    virtual ~Predicate ();

    virtual unsigned GetPredicateExpressionFlags ();

    virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
    virtual XPath_Node *GetNextNodeL (XPath_Context *context);
    virtual unsigned GetProducerFlags ();

#ifdef XPATH_NODE_PROFILE_SUPPORT
    virtual BOOL TransformL (XPath_Parser *parser, Transform transform, TransformData &data);
#endif // XPATH_NODE_PROFILE_SUPPORT

    enum MatchStatus
      {
        MATCH_NOT_MATCHED,
        /**< Node wasn't matched, try next node. */
        MATCH_LAST_IN_LOCAL_CONTEXT,
        /**< Node was matched, so return it, but it is the last in the
             local context, so no need to try any more nodes. */
        MATCH_MATCHED,
        /**< Node was matched, plain and simple. */
        MATCH_NONE_IN_LOCAL_CONTEXT
        /**< Node wasn't matched, and neither will any other nodes in the
             current local context. */
      };

    virtual MatchStatus MatchL (XPath_Context *context, XPath_Node *node) = 0;

  protected:
    virtual XPath_Producer *GetPreviousInternal (What what, BOOL include_self);
  };
};

class XPath_NodeTypeTest
  : public XPath_Step::NodeTest
{
protected:
  XPath_NodeTypeTest (XPath_Producer *producer, XPath_NodeTestType type = XP_NODETEST_NODETYPE);

  XPath_NodeType nodetype;

public:
  static XPath_NodeTypeTest *MakeL (XPath_Producer *producer, XPath_NodeType nodetype);

  virtual BOOL MatchL (XPath_Context *context, XPath_Node *node);

  virtual BOOL TransformL (XPath_Parser *parser, Transform transform, TransformData &data);

  XPath_NodeType GetNodeType () { return nodetype; }
};

class XPath_NameTest
  : public XPath_NodeTypeTest
{
protected:
  XPath_NameTest (XPath_Producer *producer);

  XMLExpandedName name;

public:
  static XPath_NameTest *MakeL (XPath_Producer *producer, XPath_NodeType nodetype, const XMLExpandedName &name);

  virtual BOOL MatchL (XPath_Context *context, XPath_Node *node);

  virtual BOOL TransformL (XPath_Parser *parser, Transform transform, TransformData &data);

  const XMLExpandedName &GetName () { return name; }
};

class XPath_PITest
  : public XPath_Step::NodeTest
{
protected:
  XPath_PITest (XPath_Producer *producer);

  uni_char *target;

public:
  static XPath_PITest *MakeL (XPath_Producer *producer, const uni_char *target, unsigned length);

  virtual ~XPath_PITest ();
  virtual BOOL MatchL (XPath_Context *context, XPath_Node *node);

  const uni_char *GetTarget () { return target; }
};

#ifdef XPATH_CURRENTLY_UNUSED

/* Non-standard, used when the first predicate is a location path expression
   with a location path containing a single step with the attribute axis and
   a name test. */
class XPath_HasAttributeTest
  : public XPath_Step::NodeTest
{
protected:
  XPath_HasAttributeTest (XPath_Producer *producer);

  XMLExpandedName name;

public:
  static XPath_HasAttributeTest *MakeL (XPath_Producer *producer, const XMLExpandedName &name);

  virtual BOOL MatchL (XPath_Context *context, XPath_Node *node);
};

/* Non-standard, used when the first predicate is an equality expression
   between a literal exression and a location path expression with a
   location path containing a single step with the attribute axis and a
   name test. */
class XPath_AttributeValueTest
  : public XPath_Step::NodeTest
{
protected:
  XPath_AttributeValueTest (XPath_Producer *producer);

  XMLExpandedName name;
  uni_char *value;
  BOOL equal;

public:
  static XPath_AttributeValueTest *MakeL (XPath_Producer *producer, const XMLExpandedName &name, XPath_Value *value, BOOL equal);

  virtual ~XPath_AttributeValueTest ();
  virtual BOOL MatchL (XPath_Context *context, XPath_Node *node);

  const XMLExpandedName &GetName () { return name; }
  const uni_char *GetValue () { return value; }
  BOOL GetEqual () { return equal; }
};

#endif // XPATH_CURRENTLY_UNUSED

class XPath_LastPredicate
  : public XPath_Step::Predicate
{
protected:
  XPath_LastPredicate (XPath_Parser *parser, XPath_Producer *producer);

  unsigned state_index, previous_index;

public:
  static XPath_LastPredicate *MakeL (XPath_Parser *parser, XPath_Producer *producer);

  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
  virtual XPath_Node *GetNextNodeL (XPath_Context *context);
  virtual void OptimizeL (unsigned hints);

  virtual MatchStatus MatchL (XPath_Context *context, XPath_Node *node);
};

class XPath_ConstantPositionPredicate
  : public XPath_Step::Predicate
{
private:
  XPath_ConstantPositionPredicate (XPath_Parser *parser, XPath_Producer *producer, unsigned position);

  unsigned position;

public:
  static XPath_ConstantPositionPredicate *MakeL (XPath_Parser *parser, XPath_Producer *producer, unsigned position);

  virtual void OptimizeL (unsigned hints);

  virtual MatchStatus MatchL (XPath_Context *context, XPath_Node *node);
};

class XPath_RegularPredicate
  : public XPath_Step::Predicate
{
private:
  XPath_BooleanExpression *condition;
  XPath_NumberExpression *position;
  unsigned state_index;

#ifdef XPATH_EXTENSION_SUPPORT
  XPath_Unknown *unknown;
  unsigned unknown_result_type_index;
#endif // XPATH_EXTENSION_SUPPORT

  XPath_RegularPredicate (XPath_Parser *parser, XPath_Producer *producer, XPath_Expression *expression);

public:
  static XPath_RegularPredicate *MakeL (XPath_Parser *parser, XPath_Producer *producer, XPath_Expression *expression);

  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
  virtual void OptimizeL (unsigned hints);

  virtual MatchStatus MatchL (XPath_Context *context, XPath_Node *node);

  virtual BOOL TransformL (XPath_Parser *parser, Transform transform, TransformData &data);
};

class XPath_SingleAttribute
  : public XPath_Step::Axis
{
private:
  XMLExpandedName name;

  XPath_SingleAttribute (XPath_Parser *parser, XPath_Producer *producer);

public:
  static XPath_SingleAttribute *MakeL (XPath_Parser *parser, XPath_Producer *producer, const XMLExpandedName &name);

  virtual XPath_Node *GetNextNodeL (XPath_Context *context);
  virtual unsigned GetProducerFlags ();
  virtual BOOL TransformL (XPath_Parser *parser, Transform transform, TransformData &data);
};

#endif // XPSTEP_H
