/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XSLT_CAPABILITIES_H
#define XSLT_CAPABILITIES_H

/* Supports XSLT_Stylesheet::OutputSpecification. */
#define XSLT_CAP_OUTPUT_SPECIFICATION

/* Supports XSLT_Stylesheet::DisableOutputEscaping. */
#define XSLT_CAP_DISABLE_OUTPUT_ESCAPING

/* This is the morph_2 version of XSLT with a totally different API
   than morph_1. */
#define XSLT_CAP_MORPH_2

/* XSLT_StylesheetParser::Callback and XSLT_Stylesheet::Transformation::Callback
   has HandleMessage() callbacks. */
#define XSLT_CAP_HANDLEMESSAGE

/* XSLT_Handler has HandleMessage() callback too. */
#define XSLT_CAP_XSLTHANDLER_HANDLEMESSAGE

/* XSLT_Stylesheet::Transformation::SetXMLParseMode() is supported. */
#define XSLT_CAP_TRANSFORMATION_SETXMLPARSEMODE

/* XSLT_Stylesheet::Transformation::SetDefaultOutputMethod() is supported. */
#define XSLT_CAP_TRANSFORMATION_SETDEFAULTOUTPUTMETHOD

/* Handles (and expects) XMLLanguageParser::GetCurrentNamespaceDeclaration() to
   return a chain that always includes declarations of the 'xml' and 'xmlns'
   prefixes.  If xmlutils doesn't support this, bug 340048 is unfixed.  But if
   xmlutils did this without it being handled, many (and invalid) declarations
   of those prefixes would appear in the output. */
#define XSLT_CAP_EXPECTS_XML_AND_XMLNS_PREFIXES_DECLARED

#endif // XSLT_CAPABILITIES_H
