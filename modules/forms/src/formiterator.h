/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef FORMITERATOR_H
#define FORMITERATOR_H

#include "modules/logdoc/htm_elm.h"

class FramesDocument;
class HTML_Element;

/**
 * Iterates over a document tree that mustn't change during the iteration.
 */
class FormIterator
{
	FramesDocument* m_frames_doc;
	HTML_Element* m_form;
	HTML_Element* m_current;
	BOOL m_include_output;
#ifdef _PLUGIN_SUPPORT_
	BOOL m_include_object;
#endif // _PLUGIN_SUPPORT_
public:
	FormIterator(FramesDocument* frames_doc, const HTML_Element* form_element);

	/**
	 * Returns the next form element or NULL if no more.
	 */
	HTML_Element* GetNext();
	void SetIncludeOutputElements() { m_include_output = TRUE; }
#ifdef _PLUGIN_SUPPORT_
	void SetIncludeObjectElements() { m_include_object = TRUE; }
#endif // _PLUGIN_SUPPORT_
};

#endif // FORMITERATOR_H
