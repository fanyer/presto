/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_HTMLSOURCECODEOUTPUTHANDLER_H
#define XSLT_HTMLSOURCECODEOUTPUTHANDLER_H

#ifdef XSLT_SUPPORT
# include "modules/xslt/src/xslt_xmloutputhandler.h"

class XSLT_OutputBuffer;
class XSLT_XMLSourceCodeOutputHandler;
class XSLT_StylesheetImpl;

class XSLT_HTMLSourceCodeOutputHandler
  : public XSLT_OutputHandler
{
public:
  XSLT_HTMLSourceCodeOutputHandler (XSLT_OutputBuffer *buffer, XSLT_StylesheetImpl *stylesheet);

  virtual ~XSLT_HTMLSourceCodeOutputHandler ();

private:
  /* From XSLT_OutputHandler: */
  virtual void StartElementL (const XMLCompleteName &name);
  virtual void AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified);
  virtual void AddTextL (const uni_char *data, BOOL disable_output_escaping);
  virtual void AddCommentL (const uni_char *data);
  virtual void AddProcessingInstructionL (const uni_char *target, const uni_char *data);
  virtual void EndElementL (const XMLCompleteName &name);
  virtual void EndOutputL ();

  BOOL OutputTagL (BOOL end_element);

  OpString element_name;
  BOOL in_html_starttag, in_xml_starttag;
  unsigned in_cdata_element;

  class Attribute
  {
  public:
    Attribute ()
      : next (0)
    {
    }

    ~Attribute ()
    {
      OP_DELETE (next);
    }

    void Reset ()
    {
      name.Empty ();
    }

    OpString name;
    OpString value;
    Attribute *next;
  };

  Attribute *first_used_attribute, *first_free_attribute;

  XSLT_OutputBuffer *buffer;
  const XSLT_Stylesheet::OutputSpecification *output_specification;
  BOOL output_doctype;

  void ConstructXMLBackupL ();

  XSLT_OutputHandler *xml_backup;
};

#endif // XSLT_SUPPORT
#endif // XSLT_HTMLSOURCECODEOUTPUTHANDLER_H
