/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPSTHELPERS_H
#define XPSTHELPERS_H

#ifdef XPATH_SUPPORT
#ifdef SELFTEST

#include "modules/xpath/xpath.h"

class FramesDocument;
class XPathNode;

const char *XPath_TestNumber (FramesDocument *doc, const char *expression, double expected);
const char *XPath_TestString (FramesDocument *doc, const char *expression, const char *expected);
const char *XPath_TestBoolean (FramesDocument *doc, const char *expression, BOOL expected);
const char *XPath_TestBooleanWithParam (FramesDocument *doc, const char *expression, BOOL expected, const char *param);
const char *XPath_TestNodeSet (FramesDocument *doc, const char *expression, XPathNode *contextNode, int *expectedIndeces, BOOL ordered = FALSE);
const char *XPath_TestNodeSet (FramesDocument *doc, const char *expression, int contextNodeIndex, int *expectedIndeces, BOOL ordered = FALSE);
const char *XPath_TestNodeSetAttribute (FramesDocument *doc, const char *expression, int contextNodeIndex, const char *localpart, const char *uri, int *expectedIndeces, BOOL ordered = FALSE);
const char *XPath_TestNodeSetNamespace (FramesDocument *doc, const char *expression, int contextNodeIndex, const char *prefix, const char *uri, int *expectedIndeces, BOOL ordered = FALSE);

#ifdef XPATH_PATTERN_SUPPORT
const char *XPath_TestPattern (FramesDocument *doc, const char *pattern, const char *nodes, BOOL match_expected);

#ifdef XPATH_NODE_PROFILE_SUPPORT
const char *XPath_TestProfileVerdict (const char *expressionsrc, const char *patternsrc, XPathPattern::ProfileVerdict expected);
#endif // XPATH_NODE_PROFILE_SUPPORT
#endif // XPATH_PATTERN_SUPPORT

double XPath_Zero ();

#endif // SELFTEST
#endif // XPATH_SUPPORT

#endif // XPSTHELPERS_H
