/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_SUPPORT

#include "modules/xpath/src/xplexer.h"
#include "modules/xpath/src/xpdefs.h"
#include "modules/xpath/src/xputils.h"
#include "modules/xmlutils/xmlutils.h"

static BOOL
IsDigit (uni_char ch)
{
  return ch >= '0' && ch <= '9';
}

XPath_Token::XPath_Token ()
{
  Reset ();
}

XPath_Token::XPath_Token (XPath_TokenType type, const uni_char *value, unsigned length)
  : type (type),
    value (value),
    length (length)
{
}

void
XPath_Token::Reset ()
{
  type = XP_TOKEN_INVALID;
  value = 0;
  length = 0;
#ifdef XPATH_ERRORS
  location = XPath_Location ();
#endif // XPATH_ERRORS
}

BOOL
XPath_Token::operator== (XPath_TokenType other_type) const
{
  return type == other_type;
}

BOOL
XPath_Token::operator!= (XPath_TokenType other_type) const
{
  return type != other_type;
}

BOOL
XPath_Token::operator== (const char *string) const
{
  if (type == XP_TOKEN_LITERAL)
    return FALSE;

  unsigned index;

  for (index = 0; index < length; ++index)
    if (value[index] != string[index])
      return FALSE;

  return string[index] == 0;
}

BOOL
XPath_Token::operator!= (const char *string) const
{
  return !operator== (string);
}

#ifdef XPATH_ERRORS

void
XPath_Lexer::SyntaxErrorL (const char *message, const XPath_Token &token)
{
  if (error_message_storage)
    {
      LEAVE_IF_ERROR (error_message_storage->AppendFormat (UNI_L ("syntax error at offset %u: "), static_cast<unsigned> (token.value - start) + 1));
      error_message_storage->AppendL (message);

      int index = error_message_storage->Find ("''");
      if (index != KNotFound)
        LEAVE_IF_ERROR (error_message_storage->Insert (index + 1, token.value, token.length));
    }

  LEAVE (OpStatus::ERR);
}

# define XPATH_SYNTAX_ERROR(message, token) SyntaxErrorL (message, token)
#else // XPATH_ERRORS
# define XPATH_SYNTAX_ERROR(message, token) LEAVE (OpStatus::ERR)
#endif // XPATH_ERRORS

void
XPath_Lexer::SkipWhitespace ()
{
  while (*string && uni_isspace (*string))
    ++string;
}

void
XPath_Lexer::ReadNameL (BOOL special)
{
  const uni_char *start = string;
  BOOL colonfound = FALSE;

  if (*string == '*' && !special)
    {
      current = XPath_Token (XP_TOKEN_NAMETEST, start, 1);
      ++string;
      return;
    }
  else if (XMLUtils::IsNameFirst (XMLVERSION_1_0, *string) || *string == '_' || *string == '$')
    {
      BOOL isvariable = *string == '$';

      while (*++string)
        if (*string == ':')
          if (colonfound)
            break;
          else
            colonfound = TRUE;
        else if (XMLUtils::IsName (XMLVERSION_1_0, *string) || IsDigit (*string) || *string == '.' || *string == '-' || *string == '_')
          continue;
        else
          break;

      if (isvariable)
        current = XPath_Token (XP_TOKEN_VARIABLEREFERENCE, start + 1, string - start - 1);
      else if (colonfound && *(string - 1) == ':' && *string == '*')
        {
          current = XPath_Token (XP_TOKEN_NAMETEST, start, string - start + 1);
          ++string;
        }
      else
        {
          if (colonfound && *(string - 1) == ':' && *string == ':')
            --string;

          if (special)
            {
              current = XPath_Token (XP_TOKEN_OPERATOR, start, string - start);

              if (current != "and" && current != "or" && current != "mod" && current != "div")
                XPATH_SYNTAX_ERROR ("expected operator name, got ''", current);

              return;
            }

          const uni_char *tmp = string;

          SkipWhitespace ();

          if (*string == '(')
            {
              current = XPath_Token (XP_TOKEN_NODETYPE, start, tmp - start);

              if (current == "comment" || current == "text" || current == "processing-instruction" || current == "node")
                return;

              current = XPath_Token (XP_TOKEN_FUNCTIONNAME, start, tmp - start);
            }
          else if (*string == ':' && *(string + 1) == ':')
            {
              current = XPath_Token (XP_TOKEN_AXISNAME, start, tmp - start);

              if (current != "ancestor" && current != "ancestor-or-self" && current != "attribute" &&
                  current != "child" && current != "descendant" && current != "descendant-or-self" &&
                  current != "following" && current != "following-sibling" && current != "namespace" &&
                  current != "parent" && current != "preceding" && current != "preceding-sibling" &&
                  current != "self")
                XPATH_SYNTAX_ERROR ("invalid axis name: ''", current);
            }
          else
            current = XPath_Token (XP_TOKEN_NAMETEST, start, tmp - start);

          string = tmp;
        }

      return;
    }

  XPATH_SYNTAX_ERROR ("unexpected ''", XPath_Token (XP_TOKEN_INVALID, start, 1));
}

XPath_Lexer::XPath_Lexer (const uni_char *string)
  : string (string),
    start (string)
{
#ifdef XPATH_ERRORS
  error_message_storage = 0;
#endif // XPATH_ERRORS
}

void
XPath_Lexer::Reset ()
{
  string = start;
  current.Reset ();
  last.Reset ();
}

const XPath_Token &
XPath_Lexer::GetNextTokenL ()
{
  if (current == XP_TOKEN_INVALID)
    {
      SkipWhitespace ();

#ifdef XPATH_ERRORS
      unsigned start = GetOffset ();
#endif // XPATH_ERRORS

      BOOL special = last != XP_TOKEN_INVALID && last != "@" && last != "::" && last != "(" && last != "[" && last != "," && last != XP_TOKEN_OPERATOR;

      if (!*string)
        current = XPath_Token (XP_TOKEN_END, 0, 0);
      else if (*string < 256 && op_strchr ("()[]@,:", *string))
        {
          unsigned length = 1;

          if (*string == '.' && *(string + 1) == '.')
            ++length;
          else if (*string == ':')
            if (*(string + 1) != ':')
              XPATH_SYNTAX_ERROR ("unexpected ''", XPath_Token (XP_TOKEN_INVALID, string, 1));
            else
              ++length;

          current = XPath_Token (XP_TOKEN_PUNCTUATOR, string, length);
          string += length;
        }
      else if (*string < 256 && op_strchr ("/|+-=!<>", *string))
        {
          unsigned length = 1;

          if ((*string == '<' || *string == '>') && *(string + 1) == '=' || *string == '/' && *(string + 1) == '/')
            ++length;
          else if (*string == '!')
            if (*(string + 1) != '=')
              XPATH_SYNTAX_ERROR ("unexpected ''", XPath_Token (XP_TOKEN_INVALID, string, 1));
            else
              ++length;

          current = XPath_Token (XP_TOKEN_OPERATOR, string, length);
          string += length;
        }
      else if (*string == '*' && special)
        {
          current = XPath_Token (XP_TOKEN_OPERATOR, string, 1);
          ++string;
        }
      else if (*string == '\'' || *string == '"')
        {
          const uni_char delimiter = *string, *start = ++string;
          unsigned length = 0;

          while (*string && *string != delimiter)
            {
              ++string;
              ++length;
            }

          if (*string != delimiter)
            XPATH_SYNTAX_ERROR ("unterminated string literal", XPath_Token (XP_TOKEN_INVALID, start - 1, 0));

          current = XPath_Token (XP_TOKEN_LITERAL, start, length);
          ++string;
        }
      else if (*string == '.' || IsDigit (*string))
        if (*string == '.' && !IsDigit (*(string + 1)))
          {
            unsigned length = 1;

            if (*(string + 1) == '.')
              ++length;

            current = XPath_Token (XP_TOKEN_PUNCTUATOR, string, length);
            string += length;
          }
        else
          {
            const uni_char *start = string;

            while (IsDigit (*string))
              ++string;

            if (*string == '.')
              {
                ++string;

                while (IsDigit (*string))
                  ++string;
              }

            current = XPath_Token (XP_TOKEN_NUMBER, start, string - start);
          }
      else
        ReadNameL (special);

#ifdef XPATH_ERRORS
      current.location.start = start;
      current.location.end = GetOffset ();
#endif // XPATH_ERRORS
    }

  return current;
}

const XPath_Token &
XPath_Lexer::GetNextToken ()
{
  TRAPD (status, GetNextTokenL ());
  OP_ASSERT (status == OpStatus::OK);
  OpStatus::Ignore (status);
  return current;
}

void
XPath_Lexer::ConsumeToken ()
{
  last = current;
  current = XPath_Token ();
}

const XPath_Token &
XPath_Lexer::ConsumeAndGetNextTokenL ()
{
  ConsumeToken ();
  return GetNextTokenL ();
}

#endif // XPATH_SUPPORT
