#ifndef CSS_GRAMMAR_H
#define CSS_GRAMMAR_H

#ifdef BISON_PARSER

/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C

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




#endif // BISON_PARSER
#endif // CSS_GRAMMAR_H
