/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2012
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_parser.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_expr.h"
#include "modules/ecmascript/carakan/src/compiler/es_parser_inlines.h"


#ifdef _DEBUG
static bool DebugFalse()
{
  return false;
}

#define DEBUG_FALSE DebugFalse()
#else // _DEBUG
#define DEBUG_FALSE false
#endif // _DEBUG


#define PARSE_FAILED(code) do { automatic_error_code = code; return FALSE; } while (0)


static JString *
ValueAsString (ES_Context *context, ES_Value_Internal &value)
{
  if (value.IsString ())
    return value.GetString ();
  else if (value.IsNumber ())
    return tostring (context, value.GetNumAsDouble ());
  else if (value.IsBoolean ())
    return value.GetBoolean () ? context->rt_data->strings[STRING_true] : context->rt_data->strings[STRING_false];
  else if (value.IsNull ())
    return context->rt_data->strings[STRING_null];
  else
    return context->rt_data->strings[STRING_undefined];
}


#ifdef USE_CUSTOM_DOUBLE_OPS
# define ADD_DOUBLES(a, b) AddDoubles(a, b)
# define SUB_DOUBLES(a, b) SubtractDoubles(a, b)
# define MUL_DOUBLES(a, b) MultiplyDoubles(a, b)
# define DIV_DOUBLES(a, b) DivideDoubles(a, b)
#else // USE_CUSTOM_DOUBLE_OPS
# define ADD_DOUBLES(a, b) ((a) + (b))
# define SUB_DOUBLES(a, b) ((a) - (b))
# define MUL_DOUBLES(a, b) ((a) * (b))
# define DIV_DOUBLES(a, b) ((a) / (b))
#endif // USE_CUSTOM_DOUBLE_OPS


bool
ES_Parser::EvaluateConstantBinaryExpression (unsigned type, ES_Expression *left, ES_Expression *right)
{
  if (left->GetType () == ES_Expression::TYPE_LITERAL && right->GetType () == ES_Expression::TYPE_LITERAL)
    {
      ES_Value_Internal vleft = static_cast<ES_LiteralExpr *> (left)->GetValue ();
      ES_Value_Internal vright = static_cast<ES_LiteralExpr *> (right)->GetValue ();

      if (type >= ES_Expression::TYPE_SHIFT_LEFT && type <= ES_Expression::TYPE_SHIFT_SIGNED_RIGHT ||
          type >= ES_Expression::TYPE_BITWISE_AND && type <= ES_Expression::TYPE_BITWISE_OR)
        {
          int ileft = vleft.AsNumber (context).GetNumAsInt32 ();
          int iright = vright.AsNumber (context).GetNumAsInt32 ();
          int result;

          switch (type)
            {
            case ES_Expression::TYPE_SHIFT_LEFT:
              result = ileft << (iright & 31);
              break;

            case ES_Expression::TYPE_SHIFT_SIGNED_RIGHT:
              result = ileft >> (iright & 31);
              break;

            case ES_Expression::TYPE_BITWISE_AND:
              result = ileft & iright;
              break;

            case ES_Expression::TYPE_BITWISE_XOR:
              result = ileft ^ iright;
              break;

            case ES_Expression::TYPE_BITWISE_OR:
              result = ileft | iright;
              break;

            default:
              OP_ASSERT(FALSE);
              result = 0;
            }

          PushExpression (OP_NEWGRO_L (ES_LiteralExpr, (result), Arena ()));
          return true;
        }
      else if (type == ES_Expression::TYPE_SHIFT_UNSIGNED_RIGHT)
        {
          ES_Value_Internal result;
          result.SetUInt32 (vleft.AsNumber (context).GetNumAsUInt32 () >> (vright.AsNumber (context).GetNumAsUInt32 () & 31u));
          PushExpression (OP_NEWGRO_L (ES_LiteralExpr, (result), Arena ()));
          return true;
        }
      else if (type >= ES_Expression::TYPE_MULTIPLY && type <= ES_Expression::TYPE_SUBTRACT)
        {
        numeric:
          double dleft = vleft.AsNumber (context).GetNumAsDouble ();
          double dright = vright.AsNumber (context).GetNumAsDouble ();
          double result;

          switch (type)
            {
            case ES_Expression::TYPE_MULTIPLY:
              result = MUL_DOUBLES (dleft, dright);
              break;

            case ES_Expression::TYPE_DIVIDE:
              result = DIV_DOUBLES (dleft, dright);
              break;

            case ES_Expression::TYPE_REMAINDER:
              if (op_isnan (dleft) || op_isnan (dright) || op_isinf (dleft) || dright == 0)
                result = op_nan (NULL);
              else if (dleft == 0 || op_isinf (dright))
                result = dleft;
              else
                result = op_fmod (dleft, dright);
              break;

            case ES_Expression::TYPE_SUBTRACT:
              result = SUB_DOUBLES (dleft, dright);
              break;

            case ES_Expression::TYPE_ADD:
              result = ADD_DOUBLES (dleft, dright);
              break;

            default:
               OP_ASSERT(FALSE);
               result = 0;
            }

          PushExpression (OP_NEWGRO_L (ES_LiteralExpr, (result), Arena ()));
          return true;
        }
      else if (type == ES_Expression::TYPE_ADD)
        if (vleft.IsString() || vright.IsString())
          {
            JString *sleft = ValueAsString (context, vleft);
            JString *sright = ValueAsString (context, vright);
            JString *result;

            if (Length (sleft) == 0)
              result = sright;
            else if (Length (sright) == 0)
              result = sleft;
            else
              {
                result = Share (context, sleft);
                Append (context, result, sright);
              }

            PushExpression (OP_NEWGRO_L (ES_LiteralExpr, (result), Arena ()));
            return true;
          }
        else
          goto numeric;
    }

  if (left->GetType () == ES_Expression::TYPE_LITERAL && (type == ES_Expression::TYPE_LOGICAL_OR || type == ES_Expression::TYPE_LOGICAL_AND))
    {
      if (static_cast<ES_LiteralExpr *> (left)->GetValue ().AsBoolean ().GetBoolean () ? type == ES_Expression::TYPE_LOGICAL_OR : type == ES_Expression::TYPE_LOGICAL_AND)
        PushExpression (left);
      else
        PushExpression (right);

      return true;
    }

  return false;
}


static ES_LogicalExpr *
MakeLogicalExpr (unsigned type, ES_Expression *left, ES_Expression *right, OpMemGroup *arena)
{
  if (right->GetType() == static_cast<ES_Expression::Type> (type))
    {
      left = MakeLogicalExpr (type, left, static_cast<ES_LogicalExpr *> (right)->GetLeft (), arena);
      right = static_cast<ES_LogicalExpr *> (right)->GetRight ();
    }

  return OP_NEWGRO_L (ES_LogicalExpr, (static_cast<ES_LogicalExpr::Type> (type), left, right), arena);
}


bool
ES_Parser::ParseLogicalExpr (unsigned &depth, unsigned rhs_production, bool allow_in, unsigned type)
{
  ES_Expression *left;
  ES_Expression *right;

  if (!ParseExpression (depth, rhs_production, allow_in, expression_stack_used))
    return DEBUG_FALSE;

  right = PopExpression ();
  left = PopExpression ();

  if (in_typeof || !EvaluateConstantBinaryExpression (type, left, right))
    PushExpression (MakeLogicalExpr (type, left, right, Arena ()));
  else if (!in_typeof)
    depth--;

  return true;
}


bool
ES_Parser::ParseBitwiseExpr (unsigned &depth, unsigned rhs_production, bool allow_in, unsigned type)
{
  ES_Expression *left;
  ES_Expression *right;

  if (!ParseExpression (depth, rhs_production, allow_in, expression_stack_used))
    return DEBUG_FALSE;

  right = PopExpression ();
  left = PopExpression ();

  if (!EvaluateConstantBinaryExpression (type, left, right))
    {
      /* Convert literal operands into int32, since we expect int32 operands and
         would otherwise just convert them runtime every time the expression is
         evaluated. */
      if (left->GetType () == ES_Expression::TYPE_LITERAL && left->GetValueType () != ESTYPE_INT32)
        static_cast<ES_LiteralExpr *> (left)->GetValue ().SetInt32 (static_cast<ES_LiteralExpr *> (left)->GetValue ().AsNumber (context).GetNumAsInt32 ());
      if (right->GetType () == ES_Expression::TYPE_LITERAL && right->GetValueType () != ESTYPE_INT32)
        static_cast<ES_LiteralExpr *> (right)->GetValue ().SetInt32 (static_cast<ES_LiteralExpr *> (right)->GetValue ().AsNumber (context).GetNumAsInt32 ());

      PushExpression (OP_NEWGRO_L (ES_BitwiseExpr, (static_cast<ES_Expression::Type> (type), left, right), Arena ()));
    }
  else
    depth--;

  return true;
}


#define STORE_TOKEN_START() \
  unsigned index = last_token.start; \
  unsigned line = last_token.line;

#define LOCATION() , ES_SourceLocation(index, line, last_token.end - index)
#define LOCATION_FROM(expr) , ES_SourceLocation (expr->GetSourceLocation ().Index (), expr->GetSourceLocation ().Line (), last_token.end - expr->GetSourceLocation ().Index ())

bool
ES_Parser::ParseAccessor (AccessorType type, JString *name, unsigned check_count, unsigned index, unsigned line, unsigned column)
{
#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
  ES_SourceLocation name_location = last_token.GetSourceLocation();
# define NAME_LOCATION , name_location
#else // ES_LEXER_SOURCE_LOCATION_SUPPORT
# define NAME_LOCATION
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT

  unsigned identifiers_before = identifier_stack_used;
  unsigned functions_before = function_stack_used;
  unsigned function_data_before = function_data_stack_used;
  unsigned statements_before = statement_stack_used;
  unsigned old_function_data_index = function_data_index;
  ES_SourceLocation start_location, end_location;
  function_data_index = 0;
  in_function++;

  if (!ParsePunctuator (ES_Token::LEFT_PAREN) ||
      type == ACCESSOR_SET && !ParseFormalParameterList () ||
      !ParsePunctuator (ES_Token::RIGHT_PAREN) ||
      !ParseSourceElements (true, &start_location) ||
      !ParsePunctuator (ES_Token::RIGHT_BRACE, end_location))
    return DEBUG_FALSE;

  in_function--;

  unsigned parameter_names_count = identifier_stack_used - identifiers_before;
  unsigned functions_count = function_stack_used - functions_before;
  unsigned function_data_count = function_data_stack_used - function_data_before;
  unsigned statements_count = statement_stack_used - statements_before;
  function_data_index = old_function_data_index;

  is_simple_eval = FALSE;

  if (!CompileFunction (NULL, parameter_names_count, es_maxu (functions_count, function_data_count), statements_count, index, line, column, last_token.end, start_location, end_location, NULL, true))
    return DEBUG_FALSE;

  if (in_function || is_strict_eval)
    {
      if (!PushProperty (check_count, name NAME_LOCATION, OP_NEWGRO_L (ES_FunctionExpr, (function_data_index - 1), Arena ()), type == ACCESSOR_GET, type == ACCESSOR_SET))
        return DEBUG_FALSE;
    }
  else
    {
      if (!PushProperty (check_count, name NAME_LOCATION, OP_NEWGRO_L (ES_FunctionExpr, (function_stack[function_stack_used - 1]), Arena ()), type == ACCESSOR_GET, type == ACCESSOR_SET))
        return DEBUG_FALSE;
    }

#undef NAME_LOCATION

  return true;
}

bool
ES_Parser::ParseExpression (unsigned depth, unsigned production, bool allow_in, unsigned expr_stack_base, bool opt)
{
 recurse:

  if (++depth > ES_MAXIMUM_SYNTAX_TREE_DEPTH)
    PARSE_FAILED(INPUT_TOO_COMPLEX);

  unsigned expression_stack_length = expression_stack_used - expr_stack_base;

  JString *identifier;
  ES_Value_Internal value;
  ES_Expression *left;
  ES_Expression *right;

  /* These are always allowed and always unambigious. */
  if (expression_stack_length == 0)
    {
      if (ParseKeyword (ES_Token::KEYWORD_THIS))
        {
          PushExpression (OP_NEWGRO_L (ES_ThisExpr, (), Arena ()));
          goto recurse;
        }

      if (ParseIdentifier (identifier))
        {
          if (in_function && identifier == ident_arguments)
            uses_arguments = TRUE;
          else if (identifier == ident_eval)
            uses_eval = TRUE;

          PushExpression (OP_NEWGRO_L (ES_IdentifierExpr, (identifier), Arena ()));
          goto recurse;
        }

      bool regexp;

      if (ParseLiteral (value, regexp))
        {
          if (regexp)
            {
              ES_Expression *expr = OP_NEWGRO_L (ES_RegExpLiteralExpr, (token.regexp, token.regexp_source), Arena ());
              token.regexp = NULL;
              PushExpression (expr);
            }
          else
            PushExpression (OP_NEWGRO_L (ES_LiteralExpr, (value), Arena ()));
          goto recurse;
        }

      if (ParsePunctuator (ES_Token::LEFT_PAREN))
        {
          if (!ParseExpression (depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) || !ParsePunctuator (ES_Token::RIGHT_PAREN))
            return DEBUG_FALSE;

          goto recurse;
        }
    }

  ES_Expression::Production p = static_cast<ES_Expression::Production>(production);

  if (expression_stack_length == 0 && p < ES_Expression::PROD_UNARY_EXPR)
    p = ES_Expression::PROD_UNARY_EXPR;

  if (!HandleLinebreak ())
    return DEBUG_FALSE;

  switch (p)
    {
    case ES_Expression::PROD_EXPRESSION:
      if (ParsePunctuator (ES_Token::COMMA))
        {
          do
            if (!ParseExpression (depth, ES_Expression::PROD_ASSIGNMENT_EXPR, allow_in, expression_stack_used))
              return DEBUG_FALSE;
          while (ParsePunctuator (ES_Token::COMMA));

          unsigned expressions_count = expression_stack_used - expr_stack_base;
          ES_Expression **expressions = PopExpressions (expressions_count);

          PushExpression (OP_NEWGRO_L (ES_CommaExpr, (expressions, expressions_count), Arena ()));
          goto recurse;
        }

    case ES_Expression::PROD_ASSIGNMENT_EXPR:
      if (token.type == ES_Token::PUNCTUATOR)
        {
          bool is_assign = true;
          bool is_compound_assign = true;

          bool is_binary_number = false;
          bool is_additive = false;
          bool is_shift = false;

          ES_Expression::Type type = ES_Expression::TYPE_THIS;

          switch (token.punctuator)
            {
            case ES_Token::ASSIGN:
              is_compound_assign = false;
              break;

            case ES_Token::ADD_ASSIGN:
              is_additive = true;
              break;

            case ES_Token::MULTIPLY_ASSIGN:
              is_binary_number = true;
              type = ES_BinaryNumberExpr::TYPE_MULTIPLY;
              break;

            case ES_Token::DIVIDE_ASSIGN:
              is_binary_number = true;
              type = ES_BinaryNumberExpr::TYPE_DIVIDE;
              break;

            case ES_Token::REMAINDER_ASSIGN:
              is_binary_number = true;
              type = ES_BinaryNumberExpr::TYPE_REMAINDER;
              break;

            case ES_Token::SUBTRACT_ASSIGN:
              is_binary_number = true;
              type = ES_BinaryNumberExpr::TYPE_SUBTRACT;
              break;

            case ES_Token::SHIFT_LEFT_ASSIGN:
              is_shift = true;
              type = ES_ShiftExpr::TYPE_SHIFT_LEFT;
              break;

            case ES_Token::SHIFT_SIGNED_RIGHT_ASSIGN:
              is_shift = true;
              type = ES_ShiftExpr::TYPE_SHIFT_SIGNED_RIGHT;
              break;

            case ES_Token::SHIFT_UNSIGNED_RIGHT_ASSIGN:
              is_shift = true;
              type = ES_ShiftExpr::TYPE_SHIFT_UNSIGNED_RIGHT;
              break;

            case ES_Token::BITWISE_AND_ASSIGN:
              type = ES_BitwiseExpr::TYPE_BITWISE_AND;
              break;

            case ES_Token::BITWISE_OR_ASSIGN:
              type = ES_BitwiseExpr::TYPE_BITWISE_OR;
              break;

            case ES_Token::BITWISE_XOR_ASSIGN:
              type = ES_BitwiseExpr::TYPE_BITWISE_XOR;
              break;

            default:
              is_assign = false;
            }

          if (is_assign)
            {
              ES_Expression *target;
              ES_Expression *source;
              ES_BinaryExpr *compound_source;

              if (!NextToken ())
                return DEBUG_FALSE;

              target = PopExpression ();

              JString *name;

              if (target->IsIdentifier (name))
                if (!ValidateIdentifier (name, &target->GetSourceLocation ()))
                  return DEBUG_FALSE;

              JString *old_debug_name = current_debug_name;
              ES_Expression *old_debug_name_expr = current_debug_name_expr;

              current_debug_name = NULL;
              current_debug_name_expr = NULL;

              if (token.type == ES_Token::KEYWORD && token.keyword == ES_Token::KEYWORD_FUNCTION)
                if (target->GetType () == ES_Expression::TYPE_IDENTIFIER || target->GetType () == ES_Expression::TYPE_PROPERTY_REFERENCE)
                  current_debug_name_expr = target;

              if (!ParseExpression (depth, ES_Expression::PROD_ASSIGNMENT_EXPR, allow_in, expression_stack_used))
                return DEBUG_FALSE;

              current_debug_name = old_debug_name;
              current_debug_name_expr = old_debug_name_expr;

              source = PopExpression ();

/* It's apparently wrong to report this error compile time, although
   the specification explicitly allows it. */
#if 0
              if (!target->IsLValue ())
                {
                  error_index = target->GetSourceLocation ().Index ();
                  error_line = target->GetSourceLocation ().Line ();

                  PARSE_FAILED (EXPECTED_LVALUE);
                }
#endif // 0

              if (!is_compound_assign)
                {
                  PushExpression (OP_NEWGRO_L (ES_AssignExpr, (target, source), Arena ()));
                  goto recurse;
                }
              else if (is_binary_number)
                compound_source = OP_NEWGRO_L (ES_BinaryNumberExpr, (type, target, source), Arena ());
              else if (is_additive)
                compound_source = OP_NEWGRO_L (ES_AddExpr, (ES_AddExpr::UNKNOWN, target, source), Arena ());
              else if (is_shift)
                compound_source = OP_NEWGRO_L (ES_ShiftExpr, (type, target, source), Arena ());
              else
                compound_source = OP_NEWGRO_L (ES_BitwiseExpr, (type, target, source), Arena ());

              compound_source->SetIsCompoundAssign ();

              PushExpression (OP_NEWGRO_L (ES_AssignExpr, (0, compound_source), Arena ()));
              goto recurse;
            }
        }

    case ES_Expression::PROD_CONDITIONAL_EXPR:
      if (ParsePunctuator (ES_Token::CONDITIONAL_TRUE))
        {
          ES_Expression *condition;
          ES_Expression *first;
          ES_Expression *second;

          if (!ParseExpression (depth, ES_Expression::PROD_ASSIGNMENT_EXPR, true, expression_stack_used) ||
              !ParsePunctuator (ES_Token::CONDITIONAL_FALSE) ||
              !ParseExpression (depth, ES_Expression::PROD_ASSIGNMENT_EXPR, allow_in, expression_stack_used))
            return DEBUG_FALSE;

          second = PopExpression ();
          first = PopExpression ();
          condition = PopExpression ();

          PushExpression (OP_NEWGRO_L (ES_ConditionalExpr, (condition, first, second), Arena ()));
          goto recurse;
        }

    case ES_Expression::PROD_LOGICAL_OR_EXPR:
      if (ParsePunctuator (ES_Token::LOGICAL_OR))
        if (ParseLogicalExpr (depth, ES_Expression::PROD_LOGICAL_AND_EXPR, allow_in, ES_LogicalExpr::TYPE_LOGICAL_OR))
          goto recurse;
        else
          return DEBUG_FALSE;

    case ES_Expression::PROD_LOGICAL_AND_EXPR:
      if (ParsePunctuator (ES_Token::LOGICAL_AND))
        if (ParseLogicalExpr (depth, ES_Expression::PROD_BITWISE_OR_EXPR, allow_in, ES_LogicalExpr::TYPE_LOGICAL_AND))
          goto recurse;
        else
          return DEBUG_FALSE;

    case ES_Expression::PROD_BITWISE_OR_EXPR:
      if (ParsePunctuator (ES_Token::BITWISE_OR))
        if (ParseBitwiseExpr (depth, ES_Expression::PROD_BITWISE_XOR_EXPR, allow_in, ES_BitwiseExpr::TYPE_BITWISE_OR))
          goto recurse;
        else
          return DEBUG_FALSE;

    case ES_Expression::PROD_BITWISE_XOR_EXPR:
      if (ParsePunctuator (ES_Token::BITWISE_XOR))
        if (ParseBitwiseExpr (depth, ES_Expression::PROD_BITWISE_AND_EXPR, allow_in, ES_BitwiseExpr::TYPE_BITWISE_XOR))
          goto recurse;
        else
          return DEBUG_FALSE;

    case ES_Expression::PROD_BITWISE_AND_EXPR:
      if (ParsePunctuator (ES_Token::BITWISE_AND))
        if (ParseBitwiseExpr (depth, ES_Expression::PROD_EQUALITY_EXPR, allow_in, ES_BitwiseExpr::TYPE_BITWISE_AND))
          goto recurse;
        else
          return DEBUG_FALSE;

    case ES_Expression::PROD_EQUALITY_EXPR:
      if (token.type == ES_Token::PUNCTUATOR)
        {
          ES_EqualityExpr::Type type;

          switch (token.punctuator)
            {
            case ES_Token::EQUAL:
              type = ES_EqualityExpr::TYPE_EQUAL;
              break;

            case ES_Token::NOT_EQUAL:
              type = ES_EqualityExpr::TYPE_NOT_EQUAL;
              break;

            case ES_Token::STRICT_EQUAL:
              type = ES_EqualityExpr::TYPE_STRICT_EQUAL;
              break;

            case ES_Token::STRICT_NOT_EQUAL:
              type = ES_EqualityExpr::TYPE_STRICT_NOT_EQUAL;
              break;

            default:
              /* Abuse TYPE_THIS to mean "not equality" */
              type = ES_Expression::TYPE_THIS;
            }

          if (type != ES_Expression::TYPE_THIS)
            {
              if (!NextToken ())
                PARSE_FAILED (GENERIC_ERROR);

              if (!ParseExpression (depth, ES_Expression::PROD_RELATIONAL_EXPR, allow_in, expression_stack_used))
                return DEBUG_FALSE;

              right = PopExpression ();
              left = PopExpression ();

              PushExpression (OP_NEWGRO_L (ES_EqualityExpr, (type, left, right), Arena ()));
              goto recurse;
            }
        }

    case ES_Expression::PROD_RELATIONAL_EXPR:
      if (token.type == ES_Token::PUNCTUATOR || token.type == ES_Token::KEYWORD)
        {
          bool is_relational = false;
          bool is_instanceof_or_in = false;

          ES_RelationalExpr::Type relational_type = ES_Expression::TYPE_THIS;
          ES_InstanceofOrInExpr::Type instanceof_or_in_type = ES_Expression::TYPE_THIS;

          if (token.type == ES_Token::PUNCTUATOR)
            {
              is_relational = true;

              switch (token.punctuator)
                {
                case ES_Token::LESS_THAN:
                  relational_type = ES_RelationalExpr::TYPE_LESS_THAN;
                  break;

                case ES_Token::GREATER_THAN:
                  relational_type = ES_RelationalExpr::TYPE_GREATER_THAN;
                  break;

                case ES_Token::LESS_THAN_OR_EQUAL:
                  relational_type = ES_RelationalExpr::TYPE_LESS_THAN_OR_EQUAL;
                  break;

                case ES_Token::GREATER_THAN_OR_EQUAL:
                  relational_type = ES_RelationalExpr::TYPE_GREATER_THAN_OR_EQUAL;
                  break;

                default:
                  is_relational = false;
                }
            }
          else if (token.keyword == ES_Token::KEYWORD_INSTANCEOF)
            {
              is_instanceof_or_in = true;
              instanceof_or_in_type = ES_InstanceofOrInExpr::TYPE_INSTANCEOF;
            }
          else if (token.keyword == ES_Token::KEYWORD_IN && allow_in)
            {
              is_instanceof_or_in = true;
              instanceof_or_in_type = ES_InstanceofOrInExpr::TYPE_IN;
            }

          if (is_relational || is_instanceof_or_in)
            {
              if (!NextToken ())
                PARSE_FAILED (GENERIC_ERROR);

              if (!ParseExpression (depth, ES_Expression::PROD_SHIFT_EXPR, true, expression_stack_used))
                return DEBUG_FALSE;

              right = PopExpression ();
              left = PopExpression ();

              if (is_relational)
                PushExpression (OP_NEWGRO_L (ES_RelationalExpr, (relational_type, left, right), Arena ()));
              else
                PushExpression (OP_NEWGRO_L (ES_InstanceofOrInExpr, (instanceof_or_in_type, left, right), Arena ()));

              goto recurse;
            }
        }

    case ES_Expression::PROD_SHIFT_EXPR:
      if (token.type == ES_Token::PUNCTUATOR)
        {
          ES_ShiftExpr::Type type;

          switch (token.punctuator)
            {
            case ES_Token::SHIFT_LEFT:
              type = ES_ShiftExpr::TYPE_SHIFT_LEFT;
              break;

            case ES_Token::SHIFT_SIGNED_RIGHT:
              type = ES_ShiftExpr::TYPE_SHIFT_SIGNED_RIGHT;
              break;

            case ES_Token::SHIFT_UNSIGNED_RIGHT:
              type = ES_ShiftExpr::TYPE_SHIFT_UNSIGNED_RIGHT;
              break;

            default:
              /* Abuse TYPE_THIS to mean "not shift" */
              type = ES_Expression::TYPE_THIS;
            }

          if (type != ES_Expression::TYPE_THIS)
            {
              if (!NextToken ())
                PARSE_FAILED (GENERIC_ERROR);

              if (!ParseExpression (depth, ES_Expression::PROD_ADDITIVE_EXPR, true, expression_stack_used))
                return DEBUG_FALSE;

              right = PopExpression ();
              left = PopExpression ();

              if (!EvaluateConstantBinaryExpression (type, left, right))
                PushExpression (OP_NEWGRO_L (ES_ShiftExpr, (type, left, right), Arena ()));
              else
                depth--;

              goto recurse;
            }
        }

    case ES_Expression::PROD_ADDITIVE_EXPR:
      if (token.type == ES_Token::PUNCTUATOR)
        {
          bool is_add = false;
          bool is_subtract = false;

          if (token.punctuator == ES_Token::ADD)
            is_add = true;
          else if (token.punctuator == ES_Token::SUBTRACT)
            is_subtract = true;

          if (is_add || is_subtract)
            {
              if (!NextToken ())
                PARSE_FAILED (GENERIC_ERROR);

              if (!ParseExpression (depth, ES_Expression::PROD_MULTIPLICATIVE_EXPR, true, expression_stack_used))
                return DEBUG_FALSE;

              right = PopExpression ();
              left = PopExpression ();

              if (is_add)
                if (right->GetType () == ES_Expression::TYPE_LITERAL)
                  if (left->GetType () == ES_Expression::TYPE_ADD)
                    {
                      ES_Expression *left_right = static_cast<ES_BinaryExpr *> (left)->GetRight ();

                      if (left_right->GetType () == ES_Expression::TYPE_LITERAL && left_right->GetValueType () == ESTYPE_STRING)
                        {
                          left = static_cast<ES_BinaryExpr *> (left)->GetLeft ();

                          EvaluateConstantBinaryExpression (ES_Expression::TYPE_ADD, left_right, right);
                          depth--;

                          right = PopExpression ();
                        }
                    }

              if (!EvaluateConstantBinaryExpression (is_add ? ES_Expression::TYPE_ADD : ES_Expression::TYPE_SUBTRACT, left, right))
                if (is_add)
                  /* Should check if either left or right is a string
                     literal and use ES_AddExpr::STRINGS if so. */
                  PushExpression (OP_NEWGRO_L (ES_AddExpr, (ES_AddExpr::UNKNOWN, left, right), Arena ()));
                else
                  PushExpression (OP_NEWGRO_L (ES_BinaryNumberExpr, (ES_BinaryNumberExpr::TYPE_SUBTRACT, left, right), Arena ()));
              else
                depth--;

              goto recurse;
            }
        }

    case ES_Expression::PROD_MULTIPLICATIVE_EXPR:
      if (token.type == ES_Token::PUNCTUATOR)
        {
          ES_BinaryNumberExpr::Type type;

          switch (token.punctuator)
            {
            case ES_Token::MULTIPLY:
              type = ES_BinaryNumberExpr::TYPE_MULTIPLY;
              break;

            case ES_Token::DIVIDE:
              type = ES_BinaryNumberExpr::TYPE_DIVIDE;
              break;

            case ES_Token::REMAINDER:
              type = ES_BinaryNumberExpr::TYPE_REMAINDER;
              break;

            default:
              /* Abuse TYPE_THIS to mean "not multiplicative" */
              type = ES_Expression::TYPE_THIS;
            }

          if (type != ES_Expression::TYPE_THIS)
            {
              if (!NextToken ())
                PARSE_FAILED (GENERIC_ERROR);

              if (!ParseExpression (depth, ES_Expression::PROD_UNARY_EXPR, true, expression_stack_used))
                return DEBUG_FALSE;

              right = PopExpression ();
              left = PopExpression ();

              if (!EvaluateConstantBinaryExpression (type, left, right))
                PushExpression (OP_NEWGRO_L (ES_BinaryNumberExpr, (type, left, right), Arena ()));
              else
                depth--;

              goto recurse;
            }
        }

    case ES_Expression::PROD_UNARY_EXPR:
      if (expression_stack_length == 0)
        {
          if (ParseKeyword (ES_Token::KEYWORD_DELETE))
            {
              STORE_TOKEN_START ();

              ES_Expression *expr;

              if (!ParseExpression (depth, ES_Expression::PROD_UNARY_EXPR, true, expression_stack_used))
                return DEBUG_FALSE;

              expr = PopExpression ();

              if (is_strict_mode && expr->GetType () == ES_Expression::TYPE_IDENTIFIER)
                {
                  error_code = INVALID_USE_OF_DELETE;
                  return DEBUG_FALSE;
                }

              PushExpression (OP_NEWGRO_L (ES_DeleteExpr, (expr), Arena ()) LOCATION ());
              goto recurse;
            }

          bool is_unary = true;
          bool is_inc_or_dec = false;

          ES_UnaryExpr::Type unary_type = ES_UnaryExpr::TYPE_THIS;
          ES_IncrementOrDecrementExpr::Type inc_or_dec_type = ES_IncrementOrDecrementExpr::POST_INCREMENT;

          if (token.type == ES_Token::KEYWORD)
            switch (token.keyword)
              {
              case ES_Token::KEYWORD_VOID:
                unary_type = ES_UnaryExpr::TYPE_VOID;
                break;

              case ES_Token::KEYWORD_TYPEOF:
                unary_type = ES_UnaryExpr::TYPE_TYPEOF;
                break;

              default:
                is_unary = false;
              }
          else if (token.type == ES_Token::PUNCTUATOR)
            switch (token.punctuator)
              {
              case ES_Token::INCREMENT:
                is_inc_or_dec = true;
                is_unary = false;
                inc_or_dec_type = ES_IncrementOrDecrementExpr::PRE_INCREMENT;
                break;

              case ES_Token::DECREMENT:
                is_inc_or_dec = true;
                is_unary = false;
                inc_or_dec_type = ES_IncrementOrDecrementExpr::PRE_DECREMENT;
                break;

              case ES_Token::ADD:
                unary_type = ES_UnaryExpr::TYPE_PLUS;
                break;

              case ES_Token::SUBTRACT:
                unary_type = ES_UnaryExpr::TYPE_MINUS;
                break;

              case ES_Token::BITWISE_NOT:
                unary_type = ES_UnaryExpr::TYPE_BITWISE_NOT;
                break;

              case ES_Token::LOGICAL_NOT:
                unary_type = ES_UnaryExpr::TYPE_LOGICAL_NOT;
                break;

              default:
                is_unary = false;
              }
          else
            is_unary = false;

          if (is_unary || is_inc_or_dec)
            {
              ES_Expression *expr;

              if (!NextToken ())
                PARSE_FAILED (GENERIC_ERROR);

              STORE_TOKEN_START ();

              if (is_unary && unary_type == ES_UnaryExpr::TYPE_TYPEOF)
                ++in_typeof;

              if (!ParseExpression (depth, ES_Expression::PROD_UNARY_EXPR, true, expression_stack_used))
                return DEBUG_FALSE;

              if (is_unary && unary_type == ES_UnaryExpr::TYPE_TYPEOF)
                --in_typeof;

              expr = PopExpression ();

              if (is_unary)
                {
                  if (expr->GetType () == ES_Expression::TYPE_LITERAL)
                    {
                      ES_LiteralExpr *literal = static_cast<ES_LiteralExpr *>(expr);

                      if (unary_type == ES_UnaryExpr::TYPE_PLUS || unary_type == ES_UnaryExpr::TYPE_MINUS)
                        {
                          double value = literal->GetValue ().AsNumber (context).GetNumAsDouble ();

                          if (unary_type == ES_UnaryExpr::TYPE_MINUS)
                            value = -value;

                          PushExpression (OP_NEWGRO_L (ES_LiteralExpr, (value), Arena ()));
                          goto recurse;
                        }
                      else if (unary_type == ES_UnaryExpr::TYPE_BITWISE_NOT)
                        {
                          int value = literal->GetValue ().AsNumber (context).GetNumAsInt32 ();

                          PushExpression (OP_NEWGRO_L (ES_LiteralExpr, (~value), Arena ()));
                          goto recurse;
                        }
                      else if (unary_type == ES_UnaryExpr::TYPE_LOGICAL_NOT)
                        {
                          value.SetBoolean (!literal->GetValue ().AsBoolean ().GetBoolean ());

                          PushExpression (OP_NEWGRO_L (ES_LiteralExpr, (value), Arena ()));
                          goto recurse;
                        }
                    }

                  PushExpression (OP_NEWGRO_L (ES_UnaryExpr, (unary_type, expr), Arena ()) LOCATION ());
                }
              else
                {
                  JString *name;

                  if (expr->IsIdentifier (name))
                    if (!ValidateIdentifier (name, &expr->GetSourceLocation ()))
                      return DEBUG_FALSE;

                  PushExpression (OP_NEWGRO_L (ES_IncrementOrDecrementExpr, (inc_or_dec_type, expr), Arena ()) LOCATION ());
                }
              goto recurse;
            }
        }

    case ES_Expression::PROD_POSTFIX_EXPR:
      if (expression_stack_length > 0)
        {
          bool previous_allow_linebreak = SetAllowLinebreak (false);
          ES_IncrementOrDecrementExpr::Type type;

          /* Abuse PRE_INCREMENT to mean "not postfix" */
          if (last_token.type == ES_Token::LINEBREAK)
            type = ES_IncrementOrDecrementExpr::PRE_INCREMENT;
          else if (ParsePunctuator (ES_Token::INCREMENT))
            type = ES_IncrementOrDecrementExpr::POST_INCREMENT;
          else if (ParsePunctuator (ES_Token::DECREMENT))
            type = ES_IncrementOrDecrementExpr::POST_DECREMENT;
          else
            type = ES_IncrementOrDecrementExpr::PRE_INCREMENT;

          SetAllowLinebreak (previous_allow_linebreak);

          if (type != ES_IncrementOrDecrementExpr::PRE_INCREMENT)
            {
              ES_Expression *expr;

              expr = PopExpression ();

              JString *name;

              if (expr->IsIdentifier (name))
                if (!ValidateIdentifier (name, &expr->GetSourceLocation ()))
                  return DEBUG_FALSE;

              PushExpression (OP_NEWGRO_L (ES_IncrementOrDecrementExpr, (type, expr), Arena ()) LOCATION_FROM (expr));
              goto recurse;
            }
        }

    case ES_Expression::PROD_LEFT_HAND_SIDE_EXPR:
    case ES_Expression::PROD_CALL_EXPR:
      if (expression_stack_length > 0)
        {
          unsigned exprs_before = expression_stack_used;

          if (ParsePunctuator (ES_Token::LEFT_PAREN))
            if (ParseArguments (depth))
              {
                ES_Expression *func;
                unsigned args_count;
                ES_Expression **args;

                args_count = expression_stack_used - exprs_before;
                args = PopExpressions (args_count);

                func = PopExpression ();

                PushExpression (OP_NEWGRO_L (ES_CallExpr, (func, args_count, args), Arena ()) LOCATION_FROM (func));
                goto recurse;
              }
            else
              return DEBUG_FALSE;
        }

    case ES_Expression::PROD_NEW_EXPR:
    case ES_Expression::PROD_MEMBER_EXPR:
      if (expression_stack_length > 0)
        {
          switch (ParsePunctuator1 (ES_Token::LEFT_BRACKET))
            {
            case INVALID_TOKEN:
              automatic_error_code = EXPECTED_EXPRESSION;
              return DEBUG_FALSE;

            case FOUND:
              ES_Expression *base;
              ES_Expression *index;

              if (!ParseExpression (depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) ||
                  !ParsePunctuator (ES_Token::RIGHT_BRACKET))
                return DEBUG_FALSE;

              index = PopExpression ();
              base = PopExpression ();

              if (index->GetType () == ES_Expression::TYPE_LITERAL && index->GetValueType () == ESTYPE_STRING)
                {
                  JString *name = static_cast<ES_LiteralExpr *> (index)->GetValue ().GetString ();

		  unsigned idx;
		  if (convertindex (Storage (context, name), Length (name), idx))
                    static_cast<ES_LiteralExpr *> (index)->GetValue ().SetNumber (idx);
                  else
                    {
                      PushExpression (OP_NEWGRO_L (ES_PropertyReferenceExpr, (base, name), Arena ()) LOCATION_FROM (base));
                      goto recurse;
                    }
                }

              PushExpression (OP_NEWGRO_L (ES_ArrayReferenceExpr, (base, index), Arena ()) LOCATION_FROM (base));
              goto recurse;
            }

          switch (ParsePunctuator1 (ES_Token::PERIOD))
            {
            case INVALID_TOKEN:
              automatic_error_code = EXPECTED_IDENTIFIER;
              return DEBUG_FALSE;

            case FOUND:
              ES_Expression *base;
              JString *name;

              if (!ParseIdentifier (name, false, true))
                return DEBUG_FALSE;

              base = PopExpression ();

              PushExpression (OP_NEWGRO_L (ES_PropertyReferenceExpr, (base, name), Arena ()) LOCATION_FROM (base));
              goto recurse;
            }
        }

      if (expression_stack_length == 0)
        if (ParseKeyword (ES_Token::KEYWORD_NEW))
          {
            STORE_TOKEN_START ();

            ES_Expression *ctor;
            unsigned args_count;
            ES_Expression **args;

            if (expression_stack_length > 0)
              PARSE_FAILED (GENERIC_ERROR);

            if (!ParseExpression (depth, ES_Expression::PROD_NEW_EXPR, true, expression_stack_used))
              return DEBUG_FALSE;

            unsigned exprs_before = expression_stack_used;

            if (ParsePunctuator (ES_Token::LEFT_PAREN))
              if (ParseArguments (depth))
                {
                  args_count = expression_stack_used - exprs_before;
                  args = PopExpressions (args_count);
                }
              else
                return DEBUG_FALSE;
            else if (production < ES_Expression::PROD_MEMBER_EXPR)
              {
                args_count = 0;
                args = 0;
              }
            else
              PARSE_FAILED (GENERIC_ERROR);

            ctor = PopExpression ();

            PushExpression (OP_NEWGRO_L (ES_NewExpr, (ctor, args_count, args), Arena ()) LOCATION ());
            goto recurse;
          }
        else if (ParseKeyword (ES_Token::KEYWORD_FUNCTION))
          {
            if (!ParseFunctionExpr ())
              return DEBUG_FALSE;

            goto recurse;
          }

    case ES_Expression::PROD_PRIMARY_EXPR:
      if (expression_stack_length == 0)
        {
          if (ParsePunctuator (ES_Token::LEFT_BRACKET))
            {
              STORE_TOKEN_START ();

              unsigned exprs_count;
              ES_Expression **exprs;

              unsigned expressions_before = expression_stack_used;

              while (!ParsePunctuator (ES_Token::RIGHT_BRACKET))
                if (ParseExpression (depth, ES_Expression::PROD_ASSIGNMENT_EXPR, true, expression_stack_used))
                  {
                    if (!ParsePunctuator (ES_Token::COMMA))
                      if (ParsePunctuator (ES_Token::RIGHT_BRACKET))
                        break;
                      else
                        PARSE_FAILED (GENERIC_ERROR);
                  }
                else if (ParsePunctuator (ES_Token::COMMA))
                  PushExpression (NULL);
                else
                  PARSE_FAILED (GENERIC_ERROR);

              exprs_count = expression_stack_used - expressions_before;
              exprs = PopExpressions (exprs_count);

              PushExpression (OP_NEWGRO_L (ES_ArrayLiteralExpr, (exprs_count, exprs), Arena ()) LOCATION ());
              goto recurse;
            }

          if (ParsePunctuator (ES_Token::LEFT_BRACE))
            {
              unsigned index = last_token.start;
              unsigned line = last_token.line;

              unsigned props_count, properties_before = property_stack_used;

              while (1)
                {
                  JString *name;

                  if (!ParseProperty (name, false))
                    if (ParsePunctuator (ES_Token::RIGHT_BRACE))
                      break;
                    else
                      return DEBUG_FALSE;

                  if (name)
                    {
                      JString *actual_name;
                      unsigned check_count = property_stack_used - properties_before;

#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
                      unsigned index = last_token.start;
                      unsigned line = last_token.line;
                      unsigned column = last_token.column;
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT

                      if (name->Equals (UNI_L ("get"), 3))
                        if (ParseProperty (actual_name, false))
                          {
                            if (!ParseAccessor (ACCESSOR_GET, actual_name, check_count, index, line, column))
                              return DEBUG_FALSE;
                          }
                        else
                          goto regular_property;
                      else if (name->Equals (UNI_L ("set"), 3))
                        if (ParseProperty (actual_name, false))
                          {
                            if (!ParseAccessor (ACCESSOR_SET, actual_name, check_count, index, line, column))
                              return DEBUG_FALSE;
                          }
                        else
                          goto regular_property;
                      else
                        {
                        regular_property:
#ifdef ES_LEXER_SOURCE_LOCATION_SUPPORT
                          ES_SourceLocation name_location = last_token.GetSourceLocation();
# define NAME_LOCATION , name_location
#else // ES_LEXER_SOURCE_LOCATION_SUPPORT
# define NAME_LOCATION
#endif // ES_LEXER_SOURCE_LOCATION_SUPPORT


                          JString *old_debug_name = current_debug_name;
                          ES_Expression *old_debug_name_expr = current_debug_name_expr;

                          current_debug_name = name;
                          current_debug_name_expr = NULL;

                          if (!ParsePunctuator (ES_Token::CONDITIONAL_FALSE) ||
                              !ParseExpression (depth, ES_Expression::PROD_ASSIGNMENT_EXPR, true, expression_stack_used))
                            return DEBUG_FALSE;

                          current_debug_name = old_debug_name;
                          current_debug_name_expr = old_debug_name_expr;

                          if (!PushProperty (check_count, name NAME_LOCATION, PopExpression ()))
                            return DEBUG_FALSE;
                        }
                    }

                  if (!ParsePunctuator (ES_Token::COMMA))
                    if (!ParsePunctuator (ES_Token::RIGHT_BRACE))
                      return DEBUG_FALSE;
                    else
                      break;
                }

#undef NAME_LOCATION

              props_count = property_stack_used - properties_before;

              ES_ObjectLiteralExpr::Property *properties = static_cast<ES_ObjectLiteralExpr::Property *>(PopProperties (props_count));

              PushExpression (OP_NEWGRO_L (ES_ObjectLiteralExpr, (props_count, properties), Arena ()) LOCATION ());
              goto recurse;
            }
        }

      if (expression_stack_length == 1 || (expression_stack_length == 0 && opt))
        return true;
      else
        {
          automatic_error_code = ES_Parser::EXPECTED_EXPRESSION;

          return DEBUG_FALSE;
        }
    }

  /* Never reached. */
  return DEBUG_FALSE;
}
