/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLPARSER_XMLINTERNALPARSER_H
#define XMLPARSER_XMLINTERNALPARSER_H

#include "modules/xmlparser/xmlcommon.h"
#include "modules/xmlparser/xmldoctype.h"
#include "modules/xmlparser/xmlbuffer.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltypes.h"
#include "modules/xmlutils/xmltokenbackend.h"

class TempBuffer;

class XMLParser;
class XMLInternalParser;
class XMLInternalParserState;
class XMLDataSource;
class XMLBuffer;
class XMLTokenHandler;
class XMLDataSourceHandler;
class XMLConditionalContext;
class XMLCheckingTokenHandler;
class XMLErrorReport;

#ifdef XML_VALIDATING_PE_IN_GROUP

class XMLPEInGroup
{
public:
  XMLPEInGroup (XMLPEInGroup *next, XMLDoctype::Entity *entity)
    : next (next),
      entity (entity)
    {
      XML_OBJECT_CREATED (XMLPEInGroup);
    }

  ~XMLPEInGroup ()
    {
      delete next;

      XML_OBJECT_DESTROYED (XMLPEInGroup);
    }

  XMLPEInGroup *next;
  XMLDoctype::Entity *entity;
};

#endif // XML_VALIDATING_PE_IN_GROUP
#ifdef XML_SUPPORT_CONDITIONAL_SECTIONS

class XMLConditionalContext
{
public:
  enum Type
    {
      Include, Ignore
    };

  XMLConditionalContext *next;
  Type type;
  unsigned depth;
};

#endif // XML_SUPPORT_CONDITIONAL_SECTIONS

class XMLContentSpecGroup
{
public:
  XMLContentSpecGroup (XMLContentSpecGroup *next)
    : next (next),
      type (UNDECIDED)
    {
      XML_OBJECT_CREATED (XMLContentSpecGroup);
    }

  ~XMLContentSpecGroup ()
    {
      delete next;

      XML_OBJECT_DESTROYED (XMLContentSpecGroup);
    }

  XMLContentSpecGroup *next;
  enum GroupType { UNDECIDED, SEQUENCE, CHOICE } type;
};

class XMLCurrentEntity
{
public:
  XMLDoctype::Entity *entity;
  XMLCurrentEntity *previous;
};

class XMLValidityReport;

class XMLInternalParser
  : public XMLTokenBackend
{
public:
  class ParseResultClass : public OpStatus
  {
  public:
    enum
    {
      OK = OpStatus::OK,
      ERR_NO_MEMORY = OpStatus::ERR_NO_MEMORY,
      USER_SUCCESS = OpStatus::USER_SUCCESS,
      USER_ERROR = OpStatus::USER_ERROR
    };
  };

  enum ParseResult
    {
      PARSE_RESULT_OK           = ParseResultClass::OK,
      PARSE_RESULT_FINISHED     = ParseResultClass::USER_SUCCESS + 1,
      PARSE_RESULT_OOM          = ParseResultClass::ERR_NO_MEMORY,
      PARSE_RESULT_BLOCK        = ParseResultClass::USER_ERROR + 1,
      PARSE_RESULT_WRONG_BUFFER = ParseResultClass::USER_ERROR + 2,
      PARSE_RESULT_END_OF_BUF   = ParseResultClass::USER_ERROR + 3,
      PARSE_RESULT_ERROR        = ParseResultClass::USER_ERROR + 4,
      PARSE_RESULT_FATAL        = ParseResultClass::USER_ERROR + 5
    };

  enum ParseError
    {
      PARSE_ERROR_Invalid_Syntax,
      PARSE_ERROR_Invalid_Char,
      PARSE_ERROR_Invalid_PITarget,
      PARSE_ERROR_Invalid_ExternalId,
      PARSE_ERROR_Invalid_XMLDecl,
      PARSE_ERROR_Invalid_XMLDecl_version,
      PARSE_ERROR_Invalid_XMLDecl_encoding,
      PARSE_ERROR_Invalid_XMLDecl_standalone,
      PARSE_ERROR_Invalid_SystemLiteral,
      PARSE_ERROR_Invalid_PubidLiteral,
      PARSE_ERROR_Invalid_TextDecl,
      PARSE_ERROR_Invalid_TextDecl_version,
      PARSE_ERROR_Invalid_TextDecl_encoding,
      PARSE_ERROR_Invalid_DOCTYPE,
      PARSE_ERROR_Invalid_DOCTYPE_name,
      PARSE_ERROR_Invalid_DOCTYPE_conditional,
      PARSE_ERROR_Invalid_ELEMENT,
      PARSE_ERROR_Invalid_ELEMENT_name,
      PARSE_ERROR_Invalid_ELEMENT_contentspec,
      PARSE_ERROR_Invalid_ELEMENT_contentspec_Mixed,
      PARSE_ERROR_Invalid_ELEMENT_contentspec_cp,
      PARSE_ERROR_Invalid_ELEMENT_contentspec_seq,
      PARSE_ERROR_Invalid_ATTLIST,
      PARSE_ERROR_Invalid_ATTLIST_elementname,
      PARSE_ERROR_Invalid_ATTLIST_attributename,
      PARSE_ERROR_Invalid_ATTLIST_NotationType,
      PARSE_ERROR_Invalid_ATTLIST_Enumeration,
      PARSE_ERROR_Invalid_ATTLIST_DefaultDecl,
      PARSE_ERROR_Invalid_ENTITY,
      PARSE_ERROR_Invalid_ENTITY_name,
      PARSE_ERROR_Invalid_NOTATION,
      PARSE_ERROR_Invalid_NOTATION_name,
      PARSE_ERROR_Invalid_STag,
      PARSE_ERROR_Invalid_STag_Attribute,
      PARSE_ERROR_Invalid_STag_AttValue,
      PARSE_ERROR_Invalid_ETag,
      PARSE_ERROR_Invalid_Reference_char,
      PARSE_ERROR_Invalid_Reference_GE,
      PARSE_ERROR_Invalid_Reference_PE,
      PARSE_ERROR_Invalid_XML_StyleSheet,

      PARSE_ERROR_Unexpected_EOF,
      PARSE_ERROR_Unexpected_STag,
      PARSE_ERROR_Unexpected_ETag,
      PARSE_ERROR_Unexpected_Text,
      PARSE_ERROR_Unexpected_CDATA,
      PARSE_ERROR_Unexpected_Reference,

      PARSE_ERROR_Duplicate_DOCTYPE,
      PARSE_ERROR_Mismatched_ETag,

      // If the parser was configured to refuse them.
      PARSE_ERROR_NotAllowed_XMLDeclaration,
      PARSE_ERROR_NotAllowed_DOCTYPE,

      PARSE_ERROR_InvalidCharacterEncoding,
      PARSE_ERROR_TooManyNestedEntityReferences,

      WELL_FORMEDNESS_ERROR_FIRST,

      WELL_FORMEDNESS_ERROR_PE_in_Internal_Subset = WELL_FORMEDNESS_ERROR_FIRST,
      WELL_FORMEDNESS_ERROR_GE_in_Doctype,
      WELL_FORMEDNESS_ERROR_PE_between_Decls,
      WELL_FORMEDNESS_ERROR_GE_matches_content,
      WELL_FORMEDNESS_ERROR_Undeclared_Entity,
      WELL_FORMEDNESS_ERROR_Recursive_Entity,
      WELL_FORMEDNESS_ERROR_External_GE,
      WELL_FORMEDNESS_ERROR_External_GE_in_AttValue,
      WELL_FORMEDNESS_ERROR_Unparsed_GE_in_content,
      WELL_FORMEDNESS_ERROR_UniqueAttSpec,
      WELL_FORMEDNESS_ERROR_UniqueAttSpec_XMLStylesheet,
      WELL_FORMEDNESS_ERROR_Invalid_AttValue,
      WELL_FORMEDNESS_ERROR_Invalid_AttValue_CharRef,
      WELL_FORMEDNESS_ERROR_Invalid_AttValue_Reference,
      WELL_FORMEDNESS_ERROR_Missing_Document_Element,
      WELL_FORMEDNESS_ERROR_OutOfPlace_XMLDecl,
      WELL_FORMEDNESS_ERROR_OutOfPlace_XMLStylesheet,
      WELL_FORMEDNESS_ERROR_OutOfPlace_DOCTYPE,
      WELL_FORMEDNESS_ERROR_Invalid_CDATA_End,
      WELL_FORMEDNESS_ERROR_Invalid_Comment,
      WELL_FORMEDNESS_ERROR_NS_InvalidNSDeclaration,
      WELL_FORMEDNESS_ERROR_NS_UndeclaredPrefix,
      WELL_FORMEDNESS_ERROR_NS_InvalidNCName,
      WELL_FORMEDNESS_ERROR_NS_InvalidQName,
      WELL_FORMEDNESS_ERROR_NS_ReservedPrefix,
      WELL_FORMEDNESS_ERROR_ExternalEntityHasLaterVersion,

      WELL_FORMEDNESS_ERROR_LAST,

      UNSUPPORTED_ConditionalSections,

      VALIDITY_ERROR_FIRST,

      VALIDITY_ERROR_Undeclared_Element = VALIDITY_ERROR_FIRST,
      VALIDITY_ERROR_Undeclared_Attribute,
      VALIDITY_ERROR_Undeclared_Entity,
      VALIDITY_ERROR_Undeclared_Notation,
      VALIDITY_ERROR_ProperGroupPENesting,
      VALIDITY_ERROR_PEinXMLDecl,
      VALIDITY_ERROR_ElementValid,
      VALIDITY_ERROR_RequiredAttribute,
      VALIDITY_ERROR_FixedAttributeDefault,
      VALIDITY_ERROR_ElementAlreadyDeclared,
      VALIDITY_ERROR_AttValue_Invalid,
      VALIDITY_ERROR_AttValue_EntityNotDeclared,
      VALIDITY_ERROR_AttValue_EntityNotUnparsed,
      VALIDITY_ERROR_IDAttribute_HasDefaultValue,
      VALIDITY_ERROR_IDAttribute_NotUnique,
      VALIDITY_ERROR_IDAttribute_OnePerElement,
      VALIDITY_ERROR_IDREFAttribute_NotMatched,
      VALIDITY_ERROR_StandaloneDocumentDecl_DefaultAttrValue,
      VALIDITY_ERROR_StandaloneDocumentDecl_ElementContentWhiteSpace,
      VALIDITY_ERROR_StandaloneDocumentDecl_AttrValueNormalized,
      VALIDITY_ERROR_Invalid_ELEMENT_contentspec_Mixed,
      VALIDITY_ERROR_Enumeration_NoDuplicateTokens,
      VALIDITY_ERROR_Invalid_XmlSpaceAttribute,

      COMPATIBILITY_ERROR_ContentModelDeterministic
    };

  enum ParseContext
    {
      PARSE_CONTEXT_DOCUMENT,
      PARSE_CONTEXT_INTERNAL_SUBSET,
      PARSE_CONTEXT_EXTERNAL_SUBSET_INITIAL,
      PARSE_CONTEXT_EXTERNAL_SUBSET,
      PARSE_CONTEXT_PE_BETWEEN_DECLS_INITIAL,
      PARSE_CONTEXT_PE_BETWEEN_DECLS,
      PARSE_CONTEXT_PARAMETER_ENTITY,
      PARSE_CONTEXT_GENERAL_ENTITY,
      PARSE_CONTEXT_SKIP
    };

  XMLInternalParser (XMLParser *parser, const XMLParser::Configuration *configuration);
  virtual ~XMLInternalParser ();

  OP_STATUS Initialize (XMLDataSource *source);
  ParseResult Parse (XMLDataSource *source);

#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN
  BOOL CanProcessToken ();
  ParseResult ProcessToken (XMLDataSource *source, XMLToken &token, BOOL &processed);
#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN

  OP_STATUS SignalInvalidEncodingError (XMLDataSource *source);

  BOOL IsPaused () { return is_paused; }
  BOOL TokenHandlerBlocked () { return token_handler_blocked; }

#ifdef XML_ERRORS
  const XMLRange &GetErrorPosition ();

  OP_STATUS GenerateErrorReport (XMLDataSource *source, XMLErrorReport *errorreport, unsigned context_lines);

  void GetErrorDescription(const char *&error, const char *&uri, const char *&fragment_id);
#endif // XML_ERRORS

  BOOL SetLastError (ParseError error, XMLRange *location = 0, XMLRange *relatedlocation = 0);
  void SetLastErrorLocation (XMLRange *range1, XMLRange *range2 = 0);
  void SetLastErrorData (const uni_char *datum1, unsigned datum1_length, const uni_char *datum2 = 0, unsigned datum2_length = 0);

  void GetLastError (ParseError &error, unsigned &line, unsigned &column, unsigned &character);

  XMLParser *GetXMLParser () { return parser; }

#ifdef XML_VALIDATING
  XMLValidityReport *GetValidityReport ();
#endif // XML_VALIDATING

  /* From XMLTokenBackend. */
  virtual BOOL GetLiteralIsWhitespace ();
  virtual const uni_char *GetLiteralSimpleValue ();
  virtual uni_char *GetLiteralAllocatedValue ();
  virtual unsigned GetLiteralLength ();
  virtual OP_STATUS GetLiteral (XMLToken::Literal &literal);
  virtual void ReleaseLiteralPart (unsigned index);
#ifdef XML_ERRORS
  virtual BOOL GetTokenRange(XMLRange &range);
  virtual BOOL GetAttributeRange(XMLRange &range, unsigned index);
#endif // XML_ERRORS

  BOOL GetLiteralIsSimple ();
  uni_char *GetLiteral ();

  void GetLiteralPart (unsigned index, uni_char *&data, unsigned &data_length, BOOL &need_copy);
  unsigned GetLiteralPartsCount ();

#ifdef XML_ERRORS
  void GetLocation (unsigned index, XMLPoint &point);
#endif // XML_ERRORS

  XMLDoctype::Entity *GetCurrentEntity ();

  void SetDoctype (XMLDoctype *doctype);
  XMLDoctype *GetDoctype ();

  void SetTokenHandler (XMLTokenHandler *token_handler, BOOL delete_token_handler = TRUE);
  XMLTokenHandler *GetTokenHandler ();

  void SetDataSourceHandler (XMLDataSourceHandler *datasource_handler, BOOL delete_datasource_handler = TRUE);
  XMLDataSourceHandler *GetDataSourceHandler ();

  void SetXmlValidating (BOOL value, BOOL fatal);
  void SetXmlStrict (BOOL value);
  void SetXmlCompatibility (BOOL value);
  void SetEntityReferenceTokens (BOOL value);
  void SetLoadExternalEntities (BOOL value);

  BOOL GetXmlValidating ();
  BOOL GetXmlStrict ();
  BOOL GetXmlCompatibility ();
  BOOL GetEntityReferenceTokens ();

  XMLVersion GetVersion () { return version; }
  XMLStandalone GetStandalone () { return standalone; }
  const uni_char *GetEncoding () { return encoding; }

  BOOL IsValidChar (unsigned c);
  BOOL IsValidUnrestrictedChar (unsigned c);

  void CheckValidChars (const uni_char *ptr, const uni_char *ptr_end, BOOL final);
  static BOOL IsWhitespace (uni_char c);
  BOOL IsWhitespaceInMarkup (uni_char c);
  BOOL IsNameFirst (unsigned c);
  BOOL IsName (unsigned c);
  static BOOL IsPubidChar (uni_char c);
  static BOOL IsHexDigit (uni_char c);
  static BOOL IsDecDigit (uni_char c);

  /* Note: 'string' a complete character reference, including
     leading ampersand and trailing semicolon. */
  static BOOL CalculateCharRef (const uni_char *string, unsigned string_length, unsigned &ch_value, BOOL is_hex);

  static void CopyString (uni_char *&dest, const uni_char *data, unsigned data_length);
  static BOOL CompareStrings (const uni_char *str1, const uni_char *str2);
  static BOOL CompareStrings (const uni_char *str1, unsigned length1, const uni_char *str2);

  BOOL IsValidNCName (const uni_char *value, unsigned value_length);
  BOOL IsValidQName (const uni_char *value, unsigned value_length);

  void SetMaxTokensPerCall (unsigned limit) { max_tokens_per_call = limit; }

protected:
  friend class XMLCheckingTokenHandler;

  const uni_char *buffer;
  unsigned length;
  unsigned index;

  ParseContext current_context;
  XMLConditionalContext *current_conditional_context;
  XMLBuffer *current_buffer;

  unsigned reference_start;
  unsigned reference_length;
  XMLDoctype::Entity *reference_entity;
  unsigned reference_character_value;

  uni_char *normalize_buffer, normalize_last_ch;
  unsigned normalize_buffer_count;
  unsigned normalize_buffer_total;
  XMLCurrentEntity *normalize_current_entity;

  const uni_char *literal;
  unsigned literal_start;
  unsigned literal_length;
#ifdef XML_SUPPORT_EXTERNAL_ENTITIES
  uni_char *literal_buffer;
  unsigned literal_buffer_total;
#endif // XML_SUPPORT_EXTERNAL_ENTITIES

  uni_char *pubid_literal;
  uni_char *system_literal;

  unsigned in_text:1;
  unsigned in_comment:1;
  unsigned in_cdata:1;
  unsigned expecting_eof:1;
  unsigned is_character_reference:1;
  unsigned skip_remaining_doctype:1;
  unsigned skipped_markup_declarations:1;
  unsigned has_doctype:1;
  unsigned attribute_is_cdata:1;
  unsigned attribute_ws_normalized:1;

  unsigned xml_validating:1;
  unsigned xml_validating_fatal:1;
  unsigned xml_strict:1;
  unsigned xml_compatibility:1;
  unsigned entity_reference_tokens:1;
  unsigned load_external_entities:1;

  unsigned external_call:1;
  unsigned is_handling_token:1;
  unsigned is_custom_error_location:1;

  const XMLParser::Configuration *configuration;

  unsigned attlist_index;
  unsigned attlist_skip;

  XMLDataSource *current_source;
  XMLInternalParserState *current_state;
  XMLDoctype::Entity *current_related_entity;

  XMLVersion version;
  XMLStandalone standalone;
  uni_char *encoding;

  XMLVersion textdecl_version;
  XMLStandalone textdecl_standalone;
  const uni_char *textdecl_encoding;
  unsigned textdecl_encoding_start;
  unsigned textdecl_encoding_length;

  unsigned buffer_id;
  unsigned max_tokens_per_call, token_counter, nested_entity_references_allowed;
  BOOL is_paused;
  BOOL is_finished;
  BOOL token_handler_blocked;
  BOOL has_seen_linebreak;

#ifdef XML_VALIDATING
  XMLValidityReport *validity_report;
  XMLRange attribute_range;
#endif // XML_VALIDATING

  /* Automatically cleaned up on errors. */
#ifdef XML_STORE_ELEMENTS
  XMLDoctype::Element *current_element;
#endif // XML_STORE_ELEMENTS
#ifdef XML_STORE_ATTRIBUTES
  XMLDoctype::Attribute *current_attribute;
#endif // XML_STORE_ATTRIBUTES
  XMLDoctype::Entity *current_entity;
#ifdef XML_STORE_NOTATIONS
  XMLDoctype::Notation *current_notation;
#endif // XML_STORE_NOTATIONS
#ifdef XML_VALIDATING_PE_IN_GROUP
  XMLPEInGroup *current_peingroup;
#endif // XML_VALIDATING_PE_IN_GROUP
  XMLContentSpecGroup *current_contentspecgroup;

#ifdef XML_VALIDATING_PE_IN_MARKUP_DECL
  XMLDoctype::Entity *containing_entity;
#endif // XML_VALIDATING_PE_IN_MARKUP_DECL

  XMLDataSource *blocking_source;

  XMLParser *parser;
  XMLDoctype *doctype;
  XMLTokenHandler *token_handler;
  XMLCheckingTokenHandler *checking_token_handler;
  XMLDataSourceHandler *datasource_handler;

  XMLToken token;
  XMLToken::Type last_token_type;

  ParseError last_error;
  XMLRange last_error_location;
  unsigned last_error_character;

  TempBuffer pi_data;

#ifdef XML_ERRORS
  XMLRange token_range, last_related_error_location;
  XMLPoint token_start;
  unsigned *attribute_indeces;
  unsigned attribute_indeces_total;
  unsigned current_item_start;

  void StoreAttributeRange (unsigned attrindex, BOOL start);
#endif // XML_ERRORS

  BOOL delete_token_handler;
  BOOL delete_datasource_handler;

  void ParseInternal ();
  BOOL HandleToken ();
  /**< Will LEAVE if the token handler signalled a fatal error. */

#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN
  void ProcessTokenInternal (XMLToken &token, BOOL &processed);
#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN

#ifdef XML_VALIDATING
  void CheckName (XMLDoctype::Attribute::Type type, const uni_char *value, unsigned value_length);
  void CheckNames (XMLDoctype::Attribute::Type type, const uni_char *value, unsigned value_length);
  void CheckAttValue (XMLDoctype::Attribute *attribute, const uni_char *value, unsigned value_length);
#endif // XML_VALIDATING

  void HandleError (ParseError error, unsigned index = ~0u, unsigned length = ~0u);
  /**< Will LEAVE if the error is fatal (that is, unless the error is a
       validity error and validity errors are considered non-fatal.) */

  BOOL GrowBetweenMarkup ();
  BOOL GrowInMarkup ();

  unsigned GetCurrentChar (unsigned &offset);

  BOOL Match (const uni_char *string, unsigned string_length);
  BOOL MatchFollowedByWhitespaceOrPEReference (const uni_char *string, unsigned string_length);

  void ScanFor (const uni_char *string, unsigned string_length);
  BOOL ScanFor (uni_char ch);

#ifdef XML_SUPPORT_EXTERNAL_ENTITIES
  unsigned ConsumeCharInDoctype ();
  void AddToLiteralBuffer (uni_char ch);
#endif // XML_SUPPORT_EXTERNAL_ENTITIES

  void ConsumeChar ();
  BOOL ConsumeWhitespace ();
  BOOL ConsumeWhitespaceAndPEReference ();
  void ConsumeName ();

  BOOL ConsumeEntityReference (BOOL optional = FALSE);

  BOOL ReadName ();
  BOOL ReadNmToken ();

#ifdef XML_CHECK_NAMESPACE_WELL_FORMED
  BOOL ReadNCName ();
  BOOL ReadQName ();
# define XML_READNCNAME ReadNCName
# define XML_READQNAME ReadQName
#else // XML_CHECK_NAMESPACE_WELL_FORMED
# define XML_READNCNAME ReadName
# define XML_READQNAME ReadName
#endif // XML_CHECK_NAMESPACE_WELL_FORMED

  BOOL ReadQuotedLiteral (BOOL &unproblematic);

  void ReadDocument ();
  void ReadDoctypeSubset ();
  void ReadParameterEntity ();
  void ReadGeneralEntity ();

  void ReadPIToken (BOOL in_doctype = FALSE);
  void ReadCommentToken (BOOL in_doctype = FALSE);
  void ReadCDATAToken ();
  void ReadDOCTYPEToken ();
  void ContinueDOCTYPEToken ();
  void FinishDOCTYPEToken ();
  void ReadSTagToken ();
  void ReadETagToken ();
  BOOL ReadTextToken ();
  BOOL ReadReference ();
  void ProcessAttribute (const uni_char *name, unsigned name_length, BOOL need_normalize, unsigned attrindex = 0);
  void ReadAttributes ();

  BOOL ReadTextDecl (BOOL xmldecl = FALSE);
  void ReadELEMENTDecl ();
  void ReadATTLISTDecl ();
  void ReadENTITYDecl ();
  void ReadNOTATIONDecl ();

#ifdef XML_STORE_ATTRIBUTES
  void AddAttribute ();
#endif // XML_STORE_ATTRIBUTES
#ifdef XML_STORE_ELEMENTS
  void AddElement ();
#endif // XML_STORE_ELEMENTS
  void AddEntity ();
#ifdef XML_STORE_NOTATIONS
  void AddNotation ();
#endif // XML_STORE_NOTATIONS

  void ReadConditionalSection ();
  void ReadIgnoreSection ();

  void Cleanup (BOOL cleanup_all = TRUE);

  void ReadExternalIdProduction (BOOL accept_missing, BOOL accept_missing_system);
  void ReadPEReference ();

  void NormalizeReset ();
  void NormalizeGrow (unsigned added_count);
  void NormalizeAddChar (uni_char ch);
  void NormalizePushChar (uni_char ch);
  BOOL NormalizePushString (const uni_char *string, unsigned string_length, BOOL is_attribute, BOOL normalize_linebreaks);
  BOOL NormalizeCharRef (const uni_char *value, unsigned start_index, unsigned end_index);
  BOOL Normalize (const uni_char *value, unsigned value_length, BOOL is_attribute, BOOL normalize_linebreaks, unsigned error_index = ~0u, unsigned error_length = ~0u);
  BOOL NormalizeAttributeValue (const uni_char *name, unsigned name_length, BOOL unproblematic);
#ifdef XML_STORE_ATTRIBUTES
  XMLDoctype::Attribute *normalize_attribute;
#endif // XML_STORE_ATTRIBUTES

  uni_char *GetNormalizedString ();
  unsigned GetNormalizedStringLength ();

  void LoadEntity (XMLDoctype::Entity *entity, ParseContext context);
  void StartEntityWellFormednessCheck ();
  BOOL EndEntityWellFormednessCheck ();

#ifdef XML_ERRORS
  static const char *GetErrorStringInternal (XMLInternalParser::ParseError error);
#endif // XML_ERRORS
};

class XMLInternalParserState
{
public:
  XMLInternalParserState ();
  ~XMLInternalParserState ();

  XMLInternalParser::ParseContext context;
  XMLDoctype::Entity *entity;
  XMLConditionalContext *conditional_context;
};

#ifdef XML_VALIDATING

class XMLValidityReport
{
public:
  XMLValidityReport ();
  ~XMLValidityReport ();

  void GetError (unsigned index, XMLInternalParser::ParseError &error, XMLRange *&ranges, unsigned &ranges_count, const uni_char **&data, unsigned &data_count);
  unsigned GetErrorsCount ();

  const uni_char *GetLine (unsigned line);
  unsigned GetLinesCount ();

  void AddError (XMLInternalParser::ParseError error);
  void AddRange (XMLRange &range);
  void AddDatum (const uni_char *datum, unsigned datum_length);

  void SetLine (const uni_char *data, unsigned data_length);

protected:
  class Error
  {
  public:
    XMLInternalParser::ParseError error;
    XMLRange *ranges;
    unsigned ranges_count;
    uni_char **data;
    unsigned data_count;
  };

  Error *errors;
  unsigned errors_count, errors_total;

  uni_char **lines;
  unsigned lines_count, lines_total;
};

#endif // XML_VALIDATING
#endif // XMLPARSER_XMLINTERNALPARSER_H
