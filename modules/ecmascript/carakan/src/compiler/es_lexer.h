/* -*- mode: c++ -*- */

#ifndef ES_LEXER_H
#define ES_LEXER_H


class ES_Token
{
public:
  enum Type
    {
      INVALID,
      RESERVED,
      LINEBREAK,
      LITERAL,
      PUNCTUATOR,
      KEYWORD,
      IDENTIFIER,
      END
    };

  enum Punctuator
    {
      NOT_A_PUNCTUATOR,

      /* One character long. */
      LEFT_BRACE,
      RIGHT_BRACE,
      LEFT_PAREN,
      RIGHT_PAREN,
      LEFT_BRACKET,
      RIGHT_BRACKET,
      PERIOD,
      SEMICOLON,
      COMMA,
      LESS_THAN,
      GREATER_THAN,
      LOGICAL_NOT,
      BITWISE_NOT,
      QUESTION_MARK,
      CONDITIONAL_TRUE = QUESTION_MARK,
      COLON,
      CONDITIONAL_FALSE = COLON,
      ASSIGN,
      ADD,
      SUBTRACT,
      MULTIPLY,
      DIVIDE,
      REMAINDER,
      BITWISE_AND,
      BITWISE_OR,
      BITWISE_XOR,

      /* Two characters long. */
      LESS_THAN_OR_EQUAL,
      GREATER_THAN_OR_EQUAL,
      EQUAL,
      NOT_EQUAL,
      INCREMENT,
      DECREMENT,
      LOGICAL_AND,
      LOGICAL_OR,
      SHIFT_LEFT,
      SHIFT_SIGNED_RIGHT,
      ADD_ASSIGN,
      SUBTRACT_ASSIGN,
      MULTIPLY_ASSIGN,
      DIVIDE_ASSIGN,
      REMAINDER_ASSIGN,
      BITWISE_AND_ASSIGN,
      BITWISE_OR_ASSIGN,
      BITWISE_XOR_ASSIGN,

      /* Not punctuators, but anyway. */
      SINGLE_LINE_COMMENT,
      MULTI_LINE_COMMENT,

      /* Three characters long. */
      STRICT_EQUAL,
      STRICT_NOT_EQUAL,
      SHIFT_UNSIGNED_RIGHT,
      SHIFT_LEFT_ASSIGN,
      SHIFT_SIGNED_RIGHT_ASSIGN,

      /* Four characters long. */
      SHIFT_UNSIGNED_RIGHT_ASSIGN,

      PUNCTUATORS_COUNT
    };

  enum Keyword
    {
      KEYWORD_BREAK,
      KEYWORD_CASE,
      KEYWORD_CATCH,
      KEYWORD_CONTINUE,
      KEYWORD_DEBUGGER,
      KEYWORD_DEFAULT,
      KEYWORD_DELETE,
      KEYWORD_DO,
      KEYWORD_ELSE,
      KEYWORD_FALSE,
      KEYWORD_FINALLY,
      KEYWORD_FOR,
      KEYWORD_FUNCTION,
      KEYWORD_IF,
      KEYWORD_IN,
      KEYWORD_INSTANCEOF,
      KEYWORD_NEW,
      KEYWORD_NULL,
      KEYWORD_RETURN,
      KEYWORD_SWITCH,
      KEYWORD_THIS,
      KEYWORD_THROW,
      KEYWORD_TRUE,
      KEYWORD_TRY,
      KEYWORD_TYPEOF,
      KEYWORD_VAR,
      KEYWORD_VOID,
      KEYWORD_WHILE,
      KEYWORD_WITH,

      KEYWORD_FUTURE_RESERVED,
      KEYWORDS_COUNT
    };

  Type type;

#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
  unsigned start;
  unsigned end;

  unsigned line;
  unsigned column;

  ES_SourceLocation  GetSourceLocation() const { return ES_SourceLocation(start, line, end-start); }
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT

  ES_Value_Internal value;
  JString *identifier;
  Punctuator punctuator;
  Keyword keyword;
  ES_RegExp_Information *regexp;
  JString *regexp_source;
  bool contained_escapes;
};


class ES_Fragments
{
public:
  ES_Fragments (const uni_char **source, unsigned *source_length)
    : fragments (source),
      fragment_lengths (source_length),
      fragments_count (1),
      length (0)
  {
  }

  ES_Fragments ()
  {
  }

  unsigned GetFragmentsCount () { return fragments_count; }
  void GetFragment (unsigned index, const uni_char *&fragment, unsigned &length) { fragment = fragments[index]; length = fragment_lengths[index]; }

  const uni_char **fragments;
  unsigned *fragment_lengths;
  unsigned fragments_count;
  unsigned length;
};

class ES_RegExp_Information;
class OpMemGroup;

class ES_Parser;

class ES_Lexer
{
public:
  ES_Lexer (ES_Context *context, OpMemGroup *arena = NULL, ES_Fragments *source = NULL, JString *base = NULL, unsigned document_line = 1, unsigned document_line_start = 0);

  ES_Fragments *GetSource () { return source; }
  void SetSource (ES_Fragments *new_source, JString *new_base = NULL);
  void SetSkipLinebreaks (BOOL value) { skip_linebreaks = value; }
  void SetIsStrictMode (BOOL value) { is_strict_mode = value; }
  void SetEmitComments (BOOL value) { emit_comments = value; }
  /**< Emit comment tokens. */

  void SetStringsTable (ES_Identifier_List *list) { strings_table = list; }
  void SetLongStringLiteralTable (ES_Identifier_Hash_Table *table) { long_string_literal_table = table; }

  void NextToken ();
  void RevertTokenRegexp ();
  void RevertTokenIdentifier ();
  ES_Token &GetToken ();

  JString *GetSourceString ();
  JStringStorage *GetSourceStringStorage ();

  const char *GetTokenAsCString ();
  JString *GetTokenAsJString (bool add_quotes = true);

  unsigned GetDocumentLine() const { return document_line; }
  unsigned GetDocumentColumn() const { return document_column; }

  static unsigned GetColumn(const uni_char *source, unsigned index);
  /**< Get the column for the character at @c index. */

  inline static BOOL IsWhitespace (int ch);
  static BOOL IsLinebreak (int ch);
  static BOOL IsPunctuatorChar (int ch);
  static BOOL IsContinuedPunctuatorChar (int ch);
  static BOOL IsDecimalDigit (int ch);
  static BOOL IsHexDigit (int ch);
  static BOOL IsOctalDigit (int ch);
  static BOOL IsNonzeroDigit (int ch);
  inline static BOOL IsIdentifierStart (int ch);
  inline static BOOL IsIdentifierPart (int ch);

  static unsigned DecimalDigitValue (int ch);
  static unsigned HexDigitValue (int ch);

  ES_Token::Keyword KeywordFromIdentifier (const uni_char *name, unsigned length);

  static JString *ProcessStringLiteral(ES_Context *context, const uni_char *literal, unsigned length, BOOL is_strict_mode);

  void SetArena(OpMemGroup *a) { current_arena = a; }
  void SetParser(ES_Parser *p) { owner_parser = p; }

  JString *MakeSharedString (JString *str);

  class State
  {
  public:
    State (ES_Lexer *lexer)
      : lexer (lexer),
        fragment_number (lexer->fragment_number),
        fragment_index (lexer->fragment_index),
#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
        line_number (lexer->line_number),
        line_start (lexer->line_start),
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT
        start_of_line (lexer->start_of_line)
    {
    }

    void Restore ()
    {
      lexer->fragment_number = fragment_number;
      lexer->fragment_index = fragment_index;
#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
      lexer->line_number = line_number;
      lexer->line_start = line_start;
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT
      lexer->start_of_line = start_of_line;
    }

  private:
    friend class ES_Lexer;
    ES_Lexer *lexer;
    unsigned fragment_number, fragment_index;
#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
    unsigned line_number, line_start;
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT
    BOOL start_of_line;
  };

private:
  friend class State;

  BOOL NextTokenInternal ();

  static BOOL IsWhitespaceSlow (int ch);
  static BOOL IsIdentifierPartSlow (int ch);

  BOOL MatchDecimalLiteral ();
  BOOL MatchHexLiteral ();
  BOOL MatchOctalLiteral (unsigned &radix);

  BOOL SingleLineComment ();
  BOOL MultiLineComment ();
  BOOL NumericLiteral (unsigned radix);
  BOOL StringLiteral ();
  BOOL RegexpLiteral ();
  BOOL Identifier ();
  BOOL HandlePunctuator (int ch);

  BOOL HandleCharEscape(uni_char &ch);
  BOOL HandleEscape (BOOL in_literal);
  BOOL HandleLinebreak (BOOL optional = FALSE);
  void SetInvalid ();

  JString *MakeString (const uni_char *data, unsigned length, BOOL simple, unsigned hash = 0);

  void HandleError ();

  inline void StartToken ();

  BOOL NextChar (BOOL optional = FALSE)
  {
    if (++fragment_index < fragment_length)
      {
        character = fragment[fragment_index];
        return TRUE;
      }
    else
      return NextCharSlow(optional);
  }
  BOOL NextCharSlow (BOOL optional = FALSE);

  inline BOOL IsLookingAt (const char *string, unsigned string_length);
  BOOL IsLookingAtSlow (const char *string, unsigned string_length);
  BOOL IsLookingAt (BOOL (*fun)(int), unsigned start, unsigned length);
  void ResetBuffer ();
  void ResetExpand ();
  BOOL Expand ();

  ES_Context *context;
  ES_Identifier_List *strings_table;
  ES_Identifier_Hash_Table *long_string_literal_table;

  ES_Fragments *source;
  JString *base;
  BOOL skip_linebreaks;
  BOOL is_strict_mode;
  BOOL emit_comments;

  int character;

  const uni_char *fragment;
  unsigned fragment_index, fragment_length, fragment_offset;
  unsigned fragment_number, fragments_count;
  unsigned expand_fragment_index, expand_fragment_number;

  ES_TempBuffer buffer;

  const uni_char *identifier;
  unsigned identifier_length;
  BOOL simple_identifier;

  unsigned start_index;

#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
  unsigned line_number, line_start;
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT
  unsigned document_line, document_column;

  BOOL start_of_line;

  ES_Token token;

  const char *error_string, *eof_context;
  const char *regexp_error_string;
  char error_string_buffer[56]; // widened to 56 to fix CARAKAN-813

  unsigned error_index, error_line, error_column;

  ES_Fragments local_source;
  const uni_char *local_source_string;
  unsigned local_source_length;

#ifdef ES_DEBUG__DUMP_TOKENS
  static const char *punctuator_names[ES_Token::PUNCTUATORS_COUNT];
#endif // ES_DEBUG__DUMP_TOKENS

  ES_Parser *owner_parser;

  OpMemGroup *current_arena;

  OpMemGroup *Arena () { return current_arena; }
};


inline BOOL
ES_Lexer::IsDecimalDigit (int ch)
{
  return ch >= '0' && ch <= '9';
}


inline BOOL
ES_Lexer::IsOctalDigit (int ch)
{
  return ch >= '0' && ch <= '7';
}


inline BOOL
ES_Lexer::IsHexDigit (int ch)
{
  return ((ch >= '0' && ch <= '9') ||
          (ch >= 'A' && ch <= 'F') ||
          (ch >= 'a' && ch <= 'f'));
}


inline BOOL
ES_Lexer::IsLinebreak (int ch)
{
  switch (ch)
    {
    case 0x000A: // LINE FEED
    case 0x000D: // CARRIAGE RETURN
    case 0x2028: // LINE SEPARATOR
    case 0x2029: // PARAGRAPH SEPARATOR
      return TRUE;
    }
  return FALSE;
}


inline ES_Token &
ES_Lexer::GetToken ()
{
  return token;
}


inline BOOL
ES_Lexer::IsWhitespace (int ch)
{
  if (ch == 32)
    return TRUE;
  else if (ch < 0xA0)
    return ch >= 9 && ch <= 13;
  else
    return IsWhitespaceSlow (ch);
}


inline BOOL
ES_Lexer::IsIdentifierStart (int ch)
{
  return ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z' || ch == '$' || ch == '_' || (ch >= 128 && Unicode::IsAlpha(ch));
}


inline BOOL
ES_Lexer::IsIdentifierPart (int ch)
{
  return ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z' || ch >= '0' && ch <= '9' || ch == '$' || ch == '_' || (ch >= 128 && IsIdentifierPartSlow (ch));
}


inline BOOL
ES_Lexer::IsLookingAt (const char *string, unsigned string_length)
{
  if (fragment_index + string_length <= fragment_length)
    {
      for (unsigned i = fragment_index, j = 0; j < string_length; ++i, ++j)
        if (fragment[i] != string[j])
          return FALSE;

      return TRUE;
    }
  else
    return IsLookingAtSlow (string, string_length);
}


inline unsigned
ES_Lexer::HexDigitValue (int ch)
{
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  else if (ch >= 'A' && ch <= 'F')
    return 10 + ch - 'A';
  else
    return 10 + ch - 'a';
}


#endif // ES_LEXER_H
