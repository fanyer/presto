
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton interface for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     IM_ALERT = 258,
     IM_ALTERNATIVE = 259,
     IM_APPENDUID = 260,
     IM_APPLICATION = 261,
     IM_ARCHIVE = 262,
     IM_ATOM = 263,
     IM_AUDIO = 264,
     IM_AUTH_LOGIN = 265,
     IM_AUTH_CRAMMD5 = 266,
     IM_AUTH_PLAIN = 267,
     IM_BADCHARSET = 268,
     IM_BASE64 = 269,
     IM_BINARY = 270,
     IM_BIT7 = 271,
     IM_BIT8 = 272,
     IM_BODY = 273,
     IM_BODYSTRUCTURE = 274,
     IM_BYE = 275,
     IM_CAPABILITY = 276,
     IM_CLOSED = 277,
     IM_COMPRESS_DEFLATE = 278,
     IM_COMPRESSIONACTIVE = 279,
     IM_CONDSTORE = 280,
     IM_CONT = 281,
     IM_COPYUID = 282,
     IM_CRLF = 283,
     IM_DIGIT = 284,
     IM_DIGIT2 = 285,
     IM_DIGIT4 = 286,
     IM_EARLIER = 287,
     IM_ENABLE = 288,
     IM_ENABLED = 289,
     IM_ENVELOPE = 290,
     IM_EXISTS = 291,
     IM_EXPUNGE = 292,
     IM_FETCH = 293,
     IM_FLAGS = 294,
     IM_FLAG_ANSWERED = 295,
     IM_FLAG_DELETED = 296,
     IM_FLAG_DRAFT = 297,
     IM_FLAG_EXTENSION = 298,
     IM_FLAG_FLAGGED = 299,
     IM_FLAG_RECENT = 300,
     IM_FLAG_SEEN = 301,
     IM_FLAG_STAR = 302,
     IM_FLAG_IGNORE = 303,
     IM_FLAG_SPAM = 304,
     IM_FLAG_NOT_SPAM = 305,
     IM_FREETEXT = 306,
     IM_HEADER = 307,
     IM_HEADERFIELDS = 308,
     IM_HEADERFIELDSNOT = 309,
     IM_HIGHESTMODSEQ = 310,
     IM_ID = 311,
     IM_IDLE = 312,
     IM_IMAGE = 313,
     IM_IMAP4 = 314,
     IM_IMAP4REV1 = 315,
     IM_INBOX = 316,
     IM_INTERNALDATE = 317,
     IM_KEYWORD = 318,
     IM_LFLAG_ALLMAIL = 319,
     IM_LFLAG_ARCHIVE = 320,
     IM_LFLAG_DRAFTS = 321,
     IM_LFLAG_FLAGGED = 322,
     IM_LFLAG_HASCHILDREN = 323,
     IM_LFLAG_HASNOCHILDREN = 324,
     IM_LFLAG_MARKED = 325,
     IM_LFLAG_INBOX = 326,
     IM_LFLAG_NOINFERIORS = 327,
     IM_LFLAG_NOSELECT = 328,
     IM_LFLAG_SENT = 329,
     IM_LFLAG_SPAM = 330,
     IM_LFLAG_TRASH = 331,
     IM_LFLAG_UNMARKED = 332,
     IM_LIST = 333,
     IM_LITERAL = 334,
     IM_LITERALPLUS = 335,
     IM_LOGINDISABLED = 336,
     IM_LSUB = 337,
     IM_MEDIARFC822 = 338,
     IM_MESSAGE = 339,
     IM_MESSAGES = 340,
     IM_MIME = 341,
     IM_MODIFIED = 342,
     IM_MODSEQ = 343,
     IM_MONTH = 344,
     IM_NAMESPACE = 345,
     IM_NIL = 346,
     IM_NOMODSEQ = 347,
     IM_NUMBER = 348,
     IM_NZNUMBER = 349,
     IM_OGG = 350,
     IM_PARSE = 351,
     IM_PERMANENTFLAGS = 352,
     IM_PURE_ASTRING = 353,
     IM_QRESYNC = 354,
     IM_QUOTEDPRINTABLE = 355,
     IM_QUOTED_STRING = 356,
     IM_READONLY = 357,
     IM_READWRITE = 358,
     IM_RECENT = 359,
     IM_RFC822 = 360,
     IM_RFC822HEADER = 361,
     IM_RFC822SIZE = 362,
     IM_RFC822TEXT = 363,
     IM_SEARCH = 364,
     IM_SINGLE_QUOTED_CHAR = 365,
     IM_SP = 366,
     IM_SPECIALUSE = 367,
     IM_STARTTLS = 368,
     IM_STATE_BAD = 369,
     IM_STATE_NO = 370,
     IM_STATE_OK = 371,
     IM_STATUS = 372,
     IM_TAG = 373,
     IM_TEXT = 374,
     IM_TRYCREATE = 375,
     IM_UID = 376,
     IM_UIDNEXT = 377,
     IM_UIDPLUS = 378,
     IM_UIDVALIDITY = 379,
     IM_UNSEEN = 380,
     IM_UNSELECT = 381,
     IM_UNTAGGED = 382,
     IM_VANISHED = 383,
     IM_VIDEO = 384,
     IM_XLIST = 385,
     IM_ZONE = 386
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 1676 of yacc.c  */
#line 302 "imap-parse.y"

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



/* Line 1676 of yacc.c  */
#line 226 "imap-parse.hpp"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif




