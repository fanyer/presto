/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/regexp/src/re_config.h"
#include "modules/regexp/src/re_parser.h"
#include "modules/regexp/src/re_tree.h"
#include "modules/regexp/src/re_class.h"

RE_Parser::RE_Parser (BOOL case_insensitive, OpMemGroup *arena)
  : arena (arena),
    stack (0),
    stack_used (0),
    stack_allocated (0),
    current (0),
    free_production (0),
    characters (0),
    characters_used (0),
    case_insensitive (case_insensitive)
{
}

void
RE_Parser::Push (RE_TreeNode *node)
{
  if (stack_used == stack_allocated)
    {
      unsigned new_stack_allocated = stack_allocated == 0 ? 64 : stack_allocated + stack_allocated;
      RE_TreeNode **new_stack = OP_NEWGROA_L (RE_TreeNode *, new_stack_allocated, arena);

      op_memcpy (new_stack, stack, stack_used * sizeof *stack);

      stack = new_stack;
      stack_allocated = new_stack_allocated;
    }

  stack[stack_used++] = node;
}

RE_TreeNode **
RE_Parser::Pop (unsigned count)
{
  if (count == 0)
    return 0;
  else
    {
      RE_TreeNode **nodes = OP_NEWGROA_L (RE_TreeNode *, count, arena);

      op_memcpy (nodes, stack + stack_used - count, count * sizeof *nodes);
      stack_used -= count;

      return nodes;
    }
}

void
RE_Parser::Open (Production::Type type)
{
  Production *production;

  if (free_production)
    {
      production = free_production;
      free_production = production->next;
    }
  else
    production = OP_NEWGRO_L (Production, (), arena);

  production->type = type;
  production->start = stack_used;
  production->next = current;

  current = production;
}

void
RE_Parser::Close ()
{
  unsigned count = stack_used - current->start;

  if (current->type != Production::TYPE_SEQUENCE || count != 1)
    {
      if (current->type == Production::TYPE_ALTERNATIVES)
        Push (OP_NEWGRO_L (RE_Alternatives, (Pop (count), count), arena));
      else
        {
          if (current->type == Production::TYPE_GROUP || count != 1)
            Push (OP_NEWGRO_L (RE_Sequence, (Pop (count), count), arena));

          switch (current->type)
            {
            case Production::TYPE_CAPTURE:
              Top () = OP_NEWGRO_L (RE_Capture, (Top (), current->index), arena);
              break;

            case Production::TYPE_LOOKAHEAD_POSITIVE:
              Top () = OP_NEWGRO_L (RE_Lookahead, (Top (), TRUE), arena);
              break;

            case Production::TYPE_LOOKAHEAD_NEGATIVE:
              Top () = OP_NEWGRO_L (RE_Lookahead, (Top (), FALSE), arena);
              break;
            }
        }
    }

  free_production = current;
  current = current->next;
}

RE_TreeNode *
RE_Parser::Parse (const uni_char *source, unsigned source_length)
{
#define EXPECT_MORE(count) do { if (source_index + count >= source_length) return 0; } while (0)
#define GET_CHAR() do { EXPECT_MORE (1); ch = source[source_index++]; } while (0)
#define GET_CHAR_OPTIONAL() ((source_index < source_length) ? (ch = source[source_index++], TRUE) : FALSE)
#define BACK_CHAR() do { --source_index; } while (0)
#define UNEXPECTED_CHAR() return 0
#define INVALID_CLASS() return 0
#define EXPECTED_PAREN() return 0

  characters = OP_NEWGROA_L (uni_char, source_length, arena);
  characters_used = 0;

  Open (Production::TYPE_SEQUENCE);

  unsigned source_index = 0;
  int ch;

  while (GET_CHAR_OPTIONAL ())
    {
      if (ch == '(')
        {
          GET_CHAR ();

          if (ch == '?')
            {
              GET_CHAR ();

              if (ch == ':')
                Open (Production::TYPE_GROUP);
              else if (ch == '=')
                Open (Production::TYPE_LOOKAHEAD_POSITIVE);
              else if (ch == '!')
                Open (Production::TYPE_LOOKAHEAD_NEGATIVE);
              else
                UNEXPECTED_CHAR ();
            }
          else
            {
              Open (Production::TYPE_CAPTURE);
              current->index = capture_count++;

              BACK_CHAR ();
            }
        }
      else if (ch == ')')
        {
          while (current && current->type < Production::TYPE_CLOSED_BY_PAREN)
            Close ();

          if (!current)
            UNEXPECTED_CHAR ();

          Close ();
        }
      else if (ch == '*' || ch == '?' || ch == '+' || ch == '{')
        {
          if (stack_used == 0 || Top ()->GetNodeType () == RE_TreeNode::TYPE_QUANTIFIER)
            UNEXPECTED_CHAR ();

          unsigned min = 0, max = UINT_MAX;
          BOOL greedy = TRUE;

          if (ch == '?')
            max = 1;
          else if (ch == '+')
            min = 1;
          else if (ch == '{')
            {
              GET_CHAR ();

              if ((ch < '0' || ch > '9') && ch != ',')
                UNEXPECTED_CHAR ();

              while (ch >= '0' && ch <= '9')
                {
                  min = min * 10 + (ch - '0');
                  GET_CHAR ();
                }

              if (ch == ',')
                {
                  GET_CHAR ();

                  if (ch >= '0' && ch <= '9')
                    {
                      max = 0;

                      while (ch >= '0' && ch <= '9')
                        {
                          max = max * 10 + (ch - '0');
                          GET_CHAR ();
                        }
                    }
                }
              else
                max = min;

              if (ch != '}')
                UNEXPECTED_CHAR ();
            }

          if (GET_CHAR_OPTIONAL ())
            if (ch == '?')
              greedy = FALSE;
            else
              BACK_CHAR ();

          Top () = OP_NEWGRO_L (RE_Quantifier, (Top (), loop_count++, min, max, greedy), arena);
        }
      else if (ch == '|')
        {
          unsigned count = stack_used - current->start;

          if (current->type == Production::TYPE_CAPTURE && count > 1)
            Push (OP_NEWGRO_L (RE_Sequence, (Pop (count), count), arena));

          while (current && current->type == Production::TYPE_SEQUENCE)
            Close ();

          if (!current || current->type != Production::TYPE_ALTERNATIVES)
            {
              Open (Production::TYPE_ALTERNATIVES);
              --current->start;
            }

          Open (Production::TYPE_SEQUENCE);
        }
      else if (ch == '[')
        {
          RE_Class *cls = OP_NEW_L (RE_Class, (case_insensitive));

          if (!cls->ConstructL (0, source, source_index, source_length, FALSE))
            INVALID_CLASS ();

          Push (OP_NEWGRO_L (RE_CharacterClass, (cls), arena));
        }
      else
        {
          uni_char *start = characters + characters_used, *write = start;

          do
            {
              if (ch == '\\')
                {
                  GET_CHAR ();

                  switch (ch)
                    {
                    case 'd':
                    case 'D':
                    case 's':
                    case 'S':
                    case 'w':
                    case 'W':
                    case 'b':
                    case 'B':
                      ch = '\\';
                      --source_index;
                      goto finished;

                    case 't': ch = 9; goto add_char;
                    case 'n': ch = 10; goto add_char;
                    case 'v': ch = 11; goto add_char;
                    case 'f': ch = 12; goto add_char;
                    case 'r': ch = 13; goto add_char;

                    case 'x':
                    case 'u':
                    case 'c':
                    case '0':
                      OP_ASSERT (FALSE);
                    }
                }
              else if (ch == '{' || ch == '*' || ch == '?' || ch == '+')
                {
                  if (write - start > 1)
                    {
                      ch = source[source_index -= 2];
                      --write;
                    }
                  else
                    BACK_CHAR ();

                  break;
                }
              else if (ch == '(' || ch == '[' || ch == ')' || ch == '|')
                {
                  BACK_CHAR ();
                  break;
                }
              else
              add_char: *write++ = ch;
            }
          while (GET_CHAR_OPTIONAL ());

        finished:
          if (start < write)
            {
              unsigned length = write - start;
              Push (OP_NEWGRO_L (RE_Characters, (start, length, case_insensitive), arena));
              characters_used += length;
            }

          if (ch == '\\')
            {
              GET_CHAR ();

              RE_Class *cls = OP_NEW_L (RE_Class, (case_insensitive));
              cls->ConstructL (ch);
              Push (OP_NEWGRO_L (RE_CharacterClass, (cls), arena));
            }
        }
    }

  while (current && current->type < Production::TYPE_CLOSED_BY_PAREN)
    Close ();

  if (current)
    EXPECTED_PAREN ();

  OP_ASSERT (stack_used == 1);

  return stack[0];
}
