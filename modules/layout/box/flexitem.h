/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef FLEXITEM_H
#define FLEXITEM_H

#include "modules/layout/box/blockbox.h"

class FlexItemList;

/** Layout box for a flex item; child of a flexbox (also known as flex container).

	This class is used to represent all kinds of flex items, no matter whether
	it's relatively positioned, establishes a stacking context, or what not. */

class FlexItemBox
	: public VerticalBox,
	  private Link // Used for logical sorting; see flexorder_prev and flexorder_next for 'flex-order' sorting.
{
private:

	/** Previous item, in flex order. */

	FlexItemBox*	flexorder_prev;

	/** Next item, in flex order. */

	FlexItemBox*	flexorder_next;

	/** Space manager for allocating space.

		All flex items establish a new block formatting context. */

	SpaceManager	space_manager;

	/** Z element for stacking context.

		Only used if flexitem_packed.has_z_element is set. */

	ZElement		z_element;

	/** Ordered list of elements with z-index different from 0 and 'auto', etc.

		Only used if flexitem_packed.has_stacking_context is set. */

	StackingContext	stacking_context;

	/** Used 'left' (if relatively positioned). */

	LayoutCoord		left;

	/** Used 'top' (if relatively positioned). */

	LayoutCoord		top;

	/** Used 'margin-left'. */

	LayoutCoord		margin_left;

	/** Used 'margin-right'. */

	LayoutCoord		margin_right;

	/** Used 'margin-top'. */

	LayoutCoord		margin_top;

	/** Used 'margin-bottom'. */

	LayoutCoord		margin_bottom;

	/** Sum of used size of main axis border and padding. */

	LayoutCoord		main_border_padding;

	/** Sum of used size of cross axis border and padding. */

	LayoutCoord		cross_border_padding;

	/** Used 'min-width' (for horizontal items) or 'min-height' (for vertical items). */

	LayoutCoord		min_main_size;

	/** Used 'max-width' (for horizontal items) or 'max-height' (for vertical items). */

	LayoutCoord		max_main_size;

	/** Used 'min-height' (for horizontal items) or 'min-width' (for vertical items). */

	LayoutCoord		min_cross_size;

	/** Used 'max-height' (for horizontal items) or 'max-width' (for vertical items). */

	LayoutCoord		max_cross_size;

	/** Used value of misc. flexbox properties. */

	FlexboxModes	modes;

	/** Preferred main size.

		This is <preferred-size> from the 'flex' property, unless it is 'auto',
		in which case this is the 'width' or 'height' value. */

	LayoutCoord		preferred_main_size;

	/** Computed cross size.

		This is the computed value of the 'height' (horizontal flexbox) or
		'width' (vertical flexbox) property. */

	LayoutCoord		computed_cross_size;

	/** Minimum height of margin box.

		This is the height that the item needs for its contents not to
		overflow. */

	LayoutCoord		minimum_margin_height;

	/** Hypothetical height of margin box.

		This is the height that the item will have if it isn't stretched or
		flexed. */

	LayoutCoord		hypothetical_margin_height;

	/** X position of box (left border edge). */

	LayoutCoord		x;

	/** Y position of box (top border edge). */

	LayoutCoord		y;

	/** New main axis start margin edge static position (for upcoming reflow). */

	LayoutCoord		new_main_margin_edge;

	/** New main axis start margin edge static position (for upcoming reflow). */

	LayoutCoord		new_cross_margin_edge;

	/** New main axis margin box size (for upcoming reflow).

		This is the margin box width for items in horizontal flexboxes, while
		it's the margin box height for items in vertical flexboxes.

		Note that this doesn't include auto margins. */

	LayoutCoord		new_main_margin_size;

	/** New cross axis margin box size (for upcoming reflow).

		This is the margin box height for items in horizontal flexboxes, while
		it's the margin box width for items in vertical flexboxes.

		Note that this doesn't include auto margins. */

	LayoutCoord		new_cross_margin_size;

	/** Used value of the 'flex-grow' property. */

	float			flex_grow;

	/** Used value of the 'flex-shrink' property. */

	float			flex_shrink;

	/** Ordinal group ('order' property) for this item.

		The lower the value (compared to the other items), the closer to the
		start of the container will this item be put. In addtion to affecting
		the position of the item, it also affects painting order. */

	int				order;

	union
	{
		struct
		{
			/** Set if this is a relatively positioned box. */

			unsigned int
					is_positioned:1;

			/** Set if the z_element member is to be used. */

			unsigned int
					has_z_element:1;

			/** Set if the stacking_context member is to be used. */

			unsigned int
					has_stacking_context:1;

			/** Set if box-sizing is border-box. Not set if box-sizing is content-box. */

			unsigned int
					border_box_sizing:1;

			/** Set if margin-left is auto. */

			unsigned int
					margin_left_auto:1;

			/** Set if margin-right is auto. */

			unsigned int
					margin_right_auto:1;

			/** Set if margin-top is auto. */

			unsigned int
					margin_top_auto:1;

			/** Set if margin-bottom is auto. */

			unsigned int
					margin_bottom_auto:1;

			/** Flex violation (if flexing caused min/max-width/height constraints violation).

				None, negative or positive. */

			signed int
					violation:2;

			/** Set if the cross-size of this item should be stretched (align-self:stretch). */

			unsigned int
					stretch:1;

			/** Set if visibility:collapse is specified on this item. */

			unsigned int
					visibility_collapse:1;

			/** Set if this item lives in a vertical flexbox. */

			unsigned int
					vertical:1;

			/** TRUE if this flex item lives in a paged or multicol container. */

			unsigned int
						in_multipane:1;

#ifdef PAGED_MEDIA_SUPPORT

			/** Page break after this item. */

			unsigned int
						trailing_page_break:1;

			/** Pending break on this item.

				We need this when inserting an implicit page break, in order to
				know where to continue page breaking in the next reflow
				pass. */

			unsigned int
						page_break_pending:1;

#endif // PAGED_MEDIA_SUPPORT

			/** Page BREAK_POLICY before this item. */

			unsigned int
						page_break_before:3;

			/** Page BREAK_POLICY after this item. */

			unsigned int
						page_break_after:3;

			/** Column BREAK_POLICY before this item. */

			unsigned int
						column_break_before:2;

			/** Column BREAK_POLICY after this item. */

			unsigned int
						column_break_after:2;

		} flexitem_packed;
		UINT32		flexitem_packed_init;
	};

protected:

	/** Restart z element list. */

	virtual void	RestartStackingContext() { if (flexitem_packed.has_stacking_context) stacking_context.Restart(); }

public:

					FlexItemBox(HTML_Element* element, Content* content);

	virtual			~FlexItemBox();

	/** Get the next item in the list (logical order or flex order). */

	FlexItemBox*	Suc(BOOL logical) const { return logical ? (FlexItemBox*) Link::Suc() : flexorder_next; }

	/** Get the previous item in the list (logical order or flex order). */

	FlexItemBox*	Pred(BOOL logical) const { return logical ? (FlexItemBox*) Link::Pred() : flexorder_prev; }

	/** Take this item out from the layout stack. */

	void			Out();

	/** Add this item to a layout stack.

		This will insert it into both a logical list and a display order list
		(based on 'order').

		@param layout_stack The layout stack */

	void			Into(FlexItemList* layout_stack);

	/** Is this box a flex item? */

	virtual BOOL	IsFlexItemBox() const { return TRUE; }

	/** Return the space manager of the object, if it has any. */

	virtual SpaceManager*
					GetLocalSpaceManager() { return &space_manager; }

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return flexitem_packed.has_z_element ? &z_element : NULL; }

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return flexitem_packed.has_stacking_context ? &stacking_context : NULL; }

	/** Is this box a positioned box? */

	virtual BOOL	IsPositionedBox() const { return flexitem_packed.is_positioned; }

	/** Is this box (and potentially also its children) columnizable?

		A box may only be columnizable if it lives in a paged or multicol container.

		A descendant of a box that isn't columnizable can never be columnizable (unless
		there's a new paged or multicol container between that box and its descendant).

		Table-rows, table-captions, table-row-groups and block-level descendant boxes of
		a paged or multicol container are typically columnizable. Being columnizable
		means that they need to be taken into account by the ancestor paged or multicol
		container somehow. They may become a start or stop element of a column or
		page. It may also be possible to split them over multiple columns or pages, but
		that depends on details about the box type, and that is where the
		"require_children_columnizable" parameter comes into play.

		@param require_children_columnizable If TRUE, only return TRUE if this box is of
		such a type that not only the box itself is columnizable, but also that it
		doesn't prevent its children from becoming columnizable. A table-row, block with
		scrollbars, absolutely-positioned box or float prevents children from being
		columnizable (FALSE will be returned it this parameter is TRUE), but the box
		itself may be very well be columnizable. Example: an absolutely positioned box
		may live inside of a paged or multicol container (so that its Y position is
		stored as a virtual Y position within the paged or multicol container), but its
		descendants do not have to worry about the multi-columnness or multi-pagedness.

		@return TRUE if this box (and possibly its children too) is columnizable. Return
		FALSE if this box isn't columnizable, or if require_children_columnizable is set
		and the children aren't columnizable. */

	virtual BOOL	IsColumnizable(BOOL require_children_columnizable = FALSE) const;

	/** Return TRUE if this is a special anonymous flex item for an absolutely positioned child. */

	BOOL			IsAbsolutePositionedSpecialItem() const;

	/** Set page and column breaking policies on this flex item box. */

	void			SetBreakPolicies(BREAK_POLICY page_break_before, BREAK_POLICY column_break_before,
									 BREAK_POLICY page_break_after, BREAK_POLICY column_break_after);

	/** Get page break policy before this layout element. */

	BREAK_POLICY	GetPageBreakPolicyBefore() const { return (BREAK_POLICY) flexitem_packed.page_break_before; }

	/** Get page break policy after this layout element. */

	BREAK_POLICY	GetPageBreakPolicyAfter() const { return (BREAK_POLICY) flexitem_packed.page_break_after; }

	/** Get column break policy before this layout element. */

	BREAK_POLICY	GetColumnBreakPolicyBefore() const { return (BREAK_POLICY) flexitem_packed.column_break_before; }

	/** Get column break policy after this layout element. */

	BREAK_POLICY	GetColumnBreakPolicyAfter() const { return (BREAK_POLICY) flexitem_packed.column_break_after; }

	/** Steal and return page break policy before this layout element. */

	BREAK_POLICY	StealPageBreakPolicyBefore() { BREAK_POLICY policy = (BREAK_POLICY) flexitem_packed.page_break_before; flexitem_packed.page_break_before = BREAK_ALLOW; return policy; }

	/** Steal and return page break policy after this layout element. */

	BREAK_POLICY	StealPageBreakPolicyAfter() { BREAK_POLICY policy = (BREAK_POLICY) flexitem_packed.page_break_after; flexitem_packed.page_break_after = BREAK_ALLOW; return policy; }

	/** Steal and return column break policy before this layout element. */

	BREAK_POLICY	StealColumnBreakPolicyBefore() { BREAK_POLICY policy = (BREAK_POLICY) flexitem_packed.column_break_before; flexitem_packed.column_break_before = BREAK_ALLOW; return policy; }

	/** Steal and return column break policy after this layout element. */

	BREAK_POLICY	StealColumnBreakPolicyAfter() { BREAK_POLICY policy = (BREAK_POLICY) flexitem_packed.column_break_after; flexitem_packed.column_break_after = BREAK_ALLOW; return policy; }

	/** Get flexed (horizontal flexbox) or stretched (vertical flexbox) border box width. */

	LayoutCoord		GetFlexWidth(LayoutProperties* cascade) const;

	/** Get flexed (vertical flexbox) or stretched (horizontal flexbox) border box height.

		Returns CONTENT_HEIGHT_AUTO if not applicable. It is "not applicable"
		if this box is a horizontal item that shouldn't stretch. */

	LayoutCoord		GetFlexHeight(LayoutProperties* cascade) const;

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Propagate bottom margins and bounding box to parents, and walk the dog. */

	virtual void	PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* bottom_margin = 0, BOOL has_inflow_content = TRUE);

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

	/** Invalidate the screen area that the box uses. */

	virtual void	Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const;

	/** Do we need to calculate min/max widths of this box's content? */

	virtual BOOL	NeedMinMaxWidthCalculation(LayoutProperties* cascade) const;

	/** Propagate widths to flexbox. */

	virtual void	PropagateWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width);

	/** Propagate minimum and hypothetical border box heights. */

	void			PropagateHeights(LayoutCoord min_border_height, LayoutCoord hyp_border_height);

	/** Propagate the 'order' property from an absolutely positioned child. */

	void			PropagateOrder(int child_order);

	/** Get baseline of flex item. */

	LayoutCoord		GetItemBaseline() const { return content->GetBaseline(); }

	/** Get the baseline this flexitem will have if maximum width is satisfied. */

	LayoutCoord		GetItemMinBaseline(const HTMLayoutProperties& props) const { return content->GetMinBaseline(props); }

	/** Traverse box with children. */

	void			Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops);

	/** Get X position; left border edge relative to parent flexbox's left border edge. */

	LayoutCoord		GetX() const { return x; }

	/** Get Y position; top border edge relative to parent flexbox's top border edge. */

	LayoutCoord		GetY() const { return y; }

	/** Get static Y position of margin box, relative to parent flexbox's top border edge. */

	LayoutCoord		GetStaticMarginY() const { return y - top - margin_top; }

	/** Get 'box-sizing' property. */

	CSSValue		GetBoxSizing() const { return flexitem_packed.border_box_sizing ? CSS_VALUE_border_box : CSS_VALUE_content_box; }

	/** Get computed cross size. */

	LayoutCoord		GetComputedCrossSize() const { return computed_cross_size; }

	/** Get preferred main-axis border box size.

		This is based on the 'preferred-size' property, unless it is 'auto', in
		which case it is based on the 'width' or 'height' value. This is the
		"computed" value, meaning that percentages or auto sizes have not yet
		been resolved. */

	LayoutCoord		GetPreferredMainBorderSize() const
	{
		if (preferred_main_size >= 0 && GetBoxSizing() == CSS_VALUE_content_box)
			return preferred_main_size + main_border_padding;
		else
			return preferred_main_size;
	}

	/** Get flex base size (main axis margin box size).

		This is based on the value of the 'flex-basis' property, unless it is
		'auto', in which case this is the 'width' or 'height' value (whatever
		the main axis is). If the main axis size also is auto, use the maximum
		size calculated from contents.

		If the size is a percentage, it will be resolved against
		'containing_block_size'. If that value is auto, the main size will be
		treated as auto (and the maximum size calculated from contents will be
		used; see previous paragraph).

		@param containing_block_size Size of containing block, or
		CONTENT_SIZE_AUTO if auto. */

	LayoutCoord		GetFlexBaseSize(LayoutCoord containing_block_size);

	/** Get hypothetical height of margin box.

		This is the height that the item will have if it isn't stretched or
		flexed. */

	LayoutCoord		GetHypotheticalMarginBoxHeight() const { return hypothetical_margin_height; }

	/** Get positive flex (used value of the 'flex-grow' property).

		This indicates this item's willingness to increase in size (from
		preferred size). It comprises a fraction of the total positive flex
		among all items, when excess flexbox space is to be distributed. */

	float			GetFlexGrow() const { return flex_grow; }

	/** Get positive flex (used value of the 'flex-shrink' property).

		This indicates this item's willingness to decrease in size (from
		preferred size). It comprises a fraction of the total negative flex
		among all items, when an insufficient flexbox size situation is to be
		resolved. */

	float			GetFlexShrink() const { return flex_shrink; }

	/** Get the ordinal group ('order' property) to which this item belongs. */

	int				GetOrder() const { return order; }

	/** Set new main axis start margin edge static position (for upcoming reflow). */

	void			SetNewMainMarginEdge(LayoutCoord edge) { new_main_margin_edge = edge; }

	/** Set new cross axis start margin edge static position (for upcoming reflow). */

	void			SetNewCrossMarginEdge(LayoutCoord edge) { new_cross_margin_edge = edge; }

	/** Set new main axis margin box size (for upcoming reflow). */

	void			SetNewMainMarginSize(LayoutCoord size) { new_main_margin_size = size; }

	/** Set new cross axis margin box size (for upcoming reflow). */

	void			SetNewCrossMarginSize(LayoutCoord size) { new_cross_margin_size = size; }

	/** Get new main axis start margin edge static position (for upcoming reflow). */

	LayoutCoord		GetNewMainMarginEdge() const { return new_main_margin_edge; }

	/** Get new cross axis start margin edge static position (for upcoming reflow). */

	LayoutCoord		GetNewCrossMarginEdge() const { return new_cross_margin_edge; }

	/** Get new main axis margin box size (for upcoming reflow). */

	LayoutCoord		GetNewMainMarginSize() const { return new_main_margin_size; }

	/** Get new cross axis margin box size (for upcoming reflow). */

	LayoutCoord		GetNewCrossMarginSize() const { return new_cross_margin_size; }

	/** Get new static Y position of margin box (for upcoming reflow). */

	LayoutCoord		GetNewStaticMarginY(FlexContent* flexbox) const { LayoutCoord dummy, y; CalculateVisualMarginPos(flexbox, dummy, y); return y; }

	/** Calculate real coordinates (values for upcoming reflow).

		This will convert from main/cross/reverse mumbo-jumbo stuff to left and
		top margin box values, relative to the border box of the flex
		container. */

	void			CalculateVisualMarginPos(FlexContent* flexbox, LayoutCoord& x, LayoutCoord& y) const;

	/** Push new static Y position of margin box (for upcoming reflow) downwards. */

	void			MoveNewStaticMarginY(FlexContent* flexbox, LayoutCoord amount);

	/** Constrain the specified main size to min/max-width/height and return that value. */

	LayoutCoord		GetConstrainedMainSize(LayoutCoord margin_box_size) const;

	/** Constrain the specified cross size to min/max-height/width and return that value. */

	LayoutCoord		GetConstrainedCrossSize(LayoutCoord margin_box_size) const;

	/** Set flex violation (if flexing caused min/max-width/height constraints violation).

		@param violation None, negative or positive. */

	void			SetViolation(int violation) { flexitem_packed.violation = MIN(1, MAX(-1, violation)); }

	/** Get flex violation (if flexing caused min/max-width/height constraints violation).

		@return None, negative or positive. */

	int				GetViolation() const { return flexitem_packed.violation; }

	/** Given the specified violation sign (positive, negative, zero), is this item violated? */

	BOOL			IsViolated(int sign) const { return sign < 0 ? GetViolation() < 0 : sign > 0 ? GetViolation() > 0 : FALSE; }

	/** Get used value of margin-left. 0 if auto. */

	LayoutCoord		GetMarginLeft() const { return margin_left; }

	/** Get used value of margin-right. 0 if auto. */

	LayoutCoord		GetMarginRight() const { return margin_right; }

	/** Get used value of margin-top. 0 if auto. */

	LayoutCoord		GetMarginTop() const { return margin_top; }

	/** Get used value of margin-bottom. 0 if auto. */

	LayoutCoord		GetMarginBottom() const { return margin_bottom; }

	/** Is margin-left auto? */

	BOOL			IsMarginLeftAuto() const { return flexitem_packed.margin_left_auto; }

	/** Is margin-right auto? */

	BOOL			IsMarginRightAuto() const { return flexitem_packed.margin_right_auto; }

	/** Is margin-top auto? */

	BOOL			IsMarginTopAuto() const { return flexitem_packed.margin_top_auto; }

	/** Is margin-bottom auto? */

	BOOL			IsMarginBottomAuto() const { return flexitem_packed.margin_bottom_auto; }

	/** Get sum of used size of cross axis border and padding. */

	LayoutCoord		GetCrossBorderPadding() const { return cross_border_padding; }

	/** Get used value of misc. flexbox properties. */

	FlexboxModes	GetFlexboxModes() const { return modes; }

	/** Return TRUE if the cross-size of this item should be stretched (align-self:stretch). */

	BOOL			AllowStretch() const { return flexitem_packed.stretch; }

	/** Return TRUE if the 'visibility' property is set to 'collapse'. */

	BOOL			IsVisibilityCollapse() const { return flexitem_packed.visibility_collapse; }

	/** Return TRUE if this block is inside of a paged or multicol container. */

	BOOL			IsInMultiPaneContainer() const { return flexitem_packed.in_multipane == 1; }

#ifdef PAGED_MEDIA_SUPPORT

	/** Add a trailing page break after this item. */

	void			AddTrailingPageBreak() { flexitem_packed.trailing_page_break = 1; }

	/** Add an implicit page break after this item.

		Implicit page breaks may affect elements already that have already been
		laid out, so we need to mark the break as pending, and resume page
		breaking at this point in the next reflow pass. */

	void			AddTrailingImplicitPageBreak() { flexitem_packed.page_break_pending = 1; AddTrailingPageBreak(); }

	/** Reset the flag and return TRUE if this is where the pending page break was. */

	BOOL			FindPendingPageBreak()
	{
		if (flexitem_packed.page_break_pending)
		{
			flexitem_packed.page_break_pending = 0;
			OP_ASSERT(flexitem_packed.trailing_page_break);
			return TRUE;
		}

		return FALSE;
	}

	/** Return TRUE if there should be a page break after this item. */

	BOOL			HasTrailingPageBreak() const { return flexitem_packed.trailing_page_break; }

	/** There should no longer be a page break after this item. */

	void			RemoveTrailingPageBreak() { flexitem_packed.trailing_page_break = 0; flexitem_packed.page_break_pending = 0; }

#endif // PAGED_MEDIA_SUPPORT
};

#ifdef CSS_TRANSFORMS
TRANSFORMED_BOX(FlexItemBox);
#endif // CSS_TRANSFORMS

#endif // FLEXITEM_H
