/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#ifndef ES_PARSER_INLINES_H
#define ES_PARSER_INLINES_H


bool
ES_Parser::HandleLinebreak()
{
    while (allow_linebreak && token.type == ES_Token::LINEBREAK)
    {
        if (!NextToken())
            return false;

        linebreak_seen = true;
    }

    if (token.type != ES_Token::LINEBREAK)
        allow_linebreak = true;

    return token.type != ES_Token::INVALID;
}


bool
ES_Parser::ParseIdentifier(JString *&i, bool opt, bool allow_reserved)
{
    if (!HandleLinebreak())
        return false;

    i = 0;

    if (token.type != ES_Token::IDENTIFIER)
        if (opt)
            return true;
        else if (allow_reserved && (token.type == ES_Token::RESERVED ||
                                    token.type == ES_Token::LITERAL && (token.value.IsBoolean() || token.value.IsNull()) ||
                                    token.type == ES_Token::KEYWORD))
        {
            lexer->RevertTokenIdentifier();
            token = lexer->GetToken();
        }
        else
        {
            automatic_error_code = EXPECTED_IDENTIFIER;

            return false;
        }

    i = token.identifier;

    if (!NextToken())
        return false;

    return true;
}


inline bool
ES_Parser::ParsePunctuator(ES_Token::Punctuator p, bool opt)
{
    return ParsePunctuator1(p) == FOUND || opt;
}

inline bool
ES_Parser::ParsePunctuator(ES_Token::Punctuator p, ES_SourceLocation &location)
{
    bool res = ParsePunctuator(p);
    location = last_token.GetSourceLocation();
    return res;
}


inline ES_Parser::ParseTokenResult
ES_Parser::ParsePunctuator1(ES_Token::Punctuator p)
{
    if (!HandleLinebreak())
        return INVALID_TOKEN;

    if (token.type != ES_Token::PUNCTUATOR || token.punctuator != p)
    {
        switch (p)
        {
        case ES_Token::SEMICOLON:
            automatic_error_code = EXPECTED_SEMICOLON;
            break;

        case ES_Token::RIGHT_BRACE:
            automatic_error_code = EXPECTED_RIGHT_BRACE;
            break;

        case ES_Token::RIGHT_BRACKET:
            automatic_error_code = EXPECTED_RIGHT_BRACKET;
            break;

        case ES_Token::RIGHT_PAREN:
            automatic_error_code = EXPECTED_RIGHT_PAREN;
            break;

        default:
            automatic_error_code = GENERIC_ERROR;
        }

        return NOT_FOUND;
    }

    return NextToken() ? FOUND : INVALID_TOKEN;
}


inline bool
ES_Parser::ParseKeyword(ES_Token::Keyword k, bool opt)
{
    if (!HandleLinebreak())
        return false;

    if (token.type != ES_Token::KEYWORD || token.keyword != k)
        return opt;

    unsigned kw_error_index = token.start;
    unsigned kw_error_line = token.line;

    if (!NextToken())
        return false;

    error_index = kw_error_index;
    error_line = kw_error_line;

    return true;
}

inline bool ES_Parser::IsNearStackLimit()
{
    unsigned char dummy;
    int stack_used = stack_base - &dummy;
    /* Having a SYSTEM_ setting for stack direction would be preferable. */
    if (stack_used < 0)
        stack_used = -stack_used;
    return stack_used >= (ES_CARAKAN_PARM_MAX_PARSER_STACK - ES_MINIMUM_STACK_REMAINING);
}

#endif /* ES_PARSER_INLINES_H */
