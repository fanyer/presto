/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_PATTERN_SUPPORT

#include "modules/xpath/src/xppattern.h"
#include "modules/xpath/src/xputils.h"
#include "modules/xpath/src/xpparser.h"
#include "modules/xpath/src/xpunknown.h"
#include "modules/xpath/src/xpapiimpl.h"

static BOOL
XPath_MatchPatternsL (XPath_Pattern **patterns, unsigned patterns_count, unsigned &matched_index, XPath_Node *node)
{
  /* Since we will be using it with all sorts of contexts, it would be
     strange if it was owned by one. */
  OP_ASSERT (node->IsTemporary () || node->IsIndependent ());

  for (matched_index = 0; matched_index < patterns_count; ++matched_index)
    if (patterns[matched_index]->MatchL (node))
      return TRUE;

  return FALSE;
}

static BOOL
XPath_EquivalentNode (XPath_Node *origin, XPath_Node *node)
{
  if (origin->type == node->type)
    switch (origin->type)
      {
      case XP_NODE_ROOT:
      case XP_NODE_COMMENT:
      case XP_NODE_TEXT:
        return TRUE;

      default:
        return XPath_Node::SameExpandedName (origin, node);
      }
  else
    return FALSE;
}

static unsigned *
XPath_AddNumberL (unsigned &numbers_count, unsigned *&numbers)
{
  if ((numbers_count ^ (numbers_count - 1)) == (numbers_count | (numbers_count - 1)))
    {
      unsigned *new_numbers = OP_NEWA_L (unsigned, numbers_count ? numbers_count + numbers_count : 1);
      op_memcpy (new_numbers, numbers, numbers_count * sizeof (unsigned));
      OP_DELETEA (numbers);
      numbers = new_numbers;
    }

  unsigned *number = &numbers[numbers_count++];
  *number = 1;
  return number;
}

/* static */ OP_BOOLEAN
XPath_Pattern::Match (unsigned &pattern_index, XPath_Pattern **patterns, unsigned patterns_count, XPath_Node *node)
{
  OP_MEMORY_VAR BOOL matched;
  TRAPD (status, matched = XPath_MatchPatternsL (patterns, patterns_count, pattern_index, node));
  return OpStatus::IsSuccess (status) ? (matched ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE) : status;
}

static void
XPath_PatternCountLevelledL (unsigned &numbers_count, unsigned *&numbers, BOOL single, XPath_Pattern **from_patterns, unsigned from_patterns_count, XPath_Pattern **count_patterns, unsigned count_patterns_count, XPath_Node *origin, unsigned &failed_pattern_index, unsigned cost)
{
#define MATCH_FROM(node) (++cost, (from_patterns_count != 0 && XPath_MatchPatternsL (from_patterns, from_patterns_count, failed_pattern_index, node)))
#define MATCH_COUNT(node) (++cost, (count_patterns_count != 0 ? XPath_MatchPatternsL (count_patterns, count_patterns_count, failed_pattern_index, node) : origin == node || XPath_EquivalentNode (origin, node)))
#define ADD_NUMBER() (current = XPath_AddNumberL (numbers_count, numbers))
#define INC_NUMBER() (++*current)

  XMLTreeAccessor *tree = origin->tree;
  XMLTreeAccessor::Node *treenode = origin->treenode;

  numbers_count = 0;
  numbers = 0;
  cost = 0;

  unsigned *current = 0;

  if ((origin->type == XP_NODE_ATTRIBUTE || origin->type == XP_NODE_NAMESPACE))
    {
      if (MATCH_COUNT (origin))
        {
          /* Will be 1 for sure, since 'origin' has no siblings. */
          ADD_NUMBER ();

          if (single)
            return;
        }

      XPath_Node temporary (tree, treenode);

      if (MATCH_FROM (&temporary))
        return;
    }

  /* We start by considering 'origin'.  If 'origin' was an attribute or
     namespace node, we're actually starting with its parent node, but then
     the case above will have handled counting it and checking whether it
     matches the 'from' patterns. */

  while (TRUE)
    {
      XPath_Node temporary (tree, treenode);

      if (MATCH_COUNT (&temporary))
        {
          ADD_NUMBER ();

          XMLTreeAccessor::Node *sibling = tree->GetPreviousSibling (treenode);

          while (sibling)
            {
              XPath_Node temporary (tree, sibling);

              if (MATCH_COUNT (&temporary))
                INC_NUMBER ();

              sibling = tree->GetPreviousSibling (sibling);
            }

          if (single)
            return;
        }

      treenode = tree->GetParent (treenode);

      if (treenode)
        {
          temporary.Set (tree, treenode);

          if (MATCH_FROM (&temporary))
            return;
        }
      else
        return;
    }

#undef MATCH_COUNT
#undef MATCH_FROM
}

/* static */ OP_STATUS
XPath_Pattern::CountLevelled (unsigned &numbers_count, unsigned *&numbers, BOOL single, XPath_Pattern **from_patterns, unsigned from_patterns_count, XPath_Pattern **count_patterns, unsigned count_patterns_count, XPath_Node *origin, unsigned &failed_pattern_index, unsigned &cost)
{
  TRAPD (status, XPath_PatternCountLevelledL (numbers_count, numbers, single, from_patterns, from_patterns_count, count_patterns, count_patterns_count, origin, failed_pattern_index, cost));
  return status;
}

static void
XPath_PatternCountAnyL (unsigned &number, XPath_Pattern **from_patterns, unsigned from_patterns_count, XPath_Pattern **count_patterns, unsigned count_patterns_count, XPath_Node *origin, unsigned &failed_pattern_index, unsigned &cost)
{
#define MATCH_FROM(node) (from_patterns_count != 0 && (++cost, XPath_MatchPatternsL (from_patterns, from_patterns_count, failed_pattern_index, node)))
#define MATCH_COUNT(node) (count_patterns_count != 0 ? (++cost, XPath_MatchPatternsL (count_patterns, count_patterns_count, failed_pattern_index, node)) : origin == node || XPath_EquivalentNode (origin, node))

  /* FIXME: This implementation could/should use XPath_Scan to move most of
     the traversal closer to the tree accessor. */

  XMLTreeAccessor *tree = origin->tree;
  XMLTreeAccessor::Node *treenode = origin->treenode;

  number = 0;
  cost = 0;

  if ((origin->type == XP_NODE_ATTRIBUTE || origin->type == XP_NODE_NAMESPACE) && MATCH_COUNT (origin))
    {
      ++number;

      XPath_Node temporary (tree, treenode);

      if (MATCH_FROM (&temporary))
        return;
    }

  while (TRUE)
    {
      XPath_Node temporary (tree, treenode);

      if (MATCH_COUNT (&temporary))
        ++number;

      treenode = tree->GetPrevious (treenode);

      if (treenode)
        {
          temporary.Set (tree, treenode);

          if (MATCH_FROM (&temporary))
            return;
        }
      else
        return;
    }

#undef MATCH_COUNT
#undef MATCH_FROM
}

/* static */ OP_STATUS
XPath_Pattern::CountAny (unsigned &number, XPath_Pattern **from_patterns, unsigned from_patterns_count, XPath_Pattern **count_patterns, unsigned count_patterns_count, XPath_Node *origin, unsigned &failed_pattern_index, unsigned &cost)
{
  TRAPD (status, XPath_PatternCountAnyL (number, from_patterns, from_patterns_count, count_patterns, count_patterns_count, origin, failed_pattern_index, cost));
  return status;
}

XPath_SimplePattern::Step::Step ()
  : predicates (0),
    predicates_count (0),
    previous_step (0)
{
}

XPath_SimplePattern::Step::~Step ()
{
  for (unsigned index = 0; index < predicates_count; ++index)
    OP_DELETE (predicates[index]);
  OP_DELETEA (predicates);
  OP_DELETE (previous_step);
}

BOOL
XPath_SimplePattern::Step::MatchL (XPath_Context *context0, XPath_Node *node)
{
#ifdef _DEBUG
  BOOL result;
  for (unsigned index = 0; index < predicates_count; ++index)
    {
      XPath_Context context (context0, node, 0, 0);
      context.iterations = ~0u;
      TRAPD (status, result = predicates[index]->EvaluateToBooleanL (&context, TRUE));
      /* This is supposed to be uninterruptible! */
      OP_ASSERT (status == OpStatus::OK || status == OpStatus::ERR || status == OpStatus::ERR_NO_MEMORY);
      if (status != OpStatus::OK)
        LEAVE (status);
      if (!result)
        return result;
    }
#else // _DEBUG
  for (unsigned index = 0; index < predicates_count; ++index)
    {
      XPath_Context context (context0, node, 0, 0);
      context.iterations = ~0u;
      if (!predicates[index]->EvaluateToBooleanL (&context, TRUE))
        return FALSE;
    }
#endif // _DEBUG

  return !previous_step || previous_step->MatchL (context0, node);
}

XPath_SimplePattern::NodeStep::NodeStep ()
  : filter (0),
    pitarget (0)
{
}

XPath_SimplePattern::NodeStep::~NodeStep ()
{
  OP_DELETE (filter);
  OP_DELETE (pitarget);
}

BOOL
XPath_SimplePattern::NodeStep::MatchL (XPath_Context *context, XPath_Node *node)
{
  if (pitarget)
    {
      /* An XMLTreeAccessor filter should have caught this earlier. */
      OP_ASSERT (node->type == XP_NODE_PI);

      if (pitarget->Compare (node->tree->GetPITarget (node->treenode)) != 0)
        return FALSE;
    }

  return Step::MatchL (context, node);
}

XPath_SimplePattern::NonFinalNodeStep::NonFinalNodeStep (XPath_Axis axis)
  : axis (axis)
{
}

BOOL
XPath_SimplePattern::NonFinalNodeStep::MatchL (XPath_Context *context, XPath_Node *node)
{
  XMLTreeAccessor *tree = node->tree;
  XMLTreeAccessor::Node *treenode = node->treenode;

 again:
  if (filter)
    filter->SetFilter (tree);

  if (node->type != XP_NODE_ATTRIBUTE && node->type != XP_NODE_NAMESPACE)
    if (axis == XP_AXIS_PARENT)
      treenode = tree->GetParent (treenode);
    else
      treenode = tree->GetAncestor (treenode);
  else if (!tree->FilterNode (treenode))
    if (axis == XP_AXIS_PARENT)
      treenode = 0;
    else
      treenode = tree->GetAncestor (treenode);

  if (filter)
    tree->ResetFilters ();

  if (treenode)
    {
      XPath_Node temporary (tree, treenode);

      if (NodeStep::MatchL (context, &temporary))
        return TRUE;
      else if (axis == XP_AXIS_ANCESTOR)
        goto again;
    }

  return FALSE;
}

BOOL
XPath_SimplePattern::FinalNodeStep::MatchL (XPath_Context *context, XPath_Node *node)
{
  XMLTreeAccessor *tree = node->tree;

  if (filter)
    {
      filter->SetFilter (tree);
      BOOL included = node->tree->FilterNode (node->treenode);
      tree->ResetFilters ();
      if (!included)
        return FALSE;
    }

  return NodeStep::MatchL (context, node);
}

#ifdef XPATH_NODE_PROFILE_SUPPORT

static XPathPattern::ProfileVerdict
XPath_GetProfileVerdict (const XMLExpandedName &profile_name, const XMLExpandedName &pattern_name)
{
  if (profile_name != pattern_name)
    if (profile_name.GetUri () == pattern_name.GetUri () || profile_name.GetUri () && pattern_name.GetUri () && uni_strcmp (profile_name.GetUri (), pattern_name.GetUri ()) == 0)
      {
        /* Same namespace URI or both null namespace: */
        if (!*profile_name.GetLocalPart ())
          /* Profile's name-test is 'ns:*' or '*', and patterns is specific (or
             they would have compared equal.)  Profile can include nodes the
             pattern doesn't match. */
          return XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES;
        else if (!*pattern_name.GetLocalPart ())
          /* Pattern's name-test is 'ns:*' or '*', and profile is specific.
             Profile can't include nodes the pattern doesn't match. */
          return XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES;
        else
          /* Both profile and pattern tests for specific element names, and they
             weren't the same.  Pattern cannot match profiled nodes. */
          return XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES;
      }
    else
      /* Different namespace URI:s in name-tests => incompatible names. */
      return XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES;
  else
    /* Equal name-tests.  Could be they are both 'ns:*' (for the same namespace)
       or '*', but in that case the pattern still matches anything the profile
       can throw at it. */
    return XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES;
}

static XPathPattern::ProfileVerdict
XPath_GetProfileVerdict (XPath_XMLTreeAccessorFilter *profile_filter, XPath_XMLTreeAccessorFilter *pattern_filter)
{
  unsigned flags = profile_filter->flags & pattern_filter->flags;

  if ((flags & XPath_XMLTreeAccessorFilter::FLAG_HAS_NODETYPE) != 0)
    {
      /* Both test node type. */
      if (profile_filter->nodetype != pattern_filter->nodetype)
        /* But not the same type! */
        return XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES;
      else if (profile_filter->nodetype == XMLTreeAccessor::TYPE_ELEMENT)
        {
          /* Both include only elements. */

          BOOL will_match = TRUE;

          if ((flags & XPath_XMLTreeAccessorFilter::FLAG_HAS_ELEMENT_NAME) != 0)
            /* Both check element name: */
            switch (XPath_GetProfileVerdict (profile_filter->element_name, pattern_filter->element_name))
              {
              case XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES:
                return XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES;

              case XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES:
                will_match = FALSE;
              }
          else if ((pattern_filter->flags & XPath_XMLTreeAccessorFilter::FLAG_HAS_ELEMENT_NAME) != 0)
            /* Profile doesn't check element name, pattern matches more or less
               specific element names only. */
            will_match = FALSE;

          if ((pattern_filter->flags & (XPath_XMLTreeAccessorFilter::FLAG_HAS_ATTRIBUTE_NAME | XPath_XMLTreeAccessorFilter::FLAG_HAS_ATTRIBUTE_VALUE)) != 0)
            /* Pattern excludes elements based on existance named attribute
               and/or value of that/some attribute.  We could be more accurate
               here, obviously. */
            will_match = FALSE;

          return will_match ? XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES : XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES;
        }
      else if (profile_filter->nodetype == XMLTreeAccessor::TYPE_TEXT || profile_filter->nodetype == XMLTreeAccessor::TYPE_COMMENT)
        return XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES;
    }

  /* This is the safe answer: might match, might not. */
  return XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES;
}

XPathPattern::ProfileVerdict
XPath_SimplePattern::FinalNodeStep::GetProfileVerdict (XPath_XMLTreeAccessorFilter *profile_filter)
{
  XPathPattern::ProfileVerdict verdict = filter ? XPath_GetProfileVerdict (profile_filter, filter) : XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES;

  if (verdict == XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES && predicates_count == 0 && !previous_step)
    return XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES;
  else if (verdict != XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES)
    return XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES;
  else
    return XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES;
}

#endif // XPATH_NODE_PROFILE_SUPPORT

XPath_SimplePattern::FinalAttributeStep::~FinalAttributeStep ()
{
  OP_DELETE (name);
}

BOOL
XPath_SimplePattern::FinalAttributeStep::MatchL (XPath_Context *context, XPath_Node *node)
{
  if (all || XPath_CompareNames (*name, node->name, node->tree->IsCaseSensitive ()))
    return Step::MatchL (context, node);
  else
    return FALSE;
}

void
XPath_SimplePattern::AddNextStepL (XPath_Parser *parser, BOOL final)
{
  Step *step;

  if (next_axis == XP_AXIS_SELF)
    return;

  if (next_axis == XP_AXIS_CHILD)
    {
      if (current_step)
        {
          if (!current_step->filter)
            current_step->filter = OP_NEW_L (XPath_XMLTreeAccessorFilter, ());

          current_step->filter->flags |= XPath_XMLTreeAccessorFilter::FLAG_HAS_NODETYPE;
          current_step->filter->nodetype = XMLTreeAccessor::TYPE_ELEMENT;
        }
      else if (absolute)
        {
          current_step = static_cast<NodeStep *> (OP_NEW_L (NonFinalNodeStep, (XP_AXIS_PARENT)));
          current_step->filter = OP_NEW_L (XPath_XMLTreeAccessorFilter, ());
          current_step->filter->flags = XPath_XMLTreeAccessorFilter::FLAG_HAS_NODETYPE;
          current_step->filter->nodetype = XMLTreeAccessor::TYPE_ROOT;
        }

      NodeStep *nstep = final ? static_cast<NodeStep *> (OP_NEW_L (FinalNodeStep, ())) : static_cast<NodeStep *> (OP_NEW_L (NonFinalNodeStep, (next_separator_descendant ? XP_AXIS_ANCESTOR : XP_AXIS_PARENT)));

      nstep->filter = next_filter;
      next_filter = 0;

      nstep->pitarget = next_pitarget;
      next_pitarget = 0;

      nstep->previous_step = static_cast<NonFinalNodeStep *> (current_step);

      if (final)
        {
          step = nodestep = static_cast<FinalNodeStep *> (nstep);
          current_step = 0;
        }
      else
        step = current_step = nstep;
    }
  else if (!final || absolute && !current_step)
    {
      invalid = TRUE;
      return;
    }
  else
    {
      FinalAttributeStep *astep = OP_NEW_L (FinalAttributeStep, ());

      astep->all = next_attribute_all;
      next_attribute_all = FALSE;

      astep->name = next_attribute_name;
      next_attribute_name = 0;

      astep->previous_step = static_cast<NonFinalNodeStep *> (current_step);
      step = attributestep = astep;
      current_step = 0;
    }

  if (next_predicates_count != 0)
    {
      step->predicates = OP_NEWA_L (XPath_BooleanExpression *, next_predicates_count);
      step->predicates_count = next_predicates_count;

      unsigned index;

      for (index = 0; index < next_predicates_count; ++index)
        step->predicates[index] = 0;

      for (index = 0; index < next_predicates_count; ++index)
        {
          XPath_Expression *expression = next_predicates[index];
          next_predicates[index] = 0;

          /* We know it should be treated as a boolean expression, because if
             it had been a number expression (or an unknown expression) we
             would have fallen back to creating a XPath_ComplexPattern
             instead. */
          step->predicates[index] = XPath_BooleanExpression::MakeL (parser, expression);
        }

      next_predicates_count = 0;
    }
}

void
XPath_SimplePattern::SetFilter (XMLTreeAccessor *tree)
{
  if (nodestep && nodestep->filter)
    nodestep->filter->SetFilter (tree);
}

XPath_SimplePattern::XPath_SimplePattern ()
  : nodestep (0),
    attributestep (0),
    invalid (FALSE),
    first (TRUE),
    absolute (FALSE),
    next_separator_descendant (FALSE),
    next_axis (XP_AXIS_SELF),
    next_filter (0),
    next_pitarget (0),
    next_attribute_all (TRUE),
    next_attribute_name (0),
    next_predicates (0),
    next_predicates_count (0),
    next_predicates_total (0),
    current_step (0)
{
}

XPath_SimplePattern::~XPath_SimplePattern ()
{
  OP_DELETE (nodestep);
  OP_DELETE (attributestep);
  OP_DELETE (next_filter);
  OP_DELETE (next_pitarget);
  OP_DELETE (next_attribute_name);
  for (unsigned index = 0; index < next_predicates_count; ++index)
    OP_DELETE (next_predicates[index]);
  OP_DELETEA (next_predicates);
  OP_DELETE (current_step);
}

void
XPath_SimplePattern::AddSeparatorL (XPath_Parser *parser, BOOL descendant)
{
  next_separator_descendant = descendant;

  if (!first)
    AddNextStepL (parser, FALSE);
  else
    absolute = !descendant;

  first = FALSE;
  next_separator_descendant = FALSE;
  next_axis = XP_AXIS_SELF;
  next_filter = 0;
  next_pitarget = 0;
  next_predicates_count = 0;
}

void
XPath_SimplePattern::AddAxisL (XPath_Parser *parser, XPath_Axis axis)
{
  first = FALSE;

  if (absolute && !next_separator_descendant && !current_step)
    {
      OP_ASSERT (next_axis == XP_AXIS_SELF);
      AddNextStepL (parser, FALSE);
    }

  OP_ASSERT (axis == XP_AXIS_CHILD || axis == XP_AXIS_ATTRIBUTE);
  next_axis = axis;
}

void
XPath_SimplePattern::AddNodeTypeTestL (XPath_Parser *parser, XPath_NodeType type)
{
  if (next_axis == XP_AXIS_CHILD)
    {
      next_filter = OP_NEW_L (XPath_XMLTreeAccessorFilter, ());
      next_filter->flags = XPath_XMLTreeAccessorFilter::FLAG_HAS_NODETYPE;
      next_filter->nodetype = XPath_Utils::ConvertNodeType (type);
    }
  else if (type != XP_NODE_ATTRIBUTE)
    invalid = TRUE;
}

void
XPath_SimplePattern::AddNameTestL (XPath_Parser *parser, const XMLExpandedName &name)
{
  BOOL all = !name.GetUri () && name.GetLocalPart ()[0] == '*';
  if (next_axis == XP_AXIS_CHILD)
    {
      next_filter = OP_NEW_L (XPath_XMLTreeAccessorFilter, ());
      next_filter->flags = XPath_XMLTreeAccessorFilter::FLAG_HAS_NODETYPE;
      next_filter->nodetype = XMLTreeAccessor::TYPE_ELEMENT;
      if (!all)
        {
          next_filter->flags |= XPath_XMLTreeAccessorFilter::FLAG_HAS_ELEMENT_NAME;

          if (name.GetLocalPart ()[0] == '*' && name.GetLocalPart ()[1] == 0)
            {
              XMLExpandedName filter_name (name.GetUri (), UNI_L (""));
              next_filter->element_name.SetL (filter_name);
            }
          else
            next_filter->element_name.SetL (name);
        }
    }
  else
    {
      next_attribute_all = all;
      if (!next_attribute_all)
        {
          next_attribute_name = OP_NEW_L (XMLExpandedName, ());
          next_attribute_name->SetL (name);
        }
    }
}

void
XPath_SimplePattern::AddPITestL (XPath_Parser *parser, const uni_char *target, unsigned length)
{
  if (next_axis != XP_AXIS_CHILD)
    invalid = TRUE;
  else
    {
      AddNodeTypeTestL (parser, XP_NODE_PI);

      if (target)
        {
          next_pitarget = OP_NEW_L (OpString, ());
          next_pitarget->SetL (target, length);
        }
    }
}

void
XPath_SimplePattern::AddPredicateL (XPath_Parser *parser, XPath_Expression *predicate)
{
  OpStackAutoPtr<XPath_Expression> predicate_anchor (predicate);
  BOOL temporary_filter;

  if (!next_filter)
    {
      next_filter = OP_NEW_L (XPath_XMLTreeAccessorFilter, ());
      temporary_filter = TRUE;
    }
  else
    temporary_filter = FALSE;

  XPath_Expression::TransformData data;
  data.filter.filter = next_filter;
  data.filter.partial = FALSE;

  if (predicate->TransformL (parser, XPath_Expression::TRANSFORM_XMLTREEACCESSOR_FILTER, data))
    {
      if (!data.filter.partial)
        return;
    }
  else if (temporary_filter)
    {
      OP_DELETE (next_filter);
      next_filter = 0;
    }

  if (next_predicates_count == next_predicates_total)
    {
      unsigned new_predicates_total = next_predicates_total == 0 ? 4 : next_predicates_total + next_predicates_total;
      XPath_Expression **new_predicates = OP_NEWA_L (XPath_Expression *, new_predicates_total);

      op_memcpy (new_predicates, next_predicates, next_predicates_count * sizeof *next_predicates);

      next_predicates = new_predicates;
      next_predicates_total = new_predicates_total;
    }

  next_predicates[next_predicates_count++] = predicate;
  predicate_anchor.release ();
}

void
XPath_SimplePattern::CloseL (XPath_Parser *parser)
{
  AddNextStepL (parser, TRUE);
}

#ifdef XPATH_NODE_PROFILE_SUPPORT

/* virtual */ XPathPattern::ProfileVerdict
XPath_SimplePattern::GetProfileVerdict (XPathNodeProfileImpl *profile)
{
  if (invalid)
    return XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES;
  else if (attributestep)
    return profile->GetIncludesAttributes () ? XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES : XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES;
  else if (nodestep)
    {
      XPath_XMLTreeAccessorFilter **filters = profile->GetFilters ();
      unsigned filters_count = profile->GetFiltersCount ();

      if (filters_count == 0)
        /* No filters mean we haven't a clue. */
        return XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES;
      else
        {
          unsigned index, will_match_count = 0, can_match_count = 0;

          for (index = 0; index < filters_count; ++index)
            {
              XPathPattern::ProfileVerdict verdict = nodestep->GetProfileVerdict (filters[index]);
              if (verdict == XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES)
                ++will_match_count;
              else if (verdict == XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES)
                ++can_match_count;
            }

          if (will_match_count == filters_count)
            return !absolute && !profile->GetIncludesRoots () && !profile->GetIncludesAttributes () && !profile->GetIncludesNamespaces () ? XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES : XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES;
          else if (will_match_count != 0 || can_match_count != 0)
            return XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES;
          else
            return XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES;
        }
    }
  else
    {
      if (profile->GetIncludesRoots ())
        if (!profile->GetIncludesRegulars () && !profile->GetIncludesAttributes () && !profile->GetIncludesNamespaces ())
          return XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES;
        else
          return XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES;
      else
        return XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES;
    }
}

#endif // XPATH_NODE_PROFILE_SUPPORT

BOOL
XPath_SimplePattern::MatchL (XPath_Node *node)
{
  if (invalid)
    return FALSE;

  XPath_Context context (global, 0, 0, 0);

  if (node->type == XP_NODE_ROOT)
    return !nodestep && !attributestep;
  else if (node->type != XP_NODE_ATTRIBUTE)
    return nodestep && nodestep->MatchL (&context, node);
  else
    return attributestep && attributestep->MatchL (&context, node);
}

void
XPath_SimplePattern::PrepareL (XMLTreeAccessor *tree)
{
}

void
XPath_ComplexPattern::MatchingNodes::AddSimpleL (XMLTreeAccessor::Node *node)
{
  unsigned hash = XP_HASH_POINTER (node);

  if (simple_capacity == 0)
    {
      simple_hashed = OP_NEWA_L (XMLTreeAccessor::Node *, 32);
      simple_count = 1;
      simple_capacity = 32;

      op_memset (simple_hashed, 0, simple_capacity * sizeof (XMLTreeAccessor::Node *));

      simple_hashed[hash & 31] = node;
    }
  else
    {
      XMLTreeAccessor::Node **hashed = simple_hashed, *entry;
      unsigned capacity = simple_capacity;

      unsigned index = hash;
      unsigned mask = capacity - 1;

      while (TRUE)
        {
          entry = hashed[index & mask];

          if (entry == node)
            return;
          else if (!entry)
            break;

          index = (index << 2) + index + hash + 1;
          hash >>= 5;
        }

      if (simple_count + simple_count < capacity)
        {
          hashed[index & mask] = node;
          ++simple_count;
        }
      else
        {
          /* Grow. */

          simple_hashed = OP_NEWA_L (XMLTreeAccessor::Node *, simple_capacity + simple_capacity);
          simple_count = 0;
          simple_capacity += simple_capacity;

          op_memset (simple_hashed, 0, simple_capacity * sizeof (XMLTreeAccessor::Node *));

          ANCHOR_ARRAY (XMLTreeAccessor::Node *, hashed);

          for (unsigned index = 0; index < capacity; ++index)
            if (hashed[index])
              AddSimpleL (hashed[index]);

          AddSimpleL (node);
        }
    }
}

XPath_ComplexPattern::MatchingNodes::Compound **
XPath_ComplexPattern::MatchingNodes::AddCompoundL (XMLTreeAccessor::Node *node)
{
  unsigned hash = XP_HASH_POINTER (node);

  if (compound_capacity == 0)
    {
      compound_hashed = OP_NEWA_L (Compound *, 8);
      compound_capacity = 8;

      op_memset (compound_hashed, 0, compound_capacity * sizeof (Compound *));

      return &compound_hashed[hash & 7];
    }
  else
    {
      Compound **hashed = compound_hashed, *entry;
      unsigned capacity = compound_capacity;

      unsigned index = hash;
      unsigned mask = capacity - 1;

      while (TRUE)
        {
          entry = hashed[index & mask];

          if (!entry)
            break;
          else if (entry->owner == node)
            return &hashed[index & mask];

          index = (index << 2) + index + hash + 1;
        }

      if (compound_count + compound_count < capacity)
        return &hashed[index & mask];
      else
        {
          /* Grow. */

          compound_hashed = OP_NEWA_L (Compound *, compound_capacity + compound_capacity);
          compound_count = 0;
          compound_capacity += compound_capacity;

          op_memset (compound_hashed, 0, compound_capacity * sizeof (Compound *));

          ANCHOR_ARRAY (Compound *, hashed);

          for (unsigned index = 0; index < capacity; ++index)
            if (Compound *entry = hashed[index])
              {
                *AddCompoundL (entry->owner) = entry;
                ++compound_count;
              }

          return AddCompoundL (node);
        }
    }
}

XPath_ComplexPattern::MatchingNodes::MatchingNodes ()
  : simple_hashed (0),
    simple_count (0),
    simple_capacity (0),
    compound_hashed (0),
    compound_count (0),
    compound_capacity (0),
    state (INITIAL),
    next (0)
{
}

void
XPath_DeleteXMLExpandedName (const void *key, void *data)
{
  XMLExpandedName *name = static_cast<XMLExpandedName *> (data);
  OP_DELETE (name);
}

XPath_ComplexPattern::MatchingNodes::~MatchingNodes ()
{
  OP_DELETEA (simple_hashed);

  for (unsigned index = 0; index < compound_capacity; ++index)
    if (Compound *compound = compound_hashed[index])
      {
        if (compound->attributes)
          {
            compound->attributes->ForEach (XPath_DeleteXMLExpandedName);
            OP_DELETE (compound->attributes);
          }
        if (compound->namespaces)
          {
            compound->namespaces->ForEach (XPath_DeleteXMLExpandedName);
            OP_DELETE (compound->namespaces);
          }

        OP_DELETE (compound);
      }
  OP_DELETEA (compound_hashed);
}

void
XPath_ComplexPattern::MatchingNodes::AddNodeL (XPath_Node *node)
{
  XMLTreeAccessor::Node *treenode = node->treenode;

  if (node->type != XP_NODE_ATTRIBUTE && node->type != XP_NODE_NAMESPACE)
    {
      /* Simple case: just add the XMLTreeAccessor::Node pointer to the
         pointer set in simple_hashed/simple_capacity. */

      AddSimpleL (treenode);
    }
  else
    {
      Compound **entry_ptr = AddCompoundL (treenode), *entry;

      if (!*entry_ptr)
        {
          entry = *entry_ptr = OP_NEW_L (Compound, (treenode));
          ++compound_count;
        }
      else
        entry = *entry_ptr;

      OpHashTable **table_ptr = node->type == XP_NODE_ATTRIBUTE ? &entry->attributes : &entry->namespaces, *table;

      if (!*table_ptr)
        table = *table_ptr = OP_NEW_L (OpHashTable, (g_opera->xpath_module.xmlexpandedname_hash_functions));
      else
        {
          table = *table_ptr;

          if (table->Contains (static_cast<XMLExpandedName *> (&node->name)))
            return;
        }

      XMLExpandedName *name = OP_NEW (XMLExpandedName, ());

      if (!name || name->Set (node->name) == OpStatus::ERR_NO_MEMORY || table->Add (name, name) == OpStatus::ERR_NO_MEMORY)
        {
          OP_DELETE (name);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }
    }
}

BOOL
XPath_ComplexPattern::MatchingNodes::Match (XPath_Node *node)
{
  XMLTreeAccessor::Node *treenode = node->treenode;

  unsigned hash = XP_HASH_POINTER (treenode);
  unsigned hash_index = hash;

  if (node->type != XP_NODE_ATTRIBUTE && node->type != XP_NODE_NAMESPACE)
    {
      if (simple_hashed)
        {
          unsigned mask = simple_capacity - 1;

          while (XMLTreeAccessor::Node *entry = simple_hashed[hash_index & mask])
            {
              if (entry == treenode)
                return TRUE;

              hash_index = (hash_index << 2) + hash_index + hash + 1;
              hash >>= 5;
            }
        }
    }
  else
    {
      if (compound_hashed)
        {
          unsigned mask = compound_capacity - 1;

          while (Compound *entry = compound_hashed[hash_index & mask])
            {
              if (entry->owner == treenode)
                {
                  OpHashTable *table;

                  if (node->type == XP_NODE_ATTRIBUTE)
                    table = entry->attributes;
                  else
                    table = entry->namespaces;

                  return table && table->Contains (static_cast<XMLExpandedName *> (&node->name));
                }

              hash_index = (hash_index << 2) + hash_index + hash + 1;
              hash >>= 5;
            }
        }
    }

  return FALSE;
}

/* virtual */
XPath_ComplexPattern::~XPath_ComplexPattern ()
{
  OP_DELETE (producer);
}

/* virtual */ BOOL
XPath_ComplexPattern::MatchL (XPath_Node *node)
{
  return global->pattern_context->FindMatchingNodes (this, node->tree)->Match (node);
}

#ifdef XPATH_NODE_PROFILE_SUPPORT

/* virtual */ XPathPattern::ProfileVerdict
XPath_ComplexPattern::GetProfileVerdict (XPathNodeProfileImpl *profile)
{
  XPath_Producer::TransformData data;

  data.nodeprofile.filters = 0;
  data.nodeprofile.filters_count = 0;
  data.nodeprofile.includes_regulars = 0;
  data.nodeprofile.includes_roots = 0;
  data.nodeprofile.includes_attributes = 0;
  data.nodeprofile.includes_namespaces = 0;
  data.nodeprofile.filtered = 0;

  BOOL handled = FALSE;

  TRAPD (status, handled = producer->TransformL (0, XPath_Producer::TRANSFORM_NODE_PROFILE, data));

  if (!handled || OpStatus::IsMemoryError (status))
    return XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES;

  /* This is a pattern; patterns can't match namespaces. */
  OP_ASSERT (!data.nodeprofile.includes_namespaces);

  BOOL will_match = TRUE, can_match = FALSE;

  if (profile->GetIncludesRegulars ())
    if (!data.nodeprofile.includes_regulars)
      will_match = FALSE;
    else
      {
        if (data.nodeprofile.filters_count != 0)
          {
            /* Pattern had filters => pattern does not match all regular nodes. */
            if (profile->GetFiltersCount () == 0)
              {
                /* Profile had no filter => profile can include any regular node. */
                will_match = FALSE;
                can_match = TRUE;
              }
            else
              /* Both pattern and profile has filters. */
              for (unsigned index1 = 0; index1 < profile->GetFiltersCount (); ++index1)
                {
                  XPath_XMLTreeAccessorFilter *profile_filter = profile->GetFilters ()[index1];

                  for (unsigned index2 = 0; index2 < data.nodeprofile.filters_count; ++index2)
                    switch (XPath_GetProfileVerdict (profile_filter, data.nodeprofile.filters[index2]))
                      {
                      case XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES:
                        can_match = TRUE;
                        break;

                      case XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES:
                        will_match = FALSE;
                        can_match = TRUE;
                        break;

                      case XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES:
                        will_match = FALSE;
                        break;
                      }
                }

            OP_DELETEA (data.nodeprofile.filters);
          }
        else
          /* Pattern had no filters => pattern matches any regular node. */
          can_match = TRUE;
      }

  if (profile->GetIncludesRoots ())
    if (!data.nodeprofile.includes_roots)
      will_match = FALSE;
    else
      can_match = TRUE;

  if (profile->GetIncludesAttributes ())
    if (!data.nodeprofile.includes_attributes)
      will_match = FALSE;
    else
      can_match = TRUE;

  if (profile->GetIncludesNamespaces ())
    will_match = FALSE;

  if (data.nodeprofile.filtered)
    will_match = FALSE;

  if (will_match)
    {
      /* This is supposed to be a "complex" pattern.  How can we know statically
         what nodes it matches? */
      OP_ASSERT (FALSE);

      return XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES;
    }
  else if (can_match)
    return XPathPattern::PATTERN_CAN_MATCH_PROFILED_NODES;
  else
    return XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES;
}

#endif // XPATH_NODE_PROFILE_SUPPORT

/* virtual */ void
XPath_ComplexPattern::PrepareL (XMLTreeAccessor *tree)
{
  XPath_PatternContext *pattern_context = global->pattern_context;
  MatchingNodes *matching_nodes = pattern_context->FindMatchingNodes (this, tree);

  if (!matching_nodes)
    {
      OpAutoPtr<MatchingNodes> anchor (matching_nodes = OP_NEW_L (MatchingNodes, ()));
      pattern_context->AddMatchingNodesL (this, tree, matching_nodes);
      anchor.release ();
    }

  if (matching_nodes->state != MatchingNodes::FINISHED)
    {
      XPath_Node temporary (tree, tree->GetRoot ());
      XPath_Context context (global, &temporary, 0, 0);

      if (matching_nodes->state == MatchingNodes::INITIAL)
        {
          producer->Reset (&context);
          matching_nodes->state = MatchingNodes::EVALUATING;
        }

      while (XPath_Node *node = producer->GetNextNodeL (&context))
        {
          matching_nodes->AddNodeL (node);
          XPath_Node::DecRef (&context, node);
        }

      matching_nodes->state = MatchingNodes::FINISHED;
    }
}

XPath_PatternContext::~XPath_PatternContext ()
{
  while (MatchingNodesPerPattern *per_pattern = matching_nodes_per_pattern)
    {
      matching_nodes_per_pattern = per_pattern->next;

      while (MatchingNodesPerPattern::MatchingNodesPerTree *per_tree = per_pattern->matching_nodes_per_tree)
        {
          per_pattern->matching_nodes_per_tree = per_tree->next;
          OP_DELETE (per_tree->matching_nodes);
          OP_DELETE (per_tree);
        }

      OP_DELETE (per_pattern);
    }
}

void
XPath_PatternContext::InvalidateTree (XMLTreeAccessor *tree)
{
  MatchingNodesPerPattern *per_pattern = matching_nodes_per_pattern;

  while (per_pattern)
    {
      MatchingNodesPerPattern::MatchingNodesPerTree **ptr = &per_pattern->matching_nodes_per_tree;

      while (*ptr && (*ptr)->tree != tree)
        ptr = &(*ptr)->next;

      if (MatchingNodesPerPattern::MatchingNodesPerTree *per_tree = *ptr)
        {
          *ptr = per_tree->next;

          OP_DELETE (per_tree->matching_nodes);
          OP_DELETE (per_tree);
        }

      per_pattern = per_pattern->next;
    }
}

XPath_ComplexPattern::MatchingNodes *
XPath_PatternContext::FindMatchingNodes (XPath_ComplexPattern *pattern, XMLTreeAccessor *tree)
{
  /* FIXME: tree per pattern will be rather stable, that is, matching will be
     performed for many nodes from one tree before another tree is involved,
     and usually only nodes from a single tree will ever be matched.  But
     pattern will probably tend to vary in a "round-robin" fashion, in which
     case this LRU list will also have the sought for pattern last, and thus
     perform quite bad.  A hash table would probably be reasonable. */

  if (MatchingNodesPerPattern *per_pattern = matching_nodes_per_pattern)
    {
      if (per_pattern->pattern != pattern)
        {
          MatchingNodesPerPattern **ptr = &per_pattern->next;

          while (*ptr && (*ptr)->pattern != pattern)
            ptr = &(*ptr)->next;

          if (*ptr)
            {
              per_pattern = *ptr;
              *ptr = per_pattern->next;

              per_pattern->next = matching_nodes_per_pattern;
              matching_nodes_per_pattern = per_pattern;
            }
          else
            return 0;
        }

      if (MatchingNodesPerPattern::MatchingNodesPerTree *per_tree = per_pattern->matching_nodes_per_tree)
        {
          if (per_tree->tree != tree)
            {
              MatchingNodesPerPattern::MatchingNodesPerTree **ptr = &per_tree->next;

              while (*ptr && (*ptr)->tree != tree)
                ptr = &(*ptr)->next;

              if (*ptr)
                {
                  per_tree = *ptr;
                  *ptr = per_tree->next;

                  per_tree->next = per_pattern->matching_nodes_per_tree;
                  per_pattern->matching_nodes_per_tree = per_tree;
                }
              else
                return 0;
            }

          return per_tree->matching_nodes;
        }
    }

  return 0;
}

void
XPath_PatternContext::AddMatchingNodesL (XPath_ComplexPattern *pattern, XMLTreeAccessor *tree, XPath_ComplexPattern::MatchingNodes *matching_nodes)
{
  /* We're assuming a FindMatchingNodes call has just been made, and it would
     have moved the right per-pattern object first in the list, had it
     succeeded in finding one. */
  MatchingNodesPerPattern *per_pattern = matching_nodes_per_pattern;

  if (!per_pattern || per_pattern->pattern != pattern)
    {
      per_pattern = OP_NEW_L (MatchingNodesPerPattern, ());
      per_pattern->pattern = pattern;
      per_pattern->matching_nodes_per_tree = 0;

      /* Add first in the list. */
      per_pattern->next = matching_nodes_per_pattern;
      matching_nodes_per_pattern = per_pattern;
    }

  MatchingNodesPerPattern::MatchingNodesPerTree *per_tree = OP_NEW_L (MatchingNodesPerPattern::MatchingNodesPerTree, ());
  per_tree->tree = tree;
  per_tree->matching_nodes = matching_nodes;

  /* Add first in the list. */
  per_tree->next = per_pattern->matching_nodes_per_tree;
  per_pattern->matching_nodes_per_tree = per_tree;
}

#endif // XPATH_PATTERN_SUPPORT
