/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
**
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SN_UTIL_H
#define SN_UTIL_H

#ifdef SPATIAL_NAVIGATION_ALGORITHM


class FramesDocument;
class HTML_Element;
class Container;
class LogicalDocument;

#define SN_MARGIN 1

#define SN_EMPTY_RECT(r) r.left = r.top = r.right = r.bottom = 0
#define SN_IS_RECT_EMPTY(r) (r.right <= r.left || r.bottom <= r.top)


/**
 * Utility class, providing methods used in different part of the module.
 * Contains no data only static utility functions.
 */
class SnUtil
{
public:
	
	/**
	 * Check if a link is entirly inside the current view.
	 * Can be used for any kind of rectangles.
	 * Returns TRUE if the first element or set of coordinates, are
	 * inside but not identical to the second element (or set of coordinates).
	 */
	static BOOL IsInView(const OpRect &element, const OpRect &view);
	static BOOL IsInView(INT32 x, INT32 y, INT32 w, INT32 h,
						 INT32 viewX, INT32 viewY, 
						 INT32 viewWidth, INT32 viewHeight);
	
	/**
	 * Check if a link is partially or fully inside the current view.
	 * Can be used for any kind of rectangles.
	 * Returns TRUE if the first element or set of coordinates, are
	 * partially inside the second element (or set of coordiantes.
	 */
	static BOOL IsPartiallyInView(const OpRect &element, const OpRect &view);
	static BOOL IsPartiallyInView(INT32 x, INT32 y, INT32 w, INT32 h,
								  INT32 viewX, INT32 viewY, 
								  INT32 viewWidth, INT32 viewHeight);

	/**
	 * Checks if the RECT is partly or fully inside the viewport of the
	 * document. Does not adjust for scrolling.
	 * @param element  The rectangle to check if inside view
	 * @param doc      The document which size will be used to check if is inside
	 * @param iframe_rect A pointer to iframe rectangle in parent document. Must not be NULL, when doc is an iframe, not used otherwise.
	 * @return  TRUE if fully or partially inside */
	static BOOL IsPartiallyInView(const RECT &element, FramesDocument* doc, const RECT* iframe_rect);

	/**
	 * Checks if the RECT is fully inside the viewport in vertical dimension and
	 * fully or partially in horizontal dimension of the document's viewport.
	 * Does not adjust for scrolling.
	 * @param element  The rectangle to check if inside view
	 * @param doc      The document which size will be used to check if is inside
	 * @param iframe_rect A pointer to iframe rectangle in parent document. Must not be NULL, when doc is an iframe, not used otherwise.
	 * @return  TRUE if fully or partially inside */
	static BOOL IsFullyInView(const RECT &element, FramesDocument* doc, const RECT* iframe_rect);

	/**
	 * Checks if the OpRect is partly or fully inside the viewport of the
	 * document. Does not adjust for scrolling.
	 * @param element  The rectangle to check if inside view
	 * @param doc      The document which size will be used to check if is inside
	 * @param iframe_rect A pointer to iframe rectangle in parent document. Must not be NULL, when doc is an iframe, not used otherwise.
	 * @return  TRUE if fully or partially inside */
	static BOOL IsPartiallyInView(const OpRect &element, FramesDocument* doc, const RECT* iframe_rect);

	/**
	 * Gets visible document part (in document coordinates) of doc.
	 * @param[out] visible	An output rectangle
	 * @param doc			The document, which visible part will be passed.
	 * @param iframe_rect	A pointer to iframe rectangle in parent document. Must not be NULL, when doc is an iframe, not used otherwise.
	*/
	static void GetVisibleDocumentPart(OpRect &visible, FramesDocument* doc, const RECT* iframe_rect);

	/** An element fits comfortably if it takes less then 50% of height and width.
	 *  Does not consider if it is actually inside the area now, just if it could fit.
	 *  If it does fit then it can be made fully visible by scrolling by only the 
	 *  regular 50% scrolldistances.
	 *  @param rect  The rect we check if its size fits inside the document window
	 *  @param doc   Document we want to check if it fits inside
	 *  @param iframe_rect a pointer to iframe rectangle in parent document. Must not be NULL, when doc is an iframe, not used otherwise. 
	 *  @return TRUE if it does fits (even if it's not in view now).  FALSE if either
	 *          height or width is more than 50% of document window size. */ 
	static BOOL FitsComfortably(const RECT& rect, FramesDocument* document, const RECT* iframe_rect);

	/** Returns TRUE if the document can be scrolled in direction, otherwise FALSE
	 *  @param iframe_rect a pointer to iframe rectangle in parent document. Must not be NULL, when document is an iframe, not used otherwise.
	*/
	static BOOL CanScroll(INT32 direction, FramesDocument* document, const RECT* iframe_rect);
	
	/** 
	 * Test if interval [a1..a2) overlaps [b1..b2) (a2 and b2 mark the first pixel outside)
	 *
	 *       a1---------------a2
	 *                   b1--------------b2
	 *  In the case above itersect() returns true.
	 * Returns FALSE if a1 >= a2 or b1 >= b2 
	 */
	static BOOL Intersect(INT32 a1, INT32 a2, INT32 b1, INT32 b2);

	/** 
	 * Transpose rectangle.
	 * Rotate the rectangle 'direction' degrees right.
	 */
	static void Transpose(INT32 direction, OpRect& rect);
	static void Transpose(INT32 direction, INT32& x, INT32& y, INT32& w, INT32& h);	

	/** 
	 * UnTranspose rectangle.
	 * Roteate the rectangle 'direction' degrees left.
	 */
//	static void UnTranspose(INT32 direction, OpRect& rect);
//	static void UnTranspose(INT32 direction, INT32& x, INT32& y, INT32& w, INT32& h);	

  
	/**
	 * Get the minimal distance between a point and a rectangle.
	 * If the point is inside the rectangle the distance is 0 otherwise
	 * it's the distance to the closest edge.
	 */
//	static INT32 DistancePointToRect(INT32 x, INT32 y, INT32 w, INT32 h, 
//									 INT32 xRef, INT32 yRef);

	/**
	 * Square an integer. 
	 */
	static inline INT32 sqr(INT32 a) { return a*a; };

	/**
	 * Checks if a doc has either SmartFrames or StackedFrames
	 */
	static BOOL HasSpecialFrames(FramesDocument* frmDoc);

	/**
	 * Gets the correct doc to scroll, fixes frames/iframes pages
	 * in SmartFrames/StackedFrames mode
	 */
	static FramesDocument* GetDocToScroll(FramesDocument* frmDoc);

	static BOOL HasEventHandler(FramesDocument* doc, HTML_Element* hElm, HTML_Element** html_elm = NULL);

	/** 
	 *  Restricts a value to be within given limits
	 *  @param value        the value to restrict
	 *  @param lower_limit  the return value won't be lower than this
	 *  @param upper_limit  the return value won't be higher than this.  upper_limit must be >= lower_limit
	 *  @return             value if it is within the limits, otherwise the closest limiter
	 */
//	static int RestrictedTo(int value, int lower_limit, int upper_limit);
//	static short RestrictedTo(short value, short lower_limit, short upper_limit);
	static long RestrictedTo(long value, long lower_limit, long upper_limit);

	/** Calculate the rectangle of an area of an image map element
	 *  It does this by calculating from the image map's rectangle and adjusting
	 *  for the area's shape, position and size
	 * @param   areaElm			The area element to check
	 * @param	mapElm			The map element the area belongs to
	 * @param	mapRect			The rectangle of the map
	 * @param   areaRect		The resulting rectangle for the given area.  Only valid if function returns TRUE
	 * @return	TRUE if able to calculate a rectangle, otherwise FALSE
	 */
	static BOOL GetImageMapAreaRect(HTML_Element* areaElm, HTML_Element* mapElm, const RECT& mapRect, RECT& areaRect);

	/**
	 *  Get the closest scrollable element that helm is inside. A scrollable
	 *  element is either a ScrollableContainer or a PagedContainer. A
	 *  ScrollableContainer that doesn't have a scrollbar is normally not
	 *  treated as scrollable (see SN_SCROLL_OVERFLOW_HIDDEN). A PagedContainer
	 *  that only contains one page is not treated as scrollable.
	 *  @param      helm The element, which we want to check in which scrollable it is
	 *  @param      doc The document in which the helm is
	 *  @param[out] scrollableRect If not NULL, assigned scrollable element's rect
	 *  						   (@see GetScrollableElementRect()). Undefined if helm
	 *  						   is not inside any scrollable.
	 *  @return     scrollable element which helm is inside, or NULL if none found
	 */
	static HTML_Element* GetScrollableAncestor(HTML_Element* helm, FramesDocument *doc, OpRect* scrollableRect);

	/**
	  *  Get the scrollable container's placeholder's rectangle from CONTENT_BOX.
	  *  The rectangle excludes scrollbars and page control widgets.
	  *  @param      content The scrollable container whose rectangle we want to get.
	  *						 This should either be a ScrollableContainer or a PagedContainer.
	  *  @param      doc The document in which the scrollable is
	  *  @param[out] scrollableRect Scrollable container's placeholder's rectangle.
	  */
	 static void GetScrollableElementRect(class Content* content, FramesDocument* doc, OpRect& scrollableRect);
};

class ElementTypeCheck
{
public:
	static BOOL IsFormObject(HTML_Element* element, FramesDocument* doc);
	static BOOL IsClickable(HTML_Element* element, FramesDocument* doc);
	static BOOL IsHeading(HTML_Element* element, FramesDocument* doc);
	static BOOL IsParagraph(HTML_Element* element, FramesDocument* doc);
	static BOOL IsImage(HTML_Element* element, FramesDocument* doc);
	static BOOL IsLink(HTML_Element* element, FramesDocument* doc);
	/** Checks if the element is a link with the specific protocol
	 * @param element Element to check
	 * @param protocol A string with the protocol prefix to check for 
	 * 		  (e.g. http://, mailto:, tel: etc.). Must not be NULL
	 * @return TRUE if it is a link with the correct type, otherwise FALSE
	 */
	static BOOL IsSpecificLink(HTML_Element* element, const uni_char* protocol);
	static BOOL IsMailtoLink(HTML_Element* element, FramesDocument* doc);
	static BOOL IsTelLink(HTML_Element* element, FramesDocument* doc);
	static BOOL IsWtaiLink(HTML_Element* element, FramesDocument* doc);
	/**
	 * YES if it's explicitly tabbable (has positive tabindex attribute),
	 * NO if it's explicitly non-tabbable (has negative tabindex attribute),
	 * MAYBE otherwise.
	 */
	static BOOL3 IsTabbable(HTML_Element* element);

	static BOOL IsIFrameElement(HTML_Element* elm, LogicalDocument* logdoc);

#ifdef _WML_SUPPORT_
	static BOOL IsWMLLink(HTML_Element* element, FramesDocument* doc);	
#endif // _WML_SUPPORT_

#ifdef _PLUGIN_NAVIGATION_SUPPORT_
	static BOOL IsNavigatePlugin(HTML_Element* element);
#endif // _PLUGIN_NAVIGATION_SUPPORT_
};

#endif // SPATIAL_NAVIGATION_ALGORITHM

#endif // SN_UTIL_H
