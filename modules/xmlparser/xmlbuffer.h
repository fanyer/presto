/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLPARSER_XMLBUFFER_H
#define XMLPARSER_XMLBUFFER_H

#include "modules/xmlparser/xmldoctype.h"

class XMLDataSource;
class XMLValidityReport;

class XMLBuffer
{
public:
  XMLBuffer (XMLDataSource *source, BOOL is_xml_1_1);
  ~XMLBuffer ();

  OP_STATUS Initialize (unsigned literalsize);
  /**< Initializes the buffer and sets the sizes of the work buffer and
       literal parts.  Can only be called once per buffer. */

  void SetIsXML11 ();
  /**< The document is parsed as XML 1.1. */

  void Consume (BOOL leave_entity_reference = FALSE, BOOL no_linebreaks = FALSE);
  /**< Consume the first 'length' characters as the parser sees it. */

  void ConsumeEntityReference (BOOL consume);

  BOOL GrowL (BOOL leave_entity_reference);
  /**< Make more data available for the parser.  May change the values of the
       parser's 'buffer', 'index', 'length' and 'offset' fields.  Returns TRUE
       if more data is available after the call.
       Leaves if out of memory */

  void ConsumeFromDataSource ();
  /**< Consume all internally consumed data from the external data source. */

  BOOL IsAtEnd ();
  /**< Returns TRUE if all data has been processed. */

  BOOL IsAllSeen ();
  /**< Returns TRUE if all data has seen (if the current parser buffer
       contains all remaining data.) */

  void ExpandCharacterReference (unsigned length, unsigned character);
  /**< Expand a character reference at 'index', replacing 'length' characters.  The
       'index' should be local (that is, relative the parser's 'buffer'
       pointer.) */

  void ExpandEntityReference (unsigned length, XMLDoctype::Entity *entity);
  /**< Expand a entity reference at 'index', replacing 'length' characters.
       The 'index' should be local (that is, relative the parser's 'buffer'
       pointer.)  If 'add_padding' is TRUE, a pair of space characters are
       added surrounding the expanded text.  If 'signal_end' is TRUE the end
       of the reference will be signal by simulating an end-of-buffer
       situation after which IsAtEntityEnd returns TRUE. */

  BOOL CheckEntityAt (unsigned start, unsigned end, unsigned &line, unsigned &column, XMLDoctype::Entity *&entity);
  /**< Check that the range ['start', 'end') does not cross the start or end
       of an expanded entity reference.  The indeces 'start' and 'end should
       be global.  If the function returns FALSE, 'line' and 'column' will
       have been set to the location of the offending entity expansion and
       'entity' will have been set to the offending entity. */

  BOOL IsInEntityReference (XMLDoctype::Entity *entity);
  /**< Returns TRUE if the character at 'index' comes from an expanded
       reference to 'entity'.  Used to check for recursive entities.  The
       'index' should be local (that is, relative the parser's 'buffer'
       pointer.) */

  BOOL IsInEntityReference () { return current_state != first_state; }
  /**< Returns TRUE if parser's 'buffer' is the replacement text of an entity. */

  XMLDoctype::Entity *GetCurrentEntity ();
  /**< Returns current entity (valid when IsInEntityReference returns TRUE.) */

  void SetParserFields (const uni_char *&buffer, unsigned &index, unsigned &length);
  /**< Set the parser's 'buffer', 'length', 'index' and 'offset' fields.
       Updates the value's of the fields as well as stores pointers to them
       so that they can be updated by other functions.  They are updated by
       Consume, Flush, Grow and ExpandEntity. */

  void LiteralStart (BOOL leave_entity_reference);
  /**< Mark the start of literal data. */

  void LiteralEnd (BOOL leave_entity_reference);
  /**< Mark the end of literal data. */

  BOOL GetLiteralIsWhitespace ();
  /**< Returns TRUE if the current literal matches the 'S' production. */

  BOOL GetLiteralIsSimple ();
  /**< Returns TRUE if the current literal is contained entirely in the work
       buffer.  If it is, the pointer returned by GetLiteral points directly
       into the work buffer and needs to be copied.  If not, the pointer
       returned by GetLiteral is allocated and needs to be freed by the caller
       (using delete[].) */

  uni_char *GetLiteral (BOOL copy = TRUE);
  /**< Returns the current literal.  See GetLiteralIsSimple. */

  unsigned GetLiteralLength ();
  /**< Returns the length of the current literal. */

  void GetLiteralPart (unsigned index, uni_char *&data, unsigned &data_length, BOOL &need_copy);
  /**< Retrieves the 'index'th part of the current literal.  The
       'data' and 'data_length' arguments will be set to the part's
       contents and length, respectively.  The 'need_copy' argument is
       set to FALSE if the caller can take over ownership of the part
       by calling ReleaseLiteralPart with the same index. */

  void ReleaseLiteralPart (unsigned index);
  /**< Releases the 'index'th part of the current literal, so that it
       is not reused and not freed by the buffer later. */

  unsigned GetLiteralPartsCount ();
  /**< Returns the number of parts in the current literal.  Each part can be
       retrieved using the GetLiteralPart function. */

  void SetCopyBuffer (TempBuffer *buffer);

  void NormalizeLinebreaks (uni_char *buffer, unsigned &length);
  /**< Normalize linebreaks in the text in 'buffer'.  'Buffer' is not null
       terminated; the first 'length' characters are used.  Upon return,
       'length' will have been adjusted to exclude any linebreak characters that
       were normalized away.  */

#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN
  void InsertCharacterData (const uni_char *data, unsigned data_length);
#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN

#ifdef XML_VALIDATING
  void SetValidityReport (XMLValidityReport *validity_report);
#endif // XML_VALIDATING

#ifdef XML_ERRORS
  void GetLocation (unsigned index, XMLPoint &point, BOOL no_linebreaks = FALSE);
  /**< Gets the line and column (in the base data source) of the character
       with the global index 'index'. */
#endif // XML_ERRORS

protected:
  class State
  {
  public:
    const uni_char *buffer;

    unsigned index;
    unsigned length;
    unsigned literal_start;
    unsigned literal_end;
    unsigned consumed;
    unsigned consumed_before;

    XMLDoctype::Entity *entity;
    /**< The referenced entity. */

    State *previous_state;
    State *next_state;

#ifdef XML_ERRORS
    unsigned line;
    /**< Line number of the first character of the entity reference. */

    unsigned column;
    /**< Column number of the first character of the entity reference. */
#endif // XML_ERRORS
  };

  void DiscardState ();
  void DeleteStates (State *state);
  void CopyFromParserFields ();
  void CopyToParserFields ();
  void FlushToLiteral ();
  void CopyToLiteral (const uni_char *data, unsigned data_length, BOOL normalize_linebreaks);
  uni_char *AddLiteralBuffer ();

  State *NewState ();

#ifdef XML_VALIDATING
  void CopyToLineBuffer (const uni_char *data, unsigned data_length);
#endif // XML_VALIDATING

#ifdef XML_ERRORS
  void UpdateLine (const uni_char *data, unsigned data_length);
#endif // XML_ERRORS

  State *current_state;
  /**< Innermost current entity expansion. */

  State *first_state;
  /**< Outermost current entity expansion. */

  BOOL in_literal;

  uni_char **literal_buffers;
  unsigned literal_copied;
  unsigned literal_buffers_used, literal_buffers_count, literal_buffers_total;
  unsigned literal_buffer_offset, literal_buffer_size;

  const uni_char **parser_buffer;
  /**< Pointer to the parser's 'buffer' field. */

  unsigned *parser_index;
  /**< Pointer to the parser's 'index' field. */

  unsigned *parser_length;
  /**< Pointer to the parser's 'length' field. */

  XMLDataSource *data_source;
  /**< Base data source. */

  BOOL is_xml_1_1;
  /**< TRUE if the document is parsed as XML 1.1. */

  BOOL at_end;
  /**< TRUE if IsAtEnd returnes TRUE.  See IsAtEnd. */

  BOOL last_was_carriage_return;
  /**< Last character copied into the literal buffer was a CARRIAGE
       RETURN that was converted to a LINE FEED as part of normal
       line-break normalization. */

#ifdef XML_VALIDATING
  BOOL is_validating;

  uni_char *line_buffer;
  unsigned line_buffer_offset, line_buffer_size;

  XMLValidityReport *validity_report;
#endif // XML_VALIDATING

#ifdef XML_ERRORS
  unsigned line;
  /**< Line (in the base data source) of the first non-consumed character from
       the data source. */

  unsigned column;
  /**< Column (in the base data source) of the first non-consumed character from
       the data source. */

  unsigned character;
  /**< Index (in the base data source) of the first non-consumed character from
       the data source. */

  uni_char previous_ch;
  /**< The previous character from the base data source (essentially
       'data_source->GetData ()[base_offset-1]', except base_offset might be
       zero and the character flushed.) */

  unsigned cached_index, cached_line, cached_column;
#endif // XML_ERRORS

  TempBuffer *copy_buffer;

  State *free_state;
};

#endif // XMLPARSER_XMLBUFFER_H
