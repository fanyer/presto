/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_transformation.h"
# include "modules/xslt/src/xslt_stylesheet.h"
# include "modules/xslt/src/xslt_engine.h"
# include "modules/xslt/src/xslt_outputhandler.h"
# include "modules/xslt/src/xslt_outputbuffer.h"
# include "modules/xslt/src/xslt_xmltokenhandleroutputhandler.h"
# include "modules/xslt/src/xslt_xmlsourcecodeoutputhandler.h"
# include "modules/xslt/src/xslt_htmlsourcecodeoutputhandler.h"
# include "modules/xslt/src/xslt_textoutputhandler.h"
# include "modules/xslt/src/xslt_recordingoutputhandler.h"
# include "modules/xslt/src/xslt_copytofileoutputhandler.h"
# include "modules/xslt/src/xslt_tree.h"
# include "modules/xslt/src/xslt_variable.h"

# ifdef XSLT_TIMING
#  include "modules/pi/OpSystemInfo.h"
# endif // XSLT_TIMING

#ifdef XSLT_ERRORS

/* virtual */ OP_BOOLEAN
XSLT_Stylesheet::Transformation::Callback::HandleMessage (MessageType type, const uni_char *message)
{
  return OpBoolean::IS_FALSE;
}

#endif // XSLT_ERRORS

XSLT_TransformationImpl::XSLT_TransformationImpl (XSLT_StylesheetImpl *stylesheet, const XSLT_Stylesheet::Input &input , XSLT_Stylesheet::OutputForm outputform)
  : callback (0),
    input (input),
    outputform (outputform),
    outputmethod (stylesheet->GetOutputMethod ()),
    stylesheet (stylesheet),
    program (0),
    engine (0),
    pattern_context (0),
    switched_outputform (FALSE),
    owns_string_data_collector (FALSE),
    string_data_collector (0),
    output_buffer (0),
    owns_token_handler (FALSE),
    token_handler (0),
    parse_mode (XMLParser::PARSEMODE_DOCUMENT),
    output_handler (0)
{
#ifdef XSLT_STATISTICS
  instructions_executed = 0;
  patterns_tried_and_matched = 0;
  patterns_tried_and_not_matched = 0;
#endif // XSLT_STATISTICS

#ifdef XSLT_TIMING
  total_time = 0;
  longest_chunk = 0;
#endif // XSLT_TIMING
}

XSLT_TransformationImpl::~XSLT_TransformationImpl()
{
  OP_DELETE (output_handler);
  OP_DELETE (output_buffer);

  if (owns_token_handler)
    OP_DELETE (token_handler);

  if (owns_string_data_collector)
    OP_DELETE (string_data_collector);

  XPathPatternContext::Free (pattern_context);

  OP_DELETE (engine);

  for (LoadedTree *tree = static_cast<LoadedTree *> (loaded_trees.First ()); tree; tree = static_cast<LoadedTree *> (tree->Suc ()))
    if (!tree->finished)
      callback->CancelLoadDocument (tree);

  loaded_trees.Clear ();

#ifdef XSLT_TIMING
  printf ("XSLT\n\tTotal time: %.4f\n\tLongest chunk: %.4f\n", total_time, longest_chunk);
#endif // XSLT_TIMING

#ifdef XSLT_STATISTICS
  printf ("XSLT\n\tInstructions executed: %u\n", instructions_executed);
  if (patterns_tried_and_matched != 0 || patterns_tried_and_not_matched != 0)
    printf ("\tPatterns tried: %u (%u matched, %u %%)\n", patterns_tried_and_matched + patterns_tried_and_not_matched, patterns_tried_and_matched, (100 * patterns_tried_and_matched) / (patterns_tried_and_matched + patterns_tried_and_not_matched));
#endif // XSLT_STATISTICS
}

/* virtual */ XSLT_Stylesheet::Transformation::Status
XSLT_TransformationImpl::Transform ()
{
  OP_ASSERT(program);

  if (!engine)
    {
      if (outputform == XSLT_Stylesheet::OUTPUT_DELAYED_DECISION && outputmethod != XSLT_OUTPUT_DEFAULT)
        /* Caller wanted to wait and set an output handler when the output
           method is known.  And we know already. */
        return TRANSFORM_NEEDS_OUTPUTHANDLER;

      // Program not started yet
      engine = OP_NEW (XSLT_Engine, (this));
      if (!engine)
        return TRANSFORM_NO_MEMORY;

      if (OpStatus::IsError (CreateOutputHandler()))
        return TRANSFORM_NO_MEMORY;

      if (OpStatus::IsError (XPathPatternContext::Make (pattern_context)))
        return TRANSFORM_NO_MEMORY;

      TRAPD (status, engine->InitializeL (program, input.tree, input.node, output_handler, outputmethod == XSLT_OUTPUT_DEFAULT));
      if (OpStatus::IsError (status))
        {
          if (OpStatus::IsMemoryError (status))
            return TRANSFORM_NO_MEMORY;

          return TRANSFORM_FAILED;
        }
    }

  if (switched_outputform)
    {
      OP_ASSERT (engine->NeedsOutputHandlerSwitch ());

      if (OpStatus::IsError (SwitchOutputHandler ()))
        return TRANSFORM_NO_MEMORY;

      switched_outputform = FALSE;
    }

  if (!engine->IsProgramCompleted ())
    {
#ifdef XSLT_TIMING
      double before = g_op_time_info->GetRuntimeMS ();
#endif // XSLT_TIMING

      OP_STATUS status = engine->ExecuteProgram ();

#ifdef XSLT_TIMING
      double after = g_op_time_info->GetRuntimeMS ();
      double chunk = after - before;

      if (chunk > longest_chunk)
        longest_chunk = chunk;

      total_time += chunk;
#endif // XSLT_TIMING

      if (OpStatus::IsError (status))
        return TRANSFORM_FAILED;

      if (engine->NeedsOutputHandlerSwitch ())
        {
          if (engine->HasAutoDetectedHTML ())
            outputmethod = XSLT_OUTPUT_HTML;
          else
            outputmethod = XSLT_OUTPUT_XML;

          if (outputform == XSLT_Stylesheet::OUTPUT_DELAYED_DECISION)
            return TRANSFORM_NEEDS_OUTPUTHANDLER;
          else
            {
              status = SwitchOutputHandler ();

              if (OpStatus::IsError (status))
                return OpStatus::IsMemoryError (status) ? TRANSFORM_NO_MEMORY : TRANSFORM_FAILED;
            }
        }
    }

  if (engine->IsProgramCompleted ())
    return TRANSFORM_FINISHED;

  if (engine->IsProgramBlocked ())
    return TRANSFORM_BLOCKED;
  else
    return TRANSFORM_PAUSED;
}

/* virtual */ void
XSLT_TransformationImpl::SetCallback (Callback *cb)
{
  callback = cb;
}

/* virtual */ void
XSLT_TransformationImpl::SetXMLTokenHandler (XMLTokenHandler *tokenhandler, BOOL owned_by_transformation)
{
  OP_ASSERT (tokenhandler);
  token_handler = tokenhandler;
  owns_token_handler = owned_by_transformation;
}

/* virtual */ void
XSLT_TransformationImpl::SetXMLParseMode (XMLParser::ParseMode parsemode)
{
  parse_mode = parsemode;
}

/* virtual */ void
XSLT_TransformationImpl::SetStringDataCollector (StringDataCollector *collector, BOOL owned_by_transformation)
{
  OP_ASSERT(collector);
  string_data_collector = collector;
  owns_string_data_collector = owned_by_transformation;
}

/* virtual */ void
XSLT_TransformationImpl::SetDefaultOutputMethod (XSLT_Stylesheet::OutputSpecification::Method method)
{
  if (outputmethod == XSLT_OUTPUT_DEFAULT)
    switch (method)
      {
      case XSLT_Stylesheet::OutputSpecification::METHOD_XML:
        outputmethod = XSLT_OUTPUT_XML;
        break;

      case XSLT_Stylesheet::OutputSpecification::METHOD_HTML:
        outputmethod = XSLT_OUTPUT_HTML;
        break;

      case XSLT_Stylesheet::OutputSpecification::METHOD_TEXT:
        outputmethod = XSLT_OUTPUT_TEXT;
        break;

      default:
        OP_ASSERT (FALSE);
      }
}

/* virtual */ void
XSLT_TransformationImpl::SetDelayedOutputForm (XSLT_Stylesheet::OutputForm new_outputform)
{
  OP_ASSERT (outputform == XSLT_Stylesheet::OUTPUT_DELAYED_DECISION);
  OP_ASSERT (new_outputform != XSLT_Stylesheet::OUTPUT_DELAYED_DECISION);
  outputform = new_outputform;
  switched_outputform = engine != 0;
}

/* virtual */ XSLT_Stylesheet::OutputSpecification::Method
XSLT_TransformationImpl::GetOutputMethod ()
{
  OP_ASSERT(outputform == XSLT_Stylesheet::OUTPUT_DELAYED_DECISION);

  switch (outputmethod)
    {
    default:
      return XSLT_Stylesheet::OutputSpecification::METHOD_XML;

    case XSLT_OUTPUT_HTML:
      return XSLT_Stylesheet::OutputSpecification::METHOD_HTML;

    case XSLT_OUTPUT_TEXT:
      return XSLT_Stylesheet::OutputSpecification::METHOD_TEXT;
    }
}

#ifdef XSLT_ERRORS

/* virtual */ OP_BOOLEAN
XSLT_TransformationImpl::HandleMessage (XSLT_MessageHandler::MessageType type, const uni_char *message)
{
  /* A static_cast should be valid for converting between enumerated types, but
     apparently GCC 2.95 doesn't think so. */
  return callback->HandleMessage ((XSLT_Stylesheet::Transformation::Callback::MessageType) type, message);
}

#endif // XSLT_ERRORS

void
XSLT_TransformationImpl::StartTransformationL ()
{
  if (input.parameters_count != 0)
    {
      XSLT_Stylesheet::Input::Parameter *parameters = input.parameters;

      input.parameters = OP_NEWA (XSLT_Stylesheet::Input::Parameter, input.parameters_count);
      if (!input.parameters)
        LEAVE (OpStatus::ERR_NO_MEMORY);

      for (unsigned index = 0; index < input.parameters_count; ++index)
        {
          input.parameters[index].name.SetL (parameters[index].name);
          input.parameters[index].value = static_cast<XSLT_ParameterValue *> (parameters[index].value)->CopyL ();
        }
    }
  else
    input.parameters = 0;

  program = stylesheet->GetRootProgram ();
}

XSLT_Tree *
XSLT_TransformationImpl::GetTreeByURL (URL url)
{
  for (LoadedTree *loaded_tree = static_cast<LoadedTree *> (loaded_trees.First ()); loaded_tree; loaded_tree = static_cast<LoadedTree *> (loaded_tree->Suc ()))
    if (loaded_tree->url == url)
      return loaded_tree->tree;
  return NULL;
}

OP_BOOLEAN
XSLT_TransformationImpl::LoadTree (URL url)
{
  LoadedTree *loaded_tree;

  for (loaded_tree = static_cast<LoadedTree *> (loaded_trees.First ()); loaded_tree; loaded_tree = static_cast<LoadedTree *> (loaded_tree->Suc ()))
    if (loaded_tree->url == url)
      if (!loaded_tree->finished)
        /* Already loading.  A bit weird, really. */
        return OpStatus::OK;
      else
        /* Tried to load but failed. */
        return OpStatus::ERR;

  loaded_tree = OP_NEW (LoadedTree, (this, url));
  if (!loaded_tree)
    return OpStatus::ERR_NO_MEMORY;

  TRAPD (status, loaded_tree->tree = XSLT_Tree::MakeL ());

  if (OpStatus::IsMemoryError (status) || OpStatus::IsMemoryError (status = callback->LoadDocument (url, loaded_tree)))
    {
      OP_DELETE (loaded_tree);
      return OpStatus::ERR_NO_MEMORY;
    }

  loaded_tree->Into (&loaded_trees);

  if (status == OpStatus::ERR)
    {
      loaded_tree->finished = TRUE;
      return OpBoolean::IS_FALSE;
    }
  else
    return OpBoolean::IS_TRUE;
}

void
XSLT_TransformationImpl::Continue ()
{
  callback->ContinueTransformation (this);
}

BOOL
XSLT_TransformationImpl::CreateVariableValueFromParameterL (XSLT_VariableValue *&value, const XMLExpandedName &name)
{
  for (unsigned index = 0; index < input.parameters_count; ++index)
    if (input.parameters[index].name == name)
      {
        value = static_cast<XSLT_ParameterValue *> (input.parameters[index].value)->MakeVariableValueL ();
        return value != 0;
      }

  return FALSE;
}

OP_STATUS
XSLT_TransformationImpl::SwitchOutputHandler()
{
  OP_ASSERT (output_handler);
  OP_ASSERT (engine);

  XSLT_OutputHandler *old_output_handler = output_handler;
  output_handler = 0;

  if (OpStatus::IsError (CreateOutputHandler()))
    {
      output_handler = old_output_handler;
      return OpStatus::ERR_NO_MEMORY;
    }

  OP_STATUS status = engine->SwitchOutputHandler (output_handler);

  OP_DELETE (old_output_handler);
  return status;
}

OP_STATUS
XSLT_TransformationImpl::CreateOutputHandler ()
{
  OP_DELETE (output_handler); // In case we get an OOM at the first attempt or need to switch outpuhandler
  output_handler = 0;

  if (outputform == XSLT_Stylesheet::OUTPUT_DELAYED_DECISION || outputmethod == XSLT_OUTPUT_DEFAULT)
    output_handler = OP_NEW (XSLT_RecordingOutputHandler, (engine));
  else if (outputform == XSLT_Stylesheet::OUTPUT_XMLTOKENHANDLER)
    {
      if (outputmethod != XSLT_OUTPUT_XML)
        return OpStatus::ERR;

      OP_ASSERT (token_handler);

      output_handler = OP_NEW (XSLT_XMLTokenHandlerOutputHandler, (token_handler, parse_mode, this));

      if (output_handler && OpStatus::IsMemoryError (static_cast<XSLT_XMLTokenHandlerOutputHandler *> (output_handler)->Construct ()))
        {
          OP_DELETE (output_handler);
          output_handler = 0;
        }
    }
  else
    {
      OP_ASSERT (string_data_collector);

      output_buffer = OP_NEW (XSLT_OutputBuffer, (string_data_collector));
      if (!output_buffer)
        return OpStatus::ERR_NO_MEMORY;

      if (outputmethod == XSLT_OUTPUT_TEXT)
        output_handler = OP_NEW (XSLT_TextOutputHandler, (output_buffer));
      else if (outputmethod == XSLT_OUTPUT_HTML)
        output_handler = OP_NEW (XSLT_HTMLSourceCodeOutputHandler, (output_buffer, stylesheet));
      else
        output_handler = OP_NEW (XSLT_XMLSourceCodeOutputHandler, (output_buffer, stylesheet));
    }

#ifdef XSLT_COPY_TO_FILE_SUPPORT
  if (output_handler && !stylesheet->GetCopyToFile ().IsEmpty ())
    if (XSLT_OutputHandler *copy_output_handler = OP_NEW (XSLT_CopyToFileOutputHandler, (stylesheet->GetCopyToFile ().CStr (), output_handler, outputmethod, stylesheet)))
      output_handler = copy_output_handler;
    else
      {
        OP_DELETE (output_handler);
        return OpStatus::ERR_NO_MEMORY;
      }
#endif // XSLT_COPY_TO_FILE_SUPPORT

  if (output_handler)
    {
#ifdef XSLT_ERRORS
      output_handler->SetTransformation (this);
#endif // XSLT_ERRORS
      return OpStatus::OK;
    }
  else
    return OpStatus::ERR_NO_MEMORY;
}

/* virtual */
XSLT_TransformationImpl::LoadedTree::~LoadedTree ()
{
  OP_DELETE (tree);
}

/* virtual */ XMLTokenHandler::Result
XSLT_TransformationImpl::LoadedTree::HandleToken (XMLToken &token)
{
  return tree->HandleToken (token);
}

/* virtual */ void
XSLT_TransformationImpl::LoadedTree::ParsingStopped (XMLParser *parser)
{
  finished = TRUE;

  if (!parser->IsFinished ())
    {
      OP_DELETE (tree);
      tree = NULL;
    }

  transformation->Continue ();
}

#endif // XSLT_SUPPORT
