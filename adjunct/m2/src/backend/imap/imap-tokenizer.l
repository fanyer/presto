%{
/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/* This is a flex scanner. Use flex to create the actual scanner from imap-tokenizer.l.
 * Do not modify the scanner imap-tokenizer.cpp itself, modify imap-tokenizer.l instead
 * and regenerate the scanner.
 *
 * To generate the scanner, use:
 *   flex imap-tokenizer.l
 *
 * If you want debug output (very verbose!), use:
 *   flex -d imap-tokenizer.l
 */

#include "adjunct/m2/src/backend/imap/imap-parseelements.h"
#include "adjunct/m2/src/backend/imap/imap-parse.hpp"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/util/parser.h"
#include "adjunct/desktop_util/string/hash.h"

typedef YYSTYPE IMAPTokenType;
#define YY_EXTRA_TYPE IncomingParser<IMAPTokenType>*

#define YY_NO_UNISTD_H

// Redefine the input macro to copy up to max_size characters from the IncomingParser into buf.
// result is the number of characters read. buf is not null-terminated.
#define YY_INPUT(buf,result,max_size) yyget_extra(yyscanner)->GetParseInput(buf, result, max_size)

// Redefine checking for end of input
#define YY_SKIP_YYWRAP
#define IMAP4wrap(yyscanner) !yyget_extra(yyscanner)->HasInput()

// Helper macro for resetting the state (popping all states)
#define RESET_STATE while (YY_START != INITIAL) { yy_pop_state(yyscanner); }

// Enable debugging when asked
#ifdef FLEX_DEBUG
# define SET_DEBUG IMAP4set_debug(1, yyscanner)
#else
# define SET_DEBUG ((void)0)
#endif

// Helper functions go here 
YY_EXTRA_TYPE IMAP4get_extra (yyscan_t yyscanner); 
namespace ImapTokenizer 
{ 
    /** Create a new ImapUnion in yylval->generic that contains a copy of the specified string with specified 
      * length 
      * @param str Pointer to a char (C string), doesn't have to be NUL-terminated 
      * @param len Length of string to copy 
      * @param yylval Where to put result 
      * @param yyscanner This tokenizer 
      */ 
    void CopyText(const char* str, unsigned len, YYSTYPE* yylval, yyscan_t yyscanner) 
    { 
        yylval->generic = yyget_extra(yyscanner)->AddElement(new ImapUnion(str, len)); 
        if (!yylval->generic || (len > 0 && !yylval->generic->m_val_string)) 
        { 
            yy_fatal_error("Out of memory!", yyscanner); 
        } 
    } 
 
    /** Create a new ImapUnion in yylval->generic that contains a copy of the specified quoted string with specified 
      * length. Unquotes the string. 
      * @param str Pointer to a char (C string), doesn't have to be NUL-terminated, with a quoted string 
      * @param len Length of string to copy 
      * @param yylval Where to put result 
      * @param yyscanner This tokenizer 
      */ 
    void CopyQuotedString(const char* str, unsigned len, YYSTYPE* yylval, yyscan_t yyscanner) 
    { 
        CopyText("", len - 2, yylval, yyscanner); 
        char* target = yylval->generic->m_val_string; 
 
        for (int i = 1; i < (int)len - 1; i++) 
        { 
            if (str[i] == '\\' && str[i-1] != '\\') 
                continue; 
            *target++ = str[i]; 
        } 
        *target = '\0'; 
    } 
}; 
using namespace ImapTokenizer; 

%}

%option stack
%option prefix="IMAP4"
%option outfile="imap-tokenizer.cpp"
%option reentrant
%option bison-bridge
%option case-insensitive
%option header-file="imap-tokenizer.h"
%option noyy_top_state
%option nounput
%option never-interactive

%x cont
%s untagged
%s tagged
%x resp_text
%x number_code
%x unknown_code
%x text
%x capability
%s badcharset
%s flag
%x ex_flag
%s mbx_list
%s mailbox_data
%s fetch
%x date
%s status
%s mailbox
%x section
%x literal
%x header_fields
%x vanished

ATOM-CHAR				[^(){ [:cntrl:]\]%*"\\]
ASTRING-CHAR			[^(){ [:cntrl:]%*"\\]
TAG-CHAR				[^(){ [:cntrl:]%*"\\+]
QUOTED-CHAR				(([^\\"])|(\\[\\"]))
SECT-CHAR				[^(){ [:cntrl:]\[\]%*"\\<>]
TEXT-CHAR				[^\r\n]
CRLF					\x0d\x0a
SP						\x20
NZNUMBER				[1-9][0-9]*
NUMBER					[0-9]+

%%
	SET_DEBUG;

	IncomingParser<YYSTYPE>* parser = yyget_extra(yyscanner);

	if (parser->NeedsReset())
	{
		RESET_STATE;
		parser->SetNeedsReset(FALSE);
	}

<INITIAL>{
	\+{SP}?					yy_push_state(cont, yyscanner); return IM_CONT; /* hack: {SP} optional to handle Groupwise server, bug #318905 */
	\*{SP}					yy_push_state(untagged, yyscanner); return IM_UNTAGGED;
	{TAG-CHAR}+{SP}			yy_push_state(tagged, yyscanner); yylval->number = Hash::String(yytext, yyleng - 1); return IM_TAG;
}

<resp_text>{
	alert					return IM_ALERT;
	badcharset				yy_push_state(badcharset, yyscanner); return IM_BADCHARSET;
	parse					return IM_PARSE;
	permanentflags			yy_push_state(ex_flag, yyscanner); return IM_PERMANENTFLAGS;
	read-only				return IM_READONLY;
	read-write				return IM_READWRITE;
	trycreate				return IM_TRYCREATE;
	nomodseq				return IM_NOMODSEQ;
	closed					return IM_CLOSED;
	compressionactive		return IM_COMPRESSIONACTIVE;
	uidnext					yy_push_state(number_code, yyscanner); return IM_UIDNEXT;
	uidvalidity				yy_push_state(number_code, yyscanner); return IM_UIDVALIDITY;
	unseen					yy_push_state(number_code, yyscanner); return IM_UNSEEN;
	appenduid				yy_push_state(number_code, yyscanner); return IM_APPENDUID;
	copyuid					yy_push_state(number_code, yyscanner); return IM_COPYUID;
	highestmodseq			yy_push_state(number_code, yyscanner); return IM_HIGHESTMODSEQ;
	modified				yy_push_state(number_code, yyscanner); return IM_MODIFIED;
	capability				yy_push_state(capability, yyscanner); return IM_CAPABILITY;
	uid{SP}					/* do nothing, some servers put this in front of appenduid/copyuid */
	{ATOM-CHAR}+			yy_push_state(unknown_code, yyscanner); // Code we don't know about; Don't copy, we don't care
}

<number_code,vanished>{
	{NZNUMBER}				yylval->number=op_strtoui64(yytext, 0, 10); return IM_NZNUMBER;
	{NUMBER}				yylval->number=op_strtoui64(yytext, 0, 10); return IM_NUMBER;
}

<unknown_code>[^\]]+	/* do nothing */

<capability>{
	auth=plain				return IM_AUTH_PLAIN;
	auth=login				return IM_AUTH_LOGIN;
	auth=cram-md5			return IM_AUTH_CRAMMD5;
	compress=deflate		return IM_COMPRESS_DEFLATE;
	idle					return IM_IDLE;
	imap4rev1				return IM_IMAP4REV1;
	imap4					return IM_IMAP4;
	literal\+				return IM_LITERALPLUS;
	logindisabled			return IM_LOGINDISABLED;
	namespace				return IM_NAMESPACE;
	starttls				return IM_STARTTLS;
	uidplus					return IM_UIDPLUS;
	unselect				return IM_UNSELECT;
	condstore				return IM_CONDSTORE;
	enable					return IM_ENABLE;
	qresync					return IM_QRESYNC;
	id						return IM_ID;
	xlist					return IM_XLIST;
	special-use				return IM_SPECIALUSE;
	{ATOM-CHAR}+			yylval->generic=NULL; return IM_ATOM; // Don't copy, We don't care if it's unknown
}

<resp_text,capability,badcharset,ex_flag,unknown_code,number_code>{
	\]{SP}?					yy_push_state(text, yyscanner); // End of code
}

<cont>{
	\[						yy_push_state(resp_text, yyscanner);
	.|\n					yy_push_state(text, yyscanner); yyless(0);
}

<text>{
	{TEXT-CHAR}+			CopyText(yytext, yyleng, yylval, yyscanner); return IM_FREETEXT;
}

	/* flags */
<flag,ex_flag>{
	\\recent				return IM_FLAG_RECENT;
	\\answered				return IM_FLAG_ANSWERED;
	\\flagged				return IM_FLAG_FLAGGED;
	\\deleted				return IM_FLAG_DELETED;
	\\seen					return IM_FLAG_SEEN;
	\\draft					return IM_FLAG_DRAFT;
	\\\*					return IM_FLAG_STAR;
	\\{ATOM-CHAR}+			return IM_FLAG_EXTENSION;
	$Junk					|
	Junk					|
	JunkRecorded			return IM_FLAG_SPAM;
	$NotJunk				|
	NonJunk					|
	NotJunk					return IM_FLAG_NOT_SPAM;
	hasatt					|
	hasnoatt				|
	hastd					|
	hasnotd					|
	XAOL-{ATOM-CHAR}+		|
	xof-{ATOM-CHAR}+		return IM_FLAG_IGNORE;
	{ATOM-CHAR}+			yylval->number = g_m2_engine->GetIndexer()->GetKeywordID(yytext); return IM_KEYWORD;
	\"{QUOTED-CHAR}*\"		yylval->number = g_m2_engine->GetIndexer()->GetKeywordID(yytext); return IM_KEYWORD;
}

<flag>\)					yy_pop_state(yyscanner); return (int) yytext[0];

	/* list flags */
<mbx_list>{
	\\noinferiors			return IM_LFLAG_NOINFERIORS;
	\\noselect				return IM_LFLAG_NOSELECT;
	\\marked				return IM_LFLAG_MARKED;
	\\unmarked				return IM_LFLAG_UNMARKED;
	\\haschildren			return IM_LFLAG_HASCHILDREN;
	\\hasnochildren			return IM_LFLAG_HASNOCHILDREN;
	\\inbox					return IM_LFLAG_INBOX;
	\\drafts				return IM_LFLAG_DRAFTS;
	\\trash					return IM_LFLAG_TRASH;
	\\sent					return IM_LFLAG_SENT;
	\\spam					return IM_LFLAG_SPAM;
	\\junk					return IM_LFLAG_SPAM;
	\\starred				return IM_LFLAG_FLAGGED;
	\\flagged				return IM_LFLAG_FLAGGED;
	\\allmail				return IM_LFLAG_ALLMAIL;
	\\all					return IM_LFLAG_ALLMAIL;
	\\archive				return IM_LFLAG_ARCHIVE;
	\\{ATOM-CHAR}+			return IM_FLAG_EXTENSION;
	\)						yy_push_state(mailbox, yyscanner); return (int) yytext[0];
}

	/* resp-cond-state */
<tagged,untagged>{
	ok{SP}					yy_push_state(cont, yyscanner); return IM_STATE_OK;
	no{SP}					yy_push_state(cont, yyscanner); return IM_STATE_NO;
	bad{SP}					yy_push_state(cont, yyscanner); return IM_STATE_BAD;
}

<untagged>{
	bye						yy_push_state(text, yyscanner); return IM_BYE;
	list					yy_push_state(mbx_list, yyscanner); return IM_LIST;
	xlist					yy_push_state(mbx_list, yyscanner); return IM_LIST;
	lsub					yy_push_state(mbx_list, yyscanner); return IM_LSUB;
	search					return IM_SEARCH;
	modseq					return IM_MODSEQ;
	status					yy_push_state(status, yyscanner); return IM_STATUS;
	exists					return IM_EXISTS;
	recent					return IM_RECENT;
	expunge					return IM_EXPUNGE;
	vanished				yy_push_state(vanished, yyscanner); return IM_VANISHED;
	fetch					yy_push_state(fetch, yyscanner); return IM_FETCH;
	capability				yy_push_state(capability, yyscanner); return IM_CAPABILITY;
	namespace				return IM_NAMESPACE;
	enabled					yy_push_state(capability, yyscanner); return IM_ENABLED;
	id						return IM_ID;
}

<vanished>{
	earlier					return IM_EARLIER;
}

<untagged,fetch>{
	flags					yy_push_state(flag, yyscanner); return IM_FLAGS;
}

<fetch,section>{
	modseq					return IM_MODSEQ;
	envelope				return IM_ENVELOPE;
	internaldate			yy_push_state(date, yyscanner); return IM_INTERNALDATE;
	rfc822					return IM_RFC822;
	rfc822\.header			return IM_RFC822HEADER;
	rfc822\.text			return IM_RFC822TEXT;
	rfc822\.size			return IM_RFC822SIZE;
	body					return IM_BODY;
	body\[[^\]]*\]<{NUMBER}> |
	body\[[^\]]*\]			yy_push_state(section, yyscanner); yyless(4); return IM_BODY;
	bodystructure			return IM_BODYSTRUCTURE;
	uid						return IM_UID;

	\"rfc822\"				return IM_MEDIARFC822;
	\"application\"			return IM_APPLICATION;
	\"audio\"				return IM_AUDIO;
	\"image\"				return IM_IMAGE;
	\"message\"				return IM_MESSAGE;
	\"video\"				return IM_VIDEO;
	\"zip\"					|
	\"x-zip\"				|
	\"x-rar-compressed\"	return IM_ARCHIVE;
	\"ogg\"					return IM_OGG;
	\"7bit\"				return IM_BIT7;
	\"8bit\"				return IM_BIT8;
	\"binary\"				return IM_BINARY;
	\"base64\"				return IM_BASE64;
	\"quoted\-printable\"	return IM_QUOTEDPRINTABLE;
	\"text\"				return IM_TEXT;
	\"alternative\"			return IM_ALTERNATIVE;
	header					return IM_HEADER;
	header\.fields			yy_push_state(header_fields, yyscanner); return IM_HEADERFIELDS;
	header\.fields\.not		yy_push_state(header_fields, yyscanner); return IM_HEADERFIELDSNOT;
	text					return IM_TEXT;
	mime					return IM_MIME;
}

<header_fields>{
	{SECT-CHAR}+			CopyText(yytext, yyleng, yylval, yyscanner); return IM_ATOM;
	\"{QUOTED-CHAR}\"		CopyQuotedString(yytext, yyleng, yylval, yyscanner); return IM_SINGLE_QUOTED_CHAR;
	\"{QUOTED-CHAR}*\"		CopyQuotedString(yytext, yyleng, yylval, yyscanner); return IM_QUOTED_STRING;
	\)						yy_pop_state(yyscanner); return ')';
}

<section>{
	{NZNUMBER}				yylval->number=op_strtoui64(yytext, 0, 10); return IM_NZNUMBER;
	{NUMBER}				yylval->number=op_strtoui64(yytext, 0, 10); return IM_NUMBER;
	\]<{NUMBER}>			yyless(1); return ']';
	>						yy_pop_state(yyscanner); return '>';
	\]						yy_pop_state(yyscanner); return ']';
}

<date>{
	[0-9]					yylval->number = op_atoi(yytext); return IM_DIGIT;
	[0-9]{2}				yylval->number = op_atoi(yytext); return IM_DIGIT2;
	[0-9]{4}				yylval->number = op_atoi(yytext); return IM_DIGIT4;
	\-[0-9]{4}				yylval->signed_number = op_atoi(yytext); yy_pop_state(yyscanner); return IM_ZONE;
	\+[0-9]{4}				yylval->signed_number = op_atoi(yytext); yy_pop_state(yyscanner); return IM_ZONE;
	jan						yylval->number = 1; return IM_MONTH;
	feb						yylval->number = 2; return IM_MONTH;
	mar						yylval->number = 3; return IM_MONTH;
	apr						yylval->number = 4; return IM_MONTH;
	may						yylval->number = 5; return IM_MONTH;
	jun						yylval->number = 6; return IM_MONTH;
	jul						yylval->number = 7; return IM_MONTH;
	aug						yylval->number = 8; return IM_MONTH;
	sep						yylval->number = 9; return IM_MONTH;
	oct						yylval->number = 10; return IM_MONTH;
	nov						yylval->number = 11; return IM_MONTH;
	dec						yylval->number = 12; return IM_MONTH;
}

<status>{
	messages				return IM_MESSAGES;
	recent					return IM_RECENT;
	uidnext					return IM_UIDNEXT;
	uidvalidity				return IM_UIDVALIDITY;
	unseen					return IM_UNSEEN;
	highestmodseq			return IM_HIGHESTMODSEQ;
}

<mailbox,status>{
	inbox					CopyText("INBOX", 5, yylval, yyscanner); return IM_INBOX;
}

<literal>{
	.*\n					{
								StreamBuffer<char>& buffer   = parser->GetTempBuffer();
								size_t toappend				 = min(buffer.GetCapacity() - buffer.GetFilled(), (size_t)yyleng);

								// Append as much as possible to the buffer
								buffer.Append(yytext, toappend);

								// Return unused part to the stack
								yyless(toappend);

								// Check if we're done with this literal
								if (buffer.GetFilled() == buffer.GetCapacity())
								{
									yylval->generic = parser->AddElement(OP_NEW(ImapUnion,(0)));
									if (!yylval->generic)
										YY_FATAL_ERROR("Out of memory!");
									yylval->generic->m_length     = buffer.GetCapacity();
									yylval->generic->m_val_string = buffer.Release();
									yy_pop_state(yyscanner);
									return IM_LITERAL;
								}
							}
}

	/* For all inclusive states */
nil							return IM_NIL;

{NZNUMBER}					yylval->number=op_strtoui64(yytext, 0, 10); return IM_NZNUMBER;
{NUMBER}					yylval->number=op_strtoui64(yytext, 0, 10); return IM_NUMBER;

{ATOM-CHAR}+				CopyText(yytext, yyleng, yylval, yyscanner); return IM_ATOM;
{ASTRING-CHAR}+				CopyText(yytext, yyleng, yylval, yyscanner); return IM_PURE_ASTRING;
\"{QUOTED-CHAR}\"			CopyQuotedString(yytext, yyleng, yylval, yyscanner); return IM_SINGLE_QUOTED_CHAR;
\"{QUOTED-CHAR}*\"			CopyQuotedString(yytext, yyleng, yylval, yyscanner); return IM_QUOTED_STRING;
\{[0-9]+\}{CRLF}			{
								parser->GetTempBuffer().Reserve(atoi(yytext + 1));
								yy_push_state(literal, yyscanner);
							}

	/* For all states, even exclusive ones */
<*>{CRLF}					{
								// This marks a complete command. Reset the state, valid queue
								RESET_STATE;
								parser->MarkValidQueue();
								return IM_CRLF;
							}
<*>{SP}						return IM_SP;
<*>.|\n						return yytext[0];

%%
