/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPAPIIMPL_H
#define XPAPIIMPL_H

#ifdef XPATH_SUPPORT

#include "modules/xpath/xpath.h"
#include "modules/xpath/src/xpdefs.h"
#include "modules/xpath/src/xpcontext.h"
#include "modules/xpath/src/xpnode.h"
#include "modules/xpath/src/xpstep.h"
#include "modules/util/adt/opvector.h"

class XPath_Namespaces;
class XPath_Value;
class XPath_Expression;
class XPath_Producer;
class XPath_LocationPath;
class XPath_Function;
class XPath_Variable;

#ifdef XPATH_EXTENSION_SUPPORT
class XPath_VariableReader;
#endif // XPATH_EXTENSION_SUPPORT

#ifdef XPATH_PATTERN_SUPPORT
class XPath_Pattern;
#endif // XPATH_PATTERN_SUPPORT

class XPathNamespacesImpl
  : public XPathNamespaces
{
private:
  XPath_Namespaces *storage;

public:
  XPathNamespacesImpl (XPath_Namespaces *storage);
  ~XPathNamespacesImpl ();

  XPath_Namespaces *GetStorage ();

  virtual OP_STATUS Add (const uni_char *prefix, const uni_char *uri);
  virtual OP_STATUS Remove (const uni_char *prefix);
  virtual unsigned GetCount ();
  virtual const uni_char *GetPrefix (unsigned index);
  virtual const uni_char *GetURI (unsigned index);
  virtual const uni_char *GetURI (const uni_char *prefix);
  virtual OP_STATUS SetURI (unsigned index, const uni_char *uri);
};

class XPathExpressionEvaluateImpl;

class XPathNodeImpl
  : public XPathNode
{
private:
  XPath_Node *node;
  XPath_GlobalContext *global_context;
  unsigned refcount;

public:
  static OP_STATUS Make (XPathNode *&node, XPath_Node *n, XPath_GlobalContext *global_context = 0);

  XPathNodeImpl (XPath_Node *node, XPath_GlobalContext *global_context = 0);
  ~XPathNodeImpl ();

  XPath_Node *GetInternalNode ();

  virtual Type GetType ();
  virtual XMLTreeAccessor *GetTreeAccessor ();
  virtual XMLTreeAccessor::Node *GetNode ();
  virtual void GetNodeName (XMLCompleteName &name);
  virtual void GetExpandedName (XMLExpandedName &name);
  virtual OP_STATUS GetStringValue (TempBuffer &value);
  virtual BOOL IsWhitespaceOnly ();
  virtual const uni_char *GetNamespacePrefix ();
  virtual const uni_char *GetNamespaceURI ();

  static XPathNodeImpl *IncRef (XPathNode *node);
  static void DecRef (XPathNode *node);
};

#ifdef XPATH_NODE_PROFILE_SUPPORT

class XPath_XMLTreeAccessorFilter;

class XPathNodeProfileImpl
  : public XPathNodeProfile
{
private:
  XPath_XMLTreeAccessorFilter **filters, single;
  unsigned filters_count;

  BOOL includes_regulars, includes_roots, includes_attributes, includes_namespaces;

public:
  XPathNodeProfileImpl ()
    : filters (0),
      filters_count (0),
      includes_regulars (TRUE),
      includes_roots (TRUE),
      includes_attributes (TRUE),
      includes_namespaces (TRUE)
  {
  }

  OpString source;

  virtual ~XPathNodeProfileImpl ();

  virtual BOOL Includes (XPathNode *node);

  void SetFilters (XPath_XMLTreeAccessorFilter **filters0, unsigned filters_count0) { filters = filters0; filters_count = filters_count0; }
  void SetExcludesRegulars () { includes_regulars = FALSE; }
  void SetExcludesRoots () { includes_roots = FALSE; }
  void SetExcludesAttributes () { includes_attributes = FALSE; }
  void SetExcludesNamespaces () { includes_namespaces = FALSE; }

  XPath_XMLTreeAccessorFilter *GetSingleFilter () { return &single; }
  XPath_XMLTreeAccessorFilter **GetFilters () { return filters; }
  unsigned GetFiltersCount () { return filters_count; }
  BOOL GetIncludesRegulars () { return includes_regulars; }
  BOOL GetIncludesRoots () { return includes_roots; }
  BOOL GetIncludesAttributes () { return includes_attributes; }
  BOOL GetIncludesNamespaces () { return includes_namespaces; }
};

#endif // XPATH_NODE_PROFILE_SUPPORT

class XPathExpressionImpl
  : public XPathExpression
{
private:
  friend class XPathExpression;
  friend OP_STATUS XPathExpression::Evaluate::Make (Evaluate *&, XPathExpression *);
  friend void XPathExpression::Evaluate::Free (Evaluate *);

  OpString source;
  XPath_Namespaces *namespaces;
#ifdef XPATH_EXTENSION_SUPPORT
  XPathExtensions *extensions;
#endif // XPATH_EXTENSION_SUPPORT
#ifdef XPATH_NODE_PROFILE_SUPPORT
  XPathNodeProfileImpl nodeprofile;
#endif // XPATH_NODE_PROFILE_SUPPORT
  unsigned refcount;

  XPathExpressionEvaluateImpl *cached_evaluate;

#ifdef XPATH_ERRORS
  void CompileL (OpString *error_message);
#else // XPATH_ERRORS
  void CompileL ();
#endif // XPATH_ERRORS

public:
  XPathExpressionImpl ();
  virtual ~XPathExpressionImpl ();

  virtual unsigned GetExpressionFlags ();

#ifdef XPATH_NODE_PROFILE_SUPPORT
  virtual OP_STATUS GetNodeProfile (XPathNodeProfile *&profile);
#endif // XPATH_NODE_PROFILE_SUPPORT

  OP_STATUS SetSource (const uni_char *source0) { return source.Set (source0); }

#ifdef XPATH_ERRORS
  OP_STATUS Compile (OpString *error_message);
  const OpString *GetSource () { return &source; }
#else // XPATH_ERRORS
  OP_STATUS Compile ();
#endif // XPATH_ERRORS

  XPath_Expression *primitive;
  XPath_Producer *ordered_single, *unordered_single, *ordered, *unordered;
  XPath_ContextStateSizes primitive_state_sizes, nodeset_state_sizes;
#ifdef XPATH_EXTENSION_SUPPORT
  XPath_VariableReader *nodeset_readers, *primitive_readers;
#endif // XPATH_EXTENSION_SUPPORT

  static XPathExpressionImpl *IncRef (XPathExpressionImpl *expression);
  static void DecRef (XPathExpressionImpl *expression);
};

class XPathExpressionEvaluateImpl
  : public XPathExpression::Evaluate
{
private:
  friend class XPathExpression::Evaluate;

  XPathExpressionImpl *expression;
  XPathNodeImpl *context_node;
  unsigned context_position, context_size;
  unsigned when_number, when_boolean, when_string, when_nodeset;

  BOOL started, need_reset, finished, failed;
  XPath_GlobalContext global_context;
  XPath_Producer *producer;

  OpString result_string;
  double result_number;
  XPathNode *result_node;
  XPath_NodeList *result_nodelist;
  XPathNode **result_nodes;
  unsigned result_nodes_count;

#ifdef XPATH_ERRORS
  OpString error_message;
#endif // XPATH_ERRORS

  OP_STATUS Start (const XPath_ContextStateSizes *state_sizes);
  void Reset (BOOL from_constructor = FALSE);

public:
  XPathExpressionEvaluateImpl (XPathExpressionImpl *expression);
  ~XPathExpressionEvaluateImpl ();

  XPath_GlobalContext *GetGlobalContext () { return &global_context; }

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
  virtual const uni_char *GetErrorMessage ();
};

#ifdef XPATH_PATTERN_SUPPORT

class XPathPatternContextImpl
  : public XPathPatternContext
{
private:
  friend class XPathPatternContext;

  XPath_PatternContext *context;

public:
  XPathPatternContextImpl ()
    : context (0)
  {
  }

  virtual ~XPathPatternContextImpl ();

  virtual void InvalidateTree (XMLTreeAccessor *tree);

  XPath_PatternContext *GetContext () { return context; }
};

class XPathPatternMatchImpl;

class XPathPatternImpl
  : public XPathPattern
{
private:
  friend class XPathPattern;
  friend OP_STATUS XPathPattern::Match::Make (Match *&, XPathPatternContext *, XPathNode *, XPathPattern **, unsigned);
  friend void XPathPattern::Match::Free (Match *);

  OpString source;
  XPath_Namespaces *namespaces;
#ifdef XPATH_EXTENSION_SUPPORT
  XPathExtensions *extensions;
#endif // XPATH_EXTENSION_SUPPORT

  XPathPatternMatchImpl *cached_match;

#ifdef XPATH_ERRORS
  void CompileL (OpString *error_message);
  OP_STATUS Compile (OpString *error_message);
#else // XPATH_ERRORS
  void CompileL ();
  OP_STATUS Compile ();
#endif // XPATH_ERRORS

public:
  XPathPatternImpl ();
  virtual ~XPathPatternImpl ();

  XPath_Namespaces *GetNamespaces () { return namespaces; }
#ifdef XPATH_EXTENSION_SUPPORT
  XPathExtensions *GetExtensions () { return extensions; }
#endif // XPATH_EXTENSION_SUPPORT

  XPath_Pattern *pattern;
  XPath_ContextStateSizes sizes;
#ifdef XPATH_EXTENSION_SUPPORT
  XPath_VariableReader *readers;
#endif // XPATH_EXTENSION_SUPPORT
  float priority;

  virtual float GetPriority ();
#ifdef XPATH_NODE_PROFILE_SUPPORT
  virtual ProfileVerdict GetProfileVerdict (XPathNodeProfile *profile);
#endif // XPATH_NODE_PROFILE_SUPPORT

#ifdef XPATH_ERRORS
  const OpString *GetSource () { return &source; }
#endif // XPATH_ERRORS
};

class XPathMultiplePatternsHelper
{
protected:
  friend void XPathPattern::Match::Free (XPathPattern::Match *);

  XPathPatternImpl **patterns;
  XPath_Pattern **patterns_internal;
  unsigned pattern_index, patterns_count;
  XPath_GlobalContext *global_contexts;
  BOOL started, prepared, failed;

  XPathMultiplePatternsHelper ();
  ~XPathMultiplePatternsHelper ();

  OP_STATUS SetPatterns (XPathPattern **patterns, unsigned patterns_count, XPathPatternContext *pattern_context);
  OP_BOOLEAN StartAndPrepare (XMLTreeAccessor *tree);
#ifdef XPATH_EXTENSION_SUPPORT
  void SetExtensionsContext (XPathExtensions::Context *context);
#endif // XPATH_EXTENSION_SUPPORT
  void Reset (BOOL from_constructor = FALSE);

#ifdef XPATH_ERRORS
  OpString error_message;
  const OpString *failed_pattern_source;

  const uni_char *GetErrorMessage () { if (failed) return error_message.IsEmpty () ? UNI_L ("unknown error") : error_message.CStr (); else return 0; }
  const uni_char *GetFailedPatternSource () { if (failed_pattern_source) return failed_pattern_source->CStr (); else return 0; }
#endif // XPATH_ERRORS
};

class XPathPatternSearchImpl
  : public XPathPattern::Search
{
private:
  friend class XPathPattern::Search;

  XPathExpression *expression;
  XPathExpression::Evaluate *evaluate;

  XPathPatternSearchImpl (XPathExpression *expression);
  ~XPathPatternSearchImpl ();

public:
  virtual OP_STATUS Start (XPathNode *node);
#ifdef XPATH_EXTENSION_SUPPORT
  virtual void SetExtensionsContext (XPathExtensions::Context *context);
#endif // XPATH_EXTENSION_SUPPORT
  virtual void SetCostLimit (unsigned limit);
  virtual unsigned GetLastOperationCost ();
  virtual OP_BOOLEAN GetNextNode (XPathNode *&node);
#ifdef XPATH_ERRORS
  virtual const uni_char *GetErrorMessage () { return evaluate->GetErrorMessage (); }
  virtual const uni_char *GetFailedPatternSource () { return static_cast<XPathExpressionImpl *> (expression)->GetSource ()->CStr (); }
#endif // XPATH_ERRORS
  virtual void Stop ();
};

class XPathPatternCountImpl
  : public XPathPattern::Count,
    public XPathMultiplePatternsHelper
{
private:
  friend class XPathPattern::Count;

  Level level;
  XPathNodeImpl *node;
  unsigned count_patterns_count, from_patterns_count, cost;

  XPathPatternCountImpl ();
  ~XPathPatternCountImpl ();

public:
#ifdef XPATH_EXTENSION_SUPPORT
  virtual void SetExtensionsContext (XPathExtensions::Context *context);
#endif // XPATH_EXTENSION_SUPPORT
  virtual void SetCostLimit (unsigned limit);
  virtual unsigned GetLastOperationCost ();
  virtual OP_BOOLEAN GetResultLevelled (unsigned &numbers_count, unsigned *&numbers);
  virtual OP_BOOLEAN GetResultAny (unsigned &number);
#ifdef XPATH_ERRORS
  virtual const uni_char *GetErrorMessage () { return XPathMultiplePatternsHelper::GetErrorMessage (); }
  virtual const uni_char *GetFailedPatternSource () { return XPathMultiplePatternsHelper::GetFailedPatternSource (); }
#endif // XPATH_ERRORS
};

class XPathPatternMatchImpl
  : public XPathPattern::Match,
    public XPathMultiplePatternsHelper
{
private:
  friend class XPathPattern::Match;

  XPathNodeImpl *node;

  void Reset ();

public:
  XPathPatternMatchImpl ();
  ~XPathPatternMatchImpl ();

#ifdef XPATH_EXTENSION_SUPPORT
  virtual void SetExtensionsContext (XPathExtensions::Context *context);
#endif // XPATH_EXTENSION_SUPPORT
  virtual void SetCostLimit (unsigned limit);
  virtual unsigned GetLastOperationCost ();
  virtual OP_BOOLEAN GetMatchedPattern (XPathPattern *&matched);
#ifdef XPATH_ERRORS
  virtual const uni_char *GetErrorMessage () { return XPathMultiplePatternsHelper::GetErrorMessage (); }
  virtual const uni_char *GetFailedPatternSource () { return XPathMultiplePatternsHelper::GetFailedPatternSource (); }
#endif // XPATH_ERRORS
};

#endif // XPATH_PATTERN_SUPPORT
#endif // XPATH_SUPPORT
#endif // XPAPIIMPL_H
