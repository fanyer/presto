/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_OUTPUTHANDLER_H
#define XSLT_OUTPUTHANDLER_H

#ifdef XSLT_SUPPORT

#include "modules/xmlutils/xmltreeaccessor.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xmlutils/xmltokenbackend.h"
#include "modules/xmlutils/xmlutils.h"

class XMLCompleteName;
class XMLTokenHandler;
class XMLParser;
class XSLT_StylesheetImpl;
class XSLT_TransformationImpl;

/**
 * Accepts output from the xslt engine. Implement this to get the result.
 */
class XSLT_OutputHandler
{
public:
  virtual ~XSLT_OutputHandler();

  virtual void StartElementL (const XMLCompleteName &name) = 0;
  virtual void SuggestNamespaceDeclarationL (XSLT_Element *element, XMLNamespaceDeclaration *nsdeclaration, BOOL skip_excluded_namespaces);
  virtual void AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified) = 0;
  virtual void AddTextL (const uni_char *data, BOOL disable_output_escaping) = 0;
  virtual void AddCommentL (const uni_char *data) = 0;
  virtual void AddProcessingInstructionL (const uni_char *target, const uni_char *data) = 0;
  virtual void EndElementL (const XMLCompleteName &name) = 0;
  virtual void EndOutputL () = 0;

  void CopyOfL (XSLT_Element *copy_of, XMLTreeAccessor *tree, XMLTreeAccessor::Node *node);
  void CopyOfL (XSLT_Element *copy_of, XMLTreeAccessor *tree, XMLTreeAccessor::Node *node, const XMLCompleteName &attribute_name);

  void SetTransformation (XSLT_TransformationImpl *new_transformation) { transformation = new_transformation; }

protected:
  XSLT_TransformationImpl *transformation;
};

#endif // XSLT_SUPPORT
#endif // XSLT_OUTPUTHANDLER_H
