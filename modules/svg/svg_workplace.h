/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SVG_WORKPLACE_H
#define SVG_WORKPLACE_H

#ifdef SVG_SUPPORT

class SVGImage;
class SVGImageRef;

#ifndef SVG_DOCUMENT_CLASS
# define SVG_DOCUMENT_CLASS FramesDocument
#endif // !SVG_DOCUMENT_CLASS

#include "modules/layout/traverse/traverse.h" // for TextSelectionPoint

class SVG_DOCUMENT_CLASS;
class CSS_WebFont;

/**
 * This class is the part of LogicalDocument that takes care of
 * everything SVG related.
 */
class SVGWorkplace
{
public:
	class LoadListener
	{
	public:
		virtual ~LoadListener() {}
		virtual void	OnLoad(FramesDocument* doc, URL url, HTML_Element* resolved_element) = 0;
		virtual void	OnLoadingFailed(FramesDocument* doc, URL url) = 0;
	};

	virtual ~SVGWorkplace() {}

	/** Clean up all document-related references held by this workplace. */
	virtual void Clean() = 0;

	/**
	 * Create a new SVGWorkplace.
	 *
	 * @param svg_workplace A newly allocated SVGWorkplace will be returned here
	 * @param document The document to create a workplace in, must be non-NULL
	 */
	static OP_STATUS Create(SVGWorkplace** svg_workplace, SVG_DOCUMENT_CLASS* document);

	/**
	 * Sets the visible flag for SVG:s dependent on the scroll position.
	 *
	 * @param visible_area The document area visible on
	 * screen. Anything outside doesn't need to bother with painting
	 * related calculations.
	 */
	virtual void RegisterVisibleArea(const OpRect& visible_area) = 0;

	/**
	 * Set as current document
	 *
	 * Call when the document containing this workplace has been
	 * enabled/disabled.
	 *
	 * @param state TRUE if it is the current document, FALSE otherwise
	 */
	virtual OP_STATUS SetAsCurrentDoc(BOOL state) = 0;

	/**
	 * There can be at most one textselection per LogicalDocument. A
	 * LogicalDocument can contain multiple svg fragments.
	 *
	 * @return TRUE if any svg fragment has selected text, FALSE otherwise
	 */
	virtual BOOL HasSelectedText() = 0;

	/**
	 * Gives selection end-points for the current selection or caret-position.
	 *
	 * @param anchor The selection end-point where the user STARTED the selection, that is, the end-point where
	 * the caret is NOT located. This is if we're not having a selection and just the caret, then will both anchor
	 * and focus be the caret end-point.
	 * @param focus The selection end-point where the user is currently selection, that is, the end-point where
	 * the caret IS located. Unfortunately we currently might have situations where the caret is not located at
	 * any of the selection end-points, then anchor will be the start of the selection and focus the end, in logical
	 * order.
	 * @return TRUE if we succeeded to get a selection.
	 */
	virtual BOOL GetSelection(SelectionBoundaryPoint &anchor, SelectionBoundaryPoint &focus) = 0;

	/**
	 * Initiates loading of an SVG from a URL. This call is not synchronous, so you'll get
	 * an SVGImageRef 'ticket' to collect your result later on via GetSVGImage().
	 *
	 * @param url The url to the SVG to load
	 * @param out_ref A reference to this load, which may be used with GetSVGImage to get
	 *        the resulting SVGImage.
	 */
	virtual OP_STATUS GetSVGImageRef(URL url, SVGImageRef** out_ref) = 0;

	/**
	 * Returns the first SVG on this page. "First" doesn't
	 * necessarily mean that it is earliest in the document,
	 * but can be in any order. Only real SVG:s are returned,
	 * i.e. no SVG:s that are part of another SVG is included.
	 *
	 * @return NULL if there are no SVG:s.
	 */
	virtual SVGImage* GetFirstSVG() = 0;

	/**
	 * Returns the next SVG on this page. Only real SVG:s are returned,
	 * i.e. no SVG:s that are part of another SVG is included.
	 *
	 * @param prev_svg An SVG returned from an earlier call to GetFirstSVG or GetNextSVG.
	 *
	 * @return NULL if there are no more SVG:s.
	 */
	virtual SVGImage* GetNextSVG(SVGImage* prev_svg) = 0;

	 /**
	  * Loads the CSS resource pointed to by the given url. The listener may get called from
	  * this method, if the resource is already available.
	  *
	  * @param url The resource to load (must contain a rel-part to return anything valid)
	  * @param listener The listener
	  *
	  * @return OpStatus::ERR if generic error, OpStatus::ERR_NO_MEMORY on OOM, OpStatus:OK otherwise
	  */
	virtual OP_STATUS LoadCSSResource(URL url, SVGWorkplace::LoadListener* listener) = 0;

	/**
	 * Invalidate fonts on all SVGs.
	 *
	 * SVG doesn't yet set attribute styles during the default style
	 * loading stage, as HTML does.
	 *
	 * This means that marking properties dirty alone isn't
	 * enough, we need to send an extra notification to the SVG
	 * code that WebFonts have changed.
	 *
	 * This work-around can be removed when the SVG attribute styles
	 * are set in GetDefaultStyleProperties().
	 *
	 * Note: This invalidates the _use_ of fonts, not the actual fonts
	 * themselves.
	 *
	 */
	virtual void InvalidateFonts() = 0;

	/**
	 * Stop loading the CSS resource pointed to by the given url + listener.
	 *
	 * @param url The resource to stop loading
	 * @param listener The listener that is no longer interested in the given resource
	 */
	virtual void StopLoadingCSSResource(URL url, SVGWorkplace::LoadListener* listener) = 0;

	/**
	 * Get default style properties for a SVG element
	 *
	 * Map attributes considered as presentational hints to style. Consider this the
	 * SVG extension of CSSCollection::GetDefaultStyleProperties().
	 */
	virtual OP_STATUS GetDefaultStyleProperties(HTML_Element* element, CSS_Properties* css_properties) = 0;

};

#endif // SVG_SUPPORT
#endif // _SVG_WORKPLACE_
