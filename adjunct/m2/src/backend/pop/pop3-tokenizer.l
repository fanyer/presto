%{
/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

/* This is a flex scanner. Use flex to create the actual scanner from pop3-tokenizer.l.
 * Do not modify the scanner pop3-tokenizer.cpp itself, modify pop3-tokenizer.l instead
 * and regenerate the scanner.
 *
 * To generate the scanner, use:
 *   flex pop3-tokenizer.l
 */

// Needed to differentiate from the IMAPParser::YYSTYPE, see DSK-221435
namespace POP3Parser {
	union YYSTYPE;
}
using POP3Parser::YYSTYPE;
#define YY_EXTRA_TYPE IncomingParser<YYSTYPE>*

#include "adjunct/m2/src/backend/pop/pop3-parse.hpp"
#include "adjunct/m2/src/util/parser.h"

#define YY_NO_UNISTD_H

// Redefine the input macro to copy up to max_size characters from the IncomingParser into buf.
// result is the number of characters read. buf is not null-terminated.
#define YY_INPUT(buf,result,max_size) yyget_extra(yyscanner)->GetParseInput(buf, result, max_size)

// Redefine checking for end of input
#define YY_SKIP_YYWRAP
#define POP3wrap(yyscanner) !yyget_extra(yyscanner)->HasInput()

// Enable debugging when asked
#ifdef FLEX_DEBUG
# define SET_DEBUG POP3set_debug(1, yyscanner)
#else
# define SET_DEBUG ((void)0)
#endif

%}

%option prefix="POP3"
%option outfile="pop3-tokenizer.cpp"
%option header-file="pop3-tokenizer.h"
%option reentrant
%option bison-bridge
%option case-insensitive
%option never-interactive
%option nounput

%x GREETING
%x EXTENDED_GREETING
%s CAPA
%s RETR
%s DOT_STUFFED
%s LIST
%s LIST_LIST
%s UIDL
%s UIDL_UIDL

SP			\x20

%%
	SET_DEBUG;

	IncomingParser<YYSTYPE>* parser = yyget_extra(yyscanner);

	// Enter correct state
	if (parser->NeedsReset())
	{
		BEGIN(parser->GetRequiredState());
		parser->SetNeedsReset(FALSE);
	}

	/* Terminator is valid in all states */
\.\r\n						parser->MarkValidQueue(); BEGIN(0); return POP_TERMINATOR;

	/* Single-line responses (exclusive) */
<GREETING>{
	\+OK					BEGIN(EXTENDED_GREETING); return POP_GREETING_OK;
}

<EXTENDED_GREETING>{
	[^\r\n<]+				/* Do nothing */
	\<[^>]*\>				StringParseElement::CopyText(yytext, yyleng, yylval->string, parser); return POP_TIMESTAMP;
	\r\n					parser->MarkValidQueue(); BEGIN(0); return POP_CRLF;
}

	/* Multi-line responses (inclusive) */
<CAPA>{
	SASL					return POP_CAPA_SASL;
	STARTTLS				|
	STLS					return POP_CAPA_STARTTLS;
	TOP						return POP_CAPA_TOP;
	UIDL					return POP_CAPA_UIDL;
	USER					return POP_CAPA_USER;
	CRAM-MD5				return POP_CAPA_CRAMMD5;
	PLAIN					return POP_CAPA_PLAIN;
	[^\r\n ]+				return POP_CAPA_UNKNOWN;
}

<RETR>{
	\+OK[^\r\n]*\r\n		parser->MarkValidQueue(); yylval->string = 0; BEGIN(DOT_STUFFED); return POP_SINGLE_LINE_OK;
	-ERR\r\n				parser->MarkValidQueue(); yylval->string = 0; BEGIN(0); return POP_SINGLE_LINE_ERR;
	-ERR{SP}[^\r\n]*\r\n	parser->MarkValidQueue(); StringParseElement::CopyText(yytext + 5, yyleng - 7, yylval->string, parser); BEGIN(0); return POP_SINGLE_LINE_ERR;
}

<DOT_STUFFED>{
	\.[^\n]+\n				parser->GetTempBuffer().Append(yytext + 1, yyleng - 1); return POP_DOT_STUFFED; // dot-stuffed
	[^\n]*\n				parser->GetTempBuffer().Append(yytext, yyleng); return POP_DOT_STUFFED; // normal multiline response
}

<LIST>{
	{SP}+					BEGIN(LIST_LIST); return ' '; // we accept more than one space for broken servers, bug #328529
}

<LIST_LIST>{
	[0-9]+[^\r\n]*			yylval->number = atoi(yytext); BEGIN(LIST); return POP_NUMBER; // We accept anything after the normal LIST, bug #328844
}

<UIDL>{
	{SP}+					BEGIN(UIDL_UIDL); return ' '; // we accept more than one space for broken servers, bug #328529
}

<UIDL_UIDL>{
	[\x21-\x7e]+{SP}*		{
								// We accept spaces after the UIDL for broken servers, bug #342350
								StringParseElement::CopyText(yytext, yyleng, yylval->string, parser);
								return POP_UIDL;
							}
	\r\n					BEGIN(UIDL); parser->MarkValidQueue(); return POP_CRLF;
}

	/* Generic, for all inclusive states */
\+OK\r\n					parser->MarkValidQueue(); yylval->string = 0; return POP_SINGLE_LINE_OK;
\+OK{SP}[^\r\n]*\r\n		parser->MarkValidQueue(); StringParseElement::CopyText(yytext + 4, yyleng - 6, yylval->string, parser); return POP_SINGLE_LINE_OK;
-ERR\r\n					parser->MarkValidQueue(); yylval->string = 0; BEGIN(0); return POP_SINGLE_LINE_ERR;
-ERR{SP}[^\r\n]*\r\n		parser->MarkValidQueue(); StringParseElement::CopyText(yytext + 5, yyleng - 7, yylval->string, parser); BEGIN(0); return POP_SINGLE_LINE_ERR;
\+{SP}[^\r\n]*\r\n			parser->MarkValidQueue(); StringParseElement::CopyText(yytext + 2, yyleng - 4, yylval->string, parser); return POP_CONT_REQ;
\r\n						parser->MarkValidQueue(); return POP_CRLF;
[0-9]+						yylval->number = atoi(yytext); return POP_NUMBER;
.|\n						return (int) yytext[0];

%%