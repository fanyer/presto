#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_parser.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_stmt.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_expr.h"
#include "modules/ecmascript/carakan/src/compiler/es_parser_inlines.h"


bool
ES_Parser::ParseStatement (unsigned depth)
{
    if (IsNearStackLimit() || ++depth > ES_MAXIMUM_SYNTAX_TREE_DEPTH)
    {
      error_code = INPUT_TOO_COMPLEX;
      return false;
    }

  ES_Statement *statement;

  if (!HandleLinebreak ())
    return false;

  unsigned start_index = token.start;
  unsigned start_line = token.line;

  unsigned end_index = UINT_MAX;

  if (ParsePunctuator (ES_Token::LEFT_BRACE))
    {
      unsigned statements_before = statement_stack_used;

      while (1)
        {
          if (ParsePunctuator (ES_Token::RIGHT_BRACE))
            break;
          else if (!ParseStatement (depth))
            return false;
        }

      unsigned statements_count = statement_stack_used - statements_before;
      if (statements_count != 0)
        {
          ES_Statement **statements = PopStatements (statements_count);
          statement = OP_NEWGRO_L (ES_BlockStmt, (statements_count, statements), Arena ());
        }
      else
        statement = OP_NEWGRO_L (ES_EmptyStmt, (TRUE), Arena ());
    }
  else if (ParseKeyword (ES_Token::KEYWORD_VAR))
    {
      unsigned identifiers_before = identifier_stack_used;

      if (!ParseVariableDeclList (depth, true) ||
          !ParseSemicolon ())
        return false;

      unsigned decls_count = identifier_stack_used - identifiers_before;

      JString **names = PopIdentifiers (decls_count);
      ES_Expression **initializers = PopExpressions (decls_count);

      statement = OP_NEWGRO_L (ES_VariableDeclStmt, (decls_count, names, initializers), Arena ());
    }
  else if (ParsePunctuator (ES_Token::SEMICOLON))
    statement = OP_NEWGRO_L (ES_EmptyStmt, (), Arena ());
  else if (ParseKeyword (ES_Token::KEYWORD_IF))
    {
      ES_Expression *condition;
      ES_Statement *ifstmt;
      ES_Statement *elsestmt;

      if (!ParsePunctuator (ES_Token::LEFT_PAREN) ||
          !ParseExpression (depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) ||
          !ParsePunctuator (ES_Token::RIGHT_PAREN))
        return false;

      end_index = last_token.end;

      if (!ParseStatement (depth))
        return false;

      condition = PopExpression ();
      ifstmt = PopStatement ();

      if (ParseKeyword (ES_Token::KEYWORD_ELSE))
        {
          if (!ParseStatement (depth))
            return false;

          elsestmt = PopStatement ();
        }
      else
        elsestmt = 0;

      statement = OP_NEWGRO_L (ES_IfStmt, (condition, ifstmt, elsestmt), Arena ());
    }
  else if (ParseKeyword (ES_Token::KEYWORD_DO))
    {
      ES_Expression *condition;
      ES_Statement *body;

      if (!ParseStatement (depth) || !HandleLinebreak ())
        return false;

      if (!ParseKeyword (ES_Token::KEYWORD_WHILE) ||
          !ParsePunctuator (ES_Token::LEFT_PAREN) ||
          !ParseExpression (depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) ||
          !ParsePunctuator (ES_Token::RIGHT_PAREN))
        return false;

      if (!HandleLinebreak () || !ParseSemicolon (true))
        return false;

      condition = PopExpression ();
      body = PopStatement ();

      statement = OP_NEWGRO_L (ES_WhileStmt, (ES_WhileStmt::TYPE_DO_WHILE, condition, body), Arena ());
    }
  else if (ParseKeyword (ES_Token::KEYWORD_WHILE))
    {
      ES_Expression *condition;
      ES_Statement *body;

      if (!ParsePunctuator (ES_Token::LEFT_PAREN) ||
          !ParseExpression (depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) ||
          !ParsePunctuator (ES_Token::RIGHT_PAREN))
        return false;

      end_index = last_token.end;

      if (!ParseStatement (depth))
        return false;

      condition = PopExpression ();
      body = PopStatement ();

      statement = OP_NEWGRO_L (ES_WhileStmt, (ES_WhileStmt::TYPE_WHILE, condition, body), Arena ());
    }
  else if (ParseKeyword (ES_Token::KEYWORD_FOR))
    {
      ES_VariableDeclStmt *init_vardecl = NULL;
      ES_ExpressionStmt *init_stmt = NULL;
      ES_Expression *init_expr = NULL;
      ES_Expression *condition = NULL;
      ES_ExpressionStmt *iteration = NULL;
      ES_Expression *source = NULL;
      ES_Statement *body;

      bool is_vardecl = false;
      bool is_forin = false;

      if (!ParsePunctuator (ES_Token::LEFT_PAREN))
        return false;

      if (ParseKeyword (ES_Token::KEYWORD_VAR))
        {
          unsigned vardecl_start = last_token.start;
          unsigned vardecl_line = last_token.line;

          is_vardecl = true;

          unsigned identifiers_before = identifier_stack_used;

          if (!ParseVariableDeclList (depth, false))
            return false;

          unsigned decls_count = identifier_stack_used - identifiers_before;

          if (decls_count == 1 && ParseKeyword (ES_Token::KEYWORD_IN))
            is_forin = true;

          JString **names = PopIdentifiers (decls_count);
          ES_Expression **initializers = PopExpressions (decls_count);

          init_vardecl = OP_NEWGRO_L (ES_VariableDeclStmt, (decls_count, names, initializers), Arena ());

          init_vardecl->SetSourceLocation (ES_SourceLocation (vardecl_start, vardecl_line, last_token.end - vardecl_start));
        }
      else
        {
          unsigned expression_stack_before = expression_stack_used;

          if (!ParseExpression (depth, ES_Expression::PROD_EXPRESSION, false, expression_stack_used, true))
            return false;

          if (expression_stack_before != expression_stack_used)
            {
              init_expr = PopExpression ();

              if (init_expr->IsLValue () && ParseKeyword (ES_Token::KEYWORD_IN))
                {
                  JString *name;

                  if (init_expr->IsIdentifier (name) && !ValidateIdentifier (name))
                    return false;

                  is_forin = true;
                }
              else
                init_stmt = OP_NEWGRO_L (ES_ExpressionStmt, (init_expr), Arena ());
            }
          else
            init_stmt = 0;
        }

      if (is_forin)
        {
          if (!ParseExpression (depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used))
            return false;

          source = PopExpression ();
        }
      else
        {
          if (!ParsePunctuator (ES_Token::SEMICOLON))
            return false;

          unsigned expression_stack_before = expression_stack_used;

          if (!ParseExpression (depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used, true))
            return false;

          if (expression_stack_before != expression_stack_used)
            condition = PopExpression ();
          else
            condition = 0;

          if (!ParsePunctuator (ES_Token::SEMICOLON))
            return false;

          expression_stack_before = expression_stack_used;

          if (!ParseExpression (depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used, true))
            return false;

          if (expression_stack_before != expression_stack_used)
            iteration = OP_NEWGRO_L (ES_ExpressionStmt, (PopExpression ()), Arena ());
          else
            iteration = 0;
        }

      if (!ParsePunctuator (ES_Token::RIGHT_PAREN))
        return false;

      end_index = last_token.end;

      if (!ParseStatement (depth))
        return false;

      body = PopStatement ();

      if (is_forin)
        if (is_vardecl)
          statement = OP_NEWGRO_L (ES_ForInStmt, (init_vardecl, source, body), Arena ());
        else
          statement = OP_NEWGRO_L (ES_ForInStmt, (init_expr, source, body), Arena ());
      else
        if (is_vardecl)
          statement = OP_NEWGRO_L (ES_ForStmt, (init_vardecl, condition, iteration, body), Arena ());
        else
          statement = OP_NEWGRO_L (ES_ForStmt, (init_stmt, condition, iteration, body), Arena ());
    }
  else if (ParseKeyword (ES_Token::KEYWORD_CONTINUE))
    {
      JString *target;

      bool previous_allow_linebreak = SetAllowLinebreak (false);

      if (!ParseIdentifier (target, true))
        return false;

      SetAllowLinebreak (previous_allow_linebreak);

      if (!ParseSemicolon ())
        return false;

      statement = OP_NEWGRO_L (ES_ContinueStmt, (target), Arena ());
    }
  else if (ParseKeyword (ES_Token::KEYWORD_BREAK))
    {
      JString *target;

      bool previous_allow_linebreak = SetAllowLinebreak (false);

      if (!ParseIdentifier (target, true))
        return false;

      SetAllowLinebreak (previous_allow_linebreak);

      if (!ParseSemicolon ())
        return false;

      statement = OP_NEWGRO_L (ES_BreakStmt, (target), Arena ());
    }
  else if (ParseKeyword (ES_Token::KEYWORD_RETURN))
    {
      if (!in_function && !allow_return_in_program)
        {
          error_code = INVALID_RETURN;

          return false;
        }

      ES_Expression *expression;

      bool previous_allow_linebreak = SetAllowLinebreak (false);

      unsigned expression_stack_before = expression_stack_used;

      if (!ParseExpression (depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used, true))
        return false;

      if (expression_stack_before != expression_stack_used)
        expression = PopExpression ();
      else
        expression = 0;

      SetAllowLinebreak (previous_allow_linebreak);

      if (!ParseSemicolon ())
        return false;

      statement = OP_NEWGRO_L (ES_ReturnStmt, (expression), Arena ());
    }
  else if (ParseKeyword (ES_Token::KEYWORD_WITH))
    {
      if (is_strict_mode)
        {
          error_code = WITH_IN_STRICT_MODE;

          return false;
        }

      ES_Expression *expression;
      ES_Statement *body;

      in_with++;

      if (!ParsePunctuator (ES_Token::LEFT_PAREN) ||
          !ParseExpression (depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) ||
          !ParsePunctuator (ES_Token::RIGHT_PAREN))
        return false;

      end_index = last_token.end;

      if (!ParseStatement (depth))
        return false;

      expression = PopExpression ();
      body = PopStatement ();

      statement = OP_NEWGRO_L (ES_WithStmt, (expression, body), Arena ());

      in_with--;
    }
  else if (ParseKeyword (ES_Token::KEYWORD_SWITCH))
    {
      if (!ParsePunctuator (ES_Token::LEFT_PAREN) ||
          !ParseExpression (depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) ||
          !ParsePunctuator (ES_Token::RIGHT_PAREN))
        return false;

      end_index = last_token.end;

      if (!ParsePunctuator (ES_Token::LEFT_BRACE))
        return false;

      ES_Expression *expression = PopExpression ();

      unsigned statements_before = statement_stack_used;
      unsigned case_clauses_count = 0;
      bool seen_default = false;

      ES_SwitchStmt::CaseClause *first = 0;
      ES_SwitchStmt::CaseClause *current = 0;

      while (1)
        {
          if (ParsePunctuator (ES_Token::RIGHT_BRACE))
            break;
          else if (ParseKeyword (ES_Token::KEYWORD_CASE))
            {
              unsigned case_index = last_token.start;
              unsigned case_line = last_token.line;

              if (!ParseExpression (depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used) ||
                  !ParsePunctuator (ES_Token::CONDITIONAL_FALSE))
                return false;

              ES_SwitchStmt::CaseClause *new_current = OP_NEWGRO_L (ES_SwitchStmt::CaseClause, (), Arena ());

              new_current->expression = PopExpression ();
              new_current->statements_count = 0;

              new_current->location = ES_SourceLocation(case_index, case_line, last_token.end - case_index);

              if (current)
                current->next = new_current;
              else
                first = new_current;

              current = new_current;
              ++case_clauses_count;
            }
          else if (ParseKeyword (ES_Token::KEYWORD_DEFAULT))
            {
              if (seen_default)
                {
                  error_code = DUPLICATE_DEFAULT;

                  return false;
                }

              seen_default = true;

              unsigned case_index = last_token.start;
              unsigned case_line = last_token.line;

              if (!ParsePunctuator (ES_Token::CONDITIONAL_FALSE))
                return false;

              ES_SwitchStmt::CaseClause *new_current = OP_NEWGRO_L (ES_SwitchStmt::CaseClause, (), Arena ());

              new_current->expression = 0;
              new_current->statements_count = 0;

              new_current->location = ES_SourceLocation(case_index, case_line, last_token.end - case_index);

              if (current)
                current->next = new_current;
              else
                first = new_current;

              current = new_current;
              ++case_clauses_count;
            }
          else
            {
              if (!current || !ParseStatement (depth))
                return false;

              ++current->statements_count;
            }
        }

      if (current)
        current->next = 0;

      unsigned statements_count = statement_stack_used - statements_before;
      ES_Statement **statements = PopStatements (statements_count);

      statement = OP_NEWGRO_L (ES_SwitchStmt, (expression, first, statements_count, statements), Arena ());
    }
  else if (ParseKeyword (ES_Token::KEYWORD_THROW))
    {
      ES_Expression *expression;

      bool previous_allow_linebreak = SetAllowLinebreak (false);

      if (!ParseExpression (depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_used))
        return false;

      SetAllowLinebreak (previous_allow_linebreak);

      if (!ParseSemicolon ())
        return false;

      expression = PopExpression ();

      statement = OP_NEWGRO_L (ES_ThrowStmt, (expression), Arena ());
    }
  else if (ParseKeyword (ES_Token::KEYWORD_TRY))
    {
      ES_Statement *try_block;
      JString *catch_identifier;
      ES_Statement *catch_block;
      ES_Statement *finally_block;

      if (!ParseStatement (depth))
        return false;

      try_block = PopStatement ();

      if (!try_block->IsBlock ())
        return false;

      if (ParseKeyword (ES_Token::KEYWORD_CATCH))
        {
          ++in_with;

          if (!ParsePunctuator (ES_Token::LEFT_PAREN) ||
              !ParseIdentifier (catch_identifier) ||
              !ValidateIdentifier (catch_identifier) ||
              !ParsePunctuator (ES_Token::RIGHT_PAREN) ||
              !ParseStatement (depth))
            return false;

          --in_with;

          catch_block = PopStatement ();

          if (!catch_block->IsBlock ())
            return false;
        }
      else
        {
          catch_identifier = 0;
          catch_block = 0;
        }

      if (ParseKeyword (ES_Token::KEYWORD_FINALLY))
        {
          if (!ParseStatement (depth))
            return false;

          finally_block = PopStatement ();

          if (!finally_block->IsBlock ())
            return false;
        }
      else
        finally_block = 0;

      if (!catch_block && !finally_block)
        {
          error_code = TRY_WITHOUT_CATCH_OR_FINALLY;

          return false;
        }

      statement = OP_NEWGRO_L (ES_TryStmt, (try_block, catch_identifier, catch_block, finally_block), Arena ());
    }
  else if (ParseKeyword (ES_Token::KEYWORD_FUNCTION))
    {
      if (is_strict_mode)
        {
          error_code = FUNCTION_DECL_IN_STATEMENT;
          return false;
        }

      if (!ParseFunctionDecl (true))
        return false;

      statement = OP_NEWGRO_L (ES_EmptyStmt, (), Arena ());
    }
  else if (ParseKeyword (ES_Token::KEYWORD_DEBUGGER))
    {
      if (!ParseSemicolon ())
        return false;

#ifdef ECMASCRIPT_DEBUGGER
      statement = OP_NEWGRO_L (ES_DebuggerStmt, (), Arena ());
#else // ECMASCRIPT_DEBUGGER
      statement = OP_NEWGRO_L (ES_EmptyStmt, (), Arena ());
#endif // ECMASCRIPT_DEBUGGER
    }
  else
    {
      JString *identifier;

      unsigned expression_stack_base = expression_stack_used;

      if (ParseIdentifier (identifier))
        {
          if (ParsePunctuator (ES_Token::CONDITIONAL_FALSE))
            {
              end_index = last_token.end;
              ES_SourceLocation lbl_loc(start_index, start_line, end_index - start_index);

              ES_Statement *statement;

              if (!ParseStatement (depth))
                return false;

              statement = PopStatement ();

              ES_LabelledStmt *lbl_stmt = OP_NEWGRO_L (ES_LabelledStmt, (identifier, statement), Arena ());

              lbl_stmt->SetSourceLocation(lbl_loc);

              PushStatement (lbl_stmt);

              return true;
            }
          else
	    {
	      if (in_function && identifier == ident_arguments)
		uses_arguments = TRUE;
	      else if (identifier == ident_eval)
		uses_eval = TRUE;

	      PushExpression (OP_NEWGRO_L (ES_IdentifierExpr, (identifier), Arena ()));
	    }
        }

      ES_Expression *expression;

      if (!ParseExpression (depth, ES_Expression::PROD_EXPRESSION, true, expression_stack_base))
        return false;

      expression = PopExpression ();

      if (!ParseSemicolon ())
        return false;

      statement = OP_NEWGRO_L (ES_ExpressionStmt, (expression), Arena ());
    }

  if (end_index == UINT_MAX)
    end_index = last_token.end;

  statement->SetSourceLocation(ES_SourceLocation(start_index, start_line, end_index - start_index));

  PushStatement (statement);
  return true;
}
