/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/xmlparser/xmlcommon.h"
#include "modules/xmlparser/xmlinternalparser.h"
#include "modules/xmlparser/xmldoctype.h"
#include "modules/xmlparser/xmlbuffer.h"
#include "modules/xmlparser/xmldatasource.h"

BOOL
XMLInternalParser::IsPubidChar (uni_char c)
{
  if (c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' || c >= '0' && c <= '9')
    return TRUE;
  else
    return c < 128 && op_strchr ("\x0a\x0d -'()+,./:=?;!*#@$_%", c) != 0;
}

BOOL
XMLInternalParser::ConsumeWhitespaceAndPEReference ()
{
  BOOL consumed = FALSE;

  while (1)
    {
      const uni_char *ptr = buffer + index, *ptr_end = buffer + length;

      if (ptr != ptr_end)
        if (IsWhitespaceInMarkup (*ptr))
          {
            consumed = TRUE;

            ++ptr;

            while (ptr != ptr_end && IsWhitespaceInMarkup (*ptr))
              ++ptr;
          }

      index = ptr - buffer;

      if (ptr == ptr_end)
        {
          XMLDoctype::Entity *entity = current_buffer->GetCurrentEntity ();

          if (!current_buffer->GrowL (FALSE))
            LEAVE (PARSE_RESULT_END_OF_BUF);

          if (entity != current_buffer->GetCurrentEntity ())
            consumed = TRUE;

          continue;
        }

      /* Might happen when this function is called from
         ReadExternalIdProduction called from ReadDOCTYPEToken. */
      if (current_context == PARSE_CONTEXT_DOCUMENT)
        break;

      if (buffer[index] == '%')
        {
          if (!ConsumeEntityReference (TRUE))
            break;

          consumed = TRUE;

          if (current_context == PARSE_CONTEXT_INTERNAL_SUBSET)
            HandleError (WELL_FORMEDNESS_ERROR_PE_in_Internal_Subset);

          if (!reference_entity)
            {
#ifdef XML_VALIDATING_ENTITY_DECLARED
              if (xml_validating)
                HandleError (VALIDITY_ERROR_Undeclared_Entity);
#endif // XML_VALIDATING_ENTITY_DECLARED

              continue;
            }

          if (!reference_entity->GetValue ())
            {
              index = reference_start;

              /* This really means "possibly skipped markup declarations". */
              skipped_markup_declarations = TRUE;

#ifdef XML_SUPPORT_EXTERNAL_ENTITIES
              LoadEntity (reference_entity, PARSE_CONTEXT_PARAMETER_ENTITY);
#endif // XML_SUPPORT_EXTERNAL_ENTITIES

              /* LoadEntity will throw PARSE_RESULT_BLOCK unless the
                 entity will not be loaded, in which case it is skipped
                 and the rest of the contents of the document type
                 declaration will be parsed but not processed. */

              index += reference_length;

              if (standalone != XMLSTANDALONE_YES)
                skip_remaining_doctype = TRUE;
            }
          else
            {
              current_buffer->ExpandEntityReference (reference_length, reference_entity);

              if (reference_entity && ++token_counter == max_tokens_per_call)
                {
                  token_counter = 0;
                  is_paused = TRUE;
                  LEAVE (PARSE_RESULT_BLOCK);
                }
            }
        }
      else if (buffer[index] == '&')
        HandleError (WELL_FORMEDNESS_ERROR_GE_in_Doctype);
      else
        break;
    }

  return consumed;
}

void
XMLInternalParser::ReadExternalIdProduction (BOOL accept_missing, BOOL accept_missing_system)
{
  unsigned index_before = index;
  BOOL read_public;

  OP_DELETEA(system_literal);
  OP_DELETEA(pubid_literal);
  pubid_literal = 0;
  system_literal = 0;

  ConsumeWhitespaceAndPEReference ();

  if (MatchFollowedByWhitespaceOrPEReference (UNI_L ("PUBLIC"), 6))
    read_public = TRUE;
  else if (MatchFollowedByWhitespaceOrPEReference (UNI_L ("SYSTEM"), 6))
    read_public = FALSE;
  else
    {
      if (!accept_missing)
        HandleError (PARSE_ERROR_Invalid_ExternalId, index_before);

      index = index_before;
      return;
    }

  BOOL had_whitespace;

  if (read_public)
    {
      BOOL unproblematic;

      if (!ReadQuotedLiteral (unproblematic))
        HandleError (PARSE_ERROR_Invalid_PubidLiteral);

      const uni_char *ptr = literal, *ptr_end = ptr + literal_length;
      while (ptr != ptr_end && IsPubidChar (*ptr)) ++ptr;

      if (ptr != ptr_end)
#ifdef XML_SUPPORT_EXTERNAL_ENTITIES
        HandleError (PARSE_ERROR_Invalid_PubidLiteral, literal == literal_buffer ? literal_start : ptr - buffer);
#else // XML_SUPPORT_EXTERNAL_ENTITIES
        HandleError (PARSE_ERROR_Invalid_PubidLiteral, ptr - buffer);
#endif // XML_SUPPORT_EXTERNAL_ENTITIES

      CopyString (pubid_literal, literal, literal_length);

      had_whitespace = ConsumeWhitespaceAndPEReference ();
    }
  else
    had_whitespace = TRUE;

  BOOL unproblematic;

  if (!had_whitespace || !ReadQuotedLiteral (unproblematic))
    if (read_public && accept_missing_system)
      return;
    else
      HandleError (PARSE_ERROR_Invalid_SystemLiteral);

  CopyString (system_literal, literal, literal_length);
}

void
XMLInternalParser::ReadDoctypeSubset ()
{
  BOOL error = FALSE;

  if (in_comment)
    goto read_comment;

  while (1)
    {
#ifdef XML_SUPPORT_CONDITIONAL_SECTIONS
      if (current_conditional_context && current_conditional_context->type == XMLConditionalContext::Ignore)
        ReadIgnoreSection ();
#endif // XML_SUPPORT_CONDITIONAL_SECTIONS

      while (index < length && IsWhitespaceInMarkup (buffer[index]))
        ++index;

      if (index == length)
        {
          current_buffer->Consume ();

          if (!current_buffer->GrowL (FALSE))
            {
              expecting_eof = current_conditional_context == NULL;
              LEAVE (PARSE_RESULT_END_OF_BUF);
            }
        }
      else
        {
          if (current_context == PARSE_CONTEXT_INTERNAL_SUBSET && buffer[index] == ']')
            {
              current_buffer->Consume ();
              return;
            }
#ifdef XML_SUPPORT_CONDITIONAL_SECTIONS
          else if (current_conditional_context && Match (UNI_L ("]]>"), 3))
            {
              XMLConditionalContext *discarded_context = current_conditional_context;

              current_conditional_context = discarded_context->next;
              OP_DELETE (discarded_context);
            }
#endif // XML_SUPPORT_CONDITIONAL_SECTIONS
          else if (buffer[index] == '%')
            {
              ReadPEReference ();
              continue;
            }
          else if (buffer[index] == '<')
            {
#ifdef XML_ERRORS
              current_buffer->GetLocation (0, last_error_location.start);
#endif // XML_ERRORS

#ifdef XML_VALIDATING_PE_IN_MARKUP_DECL
              containing_entity = GetCurrentEntity ();
#endif // XML_VALIDATING_PE_IN_MARKUP_DECL

              ConsumeChar ();

              if (buffer[index] == '?')
                {
                  ConsumeChar ();
                  ReadPIToken (TRUE);
                }
              else if (buffer[index] == '!')
                {
                  ConsumeChar ();

                  if (Match (UNI_L ("--"), 2))
                    read_comment: ReadCommentToken (TRUE);
                  else if (Match (UNI_L ("["), 1))
#ifdef XML_SUPPORT_CONDITIONAL_SECTIONS
                    ReadConditionalSection ();
#else // XML_SUPPORT_CONDITIONAL_SECTIONS
                    HandleError (UNSUPPORTED_ConditionalSections);
#endif // XML_SUPPORT_CONDITIONAL_SECTIONS
                  else if (MatchFollowedByWhitespaceOrPEReference (UNI_L ("ELEMENT"), 7))
                    ReadELEMENTDecl ();
                  else if (MatchFollowedByWhitespaceOrPEReference (UNI_L ("ATTLIST"), 7))
                    ReadATTLISTDecl ();
                  else if (MatchFollowedByWhitespaceOrPEReference (UNI_L ("ENTITY"), 6))
                    ReadENTITYDecl ();
                  else if (MatchFollowedByWhitespaceOrPEReference (UNI_L ("NOTATION"), 8))
                    ReadNOTATIONDecl ();
                  else
                    error = TRUE;
                }
              else
                error = TRUE;
            }
          else
            {
#ifdef XML_ERRORS
              current_buffer->GetLocation (0, last_error_location.start);
#endif // XML_ERRORS

              error = TRUE;
            }

          if (error)
            HandleError (PARSE_ERROR_Invalid_DOCTYPE);
          else
            {
#ifdef XML_VALIDATING_PE_IN_MARKUP_DECL
              if (xml_validating && GetCurrentEntity () != containing_entity)
                HandleError (VALIDITY_ERROR_PEinXMLDecl);
#endif // XML_VALIDATING_PE_IN_MARKUP_DECL

              current_buffer->Consume ();
            }
        }
    }
}

#ifdef XML_SUPPORT_CONDITIONAL_SECTIONS

void
XMLInternalParser::ReadConditionalSection ()
{
  XMLConditionalContext::Type type;

  if (current_context != PARSE_CONTEXT_EXTERNAL_SUBSET && current_context != PARSE_CONTEXT_PE_BETWEEN_DECLS)
    HandleError (PARSE_ERROR_Invalid_DOCTYPE);

  ConsumeWhitespaceAndPEReference ();

  if (Match (UNI_L ("INCLUDE"), 7))
    type = XMLConditionalContext::Include;
  else if (Match (UNI_L ("IGNORE"), 6) || skip_remaining_doctype)
    type = XMLConditionalContext::Ignore;
  else
    {
      HandleError (PARSE_ERROR_Invalid_DOCTYPE_conditional);
      return;
    }

  ConsumeWhitespaceAndPEReference ();

  if (buffer[index] != '[')
    HandleError (PARSE_ERROR_Invalid_DOCTYPE_conditional);

  XMLConditionalContext *context = OP_NEW_L (XMLConditionalContext, ());
  context->next = current_conditional_context;
  context->type = type;
  context->depth = 1;
  current_conditional_context = context;

  ++index;
  current_buffer->Consume ();
}

void
XMLInternalParser::ReadIgnoreSection ()
{
  while (1)
    {
      const uni_char *ptr = buffer + index, *ptr_end = buffer + length;

      while (ptr != ptr_end && *ptr != ']' && *ptr != '<')
        ++ptr;

      index = ptr - buffer;

      if (index == length)
        {
          current_buffer->Consume ();

          if (!current_buffer->GrowL (FALSE))
            LEAVE (PARSE_RESULT_END_OF_BUF);
        }
      else if (Match (UNI_L ("]]>"), 3))
        {
          if (--current_conditional_context->depth == 0)
            {
              XMLConditionalContext *discarded_context = current_conditional_context;

              current_conditional_context = discarded_context->next;
              OP_DELETE (discarded_context);

              current_buffer->Consume ();
              return;
            }
        }
      else if (Match (UNI_L ("<!["), 3))
        ++current_conditional_context->depth;
      else
        ++index;
    }
}

#endif // XML_SUPPORT_CONDITIONAL_SECTIONS

void
XMLInternalParser::ReadDOCTYPEToken ()
{
  token.SetType (XMLToken::TYPE_DOCTYPE);

  if (!ConsumeWhitespace ())
    HandleError (PARSE_ERROR_Invalid_DOCTYPE);

  if (!XML_READQNAME ())
    HandleError (PARSE_ERROR_Invalid_DOCTYPE_name);

  doctype->SetName (literal, literal_length);

  ReadExternalIdProduction (TRUE, FALSE);

  if (pubid_literal)
    {
      doctype->SetPubid (pubid_literal);
      pubid_literal = 0;
    }

  if (system_literal)
    {
      doctype->SetSystem (system_literal);
      system_literal = 0;
    }

  ConsumeWhitespace ();

  /* ('[' (markupdecl | DeclSep)* ']' S?)? */
  if (buffer[index] == '[')
    {
      ++index;
      current_buffer->Consume ();

      if (configuration->store_internal_subset)
        current_buffer->SetCopyBuffer (doctype->GetInternalSubsetBuffer ());

      current_context = PARSE_CONTEXT_INTERNAL_SUBSET;

      ReadDoctypeSubset ();

      if (configuration->store_internal_subset)
        current_buffer->SetCopyBuffer (0);

      current_context = PARSE_CONTEXT_DOCUMENT;

      /* Consumes ']'. */
      ++index;

      ConsumeWhitespace ();
    }

  if (buffer[index] != '>')
    HandleError (PARSE_ERROR_Invalid_DOCTYPE);

  ++index;

  current_buffer->Consume ();

  FinishDOCTYPEToken ();
}

void
XMLInternalParser::ContinueDOCTYPEToken ()
{
  token.SetType (XMLToken::TYPE_DOCTYPE);

  ReadDoctypeSubset ();

  if (configuration->store_internal_subset)
    current_buffer->SetCopyBuffer (0);

  /* Consumes ']'. */
  ++index;

  ConsumeWhitespace ();

  if (buffer[index] != '>')
    HandleError (PARSE_ERROR_Invalid_DOCTYPE);

  ++index;
  current_buffer->Consume ();
  current_context = PARSE_CONTEXT_DOCUMENT;

  FinishDOCTYPEToken ();
}

#ifdef XML_STORE_ELEMENTS
typedef XMLDoctype::Element XMLDoctypeElement;
#else // XML_STORE_ELEMENTS
typedef void XMLDoctypeElement;
#endif // XML_STORE_ELEMENTS

static BOOL
HandleQuantifier (XMLDoctypeElement *element, uni_char ch)
{
  if (ch == '?' || ch == '*' || ch == '+')
    {
#ifdef XML_VALIDATING_ELEMENT_CONTENT
      if (element)
        if (ch == '?')
          element->SetOptional ();
        else if (ch == '*')
          element->SetRepeatZeroOrMore ();
        else
          element->SetRepeatOneOrMore ();
#endif // XML_VALIDATING_ELEMENT_CONTENT

      return TRUE;
    }
  else
    return FALSE;
}

void
XMLInternalParser::ReadELEMENTDecl ()
{
#ifdef XML_STORE_ELEMENTS
  current_element = OP_NEW(XMLDoctypeElement, (current_context != PARSE_CONTEXT_INTERNAL_SUBSET));
  if (!current_element)
    LEAVE (PARSE_RESULT_OOM);
  XMLDoctypeElement *quantifier_element;
  if (xml_validating)
    quantifier_element = current_element;
  else
    quantifier_element = 0;
#else // XML_STORE_ELEMENTS
  XMLDoctypeElement *const quantifier_element = 0;
#endif // XML_STORE_ELEMENTS

  /*
   elementdecl ::= '<!ELEMENT' S Name S contentspec S? '>'
   */

  /* Note: '<!ELEMENT' has already been consumed, index points to the
     first character after it. */

  if (!XML_READQNAME ())
    HandleError (PARSE_ERROR_Invalid_ELEMENT_name);

#ifdef XML_STORE_ELEMENTS
  current_element->SetName (literal, literal_length);
#endif // XML_STORE_ELEMENTS

  if (!ConsumeWhitespaceAndPEReference ())
    HandleError (PARSE_ERROR_Invalid_ELEMENT);

  /*
   contentspec ::= 'EMPTY' | 'ANY' | Mixed | children
   */

#undef OPTIONAL

#ifdef XML_VALIDATING_ELEMENT_CONTENT
# define OPTIONAL(expression) do { if (xml_validating) expression; } while (0)
#else // XML_VALIDATING_ELEMENT_CONTENT
# define OPTIONAL(expression)
#endif // XML_VALIDATING_ELEMENT_CONTENT

  if (Match (UNI_L ("EMPTY"), 5))
    OPTIONAL (current_element->SetContentModel (XMLDoctypeElement::CONTENT_MODEL_EMPTY));
  else if (Match (UNI_L ("ANY"), 3))
    OPTIONAL (current_element->SetContentModel (XMLDoctypeElement::CONTENT_MODEL_ANY));
  else
    {
      /*
       Mixed    ::= '(' S? '#PCDATA' (S? '|' S? Name)* S? ')*' |
                    '(' S? '#PCDATA' S? ')'

       children ::= (choice | seq) ('?' | '*' | '+')?
       cp       ::= (Name | choice | seq) ('?' | '*' | '+')?
       choice   ::= '(' S? cp (S? '|' S? cp)+ S? ')'
       seq      ::= '(' S? cp (S? ',' S? cp)* S? ')'
       */

      if (buffer[index] != '(')
        HandleError (PARSE_ERROR_Invalid_ELEMENT_contentspec);

#ifdef XML_VALIDATING_PE_IN_GROUP
      if (xml_validating)
        current_peingroup = OP_NEW_L(XMLPEInGroup, (0, GetCurrentEntity ()));
#endif // XML_VALIDATING_PE_IN_GROUP

      ++index;
      ConsumeWhitespaceAndPEReference ();

      if (Match (UNI_L ("#PCDATA"), 7))
        {
#ifdef XML_VALIDATING_ELEMENT_CONTENT
          if (xml_validating)
            {
              current_element->SetContentModel (XMLDoctypeElement::CONTENT_MODEL_Mixed);
              current_element->Open ();
              current_element->SetChoice ();
            }
#endif // XML_VALIDATING_ELEMENT_CONTENT

          ConsumeWhitespaceAndPEReference ();

          if (buffer[index] != ')')
            {
              while (1)
                {
                  if (buffer[index] != '|')
                    HandleError (PARSE_ERROR_Invalid_ELEMENT_contentspec_Mixed);

                  ++index;
                  ConsumeWhitespaceAndPEReference ();

                  if (!XML_READQNAME ())
                    HandleError (PARSE_ERROR_Invalid_ELEMENT_contentspec_Mixed);

#ifdef XML_VALIDATING_ELEMENT_CONTENT
                  if (xml_validating)
                    {
                      if (current_element->HasChild (literal, literal_length))
                        HandleError (VALIDITY_ERROR_Invalid_ELEMENT_contentspec_Mixed, literal_start);

                      current_element->AddChild (literal, literal_length);
                    }
#endif // XML_VALIDATING_ELEMENT_CONTENT

                  ConsumeWhitespaceAndPEReference ();

                  if (buffer[index] == ')')
                    break;
                }

              ConsumeChar ();

              if (buffer[index] != '*')
                HandleError (PARSE_ERROR_Invalid_ELEMENT_contentspec_Mixed);

              ++index;
            }
          else
            {
              ConsumeChar ();

              if (buffer[index] == '*')
                ++index;
            }

#ifdef XML_VALIDATING_ELEMENT_CONTENT
          if (xml_validating)
            {
              current_element->Close ();
              current_element->SetRepeatZeroOrMore ();
            }
#endif // XML_VALIDATING_ELEMENT_CONTENT

#ifdef XML_VALIDATING_PE_IN_GROUP
          if (xml_validating)
            {
              if (GetCurrentEntity () != current_peingroup->entity)
                HandleError (VALIDITY_ERROR_ProperGroupPENesting);

              OP_DELETE(current_peingroup);
              current_peingroup = 0;
            }
#endif // XML_VALIDATING_PE_IN_GROUP
        }
      else
        {
          current_contentspecgroup = OP_NEW_L(XMLContentSpecGroup, (0));

#ifdef XML_VALIDATING_ELEMENT_CONTENT
          if (xml_validating)
            {
              current_element->SetContentModel (XMLDoctypeElement::CONTENT_MODEL_children);
              current_element->Open ();
            }
#endif // XML_VALIDATING_ELEMENT_CONTENT

          unsigned depth = 1;

          enum
            {
              ITEM, SEPARATOR
            } expect = ITEM;

          while (1)
            {
              if (index == length)
                if (!GrowBetweenMarkup ())
                  LEAVE (PARSE_RESULT_END_OF_BUF);

              if (expect == ITEM)
                if (buffer[index] == '(')
                  {
                    current_contentspecgroup = OP_NEW_L(XMLContentSpecGroup, (current_contentspecgroup));

                    ++depth;
                    OPTIONAL (current_element->Open ());

#ifdef XML_VALIDATING_PE_IN_GROUP
                    if (xml_validating)
                      current_peingroup = OP_NEW_L(XMLPEInGroup, (current_peingroup, GetCurrentEntity ()));
#endif // XML_VALIDATING_PE_IN_GROUP

                    ConsumeChar ();
                    ConsumeWhitespaceAndPEReference ();
                  }
                else if (XML_READQNAME ())
                  {
                    OPTIONAL (current_element->AddChild (literal, literal_length));
                    expect = SEPARATOR;
                  }
                else
                  HandleError (PARSE_ERROR_Invalid_ELEMENT_contentspec_cp);
              else
                {
                  if (HandleQuantifier (quantifier_element, buffer[index]))
                    ConsumeChar ();

                  ConsumeWhitespaceAndPEReference ();

                  if (buffer[index] == ')')
                    {
                      XMLContentSpecGroup *discarded_contentspecgroup = current_contentspecgroup;
                      current_contentspecgroup = discarded_contentspecgroup->next;
                      discarded_contentspecgroup->next = 0;
                      OP_DELETE(discarded_contentspecgroup);

#ifdef XML_VALIDATING_PE_IN_GROUP
                      if (xml_validating)
                        {
                          if (GetCurrentEntity () != current_peingroup->entity)
                            HandleError (VALIDITY_ERROR_ProperGroupPENesting);

                          XMLPEInGroup *discarded_peingroup = current_peingroup;
                          current_peingroup = discarded_peingroup->next;
                          discarded_peingroup->next = 0;
                          OP_DELETE(discarded_peingroup);
                        }
#endif // XML_VALIDATING_PE_IN_GROUP

                      if (--depth == 0)
                        break;
#ifdef XML_VALIDATING_ELEMENT_CONTENT
                      else
                        OPTIONAL (current_element->Close ());
#endif // XML_VALIDATING_ELEMENT_CONTENT

                      ConsumeChar ();
                      continue;
                    }
                  else if (buffer[index] == '|')
                    {
                      if (current_contentspecgroup->type == XMLContentSpecGroup::SEQUENCE)
                        HandleError (PARSE_ERROR_Invalid_ELEMENT_contentspec_seq);
                      else
                        current_contentspecgroup->type = XMLContentSpecGroup::CHOICE;

                      expect = ITEM;
                      OPTIONAL (current_element->SetChoice ());
                    }
                  else if (buffer[index] == ',')
                    {
                      if (current_contentspecgroup->type == XMLContentSpecGroup::CHOICE)
                        HandleError (PARSE_ERROR_Invalid_ELEMENT_contentspec_seq);
                      else
                        current_contentspecgroup->type = XMLContentSpecGroup::SEQUENCE;

                      expect = ITEM;
                      OPTIONAL (current_element->SetSequence ());
                    }
                  else
                    HandleError (PARSE_ERROR_Invalid_ELEMENT_contentspec_cp);

                  ConsumeChar ();
                  ConsumeWhitespaceAndPEReference ();
                }
            }

          ConsumeChar ();

          OPTIONAL (current_element->Close ());

          if (HandleQuantifier (quantifier_element, buffer[index]))
              ++index;

#ifdef XML_VALIDATING_ELEMENT_CONTENT
          if (xml_validating)
            if (!current_element->IsDeterministic ())
              HandleError (COMPATIBILITY_ERROR_ContentModelDeterministic);
#endif // XML_VALIDATING_ELEMENT_CONTENT
        }
    }

  ConsumeWhitespaceAndPEReference ();

  if (buffer[index] != '>')
    HandleError (PARSE_ERROR_Invalid_ELEMENT);

  ++index;

#ifdef XML_STORE_ELEMENTS
  AddElement ();
#endif // XML_STORE_ELEMENTS
}

#ifdef XML_STORE_ELEMENTS

void
XMLInternalParser::AddElement ()
{
  if (skip_remaining_doctype)
    OP_DELETE(current_element);
  else if (doctype->GetElement (current_element->GetName ()))
    {
#ifdef XML_VALIDATING
      if (xml_validating)
        HandleError (VALIDITY_ERROR_ElementAlreadyDeclared);
#endif // XML_VALIDATING

      OP_DELETE(current_element);
    }
  else
    {
	  XMLDoctype::Element* new_element = current_element;
	  // AddElement will delete element and leave on OOM so make sure we don't keep a pointer to current_element
	  current_element = 0;
      doctype->AddElement (new_element);
    }

  current_element = 0;
}

#endif // XML_STORE_ELEMENTS

const unsigned XML_AttType_strings_lengths[] =
  {
    5,
    6,
    5,
    2,
    6,
    8,
    8,
    7
  };

const XMLDoctype::Attribute::Type XML_AttrType_types[] =
  {
    XMLDoctype::Attribute::TYPE_String,
    XMLDoctype::Attribute::TYPE_Tokenized_IDREFS,
    XMLDoctype::Attribute::TYPE_Tokenized_IDREF,
    XMLDoctype::Attribute::TYPE_Tokenized_ID,
    XMLDoctype::Attribute::TYPE_Tokenized_ENTITY,
    XMLDoctype::Attribute::TYPE_Tokenized_ENTITIES,
    XMLDoctype::Attribute::TYPE_Tokenized_NMTOKENS,
    XMLDoctype::Attribute::TYPE_Tokenized_NMTOKEN
  };

void
XMLInternalParser::ReadATTLISTDecl ()
{
  unsigned element_name_start, element_name_length;

  /*
   AttlistDecl ::= '<!ATTLIST' S Name AttDef* S? '>'
  */

  /* Note: '<!ATTLIST' has already been consumed, index points to the
     first character after it. */

  if (!XML_READQNAME ())
    HandleError (PARSE_ERROR_Invalid_ATTLIST_elementname);

  element_name_start = literal_start;
  element_name_length = literal_length;

  while (ConsumeWhitespaceAndPEReference ())
    {
      /*
       AttDef ::= S Name S AttType S DefaultDecl
      */

      if (!XML_READQNAME ())
        break;

#undef OPTIONAL

#ifdef XML_STORE_ATTRIBUTES
      current_attribute = OP_NEW_L(XMLDoctype::Attribute, (current_context != PARSE_CONTEXT_INTERNAL_SUBSET));

      current_attribute->SetElementName (buffer + element_name_start, element_name_length);
      current_attribute->SetAttributeName (literal, literal_length);

# define OPTIONAL(expr) expr
#else // XML_STORE_ATTRIBUTES
# define OPTIONAL(expr)
#endif // XML_STORE_ATTRIBUTES

      if (!ConsumeWhitespaceAndPEReference ())
        HandleError (buffer[index] == '>' ? PARSE_ERROR_Invalid_ATTLIST : PARSE_ERROR_Invalid_ATTLIST_elementname);

      /*
       AttType       ::= StringType | TokenizedType | EnumeratedType
       StringType    ::= 'CDATA'
       TokenizedType ::= 'ID' | 'IDREF' | 'IDREFS' | 'ENTITY' |
                         'ENTITIES' | 'NMTOKEN' | 'NMTOKENS'
      */

      XMLDoctype::Attribute::Type type = XMLDoctype::Attribute::TYPE_Unknown;
      const uni_char *AttType_string = UNI_L ("CDATAIDREFSIDREFIDENTITYENTITIESNMTOKENSNMTOKEN");

      for (unsigned index2 = 0; index2 < 8; ++index2)
        {
          unsigned length = XML_AttType_strings_lengths[index2];
          if (MatchFollowedByWhitespaceOrPEReference (AttType_string, length))
            {
              type = XML_AttrType_types[index2];
              break;
            }
          else
            AttType_string += length;
        }

#ifdef XML_VALIDATING
      if (xml_validating && attlist_index >= attlist_skip && type == XMLDoctype::Attribute::TYPE_Tokenized_ID && doctype->GetElementHasIDAttribute (buffer + element_name_start, element_name_length))
        HandleError (VALIDITY_ERROR_IDAttribute_OnePerElement, literal_start);
#endif // XML_VALIDATING

      if (type == XMLDoctype::Attribute::TYPE_Unknown)
        {
          /*
           EnumeratedType ::= NotationType | Enumeration
           NotationType   ::= 'NOTATION' S
                              '(' S? Name (S? '|' S? Name)* S? ')'
           Enumeration    ::= '(' S? Nmtoken (S? '|' S? Nmtoken)* S? ')'
           */

          ParseError error;

          if (MatchFollowedByWhitespaceOrPEReference (UNI_L ("NOTATION"), 8))
            {
              type = XMLDoctype::Attribute::TYPE_Enumerated_Notation;
              error = PARSE_ERROR_Invalid_ATTLIST_NotationType;
            }
          else
            {
              type = XMLDoctype::Attribute::TYPE_Enumerated_Enumeration;
              error = PARSE_ERROR_Invalid_ATTLIST_Enumeration;
            }

          if (buffer[index] != '(')
            HandleError (error);

          ++index;

          unsigned count = 0;

          while (1)
            {
              ConsumeWhitespaceAndPEReference ();

              if (type == XMLDoctype::Attribute::TYPE_Enumerated_Enumeration ? !ReadNmToken () : !XML_READNCNAME ())
                HandleError (error);

#ifdef XML_STORE_ATTRIBUTES
#ifdef XML_VALIDATING
              if (xml_validating)
                {
                  const uni_char **enumerators = current_attribute->GetEnumerators ();
                  unsigned enumerators_count = current_attribute->GetEnumeratorsCount ();

                  for (unsigned enumerator_index = 0; enumerator_index < enumerators_count; ++enumerator_index)
                    if (CompareStrings (literal, literal_length, enumerators[enumerator_index]))
                      {
                        HandleError (VALIDITY_ERROR_Enumeration_NoDuplicateTokens);
                        break;
                      }
                }
#endif // XML_VALIDATING

              current_attribute->AddEnumerator (literal, literal_length);
#endif // XML_STORE_ATTRIBUTES

              ++count;

              ConsumeWhitespaceAndPEReference ();

              if (buffer[index] == ')')
                break;
              else if (buffer[index] != '|')
                HandleError (error);

              ++index;
            }

          if (count == 0)
            HandleError (error);

          ++index;

          if (!ConsumeWhitespaceAndPEReference ())
            HandleError (PARSE_ERROR_Invalid_ATTLIST);
        }

      OPTIONAL (current_attribute->SetType (type));

      /*
       DefaultDecl ::= '#REQUIRED' | '#IMPLIED' |
                       (('#FIXED' S)? AttValue)
       */

      if (Match (UNI_L ("#REQUIRED"), 9))
        OPTIONAL (current_attribute->SetFlag (XMLDoctype::Attribute::FLAG_REQUIRED));
      else if (Match (UNI_L ("#IMPLIED"), 8))
        OPTIONAL (current_attribute->SetFlag (XMLDoctype::Attribute::FLAG_IMPLIED));
      else
        {
#ifdef XML_VALIDATING
          if (xml_validating && type == XMLDoctype::Attribute::TYPE_Tokenized_ID)
            HandleError (VALIDITY_ERROR_IDAttribute_HasDefaultValue);
#endif // XML_VALIDATING

#ifdef XML_STORE_ATTRIBUTES
          if (MatchFollowedByWhitespaceOrPEReference (UNI_L ("#FIXED"), 6))
            current_attribute->SetFlag (XMLDoctype::Attribute::FLAG_FIXED);
#else // XML_STORE_ATTRIBUTES
          MatchFollowedByWhitespaceOrPEReference (UNI_L ("#FIXED"), 6);
#endif // XML_STORE_ATTRIBUTES

          BOOL in_entity_reference = current_buffer->IsInEntityReference (), unproblematic;

          if (!ReadQuotedLiteral (unproblematic))
            HandleError (PARSE_ERROR_Invalid_ATTLIST_DefaultDecl);

#ifdef XML_ATTRIBUTE_DEFAULT_VALUES
          NormalizeReset ();

#ifdef XML_STORE_ATTRIBUTES
          attribute_ws_normalized = FALSE;
          attribute_is_cdata = current_attribute->GetType () == XMLDoctype::Attribute::TYPE_String;
#else // XML_STORE_ATTRIBUTES
          attribute_is_cdata = TRUE;
#endif // XML_STORE_ATTRIBUTES

#ifdef XML_SUPPORT_EXTERNAL_ENTITIES
          Normalize (literal, literal_length, TRUE, !in_entity_reference, literal == literal_buffer ? literal_start : ~0u);
#else // XML_SUPPORT_EXTERNAL_ENTITIES
          Normalize (literal, literal_length, TRUE, !in_entity_reference);
#endif // XML_SUPPORT_EXTERNAL_ENTITIES

          uni_char *value = GetNormalizedString ();
          unsigned value_length = GetNormalizedStringLength ();

#ifdef XML_VALIDATING
          if (xml_validating)
            CheckAttValue (current_attribute, value, value_length);
#endif // XML_VALIDATING

          current_attribute->SetDefaultValue (value, value_length);
#endif // XML_ATTRIBUTE_DEFAULT_VALUES
        }

#ifdef XML_VALIDATING
      if (uni_strcmp (current_attribute->GetAttributeName (), "xml:space") == 0)
        {
          BOOL valid = TRUE;

          if (type != XMLDoctype::Attribute::TYPE_Enumerated_Enumeration)
            valid = FALSE;
          else
            {
              const uni_char **enumerators = current_attribute->GetEnumerators ();
              unsigned enumerators_count = current_attribute->GetEnumeratorsCount ();

              for (unsigned index = 0; index < enumerators_count; ++index)
                if (uni_strcmp (enumerators[index], "default") != 0 && uni_strcmp (enumerators[index], "preserve") != 0)
                  valid = FALSE;
            }

          if (!valid)
            HandleError (VALIDITY_ERROR_Invalid_XmlSpaceAttribute);
        }
#endif // XML_VALIDATING

#ifdef XML_STORE_ATTRIBUTES
      AddAttribute ();
#endif // XML_STORE_ATTRIBUTES
    }

  if (buffer[index] != '>')
    HandleError (PARSE_ERROR_Invalid_ATTLIST_attributename);

  ++index;

  attlist_index = attlist_skip = 0;
}

#ifdef XML_STORE_ATTRIBUTES

void
XMLInternalParser::AddAttribute ()
{
  if (skip_remaining_doctype || attlist_index < attlist_skip)
    OP_DELETE(current_attribute);
  else
    {
	  XMLDoctype::Attribute* new_attr = current_attribute;
	  // AddAttribute will delete attribute and leave on OOM so make sure we don't keep a pointer to current_attribute
	  current_attribute = 0;
      doctype->AddAttribute (new_attr);
    }

  current_attribute = 0;
  ++attlist_index;
}

#endif // XML_STORE_ATTRIBUTES

void
XMLInternalParser::ReadENTITYDecl ()
{
  BOOL general_entity = TRUE;

  current_entity = OP_NEW_L(XMLDoctype::Entity, (current_context != PARSE_CONTEXT_INTERNAL_SUBSET));

  /*
   EntityDecl ::= GEDecl | PEDecl
   GEDecl     ::= '<!ENTITY' S Name S EntityDef S? '>'
   PEDecl     ::= '<!ENTITY' S '%' S Name S PEDef S? '>'
   EntityDef  ::= EntityValue | (ExternalID NDataDecl?)
   PEDef      ::= EntityValue | ExternalID
  */

  /* Note: '<!ENTITY' has already been consumed, index points to the
     first character after it. */

  if (buffer[index] == '%')
    {
      general_entity = FALSE;
      current_entity->SetType (XMLDoctype::Entity::TYPE_Parameter);

      ++index;

      if (!ConsumeWhitespaceAndPEReference ())
        HandleError (PARSE_ERROR_Invalid_ENTITY);
    }

  if (!XML_READNCNAME ())
    HandleError (PARSE_ERROR_Invalid_ENTITY_name);

  current_entity->SetName (literal, literal_length);

  if (!ConsumeWhitespaceAndPEReference ())
    HandleError (PARSE_ERROR_Invalid_ENTITY);

  BOOL in_entity_reference = current_buffer->IsInEntityReference (), unproblematic;

  if (ReadQuotedLiteral (unproblematic))
    {
      /*
       EntityValue ::= '"' ([^%&"] | PEReference | Reference)* '"' |
                       "'" ([^%&'] | PEReference | Reference)* "'"
      */

      NormalizeReset ();

      attribute_is_cdata = FALSE;

#ifdef XML_SUPPORT_EXTERNAL_ENTITIES
      Normalize (literal, literal_length, FALSE, !in_entity_reference, literal == literal_buffer ? index : ~0u);
#else // XML_SUPPORT_EXTERNAL_ENTITIES
      Normalize (literal, literal_length, FALSE, !in_entity_reference);
#endif // XML_SUPPORT_EXTERNAL_ENTITIES

      current_entity->SetValue (GetNormalizedString (), GetNormalizedStringLength (), FALSE);
    }
  else
    {
      ReadExternalIdProduction (FALSE, FALSE);

      if (pubid_literal)
        {
          current_entity->SetPubid (pubid_literal);
          pubid_literal = 0;
        }

      if (system_literal)
        {
          current_entity->SetSystem (system_literal, current_source->GetURL ());
          system_literal = 0;
        }

      if (general_entity && ConsumeWhitespaceAndPEReference ())
        {
          /*
           NDataDecl ::= S 'NDATA' S Name
          */

          if (Match (UNI_L ("NDATA"), 5))
            {
              if (!ConsumeWhitespaceAndPEReference () || !XML_READNCNAME ())
                HandleError (PARSE_ERROR_Invalid_ENTITY);

              current_entity->SetNDataName (literal, literal_length);
            }
        }
    }

  ConsumeWhitespaceAndPEReference ();

  if (buffer[index] != '>')
    HandleError (PARSE_ERROR_Invalid_ENTITY);

  ++index;

  AddEntity ();
}

void
XMLInternalParser::AddEntity ()
{
  if (current_entity)
    {
      if (skip_remaining_doctype)
        OP_DELETE(current_entity);
      else
        doctype->AddEntity (current_entity);

      current_entity = 0;
    }
}

void
XMLInternalParser::ReadNOTATIONDecl ()
{
#undef OPTIONAL

#ifdef XML_STORE_NOTATIONS
  current_notation = OP_NEW(XMLDoctype::Notation, ());

# define OPTIONAL(expr) expr
#else // XML_STORE_NOTATIONS
# define OPTIONAL(expr)
#endif // XML_STORE_NOTATIONS

  /*
   NotationDecl ::= <!NOTATION' S Name S (ExternalID | PublicID) S? '>'
  */

  /* Note: '<!NOTATION' has already been consumed, index points to the
     first character after it. */

  if (!XML_READNCNAME ())
    HandleError (PARSE_ERROR_Invalid_NOTATION_name);

  OPTIONAL (current_notation->SetName (literal, literal_length));

  ConsumeWhitespaceAndPEReference ();

  ReadExternalIdProduction (FALSE, TRUE);

  OPTIONAL (if (pubid_literal) { current_notation->SetPubid (pubid_literal); pubid_literal = 0; });
  OPTIONAL (if (system_literal) { current_notation->SetSystem (system_literal); system_literal = 0; });

  ConsumeWhitespaceAndPEReference ();

  if (buffer[index] != '>')
    HandleError (PARSE_ERROR_Invalid_NOTATION);

  ++index;

  OPTIONAL (AddNotation ());
}

#ifdef XML_STORE_NOTATIONS

void
XMLInternalParser::AddNotation ()
{
  if (skip_remaining_doctype)
    OP_DELETE(current_notation);
  else
    {
	  XMLDoctype::Notation* new_notation = current_notation;
	  // AddNotation will delete notation and leave on OOM so make sure we don't keep a pointer to current_notation
	  current_notation = 0;
      doctype->AddNotation (new_notation);
    }

  current_notation = 0;
}

#endif // XML_STORE_NOTATIONS

void
XMLInternalParser::ReadPEReference ()
{
  ConsumeEntityReference ();

  if (!reference_entity)
    {
#ifdef XML_VALIDATING_ENTITY_DECLARED
      if (xml_validating)
        HandleError (VALIDITY_ERROR_Undeclared_Entity);
#endif // XML_VALIDATING_ENTITY_DECLARED
    }
  else
    {
      current_buffer->Consume ();

      /* This really means "possibly skipped markup declarations". */
      skipped_markup_declarations = TRUE;

      LoadEntity (reference_entity, reference_entity->GetValueType () == XMLDoctype::Entity::VALUE_TYPE_Internal ? PARSE_CONTEXT_PE_BETWEEN_DECLS : PARSE_CONTEXT_PE_BETWEEN_DECLS_INITIAL);

      /* LoadEntity will throw PARSE_RESULT_BLOCK unless the
         entity will not be loaded, in which case it is skipped
         and the rest of the contents of the document type
         declaration will be parsed but not processed. */

      if (standalone != XMLSTANDALONE_YES)
        skip_remaining_doctype = TRUE;
    }
}

void
XMLInternalParser::FinishDOCTYPEToken ()
{
  if ((doctype->GetPubid () || doctype->GetSystem ()) && datasource_handler)
    {
      /* This really means "possibly skipped markup declarations". */
      skipped_markup_declarations = TRUE;

#ifdef XML_SUPPORT_EXTERNAL_ENTITIES
      if (XMLDoctype *external_subset = datasource_handler->GetCachedExternalSubset (doctype->GetPubid (), doctype->GetSystem (), current_source->GetURL ()))
        if (doctype->SetExternalSubset (external_subset, TRUE))
          return;

      if (load_external_entities)
        {
          XMLDataSource *source;

          LEAVE_IF_ERROR (datasource_handler->CreateExternalDataSource (source, doctype->GetPubid (), doctype->GetSystem (), current_source->GetURL ()));

          if (source)
            {
              OpStackAutoPtr<XMLDataSource> anchor (source);

              XMLBuffer *buffer = OP_NEW_L(XMLBuffer, (source, version == XMLVERSION_1_1));
              buffer->Initialize (32768);
              source->SetBuffer (buffer);

              XMLInternalParserState *state = OP_NEW_L(XMLInternalParserState, ());
              state->context = PARSE_CONTEXT_EXTERNAL_SUBSET_INITIAL;
              source->SetParserState (state);
              source->SetNextSource (current_source);

              blocking_source = source;

              anchor.release ();
              LEAVE (PARSE_RESULT_BLOCK);
            }
        }
#endif // XML_SUPPORT_EXTERNAL_ENTITIES
    }

  doctype->Finish ();

#ifdef XML_VALIDATING
  if (xml_validating)
    {
      BOOL invalid = FALSE;

      /* FIXME: errors reported here will have fairly bogus error locations. */
      doctype->ValidateDoctype (this, invalid);

      if (invalid && xml_validating_fatal)
        LEAVE (PARSE_RESULT_ERROR);
    }
#endif // XML_VALIDATING

  if (!HandleToken ())
    LEAVE (PARSE_RESULT_BLOCK);
}
