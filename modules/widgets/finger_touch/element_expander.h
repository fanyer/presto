/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ELEMENT_EXPANDER_H
#define ELEMENT_EXPANDER_H

#include "modules/util/simset.h"

class FramesDocument;
class HTML_Element;
class OpWindow;

/** @short Expander of nearby elements of interest.
 *
 * Contains a list of nearby elements of interest, provides functionality to
 * lay them out in a sensible manner, so that the user is able to select one
 * of them easily using a finger.
 *
 * This interface the public API for the fingertouch feature, and is the only
 * file to be included outside of widgets/finger_touch/.
 *
 * Pass an instance of this class to layout for traversal. It will populate it
 * with nearby elements of interest. Then call Show() to display nearby
 * elements of interest. Call Hide() to hide them.
 */
class ElementExpander :
	public Head
{
public:
	/** Factory function, use it to create an ElementExpander object */
	static ElementExpander* Create(FramesDocument* document, OpPoint origin, unsigned int radius);

	virtual ~ElementExpander() {}

	/** Lay out and display expanded elements. Enter expanded elements mode.
	 *
	 * @return IS_TRUE if elements were expanded, IS_FALSE if there turned out
	 * to be no elements to expand, ERR_NO_MEMORY on OOM, ERR for other errors.
	 */
	virtual OP_BOOLEAN Show() = 0;

	/** Hide expanded elements.
	 *
	 * @param animate If TRUE, all elements will typically fade out or
	 * be hidden using some other animation effect. If FALSE, elements
	 * will be hidden immediately.
	 * @param can_ignore If TRUE, the Hide event will be ignored if
	 * the ElementExpander is expanding and no animations have been
	 * started yet. This is a workaround for when tapping really fast.
	 */
	virtual void Hide(BOOL animate, BOOL can_ignore = FALSE) = 0;

	/** Scroll expanded elements of interest as well as the overlay window
	 * @param dx Numbers of pixels to scroll right (or left if negative)
	 * @param dy Numbers of pixels to scroll down (or up if negative)
	 */
	virtual void Scroll(int dx, int dy) = 0;

	/** Invoked when an element is removed from the document */
	virtual void OnElementRemoved(HTML_Element* html_element) = 0;

	/** @return The expected fingertip diameter in pixels. 
	 * @param document is needed by g_op_screen_info to figure
	 * out DPI from the right screen.
	 */
	static int GetFingertipPixelRadius(OpWindow* opWindow);

	/** @return TRUE if detection of nearby elements is enabled. */
	static BOOL IsEnabled();

protected:
	ElementExpander() {}

private:
	// Never to be used
	ElementExpander(const ElementExpander& e);
	void operator=(const ElementExpander& e);
};

#endif // ELEMENT_EXPANDER_H
