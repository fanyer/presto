/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XML_ERRORS

#include "modules/xmlparser/xmlcommon.h"
#include "modules/xmlparser/xmlinternalparser.h"

/* static */ const char *
XMLInternalParser::GetErrorStringInternal (XMLInternalParser::ParseError error)
{
  switch (error)
    {
    case PARSE_ERROR_Invalid_Syntax:
      return "invalid syntax\0";
    case PARSE_ERROR_Invalid_Char:
      return "invalid character\0NT-Char";
    case PARSE_ERROR_Invalid_PITarget:
      return "invalid processing instruction\0NT-PI";
    case PARSE_ERROR_Invalid_ExternalId:
      return "invalid external ID\0NT-ExternalID";
    case PARSE_ERROR_Invalid_XMLDecl:
    case PARSE_ERROR_Invalid_XMLDecl_version:
    case PARSE_ERROR_Invalid_XMLDecl_encoding:
    case PARSE_ERROR_Invalid_XMLDecl_standalone:
      return "invalid XML declaration\0NT-XMLDecl";
    case PARSE_ERROR_Invalid_SystemLiteral:
      return "invalid system literal\0NT-SystemLiteral";
    case PARSE_ERROR_Invalid_PubidLiteral:
      return "invalid public ID literal\0NT-PubidLiteral";
    case PARSE_ERROR_Invalid_TextDecl:
    case PARSE_ERROR_Invalid_TextDecl_version:
    case PARSE_ERROR_Invalid_TextDecl_encoding:
      return "invalid text declaration\0NT-TextDecl";
    case PARSE_ERROR_Invalid_DOCTYPE:
      return "invalid document type declaration\0NT-doctypedecl";
    case PARSE_ERROR_Invalid_DOCTYPE_name:
      return "invalid name in document type declaration\0NT-doctypedecl";
    case PARSE_ERROR_Invalid_DOCTYPE_conditional:
      return "invalid conditional section\0NT-conditionalSection";
    case PARSE_ERROR_Invalid_ELEMENT:
      return "invalid element type declaration\0NT-elementdecl";
    case PARSE_ERROR_Invalid_ELEMENT_name:
      return "invalid name in element type declaration\0NT-elementdecl";
    case PARSE_ERROR_Invalid_ELEMENT_contentspec:
      return "invalid content specification in element type declaration\0NT-contentspec";
    case PARSE_ERROR_Invalid_ELEMENT_contentspec_Mixed:
      return "invalid content specification in element type declaration\0NT-Mixed";
    case PARSE_ERROR_Invalid_ELEMENT_contentspec_cp:
      return "invalid content specification in element type declaration\0NT-cp";
    case PARSE_ERROR_Invalid_ELEMENT_contentspec_seq:
      return "invalid content specification in element type declaration\0NT-seq";
    case PARSE_ERROR_Invalid_ATTLIST:
      return "invalid attribute-list declaration\0NT-AttlistDecl";
    case PARSE_ERROR_Invalid_ATTLIST_elementname:
      return "invalid element name in attribute-list declaration\0NT-AttlistDecl";
    case PARSE_ERROR_Invalid_ATTLIST_attributename:
      return "invalid attribute name in attribute-list declaration\0NT-AttDef";
    case PARSE_ERROR_Invalid_ATTLIST_NotationType:
      return "invalid notation type specification in attribute-list declaration\0NT-NotationType";
    case PARSE_ERROR_Invalid_ATTLIST_Enumeration:
      return "invalid enumeration specification in attribute-list declaration\0NT-Enumeration";
    case PARSE_ERROR_Invalid_ATTLIST_DefaultDecl:
      return "invalid default value specification in attribute-list declaration\0NT-DefaultDecl";
    case PARSE_ERROR_Invalid_ENTITY:
      return "invalid entity declaration\0NT-EntityDecl";
    case PARSE_ERROR_Invalid_ENTITY_name:
      return "invalid name in entity declaration\0NT-EntityDecl";
    case PARSE_ERROR_Invalid_NOTATION:
      return "invalid notation declaration\0NT-NotationDecl";
    case PARSE_ERROR_Invalid_NOTATION_name:
      return "invalid name in notation declaration\0NT-NotationDecl";
    case PARSE_ERROR_Invalid_STag:
      return "invalid start-tag\0NT-STag";
    case PARSE_ERROR_Invalid_STag_Attribute:
      return "invalid attribute specification\0NT-Attribute";
    case PARSE_ERROR_Invalid_STag_AttValue:
      return "invalid attribute value\0NT-AttValue";
    case PARSE_ERROR_Invalid_ETag:
      return "invalid end-tag\0NT-ETag";
    case PARSE_ERROR_Invalid_Reference_char:
      return "invalid character reference\0NT-CharRef";
    case PARSE_ERROR_Invalid_Reference_GE:
      return "invalid entity reference\0NT-EntityRef";
    case PARSE_ERROR_Invalid_Reference_PE:
      return "invalid entity reference\0NT-PEReference";
    case PARSE_ERROR_Invalid_XML_StyleSheet:
      return "invalid xml-stylesheet processing instruction\0";

    case PARSE_ERROR_Unexpected_EOF:
      return "unexpected end-of-file\0";
    case PARSE_ERROR_Unexpected_STag:
      return "unexpected start-tag (root element already specified)\0";
    case PARSE_ERROR_Unexpected_ETag:
      return "unexpected end-tag (no current element)\0";
    case PARSE_ERROR_Unexpected_Text:
      return "unexpected text (non-whitespace text outside root element)\0";
    case PARSE_ERROR_Unexpected_CDATA:
      return "unexpected character-data section (character data outside root element)\0";
    case PARSE_ERROR_Unexpected_Reference:
      return "unexpected character or entity reference (reference outside root element)\0";

    case PARSE_ERROR_Duplicate_DOCTYPE:
      return "duplicated document type declaration\0";
    case PARSE_ERROR_Mismatched_ETag:
      return "mismatched end-tag\0";

    case PARSE_ERROR_InvalidCharacterEncoding:
      return "illegal byte sequence in encoding\0charencoding";
    case PARSE_ERROR_TooManyNestedEntityReferences:
      return "too many nested entity references\0";

    case WELL_FORMEDNESS_ERROR_PE_in_Internal_Subset:
      return "well-formedness constraint: parsed entities in internal subset\0wfc-PEinInternalSubset";
    case WELL_FORMEDNESS_ERROR_GE_in_Doctype:
      return "invalid general entity reference in document type declaration\0";
    case WELL_FORMEDNESS_ERROR_PE_between_Decls:
      return "well-formedness constraint: parsed entity between declarations\0PE-between-Decls";
    case WELL_FORMEDNESS_ERROR_GE_matches_content:
      return "general parsed entity is not well-formed\0wf-entities";
    case WELL_FORMEDNESS_ERROR_Undeclared_Entity:
      return "well-formedness constraint: entity declared\0wf-entdeclared";
    case WELL_FORMEDNESS_ERROR_Recursive_Entity:
      return "well-formedness constraint: no recursion\0norecursion";
    case WELL_FORMEDNESS_ERROR_External_GE:
      return "external gernal parsed entity is not well-formed\0NT-extParsedEnt";
    case WELL_FORMEDNESS_ERROR_External_GE_in_AttValue:
      return "well-formedness constraint: no external entity references (in attribute values)\0NoExternalRefs";
    case WELL_FORMEDNESS_ERROR_Unparsed_GE_in_content:
      return "well-formedness constraint: parsed entity\0textent";
    case WELL_FORMEDNESS_ERROR_UniqueAttSpec:
      return "well-formedness constraint: unique attribute definition\0uniqattspec";
    case WELL_FORMEDNESS_ERROR_UniqueAttSpec_XMLStylesheet:
      return "invalid xml-stylesheet processing instruction: duplicate PseudoAtt name\0NT-PseudoAtt";
    case WELL_FORMEDNESS_ERROR_Invalid_AttValue:
      return "well-formedness constraint: no < in attribute values\0CleanAttrVals";
    case WELL_FORMEDNESS_ERROR_Invalid_AttValue_CharRef:
      return "invalid character reference in attribute value\0";
    case WELL_FORMEDNESS_ERROR_Invalid_AttValue_Reference:
      return "invalid entity reference in attribute value\0";
    case WELL_FORMEDNESS_ERROR_Missing_Document_Element:
      return "missing root element\0";
    case WELL_FORMEDNESS_ERROR_OutOfPlace_XMLDecl:
      return "XML declaration not at beginning of document\0";
    case WELL_FORMEDNESS_ERROR_OutOfPlace_XMLStylesheet:
      return "invalid xml-stylesheet processing instruction: outside document prolog\0";
    case WELL_FORMEDNESS_ERROR_OutOfPlace_DOCTYPE:
      return "document type declaration after content\0";
    case WELL_FORMEDNESS_ERROR_Invalid_CDATA_End:
      return "invalid character-data section end marker ']]>' in text\0dt-chardata";
    case WELL_FORMEDNESS_ERROR_Invalid_Comment:
      return "invalid comment (containing '--' that is not followed by '>')\0";
    case WELL_FORMEDNESS_ERROR_NS_InvalidNSDeclaration:
      return "invalid XML namespace declaration\0xmlReserved";
    case WELL_FORMEDNESS_ERROR_NS_UndeclaredPrefix:
      return "undeclared XML namespace prefix used in attribute name\0nsc-NSDeclared";
    case WELL_FORMEDNESS_ERROR_NS_InvalidNCName:
      return "invalid unqualified name\0NT-NCName";
    case WELL_FORMEDNESS_ERROR_NS_InvalidQName:
      return "invalid qualified name\0NT-QName";
    case WELL_FORMEDNESS_ERROR_NS_ReservedPrefix:
      return "incorrect use of reserved namespace prefix\0xmlReserved";
    case WELL_FORMEDNESS_ERROR_ExternalEntityHasLaterVersion:
      return "external entity has text declaration specifying later XML version than document entity's XML declaration\0sec-version-info";

    default:
      /* Should be more verbose, but since validation isn't active anyway... */
      return "validity error\0";
    }
}

void
XMLInternalParser::GetErrorDescription(const char *&error, const char *&uri, const char *&fragment_id)
{
  error = GetErrorStringInternal (last_error);
  fragment_id = error + op_strlen (error) + 1;

  if (!*fragment_id)
    fragment_id = 0;

  switch (last_error)
    {
    case WELL_FORMEDNESS_ERROR_NS_InvalidNSDeclaration:
    case WELL_FORMEDNESS_ERROR_NS_UndeclaredPrefix:
    case WELL_FORMEDNESS_ERROR_NS_InvalidNCName:
    case WELL_FORMEDNESS_ERROR_NS_InvalidQName:
    case WELL_FORMEDNESS_ERROR_NS_ReservedPrefix:
      /* Namespaces in XML 1.1 because the 1.0 version simply isn't as good. */
      uri = "http://www.w3.org/TR/xml-names11/";
      break;

    case WELL_FORMEDNESS_ERROR_ExternalEntityHasLaterVersion:
      /* XML 1.1 since XML 1.0 doesn't mention document entities and external
         entities with different versions.  This is not a 1.1 specific error
         though; XML 1.0 doesn't allow a VersionInfo other than "1.0", so the
         same rule still applies. */
      uri = "http://www.w3.org/TR/xml11/";
      break;

    case WELL_FORMEDNESS_ERROR_UniqueAttSpec_XMLStylesheet:
    case WELL_FORMEDNESS_ERROR_OutOfPlace_XMLStylesheet:
      uri = "http://www.w3.org/TR/xml-stylesheet/";
      break;

    default:
      /* These apply both to XML 1.0 and XML 1.1, but the fragment identifiers
         are for XML 1.0, and it is probably to be considered the most relevant
         specification anyhow. */
      uri = "http://www.w3.org/TR/REC-xml/";
    }
}

#endif // XML_ERRORS
