/* vim:tabstop=2:expandtab
 *
 * Copyright (c) 2007 Opera Software
 */

%{
#include "core/pch.h"

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

%}

%pure_parser

%union {
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
}

%token CSS_TOK_SPACE
%token CSS_TOK_CDO
%token CSS_TOK_CDC
%token CSS_TOK_INCLUDES
%token CSS_TOK_DASHMATCH
%token CSS_TOK_PREFIXMATCH
%token CSS_TOK_SUFFIXMATCH
%token CSS_TOK_SUBSTRMATCH
%token CSS_TOK_STRING_TRUNC
%token <str> CSS_TOK_STRING
%token <str> CSS_TOK_IDENT
%token <str> CSS_TOK_HASH
%token CSS_TOK_IMPORT_SYM
%token CSS_TOK_PAGE_SYM
%token CSS_TOK_MEDIA_SYM
%token CSS_TOK_SUPPORTS_SYM
%token CSS_TOK_FONT_FACE_SYM
%token CSS_TOK_CHARSET_SYM
%token CSS_TOK_NAMESPACE_SYM
%token CSS_TOK_VIEWPORT_SYM
%token CSS_TOK_KEYFRAMES_SYM
%token CSS_TOK_ATKEYWORD
%token CSS_TOK_IMPORTANT_SYM
%token <number> CSS_TOK_EMS
%token <number> CSS_TOK_REMS
%token <number> CSS_TOK_EXS
%token <number> CSS_TOK_LENGTH_PX
%token <number> CSS_TOK_LENGTH_CM
%token <number> CSS_TOK_LENGTH_MM
%token <number> CSS_TOK_LENGTH_IN
%token <number> CSS_TOK_LENGTH_PT
%token <number> CSS_TOK_LENGTH_PC
%token <number> CSS_TOK_ANGLE
%token <number> CSS_TOK_TIME
%token <number> CSS_TOK_FREQ
%token <number> CSS_TOK_DIMEN
%token <number> CSS_TOK_PERCENTAGE
%token <number> CSS_TOK_DPI
%token <number> CSS_TOK_DPCM
%token <number> CSS_TOK_DPPX
%token <number> CSS_TOK_NUMBER
%token <uinteger> CSS_TOK_INTEGER
%token <uinteger> CSS_TOK_N
%token <str> CSS_TOK_URI
%token <str> CSS_TOK_FUNCTION
%token CSS_TOK_UNICODERANGE
%token CSS_TOK_RGB_FUNC
%token CSS_TOK_HSL_FUNC
%token CSS_TOK_NOT_FUNC
%token CSS_TOK_RGBA_FUNC
%token CSS_TOK_HSLA_FUNC
%token CSS_TOK_LINEAR_GRADIENT_FUNC
%token CSS_TOK_WEBKIT_LINEAR_GRADIENT_FUNC
%token CSS_TOK_O_LINEAR_GRADIENT_FUNC
%token CSS_TOK_REPEATING_LINEAR_GRADIENT_FUNC
%token CSS_TOK_RADIAL_GRADIENT_FUNC
%token CSS_TOK_REPEATING_RADIAL_GRADIENT_FUNC
%token CSS_TOK_DOUBLE_RAINBOW_FUNC
%token CSS_TOK_ATTR_FUNC
%token <shortint> CSS_TOK_TRANSFORM_FUNC
%token CSS_TOK_COUNTER_FUNC
%token CSS_TOK_COUNTERS_FUNC
%token CSS_TOK_RECT_FUNC
%token CSS_TOK_DASHBOARD_REGION_FUNC
%token CSS_TOK_OR
%token CSS_TOK_AND
%token CSS_TOK_NOT
%token CSS_TOK_ONLY
%token CSS_TOK_LANG_FUNC
%token CSS_TOK_SKIN_FUNC
%token CSS_TOK_LANGUAGE_STRING_FUNC
%token CSS_TOK_NTH_CHILD_FUNC
%token CSS_TOK_NTH_OF_TYPE_FUNC
%token CSS_TOK_NTH_LAST_CHILD_FUNC
%token CSS_TOK_NTH_LAST_OF_TYPE_FUNC
%token CSS_TOK_FORMAT_FUNC
%token CSS_TOK_LOCAL_FUNC
%token CSS_TOK_CUBIC_BEZ_FUNC
%token CSS_TOK_INTEGER_FUNC
%token CSS_TOK_STEPS_FUNC
%token CSS_TOK_CUE_FUNC
%token CSS_TOK_EOF
%token CSS_TOK_UNRECOGNISED_CHAR

%token CSS_TOK_STYLE_ATTR
%token CSS_TOK_STYLE_ATTR_STRICT

%token CSS_TOK_DOM_STYLE_PROPERTY
%token CSS_TOK_DOM_PAGE_PROPERTY
%token CSS_TOK_DOM_FONT_DESCRIPTOR
%token CSS_TOK_DOM_VIEWPORT_PROPERTY
%token CSS_TOK_DOM_KEYFRAME_PROPERTY

%token CSS_TOK_DOM_RULE
%token CSS_TOK_DOM_STYLE_RULE
%token CSS_TOK_DOM_CHARSET_RULE
%token CSS_TOK_DOM_IMPORT_RULE
%token CSS_TOK_DOM_SUPPORTS_RULE
%token CSS_TOK_DOM_MEDIA_RULE
%token CSS_TOK_DOM_FONT_FACE_RULE
%token CSS_TOK_DOM_PAGE_RULE
%token CSS_TOK_DOM_VIEWPORT_RULE
%token CSS_TOK_DOM_KEYFRAMES_RULE
%token CSS_TOK_DOM_KEYFRAME_RULE
%token CSS_TOK_DOM_SELECTOR
%token CSS_TOK_DOM_PAGE_SELECTOR
%token CSS_TOK_DOM_MEDIA_LIST
%token CSS_TOK_DOM_MEDIUM
%token CSS_TOK_DECLARATION

%type <shortint> property format_list
%type <ushortint> combinator eq_op nth_func
%type <boolean> prio simple_selector selector selector_parms selector_parms_notempty selectors media_query_op ns_prefix media_query_exp media_query_exp_list
%type <value> term num_term operator steps_start
%type <str> id_or_str namespace_uri string_or_uri hash page_name page_selector
%type <boolean> selector_parm class attrib pseudo not_parm type_selector elm_type end_of_import keyframe_selector_list
%type <number> percentage number keyframe_selector
%type <integer> unary_operator integer
%type <id_with_ns> attr_name
%type <linear_func> nth_expr

%%

stylesheet
: commenttags_spaces stylesheet_body CSS_TOK_EOF
| CSS_TOK_STYLE_ATTR_STRICT { parser->StoreBlockLevel(yychar); } spaces declarations trailing_bracket CSS_TOK_EOF
| CSS_TOK_STYLE_ATTR { parser->StoreBlockLevel(yychar); } spaces quirk_style_attr
| style_decl_token spaces expr prio CSS_TOK_EOF
{
  CSS_property_list* prop_list = OP_NEW(CSS_property_list, ());
  if (!prop_list)
    YYOUTOFMEM;
  parser->SetCurrentPropList(prop_list);
  CSS_Parser::DeclStatus ds = parser->AddDeclarationL(parser->GetDOMProperty(), $4);
  if (ds == CSS_Parser::OUT_OF_MEM)
    YYOUTOFMEM;
#ifdef CSS_ERROR_SUPPORT
  else if (parser->GetDOMProperty() >= 0 && ds == CSS_Parser::INVALID)
    parser->InvalidDeclarationL(ds, parser->GetDOMProperty());
#endif // CSS_ERROR_SUPPORT
}
| CSS_TOK_DOM_STYLE_RULE spaces ruleset spaces CSS_TOK_EOF
| CSS_TOK_DOM_CHARSET_RULE spaces charset spaces CSS_TOK_EOF
| CSS_TOK_DOM_IMPORT_RULE spaces import spaces CSS_TOK_EOF
| CSS_TOK_DOM_SUPPORTS_RULE spaces supports spaces CSS_TOK_EOF
| CSS_TOK_DOM_MEDIA_RULE spaces media spaces CSS_TOK_EOF
| CSS_TOK_DOM_FONT_FACE_RULE spaces font_face spaces CSS_TOK_EOF
| CSS_TOK_DOM_PAGE_RULE spaces page spaces CSS_TOK_EOF
| CSS_TOK_DOM_VIEWPORT_RULE spaces viewport spaces CSS_TOK_EOF
| CSS_TOK_DOM_KEYFRAMES_RULE spaces keyframes spaces CSS_TOK_EOF
| CSS_TOK_DOM_KEYFRAME_RULE spaces { parser->BeginKeyframes(); } keyframe_ruleset CSS_TOK_EOF
| CSS_TOK_DOM_RULE spaces rule spaces CSS_TOK_EOF
| CSS_TOK_DOM_SELECTOR spaces selectors CSS_TOK_EOF
{
  if ($3)
    parser->EndSelectorL();
  else if (!parser->GetStyleSheet())
    /* Only flag as syntax error for Selectors API parsing.
       (GetStyleSheet() will return null for Selectors API
       parsing, but non-NULL for selectorText).
       We've decided to violate the spec for interoperability
       for now for DOM CSS (See CORE-10244). */
    parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
}
| CSS_TOK_DOM_PAGE_SELECTOR spaces page_selector CSS_TOK_EOF
{
  uni_char* page_name = NULL;
  ANCHOR_ARRAY(uni_char, page_name);
  if ($3.str_len > 0)
    {
      page_name = parser->InputBuffer()->GetString($3.start_pos, $3.str_len);
      if (!page_name)
	YYOUTOFMEM;
      ANCHOR_ARRAY_RESET(page_name);
    }
  parser->EndSelectorL(page_name);
  ANCHOR_ARRAY_RELEASE(page_name);
}
| CSS_TOK_DOM_MEDIA_LIST spaces media_list CSS_TOK_EOF
| CSS_TOK_DOM_MEDIA_LIST spaces media_list ';' CSS_TOK_EOF
| CSS_TOK_DOM_MEDIA_LIST spaces media_list '{' CSS_TOK_EOF
| CSS_TOK_DOM_MEDIUM spaces media_query CSS_TOK_EOF
| CSS_TOK_DOM_MEDIUM spaces media_query ';' CSS_TOK_EOF
| CSS_TOK_DOM_MEDIUM spaces media_query '{' CSS_TOK_EOF
| CSS_TOK_DOM_MEDIUM spaces media_query ',' CSS_TOK_EOF
| CSS_TOK_DECLARATION spaces declaration CSS_TOK_EOF
;

style_decl_token
: CSS_TOK_DOM_STYLE_PROPERTY
| CSS_TOK_DOM_KEYFRAME_PROPERTY { parser->BeginKeyframes(); }
| CSS_TOK_DOM_PAGE_PROPERTY { parser->BeginPage(); }
| CSS_TOK_DOM_FONT_DESCRIPTOR { parser->BeginFontface(); }
| CSS_TOK_DOM_VIEWPORT_PROPERTY { parser->BeginViewport(); }
;

trailing_bracket
: /* empty */
| '}' { parser->TerminateLastDecl(); }
;

quirk_style_attr
: declarations trailing_bracket CSS_TOK_EOF
| '{' { parser->StoreBlockLevel(yychar); } spaces declarations '}' spaces CSS_TOK_EOF
;

rule
: ruleset
| charset
| import
| supports
| media
| font_face
| page
| viewport
| keyframes
;

charset
: CSS_TOK_CHARSET_SYM spaces CSS_TOK_STRING spaces ';'
{
  if (parser->AllowCharset())
    {
      parser->SetAllowLevel(CSS_Parser::ALLOW_IMPORT);
      uni_char* charset = parser->InputBuffer()->GetString($3.start_pos, $3.str_len);
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
| CSS_TOK_CHARSET_SYM block_or_semi_recovery
{
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("@charset syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
;

stylesheet_body
: /* empty */
| stylesheet_body { parser->StoreBlockLevel(yychar); } stylesheet_body_elem commenttags_spaces
;

stylesheet_body_elem
: ruleset
| supports
| media
| page { parser->EndPage(); }
| font_face { parser->EndFontface(); }
| import
| namespace
| unknown_at
| charset
| viewport { parser->EndViewport(); }
| keyframes
| error CSS_TOK_CDC
{
  parser->DiscardRuleset();
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Syntax error before comment end (\"-->\")"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
;

block_or_semi_recovery
: error ';'
{
  parser->DiscardRuleset();
  parser->RecoverBlockOrSemi(';');
}
| error '}'
{
  parser->DiscardRuleset();
  parser->RecoverBlockOrSemi('}');
}
;

namespace
: CSS_TOK_NAMESPACE_SYM spaces CSS_TOK_IDENT spaces namespace_uri spaces end_of_ns
{
  if (parser->AllowNamespace())
    {
      unsigned int len = $3.str_len + $5.str_len;
      if (len < g_memory_manager->GetTempBufLen()-1)
	{
	  uni_char* name = (uni_char*)g_memory_manager->GetTempBuf();
	  uni_char* uri = name + $3.str_len + 1;
	  parser->InputBuffer()->GetString(name, $3.start_pos, $3.str_len);
	  parser->InputBuffer()->GetString(uri, $5.start_pos, $5.str_len);
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
| CSS_TOK_NAMESPACE_SYM spaces namespace_uri spaces end_of_ns
{
  if (parser->AllowNamespace())
    {
      uni_char* uri = (uni_char*)g_memory_manager->GetTempBuf();
      if ($3.str_len < g_memory_manager->GetTempBufLen())
	{
	  parser->InputBuffer()->GetString(uri, $3.start_pos, $3.str_len);
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
| CSS_TOK_NAMESPACE_SYM block_or_semi_recovery
{
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("@namespace syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
;

namespace_uri
: CSS_TOK_STRING { $$ = $1; }
| CSS_TOK_URI { $$ = $1; }
;

end_of_ns
: ';'
| CSS_TOK_EOF { parser->UnGetEOF(); }
;

unknown_at
: CSS_TOK_ATKEYWORD block_or_semi_recovery
{
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Unknown at-rule"), OpConsoleEngine::Information);
#endif // CSS_ERROR_SUPPORT
}
;

block
: '{' spaces block_elements '}';

block_elements
: /* empty */ | block_elements block_element;

block_element
: value_element
| ';' spaces;

value_element
: any spaces | block spaces | CSS_TOK_IMPORT_SYM spaces | CSS_TOK_PAGE_SYM spaces | CSS_TOK_SUPPORTS_SYM spaces | CSS_TOK_MEDIA_SYM spaces | CSS_TOK_FONT_FACE_SYM spaces | CSS_TOK_CHARSET_SYM spaces | CSS_TOK_NAMESPACE_SYM spaces | CSS_TOK_VIEWPORT_SYM spaces | CSS_TOK_KEYFRAMES_SYM spaces | CSS_TOK_ATKEYWORD spaces;

core_declaration
: CSS_TOK_IDENT spaces ':' spaces value ;

value
: value_element
| value value_element;

any_function
: fn_name spaces any_list ')';

fn_name
: CSS_TOK_ATTR_FUNC
| CSS_TOK_COUNTER_FUNC
| CSS_TOK_COUNTERS_FUNC
| CSS_TOK_CUBIC_BEZ_FUNC
| CSS_TOK_CUE_FUNC
| CSS_TOK_DASHBOARD_REGION_FUNC
| CSS_TOK_DOUBLE_RAINBOW_FUNC
| CSS_TOK_FORMAT_FUNC
| CSS_TOK_HSLA_FUNC
| CSS_TOK_HSL_FUNC
| CSS_TOK_INTEGER_FUNC
| CSS_TOK_LANG_FUNC
| CSS_TOK_LANGUAGE_STRING_FUNC
| CSS_TOK_LINEAR_GRADIENT_FUNC
| CSS_TOK_LOCAL_FUNC
| CSS_TOK_NOT_FUNC
| CSS_TOK_NTH_CHILD_FUNC
| CSS_TOK_NTH_LAST_CHILD_FUNC
| CSS_TOK_NTH_LAST_OF_TYPE_FUNC
| CSS_TOK_NTH_OF_TYPE_FUNC
| CSS_TOK_RADIAL_GRADIENT_FUNC
| CSS_TOK_RECT_FUNC
| CSS_TOK_REPEATING_LINEAR_GRADIENT_FUNC
| CSS_TOK_REPEATING_RADIAL_GRADIENT_FUNC
| CSS_TOK_RGBA_FUNC
| CSS_TOK_RGB_FUNC
| CSS_TOK_SKIN_FUNC
| CSS_TOK_STEPS_FUNC
| CSS_TOK_TRANSFORM_FUNC
| CSS_TOK_WEBKIT_LINEAR_GRADIENT_FUNC
| CSS_TOK_O_LINEAR_GRADIENT_FUNC
| CSS_TOK_FUNCTION;

any
: CSS_TOK_IDENT {}
| CSS_TOK_NUMBER {}
| CSS_TOK_INTEGER {}
| CSS_TOK_N {}
| CSS_TOK_IMPORTANT_SYM
| CSS_TOK_EMS {} | CSS_TOK_REMS {} | CSS_TOK_EXS {} | CSS_TOK_LENGTH_PX {} | CSS_TOK_LENGTH_CM {} | CSS_TOK_LENGTH_MM {} | CSS_TOK_LENGTH_IN {} | CSS_TOK_LENGTH_PT {}
| CSS_TOK_LENGTH_PC {} | CSS_TOK_ANGLE {} | CSS_TOK_TIME {} | CSS_TOK_FREQ {}
| CSS_TOK_PERCENTAGE {} | CSS_TOK_DPI | CSS_TOK_DPCM | CSS_TOK_DPPX {}
| CSS_TOK_DIMEN {}
| CSS_TOK_STRING_TRUNC
| CSS_TOK_STRING {}
| CSS_TOK_URI {}
| CSS_TOK_HASH {}
| CSS_TOK_UNICODERANGE
| CSS_TOK_INCLUDES
| any_function
| CSS_TOK_DASHMATCH | CSS_TOK_PREFIXMATCH | CSS_TOK_SUFFIXMATCH | CSS_TOK_SUBSTRMATCH
| CSS_TOK_OR
| CSS_TOK_AND
| CSS_TOK_NOT
| CSS_TOK_ONLY
| CSS_TOK_UNRECOGNISED_CHAR
| ':'
| '.'
| ','
| '/'
| '+'
| '-'
| '>'
| '*'
| '$'
| '~'
| '#'
| '(' spaces any_list ')'
| '[' spaces any_list ']'
;

any_list
: /* empty */ | any_list any spaces;

commenttags_spaces
: /* empty */ | commenttags_spaces comtag_or_space;

comtag_or_space
: CSS_TOK_SPACE | CSS_TOK_CDO | CSS_TOK_CDC;

spaces
: /*empty*/ | spaces CSS_TOK_SPACE;

spaces_plus
: CSS_TOK_SPACE | spaces_plus CSS_TOK_SPACE;

end_of_import
: ';' { $$ = TRUE; }
| CSS_TOK_EOF { parser->UnGetEOF(); $$ = TRUE; }
| '{' { parser->StoreBlockLevel(yychar); } error '}'
{
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("@import syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
  parser->DiscardRuleset(); parser->RecoverBlockOrSemi('}');
  $$ = FALSE;
}
;

import
: CSS_TOK_IMPORT_SYM spaces string_or_uri spaces media_list end_of_import
{
  if (!$6)
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
| CSS_TOK_IMPORT_SYM block_or_semi_recovery
{
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("@import syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
;

string_or_uri
: CSS_TOK_STRING | CSS_TOK_URI
;


supports_condition
: supports_negation
| supports_conjunction
| supports_disjunction
| supports_condition_in_parens
;

supports_condition_in_parens
: '(' spaces supports_condition ')' spaces
| supports_declaration_condition
;

supports_negation
: CSS_TOK_NOT spaces supports_condition_in_parens
{
  parser->NegateCurrentCondition();
}
;

supports_conjunction
: supports_condition_in_parens CSS_TOK_AND spaces supports_condition_in_parens
{
  CSS_CombinedCondition *cc;

  cc = OP_NEW(CSS_ConditionConjunction,());
  if (!cc) YYOUTOFMEM;

  cc->AddFirst(parser->PopCondition());
  cc->AddFirst(parser->PopCondition());

  parser->QueueCondition(cc);
}
| supports_conjunction  CSS_TOK_AND spaces supports_condition_in_parens
{
  CSS_Condition *c;
  CSS_CombinedCondition *cc;
  c = parser->PopCondition();
  cc = static_cast<CSS_CombinedCondition*>(parser->PopCondition());
  cc->AddLast(c);
  parser->QueueCondition(cc);
}
;

supports_disjunction
: supports_condition_in_parens CSS_TOK_OR spaces supports_condition_in_parens
{
  CSS_CombinedCondition *cc;
  cc = OP_NEW(CSS_ConditionDisjunction,());
  if (!cc) YYOUTOFMEM;

  cc->AddFirst(parser->PopCondition());
  cc->AddFirst(parser->PopCondition());

  parser->QueueCondition(cc);
}
| supports_disjunction  CSS_TOK_OR spaces supports_condition_in_parens
{
  CSS_Condition *c;
  CSS_CombinedCondition *cc;
  c = parser->PopCondition();
  cc = static_cast<CSS_CombinedCondition*>(parser->PopCondition());
  cc->AddLast(c);
  parser->QueueCondition(cc);
}
;

supports_declaration_condition
: '(' spaces core_declaration ')' spaces
{

  CSS_SimpleCondition *c = OP_NEW(CSS_SimpleCondition,());
  if (!c) YYOUTOFMEM;

  uni_char *decl = parser->InputBuffer()->GetString($<str>1.start_pos + 1, $<str>4.start_pos - $<str>1.start_pos - 1);

  if (!decl || OpStatus::IsMemoryError(c->SetDecl(decl)))
  {
    OP_DELETE(c);
    OP_DELETE(decl);
    YYOUTOFMEM;
  }

  parser->QueueCondition(c);
}
;

supports_start
: CSS_TOK_SUPPORTS_SYM spaces supports_condition ;

supports
: supports_start '{' spaces
{
  if (parser->AllowRuleset())
    parser->AddSupportsRuleL();
  else
    LEAVE(CSSParseStatus::HIERARCHY_ERROR);
}
group_rule_body '}'
{
  parser->EndConditionalRule();
}
| CSS_TOK_SUPPORTS_SYM block_or_semi_recovery
{
  parser->FlushConditionQueue();
}
| supports_start block_or_semi_recovery
{
  parser->FlushConditionQueue();
}
| supports_start error CSS_TOK_EOF
{
  parser->UnGetEOF();
  parser->FlushConditionQueue();
}
| supports_start CSS_TOK_EOF
{
  parser->UnGetEOF();
  parser->FlushConditionQueue();
}
;

media_start
: CSS_TOK_MEDIA_SYM spaces media_list
;

media
: media_start '{' spaces
{
  if (parser->AllowRuleset())
    parser->AddMediaRuleL();
  else
    LEAVE(CSSParseStatus::HIERARCHY_ERROR);
}
group_rule_body '}'
{
  parser->EndConditionalRule();
}
| media_start ';'
;

media_list
: /* empty */
| media_query_list
{
  parser->EndMediaQueryListL();
}
;

media_query_list
: media_query
{
  CSS_MediaObject* new_media_obj = OP_NEW(CSS_MediaObject, ());
  if (new_media_obj)
    parser->SetCurrentMediaObject(new_media_obj);
  else
    YYOUTOFMEM;
  parser->EndMediaQueryL();
}
| media_query_list { parser->StoreBlockLevel(yychar); } ',' spaces media_query_or_eof
{
  parser->EndMediaQueryL();
}
;

media_query_or_eof
: media_query
| CSS_TOK_EOF
{
  parser->UnGetEOF();
}
;

media_query
: media_query_op CSS_TOK_IDENT spaces
{
  CSS_MediaType media_type = parser->InputBuffer()->GetMediaType($2.start_pos, $2.str_len);
  CSS_MediaQuery* new_query = OP_NEW_L(CSS_MediaQuery, ($1, media_type));
  parser->SetCurrentMediaQuery(new_query);
} media_query_exp_list
{
  if (!$5)
    parser->SetCurrentMediaQuery(NULL);
}
|
{
  CSS_MediaQuery* new_query = OP_NEW_L(CSS_MediaQuery, (FALSE, CSS_MEDIA_TYPE_ALL));
  parser->SetCurrentMediaQuery(new_query);
} media_query_exp spaces media_query_exp_list
{
  if (!$2 || !$4)
    parser->SetCurrentMediaQuery(NULL);
}
| error '{'
{
  parser->RecoverMediaQuery('{');
  parser->SetCurrentMediaQuery(NULL);
}
| error ','
{
  parser->RecoverMediaQuery(',');
  parser->SetCurrentMediaQuery(NULL);
}
| error ';'
{
  parser->RecoverMediaQuery(';');
  parser->SetCurrentMediaQuery(NULL);
}
| error CSS_TOK_EOF
{
  parser->UnGetEOF();
  parser->SetCurrentMediaQuery(NULL);
}
;

media_query_op
: { $$ = FALSE; }
| CSS_TOK_ONLY spaces { $$ = FALSE; }
| CSS_TOK_NOT spaces { $$ = TRUE; }
;

media_query_exp_list
: media_query_exp_list CSS_TOK_AND spaces media_query_exp spaces
{
  $$ = $1 && $4;
}
| /*empty*/
{
  $$ = TRUE;
}
;

media_query_exp
: '(' spaces CSS_TOK_IDENT spaces ')'
{
  CSS_MediaFeature feature = parser->InputBuffer()->GetMediaFeature($3.start_pos, $3.str_len);
  $$ = parser->AddMediaFeatureExprL(feature);
}
| '(' spaces CSS_TOK_IDENT spaces ':' spaces expr ')'
{
  CSS_MediaFeature feature = parser->InputBuffer()->GetMediaFeature($3.start_pos, $3.str_len);
  $$ = parser->AddMediaFeatureExprL(feature);
  parser->ResetValCount();
}
| '(' error ')'
{
  $$ = FALSE;
  parser->ResetValCount();
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Media feature syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
;

page_name
: CSS_TOK_IDENT { $$ = $1; }
;

pseudo_page
: ':' CSS_TOK_IDENT
{
  CSS_Selector* sel = OP_NEW(CSS_Selector, ());
  CSS_SimpleSelector* simple_sel = OP_NEW(CSS_SimpleSelector, (Markup::HTE_ANY));
  CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_PSEUDO_PAGE, parser->InputBuffer()->GetPseudoPage($2.start_pos, $2.str_len)));

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
;

page_selector
: CSS_TOK_PAGE_SYM spaces { $$.start_pos = 0; $$.str_len = 0; }
| CSS_TOK_PAGE_SYM spaces pseudo_page spaces { $$.start_pos = 0; $$.str_len = 0; }
| CSS_TOK_PAGE_SYM spaces page_name spaces { $$ = $3; }
| CSS_TOK_PAGE_SYM spaces page_name pseudo_page spaces { $$ = $3; }
;

page
: page_selector '{' spaces { parser->StoreBlockLevel(yychar); parser->BeginPage(); } declarations '}'
{
  uni_char* page_name = NULL;
  ANCHOR_ARRAY(uni_char, page_name);
  if ($1.str_len > 0)
    {
      page_name = parser->InputBuffer()->GetString($1.start_pos, $1.str_len);
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
| CSS_TOK_PAGE_SYM block_or_semi_recovery
{
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("@page syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
;

font_face
: CSS_TOK_FONT_FACE_SYM spaces '{' spaces { parser->StoreBlockLevel(yychar); parser->BeginFontface(); } declarations '}'
{
  if (parser->AllowRuleset())
    parser->AddFontfaceRuleL();
  else
    {
      parser->DiscardRuleset();
      LEAVE(CSSParseStatus::HIERARCHY_ERROR);
    }
}
| CSS_TOK_FONT_FACE_SYM block_or_semi_recovery
{
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("@font-face syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
;

viewport
: CSS_TOK_VIEWPORT_SYM spaces '{' spaces { parser->StoreBlockLevel(yychar); parser->BeginViewport(); } declarations '}'
{
  parser->AddViewportRuleL();
}
| CSS_TOK_VIEWPORT_SYM block_or_semi_recovery
{
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("@-o-viewport syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
;

keyframe_selector
: CSS_TOK_IDENT
{
#ifdef CSS_ANIMATIONS
  $$.number = parser->InputBuffer()->GetKeyframePosition($1.start_pos, $1.str_len);
  $$.start_pos = $1.start_pos;
  $$.str_len = $1.str_len;

  if ($$.number < 0)
    {
      parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
#ifdef CSS_ERROR_SUPPORT
      parser->EmitErrorL(UNI_L("Unknown keyframe selector"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
    }
#endif // CSS_ANIMATIONS
}
| percentage
{
  $$ = $1;
  $$.number /= 100.0;
}
;

keyframe_selector_list
: keyframe_selector_list spaces ',' spaces keyframe_selector
{
#ifdef CSS_ANIMATIONS
  $$ = $1 && $5.number >= 0;
  if ($$)
    {
      CSS_KeyframeSelector* keyframe_selector = OP_NEW(CSS_KeyframeSelector, ($5.number));
      if (!keyframe_selector)
	YYOUTOFMEM;
      parser->AppendKeyframeSelector(keyframe_selector);
    }
#endif // CSS_ANIMATIONS
}
| keyframe_selector
{
#ifdef CSS_ANIMATIONS
  $$ = $1.number >= 0;
  if ($$)
    {
      CSS_KeyframeSelector* keyframe_selector = OP_NEW(CSS_KeyframeSelector, ($1.number));
      if (!keyframe_selector)
	YYOUTOFMEM;
      parser->AppendKeyframeSelector(keyframe_selector);
    }
#endif // CSS_ANIMATIONS
}
;

keyframe_ruleset
: keyframe_selector_list spaces '{' spaces { parser->StoreBlockLevel(yychar); } declarations '}'
{
#ifdef CSS_ANIMATIONS
  if ($1)
    parser->AddKeyframeRuleL();
  else
    parser->DiscardRuleset();
#endif // CSS_ANIMATIONS
}
| error '{'
{
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
  parser->RecoverBlockOrSemi('{');
  parser->DiscardRuleset();

#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Unknown keyframe selector"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
| error '}'
{
  parser->SetErrorOccurred(CSSParseStatus::SYNTAX_ERROR);
  parser->RecoverBlockOrSemi('}');
  parser->DiscardRuleset();

#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Malformed keyframe"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
;

keyframe_ruleset_list
: keyframe_ruleset_list { parser->StoreBlockLevel(yychar); } keyframe_ruleset spaces
| /*empty*/
;

keyframes
: CSS_TOK_KEYFRAMES_SYM spaces CSS_TOK_IDENT spaces '{' spaces
{
#ifdef CSS_ANIMATIONS
  parser->StoreBlockLevel(yychar);

  uni_char* keyframes_name = NULL;
  ANCHOR_ARRAY(uni_char, keyframes_name);
  if ($3.str_len > 0)
    {
      keyframes_name = parser->InputBuffer()->GetString($3.start_pos, $3.str_len);
      if (!keyframes_name)
	YYOUTOFMEM;
      ANCHOR_ARRAY_RESET(keyframes_name);
    }

  parser->BeginKeyframesL(keyframes_name);
  ANCHOR_ARRAY_RELEASE(keyframes_name);
#endif // CSS_ANIMATIONS
}
keyframe_ruleset_list '}'
{
  parser->EndKeyframes();
}
| CSS_TOK_KEYFRAMES_SYM block_or_semi_recovery
;

operator
: '/' spaces { $$.token = CSS_SLASH; } | ',' spaces { $$.token = CSS_COMMA; } | /* empty */ { $$.token = 0; }
;

combinator
: '+' spaces { $$=CSS_COMBINATOR_ADJACENT; }
| '>' spaces { $$=CSS_COMBINATOR_CHILD; }
| '~' spaces { $$=CSS_COMBINATOR_ADJACENT_INDIRECT; }
;

unary_operator
: '-' { $$.integer=-1; } | '+' { $$.integer=1; }
;

property
: CSS_TOK_IDENT spaces
{
  $$ = parser->InputBuffer()->GetPropertySymbol($1.start_pos, $1.str_len);
#ifdef CSS_ERROR_SUPPORT
  if ($$ < 0)
    {
      OpString msg;
      ANCHOR(OpString, msg);
      uni_char* cstr = msg.ReserveL($1.str_len+2);
      int start_pos = $1.start_pos;
      int str_len = $1.str_len;
      parser->InputBuffer()->GetString(cstr, start_pos, str_len);
      msg.AppendL(" is an unknown property");
      parser->EmitErrorL(msg.CStr(), OpConsoleEngine::Information);
    }
#endif // CSS_ERROR_SUPPORT
}
;

nested_statement
: ruleset
| media
| page { parser->EndPage(); }
| font_face { parser->EndFontface(); }
| import
| namespace
| charset
| keyframes
| supports
| viewport { parser->EndViewport(); }
| unknown_at
| error '}'
{
  parser->DiscardRuleset();
  parser->RecoverBlockOrSemi('}');
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Syntax error at end of conditional rule"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
;

group_rule_body
: { parser->StoreBlockLevel(yychar); } /* empty */
| group_rule_body { parser->StoreBlockLevel(yychar); } nested_statement spaces
;

ruleset
: selectors '{' spaces { parser->StoreBlockLevel(yychar); } declarations '}'
{
  if ($1 && parser->AllowRuleset())
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
| error '{'
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
;

declarations
:
{
  CSS_property_list* new_list = OP_NEW(CSS_property_list, ());
  if (!new_list)
    YYOUTOFMEM;
  parser->SetCurrentPropList(new_list);
} declaration
| declarations { parser->StoreBlockLevel(yychar); } ';' spaces declaration
;

declaration
: property ':' spaces expr prio
{
  if ($1 >= 0)
    {
      CSS_Parser::DeclStatus ds = parser->AddDeclarationL($1, $5);
      if (ds == CSS_Parser::OUT_OF_MEM)
	YYOUTOFMEM;
#ifdef CSS_ERROR_SUPPORT
      else if (ds == CSS_Parser::INVALID)
	parser->InvalidDeclarationL(ds, $1);
#endif // CSS_ERROR_SUPPORT
    }
  else
    parser->EmptyDecl();

  parser->ResetValCount();
}
| property ':' spaces expr prio error ';'
{
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Declaration syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
  parser->EmptyDecl();
  parser->ResetValCount();
  parser->RecoverDeclaration(';');
}
| property ':' spaces expr prio error '}'
{
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Declaration syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
  parser->EmptyDecl();
  parser->ResetValCount();
  parser->RecoverDeclaration('}');
}
| property ':' spaces expr prio error CSS_TOK_EOF
{
  parser->UnGetEOF();
  parser->EmptyDecl();
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Declaration syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
| /* empty */ { parser->EmptyDecl(); }
| error ';'
{
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Declaration syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
  parser->EmptyDecl();
  parser->ResetValCount();
  parser->RecoverDeclaration(';');
}
| error '}'
{
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Declaration syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
  parser->EmptyDecl();
  parser->ResetValCount();
  parser->RecoverDeclaration('}');
}
| error CSS_TOK_EOF
{
  parser->UnGetEOF();
  parser->EmptyDecl();
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Declaration syntax error"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
}
;

selectors
: selector
{
  // Ensure we don't have any selectors left from a previous error token.
  OP_ASSERT(!parser->HasSelectors());
  $$ = $1;
  if ($$)
    parser->AddCurrentSelectorL();
}
| selectors ',' spaces selector
{
  $$ = $1 && $4;
  if ($$)
    parser->AddCurrentSelectorL();
}
;

selector
: simple_selector
{
  $$ = $1;
  if ($1)
    {
      CSS_Selector* new_sel = OP_NEW(CSS_Selector, ());
      if (new_sel)
	{
	  parser->SetCurrentSelector(new_sel);
	  $$ = parser->AddCurrentSimpleSelector();
	}
      else
	YYOUTOFMEM;
    }
  else
    parser->DeleteCurrentSimpleSelector();
}
| simple_selector spaces_plus
{
  $$ = $1;
  if ($1)
    {
      CSS_Selector* new_sel = OP_NEW(CSS_Selector, ());
      if (new_sel)
	{
	  parser->SetCurrentSelector(new_sel);
	  $$ = parser->AddCurrentSimpleSelector();
	}
      else
	YYOUTOFMEM;
    }
  else
    parser->DeleteCurrentSimpleSelector();
}
| simple_selector combinator selector
{
  $$ = $1 && $3;
  if ($$)
    $$ = parser->AddCurrentSimpleSelector($2);
  else
    {
      parser->DeleteCurrentSimpleSelector();
      parser->DeleteCurrentSelector();
    }
}
| simple_selector spaces_plus combinator selector
{
  $$ = $1 && $4;
  if ($$)
    $$ = parser->AddCurrentSimpleSelector($3);
  else
    {
      parser->DeleteCurrentSimpleSelector();
      parser->DeleteCurrentSelector();
    }
}
| simple_selector spaces_plus selector
{
  $$ = $1 && $3;
  if ($$)
    $$ = parser->AddCurrentSimpleSelector(CSS_COMBINATOR_DESCENDANT);
  else
    {
      parser->DeleteCurrentSimpleSelector();
      parser->DeleteCurrentSelector();
    }
}
;

simple_selector
: element_name selector_parms
{
  $$=$2;
}
| '|' { parser->SetCurrentNameSpaceIdx(NS_IDX_DEFAULT); } element_name { parser->ResetCurrentNameSpaceIdx(); } selector_parms
{
  $$=$5;
}
| '*' '|' { parser->SetCurrentNameSpaceIdx(NS_IDX_ANY_NAMESPACE); } element_name { parser->ResetCurrentNameSpaceIdx(); } selector_parms
{
  $$=$6;
}
| CSS_TOK_IDENT '|'
{
  uni_char* name = (uni_char*)g_memory_manager->GetTempBuf();
  int len = MIN($1.str_len, g_memory_manager->GetTempBufLen()-1);
  if (parser->InputBuffer()->GetString(name, $1.start_pos, len))
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
element_name { parser->ResetCurrentNameSpaceIdx(); } selector_parms
{
  $$=($6 && parser->GetCurrentSimpleSelector()->GetNameSpaceIdx() != NS_IDX_NOT_FOUND);
}
|
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
selector_parms_notempty { $$=$2; }
;

selector_parms
: /* empty */ { $$ = TRUE; }
| selector_parms selector_parm
{
  $$ = ($1 && $2);
}
;

selector_parms_notempty
: selector_parms selector_parm
{
  $$ = ($1 && $2);
}
;

selector_parm
: CSS_TOK_HASH
{
  uni_char* text = parser->InputBuffer()->GetString($1.start_pos, $1.str_len, FALSE);
  if (text)
    {
      if (text[0] == '-' && ($1.str_len == 1 || text[1] == '-' || uni_isdigit(text[1])) || parser->StrictMode() && uni_isdigit(text[0]))
	{
	  OP_DELETEA(text);
	  $$ = FALSE;
#ifdef CSS_ERROR_SUPPORT
	  parser->EmitErrorL(UNI_L("Invalid id selector"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
	  break;
	}

      CSS_Buffer::CopyAndRemoveEscapes(text, text, uni_strlen(text));
      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_ID, 0, text));
      if (sel_attr)
	$$ = parser->AddSelectorAttributeL(sel_attr);
      else
	{
	  OP_DELETEA(text);
	  YYOUTOFMEM;
	}
    }
  else
    YYOUTOFMEM;
}
| class { $$=$1; }
| attrib { $$=$1; }
| pseudo { $$=$1; }
| not_parm { $$=$1; }
;

not_parm
: ':' CSS_TOK_NOT_FUNC spaces selector_parm spaces ')'
{
  $$ = $4;
  if ($4)
    {
      CSS_SelectorAttribute* sel_attr = parser->GetCurrentSimpleSelector()->GetLastAttr();
      BOOL negated = sel_attr->IsNegated();
      if (!negated && (sel_attr->GetType() != CSS_SEL_ATTR_TYPE_PSEUDO_CLASS || !IsPseudoElement(sel_attr->GetPseudoClass())))
	{
	  sel_attr->SetNegated();
	}
      else
	{
	  $$ = FALSE;
#ifdef CSS_ERROR_SUPPORT
	  if (negated)
	    parser->EmitErrorL(UNI_L("Recursive :not"), OpConsoleEngine::Error);
	  else
	    parser->EmitErrorL(UNI_L("Pseudo elements not allowed within a :not"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
	}
    }
}
| ':' CSS_TOK_NOT_FUNC spaces type_selector spaces ')'
{
  $$ = $4;
  if ($4)
    parser->GetCurrentSimpleSelector()->GetLastAttr()->SetNegated();
}
;

class
: '.' CSS_TOK_IDENT
{
  $$ = parser->AddClassSelectorL($2.start_pos, $2.str_len);
}
;

type_selector
: ns_prefix '|' elm_type
{
  parser->ResetCurrentNameSpaceIdx();
  $$ = $1 && $3;
}
| '|' { parser->SetCurrentNameSpaceIdx(NS_IDX_DEFAULT); } elm_type
{
  parser->ResetCurrentNameSpaceIdx();
  $$ = $3;
}
| elm_type
{
  $$ = $1;
}
;

ns_prefix
: CSS_TOK_IDENT
{
  $$ = FALSE;
  uni_char* name = (uni_char*)g_memory_manager->GetTempBuf();
  int len = MIN($1.str_len, g_memory_manager->GetTempBufLen()-1);
  if (parser->InputBuffer()->GetString(name, $1.start_pos, len))
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
	$$ = TRUE;

      parser->SetCurrentNameSpaceIdx(ns_idx);
    }
}
| '*'
{
  parser->SetCurrentNameSpaceIdx(NS_IDX_ANY_NAMESPACE);
  $$ = TRUE;
}
;

elm_type
: CSS_TOK_IDENT
{
  int ns_idx = parser->GetCurrentNameSpaceIdx();
  int tag = Markup::HTE_UNKNOWN;

  if (ns_idx != NS_IDX_NOT_FOUND)
    {
      BOOL is_xml = parser->IsXml();

      if (ns_idx <= NS_IDX_ANY_NAMESPACE)
	    tag = parser->InputBuffer()->GetTag($1.start_pos, $1.str_len, NS_HTML, is_xml, is_xml);
      else
	    tag = parser->InputBuffer()->GetTag($1.start_pos, $1.str_len, g_ns_manager->GetNsTypeAt(ns_idx), is_xml, FALSE);
    }

  uni_char* text = NULL;
  if (tag == Markup::HTE_UNKNOWN)
    {
      text = parser->InputBuffer()->GetString($1.start_pos, $1.str_len);
      if (!text)
	YYOUTOFMEM;
    }

  CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_ELM, tag, text, ns_idx));
  if (sel_attr)
    $$ = parser->AddSelectorAttributeL(sel_attr);
  else
    {
      OP_DELETEA(text);
      YYOUTOFMEM;
    }
}
| '*'
{
  CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_ELM, Markup::HTE_ANY, NULL, parser->GetCurrentNameSpaceIdx()));
  if (sel_attr)
    $$ = parser->AddSelectorAttributeL(sel_attr);
  else
    YYOUTOFMEM;
}
;

element_name
: CSS_TOK_IDENT
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
	  tag = parser->InputBuffer()->GetTag($1.start_pos, $1.str_len, NS_HTML, is_xml, is_xml);
	else
	  tag = parser->InputBuffer()->GetTag($1.start_pos, $1.str_len, g_ns_manager->GetNsTypeAt(ns_idx), is_xml, FALSE);
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

  uni_char* text = parser->InputBuffer()->GetString($1.start_pos, $1.str_len);
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
| '*'
{
  CSS_SimpleSelector* new_sel = OP_NEW(CSS_SimpleSelector, (Markup::HTE_ANY));
  if (!new_sel)
    YYOUTOFMEM;
  int ns_idx = parser->GetCurrentNameSpaceIdx();

  new_sel->SetNameSpaceIdx(ns_idx);
  parser->SetCurrentSimpleSelector(new_sel);
}
;

attr_name
: CSS_TOK_IDENT '|' CSS_TOK_IDENT
{
  $$.start_pos = $3.start_pos;
  $$.str_len = $3.str_len;
  uni_char* name = (uni_char*)g_memory_manager->GetTempBuf();
  int len = MIN($1.str_len, g_memory_manager->GetTempBufLen()-1);
  if (parser->InputBuffer()->GetString(name, $1.start_pos, len))
    $$.ns_idx = parser->GetNameSpaceIdx(name);
  else
    $$.ns_idx = NS_IDX_NOT_FOUND;
}
| '*' '|' CSS_TOK_IDENT
{
  $$.start_pos = $3.start_pos;
  $$.str_len = $3.str_len;
  $$.ns_idx = NS_IDX_ANY_NAMESPACE;
}
| '|' CSS_TOK_IDENT
{
  $$.start_pos = $2.start_pos;
  $$.str_len = $2.str_len;
  $$.ns_idx = NS_IDX_DEFAULT;
}
| CSS_TOK_IDENT
{
  $$.start_pos = $1.start_pos;
  $$.str_len = $1.str_len;
  $$.ns_idx = NS_IDX_DEFAULT;
}
;

attrib
: '[' spaces attr_name spaces ']'
{
  if ($3.ns_idx == NS_IDX_NOT_FOUND)
    {
#ifdef CSS_ERROR_SUPPORT
      parser->EmitErrorL(UNI_L("Use of undeclared namespace"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
      parser->SetErrorOccurred(CSSParseStatus::NAMESPACE_ERROR);
      $$ = FALSE;
      break;
    }
  if (parser->IsXml())
    {
      uni_char* name_and_val = parser->InputBuffer()->GetString($3.start_pos, $3.str_len);
      if (name_and_val)
	{
	  CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_HTMLATTR_DEFINED, name_and_val, $3.ns_idx));
	  if (sel_attr)
	    $$ = parser->AddSelectorAttributeL(sel_attr);
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
      short attr_type = (short)parser->InputBuffer()->GetHtmlAttr($3.start_pos, $3.str_len);
      if (attr_type != Markup::HA_XML)
	{
	  CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_HTMLATTR_DEFINED, attr_type, (uni_char*)0, $3.ns_idx));
	  if (sel_attr)
	    $$ = parser->AddSelectorAttributeL(sel_attr);
	  else
	    YYOUTOFMEM;
	}
      else
	{
	  uni_char* name_and_val = parser->InputBuffer()->GetString($3.start_pos, $3.str_len);
	  if (name_and_val)
	    {
	      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_HTMLATTR_DEFINED, name_and_val, $3.ns_idx));
	      if (sel_attr)
		$$ = parser->AddSelectorAttributeL(sel_attr);
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
| '[' spaces attr_name spaces eq_op spaces id_or_str spaces ']'
{
  if ($3.ns_idx == NS_IDX_NOT_FOUND)
    {
#ifdef CSS_ERROR_SUPPORT
      parser->EmitErrorL(UNI_L("Use of undeclared namespace"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
      parser->SetErrorOccurred(CSSParseStatus::NAMESPACE_ERROR);
      $$ = FALSE;
      break;
    }
  if (parser->IsXml())
    {
      uni_char* name_and_val = parser->InputBuffer()->GetNameAndVal($3.start_pos,
								    $3.str_len,
								    $7.start_pos,
								    $7.str_len);
      if (name_and_val)
	{
	  CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, ($5, name_and_val, $3.ns_idx));
	  if (sel_attr)
	    $$ = parser->AddSelectorAttributeL(sel_attr);
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
      short attr_type = (short)parser->InputBuffer()->GetHtmlAttr($3.start_pos, $3.str_len);
      if (attr_type != Markup::HA_XML)
	{
	  uni_char* text = parser->InputBuffer()->GetString($7.start_pos, $7.str_len);
	  if (text)
	    {
	      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, ($5, attr_type, text, $3.ns_idx));
	      if (sel_attr)
		$$ = parser->AddSelectorAttributeL(sel_attr);
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
	  uni_char* name_and_val = parser->InputBuffer()->GetNameAndVal($3.start_pos,
									$3.str_len,
									$7.start_pos,
									$7.str_len);
	  if (name_and_val)
	    {
	      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, ($5, name_and_val, $3.ns_idx));
	      if (sel_attr)
		$$ = parser->AddSelectorAttributeL(sel_attr);
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
;

id_or_str
: CSS_TOK_IDENT { $$=$1; }
| CSS_TOK_STRING { $$=$1; }
;

eq_op
: '=' { $$ = CSS_SEL_ATTR_TYPE_HTMLATTR_EQUAL; }
| CSS_TOK_INCLUDES { $$ = CSS_SEL_ATTR_TYPE_HTMLATTR_INCLUDES; }
| CSS_TOK_DASHMATCH { $$ = CSS_SEL_ATTR_TYPE_HTMLATTR_DASHMATCH; }
| CSS_TOK_PREFIXMATCH { $$ = CSS_SEL_ATTR_TYPE_HTMLATTR_PREFIXMATCH; }
| CSS_TOK_SUFFIXMATCH { $$ = CSS_SEL_ATTR_TYPE_HTMLATTR_SUFFIXMATCH; }
| CSS_TOK_SUBSTRMATCH { $$ = CSS_SEL_ATTR_TYPE_HTMLATTR_SUBSTRMATCH; }
;

pseudo
: ':' CSS_TOK_IDENT
{
  unsigned short pseudo_class = parser->InputBuffer()->GetPseudoSymbol($2.start_pos, $2.str_len);
  if (pseudo_class != PSEUDO_CLASS_UNKNOWN &&
      pseudo_class != PSEUDO_CLASS_SELECTION &&
      pseudo_class != PSEUDO_CLASS_CUE)
    {
      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_PSEUDO_CLASS, pseudo_class));
      if (sel_attr)
	$$ = parser->AddSelectorAttributeL(sel_attr);
      else
	YYOUTOFMEM;
    }
  else
    {
#ifdef CSS_ERROR_SUPPORT
      parser->EmitErrorL(UNI_L("Unknown pseudo class"), OpConsoleEngine::Information);
#endif // CSS_ERROR_SUPPORT
      $$ = FALSE;
    }
}
| ':' ':' CSS_TOK_IDENT
{
  unsigned short pseudo_class = parser->InputBuffer()->GetPseudoSymbol($3.start_pos, $3.str_len);
  if (IsPseudoElement(pseudo_class))
    {
      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_PSEUDO_CLASS, pseudo_class));
      if (sel_attr)
	$$ = parser->AddSelectorAttributeL(sel_attr);
      else
	YYOUTOFMEM;
    }
  else
    {
#ifdef CSS_ERROR_SUPPORT
      parser->EmitErrorL(UNI_L("\"::\" not followed by known pseudo element"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
      $$ = FALSE;
    }
}
| ':' ':' CSS_TOK_CUE_FUNC spaces
{
#ifdef MEDIA_HTML_SUPPORT
  parser->BeginCueSelector();
#endif // MEDIA_HTML_SUPPORT
}
selectors ')'
{
#ifdef MEDIA_HTML_SUPPORT
  parser->EndCueSelector();

  CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_PSEUDO_CLASS, PSEUDO_CLASS_CUE));
  if (sel_attr)
    $$ = parser->AddSelectorAttributeL(sel_attr);
  else
    YYOUTOFMEM;

  $$ = ($$ && $6);
#else
  $$ = FALSE;
#endif // MEDIA_HTML_SUPPORT
}
| ':' CSS_TOK_LANG_FUNC spaces CSS_TOK_IDENT spaces ')'
{
  uni_char* text = parser->InputBuffer()->GetString($4.start_pos, $4.str_len);
  if (text)
    {
      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_PSEUDO_CLASS, PSEUDO_CLASS_LANG, text));
      if (sel_attr)
	$$ = parser->AddSelectorAttributeL(sel_attr);
      else
	YYOUTOFMEM;
    }
  else
    YYOUTOFMEM;
}
| ':' nth_func spaces nth_expr ')'
{
  if ($4.a == INT_MIN && $4.b == INT_MIN)
    $$ = FALSE;
  else
    {
      CSS_SelectorAttribute* sel_attr = OP_NEW(CSS_SelectorAttribute, ($2, $4.a, $4.b));
      if (sel_attr)
	$$ = parser->AddSelectorAttributeL(sel_attr);
      else
	YYOUTOFMEM;
    }
}
;

nth_func
: CSS_TOK_NTH_CHILD_FUNC { $$ = CSS_SEL_ATTR_TYPE_NTH_CHILD; }
| CSS_TOK_NTH_OF_TYPE_FUNC { $$ = CSS_SEL_ATTR_TYPE_NTH_OF_TYPE; }
| CSS_TOK_NTH_LAST_CHILD_FUNC { $$ = CSS_SEL_ATTR_TYPE_NTH_LAST_CHILD; }
| CSS_TOK_NTH_LAST_OF_TYPE_FUNC { $$ = CSS_SEL_ATTR_TYPE_NTH_LAST_OF_TYPE; }
;

nth_expr
: CSS_TOK_N spaces unary_operator spaces CSS_TOK_INTEGER spaces
{
  $$.a = $1.integer;
  $$.b = $3.integer * $5.integer;
}
| unary_operator CSS_TOK_N spaces unary_operator spaces CSS_TOK_INTEGER spaces
{
  $$.a = $1.integer * $2.integer;
  $$.b = $4.integer * $6.integer;
}
| CSS_TOK_N spaces
{
  $$.a = $1.integer;
  $$.b = 0;
}
| unary_operator CSS_TOK_N spaces
{
  $$.a = $1.integer * $2.integer;
  $$.b = 0;
}
| CSS_TOK_INTEGER spaces
{
  $$.a = 0;
  $$.b = $1.integer;
}
| unary_operator CSS_TOK_INTEGER spaces
{
  $$.a = 0;
  $$.b = $1.integer * $2.integer;
}
| CSS_TOK_IDENT spaces
{
  $$.a = $$.b = INT_MIN;
  int len = $1.str_len;
  if (len == 3 || len == 4)
    {
      uni_char ident[8]; /* ARRAY OK 2009-02-12 rune */
      parser->InputBuffer()->GetString(ident, $1.start_pos, $1.str_len);
      if (len == 3 && uni_stri_eq(ident, "ODD"))
	{
	  $$.a = 2;
	  $$.b = 1;
	}
      else if (len == 4 && uni_stri_eq(ident, "EVEN"))
	{
	  $$.a = 2;
	  $$.b = 0;
	}
    }
}
;

prio
: CSS_TOK_IMPORTANT_SYM spaces { $$ = TRUE; }
| /* empty */ { $$ = FALSE; }
;

expr
: term
{
  if ($1.token == CSS_FUNCTION_END)
    parser->CommitFunctionValuesL();

  if ($1.token)
    parser->AddValueL($1);
}
| expr operator term
{
  if ($2.token)
    parser->AddValueL($2);

  if ($3.token == CSS_FUNCTION_END)
    parser->CommitFunctionValuesL();

  if ($3.token)
    parser->AddValueL($3);
}
;

function_expr
: term
{
  if ($1.token)
    parser->AddFunctionValueL($1);
}
| function_expr operator term
{
  if ($2.token)
    parser->AddFunctionValueL($2);
  if ($3.token)
    parser->AddFunctionValueL($3);
}
;

term
: unary_operator num_term
{
  $$ = $2;
  if ($$.token == CSS_INT_NUMBER)
    $$.value.integer.integer *= $1.integer;
  else
    $$.value.number.number *= $1.integer;
}
| num_term { $$=$1; }
| CSS_TOK_STRING spaces
{
  $$.token = CSS_STRING_LITERAL;
  $$.value.str.start_pos = $1.start_pos;
  $$.value.str.str_len = $1.str_len;
}
| CSS_TOK_IDENT spaces
{
  $$.token = CSS_IDENT;
  $$.value.str.start_pos = $1.start_pos;
  $$.value.str.str_len = $1.str_len;
}
| CSS_TOK_URI spaces
{
  $$.token = CSS_FUNCTION_URL;
  $$.value.str.start_pos = $1.start_pos;
  $$.value.str.str_len = $1.str_len;
}
| CSS_TOK_UNICODERANGE spaces
{
  $$.token = -1; /* what do we put here? */
}
| hash
{
  $$.token = CSS_HASH;
  $$.value.str.start_pos = $1.start_pos;
  $$.value.str.str_len = $1.str_len;
}
| CSS_TOK_RGB_FUNC spaces percentage spaces ',' spaces percentage spaces ',' spaces percentage spaces ')' spaces
{
  $$.token = CSS_FUNCTION_RGB;

  if ($3.number < 0.0)
    $3.number = 0.0;
  else if ($3.number > 100.0)
    $3.number = 100.0;
  if ($7.number < 0.0)
    $7.number = 0.0;
  else if ($7.number > 100.0)
    $7.number = 100.0;
  if ($11.number < 0.0)
    $11.number = 0.0;
  else if ($11.number > 100.0)
    $11.number = 100.0;

  $$.value.color = OP_RGB((int)OpRound(($3.number*255)/100), (int)OpRound(($7.number*255)/100), (int)OpRound(($11.number*255)/100));
}
| CSS_TOK_RGB_FUNC spaces integer spaces ',' spaces integer spaces ',' spaces integer spaces ')' spaces
{
  $$.token = CSS_FUNCTION_RGB;

  if ($3.integer < 0)
    $3.integer = 0;
  else if ($3.integer > 255)
    $3.integer = 255;
  if ($7.integer < 0)
    $7.integer = 0;
  else if ($7.integer > 255)
    $7.integer = 255;
  if ($11.integer < 0)
    $11.integer = 0;
  else if ($11.integer > 255)
    $11.integer = 255;

  $$.value.color = OP_RGB($3.integer, $7.integer, $11.integer);
}
| CSS_TOK_RGBA_FUNC spaces percentage spaces ',' spaces percentage spaces ',' spaces percentage spaces ',' spaces number spaces ')' spaces
{
  $$.token = CSS_FUNCTION_RGBA;

  if ($3.number < 0.0)
    $3.number = 0.0;
  else if ($3.number > 100.0)
    $3.number = 100.0;
  if ($7.number < 0.0)
    $7.number = 0.0;
  else if ($7.number > 100.0)
    $7.number = 100.0;
  if ($11.number < 0.0)
    $11.number = 0.0;
  else if ($11.number > 100.0)
    $11.number = 100.0;
  if ($15.number < 0.0)
    $15.number = 0.0;
  else if ($15.number > 1.0)
    $15.number = 1.0;

  $$.value.color = OP_RGBA((int)OpRound(($3.number*255)/100), (int)OpRound(($7.number*255)/100), (int)OpRound(($11.number*255)/100), (int)OpRound($15.number*255));
}
| CSS_TOK_RGBA_FUNC spaces integer spaces ',' spaces integer spaces ',' spaces integer spaces ',' spaces number spaces ')' spaces
{
  $$.token = CSS_FUNCTION_RGBA;

  if ($3.integer < 0)
    $3.integer = 0;
  else if ($3.integer > 255)
    $3.integer = 255;
  if ($7.integer < 0)
    $7.integer = 0;
  else if ($7.integer > 255)
    $7.integer = 255;
  if ($11.integer < 0)
    $11.integer = 0;
  else if ($11.integer > 255)
    $11.integer = 255;
  if ($15.number < 0.0)
    $15.number = 0.0;
  else if ($15.number > 1.0)
    $15.number = 1.0;

  $$.value.color = OP_RGBA($3.integer, $7.integer, $11.integer, (int)OpRound($15.number*255));
}
| CSS_TOK_HSL_FUNC spaces number spaces ',' spaces percentage spaces ',' spaces percentage spaces ')' spaces
{
  $$.token = CSS_FUNCTION_RGB;
  $$.value.color = HSLA_TO_RGBA($3.number, $7.number, $11.number);
}
| CSS_TOK_HSLA_FUNC spaces number spaces ',' spaces percentage spaces ',' spaces percentage spaces ',' spaces number spaces ')' spaces
{
  $$.token = CSS_FUNCTION_RGBA;
  if ($15.number < 0.0)
    $15.number = 0.0;
  else if ($15.number > 1.0)
    $15.number = 1.0;
  $$.value.color = HSLA_TO_RGBA($3.number, $7.number, $11.number, $15.number);
}
| CSS_TOK_LINEAR_GRADIENT_FUNC { PropertyValue val; val.token = CSS_FUNCTION_LINEAR_GRADIENT; val.value.str.str_len = 0; parser->AddFunctionValueL(val); } spaces function_expr ')' spaces
{
  $$.token = CSS_FUNCTION_END;
}
| CSS_TOK_WEBKIT_LINEAR_GRADIENT_FUNC { PropertyValue val; val.token = CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT; val.value.str.str_len = 0; parser->AddFunctionValueL(val); } spaces function_expr ')' spaces
{
  $$.token = CSS_FUNCTION_END;
}
| CSS_TOK_O_LINEAR_GRADIENT_FUNC { PropertyValue val; val.token = CSS_FUNCTION_O_LINEAR_GRADIENT; val.value.str.str_len = 0; parser->AddFunctionValueL(val); } spaces function_expr ')' spaces
{
  $$.token = CSS_FUNCTION_END;
}
| CSS_TOK_REPEATING_LINEAR_GRADIENT_FUNC { PropertyValue val; val.token = CSS_FUNCTION_REPEATING_LINEAR_GRADIENT; val.value.str.str_len = 0; parser->AddFunctionValueL(val); } spaces function_expr ')' spaces
{
  $$.token = CSS_FUNCTION_END;
}
| CSS_TOK_RADIAL_GRADIENT_FUNC { PropertyValue val; val.token = CSS_FUNCTION_RADIAL_GRADIENT; val.value.str.str_len = 0; parser->AddFunctionValueL(val); } spaces function_expr ')' spaces
{
  $$.token = CSS_FUNCTION_END;
}
| CSS_TOK_REPEATING_RADIAL_GRADIENT_FUNC { PropertyValue val; val.token = CSS_FUNCTION_REPEATING_RADIAL_GRADIENT; val.value.str.str_len = 0; parser->AddFunctionValueL(val); } spaces function_expr ')' spaces
{
  $$.token = CSS_FUNCTION_END;
}
| CSS_TOK_DOUBLE_RAINBOW_FUNC { PropertyValue val; val.token = CSS_FUNCTION_DOUBLE_RAINBOW; val.value.str.str_len = 0; parser->AddFunctionValueL(val); } spaces function_expr ')' spaces
{
  $$.token = CSS_FUNCTION_END;
}
| CSS_TOK_ATTR_FUNC spaces CSS_TOK_IDENT spaces ')' spaces
{
  $$.token = CSS_FUNCTION_ATTR;
  $$.value.str.start_pos = $3.start_pos;
  $$.value.str.str_len = $3.str_len;
}
| CSS_TOK_COUNTER_FUNC spaces CSS_TOK_IDENT spaces ')' spaces
{
  $$.token = CSS_FUNCTION_COUNTER;
  $$.value.str.start_pos = $3.start_pos;
  $$.value.str.str_len = $3.str_len;
}
| CSS_TOK_COUNTER_FUNC spaces CSS_TOK_IDENT spaces ',' spaces CSS_TOK_IDENT spaces ')' spaces
{
  $$.token = CSS_FUNCTION_COUNTER;
  $$.value.str.start_pos = $3.start_pos;
  $$.value.str.str_len = $7.start_pos - $3.start_pos + $7.str_len;
}
| CSS_TOK_COUNTERS_FUNC spaces CSS_TOK_IDENT spaces ',' spaces CSS_TOK_STRING spaces ')' spaces
{
  $$.token = CSS_FUNCTION_COUNTERS;
  $$.value.str.start_pos = $3.start_pos;
  $$.value.str.str_len = $7.start_pos - $3.start_pos + $7.str_len + 1;
}
| CSS_TOK_COUNTERS_FUNC spaces CSS_TOK_IDENT spaces ',' spaces CSS_TOK_STRING spaces ',' spaces CSS_TOK_IDENT spaces ')' spaces
{
  $$.token = CSS_FUNCTION_COUNTERS;
  $$.value.str.start_pos = $3.start_pos;
  $$.value.str.str_len = $11.start_pos - $3.start_pos + $11.str_len;
}
| CSS_TOK_RECT_FUNC spaces term term term term ')' spaces
{
  PropertyValue val;
  val.token = CSS_FUNCTION_RECT;
  parser->AddValueL(val);
  parser->AddValueL($3);
  parser->AddValueL($4);
  parser->AddValueL($5);
  parser->AddValueL($6);
  $$.token = CSS_FUNCTION_RECT;
}
| CSS_TOK_RECT_FUNC spaces term ',' spaces term ',' spaces term ',' spaces term ')' spaces
{
  PropertyValue val;
  val.token = CSS_FUNCTION_RECT;
  parser->AddValueL(val);
  parser->AddValueL($3);
  parser->AddValueL($6);
  parser->AddValueL($9);
  parser->AddValueL($12);
  $$.token = CSS_FUNCTION_RECT;
}
| CSS_TOK_SKIN_FUNC spaces CSS_TOK_STRING spaces ')' spaces
{
  $$.token = CSS_FUNCTION_SKIN;
  $$.value.str.start_pos = $3.start_pos;
  $$.value.str.str_len = $3.str_len;
}
| CSS_TOK_DASHBOARD_REGION_FUNC spaces CSS_TOK_IDENT spaces CSS_TOK_IDENT spaces region_offsets ')' spaces
{
#ifdef GADGET_SUPPORT
  PropertyValue val;
  val.token = CSS_IDENT;
  val.value.str.start_pos = $3.start_pos;
  val.value.str.str_len = $3.str_len;
  parser->AddValueL(val);
  val.value.str.start_pos = $5.start_pos;
  val.value.str.str_len = $5.str_len;
  parser->AddValueL(val);
#endif // GADGET_SUPPORT
  $$.token = 0;
}
| CSS_TOK_LANGUAGE_STRING_FUNC spaces integer spaces ')' spaces
{
  $$.token = CSS_FUNCTION_LANGUAGE_STRING;
  $$.value.integer.integer = $3.integer;
}
| CSS_TOK_LANGUAGE_STRING_FUNC spaces CSS_TOK_IDENT spaces ')' spaces
{
  $$.token = CSS_FUNCTION_LANGUAGE_STRING;
#ifdef LOCALE_SET_FROM_STRING
  uni_char* locale_string = parser->InputBuffer()->GetString($3.start_pos, $3.str_len);
  if (locale_string)
  {
    $$.value.integer.integer = Str::LocaleString(locale_string);
    OP_DELETEA(locale_string);
  }
  else
    YYOUTOFMEM;
#else
  $$.value.integer.integer = 0;
#endif // LOCALE_SET_FROM_STRING
}
| CSS_TOK_LOCAL_FUNC spaces { PropertyValue val; val.token = CSS_FUNCTION_LOCAL_START; val.value.str.str_len = 0; parser->AddFunctionValueL(val); } function_expr ')' spaces
{
  $$.token = CSS_FUNCTION_END;
}
| CSS_TOK_FORMAT_FUNC spaces format_list ')' spaces
{
  $$.token = CSS_FUNCTION_FORMAT;
  $$.value.shortint = $3;
}
| CSS_TOK_CUBIC_BEZ_FUNC spaces number spaces ',' spaces number spaces ',' spaces number spaces ',' spaces number spaces ')' spaces
{
  PropertyValue val;
  val.token = CSS_FUNCTION_CUBIC_BEZ;
  parser->AddValueL(val);
  val.token = CSS_NUMBER;
  val.value.number.number = $3.number;
  parser->AddValueL(val);
  val.value.number.number = $7.number;
  parser->AddValueL(val);
  val.value.number.number = $11.number;
  parser->AddValueL(val);
  val.value.number.number = $15.number;
  parser->AddValueL(val);
  $$.token = 0;
}
| CSS_TOK_INTEGER_FUNC spaces integer spaces ')' spaces
{
  $$.token = CSS_FUNCTION_INTEGER;
  $$.value.integer.integer = $3.integer;
}
| CSS_TOK_STEPS_FUNC spaces integer spaces steps_start ')' spaces
{
  if ($5.value.steps.steps > 0)
    {
      $$ = $5;
      OP_ASSERT($5.token == CSS_FUNCTION_STEPS);
      $$.value.steps.steps = $3.integer;
    }
  else
    $$.token = 0;
}
| block spaces { $$.token = -1; }
| CSS_TOK_ATKEYWORD spaces { $$.token = -1; }
| '*' spaces { $$.token = CSS_ASTERISK_CHAR; }
| '#' spaces { $$.token = CSS_HASH_CHAR; }
| transform_name transform_arg_list ')' spaces { $$.token = 0; }
;

steps_start
: ',' spaces CSS_TOK_IDENT spaces
{
  $$.token = CSS_FUNCTION_STEPS;
  $$.value.steps.steps = 1;
  short keyword = parser->InputBuffer()->GetValueSymbol($3.start_pos, $3.str_len);
  if (keyword == CSS_VALUE_start)
    $$.value.steps.start = TRUE;
  else if (keyword == CSS_VALUE_end)
    $$.value.steps.start = FALSE;
  else
    $$.value.steps.steps = 0;
}
| /* empty */
{
  $$.token = CSS_FUNCTION_STEPS;
  $$.value.steps.steps = 1;
  $$.value.steps.start = FALSE;
}
;

transform_name
: CSS_TOK_TRANSFORM_FUNC
{
  PropertyValue val;
  val.token = $1;
  parser->AddValueL(val);
}
;

transform_arg_list
: spaces transform_arg
| transform_arg_list ',' spaces transform_arg;

transform_arg
: unary_operator num_term
{
  $2.value.number.number*=$1.integer;
  parser->AddValueL($2);
}
| num_term
{
  parser->AddValueL($1);
}
;

format_list
: id_or_str spaces
{
  $$ = parser->InputBuffer()->GetFontFormat($1.start_pos, $1.str_len);
}
| format_list ',' spaces id_or_str spaces
{
  $$ = $1 | parser->InputBuffer()->GetFontFormat($4.start_pos, $4.str_len);
}
;

region_offsets
: /* empty */
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
| num_term num_term num_term num_term
{
#ifdef GADGET_SUPPORT
  PropertyValue val;
  val.token = CSS_FUNCTION_DASHBOARD_REGION;
  parser->AddValueL(val);
  parser->AddValueL($1);
  parser->AddValueL($2);
  parser->AddValueL($3);
  parser->AddValueL($4);
#endif // GADGET_SUPPORT
}
;

integer
: unary_operator CSS_TOK_INTEGER
{
  $$.start_pos = $2.start_pos;
  $$.str_len = $2.str_len;
  $$.integer = $1.integer * $2.integer;
}
| CSS_TOK_INTEGER
{
  $$.start_pos = $1.start_pos;
  $$.str_len = $1.str_len;
  $$.integer = static_cast<int>($1.integer);
}
;

percentage
: unary_operator CSS_TOK_PERCENTAGE { $$ = $2; $$.number *= $1.integer; }
| CSS_TOK_PERCENTAGE { $$ = $1; }
;

number
: unary_operator CSS_TOK_NUMBER { $$ = $2; $$.number *= $1.integer; }
| CSS_TOK_NUMBER { $$ = $1; }
| integer
{
  $$.start_pos = $1.start_pos;
  $$.str_len = $1.str_len;
  $$.number = double($1.integer);
}
;

num_term
: CSS_TOK_NUMBER spaces { $$.token=CSS_NUMBER; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_INTEGER spaces
{
  if (parser->UseIntegerTokens())
    {
      $$.token = CSS_INT_NUMBER;
      $$.value.integer.integer = MIN($1.integer, INT_MAX);
      $$.value.integer.start_pos = $1.start_pos;
      $$.value.integer.str_len = $1.str_len;
    }
  else
    {
      $$.token = CSS_NUMBER;
      $$.value.number.number = double($1.integer);
      $$.value.number.start_pos = $1.start_pos;
      $$.value.number.str_len = $1.str_len;
    }
}
| CSS_TOK_PERCENTAGE spaces { $$.token=CSS_PERCENTAGE; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_LENGTH_PX spaces { $$.token=CSS_PX; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_LENGTH_CM spaces { $$.token=CSS_CM; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_LENGTH_MM spaces { $$.token=CSS_MM; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_LENGTH_IN spaces { $$.token=CSS_IN; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_LENGTH_PT spaces { $$.token=CSS_PT; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_LENGTH_PC spaces { $$.token=CSS_PC; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_EMS spaces { $$.token=CSS_EM; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_REMS spaces { $$.token=CSS_REM; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_EXS spaces { $$.token=CSS_EX; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_ANGLE spaces { $$.token=CSS_RAD; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_TIME spaces { $$.token=CSS_SECOND; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_FREQ spaces { $$.token=CSS_HZ; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_DIMEN spaces { $$.token=CSS_DIMEN; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_DPI spaces { $$.token=CSS_DPI; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_DPCM spaces { $$.token=CSS_DPCM; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| CSS_TOK_DPPX spaces { $$.token=CSS_DPPX; $$.value.number.number=$1.number; $$.value.number.start_pos = $1.start_pos; $$.value.number.str_len = $1.str_len; }
| function {
#ifdef CSS_ERROR_SUPPORT
  parser->EmitErrorL(UNI_L("Unrecognized function"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
  $$.token = CSS_FUNCTION;
}
;

function
: CSS_TOK_FUNCTION spaces expr_ignore ')' spaces {}
;

expr_ignore
: term {}
| expr_ignore operator term {}
;

hash
: CSS_TOK_HASH spaces
{
  $$.start_pos = $1.start_pos;
  $$.str_len = $1.str_len;
}
;

%%

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
