/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_outputhandler.h"

#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltokenhandler.h"

/* virtual */
XSLT_OutputHandler::~XSLT_OutputHandler()
{
}

/* virtual */ void
XSLT_OutputHandler::SuggestNamespaceDeclarationL (XSLT_Element *element, XMLNamespaceDeclaration *nsdeclaration, BOOL skip_excluded_namespaces)
{
}

void
XSLT_OutputHandler::CopyOfL (XSLT_Element *copy_of, XMLTreeAccessor *tree, XMLTreeAccessor::Node *node)
{
  TempBuffer buffer; ANCHOR (TempBuffer, buffer);
  XMLCompleteName name; ANCHOR (XMLCompleteName, name);

  XMLTreeAccessor::Node *stop = tree->GetNextNonDescendant (node);
  XMLTreeAccessor::Node *root = node;

  XMLTreeAccessor::Attributes *attributes;
  const uni_char *data;

  while (node != stop)
    {
      switch (tree->GetNodeType (node))
        {
        case XMLTreeAccessor::TYPE_ELEMENT:
          {
            tree->GetName (name, node);
            StartElementL (name);

            XMLTreeAccessor::Namespaces *namespaces;
            LEAVE_IF_ERROR (tree->GetNamespaces (namespaces, node));

            XMLNamespaceDeclaration::Reference nsdeclaration; ANCHOR (XMLNamespaceDeclaration::Reference, nsdeclaration);
            for (unsigned nsindex = 0, nscount = namespaces->GetCount (); nsindex < nscount; ++nsindex)
              {
                const uni_char *uri, *prefix;
                namespaces->GetNamespace (nsindex, uri, prefix);
                if (prefix && uni_strcmp (prefix, "xml") != 0)
                  XMLNamespaceDeclaration::PushL (nsdeclaration, uri, ~0u, prefix, ~0u, 1);
              }
            SuggestNamespaceDeclarationL (copy_of, nsdeclaration, FALSE);

            tree->GetAttributes (attributes, node, FALSE, TRUE);
            for (unsigned index = 0, count = attributes->GetCount (); index < count; ++index)
              {
                const uni_char *value;
                BOOL id, specified;

                LEAVE_IF_ERROR (attributes->GetAttribute (index, name, value, id, specified, &buffer));

                AddAttributeL (name, value, id, specified);

                buffer.Clear ();
              }
          }
          break;

        case XMLTreeAccessor::TYPE_TEXT:
        case XMLTreeAccessor::TYPE_CDATA_SECTION:
          LEAVE_IF_ERROR (tree->GetData (data, node, &buffer));
          AddTextL (data, FALSE);
          buffer.Clear ();
          break;

        case XMLTreeAccessor::TYPE_COMMENT:
          LEAVE_IF_ERROR (tree->GetData (data, node, &buffer));
          AddCommentL (data);
          buffer.Clear ();
          break;

        case XMLTreeAccessor::TYPE_PROCESSING_INSTRUCTION:
          LEAVE_IF_ERROR (tree->GetData (data, node, &buffer));
          AddProcessingInstructionL (tree->GetPITarget (node), data);
          buffer.Clear ();
          break;

        default:
          break;
        }


      XMLTreeAccessor::Node *old_node = node;

      node = tree->GetNext (node);

      // Close elements needing closing
      // <one><two></two></one><three/>
      if (node != stop)
        {
          OP_ASSERT(node);
          OP_ASSERT(tree->IsAncestorOf(root, node));
          while(!tree->IsAncestorOf(old_node, node))
            {
              if (tree->GetNodeType(old_node) == XMLTreeAccessor::TYPE_ELEMENT)
                {
                  tree->GetName (name, old_node);
                  EndElementL (name);
                }
              old_node = tree->GetParent(old_node);
            }
        }
      else
        {
          // Close everything
          for (;;old_node = tree->GetParent(old_node))
            {
              if (tree->GetNodeType(old_node) == XMLTreeAccessor::TYPE_ELEMENT)
                {
                  tree->GetName (name, old_node);
                  EndElementL (name);
                }
              if (old_node == root)
                break;
            }
        }
    }
}

void
XSLT_OutputHandler::CopyOfL (XSLT_Element *copy_of, XMLTreeAccessor *tree, XMLTreeAccessor::Node *node, const XMLCompleteName &attribute_name)
{
  const uni_char *value;
  BOOL id, specified;
  TempBuffer buffer; ANCHOR (TempBuffer, buffer);

  LEAVE_IF_ERROR (tree->GetAttribute(node, attribute_name, value, id, specified, &buffer));

  AddAttributeL (attribute_name, value, id, specified);
}

#endif // XSLT_SUPPORT
