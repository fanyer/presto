/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPATHCAPABILITIES_H
#define XPATHCAPABILITIES_H

/* Support for optionally case insensitive NameTests via an extra argument to
   XPathExpression::Make. */
#define XPATH_CAP_CASE_INSENSITIVE_NAMETEST

/* Support for the class XPathPattern::MatchKey and the match_key argument to
   XPathPattern::{Match,Count,Search}. */
#define XPATH_CAP_MATCH_KEY

/* This is version shortcuts_3. */
#define XPATH_CAP_SHORTCUTS_3

/* XPathExpression::GetExpressionFlags() is supported. */
#define XPATH_CAP_XPATHEXPRESSION_GETEXPRESSIONFLAGS

/* XPathExpression::ExpressionData and accompanying XPathExpression::Make() are
   supported. */
#define XPATH_CAP_NEW_XPATHEXPRESSION_MAKE

/* XPathPattern::Search is reusable and has a slightly modified API. */
#define XPATH_CAP_NEW_XPATHPATTERN_SEARCH

#endif // XPATHCAPABILITIES_H
