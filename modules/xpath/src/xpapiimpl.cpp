/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xpapiimpl.h"
#include "modules/xpath/src/xpnamespaces.h"
#include "modules/xpath/src/xpexpr.h"
#include "modules/xpath/src/xpparser.h"
#include "modules/xpath/src/xpnode.h"
#include "modules/xpath/src/xpnodeset.h"
#include "modules/xpath/src/xpnodelist.h"
#include "modules/xpath/src/xpvalue.h"
#include "modules/xpath/src/xpunknown.h"
#include "modules/xpath/src/xppattern.h"
#include "modules/xpath/src/xputils.h"

#include "modules/xmlutils/xmlnames.h"
#include "modules/util/tempbuf.h"

/* static */ void
XPathNamespaces::Free (XPathNamespaces *namespaces)
{
  XPathNamespacesImpl *impl = static_cast<XPathNamespacesImpl *> (namespaces);
  OP_DELETE (impl);
}

/* static */ OP_STATUS
XPathNamespaces::Make (XPathNamespaces *&namespaces, XMLNamespaceDeclaration *nsdeclaration)
{
  XPath_Namespaces *storage = OP_NEW (XPath_Namespaces, ());

  if (!storage)
    return OpStatus::ERR_NO_MEMORY;

  namespaces = OP_NEW (XPathNamespacesImpl, (storage));

  XPath_Namespaces::DecRef (storage);

  if (!namespaces)
    return OpStatus::ERR_NO_MEMORY;

  OP_ASSERT (FALSE);

  return OpStatus::OK;
}

/* static */ OP_STATUS
XPathNamespaces::Make (XPathNamespaces *&namespaces)
{
  XPath_Namespaces *storage = OP_NEW (XPath_Namespaces, ());

  if (!storage)
    return OpStatus::ERR_NO_MEMORY;

  namespaces = OP_NEW (XPathNamespacesImpl, (storage));

  XPath_Namespaces::DecRef (storage);

  if (!namespaces)
    return OpStatus::ERR_NO_MEMORY;

  return OpStatus::OK;
}

/* static */ OP_STATUS
XPathNamespaces::Make (XPathNamespaces *&namespaces, const uni_char *expression)
{
  XPath_Namespaces *storage = OP_NEW (XPath_Namespaces, ());

  if (!storage)
    return OpStatus::ERR_NO_MEMORY;

  namespaces = OP_NEW (XPathNamespacesImpl, (storage));

  XPath_Namespaces::DecRef (storage);

  if (!namespaces)
    return OpStatus::ERR_NO_MEMORY;

  TRAPD (status, storage->CollectPrefixesFromExpressionL (expression));

  if (OpStatus::IsMemoryError (status))
    {
      OP_DELETE (namespaces);
      return status;
    }

  return OpStatus::OK;
}

/* virtual */
XPathNamespaces::~XPathNamespaces ()
{
}

XPathNamespacesImpl::XPathNamespacesImpl (XPath_Namespaces *storage)
  : storage (XPath_Namespaces::IncRef (storage))
{
}

XPathNamespacesImpl::~XPathNamespacesImpl ()
{
  XPath_Namespaces::DecRef (storage);
}

XPath_Namespaces *
XPathNamespacesImpl::GetStorage ()
{
  return storage;
}

/* virtual */ OP_STATUS
XPathNamespacesImpl::Add (const uni_char *prefix, const uni_char *uri)
{
  TRAPD (status, storage->SetL (prefix, uri));
  return status;
}

/* virtual */ OP_STATUS
XPathNamespacesImpl::Remove (const uni_char *prefix)
{
  TRAPD (status, storage->RemoveL (prefix));
  return status;
}

/* virtual */ unsigned
XPathNamespacesImpl::GetCount ()
{
  return storage->GetCount ();
}

/* virtual */ const uni_char *
XPathNamespacesImpl::GetPrefix (unsigned index)
{
  return storage->GetPrefix (index);
}

/* virtual */ const uni_char *
XPathNamespacesImpl::GetURI (unsigned index)
{
  return storage->GetURI (index);
}

/* virtual */ const uni_char *
XPathNamespacesImpl::GetURI (const uni_char *prefix)
{
  return storage->GetURI (prefix);
}

/* virtual */ OP_STATUS
XPathNamespacesImpl::SetURI (unsigned index, const uni_char *uri)
{
  TRAPD (status, storage->SetURIL (index, uri));
  return status;
}

/* static */ void
XPathNode::Free (XPathNode *node)
{
  XPathNodeImpl::DecRef ((XPathNodeImpl *) node);
}

static OP_STATUS
XPath_MakeNode (XPath_Node *&node, XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode)
{
  TRAPD (status, node = XPath_Node::MakeL (tree, treenode));
  return status;
}

static OP_STATUS
XPath_SetAttribute (XPath_Node *node, const XMLCompleteName &name)
{
  TRAPD (status, node->SetAttributeL (node->tree, node->treenode, name));
  return status;
}

static OP_STATUS
XPath_SetNamespace (XPath_Node *node, const uni_char *prefix, const uni_char *uri)
{
  TRAPD (status, node->SetNamespaceL (node->tree, node->treenode, prefix, uri));
  return status;
}

/* static */ OP_STATUS
XPathNode::Make (XPathNode *&node, XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode)
{
  XPath_Node *n = 0;

  XMLTreeAccessor::NodeType treenode_type = tree->GetNodeType (treenode);
  if (treenode_type == XMLTreeAccessor::TYPE_TEXT || treenode_type == XMLTreeAccessor::TYPE_CDATA_SECTION)
    while (XMLTreeAccessor::Node *previous_sibling = tree->GetPreviousSibling (treenode))
      {
        XMLTreeAccessor::NodeType previous_sibling_type = tree->GetNodeType (previous_sibling);
        if (previous_sibling_type == XMLTreeAccessor::TYPE_TEXT || previous_sibling_type == XMLTreeAccessor::TYPE_CDATA_SECTION)
          treenode = previous_sibling;
        else
          break;
      }


  RETURN_IF_ERROR (XPath_MakeNode (n, tree, treenode));

  node = OP_NEW (XPathNodeImpl, (n));

  if (!node)
    {
      XPath_Node::DecRef (NULL, n);
      return OpStatus::ERR_NO_MEMORY;
    }

  return OpStatus::OK;
}

/* static */ OP_STATUS
XPathNode::MakeAttribute (XPathNode *&node, XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode, const XMLCompleteName &name)
{
  RETURN_IF_ERROR (Make (node, tree, treenode));

  OP_STATUS status = XPath_SetAttribute (((XPathNodeImpl *) node)->GetInternalNode (), name);

  if (OpStatus::IsError (status))
    {
      Free (node);
      return status;
    }

  return OpStatus::OK;
}

/* static */ OP_STATUS
XPathNode::MakeNamespace (XPathNode *&node, XMLTreeAccessor *tree, XMLTreeAccessor::Node *treenode, const uni_char *prefix, const uni_char *uri)
{
  RETURN_IF_ERROR (Make (node, tree, treenode));

  OP_STATUS status = XPath_SetNamespace (((XPathNodeImpl *) node)->GetInternalNode (), prefix, uri);

  if (OpStatus::IsError (status))
    {
      Free (node);
      return status;
    }

  return OpStatus::OK;
}

#ifdef XPATH_EXTENSION_SUPPORT

/* static */ OP_STATUS
XPathNode::MakeCopy (XPathNode *&node, XPathNode *original0)
{
  XPathNodeImpl *original = static_cast<XPathNodeImpl *> (original0);
  XPath_Node *internal = original->GetInternalNode ();

  if (internal->IsIndependent ())
    node = XPathNodeImpl::IncRef (original);
  else
    {
      XPath_Node *internal_copy;

      RETURN_IF_ERROR (XPath_MakeNode (internal_copy, internal->tree, internal->treenode));

      if (internal->type == XP_NODE_ATTRIBUTE)
        RETURN_IF_ERROR (XPath_SetAttribute (internal_copy, internal->name));
      else if (internal->type == XP_NODE_NAMESPACE)
        RETURN_IF_ERROR (XPath_SetNamespace (internal_copy, internal->name.GetLocalPart (), internal->name.GetUri ()));

      RETURN_IF_ERROR (XPathNodeImpl::Make (node, internal_copy));
    }

  return OpStatus::OK;
}

#endif // XPATH_EXTENSION_SUPPORT

/* statuc */ BOOL
XPathNode::IsSameNode (XPathNode *node1, XPathNode *node2)
{
  return XPath_Node::Equals (((XPathNodeImpl *) node1)->GetInternalNode (), ((XPathNodeImpl *) node2)->GetInternalNode ());
}

/* virtual */
XPathNode::~XPathNode ()
{
}

static void
XPath_DecRefViaContext (XPath_Node *node, XPath_GlobalContext *global_context)
{
  if (global_context)
    {
      XPath_Context context (global_context, 0, 0, 0);
      XPath_Node::DecRef (&context, node);
    }
  else
    XPath_Node::DecRef (0, node);
}

/* static */ OP_STATUS
XPathNodeImpl::Make (XPathNode *&node, XPath_Node *n, XPath_GlobalContext *global_context)
{
  OP_ASSERT(n);
  node = OP_NEW (XPathNodeImpl, (n, global_context));

  if (!node)
    {
      XPath_DecRefViaContext (n, global_context);
      return OpStatus::ERR_NO_MEMORY;
    }

  return OpStatus::OK;
}

XPathNodeImpl::XPathNodeImpl (XPath_Node *node, XPath_GlobalContext *global_context)
  : node (node),
    global_context (global_context),
    refcount (1)
{
}

XPathNodeImpl::~XPathNodeImpl ()
{
  XPath_DecRefViaContext (node, global_context);
}

XPath_Node *
XPathNodeImpl::GetInternalNode ()
{
  return node;
}

/* virtual */ XPathNode::Type
XPathNodeImpl::GetType ()
{
  unsigned type = 0, nodetype = node->type;

  while ((nodetype & 1) == 0)
    ++type, nodetype >>= 1;

  return (Type) type;
}

/* virtual */ XMLTreeAccessor *
XPathNodeImpl::GetTreeAccessor ()
{
  return node->tree;
}

/* virtual */ XMLTreeAccessor::Node *
XPathNodeImpl::GetNode ()
{
  return node->treenode;
}

/* virtual */ void
XPathNodeImpl::GetNodeName (XMLCompleteName &name)
{
  switch (node->GetType ())
    {
    case XP_NODE_ELEMENT:
      node->tree->GetName (name, node->treenode);
      break;

    case XP_NODE_ATTRIBUTE:
      name = node->name;
    }
}

/* virtual */ void
XPathNodeImpl::GetExpandedName (XMLExpandedName &name)
{
  switch (node->GetType ())
    {
    case XP_NODE_ROOT:
    case XP_NODE_TEXT:
    case XP_NODE_COMMENT:
      break;

    default:
      node->GetExpandedName (name);
    }
}

/* virtual */ OP_STATUS
XPathNodeImpl::GetStringValue (TempBuffer &value)
{
  return node->GetStringValue (value);
}

/* virtual */ BOOL
XPathNodeImpl::IsWhitespaceOnly ()
{
  return node->IsWhitespaceOnly ();
}

/* virtual */ const uni_char *
XPathNodeImpl::GetNamespacePrefix ()
{
  if (node->type == XP_NODE_NAMESPACE)
    return node->name.GetLocalPart ();
  else
    return 0;
}

/* virtual */ const uni_char *
XPathNodeImpl::GetNamespaceURI ()
{
  if (node->type == XP_NODE_NAMESPACE)
    return node->name.GetUri ();
  else
    return 0;
}

/* static */ XPathNodeImpl *
XPathNodeImpl::IncRef (XPathNode *node)
{
  if (node)
    ++static_cast<XPathNodeImpl *> (node)->refcount;
  return static_cast<XPathNodeImpl *> (node);
}

/* static */ void
XPathNodeImpl::DecRef (XPathNode *node)
{
  XPathNodeImpl *impl = static_cast<XPathNodeImpl *> (node);
  if (impl && --impl->refcount == 0)
    OP_DELETE (impl);
}

#ifdef XPATH_NODE_PROFILE_SUPPORT

/* static */ OP_STATUS
XPathNodeProfile::Make (XPathNodeProfile *&profile, XPathNode::Type type)
{
  XPathNodeProfileImpl *impl = OP_NEW (XPathNodeProfileImpl, ());

  if (!impl)
    return OpStatus::ERR_NO_MEMORY;

  if (type == XPathNode::TEXT_NODE)
    impl->source.Set("text()");
  else if (type == XPathNode::COMMENT_NODE)
    impl->source.Set("comment()");

  if (type == XPathNode::ATTRIBUTE_NODE)
    {
      impl->SetExcludesRegulars ();
      impl->SetExcludesRoots ();
      impl->SetExcludesNamespaces ();
    }
  else if (type == XPathNode::NAMESPACE_NODE)
    {
      impl->SetExcludesRegulars ();
      impl->SetExcludesRoots ();
      impl->SetExcludesAttributes ();
    }
  else if (type == XPathNode::ROOT_NODE)
    {
      impl->SetExcludesRegulars ();
      impl->SetExcludesAttributes ();
      impl->SetExcludesNamespaces ();
    }
  else
    {
      impl->SetExcludesRoots ();
      impl->SetExcludesAttributes ();
      impl->SetExcludesNamespaces ();

      XPath_XMLTreeAccessorFilter **filters = OP_NEWA (XPath_XMLTreeAccessorFilter *, 1);
      if (!filters)
        {
          OP_DELETE (impl);
          return OpStatus::ERR_NO_MEMORY;
        }

      impl->GetSingleFilter ()->flags = XPath_XMLTreeAccessorFilter::FLAG_HAS_NODETYPE;
      impl->GetSingleFilter ()->nodetype = XPath_Utils::ConvertNodeType (type);

      filters[0] = impl->GetSingleFilter ();
      impl->SetFilters (filters, 1);
    }

  profile = impl;
  return OpStatus::OK;
}

/* static */ void
XPathNodeProfile::Free (XPathNodeProfile *profile)
{
  XPathNodeProfileImpl *impl = static_cast<XPathNodeProfileImpl *> (profile);
  OP_DELETE (impl);
}

/* virtual */
XPathNodeProfile::~XPathNodeProfile ()
{
}

/* virtual */
XPathNodeProfileImpl::~XPathNodeProfileImpl ()
{
  OP_DELETEA (filters);
}

/* virtual */ BOOL
XPathNodeProfileImpl::Includes (XPathNode *node0)
{
  XPath_Node *node = static_cast<XPathNodeImpl *> (node0)->GetInternalNode ();

  if (node->type == XP_NODE_ATTRIBUTE)
    return includes_attributes;
  else if (node->type == XP_NODE_NAMESPACE)
    return includes_namespaces;
  else if (filters_count != 0)
    {
      XMLTreeAccessor *tree = node->tree;
      XMLTreeAccessor::Node *treenode = node->treenode;

      for (unsigned index = 0; index < filters_count; ++index)
        {
          filters[index]->SetFilter (tree);
          BOOL included = node->tree->FilterNode (treenode);
          tree->ResetFilters ();
          if (included)
            return TRUE;
        }

      return FALSE;
    }
  else
    /* No filters typically means "we have no idea" rather than "includes
       nothing." */
    return TRUE;
}

#endif // XPATH_NODE_PROFILE_SUPPORT

XPathExpression::ExpressionData::ExpressionData ()
  : source (0)
  , namespaces (0)
#ifdef XPATH_EXTENSION_SUPPORT
  , extensions (0)
#endif // XPATH_EXTENSION_SUPPORT
#ifdef XPATH_ERRORS
  , error_message (0)
#endif // XPATH_ERRORS
{
}

/* static */ OP_STATUS
XPathExpression::Make (XPathExpression *&expression, const ExpressionData &data)
{
  XPathExpressionImpl *impl = OP_NEW (XPathExpressionImpl, ());

  if (!impl)
    return OpStatus::ERR_NO_MEMORY;

  OpAutoPtr<XPathExpressionImpl> impl_anchor (impl);

  impl->namespaces = data.namespaces ? XPath_Namespaces::IncRef (static_cast<XPathNamespacesImpl *> (data.namespaces)->GetStorage ()) : 0;
#ifdef XPATH_EXTENSION_SUPPORT
  impl->extensions = data.extensions;
#endif // XPATH_EXTENSION_SUPPORT

  RETURN_IF_ERROR (impl->SetSource (data.source));

#ifdef XPATH_ERRORS
  RETURN_IF_ERROR (impl->Compile (data.error_message));
#else // XPATH_ERRORS
  RETURN_IF_ERROR (impl->Compile ());
#endif // XPATH_ERRORS

  if (!impl->primitive && !impl->unordered_single)
    return OpStatus::ERR;

  expression = XPathExpressionImpl::IncRef (impl);
  impl_anchor.release ();

  return OpStatus::OK;
}

/* static */ OP_STATUS
XPathExpression::Make (XPathExpression *&expression, const uni_char *source, XPathNamespaces *namespaces)
{
  ExpressionData data;
  data.source = source;
  data.namespaces = namespaces;
  return Make (expression, data);
}

#ifdef XPATH_EXTENSION_SUPPORT

/* static */ OP_STATUS
XPathExpression::Make (XPathExpression *&expression, const uni_char *source, XPathNamespaces *namespaces, XPathExtensions *extensions)
{
  ExpressionData data;
  data.source = source;
  data.namespaces = namespaces;
  data.extensions = extensions;
  return Make (expression, data);
}

#endif // XPATH_EXTENSION_SUPPORT

/* static */ void
XPathExpression::Free (XPathExpression *expression)
{
  XPathExpressionImpl::DecRef (static_cast<XPathExpressionImpl *> (expression));
}

/* virtual */
XPathExpression::~XPathExpression ()
{
}

void
#ifdef XPATH_ERRORS
XPathExpressionImpl::CompileL (OpString *error_message)
#else // XPATH_ERRORS
XPathExpressionImpl::CompileL ()
#endif // XPATH_ERRORS
{
  XPath_Parser parser_nodeset (namespaces, source.CStr ()); ANCHOR (XPath_Parser, parser_nodeset);
#ifdef XPATH_EXTENSION_SUPPORT
  parser_nodeset.SetExtensions (extensions);
#endif // XPATH_EXTENSION_SUPPORT
#ifdef XPATH_ERRORS
  parser_nodeset.SetErrorMessageStorage (error_message);
#endif // XPATH_ERRORS
  unordered_single = parser_nodeset.ParseToProducerL (FALSE, FALSE);

  if (unordered_single)
    {
#ifdef XPATH_EXTENSION_SUPPORT
      nodeset_readers = parser_nodeset.TakeVariableReaders ();
#endif // XPATH_EXTENSION_SUPPORT

      unsigned unordered_single_flags = unordered_single->GetProducerFlags ();

      if ((unordered_single_flags & XPath_Producer::FLAG_NO_DUPLICATES) != 0)
        {
          unordered = unordered_single;

          if ((unordered_single_flags & XPath_Producer::FLAG_DOCUMENT_ORDER) != 0)
            ordered = unordered_single;
        }

      if ((unordered_single_flags & XPath_Producer::FLAG_DOCUMENT_ORDER) != 0)
        ordered_single = unordered_single;

      if (!unordered)
        unordered = XPath_Producer::EnsureFlagsL (&parser_nodeset, unordered_single, XPath_Producer::FLAG_NO_DUPLICATES | XPath_Producer::FLAG_SECONDARY_WRAPPER);

      if (!ordered_single)
        {
          ordered_single = XPath_Producer::EnsureFlagsL (&parser_nodeset, unordered_single, XPath_Producer::FLAG_DOCUMENT_ORDER | XPath_Producer::FLAG_SECONDARY_WRAPPER);

          if ((ordered_single->GetProducerFlags () & XPath_Producer::FLAG_NO_DUPLICATES) != 0)
            ordered = ordered_single;
        }

      if (!ordered)
        ordered = XPath_Producer::EnsureFlagsL (&parser_nodeset, unordered_single, XPath_Producer::FLAG_DOCUMENT_ORDER | XPath_Producer::FLAG_NO_DUPLICATES | XPath_Producer::FLAG_SECONDARY_WRAPPER);

      parser_nodeset.CopyStateSizes (nodeset_state_sizes);
    }
  else
    {
      XPath_Parser parser_primitive (namespaces, source.CStr ()); ANCHOR (XPath_Parser, parser_primitive);
#ifdef XPATH_EXTENSION_SUPPORT
      parser_primitive.SetExtensions (extensions);
#endif // XPATH_EXTENSION_SUPPORT
#ifdef XPATH_ERRORS
      parser_primitive.SetErrorMessageStorage (error_message);
#endif // XPATH_ERRORS
      primitive = parser_primitive.ParseToPrimitiveL ();
#ifdef XPATH_EXTENSION_SUPPORT
      primitive_readers = parser_primitive.TakeVariableReaders ();

      if (primitive->HasFlag (XPath_Expression::FLAG_UNKNOWN) && primitive->GetResultType () == XP_VALUE_INVALID)
        {
          unordered_single = static_cast<XPath_Unknown *> (primitive);

          BOOL is_ordered = (unordered_single->GetProducerFlags () & XPath_Producer::FLAG_DOCUMENT_ORDER) != 0;
          BOOL has_no_duplicates = (unordered_single->GetProducerFlags () & XPath_Producer::FLAG_NO_DUPLICATES) != 0;

          if (is_ordered)
            {
              ordered_single = unordered_single;

              if (has_no_duplicates)
                ordered = unordered_single;
            }
          if (has_no_duplicates)
            unordered = unordered_single;

          if (!ordered_single)
            {
              ordered_single = XPath_Producer::EnsureFlagsL (&parser_primitive, unordered_single, XPath_Producer::FLAG_DOCUMENT_ORDER | XPath_Producer::FLAG_SECONDARY_WRAPPER);

              if ((ordered_single->GetProducerFlags () & XPath_Producer::FLAG_NO_DUPLICATES) != 0)
                ordered = ordered_single;
            }

          if (!unordered)
            unordered = XPath_Producer::EnsureFlagsL (&parser_primitive, unordered_single, XPath_Producer::FLAG_NO_DUPLICATES | XPath_Producer::FLAG_SECONDARY_WRAPPER);

          if (!ordered)
            ordered = XPath_Producer::EnsureFlagsL (&parser_primitive, unordered_single, XPath_Producer::FLAG_DOCUMENT_ORDER | XPath_Producer::FLAG_NO_DUPLICATES | XPath_Producer::FLAG_SECONDARY_WRAPPER);

          parser_primitive.CopyStateSizes (nodeset_state_sizes);
          nodeset_readers = primitive_readers;
        }
#endif // XPATH_EXTENSION_SUPPORT

      parser_primitive.CopyStateSizes (primitive_state_sizes);
    }
}

XPathExpressionImpl::XPathExpressionImpl ()
  : namespaces (NULL),
    refcount (0),
    cached_evaluate (0),
    primitive (0),
    ordered_single (0),
    unordered_single (0),
    ordered (0),
    unordered (0)
{
#ifdef XPATH_EXTENSION_SUPPORT
  nodeset_readers = 0;
  primitive_readers = 0;
#endif // XPATH_EXTENSION_SUPPORT
}

XPathExpressionImpl::~XPathExpressionImpl ()
{
  OP_DELETE (cached_evaluate);

  OP_ASSERT (refcount == 0);

  XPath_Namespaces::DecRef (namespaces);

  if (ordered == unordered || ordered == ordered_single)
    ordered = 0;
  if (ordered_single == unordered_single)
    ordered_single = 0;
  if (unordered == unordered_single)
    unordered = 0;

#ifdef XPATH_EXTENSION_SUPPORT
  if (primitive && unordered_single)
    {
      OP_ASSERT (primitive->HasFlag (XPath_Expression::FLAG_UNKNOWN) && static_cast<XPath_Unknown *> (primitive) == unordered_single);
      unordered_single = 0;
      nodeset_readers = 0;
    }
#endif // XPATH_EXTENSION_SUPPORT

  OP_DELETE (primitive);
  OP_DELETE (ordered_single);
  OP_DELETE (unordered_single);
  OP_DELETE (ordered);
  OP_DELETE (unordered);

#ifdef XPATH_EXTENSION_SUPPORT
  OP_DELETE (nodeset_readers);
  OP_DELETE (primitive_readers);
#endif // XPATH_EXTENSION_SUPPORT
}

/* virtual */ unsigned
XPathExpressionImpl::GetExpressionFlags ()
{
  unsigned return_flags = 0, expression_flags;

  if (unordered_single)
    expression_flags = unordered_single->GetExpressionFlags ();
  else
    expression_flags = primitive->GetExpressionFlags ();

  if ((expression_flags & XPath_Expression::FLAG_CONTEXT_SIZE))
    return_flags |= XPathExpression::FLAG_NEEDS_CONTEXT_SIZE;
  if ((expression_flags & XPath_Expression::FLAG_CONTEXT_POSITION))
    return_flags |= XPathExpression::FLAG_NEEDS_CONTEXT_POSITION;

  return return_flags;
}

#ifdef XPATH_NODE_PROFILE_SUPPORT

/* virtual */ OP_STATUS
XPathExpressionImpl::GetNodeProfile (XPathNodeProfile *&profile)
{
  profile = 0;

  /* All producers would return the same set of nodes, and the only difference
     in practice is that some are nodeset filters, nodeset collectors or
     nodelist collectors, used to filter out duplicates or ensure order, that
     don't affect the node profile.  By asking 'unordered_single', we happen to
     bypass them. */
  if (unordered_single)
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

      TRAPD (status, handled = unordered_single->TransformL (0, XPath_Producer::TRANSFORM_NODE_PROFILE, data));

      if (OpStatus::IsMemoryError (status))
        return status;
      else if (handled)
        {
          if (!data.nodeprofile.includes_regulars)
            nodeprofile.SetExcludesRegulars ();
          else if (data.nodeprofile.filters_count != 0)
            nodeprofile.SetFilters (data.nodeprofile.filters, data.nodeprofile.filters_count);

          if (!data.nodeprofile.includes_roots)
            nodeprofile.SetExcludesRoots ();
          if (!data.nodeprofile.includes_attributes)
            nodeprofile.SetExcludesAttributes ();
          if (!data.nodeprofile.includes_namespaces)
            nodeprofile.SetExcludesNamespaces ();

          nodeprofile.source.Set (source);

          profile = &nodeprofile;
        }
    }

  return OpStatus::OK;
}

#endif // XPATH_NODE_PROFILE_SUPPORT

#ifdef XPATH_ERRORS

OP_STATUS
XPathExpressionImpl::Compile (OpString *error_message)
{
  TRAPD (status, CompileL (error_message));
  return status;
}

#else // XPATH_ERRORS

OP_STATUS
XPathExpressionImpl::Compile ()
{
  TRAPD (status, CompileL ());
  return status;
}

#endif // XPATH_ERRORS

/* static */ XPathExpressionImpl *
XPathExpressionImpl::IncRef (XPathExpressionImpl *expression)
{
  if (expression)
    ++expression->refcount;
  return expression;
}

/* static */ void
XPathExpressionImpl::DecRef (XPathExpressionImpl *expression)
{
  if (expression && --expression->refcount == 0)
    OP_DELETE (expression);
}

/* static */ OP_STATUS
XPathExpression::Evaluate::Make (Evaluate *&evaluate, XPathExpression *expression)
{
  XPathExpressionImpl *impl = static_cast<XPathExpressionImpl *> (expression);

  if (impl->cached_evaluate)
    {
      XPathExpressionImpl::IncRef (impl);

      evaluate = impl->cached_evaluate;
      impl->cached_evaluate = 0;

      return OpStatus::OK;
    }
  else
    {
      evaluate = OP_NEW (XPathExpressionEvaluateImpl, ((XPathExpressionImpl *) expression));
      return evaluate ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
    }
}

/* static */ void
XPathExpression::Evaluate::Free (Evaluate *evaluate)
{
  if (evaluate)
    {
      XPathExpressionEvaluateImpl *impl = static_cast<XPathExpressionEvaluateImpl *> (evaluate);

      unsigned refcount = impl->expression->refcount;

      /* Might destroy impl->expression. */
      impl->Reset ();

      /* If the expression's reference count was one, then this evaluate object
         was the only thing keeping it alive, and the call to Reset above would
         have destroyed it. */
      if (refcount > 1 && !impl->expression->cached_evaluate)
        impl->expression->cached_evaluate = impl;
      else
        OP_DELETE (impl);
    }
}

/* virtual */
XPathExpression::Evaluate::~Evaluate ()
{
}

OP_STATUS
XPathExpressionEvaluateImpl::Start (const XPath_ContextStateSizes *state_sizes)
{
  RETURN_IF_ERROR (global_context.AllocateStates (*state_sizes));

#ifdef XPATH_EXTENSION_SUPPORT
  if (expression->nodeset_readers || expression->primitive_readers)
    {
      XPath_VariableReader *reader;
      XPath_Context context (&global_context, 0, 0, 0);

      reader = expression->nodeset_readers;
      while (reader)
        {
          reader->Start (&context);
          reader = reader->GetNext ();
        }

      reader = expression->primitive_readers;
      while (reader)
        {
          reader->Start (&context);
          reader = reader->GetNext ();
        }
    }
#endif // XPATH_EXTENSION_SUPPORT

#ifdef XPATH_ERRORS
  error_message.Empty ();
  global_context.error_message = &error_message;
  global_context.source = expression->GetSource ();
#endif // XPATH_ERRORS

  started = TRUE;
  return OpStatus::OK;
}

void
XPathExpressionEvaluateImpl::Reset (BOOL from_constructor)
{
  if (!from_constructor)
    {
      XPathNodeImpl::DecRef (context_node);

      if (started)
        {
          XPathNodeImpl::DecRef (static_cast<XPathNodeImpl *> (result_node));
          result_string.Empty ();

          if (result_nodelist)
            {
              XPath_Context context (&global_context, 0, 0, 0);
              result_nodelist->Clear (&context);
              OP_DELETE (result_nodelist);
            }

          if (result_nodes)
            {
              for (unsigned index = 0; index < result_nodes_count; ++index)
                XPathNodeImpl::DecRef (static_cast<XPathNodeImpl *> (result_nodes[index]));
              OP_DELETEA (result_nodes);
            }

#ifdef XPATH_EXTENSION_SUPPORT
          if (expression->nodeset_readers || expression->primitive_readers)
            {
              XPath_VariableReader *reader;
              XPath_Context context (&global_context, 0, 0, 0);

              reader = expression->nodeset_readers;
              while (reader)
                {
                  reader->Finish (&context);
                  reader = reader->GetNext ();
                }

              reader = expression->primitive_readers;
              while (reader)
                {
                  reader->Finish (&context);
                  reader = reader->GetNext ();
                }
            }
#endif // XPATH_EXTENSION_SUPPORT

          global_context.Clean (failed);
        }

      XPathExpressionImpl::DecRef (expression);
    }

  context_node = 0;
  context_position = 0;
  context_size = 0;
  when_number = PRIMITIVE_NUMBER;
  when_boolean = PRIMITIVE_BOOLEAN;
  when_string = PRIMITIVE_STRING;
  when_nodeset = NODESET_ITERATOR;
  started = finished = failed = FALSE;
  need_reset = TRUE;
  producer = 0;
  result_string.Empty ();
  result_node = 0;
  result_nodelist = 0;
  result_nodes = 0;
  result_nodes_count = 0;
}

XPathExpressionEvaluateImpl::XPathExpressionEvaluateImpl (XPathExpressionImpl *expression)
  : expression (XPathExpressionImpl::IncRef (expression))
{
  Reset (TRUE);
}

/* virtual */
XPathExpressionEvaluateImpl::~XPathExpressionEvaluateImpl ()
{
}

/* virtual */ void
XPathExpressionEvaluateImpl::SetContext (XPathNode *node, unsigned position, unsigned size)
{
  context_node = static_cast<XPathNodeImpl *> (node);
  context_position = position;
  context_size = size;
}

#ifdef XPATH_EXTENSION_SUPPORT

/* virtual */ void
XPathExpressionEvaluateImpl::SetExtensionsContext (XPathExtensions::Context *extensions_context)
{
  global_context.extensions_context = extensions_context;
}

#endif // XPATH_EXTENSION_SUPPORT

/* virtual */ void
XPathExpressionEvaluateImpl::SetCostLimit (unsigned limit)
{
  /* FIXME: Implement this. */
}

/* virtual */ unsigned
XPathExpressionEvaluateImpl::GetLastOperationCost ()
{
  /* FIXME: Implement this. */
  return 0;
}

/* virtual */ void
XPathExpressionEvaluateImpl::SetRequestedType (unsigned type)
{
  /* Can't change our minds once we've started. */
  OP_ASSERT (!started || type == when_number && type == when_boolean && type == when_string && type == when_nodeset);

  when_nodeset = type;
  // Can only convert primitive types to other primitive types
  if (type != PRIMITIVE_NUMBER && type != PRIMITIVE_BOOLEAN && type != PRIMITIVE_STRING)
    when_number = when_boolean = when_string = TYPE_INVALID;
  else
    when_number = when_boolean = when_string = type;
}

/* virtual */ void
XPathExpressionEvaluateImpl::SetRequestedType (Type when_number0, Type when_boolean0, Type when_string0, unsigned when_nodeset0)
{
  /* Can't change our minds once we've started. */
  OP_ASSERT (!started || when_number == static_cast<unsigned> (when_number0) && when_boolean == static_cast<unsigned> (when_boolean0) && when_string == static_cast<unsigned> (when_string0) && when_nodeset == when_nodeset0);

  // Can only convert primitive types to other primitive types
  if (when_number0 != PRIMITIVE_NUMBER && when_number0 != PRIMITIVE_BOOLEAN && when_number0 != PRIMITIVE_STRING)
    when_number = TYPE_INVALID;
  else
    when_number = when_number0;

  if (when_boolean0 != PRIMITIVE_NUMBER && when_boolean0 != PRIMITIVE_BOOLEAN && when_boolean0 != PRIMITIVE_STRING)
    when_boolean = TYPE_INVALID;
  else
    when_boolean = when_boolean0;

  if (when_string0 != PRIMITIVE_NUMBER && when_string0 != PRIMITIVE_BOOLEAN && when_string0 != PRIMITIVE_STRING)
    when_string = TYPE_INVALID;
  else
    when_string = when_string0;

  when_nodeset = when_nodeset0;
}

#ifdef XPATH_EXTENSION_SUPPORT

static OP_BOOLEAN
XPath_GetActualResultType (XPath_ValueType &type, XPath_Expression *expression, XPath_Context *context, BOOL initial)
{
  TRAPD (result, type = static_cast <XPath_Unknown *> (expression)->GetActualResultTypeL (context, initial));
  return result == OpStatus::OK ? OpBoolean::IS_TRUE : result;
}

#endif // XPATH_EXTENSION_SUPPORT

/* virtual */ OP_BOOLEAN
XPathExpressionEvaluateImpl::CheckResultType (unsigned &type)
{
  XPath_Producer *use_producer = expression->ordered_single;
  XPath_ValueType internal_type = XP_VALUE_INVALID;

#ifdef XPATH_EXTENSION_SUPPORT
  if (expression->primitive && expression->primitive->HasFlag (XPath_Expression::FLAG_UNKNOWN))
    {
      BOOL initial = !started;

      if (initial)
        RETURN_IF_ERROR (Start (&expression->primitive_state_sizes));

      XPath_Context context (&global_context, context_node->GetInternalNode (), context_position, context_size);

      OP_BOOLEAN finished = XPath_GetActualResultType (internal_type, expression->primitive, &context, initial);

      if (finished != OpBoolean::IS_TRUE)
        {
          if (finished == OpStatus::ERR)
            failed = TRUE;

          return finished;
        }

      if (internal_type != XP_VALUE_NODESET)
        use_producer = 0;
    }
#endif // XPATH_EXTENSION_SUPPORT

  type = TYPE_INVALID;

  if (use_producer)
    /* Could compile a producer; means the intrinsic type of the expression
       is a node-set. */
    type = when_nodeset;
  else
    {
      /* Otherwise intrinsic type was a primitive. */
      unsigned resulttype = internal_type != XP_VALUE_INVALID ? static_cast<unsigned> (internal_type) : expression->primitive->GetResultType ();

      switch (resulttype)
        {
        case XP_VALUE_NUMBER:
          type = when_number;
          break;

        case XP_VALUE_BOOLEAN:
          type = when_boolean;
          break;

        case XP_VALUE_STRING:
          type = when_string;
          break;

        default:
          /* If the expression's intrinsic type was a node-set, we should've
             had producers and not end up here. */
          OP_ASSERT (FALSE);
        }
    }

  if (type == TYPE_INVALID)
    {
      failed = TRUE;
      return OpStatus::ERR;
    }
  else
    return OpBoolean::IS_TRUE;
}

static OP_BOOLEAN
XPath_EvaluateExpression (XPath_Value *&value, XPath_Expression *expression, XPath_Context *context, BOOL initial, XPath_ValueType type)
{
  XPath_ValueType types[4] = { type, type, type, type };
  TRAPD (result, value = expression->EvaluateL (context, initial, types));
  return result == OpStatus::OK ? OpBoolean::IS_TRUE : result;
}

/* virtual */ OP_BOOLEAN
XPathExpressionEvaluateImpl::GetNumberResult (double &result)
{
  if (!finished)
    {
      BOOL initial;

      if (!started)
        {
          XPath_ContextStateSizes *state_sizes;

          if (expression->ordered_single)
            state_sizes = &expression->nodeset_state_sizes;
          else
            state_sizes = &expression->primitive_state_sizes;

          RETURN_IF_ERROR (Start (state_sizes));
          initial = TRUE;
        }
      else
        initial = FALSE;

      XPath_Context context (&global_context, context_node->GetInternalNode (), context_position, context_size);
      XPath_Producer *use_producer = expression->ordered_single;

#ifdef XPATH_EXTENSION_SUPPORT
      if (expression->primitive && expression->primitive->HasFlag (XPath_Expression::FLAG_UNKNOWN))
        {
          XPath_ValueType type;

          OP_BOOLEAN finished = XPath_GetActualResultType (type, expression->primitive, &context, initial);

          if (finished != OpBoolean::IS_TRUE)
            {
              if (finished == OpStatus::ERR)
                failed = TRUE;

              return finished;
            }

          initial = FALSE;

          if (type != XP_VALUE_NODESET)
            use_producer = 0;
        }
#endif // XPATH_EXTENSION_SUPPORT

      if (use_producer)
        {
          if (need_reset)
            {
              use_producer->Reset (&context);
              need_reset = FALSE;
            }

          XPath_Node *node;

          OP_BOOLEAN is_finished = use_producer->GetNextNode (node, &context);

          if (is_finished == OpBoolean::IS_TRUE)
            {
              if (node)
                {
                  TempBuffer buffer;

                  OP_STATUS status = node->GetStringValue (buffer);

                  XPath_Node::DecRef (&context, node);

                  RETURN_IF_ERROR (status);

                  result_number = XPath_Value::AsNumber (buffer.GetStorage () ? buffer.GetStorage () : UNI_L (""));
                }
              else
                result_number = op_nan (0);

              finished = TRUE;
            }
          else
            return is_finished;
        }
      else
        {
          XPath_Value *value;

          OP_BOOLEAN is_finished = XPath_EvaluateExpression (value, expression->primitive, &context, initial, XP_VALUE_NUMBER);

          if (is_finished == OpBoolean::IS_TRUE)
            {
              result_number = value->data.number;
              XPath_Value::DecRef (&context, value);

              finished = TRUE;
            }
          else
            {
              if (is_finished == OpStatus::ERR)
                failed = TRUE;

              return is_finished;
            }
        }
    }

  result = result_number;
  return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
XPathExpressionEvaluateImpl::GetBooleanResult (BOOL &result)
{
  BOOL initial;

  if (!started)
    {
      XPath_ContextStateSizes *state_sizes;

      if (expression->unordered_single)
        /* The expression's intrinsic type is a node-set, so the boolean value
           is true if the resulting node-set is non-empty, and false if it is
           empty. */
        state_sizes = &expression->nodeset_state_sizes;
      else
        state_sizes = &expression->primitive_state_sizes;

      RETURN_IF_ERROR (Start (state_sizes));
      initial = TRUE;
    }
  else
    initial = FALSE;

  XPath_Context context (&global_context, context_node->GetInternalNode (), context_position, context_size);
  XPath_Producer *use_producer = expression->unordered_single;

#ifdef XPATH_EXTENSION_SUPPORT
  if (expression->primitive && expression->primitive->HasFlag (XPath_Expression::FLAG_UNKNOWN))
    {
      XPath_ValueType type;

      OP_BOOLEAN finished = XPath_GetActualResultType (type, expression->primitive, &context, initial);

      if (finished != OpBoolean::IS_TRUE)
        {
          if (finished == OpStatus::ERR)
            failed = TRUE;

          return finished;
        }

      initial = FALSE;

      if (type != XP_VALUE_NODESET)
        use_producer = 0;
    }
#endif // XPATH_EXTENSION_SUPPORT

  if (use_producer)
    {
      if (need_reset)
        {
          use_producer->Reset (&context);
          need_reset = FALSE;
        }

      XPath_Node *node;

      OP_BOOLEAN finished = use_producer->GetNextNode (node, &context);

      if (finished == OpBoolean::IS_TRUE)
        {
          result = node != 0;
          XPath_Node::DecRef (&context, node);
        }
      else if (finished == OpStatus::ERR)
        failed = TRUE;

      return finished;
    }
  else
    {
      XPath_Value *value;

      OP_BOOLEAN finished = XPath_EvaluateExpression (value, expression->primitive, &context, initial, XP_VALUE_BOOLEAN);

      if (finished == OpBoolean::IS_TRUE)
        {
          result = value->data.boolean;
          XPath_Value::DecRef (&context, value);
        }
      else if (finished == OpStatus::ERR)
        failed = TRUE;

      return finished;
    }
}

/* virtual */ OP_BOOLEAN
XPathExpressionEvaluateImpl::GetStringResult (const uni_char *&result)
{
  if (!finished)
    {
      BOOL initial;

      if (!started)
        {
          XPath_ContextStateSizes *state_sizes;

          if (expression->ordered_single)
            state_sizes = &expression->nodeset_state_sizes;
          else
            state_sizes = &expression->primitive_state_sizes;

          RETURN_IF_ERROR (Start (state_sizes));
          initial = TRUE;
        }
      else
        initial = FALSE;

      XPath_Context context (&global_context, context_node->GetInternalNode (), context_position, context_size);
      XPath_Producer *use_producer = expression->ordered_single;

#ifdef XPATH_EXTENSION_SUPPORT
      if (expression->primitive && expression->primitive->HasFlag (XPath_Expression::FLAG_UNKNOWN))
        {
          XPath_ValueType type;

          OP_BOOLEAN finished = XPath_GetActualResultType (type, expression->primitive, &context, initial);

          if (finished != OpBoolean::IS_TRUE)
            {
              if (finished == OpStatus::ERR)
                failed = TRUE;

              return finished;
            }

          initial = FALSE;

          if (type != XP_VALUE_NODESET)
            use_producer = 0;
        }
#endif // XPATH_EXTENSION_SUPPORT

      if (use_producer)
        {
          if (need_reset)
            {
              use_producer->Reset (&context);
              need_reset = FALSE;
            }

          XPath_Node *node;

          OP_BOOLEAN is_finished = use_producer->GetNextNode (node, &context);

          if (is_finished == OpBoolean::IS_TRUE)
            {
              if (node)
                {
                  TempBuffer buffer;

                  OP_STATUS status = node->GetStringValue (buffer);

                  XPath_Node::DecRef (&context, node);

                  RETURN_IF_ERROR (status);

                  if (buffer.GetStorage ())
                    RETURN_IF_ERROR (result_string.Set (buffer.GetStorage ()));
                }

              finished = TRUE;
            }
          else
            {
              if (is_finished == OpStatus::ERR)
                failed = TRUE;

              return is_finished;
            }
        }
      else
        {
          XPath_Value *value;

          OP_BOOLEAN is_finished = XPath_EvaluateExpression (value, expression->primitive, &context, initial, XP_VALUE_STRING);

          if (is_finished == OpBoolean::IS_TRUE)
            {
              RETURN_IF_ERROR (result_string.Set (value->data.string));
              XPath_Value::DecRef (&context, value);

              finished = TRUE;
            }
          else
            {
              if (is_finished == OpStatus::ERR)
                failed = TRUE;

              return is_finished;
            }
        }
    }

  result = result_string.CStr ();
  if (!result)
    result = UNI_L ("");
  return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
XPathExpressionEvaluateImpl::GetSingleNode (XPathNode *&node)
{
  if (!producer)
    if ((when_nodeset & NODESET_FLAG_ORDERED) != 0)
      producer = expression->ordered_single;
    else
      producer = expression->unordered_single;

  if (!producer)
    {
      /* Incorrect use: CheckResultType() wouldn't have said "node-set". */
      OP_ASSERT (FALSE);
      return OpStatus::ERR;
    }

  if (!started)
    RETURN_IF_ERROR (Start (&expression->nodeset_state_sizes));

  if (!finished)
    {
      XPath_Context context (&global_context, context_node->GetInternalNode (), context_position, context_size);

      if (need_reset)
        {
          producer->Reset (&context);
          need_reset = FALSE;
        }

      XPath_Node *result;

      OP_BOOLEAN is_finished = producer->GetNextNode (result, &context);

      if (is_finished != OpBoolean::IS_TRUE)
        {
          if (is_finished == OpStatus::ERR)
            failed = TRUE;

          return is_finished;
        }

      finished = TRUE;

      if (!result)
        result_node = 0;
      else
        RETURN_IF_ERROR (XPathNodeImpl::Make (result_node, result, &global_context));
    }

  node = result_node;
  return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
XPathExpressionEvaluateImpl::GetNextNode (XPathNode *&node)
{
  if (!producer)
    if ((when_nodeset & NODESET_FLAG_ORDERED) != 0)
      producer = expression->ordered;
    else
      producer = expression->unordered;

  if (!producer)
    {
      /* Incorrect use: CheckResultType() wouldn't have said "node-set". */
      OP_ASSERT (FALSE);
      return OpStatus::ERR;
    }

  if (!started)
    RETURN_IF_ERROR (Start (&expression->nodeset_state_sizes));
  else if (result_node)
    {
      XPathNodeImpl::DecRef (static_cast<XPathNodeImpl *> (result_node));
      result_node = 0;
    }

  XPath_Context context (&global_context, context_node->GetInternalNode (), context_position, context_size);

  if (need_reset)
    {
      producer->Reset (&context);
      need_reset = FALSE;
    }

  XPath_Node *result;

  OP_BOOLEAN is_finished = producer->GetNextNode (result, &context);

  if (is_finished != OpBoolean::IS_TRUE)
    {
      if (is_finished == OpStatus::ERR)
        failed = TRUE;

      return is_finished;
    }

  if (!result)
    node = 0;
  else
    {
      RETURN_IF_ERROR (XPathNodeImpl::Make (result_node, result, &global_context));
      node = result_node;
    }

  return OpBoolean::IS_TRUE;
}

static void
XPath_CollectNodesL (XPath_Context *context, XPath_Producer *producer, XPath_NodeList *nodelist)
{
  while (TRUE)
    {
      XPath_Node *node = producer->GetNextNodeL (context);

      if (!node)
        return;
      else
        {
          nodelist->AddL (context, node);
          XPath_Node::DecRef (context, node);
        }
    }
}

static OP_BOOLEAN
XPath_CollectNodes (XPath_Context *context, XPath_Producer *producer, XPath_NodeList *nodelist)
{
  OP_BOOLEAN result;
  TRAP (result, XPath_CollectNodesL (context, producer, nodelist));
  return result == OpStatus::OK ? OpBoolean::IS_TRUE : result;
}

/* virtual */ OP_BOOLEAN
XPathExpressionEvaluateImpl::GetNodesCount (unsigned &count0)
{
  if (!producer)
    if ((when_nodeset & NODESET_FLAG_ORDERED) != 0)
      producer = expression->ordered;
    else
      producer = expression->unordered;

  if (!producer)
    {
      /* Incorrect use: CheckResultType() wouldn't have said "node-set". */
      OP_ASSERT (FALSE);
      return OpStatus::ERR;
    }

  if (!started)
    RETURN_IF_ERROR (Start (&expression->nodeset_state_sizes));

  if (!finished)
    {
      if (!result_nodelist)
        {
          result_nodelist = OP_NEW (XPath_NodeList, ());
          if (!result_nodelist)
            return OpStatus::ERR_NO_MEMORY;
        }

      XPath_Context context (&global_context, context_node->GetInternalNode (), context_position, context_size);

      if (need_reset)
        {
          producer->Reset (&context);
          need_reset = FALSE;
        }

      OP_BOOLEAN is_finished = XPath_CollectNodes (&context, producer, result_nodelist);

      if (is_finished == OpBoolean::IS_TRUE)
        {
          unsigned count = result_nodelist->GetCount ();

          if (count != 0)
            {
              result_nodes = OP_NEWA (XPathNode *, count);
              if (!result_nodes)
                return OpStatus::ERR_NO_MEMORY;

              for (unsigned index = 0; index < count; ++index, ++result_nodes_count)
                {
                  OP_STATUS status = XPathNodeImpl::Make (result_nodes[index], XPath_Node::IncRef (result_nodelist->Get (index)), &global_context);
                  if (OpStatus::IsError(status))
                    {
                      // Fill the remaining slots by NULL so that we don't try to delete junk pointers
                      while (index < count)
                        {
                          result_nodes[index] = NULL;
                          index++;
                        }
                      return status;
                    }
                }
            }
          else
            result_nodes = NULL;

          count0 = count;

          finished = TRUE;
        }
      else if (is_finished == OpStatus::ERR)
        failed = TRUE;

      return is_finished;
    }

  count0 = result_nodes_count;
  return OpBoolean::IS_TRUE;
}

/* virtual */ OP_STATUS
XPathExpressionEvaluateImpl::GetNode (XPathNode *&node, unsigned index)
{
  if (index < result_nodes_count)
    {
      node = result_nodes[index];
      return OpStatus::OK;
    }
  else
    return OpStatus::ERR;
}

#ifdef XPATH_ERRORS

/* virtual */ const uni_char *
XPathExpressionEvaluateImpl::GetErrorMessage ()
{
  return failed ? (error_message.IsEmpty () ? UNI_L ("unknown error") : error_message.CStr ()) : 0;
}

#endif // XPATH_ERRORS

#ifdef XPATH_PATTERN_SUPPORT

/* static */ void
XPathPatternContext::Free (XPathPatternContext *context)
{
  XPathPatternContextImpl *impl = static_cast<XPathPatternContextImpl *> (context);
  OP_DELETE (impl);
}

/* static */ OP_STATUS
XPathPatternContext::Make (XPathPatternContext *&context)
{
  context = OP_NEW (XPathPatternContextImpl, ());
  if (!context || !(static_cast<XPathPatternContextImpl *> (context)->context = OP_NEW (XPath_PatternContext, ())))
    {
      OP_DELETE (context);
      return OpStatus::ERR_NO_MEMORY;
    }
  return OpStatus::OK;
}

/* virtual */
XPathPatternContext::~XPathPatternContext ()
{
}

/* virtual */
XPathPatternContextImpl::~XPathPatternContextImpl ()
{
  OP_DELETE (context);
}

/* virtual */ void
XPathPatternContextImpl::InvalidateTree (XMLTreeAccessor *tree)
{
  context->InvalidateTree (tree);
}

XPathPattern::PatternData::PatternData ()
  : source (0),
    namespaces (0)
{
#ifdef XPATH_EXTENSION_SUPPORT
  extensions = 0;
#endif // XPATH_EXTENSION_SUPPORT
#ifdef XPATH_ERRORS
  error_message = 0;
#endif // XPATH_ERRORS
}

/* static */ OP_STATUS
XPathPattern::Make (XPathPattern *&pattern, const XPathPattern::PatternData &data)
{
  XPathPatternImpl *impl = OP_NEW (XPathPatternImpl, ());

  if (!impl)
    return OpStatus::ERR_NO_MEMORY;

  impl->namespaces = data.namespaces ? XPath_Namespaces::IncRef (static_cast<XPathNamespacesImpl *> (data.namespaces)->GetStorage ()) : 0;
#ifdef XPATH_EXTENSION_SUPPORT
  impl->extensions = data.extensions;
#endif // XPATH_EXTENSION_SUPPORT

  OP_STATUS status;

#ifdef XPATH_ERRORS
  if (OpStatus::IsMemoryError (status = impl->source.Set (data.source)) || OpStatus::IsError (status = impl->Compile (data.error_message)))
#else // XPATH_ERRORS
  if (OpStatus::IsMemoryError (status = impl->source.Set (data.source)) || OpStatus::IsError (status = impl->Compile ()))
#endif // XPATH_ERRORS
    {
      OP_DELETE (impl);
      return status;
    }

  pattern = impl;
  return OpStatus::OK;
}

/* static */ OP_STATUS
XPathPattern::Make (XPathPattern *&pattern, const uni_char *source, XPathNamespaces *namespaces, XPathExtensions *extensions)
{
  XPathPattern::PatternData data;
  data.source = source;
  data.namespaces = namespaces;
  data.extensions = extensions;
  return Make (pattern, data);
}

static BOOL
XPath_GetNextAlternativeL (const uni_char *&alternative, unsigned &alternative_length, const uni_char *&source)
{
  XPath_Lexer lexer (source);
  XPath_Token token;
  TempBuffer buffer; ANCHOR (TempBuffer, buffer);

  unsigned bracket_level = 0;
  BOOL found = FALSE;

  alternative = source;

  token = lexer.GetNextTokenL ();
  while (token != XP_TOKEN_END)
    {
      found = TRUE;

      if (bracket_level == 0 && token == XP_TOKEN_OPERATOR && token == "|")
        {
          alternative_length = lexer.GetOffset () - 1;
          source += lexer.GetOffset ();
          return TRUE;
        }
      else if (token == XP_TOKEN_PUNCTUATOR)
        if (token == "[" || token == "(")
          ++bracket_level;
        else if (token == "]" || token == ")")
          if (bracket_level == 0)
            /* No need to produce error message: user is assumed to use the
               regular pattern parsing functionality to produce a detailed error
               message. */
            LEAVE (OpStatus::ERR);
          else
            --bracket_level;

      token = lexer.ConsumeAndGetNextTokenL ();
    }

  if (found)
    {
      alternative_length = uni_strlen (alternative);
      source += alternative_length;
      return TRUE;
    }
  else
    return FALSE;
}

/* static */ OP_BOOLEAN
XPathPattern::GetNextAlternative(const uni_char *&alternative, unsigned &alternative_length, const uni_char *&source)
{
  OP_MEMORY_VAR BOOL found = FALSE;

  RETURN_IF_LEAVE (found = XPath_GetNextAlternativeL (alternative, alternative_length, source));

  return found ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

/* static */ void
XPathPattern::Free (XPathPattern *pattern)
{
  XPathPatternImpl *impl = static_cast<XPathPatternImpl *> (pattern);
  OP_DELETE (impl);
}

/* virtual */
XPathPattern::~XPathPattern ()
{
}

#ifdef XPATH_ERRORS

void
XPathPatternImpl::CompileL (OpString *error_message)
{
  XPath_Parser parser (namespaces, source.CStr ()); ANCHOR (XPath_Parser, parser);
  parser.SetExtensions (extensions);
  parser.SetErrorMessageStorage (error_message);
  pattern = parser.ParsePatternL (priority);
  parser.CopyStateSizes (sizes);
#ifdef XPATH_EXTENSION_SUPPORT
  readers = parser.TakeVariableReaders ();
#endif // XPATH_EXTENSION_SUPPORT
}

OP_STATUS
XPathPatternImpl::Compile (OpString *error_message)
{
  TRAPD (status, CompileL (error_message));
  return status;
}

#else // XPATH_ERRORS

void
XPathPatternImpl::CompileL ()
{
  XPath_Parser parser (namespaces, source.CStr ()); ANCHOR (XPath_Parser, parser);
  parser.SetExtensions (extensions);
  pattern = parser.ParsePatternL (priority);
  parser.CopyStateSizes (sizes);
#ifdef XPATH_EXTENSION_SUPPORT
  readers = parser.TakeVariableReaders ();
#endif // XPATH_EXTENSION_SUPPORT
}

OP_STATUS
XPathPatternImpl::Compile ()
{
  TRAPD (status, CompileL ());
  return status;
}

#endif // XPATH_ERRORS

XPathPatternImpl::XPathPatternImpl ()
  : namespaces (0),
    cached_match (0),
    pattern (0)
{
#ifdef XPATH_EXTENSION_SUPPORT
  readers = 0;
#endif // XPATH_EXTENSION_SUPPORT
}

/* virtual */
XPathPatternImpl::~XPathPatternImpl ()
{
  XPath_Namespaces::DecRef (namespaces);

  /* Need to be destroyed before 'readers'. */
  OP_DELETE (cached_match);

  OP_DELETE (pattern);
#ifdef XPATH_EXTENSION_SUPPORT
  OP_DELETE (readers);
#endif // XPATH_EXTENSION_SUPPORT
}

/* virtual */ float
XPathPatternImpl::GetPriority ()
{
  return priority;
}

#ifdef XPATH_NODE_PROFILE_SUPPORT

/* virtual */ XPathPattern::ProfileVerdict
XPathPatternImpl::GetProfileVerdict (XPathNodeProfile *profile)
{
#ifdef XPATH_DEBUG_GETPROFILEVERDICT
  XPathPattern::ProfileVerdict verdict = pattern->GetProfileVerdict (static_cast<XPathNodeProfileImpl *> (profile));

  OpString8 expression8, pattern8;
  expression8.SetUTF8FromUTF16 (static_cast<XPathNodeProfileImpl *> (profile)->source);
  pattern8.SetUTF8FromUTF16 (source);

  printf ("E(%s) vs P(%s): ", expression8.CStr (), pattern8.CStr ());

  switch (verdict)
    {
    case PATTERN_WILL_MATCH_PROFILED_NODES: printf ("will match\n"); break;
    case PATTERN_CAN_MATCH_PROFILED_NODES: printf ("can match\n"); break;
    case PATTERN_DOES_NOT_MATCH_PROFILED_NODES: printf ("does not match\n"); break;
    }
#endif // XPATH_DEBUG_GETPROFILEVERDICT

  return pattern->GetProfileVerdict (static_cast<XPathNodeProfileImpl *> (profile));
}

#endif // XPATH_NODE_PROFILE_SUPPORT

XPathMultiplePatternsHelper::XPathMultiplePatternsHelper ()
  : patterns (0),
    patterns_count (0),
    global_contexts (0)
{
  Reset (TRUE);
}

XPathMultiplePatternsHelper::~XPathMultiplePatternsHelper ()
{
  Reset ();

  OP_DELETEA (patterns);
  OP_DELETEA (patterns_internal);
  OP_DELETEA (global_contexts);
}

static OP_BOOLEAN
XPath_PreparePattern (XPath_Pattern *pattern, XPath_GlobalContext *global, XMLTreeAccessor *tree)
{
  pattern->SetGlobalContext (global);
  TRAPD (status, pattern->PrepareL (tree));
  return status == OpStatus::OK ? OpBoolean::IS_TRUE : status;
}

OP_BOOLEAN
XPathMultiplePatternsHelper::StartAndPrepare (XMLTreeAccessor *tree)
{
  if (!started)
    {
      for (unsigned index = 0; index < patterns_count; ++index)
        {
          RETURN_IF_ERROR (global_contexts[index].AllocateStates (patterns[index]->sizes));

#ifdef XPATH_EXTENSION_SUPPORT
          if (XPath_VariableReader *reader = patterns[index]->readers)
            {
              XPath_Context context (&global_contexts[index], 0, 0, 0);

              while (reader)
                {
                  reader->Start (&context);
                  reader = reader->GetNext ();
                }
            }
#endif // XPATH_EXTENSION_SUPPORT
        }

      started = TRUE;
    }

  if (!prepared)
    {
      while (pattern_index < patterns_count)
        {
#ifdef XPATH_ERRORS
          global_contexts[pattern_index].error_message = &error_message;
          global_contexts[pattern_index].source = patterns[pattern_index]->GetSource ();
#endif // XPATH_ERRORS

          OP_BOOLEAN completed = XPath_PreparePattern (patterns_internal[pattern_index], &global_contexts[pattern_index], tree);

          if (completed == OpBoolean::IS_TRUE)
            ++pattern_index;
          else
            {
              if (OpStatus::IsError (completed))
                {
                  failed = TRUE;

#ifdef XPATH_ERRORS
                  if (completed == OpStatus::ERR)
                    failed_pattern_source = patterns[pattern_index]->GetSource ();
#endif // XPATH_ERRORS
                }

              return completed;
            }
        }

      prepared = TRUE;
    }
  else
    for (unsigned index = 0; index < patterns_count; ++index)
      {
        patterns_internal[index]->SetGlobalContext (&global_contexts[index]);

#ifdef XPATH_ERRORS
        global_contexts[index].error_message = &error_message;
        global_contexts[index].source = patterns[index]->GetSource ();
#endif // XPATH_ERRORS
      }

  return OpBoolean::IS_TRUE;
}

OP_STATUS
XPathMultiplePatternsHelper::SetPatterns (XPathPattern **patterns0, unsigned patterns_count0, XPathPatternContext *pattern_context)
{
  patterns = OP_NEWA (XPathPatternImpl *, patterns_count0);
  patterns_internal = OP_NEWA (XPath_Pattern *, patterns_count0);
  global_contexts = OP_NEWA (XPath_GlobalContext, patterns_count0);

  if (!patterns || !patterns_internal || !global_contexts)
    return OpStatus::ERR_NO_MEMORY;

  for (unsigned index = 0; index < patterns_count0; ++index)
    {
      patterns[index] = static_cast<XPathPatternImpl *> (patterns0[index]);
      patterns_internal[index] = patterns[index]->pattern;
      global_contexts[index].pattern_context = static_cast<XPathPatternContextImpl *> (pattern_context)->GetContext ();
    }

  patterns_count = patterns_count0;
  return OpStatus::OK;
}

void
XPathMultiplePatternsHelper::Reset (BOOL from_constructor)
{
  if (!from_constructor)
    for (unsigned index = 0, count = patterns_count; index < count; ++index)
      {
#ifdef XPATH_EXTENSION_SUPPORT
          if (XPath_VariableReader *reader = patterns[index]->readers)
            {
              XPath_Context context (&global_contexts[index], 0, 0, 0);

              while (reader)
                {
                  reader->Finish (&context);
                  reader = reader->GetNext ();
                }
            }
#endif // XPATH_EXTENSION_SUPPORT

        global_contexts[index].Clean (failed);
      }

  pattern_index = 0;
  started = prepared = failed = FALSE;
}

#ifdef XPATH_EXTENSION_SUPPORT

void
XPathMultiplePatternsHelper::SetExtensionsContext (XPathExtensions::Context *context)
{
  for (unsigned index = 0, count = patterns_count; index < count; ++index)
    global_contexts[index].extensions_context = context;
}

#endif // XPATH_EXTENSION_SUPPORT

/* static */ OP_STATUS
XPathPattern::Search::Make (Search *&search, XPathPatternContext *context, XPathNode *node, XPathPattern **patterns, unsigned patterns_count)
{
  if (OpStatus::IsMemoryError (Make (search, context, patterns, patterns_count)))
    {
      XPathNode::Free (node);
      return OpStatus::ERR_NO_MEMORY;
    }
  return static_cast<XPathPatternSearchImpl *> (search)->Start (node);
}

/* static */ OP_STATUS
XPathPattern::Search::Make (Search *&search, XPathPatternContext *context, XPathPattern **patterns, unsigned patterns_count)
{
  XPath_Namespaces *namespaces = 0;
#ifdef XPATH_EXTENSION_SUPPORT
  XPathExtensions *extensions = 0;
#endif // XPATH_EXTENSION_SUPPORT
  TempBuffer expression_source;

  for (unsigned index = 0; index < patterns_count; ++index)
    {
      OP_ASSERT (!namespaces || namespaces == static_cast<XPathPatternImpl *> (patterns[index])->GetNamespaces ());
      namespaces = static_cast<XPathPatternImpl *> (patterns[index])->GetNamespaces ();

#ifdef XPATH_EXTENSION_SUPPORT
      OP_ASSERT (!extensions || extensions == static_cast<XPathPatternImpl *> (patterns[index])->GetExtensions ());
      extensions = static_cast<XPathPatternImpl *> (patterns[index])->GetExtensions ();
#endif // XPATH_EXTENSION_SUPPORT

      if (index != 0)
        RETURN_IF_ERROR (expression_source.Append (" | "));

      const OpString *pattern_source = static_cast<XPathPatternImpl *> (patterns[index])->GetSource ();

      XPath_Lexer lexer (pattern_source->CStr ());
      XPath_Token token (lexer.GetNextToken ());

      if (token != XP_TOKEN_FUNCTIONNAME && token != XP_TOKEN_OPERATOR)
        RETURN_IF_ERROR (expression_source.Append ("//"));

      RETURN_IF_ERROR (expression_source.Append (pattern_source->CStr ()));
    }

  XPathNamespacesImpl namespacesimpl (namespaces);
  XPathExpression *expression;
  XPathExpression::ExpressionData data;

  data.source = expression_source.GetStorage ();
  data.namespaces = &namespacesimpl;
#ifdef XPATH_EXTENSION_SUPPORT
  data.extensions = extensions;
#endif // XPATH_EXTENSION_SUPPORT

  RETURN_IF_ERROR (XPathExpression::Make (expression, data));

  search = OP_NEW (XPathPatternSearchImpl, (expression));

  if (!search)
    {
      XPathExpression::Free (expression);
      return OpStatus::ERR_NO_MEMORY;
    }

  return OpStatus::OK;
}

/* static */ void
XPathPattern::Search::Free (Search *search)
{
  XPathPatternSearchImpl *impl = static_cast<XPathPatternSearchImpl *> (search);
  OP_DELETE (impl);
}

/* virtual */
XPathPattern::Search::~Search ()
{
}

XPathPatternSearchImpl::XPathPatternSearchImpl (XPathExpression *expression)
  : expression (expression),
    evaluate (0)
{
}

/* virtual */
XPathPatternSearchImpl::~XPathPatternSearchImpl ()
{
  XPathExpression::Evaluate::Free (evaluate);
  XPathExpression::Free (expression);
}

/* virtual */ OP_STATUS
XPathPatternSearchImpl::Start (XPathNode *node)
{
  /* Someone forgot to call Stop ()...*/
  OP_ASSERT (!evaluate);

  RETURN_IF_ERROR (XPathExpression::Evaluate::Make (evaluate, expression));

  evaluate->SetContext (node);
  evaluate->SetRequestedType (XPathExpression::Evaluate::NODESET_ITERATOR | XPathExpression::Evaluate::NODESET_FLAG_ORDERED);

  return OpStatus::OK;
}

#ifdef XPATH_EXTENSION_SUPPORT

/* virtual */ void
XPathPatternSearchImpl::SetExtensionsContext (XPathExtensions::Context *context)
{
  evaluate->SetExtensionsContext (context);
}

#endif // XPATH_EXTENSION_SUPPORT

/* virtual */ void
XPathPatternSearchImpl::SetCostLimit (unsigned limit)
{
  evaluate->SetCostLimit (limit);
}

/* virtual */ unsigned
XPathPatternSearchImpl::GetLastOperationCost ()
{
  return evaluate->GetLastOperationCost ();
}

/* virtual */ OP_BOOLEAN
XPathPatternSearchImpl::GetNextNode (XPathNode *&node)
{
  return evaluate->GetNextNode (node);
}

/* virtual */ void
XPathPatternSearchImpl::Stop ()
{
  XPathExpression::Evaluate::Free (evaluate);
  evaluate = 0;
}

/* static */ OP_STATUS
XPathPattern::Count::Make (Count *&count, XPathPatternContext *context, Level level, XPathNode *node, XPathPattern **count_patterns, unsigned count_patterns_count, XPathPattern **from_patterns, unsigned from_patterns_count)
{
  XPathPattern **all_patterns = OP_NEWA (XPathPattern *, count_patterns_count + from_patterns_count);

  if (!all_patterns)
    {
      XPathNodeImpl::DecRef (node);
      return OpStatus::ERR_NO_MEMORY;
    }

  ANCHOR_ARRAY (XPathPattern *, all_patterns);

  op_memcpy (all_patterns, count_patterns, count_patterns_count * sizeof *count_patterns);
  op_memcpy (all_patterns + count_patterns_count, from_patterns, from_patterns_count * sizeof *from_patterns);

  XPathPatternCountImpl *impl = OP_NEW (XPathPatternCountImpl, ());
  XPathNode *copy;

  if (!impl || OpStatus::IsMemoryError (impl->SetPatterns (all_patterns, count_patterns_count + from_patterns_count, context)) || OpStatus::IsMemoryError (XPathNode::MakeCopy (copy, node)))
    {
      OP_DELETE (impl);
      XPathNodeImpl::DecRef (node);
      return OpStatus::ERR_NO_MEMORY;
    }

  impl->level = level;
  impl->node = static_cast<XPathNodeImpl *> (copy);
  impl->count_patterns_count = count_patterns_count;
  impl->from_patterns_count = from_patterns_count;

  XPathNodeImpl::DecRef (node);

  count = impl;
  return OpStatus::OK;
}

/* static */ void
XPathPattern::Count::Free (Count *count)
{
  XPathPatternCountImpl *impl = static_cast<XPathPatternCountImpl *> (count);
  OP_DELETE (impl);
}

/* virtual */
XPathPattern::Count::~Count ()
{
}

XPathPatternCountImpl::XPathPatternCountImpl ()
  : node (0),
    cost (0)
{
}

/* virtual */
XPathPatternCountImpl::~XPathPatternCountImpl ()
{
  XPathNodeImpl::DecRef (static_cast<XPathNodeImpl *> (node));
}

#ifdef XPATH_EXTENSION_SUPPORT

/* virtual */ void
XPathPatternCountImpl::SetExtensionsContext (XPathExtensions::Context *context)
{
  XPathMultiplePatternsHelper::SetExtensionsContext (context);
}

#endif // XPATH_EXTENSION_SUPPORT

/* virtual */ void
XPathPatternCountImpl::SetCostLimit (unsigned limit)
{
  /* If counting was interruptable, we could actually implement a limit. */
}

/* virtual */ unsigned
XPathPatternCountImpl::GetLastOperationCost ()
{
  return cost;
}

static void
XPath_ReverseNumbers (unsigned numbers_count, unsigned *numbers)
{
  for (unsigned index = 0, count = numbers_count >> 1; index < count; ++index)
    {
      unsigned number = numbers[index];
      numbers[index] = numbers[numbers_count - index - 1];
      numbers[numbers_count - index - 1] = number;
    }
}

/* virtual */ OP_BOOLEAN
XPathPatternCountImpl::GetResultLevelled (unsigned &numbers_count, unsigned *&numbers)
{
  OP_BOOLEAN result = StartAndPrepare (node->GetInternalNode ()->tree);

  if (result != OpBoolean::IS_TRUE)
    return result;

  unsigned failed_pattern_index;

  OP_STATUS status = XPath_Pattern::CountLevelled (numbers_count, numbers, level == LEVEL_SINGLE, patterns_internal + count_patterns_count, from_patterns_count, patterns_internal, count_patterns_count, node->GetInternalNode (), failed_pattern_index, cost);

  if (status == OpStatus::OK)
    {
      XPath_ReverseNumbers (numbers_count, numbers);
      return OpBoolean::IS_TRUE;
    }
  else
    {
      failed = TRUE;

#ifdef XPATH_ERRORS
      if (status == OpStatus::ERR)
        failed_pattern_source = patterns[failed_pattern_index]->GetSource ();
#endif // XPATH_ERRORS

      return status;
    }
}

/* virtual */ OP_BOOLEAN
XPathPatternCountImpl::GetResultAny (unsigned &number)
{
  OP_BOOLEAN result = StartAndPrepare (node->GetInternalNode ()->tree);

  if (result != OpBoolean::IS_TRUE)
    return result;

  unsigned failed_pattern_index;

  OP_STATUS status = XPath_Pattern::CountAny (number, patterns_internal + count_patterns_count, from_patterns_count, patterns_internal, count_patterns_count, node->GetInternalNode (), failed_pattern_index, cost);

  if (status == OpStatus::OK)
    return OpBoolean::IS_TRUE;
  else
    {
      failed = TRUE;

#ifdef XPATH_ERRORS
      if (status == OpStatus::ERR)
        failed_pattern_source = patterns[failed_pattern_index]->GetSource ();
#endif // XPATH_ERRORS

      return status;
    }
}

/* static */ OP_STATUS
XPathPattern::Match::Make (XPathPattern::Match *&match, XPathPatternContext *context, XPathNode *node, XPathPattern **patterns, unsigned patterns_count)
{
  XPathPatternMatchImpl *impl;

  if (patterns_count == 1)
    {
      XPathPatternImpl *pattern = static_cast<XPathPatternImpl *> (patterns[0]);

      impl = pattern->cached_match;
      if (impl)
        impl->global_contexts[0].pattern_context = static_cast<XPathPatternContextImpl *> (context)->GetContext ();
      pattern->cached_match = 0;
    }
  else
    impl = 0;

  if (!impl)
    {
      impl = OP_NEW (XPathPatternMatchImpl, ());

      if (!impl || OpStatus::IsMemoryError (impl->SetPatterns (patterns, patterns_count, context)))
        {
          OP_DELETE (impl);
          XPathNodeImpl::DecRef (node);
          return OpStatus::ERR_NO_MEMORY;
        }
    }

  XPathNode *copy;

  if (OpStatus::IsMemoryError (XPathNode::MakeCopy (copy, node)))
    {
      OP_DELETE (impl);
      XPathNodeImpl::DecRef (node);
      return OpStatus::ERR_NO_MEMORY;
    }

  impl->node = static_cast<XPathNodeImpl *> (copy);

  XPathNodeImpl::DecRef (node);

  match = impl;
  return OpStatus::OK;
}

/* static */ void
XPathPattern::Match::Free (XPathPattern::Match *match)
{
  if (match)
    {
      XPathPatternMatchImpl *impl = static_cast<XPathPatternMatchImpl *> (match);

      if (impl->patterns_count == 1 && !impl->patterns[0]->cached_match)
        {
          impl->Reset ();
          impl->patterns[0]->cached_match = impl;
        }
      else
        OP_DELETE (impl);
    }
}

/* virtual */
XPathPattern::Match::~Match ()
{
}

void
XPathPatternMatchImpl::Reset ()
{
  XPathNodeImpl::DecRef (node);
  node = 0;

  XPathMultiplePatternsHelper::Reset ();
}

XPathPatternMatchImpl::XPathPatternMatchImpl ()
  : node (0)
{
}

XPathPatternMatchImpl::~XPathPatternMatchImpl ()
{
  XPathNodeImpl::DecRef (node);
}

#ifdef XPATH_EXTENSION_SUPPORT

/* virtual */ void
XPathPatternMatchImpl::SetExtensionsContext (XPathExtensions::Context *context)
{
  XPathMultiplePatternsHelper::SetExtensionsContext (context);
}

#endif // XPATH_EXTENSION_SUPPORT

/* virtual */ void
XPathPatternMatchImpl::SetCostLimit (unsigned limit)
{
  /* FIXME: Implement this. */
}

/* virtual */ unsigned
XPathPatternMatchImpl::GetLastOperationCost ()
{
  /* FIXME: Implement this. */
  return 0;
}

/* virtual */ OP_BOOLEAN
XPathPatternMatchImpl::GetMatchedPattern (XPathPattern *&matched)
{
  OP_BOOLEAN result = StartAndPrepare (node->GetInternalNode ()->tree);

  if (result != OpBoolean::IS_TRUE)
    return result;

  unsigned pattern_index;

  OP_BOOLEAN any_matched = XPath_Pattern::Match (pattern_index, patterns_internal, patterns_count, node->GetInternalNode ());

  if (OpStatus::IsSuccess (any_matched))
    {
      if (any_matched == OpBoolean::IS_TRUE)
        matched = patterns[pattern_index];
      else
        matched = 0;

      return OpBoolean::IS_TRUE;
    }
  else
    {
      failed = TRUE;

#ifdef XPATH_ERRORS
      if (any_matched == OpStatus::ERR)
        failed_pattern_source = patterns[pattern_index]->GetSource ();
#endif // XPATH_ERRORS

      return any_matched;
    }
}

#endif // XPATH_PATTERN_SUPPORT

/* static */ BOOL
XPath::IsSupportedFunction (const XMLExpandedName &name)
{
  if (!name.GetUri ())
    {
      const uni_char *localpart = name.GetLocalPart ();

      if (uni_strchr (localpart, ',') == 0)
        {
          const uni_char* db_str = UNI_L (",string,number,boolean,position,last,id,count,sum,local-name,namespace-uri,name,true,false,not,lang,floor,ceiling,round,concat,starts-with,contains,substring-before,substring-after,substring,string-length,normalize-space,translate,");
          while (1)
            {
              db_str = uni_strstr (db_str, localpart);
              if (!db_str)
                break;

              if (db_str[-1] == ',' && db_str[uni_strlen (localpart)] == ',')
                return TRUE;
              db_str += uni_strlen (localpart);
            }
        }
    }

  return FALSE;
}

#endif // XPATH_SUPPORT
