/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPLEXER_H
#define XPLEXER_H

#include "modules/xpath/src/xpdefs.h"

enum XPath_TokenType
  {
    XP_TOKEN_INVALID,

    XP_TOKEN_LITERAL,
    XP_TOKEN_NUMBER,
    XP_TOKEN_OPERATOR,
    XP_TOKEN_PUNCTUATOR,
    XP_TOKEN_FUNCTIONNAME,
    XP_TOKEN_VARIABLEREFERENCE,
    XP_TOKEN_NAMETEST,
    XP_TOKEN_NODETYPE,
    XP_TOKEN_AXISNAME,

    XP_TOKEN_END
  };

class XPath_Token
{
public:
  XPath_Token ();
  XPath_Token (XPath_TokenType type, const uni_char *value, unsigned length);

  void Reset ();

  BOOL operator== (XPath_TokenType other_type) const;
  BOOL operator!= (XPath_TokenType other_type) const;

  BOOL operator== (const char *string) const;
  BOOL operator!= (const char *string) const;

  XPath_TokenType type;
  const uni_char *value;
  unsigned length;

#ifdef XPATH_ERRORS
  XPath_Location location;
  const XPath_Location &GetLocation () const { return location; }
#endif // XPATH_ERRORS
};

class XPath_Lexer
{
protected:
  XPath_Token current, last;
  const uni_char *string, *start;

#ifdef XPATH_ERRORS
  void SyntaxErrorL (const char *message, const XPath_Token &token);
#endif // XPATH_ERRORS

  void SkipWhitespace ();
  void ReadNameL (BOOL special);

public:
  XPath_Lexer (const uni_char *string);

#ifdef XPATH_ERRORS
  OpString *error_message_storage;

  const uni_char *GetSource () { return start; }
#endif // XPATH_ERRORS

  void Reset ();
  void PrependToken (const XPath_Token &token) { current = token; }

  const XPath_Token &GetNextTokenL ();
  const XPath_Token &GetNextToken ();
  void ConsumeToken ();

  const XPath_Token &ConsumeAndGetNextTokenL ();

  unsigned GetOffset () { return string - start; }
};

#endif // XPLEXER_H
