/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_xmloutputhandler.h"

/* virtual */ void
XSLT_XMLOutputHandler::StartElementL (const XMLCompleteName &name)
{
  CallOutputTagL (XMLToken::TYPE_STag);
  LEAVE_IF_ERROR (nsnormalizer.StartElement (name));
  LEAVE_IF_ERROR (use_cdata_sections.Add (stylesheet && stylesheet->UseCDATASection (name)));
  in_starttag = TRUE;
  ++level;
}

/* virtual */ void
XSLT_XMLOutputHandler::SuggestNamespaceDeclarationL (XSLT_Element *element, XMLNamespaceDeclaration *nsdeclaration, BOOL skip_excluded_namespaces)
{
  if (nsdeclaration != suggested_ns)
    {
      suggested_ns = nsdeclaration;
      suggested_ns_level = level;

      XMLNamespaceDeclaration *existing = nsnormalizer.GetNamespaceDeclaration ();

      while (nsdeclaration)
        {
          const uni_char *prefix = nsdeclaration->GetPrefix ();

          if (prefix && !uni_str_eq (prefix, "xml") && !uni_str_eq (prefix, "xmlns") &&
              !XMLNamespaceDeclaration::FindDeclaration (existing, prefix) &&
              !(skip_excluded_namespaces && element->IsExcludedNamespace (nsdeclaration->GetUri ())) &&
              !element->IsExtensionNamespace (nsdeclaration->GetUri ()))
            if (OpStatus::IsMemoryError (nsnormalizer.AddAttribute (XMLCompleteName (UNI_L ("http://www.w3.org/2000/xmlns/"), UNI_L ("xmlns"), prefix), nsdeclaration->GetUri (), FALSE)))
              LEAVE (OpStatus::ERR_NO_MEMORY);

          nsdeclaration = nsdeclaration->GetPrevious ();
        }
    }
}

/* virtual */ void
XSLT_XMLOutputHandler::AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified)
{
  LEAVE_IF_ERROR (nsnormalizer.AddAttribute (name, value, TRUE));
}

/* virtual */ void
XSLT_XMLOutputHandler::AddTextL (const uni_char *data, BOOL disable_output_escaping)
{
  CallOutputTagL (XMLToken::TYPE_STag);
}

/* virtual */ void
XSLT_XMLOutputHandler::AddCommentL (const uni_char *data)
{
  CallOutputTagL (XMLToken::TYPE_STag);
}

/* virtual */ void
XSLT_XMLOutputHandler::AddProcessingInstructionL (const uni_char *target, const uni_char *data)
{
  CallOutputTagL (XMLToken::TYPE_STag);
}

/* virtual */ void
XSLT_XMLOutputHandler::EndElementL (const XMLCompleteName &name)
{
  CallOutputTagL (XMLToken::TYPE_ETag, &name);
  nsnormalizer.EndElement ();
  if (level == suggested_ns_level)
    {
      suggested_ns = 0;
      suggested_ns_level = 0;
    }
  --level;
}

/* virtual */ void
XSLT_XMLOutputHandler::EndOutputL ()
{
  CallOutputTagL (XMLToken::TYPE_Finished);
}

BOOL
XSLT_XMLOutputHandler::UseCDATASections ()
{
  if (unsigned count = use_cdata_sections.GetCount ())
    return use_cdata_sections.Get (count - 1) != 0;
  else
    return FALSE;
}

void
XSLT_XMLOutputHandler::CallOutputTagL (XMLToken::Type type, const XMLCompleteName *name)
{
  if (output_xmldecl)
    {
      if (output_specification && !output_specification->omit_xml_declaration)
        {
          XMLDocumentInformation document_info; ANCHOR (XMLDocumentInformation, document_info);
          XMLVersion version;
          if (output_specification->version && uni_str_eq (output_specification->version, "1.1"))
            version = XMLVERSION_1_1;
          else
            version = XMLVERSION_1_0;
          LEAVE_IF_ERROR (document_info.SetXMLDeclaration (version, output_specification->standalone, output_specification->encoding));

          OutputXMLDeclL (document_info);
        }
      output_xmldecl = FALSE;
    }

  if (in_starttag)
    {
      if (output_doctype)
        {
          if (output_specification && output_specification->doctype_system_id)
            {
              XMLDocumentInformation document_info; ANCHOR (XMLDocumentInformation, document_info);
              OpString qname; ANCHOR (OpString, qname);
              const XMLCompleteName element_name = nsnormalizer.GetElementName ();
              if (element_name.GetPrefix ())
                qname.AppendFormat (UNI_L ("%s:%s"), element_name.GetPrefix (), element_name.GetLocalPart ());
              else
                qname.SetL (element_name.GetLocalPart ());
              LEAVE_IF_ERROR (document_info.SetDoctypeDeclaration (qname.CStr (), output_specification->doctype_public_id, output_specification->doctype_system_id, 0));

              OutputDocumentTypeDeclL (document_info);
            }
          output_doctype = FALSE;
        }

      LEAVE_IF_ERROR (nsnormalizer.FinishAttributes ());
    }

  if (type == XMLToken::TYPE_ETag)
    OutputTagL (in_starttag ? XMLToken::TYPE_EmptyElemTag : XMLToken::TYPE_ETag, *name);
  else if (in_starttag)
    OutputTagL (XMLToken::TYPE_STag, nsnormalizer.GetElementName ());

  in_starttag = FALSE;
}

#endif // XSLT_SUPPORT
