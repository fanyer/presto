/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_SCOPE_ELEMENT_INSPECTOR_H
#define OP_SCOPE_ELEMENT_INSPECTOR_H

#ifdef SCOPE_ECMASCRIPT_DEBUGGER

class FramesDocument;
class HTML_Element;

/**
 * An interface for notifying the client that an element on the
 * page should be inspected.
 */
class OpScopeElementInspector
{
public:

	/**
	 * Call this whenever the client should change its view to
	 * inspect a certain element on the page.
	 *
	 * @param doc The FramesDocument where the state change happened.
	 * @param html_elm The HTML_Element to inspect.
	 */
	static OP_STATUS InspectElement(FramesDocument *doc, HTML_Element* html_elm);

}; // OpScopeElementInspector

#endif // SCOPE_ECMASCRIPT_DEBUGGER

#endif // OP_SCOPE_ELEMENT_INSPECTOR_H
