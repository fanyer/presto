/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/xmlparser/xmlcommon.h"
#include "modules/xmlparser/xmlinternalparser.h"
#include "modules/xmlparser/xmlbuffer.h"
#include "modules/xmlparser/xmldatasource.h"
#include "modules/xmlparser/xmldoctype.h"
#include "modules/xmlparser/src/xmlcheckingtokenhandler.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/xmlutils/xmlerrorreport.h"

#include "modules/util/str.h"
#include "modules/util/tempbuf.h"

#ifdef XML_DUMP_DOCTYPE
# include "modules/doc/frm_doc.h"
# include "modules/dom/domenvironment.h"
# include "modules/dom/domutils.h"
# include "modules/ecmascript/ecmascript.h"
#endif // XML_DUMP_DOCTYPE

#if 0
# include "modules/newdebug/src/profiler.h"
# define MARKUP_PROFILE_PARSER(id) OP_PROFILE(id)
#else
# define MARKUP_PROFILE_PARSER(id)
#endif

extern unsigned
XMLLength (const uni_char *string, unsigned string_length);

extern void **
XMLGrowArray (void **old_array, unsigned count, unsigned &total);

#define GROW_ARRAY(array, type) (array = (type *) XMLGrowArray ((void **) array, array##_count, array##_total))

XMLInternalParser::XMLInternalParser (XMLParser *parser, const XMLParser::Configuration *configuration)
  : current_context (PARSE_CONTEXT_DOCUMENT),
    current_conditional_context (0),
    normalize_buffer (0),
    normalize_buffer_count (0),
    normalize_buffer_total (0),
#ifdef XML_SUPPORT_EXTERNAL_ENTITIES
    literal_buffer (0),
    literal_buffer_total (0),
#endif // XML_SUPPORT_EXTERNAL_ENTITIES
    pubid_literal (0),
    system_literal (0),
    in_text (FALSE),
    in_comment (FALSE),
    in_cdata (FALSE),
    skip_remaining_doctype (FALSE),
    skipped_markup_declarations (FALSE),
    has_doctype (FALSE),
    xml_validating (FALSE),
    xml_strict (TRUE),
    xml_compatibility (FALSE),
    entity_reference_tokens (FALSE),
    load_external_entities (FALSE),
    is_handling_token (FALSE),
    is_custom_error_location (FALSE),
    configuration (configuration),
    attlist_index (0),
    attlist_skip (0),
    current_source (0),
    version (XMLVERSION_1_0),
    standalone (XMLSTANDALONE_NONE),
    encoding (0),
    buffer_id (0),
    max_tokens_per_call (0),
    token_counter (0),
    nested_entity_references_allowed (configuration ? configuration->max_nested_entity_references : 0),
    is_paused (FALSE),
    is_finished (FALSE),
    token_handler_blocked (FALSE),
    has_seen_linebreak (TRUE),
#ifdef XML_VALIDATING
    validity_report (0),
#endif // XML_VALIDATING
#ifdef XML_STORE_ELEMENTS
    current_element (0),
#endif // XML_STORE_ELEMENTS
#ifdef XML_STORE_ATTRIBUTES
    current_attribute (0),
#endif // XML_STORE_ATTRIBUTES
    current_entity (0),
#ifdef XML_STORE_NOTATIONS
    current_notation (0),
#endif // XML_STORE_NOTATIONS
#ifdef XML_VALIDATING_PE_IN_GROUP
    current_peingroup (0),
#endif // XML_VALIDATING_PE_IN_GROUP
    current_contentspecgroup (0),
    blocking_source (0),
    parser (parser),
    doctype (0),
    token_handler (0),
    checking_token_handler (0),
    datasource_handler (0),
    token (parser, this),
    last_token_type (XMLToken::TYPE_Unknown),
#ifdef XML_ERRORS
    attribute_indeces (0),
    attribute_indeces_total (0),
    current_item_start (~0u),
#endif // XML_ERRORS
    delete_token_handler (FALSE),
    delete_datasource_handler (FALSE)
{
  XML_OBJECT_CREATED (XMLInternalParser);
}

XMLInternalParser::~XMLInternalParser ()
{
  XMLDoctype::DecRef (doctype);
  doctype = 0;

  Cleanup ();

  if (delete_token_handler)
    OP_DELETE(token_handler);
  if (delete_datasource_handler)
    OP_DELETE(datasource_handler);

  OP_DELETE(checking_token_handler);

#ifdef XML_SUPPORT_EXTERNAL_ENTITIES
  OP_DELETEA(literal_buffer);
#endif // XML_SUPPORT_EXTERNAL_ENTITIES

  OP_DELETEA(encoding);

  OP_DELETEA(pubid_literal);
  OP_DELETEA(system_literal);

  OP_DELETEA(normalize_buffer);

#ifdef XML_ERRORS
  OP_DELETEA(attribute_indeces);
#endif // XML_ERRORS

  XML_OBJECT_DESTROYED (XMLInternalParser);
}

#ifdef XML_DUMP_DOCTYPE

static unsigned
XML_HasKeyword (const uni_char *keywords, const char *keyword)
{
  unsigned keyword_length = op_strlen (keyword);

  while (uni_strlen (keywords) >= keyword_length)
    {
      while (*keywords && uni_strchr (UNI_L (" ,;:+"), *keywords))
        ++keywords;

      if (uni_strlen (keywords) < keyword_length)
        break;

      if (uni_strncmp (keywords, keyword, keyword_length) == 0 && (!keywords[keyword_length] || uni_strchr (UNI_L (" ,;:+"), keywords[keyword_length])))
        return 1;

      while (*keywords && !uni_strchr (UNI_L (" ,;:+"), *keywords))
        ++keywords;
    }

  return 0;
}

static int
XML_DumpDoctype (ES_Value *argv, int argc, ES_Value *return_value, DOM_Environment::CallbackSecurityInfo *security_info)
{
  return_value->type = VALUE_BOOLEAN;
  return_value->value.boolean = FALSE;

  if (argc == 3 && argv[0].type == VALUE_OBJECT && argv[1].type == VALUE_STRING && argv[2].type == VALUE_STRING)
    {
      DOM_Object *object = DOM_Utils::GetDOM_Object (argv[0].value.object);

      if (!object || !DOM_Utils::IsA (object, DOM_TYPE_DOCUMENTTYPE))
        return ES_VALUE;

      FramesDocument *document = DOM_Utils::GetDOM_Environment (object)->GetFramesDocument ();

      if (!document->GetHLDocProfile ())
        return ES_VALUE;

      XMLDoctype *doctype = document->GetLogicalDocument ()->GetXMLDocumentInfo ()->GetDoctype ();

      if (!doctype)
        return ES_VALUE;

      XMLDoctype::DumpSpecification specification;

      const uni_char *keywords = argv[2].value.string;

      specification.elements = XML_HasKeyword (keywords, "Elements");
      specification.contentspecs = XML_HasKeyword (keywords, "ContentSpecs");
      specification.attributes = XML_HasKeyword (keywords, "Attributes");
      specification.allattributes = XML_HasKeyword (keywords, "AllAttributes");
      specification.alltokenizedattributes = XML_HasKeyword (keywords, "AllTokenizedAttributes");
      specification.attributedefaults = XML_HasKeyword (keywords, "AttributeDefaults");
      specification.idattributes = XML_HasKeyword (keywords, "IdAttributes");
      specification.generalentities = XML_HasKeyword (keywords, "GeneralEntities");
      specification.parameterentities = XML_HasKeyword (keywords, "ParameterEntities");
      specification.externalentities = XML_HasKeyword (keywords, "ExternalEntities");
      specification.unparsedentities = XML_HasKeyword (keywords, "UnparsedEntities");

      doctype->DumpToFile (argv[1].value.string, specification);
      return_value->value.boolean = TRUE;
    }

  return ES_VALUE;
}

#endif // XML_DUMP_DOCTYPE

OP_STATUS
XMLInternalParser::Initialize (XMLDataSource *source)
{
  if (!source->GetBuffer ())
    {
      XMLBuffer *buffer = OP_NEW(XMLBuffer, (source, FALSE));

      if (!buffer)
        return OpStatus::ERR_NO_MEMORY;

      source->SetBuffer (buffer);

      RETURN_IF_ERROR (buffer->Initialize (configuration->preferred_literal_part_length == 0 ? 32768 : configuration->preferred_literal_part_length));
    }

  if (!source->GetParserState ())
    {
      XMLInternalParserState *state = OP_NEW(XMLInternalParserState, ());

      if (!state)
        return OpStatus::ERR_NO_MEMORY;

      source->SetParserState (state);
    }

  if (!doctype)
    {
      doctype = OP_NEW(XMLDoctype, ());

      if (!doctype)
        return OpStatus::ERR_NO_MEMORY;

      TRAPD (status, doctype->InitEntities ());
      RETURN_IF_ERROR (status);
    }

  checking_token_handler = OP_NEW(XMLCheckingTokenHandler, (this, token_handler, configuration->parse_mode == XMLParser::PARSEMODE_FRAGMENT, configuration->nsdeclaration));

  if (!checking_token_handler)
    return OpStatus::ERR_NO_MEMORY;

#ifdef XML_VALIDATING
  if (xml_validating && !xml_validating_fatal)
    {
      validity_report = OP_NEW(XMLValidityReport, ());

      if (!validity_report)
        return OpStatus::ERR_NO_MEMORY;

      source->GetBuffer ()->SetValidityReport (validity_report);
    }
#endif // XML_VALIDATING

#ifdef XML_DUMP_DOCTYPE
  static BOOL callback_registered = FALSE;

  if (!callback_registered)
    {
      RETURN_IF_ERROR (DOM_Environment::AddCallback (XML_DumpDoctype, DOM_Environment::OPERA_CALLBACK, "dumpDoctype", "-ss-"));
      callback_registered = TRUE;
    }
#endif // XML_DUMP_DOCTYPE

  return OpStatus::OK;
}

XMLInternalParser::ParseResult
XMLInternalParser::Parse (XMLDataSource *source)
{
  if (is_finished)
    return PARSE_RESULT_FINISHED;

  if (blocking_source && blocking_source != source)
    return PARSE_RESULT_WRONG_BUFFER;

  ParseResult result = PARSE_RESULT_OK;

  external_call = 0;
  token_counter = 0;
  is_paused = FALSE;
  token_handler_blocked = FALSE;

  if (configuration)
    max_tokens_per_call = configuration->max_tokens_per_call;

  current_source = source;
  current_buffer = source->GetBuffer ();
  current_state = source->GetParserState ();
  current_context = current_state->context;
  current_conditional_context = current_state->conditional_context;
  current_related_entity = current_state->entity;

  current_buffer->SetParserFields (buffer, length, index);

  while (1)
    {
      TRAPD (status, ParseInternal ());

      current_buffer->ConsumeFromDataSource ();

      result = (ParseResult) status;

      if (result != PARSE_RESULT_OK)
        {
          current_state->context = current_context;
          current_state->conditional_context = current_conditional_context;

          switch (result)
            {
            case PARSE_RESULT_END_OF_BUF:
              if (current_buffer->IsAllSeen () && !expecting_eof)
                {
                  switch (current_context)
                    {
                    case PARSE_CONTEXT_PE_BETWEEN_DECLS:
                      last_error = WELL_FORMEDNESS_ERROR_PE_between_Decls;
                      break;

                    default:
                      last_error = PARSE_ERROR_Unexpected_EOF;
                    }

#ifdef XML_ERRORS
                  is_custom_error_location = FALSE;
                  if (current_item_start != ~0u)
                    current_buffer->GetLocation (current_item_start, token_range.start);
                  current_buffer->GetLocation (length, token_range.end);
#endif // XML_ERRORS

                  Cleanup ();

                  return PARSE_RESULT_ERROR;
                }
              else if (current_buffer->IsAtEnd ())
                {
                  blocking_source = current_source->GetNextSource ();

                  if (blocking_source)
                    {
                      datasource_handler->DestroyDataSource (current_source);
                      current_source = 0;

                      if (current_context == PARSE_CONTEXT_EXTERNAL_SUBSET)
                        {
                          doctype->Finish ();

#ifdef XML_VALIDATING
                          if (xml_validating)
                            {
                              BOOL invalid = FALSE;

                              doctype->ValidateDoctype (this, invalid);

                              if (invalid && xml_validating_fatal)
                                return PARSE_RESULT_ERROR;
                            }
#endif // XML_VALIDATING

                          token.SetType (XMLToken::TYPE_DOCTYPE);
                          result = PARSE_RESULT_OK;
                          goto handle_token;
                        }

                      return PARSE_RESULT_OK;
                    }
                  else
                    {
                      token.SetType (XMLToken::TYPE_Finished);

#ifdef XML_ERRORS
                      current_buffer->GetLocation (0, token_range.start);
                      current_buffer->GetLocation (0, token_range.end);
#endif // XML_ERRORS

                      result = PARSE_RESULT_FINISHED;
                      goto handle_token;
                    }
                }
                /* fall through */

            case PARSE_RESULT_BLOCK:
              Cleanup (FALSE);
              return PARSE_RESULT_OK;

            default:
              Cleanup ();
            }

          break;
        }
    }

  return result;

handle_token:
  is_handling_token = TRUE;
  XMLTokenHandler::Result token_result = checking_token_handler->HandleToken (token);
  is_handling_token = FALSE;

  if (token_result == XMLTokenHandler::RESULT_ERROR)
    return PARSE_RESULT_ERROR;
  else if (token_result == XMLTokenHandler::RESULT_OOM)
    return PARSE_RESULT_OOM;
  else if (token_result == XMLTokenHandler::RESULT_BLOCK)
    {
      token_handler_blocked = TRUE;
      if (token.GetType () == XMLToken::TYPE_Finished)
        is_finished = TRUE;
      return PARSE_RESULT_OK;
    }
  else
    return result;
}

#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN

BOOL
XMLInternalParser::CanProcessToken ()
{
  return current_context == PARSE_CONTEXT_DOCUMENT && !in_comment && !in_cdata;
}

XMLInternalParser::ParseResult
XMLInternalParser::ProcessToken (XMLDataSource *source, XMLToken &token, BOOL &processed)
{
  OP_ASSERT (CanProcessToken ());

  external_call = 0;
  token_counter = 0;
  is_paused = FALSE;
  token_handler_blocked = FALSE;

  current_buffer = source->GetBuffer ();
  current_buffer->SetParserFields (buffer, length, index);

  if (in_text && index != length)
    {
      processed = FALSE;
      return PARSE_RESULT_OK;
    }
  else
    {
      TRAPD (status, ProcessTokenInternal (token, processed));
      return (ParseResult) status;
    }
}

#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN

OP_STATUS
XMLInternalParser::SignalInvalidEncodingError (XMLDataSource *source)
{
  current_buffer = source->GetBuffer ();
  current_buffer->SetParserFields (buffer, length, index);

  external_call = TRUE;
  HandleError (PARSE_ERROR_InvalidCharacterEncoding);
  external_call = FALSE;

  Cleanup ();
  return OpStatus::OK;
}

#ifdef XML_ERRORS

const XMLRange &
XMLInternalParser::GetErrorPosition ()
{
  if (!is_custom_error_location)
    last_error_location = token_range;

  if (last_error_location.start.IsValid () && !last_error_location.end.IsValid ())
    {
      last_error_location.end.line = last_error_location.start.line;
      last_error_location.end.column = last_error_location.start.column + 1;
    }

  return last_error_location;
}

static OP_STATUS
XML_AddErrorReportItem (XMLErrorReport *report, XMLErrorReport::Item::Type type, unsigned linenr, unsigned columnnr, const uni_char *line = 0, unsigned length = 0, unsigned start = 0, unsigned end = 0)
{
  /* If these assertions fail, the parser produced invalid ranges, or
      the data from the data source wasn't the same the second time we
      read it as it was when the parser parsed it. */

  if (line)
    {
      if (end > length)
        {
          OP_ASSERT (FALSE);
          end = length;
        }
      if (start > end)
        {
          OP_ASSERT (FALSE);
          start = end;
        }
      return report->AddItem (type, linenr, columnnr, line + start, end - start);
    }
  else
    return report->AddItem (type, linenr, columnnr);
}

OP_STATUS
XMLInternalParser::GenerateErrorReport (XMLDataSource *source, XMLErrorReport *errorreport, unsigned context_lines)
{
  const XMLRange &range1 = GetErrorPosition (), &range2 = last_related_error_location;

  OP_BOOLEAN restart_result = source->Restart ();
  RETURN_IF_ERROR(restart_result);
  if (restart_result == OpBoolean::IS_TRUE)
    {
      unsigned linenr = 0, first_line1, last_line1, first_line2, last_line2, context_lines_before, context_lines_after;
      unsigned start_column1, end_column1, start_column2, end_column2;

      if (context_lines != 0)
        {
          context_lines_before = context_lines / 2;
          context_lines_after = context_lines - context_lines_before - 1;
        }
      else
        {
          context_lines_before = ~0u;
          context_lines_after = ~0u;
        }

      if (range1.start.IsValid ())
        {
          first_line1 = range1.start.line > context_lines_before ? range1.start.line - context_lines_before : 0;
          start_column1 = range1.start.column;

          if (range1.end.IsValid ())
            {
              last_line1 = range1.end.line + context_lines_after;
              end_column1 = range1.end.column;
            }
          else
            {
              last_line1 = range1.start.line + context_lines_after;
              end_column1 = start_column1 + 1;
            }
        }
      else
        {
          first_line1 = ~0u;
          last_line1 = 0;
          start_column1 = end_column1 = ~0u;
        }

      if (range2.start.IsValid ())
        {
          first_line2 = range2.start.line > context_lines_before ? range2.start.line - context_lines_before : 0;
          start_column2 = range2.start.column;

          if (range2.end.IsValid ())
            {
              last_line2 = range2.end.line + context_lines_after;
              end_column2 = range2.end.column;
            }
          else
            {
              last_line2 = range2.start.line + context_lines_after;
              end_column2 = start_column2 + 1;
            }
        }
      else
        {
          first_line2 = ~0u;
          last_line2 = 0;
          start_column2 = end_column2 = ~0u;
        }

      BOOL last_line = FALSE;

      while (TRUE)
        {
          const uni_char *data = source->GetData (), *line = data;
          unsigned data_length = source->GetDataLength (), line_length = data_length;

          if (last_line || XMLUtils::GetLine (version, data, data_length, line, line_length))
            {
              unsigned this_line = linenr++;

              if (this_line > last_line1 && this_line > last_line2)
                break;

              if (!(this_line < first_line1 && this_line < first_line2 || this_line > last_line1 && this_line < first_line2 || this_line > last_line2 && this_line < first_line1))
                {
                  bool is_start1 = range1.start.IsValid () ? this_line == range1.start.line : false;
                  bool is_end1 = range1.end.IsValid () ? this_line == range1.end.line : is_start1;

                  bool is_start2 = range2.start.IsValid () ? this_line == range2.start.line : false;
                  bool is_end2 = range2.end.IsValid () ? this_line == range2.end.line : is_start2;

                  if (!is_start1 && !is_end1 && !is_start2 && !is_end2)
                    RETURN_IF_ERROR (XML_AddErrorReportItem (errorreport, XMLErrorReport::Item::TYPE_LINE_END, this_line, 0, line, line_length, 0, line_length));
                  else
                    {
                      unsigned consumed = 0;

                      while (TRUE)
                        {
                          XMLErrorReport::Item::Type type = XMLErrorReport::Item::TYPE_LINE_END;
                          unsigned column = line_length;

                          if (is_start1)
                            {
                              column = start_column1;
                              type = XMLErrorReport::Item::TYPE_ERROR_START;
                            }
                          else if (is_end1)
                            {
                              column = end_column1;
                              type = XMLErrorReport::Item::TYPE_ERROR_END;
                            }

                          if (is_start2 && start_column2 < column)
                            {
                              column = start_column2;
                              type = XMLErrorReport::Item::TYPE_INFORMATION_START;
                            }
                          else if (is_end2 && end_column2 <= column)
                            {
                              column = end_column2;
                              type = XMLErrorReport::Item::TYPE_INFORMATION_END;
                            }

                          if (type == XMLErrorReport::Item::TYPE_LINE_END)
                            {
                              RETURN_IF_ERROR (XML_AddErrorReportItem (errorreport, XMLErrorReport::Item::TYPE_LINE_END, this_line, consumed, line, line_length, consumed, line_length));
                              break;
                            }

                          if (consumed < column)
                            {
                              RETURN_IF_ERROR (XML_AddErrorReportItem (errorreport, XMLErrorReport::Item::TYPE_LINE_FRAGMENT, this_line, consumed, line, line_length, consumed, column));
                              consumed = column;
                            }

                          RETURN_IF_ERROR (XML_AddErrorReportItem (errorreport, type, this_line, column));

                          switch (type)
                            {
                            case XMLErrorReport::Item::TYPE_ERROR_START: is_start1 = false; break;
                            case XMLErrorReport::Item::TYPE_ERROR_END: is_end1 = false; break;
                            case XMLErrorReport::Item::TYPE_INFORMATION_START: is_start2 = false; break;
                            case XMLErrorReport::Item::TYPE_INFORMATION_END: is_end2 = false; break;
                            }
                        }
                    }
                }

              source->Consume (data - source->GetData ());

              if (!last_line)
                continue;
            }

          if (last_line)
            break;

          OP_BOOLEAN grow_result = source->Grow ();
          RETURN_IF_ERROR (grow_result);
          if (grow_result == OpBoolean::IS_FALSE)
            if (data_length != 0)
              last_line = TRUE;
            else
              break;
        }
    }

  return OpStatus::OK;
}

#endif // XML_ERRORS

BOOL
XMLInternalParser::SetLastError (ParseError error, XMLRange *location, XMLRange *related_location)
{
  last_error = error;

  if (location)
    {
      last_error_location = *location;
      is_custom_error_location = TRUE;
    }
  else
    is_custom_error_location = FALSE;

#ifdef XML_ERRORS
  if (related_location)
    last_related_error_location = *related_location;
  else
    last_related_error_location = XMLRange();
#endif // XML_ERRORS

#ifdef XML_VALIDATING
  if (error < VALIDITY_ERROR_FIRST || xml_validating_fatal)
    return TRUE;
  else
    {
      OP_ASSERT (xml_validating);

      if (validity_report)
        validity_report->AddError (error);

      return FALSE;
    }
#else // XML_VALIDATING
  return TRUE;
#endif // XML_VALIDATING
}

#ifdef XML_VALIDATING
void
XMLInternalParser::SetLastErrorLocation (XMLRange *range1, XMLRange *range2)
{
  OP_ASSERT (xml_validating);

  if (validity_report)
    {
      validity_report->AddRange (*range1);

      if (range2)
        validity_report->AddRange (*range2);
    }
}

void
XMLInternalParser::SetLastErrorData (const uni_char *datum1, unsigned datum1_length, const uni_char *datum2, unsigned datum2_length)
{
  OP_ASSERT (xml_validating);

  if (validity_report)
    {
      validity_report->AddDatum (datum1, datum1_length);

      if (datum2)
        validity_report->AddDatum (datum2, datum2_length);
    }
}
#endif // XML_VALIDATING

void
XMLInternalParser::GetLastError (ParseError &error, unsigned &line, unsigned &column, unsigned &character)
{
  error = last_error;
  line = last_error_location.start.line;
  column = last_error_location.start.column;
  character = last_error_character;
}

#ifdef XML_VALIDATING

XMLValidityReport *
XMLInternalParser::GetValidityReport ()
{
  return validity_report;
}

#endif // XML_VALIDATING

/* virtual */ BOOL
XMLInternalParser::GetLiteralIsWhitespace ()
{
  return current_buffer->GetLiteralIsWhitespace ();
}

/* virtual */ const uni_char *
XMLInternalParser::GetLiteralSimpleValue ()
{
  if (current_buffer->GetLiteralIsSimple ())
    return current_buffer->GetLiteral (FALSE);
  else
    return 0;
}

/* virtual */ uni_char *
XMLInternalParser::GetLiteralAllocatedValue ()
{
  return current_buffer->GetLiteral (TRUE);
}

unsigned
XMLInternalParser::GetLiteralLength ()
{
  return current_buffer->GetLiteralLength ();
}

/* virtual */ OP_STATUS
XMLInternalParser::GetLiteral (XMLToken::Literal &literal)
{
  unsigned parts_count = current_buffer->GetLiteralPartsCount ();

  RETURN_IF_ERROR (literal.SetPartsCount (parts_count));

  for (unsigned index = 0; index < parts_count; ++index)
    {
      uni_char *data;
      unsigned data_length;
      BOOL need_copy;

      current_buffer->GetLiteralPart (index, data, data_length, need_copy);

      RETURN_IF_ERROR (literal.SetPart (index, data, data_length, need_copy));
    }

  return OpStatus::OK;
}

/* virtual */ void
XMLInternalParser::ReleaseLiteralPart (unsigned index)
{
  current_buffer->ReleaseLiteralPart (index);
}

#ifdef XML_ERRORS

/* virtual */ BOOL
XMLInternalParser::GetTokenRange (XMLRange &range)
{
  OP_ASSERT (is_handling_token);

  switch (token.GetType ())
    {
    case XMLToken::TYPE_Comment:
    case XMLToken::TYPE_CDATA:
    case XMLToken::TYPE_Text:
      range.start = token_start;
      break;

    default:
      current_buffer->GetLocation (0, range.start, !has_seen_linebreak);
    }

  current_buffer->GetLocation (index, range.end, !has_seen_linebreak);

  return range.start.IsValid () && range.end.IsValid ();
}

/* virtual */ BOOL
XMLInternalParser::GetAttributeRange(XMLRange &range, unsigned index)
{
  OP_ASSERT (is_handling_token && (token.GetType () == XMLToken::TYPE_STag || token.GetType () == XMLToken::TYPE_EmptyElemTag || token.GetType () == XMLToken::TYPE_PI && token.GetName () == XMLExpandedName (UNI_L ("xml-stylesheet"))));

  if (index < attribute_indeces_total)
    {
      GetLocation (attribute_indeces[index * 2], range.start);
      GetLocation (attribute_indeces[index * 2 + 1], range.end);
      return TRUE;
    }
  else
    return FALSE;
}

#endif // XML_ERRORS

BOOL
XMLInternalParser::GetLiteralIsSimple ()
{
  return current_buffer->GetLiteralIsSimple ();
}

uni_char *
XMLInternalParser::GetLiteral ()
{
  return current_buffer->GetLiteral ();
}

void
XMLInternalParser::GetLiteralPart (unsigned index, uni_char *&data, unsigned &data_length, BOOL &need_copy)
{
  current_buffer->GetLiteralPart (index, data, data_length, need_copy);
}

unsigned
XMLInternalParser::GetLiteralPartsCount ()
{
  return current_buffer->GetLiteralPartsCount ();
}

#ifdef XML_ERRORS

void
XMLInternalParser::GetLocation (unsigned index, XMLPoint &point)
{
  current_buffer->GetLocation (index, point);
}

#endif // XML_ERRORS

XMLDoctype::Entity *
XMLInternalParser::GetCurrentEntity ()
{
  return current_buffer->GetCurrentEntity ();
}

void
XMLInternalParser::SetDoctype (XMLDoctype *new_doctype)
{
  doctype = new_doctype;
}

XMLDoctype *
XMLInternalParser::GetDoctype ()
{
  return doctype;
}

void
XMLInternalParser::SetTokenHandler (XMLTokenHandler *new_token_handler, BOOL new_delete_token_handler)
{
  token_handler = new_token_handler;
  delete_token_handler = new_delete_token_handler;

  if (checking_token_handler)
    checking_token_handler->SetSecondary (token_handler);
}

XMLTokenHandler *
XMLInternalParser::GetTokenHandler ()
{
  return token_handler;
}

void
XMLInternalParser::SetDataSourceHandler (XMLDataSourceHandler *new_datasource_handler, BOOL new_delete_datasource_handler)
{
  datasource_handler = new_datasource_handler;
  delete_datasource_handler = new_delete_datasource_handler;
}

XMLDataSourceHandler *
XMLInternalParser::GetDataSourceHandler ()
{
  return datasource_handler;
}

void
XMLInternalParser::SetXmlValidating (BOOL value, BOOL fatal)
{
  xml_validating = !!value;
  xml_validating_fatal = !!fatal;

  if (xml_validating)
    load_external_entities = 1;
}

void
XMLInternalParser::SetXmlStrict (BOOL value)
{
  xml_strict = !!value;
}

void
XMLInternalParser::SetXmlCompatibility (BOOL value)
{
  xml_compatibility = !!value;
}

void
XMLInternalParser::SetEntityReferenceTokens (BOOL value)
{
  entity_reference_tokens = !!value;
}

void
XMLInternalParser::SetLoadExternalEntities (BOOL value)
{
  OP_ASSERT (value || !xml_validating);
  load_external_entities = !!value;
}

BOOL
XMLInternalParser::GetXmlValidating ()
{
  return xml_validating;
}

BOOL
XMLInternalParser::GetXmlStrict ()
{
  return xml_strict;
}

BOOL
XMLInternalParser::GetXmlCompatibility ()
{
  return xml_compatibility;
}

BOOL
XMLInternalParser::GetEntityReferenceTokens ()
{
  return entity_reference_tokens;
}

#ifdef SELFTEST
/* characterclassification.ot wants to call these functions, so they
   can't be static if SELFTEST is on (and it doesn't really matter
   that they aren't, either.) */
# define XML_STATIC
#else // SELFTEST
# define XML_STATIC static
#endif // SELFTEST

XML_STATIC BOOL
XMLInternalParser_IsValidChar10 (unsigned c)
{
  if (c < 128)
    return (XMLUtils::characters[c] & XMLUtils::XML_VALID_10) != 0;
  else
    return c <= 0xd7ff || c >= 0xe000 && c <= 0xfffd || c >= 0x10000 && c <= 0x10ffff;
}

XML_STATIC BOOL
XMLInternalParser_IsValidChar11 (unsigned c)
{
  if (c < 128)
    return (XMLUtils::characters[c] & XMLUtils::XML_VALID_11) != 0;
  else
    return c <= 0xd7ff || c >= 0xe000 && c <= 0xfffd || c >= 0x10000 && c <= 0x10ffff;
}

XML_STATIC BOOL
XMLInternalParser_IsValidUnrestrictedChar11 (unsigned c)
{
  if (c < 128)
    return (XMLUtils::characters[c] & XMLUtils::XML_UNRESTRICTED_11) != 0;
  else
    return c == 0x85 || c >= 0xa0 && c <= 0xd7ff || c >= 0xe000 && c <= 0xfffd || c >= 0x10000 && c <= 0x10ffff;
}

BOOL
XMLInternalParser::IsValidChar (unsigned c)
{
  if (version == XMLVERSION_1_0)
    return XMLInternalParser_IsValidChar10 (c);
  else
    return XMLInternalParser_IsValidChar11 (c);
}

BOOL
XMLInternalParser::IsValidUnrestrictedChar (unsigned c)
{
  if (version == XMLVERSION_1_0)
    return XMLInternalParser_IsValidChar10 (c);
  else
    return XMLInternalParser_IsValidUnrestrictedChar11 (c);
}

void
XMLInternalParser::CheckValidChars (const uni_char *ptr, const uni_char *ptr_end, BOOL final)
{
  while (1)
    {
      if (version == XMLVERSION_1_0)
        while (ptr != ptr_end && XMLInternalParser_IsValidChar10 (*ptr)) ++ptr;
      else if (!current_buffer->IsInEntityReference ())
        while (ptr != ptr_end && XMLInternalParser_IsValidUnrestrictedChar11 (*ptr)) ++ptr;
      else
        while (ptr != ptr_end && XMLInternalParser_IsValidChar11 (*ptr)) ++ptr;

      if (ptr == ptr_end)
        return;
      else if (*ptr < 0xd800 || *ptr > 0xdbff)
        break;
      else if (++ptr == ptr_end)
        if (final)
          break;
        else
          return;
      else if (*ptr < 0xdc00 || *ptr > 0xdfff)
        break;
      else
        ++ptr;
    }

  unsigned error_index;

  if (ptr >= buffer && ptr < buffer + length)
    error_index = ptr - buffer;
  else
    error_index = index;

  HandleError (PARSE_ERROR_Invalid_Char, error_index);
}

/* static */ BOOL
XMLInternalParser::IsWhitespace (uni_char c)
{
  /* Returns TRUE for characters that match the S production. */
  return XMLUtils::IsSpace (c);
}

BOOL
XMLInternalParser::IsWhitespaceInMarkup (uni_char c)
{
  /* Returns TRUE for characters that would match the S production if
     after linebreak normalization (which is only done on literals and
     attribute values, not on characters in markup.) */
  if (c == 0x09 || c == 0x20)
    return TRUE;
  else if (c == 0x0a || c == 0x0d || version == XMLVERSION_1_1 && (c == 0x85 || c == 0x2028))
    {
      has_seen_linebreak = TRUE;
      return TRUE;
    }
  else
    return FALSE;
}

BOOL
XMLInternalParser::IsNameFirst (unsigned c)
{
  return XMLUtils::IsNameFirst (version, c);
}

BOOL
XMLInternalParser::IsName (unsigned c)
{
  return XMLUtils::IsName (version, c);
}

/* static */ BOOL
XMLInternalParser::IsHexDigit (uni_char c)
{
  return XMLUtils::IsHexDigit (c);
}

/* static */ BOOL
XMLInternalParser::IsDecDigit (uni_char c)
{
  return XMLUtils::IsDecDigit (c);
}

/* static */
BOOL
XMLInternalParser::CalculateCharRef (const uni_char *string, unsigned string_length, unsigned &ch_value, BOOL is_hex)
{
  ch_value = 0;

  for (const uni_char *string_end = string + string_length; string != string_end; ++string)
    if (is_hex)
      if (*string >= '0' && *string <= '9')
        ch_value = (ch_value << 4) + *string - '0';
      else if (*string >= 'A' && *string <= 'F')
        ch_value = (ch_value << 4) + *string - 'A' + 10;
      else if (*string >= 'a' && *string <= 'f')
        ch_value = (ch_value << 4) + *string - 'a' + 10;
      else
        return FALSE;
    else
      if (*string >= '0' && *string <= '9')
        ch_value = (ch_value * 10) + *string - '0';
      else
        return FALSE;

  return TRUE;
}

/* static */
void
XMLInternalParser::CopyString (uni_char *&dest, const uni_char *data, unsigned data_length)
{
  data_length = XMLLength (data, data_length);

  dest = OP_NEWA(uni_char, data_length + 1);
  if (!dest)
    LEAVE (OpStatus::ERR_NO_MEMORY);

  op_memcpy (dest, data, UNICODE_SIZE (data_length));
  dest[data_length] = 0;
}

/* static */
BOOL
XMLInternalParser::CompareStrings (const uni_char *str1, const uni_char *str2)
{
  return str1 == str2 || str1 && str2 && uni_str_eq (str1, str2);
}

/* static */
BOOL
XMLInternalParser::CompareStrings (const uni_char *str1, unsigned length1, const uni_char *str2)
{
  return str1 == str2 || str1 && str2 && length1 == uni_strlen (str2) && uni_strncmp (str1, str2, length1) == 0;
}

BOOL
XMLInternalParser::IsValidNCName (const uni_char *value, unsigned value_length)
{
  if (value_length == 0)
    return FALSE;

  const uni_char *ptr = value, *ptr_end = value + value_length;

  while (ptr != ptr_end)
    if (*ptr++ == ':')
      return FALSE;

  return TRUE;
}

BOOL
XMLInternalParser::IsValidQName (const uni_char *value, unsigned value_length)
{
  if (value_length == 0 || value[0] == ':' || value[value_length - 1] == ':')
    return FALSE;

  const uni_char *ptr = value, *ptr_end = value + value_length;
  unsigned colon = 0;

  while (ptr != ptr_end)
    if (*ptr++ == ':')
      ++colon;

  return colon == 0 || colon == 1;
}

#ifdef XML_ERRORS

void
XMLInternalParser::StoreAttributeRange (unsigned attrindex, BOOL start)
{
  if (attrindex >= attribute_indeces_total)
    {
      unsigned new_attribute_indeces_total = attribute_indeces_total == 0 ? 16 : attribute_indeces_total + attribute_indeces_total;
      unsigned *new_attribute_indeces = OP_NEWA_L(unsigned, new_attribute_indeces_total * 2);

      op_memcpy (new_attribute_indeces, attribute_indeces, attribute_indeces_total * sizeof attribute_indeces[0] * 2);

      OP_DELETEA(attribute_indeces);
      attribute_indeces = new_attribute_indeces;
      attribute_indeces_total = new_attribute_indeces_total;
    }

  attribute_indeces[attrindex * 2 + !start] = index;
}

#endif // XML_ERRORS

void
XMLInternalParser::ParseInternal ()
{
  while (1)
    {
      expecting_eof = FALSE;

#ifdef XML_ERRORS
      current_buffer->GetLocation (0, last_error_location.start);
#endif // XML_ERRORS

      switch (current_context)
        {
        case PARSE_CONTEXT_DOCUMENT:
          ReadDocument ();
          break;

        case PARSE_CONTEXT_INTERNAL_SUBSET:
          ContinueDOCTYPEToken ();
          break;

        case PARSE_CONTEXT_EXTERNAL_SUBSET_INITIAL:
        case PARSE_CONTEXT_PE_BETWEEN_DECLS_INITIAL:
          if (ReadTextDecl ())
            {
              if (!textdecl_encoding || textdecl_standalone != XMLSTANDALONE_NONE)
                HandleError (WELL_FORMEDNESS_ERROR_External_GE);

              current_source->SetEncoding (textdecl_encoding, textdecl_encoding_length);
              current_buffer->Consume ();
            }
          current_context = (ParseContext) (current_context + 1);

        case PARSE_CONTEXT_EXTERNAL_SUBSET:
        case PARSE_CONTEXT_PE_BETWEEN_DECLS:
          ReadDoctypeSubset ();
          break;

#ifdef XML_SUPPORT_EXTERNAL_ENTITIES
        case PARSE_CONTEXT_PARAMETER_ENTITY:
          ReadParameterEntity ();
          break;

        case PARSE_CONTEXT_GENERAL_ENTITY:
          ReadGeneralEntity ();
          break;
#endif // XML_SUPPORT_EXTERNAL_ENTITIES

        case PARSE_CONTEXT_SKIP:
          expecting_eof = TRUE;
          LEAVE (PARSE_RESULT_END_OF_BUF);
        }
    }
}

BOOL
XMLInternalParser::HandleToken ()
{
  last_token_type = token.GetType ();

  is_handling_token = TRUE;
  XMLTokenHandler::Result result = checking_token_handler->HandleToken (token);
  is_handling_token = FALSE;

  token.Initialize ();

  if (result == XMLTokenHandler::RESULT_OK)
    {
#ifdef XML_ERRORS
      last_error_location = XMLRange();
#endif // XML_ERRORS

      if (++token_counter == max_tokens_per_call)
        {
          is_paused = TRUE;
          token_counter = 0;
          return FALSE;
        }
      else
        return TRUE;
    }

  switch (result)
    {
    case XMLTokenHandler::RESULT_ERROR:
      LEAVE (PARSE_RESULT_ERROR);

    case XMLTokenHandler::RESULT_OOM:
      LEAVE (PARSE_RESULT_OOM);
    }

  token_handler_blocked = TRUE;
  return FALSE;
}

#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN

void
XMLInternalParser::ProcessTokenInternal (XMLToken &token0, BOOL &processed)
{
  processed = TRUE;

  if (token0.GetType () == XMLToken::TYPE_Text)
    {
      if (!in_text)
        {
          token.SetType (XMLToken::TYPE_Text);

#ifdef XML_ENTITY_REFERENCE_TOKENS
          current_buffer->LiteralStart (entity_reference_tokens);
#else // XML_ENTITY_REFERENCE_TOKENS
          current_buffer->LiteralStart (FALSE);
#endif // XML_ENTITY_REFERENCE_TOKENS

          in_text = TRUE;
        }

      const uni_char *simplevalue = token0.GetLiteralSimpleValue();
      if (simplevalue)
        current_buffer->InsertCharacterData (simplevalue, token0.GetLiteralLength ());
      else
        {
          XMLToken::Literal literal; ANCHOR (XMLToken::Literal, literal);

          LEAVE_IF_ERROR (token0.GetLiteral (literal));

          for (unsigned index = 0; index < literal.GetPartsCount (); ++index)
            current_buffer->InsertCharacterData (literal.GetPart (index), literal.GetPartLength (index));
        }
    }
  else
    {
      if (in_text)
        {
          in_text = FALSE;

#ifdef XML_ENTITY_REFERENCE_TOKENS
          current_buffer->LiteralEnd (entity_reference_tokens);
#else // XML_ENTITY_REFERENCE_TOKENS
          current_buffer->LiteralEnd (FALSE);
#endif // XML_ENTITY_REFERENCE_TOKENS

          if (current_buffer->GetLiteralLength () != 0)
            if (!HandleToken ())
              {
                processed = FALSE;
                LEAVE (PARSE_RESULT_BLOCK);
              }
        }

      is_handling_token = TRUE;
      XMLTokenHandler::Result result = checking_token_handler->HandleToken (token0, TRUE);
      is_handling_token = FALSE;

      switch (result)
        {
        case XMLTokenHandler::RESULT_ERROR:
          LEAVE (PARSE_RESULT_ERROR);

        case XMLTokenHandler::RESULT_OOM:
          LEAVE (PARSE_RESULT_OOM);

        case XMLTokenHandler::RESULT_BLOCK:
          token_handler_blocked = TRUE;
        }
    }
}

#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN
#ifdef XML_VALIDATING

void
XMLInternalParser::CheckName (XMLDoctype::Attribute::Type type, const uni_char *value, unsigned value_length)
{
  BOOL strict_first = type != XMLDoctype::Attribute::TYPE_Tokenized_NMTOKENS && type != XMLDoctype::Attribute::TYPE_Enumerated_Enumeration;

  if (value_length != 0 && (!strict_first || IsNameFirst (value[0])))
    {
      for (unsigned index = strict_first ? 1 : 0; index < value_length; ++index)
        if (!IsName (value[index]))
          {
            HandleError (XMLInternalParser::VALIDITY_ERROR_AttValue_Invalid);
            return;
          }

      return;
    }

  HandleError (XMLInternalParser::VALIDITY_ERROR_AttValue_Invalid);
}

void
XMLInternalParser::CheckNames (XMLDoctype::Attribute::Type type, const uni_char *value, unsigned value_length)
{
  if (value_length == 0)
    HandleError (XMLInternalParser::VALIDITY_ERROR_AttValue_Invalid);

  unsigned index = 0;

  while (index < value_length)
    {
      unsigned start = index;

      while (index != value_length && value[index] != 0x20) ++index;

      CheckName (type, value + start, index - start);

      ++index;
    }
}

void
XMLInternalParser::CheckAttValue (XMLDoctype::Attribute *attribute, const uni_char *value, unsigned value_length)
{
  XMLDoctype::Attribute::Type type = attribute->GetType ();

  if (type != XMLDoctype::Attribute::TYPE_String)
    switch (type)
      {
      case XMLDoctype::Attribute::TYPE_Tokenized_ID:
      case XMLDoctype::Attribute::TYPE_Tokenized_IDREF:
      case XMLDoctype::Attribute::TYPE_Tokenized_ENTITY:
      case XMLDoctype::Attribute::TYPE_Tokenized_NMTOKEN:
        CheckName (type, value, value_length);
        break;

      case XMLDoctype::Attribute::TYPE_Tokenized_IDREFS:
      case XMLDoctype::Attribute::TYPE_Tokenized_ENTITIES:
      case XMLDoctype::Attribute::TYPE_Tokenized_NMTOKENS:
        CheckNames (type, value, value_length);
        break;

      case XMLDoctype::Attribute::TYPE_Enumerated_Notation:
      case XMLDoctype::Attribute::TYPE_Enumerated_Enumeration:
        const uni_char **ptr = attribute->GetEnumerators (), **ptr_end = ptr + attribute->GetEnumeratorsCount ();
        while (ptr != ptr_end)
          if (CompareStrings (value, value_length, *ptr))
            break;
          else
            ++ptr;
        if (ptr == ptr_end)
          HandleError (XMLInternalParser::VALIDITY_ERROR_AttValue_Invalid);
      }
}

#endif // XML_VALIDATING

BOOL
XMLInternalParser::Match (const uni_char *string, unsigned string_length)
{
  while (1)
    {
      unsigned compare_length = length - index;

      if (string_length < compare_length)
        compare_length = string_length;

      if (op_memcmp (string, buffer + index, UNICODE_SIZE (compare_length)) != 0)
        return FALSE;
      else if (compare_length == string_length)
        {
          index += string_length;
          return TRUE;
        }

      if (!GrowInMarkup ())
        return FALSE;
    }
}

BOOL
XMLInternalParser::MatchFollowedByWhitespaceOrPEReference (const uni_char *string, unsigned string_length)
{
  if (Match (string, string_length))
    if (ConsumeWhitespaceAndPEReference ())
      return TRUE;
    else
      {
        index -= string_length;
        return FALSE;
      }
  else
    return FALSE;
}

BOOL
XMLInternalParser::ScanFor (uni_char ch)
{
  if (index == length)
    GrowInMarkup ();

  BOOL unproblematic = TRUE;

  while (1)
    {
      const uni_char *ptr = buffer + index, *ptr_end = buffer + length;

      while (ptr != ptr_end && *ptr != ch)
        {
          if (*ptr < 0x20 || *ptr == '&' || *ptr == '<' || *ptr >= 0x7f)
            unproblematic = FALSE;
          ++ptr;
        }

      index = ptr - buffer;

      if (ptr != ptr_end)
        return unproblematic;

      GrowInMarkup ();
    }
}

void
XMLInternalParser::ScanFor (const uni_char *string, unsigned string_length)
{
  OP_ASSERT (string_length > 1);

  unsigned start_index = index;
  uni_char first_ch = *string;
  ++string;

  while (1)
    {
      while (index + string_length > length)
        GrowInMarkup ();

      const uni_char *ptr = buffer + index, *ptr_start = ptr, *ptr_end = buffer + length - string_length + 1;

      while (1)
        {
          while (ptr != ptr_end && *ptr != first_ch) ++ptr;

          if (ptr == ptr_end)
            {
              index = ptr - buffer;

              if (in_comment || in_cdata)
                {
                  CheckValidChars (ptr_start, ptr_end, FALSE);

                  /* If the last character might be the first of a surrogate
                     pair, leave it so that we can check the whole surrogate
                     pair later. */
                  uni_char last_ch = *(ptr - 1);
                  if (last_ch >= 0xd800 && last_ch <= 0xdbff)
                    --index;

                  current_buffer->Consume ();
                }

              start_index = index;
              break;
            }

          ++ptr;

          if (op_memcmp (string, ptr, UNICODE_SIZE (string_length - 1)) == 0)
            {
              CheckValidChars (buffer + start_index, ptr, TRUE);

              index = ptr - buffer - 1;
              return;
            }
        }
    }
}

void
XMLInternalParser::ConsumeChar ()
{
	if (++index == length)
		GrowInMarkup ();
}

#ifdef XML_SUPPORT_EXTERNAL_ENTITIES

unsigned
XMLInternalParser::ConsumeCharInDoctype ()
{
  if (++index == length)
    if (!GrowInMarkup ())
      {
        unsigned consumed_entities = 0;

        while (index == length && current_buffer->IsInEntityReference ())
          {
            current_buffer->ConsumeEntityReference (FALSE);
            ++consumed_entities;
          }

        return consumed_entities;
      }

  return 0;
}

#endif // XML_SUPPORT_EXTERNAL_ENTITIES

BOOL
XMLInternalParser::ConsumeWhitespace ()
{
  if (index == length)
    GrowInMarkup ();

  if (IsWhitespaceInMarkup (buffer[index]))
    {
      ++index;

      while (1)
        {
          const uni_char *ptr = buffer + index, *ptr_end = buffer + length;

          while (ptr != ptr_end && IsWhitespaceInMarkup (*ptr))
            ++ptr;

          index = ptr - buffer;

          if (ptr == ptr_end)
            GrowInMarkup ();
          else
            return TRUE;
        }
    }

  return FALSE;
}

BOOL
XMLInternalParser::GrowBetweenMarkup ()
{
#ifdef XML_ENTITY_REFERENCE_TOKENS
  if (current_buffer->Grow (entity_reference_tokens && current_context == PARSE_CONTEXT_DOCUMENT))
#else // XML_ENTITY_REFERENCE_TOKENS
  if (current_buffer->GrowL (FALSE))
#endif // XML_ENTITY_REFERENCE_TOKENS
    {
      literal = buffer + literal_start;
      ++buffer_id;

      return TRUE;
    }
  else if (!current_buffer->IsAllSeen () && !current_buffer->IsInEntityReference ())
    LEAVE (PARSE_RESULT_END_OF_BUF);

  return FALSE;
}

BOOL
XMLInternalParser::GrowInMarkup ()
{
  if (!current_buffer->GrowL (TRUE))
    if (current_buffer->IsInEntityReference ())
      if (current_context == PARSE_CONTEXT_DOCUMENT)
        HandleError (WELL_FORMEDNESS_ERROR_GE_matches_content);
      else
        return FALSE;
    else if (current_buffer->IsAtEnd () && !expecting_eof)
      HandleError (PARSE_ERROR_Unexpected_EOF);
    else
      LEAVE (PARSE_RESULT_END_OF_BUF);

  literal = buffer + literal_start;
  ++buffer_id;

  return TRUE;
}

unsigned
XMLInternalParser::GetCurrentChar (unsigned &offset)
{
  unsigned ch = buffer[index];

  if (ch >= 0xd800 && ch <= 0xdbff)
    {
      if (index + 1 == length)
        if (!GrowInMarkup ())
          return ch;

      uni_char ch2 = buffer[index + 1];

      if (ch2 < 0xdc00 || ch2 > 0xdfff)
        {
          HandleError (PARSE_ERROR_Invalid_Char);
          return ch;
        }

      ch = 0x10000 + ((ch - 0xd800) << 10) + ch2 - 0xdc00;
      offset = 2;
    }
  else
    offset = 1;

  return ch;
}

BOOL
XMLInternalParser::ReadName ()
{
  unsigned ch, offset;

  if (!IsNameFirst (ch = GetCurrentChar (offset)))
    return FALSE;
  else
    index += offset;

  ReadNmToken ();

  literal_start -= offset;
  literal -= offset;
  literal_length += offset;

  return TRUE;
}

BOOL
XMLInternalParser::ReadNmToken ()
{
  literal_start = index;

  while (1)
    {
      unsigned ch, offset;

      while (index != length)
        if (IsName (ch = GetCurrentChar (offset)))
          index += offset;
        else
          goto end;

      if (!GrowInMarkup ())
        break;
    }

end:
  literal = buffer + literal_start;
  literal_length = index - literal_start;

  return literal_length != 0;
}

#ifdef XML_CHECK_NAMESPACE_WELL_FORMED

BOOL
XMLInternalParser::ReadNCName ()
{
#ifdef XML_ERRORS
  current_item_start = index;
#endif // XML_ERRORS

  if (ReadName ())
    if (IsValidNCName (literal, literal_length))
      {
#ifdef XML_ERRORS
        current_item_start = ~0u;
#endif // XML_ERRORS

        return TRUE;
      }
    else
      HandleError (WELL_FORMEDNESS_ERROR_NS_InvalidNCName, literal_start, literal_length);

  return FALSE;
}

BOOL
XMLInternalParser::ReadQName ()
{
#ifdef XML_ERRORS
  current_item_start = index;
#endif // XML_ERRORS

  literal_start = index;

  const uni_char *ptr = buffer + index, *ptr_end = buffer + length;
  uni_char first, last = 0;

  if ((first = *ptr) < 0x80 && (XMLUtils::characters[first] & XMLUtils::XML_NAMEFIRST) != 0 && first != ':')
    {
      ++ptr;
      while (ptr != ptr_end && (last = *ptr) < 0x80 && (XMLUtils::characters[last] & XMLUtils::XML_NAME) != 0 && last != ':') ++ptr;
      if (ptr != ptr_end && last < 0x80 && last != ':')
        {
          index = ptr - buffer;
          literal = buffer + literal_start;
          literal_length = index - literal_start;

#ifdef XML_ERRORS
          current_item_start = ~0u;
#endif // XML_ERRORS

          return TRUE;
        }
    }

  if (ReadName ())
    if (IsValidQName (literal, literal_length))
      {
#ifdef XML_ERRORS
        current_item_start = ~0u;
#endif // XML_ERRORS

        return TRUE;
      }
    else
      HandleError (WELL_FORMEDNESS_ERROR_NS_InvalidQName, literal_start, literal_length);
  return FALSE;
}

#endif // XML_CHECK_NAMESPACE_WELL_FORMED

BOOL
XMLInternalParser::ReadQuotedLiteral (BOOL &unproblematic)
{
#ifdef XML_ERRORS
  current_item_start = index;
#endif // XML_ERRORS

  uni_char quote = buffer[index];

  if (quote != '\'' && quote != '"')
    return FALSE;

  literal_start = index + 1;

#ifdef XML_SUPPORT_EXTERNAL_ENTITIES
  if (GetCurrentEntity () && (current_context == PARSE_CONTEXT_EXTERNAL_SUBSET || current_context == PARSE_CONTEXT_PE_BETWEEN_DECLS))
    {
      unproblematic = FALSE;

      OP_ASSERT (GetCurrentEntity ()->GetType () == XMLDoctype::Entity::TYPE_Parameter);

      literal_length = 0;

      unsigned quotes = 0;
      uni_char ch;

      while (1)
        {
          if ((ch = buffer[index]) != quote)
            AddToLiteralBuffer (ch);
          else if (++quotes == 2)
            break;

          unsigned consumed_entities = ConsumeCharInDoctype ();

          while (consumed_entities--)
            AddToLiteralBuffer (' ');
        }

      ConsumeCharInDoctype ();

      literal = literal_buffer;
    }
  else
#endif // XML_SUPPORT_EXTERNAL_ENTITIES
    {
      ConsumeChar ();
      unproblematic = ScanFor (quote);
      ConsumeChar ();

      literal = buffer + literal_start;
      literal_length = index - literal_start - 1;
    }

  if (!unproblematic)
    {
      CheckValidChars (literal, literal + literal_length, TRUE);
      has_seen_linebreak = TRUE;
    }

#ifdef XML_ERRORS
  current_item_start = ~0u;
#endif // XML_ERRORS

  return TRUE;
}

#ifdef XML_SUPPORT_EXTERNAL_ENTITIES

void
XMLInternalParser::AddToLiteralBuffer (uni_char ch)
{
  if (literal_length == literal_buffer_total)
    {
      unsigned new_literal_buffer_total = literal_buffer_total == 0 ? 256 : literal_buffer_total << 1;
      uni_char *new_literal_buffer = OP_NEWA_L(uni_char, new_literal_buffer_total);

      if (literal_length != 0)
        op_memcpy (new_literal_buffer, literal_buffer, UNICODE_SIZE (literal_buffer_total));

      OP_DELETEA(literal_buffer);

      literal_buffer = new_literal_buffer;
      literal_buffer_total = new_literal_buffer_total;
    }

  literal_buffer[literal_length++] = ch;
}

#endif // XML_SUPPORT_EXTERNAL_ENTITIES

BOOL
XMLInternalParser::ConsumeEntityReference (BOOL optional)
{
  MARKUP_PROFILE_PARSER ("MPa:ConsumeReference");

  reference_start = index;
  reference_entity = 0;

  ConsumeChar ();

  ParseError error;

  if (buffer[index] == '#')
    {
      error = PARSE_ERROR_Invalid_Reference_char;

      ConsumeChar ();

      is_character_reference = TRUE;

      BOOL is_hex;

      if (buffer[index] == 'x')
        {
          ConsumeChar ();
          is_hex = TRUE;
        }
      else
        is_hex = FALSE;

      unsigned value_start = index;

      if (is_hex)
        while (IsHexDigit (buffer[index]))
          ConsumeChar ();
      else
        while (IsDecDigit (buffer[index]))
          ConsumeChar ();

      unsigned value_length = index - value_start;

      if (value_length == 0)
        HandleError (error, reference_start);

      if (!CalculateCharRef (buffer + value_start, value_length, reference_character_value, is_hex) ||
          !IsValidChar (reference_character_value))
        HandleError (error, reference_start);
    }
  else
    {
      XMLDoctype::Entity::Type type;

      if (buffer[reference_start] == '&')
        {
          type = XMLDoctype::Entity::TYPE_General;
          error = PARSE_ERROR_Invalid_Reference_GE;
        }
      else
        {
          type = XMLDoctype::Entity::TYPE_Parameter;
          error = PARSE_ERROR_Invalid_Reference_PE;
        }

      is_character_reference = FALSE;

      if (optional && !IsNameFirst (buffer[index]))
        {
          index = reference_start;
          return FALSE;
        }

      if (!XML_READNCNAME ())
        HandleError (error, reference_start);

      reference_entity = doctype->GetEntity (type, literal, literal_length);

      if (current_context == PARSE_CONTEXT_DOCUMENT || current_context == PARSE_CONTEXT_INTERNAL_SUBSET)
        if ((!reference_entity || reference_entity->GetDeclaredInExternalSubset ()) &&
            (!skipped_markup_declarations || standalone == XMLSTANDALONE_YES))
          HandleError (WELL_FORMEDNESS_ERROR_Undeclared_Entity, reference_start);

      if (reference_entity)
        if (current_buffer->IsInEntityReference (reference_entity))
          HandleError (WELL_FORMEDNESS_ERROR_Recursive_Entity, reference_start);
        else if (reference_entity->GetValueType () == XMLDoctype::Entity::VALUE_TYPE_External_NDataDecl)
          HandleError (WELL_FORMEDNESS_ERROR_Unparsed_GE_in_content, reference_start);

      if (index == length)
        GrowInMarkup ();
    }

  if (buffer[index] != ';')
    HandleError (error, reference_start);

  ++index;

  if (current_context == PARSE_CONTEXT_DOCUMENT && !checking_token_handler->IsReferenceAllowed ())
    HandleError (PARSE_ERROR_Unexpected_Reference, reference_start, index - reference_start);

  reference_length = index - reference_start;
  return TRUE;
}

void
XMLInternalParser::HandleError (ParseError error, unsigned error_index, unsigned error_length)
{
#ifdef XML_ERRORS
  XMLRange range;

  if (error_index == ~0u)
    error_index = index;

  current_buffer->GetLocation (error_index, range.start);

  if (error_length != ~0u)
    current_buffer->GetLocation (error_index + error_length, range.end);

  if (SetLastError (error, &range))
#else // XML_ERRORS
  if (SetLastError (error))
#endif // XML_ERRORS
    if (!external_call)
      LEAVE (PARSE_RESULT_ERROR);
}

void
XMLInternalParser::Cleanup (BOOL cleanup_all)
{
  if (doctype)
    doctype->Cleanup ();

  OP_DELETEA(pubid_literal);
  pubid_literal = 0;
  OP_DELETEA(system_literal);
  system_literal = 0;

#ifdef XML_STORE_ELEMENTS
  OP_DELETE(current_element);
  current_element = 0;
#endif // XML_STORE_ELEMENTS
#ifdef XML_STORE_ATTRIBUTES
  OP_DELETE(current_attribute);
  current_attribute = 0;
  attlist_skip = attlist_index;
  attlist_index = 0;
#endif // XML_STORE_ATTRIBUTES
  OP_DELETE(current_entity);
  current_entity = 0;
#ifdef XML_STORE_NOTATIONS
  OP_DELETE(current_notation);
  current_notation = 0;
#endif // XML_STORE_NOTATIONS
#ifdef XML_VALIDATING_PE_IN_GROUP
  OP_DELETE(current_peingroup);
  current_peingroup = 0;
#endif // XML_VALIDATING_PE_IN_GROUP
  OP_DELETE(current_contentspecgroup);
  current_contentspecgroup = 0;

  if (cleanup_all)
    {
#ifdef XML_SUPPORT_CONDITIONAL_SECTIONS
      while (XMLConditionalContext *discarded_context = current_conditional_context)
        {
          current_conditional_context = discarded_context->next;
          OP_DELETE (discarded_context);
        }
#endif // XML_SUPPORT_CONDITIONAL_SECTIONS

      if (current_source)
        while (XMLDataSource *next_source = current_source->GetNextSource ())
          {
            datasource_handler->DestroyDataSource (current_source);
            current_source = next_source;
          }
    }
}

XMLInternalParserState::XMLInternalParserState ()
  : context (XMLInternalParser::PARSE_CONTEXT_DOCUMENT),
    entity (0),
    conditional_context (0)
{
  XML_OBJECT_CREATED (XMLInternalParserState);
}

XMLInternalParserState::~XMLInternalParserState ()
{
  XML_OBJECT_DESTROYED (XMLInternalParserState);
}

#ifdef XML_VALIDATING

XMLValidityReport::XMLValidityReport ()
  : errors (0),
    errors_count (0),
    errors_total (0),
    lines (0),
    lines_count (0),
    lines_total (0)
{
}

XMLValidityReport::~XMLValidityReport ()
{
  Error *ptr = errors, *ptr_end = ptr + errors_count;

  while (ptr != ptr_end)
    {
      OP_DELETEA(ptr->ranges);
      for (unsigned index = 0; index < ptr->data_count; ++index)
        OP_DELETEA(ptr->data[index]);
      OP_DELETEA(ptr->data);
      ++ptr;
    }

  OP_DELETEA(errors);
  for (unsigned index = 0; index < lines_count; ++index)
    OP_DELETEA(lines[index]);
  OP_DELETEA(lines);
}

void
XMLValidityReport::GetError (unsigned index, XMLInternalParser::ParseError &error0, XMLRange *&ranges, unsigned &ranges_count, const uni_char **&data, unsigned &data_count)
{
  Error &error = errors[index];

  error0 = error.error;
  ranges = error.ranges;
  ranges_count = error.ranges_count;
  data = (const uni_char **) error.data;
  data_count = error.data_count;
}

unsigned
XMLValidityReport::GetErrorsCount ()
{
  return errors_count;
}

const uni_char *
XMLValidityReport::GetLine (unsigned line)
{
  return lines[line];
}

unsigned
XMLValidityReport::GetLinesCount ()
{
  return lines_count;
}

void
XMLValidityReport::AddError (XMLInternalParser::ParseError errorcode)
{
  if (errors_count == errors_total)
    {
      unsigned new_errors_total = errors_total == 0 ? 8 : errors_total + errors_total;
      Error *new_errors = OP_NEWA_L(Error, new_errors_total);

      if (errors)
        {
          op_memcpy (new_errors, errors, errors_count * sizeof errors[0]);
          OP_DELETEA(errors);
        }

      errors = new_errors;
      errors_total = new_errors_total;
    }

  Error &error = errors[errors_count++];

  error.error = errorcode;
  error.ranges = 0;
  error.ranges_count = 0;
  error.data = 0;
  error.data_count = 0;
}

void
XMLValidityReport::AddRange (XMLRange &range)
{
  Error &error = errors[errors_count - 1];

  XMLRange *new_ranges = OP_NEWA_L(XMLRange, error.ranges_count + 1);

  if (error.ranges_count != 0)
  {
    op_memcpy (new_ranges, error.ranges, error.ranges_count * sizeof new_ranges[0]);
    OP_DELETEA(error.ranges);
  }

  error.ranges = new_ranges;
  error.ranges[error.ranges_count++] = range;
}

void
XMLValidityReport::AddDatum (const uni_char *datum, unsigned datum_length)
{
  Error &error = errors[errors_count - 1];

  uni_char **new_data = OP_NEWA_L(uni_char *, error.data_count + 1);

  if (error.data_count != 0)
  {
    op_memcpy (new_data, error.data, error.data_count * sizeof new_data[0]);
    OP_DELETEA(error.data);
  }

  error.data = new_data;
  XMLInternalParser::CopyString (error.data[error.data_count++], datum, datum_length);
}

void
XMLValidityReport::SetLine (const uni_char *data, unsigned data_length)
{
  GROW_ARRAY (lines, uni_char *);

  XMLInternalParser::CopyString (lines[lines_count], data, data_length);
  ++lines_count;
}

#endif // XML_VALIDATING

#ifdef XML_DEBUGGING

unsigned g_XMLObject_count = 0;
unsigned g_XMLInternalParser_count = 0;
unsigned g_XMLInternalParserState_count = 0;
unsigned g_XMLDoctype_count = 0;
unsigned g_XMLDoctypeAttribute_count = 0;
unsigned g_XMLDoctypeElement_count = 0;
unsigned g_XMLDoctypeEntity_count = 0;
unsigned g_XMLDoctypeNotation_count = 0;
unsigned g_XMLPEInGroup_count = 0;
unsigned g_XMLContentSpecGroup_count = 0;
unsigned g_XMLBuffer_count = 0;
unsigned g_XMLBufferState_count = 0;
unsigned g_XMLDataSource_count = 0;

class XMLInternalParserDebug
{
public:
  ~XMLInternalParserDebug ()
    {
      OP_ASSERT (g_XMLObject_count == 0);
      OP_ASSERT (g_XMLInternalParser_count == 0);
      OP_ASSERT (g_XMLInternalParserState_count == 0);
      OP_ASSERT (g_XMLDoctype_count == 0);
      OP_ASSERT (g_XMLDoctypeAttribute_count == 0);
      OP_ASSERT (g_XMLDoctypeElement_count == 0);
      OP_ASSERT (g_XMLDoctypeEntity_count == 0);
      OP_ASSERT (g_XMLDoctypeNotation_count == 0);
      OP_ASSERT (g_XMLDoctypeEntity_count == 0);
      OP_ASSERT (g_XMLPEInGroup_count == 0);
      OP_ASSERT (g_XMLContentSpecGroup_count == 0);
      OP_ASSERT (g_XMLBuffer_count == 0);
      OP_ASSERT (g_XMLBufferState_count == 0);
      OP_ASSERT (g_XMLDataSource_count == 0);
    }
} g_XMLDebug;

#endif // XML_DEBUGGING
