/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_ENGINE_H
#define XSLT_ENGINE_H

#ifdef XSLT_SUPPORT

#include "modules/xpath/xpath.h"
#include "modules/xslt/src/xslt_outputhandler.h"
#include "modules/util/OpHashTable.h"

class XSLT_TransformationImpl;
class XSLT_Program;
class XSLT_NodeList;
class XSLT_Tree;
class XSLT_Sort;
class XSLT_Number;
class XSLT_SortState;
class XSLT_VariableValue;
class XSLT_CopyOf;
class XSLT_Engine;
class XPathValue;

/**
 * Stores current values of variables. Could make it layered
 * so that we can delete value as soon as we leave scope by
 * deleting that scope's layer.
 */
class XSLT_VariableStore
  : private OpHashTable
{
public:
  XSLT_VariableStore (XSLT_VariableStore *next = 0) : next (next) {}
  ~XSLT_VariableStore ();

  /**
   * Takes ownership of |value|. Replaces any previous value.
   */
  void SetVariableValueL (const XSLT_Variable *variable_elm, XSLT_VariableValue *value);

  /**
   * Returns the value of the element. Can it be NULL? I can't think
   * of such a case since we, if the value hadn't been in scope wouldn't
   * have found variable_elm in the variable lookup.
   */
  XSLT_VariableValue *GetVariableValue (const XSLT_Variable *variable_elm);


  /**
   * Looks up a with_param (well, type is probably not checked) with the name supplied and returns it. NULL if nothing found.
   */
  XSLT_Variable *GetWithParamL (const XMLExpandedName &name);

  /**
   * Copies a value from one variable store to another.
   */
  void CopyValueFromOtherL (const XSLT_Variable *variable_to_set, XSLT_VariableStore *other_store, const XSLT_Variable *with_param_to_steal);

  static void PushL (XSLT_VariableStore *&store);
  static void Pop (XSLT_VariableStore *&store);

private:
  XSLT_VariableStore *next;
};

class XSLT_KeyValue
{
private:
  OpString value;
  OpVector<XPathNode> nodes;

public:
  static XSLT_KeyValue *MakeL (const uni_char *value, XPathNode *node);
  ~XSLT_KeyValue ();

  const uni_char *GetValue () { return value.CStr (); }

  void AddNodeL (XPathNode *node);
  void FindNodesL (XPathValue *result);
};

class XSLT_KeyValueTableBase
{
protected:
  XSLT_KeyValueTableBase (XMLTreeAccessor *tree)
    : tree (tree)
  {
  }

  XMLExpandedName name;
  XMLTreeAccessor *tree;

public:
  XSLT_KeyValueTableBase (const XMLExpandedName name, XMLTreeAccessor *tree)
    : name (name),
      tree (tree)
  {
  }

  UINT32 Hash () const;
  BOOL Equals (const XSLT_KeyValueTableBase *other) const;
};

/** Table of value => nodes mappings. */
class XSLT_KeyValueTable
  : public XSLT_KeyValueTableBase,
    private OpAutoStringHashTable<XSLT_KeyValue>
{
private:
  XSLT_KeyValueTable (XMLTreeAccessor *tree)
    : XSLT_KeyValueTableBase (tree)
  {
  }

public:
  static XSLT_KeyValueTable *MakeL (const XMLExpandedName &name, XMLTreeAccessor *tree);

  void AddNodeL (const uni_char *value, XPathNode *node);
  void FindNodesL (const uni_char *value, XPathValue *result);
};

/** Table of {tree, key} => XSLT_KeyValueTable mappings. */
class XSLT_KeyTable
  : private OpHashTable
{
private:
  XSLT_Engine *engine;

  /* From OpHashTable: */
  virtual void Delete (void *value);
  virtual UINT32 Hash (const void *key);
  virtual BOOL KeysAreEqual (const void *key1, const void *key2);

public:
  XSLT_KeyTable (XSLT_Engine *engine)
    : OpHashTable (this),
      engine (engine)
  {
  }

  virtual ~XSLT_KeyTable ();

  void AddValueL (const XMLExpandedName &name, const uni_char *string, XPathNode *node);
  BOOL FindNodesL (XSLT_Engine *engine, const XMLExpandedName &name, XMLTreeAccessor *tree, const uni_char *string, XPathValue *result);
};


class XSLT_Engine
  : private XPathExtensions::Context /* so that we can send it through to xpath functions. */
{
protected:
  class ProgramState
  {
  public:
    ProgramState (XSLT_Program* program, ProgramState* previous_state);
    ~ProgramState ();

    XSLT_Program *program;
    unsigned instruction_index;
    UINTPTR *instruction_states;

    TempBuffer string_buffer;
    double current_number;
    BOOL current_boolean;
    const uni_char *current_string;
    XMLCompleteName current_name;

    XPathExpression::Evaluate *evaluate;
    XPathPattern::Match *match;
    XPathPattern::Count *count;
    XPathPattern::Search *search;
#ifdef XSLT_ERRORS
    XSLT_XPathExpressionOrPattern *xpath_wrapper;
#endif // XSLT_ERRORS

    XPathNode *context_node, *current_node;
    unsigned context_position, context_size;

    XSLT_NodeList *nodelist;
    XSLT_Tree *tree;
    XMLTreeAccessor::Node *treenode;
    unsigned node_index;

    XSLT_SortState* sortstate;

    BOOL owns_variables;
    XSLT_VariableStore *variables;

    XSLT_VariableStore *caller_supplied_params;
    XSLT_VariableStore *params_collected_for_call;

    ProgramState *previous_state;
  } *current_state;

  unsigned recursion_depth;

  XSLT_TransformationImpl *transformation;
  XSLT_OutputHandler *output_handler;
  int instructions_left_in_slice;
  BOOL is_blocked, is_program_interrupted;

  BOOL uncertain_output_method;
  BOOL need_output_handler_switch;
  BOOL auto_detected_html;
  XSLT_OutputHandler *root_output_handler;

  XSLT_VariableStore global_variables;
  XSLT_KeyTable keytable;

#ifdef XSLT_ERRORS
  OpString error_detail;
#endif // XSLT_ERRORS

  class LocalOutputHandler
    : public XSLT_OutputHandler
  {
  public:
    enum Type { TYPE_COLLECTTEXT, TYPE_COLLECTRESULTTREEFRAGMENT } type;

    LocalOutputHandler(Type type)
      : type (type)
    {
    }
  };

  class CollectText
    : public LocalOutputHandler
  {
  protected:
    XSLT_OutputHandler *previous_output_handler;
    TempBuffer buffer;
    unsigned ignore_level;

    const uni_char* GetCollectedText() { return buffer.GetStorage(); }

  public:
    CollectText ()
      : LocalOutputHandler (TYPE_COLLECTTEXT),
        ignore_level (0)
    {
    }

    static XSLT_OutputHandler *PushL (XSLT_OutputHandler *previous_output_handler);
    static XSLT_OutputHandler *PopL (XSLT_OutputHandler *output_handler, TempBuffer &result);
    static XSLT_OutputHandler *Pop (XSLT_OutputHandler *output_handler);

    virtual void StartElementL (const XMLCompleteName &name);
    virtual void AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified);
    virtual void AddTextL (const uni_char *data, BOOL disable_output_escaping);
    virtual void AddCommentL (const uni_char *data);
    virtual void AddProcessingInstructionL (const uni_char *target, const uni_char *data);
    virtual void EndElementL (const XMLCompleteName &name);
    virtual void EndOutputL ();
  };

  class CollectResultTreeFragment
    : public LocalOutputHandler
  {
  protected:
    XSLT_OutputHandler *previous_output_handler;
    XSLT_Tree *tree;

  public:
    CollectResultTreeFragment ()
      : LocalOutputHandler (TYPE_COLLECTRESULTTREEFRAGMENT)
    {
    }

    static XSLT_OutputHandler *PushL (XSLT_OutputHandler *previous_output_handler);
    static XSLT_OutputHandler *Pop (XSLT_OutputHandler *output_handler, XSLT_Tree **tree);

    virtual void StartElementL (const XMLCompleteName &name);
    virtual void AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified);
    virtual void AddTextL (const uni_char *data, BOOL disable_output_escaping);
    virtual void AddCommentL (const uni_char *data);
    virtual void AddProcessingInstructionL (const uni_char *target, const uni_char *data);
    virtual void EndElementL (const XMLCompleteName &name);
    virtual void EndOutputL ();
  };

  /* Instruction state access. */
  UINTPTR &GetInstructionStateL ();

  /* String access. */
  void SetString (const uni_char *string);
  void AppendStringL (const uni_char *string);
  const uni_char *GetString ();
  void ClearString ();

  /* Name access. */
  void SetName (const XMLCompleteName &name);
  BOOL SetQNameL ();
  void SetURIL ();
  void ResolveNameL (XMLNamespaceDeclaration *nsdeclaration, BOOL use_default, XSLT_Instruction *instruction);
  void SetNameFromNode ();
  const XMLCompleteName &GetName ();

  /* Element name stack. */
  XMLCompleteName **element_name_stack;
  unsigned element_name_stack_used, element_name_stack_total;

  void PushElementNameL (const XMLCompleteName &name);
  void PopElementName ();
  const XMLCompleteName &GetCurrentElementName ();

  /* Output. */
  void StartElementL (const XMLCompleteName &name, BOOL push);
  void AddAttributeL (const XMLCompleteName &name);
  void SuggestNamespaceDeclarationL (XSLT_Element *element);
  void AddTextL (XSLT_Instruction *instruction);
  void AddCommentL ();
  void AddProcessingInstructionL ();
  void EndElementL (const XMLCompleteName *name);

  void SendMessageL (XSLT_Instruction *instruction);

  /** Returns FALSE if the sort wasn't fully done and we need to
      continue later, TRUE if the sort is complete.
  */
  BOOL SortL (XSLT_Sort *sort);

  void SetSortParameterFromStringL (XSLT_Sort *sort, XSLT_Instruction::Code instr);

  /** Returns FALSE if the sort wasn't fully done and we need to
      continue later, TRUE if the sort is complete.
  */
  BOOL AddFormattedNumberL (XSLT_Number *number);

  /**
   * Will register the value of the current node in this key for faster lookup in the key method.
   * Returns FALSE if we need to come back later to this instruction.
   */
  BOOL ProcessKeyL(XSLT_Key *key);

  void TestAndSetIfParamIsPresetL(const XSLT_Variable* variable);
  /** Returns FALSE if the variable wasn't fully set and we need to
      continue later, TRUE if it was fully set.
  */
  BOOL SetVariableL(XSLT_Instruction::Code instruction,
                    const XSLT_Variable* variable);

  BOOL CreateVariableValueFromEvaluateL(XSLT_VariableValue*& value);

  /**
   * Returns TRUE if finished, FALSE if not yet finished.
   */
  BOOL AddCopyOfEvaluateL (XSLT_CopyOf *copy_of);

  /**
   * Returns the type copied. If type is element then we should jump past the next instruction. If the type is root, then we should just go to the next instruction. In all other cases we should jump per the argument of the instruction.
   *
   * @param node The node to copy.
   */
  XPathNode::Type AddCopyL (XPathNode *node);

  void ExecuteProgramL ();

  BOOL CalculateContextSizeL ();

  enum
    {
      // See EvaluateExpressionL below
      EVALUATE_TYPE_KEY = -1
    };
  /**
   * if type >= 0 then it's a XPathExpression::Evaluate::Type, else it has some special meaning. -1/EVALUATE_TYPE_KEY is "key".
   */
  BOOL EvaluateExpressionL (int type, unsigned expression_index);
  BOOL MatchPatternL (unsigned pattern_index, unsigned patterns_count);
  void SearchPatternL (UINTPTR argument);
  BOOL CountPatternsAndAddL (const XSLT_Number *number_elm);

  /**
   // Helper method for all CallProgramOn*
   * @param take_context_node if TRUE then this method takes care of context_node regardless of return status
   */
  void CallProgramL (XPathNode* context_node, BOOL take_context_node, XSLT_Program *program, BOOL open_new_variable_scope);
  void CallProgramL (XPathNode* context_node, BOOL take_context_node, unsigned program_index, BOOL open_new_variable_scope);

  BOOL CallProgramOnNodeL (unsigned program_index, BOOL open_new_variable_scope, BOOL copy_context);

  BOOL GetNextNodeL (XPathNode *&node, BOOL &owns_node, unsigned &position, unsigned &size);

  /**
   * int // 0 = block, 1 = call program, 2 = next instruction
   */
  int CallProgramOnNextNodeL (unsigned program_index, BOOL open_new_variable_scope);

  int ApplyTemplatesL (UINTPTR &argument);
  BOOL ApplyBuiltInTemplateL (UINTPTR &argument);

  /**
   * FALSE = end of the whole program, TRUE = ended frame
   */
  BOOL Return();

  void StartCollectResultTreeFragmentL ();
  void StartCollectTextL ();
  void EndCollectTextL ();
  void EndCollectResultTreeFragment ();

  XSLT_VariableValue *GetVariableValue (const XSLT_Variable* variable_elm);
  OP_BOOLEAN FindKeyedNodes (const XMLExpandedName &name, XMLTreeAccessor *tree, const uni_char *value, XPathValue *result);
  void CalculateVariableValueL (XSLT_Variable *variable, XSLT_VariableValue *value);

  void SignalErrorL (XSLT_Instruction *instruction, const char *reason);

  void HandleXPathResultL (OP_STATUS status, ProgramState *state);
  void HandleXPathResultL (OP_STATUS status, ProgramState *state, XPathExpression::Evaluate *evaluate, XSLT_XPathExpressionOrPattern *wrapper);

public:
  XSLT_Engine (XSLT_TransformationImpl *transformation);
  ~XSLT_Engine ();

  void InitializeL (XSLT_Program* program, XMLTreeAccessor* tree, XMLTreeAccessor::Node* treenode, XSLT_OutputHandler *output_handler, BOOL uncertain_output_method);

  BOOL IsProgramCompleted () { return !current_state; }

  BOOL IsProgramBlocked () { BOOL result = is_blocked; is_blocked = FALSE; return result; }
  void SetIsProgramBlocked () { is_blocked = TRUE; instructions_left_in_slice = 0; }

  BOOL IsProgramInterrupted () { BOOL result = is_program_interrupted; is_program_interrupted = FALSE; return result; }
  void SetIsProgramInterrupted () { is_program_interrupted = TRUE; }

  void SetDetectedOutputMethod (XSLT_Stylesheet::OutputSpecification::Method method);
  BOOL NeedsOutputHandlerSwitch() { return need_output_handler_switch || uncertain_output_method && IsProgramCompleted (); }
  BOOL HasAutoDetectedHTML() { return auto_detected_html; }
  OP_STATUS SwitchOutputHandler(XSLT_OutputHandler* new_root_outputhandler);

  OP_STATUS ExecuteProgram() { TRAPD(status, ExecuteProgramL()); return status; }

  XSLT_TransformationImpl *GetTransformation () { return transformation; }
  XSLT_Instruction *GetCurrentInstruction () { return current_state ? &current_state->program->instructions[current_state->instruction_index] : 0; }

  void HandleXPathResultL (OP_STATUS status, XPathExpression::Evaluate *evaluate, XSLT_XPathExpressionOrPattern *wrapper);

  void CalculateKeyValueL (XSLT_Key *key, XMLTreeAccessor *tree);

  static XSLT_TransformationImpl *GetTransformation (XPathExtensions::Context *context);
  static XPathNode* GetCurrentNode (XPathExtensions::Context *context);
  static XSLT_VariableValue *GetVariableValue (XPathExtensions::Context *context, const XSLT_Variable* variable_elm);
  static OP_BOOLEAN FindKeyedNodes (XPathExtensions::Context *context, const XMLExpandedName &name, XMLTreeAccessor *tree, const uni_char *value, XPathValue *result);
  static void SetBlocked (XPathExtensions::Context *context);
  static OP_STATUS CalculateVariableValue (XPathExtensions::Context *context, XSLT_Variable *variable, XSLT_VariableValue *value);
#ifdef XSLT_ERRORS
  static OP_STATUS ReportCircularVariables (XPathExtensions::Context *context, const XMLCompleteName &name);
#endif // XSLT_ERRORS
};

#endif // XSLT_SUPPORT
#endif // XSLT_ENGINE_H
