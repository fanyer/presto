/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpstep.h"
#include "modules/xpath/src/xpnodelist.h"
#include "modules/xpath/src/xputils.h"
#include "modules/xpath/src/xpexpr.h"
#include "modules/xpath/src/xpvalue.h"
#include "modules/xpath/src/xpparser.h"

XPath_XMLTreeAccessorFilter::XPath_XMLTreeAccessorFilter ()
  : flags (0),
    element_name_data1 (~0u),
    element_name_data2 (~0u),
    attribute_name_data1 (~0u),
    attribute_name_data2 (~0u)
{
}

void
XPath_XMLTreeAccessorFilter::SetFilter (XMLTreeAccessor *tree)
{
  if (flags != 0)
    {
      if ((flags & FLAG_HAS_NODETYPE) != 0)
        tree->SetNodeTypeFilter (nodetype, TRUE);
      if ((flags & FLAG_HAS_ELEMENT_NAME) != 0)
        OpStatus::Ignore (tree->SetElementNameFilter (element_name, FALSE, &element_name_data1, &element_name_data2));
      if ((flags & FLAG_HAS_ATTRIBUTE_NAME) != 0)
        OpStatus::Ignore (tree->SetAttributeNameFilter (attribute_name, FALSE, &attribute_name_data1, &attribute_name_data2));
      if ((flags & FLAG_HAS_ATTRIBUTE_VALUE) != 0)
        OpStatus::Ignore (tree->SetAttributeValueFilter (attribute_value.CStr (), FALSE));
    }
}

XPath_Step::Axis::Axis (XPath_Parser *parser, XPath_Producer *producer, XPath_Axis axis)
  : XPath_ChainProducer (producer),
    axis (axis),
    manual_context_reset (FALSE),
    reversed (FALSE),
    skip_descendants (FALSE),
    index_index (parser->GetStateIndex ()),
    count_index (parser->GetStateIndex ()),
    skip_descendants_index (parser->GetStateIndex ()),
    node_index (parser->GetNodeIndex ()),
    related_index (parser->GetNodeIndex ()),
    localci_index (parser->GetContextInformationIndex ())
{
  ci_index = localci_index;
}

BOOL
XPath_Step::Axis::HasOverlappingAxes (BOOL &disordered)
{
  /* Called from GetProducerFlags, where mixed forward and reverse axes is
     excluded first.  So in this function, if we see a forward axis, we can
     assume that all other axes are forward axes, and vice versa. */

  Axis *previous = (Axis *) GetPrevious (PREVIOUS_AXIS, FALSE);

  if (previous)
    if (previous->HasOverlappingAxes (disordered))
      return TRUE;
    else if (axis == XP_AXIS_ATTRIBUTE || axis == XP_AXIS_NAMESPACE || axis == XP_AXIS_SELF)
      /* These can't overlap in themselves. */
      return FALSE;
    else
      do
        switch (previous->axis)
          {
          case XP_AXIS_ANCESTOR:
          case XP_AXIS_ANCESTOR_OR_SELF:
            /* ancestor::node()/parent::node() or more accurately any
               combination of an ancestor or ancestor-or-self step followed by
               any number of parent steps is okay; the/each parent step just
               excludes the nearest ancestor (or self) from the set of
               ancestors. */
            if (axis != XP_AXIS_PARENT)
              return TRUE;
            break;

          case XP_AXIS_CHILD:
          case XP_AXIS_FOLLOWING_SIBLING:
            if (axis == XP_AXIS_FOLLOWING || axis == XP_AXIS_FOLLOWING_SIBLING)
              return TRUE;
            break;

          case XP_AXIS_ATTRIBUTE:
          case XP_AXIS_NAMESPACE:
            if (axis == XP_AXIS_FOLLOWING)
              return TRUE;
            break;

          case XP_AXIS_PRECEDING_SIBLING:
            return TRUE;

          case XP_AXIS_DESCENDANT:
          case XP_AXIS_DESCENDANT_OR_SELF:
            if (previous->skip_descendants)
              break;

          case XP_AXIS_FOLLOWING:
          case XP_AXIS_PRECEDING:
            /* Theory: 'descendant', 'descendant-or-self', 'following' and
               'preceding' will not visit any one node twice.  And the nodes
               produced by 'child' have one parent each.  If no parent node is
               visited twice, then no child node can be visited twice.  Hence no
               overlapping.  But no order either, because the second child of an
               element is visited before the children of its first child. */
            if (axis == XP_AXIS_CHILD)
              {
                disordered = TRUE;
                return FALSE;
              }
            else
              return TRUE;

          default:
            break;
          }
      while ((previous = (Axis *) previous->GetPrevious (PREVIOUS_AXIS, FALSE)) != 0);

  return FALSE;
}

BOOL
XPath_Step::Axis::IsNodeIncluded (XPath_Node *node)
{
  if (filter.flags != 0)
    {
      if ((node->type & XP_NODEMASK_SPECIAL) != 0)
        /* Attribute or namespace node; can't match an XMLTreeAccessor filter,
           meaning we wouldn't try to use one, so obviously not included if we
           are using one. */
        return FALSE;

      if ((filter.flags & XPath_XMLTreeAccessorFilter::FLAG_HAS_NODETYPE) != 0)
        if (node->tree->GetNodeType (node->treenode) != filter.nodetype)
          /* Wrong type of node. */
          return FALSE;

      if ((filter.flags & (XPath_XMLTreeAccessorFilter::FLAG_HAS_ELEMENT_NAME | XPath_XMLTreeAccessorFilter::FLAG_HAS_ATTRIBUTE_NAME | XPath_XMLTreeAccessorFilter::FLAG_HAS_ATTRIBUTE_VALUE)) != 0)
        {
          /* Let the XMLTreeAccessor say. */
          filter.SetFilter (node->tree);
          BOOL included = node->tree->FilterNode (node->treenode);
          node->tree->ResetFilters ();
          return included;
        }
    }

  return TRUE;
}

/* static */ XPath_Step::Axis *
XPath_Step::Axis::MakeL (XPath_Parser *parser, XPath_Producer *producer, XPath_Axis axis0)
{
  return OP_NEW_L (XPath_Step::Axis, (parser, producer, axis0));
}

/* virtual */ BOOL
XPath_Step::Axis::Reset (XPath_Context *context, BOOL local_context_only)
{
  if (!local_context_only)
    {
      context->states[index_index] = context->states[count_index] = context->states[skip_descendants_index] = 0;
      context->nodes[node_index].Reset ();
      context->nodes[related_index].Reset ();
      context->cis[localci_index].Reset ();
      return XPath_ChainProducer::Reset (context, local_context_only);
    }
  else if (context->nodes[node_index].IsValid ())
    {
      context->states[index_index] = ~0u;
      context->cis[localci_index].Reset ();
      return TRUE;
    }
  else
    return FALSE;
}

/* virtual */ XPath_Node *
XPath_Step::Axis::GetNextNodeL (XPath_Context *context)
{
  unsigned &index = context->states[index_index];
  unsigned &count = context->states[count_index];
  XPath_Node &node_stored = context->nodes[node_index];
  XPath_Node &related_stored = context->nodes[related_index];
  XPath_Node *node = 0, *related = 0;

  if (node_stored.IsValid ())
    if (index == ~0u)
      node_stored.Reset ();
    else
      node = XPath_Node::IncRef (&node_stored);

  if (related_stored.IsValid ())
    related = &related_stored;

  /* Cached value: all nodes traversed by us will be related and thus come
     from the same tree accessor. */
  XMLTreeAccessor *tree = 0;

  /* Used later. */
  XMLTreeAccessor::Node *treenode;

  if (node)
    tree = node->tree;

  BOOL empty_context_node = FALSE;

  /* If we have no node, this is the first call after a reset.  Otherwise if
     this is the second call we've returned one node, so for the axes 'parent'
     and 'self', that only contain one node, we're done with the current
     node. */
  if (!node || index == 1 && (axis == XP_AXIS_PARENT || axis == XP_AXIS_SELF))
    {
    get_new_node:
      if (tree && filter.flags != 0)
        tree->ResetFilters ();

      if (node)
        XPath_Node::DecRef (context, node);

      if (manual_context_reset && node_stored.IsValid () && !empty_context_node)
        return 0;

      /* Reset our stored current node, so that if the call to GetNextNodeL
         below leaves, we'll try it again immediately the next time we're
         called. */
      node_stored.Reset ();
      related_stored.Reset ();

      /* Get a new "context node" from the base producer. */
      node = producer->GetNextNodeL (context);

      if (!node)
        return 0;
      else if (axis == XP_AXIS_DESCENDANT || axis == XP_AXIS_DESCENDANT_OR_SELF)
        {
          XPath_Step::Axis *axis = GetPreviousAxis ();
          if (axis && axis->skip_descendants)
            context->states[axis->skip_descendants_index] = 1;
        }

      /* If we get back to 'get_new_node' during this call, then this context
         node was empty. */
      empty_context_node = TRUE;

      /* Theoretically, our base producer might return nodes from a mixed tree
         node-set.  Better safe than sorry. */
      tree = node->tree;

      /* New context node means position is reset to zero.  Note that
         localci.position is not updated by us; it is updated by the next
         position predicate for each node it sees.  Such a predicate also
         introduces its own new position for following position predicate
         steps. */
      index = context->cis[localci_index].position = 0;

      if (axis == XP_AXIS_DESCENDANT_OR_SELF || axis == XP_AXIS_DESCENDANT || axis == XP_AXIS_PRECEDING)
        {
          related_stored.Set (tree, node->treenode);
          related = &related_stored;
        }
      else if (axis == XP_AXIS_ATTRIBUTE)
        {
          count = tree->GetAttributes (node->treenode, FALSE, TRUE)->GetCount ();
        }
      else if (axis == XP_AXIS_NAMESPACE)
        {
          XMLTreeAccessor::Namespaces *namespaces;
          LEAVE_IF_ERROR (tree->GetNamespaces (namespaces, node->treenode));
          count = namespaces->GetCount ();
        }

#ifdef XPATH_DEBUG_MODE
      if (--context->iterations == 0)
        {
          node_stored.CopyL (node);
          XPath_Node::DecRef (context, node);

          /* Probably leaves. */
          XPath_Pause (context);

          /* But just in case it didn't. */
          node = &node_stored;
        }
#endif // XPATH_DEBUG_MODE
    }

  BOOL special_node = (node->type & XP_NODEMASK_SPECIAL) != 0;

  if (index == 0 && (axis == XP_AXIS_ANCESTOR_OR_SELF || axis == XP_AXIS_DESCENDANT_OR_SELF || axis == XP_AXIS_SELF))
    {
      /* Handle the 'self' part of these axes. */
      ++index;
      if (IsNodeIncluded (node))
        {
          if (node != &node_stored)
            node_stored.CopyL (node);
          return node;
        }
      else if (axis == XP_AXIS_SELF)
        goto get_new_node;
    }

  if (special_node)
    if (index == 0 && (axis == XP_AXIS_ANCESTOR || axis == XP_AXIS_PARENT) || index == 1 && axis == XP_AXIS_ANCESTOR_OR_SELF)
      {
        ++index;
        node_stored.Set (tree, node->treenode);
        if (IsNodeIncluded (&node_stored))
          goto return_node_stored;
        else if (axis == XP_AXIS_PARENT)
          goto get_new_node;
      }
    else switch (axis)
      {
      case XP_AXIS_ANCESTOR:
      case XP_AXIS_ANCESTOR_OR_SELF:
      case XP_AXIS_FOLLOWING:
      case XP_AXIS_PRECEDING:
        /* These can produce nodes via the tree accessor from an attribute or
           namespace context node. */
        break;

      case XP_AXIS_ATTRIBUTE:
      case XP_AXIS_NAMESPACE:
        /* These return special nodes, so at 'index > 0', 'node' will be the
           most recently returned node, and thus special.  But if 'index == 0',
           the axis is being "applied" to a special node, in which case it is of
           course empty.  (If we're receiving nodes from a previous step, this
           doesn't happen, because the parser detects the silliness, but we
           might be receiving nodes from an arbitrary expression.) */
        if (index == 0)
          goto get_new_node;
        break;

      default:
        /* No other axes can. */
        goto get_new_node;
      }

  treenode = node->treenode;

  if (filter.flags != 0)
    filter.SetFilter (tree);

  bool first;
  first = index == 0;

 get_new_treenode:
  switch (axis)
    {
    case XP_AXIS_ANCESTOR_OR_SELF:
    case XP_AXIS_ANCESTOR:
      treenode = tree->GetAncestor (treenode);
      break;

    case XP_AXIS_PARENT:
      treenode = tree->GetParent (treenode);
      break;

    case XP_AXIS_ATTRIBUTE:
      if (index < count)
        {
          XMLCompleteName name;
          const uni_char *value;
          BOOL id, specified;
          unsigned revindex = count - (index + 1);

          LEAVE_IF_ERROR (tree->GetAttributes (treenode, FALSE, TRUE)->GetAttribute (reversed ? revindex : index, name, value, id, specified, 0));

          node_stored.SetAttributeL (tree, treenode, name);
          ++index;
          goto return_node_stored;
        }
      else
        goto get_new_node;

    case XP_AXIS_CHILD:
      if (first)
        treenode = reversed ? tree->GetLastChild (treenode) : tree->GetFirstChild (treenode);
      else
        treenode = reversed ? tree->GetPreviousSibling (treenode) : tree->GetNextSibling (treenode);
      break;

    case XP_AXIS_FOLLOWING_SIBLING:
      treenode = tree->GetNextSibling (treenode);
      break;

    case XP_AXIS_DESCENDANT_OR_SELF:
    case XP_AXIS_DESCENDANT:
      OP_ASSERT (related);
      if (skip_descendants && index != 0 && context->states[skip_descendants_index])
        {
          treenode = tree->GetNextNonDescendant (treenode);
          if (treenode && !tree->IsAncestorOf (related->treenode, treenode))
            treenode = 0;
          context->states[skip_descendants_index] = 0;
        }
      else
        treenode = tree->GetNextDescendant (treenode, related->treenode);
      break;

    case XP_AXIS_FOLLOWING:
      treenode = first && !special_node ? tree->GetNextNonDescendant (treenode) : tree->GetNext (treenode);
      break;

    case XP_AXIS_NAMESPACE:
      if (index < count)
        {
          /* FIXME: XMLTreeAccessor::GetNamespaces is expensive.  Can we avoid
             calling it once for each node, somehow? */
          XMLTreeAccessor::Namespaces *namespaces;
          LEAVE_IF_ERROR (tree->GetNamespaces (namespaces, treenode));
          const uni_char *prefix, *uri;
          unsigned revindex = count - (index + 1);
          namespaces->GetNamespace (reversed ? revindex : index, uri, prefix);
          node_stored.SetNamespaceL (tree, treenode, prefix, uri);
          ++index;
          goto return_node_stored;
        }
      else
        goto get_new_node;

    case XP_AXIS_PRECEDING:
      OP_ASSERT (related);
      treenode = tree->GetPreviousNonAncestor (treenode, related->treenode);
      break;

    case XP_AXIS_PRECEDING_SIBLING:
      treenode = tree->GetPreviousSibling (treenode);
      break;
    }

  if (!treenode)
    {
      if (filter.flags != 0)
        tree->ResetFilters ();

      goto get_new_node;
    }
  else
    {
      XMLTreeAccessor::NodeType treenode_type = tree->GetNodeType (treenode);
      if (treenode_type == XMLTreeAccessor::TYPE_TEXT || treenode_type == XMLTreeAccessor::TYPE_CDATA_SECTION)
        {
          if (filter.flags != 0)
            tree->ResetFilters ();

          if (XMLTreeAccessor::Node *previous_sibling = tree->GetPreviousSibling (treenode))
            {
              XMLTreeAccessor::NodeType previous_sibling_type = tree->GetNodeType (previous_sibling);
              if (previous_sibling_type == XMLTreeAccessor::TYPE_TEXT || previous_sibling_type == XMLTreeAccessor::TYPE_CDATA_SECTION)
                {
                  first = false;
                  /* This node doesn't exist. */
                  goto get_new_treenode;
                }
            }
        }
      else if (treenode_type == XMLTreeAccessor::TYPE_DOCTYPE)
        {
          first = false;
          goto get_new_treenode;
        }
    }

  ++index;

  node_stored.Set (tree, treenode);

 return_node_stored:
  if (filter.flags != 0)
    tree->ResetFilters ();

  XPath_Node::DecRef (context, node);
  return XPath_Node::IncRef (&node_stored);
}

/* virtual */ void
XPath_Step::Axis::SetManualLocalContextReset (BOOL value)
{
  manual_context_reset = value;
}

/* virtual */ unsigned
XPath_Step::Axis::GetProducerFlags ()
{
  unsigned flags = producer->GetProducerFlags () & MASK_INHERITED, localflags = 0;

  switch (axis)
    {
    case XP_AXIS_ANCESTOR:
    case XP_AXIS_ANCESTOR_OR_SELF:
    case XP_AXIS_PRECEDING:
    case XP_AXIS_PRECEDING_SIBLING:
      localflags = FLAG_REVERSE_DOCUMENT_ORDER;
      break;

    case XP_AXIS_ATTRIBUTE:
    case XP_AXIS_CHILD:
    case XP_AXIS_NAMESPACE:
      if (reversed)
        {
          localflags = FLAG_REVERSE_DOCUMENT_ORDER;
          break;
        }

    case XP_AXIS_DESCENDANT:
    case XP_AXIS_DESCENDANT_OR_SELF:
    case XP_AXIS_FOLLOWING:
    case XP_AXIS_FOLLOWING_SIBLING:
    case XP_AXIS_SELF:
      localflags = FLAG_DOCUMENT_ORDER;
      break;

    case XP_AXIS_PARENT:
      localflags = flags & MASK_ORDER;
      flags = flags & ~FLAG_NO_DUPLICATES;
    }

  if ((flags & FLAG_SINGLE_NODE) != 0)
    /* If our producer is the initial context node (or some other
       producer that returns a single node) its claimed order is
       irrelevant, so we pretend it claimed to return nodes in our
       order to avoid unnecessary disorder. */
    flags = (flags & ~MASK_ORDER) | localflags;

  BOOL disordered = FALSE;

  if ((flags & MASK_ORDER) != (localflags & MASK_ORDER) || HasOverlappingAxes (disordered))
    /* Mixed order or overlapping axes => no order and duplicates. */
    flags &= ~(MASK_ORDER | FLAG_NO_DUPLICATES);

  if (disordered)
    /* HasOverlappingAxes() determined there couldn't be duplicates, but
       that nodes might be visited in non-document order. */
    flags &= ~MASK_ORDER;

  if (axis != XP_AXIS_SELF)
    /* The self axis is 1:1, so we do know the size if the previous
       producer does.  But all other axes are either 1:many or
       possibly 1:0, so we don't know the size. */
    flags &= ~(FLAG_KNOWN_SIZE | FLAG_SINGLE_NODE);

  return flags;
}

/* virtual */ void
XPath_Step::Axis::OptimizeL (unsigned hints)
{
  if ((hints & HINT_CONTEXT_DEPENDENT_PREDICATE) == 0 && axis == XP_AXIS_CHILD)
    {
      Axis *previous = GetPreviousAxis ();

      if (previous == producer && previous->axis == XP_AXIS_DESCENDANT_OR_SELF)
        {
          /* This step is preceded by the step

               descendant-or-self::node()

             and there are no context size or position dependent predicates on
             this step.  We can drop the preceding step and change our axis to
             descendant.  This is better. */

          producer = GoAway (previous);
          axis = XP_AXIS_DESCENDANT;
        }
    }

  XPath_ChainProducer::OptimizeL (hints & ~HINT_CONTEXT_DEPENDENT_PREDICATE);

  if ((hints & HINT_CONTEXT_DEPENDENT_PREDICATE) == 0 && (axis == XP_AXIS_DESCENDANT || axis == XP_AXIS_DESCENDANT_OR_SELF))
    {
      Axis *previous = GetPreviousAxis ();

      if (previous && (previous->axis == XP_AXIS_DESCENDANT || previous->axis == XP_AXIS_DESCENDANT_OR_SELF))
        previous->skip_descendants = TRUE;
    }
}

#ifdef XPATH_NODE_PROFILE_SUPPORT

static void
XPath_SetPossibleNodeTypes (XPath_Step::Axis *axis, XPath_Producer::TransformData &data)
{
  while (axis)
    {
      switch (axis->GetAxis ())
        {
        case XP_AXIS_SELF:
          break;

        case XP_AXIS_ANCESTOR_OR_SELF:
          if ((axis->GetFilter ()->flags & XPath_XMLTreeAccessorFilter::FLAG_HAS_NODETYPE) == 0 || axis->GetFilter ()->nodetype == XMLTreeAccessor::TYPE_ROOT)
            data.nodeprofile.includes_roots = 1;
          /* fall through */

        case XP_AXIS_DESCENDANT_OR_SELF:
          data.nodeprofile.includes_regulars = 1;
          break;

        case XP_AXIS_ATTRIBUTE:
          data.nodeprofile.includes_attributes = 1;
          return;

        case XP_AXIS_NAMESPACE:
          data.nodeprofile.includes_namespaces = 1;
          return;

        case XP_AXIS_ANCESTOR:
        case XP_AXIS_PARENT:
          if ((axis->GetFilter ()->flags & XPath_XMLTreeAccessorFilter::FLAG_HAS_NODETYPE) == 0 || axis->GetFilter ()->nodetype == XMLTreeAccessor::TYPE_ROOT)
            data.nodeprofile.includes_roots = 1;
          /* fall through */

        default:
          data.nodeprofile.includes_regulars = 1;
          return;
        }

      XPath_Step::Axis *previous_axis = axis->GetPreviousAxis ();

      if (previous_axis)
        axis = previous_axis;
      else
        {
          switch (axis->GetAxis ())
            {
            case XP_AXIS_SELF:
              data.nodeprofile.includes_regulars = 1;
              data.nodeprofile.includes_roots = 1;
              /* fall through */

            case XP_AXIS_ANCESTOR_OR_SELF:
            case XP_AXIS_DESCENDANT_OR_SELF:
              data.nodeprofile.includes_attributes = 1;
              data.nodeprofile.includes_namespaces = 1;
            }
          return;
        }
    }
}

#endif // XPATH_NODE_PROFILE_SUPPORT

/* virtual */ BOOL
XPath_Step::Axis::TransformL (XPath_Parser *parser, Transform transform, TransformData &data)
{
#ifdef XPATH_NODE_PROFILE_SUPPORT
  if (transform == TRANSFORM_NODE_PROFILE)
    {
      XPath_SetPossibleNodeTypes (this, data);

      if (data.nodeprofile.includes_regulars && !data.nodeprofile.includes_roots && !data.nodeprofile.includes_attributes && !data.nodeprofile.includes_namespaces)
        if (filter.flags != 0)
          {
            data.nodeprofile.filters = OP_NEWA_L (XPath_XMLTreeAccessorFilter *, 1);
            data.nodeprofile.filters[0] = &filter;
            data.nodeprofile.filters_count = 1;
          }

      XPath_Step::Axis *previous_axis = GetPreviousAxis ();
      if (previous_axis != GetPrevious () ||
          previous_axis->axis != XP_AXIS_DESCENDANT_OR_SELF ||
          previous_axis->GetPrevious () != previous_axis->GetPrevious (PREVIOUS_CONTEXT_NODE))
        /* "Filtered" in the pattern sense by this not being the first (and
           only) step in the pattern.  Only complex patterns uses this at all,
           and they will prepend the pattern with "//" to find all matched
           nodes.  So "//x" is a pattern matching all elements named x, while
           "//x/x" or any other multi-step pattern is not.  We signal the not
           case by saying the profile is "filtered" which means we won't
           guarantee that any included node will actually match. */
        data.nodeprofile.filtered = 1;
      return TRUE;
    }
#endif // XPATH_NODE_PROFILE_SUPPORT

  if (transform == TRANSFORM_REVERSE_AXIS && !reversed)
    switch (axis)
      {
      case XP_AXIS_CHILD:
      case XP_AXIS_ATTRIBUTE:
      case XP_AXIS_NAMESPACE:
        reversed = TRUE;
        return TRUE;
      }

  return FALSE;
}

static BOOL
XPath_IncludesSelf (XPath_Axis axis)
{
  switch (axis)
    {
    case XP_AXIS_ANCESTOR_OR_SELF:
    case XP_AXIS_DESCENDANT_OR_SELF:
    case XP_AXIS_SELF:
      return TRUE;
    }
  return FALSE;
}

/* static */ BOOL
XPath_Step::Axis::IsMutuallyExclusive (Axis *axis1, Axis *axis2)
{
  if (!axis1 || !axis2)
    return FALSE;

  XPath_Axis type1 = axis2->axis, type2 = axis2->axis;

  if (type1 != type2)
    if (type1 == XP_AXIS_ATTRIBUTE || type2 == XP_AXIS_ATTRIBUTE ||
        type1 == XP_AXIS_NAMESPACE || type2 == XP_AXIS_NAMESPACE)
      return TRUE;
    else if (XPath_IncludesSelf (type1) && XPath_IncludesSelf (type2))
      return FALSE;
    else if (axis1->producer->GetProducerType () == TYPE_SINGLE && axis2->producer->GetProducerType () == TYPE_SINGLE)
      /* Implement this smarter?  And is it at all correct? */
      switch (type1)
        {
        case XP_AXIS_ANCESTOR:
        case XP_AXIS_ANCESTOR_OR_SELF:
        case XP_AXIS_PARENT:
          switch (type2)
            {
            case XP_AXIS_CHILD:
            case XP_AXIS_DESCENDANT:
            case XP_AXIS_DESCENDANT_OR_SELF:
            case XP_AXIS_FOLLOWING:
            case XP_AXIS_FOLLOWING_SIBLING:
            case XP_AXIS_PRECEDING:
            case XP_AXIS_PRECEDING_SIBLING:
            case XP_AXIS_SELF:
              return TRUE;
            }
          break;

        case XP_AXIS_CHILD:
        case XP_AXIS_DESCENDANT:
        case XP_AXIS_DESCENDANT_OR_SELF:
          switch (type2)
            {
            case XP_AXIS_ANCESTOR:
            case XP_AXIS_ANCESTOR_OR_SELF:
            case XP_AXIS_FOLLOWING:
            case XP_AXIS_FOLLOWING_SIBLING:
            case XP_AXIS_PARENT:
            case XP_AXIS_PRECEDING:
            case XP_AXIS_PRECEDING_SIBLING:
            case XP_AXIS_SELF:
              return TRUE;
            }
          break;

        case XP_AXIS_FOLLOWING:
        case XP_AXIS_FOLLOWING_SIBLING:
          switch (type2)
            {
            case XP_AXIS_ANCESTOR:
            case XP_AXIS_ANCESTOR_OR_SELF:
            case XP_AXIS_CHILD:
            case XP_AXIS_DESCENDANT:
            case XP_AXIS_DESCENDANT_OR_SELF:
            case XP_AXIS_PARENT:
            case XP_AXIS_PRECEDING:
            case XP_AXIS_PRECEDING_SIBLING:
            case XP_AXIS_SELF:
              return TRUE;
            }
          break;

        case XP_AXIS_PRECEDING:
        case XP_AXIS_PRECEDING_SIBLING:
          switch (type2)
            {
            case XP_AXIS_ANCESTOR:
            case XP_AXIS_ANCESTOR_OR_SELF:
            case XP_AXIS_CHILD:
            case XP_AXIS_DESCENDANT:
            case XP_AXIS_DESCENDANT_OR_SELF:
            case XP_AXIS_FOLLOWING:
            case XP_AXIS_FOLLOWING_SIBLING:
            case XP_AXIS_PARENT:
            case XP_AXIS_SELF:
              return TRUE;
            }
          break;

        case XP_AXIS_SELF:
          return TRUE;
        }

  return FALSE;
}

/* static */ XPath_Producer *
XPath_Step::Axis::GoAway (Axis *axis)
{
  XPath_Producer *producer = axis->producer;

  axis->producer = 0;
  OP_DELETE (axis);

  return producer;
}

/* virtual */ XPath_Producer *
XPath_Step::Axis::GetPreviousInternal (What what, BOOL include_self)
{
  if (what == PREVIOUS_AXIS && include_self)
    return this;
  else if (what == PREVIOUS_NODETEST || what == PREVIOUS_PREDICATE)
    return 0;
  else
    return XPath_ChainProducer::GetPreviousInternal (what, include_self);
}

XPath_Step::NodeTest::NodeTest (XPath_Producer *producer, XPath_NodeTestType type)
  : XPath_Filter (producer),
    type (type)
{
}

/* static */ XPath_NodeType
XPath_Step::NodeTest::GetMatchedNodeType (NodeTest *nodetest)
{
  if (nodetest->type == XP_NODETEST_NODETYPE || nodetest->type == XP_NODETEST_NAME)
    return ((XPath_NodeTypeTest *) nodetest)->GetNodeType ();
  else if (nodetest->type == XP_NODETEST_PI)
    return XP_NODE_PI;
  else
    /* The rest check attributes. */
    return XP_NODE_ELEMENT;
}

static BOOL
XPath_IsMutuallyExclusiveNames (const XMLExpandedName &name1, const XMLExpandedName &name2)
{
  const uni_char *uri1 = name1.GetUri (), *uri2 = name2.GetUri ();
  if (uri1 == uri2 || uri1 && uri2 && uni_strcmp (uri1, uri2) == 0)
    {
      const uni_char *localpart1 = name1.GetLocalPart (), *localpart2 = name2.GetLocalPart ();
      if (*localpart1 != '*' && *localpart2 != '*' && uni_strcmp (localpart1, localpart2) != 0)
        return TRUE;
    }
  return FALSE;
}

static BOOL
XPath_IsMutuallyExclusiveTargets (const uni_char *target1, const uni_char *target2)
{
  return target1 && target2 && uni_strcmp (target1, target2) != 0;
}

/* static */ BOOL
XPath_Step::NodeTest::IsMutuallyExclusive (NodeTest *nodetest1, NodeTest *nodetest2)
{
  if (!nodetest1 || !nodetest2)
    /* No node-test means "node()" which naturally includes all nodes
       any other node-test includes. */
    return FALSE;

  while (nodetest1)
    {
      XPath_NodeTestType type1 = nodetest1->type;
      XPath_NodeType matched = GetMatchedNodeType (nodetest1);

      NodeTest *other = nodetest2;
      while (other)
        {
          if (matched != GetMatchedNodeType (other))
            return TRUE;

          if (type1 == other->type)
            switch (type1)
              {
              case XP_NODETEST_NAME:
                if (XPath_IsMutuallyExclusiveNames (((XPath_NameTest *) nodetest1)->GetName (),
                                                    ((XPath_NameTest *) other)->GetName ()))
                  return TRUE;
                break;

              case XP_NODETEST_PI:
                if (XPath_IsMutuallyExclusiveTargets (((XPath_PITest *) nodetest1)->GetTarget (),
                                                      ((XPath_PITest *) other)->GetTarget ()))
                  return TRUE;
                break;

#ifdef XPATH_CURRENTLY_UNUSED
              case XP_NODETEST_ATTRIBUTEVALUE:
                XPath_AttributeValueTest *atv1 = (XPath_AttributeValueTest *) nodetest1;
                XPath_AttributeValueTest *atv2 = (XPath_AttributeValueTest *) other;

                /* Two tests testing that the same attribute value is equal
                   to two different values are mutually exclusive. */
                if (atv1->GetEqual () && atv2->GetEqual () && atv1->GetName () == atv2->GetName () && uni_strcmp (atv1->GetValue (), atv2->GetValue ()) == 0)
                  return TRUE;
#endif // XPATH_CURRENTLY_UNUSED
              }

          other = static_cast<XPath_Step::NodeTest *> (other->GetPrevious (PREVIOUS_NODETEST, FALSE));
        }

      nodetest1 = static_cast<XPath_Step::NodeTest *> (nodetest1->GetPrevious (PREVIOUS_NODETEST, FALSE));
    }

  return FALSE;
}

/* virtual */ XPath_Producer *
XPath_Step::NodeTest::GetPreviousInternal (What what, BOOL include_self)
{
  if (what == PREVIOUS_NODETEST && include_self)
    return this;
  else if (what == PREVIOUS_PREDICATE)
    return 0;
  else
    return XPath_ChainProducer::GetPreviousInternal (what, include_self);
}

/* static */ XPath_Step::Predicate *
XPath_Step::Predicate::MakeL (XPath_Parser *parser, XPath_Producer *producer, XPath_Expression *expression, BOOL is_predicate_expression)
{
  XMLExpandedName functionname;

  BOOL is_only_last = expression->IsFunctionCall (functionname) && functionname == XMLExpandedName (UNI_L ("last"));

  unsigned expression_flags = expression->GetPredicateExpressionFlags ();

  OpStackAutoPtr<XPath_Producer> producer_anchor (NULL);

  if (!is_predicate_expression)
    static_cast<XPath_Step::Axis *> (producer->GetPrevious (PREVIOUS_AXIS, TRUE))->SetManualLocalContextReset (TRUE);
  else
    {
      unsigned producer_flags = 0;

      if ((expression_flags & XPath_Expression::FLAG_CONTEXT_POSITION) != 0)
        producer_flags |= XPath_Producer::FLAG_DOCUMENT_ORDER;
      if (!is_only_last && (expression_flags & XPath_Expression::FLAG_CONTEXT_SIZE) != 0)
        producer_flags |= XPath_Producer::FLAG_KNOWN_SIZE;

      XPath_Producer * new_producer = XPath_Producer::EnsureFlagsL (parser, producer, producer_flags | XPath_Producer::FLAG_LOCAL_CONTEXT_POSITION);
      if (new_producer != producer) // only anchor if a new producer is made (input argument is a member variable)
        {
          producer = new_producer;
          producer_anchor.reset (producer);
        }
    }

  if (!is_only_last && !is_predicate_expression && (expression_flags & XPath_Expression::FLAG_CONTEXT_SIZE) != 0)
    {
      producer = OP_NEW_L (XPath_NodeListCollector, (parser, producer, 0));
      producer_anchor.release ();
      producer_anchor.reset (producer);
    }

  Predicate *predicate;

  if (is_only_last)
    {
      TransformData data;
      if (!is_predicate_expression && producer->TransformL (parser, TRANSFORM_REVERSE_AXIS, data))
        predicate = XPath_ConstantPositionPredicate::MakeL (parser, producer, 1);
      else
        predicate = XPath_LastPredicate::MakeL (parser, producer);
      OP_DELETE(expression);
    }
  else
    {
      predicate = 0;

      if ((expression_flags & (XPath_Expression::FLAG_NUMBER | XPath_Expression::FLAG_CONSTANT)) == (XPath_Expression::FLAG_NUMBER | XPath_Expression::FLAG_CONSTANT))
        if (double number = static_cast<XPath_NumberExpression *> (expression)->EvaluateToNumberL (0, TRUE))
          if (op_isintegral (number) && number >= 1. && number < 4294967296.)
            {
              predicate = XPath_ConstantPositionPredicate::MakeL (parser, producer, static_cast<unsigned> (number));
              OP_DELETE(expression);
            }

      if (!predicate)
        predicate = XPath_RegularPredicate::MakeL (parser, producer, expression);
    }

  producer_anchor.release ();

  predicate->is_predicate_expression = is_predicate_expression;
  return predicate;
}

XPath_Step::Predicate::Predicate (XPath_Parser *parser, XPath_Producer *producer, XPath_Expression *expression)
  : XPath_ChainProducer (producer),
    expression (expression),
    state_index (parser->GetStateIndex ()),
    node_index (parser->GetNodeIndex ()),
    localci_index (parser->GetContextInformationIndex ())
{
  ci_index = localci_index;
}

/* virtual */
XPath_Step::Predicate::~Predicate ()
{
  OP_DELETE (expression);
}

unsigned
XPath_Step::Predicate::GetPredicateExpressionFlags ()
{
  return expression ? expression->GetPredicateExpressionFlags () : 0;
}

/* virtual */ BOOL
XPath_Step::Predicate::Reset (XPath_Context *context, BOOL local_context_only)
{
  if (!local_context_only || !is_predicate_expression)
    {
      context->states[state_index] = 0;
      context->cis[localci_index].Reset ();
      return XPath_ChainProducer::Reset (context, local_context_only);
    }
  else
    return FALSE;
}

/* virtual */ XPath_Node *
XPath_Step::Predicate::GetNextNodeL (XPath_Context *context)
{
  unsigned &state = context->states[state_index];
  XPath_Node &node_stored = context->nodes[node_index];
  XPath_Node *node = &node_stored;

  while (TRUE)
    {
      if (state == 1)
        goto match_node;
      else if (state == 2 && !Reset (context, TRUE))
        return 0;

      while (TRUE)
        {
          state = 0;

          node = producer->GetNextNodeL (context);

          if (!node)
            break;

          state = 1;

          node_stored.CopyL (node);
          XPath_Node::DecRef (context, node);

          XP_SEQUENCE_POINT (context);

        match_node:
          MatchStatus match_status = MatchL (context, &node_stored);

          state = 0;

          switch (match_status)
            {
            case MATCH_NONE_IN_LOCAL_CONTEXT:
              state = 2;
              /* fall through */

            case MATCH_NOT_MATCHED:
              break;

            case MATCH_LAST_IN_LOCAL_CONTEXT:
              state = 2;
              /* fall through */

            case MATCH_MATCHED:
              return XPath_Node::IncRef (&node_stored);
            }
        }

      state = 2;

      XP_SEQUENCE_POINT (context);
    }
}

/* virtual */ unsigned
XPath_Step::Predicate::GetProducerFlags ()
{
  if (!is_predicate_expression)
    return GetAxis ()->GetProducerFlags () & ~FLAG_KNOWN_SIZE;
  else
    return XPath_ChainProducer::GetProducerFlags ();
}

#ifdef XPATH_NODE_PROFILE_SUPPORT

/* virtual */ BOOL
XPath_Step::Predicate::TransformL (XPath_Parser *parser, Transform transform, TransformData &data)
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

/* virtual */ XPath_Producer *
XPath_Step::Predicate::GetPreviousInternal (What what, BOOL include_self)
{
  if (what == PREVIOUS_PREDICATE && include_self)
    return this;
  else
    return XPath_ChainProducer::GetPreviousInternal (what, include_self);
}

XPath_NodeTypeTest::XPath_NodeTypeTest (XPath_Producer *producer, XPath_NodeTestType type)
  : XPath_Step::NodeTest (producer, type)
{
}

/* static */ XPath_NodeTypeTest *
XPath_NodeTypeTest::MakeL (XPath_Producer *producer, XPath_NodeType nodetype)
{
  XPath_NodeTypeTest *test = OP_NEW_L (XPath_NodeTypeTest, (producer));
  test->type = XP_NODETEST_NODETYPE;
  test->nodetype = nodetype;
  return test;
}

/* virtual */ BOOL
XPath_NodeTypeTest::MatchL (XPath_Context *context, XPath_Node *node)
{
  return node->GetType () == nodetype;
}

/* virtual */ BOOL
XPath_NodeTypeTest::TransformL (XPath_Parser *parser, Transform transform, TransformData &data)
{
  if (transform == TRANSFORM_XMLTREEACCESSOR_FILTER)
    {
      XMLTreeAccessor::NodeType type;

      switch (nodetype)
        {
        case XP_NODE_ROOT:
          type = XMLTreeAccessor::TYPE_ROOT;
          break;

        case XP_NODE_ELEMENT:
          type = XMLTreeAccessor::TYPE_ELEMENT;
          break;

        case XP_NODE_TEXT:
          type = XMLTreeAccessor::TYPE_TEXT;
          break;

        case XP_NODE_PI:
          type = XMLTreeAccessor::TYPE_PROCESSING_INSTRUCTION;
          break;

        case XP_NODE_COMMENT:
          type = XMLTreeAccessor::TYPE_COMMENT;
          break;

        case XP_NODE_DOCTYPE:
          type = XMLTreeAccessor::TYPE_DOCTYPE;
          break;

        default:
          return FALSE;
        }

      if ((data.filter.filter->flags & XPath_XMLTreeAccessorFilter::FLAG_HAS_NODETYPE) == 0)
        {
          data.filter.filter->flags |= XPath_XMLTreeAccessorFilter::FLAG_HAS_NODETYPE;
          data.filter.filter->nodetype = type;

          return TRUE;
        }
      else if (data.filter.filter->nodetype == type)
        return TRUE;
    }
  else if (transform == TRANSFORM_REVERSE_AXIS)
    return producer->TransformL (parser, transform, data);

  return FALSE;
}

XPath_NameTest::XPath_NameTest (XPath_Producer *producer)
  : XPath_NodeTypeTest (producer, XP_NODETEST_NAME)
{
}

/* static */ XPath_NameTest *
XPath_NameTest::MakeL (XPath_Producer *producer, XPath_NodeType nodetype, const XMLExpandedName &name)
{
  XPath_NameTest *test = OP_NEW_L (XPath_NameTest, (producer));
  OpStackAutoPtr<XPath_NameTest> anchor (test);
  test->type = XP_NODETEST_NAME;
  test->nodetype = nodetype;
  test->name.SetL (name);
  anchor.release ();
  return test;
}

/* virtual */ BOOL
XPath_NameTest::TransformL (XPath_Parser *parser, Transform transform, TransformData &data)
{
  if (transform == TRANSFORM_XMLTREEACCESSOR_FILTER && nodetype == XP_NODE_ELEMENT && XPath_NodeTypeTest::TransformL (parser, transform, data))
    {
      if ((data.filter.filter->flags & XPath_XMLTreeAccessorFilter::FLAG_HAS_ELEMENT_NAME) == 0)
        {
          data.filter.filter->flags |= XPath_XMLTreeAccessorFilter::FLAG_HAS_ELEMENT_NAME;

          XMLExpandedName filter_name;
          if (name.GetLocalPart ()[0] == '*' && !name.GetLocalPart ()[1])
            filter_name = XMLExpandedName (name.GetUri (), UNI_L (""));
          else
            filter_name = name;

          data.filter.filter->element_name.SetL (filter_name);
          return TRUE;
        }
    }
  else if (transform == TRANSFORM_REVERSE_AXIS)
    return producer->TransformL (parser, transform, data);

  return FALSE;
}

BOOL
XPath_CompareNames (const XMLExpandedName &wild, const XMLExpandedName &actual, BOOL case_sensitive)
{
  const uni_char *wild_localpart = wild.GetLocalPart ();
  if (wild_localpart && wild_localpart[0] == '*')
    {
      const uni_char *wild_uri = wild.GetUri (), *actual_uri = actual.GetUri ();
      if (actual_uri && uni_strcmp (wild_uri, actual_uri) == 0)
        return TRUE;
    }
  else
    {
      const uni_char *actual_localpart = actual.GetLocalPart ();
      if (wild_localpart && actual_localpart && (case_sensitive ? uni_strcmp (wild_localpart, actual_localpart) == 0 : uni_stricmp (wild_localpart, actual_localpart) == 0))
        {
          const uni_char *wild_uri = wild.GetUri (), *actual_uri = actual.GetUri ();
          if (!wild_uri && !actual_uri || wild_uri && actual_uri && uni_strcmp (wild_uri, actual_uri) == 0)
            return TRUE;
        }
    }
  return FALSE;
}

/* virtual */ BOOL
XPath_NameTest::MatchL (XPath_Context *context, XPath_Node *node)
{
  if (!XPath_NodeTypeTest::MatchL (context, node))
    return FALSE;

  XMLExpandedName nodename;
  node->GetExpandedName (nodename);

  return XPath_CompareNames (name, nodename, node->tree->IsCaseSensitive ());
}

XPath_PITest::XPath_PITest (XPath_Producer *producer)
  : XPath_Step::NodeTest (producer, XP_NODETEST_PI),
    target (0)
{
}

/* static */ XPath_PITest *
XPath_PITest::MakeL (XPath_Producer *producer, const uni_char *target, unsigned length)
{
  XPath_PITest *test = OP_NEW_L (XPath_PITest, (producer));
  OpStackAutoPtr<XPath_PITest> anchor (test);
  test->type = XP_NODETEST_PI;
  test->target = XPath_Utils::CopyStringL (target, length);
  anchor.release ();
  return test;
}

/* virtual */
XPath_PITest::~XPath_PITest ()
{
  OP_DELETEA (target);
}

/* virtual */ BOOL
XPath_PITest::MatchL (XPath_Context *context, XPath_Node *node)
{
  return node->GetType () == XP_NODE_PI && uni_strcmp (node->tree->GetPITarget (node->treenode), target) == 0;
}

#ifdef XPATH_CURRENTLY_UNUSED

XPath_HasAttributeTest::XPath_HasAttributeTest (XPath_Producer *producer)
  : XPath_Step::NodeTest (producer, XP_NODETEST_HASATTRIBUTE)
{
}

/* static */ XPath_HasAttributeTest *
XPath_HasAttributeTest::MakeL (XPath_Producer *producer, const XMLExpandedName &name)
{
  XPath_HasAttributeTest *test = OP_NEW_L (XPath_HasAttributeTest, (producer));
  OpStackAutoPtr<XPath_HasAttributeTest> anchor (test);
  test->type = XP_NODETEST_HASATTRIBUTE;
  test->name.SetL (name);
  anchor.release ();
  return test;
}

/* virtual */ BOOL
XPath_HasAttributeTest::MatchL (XPath_Context *context, XPath_Node *node)
{
  return node->GetType () == XP_NODE_ELEMENT && node->tree->HasAttribute (tree->GetAttributes (treenode, FALSE, TRUE), name);
}

XPath_AttributeValueTest::XPath_AttributeValueTest (XPath_Producer *producer)
  : XPath_Step::NodeTest (producer, XP_NODETEST_ATTRIBUTEVALUE),
    value (0)
{
}

/* static */ XPath_AttributeValueTest *
XPath_AttributeValueTest::MakeL (XPath_Producer *producer, const XMLExpandedName &name, XPath_Value *value, BOOL equal)
{
  XPath_AttributeValueTest *test = OP_NEW_L (XPath_AttributeValueTest, (producer));
  OpStackAutoPtr<XPath_AttributeValueTest> anchor (test);

  test->value = 0;
  test->name.SetL (name);
  test->equal = equal;

  TempBuffer buffer; ANCHOR (TempBuffer, buffer);

  test->value = XPath_Utils::CopyStringL (value->AsStringL (buffer));

  anchor.release ();
  return test;
}

/* virtual */
XPath_AttributeValueTest::~XPath_AttributeValueTest ()
{
  OP_DELETEA (value);
}

/* virtual */ BOOL
XPath_AttributeValueTest::MatchL (XPath_Context *context, XPath_Node *node)
{
  /* FIXME: Make sure to test this; the conditions here are wierd. */
  return node->GetType () == XP_NODE_ELEMENT ? !node->tree->HasAttribute (node->treenode, name, value) == !equal : !!*value == !equal;
}

#endif // XPATH_CURRENTLY_UNUSED

XPath_LastPredicate::XPath_LastPredicate (XPath_Parser *parser, XPath_Producer *producer)
  : XPath_Step::Predicate (parser, producer, 0),
    state_index (parser->GetStateIndex ()),
    previous_index (parser->GetNodeIndex ())
{
}

/* static */ XPath_LastPredicate *
XPath_LastPredicate::MakeL (XPath_Parser *parser, XPath_Producer *producer)
{
  return OP_NEW_L (XPath_LastPredicate, (parser, producer));
}

/* virtual */ BOOL
XPath_LastPredicate::Reset (XPath_Context *context, BOOL local_context_only)
{
  if (!local_context_only || !is_predicate_expression)
    {
      context->states[state_index] = 0;
      context->nodes[previous_index].Reset ();
      return Predicate::Reset (context, local_context_only);
    }
  else
    return FALSE;
}

/* virtual */ XPath_Node *
XPath_LastPredicate::GetNextNodeL (XPath_Context *context)
{
  unsigned &state = context->states[state_index];

  while (TRUE)
    {
      if (state == 1 && !Reset (context, TRUE))
        return 0;

      XPath_Node &previous = context->nodes[previous_index];

      while (XPath_Node *node = producer->GetNextNodeL (context))
        {
          previous.CopyL (node);
          XPath_Node::DecRef (context, node);

          XP_SEQUENCE_POINT (context);
        }

      state = 1;

      if (previous.IsValid ())
        return XPath_Node::IncRef (&previous);
    }
}

/* virtual */ void
XPath_LastPredicate::OptimizeL (unsigned hints)
{
  hints |= HINT_CONTEXT_DEPENDENT_PREDICATE;
  XPath_ChainProducer::OptimizeL (hints);
}

/* virtual */ XPath_Step::Predicate::MatchStatus
XPath_LastPredicate::MatchL (XPath_Context *context, XPath_Node *node)
{
  /* Not used; we have our own GetNextNodeL instead. */
  return MATCH_NOT_MATCHED;
}

XPath_ConstantPositionPredicate::XPath_ConstantPositionPredicate (XPath_Parser *parser, XPath_Producer *producer, unsigned position)
  : XPath_Step::Predicate (parser, producer, 0),
    position (position)
{
}

/* static */ XPath_ConstantPositionPredicate *
XPath_ConstantPositionPredicate::MakeL (XPath_Parser *parser, XPath_Producer *producer, unsigned position)
{
  return OP_NEW_L (XPath_ConstantPositionPredicate, (parser, producer, position));
}

/* virtual */ void
XPath_ConstantPositionPredicate::OptimizeL (unsigned hints)
{
  hints |= HINT_CONTEXT_DEPENDENT_PREDICATE;
  XPath_ChainProducer::OptimizeL (hints);
}

/* virtual */ XPath_Step::Predicate::MatchStatus
XPath_ConstantPositionPredicate::MatchL (XPath_Context *context, XPath_Node *node)
{
  XPath_ContextInformation &ci = context->cis[producer->ci_index];
  unsigned current = ++ci.position;

  if (current < position)
    return MATCH_NOT_MATCHED;
  else if (current == position)
    return MATCH_LAST_IN_LOCAL_CONTEXT;
  else
    return MATCH_NONE_IN_LOCAL_CONTEXT;
}

XPath_RegularPredicate::XPath_RegularPredicate (XPath_Parser *parser, XPath_Producer *producer, XPath_Expression *expression)
  : XPath_Step::Predicate (parser, producer, expression),
    state_index (parser->GetStateIndex ())
{
}

/* static */ XPath_RegularPredicate *
XPath_RegularPredicate::MakeL (XPath_Parser *parser, XPath_Producer *producer, XPath_Expression *expression)
{
  unsigned flags = expression->GetExpressionFlags ();

  OpStackAutoPtr<XPath_Expression> expression_anchor (NULL);
  XPath_BooleanExpression *condition = 0;
  XPath_NumberExpression *position = 0;
#ifdef XPATH_EXTENSION_SUPPORT
  XPath_Unknown *unknown = 0;

  if ((flags & XPath_Expression::FLAG_UNKNOWN) != 0)
    unknown = static_cast<XPath_Unknown *> (expression);
  else
#endif // XPATH_EXTENSION_SUPPORT
    {
      XPath_Expression *local_expression = 0;
      if ((flags & XPath_Expression::FLAG_NUMBER) != 0)
        local_expression = position = XPath_NumberExpression::MakeL (parser, expression);
      else
        local_expression = condition = XPath_BooleanExpression::MakeL (parser, expression);

      if (local_expression != expression)
        expression_anchor.reset (local_expression);

      expression = local_expression;
    }

  XPath_RegularPredicate *predicate = OP_NEW_L (XPath_RegularPredicate, (parser, producer, expression));

  if (!predicate)
      LEAVE (OpStatus::ERR_NO_MEMORY);

  predicate->condition = condition;
  predicate->position = position;
#ifdef XPATH_EXTENSION_SUPPORT
  if ((predicate->unknown = unknown) != 0)
    predicate->unknown_result_type_index = parser->GetStateIndex ();
#endif // XPATH_EXTENSION_SUPPORT

  expression_anchor.release ();
  return predicate;
}

/* virtual */ BOOL
XPath_RegularPredicate::Reset (XPath_Context *context, BOOL local_context_only)
{
  context->states[state_index] = 0;
  return Predicate::Reset (context, local_context_only);
}

/* virtual */ void
XPath_RegularPredicate::OptimizeL (unsigned hints)
{
  if ((GetPredicateExpressionFlags () & (XPath_Expression::FLAG_CONTEXT_SIZE | XPath_Expression::FLAG_CONTEXT_POSITION)) != 0)
    hints |= HINT_CONTEXT_DEPENDENT_PREDICATE;

  XPath_ChainProducer::OptimizeL (hints);
}

/* virtual */ XPath_Step::Predicate::MatchStatus
XPath_RegularPredicate::MatchL (XPath_Context *context, XPath_Node *node)
{
  unsigned &state = context->states[state_index];
  XPath_ContextInformation &ci = context->cis[producer->ci_index];

  BOOL initial;

  if (state == 0)
    {
      initial = TRUE;
      state = 1;
    }
  else
    initial = FALSE;

  XPath_Context local (context, node, ci.position + 1, ci.size);
  BOOL include;

  if (condition)
    include = condition->EvaluateToBooleanL (&local, initial);
#ifdef XPATH_EXTENSION_SUPPORT
  else if (position)
#else // XPATH_EXTENSION_SUPPORT
  else
#endif // XPATH_EXTENSION_SUPPORT
    {
      double result = position->EvaluateToNumberL (&local, initial);
      include = op_isintegral (result) && result == static_cast<double> (ci.position + 1);
    }
#ifdef XPATH_EXTENSION_SUPPORT
  else
    {
      XPath_ValueType result_type;

      if (state == 1)
        {
          context->states[unknown_result_type_index] = result_type = unknown->GetActualResultTypeL (&local, initial);
          state = 2;
        }
      else
        result_type = static_cast<XPath_ValueType> (context->states[unknown_result_type_index]);

      if (result_type == XP_VALUE_NODESET)
        if (XPath_Node *next_node = unknown->GetNextNodeL (&local))
          {
            XPath_Node::DecRef (context, next_node);
            return MATCH_MATCHED;
          }
        else
          return MATCH_NOT_MATCHED;
      else
        {
          XPath_ValueType types[4];
          types[XPath_Expression::WHEN_NUMBER] = XP_VALUE_NUMBER;
          types[XPath_Expression::WHEN_BOOLEAN] = XP_VALUE_BOOLEAN;
          types[XPath_Expression::WHEN_STRING] = XP_VALUE_BOOLEAN;
          types[XPath_Expression::WHEN_NODESET] = XP_VALUE_INVALID;

          XPath_Value *value = unknown->EvaluateL (&local, FALSE, types, result_type);

          if (value->type == XP_VALUE_NUMBER)
            include = op_isintegral (value->data.number) && value->data.number == static_cast<double> (ci.position + 1);
          else
            include = value->data.boolean;

          XPath_Value::DecRef (context, value);
        }
    }
#endif // XPATH_EXTENSION_SUPPORT

  ++ci.position;

  state = 0;

  return include ? MATCH_MATCHED : MATCH_NOT_MATCHED;
}

/* virtual */ BOOL
XPath_RegularPredicate::TransformL (XPath_Parser *parser, Transform transform, TransformData &data0)
{
  if (transform == TRANSFORM_XMLTREEACCESSOR_FILTER)
    {
      if (XPath_Step::Axis *axis = GetAxis ())
        if (axis->GetAxis () != XP_AXIS_ATTRIBUTE && axis->GetAxis () != XP_AXIS_NAMESPACE)
          {
            XPath_Expression::TransformData data;
            data.filter.filter = data0.filter.filter;
            data.filter.partial = FALSE;

            if (expression->TransformL (parser, XPath_Expression::TRANSFORM_XMLTREEACCESSOR_FILTER, data))
              {
                data0.filter.partial = data.filter.partial;
                return TRUE;
              }
          }
    }
  else if ((GetPredicateExpressionFlags () & XPath_Expression::FLAG_CONTEXT_POSITION) == 0)
    return producer->TransformL (parser, transform, data0);

  return FALSE;
}

XPath_SingleAttribute::XPath_SingleAttribute (XPath_Parser *parser, XPath_Producer *producer)
  : XPath_Step::Axis (parser, producer, XP_AXIS_ATTRIBUTE)
{
}

/* static */ XPath_SingleAttribute *
XPath_SingleAttribute::MakeL (XPath_Parser *parser, XPath_Producer *producer, const XMLExpandedName &name)
{
  XPath_SingleAttribute *single_attribute = OP_NEW (XPath_SingleAttribute, (parser, producer));

  if (!single_attribute || OpStatus::IsMemoryError (single_attribute->name.Set (name)))
    {
      if (single_attribute)
        OP_DELETE (single_attribute);
      else
        OP_DELETE (producer);

      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  return single_attribute;
}

/* virtual */ XPath_Node *
XPath_SingleAttribute::GetNextNodeL (XPath_Context *context)
{
  while (XPath_Node *node = producer->GetNextNodeL (context))
    if (node->type == XP_NODE_ELEMENT)
      {
        XMLTreeAccessor *tree = node->tree;
        XMLTreeAccessor::Node *treenode = node->treenode;

        XPath_Node::DecRef (context, node);

        XMLTreeAccessor::Attributes *attributes = tree->GetAttributes (treenode, FALSE, TRUE);
        if (tree->HasAttribute (attributes, name))
          {
            XMLCompleteName complete_name;

            if (name.GetUri ())
              {
                /* Need to figure out the attribute's prefix. */
                const uni_char *value;
                BOOL id, specified;

                OpStatus::Ignore (attributes->GetAttribute (tree->GetAttributeIndex (attributes, name), complete_name, value, id, specified, 0));
              }
            else
              complete_name = name;

            XPath_Node &node_stored = context->nodes[node_index];
            node_stored.SetAttributeL (tree, treenode, complete_name);
            return XPath_Node::IncRef (&node_stored);
          }
      }
    else
      XPath_Node::DecRef (context, node);

  context->nodes[node_index].Reset ();
  return 0;
}

/* virtual */ unsigned
XPath_SingleAttribute::GetProducerFlags ()
{
  /* We return one or zero attribute nodes from every element node from the
     producer, so if the producer claims to be returning nodes in a certain
     order, we return nodes in that order as well, and if the producer claims
     not to be producing the same node twice, then neither will we. */
  return producer->GetProducerFlags () & (MASK_ORDER | FLAG_NO_DUPLICATES | MASK_NEEDS | FLAG_BLOCKING);
}

/* virtual */ BOOL
XPath_SingleAttribute::TransformL (XPath_Parser *parser, Transform transform, TransformData &data)
{
  if (transform == TRANSFORM_ATTRIBUTE_NAME)
    {
      data.name = &name;
      return TRUE;
    }
  else
    return Axis::TransformL (parser, transform, data);
}

#endif // XPATH_SUPPORT
