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

/* This is a bison parser. Use bison to create the actual parser from pop3-parse.y.
 * Do not modify the parser pop3-parse.cpp itself, modify pop3-parse.y instead
 * and regenerate the parser.
 *
 * To generate the parser, use:
 *   bison pop3-parse.y
 */

// Needed to differentiate from the IMAPParser::YYSTYPE, see DSK-221435
namespace POP3Parser {
	union YYSTYPE;
}
using POP3Parser::YYSTYPE;

#define YY_NO_UNISTD_H
#define YYINCLUDED_STDLIB_H
#define YY_EXTRA_TYPE IncomingParser<YYSTYPE>*

#ifdef MSWIN
// Disable some warnings that MSVC issues for the generated code that we cannot fix but know to be safe
# pragma warning (disable:4065) // Switch statement with no cases but default
# pragma warning (disable:4702) // Unreachable code
#endif

#include "adjunct/m2/src/util/parser.h"
#include "adjunct/m2/src/backend/pop/pop3-parse.hpp"
#include "adjunct/m2/src/backend/pop/pop3-tokenizer.h"
#include "adjunct/m2/src/backend/pop/pop3-protocol.h"

#undef yylex
#define yylex POP3get_extra(yyscanner)->GetNextProcessedToken

void POP3error(yyscan_t yyscanner, POP3* connection, const char *str)
{
	connection->OnParseError(str);
}

%}

%name-prefix="POP3"
%output="pop3-parse.cpp"
%defines
%pure-parser
%parse-param {yyscan_t yyscanner}
%lex-param {yyscan_t yyscanner}
%parse-param {POP3* connection}
%initial-action
{
#if YYDEBUG
	yydebug = 1;
#endif
}

%token POP_CAPA_CRAMMD5
%token POP_CAPA_PLAIN
%token POP_CAPA_SASL
%token POP_CAPA_STARTTLS
%token POP_CAPA_TOP
%token POP_CAPA_UIDL
%token POP_CAPA_UNKNOWN
%token POP_CAPA_USER
%token <string> POP_CONT_REQ
%token POP_CRLF
%token POP_DOT_STUFFED
%token POP_GREETING_OK
%token <number> POP_NUMBER
%token <string> POP_SINGLE_LINE_ERR
%token <string> POP_SINGLE_LINE_OK
%token POP_TERMINATOR
%token <string> POP_TIMESTAMP
%token <string> POP_UIDL

%union
{
	int         number;
	const char* string;
}

%%

responses:		  /* empty */
				| responses response
				;

response:		  single_line
				| multi_line
				| greeting
				| continue_req
				;

single_line:	  POP_SINGLE_LINE_OK				{ connection->OnSuccess($1); }
				| POP_SINGLE_LINE_ERR				{ connection->OnError($1); }
				;

continue_req:	  POP_CONT_REQ						{ connection->OnContinueReq($1); }
				;

multi_line:	  	  POP_TERMINATOR					{ connection->OnMultiLineComplete(); }
				| POP_DOT_STUFFED
				| capability param_list POP_CRLF
				| msg_info
				;

capability:		  POP_CAPA_SASL						{ connection->AddCapability(POP3::CAPA_SASL); }
				| POP_CAPA_STARTTLS					{ connection->AddCapability(POP3::CAPA_STARTTLS); }
				| POP_CAPA_TOP						{ connection->AddCapability(POP3::CAPA_TOP); }
				| POP_CAPA_UIDL						{ connection->AddCapability(POP3::CAPA_UIDL); }
				| POP_CAPA_USER						{ connection->AddCapability(POP3::CAPA_USER); }
				| POP_CAPA_CRAMMD5					{ connection->AddCapability(POP3::CAPA_CRAMMD5); }
				| POP_CAPA_PLAIN					{ connection->AddCapability(POP3::CAPA_PLAIN); }
				| POP_CAPA_UNKNOWN
				;

param_list:		  /* empty */
				| param_list ' ' capability			/* We allow capabilities as parameters and vice versa, to fix bug #328619 */
				;

msg_info:		  POP_NUMBER ' ' POP_UIDL POP_CRLF	{ connection->OnUIDL($1, $3); }
				| POP_NUMBER ' ' POP_NUMBER POP_CRLF { connection->OnList($1, $3); }
				| POP_NUMBER ' ' POP_CRLF			/* illegal entry, see bug #346634 */
				;

greeting:		  POP_GREETING_OK timestamp POP_CRLF { connection->OnGreeting(); }
				;

timestamp:		  /* empty */
				| POP_TIMESTAMP						{ connection->OnTimestamp($1); }
				;

%%
