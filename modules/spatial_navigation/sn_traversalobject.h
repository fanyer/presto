/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SN_TRAVERSALOBJECT_H
#define SN_TRAVERSALOBJECT_H

#ifdef _SPAT_NAV_SUPPORT_

#include "modules/spatial_navigation/sn_filter.h"
#include "modules/spatial_navigation/sn_algorithm.h"
#include "modules/spatial_navigation/sn_debug.h"

#include "modules/layout/box/box.h"
#include "modules/layout/traverse/traverse.h"

/**
 * SnTraversalObject is used for traversing the page and finding navigatable
 * elements.  Traversing will be started from the SnHandler, and when done with
 * traversing the best element to navigate to (according to the SnAlgorithm and
 * SnNavigationFilter) can be found with GetCurrentElm().
 */
class SnTraversalObject
  : public VisualTraversalObject
{
public:
	enum HitBoxResult {
		NO_HIT,
		HIT_ELEMENT,
		CHANGE_ELEMENT
	};

					SnTraversalObject(FramesDocument* doc, FramesDocument* currentDoc, VisualDevice* visual_device, const RECT& area, short direction, OpSnNavFilter* nav_filter, HTML_Element* previous_element, HTML_Element* previous_scrollable, const RECT& previous_area, BOOL fourway, BOOL reconsider_as_scrollable, const RECT* iframe_rect_ptr, OpVector<HTML_Element> *exclude);
					~SnTraversalObject() { OP_DELETE(algorithm); }

	/** Traverse elements in logical order? */

	virtual BOOL	TraverseInLogicalOrder() const { return FALSE; }

	/** Enter vertical box */

	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);

	/** Enter inline box */

	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);

	/** Enter scrollable content. */

	virtual BOOL	EnterScrollable(const HTMLayoutProperties& props, ScrollableArea* scrollable, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

	/** Leave scrollable content. */

	virtual void	LeaveScrollable(ScrollableArea* scrollable, TraverseInfo& traverse_info);

#ifdef PAGED_MEDIA_SUPPORT

	/** Enter paged container. */

	virtual BOOL	EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info);

	/** Leave paged container. */

	virtual void	LeavePagedContainer(LayoutProperties* layout_props, PagedContainer* content, TraverseInfo& traverse_info);

	/** Enter column in multicol container or page in paged container. */

	virtual BOOL	EnterPane(LayoutProperties* multipane_lprops, Column* pane, BOOL is_column, TraverseInfo& traverse_info);

	/** Leave column in multicol container or page in paged container. */

	virtual void	LeavePane(TraverseInfo& traverse_info);

#endif // PAGED_MEDIA_SUPPORT

	virtual BOOL 	EnterTableRow(TableRowBox* box);

	OP_STATUS		Init(BOOL activeLinkHasLineBreak);

	void			FindNextLink(HTML_Element* root);

	/** Get current element.  I.e. the best element to navigate to so far.
	 *  Only valid after traversal has started.  Will return NULL if no 
	 *  acceptable element has been found
	 * @return	The current element
	 */
	HTML_Element*	GetCurrentElm() const { return currentElm; }

	/** Scrollable (ScrollableContainer or PagedContainer) which current element is inside
	 * @return current scrollable element, if any
	 */
	HTML_Element*	GetCurrentScrollable() const { return currentScrollable; }

	/** Get area of current element.  Only valid if GetCurrentElm returns
	 * 	non-NULL.
	 * @return	area of current element.
	 */
	const RECT&		GetCurrentRect() const { return linkRect; }

	HTML_Element*	GetNavDirElement() { return navDirElement; }
	FramesDocument*	GetNavDirDocument() { return navDirDocument; }

private:

	BOOL			FindAndSetNavDirLinkInFrameset(FramesDocument* frmDoc, const uni_char* id);
	void			FindAndSetNavDirLink(CSS_gen_array_decl* cssdecl);

	/** Checks whether there is a CSS3 nav direction property set in the current
	 *	navigation direction. If there is such, sets the defined nav dir link.
	 * @param layout_props The LayoutProperties of the element to check or
	 * of the parent.
	 * @param The element to check or NULL. If NULL, then the layout_props are
	 * the element's one. Otherwise they are the parent props.
	 * @return TRUE if the nav direction property is present, FALSE otherwise.
	 */
	BOOL			TestNavDirLink(LayoutProperties* layout_props, HTML_Element* element);

	/** Tests if a Box is navigatable
	 *  @param layout_props      The LayoutProperties for the Box we want to check if is navigatable
	 *  @return                  TRUE if the box is navigatable, FALSE otherwise
	 */
	HitBoxResult	HitBox(LayoutProperties& layout_props);

	/** Tests if a element with event handler is navigatable
	 *  @param html_element      The element we want to check if is navigatable
	 *  @return                  NO_HIT if not navigatable, HIT_ELEMENT if navigatable and CHANGE_ELEMENT
	 *                           if it is navigatable and has an outer element with the same event handler.
	 */
	HitBoxResult    HitEventHandlerElement(HTML_Element* html_element);

	/** Test if a BlockBox is navigatable, calls IntersectBox if it is
	 *  @param layout_props      The LayoutProperties for the Box we want to check if is navigatable
	 *  @return					 always FALSE
	 */
	BOOL			TestBlock(LayoutProperties& layout_props);

	/** Test if a TableRowBox is navigatable, calls IntersectBox if it is
	 *  @param layout_props      The LayoutProperties for the Box we want to check if is navigatable
	 *  @return					 always FALSE
	 */
	BOOL			TestTableRow(HTML_Element* elm, const TableRowBox* box);

	/** Test if a InlineBox is navigatable, calls IntersectBox if it is
	 *  @param layout_props      The LayoutProperties for the Box we want to check if is navigatable
	 *  @return					 always FALSE
	 */
	BOOL			TestInline(LayoutProperties& layout_props, const RECT& box_area);

#ifdef SVG_SUPPORT
	/** Test if a SVG box should is navigatable, calls IntersectBox if it is
	 *  @param layout_props      The LayoutProperties for the Box we want to check if is navigatable
	 *  @return					 always FALSE
	 */
	void			TestSVG(LayoutProperties* layout_props, const RECT& box_area);
#endif // SVG_SUPPORT

	/** Resizes the search area to be between the staring point of the search 
	 *  and the best element found so far, or the edge of the 
	 *  page/frame/scollable if no element found yet.
	 */
	void			ResizeSearchArea();

	/** Check if an imagemap should be navigated to
	 * @param	html_element    The element which has an image map
	 * @param   rect			Rectangle of the image map
	 */
	void			CheckUsemapElement(HTML_Element* html_element, const RECT& rect);

	/** When a navigatable element has been found this method is called to 
	 *  determine if it is the best element yet, save it as current element.
	 * @param html_element	The element we have found to be navigatable
	 * @param box_area		The area of the element
	 * @param scrollable	Is this a scrollable element (either ScrollableContainer or PagedContainer)?
	 * @param props         The pointer to element's HTMLayoutProperties, may be NULL
	 */
	void			IntersectBox(HTML_Element* html_element, const RECT& box_area, BOOL scrollable = FALSE, const HTMLayoutProperties* props = NULL);

	/** Sets the current element, i.e. the best element for navigation found
	 *  so far.
	 * @param	html_element	The new element we want to be current
	 * @param	box_area		The area of the new element
	 * @param	props           The pointer to element's HTMLayoutProperties, may be NULL
	 */
	void			SetCurrentElement(HTML_Element* html_element, const RECT& box_area, const HTMLayoutProperties* props = NULL);

	void			SetAlternativeElement(HTML_Element* html_element, const RECT& box_area, const HTMLayoutProperties* props = NULL);

	/** Determine if an element has a DOM event handler
	 * @param	hElm	The element to check
	 * @param	handler If not NULL, this will be set to the element where the 
	 * 					handler is attached.  Will be hElm or one of its 
	 * 					parents. Only valid if return value of function is TRUE.
	 * @return			TRUE if hElm has some sort of event handler.
	 */
	BOOL			HasEventHandler(HTML_Element* hElm, HTML_Element** handler = NULL);

	/** 
	 * Distance in the locical tree from lastElm to the common parent
	 * of lastElm and html_element.
	 * @param	html_elememnt	Element to compare with lastElm.
	 * @return					The distance. If more than SN_SEARCH_HEIGHT return INT32_MAX.
	 */
	INT32			DistanceToLastElm(HTML_Element* html_element);

	/** 
	 * Same as DistanceToLastElm but only counts the symetrical distance.
	 * @param	html_elememnt	Element to compare with lastElm.
	 * @return					The distance. If more than SN_SEARCH_HEIGHT or if lastElm and 
	 *							html_element is not on the same level return INT32_MAX.
	 */
	INT32			SymDistanceToLastElm(HTML_Element* html_element);


	RECT			linkRect;		///< area of best element found so far. Connected with currentElm.
	HTML_Element*	currentElm;     ///< best navigatable element found so far.
	HTML_Element*	currentScrollable; ///< current scrollable (ScrollableContainer or PagedContainer) we're inside
	HTML_Element*	lastElm;
	BOOL            reconsider_as_scrollable;
	HTML_Element*	lastHandlerElm;
	HTML_Element*	lastScrollable;
	RECT			lastRect;		///< area from which we start navigation (e.g. area of current highlighted element). Connected with lastElm.
	HTML_Element*	alternativeElement;
	RECT			alternativeRect;		///< area of alternative element (if any)

	short			direction;		///< direction we're searching for links in. 90 = right, 0 = up, 180 = left, 270 = down
	OpSnNavFilter*	nav_filter;		///< filter used for determining if an element is navigatable
	BOOL			fourway;		///< does this navigation use fourway or twoway navigation
	BOOL			inCurrentScrollable; ///< if is currently inside a scrollable (ScrollableContainer or PagedContainer)
	OpSnAlgorithm*	algorithm;		///< algorithm used for determining if an element is navigatable

	FramesDocument*	currentDoc;

	HTML_Element*	navDirElement;
	FramesDocument*	navDirDocument;

	const RECT*		iframe_rect_ptr;	///<a pointer to iframe rectangle in parent doc, if searching inside an iframe document, NULL otherwise

#ifdef SN_DEBUG_TRACE
	SnTrace			trace;
#endif // SN_DEBUG_TRACE

	INT32 bestElmDist;	///< logical distance from lastElm up to common parrent of lastElm and html_element
	OpVector<HTML_Element> *exclusions; // Exclusions list - used to skip some elements when it's known there's no point inspecting them
};

#endif // _SPAT_NAV_SUPPORT_

#endif // SN_TRAVERSALOBJECT_H
