#include "core/pch.h"

#ifdef BISON_PARSER

/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison implementation for Yacc-like parsers in C

      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.

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
#define YYBISON_VERSION "2.5"

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
#define yyparse         cssparse
#define yylex           csslex
#define yyerror         csserror
#define yylval          csslval
#define yychar          csschar
#define yydebug         cssdebug
#define yynerrs         cssnerrs


/* Copy the first part of user declarations.  */



#include "modules/style/src/css_parser.h"
#include "modules/style/css.h"
#include "modules/style/css_media.h"
#include "modules/style/src/css_keyframes.h"
#include "modules/style/src/css_charset_rule.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/gen_math.h"

#define parser static_cast<CSS_Parser*>(parm)
#define YYPARSE_PARAM parm
#define YYLEX_PARAM parser

#define YYOUTOFMEM goto yyexhaustedlab

// Don't allocate memory from stack. Not predictable and might
// cause problems on devices with little stack memory.
#define YYSTACK_USE_ALLOCA 0

#ifdef OPERA_USE_LESS_STACK
#define YYINITDEPTH 1
#else
// Based on measuring "yystacksize" when running with YYINITDEPTH=1.
#define YYINITDEPTH 32
#endif

#define csserror(a) csserrorverb(a, parser)

#define YYMALLOC parser->YYMalloc
#define YYFREE parser->YYFree

int yylex(YYSTYPE* value, CSS_Parser* css_parser);
int csserrorverb(const char* s, CSS_Parser* css_parser);




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
     CSS_TOK_SPACE = 258,
     CSS_TOK_CDO = 259,
     CSS_TOK_CDC = 260,
     CSS_TOK_INCLUDES = 261,
     CSS_TOK_DASHMATCH = 262,
     CSS_TOK_PREFIXMATCH = 263,
     CSS_TOK_SUFFIXMATCH = 264,
     CSS_TOK_SUBSTRMATCH = 265,
     CSS_TOK_STRING_TRUNC = 266,
     CSS_TOK_STRING = 267,
     CSS_TOK_IDENT = 268,
     CSS_TOK_HASH = 269,
     CSS_TOK_IMPORT_SYM = 270,
     CSS_TOK_PAGE_SYM = 271,
     CSS_TOK_MEDIA_SYM = 272,
     CSS_TOK_SUPPORTS_SYM = 273,
     CSS_TOK_FONT_FACE_SYM = 274,
     CSS_TOK_CHARSET_SYM = 275,
     CSS_TOK_NAMESPACE_SYM = 276,
     CSS_TOK_VIEWPORT_SYM = 277,
     CSS_TOK_KEYFRAMES_SYM = 278,
     CSS_TOK_ATKEYWORD = 279,
     CSS_TOK_IMPORTANT_SYM = 280,
     CSS_TOK_EMS = 281,
     CSS_TOK_REMS = 282,
     CSS_TOK_EXS = 283,
     CSS_TOK_LENGTH_PX = 284,
     CSS_TOK_LENGTH_CM = 285,
     CSS_TOK_LENGTH_MM = 286,
     CSS_TOK_LENGTH_IN = 287,
     CSS_TOK_LENGTH_PT = 288,
     CSS_TOK_LENGTH_PC = 289,
     CSS_TOK_ANGLE = 290,
     CSS_TOK_TIME = 291,
     CSS_TOK_FREQ = 292,
     CSS_TOK_DIMEN = 293,
     CSS_TOK_PERCENTAGE = 294,
     CSS_TOK_DPI = 295,
     CSS_TOK_DPCM = 296,
     CSS_TOK_DPPX = 297,
     CSS_TOK_NUMBER = 298,
     CSS_TOK_INTEGER = 299,
     CSS_TOK_N = 300,
     CSS_TOK_URI = 301,
     CSS_TOK_FUNCTION = 302,
     CSS_TOK_UNICODERANGE = 303,
     CSS_TOK_RGB_FUNC = 304,
     CSS_TOK_HSL_FUNC = 305,
     CSS_TOK_NOT_FUNC = 306,
     CSS_TOK_RGBA_FUNC = 307,
     CSS_TOK_HSLA_FUNC = 308,
     CSS_TOK_LINEAR_GRADIENT_FUNC = 309,
     CSS_TOK_WEBKIT_LINEAR_GRADIENT_FUNC = 310,
     CSS_TOK_O_LINEAR_GRADIENT_FUNC = 311,
     CSS_TOK_REPEATING_LINEAR_GRADIENT_FUNC = 312,
     CSS_TOK_RADIAL_GRADIENT_FUNC = 313,
     CSS_TOK_REPEATING_RADIAL_GRADIENT_FUNC = 314,
     CSS_TOK_DOUBLE_RAINBOW_FUNC = 315,
     CSS_TOK_ATTR_FUNC = 316,
     CSS_TOK_TRANSFORM_FUNC = 317,
     CSS_TOK_COUNTER_FUNC = 318,
     CSS_TOK_COUNTERS_FUNC = 319,
     CSS_TOK_RECT_FUNC = 320,
     CSS_TOK_DASHBOARD_REGION_FUNC = 321,
     CSS_TOK_OR = 322,
     CSS_TOK_AND = 323,
     CSS_TOK_NOT = 324,
     CSS_TOK_ONLY = 325,
     CSS_TOK_LANG_FUNC = 326,
     CSS_TOK_SKIN_FUNC = 327,
     CSS_TOK_LANGUAGE_STRING_FUNC = 328,
     CSS_TOK_NTH_CHILD_FUNC = 329,
     CSS_TOK_NTH_OF_TYPE_FUNC = 330,
     CSS_TOK_NTH_LAST_CHILD_FUNC = 331,
     CSS_TOK_NTH_LAST_OF_TYPE_FUNC = 332,
     CSS_TOK_FORMAT_FUNC = 333,
     CSS_TOK_LOCAL_FUNC = 334,
     CSS_TOK_CUBIC_BEZ_FUNC = 335,
     CSS_TOK_INTEGER_FUNC = 336,
     CSS_TOK_STEPS_FUNC = 337,
     CSS_TOK_CUE_FUNC = 338,
     CSS_TOK_EOF = 339,
     CSS_TOK_UNRECOGNISED_CHAR = 340,
     CSS_TOK_STYLE_ATTR = 341,
     CSS_TOK_STYLE_ATTR_STRICT = 342,
     CSS_TOK_DOM_STYLE_PROPERTY = 343,
     CSS_TOK_DOM_PAGE_PROPERTY = 344,
     CSS_TOK_DOM_FONT_DESCRIPTOR = 345,
     CSS_TOK_DOM_VIEWPORT_PROPERTY = 346,
     CSS_TOK_DOM_KEYFRAME_PROPERTY = 347,
     CSS_TOK_DOM_RULE = 348,
     CSS_TOK_DOM_STYLE_RULE = 349,
     CSS_TOK_DOM_CHARSET_RULE = 350,
     CSS_TOK_DOM_IMPORT_RULE = 351,
     CSS_TOK_DOM_SUPPORTS_RULE = 352,
     CSS_TOK_DOM_MEDIA_RULE = 353,
     CSS_TOK_DOM_FONT_FACE_RULE = 354,
     CSS_TOK_DOM_PAGE_RULE = 355,
     CSS_TOK_DOM_VIEWPORT_RULE = 356,
     CSS_TOK_DOM_KEYFRAMES_RULE = 357,
     CSS_TOK_DOM_KEYFRAME_RULE = 358,
     CSS_TOK_DOM_SELECTOR = 359,
     CSS_TOK_DOM_PAGE_SELECTOR = 360,
     CSS_TOK_DOM_MEDIA_LIST = 361,
     CSS_TOK_DOM_MEDIUM = 362,
     CSS_TOK_DECLARATION = 363
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{


  struct {
    int first;
    int last;
  } int_pair;
  struct {
    unsigned int start_pos;
    unsigned int str_len;
  } str;
  struct {
    unsigned int start_pos;
    unsigned int str_len;
    double number;
  } number;
  unsigned short ushortint;
  short shortint;
  struct {
    unsigned int start_pos;
    unsigned int str_len;
    int integer;
  } integer;
  struct {
    unsigned int start_pos;
    unsigned int str_len;
    unsigned int integer;
  } uinteger;
  bool boolean;
  PropertyValue value;
  struct {
    unsigned int start_pos;
    unsigned int str_len;
    int ns_idx;
  } id_with_ns;
  struct {
    int a;
    int b;
  } linear_func;



} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */



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
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
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
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
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
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
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

# define YYCOPY_NEEDED 1

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

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
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
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  45
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   2262

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  129
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  142
/* YYNRULES -- Number of rules.  */
#define YYNRULES  480
/* YYNRULES -- Number of states.  */
#define YYNSTATES  1042

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   363

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,   123,   121,     2,     2,     2,
     124,   114,   120,   117,   111,   118,   115,   116,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,   113,   109,
       2,   128,   119,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   125,     2,   126,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   110,   127,   112,   122,     2,     2,     2,
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
     105,   106,   107,   108
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     7,     8,    15,    16,    21,    27,    33,
      39,    45,    51,    57,    63,    69,    75,    81,    82,    88,
      94,    99,   104,   109,   115,   121,   126,   132,   138,   144,
     149,   151,   153,   155,   157,   159,   160,   162,   166,   167,
     175,   177,   179,   181,   183,   185,   187,   189,   191,   193,
     199,   202,   203,   204,   209,   211,   213,   215,   217,   219,
     221,   223,   225,   227,   229,   231,   234,   237,   240,   248,
     254,   257,   259,   261,   263,   265,   268,   273,   274,   277,
     279,   282,   285,   288,   291,   294,   297,   300,   303,   306,
     309,   312,   315,   318,   324,   326,   329,   334,   336,   338,
     340,   342,   344,   346,   348,   350,   352,   354,   356,   358,
     360,   362,   364,   366,   368,   370,   372,   374,   376,   378,
     380,   382,   384,   386,   388,   390,   392,   394,   396,   398,
     400,   402,   404,   406,   408,   410,   412,   414,   416,   418,
     420,   422,   424,   426,   428,   430,   432,   434,   436,   438,
     440,   442,   444,   446,   448,   450,   452,   454,   456,   458,
     460,   462,   464,   466,   468,   470,   472,   474,   476,   478,
     480,   482,   484,   486,   488,   490,   492,   494,   496,   501,
     506,   507,   511,   512,   515,   517,   519,   521,   522,   525,
     527,   530,   532,   534,   535,   540,   547,   550,   552,   554,
     556,   558,   560,   562,   568,   570,   574,   579,   584,   589,
     594,   600,   604,   605,   612,   615,   618,   622,   625,   629,
     630,   637,   640,   641,   643,   645,   646,   652,   654,   656,
     657,   663,   664,   669,   672,   675,   678,   681,   682,   685,
     688,   694,   695,   701,   710,   714,   716,   719,   722,   727,
     732,   738,   739,   746,   749,   750,   758,   761,   762,   770,
     773,   775,   777,   783,   785,   786,   794,   797,   800,   801,
     806,   807,   808,   818,   821,   824,   827,   828,   831,   834,
     837,   839,   841,   844,   846,   848,   850,   852,   854,   856,
     858,   860,   862,   864,   866,   869,   870,   871,   876,   877,
     884,   887,   888,   891,   892,   898,   904,   912,   920,   928,
     929,   932,   935,   938,   940,   945,   947,   950,   954,   959,
     963,   966,   967,   968,   974,   975,   976,   983,   984,   985,
     992,   993,   996,   997,  1000,  1003,  1005,  1007,  1009,  1011,
    1013,  1020,  1027,  1030,  1034,  1035,  1039,  1041,  1043,  1045,
    1047,  1049,  1051,  1053,  1057,  1061,  1064,  1066,  1072,  1082,
    1084,  1086,  1088,  1090,  1092,  1094,  1096,  1098,  1101,  1105,
    1106,  1114,  1121,  1127,  1129,  1131,  1133,  1135,  1142,  1150,
    1153,  1157,  1160,  1164,  1167,  1170,  1171,  1173,  1177,  1179,
    1183,  1186,  1188,  1191,  1194,  1197,  1200,  1202,  1217,  1232,
    1251,  1270,  1285,  1304,  1305,  1312,  1313,  1320,  1321,  1328,
    1329,  1336,  1337,  1344,  1345,  1352,  1353,  1360,  1367,  1374,
    1385,  1396,  1411,  1420,  1435,  1442,  1452,  1459,  1466,  1467,
    1474,  1480,  1499,  1506,  1514,  1517,  1520,  1523,  1526,  1531,
    1536,  1537,  1539,  1542,  1547,  1550,  1552,  1555,  1561,  1562,
    1567,  1570,  1572,  1575,  1577,  1580,  1582,  1584,  1587,  1590,
    1593,  1596,  1599,  1602,  1605,  1608,  1611,  1614,  1617,  1620,
    1623,  1626,  1629,  1632,  1635,  1638,  1641,  1643,  1649,  1651,
    1655
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     130,     0,    -1,   158,   140,    84,    -1,    -1,    87,   131,
     160,   214,   135,    84,    -1,    -1,    86,   132,   160,   136,
      -1,   134,   160,   247,   246,    84,    -1,    94,   160,   212,
     160,    84,    -1,    95,   160,   139,   160,    84,    -1,    96,
     160,   164,   160,    84,    -1,    97,   160,   173,   160,    84,
      -1,    98,   160,   176,   160,    84,    -1,    99,   160,   193,
     160,    84,    -1,   100,   160,   191,   160,    84,    -1,   101,
     160,   195,   160,    84,    -1,   102,   160,   203,   160,    84,
      -1,    -1,   103,   160,   133,   199,    84,    -1,    93,   160,
     138,   160,    84,    -1,   104,   160,   218,    84,    -1,   105,
     160,   190,    84,    -1,   106,   160,   178,    84,    -1,   106,
     160,   178,   109,    84,    -1,   106,   160,   178,   110,    84,
      -1,   107,   160,   182,    84,    -1,   107,   160,   182,   109,
      84,    -1,   107,   160,   182,   110,    84,    -1,   107,   160,
     182,   111,    84,    -1,   108,   160,   217,    84,    -1,    88,
      -1,    92,    -1,    89,    -1,    90,    -1,    91,    -1,    -1,
     112,    -1,   214,   135,    84,    -1,    -1,   110,   137,   160,
     214,   112,   160,    84,    -1,   212,    -1,   139,    -1,   164,
      -1,   173,    -1,   176,    -1,   193,    -1,   191,    -1,   195,
      -1,   203,    -1,    20,   160,    12,   160,   109,    -1,    20,
     143,    -1,    -1,    -1,   140,   141,   142,   158,    -1,   212,
      -1,   173,    -1,   176,    -1,   191,    -1,   193,    -1,   164,
      -1,   144,    -1,   147,    -1,   139,    -1,   195,    -1,   203,
      -1,     1,     5,    -1,     1,   109,    -1,     1,   112,    -1,
      21,   160,    13,   160,   145,   160,   146,    -1,    21,   160,
     145,   160,   146,    -1,    21,   143,    -1,    12,    -1,    46,
      -1,   109,    -1,    84,    -1,    24,   143,    -1,   110,   160,
     149,   112,    -1,    -1,   149,   150,    -1,   151,    -1,   109,
     160,    -1,   156,   160,    -1,   148,   160,    -1,    15,   160,
      -1,    16,   160,    -1,    18,   160,    -1,    17,   160,    -1,
      19,   160,    -1,    20,   160,    -1,    21,   160,    -1,    22,
     160,    -1,    23,   160,    -1,    24,   160,    -1,    13,   160,
     113,   160,   153,    -1,   151,    -1,   153,   151,    -1,   155,
     160,   157,   114,    -1,    61,    -1,    63,    -1,    64,    -1,
      80,    -1,    83,    -1,    66,    -1,    60,    -1,    78,    -1,
      53,    -1,    50,    -1,    81,    -1,    71,    -1,    73,    -1,
      54,    -1,    79,    -1,    51,    -1,    74,    -1,    76,    -1,
      77,    -1,    75,    -1,    58,    -1,    65,    -1,    57,    -1,
      59,    -1,    52,    -1,    49,    -1,    72,    -1,    82,    -1,
      62,    -1,    55,    -1,    56,    -1,    47,    -1,    13,    -1,
      43,    -1,    44,    -1,    45,    -1,    25,    -1,    26,    -1,
      27,    -1,    28,    -1,    29,    -1,    30,    -1,    31,    -1,
      32,    -1,    33,    -1,    34,    -1,    35,    -1,    36,    -1,
      37,    -1,    39,    -1,    40,    -1,    41,    -1,    42,    -1,
      38,    -1,    11,    -1,    12,    -1,    46,    -1,    14,    -1,
      48,    -1,     6,    -1,   154,    -1,     7,    -1,     8,    -1,
       9,    -1,    10,    -1,    67,    -1,    68,    -1,    69,    -1,
      70,    -1,    85,    -1,   113,    -1,   115,    -1,   111,    -1,
     116,    -1,   117,    -1,   118,    -1,   119,    -1,   120,    -1,
     121,    -1,   122,    -1,   123,    -1,   124,   160,   157,   114,
      -1,   125,   160,   157,   126,    -1,    -1,   157,   156,   160,
      -1,    -1,   158,   159,    -1,     3,    -1,     4,    -1,     5,
      -1,    -1,   160,     3,    -1,     3,    -1,   161,     3,    -1,
     109,    -1,    84,    -1,    -1,   110,   163,     1,   112,    -1,
      15,   160,   165,   160,   178,   162,    -1,    15,   143,    -1,
      12,    -1,    46,    -1,   168,    -1,   169,    -1,   170,    -1,
     167,    -1,   124,   160,   166,   114,   160,    -1,   171,    -1,
      69,   160,   167,    -1,   167,    68,   160,   167,    -1,   169,
      68,   160,   167,    -1,   167,    67,   160,   167,    -1,   170,
      67,   160,   167,    -1,   124,   160,   152,   114,   160,    -1,
      18,   160,   166,    -1,    -1,   172,   110,   160,   174,   210,
     112,    -1,    18,   143,    -1,   172,   143,    -1,   172,     1,
      84,    -1,   172,    84,    -1,    17,   160,   178,    -1,    -1,
     175,   110,   160,   177,   210,   112,    -1,   175,   109,    -1,
      -1,   179,    -1,   182,    -1,    -1,   179,   180,   111,   160,
     181,    -1,   182,    -1,    84,    -1,    -1,   185,    13,   160,
     183,   186,    -1,    -1,   184,   187,   160,   186,    -1,     1,
     110,    -1,     1,   111,    -1,     1,   109,    -1,     1,    84,
      -1,    -1,    70,   160,    -1,    69,   160,    -1,   186,    68,
     160,   187,   160,    -1,    -1,   124,   160,    13,   160,   114,
      -1,   124,   160,    13,   160,   113,   160,   247,   114,    -1,
     124,     1,   114,    -1,    13,    -1,   113,    13,    -1,    16,
     160,    -1,    16,   160,   189,   160,    -1,    16,   160,   188,
     160,    -1,    16,   160,   188,   189,   160,    -1,    -1,   190,
     110,   160,   192,   214,   112,    -1,    16,   143,    -1,    -1,
      19,   160,   110,   160,   194,   214,   112,    -1,    19,   143,
      -1,    -1,    22,   160,   110,   160,   196,   214,   112,    -1,
      22,   143,    -1,    13,    -1,   265,    -1,   198,   160,   111,
     160,   197,    -1,   197,    -1,    -1,   198,   160,   110,   160,
     200,   214,   112,    -1,     1,   110,    -1,     1,   112,    -1,
      -1,   201,   202,   199,   160,    -1,    -1,    -1,    23,   160,
      13,   160,   110,   160,   204,   201,   112,    -1,    23,   143,
      -1,   116,   160,    -1,   111,   160,    -1,    -1,   117,   160,
      -1,   119,   160,    -1,   122,   160,    -1,   118,    -1,   117,
      -1,    13,   160,    -1,   212,    -1,   176,    -1,   191,    -1,
     193,    -1,   164,    -1,   144,    -1,   139,    -1,   203,    -1,
     173,    -1,   195,    -1,   147,    -1,     1,   112,    -1,    -1,
      -1,   210,   211,   209,   160,    -1,    -1,   218,   110,   160,
     213,   214,   112,    -1,     1,   110,    -1,    -1,   215,   217,
      -1,    -1,   214,   216,   109,   160,   217,    -1,   208,   113,
     160,   247,   246,    -1,   208,   113,   160,   247,   246,     1,
     109,    -1,   208,   113,   160,   247,   246,     1,   112,    -1,
     208,   113,   160,   247,   246,     1,    84,    -1,    -1,     1,
     109,    -1,     1,   112,    -1,     1,    84,    -1,   219,    -1,
     218,   111,   160,   219,    -1,   220,    -1,   220,   161,    -1,
     220,   206,   219,    -1,   220,   161,   206,   219,    -1,   220,
     161,   219,    -1,   237,   228,    -1,    -1,    -1,   127,   221,
     237,   222,   228,    -1,    -1,    -1,   120,   127,   223,   237,
     224,   228,    -1,    -1,    -1,    13,   127,   225,   237,   226,
     228,    -1,    -1,   227,   229,    -1,    -1,   228,   230,    -1,
     228,   230,    -1,    14,    -1,   232,    -1,   239,    -1,   242,
      -1,   231,    -1,   113,    51,   160,   230,   160,   114,    -1,
     113,    51,   160,   233,   160,   114,    -1,   115,    13,    -1,
     235,   127,   236,    -1,    -1,   127,   234,   236,    -1,   236,
      -1,    13,    -1,   120,    -1,    13,    -1,   120,    -1,    13,
      -1,   120,    -1,    13,   127,    13,    -1,   120,   127,    13,
      -1,   127,    13,    -1,    13,    -1,   125,   160,   238,   160,
     126,    -1,   125,   160,   238,   160,   241,   160,   240,   160,
     126,    -1,    13,    -1,    12,    -1,   128,    -1,     6,    -1,
       7,    -1,     8,    -1,     9,    -1,    10,    -1,   113,    13,
      -1,   113,   113,    13,    -1,    -1,   113,   113,    83,   160,
     243,   218,   114,    -1,   113,    71,   160,    13,   160,   114,
      -1,   113,   244,   160,   245,   114,    -1,    74,    -1,    75,
      -1,    76,    -1,    77,    -1,    45,   160,   207,   160,    44,
     160,    -1,   207,    45,   160,   207,   160,    44,   160,    -1,
      45,   160,    -1,   207,    45,   160,    -1,    44,   160,    -1,
     207,    44,   160,    -1,    13,   160,    -1,    25,   160,    -1,
      -1,   249,    -1,   247,   205,   249,    -1,   249,    -1,   248,
     205,   249,    -1,   207,   267,    -1,   267,    -1,    12,   160,
      -1,    13,   160,    -1,    46,   160,    -1,    48,   160,    -1,
     270,    -1,    49,   160,   265,   160,   111,   160,   265,   160,
     111,   160,   265,   160,   114,   160,    -1,    49,   160,   264,
     160,   111,   160,   264,   160,   111,   160,   264,   160,   114,
     160,    -1,    52,   160,   265,   160,   111,   160,   265,   160,
     111,   160,   265,   160,   111,   160,   266,   160,   114,   160,
      -1,    52,   160,   264,   160,   111,   160,   264,   160,   111,
     160,   264,   160,   111,   160,   266,   160,   114,   160,    -1,
      50,   160,   266,   160,   111,   160,   265,   160,   111,   160,
     265,   160,   114,   160,    -1,    53,   160,   266,   160,   111,
     160,   265,   160,   111,   160,   265,   160,   111,   160,   266,
     160,   114,   160,    -1,    -1,    54,   250,   160,   248,   114,
     160,    -1,    -1,    55,   251,   160,   248,   114,   160,    -1,
      -1,    56,   252,   160,   248,   114,   160,    -1,    -1,    57,
     253,   160,   248,   114,   160,    -1,    -1,    58,   254,   160,
     248,   114,   160,    -1,    -1,    59,   255,   160,   248,   114,
     160,    -1,    -1,    60,   256,   160,   248,   114,   160,    -1,
      61,   160,    13,   160,   114,   160,    -1,    63,   160,    13,
     160,   114,   160,    -1,    63,   160,    13,   160,   111,   160,
      13,   160,   114,   160,    -1,    64,   160,    13,   160,   111,
     160,    12,   160,   114,   160,    -1,    64,   160,    13,   160,
     111,   160,    12,   160,   111,   160,    13,   160,   114,   160,
      -1,    65,   160,   249,   249,   249,   249,   114,   160,    -1,
      65,   160,   249,   111,   160,   249,   111,   160,   249,   111,
     160,   249,   114,   160,    -1,    72,   160,    12,   160,   114,
     160,    -1,    66,   160,    13,   160,    13,   160,   263,   114,
     160,    -1,    73,   160,   264,   160,   114,   160,    -1,    73,
     160,    13,   160,   114,   160,    -1,    -1,    79,   160,   257,
     248,   114,   160,    -1,    78,   160,   262,   114,   160,    -1,
      80,   160,   266,   160,   111,   160,   266,   160,   111,   160,
     266,   160,   111,   160,   266,   160,   114,   160,    -1,    81,
     160,   264,   160,   114,   160,    -1,    82,   160,   264,   160,
     258,   114,   160,    -1,   148,   160,    -1,    24,   160,    -1,
     120,   160,    -1,   123,   160,    -1,   259,   260,   114,   160,
      -1,   111,   160,    13,   160,    -1,    -1,    62,    -1,   160,
     261,    -1,   260,   111,   160,   261,    -1,   207,   267,    -1,
     267,    -1,   240,   160,    -1,   262,   111,   160,   240,   160,
      -1,    -1,   267,   267,   267,   267,    -1,   207,    44,    -1,
      44,    -1,   207,    39,    -1,    39,    -1,   207,    43,    -1,
      43,    -1,   264,    -1,    43,   160,    -1,    44,   160,    -1,
      39,   160,    -1,    29,   160,    -1,    30,   160,    -1,    31,
     160,    -1,    32,   160,    -1,    33,   160,    -1,    34,   160,
      -1,    26,   160,    -1,    27,   160,    -1,    28,   160,    -1,
      35,   160,    -1,    36,   160,    -1,    37,   160,    -1,    38,
     160,    -1,    40,   160,    -1,    41,   160,    -1,    42,   160,
      -1,   268,    -1,    47,   160,   269,   114,   160,    -1,   249,
      -1,   269,   205,   249,    -1,    14,   160,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   209,   209,   210,   210,   211,   211,   212,   226,   227,
     228,   229,   230,   231,   232,   233,   234,   235,   235,   236,
     237,   249,   263,   264,   265,   266,   267,   268,   269,   270,
     274,   275,   276,   277,   278,   281,   283,   287,   288,   288,
     292,   293,   294,   295,   296,   297,   298,   299,   300,   304,
     329,   338,   340,   340,   344,   345,   346,   347,   348,   349,
     350,   351,   352,   353,   354,   355,   365,   370,   378,   400,
     420,   430,   431,   435,   436,   440,   449,   451,   452,   455,
     456,   459,   459,   459,   459,   459,   459,   459,   459,   459,
     459,   459,   459,   462,   465,   466,   469,   472,   473,   474,
     475,   476,   477,   478,   479,   480,   481,   482,   483,   484,
     485,   486,   487,   488,   489,   490,   491,   492,   493,   494,
     495,   496,   497,   498,   499,   500,   501,   502,   503,   506,
     507,   508,   509,   510,   511,   511,   511,   511,   511,   511,
     511,   511,   512,   512,   512,   512,   513,   513,   513,   513,
     514,   515,   516,   517,   518,   519,   520,   521,   522,   522,
     522,   522,   523,   524,   525,   526,   527,   528,   529,   530,
     531,   532,   533,   534,   535,   536,   537,   538,   539,   540,
     543,   544,   546,   547,   550,   550,   550,   552,   553,   556,
     556,   559,   560,   561,   561,   572,   593,   603,   603,   608,
     609,   610,   611,   615,   616,   620,   627,   639,   651,   662,
     674,   694,   698,   697,   708,   712,   716,   721,   729,   734,
     733,   744,   747,   749,   756,   765,   765,   772,   773,   781,
     780,   791,   791,   799,   804,   809,   814,   822,   823,   824,
     828,   833,   839,   844,   850,   861,   865,   889,   890,   891,
     892,   896,   896,   918,   928,   928,   938,   948,   948,   952,
     962,   978,   986,   999,  1015,  1015,  1024,  1034,  1047,  1047,
    1048,  1053,  1052,  1075,  1079,  1079,  1079,  1083,  1084,  1085,
    1089,  1089,  1093,  1113,  1114,  1115,  1116,  1117,  1118,  1119,
    1120,  1121,  1122,  1123,  1124,  1135,  1136,  1136,  1140,  1140,
    1153,  1169,  1169,  1175,  1175,  1179,  1196,  1205,  1214,  1222,
    1223,  1232,  1241,  1252,  1260,  1269,  1286,  1303,  1314,  1325,
    1339,  1343,  1343,  1343,  1347,  1347,  1347,  1352,  1368,  1351,
    1373,  1373,  1387,  1388,  1395,  1402,  1430,  1431,  1432,  1433,
    1437,  1460,  1469,  1476,  1481,  1481,  1486,  1493,  1514,  1522,
    1554,  1565,  1610,  1623,  1634,  1640,  1646,  1655,  1713,  1789,
    1790,  1794,  1795,  1796,  1797,  1798,  1799,  1803,  1824,  1844,
    1843,  1865,  1879,  1895,  1896,  1897,  1898,  1902,  1907,  1912,
    1917,  1922,  1927,  1932,  1955,  1956,  1960,  1968,  1982,  1987,
    1997,  2005,  2006,  2012,  2018,  2024,  2028,  2034,  2053,  2072,
    2095,  2118,  2123,  2132,  2132,  2136,  2136,  2140,  2140,  2144,
    2144,  2148,  2148,  2152,  2152,  2156,  2156,  2160,  2166,  2172,
    2178,  2184,  2190,  2201,  2212,  2218,  2232,  2237,  2253,  2253,
    2257,  2262,  2278,  2283,  2294,  2295,  2296,  2297,  2298,  2302,
    2315,  2323,  2332,  2333,  2336,  2341,  2348,  2352,  2360,  2373,
    2388,  2394,  2403,  2404,  2408,  2409,  2410,  2419,  2420,  2437,
    2438,  2439,  2440,  2441,  2442,  2443,  2444,  2445,  2446,  2447,
    2448,  2449,  2450,  2451,  2452,  2453,  2454,  2463,  2467,  2468,
    2472
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "CSS_TOK_SPACE", "CSS_TOK_CDO",
  "CSS_TOK_CDC", "CSS_TOK_INCLUDES", "CSS_TOK_DASHMATCH",
  "CSS_TOK_PREFIXMATCH", "CSS_TOK_SUFFIXMATCH", "CSS_TOK_SUBSTRMATCH",
  "CSS_TOK_STRING_TRUNC", "CSS_TOK_STRING", "CSS_TOK_IDENT",
  "CSS_TOK_HASH", "CSS_TOK_IMPORT_SYM", "CSS_TOK_PAGE_SYM",
  "CSS_TOK_MEDIA_SYM", "CSS_TOK_SUPPORTS_SYM", "CSS_TOK_FONT_FACE_SYM",
  "CSS_TOK_CHARSET_SYM", "CSS_TOK_NAMESPACE_SYM", "CSS_TOK_VIEWPORT_SYM",
  "CSS_TOK_KEYFRAMES_SYM", "CSS_TOK_ATKEYWORD", "CSS_TOK_IMPORTANT_SYM",
  "CSS_TOK_EMS", "CSS_TOK_REMS", "CSS_TOK_EXS", "CSS_TOK_LENGTH_PX",
  "CSS_TOK_LENGTH_CM", "CSS_TOK_LENGTH_MM", "CSS_TOK_LENGTH_IN",
  "CSS_TOK_LENGTH_PT", "CSS_TOK_LENGTH_PC", "CSS_TOK_ANGLE",
  "CSS_TOK_TIME", "CSS_TOK_FREQ", "CSS_TOK_DIMEN", "CSS_TOK_PERCENTAGE",
  "CSS_TOK_DPI", "CSS_TOK_DPCM", "CSS_TOK_DPPX", "CSS_TOK_NUMBER",
  "CSS_TOK_INTEGER", "CSS_TOK_N", "CSS_TOK_URI", "CSS_TOK_FUNCTION",
  "CSS_TOK_UNICODERANGE", "CSS_TOK_RGB_FUNC", "CSS_TOK_HSL_FUNC",
  "CSS_TOK_NOT_FUNC", "CSS_TOK_RGBA_FUNC", "CSS_TOK_HSLA_FUNC",
  "CSS_TOK_LINEAR_GRADIENT_FUNC", "CSS_TOK_WEBKIT_LINEAR_GRADIENT_FUNC",
  "CSS_TOK_O_LINEAR_GRADIENT_FUNC",
  "CSS_TOK_REPEATING_LINEAR_GRADIENT_FUNC", "CSS_TOK_RADIAL_GRADIENT_FUNC",
  "CSS_TOK_REPEATING_RADIAL_GRADIENT_FUNC", "CSS_TOK_DOUBLE_RAINBOW_FUNC",
  "CSS_TOK_ATTR_FUNC", "CSS_TOK_TRANSFORM_FUNC", "CSS_TOK_COUNTER_FUNC",
  "CSS_TOK_COUNTERS_FUNC", "CSS_TOK_RECT_FUNC",
  "CSS_TOK_DASHBOARD_REGION_FUNC", "CSS_TOK_OR", "CSS_TOK_AND",
  "CSS_TOK_NOT", "CSS_TOK_ONLY", "CSS_TOK_LANG_FUNC", "CSS_TOK_SKIN_FUNC",
  "CSS_TOK_LANGUAGE_STRING_FUNC", "CSS_TOK_NTH_CHILD_FUNC",
  "CSS_TOK_NTH_OF_TYPE_FUNC", "CSS_TOK_NTH_LAST_CHILD_FUNC",
  "CSS_TOK_NTH_LAST_OF_TYPE_FUNC", "CSS_TOK_FORMAT_FUNC",
  "CSS_TOK_LOCAL_FUNC", "CSS_TOK_CUBIC_BEZ_FUNC", "CSS_TOK_INTEGER_FUNC",
  "CSS_TOK_STEPS_FUNC", "CSS_TOK_CUE_FUNC", "CSS_TOK_EOF",
  "CSS_TOK_UNRECOGNISED_CHAR", "CSS_TOK_STYLE_ATTR",
  "CSS_TOK_STYLE_ATTR_STRICT", "CSS_TOK_DOM_STYLE_PROPERTY",
  "CSS_TOK_DOM_PAGE_PROPERTY", "CSS_TOK_DOM_FONT_DESCRIPTOR",
  "CSS_TOK_DOM_VIEWPORT_PROPERTY", "CSS_TOK_DOM_KEYFRAME_PROPERTY",
  "CSS_TOK_DOM_RULE", "CSS_TOK_DOM_STYLE_RULE", "CSS_TOK_DOM_CHARSET_RULE",
  "CSS_TOK_DOM_IMPORT_RULE", "CSS_TOK_DOM_SUPPORTS_RULE",
  "CSS_TOK_DOM_MEDIA_RULE", "CSS_TOK_DOM_FONT_FACE_RULE",
  "CSS_TOK_DOM_PAGE_RULE", "CSS_TOK_DOM_VIEWPORT_RULE",
  "CSS_TOK_DOM_KEYFRAMES_RULE", "CSS_TOK_DOM_KEYFRAME_RULE",
  "CSS_TOK_DOM_SELECTOR", "CSS_TOK_DOM_PAGE_SELECTOR",
  "CSS_TOK_DOM_MEDIA_LIST", "CSS_TOK_DOM_MEDIUM", "CSS_TOK_DECLARATION",
  "';'", "'{'", "','", "'}'", "':'", "')'", "'.'", "'/'", "'+'", "'-'",
  "'>'", "'*'", "'$'", "'~'", "'#'", "'('", "'['", "']'", "'|'", "'='",
  "$accept", "stylesheet", "$@1", "$@2", "$@3", "style_decl_token",
  "trailing_bracket", "quirk_style_attr", "$@4", "rule", "charset",
  "stylesheet_body", "$@5", "stylesheet_body_elem",
  "block_or_semi_recovery", "namespace", "namespace_uri", "end_of_ns",
  "unknown_at", "block", "block_elements", "block_element",
  "value_element", "core_declaration", "value", "any_function", "fn_name",
  "any", "any_list", "commenttags_spaces", "comtag_or_space", "spaces",
  "spaces_plus", "end_of_import", "$@6", "import", "string_or_uri",
  "supports_condition", "supports_condition_in_parens",
  "supports_negation", "supports_conjunction", "supports_disjunction",
  "supports_declaration_condition", "supports_start", "supports", "$@7",
  "media_start", "media", "$@8", "media_list", "media_query_list", "$@9",
  "media_query_or_eof", "media_query", "$@10", "$@11", "media_query_op",
  "media_query_exp_list", "media_query_exp", "page_name", "pseudo_page",
  "page_selector", "page", "$@12", "font_face", "$@13", "viewport", "$@14",
  "keyframe_selector", "keyframe_selector_list", "keyframe_ruleset",
  "$@15", "keyframe_ruleset_list", "$@16", "keyframes", "$@17", "operator",
  "combinator", "unary_operator", "property", "nested_statement",
  "group_rule_body", "$@18", "ruleset", "$@19", "declarations", "$@20",
  "$@21", "declaration", "selectors", "selector", "simple_selector",
  "$@22", "$@23", "$@24", "$@25", "$@26", "$@27", "$@28", "selector_parms",
  "selector_parms_notempty", "selector_parm", "not_parm", "class",
  "type_selector", "$@29", "ns_prefix", "elm_type", "element_name",
  "attr_name", "attrib", "id_or_str", "eq_op", "pseudo", "$@30",
  "nth_func", "nth_expr", "prio", "expr", "function_expr", "term", "$@31",
  "$@32", "$@33", "$@34", "$@35", "$@36", "$@37", "$@38", "steps_start",
  "transform_name", "transform_arg_list", "transform_arg", "format_list",
  "region_offsets", "integer", "percentage", "number", "num_term",
  "function", "expr_ignore", "hash", 0
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
     355,   356,   357,   358,   359,   360,   361,   362,   363,    59,
     123,    44,   125,    58,    41,    46,    47,    43,    45,    62,
      42,    36,   126,    35,    40,    91,    93,   124,    61
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   129,   130,   131,   130,   132,   130,   130,   130,   130,
     130,   130,   130,   130,   130,   130,   130,   133,   130,   130,
     130,   130,   130,   130,   130,   130,   130,   130,   130,   130,
     134,   134,   134,   134,   134,   135,   135,   136,   137,   136,
     138,   138,   138,   138,   138,   138,   138,   138,   138,   139,
     139,   140,   141,   140,   142,   142,   142,   142,   142,   142,
     142,   142,   142,   142,   142,   142,   143,   143,   144,   144,
     144,   145,   145,   146,   146,   147,   148,   149,   149,   150,
     150,   151,   151,   151,   151,   151,   151,   151,   151,   151,
     151,   151,   151,   152,   153,   153,   154,   155,   155,   155,
     155,   155,   155,   155,   155,   155,   155,   155,   155,   155,
     155,   155,   155,   155,   155,   155,   155,   155,   155,   155,
     155,   155,   155,   155,   155,   155,   155,   155,   155,   156,
     156,   156,   156,   156,   156,   156,   156,   156,   156,   156,
     156,   156,   156,   156,   156,   156,   156,   156,   156,   156,
     156,   156,   156,   156,   156,   156,   156,   156,   156,   156,
     156,   156,   156,   156,   156,   156,   156,   156,   156,   156,
     156,   156,   156,   156,   156,   156,   156,   156,   156,   156,
     157,   157,   158,   158,   159,   159,   159,   160,   160,   161,
     161,   162,   162,   163,   162,   164,   164,   165,   165,   166,
     166,   166,   166,   167,   167,   168,   169,   169,   170,   170,
     171,   172,   174,   173,   173,   173,   173,   173,   175,   177,
     176,   176,   178,   178,   179,   180,   179,   181,   181,   183,
     182,   184,   182,   182,   182,   182,   182,   185,   185,   185,
     186,   186,   187,   187,   187,   188,   189,   190,   190,   190,
     190,   192,   191,   191,   194,   193,   193,   196,   195,   195,
     197,   197,   198,   198,   200,   199,   199,   199,   202,   201,
     201,   204,   203,   203,   205,   205,   205,   206,   206,   206,
     207,   207,   208,   209,   209,   209,   209,   209,   209,   209,
     209,   209,   209,   209,   209,   210,   211,   210,   213,   212,
     212,   215,   214,   216,   214,   217,   217,   217,   217,   217,
     217,   217,   217,   218,   218,   219,   219,   219,   219,   219,
     220,   221,   222,   220,   223,   224,   220,   225,   226,   220,
     227,   220,   228,   228,   229,   230,   230,   230,   230,   230,
     231,   231,   232,   233,   234,   233,   233,   235,   235,   236,
     236,   237,   237,   238,   238,   238,   238,   239,   239,   240,
     240,   241,   241,   241,   241,   241,   241,   242,   242,   243,
     242,   242,   242,   244,   244,   244,   244,   245,   245,   245,
     245,   245,   245,   245,   246,   246,   247,   247,   248,   248,
     249,   249,   249,   249,   249,   249,   249,   249,   249,   249,
     249,   249,   249,   250,   249,   251,   249,   252,   249,   253,
     249,   254,   249,   255,   249,   256,   249,   249,   249,   249,
     249,   249,   249,   249,   249,   249,   249,   249,   257,   249,
     249,   249,   249,   249,   249,   249,   249,   249,   249,   258,
     258,   259,   260,   260,   261,   261,   262,   262,   263,   263,
     264,   264,   265,   265,   266,   266,   266,   267,   267,   267,
     267,   267,   267,   267,   267,   267,   267,   267,   267,   267,
     267,   267,   267,   267,   267,   267,   267,   268,   269,   269,
     270
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     3,     0,     6,     0,     4,     5,     5,     5,
       5,     5,     5,     5,     5,     5,     5,     0,     5,     5,
       4,     4,     4,     5,     5,     4,     5,     5,     5,     4,
       1,     1,     1,     1,     1,     0,     1,     3,     0,     7,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     5,
       2,     0,     0,     4,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     2,     2,     7,     5,
       2,     1,     1,     1,     1,     2,     4,     0,     2,     1,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     5,     1,     2,     4,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     4,     4,
       0,     3,     0,     2,     1,     1,     1,     0,     2,     1,
       2,     1,     1,     0,     4,     6,     2,     1,     1,     1,
       1,     1,     1,     5,     1,     3,     4,     4,     4,     4,
       5,     3,     0,     6,     2,     2,     3,     2,     3,     0,
       6,     2,     0,     1,     1,     0,     5,     1,     1,     0,
       5,     0,     4,     2,     2,     2,     2,     0,     2,     2,
       5,     0,     5,     8,     3,     1,     2,     2,     4,     4,
       5,     0,     6,     2,     0,     7,     2,     0,     7,     2,
       1,     1,     5,     1,     0,     7,     2,     2,     0,     4,
       0,     0,     9,     2,     2,     2,     0,     2,     2,     2,
       1,     1,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     0,     0,     4,     0,     6,
       2,     0,     2,     0,     5,     5,     7,     7,     7,     0,
       2,     2,     2,     1,     4,     1,     2,     3,     4,     3,
       2,     0,     0,     5,     0,     0,     6,     0,     0,     6,
       0,     2,     0,     2,     2,     1,     1,     1,     1,     1,
       6,     6,     2,     3,     0,     3,     1,     1,     1,     1,
       1,     1,     1,     3,     3,     2,     1,     5,     9,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     3,     0,
       7,     6,     5,     1,     1,     1,     1,     6,     7,     2,
       3,     2,     3,     2,     2,     0,     1,     3,     1,     3,
       2,     1,     2,     2,     2,     2,     1,    14,    14,    18,
      18,    14,    18,     0,     6,     0,     6,     0,     6,     0,
       6,     0,     6,     0,     6,     0,     6,     6,     6,    10,
      10,    14,     8,    14,     6,     9,     6,     6,     0,     6,
       5,    18,     6,     7,     2,     2,     2,     2,     4,     4,
       0,     1,     2,     4,     2,     1,     2,     5,     0,     4,
       2,     1,     2,     1,     2,     1,     1,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     5,     1,     3,
       2
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
     182,     5,     3,    30,    32,    33,    34,    31,   187,   187,
     187,   187,   187,   187,   187,   187,   187,   187,   187,   187,
     187,   187,   187,   187,     0,   187,    51,   187,   187,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    17,
     330,     0,     0,     0,     0,     1,     0,   184,   185,   186,
      52,   183,   301,   301,     0,   188,   351,     0,     0,   187,
       0,     0,     0,     0,     0,   352,   321,   187,    41,    42,
       0,    43,     0,    44,     0,    46,    45,    47,    48,    40,
       0,   313,   315,   332,   332,   187,   187,   187,   187,   187,
     187,   187,   187,   187,     0,     0,   187,     0,     0,   187,
     187,     0,   223,   224,     0,     0,     0,     0,   187,     0,
       0,   187,   187,   187,   187,   187,   187,   187,   187,   187,
     187,   187,   187,   187,   187,   187,   187,   187,   187,   187,
     187,   187,   187,   187,   187,   187,   187,   187,   187,   187,
     187,   403,   405,   407,   409,   411,   413,   415,   187,   441,
     187,   187,   187,   187,   187,   187,   187,   187,   187,   187,
     187,   187,   281,   280,   187,   187,   187,     0,   276,   386,
     187,   391,   476,   396,     2,     0,    38,     6,    35,     0,
      35,   300,   327,     0,   196,     0,   253,   247,     0,   214,
       0,   256,     0,    50,     0,   259,     0,   273,     0,   324,
       0,     0,     0,   217,   187,   215,   221,   187,   187,   187,
     187,   189,   187,   187,   187,   316,   330,     0,   331,   320,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     260,   453,   263,   187,     0,     0,   261,    20,    21,   236,
     235,   233,   234,   239,   238,    22,     0,     0,     0,     0,
     187,   187,    25,     0,     0,     0,   312,   310,   311,   282,
     187,    29,   392,   393,   480,   435,   466,   467,   468,   460,
     461,   462,   463,   464,   465,   469,   470,   471,   472,   459,
     473,   474,   475,   457,   458,   394,     0,   395,     0,     0,
       0,     0,   187,   187,   187,   187,   187,   187,   187,     0,
       0,     0,     0,     0,     0,     0,     0,   428,     0,     0,
       0,    77,   436,   437,   434,   390,   187,   187,   187,     0,
       0,     0,     0,     0,     0,     0,    62,   182,    60,    61,
      59,    55,    56,    57,    58,    63,    64,    54,   187,    36,
       0,     0,   302,     0,     0,    66,    67,   197,   198,   187,
     245,     0,   187,   187,   218,   187,   187,   211,   202,   199,
     200,   201,   204,   187,   187,   187,   187,     0,   351,   352,
     322,    19,   216,   212,   219,   251,   298,   330,   277,   278,
     279,   190,   330,   319,   317,   335,     0,     0,   187,   334,
     339,   336,   337,   338,   333,     8,     9,    10,    11,    12,
      13,    14,    15,    16,   266,   267,     0,    18,   452,    23,
      24,   187,     0,     0,   241,   229,    26,    27,    28,     0,
     478,   276,   451,     0,   187,   187,   455,     0,   456,   187,
     187,   187,   187,     0,     0,     0,     0,     0,     0,     0,
     187,   187,   187,     0,   187,   187,   187,     0,   187,   360,
     359,   187,     0,     0,   187,   187,   187,     0,   384,   275,
     274,   387,     7,     0,   442,   445,   187,   187,    65,    70,
       0,    75,    53,   301,    37,   187,     4,   328,     0,   246,
     249,   187,   248,     0,     0,   187,   187,   187,   187,   254,
       0,   257,     0,   325,   332,   295,   295,   301,   301,   314,
     318,   367,   187,   187,   373,   374,   375,   376,     0,   187,
     342,     0,   187,   187,     0,   244,   187,   232,   241,   276,
     187,     0,   450,     0,     0,   454,     0,     0,     0,     0,
     276,   388,   276,   276,   276,   276,   276,   276,     0,     0,
       0,   187,     0,     0,     0,     0,     0,   446,   187,   187,
     276,     0,     0,   440,   156,   158,   159,   160,   161,   151,
     152,   129,   154,   187,   187,   187,   187,   187,   187,   187,
     187,   187,   187,   133,   134,   135,   136,   137,   138,   139,
     140,   141,   142,   143,   144,   145,   150,   146,   147,   148,
     149,   130,   131,   132,   153,   128,   155,   122,   106,   112,
     121,   105,   110,   126,   127,   119,   117,   120,   103,    97,
     125,    98,    99,   118,   102,   162,   163,   164,   165,   108,
     123,   109,   113,   116,   114,   115,   104,   111,   100,   107,
     124,   101,   166,   187,   169,    76,   167,   168,   170,   171,
     172,   173,   174,   175,   176,   177,   187,   187,   187,    78,
      79,   157,   187,   187,   444,     0,   438,    71,   187,    72,
     187,   303,     0,   332,     0,   250,   205,   187,     0,     0,
       0,     0,     0,     0,   301,    49,   301,   187,   332,   323,
     296,   296,   303,   303,     0,     0,   368,   187,     0,   356,
       0,     0,   187,   264,     0,   228,   226,   227,     0,   187,
     230,     0,   477,   479,   187,   187,   187,   187,   187,   187,
     187,     0,   187,   187,   187,   187,   187,   187,   187,   187,
     187,   187,     0,     0,   187,   187,   187,   187,     0,   430,
     187,   187,   187,   187,     0,    83,    84,    86,    85,    87,
      88,    89,    90,    91,    92,    80,   180,   180,    82,   180,
      81,   443,     0,     0,   187,   304,   329,   192,   191,   193,
     195,     0,   187,   187,   208,   206,   207,   209,   303,   303,
     271,   326,   213,     0,   220,   252,   299,   349,   350,   344,
     187,   187,     0,   346,   187,   369,   187,   187,   187,     0,
       0,     0,     0,   355,     0,   301,   262,   187,   242,     0,
       0,     0,     0,     0,     0,     0,     0,   404,   389,   406,
     408,   410,   412,   414,   416,   417,     0,   418,     0,     0,
       0,   448,   424,   427,   426,   187,   429,     0,   432,     0,
     187,     0,     0,     0,   187,    74,    73,    69,     0,     0,
     187,   210,   203,   255,   258,   270,     0,   289,   288,   293,
     287,   291,   284,   285,   286,   292,   290,   187,   283,     0,
       0,     0,     0,     0,   330,   383,   381,   379,   187,   187,
     372,   353,   354,   362,   363,   364,   365,   366,   357,   361,
     187,   303,     0,   187,   308,   306,   307,   187,   187,   187,
     187,   187,   187,   187,   187,   187,   187,     0,     0,   447,
     187,   187,   433,   178,   187,   179,    96,     0,    39,     0,
       0,   268,   294,   297,   349,   350,   345,   340,   341,   343,
     371,     0,   187,   382,   380,     0,   265,   276,   240,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   422,   187,
       0,     0,   439,   181,    68,   194,    94,    93,   272,     0,
     370,     0,   187,   187,   243,   187,   187,   187,   187,   187,
     187,   187,   187,   187,     0,   425,     0,   187,    95,   187,
     187,     0,     0,     0,     0,     0,     0,     0,     0,   419,
       0,   420,   187,   449,     0,   269,   377,   187,   358,   187,
     187,   187,   187,   187,   187,   187,     0,   187,   378,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   187,   187,
     187,   187,   187,   187,   187,   187,   187,   398,   397,   401,
       0,     0,     0,   421,   423,     0,   187,   187,   187,   187,
       0,     0,     0,     0,   187,   187,   187,   187,   400,   399,
     402,   431
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    24,    28,    27,    94,    25,   340,   177,   338,    67,
      68,    50,   175,   327,   184,   328,   660,   837,   329,   166,
     457,   649,   650,   668,   947,   651,   652,   653,   831,    26,
      51,   187,   215,   760,   839,    69,   349,   357,   358,   359,
     360,   361,   362,    70,    71,   495,    72,    73,   496,   101,
     102,   248,   696,   103,   518,   104,   105,   517,   250,   352,
     353,    74,    75,   497,    76,   674,    77,   676,   232,   233,
     234,   795,   911,   949,    78,   845,   711,   216,   167,   109,
     857,   680,   773,    79,   498,   178,   179,   341,   110,    80,
      81,    82,   200,   494,   367,   678,   344,   663,    83,   217,
     218,   394,   390,   391,   781,   859,   782,   783,    84,   692,
     392,   451,   880,   393,   864,   509,   790,   320,   168,   530,
     531,   292,   293,   294,   295,   296,   297,   298,   453,   734,
     170,   322,   464,   452,   897,   428,   236,   429,   171,   172,
     421,   173
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -691
static const yytype_int16 yypact[] =
{
     968,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,    89,  -691,   923,  -691,  -691,   792,
     208,   376,   789,   645,    84,   662,   677,    72,   613,   138,
     195,   735,   468,   190,   630,  -691,  1093,  -691,  -691,  -691,
      76,  -691,   304,   138,    79,  -691,   101,    87,   541,  -691,
      70,   276,   826,   276,   833,   106,  -691,  -691,  -691,  -691,
     303,  -691,   520,  -691,   171,  -691,  -691,  -691,  -691,  -691,
     866,  -691,   131,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,   371,   530,  -691,   221,   732,  -691,
    -691,   749,   218,  -691,   254,   369,   741,   558,  -691,   298,
     365,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  1150,   712,  -691,
    -691,  -691,  -691,  -691,  -691,   248,  -691,  -691,   -65,   497,
     -65,  -691,  -691,   -16,  -691,   753,  -691,   438,   243,  -691,
      94,  -691,   568,  -691,   585,  -691,   623,  -691,   608,  -691,
     206,   270,   562,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,   405,   216,   268,  -691,   268,
     382,   540,   592,   593,   601,   602,   646,   657,   658,   798,
    -691,  -691,  -691,  -691,   551,   627,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,   138,   138,  -691,   574,   589,   578,   834,
    -691,  -691,  -691,   652,   656,   675,  -691,  -691,  -691,   138,
    -691,  -691,   138,   138,   138,   138,   138,   138,   138,   138,
     138,   138,   138,   138,   138,   138,   138,   138,   138,   138,
     138,   138,   138,   138,   138,   138,  1093,   138,   280,   385,
     280,   385,  -691,  -691,  -691,  -691,  -691,  -691,  -691,   669,
     740,   842,  1093,   853,   862,   452,   850,   138,   385,   432,
     432,   138,   138,   138,   138,  -691,  -691,  -691,  -691,  2139,
     690,  1998,   342,   372,   818,   797,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
     729,   739,  -691,   787,   206,  -691,  -691,  -691,  -691,  -691,
    -691,   878,   764,  -691,  -691,  -691,  -691,  -691,   896,  -691,
     843,   828,  -691,  -691,  -691,  -691,  -691,   206,  -691,  -691,
    -691,  -691,  -691,   138,   138,   138,   138,   195,   138,   138,
     138,  -691,   216,  -691,  -691,  -691,   726,   889,  -691,   476,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,   315,  -691,  -691,  -691,
    -691,  -691,   801,   854,   138,   138,  -691,  -691,  -691,  1093,
    -691,   819,  -691,   188,  -691,  -691,  -691,   955,  -691,  -691,
    -691,  -691,  -691,  1093,  1093,  1093,  1093,  1093,  1093,  1093,
    -691,  -691,  -691,  2039,  -691,  -691,  -691,   885,  -691,  -691,
    -691,  -691,   576,  2139,  -691,  -691,  -691,  1538,   138,   138,
     138,  -691,  -691,  1150,  -691,  -691,  -691,  -691,  -691,  -691,
     772,  -691,   923,   138,  -691,  -691,  -691,  -691,   468,  -691,
     138,  -691,   138,    73,   182,  -691,  -691,  -691,  -691,   138,
     223,   138,   624,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,    52,  -691,
    -691,   211,  -691,  -691,   533,  -691,  -691,   869,  -691,   253,
    -691,  2139,  -691,   134,   214,  -691,   343,   404,   406,   424,
     827,  -691,   856,   864,   877,   881,   899,   973,    45,   228,
     488,  -691,  2139,   865,   133,   220,   252,   138,  -691,  -691,
     974,   489,   346,   523,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  1998,   138,  -691,  -691,  -691,
    -691,   830,   619,  -691,   760,   138,  -691,  -691,   832,   846,
      73,    73,    73,    73,  -691,  -691,  -691,  -691,  -691,   268,
     857,   861,   882,   884,   337,   886,  -691,  -691,   357,   813,
     839,   977,  -691,   138,    59,  -691,  -691,  -691,   317,  -691,
     869,   506,   138,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  2139,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  1093,  2139,  -691,  -691,  -691,  -691,   850,   138,
    -691,  -691,  -691,  -691,   902,   138,   138,   138,   138,   138,
     138,   138,   138,   138,   138,   138,   138,   138,   138,   138,
     138,  -691,   778,    83,  -691,  -691,   268,  -691,  -691,  -691,
    -691,    61,  -691,  -691,  -691,  -691,  -691,  -691,   905,   910,
     138,   268,  -691,   279,  -691,  -691,  -691,   897,   898,  -691,
    -691,  -691,   903,  -691,  -691,   138,  -691,  -691,  -691,   967,
     909,  1001,  1018,  -691,    60,  -691,  -691,  -691,  -691,    92,
     648,   432,   272,   272,   432,   272,   272,   138,  -691,   138,
     138,   138,   138,   138,   138,   138,   887,   138,   894,   940,
     938,  1006,   138,   138,   138,  -691,   138,   385,   138,   891,
    -691,  1778,  1417,  1898,  -691,  -691,  -691,  -691,   665,  1076,
    -691,   138,   138,  -691,  -691,  -691,   806,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,   615,
     356,   373,   615,   392,   216,   138,   138,    37,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,   979,  1093,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,   972,  1150,   138,
    -691,  -691,   138,  -691,  -691,  -691,  -691,    83,  -691,   982,
    1297,   986,  -691,   138,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,   647,  -691,   138,    37,   850,  -691,   981,   138,   526,
     536,   542,   544,   545,   577,   397,   417,  1093,   138,  -691,
    1150,   581,   138,   138,  -691,  -691,  -691,  1658,  -691,   371,
    -691,    58,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,   988,   138,  1150,  -691,  -691,  -691,
    -691,   135,    35,   432,   272,   272,   432,   272,   272,   138,
     900,   138,  -691,  -691,   385,   138,   138,  -691,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  1093,  -691,   138,   402,
     409,   439,   604,   609,   610,   460,   987,   616,  -691,  -691,
    -691,  -691,  -691,  -691,  -691,  -691,  -691,   138,   138,   138,
     385,   385,   385,   138,   138,   385,  -691,  -691,  -691,  -691,
     465,   469,   480,   496,  -691,  -691,  -691,  -691,   138,   138,
     138,   138
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -691,  -691,  -691,  -691,  -691,  -691,   913,  -691,  -691,  -691,
      -9,  -691,  -691,  -691,   120,   327,   350,   196,   335,  -426,
    -691,  -691,  -690,  -691,  -691,  -691,  -691,   125,   187,   782,
    -691,    -8,  -691,  -691,  -691,    -7,  -691,   626,  -433,  -691,
    -691,  -691,  -691,  -691,    -5,  -691,  -691,    -4,  -691,  -151,
    -691,  -691,  -691,    -1,  -691,  -691,  -691,   594,   312,  -691,
     761,  1073,   -10,  -691,    -3,  -691,    -2,  -691,   421,  -691,
     167,  -691,  -691,  -691,   -11,  -691,  -145,   945,   277,  -691,
    -691,   622,  -691,    -6,  -691,   -12,  -691,  -691,  -143,   -24,
    -170,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,   -55,
    -691,  -184,  -691,  -691,  -691,  -691,  -691,    14,  -157,  -691,
    -691,  -689,  -691,  -691,  -691,  -691,  -691,   642,  -385,   486,
     -28,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,  -691,
    -691,  -691,   483,  -691,  -691,   -75,  -230,  -234,  -146,  -691,
    -691,  -691
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -386
static const yytype_int16 yytable[] =
{
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    95,    46,   169,    52,
      53,   315,    86,   319,    85,    87,    91,    93,    88,   219,
      89,   648,    90,   389,   519,    92,   342,   354,    55,   825,
      55,   180,   106,   370,  -303,   383,   384,   339,    55,   185,
     666,   188,   190,   192,   194,   196,   198,   432,   425,   201,
     431,    55,    55,    55,    55,   686,   873,   874,   875,   876,
     877,   183,   230,  -187,   454,    55,    55,   220,   221,   222,
     223,   224,   225,   226,   227,   228,    55,    55,   183,    45,
    -187,   243,   244,   345,    63,    55,   346,    55,   231,  -187,
     259,    59,   970,   262,   263,   264,   265,   266,   267,   268,
     269,   270,   271,   272,   273,   274,   275,   276,   277,   278,
     279,   280,   281,   282,   283,   284,   285,   286,   287,   288,
     289,   290,   291,  -187,   211,   687,    55,    55,    55,  -187,
     299,    55,   300,   301,   302,   303,   304,   305,   306,   307,
     308,   309,   310,   311,   162,   163,   312,   313,   314,   718,
     174,   988,   321,   355,   336,   333,   326,   835,   330,   337,
     331,   332,   334,   335,   840,   465,   162,   163,   186,   987,
     189,   191,   193,   195,   197,    55,   878,   477,   879,   181,
     205,    98,   836,    55,  -187,   667,   373,   356,    55,   374,
     375,   376,   377,  -237,   378,   379,   380,   499,    56,    54,
     493,    55,   500,   424,    55,   430,   249,    55,   356,   368,
     946,    56,  -330,    55,   689,   406,    55,   408,   182,    56,
     448,    55,   522,   199,   455,   456,   953,   764,   765,   766,
     767,   413,   414,   415,    98,   704,    55,   725,   212,   323,
     213,   355,   419,   214,  -385,    55,  -237,   968,   420,    99,
     100,    56,  -330,    57,    58,    59,    60,    61,    62,   324,
      63,    64,   325,    55,   443,    55,   521,   183,   316,  -187,
     846,   208,   385,    55,   433,   434,   435,   436,   437,   438,
     439,   461,    56,  -330,    57,    58,    59,    60,    61,    62,
     324,    63,    64,   325,   202,   238,   356,    55,   458,   459,
     460,   231,    99,   100,  -231,    65,   470,   654,    55,   231,
      55,  -330,    66,  -330,   422,   705,   369,   664,    65,  -225,
     473,   690,   675,  -330,   726,    66,    65,  -385,   691,   719,
      55,   478,   720,    66,   480,   482,    55,   483,   484,    55,
     777,   385,  -222,  -222,   371,   489,   490,   491,   492,    55,
      55,  -330,  -385,  -330,   317,  -385,   727,  -231,    65,   318,
     786,   235,   229,  -330,   319,    66,    55,   468,   249,    55,
     511,   386,   251,   387,   230,    55,  -187,   203,    55,   162,
     163,   169,  -330,   388,  -330,    55,    62,   162,   163,    65,
      55,   787,   788,   514,  -330,    55,    66,    55,   381,    55,
     231,   260,    55,   204,   176,   542,   523,   524,    56,  -330,
      55,   526,   527,   528,   529,   512,   513,    55,   426,   422,
     797,   798,   538,   539,   540,    55,   543,   544,   545,   679,
     546,    55,    55,   547,   469,   471,   551,   552,   553,   261,
     386,   350,   387,   466,   706,    55,   467,   778,   655,   656,
     732,   661,   388,    55,   779,   446,   395,   662,    55,    98,
     917,    55,    55,   665,   162,   163,   422,   670,   671,   672,
     673,  -237,   181,    55,   648,   682,   683,   918,   162,   163,
    -333,    55,    55,   703,   684,   685,   422,   927,   107,    55,
     780,   688,   162,   163,   693,   694,   920,   800,   698,   465,
     108,   961,   702,   697,   723,   707,  1008,   708,  -330,   755,
    -330,   648,   212,  1009,   213,    65,    55,   214,   962,    55,
    -330,   963,    66,   722,    98,   709,    55,    99,   100,    55,
     728,   729,   183,    55,  -187,    55,  -237,    55,    55,   162,
     163,   351,  -222,  1010,  -187,   735,   736,   737,   738,   739,
     740,   741,   742,   743,   744,   423,   427,   423,   427,   162,
     163,    55,   888,   889,  1014,   891,   892,  -222,  -222,  1034,
      55,  -309,   447,  1035,    55,   427,   447,   447,    55,  -333,
    -305,  -333,  -231,   900,  1036,    55,    55,   364,   463,   721,
     731,  -333,    99,   100,    55,    55,  -309,    55,   756,  -309,
    1037,    55,    55,    55,   237,  -305,    55,   695,  -305,    55,
     107,   366,    55,   771,   396,   745,    55,    55,   914,   206,
     207,   107,   108,    55,   733,   407,    64,   955,   746,   747,
     748,   210,   256,   108,   749,   750,   372,   956,    55,    55,
     752,  -187,   753,   957,  -187,   958,   959,  -231,   409,   761,
      55,    55,   768,    60,   769,    55,   408,   257,    55,   770,
     258,   345,    55,   410,   346,   898,   397,   398,   363,   785,
      55,    61,   440,   808,   794,   399,   400,   548,   960,   411,
     549,   799,   967,    58,   819,   820,   801,   802,   803,   804,
     805,   806,   807,  -309,   809,   810,   811,   812,   813,   814,
     815,   816,   817,   818,  -309,  1011,   821,   822,   823,   824,
    1012,  1013,   826,   827,   828,   829,   887,  1016,  -309,   890,
     401,  -309,   884,   365,   677,   915,   416,   316,    55,   501,
     417,   402,   403,    55,   990,   991,   838,   993,   994,   908,
     997,    96,   940,   441,   841,   842,    55,   885,   210,   418,
     886,   950,   856,   853,   847,   347,   850,   858,   851,   852,
     854,   855,   860,   861,   462,    55,   863,   502,   865,   866,
     867,    55,   319,   881,   657,   658,  1026,  1027,  1028,   882,
     657,  1029,    55,    54,   966,    55,  -385,   503,   183,   348,
     504,   505,   506,   507,    57,    56,  -330,    57,    58,    59,
      60,    61,    62,   474,    63,    64,   239,   899,   659,   183,
     983,  -187,   902,   317,   659,   252,   907,   183,   318,  -187,
    -187,  -187,   910,   245,   183,   412,  -187,  -187,  -187,   508,
     921,   240,   241,   242,   757,    55,  -187,  -187,   475,   913,
     253,   254,   255,    55,   169,   442,    55,    55,   246,   247,
     923,   924,   449,   450,  -187,    55,   444,   516,    55,   758,
     759,   476,   925,   916,   445,   928,   919,   351,   724,   929,
     930,   931,   932,   933,   934,   935,   936,   937,   938,    55,
      55,   479,   941,   942,    55,   488,   943,    55,   989,   784,
     893,   992,   510,    55,   901,  -330,   894,  -330,   404,   964,
     405,   487,    65,   995,   951,   515,   181,  -330,   912,    66,
     532,   533,   534,   535,   536,   537,    47,    48,    49,   522,
     317,   965,   463,   520,   832,   318,   833,   699,   317,   550,
     791,   710,   754,   318,   971,   972,   762,   973,   974,   975,
     976,   977,   978,   979,   980,   981,   904,   904,   904,   984,
     763,   985,   986,   485,   486,   789,   792,   317,  1006,   772,
     712,   235,   318,   774,   996,   317,   209,   210,   713,   998,
     318,   999,  1000,  1001,  1002,  1003,  1004,  1005,   317,  1007,
     793,   714,   317,   318,   775,   715,   776,   318,   525,   522,
    1017,  1018,  1019,  1020,  1021,  1022,  1023,  1024,  1025,    55,
     317,   868,   869,   716,   871,   318,   830,   843,  1030,  1031,
    1032,  1033,   844,   870,  -347,  -348,  1038,  1039,  1040,  1041,
     862,   872,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   895,   896,   135,     1,     2,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,   909,   447,   235,
     235,   447,   235,   235,   317,   317,   939,   717,   730,   318,
     318,   926,   317,   343,   945,   954,    55,   318,   948,   982,
     848,  1015,   834,   944,   427,   111,   112,   113,   849,   472,
     669,   883,   700,   481,    97,   796,   969,   114,   681,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   751,   134,
     135,   136,   137,   138,   922,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     382,   701,     0,     0,     0,   154,   155,     0,     0,     0,
       0,   156,   157,   158,   159,   160,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,     0,     0,   135,     0,     0,
       0,   952,     0,   161,     0,     0,     0,     0,     0,     0,
     162,   163,     0,   164,     0,     0,   165,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   235,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     447,   235,   235,   447,   235,   235,     0,     0,     0,     0,
       0,   427,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   427,   427,   427,
      55,     0,   427,   554,   555,   556,   557,   558,   559,   560,
     561,   562,   563,   564,   565,   566,   567,   568,   569,   570,
     571,   572,   573,   574,   575,   576,   577,   578,   579,   580,
     581,   582,   583,   584,   585,   586,   587,   588,   589,   590,
     591,   592,   593,   594,   595,   596,   597,   598,   599,   600,
     601,   602,   603,   604,   605,   606,   607,   608,   609,   610,
     611,   612,   613,   614,   615,   616,   617,   618,   619,   620,
     621,   622,   623,   624,   625,   626,   627,   628,   629,   630,
     631,     0,   632,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   161,   634,     0,
     636,     0,   637,   638,   639,   640,   641,   642,   643,   644,
     645,   646,   647,   554,   555,   556,   557,   558,   559,   560,
     561,   562,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   573,   574,   575,   576,   577,   578,   579,   580,
     581,   582,   583,   584,   585,   586,   587,   588,   589,   590,
     591,   592,   593,   594,   595,   596,   597,   598,   599,   600,
     601,   602,   603,   604,   605,   606,   607,   608,   609,   610,
     611,   612,   613,   614,   615,   616,   617,   618,   619,   620,
     621,   622,   623,   624,   625,   626,   627,   628,   629,   630,
     631,     0,   632,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   634,     0,
     636,     0,   637,   638,   639,   640,   641,   642,   643,   644,
     645,   646,   647,   905,   554,   555,   556,   557,   558,   559,
     560,   561,   562,   563,   564,   565,   566,   567,   568,   569,
     570,   571,   572,   573,   574,   575,   576,   577,   578,   579,
     580,   581,   582,   583,   584,   585,   586,   587,   588,   589,
     590,   591,   592,   593,   594,   595,   596,   597,   598,   599,
     600,   601,   602,   603,   604,   605,   606,   607,   608,   609,
     610,   611,   612,   613,   614,   615,   616,   617,   618,   619,
     620,   621,   622,   623,   624,   625,   626,   627,   628,   629,
     630,   631,     0,   632,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   633,   161,   634,
     635,   636,     0,   637,   638,   639,   640,   641,   642,   643,
     644,   645,   646,   647,   554,   555,   556,   557,   558,   559,
     560,   561,   562,   563,   564,   565,   566,   567,   568,   569,
     570,   571,   572,   573,   574,   575,   576,   577,   578,   579,
     580,   581,   582,   583,   584,   585,   586,   587,   588,   589,
     590,   591,   592,   593,   594,   595,   596,   597,   598,   599,
     600,   601,   602,   603,   604,   605,   606,   607,   608,   609,
     610,   611,   612,   613,   614,   615,   616,   617,   618,   619,
     620,   621,   622,   623,   624,   625,   626,   627,   628,   629,
     630,   631,     0,   632,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   161,   634,
       0,   636,     0,   637,   638,   639,   640,   641,   642,   643,
     644,   645,   646,   647,   554,   555,   556,   557,   558,   559,
     560,   561,   562,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   573,   574,   575,   576,   577,   578,   579,
     580,   581,   582,   583,   584,   585,   586,   587,   588,   589,
     590,   591,   592,   593,   594,   595,   596,   597,   598,   599,
     600,   601,   602,   603,   604,   605,   606,   607,   608,   609,
     610,   611,   612,   613,   614,   615,   616,   617,   618,   619,
     620,   621,   622,   623,   624,   625,   626,   627,   628,   629,
     630,   631,     0,   632,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   634,
       0,   636,   903,   637,   638,   639,   640,   641,   642,   643,
     644,   645,   646,   647,   554,   555,   556,   557,   558,   559,
     560,   561,   562,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   573,   574,   575,   576,   577,   578,   579,
     580,   581,   582,   583,   584,   585,   586,   587,   588,   589,
     590,   591,   592,   593,   594,   595,   596,   597,   598,   599,
     600,   601,   602,   603,   604,   605,   606,   607,   608,   609,
     610,   611,   612,   613,   614,   615,   616,   617,   618,   619,
     620,   621,   622,   623,   624,   625,   626,   627,   628,   629,
     630,   631,     0,   632,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    55,     0,     0,     0,     0,     0,     0,     0,   634,
       0,   636,   906,   637,   638,   639,   640,   641,   642,   643,
     644,   645,   646,   647,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,     0,     0,   135,     0,     0,     0,     0,
       0,   111,   112,   113,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   114,     0,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,     0,   134,   135,   136,   137,   138,
       0,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,   153,     0,     0,     0,     0,
       0,   154,   155,     0,     0,   162,   163,   156,   157,   158,
     159,   160,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   161,
     541,   111,   112,   113,     0,     0,   162,   163,     0,   164,
       0,     0,   165,   114,     0,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,     0,   134,   135,   136,   137,   138,
       0,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,   153,     0,     0,     0,     0,
       0,   154,   155,     0,     0,     0,     0,   156,   157,   158,
     159,   160,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   161,
       0,     0,     0,     0,     0,     0,   162,   163,     0,   164,
       0,     0,   165
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-691))

#define yytable_value_is_error(yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    40,    25,    46,    27,
      28,   167,    31,   168,    30,    32,    36,    38,    33,    84,
      34,   457,    35,   217,   419,    37,   179,   188,     3,   728,
       3,    53,    43,   200,   109,   215,   216,   112,     3,    57,
     483,    59,    60,    61,    62,    63,    64,   291,   288,    67,
     290,     3,     3,     3,     3,    13,     6,     7,     8,     9,
      10,     1,    13,     3,   308,     3,     3,    85,    86,    87,
      88,    89,    90,    91,    92,    93,     3,     3,     1,     0,
       3,    99,   100,   109,    22,     3,   112,     3,    39,    12,
     108,    17,    44,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,    46,     3,    83,     3,     3,     3,    69,
     148,     3,   150,   151,   152,   153,   154,   155,   156,   157,
     158,   159,   160,   161,   117,   118,   164,   165,   166,   114,
      84,   126,   170,    69,   175,   175,   175,    84,   175,   175,
     175,   175,   175,   175,   113,   321,   117,   118,    58,    44,
      60,    61,    62,    63,    64,     3,   126,   344,   128,   110,
      70,     1,   109,     3,   124,    13,   204,   124,     3,   207,
     208,   209,   210,    13,   212,   213,   214,   377,    13,     1,
     367,     3,   382,   288,     3,   290,   124,     3,   124,    13,
     910,    13,    14,     3,    13,   233,     3,    39,   127,    13,
     305,     3,    44,   127,   309,   310,   925,   670,   671,   672,
     673,   249,   250,   251,     1,   111,     3,   114,   117,     1,
     119,    69,   260,   122,     1,     3,    13,   947,   286,    69,
      70,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,     3,   302,     3,   421,     1,    25,     3,
       1,   110,    14,     3,   292,   293,   294,   295,   296,   297,
     298,   319,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,     1,    84,   124,     3,   316,   317,
     318,    39,    69,    70,   124,   120,   324,   463,     3,    39,
       3,   113,   127,   115,    44,   111,   120,   478,   120,   111,
     338,   120,   109,   125,   114,   127,   120,    84,   127,   111,
       3,   349,   114,   127,   352,   353,     3,   355,   356,     3,
      13,    14,   109,   110,    84,   363,   364,   365,   366,     3,
       3,   113,   109,   115,   111,   112,   114,   124,   120,   116,
      13,    94,     1,   125,   519,   127,     3,     5,   124,     3,
     388,   113,    13,   115,    13,     3,   110,    84,     3,   117,
     118,   419,   113,   125,   115,     3,    20,   117,   118,   120,
       3,    44,    45,   411,   125,     3,   127,     3,     3,     3,
      39,   113,     3,   110,   110,   443,   424,   425,    13,    14,
       3,   429,   430,   431,   432,   110,   111,     3,    43,    44,
     113,   114,   440,   441,   442,     3,   444,   445,   446,   494,
     448,     3,     3,   451,   324,   325,   454,   455,   456,    84,
     113,    13,   115,   111,   111,     3,   114,   120,   466,   467,
     114,   473,   125,     3,   127,    13,    84,   475,     3,     1,
     114,     3,     3,   481,   117,   118,    44,   485,   486,   487,
     488,    13,   110,     3,   910,   497,   498,   114,   117,   118,
      14,     3,     3,   521,   502,   503,    44,   882,     1,     3,
     684,   509,   117,   118,   512,   513,   114,     1,   516,   655,
      13,   114,   520,   514,   542,   111,   114,   111,   113,   662,
     115,   947,   117,   114,   119,   120,     3,   122,   111,     3,
     125,   114,   127,   541,     1,   111,     3,    69,    70,     3,
     548,   549,     1,     3,     3,     3,    13,     3,     3,   117,
     118,   113,    84,   114,    13,   563,   564,   565,   566,   567,
     568,   569,   570,   571,   572,   288,   289,   290,   291,   117,
     118,     3,   802,   803,   114,   805,   806,   109,   110,   114,
       3,    84,   305,   114,     3,   308,   309,   310,     3,   113,
      84,   115,   124,   827,   114,     3,     3,    12,   321,   111,
     111,   125,    69,    70,     3,     3,   109,     3,   663,   112,
     114,     3,     3,     3,    84,   109,     3,    84,   112,     3,
       1,    13,     3,   678,    84,   633,     3,     3,    13,   109,
     110,     1,    13,     3,   111,    84,    23,   111,   646,   647,
     648,   111,    84,    13,   652,   653,    84,   111,     3,     3,
     658,   110,   660,   111,   113,   111,   111,   124,    84,   667,
       3,     3,   674,    18,   676,     3,    39,   109,     3,   677,
     112,   109,     3,    84,   112,   821,    84,    84,   110,   687,
       3,    19,    13,   711,   692,    84,    84,   111,   111,   111,
     114,   699,   111,    16,   722,   723,   704,   705,   706,   707,
     708,   709,   710,    84,   712,   713,   714,   715,   716,   717,
     718,   719,   720,   721,    84,   111,   724,   725,   726,   727,
     111,   111,   730,   731,   732,   733,   801,   111,   109,   804,
      84,   112,    84,   110,   110,   120,    84,    25,     3,    13,
      84,    84,    84,     3,   974,   975,   754,   977,   978,    84,
     984,    16,   898,    13,   762,   763,     3,   109,   111,    84,
     112,   114,   773,   773,   773,    12,   773,   773,   773,   773,
     773,   773,   780,   781,    84,     3,   784,    51,   786,   787,
     788,     3,   927,   795,    12,    13,  1020,  1021,  1022,   797,
      12,  1025,     3,     1,   940,     3,    84,    71,     1,    46,
      74,    75,    76,    77,    15,    13,    14,    15,    16,    17,
      18,    19,    20,    84,    22,    23,    84,   825,    46,     1,
     966,     3,   830,   111,    46,    84,   834,     1,   116,     3,
      12,    13,   840,    84,     1,     1,     3,     3,    12,   113,
     864,   109,   110,   111,    84,     3,    13,    13,   109,   857,
     109,   110,   111,     3,   882,    13,     3,     3,   109,   110,
     868,   869,    12,    13,    46,     3,    13,    13,     3,   109,
     110,    84,   880,   859,    12,   883,   862,   113,    13,   887,
     888,   889,   890,   891,   892,   893,   894,   895,   896,     3,
       3,    13,   900,   901,     3,    67,   904,     3,   973,    13,
      13,   976,    13,     3,    13,   113,    12,   115,   110,   937,
     112,    68,   120,    13,   922,   114,   110,   125,   112,   127,
     434,   435,   436,   437,   438,   439,     3,     4,     5,    44,
     111,   939,   655,   114,   747,   116,   749,    68,   111,   453,
     127,   114,   112,   116,   952,   953,   114,   955,   956,   957,
     958,   959,   960,   961,   962,   963,   831,   832,   833,   967,
     114,   969,   970,    67,    68,   688,   127,   111,   996,   112,
     114,   694,   116,   112,   982,   111,   110,   111,   114,   987,
     116,   989,   990,   991,   992,   993,   994,   995,   111,   997,
      13,   114,   111,   116,   112,   114,   112,   116,    43,    44,
    1008,  1009,  1010,  1011,  1012,  1013,  1014,  1015,  1016,     3,
     111,    44,    45,   114,    13,   116,   114,   112,  1026,  1027,
    1028,  1029,   112,   114,   127,   127,  1034,  1035,  1036,  1037,
     127,    13,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,   111,   114,    47,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,     1,   801,   802,
     803,   804,   805,   806,   111,   111,   114,   114,   114,   116,
     116,   112,   111,   180,   112,   114,     3,   116,   112,   111,
     773,   114,   752,   907,   827,    12,    13,    14,   773,   327,
     484,   799,   518,   352,    41,   694,   949,    24,   496,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,   655,    46,
      47,    48,    49,    50,   867,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
     215,   519,    -1,    -1,    -1,    72,    73,    -1,    -1,    -1,
      -1,    78,    79,    80,    81,    82,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    -1,    -1,    47,    -1,    -1,
      -1,   924,    -1,   110,    -1,    -1,    -1,    -1,    -1,    -1,
     117,   118,    -1,   120,    -1,    -1,   123,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   949,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     973,   974,   975,   976,   977,   978,    -1,    -1,    -1,    -1,
      -1,   984,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,  1020,  1021,  1022,
       3,    -1,  1025,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    -1,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,   111,    -1,
     113,    -1,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    -1,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   111,    -1,
     113,    -1,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    -1,    85,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   109,   110,   111,
     112,   113,    -1,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    -1,    85,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,   111,
      -1,   113,    -1,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    -1,    85,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   111,
      -1,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    -1,    85,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     3,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   111,
      -1,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    -1,    -1,    47,    -1,    -1,    -1,    -1,
      -1,    12,    13,    14,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    24,    -1,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    -1,    46,    47,    48,    49,    50,
      -1,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    -1,    -1,    -1,    -1,
      -1,    72,    73,    -1,    -1,   117,   118,    78,    79,    80,
      81,    82,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,
     111,    12,    13,    14,    -1,    -1,   117,   118,    -1,   120,
      -1,    -1,   123,    24,    -1,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    -1,    46,    47,    48,    49,    50,
      -1,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    -1,    -1,    -1,    -1,
      -1,    72,    73,    -1,    -1,    -1,    -1,    78,    79,    80,
      81,    82,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   110,
      -1,    -1,    -1,    -1,    -1,    -1,   117,   118,    -1,   120,
      -1,    -1,   123
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   130,   134,   158,   132,   131,   160,
     160,   160,   160,   160,   160,   160,   160,   160,   160,   160,
     160,   160,   160,   160,   160,     0,   160,     3,     4,     5,
     140,   159,   160,   160,     1,     3,    13,    15,    16,    17,
      18,    19,    20,    22,    23,   120,   127,   138,   139,   164,
     172,   173,   175,   176,   190,   191,   193,   195,   203,   212,
     218,   219,   220,   227,   237,   212,   139,   164,   173,   176,
     193,   191,   195,   203,   133,   218,    16,   190,     1,    69,
      70,   178,   179,   182,   184,   185,   182,     1,    13,   208,
     217,    12,    13,    14,    24,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    46,    47,    48,    49,    50,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    72,    73,    78,    79,    80,    81,
      82,   110,   117,   118,   120,   123,   148,   207,   247,   249,
     259,   267,   268,   270,    84,   141,   110,   136,   214,   215,
     214,   110,   127,     1,   143,   160,   143,   160,   160,   143,
     160,   143,   160,   143,   160,   143,   160,   143,   160,   127,
     221,   160,     1,    84,   110,   143,   109,   110,   110,   110,
     111,     3,   117,   119,   122,   161,   206,   228,   229,   228,
     160,   160,   160,   160,   160,   160,   160,   160,   160,     1,
      13,    39,   197,   198,   199,   207,   265,    84,    84,    84,
     109,   110,   111,   160,   160,    84,   109,   110,   180,   124,
     187,    13,    84,   109,   110,   111,    84,   109,   112,   160,
     113,    84,   160,   160,   160,   160,   160,   160,   160,   160,
     160,   160,   160,   160,   160,   160,   160,   160,   160,   160,
     160,   160,   160,   160,   160,   160,   160,   160,   160,   160,
     160,   160,   250,   251,   252,   253,   254,   255,   256,   160,
     160,   160,   160,   160,   160,   160,   160,   160,   160,   160,
     160,   160,   160,   160,   160,   267,    25,   111,   116,   205,
     246,   160,   260,     1,    21,    24,   139,   142,   144,   147,
     164,   173,   176,   191,   193,   195,   203,   212,   137,   112,
     135,   216,   217,   135,   225,   109,   112,    12,    46,   165,
      13,   113,   188,   189,   178,    69,   124,   166,   167,   168,
     169,   170,   171,   110,    12,   110,    13,   223,    13,   120,
     237,    84,    84,   160,   160,   160,   160,   160,   160,   160,
     160,     3,   206,   219,   219,    14,   113,   115,   125,   230,
     231,   232,   239,   242,   230,    84,    84,    84,    84,    84,
      84,    84,    84,    84,   110,   112,   160,    84,    39,    84,
      84,   111,     1,   160,   160,   160,    84,    84,    84,   160,
     249,   269,    44,   207,   264,   265,    43,   207,   264,   266,
     264,   265,   266,   160,   160,   160,   160,   160,   160,   160,
      13,    13,    13,   249,    13,    12,    13,   207,   264,    12,
      13,   240,   262,   257,   266,   264,   264,   149,   160,   160,
     160,   249,    84,   207,   261,   267,   111,   114,     5,   143,
     160,   143,   158,   160,    84,   109,    84,   237,   160,    13,
     160,   189,   160,   160,   160,    67,    68,    68,    67,   160,
     160,   160,   160,   237,   222,   174,   177,   192,   213,   219,
     219,    13,    51,    71,    74,    75,    76,    77,   113,   244,
      13,   160,   110,   111,   160,   114,    13,   186,   183,   247,
     114,   205,    44,   160,   160,    43,   160,   160,   160,   160,
     248,   249,   248,   248,   248,   248,   248,   248,   160,   160,
     160,   111,   249,   160,   160,   160,   160,   160,   111,   114,
     248,   160,   160,   160,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    85,   109,   111,   112,   113,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   148,   150,
     151,   154,   155,   156,   267,   160,   160,    12,    13,    46,
     145,   214,   160,   226,   178,   160,   167,    13,   152,   166,
     160,   160,   160,   160,   194,   109,   196,   110,   224,   228,
     210,   210,   214,   214,   160,   160,    13,    83,   160,    13,
     120,   127,   238,   160,   160,    84,   181,   182,   160,    68,
     186,   246,   160,   249,   111,   111,   111,   111,   111,   111,
     114,   205,   114,   114,   114,   114,   114,   114,   114,   111,
     114,   111,   160,   249,    13,   114,   114,   114,   160,   160,
     114,   111,   114,   111,   258,   160,   160,   160,   160,   160,
     160,   160,   160,   160,   160,   160,   160,   160,   160,   160,
     160,   261,   160,   160,   112,   217,   228,    84,   109,   110,
     162,   160,   114,   114,   167,   167,   167,   167,   214,   214,
     160,   228,   112,   211,   112,   112,   112,    13,   120,   127,
     230,   233,   235,   236,    13,   160,    13,    44,    45,   207,
     245,   127,   127,    13,   160,   200,   197,   113,   114,   160,
       1,   160,   160,   160,   160,   160,   160,   160,   249,   160,
     160,   160,   160,   160,   160,   160,   160,   160,   160,   249,
     249,   160,   160,   160,   160,   240,   160,   160,   160,   160,
     114,   157,   157,   157,   145,    84,   109,   146,   160,   163,
     113,   160,   160,   112,   112,   204,     1,   139,   144,   147,
     164,   173,   176,   191,   193,   195,   203,   209,   212,   234,
     160,   160,   127,   160,   243,   160,   160,   160,    44,    45,
     114,    13,    13,     6,     7,     8,     9,    10,   126,   128,
     241,   214,   160,   187,    84,   109,   112,   264,   265,   265,
     264,   265,   265,    13,    12,   111,   114,   263,   267,   160,
     266,    13,   160,   114,   156,   126,   114,   160,    84,     1,
     160,   201,   112,   160,    13,   120,   236,   114,   114,   236,
     114,   218,   207,   160,   160,   160,   112,   247,   160,   160,
     160,   160,   160,   160,   160,   160,   160,   160,   160,   114,
     267,   160,   160,   160,   146,   112,   151,   153,   112,   202,
     114,   160,   207,   240,   114,   111,   111,   111,   111,   111,
     111,   114,   111,   114,   249,   160,   267,   111,   151,   199,
      44,   160,   160,   160,   160,   160,   160,   160,   160,   160,
     160,   160,   111,   267,   160,   160,   160,    44,   126,   264,
     265,   265,   264,   265,   265,    13,   160,   266,   160,   160,
     160,   160,   160,   160,   160,   160,   249,   160,   114,   114,
     114,   111,   111,   111,   114,   114,   111,   160,   160,   160,
     160,   160,   160,   160,   160,   160,   266,   266,   266,   266,
     160,   160,   160,   160,   114,   114,   114,   114,   160,   160,
     160,   160
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
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
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


/* This macro is provided for backward compatibility. */

#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
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
		  Type, Value); \
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
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  YYUSE(yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
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
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
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
		       		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
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

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = 0;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YYUSE(yytype);
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
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/*----------.
| yyparse.  |
`----------*/

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
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
yylval.integer.integer = 0;

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
  char yymsgbuf[128]; /* ARRAY OK 2009-02-12 rune */
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
  if (yypact_value_is_default (yyn))
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
      if (yytable_value_is_error (yyn))
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

    { parser->StoreBlockLevel(yychar); }
    break;

  case 5:

    { parser->StoreBlockLevel(yychar); }
    break;

  case 7:

    {
  CSS_property_list* prop_list = OP_NEW(CSS_property_list, ());
  if (!prop_list)
    YYOUTOFMEM;
  parser->SetCurrentPropList(prop_list);
  CSS_Parser::DeclStatus ds = parser->AddDeclarationL(parser->GetDOMProperty(), (yyvsp[(4) - (5)].boolean));
  if (ds == CSS_Parser::OUT_OF_MEM)
    YYOUTOFMEM;
#ifdef CSS_ERROR_SUPPORT
  else if (parser->GetDOMProperty() >= 0 && ds == CSS_Parser::INVALID)
    parser->InvalidDeclarationL(ds, parser->GetDOMProperty());
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 17:

    { parser->BeginKeyframes(); }
    break;

  case 20:

    {
  if ((yyvsp[(3) - (4)].boolean))
    parser->EndSelectorL();
  else if (!parser->GetStyleSheet())
    /* Only flag as syntax error for Selectors API parsing.
       (GetStyleSheet() will return null for Selectors API
       parsing, but non-NULL for selectorText).
       We've decided to violate the spec for interoperability
       for now for DOM CSS (See CORE-10244). */
    parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
}
    break;

  case 21:

    {
  uni_char* page_name = NULL;
  ANCHOR_ARRAY(uni_char, page_name);
  if ((yyvsp[(3) - (4)].str).str_len > 0)
    {
      page_name = parser->InputBuffer()->GetString((yyvsp[(3) - (4)].str).start_pos, (yyvsp[(3) - (4)].str).str_len);
      if (!page_name)
	YYOUTOFMEM;
      ANCHOR_ARRAY_RESET(page_name);
    }
  parser->EndSelectorL(page_name);
  ANCHOR_ARRAY_RELEASE(page_name);
}
    break;

  case 31:

    { parser->BeginKeyframes(); }
    break;

  case 32:

    { parser->BeginPage(); }
    break;

  case 33:

    { parser->BeginFontface(); }
    break;

  case 34:

    { parser->BeginViewport(); }
    break;

  case 36:

    { parser->TerminateLastDecl(); }
    break;

  case 38:

    { parser->StoreBlockLevel(yychar); }
    break;

  case 49:

    {
  if (parser->AllowCharset())
    {
      parser->SetAllowLevel(CSS_Parser::ALLOW_IMPORT);
      uni_char* charset = parser->InputBuffer()->GetString((yyvsp[(3) - (5)].str).start_pos, (yyvsp[(3) - (5)].str).str_len);
      if (!charset)
	YYOUTOFMEM;
      CSS_CharsetRule* rule = OP_NEW(CSS_CharsetRule, ());
      if (!rule)
	{
	  OP_DELETEA(charset);
	  YYOUTOFMEM;
	}
      rule->SetCharset(charset);
      parser->AddRuleL(rule);
    }
  else
    {
      parser->SetErrorOccurred(CSSParseStatus::HIERARCHY_ERROR);
#ifdef CSS_ERROR_SUPPORT
      parser->EmitErrorL(UNI_L("@charset must precede other rules"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
    }
}
    break;

  case 50:

    {
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("@charset syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 52:

    { parser->StoreBlockLevel(yychar); }
    break;

  case 57:

    { parser->EndPage(); }
    break;

  case 58:

    { parser->EndFontface(); }
    break;

  case 63:

    { parser->EndViewport(); }
    break;

  case 65:

    {
  parser->DiscardRuleset();
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Syntax error before comment end (\"-->\")"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 66:

    {
  parser->DiscardRuleset();
  parser->RecoverBlockOrSemi(';');
}
    break;

  case 67:

    {
  parser->DiscardRuleset();
  parser->RecoverBlockOrSemi('}');
}
    break;

  case 68:

    {
  if (parser->AllowNamespace())
    {
      unsigned int len = (yyvsp[(3) - (7)].str).str_len + (yyvsp[(5) - (7)].str).str_len;
      if (len < g_memory_manager->GetTempBufLen()-1)
	{
	  uni_char* name = (uni_char*)g_memory_manager->GetTempBuf();
	  uni_char* uri = name + (yyvsp[(3) - (7)].str).str_len + 1;
	  parser->InputBuffer()->GetString(name, (yyvsp[(3) - (7)].str).start_pos, (yyvsp[(3) - (7)].str).str_len);
	  parser->InputBuffer()->GetString(uri, (yyvsp[(5) - (7)].str).start_pos, (yyvsp[(5) - (7)].str).str_len);
	  parser->AddNameSpaceL(name, uri);
	}
    }
  else
    {
      parser->SetErrorOccurred(CSSParseStatus::HIERARCHY_ERROR);
#ifdef CSS_ERROR_SUPPORT
      parser->EmitErrorL(UNI_L("@namespace must precede all style rules"), OpConsoleEngine::Error);
#endif
    }
}
    break;

  case 69:

    {
  if (parser->AllowNamespace())
    {
      uni_char* uri = (uni_char*)g_memory_manager->GetTempBuf();
      if ((yyvsp[(3) - (5)].str).str_len < g_memory_manager->GetTempBufLen())
	{
	  parser->InputBuffer()->GetString(uri, (yyvsp[(3) - (5)].str).start_pos, (yyvsp[(3) - (5)].str).str_len);
	  parser->AddNameSpaceL(NULL, uri);
	  parser->ResetCurrentNameSpaceIdx();
	}
    }
  else
    {
      parser->SetErrorOccurred(CSSParseStatus::HIERARCHY_ERROR);
#ifdef CSS_ERROR_SUPPORT
      parser->EmitErrorL(UNI_L("@namespace must precede all style rules"), OpConsoleEngine::Error);
#endif
    }
}
    break;

  case 70:

    {
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("@namespace syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 71:

    { (yyval.str) = (yyvsp[(1) - (1)].str); }
    break;

  case 72:

    { (yyval.str) = (yyvsp[(1) - (1)].str); }
    break;

  case 74:

    { parser->UnGetEOF(); }
    break;

  case 75:

    {
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Unknown at-rule"), OpConsoleEngine::Information);
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 129:

    {}
    break;

  case 130:

    {}
    break;

  case 131:

    {}
    break;

  case 132:

    {}
    break;

  case 134:

    {}
    break;

  case 135:

    {}
    break;

  case 136:

    {}
    break;

  case 137:

    {}
    break;

  case 138:

    {}
    break;

  case 139:

    {}
    break;

  case 140:

    {}
    break;

  case 141:

    {}
    break;

  case 142:

    {}
    break;

  case 143:

    {}
    break;

  case 144:

    {}
    break;

  case 145:

    {}
    break;

  case 146:

    {}
    break;

  case 149:

    {}
    break;

  case 150:

    {}
    break;

  case 152:

    {}
    break;

  case 153:

    {}
    break;

  case 154:

    {}
    break;

  case 191:

    { (yyval.boolean) = TRUE; }
    break;

  case 192:

    { parser->UnGetEOF(); (yyval.boolean) = TRUE; }
    break;

  case 193:

    { parser->StoreBlockLevel(yychar); }
    break;

  case 194:

    {
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("@import syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
  parser->DiscardRuleset(); parser->RecoverBlockOrSemi('}');
  (yyval.boolean) = FALSE;
}
    break;

  case 195:

    {
  if (!(yyvsp[(6) - (6)].boolean))
    break;

  if (parser->AllowImport())
    {
      uni_char* string = parser->InputBuffer()->GetString((yyvsp[(3) - (6)].str).start_pos, (yyvsp[(3) - (6)].str).str_len);
      if (!string)
        LEAVE(OpStatus::ERR_NO_MEMORY);
      ANCHOR_ARRAY(uni_char, string);
      parser->AddImportL(string);
    }
  else
    {
      parser->SetErrorOccurred(CSSParseStatus::HIERARCHY_ERROR);
#ifdef CSS_ERROR_SUPPORT
      parser->EmitErrorL(UNI_L("@import must precede other rules"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
    }
}
    break;

  case 196:

    {
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("@import syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 205:

    {
  parser->NegateCurrentCondition();
}
    break;

  case 206:

    {
  CSS_CombinedCondition *cc;

  cc = OP_NEW(CSS_ConditionConjunction,());
  if (!cc) YYOUTOFMEM;

  cc->AddFirst(parser->PopCondition());
  cc->AddFirst(parser->PopCondition());

  parser->QueueCondition(cc);
}
    break;

  case 207:

    {
  CSS_Condition *c;
  CSS_CombinedCondition *cc;
  c = parser->PopCondition();
  cc = static_cast<CSS_CombinedCondition*>(parser->PopCondition());
  cc->AddLast(c);
  parser->QueueCondition(cc);
}
    break;

  case 208:

    {
  CSS_CombinedCondition *cc;
  cc = OP_NEW(CSS_ConditionDisjunction,());
  if (!cc) YYOUTOFMEM;

  cc->AddFirst(parser->PopCondition());
  cc->AddFirst(parser->PopCondition());

  parser->QueueCondition(cc);
}
    break;

  case 209:

    {
  CSS_Condition *c;
  CSS_CombinedCondition *cc;
  c = parser->PopCondition();
  cc = static_cast<CSS_CombinedCondition*>(parser->PopCondition());
  cc->AddLast(c);
  parser->QueueCondition(cc);
}
    break;

  case 210:

    {

  CSS_SimpleCondition *c = OP_NEW(CSS_SimpleCondition,());
  if (!c) YYOUTOFMEM;

  uni_char *decl = parser->InputBuffer()->GetString((yyvsp[(1) - (5)].str).start_pos + 1, (yyvsp[(4) - (5)].str).start_pos - (yyvsp[(1) - (5)].str).start_pos - 1);

  if (!decl || OpStatus::IsMemoryError(c->SetDecl(decl)))
  {
    OP_DELETE(c);
    OP_DELETE(decl);
    YYOUTOFMEM;
  }

  parser->QueueCondition(c);
}
    break;

  case 212:

    {
  if (parser->AllowRuleset())
    parser->AddSupportsRuleL();
  else
    LEAVE(CSSParseStatus::HIERARCHY_ERROR);
}
    break;

  case 213:

    {
  parser->EndConditionalRule();
}
    break;

  case 214:

    {
  parser->FlushConditionQueue();
}
    break;

  case 215:

    {
  parser->FlushConditionQueue();
}
    break;

  case 216:

    {
  parser->UnGetEOF();
  parser->FlushConditionQueue();
}
    break;

  case 217:

    {
  parser->UnGetEOF();
  parser->FlushConditionQueue();
}
    break;

  case 219:

    {
  if (parser->AllowRuleset())
    parser->AddMediaRuleL();
  else
    LEAVE(CSSParseStatus::HIERARCHY_ERROR);
}
    break;

  case 220:

    {
  parser->EndConditionalRule();
}
    break;

  case 223:

    {
  parser->EndMediaQueryListL();
}
    break;

  case 224:

    {
  CSS_MediaObject* new_media_obj = OP_NEW(CSS_MediaObject, ());
  if (new_media_obj)
    parser->SetCurrentMediaObject(new_media_obj);
  else
    YYOUTOFMEM;
  parser->EndMediaQueryL();
}
    break;

  case 225:

    { parser->StoreBlockLevel(yychar); }
    break;

  case 226:

    {
  parser->EndMediaQueryL();
}
    break;

  case 228:

    {
  parser->UnGetEOF();
}
    break;

  case 229:

    {
  CSS_MediaType media_type = parser->InputBuffer()->GetMediaType((yyvsp[(2) - (3)].str).start_pos, (yyvsp[(2) - (3)].str).str_len);
  CSS_MediaQuery* new_query = OP_NEW_L(CSS_MediaQuery, ((yyvsp[(1) - (3)].boolean), media_type));
  parser->SetCurrentMediaQuery(new_query);
}
    break;

  case 230:

    {
  if (!(yyvsp[(5) - (5)].boolean))
    parser->SetCurrentMediaQuery(NULL);
}
    break;

  case 231:

    {
  CSS_MediaQuery* new_query = OP_NEW_L(CSS_MediaQuery, (FALSE, CSS_MEDIA_TYPE_ALL));
  parser->SetCurrentMediaQuery(new_query);
}
    break;

  case 232:

    {
  if (!(yyvsp[(2) - (4)].boolean) || !(yyvsp[(4) - (4)].boolean))
    parser->SetCurrentMediaQuery(NULL);
}
    break;

  case 233:

    {
  parser->RecoverMediaQuery('{');
  parser->SetCurrentMediaQuery(NULL);
}
    break;

  case 234:

    {
  parser->RecoverMediaQuery(',');
  parser->SetCurrentMediaQuery(NULL);
}
    break;

  case 235:

    {
  parser->RecoverMediaQuery(';');
  parser->SetCurrentMediaQuery(NULL);
}
    break;

  case 236:

    {
  parser->UnGetEOF();
  parser->SetCurrentMediaQuery(NULL);
}
    break;

  case 237:

    { (yyval.boolean) = FALSE; }
    break;

  case 238:

    { (yyval.boolean) = FALSE; }
    break;

  case 239:

    { (yyval.boolean) = TRUE; }
    break;

  case 240:

    {
  (yyval.boolean) = (yyvsp[(1) - (5)].boolean) && (yyvsp[(4) - (5)].boolean);
}
    break;

  case 241:

    {
  (yyval.boolean) = TRUE;
}
    break;

  case 242:

    {
  CSS_MediaFeature feature = parser->InputBuffer()->GetMediaFeature((yyvsp[(3) - (5)].str).start_pos, (yyvsp[(3) - (5)].str).str_len);
  (yyval.boolean) = parser->AddMediaFeatureExprL(feature);
}
    break;

  case 243:

    {
  CSS_MediaFeature feature = parser->InputBuffer()->GetMediaFeature((yyvsp[(3) - (8)].str).start_pos, (yyvsp[(3) - (8)].str).str_len);
  (yyval.boolean) = parser->AddMediaFeatureExprL(feature);
  parser->ResetValCount();
}
    break;

  case 244:

    {
  (yyval.boolean) = FALSE;
  parser->ResetValCount();
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Media feature syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 245:

    { (yyval.str) = (yyvsp[(1) - (1)].str); }
    break;

  case 246:

    {
  CSS_Selector* sel = OP_NEW(CSS_Selector, ());
  CSS_SimpleSelector* simple_sel = OP_NEW(CSS_SimpleSelector, (Markup::HTE_ANY));
  CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_PSEUDO_PAGE, parser->InputBuffer()->GetPseudoPage((yyvsp[(2) - (2)].str).start_pos, (yyvsp[(2) - (2)].str).str_len)));

  if (sel && simple_sel && sel_attr)
    {
      simple_sel->AddAttribute(sel_attr);
      sel->AddSimpleSelector(simple_sel);
      parser->SetCurrentSelector(sel);
      parser->AddCurrentSelectorL();
    }
  else
    {
      OP_DELETE(sel);
      OP_DELETE(simple_sel);
      OP_DELETE(sel_attr);
      YYOUTOFMEM;
    }
}
    break;

  case 247:

    { (yyval.str).start_pos = 0; (yyval.str).str_len = 0; }
    break;

  case 248:

    { (yyval.str).start_pos = 0; (yyval.str).str_len = 0; }
    break;

  case 249:

    { (yyval.str) = (yyvsp[(3) - (4)].str); }
    break;

  case 250:

    { (yyval.str) = (yyvsp[(3) - (5)].str); }
    break;

  case 251:

    { parser->StoreBlockLevel(yychar); parser->BeginPage(); }
    break;

  case 252:

    {
  uni_char* page_name = NULL;
  ANCHOR_ARRAY(uni_char, page_name);
  if ((yyvsp[(1) - (6)].str).str_len > 0)
    {
      page_name = parser->InputBuffer()->GetString((yyvsp[(1) - (6)].str).start_pos, (yyvsp[(1) - (6)].str).str_len);
      if (!page_name)
	YYOUTOFMEM;
      ANCHOR_ARRAY_RESET(page_name);
    }
  if (parser->AllowRuleset())
    {
      parser->EndPageRulesetL(page_name);
      ANCHOR_ARRAY_RELEASE(page_name);
    }
  else
    {
      parser->DiscardRuleset();
      LEAVE(CSSParseStatus::HIERARCHY_ERROR);
    }
}
    break;

  case 253:

    {
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("@page syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 254:

    { parser->StoreBlockLevel(yychar); parser->BeginFontface(); }
    break;

  case 255:

    {
  if (parser->AllowRuleset())
    parser->AddFontfaceRuleL();
  else
    {
      parser->DiscardRuleset();
      LEAVE(CSSParseStatus::HIERARCHY_ERROR);
    }
}
    break;

  case 256:

    {
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("@font-face syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 257:

    { parser->StoreBlockLevel(yychar); parser->BeginViewport(); }
    break;

  case 258:

    {
  parser->AddViewportRuleL();
}
    break;

  case 259:

    {
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("@-o-viewport syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 260:

    {
#ifdef CSS_ANIMATIONS
  (yyval.number).number = parser->InputBuffer()->GetKeyframePosition((yyvsp[(1) - (1)].str).start_pos, (yyvsp[(1) - (1)].str).str_len);
  (yyval.number).start_pos = (yyvsp[(1) - (1)].str).start_pos;
  (yyval.number).str_len = (yyvsp[(1) - (1)].str).str_len;

  if ((yyval.number).number < 0)
    {
      parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
#ifdef CSS_ERROR_SUPPORT
      parser->EmitErrorL(UNI_L("Unknown keyframe selector"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
    }
#endif // CSS_ANIMATIONS
}
    break;

  case 261:

    {
  (yyval.number) = (yyvsp[(1) - (1)].number);
  (yyval.number).number /= 100.0;
}
    break;

  case 262:

    {
#ifdef CSS_ANIMATIONS
  (yyval.boolean) = (yyvsp[(1) - (5)].boolean) && (yyvsp[(5) - (5)].number).number >= 0;
  if ((yyval.boolean))
    {
      CSS_KeyframeSelector* keyframe_selector = OP_NEW(CSS_KeyframeSelector, ((yyvsp[(5) - (5)].number).number));
      if (!keyframe_selector)
	YYOUTOFMEM;
      parser->AppendKeyframeSelector(keyframe_selector);
    }
#endif // CSS_ANIMATIONS
}
    break;

  case 263:

    {
#ifdef CSS_ANIMATIONS
  (yyval.boolean) = (yyvsp[(1) - (1)].number).number >= 0;
  if ((yyval.boolean))
    {
      CSS_KeyframeSelector* keyframe_selector = OP_NEW(CSS_KeyframeSelector, ((yyvsp[(1) - (1)].number).number));
      if (!keyframe_selector)
	YYOUTOFMEM;
      parser->AppendKeyframeSelector(keyframe_selector);
    }
#endif // CSS_ANIMATIONS
}
    break;

  case 264:

    { parser->StoreBlockLevel(yychar); }
    break;

  case 265:

    {
#ifdef CSS_ANIMATIONS
  if ((yyvsp[(1) - (7)].boolean))
    parser->AddKeyframeRuleL();
  else
    parser->DiscardRuleset();
#endif // CSS_ANIMATIONS
}
    break;

  case 266:

    {
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
  parser->RecoverBlockOrSemi('{');
  parser->DiscardRuleset();

#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Unknown keyframe selector"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 267:

    {
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
  parser->RecoverBlockOrSemi('}');
  parser->DiscardRuleset();

#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Malformed keyframe"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 268:

    { parser->StoreBlockLevel(yychar); }
    break;

  case 271:

    {
#ifdef CSS_ANIMATIONS
  parser->StoreBlockLevel(yychar);

  uni_char* keyframes_name = NULL;
  ANCHOR_ARRAY(uni_char, keyframes_name);
  if ((yyvsp[(3) - (6)].str).str_len > 0)
    {
      keyframes_name = parser->InputBuffer()->GetString((yyvsp[(3) - (6)].str).start_pos, (yyvsp[(3) - (6)].str).str_len);
      if (!keyframes_name)
	YYOUTOFMEM;
      ANCHOR_ARRAY_RESET(keyframes_name);
    }

  parser->BeginKeyframesL(keyframes_name);
  ANCHOR_ARRAY_RELEASE(keyframes_name);
#endif // CSS_ANIMATIONS
}
    break;

  case 272:

    {
  parser->EndKeyframes();
}
    break;

  case 274:

    { (yyval.value).token = CSS_SLASH; }
    break;

  case 275:

    { (yyval.value).token = CSS_COMMA; }
    break;

  case 276:

    { (yyval.value).token = 0; }
    break;

  case 277:

    { (yyval.ushortint)=CSS_COMBINATOR_ADJACENT; }
    break;

  case 278:

    { (yyval.ushortint)=CSS_COMBINATOR_CHILD; }
    break;

  case 279:

    { (yyval.ushortint)=CSS_COMBINATOR_ADJACENT_INDIRECT; }
    break;

  case 280:

    { (yyval.integer).integer=-1; }
    break;

  case 281:

    { (yyval.integer).integer=1; }
    break;

  case 282:

    {
  (yyval.shortint) = parser->InputBuffer()->GetPropertySymbol((yyvsp[(1) - (2)].str).start_pos, (yyvsp[(1) - (2)].str).str_len);
#ifdef CSS_ERROR_SUPPORT
  if ((yyval.shortint) < 0)
    {
      OpString msg;
      ANCHOR(OpString, msg);
      uni_char* cstr = msg.ReserveL((yyvsp[(1) - (2)].str).str_len+2);
      int start_pos = (yyvsp[(1) - (2)].str).start_pos;
      int str_len = (yyvsp[(1) - (2)].str).str_len;
      parser->InputBuffer()->GetString(cstr, start_pos, str_len);
      msg.AppendL(" is an unknown property");
      parser->EmitErrorL(msg.CStr(), OpConsoleEngine::Information);
    }
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 285:

    { parser->EndPage(); }
    break;

  case 286:

    { parser->EndFontface(); }
    break;

  case 292:

    { parser->EndViewport(); }
    break;

  case 294:

    {
  parser->DiscardRuleset();
  parser->RecoverBlockOrSemi('}');
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Syntax error at end of conditional rule"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 295:

    { parser->StoreBlockLevel(yychar); }
    break;

  case 296:

    { parser->StoreBlockLevel(yychar); }
    break;

  case 298:

    { parser->StoreBlockLevel(yychar); }
    break;

  case 299:

    {
  if ((yyvsp[(1) - (6)].boolean) && parser->AllowRuleset())
    {
      parser->EndRulesetL();
    }
  else
    {
      parser->DiscardRuleset();
      if (!parser->AllowRuleset())
	LEAVE(CSSParseStatus::HIERARCHY_ERROR);
    }
}
    break;

  case 300:

    {
  parser->InvalidBlockHit();
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
  parser->RecoverBlockOrSemi('{');
  parser->DiscardRuleset();

#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Selector syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
  parser->ResetCurrentNameSpaceIdx();
}
    break;

  case 301:

    {
  CSS_property_list* new_list = OP_NEW(CSS_property_list, ());
  if (!new_list)
    YYOUTOFMEM;
  parser->SetCurrentPropList(new_list);
}
    break;

  case 303:

    { parser->StoreBlockLevel(yychar); }
    break;

  case 305:

    {
  if ((yyvsp[(1) - (5)].shortint) >= 0)
    {
      CSS_Parser::DeclStatus ds = parser->AddDeclarationL((yyvsp[(1) - (5)].shortint), (yyvsp[(5) - (5)].boolean));
      if (ds == CSS_Parser::OUT_OF_MEM)
	YYOUTOFMEM;
#ifdef CSS_ERROR_SUPPORT
      else if (ds == CSS_Parser::INVALID)
	parser->InvalidDeclarationL(ds, (yyvsp[(1) - (5)].shortint));
#endif // CSS_ERROR_SUPPORT
    }
  else
    parser->EmptyDecl();

  parser->ResetValCount();
}
    break;

  case 306:

    {
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Declaration syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
  parser->EmptyDecl();
  parser->ResetValCount();
  parser->RecoverDeclaration(';');
}
    break;

  case 307:

    {
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Declaration syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
  parser->EmptyDecl();
  parser->ResetValCount();
  parser->RecoverDeclaration('}');
}
    break;

  case 308:

    {
  parser->UnGetEOF();
  parser->EmptyDecl();
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Declaration syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 309:

    { parser->EmptyDecl(); }
    break;

  case 310:

    {
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Declaration syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
  parser->EmptyDecl();
  parser->ResetValCount();
  parser->RecoverDeclaration(';');
}
    break;

  case 311:

    {
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Declaration syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
  parser->EmptyDecl();
  parser->ResetValCount();
  parser->RecoverDeclaration('}');
}
    break;

  case 312:

    {
  parser->UnGetEOF();
  parser->EmptyDecl();
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Declaration syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
    break;

  case 313:

    {
  // Ensure we don't have any selectors left from a previous error token.
  OP_ASSERT(!parser->HasSelectors());
  (yyval.boolean) = (yyvsp[(1) - (1)].boolean);
  if ((yyval.boolean))
    parser->AddCurrentSelectorL();
}
    break;

  case 314:

    {
  (yyval.boolean) = (yyvsp[(1) - (4)].boolean) && (yyvsp[(4) - (4)].boolean);
  if ((yyval.boolean))
    parser->AddCurrentSelectorL();
}
    break;

  case 315:

    {
  (yyval.boolean) = (yyvsp[(1) - (1)].boolean);
  if ((yyvsp[(1) - (1)].boolean))
    {
      CSS_Selector* new_sel = OP_NEW(CSS_Selector, ());
      if (new_sel)
	{
	  parser->SetCurrentSelector(new_sel);
	  (yyval.boolean) = parser->AddCurrentSimpleSelector();
	}
      else
	YYOUTOFMEM;
    }
  else
    parser->DeleteCurrentSimpleSelector();
}
    break;

  case 316:

    {
  (yyval.boolean) = (yyvsp[(1) - (2)].boolean);
  if ((yyvsp[(1) - (2)].boolean))
    {
      CSS_Selector* new_sel = OP_NEW(CSS_Selector, ());
      if (new_sel)
	{
	  parser->SetCurrentSelector(new_sel);
	  (yyval.boolean) = parser->AddCurrentSimpleSelector();
	}
      else
	YYOUTOFMEM;
    }
  else
    parser->DeleteCurrentSimpleSelector();
}
    break;

  case 317:

    {
  (yyval.boolean) = (yyvsp[(1) - (3)].boolean) && (yyvsp[(3) - (3)].boolean);
  if ((yyval.boolean))
    (yyval.boolean) = parser->AddCurrentSimpleSelector((yyvsp[(2) - (3)].ushortint));
  else
    {
      parser->DeleteCurrentSimpleSelector();
      parser->DeleteCurrentSelector();
    }
}
    break;

  case 318:

    {
  (yyval.boolean) = (yyvsp[(1) - (4)].boolean) && (yyvsp[(4) - (4)].boolean);
  if ((yyval.boolean))
    (yyval.boolean) = parser->AddCurrentSimpleSelector((yyvsp[(3) - (4)].ushortint));
  else
    {
      parser->DeleteCurrentSimpleSelector();
      parser->DeleteCurrentSelector();
    }
}
    break;

  case 319:

    {
  (yyval.boolean) = (yyvsp[(1) - (3)].boolean) && (yyvsp[(3) - (3)].boolean);
  if ((yyval.boolean))
    (yyval.boolean) = parser->AddCurrentSimpleSelector(CSS_COMBINATOR_DESCENDANT);
  else
    {
      parser->DeleteCurrentSimpleSelector();
      parser->DeleteCurrentSelector();
    }
}
    break;

  case 320:

    {
  (yyval.boolean)=(yyvsp[(2) - (2)].boolean);
}
    break;

  case 321:

    { parser->SetCurrentNameSpaceIdx(NS_IDX_DEFAULT); }
    break;

  case 322:

    { parser->ResetCurrentNameSpaceIdx(); }
    break;

  case 323:

    {
  (yyval.boolean)=(yyvsp[(5) - (5)].boolean);
}
    break;

  case 324:

    { parser->SetCurrentNameSpaceIdx(NS_IDX_ANY_NAMESPACE); }
    break;

  case 325:

    { parser->ResetCurrentNameSpaceIdx(); }
    break;

  case 326:

    {
  (yyval.boolean)=(yyvsp[(6) - (6)].boolean);
}
    break;

  case 327:

    {
  uni_char* name = (uni_char*)g_memory_manager->GetTempBuf();
  int len = MIN((yyvsp[(1) - (2)].str).str_len, g_memory_manager->GetTempBufLen()-1);
  if (parser->InputBuffer()->GetString(name, (yyvsp[(1) - (2)].str).start_pos, len))
    {
      int ns_idx = parser->GetNameSpaceIdx(name);
      if (ns_idx == NS_IDX_NOT_FOUND)
	{
#ifdef CSS_ERROR_SUPPORT
	  parser->EmitErrorL(UNI_L("Use of undeclared namespace"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
	  parser->SetErrorOccurred(CSSParseStatus::NAMESPACE_ERROR);
	}
      parser->SetCurrentNameSpaceIdx(ns_idx);
    }
}
    break;

  case 328:

    { parser->ResetCurrentNameSpaceIdx(); }
    break;

  case 329:

    {
  (yyval.boolean)=((yyvsp[(6) - (6)].boolean) && parser->GetCurrentSimpleSelector()->GetNameSpaceIdx() != NS_IDX_NOT_FOUND);
}
    break;

  case 330:

    {
  CSS_SimpleSelector* new_sel = OP_NEW(CSS_SimpleSelector, (Markup::HTE_ANY));
  if (new_sel)
    {
      new_sel->SetNameSpaceIdx(parser->GetCurrentNameSpaceIdx());
      parser->SetCurrentSimpleSelector(new_sel);
    }
  else
    YYOUTOFMEM;
}
    break;

  case 331:

    { (yyval.boolean)=(yyvsp[(2) - (2)].boolean); }
    break;

  case 332:

    { (yyval.boolean) = TRUE; }
    break;

  case 333:

    {
  (yyval.boolean) = ((yyvsp[(1) - (2)].boolean) && (yyvsp[(2) - (2)].boolean));
}
    break;

  case 334:

    {
  (yyval.boolean) = ((yyvsp[(1) - (2)].boolean) && (yyvsp[(2) - (2)].boolean));
}
    break;

  case 335:

    {
  uni_char* text = parser->InputBuffer()->GetString((yyvsp[(1) - (1)].str).start_pos, (yyvsp[(1) - (1)].str).str_len, FALSE);
  if (text)
    {
      if (text[0] == '-' && ((yyvsp[(1) - (1)].str).str_len == 1 || text[1] == '-' || uni_isdigit(text[1])) || parser->StrictMode() && uni_isdigit(text[0]))
	{
	  OP_DELETEA(text);
	  (yyval.boolean) = FALSE;
#ifdef CSS_ERROR_SUPPORT
	  parser->EmitErrorL(UNI_L("Invalid id selector"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
	  break;
	}

      CSS_Buffer::CopyAndRemoveEscapes(text, text, uni_strlen(text));
      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_ID, 0, text));
      if (sel_attr)
	(yyval.boolean) = parser->AddSelectorAttributeL(sel_attr);
      else
	{
	  OP_DELETEA(text);
	  YYOUTOFMEM;
	}
    }
  else
    YYOUTOFMEM;
}
    break;

  case 336:

    { (yyval.boolean)=(yyvsp[(1) - (1)].boolean); }
    break;

  case 337:

    { (yyval.boolean)=(yyvsp[(1) - (1)].boolean); }
    break;

  case 338:

    { (yyval.boolean)=(yyvsp[(1) - (1)].boolean); }
    break;

  case 339:

    { (yyval.boolean)=(yyvsp[(1) - (1)].boolean); }
    break;

  case 340:

    {
  (yyval.boolean) = (yyvsp[(4) - (6)].boolean);
  if ((yyvsp[(4) - (6)].boolean))
    {
      CSS_SelectorAttribute* sel_attr = parser->GetCurrentSimpleSelector()->GetLastAttr();
      BOOL negated = sel_attr->IsNegated();
      if (!negated && (sel_attr->GetType() != CSS_SEL_ATTR_TYPE_PSEUDO_CLASS || !IsPseudoElement(sel_attr->GetPseudoClass())))
	{
	  sel_attr->SetNegated();
	}
      else
	{
	  (yyval.boolean) = FALSE;
#ifdef CSS_ERROR_SUPPORT
	  if (negated)
	    parser->EmitErrorL(UNI_L("Recursive :not"), OpConsoleEngine::Error);
	  else
	    parser->EmitErrorL(UNI_L("Pseudo elements not allowed within a :not"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
	}
    }
}
    break;

  case 341:

    {
  (yyval.boolean) = (yyvsp[(4) - (6)].boolean);
  if ((yyvsp[(4) - (6)].boolean))
    parser->GetCurrentSimpleSelector()->GetLastAttr()->SetNegated();
}
    break;

  case 342:

    {
  (yyval.boolean) = parser->AddClassSelectorL((yyvsp[(2) - (2)].str).start_pos, (yyvsp[(2) - (2)].str).str_len);
}
    break;

  case 343:

    {
  parser->ResetCurrentNameSpaceIdx();
  (yyval.boolean) = (yyvsp[(1) - (3)].boolean) && (yyvsp[(3) - (3)].boolean);
}
    break;

  case 344:

    { parser->SetCurrentNameSpaceIdx(NS_IDX_DEFAULT); }
    break;

  case 345:

    {
  parser->ResetCurrentNameSpaceIdx();
  (yyval.boolean) = (yyvsp[(3) - (3)].boolean);
}
    break;

  case 346:

    {
  (yyval.boolean) = (yyvsp[(1) - (1)].boolean);
}
    break;

  case 347:

    {
  (yyval.boolean) = FALSE;
  uni_char* name = (uni_char*)g_memory_manager->GetTempBuf();
  int len = MIN((yyvsp[(1) - (1)].str).str_len, g_memory_manager->GetTempBufLen()-1);
  if (parser->InputBuffer()->GetString(name, (yyvsp[(1) - (1)].str).start_pos, len))
    {
      int ns_idx = parser->GetNameSpaceIdx(name);
      if (ns_idx == NS_IDX_NOT_FOUND)
	{
#ifdef CSS_ERROR_SUPPORT
	  parser->EmitErrorL(UNI_L("Use of undeclared namespace"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
	  parser->SetErrorOccurred(CSSParseStatus::NAMESPACE_ERROR);
	}
      else
	(yyval.boolean) = TRUE;

      parser->SetCurrentNameSpaceIdx(ns_idx);
    }
}
    break;

  case 348:

    {
  parser->SetCurrentNameSpaceIdx(NS_IDX_ANY_NAMESPACE);
  (yyval.boolean) = TRUE;
}
    break;

  case 349:

    {
  int ns_idx = parser->GetCurrentNameSpaceIdx();
  int tag = Markup::HTE_UNKNOWN;

  if (ns_idx != NS_IDX_NOT_FOUND)
    {
      BOOL is_xml = parser->IsXml();

      if (ns_idx <= NS_IDX_ANY_NAMESPACE)
	    tag = parser->InputBuffer()->GetTag((yyvsp[(1) - (1)].str).start_pos, (yyvsp[(1) - (1)].str).str_len, NS_HTML, is_xml, is_xml);
      else
	    tag = parser->InputBuffer()->GetTag((yyvsp[(1) - (1)].str).start_pos, (yyvsp[(1) - (1)].str).str_len, g_ns_manager->GetNsTypeAt(ns_idx), is_xml, FALSE);
    }

  uni_char* text = NULL;
  if (tag == Markup::HTE_UNKNOWN)
    {
      text = parser->InputBuffer()->GetString((yyvsp[(1) - (1)].str).start_pos, (yyvsp[(1) - (1)].str).str_len);
      if (!text)
	YYOUTOFMEM;
    }

  CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_ELM, tag, text, ns_idx));
  if (sel_attr)
    (yyval.boolean) = parser->AddSelectorAttributeL(sel_attr);
  else
    {
      OP_DELETEA(text);
      YYOUTOFMEM;
    }
}
    break;

  case 350:

    {
  CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_ELM, Markup::HTE_ANY, NULL, parser->GetCurrentNameSpaceIdx()));
  if (sel_attr)
    (yyval.boolean) = parser->AddSelectorAttributeL(sel_attr);
  else
    YYOUTOFMEM;
}
    break;

  case 351:

    {
  CSS_SimpleSelector* new_sel = NULL;

  int ns_idx = parser->GetCurrentNameSpaceIdx();
  int tag = Markup::HTE_UNKNOWN;

  if (ns_idx == NS_IDX_NOT_FOUND)
    tag = Markup::HTE_UNKNOWN;
  else
  {
	BOOL is_xml = parser->IsXml();

	if (ns_idx <= NS_IDX_ANY_NAMESPACE)
	  tag = parser->InputBuffer()->GetTag((yyvsp[(1) - (1)].str).start_pos, (yyvsp[(1) - (1)].str).str_len, NS_HTML, is_xml, is_xml);
	else
	  tag = parser->InputBuffer()->GetTag((yyvsp[(1) - (1)].str).start_pos, (yyvsp[(1) - (1)].str).str_len, g_ns_manager->GetNsTypeAt(ns_idx), is_xml, FALSE);
  }

  if (tag != Markup::HTE_UNKNOWN)
	{
	  new_sel = OP_NEW(CSS_SimpleSelector, (tag));
	  if (!new_sel)
	    YYOUTOFMEM;
	  new_sel->SetNameSpaceIdx(ns_idx);
	  parser->SetCurrentSimpleSelector(new_sel);
	  break;
	}

  new_sel = OP_NEW(CSS_SimpleSelector, (Markup::HTE_UNKNOWN));
  if (!new_sel)
    YYOUTOFMEM;

  uni_char* text = parser->InputBuffer()->GetString((yyvsp[(1) - (1)].str).start_pos, (yyvsp[(1) - (1)].str).str_len);
  if (text)
    new_sel->SetElmName(text);
  else
    {
      OP_DELETE(new_sel);
      YYOUTOFMEM;
    }

  new_sel->SetNameSpaceIdx(ns_idx);
  parser->SetCurrentSimpleSelector(new_sel);
}
    break;

  case 352:

    {
  CSS_SimpleSelector* new_sel = OP_NEW(CSS_SimpleSelector, (Markup::HTE_ANY));
  if (!new_sel)
    YYOUTOFMEM;
  int ns_idx = parser->GetCurrentNameSpaceIdx();

  new_sel->SetNameSpaceIdx(ns_idx);
  parser->SetCurrentSimpleSelector(new_sel);
}
    break;

  case 353:

    {
  (yyval.id_with_ns).start_pos = (yyvsp[(3) - (3)].str).start_pos;
  (yyval.id_with_ns).str_len = (yyvsp[(3) - (3)].str).str_len;
  uni_char* name = (uni_char*)g_memory_manager->GetTempBuf();
  int len = MIN((yyvsp[(1) - (3)].str).str_len, g_memory_manager->GetTempBufLen()-1);
  if (parser->InputBuffer()->GetString(name, (yyvsp[(1) - (3)].str).start_pos, len))
    (yyval.id_with_ns).ns_idx = parser->GetNameSpaceIdx(name);
  else
    (yyval.id_with_ns).ns_idx = NS_IDX_NOT_FOUND;
}
    break;

  case 354:

    {
  (yyval.id_with_ns).start_pos = (yyvsp[(3) - (3)].str).start_pos;
  (yyval.id_with_ns).str_len = (yyvsp[(3) - (3)].str).str_len;
  (yyval.id_with_ns).ns_idx = NS_IDX_ANY_NAMESPACE;
}
    break;

  case 355:

    {
  (yyval.id_with_ns).start_pos = (yyvsp[(2) - (2)].str).start_pos;
  (yyval.id_with_ns).str_len = (yyvsp[(2) - (2)].str).str_len;
  (yyval.id_with_ns).ns_idx = NS_IDX_DEFAULT;
}
    break;

  case 356:

    {
  (yyval.id_with_ns).start_pos = (yyvsp[(1) - (1)].str).start_pos;
  (yyval.id_with_ns).str_len = (yyvsp[(1) - (1)].str).str_len;
  (yyval.id_with_ns).ns_idx = NS_IDX_DEFAULT;
}
    break;

  case 357:

    {
  if ((yyvsp[(3) - (5)].id_with_ns).ns_idx == NS_IDX_NOT_FOUND)
    {
#ifdef CSS_ERROR_SUPPORT
      parser->EmitErrorL(UNI_L("Use of undeclared namespace"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
      parser->SetErrorOccurred(CSSParseStatus::NAMESPACE_ERROR);
      (yyval.boolean) = FALSE;
      break;
    }
  if (parser->IsXml())
    {
      uni_char* name_and_val = parser->InputBuffer()->GetString((yyvsp[(3) - (5)].id_with_ns).start_pos, (yyvsp[(3) - (5)].id_with_ns).str_len);
      if (name_and_val)
	{
	  CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_HTMLATTR_DEFINED, name_and_val, (yyvsp[(3) - (5)].id_with_ns).ns_idx));
	  if (sel_attr)
	    (yyval.boolean) = parser->AddSelectorAttributeL(sel_attr);
	  else
	    {
	      OP_DELETEA(name_and_val);
	      YYOUTOFMEM;
	    }
	}
      else
	YYOUTOFMEM;
    }
  else
    {
      short attr_type = (short)parser->InputBuffer()->GetHtmlAttr((yyvsp[(3) - (5)].id_with_ns).start_pos, (yyvsp[(3) - (5)].id_with_ns).str_len);
      if (attr_type != Markup::HA_XML)
	{
	  CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_HTMLATTR_DEFINED, attr_type, (uni_char*)0, (yyvsp[(3) - (5)].id_with_ns).ns_idx));
	  if (sel_attr)
	    (yyval.boolean) = parser->AddSelectorAttributeL(sel_attr);
	  else
	    YYOUTOFMEM;
	}
      else
	{
	  uni_char* name_and_val = parser->InputBuffer()->GetString((yyvsp[(3) - (5)].id_with_ns).start_pos, (yyvsp[(3) - (5)].id_with_ns).str_len);
	  if (name_and_val)
	    {
	      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_HTMLATTR_DEFINED, name_and_val, (yyvsp[(3) - (5)].id_with_ns).ns_idx));
	      if (sel_attr)
		(yyval.boolean) = parser->AddSelectorAttributeL(sel_attr);
	      else
		{
		  OP_DELETEA(name_and_val);
		  YYOUTOFMEM;
		}
	    }
	  else
	    YYOUTOFMEM;
	}
    }
}
    break;

  case 358:

    {
  if ((yyvsp[(3) - (9)].id_with_ns).ns_idx == NS_IDX_NOT_FOUND)
    {
#ifdef CSS_ERROR_SUPPORT
      parser->EmitErrorL(UNI_L("Use of undeclared namespace"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
      parser->SetErrorOccurred(CSSParseStatus::NAMESPACE_ERROR);
      (yyval.boolean) = FALSE;
      break;
    }
  if (parser->IsXml())
    {
      uni_char* name_and_val = parser->InputBuffer()->GetNameAndVal((yyvsp[(3) - (9)].id_with_ns).start_pos,
								    (yyvsp[(3) - (9)].id_with_ns).str_len,
								    (yyvsp[(7) - (9)].str).start_pos,
								    (yyvsp[(7) - (9)].str).str_len);
      if (name_and_val)
	{
	  CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, ((yyvsp[(5) - (9)].ushortint), name_and_val, (yyvsp[(3) - (9)].id_with_ns).ns_idx));
	  if (sel_attr)
	    (yyval.boolean) = parser->AddSelectorAttributeL(sel_attr);
	  else
	    {
	      OP_DELETEA(name_and_val);
	      YYOUTOFMEM;
	    }
	}
      else
	YYOUTOFMEM;
    }
  else
    {
      short attr_type = (short)parser->InputBuffer()->GetHtmlAttr((yyvsp[(3) - (9)].id_with_ns).start_pos, (yyvsp[(3) - (9)].id_with_ns).str_len);
      if (attr_type != Markup::HA_XML)
	{
	  uni_char* text = parser->InputBuffer()->GetString((yyvsp[(7) - (9)].str).start_pos, (yyvsp[(7) - (9)].str).str_len);
	  if (text)
	    {
	      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, ((yyvsp[(5) - (9)].ushortint), attr_type, text, (yyvsp[(3) - (9)].id_with_ns).ns_idx));
	      if (sel_attr)
		(yyval.boolean) = parser->AddSelectorAttributeL(sel_attr);
	      else
		{
		  OP_DELETEA(text);
		  YYOUTOFMEM;
		}
	    }
	  else
	    YYOUTOFMEM;
	}
      else
	{
	  uni_char* name_and_val = parser->InputBuffer()->GetNameAndVal((yyvsp[(3) - (9)].id_with_ns).start_pos,
									(yyvsp[(3) - (9)].id_with_ns).str_len,
									(yyvsp[(7) - (9)].str).start_pos,
									(yyvsp[(7) - (9)].str).str_len);
	  if (name_and_val)
	    {
	      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, ((yyvsp[(5) - (9)].ushortint), name_and_val, (yyvsp[(3) - (9)].id_with_ns).ns_idx));
	      if (sel_attr)
		(yyval.boolean) = parser->AddSelectorAttributeL(sel_attr);
	      else
		{
		  OP_DELETEA(name_and_val);
		  YYOUTOFMEM;
		}
	    }
	  else
	    YYOUTOFMEM;
	}
    }
}
    break;

  case 359:

    { (yyval.str)=(yyvsp[(1) - (1)].str); }
    break;

  case 360:

    { (yyval.str)=(yyvsp[(1) - (1)].str); }
    break;

  case 361:

    { (yyval.ushortint) = CSS_SEL_ATTR_TYPE_HTMLATTR_EQUAL; }
    break;

  case 362:

    { (yyval.ushortint) = CSS_SEL_ATTR_TYPE_HTMLATTR_INCLUDES; }
    break;

  case 363:

    { (yyval.ushortint) = CSS_SEL_ATTR_TYPE_HTMLATTR_DASHMATCH; }
    break;

  case 364:

    { (yyval.ushortint) = CSS_SEL_ATTR_TYPE_HTMLATTR_PREFIXMATCH; }
    break;

  case 365:

    { (yyval.ushortint) = CSS_SEL_ATTR_TYPE_HTMLATTR_SUFFIXMATCH; }
    break;

  case 366:

    { (yyval.ushortint) = CSS_SEL_ATTR_TYPE_HTMLATTR_SUBSTRMATCH; }
    break;

  case 367:

    {
  unsigned short pseudo_class = parser->InputBuffer()->GetPseudoSymbol((yyvsp[(2) - (2)].str).start_pos, (yyvsp[(2) - (2)].str).str_len);
  if (pseudo_class != PSEUDO_CLASS_UNKNOWN &&
      pseudo_class != PSEUDO_CLASS_SELECTION &&
      pseudo_class != PSEUDO_CLASS_CUE)
    {
      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_PSEUDO_CLASS, pseudo_class));
      if (sel_attr)
	(yyval.boolean) = parser->AddSelectorAttributeL(sel_attr);
      else
	YYOUTOFMEM;
    }
  else
    {
#ifdef CSS_ERROR_SUPPORT
      parser->EmitErrorL(UNI_L("Unknown pseudo class"), OpConsoleEngine::Information);
#endif // CSS_ERROR_SUPPORT
      (yyval.boolean) = FALSE;
    }
}
    break;

  case 368:

    {
  unsigned short pseudo_class = parser->InputBuffer()->GetPseudoSymbol((yyvsp[(3) - (3)].str).start_pos, (yyvsp[(3) - (3)].str).str_len);
  if (IsPseudoElement(pseudo_class))
    {
      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_PSEUDO_CLASS, pseudo_class));
      if (sel_attr)
	(yyval.boolean) = parser->AddSelectorAttributeL(sel_attr);
      else
	YYOUTOFMEM;
    }
  else
    {
#ifdef CSS_ERROR_SUPPORT
      parser->EmitErrorL(UNI_L("\"::\" not followed by known pseudo element"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
      (yyval.boolean) = FALSE;
    }
}
    break;

  case 369:

    {
#ifdef MEDIA_HTML_SUPPORT
  parser->BeginCueSelector();
#endif // MEDIA_HTML_SUPPORT
}
    break;

  case 370:

    {
#ifdef MEDIA_HTML_SUPPORT
  parser->EndCueSelector();

  CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_PSEUDO_CLASS, PSEUDO_CLASS_CUE));
  if (sel_attr)
    (yyval.boolean) = parser->AddSelectorAttributeL(sel_attr);
  else
    YYOUTOFMEM;

  (yyval.boolean) = ((yyval.boolean) && (yyvsp[(6) - (7)].boolean));
#else
  (yyval.boolean) = FALSE;
#endif // MEDIA_HTML_SUPPORT
}
    break;

  case 371:

    {
  uni_char* text = parser->InputBuffer()->GetString((yyvsp[(4) - (6)].str).start_pos, (yyvsp[(4) - (6)].str).str_len);
  if (text)
    {
      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_PSEUDO_CLASS, PSEUDO_CLASS_LANG, text));
      if (sel_attr)
	(yyval.boolean) = parser->AddSelectorAttributeL(sel_attr);
      else
	YYOUTOFMEM;
    }
  else
    YYOUTOFMEM;
}
    break;

  case 372:

    {
  if ((yyvsp[(4) - (5)].linear_func).a == INT_MIN && (yyvsp[(4) - (5)].linear_func).b == INT_MIN)
    (yyval.boolean) = FALSE;
  else
    {
      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, ((yyvsp[(2) - (5)].ushortint), (yyvsp[(4) - (5)].linear_func).a, (yyvsp[(4) - (5)].linear_func).b));
      if (sel_attr)
	(yyval.boolean) = parser->AddSelectorAttributeL(sel_attr);
      else
	YYOUTOFMEM;
    }
}
    break;

  case 373:

    { (yyval.ushortint) = CSS_SEL_ATTR_TYPE_NTH_CHILD; }
    break;

  case 374:

    { (yyval.ushortint) = CSS_SEL_ATTR_TYPE_NTH_OF_TYPE; }
    break;

  case 375:

    { (yyval.ushortint) = CSS_SEL_ATTR_TYPE_NTH_LAST_CHILD; }
    break;

  case 376:

    { (yyval.ushortint) = CSS_SEL_ATTR_TYPE_NTH_LAST_OF_TYPE; }
    break;

  case 377:

    {
  (yyval.linear_func).a = (yyvsp[(1) - (6)].uinteger).integer;
  (yyval.linear_func).b = (yyvsp[(3) - (6)].integer).integer * (yyvsp[(5) - (6)].uinteger).integer;
}
    break;

  case 378:

    {
  (yyval.linear_func).a = (yyvsp[(1) - (7)].integer).integer * (yyvsp[(2) - (7)].uinteger).integer;
  (yyval.linear_func).b = (yyvsp[(4) - (7)].integer).integer * (yyvsp[(6) - (7)].uinteger).integer;
}
    break;

  case 379:

    {
  (yyval.linear_func).a = (yyvsp[(1) - (2)].uinteger).integer;
  (yyval.linear_func).b = 0;
}
    break;

  case 380:

    {
  (yyval.linear_func).a = (yyvsp[(1) - (3)].integer).integer * (yyvsp[(2) - (3)].uinteger).integer;
  (yyval.linear_func).b = 0;
}
    break;

  case 381:

    {
  (yyval.linear_func).a = 0;
  (yyval.linear_func).b = (yyvsp[(1) - (2)].uinteger).integer;
}
    break;

  case 382:

    {
  (yyval.linear_func).a = 0;
  (yyval.linear_func).b = (yyvsp[(1) - (3)].integer).integer * (yyvsp[(2) - (3)].uinteger).integer;
}
    break;

  case 383:

    {
  (yyval.linear_func).a = (yyval.linear_func).b = INT_MIN;
  int len = (yyvsp[(1) - (2)].str).str_len;
  if (len == 3 || len == 4)
    {
      uni_char ident[8]; /* ARRAY OK 2009-02-12 rune */
      parser->InputBuffer()->GetString(ident, (yyvsp[(1) - (2)].str).start_pos, (yyvsp[(1) - (2)].str).str_len);
      if (len == 3 && uni_stri_eq(ident, "ODD"))
	{
	  (yyval.linear_func).a = 2;
	  (yyval.linear_func).b = 1;
	}
      else if (len == 4 && uni_stri_eq(ident, "EVEN"))
	{
	  (yyval.linear_func).a = 2;
	  (yyval.linear_func).b = 0;
	}
    }
}
    break;

  case 384:

    { (yyval.boolean) = TRUE; }
    break;

  case 385:

    { (yyval.boolean) = FALSE; }
    break;

  case 386:

    {
  if ((yyvsp[(1) - (1)].value).token == CSS_FUNCTION_END)
    parser->CommitFunctionValuesL();

  if ((yyvsp[(1) - (1)].value).token)
    parser->AddValueL((yyvsp[(1) - (1)].value));
}
    break;

  case 387:

    {
  if ((yyvsp[(2) - (3)].value).token)
    parser->AddValueL((yyvsp[(2) - (3)].value));

  if ((yyvsp[(3) - (3)].value).token == CSS_FUNCTION_END)
    parser->CommitFunctionValuesL();

  if ((yyvsp[(3) - (3)].value).token)
    parser->AddValueL((yyvsp[(3) - (3)].value));
}
    break;

  case 388:

    {
  if ((yyvsp[(1) - (1)].value).token)
    parser->AddFunctionValueL((yyvsp[(1) - (1)].value));
}
    break;

  case 389:

    {
  if ((yyvsp[(2) - (3)].value).token)
    parser->AddFunctionValueL((yyvsp[(2) - (3)].value));
  if ((yyvsp[(3) - (3)].value).token)
    parser->AddFunctionValueL((yyvsp[(3) - (3)].value));
}
    break;

  case 390:

    {
  (yyval.value) = (yyvsp[(2) - (2)].value);
  if ((yyval.value).token == CSS_INT_NUMBER)
    (yyval.value).value.integer.integer *= (yyvsp[(1) - (2)].integer).integer;
  else
    (yyval.value).value.number.number *= (yyvsp[(1) - (2)].integer).integer;
}
    break;

  case 391:

    { (yyval.value)=(yyvsp[(1) - (1)].value); }
    break;

  case 392:

    {
  (yyval.value).token = CSS_STRING_LITERAL;
  (yyval.value).value.str.start_pos = (yyvsp[(1) - (2)].str).start_pos;
  (yyval.value).value.str.str_len = (yyvsp[(1) - (2)].str).str_len;
}
    break;

  case 393:

    {
  (yyval.value).token = CSS_IDENT;
  (yyval.value).value.str.start_pos = (yyvsp[(1) - (2)].str).start_pos;
  (yyval.value).value.str.str_len = (yyvsp[(1) - (2)].str).str_len;
}
    break;

  case 394:

    {
  (yyval.value).token = CSS_FUNCTION_URL;
  (yyval.value).value.str.start_pos = (yyvsp[(1) - (2)].str).start_pos;
  (yyval.value).value.str.str_len = (yyvsp[(1) - (2)].str).str_len;
}
    break;

  case 395:

    {
  (yyval.value).token = -1; /* what do we put here? */
}
    break;

  case 396:

    {
  (yyval.value).token = CSS_HASH;
  (yyval.value).value.str.start_pos = (yyvsp[(1) - (1)].str).start_pos;
  (yyval.value).value.str.str_len = (yyvsp[(1) - (1)].str).str_len;
}
    break;

  case 397:

    {
  (yyval.value).token = CSS_FUNCTION_RGB;

  if ((yyvsp[(3) - (14)].number).number < 0.0)
    (yyvsp[(3) - (14)].number).number = 0.0;
  else if ((yyvsp[(3) - (14)].number).number > 100.0)
    (yyvsp[(3) - (14)].number).number = 100.0;
  if ((yyvsp[(7) - (14)].number).number < 0.0)
    (yyvsp[(7) - (14)].number).number = 0.0;
  else if ((yyvsp[(7) - (14)].number).number > 100.0)
    (yyvsp[(7) - (14)].number).number = 100.0;
  if ((yyvsp[(11) - (14)].number).number < 0.0)
    (yyvsp[(11) - (14)].number).number = 0.0;
  else if ((yyvsp[(11) - (14)].number).number > 100.0)
    (yyvsp[(11) - (14)].number).number = 100.0;

  (yyval.value).value.color = OP_RGB((int)OpRound(((yyvsp[(3) - (14)].number).number*255)/100), (int)OpRound(((yyvsp[(7) - (14)].number).number*255)/100), (int)OpRound(((yyvsp[(11) - (14)].number).number*255)/100));
}
    break;

  case 398:

    {
  (yyval.value).token = CSS_FUNCTION_RGB;

  if ((yyvsp[(3) - (14)].integer).integer < 0)
    (yyvsp[(3) - (14)].integer).integer = 0;
  else if ((yyvsp[(3) - (14)].integer).integer > 255)
    (yyvsp[(3) - (14)].integer).integer = 255;
  if ((yyvsp[(7) - (14)].integer).integer < 0)
    (yyvsp[(7) - (14)].integer).integer = 0;
  else if ((yyvsp[(7) - (14)].integer).integer > 255)
    (yyvsp[(7) - (14)].integer).integer = 255;
  if ((yyvsp[(11) - (14)].integer).integer < 0)
    (yyvsp[(11) - (14)].integer).integer = 0;
  else if ((yyvsp[(11) - (14)].integer).integer > 255)
    (yyvsp[(11) - (14)].integer).integer = 255;

  (yyval.value).value.color = OP_RGB((yyvsp[(3) - (14)].integer).integer, (yyvsp[(7) - (14)].integer).integer, (yyvsp[(11) - (14)].integer).integer);
}
    break;

  case 399:

    {
  (yyval.value).token = CSS_FUNCTION_RGBA;

  if ((yyvsp[(3) - (18)].number).number < 0.0)
    (yyvsp[(3) - (18)].number).number = 0.0;
  else if ((yyvsp[(3) - (18)].number).number > 100.0)
    (yyvsp[(3) - (18)].number).number = 100.0;
  if ((yyvsp[(7) - (18)].number).number < 0.0)
    (yyvsp[(7) - (18)].number).number = 0.0;
  else if ((yyvsp[(7) - (18)].number).number > 100.0)
    (yyvsp[(7) - (18)].number).number = 100.0;
  if ((yyvsp[(11) - (18)].number).number < 0.0)
    (yyvsp[(11) - (18)].number).number = 0.0;
  else if ((yyvsp[(11) - (18)].number).number > 100.0)
    (yyvsp[(11) - (18)].number).number = 100.0;
  if ((yyvsp[(15) - (18)].number).number < 0.0)
    (yyvsp[(15) - (18)].number).number = 0.0;
  else if ((yyvsp[(15) - (18)].number).number > 1.0)
    (yyvsp[(15) - (18)].number).number = 1.0;

  (yyval.value).value.color = OP_RGBA((int)OpRound(((yyvsp[(3) - (18)].number).number*255)/100), (int)OpRound(((yyvsp[(7) - (18)].number).number*255)/100), (int)OpRound(((yyvsp[(11) - (18)].number).number*255)/100), (int)OpRound((yyvsp[(15) - (18)].number).number*255));
}
    break;

  case 400:

    {
  (yyval.value).token = CSS_FUNCTION_RGBA;

  if ((yyvsp[(3) - (18)].integer).integer < 0)
    (yyvsp[(3) - (18)].integer).integer = 0;
  else if ((yyvsp[(3) - (18)].integer).integer > 255)
    (yyvsp[(3) - (18)].integer).integer = 255;
  if ((yyvsp[(7) - (18)].integer).integer < 0)
    (yyvsp[(7) - (18)].integer).integer = 0;
  else if ((yyvsp[(7) - (18)].integer).integer > 255)
    (yyvsp[(7) - (18)].integer).integer = 255;
  if ((yyvsp[(11) - (18)].integer).integer < 0)
    (yyvsp[(11) - (18)].integer).integer = 0;
  else if ((yyvsp[(11) - (18)].integer).integer > 255)
    (yyvsp[(11) - (18)].integer).integer = 255;
  if ((yyvsp[(15) - (18)].number).number < 0.0)
    (yyvsp[(15) - (18)].number).number = 0.0;
  else if ((yyvsp[(15) - (18)].number).number > 1.0)
    (yyvsp[(15) - (18)].number).number = 1.0;

  (yyval.value).value.color = OP_RGBA((yyvsp[(3) - (18)].integer).integer, (yyvsp[(7) - (18)].integer).integer, (yyvsp[(11) - (18)].integer).integer, (int)OpRound((yyvsp[(15) - (18)].number).number*255));
}
    break;

  case 401:

    {
  (yyval.value).token = CSS_FUNCTION_RGB;
  (yyval.value).value.color = HSLA_TO_RGBA((yyvsp[(3) - (14)].number).number, (yyvsp[(7) - (14)].number).number, (yyvsp[(11) - (14)].number).number);
}
    break;

  case 402:

    {
  (yyval.value).token = CSS_FUNCTION_RGBA;
  if ((yyvsp[(15) - (18)].number).number < 0.0)
    (yyvsp[(15) - (18)].number).number = 0.0;
  else if ((yyvsp[(15) - (18)].number).number > 1.0)
    (yyvsp[(15) - (18)].number).number = 1.0;
  (yyval.value).value.color = HSLA_TO_RGBA((yyvsp[(3) - (18)].number).number, (yyvsp[(7) - (18)].number).number, (yyvsp[(11) - (18)].number).number, (yyvsp[(15) - (18)].number).number);
}
    break;

  case 403:

    { PropertyValue val; val.token = CSS_FUNCTION_LINEAR_GRADIENT; val.value.str.str_len = 0; parser->AddFunctionValueL(val); }
    break;

  case 404:

    {
  (yyval.value).token = CSS_FUNCTION_END;
}
    break;

  case 405:

    { PropertyValue val; val.token = CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT; val.value.str.str_len = 0; parser->AddFunctionValueL(val); }
    break;

  case 406:

    {
  (yyval.value).token = CSS_FUNCTION_END;
}
    break;

  case 407:

    { PropertyValue val; val.token = CSS_FUNCTION_O_LINEAR_GRADIENT; val.value.str.str_len = 0; parser->AddFunctionValueL(val); }
    break;

  case 408:

    {
  (yyval.value).token = CSS_FUNCTION_END;
}
    break;

  case 409:

    { PropertyValue val; val.token = CSS_FUNCTION_REPEATING_LINEAR_GRADIENT; val.value.str.str_len = 0; parser->AddFunctionValueL(val); }
    break;

  case 410:

    {
  (yyval.value).token = CSS_FUNCTION_END;
}
    break;

  case 411:

    { PropertyValue val; val.token = CSS_FUNCTION_RADIAL_GRADIENT; val.value.str.str_len = 0; parser->AddFunctionValueL(val); }
    break;

  case 412:

    {
  (yyval.value).token = CSS_FUNCTION_END;
}
    break;

  case 413:

    { PropertyValue val; val.token = CSS_FUNCTION_REPEATING_RADIAL_GRADIENT; val.value.str.str_len = 0; parser->AddFunctionValueL(val); }
    break;

  case 414:

    {
  (yyval.value).token = CSS_FUNCTION_END;
}
    break;

  case 415:

    { PropertyValue val; val.token = CSS_FUNCTION_DOUBLE_RAINBOW; val.value.str.str_len = 0; parser->AddFunctionValueL(val); }
    break;

  case 416:

    {
  (yyval.value).token = CSS_FUNCTION_END;
}
    break;

  case 417:

    {
  (yyval.value).token = CSS_FUNCTION_ATTR;
  (yyval.value).value.str.start_pos = (yyvsp[(3) - (6)].str).start_pos;
  (yyval.value).value.str.str_len = (yyvsp[(3) - (6)].str).str_len;
}
    break;

  case 418:

    {
  (yyval.value).token = CSS_FUNCTION_COUNTER;
  (yyval.value).value.str.start_pos = (yyvsp[(3) - (6)].str).start_pos;
  (yyval.value).value.str.str_len = (yyvsp[(3) - (6)].str).str_len;
}
    break;

  case 419:

    {
  (yyval.value).token = CSS_FUNCTION_COUNTER;
  (yyval.value).value.str.start_pos = (yyvsp[(3) - (10)].str).start_pos;
  (yyval.value).value.str.str_len = (yyvsp[(7) - (10)].str).start_pos - (yyvsp[(3) - (10)].str).start_pos + (yyvsp[(7) - (10)].str).str_len;
}
    break;

  case 420:

    {
  (yyval.value).token = CSS_FUNCTION_COUNTERS;
  (yyval.value).value.str.start_pos = (yyvsp[(3) - (10)].str).start_pos;
  (yyval.value).value.str.str_len = (yyvsp[(7) - (10)].str).start_pos - (yyvsp[(3) - (10)].str).start_pos + (yyvsp[(7) - (10)].str).str_len + 1;
}
    break;

  case 421:

    {
  (yyval.value).token = CSS_FUNCTION_COUNTERS;
  (yyval.value).value.str.start_pos = (yyvsp[(3) - (14)].str).start_pos;
  (yyval.value).value.str.str_len = (yyvsp[(11) - (14)].str).start_pos - (yyvsp[(3) - (14)].str).start_pos + (yyvsp[(11) - (14)].str).str_len;
}
    break;

  case 422:

    {
  PropertyValue val;
  val.token = CSS_FUNCTION_RECT;
  parser->AddValueL(val);
  parser->AddValueL((yyvsp[(3) - (8)].value));
  parser->AddValueL((yyvsp[(4) - (8)].value));
  parser->AddValueL((yyvsp[(5) - (8)].value));
  parser->AddValueL((yyvsp[(6) - (8)].value));
  (yyval.value).token = CSS_FUNCTION_RECT;
}
    break;

  case 423:

    {
  PropertyValue val;
  val.token = CSS_FUNCTION_RECT;
  parser->AddValueL(val);
  parser->AddValueL((yyvsp[(3) - (14)].value));
  parser->AddValueL((yyvsp[(6) - (14)].value));
  parser->AddValueL((yyvsp[(9) - (14)].value));
  parser->AddValueL((yyvsp[(12) - (14)].value));
  (yyval.value).token = CSS_FUNCTION_RECT;
}
    break;

  case 424:

    {
  (yyval.value).token = CSS_FUNCTION_SKIN;
  (yyval.value).value.str.start_pos = (yyvsp[(3) - (6)].str).start_pos;
  (yyval.value).value.str.str_len = (yyvsp[(3) - (6)].str).str_len;
}
    break;

  case 425:

    {
#ifdef GADGET_SUPPORT
  PropertyValue val;
  val.token = CSS_IDENT;
  val.value.str.start_pos = (yyvsp[(3) - (9)].str).start_pos;
  val.value.str.str_len = (yyvsp[(3) - (9)].str).str_len;
  parser->AddValueL(val);
  val.value.str.start_pos = (yyvsp[(5) - (9)].str).start_pos;
  val.value.str.str_len = (yyvsp[(5) - (9)].str).str_len;
  parser->AddValueL(val);
#endif // GADGET_SUPPORT
  (yyval.value).token = 0;
}
    break;

  case 426:

    {
  (yyval.value).token = CSS_FUNCTION_LANGUAGE_STRING;
  (yyval.value).value.integer.integer = (yyvsp[(3) - (6)].integer).integer;
}
    break;

  case 427:

    {
  (yyval.value).token = CSS_FUNCTION_LANGUAGE_STRING;
#ifdef LOCALE_SET_FROM_STRING
  uni_char* locale_string = parser->InputBuffer()->GetString((yyvsp[(3) - (6)].str).start_pos, (yyvsp[(3) - (6)].str).str_len);
  if (locale_string)
  {
    (yyval.value).value.integer.integer = Str::LocaleString(locale_string);
    OP_DELETEA(locale_string);
  }
  else
    YYOUTOFMEM;
#else
  (yyval.value).value.integer.integer = 0;
#endif // LOCALE_SET_FROM_STRING
}
    break;

  case 428:

    { PropertyValue val; val.token = CSS_FUNCTION_LOCAL_START; val.value.str.str_len = 0; parser->AddFunctionValueL(val); }
    break;

  case 429:

    {
  (yyval.value).token = CSS_FUNCTION_END;
}
    break;

  case 430:

    {
  (yyval.value).token = CSS_FUNCTION_FORMAT;
  (yyval.value).value.shortint = (yyvsp[(3) - (5)].shortint);
}
    break;

  case 431:

    {
  PropertyValue val;
  val.token = CSS_FUNCTION_CUBIC_BEZ;
  parser->AddValueL(val);
  val.token = CSS_NUMBER;
  val.value.number.number = (yyvsp[(3) - (18)].number).number;
  parser->AddValueL(val);
  val.value.number.number = (yyvsp[(7) - (18)].number).number;
  parser->AddValueL(val);
  val.value.number.number = (yyvsp[(11) - (18)].number).number;
  parser->AddValueL(val);
  val.value.number.number = (yyvsp[(15) - (18)].number).number;
  parser->AddValueL(val);
  (yyval.value).token = 0;
}
    break;

  case 432:

    {
  (yyval.value).token = CSS_FUNCTION_INTEGER;
  (yyval.value).value.integer.integer = (yyvsp[(3) - (6)].integer).integer;
}
    break;

  case 433:

    {
  if ((yyvsp[(5) - (7)].value).value.steps.steps > 0)
    {
      (yyval.value) = (yyvsp[(5) - (7)].value);
      OP_ASSERT((yyvsp[(5) - (7)].value).token == CSS_FUNCTION_STEPS);
      (yyval.value).value.steps.steps = (yyvsp[(3) - (7)].integer).integer;
    }
  else
    (yyval.value).token = 0;
}
    break;

  case 434:

    { (yyval.value).token = -1; }
    break;

  case 435:

    { (yyval.value).token = -1; }
    break;

  case 436:

    { (yyval.value).token = CSS_ASTERISK_CHAR; }
    break;

  case 437:

    { (yyval.value).token = CSS_HASH_CHAR; }
    break;

  case 438:

    { (yyval.value).token = 0; }
    break;

  case 439:

    {
  (yyval.value).token = CSS_FUNCTION_STEPS;
  (yyval.value).value.steps.steps = 1;
  short keyword = parser->InputBuffer()->GetValueSymbol((yyvsp[(3) - (4)].str).start_pos, (yyvsp[(3) - (4)].str).str_len);
  if (keyword == CSS_VALUE_start)
    (yyval.value).value.steps.start = TRUE;
  else if (keyword == CSS_VALUE_end)
    (yyval.value).value.steps.start = FALSE;
  else
    (yyval.value).value.steps.steps = 0;
}
    break;

  case 440:

    {
  (yyval.value).token = CSS_FUNCTION_STEPS;
  (yyval.value).value.steps.steps = 1;
  (yyval.value).value.steps.start = FALSE;
}
    break;

  case 441:

    {
  PropertyValue val;
  val.token = (yyvsp[(1) - (1)].shortint);
  parser->AddValueL(val);
}
    break;

  case 444:

    {
  (yyvsp[(2) - (2)].value).value.number.number*=(yyvsp[(1) - (2)].integer).integer;
  parser->AddValueL((yyvsp[(2) - (2)].value));
}
    break;

  case 445:

    {
  parser->AddValueL((yyvsp[(1) - (1)].value));
}
    break;

  case 446:

    {
  (yyval.shortint) = parser->InputBuffer()->GetFontFormat((yyvsp[(1) - (2)].str).start_pos, (yyvsp[(1) - (2)].str).str_len);
}
    break;

  case 447:

    {
  (yyval.shortint) = (yyvsp[(1) - (5)].shortint) | parser->InputBuffer()->GetFontFormat((yyvsp[(4) - (5)].str).start_pos, (yyvsp[(4) - (5)].str).str_len);
}
    break;

  case 448:

    {
#ifdef GADGET_SUPPORT
  PropertyValue val;
  val.token = CSS_FUNCTION_DASHBOARD_REGION;
  parser->AddValueL(val);
  val.token = CSS_NUMBER;
  val.value.number.number = 0;
  for (int i=0; i<4; i++)
    {
      parser->AddValueL(val);
    }
#endif // GADGET_SUPPORT
}
    break;

  case 449:

    {
#ifdef GADGET_SUPPORT
  PropertyValue val;
  val.token = CSS_FUNCTION_DASHBOARD_REGION;
  parser->AddValueL(val);
  parser->AddValueL((yyvsp[(1) - (4)].value));
  parser->AddValueL((yyvsp[(2) - (4)].value));
  parser->AddValueL((yyvsp[(3) - (4)].value));
  parser->AddValueL((yyvsp[(4) - (4)].value));
#endif // GADGET_SUPPORT
}
    break;

  case 450:

    {
  (yyval.integer).start_pos = (yyvsp[(2) - (2)].uinteger).start_pos;
  (yyval.integer).str_len = (yyvsp[(2) - (2)].uinteger).str_len;
  (yyval.integer).integer = (yyvsp[(1) - (2)].integer).integer * (yyvsp[(2) - (2)].uinteger).integer;
}
    break;

  case 451:

    {
  (yyval.integer).start_pos = (yyvsp[(1) - (1)].uinteger).start_pos;
  (yyval.integer).str_len = (yyvsp[(1) - (1)].uinteger).str_len;
  (yyval.integer).integer = static_cast<int>((yyvsp[(1) - (1)].uinteger).integer);
}
    break;

  case 452:

    { (yyval.number) = (yyvsp[(2) - (2)].number); (yyval.number).number *= (yyvsp[(1) - (2)].integer).integer; }
    break;

  case 453:

    { (yyval.number) = (yyvsp[(1) - (1)].number); }
    break;

  case 454:

    { (yyval.number) = (yyvsp[(2) - (2)].number); (yyval.number).number *= (yyvsp[(1) - (2)].integer).integer; }
    break;

  case 455:

    { (yyval.number) = (yyvsp[(1) - (1)].number); }
    break;

  case 456:

    {
  (yyval.number).start_pos = (yyvsp[(1) - (1)].integer).start_pos;
  (yyval.number).str_len = (yyvsp[(1) - (1)].integer).str_len;
  (yyval.number).number = double((yyvsp[(1) - (1)].integer).integer);
}
    break;

  case 457:

    { (yyval.value).token=CSS_NUMBER; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 458:

    {
  if (parser->UseIntegerTokens())
    {
      (yyval.value).token = CSS_INT_NUMBER;
      (yyval.value).value.integer.integer = MIN((yyvsp[(1) - (2)].uinteger).integer, INT_MAX);
      (yyval.value).value.integer.start_pos = (yyvsp[(1) - (2)].uinteger).start_pos;
      (yyval.value).value.integer.str_len = (yyvsp[(1) - (2)].uinteger).str_len;
    }
  else
    {
      (yyval.value).token = CSS_NUMBER;
      (yyval.value).value.number.number = double((yyvsp[(1) - (2)].uinteger).integer);
      (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].uinteger).start_pos;
      (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].uinteger).str_len;
    }
}
    break;

  case 459:

    { (yyval.value).token=CSS_PERCENTAGE; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 460:

    { (yyval.value).token=CSS_PX; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 461:

    { (yyval.value).token=CSS_CM; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 462:

    { (yyval.value).token=CSS_MM; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 463:

    { (yyval.value).token=CSS_IN; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 464:

    { (yyval.value).token=CSS_PT; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 465:

    { (yyval.value).token=CSS_PC; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 466:

    { (yyval.value).token=CSS_EM; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 467:

    { (yyval.value).token=CSS_REM; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 468:

    { (yyval.value).token=CSS_EX; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 469:

    { (yyval.value).token=CSS_RAD; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 470:

    { (yyval.value).token=CSS_SECOND; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 471:

    { (yyval.value).token=CSS_HZ; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 472:

    { (yyval.value).token=CSS_DIMEN; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 473:

    { (yyval.value).token=CSS_DPI; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 474:

    { (yyval.value).token=CSS_DPCM; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 475:

    { (yyval.value).token=CSS_DPPX; (yyval.value).value.number.number=(yyvsp[(1) - (2)].number).number; (yyval.value).value.number.start_pos = (yyvsp[(1) - (2)].number).start_pos; (yyval.value).value.number.str_len = (yyvsp[(1) - (2)].number).str_len; }
    break;

  case 476:

    {
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Unrecognized function"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
  (yyval.value).token = CSS_FUNCTION;
}
    break;

  case 477:

    {}
    break;

  case 478:

    {}
    break;

  case 479:

    {}
    break;

  case 480:

    {
  (yyval.str).start_pos = (yyvsp[(1) - (2)].str).start_pos;
  (yyval.str).str_len = (yyvsp[(1) - (2)].str).str_len;
}
    break;



      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
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
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
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
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
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
		  yystos[yystate], yyvsp);
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
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
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





inline int
yylex(YYSTYPE* value, CSS_Parser* css_parser)
{
  return css_parser->Lex(value);
}

inline int
csserrorverb(const char* s, CSS_Parser* css_parser)
{
  return 0;
}

#endif // BISON_PARSER
