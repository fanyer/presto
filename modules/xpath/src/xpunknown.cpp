/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XPATH_EXTENSION_SUPPORT

#include "modules/xpath/src/xpunknown.h"
#include "modules/xpath/src/xpapiimpl.h"
#include "modules/xpath/src/xpparser.h"
#include "modules/xpath/src/xpvalue.h"

#include "modules/util/str.h"

static unsigned
XPath_ImportValueType (unsigned type)
{
  switch (type)
    {
    case XPathVariable::TYPE_NUMBER:
      return XP_VALUE_NUMBER;

    case XPathVariable::TYPE_BOOLEAN:
      return XP_VALUE_BOOLEAN;

    case XPathVariable::TYPE_STRING:
      return XP_VALUE_STRING;

    case XPathVariable::TYPE_NODESET:
      return XP_VALUE_NODESET;

    default:
      return XP_VALUE_INVALID;
    }
}

static unsigned
XPath_ExportValueType (unsigned type)
{
  switch (type)
    {
    case XP_VALUE_NUMBER:
      return XPathVariable::TYPE_NUMBER;

    case XP_VALUE_BOOLEAN:
      return XPathVariable::TYPE_BOOLEAN;

    case XP_VALUE_STRING:
      return XPathVariable::TYPE_STRING;

    case XP_VALUE_NODESET:
      return XPathVariable::TYPE_NODESET;

    default:
      return XPathVariable::TYPE_ANY;
    }
}

/* virtual */ unsigned
XPathFunction::GetFlags ()
{
  return 0;
}

/* virtual */ unsigned
XPathVariable::GetFlags ()
{
  return 0;
}

class XPath_ValueImpl
  : public XPathValue
{
public:
  XPath_ValueImpl (XPath_Context *context, XPath_Value *storage)
    : context (context),
      value_storage (storage),
      nodelist_storage (0),
      ordered (FALSE),
      duplicates (FALSE),
      invalid (FALSE),
      pause_on_addnode (FALSE)
  {
  }

  XPath_ValueImpl (XPath_Context *context, XPath_NodeList *storage, BOOL pause_on_addnode)
    : context (context),
      value_storage (0),
      nodelist_storage (storage),
      ordered (FALSE),
      duplicates (FALSE),
      invalid (FALSE),
      pause_on_addnode (pause_on_addnode)
  {
  }

  XPath_Context *context;
  XPath_Value *value_storage;
  XPath_NodeList *nodelist_storage;
  BOOL ordered, duplicates, invalid, pause_on_addnode;
  XPath_ValueType type;

  virtual void SetNumber (double value);
  virtual void SetBoolean (BOOL value);
  virtual OP_STATUS SetString (const uni_char *value, unsigned value_length);
  virtual OP_STATUS SetNodeSet (BOOL ordered, BOOL duplicates);
  virtual OP_STATUS AddNode (XPathNode *node, AddNodeStatus &status);
};

/* virtual */ void
XPath_ValueImpl::SetNumber (double value)
{
  if (value_storage)
    {
      value_storage->type = XP_VALUE_NUMBER;
      value_storage->data.number = value;
    }
  else
    invalid = TRUE;
  type = XP_VALUE_NUMBER;
}

/* virtual */ void
XPath_ValueImpl::SetBoolean (BOOL value)
{
  if (value_storage)
    {
      value_storage->type = XP_VALUE_BOOLEAN;
      value_storage->data.boolean = !!value;
    }
  else
    invalid = TRUE;
  type = XP_VALUE_BOOLEAN;
}

/* virtual */ OP_STATUS
XPath_ValueImpl::SetString (const uni_char *value, unsigned value_length)
{
  if (value_storage)
    {
      if (value_length == ~0u)
        value_length = uni_strlen (value);

      uni_char *copy = UniSetNewStrN (value, value_length);

      if (!copy)
        return OpStatus::ERR_NO_MEMORY;

      value_storage->type = XP_VALUE_STRING;
      value_storage->data.string = copy;
    }
  else
    invalid = TRUE;

  type = XP_VALUE_STRING;
  return OpStatus::OK;
}

/* virtual */ OP_STATUS
XPath_ValueImpl::SetNodeSet (BOOL new_ordered, BOOL new_duplicates)
{
  ordered = new_duplicates;
  duplicates = new_duplicates;

  if (value_storage)
    if (ordered && !duplicates)
      {
        XPath_NodeList *nodelist = OP_NEW (XPath_NodeList, ());

        if (!nodelist)
          return OpStatus::ERR_NO_MEMORY;

        value_storage->type = XP_VALUE_NODELIST;
        value_storage->data.nodelist = nodelist;
      }
    else
      {
        XPath_NodeSet *nodeset = OP_NEW (XPath_NodeSet, ());

        if (!nodeset)
          return OpStatus::ERR_NO_MEMORY;

        value_storage->type = XP_VALUE_NODESET;
        value_storage->data.nodeset = nodeset;
      }

  type = XP_VALUE_NODESET;
  return OpStatus::OK;
}

/* virtual */ OP_STATUS
XPath_ValueImpl::AddNode (XPathNode *node, AddNodeStatus &addstatus)
{
  OP_STATUS status = OpStatus::OK;
  addstatus = pause_on_addnode ? XPathValue::ADDNODE_PAUSE : XPathValue::ADDNODE_CONTINUE;

  if (nodelist_storage)
    {
      TRAP (status, nodelist_storage->AddL (context, static_cast<XPathNodeImpl *> (node)->GetInternalNode ()));
    }
  else if (value_storage && value_storage->type == XP_VALUE_NODELIST)
    {
      TRAP (status, value_storage->data.nodelist->AddL (context, static_cast<XPathNodeImpl *> (node)->GetInternalNode ()));
    }
  else if (value_storage && value_storage->type == XP_VALUE_NODESET)
    {
      TRAP (status, value_storage->data.nodeset->AddL (context, static_cast<XPathNodeImpl *> (node)->GetInternalNode ()));
    }
  else
    addstatus = XPathValue::ADDNODE_STOP;

  XPathNodeImpl::DecRef (static_cast<XPathNodeImpl *> (node));
  return status;
}

class XPath_VariableUnknown
  : public XPath_Unknown
{
private:
  XPath_VariableReader *reader;
  unsigned index_index;
  unsigned producer_flags;
  unsigned localci_index;

  void Reset (XPath_Context *context);

public:
  XPath_VariableUnknown (XPath_Parser *parser, XPath_VariableReader *reader);

  /* From XPath_Expression: */
  virtual unsigned GetExpressionFlags ();
  virtual XPath_Value *EvaluateToValueL (XPath_Context *context, BOOL initial);

  /* From XPath_Producer: */
  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
  virtual XPath_Node *GetNextNodeL (XPath_Context *context);
  virtual unsigned GetProducerFlags ();

  /* From XPath_Unknown: */
  virtual UnknownType GetUnknownType ();
  virtual unsigned GetExternalType ();
  virtual XPath_ValueType GetActualResultTypeL (XPath_Context *context, BOOL initial);
  virtual void SetProducerFlags (unsigned producer_flags);
};

enum XPath_VariableNodeSetFlags
  {
    XPATH_VARIABLE_NODESET_UNORDERED = 2,
    XPATH_VARIABLE_NODESET_DUPLICATES = 4
  };

void
XPath_VariableUnknown::Reset (XPath_Context *context)
{
  context->states[index_index] = 0;
  context->cis[localci_index].Reset ();
}

XPath_VariableUnknown::XPath_VariableUnknown (XPath_Parser *parser, XPath_VariableReader *reader)
  : XPath_Unknown (parser),
    reader (reader),
    index_index (parser->GetStateIndex ()),
    producer_flags (0),
    localci_index (parser->GetContextInformationIndex ())
{
  ci_index = localci_index;
}

/* virtual */ unsigned
XPath_VariableUnknown::GetExpressionFlags ()
{
  unsigned flags = XPath_Unknown::GetExpressionFlags ();

  if ((reader->GetVariable ()->GetFlags () & XPathVariable::FLAG_BLOCKING) != 0)
    flags |= XPath_Expression::FLAG_BLOCKING;

  return flags;
}

/* virtual */ XPath_Value *
XPath_VariableUnknown::EvaluateToValueL (XPath_Context *context, BOOL initial)
{
  XPath_Value *value;
  unsigned *state;

  if (initial)
    Reset (context);

  reader->GetValueL (value, state, context, this, TRUE);

  return XPath_Value::IncRef (value);
}

/* virtual */ BOOL
XPath_VariableUnknown::Reset (XPath_Context *context, BOOL local_context_only)
{
  Reset (context);
  return FALSE;
}

/* virtual */ XPath_Node *
XPath_VariableUnknown::GetNextNodeL (XPath_Context *context)
{
  unsigned *state;
  XPath_Value *value;

  reader->GetValueL (value, state, context, this, FALSE);

  if (value->type == XP_VALUE_NODESET || value->type == XP_VALUE_NODELIST)
    {
      XPath_NodeSet *nodeset = value->type == XP_VALUE_NODESET ? value->data.nodeset : NULL;
      XPath_NodeList *nodelist = value->type == XP_VALUE_NODELIST ? value->data.nodelist : NULL;
      unsigned &index = context->states[index_index], count = nodeset ? nodeset->GetCount () : nodelist->GetCount ();

      if (index == 0)
        {
          if ((*state & XPATH_VARIABLE_NODESET_UNORDERED) != 0 && (producer_flags & XPath_Producer::MASK_ORDER) != 0)
            {
              if (nodeset)
                nodeset->SortL ();
              else
                nodelist->SortL ();

              *state &= ~XPATH_VARIABLE_NODESET_UNORDERED;
            }
        }

      if (index < count)
        {
          BOOL reversed = (producer_flags & XPath_Producer::FLAG_REVERSE_DOCUMENT_ORDER) != 0;

          if (nodeset)
            return XPath_Node::IncRef (nodeset->Get (index++, reversed));
          else
            return XPath_Node::IncRef (nodelist->Get (index++, reversed));
        }
    }
  else
    XPATH_EVALUATION_ERROR_WITH_TYPE ("expected node-set, got %s", this, value->type);

  return 0;
}

/* virtual */ unsigned
XPath_VariableUnknown::GetProducerFlags ()
{
  unsigned flags = XPath_Unknown::GetProducerFlags ();

  if ((reader->GetVariable ()->GetFlags () & XPathVariable::FLAG_BLOCKING) != 0)
    flags |= XPath_Producer::FLAG_BLOCKING;

  return flags;
}

/* virtual */ XPath_Unknown::UnknownType
XPath_VariableUnknown::GetUnknownType ()
{
  return UNKNOWN_VARIABLE_REFERENCE;
}

/* virtual */ unsigned
XPath_VariableUnknown::GetExternalType ()
{
  return reader->GetVariable ()->GetValueType ();
}

/* virtual */ XPath_ValueType
XPath_VariableUnknown::GetActualResultTypeL (XPath_Context *context, BOOL initial)
{
  XPath_Value *value;
  unsigned *state;

  if (initial)
    Reset (context);

  reader->GetValueL (value, state, context, this, FALSE);

  return value->type;
}

/* virtual */ void
XPath_VariableUnknown::SetProducerFlags (unsigned new_producer_flags)
{
  producer_flags = new_producer_flags;
}

/* static */ XPath_Unknown *
XPath_Unknown::MakeL (XPath_Parser *parser, XPath_VariableReader *reader)
{
  return OP_NEW_L (XPath_VariableUnknown, (parser, reader));
}

class XPath_FunctionUnknown
  : public XPath_Unknown
{
private:
  friend XPath_Unknown *XPath_Unknown::MakeL (XPath_Parser *parser, XPathFunction *function, XPath_Expression **arguments, unsigned arguments_count);

  XPathFunction *function;
  unsigned state_index, argindex_index, argtypes_index, value_index, nodelist_index, functionstate_index, localci_index, producer_flags;

  class Argument
    : public XPathExpression::Evaluate
  {
  private:
    XPath_Expression *expression;
    XPath_Producer *producer, *ordered_single, *ordered, *unordered;
    XPath_Unknown *unknown;
    XPathNode *returned_node;

    unsigned flags, state_index, actual_type_index, position_index, size_index, type_indeces[4], buffer_index, value_index, node_index, nodelist_index;
#ifdef XPATH_ERRORS
    XPath_Location location;
    BOOL failed;
#endif // XPATH_ERRORS

    OP_BOOLEAN GetActualResultType (XPath_ValueType &type);

    Argument (XPath_Parser *parser, unsigned flags, XPath_Expression *expression, XPath_Producer *producer, XPath_Producer *ordered_single, XPath_Producer *ordered, XPath_Producer *unordered, XPath_Unknown *unknown);

  public:
    static Argument *MakeL (XPath_Parser *parser, XPath_Expression *expression);

    ~Argument ();

    unsigned GetExpressionFlags () { return flags; }

    XPath_Context *current_context;
    /**< Set during calls out to the extension implementation. */

    void ResetL (XPath_Context *context);
    void ResetReturnedValue (BOOL initial);

    XPath_ValueType GetActualResultTypeL (XPath_Context *context, BOOL initial);

    virtual void SetContext (XPathNode *node, unsigned position, unsigned size);
#ifdef XPATH_EXTENSION_SUPPORT
    virtual void SetExtensionsContext (XPathExtensions::Context *extensions_context);
#endif // XPATH_EXTENSION_SUPPORT
    virtual void SetRequestedType (unsigned type);
    virtual void SetRequestedType (Type when_number, Type when_boolean, Type when_string, unsigned when_nodeset);
    virtual void SetCostLimit (unsigned limit);
    virtual unsigned GetLastOperationCost ();
    virtual OP_BOOLEAN CheckResultType (unsigned &type);
    virtual OP_BOOLEAN GetNumberResult (double &result);
    virtual OP_BOOLEAN GetBooleanResult (BOOL &result);
    virtual OP_BOOLEAN GetStringResult (const uni_char *&result);
    virtual OP_BOOLEAN GetSingleNode (XPathNode *&node);
    virtual OP_BOOLEAN GetNextNode (XPathNode *&node);
    virtual OP_BOOLEAN GetNodesCount (unsigned &count);
    virtual OP_STATUS GetNode (XPathNode *&node, unsigned index);
#ifdef XPATH_ERRORS
    virtual const uni_char *GetErrorMessage ();
#endif // XPATH_ERRORS
  };

  friend class Argument;

  XPathExpression::Evaluate **arguments;
  unsigned arguments_count;

  unsigned result_type;

  XPath_FunctionUnknown (XPath_Parser *parser, XPathFunction *function, XPathExpression::Evaluate **arguments, unsigned arguments_count);

  void SetupCallContextL (XPathFunction::CallContext &callcontext, XPath_Context *context);
  void DestroyCallContextL (XPathFunction::CallContext &callcontext);

public:
  virtual ~XPath_FunctionUnknown ();

  /* From XPath_Expression: */
  virtual unsigned GetExpressionFlags ();
  virtual XPath_Value *EvaluateToValueL (XPath_Context *context, BOOL initial);

  /* From XPath_Producer: */
  virtual BOOL Reset (XPath_Context *context, BOOL local_context_only);
  virtual XPath_Node *GetNextNodeL (XPath_Context *context);
  virtual unsigned GetProducerFlags ();

  /* From XPath_Unknown: */
  virtual UnknownType GetUnknownType ();
  virtual unsigned GetExternalType ();
  virtual XPath_ValueType GetActualResultTypeL (XPath_Context *context, BOOL initial);
  virtual void SetProducerFlags (unsigned producer_flags);
};

static OP_BOOLEAN
XPath_GetActualResultType (XPath_ValueType &type, XPath_Unknown *unknown, XPath_Context *context, BOOL initial)
{
  TRAPD (status, type = unknown->GetActualResultTypeL (context, initial));
  return status == OpStatus::OK ? OpBoolean::IS_TRUE : status;
}

OP_BOOLEAN
XPath_FunctionUnknown::Argument::GetActualResultType (XPath_ValueType &actual)
{
  unsigned &state = current_context->states[state_index];

  if (state == ~0u)
    return OpStatus::ERR_NO_MEMORY;

  if (unknown)
    if (state < 2)
      {
        BOOL initial = state == 0;

        state = 1;

        OP_BOOLEAN finished = XPath_GetActualResultType (actual, unknown, current_context, initial);

        if (finished != OpBoolean::IS_TRUE)
          return finished;

        current_context->states[actual_type_index] = actual;

        state = 2;
      }
    else
      actual = static_cast<XPath_ValueType> (current_context->states[actual_type_index]);
  else if (expression)
    actual = static_cast<XPath_ValueType> (expression->GetResultType ());
  else
    actual = XP_VALUE_NODESET;

  if (state < 2)
    state = 2;

  return OpBoolean::IS_TRUE;
}

XPath_FunctionUnknown::Argument::Argument (XPath_Parser *parser, unsigned flags, XPath_Expression *expression, XPath_Producer *producer, XPath_Producer *ordered_single, XPath_Producer *ordered, XPath_Producer *unordered, XPath_Unknown *unknown)
  : expression (expression),
    producer (producer),
    ordered_single (ordered_single),
    ordered (ordered),
    unordered (unordered),
    unknown (unknown),
    returned_node (0),
    flags (flags),
    state_index (parser->GetStateIndex ()),
    actual_type_index (parser->GetStateIndex ()),
    position_index (parser->GetStateIndex ()),
    size_index (parser->GetStateIndex ()),
    buffer_index (parser->GetBufferIndex ()),
    value_index (parser->GetValueIndex ()),
    node_index (parser->GetNodeIndex ()),
    nodelist_index (producer || unknown ? parser->GetNodeListIndex () : ~0u)
{
  for (unsigned index = 0; index < 4; ++index)
    type_indeces[index] = parser->GetStateIndex ();
}

/* static */ XPath_FunctionUnknown::Argument *
XPath_FunctionUnknown::Argument::MakeL (XPath_Parser *parser, XPath_Expression *expression)
{
  unsigned expression_flags = expression->GetExpressionFlags ();

#ifdef XPATH_ERRORS
  XPath_Location location = expression->location;
#endif // XPATH_ERRORS

  XPath_Producer *producer = 0, *ordered_single = 0, *ordered = 0, *unordered = 0;
  XPath_Unknown *unknown = 0;

  if ((expression_flags & XPath_Expression::FLAG_UNKNOWN) != 0)
    {
      producer = unknown = static_cast<XPath_Unknown *> (expression);
      expression = 0;
    }
  else if ((expression_flags & XPath_Expression::FLAG_PRODUCER) != 0)
    {
      producer = XPath_Expression::GetProducerL (parser, expression);
      expression = 0;
    }

  OpStackAutoPtr<XPath_Expression> expression_anchor (expression);
  OpStackAutoPtr<XPath_Producer> producer_anchor (producer);

  if (producer)
    {
      ordered_single = XPath_Producer::EnsureFlagsL (parser, producer, XPath_Producer::FLAG_DOCUMENT_ORDER | XPath_Producer::FLAG_SECONDARY_WRAPPER);
      ordered = XPath_Producer::EnsureFlagsL (parser, producer, XPath_Producer::FLAG_DOCUMENT_ORDER | XPath_Producer::FLAG_NO_DUPLICATES | XPath_Producer::FLAG_SECONDARY_WRAPPER);
      unordered = XPath_Producer::EnsureFlagsL (parser, producer, XPath_Producer::FLAG_NO_DUPLICATES | XPath_Producer::FLAG_SECONDARY_WRAPPER);
    }

  Argument *argument = OP_NEW_L (Argument, (parser, expression_flags, expression, producer, ordered_single, ordered, unordered, unknown));

  expression_anchor.release ();
  producer_anchor.release ();

#ifdef XPATH_ERRORS
  argument->location = location;
#endif // XPATH_ERRORS

  return argument;
}

XPath_FunctionUnknown::Argument::~Argument ()
{
  if (ordered_single != producer)
    OP_DELETE (ordered_single);
  if (ordered != producer)
    OP_DELETE (ordered);
  if (unordered != producer)
    OP_DELETE (unordered);

  OP_DELETE (expression);
  OP_DELETE (producer);
}

void
XPath_FunctionUnknown::Argument::ResetL (XPath_Context *context)
{
  context->states[state_index] = 0;
  context->states[type_indeces[XPath_Expression::WHEN_NUMBER]] = XPathExpression::Evaluate::PRIMITIVE_NUMBER;
  context->states[type_indeces[XPath_Expression::WHEN_BOOLEAN]] = XPathExpression::Evaluate::PRIMITIVE_BOOLEAN;
  context->states[type_indeces[XPath_Expression::WHEN_STRING]] = XPathExpression::Evaluate::PRIMITIVE_STRING;
  context->states[type_indeces[XPath_Expression::WHEN_NODESET]] = XPathExpression::Evaluate::NODESET_ITERATOR;
  context->nodes[node_index].CopyL (context->node);
}

void
XPath_FunctionUnknown::Argument::ResetReturnedValue (BOOL initial)
{
  if (!initial)
    XPathNodeImpl::DecRef (returned_node);
  returned_node = 0;
#ifdef XPATH_ERRORS
  failed = FALSE;
#endif // XPATH_ERRORS
}

XPath_ValueType
XPath_FunctionUnknown::Argument::GetActualResultTypeL (XPath_Context *context, BOOL initial)
{
  XPath_ValueType type;

  if (unknown)
    type = unknown->GetActualResultTypeL (context, initial);
  else if (expression)
    type = static_cast<XPath_ValueType> (expression->GetResultType ());
  else
    type = XP_VALUE_NODESET;

  return type;
}

static BOOL
XPath_ImportNode (XPath_Node &node, XPath_Context *context, XPath_Node *import)
{
  TRAPD (status, node.CopyL (import));
  return !OpStatus::IsMemoryError (status);
}

/* virtual */ void
XPath_FunctionUnknown::Argument::SetContext (XPathNode *node, unsigned position, unsigned size)
{
  OP_ASSERT (current_context->states[state_index] == 0);

  if (XPath_ImportNode (current_context->nodes[node_index], current_context, static_cast<XPathNodeImpl *> (node)->GetInternalNode ()))
    {
      current_context->states[position_index] = position;
      current_context->states[size_index] = size;
    }
  else
    /* Means failure.  Will be picked up by the next call to a function that
       can actually signal it back to someone. */
    current_context->states[state_index] = ~0u;
}

/* virtual */ void
XPath_FunctionUnknown::Argument::SetExtensionsContext (XPathExtensions::Context *extensions_context)
{
  /* Should be set on the top-level Evaluate object instead.  FIXME: Document
     this in the API too. */
  OP_ASSERT (FALSE);
}

/* virtual */ void
XPath_FunctionUnknown::Argument::SetRequestedType (unsigned type)
{
  for (unsigned index = 0; index < 4; ++index)
    current_context->states[type_indeces[index]] = type;
}

/* virtual */ void
XPath_FunctionUnknown::Argument::SetRequestedType (Type when_number, Type when_boolean, Type when_string, unsigned when_nodeset)
{
  /* Requesting a node-set when the intrinsic type is a primitive is not
     allowed. */
  OP_ASSERT ((when_number & 0xf0) == 0);
  OP_ASSERT ((when_boolean & 0xf0) == 0);
  OP_ASSERT ((when_string & 0xf0) == 0);

  current_context->states[type_indeces[XPath_Expression::WHEN_NUMBER]] = when_number;
  current_context->states[type_indeces[XPath_Expression::WHEN_BOOLEAN]] = when_boolean;
  current_context->states[type_indeces[XPath_Expression::WHEN_STRING]] = when_string;
  current_context->states[type_indeces[XPath_Expression::WHEN_NODESET]] = when_nodeset;
}

/* virtual */ void
XPath_FunctionUnknown::Argument::SetCostLimit (unsigned limit)
{
  /* FIXME: Implement this. */
}

/* virtual */ unsigned
XPath_FunctionUnknown::Argument::GetLastOperationCost ()
{
  /* FIXME: Implement this. */
  return 0;
}

/* virtual */ OP_BOOLEAN
XPath_FunctionUnknown::Argument::CheckResultType (unsigned &type)
{
  XPath_ValueType internal_type;

  OP_BOOLEAN finished = GetActualResultType (internal_type);

  if (finished != OpBoolean::IS_TRUE)
    return finished;

  switch (internal_type)
    {
    case XP_VALUE_NUMBER:
      type = current_context->states[type_indeces[XPath_Expression::WHEN_NUMBER]];
      break;

    case XP_VALUE_BOOLEAN:
      type = current_context->states[type_indeces[XPath_Expression::WHEN_BOOLEAN]];
      break;

    case XP_VALUE_STRING:
      type = current_context->states[type_indeces[XPath_Expression::WHEN_STRING]];
      break;

    default:
      type = current_context->states[type_indeces[XPath_Expression::WHEN_NODESET]];
    }

  return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
XPath_FunctionUnknown::Argument::GetNumberResult (double &result)
{
  XPath_ValueType internal_type;

  OP_BOOLEAN finished = GetActualResultType (internal_type);

  if (finished != OpBoolean::IS_TRUE)
    return finished;

#ifdef _DEBUG
  unsigned checked_type;
  OP_ASSERT (CheckResultType (checked_type) == OpBoolean::IS_TRUE && checked_type == XPathExpression::Evaluate::PRIMITIVE_NUMBER);
#endif // _DEBUG

  XPath_Context context (current_context, &current_context->nodes[node_index], current_context->states[position_index], current_context->states[size_index]);
  unsigned &state = context.states[state_index];

  if (internal_type == XP_VALUE_NODESET)
    {
      OP_ASSERT (ordered_single);

      if (state == 2)
        ordered_single->Reset (&context);

      state = 3;

      XPath_Node *node;

      finished = ordered_single->GetNextNode (node, &context);

      if (finished != OpBoolean::IS_TRUE)
        return finished;

      if (node)
        {
          TempBuffer buffer;

          OP_STATUS status = node->GetStringValue (buffer);

          XPath_Node::DecRef (&context, node);

          RETURN_IF_ERROR (status);

          result = XPath_Value::AsNumber (buffer.GetStorage ());
        }
      else
        result = op_nan (0);
    }
  else
    {
      XPath_Expression *use_expression = unknown ? unknown : expression;

      BOOL initial = state == 2;

      state = 3;

      XPath_Value *value;
      XPath_ValueType conversion[4] = { XP_VALUE_NUMBER, XP_VALUE_NUMBER, XP_VALUE_NUMBER, XP_VALUE_INVALID };

      finished = use_expression->Evaluate (value, &context, initial, conversion);

      if (finished != OpBoolean::IS_TRUE)
        return finished;

      /* FIXME: Need to trap here, but really only if value is a node-set,
         which we know it isn't. */
      result = value->AsNumberL ();

      XPath_Value::DecRef (&context, value);
    }

  return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
XPath_FunctionUnknown::Argument::GetBooleanResult (BOOL &result)
{
  XPath_ValueType internal_type;

  OP_BOOLEAN finished = GetActualResultType (internal_type);

  if (finished != OpBoolean::IS_TRUE)
    return finished;

#ifdef _DEBUG
  unsigned checked_type;
  OP_ASSERT (CheckResultType (checked_type) == OpBoolean::IS_TRUE && checked_type == XPathExpression::Evaluate::PRIMITIVE_BOOLEAN);
#endif // _DEBUG

  XPath_Context context (current_context, &current_context->nodes[node_index], current_context->states[position_index], current_context->states[size_index]);
  unsigned &state = context.states[state_index];

  if (internal_type == XP_VALUE_NODESET)
    {
      OP_ASSERT (producer);

      if (state == 2)
        producer->Reset (&context);

      state = 3;

      XPath_Node *node;

      finished = producer->GetNextNode (node, &context);

      if (finished != OpBoolean::IS_TRUE)
        return finished;

      result = node != 0;

      XPath_Node::DecRef (&context, node);
    }
  else
    {
      XPath_Expression *use_expression = unknown ? unknown : expression;

      BOOL initial = state == 2;

      state = 3;

      XPath_Value *value;
      XPath_ValueType conversion[4] = { XP_VALUE_BOOLEAN, XP_VALUE_BOOLEAN, XP_VALUE_BOOLEAN, XP_VALUE_INVALID };

      finished = use_expression->Evaluate (value, &context, initial, conversion);

      if (finished != OpBoolean::IS_TRUE)
        return finished;

      result = value->AsBoolean ();

      XPath_Value::DecRef (&context, value);
    }

  return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
XPath_FunctionUnknown::Argument::GetStringResult (const uni_char *&result)
{
  XPath_ValueType internal_type;

  OP_BOOLEAN finished = GetActualResultType (internal_type);

  if (finished != OpBoolean::IS_TRUE)
    return finished;

#ifdef _DEBUG
  unsigned checked_type;
  OP_ASSERT (CheckResultType (checked_type) == OpBoolean::IS_TRUE);
  OP_ASSERT (checked_type == XPathExpression::Evaluate::PRIMITIVE_STRING);
#endif // _DEBUG

  XPath_Context context (current_context, &current_context->nodes[node_index], current_context->states[position_index], current_context->states[size_index]);
  unsigned &state = context.states[state_index];

  if (internal_type == XP_VALUE_NODESET)
    {
      OP_ASSERT (ordered_single);

      if (state == 2)
        ordered_single->Reset (&context);

      state = 3;

      XPath_Node *node;

      finished = ordered_single->GetNextNode (node, &context);

      if (finished != OpBoolean::IS_TRUE)
        return finished;

      if (node)
        {
          TempBuffer &buffer = context.buffers[buffer_index];

          OP_STATUS status = node->GetStringValue (buffer);

          XPath_Node::DecRef (&context, node);

          RETURN_IF_ERROR (status);

          result = buffer.GetStorage ();
        }
      else
        result = UNI_L ("");
    }
  else
    {
      XPath_Expression *use_expression = unknown ? unknown : expression;

      BOOL initial = state == 2;

      state = 3;

      XPath_Value *&value = context.values[value_index];

      if (value)
        {
          XPath_Value::DecRef (&context, value);
          value = 0;
        }

      XPath_ValueType conversion[4] = { XP_VALUE_STRING, XP_VALUE_STRING, XP_VALUE_STRING, XP_VALUE_INVALID };

      finished = use_expression->Evaluate (value, &context, initial, conversion);

      if (finished != OpBoolean::IS_TRUE)
        return finished;

      result = value->data.string;
    }

  return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
XPath_FunctionUnknown::Argument::GetSingleNode (XPathNode *&node0)
{
  unsigned &state = current_context->states[state_index];

  if (state != 4)
    {
      OP_BOOLEAN finished = GetNextNode (node0);

      if (finished != OpBoolean::IS_TRUE)
        return finished;

      state = 4;
    }
  else
    node0 = returned_node;

  return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
XPath_FunctionUnknown::Argument::GetNextNode (XPathNode *&node0)
{
  XPathNodeImpl::DecRef (returned_node);

  XPath_ValueType internal_type;

  OP_BOOLEAN finished = GetActualResultType (internal_type);

  if (finished != OpBoolean::IS_TRUE)
    return finished;

  if (internal_type != XP_VALUE_NODESET)
    {
#ifdef XPATH_ERRORS
      failed = TRUE;
      return current_context->SetError ("expected node-set", location);
#else // XPATH_ERRORS
      return OpStatus::ERR;
#endif // XPATH_ERRORS
    }

  XPath_Context context (current_context, &current_context->nodes[node_index], current_context->states[position_index], current_context->states[size_index]);
  unsigned &state = context.states[state_index];

  OP_ASSERT (ordered_single);

  XPath_Producer *use_producer;

  if ((current_context->states[type_indeces[XPath_Expression::WHEN_NODESET]] & XPathExpression::Evaluate::NODESET_FLAG_ORDERED) != 0)
    use_producer = ordered;
  else
    use_producer = unordered;

  if (state == 2)
    use_producer->Reset (&context);

  state = 3;

  XPath_Node *node;

  finished = use_producer->GetNextNode (node, &context);

  if (finished != OpBoolean::IS_TRUE)
    return finished;

  if (node)
    {
      RETURN_IF_ERROR (XPathNodeImpl::Make (node0, node, current_context->global));
      returned_node = static_cast<XPathNodeImpl *> (node0);
    }
  else
    node0 = 0;

  return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
XPath_FunctionUnknown::Argument::GetNodesCount (unsigned &count)
{
  OP_ASSERT (!"Not supported!");
  return OpStatus::ERR;
}

/* virtual */ OP_STATUS
XPath_FunctionUnknown::Argument::GetNode (XPathNode *&node, unsigned index)
{
  OP_ASSERT (!"Not supported!");
  return OpStatus::ERR;
}

#ifdef XPATH_ERRORS

/* virtual */ const uni_char *
XPath_FunctionUnknown::Argument::GetErrorMessage ()
{
  if (failed)
    {
      if (OpString *error_message = current_context->global->error_message)
        if (!error_message->IsEmpty ())
          return error_message->CStr ();
      return UNI_L ("unknown error");
    }
  else
    return 0;
}

#endif // XPATH_ERRORS

XPath_FunctionUnknown::XPath_FunctionUnknown (XPath_Parser *parser, XPathFunction *function, XPathExpression::Evaluate **arguments, unsigned arguments_count)
  : XPath_Unknown (parser),
    function (function),
    state_index (parser->GetStateIndex ()),
    argindex_index (parser->GetStateIndex ()),
    value_index (parser->GetValueIndex ()),
    nodelist_index (parser->GetNodeListIndex ()),
    functionstate_index (parser->GetFunctionStateIndex ()),
    localci_index (parser->GetContextInformationIndex ()),
    producer_flags (0),
    arguments (arguments),
    arguments_count (0),
    result_type (XPathFunction::TYPE_INVALID)
{
  if (arguments_count != 0)
    {
      /* Allocate one state for each argument, to hold its argument type in
         GetActualResultTypeL ().  We rely on the parser to give us consequtive
         numbers, because we will use them as an unsigned[]. */

      argtypes_index = parser->GetStateIndex ();
      for (unsigned index = 1; index < arguments_count; ++index)
        parser->GetStateIndex ();
    }

  ci_index = localci_index;
}

void
XPath_FunctionUnknown::SetupCallContextL (XPathFunction::CallContext &callcontext, XPath_Context *context)
{
  LEAVE_IF_ERROR (XPathNodeImpl::Make (callcontext.context_node, XPath_Node::IncRef (context->node), context->global));

  unsigned flags = function->GetFlags ();
  callcontext.context_position = (flags & XPathFunction::FLAG_NEEDS_CONTEXT_POSITION) != 0 ? context->position : 0;
  callcontext.context_size = (flags & XPathFunction::FLAG_NEEDS_CONTEXT_SIZE) != 0 ? context->size : 0;

  callcontext.arguments = arguments;
  callcontext.arguments_count = arguments_count;

  for (unsigned index = 0; index < arguments_count; ++index)
    {
      static_cast<Argument *> (arguments[index])->current_context = context;
      static_cast<Argument *> (arguments[index])->ResetReturnedValue (TRUE);
    }
}

void
XPath_FunctionUnknown::DestroyCallContextL (XPathFunction::CallContext &callcontext)
{
  XPathNodeImpl::DecRef (static_cast<XPathNodeImpl *> (callcontext.context_node));

  for (unsigned index = 0; index < arguments_count; ++index)
    {
      static_cast<Argument *> (arguments[index])->current_context = 0;
      static_cast<Argument *> (arguments[index])->ResetReturnedValue (FALSE);
    }
}

/* virtual */
XPath_FunctionUnknown::~XPath_FunctionUnknown ()
{
  OP_DELETE (function);
  for (unsigned index = 0; index < arguments_count; ++index)
    {
      XPath_FunctionUnknown::Argument *argument = static_cast<XPath_FunctionUnknown::Argument *> (arguments[index]);
      OP_DELETE (argument);
    }
  OP_DELETEA (arguments);
}

/* virtual */ unsigned
XPath_FunctionUnknown::GetExpressionFlags ()
{
  unsigned flags = XPath_Unknown::GetExpressionFlags (), function_flags = function->GetFlags ();

  for (unsigned index = 0; index < arguments_count; ++index)
    flags |= static_cast<Argument *> (arguments[index])->GetExpressionFlags () & XPath_Expression::MASK_INHERITED;

  if ((function_flags & XPathFunction::FLAG_NEEDS_CONTEXT_POSITION) != 0)
    flags |= XPath_Expression::FLAG_CONTEXT_POSITION;
  if ((function_flags & XPathFunction::FLAG_NEEDS_CONTEXT_SIZE) != 0)
    flags |= XPath_Expression::FLAG_CONTEXT_SIZE;
  if ((function_flags & XPathFunction::FLAG_BLOCKING) != 0)
    flags |= XPath_Expression::FLAG_BLOCKING;

  return flags;
}

/* virtual */ XPath_Value *
XPath_FunctionUnknown::EvaluateToValueL (XPath_Context *context, BOOL initial)
{
  XPath_Value *value;

  if (initial)
    Reset (context, FALSE);

  if (XPath_Value *existing = context->values[value_index])
    value = existing;
  else
    context->values[value_index] = value = XPath_Value::MakeL (context, XP_VALUE_INVALID);

  XPath_ValueImpl valueimpl (context, value);

  XPathFunction::CallContext callcontext;
  SetupCallContextL (callcontext, context);

  XPathFunction::State *&functionstate = context->functionstates[functionstate_index];
  XPathFunction::Result result;

  do
    result = function->Call (valueimpl, context->global->extensions_context, &callcontext, functionstate);
  while (result == XPathFunction::RESULT_PAUSED);

  DestroyCallContextL (callcontext);

  if (result == XPathFunction::RESULT_BLOCKED)
    LEAVE (OpBoolean::IS_FALSE);

  OP_DELETE (functionstate);
  functionstate = 0;

  if (result == XPathFunction::RESULT_FAILED || result == XPathFunction::RESULT_OOM)
    {
      value->Reset (context);
      LEAVE (result == XPathFunction::RESULT_FAILED ? OpStatus::ERR : OpStatus::ERR_NO_MEMORY);
    }

  return XPath_Value::IncRef (value);
}

/* virtual */ BOOL
XPath_FunctionUnknown::Reset (XPath_Context *context, BOOL local_context_only)
{
  context->states[state_index] = 0;
  context->states[argindex_index] = 0;
  context->nodelists[nodelist_index].Clear (context);
  context->cis[localci_index].Reset ();

  OP_DELETE (context->functionstates[functionstate_index]);
  context->functionstates[functionstate_index] = 0;

  if (XPath_Value *value = context->values[value_index])
    value->Reset (context);

  for (unsigned index = 0; index < arguments_count; ++index)
    {
      static_cast<Argument *> (arguments[index])->ResetL (context);
      context->states[argtypes_index + index] = XP_VALUE_INVALID;
    }

  return FALSE;
}

/* virtual */ XPath_Node *
XPath_FunctionUnknown::GetNextNodeL (XPath_Context *context)
{
  unsigned &state = context->states[state_index];
  XPath_NodeList &nodelist = context->nodelists[nodelist_index];

  if (state == 0)
    {
      Reset (context, FALSE);

    again:
      state = 1;
    }

  if (state == 1)
    {
      XPath_ValueImpl valueimpl (context, &nodelist, TRUE);

      XPathFunction::CallContext callcontext;
      SetupCallContextL (callcontext, context);

      XPathFunction::State *&functionstate = context->functionstates[functionstate_index];

      XPathFunction::Result result = function->Call (valueimpl, context->global->extensions_context, &callcontext, functionstate);

      DestroyCallContextL (callcontext);

      if (result != XPathFunction::RESULT_PAUSED && result != XPathFunction::RESULT_BLOCKED || valueimpl.invalid)
        {
          OP_DELETE (functionstate);
          functionstate = 0;
        }

      if (valueimpl.invalid)
        XPATH_EVALUATION_ERROR_WITH_TYPE ("expected node-set, got %s", this, valueimpl.type);

      if (result == XPathFunction::RESULT_FAILED || result == XPathFunction::RESULT_OOM)
        LEAVE (result == XPathFunction::RESULT_FAILED ? OpStatus::ERR : OpStatus::ERR_NO_MEMORY);

      if (result == XPathFunction::RESULT_PAUSED)
        state = 2;
      else if (result == XPathFunction::RESULT_BLOCKED)
        state = 3;
      else
        state = 4;
    }

  if (nodelist.GetCount () != 0)
    return nodelist.Pop (0);
  else if (state == 2)
    goto again;
  else if (state == 3)
    {
      state = 1;
      LEAVE (OpBoolean::IS_FALSE);
    }

  return 0;
}

/* virtual */ unsigned
XPath_FunctionUnknown::GetProducerFlags ()
{
  unsigned flags = XPath_Unknown::GetProducerFlags (), function_flags = function->GetFlags ();

  for (unsigned index = 0; index < arguments_count; ++index)
    {
      unsigned argument_flags = static_cast<Argument *> (arguments[index])->GetExpressionFlags () & XPath_Expression::MASK_INHERITED;

      if ((argument_flags & XPath_Expression::FLAG_CONTEXT_SIZE) != 0)
        flags |= XPath_Producer::FLAG_CONTEXT_SIZE;
      if ((argument_flags & XPath_Expression::FLAG_CONTEXT_POSITION) != 0)
        flags |= XPath_Producer::FLAG_CONTEXT_POSITION;
      if ((argument_flags & XPath_Expression::FLAG_BLOCKING) != 0)
        flags |= XPath_Producer::FLAG_BLOCKING;
    }

  if ((function_flags & XPathFunction::FLAG_NEEDS_CONTEXT_POSITION) != 0)
    flags |= XPath_Producer::FLAG_CONTEXT_POSITION;
  if ((function_flags & XPathFunction::FLAG_NEEDS_CONTEXT_SIZE) != 0)
    flags |= XPath_Producer::FLAG_CONTEXT_SIZE;
  if ((function_flags & XPathFunction::FLAG_BLOCKING) != 0)
    flags |= XPath_Producer::FLAG_BLOCKING;

  return flags;
}

/* virtual */ XPath_Unknown::UnknownType
XPath_FunctionUnknown::GetUnknownType ()
{
  return UNKNOWN_FUNCTION_CALL;
}

/* virtual */ unsigned
XPath_FunctionUnknown::GetExternalType ()
{
  return result_type;
}

/* virtual */ XPath_ValueType
XPath_FunctionUnknown::GetActualResultTypeL (XPath_Context *context, BOOL initial)
{
  unsigned &argument_index = context->states[argindex_index], *argument_types = context->states + argtypes_index;
  BOOL argument_initial = FALSE;

  if (initial)
    {
      Reset (context, FALSE);
      argument_initial = TRUE;
    }

  for (; argument_index < arguments_count; ++argument_index)
    {
      unsigned argument_type = XPathFunction::TYPE_ANY;

      switch (static_cast<Argument *> (arguments[argument_index])->GetActualResultTypeL (context, argument_initial))
        {
        case XP_VALUE_NUMBER:
          argument_type = XPathFunction::TYPE_NUMBER;
          break;

        case XP_VALUE_BOOLEAN:
          argument_type = XPathFunction::TYPE_BOOLEAN;
          break;

        case XP_VALUE_STRING:
          argument_type = XPathFunction::TYPE_STRING;
          break;

        default:
          argument_type = XPathFunction::TYPE_NODESET;
        }

      argument_types[argument_index] = argument_type;
      argument_initial = TRUE;
    }

  switch (function->GetResultType (argument_types, arguments_count))
    {
    case XPathFunction::TYPE_NUMBER:
      return XP_VALUE_NUMBER;

    case XPathFunction::TYPE_BOOLEAN:
      return XP_VALUE_BOOLEAN;

    case XPathFunction::TYPE_STRING:
      return XP_VALUE_STRING;

    case XPathFunction::TYPE_NODESET:
      return XP_VALUE_NODESET;

    default:
      /* FIXME: Function call needs to be started (and possibly
         finished) before we can say what it returns. */
      OP_ASSERT (FALSE);
      LEAVE (OpStatus::ERR);
    }

  return XP_VALUE_INVALID;
}

/* virtual */ void
XPath_FunctionUnknown::SetProducerFlags (unsigned flags)
{
  producer_flags = flags;
}

static unsigned
XPath_GetArgumentType (XPath_Expression *expression)
{
  if (expression->HasFlag (XPath_Expression::FLAG_UNKNOWN))
    return static_cast<XPath_Unknown *> (expression)->GetExternalType ();
  else
    return XPath_ExportValueType (expression->GetResultType ());
}

/* static */ XPath_Unknown *
XPath_Unknown::MakeL (XPath_Parser *parser, XPathFunction *function, XPath_Expression **arguments0, unsigned arguments_count)
{
  XPathExpression::Evaluate **arguments = OP_NEWA_L (XPathExpression::Evaluate *, arguments_count);

  XPath_FunctionUnknown *unknown = OP_NEW_L (XPath_FunctionUnknown, (parser, function, arguments, arguments_count));
  OpStackAutoPtr<XPath_FunctionUnknown> unknown_anchor (unknown);

  unsigned *argument_types = OP_NEWA_L (unsigned, arguments_count);
  ANCHOR_ARRAY (unsigned, argument_types);

  for (unsigned index = 0; index < arguments_count; ++index)
    {
      XPath_Expression *argument = arguments0[index];
      arguments0[index] = 0;

      argument_types[index] = XPath_GetArgumentType (argument);
      arguments[index] = XPath_FunctionUnknown::Argument::MakeL (parser, argument);

      ++unknown->arguments_count;
    }

  unknown->result_type = function->GetResultType (argument_types, arguments_count);

  if (unknown->result_type == XPathFunction::TYPE_INVALID)
    XPATH_COMPILATION_ERROR_NAME ("invalid arguments to function: ''", parser->GetCurrentLocation ());

  unknown_anchor.release ();
  return unknown;
}

/* virtual */ unsigned
XPath_Unknown::GetResultType ()
{
  return XPath_ImportValueType (GetExternalType ());
}

/* virtual */ unsigned
XPath_Unknown::GetExpressionFlags ()
{
  return XPath_Expression::FLAG_PRODUCER | XPath_Expression::FLAG_UNKNOWN | XPath_Expression::FLAG_COMPLEXITY_COMPLEX;
}

/* virtual */ unsigned
XPath_Unknown::GetProducerFlags ()
{
  return XPath_Producer::FLAG_UNKNOWN;
}

XPath_VariableReader::XPath_VariableReader (XPath_Parser *parser, XPathVariable *variable)
  : variable (variable),
    state_index (parser->GetStateIndex ()),
    variablestate_index (parser->GetVariableStateIndex ()),
    value_index (parser->GetValueIndex ()),
    next (0)
{
}

/* static */ XPath_VariableReader *
XPath_VariableReader::MakeL (XPath_Parser *parser, const XMLExpandedName &name, XPathVariable *variable)
{
  XPath_VariableReader *reader = OP_NEW (XPath_VariableReader, (parser, variable));

  if (!reader)
    {
      OP_DELETE (variable);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  if (OpStatus::IsMemoryError (reader->name.Set (name)))
    {
      OP_DELETE (reader);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  return reader;
}

XPath_VariableReader::~XPath_VariableReader ()
{
  OP_DELETE (variable);
  OP_DELETE (next);
}

void
XPath_VariableReader::Start (XPath_Context *context)
{
  context->states[state_index] = 0;
  context->variablestates[variablestate_index] = 0;
  context->values[value_index] = 0;
}

void
XPath_VariableReader::Finish (XPath_Context *context)
{
  XPath_Value::DecRef (context, context->values[value_index]);
}

void
XPath_VariableReader::GetValueL (XPath_Value *&value, unsigned *&state0, XPath_Context *context, XPath_Expression *caller, BOOL full)
{
  if (XPath_Value *existing = context->values[value_index])
    value = existing;
  else
    context->values[value_index] = value = XPath_Value::MakeL (context, XP_VALUE_INVALID);

  if (context->states[state_index] == 0)
    {
      XPath_ValueImpl valueimpl (context, value);

    again:
      switch (variable->GetValue (valueimpl, context->global->extensions_context, context->variablestates[variablestate_index]))
        {
        case XPathVariable::RESULT_FINISHED:
          break;

        case XPathVariable::RESULT_PAUSED:
          if (full || !valueimpl.ordered)
            goto again;
          else
            break;

        case XPathVariable::RESULT_BLOCKED:
          LEAVE (OpBoolean::IS_FALSE);

        case XPathVariable::RESULT_FAILED:
          XPATH_EVALUATION_ERROR ("failure to read variable value", caller);

        case XPathVariable::RESULT_OOM:
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }

      OP_DELETE (context->variablestates[variablestate_index]);
      context->variablestates[variablestate_index] = 0;

      unsigned state = 1;

      if (!valueimpl.ordered)
        state |= XPATH_VARIABLE_NODESET_UNORDERED;
      if (valueimpl.duplicates)
        state |= XPATH_VARIABLE_NODESET_DUPLICATES;

      context->states[state_index] = state;
    }

  state0 = &context->states[state_index];
}

#endif // XPATH_EXTENSION_SUPPORT
