/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_recordingoutputhandler.h"
# include "modules/xslt/src/xslt_engine.h"

XSLT_RecordingOutputHandler::CommandEntry *
XSLT_RecordingOutputHandler::InsertCommandL (CommandEntry::Type type)
{
  CommandEntry *command = OP_NEW_L (CommandEntry, ());

  if (OpStatus::IsMemoryError (commands.Add (command)))
    {
      OP_DELETE (command);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  command->type = type;
  return command;
}

void
XSLT_RecordingOutputHandler::ReplayCommandsL (XSLT_OutputHandler *output_handler)
{
  while (commands_offset < commands.GetCount ())
    {
      CommandEntry* command = commands.Get (commands_offset++);

      switch (command->type)
        {
        case CommandEntry::START_ELEMENT:
          output_handler->StartElementL (command->name);
          break;

        case CommandEntry::ADD_ATTRIBUTE:
          output_handler->AddAttributeL (command->name, command->string1.CStr (), command->bool1, command->bool2);
          break;

        case CommandEntry::ADD_TEXT:
          output_handler->AddTextL (command->string1.CStr (), command->bool1);
          break;

        case CommandEntry::ADD_COMMENT:
          output_handler->AddCommentL (command->string1.CStr ());
          break;

        case CommandEntry::ADD_PROCESSING_INSTRUCTION:
          output_handler->AddProcessingInstructionL (command->string1.CStr (), command->string2.CStr ());
          break;

        case CommandEntry::END_ELEMENT:
          output_handler->EndElementL (command->name);
          break;

        case CommandEntry::END_OUTPUT:
          output_handler->EndOutputL ();
          break;
        }
    }
}

/* virtual */ void
XSLT_RecordingOutputHandler::StartElementL (const XMLCompleteName &name)
{
  InsertCommandL (CommandEntry::START_ELEMENT)->name.SetL (name);

  if (!has_root_element)
    {
      has_root_element = TRUE;

      BOOL use_html_output = !name.GetUri () && uni_stri_eq (name.GetLocalPart (), "html");

      engine->SetDetectedOutputMethod (use_html_output ? XSLT_Stylesheet::OutputSpecification::METHOD_HTML : XSLT_Stylesheet::OutputSpecification::METHOD_XML);
    }
}

/* virtual */ void
XSLT_RecordingOutputHandler::AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified)
{
  CommandEntry *command = InsertCommandL (CommandEntry::ADD_ATTRIBUTE);
  command->name.SetL (name);
  command->string1.SetL (value);
  command->bool1 = id;
  command->bool2 = specified;
}

/* virtual */ void
XSLT_RecordingOutputHandler::AddTextL (const uni_char *data, BOOL disable_output_escaping)
{
  CommandEntry *command = InsertCommandL (CommandEntry::ADD_TEXT);
  command->string1.SetL (data);
  command->bool1 = disable_output_escaping;
}

/* virtual */ void
XSLT_RecordingOutputHandler::AddCommentL (const uni_char *data)
{
  InsertCommandL (CommandEntry::ADD_COMMENT)->string1.SetL (data);
}

/* virtual */ void
XSLT_RecordingOutputHandler::AddProcessingInstructionL (const uni_char *target, const uni_char *data)
{
  CommandEntry *command = InsertCommandL (CommandEntry::ADD_PROCESSING_INSTRUCTION);
  command->string1.SetL (target);
  command->string2.SetL (data);
}

/* virtual */ void
XSLT_RecordingOutputHandler::EndElementL (const XMLCompleteName &name)
{
  InsertCommandL (CommandEntry::END_ELEMENT)->name.SetL (name);
}

/* virtual */ void
XSLT_RecordingOutputHandler::EndOutputL ()
{
  InsertCommandL (CommandEntry::END_OUTPUT);

  if (!has_root_element)
    engine->SetDetectedOutputMethod (XSLT_Stylesheet::OutputSpecification::METHOD_XML);
}

#endif // XSLT_SUPPORT
