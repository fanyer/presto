
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0

/* Substitute the variable and function names.  */
#define yyparse         IMAP4parse
#define yylex           IMAP4lex
#define yyerror         IMAP4error
#define yylval          IMAP4lval
#define yychar          IMAP4char
#define yydebug         IMAP4debug
#define yynerrs         IMAP4nerrs


/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 1 "imap-parse.y"

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



/* Line 189 of yacc.c  */
#line 169 "imap-parse.cpp"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


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

/* Line 214 of yacc.c  */
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



/* Line 214 of yacc.c  */
#line 379 "imap-parse.cpp"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 391 "imap-parse.cpp"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   608

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  143
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  117
/* YYNRULES -- Number of rules.  */
#define YYNRULES  319
/* YYNRULES -- Number of states.  */
#define YYNSTATES  594

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   386

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,   141,     2,     2,     2,     2,     2,
     132,   133,     2,     2,   135,   142,   137,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,   134,     2,
     136,     2,   138,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   139,     2,   140,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     4,     7,     9,    11,    13,    16,    20,
      23,    25,    26,    28,    30,    36,    39,    41,    49,    51,
      57,    59,    61,    63,    67,    71,    75,    79,    81,    85,
      87,    89,    90,    95,    98,    99,   102,   105,   106,   110,
     113,   114,   117,   118,   122,   125,   127,   129,   131,   133,
     135,   137,   139,   141,   143,   145,   147,   149,   151,   153,
     155,   157,   159,   161,   163,   165,   169,   176,   178,   179,
     185,   188,   190,   192,   196,   201,   205,   207,   209,   211,
     213,   215,   217,   219,   227,   229,   234,   241,   242,   245,
     246,   249,   257,   260,   262,   264,   266,   272,   276,   280,
     283,   293,   297,   301,   302,   303,   305,   306,   310,   311,
     314,   315,   319,   323,   327,   331,   335,   339,   343,   344,
     347,   350,   353,   354,   358,   362,   364,   366,   368,   370,
     372,   374,   376,   378,   380,   382,   384,   385,   389,   390,
     396,   397,   400,   401,   405,   413,   417,   418,   425,   433,
     437,   441,   445,   446,   449,   452,   453,   457,   461,   465,
     471,   473,   479,   483,   487,   491,   495,   499,   503,   507,
     511,   521,   526,   530,   534,   535,   537,   539,   541,   545,
     549,   551,   556,   560,   561,   565,   567,   570,   571,   575,
     577,   579,   581,   583,   585,   607,   623,   627,   631,   635,
     641,   647,   653,   655,   661,   662,   665,   666,   669,   670,
     673,   676,   679,   688,   690,   700,   707,   711,   713,   714,
     718,   720,   722,   724,   726,   728,   730,   732,   734,   736,
     738,   740,   742,   744,   746,   755,   757,   759,   761,   763,
     765,   767,   769,   771,   773,   775,   777,   779,   781,   783,
     785,   788,   790,   791,   795,   796,   800,   802,   807,   811,
     812,   816,   818,   824,   828,   830,   831,   835,   837,   839,
     844,   848,   851,   853,   855,   859,   861,   871,   872,   875,
     877,   879,   880,   883,   884,   888,   890,   892,   894,   896,
     898,   900,   902,   904,   906,   908,   910,   912,   914,   916,
     918,   920,   922,   924,   926,   928,   930,   932,   934,   936,
     938,   939,   942,   943,   947,   949,   951,   953,   955,   957
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     144,     0,    -1,    -1,   144,   145,    -1,   146,    -1,   166,
      -1,   163,    -1,     1,    28,    -1,    26,   147,    28,    -1,
     149,   148,    -1,   148,    -1,    -1,    51,    -1,     3,    -1,
       5,   111,    94,   111,   208,    -1,    13,   150,    -1,   155,
      -1,    27,   111,    94,   111,   153,   111,   153,    -1,    96,
      -1,    97,   111,   132,   182,   133,    -1,   102,    -1,   103,
      -1,   120,    -1,   122,   111,    94,    -1,   124,   111,    94,
      -1,   125,   111,   259,    -1,    55,   111,    94,    -1,    92,
      -1,    87,   111,   153,    -1,    22,    -1,    24,    -1,    -1,
     111,   132,   255,   133,    -1,    94,   152,    -1,    -1,   134,
      94,    -1,   151,   154,    -1,    -1,   154,   135,   151,    -1,
      21,   156,    -1,    -1,   111,   157,    -1,    -1,   157,   158,
     111,    -1,   157,   158,    -1,    12,    -1,    10,    -1,    11,
      -1,    23,    -1,    57,    -1,    60,    -1,    59,    -1,    80,
      -1,    81,    -1,    90,    -1,   113,    -1,   123,    -1,   126,
      -1,    25,    -1,    33,    -1,    99,    -1,   130,    -1,   112,
      -1,    56,    -1,     8,    -1,    56,   111,   160,    -1,   132,
     257,   111,   246,   161,   133,    -1,    91,    -1,    -1,   111,
     257,   111,   246,   161,    -1,    34,   156,    -1,   164,    -1,
     165,    -1,   118,   174,    28,    -1,   127,    20,   148,    28,
      -1,   127,   167,    28,    -1,   174,    -1,   176,    -1,   190,
      -1,   155,    -1,   168,    -1,   162,    -1,   159,    -1,    90,
     111,   169,   111,   169,   111,   169,    -1,    91,    -1,   132,
     170,   171,   133,    -1,   132,   257,   111,   252,   172,   133,
      -1,    -1,   171,   170,    -1,    -1,   172,   173,    -1,   111,
     257,   111,   132,   257,   223,   133,    -1,   175,   147,    -1,
     116,    -1,   114,    -1,   115,    -1,    39,   111,   132,   182,
     133,    -1,    78,   111,   189,    -1,    82,   111,   189,    -1,
     109,   185,    -1,   117,   111,   253,   111,   178,   132,   179,
     133,   177,    -1,   259,   111,    36,    -1,   259,   111,   104,
      -1,    -1,    -1,   111,    -1,    -1,   253,   111,   178,    -1,
      -1,   181,   180,    -1,    -1,   180,   111,   181,    -1,    85,
     111,   259,    -1,   104,   111,   259,    -1,   122,   111,   259,
      -1,   124,   111,   259,    -1,   125,   111,   259,    -1,    55,
     111,   259,    -1,    -1,   184,   183,    -1,    63,   183,    -1,
       1,   183,    -1,    -1,   183,   111,   184,    -1,   183,   111,
      63,    -1,    40,    -1,    44,    -1,    41,    -1,    46,    -1,
      42,    -1,    47,    -1,    45,    -1,    43,    -1,    48,    -1,
      49,    -1,    50,    -1,    -1,   111,   187,   186,    -1,    -1,
     132,    88,   111,    94,   133,    -1,    -1,    94,   188,    -1,
      -1,   188,   111,    94,    -1,   132,   247,   133,   111,   252,
     111,   253,    -1,   259,   111,    37,    -1,    -1,   259,   111,
      38,   111,   191,   192,    -1,   128,   111,   132,    32,   133,
     111,   153,    -1,   128,   111,   153,    -1,   132,   193,   133,
      -1,   132,     1,   133,    -1,    -1,   195,   194,    -1,   197,
     194,    -1,    -1,   194,   111,   195,    -1,   194,   111,   197,
      -1,   194,   111,     1,    -1,    39,   111,   132,   182,   133,
      -1,   196,    -1,    88,   111,   132,    94,   133,    -1,    35,
     111,   209,    -1,    62,   111,   210,    -1,   105,   111,   246,
      -1,   106,   111,   246,    -1,   108,   111,   246,    -1,   107,
     111,   259,    -1,    18,   111,   211,    -1,    19,   111,   211,
      -1,    18,   198,   136,   259,   137,    94,   138,   111,   246,
      -1,    18,   198,   111,   246,    -1,   121,   111,   208,    -1,
     139,   199,   140,    -1,    -1,   200,    -1,   204,    -1,    52,
      -1,    53,   111,   201,    -1,    54,   111,   201,    -1,   119,
      -1,   132,   203,   202,   133,    -1,   132,     1,   133,    -1,
      -1,   202,   111,   203,    -1,   254,    -1,    94,   205,    -1,
      -1,   205,   137,   206,    -1,   207,    -1,    94,    -1,   200,
      -1,    86,    -1,    94,    -1,   132,   246,   111,   246,   111,
     243,   111,   243,   111,   243,   111,   243,   111,   243,   111,
     243,   111,   246,   111,   246,   133,    -1,   141,   241,   142,
      89,   142,   242,   111,    30,   134,    30,   134,    30,   111,
     131,   141,    -1,   132,   212,   133,    -1,   132,   213,   133,
      -1,   132,     1,   133,    -1,   228,   111,   230,   111,   218,
      -1,    84,   111,   230,   111,   218,    -1,    84,   111,    83,
     111,   219,    -1,   229,    -1,   211,   214,   111,   230,   216,
      -1,    -1,   214,   211,    -1,    -1,   111,   231,    -1,    -1,
     111,   217,    -1,   222,   233,    -1,   221,   215,    -1,   221,
     111,   209,   111,   211,   111,   220,   215,    -1,   259,    -1,
     222,   111,   224,   111,   225,   111,   226,   111,   227,    -1,
     132,   257,   111,   257,   223,   133,    -1,   132,     1,   133,
      -1,    91,    -1,    -1,   223,   111,   257,    -1,   246,    -1,
     246,    -1,    16,    -1,    17,    -1,    15,    -1,    14,    -1,
     100,    -1,   257,    -1,   259,    -1,     6,    -1,     9,    -1,
      58,    -1,   129,    -1,   257,    -1,   119,   111,   230,   111,
     221,   111,   220,   215,    -1,     7,    -1,    95,    -1,     6,
      -1,     9,    -1,    58,    -1,    84,    -1,   129,    -1,    16,
      -1,    17,    -1,    15,    -1,    14,    -1,   100,    -1,   119,
      -1,     4,    -1,   257,    -1,   232,   233,    -1,   246,    -1,
      -1,   111,   238,   234,    -1,    -1,   111,   235,   236,    -1,
     246,    -1,   132,   257,   223,   133,    -1,   132,     1,   133,
      -1,    -1,   111,   237,   239,    -1,   246,    -1,   132,   257,
     111,   222,   133,    -1,   132,     1,   133,    -1,    91,    -1,
      -1,   239,   111,   240,    -1,   246,    -1,   259,    -1,   132,
     240,   239,   133,    -1,   132,     1,   133,    -1,   111,    29,
      -1,    30,    -1,    31,    -1,   132,   245,   133,    -1,    91,
      -1,   132,   246,   111,   246,   111,   246,   111,   246,   133,
      -1,    -1,   245,   244,    -1,   257,    -1,    91,    -1,    -1,
     249,   248,    -1,    -1,   248,   111,   249,    -1,   250,    -1,
     251,    -1,    72,    -1,    43,    -1,    73,    -1,    70,    -1,
      77,    -1,    68,    -1,    69,    -1,    71,    -1,    66,    -1,
      76,    -1,    74,    -1,    75,    -1,    67,    -1,    64,    -1,
      65,    -1,   110,    -1,    91,    -1,    61,    -1,   254,    -1,
      98,    -1,     8,    -1,   257,    -1,   259,    -1,    -1,   254,
     256,    -1,    -1,   256,   111,   254,    -1,   258,    -1,    79,
      -1,   110,    -1,   101,    -1,    94,    -1,    93,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   345,   345,   346,   349,   350,   351,   352,   355,   358,
     359,   362,   363,   366,   367,   368,   369,   370,   371,   372,
     373,   374,   375,   376,   377,   378,   379,   380,   381,   382,
     383,   386,   387,   390,   393,   394,   397,   408,   409,   420,
     423,   424,   427,   428,   429,   432,   433,   434,   435,   436,
     437,   438,   439,   440,   441,   442,   443,   444,   445,   446,
     447,   448,   449,   450,   451,   454,   456,   457,   459,   460,
     463,   465,   466,   469,   472,   475,   478,   479,   480,   481,
     482,   483,   484,   487,   490,   491,   494,   499,   500,   503,
     505,   508,   511,   514,   515,   516,   519,   520,   521,   522,
     523,   524,   525,   526,   529,   530,   533,   534,   539,   540,
     543,   544,   547,   548,   549,   550,   551,   552,   555,   556,
     557,   563,   566,   567,   568,   576,   577,   578,   579,   580,
     581,   582,   583,   584,   585,   586,   589,   590,   593,   594,
     597,   598,   601,   602,   605,   608,   610,   609,   613,   614,
     617,   618,   621,   622,   623,   626,   627,   628,   629,   632,
     637,   640,   643,   644,   645,   646,   647,   648,   649,   650,
     651,   652,   653,   656,   659,   660,   661,   664,   665,   666,
     667,   670,   671,   674,   675,   678,   681,   685,   686,   690,
     691,   694,   695,   698,   701,   705,   709,   710,   711,   714,
     715,   716,   717,   720,   724,   725,   728,   729,   732,   733,
     736,   739,   742,   745,   748,   751,   752,   753,   756,   757,
     760,   763,   766,   767,   768,   769,   770,   771,   774,   777,
     778,   779,   780,   781,   784,   790,   791,   792,   793,   794,
     795,   796,   797,   798,   799,   800,   801,   802,   803,   804,
     807,   810,   813,   814,   817,   818,   821,   822,   823,   826,
     827,   830,   833,   834,   835,   838,   839,   842,   843,   844,
     845,   848,   849,   852,   855,   856,   859,   863,   864,   867,
     868,   871,   872,   875,   876,   879,   880,   883,   884,   887,
     888,   889,   890,   891,   892,   893,   894,   895,   896,   897,
     898,   899,   902,   903,   906,   907,   910,   911,   912,   913,
     921,   922,   925,   926,   929,   930,   933,   934,   937,   938
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "IM_ALERT", "IM_ALTERNATIVE",
  "IM_APPENDUID", "IM_APPLICATION", "IM_ARCHIVE", "IM_ATOM", "IM_AUDIO",
  "IM_AUTH_LOGIN", "IM_AUTH_CRAMMD5", "IM_AUTH_PLAIN", "IM_BADCHARSET",
  "IM_BASE64", "IM_BINARY", "IM_BIT7", "IM_BIT8", "IM_BODY",
  "IM_BODYSTRUCTURE", "IM_BYE", "IM_CAPABILITY", "IM_CLOSED",
  "IM_COMPRESS_DEFLATE", "IM_COMPRESSIONACTIVE", "IM_CONDSTORE", "IM_CONT",
  "IM_COPYUID", "IM_CRLF", "IM_DIGIT", "IM_DIGIT2", "IM_DIGIT4",
  "IM_EARLIER", "IM_ENABLE", "IM_ENABLED", "IM_ENVELOPE", "IM_EXISTS",
  "IM_EXPUNGE", "IM_FETCH", "IM_FLAGS", "IM_FLAG_ANSWERED",
  "IM_FLAG_DELETED", "IM_FLAG_DRAFT", "IM_FLAG_EXTENSION",
  "IM_FLAG_FLAGGED", "IM_FLAG_RECENT", "IM_FLAG_SEEN", "IM_FLAG_STAR",
  "IM_FLAG_IGNORE", "IM_FLAG_SPAM", "IM_FLAG_NOT_SPAM", "IM_FREETEXT",
  "IM_HEADER", "IM_HEADERFIELDS", "IM_HEADERFIELDSNOT", "IM_HIGHESTMODSEQ",
  "IM_ID", "IM_IDLE", "IM_IMAGE", "IM_IMAP4", "IM_IMAP4REV1", "IM_INBOX",
  "IM_INTERNALDATE", "IM_KEYWORD", "IM_LFLAG_ALLMAIL", "IM_LFLAG_ARCHIVE",
  "IM_LFLAG_DRAFTS", "IM_LFLAG_FLAGGED", "IM_LFLAG_HASCHILDREN",
  "IM_LFLAG_HASNOCHILDREN", "IM_LFLAG_MARKED", "IM_LFLAG_INBOX",
  "IM_LFLAG_NOINFERIORS", "IM_LFLAG_NOSELECT", "IM_LFLAG_SENT",
  "IM_LFLAG_SPAM", "IM_LFLAG_TRASH", "IM_LFLAG_UNMARKED", "IM_LIST",
  "IM_LITERAL", "IM_LITERALPLUS", "IM_LOGINDISABLED", "IM_LSUB",
  "IM_MEDIARFC822", "IM_MESSAGE", "IM_MESSAGES", "IM_MIME", "IM_MODIFIED",
  "IM_MODSEQ", "IM_MONTH", "IM_NAMESPACE", "IM_NIL", "IM_NOMODSEQ",
  "IM_NUMBER", "IM_NZNUMBER", "IM_OGG", "IM_PARSE", "IM_PERMANENTFLAGS",
  "IM_PURE_ASTRING", "IM_QRESYNC", "IM_QUOTEDPRINTABLE",
  "IM_QUOTED_STRING", "IM_READONLY", "IM_READWRITE", "IM_RECENT",
  "IM_RFC822", "IM_RFC822HEADER", "IM_RFC822SIZE", "IM_RFC822TEXT",
  "IM_SEARCH", "IM_SINGLE_QUOTED_CHAR", "IM_SP", "IM_SPECIALUSE",
  "IM_STARTTLS", "IM_STATE_BAD", "IM_STATE_NO", "IM_STATE_OK", "IM_STATUS",
  "IM_TAG", "IM_TEXT", "IM_TRYCREATE", "IM_UID", "IM_UIDNEXT",
  "IM_UIDPLUS", "IM_UIDVALIDITY", "IM_UNSEEN", "IM_UNSELECT",
  "IM_UNTAGGED", "IM_VANISHED", "IM_VIDEO", "IM_XLIST", "IM_ZONE", "'('",
  "')'", "':'", "','", "'<'", "'.'", "'>'", "'['", "']'", "'\"'", "'-'",
  "$accept", "responses", "response", "continue_req", "resp_text", "text",
  "resp_text_code", "badcharset", "seq_range", "seq_range_end",
  "sequence_set", "seq_set_list", "capability_data", "capability_list",
  "capability_lst", "capability", "id_response", "id_params_list",
  "id_params_lst", "enabled_data", "response_done", "response_tagged",
  "response_fatal", "response_data", "untagged_data", "namespace_data",
  "namespace", "namespace_desc", "namespace_desc_list",
  "namespace_ext_list", "namespace_ext", "resp_cond_state", "state_word",
  "mailbox_data", "illegal_space", "illegal_mailbox", "status_att_list",
  "status_att_lst", "status_att", "flag_list", "flag_lst", "flag",
  "search_data", "search_sort_mod_seq", "nznumber_list", "nznumber_lst",
  "mailbox_list", "message_data", "$@1", "msg_att", "msg_att_list",
  "msg_att_lst", "msg_att_dynamic", "fetch_mod_resp", "msg_att_static",
  "section", "section_spec", "section_msgtext", "header_list",
  "header_fld_name_list", "header_fld_name", "section_part",
  "section_part_list", "section_part_spec", "section_text", "uniqueid",
  "envelope", "date_time", "body", "body_type_1part", "body_type_mpart",
  "body_list", "opt_ext_1part", "opt_ext_mpart", "body_ext_mpart",
  "body_type_basic", "body_type_msg", "body_fld_lines", "body_fields",
  "body_fld_param", "string_list", "body_fld_id", "body_fld_desc",
  "body_fld_enc", "body_fld_octets", "media_type", "body_type_text",
  "media_subtype", "body_ext_1part", "body_fld_md5", "opt_ext_dsp",
  "opt_ext_lang", "body_fld_lang", "opt_ext_loc", "body_fld_loc",
  "body_fld_dsp", "body_extension_list", "body_extension",
  "date_day_fixed", "date_year", "env_address", "address", "address_list",
  "nstring", "mbx_list_flags", "mbx_lst_flags", "mbx_list_flag",
  "mbx_list_oflag", "mbx_list_sflag", "delimiter", "mailbox", "astring",
  "astring_list", "astring_lst", "string", "quoted_string", "number", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,   380,   381,   382,   383,   384,
     385,   386,    40,    41,    58,    44,    60,    46,    62,    91,
      93,    34,    45
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   143,   144,   144,   145,   145,   145,   145,   146,   147,
     147,   148,   148,   149,   149,   149,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   149,   149,   149,   149,   149,
     149,   150,   150,   151,   152,   152,   153,   154,   154,   155,
     156,   156,   157,   157,   157,   158,   158,   158,   158,   158,
     158,   158,   158,   158,   158,   158,   158,   158,   158,   158,
     158,   158,   158,   158,   158,   159,   160,   160,   161,   161,
     162,   163,   163,   164,   165,   166,   167,   167,   167,   167,
     167,   167,   167,   168,   169,   169,   170,   171,   171,   172,
     172,   173,   174,   175,   175,   175,   176,   176,   176,   176,
     176,   176,   176,   176,   177,   177,   178,   178,   179,   179,
     180,   180,   181,   181,   181,   181,   181,   181,   182,   182,
     182,   182,   183,   183,   183,   184,   184,   184,   184,   184,
     184,   184,   184,   184,   184,   184,   185,   185,   186,   186,
     187,   187,   188,   188,   189,   190,   191,   190,   190,   190,
     192,   192,   193,   193,   193,   194,   194,   194,   194,   195,
     195,   196,   197,   197,   197,   197,   197,   197,   197,   197,
     197,   197,   197,   198,   199,   199,   199,   200,   200,   200,
     200,   201,   201,   202,   202,   203,   204,   205,   205,   206,
     206,   207,   207,   208,   209,   210,   211,   211,   211,   212,
     212,   212,   212,   213,   214,   214,   215,   215,   216,   216,
     217,   218,   219,   220,   221,   222,   222,   222,   223,   223,
     224,   225,   226,   226,   226,   226,   226,   226,   227,   228,
     228,   228,   228,   228,   229,   230,   230,   230,   230,   230,
     230,   230,   230,   230,   230,   230,   230,   230,   230,   230,
     231,   232,   233,   233,   234,   234,   235,   235,   235,   236,
     236,   237,   238,   238,   238,   239,   239,   240,   240,   240,
     240,   241,   241,   242,   243,   243,   244,   245,   245,   246,
     246,   247,   247,   248,   248,   249,   249,   250,   250,   251,
     251,   251,   251,   251,   251,   251,   251,   251,   251,   251,
     251,   251,   252,   252,   253,   253,   254,   254,   254,   254,
     255,   255,   256,   256,   257,   257,   258,   258,   259,   259
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     2,     1,     1,     1,     2,     3,     2,
       1,     0,     1,     1,     5,     2,     1,     7,     1,     5,
       1,     1,     1,     3,     3,     3,     3,     1,     3,     1,
       1,     0,     4,     2,     0,     2,     2,     0,     3,     2,
       0,     2,     0,     3,     2,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     3,     6,     1,     0,     5,
       2,     1,     1,     3,     4,     3,     1,     1,     1,     1,
       1,     1,     1,     7,     1,     4,     6,     0,     2,     0,
       2,     7,     2,     1,     1,     1,     5,     3,     3,     2,
       9,     3,     3,     0,     0,     1,     0,     3,     0,     2,
       0,     3,     3,     3,     3,     3,     3,     3,     0,     2,
       2,     2,     0,     3,     3,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     0,     3,     0,     5,
       0,     2,     0,     3,     7,     3,     0,     6,     7,     3,
       3,     3,     0,     2,     2,     0,     3,     3,     3,     5,
       1,     5,     3,     3,     3,     3,     3,     3,     3,     3,
       9,     4,     3,     3,     0,     1,     1,     1,     3,     3,
       1,     4,     3,     0,     3,     1,     2,     0,     3,     1,
       1,     1,     1,     1,    21,    15,     3,     3,     3,     5,
       5,     5,     1,     5,     0,     2,     0,     2,     0,     2,
       2,     2,     8,     1,     9,     6,     3,     1,     0,     3,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     8,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     1,     0,     3,     0,     3,     1,     4,     3,     0,
       3,     1,     5,     3,     1,     0,     3,     1,     1,     4,
       3,     2,     1,     1,     3,     1,     9,     0,     2,     1,
       1,     0,     2,     0,     3,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       0,     2,     0,     3,     1,     1,     1,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     1,     0,    11,     0,   103,     3,     4,     6,
      71,    72,     5,     7,    13,     0,    31,    40,    29,    30,
       0,    12,     0,     0,    27,    18,     0,    20,    21,    22,
       0,     0,     0,     0,    10,    11,    16,    94,    95,    93,
       0,    11,    11,    40,     0,     0,     0,     0,     0,   319,
     318,   136,     0,     0,    79,    82,    81,     0,    80,    76,
      77,    78,     0,     0,     0,    15,    42,    39,     0,     0,
       0,     0,     0,     0,     0,     8,     9,    73,    92,     0,
      70,     0,     0,     0,     0,     0,   140,    99,     0,     0,
      75,     0,     0,   310,    41,     0,    26,    34,    37,    28,
       0,    23,    24,    25,    74,     0,    67,     0,    65,   281,
      97,    98,    84,     0,     0,   142,   138,   307,   304,   315,
     306,   317,   316,     0,   305,   308,   314,   309,     0,   149,
     101,   145,     0,   102,     0,   312,     0,    64,    46,    47,
      45,    48,    58,    59,    63,    49,    51,    50,    52,    53,
      54,    60,    62,    55,    56,    57,    61,    44,     0,     0,
      33,    36,   122,   125,   127,   129,   132,   126,   131,   128,
     130,   133,   134,   135,   122,     0,   122,     0,     0,   288,
     300,   301,   295,   299,   292,   293,   290,   294,   287,   289,
     297,   298,   296,   291,     0,   283,   285,   286,     0,    87,
       0,   141,     0,   137,   106,     0,   146,   193,    14,   311,
      32,    43,     0,    35,     0,   121,   120,    19,   119,    96,
       0,     0,   282,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    38,     0,   280,    68,   279,     0,
       0,     0,    85,    88,     0,   143,     0,   108,   106,     0,
       0,   147,   313,    17,   124,   123,     0,     0,   303,   302,
       0,   284,    89,    83,     0,     0,     0,     0,     0,     0,
       0,     0,   110,   107,   148,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   155,   160,
     155,     0,    66,     0,     0,   139,     0,     0,     0,     0,
       0,     0,   104,   109,   151,     0,   174,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   150,   153,
     154,     0,   144,     0,    86,    90,   117,   112,   113,   114,
     115,   116,   105,   100,     0,     0,   168,   177,     0,     0,
     187,   180,     0,   175,   176,     0,     0,   169,     0,   162,
       0,     0,   163,     0,   164,   165,   167,   166,   172,     0,
      68,     0,   111,     0,   229,   230,   231,     0,     0,   232,
     204,     0,     0,     0,   202,   233,     0,     0,   186,   173,
     171,     0,     0,     0,   272,     0,     0,     0,   158,   156,
     157,    69,     0,   198,     0,     0,     0,   196,   197,     0,
       0,   178,   179,     0,     0,     0,   159,   271,     0,   161,
       0,   248,   237,   235,   238,   245,   244,   242,   243,   239,
       0,   240,   236,   246,   247,   241,     0,   249,     0,     0,
     205,     0,     0,   183,   185,   192,   190,   191,   188,   189,
       0,     0,     0,   218,     0,     0,     0,   208,     0,   182,
       0,     0,     0,     0,     0,   217,     0,   201,     0,     0,
     200,   206,     0,     0,   203,   199,     0,   181,     0,   275,
     277,     0,   273,     0,     0,    91,     0,     0,     0,     0,
       0,   211,     0,   209,   252,   184,   170,     0,     0,     0,
     219,   216,     0,     0,     0,   220,   207,   252,   251,   206,
     213,     0,   210,     0,   274,   278,     0,     0,   218,     0,
       0,   250,   234,   264,     0,   254,     0,     0,     0,     0,
       0,     0,   221,     0,     0,     0,   253,     0,     0,     0,
     215,     0,     0,   263,     0,     0,   259,   256,     0,     0,
       0,   206,   225,   224,   222,   223,   226,     0,   227,     0,
       0,   218,     0,   255,     0,     0,     0,   212,     0,   262,
     258,     0,   265,   261,     0,     0,     0,   214,   228,   257,
     260,     0,     0,     0,     0,     0,     0,   195,     0,   266,
     267,   268,   276,     0,     0,   265,     0,   270,     0,     0,
     269,     0,     0,   194
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     7,     8,    33,    34,    35,    65,    98,   160,
      99,   161,    36,    67,    94,   157,    55,   108,   257,    56,
       9,    10,    11,    12,    57,    58,   114,   199,   224,   294,
     325,    40,    41,    60,   333,   228,   271,   303,   272,   175,
     215,   176,    87,   203,   116,   201,   110,    61,   231,   251,
     287,   319,   288,   289,   290,   307,   342,   343,   401,   450,
     433,   344,   378,   438,   439,   208,   349,   352,   336,   371,
     372,   396,   481,   464,   483,   460,   457,   499,   461,   459,
     454,   494,   521,   547,   567,   373,   374,   426,   496,   497,
     502,   526,   536,   553,   562,   515,   570,   579,   386,   473,
     471,   505,   487,   580,   194,   222,   195,   196,   197,   260,
     229,   124,   136,   209,   238,   126,   127
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -493
static const yytype_int16 yypact[] =
{
    -493,    17,  -493,    -9,   415,   106,   460,  -493,  -493,  -493,
    -493,  -493,  -493,  -493,  -493,   -78,   -75,   -69,  -493,  -493,
     -37,  -493,   -31,   -14,  -493,  -493,    35,  -493,  -493,  -493,
      42,    54,    61,    72,  -493,    82,  -493,  -493,  -493,  -493,
     149,   415,    82,   -69,    81,    99,   113,   119,   120,  -493,
    -493,   127,   128,   131,  -493,  -493,  -493,   152,  -493,  -493,
    -493,  -493,   132,   121,    85,  -493,  -493,  -493,   135,   151,
     160,   114,   162,   164,    -5,  -493,  -493,  -493,  -493,   229,
    -493,   143,   -70,   144,   144,   -67,   165,  -493,   118,   -59,
    -493,   -11,   153,   139,   341,   156,  -493,   148,  -493,  -493,
       4,  -493,  -493,  -493,  -493,     4,  -493,    15,  -493,   527,
    -493,  -493,  -493,   146,   170,  -493,   157,  -493,  -493,  -493,
    -493,  -493,  -493,   172,  -493,  -493,  -493,  -493,   253,  -493,
    -493,  -493,   177,  -493,   205,  -493,   167,  -493,  -493,  -493,
    -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,
    -493,  -493,  -493,  -493,  -493,  -493,  -493,   190,   160,   208,
    -493,   168,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,
    -493,  -493,  -493,  -493,  -493,   178,  -493,   182,   209,  -493,
    -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,
    -493,  -493,  -493,  -493,   183,  -493,  -493,  -493,    15,  -493,
     -67,   211,   231,  -493,   118,   191,  -493,  -493,  -493,   215,
    -493,  -493,   221,  -493,   160,   222,   222,  -493,   222,  -493,
     104,   223,   224,   227,    71,   233,   237,   239,   216,   243,
     244,   225,   139,   160,  -493,   515,  -493,   248,  -493,   -54,
     527,   -54,  -493,  -493,   -67,  -493,   266,    69,   118,   160,
      22,  -493,  -493,  -493,  -493,  -493,    15,   204,  -493,  -493,
     250,  -493,  -493,  -493,   230,   251,   256,   257,   260,   262,
     267,   247,  -493,  -493,  -493,   249,   -36,   274,   277,   279,
     284,   285,   288,   291,   292,   295,   296,   275,  -493,  -493,
    -493,   299,  -493,   118,   -28,  -493,    -5,    -5,    -5,    -5,
      -5,    -5,   300,   301,  -493,   245,   147,   -32,   245,   281,
     282,   276,   283,   104,   104,    -5,   104,   205,  -493,   305,
     305,   104,  -493,    15,  -493,  -493,  -493,  -493,  -493,  -493,
    -493,  -493,  -493,  -493,    69,    57,  -493,  -493,   308,   312,
    -493,  -493,   286,  -493,  -493,   104,    -5,  -493,   104,  -493,
       4,   -15,  -493,   330,  -493,  -493,  -493,  -493,  -493,   163,
     248,   314,  -493,   297,  -493,  -493,  -493,   318,   321,  -493,
    -493,   310,   311,   322,  -493,  -493,   302,   302,   304,  -493,
    -493,   309,   327,   316,  -493,   418,   313,   317,  -493,  -493,
    -493,  -493,   319,  -493,   246,   377,   -25,  -493,  -493,   377,
      77,  -493,  -493,   220,   358,   104,  -493,  -493,   368,  -493,
      15,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,
     347,  -493,  -493,  -493,  -493,  -493,   348,  -493,   349,   377,
    -493,   351,   332,  -493,  -493,  -493,  -493,  -493,  -493,  -493,
     325,   357,   331,  -493,   -63,   -63,   -63,   363,   -63,  -493,
     -13,   364,   -62,   438,    27,  -493,    11,  -493,   371,   372,
    -493,   373,   374,   -63,  -493,  -493,   139,  -493,   104,  -493,
    -493,   375,  -493,   378,    15,  -493,   346,   379,   281,   104,
     104,  -493,    -5,  -493,   380,  -493,  -493,    94,   -62,   458,
    -493,  -493,    15,   381,   382,  -493,  -493,   380,  -493,   373,
    -493,   -61,  -493,   104,  -493,  -493,   384,   366,  -493,   245,
     104,  -493,  -493,  -493,    12,   386,   387,   -62,   471,    51,
     392,   393,  -493,   376,   394,   226,  -493,   104,   397,   385,
    -493,    -5,   134,  -493,   -63,    13,   399,  -493,   402,   -62,
     484,   373,  -493,  -493,  -493,  -493,  -493,   404,  -493,   388,
     389,  -493,   104,  -493,   104,   409,   412,  -493,    -5,  -493,
    -493,    63,  -493,  -493,   413,   -62,   395,  -493,  -493,  -493,
     416,   104,   417,   390,   186,   396,   -62,  -493,     8,  -493,
    -493,  -493,  -493,   419,   400,  -493,   104,  -493,    74,   421,
    -493,   104,   401,  -493
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -493,  -493,  -493,  -493,   495,    -3,  -493,  -493,   329,  -493,
     -81,  -493,   535,   501,  -493,  -493,  -493,  -493,   185,  -493,
    -493,  -493,  -493,  -493,  -493,  -493,  -189,   323,  -493,  -493,
    -493,   540,  -493,  -493,  -493,   303,  -493,  -493,   214,  -103,
     -17,   336,  -493,  -493,  -493,  -493,   465,  -493,  -493,  -493,
    -493,   289,   207,  -493,   228,  -493,  -493,   179,   196,  -493,
     101,  -493,  -493,  -493,  -493,   263,   103,  -493,  -301,  -493,
    -493,  -493,  -477,  -493,  -493,   136,  -493,    52,  -256,  -453,
    -492,  -493,  -493,  -493,  -493,  -493,  -493,  -323,  -493,  -493,
      88,  -493,  -493,  -493,  -493,  -493,     1,    28,  -493,  -493,
    -457,  -493,  -493,  -182,  -493,  -493,   350,  -493,  -493,   367,
     -84,   -90,  -493,  -493,   -87,  -493,    -6
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -153
static const yytype_int16 yytable[] =
{
      62,   125,   177,   135,   123,   162,   125,   347,   129,   584,
     484,   225,   476,   523,   550,   384,   519,     2,     3,    13,
     178,   106,   512,   275,   112,   130,   131,   132,   455,   469,
     513,   506,    76,    63,   370,    97,    64,   258,   237,    79,
     276,   277,    66,     4,   163,   164,   165,   166,   167,   168,
     169,   170,   171,   172,   173,   263,   259,   278,   363,   561,
     528,   279,   107,   364,   557,   113,   365,   174,   103,   456,
     470,   514,   428,   128,    68,   305,   431,   212,   432,   345,
      69,   549,   555,   323,   280,   117,   429,   119,    49,    50,
     119,   119,   119,   133,   119,   430,   385,    70,   466,   236,
      75,    49,    50,   306,   346,   324,   447,   335,   572,   121,
     281,   223,   121,   121,   121,   366,   121,   125,   122,   583,
     467,   122,   122,   122,   265,   122,   117,   282,   283,   284,
     285,   354,   355,    21,   357,     5,   119,  -118,   474,   360,
     578,   367,   252,   286,     6,   125,    71,   117,   542,   543,
     544,   545,   253,    72,   266,  -152,   119,   216,   121,   218,
     475,   125,   474,   380,   388,    73,   382,   122,   274,   291,
      49,    50,    74,   267,   474,   120,   368,    77,   121,   118,
      90,   276,   277,   119,   530,   574,   369,   122,   458,   335,
     462,   268,    81,   269,   270,   236,   569,   119,   278,   337,
     338,   339,   279,   198,   242,   121,   125,   590,   520,   322,
      82,    49,    50,   119,   122,    92,   120,    93,   119,   121,
      37,    38,    39,   441,    83,   280,   503,   504,   122,    95,
      84,    85,    49,    50,   546,   121,   361,   120,    86,    88,
     121,   340,    89,    91,   122,    96,   100,   383,   375,   122,
     411,   281,   412,   413,    97,   414,   101,   104,   102,   115,
     415,   416,   417,   418,   134,   119,   341,   158,   282,   283,
     284,   285,   337,   338,   339,   105,   109,   236,   198,    49,
      50,   200,   159,   204,   286,   205,   486,   121,   206,   202,
     326,   327,   328,   329,   330,   331,   122,   495,   498,   207,
     210,   211,   213,   214,   419,   119,   435,   427,   427,   356,
     434,   217,   427,   125,   436,   219,   221,   236,   578,   227,
     220,   516,   226,   443,   230,   119,   232,   121,   522,   420,
     421,   245,   233,   235,   239,   240,   122,   292,   241,   341,
     381,   422,   427,   537,   244,   538,   423,   121,   247,   137,
     246,   138,   139,   140,   248,   249,   122,   250,   535,   256,
     264,   293,   296,   295,   141,   424,   142,   297,   298,   477,
     563,   299,   564,   300,   143,   425,   434,   335,   301,   125,
     302,   411,   304,   412,   413,   308,   414,   490,   309,   575,
     310,   415,   416,   417,   418,   311,   312,   144,   145,   313,
     146,   147,   314,   315,   589,   508,   316,   317,   318,   592,
     321,   332,   334,   348,   350,   353,   359,   351,    14,   376,
      15,   148,   149,   377,   387,   392,   379,   524,    16,   394,
     393,   150,   395,   399,   400,   419,    17,    18,   405,    19,
     151,   403,    20,   397,   398,   548,   404,   407,   551,   406,
     409,   410,   440,   152,   153,   408,   119,   442,   444,   445,
     446,   421,   448,   451,   154,   449,    21,   155,   452,   472,
      22,   156,   422,   453,   463,   468,   500,   423,   121,   491,
      42,    17,   478,   479,   480,   482,   488,   122,   507,   489,
     492,   501,   509,   510,    43,   517,   424,   525,   527,    44,
     518,   529,    23,   531,   532,   534,   425,    24,   539,   533,
     552,    25,    26,   554,   556,   558,    45,    27,    28,   540,
     565,   559,   560,   566,   571,   500,   573,   574,   576,   582,
     586,   577,   591,   587,   593,    29,    78,    30,    46,    31,
      32,    54,    47,   234,    80,   391,    59,   243,   362,   111,
      48,   273,   568,    49,    50,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   389,   485,   581,    51,
     179,   255,   581,   402,    37,    38,    39,    52,   254,   320,
     358,   493,   437,   541,   465,   511,   588,   390,    53,     0,
     261,   180,   181,   182,   183,   184,   185,   186,   187,   188,
     189,   190,   191,   192,   193,     0,   585,     0,   262
};

static const yytype_int16 yycheck[] =
{
       6,    88,   105,    93,    88,     1,    93,   308,    89,     1,
     463,   200,     1,     1,     1,    30,   508,     0,     1,    28,
     107,    91,   499,     1,    91,    36,    37,    38,    91,    91,
      91,   488,    35,   111,   335,    94,   111,    91,   220,    42,
      18,    19,   111,    26,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,   244,   110,    35,     1,   551,
     517,    39,   132,     6,   541,   132,     9,    63,    74,   132,
     132,   132,   395,   132,   111,   111,   399,   158,     1,   111,
     111,   534,   539,   111,    62,     8,   111,    79,    93,    94,
      79,    79,    79,   104,    79,   396,   111,   111,   111,    91,
      28,    93,    94,   139,   136,   133,   429,   132,   565,   101,
      88,   198,   101,   101,   101,    58,   101,   204,   110,   576,
     133,   110,   110,   110,    55,   110,     8,   105,   106,   107,
     108,   313,   314,    51,   316,   118,    79,   133,   111,   321,
     132,    84,   232,   121,   127,   232,   111,     8,    14,    15,
      16,    17,   233,   111,    85,   133,    79,   174,   101,   176,
     133,   248,   111,   345,     1,   111,   348,   110,   249,   256,
      93,    94,   111,   104,   111,    98,   119,    28,   101,    61,
      28,    18,    19,    79,   133,   111,   129,   110,   444,   132,
     446,   122,   111,   124,   125,    91,   133,    79,    35,    52,
      53,    54,    39,   132,   133,   101,   293,   133,   509,   293,
     111,    93,    94,    79,   110,    94,    98,   132,    79,   101,
     114,   115,   116,   405,   111,    62,   132,   133,   110,    94,
     111,   111,    93,    94,   100,   101,   323,    98,   111,   111,
     101,    94,   111,   111,   110,    94,   132,   350,   335,   110,
       4,    88,     6,     7,    94,     9,    94,    28,    94,    94,
      14,    15,    16,    17,   111,    79,   119,   111,   105,   106,
     107,   108,    52,    53,    54,   132,   132,    91,   132,    93,
      94,   111,   134,   111,   121,    32,   468,   101,   111,   132,
     296,   297,   298,   299,   300,   301,   110,   479,   480,    94,
     133,   111,    94,   135,    58,    79,    86,   394,   395,   315,
     400,   133,   399,   400,    94,   133,   133,    91,   132,    88,
     111,   503,   111,   410,   133,    79,   111,   101,   510,    83,
      84,    94,   111,   111,   111,   111,   110,   133,   111,   119,
     346,    95,   429,   525,   111,   527,   100,   101,   132,     8,
     111,    10,    11,    12,   111,   111,   110,   132,   132,   111,
      94,   111,   111,   133,    23,   119,    25,   111,   111,   456,
     552,   111,   554,   111,    33,   129,   466,   132,   111,   466,
     133,     4,   133,     6,     7,   111,     9,   474,   111,   571,
     111,    14,    15,    16,    17,   111,   111,    56,    57,   111,
      59,    60,   111,   111,   586,   492,   111,   111,   133,   591,
     111,   111,   111,   132,   132,   132,   111,   141,     3,   111,
       5,    80,    81,   111,    94,   111,   140,   514,    13,   111,
     133,    90,   111,   111,   132,    58,    21,    22,   111,    24,
      99,   137,    27,   133,   133,   532,   137,    29,   535,   133,
     133,   132,    94,   112,   113,   142,    79,    89,   111,   111,
     111,    84,   111,   138,   123,   133,    51,   126,   111,    31,
      55,   130,    95,   142,   111,   111,   482,   100,   101,   133,
      20,    21,   111,   111,   111,   111,   111,   110,    30,   111,
     111,   111,   111,   111,    34,   111,   119,   111,   111,    39,
     134,    30,    87,   111,   111,   111,   129,    92,   111,   133,
     111,    96,    97,   111,    30,   111,    56,   102,   103,   134,
     111,   133,   133,   111,   111,   531,   131,   111,   111,   133,
     111,   141,   111,   133,   133,   120,    41,   122,    78,   124,
     125,     6,    82,   214,    43,   360,     6,   224,   334,    84,
      90,   248,   558,    93,    94,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,   359,   466,   574,   109,
      43,   235,   578,   377,   114,   115,   116,   117,    63,   290,
     317,   478,   403,   531,   448,   497,   585,   359,   128,    -1,
     240,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    -1,   578,    -1,   241
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   144,     0,     1,    26,   118,   127,   145,   146,   163,
     164,   165,   166,    28,     3,     5,    13,    21,    22,    24,
      27,    51,    55,    87,    92,    96,    97,   102,   103,   120,
     122,   124,   125,   147,   148,   149,   155,   114,   115,   116,
     174,   175,    20,    34,    39,    56,    78,    82,    90,    93,
      94,   109,   117,   128,   155,   159,   162,   167,   168,   174,
     176,   190,   259,   111,   111,   150,   111,   156,   111,   111,
     111,   111,   111,   111,   111,    28,   148,    28,   147,   148,
     156,   111,   111,   111,   111,   111,   111,   185,   111,   111,
      28,   111,    94,   132,   157,    94,    94,    94,   151,   153,
     132,    94,    94,   259,    28,   132,    91,   132,   160,   132,
     189,   189,    91,   132,   169,    94,   187,     8,    61,    79,
      98,   101,   110,   253,   254,   257,   258,   259,   132,   153,
      36,    37,    38,   104,   111,   254,   255,     8,    10,    11,
      12,    23,    25,    33,    56,    57,    59,    60,    80,    81,
      90,    99,   112,   113,   123,   126,   130,   158,   111,   134,
     152,   154,     1,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    63,   182,   184,   182,   257,    43,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,   247,   249,   250,   251,   132,   170,
     111,   188,   132,   186,   111,    32,   111,    94,   208,   256,
     133,   111,   153,    94,   135,   183,   183,   133,   183,   133,
     111,   133,   248,   257,   171,   169,   111,    88,   178,   253,
     133,   191,   111,   111,   151,   111,    91,   246,   257,   111,
     111,   111,   133,   170,   111,    94,   111,   132,   111,   111,
     132,   192,   254,   153,    63,   184,   111,   161,    91,   110,
     252,   249,   252,   169,    94,    55,    85,   104,   122,   124,
     125,   179,   181,   178,   153,     1,    18,    19,    35,    39,
      62,    88,   105,   106,   107,   108,   121,   193,   195,   196,
     197,   257,   133,   111,   172,   133,   111,   111,   111,   111,
     111,   111,   133,   180,   133,   111,   139,   198,   111,   111,
     111,   111,   111,   111,   111,   111,   111,   111,   133,   194,
     194,   111,   253,   111,   133,   173,   259,   259,   259,   259,
     259,   259,   111,   177,   111,   132,   211,    52,    53,    54,
      94,   119,   199,   200,   204,   111,   136,   211,   132,   209,
     132,   141,   210,   132,   246,   246,   259,   246,   208,   111,
     246,   257,   181,     1,     6,     9,    58,    84,   119,   129,
     211,   212,   213,   228,   229,   257,   111,   111,   205,   140,
     246,   259,   246,   182,    30,   111,   241,    94,     1,   195,
     197,   161,   111,   133,   111,   111,   214,   133,   133,   111,
     132,   201,   201,   137,   137,   111,   133,    29,   142,   133,
     132,     4,     6,     7,     9,    14,    15,    16,    17,    58,
      83,    84,    95,   100,   119,   129,   230,   257,   230,   111,
     211,   230,     1,   203,   254,    86,    94,   200,   206,   207,
      94,   246,    89,   257,   111,   111,   111,   230,   111,   133,
     202,   138,   111,   142,   223,    91,   132,   219,   221,   222,
     218,   221,   221,   111,   216,   218,   111,   133,   111,    91,
     132,   243,    31,   242,   111,   133,     1,   257,   111,   111,
     111,   215,   111,   217,   222,   203,   246,   245,   111,   111,
     257,   133,   111,   209,   224,   246,   231,   232,   246,   220,
     259,   111,   233,   132,   133,   244,   243,    30,   257,   111,
     111,   233,   215,    91,   132,   238,   246,   111,   134,   223,
     211,   225,   246,     1,   257,   111,   234,   111,   243,    30,
     133,   111,   111,   133,   111,   132,   235,   246,   246,   111,
     134,   220,    14,    15,    16,    17,   100,   226,   257,   222,
       1,   257,   111,   236,   111,   243,    30,   215,   111,   133,
     133,   223,   237,   246,   246,   111,   111,   227,   259,   133,
     239,   111,   243,   131,   111,   246,   111,   141,   132,   240,
     246,   259,   133,   243,     1,   240,   111,   133,   239,   246,
     133,   111,   246,   133
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (yyscanner, processor, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, yyscanner)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, yyscanner, processor); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, yyscan_t yyscanner, ImapProcessor* processor)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, yyscanner, processor)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    yyscan_t yyscanner;
    ImapProcessor* processor;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (yyscanner);
  YYUSE (processor);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, yyscan_t yyscanner, ImapProcessor* processor)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, yyscanner, processor)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    yyscan_t yyscanner;
    ImapProcessor* processor;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yyscanner, processor);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, yyscan_t yyscanner, ImapProcessor* processor)
#else
static void
yy_reduce_print (yyvsp, yyrule, yyscanner, processor)
    YYSTYPE *yyvsp;
    int yyrule;
    yyscan_t yyscanner;
    ImapProcessor* processor;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       , yyscanner, processor);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule, yyscanner, processor); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, yyscan_t yyscanner, ImapProcessor* processor)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, yyscanner, processor)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    yyscan_t yyscanner;
    ImapProcessor* processor;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (yyscanner);
  YYUSE (processor);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}

/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (yyscan_t yyscanner, ImapProcessor* processor);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */





/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (yyscan_t yyscanner, ImapProcessor* processor)
#else
int
yyparse (yyscanner, processor)
    yyscan_t yyscanner;
    ImapProcessor* processor;
#endif
#endif
{
/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

/* User initialization code.  */

/* Line 1242 of yacc.c  */
#line 96 "imap-parse.y"
{
#if YYDEBUG
	yydebug = 1;
#endif
}

/* Line 1242 of yacc.c  */
#line 2035 "imap-parse.cpp"

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 3:

/* Line 1455 of yacc.c  */
#line 346 "imap-parse.y"
    { processor->NextCommand(); ;}
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 355 "imap-parse.y"
    { REPORT(processor->Continue((yyvsp[(2) - (3)].response_code).freetext)); ;}
    break;

  case 9:

/* Line 1455 of yacc.c  */
#line 358 "imap-parse.y"
    { (yyval.response_code) = (yyvsp[(1) - (2)].response_code); (yyval.response_code).freetext = (yyvsp[(2) - (2)].generic); ;}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 359 "imap-parse.y"
    { (yyval.response_code).code = 0; (yyval.response_code).freetext = (yyvsp[(1) - (1)].generic); ;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 362 "imap-parse.y"
    { (yyval.generic) = NULL; ;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 363 "imap-parse.y"
    { (yyval.generic) = (yyvsp[(1) - (1)].generic); ;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 366 "imap-parse.y"
    { (yyval.response_code).code = IM_ALERT; ;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 367 "imap-parse.y"
    { (yyval.response_code).code = IM_APPENDUID; REPORT(processor->AppendUid((yyvsp[(3) - (5)].number), (yyvsp[(5) - (5)].number))); ;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 368 "imap-parse.y"
    { (yyval.response_code).code = IM_BADCHARSET; ;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 369 "imap-parse.y"
    { (yyval.response_code).code = IM_CAPABILITY; processor->Capability((yyvsp[(1) - (1)].number)); ;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 370 "imap-parse.y"
    { (yyval.response_code).code = IM_COPYUID; REPORT(processor->CopyUid((yyvsp[(3) - (7)].number), (yyvsp[(5) - (7)].number_vector), (yyvsp[(7) - (7)].number_vector))); ;}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 371 "imap-parse.y"
    { (yyval.response_code).code = IM_PARSE; ;}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 372 "imap-parse.y"
    { (yyval.response_code).code = IM_PERMANENTFLAGS; REPORT(processor->Flags((yyvsp[(4) - (5)].flags).flags, (yyvsp[(4) - (5)].flags).keywords, TRUE)); ;}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 373 "imap-parse.y"
    { (yyval.response_code).code = IM_READONLY; REPORT(processor->SetReadOnly(TRUE)); ;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 374 "imap-parse.y"
    { (yyval.response_code).code = IM_READWRITE; REPORT(processor->SetReadOnly(FALSE)); ;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 375 "imap-parse.y"
    { (yyval.response_code).code = IM_TRYCREATE; /* not handled */ ;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 376 "imap-parse.y"
    { (yyval.response_code).code = IM_UIDNEXT; REPORT(processor->UidNext((yyvsp[(3) - (3)].number))); ;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 377 "imap-parse.y"
    { (yyval.response_code).code = IM_UIDVALIDITY; REPORT(processor->UidValidity((yyvsp[(3) - (3)].number))); ;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 378 "imap-parse.y"
    { (yyval.response_code).code = IM_UNSEEN; REPORT(processor->Unseen((yyvsp[(3) - (3)].number))); ;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 379 "imap-parse.y"
    { (yyval.response_code).code = IM_HIGHESTMODSEQ; REPORT(processor->HighestModSeq((yyvsp[(3) - (3)].number))); ;}
    break;

  case 27:

/* Line 1455 of yacc.c  */
#line 380 "imap-parse.y"
    { (yyval.response_code).code = IM_NOMODSEQ; REPORT(processor->NoModSeq()); ;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 381 "imap-parse.y"
    { (yyval.response_code).code = IM_MODIFIED; ;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 382 "imap-parse.y"
    { (yyval.response_code).code = IM_CLOSED; ;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 383 "imap-parse.y"
    { (yyval.response_code).code = IM_COMPRESSIONACTIVE; ;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 386 "imap-parse.y"
    { NEW_ELEMENT((yyval.vector), ImapVector, ()); ;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 387 "imap-parse.y"
    { (yyval.vector) = (yyvsp[(3) - (4)].vector); ;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 390 "imap-parse.y"
    { (yyval.range).start = (yyvsp[(1) - (2)].number); (yyval.range).end = (yyvsp[(2) - (2)].number) ? (yyvsp[(2) - (2)].number) : (yyvsp[(1) - (2)].number); ;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 393 "imap-parse.y"
    { (yyval.number) = 0; ;}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 394 "imap-parse.y"
    { (yyval.number) = (yyvsp[(2) - (2)].number); ;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 398 "imap-parse.y"
    {
					(yyval.number_vector) = (yyvsp[(2) - (2)].number_vector);
					while ((yyvsp[(1) - (2)].range).end >= (yyvsp[(1) - (2)].range).start)
					{
						REPORT((yyval.number_vector)->Insert(0, (yyvsp[(1) - (2)].range).end));
						(yyvsp[(1) - (2)].range).end--;
					}
				  ;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 408 "imap-parse.y"
    { NEW_ELEMENT((yyval.number_vector), ImapNumberVector, ()); ;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 410 "imap-parse.y"
    {
					(yyval.number_vector) = (yyvsp[(1) - (3)].number_vector);
					while ((yyvsp[(3) - (3)].range).start <= (yyvsp[(3) - (3)].range).end)
					{
						REPORT((yyval.number_vector)->Add((yyvsp[(3) - (3)].range).start));
						(yyvsp[(3) - (3)].range).start++;
					}
				  ;}
    break;

  case 39:

/* Line 1455 of yacc.c  */
#line 420 "imap-parse.y"
    { (yyval.number) = (yyvsp[(2) - (2)].number); ;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 423 "imap-parse.y"
    { (yyval.number) = ImapFlags::NONE; ;}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 424 "imap-parse.y"
    { (yyval.number) = (yyvsp[(2) - (2)].number); ;}
    break;

  case 42:

/* Line 1455 of yacc.c  */
#line 427 "imap-parse.y"
    { (yyval.number) = ImapFlags::NONE; ;}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 428 "imap-parse.y"
    { (yyval.number) = (yyvsp[(1) - (3)].number) | (yyvsp[(2) - (3)].number); ;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 429 "imap-parse.y"
    { (yyval.number) = (yyvsp[(1) - (2)].number) | (yyvsp[(2) - (2)].number); ;}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 432 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_AUTH_PLAIN; ;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 433 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_AUTH_LOGIN; ;}
    break;

  case 47:

/* Line 1455 of yacc.c  */
#line 434 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_AUTH_CRAMMD5; ;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 435 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_COMPRESS_DEFLATE; ;}
    break;

  case 49:

/* Line 1455 of yacc.c  */
#line 436 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_IDLE; ;}
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 437 "imap-parse.y"
    { (yyval.number) = ImapFlags::NONE; ;}
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 438 "imap-parse.y"
    { (yyval.number) = ImapFlags::NONE; ;}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 439 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_LITERALPLUS; ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 440 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_LOGINDISABLED; ;}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 441 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_NAMESPACE; ;}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 442 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_STARTTLS; ;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 443 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_UIDPLUS; ;}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 444 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_UNSELECT; ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 445 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_CONDSTORE; ;}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 446 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_ENABLE; ;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 447 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_QRESYNC; ;}
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 448 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_XLIST; ;}
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 449 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_SPECIAL_USE; ;}
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 450 "imap-parse.y"
    { (yyval.number) = ImapFlags::CAP_ID; ;}
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 451 "imap-parse.y"
    { (yyval.number) = ImapFlags::NONE; ;}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 463 "imap-parse.y"
    { (yyval.number) = (yyvsp[(2) - (2)].number); ;}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 469 "imap-parse.y"
    { REPORT(processor->TaggedState((yyvsp[(1) - (3)].number), (yyvsp[(2) - (3)].response_code).state, (yyvsp[(2) - (3)].response_code).code, (yyvsp[(2) - (3)].response_code).freetext)); ;}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 472 "imap-parse.y"
    { REPORT(processor->Bye()); ;}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 478 "imap-parse.y"
    { REPORT(processor->UntaggedState((yyvsp[(1) - (1)].response_code).state, (yyvsp[(1) - (1)].response_code).code, (yyvsp[(1) - (1)].response_code).freetext)); ;}
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 481 "imap-parse.y"
    { processor->Capability((yyvsp[(1) - (1)].number)); ;}
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 483 "imap-parse.y"
    { processor->Enabled((yyvsp[(1) - (1)].number)); ;}
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 487 "imap-parse.y"
    { REPORT(processor->Namespace((yyvsp[(3) - (7)].vector), (yyvsp[(5) - (7)].vector), (yyvsp[(7) - (7)].vector))); ;}
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 490 "imap-parse.y"
    { NEW_ELEMENT((yyval.vector), ImapVector, ()); ;}
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 491 "imap-parse.y"
    { REPORT((yyvsp[(3) - (4)].vector)->Add((yyvsp[(2) - (4)].name_space))); (yyval.vector) = (yyvsp[(3) - (4)].vector); ;}
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 495 "imap-parse.y"
    { NEW_ELEMENT((yyval.name_space), ImapNamespaceDesc, ((yyvsp[(2) - (6)].generic), (yyvsp[(4) - (6)].number)));  ;}
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 499 "imap-parse.y"
    { NEW_ELEMENT((yyval.vector), ImapVector, ()); ;}
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 500 "imap-parse.y"
    { REPORT((yyvsp[(1) - (2)].vector)->Add((yyvsp[(2) - (2)].name_space))); (yyval.vector) = (yyvsp[(1) - (2)].vector); ;}
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 511 "imap-parse.y"
    { (yyval.response_code) = (yyvsp[(2) - (2)].response_code); (yyval.response_code).state = (yyvsp[(1) - (2)].number); ;}
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 514 "imap-parse.y"
    { (yyval.number) = IM_STATE_OK; ;}
    break;

  case 94:

/* Line 1455 of yacc.c  */
#line 515 "imap-parse.y"
    { (yyval.number) = IM_STATE_BAD; ;}
    break;

  case 95:

/* Line 1455 of yacc.c  */
#line 516 "imap-parse.y"
    { (yyval.number) = IM_STATE_NO; ;}
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 519 "imap-parse.y"
    { REPORT(processor->Flags((yyvsp[(4) - (5)].flags).flags, (yyvsp[(4) - (5)].flags).keywords, FALSE)); ;}
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 520 "imap-parse.y"
    { REPORT(processor->List((yyvsp[(3) - (3)].mailboxlist).flags, (yyvsp[(3) - (3)].mailboxlist).delimiter, (yyvsp[(3) - (3)].mailboxlist).mailbox)); ;}
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 521 "imap-parse.y"
    { REPORT(processor->List((yyvsp[(3) - (3)].mailboxlist).flags, (yyvsp[(3) - (3)].mailboxlist).delimiter, (yyvsp[(3) - (3)].mailboxlist).mailbox, TRUE)); ;}
    break;

  case 99:

/* Line 1455 of yacc.c  */
#line 522 "imap-parse.y"
    { REPORT(processor->Search((yyvsp[(2) - (2)].number_vector))); ;}
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 523 "imap-parse.y"
    { /* handled further down */;}
    break;

  case 101:

/* Line 1455 of yacc.c  */
#line 524 "imap-parse.y"
    { REPORT(processor->Exists((yyvsp[(1) - (3)].number))); ;}
    break;

  case 102:

/* Line 1455 of yacc.c  */
#line 525 "imap-parse.y"
    { REPORT(processor->Recent((yyvsp[(1) - (3)].number))); ;}
    break;

  case 112:

/* Line 1455 of yacc.c  */
#line 547 "imap-parse.y"
    { REPORT(processor->Status(IM_MESSAGES, (yyvsp[(3) - (3)].number))); ;}
    break;

  case 113:

/* Line 1455 of yacc.c  */
#line 548 "imap-parse.y"
    { REPORT(processor->Status(IM_RECENT, (yyvsp[(3) - (3)].number))); ;}
    break;

  case 114:

/* Line 1455 of yacc.c  */
#line 549 "imap-parse.y"
    { REPORT(processor->Status(IM_UIDNEXT, (yyvsp[(3) - (3)].number))); ;}
    break;

  case 115:

/* Line 1455 of yacc.c  */
#line 550 "imap-parse.y"
    { REPORT(processor->Status(IM_UIDVALIDITY, (yyvsp[(3) - (3)].number))); ;}
    break;

  case 116:

/* Line 1455 of yacc.c  */
#line 551 "imap-parse.y"
    { REPORT(processor->Status(IM_UNSEEN, (yyvsp[(3) - (3)].number))); ;}
    break;

  case 117:

/* Line 1455 of yacc.c  */
#line 552 "imap-parse.y"
    { REPORT(processor->Status(IM_HIGHESTMODSEQ, (yyvsp[(3) - (3)].number))); ;}
    break;

  case 118:

/* Line 1455 of yacc.c  */
#line 555 "imap-parse.y"
    { (yyval.flags).flags = 0; (yyval.flags).keywords = NULL; ;}
    break;

  case 119:

/* Line 1455 of yacc.c  */
#line 556 "imap-parse.y"
    { (yyval.flags) = (yyvsp[(2) - (2)].flags); (yyval.flags).flags |= (yyvsp[(1) - (2)].signed_number); ;}
    break;

  case 120:

/* Line 1455 of yacc.c  */
#line 558 "imap-parse.y"
    {
					(yyval.flags) = (yyvsp[(2) - (2)].flags);
					NEW_ELEMENT_IF_NEEDED((yyval.flags).keywords, ImapSortedVector, ());
					REPORT((yyval.flags).keywords->Insert((yyvsp[(1) - (2)].number)));
				  ;}
    break;

  case 121:

/* Line 1455 of yacc.c  */
#line 563 "imap-parse.y"
    { (yyval.flags) = (yyvsp[(2) - (2)].flags); ;}
    break;

  case 122:

/* Line 1455 of yacc.c  */
#line 566 "imap-parse.y"
    { (yyval.flags).flags = 0; (yyval.flags).keywords = NULL; ;}
    break;

  case 123:

/* Line 1455 of yacc.c  */
#line 567 "imap-parse.y"
    { (yyval.flags) = (yyvsp[(1) - (3)].flags); (yyval.flags).flags |= (yyvsp[(3) - (3)].signed_number); ;}
    break;

  case 124:

/* Line 1455 of yacc.c  */
#line 569 "imap-parse.y"
    {
					(yyval.flags) = (yyvsp[(1) - (3)].flags);
					NEW_ELEMENT_IF_NEEDED((yyval.flags).keywords, ImapSortedVector, ());
					REPORT((yyval.flags).keywords->Insert((yyvsp[(3) - (3)].number)));
				  ;}
    break;

  case 125:

/* Line 1455 of yacc.c  */
#line 576 "imap-parse.y"
    { (yyval.signed_number) = ImapFlags::FLAG_ANSWERED; ;}
    break;

  case 126:

/* Line 1455 of yacc.c  */
#line 577 "imap-parse.y"
    { (yyval.signed_number) = ImapFlags::FLAG_FLAGGED; ;}
    break;

  case 127:

/* Line 1455 of yacc.c  */
#line 578 "imap-parse.y"
    { (yyval.signed_number) = ImapFlags::FLAG_DELETED; ;}
    break;

  case 128:

/* Line 1455 of yacc.c  */
#line 579 "imap-parse.y"
    { (yyval.signed_number) = ImapFlags::FLAG_SEEN; ;}
    break;

  case 129:

/* Line 1455 of yacc.c  */
#line 580 "imap-parse.y"
    { (yyval.signed_number) = ImapFlags::FLAG_DRAFT; ;}
    break;

  case 130:

/* Line 1455 of yacc.c  */
#line 581 "imap-parse.y"
    { (yyval.signed_number) = ImapFlags::FLAG_STAR; ;}
    break;

  case 131:

/* Line 1455 of yacc.c  */
#line 582 "imap-parse.y"
    { (yyval.signed_number) = ImapFlags::FLAG_RECENT; ;}
    break;

  case 132:

/* Line 1455 of yacc.c  */
#line 583 "imap-parse.y"
    { (yyval.signed_number) = ImapFlags::NONE; /* ignored */ ;}
    break;

  case 133:

/* Line 1455 of yacc.c  */
#line 584 "imap-parse.y"
    { (yyval.signed_number) = ImapFlags::NONE; /* ignored */ ;}
    break;

  case 134:

/* Line 1455 of yacc.c  */
#line 585 "imap-parse.y"
    { (yyval.signed_number) = ImapFlags::FLAG_SPAM; ;}
    break;

  case 135:

/* Line 1455 of yacc.c  */
#line 586 "imap-parse.y"
    { (yyval.signed_number) = ImapFlags::FLAG_NOT_SPAM; ;}
    break;

  case 136:

/* Line 1455 of yacc.c  */
#line 589 "imap-parse.y"
    { NEW_ELEMENT((yyval.number_vector), ImapNumberVector, ()); ;}
    break;

  case 137:

/* Line 1455 of yacc.c  */
#line 590 "imap-parse.y"
    { (yyval.number_vector) = (yyvsp[(2) - (3)].number_vector); ;}
    break;

  case 140:

/* Line 1455 of yacc.c  */
#line 597 "imap-parse.y"
    { NEW_ELEMENT((yyval.number_vector), ImapNumberVector, ()); ;}
    break;

  case 141:

/* Line 1455 of yacc.c  */
#line 598 "imap-parse.y"
    { REPORT((yyvsp[(2) - (2)].number_vector)->Add((yyvsp[(1) - (2)].number))); (yyval.number_vector) = (yyvsp[(2) - (2)].number_vector); ;}
    break;

  case 142:

/* Line 1455 of yacc.c  */
#line 601 "imap-parse.y"
    { NEW_ELEMENT((yyval.number_vector), ImapNumberVector, ()); ;}
    break;

  case 143:

/* Line 1455 of yacc.c  */
#line 602 "imap-parse.y"
    { REPORT((yyvsp[(1) - (3)].number_vector)->Add((yyvsp[(3) - (3)].number))); (yyval.number_vector) = (yyvsp[(1) - (3)].number_vector); ;}
    break;

  case 144:

/* Line 1455 of yacc.c  */
#line 605 "imap-parse.y"
    { (yyval.mailboxlist).flags = (yyvsp[(2) - (7)].number); (yyval.mailboxlist).delimiter = (yyvsp[(5) - (7)].number); (yyval.mailboxlist).mailbox = (yyvsp[(7) - (7)].generic); ;}
    break;

  case 145:

/* Line 1455 of yacc.c  */
#line 608 "imap-parse.y"
    { REPORT(processor->Expunge((yyvsp[(1) - (3)].number))); ;}
    break;

  case 146:

/* Line 1455 of yacc.c  */
#line 610 "imap-parse.y"
    { processor->ResetMessage(); processor->SetMessageServerId((yyvsp[(1) - (4)].number)); ;}
    break;

  case 147:

/* Line 1455 of yacc.c  */
#line 612 "imap-parse.y"
    { REPORT(processor->Fetch()); ;}
    break;

  case 148:

/* Line 1455 of yacc.c  */
#line 613 "imap-parse.y"
    { REPORT(processor->Vanished((yyvsp[(7) - (7)].number_vector), TRUE)); ;}
    break;

  case 149:

/* Line 1455 of yacc.c  */
#line 614 "imap-parse.y"
    { REPORT(processor->Vanished((yyvsp[(3) - (3)].number_vector), FALSE)); ;}
    break;

  case 159:

/* Line 1455 of yacc.c  */
#line 633 "imap-parse.y"
    {
					processor->SetMessageFlags((yyvsp[(4) - (5)].flags).flags);
					processor->SetMessageKeywords((yyvsp[(4) - (5)].flags).keywords);
				  ;}
    break;

  case 161:

/* Line 1455 of yacc.c  */
#line 640 "imap-parse.y"
    { processor->SetMessageModSeq((yyvsp[(4) - (5)].number)); ;}
    break;

  case 162:

/* Line 1455 of yacc.c  */
#line 643 "imap-parse.y"
    { /* ignored */ ;}
    break;

  case 163:

/* Line 1455 of yacc.c  */
#line 644 "imap-parse.y"
    { /* ignored */ ;}
    break;

  case 164:

/* Line 1455 of yacc.c  */
#line 645 "imap-parse.y"
    { processor->SetMessageRawText(ImapSection::COMPLETE, (yyvsp[(3) - (3)].generic)); ;}
    break;

  case 165:

/* Line 1455 of yacc.c  */
#line 646 "imap-parse.y"
    { processor->SetMessageRawText(ImapSection::HEADERS, (yyvsp[(3) - (3)].generic)); ;}
    break;

  case 166:

/* Line 1455 of yacc.c  */
#line 647 "imap-parse.y"
    { processor->SetMessageRawText(ImapSection::PART, (yyvsp[(3) - (3)].generic)); ;}
    break;

  case 167:

/* Line 1455 of yacc.c  */
#line 648 "imap-parse.y"
    { processor->SetMessageSize((yyvsp[(3) - (3)].number)); ;}
    break;

  case 168:

/* Line 1455 of yacc.c  */
#line 649 "imap-parse.y"
    { processor->SetMessageBodyStructure((yyvsp[(3) - (3)].body)); ;}
    break;

  case 169:

/* Line 1455 of yacc.c  */
#line 650 "imap-parse.y"
    { processor->SetMessageBodyStructure((yyvsp[(3) - (3)].body)); ;}
    break;

  case 170:

/* Line 1455 of yacc.c  */
#line 651 "imap-parse.y"
    { processor->SetMessageRawText((yyvsp[(2) - (9)].number), (yyvsp[(9) - (9)].generic)); ;}
    break;

  case 171:

/* Line 1455 of yacc.c  */
#line 652 "imap-parse.y"
    { processor->SetMessageRawText((yyvsp[(2) - (4)].number), (yyvsp[(4) - (4)].generic)); ;}
    break;

  case 172:

/* Line 1455 of yacc.c  */
#line 653 "imap-parse.y"
    { processor->SetMessageUID((yyvsp[(3) - (3)].number)); ;}
    break;

  case 173:

/* Line 1455 of yacc.c  */
#line 656 "imap-parse.y"
    { (yyval.number) = (yyvsp[(2) - (3)].number); ;}
    break;

  case 174:

/* Line 1455 of yacc.c  */
#line 659 "imap-parse.y"
    { (yyval.number) = ImapSection::COMPLETE; ;}
    break;

  case 177:

/* Line 1455 of yacc.c  */
#line 664 "imap-parse.y"
    { (yyval.number) = ImapSection::HEADERS; ;}
    break;

  case 178:

/* Line 1455 of yacc.c  */
#line 665 "imap-parse.y"
    { (yyval.number) = ImapSection::HEADERS; ;}
    break;

  case 179:

/* Line 1455 of yacc.c  */
#line 666 "imap-parse.y"
    { (yyval.number) = ImapSection::HEADERS; ;}
    break;

  case 180:

/* Line 1455 of yacc.c  */
#line 667 "imap-parse.y"
    { (yyval.number) = ImapSection::PART; ;}
    break;

  case 186:

/* Line 1455 of yacc.c  */
#line 681 "imap-parse.y"
    { (yyval.number) = (yyvsp[(2) - (2)].number); ;}
    break;

  case 187:

/* Line 1455 of yacc.c  */
#line 685 "imap-parse.y"
    { (yyval.number) = ImapSection::PART; ;}
    break;

  case 188:

/* Line 1455 of yacc.c  */
#line 686 "imap-parse.y"
    { (yyval.number) = (yyvsp[(3) - (3)].number); ;}
    break;

  case 190:

/* Line 1455 of yacc.c  */
#line 691 "imap-parse.y"
    { (yyval.number) = ImapSection::PART; ;}
    break;

  case 192:

/* Line 1455 of yacc.c  */
#line 695 "imap-parse.y"
    { (yyval.number) = ImapSection::MIME; ;}
    break;

  case 194:

/* Line 1455 of yacc.c  */
#line 702 "imap-parse.y"
    { NEW_ELEMENT((yyval.envelope), ImapEnvelope, ((yyvsp[(2) - (21)].generic), (yyvsp[(4) - (21)].generic), (yyvsp[(6) - (21)].vector), (yyvsp[(8) - (21)].vector), (yyvsp[(10) - (21)].vector), (yyvsp[(12) - (21)].vector), (yyvsp[(14) - (21)].vector), (yyvsp[(16) - (21)].vector), (yyvsp[(18) - (21)].generic), (yyvsp[(20) - (21)].generic))); ;}
    break;

  case 195:

/* Line 1455 of yacc.c  */
#line 706 "imap-parse.y"
    { NEW_ELEMENT((yyval.date_time), ImapDateTime, ((yyvsp[(2) - (15)].number), (yyvsp[(4) - (15)].number), (yyvsp[(6) - (15)].number), (yyvsp[(8) - (15)].number), (yyvsp[(10) - (15)].number), (yyvsp[(12) - (15)].number), (yyvsp[(14) - (15)].signed_number))); ;}
    break;

  case 196:

/* Line 1455 of yacc.c  */
#line 709 "imap-parse.y"
    { (yyval.body) = (yyvsp[(2) - (3)].body); ;}
    break;

  case 197:

/* Line 1455 of yacc.c  */
#line 710 "imap-parse.y"
    { (yyval.body) = (yyvsp[(2) - (3)].body); ;}
    break;

  case 198:

/* Line 1455 of yacc.c  */
#line 711 "imap-parse.y"
    { (yyval.body) = NULL; ;}
    break;

  case 199:

/* Line 1455 of yacc.c  */
#line 714 "imap-parse.y"
    { NEW_ELEMENT((yyval.body), ImapBody, ((yyvsp[(1) - (5)].number), (yyvsp[(3) - (5)].number))); ;}
    break;

  case 200:

/* Line 1455 of yacc.c  */
#line 715 "imap-parse.y"
    { NEW_ELEMENT((yyval.body), ImapBody, (IM_MESSAGE, (yyvsp[(3) - (5)].number))); ;}
    break;

  case 201:

/* Line 1455 of yacc.c  */
#line 716 "imap-parse.y"
    { NEW_ELEMENT((yyval.body), ImapBody, (IM_MESSAGE, IM_MEDIARFC822, (yyvsp[(5) - (5)].body))); ;}
    break;

  case 203:

/* Line 1455 of yacc.c  */
#line 721 "imap-parse.y"
    { REPORT((yyvsp[(2) - (5)].vector)->Insert(0, (yyvsp[(1) - (5)].body))); NEW_ELEMENT((yyval.body), ImapBody, (0, (yyvsp[(4) - (5)].number), (yyvsp[(2) - (5)].vector))); ;}
    break;

  case 204:

/* Line 1455 of yacc.c  */
#line 724 "imap-parse.y"
    { NEW_ELEMENT((yyval.vector), ImapVector, ()); ;}
    break;

  case 205:

/* Line 1455 of yacc.c  */
#line 725 "imap-parse.y"
    { REPORT((yyvsp[(1) - (2)].vector)->Add((yyvsp[(2) - (2)].body))); (yyval.vector) = (yyvsp[(1) - (2)].vector); ;}
    break;

  case 212:

/* Line 1455 of yacc.c  */
#line 742 "imap-parse.y"
    { (yyval.body) = (yyvsp[(5) - (8)].body); ;}
    break;

  case 215:

/* Line 1455 of yacc.c  */
#line 751 "imap-parse.y"
    { REPORT((yyvsp[(5) - (6)].vector)->Add((yyvsp[(2) - (6)].generic))); REPORT((yyvsp[(5) - (6)].vector)->Add((yyvsp[(4) - (6)].generic))); (yyval.vector) = (yyvsp[(5) - (6)].vector); ;}
    break;

  case 216:

/* Line 1455 of yacc.c  */
#line 752 "imap-parse.y"
    { NEW_ELEMENT((yyval.vector), ImapVector, ()); ;}
    break;

  case 217:

/* Line 1455 of yacc.c  */
#line 753 "imap-parse.y"
    { NEW_ELEMENT((yyval.vector), ImapVector, ()); ;}
    break;

  case 218:

/* Line 1455 of yacc.c  */
#line 756 "imap-parse.y"
    { NEW_ELEMENT((yyval.vector), ImapVector, ()); ;}
    break;

  case 219:

/* Line 1455 of yacc.c  */
#line 757 "imap-parse.y"
    { REPORT((yyvsp[(1) - (3)].vector)->Add((yyvsp[(3) - (3)].generic))); (yyval.vector) = (yyvsp[(1) - (3)].vector); ;}
    break;

  case 222:

/* Line 1455 of yacc.c  */
#line 766 "imap-parse.y"
    { NEW_ELEMENT((yyval.generic), ImapUnion, (IM_BIT7)); ;}
    break;

  case 223:

/* Line 1455 of yacc.c  */
#line 767 "imap-parse.y"
    { NEW_ELEMENT((yyval.generic), ImapUnion, (IM_BIT8)); ;}
    break;

  case 224:

/* Line 1455 of yacc.c  */
#line 768 "imap-parse.y"
    { NEW_ELEMENT((yyval.generic), ImapUnion, (IM_BINARY)); ;}
    break;

  case 225:

/* Line 1455 of yacc.c  */
#line 769 "imap-parse.y"
    { NEW_ELEMENT((yyval.generic), ImapUnion, (IM_BASE64)); ;}
    break;

  case 226:

/* Line 1455 of yacc.c  */
#line 770 "imap-parse.y"
    { NEW_ELEMENT((yyval.generic), ImapUnion, (IM_QUOTEDPRINTABLE)); ;}
    break;

  case 227:

/* Line 1455 of yacc.c  */
#line 771 "imap-parse.y"
    { (yyval.generic) = (yyvsp[(1) - (1)].generic); ;}
    break;

  case 229:

/* Line 1455 of yacc.c  */
#line 777 "imap-parse.y"
    { (yyval.number) = IM_APPLICATION; ;}
    break;

  case 230:

/* Line 1455 of yacc.c  */
#line 778 "imap-parse.y"
    { (yyval.number) = IM_AUDIO; ;}
    break;

  case 231:

/* Line 1455 of yacc.c  */
#line 779 "imap-parse.y"
    { (yyval.number) = IM_IMAGE; ;}
    break;

  case 232:

/* Line 1455 of yacc.c  */
#line 780 "imap-parse.y"
    { (yyval.number) = IM_VIDEO; ;}
    break;

  case 233:

/* Line 1455 of yacc.c  */
#line 781 "imap-parse.y"
    { (yyval.number) = 0; ;}
    break;

  case 234:

/* Line 1455 of yacc.c  */
#line 785 "imap-parse.y"
    {
					NEW_ELEMENT((yyval.body), ImapBody, (IM_TEXT, (yyvsp[(3) - (8)].number)));
				  ;}
    break;

  case 235:

/* Line 1455 of yacc.c  */
#line 790 "imap-parse.y"
    { (yyval.number) = IM_ARCHIVE; ;}
    break;

  case 236:

/* Line 1455 of yacc.c  */
#line 791 "imap-parse.y"
    { (yyval.number) = IM_OGG; ;}
    break;

  case 237:

/* Line 1455 of yacc.c  */
#line 792 "imap-parse.y"
    { (yyval.number) = 0; ;}
    break;

  case 238:

/* Line 1455 of yacc.c  */
#line 793 "imap-parse.y"
    { (yyval.number) = 0; ;}
    break;

  case 239:

/* Line 1455 of yacc.c  */
#line 794 "imap-parse.y"
    { (yyval.number) = 0; ;}
    break;

  case 240:

/* Line 1455 of yacc.c  */
#line 795 "imap-parse.y"
    { (yyval.number) = 0; ;}
    break;

  case 241:

/* Line 1455 of yacc.c  */
#line 796 "imap-parse.y"
    { (yyval.number) = 0; ;}
    break;

  case 242:

/* Line 1455 of yacc.c  */
#line 797 "imap-parse.y"
    { (yyval.number) = 0; ;}
    break;

  case 243:

/* Line 1455 of yacc.c  */
#line 798 "imap-parse.y"
    { (yyval.number) = 0; ;}
    break;

  case 244:

/* Line 1455 of yacc.c  */
#line 799 "imap-parse.y"
    { (yyval.number) = 0; ;}
    break;

  case 245:

/* Line 1455 of yacc.c  */
#line 800 "imap-parse.y"
    { (yyval.number) = 0; ;}
    break;

  case 246:

/* Line 1455 of yacc.c  */
#line 801 "imap-parse.y"
    { (yyval.number) = 0; ;}
    break;

  case 247:

/* Line 1455 of yacc.c  */
#line 802 "imap-parse.y"
    { (yyval.number) = 0; ;}
    break;

  case 248:

/* Line 1455 of yacc.c  */
#line 803 "imap-parse.y"
    { (yyval.number) = IM_ALTERNATIVE; ;}
    break;

  case 249:

/* Line 1455 of yacc.c  */
#line 804 "imap-parse.y"
    { (yyval.number) = 0; ;}
    break;

  case 271:

/* Line 1455 of yacc.c  */
#line 848 "imap-parse.y"
    { (yyval.number) = (yyvsp[(2) - (2)].number); ;}
    break;

  case 274:

/* Line 1455 of yacc.c  */
#line 855 "imap-parse.y"
    { (yyval.vector) = (yyvsp[(2) - (3)].vector); ;}
    break;

  case 275:

/* Line 1455 of yacc.c  */
#line 856 "imap-parse.y"
    { NEW_ELEMENT((yyval.vector), ImapVector, ()); ;}
    break;

  case 276:

/* Line 1455 of yacc.c  */
#line 860 "imap-parse.y"
    { NEW_ELEMENT((yyval.address), ImapAddress, ((yyvsp[(2) - (9)].generic), (yyvsp[(4) - (9)].generic), (yyvsp[(6) - (9)].generic), (yyvsp[(8) - (9)].generic))); ;}
    break;

  case 277:

/* Line 1455 of yacc.c  */
#line 863 "imap-parse.y"
    { NEW_ELEMENT((yyval.vector), ImapVector, ()); ;}
    break;

  case 278:

/* Line 1455 of yacc.c  */
#line 864 "imap-parse.y"
    { REPORT((yyvsp[(1) - (2)].vector)->Add((yyvsp[(2) - (2)].address))); (yyval.vector) = (yyvsp[(1) - (2)].vector); ;}
    break;

  case 279:

/* Line 1455 of yacc.c  */
#line 867 "imap-parse.y"
    { (yyval.generic) = (yyvsp[(1) - (1)].generic); ;}
    break;

  case 280:

/* Line 1455 of yacc.c  */
#line 868 "imap-parse.y"
    { (yyval.generic) = NULL; ;}
    break;

  case 281:

/* Line 1455 of yacc.c  */
#line 871 "imap-parse.y"
    { (yyval.number) = ImapFlags::NONE; ;}
    break;

  case 282:

/* Line 1455 of yacc.c  */
#line 872 "imap-parse.y"
    { (yyval.number) = (yyvsp[(2) - (2)].number) | (yyvsp[(1) - (2)].number); ;}
    break;

  case 283:

/* Line 1455 of yacc.c  */
#line 875 "imap-parse.y"
    { (yyval.number) = ImapFlags::NONE; ;}
    break;

  case 284:

/* Line 1455 of yacc.c  */
#line 876 "imap-parse.y"
    { (yyval.number) = (yyvsp[(1) - (3)].number) | (yyvsp[(3) - (3)].number); ;}
    break;

  case 285:

/* Line 1455 of yacc.c  */
#line 879 "imap-parse.y"
    { (yyval.number) = (yyvsp[(1) - (1)].number); ;}
    break;

  case 286:

/* Line 1455 of yacc.c  */
#line 880 "imap-parse.y"
    { (yyval.number) = (yyvsp[(1) - (1)].number); ;}
    break;

  case 287:

/* Line 1455 of yacc.c  */
#line 883 "imap-parse.y"
    { (yyval.number) = ImapFlags::LFLAG_NOINFERIORS; ;}
    break;

  case 288:

/* Line 1455 of yacc.c  */
#line 884 "imap-parse.y"
    { /* ignored */ (yyval.number) = ImapFlags::NONE; ;}
    break;

  case 289:

/* Line 1455 of yacc.c  */
#line 887 "imap-parse.y"
    { (yyval.number) = ImapFlags::LFLAG_NOSELECT; ;}
    break;

  case 290:

/* Line 1455 of yacc.c  */
#line 888 "imap-parse.y"
    { (yyval.number) = ImapFlags::LFLAG_MARKED;  ;}
    break;

  case 291:

/* Line 1455 of yacc.c  */
#line 889 "imap-parse.y"
    { (yyval.number) = ImapFlags::LFLAG_UNMARKED; ;}
    break;

  case 292:

/* Line 1455 of yacc.c  */
#line 890 "imap-parse.y"
    { (yyval.number) = ImapFlags::LFLAG_HASCHILDREN; ;}
    break;

  case 293:

/* Line 1455 of yacc.c  */
#line 891 "imap-parse.y"
    { (yyval.number) = ImapFlags::LFLAG_HASNOCHILDREN; ;}
    break;

  case 294:

/* Line 1455 of yacc.c  */
#line 892 "imap-parse.y"
    { (yyval.number) = ImapFlags::LFLAG_INBOX; ;}
    break;

  case 295:

/* Line 1455 of yacc.c  */
#line 893 "imap-parse.y"
    { (yyval.number) = ImapFlags::LFLAG_DRAFTS; ;}
    break;

  case 296:

/* Line 1455 of yacc.c  */
#line 894 "imap-parse.y"
    { (yyval.number) = ImapFlags::LFLAG_TRASH; ;}
    break;

  case 297:

/* Line 1455 of yacc.c  */
#line 895 "imap-parse.y"
    { (yyval.number) = ImapFlags::LFLAG_SENT; ;}
    break;

  case 298:

/* Line 1455 of yacc.c  */
#line 896 "imap-parse.y"
    { (yyval.number) = ImapFlags::LFLAG_SPAM; ;}
    break;

  case 299:

/* Line 1455 of yacc.c  */
#line 897 "imap-parse.y"
    { (yyval.number) = ImapFlags::LFLAG_FLAGGED; ;}
    break;

  case 300:

/* Line 1455 of yacc.c  */
#line 898 "imap-parse.y"
    { (yyval.number) = ImapFlags::LFLAG_ALLMAIL; ;}
    break;

  case 301:

/* Line 1455 of yacc.c  */
#line 899 "imap-parse.y"
    { (yyval.number) = ImapFlags::LFLAG_ARCHIVE; ;}
    break;

  case 302:

/* Line 1455 of yacc.c  */
#line 902 "imap-parse.y"
    { (yyval.number) = (yyvsp[(1) - (1)].generic)->m_val_string[0]; ;}
    break;

  case 303:

/* Line 1455 of yacc.c  */
#line 903 "imap-parse.y"
    { (yyval.number) = '\0' ;}
    break;

  case 309:

/* Line 1455 of yacc.c  */
#line 914 "imap-parse.y"
    {
					NEW_ELEMENT((yyval.generic), ImapUnion, ("", INT_STRING_SIZE));
					CHECK_MEMORY((yyval.generic)->m_val_string);
					op_sprintf((yyval.generic)->m_val_string, "%lld", (long long int)(yyvsp[(1) - (1)].number));
				  ;}
    break;

  case 310:

/* Line 1455 of yacc.c  */
#line 921 "imap-parse.y"
    { NEW_ELEMENT((yyval.vector), ImapVector, ()); ;}
    break;

  case 311:

/* Line 1455 of yacc.c  */
#line 922 "imap-parse.y"
    { REPORT((yyvsp[(2) - (2)].vector)->Add((yyvsp[(1) - (2)].generic))); (yyval.vector) = (yyvsp[(2) - (2)].vector); ;}
    break;

  case 312:

/* Line 1455 of yacc.c  */
#line 925 "imap-parse.y"
    { NEW_ELEMENT((yyval.vector), ImapVector, ()); ;}
    break;

  case 313:

/* Line 1455 of yacc.c  */
#line 926 "imap-parse.y"
    { REPORT((yyvsp[(1) - (3)].vector)->Add((yyvsp[(3) - (3)].generic))); (yyval.vector) = (yyvsp[(1) - (3)].vector); ;}
    break;



/* Line 1455 of yacc.c  */
#line 3794 "imap-parse.cpp"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (yyscanner, processor, YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yyscanner, processor, yymsg);
	  }
	else
	  {
	    yyerror (yyscanner, processor, YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval, yyscanner, processor);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, yyscanner, processor);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (yyscanner, processor, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, yyscanner, processor);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, yyscanner, processor);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 1675 of yacc.c  */
#line 941 "imap-parse.y"


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

