/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_tree.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmlparser.h"

#define IMPORT(node) static_cast<XSLT_Tree::TreeNode *> (node)
#define EXPORT(node) static_cast<XSLT_Tree::TreeNode *> (node)

XSLT_Tree::XSLT_Tree ()
{
}

void
XSLT_Tree::TreeNode::Delete (Element *element)
{
  OP_DELETE (element);
}

void
XSLT_Tree::TreeNode::Delete (CharacterData *characterdata)
{
  OP_DELETE (characterdata);
}

void
XSLT_Tree::TreeNode::Delete (ProcessingInstruction *pi)
{
  OP_DELETE (pi);
}

XSLT_Tree::Container::~Container ()
{
  TreeNode *node = firstchild;
  while (node)
    {
      TreeNode *next = node->nextsibling;
      if (node->type == XMLTreeAccessor::TYPE_ELEMENT)
        Delete (static_cast<Element *> (node));
      else if (node->type == XMLTreeAccessor::TYPE_PROCESSING_INSTRUCTION)
        Delete (static_cast<ProcessingInstruction *> (node));
      else
        Delete (static_cast<CharacterData *> (node));
      node = next;
    }
}

XSLT_Tree::Element::~Element ()
{
  Attribute *attr = firstattr;
  while (attr)
    {
      Attribute *next = attr->nextattr;
      OP_DELETE (attr);
      attr = next;
    }
}

void
XSLT_Tree::AddNode (TreeNode *node)
{
  node->parent = current;
  if ((node->previoussibling = current->lastchild) != 0)
    current->lastchild = current->lastchild->nextsibling = node;
  else
    current->firstchild = current->lastchild = node;
  node->nextsibling = 0;
  if (node->type == XMLTreeAccessor::TYPE_ELEMENT)
    current = (Container *) node;
}

OP_STATUS
XSLT_Tree::StartEntity (const URL &url, const XMLDocumentInformation &documentinfo, BOOL entity_reference)
{
  if (!entity_reference)
    {
    }

  return OpStatus::OK;
}

OP_STATUS
XSLT_Tree::StartElement (const XMLCompleteNameN &name)
{
  Element *element = OP_NEW (Element, ());

  if (!element)
    return OpStatus::ERR_NO_MEMORY;

  if (OpStatus::IsMemoryError (element->name.Set (name)))
    {
      OP_DELETE (element);
      return OpStatus::ERR_NO_MEMORY;
    }

  element->type = XMLTreeAccessor::TYPE_ELEMENT;
  element->firstchild = element->lastchild = 0;

  AddNode (element);
  return OpStatus::OK;
}

OP_STATUS
XSLT_Tree::AddAttribute (const XMLCompleteNameN &name, const uni_char *value, unsigned value_length, BOOL id)
{
  Element::Attribute *attribute = OP_NEW (Element::Attribute, ());

  if (!attribute)
    return OpStatus::ERR_NO_MEMORY;

  if (OpStatus::IsMemoryError (attribute->name.Set (name)) || OpStatus::IsMemoryError (attribute->value.Set (value, value_length)))
    {
      OP_DELETE (attribute);
      return OpStatus::ERR_NO_MEMORY;
    }

  if (((Element *) current)->lastattr)
    ((Element *) current)->lastattr->nextattr = attribute;
  else
    ((Element *) current)->firstattr = attribute;
  ((Element *) current)->lastattr = attribute;

  if (id)
    {
      void *data = current;

      if (OpStatus::IsMemoryError (idtable.Add (attribute->value.CStr (), data)))
        return OpStatus::ERR_NO_MEMORY;
    }

  return OpStatus::OK;
}

OP_STATUS
XSLT_Tree::AddCharacterData (XMLTreeAccessor::NodeType nodetype, const uni_char *data, BOOL delete_data)
{
  if (!data)
    {
      OP_ASSERT (delete_data);
      return OpStatus::ERR_NO_MEMORY;
    }

  uni_char *to_delete = delete_data ? (uni_char *) data : 0;
  ANCHOR_ARRAY (uni_char, to_delete);

  if (current->lastchild && nodetype == XMLTreeAccessor::TYPE_TEXT && current->lastchild->type == XMLTreeAccessor::TYPE_TEXT)
    return static_cast<CharacterData *> (current->lastchild)->data.Append (data);

  CharacterData *characterdata = OP_NEW (CharacterData, ());

  if (!characterdata)
    return OpStatus::ERR_NO_MEMORY;

  if (OpStatus::IsMemoryError (characterdata->data.Set (data)))
    {
      OP_DELETE (characterdata);
      return OpStatus::ERR_NO_MEMORY;
    }

  characterdata->type = nodetype;

  AddNode (characterdata);
  return OpStatus::OK;
}

OP_STATUS
XSLT_Tree::AddProcessingInstruction (const uni_char *target, unsigned target_length, const uni_char *data, unsigned data_length)
{
  ProcessingInstruction *procinst = OP_NEW (ProcessingInstruction, ());

  if (!procinst)
    return OpStatus::ERR_NO_MEMORY;

  if (OpStatus::IsMemoryError (procinst->target.Set (target, target_length)) || OpStatus::IsMemoryError (procinst->data.Set (data, data_length)))
    {
      OP_DELETE (procinst);
      return OpStatus::ERR_NO_MEMORY;
    }

  procinst->type = XMLTreeAccessor::TYPE_PROCESSING_INSTRUCTION;

  AddNode (procinst);
  return OpStatus::OK;
}

void
XSLT_Tree::EndElement ()
{
  current = current->parent;
}

OP_STATUS
XSLT_Tree::HandleTokenInternal (XMLToken &token)
{
  switch (token.GetType ())
    {
    case XMLToken::TYPE_PI:
      return AddProcessingInstruction (token.GetName ().GetLocalPart (), token.GetName ().GetLocalPartLength (), token.GetData (), token.GetDataLength ());

    case XMLToken::TYPE_Comment:
      return AddCharacterData (XMLTreeAccessor::TYPE_COMMENT, token.GetLiteralAllocatedValue (), TRUE);

    case XMLToken::TYPE_CDATA:
#ifdef SELFTEST
      /* The selftests in selftest/handler.ot uses XSLT_Tree objects to
         represent trees, so in order for it to be able to distinguish between
         ordinary text and CDATA sections, we have this here.  In non-selftest
         builds, we never need to do that, so TYPE_CDATA just falls through to
         the next case.  That will then merge sequences of text and CDATA
         sections into a single character data node, which is somewhat more
         efficient. */
      return AddCharacterData (XMLTreeAccessor::TYPE_CDATA_SECTION, token.GetLiteralAllocatedValue (), TRUE);
#endif // SELFTEST
    case XMLToken::TYPE_Text:
      return AddCharacterData (XMLTreeAccessor::TYPE_TEXT, token.GetLiteralAllocatedValue (), TRUE);

    case XMLToken::TYPE_STag:
    case XMLToken::TYPE_ETag:
    case XMLToken::TYPE_EmptyElemTag:
      if (token.GetType () != XMLToken::TYPE_ETag)
        {
          RETURN_IF_ERROR (StartElement (token.GetName ()));
          XMLToken::Attribute *attributes = token.GetAttributes ();
          for (unsigned index = 0, count = token.GetAttributesCount (); index < count; ++index)
            {
              XMLToken::Attribute &attribute = attributes[index];
              RETURN_IF_ERROR (AddAttribute (attribute.GetName (), attribute.GetValue (), attribute.GetValueLength (), attribute.GetId ()));
            }
        }
      if (token.GetType () != XMLToken::TYPE_STag)
        EndElement ();
      return OpStatus::OK;

    case XMLToken::TYPE_Finished:
      documenturl = token.GetParser ()->GetURL ();
      RETURN_IF_ERROR(documentinformation.Copy(token.GetParser ()->GetDocumentInformation ()));
    }

  return OpStatus::OK;
}

#ifdef SELFTEST

/* static */ BOOL
XSLT_Tree::CompareNodes (TreeNode *node1, TreeNode *node2)
{
  if (node1->type != node2->type)
    return FALSE;
  else switch (node1->type)
    {
    case XMLTreeAccessor::TYPE_PROCESSING_INSTRUCTION:
      if (!(static_cast<ProcessingInstruction *> (node1)->target == static_cast<ProcessingInstruction *> (node2)->target))
        return FALSE;
      /* fall through */

    case XMLTreeAccessor::TYPE_TEXT:
    case XMLTreeAccessor::TYPE_CDATA_SECTION:
    case XMLTreeAccessor::TYPE_COMMENT:
      return static_cast<CharacterData *> (node1)->data == static_cast<CharacterData *> (node2)->data;

    case XMLTreeAccessor::TYPE_ELEMENT:
      if (static_cast<Element *> (node1)->name != static_cast<Element *> (node2)->name)
        return FALSE;
      else
        {
          /* Compare order of attributes as well.  This is used for testing
             only, and the output order of attributes should be stable.  If at
             some point in the future it isn't for some reason, fix this. */

          Element::Attribute *attr1 = static_cast<Element *> (node1)->firstattr;
          Element::Attribute *attr2 = static_cast<Element *> (node2)->firstattr;

          while (attr1 && attr2)
            {
              if (attr1->name != attr2->name)
                return FALSE;
              else if (!(attr1->value == attr2->value))
                return FALSE;

              attr1 = attr1->nextattr;
              attr2 = attr2->nextattr;
            }

          if (attr1 || attr2)
            return FALSE;
        }

    case XMLTreeAccessor::TYPE_ROOT:
      TreeNode *child1 = static_cast<Container *> (node1)->firstchild;
      TreeNode *child2 = static_cast<Container *> (node2)->firstchild;

      while (child1 && child2)
        {
          if (!CompareNodes (child1, child2))
            return FALSE;

          child1 = child1->nextsibling;
          child2 = child2->nextsibling;
        }

      if (child1 || child2)
        return FALSE;
    }

  return TRUE;
}

#endif // SELFTEST

/* static */ XSLT_Tree *
XSLT_Tree::MakeL ()
{
  XSLT_Tree *tree = OP_NEW_L (XSLT_Tree, ());

  LEAVE_IF_ERROR (tree->ConstructFallbackImplementation ());

  tree->tree_root.parent = 0;
  tree->tree_root.firstchild = tree->tree_root.lastchild = 0;
  tree->tree_root.type = XMLTreeAccessor::TYPE_ROOT;
  tree->current = &tree->tree_root;

  return tree;
}

#ifdef SELFTEST

/* static */ OP_STATUS
XSLT_Tree::Make (XSLT_Tree *&tree)
{
  TRAPD (status, tree = MakeL ());
  return status;
}

#endif // SELFTEST

/* virtual */
XSLT_Tree::~XSLT_Tree ()
{
}

/* virtual */ BOOL
XSLT_Tree::IsCaseSensitive ()
{
  return TRUE;
}

/* virtual */ XMLTreeAccessor::Node *
XSLT_Tree::GetRoot ()
{
  return EXPORT (&tree_root);
}

/* virtual */ XMLTreeAccessor::Node *
XSLT_Tree::GetParent(XMLTreeAccessor::Node *from)
{
  XMLTreeAccessor::Node *parent = EXPORT (IMPORT (from)->parent);
  return parent && FilterNode (parent) ? parent : 0;
}

/* virtual */ XMLTreeAccessor::Node *
XSLT_Tree::GetFirstChild(XMLTreeAccessor::Node *from)
{
  switch (IMPORT (from)->type)
    {
    default:
      return 0;

    case XMLTreeAccessor::TYPE_ROOT:
    case XMLTreeAccessor::TYPE_ELEMENT:
      TreeNode *child = static_cast<Container *> (IMPORT (from))->firstchild;
      while (child && !FilterNode (EXPORT (child)))
        child = child->nextsibling;
      return EXPORT (child);
    }
}

/* virtual */ XMLTreeAccessor::Node *
XSLT_Tree::GetLastChild(XMLTreeAccessor::Node *from)
{
  switch (IMPORT (from)->type)
    {
    default:
      return 0;

    case XMLTreeAccessor::TYPE_ROOT:
    case XMLTreeAccessor::TYPE_ELEMENT:
      TreeNode *child = static_cast<Container *> (IMPORT (from))->lastchild;
      while (child && !FilterNode (EXPORT (child)))
        child = child->previoussibling;
      return EXPORT (child);
    }
}

/* virtual */ XMLTreeAccessor::Node *
XSLT_Tree::GetNextSibling(XMLTreeAccessor::Node *from)
{
  TreeNode *sibling = IMPORT (from)->nextsibling;
  while (sibling && !FilterNode (EXPORT (sibling)))
    sibling = sibling->nextsibling;
  return EXPORT (sibling);
}

/* virtual */ XMLTreeAccessor::Node *
XSLT_Tree::GetPreviousSibling(XMLTreeAccessor::Node *from)
{
  TreeNode *sibling = IMPORT (from)->previoussibling;
  while (sibling && !FilterNode (EXPORT (sibling)))
    sibling = sibling->previoussibling;
  return EXPORT (sibling);
}

/* virtual */ XMLTreeAccessor::NodeType
XSLT_Tree::GetNodeType(XMLTreeAccessor::Node *node)
{
  return IMPORT (node)->type;
}

/* virtual */ void
XSLT_Tree::GetName(XMLCompleteName &name, XMLTreeAccessor::Node *node)
{
  OP_ASSERT(node);
  switch (IMPORT (node)->type)
    {
    case XMLTreeAccessor::TYPE_ELEMENT:
      name = static_cast<Element *> (IMPORT (node))->name;
      break;

    case XMLTreeAccessor::TYPE_PROCESSING_INSTRUCTION:
      name = XMLCompleteName (static_cast<ProcessingInstruction *> (IMPORT (node))->target.CStr ());
    }
}

/* virtual */ void
XSLT_Tree::GetAttributes (XMLTreeAccessor::Attributes *&out_attributes, XMLTreeAccessor::Node *node, BOOL ignore_default, BOOL ignore_nsdeclarations)
{
  if (IMPORT (node)->type == XMLTreeAccessor::TYPE_ELEMENT)
    attributes.Reset (ignore_default, ignore_nsdeclarations, static_cast<XSLT_Tree::Element *> (IMPORT (node)));
  else
    attributes.Reset (FALSE, FALSE, 0);

  out_attributes = &attributes;
}

/* virtual*/ BOOL
XSLT_Tree::IsEmptyText (XMLTreeAccessor::Node *node)
{
  if (IMPORT (node)->type == XMLTreeAccessor::TYPE_TEXT)
    return static_cast<CharacterData *> (IMPORT (node))->data.IsEmpty ();
  else
    return FALSE;
}

/* virtual*/ BOOL
XSLT_Tree::IsWhitespaceOnly (XMLTreeAccessor::Node *node)
{
  if (IMPORT (node)->type == XMLTreeAccessor::TYPE_TEXT)
    return XMLUtils::IsWhitespace (static_cast<CharacterData *> (IMPORT (node))->data.CStr ());
  else
    return FALSE;
}

/* virtual*/ const uni_char *
XSLT_Tree::GetPITarget (XMLTreeAccessor::Node *node)
{
  return static_cast<ProcessingInstruction *> (IMPORT (node))->target.CStr ();
}

/* virtual */ OP_STATUS
XSLT_Tree::GetData (const uni_char *&data, XMLTreeAccessor::Node *node, TempBuffer *buffer)
{
  switch (IMPORT (node)->type)
    {
    case XMLTreeAccessor::TYPE_TEXT:
    case XMLTreeAccessor::TYPE_CDATA_SECTION:
    case XMLTreeAccessor::TYPE_COMMENT:
    case XMLTreeAccessor::TYPE_PROCESSING_INSTRUCTION:
      data = static_cast<CharacterData *> (IMPORT (node))->data.CStr ();
    }

  return OpStatus::OK;
}

/* virtual */ OP_STATUS
XSLT_Tree::GetElementById (XMLTreeAccessor::Node *&node, const uni_char *id)
{
  void *data;

  if (OpStatus::IsSuccess (idtable.GetData (id, &data)))
    node = (XMLTreeAccessor::Node *) data;
  else
    node = 0;

  return OpStatus::OK;
}

/* virtual */ URL
XSLT_Tree::GetDocumentURL ()
{
  return documenturl;
}

/* virtual */ const XMLDocumentInformation *
XSLT_Tree::GetDocumentInformation()
{
  return &documentinformation;
}

/* virtual */ void
XSLT_Tree::StartElementL (const XMLCompleteName &name)
{
  LEAVE_IF_ERROR (StartElement (name));
}

/* virtual */ void
XSLT_Tree::AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified)
{
  if (!current->firstchild && current->type == XMLTreeAccessor::TYPE_ELEMENT)
    LEAVE_IF_ERROR (AddAttribute (name, value, uni_strlen (value), id));
}

/* virtual */ void
XSLT_Tree::AddTextL (const uni_char *data, BOOL disable_output_escaping)
{
  /* Signal warning if disable_output_escaping==TRUE? */
  LEAVE_IF_ERROR (AddCharacterData (XMLTreeAccessor::TYPE_TEXT, data, FALSE));
}

/* virtual */ void
XSLT_Tree::AddCommentL (const uni_char *data)
{
  LEAVE_IF_ERROR (AddCharacterData (XMLTreeAccessor::TYPE_COMMENT, data, FALSE));
}

/* virtual */ void
XSLT_Tree::AddProcessingInstructionL (const uni_char *target, const uni_char *data)
{
  LEAVE_IF_ERROR (AddProcessingInstruction (target, uni_strlen (target), data, uni_strlen (data)));
}

/* virtual */ void
XSLT_Tree::EndElementL (const XMLCompleteName &name)
{
  current = current->parent;
  OP_ASSERT (current);
}

/* virtual */ void
XSLT_Tree::EndOutputL ()
{
  OP_ASSERT (current->type == XMLTreeAccessor::TYPE_ROOT);
}

/* virtual */ XMLTokenHandler::Result
XSLT_Tree::HandleToken (XMLToken &token)
{
  if (OpStatus::IsMemoryError (HandleTokenInternal (token)))
    return XMLTokenHandler::RESULT_OOM;
  else
    return XMLTokenHandler::RESULT_OK;
}

/* virtual */ unsigned
XSLT_Tree::Attributes::GetCount ()
{
  unsigned count = 0;
  Element::Attribute *attr = element ? element->firstattr : 0;
  while (attr)
    {
      if (!ignore_nsdeclarations || !attr->name.IsXMLNamespaces ())
        count++;
      attr = attr->nextattr;
    }
  return count;
}

/* virtual */ OP_STATUS
XSLT_Tree::Attributes::GetAttribute (unsigned index, XMLCompleteName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer)
{
  OP_ASSERT (index < GetCount ());
  Element::Attribute *attr = element->firstattr;
  while (index)
    {
      if (!ignore_nsdeclarations || !attr->name.IsXMLNamespaces ())
        --index;
      attr = attr->nextattr;
      OP_ASSERT (attr);
    }

  name = attr->name;
  value = attr->value.CStr();
  id = FALSE;
  specified = TRUE;
  return OpStatus::OK;
}

#ifdef SELFTEST

static BOOL
XSLT_ComparePossiblyNullStrings (const uni_char *string1, const uni_char *string2)
{
  return !string1 && !string2 || string1 && string2 && uni_strcmp (string1, string2) == 0;
}

/* static */ BOOL
XSLT_Tree::Compare (XSLT_Tree *tree1, XSLT_Tree *tree2)
{
  /* Compare XML declaration. */
  if (tree1->documentinformation.GetXMLDeclarationPresent () != tree2->documentinformation.GetXMLDeclarationPresent ())
    return FALSE;
  if (tree1->documentinformation.GetVersion () != tree2->documentinformation.GetVersion ())
    return FALSE;
  if (tree1->documentinformation.GetStandalone () != tree2->documentinformation.GetStandalone ())
    return FALSE;
  if (!XSLT_ComparePossiblyNullStrings (tree1->documentinformation.GetEncoding (), tree2->documentinformation.GetEncoding ()))
    return FALSE;

  /* Compare document type declaration. */
  if (tree1->documentinformation.GetDoctypeDeclarationPresent () != tree2->documentinformation.GetDoctypeDeclarationPresent ())
    return FALSE;
  if (!XSLT_ComparePossiblyNullStrings (tree1->documentinformation.GetDoctypeName (), tree2->documentinformation.GetDoctypeName ()))
    return FALSE;
  if (!XSLT_ComparePossiblyNullStrings (tree1->documentinformation.GetPublicId (), tree2->documentinformation.GetPublicId ()))
    return FALSE;
  if (!XSLT_ComparePossiblyNullStrings (tree1->documentinformation.GetSystemId (), tree2->documentinformation.GetSystemId ()))
    return FALSE;
  if (!XSLT_ComparePossiblyNullStrings (tree1->documentinformation.GetInternalSubset (), tree2->documentinformation.GetInternalSubset ()))
    return FALSE;

  /* Compare tree itself. */
  return CompareNodes (&tree1->tree_root, &tree2->tree_root);
}

#endif // SELFTEST
#endif // XSLT_SUPPORT
