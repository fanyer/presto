/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

/** @file doc_st_utils.h
 *
 * provides more sophisticated utilities related with doc, scaling, viewport, ecmascript etc. operations
 * that can be used in selftests
 *
 */

#ifndef DOC_SELFTEST_UTIL_H
#define DOC_SELFTEST_UTIL_H

#include "modules/hardcore/timer/optimer.h"

class FramesDocument;
class FramesDocElm;
class HTML_Element;

class DocSTUtils
{
public:

	static void SetScale(FramesDocument *doc, int scale);

	static FramesDocElm* GetFrameById(FramesDocument *doc, const char* id);
};

/** This class is a base for the following use case in async precondition selftests.
	We initiate some operation, then wait until everything that was expected
	have been done. Passes if it happens withing a reasonable time, fails in
	case of a timeout (5s). */

class AsyncSTOperationObject : public OpTimerListener
{
public:
	virtual void OnTimeOut(OpTimer* timer);
	static FramesDocument* GetTestDocument();

protected:
	AsyncSTOperationObject();

	virtual BOOL CheckPassCondition() = 0;

	/** Stops the timer and deletes self. */
	void Abort();

	OpTimer m_timer;
	int m_time_counter;
};

/// Document loading operations. Pass: document properly loaded.

class WaitUntilLoadedObj : public AsyncSTOperationObject
{
public:
	/**
	 * Opens the url in the Window and waits until loaded (or until timeout occurs).
	 * @param win the window to open url in
	 * @param url url to load
	 */
	static void LoadAndWaitUntilLoaded(Window* win, const uni_char* url);
	/**
	 * Tells to reload the current top doc and waits until loaded (or until timeout occurs).
	 * @param top_doc the document to reload
	 */
	static void ReloadAndWaitUntilLoaded(FramesDocument* top_doc);

	/**
	 * Asks the window to move in history and wait until target doc is loaded.
	 * @param win the window to open url in
	 * @param forward TRUE - move forward one step
	 *	FALSE - move back one step
	 */
	static void MoveInHistoryAndWaitUntilLoaded(Window* win, BOOL forward);

	static OP_STATUS Paint();

protected:

	virtual BOOL CheckPassCondition();

	WaitUntilLoadedObj()
		: AsyncSTOperationObject() {}
};

/// Move the visual viewport. Pass: visual viewport is on the expected position.

class MoveVisualViewportObj : public AsyncSTOperationObject
{
public:
	static void MoveVisualViewport(INT32 x, INT32 y);

protected:

	virtual BOOL CheckPassCondition();

private:

	MoveVisualViewportObj(FramesDocument* doc, INT32 x, INT32 y);

	int x,y;
};

/**	Checks whether a ScrollableContainer has been scrolled. PASS: y scroll pos of a ScrollableContainer
	is greater than 0. WARNING: make sure the HTML_Element* of the scrollable doesn't get deleted during the test. */

class ScrollableScrolledObj : public AsyncSTOperationObject
{
public:
	static void WaitUntilScrollableScrolled(HTML_Element* scrollable);

protected:

	virtual BOOL CheckPassCondition();

private:

	ScrollableScrolledObj(HTML_Element* scrollable) : scrollable(scrollable) {}

	HTML_Element* scrollable;
};

/**	Used to make a check whether some js function has been executed. The js function is expected
	to change the id of some element to "function_executed". PASS: there is an element
	with id "function_executed" in the document. Will not work properly with ecma script disabled.
	WARNING: Make sure the document in which the script runs doesn't get deleted. */

class CheckJSFunctionExecutedObj : public AsyncSTOperationObject
{
public:
	static void WaitUntilJSFunctionExecuted(FramesDocument* doc);

protected:

	virtual BOOL CheckPassCondition();

private:

	CheckJSFunctionExecutedObj(FramesDocument* scripted_doc)
		: scripted_doc(scripted_doc) {}

	FramesDocument* scripted_doc;
};

#ifdef SVG_SUPPORT

/** Waits until SVGImage pointer associated with an HEListElm associated with
	the given html element is ready (that image is either background or list item marker) */

class WaitUntilSVGReadyObj : public AsyncSTOperationObject
{
public:

	/** Starts waiting for the SVG image to be ready.
	 * @param id the id of the html element that the SVG is associated with.
	 */
	static void WaitUntilSVGReady(const char* id);

protected:

	virtual BOOL CheckPassCondition();

	WaitUntilSVGReadyObj(const char* id)
		: AsyncSTOperationObject()
		, id(id) {}

private:

	/// The id of the element, which the SVG image is associate with.
	OpStringC8 id;
};

#endif // SVG_SUPPORT

#endif // not included
