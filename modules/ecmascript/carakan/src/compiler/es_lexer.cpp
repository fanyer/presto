/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) Opera Software ASA  2008-2010
 *
 * @author Jens Lindstrom
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/util/es_tempbuf.h"
#include "modules/ecmascript/carakan/src/compiler/es_lexer.h"
#include "modules/ecmascript/carakan/src/compiler/es_parser.h"
#include "modules/ecmascript/carakan/src/object/es_regexp_object.h"
#include "modules/regexp/include/regexp_advanced_api.h"

#define NEXT_CHAR() do { if (!NextChar ()) return FALSE; } while (0)
#define NEXT_CHAR_OPTIONAL() NextChar (TRUE)


ES_Lexer::ES_Lexer (ES_Context *context, OpMemGroup *arena, ES_Fragments *source, JString *base, unsigned document_line, unsigned document_column)
  : context (context),
    strings_table (NULL),
    long_string_literal_table (NULL),
    source (NULL),
    base (NULL),
    skip_linebreaks (FALSE),
    is_strict_mode (FALSE),
    emit_comments (FALSE),
    fragment_index (0),
    fragment_offset (0),
    fragment_number (0),
    document_line(document_line),
    document_column(document_column),
    owner_parser (NULL),
    current_arena (arena)
{
  if (source)
    SetSource (source, base);

  start_of_line = TRUE;
}


void
ES_Lexer::SetSource (ES_Fragments *new_source, JString *new_base)
{
  source = new_source;
  base = new_base;
  fragments_count = source->GetFragmentsCount ();
  source->GetFragment (0, fragment, fragment_length);
  source->length = 0;
  for (unsigned index = 0; index < source->fragments_count; ++index)
    source->length += source->fragment_lengths[index];
  fragment_index = 0;
  fragment_offset = 0;
  fragment_number = 0;
#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
  line_number = 1;
  line_start = 0;
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT
}


void
ES_Lexer::NextToken ()
{
  NextTokenInternal ();

  if (token.type != ES_Token::INVALID)
    {
      if (token.type == ES_Token::IDENTIFIER)
        {
          simple_identifier = identifier != NULL;

          if (!simple_identifier)
            {
              identifier = buffer.GetStorage ();
              identifier_length = buffer.Length ();
            }

          ES_Token::Keyword keyword = KeywordFromIdentifier (identifier, identifier_length);

          if (keyword == ES_Token::KEYWORD_FUTURE_RESERVED)
            {
              token.type = ES_Token::RESERVED;

              error_string = "reserved word used";
              HandleError ();
            }
          else if (keyword != ES_Token::KEYWORDS_COUNT)
            {
              token.type = ES_Token::LITERAL;

              if (keyword == ES_Token::KEYWORD_TRUE)
                token.value.SetTrue ();
              else if (keyword == ES_Token::KEYWORD_FALSE)
                token.value.SetFalse ();
              else if (keyword == ES_Token::KEYWORD_NULL)
                token.value.SetNull ();
              else
                {
                  token.type = ES_Token::KEYWORD;
                  token.keyword = keyword;
                }
            }
          else
            {
              token.type = ES_Token::IDENTIFIER;
              token.identifier = MakeString (identifier, identifier_length, simple_identifier);
            }
        }

#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
      token.end = fragment_offset + fragment_index;

#ifdef _DEBUG
      OP_ASSERT (token.type != ES_Token::END && token.start < source->length || token.type == ES_Token::END /* && token.start == source->length */);
      OP_ASSERT (token.end <= source->length);
#endif // _DEBUG
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT
    }
  else
    HandleError ();
}

BOOL
ES_Lexer::NextTokenInternal ()
{
  token.type = ES_Token::END;

  OP_ASSERT(fragment_index <= fragment_length);
  if (fragment_index == fragment_length)
    {
#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
      token.start = fragment_offset + fragment_index;
      token.line = line_number;
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT

      return TRUE;
    }

  character = fragment[fragment_index];

  int ch;

  if (skip_linebreaks)
    while (IsWhitespace (ch = character))
      {
        if (!NEXT_CHAR_OPTIONAL ())
          return TRUE;
      }
  else
    {
      while (IsWhitespace (ch = character))
        if (IsLinebreak (ch))
          {
            StartToken ();
            token.type = ES_Token::LINEBREAK;
            if (!HandleLinebreak (TRUE))
              {
                token.type = ES_Token::END;
                return TRUE;
              }
          }
        else if (!NEXT_CHAR_OPTIONAL ())
          return TRUE;

      if (token.type == ES_Token::LINEBREAK)
        return TRUE;
    }

  if (start_of_line && IsLookingAt("-->", 3))
    {
      StartToken ();
      SingleLineComment ();
      return TRUE;
    }

  start_of_line = FALSE;

  SetInvalid ();
  StartToken ();

  unsigned radix;

  if (ch == '\'' || ch == '"')
    return StringLiteral ();
  else if (IsIdentifierStart (ch) || ch == '\\')
    return Identifier ();

  if (ch == '0')
    if (IsLookingAt ("0x", 2) || IsLookingAt ("0X", 2))
      if (MatchHexLiteral ())
        return NumericLiteral (16);
      else
        return FALSE;
    else if (IsLookingAt ("0", 1) && !(IsLookingAt ("0e", 2) || IsLookingAt ("0E", 2) || IsLookingAt ("0.", 2)))
      if (MatchOctalLiteral (radix))
        if (is_strict_mode && radix == 8)
          return FALSE;
        else
          return NumericLiteral (radix);
      else
        return FALSE;

  if (MatchDecimalLiteral ())
    return NumericLiteral (10);
  else if (HandlePunctuator (ch))
    return TRUE;
  /* HandlePunctuator will always set token.type to PUNCTUATOR and just return
     FALSE if it turns out there wasn't a punctuator there.  But it may also
     redirect to SingleLineComment() or MultiLineComment(), and those will set
     token.type to INVALID if the comment (or the following token) is invalid,
     and this check here is to avoid overwriting the error message with a
     generic one in those cases. */
  else if (token.type == ES_Token::INVALID)
    return FALSE;
  else
    {
      SetInvalid ();

      error_string = error_string_buffer;

      op_sprintf (error_string_buffer, "unexpected character: U+%.4X", (unsigned) ch);

      HandleError ();

      return FALSE;
    }
}


void
ES_Lexer::RevertTokenRegexp ()
{
  if (token.type == ES_Token::PUNCTUATOR)
    {
      ES_Token::Punctuator p = token.punctuator;

      if (p == ES_Token::DIVIDE || p == ES_Token::DIVIDE_ASSIGN)
        {
          ResetBuffer ();

          if (p == ES_Token::DIVIDE_ASSIGN)
            buffer.AppendL ('=');

          --fragment_index;

          if (NEXT_CHAR_OPTIONAL ())
            {
              TRAPD (error, RegexpLiteral ());

              if (OpStatus::IsMemoryError (error))
                LEAVE (error);

#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
              token.end = fragment_offset + fragment_index;
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT
              if (token.type == ES_Token::INVALID)
                {
                  SetInvalid ();
                  HandleError ();
                }
            }
        }
    }
}


void
ES_Lexer::RevertTokenIdentifier ()
{
  OP_ASSERT (token.type == ES_Token::RESERVED || token.type == ES_Token::LITERAL || token.type == ES_Token::KEYWORD);
  OP_ASSERT (identifier);

  token.type = ES_Token::IDENTIFIER;
  token.identifier = MakeString (identifier, identifier_length, simple_identifier);
}


JString *
ES_Lexer::GetSourceString ()
{
  if (!base)
    {
      base = JString::Make (context, source->length);

      uni_char *storage = Storage (context, base);

      for (unsigned index = 0; index < source->fragments_count; ++index)
        {
          unsigned length = source->fragment_lengths[index];
          op_memcpy (storage, source->fragments[index], length * sizeof (uni_char));
          storage += length;
        }

      *storage = 0;
    }

  return base;
}


JStringStorage *
ES_Lexer::GetSourceStringStorage ()
{
  JStringStorage *OP_MEMORY_VAR ss = JStringStorage::MakeStatic (context, source->length);
  uni_char *storage = ss->storage;

  for (unsigned index = 0; index < source->fragments_count; ++index)
    {
      unsigned length = source->fragment_lengths[index];
      op_memcpy (storage, source->fragments[index], length * sizeof (uni_char));
      storage += length;
    }

  JString *new_base = NULL;

  TRAPD (status, new_base = JString::Make (context, ss, 0, source->length));
  if (OpStatus::IsMemoryError (status))
    {
      op_free (ss);
      LEAVE (OpStatus::ERR_NO_MEMORY);
      return NULL;
    }

  local_source_string = Storage(context, new_base);
  local_source_length = source->length;

  local_source.fragments = &local_source_string;
  local_source.fragment_lengths = &local_source_length;
  local_source.fragments_count = 1;

  SetSource (&local_source, new_base);

  return ss;
}


const char *
ES_Lexer::GetTokenAsCString ()
{
  switch (token.type)
    {
    case ES_Token::END:
      return "<end of file>";

    case ES_Token::LINEBREAK:
      return "<linebreak>";

    case ES_Token::PUNCTUATOR:
      switch (token.punctuator)
        {
        case ES_Token::LEFT_BRACE: return "'{'";
        case ES_Token::RIGHT_BRACE: return "'}'";
        case ES_Token::LEFT_PAREN: return "'('";
        case ES_Token::RIGHT_PAREN: return "')'";
        case ES_Token::LEFT_BRACKET: return "'['";
        case ES_Token::RIGHT_BRACKET: return "']'";
        case ES_Token::PERIOD: return "'.'";
        case ES_Token::SEMICOLON: return "';'";
        case ES_Token::COMMA: return "','";
        case ES_Token::LESS_THAN: return "'<'";
        case ES_Token::GREATER_THAN: return "'>'";
        case ES_Token::LOGICAL_NOT: return "'!'";
        case ES_Token::BITWISE_NOT: return "'~'";
        case ES_Token::CONDITIONAL_TRUE: return "'?'";
        case ES_Token::CONDITIONAL_FALSE: return "':'";
        case ES_Token::ASSIGN: return "'='";
        case ES_Token::ADD: return "'+'";
        case ES_Token::SUBTRACT: return "'-'";
        case ES_Token::MULTIPLY: return "'*'";
        case ES_Token::DIVIDE: return "'/'";
        case ES_Token::REMAINDER: return "'%'";
        case ES_Token::BITWISE_AND: return "'&'";
        case ES_Token::BITWISE_OR: return "'|'";
        case ES_Token::BITWISE_XOR: return "'^'";
        case ES_Token::LESS_THAN_OR_EQUAL: return "'<='";
        case ES_Token::GREATER_THAN_OR_EQUAL: return "'>='";
        case ES_Token::EQUAL: return "'=='";
        case ES_Token::NOT_EQUAL: return "'!='";
        case ES_Token::INCREMENT: return "'++'";
        case ES_Token::DECREMENT: return "'--'";
        case ES_Token::LOGICAL_AND: return "'&&'";
        case ES_Token::LOGICAL_OR: return "'||'";
        case ES_Token::SHIFT_LEFT: return "'<<'";
        case ES_Token::SHIFT_SIGNED_RIGHT: return "'>>'";
        case ES_Token::ADD_ASSIGN: return "'+='";
        case ES_Token::SUBTRACT_ASSIGN: return "'-='";
        case ES_Token::MULTIPLY_ASSIGN: return "'*='";
        case ES_Token::DIVIDE_ASSIGN: return "'/='";
        case ES_Token::REMAINDER_ASSIGN: return "'%='";
        case ES_Token::BITWISE_AND_ASSIGN: return "'&='";
        case ES_Token::BITWISE_OR_ASSIGN: return "'|='";
        case ES_Token::BITWISE_XOR_ASSIGN: return "'^='";
        case ES_Token::SINGLE_LINE_COMMENT: return "<comment>";
        case ES_Token::MULTI_LINE_COMMENT: return "<comment>";
        case ES_Token::STRICT_EQUAL: return "'==='";
        case ES_Token::STRICT_NOT_EQUAL: return "'!=='";
        case ES_Token::SHIFT_UNSIGNED_RIGHT: return "'>>>'";
        case ES_Token::SHIFT_LEFT_ASSIGN: return "'<<='";
        case ES_Token::SHIFT_SIGNED_RIGHT_ASSIGN: return "'>>='";
        case ES_Token::SHIFT_UNSIGNED_RIGHT_ASSIGN: return "'>>>='";
        }
      break;

    case ES_Token::KEYWORD:
      switch (token.keyword)
        {
        case ES_Token::KEYWORD_BREAK: return "keyword 'break'";
        case ES_Token::KEYWORD_CASE: return "keyword 'case'";
        case ES_Token::KEYWORD_CATCH: return "keyword 'catch'";
        case ES_Token::KEYWORD_CONTINUE: return "keyword 'continue'";
        case ES_Token::KEYWORD_DEBUGGER: return "keyword 'debugger'";
        case ES_Token::KEYWORD_DEFAULT: return "keyword 'default'";
        case ES_Token::KEYWORD_DELETE: return "keyword 'delete'";
        case ES_Token::KEYWORD_DO: return "keyword 'do'";
        case ES_Token::KEYWORD_ELSE: return "keyword 'else'";
        case ES_Token::KEYWORD_FALSE: return "keyword 'false'";
        case ES_Token::KEYWORD_FINALLY: return "keyword 'finally'";
        case ES_Token::KEYWORD_FOR: return "keyword 'for'";
        case ES_Token::KEYWORD_FUNCTION: return "keyword 'function'";
        case ES_Token::KEYWORD_IF: return "keyword 'if'";
        case ES_Token::KEYWORD_IN: return "keyword 'in'";
        case ES_Token::KEYWORD_INSTANCEOF: return "keyword 'instanceof'";
        case ES_Token::KEYWORD_NEW: return "keyword 'new'";
        case ES_Token::KEYWORD_NULL: return "keyword 'null'";
        case ES_Token::KEYWORD_RETURN: return "keyword 'return'";
        case ES_Token::KEYWORD_SWITCH: return "keyword 'switch'";
        case ES_Token::KEYWORD_THIS: return "keyword 'this'";
        case ES_Token::KEYWORD_THROW: return "keyword 'throw'";
        case ES_Token::KEYWORD_TRUE: return "keyword 'true'";
        case ES_Token::KEYWORD_TRY: return "keyword 'try'";
        case ES_Token::KEYWORD_TYPEOF: return "keyword 'typeof'";
        case ES_Token::KEYWORD_VAR: return "keyword 'var'";
        case ES_Token::KEYWORD_VOID: return "keyword 'void'";
        case ES_Token::KEYWORD_WHILE: return "keyword 'while'";
        case ES_Token::KEYWORD_WITH: return "keyword 'with'";
        }
      break;

    case ES_Token::LITERAL:
      if (token.value.IsString ())
        return "<string>";
      else if (token.value.IsBoolean ())
        return token.value.GetBoolean () ? "'true'" : "'false'";
      else if (token.value.IsNull ())
        return "'null'";
      else if (token.value.IsUndefined ())
        return "'undefined'";
      else
        return NULL;
      break;

    case ES_Token::IDENTIFIER:
    case ES_Token::RESERVED:
      return NULL;

    default:
      return "<invalid token>";
    }

  return NULL;
}


JString *
ES_Lexer::GetTokenAsJString (bool add_quotes)
{
  unsigned prefix;

  if (token.type == ES_Token::RESERVED)
    prefix = 14;
  else
    prefix = 0;

  JString *result = JString::Make (context, prefix + (add_quotes ? 2 : 0) + (token.end - token.start));
  uni_char *ptr = Storage (context, result);

  if (token.type == ES_Token::RESERVED)
    {
      op_memcpy (ptr, UNI_L ("reserved word "), UNICODE_SIZE (14));
      ptr += 14;
    }

  if (add_quotes)
    *ptr++ = '\'';

  for (unsigned index = token.start, length = token.end - token.start, fi = 0; length != 0; ++fi)
    {
      unsigned fl = source->fragment_lengths[fi];

      if (index < fl)
        {
          unsigned local_length = es_minu (length, fl - index);

          op_memcpy (ptr, source->fragments[fi] + index, local_length * sizeof (uni_char));

          index = 0;
          length -= local_length;
          ptr += local_length;
        }
      else
        index -= fl;
    }

  if (add_quotes)
    *ptr++ = '\'';

  return result;
}


/* static */ unsigned
ES_Lexer::GetColumn(const uni_char *source, unsigned index)
{
  unsigned column = 0;

  for (unsigned ci = index - 1; ci != UINT_MAX; --ci)
      if (ES_Lexer::IsLinebreak(source[ci]))
          break;
      else
          ++column;

  return column;
}


BOOL
ES_Lexer::IsWhitespaceSlow (int ch)
{
  switch (ch)
    {
    case 0x00A0: // NO-BREAK SPACE
    case 0x200B: // ZERO WIDTH SPACE
    case 0xFEFF: // BYTE ORDER MARK
    case 0xFFFE: // 0xFFEF in wrong byte-order
      return TRUE;
    default:
      return uni_isspace(ch);
    }
}


BOOL
ES_Lexer::IsPunctuatorChar (int ch)
{
  return op_strchr ("{}()[].;,<>=!+-*/%&|^~?:", ch) != 0;
}


BOOL
ES_Lexer::IsContinuedPunctuatorChar (int ch)
{
  return op_strchr ("=<>+-&|/*", ch) != 0;
}


BOOL
ES_Lexer::IsIdentifierPartSlow (int ch)
{
  switch (Unicode::GetCharacterClass (ch))
    {
    case CC_Ll:
    case CC_Lm:
    case CC_Lo:
    case CC_Lt:
    case CC_Lu:
    case CC_Mc:
    case CC_Mn:
    case CC_Nd:
    case CC_Pc:
      return TRUE;

    /* cf. ES5.1, Section 7.1 (Unicode Format-Control Characters.) */
    case CC_Cf:
      return (ch == 0x200c || ch == 0x200d);

    default:
      return FALSE;
    }
}


unsigned
ES_Lexer::DecimalDigitValue (int ch)
{
  return ch - '0';
}


static BOOL
IsIdentifier (const uni_char *name, unsigned name_length, const char *rest, unsigned total_length)
{
  if (name_length == total_length)
    {
      while (--total_length)
        if (*name++ != *rest++)
          return FALSE;
      return TRUE;
    }
  return FALSE;
}


ES_Token::Keyword
ES_Lexer::KeywordFromIdentifier (const uni_char *name, unsigned length)
{
  switch (*name++)
    {
    case 'b':
      if (IsIdentifier (name, length, "reak", 5))
        return ES_Token::KEYWORD_BREAK;
      else
        break;

    case 'c':
      if (IsIdentifier (name, length, "ase", 4))
        return ES_Token::KEYWORD_CASE;
      else if (IsIdentifier (name, length, "atch", 5))
        return ES_Token::KEYWORD_CATCH;
      else if (IsIdentifier (name, length, "lass", 5))
        return ES_Token::KEYWORD_FUTURE_RESERVED;
      else if (IsIdentifier (name, length, "ontinue", 8))
        return ES_Token::KEYWORD_CONTINUE;
      else if (IsIdentifier (name, length, "onst", 5))
        if (is_strict_mode)
          return ES_Token::KEYWORD_FUTURE_RESERVED;
        else
          return ES_Token::KEYWORD_VAR;
      else
        break;

    case 'd':
      if (IsIdentifier (name, length, "efault", 7))
        return ES_Token::KEYWORD_DEFAULT;
      else if (IsIdentifier (name, length, "elete", 6))
        return ES_Token::KEYWORD_DELETE;
      else if (name[0] == 'o' && length == 2)
        return ES_Token::KEYWORD_DO;
      else if (IsIdentifier (name, length, "ebugger", 8))
        return ES_Token::KEYWORD_DEBUGGER;
      else
        break;

    case 'e':
      if (IsIdentifier (name, length, "lse", 4))
        return ES_Token::KEYWORD_ELSE;
      else if (IsIdentifier (name, length, "num", 4))
        return ES_Token::KEYWORD_FUTURE_RESERVED;
      else if (IsIdentifier (name, length, "xport", 6))
        return ES_Token::KEYWORD_FUTURE_RESERVED;
      else if (IsIdentifier (name, length, "xtends", 7))
        return ES_Token::KEYWORD_FUTURE_RESERVED;
      else
        break;

    case 'f':
      if (IsIdentifier (name, length, "alse", 5))
        return ES_Token::KEYWORD_FALSE;
      else if (name[0] == 'o' && name[1] == 'r' && length == 3)
        return ES_Token::KEYWORD_FOR;
      else if (IsIdentifier (name, length, "unction", 8))
        return ES_Token::KEYWORD_FUNCTION;
      else if (IsIdentifier (name, length, "inally", 7))
        return ES_Token::KEYWORD_FINALLY;
      else
        break;

    case 'i':
      if (name[0] == 'f' && length == 2)
        return ES_Token::KEYWORD_IF;
      else if (name[0] == 'n' && length == 2)
        return ES_Token::KEYWORD_IN;
      else if (IsIdentifier (name, length, "nstanceof", 10))
        return ES_Token::KEYWORD_INSTANCEOF;
      else if (IsIdentifier (name, length, "mport", 6))
        return ES_Token::KEYWORD_FUTURE_RESERVED;
      else if (is_strict_mode)
        if (IsIdentifier (name, length, "mplements", 10))
          return ES_Token::KEYWORD_FUTURE_RESERVED;
        else if (IsIdentifier (name, length, "nterface", 9))
          return ES_Token::KEYWORD_FUTURE_RESERVED;
      break;

    case 'l':
      if (is_strict_mode && IsIdentifier (name, length, "et", 3))
        return ES_Token::KEYWORD_FUTURE_RESERVED;
      else
        break;

    case 'n':
      if (name[0] == 'e' && name[1] == 'w' && length == 3)
        return ES_Token::KEYWORD_NEW;
      else if (IsIdentifier (name, length, "ull", 4))
        return ES_Token::KEYWORD_NULL;
      else
        break;

    case 'p':
      if (is_strict_mode)
        if (IsIdentifier (name, length, "ackage", 7))
          return ES_Token::KEYWORD_FUTURE_RESERVED;
        else if (IsIdentifier (name, length, "rivate", 7))
          return ES_Token::KEYWORD_FUTURE_RESERVED;
        else if (IsIdentifier (name, length, "rotected", 9))
          return ES_Token::KEYWORD_FUTURE_RESERVED;
        else if (IsIdentifier (name, length, "ublic", 6))
          return ES_Token::KEYWORD_FUTURE_RESERVED;
      break;

    case 'r':
      if (IsIdentifier (name, length, "eturn", 6))
        return ES_Token::KEYWORD_RETURN;
      else
        break;

    case 's':
      if (IsIdentifier (name, length, "witch", 6))
        return ES_Token::KEYWORD_SWITCH;
      else if (IsIdentifier (name, length, "uper", 5))
        return ES_Token::KEYWORD_FUTURE_RESERVED;
      else if (is_strict_mode && IsIdentifier (name, length, "tatic", 6))
        return ES_Token::KEYWORD_FUTURE_RESERVED;
      else
        break;

    case 't':
      if (IsIdentifier (name, length, "his", 4))
        return ES_Token::KEYWORD_THIS;
      else if (IsIdentifier (name, length, "hrow", 5))
        return ES_Token::KEYWORD_THROW;
      else if (IsIdentifier (name, length, "rue", 4))
        return ES_Token::KEYWORD_TRUE;
      else if (name[0] == 'r' && name[1] == 'y' && length == 3)
        return ES_Token::KEYWORD_TRY;
      else if (IsIdentifier (name, length, "ypeof", 6))
        return ES_Token::KEYWORD_TYPEOF;
      else
        break;

    case 'v':
      if (name[0] == 'a' && name[1] == 'r' && length == 3)
        return ES_Token::KEYWORD_VAR;
      else if (IsIdentifier (name, length, "oid", 4))
        return ES_Token::KEYWORD_VOID;
      else
        break;

    case 'w':
      if (IsIdentifier (name, length, "hile", 5))
        return ES_Token::KEYWORD_WHILE;
      else if (IsIdentifier (name, length, "ith", 4))
        return ES_Token::KEYWORD_WITH;
      else
        break;

    case 'y':
      if (is_strict_mode && IsIdentifier (name, length, "ield", 5))
        return ES_Token::KEYWORD_FUTURE_RESERVED;
      break;
    }

  return ES_Token::KEYWORDS_COUNT;
}


BOOL
ES_Lexer::MatchDecimalLiteral ()
{
  unsigned previous_fi = fragment_index, previous_fn = fragment_number;

  while (IsDecimalDigit (character))
    {
      buffer.AppendL (character);
      if (!NEXT_CHAR_OPTIONAL ())
        return TRUE;
    }

  if (character == '.')
    {
      buffer.AppendL (character);

      if (NEXT_CHAR_OPTIONAL ())
        {
          if (IsDecimalDigit (character))
            do
              {
                buffer.AppendL (character);
                if (!NEXT_CHAR_OPTIONAL ())
                  return TRUE;
              }
            while (IsDecimalDigit (character));
          else if (buffer.Length () == 1)
            goto restore_and_return_false;
        }
      else
        if (buffer.Length () > 1)
          return TRUE;
        else
          goto restore_and_return_false;
    }

  if (buffer.Length () != 0)
    {
      if (character == 'e' || character == 'E')
        {
          eof_context = "in numeric literal";

          buffer.AppendL (character);
          NEXT_CHAR ();

          if (character == '+' || character == '-')
            {
              buffer.AppendL (character);
              NEXT_CHAR ();
            }

          if (IsDecimalDigit (character))
            {
              do
                {
                  buffer.AppendL (character);
                  if (!NEXT_CHAR_OPTIONAL ())
                    return TRUE;
                }
              while (IsDecimalDigit (character));
            }
          else
            goto restore_and_return_false;
        }

      if (!IsIdentifierPart (character))
        return TRUE;
    }
  else
    return FALSE;

 restore_and_return_false:
  fragment_index = previous_fi;

  if (fragment_number != previous_fn)
    do
      {
        source->GetFragment (--fragment_number, fragment, fragment_length);
        fragment_offset -= fragment_length;
      }
    while (fragment_number != previous_fn);

  character = fragment[fragment_index];

  return FALSE;
}


BOOL
ES_Lexer::MatchHexLiteral ()
{
  eof_context = "in numeric literal";

  NEXT_CHAR ();
  NEXT_CHAR ();

  if (IsHexDigit (character))
    {
      do
        {
          buffer.AppendL (character);
          if (!NEXT_CHAR_OPTIONAL ())
            return TRUE;
        }
      while (IsHexDigit (character));

      if (IsIdentifierPart (character))
        {
          SetInvalid ();

          op_sprintf (error_string_buffer, "invalid character after numeric literal: '%c'", character);
          error_string = error_string_buffer;

          HandleError ();

          return FALSE;
        }
    }
  else
    {
      SetInvalid ();

      op_sprintf (error_string_buffer, "invalid character after numeric literal: '%c'", character);
      error_string = error_string_buffer;

      HandleError ();

      return FALSE;
    }

  return TRUE;
}


BOOL
ES_Lexer::MatchOctalLiteral (unsigned &radix)
{
  radix = 8;

  if (!NEXT_CHAR_OPTIONAL ())
    {
      radix = 10;
      buffer.AppendL ('0');
      return TRUE;
    }

  while (character >= '0' && character <= '7')
    {
      buffer.AppendL (character);
      if (!NEXT_CHAR_OPTIONAL ())
        return TRUE;
    }

  if (character == '8' || character == '9')
    {
      radix = 10;
      return MatchDecimalLiteral ();
    }

  if (IsIdentifierPart (character))
    {
      SetInvalid ();

      op_sprintf (error_string_buffer, "invalid character after numeric literal: '%c'.", character);
      error_string = error_string_buffer;

      HandleError ();

      return FALSE;
    }

  if (buffer.Length () == 0)
    {
      buffer.AppendL ('0');
      radix = 10;
    }

  return TRUE;
}


BOOL
ES_Lexer::SingleLineComment ()
{
  token.type = ES_Token::END;

  while (1)
    if (IsLinebreak (character))
      {
        if (emit_comments)
          break;

        HandleLinebreak (TRUE);
        if (skip_linebreaks)
          {
            /* Linebreak tokens are skipped when attempting to parse input
               as a (literal) value, an optimization that pays off for
               certain eval() inputs. However, the call to NextToken()
               here will cause a recursive call to SingleLineComment()
               comment should the next line also starts a comment.

               Simply disable the skipping (and optimization) to avoid
               such potentially deep recursion. */
            SetSkipLinebreaks (FALSE);
            NextToken ();
            SetSkipLinebreaks (TRUE);
            return token.type != ES_Token::INVALID;
          }
        token.type = ES_Token::LINEBREAK;
        return TRUE;
      }
    else if (!NEXT_CHAR_OPTIONAL ())
        break;

  if (emit_comments)
    {
      token.type = ES_Token::PUNCTUATOR;
      token.punctuator = ES_Token::SINGLE_LINE_COMMENT;
    }

  return TRUE;
}


BOOL
ES_Lexer::MultiLineComment ()
{
  SetInvalid ();

  eof_context = "in multiline comment";

  NEXT_CHAR ();

  BOOL linebreak = FALSE;
  int ch;

  while (1)
    if (IsLinebreak (ch = character))
      {
        linebreak = TRUE;
        if (!HandleLinebreak ())
          return FALSE;
      }
    else
      {
        NEXT_CHAR ();

        if (ch == '*' && character == '/')
          {
            BOOL has_next = NEXT_CHAR_OPTIONAL ();

            if (emit_comments)
            {
              token.type = ES_Token::PUNCTUATOR;
              token.punctuator = ES_Token::MULTI_LINE_COMMENT;
              return TRUE;
            }

            if (has_next)
              if (linebreak && !skip_linebreaks)
                {
                  token.type = ES_Token::LINEBREAK;
                  return TRUE;
                }
              else
                {
                  /* See SingleLineComment() comment why this is done. */
                  BOOL skipped_linebreaks = skip_linebreaks;
                  if (skipped_linebreaks)
                    SetSkipLinebreaks (FALSE);
                  NextToken ();
                  if (skipped_linebreaks)
                    SetSkipLinebreaks (TRUE);
                  return token.type != ES_Token::INVALID;
                }
            else
              {
                token.type = ES_Token::END;
                return TRUE;
              }
          }
      }
}


BOOL
ES_Lexer::NumericLiteral (unsigned radix)
{
  const uni_char *string = buffer.GetStorage ();

  double value = fromradix (string, NULL, radix, buffer.Length ());

  token.type = ES_Token::LITERAL;

  if (op_isnan (value))
    {
      token.type = ES_Token::INVALID;

      error_string = "invalid numeric literal.";

      HandleError ();

      return FALSE;
    }
  else if (op_isintegral (value) && value <= INT_MAX && value >= INT_MIN)
    token.value.SetInt32 (DOUBLE2INT32 (value));
  else
    token.value.SetDouble (value);

  return TRUE;
}


BOOL
ES_Lexer::StringLiteral ()
{
  SetInvalid ();

  eof_context = "in string literal";

  int delim = character;

  NEXT_CHAR ();

  const uni_char *ptr = fragment + fragment_index, *ptr_start = ptr, *ptr_end = fragment + fragment_length;

  unsigned start_index = fragment_offset + fragment_index - 1, hash;

  ES_HASH_INIT (hash);

  while (ptr != ptr_end)
    if (*ptr == delim)
      {
        token.type = ES_Token::LITERAL;
        token.value.SetString (MakeString (ptr_start, ptr - ptr_start, TRUE, hash));
        token.contained_escapes = false;

        fragment_index = ptr - fragment;
        NEXT_CHAR_OPTIONAL ();

        return TRUE;
      }
    else if (*ptr == '\\' || IsLinebreak (*ptr))
      break;
    else
      ES_HASH_UPDATE (hash, *ptr++);

  buffer.AppendL (ptr_start, ptr - ptr_start);

  fragment_index = ptr - 1 - fragment;

  NEXT_CHAR ();

  while (1)
    {
      int ch = character;

      if (ch == '\\')
        {
          if (!HandleEscape (true))
            return FALSE;
        }
      else if (IsLinebreak (ch))
        {
          SetInvalid ();

          error_string = "in string literal: invalid line terminator.";

          return FALSE;
        }
      else if (ch == delim)
        {
          token.type = ES_Token::LITERAL;
          token.value.SetString (MakeString (buffer.GetStorage (), buffer.Length (), FALSE));
          token.contained_escapes = true;

          if (long_string_literal_table && buffer.Length () > 1024)
            long_string_literal_table->AddL (context, token.value.GetString (), start_index);

          NEXT_CHAR_OPTIONAL ();

          return TRUE;
        }
      else
        {
          buffer.AppendL (character);
          NEXT_CHAR ();
        }
    }
}


/* static */ JString *
ES_Lexer::ProcessStringLiteral(ES_Context *context, const uni_char *literal, unsigned length, BOOL is_strict_mode)
{
  JString *string = JString::Make (context, length);
  uni_char *write = Storage (context, string), *stop = write + length;

  while (write != stop)
    {
      uni_char ch = *++literal;

      if (ch != '\\')
        *write++ = ch;
      else
        {
          ch = *++literal;

          if (ch == 10 || ch == 13)
            {
              if (literal[1] == 10 || literal[1] == 13)
                ++literal;
            }
          else if (ch == 'u')
            {
              *write++ = (HexDigitValue (literal[1]) << 12) + (HexDigitValue (literal[2]) << 8) + (HexDigitValue (literal[3]) << 4) + HexDigitValue (literal[4]);
              literal += 4;
            }
          else if (ch == 'x')
            {
              *write++ = (HexDigitValue (literal[1]) << 4) + HexDigitValue (literal[2]);
              literal += 2;
            }
          else if (!is_strict_mode && IsOctalDigit (ch))
            {
              ch = DecimalDigitValue (ch);

              for (unsigned count = 1; count < 3 && IsOctalDigit (literal[1]); ++count)
                ch = (ch << 3) + DecimalDigitValue (*++literal);

              *write++ = ch;
            }
          else if (ch == '0')
            *write++ = 0;
          else if (ch == 'b')
            *write++ = '\b';
          else if (ch == 't')
            *write++ = '\t';
          else if (ch == 'n')
            *write++ = '\n';
          else if (ch == 'v')
            *write++ = '\v';
          else if (ch == 'f')
            *write++ = '\f';
          else if (ch == 'r')
            *write++ = '\r';
          else
            *write++ = ch;
        }
    }

  OP_ASSERT (write - Storage (context, string) == static_cast<int>(length));
  return string;
}


BOOL
ES_Lexer::RegexpLiteral ()
{
  BOOL g = FALSE, i = FALSE, m = FALSE, x = FALSE, y = FALSE;
  unsigned flags = 0;
  RegExp *regexp;
  RegExpFlags reflags;

  while (character != '/')
    {
      buffer.AppendL (character);

      if (character == '\\')
        {
          if (!NEXT_CHAR_OPTIONAL ())
            goto invalid_eof;
          else if (IsLinebreak(character))
            goto invalid_escape;

          buffer.AppendL (character);
        }
      else if (character == '[')
        {
          if (!NEXT_CHAR_OPTIONAL ())
            goto invalid_eof;

          while (character != ']')
            {
              if (IsLinebreak (character))
                goto invalid_linebreak;

              buffer.AppendL (character);

              if (character == '\\')
                {
                  if (!NEXT_CHAR_OPTIONAL ())
                    goto invalid_eof;
                  if (IsLinebreak (character))
                    goto invalid_linebreak;
                  buffer.AppendL (character);
                }

              if (!NEXT_CHAR_OPTIONAL ())
                goto invalid_eof;
            }

          buffer.AppendL (character);
        }
      else if (IsLinebreak (character))
        goto invalid_linebreak;

      if (!NEXT_CHAR_OPTIONAL ())
        goto invalid_eof;
    }

  while (NEXT_CHAR_OPTIONAL ())
    {
      uni_char ch = character;

      if (ch == '\\')
        {
          if (!HandleCharEscape(ch))
            break;
        }

      if (!IsIdentifierPart (ch))
        break;
      else if (ch == 'g' && !g)
        {
          g = TRUE;
          flags |= REGEXP_FLAG_GLOBAL;
        }
      else if (ch == 'i' && !i)
        {
          i = TRUE;
          flags |= REGEXP_FLAG_IGNORECASE;
        }
      else if (ch == 'm' && !m)
        {
          m = TRUE;
          flags |= REGEXP_FLAG_MULTILINE;
        }
#ifdef ES_NON_STANDARD_REGEXP_FEATURES
      else if (ch == 'x' && !x)
        {
          x = TRUE;
          flags |= REGEXP_FLAG_EXTENDED;
        }
      else if (ch == 'y' && !y)
        {
          y = TRUE;
          flags |= REGEXP_FLAG_NOSEARCH;
        }
#endif // ES_NON_STANDARD_REGEXP_FEATURES
      else
        {
          SetInvalid ();

          error_string = error_string_buffer;
          op_sprintf (error_string_buffer, "in regexp literal: invalid flag: '%c'", ch);

          return FALSE;
        }
    }

  if (owner_parser)
    {
      regexp = OP_NEW_L (RegExp, ());
      reflags.ignore_case = i ? YES : NO;
      reflags.multi_line = m ? YES : NO;
      reflags.ignore_whitespace = x;
      reflags.searching = !y;

      OP_STATUS status;

      if (OpStatus::IsMemoryError (status = regexp->Init (buffer.GetStorage (), buffer.Length (), NULL, &reflags)))
        {
          regexp->DecRef ();
          context->AbortOutOfMemory ();
        }
      else if (OpStatus::IsError (status))
        {
          regexp->DecRef ();

          SetInvalid ();

          error_string = error_string_buffer;
          op_sprintf (error_string_buffer, "invalid regexp literal");

          return FALSE;
        }
      else if (!(token.regexp = OP_NEWGRO_L (ES_RegExp_Information, (), Arena ())))
        {
          regexp->DecRef ();
          context->AbortOutOfMemory ();
        }

#ifdef ES_NATIVE_SUPPORT
      if (context->UseNativeDispatcher ())
        {
          status = regexp->CreateNativeMatcher (g_executableMemory);

          if (OpStatus::IsMemoryError (status))
            {
              regexp->DecRef ();
              context->AbortOutOfMemory ();
            }
        }
#endif // ES_NATIVE_SUPPORT

      token.regexp->regexp = regexp;
      token.regexp->source = UINT_MAX;
      token.regexp_source = MakeString (buffer.GetStorage (), buffer.Length (), FALSE);
      token.regexp->flags = flags;

      /* The active parser keeps track of the RegExp literals, so as to allow a
         clean release (failure) or re-assignment of ownership (success) when
         parsing is done. */
      status = owner_parser->RegisterRegExpLiteral (token.regexp);
      if (OpStatus::IsMemoryError (status))
        {
          token.regexp = NULL;
          regexp->DecRef ();
          context->AbortOutOfMemory ();
        }
    }

  token.type = ES_Token::LITERAL;

  return TRUE;

 invalid_linebreak:
  token.type = ES_Token::INVALID;
  error_string = "in regexp literal: line break character encountered";
  return FALSE;

 invalid_escape:
  token.type = ES_Token::INVALID;
  error_string = "in regexp literal: invalid escape sequence";
  return FALSE;

 invalid_eof:
  token.type = ES_Token::INVALID;
  error_string = "in regexp literal: unexpected end of input";
  return FALSE;
}


BOOL
ES_Lexer::Identifier ()
{
  token.type = ES_Token::IDENTIFIER;

  const uni_char *ptr = fragment + fragment_index, *ptr_start = ptr, *ptr_end = fragment + fragment_length;

  while (ptr != ptr_end)
    if (*ptr == '\\')
      break;
    else if (!IsIdentifierPart (*ptr))
      {
        identifier = ptr_start;
        identifier_length = ptr - ptr_start;

        fragment_index = ptr - fragment - 1;
        NEXT_CHAR_OPTIONAL ();

        return TRUE;
      }
    else
      ++ptr;

  identifier = NULL;

  do
    {
      while (character == '\\')
        {
          if (!HandleEscape (FALSE))
            return FALSE;
        }

      if (!IsIdentifierPart (character))
        break;
      else
        buffer.AppendL (character);
    }
  while (NEXT_CHAR_OPTIONAL ());

  return TRUE;
}


#if 0

static unsigned
FindString (const char *haystack, const uni_char *needle, unsigned length)
{
  for (unsigned index = 0, cindex; *haystack; ++index, haystack += length)
    {
      for (cindex = 0; cindex < length; ++cindex)
        if (haystack[cindex] != needle[cindex])
          break;
      if (cindex == length)
        return index;
    }

  return ~0u;
}

#endif // 0


BOOL
ES_Lexer::HandlePunctuator (int ch)
{
  enum { FOLLOWED_BY_1EQUALS = -1, FOLLOWED_BY_2EQUALS = -2, PLUS = -3, MINUS = -4, AMPERSAND = -5, PIPE = -6, LESS_THAN = -7, GREATER_THAN = -8, SLASH = -9 };

  static const signed char table1[] = {
    0,  // space
    FOLLOWED_BY_2EQUALS, // exclamation mark
    0,  // quotation mark
    0,  // hash
    0,  // dollar sign
    FOLLOWED_BY_1EQUALS, // percent sign
    AMPERSAND, // ampersand
    0,  // apostrophe
    ES_Token::LEFT_PAREN,
    ES_Token::RIGHT_PAREN,
    FOLLOWED_BY_1EQUALS, // asterisk
    PLUS, // plus
    ES_Token::COMMA,
    MINUS, // minus
    ES_Token::PERIOD,
    SLASH, // slash

    /* digits */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    ES_Token::COLON,
    ES_Token::SEMICOLON,
    LESS_THAN, // less than
    FOLLOWED_BY_2EQUALS, // equals
    GREATER_THAN, // greater than
    ES_Token::QUESTION_MARK,
    0,  // at

    /* A-Z */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    ES_Token::LEFT_BRACKET,
    0,  // backslash
    ES_Token::RIGHT_BRACKET,
    FOLLOWED_BY_1EQUALS, // caret
    0,  // underscore
    0,  // grave accent

    /* a-z */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    ES_Token::LEFT_BRACE,
    PIPE, // pipe
    ES_Token::RIGHT_BRACE,
    ES_Token::BITWISE_NOT  // tilde
  };

  token.type = ES_Token::PUNCTUATOR;

  if (ch < 32)
    return FALSE;

  if (!((ch - 32) < static_cast<int> (sizeof table1)))
    return FALSE;

  int r = table1[ch - 32];

  if (r == 0)
    return FALSE;
  else if (r > 0)
    goto finished_consume;

  switch (r)
    {
    case FOLLOWED_BY_1EQUALS:
      if (NEXT_CHAR_OPTIONAL () && fragment[fragment_index] == '=')
        switch (ch)
          {
          case '%': r = ES_Token::REMAINDER_ASSIGN; goto finished_consume;
          case '&': r = ES_Token::BITWISE_AND_ASSIGN; goto finished_consume;
          case '*': r = ES_Token::MULTIPLY_ASSIGN; goto finished_consume;
          case '+': r = ES_Token::ADD_ASSIGN; goto finished_consume;
          case '-': r = ES_Token::SUBTRACT_ASSIGN; goto finished_consume;
          case '<': r = ES_Token::LESS_THAN_OR_EQUAL; goto finished_consume;
          case '>': r = ES_Token::GREATER_THAN_OR_EQUAL; goto finished_consume;
          case '^': r = ES_Token::BITWISE_XOR_ASSIGN; goto finished_consume;
          case '|': r = ES_Token::BITWISE_OR_ASSIGN; goto finished_consume;
          }
      else
        switch (ch)
          {
          case '%': r = ES_Token::REMAINDER; goto finished;
          case '&': r = ES_Token::BITWISE_AND; goto finished;
          case '*': r = ES_Token::MULTIPLY; goto finished;
          case '+': r = ES_Token::ADD; goto finished;
          case '-': r = ES_Token::SUBTRACT; goto finished;
          case '<': r = ES_Token::LESS_THAN; goto finished;
          case '>': r = ES_Token::GREATER_THAN; goto finished;
          case '^': r = ES_Token::BITWISE_XOR; goto finished;
          case '|': r = ES_Token::BITWISE_OR; goto finished;
          }

    case FOLLOWED_BY_2EQUALS:
      if (NEXT_CHAR_OPTIONAL () && fragment[fragment_index] == '=')
        if (NEXT_CHAR_OPTIONAL () && fragment[fragment_index] == '=')
          {
            r = ch == '!' ? ES_Token::STRICT_NOT_EQUAL : ES_Token::STRICT_EQUAL;
            goto finished_consume;
          }
        else
          {
            r = ch == '!' ? ES_Token::NOT_EQUAL : ES_Token::EQUAL;
            goto finished;
          }
      else
        {
          r = ch == '!' ? ES_Token::LOGICAL_NOT : ES_Token::ASSIGN;
          goto finished;
        }

    case PLUS:
      if (NEXT_CHAR_OPTIONAL ())
        if (fragment[fragment_index] == '=')
          {
            r = ES_Token::ADD_ASSIGN;
            goto finished_consume;
          }
        else if (fragment[fragment_index] == '+')
          {
            r = ES_Token::INCREMENT;
            goto finished_consume;
          }

      r = ES_Token::ADD;
      goto finished;

    case MINUS:
      if (NEXT_CHAR_OPTIONAL ())
        if (fragment[fragment_index] == '=')
          {
            r = ES_Token::SUBTRACT_ASSIGN;
            goto finished_consume;
          }
        else if (fragment[fragment_index] == '-')
          {
            r = ES_Token::DECREMENT;
            goto finished_consume;
          }

      r = ES_Token::SUBTRACT;
      goto finished;

    case AMPERSAND:
      if (NEXT_CHAR_OPTIONAL ())
        if (fragment[fragment_index] == '=')
          {
            r = ES_Token::BITWISE_AND_ASSIGN;
            goto finished_consume;
          }
        else if (fragment[fragment_index] == '&')
          {
            r = ES_Token::LOGICAL_AND;
            goto finished_consume;
          }

      r = ES_Token::BITWISE_AND;
      goto finished;

    case PIPE:
      if (NEXT_CHAR_OPTIONAL ())
        if (fragment[fragment_index] == '=')
          {
            r = ES_Token::BITWISE_OR_ASSIGN;
            goto finished_consume;
          }
        else if (fragment[fragment_index] == '|')
          {
            r = ES_Token::LOGICAL_OR;
            goto finished_consume;
          }

      r = ES_Token::BITWISE_OR;
      goto finished;

    case LESS_THAN:
      if (NEXT_CHAR_OPTIONAL ())
        if (fragment[fragment_index] == '<')
          if (NEXT_CHAR_OPTIONAL () && fragment[fragment_index] == '=')
            {
              r = ES_Token::SHIFT_LEFT_ASSIGN;
              goto finished_consume;
            }
          else
            {
              r = ES_Token::SHIFT_LEFT;
              goto finished;
            }
        else if (fragment[fragment_index] == '=')
          {
            r = ES_Token::LESS_THAN_OR_EQUAL;
            goto finished_consume;
          }
        else if (fragment[fragment_index] == '!' && IsLookingAt ("!--", 3))
          return SingleLineComment ();

      r = ES_Token::LESS_THAN;
      goto finished;

    case GREATER_THAN:
      if (NEXT_CHAR_OPTIONAL ())
        if (fragment[fragment_index] == '>')
          {
            if (NEXT_CHAR_OPTIONAL ())
              if (fragment[fragment_index] == '>')
                if (NEXT_CHAR_OPTIONAL () && fragment[fragment_index] == '=')
                  {
                    r = ES_Token::SHIFT_UNSIGNED_RIGHT_ASSIGN;
                    goto finished_consume;
                  }
                else
                  {
                    r = ES_Token::SHIFT_UNSIGNED_RIGHT;
                    goto finished;
                  }
              else if (fragment[fragment_index] == '=')
                {
                  r = ES_Token::SHIFT_SIGNED_RIGHT_ASSIGN;
                  goto finished_consume;
                }

            r = ES_Token::SHIFT_SIGNED_RIGHT;
            goto finished;
          }
        else if (fragment[fragment_index] == '=')
          {
            r = ES_Token::GREATER_THAN_OR_EQUAL;
            goto finished_consume;
          }

      r = ES_Token::GREATER_THAN;
      goto finished;

    case SLASH:
      if (NEXT_CHAR_OPTIONAL ())
        if (fragment[fragment_index] == '*')
          return MultiLineComment ();
        else if (fragment[fragment_index] == '/')
          return SingleLineComment ();
        else if (fragment[fragment_index] == '=')
          {
            r = ES_Token::DIVIDE_ASSIGN;
            goto finished_consume;
          }

      r = ES_Token::DIVIDE;
      goto finished;
    }

 finished_consume:
  NEXT_CHAR_OPTIONAL ();

 finished:
  token.punctuator = (ES_Token::Punctuator) r;
  return TRUE;
}


BOOL
ES_Lexer::HandleCharEscape(uni_char &ch)
{
  OP_ASSERT (character == '\\');

  SetInvalid ();

  if (!IsLookingAt("\\u", 2) || !IsLookingAt(IsHexDigit, 2, 4))
    return FALSE;

  eof_context = "in character escape sequence";

  NEXT_CHAR ();

  ch = 0;

  for (int i = 0; i < 4; i++)
    {
      NEXT_CHAR ();

      ch = (ch << 4) + HexDigitValue (character);
    }

  return TRUE;
}

BOOL
ES_Lexer::HandleEscape (BOOL in_literal)
{
  OP_ASSERT (character == '\\');

  ES_Token::Type old_type = token.type;

  eof_context = "in character escape sequence";

  SetInvalid ();

  NEXT_CHAR ();

  if (!in_literal && character != 'u')
    {
      error_string = "invalid character escape sequence";

      return FALSE;
    }

  if (character == 'x' || character == 'u')
    {
      unsigned length = character == 'x' ? 2 : 4;
      int ch = 0;

      if (IsLookingAt(IsHexDigit, 1, length))
        {
          while (length-- != 0)
            {
              NEXT_CHAR ();

              ch = (ch << 4) + HexDigitValue (character);
            }

          if (!in_literal && (buffer.Length () == 0 ? !IsIdentifierStart (ch) : !IsIdentifierPart (ch)))
            {
              error_string = "invalid character escape sequence";

              return FALSE;
            }

          buffer.AppendL (ch);
        }
      else
        {
          error_string = "invalid character escape sequence";

          return FALSE;
        }

      NEXT_CHAR_OPTIONAL ();

      token.type = old_type;
      return TRUE;
    }
  else if (character == '0')
    {
      int ch = 0;

      if (NEXT_CHAR_OPTIONAL ())
        if (IsOctalDigit (character))
          {
            ch += DecimalDigitValue (character);

            if (NEXT_CHAR_OPTIONAL ())
              if (IsOctalDigit (character))
                {
                  ch = (ch << 3) + DecimalDigitValue (character);
                  NEXT_CHAR_OPTIONAL ();
                }

            if (is_strict_mode)
              {
                error_string = "invalid octal escape sequence in strict mode";

                return FALSE;
              }
          }

      buffer.AppendL (ch);

      token.type = old_type;
      return TRUE;
    }
  else if (character >= '1' && character <= '7')
    {
      int ch = character - '0', count = 1;
      while (NEXT_CHAR_OPTIONAL ())
        if (count < 3 && character >= '0' && character <= '7')
          {
            int val = character - '0';
            if (ch * 8 + val >= 256)
              break;
            ch = ch * 8 + val;
            ++count;
          }
        else
          break;

      if (is_strict_mode)
        {
          error_string = "invalid octal escape sequence in strict mode";

          return FALSE;
        }

      buffer.AppendL (ch);

      token.type = old_type;
      return TRUE;
    }
  else
    {
      const char *from = "btnvfr";
      const char *to = "\b\t\n\v\f\r";

      while (*from)
        if (*from == character)
          {
            buffer.AppendL (*to);
            break;
          }
        else
          ++from, ++to;

      if (!*from)
        if (IsLinebreak (character))
          {
            if (!HandleLinebreak ())
              return FALSE;
            else
              {
                token.type = old_type;
                return TRUE;
              }
          }
        else
          buffer.AppendL (character);

      NEXT_CHAR_OPTIONAL ();

      token.type = old_type;
      return TRUE;
    }
}


BOOL
ES_Lexer::HandleLinebreak (BOOL optional)
{
#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
  ++line_number;

  int ch = character;
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT

  start_of_line = TRUE;

  if (!NextChar (optional))
    return FALSE;

#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
  if (IsLinebreak (character) && ch != character)
    if (!NextChar (optional))
      return FALSE;

  line_start = fragment_offset + fragment_index;
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT

  return TRUE;
}


void
ES_Lexer::SetInvalid ()
{
  token.type = ES_Token::INVALID;
  token.identifier = NULL;

  error_index = fragment_offset + fragment_index;
  error_string = "Invalid character";

#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
  error_line = line_number;
  error_column = error_index - line_start;
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT
}


JString *
ES_Lexer::MakeString (const uni_char *data, unsigned length, BOOL simple, unsigned hash)
{
  if (length == 1 && *data < STRING_NUMCHARS)
    return context->rt_data->strings[STRING_nul + *data];
  else if (strings_table)
    {
      JString *string;

      JTemporaryString temporary (data, length, hash);
      unsigned index;

      string = strings_table->FindOrFindPositionFor (temporary, index);
      if (!string)
        {
          string = temporary.Allocate (context, simple ? base : NULL);
          string->hash = static_cast<JString *> (temporary)->hash;
          strings_table->AppendAtIndexL (context, string, index, index);
        }

      return string;
    }
  else
    return JString::Make (context, data, length);
}

JString *
ES_Lexer::MakeSharedString (JString *str)
{
  if (str->IsBuiltinString())
    return str;
  else if (strings_table)
    {
      unsigned index;
      if (JString *string = strings_table->FindOrFindPositionFor (str, index))
        str = string;
      else
        strings_table->AppendAtIndexL (context, str, index, index);

      return str;
    }
  else
    return str;
}

void
ES_Lexer::HandleError ()
{
  token.value.SetString (JString::Make (context, error_string));

#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
  token.start = error_index;
  token.end = fragment_offset + fragment_index;
  token.line = error_line;
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT
}


inline void
ES_Lexer::StartToken ()
{
  ResetBuffer ();

  start_index = fragment_index;

#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
  token.start = fragment_offset + fragment_index;
  token.line = line_number;
  token.column = fragment_offset + fragment_index - line_start;
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT
}


BOOL
ES_Lexer::NextCharSlow (BOOL optional)
{
  if (++fragment_number == fragments_count)
    {
      if (!optional)
        {
          SetInvalid ();

          OP_ASSERT(op_strlen(eof_context) + op_strlen(": unexpected end of script") < sizeof(error_string_buffer));
          op_snprintf (error_string_buffer, sizeof(error_string_buffer), "%s%s", eof_context, ": unexpected end of script");

          error_string = error_string_buffer;
        }

      fragment_index = fragment_length;
      --fragment_number;
      character = -1;

      return FALSE;
    }

  fragment_index = 0;
  fragment_offset += fragment_length;

  source->GetFragment (fragment_number, fragment, fragment_length);

#ifdef _DEBUG
  OP_ASSERT (fragment_offset + fragment_length <= source->length);
#endif // _DEBUG

  character = fragment[fragment_index];
  return TRUE;
}


BOOL
ES_Lexer::IsLookingAtSlow (const char *string, unsigned string_length)
{
  unsigned fi = fragment_index, fl = fragment_length, fn = fragment_number;
  const uni_char *f = fragment;

  for (unsigned index = 0; index < string_length; ++index)
    {
      while (fi == fl)
        if (++fn == fragments_count)
          return FALSE;
        else
          source->GetFragment (fn, f, fl), fi = 0;

      if (f[fi++] != string[index])
        return FALSE;
    }

  return TRUE;
}


BOOL
ES_Lexer::IsLookingAt (BOOL (*fun)(int), unsigned start, unsigned length)
{
  unsigned fi = fragment_index, fl = fragment_length, fn = fragment_number;
  const uni_char *f = fragment;

  for (unsigned index = 0; index < start; ++index)
    {
      while (fi == fl)
        if (++fn == fragments_count)
          return FALSE;
        else
          source->GetFragment (fn, f, fl), fi = 0;

      fi++;
    }

  for (unsigned index = 0; index < length; ++index)
    {
      while (fi == fl)
        if (++fn == fragments_count)
          return FALSE;
        else
          source->GetFragment (fn, f, fl), fi = 0;

      if (!fun(f[fi++]))
        return FALSE;
    }

  return TRUE;
}

void
ES_Lexer::ResetBuffer ()
{
  buffer.Clear ();
}
