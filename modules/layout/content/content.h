/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** @file content.h
 *
 * Content class prototypes for document layout.
 */

#ifndef _CONTENT_H_
#define _CONTENT_H_

#include "modules/forms/piforms.h"
#include "modules/dochand/fdelm.h"
#include "modules/display/coreview/coreview.h"


#ifdef _PLUGIN_SUPPORT_
#include "modules/ns4plugins/opns4plugin.h"
#endif // _PLUGIN_SUPPORT_

class VerticalLayout;
class Media;
class Canvas;
#ifdef DOM_JIL_API_SUPPORT
class JILCameraPreviewElement;
#endif
class IFrameContent;
class FlexContent;
class MultiColumnContainer;
class ScrollableContainer;
#ifdef PAGED_MEDIA_SUPPORT
class PagedContainer;
# include "modules/widgets/OpPageControl.h"
#endif // PAGED_MEDIA_SUPPORT

#include "modules/layout/bidi/characterdata.h"
#include "modules/layout/content/multicol.h"
#include "modules/layout/content/scrollable.h"
#include "modules/doc/frm_doc.h"
#include "modules/layout/box/box.h"
#include "modules/layout/layout_coord.h"
#include "modules/style/css_all_values.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpScrollbar.h"
#include "modules/widgets/OpProgress.h"
#include "modules/pi/OpPluginWindow.h"
#include "modules/logdoc/complex_attr.h"

class InlineRunInBox;
class FloatingBox;
class PaintObject;

enum TRUE_TABLE_CANDIDATE
{
	NEUTRAL,
	POSITIVE,
	NEGATIVE,
	DISABLE
};

#ifdef PAGED_MEDIA_SUPPORT

enum SEARCH_CONSTRAINT
{
	SOFT = 0,
	HARD = 1,
	ONLY_PREDECESSORS = 2
};

#endif // PAGED_MEDIA_SUPPORT

/** Content. */

class Content
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

private:

	/** Delete reflow state. */

	virtual void	DeleteReflowState() {}

protected:

	friend class Content_Box;

	/** Placeholder. */

	Content_Box*	placeholder;

	/** Unless its computed value is auto, calculate the actual value of the
		CSS height property: start with the CSS computed height, then apply
		constraints and resolve percentage values.

		NOTE: Asserts that the placeholder has successfully initialised its reflow
		state.

		@return The actual value of the CSS height property, or
		CONTENT_HEIGHT_AUTO if its computed value is auto. */

	LayoutCoord		CalculateCSSHeight(LayoutInfo& info, LayoutProperties* cascade);

	/** Has reflow been forced? */

	virtual BOOL	GetReflowForced() const { return FALSE; }

	/** Set reflow force status, which will be checked and reset during SignalChange(). */

	virtual void	SetReflowForced(BOOL force_reflow_on) { OP_ASSERT(!"May miss reflow"); }

public:

	/** Constructor.

		Set placeholder to NULL initially. During layout box creation, Content
		is allocated before Box. If we somehow don't allocate a Box (due to OOM
		or table structure correction) and just delete the Content, its
		destructor may want to access its placeholder, but if it checks for
		NULL first (and we actually set it to NULL initially), at least we
		won't crash. ScrollableContainer is one such class which is known to
		access its placeholder. */

					Content()
						: placeholder(NULL) {}

	virtual			~Content() {}

	/** Set placeholder of box. */

	void			SetPlaceholder(Content_Box* content_box) { placeholder = content_box; }

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info) { return LAYOUT_CONTINUE; }

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info) {}

	/** Grow width of inline box. */

	virtual void	GrowInlineWidth(LayoutCoord increment) {}

	/** Increase actual width and min/max widths. */

	virtual void	IncreaseWidths(LayoutCoord increment) { OP_ASSERT(!"Not supported on this type of content"); }

	/** Get width of container. */

	virtual LayoutCoord
					GetWidth() const { return LayoutCoord(0); }

	/** Get height of border box. */

	virtual LayoutCoord
					GetHeight() const = 0;

	/** Get minimum height (height if maximum width is satisfied) */

	virtual LayoutCoord
					GetMinHeight() const = 0;

	/** Grow height of content. */

	virtual void	GrowHeight(LayoutCoord extra) {}

	/** Count number of words in segment. */

	static int		CountWords(LineSegment& segment, HTML_Element* containing_element);

	/** Count number of words in segment. */

	int			CountWords(LineSegment& segment) const;

	/** Return TRUE if the min/max widths of this content are up-to-date. */

	virtual BOOL	HasValidMinMaxWidths() const { return TRUE; }

	/** Compute size of content and return IS_TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info) { return OpBoolean::IS_TRUE; }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN) = 0;

	/** Paint box with children on screen. */

	virtual OP_STATUS
					Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area) { return OpStatus::OK; }

	/** Get HTML element this box belongs to. */

	HTML_Element*	GetHtmlElement() const { return placeholder->GetHtmlElement(); }

	/** Cast this object to container, if it is a container. */

	virtual Container*
					GetContainer() { return NULL; }

	/** Cast this object to table content, if it is one. */

	virtual TableContent*
					GetTableContent() { return NULL; }

	/** Cast this object to selectcontent, if it is one. */

	virtual SelectContent*
					GetSelectContent() { return NULL; }

	/** Cast this object to ButtonContainer, if it is one. */

	virtual ButtonContainer*
					GetButtonContainer() { return NULL; }

	/** Cast this object to ScrollableContainer, if it is one. */

	virtual ScrollableContainer*
					GetScrollableContainer() { return NULL; }

	/** Cast this object to ScrollableArea, if it is one. */

	virtual ScrollableArea*
					GetScrollable() { return NULL; }

	/** Cast this object to MultiColumnContainer, if it is one. */

	virtual MultiColumnContainer*
					GetMultiColumnContainer() { return NULL; }

#ifdef PAGED_MEDIA_SUPPORT

	/** Cast this object to PagedContainer, if it is one. */

	virtual PagedContainer*
					GetPagedContainer() { return NULL; }

#endif // PAGED_MEDIA_SUPPORT

	/** Cast this object to FlexContent, if it is one. */

	virtual FlexContent*
					GetFlexContent() { return NULL; }

	/** Get svg content. */

#ifdef SVG_SUPPORT
	virtual SVGContent*
					GetSVGContent() { return NULL; }
#endif // } SVG_SUPPORT

	/** Get bullet content. */

	virtual BulletContent*
					GetBulletContent() { return NULL; }

	/** Get baseline of first or last line.

		@param last_line TRUE if last line baseline search (inline-block case).	*/

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const = 0;

	/** Get baseline of inline block or content of inline element that
		has a baseline (e.g. ReplacedContent with baseline, BulletContent). */

	virtual LayoutCoord
					GetBaseline(const HTMLayoutProperties& props) const { return GetBaseline(); }

	/** Get the baseline if maximum width is satisfied. */

	virtual LayoutCoord
					GetMinBaseline(const HTMLayoutProperties& props) const = 0;

	/** Is this content inline content? */

	virtual BOOL	IsInlineContent() const { return FALSE; }

	/** Is considered a containing element, e.g. container, table or flexbox? */

	virtual BOOL	IsContainingElement() const { return FALSE; }

	/** Is this content replaced content? */

	virtual BOOL	IsReplaced() const { return FALSE; }

	/** Is this content replaced content with baseline? */

	virtual BOOL	IsReplacedWithBaseline() const { return FALSE; }

	/** Is this image content? */

	virtual BOOL	IsImage() const { return FALSE; }

	/** Is external content? */

	virtual BOOL	IsExternal() const { return FALSE; }

	/** Is this embed content? */

	virtual BOOL	IsEmbed() const { return FALSE; }

	/** run-in or compact? */

	virtual BOOL	IsRunIn() const { return FALSE; }

	/** Is this iframe content? */

	virtual BOOL	IsIFrame() const { return FALSE; }

	/** Is there typically a CoreView associated with this content ?

		@see class CoreView */

	virtual BOOL	HasCoreView() const { return FALSE; }

	/** Is content weak? */

	virtual BOOL	IsWeakContent() const { return FALSE; }

	/** Is this a container that divides content into panes?

		Multicol and paged containers are such containers. */

	virtual BOOL	IsMultiPaneContainer() const { return FALSE; }

	/** Create form, plugin and iframe objects */

	virtual OP_STATUS
					Enable(FramesDocument* doc) { return OpStatus::OK; }

	/** Mark content as dirty. Content will be removed before reflow */

	virtual void	MarkDisabled() {}

    /** Remove form, plugin and iframe objects */

	virtual void	Disable(FramesDocument* doc) {}

	/** Hide external content, such as plug-ins, iframes, etc. */

	virtual void	Hide(FramesDocument* doc) { Disable(doc); }

	/** Stores the formobject in the HTML_Element (for backup during reflow)  */

	virtual void	StoreFormObject() {}

	/** Add option element to 'select' content */

	virtual OP_STATUS
					AddOption(HTML_Element* option_element, unsigned int &index) { index = UINT_MAX; return OpStatus::ERR; }

	/** Remove option element from 'select' content */

	virtual void	RemoveOption(HTML_Element* option_element) { }

	/** Remove optiongroup element from 'select' content */

	virtual void	RemoveOptionGroup(HTML_Element* optiongroup_element) { }

	/** Begin optiongroup in 'select' content */

	virtual void	BeginOptionGroup(HTML_Element* optiongroup_element) {}

	/** End optiongroup in 'select' content */

	virtual void	EndOptionGroup(HTML_Element* optiongroup_element) {}

	/** Layout option element in 'select' content */

	virtual OP_STATUS
					LayoutOption(LayoutProperties* cascade, LayoutWorkplace* workplace, unsigned int index) { return OpStatus::OK; }

	/** Lay out form content. */

	virtual OP_STATUS
					LayoutFormContent(LayoutProperties* cascade, LayoutWorkplace* workplace) { return OpStatus::OK; }

	/** Get form object. */

	virtual FormObject*
					GetFormObject() const { return NULL; }

	/** Calculate bottom margin of layout element, by collapsing with adjacent child margins.
	 * This is only done when skipping layout of a box.
	 * @param cascade properties of this element
	 * @param info Layout information structure
	 * @param bottom_margin Margin state
	 * @param include_this If FALSE, omit margins specified on this element from the calculation
	 * and just collapse adjacent child margins.
	 * @return OpBoolean::IS_FALSE if top and bottom margins of this element are adjacent,
	 * OpBoolean::IS_TRUE otherwise, unless an error occurred.
	 */

	virtual OP_BOOLEAN
					CalculateBottomMargins(LayoutProperties* cascade, LayoutInfo& info, VerticalMargin* bottom_margin, BOOL include_this = TRUE) const;

	/** Calculate top margin of layout element, by collapsing with adjacent child margins.
	 * This is only done when skipping layout of a box.
	 * @param cascade properties of this element
	 * @param info Layout information structure
	 * @param top_margin Margin state
	 * @param include_self_bottom If FALSE, don't collapse top with bottom margin
	 * specified on this element, even if they are adjoining.
	 * @return OpBoolean::IS_FALSE if top and bottom margins of this element are adjacent,
	 * OpBoolean::IS_TRUE otherwise, unless an error occurred.
	 */

	virtual OP_BOOLEAN
					CalculateTopMargins(LayoutProperties* cascade, LayoutInfo& info, VerticalMargin* top_margin, BOOL include_self_bottom) const;

	/** Get placeholder. */

	Content_Box*	GetPlaceholder() { return placeholder; }

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc) {}

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props) {}

	/** Traverse box with children. */

	virtual BOOL	LineTraverse(TraversalObject* traversal_object, LayoutProperties* layout_props, LineSegment& segment, LayoutCoord baseline, LayoutCoord x);

	/** Get min/max width.

		Returns FALSE if content has no min/max width. */

	virtual BOOL	GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const = 0;

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth() = 0;

#ifdef PAGED_MEDIA_SUPPORT

	/** Attempt to break page.

		Will attempt to break page in the subtree established by this element. */

	virtual BREAKST	AttemptPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain) { return BREAK_KEEP_LOOKING; }

	/** Insert a page break.

		`strength' means how badly we want to break the page.

	      0 = honour all CSS page breaking constraints,
	      1 = do not honour "page-break-inside: avoid" rules,
	      2 = just break page at first opportunity,
	      3 = force page break. */

	BREAKST			InsertPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain);

#endif // PAGED_MEDIA_SUPPORT

	/** Combine the specified break policies with child break policies that should be propagated to their parent. */

	virtual void	CombineChildBreakProperties(BREAK_POLICY& page_break_before,
												BREAK_POLICY& column_break_before,
												BREAK_POLICY& page_break_after,
												BREAK_POLICY& column_break_after) {}

	/** Get the logical top and bottom of a loose subtree of inline boxes limited by end_position.

		A subtree is defined as 'loose' if the root has vertical-aligment 'top' or 'bottom'
		and consist of all descendants that are not loose themselves. */

	BOOL			GetLooseSubtreeTopAndBottom(TraversalObject* traversal_object, LayoutProperties* cascade, LayoutCoord current_baseline, LayoutCoord end_position, HTML_Element* start_element, LayoutCoord& logical_top, LayoutCoord& logical_bottom);

	/** Is there any in-flow content? */

	virtual BOOL	IsEmpty() const { return FALSE; }

	/** Is height relative? */

	virtual BOOL	HeightIsRelative() const { return FALSE; }

	/** Is this a True Table candidate? */

	virtual TRUE_TABLE_CANDIDATE
					IsTrueTableCandidate() const { return NEGATIVE; }

	/** Is this content shrink-to-fit?

		Return TRUE if it is shrink-to-fit, i.e. that it calculates its width
		based on min/max width propagation fromt its child.
		ShrinkToFitContainer is one such type. */

	virtual BOOL	IsShrinkToFit() const { return FALSE; }

	/** Get scroll offset, if applicable for this type of box / content. */

	virtual OpPoint	GetScrollOffset() { return OpPoint(); }

	/** Set scroll offset, if applicable for this type of box / content.

		@param x If non-NULL, the new X position. If NULL, leave current X position as is.
		@param y If non-NULL, the new Y position. If NULL, leave current Y position as is. */

	virtual void	SetScrollOffset(int* x, int* y) {}

	/** Get the percent of normal width when page is shrunk in ERA. */

	virtual int		GetPercentOfNormalWidth() const { return 100; }

	/** Scale border and padding (in ERA)? */

	virtual BOOL	ScaleBorderAndPadding() const { return FALSE; }

	/** Force reflow of this element later. */

	void			ForceReflow(LayoutInfo& info);

	/** Remove cached info */
	virtual	BOOL	RemoveCachedInfo() { return FALSE; }

	/** Yield layout of this currently reflowing content. */
	virtual void	YieldLayout(LayoutInfo& info) {}

	/** Return TRUE if this element can be split into multiple outer columns or pages.

		In general, descendant content of a paged or multicol container is
		columnizable, but this is not the case if the content is or is inside
		of a container with scrollbars, button, replaced content, etc. */

	virtual BOOL	IsColumnizable() const { return FALSE; }

	/** Distribute content into columns.

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	Columnize(Columnizer& columnizer, Container* container) { return TRUE; }

	/** Figure out to which column(s) or spanned element a box belongs. */

	virtual void	FindColumn(ColumnFinder& cf);

	/** Return the OpInputContext of this content, if it establishes one. */

	virtual OpInputContext*
					GetInputContext() { return NULL; }

	/** Should this content layout its children? */

	virtual BOOL	ShouldLayoutChildren() { return TRUE; }

	/** Find the parent CoreView in the element tree, or NULL if there is none. */

	static CoreView*
					FindParentCoreView(HTML_Element* html_element);

	/** Find the nearest input context by walking the ancestry.

		This may be an input context established by 'html_element' itself. */

	static OpInputContext*
					FindInputContext(HTML_Element* html_element);

	/** Find the nearest ancestor that is a scrollable or paged container.

		@return Such content, or NULL if there was no such ancestor. */

	static Content*	FindParentOverflowContent(HTML_Element* html_element);

	/** Return the first fixed positioned ancestor or NULL if no such exists.

		Includes the element of the initial cascade in the search iteration. */

	static HTML_Element*
					GetFixedPositionedAncestor(LayoutProperties* cascade);

	/** Handle event that occurred on this element. */

	virtual void	HandleEvent(FramesDocument* doc, DOM_EventType event, int offset_x, int offset_y) {}

	/** Return TRUE if the actual value of 'overflow' is 'visible'. */

	virtual BOOL	IsOverflowVisible() { return TRUE; }

#ifdef DOM_FULLSCREEN_MODE
	/** Paint content on screen as a fullscreen element. */

	virtual OP_STATUS
					PaintFullscreen(FramesDocument* doc, VisualDevice* visual_device, HTML_Element* elm, const RECT& area) { return OpStatus::ERR; }
#endif // DOM_FULLSCREEN_MODE
};


#ifdef SUPPORT_TEXT_DIRECTION
# include "modules/unicode/unicode_bidiutils.h"
#endif // SUPPORT_TEXT_DIRECTION


enum HasFirstLetter
{
	FIRST_LETTER_NO = 0,
	FIRST_LETTER_THIS = 1,
	FIRST_LETTER_PARENT = 2
};

struct LineBoxReflowState
{
	/** @name Line's bounding box
		Line's bounding box.

		bounding_box_left_non_relative is bounding_box_left,
		but not counting the relative position offsets.
		This value is needed in two cases:
		- rtl, because when closing the line in rtl container
		that bounding box part will be moved to the right,
		while the difference between bounding_box_left
		and bounding_box_left_non_relative will be still on the left.
		- the line contains the boxes that are not affected by text-align
		(compact boxes and outside list item markers). In such case
		we need to store the left edge of such box, to be sure
		that we include it in the final bounding box.
	 */

	//@{
	LayoutCoord		bounding_box_left_non_relative;
	LayoutCoord		bounding_box_left;
	LayoutCoord		bounding_box_right;
	LayoutCoord		bounding_box_above_top;
	LayoutCoord		bounding_box_below_top;
	LayoutCoord		bounding_box_above_baseline;
	LayoutCoord		bounding_box_below_baseline;
	LayoutCoord		bounding_box_above_bottom;
	LayoutCoord		bounding_box_below_bottom;
	//@}

	/** Minimum height of line box. */

	LayoutCoord		minimum_height;

	/** @name Line's content box
		Line's content box

		Because inlines may be positioned relative to top, baseline or bottom of line
		we need to keep content's height of each of such positioned inline. Until line height
		and baseline are not fixed (until closing line) we can't easily calculate overflow.
		Inlines positioned relatively to top of line will update 'content_box_below_top' only,
		inlines positioned relatively to baseline will update 'content_box_below_baseline'
		and bottom aligned inlines will update 'content_box_below_bottom'. When line height
		and baseline get fixed we can turn those heights into overflow. */

	//@{
	LayoutCoord		content_box_right;
	LayoutCoord		content_box_below_top;
	LayoutCoord		content_box_below_baseline;
	LayoutCoord		content_box_below_bottom;
	//@}

	/** @name Abspos content box
		Content box of auto aligned absolutely positioned descendants. Usually absolutely
		positioned element propagates its bounding and content box up to first positioned
		element (containing block) but if it is inside an inline and has auto vertical
		top & bottom offset, we can't guess its final position until line is closed.
		Therefore we store its content box separately relative to line position and will
		propagate it when line is closed to make sure scrollbars are triggered when necessary */

	//@{
	LayoutCoord		abspos_content_box_right;
	LayoutCoord		abspos_content_box_below_top;
	LayoutCoord		abspos_content_box_below_baseline;
	LayoutCoord		abspos_content_box_below_bottom;
	//@}

	/** @name Min/Max inline content virtual position
		Due to negative margins inline content may be positioned at virtual
		position lower than line's virtual start or greater than line's virtual
		end. Line's bbox left edge will be set to infinity if there is
		content before line start and analogically line's right edge will be
		set to infinity if line's max virtual position will be greater than line's
		virtual end. Infinite bbox edges need to be set using logical virtual
		positions instead of psychical graphics units as those can be affected
		by RTL layout. */

	//@{
	LayoutCoord		min_inline_virtual_pos;
	LayoutCoord		max_inline_virtual_pos;
	//@}

	/** Reset values */

	void			Reset()
	{
		bounding_box_left_non_relative = LAYOUT_COORD_HALF_MAX;
		bounding_box_left = LAYOUT_COORD_HALF_MAX;
		bounding_box_right = LAYOUT_COORD_HALF_MIN;
		bounding_box_above_top = LayoutCoord(0);
		bounding_box_below_top = LayoutCoord(0);
		bounding_box_above_baseline = LayoutCoord(0);
		bounding_box_below_baseline = LAYOUT_COORD_MIN;
		bounding_box_above_bottom = LayoutCoord(0);
		bounding_box_below_bottom = LAYOUT_COORD_MIN;
		minimum_height = LayoutCoord(0);
		content_box_right = LAYOUT_COORD_HALF_MIN;
		content_box_below_top = LayoutCoord(0);
		content_box_below_baseline = LAYOUT_COORD_HALF_MIN;
		content_box_below_bottom = LAYOUT_COORD_HALF_MIN;
		abspos_content_box_right = LayoutCoord(0);
		abspos_content_box_below_top = LayoutCoord(0);
		abspos_content_box_below_baseline = LAYOUT_COORD_HALF_MIN;
		abspos_content_box_below_bottom = LAYOUT_COORD_HALF_MIN;
		min_inline_virtual_pos = LAYOUT_COORD_MAX;
		max_inline_virtual_pos = LAYOUT_COORD_MIN;
	}

	/** Is this box empty? */

	BOOL			IsEmpty() const { return !(bounding_box_above_top || bounding_box_above_baseline || bounding_box_above_bottom || bounding_box_below_top || bounding_box_below_baseline != LAYOUT_COORD_MIN || bounding_box_below_bottom != LAYOUT_COORD_MIN); }
};

struct UncommittedLineBoxReflowState : public LineBoxReflowState
{
	/** Line size above baseline after last point line could be broken at. */

	LayoutCoord		height_above_baseline;

	/** Line box size below baseline after last point line could be broken at. */

	LayoutCoord		height_below_baseline;

	/** Reset uncommitted reflow state */

	void			Reset()
	{
		LineBoxReflowState::Reset();

		height_above_baseline = LAYOUT_COORD_HALF_MIN;
		height_below_baseline = LAYOUT_COORD_HALF_MIN;
	}
};

struct LineReflowState
{
private:
	/** Commit pending bounding box */

	void			CommitPendingBoundingBox(LayoutCoord left)
	{
		if (committed.minimum_height < uncommitted.minimum_height)
			committed.minimum_height = uncommitted.minimum_height;

		if (committed.bounding_box_left_non_relative > uncommitted.bounding_box_left_non_relative + left)
			committed.bounding_box_left_non_relative = uncommitted.bounding_box_left_non_relative + left;

		if (committed.bounding_box_left > uncommitted.bounding_box_left + left)
			committed.bounding_box_left = uncommitted.bounding_box_left + left;

		if (committed.bounding_box_right < uncommitted.bounding_box_right + left)
			committed.bounding_box_right = uncommitted.bounding_box_right + left;

		if (committed.bounding_box_above_baseline < uncommitted.bounding_box_above_baseline)
			committed.bounding_box_above_baseline = uncommitted.bounding_box_above_baseline;

		if (committed.bounding_box_below_baseline < uncommitted.bounding_box_below_baseline)
			committed.bounding_box_below_baseline = uncommitted.bounding_box_below_baseline;

		if (committed.bounding_box_above_top < uncommitted.bounding_box_above_top)
			committed.bounding_box_above_top = uncommitted.bounding_box_above_top;

		if (committed.bounding_box_below_top < uncommitted.bounding_box_below_top)
			committed.bounding_box_below_top = uncommitted.bounding_box_below_top;

		if (committed.bounding_box_above_bottom < uncommitted.bounding_box_above_bottom)
			committed.bounding_box_above_bottom = uncommitted.bounding_box_above_bottom;

		if (committed.bounding_box_below_bottom < uncommitted.bounding_box_below_bottom)
			committed.bounding_box_below_bottom = uncommitted.bounding_box_below_bottom;
	}

	void			CommitPendingContentBox(LayoutCoord left)
	{
		if (uncommitted.content_box_right >= 0 || left >= 0 || uncommitted.content_box_right >= LAYOUT_COORD_HALF_MIN - left)
		{
			LayoutCoord new_right = uncommitted.content_box_right + left;

			if (committed.content_box_right < new_right)
				committed.content_box_right = new_right;
		}

		if (committed.content_box_below_top < uncommitted.content_box_below_top)
			committed.content_box_below_top = uncommitted.content_box_below_top;

		if (committed.content_box_below_baseline < uncommitted.content_box_below_baseline)
			committed.content_box_below_baseline = uncommitted.content_box_below_baseline;

		if (committed.content_box_below_bottom < uncommitted.content_box_below_bottom)
			committed.content_box_below_bottom = uncommitted.content_box_below_bottom;

		if (uncommitted.abspos_content_box_right && committed.abspos_content_box_right < uncommitted.abspos_content_box_right + left)
			committed.abspos_content_box_right = uncommitted.abspos_content_box_right + left;

		if (committed.abspos_content_box_below_top < uncommitted.abspos_content_box_below_top)
			committed.abspos_content_box_below_top = uncommitted.abspos_content_box_below_top;

		if (committed.abspos_content_box_below_baseline < uncommitted.abspos_content_box_below_baseline)
			committed.abspos_content_box_below_baseline = uncommitted.abspos_content_box_below_baseline;

		if (committed.abspos_content_box_below_bottom < uncommitted.abspos_content_box_below_bottom)
			committed.abspos_content_box_below_bottom = uncommitted.abspos_content_box_below_bottom;
	}

public:
	/** Uncommitted line box reflow state */

	UncommittedLineBoxReflowState
					uncommitted;

	/** Committed line box reflow state */

	LineBoxReflowState
					committed;

					LineReflowState() { Reset(); }

	/** Reset line reflow state */

	void			Reset() { uncommitted.Reset(); committed.Reset(); }

	/** Resets committed line info - prepares for new line */

	void			ResetCommitted() { committed.Reset(); }

	/** Resets uncommitted line info - done after content has been committed */

	void			ResetUncommitted() { uncommitted.Reset(); }

	/** Commit pending boxes */

	void			CommitPendingBoxes(LayoutCoord left)
	{
		CommitPendingBoundingBox(left);
		CommitPendingContentBox(left);
	}

	void			GrowUncommittedLineHeight(LayoutCoord line_height, LayoutCoord above_baseline, LayoutCoord below_baseline)
	{
		if (uncommitted.minimum_height < line_height)
			uncommitted.minimum_height = line_height;

		if (uncommitted.height_above_baseline < above_baseline)
			uncommitted.height_above_baseline = above_baseline;

		if (uncommitted.height_below_baseline < below_baseline)
			uncommitted.height_below_baseline = below_baseline;
	}

	/** Adjusts uncommitted bounding box and content rect when new content is being added to line */

	void			AdjustPendingBoundingBox(RECT &bounding_box, RECT &content_rect, LayoutCoord relative_x, LayoutCoord relative_y, LayoutCoord allocated_position, const InlineVerticalAlignment& valign);

	/** Adjust uncommitted abspos propagated content box */

	void			AdjustPendingAbsPosContentBox(RECT &content_rect, LayoutCoord relative_x, LayoutCoord relative_y, LayoutCoord allocated_position, const InlineVerticalAlignment& valign);

	/** Adjust line virtual min/max boundaries */

	void			AdjustPendingVirtualRange(LayoutCoord pending_line_pos, LayoutCoord box_width)
	{
		if (uncommitted.min_inline_virtual_pos > pending_line_pos)
			uncommitted.min_inline_virtual_pos = pending_line_pos;

		/* An inline box with negative margin allocates its position before
		   allocating negative margins. We need to take this position
		   in account now, or later it will decrease by inline's negative margin. */

		if (box_width < 0)
			box_width = LayoutCoord(0);

		if (uncommitted.max_inline_virtual_pos < pending_line_pos + box_width)
			uncommitted.max_inline_virtual_pos = pending_line_pos + box_width;
	}

	/** Commit pending line virtual min/max boundaries */

	void			CommitPendingVirtualRange()
	{
		if (committed.min_inline_virtual_pos > uncommitted.min_inline_virtual_pos)
			committed.min_inline_virtual_pos = uncommitted.min_inline_virtual_pos;

		if (committed.max_inline_virtual_pos < uncommitted.max_inline_virtual_pos)
			committed.max_inline_virtual_pos = uncommitted.max_inline_virtual_pos;
	}
};

/** Container width type.

	When calculating and working with container widths, sometimes we need the
	full width of the ancestor multicol container (column-span:all), while in
	other cases we want the width of the containing block established by a
	column.

	These values differ only if there is actually a multicol container
	ancestor. */

enum ContainerWidthType
{
	INNER_CONTENT_WIDTH, ///< Width of one column
	OUTER_CONTENT_WIDTH ///< Content width of the ancestor multicol container
};

/** Flags that control the state of the container related with an list item
	marker state.

	Flexible marker vertical alignment (first two lowest bits):
	They describe whether the container has a flexible marker and if yes,
	in what state it is. Flexible marker is present when the container,
	whose placeholder has display list-item and list-style-position outside,
	has	a list marker that is the only element in its line and there was no
	interruption that changed the marker to "not flexible" state.
	The states are:

	FLEXIBLE_MARKER_NOT_ALIGNED: The marker was not yet aligned to any line
	during the container reflow.
	FLEXIBLE_MARKER_ALIGNED_TO_SINGLE_OUTSIDE: Valid only when in
	FLEXIBLE_MARKER_NOT_ALIGNED state.
	The marker was aligned with a line that contains only another outside marker
	(may happen in case of nested lists). In such state we disallow another
	alignment with the further lines that contain only an outside marker.
	Flexible marker aligned (FLEXIBLE_MARKER_NOT_ALIGNED flag off):
	The marker was aligned with a line that has some other element than
	outside marker on it. In such state the marker won't be moved further.
	This is also immediate state, if there is something more on the marker
	line itself.

	Flexible marker also doesn't affect the reflow position inside
	its container.

	Interruptions that put the outside marker immediately to
	"not flexible" state:
	- table
	- <br>.

	Descendants whose content can't influence the flexible marker alignment:
	- floating boxes
	- absolute positioned boxes
	- transformed boxes
	- all non-blocks

	ANCESTOR_HAS_FLEXIBLE_MARKER:
	Set when some ancestor container has an outside list item marker in
	either FLEXIBLE_MARKER_NOT_ALIGNED or
	FLEXIBLE_MARKER_ALIGNED_TO_SINGLE_OUTSIDE state.

	MARKER_BEFORE_NEXT_ELEMENT - the flag is turned on when a list item marker
	allocates line space. It is turned off when the next element on the same
	line allocates line space or the line as vertical layout is closed.

	INSIDE_MARKER_BEFORE_COMMIT - the flag is turned on when an inside list
	item marker allocates line space. It is turned off when the marker content
	is commited to a line. It is used to prevent word count increase (an inside
	marker should not be counted as a separate word, so that when text-align
	is justify, it sticks to the following word. */

enum ListItemMarkerStateFlags
{
	// Vertical alignment of an outside marker state.
	FLEXIBLE_MARKER_NOT_ALIGNED = 1,
	FLEXIBLE_MARKER_ALIGNED_TO_SINGLE_OUTSIDE = 2,

	ANCESTOR_HAS_FLEXIBLE_MARKER = 4,

	MARKER_BEFORE_NEXT_ELEMENT = 8,
	INSIDE_MARKER_BEFORE_COMMIT = 16,
};

/** State of list item layout. */

class ListItemState
{
private:

	/** Value assigned to the previously added list item. */

	int				previous_value;

	/** TRUE if the list items have descending numeration */

	BOOL			reversed;

public:

					ListItemState()
						: previous_value(0),
						  reversed(FALSE) {}

	/** Initialize the list item counter.

		Calculate the initial value minus 1 (or plus 1, if the counting is
		reversed) to use on list item children of the specified
		element. Returns FALSE on OOM, TRUE otherwise. */

	BOOL			Init(LayoutInfo& info, HTML_Element* html_element);

	/** Return TRUE if counting is reversed. */

	BOOL			IsReversed() const { return reversed; }

	/** Get the numeric value to assign to the specified list item element.

		If the specified element defines a number, use and return
		that. Otherwise, increase (or decrease for reversed lists) the previous
		value used with with 1 and return that. */

	int				GetNewValue(HTML_Element* list_item_element);
};

/** Container reflow state.

	Its data is organized in three parts:
	PART 1 - data mostly initialized in the constructor
	PART 2 - data mostly initialized in Reset()
	PART 3 - min/max width calculation state data (mostly initialized in InitMinMaxData() and Reset()) */

class ContainerReflowState
{
public:

	// PART 1 - data mostly initialized in the constructor

#ifdef SUPPORT_TEXT_DIRECTION

	/** BiDi status. */

	BidiCalculation*
					bidi_calculation;

#endif // SUPPORT_TEXT_DIRECTION

	/** Current reflowed line. */

	VerticalLayout*	reflow_line;

	/** Distance from the top border edge of the current block formatting
		context to the top border edge of the parent container. */

	LayoutCoord		parent_bfc_y;

	/** Minimum distance from the top border edge of the current block
		formatting context to the minimum top border edge of the parent container. */

	LayoutCoord		parent_bfc_min_y;

	/** Distance from the left border edge of the current block formatting
		context to the left border edge of the parent container. */

	LayoutCoord		parent_bfc_x;

	/** CSS specified height. */

	LayoutCoord		css_height;

	/** Bottom of lowest float, if floats affect the height of this container; otherwise it will be LAYOUT_COORD_MIN. */

	LayoutCoord		float_bottom;

	/** Maximum height that current line is allowed to use. */

	LayoutCoord		max_line_height;

private:

	/** Position on virtual line that next line should start on. */

	LayoutCoord		current_line_position;

public:

	/** Base character from last text box. */

	UnicodePoint	last_base_character;

	/** Last element that the line can be broken before, i.e. the current start
		element candidate of the next line. */

	HTML_Element*	break_before_content;

	/** run-in or compact? First-line cancels run-ins. */

	BOOL			is_run_in;

	/** If this element has a first-line pseudo-element and current line is first line. */

	BOOL			is_css_first_line;

	/** If this container has a first-letter pseudo-element. */

	unsigned short	has_css_first_letter;

	/** The first floating box that is to be moved below the current line. */

	FloatingBox*	first_moved_floating_box;

	/** @see ListItemMarkerStateFlags. */

	int				list_item_marker_state;

	/** Used to adjust the reflow_position value in the moment of finishing
		a list item container if the first line in it has just
		an outside list item marker lying on it. */

	LayoutCoord		list_marker_bottom;

	/** State of list item layout. */

	ListItemState	list_item_state;

	/** Relative position of inline elements */

	LayoutCoord		relative_x;
	LayoutCoord		relative_y;

#ifdef CSS_TRANSFORMS

	/** Transform of inline elements */

	AffineTransform	*transform;

#endif // CSS_TRANSFORMS

	/* First weak element in a an unbreakable sequence of elements. (MSR) */

	HTML_Element*	first_unbreakable_weak_element;

	/* Last weak element in a an unbreakable sequence of elements. (MSR) */

	HTML_Element*	last_unbreakable_weak_element;

	/* Previous letter should allow aggressive linebreak (ERA, awb) */

	BOOL			last_letter_allow_aggressive_break_after;

	/** Do we have any line content at all? */

	BOOL			line_has_content;

	/** Is this container in the initial block formatting context? */

	BOOL			affects_flexroot;

	/** Rightmost edge among the absolute positioned descendants, relative to this container. */

	LayoutCoord		abspos_right_edge;

	/** Largest sum of horizontal border, padding and offset among descendant absolutely positioned boxes. */

	LayoutCoord		abspos_noncontent;

	/** Largest percentage width among descendant absolutely positioned boxes. */

	LayoutFixed		abspos_percentage;

	/** Forcibly prevents the bottom margin from being collapsed if there is element in the layout stack
		with applied clearance  */

	BOOL			clearance_stops_bottom_margin_collapsing;

	/** TRUE if there was a line that ignored recommended limited paragraph width
		and we haven't yet encountered something that could indicate a new
		"paragraph" inside the container. That can be for example a block
		level element that influences the reflow position in a container
		or a layout break due to double <br>. */

	BOOL			was_lpw_ignored;

	/** Total available maximum width for this container.

		Set to "infinity" when not inside fixed-width containers, but
		restricted when inside fixed-width containers (in many cases the same
		as content_width - but not always). */

	LayoutCoord		total_max_width;

	/** TRUE if this container, or its parent container, has a fixed width specified. */

	BOOL			inside_fixed_width;

	/** TRUE if this container has a fixed height specified.
		Also TRUE when this container has a percentage height and its parent conatiner
		has fixed height specified. */

	BOOL			inside_fixed_height;

	/** TRUE if final height of this container is already known. Set in UpdateHeight() */

	BOOL			height_known;

	// PART 2 - data mostly initialized in Reset()

	/** Currently reflowed line's reflow state */

	LineReflowState	line_state;

	/** If current line is forced to be ended. */

	BOOL			forced_line_end;

	/** Bottom of reflow. */

	LayoutCoord		reflow_position;

	/** Page breaking policy after current element. */

	BREAK_POLICY	page_break_policy;

	/** Column breaking policy after current element. */

	BREAK_POLICY	column_break_policy;

	/** Margin collapsing state. */

	VerticalMargin	margin_state;

	/** Prevent the bottom margin of elements in the layout stack from being collapsed with their
		container's bottom margin. */

	BOOL			stop_bottom_margin_collapsing;

	/** Available width for content. */

	LayoutCoord		content_width;

	/** Available width for content if spanning all columns. */

	LayoutCoord		outer_content_width;

private:

	/** Width of word that is currently being laid out. */

	LayoutCoord		word_width;

	/** Width of non-content (margin, border, padding) currently being laid out. */

	LayoutCoord		noncontent_width;

	/** Whitespace before current word. */

	LayoutCoord		leading_whitespace;

public:

	/** Number of word that are currently being laid out. */

	short			words;

	/** Is there whitespace before current word? */

	BOOL			collapse_whitespace;

	/** Has content been allocated since last commit? Note that this may include zero-width content. */

	BOOL			content_allocated;

	/** This is a candidate for "height is min height". To actually
	    apply the tweak, apply_height_is_min_height has to be set. */

	BOOL			height_is_min_height_candidate;

	/** Whether the "height is min height" tweak should actually be
	    applied. If true, then the height property is really the
	    min-height, and not the height. */

	BOOL			apply_height_is_min_height;

	/** A previous element of display type "run-in" or "compact". */

	Content_Box*	pending_run_in_box;

	/** Margin space for the compact. */

	LayoutCoord		pending_compact_space;

	// PART 3 - min/max width calculation state data

	/** Propagate min/max widths to parent? */

	BOOL			propagate_min_max_widths;

	/** (Re-)calculate min/max widths in this reflow pass? */

	BOOL			calculate_min_max_widths;

	/** Uncommitted contribution to line_max_width. */

	LayoutCoord		pending_line_max_width;

	/** Minimum width of word/inline element that is currently being laid out, and not yet committed to the line.

	    This may not be the same as word_width, because inline elements
	    laid out may have widths given in percentage, which should not
	    affect min width in the same way. */

	LayoutCoord		pending_line_min_width;

	/** This is the part of the current word that is weak content (in MSR mode). */

	LayoutCoord		weak_content_width;

	/** This is the part of the content that is defined to be True Table content. */

	long			true_table_content_width;

	/** Number of lines (or content parts without forced line break) that is either True Table or not. */

	int				true_table_lines;
	int				non_true_table_lines;

	/** Maximum width of current virtual line. */

	LayoutCoord		line_max_width;

	/** Available space for max width.

		In auto-width containers this is "indefinite". However, if the actual maximum
		container width can be determined (becaused of fixed width), it will be set to a
		fixed value, which is used to keep min_reflow_position (and ultimately the
		minimum height of the container) correct. */

	LayoutCoord		available_line_max_width;

	/** If maximum width of its container is satisfied, this will be the bottom of lowest float; otherwise it will be LAYOUT_COORD_MIN. */

	LayoutCoord		float_min_bottom;

	/** Current reflowed block may not overlap with floats. */

	BOOL			avoid_float_overlap;

	/** Current reflow position if maximum_width is satisfied. */

	LayoutCoord		min_reflow_position;

	/** Stack position of current element if maximum_width is satisfied.*/

	LayoutCoord		cur_elm_min_stack_position;

	/** Minimum height of virtual line. */

	LayoutCoord		virtual_line_min_height;

	/** Ascent of virtual line. */

	LayoutCoord		virtual_line_ascent;

	/** Descent of virtual line. */

	LayoutCoord		virtual_line_descent;

	/** Uncommitted minimum height of virtual line. */

	LayoutCoord		pending_virtual_line_min_height;

	/** Uncommitted ascent of virtual line. */

	LayoutCoord		pending_virtual_line_ascent;

	/** Uncommitted descent of virtual line. */

	LayoutCoord		pending_virtual_line_descent;

	/** Minimum distance from left content edge of this container to the left content edge of the BFC.

		Does not include percentual padding and margin. May be negative (due to negative margins). */

	LayoutCoord		bfc_left_edge_min_distance;

	/** Minimum distance from right content edge of this container to the right content edge of the BFC.

		Does not include percentual padding and margin. May be negative (due to negative margins). */

	LayoutCoord		bfc_right_edge_min_distance;

	/** Total maximum width of floats that affect the current layout element. */

	LayoutCoord		floats_max_width;

	/** New horizontal MARQUEE scroll width.

		Derived from maximum width during width propagation from child content. */

	LayoutCoord		marquee_scroll_width;

#ifdef DEBUG
	/** Minimum width before reflow. */

	LayoutCoord		old_min_width;

	/** Maximum width before reflow. */

	LayoutCoord		old_max_width;
#endif // DEBUG

					ContainerReflowState()
					  :
#ifdef SUPPORT_TEXT_DIRECTION
					    bidi_calculation(NULL),
#endif // SUPPORT_TEXT_DIRECTION
						reflow_line(NULL),
						parent_bfc_y(LAYOUT_COORD_MIN),
						parent_bfc_min_y(LAYOUT_COORD_MIN),
						parent_bfc_x(LAYOUT_COORD_MIN),
						css_height(CONTENT_HEIGHT_AUTO),
						float_bottom(LAYOUT_COORD_MIN),
						max_line_height(LAYOUT_COORD_MAX),
						current_line_position(0),
						last_base_character(0),
						break_before_content(NULL),
						first_moved_floating_box(NULL),
						list_item_marker_state(0),
						list_marker_bottom(0),
						relative_x(0),
						relative_y(0),
#ifdef CSS_TRANSFORMS
						transform(NULL),
#endif
						last_letter_allow_aggressive_break_after(FALSE),
						line_has_content(FALSE),
						affects_flexroot(FALSE),
						abspos_right_edge(0),
						abspos_noncontent(0),
						abspos_percentage(0),
						clearance_stops_bottom_margin_collapsing(FALSE),
						was_lpw_ignored(FALSE),
						total_max_width(LAYOUT_COORD_MAX),
						inside_fixed_width(FALSE),
						inside_fixed_height(FALSE),
						height_known(FALSE)
					{}
#ifdef SUPPORT_TEXT_DIRECTION
					~ContainerReflowState() { OP_DELETE(bidi_calculation); }
#endif

	/** Reset reflow state initially or after ::first-line.

		Will be called at the beginning of layout, and after the first line has
		been completed (if there is a ::first-line pseudo element). Only
		variables that should be reset after ::first-line should be put
		here. Everything else belongs in the constructor. */

	void			Reset() {
								line_state.Reset();
								forced_line_end = FALSE;
								page_break_policy = BREAK_ALLOW;
								column_break_policy = BREAK_ALLOW;
								margin_state.Reset();
								word_width = LayoutCoord(0);
								noncontent_width = LayoutCoord(0);
								leading_whitespace = LayoutCoord(0);
								words = 0;
								collapse_whitespace = TRUE;
								content_allocated = FALSE;

								pending_run_in_box = NULL;
								first_unbreakable_weak_element = NULL;
								last_unbreakable_weak_element = NULL;

								if (calculate_min_max_widths)
								{
									pending_line_max_width = LayoutCoord(0);
									pending_line_min_width = LayoutCoord(0);
									weak_content_width = LayoutCoord(0);
									true_table_content_width = 0;
									true_table_lines = 0;
									non_true_table_lines = 0;
								}
							}

	/** Initialize data only used for min/max calculation. */

	void			InitMinMaxData()
	{
		line_max_width = LayoutCoord(0);
		available_line_max_width = LAYOUT_COORD_MIN;
		float_min_bottom = LAYOUT_COORD_MIN;
		cur_elm_min_stack_position = LayoutCoord(0);
		virtual_line_min_height = LayoutCoord(0);
		virtual_line_ascent = LayoutCoord(0);
		virtual_line_descent = LayoutCoord(0);
		pending_virtual_line_min_height = LayoutCoord(0);
		pending_virtual_line_ascent = LayoutCoord(0);
		pending_virtual_line_descent = LayoutCoord(0);
		bfc_left_edge_min_distance = LayoutCoord(0);
		bfc_right_edge_min_distance = LayoutCoord(0);
		floats_max_width = LayoutCoord(0);
		marquee_scroll_width = LayoutCoord(0);
	}

	LayoutCoord		GetWordWidth() const { return word_width; }

	LayoutCoord		GetNoncontentWidth() const { return noncontent_width; }

	LayoutCoord		GetLeadingWhitespaceWidth() const { return leading_whitespace; }

	void			ResetPendingContent() { leading_whitespace = LayoutCoord(0); word_width = LayoutCoord(0); noncontent_width = LayoutCoord(0); }

	LayoutCoord		GetLineCurrentPosition() const { return current_line_position; }

	void			AllocateLineVirtualSpace(LayoutCoord allocate_word, LayoutCoord allocate_non_content, LayoutCoord allocate_leading_whitespace)
	{
		line_state.AdjustPendingVirtualRange(GetLineCurrentPosition() + GetPendingContent(), allocate_word + allocate_non_content + allocate_leading_whitespace);

		word_width += allocate_word;
		noncontent_width += allocate_non_content;
		leading_whitespace += allocate_leading_whitespace;
	}

	void			CommitPendingInlineContent(const Line* line);

	/** A new line has been added for reflow. */

	void			StartNewLine() { line_state.ResetCommitted(); }

	LayoutCoord		GetVirtualLineHeight() const
	{
					LayoutCoord height = virtual_line_ascent + virtual_line_descent;

					return MAX(height, virtual_line_min_height);
	}

	void			GrowVirtualLineHeight(LayoutCoord line_height, LayoutCoord above_baseline, LayoutCoord below_baseline)
	{
					if (virtual_line_min_height < line_height)
						virtual_line_min_height = line_height;

					if (virtual_line_ascent < above_baseline)
						virtual_line_ascent = above_baseline;

					if (virtual_line_descent < below_baseline)
						virtual_line_descent = below_baseline;
	}

	LayoutCoord		GetPendingContent() const { return word_width + noncontent_width + leading_whitespace; }

	/** Finish a "virtual" line for maximum width calculation. */

	void			FinishVirtualLine()
	{
					LayoutCoord virtual_line_height = virtual_line_ascent + virtual_line_descent;

					if (virtual_line_height < virtual_line_min_height)
						virtual_line_height = virtual_line_min_height;

					if (virtual_line_height > 0)
						min_reflow_position = cur_elm_min_stack_position + virtual_line_height;

					virtual_line_min_height = LayoutCoord(0);
					virtual_line_ascent = LayoutCoord(0);
					virtual_line_descent = LayoutCoord(0);
	}

	/* This pool has two purposes: First, it takes some load off the malloc,
	   because these elements are large and large elements can be hard
	   to allocate.  Second, because there are very few such elements but
	   they are heavily used, it provides them with improved locality on
	   systems with tiny caches and poor mallocs (Nokia/Symbian type things). */

	void*			operator new(size_t size) OP_NOTHROW { return g_container_reflow_state_pool->New(sizeof(ContainerReflowState)); }
	void			operator delete(void* ptr, size_t size) { g_container_reflow_state_pool->Delete(ptr); }
};

/** Vertical layout element. */

class VerticalLayout
  : public Link
{
public:
	/** Is this a line? */

	virtual BOOL	IsLine() const { return FALSE; }

	/** Is this a 'first-line' line? */

	virtual BOOL	IsFirstLine() const { return FALSE; }

	/** Is this a block? */

	virtual BOOL	IsBlock() const { return FALSE; }

	/** Is this element a page break? */

	virtual BOOL	IsPageBreak() const { return FALSE; }

	/** Is this a layout break? */

	virtual BOOL    IsLayoutBreak() const { return FALSE; }

	/** Is this an element that logically belongs to the stack? */

	virtual BOOL	IsInStack(BOOL include_floats = FALSE) const { return TRUE; }

	/** Get height. */

	virtual LayoutCoord
					GetLayoutHeight() const = 0;

	/** Get height if maximum width is satisfied. */

	virtual LayoutCoord
					GetLayoutMinHeight() const { return LayoutCoord(0); }

	/** Get baseline of vertical layout.

		@param last_line TRUE if last line baseline search (inline-block case).	*/

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const = 0;

	/** Get position of box. */

	virtual LayoutCoord
					GetStackPosition() const = 0;

	/** Convenience overloading. */

    VerticalLayout*	Suc() const { return static_cast<VerticalLayout*>(Link::Suc()); }

	/** Convenience overloading. */

    VerticalLayout*	Pred() const { return static_cast<VerticalLayout*>(Link::Pred()); }

	/** Find previous non-float, non-absolute element. */

	VerticalLayout*	GetPreviousElement(BOOL include_floats = FALSE) const;

	/** Calculate bottom margins of layout element, by collapsing with adjoining child margins.
	 * This is only done when skipping layout of a box.
	 * @param parent_cascade parent properties
	 * @param info Layout information structure
	 * @param bottom_margin Margin state
	 * @return OpBoolean::IS_FALSE if top and bottom margins of this element are adjoining,
	 * OpBoolean::IS_TRUE otherwise, unless an error occurred.
	 */

	virtual OP_BOOLEAN
					CalculateBottomMargins(LayoutProperties* parent_cascade, LayoutInfo& info, VerticalMargin* bottom_margin) const { return OpBoolean::IS_TRUE; }

	/** Calculate top margins of layout element, by collapsing with adjoining child margins.
	 * This is only done when skipping layout of a box.
	 * @param parent_cascade parent properties
	 * @param info Layout information structure
	 * @param top_margin Margin state
	 * @return OpBoolean::IS_FALSE if top and bottom margins of this element are adjoining,
	 * OpBoolean::IS_TRUE otherwise, unless an error occurred.
	 */

	virtual OP_BOOLEAN
					CalculateTopMargins(LayoutProperties* parent_cascade, LayoutInfo& info, VerticalMargin* top_margin) const { return OpBoolean::IS_TRUE; }

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element) {}

	/** Page can be broken before this layout element.

		Called when CSS specified orphans and widows have been
		satified. Used only for lines and BRs. */

	virtual void	SetAllowBreakBefore() {}

#ifdef PAGED_MEDIA_SUPPORT

	/** Attempt to break page.

		Will attempt to break page in the subtree established by this element. */

	virtual BREAKST	AttemptPageBreak(LayoutInfo& info, int strength) { return BREAK_KEEP_LOOKING; }

#endif // PAGED_MEDIA_SUPPORT

	/** Get page break policy before this layout element. */

	virtual BREAK_POLICY
					GetPageBreakPolicyBefore() const { return BREAK_ALLOW; }

	/** Get page break policy after this layout element. */

	virtual BREAK_POLICY
					GetPageBreakPolicyAfter() const { return BREAK_ALLOW; }

	/** Get column break policy before this layout element. */

	virtual BREAK_POLICY
					GetColumnBreakPolicyBefore() const { return BREAK_ALLOW; }

	/** Get column break policy after this layout element. */

	virtual BREAK_POLICY
					GetColumnBreakPolicyAfter() const { return BREAK_ALLOW; }

	/** Is this a True Table candidate? */

	virtual TRUE_TABLE_CANDIDATE
					IsTrueTableCandidate() const { return POSITIVE; }

	/**
	 * Move this VerticalLayout and all its content down by an offset.
	 *
	 * @param offset_y Offset to move down
	 * @param containing_element. Element of the container for this vertical layout.
	 *			Primarily used when moving lines, to determine where to stop.
	 */
	virtual void	MoveDown(LayoutCoord offset_y, HTML_Element* containing_element) = 0;

	/** Distribute content into columns.

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	Columnize(Columnizer& columnizer, Container* container) { return TRUE; }

	/** Figure out to which column(s) or spanned element a box belongs. */

	virtual void	FindColumn(ColumnFinder& cf, Container* container) {}
};


#define BOUNDING_BOX_LINE_TOP_BOTTOM_MAX	0x01ff

/** Layout line.

	Inline elements are ordered in lines. */

class Line
  : public VerticalLayout
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

protected:

	/** Y position of box. */

	LayoutCoord		y;

	/** Element that this line starts with. */

	HTML_Element*	start_element;

	/** Virtual X position that this line starts with. */

	LayoutCoord		position;

	/** X position of box. */

	LayoutCoord		x;

	/** Width of this line. */

	LayoutCoord		width;

	/** Space that has been consumed on this line. */

	LayoutCoord		used_space;

	/** Height of line. */

	LayoutCoord		height;

	/** Baseline of line. */

	LayoutCoord		baseline;

	union
	{
		struct
		{
			/** Number of whitespace separated words on this line. */

			unsigned int	number_of_words:11;

			/** Left edge of bounding box */

			unsigned int	bounding_box_left:1;

			/** Right edge of bounding box */

			unsigned int	bounding_box_right:1;

			/** Top edge of bounding box */

			unsigned int	bounding_box_top:9;

			/** Bottom edge of bounding box */

			unsigned int	bounding_box_bottom:9;

			/** Line is ended by a forced break. Either <br> or content: "\A" */

			unsigned int	has_forced_break:1;
		} packed;
		UINT32		packed_init;
	};

	union
	{
		struct
		{
			/** Width of trailing whitespace on line. */

			unsigned int	trailing_whitespace:14;

			/** If FALSE, a column/page break before this element should be avoided.

				This is used to honor orphans / widows settings. */

			unsigned int	can_break_before:1;

			/** Is this a 'first-line' line? */

			unsigned int	is_first_line:1;

			/** Logical left overflow */

			unsigned int	logical_overflow_left:1;

			/** Logical right overflow */

			unsigned int	logical_overflow_right:1;

			/** Does this line contain an outside list item marker? */

			unsigned int	has_outside_list_item_marker:1;

			/** Does this line contain an outside list item marker and nothing else? */

			unsigned int	has_only_outside_list_item_marker:1;
		} packed2;
		UINT32		packed2_init;
	};

#ifdef SUPPORT_TEXT_DIRECTION
	/** Represents one stretch of text at the same bidi level. */

	struct BidiSegment
	{
		/** Width of segment in pixels. */

		signed int		width:15;

		/** Pixel offset of start of segment. This is the offset of the segment
		    /after/ bidi reordering has been done. */

		signed int		offset:16;

		unsigned int	left_to_right:1;

		/** The element to which the first character in this segment belongs. */

		HTML_Element*   start_element;

		/** Number of words to the visual left of this segment */

		short			words_to_the_left;
	};

	/** Compute bidi segments for this line. */

	OP_STATUS		ResolveBidiSegments(const Head& paragraph_bidi_segments, int& bidi_segment_count, BidiSegment*& bidi_segments);

	/** Calculate number of words in each segment, and how manu words are to the left of each segment. Used by align=justify for bidi lines */

	OP_STATUS       CalculateBidiJustifyPositions(BidiSegment* bidi_segments, int bidi_segment_count, LineSegment* segment, Line* next_line, HTML_Element* containing_element);

#endif

public:

					Line()
						: y(0),
						  x(0),
						  width(0),
						  used_space(0),
						  height(0),
						  baseline(0) { packed_init = 0; packed2_init = 0; packed.bounding_box_right = 1; packed.bounding_box_bottom = BOUNDING_BOX_LINE_TOP_BOTTOM_MAX; }

	/** Get the next line in the layout stack. */

	Line*			GetNextLine(BOOL& anonymous_box_ended) const;

	/** Return TRUE if the specified virtual range may be part of this line.

		@param start Virtual start position
		@param end Virtual end position.

		@return Telling if a line contains a virtual position or not is
		somewhat tricky, because of negative margins and empty lines. This
		method is optimistic; it's supposed to only return FALSE if we can be
		absolutely certain that no part of the virtual range is on this
		line. Otherwise it should return TRUE. */

	BOOL			MayIntersectLogically(LayoutCoord start, LayoutCoord end) const;

	/** Set the line position, relative to the border edge of the container. */

	void			SetPosition(LayoutCoord new_x, LayoutCoord new_y) { x = new_x; y = new_y; }

	/** Set width of line. */

	void			SetWidth(LayoutCoord new_width) { width = new_width; }

	/** Set height of line. */

	void			SetHeight(LayoutCoord new_height) { if (height < new_height) height = new_height; }

	/** Get width of line. */

	LayoutCoord		GetWidth() const { return width; }

	/** Set leading HTML Element. */

	void			SetStartElement(HTML_Element* element) { start_element = element; }

	/** Get leading HTML Element. */

	HTML_Element*	GetStartElement() const { return start_element; }

	/** Set virtual X position that this line starts with. */

	void			SetStartPosition(LayoutCoord start_position) { position = start_position; }

	/** Get virtual X position that this line starts with. */

	LayoutCoord		GetStartPosition() const { return position; }

	/** Get virtual X position that this line ends with.

		This is used to lay out elements at the end of the line, and by
		preceding lines to know where they should pick up. */

	LayoutCoord		GetEndPosition() const { return position + used_space; }

	/** Get height of line. */

	virtual LayoutCoord
					GetLayoutHeight() const { return height > 0 ? height : LayoutCoord(0); }

	/** Get position of line. */

	virtual LayoutCoord
					GetStackPosition() const { return y; }

	/** Get Y position of the line, relative to the border edge of the container. */

	LayoutCoord		GetY() const { return y; }

	/** Get X position of the line, relative to the border edge of the container. */

	LayoutCoord		GetX() const { return x; }

	/** Get baseline of vertical layout.

		@param last_line TRUE if last line baseline search (inline-block case).	*/

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const { return baseline; }

	/** How much space left is there on this line? */

	LayoutCoord		GetSpaceLeft() const { return width - used_space; }

	/** How much space is used on this line? */

	LayoutCoord		GetUsedSpace() const { return used_space - LayoutCoord(packed2.trailing_whitespace); }

	/** Get trailing whitespace of line. */

	LayoutCoord		GetTrailingWhitespace() const { return LayoutCoord(packed2.trailing_whitespace); }

	/** Set final number of whitespace separated words on this line. */

	void			SetNumberOfWords() { if (!packed2.trailing_whitespace) packed.number_of_words++; }

	/** Get number of whitespace separated words on this line. */

	short			GetNumberOfWords() const { return packed.number_of_words; }

	/** Allocate space on this line. */

	void			AllocateSpace(LayoutCoord box_width, LayoutCoord leading_whitespace, unsigned short words, LayoutCoord box_above_baseline, LayoutCoord box_below_baseline);

	/** Is this a line? */

	virtual BOOL	IsLine() const { return TRUE; }

	/** Is this a 'first-line' line? */

	virtual BOOL	IsFirstLine() const { return packed2.is_first_line; }

	/** Is this an element that logically belongs to the stack?
		Lines with only empty content are not considered to be part of the
		stack. Among other things, they need to be skipped when margin
		collapsing is performed. */

	virtual BOOL	IsInStack(BOOL include_floats = FALSE) const { return HasContent(); }

	/** This a 'first-line' line. */

	void			SetFirstLine() { packed2.is_first_line = 1; }

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element);

	/** Orphans and widows settings do not prevent a column / page break before this element. */

	void			SetAllowBreakBefore() { packed2.can_break_before = TRUE; }

	/** Get page break policy before this layout element. */

	virtual BREAK_POLICY
					GetPageBreakPolicyBefore() const;

	/** Get page break policy after this layout element. */

	virtual BREAK_POLICY
					GetPageBreakPolicyAfter() const;

	/** Get column break policy before this layout element. */

	virtual BREAK_POLICY
					GetColumnBreakPolicyBefore() const;

	/** Get column break policy after this layout element. */

	virtual BREAK_POLICY
					GetColumnBreakPolicyAfter() const;

	/** Get bounding box values. */

	BOOL			GetBoundingBoxLeft() const { return packed.bounding_box_left == 1; }
	BOOL			GetBoundingBoxRight() const { return packed.bounding_box_right == 1; }
	short			GetBoundingBoxTop() const { return packed.bounding_box_top; }
	short			GetBoundingBoxBottom() const { return packed.bounding_box_bottom; }
	BOOL			GetBoundingBoxTopInfinite() const { return packed.bounding_box_top == BOUNDING_BOX_LINE_TOP_BOTTOM_MAX; }
	BOOL			GetBoundingBoxBottomInfinite() const { return packed.bounding_box_bottom == BOUNDING_BOX_LINE_TOP_BOTTOM_MAX; }

	/** Has logical left overflow? */

	BOOL			HasContentBeforeStart() const { return packed2.logical_overflow_left; }

	/** Has logical right overflow? */

	BOOL			HasContentAfterEnd() const { return packed2.logical_overflow_right; }

	/** Set logical overflow values */

	void			SetHasOverflowingContent(BOOL left, BOOL right) { packed2.logical_overflow_left = left; packed2.logical_overflow_right = right; }

	/** Set bounding box values. */

	void			SetBoundingBox(LayoutCoord line_box_left, LayoutCoord line_box_right, LayoutCoord line_box_above_baseline, LayoutCoord line_box_below_baseline);

	/** Clear bounding box values. */

	void			ClearBoundingBox() { packed.bounding_box_right = 0; packed.bounding_box_bottom = 0; }

	/** Get the bounding box for this Line relative to the Line itself.
		@param bounding_box The returned bounding box. */
	void			GetBoundingBox(AbsoluteBoundingBox& bounding_box) const;

	/** Line is ended by a forced break. Either <br> or content: "\A" */
	BOOL			HasForcedBreak() const { return packed.has_forced_break; }

	/** Mark Line as ended by a forced break. Either <br> or content: "\A" */
	void			SetHasForcedBreak() { packed.has_forced_break = 1; }

	/** Specify if a line has content or not.

		Some lines are created even if there is no content, because even empty
		inline elements need to belong to a line, so that they can be
		traversed. Such empty lines must be ignored when margin collapsing is
		calculated. Initially, all lines are defined to have content. They will
		automatically be regarded as lines with content when non-empty content
		is added.

		@param has_content TRUE if the line has content, FALSE if not. FALSE
		cannot be used if actual content has already been added to the line. */

	void			SetHasContent(BOOL has_content)
	{
		if (has_content)
		{
			if (height < 0)
				height = LayoutCoord(0);
		}
		else
		{
			OP_ASSERT(height <= 0);
			height = LayoutCoord(-1);
		}
	}

	/** Return whether the line has content or not.

		@return TRUE if the line has content. If FALSE is returned, the line
		should not affect layout of preceding or subsequent elements in the
		layout stack, or ancestor containers. If FALSE is returned, it means
		that the line has no content, and should be regarded as non-existant in
		most senses, such as when it comes to margin collapsing. If FALSE is
		returned, the line is simply there for implementation-specific reasons,
		typically that it has to be there to be able to traverse some empty
		inline element, absolutely-positioned child content of such an empty
		element. */

	BOOL			HasContent() const { return height >= 0; }

	/**
	 * Move this VerticalLayout and all its content down by an offset.
	 *
	 * @param offset_y Offset to move down
	 * @param containing_element. Element of the container for this vertical layout.
	 *			Used to determine where the line stops.
	 */
	virtual void	MoveDown(LayoutCoord offset_y, HTML_Element* containing_element);

	/** Return the number of pixels of horizontal offset from a left aligned line in ltr.
		Text align 'justify' will be treated as the default align for given direction.
		Unless rtl_justify_offset is TRUE.

		@param props The computed styles for this line's container. Used to get text-align
					 and direction values.
		@param rtl_justify_offset Matters only in case of direction rtl.
			When TRUE we will compute the offset as if the line had text-align left.
			That is needed during the traverse, because if the line is justified,
			the particular boxes' offsets will be computed according to the current
			accumulated justified space. The base line content offset is
			then zero just as for left-to-right justified lines.
		@param directional If TRUE, return the offset from the right edge of a right aligned
						   line in rtl when the direction is rtl.
		@return The number of pixels to move this line to the left (negative) or right to adjust
				for text-alignment.	*/

	LayoutCoord		GetTextAlignOffset(const HTMLayoutProperties& props
#ifdef SUPPORT_TEXT_DIRECTION
									   , BOOL rtl_justify_offset = FALSE
									   , BOOL directional = FALSE
#endif // SUPPORT_TEXT_DIRECTION
									   ) const;

	/** Figure out to which column(s) or spanned element a box belongs. */

	virtual void	FindColumn(ColumnFinder& cf, Container* container);

	/** Set this to be an outside list item marker line. */

	void			SetIsOutsideListMarkerLine() { packed2.has_outside_list_item_marker = 1; }

	/** Set this to be a line that contains only an outside list item marker. */

	void			SetIsOnlyOutsideListMarkerLine() { packed2.has_only_outside_list_item_marker = 1; }

	/** Does this line contain an outside list item marker? */

	BOOL			IsOutsideListMarkerLine() const { return packed2.has_outside_list_item_marker; }

	/** Does this line contain an outside list item marker and nothing else? */

	BOOL			IsOnlyOutsideListMarkerLine() const { return packed2.has_only_outside_list_item_marker; }

	/** Does this line contain an inline box that is not affected by text-align property? */

	BOOL			HasBoxNotAffectedByTextAlign() const { OP_ASSERT(start_element); return IsOutsideListMarkerLine() || start_element->GetLayoutBox()->IsInlineCompactBox(); }
};

/** Layout break. */

class LayoutBreak
  : public VerticalLayout
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

private:

	/** HTML element. */

	HTML_Element*	html_element;

	union
	{
		struct
		{
			/** Can/must we break page before this block? See type BREAK_POLICY. */

			unsigned int
						page_break_before:3;

			/** Can/must we break page after this block? See type BREAK_POLICY. */

			unsigned int
						page_break_after:3;

			/** Can/must we break column before this block? See type BREAK_POLICY. */

			unsigned int
						column_break_before:2;

			/** Can/must we break column after this block? See type BREAK_POLICY. */

			unsigned int
						column_break_after:2;

			/** TRUE if this break terminates a preceding Line.

				Normally we only add a LayoutBreak if there's no preceding line
				to terminate, but if there's a BR with non-initial break-before
				or break-after properties, we need somewhere to store these,
				and the Line class is out of bits. */

			unsigned int
						is_line_terminator:1;

			/** If FALSE, a column/page break before this element should be avoided.

				This is used to honor orphans / widows settings. */

			unsigned int
					can_break_before:1;
		} packed;
		UINT32		packed_init;
	};

	/** X position of layout break. */

	LayoutCoord		x;

	/** Y position of layout break. */

	LayoutCoord		y;

	/** If this would be a line, it would be this wide. This value is
		used to calculate the position of the layout break in case of
		text-align:center or right during traverse. */

	LayoutCoord		width;

	/** Height of the layout break. */

	LayoutCoord		height;

public:

					LayoutBreak(HTML_Element* html_element, LayoutCoord ypos, LayoutCoord xpos, LayoutCoord w, LayoutCoord h)
						: html_element(html_element),
						  packed_init(0),
						  x(xpos),
						  y(ypos),
						  width(w),
						  height(h) { }

	/** Is this a layout break? */

	virtual BOOL    IsLayoutBreak() const { return TRUE; }

	/** Get html element break belongs to. */

	HTML_Element*	GetHtmlElement() const { return html_element; }

	/** Specify that this is a line terminator. */

	void			SetIsLineTerminator() { packed.is_line_terminator = 1; }

	/** Return TRUE if this is a line terminator. */

	BOOL			IsLineTerminator() const { return packed.is_line_terminator; }

	/** Get height. */

	virtual LayoutCoord
					GetLayoutHeight() const { return height; }

	/** Get baseline of vertical layout.

		@param last_line TRUE if last line baseline search (inline-block case).	*/

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const { return LayoutCoord(0); }

	/** Get position. */

	virtual LayoutCoord
					GetStackPosition() const { return y; }

	/** Get X position of the layout break, relative to the border edge of the container. */

	LayoutCoord		GetX() const { return x; }

	/** Set page and column breaking policies on this line break. */

	void			SetBreakPolicies(BREAK_POLICY page_break_before, BREAK_POLICY column_break_before,
									 BREAK_POLICY page_break_after, BREAK_POLICY column_break_after);

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element);

	/** Orphans and widows settings do not prevent a column / page break before this element. */

	void			SetAllowBreakBefore() { packed.can_break_before = TRUE; }

	/** Get page break policy before this layout element. */

	virtual BREAK_POLICY
					GetPageBreakPolicyBefore() const { return packed.page_break_before == BREAK_ALLOW ? packed.can_break_before ? BREAK_ALLOW : BREAK_AVOID : (BREAK_POLICY) packed.page_break_before; }

	/** Get page break policy after this layout element. */

	virtual BREAK_POLICY
					GetPageBreakPolicyAfter() const { return (BREAK_POLICY) packed.page_break_after; }

	/** Get column break policy before this layout element. */

	virtual BREAK_POLICY
					GetColumnBreakPolicyBefore() const { return packed.column_break_before == BREAK_ALLOW ? packed.can_break_before ? BREAK_ALLOW : BREAK_AVOID : (BREAK_POLICY) packed.column_break_before; }

	/** Get column break policy after this layout element. */

	virtual BREAK_POLICY
					GetColumnBreakPolicyAfter() const { return (BREAK_POLICY) packed.column_break_after; }

	/**
	 * Move this VerticalLayout and all its content down by an offset.
	 *
	 * @param offset_y Offset to move down
	 * @param containing_element. Element of the container for this vertical layout. Not used in this implementation.
	 */
	virtual void	MoveDown(LayoutCoord offset_y, HTML_Element* containing_element) { y += offset_y; }
};

#ifdef PAGED_MEDIA_SUPPORT

/** Explicit page break. */

class ExplicitPageBreak
  : public VerticalLayout
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

private:

	/** The last element before this page break. */

	HTML_Element*	break_after;

	/** Y position of box. */

	long			y;

public:

					ExplicitPageBreak(VerticalLayout* break_after);

	virtual LayoutCoord
					GetLayoutHeight() const { return LayoutCoord(0); }

	/** Get baseline of vertical layout.

		@param last_line TRUE if last line baseline search (inline-block case).	*/

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const { return LayoutCoord(0); }

	/** Should page break come into effect after layout element? */

	BOOL			Trails(VerticalLayout* break_after) const;

	/** Get position of box. */

	virtual LayoutCoord
					GetStackPosition() const { OP_ASSERT(0); return LayoutCoord(0); }

	/** Is this element a page break? */

	virtual BOOL	IsPageBreak() const { return TRUE; }

	/** Is this element an implicit page break? */

	virtual BOOL	IsImplicitPageBreak() const { return FALSE; }

	/** Is this element a pending page break? */

	virtual BOOL	IsPendingPageBreak() const { return FALSE; }

	/**
	 * Move this VerticalLayout and all its content down by an offset.
	 *
	 * @param offset_y Offset to move down
	 * @param containing_element. Element of the container for this vertical layout.
	 *
	 * OBS! This should be avoided if done after the actual layout. If you have a legitimate
	 *  use for this function, please implement it and remove the assert.
	 */

	virtual void	MoveDown(LayoutCoord offset_y, HTML_Element* containing_element) { OP_ASSERT(0); }
};

/** Implicit page break. */

class ImplicitPageBreak
  : public ExplicitPageBreak
{
public:

					ImplicitPageBreak(VerticalLayout* vertical_layout)
					  : ExplicitPageBreak(vertical_layout) {}

	/** Is this an element that logically belongs to the stack? */

	virtual BOOL	IsInStack(BOOL include_floats = FALSE) const { return FALSE; }

	/** Is this element an implicit page break? */

	virtual BOOL	IsImplicitPageBreak() const { return TRUE; }
};

/** Pending page break.

	There may only be one pending page break in the document at
    any given time.  It's used for finding where to continue
    page breaking at reflow. */

class PendingPageBreak
  : public ImplicitPageBreak
{
public:

					PendingPageBreak(VerticalLayout* vertical_layout)
					  : ImplicitPageBreak(vertical_layout) {}

	/** Is this element a pending page break? */

	virtual BOOL	IsPendingPageBreak() const { return TRUE; }
};

#endif // PAGED_MEDIA_SUPPORT

class Container
  : public Content
{
private:

	/** Layout stack.

		This is an ordered list of layout block boxes, floats and line
		boxes. Elements are drawn in the order of the list. */

	Head			layout_stack;

#ifdef SUPPORT_TEXT_DIRECTION

	/** List of bidi segments. */

	Head*			bidi_segments;

#endif

	enum CLOSED_BY
	{
		LINE = 0,
		BLOCK = 1,
		FINISHLAYOUT = 2,
		BREAK = 3
	};

	/** Add element to layout stack. */

	void			AddElement(VerticalLayout* elm)
	{
		elm->Out();

		if (reflow_state->reflow_line)
		{
			elm->Follow(reflow_state->reflow_line);

			if (reflow_state->has_css_first_letter != FIRST_LETTER_NO)
				if (reflow_state->reflow_line->IsInStack())
					reflow_state->has_css_first_letter = FIRST_LETTER_NO;
		}
		else
			elm->IntoStart(&layout_stack);

		reflow_state->reflow_line = elm;
		reflow_state->forced_line_end = FALSE;
	}

	/** Initialise reflow state. */

	BOOL			InitialiseReflowState() { if (reflow_state) return TRUE; reflow_state = new ContainerReflowState; return reflow_state != NULL; }

	/** Delete reflow state. */

	virtual void	DeleteReflowState() { delete reflow_state; reflow_state = NULL ;}

	/** Attempt to lay out run-in. */

	OP_BOOLEAN		LayoutRunIn(LayoutProperties* child_cascade, LayoutInfo& info, Content_Box* run_in, LayoutCoord pending_compact_space);

#ifdef PAGED_MEDIA_SUPPORT

	/** Break page.

		Insert page break and move layout elements down to new page. */

	BOOL			GetPendingPageBreak(LayoutInfo& info, VerticalLayout* available_break);

	/** Break page. */

	BOOL			AddPageBreaks(LayoutInfo& info, LayoutCoord& position);

#endif // PAGED_MEDIA_SUPPORT

	/** Insert page or column break.

		@param info Layout info
		@param page_break Policy for page breaking
		@param column_break Policy for column breaking
		@param multipane_container Nearest multi-pane (multicol or paged) container
		@param space_manager Nearest space manager.
		@return FALSE on OOM, TRUE otherwise. */

	BOOL			InsertPageOrColumnBreak(LayoutInfo& info, BREAK_POLICY page_break, BREAK_POLICY column_break, Container* multipane_container, SpaceManager* space_manager);

	/** Propagate break policies decomposed from the break-after property on some descendant. */

	void			PropagateBreakPolicies(BREAK_POLICY page_break, BREAK_POLICY column_break);

#ifdef SUPPORT_TEXT_DIRECTION

	/** Reconstruct bidi status when starting in the middle of a container. */

	BOOL			ReconstructBidiStatus(LayoutInfo& info);

#endif

	/** Set whether container is candidate for true table. */

	void			SetTrueTableCandidate(TRUE_TABLE_CANDIDATE candidate) { packed.true_table_candidate = candidate; }

	/** Push floats that should be below the current line below it. */

	void			PushFloatsBelowLine(LayoutInfo& info, Line* line);

	enum MarkerAlignControlValue
	{
		/** The line that we align the markers to is a line that contains something else
			than just an outside list item marker. */
		MARKER_ALIGN_PROPER_LINE,
		/** The line that we align the markers to contains only an outside list item marker.
			This value should be used for the first call. */
		MARKER_ALIGN_TO_SINGLE_OUTSIDE_FIRST_CALL,
		/** The line that we align the markers to contains only an outside list item marker.
			This value should be used for the recursive calls. */
		MARKER_ALIGN_TO_SINGLE_OUTSIDE_RECURSIVE_CALL
	};

	/** Aligns all the pending flexible markers with the line
		just being closed (baseline wise).

		@param baseline The baseline of the line that we align to.
						Relative to the container.
		@param control_value Controls the behavior during marker alignment
							 in first and recursive calls.
							 @see MarkerAlignControlValue. */

	void			AlignMarkers(LayoutCoord baseline, MarkerAlignControlValue control_value);

	/** Clears flexible markers or marks them aligned with a line that contains
		only an outside marker. Starts from this container and calls this
		method recursively on the ancestor container if it (or some further
		ancestor) has flexible markers.
		@see ListItemMarkerStateFlags.

		@param after_single_outside FALSE - set the markers as aligned,
									TRUE - marks them as aligned with a line
									that contains only an outside marker. */

	void			ClearFlexibleMarkers(BOOL after_single_outside = FALSE);

protected:

	union
	{
		struct
		{
			/** State of margin collapsing? */

			unsigned int	no_content:1;
			unsigned int	stop_top_margin_collapsing:1;

			/** If set, we cannot skip layout of this container, even if the width is unchanged. */

			unsigned int	unskippable:1;

			/** Height is relative (either percentage or depends on top/bottom offsets). */

			unsigned int	relative_height:1;

			/** TRUE if computed height is auto. */

			unsigned int	height_auto:1;

			/** Avoid page break inside container? */

			unsigned int	avoid_page_break_inside:1;

			/** Avoid column break inside container? */

			unsigned int	avoid_column_break_inside:1;

			/** Pending reflow? */

			unsigned int	needs_reflow:1;

			/** Is this a True Table candidate? */

			unsigned int	true_table_candidate:2;

			/** Set to TRUE when min/max values are calculated and up-to-date. */

			unsigned int	minmax_calculated:1;

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE
			/** Only traverse blocks? Set when this element needs to be traversed even if it needs reflow first. */

			unsigned int	only_traverse_blocks:1;
#endif // LAYOUT_TRAVERSE_DIRTY_TREE
		} packed;
		UINT32		packed_init;
	};

	/** Height of box. */

	LayoutCoord		height;

	/** Height of box if maximum_width is satisfied. */

	LayoutCoord		min_height;

	/** Minimum width.

		This is the minimal width this container can have without
		having child content overflow it. This is typically equal to
		the margin box width of the widest unbreakable piece of content
		in this container. */

	LayoutCoord		minimum_width;

	/** Normal layout mode minimum width.

		In normal layout mode, this is equal to minimum_width. In MSR
		and other ERA modes this is the minimum width _as if laid out_
		in normal mode; i.e. minimum width without applying any ERA
		tricks. */

	LayoutCoord		normal_minimum_width;

	/** Maximum width.

		This is the width that this container would need in order to
		avoid implicit line breaks and floats being shifted downwards
		(due to lack of horizontal space). This is also known as
		"preferred width" in some specs. */

	LayoutCoord		maximum_width;

	/** State of reflow in box.

		This should be deleted when reflow is complete. */

	ContainerReflowState*
					reflow_state;

	/** Update height of container. */

	virtual void	UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height);

	/** Do we need to calculate min/max widths of this box's content? */

	virtual BOOL	NeedMinMaxWidthCalculation(LayoutProperties* cascade) const;

	/** Apply min-width property constraints on min/max values. */

	virtual void	ApplyMinWidthProperty(const HTMLayoutProperties& props, LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width);

	/** Get min/max width. */

	virtual BOOL	GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const;

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth();

	/** Increase the height of the pending line so that it is at least
		as high as the minimal height of line boxes for this block, as
		specified by the 'line-height' property. */

	void			SetMinimalLineHeight(LayoutInfo &info, const HTMLayoutProperties &props);

	/** Calculates new line's position

		Calculates new line's position based on given input parameters:

		@param info Layout information structure
		@param element line's start element (needed for LPW calculations)
		@param prev previous layout element (if any)
		@param width line's initial width on input and final line's width on output
			   (may be shortened by floats or LPW)
		@param min_width line's min width
		@param line_height line's height
		@param line_x line's left offset
		@param line_y line's top offset
		@return max line's height. */

	LayoutCoord		GetLinePosition(const LayoutInfo& info, HTML_Element *element, VerticalLayout *prev, LayoutCoord& width, LayoutCoord min_width, LayoutCoord line_height, LayoutCoord& line_x, LayoutCoord& line_y) const;

public:

					Container();

	virtual			~Container();

	/** Has reflow been forced? */

	virtual BOOL	GetReflowForced() const { return packed.needs_reflow; }

	/** Set reflow force status, which will be checked and reset during SignalChange(). */

	virtual void	SetReflowForced(BOOL force_reflow_on) { packed.needs_reflow = !!force_reflow_on; }

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

	/** Signal that an HTML_Element in this container is about to be deleted. */

	void			SignalChildDeleted(FramesDocument* doc, Box* child);

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

	/** Remove previous layout element. */

	void			RemovePreviousLayoutElement(LayoutInfo &info);

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Increase actual width and min/max widths. */

	virtual void	IncreaseWidths(LayoutCoord increment);

	/** Recalculate the top margin after a new block box has been added to a container's layout
		stack. Collapse the margin with preceding adjacent margins if appropriate. If the top
		margin of this block is adjacent to an ancestor's top margin, it may cause the ancestor's
		Y position to change. If the top margin of this block is adjacent to a preceding sibling's
		bottom margin, this block may change its Y position.

		@return TRUE if the Y position of any element was changed. */

	BOOL			RecalculateTopMargins(LayoutInfo& info, const VerticalMargin* top_margin, BOOL bottom_margin = FALSE);

	/** Update margins, bounding box and start position for the next element in the layout stack.
		Children of this container call this method to set new bottom margin values to use in
		margin collapsing in which successive content in the layout stack participates. */

	virtual void	UpdateBottomMargins(const LayoutInfo& info, const VerticalMargin* bottom_margin, AbsoluteBoundingBox* child_bounding_box = NULL, BOOL has_inflow_content = TRUE);

	/** Expand relevant parent containers and their bounding-boxes to contain floating and
		absolutely positioned boxes. A call to this method is only to be initiated by floating or
		absolutely positioned boxes.

		@param info Layout information structure
		@param bottom Only relevant for floats: The bottom margin edge of the float, relative to
		top border-edge of this box.
		@param min_bottom Only relevant for floats: The bottom margin edge of the float if
		maximum width of its container is satisfied
		@param child_bounding_box Bounding-box of the propagating descendant, joined with the
		bounding-boxes of the elements up the parent chain towards this element. Relative to the
		top border-edge of this box.
		@param opts Bounding box propagation options */

	virtual void	PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts);

	/** Commit pending margin, to prevent it from collapsing with later margins. */

	void			CommitMargins();

	/** Propagate the right edge of absolute positioned boxes. */

	virtual void	PropagateRightEdge(const LayoutInfo& info, LayoutCoord right, LayoutCoord noncontent, LayoutFixed percentage);

	/** Propagate height of pane-attached float to its flow root. */

	virtual void	PropagatePaneFloatHeight(LayoutCoord float_height) { OP_ASSERT(!"Should only be called on multicol or paged container"); }

	/** Update bounding box. */

	virtual void	UpdateBoundingBox(const AbsoluteBoundingBox& child_bounding_box, BOOL skip_content);

	/** Cast this object to container, if it is a container */

	virtual Container*
					GetContainer() { return this; }

	/** This container is currently reflowing. */

	BOOL			IsReflowing()  { return reflow_state ? TRUE : FALSE; }

	/** In normal layout mode, if the computed width property is a pixel value, will it be used, unchanged?

		@return TRUE if an absolute width property value will always be honored. Returns
		FALSE if the value will be taken into consideration but content or containing
		block width may cause the used value will be different. */

	BOOL			HonorsAbsoluteWidth() const { return !placeholder->IsTableCell() && !placeholder->IsTableCaption() && !placeholder->IsFlexItemBox(); }

	/** Stop bottom margin collapsing if child has clearance applied */

	void			ClearanceStopsMarginCollapsing(BOOL stop_collapsing) { reflow_state->clearance_stops_bottom_margin_collapsing = stop_collapsing; }

	/** Returns container's margin state.

		@return TRUE if container's margins are adjoining and container has no content that could prevent
		margins from collapsing */

	BOOL			MarginsCollapsing() const { return !reflow_state->stop_bottom_margin_collapsing && !packed.stop_top_margin_collapsing && packed.no_content; }

	/** Get width of container. */

	virtual LayoutCoord
					GetWidth() const { return placeholder->GetWidth(); }

	/** Get the width of the containing block established by this container. */

	LayoutCoord		GetContentWidth() const { return reflow_state->content_width; }

	/** Calculate the width of the containing block established by this container. */

	virtual LayoutCoord
					CalculateContentWidth(const HTMLayoutProperties& props, ContainerWidthType width_type = INNER_CONTENT_WIDTH) const;

	/** Get height of border box. */

	virtual LayoutCoord
					GetHeight() const { return height; }

	/** Get minimum height (height if maximum width is satisfied) */

	virtual LayoutCoord
					GetMinHeight() const { return min_height; }

	/** Return the inline run-in at the beginning of the first line of the container, if present. */

	InlineRunInBox*	GetInlineRunIn() const;

	/** Grow height of content. */

	virtual void	GrowHeight(LayoutCoord extra) { height += extra; }

	/** Prepare the container for layout; calculate margin collapsing policy,
		CSS-specified height, and more.

		@param height (out) CSS actual height of the container, or 0 if the CSS computed
		height is auto
		@return FALSE on OOM, TRUE otherwise */

	BOOL			PrimeProperties(const HTMLayoutProperties& props, LayoutInfo& info, LayoutProperties* cascade, LayoutCoord& height);

	/** Get position to reflow into. */

	LayoutCoord		GetReflowPosition(BOOL inline_position = FALSE) const;

	/** Finish reflowing box.

		`close_anonymous_box' is set when an anonymous box would
		be closed. */

	LAYST			CloseVerticalLayout(LayoutInfo& info, CLOSED_BY closed_by);

	/** Re-layout content that overflowed first-line. */

	BOOL			EndFirstLine(LayoutInfo& info, LayoutProperties* child_cascade);

	/** Return line that is being reflowed.

		This method returns the current layout line that is being
		reflowed. */

	Line*			GetReflowLine() const { if (!reflow_state->forced_line_end && reflow_state->reflow_line && reflow_state->reflow_line->IsLine()) return (Line*) reflow_state->reflow_line; else return NULL; }

#ifdef LIMIT_PARAGRAPH_WIDTH_SCAN_FOR_DELIMITER_CHARACTERS
	/** Scan for menu delimiter character from here. If "|" or "::" is found, era text wrap should be inhibited.*/

	BOOL			ScanForMenuDelimiterCharacter(HTML_Element* start, HTML_Element* stop) const;
#endif

	/** Helper for GetNewLine to limit the paragraph width according
		to the current limit paragraph settings and tweaks.

		This algorithm will decide if the width should be limited the max paragraph width.

		If the width is limited, the new width will be returned in the width parameter.

		If the content is right or center aligned, width limit will also affect the line position
		through the line_x out parameter.

		If the limit is affected by a left float, and the text is left aligned, we may also decide
		that the new limit creates a too narrow line, and instead move the line down below the float.
		This new position is returned through the line_x and line_y parameters.

		@param info The layout info
		@param cascade Properties of the element corresponding to the container
		@param start_element The element that the new line will start with
		@param prev_vertical_layout The previous vertical layout in the container.
		@param line_height The line height of the line we are creating. Used as indata to calculate
			   new position relative to floats if the resulting width is too narrow.
		@param min_width Minimum width of content on the line we are about to create. Used as indata
			   to the float algorigthm. See also line_height.
		@param before_line_x The line x position before we adjusted the line for floats
		@param before_line_y The line y position before we adjusted the line for floats
		@param width (in/out) Input is the width of the line, output is the limited line width
		@param line_x (in/out) Input is the x position of the line. Output is the line_x after limit paragraph width
		@param line_y (in/out) Input is the y position of the line. Output is the line_y after limit paragraph width */

	void			 LimitParagraphWidth(const LayoutInfo& info,
										 LayoutProperties* cascade,
										 HTML_Element* start_element,
										 VerticalLayout* prev_vertical_layout,
										 LayoutCoord line_height,
										 LayoutCoord min_width,
										 LayoutCoord before_line_x,
										 LayoutCoord before_line_y,
										 LayoutCoord& width,
										 LayoutCoord& line_x,
										 LayoutCoord& line_y) const;

	/** Get new line to reflow onto.

		Returns TRUE when first-line is finished. */

	LAYST			GetNewLine(LayoutInfo& info, HTML_Element* element, LayoutCoord min_width);

	/** Insert line break into layout stack.

		Returns TRUE when first-line is finished. */

	LAYST			GetNewBreak(LayoutInfo& info, LayoutProperties* break_cascade);

	/** Get new block to reflow onto. */

	LAYST			GetNewBlockStage1(LayoutInfo& info, LayoutProperties* box_cascade, BlockBox* box);

	/** Get new block to reflow onto. */

	BOOL			GetNewBlockStage2(LayoutInfo& info, LayoutProperties* box_cascade, BlockBox* box);

	/** Get new floating block to reflow onto. */

	LAYST			GetNewFloatStage1(LayoutInfo& info, LayoutProperties* box_cascade, FloatingBox* floating_box);

	/** Get new floating block to reflow onto. */

	void			GetNewFloatStage2(LayoutInfo& info, LayoutProperties* box_cascade, FloatingBox* floating_box);

	/** Get new block for absolute positioned box. */

	LAYST			GetNewAbsolutePositionedBlock(LayoutInfo& info, LayoutProperties* box_cascade, AbsolutePositionedBox* abs_box);

	/** Get new column/pane-attached float. */

	LAYST			GetNewPaneFloat(LayoutInfo& info, LayoutProperties* box_cascade, FloatedPaneBox* box);

#ifdef PAGED_MEDIA_SUPPORT

	/** Search for trailing page breaks and skip to their pages. */

	BOOL			SkipPageBreaks(LayoutInfo& info);

	/** Insert page break. */

	BOOL			GetExplicitPageBreak(const LayoutInfo& info, BREAK_POLICY page_break);

	/** Attempt to break page. */

	virtual BREAKST	AttemptPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain);

#endif // PAGED_MEDIA_SUPPORT

	/** Return TRUE if we are to avoid page breaks inside this container. */

	BOOL			AvoidPageBreakInside() const { return packed.avoid_column_break_inside; }

	/** Return TRUE if we are to avoid column breaks inside this container. */

	BOOL			AvoidColumnBreakInside() const { return packed.avoid_column_break_inside; }

	/** Propagate a break point caused by break properties or a spanned element.

		These are discovered during layout, and propagated to the nearest
		multicol container when appropriate. They are needed by the columnizer
		to do column balancing.

		@param virtual_y Virtual Y position of the break (relative to the top
		border edge of this container)
		@param break_type Type of break point
		@param breakpoint If non-NULL, it will be set to the MultiColBreakpoint
		created, if any.
		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint);

	/** Get current static position.
		Used by absolute positioned boxes to guess their position.

		@param info Layout information structure
		@param element html element for the abs pos box
		@param inline_position indicates whether abspos was an inline element and
			   its static position depends on line it belongs to. */

	LayoutCoord		GetStaticPosition(const LayoutInfo& info, HTML_Element *element, BOOL inline_position) const;

	/** Calculate virtual X position to be used with a block that's about to be added.

		Some blocks may have to be traversed as part of a line, especially
		during logical traversal. This method will check if the new block is to
		be allowed to be traversed like that, and if so, calculate its virtual
		position.

		@param box_cascade The LayoutProperties entry for the new block

		@return The virtual position to use with a block, or LAYOUT_COORD_MIN
		if the block we're about to add shall always be traversed during
		regular container traversal. */

	LayoutCoord		CalculateBlockVirtualPosition(LayoutProperties* box_cascade) const;

	/** Collapse the current max negative and max positive margins with the provided
		top margin in order to find hypothetical static position of an auto positioned
		absolute positioned element, or an element with clearance. */

	LayoutCoord		CollapseHypotheticalMargins(LayoutCoord margin_top) const
	{
		if (margin_top >= 0)
			return MAX(margin_top, reflow_state->margin_state.max_positive) - reflow_state->margin_state.max_negative;
		else
			return reflow_state->margin_state.max_positive - MAX(-margin_top, reflow_state->margin_state.max_negative);
	}

	/** Does this container have 'auto' height */

	LayoutCoord		GetCSSHeight() const { return reflow_state->css_height; }

#ifdef SUPPORT_TEXT_DIRECTION

	/** Append bidi stretch.

	    Will update screen on previous lines that are affected. */

	BOOL           AppendBidiStretch(BidiCategory type, int width, HTML_Element* element, LayoutCoord virtual_position, const LayoutInfo* info);

	/** Initialise bidi calculation. */

	BOOL			InitBidiCalculation(const HTMLayoutProperties* props);

	/** Get bidi segments */

	const Head*		GetBidiSegments() const { return bidi_segments; }

#endif

	/** Allocate space on current line. */

	LAYST			AllocateLineSpace(HTML_Element* element,
									  LayoutInfo& info,
									  LayoutCoord box_width,
									  LayoutCoord min_width,
									  LayoutCoord max_width,
									  const InlineVerticalAlignment& valign,
									  LayoutCoord bbox_top,
									  LayoutCoord bbox_bottom,
									  LayoutCoord bbox_left,
									  LayoutCoord bbox_right,
									  LayoutCoord content_overflow_right,
									  LayoutCoord content_overflow_bottom,
									  LayoutCoord abspos_content_right,
									  LayoutCoord abspos_content_bottom,
									  UnicodePoint ch,
									  int break_property,
									  BOOL keep_min_widths);

	/** Allocate space for weak content on current line. */

	void			AllocateWeakContentSpace(HTML_Element* element, LayoutCoord additional_weak_space, BOOL normal_mode);

	/** Allocate non-content (eg. border) space on current line. */

	BOOL			AllocateNoncontentLineSpace(HTML_Element* element, LayoutCoord box_width, LayoutCoord min_width, LayoutCoord outline_width, const InlineVerticalAlignment& valign, LayoutCoord negative_margin_left, LayoutCoord negative_margin_right);

	/** Allocate content box propagated from absolutely positioned descendant. When line layout is finished
		properly translated content box will be propagated towards abspos containing block */

	void			AllocateAbsPosContentBox(const InlineVerticalAlignment& valign, unsigned short box_right, long box_bottom);

	/** Allocate "space" for a WBR element.

		Roughly speaking, WBR should cause the same effect as U+200B
		(ZERO-WIDTH SPACE). */

	LAYST			AllocateWbr(LayoutInfo& info, HTML_Element* html_element);

	/** Create an empty line. We need a line to reach positioned block boxes and
		(empty) inline boxes during line traversal. */

	LAYST			CreateEmptyLine(LayoutInfo& info);

	/** Create empty first line. */

	LAYST			CreateEmptyFirstLine(LayoutInfo& info, HTML_Element* element);

	/** Force end of line.

		Returns TRUE when first-line is finished. */

	LAYST			ForceLineEnd(LayoutInfo& info, HTML_Element* text_element, BOOL create_empty = TRUE);

	/** Return TRUE if the min/max widths of this content are up-to-date. */

	virtual BOOL	HasValidMinMaxWidths() const { return !!packed.minmax_calculated; }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Layout container and catch possible first-line end. */

	LAYST			LayoutWithFirstLine(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Find distance to next tab stop. */

	LayoutCoord		GetTabSpace();

	/** Allocate whitespace on current line. */

	BOOL			AllocateLineWhitespace(LayoutCoord whitespace, HTML_Element* element);

	/** If this element has a first-line pseudo-element and current line is first line. */

	BOOL			IsCssFirstLine() const { return reflow_state && reflow_state->is_css_first_line; }

	/** run-in or compact? */

	virtual BOOL	IsRunIn() const { return reflow_state && reflow_state->is_run_in; }

	/** Clear first line indicator. */

	void			ClearCssFirstLine(HTML_Element* element = NULL) { if (reflow_state) { reflow_state->is_css_first_line = FALSE; if (element) reflow_state->break_before_content = element; } }

	/** If this container has a first-letter pseudo-element. */

	BOOL			IsCssFirstLetter() const;

	/** Return the first-letter state of the this containers reflow-state. */

	unsigned short	HasCssFirstLetter() const { return reflow_state ? reflow_state->has_css_first_letter : (unsigned short)FIRST_LETTER_NO; }

	/** When we have added a first-letter pseudo element to a container, make sure we don't add more. */

	void			FirstLetterFinished() { if (reflow_state) reflow_state->has_css_first_letter = FIRST_LETTER_NO; }

	/** Check if this is a one line box that fit inside margin_width */

	BOOL			IsNarrowerThan(LayoutCoord margin_width) const;

	/** Get the numeric value to assign to the specified list item element.

		Will also update an internal counter, so that list item elements added
		that don't specify a value attribute will use and increment this value.

		@param list_item_element The list item element whose numbering we process.
		@return The number that list_item_element should use. */

	int				GetNewListItemValue(HTML_Element* list_item_element) { return reflow_state->list_item_state.GetNewValue(list_item_element); }

	/** Is this container a reversed list? */

	BOOL			IsReversedList() { return reflow_state->list_item_state.IsReversed(); }

	/** Get whether whitespace should be collapsed. */

	BOOL			GetCollapseWhitespace() const { return reflow_state->collapse_whitespace; }

	/** Get baseline of first or last line. */

	LayoutCoord		LocalGetBaseline(BOOL last) const;

	/** Get baseline of first or last line.

		@param last_line TRUE if last line baseline search (inline-block case).	*/

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const;

	/** Get baseline of inline block or content of inline element that
		has a baseline (e.g. ReplacedContent with baseline, BulletContent). */

	virtual LayoutCoord
					GetBaseline(const HTMLayoutProperties& props) const;

	/** Get the baseline if maximum width is satisfied. */

	virtual LayoutCoord
					GetMinBaseline(const HTMLayoutProperties& props) const;

#ifdef MEDIA_HTML_SUPPORT

	/** Get height of first line. */

	LayoutCoord		GetFirstLineHeight() const;

#endif // MEDIA_HTML_SUPPORT

	/** Is considered a containing element, e.g. container, table or flexbox? */

	virtual BOOL	IsContainingElement() const { return TRUE; }

	/** Propagate min/max widths. */

	virtual void	PropagateMinMaxWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width);

	/** Propagate min/max widths from in-flow child blocks. */

	void			PropagateBlockWidths(const LayoutInfo& info, LayoutProperties* box_cascade, BlockBox* block_box, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width);

	/** Propagate min/max widths for floating boxes. */

	void			PropagateFloatWidths(const LayoutInfo& info, LayoutProperties* box_cascade, FloatingBox* floating_box, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width);

	/** Does child content need to propagate min/max widths to this container? */

	BOOL			IsCalculatingMinMaxWidths() const { return reflow_state->calculate_min_max_widths; }

	/** Return TRUE if this container, or an ancestor container in the same STF container as this container, has a fixed height specified. */

	BOOL			IsInsideFixedHeight() const { return reflow_state->inside_fixed_height; }

	/** Set pending run-in.

		A pending run-in may be "stolen" by a sibling container, if the criterias for a
		run-in to become an inline box were satisfied. */

	void			SetPendingRunIn(Content_Box* run_in, LayoutInfo& info, LayoutCoord border_padding_margin);

	/** Is there a pending run-in?

		Cannot skip reflow of unchanged blocks if there is. */

	BOOL			HasPendingRunIn() const { return reflow_state->pending_run_in_box != NULL; }

	/** Get pending run-in.

		This will cause a currently pending run-in (if any) to no longer be pending. */

	Content_Box*	StealPendingRunIn(LayoutInfo& info, LayoutCoord& pending_compact_space);

	/** Calculate bottom margins of layout element, by collapsing with adjoining child margins.
	 * This is only done when skipping layout of a box.
	 * @param cascade properties of this element
	 * @param info Layout information structure
	 * @param bottom_margin Margin state
	 * @param include_this If FALSE, omit margins specified on this element from the calculation
	 * and just collapse adjacent child margins.
	 * @return OpBoolean::IS_FALSE if top and bottom margins of this element are adjoining,
	 * OpBoolean::IS_TRUE otherwise, unless an error occurred.
	 */

	virtual OP_BOOLEAN
					CalculateBottomMargins(LayoutProperties* cascade, LayoutInfo& info, VerticalMargin* bottom_margin, BOOL include_this = TRUE) const;

	/** Calculate top margins of layout element, by collapsing with adjoining child margins.
	 * This is only done when skipping layout of a box.
	 * @param cascade properties of this element
	 * @param info Layout information structure
	 * @param top_margin Margin state
	 * @param include_self_bottom If FALSE, don't collapse top with bottom margin
	 * specified on this element, even if they are adjoining.
	 * @return OpBoolean::IS_FALSE if top and bottom margins of this element are adjoining,
	 * OpBoolean::IS_TRUE otherwise, unless an error occurred.
	 */

	virtual OP_BOOLEAN
					CalculateTopMargins(LayoutProperties* cascade, LayoutInfo& info, VerticalMargin* top_margin, BOOL include_self_bottom) const;

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Update relative position used when calculating bounding box area. */

	void			SetRelativePosition(LayoutCoord x, LayoutCoord y) { reflow_state->relative_x += x; reflow_state->relative_y += y; }

	/** Get the currently accumulated relative position offsets for positioned inlines in this container. */

	void			GetRelativePosition(LayoutCoord& x, LayoutCoord& y) { x = reflow_state->relative_x; y = reflow_state->relative_y; }

#ifdef CSS_TRANSFORMS

	/** Update transforms used when calculating bounding box area */

	void			SetTransform(const AffineTransform &transform, AffineTransform *&previous_transform);

	/** Reset the transform to a previous state */

	void			ResetTransform(AffineTransform *transform);

#endif

	/** Is there any in-flow content? */

	virtual BOOL	IsEmpty() const;

	/** Is the specified layout stack the layout stack in this container? */

	BOOL			HasLayoutStack(Head* layout_stack) const { return layout_stack == &this->layout_stack; }

	/** Get current position that we are laying out on. */

	LayoutCoord		GetLinePendingPosition() const { return reflow_state->GetLineCurrentPosition() + reflow_state->GetPendingContent(); }

	/** Get line that includes given virtual position. */

	Line*			GetLineAtVirtualPosition(long pos);

	/** Commit pending line content.

		Returns LAYOUT_END_FIRST_LINE when first-line is overflowed. */

	LAYST			CommitLineContent(LayoutInfo& info, BOOL commit_all, BOOL keep_min_widths = FALSE);

	/** Set base character from last text box. */

	void			SetLastBaseCharacter(UnicodePoint last_base_character) { if (reflow_state) reflow_state->last_base_character = last_base_character; }

	/** Set last base character depending on the container's white-space value.
		It will be such that a line break will be allowed afterwards in case
		of white-space normal and pre_wrap, disallowed otherwise.

		@param white_space The white-space property of the container's element. */

	void			SetLastBaseCharacterFromWhiteSpace(const CSSValue white_space);

	/** Check if we can break line between this and last base character. */

	LinebreakOpportunity
					GetLinebreakOpportunity(UnicodePoint base_character);

	/** Return the maximum word width to fit at the end of current line if line
		can be broken, or the size of a new line minus uncommitted content. */

	LayoutCoord		GetMaxWordWidth(LinebreakOpportunity break_property);

	/** Previous letter should allow aggressive word break (ERA, awb) */

	BOOL			GetLastLetterAllowAggressiveBreakAfter() { return reflow_state && reflow_state->last_letter_allow_aggressive_break_after; }

	/** Set that previous letter should allow aggressive word break (ERA, awb) */

	void			SetLastLetterAllowAggressiveBreakAfter(BOOL set) { if (reflow_state) reflow_state->last_letter_allow_aggressive_break_after = set; }

	/** Get start element of current line. */

	HTML_Element*	GetStartElementOfCurrentLine() const;

#ifdef SUPPORT_TEXT_DIRECTION

	/** Push new inline bidi properties onto stack.
	 *
	 * @return FALSE om OOM
	 */

	BOOL			PushBidiProperties(LayoutInfo& info, BOOL ltr, BOOL override);

	/** Pop inline bidi properties from stack.
	 *
	 * @return FALSE on OOM
	 */

	BOOL			PopBidiProperties();

#endif

	/** Notify container that an image has got its size reduced. */

	void			ImageSizeReduced();

	/** Is next content placed in the beginning of a line? */

	BOOL			IsStartOfNewLine() const { return reflow_state->GetPendingContent() == 0 && (!GetReflowLine() || ((Line*) reflow_state->reflow_line)->GetUsedSpace() == 0); }

	/** Get the percent of normal width when page is shrunk in ERA. */

	virtual int		GetPercentOfNormalWidth() const;

	BOOL			IgnoreSSR_BR(HTML_Element* br_element) const;

	/** Is this a True Table candidate? */

	virtual TRUE_TABLE_CANDIDATE
					IsTrueTableCandidate() const { return (TRUE_TABLE_CANDIDATE) packed.true_table_candidate; }

	/** Does width propagation from this container affect FlexRoot? */

	BOOL			AffectsFlexRoot() const { return reflow_state && reflow_state->affects_flexroot; }

	/** Is this a marquee with horizontal content motion? */

	virtual BOOL	IsHorizontalMarquee() const { return FALSE; }

	/** This container has content that is affected by height changes in an ancestor.

		This container itself, on the other hand, is unaffected by these height
		changes, which means that they will be impossible to anticipate by
		ComputeSize() during reflow, so the container needs to be marked as
		unskippable. */

	void			SetHeightAffectsChildren() { packed.unskippable = 1; }

	/**
	 * Move content in the layout stack to center it vertically.
	 *
	 * @param info The layout info, provides VisualDevice.
	 * @param props Properties, information about padding and borders is required.
	 * @param content_height Actual height of the content
	 */

	void			CenterContentVertically(const LayoutInfo& info, const HTMLayoutProperties& props, LayoutCoord content_height);

	/** Get the distance from the top/left border edge of this block formatting
		context to the top/left border edge of this container. (Note that this
		means that if this box itself establishes a new block formatting
		context, the values will be 0).

		All three values must be 0 when calling this method. */

	void			GetBfcOffsets(LayoutCoord& bfc_x, LayoutCoord& bfc_y, LayoutCoord& bfc_min_y) const;

	/** Get new space from the specified space manager.

		The space manager will find a suitable position for content defined by
		the rectangle (x, y, min_width, min_height) that does not overlap with
		any of the floats' (in this block formatting context) margin boxes.

		It may not always be possible to satisfy the space requirements (if the
		containing block is too narrow). However, no matter what, the position
		returned (x, y) will never overlap with the margin edge of any floats
		in this block formatting context.

		@param space_manager The space manager from which to get space. This
		should always be the same space_manager as established by this
		container, or the nearest ancestor container that does.

		@param y (in/out) Relative to the top border edge of this
		container. In: highest possible Y position for the element. Out: The
		in-value will be increased if that is needed to satisfy the space
		requirements. If it is below all the floats, it will not be increased
		any further (even if the space requirement cannot be satisfied at this
		position).

		@param x (in/out) Relative to the left border edge of this
		container. In: leftmost possible X position for the element. Out: Will
		be changed if putting an element at that position would overlap with
		any floats.

		@param width (in/out) In: containing block width (ensures that space
		handed out is confined to the containing block). Out: width available
		at the final position.

		@param min_width Minimum width requirement

		@param min_height Minimum height requirement

		@param lock_vertical If TRUE, the vertical position (@see y) will
		remain untouched, even if that would mean that the layout element will
		overlap with floats. The horizontal position may be affected by floats,
		though.

		@return How much vertical space is available at the final position. */

	LayoutCoord		GetSpace(SpaceManager* space_manager, LayoutCoord& y, LayoutCoord& x, LayoutCoord& width, LayoutCoord min_width, LayoutCoord min_height, VerticalLock vertical_lock = VLOCK_OFF) const;

	/** Get maximum width of all floats at the specified minimum Y position, and change minimum Y if necessary.

		This method is similar to GetSpace(), just that it is only used when
		calculating min/max widths. It will pretend that maximum widths are
		satisfied and try to find space among the floats for content at the Y
		position bfc_y with height min_height.

		The caller suggests a Y position for some content and wants to know the
		maximum widths of floats that are beside the content. This method may
		change the Y position if it finds that there is no way the content
		would fit there, and then provide the maximum width of floats to the
		left and to the right of the content at its potentially new position.

		@param space_manager The space manager to examine. This should always
		be the same space_manager as established by this container, or the
		nearest ancestor container that does.

		@param min_y (in/out) Minimum Y position (the position that content
		would get if the maximum width of its container is satisfied), relative
		to the top border edge of this container. In: Highest (suggested)
		minimum Y position for the content. Out: Same or lower minimum Y
		position, where there might actually be room for the content.

		@param min_height Minimum height - height that the content would get if
		the maximum width of its container is satisfied.

		@param max_width Maximum width of content (the width that the content
		would need in order to avoid implicit line breaks and floats being
		shifted downwards)

		@param left_floats_max_width (out) The maximum width by which floats to
		the left of the content may possibly overlap the content's container.

		@param right_floats_max_width (out) The maximum width by which floats
		to the right of the content may possibly overlap the content's
		container. */

	void			GetFloatsMaxWidth(SpaceManager* space_manager, LayoutCoord& min_y, LayoutCoord min_height, LayoutCoord max_width, LayoutCoord& left_floats_max_width, LayoutCoord& right_floats_max_width);

	/** Get the border area (optionally) and translation for 'box'.

		This will convert the box's internal ("virtual") dimensions to the
		visual representation caused by (multiple) pages or columns.

		@param box The box of which we're interested in the position and
		(optionally) border area.
		@param x_translation (out) Set to the X translation of the first column
		or page in which 'box' lives.
		@param y_translation (out) Set to the Y translation of the first column
		or page in which 'box' lives.
		@param border_rect (out) If non-NULL, set to the union border rectangle
		of all the pages or columns in which 'box' lives. The values are
		relative to the top/left border edge of the target box (in the first
		page or column in which the box lives). Note that these values are
		presented as if the box width is 0 (so to get the correct border box
		width, do border_rect.right - border_rect.left + box->GetWidth()). */

	virtual void	GetAreaAndPaneTranslation(const Box* box, LayoutCoord& x_translation, LayoutCoord& y_translation, RECT* border_rect = NULL, ColumnFinder* ancestor_cf = NULL) { OP_ASSERT(!"This method is only meant to be called on multi-pane containers."); }

	/** Add statically positioned GCPM float. */

	virtual void	AddStaticPaneFloat(FloatedPaneBox* box) { OP_ASSERT(!"This method is only meant to be called on multi-pane containers."); }

	/** Skip over GCPM floats that are in the subtree established by the specified descendant.

		Happens when the layout engine skips layout of a subtree (optimization). */

	virtual void	SkipPaneFloats(HTML_Element* descendant) { OP_ASSERT(!"This method is only meant to be called on multi-pane containers."); }

	/** Return TRUE if this element can be split into multiple outer columns or pages.

		In general, descendant content of a paged or multicol container is
		columnizable, but this is not the case if the content is or is inside
		of a container with scrollbars, button, replaced content, etc. */

	virtual BOOL	IsColumnizable() const;

	/** Distribute content into columns.

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	Columnize(Columnizer& columnizer, Container* container);

	/** Figure out to which column(s) or spanned element a box belongs. */

	virtual void	FindColumn(ColumnFinder& cf);

	/** Change the used height if computed height was auto.

		@return TRUE if the height change was allowed (and done), FALSE if it
		was rejected. */

	BOOL			ForceHeightChange(LayoutCoord height_increase);

	/** Sets or clears the flexible marker in this container.
		During the reflow, it is illegal to call this method with value
		being TRUE, after it was once called with value being FALSE.

		@param value TRUE - set the marker to be flexible
					 (FLEXIBLE_MARKER_NOT_ALIGNED on)
					 FALSE - set the marker to be already aligned
					 (FLEXIBLE_MARKER_NOT_ALIGNED off)
					 @see ListItemMarkerStateFlags. */

	void			SetHasFlexibleMarker(BOOL value)
					{
						if (value)
						{
							/* Full setting, should also reset the FLEXIBLE_MARKER_ALIGNED_TO_SINGLE_OUTSIDE
							   flag, but we don't need this at the moment. So currently we skip it. */

							reflow_state->list_item_marker_state |= FLEXIBLE_MARKER_NOT_ALIGNED;
						}
						else
							reflow_state->list_item_marker_state &= ~FLEXIBLE_MARKER_NOT_ALIGNED;
					}

	/** TRUE when either this container has a flexible marker or some ancestor has. */

	BOOL			HasFlexibleMarkers() const
					{
						return !!(reflow_state->list_item_marker_state & (FLEXIBLE_MARKER_NOT_ALIGNED | ANCESTOR_HAS_FLEXIBLE_MARKER));
					}

	/** Find the outer multi-pane container of the specified element.

		This is the topmost ancestor multi-pane container that affects the
		element. It is the topmost ancestor multi-pane container in which this
		element "lives". There may be other ancestor multi-pane containers
		above this topmost ancestor container, but they do not affect the
		specified element, e.g. because there is an absolutely positioned box,
		float or table row inbetween. */

	static Container*
					FindOuterMultiPaneContainerOf(HTML_Element* element);

	/** @return TRUE if containing block height already has been calculated. */

	BOOL			IsContainingBlockHeightCalculated() { return !reflow_state || reflow_state->height_known; }

};

class BlockContainer
  : public Container
{
protected:

	/** Width including border and padding. */

	LayoutCoord		block_width;

public:

					BlockContainer() : block_width(-1) {}

	/** Compute size of content and return IS_TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Get width of border box. */

	virtual LayoutCoord
					GetWidth() const { return block_width; }

#ifdef MEDIA_HTML_SUPPORT

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

#endif // MEDIA_HTML_SUPPORT

	/** Is height relative? */

	virtual BOOL	HeightIsRelative() const { return packed.relative_height == 1; }
};

/** Shrink-to-fit container, using the min/max width propagation algorithm. */

class ShrinkToFitContainer
 : public BlockContainer
{
private:
	union
	{
		struct
		{
			/** Cached value of min-width property (but converted to border-box value).

				If min-width is percentual, it will be set to 0. */

			signed int
					nonpercent_min_width_prop:31;

			/** Set to true when min/max values have changed and another reflow has been requested. */

			unsigned int
					minmax_changed:1;

		} stf_packed;
		UINT32		stf_packed_init;
	};

	/** Calculate and return width, based on current min/max values and constraints. */

	LayoutCoord		CalculateHorizontalProps(LayoutProperties* cascade, LayoutInfo& info, BOOL use_all_available_width);

protected:

	/** Apply min-width property constraints on min/max values. */

	virtual void	ApplyMinWidthProperty(const HTMLayoutProperties& props, LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width);

	/** Get min/max width. */

	virtual BOOL	GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const;

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth();

public:
					ShrinkToFitContainer()
						: stf_packed_init(0) { }

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Compute size of content and return IS_TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Do we need to calculate min/max widths of this box's content? */

	virtual BOOL	NeedMinMaxWidthCalculation(LayoutProperties* cascade) const;

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

	/** Is this content shrink-to-fit?

		Return TRUE if it is shrink-to-fit, i.e. that it calculates its width
		based on min/max width propagation fromt its child.
		ShrinkToFitContainer is one such type. */

	virtual BOOL	IsShrinkToFit() const { return TRUE; }

	/** Propagate min/max widths. */

	virtual void	PropagateMinMaxWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width);
};

class ButtonContainer
	: public ShrinkToFitContainer,
	  public CachedPosition
{
private:

	OpButton*		button;

	union
	{
		struct
		{
			/** Is the width auto? */

			unsigned int
					width_auto:1;
		} button_packed;

		UINT32		button_packed_init;
	};

	/** Create and set up the OpButton object, if none exists. */

	OP_STATUS		CreateButton(VisualDevice* vis_dev);

	/** Destroy the OpButton object, if it exists. */

	void			DestroyButton();

public:
					ButtonContainer()
					  : button(NULL) { button_packed_init = 0; }
	virtual			~ButtonContainer();


	void SetButtonFocus(HTML_Element* helm, FOCUS_REASON reason) {
		if (NULL != button)
			button->SetFocus(reason);
	}

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth();

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Compute size of content and return IS_TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Cast this object to ButtonContainer, if it is one. */

	virtual ButtonContainer*
					GetButtonContainer() { return this; }

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Updates the position. Must be correct when user scrolls. */

	void			UpdateWidgetPosition();

	/** Paint the button */

	void			PaintButton(VisualDevice* visual_device, const HTMLayoutProperties& props);

	/** Create form, plugin and iframe objects */

	virtual OP_STATUS
					Enable(FramesDocument* doc);

	/** Remove form, plugin and iframe objects */

	virtual void	Disable(FramesDocument* doc);

	/** Returns the current horizontal offset */

	LayoutCoord		GetOfsX() const;

	/** Returns the current vertical offset */

	LayoutCoord		GetOfsY() const;

	/** Return the OpWidget representation of the button. */

	OpButton*		GetButton() const { return button; }

#ifdef PAGED_MEDIA_SUPPORT

	/** Cant allow break inside button mainly because of the magical vertical centering. */

	virtual BREAKST	AttemptPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain) { return BREAK_KEEP_LOOKING; }
#endif // PAGED_MEDIA_SUPPORT

	virtual void	UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height);

	/** Is this content shrink-to-fit?

		Return TRUE if it is shrink-to-fit, i.e. that it calculates its width
		based on min/max width propagation fromt its child.
		ShrinkToFitContainer is one such type. */

	virtual BOOL	IsShrinkToFit() const { return !!button_packed.width_auto; }

	/** Return TRUE if this element can be split into multiple outer columns or pages.

		In general, descendant content of a paged or multicol container is
		columnizable, but this is not the case if the content is or is inside
		of a container with scrollbars, button, replaced content, etc. */

	virtual BOOL	IsColumnizable() const;

	/** Handle event that occurred on this element.
	 *
	 * @param doc The document
	 * @param ev The type of event
	 * @param offset_x horizontal offset of the event relative to the content edge of the box,
	 *                 in the box's coordinate system.
	 * @param offset_y vertical offset of the event relative to the content edge of the box,
	 *                 in the box's coordinate system.
	 */

	virtual void	HandleEvent(FramesDocument* doc, DOM_EventType ev, int offset_x, int offset_y);
};

class MarqueeContainer
  : public ShrinkToFitContainer,
    public MessageObject,
	public CachedPosition
{
private:

	/** Real content size (width or height, depending on packed3.vertical. */

	LayoutCoord		real_content_size;

	double			nextUpdate;

	union
	{
		struct
		{
			/** Pending message? */

			unsigned int
					pending_message:1;

			/** Vertical marquee? */

			unsigned int
					vertical:1;

			/** Is a real shrink-to-fit in the box model (e.g. a float or absolutely positioned box with auto width)? */

			unsigned int
					is_shrink_to_fit:1;

			/** Set to true when a new containing block width was calculated during the last reflow pass. */

			unsigned int
					containing_block_width_changed:1;

			/** Set to TRUE when the containing block width is up-to-date. */

			unsigned int
					containing_block_width_calculated:1;
		} packed3;
		UINT32		packed3_init;
	};

public:

					MarqueeContainer();
	virtual			~MarqueeContainer();

	/** Do we need to calculate min/max widths of this box's content? */

	virtual BOOL	NeedMinMaxWidthCalculation(LayoutProperties* cascade) const;

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth();

	/** Propagate min/max widths. */

	virtual void	PropagateMinMaxWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Compute size of content and return IS_TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Handle message. */

	void			HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Update height of container. */

	virtual void	UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height);

	/** Animate marquee. */

	OP_STATUS		Animate(PaintObject* paint_object, LayoutProperties* layout_props);

	/** Is this a marquee with horizontal content motion? */

	virtual BOOL	IsHorizontalMarquee() const { return !packed3.vertical; }

	/** Calculate the width of the containing block established by this container. */

	virtual LayoutCoord
					CalculateContentWidth(const HTMLayoutProperties& props, ContainerWidthType width_type = INNER_CONTENT_WIDTH) const;

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

	/** Is this content shrink-to-fit?

		Return TRUE if it is shrink-to-fit, i.e. that it calculates its width
		based on min/max width propagation fromt its child.
		ShrinkToFitContainer is one such type. */

	virtual BOOL	IsShrinkToFit() const { return packed3.is_shrink_to_fit; }

	/** Return TRUE if this element can be split into multiple outer columns or pages.

		In general, descendant content of a paged or multicol container is
		columnizable, but this is not the case if the content is or is inside
		of a container with scrollbars, button, replaced content, etc. */

	virtual BOOL	IsColumnizable() const;
};

/** Container for the root element, in continuous media.

	@see RootPagedContainer for paged media. */

class RootContainer
  : public ShrinkToFitContainer
{
private:

	/** Is the width flexible (shrink-to-fit)? */

	BOOL			flex_root;

protected:

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth();

	/** Update height of container. */

	virtual void	UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height);

public:

					RootContainer()
					  : flex_root(FALSE) {}

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Compute size of content and return IS_TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Propagate the right edge of absolute positioned boxes. */

	virtual void	PropagateRightEdge(const LayoutInfo& info, LayoutCoord right, LayoutCoord noncontent, LayoutFixed percentage);

#ifdef PAGED_MEDIA_SUPPORT

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

#endif // PAGED_MEDIA_SUPPORT

	/** Is this content shrink-to-fit?

		Return TRUE if it is shrink-to-fit, i.e. that it calculates its width
		based on min/max width propagation fromt its child.
		ShrinkToFitContainer is one such type. */

	virtual BOOL	IsShrinkToFit() const { return flex_root; }

	/** Return TRUE if the actual value of 'overflow' is 'visible'. */

	virtual BOOL	IsOverflowVisible() { return FALSE; }
};

/** SavedScrollPos is a complex HTML element atribute that keeps
	ScrollableContainer's scrollbars positions after container has been deleted.
	It is used to preserve scroll positions when ScrollableContainer needs to be re-created.
	When ScrollableContainer starts its Layout, it checks if there is a SavedScrollPos saved by
	previous container's instance, restores it and tries to reapply previous scroll positions. */

class SavedScrollPos
	: public ComplexAttr
{
public:
					SavedScrollPos()
					  : valid(FALSE) {}

	/** Horizontal scrollbar position */

	LayoutCoord		x;

	/** Vertical scrollbar position */

	LayoutCoord		y;

	/** TRUE if x and y contain valid values, FALSE otherwise */

	BOOL			valid;

	/** Do not let this attribute be moved along with HTML element between documents */

	virtual BOOL	MoveToOtherDocument(FramesDocument *old_document, FramesDocument *new_document) { return FALSE; }
};

/** Container with non-visible overflow.

	Able to display horizontal and/or vertical scrollbars when needed/requested. */

class ScrollableContainer
  : public ShrinkToFitContainer,
	public ScrollableArea
{
private:

	union
	{
		struct
		{
			/** Is this a shrink-to-fit scrollable container? */

			unsigned int
					is_shrink_to_fit:1;

		} packed3;
		UINT32		packed3_init;
	};

protected:

	/** Update height of container. */

	virtual void	UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height);

	/** Apply min-width property constraints on min/max values. */

	virtual void	ApplyMinWidthProperty(const HTMLayoutProperties& props, LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width);

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth();

public:

					ScrollableContainer() :
                        packed3_init(0) {}

	virtual			~ScrollableContainer();

	/** Get the layout box that owns this ScrollableArea. */

	virtual Content_Box*
					GetOwningBox() { return placeholder; }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Compute size of content and return IS_TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Get baseline of first line or return this' height if searching for
		an ancestor inline-block baseline (last_line being TRUE). */

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const;

	/** Get baseline of inline block or content of inline element that
		has a baseline (e.g. ReplacedContent with baseline, BulletContent). */

	virtual LayoutCoord
					GetBaseline(const HTMLayoutProperties& props) const;

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Update margins, bounding box and start position for the next element in the layout stack.
		Children of this container call this method to set new bottom margin values to use in
		margin collapsing in which successive content in the layout stack participates. */

	virtual void	UpdateBottomMargins(const LayoutInfo& info, const VerticalMargin* bottom_margin, AbsoluteBoundingBox* child_bounding_box = NULL, BOOL has_inflow_content = TRUE);

	/** Expand relevant parent containers and their bounding-boxes to contain floating and
		absolutely positioned boxes. A call to this method is only to be initiated by floating or
		absolutely positioned boxes.

		@param info Layout information structure
		@param bottom Only relevant for floats: The bottom margin edge of the float, relative to
		top border-edge of this box.
		@param min_bottom Only relevant for floats: The bottom margin edge of the float if
		maximum width of its container is satisfied
		@param child_bounding_box Bounding-box of the propagating descendant, joined with the
		bounding-boxes of the elements up the parent chain towards this element. Relative to the
		top border-edge of this box.
		@param opts Bounding box propagation options */

	virtual void	PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts);

	/** Calculate the width of the containing block established by this container. */

	virtual LayoutCoord
					CalculateContentWidth(const HTMLayoutProperties& props, ContainerWidthType width_type = INNER_CONTENT_WIDTH) const;

	/** Remove form, plugin and iframe objects */

	virtual void	Disable(FramesDocument* doc);

	/** Create form, plugin and iframe objects */

	virtual OP_STATUS
					Enable(FramesDocument* doc);

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Cast this object to ScrollableContainer, if it is one. */

	virtual ScrollableContainer*
					GetScrollableContainer() { return this; }

	/** Cast this object to ScrollableArea, if it is one. */

	virtual ScrollableArea*
					GetScrollable() { return this; }

	/** Is there typically a CoreView associated with this content ?

		@see class CoreView */

	virtual BOOL	HasCoreView() const { return TRUE; }

#ifdef SELFTEST

	/** Get a pointer to the vertical scrollbar. */

	OpScrollbar*	GetVerticalScrollbar() { return vertical_scrollbar; }

	/** Get a pointer to the horizontal scrollbar. */

	OpScrollbar*	GetHorizontalScrollbar() { return horizontal_scrollbar; }

#endif

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

	// BEGIN - implementation of OpInputContext
	virtual const char* GetInputContextName() { return "Scrollable Container"; }
	// END - implementation of OpInputContext

	/** Is this content shrink-to-fit?

		Return TRUE if it is shrink-to-fit, i.e. that it calculates its width
		based on min/max width propagation fromt its child.
		ShrinkToFitContainer is one such type. */

	virtual BOOL	IsShrinkToFit() const { return !!packed3.is_shrink_to_fit; }

	/** Get scroll offset, if applicable for this type of box / content. */

	virtual OpPoint	GetScrollOffset();

	/** Set scroll offset, if applicable for this type of box / content.

		@param x If non-NULL, the new X position. If NULL, leave current X position as is.
		@param y If non-NULL, the new Y position. If NULL, leave current Y position as is. */

	virtual void	SetScrollOffset(int* x, int* y);

	/** Return TRUE if this element can be split into multiple outer columns or pages.

		In general, descendant content of a paged or multicol container is
		columnizable, but this is not the case if the content is or is inside
		of a container with scrollbars, button, replaced content, etc. */

	virtual BOOL	IsColumnizable() const;

	/** Handle event that occurred on this element.
	 *
	 * @param doc The document
	 * @param ev The type of event
	 * @param offset_x horizontal offset of the event relative to the content edge of the box,
	 *                 in the box's coordinate system.
	 * @param offset_y vertical offset of the event relative to the content edge of the box,
	 *                 in the box's coordinate system.
	 */

	virtual void	HandleEvent(FramesDocument* doc, DOM_EventType ev, int offset_x, int offset_y);

	/** Return the OpInputContext of this container, if it establishes one. */

	virtual OpInputContext*
					GetInputContext() { return this; }

	/** Return TRUE if the actual value of 'overflow' is 'visible'. */

	virtual BOOL	IsOverflowVisible() { return GetScrollableContainer() == NULL; }
};

/** A container with multiple columns.

	Referred to as "multi-column container" or "multicol container" from
	documentation.

	A MultiColumnContainer will lay out its content in pretty much the same way
	as any other container does it. This means that, from a multicol
	perspective, everything is actually laid out in one column; the main layout
	phase doesn't care much about multiple columns. At the beginning of layout
	of a multi-column container, the column count and width is calculated. The
	column width is provided to child content through the method
	GetContentWidth(). Spanned child elements will need to use the full
	container width, though (by calling GetOuterContentWidth()).

	When finishing layout of a multi-column container, we enter a so-called
	columnization phase, where the container is distributed into
	columns. However, if the multicol container lives inside of another
	multicol container, the columnization phase is delayed; instead of doing it
	right away when finishing its layout, it will instead take place in the
	columnization phase of the ancestor multicol container.

	Columnization means distributing content into columns. There will be one
	Column object created for each column. A ColumnRow object will be created
	to contain the Column objects that together form a row of columns. The
	Column object knows the translation of the column it represents, and this
	is what causes the multi-column effect during painting and other
	traversals. Internally the vertical layout stack of the Container will
	remain like a regular one, with each line, block or line break following
	the next, vertically. Consequently, the vertical position of each vertical
	layout element will be as if the container only had one column. The
	horizontal positions will be as if all elements were in the first
	column. And in reality they are all in the first column and there is only
	one column, like mentioned earlier. It's just that we have Column objects
	with their translations, and that's what causes the multi-column effect.

	This class needs to support many kinds of box configurations, such as
	regular block boxes, inline-blocks, boxes with non-visible overflow,
	shrink-to-fit content, table-cells, and so on. Therefore it inherits
	ScrollableContainer, but will lose its ScrollableContainer behavior if
	overflow is visible. ScrollableContainer inherits ShrinkToFitContainer, and
	is on its own able to lose its ShrinkToFitContainer behavior if the content
	isn't shrink-to-fit. Finally, MultiColumnContainer is able to lose its
	BlockContainer behavior, if it's representing a table-cell. */

class MultiColumnContainer :
	public ScrollableContainer
{
private:

	/** List of ColumnRow elements.

		This is the main product of the columnization phase (which takes place
		when finishing layout of the multicol container).

		It is regenerated each time the multicol container is reflowed. It is
		cleared when Layout() is called, and normally built when UpdateHeight()
		is called (which will take care of columnization). If the multicol
		container is a descendant of another multicol container, building takes
		place later (during columnization of the ancestor multicol
		container). */

	ColumnRowStack	rows;

	/** List of all SpannedElm objects belonging to this multicol container.

		Populated during layout, read during columnization. */

	AutoDeleteHead	spanned_elms;

	/** Column and row (and page) breaks.

		A list of MultiColBreakpoint objects. This is used for column
		balancing. They are created and inserted during layout.

		Note: To get this 100% accurate, we should consider calculating the
		virtual Y positions when finishing layout of the multicol container
		(instead of doing it when we encounter them during layout), since
		elements may be moved up or down by successive content.

		Populated during layout, read during columnization. */

	AutoDeleteHead	breakpoints;

	/** Height of content if it were to be put in one column. */

	LayoutCoord		virtual_height;

	/** Total virtual height of all pane-attached floats preceding the first explicit break. */

	LayoutCoord		floats_virtual_height;

	/** Used value of min-height property. Content-box value. */

	LayoutCoord		min_height;

	/** Maximum height. Based on height/min-height/max-height properties. Content-box value. */

	LayoutCoord		max_height;

	/** Initial content box height (minus scrollbars, if any). */

	LayoutCoord		initial_height;

	/** Total number of columns, as resolved from CSS properties. */

	short			column_count;

	/** Column width, as resolved from CSS properties. */

	LayoutCoord		column_width;

	/** Content box width. */

	LayoutCoord		content_width;

	/** Gap between columns, as resolved from CSS properties. */

	LayoutCoord		column_gap;

	union
	{
		struct
		{
			/** Is overflow different from 'visible', i.e. is it a real ScrollableContainer? */

			unsigned int
					is_scrollable:1;

			/** Is a real shrink-to-fit in the box model (e.g. a float or absolutely positioned box with auto width)? */

			unsigned int
					is_shrink_to_fit:1;

			/** Is this a table-cell? */

			unsigned int
					is_table_cell:1;

			/** Balance columns? */

			unsigned int
					balance:1;

#ifdef SUPPORT_TEXT_DIRECTION

			/** Text direction is right-to-left? */

			unsigned int
					rtl:1;

#endif // SUPPORT_TEXT_DIRECTION

			/** Is height auto? */

			unsigned int
					auto_height:1;

			/** Is max-height different from 'none'? */

			unsigned int
					has_max_height:1;

		} multicol_packed;
		UINT32		multicol_packed_init;
	};

protected:

	/** List of floats in this multicol container, sorted by document tree order.

		Only statically-positioned floats will be added. Relative positioned
		floats will belong to some stacking context and traversed from
		there. */

	PaneFloatList	logical_floats;

	/** List of floats that were in logical_floats prior to layout but haven't yet been put back into the list. */

	PaneFloatList	pending_logical_floats; // Note: could be part of some reflow state.

	/** Sum of top border and padding. */

	LayoutCoord		top_border_padding;

	/** Sum of bottom border and padding. */

	LayoutCoord		bottom_border_padding;

	/** Create and run the columnizer for this multicol container.

		This will create and add rows and columns for all content that has been
		laid out but not yet columnized.

		@param ancestor_columnizer Columnizer of ancestor multicol container,
			   if any
		@param info Layout info
		@param content_height (out) Set to the content-height of the multicol
			   container
		@param max_height Maximum height. Based on height/min-height/max-height
			   properties. Content-box value.
		@param ancestor_columnizer_here TRUE if ancestor_columnizer is not NULL and
			   distributes this container also.
		@param trial TRUE if this is an inner multicol container, and we're
			   just trying to guess its height.

		@return FALSE on OOM, TRUE otherwise. */

	BOOL			RunColumnizer(Columnizer* ancestor_columnizer,
								  const LayoutInfo& info,
								  LayoutCoord& content_height,
								  LayoutCoord max_height,
								  BOOL ancestor_columnizer_here,
								  BOOL trial);

	/** Prepare for columnization. */

	void			PrepareColumnization(const LayoutInfo& info, const HTMLayoutProperties& props, LayoutCoord height);

	/** Update height of container. */

	virtual void	UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height);

	/** Apply min-width property constraints on min/max values. */

	virtual void	ApplyMinWidthProperty(const HTMLayoutProperties& props, LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width);

public:

					MultiColumnContainer();

	/** Get width of border box. */

	virtual LayoutCoord
					GetWidth() const { return multicol_packed.is_table_cell ? Container::GetWidth() : BlockContainer::GetWidth(); }

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Update margins, bounding box and start position for the next element in the layout stack.
		Children of this container call this method to set new bottom margin values to use in
		margin collapsing in which successive content in the layout stack participates. */

	virtual void	UpdateBottomMargins(const LayoutInfo& info, const VerticalMargin* bottom_margin, AbsoluteBoundingBox* child_bounding_box = NULL, BOOL has_inflow_content = TRUE);

	/** Expand relevant parent containers and their bounding-boxes to contain floating and
		absolutely positioned boxes. A call to this method is only to be initiated by floating or
		absolutely positioned boxes.

		@param info Layout information structure
		@param bottom Only relevant for floats: The bottom margin edge of the float, relative to
		top border-edge of this box.
		@param min_bottom Only relevant for floats: The bottom margin edge of the float if
		maximum width of its container is satisfied
		@param child_bounding_box Bounding-box of the propagating descendant, joined with the
		bounding-boxes of the elements up the parent chain towards this element. Relative to the
		top border-edge of this box.
		@param opts Bounding box propagation options */

	virtual void	PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts);

	/** Propagate height of pane-attached float to its flow root. */

	virtual void	PropagatePaneFloatHeight(LayoutCoord float_height);

	/** Update bounding box. */

	virtual void	UpdateBoundingBox(const AbsoluteBoundingBox& child_bounding_box, BOOL skip_content);

	/** Remove form, plugin and iframe objects */

	virtual void	Disable(FramesDocument* doc);

	/** Create form, plugin and iframe objects */

	virtual OP_STATUS
					Enable(FramesDocument* doc);

	/** Calculate the width of the containing block established by this container. */

	virtual LayoutCoord
					CalculateContentWidth(const HTMLayoutProperties& props, ContainerWidthType width_type = INNER_CONTENT_WIDTH) const;

#ifdef PAGED_MEDIA_SUPPORT

	/** Attempt to break page. */

	virtual BREAKST	AttemptPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain);

#endif // PAGED_MEDIA_SUPPORT

	/** Propagate a break point caused by break properties or a spanned element.

		These are discovered during layout, and propagated to the nearest
		multicol container when appropriate. They are needed by the columnizer
		to do column balancing.

		@param virtual_y Virtual Y position of the break (relative to the top
		border edge of this container)
		@param break_type Type of break point
		@param breakpoint If non-NULL, it will be set to the MultiColBreakpoint
		created, if any.
		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint);

	/** Add a pending spanned element. */

	BOOL			AddPendingSpannedElm(LayoutProperties* spanned_cascade, MultiColBreakpoint* breakpoint);

	/** Compute size of content and return IS_TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Cast this object to ScrollableContainer, if it is one. */

	virtual ScrollableContainer*
					GetScrollableContainer() { return multicol_packed.is_scrollable ? this : NULL; }

	/** Cast this object to ScrollableArea, if it is one. */

	virtual ScrollableArea*
					GetScrollable() { return GetScrollableContainer(); }

	/** Cast this object to MultiColumnContainer, if it is one. */

	virtual MultiColumnContainer*
					GetMultiColumnContainer() { return this; }

	/** Is there typically a CoreView associated with this content ?

		@see class CoreView */

	virtual BOOL	HasCoreView() const { return BOOL(multicol_packed.is_scrollable); }

	/** Get baseline of first or last line.

		@param last_line TRUE if last line baseline search (inline-block case).	*/

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const;

	/** Get baseline of inline block or content of inline element that
		has a baseline (e.g. ReplacedContent with baseline, BulletContent). */

	virtual LayoutCoord
					GetBaseline(const HTMLayoutProperties& props) const;

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Traverse all column rows of the multi-column container. */

	void			TraverseRows(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Is this content shrink-to-fit?

		Return TRUE if it is shrink-to-fit, i.e. that it calculates its width
		based on min/max width propagation fromt its child.
		ShrinkToFitContainer is one such type. */

	virtual BOOL	IsShrinkToFit() const { return !!multicol_packed.is_shrink_to_fit; }

	/** Is this a container that divides content into panes?

		Multicol and paged containers are such containers. */

	virtual BOOL	IsMultiPaneContainer() const { return TRUE; }

	/** Get scroll offset, if applicable for this type of box / content. */

	virtual OpPoint	GetScrollOffset();

	/** Set scroll offset, if applicable for this type of box / content.

		@param x If non-NULL, the new X position. If NULL, leave current X position as is.
		@param y If non-NULL, the new Y position. If NULL, leave current Y position as is. */

	virtual void	SetScrollOffset(int* x, int* y);

	/** Propagate min/max widths. */

	virtual void	PropagateMinMaxWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width);

	/** Get the content width of the entire multi-column container.

		While GetContentWidth() returns the width of the containing block
		established by one column, this method returns the full content box
		width of the multi-column container. This value is used when laying out
		elements with column-span:all. */

	LayoutCoord		GetOuterContentWidth() const { return reflow_state->outer_content_width; }

	/** Get total number of columns, as resolved from CSS properties. */

	short			GetColumnCount() const { return column_count; }

	/** Get column width, as resolved from CSS properties. */

	LayoutCoord		GetColumnWidth() const { return column_width; }

	/** Get gap between columns, as resolved from CSS properties. */

	LayoutCoord		GetColumnGap() const { return column_gap; }

#ifdef SUPPORT_TEXT_DIRECTION

	/** Return TRUE if text direction is RTL. */

	BOOL			IsRTL() const { return multicol_packed.rtl; }

#endif // SUPPORT_TEXT_DIRECTION

	/** Get the first column list element (row or spanned box), if any. */

	ColumnRow*		GetFirstRow() const { return rows.First(); }

	/** Get the border area (optionally) and translation for 'box'.

		This will convert the box's internal ("virtual") dimensions to the
		visual representation caused by (multiple) pages or columns.

		@param box The box of which we're interested in the position and
		(optionally) border area.
		@param x_translation (out) Set to the X translation of the first column
		or page in which 'box' lives.
		@param y_translation (out) Set to the Y translation of the first column
		or page in which 'box' lives.
		@param border_rect (out) If non-NULL, set to the union border rectangle
		of all the pages or columns in which 'box' lives. The values are
		relative to the top/left border edge of the target box (in the first
		page or column in which the box lives). Note that these values are
		presented as if the box width is 0 (so to get the correct border box
		width, do border_rect.right - border_rect.left + box->GetWidth()). */

	virtual void	GetAreaAndPaneTranslation(const Box* box, LayoutCoord& x_translation, LayoutCoord& y_translation, RECT* border_rect = NULL, ColumnFinder* ancestor_cf = NULL);

	/** Add statically positioned GCPM float. */

	virtual void	AddStaticPaneFloat(FloatedPaneBox* box);

	/** Skip over GCPM floats that are in the subtree established by the specified descendant.

		Happens when the layout engine skips layout of a subtree (optimization). */

	virtual void	SkipPaneFloats(HTML_Element* descendant);

	/** Return TRUE if this multicol container has a column in which the specified float lives. */

	BOOL			HasPaneFloat(FloatedPaneBox* box);

	/** Return TRUE if this element can be split into multiple outer columns or pages.

		In general, descendant content of a paged or multicol container is
		columnizable, but this is not the case if the content is or is inside
		of a container with scrollbars, button, replaced content, etc. */

	virtual BOOL	IsColumnizable() const;

	/** Distribute content into columns.

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	Columnize(Columnizer& columnizer, Container* container);

	/** Figure out to which column(s) or spanned element a box belongs. */

	virtual void	FindColumn(ColumnFinder& cf);

	/** Return the OpInputContext of this container, if it establishes one. */

	virtual OpInputContext*
					GetInputContext() { return multicol_packed.is_scrollable ? this : NULL; }
};

#ifdef PAGED_MEDIA_SUPPORT

/** Container for overflow:-o-paged* */

class PagedContainer
	: public MultiColumnContainer,
	  public OpPageControlListener,
	  public MessageObject
{
private:

	/** List of pages. */

	PageStack		pages;

	/** Page control widget. */

	OpPageControl*	page_control;

	/** The visual device for the document in which this container lives. */

	VisualDevice*	visual_device;

	/** Time stamp when page change transition started. */

	double			slide_start_time;

	union
	{
		struct
		{
			/** Is a real shrink-to-fit in the box model (e.g. a float or absolutely positioned box with auto width)? */

			unsigned int
					is_shrink_to_fit:1;

			/** Is a real multicol container? */

			unsigned int
					is_multicol:1;

#ifdef SUPPORT_TEXT_DIRECTION

			/** Text direction is right-to-left? */

			unsigned int
					rtl:1;

#endif // SUPPORT_TEXT_DIRECTION

			/** If set, we paginate vertically. If not set, we paginate horizontally. */

			unsigned int
					vertical_pagination:1;

			/** TRUE if the element is "enabled".

				That means that Enable() has been called and we're properly
				connected to the CoreView and input manager, and so on.
				Enabling an element twice would have some unfortunate
				side-effects, such as the fact that setting parent input
				context will cause this element to lose keyboard focus (if it
				had it). */

			unsigned int
					enabled:1;

			/** TRUE if we have decided what the current pan operation is about.

				We may decide to pan the paged container, or we may choose to
				pass it on to our ancestor input context. */

			unsigned int
					pan_decision_made:1;

			/** TRUE if we are to block panning of this paged container.

				This will last for the rest of the panning operation (until the
				user releases the finger). When we're blocking, we're
				propagating the pan event to the ancestor input context. */

			unsigned int
					block_panning:1;

			/** Amount of pending panning along the axis of page progression. */

			signed int
					pending_panning:6;

			/** Amount of pending panning along the axis opposite to page progression. */

			signed int
					orthogonal_panning:6;

			/** TRUE if we are to report scroll events on slide animations. */

			unsigned int
					trigger_scroll_event_on_slide:1;
		} paged_packed;
		UINT32		paged_packed_init;
	};

	/** The page currently being displayed.

		The first page is page #0. */

	unsigned int	current_page;

	/** Offset relative to the current page.

		There are different page navigation methods. One method is for the user
		to grab the page and drag to the next one. This value represents the
		drag offset.

		Positive values mean that we're dragging towards the next page
		(rightwards or downwards in an LTR container). Negative values mean
		that we're dragging towards the previous page. */

	LayoutCoord		page_offset;

	/** Start offset of current slide operation, relative to current page. */

	LayoutCoord		slide_start_offset;

	/** Page width (padding box width). */

	LayoutCoord		page_width;

	/** Page height (padding box height). */

	LayoutCoord		page_height;

	/** Initialize the CoreView under the correct parent. */

	OP_STATUS		InitCoreView(VisualDevice* vis_dev);

	/** Slide one animation frame, and schedule the next one, if there's time for more. */

	void			SlideOneFrame();

	/** Calculate the X and Y translation for the specified page and offset. */

	void			CalculateTranslation(unsigned int page, LayoutCoord offset, LayoutCoord& x, LayoutCoord& y);

	/** Scroll the paged container to the specified page and offset. */

	void			ScrollTo(unsigned int page, LayoutCoord offset);

protected:

	/** Report (memory) error. */

	void			RaiseCondition(OP_STATUS error);

	/** Split content into pages.

		@return FALSE on OOM, TRUE otherwise. */

	BOOL			Paginate(const LayoutInfo& info, const HTMLayoutProperties& props, LayoutCoord& content_height);

	/** Update height of container. */

	virtual void	UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height);

	/** Apply min-width property constraints on min/max values. */

	virtual void	ApplyMinWidthProperty(const HTMLayoutProperties& props, LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width);

	/** Get min/max width. */

	virtual BOOL	GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const;

	/** Return TRUE if this is a root container for layout. */

	virtual BOOL	IsLayoutRoot() const { return FALSE; }

public:

					PagedContainer();
	virtual			~PagedContainer();

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Update margins, bounding box and start position for the next element in the layout stack.
		Children of this container call this method to set new bottom margin values to use in
		margin collapsing in which successive content in the layout stack participates. */

	virtual void	UpdateBottomMargins(const LayoutInfo& info, const VerticalMargin* bottom_margin, AbsoluteBoundingBox* child_bounding_box = NULL, BOOL has_inflow_content = TRUE);

	/** Update bounding box. */

	virtual void	UpdateBoundingBox(const AbsoluteBoundingBox& child_bounding_box, BOOL skip_content);

	/** Create form, plugin and iframe objects */

	virtual OP_STATUS
					Enable(FramesDocument* doc);

	/** Remove form, plugin and iframe objects */

	virtual void	Disable(FramesDocument* doc);

	/** Calculate the width of the containing block established by this container. */

	virtual LayoutCoord
					CalculateContentWidth(const HTMLayoutProperties& props, ContainerWidthType width_type = INNER_CONTENT_WIDTH) const;

	/** Attempt to break page. */

	virtual BREAKST	AttemptPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain);

	/** Compute size of content and return TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Cast this object to MultiColumnContainer, if it is one. */

	virtual MultiColumnContainer*
					GetMultiColumnContainer() { return paged_packed.is_multicol ? this : NULL; }

	/** Cast this object to PagedContainer, if it is one. */

	virtual PagedContainer*
					GetPagedContainer() { return this; }

	/** Get baseline of first line. */

	virtual LayoutCoord
					GetBaseline(const HTMLayoutProperties& props) const;

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Traverse all pages. */

	void			TraversePages(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Is this content shrink-to-fit?

		Return TRUE if it is shrink-to-fit, i.e. that it calculates its width
		based on min/max width propagation fromt its child.
		ShrinkToFitContainer is one such type. */

	virtual BOOL	IsShrinkToFit() const { return !!paged_packed.is_shrink_to_fit; }

	/** Get scroll offset, if applicable for this type of box / content. */

	virtual OpPoint	GetScrollOffset();

	/** Set scroll offset, if applicable for this type of box / content.

		@param x If non-NULL, the new X position. If NULL, leave current X position as is.
		@param y If non-NULL, the new Y position. If NULL, leave current Y position as is. */

	virtual void	SetScrollOffset(int* x, int* y);

	/** Get the border area (optionally) and translation for 'box'.

		This will convert the box's internal ("virtual") dimensions to the
		visual representation caused by (multiple) pages or columns.

		@param box The box of which we're interested in the position and
		(optionally) border area.
		@param x_translation (out) Set to the X translation of the first column
		or page in which 'box' lives.
		@param y_translation (out) Set to the Y translation of the first column
		or page in which 'box' lives.
		@param border_rect (out) If non-NULL, set to the union border rectangle
		of all the pages or columns in which 'box' lives. The values are
		relative to the top/left border edge of the target box (in the first
		page or column in which the box lives). Note that these values are
		presented as if the box width is 0 (so to get the correct border box
		width, do border_rect.right - border_rect.left + box->GetWidth()). */

	virtual void	GetAreaAndPaneTranslation(const Box* box, LayoutCoord& x_translation, LayoutCoord& y_translation, RECT* border_rect = NULL, ColumnFinder* ancestor_cf = NULL);

	/** Return TRUE if this paged container has a column in which the specified float lives. */

	BOOL			HasPaneFloat(FloatedPaneBox* box);

	/** Return TRUE if this element can be split into multiple outer columns or pages.

		In general, descendant content of a paged or multicol container is
		columnizable, but this is not the case if the content is or is inside
		of a container with scrollbars, button, replaced content, etc. */

	virtual BOOL	IsColumnizable() const;

	/** Handle input action.

		@param[in] The action.

		@return The "handled" state of the action. OpBoolean::IS_TRUE if the
		action was consumed. OpBoolean::IS_FALSE if it was unknown and should
		bubble. */

	OP_BOOLEAN		HandleInputAction(OpInputAction* input_action);

	/* BEGIN implement the CoreView interface */
	virtual BOOL	OnInputAction(OpInputAction* input_action);
	virtual const char*
					GetInputContextName() { return "Paged Container"; }
	virtual BOOL	GetBoundingRect(OpRect &rect);
	/* END implement the CoreView interface */

	/* BEGIN implement the OpPageControlListener interface */
	virtual void	OnPageChange(unsigned int page);
	/* END implement the OpPageControlListener interface */

	/* BEGIN implement the MessageObject interface */
	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	/* END implement the MessageObject interface */

	/** Is pagination vertical?

		@return TRUE if pagination is vertical, FALSE if it's horizontal. */

	BOOL			IsPaginationVertical() const { return paged_packed.vertical_pagination; }

#ifdef SUPPORT_TEXT_DIRECTION

	/** Is pagination right-to-left?

		Should only be called if IsPaginationVertical() returns FALSE.

		@return TRUE if pagination is right-to-left, FALSE if it's left-to-right. */

	BOOL			IsRTL() const { return paged_packed.rtl; }

#endif // SUPPORT_TEXT_DIRECTION

	/** Jump to the specified page.

		@param page New page number. 0 means the first page.
		@param allow_slide TRUE if fancy transitions from page to page are
		allowed. PrefsCollectionDisplay::SmoothScrolling also needs to be
		enabled in order for this to work.
		@param trigger_event If TRUE, trigger a DOM event if this operation
		caused a page change.
		@return FALSE on OOM, TRUE otherwise. */

	BOOL			SetCurrentPageNumber(unsigned int page, BOOL allow_slide = FALSE, BOOL trigger_event = TRUE);

	/** Set current page number. */

	unsigned int	GetCurrentPageNumber() const { return current_page; }

	/** Get total number of pages. */

	unsigned int	GetTotalPageCount() const { return pages.GetColumnCount(); }

	/** Get the page(s) containing the specified rectangle.

		The rectangle is relative to the top/left border edge of the paged
		container displaying the current page. */

	void			GetPagesContaining(const OpRect& rect, unsigned int& first_page, unsigned int& last_page);

	/** Get the horizontal scroll position for the current page.

		This will include drag / animation offset if we're currently between
		pages. */

	LayoutCoord		GetViewX() const;

	/** Get the horizontal scroll position for the specified page. */

	LayoutCoord		GetViewX(unsigned int page) const;

	/** Get the vertical scroll position for the current page.

		This will include drag / animation offset if we're currently between
		pages. */

	LayoutCoord		GetViewY() const;

	/** Get the vertical scroll position for the specified page. */

	LayoutCoord		GetViewY(unsigned int page) const;

	/** Get total width of the scrollable padding box area. */

	LayoutCoord		GetScrollPaddingBoxWidth() const;

	/** Get total height of the scrollable padding box area. */

	LayoutCoord		GetScrollPaddingBoxHeight() const;

	/** Paint the page control widget. */

	void			PaintPageControl(const HTMLayoutProperties& props, VisualDevice* visual_device);

	/** Get page control height. */

	LayoutCoord		GetPageControlHeight() const;

	/** Return TRUE if the specified coordinates are within the page control widget. */

	BOOL			IsOverPageControl(int x, int y);

	/** Return the OpInputContext of this container, if it establishes one. */

	virtual OpInputContext*
					GetInputContext() { return this; }

	/** Handle event that occurred on this element. */

	virtual void	HandleEvent(FramesDocument* doc, DOM_EventType event, int offset_x, int offset_y);

	/** Return TRUE if the actual value of 'overflow' is 'visible'. */

	virtual BOOL	IsOverflowVisible() { return FALSE; }
};

/** Container for the root element, in paged media. */

class RootPagedContainer
  : public PagedContainer
{
protected:

	/** Update height of container. */

	virtual void	UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height);

	/** Return TRUE if this is a root container for layout. */

	virtual BOOL	IsLayoutRoot() const { return TRUE; }

public:

	/** Compute size of content and return TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

	/** Return the OpInputContext of this container, if it establishes one. */

	virtual OpInputContext*
					GetInputContext() { return NULL; }
};

#endif // PAGED_MEDIA_SUPPORT

/** The content used for bullet list markers (images, SVG, simple glyphs). */

class BulletContent
  : public Content
{
private:

	/** The width of the bullet. */

	LayoutCoord		width;

	/** The height of the bullet. */

	LayoutCoord		height;

public:

	/** Sets the size of the bullet.

		@param width The width of the bullet.
		@param height The height of the bullet. */

	void			SetBulletSize(LayoutCoord width, LayoutCoord height) { this->width = width; this->height = height; }

	/** Paint the bullet.

		@param layout_props the cascade of the list marker.
		@param doc The FramesDocument where the bullet is.
		@param visual_device The visual device that handles doc.
		@return OK or OOM. */

	OP_STATUS		PaintBullet(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device);

	/** Traverse this bullet content. */

	virtual BOOL	LineTraverse(TraversalObject* traversal_object, LayoutProperties* layout_props, LineSegment& segment, LayoutCoord baseline, LayoutCoord x);

	/** Get width of container. */

	virtual LayoutCoord
					GetWidth() const { return width; }

	/** Get height of border box. */

	virtual LayoutCoord
					GetHeight() const { return height; }

	/** Get minimum height (height if maximum width is satisfied) */

	virtual LayoutCoord
					GetMinHeight() const { return height; }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN) { return LAYOUT_CONTINUE; }

	/** Get baseline of first or last line.

		@param last_line TRUE if last line baseline search (inline-block case).	*/

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const { return height; }

	/** Get the baseline if maximum width is satisfied. */

	virtual LayoutCoord
					GetMinBaseline(const HTMLayoutProperties& props) const { return height; }

	/** Get min/max width.

		Returns FALSE if content has no min/max width. */

	virtual BOOL	GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const;

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth() {}

	/** Get bullet content. */

	virtual BulletContent*
					GetBulletContent() { return this; }

	/** Should this content layout its children? */

	virtual BOOL	ShouldLayoutChildren() { return FALSE; }
};

/** Replaced content box. */

class ReplacedContent
  : public Content
{
protected:

	/** Width of border-box */

	LayoutCoord		replaced_width;

	/** Height of border-box */

	LayoutCoord		replaced_height;

	union
	{
		struct
		{

			/** Height is percentage based. */

			unsigned int
					percentage_height:1;

			/** Set to TRUE when min/max values are calculated and up-to-date. */

			unsigned int
					minmax_calculated:1;

			/** Reflow has been requested, but the element has not been marked dirty yet. */

			unsigned int
					reflow_requested:1;

			/** Size is determined. */

			unsigned int
					size_determined:1;

			/** Use alternative text for sizing and painting (when not loading). */

			unsigned int
					use_alternative_text:1;

			/** Is content visible? */

			unsigned int
					content_visible:1;

		} packed;
		UINT32		packed_init;
	};

	/** Minimum height (height if maximum width of container is satisfied) */

	LayoutCoord		minimum_height;

	/** Minimum border-box width. Not affected by ERA. */

	LayoutCoord		normal_minimum_width;

	/** Minimum border-box width. Affected by ERA. */

	LayoutCoord		minimum_width;

	/** Maximum border-box width required. */

	LayoutCoord		maximum_width;

	/** Has reflow been forced? */

	virtual BOOL	GetReflowForced() const { return packed.reflow_requested; }

	/** Set reflow force status, which will be checked and reset during SignalChange(). */

	virtual void	SetReflowForced(BOOL force_reflow_on) { packed.reflow_requested = !!force_reflow_on; }

	/** Retain aspect ratio if ERA causes the element to become narrower? */

	virtual BOOL	RetainRatio() const { return TRUE; }

	/** Get the baseline if maximum width is satisfied. */

	virtual LayoutCoord
					GetMinBaseline(const HTMLayoutProperties& props) const { return minimum_height; }

	/** Allow zero size? */

	virtual BOOL	AllowZeroSize() const { return FALSE; }

	/** Calculate minimum width of border+padding for weak content. */

	void			CalculateBorderPaddingMinWidth(LayoutProperties* cascade, LayoutCoord& border_padding_width) const;

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth();

	/** Sets content visibility - used to enable/disable content like plugins or forms */

	void			SetVisibility(FramesDocument *doc, BOOL visible);

	/** Calculate intrinsic size.

		@param cascade The cascade for this element
		@param info Layout info
		@param width (out) Set to the intrinsic width of the content-box.
		@param height (out) Set to the intrinsic height of the content-box.
		@param ratio (out) Set to the intrinsic ratio of the content-box, in 14.16 fixed point format. */

	virtual OP_STATUS
					CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio) = 0;

	/** Update size of content.

		@param cascade The cascade for this element
		@param info Layout info
		@param width Border-box width of this element
		@param height Border-box height of this element

		@return OpBoolean::IS_TRUE if size changed, OpBoolean::IS_FALSE if not, OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_BOOLEAN
					SetNewSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord width, LayoutCoord height, LayoutCoord modified_horizontal_border_padding);

	/** Calculate non-auto width and height for replaced box.
	 *
	 *  @param cascade						props for this element
	 *  @param info							Layout information for this reflow
	 *  @param calculate_min_max_widths		TRUE if we need to calculate min/max widths.
	 *  @param width						In intrinsic width of the element, out calculated width. Refers to the width of the content-box
	 *  @param height						In intrinsic height of the element, out calculated height. Refers to the height of the content-box
	 *  @param css_height					Used CSS height (from CalculateCssHeight())
	 *  @param intrinsic_ratio				In intrinsic ratio of the element. Refers to the relation between width and height
	 *                                      and a non-zero value signals that the element should retain its aspect ratio if re-scaled.
	 *  @param horizontal_border_padding	Horizontal border + padding. May be scaled by ERA
	 *  @param vertical_border_padding      Vertical border + padding
	 *  @param height_is_auto				Is the height auto or should be treated as auto?
	 *  @return ERR_NO_MEMORY if OOM, OK otherwise.
	 */

	OP_STATUS		CalculateSize(LayoutProperties* cascade, LayoutInfo& info, BOOL calculate_min_max_widths, LayoutCoord& width, LayoutCoord& height, LayoutCoord css_height, int intrinsic_ratio, LayoutCoord& horizontal_border_padding, LayoutCoord vertical_border_padding, BOOL height_is_auto);

	/** Perform some ERA-specific size-shrinking. */

	OP_STATUS		AdjustEraSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, LayoutCoord intrinsic_width, LayoutCoord intrinsic_height, LayoutCoord& horizontal_border_padding, BOOL keep_aspect_ratio);

	/** Calculate and return minimum width for ERA. */

	LayoutCoord		CalculateEraMinWidth(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord normal_minimum_width) const;

public:
					ReplacedContent() : replaced_width(0), replaced_height(0), minimum_height(0), normal_minimum_width(0), minimum_width(0), maximum_width(0) { packed_init = 0; }

	/** Calculate the size of the replaced content with object-fit.

		If the content does not support object-fit, or if object-fit fails, FALSE is returned.

	 *  @param cascade	the cascade.
	 *  @param[out] dst	the fitted size of the content. x and y are not modified.
	 */

	virtual BOOL	CalculateObjectFit(LayoutProperties* cascade, OpRect& dst) { return FALSE; }

	/** Calculate the position of the replaced content with object-position.

	  *	If the content does not support object-position, FALSE is returned.
	  *
	  * @param cascade		the cascade.
	  * @param[inout] fit	provide the size of the content to position (in) and receive the calculated position of the content (out).
	 */

	virtual BOOL	CalculateObjectPosition(LayoutProperties* cascade, OpRect& fit) { return FALSE; }

	/** Convenience method that bottles ReplacedContent::CalculateObjectFit and ReplacedContent::CalculateObjectPosition into one call.
	  *
	  * The return value is the conjunction of the two return values.
	  */
	BOOL			CalculateObjectFitPosition(LayoutProperties* cascade, OpRect& fit)
	{
		return CalculateObjectFit(cascade, fit) && CalculateObjectPosition(cascade, fit);
	}

	/** Get width of border box. */

	virtual LayoutCoord
					GetWidth() const { return replaced_width; }

	/** Get height of border box. */

	virtual LayoutCoord
					GetHeight() const { return replaced_height; }

	/** Get the size of the content box by subtracting padding and borders from
	    actual size. */

	OpRect			GetInnerRect(const HTMLayoutProperties& props) const;

	/** Get baseline of first or last line.

		@param last_line TRUE if last line baseline search (inline-block case).	*/

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const { return replaced_height; }

	/** Is this content replaced content? */

	virtual BOOL	IsReplaced() const { return TRUE; }

	/** Lay out replaced box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Increase actual width and min/max widths. */

	virtual void	IncreaseWidths(LayoutCoord increment);

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Traverse box with children. */

	virtual BOOL	LineTraverse(TraversalObject* traversal_object, LayoutProperties* layout_props, LineSegment& segment, LayoutCoord baseline, LayoutCoord x);

	/** Is height relative? */

	virtual BOOL	HeightIsRelative() const { return packed.percentage_height == 1; }

	/** Is this form content? */

	virtual BOOL	IsForm() const { return FALSE; }

	/** Detach any CoreView related with this layoutbox from its parent */
	virtual void	DetachCoreView(FramesDocument* doc) {}

	/** Attach any CoreView related with this layoutbox from its parent */
	virtual void	AttachCoreView(FramesDocument* doc, HTML_Element* element) {}

	/** Should we show alternative text instead of picture here */

	BOOL			IsUsingAltText() { return packed.use_alternative_text; }

	/** Get minimum height (height if maximum width is satisfied) */

	virtual LayoutCoord
					GetMinHeight() const { return minimum_height; }

	/** Compute size of content and return IS_TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Get min/max width. */

	virtual BOOL	GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const;

	/** Is content weak? */

	virtual BOOL	IsWeakContent() const { return TRUE; }

	/** Scale border and padding (in ERA)? */

	virtual BOOL	ScaleBorderAndPadding() const { return TRUE; }

	/** Request reflow later if necessary. */

	void			RequestReflow(LayoutWorkplace* workplace);

	void			ResetSizeDetermined() { packed.size_determined = FALSE; }

	/** Should this content layout its children? */

	virtual BOOL	ShouldLayoutChildren() { return FALSE; }

#ifdef MEDIA_HTML_SUPPORT

	/** Fetch a MediaElement from the HTML_Element supplied. The
		HTML_Element is assumed to be either a <video>, one of the
		structural video elements or a cue block element. */

	static MediaElement*
					GetMediaElement(HTML_Element* html_element);

#endif // MEDIA_HTML_SUPPORT
};

/** Calculate intrinsic ratio in fixed point format. */

inline int CalculateIntrinsicRatio(LayoutCoord intrinsic_width, LayoutCoord intrinsic_height) { return (intrinsic_width << 16) / intrinsic_height; }

/** Calculate corresponding width from a height and intrinsic width/height or ratio. */

inline LayoutCoord CalculateWidthFromIntrinsicRatio(LayoutCoord height, LayoutCoord intrinsic_width, LayoutCoord intrinsic_height, int intrinsic_ratio)
{
	return intrinsic_width && intrinsic_height ? LayoutCoord((intrinsic_width * height + intrinsic_height/2 ) / intrinsic_height) : LayoutCoord((height * intrinsic_ratio) >> 16);
}

/** Calculate corresponding height from a width and intrinsic width/height or ratio. */

inline LayoutCoord CalculateHeightFromIntrinsicRatio(LayoutCoord width, LayoutCoord intrinsic_width, LayoutCoord intrinsic_height, int intrinsic_ratio)
{
	return intrinsic_width && intrinsic_height ? LayoutCoord((intrinsic_height * width + intrinsic_width/2) / intrinsic_width) : LayoutCoord((width << 16) /  intrinsic_ratio);
}

/** Image box. */

class ImageContent
  : public ReplacedContent
{
private:
#ifdef SUPPORT_VISUAL_ADBLOCK
	Image			*m_blocked_content;	// alternative rendering cached Image only used when in content block mode and content will be blocked
#endif // SUPPORT_VISUAL_ADBLOCK

protected:

	/** Calculate intrinsic size.

		@param cascade The cascade for this element
		@param info Layout info
		@param width (out) Set to the intrinsic width of the content-box.
		@param height (out) Set to the intrinsic height of the content-box.
		@param ratio (out) Set to the intrinsic ratio of the content-box, in 14.16 fixed point format. */

	virtual OP_STATUS
					CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio);

	/** Allow zero size? */

	virtual BOOL	AllowZeroSize() const { return TRUE; }

	/** Get the image associated with this box. */

	Image			GetImage(LayoutProperties* layout_props);

	/** Check whether the image is visible, taking skins into account. */

	BOOL			GetImageVisible(LayoutProperties* layout_props);

#ifdef SKIN_SUPPORT
	/** If this is a content for a skin image, return the skin Image. Otherwise an empty image will be returned. */

	Image			GetSkinImage();
#endif // SKIN_SUPPORT

public:
	ImageContent()
#ifdef SUPPORT_VISUAL_ADBLOCK
	: m_blocked_content(NULL)
#endif // SUPPORT_VISUAL_ADBLOCK
	{ }

#ifdef SUPPORT_VISUAL_ADBLOCK
	virtual ~ImageContent() { OP_DELETE(m_blocked_content); }
#endif // SUPPORT_VISUAL_ADBLOCK

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

	/** Compute size of content and return IS_TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN) { return LAYOUT_CONTINUE; }

	/** Paint image box on screen. */

	virtual OP_STATUS
					Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area);

	/** Is this image content? */

	virtual BOOL	IsImage() const { return TRUE; }

	/** Remove form, plugin and iframe objects */

	virtual void	Disable(FramesDocument* doc);

	/** Calculate the size of the replaced content with object-fit. If the size of
		the image is not available, sizes 0 are reported and FALSE is returned. */

	virtual BOOL	CalculateObjectFit(LayoutProperties* cascade, OpRect& dst);

	/** Calculate the position of the replaced content with object-position. */

	virtual BOOL	CalculateObjectPosition(LayoutProperties* cascade, OpRect& fit);
};

#ifdef SVG_SUPPORT
/** Superclass for IFrameContent and SVGContent for intermediate storage of
	object-fit and object-position properties.

	This is necessary because the cascade is stopped at document boundaries.
  */

class ObjectFitPositionStorage
{
protected:
	/** The cascaded values of object-fit and object-position as stored during
		Layout and used during Paint. */

	ObjectFitPosition object_fit_position;

public:
	ObjectFitPosition GetObjectFitPosition() const { return object_fit_position; }
};

/** SVG box. */

class SVGContent
  : public ReplacedContent, public ObjectFitPositionStorage
{
protected:

	/** Calculate intrinsic size.

		@param cascade The cascade for this element
		@param info Layout info
		@param width (out) Set to the intrinsic width of the content-box.
		@param height (out) Set to the intrinsic height of the content-box.
		@param ratio (out) Set to the intrinsic ratio of the content-box, in 14.16 fixed point format. */

	virtual OP_STATUS
					CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio);

	/** Update size of content.

		@param cascade The cascade for this element
		@param info Layout info
		@param width Border-box width of this element
		@param height Border-box height of this element

		@return OpBoolean::IS_TRUE if size changed, OpBoolean::IS_FALSE if not, OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_BOOLEAN
					SetNewSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord width, LayoutCoord height, LayoutCoord modified_horizontal_border_padding);

	/** Get any IFrame that may be containing this SVG.
	  *
	  *	@return	any containing IFrame, or NULL if none.
	  */
	IFrameContent*	GetIFrame(LayoutInfo& info);

public:

	/** Paint SVG box on screen. */

	virtual OP_STATUS
					Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area);

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Get svg content. */

	virtual SVGContent *
					GetSVGContent() { return this; }

	/** Signal that content (images) may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

	/** Remove form, plugin, iframe objects and release focus */

	virtual void	Disable(FramesDocument* doc);

	/** Allow zero size? */

	virtual BOOL	AllowZeroSize() const { return TRUE; }

	BOOL			IsTransparentAt(FramesDocument *doc, LayoutProperties *cascade, LayoutCoord x, LayoutCoord y) const;

	/** Compute size of content and return IS_TRUE if size is changed.

		Also retrieve object-fit and object-position values from any enclosing IFrame. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Should this content layout its children? */

	virtual BOOL	ShouldLayoutChildren() { return TRUE; }

	/** Get the corresponding SVGImage. */

	class SVGImage*	GetSVGImage(FramesDocument* doc, HTML_Element* html_element);
};
#endif // SVG_SUPPORT

#ifdef _PLUGIN_SUPPORT_
enum EmbedDisplayMode
{
	EMBED_NORMAL = 0,
	EMBED_ALT
};

// Placeholder state for EmbedContent
enum EmbedPlaceholderState {
	EMBED_PSTATE_NONE = 0,
	EMBED_PSTATE_MISSING,
	EMBED_PSTATE_DISABLED,
	EMBED_PSTATE_CRASHED
};

/** Embed box. */

class EmbedContent
  : public ReplacedContent
  , public CoreViewScrollListener
  , public PluginInstallationListener // To be notified when a plugin is resolved by the platform
  , public OpPluginWindowListener
#ifdef USE_PLUGIN_EVENT_API
  , public OpInputContext
#endif
{
private:
	FramesDocument* m_frames_doc;

	/** VisualDevice used to create plugin window */
	VisualDevice*	win_visdev;

	OpNS4Plugin*	opns4plugin;

#ifndef USE_PLUGIN_EVENT_API
	OpPluginWindow*	window;
#endif // !USE_PLUGIN_EVENT_API

	OpStringS		m_plugin_name;
	OpStringS		m_plugin_mimetype;
	OpStringS		m_plugin_downloadurl;
	URL				m_plugin_content_url;

	union
	{
		struct
		{
#ifndef USE_PLUGIN_EVENT_API
			signed short
					x:16;

			unsigned short
					transparent:1;
#endif // !USE_PLUGIN_EVENT_API

			unsigned short
					internal_player:1;

			unsigned short
					show:1;

			unsigned short
					loading_failed:1;

			unsigned short
					removed:1;

			/** Display mode; see enum EmbedDisplayMode. */

			unsigned short
					display_mode:1;

#ifdef PLUGIN_AUTO_INSTALL
			/** Was the listener already set? We don't want to receive multiple notifications, even more we only unregister the listener once. */

			unsigned short
					listener_set:1;

			/** Is a plugin resolved for this mimetype? */
			unsigned short
					plugin_available:1;
#endif // PLUGIN_AUTO_INSTALL

			/** Placeholder state - plugin missing, plugin disabled */
			unsigned short
					placeholder_state:2;

			/** Is the placeholder hovered? */
			unsigned short
					hovered:1;
		} embed_packed;
		UINT32 embed_packed_init;
	};

#ifndef USE_PLUGIN_EVENT_API
	long			y;
#endif // !USE_PLUGIN_EVENT_API

	LayoutCoord		abs_doc_x;
	LayoutCoord		abs_doc_y;

#ifdef SUPPORT_VISUAL_ADBLOCK
	Image			m_blocked_content;	// alternative rendering only used when in content block mode and content will be blocked
#endif // SUPPORT_VISUAL_ADBLOCK

	/** Show embed. */

	OP_STATUS		ShowEmbed(FramesDocument* doc);

	/** Uninitialize and delete plugin. */

	void			ShutdownPlugin();

protected:

	/** Allow zero size? */

	virtual BOOL	AllowZeroSize() const { return FALSE; }

	/** Calculate intrinsic size.

		@param cascade The cascade for this element
		@param info Layout info
		@param width (out) Set to the intrinsic width of the content-box.
		@param height (out) Set to the intrinsic height of the content-box.
		@param ratio (out) Set to the intrinsic ratio of the content-box, in 14.16 fixed point format. */

	virtual OP_STATUS
					CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio);

	/** Update size of content.

		@param cascade The cascade for this element
		@param info Layout info
		@param width Border-box width of this element
		@param height Border-box height of this element

		@return OpBoolean::IS_TRUE if size changed, OpBoolean::IS_FALSE if not, OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_BOOLEAN
					SetNewSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord width, LayoutCoord height, LayoutCoord modified_horizontal_border_padding);

public:
					EmbedContent(FramesDocument* doc)
					  : m_frames_doc(doc),
						win_visdev(NULL),
						opns4plugin(NULL),
#ifndef USE_PLUGIN_EVENT_API
						window(NULL),
						y(-10000),
#endif // !USE_PLUGIN_EVENT_API
						abs_doc_x(-10000),
						abs_doc_y(-10000)
		{
			embed_packed_init = 0;
			embed_packed.show = TRUE;
#ifndef USE_PLUGIN_EVENT_API
			embed_packed.x = -10000;
#endif
		}

	virtual			~EmbedContent();

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

	/** Paint embed box on screen. */

	virtual OP_STATUS
					Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area);

	/** Create form, plugin and iframe objects */

	virtual OP_STATUS
					Enable(FramesDocument* doc);

	/** Remove form, plugin and iframe objects */

	virtual void	Disable(FramesDocument* doc);

	/** Detach any CoreView related with this layoutbox from its parent */
	virtual void	DetachCoreView(FramesDocument* doc);

	/** Attach any CoreView related with this layoutbox from its parent */
	virtual void	AttachCoreView(FramesDocument* doc, HTML_Element* element);

	/** Get the plugin */

	OpNS4Plugin*	GetOpNS4Plugin() const { return opns4plugin; }

	/** Check if loading the plug-in library failed. */

	BOOL			PluginLoadingFailed() const { return embed_packed.loading_failed; }

	/** Mark this embed as failed; do not attempt to load it. */

	void			SetPluginLoadingFailed() { embed_packed.loading_failed = TRUE; }

	/** Is this embed content? */

	virtual BOOL	IsEmbed() const { return TRUE; }

	/** Is there typically a CoreView associated with this content ?

		@see class CoreView */

	virtual BOOL	HasCoreView() const { return TRUE; }

	/** Get the OpPluginWindow used. */

	virtual OpPluginWindow*
					GetPluginWindow() const;

	/** Get the display mode of the embed content. */

	EmbedDisplayMode GetEmbedContentType() const { return (EmbedDisplayMode) embed_packed.display_mode; }

	/** Show alternative content when the plugin is not installed. Replaces usage of the default plugin */

	OP_STATUS		ShowAltPlugContent(URL* base_url);

	/** Handles the mouse events when the plugin is not installed */

	OP_STATUS		HandleMouseEvent(Window* win, DOM_EventType dom_event);

#if defined(WIC_USE_ASK_PLUGIN_DOWNLOAD) && !defined(PLUGIN_AUTO_INSTALL)
	/** Shows download dialogue for Flash Plugin if not installed */

	OP_STATUS		ShowDownloadResource(FramesDocument* doc);
#endif // WIC_USE_ASK_PLUGIN_DOWNLOAD && !PLUGIN_AUTO_INSTALL

	/** Hide external content, such as plug-ins, iframes, etc. */

	virtual void	Hide(FramesDocument* doc);

	/** Causes repaint of the content area. Used when painting custom messages and placeholder. */

	void			Invalidate();

	/** OpPluginWindowListener interface implementation */

	virtual void	OnMouseDown();
	virtual void	OnMouseUp();
	virtual void	OnMouseHover();
	virtual void	OnCrash() { embed_packed.placeholder_state = EMBED_PSTATE_CRASHED; Invalidate(); }

#ifdef USE_PLUGIN_EVENT_API
	/* OpInputContext interface implementation */
	virtual BOOL	OnInputAction(OpInputAction* action);
	virtual void	OnFocus(BOOL focus, FOCUS_REASON reason);
	virtual OpTypedObject::Type
					GetType() { return EMBEDDED_CONTENT_TYPE; }
#endif // USE_PLUGIN_EVENT_API

	/** CoreViewScrollListener interface implementation */

	virtual void	OnScroll(CoreView* view, INT32 dx, INT32 dy, CoreViewScrollListener::SCROLL_REASON reason);
	virtual void	OnParentScroll(CoreView* view, INT32 dx, INT32 dy, CoreViewScrollListener::SCROLL_REASON reason);

	/** Set plugin as removed because it performed an illegal instruction */

	void			SetRemoved() { embed_packed.removed = TRUE; }

	/** Check if plugin has been removed */

	BOOL			PluginRemoved() const { return embed_packed.removed; }

#ifdef PLUGIN_AUTO_INSTALL
public:
	/** PluginInstallationListener implementation, notifies us when the plugin is resolved and available (i.e. we can know its name) */
	virtual void	OnPluginAvailable(const uni_char* mime_type);
#endif // PLUGIN_AUTO_INSTALL

	virtual void	OnPluginDetected(const uni_char* mime_type);

protected:
	/** Paints placeholder for missing and disabled plugins */
	OP_STATUS		PaintPlaceholder(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area);
};

#endif // _PLUGIN_SUPPORT_

#ifdef MEDIA_SUPPORT
/** Media content. Depending on the object pointed by ATTR_MEDIA_ATTR_IDX it can be
	either HTML5 Audio/Video, JIL Camera/Video or - in general - any other Media based content
*/
class MediaContent
	: public ReplacedContent
{
protected:
	/** Get the Media from the HTML_Element for this video.

		@param content_elm The HTML_Element that this Content belongs to.

		@return The Media for this video content. */

	Media*			GetMedia() const;

public:
	/** Calculate intrinsic size.

		@param cascade The cascade for this element
		@param info Layout info
		@param width (out) Set to the intrinsic width of the content-box.
		@param height (out) Set to the intrinsic height of the content-box.
		@param ratio (out) Set to the intrinsic ratio of the content-box, in 14.16 fixed point format. */

	virtual OP_STATUS
					CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio);

	/** Paint video on screen. */

	virtual OP_STATUS
					Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area);

	/** Calculate the size of the replaced content with object-fit. */

	virtual BOOL	CalculateObjectFit(LayoutProperties* cascade, OpRect& dst);

	/** Calculate the position of the replaced content with object-position. */

	virtual BOOL	CalculateObjectPosition(LayoutProperties* cascade, OpRect& fit);
};
#endif // MEDIA_SUPPORT

#ifdef MEDIA_HTML_SUPPORT

/** Video content. Depending on the state of the video element, either
	a poster image or the current video frame is displayed. Controls
	may be rendered on top of this. */

class VideoContent : public MediaContent
{
private:

	union
	{
		struct
		{
			/** Cues need to be reflowed. */

			unsigned int
					reflow_cues:1;

		} video_packed;
		UINT32		video_packed_init;
	};

protected:
	MediaElement*	GetMediaElement() const { return ReplacedContent::GetMediaElement(GetHtmlElement()); }

	virtual BOOL	AllowZeroSize() const { return TRUE; }

public:

					VideoContent()
					  : video_packed_init(0) {}

	/** Calculate intrinsic size.

		@param cascade The cascade for this element
		@param info Layout info
		@param width (out) Set to the intrinsic width of the content-box.
		@param height (out) Set to the intrinsic height of the content-box.
		@param ratio (out) Set to the intrinsic ratio of the content-box, in 14.16 fixed point format. */

	virtual OP_STATUS
					CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio);

	/** Update size of content.

		@param cascade The cascade for this element
		@param info Layout info
		@param width Border-box width of this element
		@param height Border-box height of this element

		@return OpBoolean::IS_TRUE if size changed, OpBoolean::IS_FALSE if not, OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_BOOLEAN
					SetNewSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord width, LayoutCoord height, LayoutCoord modified_horizontal_border_padding);

	/** Paint video on screen. */

	virtual OP_STATUS
					Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area);

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);
};

/** Controls for Video content. Used to render controls in the correct
	order - after any tracks has been rendered. */

class MediaControlsContent : public ReplacedContent
{
public:

	/** Calculate intrinsic size. */

	virtual OP_STATUS
					CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio);

	/** Paint video controls. */

	virtual OP_STATUS
					Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area);
};

/** Wrapping container for <video>.

	A container that contains:

		VideoContent,
		BlockContainer with/for <track>'s and
		MediaControlsContent

	in that order. Should act as replaced content. */

class VideoWrapperContainer
  : public ShrinkToFitContainer
{
private:

	union
	{
		struct
		{
			/** Is the width auto? */

			unsigned int
					width_is_auto:1;

		} wrapper_packed;
		UINT32		wrapper_packed_init;
	};

public:

					VideoWrapperContainer()
						: wrapper_packed_init(0) {}

	/** Apply min-width property constraints on min/max values. */

	virtual void	ApplyMinWidthProperty(const HTMLayoutProperties& props, LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width);

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth();

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

	/** Compute size of content and return TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Return TRUE if this is a ShrinkToFitContainer that calculates its width based on min/max width propagation of its content. */

	virtual BOOL	IsShrinkToFit() const { return wrapper_packed.width_is_auto; }

	/** Update margins, bounding box and start position for the next element in the layout stack.
		Children of this container call this method to set new bottom margin values to use in
		margin collapsing in which successive content in the layout stack participates. */

	virtual void	UpdateBottomMargins(const LayoutInfo& info, const VerticalMargin* bottom_margin, AbsoluteBoundingBox* child_bounding_box = NULL, BOOL has_inflow_content = TRUE);

	/** Update height of container. */

	virtual void	UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height);
};

#endif // MEDIA_HTML_SUPPORT

#ifdef CANVAS_SUPPORT

/** Canvas box. */

class CanvasContent
  : public ReplacedContent
{
protected:

	/** Calculate intrinsic size.

		@param cascade The cascade for this element
		@param info Layout info
		@param width (out) Set to the intrinsic width of the content-box.
		@param height (out) Set to the intrinsic height of the content-box.
		@param ratio (out) Set to the intrinsic ratio of the content-box, in 14.16 fixed point format. */

	virtual OP_STATUS
					CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio);

	/** Get the Canvas object from the HTML_Element for this canvas.

		@param content_elm The HTML_Element that this Content belongs to.

		@return The Canvas for this canvas content. */

	Canvas*			GetCanvas(HTML_Element* content_elm);

public:
	/** Paint canvas on screen. */

	virtual OP_STATUS
					Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area);

# ifdef DOM_FULLSCREEN_MODE
	/** Paint canvas on screen as a fullscreen element. */

	virtual OP_STATUS
					PaintFullscreen(FramesDocument* doc, VisualDevice* visual_device, HTML_Element* elm, const RECT& area);
# endif // DOM_FULLSCREEN_MODE
};

#endif // CANVAS_SUPPORT

/** Progress box. */

class ProgressContent
  : public ReplacedContent
{
private:
	/** Progress widget. */

	OpProgress*	progress;
protected:

	/** Calculate intrinsic size.

		@param cascade The cascade for this element
		@param info Layout info
		@param width (out) Set to the intrinsic width of the content-box.
		@param height (out) Set to the intrinsic height of the content-box.
		@param ratio (out) Set to the intrinsic ratio of the content-box, in 14.16 fixed point format. */

	virtual OP_STATUS
					CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio);

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);
public:
	ProgressContent() : progress(NULL) {}
	~ProgressContent();

	/** Remove form, plugin and iframe objects */

	virtual void	Disable(FramesDocument* doc);

	/** Paint progress on screen. */

	virtual OP_STATUS
					Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area);
};

/** Iframe box. */

class IFrameContent
  : public ReplacedContent
#ifdef SVG_SUPPORT
	, public ObjectFitPositionStorage
#endif
{
private:

	FramesDocElm*	frame;

	union
	{
		struct
		{
			unsigned int	expand_iframe_height:1;
			unsigned int	expand_iframe_width:1;

			/* width: -o-content-size; is computed in two passes. The
			   first pass without the width of the frame and the
			   second one with.

			   This allows the frame to shrink along with the
			   container if there is additional rules on the width,
			   like min-width. */
			unsigned int	second_pass_width:1;

		} iframe_packed;
		unsigned char iframe_packed_init;
	};

protected:

	/** Allow zero size? */

	virtual BOOL	AllowZeroSize() const { return TRUE; }

	/** Retain aspect ratio if ERA causes the element to become narrower? */

	virtual BOOL	RetainRatio() const { return FALSE; }

	/** Calculate intrinsic size.

		@param cascade The cascade for this element
		@param info Layout info
		@param width (out) Set to the intrinsic width of the content-box.
		@param height (out) Set to the intrinsic height of the content-box.
		@param ratio (out) Set to the intrinsic ratio of the content-box, in 14.16 fixed point format. */

	virtual OP_STATUS
					CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio);

	/** Update size of content.

		@param cascade The cascade for this element
		@param info Layout info
		@param width Border-box width of this element
		@param height Border-box height of this element

		@return OpBoolean::IS_TRUE if size changed, OpBoolean::IS_FALSE if not, OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_BOOLEAN
					SetNewSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord width, LayoutCoord height, LayoutCoord modified_horizontal_border_padding);

#ifdef SVG_SUPPORT

	/** Get the SVGImage this IFrameContent contains, if any. */

	SVGImage*		GetNestedSVGImage();
#endif // SVG_SUPPORT

	BOOL			CalculateIntrinsicSizeForSVGFrame(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord &width, LayoutCoord &height, int &ratio);

public:
					IFrameContent()
					  : frame(NULL),
						iframe_packed_init(0) { }

					~IFrameContent();

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

	/** Paint iframe box on screen. */

	virtual OP_STATUS
					Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area);

#ifdef DOM_FULLSCREEN_MODE
	/** Paint iframe on screen as a fullscreen element. */

	virtual OP_STATUS
					PaintFullscreen(FramesDocument* doc, VisualDevice* visual_device, HTML_Element* elm, const RECT& area);
#endif // DOM_FULLSCREEN_MODE

	/** Create form, plugin and iframe objects */

	virtual OP_STATUS
					Enable(FramesDocument* doc);

	/** Remove form, plugin and iframe objects */

	virtual void	Disable(FramesDocument* doc);

	/** Is this iframe content? */

	virtual BOOL	IsIFrame() const { return TRUE; }

	/** Is there typically a CoreView associated with this content ?

		@see class CoreView */

	virtual BOOL	HasCoreView() const { return TRUE; }

	FramesDocElm*   GetFramesDocElm() { return frame; }

	/* Find out whether the frame has been loaded. */

	BOOL			IsFrameLoaded();

	BOOL			GetExpandIFrameHeight() { return iframe_packed.expand_iframe_height; }

	void			RestartIntrinsicWidthCalculation() { iframe_packed.second_pass_width = 0; }

#ifdef PAGED_MEDIA_SUPPORT
	/** Attempt to break page inside this iframe. */

	virtual BREAKST	AttemptPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain);
#endif // PAGED_MEDIA_SUPPORT
};

#ifdef ON_DEMAND_PLUGIN

class PluginPlaceholderContent : public IFrameContent
{
public:
					PluginPlaceholderContent(FramesDocument* doc, HTML_Element* elm)
					  : placeholder_doc(doc),
						placeholder_elm(elm),
						enable_on_demand_plugin_placeholder(FALSE)
					{
						placeholder_doc->RegisterPluginPlaceholder(placeholder_elm);
					}

	virtual			~PluginPlaceholderContent() { placeholder_doc->UnregisterPluginPlaceholder(placeholder_elm); }

	/** Paint plugin placeholder */

	virtual OP_STATUS
					Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area);

	/** Handles the mouse events to activate the plugin and show tooltip */

	OP_STATUS		HandleMouseEvent(FramesDocument* doc, DOM_EventType dom_event);

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

#ifdef LAYOUT_AUTO_ACTIVATE_HIDDEN_ON_DEMAND_PLACEHOLDERS
	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);
#endif // LAYOUT_AUTO_ACTIVATE_HIDDEN_ON_DEMAND_PLACEHOLDERS

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

private:
	FramesDocument*	placeholder_doc;
	HTML_Element*	placeholder_elm;

	BOOL			enable_on_demand_plugin_placeholder;
};

#endif // ON_DEMAND_PLUGIN

/** Form box. */

class FormContent
  : public ReplacedContent
{
private:

    BOOL			form_object_dirty;

protected:

	/** Distance from top edge to the baseline in this element. */

	LayoutCoord		baseline;

	/** Distance from top edge to the baseline in this element, ignoring any percentage in the computation. */

	LayoutCoord		nonpercent_baseline;

	/** Form object. */

	FormObject*		form_object;

	/**
	   any extra space needed for the widget to be drawn
	   correctly. this is sometimes needed when drawing check boxes
	   and radio buttons using skin painter, because common skins are
	   bigger than the default size.
	 */
	union
	{
		struct
		{
			unsigned int	left:8;
			unsigned int	top:8;
			unsigned int	right:8;
			unsigned int	bottom:8;
		} packed_used_margins;
		UINT32		packed_used_margins_init;

	};

	/** Show form. */

	virtual OP_STATUS
					ShowForm(LayoutProperties* cascade, FramesDocument* doc, LayoutCoord width, LayoutCoord height) = 0;

	/** Inform the form object about min-width and max-width. */

	virtual void	SetFormMinMaxWidth(short min_width, short max_width) {}

	/** Retain aspect ratio if ERA causes the element to become narrower? */

	virtual BOOL	RetainRatio() const { return FALSE; }

	/** Calculate intrinsic size.

		@param cascade The cascade for this element
		@param info Layout info
		@param width (out) Set to the intrinsic width of the content-box.
		@param height (out) Set to the intrinsic height of the content-box.
		@param ratio (out) Set to the intrinsic ratio of the content-box, in 14.16 fixed point format. */

	virtual OP_STATUS
					CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio);

	/** Update size of content.

		@param cascade The cascade for this element
		@param info Layout info
		@param width Border-box width of this element
		@param height Border-box height of this element

		@return OpBoolean::IS_TRUE if size changed, OpBoolean::IS_FALSE if not, OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_BOOLEAN
					SetNewSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord width, LayoutCoord height, LayoutCoord modified_horizontal_border_padding);

	/** Compute the baseline for form content. */

	void			ComputeBaseline(const HTMLayoutProperties& props);

	/** After a history travel, we need to recreate the object and
		restore the value saved in form_value. */

	OP_STATUS		RecreateFormObject(LayoutProperties* layout_props, FramesDocument* doc);

	/** Gets the position of the form object, to be passed to
		FormObject::Display or FormObject::SetPosition.  Only valid
		during traverse. */

	void			GetFormPosition(const HTMLayoutProperties& props, HTML_Element* elm, VisualDevice* visual_device, LayoutCoord& x, LayoutCoord& y);

	/** Restores a formobject that was stored with StoreFormObject */

	BOOL			RestoreFormObject(const HTMLayoutProperties& props, FramesDocument* doc);

public:

					FormContent()
					  : form_object_dirty(FALSE),
						baseline(0),
						nonpercent_baseline(0),
						form_object(NULL),
						packed_used_margins_init(0)
						{}

	virtual			~FormContent();

	/**
	   calls ReplacedContent::Layout(...) after adding any extra
	   space to bounding box of placeholder, if placeholder is a
	   BlockBox.
	 */
	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);
	/**
	   gets any extra space widget might need in order to be
	   drawn. this extra space is part of margin.
	   @param left (out) extra space used in left margin (leftwards from right edge of margin )
	   @param top (out) extra space used in top margin (upwards from bottom edge of margin)
	   @param right (out) extra space used in right margin (rightwards from left edge of margin)
	   @param bottom (out) extra space used in bottom margin (downwards from top edge of margin)
	 */
	void GetUsedMargins(UINT8& left, UINT8& top, UINT8& right, UINT8& bottom);

	/** Compute size of content and return IS_TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Stores the formobject in the HTML_Element (for backup during reflow)  */

	virtual void	StoreFormObject();

	/** Paint input box on screen. */

	virtual OP_STATUS
					Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area);

	/** Mark as disabled, will call Disable() at reflow */

	virtual void	MarkDisabled();

	/** Hides form content - does nothing but prevents against Disable() being called and FormObject being destroyed
	    when hiding form elements */

	virtual void	Hide(FramesDocument* doc);

	/** Remove form - unexternalizes form value */

	virtual void	Disable(FramesDocument* doc);

	/** Enable form - externalizes form value */

	virtual OP_STATUS
					Enable(FramesDocument *doc);

	/** Get form object. */

	virtual FormObject*
					GetFormObject() const { return form_object; }

	/** Scale border and padding (in ERA)? */

	virtual BOOL	ScaleBorderAndPadding() const { return FALSE; }

	/** Is this form content? */

	virtual BOOL	IsForm() const { return TRUE; }

	/** Is this content replaced content with baseline? */

	virtual BOOL	IsReplacedWithBaseline() const { return TRUE; }

	/** Get baseline of first or last line.

		@param last_line TRUE if last line baseline search (inline-block case).	*/

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const { return baseline; }

	/** Get the baseline if maximum width is satisfied. */

	virtual LayoutCoord
					GetMinBaseline(const HTMLayoutProperties& props) const { return nonpercent_baseline; }

	/** Remove cached info */

	virtual BOOL	RemoveCachedInfo();
};
inline void
FormContent::GetUsedMargins(UINT8& left, UINT8& top, UINT8& right, UINT8& bottom)
{
	left = packed_used_margins.left;
	top = packed_used_margins.top;
	right = packed_used_margins.right;
	bottom = packed_used_margins.bottom;
}

/** Input box.

	This includes text/password input fields, check boxes, radio buttons,
	submit buttons, etc.; anything defined as an INPUT element. */

class InputFieldContent
  : public FormContent
{
private:

	/** Show form. */

	virtual OP_STATUS
					ShowForm(LayoutProperties* cascade, FramesDocument* doc, LayoutCoord width, LayoutCoord height);

public:

	/** Remove form, plugin and iframe objects */

	virtual void	Disable(FramesDocument* doc);
};

/** Select box. */

class SelectContent
  : public FormContent
{
private:

	/** Array of option elements */

	HTML_Element**	options;

	/** Size of option element array */

	unsigned int	options_size;

	/** Number of options used */

	unsigned int	options_count;

	union
	{
		struct
		{
			/** Set when a message to invalidate the select box is pending. */

			unsigned int
					pending_invalidate:1;

			/** If lock_elements is TRUE no elements or groups will be
				added.	This should only be used during restore after
				a reflow. */

			unsigned int
					lock_elements:1;
		} select_packed;
		UINT32 select_packed_init;
	};

public:

	/** Set option content in selection object */

	OP_STATUS		SetOptionContent(HTML_Element* option, unsigned int index);

	/** Show form. */

	virtual OP_STATUS
					ShowForm(LayoutProperties* cascade, FramesDocument* doc, LayoutCoord width, LayoutCoord height);

protected:

	/** Inform the form object about min-width and max-width. */

	virtual void	SetFormMinMaxWidth(short min_width, short max_width) { const HTMLayoutProperties& props = *placeholder->GetCascade()->GetProps(); ((SelectionObject*) form_object)->SetMinMaxWidth(props.min_width, props.max_width); }

public:

					SelectContent()
					  : options(NULL),
					    options_size(0),
					    options_count(0),
					    select_packed_init(0) {}

					~SelectContent()	{	OP_DELETEA(options);
											REPORT_MEMMAN_DEC(options_size * sizeof(HTML_Element*));
										}

	virtual OP_STATUS
					Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area);

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc);

	/** Cast this object to selectcontent, if it is one. */

	virtual SelectContent*
					GetSelectContent() { return this; }

	/** Remove all options */

	void			RemoveOptions();

	/** Add option element to 'select' content */

	virtual OP_STATUS
					AddOption(HTML_Element* option_element, unsigned int &index);

	/** Remove option element from 'select' content */

	virtual void	RemoveOption(HTML_Element* option_element);

	/** Remove optiongroup element from 'select' content */

	virtual void	RemoveOptionGroup(HTML_Element* optiongroup_element);

	/** Begin optiongroup in 'select' content */

	virtual void	BeginOptionGroup(HTML_Element* optiongroup_element);

	/** End optiongroup in 'select' content */

	virtual void	EndOptionGroup(HTML_Element* optiongroup_element);

	/** Layout option element in 'select' content */

	virtual OP_STATUS
					LayoutOption(LayoutProperties* cascade, LayoutWorkplace* workplace, unsigned int index);

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Should this content layout its children? */

	virtual BOOL	ShouldLayoutChildren() { return TRUE; }
};

/** Textarea box. */

class TextareaContent
  : public FormContent
{
private:

	/** Show form. */

	virtual OP_STATUS
					ShowForm(LayoutProperties* cascade, FramesDocument* doc, LayoutCoord width, LayoutCoord height);

protected:

	/** Allow zero size? */

	virtual BOOL	AllowZeroSize() const { return TRUE; }

public:

	/** Lay out form content. */

	virtual OP_STATUS
					LayoutFormContent(LayoutProperties* cascade, LayoutWorkplace* workplace);

	/** Is value changed by user since last set. */

	BOOL			IsModified();

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Should this content layout its children? */

	virtual BOOL	ShouldLayoutChildren() { return TRUE; }
};

/** Inline content. */

class InlineContent
  : public Content
{
private:

	union
	{
		struct
		{
			/** Width of box. FIXME: Does this really have to be a signed integer? */

			signed long
					width:31;

			/** Pending reflow? */

			unsigned int
					needs_reflow:1;
		} inline_packed;
		UINT32 inline_packed_init;
	};

protected:

	/** Has reflow been forced? */

	virtual BOOL	GetReflowForced() const { return inline_packed.needs_reflow; }

	/** Set reflow force status, which will be checked and reset during SignalChange(). */

	virtual void	SetReflowForced(BOOL force_reflow_on) { inline_packed.needs_reflow = !!force_reflow_on; }

	/** Get min/max width. */

	virtual BOOL	GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const;

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth();

public:

					InlineContent() { inline_packed_init = 0; }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Get height of border box. */

	virtual LayoutCoord
					GetHeight() const { return LayoutCoord(0); }

	/** Get minimum height (height if maximum width is satisfied) */

	virtual LayoutCoord
					GetMinHeight() const { OP_ASSERT(!"Makes no sense to ask content of this type about such things"); return LayoutCoord(0); }

	/** Get baseline of first or last line.

		@param last_line TRUE if last line baseline search (inline-block case).	*/

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const { OP_ASSERT(!"Makes no sense to ask content of this type about such things"); return LayoutCoord(0); }

	/** Get the baseline if maximum width is satisfied. */

	virtual LayoutCoord
					GetMinBaseline(const HTMLayoutProperties& props) const { return LayoutCoord(0); }

	/** Is this content inline content? */

	virtual BOOL	IsInlineContent() const { return TRUE; }

	/** Get width of border box. */

	virtual LayoutCoord
					GetWidth() const { return LayoutCoord(inline_packed.width); }

	/** Grow width of inline box. */

	virtual void	GrowInlineWidth(LayoutCoord increment) { inline_packed.width += increment; }
};

#endif // _CONTENT_H_
