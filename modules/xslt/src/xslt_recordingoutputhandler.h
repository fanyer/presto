/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_RECORDINGOUTPUTHANDLER_H
#define XSLT_RECORDINGOUTPUTHANDLER_H

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_outputhandler.h"
# include "modules/util/adt/opvector.h"

class XSLT_Engine;

class XSLT_RecordingOutputHandler
  : public XSLT_OutputHandler
{
private:
  struct CommandEntry
  {
    enum Type
      {
        START_ELEMENT,
        ADD_ATTRIBUTE,
        ADD_TEXT,
        ADD_COMMENT,
        ADD_PROCESSING_INSTRUCTION,
        END_ELEMENT,
        END_OUTPUT
      };

    Type type;
    XMLCompleteName name;
    OpString string1;
    OpString string2;
    BOOL bool1;
    BOOL bool2;
  };

  XSLT_Engine *engine;
  BOOL has_root_element;

  OpAutoVector<CommandEntry> commands;
  unsigned commands_offset;

  CommandEntry *InsertCommandL (CommandEntry::Type type);

public:
  XSLT_RecordingOutputHandler (XSLT_Engine *engine)
    : engine (engine),
      has_root_element (FALSE),
      commands_offset (0)
  {
  }

  virtual void StartElementL (const XMLCompleteName &name);
  virtual void AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified);
  virtual void AddTextL (const uni_char *data, BOOL disable_output_escaping);
  virtual void AddCommentL (const uni_char *data);
  virtual void AddProcessingInstructionL (const uni_char *target, const uni_char *data);
  virtual void EndElementL (const XMLCompleteName &name);
  virtual void EndOutputL ();

  void ReplayCommandsL (XSLT_OutputHandler *output_handler);
};

#endif // XSLT_SUPPORT
#endif // XSLT_RECORDINGOUTPUTHANDLER_H
