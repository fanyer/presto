/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_CONTEXT_H
#define SVG_CONTEXT_H

#ifdef SVG_SUPPORT

/**
 * This class contain the information the SVG module want to store in
 * association to a HTML_Element. This class is subclassed inside the
 * SVG module to do different things on different elements.
 * This is the minimal interface needed for proper destruction.
 */
class SVGContext
{
public:
	/**
	 * Destruct a SVG Context
	 */
	virtual ~SVGContext() {}
};

#endif // SVG_SUPPORT
#endif // SVG_CONTEXT_H
