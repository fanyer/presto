/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2002-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DOCUMENTSTATE_H
#define DOCUMENTSTATE_H

#include "modules/util/simset.h"
#include "modules/logdoc/logdocenum.h"
#include "modules/logdoc/namespace.h"

class HTML_Element;
class FormValue;
class FramesDocument;
class FramesDocElm;

/**
 * When we restore state in a document we often need to identify an
 * HTML_Element in the document tree. Since the document might be
 * slightly different in subtle ways (random ads for instance) we use
 * a kind of map to the HTML_Element where each step is identified by
 * a PathElement and where the map is encapsulated in a
 * DocumentElementPath object.
 *
 * <p>There are only 2 methods to use. Create and Find.
 *
 * @author jl
 */
class DocumentElementPath
{
protected:
	DocumentElementPath();

	class PathElement
	{
	public:
		PathElement();
		~PathElement();

		NS_Type ns;
		int type;
		uni_char *name;
		unsigned index, offset;

		OP_STATUS Set(HTML_Element *element, unsigned character_offset);
		BOOL NameMatches(HTML_Element *element);

		HTML_Element *Find(HTML_Element *root, unsigned *offset);
		BOOL Match(HTML_Element *element);
	};

	PathElement *elements;
	unsigned elements_count;

public:
	/**
	 * Creates a map (DocumentElementPath) to an element.
	 *
	 * @param[out] path The created path if the method returns
	 * OpStatus::OK. It is owned by the called and should be deleted
	 * with the delete operator.
	 *
	 * @param[in] element The element to make a map to.
	 *
	 * @param[in] character_offset The offset in the element (relevant
	 * for text).
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	static OP_STATUS Make(DocumentElementPath *&path, HTML_Element *element, unsigned character_offset = 0);

	/**
	 * Destructor.
	 */
	~DocumentElementPath();

	/**
	 * Find the element the map directs us to in the document tree.
	 *
	 * @param[in] root The document root.
	 *
	 * @param[out] offset If not NULL will get the character offset if
	 * any.
	 *
	 * @returns The found element or NULL if not found.
	 */
	HTML_Element *Find(HTML_Element *root, unsigned *offset = NULL);

	/**
	 * Checks if the element is the one the map points out.
	 *
	 * @param[in] element The element to check.
	 *
	 * @returns TRUE if it matches, FALSE otherwise.
	 */
	BOOL Match(HTML_Element *element);
};


/**
 * Saves information about one form element for restoration if the
 * user returns to the page that used to have the form data.
 *
 * <p>These are restored one after eachother with script running in
 * between to simulate as much as possible of what the user did in an
 * effort to make it work even on dynamic pages. So far this has
 * proved quite successful but also quite slow since we wait until
 * after the page load has completed.
 *
 * @author jl
 */
class DocumentFormElementState
	: public Link
{
public:
	DocumentFormElementState();
	~DocumentFormElementState();

	DocumentElementPath *path;
	InputType inputtype;
	FormValue *value;
	int serialnr;
	BOOL signal;

	class Option
	{
	public:
		Option();
		~Option();

		int index;
		uni_char *value;
		uni_char *text;
		Option *next;

		BOOL Matches(HTML_Element *element);
	} *options;
};

/**
 * Saves information about what was in a certain frame in a document
 * for use in history navigations.
 *
 * @author jl
 */
class DocumentFrameState
	: public Link
{
public:
	DocumentFrameState();
	~DocumentFrameState();

	DocumentElementPath *path;
	FramesDocElm *fdelm;
};

/**
 * An object that encapsulates everything a user has customized while
 * visiting a page so that we can restore that if the user returns to
 * the document when navigating in history.
 *
 * <p>This includes:
 * <ul>
 * <li>Text selection
 * <li>Focused element
 * <li>Form data
 * <li>Contents in frames
 * </ul>
 *
 * @author jl
 */
class DocumentState
{
protected:
	DocumentState(FramesDocument *doc);

	FramesDocument *doc;

	DocumentElementPath *focused_element;
	DocumentElementPath *navigation_element;

	BOOL highlight_navigation_element;

#ifndef HAS_NOTEXTSELECTION
	DocumentElementPath *selection_anchor;
	DocumentElementPath *selection_focus;
#endif // HAS_NOTEXTSELECTION

	Head form_element_states, iframe_states;

	OP_STATUS AddFormElementState(HTML_Element *iter);
	OP_STATUS AddIFrameState(HTML_Element *iter, FramesDocElm *fde);

public:
	static OP_STATUS Make(DocumentState *&state, FramesDocument *doc);

	OP_STATUS StoreIFrames();

	~DocumentState();

	OP_STATUS Restore(FramesDocument *doc);
	OP_BOOLEAN RestoreForms(FramesDocument *doc);

	FramesDocElm *FindIFrame(HTML_Element *element);
};

#endif // DOCUMENTSTATE_H
