/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** @file layout_workplace.h
 *
 * class prototypes for LayoutWorkplace
 *
 */

#ifndef _LAYOUT_WORKPLACE_H_
#define _LAYOUT_WORKPLACE_H_

#include "modules/util/OpRegion.h"
#include "modules/util/simset.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/selectionpoint.h"
#include "modules/layout/layout.h"
#include "modules/layout/counters.h"
#include "modules/layout/layoutprops.h"
#include "modules/layout/element_search.h"
#include "modules/layout/transitions/transitions.h"
#include "modules/windowcommander/OpViewportController.h"

class FramesDocument;
class ReplacedContent;

class DocRootProperties
{
private:

	/** Font size of Markup::HTE_BODY or WE_CARD.
		Constrained by min/max font size settings. */
	LayoutFixed		body_font_size;

	short			body_font_number; ///< Font number of Markup::HTE_BODY or WE_CARD.
	COLORREF		body_font_color; ///< Font color of Markup::HTE_BODY or WE_CARD.
	Border			border;

	LayoutFixed		root_font_size; ///< Font size of the html element (or <svg>).

public: // The gogi module (!) accesses this one directly :(
	COLORREF		bg_color;
private:

	BgImages		bg_images;

	/* DocRootProperties has references to these declarations */


	CSS_decl*		bg_images_cp;
	CSS_decl*		bg_repeats_cp;
	CSS_decl*		bg_attachs_cp;
	CSS_decl*		bg_positions_cp;
	CSS_decl*		bg_origins_cp;
	CSS_decl*		bg_clips_cp;
	CSS_decl*		bg_sizes_cp;

	Markup::Type
					bg_element_type;
	BOOL			bg_element_changed;

	/* Corresponds to background: transparent */
	BOOL			bg_is_transparent;

	void			DereferenceDeclarations();

	// Don't copy instances of this class
	DocRootProperties(const DocRootProperties& copy);
	DocRootProperties& operator=(const DocRootProperties& copy);

public:
	DocRootProperties() :
		root_font_size(0),
		bg_images_cp(0),
		bg_repeats_cp(0),
		bg_attachs_cp(0),
		bg_positions_cp(0),
		bg_origins_cp(0),
		bg_clips_cp(0),
		bg_sizes_cp(0) { ResetBackground(); }

	~DocRootProperties() { DereferenceDeclarations(); }

	void			Reset();

	void			ResetBackground() { bg_color = USE_DEFAULT_COLOR; bg_images.Reset(); bg_element_type = Markup::HTE_DOC_ROOT; bg_is_transparent = FALSE; bg_element_changed = FALSE; }
	BOOL			HasBackground() const { return bg_color != USE_DEFAULT_COLOR || !bg_images.IsEmpty(); }
	BOOL			HasTransparentBackground() const { return bg_is_transparent; }

	const Border&	GetBorder() const { return border; }
	COLORREF		GetBackgroundColor() const { return bg_color; }
	const BgImages&	GetBackgroundImages() const { return bg_images; }
	Markup::Type
					GetBackgroundElementType() const { return bg_element_type; }
	BOOL			HasBackgroundElementChanged() const { return bg_element_changed; }
	COLORREF		GetBodyFontColor() const { return body_font_color; }

	/** Get the font size of Markup::HTE_BODY or WE_CARD in layout fixed point format,
		but constrained by min/max font size settings. */

	LayoutFixed		GetBodyFontSize() const { return body_font_size; }

	int				GetBodyFontNumber() const { return body_font_number; }
	LayoutFixed		GetRootFontSize() const { return root_font_size; }

	void			SetBorder(const Border& b) { border = b; }
	void			SetBackgroundColor(COLORREF c) { bg_color = c; }
	void			SetBackgroundImages(const BgImages& bg);
	void			SetBackgroundElementType(Markup::Type element_type) { bg_element_type = element_type; }
	void			SetBackgroundElementChanged() { bg_element_changed = TRUE; }
	void			ClearBackgroundElementChanged() { bg_element_changed = FALSE; }
	void			SetBodyFontData(LayoutFixed size, short number, COLORREF color) { body_font_size = size; body_font_number = number; body_font_color = color; }
	void			SetTransparentBackground(BOOL v) { bg_is_transparent = v; }
	void			SetRootFontSize(LayoutFixed value) { root_font_size = value; }
};

class ReflowElem
	: public Link,
	  public ElementRef
{
private:

	HTML_Element* const
					element;

public:
					ReflowElem(HTML_Element* he)
						: element(he) { SetElm(he); }

	HTML_Element*	GetElement() { return element; }

	virtual	void	OnDelete(FramesDocument *document) { Link::Out(); OP_DELETE(this); }

	void*			operator new(size_t nbytes) OP_NOTHROW { return g_reflowelem_pool->New(sizeof(ReflowElem)); }
	void			operator delete(void* p, size_t nbytes) { g_reflowelem_pool->Delete(p); }
};

#ifdef SUPPORT_TEXT_DIRECTION

enum DocBidiStatus {
	VISUAL_ENCODING,	// this document is in visual encoding
	LOGICAL_MAYBE_RTL,  // this document is in logical encoding, and, we assume, no RTL chars
	LOGICAL_HAS_RTL		// this document has RTL characters in logical encoding
};

#endif // SUPPORT_TEXT_DIRECTION

/** Replaced content backed up while its box is being recreated (during reflow).
 *
 * When a box with a certain type of replaced content (e.g. plug-ins) is being
 * deleted during reflow, its replaced content must not be deleted, because
 * that would cause the content to be reset (e.g. for a movie plug-in, the
 * movie may start over from the beginning) when the box is recreated.
 */
class StoredReplacedContent
  : public Link
{
private:
	HTML_Element*		element;
	ReplacedContent*	content;

public:
						StoredReplacedContent(HTML_Element* html_element, ReplacedContent* replaced_content)
						  : element(html_element),
							content(replaced_content) {}

	HTML_Element*		GetElement() const { return element; }
	ReplacedContent*	GetContent() const { return content; }
};

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
class InteractiveItemSearchCustomizer
	: public ElementSearchCustomizer
{
public:
	InteractiveItemSearchCustomizer()
		: ElementSearchCustomizer(TRUE, TRUE, FALSE) {}

	virtual BOOL AcceptElement(HTML_Element* elm, FramesDocument* doc);
};
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION


/** Layout workplace.

	This class deals with layout related aspects of a document, such as
	managing reflows.

	Among other things it controls the layout viewport.
	See http://projects/Core2profiles/specifications/mobile/rendering_modes.html#viewports */

class LayoutWorkplace
{
public:
	LayoutWorkplace(FramesDocument* theDoc);

	/**
	  * Delete this LayoutWorkplace instance
	  *
	  */

	~LayoutWorkplace();

	enum ForceLayoutChanged {
		NO_FORCE,				// dont force layout_changed_flag
		FORCE_FROM_ELEMENT,		// from layout changed from a particular element
		FORCE_ALL				// force for all elements
	};

	/**
	  * Return the FramesDocument this LayoutWorkplace belongs to
      *
	  * @return The FramesDocument we belong to
	  *
	  */

	FramesDocument* GetFramesDocument() { return doc; }

	/**
	  *
      *
	  *
	  *
	  */
	void			SetFramesDocument(FramesDocument* aDoc) {doc = aDoc; } // Needed because of (poor) printing (design).

	/** @return TRUE if layout is traversable, FALSE if a reflow or CSS property reload is required. */
	BOOL			IsTraversable() const;

#ifdef DISPLAY_SPOTLIGHT
	/** Add a spotlight on the specified element. */
	OP_STATUS		AddSpotlight(VisualDevice* vd, void* html_element_id);

	/** Remove a spotlight on the specified element. */
	OP_STATUS		RemoveSpotlight(VisualDevice* vd, void* html_element_id);
#endif

	/** Signal that a HTML element is about to be removed from the tree.
	 *
	 * Under some circumstances (when it is allowed to traverse a dirty
	 * structure) it may be necessary to make changes to the Box/Content
	 * structure when an HTML element is deleted.
	 */
	void			SignalHtmlElementRemoved(HTML_Element* html_element);

	/** Reflow the document. Adjust, create, or regenerate layout boxes as necessary.

		@param reflow_complete[out] Will be set to TRUE if reflow occured and
		completed (i.e. reflow was necessary, and we were allowed to perform as many reflow
		iterations as necessary), FALSE otherwise
		@param set_properties Set layout properties (and regenerate all layout boxes) even if the
		root element isn't extra dirty
		@param iterate If TRUE, perform more than one reflow iteration if necessary (it is
		necessary to reflow more than once for e.g. certain layout boxes, such as shrink-to-fit
		containers or tables) */

	OP_STATUS		Reflow(BOOL& reflow_complete, BOOL set_properties, BOOL iterate = FALSE);

	/** End forced flushing.
	 *
	 * This will mark elements damaged by forced flushing dirty.
	 */
	OP_STATUS		EndForcedFlush();

	/** Add this element to the reflow list */
	void			SetReflowElement(HTML_Element* html_element, BOOL check_if_exist = FALSE);

#ifdef PAGED_MEDIA_SUPPORT

	/** Set pending page break point in the specified element. */

	void			SetPageBreakElement(LayoutInfo& info, HTML_Element* html_element);

	/** Get the height of the root paged container control's height, if any.

		This is the height of the page control that was used when the layout
		viewport height was calculated. */

	LayoutCoord		GetPageControlHeight() const { return page_control_height; }

#endif // PAGED_MEDIA_SUPPORT

	/** Call SignalChange() on layout boxes for all HTML_Elements in the reflow list.

		This will mark the elements that need another reflow pass dirty. */

	void			ReflowPendingElements();

	/**
	 * If this document is within an iframe with -o-content-size sizing, send a SignalChange on the iframe element
	 * in the parent document
	 *
	 */
	void			HandleContentSizedIFrame(BOOL root_has_changed = FALSE);

	/** Remove all counters associated with this document. */

	void			ClearCounters() { counters.Clear(); }

	/** Get the "counter manager", the thing that keeps track of all counters in this document. */

	Counters*		GetCounters() { return& counters; }

	/** Return TRUE when storage of replaced content is enabled. */
	BOOL			StoreReplacedContentEnabled() const { return enable_store_replaced_content; }

  	/** Store the ReplacedContent object of a box that is about to be deleted.
	 *
	 * If a box is recreated for this element, and it's still going to display
	 * replaced content, GetStoredReplacedContent() will be called to get back
	 * the ReplacedContent stored.
	 *
	 * @return TRUE if successful, FALSE on OOM
	 */
	BOOL			StoreReplacedContent(HTML_Element* element, ReplacedContent* content);

	/** Get the stored ReplacedContent object for the specified HTML_Element.
	 *
	 * If an entry was found for the element, it will be removed from the list
	 * of stored replaced content and returned.
	 */
	ReplacedContent*
					GetStoredReplacedContent(HTML_Element* element);

	/** Signal that an HTML_Element's layout box is about to be deleted. */

	void			LayoutBoxRemoved(HTML_Element* html_element);

#ifdef SUPPORT_TEXT_DIRECTION
	/**
	  *
	  *
	  *
	  */
	void            SetBidiStatus(DocBidiStatus status) { doc_bidi_status = status; }

	/**
	  *
	  *
	  *
	  */

	DocBidiStatus   GetBidiStatus() { return doc_bidi_status; }

	/**
	  * Set the flag that this is a right to left document (body has direction:rtl, either by itself or inherited)
	  *
	  * @param rtl TRUE if the document is a rtl document
	  */

	void			SetRtlDocument(BOOL rtl) { is_rtl_document = rtl; }

	/**
	  *  Is this a rtl document (body has direction:rtl, either by itself or inherited)
	  *
	  * @return TRUE if this is a rtl document
	  *
	  */

	BOOL			IsRtlDocument() { return is_rtl_document; }

	/** Get the absolute value of this document's negative (leftwards) overflow. */

	int				GetNegativeOverflow();

#endif // SUPPORT_TEXT_DIRECTION

	/**
	  *
	  *
	  *
	  */

	int				GetReflowCount() const { return reflow_count; }

#if LAYOUT_MAX_REFLOWS > 0
	void            ResetInternalReflowCount() { internal_reflow_count = 0; }
#endif

	/**
	  *
	  *
	  *
	  */

	double			GetReflowStart() const { return reflow_start; }

	double			GetReflowTime() const { return reflow_time; }

	void			StoreTranslation();

	/*********** Yield methods **********/

	/**
	 * Move yield element if it is about to be deleted
	 *
	 * @param removed The element that is about to be removed
	 */
	void			ValidateYieldElement(HTML_Element* removed);

	/**
	 * Set the allow yield flag on this workplace and its children (iframes)
	 *
	 * @param set Set or unset yield allowance
	 *
	 */
	void			SetThisAndChildrenCanYield(BOOL set);

	/**
	 * Get the element about to be yielded. If this is non-null, it means we are about to restart
	 * reflowing from the yield cascade, using the next element from the yield element as first child.
	 * This requires that the yield cascade is valid.
	 *
	 * @return The element we yielded at
	 *
	 */

	HTML_Element*	GetYieldElement() { return yield_element; }

	/**
	 *  Set the element to yield from
	 *
	 * Likely this will mean that the next message can resume from this position
	 * reusing the cascade. But if not the elements will be marked dirty using the
	 * MarkYieldCascade function
	 *
	 * @param el Element to yield from
	 *
	 *
	 */
	void			SetYieldElement(HTML_Element* el) { yield_element = el; if (yield_element) yield_element->SetReferenced(TRUE); }

	/**
	 * Was any child of the element yielded from extra dirty?
	 * This would mean that the yield element would need to be marked
	 * extra dirty when dismounting the yield cascade due to changes in the tree (MarkYieldCascade)
	 *
	 * @return TRUE if a child of hte yield element was extra dirty
	 *
	 **/

	BOOL			GetYieldElementExtraDirty() { return yield_element_extra_dirty; }

	/**
	 * A child element of the yield element was extra dirty
	 * @see GetYieldElementExtraDirty
	 *
	 * @param set
	 *
	 */
	void			SetYieldElementExtraDirty(BOOL set) { yield_element_extra_dirty = set; }

	/**
	 * Tell the workplace that this operation may yield and that a reflow call may return unfinished
	 * with a OpStatus::ERR_YIELD code
	 *
	 * @param set TRUE if a reflow may yield
	 *
 	 */

	void			SetCanYield(BOOL set);

	/**
	 * Any reflow call is currently allowed to yield
	 *
	 */
	inline BOOL		CanYield() { return can_yield && max_reflow_time > 0; }

#ifdef LAYOUT_YIELD_REFLOW
	/**
	 * Next reflow will need to set the layout changed flag.
	 * This is because the previous reflow was interrupted by yield, while
	 * the layout_changed flag was set.
	 *
	 * @return flag, @see ForceLayoutChanged
	 */
	ForceLayoutChanged
					GetYieldForceLayoutChanged() { return yield_force_layout_changed; }

	/**
	 * This reflow is yielding put the layout_changed flag was set somewhere in the cascade.
	 * (or we are resetting the flag)
	 *
	 * @see GetYieldForceLayoutChanged
	 *
	 * @param set flag.
	 *
	 */
	void			SetYieldForceLayoutChanged(ForceLayoutChanged set) { yield_force_layout_changed = set; }

	/**
	 * The tree changed while we had a yield cascade
	 * This means that we need to mark the yield cascade dirty and dismount it.
	 *
	 * @param from The element that caused the tree change.
	 *
	 */
	void			MarkYieldCascade(HTML_Element* from);


	/**
	 * How long time may a reflow take? Based on MS_BEFORE_YIELD, but will double when
	 * we have yielded too much
	 */
	int				GetMaxReflowTime() { return max_reflow_time; }

	/*********** End yield methods **********/

#endif // LAYOUT_YIELD_REFLOW

	/** Return TRUE if we're in FlexRoot mode. */

	BOOL			UsingFlexRoot() const;

	/** Get layout viewport X position.

		This is the X scroll position, as far as the layout engine is
		concerned.

		@see GetLayoutViewport() */

	LayoutCoord		GetLayoutViewX() const { return layout_view_x; }

	/** Get layout viewport Y position.

		This is the Y scroll position, as far as the layout engine is
		concerned.

		@see GetLayoutViewport() */

	LayoutCoord		GetLayoutViewY() const { return layout_view_y; }

	/** Return the width of the layout viewport (initial containing block).

		@see GetLayoutViewport() */

	LayoutCoord			GetLayoutViewWidth() const { return layout_view_width; }

	/** Return the minimum width of the layout viewport (initial containing block).

		Normally the same as GetLayoutViewWidth(), but it could differ in
		FlexRoot mode.

		@see GetLayoutViewport()
		@see GetLayoutViewWidth() */

	LayoutCoord		GetLayoutViewMinWidth() const { return layout_view_min_width; }

	/** Return the maximum width of the layout viewport (initial containing block).

		Normally the same as GetLayoutViewWidth(), but it could differ in
		FlexRoot mode.

		@see GetLayoutViewport()
		@see GetLayoutViewWidth() */

	LayoutCoord		GetLayoutViewMaxWidth() const { return layout_view_max_width; }

	/** Return the height of the layout viewport (initial containing block).

		@see GetLayoutViewport() */

	LayoutCoord		GetLayoutViewHeight() const { return layout_view_height; }

	/** Get the current layout viewport of this document.

		The layout viewport is in document coordinates and dimensions.

		This is the scroll position and initial containing block size as far as
		CSS is concerned.

		For a top-level document in "normal desktop mode" (whatever that means
		anymore) with 100% zoom, the size of the layout viewport would
		traditionally be the same as the window size. However, we override this
		depending on mode, configuration and content type (examples: FlexRoot,
		zoom, viewport meta tag, preference overrides). */

	OpRect			GetLayoutViewport() const;

	/** Set new layout viewport position.

		This sets a new scroll position, as far as the layout engine is
		concerned.

		@param x X position, in document coordinates.
		@param y Y position, in document coordinates.

		@return The area that needed to be repainted because of the viewport
		position change (typically due to fixed-positioned content). Note that
		this area has already been scheduled for repaint by this method, but
		the caller may need this information to prevent the area from being
		scrolled. */

	OpRect			SetLayoutViewPos(LayoutCoord x, LayoutCoord y);

	/** Recalculate layout viewport size.

		This is necessary to do when something that may affect the layout
		viewport has changed, such as window or frame size, zoom factor,
		history navigation, etc.

		The document will be scheduled for reflow if the layout viewport
		changed. */

	void			RecalculateLayoutViewSize();

	/** FlexRoot width changed during reflow. */

	void			SignalFlexRootWidthChanged(LayoutCoord new_width) { layout_view_width = new_width; }

	/** Recalculate need for scrollbars and display / hide them.

		This will enable and disable scrollbars, based on the size of the
		document's contents and the scrollbar settings (on/off/auto). This
		method will NOT trigger a reflow if the scrollbar situation changes. It
		is up to the caller to do that, based on the return value.

		This method will also detect changes in size and visibility of the root
		container's page control widget, if it has such a thing.

		@param keep_existing_scrollbars If TRUE, do not remove scrollbars, even
		if the size of the document's contents would suggest that it would be
		appropriate. This is necessary during reflow loops, to make sure that
		we don't loop forever.

		@return TRUE if the scrollbar situation changed (i.e. scrollbars were
		added or removed), FALSE otherwise. */

	BOOL			RecalculateScrollbars(BOOL keep_existing_scrollbars = FALSE);

	/** Recalculate device media queries.

	    This is necessary to do if the screen properties have
	    changed. The queries will only be re-evaluted if the
	    conditions have changed. */

	void			RecalculateDeviceMediaQueries();

	/** Calculate and set the size of the root frameset of this document.

		This is the first step in reflowing a frameset document. */

	void			CalculateFramesetSize();

	/** Get the width used to match width media queries. */

	LayoutCoord		GetMediaQueryWidth() const { return media_query_width; }

	/** Get the height used to match height media queries. */

	LayoutCoord		GetMediaQueryHeight() const { return media_query_height; }

	/** Get the width used to match device-width media queries. */

	LayoutCoord		GetDeviceMediaQueryWidth() const { return media_query_device_width; }

	/** Get the height used to match device-height media queries. */

	LayoutCoord		GetDeviceMediaQueryHeight() const { return media_query_device_height; }

    /** Get "normal" width to use in ERA mode.
	 *
     * Normal (minimum) width of document caused by one or more
     * absolute positioned elements. A zero value means that there is
     * no absolute element placed outside right side of view. This
     * value is only used if the Absolute Positioned Strategy setting
     * is "scale".
     *
     * @return 0 if width not extended by any absolutely positioned
     * block, non-zero otherwise.
     */
	LayoutCoord		GetNormalEraWidth() const { return normal_era_width; }

	/**
	 * As GetLayoutViewWidth() but a little different for printing. Ask mg.
	 *
	 * @returns A number in document units that might be little different than
	 * the normal GetLayoutViewWidth() for printing. Don't know if it's larger
	 * or smaller and why the normal GetLayoutViewWidth() doesn't work.
	 */
	LayoutCoord		GetAbsScaleViewWidth() const;

    /**
     * How absolute positioned elements should be treated. If their
     * positions and sizes should be scale or not. As the layout team
     * for details.
     *
     * @return TRUE if they should scale, FALSE otherwise.
     */
	BOOL			GetScaleAbsoluteWidthAndPos();

	/**
	 * Calculates the width of the root container. Useful e. g. when determining scrolling viewport,
	 * because it uses the content bounding box, which excludes shadows and outlines.
	 *
	 * @return 0 if the root has no layout box or the root container's overflow is hidden; otherwise,
	 *         the width of the content bounding box.
	 */
	int				GetDocumentWidth();

	/**
	 * Calculates the height of the root container. Useful e. g. when determining scrolling viewport,
	 * because it uses the content bounding box, which excludes shadows and outlines.
	 *
	 * @return 0 if the root has no layout box or the root container's overflow is hidden; otherwise,
	 *         the height of the content bounding box.
	 */
	long			GetDocumentHeight();

	/** Set the overflow properties on the viewport.

		The layout module is responsible for setting these values and
		keeping them up-to-date. */

	void			SetViewportOverflow(short overflow_x, short overflow_y);

	/** Get the overflow-x property for the viewport.

		Used to determine how to render root scrollbars or whether or not to
		lay out in a root paged container. */

	short			GetViewportOverflowX() const;

	/** Get the overflow-y property for the viewport.

		Used to determine how to render root scrollbars or whether or not to
		lay out in a root paged container. */

	short			GetViewportOverflowY() const;

	/** Add fixed-positioned content area.

		The area needs to follow the layout viewport, i.e. it needs to be
		invalidated when the layout viewport moves.

		@param rect The rectangle in document coordinates. */

	void			AddFixedContent(const OpRect& rect);

	/** @return TRUE if there are fixed backgrounds in the document, FALSE otherwise. */

	BOOL			HasBgFixed() { return has_bg_fixed; }

	/** Specify that the background is fixed-positioned. */

	void			SetBgFixed() { has_bg_fixed = TRUE; }

	/** Reset information about fixed-positioned content. */

	void			ResetFixedContent();

	/** Get fixed-positioned content rectangle. */

	const OpRect&	GetFixedContentRect() const { return fixed_positioned_area; }

	/** Mark elements in the document tree dirty or extra dirty in ERA mode if necessary when
		crossing a crossover size limit.

		@param apply_doc_styles_changed ERA pref "mds" has changed.
		@param support_float ERA pref "ifl" has changed.
		@param column_strategy_changed ERA pref "ttt" has changed. */

	void			ERA_LayoutModeChanged(BOOL apply_doc_styles_changed, BOOL support_float, BOOL column_strategy_changed);

	BOOL			HasDirtyContentSizedIFrameChildren();

	/** Restart content sized iframe width calculation. */

	void			RestartContentSizedIFrames();

	/** *** IMPORTANT! DO NOT USE! ***

		There are existing uses of this method from SVG, but we don't quite know how it could be
		rewritten yet.

		Load CSS properties for a a single HTML_Element. The values are stored in the HTML_Element
		as CssPropertyItems and used when calculating the cascade. This method should NOT be used for reloading
		CSS properties for dirty-props elements which have had CSS properties loaded before. That would cause
		crash/invalidation/reflow/update problems because this method does not check for changes.
		ApplyPropertyChanges sets dirty-props correctly on changes and the changes are applied
		lazily by ReloadCssProperties.

		@param element The HTML_Element to load css properties for.

		@return OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS		UnsafeLoadProperties(HTML_Element* element);

	/** Apply css property changes to affected elements when an attribute (including style) or pseudo state
		changes on a given HTML_Element. The method recalculates the dynamic pseudo state of the HTML_Element
		for the pseudo classes provided in the update_pseudo bitset. This method takes into account that
		attribute or pseudo state changes can affect the properties in the subtree below the affected element
		because of descendant selectors, and siblings because of sibling selectors, and will recursively check
		properties for children if necessary (only if recurse parameter is TRUE). Note that this method only
		marks the CSS properties dirty, the actual changes are applied lazily in ReloadCssProperties.

		@param element The HTML_Element that changes its attributes or pseudo state.
		@param update_pseudo A set of bits for dynamic pseudo classes to be updated on element. There are
							 convenience macros called CSS_PSEUDO_CLASS_GROUP_MOUSE and CSS_PSEUDO_CLASS_GROUP_FORM
							 defined in style.
		@param recurse If TRUE, property changes are applied to children and siblings if necessary.
		@param attr If the property changes are applied because of an attribute change, we need the attribute idx to
					decide the potential changes caused in SetCssPropertiesFromHtmlAttr. These are not taken into account
					in LoadCssProperties. If this call is not related to an attribute change, use Markup::HA_NULL.
		@param attr_ns Namespace type for the attribute that changed value, ignored if attr parameter is Markup::HA_NULL.
		@param skip_top_element If TRUE, properties are not reloaded for the element parameter, but all its children.
								This parameter only applies when recurse is set to TRUE and the operation is equivalent
								with, and a convenience alternative for, calling ApplyPropertyChanges on all children of
								the element parameter.
		@return OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS		ApplyPropertyChanges(HTML_Element* element,
										 int update_pseudo,
										 BOOL recurse,
										 short attr = Markup::HA_NULL,
										 NS_Type attr_ns = NS_HTML,
										 BOOL skip_top_element = FALSE);

	/** Does layout operations that needs to be done when the html/xml parsers closes an element.
		Currently, just updates css properties for :last-child and friends by calling ApplyPropertyChanges.
		This has to be called for every element, even inserted elements and elements without closing tags,
		in order to get correct styling for e.g. :last-child.

		@param element The element which is closed.
		@return OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS		EndElement(HTML_Element* element) { return element->GetUpdatePseudoElm() ? ApplyPropertyChanges(element, 0, TRUE) : OpStatus::OK; }

	/** Does layout operations that needs to be done if a node was added or removed from the dom
		tree.

		Currently, updates css properties for :last-child and friends by calling
		ApplyPropertyChanges.

		It also handles the case when the an element is removed and that element has set the
		background of the viewport and the viewport has to be repainted.

		@param parent The parent element of the inserted/removed child.
		@param child The inserted/removed child.
		@param added TRUE if the child was inserted, FALSE if removed.
		@return OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS		HandleDocumentTreeChange(HTML_Element* parent, HTML_Element* child, BOOL added);

	/** Reload CSS properties for all elements that need it,
		and mark them for update or reflow as needed. */

	OP_STATUS		ReloadCssProperties();

	/** Schedule an element for repaint or reflow, depending on property changes.

		@param elm The element to mark for repaint/reflow etc.
		@param changes A set of PropertyChange (cssprops.h) values. */

	void			HandleChangeBits(HTML_Element* elm, int changes);

	/** Flag to determine if we should keep to original layout on this page - i.e. not apply any layout tricks like flex-root,
		text wrapping or adaptive zoom. Until the need for it has shown itself, this is a one-way setting. One could conceive a
		flag based model if there is need for it. */

	BOOL			KeepOriginalLayout();

	void			SetKeepOriginalLayout();

	/** Retrieve a list of paragraph rectangles within a given rectangle.
	 *
	 * This can be used to implement e.g. snap-to-paragraph scrolling.
	 *
	 * @param rect The rectangle to search for paragraphs in
	 * @param list List to save rectangles, in the form of OpRectListItem. You
	 * may pass AutoDeleteHead for your own convenience.
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM, or OpStatus::OK
	 */

	OP_STATUS		GetParagraphList(const OpRect& rect, Head& list);

	/** Searches for the elements within the specified area of the document.
	 *	Outputs the collection of them along with the complete regions that they occupy.
	 *	For more details see ElementSearchObject and ElementCollectingObject classes.
	 * @param customizer an instance of the ElementSearchCustomizer class for this search
	 * @param search_area the rectangle in the document root coordinates, that we want to search within
	 * @param[out] elements the collection of found elements
	 * @param extended_area the optional extended_area rectangle. The extended_area (if used) must fully
	 *						contain the search_area. If the extended area is used, then:
	 *						- every found element's region must contain at least one parallelogram, that
	 *						intersects the search_area
	 *						- every found element's set of parallelograms (that is a resulting region)
	 *						will be a subset of the set of the parallelograms, which is the element's complete region in the document,
	 *						such that every parallelogram of the resulting region intersects the extended_area
	 *						If extended_area is NULL, then the behavior is the same as search_area == extended_area.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. In case of OOM, the caller is responsible for incomplete
	 *		   elements collection cleanup. */

	OP_STATUS		SearchForElements(ElementSearchCustomizer& customizer,
									  const OpRect& search_area,
									  DocumentElementCollection& elements,
									  const OpRect* extended_area = NULL);

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION

	/** This is a customized version of SearchForElements that detects interactive items.
	 *	It uses InteractiveItemSearchCustomizer.
	 * @see SearchForElements
	*/

	OP_STATUS		FindNearbyInteractiveItems(const OpRect& search_area, DocumentElementCollection& elements, const OpRect* extended_area = NULL);

#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

#ifdef RESERVED_REGIONS
	/** Called when a box that constitutes a reserved region is added. */
	void			OnReservedRegionBoxAdded() { reserved_region_boxes ++; }

	/** Called when a box that constitutes a reserved region is removed. */
	void			OnReservedRegionBoxRemoved() { reserved_region_boxes --; OP_ASSERT(reserved_region_boxes >= 0); }

	/** Return TRUE if there are boxes that constitute a reserved region.
	 *
	 * Reserved regions are constituted by elements with certain DOM event
	 * handlers, or by certain box types, such as paged containers. This method
	 * checks for the latter.
	 */
	BOOL			HasReservedRegionBoxes() { return reserved_region_boxes > 0; }

	/** Retrieve a list of reserved rectangles within a given rectangle.
	 *
	 * A reserved rectangle is a rectangle in which any input events should be forwarded
	 * to Core before being handled by the platform UI, primarily because web applications
	 * may want a say in the user experience.
	 *
	 * @param[in] rect The rectangle to search for reserved rectangles in,
	 * in document coordinates.
	 * @param[out] region The region in which to include found rectangles.
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM, or OpStatus::OK.
	 */
	OP_STATUS		GetReservedRegion(const OpRect& rect, OpRegion& region);
#endif // RESERVED_REGIONS

	LayoutWorkplace *
					GetParentLayoutWorkplace();

#ifdef _PLUGIN_SUPPORT_
	/** Disable all plugins in the document. Returns TRUE if any plugins were disabled, otherwise FALSE. */
	BOOL			DisablePlugins();

	/** Returns TRUE if plugins are forced disabled. */
	BOOL			PluginsDisabled() { return plugins_disabled; }

	/** Reset plugin object. */
	void			ResetPlugin(HTML_Element *elm);
#endif // _PLUGIN_SUPPORT_

#ifdef ON_DEMAND_PLUGIN
	/** Activates a plugin placeholder element. */
	void			ActivatePluginPlaceholder(HTML_Element* placeholder);
#endif // ON_DEMAND_PLUGIN

	/** Hide IFRAME.  Called when the SRC attribute is changed.

	    Note: This doesn't necessarily make sense.  The reason this function
	    exists is to recreate behavior previously caused by ResetPlugin() being
	    called whenever a SRC attribute was changed on any element.  It was
	    never intended to do what it then did, but it turns out that hiding
	    IFRAMEs like that improves performance on certain test cases, probably
	    because prevents reflows/repaints during loading to some degree. */
	void			HideIFrame(HTML_Element *elm);

	/** Triggers a repaint of all text/content between start and end and
	    optionally returns the bounding rectangle for that area.
	    @param start The start of the selection range that needs repainting.
	    @param end The end of the selection range that needs repainting.
	    @param bounding_rect If not NULL, this will be set to the bounding rect of the selection. */
	void			UpdateSelectionRange(const SelectionBoundaryPoint* start, const SelectionBoundaryPoint* end, RECT* bounding_rect = NULL);

	/** Gets selection points encompassing the visual part of an element. Used
	    when it's more important to select the visual parts of an element than
	    the whole element. This operation will do a Reflow() if the tree is dirty.
	    @param[in] element The element to select.
	    @param[in] req_inside_doc TRUE if the new start and end selection elements must be visible in the
	                              document. If FALSE, the start and end selection elements
	                              may be completely outside the document area.
	    @param[out] visual_start_point The start point of the visual area or an empty SelectionBoundaryPoint.
	    @param[out] visual_end_point The end point of the visual area or an empty SelectionBoundaryPoint.

	    @returns OpStatus::OK on success or OpStatus::ERR_NO_MEMORY if the required
	             reflow could not complete due to a lock of memory. */
	OP_STATUS		GetVisualSelectionRange(HTML_Element* element, BOOL req_inside_doc, SelectionBoundaryPoint& visual_start_point, SelectionBoundaryPoint& visual_end_point);

	/** Checks whether the html element should be highlighted and then tells
		the VisualDevice of the document owning this to draw (or to change)
		the highlight around the given html element.

		There are two types of highlight:
		1) Simple border. Used for usemap areas always and for all elements
		if we don't use skin highlight (TWEAK_SKIN_HIGHLIGHTED_ELEMENT).
		2) "Skin" highlight. The same style as outline:-o-highlight-border.
		There is also a rule that this highlight has the same style as the specified
		outline for the element. However, if the outline is none, but it's an
		initial value, the highlight style used will be -o-highlight-border.
		See CORE-43426.

		This works together with HTML_Document::DrawHighlight.
		@see HTML_Document::DrawHighlight

		@param elm The html element to highlight
		@param highlight_rects The rectangles that cover the highlight area. Should
		be valid if num_highlight_rects is greater than 0.
		@param num_highlight_rects The number of the rectangles that cover the
		highlight area. Used mostly for the inline elements, which can span more
		than one line. Zero is allowed - in such case a single rect will be
		retrieved from the elm.
		@param clip_rect The rect that should clip the highlight rect.
		@param usemap_area The usemap area to highlight. NULL if we don't want
			   to apply the usemap area highlight rule.
		@return OpStatus::OK
				OpStatus::ERR if the rect of the elm couldn't be retrieved.
				OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS		DrawHighlight(HTML_Element* elm, OpRect* highlight_rects, int num_highlight_rects, const OpRect& clip_rect, HTML_Element* usemap_area = NULL);

	/** Returns TRUE if the element is an element of which props should apply to the viewport. */
	BOOL			HasDocRootProps(HTML_Element *element, Markup::Type element_type = Markup::HTE_UNKNOWN, HTML_Element *parent = NULL);

	DocRootProperties&
					GetDocRootProperties() { return *active_doc_root_props; }
	const DocRootProperties&
					GetDocRootProperties() const { return *active_doc_root_props; }
	void			SwitchPrintingDocRootProperties();

	/** Returns TRUE if the element is a "magic" body element. */
	static BOOL		IsMagicBodyElement(HTML_Element *element, HTML_Element *parent,
									   HTML_Element *first_body_element,
									   Markup::Type element_type = Markup::HTE_UNKNOWN);

	/** Find the parent CoreView in the element tree, or NULL if there is none. */

	static class CoreView*
					FindParentCoreView(HTML_Element* html_element);

	/** Find the nearest input context by walking the ancestry.

		This may be an input context established by 'html_element' itself. */

	static class OpInputContext*
					FindInputContext(HTML_Element* html_element);

	/** Give the root box an opportunity to handle an input action.

		This is meant to be called from the action system, though not the
		normal way since LayoutWorkplce isn't an OpInputContext. The real input
		context will delegate work to this method.

		@param[in] action The action.

		@return The "handled" state of the action. OpBoolean::IS_TRUE if the
		action was consumed. OpBoolean::IS_FALSE if it was unknown and should
		bubble.

		@see OpInputContext::OnInputAction()
		@see VisualDevice::OnInputAction()
		@see FramesDocument::OnInputAction() */

	OP_BOOLEAN		HandleInputAction(OpInputAction* action);

#ifdef PAGED_MEDIA_SUPPORT

	/** Get the current page number, if in some paged mode.

		@return The page number (the first page is page 0), or -1 if not
		applicable. */

	int				GetCurrentPageNumber();

	/** Get the total number of pages.

		@return The total number of pages. If the document isn't paged (but
		rather continuous), -1 is returned. */

	int				GetTotalPageCount();

	/** Set the current page number, if in some paged mode.

		@param page_number The page number. The first page is page 0.
		@param reason If the page change change requires us to ask for a new
		visual viewport, what should we specify as a reason to the viewport
		request listener?
		@return TRUE if the page number was attempted set. Will return FALSE
		otherwise, i.e. if we're not in some paged mode. */

	BOOL			SetCurrentPageNumber(int page_number, OpViewportChangeReason reason);

	/** If this document is paged, return the RootPagedContainer established by the root element. */

	class RootPagedContainer*
					GetRootPagedContainer() const;

#endif // PAGED_MEDIA_SUPPORT

	/** Flag to make sure we are not doing illegal things (like MarkDirty) when traversing */
	void			SetIsTraversing(BOOL set) { is_traversing = set; }
	BOOL			IsTraversing() { return is_traversing; }

	/** @return TRUE if we're in a reflow iteration, otherwise FALSE. */
	BOOL			IsReflowing()  { return is_in_reflow_iteration; }

#ifdef _DEBUG
	/** Allow or disallow dirty child props when generating the cascade. */
	void			SetAllowDirtyChildPropsDebug(BOOL set) { allow_dirty_child_props = set; }

	/** Are dirty child props when generating the cascade allowed? */
	BOOL			AllowDirtyChildPropsDebug() { return allow_dirty_child_props; }
#endif // _DEBUG

#ifdef CSS_TRANSITIONS
	/** Add computed style values from ongoing transitions to the computed styles
		for the given element.

		@param element The element we are getting computed styles for.
		@param props The computed styles for the element which should already
					 contain the computed styles as if there were no
					 transitions. */
	void GetTransitionValues(HTML_Element* element, HTMLayoutProperties& props) const { transition_manager.GetTransitionValues(element, props); }

	/** To be called when an ElementTransitions object gets OnRemove to remove
		the ElementTransitions object from the TransitionManager. Goes through
		LayoutWorkplace because OnRemove() pass the FramesDocument to the
		ElementTransitions object.

		@param element The element that is being removed from the document. */
	void RemoveTransitionsFor(HTML_Element* element) { transition_manager.ElementRemoved(element); }

#endif // CSS_TRANSITIONS

	/** Recalculate document CSS properties that are applied to the initial
		containing block.

		@param elm Element that contributes to the CSS properties applied to
				   the ICB. Typically HTML, BODY, or in general the root
				   element for the given document type.
		@return ERR_NO_MEMORY on OOM, otherwise OK. */

	OP_STATUS		RecalculateDocRootProps(HTML_Element* elm);

#ifdef DOM_FULLSCREEN_MODE
	/** Paint non-obscured fullscreen element. Currently only paints replaced
		content.

		@param element The element to paint.
		@param rect The area The part to repaint. This is in document
					coordinates relative the upper left corner of the view.
		@param visual_device The VisualDevice to paint on. This might be
							 another VisualDevice than the document's normal
							 VisualDevice.
		@return OK if painted, ERR if fullscreen paint is not supported on the
				passed element, ERR_NO_MEMORY on OOM. */

	OP_STATUS		PaintFullscreen(HTML_Element* element, const RECT& rect, VisualDevice* visual_device);
#endif // DOM_FULLSCREEN_MODE

	/** Check if an element is being rendered[1] as specified in the HTML spec.

		[1] http://www.whatwg.org/specs/web-apps/current-work/multipage/rendering.html#being-rendered

		@param element The element to check if is being rendered.
		@return TRUE if the element is being rendered, otherwise FALSE. */

	BOOL IsRendered(HTML_Element* element) const;

	/** Count the number of list items this element of type Markup::HTE_OL has.

		This function iterates all children recursively and also creates cascades
		for all list items which makes it heavy.

		It is used before reflowing a container that is a reversed list.

		@param element The element which list items should be counted.
		@param count (out) The number of list items.
		@return OpStatus::OK on success or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS		CountOrderedListItems(HTML_Element* element, unsigned& count);

private:
	LayoutCoord		FindNormalRightAbsEdge();

	/** Calculate nominal layout viewport size, in screen coordinates.

		This is based on the window / frame size.
		The actual layout viewport size may differ, based on layout settings and document type. */

	void			CalculateNominalLayoutViewSize(LayoutCoord& width, LayoutCoord& height) const;

	/** @short Calculate layout viewport size in either document or screen coordinates.

		Special attention is required to avoid rounding errors. This method
		aims to do that, by avoiding coordinate system conversions when
		possible.

		@param calculate_screen_coords If TRUE, all output parameters will be
		set to values in screen coordinates. If FALSE, they will be set in
		document coordinates.

		@param[out] width Set to layout viewport width

		@param[out] min_width Set to layout viewport minimum width. Used by FlexRoot.

		@param[out] max_width Set to layout viewport maximum width. Used by FlexRoot.

		@param[out] height Set to layout viewport height

		@param[out] mq_width The width that should be used for matching width media queries.
					This width is always in document coordinates.

		@param[out] mq_height The height that should be used for matching height media queries.
					This height is always in document coordinates. */

	void			CalculateLayoutViewSize(BOOL calculate_screen_coords, LayoutCoord& width, LayoutCoord& min_width, LayoutCoord& max_width, LayoutCoord& height, LayoutCoord& mq_width, LayoutCoord& mq_height) const;

	/** Check if scrollbars should be enabled.

		This method does not modify anything; it just checks whether or not vertical
		and horizontal scrollbars should be visible. */

	void			CalculateScrollbars(BOOL& want_vertical, BOOL& want_horizontal) const;

	void			CalculateNormalEraWidth();

	/** Perform one reflow iteration. */

	OP_BOOLEAN		ReflowOnce(LayoutInfo& info);

	/** Get change bits for the given attribute with namespace. To be used in ApplyPropertyChanges.
		This method takes into account attributes with values which are translated into css values. No
		before/after values are provided, so the change bits returned is the worst-case for that attribute.

		@param element The HTML_Element for which the attribute has changed.
		@param attr The attribute id in the given namespace.
		@param attr_ns The attribute's namespace type. */

	int				GetAttributeChangeBits(HTML_Element* element, short attr, NS_Type attr_ns);

	/** Enable temporary storage of ReplacedContent as StoredReplacedContent objects.
	 *
	 * This will be enabled during reflow, and is used when a layout box with
	 * certain types of ReplacedContent is deleted. Certain ReplacedContent
	 * types must not be deleted and recreated during reflow, as that would
	 * reset their state. Instead they are added to a list of
	 * StoredReplacedContent objects. When a box is created for an element with
	 * replaced content, GetStoredReplacedContent() is called to check if a
	 * ReplacedContent object is already present for that element, and only if
	 * there isn't, a new ReplacedContent object will be created.
	 */
	void			StartStoreReplacedContent() { OP_ASSERT(!enable_store_replaced_content && stored_replaced_content.Empty());	enable_store_replaced_content = TRUE; }

  	/** Disable temporary storage of ReplacedContent as StoredReplacedContent objects.
	 *
	 * All entries still in the list of StoredReplacedContent objects
	 * (i.e. those not fetched with GetStoredReplacedContent()) will be
	 * deleted; they were no longer needed by their element.
	 */
	void			EndStoreReplacedContent();

	/** Flag to make sure we are not doing illegal things when reflowing. MarkDirty is allowed during a ReflowOnce iteration only when yielding */
	void			SetIsInReflowIteration(BOOL set) { is_in_reflow_iteration = set; }

	BOOL			IsInReflowIteration() { return is_in_reflow_iteration; }

	/** Should the outline property of the element disable the highlight?

		@param elm The element to check.
		@param[out] highlight_outline_extent May be assigned a value different than zero
					(based on the outline width and outline offset props)
					if the element has a visible outline by itself and that is a UA highlight
					already (depends on certain settings, like TWEAK_SKIN_HIGHLIGHTED_ELEMENT).
					Otherwise zero is assigned.

					If the value is based on outline width and outline offset, it is
					exactly the outline width if the offset is not lower than zero and the width
					descreased by absolute value of the offset otherwise.
		@return OpBoolean::IS_TRUE if the highlight should be disabled
				OpBoolean::IS_FALSE otherwise
				OpStatus::ERR_NO_MEMORY in case of OOM. */

	OP_BOOLEAN		ShouldElementOutlineDisableHighlight(HTML_Element* elm, short& highlight_outline_extent) const;

private:

#ifdef LAYOUT_YIELD_REFLOW
	/** Check if ReloadCssProperties should yield, and mark remaining
		elements props-dirty if necessary.

		@param yield_element The element about to get its CSS properties
							 reloaded.
		@return TRUE if ReloadCssProperties should yield, otherwise FALSE. */
	BOOL			YieldReloadCssProperties(HTML_Element* yield_element)
	{
		if (CanYield())
		{
			double delay_time = g_op_time_info->GetWallClockMS() - GetReflowStart();

			if (delay_time > GetMaxReflowTime())
			{
				if (reload_all)
				{
					while (elm)
					{
						elm->MarkPropsDirty(doc, 0, TRUE);
						elm = elm->NextSibling();
					}
				}
				else
					elm->MarkPropsDirty(doc);

				return TRUE;
			}
		}
		return FALSE;
	}
#endif // LAYOUT_YIELD_REFLOW


	/** The FramesDocument that holds the logical document that holds me */
	FramesDocument* doc;

	/** List of reflow elements */
	Head			reflow_elements;

	/** Times this document has been reflowed. */

	int				reflow_count;

#if LAYOUT_MAX_REFLOWS > 0
	/** Reflow count to avoid infinite reflows. */
    int				internal_reflow_count;
#endif

	/** Amount of time spent reflowing. */

	double			reflow_time;

	/** when did this reflow start? */

	double			reflow_start;

#ifdef RESERVED_REGIONS
	/** Number of boxes in this document that constitute a reserved region.

		This is based on box/content type (e.g. paged container), not whether
		elements have certain event handlers or not. */
	int				reserved_region_boxes;
#endif // RESERVED_REGIONS

#ifdef PAGED_MEDIA_SUPPORT

	/** The element where a pending page break was inserted. */

	HTML_Element*	page_break_element;

	/** Set once page breaking has started.

		Sometimes, several reflow passes are required just to do regular
		non-paged layout. This must be done before we can start on pagination,
		or we may end up with circular dependencies and infinite loops. */

	BOOL			page_breaking_started;

	/** Height of the root paged container control's height, if any. */

	LayoutCoord		page_control_height;

#endif // PAGED_MEDIA_SUPPORT

	/** */
	Counters		counters;

#ifdef SUPPORT_TEXT_DIRECTION

	/** */
	DocBidiStatus   doc_bidi_status;

	BOOL			is_rtl_document;
#endif // SUPPORT_TEXT_DIRECTION

	BOOL			enable_store_replaced_content;

	/** List of StoredReplacedContent objects. */
	Head			stored_replaced_content;

	LayoutCoord		load_translation_x;
	LayoutCoord		load_translation_y;

	LayoutCoord		layout_view_x;
	LayoutCoord		layout_view_width;
	LayoutCoord		layout_view_min_width;
	LayoutCoord		layout_view_max_width;
	LayoutCoord		nominal_width;
	LayoutCoord		layout_view_y;
	LayoutCoord		layout_view_height;
	LayoutCoord		nominal_height;

	/** Width of the layout viewport including the vertical scrollbar
		used for evaluation of width media queries. */
	LayoutCoord		media_query_width;

	/** Height of the layout viewport including the horizontal scrollbar
		used for evaluation of height media queries. */
	LayoutCoord		media_query_height;

	/** Width of screen used for evaluation of device-width media
		queries. */
	LayoutCoord		media_query_device_width;

	/** Height of screen used for evaluation of device-height media
		queries. */
	LayoutCoord		media_query_device_height;

	/** Normal (minimum) width of document caused by one or more absolute positioned elements. A
		zero value means that there is no absolute element placed outside right side of view. This
		value is only used if the Absolute Positioned Strategy setting is "scale". */
	LayoutCoord		normal_era_width;

	short			viewport_overflow_x;
	short			viewport_overflow_y;

	OpRect			fixed_positioned_area;
	BOOL			has_bg_fixed;

	/* yield variables */
	HTML_Element*	yield_element;
	BOOL			yield_element_extra_dirty;

	BOOL			can_yield;
	ForceLayoutChanged	yield_force_layout_changed;

#ifdef LAYOUT_YIELD_REFLOW
	/** Cascade used when reflow yields, so that we can resume quickly (without overhead) later. */

	AutoDeleteHead	yield_cascade;
#endif // LAYOUT_YIELD_REFLOW

	int				max_reflow_time;
	int				yield_count;
	/* end yield variables */

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE
	/** Needs another reflow pass before layout can be traversed.
		This is TRUE e.g. between the first and second reflow pass required for tables and shrink-to-fit containers. */
	BOOL			needs_another_reflow_pass;
#endif // LAYOUT_TRAVERSE_DIRTY_TREE

	/** Flag to determine if we should heep to original layout on this page - i.e. not apply any layout tricks like flex-root,
	text wrapping or adaptive zoom. */
	BOOL			keep_original_layout;

#ifdef _PLUGIN_SUPPORT_
	BOOL			plugins_disabled;
#endif // _PLUGIN_SUPPORT_

	/** Flag to make sure we are not doing illegal things (like MarkDirty) when traversing */
	BOOL			is_traversing;

	/** Flag to make sure we are not doing illegal things when reflowing. MarkDirty is allowed during a ReflowOnce iteration only when yielding */
	BOOL			is_in_reflow_iteration;

	/** Force recalculation of global document properties from body
		and html elements on next property reload.

		Normally this does not need to be forced; it's handled by the
		property reload machinery. But in the case the magic body
		element disappears (and carried global properties) we must
		remember to recalculate them on the next property reload. */

	BOOL			force_doc_root_props_recalc;

	/** Root size changed externally (window / frame resize).

		This means that there's a pending reflow for it. */

	BOOL			root_size_changed;

#ifdef _DEBUG


	/** Normally, when generating the cascade, elements' properties must be up to date, and we have
		an assertion to ensure this. This flag allows for exceptions to this. */

	BOOL			allow_dirty_child_props;
#endif

	/** Use the original old_width and old height values during a reflow iteration
	    when looking for size changes in the document */
	int				saved_old_height;
	int				saved_old_width;

	DocRootProperties
					doc_root_props;
	DocRootProperties
					print_doc_root_props;
	DocRootProperties*
					active_doc_root_props;

#ifdef CSS_TRANSITIONS
	/** TransitionManager for this document. */
	TransitionManager
					transition_manager;
#endif // CSS_TRANSITIONS
};

inline COLORREF BackgroundColor(const HTMLayoutProperties &props)
{
	return props.bg_color;
}

inline BOOL HasBackground(const HTMLayoutProperties &props)
{
	return props.bg_color != USE_DEFAULT_COLOR || !props.bg_images.IsEmpty();
}

inline BOOL HasBackgroundImage(const HTMLayoutProperties &props)
{
	return !props.bg_images.IsEmpty();
}

#endif // _LAYOUT_WORKPLACE_H_
