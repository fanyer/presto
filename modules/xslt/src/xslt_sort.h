/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_SORT_H
#define XSLT_SORT_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_parser.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_instruction.h"
#include "modules/util/opstring.h"

class XPathExpression;
class XPathResult;
class XPathValue;
class XSLT_SortState;
class XSLT_Compiler;
class XSLT_Engine;

class XSLT_Sort
  : public XSLT_Element
{
public:
  enum Order
    {
      ORDER_ASCENDING,
      ORDER_DESCENDING
    };

  enum DataType
    {
      DATATYPE_TEXT,
      DATATYPE_NUMBER
    };

  enum CaseOrder
    {
      CASEORDER_UPPERFIRST,
      CASEORDER_LOWERFIRST
    };

  enum Attr
    { Attr_Order = 0,
      Attr_Lang,
      Attr_Datatype,
      Attr_Caseorder,
      Attr_LAST
    };

protected:
  XSLT_XPathExpression select;
  XSLT_String order;
  XSLT_String lang;
  XSLT_String datatype;
  XSLT_String caseorder;
  XSLT_Sort *next;

  BOOL is_secondary;

  static int CompareText (const OpString &text1, const OpString &text2, CaseOrder caseorder);
  static int CompareNumber (double number1, double number2);
  static int Compare (unsigned index1, unsigned index2, const OpString *textkeys, double *numberkeys, XSLT_SortState* sortstate);

  static void MergeSortL (unsigned count, unsigned *indeces, const OpString * const textkeys, double * const numberkeys, XSLT_SortState* sortstate);
  void SetIsSecondary() {is_secondary = TRUE;}

  static void SetParamValue (XSLT_Sort::Attr param_attr, const uni_char *param_value, XSLT_SortState *sortstate);
  void SetRemainingParametersL (XSLT_SortState*& sortstate, XSLT_Engine *engine) const;
  static void InitSortStateL (XSLT_SortState*& sortstate, XSLT_Engine *engine);

public:
  XSLT_Sort (XSLT_Element *parent, XSLT_Variable* previous_variable);
  virtual ~XSLT_Sort ();

  /** TRUE = finished, FALSE = Call me again */
  BOOL SortL (XSLT_Engine *engine, unsigned count, XPathNode **nodes, XSLT_SortState*& sortstate, int& instructions_left, XPathExtensions::Context* extensionscontext);

  virtual XSLT_Element *StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &name, BOOL &ignore_element);
  virtual BOOL EndElementL (XSLT_StylesheetParserImpl *parser);
  virtual void AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length);

  static void Push (XSLT_Sort *&first, XSLT_Sort *next);

  void CompileL (XSLT_Compiler *compiler);

  void SetNextParameterL (const uni_char *param_value, XSLT_SortState *&sortstate, XSLT_Instruction::Code instr, XSLT_Engine *engine) const;
};

/* Default values use for the supported sort attrbitutes: (wish we could use consts instead. ) */
#define SORT_PARAM_DEFAULT_ORDER     ORDER_ASCENDING
/* Note: none defined for 'lang', as we don't take that into account. */
#define SORT_PARAM_DEFAULT_DATATYPE  DATATYPE_TEXT
#define SORT_PARAM_DEFAULT_CASEORDER CASEORDER_UPPERFIRST

class XSLT_SortState
{
public:
  ~XSLT_SortState();

private:
  void ClearSortData();

  XSLT_SortState (XSLT_Engine *engine)
    : engine (engine),
      evaluate (0),
      index (0),
      unsortednodes (0),
      textkeys (0),
      numberkeys (0),
      indeces (0),
      has_calculated_sortvalues (FALSE),
      parameters_set (0),
      parameter_set_mask(0),
      sort_node(NULL),
      next (0)
  {
  }

  XSLT_Engine *engine;
  XPathExpression::Evaluate *evaluate;
  unsigned index;
  XPathNode **unsortednodes;
  OpString *textkeys;
  double *numberkeys;
  XPathExpression::Evaluate::Type resulttype;
  unsigned *indeces;
  BOOL has_calculated_sortvalues;
  int parameters_set;
  unsigned char parameter_set_mask;

  const XSLT_Sort *sort_node;

  XSLT_Sort::Order order; // param 1
  // Lang // param 2
  XSLT_Sort::DataType datatype; // param 3
  XSLT_Sort::CaseOrder caseorder; // param 4
  BOOL has_equal;

  // Used in recursive sorts
  XSLT_SortState *next;

  friend class XSLT_Sort;
};

#endif // XSLT_SUPPORT
#endif // XSLT_SORT_H
