/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef XPPATTERN_H
#define XPPATTERN_H

#ifdef XPATH_PATTERN_SUPPORT

#include "modules/xpath/src/xpstep.h"

class XPathNodeProfileImpl;

#ifdef XPATH_EXTENSION_SUPPORT
class XPath_Unknown;
#endif // XPATH_EXTENSION_SUPPORT

class XPath_Pattern
{
public:
  virtual ~XPath_Pattern () {}

  void SetGlobalContext (XPath_GlobalContext *new_global) { global = new_global; }

  virtual BOOL MatchL (XPath_Node *node) = 0;
  virtual void PrepareL (XMLTreeAccessor *tree) = 0;

#ifdef XPATH_NODE_PROFILE_SUPPORT
  virtual XPathPattern::ProfileVerdict GetProfileVerdict (XPathNodeProfileImpl *profile) = 0;
#endif // XPATH_NODE_PROFILE_SUPPORT

  static OP_BOOLEAN Match (unsigned &pattern_index, XPath_Pattern **patterns, unsigned patterns_count, XPath_Node *node);
  static OP_STATUS CountLevelled (unsigned &numbers_count, unsigned *&numbers, BOOL single, XPath_Pattern **from_patterns, unsigned from_patterns_count, XPath_Pattern **count_patterns, unsigned count_patterns_count, XPath_Node *origin, unsigned &failed_pattern_index, unsigned &cost);
  static OP_STATUS CountAny (unsigned &number, XPath_Pattern **from_patterns, unsigned from_patterns_count, XPath_Pattern **count_patterns, unsigned count_patterns_count, XPath_Node *origin, unsigned &failed_pattern_index, unsigned &cost);

protected:
  XPath_GlobalContext *global;
};

class XPath_SimplePattern
  : public XPath_Pattern
{
private:
  class NonFinalNodeStep;
  class Step; // so friend understands who we mean
  friend class Step; // so that it can refer to NonFinalNodeStep

  class Step
  {
  public:
    Step ();
    ~Step ();

    XPath_BooleanExpression **predicates;
    unsigned predicates_count;

    NonFinalNodeStep *previous_step;

    BOOL MatchL (XPath_Context *context, XPath_Node *node);
  };

  class NodeStep
    : public Step
  {
  public:
    NodeStep ();
    ~NodeStep ();

    XPath_XMLTreeAccessorFilter *filter;
    OpString *pitarget;

    BOOL MatchL (XPath_Context *context, XPath_Node *node);
  };

  class NonFinalNodeStep
    : public NodeStep
  {
  public:
    NonFinalNodeStep (XPath_Axis axis);

    XPath_Axis axis;

    BOOL MatchL (XPath_Context *context, XPath_Node *node);
  };

  class FinalNodeStep
    : public NodeStep
  {
  public:
    BOOL MatchL (XPath_Context *context, XPath_Node *node);
#ifdef XPATH_NODE_PROFILE_SUPPORT
    XPathPattern::ProfileVerdict GetProfileVerdict (XPath_XMLTreeAccessorFilter *filter);
#endif // XPATH_NODE_PROFILE_SUPPORT
  };

  class FinalAttributeStep
    : public Step
  {
  public:
    ~FinalAttributeStep ();

    BOOL all;
    XMLExpandedName *name;

    BOOL MatchL (XPath_Context *context, XPath_Node *node);
  };

  FinalNodeStep *nodestep;
  FinalAttributeStep *attributestep;
  BOOL invalid;

  BOOL first, absolute, next_separator_descendant;
  XPath_Axis next_axis;
  XPath_XMLTreeAccessorFilter *next_filter;
  OpString *next_pitarget;
  BOOL next_attribute_all;
  XMLExpandedName *next_attribute_name;
  XPath_Expression **next_predicates;
  unsigned next_predicates_count, next_predicates_total;
  NodeStep *current_step;

  void AddNextStepL (XPath_Parser *parser, BOOL final);

  void SetFilter (XMLTreeAccessor *tree);

public:
  XPath_SimplePattern ();
  virtual ~XPath_SimplePattern ();

  void SetIdArgumentL (const uni_char *argument, unsigned length);
  void SetKeyArgumentsL (const uni_char *argument1, unsigned length1, const uni_char *argument2, unsigned length2);

  void AddSeparatorL (XPath_Parser *parser, BOOL descendant);
  void AddAxisL (XPath_Parser *parser, XPath_Axis axis);
  void AddNodeTypeTestL (XPath_Parser *parser, XPath_NodeType type);
  void AddNameTestL (XPath_Parser *parser, const XMLExpandedName &name);
  void AddPITestL (XPath_Parser *parser, const uni_char *target, unsigned length);
  void AddPredicateL (XPath_Parser *parser, XPath_Expression *predicate);
  void CloseL (XPath_Parser *parser);

  virtual BOOL MatchL (XPath_Node *node);
  virtual void PrepareL (XMLTreeAccessor *tree);

#ifdef XPATH_NODE_PROFILE_SUPPORT
  virtual XPathPattern::ProfileVerdict GetProfileVerdict (XPathNodeProfileImpl *profile);
#endif // XPATH_NODE_PROFILE_SUPPORT
};

class XPath_ComplexPattern
  : public XPath_Pattern
{
public:
  class MatchingNodes
  {
  public:
    class Compound
    {
    public:
      Compound (XMLTreeAccessor::Node *owner)
        : owner (owner),
          attributes (0),
          namespaces (0)
        {
        }

      XMLTreeAccessor::Node *owner;
      OpHashTable *attributes;
      OpHashTable *namespaces;
    };

  private:
    /* Non-attribute/non-namespace nodes. */
    XMLTreeAccessor::Node **simple_hashed;
    unsigned simple_count, simple_capacity;

    void AddSimpleL (XMLTreeAccessor::Node *node);

    /* Attribute and namespace nodes. */
    Compound **compound_hashed;
    unsigned compound_count, compound_capacity;

    Compound **AddCompoundL (XMLTreeAccessor::Node *node);

  public:
    enum State { INITIAL, EVALUATING, FINISHED } state;

    MatchingNodes ();
    ~MatchingNodes ();

    void AddNodeL (XPath_Node *node);

    BOOL Match (XPath_Node *node);

    MatchingNodes *next;
  };

  XPath_ComplexPattern (XPath_Producer *producer)
    : producer (producer)
    {
    }

  virtual ~XPath_ComplexPattern ();

  virtual BOOL MatchL (XPath_Node *node);
  virtual void PrepareL (XMLTreeAccessor *tree);

#ifdef XPATH_NODE_PROFILE_SUPPORT
  virtual XPathPattern::ProfileVerdict GetProfileVerdict (XPathNodeProfileImpl *profile);
#endif // XPATH_NODE_PROFILE_SUPPORT

private:
  XPath_Producer *producer;
};

class XPath_PatternContext
{
public:
  XPath_PatternContext ()
    : matching_nodes_per_pattern (0)
  {
  }

  ~XPath_PatternContext ();

  void InvalidateTree (XMLTreeAccessor *tree);

  XPath_ComplexPattern::MatchingNodes *FindMatchingNodes (XPath_ComplexPattern *pattern, XMLTreeAccessor *tree);
  void AddMatchingNodesL (XPath_ComplexPattern *pattern, XMLTreeAccessor *tree, XPath_ComplexPattern::MatchingNodes *matching_nodes);

private:
  class MatchingNodesPerPattern
  {
  public:
    class MatchingNodesPerTree
    {
    public:
      XMLTreeAccessor *tree;
      XPath_ComplexPattern::MatchingNodes *matching_nodes;
      MatchingNodesPerTree *next;
    };

    XPath_ComplexPattern *pattern;
    MatchingNodesPerTree *matching_nodes_per_tree;
    MatchingNodesPerPattern *next;
  };

  MatchingNodesPerPattern *matching_nodes_per_pattern;
};

#endif // XPPATTERN_H
#endif // XPATH_PATTERN_SUPPORT
