/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/xmlparser/src/xmlcheckingtokenhandler.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmldocumentinfo.h"

#include "modules/util/str.h"

#if defined XML_ERRORS && defined OPERA_CONSOLE

static OP_STATUS
XMLReportTaintedNamespaceDeclarationWarning (XMLParser *parser, const uni_char *elemqname0, unsigned elemqname_length, const XMLRange &position, const uni_char *prefix, const uni_char *uri)
{
  OpString message;
  OpString elemqname;
  OpString attrqname;

  if (OpStatus::IsMemoryError (message.Set ("Warning: default XML namespace declaration attribute present in external DTD\nsubset but not specified in document.  To ensure compatibility with non-\nvalidating XML parsers, XML namespace declarations should be declared in the\ninternal DTD subset or be specified in the element start tag.\n")) ||
      OpStatus::IsMemoryError (elemqname.Append (elemqname0, elemqname_length)) ||
      OpStatus::IsMemoryError (prefix ? attrqname.AppendFormat (UNI_L ("xmlns:%s"), prefix) : attrqname.Set ("xmlns")) ||
      OpStatus::IsMemoryError (message.AppendFormat (UNI_L ("\n  Element name: %s\n  Attribute: %s=\"%s\""), elemqname.CStr (), attrqname.CStr (), uri)) ||
      OpStatus::IsMemoryError (parser->ReportConsoleMessage (OpConsoleEngine::Information, position, message.CStr ())))
    return OpStatus::ERR_NO_MEMORY;

  return OpStatus::OK;
}

#endif // XML_ERRORS && OPERA_CONSOLE

XMLCheckingTokenHandler::XMLCheckingTokenHandler (XMLInternalParser *parser, XMLTokenHandler *secondary, BOOL is_fragment, XMLNamespaceDeclaration *nsdeclaration)
#if defined XML_ERRORS && defined OPERA_CONSOLE
  : has_tainted_nsdecls (FALSE),
    current (0),
#else // XML_ERRORS && OPERA_CONSOLE
  : current (0),
#endif // XML_ERRORS && OPERA_CONSOLE
    old (0),
    has_xmldecl (FALSE),
    has_doctype (FALSE),
    has_misc (FALSE),
    has_whitespace (FALSE),
    has_root (FALSE),
    is_fragment (is_fragment),
    parser (parser),
    doctype (parser->GetDoctype ()),
    secondary (secondary),
    nsdeclaration (nsdeclaration),
    level (nsdeclaration ? nsdeclaration->GetLevel () : 0)
{
}

XMLCheckingTokenHandler::~XMLCheckingTokenHandler ()
{
  while (current)
    {
      Element *temporary = current->parent;
      OP_DELETE(current);
      current = temporary;
    }

  while (old)
    {
      Element *temporary = old->parent;
      OP_DELETE(old);
      old = temporary;
    }
}

/* virtual */ XMLTokenHandler::Result
XMLCheckingTokenHandler::HandleToken (XMLToken &token, BOOL from_processtoken)
{
  XMLTokenHandler::Result result = XMLTokenHandler::RESULT_OK;

  switch (token.GetType ())
    {
    case XMLToken::TYPE_PI:
      if (token.GetName ().GetLocalPartLength () == 3 && op_memcmp (token.GetName ().GetLocalPart (), UNI_L ("xml"), 3 * sizeof (uni_char)) == 0)
        if (!has_xmldecl && !has_doctype && !has_misc && !has_whitespace && !has_root)
          {
            has_xmldecl = TRUE;
            token.SetType (XMLToken::TYPE_XMLDecl);
          }
        else
          return SetError (XMLInternalParser::WELL_FORMEDNESS_ERROR_OutOfPlace_XMLDecl);
      else
        {
          if (token.GetName ().GetLocalPartLength () == 14 && op_memcmp (token.GetName ().GetLocalPart (), UNI_L ("xml-stylesheet"), 14 * sizeof (uni_char)) == 0)
            {
              XMLToken::Attribute *attributes = token.GetAttributes ();
              for (unsigned attrindex = 1, attrcount = token.GetAttributesCount (); attrindex < attrcount; ++attrindex)
                for (unsigned attrindex2 = 0; attrindex2 < attrindex; ++attrindex2)
                  if (attributes[attrindex].GetName () == attributes[attrindex2].GetName ())
                    return SetAttributeError (XMLInternalParser::WELL_FORMEDNESS_ERROR_UniqueAttSpec_XMLStylesheet, attrindex, attrindex2);
            }

          has_misc = TRUE;
        }
      break;

    case XMLToken::TYPE_Comment:
      has_misc = TRUE;
      break;

    case XMLToken::TYPE_CDATA:
      if (!current && !is_fragment)
        return SetError (XMLInternalParser::PARSE_ERROR_Unexpected_CDATA);
      break;

    case XMLToken::TYPE_DOCTYPE:
      if (has_doctype || has_root)
        return SetError (XMLInternalParser::WELL_FORMEDNESS_ERROR_OutOfPlace_DOCTYPE);
      else
        has_doctype = TRUE;
      break;

    case XMLToken::TYPE_STag:
    case XMLToken::TYPE_ETag:
    case XMLToken::TYPE_EmptyElemTag:
      if (token.GetType () != XMLToken::TYPE_ETag)
        {
          result = HandleSTagToken (token, from_processtoken);

          if (result != XMLTokenHandler::RESULT_OK && result != XMLTokenHandler::RESULT_BLOCK)
            return result;
        }
      if (token.GetType () != XMLToken::TYPE_STag)
        {
          XMLTokenHandler::Result local_result = HandleETagToken (token, from_processtoken);

          if (local_result != XMLTokenHandler::RESULT_OK)
            return local_result;
          else
            return result;
        }
      else
        return XMLTokenHandler::RESULT_OK;

    case XMLToken::TYPE_Text:
      if (!current && !is_fragment)
        if (parser->GetLiteralIsWhitespace ())
          {
            has_whitespace = TRUE;
            return XMLTokenHandler::RESULT_OK;
          }
        else
          return SetError (XMLInternalParser::PARSE_ERROR_Unexpected_Text);
      break;

    case XMLToken::TYPE_Finished:
      if (current)
        return SetError (XMLInternalParser::PARSE_ERROR_Unexpected_EOF);
      else if (!has_root && !is_fragment)
        return SetError (XMLInternalParser::WELL_FORMEDNESS_ERROR_Missing_Document_Element);
    }

  if (result == XMLTokenHandler::RESULT_OK)
    return CallSecondary (token);
  else
    return result;
}

XMLTokenHandler::Result
XMLCheckingTokenHandler::CallSecondary (XMLToken &token)
{
  return secondary->HandleToken (token);
}

static void
XMLCheckingTokenHandler_GetQName (const XMLCompleteNameN &name, const uni_char *&qname, unsigned &qname_length, uni_char **allocated_qname)
{
  const uni_char *localpart = name.GetLocalPart ();
  unsigned localpart_length = name.GetLocalPartLength ();

  if ((qname = name.GetPrefix ()) != 0)
    {
      unsigned prefix_length = name.GetPrefixLength ();

      qname_length = prefix_length + 1 + localpart_length;

#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN
      if (allocated_qname)
        {
          if ((*allocated_qname = OP_NEWA(uni_char, qname_length + 1)) != 0)
            {
              op_memcpy (*allocated_qname, qname, prefix_length * sizeof (uni_char));
              (*allocated_qname)[prefix_length] = ':';
              op_memcpy (*allocated_qname + prefix_length + 1, localpart, localpart_length * sizeof (uni_char));
              (*allocated_qname)[prefix_length + 1 + localpart_length] = 0;
            }
          qname = *allocated_qname;
        }
      else
#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN
        {
          /* This depends on the fact that the prefix and the localpart
             are always from the same string, with a colon between. */

          OP_ASSERT (localpart == qname + prefix_length + 1);
          OP_ASSERT (qname[prefix_length] == ':');
        }
    }
  else
    {
      qname = localpart;
      qname_length = localpart_length;

#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN
      if (allocated_qname)
        *allocated_qname = NULL;
#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN
    }
}

XMLTokenHandler::Result
XMLCheckingTokenHandler::HandleSTagToken (XMLToken &token, BOOL from_processtoken)
{
  if (!current && has_root && !is_fragment)
    return SetError (XMLInternalParser::PARSE_ERROR_Unexpected_STag);

  const uni_char *qname;
  unsigned qname_length;
  uni_char *allocated_qname = 0;

  XMLCheckingTokenHandler_GetQName (token.GetName (), qname, qname_length, from_processtoken ? &allocated_qname : 0);

#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN
  if (from_processtoken && !qname)
    return XMLTokenHandler::RESULT_OOM;
  ANCHOR_ARRAY (uni_char, allocated_qname);
#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN

  Element *element = NewElement ();
  if (!element)
    return XMLTokenHandler::RESULT_OOM;

  element->parent = current;

  if (OpStatus::IsMemoryError (element->SetQName (qname, qname_length)))
    {
      OP_DELETE(element);
      return XMLTokenHandler::RESULT_OOM;
    }

  element->containing_entity = parser->GetCurrentEntity ();

#ifdef XML_ERRORS
  if (token.GetType () == XMLToken::TYPE_STag)
    parser->GetTokenRange (element->range);
#endif // XML_ERRORS

  ++level;

  current = element;

#ifdef XML_ATTRIBUTE_DEFAULT_VALUES
  if (XMLDoctype::Element *doctype_element = doctype->GetElement (qname, qname_length))
    {
      XMLDoctype::Attribute **declared_attributes = doctype_element->GetAttributes ();

      for (unsigned attrindex = 0, attrcount = doctype_element->GetAttributesCount (); attrindex < attrcount; ++attrindex)
        {
          XMLDoctype::Attribute *declared_attribute = declared_attributes[attrindex];
          XMLDoctype::Attribute::Flag flag = declared_attribute->GetFlag ();

          if (flag != XMLDoctype::Attribute::FLAG_REQUIRED && flag != XMLDoctype::Attribute::FLAG_IMPLIED)
            {
              XMLToken::Attribute *specified_attribute = token.GetAttribute (declared_attribute->GetAttributeName ());

              if (!specified_attribute)
                {
                  XMLToken::Attribute *attr;

                  if (OpStatus::IsMemoryError (token.AddAttribute (attr)))
                    return XMLTokenHandler::RESULT_OOM;

                  XMLCompleteNameN attrname (declared_attribute->GetAttributeName (), uni_strlen (declared_attribute->GetAttributeName ()));

                  attr->SetName (attrname);
                  attr->SetValue (declared_attribute->GetDefaultValue (), uni_strlen (declared_attribute->GetDefaultValue ()), TRUE, FALSE);

#if defined XML_ERRORS && defined OPERA_CONSOLE
                  if (declared_attribute->GetDeclaredInExternalSubset () && attrname.GetPrefixLength () == 5 && uni_strncmp (attrname.GetPrefix (), "xmlns", 4) == 0 || !attrname.GetPrefix () && attrname.GetLocalPartLength () == 5 && uni_strncmp (attrname.GetLocalPart (), "xmlns", 5))
                    {
                      const uni_char *prefix;
                      unsigned prefix_length;

                      if (attrname.GetPrefix ())
                        prefix = attrname.GetLocalPart (), prefix_length = attrname.GetLocalPartLength ();
                      else
                        prefix = 0, prefix_length = 0;

                      const uni_char *uri = XMLNamespaceDeclaration::FindUri(nsdeclaration, prefix, prefix_length);

                      if (!uri || uni_strcmp (uri, declared_attribute->GetDefaultValue ()) != 0)
                        if (OpStatus::IsMemoryError (AddNamespaceDeclaration (TRUE, prefix, prefix_length, declared_attribute->GetDefaultValue (), uni_strlen (declared_attribute->GetDefaultValue()))))
                          return XMLTokenHandler::RESULT_OOM;
                    }
#endif // XML_ERRORS && OPERA_CONSOLE
                }
            }
        }
    }

#ifdef SELFTEST
  if (parser->GetXMLParser ())
#endif // SELFTEST
    {
      const XMLDocumentInformation &documentinfo = parser->GetXMLParser ()->GetDocumentInformation ();

      for (unsigned index = 0, count = documentinfo.GetKnownDefaultAttributeCount (qname, qname_length); index < count; ++index)
        {
          const uni_char *attrqname, *attrvalue;
          BOOL fixed;

          documentinfo.GetKnownDefaultAttribute (index, attrqname, attrvalue, fixed);

          if (!token.GetAttribute (attrqname))
            {
              XMLToken::Attribute *attr;

              if (OpStatus::IsMemoryError (token.AddAttribute (attr)))
                return XMLTokenHandler::RESULT_OOM;

              attr->SetName (attrqname, uni_strlen (attrqname));
              attr->SetValue (attrvalue, uni_strlen (attrvalue), TRUE, FALSE);

#if defined XML_ERRORS && defined OPERA_CONSOLE
              XMLCompleteNameN attrname (attrqname, uni_strlen (attrqname));

              if (attrname.GetPrefixLength () == 5 && uni_strncmp (attrname.GetPrefix (), "xmlns", 4) == 0 || !attrname.GetPrefix () && attrname.GetLocalPartLength () == 5 && uni_strncmp (attrname.GetLocalPart (), "xmlns", 5) == 0)
                {
                  const uni_char *prefix;
                  unsigned prefix_length;

                  if (attrname.GetPrefix ())
                    prefix = attrname.GetLocalPart (), prefix_length = attrname.GetLocalPartLength ();
                  else
                    prefix = 0, prefix_length = 0;

                  const uni_char *uri = XMLNamespaceDeclaration::FindUri(nsdeclaration, prefix, prefix_length);

                  if (!uri || uni_strcmp (uri, attrvalue) != 0)
                    if (OpStatus::IsMemoryError (AddNamespaceDeclaration (TRUE, prefix, prefix_length, attrvalue, uni_strlen (attrvalue))))
                      return XMLTokenHandler::RESULT_OOM;
                }
#endif // XML_ERRORS && OPERA_CONSOLE
            }
        }
    }
#endif // XML_ATTRIBUTE_DEFAULT_VALUES

  XMLToken::Attribute *attributes = token.GetAttributes ();
  unsigned attrindex, attrcount = token.GetAttributesCount ();

  for (attrindex = 0; attrindex < attrcount; ++attrindex)
    {
#if defined XML_ERRORS && defined OPERA_CONSOLE
      XMLNamespaceDeclaration *nsdeclaration_before = nsdeclaration;
#endif // XML_ERRORS && OPERA_CONSOLE

      XMLToken::Attribute &attr = attributes[attrindex];
      XMLCompleteNameN attrname = attr.GetName ();

      OP_STATUS status = XMLNamespaceDeclaration::ProcessAttribute (nsdeclaration, attrname, attr.GetValue (), attr.GetValueLength (), level);

      if (OpStatus::IsMemoryError (status))
        return XMLTokenHandler::RESULT_OOM;
      else if (OpStatus::IsError (status))
        return SetAttributeError (XMLInternalParser::WELL_FORMEDNESS_ERROR_NS_InvalidNSDeclaration, attrindex);

#if defined XML_ERRORS && defined OPERA_CONSOLE
      if (nsdeclaration != nsdeclaration_before)
        {
          Element *ownerelement;
          Element::NamespaceDeclaration *nsdecl;

          const uni_char *prefix = nsdeclaration->GetPrefix ();
          unsigned prefix_length = prefix ? uni_strlen (prefix) : 0;

          if (FindTaintedNamespaceDeclaration (prefix, prefix_length, ownerelement, nsdecl) && element != ownerelement)
            if (OpStatus::IsMemoryError (AddNamespaceDeclaration (FALSE, prefix, prefix_length, attr.GetValue (), attr.GetValueLength ())))
              return XMLTokenHandler::RESULT_OOM;
        }
#endif // XML_ERRORS && OPERA_CONSOLE
    }

  XMLCompleteNameN &elemname = token.GetNameForUpdate ();

  if (!XMLNamespaceDeclaration::ResolveName (nsdeclaration, elemname, TRUE))
    return SetError (XMLInternalParser::WELL_FORMEDNESS_ERROR_NS_UndeclaredPrefix);

  if (elemname.GetPrefixLength () == 5 && uni_strncmp (elemname.GetPrefix (), "xmlns", 5) == 0)
    return SetError (XMLInternalParser::WELL_FORMEDNESS_ERROR_NS_ReservedPrefix);

  element->uri = elemname.GetUri ();
  element->uri_length = elemname.GetUriLength ();

#if defined XML_ERRORS && defined OPERA_CONSOLE
  Element *ownerelement;
  Element::NamespaceDeclaration *nsdecl;

  if (FindTaintedNamespaceDeclaration (elemname.GetPrefix (), elemname.GetPrefixLength (), ownerelement, nsdecl))
    if (!nsdecl->reported)
      {
#ifdef SELFTEST
        if (parser->GetXMLParser ())
#endif // SELFTEST
          {
            if (!ownerelement->range.start.IsValid ())
              token.GetTokenRange (ownerelement->range);
            if (OpStatus::IsMemoryError (XMLReportTaintedNamespaceDeclarationWarning (parser->GetXMLParser (), ownerelement->qname, ownerelement->qname_length, ownerelement->range, nsdecl->prefix, nsdecl->uri)))
              return XMLTokenHandler::RESULT_OOM;
          }
        nsdecl->reported = TRUE;
      }
#endif // XML_ERRORS && OPERA_CONSOLE

  for (attrindex = 0; attrindex < attrcount; ++attrindex)
    {
      XMLToken::Attribute &attr = attributes[attrindex];

      XMLCompleteNameN &attrname = attr.GetNameForUpdate ();

      if (!XMLNamespaceDeclaration::ResolveName (nsdeclaration, attrname, FALSE))
        return SetAttributeError (XMLInternalParser::WELL_FORMEDNESS_ERROR_NS_UndeclaredPrefix, attrindex);

#if defined XML_ERRORS && defined OPERA_CONSOLE
      if (attrname.GetPrefix () && FindTaintedNamespaceDeclaration (attrname.GetPrefix (), attrname.GetPrefixLength (), ownerelement, nsdecl))
        if (!nsdecl->reported)
          {
#ifdef SELFTEST
            if (parser->GetXMLParser ())
#endif // SELFTEST
              {
                if (!ownerelement->range.start.IsValid ())
                  token.GetTokenRange (ownerelement->range);
                if (OpStatus::IsMemoryError (XMLReportTaintedNamespaceDeclarationWarning (parser->GetXMLParser (), ownerelement->qname, ownerelement->qname_length, ownerelement->range, nsdecl->prefix, nsdecl->uri)))
                  return XMLTokenHandler::RESULT_OOM;
              }
            nsdecl->reported = TRUE;
          }
#endif // XML_ERRORS && OPERA_CONSOLE

      if (!attr.GetId () && attrname.IsId (elemname))
        attr.SetId ();

      for (unsigned attrindex2 = 0; attrindex2 < attrindex; ++attrindex2)
        if (static_cast<const XMLExpandedNameN &> (attrname) == attributes[attrindex2].GetName ())
          return SetAttributeError (XMLInternalParser::WELL_FORMEDNESS_ERROR_UniqueAttSpec, attrindex, attrindex2);
    }

  has_root = TRUE;

  return CallSecondary (token);
}

XMLTokenHandler::Result
XMLCheckingTokenHandler::HandleETagToken (XMLToken &token, BOOL from_processtoken)
{
  if (!current)
    return SetError (XMLInternalParser::PARSE_ERROR_Unexpected_ETag);

  const uni_char *qname;
  unsigned qname_length;
  uni_char *allocated_qname = 0;

  XMLCheckingTokenHandler_GetQName (token.GetName (), qname, qname_length, from_processtoken ? &allocated_qname : 0);
#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN
  if (from_processtoken && !qname)
    return XMLTokenHandler::RESULT_OOM;
  ANCHOR_ARRAY (uni_char, allocated_qname);
#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN

  if (current->qname_length != qname_length || op_memcmp (current->qname, qname, qname_length * sizeof qname[0]) != 0)
    return SetError (XMLInternalParser::PARSE_ERROR_Mismatched_ETag);
  else if (current->containing_entity != parser->GetCurrentEntity ())
    return SetError (XMLInternalParser::WELL_FORMEDNESS_ERROR_GE_matches_content);

  XMLCompleteNameN &elemname = token.GetNameForUpdate ();

#ifdef _DEBUG
  BOOL valid = XMLNamespaceDeclaration::ResolveName (nsdeclaration, elemname, TRUE);

  /* The name (including the prefix, if any) in this token
     matched the name in the start tag token (tested just
     above,) and that name was tested to be namespace well-
     formed.  If this isn't, something strange is going on. */

  OP_ASSERT (valid);
#else // _DEBUG
  elemname.SetUri (current->uri, current->uri_length);
#endif // _DEBUG

  XMLTokenHandler::Result result;

  if (token.GetType () == XMLToken::TYPE_ETag)
    result = CallSecondary (token);
  else
    result = XMLTokenHandler::RESULT_OK;

  Element *new_current = current->parent;
  FreeElement (current);
  current = new_current;

  XMLNamespaceDeclaration::Pop (nsdeclaration, level--);

  return result;
}

XMLTokenHandler::Result
XMLCheckingTokenHandler::SetError (XMLInternalParser::ParseError error)
{
#ifdef XML_ERRORS
  XMLRange range, *rangep1, *rangep2;
  if (parser->GetTokenRange (range))
    rangep1 = &range;
  else
    rangep1 = 0;
  if (error == XMLInternalParser::PARSE_ERROR_Mismatched_ETag)
    rangep2 = &current->range;
  else
    rangep2 = 0;
  parser->SetLastError (error, rangep1, rangep2);
#else // XML_ERRORS
  parser->SetLastError (error, 0);
#endif // XML_ERRORS

  return XMLTokenHandler::RESULT_ERROR;
}

XMLTokenHandler::Result
XMLCheckingTokenHandler::SetAttributeError (XMLInternalParser::ParseError error, unsigned attrindex, unsigned attrindex_related)
{
#ifdef XML_ERRORS
  XMLRange range1, *rangep1, range2, *rangep2;
  if (parser->GetAttributeRange (range1, attrindex))
    rangep1 = &range1;
  else
    rangep1 = 0;
  if (attrindex_related != ~0u)
    if (parser->GetAttributeRange (range2, attrindex_related))
      rangep2 = &range2;
    else
      rangep2 = 0;
  else
    rangep2 = 0;
  parser->SetLastError (error, rangep1, rangep2);
#else // XML_ERRORS
  parser->SetLastError (error, 0);
#endif // XML_ERRORS

  return XMLTokenHandler::RESULT_ERROR;
}

XMLCheckingTokenHandler::Element::~Element ()
{
  if (owns_qname)
    OP_DELETEA(qname);

#if defined XML_ERRORS && defined OPERA_CONSOLE
  OP_DELETE(firstnsdecl);
#endif // XML_ERRORS && OPERA_CONSOLE
}

OP_STATUS
XMLCheckingTokenHandler::Element::SetQName (const uni_char *new_qname, unsigned new_qname_length)
{
  if (!parent || parent->qname_free <= new_qname_length)
    {
      if (qname && qname_free <= new_qname_length)
        {
          OP_DELETEA (qname);
          qname = 0;
        }

      if (!qname)
        {
          unsigned length = new_qname_length < 128 ? 256 : new_qname_length + 1;
          qname = OP_NEWA(uni_char, length);
          if (!qname)
            return OpStatus::ERR_NO_MEMORY;
          qname_free = length;
          owns_qname = TRUE;
        }
    }
  else
    {
      if (qname)
        OP_DELETEA (qname);

      qname = parent->qname + parent->qname_length;
      qname_free = parent->qname_free;
      owns_qname = FALSE;
    }

  qname_free -= new_qname_length;

  op_memcpy (qname, new_qname, new_qname_length * sizeof qname[0]);
  qname_length = new_qname_length;

  return OpStatus::OK;
}

#if defined XML_ERRORS && defined OPERA_CONSOLE

XMLCheckingTokenHandler::Element::NamespaceDeclaration::~NamespaceDeclaration ()
{
  OP_DELETEA(prefix);
  OP_DELETEA(uri);
  OP_DELETE(nextnsdecl);
}

XMLTokenHandler::Result
XMLCheckingTokenHandler::AddNamespaceDeclaration (BOOL tainted, const uni_char *prefix, unsigned prefix_length, const uni_char *uri, unsigned uri_length)
{
  Element::NamespaceDeclaration *nsdecl = OP_NEW(Element::NamespaceDeclaration, ());
  if (!nsdecl)
    return XMLTokenHandler::RESULT_OOM;

#if defined XML_ERRORS && defined OPERA_CONSOLE
  if (tainted)
    has_tainted_nsdecls = TRUE;
#endif // XML_ERRORS && OPERA_CONSOLE

  nsdecl->tainted = tainted;
  nsdecl->reported = FALSE;
  nsdecl->prefix = prefix ? UniSetNewStrN (prefix, prefix_length) : 0;
  nsdecl->uri = UniSetNewStrN (uri, uri_length);
  nsdecl->nextnsdecl = 0;

  if (prefix && !nsdecl->prefix || !nsdecl->uri)
    {
      OP_DELETE(nsdecl);
      return XMLTokenHandler::RESULT_OOM;
    }

  nsdecl->nextnsdecl = current->firstnsdecl;
  current->firstnsdecl= nsdecl;

  return XMLTokenHandler::RESULT_OK;
}

BOOL
XMLCheckingTokenHandler::FindTaintedNamespaceDeclaration (const uni_char *prefix, unsigned prefix_length, Element *&ownerelement, Element::NamespaceDeclaration *&nsdecl)
{
  if (has_tainted_nsdecls)
    {
      ownerelement = current;
      while (ownerelement)
        {
          nsdecl = ownerelement->firstnsdecl;
          while (nsdecl)
            if (prefix == nsdecl->prefix || prefix && nsdecl->prefix && prefix_length == uni_strlen (nsdecl->prefix) && uni_strncmp (prefix, nsdecl->prefix, prefix_length) == 0)
              return nsdecl->tainted;
            else
              nsdecl = nsdecl->nextnsdecl;
          ownerelement = ownerelement->parent;
        }
    }
  return FALSE;
}

#endif // XML_ERRORS && OPERA_CONSOLE

XMLCheckingTokenHandler::Element *
XMLCheckingTokenHandler::NewElement ()
{
  if (old)
    {
      Element *element = old;
      old = element->parent;
      return element;
    }
  else
    return OP_NEW(Element, ());
}

void
XMLCheckingTokenHandler::FreeElement (Element *element)
{
#if defined XML_ERRORS && defined OPERA_CONSOLE
  OP_DELETE(element->firstnsdecl);
  element->firstnsdecl = 0;
#endif // XML_ERRORS && OPERA_CONSOLE

  if (element->owns_qname)
    element->qname_free += element->qname_length;
  else
    element->qname = 0;

  element->parent = old;
  old = element;
}
