%{
/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/* This is a bison parser. Use bison to create the actual parser from imap-parse.y.
 * Do not modify the parser imap-parse.cpp itself, modify imap-parse.y instead
 * and regenerate the parser.
 *
 * To generate the parser, use:
 *   bison imap-parse.y
 *
 * If you want debug output (very verbose!), use:
 *   bison -t imap-parse.y
 */

#define YY_NO_UNISTD_H
#define YYINCLUDED_STDLIB_H
#define YY_EXTRA_TYPE IncomingParser<YYSTYPE>*

#include "adjunct/m2/src/backend/imap/imap-processor.h"
#include "adjunct/m2/src/backend/imap/imap-parse.hpp"
#include "adjunct/m2/src/backend/imap/imap-tokenizer.h"
#include "adjunct/m2/src/backend/imap/imap-parseelements.h"
#include "adjunct/m2/src/backend/imap/imap-flags.h"
#include "adjunct/m2/src/util/parser.h"

#undef yylex
#define yylex IMAP4get_extra(yyscanner)->GetNextProcessedToken

#ifdef MSWIN
#define YYFPRINTF yyfprintf
void yyfprintf (FILE *hfile, char *textfmt, ...);
#endif //MSWIN

#define CHECK_MEMORY(pointer) \
{ \
	if (!pointer) \
	{ \
		processor->SetErrorStatus(OpStatus::ERR_NO_MEMORY); \
		YYABORT; \
	} \
}

#define NEW_ELEMENT(element, type, arguments) \
{ \
	element = processor->GetParser()->AddElement(new type arguments); \
	CHECK_MEMORY(element); \
}

#define NEW_ELEMENT_IF_NEEDED(element, type, arguments) \
{ \
	if (!element) \
	{ \
		NEW_ELEMENT(element, type, arguments); \
	} \
}

#define REPORT(func) \
{ \
	if (OpStatus::IsError(func)) \
	{ \
		processor->SetErrorStatus(func); \
	}\
}

// Use an approximation of log(10) / log(256) by Eddy to get an appropriate string
// size when converting integers to string
#define INT_STRING_SIZE 2 + (sizeof(INT64) * 53) / 22

void IMAP4error(yyscan_t yyscanner, ImapProcessor* processor, const char *str)
{
	processor->OnParseError(str);
}

#ifdef MSWIN
// Disable some warnings that MSVC issues for the generated code that we cannot fix but know to be safe
# pragma warning (disable:4065) // Switch statement with no cases but default
# pragma warning (disable:4702) // Unreachable code
#endif

%}

%name-prefix="IMAP4"
%output="imap-parse.cpp"
%defines
%pure-parser
%parse-param { yyscan_t yyscanner }
%lex-param { yyscan_t yyscanner }
%parse-param { ImapProcessor* processor }
%initial-action
{
#if YYDEBUG
	yydebug = 1;
#endif
}

%token IM_ALERT
%token IM_ALTERNATIVE
%token IM_APPENDUID
%token IM_APPLICATION
%token IM_ARCHIVE
%token <generic> IM_ATOM
%token IM_AUDIO
%token IM_AUTH_LOGIN
%token IM_AUTH_CRAMMD5
%token IM_AUTH_PLAIN
%token IM_BADCHARSET
%token IM_BASE64
%token IM_BINARY
%token IM_BIT7
%token IM_BIT8
%token IM_BODY
%token IM_BODYSTRUCTURE
%token IM_BYE
%token IM_CAPABILITY
%token IM_CLOSED
%token IM_COMPRESS_DEFLATE
%token IM_COMPRESSIONACTIVE
%token IM_CONDSTORE
%token IM_CONT
%token IM_COPYUID
%token IM_CRLF
%token <number> IM_DIGIT
%token <number> IM_DIGIT2
%token <number> IM_DIGIT4
%token IM_EARLIER
%token IM_ENABLE
%token IM_ENABLED
%token IM_ENVELOPE
%token IM_EXISTS
%token IM_EXPUNGE
%token IM_FETCH
%token IM_FLAGS
%token IM_FLAG_ANSWERED
%token IM_FLAG_DELETED
%token IM_FLAG_DRAFT
%token IM_FLAG_EXTENSION
%token IM_FLAG_FLAGGED
%token IM_FLAG_RECENT
%token IM_FLAG_SEEN
%token IM_FLAG_STAR
%token IM_FLAG_IGNORE
%token IM_FLAG_SPAM
%token IM_FLAG_NOT_SPAM
%token <generic> IM_FREETEXT
%token IM_HEADER
%token IM_HEADERFIELDS
%token IM_HEADERFIELDSNOT
%token IM_HIGHESTMODSEQ
%token IM_ID
%token IM_IDLE
%token IM_IMAGE
%token IM_IMAP4
%token IM_IMAP4REV1
%token <generic> IM_INBOX
%token IM_INTERNALDATE
%token <number> IM_KEYWORD
%token IM_LFLAG_ALLMAIL
%token IM_LFLAG_ARCHIVE
%token IM_LFLAG_DRAFTS
%token IM_LFLAG_FLAGGED
%token IM_LFLAG_HASCHILDREN
%token IM_LFLAG_HASNOCHILDREN
%token IM_LFLAG_MARKED
%token IM_LFLAG_INBOX
%token IM_LFLAG_NOINFERIORS
%token IM_LFLAG_NOSELECT
%token IM_LFLAG_SENT
%token IM_LFLAG_SPAM
%token IM_LFLAG_TRASH
%token IM_LFLAG_UNMARKED
%token IM_LIST
%token <generic> IM_LITERAL
%token IM_LITERALPLUS
%token IM_LOGINDISABLED
%token IM_LSUB
%token IM_MEDIARFC822
%token IM_MESSAGE
%token IM_MESSAGES
%token IM_MIME
%token IM_MODIFIED
%token IM_MODSEQ
%token <number> IM_MONTH
%token IM_NAMESPACE
%token IM_NIL
%token IM_NOMODSEQ
%token <number> IM_NUMBER
%token <number> IM_NZNUMBER
%token IM_OGG
%token IM_PARSE
%token IM_PERMANENTFLAGS
%token <generic> IM_PURE_ASTRING
%token IM_QRESYNC
%token IM_QUOTEDPRINTABLE
%token <generic> IM_QUOTED_STRING
%token IM_READONLY
%token IM_READWRITE
%token IM_RECENT
%token IM_RFC822
%token IM_RFC822HEADER
%token IM_RFC822SIZE
%token IM_RFC822TEXT
%token IM_SEARCH
%token <generic> IM_SINGLE_QUOTED_CHAR
%token IM_SP
%token IM_SPECIALUSE
%token IM_STARTTLS
%token IM_STATE_BAD
%token IM_STATE_NO
%token IM_STATE_OK
%token IM_STATUS
%token <number> IM_TAG
%token IM_TEXT
%token IM_TRYCREATE
%token IM_UID
%token IM_UIDNEXT
%token IM_UIDPLUS
%token IM_UIDVALIDITY
%token IM_UNSEEN
%token IM_UNSELECT
%token IM_UNTAGGED
%token IM_VANISHED
%token IM_VIDEO
%token IM_XLIST
%token <signed_number> IM_ZONE

%type <number> uniqueid
%type <number_vector> search_data
%type <number_vector> nznumber_list
%type <number_vector> nznumber_lst
%type <generic> mailbox
%type <generic> astring
%type <generic> string
%type <generic> quoted_string
%type <number> delimiter
%type <mailboxlist> mailbox_list
%type <number> mbx_list_flags
%type <number> mbx_lst_flags
%type <vector> astring_list
%type <vector> astring_lst
%type <vector> string_list
%type <vector> badcharset
%type <number> mbx_list_flag
%type <number> mbx_list_oflag
%type <number> mbx_list_sflag
%type <response_code> resp_text
%type <response_code> resp_text_code
%type <response_code> resp_cond_state
%type <number> state_word
%type <number> capability_data
%type <number> capability_list
%type <number> capability_lst
%type <number> capability
%type <number> enabled_data
%type <signed_number> flag
%type <flags> flag_list
%type <flags> flag_lst
%type <generic> nstring
%type <envelope> envelope
%type <vector> env_address
%type <vector> address_list
%type <number> date_day_fixed
%type <number> date_year
%type <vector> body_fld_param
%type <generic> body_fld_id
%type <generic> body_fld_desc
%type <generic> body_fld_enc
%type <number> body_fld_octets
%type <name_space> namespace_desc
%type <generic> text
%type <range> seq_range
%type <number> seq_range_end
%type <number_vector> sequence_set
%type <number_vector> seq_set_list
%type <vector> namespace
%type <vector> namespace_desc_list
%type <number> number
%type <body> body
%type <body> body_type_1part
%type <body> body_type_mpart
%type <body> body_type_msg
%type <body> body_type_text
%type <date_time> date_time
%type <vector> body_list
%type <number> media_subtype
%type <number> media_type
%type <address> address
%type <number> section_text
%type <number> section_part
%type <number> section_part_spec
%type <number> section_part_list
%type <number> section_msgtext
%type <number> section_spec
%type <number> section

%union
{
	struct flagstruct
	{
		int flags;
		ImapSortedVector* keywords;
	};
	struct responsestruct
	{
		int state;
		int code;
		ImapUnion* freetext;
	};
	struct mailboxstruct
	{
		int flags;
		char delimiter;
		ImapUnion* mailbox;
	};
	struct rangestruct
	{
		unsigned start;
		unsigned end;
	};

	UINT64 number;
	int signed_number;
	BOOL boolean;
	ImapVector *vector;
	ImapNumberVector *number_vector;
	ImapUnion *generic;
	ImapEnvelope *envelope;
	ImapAddress *address;
	ImapDateTime *date_time;
	mailboxstruct mailboxlist;
	ImapNamespaceDesc *name_space;
	ImapBody *body;
	flagstruct flags;
	responsestruct response_code;
	rangestruct range;
}

%%

responses:		/* empty */
				| responses response { processor->NextCommand(); }
				;

response:		  continue_req
				| response_data
				| response_done
				| error IM_CRLF
				;

continue_req:	  IM_CONT resp_text IM_CRLF { REPORT(processor->Continue($2.freetext)); }
				;

resp_text:	 	  resp_text_code text { $$ = $1; $$.freetext = $2; }
				| text { $$.code = 0; $$.freetext = $1; }
				;

text:			  /* empty */ { $$ = NULL; }
				| IM_FREETEXT { $$ = $1; }
				;

resp_text_code:	  IM_ALERT { $$.code = IM_ALERT; }
				| IM_APPENDUID IM_SP IM_NZNUMBER IM_SP uniqueid { $$.code = IM_APPENDUID; REPORT(processor->AppendUid($3, $5)); }
				| IM_BADCHARSET badcharset { $$.code = IM_BADCHARSET; }
				| capability_data { $$.code = IM_CAPABILITY; processor->Capability($1); }
				| IM_COPYUID IM_SP IM_NZNUMBER IM_SP sequence_set IM_SP sequence_set { $$.code = IM_COPYUID; REPORT(processor->CopyUid($3, $5, $7)); }
				| IM_PARSE { $$.code = IM_PARSE; }
				| IM_PERMANENTFLAGS IM_SP '(' flag_list ')' { $$.code = IM_PERMANENTFLAGS; REPORT(processor->Flags($4.flags, $4.keywords, TRUE)); }
				| IM_READONLY { $$.code = IM_READONLY; REPORT(processor->SetReadOnly(TRUE)); }
				| IM_READWRITE { $$.code = IM_READWRITE; REPORT(processor->SetReadOnly(FALSE)); }
				| IM_TRYCREATE { $$.code = IM_TRYCREATE; /* not handled */ }
				| IM_UIDNEXT IM_SP IM_NZNUMBER { $$.code = IM_UIDNEXT; REPORT(processor->UidNext($3)); }
				| IM_UIDVALIDITY IM_SP IM_NZNUMBER { $$.code = IM_UIDVALIDITY; REPORT(processor->UidValidity($3)); }
				| IM_UNSEEN IM_SP number /* Hack: we allow 0 here, see bug #319952 */ { $$.code = IM_UNSEEN; REPORT(processor->Unseen($3)); }
				| IM_HIGHESTMODSEQ IM_SP IM_NZNUMBER { $$.code = IM_HIGHESTMODSEQ; REPORT(processor->HighestModSeq($3)); }
				| IM_NOMODSEQ { $$.code = IM_NOMODSEQ; REPORT(processor->NoModSeq()); }
				| IM_MODIFIED IM_SP sequence_set { $$.code = IM_MODIFIED; }
				| IM_CLOSED { $$.code = IM_CLOSED; }
				| IM_COMPRESSIONACTIVE { $$.code = IM_COMPRESSIONACTIVE; }
				;

badcharset:		  /* empty */ { NEW_ELEMENT($$, ImapVector, ()); }
				| IM_SP '(' astring_list ')' { $$ = $3; }
				;

seq_range:		  IM_NZNUMBER seq_range_end { $$.start = $1; $$.end = $2 ? $2 : $1; }
				;

seq_range_end:	  /* empty */ { $$ = 0; }
				| ':' IM_NZNUMBER { $$ = $2; }
				;

sequence_set:	  seq_range seq_set_list
				  {
					$$ = $2;
					while ($1.end >= $1.start)
					{
						REPORT($$->Insert(0, $1.end));
						$1.end--;
					}
				  }
				;

seq_set_list:	  /* empty */ { NEW_ELEMENT($$, ImapNumberVector, ()); }
				| seq_set_list ',' seq_range
				  {
					$$ = $1;
					while ($3.start <= $3.end)
					{
						REPORT($$->Add($3.start));
						$3.start++;
					}
				  }
				;

capability_data:  IM_CAPABILITY capability_list { $$ = $2; }
				;

capability_list:  /* empty */ { $$ = ImapFlags::NONE; }
				| IM_SP capability_lst { $$ = $2; }
				;

capability_lst:   /* empty */ { $$ = ImapFlags::NONE; }
				| capability_lst capability IM_SP { $$ = $1 | $2; } /* Strange hack in place for bug DSK-240249 */
				| capability_lst capability { $$ = $1 | $2; }
				;

capability:		  IM_AUTH_PLAIN { $$ = ImapFlags::CAP_AUTH_PLAIN; }
				| IM_AUTH_LOGIN { $$ = ImapFlags::CAP_AUTH_LOGIN; }
				| IM_AUTH_CRAMMD5 { $$ = ImapFlags::CAP_AUTH_CRAMMD5; }
				| IM_COMPRESS_DEFLATE { $$ = ImapFlags::CAP_COMPRESS_DEFLATE; }
				| IM_IDLE { $$ = ImapFlags::CAP_IDLE; }
				| IM_IMAP4REV1 { $$ = ImapFlags::NONE; }
				| IM_IMAP4 { $$ = ImapFlags::NONE; }
				| IM_LITERALPLUS { $$ = ImapFlags::CAP_LITERALPLUS; }
				| IM_LOGINDISABLED { $$ = ImapFlags::CAP_LOGINDISABLED; }
				| IM_NAMESPACE { $$ = ImapFlags::CAP_NAMESPACE; }
				| IM_STARTTLS { $$ = ImapFlags::CAP_STARTTLS; }
				| IM_UIDPLUS { $$ = ImapFlags::CAP_UIDPLUS; }
				| IM_UNSELECT { $$ = ImapFlags::CAP_UNSELECT; }
				| IM_CONDSTORE { $$ = ImapFlags::CAP_CONDSTORE; }
				| IM_ENABLE { $$ = ImapFlags::CAP_ENABLE; }
				| IM_QRESYNC { $$ = ImapFlags::CAP_QRESYNC; }
				| IM_XLIST { $$ = ImapFlags::CAP_XLIST; }
				| IM_SPECIALUSE { $$ = ImapFlags::CAP_SPECIAL_USE; }
				| IM_ID { $$ = ImapFlags::CAP_ID; }
				| IM_ATOM { $$ = ImapFlags::NONE; }
				;

id_response:	 IM_ID IM_SP id_params_list
				;
id_params_list:  '(' string IM_SP nstring id_params_lst ')'
				| IM_NIL
				;
id_params_lst:	  /* empty */
				| IM_SP string IM_SP nstring id_params_lst
				;

enabled_data:	  IM_ENABLED capability_list { $$ = $2; }
                                ;
response_done:	  response_tagged
				| response_fatal
				;

response_tagged:  IM_TAG resp_cond_state IM_CRLF { REPORT(processor->TaggedState($1, $2.state, $2.code, $2.freetext)); }
				;

response_fatal:	  IM_UNTAGGED IM_BYE text IM_CRLF { REPORT(processor->Bye()); }
				;

response_data:	  IM_UNTAGGED untagged_data IM_CRLF
				;

untagged_data:	  resp_cond_state { REPORT(processor->UntaggedState($1.state, $1.code, $1.freetext)); }
				| mailbox_data
				| message_data
				| capability_data { processor->Capability($1); }
				| namespace_data
				| enabled_data    { processor->Enabled($1); }
				| id_response
				;

namespace_data:	  IM_NAMESPACE IM_SP namespace IM_SP namespace IM_SP namespace { REPORT(processor->Namespace($3, $5, $7)); }
				;

namespace:		  IM_NIL { NEW_ELEMENT($$, ImapVector, ()); }
				| '(' namespace_desc namespace_desc_list ')' { REPORT($3->Add($2)); $$ = $3; }
				;

namespace_desc:	  '(' string IM_SP delimiter namespace_ext_list ')' /* namespace extensions ignored */
					{ NEW_ELEMENT($$, ImapNamespaceDesc, ($2, $4));  }
				;

namespace_desc_list:
				  /* empty */ { NEW_ELEMENT($$, ImapVector, ()); }
				| namespace_desc_list namespace_desc { REPORT($1->Add($2)); $$ = $1; }
				;

namespace_ext_list:
				  /* empty */
				| namespace_ext_list namespace_ext
				;

namespace_ext:	  IM_SP string IM_SP '(' string string_list ')'
				;

resp_cond_state:  state_word resp_text { $$ = $2; $$.state = $1; }
				;

state_word:		  IM_STATE_OK { $$ = IM_STATE_OK; }
				| IM_STATE_BAD { $$ = IM_STATE_BAD; }
				| IM_STATE_NO { $$ = IM_STATE_NO; }
				;

mailbox_data:	  IM_FLAGS IM_SP '(' flag_list ')' { REPORT(processor->Flags($4.flags, $4.keywords, FALSE)); }
				| IM_LIST IM_SP mailbox_list { REPORT(processor->List($3.flags, $3.delimiter, $3.mailbox)); }
				| IM_LSUB IM_SP mailbox_list { REPORT(processor->List($3.flags, $3.delimiter, $3.mailbox, TRUE)); }
				| IM_SEARCH search_data { REPORT(processor->Search($2)); }
				| IM_STATUS IM_SP mailbox IM_SP illegal_mailbox '(' status_att_list ')' illegal_space { /* handled further down */}
				| number IM_SP IM_EXISTS { REPORT(processor->Exists($1)); }
				| number IM_SP IM_RECENT { REPORT(processor->Recent($1)); }
				|
				;

illegal_space:	  /* empty*/
				| IM_SP  /* DSK-333891 2007 Exchange servers */ 
				;

illegal_mailbox:  /* empty */
				| mailbox IM_SP illegal_mailbox
				  /* This is only here to allow for incorrect servers that return mailbox names
					 with spaces in them, but don't do correct quoting */
				;

status_att_list:  /* empty */
				| status_att status_att_lst
				;

status_att_lst:  /* empty */
				| status_att_lst IM_SP status_att
				;

status_att:		  IM_MESSAGES IM_SP number { REPORT(processor->Status(IM_MESSAGES, $3)); }
				| IM_RECENT IM_SP number { REPORT(processor->Status(IM_RECENT, $3)); }
				| IM_UIDNEXT IM_SP number { REPORT(processor->Status(IM_UIDNEXT, $3)); }
				| IM_UIDVALIDITY IM_SP number { REPORT(processor->Status(IM_UIDVALIDITY, $3)); }
				| IM_UNSEEN IM_SP number { REPORT(processor->Status(IM_UNSEEN, $3)); }
				| IM_HIGHESTMODSEQ IM_SP number { REPORT(processor->Status(IM_HIGHESTMODSEQ, $3)); }
				;

flag_list:		  /* empty */ { $$.flags = 0; $$.keywords = NULL; }
				| flag flag_lst { $$ = $2; $$.flags |= $1; }
				| IM_KEYWORD flag_lst
				  {
					$$ = $2;
					NEW_ELEMENT_IF_NEEDED($$.keywords, ImapSortedVector, ());
					REPORT($$.keywords->Insert($1));
				  }
				| error flag_lst { $$ = $2; }
				;

flag_lst:		  /* empty */ { $$.flags = 0; $$.keywords = NULL; }
				| flag_lst IM_SP flag { $$ = $1; $$.flags |= $3; }
				| flag_lst IM_SP IM_KEYWORD
				  {
					$$ = $1;
					NEW_ELEMENT_IF_NEEDED($$.keywords, ImapSortedVector, ());
					REPORT($$.keywords->Insert($3));
				  }
				;

flag:			  IM_FLAG_ANSWERED { $$ = ImapFlags::FLAG_ANSWERED; }
				| IM_FLAG_FLAGGED { $$ = ImapFlags::FLAG_FLAGGED; }
				| IM_FLAG_DELETED { $$ = ImapFlags::FLAG_DELETED; }
				| IM_FLAG_SEEN { $$ = ImapFlags::FLAG_SEEN; }
				| IM_FLAG_DRAFT { $$ = ImapFlags::FLAG_DRAFT; }
				| IM_FLAG_STAR { $$ = ImapFlags::FLAG_STAR; }
				| IM_FLAG_RECENT { $$ = ImapFlags::FLAG_RECENT; }
				| IM_FLAG_EXTENSION { $$ = ImapFlags::NONE; /* ignored */ }
				| IM_FLAG_IGNORE { $$ = ImapFlags::NONE; /* ignored */ }
				| IM_FLAG_SPAM { $$ = ImapFlags::FLAG_SPAM; }
				| IM_FLAG_NOT_SPAM { $$ = ImapFlags::FLAG_NOT_SPAM; }
				;

search_data:	  /* empty */ { NEW_ELEMENT($$, ImapNumberVector, ()); }
				| IM_SP nznumber_list search_sort_mod_seq { $$ = $2; } /* hack in place so that 'IM_SP' by itself (without list) is valid as well, bug #285481 */
				;

search_sort_mod_seq: /* empty */
				| '(' IM_MODSEQ IM_SP IM_NZNUMBER ')'
				;

nznumber_list:	  /* empty */ { NEW_ELEMENT($$, ImapNumberVector, ()); }
				| IM_NZNUMBER nznumber_lst { REPORT($2->Add($1)); $$ = $2; }
				;

nznumber_lst:	  /* empty */ { NEW_ELEMENT($$, ImapNumberVector, ()); }
				| nznumber_lst IM_SP IM_NZNUMBER { REPORT($1->Add($3)); $$ = $1; }
				;

mailbox_list:	'(' mbx_list_flags ')' IM_SP delimiter IM_SP mailbox { $$.flags = $2; $$.delimiter = $5; $$.mailbox = $7; }
				;

message_data:	  number IM_SP IM_EXPUNGE { REPORT(processor->Expunge($1)); }
				| number IM_SP IM_FETCH IM_SP
					{ processor->ResetMessage(); processor->SetMessageServerId($1); }
				  msg_att
					{ REPORT(processor->Fetch()); }
				| IM_VANISHED IM_SP '(' IM_EARLIER ')' IM_SP sequence_set { REPORT(processor->Vanished($7, TRUE)); }
				| IM_VANISHED IM_SP sequence_set { REPORT(processor->Vanished($3, FALSE)); }
				;

msg_att:		  '(' msg_att_list ')'
				| '(' error ')'
				;

msg_att_list:	  /* empty */
				| msg_att_dynamic msg_att_lst
				| msg_att_static msg_att_lst
				;

msg_att_lst:	  /* empty */
				| msg_att_lst IM_SP msg_att_dynamic
				| msg_att_lst IM_SP msg_att_static
				| msg_att_lst IM_SP error
				;

msg_att_dynamic:  IM_FLAGS IM_SP '(' flag_list ')'
				  {
					processor->SetMessageFlags($4.flags);
					processor->SetMessageKeywords($4.keywords);
				  }
				| fetch_mod_resp
				;

fetch_mod_resp:	  IM_MODSEQ IM_SP '(' IM_NZNUMBER ')' { processor->SetMessageModSeq($4); }
				;

msg_att_static:	  IM_ENVELOPE IM_SP envelope { /* ignored */ }
				| IM_INTERNALDATE IM_SP date_time { /* ignored */ }
				| IM_RFC822 IM_SP nstring { processor->SetMessageRawText(ImapSection::COMPLETE, $3); }
				| IM_RFC822HEADER IM_SP nstring { processor->SetMessageRawText(ImapSection::HEADERS, $3); }
				| IM_RFC822TEXT IM_SP nstring { processor->SetMessageRawText(ImapSection::PART, $3); }
				| IM_RFC822SIZE IM_SP number { processor->SetMessageSize($3); }
				| IM_BODY IM_SP body { processor->SetMessageBodyStructure($3); }
				| IM_BODYSTRUCTURE IM_SP body { processor->SetMessageBodyStructure($3); }
				| IM_BODY section '<' number '.' IM_NZNUMBER '>' IM_SP nstring { processor->SetMessageRawText($2, $9); }
				| IM_BODY section IM_SP nstring { processor->SetMessageRawText($2, $4); }
				| IM_UID IM_SP uniqueid { processor->SetMessageUID($3); }
				;

section:		  '[' section_spec ']' { $$ = $2; }
				;

section_spec:	  /* empty */ { $$ = ImapSection::COMPLETE; }
				| section_msgtext
				| section_part
				;

section_msgtext:  IM_HEADER { $$ = ImapSection::HEADERS; }
				| IM_HEADERFIELDS IM_SP header_list { $$ = ImapSection::HEADERS; }
				| IM_HEADERFIELDSNOT IM_SP header_list { $$ = ImapSection::HEADERS; }
				| IM_TEXT { $$ = ImapSection::PART; }
				;

header_list:	  '(' header_fld_name header_fld_name_list ')'
				| '(' error ')'
				;

header_fld_name_list:  /* empty */
				| header_fld_name_list IM_SP header_fld_name
				;

header_fld_name:  astring
				;

section_part:	  IM_NZNUMBER section_part_list { $$ = $2; }
				;

section_part_list:
				  /* empty */ { $$ = ImapSection::PART; }
				| section_part_list '.' section_part_spec { $$ = $3; }
				;

section_part_spec:
				  section_text
				| IM_NZNUMBER { $$ = ImapSection::PART; }
				;

section_text:	  section_msgtext
				| IM_MIME { $$ = ImapSection::MIME; }
				;

uniqueid:		  IM_NZNUMBER
				;

envelope:		  '(' nstring IM_SP nstring IM_SP env_address IM_SP env_address IM_SP env_address IM_SP env_address IM_SP env_address IM_SP env_address IM_SP nstring IM_SP nstring ')'
					{ NEW_ELEMENT($$, ImapEnvelope, ($2, $4, $6, $8, $10, $12, $14, $16, $18, $20)); }
				;

date_time:		  '"' date_day_fixed '-' IM_MONTH '-' date_year IM_SP IM_DIGIT2 ':' IM_DIGIT2 ':' IM_DIGIT2 IM_SP IM_ZONE '"'
					{ NEW_ELEMENT($$, ImapDateTime, ($2, $4, $6, $8, $10, $12, $14)); }
				;

body:			  '(' body_type_1part ')' { $$ = $2; }
				| '(' body_type_mpart ')' { $$ = $2; }
				| '(' error ')'           { $$ = NULL; }
				;

body_type_1part:  media_type IM_SP media_subtype IM_SP body_type_basic { NEW_ELEMENT($$, ImapBody, ($1, $3)); }
				| IM_MESSAGE IM_SP media_subtype IM_SP body_type_basic { NEW_ELEMENT($$, ImapBody, (IM_MESSAGE, $3)); }
				| IM_MESSAGE IM_SP IM_MEDIARFC822 IM_SP body_type_msg { NEW_ELEMENT($$, ImapBody, (IM_MESSAGE, IM_MEDIARFC822, $5)); }
				| body_type_text
				;

body_type_mpart:  body body_list IM_SP media_subtype opt_ext_mpart
					{ REPORT($2->Insert(0, $1)); NEW_ELEMENT($$, ImapBody, (0, $4, $2)); }
				;

body_list:		  /* empty */ { NEW_ELEMENT($$, ImapVector, ()); }
				| body_list body { REPORT($1->Add($2)); $$ = $1; }
				;

opt_ext_1part:	  /* empty */
				| IM_SP body_ext_1part
				;

opt_ext_mpart:	  /* empty */
				| IM_SP body_ext_mpart
				;

body_ext_mpart:	  body_fld_param opt_ext_dsp
				;

body_type_basic:  body_fields opt_ext_1part
				;

body_type_msg:	  body_fields IM_SP envelope IM_SP body IM_SP body_fld_lines opt_ext_1part { $$ = $5; }
				;

body_fld_lines:	  number
				;

body_fields:	  body_fld_param IM_SP body_fld_id IM_SP body_fld_desc IM_SP body_fld_enc IM_SP body_fld_octets
				;

body_fld_param:	  '(' string IM_SP string string_list ')' { REPORT($5->Add($2)); REPORT($5->Add($4)); $$ = $5; }
				| '(' error ')' { NEW_ELEMENT($$, ImapVector, ()); }
				| IM_NIL { NEW_ELEMENT($$, ImapVector, ()); }
				;

string_list:	  /* empty */ { NEW_ELEMENT($$, ImapVector, ()); }
				| string_list IM_SP string { REPORT($1->Add($3)); $$ = $1; }
				;

body_fld_id:	  nstring
				;

body_fld_desc:	  nstring
				;

body_fld_enc:	  IM_BIT7 { NEW_ELEMENT($$, ImapUnion, (IM_BIT7)); }
				| IM_BIT8 { NEW_ELEMENT($$, ImapUnion, (IM_BIT8)); }
				| IM_BINARY { NEW_ELEMENT($$, ImapUnion, (IM_BINARY)); }
				| IM_BASE64 { NEW_ELEMENT($$, ImapUnion, (IM_BASE64)); }
				| IM_QUOTEDPRINTABLE { NEW_ELEMENT($$, ImapUnion, (IM_QUOTEDPRINTABLE)); }
				| string { $$ = $1; }
				;

body_fld_octets:  number
				;

media_type:		  IM_APPLICATION { $$ = IM_APPLICATION; }
				| IM_AUDIO { $$ = IM_AUDIO; }
				| IM_IMAGE { $$ = IM_IMAGE; }
				| IM_VIDEO { $$ = IM_VIDEO; }
				| string { $$ = 0; }
				;

body_type_text:	  IM_TEXT IM_SP media_subtype IM_SP body_fields IM_SP body_fld_lines opt_ext_1part
				  {
					NEW_ELEMENT($$, ImapBody, (IM_TEXT, $3));
				  }
				;

media_subtype:	  IM_ARCHIVE { $$ = IM_ARCHIVE; }
				| IM_OGG { $$ = IM_OGG; }
				| IM_APPLICATION { $$ = 0; }
				| IM_AUDIO { $$ = 0; }
				| IM_IMAGE { $$ = 0; }
				| IM_MESSAGE { $$ = 0; }
				| IM_VIDEO { $$ = 0; }
				| IM_BIT7 { $$ = 0; }
				| IM_BIT8 { $$ = 0; }
				| IM_BINARY { $$ = 0; }
				| IM_BASE64 { $$ = 0; }
				| IM_QUOTEDPRINTABLE { $$ = 0; }
				| IM_TEXT { $$ = 0; }
				| IM_ALTERNATIVE { $$ = IM_ALTERNATIVE; }
				| string { $$ = 0; }
				;

body_ext_1part:	  body_fld_md5 opt_ext_dsp
				;

body_fld_md5:	  nstring
				;

opt_ext_dsp:	  /* empty */
				| IM_SP body_fld_dsp opt_ext_lang
				;

opt_ext_lang:	  /* empty */
				| IM_SP body_fld_lang opt_ext_loc
				;

body_fld_lang:	  nstring
				| '(' string string_list ')'
				| '(' error ')'
				;

opt_ext_loc:	  /* empty */
				| IM_SP body_fld_loc body_extension_list
				;

body_fld_loc:	  nstring
				;

body_fld_dsp:	  '(' string IM_SP body_fld_param ')'
				| '(' error ')'
				| IM_NIL
				;

body_extension_list:  /* empty */
				| body_extension_list IM_SP body_extension
				;

body_extension:	  nstring
				| number
				| '(' body_extension body_extension_list ')'
				| '(' error ')'
				;

date_day_fixed:	  IM_SP IM_DIGIT { $$ = $2; }
				| IM_DIGIT2
				;

date_year:		  IM_DIGIT4
				;

env_address:	  '(' address_list ')' { $$ = $2; }
				| IM_NIL { NEW_ELEMENT($$, ImapVector, ()); }
				;

address:		  '(' nstring IM_SP nstring IM_SP nstring IM_SP nstring ')'
					{ NEW_ELEMENT($$, ImapAddress, ($2, $4, $6, $8)); }
				;

address_list:	  /* empty */ { NEW_ELEMENT($$, ImapVector, ()); }
				| address_list address { REPORT($1->Add($2)); $$ = $1; }
				;

nstring:		  string { $$ = $1; }
				| IM_NIL { $$ = NULL; }
				;

mbx_list_flags:	  /* empty */ { $$ = ImapFlags::NONE; }
				| mbx_list_flag mbx_lst_flags { $$ = $2 | $1; }
				;

mbx_lst_flags:	  /* empty */ { $$ = ImapFlags::NONE; }
				| mbx_lst_flags IM_SP mbx_list_flag { $$ = $1 | $3; }
				;

mbx_list_flag:	  mbx_list_oflag { $$ = $1; }
				| mbx_list_sflag { $$ = $1; }
				;

mbx_list_oflag:	  IM_LFLAG_NOINFERIORS { $$ = ImapFlags::LFLAG_NOINFERIORS; }
				| IM_FLAG_EXTENSION { /* ignored */ $$ = ImapFlags::NONE; }
				;

mbx_list_sflag:	  IM_LFLAG_NOSELECT { $$ = ImapFlags::LFLAG_NOSELECT; }
				| IM_LFLAG_MARKED { $$ = ImapFlags::LFLAG_MARKED;  }
				| IM_LFLAG_UNMARKED { $$ = ImapFlags::LFLAG_UNMARKED; }
				| IM_LFLAG_HASCHILDREN { $$ = ImapFlags::LFLAG_HASCHILDREN; }
				| IM_LFLAG_HASNOCHILDREN { $$ = ImapFlags::LFLAG_HASNOCHILDREN; }
				| IM_LFLAG_INBOX	{ $$ = ImapFlags::LFLAG_INBOX; }
				| IM_LFLAG_DRAFTS	{ $$ = ImapFlags::LFLAG_DRAFTS; }
				| IM_LFLAG_TRASH	{ $$ = ImapFlags::LFLAG_TRASH; }
				| IM_LFLAG_SENT	{ $$ = ImapFlags::LFLAG_SENT; }
				| IM_LFLAG_SPAM	{ $$ = ImapFlags::LFLAG_SPAM; }
				| IM_LFLAG_FLAGGED { $$ = ImapFlags::LFLAG_FLAGGED; }
				| IM_LFLAG_ALLMAIL { $$ = ImapFlags::LFLAG_ALLMAIL; }
				| IM_LFLAG_ARCHIVE { $$ = ImapFlags::LFLAG_ARCHIVE; }
				;

delimiter:		  IM_SINGLE_QUOTED_CHAR { $$ = $1->m_val_string[0]; }
				| IM_NIL { $$ = '\0' }
				;

mailbox:		  IM_INBOX
				| astring
				;

astring:		  IM_PURE_ASTRING
				| IM_ATOM
				| string
				| number
				  {
					NEW_ELEMENT($$, ImapUnion, ("", INT_STRING_SIZE));
					CHECK_MEMORY($$->m_val_string);
					op_sprintf($$->m_val_string, "%lld", (long long int)$1);
				  }
				;

astring_list:	  /* empty */ { NEW_ELEMENT($$, ImapVector, ()); }
				| astring astring_lst { REPORT($2->Add($1)); $$ = $2; }
				;

astring_lst:	  /* empty */ { NEW_ELEMENT($$, ImapVector, ()); }
				| astring_lst IM_SP astring { REPORT($1->Add($3)); $$ = $1; }
				;

string:			  quoted_string
				| IM_LITERAL
				;

quoted_string:	  IM_SINGLE_QUOTED_CHAR
				| IM_QUOTED_STRING
				;

number:			  IM_NZNUMBER
				| IM_NUMBER
				;

%%

#if defined(MSWIN)
// Needs to be on the bottom because YYDEBUG is defined in the middle of the file
void yyfprintf (FILE *hfile, char *textfmt, ...)
{
#ifdef YYDEBUG
	va_list args;
	OpString8 tmp;
	va_start(args, textfmt);
	if (OpStatus::IsSuccess(tmp.AppendVFormat(textfmt, args)))
	{
		OpString tmp16;
		tmp16.Set(tmp.CStr());
		OutputDebugString(tmp16.CStr());
	}
	va_end(args);
#endif // YDEBUG
}
#endif // MSWIN
