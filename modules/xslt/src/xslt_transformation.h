/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_TRANSFORMATION_H
#define XSLT_TRANSFORMATION_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/xslt.h"
#include "modules/xslt/src/xslt_types.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/url/url2.h"
#include "modules/util/simset.h"
#include "modules/xmlutils/xmlparser.h"

class XSLT_StylesheetImpl;
class XSLT_Program;
class XSLT_Engine;
class XSLT_OutputHandler;
class XSLT_OutputBuffer;
class XSLT_Tree;
class XSLT_VariableValue;

class XSLT_TransformationImpl
  : public XSLT_Stylesheet::Transformation,
    public XSLT_MessageHandler
{
public:
  XSLT_TransformationImpl (XSLT_StylesheetImpl* stylesheet, const XSLT_Stylesheet::Input &input , XSLT_Stylesheet::OutputForm outputform);

  virtual ~XSLT_TransformationImpl ();

  /* From XSLT_Stylesheet::Transformation: */
  virtual Status Transform ();
  virtual void SetCallback (Callback *cb);
  virtual void SetDefaultOutputMethod (XSLT_Stylesheet::OutputSpecification::Method method);
  virtual void SetDelayedOutputForm (XSLT_Stylesheet::OutputForm new_outputform);
  virtual XSLT_Stylesheet::OutputSpecification::Method GetOutputMethod ();
  virtual void SetXMLTokenHandler (XMLTokenHandler *tokenhandler, BOOL owned_by_transformation);
  virtual void SetXMLParseMode (XMLParser::ParseMode parsemode);
  virtual void SetStringDataCollector (StringDataCollector *collector, BOOL owned_by_transformation);

#ifdef XSLT_ERRORS
  /* From XSLT_MessageHandler: */
  virtual OP_BOOLEAN HandleMessage (XSLT_MessageHandler::MessageType type, const uni_char *message);
#endif // XSLT_ERRORS

  XSLT_StylesheetImpl *GetStylesheet () { return stylesheet; }
  XSLT_Engine *GetEngine () { return engine; }
  XPathPatternContext *GetPatternContext () { return pattern_context; }
  XMLTreeAccessor *GetInputTree () { return input.tree; }

  void StartTransformationL ();

  XSLT_Tree *GetTreeByURL (URL url);
  OP_BOOLEAN LoadTree (URL url);

  void Continue ();

  BOOL CreateVariableValueFromParameterL (XSLT_VariableValue *&value, const XMLExpandedName &name);

#ifdef XSLT_STATISTICS
  unsigned instructions_executed;
  unsigned patterns_tried_and_matched;
  unsigned patterns_tried_and_not_matched;
#endif // XSLT_STATISTICS

private:
  OP_STATUS SwitchOutputHandler ();
  OP_STATUS CreateOutputHandler ();

  XSLT_Stylesheet::Transformation::Callback *callback;
  XSLT_Stylesheet::Input input;
  XSLT_Stylesheet::OutputForm outputform;
  XSLT_OutputMethod outputmethod;

  XSLT_StylesheetImpl *stylesheet;
  XSLT_Program *program;
  XSLT_Engine *engine;
  XPathPatternContext *pattern_context;

  BOOL switched_outputform;

  BOOL owns_string_data_collector;
  StringDataCollector *string_data_collector;
  XSLT_OutputBuffer *output_buffer;

  BOOL owns_token_handler;
  XMLTokenHandler *token_handler;
  XMLParser::ParseMode parse_mode;

  XSLT_OutputHandler *output_handler;
  XMLParser *xmlparser;

#ifdef XSLT_TIMING
  double total_time, longest_chunk;
#endif // XSLT_TIMING

  class LoadedTree
    : public Link,
      public XMLTokenHandler
  {
  public:
    LoadedTree (XSLT_TransformationImpl *transformation, URL url)
      : transformation (transformation),
        url (url),
        tree (0),
        finished (FALSE)
    {
    }

    virtual ~LoadedTree ();

    virtual XMLTokenHandler::Result HandleToken (XMLToken &token);
    virtual void ParsingStopped (XMLParser *parser);

    XSLT_TransformationImpl *transformation;
    URL url;
    XSLT_Tree *tree;
    BOOL finished;
  };

  Head loaded_trees;
};

#endif // XSLT_SUPPORT
#endif // XSLT_TRANSFORMATION_H
