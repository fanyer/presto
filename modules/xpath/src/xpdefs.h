/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef XPDEFS_H
#define XPDEFS_H

#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmltreeaccessor.h"

/* 3 bit alignment on 64 bit systems and 2 bit alignment on 32 bit systems?
   Don't know if that is a sensible assumption. */
#ifdef SIXTY_FOUR_BITS
# define XP_HASH_POINTER(p) (static_cast<unsigned> (reinterpret_cast<UINTPTR> (p) >> 3))
#else // SIXTY_FOUR_BITS
# define XP_HASH_POINTER(p) (static_cast<unsigned> (reinterpret_cast<UINTPTR> (p) >> 2))
#endif // SIXTY_FOUR_BITS

enum XPath_ValueType
  {
    XP_VALUE_INVALID,

    XP_VALUE_NODE,
    XP_VALUE_NODESET,
    XP_VALUE_NODELIST,
    XP_VALUE_NUMBER,
    XP_VALUE_BOOLEAN,
    XP_VALUE_STRING
  };

extern BOOL XPath_IsPrimitive (XPath_ValueType type);

#ifdef DEBUG_ENABLE_OPASSERT
extern BOOL XPath_IsValidType (XPath_ValueType type);
#endif // DEBUG_ENABLE_OPASSERT

enum XPath_Axis
  {
    XP_AXIS_ANCESTOR,
    XP_AXIS_ANCESTOR_OR_SELF,
    XP_AXIS_ATTRIBUTE,
    XP_AXIS_CHILD,
    XP_AXIS_DESCENDANT,
    XP_AXIS_DESCENDANT_OR_SELF,
    XP_AXIS_FOLLOWING,
    XP_AXIS_FOLLOWING_SIBLING,
    XP_AXIS_NAMESPACE,
    XP_AXIS_PARENT,
    XP_AXIS_PRECEDING,
    XP_AXIS_PRECEDING_SIBLING,
    XP_AXIS_SELF
  };

enum XPath_NodeTestType
  {
    XP_NODETEST_NAME,
    XP_NODETEST_NODETYPE,
    XP_NODETEST_PI,
    XP_NODETEST_HASATTRIBUTE,
    XP_NODETEST_ATTRIBUTEVALUE
  };

enum XPath_NodeType
  {
    XP_NODE_ROOT            = 0x01,
    XP_NODE_ELEMENT         = 0x02,
    XP_NODE_TEXT            = 0x04,
    XP_NODE_ATTRIBUTE       = 0x08,
    XP_NODE_NAMESPACE       = 0x10,
    XP_NODE_PI              = 0x20,
    XP_NODE_COMMENT         = 0x40,
    XP_NODE_DOCTYPE         = 0x80,

    XP_NODEMASK_SPECIAL     = 0x18,
    XP_NODEMASK_HASCHILDREN = 0x03
  };

enum XPath_ExpressionType
  {
    // PrimaryExpr
    XP_EXPR_PRIMARY,
    XP_EXPR_VARIABLE_REFERENCE,
    XP_EXPR_GROUPING,
    XP_EXPR_LITERAL,
    XP_EXPR_NUMBER,
    XP_EXPR_FUNCTION_CALL,
    XP_EXPR_LAST_FUNCTION_CALL,

    // FilterExpr
    XP_EXPR_PREDICATE,

    // PathExpr
    XP_EXPR_PATH,
    XP_EXPR_LOCATION_PATH,
    XP_EXPR_RELATIVE_PATH,
    XP_EXPR_DESCENDANTS_RELATIVE_PATH,

    // UnionExpr
    XP_EXPR_UNION,

    // UnaryExpr
    XP_EXPR_NEGATE,

    // MultiplicativeExpr
    XP_EXPR_MULTIPLICATIVE,
    XP_EXPR_MULTIPLY,
    XP_EXPR_DIVIDE,
    XP_EXPR_REMAINDER,

    // AdditiveExpr
    XP_EXPR_ADDITIVE,
    XP_EXPR_ADD,
    XP_EXPR_SUBTRACT,

    // RelationalExpr
    XP_EXPR_RELATIONAL,
    XP_EXPR_LESS_THAN,
    XP_EXPR_GREATER_THAN,
    XP_EXPR_LESS_THAN_OR_EQUAL,
    XP_EXPR_GREATER_THAN_OR_EQUAL,

    // EqualityExpr
    XP_EXPR_EQUALITY,
    XP_EXPR_EQUAL,
    XP_EXPR_NOT_EQUAL,

    // AndExpr
    XP_EXPR_AND,

    // OrExpr
    XP_EXPR_OR,
  };

#ifdef XPATH_ERRORS
class XPath_Location
{
public:
  XPath_Location ()
    : start (0xffffu),
      end (0xffffu)
  {
  }

  XPath_Location (unsigned start, unsigned end)
    : start (start & 0xffffu),
      end (end & 0xffffu)
  {
  }

  BOOL IsValid () const { return start != 0xffffu && end != 0xffffu; }

  unsigned start:16;
  unsigned end:16;
};
#endif // XPATH_ERRORS

#endif // XPDEFS_H
