/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** @file content.cpp
 *
 * Content class implementation for document layout.
*/

#include "core/pch.h"

#include "modules/layout/content/content.h"
#include "modules/layout/content/flexcontent.h"
#include "modules/layout/box/box.h"
#include "modules/layout/box/blockbox.h"
#include "modules/layout/box/flexitem.h"
#include "modules/layout/box/inline.h"
#include "modules/layout/box/tables.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/probetools/probepoints.h"
#include "modules/layout/internal.h"
#include "modules/url/url_api.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/style/css_media.h"
#include "modules/logdoc/attr_val.h"
#include "modules/logdoc/applet_attr.h"
#include "modules/dochand/win.h"
#include "modules/spatial_navigation/handler_api.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpPluginWindow.h"
#include "modules/windowcommander/src/WindowCommander.h"

#include "modules/forms/formmanager.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/formvalue.h"
#include "modules/forms/piforms.h"
#include "modules/forms/pi_forms_listeners.h"

#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpScrollbar.h"
#include "modules/widgets/OpButton.h"

#include "modules/display/VisDevListeners.h"
#include "modules/img/image.h"

#include "modules/display/prn_dev.h"
#include "modules/display/styl_man.h"
#include "modules/doc/html_doc.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#ifdef _PLUGIN_SUPPORT_
# include "modules/ns4plugins/opns4pluginhandler.h"
# include "modules/ns4plugins/opns4plugin.h"
# ifdef QUICK // produces many compilation errors otherwise !
#  include "adjunct/quick/dialogs/AskPluginDownloadDialog.h"
# endif // QUICK
#endif // _PLUGIN_SUPPORT_

#ifdef MEDIA_HTML_SUPPORT
#include "modules/media/mediaelement.h"
#endif // MEDIA_ELEMENT

#ifdef SUPPORT_VISUAL_ADBLOCK
# include "modules/windowcommander/src/WindowCommander.h"
#endif // SUPPORT_VISUAL_ADBLOCK

#include "modules/hardcore/mh/messages.h"

#include "modules/dochand/winman.h"
#include "modules/dochand/fdelm.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"

#include "modules/viewers/viewers.h"

#ifdef _WML_SUPPORT_
# include "modules/logdoc/wml.h"
#endif // _WML_SUPPORT_

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
# include "modules/dom/domenvironment.h"
# include "modules/svg/svg_image.h"
#endif // SVG_SUPPORT

#ifdef DOCUMENT_EDIT_SUPPORT
# include "modules/documentedit/OpDocumentEdit.h"
#endif

#include "modules/forms/webforms2number.h"
#include "modules/forms/webforms2support.h"
#include "modules/forms/formvalue.h"

#include "modules/layout/layout_workplace.h"

#ifdef SKIN_SUPPORT
# include "modules/skin/OpSkinManager.h"
#endif // SKIN_SUPPORT

#ifdef WIC_USE_ASK_PLUGIN_DOWNLOAD
# include "modules/prefs/prefsmanager/collections/pc_ui.h"
#endif // WIC_USE_ASK_PLUGIN_DOWNLOAD

#ifdef MEDIA_SUPPORT
# include "modules/media/media.h"
#endif //MEDIA_SUPPORT

#ifdef CANVAS_SUPPORT
# include "modules/libvega/src/canvas/canvas.h"
#endif // CANVAS_SUPPORT

#ifdef SUPPORT_VISUAL_ADBLOCK
# define ADBLOCK_EFFECT_BOX_HEIGHT   20
# define ADBLOCK_FONT_HEIGHT         16
# define ADBLOCK_BG_PLUGIN_R         0xDD
# define ADBLOCK_BG_PLUGIN_G         0xDD
# define ADBLOCK_BG_PLUGIN_B         0xDD
# define ADBLOCK_BG_BOX_R            0xFF
# define ADBLOCK_BG_BOX_G            0x33
# define ADBLOCK_BG_BOX_B            0x33
#endif // SUPPORT_VISUAL_ADBLOCK

/** Constrain dimensions to min-width, max-width, min-height, max-height.

	The width and height parameters are in/out values, and they represents the width and height of the content-box. */

static void CheckSizeBounds(const HTMLayoutProperties& props, LayoutCoord &width, LayoutCoord& height, LayoutCoord horizontal_border_padding, LayoutCoord vertical_border_padding)
{
	if (props.box_sizing == CSS_VALUE_border_box)
	{
		width += horizontal_border_padding;
		height += vertical_border_padding;
	}

	width = props.CheckWidthBounds(width);
	height = props.CheckHeightBounds(LayoutCoord(height));

	if (props.box_sizing == CSS_VALUE_border_box)
	{
		if (width > horizontal_border_padding)
			width -= horizontal_border_padding;
		else
			width = LayoutCoord(0);

		if (height > vertical_border_padding)
			height -= vertical_border_padding;
		else
			height = LayoutCoord(0);
	}
}

static inline void CheckContentBoxWidthBounds(const HTMLayoutProperties& props, LayoutCoord &width, LayoutCoord horizontal_border_padding, BOOL ignore_percent)
{
	if (props.box_sizing == CSS_VALUE_border_box)
		width += horizontal_border_padding;

	width = props.CheckWidthBounds(width, ignore_percent);

	if (props.box_sizing == CSS_VALUE_border_box)
		width -= MIN(horizontal_border_padding, width);
}

static inline void CheckContentBoxHeightBounds(const HTMLayoutProperties& props, LayoutCoord &height, LayoutCoord vertical_border_padding, BOOL ignore_percent)
{
	if (props.box_sizing == CSS_VALUE_border_box)
		height += vertical_border_padding;

	height = props.CheckHeightBounds(height, ignore_percent);

	if (props.box_sizing == CSS_VALUE_border_box)
		height -= MIN(vertical_border_padding, height);
}

/** Convert from border-box to the box-sizing used by the forms code (typically padding-box or border-box) */

static void BorderBoxToFormBox(LayoutProperties* cascade, LayoutCoord& width, LayoutCoord& height)
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	if (FormObject::HasSpecifiedBorders(props, cascade->html_element))
	{
		// This means that borders are drawn by layout code, not by widget code, so subtract them.

		OP_ASSERT(width >= props.border.left.width + props.border.right.width);
		OP_ASSERT(height >= (props.border.top.width + props.border.bottom.width));

		width -= LayoutCoord(props.border.left.width + props.border.right.width);
		height -= LayoutCoord(props.border.top.width + props.border.bottom.width);
	}
}

/** Convert from the box-sizing used by the forms code (typically padding-box or border-box) to content-box */

static void FormBoxToContentBox(LayoutProperties* cascade, LayoutCoord& width, LayoutCoord& height)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord horizontal_border_padding = props.padding_left + props.padding_right;
	LayoutCoord vertical_border_padding = props.padding_top + props.padding_bottom;

	if (!FormObject::HasSpecifiedBorders(props, cascade->html_element))
	{
		horizontal_border_padding += LayoutCoord(props.border.left.width + props.border.right.width);
		vertical_border_padding += LayoutCoord(props.border.top.width + props.border.bottom.width);
	}

	if (width > horizontal_border_padding)
		width -= horizontal_border_padding;

	if (height > vertical_border_padding)
		height -= vertical_border_padding;
}

/** Determine what part of the image is ready for display.
 *  @return TRUE if the image should be displayed */

static BOOL SetDisplayRects(Image& img, OpRect& src, OpRect& dst, OpRect& fit)
{
	if (img.IsFailed())
		return FALSE;

	if (img.Width() && img.Height())
	{
		src.y = 0; // used for bottom-to-top images.
		src.height = 0;

		if (img.IsBottomToTop() && img.GetLowestDecodedLine() > 1)
		{
			// For bottom-to-top images, calculate top edges and heights using the bottom edge as the reference.
			src.y = img.GetLowestDecodedLine()-1;
			src.height = img.Height() - src.y;
		}
		else if (img.GetLastDecodedLine() != 0)
			// For top-to-bottom images, simply use the last decoded line.
			src.height = img.GetLastDecodedLine();
		else
			return FALSE;

		// Scale the rendered image height according to how far it is decoded.
		INT32 loaded_offset = src.y * (int)fit.height / img.Height();			// For bottom-to-top images, also scale the top.
		fit.height = fit.height * src.height / img.Height();

		src.Set(0, src.y, img.Width(), src.height);
		dst.Set(fit.x, fit.y + loaded_offset, fit.width, fit.height);
		return TRUE;
	}
	else
		return FALSE;
}

/** Return TRUE if the specified HTML element lives in a floated pane box subtree that lives in the specified column. */

static BOOL
IsInFloatedPaneBoxTree(HTML_Element* html_element, Column* column)
{
	if (!column->GetFirstFloatEntry())
		return FALSE;

	do
	{
		Box* box = html_element->GetLayoutBox();

		if (box->IsFloatedPaneBox())
			return column->HasFloat((FloatedPaneBox*) box);

		if (Container* inner_container = box->GetContainer())
			if (inner_container->IsMultiPaneContainer())
				return FALSE;

		html_element = html_element->Parent();
	}
	while (html_element);

	return FALSE;
}

Container::Container()
  :
#ifdef SUPPORT_TEXT_DIRECTION
	bidi_segments(NULL),
#endif
	height(0),
	min_height(0),
	minimum_width(0),
	normal_minimum_width(0),
	maximum_width(0),
	reflow_state(NULL)
{
	packed_init = 0;
}

/* virtual */
Container::~Container()
{
	VerticalLayout* vertical_layout;

#ifdef SUPPORT_TEXT_DIRECTION
	if (bidi_segments)
	{
		bidi_segments->Clear();
		OP_DELETE(bidi_segments);
	}
#endif

	while ((vertical_layout = (VerticalLayout*) layout_stack.First()) != 0)
	{
		vertical_layout->Out();

		if (!vertical_layout->IsBlock())
			OP_DELETE(vertical_layout);
	}

	delete reflow_state;
}

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

/** Signal that an HTML_Element in this container is about to be deleted. */

void
Container::SignalChildDeleted(FramesDocument* doc, Box* child)
{
	packed.only_traverse_blocks = 1;
}

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

/** Get baseline of first or last line. */

LayoutCoord
Container::LocalGetBaseline(BOOL last) const
{
	for (VerticalLayout* vertical_layout = static_cast<VerticalLayout*>(last ? layout_stack.Last() : layout_stack.First());
		 vertical_layout;
		 vertical_layout = last ? vertical_layout->Pred() : vertical_layout->Suc())
	{
		if (vertical_layout->IsInStack())
		{
			Content *content = vertical_layout->IsBlock() ? static_cast<BlockBox*>(vertical_layout)->GetContent() : NULL;

			if (content && content->IsReplaced())
				continue; //skip blocks with replaced content - we're searching for a line

			LayoutCoord baseline = vertical_layout->GetBaseline(last);

			if (baseline != LAYOUT_COORD_MIN)
				return vertical_layout->GetStackPosition() + baseline;
		}

		// Skip layout elements for which this container isn't the containing block.
	}

	return LAYOUT_COORD_MIN;
}

/** Get baseline of first or last line.

	@param last_line TRUE if last line baseline search (inline-block case).	*/

/* virtual */ LayoutCoord
Container::GetBaseline(BOOL last_line /*= FALSE*/) const
{
	OP_ASSERT(!placeholder->IsInlineBox());

	return LocalGetBaseline(last_line);
}

/** Get baseline of inline block or content of inline element that
	has a baseline (e.g. ReplacedContent with baseline, BulletContent). */

/* virtual */ LayoutCoord
Container::GetBaseline(const HTMLayoutProperties& props) const
{
	OP_ASSERT(placeholder->IsInlineBlockBox());

	/* For inline-blocks, the baseline is the baseline of the last in-flow
	   inline in the container. If there are no lines, or if overflow is not
	   visible, however, the baseline is the bottom margin edge. */

	if (props.overflow_x == CSS_VALUE_visible)
	{
		LayoutCoord baseline = LocalGetBaseline(TRUE);
		if (baseline != LAYOUT_COORD_MIN)
			return baseline;
	}

	return height + props.margin_bottom;
}

/** Get the baseline if maximum width is satisfied. */

/* virtual */ LayoutCoord
Container::GetMinBaseline(const HTMLayoutProperties& props) const
{
	// This is not correct, but hopefully good enough for now.

	return min_height;
}

#ifdef MEDIA_HTML_SUPPORT

/** Get height of first line. */

LayoutCoord
Container::GetFirstLineHeight() const
{
	for (VerticalLayout* vertical_layout = static_cast<VerticalLayout*>(layout_stack.First());
		 vertical_layout; vertical_layout = vertical_layout->Suc())
		if (vertical_layout->IsLine())
			return static_cast<Line*>(vertical_layout)->GetLayoutHeight();

	return LayoutCoord(0);
}

#endif // MEDIA_HTML_SUPPORT

/** Prepare the container for layout; calculate margin collapsing policy,
	CSS-specified height, and more.

	@param height (out) CSS actual height of the container, or 0 if the CSS computed
	height is auto
	@return FALSE on OOM, TRUE otherwise */

BOOL
Container::PrimeProperties(const HTMLayoutProperties& props, LayoutInfo& info, LayoutProperties* cascade, LayoutCoord& height)
{
	Container* container = cascade->container;
	LayoutCoord top_padding_border = props.padding_top + LayoutCoord(props.border.top.width);
	LayoutCoord bottom_padding_border = props.padding_bottom + LayoutCoord(props.border.bottom.width);
	HTML_Element* html_element = cascade->html_element;

	// Set up margin collapsing

	if (top_padding_border || placeholder->GetLocalSpaceManager() || html_element->Type() == Markup::HTE_HTML)
		packed.stop_top_margin_collapsing = TRUE;
	else
		packed.stop_top_margin_collapsing = FALSE;

	packed.no_content = TRUE;

	/* run-in or compact? First-line cancels run-ins. A run-in also requires a
	   container to live inside, so check for that. If a root node is attempted
	   displayed as a run-in (that may happen for SVG foreignobject), there is
	   no container. */

	reflow_state->is_run_in =
		(props.display_type == CSS_VALUE_run_in || props.display_type == CSS_VALUE_compact) &&
		!html_element->HasFirstLine() &&
		container;

	// Set up list-item stuff

	if (html_element->Type() == Markup::HTE_OL)
		if (!reflow_state->list_item_state.Init(info, html_element))
			return FALSE;

	if (container && container->HasFlexibleMarkers())
	{
		if (!placeholder->IsAbsolutePositionedBox() &&
			!placeholder->IsFloatingBox() &&
			!placeholder->IsInlineBox()
#ifdef CSS_TRANSFORMS
			&& !placeholder->GetTransformContext()
#endif // CSS_TRANSFORMS
			)
			reflow_state->list_item_marker_state |= ANCESTOR_HAS_FLEXIBLE_MARKER;
	}

	// Set up CSS height

	reflow_state->css_height = CalculateCSSHeight(info, cascade);

	reflow_state->stop_bottom_margin_collapsing = reflow_state->css_height != CONTENT_HEIGHT_AUTO && reflow_state->css_height != 0 || bottom_padding_border || placeholder->GetLocalSpaceManager() || html_element->Type() == Markup::HTE_HTML;

	placeholder->SetHasAutoHeight(reflow_state->css_height == CONTENT_HEIGHT_AUTO);

	if (reflow_state->css_height != CONTENT_HEIGHT_AUTO)
	{
		if (packed.relative_height)
			/* relative height; if it resolves to a non-auto value,
			   this container cannot be skipped in future reflow passes. */

			packed.unskippable = 1;

		if (props.box_sizing == CSS_VALUE_border_box)
			reflow_state->css_height -= top_padding_border + bottom_padding_border;

#ifdef PAGED_MEDIA_SUPPORT
		if (PagedContainer* pc = GetPagedContainer())
			reflow_state->css_height -= pc->GetPageControlHeight();
#endif // PAGED_MEDIA_SUPPORT

		if (reflow_state->css_height < 0)
			reflow_state->css_height = LayoutCoord(0);

		height = reflow_state->css_height;
	}
	else
		height = LayoutCoord(0);

	return TRUE;
}

#ifdef SUPPORT_TEXT_DIRECTION

/** Initialise bidi calculation. */

BOOL
Container::InitBidiCalculation(const HTMLayoutProperties* props)
{
	if (!bidi_segments)
	{
		bidi_segments = OP_NEW(Head, ());
		if (!bidi_segments)
			return FALSE;
	}
	else
		bidi_segments->Clear();

	reflow_state->bidi_calculation = OP_NEW(BidiCalculation, ());

	if (!reflow_state->bidi_calculation)
		return FALSE;

	reflow_state->bidi_calculation->Reset();
	reflow_state->bidi_calculation->SetSegments(bidi_segments);

	if (props)
		reflow_state->bidi_calculation->SetParagraphLevel((props->direction == CSS_VALUE_rtl) ? BIDI_R : BIDI_L,
														  props->unicode_bidi == CSS_VALUE_bidi_override);
	else
		reflow_state->bidi_calculation->SetParagraphLevel(BIDI_L, FALSE);

	return TRUE;
}

#endif

/** Reflow box and children. */

/* virtual */ LAYST
Container::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (first_child && first_child != cascade->html_element)
		return LayoutWithFirstLine(cascade, info, first_child, start_position);
	else
		if (!InitialiseReflowState())
			return LAYOUT_OUT_OF_MEMORY;
		else
		{
			const HTMLayoutProperties& props = *cascade->GetProps();
			SpaceManager* local_space_manager = placeholder->GetLocalSpaceManager();
			Container* container = cascade->container;
			Content_Box* pending_run_in = NULL;
			LayoutCoord pending_compact_space(0);

			// Cannot skip layout in a future reflow pass if the container has percentual vertical padding.

			packed.unskippable = props.GetPaddingTopIsPercent() || props.GetPaddingBottomIsPercent() || props.GetTextIndentIsPercent();

			packed.relative_height = props.HeightIsRelative(placeholder->IsAbsolutePositionedBox()) ||
				cascade->flexbox && cascade->flexbox->IsVertical();

			packed.height_auto = props.content_height == CONTENT_HEIGHT_AUTO;

			// Set up width propagation

			reflow_state->propagate_min_max_widths = NeedMinMaxWidthCalculation(cascade);
			reflow_state->calculate_min_max_widths = reflow_state->propagate_min_max_widths && !packed.minmax_calculated;

			if (reflow_state->calculate_min_max_widths)
				reflow_state->InitMinMaxData();

			// Reset current formatting

			reflow_state->Reset();

#ifdef SUPPORT_TEXT_DIRECTION
			if (info.workplace->GetBidiStatus() == LOGICAL_HAS_RTL || info.workplace->GetBidiStatus() == VISUAL_ENCODING)
			{
				if (!InitBidiCalculation(&props))
				{
					DeleteReflowState();
					return LAYOUT_OUT_OF_MEMORY;
				}

				if (info.workplace->GetBidiStatus() == VISUAL_ENCODING)
					reflow_state->bidi_calculation->SetParagraphLevel((props.direction == CSS_VALUE_rtl) ? BIDI_R : BIDI_L, TRUE);
			}
#endif

#ifdef DEBUG
			reflow_state->old_min_width = minimum_width;
			reflow_state->old_max_width = maximum_width;
#endif // DEBUG

			reflow_state->has_css_first_letter = FIRST_LETTER_NO;

			if (cascade->html_element->HasFirstLetter())
				reflow_state->has_css_first_letter |= FIRST_LETTER_THIS;

			if (container && container->HasCssFirstLetter() != FIRST_LETTER_NO &&
				placeholder->IsBlockBox() && ((BlockBox*)placeholder)->IsInStack())
				reflow_state->has_css_first_letter |= FIRST_LETTER_PARENT;

			reflow_state->reflow_position = props.padding_top + LayoutCoord(props.border.top.width);

			reflow_state->content_width = CalculateContentWidth(props, INNER_CONTENT_WIDTH);
			reflow_state->outer_content_width = CalculateContentWidth(props, OUTER_CONTENT_WIDTH);

			// Don't allow content_width to be negative (this can happen for table cells before they get any content)

			if (reflow_state->content_width < 0)
				reflow_state->content_width = LayoutCoord(0);

			reflow_state->apply_height_is_min_height = info.doc->GetLayoutMode() == LAYOUT_MSR ||
				info.doc->GetLayoutMode() == LAYOUT_AMSR;

			if (reflow_state->apply_height_is_min_height)
				reflow_state->height_is_min_height_candidate = TRUE;
			else
				reflow_state->height_is_min_height_candidate = props.max_paragraph_width > 0 && props.max_paragraph_width < GetWidth();

			height = LayoutCoord(0);

			if (info.using_flexroot)
				if (cascade->html_element->Type() == Markup::HTE_DOC_ROOT)
					reflow_state->affects_flexroot = reflow_state->calculate_min_max_widths;
				else
					if (container)
						reflow_state->affects_flexroot = container->AffectsFlexRoot() && !IsShrinkToFit();
					else
						if (cascade->table)
							reflow_state->affects_flexroot = cascade->table->AffectsFlexRoot();
						else
							if (cascade->flexbox)
								reflow_state->affects_flexroot = cascade->flexbox->AffectsFlexRoot();

			// Set initial height

			LayoutCoord content_height;
			if (!PrimeProperties(props, info, cascade, content_height))
				return LAYOUT_OUT_OF_MEMORY;

			UpdateHeight(info, cascade, content_height, LayoutCoord(0), TRUE);

			if (
#ifdef PAGED_MEDIA_SUPPORT
				info.paged_media != PAGEBREAK_OFF ||
#endif // PAGED_MEDIA_SUPPORT
				cascade->multipane_container)
			{
				BOOL avoid_page_break_inside;
				BOOL avoid_column_break_inside;

				cascade->GetAvoidBreakInside(info, avoid_page_break_inside, avoid_column_break_inside);

				packed.avoid_page_break_inside = !!avoid_page_break_inside;
				packed.avoid_column_break_inside = !!avoid_column_break_inside;
			}

			if (reflow_state->calculate_min_max_widths)
			{
				SetTrueTableCandidate(NEGATIVE);
				reflow_state->min_reflow_position = props.GetNonPercentTopBorderPadding();

				/* Make sure that horizontal border, padding, fixed width and min-width
				   are recorded (in case there is no child content to trigger that). */

				PropagateMinMaxWidths(info, LayoutCoord(0), LayoutCoord(0), LayoutCoord(0));

				if (local_space_manager && props.content_width >= 0 && props.content_height >= 0 && !reflow_state->height_is_min_height_candidate && HonorsAbsoluteWidth())
					/* If both the width and the height properties are absolute values, and this is
					   a container that will honor such, we should avoid further propagation from
					   children. This is only possible in normal layout mode, though; the width
					   property does not affect minimum width in ERA mode though (need to examine
					   content to find a minimum width then). Furthermore, we can only skip width
					   propagation if we're entering a new block formatting
					   context. Same-block-formatting-context containers may contain floats that
					   may affect ancestor containers' maximum widths and minimum heights. */

					if (info.doc->GetLayoutMode() == LAYOUT_NORMAL)
					{
#ifdef DEBUG
						reflow_state->old_min_width = minimum_width;
						reflow_state->old_max_width = maximum_width;
#endif // DEBUG

						// We can stop recording min/max widths now, unless this is a horizontal MARQUEE.

						packed.minmax_calculated = 1;
						reflow_state->calculate_min_max_widths = IsHorizontalMarquee();

						if (reflow_state->affects_flexroot)
							/* Absolutely positioned descendant boxes will still affect FlexRoot,
							   if this isn't a contaning block for such boxes. */

							reflow_state->affects_flexroot = !placeholder->IsPositionedBox();
					}
			}

			if (reflow_state->calculate_min_max_widths && container)
			{
				if (!local_space_manager)
				{
					// Set minimum distance to the left and right border edge of the BFC.

					reflow_state->bfc_left_edge_min_distance = container->reflow_state->bfc_left_edge_min_distance;
					reflow_state->bfc_right_edge_min_distance = container->reflow_state->bfc_right_edge_min_distance;

					reflow_state->bfc_left_edge_min_distance += props.GetNonPercentLeftBorderPadding();
					if (!props.GetMarginLeftIsPercent())
						reflow_state->bfc_left_edge_min_distance += props.margin_left;

					reflow_state->bfc_right_edge_min_distance += props.GetNonPercentRightBorderPadding();
					if (!props.GetMarginRightIsPercent())
						reflow_state->bfc_right_edge_min_distance += props.margin_right;
				}
			}

			if (container)
			{
				if (props.content_width >= 0)
				{
					reflow_state->inside_fixed_width =
						props.box_sizing == CSS_VALUE_content_box ||
						!props.GetPaddingLeftIsPercent() && !props.GetPaddingRightIsPercent();

					if (reflow_state->inside_fixed_width)
						reflow_state->total_max_width = GetContentWidth();
				}
				else
				{
					reflow_state->inside_fixed_width = container->reflow_state->inside_fixed_width;

					if (reflow_state->inside_fixed_width && IsShrinkToFit())
						/* Shrink-to-fit containers are obviosuly not fixed-width containers, but if a shrink-to-fit
						   float is inside another shrink-to-fit, we pretend it is, as far as maximum width propagation
						   is concerned. An unconstrained maximum width propagation from the float could cause the
						   outer shrink-to-fit container to become too wide. This solution far from perfect; minimum
						   height calculation will be off now, if it turns out that minimum width of the float becomes
						   larger than its container's fixed width. To solve this we would need two reflow passes only
						   to calculate correct min/max widths. */

						reflow_state->inside_fixed_width = container->IsCalculatingMinMaxWidths() && placeholder->IsFloatingBox();

					if (reflow_state->inside_fixed_width)
						reflow_state->total_max_width = container->reflow_state->total_max_width -
							LayoutCoord(props.margin_left + props.margin_right +
										props.border.left.width + props.border.right.width +
										props.padding_left + props.padding_right);
				}

				if (props.content_height >= 0)
					reflow_state->inside_fixed_height =
					props.box_sizing == CSS_VALUE_content_box ||
					container->reflow_state->inside_fixed_width ||
					!props.GetPaddingTopIsPercent() && !props.GetPaddingBottomIsPercent();
				else
					reflow_state->inside_fixed_height = props.GetHeightIsPercent() && container->reflow_state->inside_fixed_height;
			}

			for (VerticalLayout* vertical_layout = (VerticalLayout*) layout_stack.First(); vertical_layout; )
			{
				VerticalLayout* suc = (VerticalLayout*) vertical_layout->Suc();

#ifdef PAGED_MEDIA_SUPPORT

				// When reflowing because of page break, don't remove page breaks

				if (!info.KeepPageBreaks() || !vertical_layout->IsPageBreak())
#endif // PAGED_MEDIA_SUPPORT
				{
					if (vertical_layout->IsLine())
					{
						Line* line = (Line*) vertical_layout;

						AbsoluteBoundingBox bbox;
						line->GetBoundingBox(bbox);
						info.visual_device->UpdateRelative(bbox.GetX() + line->GetX(), bbox.GetY() + line->GetY(), bbox.GetWidth(), bbox.GetHeight());
					}

					vertical_layout->Out();

					if (!vertical_layout->IsBlock())
						OP_DELETE(vertical_layout);
				}

				vertical_layout = suc;
			}

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE
			packed.only_traverse_blocks = 0;
#endif

			if (container)
				pending_run_in = container->StealPendingRunIn(info, pending_compact_space);

			if (cascade->html_element && cascade->html_element->HasFirstLine() && cascade->html_element->Parent())
			{
				if (cascade->WantToModifyProperties(TRUE))
				{
					if (OpStatus::IsMemoryError(cascade->AddFirstLineProperties(info.hld_profile)))
						return LAYOUT_OUT_OF_MEMORY;

					cascade->use_first_line_props = TRUE;
				}
				else
					return LAYOUT_OUT_OF_MEMORY;

				reflow_state->is_css_first_line = TRUE;
			}
			else
			{
				reflow_state->is_css_first_line = FALSE;

				if (pending_run_in && placeholder->IsBlockBox() && props.display_type != CSS_VALUE_run_in)
					if (OpStatus::IsMemoryError(LayoutRunIn(cascade, info, pending_run_in, pending_compact_space)))
						return LAYOUT_OUT_OF_MEMORY;
			}

			return LayoutWithFirstLine(cascade, info);
		}
}

/** Layout container and catch possible first-line end. */

LAYST
Container::LayoutWithFirstLine(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (reflow_state->is_css_first_line)
	{
		LAYST state = placeholder->LayoutChildren(cascade, info, first_child, start_position);

		OP_ASSERT(reflow_state);

		if (state == LAYOUT_END_FIRST_LINE)
		{
			HTML_Element* layout_from = reflow_state->break_before_content;

			OP_ASSERT(layout_from);

			reflow_state->break_before_content = NULL;

			if (!layout_from)
				layout_from = first_child;

			cascade->RemoveFirstLineProperties();

			// Compute last element and start position

			start_position = reflow_state->GetLineCurrentPosition();
			first_child = layout_from;
		}
		else
			return state;
	}

	return placeholder->LayoutChildren(cascade, info, first_child, start_position);
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
BlockContainer::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord border_padding = LayoutCoord(props.padding_left +
									   props.padding_right +
									   props.border.left.width +
									   props.border.right.width);
	LayoutCoord new_width;

	// Calculate width

	if (cascade->flexbox)
		// Flex item main sizes are calculated by the flex algorithm.

		new_width = ((FlexItemBox*) placeholder)->GetFlexWidth(cascade);
	else
	{
		if (props.content_width >= 0)
		{
			new_width = props.content_width;

			if ((info.doc->GetLayoutMode() == LAYOUT_MSR ||
				 info.doc->GetLayoutMode() == LAYOUT_AMSR) && placeholder->IsFloatingBox())
			{
				const HTMLayoutProperties& container_props = *cascade->container->GetPlaceholder()->GetCascade()->GetProps();

				if (container_props.content_width > 0 && container_props.content_width > container_props.era_max_width)
					/* This is a float with computed width specified in pixels, in a
					   container that also has its computed width specified in pixels. The
					   container's computed width was larger than the allowed maximum
					   calculated by ERA, which incidentally means that the container has
					   been made narrower already. Scale the float's width along with it. */

					new_width = (new_width * container_props.era_max_width) / container_props.content_width;
			}
		}
		else
		{
			LayoutCoord available_width = placeholder->GetAvailableWidth(cascade);

			if (cascade->IsLayoutWidthAuto())
			{
				new_width = available_width - (props.margin_left + props.margin_right);

				if (props.box_sizing == CSS_VALUE_content_box)
					new_width -= border_padding;
			}
			else
				new_width = props.ResolvePercentageWidth(available_width);
		}

		// Width must be within the limits of min-width and max-width

		new_width = props.CheckWidthBounds(new_width);

		// ... and it should never be negative

		if (new_width < 0)
			new_width = LayoutCoord(0);

		/* 'new_width' has been relative to the box specified by box-sizing so
		   far. Convert it to border-box now. */

		if (props.box_sizing == CSS_VALUE_content_box)
			new_width += border_padding;
		else
			if (new_width < border_padding)
				// Avoid negative content box width

				new_width = border_padding;
	}

	if (new_width != block_width)
	{
		block_width = new_width;

		return OpBoolean::IS_TRUE;
	}

	if (packed.unskippable ||
		packed.relative_height && CalculateCSSHeight(info, cascade) != CONTENT_HEIGHT_AUTO ||
		placeholder->NeedMinMaxWidthCalculation(cascade) && !packed.minmax_calculated)
		return OpBoolean::IS_TRUE;

	return OpBoolean::IS_FALSE;
}

#ifdef MEDIA_HTML_SUPPORT

/** Finish reflowing box. */

/* virtual */ LAYST
BlockContainer::FinishLayout(LayoutInfo& info)
{
	LAYST result = Container::FinishLayout(info);
	if (result != LAYOUT_CONTINUE)
		return result;

	HTML_Element* html_element = placeholder->GetCascade()->html_element;

	if (html_element->IsMatchingType(Markup::CUEE_ROOT, NS_CUE))
		if (MediaElement* media_element = ReplacedContent::GetMediaElement(html_element))
		{
			/* Notify the <track> cue layout algorithm about the
			   resulting height of the cue box as well as the height of
			   the first line. The height of the first line is used for
			   line-based positioning. */

			OpPoint cue_move = media_element->OnCueLayoutFinished(html_element, GetHeight(), GetFirstLineHeight());

			if (cue_move.x != 0 || cue_move.y != 0)
			{
				LayoutCoord offset_x = LayoutCoord(cue_move.x);
				LayoutCoord offset_y = LayoutCoord(cue_move.y);

				OP_ASSERT(placeholder->IsBlockBox());

				BlockBox* box = static_cast<BlockBox*>(placeholder);
				box->SetX(box->GetX() + offset_x);
				box->SetY(box->GetY() + offset_y);

				info.Translate(offset_x, offset_y);

				placeholder->PropagateBottomMargins(info);
			}
		}

	return LAYOUT_CONTINUE;
}

#endif // MEDIA_HTML_SUPPORT

/** Calculate and return width, based on current min/max values and constraints. */

LayoutCoord
ShrinkToFitContainer::CalculateHorizontalProps(LayoutProperties* cascade, LayoutInfo& info, BOOL use_all_available_width)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord padding_border_width = props.padding_left + props.padding_right + LayoutCoord(props.border.left.width + props.border.right.width);
	LayoutCoord available_width = placeholder->GetAvailableWidth(cascade) - (props.margin_left + props.margin_right);
	LayoutCoord new_width;

	if (use_all_available_width)
		/* Initially, before min/max widths are known, we here "guess" that the
		   final width will become equal to available width. This is obviously
		   far from certain (it's a shrink-to-fit container, after all), but if
		   we are to guess at one value, it will have to be this one. If it
		   later turns out that we guessed right, we will only need one reflow
		   pass. */

		new_width = available_width;
	else
	{
		LayoutCoord min_width = minimum_width;

		if (info.doc->GetLayoutMode() != LAYOUT_NORMAL && placeholder->IsAbsolutePositionedBox())
			/* Since an absolutely positioned box does not propagate min/max widths to
			   ancestors, don't allow it to become narrower than its true minimum
			   width. */

			min_width = normal_minimum_width;

		/* Any padding resolved from percentual values were not added to min/max values.
		   Need to be taken into consideration when comparing with min/max widths now. */

		LayoutCoord percent_based_padding =
			(props.GetPaddingLeftIsPercent() ? props.padding_left : LayoutCoord(0)) +
			(props.GetPaddingRightIsPercent() ? props.padding_right : LayoutCoord(0));

		// Calculate border box width according to the shrink-to-fit algorithm

		new_width = MAX(min_width + percent_based_padding, MIN(maximum_width + percent_based_padding, available_width));
	}

	if (props.box_sizing == CSS_VALUE_content_box)
		// Convert to content box width while applying constraints

		new_width -= padding_border_width;

	// Constrain width to min-width and max-width

	new_width = props.CheckWidthBounds(new_width);

	if (new_width < 0)
		new_width = LayoutCoord(0);

	if (props.box_sizing == CSS_VALUE_content_box)
		// Convert back to border box width

		new_width += padding_border_width;

	else
		if (new_width < padding_border_width)
			// Avoid negative content box width

			new_width = padding_border_width;

	return new_width;
}

/** Apply min-width and max-width property constraints. */

/* virtual */ void
ShrinkToFitContainer::ApplyMinWidthProperty(const HTMLayoutProperties& props, LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width)
{
	/* Ignore min-width here. It has been stored in the object so that it will be paid attention to
	   whenever someone calls GetMinMaxWidth(). But we need values unaffected by min-width when
	   computing size in the next (if any) reflow pass, because of the combination of
	   box-sizing:border-box and percentual horizontal padding. Such padding cannot be added during
	   width propagation, but will instead be added when computing the actual size in the next
	   reflow pass. Then we need to know how much space the actual content of this container
	   _needs_. Maybe we can apply percentual padding without exceeding the border-box's min-width
	   property, or maybe not. */

	if (!props.GetMaxWidthIsPercent() && props.max_width >= 0)
	{
		if (min_width > props.max_width)
			min_width = props.max_width;

		if (normal_min_width > props.max_width)
			normal_min_width = props.max_width;

		if (max_width > props.max_width)
			max_width = props.max_width;
	}
}

/** Get min/max width. */

/* virtual */ BOOL
ShrinkToFitContainer::GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const
{
	LayoutCoord min_width_prop(stf_packed.nonpercent_min_width_prop);

	if (min_width_prop)
	{
		min_width = MAX(minimum_width, min_width_prop);
		normal_min_width = MAX(normal_minimum_width, min_width_prop);
		max_width = MAX(maximum_width, min_width_prop);
	}
	else
	{
		min_width = minimum_width;
		normal_min_width = normal_minimum_width;
		max_width = maximum_width;
	}

	return TRUE;
}

/** Clear min/max width. */

/* virtual */ void
ShrinkToFitContainer::ClearMinMaxWidth()
{
	BlockContainer::ClearMinMaxWidth();
	stf_packed.minmax_changed = 0;
}

/** Update screen. */

/* virtual */ void
ShrinkToFitContainer::UpdateScreen(LayoutInfo& info)
{
	BlockContainer::UpdateScreen(info);

	OP_ASSERT(reflow_state || packed.minmax_calculated); // Skipping layout of a container when its min/max values haven't been calculated makes no sense.

	if (reflow_state && !packed.minmax_calculated)
	{
		LayoutProperties* cascade = placeholder->GetCascade();
		LayoutCoord new_width = CalculateHorizontalProps(cascade, info, FALSE);

		if (block_width != new_width)
		{
			// Min/max sizes changed during layout. Need another reflow pass.

			block_width = new_width;
			stf_packed.minmax_changed = 1;
			info.workplace->SetReflowElement(cascade->html_element);

			for (LayoutProperties* ca = cascade; ca && ca->html_element; ca = ca->Pred())
				if (ScrollableArea* sc = ca->html_element->GetLayoutBox()->GetScrollable())
				{
					sc->UnlockAutoScrollbars();

					/* We are floating box with auto width. This means that when
					   starting layout we were not knowing yet our final content width
					   and we asked for maximum possible container space.
					   If there was a float before us we were pushed down below it.

					   Now we know our width but it's too late to possibly pull us back up so
					   we will try to do it during next reflow pass. However we may unnecessarily
					   triggered scrollbars in our parent container causing us being again
					   pushed down as space in container will be reduced by unwanted scrollbars.

					   Since we are already asking for another reflow pass we can wait with scrollbar
					   calculation to next reflow iteration and do it when our position will
					   be hopefully fixed. */

					sc->PreventAutoScrollbars();
				}
		}
	}
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
ShrinkToFitContainer::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	OP_ASSERT(!cascade->flexbox); // Flex items are sized by their containing flexbox.

	if (props.GetMinWidthIsPercent())
		stf_packed.nonpercent_min_width_prop = 0;
	else
	{
		stf_packed.nonpercent_min_width_prop = props.min_width;

		if (props.box_sizing == CSS_VALUE_content_box)
			stf_packed.nonpercent_min_width_prop += props.GetNonPercentHorizontalBorderPadding();
	}

	LayoutCoord new_width;

	if (props.column_count > 0 && props.column_width > 0)
	{
		LayoutCoord padding_border_width = props.padding_left + props.padding_right + LayoutCoord(props.border.left.width + props.border.right.width);

		new_width = LayoutCoord(props.column_width * props.column_count + props.column_gap * (props.column_count - 1) + padding_border_width);
	}
	else
		if (packed.minmax_calculated)
			new_width = CalculateHorizontalProps(cascade, info, FALSE);
		else
			/* Min/max widths are unavailable (will be (re-)calculated in this
			   reflow pass), so we need other means of setting a width. */

			if (block_width < 0)
				// New container. Let width be equal to containing block's width.

				new_width = CalculateHorizontalProps(cascade, info, TRUE);
			else
				// Keep old width. It may be that recalculating min/max widths won't affect it.

				new_width = block_width;

	if (block_width != new_width)
	{
		block_width = new_width;

		return OpBoolean::IS_TRUE;
	}

	if (packed.unskippable ||
		packed.relative_height && CalculateCSSHeight(info, cascade) != CONTENT_HEIGHT_AUTO)
		return OpBoolean::IS_TRUE;

	return OpBoolean::IS_FALSE;
}

/** Do we need to calculate min/max widths of this box's content? */

/* virtual */ BOOL
ShrinkToFitContainer::NeedMinMaxWidthCalculation(LayoutProperties* cascade) const
{
	return IsShrinkToFit() ||
		placeholder->NeedMinMaxWidthCalculation(cascade);
}

/** Signal that content may have changed. */

/* virtual */ void
ShrinkToFitContainer::SignalChange(FramesDocument* doc)
{
	if (stf_packed.minmax_changed)
	{
		stf_packed.minmax_changed = 0;
		placeholder->MarkDirty(doc, FALSE);
	}
}

/** Propagate min/max widths. */

/* virtual */ void
ShrinkToFitContainer::PropagateMinMaxWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width)
{
	if (reflow_state->inside_fixed_width)
	{
		// Cap maximum width to MAX(available width, minimum width).

		LayoutCoord container_width = reflow_state->total_max_width > 0 ? reflow_state->total_max_width : LayoutCoord(0);

		if (max_width > normal_min_width && max_width > container_width)
			max_width = MAX(normal_min_width, container_width);
	}

	BlockContainer::PropagateMinMaxWidths(info, min_width, normal_min_width, max_width);
}

/** Create and set up the OpButton object, if none exists. */

OP_STATUS
ButtonContainer::CreateButton(VisualDevice* vis_dev)
{
	if (!button)
	{
		RETURN_IF_MEMORY_ERROR(OpButton::Construct(&button));

		REPORT_MEMMAN_INC(sizeof(OpButton));
		button->SetVisualDevice(vis_dev);
		button->SetSpecialForm(TRUE);
		button->SetParentInputContext(vis_dev);

		WidgetListener* new_listener = OP_NEW(WidgetListener, (vis_dev->GetDocumentManager(), GetHtmlElement()));
		if (!new_listener)
			return LAYOUT_OUT_OF_MEMORY;
		button->SetListener(new_listener);
		REPORT_MEMMAN_INC(sizeof(WidgetListener));
	}

	return OpStatus::OK;
}

/** Destroy the OpButton object, if it exists. */

void
ButtonContainer::DestroyButton()
{
	if (button)
	{
		if (OpWidgetListener* listener = button->GetListener())
		{
			REPORT_MEMMAN_DEC(sizeof(WidgetListener));
			OP_DELETE(listener);
			button->SetListener(NULL);
		}

		button->Delete();
		button = NULL;
		REPORT_MEMMAN_DEC(sizeof(OpButton));
	}
}

ButtonContainer::~ButtonContainer()
{
	DestroyButton();
}

/** Clear min/max width. */

/* virtual */ void
ButtonContainer::ClearMinMaxWidth()
{
	if (button_packed.width_auto)
		ShrinkToFitContainer::ClearMinMaxWidth();
	else
		BlockContainer::ClearMinMaxWidth();
}

/** Update screen. */

/* virtual */ void
ButtonContainer::UpdateScreen(LayoutInfo& info)
{
	if (button_packed.width_auto)
		return ShrinkToFitContainer::UpdateScreen(info);
	else
		return BlockContainer::UpdateScreen(info);
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
ButtonContainer::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	button_packed.width_auto = cascade->GetProps()->content_width == CONTENT_WIDTH_AUTO && !cascade->flexbox;

	if (button_packed.width_auto)
		return ShrinkToFitContainer::ComputeSize(cascade, info);
	else
		return BlockContainer::ComputeSize(cascade, info);
}

/** Lay out box. */

LAYST
ButtonContainer::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (OpStatus::IsMemoryError(CreateButton(info.visual_device)))
		return LAYOUT_OUT_OF_MEMORY;

	// Propagate padding to button, so it can choose an appropriate skin.

	if (button)
		button->SetPadding(cascade->GetProps()->padding_left, cascade->GetProps()->padding_top, cascade->GetProps()->padding_right, cascade->GetProps()->padding_bottom);

	return BlockContainer::Layout(cascade, info, first_child, start_position);
}

/** Traverse box with children. */

/* virtual */ void
ButtonContainer::Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	LayoutCoord offs_x = GetOfsX();
	LayoutCoord offs_y = GetOfsY();

	traversal_object->Translate(offs_x, offs_y);

	if (button_packed.width_auto)
		ShrinkToFitContainer::Traverse(traversal_object, layout_props);
	else
		BlockContainer::Traverse(traversal_object, layout_props);

	traversal_object->Translate(-offs_x, -offs_y);
}

/** Updates the widget position. Must be correct when user scrolls. */

void
ButtonContainer::UpdateWidgetPosition()
{
	if (button == NULL)
		return;

	button->SetPosInDocument(GetPosInDocument());
}

/** Paint the button */

void ButtonContainer::PaintButton(VisualDevice* visual_device, const HTMLayoutProperties& props)
{
	if (button == NULL)
		return;

	button->SetVisualDevice(visual_device);

	HTML_Element* html_element = GetHtmlElement();
	BOOL has_specified_borders = FormObject::HasSpecifiedBorders(props, html_element);
	if (has_specified_borders)
	{
		visual_device->Translate(props.border.left.width, props.border.top.width);
		button->SetRect(OpRect(props.border.left.width,
							   props.border.top.width,
							   block_width - (props.border.left.width + props.border.right.width),
							   height - (props.border.top.width + props.border.bottom.width)));
	}
	else
		button->SetRect(OpRect(0, 0, block_width, height));

	SetPosition(visual_device->GetCTM());
	UpdateWidgetPosition();

	COLORREF bg_color = BackgroundColor(props);

	if (bg_color != USE_DEFAULT_COLOR)
		button->SetBackgroundColor(HTM_Lex::GetColValByIndex(bg_color));
	else
		button->UnsetBackgroundColor();

	button->SetForegroundColor(HTM_Lex::GetColValByIndex(props.font_color));
	FramesDocument* doc = visual_device->GetDocumentManager()->GetCurrentVisibleDoc();
	button->SetHasCssBackground(FormObject::HasSpecifiedBackgroundImage(doc, props, html_element));
	button->SetHasCssBorder(has_specified_borders);

	BOOL disabled = FALSE;

	if (doc)
	{
		disabled = html_element->IsDisabled(doc);
		BOOL focused = doc->ElementHasFocus(html_element);
		button->SetDefaultLook(focused);
		button->SetHasFocusRect(visual_device->GetDrawFocusRect());
		button->SetForcedFocusedLook(focused);
	}
	button->SetEnabled(!disabled);

	button->GenerateOnPaint(button->GetBounds());

	if (has_specified_borders)
		visual_device->Translate(-props.border.left.width, -props.border.top.width);
}

/** Create form, plugin and iframe objects */

/* virtual */ OP_STATUS
ButtonContainer::Enable(FramesDocument* doc)
{
	return CreateButton(doc->GetVisualDevice());
}

/** Remove form, plugin and iframe objects */

/* virtual */ void
ButtonContainer::Disable(FramesDocument* doc)
{
	if (doc->IsUndisplaying() || doc->IsBeingDeleted())
		DestroyButton();
	else
		if (button)
			button->SetParentInputContext(NULL);
}

/** Returns the current horizontal offset */

LayoutCoord ButtonContainer::GetOfsX() const
{
	if (button && button->packed2.is_down && !button->packed2.is_outside)
		return LayoutCoord(1);

	return LayoutCoord(0);
}

/** Returns the current vertical offset */

LayoutCoord ButtonContainer::GetOfsY() const
{
	if (button && button->packed2.is_down && !button->packed2.is_outside)
		return LayoutCoord(1);

	return LayoutCoord(0);
}

/* virtual */
void ButtonContainer::UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height)
{
	Container::UpdateHeight(info, cascade, content_height, min_content_height, initial_content_height);

	if (!initial_content_height && (reflow_state->css_height != CONTENT_HEIGHT_AUTO || placeholder->GetCascade()->flexbox))
	{
		OP_ASSERT(placeholder->GetLocalSpaceManager());

		if (content_height < reflow_state->float_bottom)
			content_height = reflow_state->float_bottom;

		CenterContentVertically(info, *cascade->GetProps(), content_height);
	}
}

/** Return TRUE if this element can be split into multiple outer columns or pages. */

/* virtual */ BOOL
ButtonContainer::IsColumnizable() const
{
	/* Highly unlikely that it's a good idea to split a button over multiple
	   columns. Google Chrome does it, though. */

	return FALSE;
}

/* virtual */ void
ButtonContainer::HandleEvent(FramesDocument* doc, DOM_EventType ev, int offset_x, int offset_y)
{
#ifndef MOUSELESS
	if (!button)
		return;

	OpPoint point(offset_x, offset_y);

	switch (ev)
	{
	case ONMOUSEDOWN:
		button->GenerateOnMouseDown(point, current_mouse_button, 1);
		break;

	case ONMOUSEMOVE:
		// Hooked mousemove events is given to the widget from HTML_Document::MouseAction
		if (OpWidget::hooked_widget == NULL)
			button->GenerateOnMouseMove(point);
		break;
	}
#endif // MOUSELESS
}

/** Propagate min/max widths. */

/* virtual */ void
MarqueeContainer::PropagateMinMaxWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width)
{
	if (!packed3.vertical && !packed3.containing_block_width_calculated)
	{
		if (reflow_state->marquee_scroll_width < max_width)
			reflow_state->marquee_scroll_width = max_width;

		/* Seems that browsers generally agree that a horizontal MARQUEE has no minimum width.
		   Using the content's minimum width is bogus and uninteresting in any case. */

		min_width = LayoutCoord(0);
		normal_min_width = LayoutCoord(0);
	}

	if (packed3.is_shrink_to_fit)
		ShrinkToFitContainer::PropagateMinMaxWidths(info, min_width, normal_min_width, max_width);
	else
		BlockContainer::PropagateMinMaxWidths(info, min_width, normal_min_width, max_width);
}

/** Update screen. */

/* virtual */ void
MarqueeContainer::UpdateScreen(LayoutInfo& info)
{
	if (packed3.is_shrink_to_fit)
		ShrinkToFitContainer::UpdateScreen(info);
	else
		BlockContainer::UpdateScreen(info);

	if (reflow_state)
		if (!packed3.vertical && !packed3.containing_block_width_calculated)
		{
			// Set new inner scroll width of a horizontal MARQUEE.

			real_content_size = reflow_state->marquee_scroll_width;

			if (real_content_size != reflow_state->content_width)
			{
				// If the inner scroll width changed during reflow, we need to reflow again.

				LayoutProperties* cascade = placeholder->GetCascade();

				packed3.containing_block_width_changed = 1;
				info.workplace->SetReflowElement(cascade->html_element);
			}
		}
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
MarqueeContainer::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	packed3.vertical = props.marquee_dir == ATTR_VALUE_up || props.marquee_dir == ATTR_VALUE_down;
	packed3.is_shrink_to_fit = props.IsShrinkToFit(cascade->html_element->Type());

	OP_BOOLEAN size_changed;

	if (packed3.is_shrink_to_fit)
		size_changed = ShrinkToFitContainer::ComputeSize(cascade, info);
	else
		size_changed = BlockContainer::ComputeSize(cascade, info);

	RETURN_IF_ERROR(size_changed);

	if (!g_main_message_handler->HasCallBack(this, MSG_MARQUEE_UPDATE, (INTPTR) this))
		if (OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_MARQUEE_UPDATE, (INTPTR) this)))
			return OpStatus::ERR_NO_MEMORY;

	return size_changed;
}

/** Finish reflowing box. */

/* virtual */ LAYST
MarqueeContainer::FinishLayout(LayoutInfo& info)
{
	LAYST result;

	if (packed3.is_shrink_to_fit)
		result = ShrinkToFitContainer::FinishLayout(info);
	else
		result = BlockContainer::FinishLayout(info);

	packed3.containing_block_width_calculated = 1;

	return result;
}

/** Remove form, plugin and iframe objects */

MarqueeContainer::MarqueeContainer()
  : real_content_size(1000),
	nextUpdate(g_op_time_info->GetRuntimeMS())
{
	packed3_init = 0;
}

/* virtual */
MarqueeContainer::~MarqueeContainer()
{
	g_main_message_handler->UnsetCallBacks(this);
}

/** Do we need to calculate min/max widths of this box's content? */

/* virtual */ BOOL
MarqueeContainer::NeedMinMaxWidthCalculation(LayoutProperties* cascade) const
{
	return !packed3.vertical ||
		packed3.is_shrink_to_fit ||
		placeholder->NeedMinMaxWidthCalculation(cascade);
}

/** Clear min/max width. */

/* virtual */ void
MarqueeContainer::ClearMinMaxWidth()
{
	packed3.containing_block_width_calculated = 0;

	if (packed3.is_shrink_to_fit)
		ShrinkToFitContainer::ClearMinMaxWidth();
	else
		BlockContainer::ClearMinMaxWidth();
}

/** Handle message. */

void
MarqueeContainer::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_MARQUEE_UPDATE);

	if (msg == MSG_MARQUEE_UPDATE)
	{
		VisualDevice* vd = (VisualDevice*) par2;

		packed3.pending_message = FALSE;

		OpRect bbox = GetEdgesInDocumentCoords(placeholder->GetBorderEdges());

		vd->Update(bbox.x, bbox.y, bbox.width, bbox.height, TRUE);
	}
}

/** Update height of container. */

/* virtual */ void
MarqueeContainer::UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height)
{
	if (packed3.vertical)
		real_content_size = content_height;

	Container::UpdateHeight(info, cascade, content_height, min_content_height, initial_content_height);
}

/** Traverse box with children. */

/* virtual */ void
MarqueeContainer::Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	HTML_Element* html_element = layout_props->html_element;
	BOOL stopped = html_element->GetSpecialBoolAttr(Markup::LAYOUTA_MARQUEE_STOPPED, SpecialNs::NS_LAYOUT);
	BOOL is_marquee_elm = html_element->IsMatchingType(Markup::HTE_MARQUEE, NS_HTML);

	LayoutCoord y(0);
	LayoutCoord x(0);

	if (!stopped || is_marquee_elm)
	{
		const HTMLayoutProperties& props = *layout_props->GetProps();
		BOOL positive = props.marquee_dir == ATTR_VALUE_down || props.marquee_dir == ATTR_VALUE_right;
		BOOL negative = html_element->GetSpecialBoolAttr(Markup::LAYOUTA_MARQUEE_NEGATIVE, SpecialNs::NS_LAYOUT);
		BOOL backwards = negative == positive;

		LayoutCoord offset = LayoutCoord(html_element->GetSpecialNumAttr(Markup::LAYOUTA_MARQUEE_OFFSET, SpecialNs::NS_LAYOUT));
		if (OpStatus::IsMemoryError(traversal_object->HandleMarquee(this, layout_props)))
			traversal_object->SetOutOfMemory();

		if (props.marquee_style == CSS_VALUE_alternate)
		{
			/* This is rather incomprehensible.  It just means that
			   when the marquee is going in the opposite direction. */
			positive = backwards;

			LayoutCoord boundary;
			if (packed3.vertical)
			{
				boundary = height - (props.padding_top +
									 props.padding_bottom +
									 LayoutCoord(props.border.top.width +
												 props.border.bottom.width));
			}
			else
			{
				boundary = block_width - (props.padding_left +
										  props.padding_right +
										  LayoutCoord(props.border.left.width +
													  props.border.right.width));
			}
			if (real_content_size > boundary)
				offset += boundary - real_content_size;
		}

		if (props.marquee_loop)
			if (packed3.vertical)
			{
				if (backwards)
					y = height - offset;
				else
					y = offset;

				if (positive)
					y -= real_content_size;
			}
			else
			{
				if (backwards)
					x = block_width - offset;
				else
					x = offset;

				if (positive)
					x -= real_content_size;
			}

#ifdef DOCUMENT_EDIT_SUPPORT
		FramesDocument* doc = traversal_object->GetDocument();
		if (doc->GetDocumentEdit() && doc->GetDocumentEdit()->GetEditableContainer(html_element))
		{
			x = LayoutCoord(0);
			y = LayoutCoord(0);
		}
#endif // DEBUG_DOCUMENT_EDIT

		traversal_object->Translate(x, y);
	}

	Container::Traverse(traversal_object, layout_props);

	if (!stopped || is_marquee_elm)
		traversal_object->Translate(-x, -y);
}

/** Handle marquee. */

OP_STATUS
MarqueeContainer::Animate(PaintObject* paint_object, LayoutProperties* layout_props)
{
	if (paint_object->GetVisualDevice()->IsPrinter())
		return OpStatus::OK;

	const HTMLayoutProperties& props = *layout_props->GetProps();
	HTML_Element* html_element = layout_props->html_element;
	LayoutCoord scrollamount = LayoutCoord((INTPTR)html_element->GetAttr(Markup::HA_SCROLLAMOUNT, ITEM_TYPE_NUM, (void*) -1));

	if (scrollamount < 0)
		scrollamount = LayoutCoord(6);

	BOOL stopped = html_element->GetSpecialBoolAttr(Markup::LAYOUTA_MARQUEE_STOPPED, SpecialNs::NS_LAYOUT);
	LayoutCoord offset = LayoutCoord(html_element->GetSpecialNumAttr(Markup::LAYOUTA_MARQUEE_OFFSET, SpecialNs::NS_LAYOUT));
	int looped = html_element->GetSpecialNumAttr(Markup::LAYOUTA_MARQUEE_LOOPED, SpecialNs::NS_LAYOUT);

	BOOL was_stopped = stopped;

	BOOL stopped_by_script = html_element->IsMarqueeStopped();

	if (scrollamount != 0 && !stopped && !stopped_by_script)
	{
		while (!stopped && g_op_time_info->GetRuntimeMS() >= nextUpdate)
		{
			LayoutCoord boundary;

			offset += scrollamount;
			nextUpdate += props.marquee_scrolldelay;

			if (packed3.vertical)
			{
				boundary = height - (props.padding_top +
									 props.padding_bottom +
									 LayoutCoord(props.border.top.width +
												 props.border.bottom.width));
			}
			else
			{
				boundary = block_width - (props.padding_left +
										  props.padding_right +
										  LayoutCoord(props.border.left.width +
													  props.border.right.width));
			}

			if (props.marquee_style == CSS_VALUE_alternate)
			{
				boundary -= real_content_size;
				if (boundary < 0)
					boundary = -boundary;
			}
			else if (props.marquee_style == CSS_VALUE_slide)
			{
				if (real_content_size > boundary)
					boundary = real_content_size;
			}
			else // CSS_VALUE_scroll
				boundary += real_content_size;

			if (offset > boundary)
			{
				if (props.marquee_style == CSS_VALUE_alternate)
				{
					BOOL negative = html_element->GetSpecialBoolAttr(Markup::LAYOUTA_MARQUEE_NEGATIVE, SpecialNs::NS_LAYOUT);
					negative = !negative;
					if (html_element->SetSpecialAttr(Markup::LAYOUTA_MARQUEE_NEGATIVE, ITEM_TYPE_BOOL, (void*)(INTPTR)negative, FALSE, SpecialNs::NS_LAYOUT) < 0)
						return OpStatus::ERR_NO_MEMORY;
				}

				if (++looped == props.marquee_loop
					|| (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MaximumMarqueeLoops) >= 0 &&
						looped == g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MaximumMarqueeLoops)))
				{
					stopped = TRUE;
					if (html_element->SetSpecialAttr(Markup::LAYOUTA_MARQUEE_STOPPED, ITEM_TYPE_BOOL, (void*)(INTPTR)stopped, FALSE, SpecialNs::NS_LAYOUT) < 0)
						return OpStatus::ERR_NO_MEMORY;
				}

				if (stopped && props.marquee_style == CSS_VALUE_slide && html_element->IsMatchingType(Markup::HTE_MARQUEE, NS_HTML))
					offset = boundary;
				else
					offset = LayoutCoord(0);
			}
		} // end while

		if (html_element->SetSpecialAttr(Markup::LAYOUTA_MARQUEE_OFFSET, ITEM_TYPE_NUM, (void*)(INTPTR)offset, FALSE, SpecialNs::NS_LAYOUT) < 0 ||
		    html_element->SetSpecialAttr(Markup::LAYOUTA_MARQUEE_LOOPED, ITEM_TYPE_NUM, (void*)(INTPTR)looped, FALSE, SpecialNs::NS_LAYOUT) < 0)
			return OpStatus::ERR_NO_MEMORY;
	} // end if !stopped
	else
		if (stopped_by_script)
			// Don't let it "scroll" while it is stopped

			nextUpdate = g_op_time_info->GetRuntimeMS();

	if (!was_stopped && !packed3.pending_message)
	{
		packed3.pending_message = TRUE;
		SetPosition(paint_object->GetVisualDevice()->GetCTM());
		g_main_message_handler->PostDelayedMessage(MSG_MARQUEE_UPDATE, (INTPTR) this, (INTPTR) paint_object->GetVisualDevice(), props.marquee_scrolldelay);
	}

	return OpStatus::OK;
}

/** Calculate the width of the containing block established by this container. */

/* virtual */ LayoutCoord
MarqueeContainer::CalculateContentWidth(const HTMLayoutProperties& props, ContainerWidthType width_type) const
{
	if (packed3.vertical)
		return BlockContainer::CalculateContentWidth(props);
	else
		return real_content_size;
}

/** Signal that content may have changed. */

/* virtual */ void
MarqueeContainer::SignalChange(FramesDocument* doc)
{
	if (packed3.is_shrink_to_fit)
		ShrinkToFitContainer::SignalChange(doc);
	else
		BlockContainer::SignalChange(doc);

	if (packed3.containing_block_width_changed)
	{
		packed3.containing_block_width_changed = 0;
		placeholder->MarkDirty(doc, FALSE);
	}
}

/** Return TRUE if this element can be split into multiple outer columns or pages. */

/* virtual */ BOOL
MarqueeContainer::IsColumnizable() const
{
	/* It should be possible to split a MARQUEE over multiple columns or pages,
	   but that would require us to do some work on clipping and invalidation
	   rectangles. Although the usefulness of splitting it can be disputed,
	   Google Chrome actually supports this. */

	return FALSE;
}

ScrollableContainer::~ScrollableContainer()
{
	/* placeholder may be NULL here if we managed to allocate content but failed to
	   allocate the box in LayoutProperties::CreateBox. */
	if (placeholder)
		SaveScrollPos(GetHtmlElement(), GetViewX(), GetViewY());
}

/* virtual */ void
ScrollableContainer::Disable(FramesDocument* doc)
{
	if (doc->IsUndisplaying() || doc->IsBeingDeleted())
		DisableCoreView();
}

/* virtual */ OP_STATUS
ScrollableContainer::Enable(FramesDocument* doc)
{
	InitCoreView(GetHtmlElement(), doc->GetVisualDevice());
	return OpStatus::OK;
}

/** Update margins and bounding box. See declaration in header file for more information. */

/* virtual */ void
ScrollableContainer::UpdateBottomMargins(const LayoutInfo& info, const VerticalMargin* bottom_margin, AbsoluteBoundingBox* child_bounding_box, BOOL has_inflow_content)
{
	if (child_bounding_box)
		UpdateContentSize(*child_bounding_box);

	Container::UpdateBottomMargins(info, bottom_margin, child_bounding_box, has_inflow_content);
}

/** Expand container to make space for floating and absolute positioned boxes. See declaration in header file for more information. */

/* virtual */ void
ScrollableContainer::PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts)
{
	BlockContainer::PropagateBottom(info, bottom, min_bottom, child_bounding_box, opts);

	if (opts.type == PROPAGATE_FLOAT || placeholder->IsPositionedBox())
		UpdateContentSize(child_bounding_box);
}

/** Calculate the width of the containing block established by this container. */

/* virtual */ LayoutCoord
ScrollableContainer::CalculateContentWidth(const HTMLayoutProperties& props, ContainerWidthType width_type) const
{
	LayoutCoord width = packed3.is_shrink_to_fit ? ShrinkToFitContainer::CalculateContentWidth(props) : BlockContainer::CalculateContentWidth(props);

	return width - LayoutCoord(GetVerticalScrollbarWidth());
}

/** Update height of container. */

/* virtual */ void
ScrollableContainer::UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord hor_scrollbar_width = GetHorizontalScrollbarWidth();
	LayoutCoord container_height = content_height;
	LayoutCoord min_container_height = min_content_height;

	container_height += hor_scrollbar_width;

	if (props.overflow_x == CSS_VALUE_scroll)
		min_container_height += hor_scrollbar_width;

	if (reflow_state->css_height != CONTENT_HEIGHT_AUTO && container_height > reflow_state->css_height)
		container_height = reflow_state->css_height;

	Container::UpdateHeight(info, cascade, container_height, min_container_height, initial_content_height);

	if (!initial_content_height)
	{
		// Add or remove scrollbars as appropriate.

		DetermineScrollbars(info, props, block_width, height);

		if (reflow_state->calculate_min_max_widths && !packed.minmax_calculated && vertical_scrollbar)
		{
			/* Only include scrollbar width in propagation if it is always visible and container width is
			   auto (with fixed width scrollbar width is already included), regardless of content,
			   or if overflow is auto and we can tell for sure that the content is going to be taller. */

			if (props.content_width == CONTENT_WIDTH_AUTO)
			{
				BOOL include_vertical_scrollbar = props.overflow_y == CSS_VALUE_scroll;

				if (!include_vertical_scrollbar && props.overflow_y == CSS_VALUE_auto)
				{
					LayoutCoord extra_height(0);

					if (props.box_sizing == CSS_VALUE_border_box)
						extra_height = props.GetNonPercentVerticalBorderPadding();

					if (props.min_height >= min_content_height + extra_height && !props.GetMinHeightIsPercent())
						// min-height stretches container height so that there is no need for a scrollbar

						include_vertical_scrollbar = FALSE;
					else
						if (props.max_height >= 0 && props.max_height < min_content_height + extra_height && !props.GetMaxHeightIsPercent())
							// max-height squashes the container height so that there will be need for a scrollbar

							include_vertical_scrollbar = TRUE;
						else
							if (reflow_state->css_height != CONTENT_HEIGHT_AUTO && (reflow_state->inside_fixed_height || props.content_height >= 0))
								// Let the height property, constrained by min-height and max-height, determine if there will be need for a scrollbar

								include_vertical_scrollbar = props.CheckHeightBounds(min_content_height + extra_height) - extra_height > reflow_state->css_height;
				}

				if (include_vertical_scrollbar)
				{
					LayoutCoord sb_width = GetVerticalScrollbarWidth();

					minimum_width += sb_width;
					normal_minimum_width += sb_width;
					maximum_width += sb_width;
				}
			}
		}
	}
}

/** Apply min-width property constraints on min/max values. */

/* virtual */ void
ScrollableContainer::ApplyMinWidthProperty(const HTMLayoutProperties& props, LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width)
{
	if (packed3.is_shrink_to_fit)
		ShrinkToFitContainer::ApplyMinWidthProperty(props, min_width, normal_min_width, max_width);
	else
		BlockContainer::ApplyMinWidthProperty(props, min_width, normal_min_width, max_width);
}

/** Clear min/max width. */

/* virtual */ void
ScrollableContainer::ClearMinMaxWidth()
{
	sc_packed.lock_auto_scrollbars = 0;

	if (packed3.is_shrink_to_fit)
		ShrinkToFitContainer::ClearMinMaxWidth();
	else
		BlockContainer::ClearMinMaxWidth();
}

/** Signal that content may have changed. */

/* virtual */ void
ScrollableContainer::SignalChange(FramesDocument* doc)
{
	if (packed3.is_shrink_to_fit)
		ShrinkToFitContainer::SignalChange(doc);
	else
		BlockContainer::SignalChange(doc);

	if (HasScrollbarVisibilityChanged())
		// Containing block established by this container has changed; reflow

		placeholder->MarkDirty(doc, FALSE);
}

/** Update screen. */

/* virtual */ void
ScrollableContainer::UpdateScreen(LayoutInfo& info)
{
	if (packed3.is_shrink_to_fit)
		ShrinkToFitContainer::UpdateScreen(info);
	else
		BlockContainer::UpdateScreen(info);

	if (reflow_state)
	{
		if (HasScrollbarVisibilityChanged())
		{
			// Scrollbars appeared or disappeared.

			info.workplace->SetReflowElement(GetHtmlElement());
			SaveScrollPos(placeholder->GetCascade()->html_element, scroll_x, scroll_y);
		}

		FinishScrollable(*placeholder->GetCascade()->GetProps(), GetWidth(), height);
	}
}

/** Get scroll offset, if applicable for this type of box / content. */

/* virtual */ OpPoint
ScrollableContainer::GetScrollOffset()
{
	return OpPoint(GetViewX(), GetViewY());
}

/** Set scroll offset, if applicable for this type of box / content. */

/* virtual */ void
ScrollableContainer::SetScrollOffset(int *x, int *y)
{
	if (x)
		SetViewX(LayoutCoord(*x), TRUE);

	if (y)
		SetViewY(LayoutCoord(*y), TRUE);
}

/** Traverse box with children. */

/* virtual */ void
ScrollableContainer::Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	const HTMLayoutProperties& props = *layout_props->GetProps();
	LayoutCoord scrollbar_adjustment = LayoutCoord(LeftHandScrollbar() ? GetVerticalScrollbarWidth() : 0);
	LayoutCoord inner_width = GetWidth() - LayoutCoord(props.border.left.width + props.border.right.width + GetVerticalScrollbarWidth());
	LayoutCoord inner_height = LayoutCoord(height - props.border.top.width - props.border.bottom.width - GetHorizontalScrollbarWidth());
	TraverseInfo traverse_info;

	if (traversal_object->EnterScrollable(props, this, inner_width, inner_height, traverse_info))
	{
		LayoutCoord view_x = GetViewX();
		LayoutCoord view_y = GetViewY();

		if (traverse_info.pretend_noscroll)
		{
			view_x = LayoutCoord(0);
			view_y = LayoutCoord(0);
		}

		LayoutCoord root_scroll_translation_x(0);
		LayoutCoord root_scroll_translation_y(0);
		BOOL positioned = placeholder->IsPositionedBox();

		traversal_object->Translate(scrollbar_adjustment - view_x, -view_y);
		traversal_object->TranslateRoot(scrollbar_adjustment - view_x, -view_y);
		traversal_object->TranslateScroll(view_x, view_y);

		if (positioned)
			traversal_object->SyncRootScroll(root_scroll_translation_x, root_scroll_translation_y);

		if (MultiColumnContainer* multicol_container = GetMultiColumnContainer())
		{
			LayoutCoord translate_y(LayoutCoord(props.border.top.width) + props.padding_top);

			traversal_object->Translate(LayoutCoord(0), translate_y);
			traversal_object->EnterMultiColumnContainer(layout_props, multicol_container);
			multicol_container->TraverseRows(traversal_object, layout_props);
			traversal_object->Translate(LayoutCoord(0), -translate_y);
		}
		else
			Container::Traverse(traversal_object, layout_props);

		if (positioned)
			traversal_object->TranslateRootScroll(-root_scroll_translation_x, -root_scroll_translation_y);

		traversal_object->TranslateScroll(-view_x, -view_y);
		traversal_object->TranslateRoot(view_x - scrollbar_adjustment, view_y);
		traversal_object->Translate(view_x - scrollbar_adjustment, view_y);

		traversal_object->LeaveScrollable(this, traverse_info);
	}
}

/** Finish reflowing box. */

/* virtual */ LAYST
ScrollableContainer::FinishLayout(LayoutInfo& info)
{
	if (reflow_state)
	{
		LayoutCoord view_x = GetViewX();
		LayoutCoord view_y = GetViewY();

		info.Translate(view_x, view_y);

		if (placeholder->IsPositionedBox())
			info.TranslateRoot(view_x, view_y);
		else
		{
			info.nonpos_scroll_x -= view_x;
			info.nonpos_scroll_y -= view_y;
		}

		SetPosition(info.visual_device->GetCTM());
	}

	if (packed3.is_shrink_to_fit)
		return ShrinkToFitContainer::FinishLayout(info);
	else
		return BlockContainer::FinishLayout(info);
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
ScrollableContainer::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	OP_BOOLEAN size_changed;
	LayoutCoord old_width = block_width;

	packed3.is_shrink_to_fit = cascade->GetProps()->IsShrinkToFit(cascade->html_element->Type());

	if (packed3.is_shrink_to_fit)
		size_changed = ShrinkToFitContainer::ComputeSize(cascade, info);
	else
		size_changed = BlockContainer::ComputeSize(cascade, info);

	if (block_width != old_width)
		UnlockAutoScrollbars();

	CoreView::SetFixedPositionSubtree(GetFixedPositionedAncestor(cascade));

	return size_changed;
}

/** Get baseline of first line or return this' height if searching for
	an ancestor inline-block baseline (last_line being TRUE). */

/* virtual */ LayoutCoord
ScrollableContainer::GetBaseline(BOOL last_line /*= FALSE*/) const
{
	/* FIXME: We should ideally return the bottom margin edge (not the bottom
	   border edge), but that requires the cascade. */

	OP_ASSERT(!placeholder->IsInlineBox());

	return last_line ? height : Container::GetBaseline(FALSE);
}

/** Get baseline of inline block or content of inline element that
	has a baseline (e.g. ReplacedContent with baseline, BulletContent). */

/* virtual */ LayoutCoord
ScrollableContainer::GetBaseline(const HTMLayoutProperties& props) const
{
	return Container::GetBaseline(props);
}

/** Reflow box and children. */

/* virtual */ LAYST
ScrollableContainer::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
	{
		OP_ASSERT(cascade->GetProps()->overflow_x != CSS_VALUE_visible && cascade->GetProps()->overflow_y != CSS_VALUE_visible);

		RETURN_VALUE_IF_ERROR(SetUpScrollable(cascade, info), LAYOUT_OUT_OF_MEMORY);

		LayoutCoord view_x = GetViewX();
		LayoutCoord view_y = GetViewY();

		info.Translate(-view_x, -view_y);

		if (placeholder->IsPositionedBox())
			info.TranslateRoot(-view_x, -view_y);
		else
		{
			info.nonpos_scroll_x += view_x;
			info.nonpos_scroll_y += view_y;
		}
	}

	return BlockContainer::Layout(cascade, info, first_child, start_position);
}

/** Return TRUE if this element can be split into multiple outer columns or pages. */

/* virtual */ BOOL
ScrollableContainer::IsColumnizable() const
{
	/* If there are no scrollbars, we _could_ have split the container over
	   multiple columns without making it look bad. However, since a
	   ScrollableContainer is a CoreView, and a CoreView is a rectangle, we
	   cannot really do that anyway. This is especially true because we update
	   the CoreView's position every time we paint the ScrollableContainer. Its
	   position would then be updated for each column painted (being moved back
	   and forth), causing infinite paint loops. */

	return FALSE;
}

/* virtual */ void
ScrollableContainer::HandleEvent(FramesDocument* doc, DOM_EventType ev, int offset_x, int offset_y)
{
#ifndef MOUSELESS
	GenerateWidgetEvent(ev, offset_x, offset_y);
#endif // !MOUSELESS
}

/** Calculate and return bottom margins of layout element. See declaration in header file for more information. */

/* virtual */ OP_BOOLEAN
Content::CalculateBottomMargins(LayoutProperties* cascade, LayoutInfo& info, VerticalMargin* bottom_margin, BOOL include_this) const
{
	if (include_this)
	{
		const HTMLayoutProperties& props = *cascade->GetProps();

		if (props.margin_bottom)
			bottom_margin->CollapseWithBottomMargin(props, TRUE);
	}

	return OpBoolean::IS_TRUE;
}

/** Calculate and return top margins of layout element. See declaration in header file for more information. */

/* virtual */ OP_BOOLEAN
Content::CalculateTopMargins(LayoutProperties* cascade, LayoutInfo& info, VerticalMargin* top_margin, BOOL include_self_bottom) const
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	if (props.margin_top)
		top_margin->CollapseWithTopMargin(props);

	return OpBoolean::IS_TRUE;
}

/** Find the parent CoreView in the element tree, or NULL if there is none. */

/* static */ CoreView*
Content::FindParentCoreView(HTML_Element* html_element)
{
	OP_ASSERT(html_element != NULL);

	if (html_element->GetLayoutBox() && !html_element->GetLayoutBox()->IsFixedPositionedBox())
	{
		BOOL is_abspos = html_element->GetLayoutBox()->IsAbsolutePositionedBox();

		for (HTML_Element* helm = Box::GetContainingElement(html_element, is_abspos, FALSE);
			 helm && helm->GetLayoutBox();
			 helm = Box::GetContainingElement(helm, is_abspos, FALSE))
		{
			is_abspos = helm->GetLayoutBox()->IsAbsolutePositionedBox();

			if (ScrollableArea *parent_sc = helm->GetLayoutBox()->GetScrollable())
				return parent_sc;
		}
	}

	return NULL;
}

/** Find the nearest input context by walking the ancestry. */

/* static */ OpInputContext*
Content::FindInputContext(HTML_Element* html_element)
{
	do
	{
		if (Box* box = html_element->GetLayoutBox())
			if (Content* content = box->GetContent())
				if (OpInputContext* input_context = content->GetInputContext())
					return input_context;

		html_element = html_element->Parent();
	}
	while (html_element);

	return NULL;
}

/** Find the nearest ancestor that is a scrollable or paged container. */

/* static */ Content*
Content::FindParentOverflowContent(HTML_Element* html_element)
{
	for (HTML_Element* elm = html_element->Parent(); elm; elm = elm->Parent())
		if (Box* box = elm->GetLayoutBox())
			if (box->GetScrollable())
				return box->GetContent();
#ifdef PAGED_MEDIA_SUPPORT
			else
				if (Container* inner_container = box->GetContainer())
					if (inner_container->GetPagedContainer())
						return inner_container;
#endif // PAGED_MEDIA_SUPPORT

	return NULL;
}

/** Return the first fixed positioned ancestor or NULL if no such exists. */

/* static */ HTML_Element*
Content::GetFixedPositionedAncestor(LayoutProperties* cascade)
{
	for (; cascade && cascade->html_element; cascade = cascade->Pred())
		if (cascade->html_element->GetLayoutBox() && cascade->html_element->GetLayoutBox()->IsFixedPositionedBox())
			return cascade->html_element;

	return NULL;
}

/** Set pending run-in. */

void
Container::SetPendingRunIn(Content_Box* run_in, LayoutInfo& info, LayoutCoord border_padding_margin)
{
	reflow_state->pending_run_in_box = run_in;
	reflow_state->pending_compact_space = border_padding_margin;

	if (run_in->IsInlineRunInBox())
		((InlineRunInBox*) run_in)->SetRunInBlockConversion(FALSE, info);
}

/** Get pending run-in. */

Content_Box*
Container::StealPendingRunIn(LayoutInfo& info, LayoutCoord& pending_compact_space)
{
	Content_Box* run_in = reflow_state->pending_run_in_box;

	reflow_state->pending_run_in_box = NULL;

	if (run_in && run_in->IsInlineRunInBox())
		((InlineRunInBox*) run_in)->SetRunInBlockConversion(TRUE, info);

	pending_compact_space = reflow_state->pending_compact_space;

	return run_in;
}

/** Calculate and return bottom margins of layout element. See declaration in header file for more information. */

/* virtual */ OP_BOOLEAN
Container::CalculateBottomMargins(LayoutProperties* cascade, LayoutInfo& info, VerticalMargin* bottom_margin, BOOL include_this) const
{
	if (include_this)
		Content::CalculateBottomMargins(cascade, info, bottom_margin, include_this);

	OP_ASSERT(cascade->html_element != NULL);

	const HTMLayoutProperties& props = *cascade->GetProps();

	if (!placeholder->GetLocalSpaceManager()
		&& (props.content_height == CONTENT_HEIGHT_AUTO || props.content_height == 0)
		&& !props.padding_bottom && !props.border.bottom.width
		&& cascade->html_element->Type() != Markup::HTE_HTML)
	{
		for (VerticalLayout* vertical_layout = (VerticalLayout*) layout_stack.Last(); vertical_layout; vertical_layout = vertical_layout->Pred())
			if (vertical_layout->IsInStack())
			{
				OP_BOOLEAN status = vertical_layout->CalculateBottomMargins(cascade, info, bottom_margin);

				if (status == OpStatus::ERR_NO_MEMORY)
					return OpStatus::ERR_NO_MEMORY;
				else
					if (status == OpBoolean::IS_TRUE)
						break;
			}

		if (packed.no_content && !props.padding_top && !props.border.top.width && props.min_height == 0)
		{
			// The top and bottom margins of this element are adjoining; collapse them

			Content::CalculateTopMargins(cascade, info, bottom_margin, TRUE);
			return OpBoolean::IS_FALSE;
		}
	}

	return OpBoolean::IS_TRUE;
}

/** Calculate and return top margins of layout element. See declaration in header file for more information. */

/* virtual */ OP_BOOLEAN
Container::CalculateTopMargins(LayoutProperties* cascade, LayoutInfo& info, VerticalMargin* top_margin, BOOL include_self_bottom) const
{
	if (include_self_bottom)
		Content::CalculateTopMargins(cascade, info, top_margin, TRUE);

	if (!packed.stop_top_margin_collapsing)
	{
		for (VerticalLayout* vertical_layout = (VerticalLayout*) layout_stack.First(); vertical_layout; vertical_layout = vertical_layout->Suc())
			if (vertical_layout->IsInStack())
			{
				OP_BOOLEAN status = vertical_layout->CalculateTopMargins(cascade, info, top_margin);

				if (status == OpStatus::ERR_NO_MEMORY)
					return OpStatus::ERR_NO_MEMORY;
				else
					if (status == OpBoolean::IS_TRUE)
						break;
			}

		if (packed.no_content)
		{
			// The top and bottom margins of this element are adjoining; collapse them

			if (include_self_bottom)
				Content::CalculateBottomMargins(cascade, info, top_margin);

			return OpBoolean::IS_FALSE;
		}
	}

	return OpBoolean::IS_TRUE;
}

/** Get new floating block to reflow onto. */

LAYST
Container::GetNewFloatStage1(LayoutInfo& info, LayoutProperties* box_cascade, FloatingBox* floating_box)
{
	/* Commit pending line content, but not if it's an outside list item marker only.
	   In such case, it should be treated same as if the float was before
	   the line in the reflow order. */
	LAYST status;
	const HTMLayoutProperties& props = *placeholder->GetCascade()->GetProps();
	if ((reflow_state->list_item_marker_state & MARKER_BEFORE_NEXT_ELEMENT) &&
		props.list_style_pos == CSS_VALUE_outside ||
		props.white_space == CSS_VALUE_nowrap)
		status = LAYOUT_CONTINUE;
	else
		status = CommitLineContent(info, FALSE);

	if (status != LAYOUT_CONTINUE)
		return status;
	else
	{
		Line* current_line = GetReflowLine();

		reflow_state->is_run_in = FALSE;

		if (!current_line && reflow_state->reflow_line)
		{
			// Must close previous

			status = CloseVerticalLayout(info, BLOCK);

			if (status != LAYOUT_CONTINUE)
				return status;
		}
#ifdef PAGED_MEDIA_SUPPORT
		else
			if (info.paged_media != PAGEBREAK_OFF)
				if (!SkipPageBreaks(info))
					return LAYOUT_OUT_OF_MEMORY;
#endif // PAGED_MEDIA_SUPPORT

		if (current_line)
			floating_box->SetY(current_line->GetY());
		else
			floating_box->SetY(reflow_state->reflow_position + reflow_state->margin_state.GetHeight());

		floating_box->SetX(props.padding_left + LayoutCoord(props.border.left.width));

		if (reflow_state->calculate_min_max_widths)
		{
			LayoutCoord min_y;

			if (current_line)
				min_y = reflow_state->min_reflow_position;
			else
				min_y = reflow_state->min_reflow_position + reflow_state->margin_state.GetHeightNonPercent();

			LayoutCoord bfc_min_y(0);
			LayoutCoord dummy_bfc_y(0);
			LayoutCoord dummy_bfc_x(0);
			GetBfcOffsets(dummy_bfc_x, dummy_bfc_y, bfc_min_y);

			floating_box->SetBfcMinY(bfc_min_y + min_y);
			floating_box->SetMinY(min_y);
		}
		else
		{
			// Need to set these, just to avoid unintialized variable access later.

			floating_box->SetBfcMinY(LayoutCoord(0));
			floating_box->SetMinY(LayoutCoord(0));
		}

		return LAYOUT_CONTINUE;
	}
}

/** Get new floating block to reflow onto. */

void
Container::GetNewFloatStage2(LayoutInfo& info, LayoutProperties* box_cascade, FloatingBox* floating_box)
{
	const HTMLayoutProperties& props = *box_cascade->GetProps();
	SpaceManager* space_manager = box_cascade->space_manager;
	BOOL align_left = props.float_type == CSS_VALUE_left;

	LayoutCoord float_width = floating_box->GetWidth();
	LayoutCoord full_width = float_width + props.margin_left + props.margin_right;
	LayoutCoord space = GetContentWidth();
	LayoutCoord block_x = floating_box->GetX();
	LayoutCoord block_y = floating_box->GetY();
	LayoutCoord dummy_bfc_min_y(0);
	LayoutCoord bfc_y(0);
	LayoutCoord bfc_x(0);
	Line* current_line = GetReflowLine();

	GetBfcOffsets(bfc_x, bfc_y, dummy_bfc_min_y);

	/* If we are on a line it can't be an outside list item marker line that
	   has no other content yet, because in such case we do not yet commit any
	   line content when the float arrives. */
	OP_ASSERT(!current_line || !current_line->IsInStack() || !current_line->IsOutsideListMarkerLine() ||
		!(reflow_state->list_item_marker_state & FLEXIBLE_MARKER_NOT_ALIGNED));

	if (current_line && current_line->IsInStack() && !reflow_state->first_moved_floating_box)
		if (!info.hld_profile->IsInStrictMode() || current_line->GetSpaceLeft() + current_line->GetTrailingWhitespace() < full_width)
			/* There isn't space for the float on this line, or we're in quirks mode. We want to
			   move it below the line, but must wait until reflow of this line is complete, because
			   its height isn't known until then. */

			reflow_state->first_moved_floating_box = floating_box;

	if (!floating_box->InList())
	{
		LayoutCoord virtual_position = CalculateBlockVirtualPosition(box_cascade);

		if (virtual_position != LAYOUT_COORD_MIN)
		{
			/* We're either actually on a line, or this float follows a block
			   that terminated a line. This float then needs to be traversed
			   during line traversal if traversing in logical order. */

			floating_box->SetOnLine(TRUE);
			floating_box->SetVirtualPosition(virtual_position);
		}
		else
		{
			OP_ASSERT(!current_line);
			floating_box->SetOnLine(FALSE);
		}

		if (current_line)
			// If we're reflowing a line, place the float before it in the layout stack.

			floating_box->Precede(current_line);
		else
			AddElement(floating_box);
	}

	FloatingBox* prev_last_float = space_manager->GetLastFloat();
	LayoutCoord forced_top = prev_last_float ? prev_last_float->GetBfcY() - prev_last_float->GetMarginTop() - bfc_y : LayoutCoord(0);

	if (block_y < forced_top)
		// Cannot be above previous floats

		block_y = forced_top;

	if (props.clear != CSS_VALUE_none)
	{
		LayoutCoord forced_bfc_top = space_manager->FindBfcBottom((CSSValue) props.clear);

		if (forced_bfc_top != LAYOUT_COORD_MIN)
		{
			forced_top = forced_bfc_top - bfc_y;

			// Apply clearance

			if (block_y < forced_top)
				block_y = forced_top;
		}
	}

	/* Now we have applied clearance (if any) and at this point we have a
	   static position of the float's margin edges. Now find the final position
	   (make sure it doesn't overlap with other floats, convert from margin
	   edges to border edges, and add any offset caused by position:relative),
	   unless this float is to be put below the line currently being laid
	   out. If it is to be put below the current line, we first need to know
	   the height of the line before we can calculate the desired Y position,
	   and only then we can check with the space manager to calculate its final
	   position. This takes place in PushFloatsBelowLine(). */

	if (!reflow_state->first_moved_floating_box)
	{
		/* Allocate space from the space manager (calculate static-positioned
		   margin edge position). */

		GetSpace(space_manager, block_y, block_x, space, full_width, LAYOUT_COORD_MAX);

		if (!align_left)
			block_x += space - full_width;

		if (full_width > 0 && current_line)
			if (!current_line->GetLayoutHeight() ?
				block_y == current_line->GetY() :
				block_y < current_line->GetY() + current_line->GetLayoutHeight())
			{
				// New float has used space from the current line; must adjust

				current_line->SetWidth(MAX(current_line->GetUsedSpace(), current_line->GetWidth() - full_width));

				if (align_left)
					// Must move current line to the right of the new float

					current_line->SetPosition(current_line->GetX() + full_width, current_line->GetY());
			}

		/* Add margins. GetX() and GetY() should return border coordinates for
		   all box types. */

		block_x += props.margin_left;
		block_y += props.margin_top;
	}

	/* Set BFC offsets now. They should not contain offsets caused by
	   relative-positioning. */

	floating_box->SetBfcX(bfc_x + block_x);
	floating_box->SetBfcY(bfc_y + block_y);

	/* Unless we have to wait until the current line has been laid out, perform
	   the last step in float positioning. */

	if (!reflow_state->first_moved_floating_box)
	{
		// Add offset caused by position:relative

		LayoutCoord left;
		LayoutCoord top;

		floating_box->GetRelativeOffsets(left, top);

		block_x += left;
		block_y += top;
	}

#ifdef PAGED_MEDIA_SUPPORT
	if (info.paged_media != PAGEBREAK_OFF && !box_cascade->multipane_container)
		info.doc->RewindPage(0);
#endif // PAGED_MEDIA_SUPPORT

	info.Translate(block_x - floating_box->GetX(), block_y - floating_box->GetY());

	floating_box->SetX(block_x);
	floating_box->SetY(block_y);

	space_manager->AddFloat(floating_box);
}

/** Get new block for absolute positioned box. */

LAYST
Container::GetNewAbsolutePositionedBlock(LayoutInfo& info, LayoutProperties* box_cascade, AbsolutePositionedBox* abs_box)
{
	Line* current_line = GetReflowLine();

	if (!current_line && reflow_state->break_before_content)
	{
		GetNewLine(info, reflow_state->break_before_content, LayoutCoord(0));
		current_line = GetReflowLine();
	}

	const HTMLayoutProperties& box_props = *box_cascade->GetProps();
	LayoutCoord virtual_position = CalculateBlockVirtualPosition(box_cascade);

	if (box_props.order != 0)
		/* The 'order' property is non-default. Propagate the value, in case
		   this is a special flex item for an absolutely positioned child. */

		if (placeholder->IsFlexItemBox())
			((FlexItemBox*) placeholder)->PropagateOrder(box_props.order);

	if (!current_line)
	{
		if (reflow_state->reflow_line)
		{
			// Must close previous

			LAYST status = CloseVerticalLayout(info, BLOCK);

			if (status != LAYOUT_CONTINUE)
				return status;
		}

		AddElement(abs_box);
	}

	if (virtual_position == LAYOUT_COORD_MIN)
	{
		OP_ASSERT(!current_line);
		abs_box->SetOnLine(FALSE);
	}
	else
	{
		abs_box->SetOnLine(TRUE);
		abs_box->SetVirtualPosition(virtual_position);
	}

	reflow_state->is_run_in = FALSE;
	if (reflow_state->line_has_content)
		reflow_state->last_base_character = 0x200B;

	return LAYOUT_CONTINUE;
}

/** Get new column/pane-attached float. */

LAYST
Container::GetNewPaneFloat(LayoutInfo& info, LayoutProperties* box_cascade, FloatedPaneBox* box)
{
	OP_ASSERT(box->GetCascade()->multipane_container);

	/* Put it into the layout stack and calculate horizontal position (which is
	   relative to the left edge of the pane in which it will eventually end
	   up). It will be positioned vertically during columnization /
	   pagination. */

	Line* current_line = GetReflowLine();

	if (current_line)
		box->Precede(current_line);
	else
	{
		if (reflow_state->reflow_line)
		{
			// Must close previous

			LAYST status = CloseVerticalLayout(info, BLOCK);

			if (status != LAYOUT_CONTINUE)
				return status;
		}

		AddElement(box);
	}

	const HTMLayoutProperties& box_props = *box_cascade->GetProps();
	const HTMLayoutProperties& mp_props = *box_cascade->multipane_container->GetPlaceholder()->GetCascade()->GetProps();

#ifdef SUPPORT_TEXT_DIRECTION
	if (mp_props.direction == CSS_VALUE_rtl)
	{
		LayoutCoord available_width = box->GetAvailableWidth(box_cascade);
		LayoutCoord margin_right;

		if (box_props.GetMarginRightAuto())
		{
			margin_right = available_width - box->GetWidth();

			if (box_props.GetMarginLeftAuto())
				margin_right /= 2;

			if (margin_right < 0)
				margin_right = LayoutCoord(0);
		}
		else
			margin_right = box_props.margin_right;

		box->SetX(LayoutCoord(mp_props.border.left.width) + mp_props.padding_left +
				  available_width - box->GetWidth() - margin_right);
	}
	else
#endif // SUPPORT_TEXT_DIRECTION
	{
		LayoutCoord margin_left;

		if (box_props.GetMarginLeftAuto())
		{
			margin_left = box->GetAvailableWidth(box_cascade) - box->GetWidth();

			if (box_props.GetMarginRightAuto())
				margin_left /= 2;

			if (margin_left < 0)
				margin_left = LayoutCoord(0);
		}
		else
			margin_left = box_props.margin_left;

		box->SetX(LayoutCoord(mp_props.border.left.width) + mp_props.padding_left + margin_left);
	}

	reflow_state->is_run_in = FALSE;

	return LAYOUT_CONTINUE;
}

/** Get current static position. */

LayoutCoord
Container::GetStaticPosition(const LayoutInfo& info, HTML_Element *element, BOOL inline_position) const
{
	/* For LTR: Get the distance from the left border edge of the nearest container
	   to the left margin edge of the next added statically positioned block/inline

	   For RTL: Get the distance from the right border edge of the nearest container
	   to the right margin edge of the next added statically positioned block/inline.

	   These are guessed positions. */

	LayoutProperties* cascade = placeholder->GetCascade();
	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord position;

#ifdef SUPPORT_TEXT_DIRECTION
	BOOL right_to_left = props.direction == CSS_VALUE_rtl;
#else
	BOOL right_to_left = FALSE;
#endif // SUPPORT_TEXT_DIRECTION

	if (inline_position)
	{
		/* Hypothetical horizontal static inline position. */

		Line* line = GetReflowLine();
		BOOL guess_line_will_break = !line;

		if (line)
		{
			/* Try to guess where the pending content will end up. */

			LayoutCoord word_width = reflow_state->GetWordWidth();
			if (word_width || reflow_state->GetNoncontentWidth())
			{
				word_width += reflow_state->GetNoncontentWidth();

				if (word_width > line->GetSpaceLeft() ||
					reflow_state->max_line_height < reflow_state->line_state.uncommitted.height_above_baseline - reflow_state->line_state.uncommitted.height_below_baseline)
				{
					/* This line will probably break and the pending content moved to next line. If
					   so, don't use the current line as it is irrelevant. */
					guess_line_will_break = TRUE;
				}
			}
		}

		LayoutCoord line_x;
		LayoutCoord line_width;
		LayoutCoord virtual_offset;

		if (guess_line_will_break)
		{
			line_width = GetContentWidth();
			LayoutCoord dummy;

			GetLinePosition(info, element, reflow_state->reflow_line, line_width, LayoutCoord(0), LayoutCoord(0), line_x, dummy);

			/* If the pending content, which we guess will be moved to the next line,
			   is wider than that line, set the horizontal position of the absolutely
			   positioned box to the start position of that line, as it would have
			   ended up at the start of a line if it was a static inline. Note that
			   this position will not be correct if there are floats involved that
			   would cause the line that the absolutely positioned ends up on to have
			   a different width/offset than the line we're getting above in
			   GetLinePosition. */
			LayoutCoord pending_content = reflow_state->GetPendingContent();
			if (pending_content >= line_width)
				pending_content = LayoutCoord(0);

			virtual_offset = pending_content;
		}
		else
		{
			line_x = line->GetX();
			line_width = line->GetWidth();
			virtual_offset = GetLinePendingPosition() - line->GetStartPosition();
		}

		if (right_to_left)
			position = placeholder->GetWidth() - (line_x + line_width) + virtual_offset;
		else
			position = line_x + virtual_offset;
	}
	else
		/* Hypothetical horizontal static block position. */
		if (right_to_left)
			position = props.padding_right + LayoutCoord(props.border.right.width);
		else
			position = props.padding_left + LayoutCoord(props.border.left.width);

	return position;
}

/** Calculate virtual X position to be used with a block that's about to be added. */

LayoutCoord
Container::CalculateBlockVirtualPosition(LayoutProperties* box_cascade) const
{
	LayoutCoord virtual_position = LAYOUT_COORD_MIN;
	LayoutCoord line_start_pos = LAYOUT_COORD_MIN;

	if (Line* current_line = GetReflowLine())
	{
		/* Use the current position on the current line as virtual position. We
		   subtract one since the current position is just past the end of the
		   line so far. If it turns out that there is no real line content to
		   follow the block (always the case for in-flow boxes; they terminate
		   lines), it would otherwise end up outside the line. */

		virtual_position = GetLinePendingPosition() - LayoutCoord(1);
		line_start_pos = current_line->GetStartPosition();
	}
	else
	{
		if (reflow_state->reflow_line)
		{
			if (reflow_state->reflow_line->IsBlock())
				/* Use the previous block's virtual position - if any (it may be
				   LAYOUT_COORD_MIN, of course). */

				virtual_position = ((BlockBox*) reflow_state->reflow_line)->GetVirtualPosition();
		}
		else if (reflow_state->break_before_content)
		{
			/* There is a pending line - not materialised yet */

			virtual_position = GetLinePendingPosition() - LayoutCoord(1);
			line_start_pos = LayoutCoord(0);
		}
	}

	if (virtual_position < line_start_pos)
		/* But don't allow it to precede the start of the current line.
		   That could otherwise happen if the line were empty or had
		   content with negative horizontal margins. */

		virtual_position = line_start_pos;

	return virtual_position;
}

/** Move run-in or compact element out of container and into a later block
	box. */

void
Container::RemovePreviousLayoutElement(LayoutInfo &info)
{
	VerticalLayout* current = reflow_state->reflow_line;

	if (current->IsBlock())
	{
		VerticalLayout* previous = current->Pred();
		LayoutCoord offset = previous->GetStackPosition() - current->GetStackPosition();

		((BlockBox*) current)->SetY(previous->GetStackPosition());
		info.Translate(LayoutCoord(0), offset);
	}
}

#ifdef LIMIT_PARAGRAPH_WIDTH_SCAN_FOR_DELIMITER_CHARACTERS

BOOL
Container::ScanForMenuDelimiterCharacter(HTML_Element* start, HTML_Element* stop) const
{
	HTML_Element* scan = start;

	int scanned_chars = 0;

	while (scan && scan != stop && scanned_chars < 200)
	{
		if (scan->Type() == Markup::HTE_TEXT)
		{
			const uni_char* text_content = (uni_char*) scan->TextContent();

			int text_content_length = scan->GetTextContentLength();

			for (int i = 0; i <= MIN(200 - scanned_chars, text_content_length - 1); i++)
			{
				if (text_content[i] == 0x2022) // bullet
					return TRUE;

				if (text_content[i] == 0x7c) //"pipe"
					return TRUE;

				if (text_content[i] == 0x3a && i >= 1 &&text_content[i-1] == 0x3a) // double colon
					return TRUE;
			}
			scanned_chars += text_content_length;
		}
		/* Don't scan past Markup::HTE_BR, since that will create a new line. */
		else if (scan->IsMatchingType(Markup::HTE_BR, NS_HTML))
			return FALSE;

		scan = (HTML_Element*)scan->Next();
	}
	return FALSE;
}

#endif

void
Container::LimitParagraphWidth(const LayoutInfo& info, LayoutProperties* cascade,
							   HTML_Element* start_element,
							   VerticalLayout* prev_vertical_layout,
							   LayoutCoord line_height, LayoutCoord min_width,
							   LayoutCoord before_line_x, LayoutCoord before_line_y,
							   LayoutCoord& width, LayoutCoord& line_x, LayoutCoord& line_y) const
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	SpaceManager* space_manager = placeholder->GetLocalSpaceManager();

	if (!space_manager)
		space_manager = cascade->space_manager;

	OP_ASSERT(props.max_paragraph_width > 0);

	/* Limited paragraph width is enabled. Determine if this line should be shortened
	   to fit the browser window width. See also the spec, which can probably be found
	   here: http://projects/Core2profiles/specifications/text_wrap.html */

	LayoutCoord limit_line_width = LayoutCoord(props.max_paragraph_width);

	/* If the line follows an implicit line break, we can be sure that it is neither a
	   headline nor a horizontal menu, and therefore such lines will have an increased
	   chance of becoming shortened. */

	BOOL implicit_line_break = (prev_vertical_layout &&
								prev_vertical_layout->IsLine() &&
								!((Line*) prev_vertical_layout)->HasForcedBreak());

#ifdef LIMIT_PARAGRAPH_WIDTH_INCLUDE_FLOAT
	limit_line_width -= line_x - before_line_x;
#endif

	/* Don't shorten what we think are headlines. We assume that lines we know that
	   will become tall are headlines, unless the line follows an implicit line break
	   (because that means that this line is on the same virtual line as the previous
	   line, and therefore it can't possibly be a headline). FIXME: Lines may become
	   tall because of an image, so assuming that tall lines are headlines is probably
	   too simplistic. It might be better to base this decision on font size.

	   Also don't shorten lines when inside horizontal MARQUEEs, since that would
	   modify layout for no reason (MARQUEEs scroll, remember?).

	   And obviously, there's no point in shortening lines that are already short
	   enough. */

	if (!reflow_state->was_lpw_ignored && width > limit_line_width)
	{
		// This line is a candidate for shortening.

		BOOL shorten_line = FALSE;

		if (!props.IsInsideHorizontalMarquee()
#if TEXT_WRAP_DONT_WRAP_FONT_SIZE > 0
			&& (implicit_line_break ||
				reflow_state->line_state.uncommitted.height_above_baseline + reflow_state->line_state.uncommitted.height_below_baseline < TEXT_WRAP_DONT_WRAP_FONT_SIZE)
#endif // TEXT_WRAP_DONT_WRAP_FONT_SIZE > 0
			)
		{
			shorten_line = implicit_line_break;

			if (!shorten_line)
			{
				// Examine the elements that are to be put on the line.

				HTML_Element* stop = cascade->html_element->NextSibling();

#ifdef LIMIT_PARAGRAPH_WIDTH_AVOID_BREAK_ON_REPLACED_CONTENT
				int replaced_content_counter = 0;
#endif
				HTML_Element* content = start_element;
				while (content != stop)
				{
					Box* box = content->GetLayoutBox();
					if (box)
					{
						if (box->IsBlockBox())
						{
							if (box->IsFloatingBox() || box->IsAbsolutePositionedBox())
							{
								content = content->NextSibling();
								continue;
							}
							else
								break; // Means the line has ended.
						}
						else
						{
							/* Don't shorten the line if we find an inline LI element. We
							   assume that such elements are part of a horizontal menu, which
							   means that we shouldn't shorten the line. */

							if (content->IsMatchingType(Markup::HTE_LI, NS_HTML) && !content->GetIsListMarkerPseudoElement())
								break;

							/* Non-replaced inline "non-"content other than LI is
							   uninteresting. Look for real content now. */

							if (!box->IsInlineContent())
							{
#ifdef LIMIT_PARAGRAPH_WIDTH_AVOID_BREAK_ON_REPLACED_CONTENT
								if (box->IsTextBox())
								{
									if (((Text_Box*)box)->IsWhitespaceOnly())
										/* If this is an empty text box, let's continue looking.
										   Otherwise, this is a candidate for wrapping. */
									{
										content = content->Next();
										continue;
									}
								}
								else if (content->IsMatchingType(Markup::HTE_BR, NS_HTML))
								{
									/* Assume line will be closed, if
									   encountering BR element. */
									shorten_line = TRUE;
									break;
								}
								else
								{
									/* Assume that everything that isn't text, is replaced content
									   (this is only almost true, but that's probably fine). Don't
									   shorten lines that begins with two instances of replaced
									   content, since we think they are part of a horizontal menu. */

									if (replaced_content_counter == 0)
									{
										replaced_content_counter++;
										content = content->Next();
										continue;
									}
									else
										break;
								}
#endif // LIMIT_PARAGRAPH_WIDTH_AVOID_BREAK_ON_REPLACED_CONTENT

#ifdef LIMIT_PARAGRAPH_WIDTH_SCAN_FOR_DELIMITER_CHARACTERS
								/* If we find something that we think is a typical delimiter
								   for a horizontal menu, don't shorten the line.  */

								if (ScanForMenuDelimiterCharacter(content, stop))
									break;
#endif // LIMIT_PARAGRAPH_WIDTH_SCAN_FOR_DELIMITER_CHARACTERS

								/* We have run out of excuses for not shortening the line, so
								   shorten it then. :) */

								shorten_line = TRUE;
								break;
							}
						}
					}

					content = content->Next();
				}
			}
		}

		if (shorten_line)
		{
			LayoutCoord old_width = width;

#ifdef LIMIT_PARAGRAPH_WIDTH_INCLUDE_FLOAT
			/* Policy: if the line is left aligned and has been shifted right by a
			   left float and:
			   - The remaining width is less than half of the max_paragraph_width
			   - OR min width doesnt fit beside the float within the max_paragraph_width
			   then try to find a new space for the line, further down. */

			if (props.text_align == CSS_VALUE_left &&
				(limit_line_width < props.max_paragraph_width / 2 ||
				 (line_x > before_line_x && min_width > limit_line_width)))
			{
				/* Remaining line is too narrow. Lets just request a space below
				   the float.

				   We do this by requesting a min width that is greater than the available
				   width. */

				/* Reset to the previous state. */
				width = MIN(GetContentWidth(), LayoutCoord(props.max_paragraph_width));
				line_x = before_line_x;
				line_y = before_line_y;

				reflow_state->max_line_height = GetSpace(space_manager, line_y, line_x, width, MAX(min_width, LayoutCoord(props.max_paragraph_width / 2 + 1)), line_height);
			}
			else
#endif // LIMIT_PARAGRAPH_WIDTH_INCLUDE_FLOAT
			{
				limit_line_width = MAX(min_width, limit_line_width);

				switch (props.text_align)
				{
				case CSS_VALUE_center:
					line_x += (width - limit_line_width) >> 1;
					break;

				case CSS_VALUE_right:
					line_x += width - limit_line_width;
					break;
				}

				width = limit_line_width;
			}
			reflow_state->apply_height_is_min_height |= width < old_width;
		}
		else
			reflow_state->was_lpw_ignored = TRUE;
	}
}

LayoutCoord
Container::GetLinePosition(const LayoutInfo& info, HTML_Element *element, VerticalLayout *prev, LayoutCoord& width, LayoutCoord min_width, LayoutCoord line_height, LayoutCoord& line_x, LayoutCoord& line_y) const
{
	LayoutProperties* cascade = placeholder->GetCascade();
	const HTMLayoutProperties& props = *cascade->GetProps();
	SpaceManager* space_manager = placeholder->GetLocalSpaceManager();

	LayoutCoord max_line_height(0);

	line_x = props.padding_left + LayoutCoord(props.border.left.width);
	line_y = reflow_state->reflow_position + reflow_state->margin_state.GetHeight();

	if (!space_manager)
		space_manager = cascade->space_manager;

	LayoutCoord before_line_x = line_x;
	LayoutCoord before_line_y = line_y;

	VerticalLock vertical_lock;

	if (props.white_space == CSS_VALUE_nowrap)
	{
#ifdef SUPPORT_TEXT_DIRECTION
		if (props.direction == CSS_VALUE_rtl)
			vertical_lock = VLOCK_KEEP_LEFT_FIXED;
		else
#endif //SUPPORT_TEXT_DIRECTION
			vertical_lock = VLOCK_KEEP_RIGHT_FIXED;
	}
	else
		vertical_lock = VLOCK_OFF;

	max_line_height = GetSpace(space_manager, line_y, line_x, width, min_width, line_height, vertical_lock);

	if (props.max_paragraph_width > 0)
		LimitParagraphWidth(info, cascade, element, prev, line_height, min_width, before_line_x, before_line_y, width, line_x, line_y);

	if (!prev)
	{
		width -= LayoutCoord(props.text_indent);
		line_x += LayoutCoord(props.text_indent);

#ifdef SUPPORT_TEXT_DIRECTION
		if (props.direction == CSS_VALUE_rtl)
			line_x -= LayoutCoord(props.text_indent); // move line left margin back again
#endif

		if (width < 0)
			width = LayoutCoord(0);
	}

	return max_line_height;
}

/** Get new line to reflow onto. */

LAYST
Container::GetNewLine(LayoutInfo& info, HTML_Element* element, LayoutCoord min_width)
{
	BOOL has_content = reflow_state->line_has_content;
	LAYST status = CloseVerticalLayout(info, LINE);

	if (status != LAYOUT_CONTINUE)
		return status;
	else
	{
		reflow_state->StartNewLine();

		LayoutProperties* cascade = placeholder->GetCascade();
		const HTMLayoutProperties& props = *cascade->GetProps();
		SpaceManager* space_manager = placeholder->GetLocalSpaceManager();
		Container* multipane_container = IsMultiPaneContainer() ? this : NULL;

		if (!space_manager)
			space_manager = cascade->space_manager;

		if (!multipane_container)
			multipane_container = cascade->multipane_container;

		if (
#ifdef PAGED_MEDIA_SUPPORT
			info.paged_media == PAGEBREAK_ON ||
#endif // PAGED_MEDIA_SUPPORT
			multipane_container)
		{
			if (BreakForced(reflow_state->page_break_policy) || BreakForced(reflow_state->column_break_policy))
				if (!InsertPageOrColumnBreak(info, reflow_state->page_break_policy, reflow_state->column_break_policy, multipane_container, space_manager))
					return LAYOUT_OUT_OF_MEMORY;

			reflow_state->page_break_policy = BREAK_ALLOW;
			reflow_state->column_break_policy = BREAK_ALLOW;
		}

		LayoutCoord line_x(0);
		LayoutCoord line_y(0);

		LayoutCoord width = GetContentWidth();
		LayoutCoord line_height;

		Line* line = OP_NEW(Line, ());
		if (!line)
			return LAYOUT_OUT_OF_MEMORY;

		if (IsCssFirstLine())
			line->SetFirstLine();

		if (min_width)
			has_content = TRUE;

		AddElement(line);

		if (!has_content)
			line->SetHasContent(FALSE);
		else
			SetMinimalLineHeight(info, props);

		line_height = reflow_state->line_state.uncommitted.height_above_baseline + reflow_state->line_state.uncommitted.height_below_baseline;
		if (line_height < reflow_state->line_state.uncommitted.minimum_height)
			line_height = reflow_state->line_state.uncommitted.minimum_height;

		VerticalLayout* prev_vertical_layout = line->GetPreviousElement();

		reflow_state->max_line_height = GetLinePosition(info, element, prev_vertical_layout, width, min_width, line_height, line_x, line_y);

		if (!prev_vertical_layout && reflow_state->calculate_min_max_widths && !props.GetTextIndentIsPercent())
		{
			reflow_state->pending_line_min_width += LayoutCoord(props.text_indent);
			reflow_state->pending_line_max_width += LayoutCoord(props.text_indent);
		}

		line->SetPosition(line_x, line_y);
		line->SetWidth(width);
		line->SetStartElement(element);
		line->SetStartPosition(reflow_state->GetLineCurrentPosition());

		if (element->GetIsListMarkerPseudoElement() && props.list_style_pos == CSS_VALUE_outside)
			line->SetIsOutsideListMarkerLine();

		return LAYOUT_CONTINUE;
	}
}

#ifdef PAGED_MEDIA_SUPPORT

/** Insert page break. */

BOOL
Container::GetExplicitPageBreak(const LayoutInfo& info, BREAK_POLICY page_break)
{
	VerticalLayout* prev = reflow_state->reflow_line;

	switch (page_break)
	{
	case BREAK_ALWAYS:
	case BREAK_LEFT:
	case BREAK_RIGHT:
		for (;;)
		{
			PageDescription* next_page_description = info.doc->AdvancePage(reflow_state->reflow_position);

			OP_ASSERT(info.doc->GetCurrentPage());

			if (next_page_description)
			{
				ExplicitPageBreak* explicit_page_break = OP_NEW(ExplicitPageBreak, (prev));

				if (explicit_page_break)
				{
					AddElement(explicit_page_break);
					reflow_state->reflow_position = LayoutCoord(info.doc->GetRelativePageTop());

					/* Reset margin collapsing, so that the next top margin is
					   retained at the top of the next page. */

					reflow_state->margin_state.Reset();

					if (packed.no_content)
					{
						packed.stop_top_margin_collapsing = TRUE;
						packed.no_content = FALSE;
					}

					switch (page_break)
					{
					case BREAK_ALWAYS:
						return TRUE;

					case BREAK_LEFT:
						if (next_page_description->GetNumber() % 2 == 1)
							break;

						return TRUE;

					case BREAK_RIGHT:
						if (next_page_description->GetNumber() % 2 == 0)
							break;

						return TRUE;
					}
				}
				else
					return FALSE;
			}
			else
				return FALSE;
		}

		break;

	default:
		// Shouldn't happen

		OP_ASSERT(0);
		return TRUE;
	}
}

ExplicitPageBreak::ExplicitPageBreak(VerticalLayout* vertical_layout)
  : break_after(NULL),
    y(0)
{
	if (vertical_layout)
		if (vertical_layout->IsPageBreak())
		{
			ExplicitPageBreak* page_break = (ExplicitPageBreak*) vertical_layout;

			break_after = page_break->break_after;
			y = page_break->y;
		}
		else
		{
			y = vertical_layout->GetStackPosition();

			if (vertical_layout->IsBlock())
				break_after = ((BlockBox*) vertical_layout)->GetHtmlElement();
			else
				if (vertical_layout->IsLine())
					break_after = ((Line*) vertical_layout)->GetStartElement();
				else
					if (vertical_layout->IsLayoutBreak())
						break_after = ((LayoutBreak*) vertical_layout)->GetHtmlElement();
		}
}

/** Should page break come into effect after layout element? */

BOOL
ExplicitPageBreak::Trails(VerticalLayout* vertical_layout) const
{
	if (!vertical_layout)
		return !break_after;
	else
		if (vertical_layout->IsBlock())
			return break_after == ((BlockBox*) vertical_layout)->GetHtmlElement();
		else
			if (vertical_layout->IsLine())
				return y == ((Line*) vertical_layout)->GetY() && break_after == ((Line*) vertical_layout)->GetStartElement();
			else
				if (vertical_layout->IsLayoutBreak())
					return break_after == ((LayoutBreak*) vertical_layout)->GetHtmlElement();
				else
					if (vertical_layout->IsPageBreak())
						return FALSE;

	OP_ASSERT(0);

	return FALSE;
}

/** Break page. */

BOOL
Container::GetPendingPageBreak(LayoutInfo& info, VerticalLayout* available_break)
{
	HTML_Element* html_element = GetHtmlElement();

	OP_ASSERT(available_break);

	PageDescription* next_page_description = info.doc->AdvancePage(available_break->GetStackPosition());

	if (next_page_description)
	{
		PendingPageBreak* pending_page_break = OP_NEW(PendingPageBreak, (available_break->Pred()));

		if (!pending_page_break)
			return FALSE;

		pending_page_break->Precede(available_break);
		info.workplace->SetPageBreakElement(info, html_element);
	}

	return TRUE;
}

/** Break page. */

BOOL
Container::AddPageBreaks(LayoutInfo& info, LayoutCoord& position)
{
	while (position > info.doc->GetRelativePageBottom())
	{
		PageDescription* next_page_description = info.doc->AdvancePage(position);

		if (next_page_description)
		{
			ImplicitPageBreak* implicit_page_break = OP_NEW(ImplicitPageBreak, (reflow_state->reflow_line));

			if (!implicit_page_break)
				return FALSE;

			AddElement(implicit_page_break);
		}

		if (position < info.doc->GetRelativePageTop())
			position = LayoutCoord(info.doc->GetRelativePageTop());
	}

	return TRUE;
}

/** Attempt to break page. */

/* virtual */ BREAKST
Container::AttemptPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain)
{
	if (strength >= 1 || !packed.avoid_page_break_inside)
	{
		VerticalLayout* potential_break_before = NULL;
		BOOL line_can_break = TRUE;
		VerticalLayout* vertical_layout = (VerticalLayout*) layout_stack.Last();
		long page_top = info.doc->GetRelativePageTop();
		long page_bottom = info.doc->GetRelativePageBottom();

		if (vertical_layout && !vertical_layout->IsInStack())
			vertical_layout = vertical_layout->GetPreviousElement();

		for (; vertical_layout; vertical_layout = vertical_layout->GetPreviousElement())
		{
			LayoutCoord element_height = vertical_layout->GetLayoutHeight();
			LayoutCoord element_bottom = vertical_layout->GetStackPosition() + element_height;

			if (element_bottom <= page_top)
				/* Element is on previous page; this means we didn't find
				   any points to break page at, and need to increase
				   `strength'. */

				break;
			else
			{
				OP_ASSERT(!vertical_layout->IsPageBreak());

				if (vertical_layout->GetStackPosition() <= page_bottom)
				{
					if (vertical_layout->IsLine() || vertical_layout->IsLayoutBreak())
					{
						if (element_bottom <= page_bottom)
							if (potential_break_before && line_can_break)
								// Break the page after us

								if (!GetPendingPageBreak(info, potential_break_before))
									return BREAK_OUT_OF_MEMORY;
								else
									return BREAK_FOUND;

						if (element_height > page_bottom - page_top)
							/* Element is bigger than page size and cannot be moved;
							   this means we didn't find any points to break page
							   at, and need to increase `strength'. */

							break;

						potential_break_before = vertical_layout;
						line_can_break = strength >= 2 || vertical_layout->GetPageBreakPolicyBefore() != BREAK_AVOID;

						if (constrain == SOFT && !line_can_break)
							/* We may be able to satisfy the orphans and widows
							   condition later, so don't break page now. */

							return BREAK_NOT_FOUND;
					}
					else
					{
						if (constrain != ONLY_PREDECESSORS)
						{
							// Don't attempt to break inside the current layout element

							if (potential_break_before && vertical_layout->GetPageBreakPolicyAfter() != BREAK_AVOID)
								// Yes, break the page after us

								if (!GetPendingPageBreak(info, potential_break_before))
									return BREAK_OUT_OF_MEMORY;
								else
									return BREAK_FOUND;

							BREAKST status = vertical_layout->AttemptPageBreak(info, strength);

							if (status != BREAK_KEEP_LOOKING)
								return status;
						}

						if (vertical_layout->GetPageBreakPolicyBefore() != BREAK_AVOID)
							// Try to break page before us

							potential_break_before = vertical_layout;
						else
							potential_break_before = NULL;

						line_can_break = TRUE;
					}
				}
				else
				{
					OP_ASSERT(!potential_break_before);
				}

				if (constrain == ONLY_PREDECESSORS)
					constrain = HARD;
			}
		}

		if (strength == 3 && potential_break_before && line_can_break)
		{
			BOOL pred_is_page_break =
				potential_break_before->Pred() &&
				potential_break_before->Pred()->IsPageBreak();

			BOOL has_already_inserted_implicit_break = pred_is_page_break &&
				static_cast<ExplicitPageBreak *>(potential_break_before->Pred())->IsImplicitPageBreak();

			if (!has_already_inserted_implicit_break)
				if (!GetPendingPageBreak(info, potential_break_before))
					return BREAK_OUT_OF_MEMORY;
				else
					return BREAK_FOUND;
		}

		if (vertical_layout)
			return BREAK_NOT_FOUND;
	}

	return BREAK_KEEP_LOOKING;
}

#endif // PAGED_MEDIA_SUPPORT

/** Propagate a break point caused by break properties or a spanned element. */

/* virtual */ BOOL
Container::PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint)
{
	return placeholder->PropagateBreakpoint(virtual_y, break_type, breakpoint);
}

/** Insert page or column break. */

BOOL
Container::InsertPageOrColumnBreak(LayoutInfo& info, BREAK_POLICY page_break, BREAK_POLICY column_break, Container* multipane_container, SpaceManager* space_manager)
{
	OP_ASSERT(BreakForced(page_break) || BreakForced(column_break));

	if (multipane_container)
	{
		/* First wave good-bye to previous floats. Move past them. They cannot
		   join us in the next column, as they need to belong to one and only
		   one column. */

		LayoutCoord clear_bfc_y = space_manager->FindBfcBottom(CSS_VALUE_both);
		LayoutCoord bfc_min_y(0);
		LayoutCoord bfc_y(0);
		LayoutCoord dummy_bfc_x(0);

		GetBfcOffsets(dummy_bfc_x, bfc_y, bfc_min_y);

		if (clear_bfc_y != LAYOUT_COORD_MIN)
		{
			LayoutCoord clear_y = clear_bfc_y - bfc_y;

			if (reflow_state->reflow_position < clear_y)
				reflow_state->reflow_position = clear_y;
		}

		if (multipane_container->GetMultiColumnContainer())
		{
			// Then report the break's position to the multi-pane container

			BREAK_TYPE break_type =
#ifdef PAGED_MEDIA_SUPPORT
				BreakForced(page_break) ? PAGE_BREAK :
#endif // PAGED_MEDIA_SUPPORT
				COLUMN_BREAK;

			if (!PropagateBreakpoint(reflow_state->reflow_position + reflow_state->margin_state.GetHeight(), break_type, NULL))
				return FALSE;
		}
	}
#ifdef PAGED_MEDIA_SUPPORT
	else
		if (info.paged_media == PAGEBREAK_ON)
			if (BreakForced(page_break))
				if (!GetExplicitPageBreak(info, page_break))
					return FALSE;
#endif // PAGED_MEDIA_SUPPORT

	return TRUE;
}

/** Propagate a break-after property from some descendant. */

void
Container::PropagateBreakPolicies(BREAK_POLICY page_break, BREAK_POLICY column_break)
{
	reflow_state->page_break_policy = CombineBreakPolicies(reflow_state->page_break_policy, page_break);
	reflow_state->column_break_policy = CombineBreakPolicies(reflow_state->column_break_policy, column_break);
}

/** Insert line break into layout stack. */

LAYST
Container::GetNewBreak(LayoutInfo& info, LayoutProperties* break_cascade)
{
	LAYST status = CommitLineContent(info, TRUE);
	VerticalLayout* vertical_layout = reflow_state->reflow_line;
	LayoutProperties* cascade = placeholder->GetCascade();
	SpaceManager* space_manager = break_cascade->space_manager;
	HTML_Element* element = break_cascade->html_element;
	const HTMLayoutProperties& break_props = *break_cascade->GetProps();
	CSSValue clear = (CSSValue) break_props.clear;
	LayoutCoord line_height = break_props.GetCalculatedLineHeight(info.visual_device);

	ClearFlexibleMarkers();

	if (status == LAYOUT_CONTINUE)
	{
		Line* current_line = GetReflowLine();

		if (!current_line && reflow_state->is_css_first_line)
			status = CreateEmptyFirstLine(info, element);

		if (status == LAYOUT_CONTINUE)
			status = CloseVerticalLayout(info, BREAK);

		if (status == LAYOUT_END_FIRST_LINE)
			reflow_state->break_before_content = element;
	}

	if (status != LAYOUT_CONTINUE)
		return status;
	else
	{
		BOOL has_non_auto_breaking_policies = FALSE;
		BOOL broke_here = FALSE;
		BREAK_POLICY page_break_before = BREAK_ALLOW;
		BREAK_POLICY column_break_before = BREAK_ALLOW;
		BREAK_POLICY page_break_after = BREAK_ALLOW;
		BREAK_POLICY column_break_after = BREAK_ALLOW;

		if (
#ifdef PAGED_MEDIA_SUPPORT
			info.paged_media == PAGEBREAK_ON ||
#endif // PAGED_MEDIA_SUPPORT
			break_cascade->multipane_container)
		{
			CssValuesToBreakPolicies(info, break_cascade, page_break_before, page_break_after, column_break_before, column_break_after);

			if (page_break_before != BREAK_ALLOW || column_break_before != BREAK_ALLOW ||
				page_break_after != BREAK_ALLOW || column_break_after != BREAK_ALLOW)
				/* Any non-auto (BREAK_ALWAYS or BREAK_AVOID, for example)
				   breaking policies require us to create a LayoutBreak object,
				   so that the policies are available for later. Both the
				   column and page breaking machineries require this. */

				has_non_auto_breaking_policies = TRUE;

			BREAK_POLICY page_break_here = CombineBreakPolicies(page_break_before, reflow_state->page_break_policy);
			BREAK_POLICY column_break_here = CombineBreakPolicies(column_break_before, reflow_state->column_break_policy);

			if (BreakForced(page_break_here) || BreakForced(column_break_here))
			{
				if (!InsertPageOrColumnBreak(info, page_break_here, column_break_here, break_cascade->multipane_container, space_manager))
					return LAYOUT_OUT_OF_MEMORY;

				broke_here = TRUE;
			}

			reflow_state->page_break_policy = page_break_after;
			reflow_state->column_break_policy = column_break_after;
		}

		LayoutCoord new_y = reflow_state->reflow_position + reflow_state->margin_state.GetHeight();
		LayoutCoord new_min_y(0);
		BOOL has_clearance = FALSE;
		BOOL has_min_clearance = FALSE;
		BOOL need_break = FALSE;

		if (reflow_state->calculate_min_max_widths)
			new_min_y = reflow_state->min_reflow_position + reflow_state->margin_state.GetHeightNonPercent();

		if ((!vertical_layout ||
			 !vertical_layout->IsLine() ||
			 !vertical_layout->GetLayoutHeight() ||
			 reflow_state->forced_line_end) &&
			!info.doc->GetHandheldEnabled())
		{
			/* If we can't force an end to the previous line, place a
			   layout break here */

			if (reflow_state->calculate_min_max_widths)
				new_min_y += line_height;

			need_break = TRUE;
		}

		if (clear != CSS_VALUE_none)
		{
			LayoutCoord break_bfc_y = space_manager->FindBfcBottom(clear);
			LayoutCoord bfc_min_y(0);
			LayoutCoord bfc_y(0);
			LayoutCoord dummy_bfc_x(0);

			GetBfcOffsets(dummy_bfc_x, bfc_y, bfc_min_y);

			if (break_bfc_y != LAYOUT_COORD_MIN)
			{
				LayoutCoord break_y = break_bfc_y - bfc_y;

				if (need_break)
				{
					/* We need a line break with the height of the line at 'new_y' but the bottom of the
					   layout break must not be lower than the bottom of floats. */

					break_y -= line_height;
				}

				if (new_y < break_y)
				{
					// clear forces box to be at least at this position

					has_clearance = TRUE;

#ifdef PAGED_MEDIA_SUPPORT
					if (info.paged_media == PAGEBREAK_ON)
						if (!AddPageBreaks(info, break_y))
							return LAYOUT_OUT_OF_MEMORY;
#endif // PAGED_MEDIA_SUPPORT

					new_y = break_y;

					if (!need_break)
						/* Remove line height when there is a line present so that the bottom of the break
						   ends up aligned with the line. */

						new_y -= line_height;
				}
			}

			if (reflow_state->calculate_min_max_widths)
			{
				LayoutCoord clear_bfc_min_y = space_manager->FindBfcMinBottom((CSSValue) clear);

				if (clear_bfc_min_y != LAYOUT_COORD_MIN)
				{
					LayoutCoord clear_min_y = clear_bfc_min_y - bfc_min_y;

					if (new_min_y < clear_min_y)
					{
						has_min_clearance = TRUE;
						new_min_y = clear_min_y;
						reflow_state->margin_state.max_negative_nonpercent = LayoutCoord(0);
						reflow_state->margin_state.max_positive_nonpercent = LayoutCoord(0);
						reflow_state->margin_state.max_default_nonpercent = LayoutCoord(0);
					}
				}
			}
		}

		reflow_state->forced_line_end = TRUE;

		if (has_clearance || need_break || has_non_auto_breaking_policies)
		{
			const HTMLayoutProperties& props = *cascade->GetProps();
			LayoutCoord x = props.padding_left + LayoutCoord(props.border.left.width);
			LayoutCoord width = GetContentWidth();

			if (has_non_auto_breaking_policies)
				if (!has_clearance && !need_break && !broke_here)
					/* We didn't break the page or column here, and this line
					   break terminated a line, so it should not add any height
					   to the container. */

					new_y -= line_height;

			GetSpace(space_manager, new_y, x, width, LayoutCoord(0), LayoutCoord(0));

			LayoutBreak* layout_break = OP_NEW(LayoutBreak, (element, new_y, x, width, line_height));

			if (need_break)
				reflow_state->was_lpw_ignored = FALSE;

			if (!layout_break)
				return LAYOUT_OUT_OF_MEMORY;
			else
			{
				if (has_non_auto_breaking_policies)
				{
					layout_break->SetBreakPolicies(page_break_before, column_break_before, page_break_after, column_break_after);

					if (!need_break && !has_clearance)
						layout_break->SetIsLineTerminator();
				}

				AddElement(layout_break);

				if (has_clearance)
					if (
#ifdef PAGED_MEDIA_SUPPORT
						info.paged_media != PAGEBREAK_OFF ||
#endif // PAGED_MEDIA_SUPPORT
						cascade->multipane_container || IsMultiPaneContainer())
						layout_break->SetAllowBreakBefore();

				AbsoluteBoundingBox bounding_box;

				bounding_box.Set(LayoutCoord(0), new_y, LayoutCoord(0), line_height);
				bounding_box.SetContentSize(LayoutCoord(0), line_height);

				UpdateBottomMargins(info, 0, &bounding_box);
			}
		}

		if (reflow_state->calculate_min_max_widths)
			if (has_min_clearance || need_break)
				if (reflow_state->min_reflow_position < new_min_y)
					reflow_state->min_reflow_position = new_min_y;

		return LAYOUT_CONTINUE;
	}
}

/** Recalculate top margins after new block box has been added. See declaration in header file for more information. */

BOOL
Container::RecalculateTopMargins(LayoutInfo& info, const VerticalMargin* top_margin, BOOL has_bottom_margin)
{
	OP_ASSERT(reflow_state->reflow_line && reflow_state->reflow_line->IsBlock());

	BlockBox* box = (BlockBox*) reflow_state->reflow_line;

	if (!packed.stop_top_margin_collapsing && packed.no_content)
	{
		// No non-empty in-flow content in this container (at least not yet). Collapse margin with child.

		if (placeholder->RecalculateTopMargins(info, top_margin))
		{
			VerticalLayout* prev = box->GetPreviousElement(TRUE);
			while (prev)
			{
				if (prev->IsLine())
					break;

				OP_ASSERT(prev->IsBlock());

				static_cast<BlockBox*>(prev)->Invalidate(placeholder->GetCascade(), info);
				prev = prev->GetPreviousElement(TRUE);
			}

			return TRUE;
		}

		return FALSE;
	}
	else
	{
		LayoutCoord new_offset(0);
		LayoutCoord new_offset_nonpercent(0);
		VerticalMargin new_top_margin;

		if (top_margin)
			new_top_margin = *top_margin;

		switch (packed.no_content)
		{
		case TRUE:
			if (placeholder->GetLocalSpaceManager())
				break;
			else
			{
				Markup::Type type = GetHtmlElement()->Type();

				if (type == Markup::HTE_BODY || type == Markup::HTE_HTML)
					break;
			}

		default:
			new_top_margin.ApplyDefaultMargin();
			break;
		}

		// Grow margins if child has bigger margins

		reflow_state->margin_state.Collapse(new_top_margin, &new_offset, &new_offset_nonpercent);

		if (!has_bottom_margin)
		{
			if (new_offset || new_offset_nonpercent)
			{
				HTML_Element* child_elm = box->GetHtmlElement();
				SpaceManager* space_manager;
				StackingContext* stacking_context;
				LayoutProperties* cascade = placeholder->GetCascade();
				ReflowState* box_state = box->GetReflowState();

				if (box_state)
				{
					space_manager = box_state->cascade->space_manager;
					stacking_context = box_state->cascade->stacking_context;

					info.Translate(LayoutCoord(0), new_offset);

					BOOL root_translation_moved = FALSE;

					for (LayoutProperties* desc_cascade = cascade->Suc(); desc_cascade && desc_cascade->html_element; desc_cascade = desc_cascade->Suc())
						if (Box* desc_box = desc_cascade->html_element->GetLayoutBox())
							if (desc_box->UpdateRootTranslationState(root_translation_moved, LayoutCoord(0), new_offset))
							{
								info.TranslateRoot(LayoutCoord(0), new_offset);
								root_translation_moved = TRUE;
							}
				}
				else
				{
					space_manager = placeholder->GetLocalSpaceManager();
					if (!space_manager)
						space_manager = cascade->space_manager;
					stacking_context = placeholder->GetLocalStackingContext();
					if (!stacking_context)
						stacking_context = cascade->stacking_context;
				}

				box->SetY(box->GetStackPosition() + new_offset);

				if (reflow_state->calculate_min_max_widths)
				{
					box->SetMinY(box->GetMinY() + new_offset_nonpercent);
					reflow_state->cur_elm_min_stack_position += new_offset_nonpercent;
				}

				// Update block formatting context offset for all open descendant containers.

				BOOL first_passed = FALSE;

				for (LayoutProperties* desc_cascade = cascade->Suc(); desc_cascade; desc_cascade = desc_cascade->Suc())
					if (desc_cascade->html_element && desc_cascade->html_element->GetLayoutBox())
						if (Container* descendant_container = desc_cascade->html_element->GetLayoutBox()->GetContainer())
							if (first_passed)
							{
								if (descendant_container->reflow_state)
								{
									descendant_container->reflow_state->parent_bfc_y = LAYOUT_COORD_MIN;
									descendant_container->reflow_state->parent_bfc_min_y = LAYOUT_COORD_MIN;
								}
							}
							else
								/* Skip the first descendant container, since the values
								   updated here are really about the parent container's
								   position in the block formatting context. */

								first_passed = TRUE;

				// Need to propagate floats' margins again since they have moved

				space_manager->PropagateBottomMargins(info, child_elm, new_offset, new_offset_nonpercent);

				if (new_offset)
					stacking_context->TranslateChildren(child_elm, -new_offset, info);

				return TRUE;
			}
		}

		return FALSE;
	}
}

/** Get new block to reflow onto. */

LAYST
Container::GetNewBlockStage1(LayoutInfo& info, LayoutProperties* box_cascade, BlockBox* box)
{
	LayoutProperties* cascade = placeholder->GetCascade();
	LAYST status = CommitLineContent(info, TRUE);

	if (status == LAYOUT_CONTINUE)
	{
		/* Keep pending run-in. CloseVerticalLayout() will reset it, but in
		   this particular case, we may want to keep it. */

		Content_Box* run_in;

		if (box->GetContainer())
			// Only keep it if the box has a container to use the runin

			run_in = reflow_state->pending_run_in_box;
		else
			run_in = NULL;

		LayoutCoord virtual_position = LAYOUT_COORD_MIN;

		for (HTML_Element* elm = box_cascade->html_element->Parent(); elm && elm != cascade->html_element; elm = elm->Parent())
			if (elm->GetLayoutBox() && elm->GetLayoutBox()->IsInlineBox())
			{
				virtual_position = CalculateBlockVirtualPosition(box_cascade);
				break;
			}

		if (virtual_position != LAYOUT_COORD_MIN)
		{
			/* If this block terminates a line, or follows one that does,
			   specify that it's "on a line". For logical traversal we need
			   this, in order to detect blocks that are children of inlines. */

			box->SetOnLine(TRUE);
			box->SetVirtualPosition(virtual_position);
		}
		else
			box->SetOnLine(FALSE);

		status = CloseVerticalLayout(info, BLOCK);

		reflow_state->pending_run_in_box = run_in;

		if (status == LAYOUT_END_FIRST_LINE)
			reflow_state->break_before_content = box_cascade->html_element;
	}

	if (status != LAYOUT_CONTINUE)
		return status;
	else
	{
		const HTMLayoutProperties& box_props = *box_cascade->GetProps();
		const HTMLayoutProperties& props = *cascade->GetProps();

		if (
#ifdef PAGED_MEDIA_SUPPORT
			info.paged_media == PAGEBREAK_ON ||
#endif // PAGED_MEDIA_SUPPORT
			box_cascade->multipane_container)
		{
			BREAK_POLICY page_break_before;
			BREAK_POLICY column_break_before;
			BREAK_POLICY page_break_after;
			BREAK_POLICY column_break_after;

			CssValuesToBreakPolicies(info, box_cascade, page_break_before, page_break_after, column_break_before, column_break_after);

			/* Now look at the first and last children of the new block, and see if their
			   break policies should be propagated to the block itself. It would be much
			   better to do this during layout of the actual children (they may not even
			   have got their boxes created yet), but that's not going to work with the
			   way explicit page breaking is implemented currently. */

			box->GetContent()->CombineChildBreakProperties(page_break_before, column_break_before, page_break_after, column_break_after);

			BREAK_POLICY page_break_here = CombineBreakPolicies(page_break_before, reflow_state->page_break_policy);
			BREAK_POLICY column_break_here = CombineBreakPolicies(column_break_before, reflow_state->column_break_policy);

			if (BreakForced(page_break_here) || BreakForced(column_break_here))
				if (!InsertPageOrColumnBreak(info, page_break_here, column_break_here, box_cascade->multipane_container, box_cascade->space_manager))
					return LAYOUT_OUT_OF_MEMORY;

			reflow_state->page_break_policy = page_break_after;
			reflow_state->column_break_policy = column_break_after;

			box->SetBreakPolicies(page_break_before, column_break_before, page_break_after, column_break_after);
		}

		if (HasFlexibleMarkers() && !box->GetContainer())
			ClearFlexibleMarkers();

		reflow_state->is_run_in = FALSE;

		CSSValue clear_prop = (CSSValue) box_props.clear;

		if (box->IsColumnSpanned())
		{
			// Don't let floats from previous columns interact with a spanned box.

			clear_prop = CSS_VALUE_both;

			// Don't let the spanned box's top margin collapse with anything.

			CommitMargins();
		}

		if (clear_prop != CSS_VALUE_none)
		{
			LayoutCoord clear_bfc_y = box_cascade->space_manager->FindBfcBottom(clear_prop);
			LayoutCoord bfc_min_y(0);
			LayoutCoord bfc_y(0);
			LayoutCoord dummy_bfc_x(0);

			GetBfcOffsets(dummy_bfc_x, bfc_y, bfc_min_y);

			if (clear_bfc_y != LAYOUT_COORD_MIN)
			{
				LayoutCoord clear_y = clear_bfc_y - bfc_y;
				LayoutCoord hypothetical_position = reflow_state->reflow_position;

				if (!packed.no_content || packed.stop_top_margin_collapsing)
					hypothetical_position += CollapseHypotheticalMargins(box_props.margin_top);

				if (hypothetical_position < clear_y)
				{
					// clear forces box to be at least at this position

					packed.stop_top_margin_collapsing = TRUE;
					packed.no_content = FALSE;

					box->SetHasClearance();

					reflow_state->margin_state.max_negative = LayoutCoord(0);
					reflow_state->margin_state.max_positive = LayoutCoord(0);
					reflow_state->margin_state.max_default = LayoutCoord(0);

#ifdef PAGED_MEDIA_SUPPORT
					if (info.paged_media == PAGEBREAK_ON)
						if (!AddPageBreaks(info, clear_y))
							return LAYOUT_OUT_OF_MEMORY;
#endif // PAGED_MEDIA_SUPPORT

					reflow_state->reflow_position = clear_y - box_props.margin_top;
				}
			}

			if (reflow_state->calculate_min_max_widths)
			{
				LayoutCoord clear_bfc_min_y = box_cascade->space_manager->FindBfcMinBottom(clear_prop);

				if (clear_bfc_min_y != LAYOUT_COORD_MIN)
				{
					LayoutCoord clear_min_y = clear_bfc_min_y - bfc_min_y;

					if (reflow_state->min_reflow_position < clear_min_y)
					{
						reflow_state->margin_state.max_negative_nonpercent = LayoutCoord(0);
						reflow_state->margin_state.max_positive_nonpercent = LayoutCoord(0);
						reflow_state->margin_state.max_default_nonpercent = LayoutCoord(0);

						LayoutCoord add = clear_min_y - reflow_state->min_reflow_position;

						if (!box_props.GetMarginTopIsPercent())
							add -= box_props.margin_top;

						reflow_state->min_reflow_position += add;
					}
				}
			}
		}

		AddElement(box);

		box->SetX(props.padding_left + LayoutCoord(props.border.left.width));
		box->SetY(reflow_state->reflow_position + reflow_state->margin_state.GetHeight());

		if (reflow_state->calculate_min_max_widths)
		{
			reflow_state->cur_elm_min_stack_position = reflow_state->min_reflow_position + reflow_state->margin_state.GetHeightNonPercent();
			box->SetMinY(reflow_state->cur_elm_min_stack_position);
		}

		return LAYOUT_CONTINUE;
	}
}

/** Get new block to reflow onto. */

BOOL
Container::GetNewBlockStage2(LayoutInfo& info, LayoutProperties* box_cascade, BlockBox* box)
{
	const HTMLayoutProperties& box_props = *box_cascade->GetProps();
	LayoutCoord available_width = box->IsColumnSpanned() ? ((MultiColumnContainer*) box_cascade->multipane_container)->GetOuterContentWidth() : GetContentWidth();
	Container* box_container = box->GetContainer();
	LayoutCoord offset_x(0);
	LayoutCoord pending_left_margin(0);
#ifdef SUPPORT_TEXT_DIRECTION
	LayoutCoord pending_right_margin(0);
#endif
	const HTMLayoutProperties* props = placeholder->GetCascade()->GetProps();

	reflow_state->avoid_float_overlap = !box_container || box->GetLocalSpaceManager();

	BOOL avoid_border_box_overlap = reflow_state->avoid_float_overlap ||
									box_props.content_width >= 0 && !info.hld_profile->IsInStrictMode();

#ifdef SUPPORT_TEXT_DIRECTION
	if (props->direction == CSS_VALUE_rtl)
		pending_right_margin = box_props.margin_right;
	else
#endif
		pending_left_margin = box_props.margin_left;

 	if (avoid_border_box_overlap)
	{
		/* The border-box of tables, replaced block boxes and regular block boxes that establish a
		   new block formatting context may not be overlapped by previous floats. In quirks mode,
		   this also applies to regular block boxes as long as they have non-auto and
		   non-percentage width. */

		const LayoutCoord old_y = box->GetStackPosition();
		LayoutCoord new_y = old_y;
		const LayoutCoord old_x = box->GetX();
		LayoutCoord new_x = old_x + pending_left_margin;

		available_width -= pending_left_margin;
		pending_left_margin = LayoutCoord(0);

		// Allocate space from the space manager

		GetSpace(box_cascade->space_manager, new_y, new_x, available_width, box->GetWidth(), LayoutCoord(0));

		offset_x = new_x - old_x;

		if (new_y != old_y)
		{
			// Pushed down by floats; re-apply top margin.

			new_y += box_props.margin_top;

			if (new_y > old_y)
			{
				// Nuke margins

				reflow_state->margin_state.Reset();

				if (box_props.margin_top)
					reflow_state->margin_state.CollapseWithTopMargin(box_props);

				// floats forces box to be at least at this position

				if (packed.no_content)
				{
					packed.stop_top_margin_collapsing = TRUE;
					packed.no_content = FALSE;
				}

				info.Translate(LayoutCoord(0), new_y - old_y);
				box->SetY(new_y);

#ifdef PAGED_MEDIA_SUPPORT
				if (info.paged_media == PAGEBREAK_ON)
					if (!AddPageBreaks(info, new_y))
						return FALSE;
#endif // PAGED_MEDIA_SUPPORT
			}
		}

		if (box->IsColumnSpanned())
		{
			// Ignore horizontal offsets caused by descendants of the multicol container.

			Container* ancestor = this;
			MultiColumnContainer* multicol = (MultiColumnContainer*) box_cascade->multipane_container;

			OP_ASSERT(multicol->GetMultiColumnContainer());

			while (ancestor != multicol)
			{
				Content_Box* ancestor_box = ancestor->GetPlaceholder();
				LayoutProperties* ancestor_cascade = ancestor_box->GetCascade();
				const HTMLayoutProperties& ancestor_props = *ancestor_cascade->GetProps();

				OP_ASSERT(ancestor_box->IsBlockBox());
				OP_ASSERT(!ancestor_box->GetLocalSpaceManager());

				offset_x -= ancestor_props.margin_left + LayoutCoord(ancestor_props.border.left.width) + ancestor_props.padding_left;

				ancestor = ancestor_cascade->container;
			}

#ifdef SUPPORT_TEXT_DIRECTION
			const HTMLayoutProperties& multicol_props = *multicol->GetPlaceholder()->GetCascade()->GetProps();

			if (multicol_props.direction == CSS_VALUE_rtl)
				// Subtract distance from first (rightmost) to last (leftmost) column.

				offset_x -= multicol->GetOuterContentWidth() - multicol->GetContentWidth();
#endif // SUPPORT_TEXT_DIRECTION
		}
	}

#ifdef SUPPORT_TEXT_DIRECTION
	if (box_props.GetMarginRightAuto() && props->direction == CSS_VALUE_rtl ||
		box_props.GetMarginLeftAuto() && props->direction == CSS_VALUE_ltr)
#else
	if (box_props.GetMarginLeftAuto())
#endif
	{
		/* Need to update left edge if left margin is auto.

		   (in case of html center or right align on block
		   elements, margins can have a faked 'auto' value and
		   numeric value at the same time) */

		LayoutCoord margin_width = available_width - box->GetWidth();

#ifdef SUPPORT_TEXT_DIRECTION
		if (props->direction == CSS_VALUE_rtl)
			margin_width -= box_props.margin_left + pending_right_margin;
		else
#endif
			margin_width -= pending_left_margin + box_props.margin_right;

		if (margin_width > 0)
		{
			// Left and right margin are equal if both are auto

#ifdef SUPPORT_TEXT_DIRECTION
			if ((props->direction == CSS_VALUE_rtl && box_props.GetMarginLeftAuto()) ||
				(props->direction == CSS_VALUE_ltr && box_props.GetMarginRightAuto()))
#else
			if (box_props.GetMarginRightAuto())
#endif
				margin_width /= 2;

#ifdef SUPPORT_TEXT_DIRECTION
			if (props->direction == CSS_VALUE_rtl)
				pending_right_margin += margin_width;
			else
#endif
				pending_left_margin += margin_width;
		}
	}

#ifdef SUPPORT_TEXT_DIRECTION
	if (props->direction == CSS_VALUE_rtl)
		offset_x += available_width - box->GetWidth() - pending_right_margin;
	else
#endif
		offset_x += pending_left_margin;

	if (reflow_state->pending_run_in_box)
	{
		if (reflow_state->pending_run_in_box->IsBlockCompactBox())
		{
			Container* run_in_content = reflow_state->pending_run_in_box->GetContainer();

			if (run_in_content)
#ifdef SUPPORT_TEXT_DIRECTION
				if (props->direction == CSS_VALUE_rtl)
				{
					if (!run_in_content->IsNarrowerThan(pending_right_margin - reflow_state->pending_compact_space))
						reflow_state->pending_run_in_box = NULL;
				}
				else
#endif
					if (!run_in_content->IsNarrowerThan(pending_left_margin - reflow_state->pending_compact_space))
						reflow_state->pending_run_in_box = NULL;
		}

#ifdef SUPPORT_TEXT_DIRECTION
		if (props->direction == CSS_VALUE_rtl)
			reflow_state->pending_compact_space = pending_right_margin;
		else
#endif
			reflow_state->pending_compact_space = pending_left_margin;
	}

	info.Translate(offset_x, LayoutCoord(0));
	box->SetX(offset_x + box->GetX());

	if (!box_props.GetMarginLeftAuto() && !box_props.GetMarginLeftIsPercent() && !props->GetPaddingLeftIsPercent())
		box->SetHasFixedLeft();

	return TRUE;
}

/** Set bounding box values. */

void
Line::SetBoundingBox(LayoutCoord line_box_left, LayoutCoord line_box_right, LayoutCoord line_box_above_baseline, LayoutCoord line_box_below_baseline)
{
	LayoutCoord above = line_box_above_baseline - baseline;
	LayoutCoord below = line_box_below_baseline - height + baseline;

	packed.bounding_box_left = line_box_left < 0;
	packed.bounding_box_right = line_box_right > width;

	if (above <= 0)
		packed.bounding_box_top = 0;
	else
		if (above < BOUNDING_BOX_LINE_TOP_BOTTOM_MAX)
			packed.bounding_box_top = above;
		else
			packed.bounding_box_top = BOUNDING_BOX_LINE_TOP_BOTTOM_MAX;

	if (below <= 0)
		packed.bounding_box_bottom = 0;
	else
		if (below < BOUNDING_BOX_LINE_TOP_BOTTOM_MAX)
			packed.bounding_box_bottom = below;
		else
			packed.bounding_box_bottom = BOUNDING_BOX_LINE_TOP_BOTTOM_MAX;
}

/** Get bounding box values. */

void
Line::GetBoundingBox(AbsoluteBoundingBox& bounding_box) const
{
	LayoutCoord bbox_x(0);
	LayoutCoord bbox_width(0);
	LayoutCoord bbox_y(0);
	LayoutCoord bbox_height(0);

	if (packed.bounding_box_left)
	{
		bbox_width = LAYOUT_COORD_MAX;

		if (packed.bounding_box_right)
			bbox_x = LAYOUT_COORD_HALF_MIN;
		else
			bbox_x = width - LAYOUT_COORD_MAX;
	}
	else
		if (packed.bounding_box_right)
			bbox_width = LAYOUT_COORD_MAX;
		else
			bbox_width = width;

	if (packed.bounding_box_top == BOUNDING_BOX_LINE_TOP_BOTTOM_MAX)
	{
		bbox_height = LAYOUT_COORD_MAX;

		if (packed.bounding_box_bottom == BOUNDING_BOX_LINE_TOP_BOTTOM_MAX)
			bbox_y = LAYOUT_COORD_HALF_MIN;
		else
			bbox_y = LayoutCoord(packed.bounding_box_bottom) + height - LAYOUT_COORD_MAX;
	}
	else
	{
		bbox_y = -LayoutCoord(packed.bounding_box_top);

		if (packed.bounding_box_bottom == BOUNDING_BOX_LINE_TOP_BOTTOM_MAX)
			bbox_height = LAYOUT_COORD_MAX;
		else
			bbox_height = height + LayoutCoord(packed.bounding_box_bottom + packed.bounding_box_top);
	}

	/* Keep the sum of the line's and the bounding box's positions from
	   becoming less than LAYOUT_COORD_HALF_MIN. */

	if (bbox_x < 0)
	{
		if (x < LAYOUT_COORD_HALF_MIN)
		{
			bbox_width += bbox_x;
			bbox_x = LayoutCoord(0);
		}
		else
			if (bbox_x + x < LAYOUT_COORD_HALF_MIN)
			{
				bbox_width -= LAYOUT_COORD_HALF_MIN - (bbox_x + x);
				bbox_x = LAYOUT_COORD_HALF_MIN - x;
			}
	}

	if (bbox_y < 0)
	{
		if (y < LAYOUT_COORD_HALF_MIN)
		{
			bbox_height += bbox_y;
			bbox_y = LayoutCoord(0);
		}
		else
			if (bbox_y + y < LAYOUT_COORD_HALF_MIN)
			{
				bbox_height -= LAYOUT_COORD_HALF_MIN - (bbox_y + y);
				bbox_y = LAYOUT_COORD_HALF_MIN - y;
			}
	}

	bounding_box.Set(bbox_x, bbox_y, bbox_width, bbox_height);
}

/* virtual */
void Line::MoveDown(LayoutCoord offset_y, HTML_Element* containing_element)
{
	SetPosition(x, y + offset_y);

	OP_ASSERT(containing_element);

	BOOL foo = FALSE;
	Line* next_line = GetNextLine(foo);

	HTML_Element* stop_element = next_line ? next_line->GetStartElement() : NULL;

	if (!stop_element)
		stop_element = containing_element->NextSibling();

	HTML_Element* child = start_element;

	while (child && child != stop_element)
	{
		Box* box = child->GetLayoutBox();

		AbsolutePositionedBox* ab = box && box->IsAbsolutePositionedBox() ? (AbsolutePositionedBox*)box : NULL;

		if (ab && ab->TraverseInLine())
			ab->MoveDown(offset_y, containing_element);

		if (!box || box->GetContainer())
		{
			/* Absolute positioned boxes with "static" top position are relative to their containers position.
			   The container or one of its parents will be moved along with all vertical layouts in the general loop.
			   Only elements that are have position relative to the <button> itself needs to be moved. */

			child = child->NextSibling();
		}
		else
			child = child->Next();
	 }
}

LayoutCoord
Line::GetTextAlignOffset(const HTMLayoutProperties& props
#ifdef SUPPORT_TEXT_DIRECTION
						 , BOOL rtl_justify_offset /*= FALSE*/
					     , BOOL directional /*= FALSE*/
#endif
						 ) const
{
	LayoutCoord x_offset(0);

#ifdef SUPPORT_TEXT_DIRECTION
	if (props.direction == CSS_VALUE_rtl)
	{
		x_offset = GetWidth() - GetUsedSpace();
		/* In RTL flow - when there is less content on the line than
		   available space (x_offset > 0):
		   1. left aligned content has 0 offset from the line's left edge.
		   2. right aligned content has 0 offset from the line's right edge. */
		if (x_offset > 0 && (!directional && props.text_align == CSS_VALUE_left ||
							 directional && props.text_align == CSS_VALUE_right ||
							 rtl_justify_offset)
			/* In rtl text, the content should never start to the
			   right of the right edge of the line. */
			|| directional && x_offset < 0)
		{
			x_offset = LayoutCoord(0);
		}
		else if (props.text_align == CSS_VALUE_center)
		{
			/* Center text. */
			if (x_offset > 0)
				x_offset /= LayoutCoord(2);
		}

		/* The offset calculated above is calculated relative to the beginning
		   of the line, regardless of direction. Negate the offset for right
		   edge offsets for correct horizontal offset. */
		if (directional)
			x_offset = -x_offset;
	}
	else
#endif // SUPPORT_TEXT_DIRECTION
	{
		/* direction == CSS_VALUE_ltr. */
		if (props.text_align == CSS_VALUE_right || props.text_align == CSS_VALUE_center)
		{
			x_offset = GetWidth() - GetUsedSpace();
			if (x_offset < 0)
				x_offset = LayoutCoord(0);
			else if (props.text_align == CSS_VALUE_center)
				/* Center text. */
				x_offset /= LayoutCoord(2);
		}
	}
	return x_offset;
}

#ifdef SUPPORT_TEXT_DIRECTION

/**
 * TODO
 *
 * inline images with unicode-bidi = normal, should be treated with regards to the direction property.
 *      need IsDirectionSet property
 * Overflow of text should happen to the left, not to the right
 *
 * Remaining rules for weak types
 * L1 Tabs and whitespaces preceding tabs.
 *
 * pack the ParagraphBidiSegment, and see of we can do without virtual_position
 *
 * align = justify does not work
 *
 * FIXME parts that are ugly and could cause errors
 *       * The thing with last_segment_type used for handling of BIDI_EN and BIDI_AN
 *
 */


/** Push new inline bidi properties onto stack. */

BOOL
Container::PushBidiProperties(LayoutInfo& info, BOOL ltr, BOOL override)
{
	if (!reflow_state->bidi_calculation)
		if (!ReconstructBidiStatus(info))
			return FALSE;

	if (reflow_state->bidi_calculation->PushLevel(ltr ? CSS_VALUE_ltr : CSS_VALUE_rtl, override) == OpStatus::ERR_NO_MEMORY)
		return FALSE;

	return TRUE;
}

/** Pop inline bidi properties from stack. */

BOOL
Container::PopBidiProperties()
{
	if (reflow_state->bidi_calculation)
		if (reflow_state->bidi_calculation->PopLevel() == OpStatus::ERR_NO_MEMORY)
			return FALSE;

	return TRUE;
}

#endif // SUPPORT_TEXT_DIRECTION

LayoutCoord
Container::GetReflowPosition(BOOL inline_position) const
{
	VerticalLayout* vertical_layout = reflow_state->reflow_line;

	if (vertical_layout && vertical_layout->IsInStack())
	{
		LayoutCoord pos = vertical_layout->GetStackPosition();

		if (vertical_layout->IsLine())
		{
			Line* line = (Line*) vertical_layout;

			// The line with only outside marker shouldn't advance reflow position.
			if (!line->IsOutsideListMarkerLine() || !(reflow_state->list_item_marker_state & FLEXIBLE_MARKER_NOT_ALIGNED))
				if (!inline_position || reflow_state->GetWordWidth() + reflow_state->GetNoncontentWidth() > line->GetSpaceLeft())
					pos += MAX(line->GetLayoutHeight(), reflow_state->line_state.uncommitted.height_above_baseline + reflow_state->line_state.uncommitted.height_below_baseline);
		}
		else
			pos += vertical_layout->GetLayoutHeight();

		return pos;
	}
	else
		return reflow_state->reflow_position;
}

#ifdef PAGED_MEDIA_SUPPORT

/** Search for trailing page breaks and skip to their pages. */

BOOL
Container::SkipPageBreaks(LayoutInfo& info)
{
	VerticalLayout* element = reflow_state->reflow_line;
	VerticalLayout* scan = element;

	if (scan)
		scan = scan->Suc();
	else
		scan = (VerticalLayout*) layout_stack.First();

	while (scan && scan->IsPageBreak())
	{
		ExplicitPageBreak* page_break = (ExplicitPageBreak*) scan;
		VerticalLayout* next = page_break->Suc();

		if (info.KeepPageBreaks())
			if (!page_break->Trails(element))
				break;
			else
			{
				info.doc->RewindPage(reflow_state->reflow_position);
				info.doc->AdvancePage(reflow_state->reflow_position);

				reflow_state->reflow_position = LayoutCoord(info.doc->GetRelativePageTop());

				// This will nuke any margin across pages

				reflow_state->margin_state.Ignore();

				if (packed.no_content)
				{
					packed.stop_top_margin_collapsing = TRUE;
					packed.no_content = FALSE;
				}

				if (page_break->IsPendingPageBreak())
				{
					// We found last page break, let normal page breaking commence

					ImplicitPageBreak* implicit_page_break = OP_NEW(ImplicitPageBreak, (element));

					if (!implicit_page_break)
						return FALSE;

					implicit_page_break->Precede(page_break);
					reflow_state->reflow_line = implicit_page_break;

					OP_ASSERT(info.paged_media == PAGEBREAK_FIND);
					info.paged_media = PAGEBREAK_ON;
				}
				else
					reflow_state->reflow_line = page_break;
			}

		if (!info.KeepPageBreaks())
		{
			// Too late; this is no longer valid

			page_break->Out();
			OP_DELETE(page_break);
		}

		scan = next;
	}

	return TRUE;
}
#endif // PAGED_MEDIA_SUPPORT

/** Finish reflowing box. */

LAYST
Container::CloseVerticalLayout(LayoutInfo& info, CLOSED_BY closed_by)
{
	VerticalLayout* vertical_layout = reflow_state->reflow_line;
	BOOL in_stack = vertical_layout && vertical_layout->IsInStack();

	if (reflow_state->pending_run_in_box)
	{
		if (reflow_state->pending_run_in_box->IsInlineRunInBox())
			// The pending inline run-in found no block to run into. Convert back to block.

			((InlineRunInBox*) reflow_state->pending_run_in_box)->SetRunInBlockConversion(TRUE, info);

		reflow_state->pending_run_in_box = NULL;
	}

	Line* line = vertical_layout && vertical_layout->IsLine() ? (Line*) vertical_layout : 0;
	LayoutProperties* cascade = placeholder->GetCascade();
	AbsoluteBoundingBox bounding_box, abspos_content_box;
	BOOL need_bounding_box = FALSE;

	if (line && !reflow_state->forced_line_end)
	{
		if (!reflow_state->line_state.committed.IsEmpty())
		{
			/* Pass bounding box to line and let it set its own bounding box
			   Also deal with an outside list marker stuff. */

			BOOL is_outside_marker_line = line->IsOutsideListMarkerLine();
			BOOL is_only_outside_list_marker_line = is_outside_marker_line && (reflow_state->list_item_marker_state & FLEXIBLE_MARKER_NOT_ALIGNED);

			if (is_only_outside_list_marker_line)
				line->SetIsOnlyOutsideListMarkerLine();

			if (HasFlexibleMarkers())
			{
				if (is_only_outside_list_marker_line)
				{
					if (reflow_state->list_item_marker_state & ANCESTOR_HAS_FLEXIBLE_MARKER)
					{
						// Align vertically previous markers to the line, which contains only the outside marker.
						LayoutCoord baseline_in_container = line->GetY() + line->GetBaseline();
						AlignMarkers(baseline_in_container, MARKER_ALIGN_TO_SINGLE_OUTSIDE_FIRST_CALL);
						// Mark flexible markers as aligned with the one on the line.
						cascade->container->ClearFlexibleMarkers(TRUE);
					}
				}
				else // line contains something else than just outside marker, perform full align.
				{
					LayoutCoord baseline_in_container = line->GetY() + line->GetBaseline();
					AlignMarkers(baseline_in_container, MARKER_ALIGN_PROPER_LINE);
					ClearFlexibleMarkers();
				}
			}

			/* Does the line have a box that is not affected by text align?
			   If yes, then we have to do two things:

			   1) Make sure that the left (right in case of rtl) edge of
			   that box contributes to the final bounding box as it was not moved by text-align.

			   2) Both for outside markers and inline compact boxes, we have to ensure
			   that their position is not affected by:
			   - padding or border of the principal box
			   - text-indent
			   - presence of floats.
			   Now it may turn out that the x position of such box
			   is outdated and needs to be adjusted, same for children text boxes.
			   FIXME: in case of bidi, segment's setting must be adjusted. That won't
			   work for inline compact boxes well. Outside list item markers are currently hacked. */
			BOOL has_bounding_box_override = line->HasBoxNotAffectedByTextAlign();
			LayoutCoord bounding_box_override_value(0);
			const HTMLayoutProperties* props = cascade->GetProps();
			if (has_bounding_box_override)
			{
				/* Left or right (in case of rtl) line box edge distance from the container's
				   placeholder's edge that is on the same side. */
				LayoutCoord line_offset;
				// The outside list item marker element or the element that owns an inline compact box.
				HTML_Element* start_element = line->GetStartElement();

				OP_ASSERT(start_element->GetLayoutBox()->IsInlineBox());
				InlineBox* first_box = static_cast<InlineBox*>(start_element->GetLayoutBox());
				LayoutCoord marker_old_pos = first_box->GetVirtualPosition();

				// Implied from assumption of both inline compact box and outside list item marker.
				OP_ASSERT(marker_old_pos < 0);

#ifdef SUPPORT_TEXT_DIRECTION
				if (props->direction == CSS_VALUE_rtl)
				{
					line_offset = GetWidth() - (line->GetX() + line->GetWidth());
					bounding_box_override_value = line->GetWidth() - marker_old_pos;
				}
				else
#endif // SUPPORT_TEXT_DIRECTION
				{
					line_offset = line->GetX();
					bounding_box_override_value = marker_old_pos;
				}

				/* NOTE: There is no point in carrying out any bbox update
				   and x position of the first inline box if the line extends
				   the container bound. In such case simply allow the first
				   box to be shifted as well by such distance. */
				if (line_offset > 0)
				{
#ifdef SUPPORT_TEXT_DIRECTION
					if (props->direction == CSS_VALUE_rtl)
						bounding_box_override_value += line_offset;
					else
#endif // SUPPORT_TEXT_DIRECTION
						bounding_box_override_value -= line_offset;

					first_box->AdjustVirtualPos(-line_offset);

#ifdef SUPPORT_TEXT_DIRECTION
					if (is_outside_marker_line && first_box->GetContent()->IsInlineContent())
					{
						/* A hack. In case of an outside list item numeration marker, update also
						   the segments that are parts of the marker. */

						ParagraphBidiSegment* iter = bidi_segments ? static_cast<ParagraphBidiSegment*>(bidi_segments->First()) : NULL;
						ParagraphBidiSegment* prev_segment = NULL;

						/* If this is a numeration list item marker, its text box must have
						   been created earlier. */
						OP_ASSERT(start_element->FirstChild());

						while (iter)
						{
							if (iter->start_element != start_element && iter->start_element != start_element->FirstChild())
								// Reached the first segment outside the list item marker.
								break;

							if (!prev_segment)
								iter->width -= line_offset; // For first segment only.
							else
								iter->virtual_position -= line_offset; // For every segment except the first one.

							prev_segment = iter;
							iter = iter->Suc();
						}

						if (prev_segment)
							prev_segment->width += line_offset; // For last segment only.
					}
#endif // SUPPORT_TEXT_DIRECTION
				}
			}

#ifdef SUPPORT_TEXT_DIRECTION
			if (props->direction == CSS_VALUE_rtl)
			{
				/* In case of rtl, the left extent on the line that doesn't come from
				   relative positioning will be moved to the right side.
				   Therefore we have to shift the bounding box accordingly. */
				LayoutCoord left_extent = reflow_state->line_state.committed.bounding_box_left_non_relative - line->GetX();
				reflow_state->line_state.committed.bounding_box_right -= left_extent;
				reflow_state->line_state.committed.bounding_box_left -= left_extent;
			}
#endif // SUPPORT_TEXT_DIRECTION

			LayoutCoord x_offset = line->GetTextAlignOffset(*props);

			/* Adjust the line bounding box by the offset added by text-alignment. */

			reflow_state->line_state.committed.bounding_box_left += x_offset;
			reflow_state->line_state.committed.bounding_box_right += x_offset;

			reflow_state->line_state.committed.content_box_right += x_offset;

			if (reflow_state->line_state.committed.abspos_content_box_right)
				reflow_state->line_state.committed.abspos_content_box_right += x_offset;

			if (has_bounding_box_override)
			{
				// Make sure we don't lose the initial outer edge of the box that is not affected by text-align.
#ifdef SUPPORT_TEXT_DIRECTION
				if (props->direction == CSS_VALUE_rtl)
				{
					if (bounding_box_override_value > reflow_state->line_state.committed.bounding_box_right)
						reflow_state->line_state.committed.bounding_box_right = bounding_box_override_value;
				}
				else
#endif // SUPPORT_TEXT_DIRECTION
					if (bounding_box_override_value < reflow_state->line_state.committed.bounding_box_left)
						reflow_state->line_state.committed.bounding_box_left = bounding_box_override_value;
			}

			/* Here, we set the bounding box for the line box. GetX() must be subtracted
			   because line_box_left includes left border and padding from the container
			   which should not be counted as overflow for the line box. */

			LayoutCoord line_y =  line->GetY();
			LayoutCoord line_height = MAX(line->GetLayoutHeight(), reflow_state->line_state.committed.minimum_height);
			LayoutCoord line_above_baseline = line->GetBaseline();
			LayoutCoord line_below_baseline = line_height - line_above_baseline;

			LayoutCoord box_above_baseline = MAX(line_above_baseline + reflow_state->line_state.committed.bounding_box_above_top,
												 reflow_state->line_state.committed.bounding_box_above_baseline);
			box_above_baseline = MAX(box_above_baseline, reflow_state->line_state.committed.bounding_box_above_bottom - line_below_baseline);

			LayoutCoord box_below_baseline = line_height - line_above_baseline;

			if (reflow_state->line_state.committed.bounding_box_below_bottom > 0)
				box_below_baseline += reflow_state->line_state.committed.bounding_box_below_bottom;

			box_below_baseline = MAX(box_below_baseline, reflow_state->line_state.committed.bounding_box_below_baseline);
			box_below_baseline = MAX(box_below_baseline, reflow_state->line_state.committed.bounding_box_below_top - line_above_baseline);

			line->SetHasOverflowingContent(line->GetStartPosition() > reflow_state->line_state.committed.min_inline_virtual_pos,
										   reflow_state->line_state.committed.max_inline_virtual_pos  > line->GetEndPosition());

			line->SetBoundingBox(reflow_state->line_state.committed.bounding_box_left - line->GetX(),
								 reflow_state->line_state.committed.bounding_box_right - line->GetX(),
								 box_above_baseline,
								 box_below_baseline);

			LayoutCoord width = reflow_state->line_state.committed.bounding_box_right - reflow_state->line_state.committed.bounding_box_left;

			/* This is the line bounding box relative to the container. */

			bounding_box.Set(reflow_state->line_state.committed.bounding_box_left, // GetX() is included in line_box_left. No need to add it here.
							 line_y + line->GetBaseline() - box_above_baseline,
							 width,
							 box_above_baseline + box_below_baseline);

			/* Check if we really need to propagate the bounding box to the container. */

			if (bounding_box.GetX() < 0 ||
				bounding_box.GetY() < 0 ||
				bounding_box.GetX() + bounding_box.GetWidth() > GetWidth() ||
				bounding_box.GetY() + bounding_box.GetHeight() > GetHeight())
			{
				need_bounding_box = TRUE;

				LayoutCoord content_bottom = line_y + MAX(line->GetLayoutHeight(), reflow_state->line_state.committed.content_box_below_top);
				content_bottom = MAX(content_bottom, line_y + line_above_baseline + reflow_state->line_state.committed.content_box_below_baseline);
				content_bottom = MAX(content_bottom, line_y + line_above_baseline + line_below_baseline + reflow_state->line_state.committed.content_box_below_bottom);

				bounding_box.SetContentSize(reflow_state->line_state.committed.content_box_right - bounding_box.GetX(),
											content_bottom - bounding_box.GetY());
			}

			if (reflow_state->line_state.committed.abspos_content_box_right ||
				reflow_state->line_state.committed.abspos_content_box_below_top ||
				reflow_state->line_state.committed.abspos_content_box_below_baseline > LAYOUT_COORD_HALF_MIN ||
				reflow_state->line_state.committed.abspos_content_box_below_bottom > LAYOUT_COORD_HALF_MIN)
			{
				LayoutCoord abspos_bottom(0);

				if (reflow_state->line_state.committed.abspos_content_box_below_top)
					abspos_bottom = line_y + reflow_state->line_state.committed.abspos_content_box_below_top;

				if (reflow_state->line_state.committed.abspos_content_box_below_baseline > LAYOUT_COORD_HALF_MIN)
					abspos_bottom = MAX(abspos_bottom, line_y + line_above_baseline + reflow_state->line_state.committed.abspos_content_box_below_baseline);

				if (reflow_state->line_state.committed.abspos_content_box_below_bottom > LAYOUT_COORD_HALF_MIN)
					abspos_bottom = MAX(abspos_bottom, line_y + line_above_baseline + line_below_baseline + reflow_state->line_state.committed.abspos_content_box_below_bottom);

				abspos_content_box.Set(LayoutCoord(0), LayoutCoord(0), reflow_state->line_state.committed.abspos_content_box_right, abspos_bottom);
				abspos_content_box.SetContentSize(abspos_content_box.GetWidth(), abspos_content_box.GetHeight());

				if (!abspos_content_box.IsEmpty())
					PropagateBottom(info, LayoutCoord(0), LayoutCoord(0), abspos_content_box, PROPAGATE_ABSPOS);
			}

			/* Invalidate the bounding box for the line. The translation is currently at the container. */

			info.visual_device->UpdateRelative(bounding_box.GetX(),
											   bounding_box.GetY(),
											   bounding_box.GetWidth(),
											   bounding_box.GetHeight());
		}
		else
			line->ClearBoundingBox();
	}

	if (in_stack)
	{
		// If we have forced line end, there isn't much to do now, since the line has already been closed.

		if (!reflow_state->forced_line_end)
		{
			Line* line = vertical_layout->IsLine() ? (Line*) vertical_layout : 0;

			if (line)
			{
				LayoutCoord line_height = line->Line::GetLayoutHeight();

				if (packed.no_content &&
					(!line->IsOutsideListMarkerLine() || !(reflow_state->list_item_marker_state & FLEXIBLE_MARKER_NOT_ALIGNED)))
					packed.no_content = FALSE;

				line->SetNumberOfWords();

				if (line_height < reflow_state->line_state.committed.minimum_height)
				{
					line_height = reflow_state->line_state.committed.minimum_height;
					line->SetHeight(line_height);
				}

				if (reflow_state->first_moved_floating_box)
					/* We know the height of the line now, and we have a list of floats whose top
					   margin edges must be below the bottom edge of the current line. */

					PushFloatsBelowLine(info, line);

				UpdateBottomMargins(info, NULL, need_bounding_box ? &bounding_box : NULL);

				if (closed_by != LINE)
				{
					// This signifies the end of a paragraph

					line->SetHasForcedBreak();

					if (reflow_state->calculate_min_max_widths)
					{
						if (info.table_strategy == TABLE_STRATEGY_TRUE_TABLE && reflow_state->true_table_lines >= 0)
						{
							// Check if the contents on the line is favorable or not for a TrueTable.

							if (reflow_state->floats_max_width + reflow_state->line_max_width > reflow_state->true_table_content_width * 2)
								/* This is not a favorable line. More than two thirds of the contents
								   (pixel-wise) on the line is unsuitable in a TrueTable. This line reduces
								   the likelyhood for an ancestor table to become a TrueTable. */

								reflow_state->non_true_table_lines++;
							else
								if (reflow_state->true_table_content_width)
									/* This is a favorable line. This line has some suitable TrueTable
									   content, and none or very little unsuitable content. This line
									   increases the likelyhood for an ancestor table to become a
									   TrueTable. */

									reflow_state->true_table_lines++;

							reflow_state->true_table_content_width = 0;
						}

						reflow_state->FinishVirtualLine();
						reflow_state->line_max_width = LayoutCoord(0);
						reflow_state->available_line_max_width = LAYOUT_COORD_MIN;
					}
				}

				reflow_state->list_item_marker_state &= ~MARKER_BEFORE_NEXT_ELEMENT;
			}
			else
				if (info.table_strategy == TABLE_STRATEGY_TRUE_TABLE && vertical_layout->IsBlock())
					switch (vertical_layout->IsTrueTableCandidate())
					{
					case NEUTRAL:
						break;

					case POSITIVE:
						if (reflow_state->true_table_lines >= 0)
							reflow_state->true_table_lines++;

						break;

					case NEGATIVE:
						reflow_state->non_true_table_lines++;
						break;

					case DISABLE:
						reflow_state->true_table_lines = -1;
						break;
					}

#ifdef CONTENT_MAGIC
			if (info.content_magic && (!line || closed_by != LINE))
				info.doc->CheckContentMagic();
#endif // CONTENT_MAGIC

			if (
#ifdef PAGED_MEDIA_SUPPORT
				info.paged_media != PAGEBREAK_OFF ||
#endif // PAGED_MEDIA_SUPPORT
				cascade->multipane_container || IsMultiPaneContainer())
			{
				// Update widows/orphans

				/* This implementation doesn't follow the spec accurately. It merely
				   marks all consecutive lines (or line breaks) that follow at least
				   'orphans' and are followed by at least 'widows' lines (or line breaks)
				   in a container as breakable. This isn't really close to what the spec
				   says, but it may be a good enough approximation for most normal cases,
				   and code-wise it has the merit of being really simple. */

				const HTMLayoutProperties& props = *placeholder->GetCascade()->GetProps();
				VerticalLayout* break_before = NULL;
				VerticalLayout* prev = NULL;
				int orphans = props.page_props.orphans;
				int widows = props.page_props.widows;

				for (VerticalLayout* ptr = vertical_layout; ptr; ptr = ptr->GetPreviousElement())
					if (ptr->IsLine() || ptr->IsLayoutBreak())
					{
						prev = ptr;

						if (widows > 0)
						{
							break_before = ptr;
							--widows;
						}
						else
							// Walked past widows (if any).

							if (--orphans <= 0)
								// Walked past orphans (if any).

								if (break_before)
								{
									break_before->SetAllowBreakBefore();
									break;
								}
								else
									/* Didn't find a break opportunity, because there was no minimum widow
									   requirement. We have now found enough orphans, and can place a break
									   opportunity after them. However, since this machinery only sets BREAK
									   BEFORE opportunities, we need to see if there's one more element, and
									   then set the last element as a break before opportunity. */

									break_before = vertical_layout;
					}
					else
					{
						if (prev)
							/* orphans / widows settings shouldn't be able to prevent
							   breaking between e.g. a block and a line. */

							prev->SetAllowBreakBefore();

						break;
					}
			}

#ifdef PAGED_MEDIA_SUPPORT
			/* If page breaking is on, switch to a new page when we overflow
			   the current one. Don't do it inside of multi-pane containers,
			   though; we have to distribute into columns and page container
			   pages before we distribute into pages for paged media. */

			if (info.paged_media == PAGEBREAK_ON && !cascade->multipane_container && !IsMultiPaneContainer())
			{
				BOOL is_iframe = FALSE;

				if (vertical_layout->IsBlock() && ((BlockBox*)vertical_layout)->GetContent() && ((BlockBox*)vertical_layout)->GetContent()->IsIFrame())
					is_iframe = TRUE;

				if (info.doc->OverflowedPage(vertical_layout->GetStackPosition(), vertical_layout->GetLayoutHeight(), is_iframe))
				{
					// Oops, element stretches onto next page!

					SEARCH_CONSTRAINT constraint = closed_by != LINE && closed_by != BREAK ? HARD : SOFT;

					if (InsertPageBreak(info, 0, constraint) != BREAK_FOUND)
						if (constraint == HARD)
							// Grumble, try harder

							if (InsertPageBreak(info, 1, HARD) != BREAK_FOUND)
								// Breaking the law, breaking the law...

								if (InsertPageBreak(info, 2, HARD) != BREAK_FOUND)
									// Insert page break before last element as a last resort

									InsertPageBreak(info, 3, HARD);
				}
			}
#endif // PAGED_MEDIA_SUPPORT

			switch (closed_by)
			{
			case BLOCK:
				reflow_state->was_lpw_ignored = FALSE;
				reflow_state->margin_state.ApplyDefaultMargin();
				break;
			case FINISHLAYOUT:
				if (placeholder->GetLocalSpaceManager())
					break;
				else
				{
					Markup::Type type = GetHtmlElement()->Type();

					if (type == Markup::HTE_BODY || type == Markup::HTE_HTML)
						break;
				}

			default:
				reflow_state->margin_state.ApplyDefaultMargin();
				break;
			}
		}

		reflow_state->line_has_content = FALSE;
	}
	else
		if (need_bounding_box)
			UpdateBoundingBox(bounding_box, FALSE);

#ifdef PAGED_MEDIA_SUPPORT
	if (info.paged_media != PAGEBREAK_OFF)
		if (!SkipPageBreaks(info))
			return LAYOUT_OUT_OF_MEMORY;
#endif // PAGED_MEDIA_SUPPORT

	if (in_stack && reflow_state->is_css_first_line)
	{
		/* For virtual line maximum width measurement, there's a bug here. The virtual
		   line maximum width may have been affected by the properties specified on
		   ::first-line, which in turn is affected by the containing block width. Virtual
		   line maximum width should NEVER depend on the containing block. */

		reflow_state->Reset();

		reflow_state->is_css_first_line = FALSE;
		reflow_state->last_base_character = 0;

		return LAYOUT_END_FIRST_LINE;
	}
	else
		return LAYOUT_CONTINUE;
}

/** Check if this is a one line box that fit inside margin_width. */

BOOL
Container::IsNarrowerThan(LayoutCoord margin_width) const
{
	VerticalLayout* vertical_layout = (VerticalLayout*) layout_stack.First();

	return vertical_layout && !vertical_layout->Suc() && vertical_layout->IsLine() && ((Line*) vertical_layout)->GetUsedSpace() < margin_width;
}

/** Update height of container. */

// Note that RootContainer::UpdateHeight does not call this method. So if you add code here you may need to duplicate it there.
/* virtual */ void
Container::UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord bbox_bottom(0);
	LayoutCoord border_padding = props.padding_top + props.padding_bottom + LayoutCoord(props.border.top.width + props.border.bottom.width);
	BOOL honor_max_height = props.max_height >= 0 && info.doc->GetLayoutMode() != LAYOUT_MSR && info.doc->GetLayoutMode() != LAYOUT_AMSR && (props.max_paragraph_width < 0 || GetWidth() < props.max_paragraph_width);

	if (reflow_state->stop_bottom_margin_collapsing)
	{
		content_height += reflow_state->margin_state.GetHeight();

		if (content_height < 0)
			content_height = LayoutCoord(0);

#ifdef PAGED_MEDIA_SUPPORT
		if (PagedContainer* this_pc = GetPagedContainer())
			border_padding += this_pc->GetPageControlHeight();
#endif // PAGED_MEDIA_SUPPORT
	}

	if (content_height < reflow_state->float_bottom)
		content_height = reflow_state->float_bottom;

	if (cascade->flexbox && !initial_content_height && !IsMultiPaneContainer())
	{
		/* We have already handled this for multicol and paged containers (in a
		   special way). For all other container types, propagate minimum and
		   hypothetical border box height now. */

		FlexItemBox* item_box = (FlexItemBox*) placeholder;
		LayoutCoord auto_height = content_height;

		if (props.box_sizing == CSS_VALUE_border_box)
			auto_height += border_padding;

		LayoutCoord hypothetical_height = props.content_height >= 0 ? props.content_height : auto_height;

		auto_height = props.CheckHeightBounds(auto_height);
		hypothetical_height = props.CheckHeightBounds(hypothetical_height);

		if (props.box_sizing != CSS_VALUE_border_box)
		{
			auto_height += border_padding;
			hypothetical_height += border_padding;
		}

		item_box->PropagateHeights(auto_height, hypothetical_height);

		if (!cascade->flexbox->IsVertical())
		{
			// Stretch stretchable items.

			LayoutCoord stretch_height = item_box->GetFlexHeight(cascade);

			if (stretch_height != CONTENT_HEIGHT_AUTO)
			{
				content_height = stretch_height - border_padding;

				if (content_height < 0)
					content_height = LayoutCoord(0);
			}
		}
	}

	if (cascade->flexbox && cascade->flexbox->IsVertical())
		// Flex item main sizes are calculated by the flex algorithm.

		content_height = ((FlexItemBox*) placeholder)->GetFlexHeight(cascade);
	else
	{
		/* Honor height property even if content is higher.

		   (IE does so only if overflow is hidden but that is
		   not according to the spec). */

		if (reflow_state->css_height != CONTENT_HEIGHT_AUTO &&
			(!reflow_state->apply_height_is_min_height || content_height < reflow_state->css_height) &&
			!placeholder->IsTableCell())
			content_height = reflow_state->css_height;

		if (props.box_sizing != CSS_VALUE_content_box)
			content_height += border_padding;

		if (honor_max_height && content_height > props.max_height)
			content_height = props.max_height;

		if (content_height < props.min_height)
			content_height = props.min_height;
		else
			if (content_height < 0 && !placeholder->IsBlockBox())
				content_height = LayoutCoord(0);

		if (props.box_sizing == CSS_VALUE_content_box)
			content_height += border_padding;
		else
			if (content_height < border_padding)
				content_height = border_padding;
	}

	if (content_height != height)
	{
		placeholder->ReduceBoundingBox(content_height - height, LayoutCoord(0), props.DecorationExtent(info.doc));

		height = content_height;
	}

	if (bbox_bottom && props.overflow_x == CSS_VALUE_visible)
	{
		AbsoluteBoundingBox generated_bounding_box;

		generated_bounding_box.Set(LayoutCoord(0), LayoutCoord(0), LayoutCoord(0), bbox_bottom);
		placeholder->UpdateBoundingBox(generated_bounding_box, FALSE);
	}

	if (reflow_state->calculate_min_max_widths)
	{
		LayoutCoord nonpercent_border_padding = props.GetNonPercentVerticalBorderPadding();

		if (reflow_state->stop_bottom_margin_collapsing)
		{
			min_content_height += reflow_state->margin_state.GetHeightNonPercent();

			if (min_content_height < 0)
				min_content_height = LayoutCoord(0);
		}

		if (min_content_height < reflow_state->float_min_bottom)
			min_content_height = reflow_state->float_min_bottom;

		if (reflow_state->css_height != CONTENT_HEIGHT_AUTO && (reflow_state->inside_fixed_height || props.content_height >= 0) &&
			(min_content_height < props.content_height || !reflow_state->apply_height_is_min_height && !placeholder->IsTableCell()))
		{
			min_content_height = props.content_height >= 0 ? props.content_height : reflow_state->css_height;

			if (props.box_sizing != CSS_VALUE_content_box)
				min_content_height = MAX(LayoutCoord(0), min_content_height - nonpercent_border_padding);
		}

		if (props.box_sizing != CSS_VALUE_content_box)
			min_content_height += nonpercent_border_padding;

		if (honor_max_height && !props.GetMaxHeightIsPercent() && min_content_height > props.max_height)
			min_content_height = props.max_height;

		if (!props.GetMinHeightIsPercent() && min_content_height < props.min_height)
			min_content_height = props.min_height;

		if (props.box_sizing == CSS_VALUE_content_box)
			min_content_height += nonpercent_border_padding;
		else
			if (min_content_height < nonpercent_border_padding)
				min_content_height = nonpercent_border_padding;

		min_height = min_content_height;
	}

	reflow_state->height_known = !initial_content_height;
}

/** Do we need to calculate min/max widths of this box's content? */

/* virtual */ BOOL
Container::NeedMinMaxWidthCalculation(LayoutProperties* cascade) const
{
	return placeholder->NeedMinMaxWidthCalculation(cascade);
}

/** Calculate the width of the containing block established by this container. */

/* virtual */ LayoutCoord
Container::CalculateContentWidth(const HTMLayoutProperties& props, ContainerWidthType width_type) const
{
	return GetWidth() - (props.padding_left + props.padding_right + LayoutCoord(props.border.left.width + props.border.right.width));
}

/** Return the inline run-in at the beginning of the first line of the container, if present. */

InlineRunInBox*
Container::GetInlineRunIn() const
{
	if (VerticalLayout* vertical_layout = (VerticalLayout*)layout_stack.First())
		if (vertical_layout->IsLine())
			if (HTML_Element* first_element = ((Line*) vertical_layout)->GetStartElement())
				if (first_element->GetLayoutBox() && first_element->GetLayoutBox()->IsInlineRunInBox())
					return (InlineRunInBox*) first_element->GetLayoutBox();

	return 0;
}

/** Update screen. */

/* virtual */ void
RootContainer::UpdateScreen(LayoutInfo& info)
{
	if (flex_root)
		ShrinkToFitContainer::UpdateScreen(info);
	else
		BlockContainer::UpdateScreen(info);
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
RootContainer::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	flex_root = info.using_flexroot;

	if (flex_root)
	{
		OP_BOOLEAN size_changed = ShrinkToFitContainer::ComputeSize(cascade, info);

		info.workplace->SignalFlexRootWidthChanged(block_width);

		return size_changed;
	}
	else
		return BlockContainer::ComputeSize(cascade, info);
}

/** Clear min/max width. */

/* virtual */ void
RootContainer::ClearMinMaxWidth()
{
	if (flex_root)
		ShrinkToFitContainer::ClearMinMaxWidth();
	else
		BlockContainer::ClearMinMaxWidth();
}

/** Update height of container. */

/* virtual */ void
RootContainer::UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height)
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	content_height += LayoutCoord(props.border.top.width + props.border.bottom.width) + reflow_state->margin_state.GetHeight();
	if (content_height < reflow_state->css_height)
		content_height = reflow_state->css_height;

	if (content_height != height)
	{
		placeholder->ReduceBoundingBox(content_height - height, LayoutCoord(0), props.DecorationExtent(info.doc));

		height = content_height;
	}
}

/** Propagate the right edge of absolute positioned boxes. */

/* virtual */ void
RootContainer::PropagateRightEdge(const LayoutInfo& info, LayoutCoord right, LayoutCoord noncontent, LayoutFixed percentage)
{
	if (reflow_state && reflow_state->affects_flexroot)
	{
		LayoutCoord percent_width_requirement(0);
		const LayoutFixed max_percentage = LAYOUT_FIXED_HUNDRED_PERCENT;

		if (percentage > LayoutFixed(0) && percentage < max_percentage && noncontent > 0)
		{
			/* If any absolutely positioned boxes has percentual width,
			   calculate how large the (initial) containing block needs to be
			   to accommodate left, margin-left, border and padding.

			   We currently assume that the intrinsic size of an absolutely
			   positioned box with percentual width is 0. That might not be
			   such a brilliant idea. However, fixing that would require quite
			   a lot of code changes. Let's save that for later. */

			LayoutFixed unused_percentage = max_percentage - percentage;

			percent_width_requirement = ReversePercentageToLayoutCoord(unused_percentage, noncontent);
		}

		LayoutCoord min_width = MAX(LayoutCoord(0), MAX(right, percent_width_requirement));

		PropagateMinMaxWidths(info, min_width, min_width, min_width);
	}
}

#ifdef PAGED_MEDIA_SUPPORT

/** Signal that content may have changed. */

/* virtual */ void
RootContainer::SignalChange(FramesDocument* doc)
{
	short overflow = doc->GetLogicalDocument()->GetLayoutWorkplace()->GetViewportOverflowX();

	if (HTMLayoutProperties::IsPagedOverflowValue(overflow))
		placeholder->MarkDirty(doc, FALSE);

	ShrinkToFitContainer::SignalChange(doc);
}

#endif // PAGED_MEDIA_SUPPORT

/** Update margins and bounding box. See declaration in header file for more information. */

/* virtual */ void
Container::UpdateBottomMargins(const LayoutInfo& info, const VerticalMargin* bottom_margin, AbsoluteBoundingBox* child_bounding_box, BOOL has_inflow_content)
{
	/* Calling this method means that container has either inflow content (margins and reflow state is updated)
	   and/or container's bounding/content box will be updated */

	if (has_inflow_content)
	{
		if (reflow_state->reflow_line)
		{
			OP_ASSERT(reflow_state->reflow_line->IsInStack());

			/* Advance reflow position and clear the no_content flag
			   - except the case when we have just closed the line that has only the list item marker
			   positioned outside. In such case just store the reflow position as it would be. */

			LayoutCoord new_reflow_pos = reflow_state->reflow_line->GetStackPosition() + reflow_state->reflow_line->GetLayoutHeight();
			if ((reflow_state->list_item_marker_state & FLEXIBLE_MARKER_NOT_ALIGNED) && reflow_state->reflow_line->IsLine()
				&& static_cast<Line*>(reflow_state->reflow_line)->IsOutsideListMarkerLine())
				reflow_state->list_marker_bottom = new_reflow_pos;
			else
			{
				reflow_state->reflow_position = new_reflow_pos;
				packed.no_content = FALSE;
			}

			if (reflow_state->calculate_min_max_widths && !reflow_state->reflow_line->IsLine())
				reflow_state->min_reflow_position = reflow_state->cur_elm_min_stack_position + reflow_state->reflow_line->GetLayoutMinHeight();
		}
		else
			packed.no_content = FALSE;

		if (bottom_margin)
			reflow_state->margin_state = *bottom_margin;
		else
			reflow_state->margin_state.Reset();

		ClearanceStopsMarginCollapsing(FALSE);
	}

	if (child_bounding_box)
	{
		const HTMLayoutProperties* props = placeholder->GetHTMLayoutProperties();

		OP_ASSERT(props);

		if (props->overflow_x == CSS_VALUE_visible)
			placeholder->UpdateBoundingBox(*child_bounding_box, FALSE);
	}
}

/** Expand container to make space for floating and absolute positioned boxes. See declaration in header file for more information. */

/* virtual */ void
Container::PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts)
{
	/* Floats only modify boxes with their own space manager. Absolutely positioned boxes only
	   modify boxes with their own stacking context. (and you'll only end up here when the
	   propagating box is one of the two aforementioned types). */

	if (reflow_state)
	{
		const HTMLayoutProperties* props = placeholder->GetCascade()->GetProps();

		if (opts.type == PROPAGATE_FLOAT && (placeholder->GetLocalSpaceManager() ||
			props->content_width != CONTENT_WIDTH_AUTO && !placeholder->IsTableBox() && !info.hld_profile->IsInStrictMode()))
		{
			bottom -= props->padding_top + LayoutCoord(props->border.top.width);

			if (opts.type == PROPAGATE_FLOAT && reflow_state->float_bottom < bottom)
				reflow_state->float_bottom = bottom;

			if (reflow_state->calculate_min_max_widths)
			{
				min_bottom -= props->GetNonPercentTopBorderPadding();

				if (opts.type == PROPAGATE_FLOAT && reflow_state->float_min_bottom < min_bottom)
					reflow_state->float_min_bottom = min_bottom;
			}

			if (props->overflow_x == CSS_VALUE_visible)
				placeholder->UpdateBoundingBox(child_bounding_box, FALSE);

			if (!packed.no_content || packed.stop_top_margin_collapsing)
				return;
		}
	}

	// Propagate past this container

	placeholder->PropagateBottom(info, bottom, min_bottom, child_bounding_box, opts);
}

/** Commit pending margin, to prevent it from collapsing with later margins. */

void
Container::CommitMargins()
{
	if (reflow_state->calculate_min_max_widths)
		reflow_state->min_reflow_position += reflow_state->margin_state.GetHeightNonPercent();

	reflow_state->reflow_position += reflow_state->margin_state.GetHeight();
	reflow_state->margin_state.Reset();
}

/** Propagate the right edge of absolute positioned boxes. */

/* virtual */ void
Container::PropagateRightEdge(const LayoutInfo& info, LayoutCoord right, LayoutCoord noncontent, LayoutFixed percentage)
{
	if (reflow_state)
	{
		LayoutCoord old_requirement = LAYOUT_COORD_MIN;
		LayoutCoord new_requirement = LAYOUT_COORD_MIN;
		const LayoutFixed max_percentage = LAYOUT_FIXED_HUNDRED_PERCENT;

		if (reflow_state->abspos_percentage > LayoutFixed(0) && reflow_state->abspos_percentage < max_percentage)
		{
			LayoutFixed unused_percentage = max_percentage - reflow_state->abspos_percentage;
			old_requirement = ReversePercentageToLayoutCoord(unused_percentage, reflow_state->abspos_noncontent);
		}

		if (percentage > LayoutFixed(0) && percentage < max_percentage)
		{
			LayoutFixed unused_percentage = max_percentage - percentage;
			new_requirement = ReversePercentageToLayoutCoord(unused_percentage, noncontent);
		}

		if (old_requirement < new_requirement)
		{
			reflow_state->abspos_noncontent = noncontent;
			reflow_state->abspos_percentage = percentage;
		}

		if (reflow_state->abspos_right_edge < right)
			reflow_state->abspos_right_edge = right;
	}
	else
		if (right > 0 || percentage > LayoutFixed(0))
			placeholder->PropagateRightEdge(info, right, noncontent, percentage);
}

/** Expand container to make space for floating and absolute positioned boxes. See declaration in header file for more information. */

/* virtual */ void
Container::UpdateBoundingBox(const AbsoluteBoundingBox& child_bounding_box, BOOL skip_content)
{
	const HTMLayoutProperties& props = *placeholder->GetCascade()->GetProps();

	if (props.overflow_x == CSS_VALUE_visible)
		placeholder->UpdateBoundingBox(child_bounding_box, skip_content);
}

/** Propagate min/max widths. */

/* virtual */ void
Container::PropagateMinMaxWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width)
{
	OP_ASSERT(info.doc->GetLayoutMode() != LAYOUT_NORMAL || minimum_width == normal_minimum_width);

	if (reflow_state->calculate_min_max_widths && !packed.minmax_calculated)
	{
		LayoutProperties* cascade = placeholder->GetCascade();
		const HTMLayoutProperties& props = *cascade->GetProps();
		LayoutCoord extra_width = props.GetNonPercentHorizontalBorderPadding();

		if (placeholder->IsTableCell())
		{
			// pay attention to desired width on table cells

			TableCellBox* cell_box = (TableCellBox*) placeholder;
			LayoutCoord desired_width = cell_box->GetDesiredWidth();
			BOOL cell_has_desired_width = desired_width > 0;

			if (desired_width == CONTENT_WIDTH_AUTO)
				/* No desired cell width. Check for desired column width then,
				   but ignore colspan, just like MSIE. */

				desired_width = cascade->table->GetCellWidth(cell_box->GetColumn(), 1, TRUE);

			if (desired_width > 0)
			{
				desired_width -= extra_width;

				if (desired_width < min_width)
				{
					if (cell_has_desired_width)
					{
						OP_ASSERT(min_width <= normal_min_width);
						max_width = normal_min_width;
					}
				}
				else
				{
					if (!info.hld_profile->IsInStrictMode() &&
						(cascade->html_element->Type() == Markup::HTE_TD ||
						 cascade->html_element->Type() == Markup::HTE_TH) &&
						cascade->html_element->HasAttr(Markup::HA_NOWRAP))
					{
						if (info.doc->GetLayoutMode() == LAYOUT_NORMAL)
							min_width = desired_width;

						if (normal_min_width < desired_width)
							normal_min_width = desired_width;
					}

					if (cell_has_desired_width)
						max_width = MAX(desired_width, normal_min_width);
				}
			}

			switch (info.doc->GetLayoutMode())
			{
			case LAYOUT_AMSR:
			case LAYOUT_MSR:
#ifdef TV_RENDERING
			case LAYOUT_TVR:
#endif // TV_RENDERING
				if ((cascade->html_element->Type() == Markup::HTE_TD ||
					 cascade->html_element->Type() == Markup::HTE_TH) &&
					cascade->html_element->HasAttr(Markup::HA_NOWRAP))
					normal_min_width = max_width;

				break;
			}
		}
		else
			if (props.content_width >= 0 && !placeholder->IsFlexItemBox())
			{
				// Honor width property

				LayoutCoord content_box_width = props.content_width;

				if (props.box_sizing == CSS_VALUE_border_box)
					content_box_width = LayoutCoord(MAX(0, content_box_width - extra_width));

				if (min_width >= content_box_width)
					max_width = min_width = normal_min_width = content_box_width;
				else
				{
					max_width = content_box_width;
					normal_min_width = content_box_width;

					if (info.doc->GetLayoutMode() == LAYOUT_NORMAL)
						min_width = normal_min_width;
				}
			}

		if (props.box_sizing == CSS_VALUE_border_box)
		{
			// Convert to border-box before constraining to min-width and max-width

			min_width += extra_width;
			normal_min_width += extra_width;
			max_width += extra_width;
		}

		/* Honor min-width and max-width properties, unless it's a flex
		   item. We cannot apply min-width and max-width this early; we must
		   leave it to the flex algorithm to figure this out. */

		if (!cascade->flexbox)
			ApplyMinWidthProperty(props, min_width, normal_min_width, max_width);

		if (props.box_sizing == CSS_VALUE_content_box)
		{
			// Convert to border-box if not done earlier

			min_width += extra_width;
			normal_min_width += extra_width;
			max_width += extra_width;
		}

		if (maximum_width < max_width)
			maximum_width = max_width;

		if (normal_minimum_width < normal_min_width)
			normal_minimum_width = normal_min_width;

		if (minimum_width < min_width)
			minimum_width = min_width;

		OP_ASSERT(info.doc->GetLayoutMode() != LAYOUT_NORMAL || minimum_width == normal_minimum_width);
	}
}

/** Apply min-width and max-width property constraints. */

/* virtual */ void
Container::ApplyMinWidthProperty(const HTMLayoutProperties& props, LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width)
{
	min_width = props.CheckWidthBounds(min_width, TRUE);
	normal_min_width = props.CheckWidthBounds(normal_min_width, TRUE);
	max_width = props.CheckWidthBounds(max_width, TRUE);
}

/** Get min/max width. */

/* virtual */ BOOL
Container::GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const
{
	min_width = minimum_width;
	normal_min_width = normal_minimum_width;
	max_width = maximum_width;

	return TRUE;
}

/** Clear min/max width. */

/* virtual */ void
Container::ClearMinMaxWidth()
{
	minimum_width = LayoutCoord(0);
	normal_minimum_width = LayoutCoord(0);
	maximum_width = LayoutCoord(0);
	packed.minmax_calculated = 0;
}

/** Increase actual width and min/max widths. */

/* virtual */ void
Container::IncreaseWidths(LayoutCoord increment)
{
	minimum_width += increment;
	normal_minimum_width += increment;
	maximum_width += increment;
}

/** Propagate min/max widths from in-flow child blocks. */

void
Container::PropagateBlockWidths(const LayoutInfo& info, LayoutProperties* box_cascade, BlockBox* block_box, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width)
{
	LayoutCoord left_floats_max_width(0);
	LayoutCoord right_floats_max_width(0);
	BOOL avoid_float_overlap = reflow_state->avoid_float_overlap;

	if (avoid_float_overlap)
		GetFloatsMaxWidth(box_cascade->space_manager, reflow_state->cur_elm_min_stack_position, block_box->GetContent()->GetMinHeight(), max_width, left_floats_max_width, right_floats_max_width);
	else
		if (!info.hld_profile->IsInStrictMode() && box_cascade->GetProps()->content_width >= 0)
		{
			// Quirk crap. Try not to look.

			Head descendants;

			/* Take all floats that are children of this non-BFC container out of the
			   space manager. In quirks mode, a container with fixed width behaves almost
			   like a BFC container. Among other things, it is affected by floats in the
			   same BFC as it lives - they cannot overlap (very much like a container
			   with non-visible overflow, but not quite). It does not have a
			   SpaceManager, though, and whatever descendant floats this container might
			   have will be added to the nearest SpaceManager ancestor. Positioning this
			   container beside its descendant floats is just nonsense, so take the
			   floats out temporarily. */

			while (FLink* flink = box_cascade->space_manager->Last())
			{
				if (!box_cascade->html_element->IsAncestorOf(flink->float_box->GetHtmlElement()))
					break;

				flink->Out();
				flink->IntoStart(&descendants);
			}

			GetFloatsMaxWidth(box_cascade->space_manager, reflow_state->cur_elm_min_stack_position, block_box->GetContent()->GetMinHeight(), max_width, left_floats_max_width, right_floats_max_width);

			while (FLink* flink = (FLink* ) descendants.First())
			{
				// Put all floats back and pretend like nothing special happened.

				flink->Out();
				box_cascade->space_manager->AddFloat(flink->float_box);
			}

			avoid_float_overlap = TRUE;
		}

	if (avoid_float_overlap && (left_floats_max_width || right_floats_max_width))
	{
		const HTMLayoutProperties& box_props = *box_cascade->GetProps();

		if (!box_props.GetMarginLeftIsPercent())
			left_floats_max_width -= MIN(box_props.margin_left, left_floats_max_width);

		if (!box_props.GetMarginRightIsPercent())
			right_floats_max_width -= MIN(box_props.margin_right, right_floats_max_width);
	}

	PropagateMinMaxWidths(info, min_width, normal_min_width, max_width + left_floats_max_width + right_floats_max_width);
}

/** Propagate min/max widths for floating boxes. */

void
Container::PropagateFloatWidths(const LayoutInfo& info, LayoutProperties* box_cascade, FloatingBox* floating_box, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width)
{
	OP_ASSERT(!floating_box->link.Suc()); // Must be the last float in the block formatting context.
	floating_box->link.Out(); // Take it out of the space manager while allocating space for it.

	SpaceManager* space_manager = box_cascade->space_manager;
	const HTMLayoutProperties& box_props = *box_cascade->GetProps();
	FloatingBox* prev_last_float = space_manager->GetLastFloat();
	Line* current_line = GetReflowLine();
	LayoutCoord block_bfc_min_y = floating_box->GetBfcMinY();
	LayoutCoord forced_bfc_min_top = prev_last_float ? prev_last_float->GetBfcMinY() - prev_last_float->GetMarginTop(TRUE) : LAYOUT_COORD_MIN;
	LayoutCoord bfc_min_y(0);
	LayoutCoord dummy_bfc_y(0);
	LayoutCoord dummy_bfc_x(0);

	GetBfcOffsets(dummy_bfc_x, dummy_bfc_y, bfc_min_y);

	if (box_props.clear != CSS_VALUE_none)
	{
		LayoutCoord clear_bfc_min_y = space_manager->FindBfcMinBottom((CSSValue) box_props.clear);

		if (forced_bfc_min_top < clear_bfc_min_y)
			forced_bfc_min_top = clear_bfc_min_y;
	}

	if (block_bfc_min_y < forced_bfc_min_top)
		// Cannot be above previous floats.

		block_bfc_min_y = forced_bfc_min_top;

	LayoutCoord left_floats_max_width;
	LayoutCoord right_floats_max_width;
	LayoutCoord float_min_height = floating_box->GetContent()->GetMinHeight();
	LayoutCoord adjacent_line_max_width(0);
	LayoutCoord block_min_y = block_bfc_min_y - bfc_min_y;

	GetFloatsMaxWidth(space_manager, block_min_y, float_min_height, max_width, left_floats_max_width, right_floats_max_width);

	if (current_line)
	{
		// We were busy reflowing a line while this float was encountered.

		LayoutCoord line_min_y = reflow_state->min_reflow_position;
		LayoutCoord virt_line_height = reflow_state->GetVirtualLineHeight();

		if (block_min_y + float_min_height >= line_min_y &&
			(block_min_y < line_min_y + virt_line_height || !virt_line_height && block_min_y == line_min_y))
			// Float wants to be beside virtual line

			if (max_width <= reflow_state->available_line_max_width)
			{
				// Virtual line fits beside float.

				/* Add max width of float so that further width propagation from the
				   current line includes it. */

				reflow_state->floats_max_width += max_width;

				/* Include max width of line. This may include trailing whitespace, which
				   is wrong, but probably not important enough to do something about. */

				adjacent_line_max_width = reflow_state->line_max_width;
			}
			else
			{
				/* Virtual line doesn't fit beside float. Push float below the line.
				   Note that this is slightly inaccurate - we don't necessarily know the
				   final virtual line height at this point. */

				block_min_y += reflow_state->GetVirtualLineHeight();

				/* See if there's space for the float there or if we have to push it
				   even further down below because of other floats below the line. */

				GetFloatsMaxWidth(space_manager, block_min_y, float_min_height, max_width, left_floats_max_width, right_floats_max_width);
			}
	}

	if (info.table_strategy == TABLE_STRATEGY_TRUE_TABLE)
		// A float always counts negatively for TrueTable detection.

		reflow_state->non_true_table_lines ++;

	block_min_y += floating_box->GetMarginTop(TRUE);
	floating_box->SetMinY(block_min_y);
	floating_box->SetBfcMinY(bfc_min_y + block_min_y);

	LayoutCoord prev_acc_max_width = floating_box->IsLeftFloat() ?
		reflow_state->bfc_left_edge_min_distance + left_floats_max_width :
		reflow_state->bfc_right_edge_min_distance + right_floats_max_width;

	floating_box->SetPrevAccumulatedMaxWidth(prev_acc_max_width);

	// Put the float back into the space manager

	space_manager->AddFloat(floating_box);

	max_width += left_floats_max_width + right_floats_max_width + adjacent_line_max_width;

	PropagateMinMaxWidths(info, min_width, normal_min_width, max_width);
}

/** Get min/max width. */

/* virtual */ BOOL
InlineContent::GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const
{
	return FALSE;
}

/** Clear min/max width. */

/* virtual */ void
InlineContent::ClearMinMaxWidth()
{
}

/** Lay out box. */

/* virtual */ LAYST
InlineContent::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (start_position == LAYOUT_COORD_MIN)
		inline_packed.width = 0;

	return placeholder->LayoutChildren(cascade, info, first_child, start_position);
}

/** Lay out replaced box. */

/* virtual */ LAYST
ReplacedContent::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	OP_ASSERT(!ShouldLayoutChildren());
	return LAYOUT_CONTINUE;
}

/** Finish reflowing box. */

/* virtual */ LAYST
ReplacedContent::FinishLayout(LayoutInfo& info)
{
	LayoutProperties *cascade = placeholder->GetCascade();
	if (placeholder->NeedMinMaxWidthCalculation(cascade))
	{
		LayoutCoord min_width;
		LayoutCoord normal_min_width;
		LayoutCoord max_width;

		if (GetMinMaxWidth(min_width, normal_min_width, max_width))
			placeholder->PropagateWidths(info, min_width, normal_min_width, max_width);
	}

#ifdef CSS_TRANSFORMS
	if (TransformContext* transform_context = placeholder->GetTransformContext())
		if (!placeholder->IsInlineBox())
			transform_context->Finish(info, cascade, GetWidth(), GetHeight());
#endif // CSS_TRANSFORMS

	// Run CalculateObjectFit and CalculateObjectPosition for the bounding box only if content could possibly overflow.
	const HTMLayoutProperties& props = *cascade->GetProps();

	if ((props.overflow_x != CSS_VALUE_hidden
		 || props.overflow_y != CSS_VALUE_hidden)
		&&
		(props.object_fit_position.fit == CSS_VALUE_cover
		 || props.object_fit_position.fit == CSS_VALUE_none
		 || (props.object_fit_position.x_percent ? props.object_fit_position.x < 0 && LayoutFixed(props.object_fit_position.x) > LAYOUT_FIXED_HUNDRED_PERCENT : props.object_fit_position.x != 0)
		 || (props.object_fit_position.y_percent ? props.object_fit_position.y < 0 && LayoutFixed(props.object_fit_position.y) > LAYOUT_FIXED_HUNDRED_PERCENT : props.object_fit_position.y != 0))
		)
	{
		OpRect dst;
		if (CalculateObjectFit(cascade, dst))
		{
			CalculateObjectPosition(cascade, dst);

			AbsoluteBoundingBox box;
			box.Set(LayoutCoord(dst.x), LayoutCoord(dst.y), LayoutCoord(dst.width), LayoutCoord(dst.height));
			box.SetContentSize(box.GetWidth(), box.GetHeight());
			placeholder->UpdateBoundingBox(box, FALSE);
		}
	}
	placeholder->PropagateBottomMargins(info);

	return LAYOUT_CONTINUE;
}

/** Increase actual width and min/max widths. */

/* virtual */ void
ReplacedContent::IncreaseWidths(LayoutCoord increment)
{
	replaced_width += increment;
	minimum_width += increment;
	normal_minimum_width += increment;
	maximum_width += increment;
}

/** Calculate minimum width of border+padding for weak content. */

void
ReplacedContent::CalculateBorderPaddingMinWidth(LayoutProperties* cascade, LayoutCoord& border_padding_width) const
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	if (ScaleBorderAndPadding())
	{
		border_padding_width = LayoutCoord(0);

		if (!props.GetPaddingLeftIsPercent())
			border_padding_width += MIN(LayoutCoord(2), props.padding_left);

		if (!props.GetPaddingRightIsPercent())
			border_padding_width += MIN(LayoutCoord(2), props.padding_right);

		if (props.border.left.width > 0)
			border_padding_width ++;

		if (props.border.right.width > 0)
			border_padding_width ++;
	}
	else
		border_padding_width = props.GetNonPercentHorizontalBorderPadding();
}

/** Clear min/max width. */

/* virtual */ void
ReplacedContent::ClearMinMaxWidth()
{
	packed.minmax_calculated = 0;
}

/** Calculate non-auto width and height for replaced box.*/

OP_STATUS
ReplacedContent::CalculateSize(LayoutProperties* cascade, LayoutInfo& info, BOOL calculate_min_max_widths, LayoutCoord& width, LayoutCoord& height, LayoutCoord css_height, int intrinsic_ratio, LayoutCoord& horizontal_border_padding, LayoutCoord vertical_border_padding, BOOL height_is_auto)
{
	HTMLayoutProperties& props = *cascade->GetProps();
	BOOL width_is_auto = props.content_width == CONTENT_WIDTH_AUTO;
	const LayoutCoord intrinsic_width = width;
	const LayoutCoord intrinsic_height = height;
	BOOL width_is_flex = FALSE;
	BOOL height_is_flex = FALSE;

	packed.percentage_height = props.content_height < 0 && props.content_height >= CONTENT_HEIGHT_MIN;

	OP_ASSERT(width >= 0 && height >= 0);

	if (cascade->flexbox)
	{
		FlexItemBox* item_box = (FlexItemBox*) placeholder;
		LayoutCoord flex_height = item_box->GetFlexHeight(cascade);

		if (flex_height != CONTENT_HEIGHT_AUTO)
		{
			height = flex_height - vertical_border_padding;

			if (height < 0)
				height = LayoutCoord(0);
		}

		width = item_box->GetFlexWidth(cascade) - horizontal_border_padding;

		if (width < 0)
			width = LayoutCoord(0);

		/* Mark the main axis as flexible. This is needed for the following reasons:

		   1. A fixed main size should not affect min/max calculation (this is
		   different from regular, non-flexbox, min/max calculation, where the
		   min/max typically becomes the fixed size).

		   2. Even if the main size is auto, it may be used to change the cross
		   size in order to retain the aspect ratio. Normally (non-flexboxedly)
		   one dimension has to be non-auto in order to affect the other.

		   3. Cross size may not affect main size. When retaining the aspect
		   ratio, this is only allowed to happen by letting the main size
		   affect the cross size, not the other way around (or we'll get
		   circular dependencies (and incorrect layout)).

		   4. In fact, the main size may not be changed at all in this method;
		   that's the flex algorithm's job. */

		if (cascade->flexbox->IsVertical())
			height_is_flex = TRUE;
		else
			width_is_flex = TRUE;
	}
	else
	{
		// If width or height are non-auto, set size based on those values; otherwise keep intrinsic size.

		if (css_height >= 0)
		{
			height = css_height;

			if (props.box_sizing == CSS_VALUE_border_box)
			{
				if (height > vertical_border_padding)
					height -= vertical_border_padding;
				else
					height = LayoutCoord(0);
			}
		}

		if (!width_is_auto && props.content_width != CONTENT_WIDTH_O_SIZE)
		{
			width = props.content_width < 0 ? props.ResolvePercentageWidth(placeholder->GetAvailableWidth(cascade)) : props.content_width;

			if (props.box_sizing == CSS_VALUE_border_box)
				if (width > horizontal_border_padding)
					width -= horizontal_border_padding;
				else
					width = LayoutCoord(0);
		}
	}

	if (intrinsic_ratio > 0)
	{
		if (width_is_auto && height_is_auto && intrinsic_width == 0 && intrinsic_height == 0 && !cascade->flexbox)
		{
			/* "If 'height' and 'width' both have computed values of 'auto' and the element has an
			   intrinsic ratio but no intrinsic height or width, then the used value of 'width' is
			   undefined in CSS 2.1. However, it is suggested that, if the containing block's width
			   does not itself depend on the replaced element's width, then the used value of
			   'width' is calculated from the constraint equation used for block-level, non-replaced
			   elements in normal flow."

			   Implement the suggestion from CSS 2.1 10.3.2 */

			width = placeholder->GetAvailableWidth(cascade) - (props.margin_left + props.margin_right) - horizontal_border_padding;

			if (width < 0)
				width = LayoutCoord(0);
		}

		/* In case we have intrinsic ratio, but no intrinsic width or height, resolve used width
		   from used height and vice versa, if applicable. */

		if (height_is_auto && width > 0 && intrinsic_height == 0)
			height = CalculateHeightFromIntrinsicRatio(width, intrinsic_width, intrinsic_height, intrinsic_ratio);
		else
			if (width_is_auto && height > 0 && intrinsic_width == 0)
				width = CalculateWidthFromIntrinsicRatio(height, intrinsic_width, intrinsic_height, intrinsic_ratio);
	}

	LayoutCoord unbounded_width = width;
	LayoutCoord unbounded_height = height;

	// Honor min-width, max-width, min-height, max-height

	CheckSizeBounds(props, width, height, horizontal_border_padding, vertical_border_padding);

	// Retain aspect ratio, if appropriate

	if (intrinsic_ratio && (width_is_auto || height_is_auto))
	{
		LayoutCoord scaled_width = CalculateWidthFromIntrinsicRatio(height, intrinsic_width, intrinsic_height, intrinsic_ratio);
		LayoutCoord scaled_height = CalculateHeightFromIntrinsicRatio(width, intrinsic_width, intrinsic_height, intrinsic_ratio);
		BOOL intrinsic_ratio_used = FALSE;

		if (width_is_auto && !width_is_flex)
			if (!height_is_auto || height_is_flex)
			{
				unbounded_width = scaled_width;
				intrinsic_ratio_used = TRUE;
			}

		if (height_is_auto && !height_is_flex)
			if (!width_is_auto || width_is_flex)
			{
				unbounded_height = scaled_height;
				intrinsic_ratio_used = TRUE;
			}

		/* Try to keep aspect ratio after having modified either dimension via width, min-width,
		   max-width, height, min-height, max-height */

		if (intrinsic_ratio_used || width != unbounded_width || height != unbounded_height)
		{
			if (scaled_height == 1 && info.doc->GetLayoutMode() != LAYOUT_NORMAL && g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(info.doc->GetPrefsRenderingMode(), PrefsCollectionDisplay::AvoidInterlaceFlicker)))
				scaled_height ++;

			CheckSizeBounds(props, scaled_width, scaled_height, horizontal_border_padding, vertical_border_padding);

			if (width_is_auto && !width_is_flex)
				width = scaled_width;

			if (height_is_auto && !height_is_flex)
				height = scaled_height;
		}
	}

	if (width < unbounded_width && props.max_width != props.normal_max_width && props.content_height > 0 && RetainRatio())
	{
		// ERA caused width decrease. Retain aspect ratio by reducing height.

		LayoutCoord old_width = props.normal_max_width > 0 && props.normal_max_width < unbounded_width ? props.normal_max_width : unbounded_width;

		height = LayoutCoord(height * width / old_width);
	}

#ifdef FIT_LARGE_IMAGES_TO_WINDOW_WIDTH
	if (info.doc->GetTopDocument() && info.doc->GetTopDocument()->GetFitImagesToWindow() &&
		info.doc->GetShowImages() && IsImage() && width > 100 && height > 50)
	{
		// Fit image to window width

		int ratio = 90;

		if (info.doc->GetLayoutMode() != LAYOUT_NORMAL)
			ratio = 60;

		unsigned int window_width = INT_MAX; // some default.
		unsigned int window_height = INT_MAX;
		int orig_width = width;
		long orig_height = height;

		Window* win = info.doc->GetTopDocument()->GetWindow();
		if (win)
		{
			win->GetCSSViewportSize(window_width, window_height);
			window_width = (window_width * ratio) / 100;
			window_height = (window_height * ratio) / 100;
		}

		if (static_cast<unsigned int>(width) > window_width && width * 1000 / window_width > height * 1000 / window_height)
		{
			height = LayoutCoord(height * window_width / width);
			width = LayoutCoord(window_width);
		}
		else
			if (static_cast<unsigned int>(height) > window_height)
			{
				width = LayoutCoord(width * window_height / height);
				height = LayoutCoord(window_height);
			}

		if (cascade->html_element->HasAttr(Markup::HA_USEMAP))
		{
			if (orig_width != width)
			{
				int width_scale = ((1000*width)/orig_width);

				if (width_scale == 0)
					width_scale = 1;

				if (cascade->html_element->SetSpecialAttr(Markup::LAYOUTA_IMAGEMAP_WIDTH_SCALE, ITEM_TYPE_NUM, (void*)(INTPTR)width_scale, FALSE, SpecialNs::NS_LAYOUT) < 0)
					return OpStatus::ERR_NO_MEMORY;
			}

			if (orig_height != height)
			{
				int height_scale = ((1000*height)/orig_height);

				if (height_scale == 0)
					height_scale = 1;

				if (cascade->html_element->SetSpecialAttr(Markup::LAYOUTA_IMAGEMAP_HEIGHT_SCALE, ITEM_TYPE_NUM, (void*)(INTPTR)height_scale, FALSE, SpecialNs::NS_LAYOUT) < 0)
					return OpStatus::ERR_NO_MEMORY;
			}
		}
	}
#endif // FIT_LARGE_IMAGES_TO_WINDOW_WIDTH

	BOOL allow_zero_size = AllowZeroSize();

	if (!allow_zero_size)
	{
		if (width < 1)
			width = LayoutCoord(1);

		if (height < 1)
			height = LayoutCoord(1);
	}

	if (info.doc->GetLayoutMode() != LAYOUT_NORMAL)
		RETURN_IF_ERROR(AdjustEraSize(cascade, info, width, height, intrinsic_width, intrinsic_height, horizontal_border_padding, intrinsic_ratio > 0));

	if (calculate_min_max_widths)
	{
		LayoutCoord min_width = intrinsic_width >= 0 ? LayoutCoord(intrinsic_width) : LayoutCoord(0);
		LayoutCoord max_width = min_width;
		LayoutCoord min_height = intrinsic_height >= 0 ? intrinsic_height : LayoutCoord(0);
		LayoutCoord nonpercent_horizontal_border_padding = props.GetNonPercentHorizontalBorderPadding();
		LayoutCoord nonpercent_vertical_border_padding = props.GetNonPercentVerticalBorderPadding();

		/* Set initial min/max width/height values (for propagation to ancestor containers and
		   tables) based on computed CSS values for width/height. Later in this method we will
		   constrain to min-width and max-width properties and resolve auto values using aspect
		   ratio. */

		if (props.content_height != CONTENT_HEIGHT_AUTO && props.content_height != CONTENT_HEIGHT_O_SIZE && !height_is_flex)
			if (props.content_height >= 0 || cascade->container && cascade->container->IsInsideFixedHeight())
			{
				if (props.content_height >= 0)
					min_height = props.content_height;
				else
					// Percentual height that can be safely resolved to a minimum height.

					min_height = css_height;

				if (props.box_sizing == CSS_VALUE_border_box)
					min_height -= MIN(nonpercent_vertical_border_padding, min_height);
			}

		if (width_is_auto || props.content_width == CONTENT_WIDTH_O_SIZE)
		{
			if (height_is_flex && intrinsic_ratio)
			{
				/* Auto-width replaced vertical flex items with an intrinsic ratio get
				   their minimum width from their minimum height. Calculate it now, and
				   leave it alone for the rest of this method. */

				LayoutCoord minimum_height(0);

				if (!props.GetMinHeightIsPercent() && props.min_height != CONTENT_SIZE_AUTO)
				{
					minimum_height = props.min_height;

					if (props.box_sizing == CSS_VALUE_border_box)
					{
						minimum_height -= nonpercent_vertical_border_padding;

						if (minimum_height < 0)
							minimum_height = LayoutCoord(0);
					}
				}

				min_width = CalculateWidthFromIntrinsicRatio(minimum_height, intrinsic_width, intrinsic_height, intrinsic_ratio);
			}
		}
		else
			if (!width_is_flex)
				if (props.content_width >= 0)
				{
					min_width = max_width = props.content_width;

					if (props.box_sizing == CSS_VALUE_border_box)
					{
						min_width -= MIN(nonpercent_horizontal_border_padding, min_width);
						max_width -= MIN(nonpercent_horizontal_border_padding, max_width);
					}
				}
				else
					min_width = LayoutCoord(0);

		if (intrinsic_ratio > 0 && ((intrinsic_width > 0) ^ (intrinsic_height > 0)))
			/* After style has been applied we check again if we can use the intrinsic ratio to
			   resolve one width/height from the other, if width or height is auto and the other
			   is not. */

			if (intrinsic_width > 0)
			{
				if (props.content_height == CONTENT_HEIGHT_AUTO)
					min_height = CalculateHeightFromIntrinsicRatio(max_width, intrinsic_width, intrinsic_height, intrinsic_ratio);
			}
			else
				if (width_is_auto && !height_is_flex)
					min_width = max_width = CalculateWidthFromIntrinsicRatio(min_height, intrinsic_width, intrinsic_height, intrinsic_ratio);

		LayoutCoord unbounded_maximum_width = max_width;
		LayoutCoord unbounded_minimum_height = min_height;

		CheckContentBoxWidthBounds(props, min_width, nonpercent_horizontal_border_padding, TRUE);
		CheckContentBoxWidthBounds(props, max_width, nonpercent_horizontal_border_padding, TRUE);
		CheckContentBoxHeightBounds(props, min_height, nonpercent_vertical_border_padding, TRUE);

		if (intrinsic_ratio && (width_is_auto || height_is_auto))
		{
			if (width_is_auto && !height_is_auto && !height_is_flex)
				min_width = max_width = CalculateWidthFromIntrinsicRatio(min_height, intrinsic_width, intrinsic_height, intrinsic_ratio);

			if (height_is_auto && !width_is_auto)
				min_height = CalculateHeightFromIntrinsicRatio(max_width, intrinsic_width, intrinsic_height, intrinsic_ratio);

			/* Try to keep aspect ratio between minimum/maximum width and minimum height after
			   having modified either dimension via width, min-width, max-width, height,
			   min-height, max-height */

			if (!width_is_auto || !height_is_auto || max_width != unbounded_maximum_width || min_height != unbounded_minimum_height)
			{
				LayoutCoord scaled_minmax_width = CalculateWidthFromIntrinsicRatio(min_height, intrinsic_width, intrinsic_height, intrinsic_ratio);
				LayoutCoord scaled_minimum_height = CalculateHeightFromIntrinsicRatio(max_width, intrinsic_width, intrinsic_height, intrinsic_ratio);

				if (scaled_minimum_height == 1 && info.doc->GetLayoutMode() != LAYOUT_NORMAL && g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(info.doc->GetPrefsRenderingMode(), PrefsCollectionDisplay::AvoidInterlaceFlicker)))
					scaled_minimum_height ++;

				if (width_is_auto && !height_is_flex || width_is_flex)
				{
					CheckContentBoxWidthBounds(props, scaled_minmax_width, nonpercent_horizontal_border_padding, TRUE);
					max_width = min_width = scaled_minmax_width;
				}

				if (height_is_auto || height_is_flex)
				{
					CheckContentBoxHeightBounds(props, scaled_minimum_height, nonpercent_vertical_border_padding, TRUE);
					min_height = scaled_minimum_height;
				}
			}
		}

		if (!allow_zero_size)
		{
			if (min_width < 1)
				min_width = LayoutCoord(1);

			if (max_width < 1)
				max_width = LayoutCoord(1);

			if (min_height < 1)
				min_height = LayoutCoord(1);
		}

		// Convert to border-box

		min_width += nonpercent_horizontal_border_padding;
		max_width += nonpercent_horizontal_border_padding;
		min_height += nonpercent_vertical_border_padding;

		// Store new min/max values.

		normal_minimum_width = min_width;
		minimum_width = info.doc->GetLayoutMode() == LAYOUT_NORMAL ? min_width : CalculateEraMinWidth(cascade, info, min_width);
		maximum_width = max_width;
		minimum_height = min_height;

		packed.minmax_calculated = 1;
	}

	return OpStatus::OK;
}

/** Perform some ERA-specific size-shrinking. */

OP_STATUS
ReplacedContent::AdjustEraSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, LayoutCoord intrinsic_width, LayoutCoord intrinsic_height, LayoutCoord& horizontal_border_padding, BOOL keep_aspect_ratio)
{
	HTML_Element* html_element = cascade->html_element;

	UINT32 unbreakable_width = (UINT32) html_element->GetSpecialNumAttr(Markup::LAYOUTA_UNBREAKABLE_WIDTH, SpecialNs::NS_LAYOUT);

	OP_ASSERT(info.doc->GetLayoutMode() != LAYOUT_NORMAL);

	if (unbreakable_width)
	{
		const HTMLayoutProperties& props = *cascade->GetProps();
		UINT16 min_unbreakable_width = unbreakable_width & 0x0000ffff;
		UINT16 weak_unbreakable_width = unbreakable_width >> 16;

		if (props.max_width < min_unbreakable_width + weak_unbreakable_width)
		{
			LayoutCoord unadjusted_width = width;

			if (min_unbreakable_width <= props.max_width)
			{
				LayoutCoord min_width;
				LayoutCoord normal_min_width;
				LayoutCoord dummy;
				LayoutCoord border_padding_min_width;

				GetMinMaxWidth(min_width, normal_min_width, dummy);
				CalculateBorderPaddingMinWidth(cascade, border_padding_min_width);

				OP_ASSERT(min_width >= border_padding_min_width);
				OP_ASSERT(normal_min_width >= horizontal_border_padding);

				min_width -= border_padding_min_width;
				normal_min_width -= horizontal_border_padding;

				if (normal_min_width > min_width)
				{
					width = normal_min_width - min_width;
					width = min_width + (width * (props.max_width - LayoutCoord(min_unbreakable_width))) / LayoutCoord(weak_unbreakable_width);
					if (width < 0)
						width = LayoutCoord(0);
				}
				else
					width = min_width;

				if (ScaleBorderAndPadding())
				{
					if (props.padding_left > 2)
						horizontal_border_padding -= props.padding_left - LayoutCoord(((props.padding_left - 2) * (props.max_width - min_unbreakable_width)) / weak_unbreakable_width) - LayoutCoord(2);

					if (props.padding_right > 2)
						horizontal_border_padding -= LayoutCoord(props.padding_right - (int(props.padding_right - 2) * (props.max_width - min_unbreakable_width)) / weak_unbreakable_width - 2);

					if (props.border.left.width > 1)
						horizontal_border_padding -= LayoutCoord(props.border.left.width - (int(props.border.left.width - 1) * (props.max_width - min_unbreakable_width)) / weak_unbreakable_width - 1);

					if (props.border.right.width > 1)
						horizontal_border_padding -= LayoutCoord(props.border.right.width - (int(props.border.right.width - 1) * (props.max_width - min_unbreakable_width)) / weak_unbreakable_width - 1);
				}
			}
			else
				if (!IsForm() || html_element->GetInputType() == INPUT_IMAGE)
					width = LayoutCoord(0);

//			OP_ASSERT(width <= unadjusted_width); // Should we really let this code increase the width?

			if (keep_aspect_ratio && unadjusted_width != width && props.content_height == CONTENT_HEIGHT_AUTO)
				height = LayoutCoord(unadjusted_width ? (height * width) / unadjusted_width : LAYOUT_COORD_MAX);

			if (packed.size_determined && IsImage() && html_element->HasAttr(Markup::HA_USEMAP))
			{
				// check if image map is scaled because of ERA

				int orig_width = width;
				long orig_height = height;

				if (props.content_width > 0)
					orig_width = props.content_width;
				else
					if (props.content_width == CONTENT_WIDTH_AUTO)
					{
						orig_width = intrinsic_width;

						if (props.content_height > 0 && intrinsic_height > 0)
							orig_width = (orig_width * props.content_height) / intrinsic_height;
					}

				if (props.content_height > 0)
					orig_height = props.content_height;
				else
					if (props.content_height == CONTENT_HEIGHT_AUTO)
					{
						orig_height = intrinsic_height;

						if (props.content_width > 0 && intrinsic_width > 0)
							orig_height = (orig_height * props.content_width) / intrinsic_width;
					}

				if (orig_width != width && orig_width != 0)
				{
					int width_scale = ((1000*width)/orig_width);

					if (width_scale == 0)
						width_scale = 1;

					if (html_element->SetSpecialAttr(Markup::LAYOUTA_IMAGEMAP_WIDTH_SCALE, ITEM_TYPE_NUM, (void*)(INTPTR)width_scale, FALSE, SpecialNs::NS_LAYOUT) < 0)
						return OpStatus::ERR_NO_MEMORY;
				}

				if (orig_height != height && orig_height != 0)
				{
					int height_scale = ((1000*height)/orig_height);

					if (height_scale == 0)
						height_scale = 1;

					if (html_element->SetSpecialAttr(Markup::LAYOUTA_IMAGEMAP_HEIGHT_SCALE, ITEM_TYPE_NUM, (void*)(INTPTR)height_scale, FALSE, SpecialNs::NS_LAYOUT) < 0)
						return OpStatus::ERR_NO_MEMORY;
				}
			}
		}
	}

	return OpStatus::OK;
}

/** Calculate and return minimum width for ERA. */

LayoutCoord
ReplacedContent::CalculateEraMinWidth(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord normal_minimum_width) const
{
	PrefsCollectionDisplay::RenderingModes rendering_mode = PrefsCollectionDisplay::MSR;

	OP_ASSERT(info.doc->GetLayoutMode() != LAYOUT_NORMAL);

	switch (info.doc->GetLayoutMode())
	{
	case LAYOUT_SSR:
		rendering_mode = PrefsCollectionDisplay::SSR;
		break;

	case LAYOUT_CSSR:
		rendering_mode = PrefsCollectionDisplay::CSSR;
		break;

	case LAYOUT_AMSR:
		rendering_mode = PrefsCollectionDisplay::AMSR;
		break;

#ifdef TV_RENDERING
	case LAYOUT_TVR:
		rendering_mode = PrefsCollectionDisplay::TVR;
		break;
#endif // TV_RENDERING

	default:
		break;
	}

	LayoutCoord flexible_width = LayoutCoord(g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::FlexibleImageSizes)));
	LayoutCoord min_width = normal_minimum_width;

	if (flexible_width != FLEXIBLE_REPLACED_WIDTH_NO)
	{
		int minimum_percent = 0;
		LayoutCoord minimum_pixels = LAYOUT_COORD_MAX;
		const HTMLayoutProperties* props = cascade->GetProps();
		HTML_Element* html_element = cascade->html_element;

		if (IsForm())
		{
			InputType input_type = html_element->GetInputType();

			if (input_type == INPUT_RADIO ||
				input_type == INPUT_CHECKBOX)
				/* Never allow radiobuttons and checkboxes to shrink more.
				   They are already very small and will become
				   unrecognizable if smaller. */

				minimum_percent = 100;
			else
				if (placeholder->GetSelectContent())
				{
					/* 50% is quite little. We know that 100% is the
					   minimum to show the biggest item.  But that
					   is usually much wider than the average item. */

					if (flexible_width  == FLEXIBLE_REPLACED_WIDTH_SCALE_MIN)
						minimum_percent = 50;

					minimum_pixels = LayoutCoord(50);
				}
				else
					if (input_type == INPUT_BUTTON ||
						input_type == INPUT_SUBMIT ||
						input_type == INPUT_RESET)
					{
						// ~85% is the minimum until the text become truncated.

						if (flexible_width  == FLEXIBLE_REPLACED_WIDTH_SCALE_MIN)
							minimum_percent = 85;

						minimum_pixels = LayoutCoord(30);
					}
					else
						if (input_type == INPUT_FILE || html_element->Type() == Markup::HTE_TEXTAREA)
						{
							if (flexible_width  == FLEXIBLE_REPLACED_WIDTH_SCALE_MIN)
								minimum_percent = 50;

							minimum_pixels = LayoutCoord(50);
						}
						else
						{
							if (flexible_width  == FLEXIBLE_REPLACED_WIDTH_SCALE_MIN)
								minimum_percent = 20;

							minimum_pixels = LayoutCoord(20);
						}
		}
		else
			if (flexible_width == FLEXIBLE_REPLACED_WIDTH_SCALE_MIN)
				minimum_percent = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::MinimumImageWidth));

		LayoutCoord padding_border = props->GetNonPercentHorizontalBorderPadding();
		LayoutCoord min_padding_border;

		CalculateBorderPaddingMinWidth(cascade, min_padding_border);

		OP_ASSERT(normal_minimum_width >= padding_border);

		min_width = normal_minimum_width - padding_border;

		min_width = ((LayoutCoord(minimum_percent) * min_width) / LayoutCoord(100));

		if (IsForm() && min_width < minimum_pixels)
			// Don't let us shrink selectelements to less than minimum_pixels.
			min_width = MIN(normal_minimum_width, minimum_pixels);

		if (min_width < 1 && !AllowZeroSize())
			min_width = LayoutCoord(1);

		min_width += min_padding_border;
	}

	return min_width;
}

#ifdef MEDIA_HTML_SUPPORT

/* static */ MediaElement*
ReplacedContent::GetMediaElement(HTML_Element* html_element)
{
	if (!html_element->IsIncludedActual())
		html_element = html_element->ParentActual();

	return html_element->GetMediaElement();
}

#endif // MEDIA_HTML_SUPPORT

/** Signal that content may have changed. */

/* virtual */ void
ImageContent::SignalChange(FramesDocument* doc)
{
	HTML_Element* html_element = GetHtmlElement();

	if (html_element->HasRealSizeDependentCss() && CheckRealSizeDependentCSS(html_element, doc))
		// css properties changed, so we need to reflow

		html_element->MarkExtraDirty(doc);
	else
		if (!packed.size_determined && doc->GetShowImages())
		{
			if (HEListElm* he = html_element->GetHEListElm(FALSE))
			{
				Image img = he->GetImage();

				if (img.Width() && img.Height())
					// Size can be determined; reflow

					placeholder->MarkDirty(doc);
			}
		}
}

#define ImageStr UNI_L("Image")

/** Paint image box on screen. */

/* virtual */ OP_STATUS
ImageContent::Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area)
{
	OP_NEW_DBG("ImageContent::Paint", "img");

	HTML_Element* html_element = layout_props->html_element;
	const HTMLayoutProperties& props = layout_props->GetCascadingProperties();
	LayoutCoord left = props.padding_left + LayoutCoord(props.border.left.width);
	LayoutCoord top = props.padding_top + LayoutCoord(props.border.top.width);
	LayoutCoord inner_height = GetHeight() - top - (props.padding_bottom + LayoutCoord(props.border.bottom.width));
	LayoutCoord inner_width = GetWidth() - left - (props.padding_right + LayoutCoord(props.border.right.width));

	Image img = GetImage(layout_props);
	HEListElm* hle = html_element->GetHEListElm(FALSE); // Skin images don't have an hle.
	URL url = html_element->GetImageURL(TRUE, doc->GetLogicalDocument());

	if (packed.size_determined && !img.IsEmpty() && doc->GetShowImages()
#ifdef HAS_ATVEF_SUPPORT
		|| img.IsAtvefImage()
#endif
		)
    {
		// Find the image's fitted height and width (before taking partial loading into account)
		OpRect fit;
		if (!CalculateObjectFitPosition(layout_props, fit))
			// object-fit failed, use cascaded values
			fit = OpRect(left, top, inner_width, inner_height);

		AffinePos doc_pos = visual_device->GetCTM();
		doc_pos.AppendTranslation(fit.x, fit.y);

		if (hle)
		{
			// We need to record the visual area of the image in doc.
			// Unfortunately we don't know exactly what part of the image
			// will be visible here. We really need to take clipping from
			// parent boxes into account which we do not so the area will
			// be too large. That will result in unncessary repaints during
			// image load and possibly that the image isn't handled correctly
			// in the image cache. See CORE-43833 for some possible effects.
			doc->SetImagePosition(html_element, doc_pos, fit.width, fit.height,
								  FALSE);//FIXME:OOM Can fail

#ifdef HAS_ATVEF_SUPPORT
			if (img.IsAtvefImage())
			{
				packed.use_alternative_text = 0;
				visual_device->ImageOut(img, OpRect(0,0,0,0), OpRect(left, top, inner_width, inner_height), hle);
				return OpStatus::OK;
			}
#endif // HAS_ATVEF_SUPPORT
		}

		OpRect src, dst;
		if (GetImageVisible(layout_props) && SetDisplayRects(img, src, dst, fit))
		{
			packed.use_alternative_text = 0;

#ifdef SUPPORT_VISUAL_ADBLOCK
			// we need to check if this image should have any special effects because it's currently
			// being marked as blocked

			if (doc->GetWindow()->GetContentBlockEditMode() && doc->GetIsURLBlocked(url))
			{
				RETURN_IF_MEMORY_ERROR(visual_device->ImageOutEffect(img, src, dst, Image::EFFECT_BLEND, 25, hle));

				int width = img.Width();
				int height = ADBLOCK_EFFECT_BOX_HEIGHT;

				if (m_blocked_content == NULL)
				{
					OpString text;
					RETURN_IF_ERROR(g_languageManager->GetString(Str::S_BLOCKED_IMAGE_TEXT, text));

					m_blocked_content = OP_NEW(Image, ());

					if (!m_blocked_content)
						return OpStatus::ERR_NO_MEMORY;

					// create a transparent bitmap to overlay the effect rendered image

					OpBitmap *bitmap;
					OP_STATUS status = OpBitmap::Create(&bitmap, width, height, FALSE, FALSE, 0, 0, TRUE);

					if (OpStatus::IsSuccess(status))
					{
						OpPainter* painter = bitmap->GetPainter();

						if (painter)
						{
							OpFont* old_font = painter->GetFont();

							FontAtt font_att;
							font_att.SetFaceName(styleManager->GetFontManager()->GetGenericFontName(GENERIC_FONT_SANSSERIF));
							font_att.SetSize(ADBLOCK_FONT_HEIGHT);
							font_att.SetWeight(7);
							OpFont* font = g_font_cache ? g_font_cache->GetFont(font_att, 100) : NULL;

							if (font)
								painter->SetFont(font);

							int x = width / 2;
							int y = (height / 2) - 8;
							UINT32 text_width = 0;
							OpPoint font_pos;

							if (font)
								text_width = font->StringWidth(text.CStr(), text.Length(), painter, 0);

							x -= text_width / 2;
							if (x < 0)
								x = 0;

							font_pos.Set(x, y);

							painter->SetColor(ADBLOCK_BG_BOX_R, ADBLOCK_BG_BOX_G, ADBLOCK_BG_BOX_B);
							painter->FillRect(OpRect(0, 0, width, ADBLOCK_EFFECT_BOX_HEIGHT));

							painter->SetColor(0, 0, 0);
							painter->DrawString(font_pos, text.CStr(), text.Length(), 0);

							painter->DrawLine(OpPoint(0, 0), OpPoint(width, 0), 1);
							painter->DrawLine(OpPoint(0, ADBLOCK_EFFECT_BOX_HEIGHT - 1), OpPoint(width, ADBLOCK_EFFECT_BOX_HEIGHT - 1), 1);

							if (old_font)
								painter->SetFont(old_font);

							g_font_cache->ReleaseFont(font);

							bitmap->ReleasePainter();
							*m_blocked_content = imgManager->GetImage(bitmap);
						}
						else
						{
							OP_DELETE(bitmap);
							OP_DELETE(m_blocked_content);
							m_blocked_content = NULL;
						}
					}
					else
					{
						OP_DELETE(m_blocked_content);
						m_blocked_content = NULL;

						RETURN_IF_MEMORY_ERROR(status);
					}
				}

				if (m_blocked_content)
				{
					int img_h = img.Height();

					dst.y = (img_h / 2) - (ADBLOCK_EFFECT_BOX_HEIGHT / 2);

					if (dst.y < 0)
						dst.y = 0;

					return visual_device->ImageOut(*m_blocked_content, src, dst, hle);
				}

				return OpStatus::OK;
			}
			else
#endif // SUPPORT_VISUAL_ADBLOCK
				return visual_device->ImageOut(img, src, dst, hle, props.image_rendering);
		}
	}

	if (img.IsFailed() && inner_width > 10 && inner_height > 10)
		packed.use_alternative_text = 1;

	if (packed.use_alternative_text || (doc->GetShowImages() && html_element->Type() == Markup::HTE_OBJECT && !img.ImageDecoded() && html_element->HasAttr(Markup::HA_STANDBY)))
	{
		OpString translated_alt_text;
		const uni_char* alt_text = packed.use_alternative_text ? html_element->GetIMG_Alt() : html_element->GetStringAttr(Markup::HA_STANDBY);

		if (!alt_text)
		{
			OP_STATUS rc = g_languageManager->GetString(Str::SI_DEFAULT_IMG_ALT_TEXT, translated_alt_text);
			OpStatus::Ignore(rc);

			alt_text = translated_alt_text.CStr();

			if (!alt_text)
				alt_text = ImageStr;
		}

		if (props.font_color != visual_device->GetColor())
			visual_device->SetColor(props.font_color);

		visual_device->SetBgColor(DEFAULT_BORDER_COLOR);

		visual_device->ImageAltOut(left,
								   top,
								   inner_width,
								   inner_height,
								   alt_text,
								   uni_strlen(alt_text),
								   (const_cast<const HTMLayoutProperties&>(props)).current_script);
	}
	return OpStatus::OK;
}

/** Helper method to calculate size of alternate text for images and applets.
	Returns TRUE if size is calculated, FALSE if no text or predefined size is too small. */

static BOOL
CalculateAlternateTextSize(LayoutInfo& info, const HTMLayoutProperties& props, HTML_Element* html_element, BOOL calculate_width, BOOL calculate_height, LayoutCoord& width, LayoutCoord& height)
{
	BOOL use_alternative_text = FALSE;
	OpString translated_alt_text;
	const uni_char* alt_txt = html_element->GetStringAttr(Markup::HA_ALT);

	if (!alt_txt)
	{
		TRAPD(rc,g_languageManager->GetStringL(Str::SI_DEFAULT_IMG_ALT_TEXT, translated_alt_text));
		alt_txt = translated_alt_text.CStr();

		if (!alt_txt)
			alt_txt = ImageStr;
	}

	if (alt_txt)
	{
		// Use alternative text for images/applets that are greater than 10x10 pixels

		if ((calculate_width || props.content_width > 10) &&
			(calculate_height || props.content_height > 10))
		{
			// Need alternative text to determine the element size
			// (for images: permanent if loading fails or temporary until image header is loaded)

			int lines = 1;

			use_alternative_text = TRUE;

			if (calculate_width && *alt_txt)
				width = LayoutCoord(2); // Border takes 2 pixels

			if (calculate_width)
			{
				int oldfont = info.visual_device->GetCurrentFontNumber();

				int tot_len = uni_strlen(alt_txt);
				const uni_char* tmp_txt = alt_txt;

				for (;;)
				{
					WordInfo wi;
					wi.Reset();

					int this_word_start_index = tmp_txt - alt_txt;

					int len_left = tot_len - this_word_start_index;

					info.visual_device->SetFont(oldfont);
					FontSupportInfo fsi(oldfont);
					wi.SetFontNumber(oldfont);

					if (!GetNextTextFragment(tmp_txt, len_left, wi, CSS_VALUE_normal, TRUE, TRUE, fsi, info.doc, props.current_script))
						break;

					info.visual_device->SetFont(wi.GetFontNumber());

					width += LayoutCoord(info.visual_device->GetTxtExtent(alt_txt + this_word_start_index, wi.GetLength()));

					if (wi.HasTrailingWhitespace() || wi.HasEndingNewline())
						width += LayoutCoord(info.visual_device->GetTxtExtent(UNI_L(" "), 1));
				}

				info.visual_device->SetFont(oldfont);
			}

			if (calculate_height)
				height = LayoutCoord(lines * (props.parent_font_ascent + props.parent_font_descent + props.parent_font_internal_leading));
		}
		else
		{
			// No alternative text so use a default value for size

			if (calculate_width)
				width = LayoutCoord(20);

			if (calculate_height)
				height = LayoutCoord(20);
		}
	}

	if (props.max_paragraph_width > 0)
		width = LayoutCoord(MIN(props.max_paragraph_width, width));

	return use_alternative_text;
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
ImageContent::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	HTML_Element* html_element = cascade->html_element;

	if (html_element->HasAttr(Markup::HA_USEMAP))
	{
		if (html_element->HasSpecialAttr(Markup::LAYOUTA_IMAGEMAP_WIDTH_SCALE, SpecialNs::NS_LAYOUT))
		{
#ifdef DEBUG_ENABLE_OPASSERT
			int res =
#endif // DEBUG_ENABLE_OPASSERT
				html_element->SetSpecialAttr(Markup::LAYOUTA_IMAGEMAP_WIDTH_SCALE, ITEM_TYPE_NUM, (void*)(INTPTR)1000, FALSE, SpecialNs::NS_LAYOUT);
			OP_ASSERT(res >= 0 || !"SetSpecialAttr should not fail when we have first checked for the presence of the attribute");
		}

		if (html_element->HasSpecialAttr(Markup::LAYOUTA_IMAGEMAP_HEIGHT_SCALE, SpecialNs::NS_LAYOUT))
		{
#ifdef DEBUG_ENABLE_OPASSERT
			int res =
#endif // DEBUG_ENABLE_OPASSERT
				html_element->SetSpecialAttr(Markup::LAYOUTA_IMAGEMAP_HEIGHT_SCALE, ITEM_TYPE_NUM, (void*)(INTPTR)1000, FALSE, SpecialNs::NS_LAYOUT);
			OP_ASSERT(res >= 0 || !"SetSpecialAttr should not fail when we have first checked for the presence of the attribute");
		}
	}

	return ReplacedContent::ComputeSize(cascade, info);
}

/** Get the image associated with this box. */

Image
ImageContent::GetImage(LayoutProperties* layout_props)
{
	HTML_Element* html_element = layout_props->html_element;
	Image img;

	if (HEListElm* hle = html_element->GetHEListElm(FALSE))
	{
		img = hle->GetImage();
	}
#ifdef SKIN_SUPPORT
	else
		// Generated content might be skin image.

		if (html_element->GetInserted() == HE_INSERTED_BY_LAYOUT)
		{
			img = GetSkinImage();
		}
#endif // SKIN_SUPPORT

	return img;
}

/** Check whether the image is visible, taking skins into account. */

BOOL
ImageContent::GetImageVisible(LayoutProperties* layout_props)
{
	HTML_Element* html_element = layout_props->html_element;
	BOOL visible = TRUE;

	if (HEListElm* hle = html_element->GetHEListElm(FALSE))
	{
		visible = hle->GetImageVisible();
	}
#ifdef SKIN_SUPPORT
	else
		// Generated content might be skin image (including list markers).
		if (html_element->GetInserted() == HE_INSERTED_BY_LAYOUT)
		{
			Image img = GetSkinImage();

			if (img.IsEmpty())
				visible = FALSE;
		}
#endif // SKIN_SUPPORT

	return visible;
}
/** Calculate intrinsic size. */

/* virtual */ OP_STATUS
ImageContent::CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio)
{
	HTML_Element* html_element = cascade->html_element;
	BOOL use_alt_text_size = TRUE;

	if (info.doc->GetShowImages())
	{
		Image img = GetImage(cascade);

		// Need width and/or height
		if (img.Width() == 0 && img.Height() == 0)
			img.PeekImageDimension();

		if (img.Width() && img.Height())
		{
			width = LayoutCoord(img.Width());
			height = LayoutCoord(img.Height());
			ratio = CalculateIntrinsicRatio(width, height);

			if (info.workplace->GetScaleAbsoluteWidthAndPos() && info.doc->GetLayoutMode() != LAYOUT_NORMAL)
			{
				LayoutCoord view_width = info.workplace->GetAbsScaleViewWidth();

				width = (width * view_width) / info.workplace->GetNormalEraWidth();
				height = (height * view_width) / info.workplace->GetNormalEraWidth();
			}

			if (info.doc->GetLayoutMode() != LAYOUT_NORMAL &&
				cascade->container &&
				!cascade->GetProps()->WidthIsPercent() &&
				!packed.size_determined)
			{
				if (html_element->HasSpecialAttr(Markup::LAYOUTA_UNBREAKABLE_WIDTH, SpecialNs::NS_LAYOUT))
				{
#ifdef DEBUG_ENABLE_OPASSERT
					int res =
#endif // DEBUG_ENABLE_OPASSERT
						html_element->SetSpecialAttr(Markup::LAYOUTA_UNBREAKABLE_WIDTH, ITEM_TYPE_NUM, (void*)(INTPTR)0, FALSE, SpecialNs::NS_LAYOUT);
					OP_ASSERT(res >= 0 || !"SetSpecialAttr should not fail when we have first checked for the presence of the attribute");
				}

				cascade->container->ImageSizeReduced();
			}

			packed.size_determined = TRUE;
			use_alt_text_size = FALSE;
		}
	}

	if (use_alt_text_size)
		// Intrinsic image size unavailble; use ALT text instead.

		packed.use_alternative_text = CalculateAlternateTextSize(info, *cascade->GetProps(), html_element, TRUE, TRUE, width, height);

	return OpStatus::OK;
}

/* virtual */ BOOL
ImageContent::CalculateObjectFit(LayoutProperties* cascade, OpRect& dst)
{
	// FIXME: I would like to use CalculateIntrinsicSize here, but it needs LayoutInfo...
	// mstensho speculates that it may be possible to use FramesDocument in CalculateIntrinsicSize
	// and thus excise LayoutInfo from the argument list.
	//int ratio;
	//CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, short& width, long& height, int &ratio);

	Image img = GetImage(cascade);
	HTMLayoutProperties& props = *cascade->GetProps();
	OpRect inner = GetInnerRect(props);

	if (img.Width() && img.Height())
		// Use 'fill' for 'auto'
		props.object_fit_position.CalculateObjectFit(CSS_VALUE_fill, OpRect(0,0, img.Width(), img.Height()), inner, dst);
	else
	{
		dst.width = dst.height = 0;
		return FALSE;
	}

	return TRUE;
}

/*virtual */ BOOL
ImageContent::CalculateObjectPosition(LayoutProperties* cascade, OpRect& fit)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	OpRect inner = GetInnerRect(props);
	props.object_fit_position.CalculateObjectPosition(inner, fit);
	return TRUE;
}

/** Remove form, plugin and iframe objects */

/* virtual */ void
ImageContent::Disable(FramesDocument* doc)
{
	GetHtmlElement()->UndisplayImage(doc, FALSE);
}

#ifdef SKIN_SUPPORT
Image
ImageContent::GetSkinImage()
{
	HTML_Element* html_element = GetHtmlElement();
	OP_ASSERT(html_element->GetInserted() == HE_INSERTED_BY_LAYOUT);

	const uni_char* skin_name = html_element->GetSpecialStringAttr(Markup::LAYOUTA_SKIN_ELM, SpecialNs::NS_LAYOUT);

	if (skin_name)
	{
		char name8[120]; /* ARRAY OK 2008-02-05 mstensho */
		uni_cstrlcpy(name8, skin_name, ARRAY_SIZE(name8));
		OpSkinElement* skin_elm = g_skin_manager->GetSkinElement(name8);
		if (skin_elm)
			return skin_elm->GetImage(0);
	}
	return Image();
}
#endif // SKIN_SUPPORT

#ifdef SVG_SUPPORT

/** Paint image box on screen. */

/* virtual */ OP_STATUS
SVGContent::Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area)
{
	HTML_Element* html_element = layout_props->html_element;
	const HTMLayoutProperties& props = layout_props->GetCascadingProperties();

	short left = props.padding_left + props.border.left.width;
	short top = props.padding_top + props.border.top.width;

	if(!doc->GetShowImages())
		return OpStatus::OK;

	OP_STATUS status = OpStatus::OK;
	if (SVGImage* svg_image = GetSVGImage(doc, html_element))
	{
		visual_device->Translate(left, top);

		svg_image->SetDocumentPos(visual_device->GetCTM());
		status = svg_image->OnPaint(visual_device, layout_props, area);

		visual_device->Translate(-left, -top);
	}

	return status;
}

/** Calculate intrinsic size. */

/* virtual */ OP_STATUS
SVGContent::CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio)
{
	width = LayoutCoord(0);
	height = LayoutCoord(0);

	if (info.doc->GetShowImages())
	{
		if (SVGImage *image = GetSVGImage(info.doc, cascade->html_element))
		{
			RETURN_IF_ERROR(image->GetIntrinsicSize(cascade, width, height, ratio));

			if (width < 0)
				width = (ratio != 0) ? LayoutCoord(0) : LayoutCoord(300);

			if (height < 0)
				height = (ratio != 0) ? LayoutCoord(0) : LayoutCoord(150);
		}
	}

	return OpStatus::OK;
}

/** Update size of content.

	@return OpBoolean::IS_TRUE if size changed, OpBoolean::IS_FALSE if not, OpStatus::ERR_NO_MEMORY on OOM. */

/* virtual */ OP_BOOLEAN
SVGContent::SetNewSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord width, LayoutCoord height,
					   LayoutCoord modified_horizontal_border_padding)
{
	OP_BOOLEAN size_changed = ReplacedContent::SetNewSize(cascade, info, width, height, modified_horizontal_border_padding);

	if (size_changed == OpBoolean::IS_TRUE)
	{
		if (SVGImage* svg_image = GetSVGImage(info.doc, cascade->html_element))
		{
			const HTMLayoutProperties& props = cascade->GetCascadingProperties();

			LayoutCoord content_width = width - LayoutCoord(props.border.left.width + props.border.right.width) -
				(props.padding_left + props.padding_right);

			LayoutCoord content_height = height - LayoutCoord(props.border.top.width + props.border.bottom.width) -
				(props.padding_top + props.padding_bottom);

			svg_image->OnContentSize(cascade, content_width, content_height);
		}
	}

	return size_changed;
}

IFrameContent *
SVGContent::GetIFrame(LayoutInfo& info)
{
	FramesDocElm *frame = info.doc->GetDocManager()->GetFrame();
	if (frame)
	{
		Box *box = frame->GetHtmlElement()->GetLayoutBox();
		if (box)
			return (IFrameContent *) box->GetContent();
	}
	return NULL;
}

/** Compute size of content and return TRUE if size is changed.

	Also retrieve object-fit and object-position values from any enclosing IFrame. */

/* virtual */ OP_BOOLEAN
SVGContent::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	if (IFrameContent *iframe = GetIFrame(info))
		object_fit_position = iframe->GetObjectFitPosition();
	else
	{
		// If this SVG does not belong to an iframe, we can use the cascaded props directly.
		const HTMLayoutProperties& props = *cascade->GetProps();
		object_fit_position = props.object_fit_position;
	}
	return ReplacedContent::ComputeSize(cascade, info);
}

/* virtual */ void
SVGContent::SignalChange(FramesDocument* doc)
{
	// An image has been loaded

	if (SVGImage* svg_image = GetSVGImage(doc, GetHtmlElement()))
		svg_image->OnSignalChange();
}

/* virtual */ void
SVGContent::Disable(FramesDocument* doc)
{
	if (SVGImage* svg_image = GetSVGImage(doc, GetHtmlElement()))
		svg_image->OnDisable();
}

BOOL
SVGContent::IsTransparentAt(FramesDocument *doc, LayoutProperties *cascade, LayoutCoord x, LayoutCoord y) const
{
	OP_ASSERT(cascade->html_element && cascade->html_element == GetHtmlElement());

	HTML_Element* elm = cascade->html_element;

	/* Dummy values, not used */
	HTML_Element* event_target = NULL;
	int offset_x = 0, offset_y = 0;

	/* Note: In the case where an Markup::SVGE_SVG element is the event_target we will call FindSVGElement
	   again from HTML_Document::MouseAction, which may be expensive. */
	return g_svg_manager->FindSVGElement(elm, doc, x, y, &event_target, offset_x, offset_y) != OpBoolean::IS_TRUE;
}

/* virtual */ LAYST
SVGContent::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (SVGImage *image = GetSVGImage(info.doc, cascade->html_element))
		if (OpStatus::IsMemoryError(image->OnReflow(cascade)))
			return LAYOUT_OUT_OF_MEMORY;

	if (first_child && first_child != cascade->html_element)
		return placeholder->LayoutChildren(cascade, info, first_child, start_position);
	else
		return placeholder->LayoutChildren(cascade, info, NULL, LayoutCoord(0));
}

SVGImage*
SVGContent::GetSVGImage(FramesDocument* doc, HTML_Element* html_element)
{
	return g_svg_manager->GetSVGImage(doc->GetLogicalDocument(), html_element);
}

#endif // SVG_SUPPORT

#ifdef MEDIA_SUPPORT
/* virtual */ BOOL
MediaContent::CalculateObjectFit(LayoutProperties* cascade, OpRect& dst)
{
	Media* media_elm = GetMedia();
	if (media_elm && media_elm->IsImageType())
	{
		const HTMLayoutProperties& props = *cascade->GetProps();
		OpRect inner = GetInnerRect(props);
		// Use 'contain' for 'auto'
		props.object_fit_position.CalculateObjectFit(CSS_VALUE_contain, OpRect(0,0, media_elm->GetIntrinsicWidth(), media_elm->GetIntrinsicHeight()), inner, dst);
	}
	else
	{
		dst.width = dst.height = 0;
		return FALSE;
	}

	return TRUE;
}

/*virtual */ BOOL
MediaContent::CalculateObjectPosition(LayoutProperties* cascade, OpRect& fit)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	OpRect inner = GetInnerRect(props);
	props.object_fit_position.CalculateObjectPosition(inner, fit);
	return TRUE;
}

Media*
MediaContent::GetMedia() const
{
	HTML_Element* html_element = GetHtmlElement();
	if (html_element->IsMatchingType(Markup::MEDE_VIDEO_DISPLAY, NS_HTML))
		html_element = html_element->ParentActual();

	return html_element->GetMedia();
}

/** Paint video box on screen. */

/* virtual */ OP_STATUS
MediaContent::Paint(LayoutProperties* layout_props, FramesDocument* doc,
					VisualDevice* visual_device, const RECT& area)
{
	Media* media_elm = GetMedia();

	if (media_elm && (!media_elm->IsImageType() || doc->GetShowImages()))
	{
		const HTMLayoutProperties& props = layout_props->GetCascadingProperties();
		OpRect inner = GetInnerRect(props);
		OpRect video;
		CalculateObjectFitPosition(layout_props, video);
		return media_elm->Paint(visual_device, video, inner);
	}

	return OpStatus::OK;
}

/** Calculate intrinsic size. */

/* virtual */ OP_STATUS
MediaContent::CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio)
{
	width = LayoutCoord(0);
	height = LayoutCoord(0);
	ratio = 0;

	Media* media_elm = GetMedia();
	if (media_elm)
	{
		// 0 if no video data is available
		width = LayoutCoord(media_elm->GetIntrinsicWidth());
		height = LayoutCoord(media_elm->GetIntrinsicHeight());
	}

	if (width && height)
	{
		ratio = CalculateIntrinsicRatio(width, height);
		packed.size_determined = 1;
	}
	else
	{
		width = LayoutCoord(300);
		height = LayoutCoord(150);
	}

	return OpStatus::OK;
}
#endif // MEDIA_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
/* virtual */ OP_STATUS
VideoContent::Paint(LayoutProperties* layout_props, FramesDocument* doc,
					VisualDevice* visual_device, const RECT& area)
{
	MediaElement* media_elm = GetMediaElement();

	if (media_elm && (!media_elm->IsImageType() || doc->GetShowImages()))
	{
		if (media_elm->DisplayPoster())
		{
			Image img;
			LayoutProperties* parent_props = layout_props->Pred();

			HTML_Element* html_element = parent_props->html_element;

			HEListElm* hle = html_element->GetHEListElmForInline(VIDEO_POSTER_INLINE);
			if (hle)
				img = hle->GetImage();

			if (packed.size_determined && !img.IsEmpty())
			{
				OpRect fit;
				const HTMLayoutProperties& props = layout_props->GetCascadingProperties();
				OpRect inner = GetInnerRect(props);

				// The poster's object-fit must be calculated as if for ImageContent
				props.object_fit_position.CalculateObjectFit(CSS_VALUE_contain, OpRect(0,0, img.Width(), img.Height()), inner, fit);
				props.object_fit_position.CalculateObjectPosition(inner, fit);

				AffinePos doc_pos = visual_device->GetCTM();
				doc_pos.AppendTranslation(inner.x, inner.y);

				doc->SetImagePosition(html_element, doc_pos, fit.width, fit.height,
									  FALSE, hle);//FIXME:OOM Can fail

				OpRect src, dst;
				if (SetDisplayRects(img, src, dst, fit))
					RETURN_IF_ERROR(visual_device->ImageOut(img, src, dst, hle, props.image_rendering));
			}
		}

		return MediaContent::Paint(layout_props, doc, visual_device, area);
	}

	return OpStatus::OK;
}

/** Calculate intrinsic size. */

/* virtual */ OP_STATUS
VideoContent::CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio)
{
	OP_STATUS result = OpStatus::OK;

	MediaElement *media_element = GetMediaElement();

	if (media_element && media_element->DisplayPoster())
	{
		LayoutProperties* parent_cascade = cascade->Pred();

		if (HEListElm* hle = parent_cascade->html_element->GetHEListElmForInline(VIDEO_POSTER_INLINE))
		{
			Image img = hle->GetImage();
			width = LayoutCoord(img.Width());
			height = LayoutCoord(img.Height());
		}

		if (width && height)
		{
			ratio = CalculateIntrinsicRatio(width, height);
			packed.size_determined = 1;
		}
		else
		{
			width = LayoutCoord(300);
			height = LayoutCoord(150);
		}
	}
	else
		result = MediaContent::CalculateIntrinsicSize(cascade, info, width, height, ratio);

	return result;
}

/** Update size of content.

	@return OpBoolean::IS_TRUE if size changed, OpBoolean::IS_FALSE if not, OpStatus::ERR_NO_MEMORY on OOM. */

/* virtual */ OP_BOOLEAN
VideoContent::SetNewSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord width, LayoutCoord height,
						 LayoutCoord modified_horizontal_border_padding)
{
	OP_BOOLEAN size_changed = MediaContent::SetNewSize(cascade, info, width, height, modified_horizontal_border_padding);

	if (size_changed == OpBoolean::IS_TRUE)
		if (MediaElement* media_element = GetMediaElement())
			/* This assumes border + padding == 0 for the video
			   display. Should be accurate with the wrapping scheme. */

			if (media_element->OnContentSize(width, height))
			{
				// If any cues existed they now need to be reflowed.

				video_packed.reflow_cues = 1;
				info.workplace->SetReflowElement(cascade->html_element);
			}

	return size_changed;
}

/** Mark dirty if poster size has changed. */

/* virtual */ void
VideoContent::SignalChange(FramesDocument* doc)
{
	MediaElement* media_elm = GetMediaElement();

	if (!packed.size_determined && media_elm->IsImageType() && doc->GetShowImages())
	{
		HTML_Element* video_element = GetHtmlElement()->ParentActual();

		if (HEListElm* he = video_element->GetHEListElmForInline(VIDEO_POSTER_INLINE))
		{
			Image img = he->GetImage();

			if (img.Width() && img.Height())
				// Size can be determined; reflow

				placeholder->MarkDirty(doc);
		}
	}

	if (video_packed.reflow_cues)
	{
		media_elm->MarkCuesDirty();
		video_packed.reflow_cues = 0;
	}
}

/** Paint video controls. */

/* virtual */ OP_STATUS
MediaControlsContent::Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area)
{
	if (MediaElement* media_elm = GetMediaElement(layout_props->html_element))
		media_elm->PaintControls(visual_device, GetInnerRect(*layout_props->GetProps()));

	return OpStatus::OK;
}

/** Calculate intrinsic size. */

/* virtual */ OP_STATUS
MediaControlsContent::CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio)
{
	if (MediaElement *media_element = GetMediaElement(cascade->html_element))
	{
		int video_width, video_height;
		media_element->GetContentSize(video_width, video_height);

		width = LayoutCoord(video_width);
		height = LayoutCoord(video_height);
	}
	return OpStatus::OK;
}

/** Apply min-width property constraints on min/max values. */

/* virtual */ void
VideoWrapperContainer::ApplyMinWidthProperty(const HTMLayoutProperties& props, LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width)
{
	if (wrapper_packed.width_is_auto)
		ShrinkToFitContainer::ApplyMinWidthProperty(props, min_width, normal_min_width, max_width);
	else
		BlockContainer::ApplyMinWidthProperty(props, min_width, normal_min_width, max_width);
}

/** Clear min/max width. */

/* virtual */ void
VideoWrapperContainer::ClearMinMaxWidth()
{
	if (wrapper_packed.width_is_auto)
		ShrinkToFitContainer::ClearMinMaxWidth();
	else
		BlockContainer::ClearMinMaxWidth();
}

/** Update screen. */

/* virtual */ void
VideoWrapperContainer::UpdateScreen(LayoutInfo& info)
{
	if (wrapper_packed.width_is_auto)
		ShrinkToFitContainer::UpdateScreen(info);
	else
		BlockContainer::UpdateScreen(info);
}

/** Signal that content may have changed. */

/* virtual */ void
VideoWrapperContainer::SignalChange(FramesDocument* doc)
{
	BOOL video_display_dirty = FALSE;
	for (HTML_Element* child = GetHtmlElement()->FirstChild(); child; child = child->Suc())
		if (child->GetInserted() == HE_INSERTED_BY_LAYOUT && child->GetNsType() == NS_HTML)
			switch (child->Type())
			{
			case Markup::MEDE_VIDEO_DISPLAY:
				if (Box* box = child->GetLayoutBox())
					box->SignalChange(doc);

				video_display_dirty = child->IsDirty();
				break;

			case Markup::MEDE_VIDEO_CONTROLS:
				if (video_display_dirty)
					child->MarkDirty(doc);
				break;
			}

	if (wrapper_packed.width_is_auto)
		ShrinkToFitContainer::SignalChange(doc);
	else
		BlockContainer::SignalChange(doc);
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
VideoWrapperContainer::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	wrapper_packed.width_is_auto = cascade->GetProps()->content_width == CONTENT_WIDTH_AUTO && !cascade->flexbox;

	if (wrapper_packed.width_is_auto)
		return ShrinkToFitContainer::ComputeSize(cascade, info);
	else
		return BlockContainer::ComputeSize(cascade, info);
}

/** Traverse box. */

/* virtual */ void
VideoWrapperContainer::Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	if (wrapper_packed.width_is_auto)
		ShrinkToFitContainer::Traverse(traversal_object, layout_props);
	else
		BlockContainer::Traverse(traversal_object, layout_props);
}

/** Update margins and bounding box. See declaration in header file for more information. */

/* virtual */ void
VideoWrapperContainer::UpdateBottomMargins(const LayoutInfo& info, const VerticalMargin* bottom_margin, AbsoluteBoundingBox* child_bounding_box, BOOL has_inflow_content)
{
	LayoutCoord prev_reflow_position = reflow_state->reflow_position;
	LayoutCoord prev_min_reflow_position = reflow_state->min_reflow_position;

	ShrinkToFitContainer::UpdateBottomMargins(info, bottom_margin, child_bounding_box, has_inflow_content);

	reflow_state->reflow_position = prev_reflow_position;
	reflow_state->min_reflow_position = prev_min_reflow_position;
}

/** Update height of container. */

/* virtual */ void
VideoWrapperContainer::UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height)
{
	if (!initial_content_height)
		/* Acquire height from the video display element. */

		for (HTML_Element* child = GetHtmlElement()->FirstChild(); child; child = child->Suc())
			if (child->GetInserted() == HE_INSERTED_BY_LAYOUT &&
				child->IsMatchingType(Markup::MEDE_VIDEO_DISPLAY, NS_HTML))
				if (Box* video_display_box = child->GetLayoutBox())
				{
					Content* video_content = video_display_box->GetContent();
					content_height = video_content->GetHeight();
					min_content_height = video_content->GetMinHeight();
				}

	ShrinkToFitContainer::UpdateHeight(info, cascade, content_height, min_content_height, initial_content_height);
}

#endif // MEDIA_HTML_SUPPORT


#ifdef CANVAS_SUPPORT

/** Calculate intrinsic size. */

/* virtual */ OP_STATUS
CanvasContent::CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio)
{
	if (info.doc->GetShowImages())
	{
		Canvas* canvas = GetCanvas(cascade->html_element);

		if (canvas)
		{
			width = LayoutCoord(canvas->GetWidth(cascade->html_element));
			height = LayoutCoord(canvas->GetHeight(cascade->html_element));
		}
		else
		{
			width = LayoutCoord(300);
			height = LayoutCoord(150);
		}
	}
	else
	{
		width = LayoutCoord(0);
		height = LayoutCoord(0);
	}

	return OpStatus::OK;
}

Canvas*
CanvasContent::GetCanvas(HTML_Element* content_elm)
{
	return (Canvas*)content_elm->GetSpecialAttr(Markup::VEGAA_CANVAS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_OGP);
}

/** Paint image box on screen. */

/* virtual */ OP_STATUS
CanvasContent::Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area)
{
	BOOL show_image = doc->GetShowImages();
	if (!show_image)
		return OpStatus::OK;

	HTML_Element* html_element = layout_props->html_element;

	if (Canvas* canvas = GetCanvas(html_element))
	{
		const HTMLayoutProperties& props = layout_props->GetCascadingProperties();
		short left         = props.padding_left + props.border.left.width;
		short top          = props.padding_top + props.border.top.width;
		short image_height = GetHeight() - top - (props.padding_bottom + props.border.bottom.width);
		short image_width  = GetWidth() - left - (props.padding_right + props.border.right.width);

		visual_device->Translate(left, top);

		canvas->Paint(visual_device, image_width, image_height, props.image_rendering);

		visual_device->Translate(-left, -top);
	}

	return OpStatus::OK;
}

# ifdef DOM_FULLSCREEN_MODE
OP_STATUS
CanvasContent::PaintFullscreen(FramesDocument* doc, VisualDevice* visual_device, HTML_Element* elm, const RECT& area)
{
	/* Create the cascade to get the properties. */

	Head prop_list;
	LayoutProperties* props = LayoutProperties::CreateCascade(elm, prop_list, doc->GetHLDocProfile());
	if (!props)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret = OpStatus::ERR;
	if (props->GetCascadingProperties().visibility == CSS_VALUE_visible)
	{
		/* Paint background. */

		OP_ASSERT(elm->GetLayoutBox() && elm->GetLayoutBox()->IsVerticalBox());
		static_cast<VerticalBox*>(elm->GetLayoutBox())->PaintBgAndBorder(props, doc, visual_device);

		/* Paint the Canvas. */

		ret = Paint(props, doc, visual_device, area);
	}
	prop_list.Clear();
	return ret;
}
# endif // DOM_FULLSCREEN_MODE

#endif // CANVAS_SUPPORT

ProgressContent::~ProgressContent()
{
	if (progress)
	{
		progress->Delete();
		REPORT_MEMMAN_DEC(sizeof(OpProgress));
	}
}

/* virtual */
void ProgressContent::Disable(FramesDocument* doc)
{
	if ((doc->IsUndisplaying() || doc->IsBeingDeleted()))
	{
		/* Doc might delete VisualDevice before this layoutbox is
		   deleted (and we delete the progress widget).  So we have
		   to reset it here so widgets doesn't do anything with it
		   from now on. */

		if (progress)
			progress->SetVisualDevice(NULL);
	}
}

/* virtual */
LAYST ProgressContent::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!progress)
	{
		OP_STATUS status = OpProgress::Construct(&progress);
		if (OpStatus::IsMemoryError(status))
			return LAYOUT_OUT_OF_MEMORY;
		if (progress)
		{
			REPORT_MEMMAN_INC(sizeof(OpProgress));
			progress->SetVisualDevice(info.visual_device);
#ifdef SUPPORT_TEXT_DIRECTION
			progress->SetRTL(cascade->GetCascadingProperties().direction == CSS_VALUE_rtl);
#endif // SUPPORT_TEXT_DIRECTION
		}
	}

	return LAYOUT_CONTINUE;
}

/** Calculate intrinsic size. */

/* virtual */ OP_STATUS
ProgressContent::CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio)
{
	const HTMLayoutProperties& props = cascade->GetCascadingProperties();

	width = LayoutCoord(100);
	height = LayoutCoord(props.font_size);

	return OpStatus::OK;
}

/** Paint progress box on screen. */

/* virtual */ OP_STATUS
ProgressContent::Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area)
{
	if (progress == NULL)
		return OpStatus::OK;

	HTML_Element* html_element = layout_props->html_element;
	const HTMLayoutProperties& props = layout_props->GetCascadingProperties();

	short left		= props.padding_left + props.border.left.width;
	short top		= props.padding_top + props.border.top.width;
	int height		= GetHeight() - top - (props.padding_bottom + props.border.bottom.width);
	int width		= GetWidth() - left - (props.padding_right + props.border.right.width);

	// Update widget properties and position

	VisualDevice* old_vis_dev = progress ? progress->GetVisualDevice() : NULL;
	COLORREF old_color = visual_device->GetColor();

	progress->SetVisualDevice(visual_device);

	AffinePos ctm = visual_device->GetCTM();
	ctm.AppendTranslation(left, top);
	progress->SetPosInDocument(ctm);
	progress->SetRect(OpRect(0, 0, width, height));

	COLORREF bg_color = BackgroundColor(props);

	if (bg_color != USE_DEFAULT_COLOR)
		progress->SetBackgroundColor(HTM_Lex::GetColValByIndex(bg_color));
	else
		progress->UnsetBackgroundColor();

	progress->SetForegroundColor(HTM_Lex::GetColValByIndex(props.font_color));
	progress->SetHasCssBackground(FormObject::HasSpecifiedBackgroundImage(doc, props, html_element));
	progress->SetHasCssBorder(FormObject::HasSpecifiedBorders(props, html_element));

	// Get data and choose type depending of progress/meter values

	if (html_element->Type() == Markup::HTE_PROGRESS)
	{
		progress->SetType(OpProgress::TYPE_PROGRESS);
		progress->SetProgress((float)WebForms2Number::GetProgressPosition(doc, html_element));
	}
	else
	{
		OpProgress::TYPE type = OpProgress::TYPE_METER_GOOD_VALUE;
		float progress_val = 0;
		double val, min, max, low, high, optimum;
		WebForms2Number::GetMeterValues(doc, html_element, val, min, max, low, high, optimum);
		if (max > min)
		{
			progress_val = (float)((val - min) / (max - min));
			/*	"If the optimum point is equal to the low boundary or the high boundary, or anywhere in between them, then the region
				between the low and high boundaries of the gauge must be treated as the optimum region, and the low and high parts,
				if any, must be treated as suboptimal. Otherwise, if the optimum point is less than the low boundary, then the region
				between the minimum value and the low boundary must be treated as the optimum region, the region between the low
				boundary and the high boundary must be treated as a suboptimal region, and the region between the high boundary
				and the maximum value must be treated as an even less good region. Finally, if the optimum point is higher than the
				high boundary, then the situation is reversed; the region between the high boundary and the maximum value must be
				treated as the optimum region, the region between the high boundary and the low boundary must be treated as a
				suboptimal region, and the remaining region between the low boundary and the minimum value must be treated as an
				even less good region." */
			if (optimum < low)
			{
				if (val > low && val <= high)
					type = OpProgress::TYPE_METER_BAD_VALUE;
				else if (val > high)
					type = OpProgress::TYPE_METER_WORST_VALUE;
			}
			else if (optimum > high)
			{
				if (val >= low && val < high)
					type = OpProgress::TYPE_METER_BAD_VALUE;
				else if (val < low)
					type = OpProgress::TYPE_METER_WORST_VALUE;
			}
			else
			{
				if (val <= low || val >= high)
					type = OpProgress::TYPE_METER_BAD_VALUE;
			}
		}
		progress->SetType(type);
		progress->SetProgress(progress_val);
	}

	// Paint

	visual_device->Translate(left, top);

	progress->GenerateOnPaint(progress->GetBounds());

	visual_device->Translate(-left, -top);

	visual_device->SetColor(old_color);

	if (old_vis_dev && visual_device != old_vis_dev)
	{
		if (progress)
			progress->SetVisualDevice(old_vis_dev);
	}

	return OpStatus::OK;
}

#ifdef _PLUGIN_SUPPORT_
/* virtual */
EmbedContent::~EmbedContent()
{
	if (m_frames_doc)
		m_frames_doc->RemovePluginInstallationListener(this);
	ShutdownPlugin();
}

/** Calculate intrinsic size. */

/* virtual */ OP_STATUS
EmbedContent::CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio)
{
	width = LayoutCoord(150);
	height = LayoutCoord(150);

	return OpStatus::OK;
}

/** Update size of content.

	@return OpBoolean::IS_TRUE if size changed, OpBoolean::IS_FALSE if not, OpStatus::ERR_NO_MEMORY on OOM. */

/* virtual */ OP_BOOLEAN
EmbedContent::SetNewSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord width, LayoutCoord height,
						 LayoutCoord modified_horizontal_border_padding)
{
	OP_BOOLEAN size_changed = ReplacedContent::SetNewSize(cascade, info, width, height, modified_horizontal_border_padding);

	if (size_changed == OpBoolean::IS_TRUE)
		RETURN_IF_ERROR(ShowEmbed(info.doc));

	OP_ASSERT(win_visdev);
	OpPluginWindow* window = GetPluginWindow();
	if (window)
		win_visdev->SetPluginFixedPositionSubtree(window, GetFixedPositionedAncestor(cascade));

	return size_changed;
}

/** Signal that content may have changed. */

/* virtual */ void
EmbedContent::SignalChange(FramesDocument* doc)
{
	Enable(doc);
}

/** Simple helper function to honor TWEAK_LAYOUT_DO_NOT_PAINT_PLUGIN_CONTENT_ON_THUMBNAIL */

static inline BOOL AllowToPaintPluginOnWindow(Window *win)
{
#ifdef DO_NOT_PAINT_PLUGIN_CONTENT_ON_THUMBNAIL
	return !win->IsThumbnailWindow();
#else
	return TRUE;
#endif
}

/** Paint image box on screen. */

#ifdef PLUGIN_AUTO_INSTALL
/* virtual */ void
EmbedContent::OnPluginAvailable(const uni_char* mime_type)
{
	// Check if this broadcast concerns us
	if (m_plugin_mimetype.HasContent() && uni_str_eq(mime_type, m_plugin_mimetype.CStr()))
	{
		BOOL available;
		m_frames_doc->RequestPluginInfo(m_plugin_mimetype, m_plugin_name, available);
		embed_packed.plugin_available = available;
		Invalidate();
	}
}
#endif // PLUGIN_AUTO_INSTALL

/* virtual */ void
EmbedContent::OnPluginDetected(const uni_char* mime_type)
{
	// Check if this broadcast concerns us
	if (m_plugin_mimetype.HasContent() && uni_str_eq(mime_type, m_plugin_mimetype.CStr()))
	{
		// Clean up the element.
		ShutdownPlugin();

		// Reset element state.
		embed_packed_init = 0;

#ifdef PLUGIN_AUTO_INSTALL
		// Disable auto-install for this mime-type.
		m_frames_doc->NotifyPluginDetected(m_plugin_mimetype);
#endif  //PLUGIN_AUTO_INSTALL

		OP_STATUS status = ShowEmbed(m_frames_doc);
		if (OpStatus::IsMemoryError(status))
			m_frames_doc->GetWindow()->RaiseCondition(status);
	}
}

OP_STATUS
EmbedContent::PaintPlaceholder(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area)
{
	// Margin left from content egdes
	const int margin = 4;
	// Padding between elements drawn - image, strings
	const int padding = 2;
	// Content border width
	const int frame_width = 1;
	// Alpha value for placeholder with mouse over
	const UINT8 alpha_hovered = 254;
	// Alpha value for placeholder with mouse out
	const UINT8 alpha_normal = 100;
	// Font height for the strings
	const UINT8 text_height = 12;
	// Font number
	const StyleManager::GenericFont font_face = StyleManager::SANS_SERIF;
	// Content frame colors
	const COLORREF frame_color_hovered = OP_RGBA(0, 0, 0, alpha_hovered);
	const COLORREF frame_color_normal = OP_RGBA(0, 0, 0, alpha_normal);
	// Background colors
	const COLORREF bg_color_hovered = OP_RGBA(230, 230, 230, 254);
	const COLORREF bg_color_normal = OP_RGBA(230, 230, 230, 254);
	// Text colors
	const COLORREF text_color_hovered = OP_RGBA(0, 0, 0, alpha_hovered);
	const COLORREF text_color_normal = OP_RGBA(0, 0, 0, alpha_normal);

	const HTMLayoutProperties& props = layout_props->GetCascadingProperties();

	int top_text_len = 0, btm_text_len = 0, font_number = 0 ;
	int skin_image_w = 0, skin_image_h = 0, skin_image_edge = 0, w = 0, h = 0, full_h = 0, draw_y_pos = 0;
	BOOL show_top_text = FALSE, show_btm_text = FALSE, show_img = FALSE;
	OpRect skin_image_rect, painter_rect;
	OpString top_text, btm_text, tmp_text;

#ifdef SKIN_SUPPORT
	// Skin element names
	const char* skin_missing = "Missing plugin";
	const char* skin_disabled = "Disabled plugin";
	const char* skin_crashed = "Plugin Crash";
	const char* skin_missing_hover = "Missing plugin hover";
	const char* skin_disabled_hover = "Disabled plugin hover";

	OpSkinElement* skin_element = NULL;
	Image skin_image;

	switch (embed_packed.placeholder_state)
	{
	case EMBED_PSTATE_MISSING:
		// Get skin element for missing plugin
		skin_element = g_skin_manager->GetSkinElement(embed_packed.hovered?skin_missing_hover:skin_missing);
		break;
	case EMBED_PSTATE_DISABLED:
		// Get sking element for disabled plugin
		skin_element = g_skin_manager->GetSkinElement(embed_packed.hovered?skin_disabled_hover:skin_disabled);
		break;
	case EMBED_PSTATE_CRASHED:
		// Get skin element for crashed plugin
		skin_element = g_skin_manager->GetSkinElement(skin_crashed);
		break;
	}

	if (skin_element)
	{
		skin_image = skin_element->GetImage(0);
		skin_image_h = skin_image.Height();
		skin_image_w = skin_image.Width();
		skin_image_edge = MAX(skin_image_w, skin_image_h);
	}
	else
		/* Currently used skin is missing one or more skin elements used by the
		   placeholder. Please include them in the skin. Projects should ideally
		   use the same elements that Opera Desktop is using. */
		OP_ASSERT(!"NULL skin element!");
#endif // SKIN_SUPPORT

	// Acquire proper strings for the placeholder we're drawing
	switch (embed_packed.placeholder_state)
	{
	case EMBED_PSTATE_MISSING:
#ifdef PLUGIN_AUTO_INSTALL
		if (embed_packed.plugin_available)
		{
			TRAP_AND_RETURN(status, g_languageManager->GetStringL(Str::S_MISSING_PLUGIN_PLACEHOLDER_PLUGIN_NAME, tmp_text));
			TRAP_AND_RETURN(status, g_languageManager->GetStringL(Str::S_MISSING_PLUGIN_PLACEHOLDER_CLICK_ME, btm_text));
			RETURN_IF_ERROR(top_text.AppendFormat(tmp_text.CStr(), m_plugin_name.CStr()));
		}
		else
#endif
		{
			TRAP_AND_RETURN(status, g_languageManager->GetStringL(Str::S_MISSING_PLUGIN_PLACEHOLDER_PLUGIN_NAME_UNKNOWN, top_text));
			TRAP_AND_RETURN(status, g_languageManager->GetStringL(Str::S_MISSING_PLUGIN_PLACEHOLDER_CLICK_ME_UNKNOWN, btm_text));
		}
		break;
	case EMBED_PSTATE_DISABLED:
		TRAP_AND_RETURN(status, g_languageManager->GetStringL(Str::S_DISABLED_PLUGIN_PLACEHOLDER_LABEL, top_text));
		break;
	case EMBED_PSTATE_CRASHED:
		TRAP_AND_RETURN(status, g_languageManager->GetStringL(Str::S_CRASHED_PLUGIN_PLACEHOLDER_LABEL, top_text));
		break;
	default:
		OP_ASSERT(!"Unknown placeholder state!");
		return OpStatus::ERR;
	}

	abs_doc_x = LayoutCoord(visual_device->GetTranslationX());
	abs_doc_y = LayoutCoord(visual_device->GetTranslationY());

	painter_rect = OpRect(props.padding_left + LayoutCoord(props.border.left.width),
						  props.padding_top + LayoutCoord(props.border.top.width),
						  replaced_width - props.padding_left - LayoutCoord(props.border.left.width) - props.padding_right - LayoutCoord(props.border.right.width),
						  replaced_height - props.padding_top - LayoutCoord(props.border.top.width) - props.padding_bottom - LayoutCoord(props.border.bottom.width));

	// Calculate the position of the placeholder elements - image, strings
	font_number = styleManager->GetGenericFontNumber(font_face, WritingSystem::LatinWestern);
	visual_device->SetFont(font_number);
	visual_device->SetFontSize(text_height);

	if (top_text.HasContent())
		top_text_len = visual_device->GetTxtExtent(top_text.CStr(), top_text.Length());

	if (btm_text.HasContent())
		btm_text_len = visual_device->GetTxtExtent(btm_text.CStr(), btm_text.Length());

	// Position the skin image
	skin_image_rect.x = 0;
	skin_image_rect.y = 0;
	// Assume the image is square, easier calculations
	skin_image_rect.width = skin_image_edge;
	skin_image_rect.height = skin_image_edge;

	// Effective content size, without the margins
	w = painter_rect.width - (2 * margin);
	h = painter_rect.height - (2 * margin);

	// Will the top text fit?
	if (!top_text.IsEmpty() && (top_text_len <= w))
	{
		if (text_height <= h)
		{
			full_h += text_height + padding;
			show_top_text = TRUE;
		}
	}

	// Will the bottom text fit?
	if (!btm_text.IsEmpty() && (btm_text_len <= w))
	{
		if (full_h + text_height <= h)
		{
			full_h += text_height + padding;
			show_btm_text = TRUE;
		}
	}

	// Finally, will the image fit?
	if (skin_image_edge <= w)
	{
		if (full_h + skin_image_edge <= h)
		{
			full_h += skin_image_edge;
			show_img = TRUE;
		}
	}

	// Start drawing the elements from this y position
	draw_y_pos = (h - full_h) / 2;

	// Draw the background, to cover any color that might be on the page below the EMBED
	visual_device->SetColor(embed_packed.hovered ? bg_color_hovered : bg_color_normal);
	visual_device->FillRect(painter_rect);

	// Draw the frame around the placeholder
	visual_device->SetColor(embed_packed.hovered ? frame_color_hovered : frame_color_normal);
	for (int i = 0; i < frame_width; i++)
		visual_device->DrawRect(OpRect(painter_rect.x + i, painter_rect.y + i, painter_rect.width - 2 * i, painter_rect.height - 2 * i));

#ifdef SKIN_SUPPORT
	// Blit the image
	if (show_img)
	{
		// Try to set opacity, needed for blitting the image
		OP_STATUS opacity_status = visual_device->BeginOpacity(painter_rect, embed_packed.hovered ? alpha_hovered : alpha_normal);

		OpRect image_draw_rect = skin_image_rect;
		image_draw_rect.x = (w - skin_image_edge) / 2;
		image_draw_rect.y = draw_y_pos;
		visual_device->ImageOut(skin_image, skin_image_rect, image_draw_rect);
		draw_y_pos += skin_image_edge + padding;

		// Leave opacity if we were able to enter it
		if (OpStatus::IsSuccess(opacity_status))
			visual_device->EndOpacity();
	}
#endif // SKIN_SUPPORT

	// Draw the strings
	visual_device->SetColor(embed_packed.hovered ? text_color_hovered : text_color_normal);

	if (show_top_text)
	{
		visual_device->TxtOut((painter_rect.width - top_text_len) / 2, painter_rect.y + draw_y_pos, top_text.CStr(), top_text.Length());
		draw_y_pos += text_height + padding;
	}

	if (show_btm_text)
	{
		visual_device->TxtOut((painter_rect.width - btm_text_len) / 2, painter_rect.y + draw_y_pos, btm_text.CStr(), btm_text.Length());
		draw_y_pos += text_height + padding;
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
EmbedContent::Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area)
{
	const HTMLayoutProperties& props = layout_props->GetCascadingProperties();
	HTML_Element* html_element = layout_props->html_element;

	if (embed_packed.placeholder_state != EMBED_PSTATE_NONE)
		return PaintPlaceholder(layout_props, doc, visual_device, area);

	if (embed_packed.display_mode != EMBED_NORMAL
#ifdef USE_PLUGIN_EVENT_API
		// Must not mess with plug-in windows when not painting them to the screen.
		|| opns4plugin && !opns4plugin->IsWindowless() && !doc->GetVisualDevice()->IsPaintingToScreen()
#endif // USE_PLUGIN_EVENT_API
		)
	{
		OpString translated_alt_text;
		const uni_char* alt_text = html_element->GetStringAttr(Markup::HA_ALT);

		if (!alt_text)
		{
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DEFAULT_PLUGIN_ALT_TEXT, translated_alt_text));
			alt_text = translated_alt_text.CStr();

			if (!alt_text)
				alt_text = ImageStr;
		}

		visual_device->ImageAltOut(props.padding_left + LayoutCoord(props.border.left.width),
								   props.padding_top + LayoutCoord(props.border.top.width),
								   replaced_width - props.padding_left - LayoutCoord(props.border.left.width) - props.padding_right - LayoutCoord(props.border.right.width),
								   replaced_height - props.padding_top - LayoutCoord(props.border.top.width) - props.padding_bottom - LayoutCoord(props.border.bottom.width),
								   alt_text,
								   uni_strlen(alt_text),
								   (const_cast<const HTMLayoutProperties&>(props)).current_script);

		return OpStatus::OK;
	}

#ifdef SUPPORT_VISUAL_ADBLOCK
	if (doc->GetWindow()->GetContentBlockEditMode())
	{
		int left_padding_border = props.padding_left + props.border.left.width;
		int top_padding_border = props.padding_top + props.border.top.width;
		int width = replaced_width - props.padding_left - props.border.left.width - props.padding_right - props.border.right.width;
		int height = replaced_height - props.padding_top - props.border.top.width - props.padding_bottom - props.border.bottom.width;

		URL url = html_element->GetImageURL(FALSE);
		if (!url.IsEmpty() && doc->GetIsURLBlocked(url))
		{
			if (m_blocked_content.IsEmpty())
			{
				OpString text;
				RETURN_IF_LEAVE(g_languageManager->GetStringL(Str::S_BLOCKED_CONTENT_TEXT, text));

				OpBitmap* bitmap;

				if (OpBitmap::Create(&bitmap, width, height, FALSE, FALSE, 0, 0, TRUE) == OpStatus::OK) // FIXME: OOM
				{
					OpPainter* painter = bitmap->GetPainter();

					if (painter)
					{
						painter->SetColor(ADBLOCK_BG_PLUGIN_R, ADBLOCK_BG_PLUGIN_G, ADBLOCK_BG_PLUGIN_B);
						painter->FillRect(OpRect(0, 0, width, height));
						painter->SetColor(0, 0, 0);
						painter->DrawLine(OpPoint(0, 0), OpPoint(width - 1 , 0), 1);
						painter->DrawLine(OpPoint(width - 1, 0), OpPoint(width - 1, height - 1), 1);
						painter->DrawLine(OpPoint(width - 1, height - 1), OpPoint(0, height - 1), 1);
						painter->DrawLine(OpPoint(0, height - 1), OpPoint(0, 0), 1);

						OpFont* old_font = painter->GetFont();
						FontAtt font_att;
						font_att.SetFaceName(styleManager->GetFontManager()->GetGenericFontName(GENERIC_FONT_SANSSERIF));
						font_att.SetSize(ADBLOCK_FONT_HEIGHT);
						font_att.SetWeight(7);
						OpFont* font = g_font_cache ? g_font_cache->GetFont(font_att, 100) : NULL;

						if (font)
							painter->SetFont(font);

						int x = width / 2;
						int y = (height / 2) - (ADBLOCK_FONT_HEIGHT / 2);

						UINT32 text_width = 0;
						OpPoint font_pos;

						if (font)
							text_width = font->StringWidth(text.CStr(), text.Length(), painter, 0);

						x -= text_width / 2;

						if (x < 0)
							x = 0;

						font_pos.Set(x, y);

						painter->SetColor(ADBLOCK_BG_BOX_R, ADBLOCK_BG_BOX_G, ADBLOCK_BG_BOX_B);
						painter->FillRect(OpRect(1, y  - 2, width - 2, ADBLOCK_EFFECT_BOX_HEIGHT));

						painter->SetColor(0, 0, 0);
						painter->DrawString(font_pos, text.CStr(), text.Length(), 0);

						painter->DrawLine(OpPoint(1, y - 2), OpPoint(width - 1, y - 2), 1);
						painter->DrawLine(OpPoint(1, y + ADBLOCK_EFFECT_BOX_HEIGHT - 2), OpPoint(width - 1, y + ADBLOCK_EFFECT_BOX_HEIGHT - 2), 1);

						if (old_font)
							painter->SetFont(old_font);

						g_font_cache->ReleaseFont(font);

						bitmap->ReleasePainter();
						m_blocked_content = imgManager->GetImage(bitmap);
					}
					else
						OP_DELETE(bitmap);
				}
			}

			Hide(doc);

			OpRect src(0, 0, width, height);
			OpRect dst(left_padding_border, top_padding_border, width, height);

			return visual_device->ImageOut(m_blocked_content, src, dst);
		}
	}
#endif // SUPPORT_VISUAL_ADBLOCK

	int width = replaced_width - props.padding_left - props.border.left.width - props.padding_right - props.border.right.width;
	int height = replaced_height - props.padding_top - props.border.top.width - props.padding_bottom - props.border.bottom.width;

	OpRect local_rect(props.padding_left + props.border.left.width,
					  props.padding_top + props.border.top.width,
					  width, height);

	OpRect bbox = visual_device->ToBBox(local_rect);

	abs_doc_x = LayoutCoord(bbox.x);
	abs_doc_y = LayoutCoord(bbox.y);

	short x = abs_doc_x - visual_device->GetRenderingViewX();
	long y = abs_doc_y - visual_device->GetRenderingViewY();

#ifndef USE_PLUGIN_EVENT_API
	x = visual_device->ScaleToScreen(x);
	y = visual_device->ScaleToScreen(y);
	width = visual_device->ScaleToScreen(bbox.width);
	height = visual_device->ScaleToScreen(bbox.height);
#endif // !USE_PLUGIN_EVENT_API

	// Add plugin area to visualdevice so it can start track overlapping layoutboxes.
	OpStatus::Ignore(visual_device->AddPluginArea(local_rect, GetPluginWindow()));

#ifdef _PRINT_SUPPORT_
	if (visual_device->IsPrinter() || visual_device->GetDocumentManager()->GetPrintPreviewVD())
	{
		if (OpNS4PluginHandler::GetHandler() && width > 0 && height > 0)
		{
			const uni_char* plugin_url = m_plugin_content_url.GetAttribute(URL::KUniName_Username_Password_Hidden, TRUE).CStr();
			OpNS4PluginHandler::GetHandler()->Print(visual_device, plugin_url, (int)x, (int)y, (unsigned int)width, (unsigned int)height);
		}
	}
	else
#endif // _PRINT_SUPPORT_

		if (opns4plugin && AllowToPaintPluginOnWindow(doc->GetWindow()))
		{
#ifdef USE_PLUGIN_EVENT_API
# ifdef SUPPORT_VISUAL_ADBLOCK
			if (OpPluginWindow* plugin_window = opns4plugin->GetPluginWindow())
				plugin_window->BlockMouseInput(doc->GetWindow()->GetContentBlockEditMode() ||
											   !html_element->GetPluginExternal() && !html_element->GetPluginActivated());
# endif // SUPPORT_VISUAL_ADBLOCK

			OpRect plugin_rect((INT32)x, (INT32)y, (INT32)width, (INT32)height);
			OpRect paint_rect(&area);

			paint_rect.IntersectWith(OpRect(0, 0, visual_device->GetRenderingViewWidth(), visual_device->GetRenderingViewHeight()));
			paint_rect.IntersectWith(plugin_rect);
			return opns4plugin->Display(plugin_rect, paint_rect, embed_packed.show, GetFixedPositionedAncestor(layout_props));
#else
			embed_packed.x = x;
			this->y = y;
			opns4plugin->SetWindow((int)embed_packed.x, (int)y, (unsigned int)width, (unsigned int)height, embed_packed.show);

			if (window)
			{
				win_visdev->ShowPluginWindow(window, TRUE);
				win_visdev->SetPluginFixedPositionRoot(window, GetFixedPositionedAncestor(layout_props));
			}
#endif // USE_PLUGIN_EVENT_API
		}

	return OpStatus::OK;
}

/** VisualDeviceScrollListener::OnDocumentScroll */

void
EmbedContent::OnScroll(CoreView* view, INT32 dx, INT32 dy, CoreViewScrollListener::SCROLL_REASON reason)
{
	if (!win_visdev || !opns4plugin)
		return;

	int xpos = abs_doc_x - win_visdev->GetRenderingViewX();
	int ypos = (int)abs_doc_y - win_visdev->GetRenderingViewY();

#ifndef USE_PLUGIN_EVENT_API
	xpos = win_visdev->ScaleToScreen(xpos);
	ypos = win_visdev->ScaleToScreen(ypos);
#endif // !USE_PLUGIN_EVENT_API

	opns4plugin->SetWindowPos(xpos, ypos);
}

/* virtual */ void
EmbedContent::OnParentScroll(CoreView* view, INT32 dx, INT32 dy, CoreViewScrollListener::SCROLL_REASON reason)
{
	OnScroll(view, dx, dy, reason);
}

/** Hide external content, such as plug-ins, iframes, etc. */

/* virtual */ void
EmbedContent::Hide(FramesDocument* doc)
{
	if (opns4plugin)
		opns4plugin->HideWindow();
}

void
EmbedContent::Invalidate()
{
	if (win_visdev)
		win_visdev->Update(abs_doc_x, abs_doc_y, replaced_width, replaced_height);
}

/** Show embed. */

OP_STATUS
EmbedContent::ShowEmbed(FramesDocument* doc)
{
	OP_STATUS status = OpStatus::OK;

	win_visdev = doc->GetVisualDevice();

	if (!opns4plugin)
	{
		HTML_Element* OP_MEMORY_VAR html_element = GetHtmlElement();
		if (!embed_packed.internal_player && !PluginRemoved() &&
			g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled, doc->GetURL()) &&
			!doc->GetWindow()->GetForcePluginsDisabled())
		{
			OP_MEMORY_VAR BOOL postpone_new_stream = FALSE;
			URL* OP_MEMORY_VAR url = NULL;
			OP_MEMORY_VAR BOOL embed_url_changed = FALSE;

#ifdef USE_PLUGIN_EVENT_API
			SetParentInputContext(win_visdev);
#endif // USE_PLUGIN_EVENT_API

			/* If this is done outside a reflow (e.g. via SignalChange), we need to make
			   sure that the plug-in area is repainted if the plug-in changes display
			   mode now. */

			win_visdev->Update(abs_doc_x, abs_doc_y, replaced_width, replaced_height, TRUE);

			switch (html_element->Type())
			{
			case Markup::HTE_EMBED:
				url = html_element->GetEMBED_URL();

				if (PluginLoadingFailed() && html_element->Parent()->Type() == Markup::HTE_OBJECT)
				{
					url = html_element->Parent()->GetUrlAttr(Markup::HA_DATA, NS_IDX_HTML, doc->GetLogicalDocument());
					embed_url_changed = !!url;
				}
				break;

			case Markup::HTE_OBJECT:
				url = html_element->GetUrlAttr(Markup::HA_DATA, NS_IDX_HTML, doc->GetLogicalDocument());
				break;
			}

			Markup::Type type;

			const uni_char* mime_type = html_element->GetEMBED_Type();
			if (!mime_type || !*mime_type)
				if (html_element->Type() == Markup::HTE_APPLET ||
					html_element->Type() == Markup::HTE_OBJECT && html_element->GetResolvedObjectType(url, type, doc->GetLogicalDocument()) == OpBoolean::IS_TRUE && type == Markup::HTE_APPLET)
					mime_type = UNI_L("application/x-java-applet");

			OpString url_mime_type;
			if (url)
			{
				URLStatus urlstatus = (URLStatus) url->GetAttribute(URL::KLoadStatus, TRUE);
				BOOL header_loaded = !!url->GetAttribute(URL::KHeaderLoaded);

				// use server mimetype as default mimetype parameter to npp_newstream

				RETURN_IF_ERROR(url_mime_type.Set(url->GetAttribute(URL::KMIME_Type, TRUE)));

				if (urlstatus == URL_LOADED || header_loaded)
				{
					if (!mime_type || !*mime_type) // use server mimetype
					{
						if (url_mime_type.IsEmpty())
							return OpStatus::OK; // ignore loaded url with no server mimetype

						mime_type = url_mime_type.CStr();
					}
				}
				else
					if (!url->IsEmpty() && urlstatus != URL_LOADING_FAILURE && urlstatus != URL_UNLOADED)
						if (html_element->Type() == Markup::HTE_APPLET || html_element->GetEMBED_Type())
							postpone_new_stream = TRUE;
						else
							return status; // wait for header to be loaded before deciding url

				if (!header_loaded && urlstatus == URL_LOADING)
					postpone_new_stream = TRUE;
			}

			BOOL hidden = html_element->GetEMBED_Hidden();

#ifdef EPOC
			if (url->ContentType() == URL_WAV_CONTENT || url->ContentType() == URL_MIDI_CONTENT)
			{
				// On Symbian devices use internal player for wav and midi
				hidden = TRUE;
			}
#endif // EPOC

			embed_packed.show = !hidden;

			URL empty_url;  //placeholder when url is not in embed tag
			if (!url)
				url = &empty_url;

			// Remember the plugin content URL.
			m_plugin_content_url = *url;

			ViewActionReply reply;
			RETURN_IF_ERROR(Viewers::GetViewAction(*url, mime_type, reply, TRUE));

			if (url == &empty_url && reply.app.IsEmpty() && html_element->Type() == Markup::HTE_OBJECT &&
				html_element->GetResolvedObjectType(url, type, doc->GetLogicalDocument()) == OpBoolean::IS_TRUE &&
				type == Markup::HTE_APPLET)
				RETURN_IF_ERROR(Viewers::GetViewAction(empty_url, UNI_L("application/x-java-applet"), reply, TRUE));

			if (url != &empty_url && reply.app.IsEmpty() && reply.action == VIEWER_OPERA) // check if action could be determined from file extension
			{
				OpString fileext;
				TRAP_AND_RETURN(op_err, url->GetAttributeL(URL::KSuggestedFileNameExtension_L, fileext));

				if (fileext.HasContent())
					if (Viewer* v = g_viewers->FindViewerByExtension(fileext))
						RETURN_IF_ERROR(Viewers::GetViewAction(*url, v->GetContentTypeString(), reply, TRUE));
			}

			if (reply.app.IsEmpty())
			{
				if (reply.action == VIEWER_OPERA || hidden) // check if default plugin could be replaced
					if (url->Status(TRUE) == URL_LOADED)
					{
						if (url->PrepareForViewing(TRUE) == MSG_OOM_CLEANUP)
							return OpStatus::ERR_NO_MEMORY;
					}
					else
						return status; // wait for url to be loaded

				if (reply.action == VIEWER_ASK_USER || reply.action == VIEWER_NOT_DEFINED)
					doc->StopLoadingInline(url, html_element, EMBED_INLINE);

#ifdef EPOC
				// enable default plugin
				RETURN_IF_ERROR(Viewers::GetViewAction(*url, UNI_L("*"), reply, TRUE));
#else // EPOC
				// Plugin not installed, display alternative content, replaces usage of default plugin
				LogicalDocument* logdoc = doc->GetLogicalDocument();
				status = ShowAltPlugContent(logdoc ? logdoc->GetBaseURL() : NULL);
				embed_packed.display_mode = EMBED_ALT;

				// Check if the plugin is missing or disabled
				PluginViewer* default_pv = NULL;
				PluginViewer* enabled_pv = NULL;

				// Find a viewer for the mimetype, is there one at all?
				Viewer* v = g_viewers->FindViewerByMimeType(reply.mime_type);
				if (v != NULL)
				{
					// Get default plugin viewer
					default_pv = v->GetDefaultPluginViewer(FALSE);
					// Get enabled plugin viewer
					enabled_pv = v->GetDefaultPluginViewer(TRUE);
				}

				// Configure the placeholder depending on what we know about the plugin viewer
				if (!enabled_pv)
				{
					// No enabled plugin viewer, have to choose the placeholder for it
					if (default_pv)
						// There is a default plugin viewer, the plugin is disabled
						embed_packed.placeholder_state = EMBED_PSTATE_DISABLED;
					else
						// There is no default plugin viewer, the plugin is missing
						embed_packed.placeholder_state = EMBED_PSTATE_MISSING;
				}
				else
				{
					// There is an enabled plugin viewer... But why are we here then?
					OP_ASSERT(!"Plugin placeholder was not chosen!");
				}

# ifdef PLUGIN_AUTO_INSTALL
				if (reply.mime_type.HasContent())
				{
					// Remember the mimetype, we'll use it later
					RETURN_IF_ERROR(m_plugin_mimetype.Set(reply.mime_type));

					// Notify platform if the plugin is missing
					if (!default_pv && !enabled_pv)
					{
						// Hook this content as listener to the document that it belongs to
						// in order to be notified about a plugin becoming available
						if (!embed_packed.listener_set)
						{
							embed_packed.listener_set = TRUE;
							doc->AddPluginInstallationListener(this);
						}
						// Notify the platform that the mimetype is missing a plugin
						doc->NotifyPluginMissing(reply.mime_type);
					}

					// If the plugin is already resolved by platform, get the plugin name and availablity of the installer
					// Only makes sense if the platform plugin manager is present
					BOOL available;
					doc->RequestPluginInfo(m_plugin_mimetype, m_plugin_name, available);
					embed_packed.plugin_available = available;
				}
# else // PLUGIN_AUTO_INSTALL
#  ifdef WIC_USE_ASK_PLUGIN_DOWNLOAD
				if (embed_packed.placeholder_state == EMBED_PSTATE_MISSING
					&& reply.mime_type.CompareI("APPLICATION/X-SHOCKWAVE-FLASH") == 0)
						RETURN_IF_ERROR(ShowDownloadResource(doc));
#  endif // WIC_USE_ASK_PLUGIN_DOWNLOAD
# endif // !PLUGIN_AUTO_INSTALL
				return status;
#endif // !EPOC
			}
			else
				if (reply.action == VIEWER_PLUGIN) // check if plugin should be enabled
				{
#ifdef SVG_SUPPORT
					// Prevents execution of applets and plugins for foreignObjects in svg in <img> elements, DSK-240651.
					if (!g_svg_manager->IsEcmaScriptEnabled(doc))
					{
						doc->StopLoadingInline(url, html_element, EMBED_INLINE);
						embed_packed.show = FALSE;
						return status;
					}
#endif // SVG_SUPPORT
				}

			if (PluginLoadingFailed() && !embed_url_changed)
				return OpStatus::OK;

			if (reply.app.HasContent() && reply.mime_type.Length())
			{
#ifndef USE_PLUGIN_EVENT_API
				/* It is assumed that 'sizeof(long)<=8' for these buffers */
				uni_char embed_width[21];  /* ARRAY OK 2009-05-11 davve */
				uni_char embed_height[21]; /* ARRAY OK 2009-05-11 davve */
				const uni_char** argn = NULL;
				const uni_char** argv = NULL;
				int argc = 0;

				embed_packed.transparent = FALSE;

				OP_STATUS status = html_element->GetEmbedAttrs(argc, argn, argv);

				if (status == OpStatus::OK)
					for (int i = 0; i < argc; i++)
						if (argn[i] && argv[i])
							if (uni_stri_eq(argn[i], "WIDTH"))
							{
								uni_ltoa(replaced_width, embed_width, 10);
								argv[i] = embed_width;
							}
							else
								if (uni_stri_eq(argn[i], "HEIGHT"))
								{
									uni_ltoa(replaced_height, embed_height, 10);
									argv[i] = embed_height;
								}
#ifdef NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND // Note: This is not a true capability define; it depends on platform defines
								else
									if ((reply.mime_type.CompareI("APPLICATION/X-SHOCKWAVE-FLASH") == 0 ||
										 reply.mime_type.CompareI("VIDEO/QUICKTIME") == 0) &&
										uni_stri_eq(argn[i], "WMODE") &&
										(uni_stri_eq(argv[i], "TRANSPARENT") || uni_stri_eq(argv[i], "OPAQUE")))
									{
										embed_packed.transparent = TRUE;
									}
#endif // NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
#ifdef EPOC
									else
										if (mime_type.Compare(mime_type_all) == 0 &&
											(uni_stri_eq(argn[i], "SRC") || uni_stri_eq(argn[i], "DATA")))
										{
											// default plugin: avoid showing url with user and password

											URL tmpurl = g_url_api->GetURL(argv[i]);

											argv[i] = tmpurl.UniName(PASSWORD_NOTHING);
										}
#endif // EPOC
										else
											if (uni_stri_eq(argn[i], "WINDOWLESS") && uni_stri_eq(argv[i], "TRUE"))
												embed_packed.transparent = TRUE;

				if (status == OpStatus::OK && !window)
				{
					CoreView* parent_view = FindParentCoreView(html_element);
					status = win_visdev->GetNewPluginWindow(window, -replaced_width, -replaced_height, replaced_width, replaced_height,
															parent_view,
															embed_packed.transparent,
															html_element);

					if (status == OpStatus::OK)
					{
						if (!((CoreViewScrollListener*)this)->InList())
							win_visdev->GetView()->AddScrollListener(this);
						window->SetListener(this);
# ifdef MANUAL_PLUGIN_ACTIVATION
						window->BlockMouseInput(!html_element->GetPluginExternal() && !html_element->GetPluginActivated());
# endif // MANUAL_PLUGIN_ACTIVATION
					}
				}
#endif // !USE_PLUGIN_EVENT_API

				if (status == OpStatus::OK && !postpone_new_stream && OpNS4PluginHandler::GetHandler())
				{
#ifdef USE_PLUGIN_EVENT_API
					opns4plugin = OpNS4PluginHandler::GetHandler()->New(reply.app.CStr(), reply.component_type, doc, reply.mime_type, doc->IsPluginDoc() ? NP_FULL : NP_EMBED, html_element, url, embed_url_changed);
#else
					opns4plugin = OpNS4PluginHandler::GetHandler()->New(reply.app.CStr(), reply.component_type, doc, window, reply.mime_type, doc->IsPluginDoc() ? NP_FULL : NP_EMBED, argc, argn, argv, html_element, url, embed_url_changed);
#endif // USE_PLUGIN_EVENT_API

					embed_packed.loading_failed = !opns4plugin;

					if (opns4plugin && win_visdev->GetView())
					{
#ifdef USE_PLUGIN_EVENT_API
						opns4plugin->SetWindowListener(this);
						OP_ASSERT(!InList());
						win_visdev->GetView()->AddScrollListener(this);
#endif // USE_PLUGIN_EVENT_API

						if (
# ifdef USER_JAVASCRIPT
							!OpStatus::IsMemoryError(html_element->PluginInitializedElement(doc->GetHLDocProfile())) && // send user javascript event
# endif // USER_JAVASCRIPT
							!url->IsEmpty())
						{
							URL moved_to = url->GetAttribute(URL::KMovedToURL, TRUE);

							// check if url has been redirected before streaming url
							// FIXME: What if the URL is redirected after this?

							url = !moved_to.IsEmpty() ? &moved_to : url;

							const uni_char* mime_type_str;
							if (url_mime_type.Length() && !uni_stri_eq(url_mime_type.CStr(), UNI_L("APPLICATION/OCTET-STREAM")))
								mime_type_str = url_mime_type.CStr();
							else
								mime_type_str = reply.mime_type.CStr();

							status = opns4plugin->AddPluginStream(*url, mime_type_str);
						}
					}
					else
						OpNS4PluginHandler::GetHandler()->Resume(doc, html_element, TRUE, TRUE);
				}

#ifndef USE_PLUGIN_EVENT_API
				OP_DELETEA(argn);
				OP_DELETEA(argv);
#endif // !USE_PLUGIN_EVENT_API
			}
			else if (url == &empty_url)
			{
				//no url found ? let the plugin resume normally, because ES might be suspended
				OpNS4PluginHandler::GetHandler()->Resume(doc, html_element, TRUE, TRUE);
			}
			else if (url->Status(TRUE) == URL_LOADING && (url->GetAttribute(URL::KMIME_Type).CStr()))
			{
				doc->StopLoadingInline(url, html_element, EMBED_INLINE);
			}
		}
		else
		{
			OpNS4PluginHandler::GetHandler()->Resume(doc, html_element, TRUE, TRUE);
			// Check if the plugins are disabled globally
			if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled, doc->GetURL()) == 0)
				embed_packed.placeholder_state = EMBED_PSTATE_DISABLED;
		}
	}

	if (opns4plugin && win_visdev->GetView())
	{
#ifdef USE_PLUGIN_EVENT_API
		OpRect plugin_rect(abs_doc_x, abs_doc_y, replaced_width, replaced_height);
		plugin_rect.OffsetBy(-win_visdev->GetRenderingViewX(), -win_visdev->GetRenderingViewY());
		status = opns4plugin->Display(plugin_rect, plugin_rect, FALSE, NULL);
#else
		opns4plugin->SetWindow(-win_visdev->ScaleToScreen(replaced_width), -win_visdev->ScaleToScreen(replaced_height), win_visdev->ScaleToScreen(replaced_width), win_visdev->ScaleToScreen(replaced_height), FALSE);
#endif // USE_PLUGIN_EVENT_API
	}

	return status;
}

/** Get the OpPluginWindow used. */

/* virtual */ OpPluginWindow*
EmbedContent::GetPluginWindow() const
{
#ifdef USE_PLUGIN_EVENT_API
	return opns4plugin ? opns4plugin->GetPluginWindow() : 0;
#else
	return window;
#endif // USE_PLUGIN_EVENT_API
}

/** Show alternative plugin content when plugin is not installed. Replaces usage of the default plugin. */

OP_STATUS
EmbedContent::ShowAltPlugContent(URL* base_url)
{
	OP_STATUS status = OpStatus::OK;
#ifndef PLUGIN_AUTO_INSTALL
	HTML_Element* html_element = GetHtmlElement();
	const uni_char** argn = NULL;
	const uni_char** argv = NULL;
	int argc = 0;
	status = html_element->GetEmbedAttrs(argc, argn, argv);
	if (status == OpStatus::OK)
	{
		for (int i = 0; i < argc; i++)
		{
			if (status == OpStatus::OK && argn[i] && argv[i])
			{
				if (uni_stri_eq(argn[i], "TYPE"))
					status = m_plugin_mimetype.Set(argv[i] ? argv[i] : UNI_L(""));
				else if (uni_stri_eq(argn[i], "PLUGINSPAGE") || uni_stri_eq(argn[i], "PLUGINSPACE"))
				{
					if (base_url)
					{
						URL url = g_url_api->GetURL(*base_url, argv[i] ? argv[i] : UNI_L(""));
						status = url.GetAttribute(URL::KUniName, m_plugin_downloadurl);
					}
					else
						status = m_plugin_downloadurl.Set(argv[i] ? argv[i] : UNI_L(""));
				}
			}
		}

		if (status == OpStatus::OK)
		{
			if (!m_plugin_mimetype.IsEmpty())
			{
				if (uni_stri_eq(m_plugin_mimetype.CStr(), "application/pdf"))
					status = m_plugin_name.Set("Adobe Acrobat");
				else if (uni_stri_eq(m_plugin_mimetype.CStr(), "application/x-al-package"))
					status = m_plugin_name.Set("Anti-Leech");
				else if (uni_stri_eq(m_plugin_mimetype.CStr(), "application/x-director"))
					status = m_plugin_name.Set("Shockwave for Director");
				else if (uni_stri_eq(m_plugin_mimetype.CStr(), "application/x-mplayer2"))
					status = m_plugin_name.Set("Windows Media Player");
				else if (uni_stri_eq(m_plugin_mimetype.CStr(), "application/x-mtx"))
					status = m_plugin_name.Set("MetaStream 3");
				else if (uni_stri_eq(m_plugin_mimetype.CStr(), "application/x-mwf"))
					status = m_plugin_name.Set("Autodesk MapGuide");
				else if (uni_stri_eq(m_plugin_mimetype.CStr(), "application/x-shockwave-flash"))
					status = m_plugin_name.Set("Macromedia Flash Player");
				else if (uni_stri_eq(m_plugin_mimetype.CStr(), "application/x-spt"))
					status = m_plugin_name.Set(UNI_L("MDL\xae Chime"));
				else if (uni_stri_eq(m_plugin_mimetype.CStr(), "audio/x-pn-realaudio-plugin"))
					status = m_plugin_name.Set("RealPlayer");
				else if (uni_stri_eq(m_plugin_mimetype.CStr(), "image/svg-xml"))
					status = m_plugin_name.Set("Adobe SVG Viewer");
				else if (uni_stri_eq(m_plugin_mimetype.CStr(), "video/quicktime"))
					status = m_plugin_name.Set("QuickTime");
				else
					status = m_plugin_name.Set("");
			}
			else
				status = m_plugin_name.Set("");
		}
	}

	OP_DELETEA(argn);
	OP_DELETEA(argv);
#endif // !PLUGIN_AUTO_INSTALL
	return status;
}

/** Handle events received for EMBED/OBJECT where the plugin is not installed. */

OP_STATUS
EmbedContent::HandleMouseEvent(Window* win, DOM_EventType dom_event)
{
	if (embed_packed.display_mode == EMBED_NORMAL
		&& embed_packed.placeholder_state == EMBED_PSTATE_NONE)
		return OpStatus::OK;

	switch (dom_event)
	{
#ifdef PLUGIN_AUTO_INSTALL
	case ONCLICK:
		if ((embed_packed.display_mode == EMBED_ALT && embed_packed.placeholder_state != EMBED_PSTATE_NONE) ||
			(embed_packed.display_mode == EMBED_NORMAL && embed_packed.placeholder_state == EMBED_PSTATE_DISABLED))
				if (m_frames_doc)
				{
					PluginInstallInformationImpl information;
					RETURN_IF_ERROR(information.SetMimeType(m_plugin_mimetype));
					information.SetURL(m_plugin_content_url);
					m_frames_doc->RequestPluginInstallation(information);
				}
		break;
#endif // PLUGIN_AUTO_INSTALL
	case ONMOUSEOUT:
		if (embed_packed.hovered)
		{
			embed_packed.hovered = FALSE;
			Invalidate();
		}
		break;
	case ONMOUSEMOVE:
	case ONMOUSEOVER:
		if (!embed_packed.hovered)
		{
			embed_packed.hovered = TRUE;
			Invalidate();
		}

		if (win)
		{
			OpString tooltip;
			switch (embed_packed.placeholder_state)
			{
			case EMBED_PSTATE_DISABLED:
				{
					TRAPD(err, g_languageManager->GetStringL(Str::S_DISABLED_PLUGIN_PLACEHOLDER_TOOLTIP, tooltip));
				}
				break;
			case EMBED_PSTATE_MISSING:
				{
					TRAPD(err, g_languageManager->GetStringL(Str::S_MISSING_PLUGIN_PLACEHOLDER_TOOLTIP, tooltip));
				}
				break;
			case EMBED_PSTATE_NONE:
				if (embed_packed.display_mode == EMBED_ALT)
				{
					OpString format;
					TRAPD(err, g_languageManager->GetStringL(Str::D_PLUGIN_DOWNLOAD_RESOURCE_STATUS, format));
					if (format.CStr())
						RETURN_IF_ERROR(tooltip.AppendFormat(format.CStr(), m_plugin_name.CStr()));
				}
				else
				{
					TRAPD(err, g_languageManager->GetStringL(Str::S_DEACTIVATED_PLUGIN_HELP_TEXT, tooltip));
				}
				break;
			case EMBED_PSTATE_CRASHED:
				break;
			}

			if (tooltip.HasContent())
				win->DisplayLinkInformation(NULL, ST_ASTRING, tooltip.CStr());
		}
		break;

	default:
		break;
	}

	return OpStatus::OK;
}

/** Create form, plugin and iframe objects */

/* virtual */ OP_STATUS
EmbedContent::Enable(FramesDocument* doc)
{
	if (!opns4plugin && replaced_width && replaced_height)
		RETURN_IF_ERROR(ShowEmbed(doc));

	return OpStatus::OK;
}

/** Uninitialize and delete plugin. */

void
EmbedContent::ShutdownPlugin()
{
#ifdef USE_PLUGIN_EVENT_API
	SetParentInputContext(NULL);
#endif // USE_PLUGIN_EVENT_API

	if (OpNS4PluginHandler::GetHandler() && opns4plugin)
	{
#ifdef USE_PLUGIN_EVENT_API
		opns4plugin->SetWindowListener(NULL);
#endif
		OpNS4PluginHandler::GetHandler()->Destroy(opns4plugin);
	}

	if (win_visdev)
	{
		win_visdev->GetView()->RemoveScrollListener(this);

#ifndef USE_PLUGIN_EVENT_API
		if (window)
			win_visdev->DeletePluginWindow(window);
#endif // !USE_PLUGIN_EVENT_API
	}

	opns4plugin = NULL;
#ifndef USE_PLUGIN_EVENT_API
	window = NULL;
	embed_packed.x = y = abs_doc_x = abs_doc_y = LayoutCoord(-10000);
#endif // !USE_PLUGIN_EVENT_API
}

/** Remove form, plugin and iframe objects */

/* virtual */ void
EmbedContent::Disable(FramesDocument* doc)
{
	ShutdownPlugin();
}

/* virtual */ void
EmbedContent::DetachCoreView(FramesDocument* doc)
{
	if (OpPluginWindow* plugin_window = GetPluginWindow())
		doc->GetVisualDevice()->DetachPluginCoreView(plugin_window);
}

/* virtual */ void
EmbedContent::AttachCoreView(FramesDocument* doc, HTML_Element* element)
{
	if (OpPluginWindow* plugin_window = GetPluginWindow())
	{
		CoreView* parent_view = FindParentCoreView(element);

		doc->GetVisualDevice()->AttachPluginCoreView(plugin_window, parent_view ? parent_view : doc->GetVisualDevice()->GetView());
	}
}

/** Ask if user wants to download the required plugin from the plugin's download site */

#if defined(WIC_USE_ASK_PLUGIN_DOWNLOAD) && !defined(PLUGIN_AUTO_INSTALL)
OP_STATUS
EmbedContent::ShowDownloadResource(FramesDocument* frames_doc)
{
	// get topmost document
	frames_doc = frames_doc->GetTopDocument();

	if (g_pcui->GetIntegerPref(PrefsCollectionUI::AskAboutFlashDownload) && !frames_doc->AskedAboutFlashDownload())
	{
		Window* window = frames_doc->GetWindow();

		frames_doc->SetAskedAboutFlashDownload(); // make sure that the user is asked only once per document

		if (!window->IsThumbnailWindow()) // don't popup a dialog when generating thumbnails
			if (WindowCommander* commander = window->GetWindowCommander())
				if (commander->GetDocumentListener())
					commander->GetDocumentListener()->OnAskPluginDownload(commander);
	}

	return OpStatus::OK;
}
#endif // WIC_USE_ASK_PLUGIN_DOWNLOAD && !PLUGIN_AUTO_INSTALL

/* virtual */ void
EmbedContent::OnMouseDown()
{
#ifdef MANUAL_PLUGIN_ACTIVATION
	HTML_Element* elm = GetHtmlElement();

# ifdef SUPPORT_VISUAL_ADBLOCK
	Window* core_window = elm->GetLogicalDocument()->GetFramesDocument()->GetWindow();

	if (core_window && core_window->GetContentBlockEditMode())
		// In AdBlock mode we handle this click in OnMouseUp

		return;
	else
# endif // SUPPORT_VISUAL_ADBLOCK
		if (opns4plugin && (elm->GetPluginExternal() || elm->GetPluginActivated()))
#endif // MANUAL_PLUGIN_ACTIVATION
		{
			opns4plugin->SetPopupsEnabled(TRUE);
		}

# ifdef USE_PLUGIN_EVENT_API
	/* When clicked on, focus should be set to this content, this will cause OnFocus to
	   be called (and focus events sent to the embedee) plus it will make us the recipient
	   of all OpInputActions so that these can be passed along appropriatly. */

	if (!IsFocused())
		g_input_manager->SetKeyboardInputContext(this, FOCUS_REASON_ACTIVATE);
# endif // USE_PLUGIN_EVENT_API
}

/* virtual */ void
EmbedContent::OnMouseUp()
{
#ifdef SUPPORT_VISUAL_ADBLOCK
	HTML_Element* elm = GetHtmlElement();
	LogicalDocument* logdoc = elm->GetLogicalDocument();
	FramesDocument* doc = logdoc->GetFramesDocument();
	Window* core_window = doc->GetWindow();
	if (core_window->GetContentBlockEditMode())
	{
		URL url = elm->GetImageURL(TRUE, logdoc);
		if (!url.IsEmpty())
		{
			// ignore further processing of this click and just notify the document listener
			WindowCommander* win_com = core_window->GetWindowCommander();
			OpStringC image_url = url.GetAttribute(URL::KUniName, TRUE);

			if (doc->GetIsURLBlocked(url))
			{
				doc->RemoveFromURLBlockedList(url);
				win_com->GetDocumentListener()->OnContentUnblocked(win_com, image_url);
			}
			else
			{
				doc->AddToURLBlockedList(url);
				win_com->GetDocumentListener()->OnContentBlocked(win_com, image_url);
			}
			elm->MarkExtraDirty(doc);
		}
	}
#endif // SUPPORT_VISUAL_ADBLOCK

#if defined(USE_PLUGIN_EVENT_API) && defined MANUAL_PLUGIN_ACTIVATION
	OpPluginWindow* plugin_window = GetPluginWindow();

	if (plugin_window && IsFocused())
	{
		plugin_window->BlockMouseInput(FALSE);
		GetHtmlElement()->SetPluginActivated(TRUE);
	}
#endif // USE_PLUGIN_EVENT_API && MANUAL_PLUGIN_ACTIVATION
}

/* virtual */ void
EmbedContent::OnMouseHover()
{
#ifdef MANUAL_PLUGIN_ACTIVATION
	HTML_Element* elm = GetHtmlElement();
	if (!elm->GetPluginExternal() && !elm->GetPluginActivated())
	{
		// show tooltip
		Window* core_window = m_frames_doc->GetWindow();
		if (core_window)
		{
			OpString tooltip; ANCHOR(OpString, tooltip);
			TRAPD(status, g_languageManager->GetStringL(Str::D_PLUGIN_ACTIVATE, tooltip));
			if (!tooltip.IsEmpty())
			{
				core_window->DisplayLinkInformation(NULL, ST_ASTRING, tooltip.CStr());
			}
		}
	}
#endif // MANUAL_PLUGIN_ACTIVATION
}

# ifdef USE_PLUGIN_EVENT_API
BOOL EmbedContent::OnInputAction(OpInputAction* action)
{
	if (!IsFocused())
		return FALSE;

	OpPluginWindow* plugin_window = GetPluginWindow();
	if (!plugin_window)
		return FALSE;

	OpPluginKeyState key_state;
	switch (action->GetAction())
	{
	case OpInputAction::ACTION_LOWLEVEL_KEY_UP:
		key_state = PLUGIN_KEY_UP;
		break;

	case OpInputAction::ACTION_LOWLEVEL_KEY_DOWN:
		key_state = PLUGIN_KEY_DOWN;
		break;

	case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
		key_state = PLUGIN_KEY_PRESSED;
		break;

	default:
		return FALSE;
	}

	return opns4plugin->DeliverKeyEvent(action->GetActionKeyCode(), action->GetKeyValue(), action->GetPlatformKeyEventData(), key_state, action->GetKeyLocation(), action->GetShiftKeys());
}

void EmbedContent::OnFocus(BOOL focus, FOCUS_REASON reason)
{
	opns4plugin->DeliverFocusEvent(!!focus, reason);
}
# endif // USE_PLUGIN_EVENT_API

#endif // _PLUGIN_SUPPORT_

#ifdef SVG_SUPPORT

/** IFrameContent, while normally used for plain iframes, is also used
	for <object> and <img> referencing external SVG content. This
	function returns TRUE when this frame is considered to be an SVG
	frame.

	SVG frames adjust their size to the size of the nested SVG, if
	any.

	Note that an <iframe> with SVG content is _not_ considered an SVG
	frame and doesn't follow the rules about sizing the frame to the
	intrinsic size of the content as SVG frames normally follow.
	Currently <embed> falls into this category as well, but that might
	be considered a bug. */

static BOOL IsSVGFrame(HTML_Element *html_element, FramesDocument *frm_doc)
{
	BOOL is_svg_frame = FALSE;

	OP_ASSERT(html_element != NULL);
	OP_ASSERT(frm_doc != NULL);

	URL url = html_element->GetImageURL(TRUE, frm_doc->GetLogicalDocument());
	is_svg_frame = (url.ContentType() == URL_SVG_CONTENT);

	return is_svg_frame;
}

#endif // SVG_SUPPORT

/* Find out whether the frame has been loaded. */

BOOL
IFrameContent::IsFrameLoaded()
{
	if (!frame)
		if (placeholder->GetHtmlElement()->GetInserted() == HE_INSERTED_BY_PARSE_AHEAD)
			return FALSE;
		else
			/* This can happen while printing if GetNewIFrame doesn't
			   return anything. This is as loaded as we'll ever be. */
			return TRUE;

	FramesDocument* iframe_doc = frame->GetCurrentDoc();
	BOOL is_printing = FALSE;

#ifdef _PRINT_SUPPORT_
	if (frame->GetPrintDoc())
	{
		iframe_doc = frame->GetPrintDoc();
		is_printing = TRUE;
	}
#endif

	if (iframe_doc)
	{
		BOOL doc_loaded = is_printing || iframe_doc->IsLoaded(FALSE);
		DM_LoadStat load_status = iframe_doc->GetDocManager()->GetLoadStatus();
		BOOL doc_man_stable = (load_status == NOT_LOADING || load_status == DOC_CREATED);

		return (doc_loaded && doc_man_stable && iframe_doc->GetDocRoot() &&
				!iframe_doc->GetDocRoot()->IsDirty());
	}

	return FALSE;
}

IFrameContent::~IFrameContent()
{
	if (frame)
		if (VisualDevice* frm_dev = frame->GetVisualDevice())
			frm_dev->SetFixedPositionSubtree(NULL);
}

/** Signal that content may have changed. */

/* virtual */ void
IFrameContent::SignalChange(FramesDocument* doc)
{
	/* After the first reflow in the nested document we must
	   get at least one reflow in the parent document. */

	if (!packed.size_determined && IsFrameLoaded())

		// Size can be determined; reflow
		placeholder->MarkDirty(doc);
}

/** Paint iframe box on screen. */

/* virtual */ OP_STATUS
IFrameContent::Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area)
{
	const HTMLayoutProperties& props = layout_props->GetCascadingProperties();
	if (frame)
	{
		OP_ASSERT(frame->GetHtmlElement() == GetHtmlElement());
		short left_border_padding = props.padding_left + props.border.left.width;
		short top_border_padding = props.padding_top + props.border.top.width;

		// Cover any plugins areas we overlap
		visual_device->CoverPluginArea(OpRect(0, 0, GetWidth(), GetHeight()));

		visual_device->Translate(left_border_padding, top_border_padding);

		frame->SetPosition(visual_device->GetCTM());

		if (props.visibility == CSS_VALUE_visible)
		{
			OP_STATUS status = frame->ShowFrames();
			if (OpStatus::IsError(status))
			{
				// Restore translation
				visual_device->Translate(-left_border_padding, -top_border_padding);
				return status;
			}
		}
		else
			frame->HideFrames();

		if (VisualDevice* frame_vd = frame->GetVisualDevice())
		{
			OpRect rect(&area);

			frame_vd->PaintIFrame(rect, visual_device);
		}
#ifdef _PRINT_SUPPORT_
		else
		{
			frame->Display(visual_device, area);
		}
#endif

		visual_device->Translate(-left_border_padding, -top_border_padding);

		if (props.opacity != 255 || props.HasBorderRadius())
		{
			FramesDocument* iframe_doc = frame->GetCurrentDoc();
			if (iframe_doc && iframe_doc->GetLogicalDocument())
				// Background must be treated as fixed when it can be seen through.

				iframe_doc->GetLogicalDocument()->GetLayoutWorkplace()->SetBgFixed();
		}
	}

	return OpStatus::OK;
}

#ifdef DOM_FULLSCREEN_MODE
OP_STATUS
IFrameContent::PaintFullscreen(FramesDocument* doc, VisualDevice* visual_device, HTML_Element* elm, const RECT& area)
{
	OP_STATUS ret = OpStatus::ERR;

	if (frame)
	{
		OP_ASSERT(frame->GetHtmlElement() == GetHtmlElement());

		Head prop_list;
		LayoutProperties* props = LayoutProperties::CreateCascade(elm, prop_list, doc->GetHLDocProfile());
		if (!props)
			return OpStatus::ERR_NO_MEMORY;

		frame->SetPosition(visual_device->GetCTM());

		if (props->GetCascadingProperties().visibility == CSS_VALUE_visible)
			ret = frame->ShowFrames();

		prop_list.Clear();
	}

	return ret;
}
#endif // DOM_FULLSCREEN_MODE

#ifdef SVG_SUPPORT
SVGImage *
IFrameContent::GetNestedSVGImage()
{
	if (frame && frame->GetCurrentDoc())
		if (LogicalDocument *logdoc = frame->GetCurrentDoc()->GetLogicalDocument())
		{
			HTML_Element *svg_element = logdoc->GetDocRoot();

			if (!svg_element)
				return NULL;

			if (Box *layout_box = svg_element->GetLayoutBox())
				if (SVGContent* svg_content = layout_box->GetSVGContent())
					return svg_content->GetSVGImage(frame->GetCurrentDoc(), svg_element);
		}

	return NULL;
}
#endif // SVG_SUPPORT

BOOL
IFrameContent::CalculateIntrinsicSizeForSVGFrame(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord &width, LayoutCoord &height, int &ratio)
{
#ifdef SVG_SUPPORT
	if (IsSVGFrame(cascade->html_element, info.doc))
		if (SVGImage *svg_image = GetNestedSVGImage())
		{
			/* The -o-content-size functionality doesn't mix with the
			   SVG sizing; make sure it is disabled: */

			iframe_packed.expand_iframe_height = 0;
			iframe_packed.expand_iframe_width = 0;

			width = LayoutCoord(0);
			height = LayoutCoord(0);

			if (info.doc->GetShowImages())
			{
				RETURN_IF_ERROR(svg_image->GetIntrinsicSize(cascade, width, height, ratio));

				if (width < 0)
					width = (ratio != 0) ? LayoutCoord(0) : LayoutCoord(300);

				if (height < 0)
					height = (ratio != 0) ? LayoutCoord(0) : LayoutCoord(150);
			}

			packed.size_determined = 1;

			return TRUE;
		}
#endif // SVG_SUPPORT

	return FALSE;
}

/** Calculate intrinsic size. */

/* virtual */ OP_STATUS
IFrameContent::CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio)
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	iframe_packed.expand_iframe_height = props.content_height == CONTENT_HEIGHT_O_SIZE;
	iframe_packed.expand_iframe_width = props.content_width == CONTENT_WIDTH_O_SIZE;

	if (iframe_packed.expand_iframe_height &&
		(props.left != HPOSITION_VALUE_AUTO && props.left < -1000 ||
		 props.top != VPOSITION_VALUE_AUTO && props.top < -1000))
		/* CSSR - page designers (read gmail) may choose to position the iframe outside
		   the screen, to make it invisible, but still make sure it loads. This hack
		   makes sure it gets hidden in this case. */

		iframe_packed.expand_iframe_height = FALSE;

	width = LayoutCoord(iframe_packed.expand_iframe_width ? 0 : 300);
	height = LayoutCoord(iframe_packed.expand_iframe_height ? 0 : 150);

	if (frame)
	{
		FramesDocument* iframe_doc = frame->GetCurrentDoc();

#ifdef _PRINT_SUPPORT_
		if (frame->GetPrintDoc())
			iframe_doc = frame->GetPrintDoc();
#endif // _PRINT_SUPPORT_

		if (iframe_doc && iframe_doc->GetDocRoot())
			if (!CalculateIntrinsicSizeForSVGFrame(cascade, info, width, height, ratio))
			{
				BOOL size_ready = TRUE;

				if (iframe_packed.expand_iframe_height)
					height = LayoutCoord(iframe_doc->Height());

				if (iframe_packed.expand_iframe_width)
				{
					if (iframe_packed.second_pass_width)
					{
						width = LayoutCoord(iframe_doc->Width());
					}
					else
					{
						iframe_packed.second_pass_width = 1;
						packed.size_determined = 0;
						size_ready = FALSE;

						/* Force a reflow in the nested document to
						   make sure that we enter the second pass. */
						iframe_doc->GetDocRoot()->MarkDirty(iframe_doc, FALSE);
					}
				}

				if (IsFrameLoaded() && size_ready)
					packed.size_determined = 1;
			}
	}
	return OpStatus::OK;
}

/** Update size of content.

	@return OpBoolean::IS_TRUE if size changed, OpBoolean::IS_FALSE if not, OpStatus::ERR_NO_MEMORY on OOM. */

/* virtual */ OP_BOOLEAN
IFrameContent::SetNewSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord width, LayoutCoord height,
						  LayoutCoord modified_horizontal_border_padding)
{
	HTML_Element* html_element = cascade->html_element;
	const HTMLayoutProperties& props = *cascade->GetProps();

	unsigned short padding_border_width = modified_horizontal_border_padding;
	unsigned short padding_border_height = props.padding_top + props.padding_bottom +
										   props.border.top.width + props.border.bottom.width;

#ifdef SVG_SUPPORT
	object_fit_position = props.object_fit_position;
#endif // SVG_SUPPORT

	OP_ASSERT(width >= padding_border_width && height >= padding_border_height || !"Content box dimensions can't be less than zero");

	int innerwidth = width - padding_border_width;
	int innerheight = height - padding_border_height;

	OP_BOOLEAN size_changed;
	if (replaced_height != height || replaced_width != width)
	{
		replaced_height = height;
		replaced_width = width;

		size_changed = OpBoolean::IS_TRUE;
	}
	else
		size_changed = OpBoolean::IS_FALSE;

	if (frame)
		frame->SetSize(innerwidth, innerheight);
	else
	{
		// Get a new inline frame from document

		BOOL load_frame = TRUE;

#ifdef _PRINT_SUPPORT_
		load_frame = !info.doc->IsPrintDocument();
#endif

#ifdef DELAYED_SCRIPT_EXECUTION
		if (html_element->GetInserted() == HE_INSERTED_BY_PARSE_AHEAD)
			return size_changed;
#endif // DELAYED_SCRIPT_EXECUTION

		OP_STATUS status = info.doc->GetNewIFrame(frame, innerwidth, innerheight, html_element, NULL, load_frame, FALSE);
		if (OpStatus::IsMemoryError(status))
			return status;
		if (OpStatus::IsError(status))
		{
			/* Probably because we're printing and can't create new
			   iframes if they were not previously known.

			   Set the size_determined-flag now because we will never
			   be able to create a frame of which we can determine the
			   size. */

			packed.size_determined = 1;
			return size_changed;
		}
		OP_ASSERT(frame);
		OP_ASSERT(frame->GetHtmlElement() == GetHtmlElement());

		if (iframe_packed.expand_iframe_height || iframe_packed.expand_iframe_width)
		{
			frame->SetNotifyParentOnContentChange();

			if (!load_frame)
			{
				/* We disallowed loading the frame; adapt to what's already present. */

				packed.reflow_requested = 1;
				info.workplace->SetReflowElement(html_element);
			}
		}

#ifdef _PRINT_SUPPORT_
		if (!info.doc->IsPrintDocument())
#endif
			frame->GetVisualDevice()->SetFixedPositionSubtree(GetFixedPositionedAncestor(cascade));
	}

	return size_changed;
}

/** Create form, plugin and iframe objects */

/* virtual */ OP_STATUS
IFrameContent::Enable(FramesDocument* doc)
{
	OP_ASSERT(!frame || frame->GetHtmlElement() == GetHtmlElement());

	if (frame)
	{
		if (CoreView* core_view = FindParentCoreView(GetHtmlElement()))
			frame->SetParentLayoutInputContext(core_view);
	}

	return OpStatus::OK;
}

/** Remove form, plugin and iframe objects */

/* virtual */ void
IFrameContent::Disable(FramesDocument* doc)
{
	if (frame)
	{
		OP_ASSERT(!frame->GetHtmlElement() || frame->GetHtmlElement() == GetHtmlElement());
		DocListElm* doc_elm = frame->GetDocManager()->CurrentDocListElm();
		if (doc_elm)
			frame->GetDocManager()->StoreViewport(doc_elm);
		frame->HideFrames();

		frame->SetParentLayoutInputContext(NULL);
	}
}

#ifdef PAGED_MEDIA_SUPPORT

/* virtual */ BREAKST
IFrameContent::AttemptPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain)
{
	if (frame)
	{
		FramesDocument* iframe_doc = (FramesDocument*)frame->GetCurrentDoc();

#ifdef _PRINT_SUPPORT_
		if (frame->GetPrintDoc())
			iframe_doc = frame->GetPrintDoc();
#endif // _PRINT_SUPPORT_

		if (iframe_doc)
		{
			BOOL found_breaks = iframe_doc->PageBreakIFrameDocument(info.GetTranslationY() - info.doc->GetPageTop());
			return found_breaks ? BREAK_FOUND : BREAK_KEEP_LOOKING;
		}
	}

	return BREAK_FOUND;
}

#endif // PAGED_MEDIA_SUPPORT


#ifdef ON_DEMAND_PLUGIN

/* virtual */ OP_STATUS
PluginPlaceholderContent::Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area)
{
	if (enable_on_demand_plugin_placeholder)
		return IFrameContent::Paint(layout_props, doc, visual_device, area);
	else
	{
		OpString translated_alt_text;
		const HTMLayoutProperties& props = layout_props->GetCascadingProperties();
		HTML_Element* html_element = layout_props->html_element;
		const uni_char* alt_text = html_element->GetStringAttr(Markup::HA_ALT);

		if (!alt_text)
		{
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DEFAULT_PLUGIN_ALT_TEXT, translated_alt_text));
			alt_text = translated_alt_text.CStr();

			if (!alt_text)
				alt_text = ImageStr;
		}

		visual_device->ImageAltOut(props.padding_left + props.border.left.width,
								   props.padding_top + props.border.top.width,
								   GetWidth() - props.padding_left - props.border.left.width - props.padding_right - props.border.right.width,
								   GetHeight() - props.padding_top - props.border.top.width - props.padding_bottom - props.border.bottom.width,
								   alt_text,
								   uni_strlen(alt_text),
								   (const_cast<const HTMLayoutProperties&>(props)).current_script);
	}

	return OpStatus::OK;
}

OP_STATUS
PluginPlaceholderContent::HandleMouseEvent(FramesDocument* doc, DOM_EventType dom_event)
{
	/* When the plugin placeholder is treated as a ImageAltOut (with
	   empty iframe document), we handle events here to show tooltips
	   and to realize the plugin on click. */

	switch (dom_event)
	{
	case ONCLICK:
	{
		if (LogicalDocument* logdoc = doc->GetLogicalDocument())
			logdoc->GetLayoutWorkplace()->ActivatePluginPlaceholder(GetHtmlElement());
		break;
	}
	case ONMOUSEMOVE:
	case ONMOUSEOVER:
	{
		OpString tooltip;
		RETURN_IF_LEAVE(g_languageManager->GetStringL(Str::S_ON_DEMAND_PLUGIN_TOOLTIP, tooltip));
		doc->GetWindow()->DisplayLinkInformation(NULL, ST_ASTRING, tooltip.CStr());
		break;
	}
	default:
		break;
	}

	return OpStatus::OK;
}

/* virtual */ LAYST
PluginPlaceholderContent::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	enable_on_demand_plugin_placeholder = (BOOL)g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::EnableOnDemandPluginPlaceholder);

	/* We need to stop the loading of this plugin if it has been
	   started or else we will get NSL */

	URL plugin_url = cascade->html_element->GetImageURL(TRUE, info.doc->GetLogicalDocument());
	info.doc->StopLoadingInline(&plugin_url, cascade->html_element, GENERIC_INLINE);

	return IFrameContent::Layout(cascade, info, first_child, start_position);
}

#ifdef LAYOUT_AUTO_ACTIVATE_HIDDEN_ON_DEMAND_PLACEHOLDERS
/* virtual */ LAYST
PluginPlaceholderContent::FinishLayout(LayoutInfo& info)
{
	LayoutProperties* cascade = placeholder->GetCascade();
	BOOL auto_demand = (!GetWidth() || !GetHeight() ||
						cascade->GetProps()->visibility != CSS_VALUE_visible);

	if (auto_demand)
		info.workplace->ActivatePluginPlaceholder(cascade->html_element);

	return IFrameContent::FinishLayout(info);
}
#endif // LAYOUT_AUTO_ACTIVATE_HIDDEN_ON_DEMAND_PLACEHOLDERS

/* virtual */ void
PluginPlaceholderContent::SignalChange(FramesDocument* doc)
{
	HTML_Element *element = GetHtmlElement();

	if (element->IsPluginDemanded())
		element->MarkExtraDirty(doc);

	IFrameContent::SignalChange(doc);
}

#endif // ON_DEMAND_PLUGIN

/** Re-layout content that overflowed first-line. */

BOOL
Container::EndFirstLine(LayoutInfo& info, LayoutProperties* child_cascade)
{
	LayoutProperties* cascade = placeholder->GetCascade();
	HTML_Element* old_stop_before = info.stop_before;

	info.stop_before = child_cascade->html_element->NextSibling();

	cascade->RemoveFirstLineProperties();

	// Compute last element and start position

	if (placeholder->LayoutChildren(cascade, info, reflow_state->break_before_content, reflow_state->GetLineCurrentPosition()) == LAYOUT_OUT_OF_MEMORY)
		return FALSE;

	info.stop_before = old_stop_before;

	return TRUE;
}

/** Finish reflowing box. */

/* virtual */ LAYST
Container::FinishLayout(LayoutInfo& info)
{
	LayoutProperties* cascade = placeholder->GetCascade();
	const HTMLayoutProperties& props = *cascade->GetProps();

	if (!reflow_state)
	{
		// container was skipped during layout

		if (NeedMinMaxWidthCalculation(cascade))
		{
			LayoutCoord min_width;
			LayoutCoord normal_min_width;
			LayoutCoord max_width;

			OP_ASSERT(packed.minmax_calculated);

			if (GetMinMaxWidth(min_width, normal_min_width, max_width))
				placeholder->PropagateWidths(info, min_width, normal_min_width, max_width);
		}

		if (!placeholder->IsBlockBox() || !((BlockBox*) placeholder)->IsInStack())
			// In-flow block boxes have already propagated their margins (BlockBox::Layout())

			placeholder->PropagateBottomMargins(info);

		return LAYOUT_CONTINUE;
	}

	switch (CommitLineContent(info, TRUE))
	{
	case LAYOUT_END_FIRST_LINE:
		// First-line finished

		if (EndFirstLine(info, placeholder->GetCascade()))
			return LAYOUT_END_FIRST_LINE;

		/* EndFirstLine() failed due to OOM; fall through */
	default:
		// OOM

		return LAYOUT_OUT_OF_MEMORY;

	case LAYOUT_CONTINUE:
#ifdef SUPPORT_TEXT_DIRECTION
		if (reflow_state->bidi_calculation)
		{
			if (reflow_state->bidi_calculation->HandleParagraphEnd() == OpStatus::ERR_NO_MEMORY)
				return LAYOUT_OUT_OF_MEMORY;

#ifdef _DEBUG
			OP_NEW_DBG("Container::FinishLayout\n", "bidi");
			ParagraphBidiSegment* seg = bidi_segments ? (ParagraphBidiSegment*)bidi_segments->First() : NULL;
			int i = 0;

			while (seg)
			{
				OP_DBG(("###i: %d level: %d, width: %d, element: %x, v_pos: %d",i,
					    seg->level,
						seg->width,
						seg->start_element,
						seg->virtual_position));

				seg = (ParagraphBidiSegment*)seg->Suc();
				i++;
			}
#endif // _DEBUG
		}
#endif // SUPPORT_TEXT_DIRECTION

		if (CloseVerticalLayout(info, FINISHLAYOUT) == LAYOUT_OUT_OF_MEMORY)
			return LAYOUT_OUT_OF_MEMORY;

		break;
	}

	if (info.doc->GetLayoutMode() != LAYOUT_NORMAL && reflow_state->calculate_min_max_widths)
		if (layout_stack.Empty())
			SetTrueTableCandidate(NEUTRAL);
		else
			if (reflow_state->true_table_lines < 0)
				SetTrueTableCandidate(DISABLE);
			else
				if (!reflow_state->non_true_table_lines || reflow_state->true_table_lines > reflow_state->non_true_table_lines)
					SetTrueTableCandidate(reflow_state->true_table_lines ? POSITIVE : NEUTRAL);
				else
					if (reflow_state->non_true_table_lines == 1)
					{
						VerticalLayout* vertical_layout = static_cast<VerticalLayout*>(layout_stack.First());
						if (vertical_layout && vertical_layout->IsLine())
						{
							LayoutCoord min_width;
							LayoutCoord normal_min_width;
							LayoutCoord max_width;

							GetMinMaxWidth(min_width, normal_min_width, max_width);

							if (max_width < placeholder->GetCascade()->GetProps()->font_size * 2)
								SetTrueTableCandidate(NEUTRAL);
						}
					}

	if (reflow_state->clearance_stops_bottom_margin_collapsing)
		reflow_state->stop_bottom_margin_collapsing = TRUE;

	UpdateScreen(info);

	if (packed.no_content && !packed.stop_top_margin_collapsing)
		if (!reflow_state->stop_bottom_margin_collapsing)
		{
			if (props.MarginsAdjoining(reflow_state->css_height))
			{
				if (props.margin_bottom)
				{
					VerticalMargin bottom_margin;

					bottom_margin.CollapseWithBottomMargin(props);
					placeholder->RecalculateTopMargins(info, &bottom_margin, TRUE);
				}

				//even though this box is empty we need to propagate content box
				//as it may come from absolutely positioned descendant
				placeholder->PropagateBottomMargins(info, NULL, FALSE);
			}
			else
			{
				packed.stop_top_margin_collapsing = TRUE;
				placeholder->PropagateBottomMargins(info);
			}
		}
		else
			packed.stop_top_margin_collapsing = TRUE;

	if (
#ifdef PAGED_MEDIA_SUPPORT
		info.paged_media == PAGEBREAK_ON ||
#endif // PAGED_MEDIA_SUPPORT
		cascade->multipane_container)
		if (reflow_state->stop_bottom_margin_collapsing)
		{
			if (BreakForced(reflow_state->page_break_policy) || BreakForced(reflow_state->column_break_policy))
				if (!InsertPageOrColumnBreak(info, reflow_state->page_break_policy, reflow_state->column_break_policy, cascade->multipane_container, cascade->space_manager))
					return LAYOUT_OUT_OF_MEMORY;
		}
		else
			/* Propagate the break property to the parent container for further
			   analysis. It may "collapse" with another break property up
			   there. Creating a break for each of them would be wrong then. */

			cascade->container->PropagateBreakPolicies(reflow_state->page_break_policy, reflow_state->column_break_policy);

	if (reflow_state->calculate_min_max_widths)
		packed.minmax_calculated = 1;
#ifdef DEBUG
	else
	{
		// If we decided not to calculate min/max widths, they'd better be left unchanged.

		OP_ASSERT(reflow_state->old_min_width == minimum_width);
		OP_ASSERT(reflow_state->old_max_width == maximum_width);
	}
#endif // DEBUG

	DeleteReflowState();

	return LAYOUT_CONTINUE;
}

void Container::CenterContentVertically(const LayoutInfo& info, const HTMLayoutProperties& props, LayoutCoord content_height)
{
	/* This will cause problems for pagebreaking, hence breaking inside
	   a ButtonContainer is disabled.

	   FIXME this implementation does not take offseting of the button bounding box because of overflowing content
	   into consideration. This is not a trivial problem because it is not certain that we can decrease the size of
	   an overflowing edge (CORE-25280).

	   If you want to use this for something else than <button>, please go ahead, but be aware of the limitations
	   mentioned above.*/

	OP_ASSERT(GetButtonContainer());

	HTML_Element* el = placeholder->GetHtmlElement();

	LayoutCoord padding_border_top = props.padding_top + LayoutCoord(props.border.top.width);
	LayoutCoord padding_border_bottom = props.padding_bottom + LayoutCoord(props.border.bottom.width);

	LayoutCoord content_box_height = height - padding_border_top - padding_border_bottom;

	OP_ASSERT(content_height >= 0 && content_box_height >= 0);

	LayoutCoord offset(0);

	if (content_height < content_box_height)
		offset = (content_box_height - content_height) / LayoutCoord(2);

	VerticalLayout* vl = (VerticalLayout*)layout_stack.First();

	if (!vl || offset == 0)
		return;

	while (vl)
	{
		vl->MoveDown(offset, el);
		vl = vl->Suc();
	}

	/* Inline-blocks already invalidated by the line */
	if (placeholder->IsBlockBox())
		info.visual_device->UpdateRelative(0, 0, GetWidth(), height);
}


/** Finish reflowing box. */

/* virtual */ void
Container::UpdateScreen(LayoutInfo& info)
{
	if (reflow_state)
	{
		LayoutProperties* cascade = placeholder->GetCascade();
		const HTMLayoutProperties& props = *cascade->GetProps();
		LayoutCoord height = MAX(reflow_state->reflow_position, reflow_state->list_marker_bottom)
			- (props.padding_top + LayoutCoord(props.border.top.width));
		LayoutCoord min_height(0);
		if (reflow_state->calculate_min_max_widths)
			min_height = MAX(reflow_state->min_reflow_position, reflow_state->list_marker_bottom)
				- props.GetNonPercentTopBorderPadding();

		if (reflow_state->list_marker_bottom > 0)
			packed.no_content = FALSE;

		UpdateHeight(info, cascade, height, min_height, FALSE);

		if (reflow_state->propagate_min_max_widths)
		{
			LayoutCoord min_width;
			LayoutCoord normal_min_width;
			LayoutCoord max_width;

			if (GetMinMaxWidth(min_width, normal_min_width, max_width))
				placeholder->PropagateWidths(info, min_width, normal_min_width, max_width);
		}

		if (MarginsCollapsing())
		{
			if (placeholder && placeholder->IsBlockBox() && ((BlockBox *)placeholder)->HasClearance())
			{
				if (placeholder->GetCascade()->container)
					placeholder->GetCascade()->container->ClearanceStopsMarginCollapsing(TRUE);
			}
		}

		if (placeholder->IsAbsolutePositionedBox())
			// FIXME: yes, this fixes a problem for absolutely positioned blocks, but we need similar stuff for absolutely positioned tables.

			((AbsolutePositionedBox*) placeholder)->UpdatePosition(info, TRUE);

#ifdef CSS_TRANSFORMS
		if (TransformContext *transform_context = placeholder->GetTransformContext())
			if (!placeholder->IsInlineBox())
				transform_context->Finish(info, cascade, GetWidth(), GetHeight());
#endif // CSS_TRANSFORMS

		if (reflow_state->abspos_right_edge > 0 || reflow_state->abspos_percentage > LayoutFixed(0))
			placeholder->PropagateRightEdge(info, reflow_state->abspos_right_edge, reflow_state->abspos_noncontent, reflow_state->abspos_percentage);

		if (placeholder->IsPositionedBox())
		{
			StackingContext *stacking_context = placeholder->GetLocalStackingContext();

			if (!stacking_context)
				stacking_context = cascade->stacking_context;

			OP_ASSERT(stacking_context);

			stacking_context->UpdateBottomAligned(info);
		}

		if (reflow_state->stop_bottom_margin_collapsing)
			// Do not propagate bottom margins to parent

			placeholder->PropagateBottomMargins(info);
		else
			if (!packed.no_content || packed.stop_top_margin_collapsing)
				placeholder->PropagateBottomMargins(info, &reflow_state->margin_state);
	}
}

/** Count number of words in segment. */

/* static */ int
Content::CountWords(LineSegment& segment, HTML_Element* containing_element)
{
	// This code is mostly duplicated in Line::Traverse()

	HTML_Element* element = NULL;
	HTML_Element* start_element = segment.start_element;
	int words = 0;

	if (start_element)
		for (HTML_Element* run = start_element; run != containing_element; run = run->Parent())
		{
			element = run;

			if (!run)
				break;
		}

	if (!element)
	{
		element = containing_element->FirstChild();
		segment.start_element = NULL;
	}

	while (element)
	{
		Box* box = element->GetLayoutBox();

		if (box && !box->CountWords(segment, words))
			break;

		element = element->Suc();
	}

	segment.start_element = start_element;

	return words;
}

/** Count number of words in segment. */

int
Content::CountWords(LineSegment& segment) const
{
	return CountWords(segment, GetHtmlElement());
}

/** Find distance from position to next tab stop. */

LayoutCoord
Container::GetTabSpace()
{
	const HTMLayoutProperties* props = placeholder->GetCascade()->GetProps();

	OP_ASSERT(props);

	if (props)
	{
		LayoutCoord tab_width = LayoutCoord(props->font_average_width * props->tab_size);

		if (tab_width)
		{
			LayoutCoord position = reflow_state->GetPendingContent();

			return LayoutCoord(((position + tab_width + 1) / tab_width) * tab_width - position);
		}
	}

	return LayoutCoord(0);
}

/** Is this a preferred recipient of extra space? */

BOOL
TableColumnInfo::IsPreferredExpandee(BOOL min_width_required, BOOL sloppy_desired_width) const
{
	if (min_width_required && !normal_min_width && !max_width || packed.has_percent)
		return FALSE;

	return desired_width == 0 && (min_width_required || packed.is_auto) || packed.preferred_expandee ||
		sloppy_desired_width && desired_width < max_width;
}

/** @return TRUE if visibility of the table-column or table-column-group box defining this column is collapsed. */

BOOL
TableColumnInfo::IsVisibilityCollapsed() const
{
	return column_box && column_box->IsVisibilityCollapsed();
}

TableContent::TableContent()
  : column_widths(NULL),
	column_count(0),
	last_used_column(0),
	old_css_height(0),
	height(0),
	row_left_offset(0),
	width(-1),
	minimum_width(0),
	normal_minimum_width(0),
	constrained_maximum_width(-1),
	maximum_width(0),
	minimum_height(0),
	reflow_state(NULL)
{
	packed_init = 0;
}

/* virtual */
TableContent::~TableContent()
{
	layout_stack.RemoveAll();

	OP_DELETE(reflow_state);
	OP_DELETEA(column_widths);
}

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

/** Signal that a TableColGroupBox in this Table is about to be deleted. */

void
TableContent::SignalChildDeleted(FramesDocument* doc, TableColGroupBox* child)
{
	if (column_widths)
	{
		BOOL found = FALSE;

		for (unsigned int i = 0; i < column_count; i++)
			if (column_widths[i].column_box == child)
			{
				column_widths[i].column_box = NULL;
				found = TRUE;
			}
			else
				if (found)
					break;
	}
}

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

/** Set X position and width of all TableColGroupBox objects in this table. */

void
TableContent::SetColGroupBoxDimensions()
{
	if (column_widths && column_count > 0)
	{
		int step = 1;
		int start_col = 0;
		int stop_col = column_count;

#ifdef SUPPORT_TEXT_DIRECTION
		if (packed.rtl)
		{
			step = -1;
			start_col = column_count - 1;
			stop_col = -1;
		}
#endif // SUPPORT_TEXT_DIRECTION

		TableColGroupBox* cur_col_box = NULL;
		TableColGroupBox* cur_col_group_box = NULL;
		LayoutCoord x(reflow_state->border_spacing_horizontal);

		for (int i = start_col; i != stop_col; i += step)
		{
			if (column_widths[i].IsVisibilityCollapsed())
				continue;

			LayoutCoord end = x + column_widths[i].width;

			if (column_widths[i].column_box)
			{
				if (column_widths[i].column_box != cur_col_box)
				{
					// Found new column box. We're in its leftmost column. Set left edge.

					cur_col_box = column_widths[i].column_box;
					cur_col_box->SetX(x);

					if (cur_col_box->IsGroup())
						cur_col_group_box = cur_col_box;
					else
					{
						Box* parent_box = cur_col_box->GetHtmlElement()->Parent()->GetLayoutBox();

						if (parent_box->IsTableColGroup())
						{
							TableColGroupBox* group_box = (TableColGroupBox*) parent_box;

							if (group_box != cur_col_group_box)
							{
								// Found new column group box. We're in its leftmost column. Set left edge.

								cur_col_group_box = group_box;
								cur_col_group_box->SetX(x);
							}
						}
					}
				}

				cur_col_box->SetWidth(end - cur_col_box->GetX());

				if (cur_col_group_box && cur_col_group_box != cur_col_box)
					cur_col_group_box->SetWidth(end - cur_col_group_box->GetX());
			}

			x = end + LayoutCoord(reflow_state->border_spacing_horizontal);
		}
	}
}

/** Get the baseline of the specified row, relative to the table top edge.

	@param row_number Row number. Negative values means counting from the bottom of the table. */

LayoutCoord
TableContent::LocalGetBaseline(long row_number) const
{
	LayoutCoord first_row = LAYOUT_COORD_MIN;

	for (TableListElement *elm = (TableListElement *)(row_number > 0 ? layout_stack.First() : layout_stack.Last());
		 elm;
		 elm = (TableListElement *)(row_number > 0 ? elm->Suc() : elm->Pred()))
	{
		if (elm->IsRowGroup() && !((TableRowGroupBox*) elm)->IsVisibilityCollapsed())
		{
			for (TableRowBox *box = row_number > 0 ? ((TableRowGroupBox *)elm)->GetFirstRow() : ((TableRowGroupBox *)elm)->GetLastRow();
				 box;
				 box = (TableRowBox *)(row_number > 0 ? box->Suc() : box->Pred()))
			{
				if (box->IsVisibilityCollapsed())
					continue;

				LayoutCoord baseline = elm->GetStaticPositionedY() + box->GetStaticPositionedY() + box->GetBaseline();

				if (baseline == LAYOUT_COORD_MAX)
					baseline = LAYOUT_COORD_MAX - LayoutCoord(1);

				if (row_number > 0)
				{
					row_number --;
					if (first_row == LAYOUT_COORD_MIN)
						first_row = baseline;
				}
				else
				{
					row_number ++;
					first_row = baseline;
				}

				if (row_number == 0)
					return baseline;
			}
		}
	}

	return first_row;
}

/** Get baseline of first or last line.

	@param last_line TRUE if last line baseline search (inline-block case).	*/

/* virtual */ LayoutCoord
TableContent::GetBaseline(BOOL last_line /*= FALSE*/) const
{
	OP_ASSERT(!placeholder->IsInlineBox());

	LayoutCoord baseline = LocalGetBaseline(1);

	return baseline == LAYOUT_COORD_MIN ? LayoutCoord(0) : baseline;
}

/** Get baseline of inline block or content of inline element that
	has a baseline (e.g. ReplacedContent with baseline, BulletContent). */

/* virtual */ LayoutCoord
TableContent::GetBaseline(const HTMLayoutProperties& props) const
{
	OP_ASSERT(placeholder->IsInlineBlockBox());

	long row_number = props.table_baseline;
	LayoutCoord baseline;

	if (row_number == 0 || (baseline = LocalGetBaseline(row_number)) == LAYOUT_COORD_MAX)
		return height + props.margin_bottom;
	else
		return baseline == LAYOUT_COORD_MIN ? LayoutCoord(0) : baseline;
}

/** Get the baseline if maximum width is satisfied. */

/* virtual */ LayoutCoord
TableContent::GetMinBaseline(const HTMLayoutProperties& props) const
{
	/* This is not correct, but hopefully good enough for now. Finding the
	   correct baseline of an inline-table would require introducing the
	   concept of "minimum height", "minimum baseline" and "minimum y
	   position" to rows and row-groups. Saving this task for later. */

	return minimum_height;
}

/** Calculate and return bottom margins of layout element. See declaration in header file for more information. */

/* virtual */ OP_BOOLEAN
TableContent::CalculateBottomMargins(LayoutProperties* cascade, LayoutInfo& info, VerticalMargin* bottom_margin, BOOL include_this) const
{
	TableListElement* element = (TableListElement*) layout_stack.Last();

	if (element && element->IsCaption())
	{
		// We have a trailing caption

		TableCaptionBox* caption_box = (TableCaptionBox*) element;

		return caption_box->CalculateBottomMargins(cascade, info, bottom_margin);
	}
	else
		Content::CalculateBottomMargins(cascade, info, bottom_margin, include_this);

	return OpBoolean::IS_TRUE;
}

/** Calculate and return top margins of layout element. See declaration in header file for more information. */

/* virtual */ OP_BOOLEAN
TableContent::CalculateTopMargins(LayoutProperties* cascade, LayoutInfo& info, VerticalMargin* top_margin, BOOL include_self_bottom) const
{
	TableListElement* element = (TableListElement*) layout_stack.First();

	if (element && element->IsCaption())
	{
		// We have a leading caption

		TableCaptionBox* caption_box = (TableCaptionBox*) element;

		return caption_box->CalculateTopMargins(cascade, info, top_margin);
	}
	else
	{
		const HTMLayoutProperties& props = *cascade->GetProps();

		if (props.margin_top)
			top_margin->CollapseWithTopMargin(props, TRUE);
	}

	return OpBoolean::IS_TRUE;
}

#ifdef PAGED_MEDIA_SUPPORT

/** Insert a page break. */

BREAKST
TableContent::PageBreakHelper(LayoutInfo& info, int strength, TableListElement* element)
{
	BREAKST status = BREAK_KEEP_LOOKING;

	if (!packed.avoid_page_break_inside || strength >= 1)
		for (; element && status == BREAK_KEEP_LOOKING; element = element->Pred())
			status = element->AttemptPageBreak(info, strength);

	return status;
}

/** Insert a page break. */

/* virtual */ BREAKST
TableContent::AttemptPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain)
{
	TableListElement* element = (TableListElement*) layout_stack.Last();

	return PageBreakHelper(info, strength, element);
}

/** Insert a page break. */

BREAKST
TableContent::InsertPageBreak(LayoutInfo& info, int strength)
{
	TableListElement* element = reflow_state->reflow_element;

	OP_ASSERT(element);

	BREAKST status = PageBreakHelper(info, strength, element);

	if (status != BREAK_KEEP_LOOKING)
		return status;

	// Keep looking for a page break

	return placeholder->InsertPageBreak(info, strength);
}

#endif // PAGED_MEDIA_SUPPORT

/** Find and propagate explicit (column/page) breaks in table. */

OP_STATUS
TableContent::FindBreaks(LayoutInfo& info, LayoutProperties* cascade)
{
	BREAK_POLICY prev_page_break_policy = BREAK_AVOID;
	BREAK_POLICY prev_column_break_policy = BREAK_AVOID;

	for (TableListElement* element = (TableListElement*) layout_stack.First(); element; element = element->Suc())
	{
		BREAK_POLICY page_break_policy = CombineBreakPolicies(prev_page_break_policy, element->GetPageBreakPolicyBefore());
		BREAK_POLICY column_break_policy = CombineBreakPolicies(prev_column_break_policy, element->GetColumnBreakPolicyBefore());

		if (BreakForced(column_break_policy) || BreakForced(page_break_policy))
		{
			BREAK_TYPE break_type = BreakForced(page_break_policy) ? PAGE_BREAK : COLUMN_BREAK;

			if (!placeholder->PropagateBreakpoint(element->GetStaticPositionedY(), break_type, NULL))
				return OpStatus::ERR_NO_MEMORY;
		}

		prev_page_break_policy = BREAK_AVOID;
		prev_column_break_policy = BREAK_AVOID;

		if (element->IsRowGroup())
			for (TableRowBox* row = ((TableRowGroupBox*) element)->GetFirstRow();
				 row;
				 row = (TableRowBox*) row->Suc())
			{
				BREAK_POLICY page_break_policy = CombineBreakPolicies(prev_page_break_policy, row->GetPageBreakPolicyBefore());
				BREAK_POLICY column_break_policy = CombineBreakPolicies(prev_column_break_policy, row->GetColumnBreakPolicyBefore());

				if (BreakForced(column_break_policy) || BreakForced(page_break_policy))
				{
					BREAK_TYPE break_type = BreakForced(page_break_policy) ? PAGE_BREAK : COLUMN_BREAK;

					if (!placeholder->PropagateBreakpoint(element->GetStaticPositionedY() + row->GetStaticPositionedY(), break_type, NULL))
						return OpStatus::ERR_NO_MEMORY;
				}

				prev_page_break_policy = row->GetPageBreakPolicyAfter();
				prev_column_break_policy = row->GetColumnBreakPolicyAfter();
			}

		prev_page_break_policy = CombineBreakPolicies(prev_page_break_policy, element->GetPageBreakPolicyAfter());
		prev_column_break_policy = CombineBreakPolicies(prev_column_break_policy, element->GetColumnBreakPolicyAfter());
	}

	if (BreakForced(prev_column_break_policy) || BreakForced(prev_page_break_policy))
	{
		BREAK_TYPE break_type = BreakForced(prev_page_break_policy) ? PAGE_BREAK : COLUMN_BREAK;

		if (!placeholder->PropagateBreakpoint(GetHeight(), break_type, NULL))
			return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/** Reset layout state. */

void
TableContent::ResetLayout(const HTMLayoutProperties& props)
{
	short dummy;
	short top_border_width;
	short bottom_border_width;
	GetBorderWidths(props, top_border_width, dummy, dummy, bottom_border_width, FALSE);

	if (!packed.use_old_row_heights)
	{
		height = LayoutCoord(0);

		if (props.box_sizing == CSS_VALUE_content_box)
			height += LayoutCoord(top_border_width + bottom_border_width) + props.padding_top + props.padding_bottom;

		if (reflow_state->css_height > 0)
			height += reflow_state->css_height;
	}

	reflow_state->reflow_element = NULL;
	reflow_state->last_top_caption = NULL;
	reflow_state->last_row_group = NULL;

	reflow_state->row_position = LayoutCoord(top_border_width);
	if (!GetCollapseBorders())
		reflow_state->row_position += props.padding_top;

	reflow_state->next_col_number = 0;

	layout_stack.RemoveAll();

	if (props.margin_bottom)
		reflow_state->margin_state.CollapseWithBottomMargin(props, TRUE);

	// Reset rowspan state for columns

	for (unsigned int i = 0; i < column_count; i++)
	{
		column_widths[i].column_box = NULL;
		column_widths[i].packed2.rowspans = 0;
	}
}

/** Unless its computed value is auto, calculate the actual value of the
	CSS height property: start with the CSS computed height, then apply
	constraints and resolve percentage values.

	NOTE: Asserts that the placeholder has successfully initialised its reflow
	state.

	@return The actual value of the CSS height property, or
	CONTENT_HEIGHT_AUTO if its computed value is auto. */

LayoutCoord
Content::CalculateCSSHeight(LayoutInfo& info, LayoutProperties* cascade)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord css_height = props.content_height;
	LayoutCoord dummy;

	/* We need to assert this to be able to safely use the reflow state of the
	   placeholder and the reflow states of the ancestors. */
	OP_ASSERT(placeholder->GetReflowState());

	if (placeholder->ResolveHeightAndVerticalMargins(cascade, css_height, dummy, dummy, info) && css_height != CONTENT_HEIGHT_O_SIZE ||
		css_height > 0 ||
		css_height == 0 && !props.GetHeightIsPercent())
		return css_height;

	if (css_height != CONTENT_HEIGHT_AUTO && css_height != CONTENT_HEIGHT_O_SIZE)
	{
		/* Resolve percentage value.
		   The CSS spec is pretty clear on how to find the containing block of
		   a box. But for the sake of getting the "right" height, we have to
		   add this:

		   In quirks mode, the containing block of a child of the BODY element
		   is the initial containing block.

		   The containing block of a replaced element or a table box is the
		   nearest ancestor with non-auto height or width, or the nearest
		   ancestor box that establishes a new block formatting context,
		   whichever comes first.

		   The containing block of a table-cell box is its table box.

		   In quirks mode, if the containing block is the initial containing
		   block, the BODY margins are subtracted from the containing block
		   size.

		   The spec says that we should use the computed height of the
		   containing block when resolving percentage height values of a
		   box - with one exception: Boxes with position absolute or fixed
		   should use the actual height of the containing block instead. We
		   extend this exception and apply it to boxes that have a table-cell
		   as their containing block, but only in quirks mode.

		   Table cells may have a percentage height relative to their parent
		   table content.

		   In strict mode, a computed value of a relative table cell height
		   will affect its percentage height children. If the table cell
		   height is declared auto, percentage height will be computed against
		   the table row. This is done in order to be more site compatible,
		   but without falling into old row height algorithm also in strict mode,
		   which leads to compute percentage heights against the actual height of
		   the row. IE9 doesn't use old row height algorithm. If the table cell
		   height is declared auto, the percentage height of a child of the table
		   cell will be resolved to auto. That is spec compliant.
		   Gecko and Webkit however do use ORH algorithm, but they differ in
		   conditions when to use it. Gecko has also differences between quirks
		   and strict mode.

		   In quirks mode, a computed value of a relative height table cell will _not_
		   affect percentage children heights. If at least one of: table height,
		   row height or cell height is specified, those percentages will be
		   calculated relative to the actual row height. */

		/* percentage heights of abs_pos are handled in ResolveHeightAndVerticalMargins. */

		OP_ASSERT(!placeholder->IsAbsolutePositionedBox());

		if (cascade->flexbox)
			// Luckily there's no flexbox support in IE6, so this one's easy.

			return props.ResolvePercentageHeight(props.containing_block_height);

		LayoutCoord border_padding(0);
		BOOL strict_mode = info.hld_profile->IsInStrictMode();

		/* Relative positioned boxes should pass auto height ancestors in quirks. This will align this
		   specific quirk with IE7. See bug 283993. */

		BOOL pass_auto_height_ancestors = !strict_mode && (IsReplaced() || GetTableContent() || placeholder->IsPositionedBox());

		while (cascade)
		{
			Container* cur_container = cascade->container;

			if (cur_container)
			{
				css_height = cur_container->GetCSSHeight();
				cascade = cur_container->GetPlaceholder()->GetCascade();

				/* Quirks mode auto height force exception.

				   This aligns us closest to Gecko. Also not that far from Webkit.
				   Consider the following case
				   <td>
					<div style="height:100%">
					 <div id="inner" style="height:100%">
					 </div>
					</div>
				   </td>
				   where the involved table has specified height (or the involved row or
				   the cell). In the reflow passes where the final row height is unknown,
				   both divs heights will be auto. However in the last (typically third)
				   pass we will resolve the 'inner' div's height to auto if the cell's
				   content height is not smaller than the cell's height in the table
				   (typically the row's height if the cell's rowspan is 1). The rationale
				   for that is that there is no point to honor the percentages if the final
				   cell height is a result of its contents only. If, however, the cell
				   height is greater (because of specified height or the fact that some
				   other cell has inflated the row), the percentages should be used to keep
				   the desired proportions.

				   NOTE: We don't need to force auto height when the outer div's container
				   is scrollable, because scrollables get zero height in the reflow passes
				   where the row height is yet unknown, so they don't contribute to the
				   final row height, which is then a base to resolve percentages.

				   NOTE: Gecko seems to not honor this exeption in case of table cell's
				   children, which seems inconsistent. But this is quirks :). */

				if (!strict_mode && cascade->GetProps()->GetHeightIsPercent() && !cur_container->GetScrollableContainer())
					if (Container* parent_container = cascade->container)
					{
						Content_Box* parent_container_placeholder = parent_container->GetPlaceholder();

						if (parent_container_placeholder->IsTableCell())
							if (static_cast<TableRowBox*>(parent_container_placeholder->GetCascade()->Pred()->html_element->GetLayoutBox())->UseOldRowHeight())
							{
								TableCellBox* table_cell = static_cast<TableCellBox*>(parent_container_placeholder);

								if (table_cell->GetHeight() <= table_cell->GetOldCellContentHeight())
									return CONTENT_HEIGHT_AUTO;
							}
					}

			}
			else
			{
				/* Table cells resolve height wrt their containing table. This
				   replicates IE behaviour. */
				TableContent* tc = cascade->table;

				if (tc)
				{
					css_height = tc->GetCSSHeight();
					cascade = tc->GetPlaceholder()->GetCascade();
				}
				else
					break;
			}

			const HTMLayoutProperties& container_props = *cascade->GetProps();
			HTML_Element *elm = cascade->html_element;
			Markup::Type type = elm->Type();
			Box* box = elm->GetLayoutBox();

			/* content inside table cells in quirks mode use the old row heights algorithm. */

			if (css_height != CONTENT_HEIGHT_AUTO && (strict_mode || !box->IsTableCell()))
			{
				css_height -= border_padding;

				if (css_height < 0)
					css_height = LayoutCoord(0);

				break;
			}

			if (cur_container && container_props.content_height < 0)
				/* Couldn't resolve height. The container is typically either auto or
				   percentual. This means that this container must be marked as
				   unskippable for future reflows. */

				cur_container->SetHeightAffectsChildren();

			border_padding += container_props.padding_top +
				container_props.padding_bottom +
				LayoutCoord(container_props.border.top.width + container_props.border.bottom.width);

			if (!strict_mode && (type == Markup::HTE_BODY || type == Markup::HTE_HTML))
				border_padding += container_props.margin_top + container_props.margin_bottom;

			if (box->IsTableCell())
			{
				OP_ASSERT(cascade->Pred()->html_element->GetLayoutBox()->IsTableRow());

				TableContent* table = cascade->Pred()->table;
				TableRowBox* row = (TableRowBox*) cascade->Pred()->html_element->GetLayoutBox();

				if (strict_mode)
				{
					if (container_props.content_height == CONTENT_HEIGHT_AUTO)
					{
						/* The table cell has declared auto height. Check if the table row has a
						   specified height. This behaviour intends to be more site compat.
						   Gecko and Webkit use the old row height algorithm also
						   in strict mode. */

						LayoutCoord r_h = cascade->Pred()->GetProps()->content_height;

						if (r_h != CONTENT_HEIGHT_AUTO)
						{
							if (r_h >= 0)
								css_height = r_h;
							else
							{
								LayoutCoord t_h = table->GetCSSHeight();
								if (t_h >= 0)
									return DoublePercentageToLayoutCoord(LayoutFixed(-r_h), LayoutFixed(-props.content_height), t_h);
							}
						}
					}
				}
				else
				{
					if ((table->GetCSSHeight() >= 0 || row->HasSpecifiedHeight()))
					{
						if (row->UseOldRowHeight())
						{
							css_height = static_cast<TableCellBox*>(box)->GetHeight() - border_padding;

							if (css_height < 0)
								css_height = LayoutCoord(0);
						}
						else
						{
							static_cast<TableCellBox*>(box)->SetHasContentWithPercentageHeight();
							table->SetHasContentWithPercentageHeight();

							/* A content that qualifies for ORH (old row height) algorithm.
							   Let its height be auto in the reflow passes, in which the row
							   height is yet unknown. Two exceptions, in which case we use zero
							   height are:
							   - A container with non visible overflow. We don't want its content
							   to affect the final row height.
							   - ReplacedContent.
							   This content might get non auto and non zero height in (normally)
							   third reflow pass, where the row height will have been determined. */

							return GetScrollable() || IsReplaced() ? LayoutCoord(0) : CONTENT_HEIGHT_AUTO;
						}
					}
				}
				break;
			}

			if (pass_auto_height_ancestors)
			{
				OP_ASSERT(!strict_mode); // pass_auto_height_ancestor removed for strict mode. IE7->IE8 change.

				if (box->IsAbsolutePositionedBox() ||
					box->IsFloatingBox() ||
					box->IsTableCell() ||
					box->IsInlineBlockBox() ||
					box->IsTableCaption() ||
					box->IsFlexItemBox() ||
					container_props.content_width != CONTENT_WIDTH_AUTO)
					// overflow != visible is not considered a block-formatting context qualifier in quirks mode.

					break;
			}
			else
				if (strict_mode || type != Markup::HTE_BODY && type != Markup::HTE_HTML)
					break;
		}

		// If the height of the containing block is 'auto' then this is also.

		if (css_height >= 0)
		{
			css_height = props.ResolvePercentageHeight(css_height);

			if (css_height <= 0)
				return LayoutCoord(0);
			else
				return css_height;
		}
	}

	return CONTENT_HEIGHT_AUTO;
}

/** Force reflow of this element later. */

void
Content::ForceReflow(LayoutInfo& info)
{
	if (!GetReflowForced())
	{
		SetReflowForced(TRUE);
		info.workplace->SetReflowElement(GetHtmlElement());
	}
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
TableContent::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	/* Automatic table layout algorithm is specified and documented at
	   ../documentation/tables.html */

	const HTMLayoutProperties& props = *cascade->GetProps();
	BOOL columns_changed = FALSE;

	OP_ASSERT(!reflow_state);

	if (!InitialiseReflowState())
		return OpStatus::ERR_NO_MEMORY;

	// Note: In MSIE fixed table layout works even if width is auto.

	if (props.table_layout == CSS_VALUE_fixed && props.content_width != CONTENT_WIDTH_AUTO)
		reflow_state->fixed_layout = FIXED_LAYOUT_ON;
	else
		reflow_state->fixed_layout = FIXED_LAYOUT_OFF;

	if (GetCollapseBorders())
	{
		reflow_state->border_spacing_vertical = 0;
		reflow_state->border_spacing_horizontal = 0;
	}
	else
	{
		reflow_state->border_spacing_vertical = props.border_spacing_vertical;
		reflow_state->border_spacing_horizontal = props.border_spacing_horizontal;
	}

	reflow_state->old_height = height;

	if (column_count > 0)
	{
		reflow_state->column_calculation = OP_NEWA(TableColumnCalculation, column_count);
		if (!reflow_state->column_calculation)
			return OpStatus::ERR_NO_MEMORY;

		for (unsigned int i = 0; i < column_count; ++i)
		{
			/* Store min/max/percentage/desired widths. After this reflow pass we may compare
			   them to the new values. If there is any difference, another reflow pass is required.

			   All tables require two layout passes. This even applies to fixed table layout,
			   since neither table width nor column widths are known at this point during the first
			   pass. Need to lay out the first row before we can calculate them. */

			reflow_state->column_calculation[i].used_min_width = column_widths[i].min_width;
			reflow_state->column_calculation[i].used_max_width = column_widths[i].max_width;
			reflow_state->column_calculation[i].used_percent = LayoutFixed(column_widths[i].packed.has_percent ? column_widths[i].packed.percent : -1);
			reflow_state->column_calculation[i].used_desired_width = column_widths[i].desired_width;
		}
	}

	packed.relative_height = props.HeightIsRelative(placeholder->IsAbsolutePositionedBox()) ||
		cascade->flexbox && cascade->flexbox->IsVertical();

	reflow_state->css_height = CalculateCSSHeight(info, cascade);

	BOOL reflow_content = FALSE;

	if (packed.relative_height && reflow_state->css_height != old_css_height)
	{
		reflow_content = TRUE; // computed height changed, need to reflow table
		packed.use_old_row_heights = 0;
		reflow_state->css_height_changed = TRUE; // pass along that we need to reflow children and content
	}

	old_css_height = reflow_state->css_height;

	CalculateCaptionMinMaxWidth(reflow_state->used_caption_min_width, reflow_state->used_caption_max_width);

	BOOL width_changed = CalculateTableWidth(cascade, info);

	if (width_changed || packed.recalculate_columns)
		columns_changed = CalculateColumnWidths(info);

	reflow_state->collapsed_width = LayoutCoord(0);

	for (int i = 0; i < column_count; i++)
		if (column_widths[i].IsVisibilityCollapsed())
			reflow_state->collapsed_width += column_widths[i].width + GetHorizontalCellSpacing();

	width -= reflow_state->collapsed_width;

	if (width < 0)
		width = LayoutCoord(0);

	packed.recalculate_columns = 0;

	if (columns_changed)
		packed.use_old_row_heights = 0;
	else
	{
		reflow_state->caption_min_width = reflow_state->used_caption_min_width;
		reflow_state->caption_max_width = reflow_state->used_caption_max_width;
	}

	if (columns_changed || reflow_content || width_changed ||
		props.GetPaddingTopIsPercent() || props.GetPaddingBottomIsPercent() ||
		!packed.minmax_calculated && placeholder->NeedMinMaxWidthCalculation(cascade))
		return OpBoolean::IS_TRUE;

	return OpBoolean::IS_FALSE;
}

/** Reflow box and children. */

/* virtual */ LAYST
TableContent::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	HTML_Element* html_element = cascade->html_element;

	if (first_child && first_child != html_element)
		return placeholder->LayoutChildren(cascade, info, first_child, start_position);
	else
	{
		// reflow_state already created in ComputeSize().

		HTMLayoutProperties* local_props = cascade->GetProps();

		ModifyBorderProps(info, cascade);

		CalculateRowDimensions(*local_props, row_left_offset, reflow_state->row_width);

		reflow_state->reflow = TRUE;

		reflow_state->highest_col_number_collapsed = (unsigned short) MAX(column_count - 1, 0);

		/* Determine the need for min/max width calculation.

		   In automatic table layout mode we need min/max widths to calculate
		   our own size.

		   If an ancestor wants our min/max widths to calculate its own size,
		   we also need to calculate them.

		   In quirks mode, percentual height may enable "use old row heights"
		   mode, which should not kick in in the same reflow pass as when
		   content is changed (or added or removed). Now, this doesn't mean
		   that anyone cares about min/max widths, but let's calculate them,
		   instead of introducing a parallel state machinery especially for
		   percentual height handling. */

		reflow_state->calculate_min_max_widths = !packed.minmax_calculated &&
			(GetFixedLayout() == FIXED_LAYOUT_OFF ||
			 placeholder->NeedMinMaxWidthCalculation(cascade) ||
			 packed.relative_height && !info.hld_profile->IsInStrictMode());

		if (!packed.minmax_calculated)
		{
			reflow_state->true_table = UNDETERMINED;

			minimum_width = LayoutCoord(0);
			normal_minimum_width = LayoutCoord(0);
			constrained_maximum_width = LayoutCoord(-1);
			maximum_width = LayoutCoord(0);

			// Reset column widths

			for (unsigned int i = 0; i < column_count; i++)
				column_widths[i].ResetWidths();
		}
		else
			reflow_state->true_table = (TRUE_TABLE_MODE) packed.true_table;

		if (local_props->max_height >= 0 && reflow_state->css_height > 0)
			if (reflow_state->css_height > local_props->max_height)
				reflow_state->css_height = local_props->max_height;

		if (local_props->min_height > 0 && reflow_state->css_height < local_props->min_height)
			reflow_state->css_height = local_props->min_height;

		placeholder->SetHasAutoHeight(TRUE);

		if (
#ifdef PAGED_MEDIA_SUPPORT
			info.paged_media != PAGEBREAK_OFF ||
#endif // PAGED_MEDIA_SUPPORT
			cascade->multipane_container)
		{
			BOOL avoid_page_break_inside;
			BOOL avoid_column_break_inside;

			cascade->GetAvoidBreakInside(info, avoid_page_break_inside, avoid_column_break_inside);

			packed.avoid_page_break_inside = !!avoid_page_break_inside;
			packed.avoid_column_break_inside = !!avoid_column_break_inside;
		}

#ifdef SUPPORT_TEXT_DIRECTION
		packed.rtl = local_props->direction == CSS_VALUE_rtl;
#endif

		packed.frame = TABLE_FRAME_not_specified;
		packed.rules = TABLE_RULES_not_specified;

		if (!GetCollapseBorders())
			if (html_element->Type() == Markup::HTE_TABLE)
			{
				BOOL has_html_border = html_element->HasNumAttr(Markup::HA_BORDER);
				if (has_html_border && long(html_element->GetAttr(Markup::HA_BORDER, ITEM_TYPE_NUM, (void*) 0)) == 0)
				{
					packed.frame = TABLE_FRAME_void;
					packed.rules = TABLE_RULES_none;
				}
				else if (has_html_border)
					packed.rules = TABLE_RULES_all;
			}

		ResetLayout(*local_props);

		placeholder->PropagateBottomMargins(info);

		return placeholder->LayoutChildren(cascade, info, NULL, LayoutCoord(0));
	}
}

/** Calculate table width. */

BOOL
TableContent::CalculateTableWidth(LayoutProperties* cascade, LayoutInfo& info)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord old_width = width;
	short dummy;
	short left_border_width;
	short right_border_width;
	LayoutCoord border_padding;
	LayoutCoord percent_based_padding(0);
	LayoutCoord scaled_normal_minimum_width = normal_minimum_width;
	BOOL width_is_auto = cascade->IsLayoutWidthAuto();
	Container* container = cascade->container;

	GetBorderWidths(props, dummy, left_border_width, right_border_width, dummy, FALSE);

	border_padding = LayoutCoord(left_border_width + right_border_width);

	if (!GetCollapseBorders())
	{
		border_padding += props.padding_left + props.padding_right;

		/* Any padding resolved from percentual values were not added to min/max values.
		   Need to be taken into consideration when comparing with min/max widths now. */

		percent_based_padding =
			(props.GetPaddingLeftIsPercent() ? props.padding_left : LayoutCoord(0)) +
			(props.GetPaddingRightIsPercent() ? props.padding_right : LayoutCoord(0));
	}

	if (cascade->flexbox)
	{
		// Flex item main sizes are calculated by the flex algorithm.

		width = ((FlexItemBox*) placeholder)->GetFlexWidth(cascade);

		if (props.box_sizing == CSS_VALUE_content_box)
			width -= border_padding;
	}
	else
	{
		LayoutCoord requested_width = props.content_width;

		/* Note here; IE and Mozilla consider explicitly setting width of table
		   to 0 to be bogus. That seems reasonable. */

		if (requested_width == 0 && GetFixedLayout() == FIXED_LAYOUT_OFF || info.column_stretch == COLUMN_STRETCH_MAXIMIZE)
		{
			width_is_auto = TRUE;
			requested_width = CONTENT_WIDTH_AUTO;
		}

		if (requested_width >= 0)
		{
			width = requested_width;

			if (info.doc->GetLayoutMode() != LAYOUT_NORMAL)
			{
				if (info.workplace->GetScaleAbsoluteWidthAndPos())
				{
					LayoutCoord layout_width = info.workplace->GetAbsScaleViewWidth();

					requested_width = (requested_width * layout_width) / info.workplace->GetNormalEraWidth();
					if (requested_width < 1)
						requested_width = LayoutCoord(1);

					scaled_normal_minimum_width = (scaled_normal_minimum_width * layout_width) / info.workplace->GetNormalEraWidth();
				}

				if (requested_width < scaled_normal_minimum_width)
					width = scaled_normal_minimum_width;
			}
		}
		else
		{
			LayoutCoord available_width = placeholder->GetAvailableWidth(cascade);

			if (width_is_auto)
			{
				width = available_width - (props.margin_left + props.margin_right);

				if (props.box_sizing == CSS_VALUE_content_box)
					width -= border_padding;
			}
			else
			{
				width = PercentageToLayoutCoord(LayoutFixed(-requested_width), available_width);

				if (LayoutFixed(-requested_width) < LAYOUT_FIXED_HUNDRED_PERCENT && width < scaled_normal_minimum_width && info.doc->GetLayoutMode() != LAYOUT_NORMAL)
					width = MIN(scaled_normal_minimum_width, available_width);
			}
		}

		// Width must be within the limits of min-width and max-width

		width = props.CheckWidthBounds(width);
	}

	OP_ASSERT(normal_minimum_width >= minimum_width);

	if (width < 0)
		width = LayoutCoord(0);

	/* 'width' has been relative to the box specified by box-sizing so
	   far. Convert it to border-box now. */

	if (props.box_sizing == CSS_VALUE_content_box)
		width += border_padding;
	else
		if (width < border_padding)
			// Avoid negative content box width

			width = border_padding;

	if (GetFixedLayout() == FIXED_LAYOUT_OFF)
	{
		if (width < minimum_width + percent_based_padding && (info.table_strategy == TABLE_STRATEGY_NORMAL || IsTrueTable()))
			// Must be at least as wide as minimum cell widths

			width = minimum_width + percent_based_padding;
		else
			if (info.column_stretch != COLUMN_STRETCH_MAXIMIZE &&
				(width_is_auto || info.doc->GetLayoutMode() != LAYOUT_NORMAL) &&
				!cascade->flexbox)
			{
				LayoutCoord max_width;

				if (!placeholder->IsAbsolutePositionedBox() && placeholder->IsBlockBox())
				{
					LayoutCoord orig_available_width = width_is_auto ? placeholder->GetAvailableWidth(cascade) : props.era_max_width;
					LayoutCoord available_width = orig_available_width;
					LayoutCoord min_height = placeholder->IsFloatingBox() ? LAYOUT_COORD_MAX : LayoutCoord(0);

					const LayoutCoord old_y = ((BlockBox*) placeholder)->GetY();
					const LayoutCoord old_x = ((BlockBox*) placeholder)->GetX();
					LayoutCoord new_y = old_y;
					LayoutCoord new_x = old_x;

					if (placeholder->IsFloatingBox())
						/* Just like for any other float, the top edge of a floated table
						   may not be above the top edge of any previous float. */

						if (FloatingBox* prev_last_float = cascade->space_manager->GetLastFloat())
						{
							LayoutCoord dummy_bfc_min_y(0);
							LayoutCoord bfc_y(0);
							LayoutCoord dummy_bfc_x(0);

							container->GetBfcOffsets(dummy_bfc_x, bfc_y, dummy_bfc_min_y);
							new_y = prev_last_float->GetBfcY() - prev_last_float->GetMarginTop() - bfc_y;
						}

					container->GetSpace(cascade->space_manager, new_y, new_x, available_width, scaled_normal_minimum_width + percent_based_padding, min_height);

					if (new_y > old_y && info.doc->GetLayoutMode() != LAYOUT_NORMAL)
					{
						// Table does not fit beside a float.
						// Try to figure out if it would have fit in the normal layout mode.

						const HTMLayoutProperties& container_props = *container->GetPlaceholder()->GetCascade()->GetProps();
						if (container_props.content_width > 0)
						{
							LayoutCoord new_available_width = container_props.content_width;
							new_x = old_x;
							new_y = old_y;
							container->GetSpace(cascade->space_manager, new_y, new_x, new_available_width, normal_minimum_width + percent_based_padding, min_height);

							if (new_y == old_y)
							{
								// The table would have fit in the normal layout mode.
								// Squeeze it to make it fit in MSR as well.

								new_x = old_x;
								new_y = old_y;
								available_width = orig_available_width;
								container->GetSpace(cascade->space_manager, new_y, new_x, available_width, minimum_width, min_height);
							}
						}
					}

					available_width -= props.margin_left + props.margin_right;

					if (available_width < minimum_width)
						max_width = minimum_width;
					else
						if (available_width > maximum_width && width_is_auto)
							max_width = maximum_width;
						else
							max_width = available_width;
				}
				else
					max_width = maximum_width;

				if (width > max_width + percent_based_padding)
				{
					// and no wider than maximum cell widths (if no width was specified)

					LayoutCoord min_borderbox_width = props.min_width;

					if (props.box_sizing == CSS_VALUE_content_box)
						min_borderbox_width += border_padding;

					width = MAX(max_width + percent_based_padding, min_borderbox_width);
				}
			}
	}
	else
	{
		/* Fixed table layout. The table needs to be at least as wide as the
		   sum of the columns with fixed width and all horizontal
		   border-spacing, padding and border. */

		LayoutCoord required_width = LayoutCoord(reflow_state->border_spacing_horizontal * (column_count + 1) + border_padding);

		for (unsigned int i = 0; i < column_count; i++)
			required_width += column_widths[i].desired_width;

		if (width < required_width)
			width = required_width;
	}

	return old_width != width;
}

/** Request reflow of table. */

void
TableContent::RequestReflow(LayoutInfo& info)
{
	ForceReflow(info);
	packed.recalculate_columns = 1;
}

/** Get column box at 'column'. */

TableColGroupBox*
TableContent::GetColumnBox(int column)
{
	if (column < GetColumnCount())
		return column_widths[column].column_box;

	return NULL;
}

/** Get column group start and stop. */

void
TableContent::GetColGroupStartStop(int column, int colspan, BOOL& colgroup_starts_left, BOOL& colgroup_stops_right)
{
//#warning Is this the right thing to do?
	if (column >= GetColumnCount())
		return;

	if (column > 0 &&
		column_widths[column].column_box &&
		column_widths[column].column_box != column_widths[column - 1].column_box)
		colgroup_starts_left = TRUE;

	column += colspan - 1;

	if (column + 1 < GetColumnCount() &&
		column_widths[column].column_box &&
		column_widths[column].column_box != column_widths[column + 1].column_box)
		colgroup_stops_right = TRUE;
}

/** Set desired table column width. */

void
TableContent::SetDesiredColumnWidth(const LayoutInfo& info, int column, int colspan, LayoutCoord desired_column_width, BOOL is_cell)
{
	if (packed.minmax_calculated)
		return;

	switch (GetFixedLayout())
	{
	case FIXED_LAYOUT_OFF:
		/* In automatic table layout we wait until cell layout is done (so that we get cell min/max
		   widths as well) before updating column widths. */

		break;

	case FIXED_LAYOUT_COMPLETED:
		// Column boxes may affect widths after the first row is completed. However, cell boxes may not.

		if (is_cell)
			break;

		// fall-through

	case FIXED_LAYOUT_ON:
		{
			unsigned int desired_is_auto = (desired_column_width == CONTENT_WIDTH_AUTO) ? 1 : 0;
			if (desired_is_auto)
				desired_column_width = LayoutCoord(0);

			int cell_count = colspan;
			BOOL is_percent = desired_column_width < 0;

			if (is_percent)
			{
				/* Ignore value if this is a cell and percent has been
				   previously specified in column. */

				if (column_widths[column].packed.has_percent && is_cell)
					return;

				desired_column_width = -desired_column_width;

				if (is_cell)
					// Cell percent may have stolen from columns - recalculate rest of column percent

					for (int i = column + colspan; i < column_count; i++)
						if (column_widths[i].column_box)
						{
							LayoutCoord col_width = column_widths[i].column_box->GetDesiredWidth();

							if (col_width != CONTENT_WIDTH_AUTO && col_width < 0)
							{
								col_width = -col_width;

								column_widths[i].packed.percent = col_width;
								column_widths[i].packed.has_percent = 1;
								column_widths[i].packed.is_auto = 0;
							}
						}
			}
			else
				if (is_cell && colspan > 1)
				{
					// A colspan > 1 in first row - subtract desired_width specified in column

					for (int i = 0; i < colspan; i++)
						if (!column_widths[column + i].packed.is_auto)
						{
							desired_column_width -= column_widths[column + i].desired_width;
							cell_count--;
						}

					if (desired_column_width < 0)
					{
						desired_column_width = LayoutCoord(0);
						desired_is_auto = 1;
					}
				}

			if (cell_count)
			{
				LayoutCoord col_desired_width = desired_column_width / LayoutCoord(cell_count);

				for (int i = 0; i < colspan; i++)
					if (!is_cell || (!column_widths[column + i].packed.has_percent && column_widths[column + i].packed.is_auto))
						// Cell boxes only affect the column width if a column box hasn't already set it.

						if (is_percent)
						{
							column_widths[column + i].packed.percent = col_desired_width;
							column_widths[column + i].packed.has_percent = 1;
							column_widths[column + i].packed.is_auto = 0;
							if (GetFixedLayout() == FIXED_LAYOUT_COMPLETED)
							{
								column_widths[column + i].desired_width = LayoutCoord(0);
								column_widths[column + i].min_width = LayoutCoord(0);
								column_widths[column + i].normal_min_width = LayoutCoord(0);
								column_widths[column + i].max_width = LayoutCoord(0);
							}
						}
						else
							if (column_widths[column + i].packed.is_auto)
							{
								cell_count--;

								if (cell_count)
									desired_column_width -= col_desired_width;
								else
									col_desired_width = desired_column_width;

								column_widths[column + i].desired_width = col_desired_width;
								column_widths[column + i].packed.is_auto = desired_is_auto;
								column_widths[column + i].min_width = col_desired_width;
								column_widths[column + i].normal_min_width = col_desired_width;
								column_widths[column + i].max_width = col_desired_width;
								if (GetFixedLayout() == FIXED_LAYOUT_COMPLETED)
								{
									column_widths[column + i].packed.percent = 0;
									column_widths[column + i].packed.has_percent = 0;
								}
							}
			}
			break;
		}
	}
}

/** Check if changes require a reflow. */

BOOL
TableContent::CheckChange(LayoutInfo& info)
{
	LayoutProperties* cascade = placeholder->GetCascade();
	BOOL needs_reflow = FALSE;

	if (packed.recalculate_columns)
		needs_reflow = TRUE;

	if (!packed.minmax_calculated)
		if (reflow_state->reflow)
		{
			if (column_count > reflow_state->new_column_count)
			{
				// Number of columns decreased in this reflow pass
				// FIXME: We may want to actually shrink the column array as well here.

				column_count = reflow_state->new_column_count;
				if (last_used_column > column_count)
					last_used_column = column_count;
				needs_reflow = TRUE;
			}

			if (reflow_state->max_colspan > 1)
				/* Since this table has cells with colspan > 1, we couldn't calculate column
				   min/max widths on the fly when the cells were added. Do it now. */

				CalculateColumnMinMaxWidths(info);

			if (column_count)
			{
				unsigned int i = 0;

				for (; i < column_count; ++i)
					if (reflow_state->column_calculation[i].used_min_width != column_widths[i].min_width ||
						reflow_state->column_calculation[i].used_max_width != column_widths[i].max_width ||
						((reflow_state->column_calculation[i].used_percent < LayoutFixed(0)) ^ (!column_widths[i].packed.has_percent)) ||
						column_widths[i].packed.has_percent && (unsigned int)LayoutFixedAsInt(reflow_state->column_calculation[i].used_percent) != column_widths[i].packed.percent ||
						reflow_state->column_calculation[i].used_desired_width != column_widths[i].desired_width)
						break;

				if (i < column_count)
				{
					needs_reflow = TRUE;

					if (HeightIsRelative())
						if (TableContent* table = placeholder->GetTable())
							table->ClearUseOldRowHeights();
				}
			}
			else
				// No columns means no actual table... But we may still have captions

				needs_reflow =
					reflow_state->caption_min_width != reflow_state->used_caption_min_width ||
					reflow_state->caption_max_width != reflow_state->used_caption_max_width;

			reflow_state->used_caption_min_width = reflow_state->caption_min_width;
			reflow_state->used_caption_max_width = reflow_state->caption_max_width;

			/* Must calculate correct min/max widths now, in case they are propagated
			   before the next reflow pass. */

			needs_reflow = ComputeMinMaxWidth(info, cascade) && GetFixedLayout() == FIXED_LAYOUT_OFF || needs_reflow;
		}

#ifdef DEBUG
	if (!reflow_state->calculate_min_max_widths && packed.minmax_calculated)
	{
		// If min/max widths were up-to-date before reflow, they'd better not have been touched now.

		for (unsigned int i = 0; i < column_count; ++i)
		{
			OP_ASSERT(reflow_state->column_calculation[i].used_min_width == column_widths[i].min_width);
			OP_ASSERT(reflow_state->column_calculation[i].used_max_width == column_widths[i].max_width);
			OP_ASSERT((reflow_state->column_calculation[i].used_percent < LayoutFixed(0)) == (!column_widths[i].packed.has_percent));
			OP_ASSERT(!column_widths[i].packed.has_percent || LayoutFixedAsInt(reflow_state->column_calculation[i].used_percent) == column_widths[i].packed.percent);
			OP_ASSERT(reflow_state->column_calculation[i].used_desired_width == column_widths[i].desired_width);
		}
	}
#endif // DEBUG

	return needs_reflow;
}

/** Propagate minimum and maximum widths from captions. */

void
TableContent::PropagateCaptionWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord max_width)
{
	if (!reflow_state->calculate_min_max_widths)
		return;

	if (reflow_state->caption_min_width < min_width)
		reflow_state->caption_min_width = min_width;

	if (reflow_state->caption_max_width < max_width)
		reflow_state->caption_max_width = max_width;
}

/** Propagate minimum and maximum cell widths to columns. */

void
TableContent::PropagateCellWidths(const LayoutInfo& info, unsigned short column, unsigned short colspan, LayoutCoord cell_desired_width, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width)
{
	if (!reflow_state->calculate_min_max_widths)
		return;

	if (reflow_state->max_colspan <= 1)
		/* If there are cells with colspan > 1, we need to update the column widths later, because
		   cells must be fed to the layout engine sorted by colspan. However, if there are no such
		   cells, we can update the column width right away. */

		UpdateColumnWidths(info, column, colspan, cell_desired_width, min_width, normal_min_width, max_width, TRUE);
}

/** Compute minimum and maximum widths for table. */

BOOL
TableContent::ComputeMinMaxWidth(const LayoutInfo& info, LayoutProperties* cascade)
{
	const LayoutCoord MaxTableWidth = LayoutCoord(SHRT_MAX);
	LayoutCoord spacing = LayoutCoord(reflow_state->border_spacing_horizontal);
	short dummy;
	short left_border_width;
	short right_border_width;
	LayoutCoord nonpercent_border_padding;
	const HTMLayoutProperties& props = *cascade->GetProps();

	GetBorderWidths(props, dummy, left_border_width, right_border_width, dummy, FALSE);

	nonpercent_border_padding = LayoutCoord(left_border_width + right_border_width);

	if (column_count && !GetCollapseBorders())
		nonpercent_border_padding +=
			(props.GetPaddingLeftIsPercent() ? LayoutCoord(0) : props.padding_left) +
			(props.GetPaddingRightIsPercent() ? LayoutCoord(0) : props.padding_right);

	// Recalculate minimum and maximum width of table

	LayoutCoord min_width = nonpercent_border_padding;
	LayoutCoord normal_min_width = nonpercent_border_padding;
	LayoutCoord old_maximum_width = maximum_width;
	LayoutCoord old_constrained_maximum_width = constrained_maximum_width;

	constrained_maximum_width = LayoutCoord(-1);
	maximum_width = nonpercent_border_padding;

	if (info.flexible_columns)
		/* In flexible columns mode, a table row is allowed to wrap. As such, the minimum width of
		   the table is the minimum of the widest cell (plus table border and padding), not the sum
		   of all column minimum widths. */

		min_width = reflow_state->colspanned_min_width + spacing;

	if (column_count)
	{
		const LayoutFixed max_percentage = LAYOUT_FIXED_HUNDRED_PERCENT;

		if (GetFixedLayout() == FIXED_LAYOUT_OFF)
		{
			// Total percentage on all columns combined

			LayoutFixed percent = LayoutFixed(0);

			// Accumulated max width of all columns that don't have a percentage set

			LayoutCoord non_percent_max_width(0);

			// Largest max width of the columns whose percentage was ignored

			LayoutCoord ignored_percent_max_width(0);

			unsigned int last_referenced_column = last_used_column;

			for (unsigned int i = 0; i < last_referenced_column; ++i)
			{
				LayoutCoord column_max_width;

				if (info.flexible_columns)
				{
					if (min_width < column_widths[i].min_width + spacing)
						min_width = column_widths[i].min_width + spacing;
				}
				else
					min_width += column_widths[i].min_width + spacing;

				normal_min_width += column_widths[i].normal_min_width + spacing;

				if (column_widths[i].desired_width > 0 &&
					(column_widths[i].desired_width > column_widths[i].max_width || column_widths[i].column_box && column_widths[i].column_box->GetDesiredWidth() > 0))
					column_max_width = MAX(column_widths[i].desired_width, column_widths[i].normal_min_width);
				else
					/* Yes; when a cell has desired width, its maximum width will be set to it, and
					   thus, if its minimum width exceeds this value, it will be larger than than
					   maximum width. */

					column_max_width = MAX(column_widths[i].max_width, column_widths[i].normal_min_width);

				maximum_width += column_max_width;

				if (column_widths[i].packed.has_percent)
				{
					if (percent + LayoutFixed(column_widths[i].packed.percent) > max_percentage)
						if (percent >= max_percentage && ignored_percent_max_width < column_max_width)
							/* When 100 or more percent of the table width already has been used by
							   previous columns, ignore percentage of this and all remaining
							   percent-based columns, but find the largest max width among them. */

							ignored_percent_max_width = column_max_width;

					percent += LayoutFixed(column_widths[i].packed.percent);

					if (column_widths[i].packed.percent == 0)
						/* Percent values can never be satisfied for this table; make it as wide as
						   possible - infinite max width */

						maximum_width = MaxTableWidth;
				}
				else
					non_percent_max_width += column_max_width;
			}

			if (percent != LayoutFixed(0) && props.content_width == CONTENT_WIDTH_AUTO)
			{
				/* Pay attention to percent-based columns. These are special in the sense that they don't
				   affect maximum width propagation if there is an ancestor table (with
				   table-layout:auto). Such are the ways of automatic table layout, and surprisingly enough,
				   all major browser engines have reached a fairly agreeing behavior here.

				   Even if percent-based columns should be ignored for maximum width propagation, they will
				   still affect the actual width calculation of the table, i.e. if the containing block is wide
				   enough, then the true table maximum width (maximum_width) will be honored - it's just that
				   it won't be propagated to ancestors. */

				if (placeholder->NeedMinMaxWidthCalculation(cascade))
				{
					LayoutProperties* ancestor_cascade = cascade;
					TableContent* ancestor_table = ancestor_cascade->table;

					while (ancestor_table)
					{
						if (ancestor_table->GetFixedLayout() == FIXED_LAYOUT_OFF)
						{
							/* Found an ancestor table with table-layout:auto - constrain
							   propagated maximum width. */

							constrained_maximum_width = maximum_width;
							break;
						}

						ancestor_cascade = ancestor_table->GetPlaceholder()->GetCascade();
						ancestor_table = ancestor_cascade->table;
					}
				}

				/* First find out how wide the table has to be to keep cell content
				   in all columns with non-percent widths from wrapping, while
				   still satisfying percentage widths. */

				LayoutCoord new_max_width;

				if (non_percent_max_width)
				{
					if (percent < max_percentage)
					{
						new_max_width = ReversePercentageToLayoutCoordRoundDown(max_percentage - percent, non_percent_max_width);

						if (maximum_width < new_max_width)
							maximum_width = new_max_width;
					}
					else
						// Percent values can never be satisfied but the table should be as wide as possible

						maximum_width = MaxTableWidth;
				}
				else
					if (ignored_percent_max_width)
					{
						/* Pretend that the largest max width of all columns whose percentage was
						   cancelled is 1% of the total width. [For consideration: Is this an important
						   feature in IE, or just a bug? The goal in step one above is achieved without
						   this hack.] */

						OP_ASSERT(percent > max_percentage);

						new_max_width = ignored_percent_max_width * LayoutCoord(100);

						if (maximum_width < new_max_width)
							maximum_width = new_max_width;
					}

				/* Step two: make sure that max width also is large enough to keep
				   cell content in all percent-based columns from wrapping. */

				LayoutFixed percent_count = LayoutFixed(0);

				for (unsigned int i = 0; i < column_count && percent_count < max_percentage; ++i)
					if (column_widths[i].packed.has_percent)
					{
						LayoutFixed col_percent = LayoutFixed(column_widths[i].packed.percent);

						if (percent_count + col_percent > max_percentage)
						{
							col_percent = max_percentage - percent_count;
							percent_count = max_percentage;
						}
						else
							percent_count += col_percent;

						if (col_percent == LayoutFixed(0))
						{
							// Percent values can never be satisfied but the table should be as wide as possible.

							maximum_width = MaxTableWidth;
							break;
						}

						new_max_width = ReversePercentageToLayoutCoord(col_percent, column_widths[i].max_width);

						if (maximum_width < new_max_width)
							maximum_width = new_max_width;
					}
			}

			min_width += spacing;
			normal_min_width += spacing;
			maximum_width += spacing * LayoutCoord(last_referenced_column + 1);

			if (constrained_maximum_width != -1)
				constrained_maximum_width += spacing * LayoutCoord(last_referenced_column + 1);
		}
		else
		{
			/* Fixed table layout. Min/max widths are not used to calculate the
			   actual size of the table then, but they are still needed for
			   width propagation to ancestors. */

			LayoutCoord percent_min_width(0);
			LayoutCoord percent_normal_min_width(0);
			LayoutCoord percent_max_width(0);
			LayoutCoord auto_min_width(0);
			LayoutCoord auto_normal_min_width(0);
			LayoutCoord auto_max_width(0);
			LayoutCoord total_fixed_width(0);
			unsigned int auto_width_columns = 0;
			LayoutFixed total_percent = LayoutFixed(0);

			for (unsigned int i = 0; i < column_count; ++i)
				if (column_widths[i].packed.has_percent)
					total_percent += LayoutFixed(column_widths[i].packed.percent);

			/* There are three different types of columns: auto width, fixed
			   width, and percentual width. Go through each column and find the
			   total width requirement for each column type. */

			for (unsigned int i = 0; i < column_count; ++i)
				if (column_widths[i].packed.has_percent)
				{
					// This column has percentual width.

					LayoutFixed percent = LayoutFixed(column_widths[i].packed.percent);

					if (percent > LayoutFixed(0))
					{
						/* If total_percent exceeds 100, reduce the percentages proportionally,
						   so they sum up to 100. */

						LayoutFixed the_whole = MAX(total_percent, max_percentage);

						LayoutCoord column_min_width = ReversePercentageOfPercentageToLayoutCoord(percent, column_widths[i].min_width, the_whole);
						LayoutCoord column_normal_min_width = ReversePercentageOfPercentageToLayoutCoord(percent, column_widths[i].normal_min_width, the_whole);
						LayoutCoord column_max_width = ReversePercentageOfPercentageToLayoutCoord(percent, column_widths[i].max_width, the_whole);

						if (percent_min_width < column_min_width)
							percent_min_width = column_min_width;

						if (percent_normal_min_width < column_normal_min_width)
							percent_normal_min_width = column_normal_min_width;

						if (percent_max_width < column_max_width)
							percent_max_width = column_max_width;
					}
				}
				else
					if (column_widths[i].desired_width > 0 || !column_widths[i].packed.is_auto)
						// This column has fixed width.

						total_fixed_width += column_widths[i].desired_width;
					else
					{
						// This column has automatic width.

						auto_width_columns ++;

						if (auto_min_width < column_widths[i].min_width)
							auto_min_width = column_widths[i].min_width;

						if (auto_normal_min_width < column_widths[i].normal_min_width)
							auto_normal_min_width = column_widths[i].normal_min_width;

						if (auto_max_width < column_widths[i].max_width)
							auto_max_width = column_widths[i].max_width;
					}

			/* Always include width required by fixed width columns. Even if
			   the table width is fixed, it will be overridden if the sum of
			   the fixed-width columns in the table is larger. */

			min_width += total_fixed_width;
			normal_min_width += total_fixed_width;
			maximum_width += total_fixed_width;

			if (props.content_width < 0 || cascade->flexbox)
			{
				BOOL calculate_min_widths = cascade->flexbox || AffectsFlexRoot();

				/* If table width isn't fixed, also make room for auto width
				   and percent width columns.

				   Only do this (properly) for FlexRoot width propagation. It
				   would indeed make sense to do this (properly) for any kind
				   of width propagation (shrink-to-fit, tables, as well as
				   FlexRoot), but CORE-30309 tells us that it would make us too
				   incompatible with the others.

				   Unless we're propagating for FlexRoot, we will therefore
				   only propagate the maximum width requirements from auto
				   width and percent width columns, while we leave minimum
				   width (and normal minimum width) unaffected. */

				if (auto_width_columns > 0)
				{
					/* Space is distributed evenly among auto-width columns, so
					   they need at least the width of the widest column multiplied
					   with the number of columns. */

					if (calculate_min_widths)
					{
						auto_min_width *= auto_width_columns;
						min_width += auto_min_width;

						auto_normal_min_width *= auto_width_columns;
						normal_min_width += auto_normal_min_width;
					}

					auto_max_width *= auto_width_columns;
					maximum_width += auto_max_width;
				}

				LayoutFixed unused_percent = total_percent >= max_percentage ? LayoutFixed(0) : max_percentage - total_percent;

				if (unused_percent > LayoutFixed(0))
				{
					/* The auto and fixed width columns must fit within the space
					   not used by percentual width columns. How wide would the
					   table have to be achieve that? */

					LayoutCoord nonpercent_width;

					if (calculate_min_widths)
					{
						nonpercent_width = ReversePercentageToLayoutCoordRoundDown(unused_percent, total_fixed_width + auto_min_width);
						if (min_width < nonpercent_width)
							min_width = nonpercent_width;

						nonpercent_width = ReversePercentageToLayoutCoordRoundDown(unused_percent, total_fixed_width + auto_normal_min_width);
						if (normal_min_width < nonpercent_width)
							normal_min_width = nonpercent_width;
					}

					nonpercent_width = ReversePercentageToLayoutCoordRoundDown(unused_percent, total_fixed_width + auto_max_width);
					if (maximum_width < nonpercent_width)
						maximum_width = nonpercent_width;
				}

				// Make room for percent width columns.

				LayoutFixed used_percent = max_percentage - unused_percent;

				if (used_percent > LayoutFixed(0))
				{
					if (used_percent < max_percentage && !auto_width_columns && !total_fixed_width)
					{
						/* Percentual width columns may be stretched to fill the entire table,
						   unless there are more suitable columns (auto-width or fixed-width
						   columns) that can receive excess space. In that case, we need to scale
						   down the min/max width requirements previously calculated, as those
						   calculations were based on the assumption that the total percentage of
						   all percent-based columns was 100. */

						if (calculate_min_widths)
						{
							percent_min_width = PercentageToLayoutCoordCeil(used_percent, percent_min_width);
							percent_normal_min_width = PercentageToLayoutCoordCeil(used_percent, percent_normal_min_width);
						}

						percent_max_width = PercentageToLayoutCoordCeil(used_percent, percent_max_width);
					}

					if (calculate_min_widths)
					{
						if (min_width < percent_min_width)
							min_width = percent_min_width;

						if (normal_min_width < percent_normal_min_width)
							normal_min_width = percent_normal_min_width;
					}

					if (maximum_width < percent_max_width)
						maximum_width = percent_max_width;
				}
			}

			/* Add horizontal border spacing between each column and between
			   the table and the first and the last column. */

			LayoutCoord border_spacing_total = LayoutCoord((column_count + 1) * spacing);

			min_width += border_spacing_total;
			normal_min_width += border_spacing_total;
			maximum_width += border_spacing_total;
		}
	}
	else
	{
		min_width += reflow_state->used_caption_min_width;
		normal_min_width += reflow_state->used_caption_min_width;
		maximum_width += reflow_state->used_caption_max_width;
	}

	if (nonpercent_border_padding && props.box_sizing == CSS_VALUE_content_box)
	{
		// Convert to content-box.

		min_width -= nonpercent_border_padding;
		normal_min_width -= nonpercent_border_padding;
		maximum_width -= nonpercent_border_padding;
	}

	// Find width property, constrained to non-percentual min-width and max-width.

	LayoutCoord content_width = props.content_width > 0 ? props.CheckWidthBounds(props.content_width, TRUE) : props.content_width;

	if (content_width > 0 && content_width < min_width)
		/* The width of the table must never be less than its minimum width. For automatic table
		   layout, this is the sum of the minimum widths propagated from all columns in the
		   table. For fixed table layout, it is the sum of the columns with fixed width. */

		content_width = min_width;

	if (GetFixedLayout() == FIXED_LAYOUT_OFF)
	{
		// Automatic table layout

		LayoutCoord computed_nonpercent_min_width = props.GetMinWidthIsPercent() ? LayoutCoord(0) : props.min_width;
		LayoutCoord computed_nonpercent_max_width = props.GetMaxWidthIsPercent() ? LayoutCoord(-1) : props.max_width;

		// Constrain minimum width to min-width and width, if layout mode is normal.

		if (info.doc->GetLayoutMode() == LAYOUT_NORMAL)
			/* Don't let the width property affect minimum content width if this table is
			   a flex item, as the flex algorithm is allowed to shrink the final width to
			   less than that. */

			if (content_width > 0 && min_width < content_width && !cascade->flexbox)
				min_width = content_width;
			else
				if (min_width < computed_nonpercent_min_width)
					min_width = computed_nonpercent_min_width;

		/* Constrain normal minimum width to min-width and width, but don't let the width
		   property affect minimum content width if this table is a flex item, as the
		   flex algorithm is allowed to shrink the final width to less than that. */

		if (content_width > 0 && normal_min_width < content_width && !cascade->flexbox)
			normal_min_width = content_width;
		else
			if (normal_min_width < computed_nonpercent_min_width)
				normal_min_width = computed_nonpercent_min_width;

		// Constrain maximum width to min-width and width, if table strategy is normal.

		if (info.table_strategy == TABLE_STRATEGY_NORMAL)
			if (content_width > 0)
				maximum_width = MAX(normal_min_width, content_width);
			else
			{
				if (computed_nonpercent_max_width >= 0 && maximum_width > computed_nonpercent_max_width)
					maximum_width = MAX(normal_min_width, computed_nonpercent_max_width);

				if (maximum_width < computed_nonpercent_min_width)
					maximum_width = MAX(normal_min_width, computed_nonpercent_min_width);
			}
	}
	else
		/* Fixed table layout. If width is fixed and this is not a flex item,
		   use that one in width propagation. Fixed widths on flex items may be
		   disregarded by the flex algorithm, which is why we cannot use it in
		   such cases. */

		if (content_width > 0 && !cascade->flexbox)
		{
			min_width = content_width;
			normal_min_width = content_width;
			maximum_width = content_width;
		}
		else
		{
			min_width = props.CheckWidthBounds(min_width, TRUE);
			normal_min_width = props.CheckWidthBounds(normal_min_width, TRUE);
			maximum_width = props.CheckWidthBounds(maximum_width, TRUE);
		}

	if (nonpercent_border_padding && props.box_sizing == CSS_VALUE_content_box)
	{
		// Convert to border-box.

		min_width += nonpercent_border_padding;
		normal_min_width += nonpercent_border_padding;
		maximum_width += nonpercent_border_padding;
	}

	BOOL changed = maximum_width != old_maximum_width ||
		constrained_maximum_width != old_constrained_maximum_width ||
		minimum_width != min_width ||
		normal_minimum_width != normal_min_width;

	minimum_width = min_width;
	normal_minimum_width = normal_min_width;

	return changed;
}

/** Find the largest minimum and maximum width among all captions. */

void
TableContent::CalculateCaptionMinMaxWidth(LayoutCoord& min_width, LayoutCoord& max_width) const
{
	min_width = LayoutCoord(0);
	max_width = LayoutCoord(0);

	for (TableListElement* element = (TableListElement*) layout_stack.First(); element; element = element->Suc())
		if (element->IsCaption())
		{
			LayoutCoord caption_min_width;
			LayoutCoord dummy;
			LayoutCoord caption_max_width;

			if (((TableCaptionBox*) element)->GetMinMaxWidth(caption_min_width, dummy, caption_max_width))
			{
				if (min_width < caption_min_width)
					min_width = caption_min_width;

				if (max_width < caption_max_width)
					max_width = caption_max_width;
			}
		}
}

/** Calculate row width and the distance from table content left edge to a row's left edge. */

/* virtual */ void
TableContent::CalculateRowDimensions(const HTMLayoutProperties& props, LayoutCoord& row_left_offset, LayoutCoord& row_width) const
{
	row_left_offset = LayoutCoord(props.border.left.width + props.padding_left + reflow_state->border_spacing_horizontal);
	row_width = width - LayoutCoord(row_left_offset + props.border.right.width + props.padding_right + reflow_state->border_spacing_horizontal);

	if (row_width < 0)
		/* A column removed with visibility:collapse will also remove one horizontal
		   border spacing, which means that the result may be negative if all columns in
		   the table have been collapsed. */

		row_width = LayoutCoord(0);
}

/** Check and update column min/max widths based on widths of one cell or column.

	Called when a new cell or column has been added, or when we need to
	recalculate the widths due to colspan. */

BOOL
TableContent::UpdateColumnWidths(const LayoutInfo& info, int column, int colspan, LayoutCoord cell_desired_width, LayoutCoord cell_min_width, LayoutCoord cell_normal_min_width, LayoutCoord cell_max_width, BOOL is_cell)
{
	unsigned int i;
	BOOL column_width_changed = FALSE;
	short non_relative_count = 0;
	short auto_count = 0;
	LayoutFixed column_percent = LayoutFixed(0);
	long column_desired_width = 0;
	LayoutCoord column_min_width(0);
	LayoutCoord column_normal_min_width(0);
	LayoutCoord column_max_width(0);
	LayoutCoord non_relative_max(0);
	unsigned int last_referenced_column = column + colspan;
	unsigned int unreferenced_columns;
	int referenced_colspan = colspan;
	LayoutMode layout_mode = info.doc->GetLayoutMode();

	OP_ASSERT(colspan == 1 || is_cell);

	if (last_referenced_column > column_count)
		last_referenced_column = column_count;

	if (last_referenced_column > last_used_column)
	{
		unreferenced_columns = last_referenced_column - last_used_column;
		last_referenced_column = last_used_column;
		referenced_colspan -= unreferenced_columns;
	}
	else
		unreferenced_columns = 0;

	if (!is_cell)
		cell_desired_width = CONTENT_WIDTH_AUTO;

	if (referenced_colspan > 1)
	{
		/* Find out how much space will be used by border-spacing between the
		   columns occupied by this cell, and subtract it from the widths, so
		   that we don't expand columns unnecessarily.

		   IE doesn't do this for maximum width, so they end up with larger
		   column maximum widths than necessary. Seems that we have never
		   attempted to copy this behavior, and I don't see why we should,
		   either. */

		LayoutCoord border_spacing = LayoutCoord((referenced_colspan - 1) * reflow_state->border_spacing_horizontal);

		cell_min_width = MAX(cell_min_width - border_spacing, LayoutCoord(0));
		cell_normal_min_width = MAX(cell_normal_min_width - border_spacing, LayoutCoord(0));
		cell_max_width = MAX(cell_max_width - border_spacing, LayoutCoord(0));

		if (cell_desired_width > 0)
			cell_desired_width = MAX(cell_desired_width - border_spacing, LayoutCoord(0));
	}

	if (cell_desired_width > 0 && cell_max_width > cell_desired_width)
		/* Ignore content maximum width if the cell has desired width set. Note
		   that this can cause minimum width to exceed maximum width. */

		cell_max_width = cell_desired_width;

	if (colspan > 1)
	{
		/* Min/max width of a colspanned cell may never be less than 1px more
		   than border-spacing multiplied with one less than the columns that
		   it spans. Detailed documentation at ../documentation/tables.html

		   Border-spacing between referenced columns should not be part of the
		   columns' minimum width - so don't add it here. */

		LayoutCoord min_width = LayoutCoord(colspan - 1 + unreferenced_columns * reflow_state->border_spacing_horizontal);

		if (cell_normal_min_width < min_width)
			cell_normal_min_width = min_width;

		if (cell_min_width < min_width)
			cell_min_width = min_width;

		if (cell_max_width < min_width)
			cell_max_width = min_width;

		if (reflow_state->colspanned_min_width < cell_min_width)
			reflow_state->colspanned_min_width = cell_min_width;
	}

	for (i = column; i < last_referenced_column; i++)
	{
		if (column_widths[i].packed.has_percent)
			column_percent += LayoutFixed(column_widths[i].packed.percent);
		else
		{
			non_relative_max += column_widths[i].max_width;
			non_relative_count++;

			if (column_widths[i].packed.is_auto)
				auto_count++;
		}

		if (referenced_colspan > 1 && !column_widths[i].desired_width)
			column_desired_width += column_widths[i].max_width;
		else
			column_desired_width += column_widths[i].desired_width;

		column_min_width += column_widths[i].min_width;
		column_normal_min_width += column_widths[i].normal_min_width;
		column_max_width += column_widths[i].max_width;
	}

	if (cell_desired_width != CONTENT_WIDTH_AUTO && (auto_count || !is_cell || GetFixedLayout() == FIXED_LAYOUT_OFF))
		if (cell_desired_width < 0)
		{
			// percent

			LayoutFixed cell_percent = LayoutFixed(-cell_desired_width);
			LayoutFixed extra_percent = cell_percent - column_percent;

			if (extra_percent > LayoutFixed(0))
			{
				if (referenced_colspan == 1)
				{
					column_widths[column].packed.percent = LayoutFixedAsInt(cell_percent);
					column_widths[column].packed.has_percent = 1;
				}
				else
					/* Distribute extra percentage across columns that don't yet have any
					   percentage. If all columns already have a percentage width, do nothing, just
					   like IE, Firefox and Konqueror. */

					if (non_relative_count)
					{
						/* FIXME: Should we cope with rounding errors here? We could at least make
						   sure that the total percentage added to the columns here equal
						   extra_percent. */

						for (i = column; i < last_referenced_column; i++)
							if (!column_widths[i].packed.has_percent)
							{
								column_widths[i].packed.has_percent = 1;

								if (non_relative_max)
									/* Distribute spanned percent between spanned columns without
									   percentage, by giving most to columns with largest maximum
									   width. */

									column_widths[i].packed.percent += LayoutFixedAsInt(MultLayoutFixedByQuotient(extra_percent, column_widths[i].max_width, non_relative_max));
								else
									/* No maximum width on the spanned columns without
									   percentage. Distribute spanned percent value evenly. */
									   column_widths[i].packed.percent += LayoutFixedAsInt(extra_percent / int(non_relative_count));
							}
					}

				column_width_changed = TRUE;
			}
		}
		else
			if (column_desired_width < cell_desired_width)
			{
				if (referenced_colspan == 1)
					column_widths[column].desired_width = cell_desired_width;
				else
					/* Adjust maximum width of the columns that don't have a desired width
					   (using extra space).

					   This approach aligns us with all major browsers. There is an open
					   question still - what should we do if:
					   1) All the spanned columns have desired width.
					   2) The sum of desired widths of all the spanned columns is different
					   from the cell that spans multiple columns. Webkit seems to honor the
					   existing columns' widths (unless the table width forces those columns
					   to expand), while Gecko expands the columns to match the width of
					   the cell width colspan > 1.

					   Our current behavior seems to match Webkit, however with certain
					   tables our later width distribution code might cause the final width
					   of the spanned columns to match neither the existing columns widths
					   nor the colspanned cell width. E.g.

					   <table style="width:300px;">
						<tr>
						 <td style="width:50px;">&nbsp;</td>
						 <td style="width:50px;">&nbsp;</td>
						 <td>&nbsp;</td>
						</tr>
						<tr>
						 <td colspan="2" style="width:200px;">&nbsp;</td>
						 <td>&nbsp;</td>
						</tr>
					   </table>

					   In the above case the width of the colspan cell will be unjustly
					   expanded to almost 300px. */

					for (i = column; i < last_referenced_column; i++)
						if (!column_desired_width)
						{
							column_widths[i].max_width += cell_desired_width / LayoutCoord(referenced_colspan);
							column_max_width += cell_desired_width / LayoutCoord(referenced_colspan);
						}
						else
							if (!column_widths[i].desired_width)
							{
								LayoutCoord new_col_width = (column_widths[i].max_width * cell_desired_width) / LayoutCoord(column_desired_width);
								column_max_width += new_col_width - column_widths[i].max_width;
								column_widths[i].max_width = new_col_width;
							}

				column_width_changed = TRUE;
			}

	/* Check if we need to increase column's normal minimum, minimum and
	   maximum widths.

	   Note for colspan: IE doesn't attempt to honor the columns' desired width
	   here (only maximum and percentage widths). That seems unreasonable, so
	   we honor them (just like Firefox and Konqueror). IE just expands columns
	   with desired width based on their maximum width (which incidentally is
	   the same as desired width if the maximum width of the content is
	   less). */

	// Expand columns' normal minimum width if necessary

	if (column_normal_min_width < cell_normal_min_width)
	{
		if (referenced_colspan == 1)
			column_widths[column].normal_min_width = cell_normal_min_width;
		else
			if (auto_count || GetFixedLayout() != FIXED_LAYOUT_COMPLETED)
			{
				LayoutCoord total_width(0);

				DistributeSpace(info, cell_normal_min_width, column, column + referenced_colspan - 1, FROM_NORMAL_MIN_WIDTH, TRUE);

				column_max_width = LayoutCoord(0);

				for (unsigned i = column; i < last_referenced_column; i++)
				{
					LayoutCoord new_width = reflow_state->column_calculation[i].width;

					OP_ASSERT(column_widths[i].normal_min_width <= new_width); // expand, not shrink!
					column_widths[i].normal_min_width = new_width;

					if (layout_mode == LAYOUT_NORMAL)
						// minimum width follows normal minimum width on normal layout mode

						column_widths[i].min_width = column_widths[i].normal_min_width;

					if (column_widths[i].max_width < new_width)
						column_widths[i].max_width = new_width;

					if (reflow_state->column_calculation[i].expanded)
						column_widths[i].packed.preferred_expandee = 1;

					column_max_width += column_widths[i].max_width;

					total_width += new_width;
				}

				OP_ASSERT(total_width == cell_normal_min_width);
			}

		column_width_changed = TRUE;
	}

	// Expand columns' minimum width if necessary

	if (layout_mode == LAYOUT_NORMAL)
		column_widths[column].min_width = column_widths[column].normal_min_width;
	else
		if (column_min_width < cell_min_width)
		{
			if (referenced_colspan == 1)
				column_widths[column].min_width = cell_min_width;
			else
			{
				LayoutCoord total_width(0);

				DistributeSpace(info, cell_min_width, column, column + referenced_colspan - 1, FROM_MIN_WIDTH, TRUE);

				column_max_width = LayoutCoord(0);

				for (unsigned i = column; i < last_referenced_column; i++)
				{
					LayoutCoord new_width = reflow_state->column_calculation[i].width;

					OP_ASSERT(column_widths[i].min_width <= new_width); // expand, not shrink!
					column_widths[i].min_width = new_width;

					if (column_widths[i].normal_min_width < new_width)
						column_widths[i].normal_min_width = new_width;

					if (column_widths[i].max_width < new_width)
						column_widths[i].max_width = new_width;

					if (reflow_state->column_calculation[i].expanded)
						column_widths[i].packed.preferred_expandee = 1;

					column_max_width += column_widths[i].max_width;

					total_width += new_width;
				}

				OP_ASSERT(total_width == cell_min_width);
			}

			column_width_changed = TRUE;
		}

	// Expand columns' maximum width if necessary

	if (column_max_width < cell_max_width)
	{
		if (referenced_colspan == 1)
			column_widths[column].max_width = cell_max_width;
		else
			if (auto_count || GetFixedLayout() != FIXED_LAYOUT_COMPLETED)
			{
				/* FIXME: Consider improving behavior when spanning percent-based
				   columns here. We make the table maximum width larger than
				   necessary. */

				LayoutCoord total_width(0);

				DistributeSpace(info, cell_max_width, column, column + referenced_colspan - 1, FROM_MAX_WIDTH, TRUE);

				for (unsigned int i = column; i < last_referenced_column; i++)
				{
					OP_ASSERT(column_widths[i].max_width <= reflow_state->column_calculation[i].width); // expand, not shrink!
					column_widths[i].max_width = reflow_state->column_calculation[i].width;

					if (reflow_state->column_calculation[i].expanded)
						column_widths[i].packed.preferred_expandee = 1;

					total_width += reflow_state->column_calculation[i].width;
				}

				OP_ASSERT(total_width == cell_max_width);
			}

		column_width_changed = TRUE;
	}

	return column_width_changed;
}

/** Calculate all column min/max/percentage widths.

	Do the cells with lowest colspan first. */

void
TableContent::CalculateColumnMinMaxWidths(const LayoutInfo& info)
{
	/* In order to calculate column min/max/percentage widths, we need to feed each table cell to
	   the layout engine. We feed them sorted on the their colspan, ascendingly. This way the total
	   min/max widths become as narrow as possible. If there are two or more cells with the same
	   colspan in the table, feed them in layout stack order - i.e. first row first. This makes us
	   compatible with IE. In Firefox and Konqueror, on the other hand, row order seems to make no
	   difference. */

	int colspan = 1;
	int next_colspan;

	do
	{
		next_colspan = INT_MAX;

		for (TableListElement* element = (TableListElement*) layout_stack.First(); element; element = element->Suc())
			if (element->IsRowGroup())
				for (TableRowBox* row = ((TableRowGroupBox*) element)->GetFirstRow(); row; row = (TableRowBox*) row->Suc())
					for (TableCellBox* cell = row->GetFirstCell(); cell; cell = (TableCellBox*) cell->Suc())
					{
						int cell_colspan = cell->GetCellColSpan();

						if (cell_colspan == colspan)
						{
							LayoutCoord min_width;
							LayoutCoord normal_min_width;
							LayoutCoord max_width;

							if (cell->GetMinMaxWidth(min_width, normal_min_width, max_width))
								UpdateColumnWidths(info, cell->GetColumn(), cell_colspan, cell->GetDesiredWidth(), min_width, normal_min_width, max_width, TRUE);
						}
						else
							if (cell_colspan > colspan && cell_colspan < next_colspan)
								next_colspan = cell_colspan;
					}

		colspan = next_colspan;
	}
	while (colspan != INT_MAX);
}

/** Recalculate top margins after new block box has been added. See declaration in header file for more information. */

BOOL
TableContent::RecalculateTopMargins(LayoutInfo& info, const VerticalMargin* top_margin)
{
	/* Only captions can propagate margins, so there is no need to
	   check whether the current element is a row group or row. */

	TableListElement* element = reflow_state->reflow_element;

	OP_ASSERT(element);

	LayoutCoord old_y = element->GetStaticPositionedY();
	LayoutCoord new_y = old_y;
	BOOL pos_changed = FALSE;

	if (layout_stack.HasLink(element))
	{
		TableListElement* pred = element->Pred();

		if (element->IsCaption())
			if (element->IsTopCaption())
			{
				if (!pred)
				{
					// Caption is on top and may propagate margins

					SpaceManager* local_space_manager = placeholder->GetLocalSpaceManager();

					if (!local_space_manager)
					{
						// propagate margins to container

						new_y = LayoutCoord(0);

						pos_changed = placeholder->RecalculateTopMargins(info, top_margin);
					}
					else
					{
						const HTMLayoutProperties* props = placeholder->GetHTMLayoutProperties();

						if (props)
						{
							// The table establishes a new block formatting context.

							new_y = top_margin ? top_margin->GetHeight() : LayoutCoord(0);

							// FIXME: is this correct and really necessary?

							if (props->margin_top)
							{
								VerticalMargin new_top_margin;

								new_top_margin.CollapseWithTopMargin(*props, TRUE);
								placeholder->RecalculateTopMargins(info, &new_top_margin);
							}
						}
					}
				}
				else
				{
					VerticalMargin new_top_margin;

					if (top_margin)
						new_top_margin = *top_margin;

					// Collapse with last child

					new_top_margin.Collapse(reflow_state->top_captions_margin_state);

					new_y = pred->GetStaticPositionedY() + pred->GetHeight() + new_top_margin.GetHeight();
					reflow_state->captions_minimum_height += new_top_margin.GetHeightNonPercent();
				}
			}
			else
			{
				// Collapse with last margin

				if (pred)
					new_y = pred->GetStaticPositionedY() + pred->GetHeight();

				if (!pred || pred->IsTopCaption() || pred->IsRowGroup())
				{
					const HTMLayoutProperties* props = placeholder->GetHTMLayoutProperties();

					// No preceding bottom captions; continue below rows

					OP_ASSERT(props);

					if (props)
					{
						new_y += LayoutCoord(props->border.bottom.width + reflow_state->border_spacing_vertical);
						if (!GetCollapseBorders())
							new_y += props->padding_bottom;
					}
				}

				// Set new vertical position of caption; collapse with bottom margin of last element

				if (top_margin)
				{
					new_y += top_margin->GetHeight();
					reflow_state->captions_minimum_height += top_margin->GetHeightNonPercent();
				}
			}
	}

	if (new_y != old_y)
	{
		element->IncreaseY(info, this, new_y - old_y);

		return TRUE;
	}

	return pos_changed;
}

/** Helper function for moving rows. */

void
TableContent::MoveElements(LayoutInfo& info, TableListElement* element, LayoutCoord increment)
{
	// move elements

	for (; element; element = element->Suc())
	{
		info.visual_device->UpdateRelative(element->GetPositionedX(),
										   element->GetPositionedY(),
										   width, // FIXME: overflow not handled
										   element->GetHeight());

		element->IncreaseY(info, this, increment);
	}

	if (!UseOldRowHeights())
		UpdateHeight(info);
}

/** Row completed, decrement rowspans. */

void
TableContent::RowCompleted()
{
	if (reflow_state->fixed_layout == FIXED_LAYOUT_ON)
		reflow_state->fixed_layout = FIXED_LAYOUT_COMPLETED;

	for (int column = GetColumnCount(); --column >= 0;)
		if (column_widths[column].packed2.rowspans > 0)
			--column_widths[column].packed2.rowspans;
}

/** Finish all row spans. */

void
TableContent::TerminateRowSpan(LayoutInfo& info, TableRowBox* last_row)
{
	if (!IsWrapped())
		for (unsigned short column = 0; column < GetColumnCount(); column++)
			if (column_widths[column].packed2.rowspans > 0)
			{
				last_row->CloseRowspannedCell(info, column, this);
				column_widths[column].packed2.rowspans = 0;
			}
}

/** Skip rowspanned columns.

    Returns TRUE if any skipped column continues into next row.
	If 'next_column_position' is given it is updated and more
	columns are checked, if not then only first column is
    checked. */

BOOL
TableContent::SkipRowSpannedColumns(LayoutInfo& info, unsigned short& column, TableRowBox* row, LayoutCoord* next_column_position, BOOL force_end, unsigned int min_rowspan)
{
	BOOL rowspan_continues = FALSE;

 	while (column < column_count && column_widths[column].packed2.rowspans > min_rowspan)
	{
		if (next_column_position && !column_widths[column].IsVisibilityCollapsed())
			*next_column_position += column_widths[column].width + LayoutCoord(reflow_state->border_spacing_horizontal);

		if (force_end || column_widths[column].packed2.rowspans == 1)
		{
			if (!packed.use_old_row_heights)
			{
				TableCellBox* cell = NULL;
				TableRowBox* scan = row;
				LayoutCoord spanned_rows_height(0);

				for (;;)
				{
					// Look for cell in column

					for (cell = scan->GetFirstCell(); cell; cell = (TableCellBox*) cell->Suc())
						if (cell->GetColumn() == column)
							// This is the cell we are looking for

							break;
						else
							if (cell->GetColumn() > column)
							{
								// Passed column, no need to look further in this row

								cell = NULL;
								break;
							}

					if (cell)
						break;

					scan = (TableRowBox*) scan->Pred();

					if (!scan)
						break;

					spanned_rows_height += scan->GetHeight() + LayoutCoord(reflow_state->border_spacing_vertical);
				}

				if (cell)
					// Now, make sure row is high enough

					row->GrowRow(info, cell->GetCellHeight() - spanned_rows_height, LayoutCoord(0));
			}
		}
		else
			// Rowspan continues onto next row

			rowspan_continues = TRUE;

		if (!next_column_position)
			break;

		column++;
	}

	return rowspan_continues;
}

/** Set child's bottom margins in this block and propagate changes to parents. See declaration in header file for more information. */

void
TableContent::PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* bottom_margin, BOOL has_inflow_content)
{
	TableListElement* element = reflow_state->reflow_element;
	const HTMLayoutProperties* props = placeholder->GetHTMLayoutProperties();

	OP_ASSERT(element);
	OP_ASSERT(element->IsCaption());

	if (layout_stack.HasLink(element))
		if (element->IsTopCaption())
		{
			// We're working on a top caption

			LayoutCoord new_y = element->GetStaticPositionedY() + element->GetHeight();
			short dummy;
			short top_border_width;
			GetBorderWidths(*props, top_border_width, dummy, dummy, dummy, FALSE);

			// Margins are stored for collapsing with later captions

			if (bottom_margin)
			{
				reflow_state->top_captions_margin_state = *bottom_margin;
				new_y += bottom_margin->GetHeight();
			}
			else
				reflow_state->top_captions_margin_state.Reset();

			new_y += LayoutCoord(top_border_width);
			if (!GetCollapseBorders())
				new_y += props->padding_top;

			if (reflow_state->row_position != new_y)
			{
				// Need to move rows and bottom captions

				LayoutCoord difference = new_y - reflow_state->row_position;

				reflow_state->row_position = new_y;

				MoveElements(info, element->Suc(), difference);
			}
		}
		else
		{
			// Margins are stored for collapsing with later captions

			if (bottom_margin)
				reflow_state->margin_state = *bottom_margin;
			else
				reflow_state->margin_state.Reset();

			if (!UseOldRowHeights())
				UpdateHeight(info);
		}

	{
		TableCaptionBox* caption = (TableCaptionBox*) element;

		// Caption can be wider than table

		AbsoluteBoundingBox caption_bounding_box;

		caption->GetBoundingBox(caption_bounding_box);

		caption_bounding_box.Translate(caption->GetPositionedX(), caption->GetPositionedY());

		if (props->overflow_x == CSS_VALUE_visible)
			placeholder->UpdateBoundingBox(caption_bounding_box, FALSE);
	}
}

/** Expand container to make space for absolute positioned boxes. See declaration in header file for more information. */

void
TableContent::PropagateBottom(const LayoutInfo& info, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts)
{
	/* Absolute positioned boxes only modify boxes with their own
	   Z list. */

	if (!placeholder->GetLocalStackingContext() || !placeholder->IsPositionedBox())
		// Propagate past this container

		placeholder->PropagateBottom(info, LayoutCoord(0), LayoutCoord(0), child_bounding_box, opts);
	else
		placeholder->UpdateBoundingBox(child_bounding_box, FALSE);
}

/** Update height. */

void
TableContent::UpdateHeight(const LayoutInfo& info)
{
	TableListElement* element = (TableListElement*) layout_stack.Last();

	if (element)
	{
		const HTMLayoutProperties* props = placeholder->GetCascade()->GetProps();
		short dummy;
		short top_border_width;
		short bottom_border_width;
		LayoutCoord old_height = height;

		GetBorderWidths(*props, top_border_width, dummy, dummy, bottom_border_width, FALSE);

		height = element->GetStaticPositionedY() + element->GetHeight();

		if (element->IsRowGroup() || element->IsTopCaption())
		{
			height += LayoutCoord(bottom_border_width);
			if (!GetCollapseBorders())
				height += props->padding_bottom;

			if (!element->IsRowGroup())
			{
				height += LayoutCoord(top_border_width);
				if (!GetCollapseBorders())
					height += props->padding_top;
			}
			else
				height += LayoutCoord(reflow_state->border_spacing_vertical);
		}

		placeholder->ReduceBoundingBox(height - old_height, LayoutCoord(0), props->DecorationExtent(info.doc));
	}
}

/** Based on column widths, get table cell width, either calculated width or desired width. */

LayoutCoord
TableContent::GetCellWidth(int column, int colspan, BOOL desired, LayoutCoord* collapsed_width) const
{
	OP_ASSERT(column_widths && colspan > 0);

	unsigned short last_column = column + colspan;
	LayoutCoord cell_width(0);

	if (last_column > column_count)
		last_column = column_count;

	if (collapsed_width)
		*collapsed_width = LayoutCoord(0);

	for (int i = column; i < last_column; i++)
	{
		LayoutCoord column_width = desired ? column_widths[i].desired_width : column_widths[i].width;

		cell_width += column_width;

		if (collapsed_width && IsColumnVisibilityCollapsed(i))
			*collapsed_width += column_width + LayoutCoord(i == column ? 0 : reflow_state->border_spacing_horizontal);
	}

	if (colspan > 1)
	{
		/* Add horizontal border-spacing between referenced columns to cell
		   width, since a colspanned cell will occupy that space too. */

		LayoutCoord spacing = LayoutCoord(reflow_state->border_spacing_horizontal * (colspan - 1));
		int last_referenced_column = GetFixedLayout() == FIXED_LAYOUT_OFF ? MIN(last_used_column, last_column) : last_column;

		cell_width += LayoutCoord(reflow_state->border_spacing_horizontal * (last_referenced_column - column - 1));

		if (spacing > cell_width)
			cell_width = spacing;
	}

	return cell_width;
}

/** Get top of first row. */

long
TableContent::GetRowsTop() const
{
	TableListElement* first_element = (TableListElement*) layout_stack.First();

	while(first_element && !first_element->IsRowGroup())
		first_element = first_element->Suc();

	if (first_element)
		return first_element->GetPositionedY();
	return 0;
}

/** Get height between top of first row and bottom of last row. */

long
TableContent::GetRowsHeight() const
{
	TableListElement* first_element = (TableListElement*) layout_stack.First();
	TableListElement* last_element = (TableListElement*) layout_stack.Last();

	while(first_element && !first_element->IsRowGroup())
		first_element = first_element->Suc();
	while(last_element && !last_element->IsRowGroup())
		last_element = last_element->Pred();

	return first_element && last_element ? last_element->GetStaticPositionedY() + last_element->GetHeight() - first_element->GetStaticPositionedY() : 0;
}

/** Get the visible rectangle of a partially visibility:collapsed table cell. */

BOOL
TableContent::GetCollapsedCellRect(OpRect& clip_rect, LayoutCoord& offset_x, LayoutCoord& offset_y, LayoutProperties* table_lprops, const TableCellBox* cell, const TableRowBox* first_row) const
{
	/* Check if and how visibility:collapse affects traversal of this cell. If the cell spans
	   multiple columns or rows, some parts need to be clipped. Implementation limitation: If the
	   visibility:visible rows or columns are disjoint, we'll only traverse the first continuous
	   set. To improve this, we'd need to traverse the same cell several times or something. */

	unsigned short colspan = cell->GetCellColSpan();
	unsigned short rowspan = cell->GetCellRowSpan();
	short start_column = SHRT_MIN;
	short end_column = SHRT_MIN;
	const TableRowBox* start_row = NULL;
	const TableRowBox* end_row = NULL;
	BOOL clip_after_last_column = FALSE;
	BOOL clip_after_last_row = FALSE;
	unsigned short i;

	clip_rect.Empty();
	offset_x = LayoutCoord(0);
	offset_y = LayoutCoord(0);

	if (cell->GetColumn() + colspan >= GetLastColumn())
		colspan = GetLastColumn() - cell->GetColumn();

	// Find start and end visible column

	for (i = 0; i < colspan; i++)
		if (IsColumnVisibilityCollapsed(cell->GetColumn() + i))
		{
			if (start_column != SHRT_MIN)
			{
				end_column = cell->GetColumn() + i - 1;
				clip_after_last_column = TRUE;
				break;
			}
		}
		else
			if (start_column == SHRT_MIN)
				start_column = cell->GetColumn() + i;

	if (start_column == SHRT_MIN)
		return TRUE; // everything is collapsed

	if (end_column == SHRT_MIN)
		end_column = cell->GetColumn() + colspan - 1;

	OP_ASSERT(end_column >= cell->GetColumn());
	OP_ASSERT(start_column <= end_column);
	OP_ASSERT(start_column < GetLastColumn());
	OP_ASSERT(end_column < GetLastColumn());

	// Find start and end visible row

	const TableRowBox* sibling_row = first_row;
	const TableRowBox* prev_row = NULL;

	for (i = 0; i < rowspan && sibling_row; i++, sibling_row = (TableRowBox*) sibling_row->Suc())
	{
		if (sibling_row->IsVisibilityCollapsed())
		{
			if (start_row)
			{
				end_row = prev_row;
				clip_after_last_row = TRUE;
				break;
			}
		}
		else
			if (!start_row)
				start_row = sibling_row;

		prev_row = sibling_row;
	}

	if (!start_row)
		return TRUE; // everything is collapsed

	if (!end_row)
		end_row = prev_row;

	OP_ASSERT(end_row);

	if (clip_after_last_column || clip_after_last_row || start_row != first_row || start_column != cell->GetColumn())
	{
		OP_ASSERT(column_widths);

		const HTMLayoutProperties& props = *table_lprops->GetProps();
		int border_spacing_horizontal;
		int border_spacing_vertical;

		if (GetCollapseBorders())
		{
			border_spacing_horizontal = 0;
			border_spacing_vertical = 0;
		}
		else
		{
			border_spacing_horizontal = props.border_spacing_horizontal;
			border_spacing_vertical = props.border_spacing_vertical;
		}

		LayoutCoord start_x(0);
		int i;

		for (i = cell->GetColumn(); i < start_column; i++)
		{
			OP_ASSERT(i < last_used_column);
			start_x += column_widths[i].width + LayoutCoord(border_spacing_horizontal);
		}

		LayoutCoord width = column_widths[i].width;

		for (i = start_column + 1; i <= end_column; i++)
		{
			OP_ASSERT(i < last_used_column);
			width += LayoutCoord(border_spacing_horizontal) + column_widths[i].width;
		}

		if (clip_after_last_column)
			clip_rect.width = width;
		else
			clip_rect.width = LAYOUT_COORD_HALF_MAX;

		if (start_column != cell->GetColumn())
		{
			offset_x = start_x;
			clip_rect.x = offset_x;
		}
		else
		{
			clip_rect.x = LAYOUT_COORD_HALF_MIN;
			clip_rect.width += -clip_rect.x;
		}

		if (clip_after_last_row)
			clip_rect.height = end_row->GetStaticPositionedY() + end_row->GetHeight() - start_row->GetStaticPositionedY();
		else
			clip_rect.height = LAYOUT_COORD_HALF_MAX;

		if (start_row != first_row)
		{
			offset_y = LayoutCoord(0);

			for (const TableRowBox* r = first_row; r != start_row; r = (TableRowBox*) r->Suc())
				offset_y += r->GetHeight() + LayoutCoord(border_spacing_vertical);

			clip_rect.y = offset_y;
		}
		else
		{
			clip_rect.y = LAYOUT_COORD_MIN / 2;
			clip_rect.height += -clip_rect.y;
		}

		return TRUE;
	}

	return FALSE;
}

/** A cell/column/column-group has been added. Adjust number of
	columns in table and update rowspan state for the columns spanned
	by the cell/column/column-group added.

	Return FALSE if OOM. */

BOOL
TableContent::AdjustNumberOfColumns(unsigned short column, unsigned short colspan, unsigned short rowspan, BOOL is_cell, LayoutWorkplace* workplace)
{
	OP_ASSERT(column < MAX_COLUMNS);

	if (column + colspan > MAX_COLUMNS)
		colspan = MAX_COLUMNS - column;

	int min_column_count = column + colspan;

	if (column_count < min_column_count)
	{
		// Ran out of space in the column_width array

		unsigned int i = 0;
		TableColumnInfo* new_column_widths = OP_NEWA(TableColumnInfo, min_column_count);
		if (!new_column_widths)
			return FALSE;

		TableColumnCalculation* new_column_calculation = OP_NEWA(TableColumnCalculation, min_column_count);
		if (!new_column_calculation)
		{
			OP_DELETEA(new_column_widths);
			return FALSE;
		}

		for (; i < column_count; ++i)
		{
			new_column_widths[i] = column_widths[i];
			new_column_calculation[i] = reflow_state->column_calculation[i];
		}

		OP_DELETEA(column_widths);
		OP_DELETEA(reflow_state->column_calculation);

		column_widths = new_column_widths;
		reflow_state->column_calculation = new_column_calculation;

		column_count = min_column_count;
	}

	if (reflow_state->new_column_count < min_column_count)
		reflow_state->new_column_count = min_column_count;

	if (is_cell && last_used_column < column + 1)
		last_used_column = column + 1;

	if (column_widths)
		// Update rowspan state for column

		for (int i = column; i < min_column_count; i++)
			if (column_widths[i].packed2.rowspans < rowspan)
				// This will overwrite previous rowspans

				column_widths[i].packed2.rowspans = rowspan;

	if (is_cell && reflow_state->max_colspan < colspan)
		reflow_state->max_colspan = colspan;

	return TRUE;
}

/** Add True Table condition for column */

void
TableContent::AddTrueTableCondition(int column, TRUE_TABLE_CANDIDATE candidate, const LayoutInfo& info)
{
	if (reflow_state->true_table == UNDETERMINED)
		switch (candidate)
		{
		case NEUTRAL:
			break;

		case POSITIVE:
			if (column_widths[column].packed2.true_table_cells < 0x03ff)
				column_widths[column].packed2.true_table_cells++;

			break;

		case NEGATIVE:
			if (column_widths[column].packed2.non_true_table_cells < 0x003f)
				column_widths[column].packed2.non_true_table_cells++;

			break;

		case DISABLE:
			reflow_state->true_table = NOT_TRUE_TABLE;
			break;
		}
}

/** Get the percent of normal width when page is shrunk in ERA. */

/* virtual */ int
TableContent::GetPercentOfNormalWidth() const
{
	if (width < normal_minimum_width)
		return ((int) width * 100) / normal_minimum_width;
	else
		return 100;
}

/** Get normal x position of column when page is shrunk in ERA. */

LayoutCoord
TableContent::GetNormalColumnX(int column, short horizontal_cell_spacing) const
{
	if (GetCollapseBorders())
		horizontal_cell_spacing = 0;

	LayoutCoord column_x = LayoutCoord(horizontal_cell_spacing);

	for(int i = 0; i < column; i++)
	{
		if (column_widths[i].desired_width > column_widths[i].normal_min_width)
			column_x += column_widths[i].desired_width;
		else
			column_x += column_widths[i].normal_min_width;

		column_x += LayoutCoord(horizontal_cell_spacing);
	}

	return column_x;
}

/** Calculate column widths for the given table width. */

BOOL
TableContent::CalculateColumnWidths(LayoutInfo& info)
{
	if (!column_count)
		return TRUE;

	/* Minimum and maximum widths of each column have been calculated at this
	   point. So has the total available table width. Based on these values we
	   calculate the final column widths to use. The final sum of the column
	   widths will be the greater of available table width and the sum of
	   column minimum widths. */

	BOOL column_width_has_changed = FALSE;
	BOOL auto_layout = GetFixedLayout() == FIXED_LAYOUT_OFF;
	unsigned int i;
	int border_spacing = reflow_state->border_spacing_horizontal * ((auto_layout ? last_used_column : column_count) + 1);
	LayoutCoord available_width = width;
	BOOL single_column = info.table_strategy == TABLE_STRATEGY_SINGLE_COLUMN || info.table_strategy == TABLE_STRATEGY_TRUE_TABLE && !IsTrueTable();

	if (available_width != LAYOUT_COORD_MAX)
	{
		const HTMLayoutProperties &props = *placeholder->GetCascade()->GetProps();
		short dummy;
		short left_border_width;
		short right_border_width;

		GetBorderWidths(props, dummy, left_border_width, right_border_width, dummy, FALSE);

		available_width -= LayoutCoord(left_border_width + right_border_width);

		if (!GetCollapseBorders())
			available_width -= props.padding_left + props.padding_right;

		if (available_width < 0)
			available_width = LayoutCoord(0);
	}

	if (single_column)
	{
		packed.is_wrapped = 1;

		for (i = 0; i < column_count; ++i)
			column_widths[i].width = available_width - LayoutCoord(reflow_state->border_spacing_horizontal * 2);

		return TRUE;
	}

	if (info.flexible_columns && (info.table_strategy != TABLE_STRATEGY_TRUE_TABLE || packed.true_table != SMALL_TABLE))
	{
		int min_width = 0;

		OP_ASSERT(info.doc->GetLayoutMode() != LAYOUT_NORMAL);

		for (i = 0; i < column_count; ++i)
		{
			min_width += column_widths[i].min_width;

			if (min_width > available_width - border_spacing)
			{
				// Exceeded available width. Wrap table.

				unsigned int start_i = 0; // Start column of current wrapped "row"
				LayoutCoord awidth(0); // Width used on current wrapped "row"
				LayoutCoord normal_extra(0);
				LayoutCoord new_available_width = available_width - LayoutCoord(reflow_state->border_spacing_horizontal);

				packed.is_wrapped = 1;

				for (i = 0; i < column_count; ++i)
				{
					OP_ASSERT(column_widths[i].normal_min_width >= column_widths[i].min_width);

					column_widths[i].width = column_widths[i].min_width;

					if (awidth + column_widths[i].min_width > new_available_width - reflow_state->border_spacing_horizontal)
					{
						// Ran out of space. Wrap to next "row".

						short columns = i - start_i;

						for (; start_i < i; ++start_i)
						{
							LayoutCoord added_width;

							if (normal_extra > new_available_width - awidth)
								added_width = LayoutCoord((column_widths[start_i].normal_min_width - column_widths[start_i].min_width) * (new_available_width - awidth) / normal_extra);
							else
								added_width = LayoutCoord(column_widths[start_i].normal_min_width - column_widths[start_i].min_width + (new_available_width - awidth - normal_extra) / columns);

							column_widths[start_i].width += added_width;
							awidth += added_width;
						}

						OP_ASSERT(new_available_width >= awidth);
						OP_ASSERT(start_i >= 1); // The first column obviously needs to fit on the first "row"

						if (start_i >= 1)
							column_widths[start_i - 1].width += new_available_width - awidth;

						awidth = LayoutCoord(0);
						normal_extra = LayoutCoord(0);
						new_available_width = available_width - LayoutCoord(reflow_state->border_spacing_horizontal);
					}

					normal_extra += column_widths[i].normal_min_width - column_widths[i].width;
					awidth += column_widths[i].min_width;
					new_available_width -= LayoutCoord(reflow_state->border_spacing_horizontal);
				}

				if (normal_extra)
					for (; start_i < column_count; ++start_i)
					{
						if (normal_extra > new_available_width - awidth)
							column_widths[start_i].width += (column_widths[start_i].normal_min_width - column_widths[start_i].min_width) * (new_available_width - awidth) / normal_extra;
						else
							column_widths[start_i].width = column_widths[start_i].normal_min_width;
					}

				return TRUE;
			}
		}
	}

	if (available_width != LAYOUT_COORD_MAX)
	{
		available_width -= LayoutCoord(border_spacing);

		if (available_width < 0)
			available_width = LayoutCoord(0);
	}

	packed.is_wrapped = 0;

	/* Scaling percentages down when they exceed 100% in case of fixed table layout
	   aligns us with Gecko and Webkit. */

	DistributeSpace(info, available_width, 0, column_count - 1, FROM_MIN_WIDTH, !auto_layout);

#ifdef DEBUG
	// All available space should be distributed across the columns. Let's verify that.

	LayoutCoord new_width(0);

	for (i = 0; i < column_count; ++i)
		new_width += reflow_state->column_calculation[i].width;

	OP_ASSERT(available_width <= new_width);
#endif // DEBUG

	for (i = 0; i < column_count; ++i)
		// Check if any columns are changed and store new value

		if (reflow_state->column_calculation[i].width != column_widths[i].width)
		{
			column_width_has_changed = TRUE;
			column_widths[i].width = reflow_state->column_calculation[i].width;
		}

	return column_width_has_changed;
}

/** Distribute the specified space across the specified columns. */

void
TableContent::DistributeSpace(const LayoutInfo& info, LayoutCoord available_width, unsigned int first_column, unsigned int last_column, SpaceDistributionMode mode, BOOL scale_down_percentage)
{
	/* This method will set the 'width' member of the 'column_calculation' array of reflow_state,
	   and the caller will know what to do with that information. Initial value of the 'width'
	   members is ignored.

	   This is the central table column width distribution algorithm. It is described in detail in
	   a dedicated chapter in the documentation (../documentation/tables.html). */

	unsigned int i;
	LayoutCoord min_width(0);
	LayoutCoord normal_min_width(0);
	LayoutCoord max_width(0);

	LayoutCoord desired_width(0);
	LayoutCoord desired_exceeded(0);
	LayoutCoord desired_extra(0);

	int relative_column_count = 0;
	int expandee_count = 0;
	int zero_width_column_count = 0; // number of columns that we probably want to give no width
	LayoutCoord non_relative_min_width(0);
	LayoutCoord expandees_min_width(0);
	LayoutCoord expandee_max_width(0);
	LayoutFixed percent_used = LayoutFixed(0);
	const LayoutFixed max_percentage = LAYOUT_FIXED_HUNDRED_PERCENT;
	LayoutCoord new_width(0);
	int columns = last_column - first_column + 1;
	LayoutCoord total_current_acc; // prevents rounding errors

	BOOL auto_layout = GetFixedLayout() == FIXED_LAYOUT_OFF;
	BOOL sloppy_desired_width = !info.hld_profile->IsInStrictMode();

	TableColumnCalculation* new_widths = reflow_state->column_calculation;

	for (i = first_column; i <= last_column; ++i)
	{
		// Start with the width according to mode

		LayoutCoord start_width;

		if (auto_layout)
		{
			switch (mode)
			{
			case FROM_MIN_WIDTH:
				start_width = column_widths[i].min_width;
				break;

			case FROM_NORMAL_MIN_WIDTH:
				start_width = column_widths[i].normal_min_width;
				break;

			case FROM_MAX_WIDTH:
				start_width = column_widths[i].max_width;
				break;

			default:
				OP_ASSERT(!"Shouldn't happen");
				start_width = LayoutCoord(0);
				break;
			};
		}
		else
			start_width = column_widths[i].desired_width;

		new_widths[i].width = start_width;
		new_widths[i].min_width = start_width;

		new_widths[i].ignore_percent = 0;
		new_widths[i].expanded = 0;

		// Set percentage on this column and update total percentage count.

		if ((scale_down_percentage || percent_used < max_percentage) && column_widths[i].packed.has_percent)
		{
			percent_used += LayoutFixed(column_widths[i].packed.percent);
			new_widths[i].percent = LayoutFixed(column_widths[i].packed.percent);
			new_widths[i].has_percent = 1;

			if (!scale_down_percentage && percent_used > max_percentage)
			{
				new_widths[i].percent -= percent_used - max_percentage;
				percent_used = max_percentage;
			}
		}
		else
			new_widths[i].has_percent = 0;

		if (auto_layout)
		{
			// Calculate minimum and maximum widths

			OP_ASSERT(column_widths[i].min_width <= column_widths[i].normal_min_width);
			min_width += column_widths[i].min_width;
			normal_min_width += column_widths[i].normal_min_width;
			max_width += column_widths[i].max_width;
		}

		if (new_widths[i].has_percent)
			// Count as relative column

			relative_column_count++;
		else
		{
			if (column_widths[i].IsPreferredExpandee(auto_layout, sloppy_desired_width))
			{
				expandee_count++;

				if (auto_layout)
				{
					expandees_min_width += start_width;
					expandee_max_width += column_widths[i].max_width;
				}
			}

			// Accumulate minimum width of non-relatively sized columns

			non_relative_min_width += start_width;

			if (auto_layout && !column_widths[i].min_width)
				zero_width_column_count ++;

			if (column_widths[i].desired_width)
			{
				// Accumulate absolute widths for later use

				desired_width += column_widths[i].desired_width;

				LayoutCoord column_desired_extra = column_widths[i].desired_width - start_width;

				if (column_desired_extra > 0)
					desired_extra += column_desired_extra;
				else
					desired_exceeded += -column_desired_extra;
			}
		}

		// Calculate used width so far

		new_width += new_widths[i].width;
	}

	LayoutCoord extra_space = available_width - new_width;

	if (auto_layout && extra_space > 0 && mode == FROM_MIN_WIDTH)
	{
		LayoutCoord normal_min_diff = normal_min_width - min_width;

		if (normal_min_diff > 0)
		{
			/* Attempt to regain some of the space that is lost because
			   min_width is smaller than normal_min_width. */

			LayoutCoord extra_acc(0); // prevents rounding errors
			LayoutCoord total_prev_acc(0); // prevents rounding errors

			new_width = LayoutCoord(0);

			for (i = first_column; i <= last_column; ++i)
			{
				LayoutCoord extra;

				if (extra_space >= normal_min_diff)
					// Just satisfy normal minimum widths of every column.

					extra = column_widths[i].normal_min_width - new_widths[i].width;
				else
				{
					/* Not enough extra space to satsify normal minimum widths
					   completely. Distribute extra space by giving more to columns with larger
					   difference between minimum width and normal minimum width. */

					extra_acc += column_widths[i].normal_min_width - column_widths[i].min_width;
					total_current_acc = extra_space * extra_acc / normal_min_diff;
					extra = total_current_acc - total_prev_acc;
					total_prev_acc = total_current_acc;

					OP_ASSERT(new_widths[i].width + extra <= column_widths[i].normal_min_width);
				}

				if (extra > 0)
				{
					// minimum width increased

					if (!new_widths[i].has_percent)
					{
						// Update statistics for non-percentage columns

						non_relative_min_width += extra;

						if (column_widths[i].desired_width)
						{
							// Update statistics for columns with desired width

							LayoutCoord column_desired_extra = column_widths[i].desired_width - column_widths[i].min_width;

							if (column_desired_extra > 0)
								desired_extra -= column_desired_extra;
							else
								desired_exceeded -= -column_desired_extra;

							column_desired_extra -= extra;

							if (column_desired_extra > 0)
								desired_extra += column_desired_extra;
							else
								desired_exceeded += -column_desired_extra;
						}

						if (column_widths[i].IsPreferredExpandee(auto_layout, sloppy_desired_width))
							expandees_min_width += extra;
					}

					new_widths[i].min_width += extra;
					new_widths[i].width += extra;
				}

				new_width += new_widths[i].width;
			}

			extra_space = available_width - new_width;
		}
	}

	if (extra_space > 0 && relative_column_count && mode != FROM_MAX_WIDTH)
	{
		// More space left; attempt to satisfy percentage widths

		do
		{
			LayoutCoord percent_width_available = available_width - non_relative_min_width;
			LayoutFixed percent_acc = LayoutFixed(0); // prevents rounding errors
			LayoutCoord total_prev_acc(0); // prevents rounding errors
			BOOL scale_percent = relative_column_count == columns - zero_width_column_count ||
				percent_used * double(available_width) > max_percentage * double(percent_width_available) ||
				percent_used > max_percentage;

			new_width = LayoutCoord(0);

			for (i = first_column; i <= last_column; ++i)
			{
				if (new_widths[i].has_percent && !new_widths[i].ignore_percent)
				{
					percent_acc += new_widths[i].percent;

					if (scale_percent)
					{
						if (percent_used > LayoutFixed(0))
							total_current_acc = PercentageOfPercentageToLayoutCoord(percent_acc, percent_width_available, percent_used);
						else
							/* In this case percent_acc var stops being percentage in layout fixed
							   point format and becomes just a counter. */
							total_current_acc = LayoutCoord(percent_width_available * LayoutFixedAsInt(++percent_acc) / relative_column_count);
					}
					else
						total_current_acc = PercentageToLayoutCoord(percent_acc, available_width);

					LayoutCoord col_width = total_current_acc - total_prev_acc;
					total_prev_acc = total_current_acc;

					if (col_width < new_widths[i].min_width)
					{
						/* This column got a width smaller than its minimum
						   width. Restart resolving percentage widths in this
						   table, this time without this column. */

						percent_used -= new_widths[i].percent;
						non_relative_min_width += new_widths[i].min_width;
						new_widths[i].ignore_percent = 1;

						/* Must reset column width here; it may have been
						   increased in a previous pass which had more space
						   for percentage widths available. */

						new_widths[i].width = new_widths[i].min_width;

						break; // restart
					}

					new_widths[i].width = col_width;
				}

				new_width += new_widths[i].width;
			}
		}
		while (i <= last_column);

		extra_space = available_width - new_width;
	}

	if (extra_space > 0 && desired_width && mode != FROM_MAX_WIDTH)
	{
		// More space left; attempt to satisfy desired widths

		LayoutCoord desired_acc(0); // prevents rounding errors
		LayoutCoord total_prev_acc(0); // prevents rounding errors

		new_width = LayoutCoord(0);

		for (i = first_column; i <= last_column; ++i)
		{
			if (column_widths[i].desired_width && !column_widths[i].packed.has_percent &&
				column_widths[i].desired_width > new_widths[i].width)
			{
				if (desired_extra <= extra_space)
					new_widths[i].width = column_widths[i].desired_width;
				else
				{
					desired_acc += column_widths[i].desired_width - new_widths[i].width;
					total_current_acc = desired_acc * extra_space / desired_extra;
					new_widths[i].width += total_current_acc - total_prev_acc;
					total_prev_acc = total_current_acc;
				}
			}

			new_width += new_widths[i].width;
		}

		extra_space = available_width - new_width;
	}

	if (auto_layout && extra_space > 0 && expandee_max_width > expandees_min_width &&
		expandee_max_width - expandees_min_width <= extra_space && mode != FROM_MAX_WIDTH)
	{
		/* More space left; in fact there's enough to satisfy the maximum width
		   of each expandable column. */

		new_width = LayoutCoord(0);

		for (i = first_column; i <= last_column; ++i)
		{
			if (column_widths[i].IsPreferredExpandee(auto_layout, sloppy_desired_width))
				new_widths[i].width = column_widths[i].max_width;

			new_width += new_widths[i].width;
		}

		extra_space = available_width - new_width;
	}

	if (extra_space > 0)
	{
		/* Still more space left; distribute all remaining space. That is,
		   afterwards there should be no extra space left. */

		LayoutCoord total_prev_acc(0); // prevents rounding errors
		total_current_acc = LayoutCoord(0); // prevents rounding errors

		if (expandee_count)
		{
			/* Column set type 1: columns with no percentage AND no desired
			   width (or desired width exceeded by maximum width) AND non-zero
			   minimum width: widen each column relative to their maximum
			   width */

			LayoutCoord expandee_width(0);

			if (auto_layout) // fixed layout doesn't care about min/max widths, so let's not waste time on this
				for (i = first_column; i <= last_column; ++i)
					if (column_widths[i].IsPreferredExpandee(TRUE, sloppy_desired_width))
						expandee_width += new_widths[i].width;

			if (expandee_width < expandee_max_width)
			{
				LayoutCoord total_diff = expandee_max_width - expandee_width;
				LayoutCoord diff_acc(0); // prevents rounding errors

				for (i = first_column; i <= last_column; ++i)
					if (column_widths[i].IsPreferredExpandee(auto_layout, sloppy_desired_width))
					{
						diff_acc += column_widths[i].max_width - new_widths[i].width;
						total_current_acc = extra_space * diff_acc / total_diff;
						new_widths[i].width += total_current_acc - total_prev_acc;
						new_widths[i].expanded = 1;
						total_prev_acc = total_current_acc;
					}
			}
			else
				if (expandee_max_width)
				{
					LayoutCoord width_acc(0); // prevents rounding errors

					for (i = first_column; i <= last_column; ++i)
						if (column_widths[i].IsPreferredExpandee(auto_layout, sloppy_desired_width))
						{
							width_acc += column_widths[i].max_width;
							total_current_acc = extra_space * width_acc / expandee_max_width;
							new_widths[i].width += total_current_acc - total_prev_acc;
							new_widths[i].expanded = 1;
							total_prev_acc = total_current_acc;
						}
				}
				else
				{
					LayoutCoord column_acc(0); // prevents rounding errors

					for (i = first_column; i <= last_column; ++i)
						if (column_widths[i].IsPreferredExpandee(auto_layout, sloppy_desired_width))
						{
							total_current_acc = LayoutCoord(++column_acc * extra_space / expandee_count);
							new_widths[i].width += total_current_acc - total_prev_acc;
							new_widths[i].expanded = 1;
							total_prev_acc = total_current_acc;
						}
				}
		}
		else
			if (columns > zero_width_column_count)
			{
				if (desired_width)
				{
					/* Column set type 2: columns with desired width: widen
					   each column by the larger of each column's minimum width
					   and desired width relative to the sum of total desired
					   width and exceeded desired width */

					int width_acc = 0; // prevents rounding errors

					for (i = first_column; i <= last_column; ++i)
						if (!new_widths[i].has_percent)
						{
							width_acc += MAX(new_widths[i].min_width, column_widths[i].desired_width);
							total_current_acc = LayoutCoord(extra_space * width_acc / (desired_width + desired_exceeded));
							new_widths[i].width += total_current_acc - total_prev_acc;
							new_widths[i].expanded = 1;
							total_prev_acc = total_current_acc;
						}
				}
				else
				{
					/* Column set type 3: columns with percentage: widen each
					   column relative to their percentage */

					OP_ASSERT(relative_column_count == columns - zero_width_column_count);

					if (percent_used > LayoutFixed(0))
					{
						LayoutFixed percent_acc = LayoutFixed(0); // prevents rounding errors

						for (i = first_column; i <= last_column; ++i)
							if (new_widths[i].has_percent)
							{
								percent_acc += new_widths[i].percent;
								total_current_acc = PercentageOfPercentageToLayoutCoord(percent_acc, extra_space, percent_used);
								new_widths[i].width += total_current_acc - total_prev_acc;
								new_widths[i].expanded = 1;
								total_prev_acc = total_current_acc;
							}
					}
					else
					{
						int column_acc = 0; // prevents rounding errors

						for (i = first_column; i <= last_column; ++i)
							if (new_widths[i].has_percent)
							{
								total_current_acc = LayoutCoord(++column_acc * extra_space / relative_column_count);
								new_widths[i].width += total_current_acc - total_prev_acc;
								new_widths[i].expanded = 1;
								total_prev_acc = total_current_acc;
							}
					}
				}
			}
			else
			{
				/* Column set type 4: all columns: widen each column relative
				   to their maximum width, or, if none has maximum width,
				   distribute remaining space evenly */

				if (max_width)
				{
					int max_acc = 0; // prevents rounding errors

					for (i = first_column; i <= last_column; ++i)
					{
						max_acc += column_widths[i].max_width;
						total_current_acc = LayoutCoord(extra_space * max_acc / max_width);
						new_widths[i].width += total_current_acc - total_prev_acc;
						new_widths[i].expanded = 1;
						total_prev_acc = total_current_acc;
					}
				}
				else
				{
					int cols = relative_column_count ? relative_column_count : columns;
					int column_acc = 0; // prevents rounding errors

					for (i = first_column; i <= last_column; ++i)
					{
						total_current_acc = LayoutCoord(++column_acc * extra_space / cols);
						new_widths[i].width += total_current_acc - total_prev_acc;
						new_widths[i].expanded = 1;
						total_prev_acc = total_current_acc;
					}
				}
			}
	}
}

/** Add caption to table. */

void
TableContent::GetNewCaption(LayoutInfo& info, TableCaptionBox* caption, int caption_side)
{
	reflow_state->reflow_element = caption;

	switch (caption_side)
	{
	case CSS_VALUE_top:
		if (reflow_state->last_top_caption)
			caption->Follow(reflow_state->last_top_caption);
		else
			caption->IntoStart(&layout_stack);

		reflow_state->last_top_caption = caption;
		break;

	case CSS_VALUE_bottom:
		caption->Into(&layout_stack);
		break;
	}
}

/** Add row group to table. */

void
TableContent::GetNewRowGroup(LayoutInfo& info, TableRowGroupBox* row_group)
{
	TableListElement* prev = NULL;
	LayoutCoord new_y;

	if (reflow_state->last_row_group)
	{
		TableRowGroupType type = row_group->GetType();

		switch (type)
		{
		case TABLE_HEADER_GROUP:
			if (((TableRowGroupBox*) (reflow_state->last_top_caption ? reflow_state->last_top_caption->Suc() : layout_stack.First()))->GetType() == TABLE_HEADER_GROUP)
				// Max one table-header-group box per table

				type = TABLE_ROW_GROUP;
			else
				prev = reflow_state->last_top_caption;

			break;
		case TABLE_FOOTER_GROUP:
			if (reflow_state->last_row_group->GetType() == TABLE_FOOTER_GROUP)
				// Max one table-footer-group box per table

				type = TABLE_ROW_GROUP;
			else
				prev = reflow_state->last_row_group;

			break;
		}

		if (type == TABLE_ROW_GROUP)
			if (reflow_state->last_row_group->GetType() == TABLE_FOOTER_GROUP)
				prev = reflow_state->last_row_group->Pred();
			else
				prev = reflow_state->last_row_group;
	}
	else
	{
		reflow_state->table_box_minimum_height += LayoutCoord(reflow_state->border_spacing_vertical);
		prev = reflow_state->last_top_caption;
	}

	if (prev)
	{
		row_group->Follow(prev);
		new_y = prev->GetStaticPositionedY() + prev->GetHeight();

		if (prev == reflow_state->last_top_caption)
		{
			short dummy;
			short top_border_width;
			const HTMLayoutProperties* props = placeholder->GetCascade()->GetProps();

			GetBorderWidths(*props, top_border_width, dummy, dummy, dummy, FALSE);

			new_y += LayoutCoord(top_border_width + reflow_state->top_captions_margin_state.GetHeight() + reflow_state->border_spacing_vertical);
			if (!GetCollapseBorders())
				new_y += props->padding_top;
		}
		else
			if (prev->IsRowGroup() && ((TableRowGroupBox*) prev)->GetLastRow())
				new_y += LayoutCoord(reflow_state->border_spacing_vertical);
	}
	else
	{
		row_group->IntoStart(&layout_stack);
		new_y = reflow_state->row_position + LayoutCoord(reflow_state->border_spacing_vertical);
	}

	reflow_state->reflow_element = row_group;

	if (!row_group->Suc() || ((TableListElement*) row_group->Suc())->IsCaption())
		reflow_state->last_row_group = row_group;

	row_group->SetStaticPositionedY(new_y);
}

/** Add columns to table. */

BOOL
TableContent::AddColumns(const LayoutInfo& info, short span, LayoutCoord width, TableColGroupBox* column_box)
{
	if (reflow_state->next_col_number < MAX_COLUMNS)
	{
		if (reflow_state->next_col_number + span > MAX_COLUMNS)
			span = MAX_COLUMNS - reflow_state->next_col_number;

		if (!AdjustNumberOfColumns(reflow_state->next_col_number, span, 0, FALSE, info.workplace))
			return FALSE;

		for (; span; --span)
		{
			column_widths[reflow_state->next_col_number].column_box = column_box;
			SetDesiredColumnWidth(info, reflow_state->next_col_number++, 1, width, FALSE);
		}
	}

	return TRUE;
}

/** Finish reflowing box. */

/* virtual */ LAYST
TableContent::FinishLayout(LayoutInfo& info)
{
	LayoutProperties* cascade = placeholder->GetCascade();
	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord used_height = reflow_state->css_height;

	if (cascade->flexbox)
	{
		FlexItemBox* item_box = (FlexItemBox*) placeholder;
		LayoutCoord stretch_height = item_box->GetFlexHeight(cascade);

		if (reflow_state->reflow)
		{
			// Calculate hypothetical height.

			LayoutCoord hypothetical_height(0);
			LayoutCoord vertical_border_padding =
				LayoutCoord(props.border.top.width) + LayoutCoord(props.border.bottom.width) +
				props.padding_top + props.padding_bottom;

			if (used_height > 0)
			{
				hypothetical_height = used_height;

				if (props.box_sizing == CSS_VALUE_content_box)
					hypothetical_height += vertical_border_padding;
			}

			LayoutCoord auto_height;

			if (TableListElement* last = (TableListElement*) layout_stack.Last())
			{
				auto_height = last->GetStaticPositionedY() + last->GetHeight();

				// Add border-spacing following the last row.

				auto_height += LayoutCoord(reflow_state->border_spacing_vertical);

				// Add bottom padding and border.

				short border_bottom, dummy;

				GetBorderWidths(props, dummy, dummy, dummy, border_bottom, FALSE);
				auto_height += LayoutCoord(border_bottom) + (GetCollapseBorders() ? LayoutCoord(0) : props.padding_bottom);
			}
			else
				auto_height = vertical_border_padding;

			if (hypothetical_height < auto_height)
				hypothetical_height = auto_height;

			// Propagate minimum and hypothetical height.

			item_box->PropagateHeights(auto_height, hypothetical_height);
		}

		if (stretch_height != CONTENT_HEIGHT_AUTO)
			// The parent flex content has a new height for us to use.

			used_height = stretch_height;
	}

	if (reflow_state->reflow_element)
		if (!packed.use_old_row_heights)
		{
			TableListElement* element;

			for (element = (TableListElement*) layout_stack.First(); element; element = element->Suc())
				if (element->IsRowGroup())
					for (TableRowBox* row = ((TableRowGroupBox*) element)->GetFirstRow(); row; row = (TableRowBox*) row->Suc())
						if (packed.has_content_with_percentage_height)
							for (TableCellBox* cell = row->GetFirstCell(); cell; cell = (TableCellBox*) cell->Suc())
								if (cell->HasContentWithPercentageHeight())
									/* The table cell has content with percentage based height, and
									   therefore it will need to be reflowed once more. */
									info.workplace->SetReflowElement(cell->GetHtmlElement(), TRUE);

			if (used_height > 0)
			{
				// The distance between two rows
				LayoutCoord row_spacing = LayoutCoord(reflow_state->border_spacing_vertical);

				// Total height of the table minus one row spacing
				LayoutCoord table_content_height = used_height - LayoutCoord(props.border.top.width + props.border.bottom.width + row_spacing);
				if (!GetCollapseBorders())
					table_content_height -= props.padding_top + props.padding_bottom;

				// Table content height minus the sum of the row heights.
				LayoutCoord extra_height = table_content_height;

				// Sum of the minimum heights of all the rows
				LayoutCoord table_row_height(0);

				// Number of rows without specified height
				LayoutCoord rows_wo_spec_height(0);

				// Total height of the rows without specified height
				LayoutCoord total_without_height(0);

				// Sum of the percentage values for all rows
				LayoutFixed total_percentage = LayoutFixed(0);

				// Used to adjust the percentage rows to fit into the table. Usually 100%
				LayoutFixed total_percentage_norm = LayoutFixed(0);

				// Height available to share between rows with percentage-height
				LayoutCoord available_height = table_content_height;

				// Count number of rows, unused space and used space. This
				// is an initialization.

				for (element = (TableListElement*) layout_stack.First(); element; element = element->Suc())
					if (element->IsRowGroup())
						for (TableRowBox* row = ((TableRowGroupBox*) element)->GetFirstRow(); row; row = (TableRowBox*) row->Suc())
						{
							extra_height -= row->GetHeight() + row_spacing;
							table_row_height += row->GetHeight();

							if (row->GetSpecifiedHeightPercentage() != LayoutFixed(0))
								total_percentage += row->GetSpecifiedHeightPercentage();
							else
								if (!row->GetSpecifiedHeightPixels())
								{
									// The row has no specified height

									total_without_height += row->GetHeight();
									available_height -= row->GetHeight() + row_spacing;
									rows_wo_spec_height++;
								}
								else
									// The row has only specified pixel-height

									available_height -= row->GetHeight() + row_spacing;
						}

				if (extra_height < 0)
				{
					/* The table content is larger than the specified table
					   height and must therefore be adjusted. */

					table_content_height += extra_height;
					available_height += extra_height;
					extra_height = LayoutCoord(0);
				}
				else
					if (extra_height > 0)
					{
						/* There exist unused height that has to be
						   shared between the rows.

						   The rows are checked to see if they should use the minimum pixel-height,
						   which is either the height of the content or the specified height.
						   The other alternative is to use the percentage height. The maximum
						   of the alternatives is used.

						   When a row has too large minimum height, the percentage can't be used
						   and then the extra height available for the other rows shrinks. Therefore
						   we have to iterate until all rows are satisfied. */

						// Only iterate if precentage-rows exist.

						LayoutFixed iterate = total_percentage;
						const LayoutFixed max_percentage = LAYOUT_FIXED_HUNDRED_PERCENT;

						while (LayoutFixedAsInt(iterate))
						{
							/* If the total percentage is less than 100%, and there are rows without
							   specified height, the norm should be 100%.  If the total percentage is more
							   than 100%, or there are no rows with unspecified height, the total
							   percentage should be the norm. */

							if (rows_wo_spec_height && total_percentage < max_percentage)
								total_percentage_norm = max_percentage;
							else
								total_percentage_norm = total_percentage;

							/* If the total height claimed by the percentage-rows is larger
							   than the available height, the norm has to be adjusted so that
							   the total percentage-height uses exactly the available height. */

							if (total_percentage * double(table_content_height)> total_percentage_norm * double(available_height))
								total_percentage_norm = MultLayoutFixedByQuotient(total_percentage, table_content_height, available_height);

							iterate = LayoutFixed(0);

							for (TableListElement* element = (TableListElement*) layout_stack.First(); element; element = element->Suc())
								if (element->IsRowGroup())
									for (TableRowBox* row = ((TableRowGroupBox*) element)->GetFirstRow(); row; row = (TableRowBox*) row->Suc())
										if (LayoutFixedAsInt(row->GetSpecifiedHeightPercentage()))
											// Percentage height is specified

											if (total_percentage_norm * double(row->GetHeight()) >= row->GetSpecifiedHeightPercentage() * double(table_content_height))
											{
												// Use pixel height

												total_percentage -= row->GetSpecifiedHeightPercentage();
												available_height -= row->GetHeight() + row_spacing;

												row->ClearSpecifiedHeightPercentage();
												row->SetSpecifiedHeightPixels(TRUE);

												/* If there are more rows with percentage height
												   more iterations might be needed. */

												iterate = available_height ? total_percentage : LayoutFixed(0);
											}
											else
												// Use percentage height

												row->SetSpecifiedHeightPixels(FALSE);
						}

						// Take care of rows with percentage height

						if (LayoutFixedAsInt(total_percentage))
							for (TableListElement* element = (TableListElement*) layout_stack.First(); element; element = element->Suc())
								if (element->IsRowGroup())
								{
									TableRowGroupBox* row_group = (TableRowGroupBox*) element;

									for (TableRowBox* row = row_group->GetFirstRow(); row; row = (TableRowBox*) row->Suc())
										if (!row->GetSpecifiedHeightPixels() &&
											LayoutFixedAsInt(row->GetSpecifiedHeightPercentage()))
										{
											LayoutCoord new_height = PercentageOfPercentageToLayoutCoord(row->GetSpecifiedHeightPercentage(), table_content_height, total_percentage_norm)
												- row_spacing;

											// Compensate for rounding-errors

											if (new_height > available_height - row_spacing)
												new_height = available_height - row_spacing;

											available_height -= new_height + row_spacing;

											row_group->ForceRowHeightIncrease(info, row, new_height - row->GetHeight(), packed.has_content_with_percentage_height);
										}
								}

						// Take care of rows with no specified height

						if (available_height)
						{
							short num_left = rows_wo_spec_height;

							extra_height = available_height;

							for (TableListElement* element = (TableListElement*) layout_stack.First(); element; element = element->Suc())
								if (element->IsRowGroup())
								{
									TableRowGroupBox* row_group = (TableRowGroupBox*) element;

									for (TableRowBox* row = row_group->GetFirstRow(); row; row = (TableRowBox*) row->Suc())
										if (!row->GetSpecifiedHeightPercentage() &&
											!row->GetSpecifiedHeightPixels())
										{
											if (num_left <= 1)
											{
												row_group->ForceRowHeightIncrease(info, row, extra_height, packed.has_content_with_percentage_height);
												extra_height = LayoutCoord(0);
												break;
											}

											LayoutCoord increment;

											if (total_without_height)
												increment = available_height * row->GetHeight() / total_without_height;
											else
												increment = available_height / rows_wo_spec_height;

											if (increment)
											{
												// Set the new height

												row_group->ForceRowHeightIncrease(info, row, increment, packed.has_content_with_percentage_height);
												extra_height -= increment;
											}

											num_left--;
										}
								}

							available_height = extra_height;
						}

						/* If all rows have specified height and the sum of
						   the rows' heights is less than the table height
						   these rows has to be filled with space. */

						if (available_height)
						{
							extra_height = available_height;

							for (TableListElement* element = (TableListElement*) layout_stack.First(); element && extra_height; element = element->Suc())
								if (element->IsRowGroup())
								{
									TableRowGroupBox* row_group = (TableRowGroupBox*) element;

									for (TableRowBox* row = row_group->GetFirstRow(); row && extra_height; row = (TableRowBox*) row->Suc())
									{
										if (!row->Suc())
										{
											row_group->ForceRowHeightIncrease(info, row, extra_height, packed.has_content_with_percentage_height);
											extra_height = LayoutCoord(0);
											break;
										}

										LayoutCoord increment;

										if (table_row_height)
											increment = available_height * row->GetHeight() / table_row_height;
										else
											if (rows_wo_spec_height)
												increment = available_height / rows_wo_spec_height;
											else
												increment = LayoutCoord(0);

										if (increment)
										{
											// Set the new height

											row_group->ForceRowHeightIncrease(info, row, increment, packed.has_content_with_percentage_height);
											extra_height -= increment;
										}
									}
								}
						}
					}
			}

			// Compensate for visibility:collapse on rows and row-groups.

			TableListElement* first_element = (TableListElement*) layout_stack.First();
			LayoutCoord collapsed_height(0);

			for (element = first_element; element; element = element->Suc())
			{
				element->IncreaseY(info, this, -collapsed_height);

				if (element->IsRowGroup())
				{
					TableRowGroupBox* row_group_box = (TableRowGroupBox*) element;

					if (row_group_box->IsVisibilityCollapsed())
						collapsed_height += row_group_box->GetHeight();
					else
					{
						TableRowBox* first_row = row_group_box->GetFirstRow();

						if (first_row)
						{
							LayoutCoord collapsed_row_height(0);

							for (TableRowBox* row = first_row; row; row = (TableRowBox*) row->Suc())
							{
								row->IncreaseY(info, -collapsed_row_height);

								if (row->IsVisibilityCollapsed())
									collapsed_row_height += row->GetHeight() + LayoutCoord(reflow_state->border_spacing_vertical);
							}

							collapsed_height += collapsed_row_height;
						}
					}
				}
			}

			height -= collapsed_height;
			OP_ASSERT(height >= 0);

			if (packed.has_content_with_percentage_height && !reflow_state->calculate_min_max_widths)
			{
				/* This table has percentual height. It also has one or more elements with
				   percentual height. Min/max widths were calculated in a previous reflow pass, and
				   we have now found some good final heights for each row. This means that it's
				   time to enable "use old row heights" mode for future reflows. */

				packed.use_old_row_heights = 1;

				/* Scrollables in this table may now freely remove their scrollbars in
				   the next reflow pass, should they so wish. */

				HTML_Element* desc = cascade->html_element->Next();
				HTML_Element* stop = cascade->html_element->NextSibling();

				while (desc != stop)
				{
					Box* box = desc->GetLayoutBox();

					if (box)
						if (ScrollableArea* sc = box->GetScrollable())
							sc->UnlockAutoScrollbars();

					if (!box ||
						box->GetTableContent() ||
						box->GetContainer() && !box->GetContainer()->HeightIsRelative() && !box->IsTableBox())
						desc = desc->NextSibling();
					else
						desc = desc->Next();
				}
			}
		}

	if (reflow_state->reflow)
	{
		LayoutCoord nonpercent_min_height = MAX(props.content_height, LayoutCoord(0));

		reflow_state->table_box_minimum_height += GetNonPercentVerticalBorderPadding(props);

		if (!props.GetMinHeightIsPercent() && nonpercent_min_height < props.min_height)
			nonpercent_min_height = props.min_height;

		if (reflow_state->table_box_minimum_height < nonpercent_min_height)
			reflow_state->table_box_minimum_height = nonpercent_min_height;

		minimum_height = reflow_state->captions_minimum_height + reflow_state->table_box_minimum_height;
	}

	if (!info.hld_profile->IsInStrictMode() && layout_stack.Empty() && placeholder->IsBlockBox())
	{
		width = LayoutCoord(0);
		height = LayoutCoord(0);
		minimum_height = LayoutCoord(0);
	}

	UpdateScreen(info);

	if (info.table_strategy == TABLE_STRATEGY_TRUE_TABLE && reflow_state->reflow)
	{
		/* This table was laid out in this pass. Check if it should become a "true table"
		   or a "small table" (ERA magic table types). */

		TRUE_TABLE_MODE new_mode;

		switch (reflow_state->true_table)
		{
		default:
			new_mode = TRUE_TABLE;
			break;

		case UNDETERMINED:
			{
				int true_table_columns = 0;
				int non_true_table_columns = 0;

				for (unsigned int i = 0; i < column_count; i++)
					if (column_widths[i].packed2.true_table_cells > column_widths[i].packed2.non_true_table_cells)
						true_table_columns++;
					else
						if (column_widths[i].packed2.true_table_cells < column_widths[i].packed2.non_true_table_cells)
							non_true_table_columns++;

				if (true_table_columns && (true_table_columns / 2) + 1 >= non_true_table_columns)
				{
					new_mode = TRUE_TABLE;
					break;
				}
			}

		case NOT_TRUE_TABLE:
		case SMALL_TABLE:
			new_mode = NOT_TRUE_TABLE;

			/* Detect "small tables", unless this is a flex item. (It could be possible
			   to do it for flex items too, but then we would need to define what a flex
			   item's available width IS, which isn't completely trivial.) */

			if (!cascade->flexbox)
			{
				int available_width = placeholder->IsFixedPositionedBox() ? info.workplace->GetLayoutViewWidth() : placeholder->GetAvailableWidth(cascade);

				if (maximum_width <= available_width)
					new_mode = SMALL_TABLE;
			}

			break;
		}

		switch (new_mode)
		{
		case SMALL_TABLE:
		case TRUE_TABLE:
			if (!IsTrueTable())
			{
				SetColumnStrategyChanged();
				RequestReflow(info);
			}
			packed.true_table = new_mode;
			break;

		case NOT_TRUE_TABLE:
			if (IsTrueTable())
			{
				SetColumnStrategyChanged();
				RequestReflow(info);
			}

			packed.true_table = NOT_TRUE_TABLE;
			break;
		}
	}

	if (reflow_state->calculate_min_max_widths)
		packed.minmax_calculated = 1;

	if (cascade->multipane_container && cascade->multipane_container->GetMultiColumnContainer())
		/* Table row groups and captions may be pushed down during
		   layout. Finding and propagating explicit page and column breaks now
		   (after table layout) will ensure that their positions are
		   correct. */

		if (OpStatus::IsMemoryError(FindBreaks(info, cascade)))
			return LAYOUT_OUT_OF_MEMORY;

	for (TableListElement* element = (TableListElement*) layout_stack.First(); element; element = element->Suc())
		if (element->IsRowGroup())
			((TableRowGroupBox*) element)->DeleteReflowState();

	DeleteReflowState();

	return LAYOUT_CONTINUE;
}

/** Update content on screen. */

/* virtual */ void
TableContent::UpdateScreen(LayoutInfo& info)
{
	if (!reflow_state)
		/* No reflow state means that layout has already been finished.
		   (placeholder->FinishLayout() first calls FinishLayout() which calls
		   UpdateScreen() (and before returning, FinishLayout() deletes the reflow
		   state). Then, placeholder->FinishLayout() calls placeholder->UpdateScreen() which
		   calls UpdateScreen() again). */

		return;

	BOOL needs_reflow = CheckChange(info);

	if (placeholder->NeedMinMaxWidthCalculation(placeholder->GetCascade()))
	{
		LayoutCoord max_width = constrained_maximum_width != -1 ? constrained_maximum_width : maximum_width;

		placeholder->PropagateWidths(info, MAX(minimum_width, reflow_state->caption_min_width), MAX(normal_minimum_width, reflow_state->caption_min_width), MAX(max_width, reflow_state->caption_min_width));
	}

	UpdateHeight(info);

#ifdef CSS_TRANSFORMS
	if (TransformContext *transform_context = placeholder->GetTransformContext())
		if (!placeholder->IsInlineBox())
			transform_context->Finish(info, placeholder->GetCascade(), GetWidth(), GetHeight());
#endif // CSS_TRANSFORMS

	if (needs_reflow)
		RequestReflow(info);
	else
	{
		SetColGroupBoxDimensions();

		for (TableListElement* element = (TableListElement*) layout_stack.First(); element; element = element->Suc())
			if (element->IsRowGroup())
			{
				TableRowGroupBox* group = (TableRowGroupBox*) element;
				TableRowGroupBoxReflowState* group_state = group->GetReflowState();

				if (group_state)
				{
					LayoutCoord new_x = group->GetPositionedX();
					LayoutCoord new_y = group->GetPositionedY();
					LayoutCoord new_height = group->GetHeight();

					if (group_state->old_height &&
						(group_state->old_x != new_x || group_state->old_y != new_y))
					{
						info.visual_device->UpdateRelative(group_state->old_x,
														   group_state->old_y,
														   reflow_state->row_width,
														   group_state->old_y + group_state->old_height);

						info.visual_device->UpdateRelative(new_x,
														   new_y,
														   reflow_state->row_width,
														   new_y + new_height);

						group_state->old_x = new_x;
						group_state->old_y = new_y;
						group_state->old_height = new_height;
					}

					AbsoluteBoundingBox child_box;

					group->GetBoundingBox(child_box);

#ifdef CSS_TRANSFORMS
					if (TransformContext *transform_context = group->GetTransformContext())
						transform_context->ApplyTransform(child_box);
#endif

					child_box.Translate(group->GetPositionedX(), group->GetPositionedY());

					if (child_box.GetX() < 0 || child_box.GetY() < 0 ||
						child_box.GetX() + child_box.GetWidth() > GetRowWidth() || child_box.GetY() + child_box.GetHeight() > GetHeight())
					{
						/* Keeping other browser engines compatibility (Gecko, WebKit)
						   - in case of tables only overflow_x matters and it controls also
						   the visiblity of y overflow. IE9 is slightly different - they
						   do not ignore overflow-y, but it also affects x overflow. */
						if (placeholder->GetCascade()->GetProps()->overflow_x != CSS_VALUE_hidden)
							placeholder->UpdateBoundingBox(child_box, FALSE);
						group->SetIsOverflowed(TRUE);
					}
					else
						group->SetIsOverflowed(FALSE);
				}
			}
	}

	// propagate margins to container

	placeholder->PropagateBottomMargins(info, &reflow_state->margin_state);
}

/** Does this table need special border handling due
    to html attributes 'frames' and 'rules' ? */

BOOL
TableContent::NeedSpecialBorders()
{
	return (packed.frame != TABLE_FRAME_not_specified && packed.frame != TABLE_FRAME_border) ||
		   (packed.rules != TABLE_RULES_not_specified && packed.rules != TABLE_RULES_all);
}

/** Get table border widths, taking border collapsing into account if applicable. */

/* virtual */ void
TableContent::GetBorderWidths(const HTMLayoutProperties& props, short& top, short& left, short& right, short& bottom, BOOL inner) const
{
	top = props.border.top.width;
	left = props.border.left.width;
	right = props.border.right.width;
	bottom = props.border.bottom.width;
}

/** Modify properties for collapsing border content. */

/* virtual */ void
TableCollapsingBorderContent::ModifyBorderProps(LayoutInfo& info, LayoutProperties* cascade)
{
	if (collapsed_border.top.width == BORDER_WIDTH_NOT_SET)
		collapsed_border = cascade->GetProps()->border;

	SetCornersCalculated(FALSE);
}

/** Calculate row width and the distance from table content left edge to a row's left edge. */

/* virtual */ void
TableCollapsingBorderContent::CalculateRowDimensions(const HTMLayoutProperties& props, LayoutCoord& row_left_offset, LayoutCoord& row_width) const
{
	row_left_offset = LayoutCoord(collapsed_border.left.width / 2);
	row_width = GetWidth() - LayoutCoord(row_left_offset + (collapsed_border.right.width + 1) / 2);
}

/** Collapse top border. */

void
TableCollapsingBorderContent::CollapseTopBorder(LayoutInfo& info, BorderEdge& cell_border_edge)
{
	const HTMLayoutProperties* props = placeholder->GetCascade()->GetProps();
	short old_width = collapsed_border.top.width;

	cell_border_edge.Collapse(props->border.top);
	collapsed_border.top.Collapse(cell_border_edge);

	LayoutCoord increment = LayoutCoord((collapsed_border.top.width - old_width) / 2);

	if (increment > 0)
	{
		for (TableListElement* element = (TableListElement*) layout_stack.First(); element; element = element->Suc())
			if (element->IsRowGroup())
			{
				MoveElements(info, element, increment);
				break;
			}
	}
}

/** Collapse left border. */

void
TableCollapsingBorderContent::CollapseLeftBorder(LayoutInfo& info, BorderEdge& cell_border_edge, unsigned short end_column)
{
	const HTMLayoutProperties* props = placeholder->GetCascade()->GetProps();
	short old_width = collapsed_border.left.width;

#ifdef SUPPORT_TEXT_DIRECTION
	if (props->direction == CSS_VALUE_rtl)
		if (reflow_state->highest_col_number_collapsed < end_column)
		{
			/* Higher column number than before. Values propagated from previous cells
			   will not collapse with the table after all, then. */

			reflow_state->highest_col_number_collapsed = end_column;
			collapsed_border.left = props->border.left;
		}
#endif //  SUPPORT_TEXT_DIRECTION

	cell_border_edge.Collapse(props->border.left);
	collapsed_border.left.Collapse(cell_border_edge);

	int increment = (collapsed_border.left.width - old_width) / 2;

	if (increment > 0)
		RequestReflow(info);
}

/** Collapse right border. */

void
TableCollapsingBorderContent::CollapseRightBorder(LayoutInfo& info, BorderEdge& cell_border_edge, unsigned short end_column)
{
	const HTMLayoutProperties* props = placeholder->GetCascade()->GetProps();
	short old_width = collapsed_border.right.width;

#ifdef SUPPORT_TEXT_DIRECTION
	if (props->direction == CSS_VALUE_ltr)
#endif //  SUPPORT_TEXT_DIRECTION
		if (reflow_state->highest_col_number_collapsed < end_column)
		{
			/* Higher column number than before. Values propagated from previous cells
			   will not collapse with the table after all, then. */

			reflow_state->highest_col_number_collapsed = end_column;
			collapsed_border.right = props->border.right;
		}

	cell_border_edge.Collapse(props->border.right);
	collapsed_border.right.Collapse(cell_border_edge);

	int increment = (collapsed_border.right.width + 1) / 2 - (old_width + 1) / 2;

	if (increment > 0)
		RequestReflow(info);
}

/** Collapse bottom border. */

void
TableCollapsingBorderContent::CollapseBottomBorder(LayoutInfo& info, BorderEdge& cell_border_edge)
{
	const HTMLayoutProperties* props = placeholder->GetCascade()->GetProps();
	short old_width = collapsed_border.bottom.width;

	cell_border_edge.Collapse(props->border.bottom);
	collapsed_border.bottom.Collapse(cell_border_edge);

	if (!UseOldRowHeights())
	{
		int increment = (collapsed_border.bottom.width + 1) / 2 - (old_width + 1) / 2;

		if (increment > 0)
			UpdateHeight(info);
	}
}

/** Reset collapsed bottom border. */

/* virtual */ void
TableCollapsingBorderContent::ResetCollapsedBottomBorder()
{
	collapsed_border.bottom = placeholder->GetCascade()->GetProps()->border.bottom;
}

/** Clear min/max width. */

/* virtual */ void
TableCollapsingBorderContent::ClearMinMaxWidth()
{
	TableContent::ClearMinMaxWidth();
	collapsed_border.Reset();
}

/** Get table border widths, taking border collapsing into account if applicable. */

/* virtual */ void
TableCollapsingBorderContent::GetBorderWidths(const HTMLayoutProperties& props, short& top, short& left, short& right, short& bottom, BOOL inner) const
{
	if (inner)
	{
		top = (collapsed_border.top.width + 1) / 2;
		left = (collapsed_border.left.width + 1) / 2;
		right = collapsed_border.right.width / 2;
		bottom = collapsed_border.bottom.width / 2;
	}
	else
	{
		top = collapsed_border.top.width / 2;
		left = collapsed_border.left.width / 2;
		right =(collapsed_border.right.width + 1) / 2;
		bottom = (collapsed_border.bottom.width + 1) / 2;
	}
}

/** Finish reflowing box. */

/* virtual */ LAYST
TableCollapsingBorderContent::FinishLayout(LayoutInfo& info)
{
	TableRowGroupBox* last_row_group = NULL;

	for (TableListElement* element = (TableListElement*) layout_stack.First(); element; element = element->Suc())
		if (element->IsRowGroup())
		{
			TableRowGroupBox* row_group = (TableRowGroupBox*) element;

			if (row_group->GetLastRow())
			{
				last_row_group = row_group;
				row_group->FinishLastBorder(this, info);
			}
		}

	if (last_row_group)
	{
		// Last row group

		TableCollapsingBorderRowBox* row = (TableCollapsingBorderRowBox*) last_row_group->GetLastRow();
		if (row)
		{
			const BorderEdge& bottom_row_border = row->GetBorder().bottom;
			BorderEdge bottom_border = bottom_row_border;
			bottom_border.Collapse(placeholder->GetCascade()->GetProps()->border.bottom);

			if (bottom_border.width != bottom_row_border.width || bottom_border.style != bottom_row_border.style)
				row->FinishBottomBorder(info, bottom_border, last_row_group, this);
		}
	}

	return TableContent::FinishLayout(info);
}

/** Initialize the list item counter. */

BOOL
ListItemState::Init(LayoutInfo& info, HTML_Element* html_element)
{
	previous_value = 0;
	reversed = FALSE;

	if (html_element->Type() == Markup::HTE_OL)
	{
		reversed = html_element->HasAttr(Markup::HA_REVERSED);

		if (html_element->HasNumAttr(Markup::HA_START))
			if (reversed)
				previous_value = html_element->GetNumAttr(Markup::HA_START, NS_IDX_HTML, 0) + 1;
			else
				previous_value = html_element->GetNumAttr(Markup::HA_START, NS_IDX_HTML, 1) - 1;
		else
			if (reversed)
			{
				unsigned count;

				if (OpStatus::IsMemoryError(info.workplace->CountOrderedListItems(html_element, count)))
					return FALSE;

				previous_value = 1 + count;
			}
	}

	return TRUE;
}

/** Get the numeric value to assign to the specified list item element. */

int
ListItemState::GetNewValue(HTML_Element* list_item_element)
{
	int value;

	if (list_item_element->HasNumAttr(Markup::HA_VALUE))
		value = list_item_element->GetNumAttr(Markup::HA_VALUE, ITEM_TYPE_NUM, -1);
	else
		if (reversed)
			value = previous_value - 1;
		else
			value = previous_value + 1;

	if (value > 0x1fffffff)
		value = 0x1fffffff;
	else
		if (value < -0x20000000)
			value = -0x20000000;

	previous_value = value;

	return value;
}

void
ContainerReflowState::CommitPendingInlineContent(const Line* line)
{
	if (line)
		line_state.CommitPendingBoxes(line->GetX() - line->GetStartPosition());

	line_state.CommitPendingVirtualRange();
	current_line_position += GetPendingContent();

	ResetPendingContent();
}

/** Create an empty line. We need a line to reach positioned block boxes and
	(empty) inline boxes during line traversal. */

LAYST
Container::CreateEmptyLine(LayoutInfo& info)
{
	if (!GetReflowLine() && reflow_state->break_before_content)
	{
		LAYST status = GetNewLine(info, reflow_state->break_before_content, LayoutCoord(0));
		if (status != LAYOUT_CONTINUE)
			return status;
	}

	return LAYOUT_CONTINUE;
}

/** Create empty first line.
	An empty line is needed by traversal of :first-line and by copying text (/n in <pre>) */

LAYST
Container::CreateEmptyFirstLine(LayoutInfo& info, HTML_Element* element)
{
	OP_ASSERT(!GetReflowLine());
	OP_ASSERT(!reflow_state->is_css_first_line || reflow_state->GetLineCurrentPosition() == 0);

	LAYST status = GetNewLine(info, element, LayoutCoord(0));

	if (status != LAYOUT_CONTINUE)
		return status;

	Line* current_line = GetReflowLine();

	OP_ASSERT(current_line);

	if (current_line)
	{
		LayoutCoord line_height = placeholder->GetCascade()->GetCascadingProperties().GetCalculatedLineHeight(info.visual_device);

		current_line->SetHeight(line_height);
		reflow_state->virtual_line_min_height = line_height;
	}

	return LAYOUT_CONTINUE;
}

/** Force end of line. */

LAYST
Container::ForceLineEnd(LayoutInfo& info, HTML_Element* text_element, BOOL create_empty)
{
	reflow_state->line_has_content = TRUE;

	LAYST status = CommitLineContent(info, TRUE);

	Line* current_line = GetReflowLine();

	if (status == LAYOUT_CONTINUE)
	{
		if (!current_line && create_empty)
			status = CreateEmptyFirstLine(info, text_element);

		if (status == LAYOUT_CONTINUE)
			status = CloseVerticalLayout(info, BREAK);

		if (status == LAYOUT_END_FIRST_LINE)
			reflow_state->break_before_content = text_element;
	}

	if (status != LAYOUT_CONTINUE)
		return status;

	reflow_state->forced_line_end = TRUE;

	return LAYOUT_CONTINUE;
}

void
Container::SetMinimalLineHeight(LayoutInfo &info, const HTMLayoutProperties &props)
{
	OP_ASSERT(reflow_state && reflow_state->reflow_line);
	OP_ASSERT(reflow_state->reflow_line->IsLine() && ((Line*)reflow_state->reflow_line)->HasContent());

	if (info.hld_profile->IsInStrictLineHeightMode())
	{
		LayoutCoord line_height;
		LayoutCoord line_ascent;

		props.GetStrictLineDimensions(info.visual_device, line_height, line_ascent);
		reflow_state->line_state.GrowUncommittedLineHeight(line_height, line_ascent, line_height - line_ascent);
	}
}

/** Commit pending line content. */

LAYST
Container::CommitLineContent(LayoutInfo& info, BOOL commit_all, BOOL keep_min_widths)
{
	BOOL content_allocated = reflow_state->content_allocated;

	if (content_allocated || reflow_state->GetNoncontentWidth() || commit_all && reflow_state->break_before_content)
	{
		LayoutCoord leading_whitespace = reflow_state->GetLeadingWhitespaceWidth();
		LayoutCoord noncontent_width = reflow_state->GetNoncontentWidth();
		Line* line = GetReflowLine();
		short words = reflow_state->words;
		LayoutCoord word_width = reflow_state->GetWordWidth() + noncontent_width;
		BOOL has_content = content_allocated || noncontent_width;

		if (has_content)
			reflow_state->line_has_content = TRUE;

		reflow_state->content_allocated = FALSE;
		reflow_state->words = 0;

		if (reflow_state->break_before_content)
		{
			if (!line ||
				word_width > line->GetSpaceLeft() ||
				reflow_state->max_line_height < reflow_state->line_state.uncommitted.height_above_baseline + reflow_state->line_state.uncommitted.height_below_baseline ||
				!reflow_state->is_css_first_line && line->IsFirstLine())
			{
				// No line, no more space on line, or finished with ::first-line

				LAYST status = GetNewLine(info, reflow_state->break_before_content, word_width);

				if (status != LAYOUT_CONTINUE)
					// first-line was overflowed or we ran out of memory, stop layout

					return status;

				line = (Line*) reflow_state->reflow_line;

				if (!leading_whitespace &&
					!(reflow_state->list_item_marker_state & INSIDE_MARKER_BEFORE_COMMIT))
					++words;
			}

			reflow_state->break_before_content = NULL;
		}

		if (line)
		{
			if (!line->HasContent() && has_content)
			{
				line->SetHasContent(TRUE);
				SetMinimalLineHeight(info, *placeholder->GetCascade()->GetProps());
			}

			line->AllocateSpace(word_width, leading_whitespace, words, reflow_state->line_state.uncommitted.height_above_baseline, reflow_state->line_state.uncommitted.height_below_baseline);
		}

		reflow_state->CommitPendingInlineContent(line);
		reflow_state->list_item_marker_state &= ~INSIDE_MARKER_BEFORE_COMMIT;

		if (reflow_state->calculate_min_max_widths &&
			line && (content_allocated || reflow_state->pending_line_max_width))
		{
			LayoutProperties* cascade = placeholder->GetCascade();
			const HTMLayoutProperties& props = *cascade->GetProps();

			if (leading_whitespace)
				keep_min_widths = FALSE;

			if (reflow_state->available_line_max_width < reflow_state->pending_line_max_width)
			{
				// First finish current "virtual" line, if any.

				reflow_state->FinishVirtualLine();

				// Then start new "virtual" line.

				reflow_state->virtual_line_min_height = reflow_state->pending_virtual_line_min_height;
				reflow_state->virtual_line_ascent = reflow_state->pending_virtual_line_ascent;
				reflow_state->virtual_line_descent = reflow_state->pending_virtual_line_descent;

				if (info.hld_profile->IsInStrictLineHeightMode())
				{
					LayoutCoord line_height;
					LayoutCoord line_ascent;

					props.GetStrictLineDimensions(info.visual_device, line_height, line_ascent);
					reflow_state->GrowVirtualLineHeight(line_height, line_ascent, line_height - line_ascent);
				}

				reflow_state->cur_elm_min_stack_position = reflow_state->min_reflow_position + reflow_state->margin_state.GetHeightNonPercent();

				// Examine the block formatting context, to see if there might be any floats that affect us.

				LayoutCoord left_floats_max_width;
				LayoutCoord right_floats_max_width;
				SpaceManager* space_manager = placeholder->GetLocalSpaceManager();

				if (!space_manager)
					space_manager = cascade->space_manager;

				GetFloatsMaxWidth(space_manager, reflow_state->cur_elm_min_stack_position, reflow_state->GetVirtualLineHeight(), reflow_state->pending_line_max_width, left_floats_max_width, right_floats_max_width);

				reflow_state->floats_max_width = left_floats_max_width + right_floats_max_width;
				reflow_state->available_line_max_width = reflow_state->total_max_width - reflow_state->floats_max_width;
			}
			else
			{
				/* Elements starting with negative position may not grow
				   line's min width - make sure that we do not propagate
				   min width of such an element. */

				if (reflow_state->line_max_width < 0 && !line->GetPreviousElement())
					reflow_state->pending_line_min_width += reflow_state->line_max_width;

				reflow_state->GrowVirtualLineHeight(reflow_state->pending_virtual_line_min_height,
													reflow_state->pending_virtual_line_ascent,
													reflow_state->pending_virtual_line_descent);
			}

			reflow_state->pending_virtual_line_min_height = reflow_state->pending_virtual_line_ascent = reflow_state->pending_virtual_line_descent = LayoutCoord(0);

			LayoutCoord pending_max_width = leading_whitespace + reflow_state->pending_line_max_width;

			reflow_state->pending_line_max_width = LayoutCoord(0);
			reflow_state->line_max_width += pending_max_width;

			if (reflow_state->available_line_max_width - LayoutCoord(LAYOUT_COORD_MAX) < pending_max_width)
				reflow_state->available_line_max_width -= pending_max_width;

			LayoutCoord min_width = reflow_state->pending_line_min_width;
			LayoutCoord max_width = reflow_state->floats_max_width + reflow_state->line_max_width - leading_whitespace;

			if (min_width < 0)
				min_width = LayoutCoord(0);

			if (max_width < 0)
				max_width = LayoutCoord(0);

			PropagateMinMaxWidths(info, min_width, min_width + reflow_state->weak_content_width, max_width);

			if (reflow_state->first_unbreakable_weak_element)
			{
				if (reflow_state->weak_content_width)
					if (reflow_state->pending_line_min_width > 0)
					{
						for (HTML_Element* element = reflow_state->first_unbreakable_weak_element; element; element = element->NextActual())
						{
							if (element->GetLayoutBox() && element->GetLayoutBox()->IsWeakContent())
							{
								UINT32 new_unbreakable_width;
								UINT32 unbreakable_width = (UINT32) element->GetSpecialNumAttr(Markup::LAYOUTA_UNBREAKABLE_WIDTH, SpecialNs::NS_LAYOUT);
								UINT16 weak_unbreakable_width = unbreakable_width >> 16;
								UINT16 min_unbreakable_width = unbreakable_width & 0x0000ffff;

								if (weak_unbreakable_width < reflow_state->weak_content_width)
									weak_unbreakable_width = reflow_state->weak_content_width;

								if (min_unbreakable_width < reflow_state->pending_line_min_width)
									min_unbreakable_width = reflow_state->pending_line_min_width;

								new_unbreakable_width = (weak_unbreakable_width << 16) | min_unbreakable_width;

								if (new_unbreakable_width != unbreakable_width)
									if (element->SetSpecialAttr(Markup::LAYOUTA_UNBREAKABLE_WIDTH, ITEM_TYPE_NUM, (void*)(INTPTR)new_unbreakable_width, FALSE, SpecialNs::NS_LAYOUT) < 0)
										return LAYOUT_OUT_OF_MEMORY;
							}

							if (element == reflow_state->last_unbreakable_weak_element)
								break;
						}
					}

				reflow_state->first_unbreakable_weak_element = NULL;
				reflow_state->last_unbreakable_weak_element = NULL;
			}

			if (!keep_min_widths)
			{
				reflow_state->pending_line_min_width = LayoutCoord(0);
				reflow_state->weak_content_width = LayoutCoord(0);
			}
		}
	}
	else
		if (commit_all && !keep_min_widths && reflow_state->calculate_min_max_widths)
		{
			reflow_state->pending_line_min_width = LayoutCoord(0);
			reflow_state->weak_content_width = LayoutCoord(0);
		}

	if (commit_all)
		reflow_state->collapse_whitespace = TRUE;

	reflow_state->line_state.ResetUncommitted();

	return LAYOUT_CONTINUE;
}

/** Notify container that an image has got its size reduced. */

void
Container::ImageSizeReduced()
{
	for (HTML_Element* element = reflow_state->first_unbreakable_weak_element; element; element = element->NextActual())
	{
		if (element->GetLayoutBox() && element->GetLayoutBox()->IsWeakContent() &&
		    element->HasSpecialAttr(Markup::LAYOUTA_UNBREAKABLE_WIDTH, SpecialNs::NS_LAYOUT))
		{
#ifdef DEBUG_ENABLE_OPASSERT
			int res =
#endif // DEBUG_ENABLE_OPASSERT
				element->SetSpecialAttr(Markup::LAYOUTA_UNBREAKABLE_WIDTH, ITEM_TYPE_NUM, (void*)(INTPTR)0, FALSE, SpecialNs::NS_LAYOUT);
			OP_ASSERT(res >= 0 || !"SetSpecialAttr should not fail when we have first checked for the presence of the attribute");
		}

		if (element == reflow_state->last_unbreakable_weak_element)
			break;
	}
}

/** Get the percent of normal width when page is shrunk in ERA. */

/* virtual */ int
Container::GetPercentOfNormalWidth() const
{
	int percent = 100;

	for (VerticalLayout* vertical_layout = (VerticalLayout*) layout_stack.First(); vertical_layout; vertical_layout = vertical_layout->Suc())
		if (vertical_layout->IsBlock())
		{
			int new_percent = ((BlockBox*)vertical_layout)->GetPercentOfNormalWidth();

			if (new_percent < percent)
				percent = new_percent;
		}

	return percent;
}

/** Attempt to lay out run-in. */

OP_BOOLEAN
Container::LayoutRunIn(LayoutProperties* child_cascade, LayoutInfo& info, Content_Box* box, LayoutCoord pending_compact_space)
{
	HTML_Element* run_in_element = box->GetHtmlElement();
	Container* old_container = NULL;
	LayoutProperties* parent_cascade = child_cascade;
	HTML_Element* parent = run_in_element->Parent();

	// Search for parent cascade - find common container for this container and the run-in.

	while ((parent_cascade = parent_cascade->Pred()) != NULL)
		if (parent_cascade->html_element == parent)
		{
			old_container = parent->GetLayoutBox()->GetContainer();
			break;
		}

	if (old_container && old_container != this)
	{
		// Found it

		AutoDeleteHead props_list;

		if (LayoutProperties* run_in_parent_cascade = parent_cascade->CloneCascade(props_list, info.hld_profile))
		{
			InlineRunInBox* run_in = box->IsInlineRunInBox() ? (InlineRunInBox*) box : NULL;

			if (LayoutProperties* run_in_cascade = run_in_parent_cascade->GetChildCascade(info, run_in_element, !run_in))
			{
				if (!run_in)
				{
					if (!run_in_cascade->ConvertToInlineRunin(info, old_container))
						return OpStatus::ERR_NO_MEMORY;

					run_in = (InlineRunInBox*) run_in_element->GetLayoutBox();
				}

				if (run_in->IsInlineCompactBox())
				{
					InlineVerticalAlignment dummy;

					if (!AllocateNoncontentLineSpace(run_in_element,
													 -pending_compact_space,
													 -pending_compact_space,
													 LayoutCoord(0),
													 dummy,
													 pending_compact_space,
													 LayoutCoord(0)))
						return OpStatus::ERR_NO_MEMORY;
				}

				OP_ASSERT(run_in_cascade->container == this);

				if (run_in->Layout(run_in_cascade, info) != LAYOUT_CONTINUE) // FIXME: not necessarily OOM
					return OpStatus::ERR_NO_MEMORY;

				/* The run-in just was just laid out as an inline since it found a
				   block to run into, so we obviously don't want to convert it back to
				   a block. */

				OP_ASSERT(run_in->IsInlineRunInBox());
				run_in->SetRunInBlockConversion(FALSE, info);

				RETURN_IF_MEMORY_ERROR(run_in_cascade->Finish(&info));

				child_cascade->GetProps()->SetDisplayProperties(info.visual_device);

				if (run_in->IsInlineCompactBox())
				{
					reflow_state->AllocateLineVirtualSpace(LayoutCoord(0), LayoutCoord(0), -GetLinePendingPosition());
					reflow_state->collapse_whitespace = TRUE;
					reflow_state->last_base_character = 0;

					/* If this assert fails, we should call SetRunInBlockConversion
					   on run_in with convert equal to TRUE, but currently that
					   would cause an infinite loop. Leaving without block
					   conversion, will result in some ugly layout.
					   Also currently, during the layout, we make sure that
					   this assert is not going to trigger. That happens
					   inside Container::GetNewBlockStage2, when the compact
					   is always previously laid out as a BlockCompactBox. */
					OP_ASSERT(reflow_state->GetLeadingWhitespaceWidth() > 0);
				}
			}
			else
				return OpStatus::ERR_NO_MEMORY;
		}
		else
			return OpStatus::ERR_NO_MEMORY;

		return OpBoolean::IS_TRUE;
	}

	/* The container was terminated, the run-in's parent is not a container
	   (e.g. the run-in is child of an inline box), or something failed. */

	return OpBoolean::IS_FALSE;
}

/** Allocate whitespace on current line. */

BOOL
Container::AllocateLineWhitespace(LayoutCoord whitespace, HTML_Element* element)
{
	LayoutCoord allocated_position = GetLinePendingPosition();

	reflow_state->AllocateLineVirtualSpace(LayoutCoord(0), LayoutCoord(0), whitespace);
	reflow_state->collapse_whitespace = TRUE;

	if (whitespace > 0)
		reflow_state->content_allocated = TRUE;

	if (reflow_state->line_state.uncommitted.bounding_box_left > allocated_position + reflow_state->relative_x)
		reflow_state->line_state.uncommitted.bounding_box_left = allocated_position + reflow_state->relative_x;

#ifdef SUPPORT_TEXT_DIRECTION
	if (reflow_state->bidi_calculation)
		if (!AppendBidiStretch(BIDI_WS, whitespace, element, allocated_position, NULL))
			return FALSE;
#endif

	return TRUE;
}

/** Allocate non-content (border, margin or padding) space on current line. */

BOOL
Container::AllocateNoncontentLineSpace(HTML_Element* element, LayoutCoord box_width, LayoutCoord min_width, LayoutCoord shadow_width, const InlineVerticalAlignment& valign, LayoutCoord negative_margin_left, LayoutCoord negative_margin_right)
{
#ifdef SUPPORT_TEXT_DIRECTION
	if (reflow_state->bidi_calculation && box_width)
		if (!AppendBidiStretch(BIDI_ON, box_width, element, GetLinePendingPosition(), NULL))
			return FALSE;
#endif

	reflow_state->AllocateLineVirtualSpace(LayoutCoord(0), box_width, LayoutCoord(0));

	if (reflow_state->calculate_min_max_widths)
	{
		reflow_state->pending_line_min_width += min_width;
		reflow_state->pending_line_max_width += min_width;
	}

	if (!reflow_state->break_before_content)
		// If line was broken before us

		reflow_state->break_before_content = element;

	RECT r = { - shadow_width - negative_margin_left,
			   - shadow_width,
			   box_width + shadow_width + negative_margin_right,
			   valign.box_below_baseline + valign.box_above_baseline };

	RECT c = { LAYOUT_COORD_HALF_MAX, LAYOUT_COORD_HALF_MAX, LAYOUT_COORD_HALF_MIN, LAYOUT_COORD_HALF_MIN };

#ifdef CSS_TRANSFORMS
	if (reflow_state->transform)
		r = reflow_state->transform->GetTransformedBBox(r);
#endif

	if (box_width)
	{
		LayoutCoord logical_above_baseline = valign.loose ? LAYOUT_COORD_HALF_MIN : valign.logical_above_baseline;
		LayoutCoord logical_below_baseline = valign.loose ? LAYOUT_COORD_HALF_MIN : valign.logical_below_baseline;
		LayoutCoord minimum_line_height = valign.loose ? MAX(valign.minimum_line_height, valign.logical_above_baseline + valign.logical_below_baseline) : valign.minimum_line_height;

		reflow_state->line_state.uncommitted.minimum_height = MAX(reflow_state->line_state.uncommitted.minimum_height, minimum_line_height);
		reflow_state->line_state.uncommitted.height_above_baseline = MAX(reflow_state->line_state.uncommitted.height_above_baseline, logical_above_baseline);
		reflow_state->line_state.uncommitted.height_below_baseline = MAX(reflow_state->line_state.uncommitted.height_below_baseline, logical_below_baseline);
	}

	reflow_state->line_state.AdjustPendingBoundingBox(r, c, reflow_state->relative_x, reflow_state->relative_y, GetLinePendingPosition(), valign);

	if (reflow_state->calculate_min_max_widths && min_width)
	{
		LayoutCoord logical_above_baseline_nonpercent = valign.loose ? LAYOUT_COORD_HALF_MIN : valign.logical_above_baseline_nonpercent;
		LayoutCoord logical_below_baseline_nonpercent = valign.loose ? LAYOUT_COORD_HALF_MIN : valign.logical_below_baseline_nonpercent;
		LayoutCoord minimum_line_height_nonpercent = valign.loose ? MAX(valign.minimum_line_height_nonpercent, valign.logical_above_baseline_nonpercent + valign.logical_below_baseline_nonpercent) : valign.minimum_line_height_nonpercent;

		if (reflow_state->pending_virtual_line_min_height < minimum_line_height_nonpercent)
			reflow_state->pending_virtual_line_min_height = minimum_line_height_nonpercent;

		if (reflow_state->pending_virtual_line_ascent < logical_above_baseline_nonpercent)
			reflow_state->pending_virtual_line_ascent = logical_above_baseline_nonpercent;

		if (reflow_state->pending_virtual_line_descent < logical_below_baseline_nonpercent)
			reflow_state->pending_virtual_line_descent = logical_below_baseline_nonpercent;
	}

	return TRUE;
}

void
Container::AllocateAbsPosContentBox(const InlineVerticalAlignment& valign, unsigned short box_right, long box_bottom)
{
	RECT c = { 0, 0, box_right, box_bottom };

#ifdef CSS_TRANSFORMS
	if (reflow_state->transform)
		c = reflow_state->transform->GetTransformedBBox(c);
#endif

	reflow_state->line_state.AdjustPendingAbsPosContentBox(c, reflow_state->relative_x, reflow_state->relative_y, GetLinePendingPosition(), valign);
}

/** Allocate "space" for a WBR element. */

LAYST
Container::AllocateWbr(LayoutInfo& info, HTML_Element* html_element)
{
	const LayoutCoord o(0); // Avoid an insane amount of "LayoutCoord(0)" below.
	InlineVerticalAlignment valign;

	// Initialize 'valign' properly.

	html_element->GetLayoutBox()->GetVerticalAlignment(&valign);

	/* Allocate a U+200B character (ZERO-WIDTH SPACE). Don't allow breaking
	   before it... */

	LAYST st = AllocateLineSpace(html_element, info, o, o, o, valign, o, o, o, o, o, o, o, o, 0x200b, LB_NO, FALSE);

	/* ... but allow breaking after it (a regular text box would call
	   container->SetLastBaseCharacter() to achieve this). */

	reflow_state->last_base_character = 0x200b;

	return st;
}

BOOL
Container::IsCssFirstLetter() const
{
	return reflow_state && reflow_state->has_css_first_letter != FIRST_LETTER_NO
		&& ((reflow_state->list_item_marker_state & MARKER_BEFORE_NEXT_ELEMENT)
		|| (reflow_state->GetLineCurrentPosition() == 0 && reflow_state->GetWordWidth() == 0
		&& (!reflow_state->reflow_line || !reflow_state->reflow_line->IsInStack())));
}

#ifdef SUPPORT_TEXT_DIRECTION

/** Append bidi stretch. */

BOOL
Container::AppendBidiStretch(BidiCategory type,
							 int width,
							 HTML_Element* element,
							 LayoutCoord virtual_position,
							 const LayoutInfo* info)
{
	long pos = reflow_state->bidi_calculation->AppendStretch(type, width, element, virtual_position);

	if (pos == LONG_MAX)
		return FALSE;

	if (pos == LONG_MAX - 1) // last stretch was not a neutral, dont bother
		return TRUE;

	if (!info)
	{
		OP_ASSERT(0);
		return TRUE;
	}

	if (pos < reflow_state->GetLineCurrentPosition())
		// Must update previous lines

		for (VerticalLayout* vertical_layout = (VerticalLayout*)layout_stack.Last(); vertical_layout && vertical_layout->IsLine(); vertical_layout = vertical_layout->GetPreviousElement(TRUE))
		{
			Line* line = (Line*) vertical_layout;

			AbsoluteBoundingBox bbox;
			line->GetBoundingBox(bbox);

			info->visual_device->UpdateRelative(bbox.GetX() + line->GetX(), bbox.GetY() + line->GetY(), bbox.GetWidth(), bbox.GetHeight());

			if (line->GetStartPosition() < pos)
				break;
		}

	return TRUE;
}

/** Reconstruct bidi status when starting in the middle of a container. */

BOOL
Container::ReconstructBidiStatus(LayoutInfo& info)
{
	// Reconstruct bidi status

	long pending_content = GetLinePendingPosition() - reflow_state->GetLineCurrentPosition();

	if (!InitBidiCalculation(NULL))
		return FALSE;

	for (VerticalLayout* vertical_layout = (VerticalLayout*) layout_stack.First(); vertical_layout; vertical_layout = vertical_layout->Suc())
		if (vertical_layout->IsInStack(FALSE))
			if (vertical_layout->IsLine())
			{
				Line* line = (Line*)vertical_layout;

				if (reflow_state->bidi_calculation->AppendStretch(BIDI_L,
																  line->GetUsedSpace() + line->GetTrailingWhitespace(),
																  line->GetStartElement(),
																  line->GetStartPosition()) == LONG_MAX)
					return FALSE; // OOM
			}

	if (pending_content)
		if (reflow_state->bidi_calculation->AppendStretch(BIDI_L,
														  pending_content,
														  reflow_state->break_before_content,
														  reflow_state->GetLineCurrentPosition()) == LONG_MAX)
			return FALSE; // OOM

	if (info.workplace->GetBidiStatus() == LOGICAL_MAYBE_RTL)
		info.workplace->SetBidiStatus(LOGICAL_HAS_RTL);

	return TRUE;
}

#endif

/** Push floats that should be below the current line below it. */

void
Container::PushFloatsBelowLine(LayoutInfo& info, Line* line)
{
	FloatingBox* moved_floating_box = reflow_state->first_moved_floating_box;

	reflow_state->first_moved_floating_box = NULL;

	/* Calculate the Y offset that was caused by the line. In some cases there
	   is no offset (offset <= 0). This happens when preceding floats, that
	   were unaffected by the current line, happened to end up below the line
	   for other reasons. We then don't need to push the floats further down
	   here, but we still need to allocate space for them and add them to the
	   space manager. */

	LayoutCoord offset = line->GetY() + line->GetLayoutHeight() - moved_floating_box->GetY();

	LayoutCoord dummy_container_bfc_min_y(0);
	LayoutCoord container_bfc_y(0);
	LayoutCoord container_bfc_x(0);

	GetBfcOffsets(container_bfc_x, container_bfc_y, dummy_container_bfc_min_y);

	SpaceManager* space_manager = placeholder->GetLocalSpaceManager();

	if (!space_manager)
		space_manager = placeholder->GetCascade()->space_manager;

	moved_floating_box->InvalidateFloatReflowCache();

#ifdef DEBUG
	FloatingBox* last_unaffected_float = moved_floating_box->link.Pred() ? moved_floating_box->link.Pred()->float_box : NULL;
#endif // DEBUG

	VerticalLayout* vertical_layout;

	// First take the floats out of the space manager.

	for (vertical_layout = moved_floating_box; vertical_layout != line; vertical_layout = vertical_layout->Suc())
		if (vertical_layout->IsBlock() && ((BlockBox*) vertical_layout)->IsFloatingBox())
			((FloatingBox*) vertical_layout)->link.Out();

#ifdef DEBUG
	// Make sure that we took out all the remaining floats, before we put them back in.

	OP_ASSERT(space_manager->GetLastFloat() == last_unaffected_float);
#endif // DEBUG

	// Then reallocate space for the floats at the correct position and put them back into the space manager.

	for (vertical_layout = moved_floating_box; vertical_layout != line; vertical_layout = vertical_layout->Suc())
		if (vertical_layout->IsBlock() && ((BlockBox*) vertical_layout)->IsFloatingBox())
		{
			FloatingBox* floating_box = (FloatingBox*) vertical_layout;

			AbsoluteBoundingBox bounding_box;
			LayoutCoord new_y = floating_box->GetY();
			LayoutCoord new_x = floating_box->GetX();
			LayoutCoord space = GetContentWidth();
			LayoutCoord full_width = floating_box->GetWidth() + floating_box->GetMarginLeft() + floating_box->GetMarginRight();

			if (offset > 0)
				new_y += offset;

			GetSpace(space_manager, new_y, new_x, space, full_width, LAYOUT_COORD_MAX);

			if (!floating_box->IsLeftFloat())
				new_x += space - full_width;

			new_y += floating_box->GetMarginTop();
			new_x += floating_box->GetMarginLeft();

			/* Set BFC offsets now. They should not contain offsets caused by
			   relative-positioning. */

			floating_box->SetBfcX(container_bfc_x + new_x);
			floating_box->SetBfcY(container_bfc_y + new_y);

			LayoutCoord relpos_left;
			LayoutCoord relpos_top;

			floating_box->GetRelativeOffsets(relpos_left, relpos_top);

			new_x += relpos_left;
			new_y += relpos_top;

			floating_box->SetX(new_x);
			floating_box->SetY(new_y);

			floating_box->GetBoundingBox(bounding_box);

			if (bounding_box.GetHeight() != LAYOUT_COORD_MAX)
				info.visual_device->UpdateRelative(bounding_box.GetX() + new_x, bounding_box.GetY() + new_y, bounding_box.GetWidth(), bounding_box.GetHeight());

			space_manager->AddFloat(floating_box);
			floating_box->PropagateBottomMargins(info);
		}
}

void
Container::SetLastBaseCharacterFromWhiteSpace(const CSSValue white_space)
{
	switch (white_space)
	{
	case CSS_VALUE_normal:
	case CSS_VALUE_pre_wrap:
		reflow_state->last_base_character = 0xFFFC;
		break;
	default:
		reflow_state->last_base_character = 0;
		break;
	}
}

/** Check if we can break line between this and last base character. */

LinebreakOpportunity
Container::GetLinebreakOpportunity(UnicodePoint base_character)
{
	if (reflow_state->last_base_character)
		if (Unicode::IsLinebreakOpportunity(reflow_state->last_base_character, base_character, reflow_state->collapse_whitespace))
			return LB_YES;

	// There was no last base character, or specifically not an opportunity for breaking

	return LB_NO;
}

/** Allocate space on current line. */

LAYST
Container::AllocateLineSpace(HTML_Element* element,
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
							 BOOL keep_min_widths)
{
	LayoutCoord logical_above_baseline = valign.loose ? LAYOUT_COORD_HALF_MIN : valign.logical_above_baseline;
	LayoutCoord logical_below_baseline = valign.loose ? LAYOUT_COORD_HALF_MIN : valign.logical_below_baseline;
	LayoutCoord minimum_line_height = valign.loose ? MAX(valign.minimum_line_height, valign.logical_above_baseline + valign.logical_below_baseline) : valign.minimum_line_height;
	LayoutCoord allocated_position = GetLinePendingPosition();

	reflow_state->line_has_content = TRUE;

	OP_ASSERT(break_property != -1);

	if (!reflow_state->break_before_content)
		// If line was broken before us

		reflow_state->break_before_content = element;

	if (reflow_state->GetLeadingWhitespaceWidth())
		++reflow_state->words;

	if (reflow_state->list_item_marker_state & MARKER_BEFORE_NEXT_ELEMENT)

		/* The below check is needed, because this method can be called
		   more than once from a marker's box. */
		if (!element->GetIsListMarkerPseudoElement() && !element->Parent()->GetIsListMarkerPseudoElement())
		{
			SetHasFlexibleMarker(FALSE);
			reflow_state->list_item_marker_state &= ~MARKER_BEFORE_NEXT_ELEMENT;
		}

#ifdef SUPPORT_TEXT_DIRECTION
	BidiCategory bidi_type = ch ? Unicode::GetBidiCategory(ch) : BIDI_ON;

	switch (info.workplace->GetBidiStatus())
	{
	case LOGICAL_MAYBE_RTL:
		if (bidi_type != BIDI_R &&
			bidi_type != BIDI_AL &&
			bidi_type != BIDI_AN &&
			bidi_type != BIDI_LRO &&
			bidi_type != BIDI_RLO &&
			bidi_type != BIDI_LRE &&
			bidi_type != BIDI_RLE &&
			bidi_type != BIDI_PDF)
			break;

	case VISUAL_ENCODING:
	case LOGICAL_HAS_RTL:
		if (!reflow_state->bidi_calculation)
			if (!ReconstructBidiStatus(info))
				return LAYOUT_OUT_OF_MEMORY;

		if (!AppendBidiStretch(bidi_type, box_width, element, allocated_position, &info))
			return LAYOUT_OUT_OF_MEMORY;

		break;
	}

	switch (bidi_type)
	{
	case BIDI_EN:
	case BIDI_ES:
	case BIDI_ET:
	case BIDI_AN:
	case BIDI_CS:
		reflow_state->true_table_content_width += box_width;
		break;

	case BIDI_LRO:
	case BIDI_RLO:
	case BIDI_LRE:
	case BIDI_RLE:
	case BIDI_PDF:
		// These do not contribute to inline layout

		return LAYOUT_CONTINUE;
	}
#else
	if ((ch >= 0x2B && ch <= 0x3B) || // '+' ',' '-' '.' '/' '0-9' ':' ';'
		(ch == 0x25))                // '%'
		reflow_state->true_table_content_width += box_width;
#endif // SUPPORT_TEXT_DIRECTION

	RECT bounding_rect = { - bbox_left, - bbox_top,
		box_width + bbox_right,
		valign.box_below_baseline + valign.box_above_baseline + bbox_bottom };

	RECT content_rect = { 0, 0, box_width + content_overflow_right, valign.box_below_baseline + valign.box_above_baseline + content_overflow_bottom };

#ifdef CSS_TRANSFORMS
	if (reflow_state->transform)
	{
		bounding_rect = reflow_state->transform->GetTransformedBBox(bounding_rect);
		content_rect = reflow_state->transform->GetTransformedBBox(content_rect);
	}
#endif

	if (break_property == LB_YES)
	{
		// May break line here; commit any content we might have

		LAYST status = CommitLineContent(info, FALSE, keep_min_widths);

		if (status != LAYOUT_CONTINUE)
			// first-line was overflowed or we ran out of memory, stop layout

			return status;

		if (abspos_content_right || abspos_content_bottom)
			AllocateAbsPosContentBox(valign, abspos_content_right, abspos_content_bottom);

		reflow_state->line_state.uncommitted.minimum_height = minimum_line_height;
		reflow_state->line_state.uncommitted.height_above_baseline = logical_above_baseline;
		reflow_state->line_state.uncommitted.height_below_baseline = logical_below_baseline;

		reflow_state->line_state.AdjustPendingBoundingBox(bounding_rect, content_rect, reflow_state->relative_x, reflow_state->relative_y, allocated_position, valign);
		reflow_state->AllocateLineVirtualSpace(box_width, LayoutCoord(0), LayoutCoord(0));

		if (!reflow_state->break_before_content)
			reflow_state->break_before_content = element;
	}
	else
	{
		if (abspos_content_right || abspos_content_bottom)
			AllocateAbsPosContentBox(valign, abspos_content_right, abspos_content_bottom);

		reflow_state->line_state.uncommitted.minimum_height = MAX(reflow_state->line_state.uncommitted.minimum_height, minimum_line_height);
		reflow_state->line_state.uncommitted.height_above_baseline = MAX(reflow_state->line_state.uncommitted.height_above_baseline, logical_above_baseline);
		reflow_state->line_state.uncommitted.height_below_baseline = MAX(reflow_state->line_state.uncommitted.height_below_baseline, logical_below_baseline);

		reflow_state->line_state.AdjustPendingBoundingBox(bounding_rect, content_rect, reflow_state->relative_x, reflow_state->relative_y, allocated_position, valign);

		LayoutCoord leading_whitespace_width = reflow_state->GetLeadingWhitespaceWidth();

		reflow_state->AllocateLineVirtualSpace(box_width + leading_whitespace_width, LayoutCoord(0),-leading_whitespace_width);

		if (reflow_state->calculate_min_max_widths)
		{
			reflow_state->pending_line_min_width += leading_whitespace_width;
			reflow_state->pending_line_max_width += leading_whitespace_width;
		}
	}

	reflow_state->content_allocated = TRUE;
	reflow_state->collapse_whitespace = FALSE;

	if (reflow_state->calculate_min_max_widths)
	{
		LayoutCoord logical_above_baseline_nonpercent = valign.loose ? LAYOUT_COORD_HALF_MIN : valign.logical_above_baseline_nonpercent;
		LayoutCoord logical_below_baseline_nonpercent = valign.loose ? LAYOUT_COORD_HALF_MIN : valign.logical_below_baseline_nonpercent;
		LayoutCoord minimum_line_height_nonpercent = valign.loose ? MAX(valign.minimum_line_height_nonpercent, valign.logical_above_baseline_nonpercent + valign.logical_below_baseline_nonpercent) : valign.minimum_line_height_nonpercent;

		if (reflow_state->pending_virtual_line_min_height < minimum_line_height_nonpercent)
			reflow_state->pending_virtual_line_min_height = minimum_line_height_nonpercent;

		if (reflow_state->pending_virtual_line_ascent < logical_above_baseline_nonpercent)
			reflow_state->pending_virtual_line_ascent = logical_above_baseline_nonpercent;

		if (reflow_state->pending_virtual_line_descent < logical_below_baseline_nonpercent)
			reflow_state->pending_virtual_line_descent = logical_below_baseline_nonpercent;

		reflow_state->pending_line_min_width += min_width;
		reflow_state->pending_line_max_width += max_width;
	}

	if (element->GetInserted() == HE_INSERTED_BY_LAYOUT &&
		(element->GetIsListMarkerPseudoElement() || element->Parent() && element->Parent()->GetIsListMarkerPseudoElement()))
	{
		if (GetPlaceholder()->GetCascade()->GetProps()->list_style_pos == CSS_VALUE_outside)
		{
			// Cancel whitespace collapse and prevent word wrap just after the outside marker.
			reflow_state->collapse_whitespace = TRUE;
			reflow_state->last_base_character = 0;
		}
		else
			reflow_state->list_item_marker_state |= INSIDE_MARKER_BEFORE_COMMIT;

		reflow_state->list_item_marker_state |=	MARKER_BEFORE_NEXT_ELEMENT;
	}

	return LAYOUT_CONTINUE;
}

/** Allocate space for weak content on current line. */

void
Container::AllocateWeakContentSpace(HTML_Element* element, LayoutCoord additional_weak_space, BOOL normal_mode)
{
	reflow_state->line_has_content = TRUE;

	if (additional_weak_space)
		if (reflow_state->calculate_min_max_widths)
			if (!normal_mode)
			{
				reflow_state->weak_content_width += LayoutCoord(additional_weak_space);

				if (element->GetLayoutBox()->IsWeakContent())
				{
					reflow_state->last_unbreakable_weak_element = element;

					if (!reflow_state->first_unbreakable_weak_element)
						reflow_state->first_unbreakable_weak_element = element;
				}
			}
}

/** Return the maximum word width to fit at the end of current line if line
	cannot be broken, or the size of a new line minus uncommitted content. */

LayoutCoord
Container::GetMaxWordWidth(LinebreakOpportunity break_property)
{
	/* This is not accurate. We don't consider floats, text-indent or list-items ... */

	LayoutCoord max_word_width = GetContentWidth();

	if (break_property == LB_NO)
		// uncommitted content cannot be broken, subtract uncommitted width

		max_word_width -= reflow_state->GetWordWidth()+ reflow_state->GetNoncontentWidth();

	return max_word_width;
}

/** Get start element of current line. */

HTML_Element*
Container::GetStartElementOfCurrentLine() const
{
	if (reflow_state->reflow_line && reflow_state->reflow_line->IsLine())
		return ((Line*)reflow_state->reflow_line)->GetStartElement();
	else
		return reflow_state->break_before_content;
}

#ifdef CSS_TRANSFORMS

void
Container::SetTransform(const AffineTransform &transform, AffineTransform *&previous_transform)
{
	previous_transform = reflow_state->transform;

	reflow_state->transform = OP_NEW(AffineTransform, ());
	if (!reflow_state->transform)

		/* Failed to allocate a transform. We will not adjust the
		   bounding box properly for this container. */

		return;

	if (previous_transform)
	{
		*reflow_state->transform = *previous_transform;
		reflow_state->transform->PostMultiply(transform);
	}
	else
		*reflow_state->transform = transform;
}

void
Container::ResetTransform(AffineTransform *transform)
{
	OP_DELETE(reflow_state->transform);
	reflow_state->transform = transform;
}

#endif // CSS_TRANSFORMS

/** Find previous non-float, non-absolute element. */

VerticalLayout*
VerticalLayout::GetPreviousElement(BOOL include_floats) const
{
	VerticalLayout* pred = Pred();

	while (pred && !pred->IsInStack(include_floats))
		// Skip over elements that are not in stack

		pred = pred->Pred();

	return pred;
}

/** Allocate space on this line. */

void
Line::AllocateSpace(LayoutCoord box_width, LayoutCoord leading_whitespace, unsigned short words, LayoutCoord box_above_baseline, LayoutCoord box_below_baseline)
{
	used_space += box_width + leading_whitespace;
	packed2.trailing_whitespace = leading_whitespace;
	packed.number_of_words += words;

	if (box_above_baseline + box_below_baseline > 0)
	{
		// Check height and baseline

		if (height == 0)
			baseline = box_above_baseline;

		if (baseline < box_above_baseline)
		{
			height += box_above_baseline - baseline;
			baseline = box_above_baseline;
		}

		if (height < baseline + box_below_baseline)
			height = baseline + box_below_baseline;
	}
}

/** Calculate intrinsic size. */

/* virtual */ OP_STATUS
FormContent::CalculateIntrinsicSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord& width, LayoutCoord& height, int &ratio)
{
	HTML_Element* html_element = cascade->html_element;
	const HTMLayoutProperties& props = *cascade->GetProps();
	int preferred_width = 0;
	int preferred_height = 0;

	form_object->GetPreferredSize(preferred_width, preferred_height, html_element->GetCols(), html_element->GetRows(), html_element->GetSize(), props);

	width = LayoutCoord(preferred_width);
	height = LayoutCoord(preferred_height);
	FormBoxToContentBox(cascade, width, height);

	return OpStatus::OK;
}

/* virtual */ LAYST
FormContent::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	// updated bounding box to include any extra space
	AbsoluteBoundingBox bb;
	UINT8 left, top, right, bottom;
	GetUsedMargins(left, top, right, bottom);
	bb.Set(LayoutCoord(-left), LayoutCoord(-top), LayoutCoord(replaced_width + left + right), LayoutCoord(replaced_height + top + bottom));
	placeholder->UpdateBoundingBox(bb, FALSE);

	if (form_object)
		// Pass properties to the form object, in case they have changed.

		form_object->InitialiseWidget(*cascade->GetProps());

	return LAYOUT_CONTINUE;
}
/** Update size of content.

	@return OpBoolean::IS_TRUE if size changed, OpBoolean::IS_FALSE if not, OpStatus::ERR_NO_MEMORY on OOM. */

/* virtual */ OP_BOOLEAN
FormContent::SetNewSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord width, LayoutCoord height,
						LayoutCoord modified_horizontal_border_padding)
{
	OP_BOOLEAN size_changed = ReplacedContent::SetNewSize(cascade, info, width, height,
															  modified_horizontal_border_padding);

	if (OpStatus::IsSuccess(size_changed))
	{
		BorderBoxToFormBox(cascade, width, height);
		form_object->SetSize(width, height);

#ifdef SKIN_SUPPORT
		HTMLayoutProperties* props = cascade->GetProps();
		UINT8 left, top, right, bottom;
		// in order for skin handling to work as expected we sometimes
		// need to paint in the margins
		if (form_object->GetWidget()->UseMargins(props->margin_left, props->margin_top, props->margin_right, props->margin_bottom,
												 left, top, right, bottom))
		{
			packed_used_margins.left = left;
			packed_used_margins.top = top;
			packed_used_margins.right = right;
			packed_used_margins.bottom = bottom;
			width  += LayoutCoord(left + right);
			height += LayoutCoord(top + bottom);
			form_object->SetSize(width, height);
		}
		else
		{
			packed_used_margins.left = 0;
			packed_used_margins.top = 0;
			packed_used_margins.right = 0;
			packed_used_margins.bottom = 0;
		}
#endif // SKIN_SUPPORT

		if (size_changed == OpBoolean::IS_TRUE)
			ComputeBaseline(*cascade->GetProps());
	}

	return size_changed;
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
FormContent::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	// Need to create form_object now, because it is needed to calculate intrinsic size.

	const HTMLayoutProperties& props = *cascade->GetProps();

	if (form_object_dirty)
        Disable(info.doc);

	if (!form_object)
		if (ShowForm(cascade, info.doc, LayoutCoord(0), LayoutCoord(0)) == OpStatus::ERR_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;

	SetFormMinMaxWidth(props.min_width, props.max_width);

	return ReplacedContent::ComputeSize(cascade, info);
}

void
FormContent::ComputeBaseline(const HTMLayoutProperties& props)
{
	if (GetSelectContent() || GetHtmlElement()->Type() == Markup::HTE_TEXTAREA)
	{
		// The baseline is a bit higher up than replaced_height.

		baseline = replaced_height - LayoutCoord(props.border.bottom.width + props.padding_bottom + props.font_descent);
		nonpercent_baseline = minimum_height - props.GetNonPercentBottomBorderPadding() - LayoutCoord(props.font_descent);
	}
	else
	{
		// Assuming that the text is vertically in the middle for now.

		baseline = LayoutCoord((replaced_height + props.font_ascent + props.font_descent) / 2 - props.font_descent);
		nonpercent_baseline = LayoutCoord((minimum_height + props.font_ascent + props.font_descent) / 2 - props.font_descent);
	}
}

OP_STATUS
FormContent::RecreateFormObject(LayoutProperties* layout_props, FramesDocument* doc)
{
	OP_ASSERT(!form_object);

	// After a history travel, we need to recreate the object and restore the value saved in form_value.

	LayoutCoord width = replaced_width;
	LayoutCoord height = replaced_height;

	BorderBoxToFormBox(layout_props, width, height);

	if (ShowForm(layout_props, doc, width, height) == OpStatus::ERR_NO_MEMORY)
		return OpStatus::ERR_NO_MEMORY;

	if (form_object)
		form_object->SetSize((short) width + packed_used_margins.left + packed_used_margins.right, (short) height + packed_used_margins.top + packed_used_margins.bottom);
	return OpStatus::OK;
}

void
FormContent::GetFormPosition(const HTMLayoutProperties& props, HTML_Element* elm, VisualDevice* visual_device, LayoutCoord& x, LayoutCoord& y)
{
	x = LayoutCoord(0);
	y = LayoutCoord(0);

	if (FormObject::HasSpecifiedBorders(props, elm))
	{
		x += LayoutCoord(props.border.left.width);
		y += LayoutCoord(props.border.top.width);
	}
}

/** Paint box on screen. */

/* virtual */ OP_STATUS
FormContent::Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area)
{
	const HTMLayoutProperties& props = layout_props->GetCascadingProperties();

	if (!form_object)
		RETURN_IF_ERROR(RecreateFormObject(layout_props, doc));

	if (form_object)
	{
		LayoutCoord x;
		LayoutCoord y;

		// Update css border width

		form_object->SetCssBorders(props);

		GetFormPosition(props, layout_props->html_element, visual_device, x, y);

		// in order for skin handling to work as expected we sometimes
		// need to paint in the margins
		int trans_x = x - packed_used_margins.left;
		int trans_y = y - packed_used_margins.top;

		visual_device->Translate(trans_x, trans_y);
		OP_STATUS status = form_object->Display(props, visual_device);
		visual_device->Translate(-trans_x, -trans_y);
		RETURN_IF_ERROR(status);
	}

	return OpStatus::OK;
}

/** Remove form, plugin and iframe objects */

/* virtual */ void
FormContent::MarkDisabled()
{
    form_object_dirty = TRUE;
}

/** Restores a formobject that was stored with StoreFormObject */

BOOL
FormContent::RestoreFormObject(const HTMLayoutProperties& props, FramesDocument* doc)
{
	if (form_object)
		return FALSE;

	HTML_Element* html_element = GetHtmlElement();

	if (html_element && html_element->GetFormObjectBackup())
	{
		FormObject* form_object_backup = html_element->GetFormObjectBackup();

		BOOL compatible_form_object;
		if (html_element->Type() == Markup::HTE_INPUT)
			compatible_form_object = form_object_backup->GetInputType() == html_element->GetInputType();
		else if (html_element->Type() == Markup::HTE_SELECT)
		{
			SelectionObject* sel_obj_backup = static_cast<SelectionObject*>(form_object_backup);
			BOOL should_be_listbox = html_element->GetMultiple() || html_element->GetSize() > 1;
			compatible_form_object = sel_obj_backup->IsListbox() == should_be_listbox;
		}
		else
			compatible_form_object = TRUE;

		if (!compatible_form_object)
		{
			// The type was changed so this FormObject is no longer compatible
			// with the element.
			html_element->DestroyFormObjectBackup();
			return FALSE;
		}

		form_object = form_object_backup;

		html_element->SetFormObjectBackup(NULL);
		form_object->SetLocked(FALSE);

		form_object->InitialiseWidget(props);

		// Get the value from the FormValue
		html_element->GetFormValue()->Externalize(form_object);

		return TRUE;
	}
	return FALSE;
}

/** Stores the formobject in the HTML_Element (for backup during reflow)  */

/* virtual */ void
FormContent::StoreFormObject()
{
#ifdef _DEBUG
	GetHtmlElement()->GetFormValue()->AssertNotExternal();
#endif // _DEBUG
	if (form_object)
	{
		HTML_Element* html_element = GetHtmlElement();
		if (html_element)
		{
			html_element->SetFormObjectBackup(form_object);
			form_object->SetLocked(TRUE);
			form_object = NULL;
		}
	}
}

BOOL FormContent::RemoveCachedInfo()
{
	return TRUE;
}

FormContent::~FormContent()
{
#ifdef _DEBUG
	if (form_object)
	{
		FramesDocument* frames_doc = (FramesDocument*)form_object->GetDocument();
		if (frames_doc && !frames_doc->IsBeingDeleted())
			GetHtmlElement()->GetFormValue()->AssertNotExternal();
	}
#endif // _DEBUG

	// Deleting the form_object may trigger things that try to access
	// and use it. Prevent that by nulling the member variable
	FormObject* to_delete = form_object;
	form_object = NULL;
	OP_DELETE(to_delete);
}

/* virtual */ OP_STATUS
FormContent::Enable(FramesDocument *doc)
{
	RETURN_IF_ERROR(ReplacedContent::Enable(doc));

	HTML_Element* he = GetHtmlElement();

	if (form_object)
	{
		he->GetFormValue()->Externalize(form_object);
	}
	else
	{
		// We destroyed the form_object in Disable, now we have to recreate it
		he->MarkDirty(doc, FALSE);
	}
	return OpStatus::OK;
}

/* virtual */ void
FormContent::Hide(FramesDocument* doc)
{
	if (form_object)
		form_object->ReleaseFocus();
}

/* virtual */ void
FormContent::Disable(FramesDocument* doc)
{
	HTML_Element* he = GetHtmlElement();

	if (form_object)
		form_object->ReleaseFocus();

	if (form_object)
	{
		he->GetFormValue()->Unexternalize(form_object);
	}

	if (doc->IsUndisplaying())
	{
		OP_DELETE(form_object);
		form_object = NULL;
	}

    form_object_dirty = FALSE;
}

/** Remove form, plugin and iframe objects */

/* virtual */ void
InputFieldContent::Disable(FramesDocument* doc)
{
	if (form_object)
	{
		HTML_Element* he = GetHtmlElement();
		switch (he->GetInputType())
		{
		case INPUT_TEXT:
		case INPUT_PASSWORD:
		case INPUT_CHECKBOX:
		case INPUT_RADIO:
		case INPUT_FILE:
		case INPUT_TEL:
		case INPUT_SEARCH:
		case INPUT_URI:
		case INPUT_DATE:
		case INPUT_WEEK:
		case INPUT_TIME:
		case INPUT_EMAIL:
		case INPUT_NUMBER:
		case INPUT_RANGE:
		case INPUT_MONTH:
		case INPUT_DATETIME:
		case INPUT_DATETIME_LOCAL:
		case INPUT_COLOR:
		default:
			FormContent::Disable(doc);
			break;

		case INPUT_SUBMIT:
		case INPUT_RESET:
		case INPUT_BUTTON:
			break;
		}

		if (form_object)
		{
			he->GetFormValue()->Unexternalize(form_object);
		}

		// Deleting the form_object may trigger things that try to access
		// and use it. Prevent that by nulling the member variable
		FormObject* to_delete = form_object;
		form_object = NULL;
		OP_DELETE(to_delete);
	}
}

/** Show form. */

/* virtual */ OP_STATUS
InputFieldContent::ShowForm(LayoutProperties* cascade, FramesDocument* doc, LayoutCoord width, LayoutCoord height)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	BOOL restored = RestoreFormObject(props, doc);

	if (restored || !form_object)
	{
		VisualDevice* visual_device = doc->GetVisualDevice();
		HTML_Element* html_element = cascade->html_element;
		InputType input_type = html_element->GetInputType();

		// Create object. Set label. Set size.

		switch (input_type)
		{
		case INPUT_CHECKBOX:
		case INPUT_RADIO:
			if (!form_object)
			{
				form_object = InputObject::Create(visual_device, props, doc,
												  input_type,
												  html_element,
												  html_element->GetReadOnly());

				if (!form_object)
					return OpStatus::ERR_NO_MEMORY;
			}

			break;

		case INPUT_FILE:
			if (!form_object)
			{
				if (OpStatus::IsError(FileUploadObject::ConstructFileUploadObject(visual_device, props, doc,
																				  html_element->GetReadOnly(),
																				  UNI_L(""),
																				  html_element,
																				  FALSE,
																				  form_object)))
					return OpStatus::ERR_NO_MEMORY;

				OP_ASSERT(form_object);
			}

			break;

		default:
			{
				// Here we set the text of submit and reset buttons
				// if they don't have a value of their own.
				OpString button_text;
				RETURN_IF_ERROR(html_element->GetFormValue()->GetValueAsText(html_element, button_text));

				if (!button_text.CStr() && (input_type == INPUT_SUBMIT || input_type == INPUT_RESET))
				{
					FormValueNoOwnValue* form_value_no_own_value = FormValueNoOwnValue::GetAs(html_element->GetFormValue());
					RETURN_IF_ERROR(form_value_no_own_value->GetEffectiveValueAsText(html_element, button_text));
				}

				if (!form_object)
				{
					form_object = InputObject::Create(visual_device, props, doc,
													  input_type,
													  html_element,
													  html_element->GetMaxLength(),
													  html_element->GetReadOnly(),
													  button_text.CStr());

					if (!form_object)
						return OpStatus::ERR_NO_MEMORY;
				}

				break;
			}
		}

		// Get value from the FormValue object and make FormValue use
		// the widget as the value storage
		html_element->GetFormValue()->Externalize(form_object);

		if (!restored)
		{
			form_object->SetParentInputContext(visual_device);

			form_object->GetWidget()->SetParentInputContext(form_object);
		}

#ifdef _WML_SUPPORT_
		if (doc->GetHLDocProfile()->IsWml())
		{
			WML_Context* wc = doc->GetHLDocProfile()->WMLGetContext();

			OP_ASSERT( wc );

			if (wc)
			{
				RETURN_IF_MEMORY_ERROR(wc->UpdateWmlInput(form_object->GetHTML_Element(), form_object));
			}
		}
#endif //_WML_SUPPORT_
	}

	return OpStatus::OK;
}

/** Show form. */

static void
AddOptionsAndGroups(SelectionObject* select, HTML_Element* he)
{
	if (he->Type() == Markup::HTE_OPTION)
		select->AddElement(UNI_L(""), FALSE, FALSE);
	else
		if (he->Type() == Markup::HTE_OPTGROUP)
		{
			select->BeginGroup(he);

			HTML_Element* child = he->FirstChild();

			while (child)
			{
				AddOptionsAndGroups(select, child);
				child = child->Suc();
			}

			select->EndGroup(he);
		}
}

/** Show form. */

/* virtual */ OP_STATUS
SelectContent::ShowForm(LayoutProperties* cascade, FramesDocument* doc, LayoutCoord width, LayoutCoord height)
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	if (RestoreFormObject(props, doc))
	{
		SelectionObject* select = (SelectionObject*) form_object;

		if (select && select->GetElementCount() > 0)
			select_packed.lock_elements = TRUE;
	}

	if (!form_object)
	{
		VisualDevice* visual_device = doc->GetVisualDevice();
		HTML_Element* html_element = cascade->html_element;
		int html_size = html_element->GetSize();
		BOOL html_multiple = html_element->GetMultiple();

		if (html_size <= 1 && html_multiple)
			html_size = 4;
		else
			if (html_size < 1)
				html_size = 1;

		RETURN_IF_MEMORY_ERROR(SelectionObject::ConstructSelectionObject(
										visual_device, props, doc,
										  html_multiple,
										  html_size,
										  replaced_width,
										  replaced_height,
										  props.min_width,
										  props.max_width,
										  html_element,
										  FALSE,
										  form_object));
		OP_ASSERT(form_object);

		SelectionObject* select = (SelectionObject*) form_object;

		select->SetParentInputContext(visual_device);
		select->GetWidget()->SetParentInputContext(form_object);

		// The old form implementation requires this!
		select->StartAddElement();

#ifdef _SSL_SUPPORT_
# ifndef _EXTERNAL_SSL_SUPPORT_
		if (html_element->Type() != Markup::HTE_KEYGEN)
# endif // _EXTERNAL_SSL_SUPPORT_
#endif // _SSL_SUPPORT
			// Insert elements. (Only happens when using history)
			if (options_count)
			{
				HTML_Element* he = html_element->FirstChild();

				while (he)
				{
					AddOptionsAndGroups(select, he);
					he = he->Suc();
				}

				OP_ASSERT(select->GetElementCount() == static_cast<int>(options_count));
				for (unsigned int i = 0; i < options_count; i++)
					RETURN_IF_ERROR(SetOptionContent(options[i], i));
			}

		if (html_element->Type() == Markup::HTE_KEYGEN || options_count)
		{
			// FIXME: Why do we let the forms code calculate the size during traversal?

			BOOL width_is_auto = placeholder->GetReflowState() && cascade->IsLayoutWidthAuto() || props.content_width == CONTENT_WIDTH_AUTO;

			select->EndAddElement(width_is_auto, props.content_height == CONTENT_HEIGHT_AUTO);
		}

		// Get value from the FormValue object
		html_element->GetFormValue()->Externalize(form_object);
	}

	return OpStatus::OK;
}

/** Remove all options */

void
SelectContent::RemoveOptions()
{
	SelectionObject* select = static_cast<SelectionObject*>(form_object);
	select->RemoveAll();
	options_count = 0;
	select_packed.lock_elements = FALSE;
}

/** Add option element to 'select' content */

/* virtual */ OP_STATUS
SelectContent::AddOption(HTML_Element* option_element, unsigned int &index)
{
	SelectionObject* select = (SelectionObject*) form_object;
	HTML_Element* html_element = GetHtmlElement();
	OP_ASSERT(option_element->IsIncludedInSelect(html_element));

	// Could fit in UINT_MAX, but SelectionObject is 'int' based, so
	// stop at INT_MAX instead.
	if (options_size >= INT_MAX)
		return OpStatus::ERR;

	// If the option already exists in the optionlist, we don't need to add it again. Not optimal.

	for (unsigned int i = 0; i < options_count; i++)
		if (option_element == options[i])
		{
			index = i;
			return OpStatus::OK;
		}

	if (options_count == options_size)
	{
		HTML_Element** new_options = OP_NEWA(HTML_Element*, options_size + 10);

		if (!new_options)
			return OpStatus::ERR_NO_MEMORY;

		if (options)
		{
			for (unsigned int i = 0; i < options_count; i++)
				new_options[i] = options[i];

			OP_DELETEA(options);
		}

		options = new_options;

		REPORT_MEMMAN_INC(10 * sizeof(HTML_Element*)); // Only if really created, missing OOM.

		options_size += 10;
	}

	index = UINT_MAX;

	/* If this isn't the first option: need to find what index it is at. */
	if (options_count > 0)
	{

		/* Look for the previous option that is included in the select. Not all option elements are included (see specs). */

		HTML_Element* prev_option_element = option_element->Prev();

		while (prev_option_element)
		{
			if (prev_option_element == html_element)
				prev_option_element = NULL;
			else
			{

				/* Checking GetLayoutBox()->IsOptionBox() is tempting but this might be called from
				   FormValueList at a time when there is no layout box yet so we need to check the
				   element structure. */

				if (prev_option_element->IsIncludedInSelect(html_element))
					break;

				prev_option_element = prev_option_element->Prev();
			}
		}

		if (prev_option_element)
		{
			for (unsigned int i = options_count; i > 0; i--)
				if (prev_option_element == options[i - 1])
				{
					index = i;
					break;
				}
		}
		else
			index = 0;

		if (index == UINT_MAX)
			index = options_count;

		/* Move all options behind the new option one step backwards. */
		for (unsigned int i = options_count; i > index; --i)
		{
			options[i] = options[i - 1];

			Box *layout_box = options[i]->GetLayoutBox();

			if (layout_box)
				((OptionBox *) layout_box)->SetIndex(i);
		}
	}

	if (index == UINT_MAX)
		index = options_count;

	options[index] = option_element;

	FormValueList* form_value = FormValueList::GetAs(html_element->GetFormValue());
	BOOL selected = form_value->IsSelectedElement(html_element, option_element);

	// Add the option empty first, and set the string with ChangeElement later.
	if (select && !select_packed.lock_elements)
	{
		// Note: overflow is not an issue when casting 'index' due to check at the head of.
		RETURN_IF_MEMORY_ERROR(select->AddElement(UNI_L(""), selected, option_element->GetDisabled(), index == options_count ? -1 : static_cast<int>(index)));
   }

	// Note: as AddOption() will fail while getting close to INT_MAX, this cannot overflow.
	++options_count;

	if (select && select_packed.lock_elements && static_cast<int>(options_count) == select->GetElementCount())
		select_packed.lock_elements = FALSE;

	return OpStatus::OK;
}

/** Remove option element from 'select' content */

/* virtual */ void
SelectContent::RemoveOption(HTML_Element* option_element)
{
	SelectionObject* select = (SelectionObject*) form_object;

	for (unsigned int i = 0; i < options_count; ++i)
		if (options[i] == option_element)
		{
			if (select)
				select->RemoveElement(i);

			for (; i < options_count - 1; ++i)
			{
				options[i] = options[i + 1];

				Box *layout_box = options[i]->GetLayoutBox();

				if (layout_box)
					((OptionBox *) layout_box)->SetIndex(i);
			}

			--options_count;
			return;
		}
}

/** Remove optiongroup element from 'select' content */

void
SelectContent::RemoveOptionGroup(HTML_Element* optiongroup_element)
{
	SelectionObject* select = (SelectionObject*) form_object;

	OP_ASSERT(optiongroup_element->IsMatchingType(Markup::HTE_OPTGROUP, NS_HTML));

	if (select)
	{
		int index = 0;

		HTML_Element* child = GetHtmlElement()->FirstChild();

		while (child)
		{
			if (child->IsMatchingType(Markup::HTE_OPTGROUP, NS_HTML))
			{
				if (child == optiongroup_element)
				{
					select->RemoveGroup(index);
					break;
				}
				index++;
			}
			child = child->Suc();
		}
	}

	HTML_Element* option_child = optiongroup_element->FirstChild();

	while (option_child)
	{
		if (option_child->IsMatchingType(Markup::HTE_OPTION, NS_HTML))
			RemoveOption(option_child);
		else if (option_child->IsMatchingType(Markup::HTE_OPTGROUP, NS_HTML))
			RemoveOptionGroup(option_child);

		option_child = option_child->Suc();
	}

}

/** Set option content in selection object */

OP_STATUS
SelectContent::SetOptionContent(HTML_Element* option, unsigned int index)
{
	OP_ASSERT(form_object);
	OP_ASSERT(!form_object || !static_cast<SelectionObject *>(form_object)->HasAsyncRebuildScheduled() || !"Must not update an unsynchronized widget.");

	OP_ASSERT(index < options_count);

	const uni_char* option_text = option->GetStringAttr(Markup::HA_LABEL);
	TempBuffer buffer;
	if (!option_text || !*option_text)
	{
		RETURN_IF_ERROR(options[index]->GetOptionText(&buffer));
		option_text = buffer.GetStorage();
	}
	OP_ASSERT(option_text);

	BOOL selected = FormValueList::IsOptionSelected(option);

	BOOL disabled = options[index]->GetDisabled();

	if (!disabled)
		// Check if it has a optgroup and if it is disabled
		if (options[index]->Parent() && options[index]->Parent()->Type() == Markup::HTE_OPTGROUP)
			if (options[index]->Parent()->GetDisabled())
				disabled = TRUE;

	if (select_packed.lock_elements)
		selected = FALSE; // Don't touch selection if locked

	if (form_object)
		RETURN_IF_ERROR(((SelectionObject* )form_object)->ChangeElement(option_text, selected, disabled, index));

	return OpStatus::OK;
}

/** Begin optiongroup in 'select' content */

void SelectContent::BeginOptionGroup(HTML_Element* optiongroup_element)
{
	SelectionObject* select = (SelectionObject*) form_object;

	if (select && !select_packed.lock_elements)
		select->BeginGroup(optiongroup_element);
}

/** End optiongroup in 'select' content */

void SelectContent::EndOptionGroup(HTML_Element* optiongroup_element)
{
	SelectionObject* select = (SelectionObject*) form_object;

	if (select && !select_packed.lock_elements)
		select->EndGroup(optiongroup_element);
}

/** Layout option element in 'select' content */

/* virtual */ OP_STATUS
SelectContent::LayoutOption(LayoutProperties* cascade, LayoutWorkplace* workplace, unsigned int index)
{
	SelectionObject* select = (SelectionObject*) form_object;
	OP_STATUS stat = OpStatus::OK;
	OP_ASSERT(index != UINT_MAX);

	if (select && index < options_count && !select->HasAsyncRebuildScheduled())
	{
		const HTMLayoutProperties& props = *cascade->GetProps();

		select->StartAddElement();
		stat = SetOptionContent(options[index], index);
		select->EndAddElement(cascade->IsLayoutWidthAuto(), props.content_height == CONTENT_HEIGHT_AUTO);
	}

	return stat;
}

/* virtual */ LAYST
SelectContent::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	LAYST st = FormContent::Layout(cascade, info, first_child, start_position);

	if (st != LAYOUT_CONTINUE)
		return st;

	if (first_child && first_child != cascade->html_element)
		return placeholder->LayoutChildren(cascade, info, first_child, start_position);
	else
		return placeholder->LayoutChildren(cascade, info, NULL, LayoutCoord(0));
}

/** Finish reflowing box. */

/* virtual */ LAYST
SelectContent::FinishLayout(LayoutInfo& info)
{
	UpdateScreen(info);

	if (info.hld_profile->GetIsOutOfMemory())
		return LAYOUT_OUT_OF_MEMORY;

	return ReplacedContent::FinishLayout(info);
}

/** Update content on screen. */

/* virtual */ void
SelectContent::UpdateScreen(LayoutInfo& info)
{
	LayoutProperties* cascade = placeholder->GetCascade();

	/* Compute size over again. If child content was added, box size may have
	   changed since last time it was computed (right before laying out this
	   box). To compute size consistently, set VisualDevice properties over
	   again to match this element, since child content may have modified
	   them. Ideally, there should be a better way to detect that content was
	   added, than comparing size computation before and afterwards. */

	if (packed.minmax_calculated && placeholder->NeedMinMaxWidthCalculation(cascade))
		/* If min/max widths are needed, make sure that they are recalculated
		   now, since child content may have been added, removed or
		   modified. */

		packed.minmax_calculated = 0;

	cascade->GetProps()->SetDisplayProperties(info.visual_device);
	OP_BOOLEAN size_changed = ComputeSize(cascade, info);

	if (size_changed == OpBoolean::IS_TRUE)
	{
		if (!select_packed.pending_invalidate)
			if (placeholder->IsFloatingBox() ||
#ifdef SUPPORT_TEXT_DIRECTION
				(placeholder->IsBlockBox() && cascade->GetProps()->direction == CSS_VALUE_rtl) ||
#endif // SUPPORT_TEXT_DIRECTION
				(placeholder->IsAbsolutePositionedBox() && ((AbsolutePositionedBox*) placeholder)->IsRightAligned()))
			{
				/* FIXME: It should be possible to correct the box position on the fly;
				   no need for another reflow pass. */

				select_packed.pending_invalidate = TRUE;
				info.workplace->SetReflowElement(cascade->html_element);
			}
	}
	else
		if (OpStatus::IsMemoryError(size_changed))
			info.hld_profile->SetIsOutOfMemory(TRUE);
}

/** Signal that content may have changed. */

/* virtual */ void
SelectContent::SignalChange(FramesDocument* doc)
{
	if (select_packed.pending_invalidate)
	{
		// Intrinsic size changed during layout. This means that min/max widths must be recalculated.

		placeholder->MarkDirty(doc, TRUE);
		select_packed.pending_invalidate = FALSE;
	}
}

/* virtual */ OP_STATUS
SelectContent::Paint(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device, const RECT& area)
{
	if (!form_object)
		RETURN_IF_ERROR(RecreateFormObject(layout_props, doc));

	if (!form_object)
		return OpStatus::OK;

	LayoutCoord x;
	LayoutCoord y;
	GetFormPosition(layout_props->GetCascadingProperties(), layout_props->html_element, visual_device, x, y);

	visual_device->Translate(x, y);
	form_object->SetWidgetPosition(visual_device);
	visual_device->Translate(-x, -y);

	if (!layout_props->html_element->IsMatchingType(Markup::HTE_KEYGEN, NS_HTML))
	{
		UINT32 start, stop;
		SelectionObject* sel_obj = (SelectionObject*) form_object;

		OpRect doc_rect(&area);
		doc_rect.x += visual_device->GetRenderingViewX();
		doc_rect.y += visual_device->GetRenderingViewY();

		/* Update only as many as necessary */
		sel_obj->GetRelevantOptionRange(doc_rect, start, stop);
		OP_ASSERT(start <= stop);
		OP_ASSERT(stop <= options_count);

		HLDocProfile* hld_profile = doc->GetLogicalDocument()->GetHLDocProfile();
		for (UINT32 i = start; i < stop; ++i)
		{
			LayoutProperties* child_props = layout_props->GetChildProperties(hld_profile, options[i]);

			if (!child_props)
				return OpStatus::ERR_NO_MEMORY;

			sel_obj->ApplyProps(i, child_props);
		}
	}

	/* Restore VisualDevice state to that of layout_props */
	layout_props->GetProps()->SetDisplayProperties(visual_device);

	return FormContent::Paint(layout_props, doc, visual_device, area);
}

/** Show form. */

/* virtual */ OP_STATUS
TextareaContent::ShowForm(LayoutProperties* cascade, FramesDocument* doc, LayoutCoord width, LayoutCoord height)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	RestoreFormObject(props, doc);
	HTML_Element* html_element = cascade->html_element;

	if (!form_object)
	{
		VisualDevice* visual_device = doc->GetVisualDevice();

		int html_cols = html_element->GetCols();
		int html_rows = html_element->GetRows();
		if (OpStatus::IsError(TextAreaObject::ConstructTextAreaObject(visual_device, props, doc,
										 html_cols,
										 html_rows,
										 html_element->GetReadOnly(),
										 html_element->Type() == Markup::HTE_INPUT ? html_element->GetValue() : UNI_L(""),
										 width,
										 height,
										 html_element,
										 FALSE,
										 form_object)))
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		OP_ASSERT(form_object);

		form_object->SetParentInputContext(visual_device);
		form_object->GetWidget()->SetParentInputContext(form_object);
	}

	// Get value from the FormValue object
	// The value we get here might be empty since we do this during
	// page load when the value isn't loaded yet. We do a ResetToDefault a little later
	// to get the new value.
	html_element->GetFormValue()->Externalize(form_object);

	return OpStatus::OK;
}

/** Is value changed by user since last set. */

BOOL
TextareaContent::IsModified()
{
	TextAreaObject* textarea = (TextAreaObject*) form_object;

	if (textarea)
		return textarea->IsUserModified();

	return FALSE;
}

/** Lay out form content. */

/* virtual */ OP_STATUS
TextareaContent::LayoutFormContent(LayoutProperties* cascade, LayoutWorkplace* workplace)
{
#ifdef _WML_SUPPORT_
	if (workplace->GetFramesDocument()->GetHLDocProfile()->IsWml())
	{
		WML_Context* wc = workplace->GetFramesDocument()->GetHLDocProfile()->WMLGetContext();

		OP_ASSERT(wc);

		if (wc)
			return wc->UpdateWmlInput(form_object->GetHTML_Element(), form_object);
	}
#endif //_WML_SUPPORT_
	return OpStatus::OK;
}

/* virtual */ LAYST
TextareaContent::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (first_child && first_child != cascade->html_element)
		return placeholder->LayoutChildren(cascade, info, first_child, start_position);
	else
		return placeholder->LayoutChildren(cascade, info, NULL, LayoutCoord(0));
}

/** Is there any in-flow content? */

/* virtual */ BOOL
Container::IsEmpty() const
{
	for (VerticalLayout* vertical_layout = (VerticalLayout*) layout_stack.First(); vertical_layout; vertical_layout = (VerticalLayout*) vertical_layout->Suc())
		if (vertical_layout->IsInStack(TRUE))
			if (!vertical_layout->IsLine() || !static_cast<Line*>(vertical_layout)->IsOnlyOutsideListMarkerLine())
				return FALSE;

	return TRUE;
}

/** Get line that includes given virtual postion. */

Line*
Container::GetLineAtVirtualPosition(long pos)
{
	for (VerticalLayout* vertical_layout = (VerticalLayout*) layout_stack.First(); vertical_layout; vertical_layout = vertical_layout->Suc())
		if (vertical_layout->IsLine())
		{
			Line* line = (Line*) vertical_layout;

			if (line->GetStartPosition() <= pos && (!line->Suc() || line->GetStartPosition() + line->GetUsedSpace() + line->GetTrailingWhitespace() > pos))
				return line;
		}

	return NULL;
}

/** Traverse box with children. */

/* virtual */ void
Container::Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	OP_PROBE6(OP_PROBE_CONTAINER_TRAVERSE);

	LayoutPoint translation;

	if (!traversal_object->GetTarget() || traversal_object->EnterContainer(layout_props, this, translation))
	{
		HTML_Element* html_element = layout_props->html_element;
		Column* pane = traversal_object->GetCurrentPane();
		VerticalLayout* vertical_layout = (VerticalLayout*) layout_stack.First();

		BOOL check_first_line = TRUE;

		for (;
			 vertical_layout && !traversal_object->IsOffTargetPath() && !traversal_object->IsPaneDone();
			 vertical_layout = vertical_layout->Suc())
		{
			if (pane)
				// Traversing a column or page.

				if (!traversal_object->IsPaneStarted())
				{
					/* We're looking for a column's or page's start element. Until
					   we find it, we shouldn't traverse anything, except for the
					   start element's ancestors. */

					const ColumnBoundaryElement& start_element = pane->GetStartElement();

					if (vertical_layout == start_element.GetVerticalLayout())
						/* We found the start element. Traverse everything as
						   normally until we find the stop element. */

						traversal_object->SetPaneStarted(TRUE);
					else
						/* Skip the element unless it's an ancestor of the start
						   element. */

						if (vertical_layout->IsBlock())
						{
							HTML_Element* child_html_element = ((BlockBox*) vertical_layout)->GetHtmlElement();

							if (!child_html_element->IsAncestorOf(start_element.GetHtmlElement()))
								// Not an ancestor of the start element.

								if (!traversal_object->TraverseInLogicalOrder())
								{
									HTML_Element* target = traversal_object->GetTarget();

									if (!target || !IsInFloatedPaneBoxTree(target, pane))
										// Also not a pane float (or a descendant of one) in this column or page.

										continue;
								}
						}
						else
							continue;
				}
				else
				{
					LayoutCoord start_offset = traversal_object->GetPaneStartOffset();

					if (start_offset > 0 && start_offset > vertical_layout->GetStackPosition())
						continue;
					else
						if (pane->ExcludeStopElement() && vertical_layout == pane->GetStopElement().GetVerticalLayout())
						{
							traversal_object->SetPaneDone(TRUE);
							break;
						}
						else
							traversal_object->SetPaneStartOffset(LayoutCoord(0));
				}

			BOOL is_first_line = FALSE;

			if (check_first_line)
				if (vertical_layout->IsFirstLine())
				{
					is_first_line = TRUE;
					check_first_line = FALSE;

					if (layout_props->WantToModifyProperties(TRUE))
					{
						if (OpStatus::IsMemoryError(layout_props->AddFirstLineProperties(traversal_object->GetDocument()->GetHLDocProfile())))
						{
							traversal_object->SetOutOfMemory();
							return;
						}

						layout_props->use_first_line_props = TRUE;
					}
					else
					{
						traversal_object->SetOutOfMemory();
						return;
					}
				}
				else
					check_first_line = !vertical_layout->IsInStack();

			traversal_object->SetInlineFormattingContext(FALSE);

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE
			if (!packed.only_traverse_blocks || vertical_layout->IsBlock())
#endif
				vertical_layout->Traverse(traversal_object, layout_props, html_element);

			if (is_first_line)
				layout_props->RemoveFirstLineProperties();

			if (pane)
			{
				const ColumnBoundaryElement& stop_element = pane->GetStopElement();

				/* If we get here, we have obviously traversed something, so mark
				   column or page traversal as started. We may have missed the
				   opportunity to do so above, if the traverse area doesn't
				   intersect with the start element. */

				traversal_object->SetPaneStarted(TRUE);
				traversal_object->SetPaneStartOffset(LayoutCoord(0));

				BOOL found_stop = FALSE;

				if (stop_element.GetVerticalLayout() == vertical_layout)
					/* We found the stop element, which means that we're done with a
					   column or page. */

					found_stop = TRUE;
				else
					if (vertical_layout->IsBlock())
					{
						HTML_Element* child_element = ((BlockBox*) vertical_layout)->GetHtmlElement();

						if (child_element->IsAncestorOf(stop_element.GetHtmlElement()))
							// We passed the stop element without visiting it.

							found_stop = TRUE;
					}

				if (found_stop)
				{
					HTML_Element* target = traversal_object->GetTarget();

					if (!target || traversal_object->TraverseInLogicalOrder() || !IsInFloatedPaneBoxTree(target, pane))
						traversal_object->SetPaneDone(TRUE);
				}
			}
		}

		traversal_object->SetInlineFormattingContext(FALSE);
		if (traversal_object->GetTarget())
			traversal_object->LeaveContainer(translation);
	}
}

/** Set page and column breaking policies on this line break. */

void
LayoutBreak::SetBreakPolicies(BREAK_POLICY page_break_before, BREAK_POLICY column_break_before,
							  BREAK_POLICY page_break_after, BREAK_POLICY column_break_after)
{
	packed.page_break_before = page_break_before;
	packed.column_break_before = column_break_before;
	packed.page_break_after = page_break_after;
	packed.column_break_after = column_break_after;

	/* The column break bitfields cannot hold 'left' and 'right' values (and
	   they are meaningless in that context anyway): */

	OP_ASSERT(column_break_before != BREAK_LEFT && column_break_before != BREAK_RIGHT);
	OP_ASSERT(column_break_after != BREAK_LEFT && column_break_after != BREAK_RIGHT);
}

/** Traverse layout break. */

/* virtual */ void
LayoutBreak::Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element)
{
	if (traversal_object->GetTraverseType() != TRAVERSE_BACKGROUND && traversal_object->EnterLayoutBreak(html_element))
	{
		VerticalLayout* pred = Pred();
		LayoutProperties* layout_props = parent_lprops->GetChildProperties(traversal_object->GetDocument()->GetHLDocProfile(),
																		   html_element);

		if (!layout_props)
		{
			traversal_object->SetOutOfMemory();
			return;
		}

		const HTMLayoutProperties& props = *layout_props->GetProps();
		LayoutCoord x_offset(0);

		if (pred && pred->IsLine())
		{
			Line* line = (Line*) pred;

			if (!line->HasForcedBreak())
				x_offset += line->GetUsedSpace();
		}

		if (props.text_align == CSS_VALUE_right)
			x_offset = width;
		else
			if (props.text_align == CSS_VALUE_center)
				x_offset = (x_offset + width) / LayoutCoord(2);

		traversal_object->Translate(x + x_offset, y);

		traversal_object->HandleLineBreak(layout_props, TRUE);
		traversal_object->HandleSelectableBox(layout_props);

		traversal_object->Translate(- (x + x_offset), -y);
	}
}

/** Get the next line in the layout stack. */

Line*
Line::GetNextLine(BOOL& anonymous_box_ended) const
{
	if (HasForcedBreak())
		anonymous_box_ended = TRUE;
	for (VerticalLayout* suc = Suc(); suc; suc = suc->Suc())
		if (suc->IsLine())
			return (Line*) suc;
		else
			if (!suc->IsPageBreak())
				anonymous_box_ended = TRUE;

	anonymous_box_ended = TRUE;

	return NULL;
}

/** Return TRUE if the specified virtual range may be part of this line. */

BOOL
Line::MayIntersectLogically(LayoutCoord start, LayoutCoord end) const
{
	OP_ASSERT(end >= start);

	/* Check if the range start is not past the line's end position and the
	   range end doesn't precede the line's start position. These checks
	   are inaccurate when line has content before left or after right
	   egde */

	if (HasContentBeforeStart() || end >= GetStartPosition())
		if (HasContentAfterEnd() || start < GetEndPosition())
			return TRUE;
		else
			if (used_space == 0 || start == end)
				/* If the line takes up no space or if the range is empty, include the
				   line's end position. The "line's end position" is really the start
				   position of the next line and isn't part of this line, but if the line
				   uses no space, we need to include it in order to hit anything at
				   all. Also, if the range is empty ('start' and 'end' are equal), we
				   need to include this, because that may be a point just outside (after
				   (LTR: to the right of)) the box we're looking for. It may happen
				   during text selection, when the box found to be the closest one to the
				   selection point is last on line, and the selection point is actually
				   past the end of the line (LTR: to the right of the line). */

				return start == GetEndPosition();

	return FALSE;
}

#ifdef SUPPORT_TEXT_DIRECTION

/** Helper function for reversing array segments. */

template<class T> void
reverse_array(T* array, int start, int end) {
	int half_way = (int) ((end - start) / 2);
	for (int ix = 0; ix < half_way; ix++) {
		T tmp = array[start + ix];
		array[start + ix] = array[end - ix - 1];
		array[end - ix - 1] = tmp;
	}
}

OP_STATUS
Line::ResolveBidiSegments(const Head& paragraph_bidi_segments, int& bidi_segment_count, BidiSegment*& bidi_segments)
{
	OP_NEW_DBG("Line::ResolveBidiSegment", "bidi");

	// 1. find out the first segment of this line

	ParagraphBidiSegment* start_segment = static_cast<ParagraphBidiSegment*>(paragraph_bidi_segments.First());

	OP_ASSERT(start_segment);

	int start_position = GetStartPosition();
	BOOL need_to_search = start_position > 0; // We are certainly not in a first line.

	if (!need_to_search)
		/* Despite the fact that start_position is not greater than zero, we still
		   might not be in the first line. That can happen e.g. in a container with
		   an outside list item marker when the line that contains the marker
		   doesn't contain anything else - it won't use any space and the following
		   line will have zero start position. Also any negative margin can cause
		   any line to have negative start position. */

		for (VerticalLayout* pred = Pred(); pred != NULL; pred = pred->Pred())
			if (pred->IsLine())
			{
				need_to_search = TRUE;
				break;
			}

	if (need_to_search)
	{
		// Find the first segment that intersects with the start_position.

		int acc_width = start_segment->width;

		while (start_position >= acc_width && start_segment->Suc())
		{
			start_segment = start_segment->Suc();
			acc_width += start_segment->width;
		}
	}

	ParagraphBidiSegment* end_segment = start_segment;


	// 2. find the last segment of this line

	int end_position = GetStartPosition() + GetUsedSpace();

	bidi_segment_count++; // we have at least a start segment...

	while (end_segment->Suc() && end_position >= ((ParagraphBidiSegment*)end_segment->Suc())->virtual_position)
	{
		end_segment = (ParagraphBidiSegment*)end_segment->Suc();
		bidi_segment_count++;
	}


	// 3. Create the new segments

	bidi_segments = OP_NEWA(BidiSegment, bidi_segment_count);

	if (!bidi_segments)
		return OpStatus::ERR_NO_MEMORY;

	int* bidi_levels = OP_NEWA(int, bidi_segment_count); // unnecessary ? could maybe use the old paragraph levels instead...

	if (!bidi_levels)
	{
		OP_DELETEA(bidi_segments);
		return OpStatus::ERR_NO_MEMORY;
	}

	int* segment_positions = OP_NEWA(int, bidi_segment_count);

	if (!segment_positions)
	{
		OP_DELETEA(bidi_segments);
		OP_DELETEA(bidi_levels);
		return OpStatus::ERR_NO_MEMORY;
	}

	int max_level = 0, min_odd_level = 61;

	// 4. Create the first segment. Could be only a part of a ParagraphBidiSegment

	if (bidi_segment_count == 1)
		bidi_segments[0].width = GetUsedSpace();
	else
		bidi_segments[0].width = start_segment->width - (GetStartPosition() - start_segment->virtual_position);

	bidi_segments[0].start_element = GetStartElement();
	bidi_segments[0].left_to_right = start_segment->level % 2 == 0;
	bidi_levels[0] = start_segment->level;
	segment_positions[0] = 0;

	max_level = MAX(bidi_levels[0], max_level);

	if (bidi_levels[0] % 2 == 1)
		min_odd_level = MIN(bidi_levels[0], min_odd_level);

	// end first segment

	ParagraphBidiSegment* middle_segment = (ParagraphBidiSegment*)start_segment->Suc();


	// 5. Create the middle segments

	int segment_counter = 1;
	if (start_segment != end_segment && middle_segment != end_segment) // we have more than two segments, thus at least one middle segment
	{

		while (middle_segment != end_segment)
		{
			bidi_segments[segment_counter].width = middle_segment->width;
			bidi_segments[segment_counter].start_element = middle_segment->start_element;
			bidi_segments[segment_counter].left_to_right = middle_segment->level % 2 == 0;

			bidi_levels[segment_counter] = middle_segment->level;

			segment_positions[segment_counter] = segment_counter;

			max_level =MAX(bidi_levels[segment_counter], max_level);

			if (bidi_levels[segment_counter] % 2 == 1)
				min_odd_level = MIN(bidi_levels[segment_counter], min_odd_level);

			segment_positions[segment_counter] = segment_counter;

		segment_counter++;
		middle_segment = (ParagraphBidiSegment*)middle_segment->Suc();
		}
	}

	// end middle segments


	// 6. create the final segment if there is one. Could be only a part of a segment

	if (end_segment != start_segment)
	{
		bidi_segments[segment_counter].width = GetStartPosition() + GetUsedSpace() - end_segment->virtual_position;

		bidi_segments[segment_counter].start_element = end_segment->start_element;
		bidi_segments[segment_counter].left_to_right = end_segment->level % 2 == 0;
		bidi_levels[segment_counter] = end_segment->level;

		max_level = MAX(bidi_levels[segment_counter], max_level);

		if (bidi_levels[segment_counter] % 2 == 1)
			min_odd_level = MIN(bidi_levels[segment_counter], min_odd_level);

		segment_positions[segment_counter] = segment_counter;
	}


	// 7. shuffle

	int ix = 0;

	for (int level = max_level; level >= min_odd_level; level--)
	{
		int run_start = -1;
		int pseudo_ix;

		for (pseudo_ix = 0; pseudo_ix < bidi_segment_count; pseudo_ix++)
		{
			ix = segment_positions[pseudo_ix];

			if (bidi_levels[ix] >= level && run_start == -1)
				// we're at the start of the run
				run_start = pseudo_ix;

			if (bidi_levels[ix] < level && run_start != -1)
			{
				// a run just ended, reverse it
				reverse_array(segment_positions, run_start, pseudo_ix);
				run_start = -1;
			}
		}

		if (run_start != -1 && bidi_levels[ix] >= level)
		{
			// reverse last run
			reverse_array(segment_positions, run_start, pseudo_ix);
		}
	}


	// 8. compute new offsets
	int offset = 0;

	for (int pseudo_ix = 0; pseudo_ix < bidi_segment_count; pseudo_ix++)
	{
		ix = segment_positions[pseudo_ix];
		bidi_segments[ix].offset = offset;
		offset += bidi_segments[ix].width;
	}
#ifdef _DEBUG

	OP_DBG(("#####"));
	for (int i = 0;i < bidi_segment_count;i++)
		OP_DBG(("i: %d width: %d, left_to_right: %d element: %x offset: %d",i, bidi_segments[i].width,
			   bidi_segments[i].left_to_right, bidi_segments[i].start_element, bidi_segments[i].offset));
#endif

	OP_DELETEA(bidi_levels);
	OP_DELETEA(segment_positions);

	// bidi_segments ownership returned to Line::Traverse

	return OpStatus::OK;
}

OP_STATUS
Line::CalculateBidiJustifyPositions(BidiSegment* bidi_segments, int bidi_segment_count, LineSegment* segment, Line* next_line, HTML_Element* containing_element)
{
	OP_NEW_DBG("Line::CalculateBidiJustifyPositions", "bidi");
	LineSegment iter_segment;

	iter_segment.container_props = segment->container_props;
	iter_segment.line = this;
	iter_segment.white_space = segment->white_space;

	iter_segment.justify = TRUE; // ignored
	iter_segment.justified_space_extra_accumulated = LayoutCoord(0); // ignored

	iter_segment.x = segment->x;

	iter_segment.word_number = 1;

	OP_ASSERT(bidi_segment_count > 1);

	LayoutCoord pos = position;

	for (int i = 0; i < bidi_segment_count; i++)
	{
		iter_segment.left_to_right = bidi_segments[i].left_to_right;
		iter_segment.length = LayoutCoord(bidi_segments[i].width);
		iter_segment.start = pos;

		iter_segment.start_element = bidi_segments[i].start_element;

		if (bidi_segment_count > i + 1)
		{
			iter_segment.stop_element = bidi_segments[i + 1].start_element;
			iter_segment.stop = pos + LayoutCoord(bidi_segments[i].width);
		}
		else
		{
			iter_segment.stop_element = next_line ? next_line->GetStartElement() : NULL;
			iter_segment.stop = GetEndPosition();
		}

		pos += iter_segment.length;

		OP_ASSERT(iter_segment.start_element->GetLayoutBox());
		int words = Content::CountWords(iter_segment, containing_element);

		/* temporary storage to save memory. At this point "words_to_the_left" means
		   the number of words in this segment */
		bidi_segments[i].words_to_the_left = words;
	}

	//	Sort the segments in visual order

	BidiSegment** tmp_bidi_segments = OP_NEWA(BidiSegment*, bidi_segment_count);

	if (!tmp_bidi_segments)
		return OpStatus::ERR_NO_MEMORY;

	BOOL* sorted = OP_NEWA(BOOL, bidi_segment_count);

	if (!sorted)
	{
		OP_DELETEA(tmp_bidi_segments);
		return OpStatus::ERR_NO_MEMORY;
	}

	for (int c = 0; c < bidi_segment_count; c++)
		sorted[c] = FALSE;

	for (int p = 0; p< bidi_segment_count;p++)
	{
		int lowest = 0;
		int min_offset = INT_MAX;

		for (int j = 0; j < bidi_segment_count; j++)
		{
			if (bidi_segments[j].offset <= min_offset && !sorted[j])
			{
				lowest = j;
				min_offset = bidi_segments[j].offset;
			}
		}
		sorted[lowest]= TRUE;
		tmp_bidi_segments[p] = &bidi_segments[lowest];
	}

	OP_DELETEA(sorted);

#ifdef _DEBUG
	OP_DBG(("unsorted: %%%%%%"));
	int q;
	for (q = 0;q < bidi_segment_count;q++)
		OP_DBG(("i: %d width: %d, left_to_right: %d element: %x offset: %d, sc: %d",q, bidi_segments[q].width,
			   bidi_segments[q].left_to_right, bidi_segments[q].start_element, bidi_segments[q].offset, bidi_segments[q].words_to_the_left));

	OP_DBG(("sorted: %%%%%%"));
	for (int q2 = 0;q2 < bidi_segment_count;q2++)
		OP_DBG(("i: %d width: %d, left_to_right: %d element: %x offset: %d, sc: %d",q2, tmp_bidi_segments[q2]->width,
			   tmp_bidi_segments[q2]->left_to_right, tmp_bidi_segments[q2]->start_element, tmp_bidi_segments[q2]->offset, tmp_bidi_segments[q2]->words_to_the_left));
#endif

	int wn = 0;

	for (int r = 0; r < bidi_segment_count; r++)
	{
		int space_count = tmp_bidi_segments[r]->words_to_the_left;

		tmp_bidi_segments[r]->words_to_the_left = wn;

		if (!tmp_bidi_segments[r]->left_to_right)
			tmp_bidi_segments[r]->words_to_the_left += space_count;

		wn += space_count;
	}

#ifdef _DEBUG
	OP_DBG(("unsorted: %%%%%%"));
	for (q = 0;q < bidi_segment_count;q++)
		OP_DBG(("i: %d width: %d, left_to_right: %d element: %x offset: %d, wtl: %d",q, bidi_segments[q].width,
			   bidi_segments[q].left_to_right, bidi_segments[q].start_element, bidi_segments[q].offset,  bidi_segments[q].words_to_the_left));
#endif

	OP_DELETEA(tmp_bidi_segments);

	return OpStatus::OK;
}
#endif

/** Traverse line. */

/* virtual */ void
Line::Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element)
{
	if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
		return;

	HTMLayoutProperties& parent_props = parent_lprops->GetCascadingProperties();

	traversal_object->Translate(x, y);

	TraverseInfo traverse_info;

	if (traversal_object->EnterLine(parent_lprops, this, containing_element, traverse_info))
	{
		HTML_Element* element = NULL;
		LineSegment segment;

		traversal_object->SetInlineFormattingContext(TRUE);

		segment.container_props = parent_lprops;
		segment.x = x;

		// Search for element to start with

		for (HTML_Element* run = start_element; run != containing_element; run = run->Parent())
		{
			element = run;

			OP_ASSERT(!run || run->GetLayoutBox());

			if (!run || (run->GetLayoutBox() && run->GetLayoutBox()->IsInlineRunInBox()))
				break;
		}

		OP_ASSERT(element && element->GetLayoutBox());

		if (element && element->GetLayoutBox())
		{
			Box* box = element->GetLayoutBox();
			BOOL anonymous_box_ended = FALSE;
			Line* next_line = GetNextLine(anonymous_box_ended);

			BOOL is_run_in = box->IsInlineRunInBox();
			BOOL has_non_text_align_box = HasBoxNotAffectedByTextAlign();

			OP_ASSERT(!box->IsInlineCompactBox() || is_run_in); // a compact is also a run-in

			segment.start = position;
			segment.start_element = start_element;

			segment.stop_element = next_line ? next_line->GetStartElement() : NULL;
			segment.stop = GetEndPosition();

			segment.length = GetUsedSpace();
			segment.line = this;
			segment.white_space = CSSValue(parent_props.white_space);
			segment.justify = FALSE;
			segment.justified_space_extra_accumulated = LayoutCoord(0);
			segment.word_number = 1;

#ifdef SUPPORT_TEXT_DIRECTION
			segment.left_to_right = TRUE;
			segment.offset = LayoutCoord(0);

			const Head* paragraph_bidi_segments = containing_element->GetLayoutBox()->GetContainer()->GetBidiSegments();
#endif

			short old_font_underline_position = 0;
			short old_font_underline_width = 0;
			HTMLayoutProperties* parent_orig_props = NULL;

			if (parent_props.text_decoration & TEXT_DECORATION_UNDERLINE)
			{
				parent_orig_props = parent_lprops->GetProps();
				old_font_underline_position = parent_orig_props->font_underline_position;
				old_font_underline_width = parent_orig_props->font_underline_width;

				for (HTML_Element* html_element = element; html_element; html_element = html_element->Suc())
					if (html_element->GetLayoutBox())
						if (!html_element->GetLayoutBox()->GetUnderline(this, parent_orig_props->font_underline_position, parent_orig_props->font_underline_width))
							break;
			}

			BOOL justify = parent_props.text_align == CSS_VALUE_justify &&
				!anonymous_box_ended &&
				packed.number_of_words > 1 &&
				segment.white_space != CSS_VALUE_pre &&
				segment.white_space != CSS_VALUE_pre_wrap;

			LayoutCoord x_offset(0);

			if (!has_non_text_align_box)
			{
				x_offset = GetTextAlignOffset(parent_props
#ifdef SUPPORT_TEXT_DIRECTION
											 , justify
#endif // SUPPORT_TEXT_DIRECTION
											 );
				segment.justify = justify;
			}
#ifdef SUPPORT_TEXT_DIRECTION
			else
			{
				// We can't do text alignment now, just offset according to direction.
				x_offset = parent_props.direction == CSS_VALUE_rtl ? GetWidth() - GetUsedSpace() : LayoutCoord(0);
			}
#endif // SUPPORT_TEXT_DIRECTION

			if (x_offset)
			{
				segment.x += x_offset;
				traversal_object->Translate(x_offset, LayoutCoord(0));
			}

#ifdef SUPPORT_TEXT_DIRECTION
			if (paragraph_bidi_segments && !paragraph_bidi_segments->Empty())
			{
				BOOL looking_for_target = traversal_object->GetTarget() && !traversal_object->IsTargetDone();
				int bidi_segment_count = 0;
				BidiSegment* bidi_segments = NULL;

				if (ResolveBidiSegments(*paragraph_bidi_segments, bidi_segment_count, bidi_segments) == OpStatus::ERR_NO_MEMORY)
				{
					traversal_object->SetOutOfMemory();
					return;
				}

				BOOL first_done = FALSE;
				BOOL continue_traverse = TRUE;
				LayoutCoord pos = position;

				LayoutCoord length = LayoutCoord(bidi_segments[0].width);
				int segment_index = 0;

				segment.left_to_right = bidi_segments[0].left_to_right;
				segment.length = length;
				segment.offset = LayoutCoord(bidi_segments[0].offset);

				if (bidi_segment_count > 1)
				{
					segment.stop_element = bidi_segments[1].start_element;
					segment.stop = segment.start + length;
				}

				if (justify)
				{
					if (bidi_segment_count > 1)
					{
						if (OpStatus::IsMemoryError(CalculateBidiJustifyPositions(bidi_segments, bidi_segment_count, &segment, next_line, containing_element)))
						{
							traversal_object->SetOutOfMemory();
							return;
						}

						segment.word_number = bidi_segments[0].words_to_the_left + 1;
					}
					else
						if (!segment.left_to_right)
							segment.word_number = GetNumberOfWords();
						else
							segment.word_number = 1;

					OP_ASSERT(packed.number_of_words > 1);
					segment.justified_space_extra_accumulated = LayoutCoord(( (GetWidth() - GetUsedSpace()) * (segment.word_number - 1) ) / (packed.number_of_words - 1));
				}

				while (element && continue_traverse)
				{
					box = element->GetLayoutBox();

					if (box)
					{
						if (first_done && box->IsInlineRunInBox())
							// Stop if next inline box is a run-in (it belongs to next block)

							break;

						traversal_object->Translate(LayoutCoord(bidi_segments[segment_index].offset), LayoutCoord(0));
						continue_traverse = box->LineTraverseBox(traversal_object, parent_lprops, segment, GetBaseline());
						traversal_object->Translate(LayoutCoord(-(int)bidi_segments[segment_index].offset), LayoutCoord(0));
					}

					segment.start_element = NULL;

					if (continue_traverse)
					{
						first_done = TRUE;

						// If first was a non text align box then align now.
						if (has_non_text_align_box)
						{
							// Untranslate the direction offset
							if (x_offset)
							{
								segment.x -= x_offset;
								traversal_object->Translate(-x_offset, LayoutCoord(0));
							}

							x_offset = GetTextAlignOffset(parent_props
#ifdef SUPPORT_TEXT_DIRECTION
														 , justify
#endif // SUPPORT_TEXT_DIRECTION
									   );

							if (justify)
							{
								/* Update the justify related data now, as we may not have done it
								   during the last segment switch, because we still haven't left the
								   non text align box (list item marker or inline compact box).

								   NOTE: Content::CountWords() seems to be sometimes working bad in case
								   of intitial segment and the first element on the line if its box
								   starts before the line's 0 virtual position. Either because of
								   negative left margin or deliberate negative x of the element's box.
								   That happens with non text align boxes. However, if we update
								   justify data here, the word_number should be correct at that moment
								   (we are in some following segment). */

								segment.justify = justify;
								OP_ASSERT(packed.number_of_words > 1);
								segment.word_number = bidi_segments[segment_index].words_to_the_left + 1;
								segment.justified_space_extra_accumulated = LayoutCoord(( (GetWidth() - GetUsedSpace()) * (segment.word_number - 1) ) / (packed.number_of_words - 1));
							}

							segment.x += x_offset;
							traversal_object->Translate(x_offset, LayoutCoord(0));
							has_non_text_align_box = FALSE;
						}

						if (is_run_in)
						{
							element = containing_element->FirstChild();
							is_run_in = FALSE;
						}
						else
							element = element->Suc();
					}
					else if (!looking_for_target || !traversal_object->IsTargetDone())
					{
						/* We have reached the end of the segment and we were not looking
						   for target or we haven't finished the target yet. We should
						   proceed to the next segment if there is such, unless we have just
						   traversed a block box. If so, it means that there is nothing more
						   on this line (see CORE-40786). */
						if (++segment_index < bidi_segment_count && !box->IsBlockBox())
						{
							pos += length;
							length = LayoutCoord(bidi_segments[segment_index].width);
							continue_traverse = TRUE;

							segment.left_to_right = bidi_segments[segment_index].left_to_right;

							segment.start = pos;
							segment.start_element = bidi_segments[segment_index].start_element;

							segment.length = length;
							segment.offset = LayoutCoord(bidi_segments[segment_index].offset);

							if (bidi_segment_count > segment_index + 1)
							{
								segment.stop_element = bidi_segments[segment_index + 1].start_element;
								segment.stop = pos + length;
							}
							else
							{
								segment.stop_element = next_line ? next_line->GetStartElement() : NULL;
								segment.stop = GetEndPosition();
							}

							if (segment.justify)
							{
								OP_ASSERT(packed.number_of_words > 1);
								segment.word_number = bidi_segments[segment_index].words_to_the_left + 1;
								segment.justified_space_extra_accumulated = LayoutCoord(( (GetWidth() - GetUsedSpace()) * (segment.word_number - 1) ) / (packed.number_of_words - 1));
							}
						}
					}
				}

				OP_DELETEA(bidi_segments);
			}
			else
#endif /* SUPPORT_TEXT_DIRECTION */
				if (box->LineTraverseBox(traversal_object, parent_lprops, segment, GetBaseline()))
				{
					segment.start_element = NULL;

					// If first was a non text align box then align now.
					if (has_non_text_align_box)
					{
						// Untranslate the direction offset.
						if (x_offset)
						{
							segment.x -= x_offset;
							traversal_object->Translate(-x_offset, LayoutCoord(0));
						}

						x_offset = GetTextAlignOffset(parent_props
#ifdef SUPPORT_TEXT_DIRECTION
													 , justify
#endif // SUPPORT_TEXT_DIRECTION
								   );

						segment.justify = justify;
						segment.x += x_offset;
						traversal_object->Translate(x_offset, LayoutCoord(0));
					}

					element = is_run_in ? containing_element->FirstChild() : element->Suc();

					for (; element; element = element->Suc())
					{
						box = element->GetLayoutBox();

						if (box)
						{
							if (box->IsInlineRunInBox())
								// Stop if next inline box is a run-in (it belongs to next block)

								break;

							if (!box->LineTraverseBox(traversal_object, parent_lprops, segment, GetBaseline()))
								break;
						}
					}
				}

			if (parent_orig_props)
			{
				parent_orig_props->font_underline_position = old_font_underline_position;
				parent_orig_props->font_underline_width = old_font_underline_width;
			}
		}

		traversal_object->LeaveLine(parent_lprops, this, containing_element, traverse_info);
		traversal_object->Translate(-segment.x, -y);
	}
	else
	{
		traversal_object->Translate(-x, -y);
		traversal_object->SetTextElement(NULL);
	}
}

/** Get page break policy before this layout element. */

/* virtual */ BREAK_POLICY
Line::GetPageBreakPolicyBefore() const
{
	if (!packed2.can_break_before)
		return BREAK_AVOID;

	if (Suc() && Suc()->IsLayoutBreak() && ((LayoutBreak*) Suc())->IsLineTerminator())
		if (Suc()->GetPageBreakPolicyBefore() == BREAK_AVOID)
			return BREAK_AVOID;

	return BREAK_ALLOW;
}

/** Get page break policy after this layout element. */

/* virtual */ BREAK_POLICY
Line::GetPageBreakPolicyAfter() const
{
	if (Suc() && Suc()->IsLayoutBreak() && ((LayoutBreak*) Suc())->IsLineTerminator())
		if (Suc()->GetPageBreakPolicyAfter() == BREAK_AVOID)
			return BREAK_AVOID;

	return BREAK_ALLOW;
}

/** Get column break policy before this layout element. */

/* virtual */ BREAK_POLICY
Line::GetColumnBreakPolicyBefore() const
{
	if (!packed2.can_break_before)
		return BREAK_AVOID;

	if (Suc() && Suc()->IsLayoutBreak() && ((LayoutBreak*) Suc())->IsLineTerminator())
		if (Suc()->GetColumnBreakPolicyBefore() == BREAK_AVOID)
			return BREAK_AVOID;

	return BREAK_ALLOW;
}

/** Get column break policy after this layout element. */

/* virtual */ BREAK_POLICY
Line::GetColumnBreakPolicyAfter() const
{
	if (Suc() && Suc()->IsLayoutBreak() && ((LayoutBreak*) Suc())->IsLineTerminator())
		if (Suc()->GetColumnBreakPolicyAfter() == BREAK_AVOID)
			return BREAK_AVOID;

	return BREAK_ALLOW;
}

/** Figure out to which column(s) or spanned element a box belongs. */

/* virtual */ void
Line::FindColumn(ColumnFinder& cf, Container* container)
{
	cf.SetPosition(y);

	/* Find inline boxes. We may also find some absolutely positioned boxes
	   here. Sometimes absolutely positioned boxes do not get their own layout
	   stack entry (but are instead part of some line). */

	HTML_Element* containing_element = container->GetHtmlElement();
	HTML_Element* target = cf.GetTarget()->GetHtmlElement();

	if (!containing_element->IsAncestorOf(target))
		return;

	VerticalLayout* next = Suc();

	while (next && !next->IsLine() && !next->IsBlock())
		next = next->Suc();

	if (next)
	{
		HTML_Element* stop_element;

		if (next->IsLine())
			stop_element = ((Line*) next)->GetStartElement();
		else
			stop_element = ((BlockBox*) next)->GetHtmlElement();

		for (HTML_Element* elm = start_element; elm; elm = elm->Next())
		{
			if (Box* box = elm->GetLayoutBox())
				if (box == cf.GetTarget() &&
					(elm != stop_element || (box->IsInlineBox() || box->IsTextBox()) && box->GetVirtualPosition() < position + used_space))
				{
					VerticalLayout* vertical_layout = this;
					LayoutCoord box_bottom = GetStackPosition() + cf.GetTarget()->GetHeight();
					LayoutCoord line_bottom;

					cf.SetBoxStartFound();

					do
					{
						// The element may span several lines.

						line_bottom = vertical_layout->GetStackPosition() + vertical_layout->GetLayoutHeight();
						cf.SetPosition(line_bottom);

						stop_element = NULL;

						for (vertical_layout = vertical_layout->Suc(); vertical_layout; vertical_layout = vertical_layout->Suc())
							if (vertical_layout->IsLine())
							{
								stop_element = ((Line*) vertical_layout)->GetStartElement();
								break;
							}
							else
								if (vertical_layout->IsBlock())
									break;
					}
					while (stop_element && elm->IsAncestorOf(stop_element));

					if (box_bottom > line_bottom)
						cf.SetPosition(box_bottom);

					cf.SetBoxEndFound();
					break;
				}

			if (elm == stop_element)
				break;
		}
	}
	else
	{
		/* There's nothing left to examine after this and the block is in this
		   container, which means that it has to be on this line. */

		LayoutCoord stack_position = GetStackPosition();

		cf.EnterChild(stack_position);
		cf.SetBoxStartFound();
		cf.SetPosition(cf.GetTarget()->GetHeight());
		cf.SetBoxEndFound();
		cf.LeaveChild(stack_position);
	}
}

/* virtual */ BOOL
Content::LineTraverse(TraversalObject* traversal_object, LayoutProperties* props, LineSegment& segment, LayoutCoord baseline, LayoutCoord x)
{
	OP_ASSERT(!GetTableContent());
	OP_ASSERT(!GetContainer());
	return placeholder->LineTraverseChildren(traversal_object, props, segment, baseline);
}

#ifdef PAGED_MEDIA_SUPPORT

/** Insert a page break. */

BREAKST
Content::InsertPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain)
{
	BREAKST status = AttemptPageBreak(info, strength, constrain);

	if (status == BREAK_KEEP_LOOKING)
		// Didn't find page break in container

		if (constrain != SOFT)
			// We have to insert a page break

			status = placeholder->InsertPageBreak(info, strength);

	return status;
}

#endif // PAGED_MEDIA_SUPPORT

/** Get the logical top and bottom of a loose subtree of inline boxes limited by end_position.

	A subtree is defined as 'loose' if the root has vertical-alignment 'top' or 'bottom'
	and consist of all descendants that are not loose themselves. */

BOOL
Content::GetLooseSubtreeTopAndBottom(TraversalObject* traversal_object, LayoutProperties* cascade, LayoutCoord current_baseline, LayoutCoord end_position, HTML_Element* start_element, LayoutCoord& logical_top, LayoutCoord& logical_bottom)
{
	HTML_Element* element = NULL;
	BOOL success = TRUE;
	HLDocProfile* hld_profile = traversal_object->GetDocument()->GetHLDocProfile();

	if (start_element)
		for (HTML_Element* run = start_element; run != cascade->html_element; run = run->Parent())
		{
			element = run;

			if (!run)
				break;
		}

	if (!element)
	{
		element = cascade->html_element->FirstChild();
		start_element = NULL;
	}

	for (; element && success; element = element->Suc())
	{
		Box* box = element->GetLayoutBox();

		if (box && !box->IsTextBox())
		{
			LayoutProperties* tmp_cascade = cascade->GetChildProperties(hld_profile, element);

			if (!tmp_cascade)
				success = FALSE;
			else
				success = box->GetLooseSubtreeTopAndBottom(traversal_object, tmp_cascade, current_baseline, end_position, start_element, logical_top, logical_bottom);
		}

		start_element = NULL;
	}

	return success;
}

/** Figure out to which column(s) or spanned element a box belongs. */

/* virtual */ void
Content::FindColumn(ColumnFinder& cf)
{
	OP_ASSERT(!GetHtmlElement()->IsAncestorOf(cf.GetTarget()->GetHtmlElement()));
}

OP_STATUS
BulletContent::PaintBullet(LayoutProperties* layout_props, FramesDocument* doc, VisualDevice* visual_device)
{
	Image img;
	int img_width;
	int img_height;
	HTML_Element* list_item_element = layout_props->html_element->Parent();
	HEListElm* helm = list_item_element->GetHEListElm(FALSE);
	OP_STATUS status = OpStatus::OK;

	const HTMLayoutProperties& props = *(layout_props->GetProps());

#ifdef SVG_SUPPORT
	SVGImage* svg_image = NULL;
	BOOL is_svg = FALSE;
	img = layout_props->GetListMarkerImage(doc, svg_image);

	img_width = static_cast<int>(img.Width());
	img_height = static_cast<int>(img.Height());

	if (svg_image)
	{
		is_svg = TRUE;

		/* Paint the SVG to buffer if needed (first paint or size changed). */

		int current_width = visual_device->ScaleToScreen(static_cast<INT32>(width));
		int current_height = visual_device->ScaleToScreen(static_cast<INT32>(height));

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		const PixelScaler& scaler = visual_device->GetVPScale();
		current_width = TO_DEVICE_PIXEL(scaler, current_width);
		current_height = TO_DEVICE_PIXEL(scaler, current_height);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		if (img.IsFailed() || img_width != current_width || img_height != current_height)
		{
			OpBitmap* res;

			if (OpStatus::IsSuccess(svg_image->PaintToBuffer(res, 0, current_width, current_height)))
			{
				Image img_svg = imgManager->GetImage(res);

				OP_ASSERT(helm); // Must be not NULL in this block "if (svg_image)".
				UrlImageContentProvider* content_provider = helm->GetUrlContentProvider();
				OP_ASSERT(content_provider); // Must be not NULL in this block "if (svg_image)".

				content_provider->ResetImage(img_svg, TRUE);

				img = img_svg;
				img_width = res->Width();
				img_height = res->Height();
			}
		}
	}
#else // SVG_SUPPORT
	img = layout_props->GetListMarkerImage(doc);
	img_width = static_cast<int>(img.Width());
	img_height = static_cast<int>(img.Height());
#endif // SVG_SUPPORT

	if (img_width > 0 && img_height > 0)
	{
		long local_img_width = static_cast<long>(width);
		long local_img_height = static_cast<long>(height);

		// Record the painted position of an image.
		if (helm)
		{
			AffinePos doc_pos = visual_device->GetCTM();
			doc->SetImagePosition(list_item_element, doc_pos, local_img_width, local_img_height, FALSE, helm);
		}

		// Paint the image (or buffered svg image).

		OpRect destRect(0, 0, local_img_width, local_img_height);

#ifdef SVG_SUPPORT
		VDStateNoScale state_noscale;

		// For svg we do the non-scaled painting from the previously cached SVG painting.
		if (is_svg)
		{
			state_noscale = visual_device->BeginNoScalePainting(destRect);
			destRect = state_noscale.dst_rect;
		}
#endif // SVG_SUPPORT

		status = visual_device->ImageOut(img,
										 OpRect(0, 0, img_width, img_height),
										 destRect,
										 helm);

#ifdef SVG_SUPPORT
		if (is_svg)
			visual_device->EndNoScalePainting(state_noscale);
#endif // SVG_SUPPORT
	}
#ifdef CSS_GRADIENT_SUPPORT
	else if (props.list_style_image_gradient)
		visual_device->GradientImageOut(OpRect(0, 0, width, height), props.font_color, *props.list_style_image_gradient);
#endif // CSS_GRADIENT_SUPPORT
	else // simple bullet
	{
		switch (props.list_style_type)
		{
		case CSS_VALUE_disc:
			visual_device->BulletOut(0, 0, width, width);
			break;

		case CSS_VALUE_circle:
			visual_device->EllipseOut(0, 0, width, width);
			break;

		case CSS_VALUE_square:
			visual_device->FillRect(OpRect(0, 0, width, width));
			break;

		case CSS_VALUE_box:
			visual_device->RectangleOut(0, 0, width, width);
			break;

		default:
			OP_ASSERT(!"BulletContent shouldn't be here!");
		}
	}

	return status;
}

/* virtual */ BOOL
BulletContent::LineTraverse(TraversalObject* traversal_object, LayoutProperties* layout_props, LineSegment& segment, LayoutCoord baseline, LayoutCoord x)
{
	if (layout_props->GetProps()->visibility == CSS_VALUE_visible || traversal_object->GetEnterHidden())
	{
		LayoutCoord y = baseline - GetBaseline();

		traversal_object->Translate(x, y);
		traversal_object->HandleBulletContent(layout_props, this);
		traversal_object->Translate(-x, -y);
	}

	return TRUE;
}

/* virtual */ BOOL
BulletContent::GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const
{
	min_width = width;
	normal_min_width = width;
	max_width = width;

	return TRUE;
}

void
ReplacedContent::SetVisibility(FramesDocument *doc, BOOL visible)
{
	if (visible)
		Enable(doc);
	else
	{
		HTML_Element* html_element = placeholder->GetHtmlElement();

		if (html_element->HasBgImage())
			html_element->UndisplayImage(doc, TRUE);

		Hide(doc);
	}

	packed.content_visible = !!visible;
}

/** Traverse box with children. */

/* virtual */ void
ReplacedContent::Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	if (layout_props->GetCascadingProperties().visibility == CSS_VALUE_visible)
	{
		if (!packed.content_visible)
			SetVisibility(traversal_object->GetDocument(), TRUE);

		traversal_object->HandleReplacedContent(layout_props, this);
		traversal_object->HandleSelectableBox(layout_props);
	}
	else if (layout_props->GetCascadingProperties().visibility != CSS_VALUE_visible && packed.content_visible)
		SetVisibility(traversal_object->GetDocument(), FALSE);
}

/* virtual */ BOOL
ReplacedContent::LineTraverse(TraversalObject* traversal_object, LayoutProperties* layout_props, LineSegment& segment, LayoutCoord baseline, LayoutCoord x)
{
	if (!traversal_object->GetTarget())
	{
		const HTMLayoutProperties& props = *layout_props->GetProps();

		if (props.visibility == CSS_VALUE_visible && !packed.content_visible)
			SetVisibility(traversal_object->GetDocument(), TRUE);
		else if (props.visibility != CSS_VALUE_visible && packed.content_visible)
			SetVisibility(traversal_object->GetDocument(), FALSE);

		if (props.visibility == CSS_VALUE_visible || traversal_object->GetEnterHidden())
		{
			LayoutCoord y = baseline - GetBaseline();
			if (!IsReplacedWithBaseline())
				y -= props.margin_bottom;

			traversal_object->Translate(x, y);

			traversal_object->HandleReplacedContent(layout_props, this);//FIXME:OOM Check if traversal_object is OOM
			traversal_object->HandleSelectableBox(layout_props);
			traversal_object->Translate(-x, -y);
		}
	}

	return TRUE;
}

OpRect
ReplacedContent::GetInnerRect(const HTMLayoutProperties& props) const
{
	INT32 left = props.padding_left + props.border.left.width;
	INT32 top = props.padding_top + props.border.top.width;
	INT32 inner_width = GetWidth() - left - (props.padding_right + props.border.right.width);
	INT32 inner_height = GetHeight() - top - (props.padding_bottom + props.border.bottom.width);
	return OpRect(left, top, inner_width, inner_height);
}

/** Update size of content.

	@return OpBoolean::IS_TRUE if size changed, OpBoolean::IS_FALSE if not, OpStatus::ERR_NO_MEMORY on OOM. */

/* virtual */ OP_BOOLEAN
ReplacedContent::SetNewSize(LayoutProperties* cascade, LayoutInfo& info, LayoutCoord width, LayoutCoord height,
							LayoutCoord modified_horizontal_border_padding)
{
	OP_ASSERT(height >= 0);

	if (replaced_height != height || replaced_width != width)
	{
		replaced_height = height;
		replaced_width = width;

		return OpBoolean::IS_TRUE;
	}
	else
		return OpBoolean::IS_FALSE;
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
ReplacedContent::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	HTML_Element* html_element = cascade->html_element;
	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord width;
	LayoutCoord height;
	LayoutCoord intrinsic_width(0);
	LayoutCoord intrinsic_height(0);
	int intrinsic_ratio = 0;
	LayoutCoord horizontal_border_padding = LayoutCoord(props.border.left.width + props.border.right.width) + props.padding_left + props.padding_right;
	LayoutCoord vertical_border_padding = LayoutCoord(props.border.top.width + props.border.bottom.width) + props.padding_top + props.padding_bottom;
	LayoutCoord modified_horizontal_border_padding = horizontal_border_padding;
	BOOL width_is_auto = cascade->IsLayoutWidthAuto();
	BOOL height_is_auto = FALSE;
	BOOL calculate_min_max_widths = placeholder->NeedMinMaxWidthCalculation(cascade) && !packed.minmax_calculated;
	LayoutCoord css_height = CalculateCSSHeight(info, cascade);

	if (props.content_height < 0)
	{
		if (props.content_height == CONTENT_HEIGHT_AUTO && placeholder->IsAbsolutePositionedBox())
			/* CSS21 10.6.5. Absolute positioned replaced content use inline replaced algorithm to determine
			   auto height */

			css_height = CONTENT_HEIGHT_AUTO;

		/* If height is percent but resolved to auto, we need to treat it as auto. */

		if (css_height == CONTENT_HEIGHT_AUTO)
			height_is_auto = TRUE;
	}

	packed.use_alternative_text = 0;

	if (width_is_auto || props.content_width < 0 || props.GetMinWidthIsPercent() ||
		height_is_auto || props.content_height < 0 || props.GetMinHeightIsPercent() ||
		cascade->flexbox)
		/* Need the intrinsic size because either dimension is auto or a
		   percentage. For percentages, calculating intrinsic size is really
		   only required if there's an ancestor table or shrink-to-fit
		   container, but checking that here is heavy (just checking if we're
		   calculating min/max in this pass won't do, since we must prevent
		   packed.size_determined from being set until we actually know the
		   intrinsic size). */

		RETURN_IF_ERROR(CalculateIntrinsicSize(cascade, info, intrinsic_width, intrinsic_height, intrinsic_ratio));
	else
		// Fixed width and height. The size is known then.

		packed.size_determined = TRUE;

	width = MIN(LAYOUT_COORD_HALF_MAX, intrinsic_width);
	height = MIN(LAYOUT_COORD_HALF_MAX, intrinsic_height);

	RETURN_IF_ERROR(CalculateSize(cascade, info, calculate_min_max_widths, width, height, css_height, intrinsic_ratio, modified_horizontal_border_padding, vertical_border_padding, height_is_auto));

	if (!width_is_auto && !height_is_auto && width > 10 && height > 10)
		/* Intrinsic size doesn't affect us, meaning that this box will not be
		   reflowed when the image is loaded. If there is an ALT attribute, set a
		   flag to make sure that the ALT text is when the box is painted. The flag
		   will be reset when the image is loaded (during painting)). */

		packed.use_alternative_text = html_element->GetIMG_Alt() != NULL;

	// Convert to border-box:

	width += modified_horizontal_border_padding;
	height += vertical_border_padding;

	if (cascade->flexbox)
	{
		// Calculate and propagate minimum and hypothetical height. Apply stretch.

		LayoutCoord auto_height = intrinsic_height + vertical_border_padding;
		LayoutCoord hypothetical_height = height;
		FlexItemBox* item_box = (FlexItemBox*) placeholder;
		OP_ASSERT(placeholder->IsFlexItemBox());

		if (cascade->flexbox->IsVertical())
		{
			/* Calculate the hypothetical height (i.e. main size) directly. It must not
			   be contaminated by later steps in the flex algorithm. */

			hypothetical_height = item_box->GetPreferredMainBorderSize();

			if (hypothetical_height < 0)
				// Percent or auto. Just use intrinsic border box height then.

				hypothetical_height = auto_height;

			if (props.content_width >= CONTENT_SIZE_MIN || item_box->AllowStretch())
				/* Set new width, so that it gets stretched (if that's supposed
				   to happen). Note that this may break the intrinsic ratio,
				   but since we don't want circular dependencies, this is how
				   it has to be. */

				width = item_box->GetFlexWidth(cascade);
		}
		else
		{
			LayoutCoord stretched_height = item_box->GetFlexHeight(cascade);

			if (stretched_height != CONTENT_HEIGHT_AUTO)
				/* Note that this may break the intrinsic ratio, but since we don't want
				   circular dependencies, this is how it has to be. */

				height = stretched_height;
		}

		item_box->PropagateHeights(auto_height, hypothetical_height);
	}

	return SetNewSize(cascade, info, width, height, modified_horizontal_border_padding);
}

/** Get min/max width for box. */

/* virtual */ BOOL
ReplacedContent::GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const
{
	min_width = minimum_width;
	normal_min_width = normal_minimum_width;
	max_width = maximum_width;

	return TRUE;
}

/** Request reflow later if necessary. */

void
ReplacedContent::RequestReflow(LayoutWorkplace* workplace)
{
	packed.reflow_requested = 1;
	workplace->SetReflowElement(GetHtmlElement());
}

/** Traverse box with children. */

/* virtual */ void
TableContent::Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	OP_PROBE6(OP_PROBE_TABLECONTENT_TRAVERSE);

	const HTMLayoutProperties& props = *layout_props->GetProps();
	LayoutCoord row_spacing(0);
	LayoutCoord padding_top(0);
	LayoutCoord padding_bottom(0);
	LayoutCoord table_top(0);
	LayoutCoord table_bottom = height;
	TableListElement* element;
	short dummy;
	short top_border_width;
	short bottom_border_width;
	TraverseInfo traverse_info;

	if (!GetCollapseBorders())
	{
		row_spacing = LayoutCoord(props.border_spacing_vertical);
		padding_top = props.padding_top;
		padding_bottom = props.padding_bottom;
	}

#ifdef SUPPORT_TEXT_DIRECTION
	packed.rtl = layout_props->GetProps()->direction == CSS_VALUE_rtl;
#endif
	GetBorderWidths(props, top_border_width, dummy, dummy, bottom_border_width, FALSE);

	// Calculate top and bottom of the actual table box, i.e. excluding any top and bottom captions.

	for (element = (TableListElement*) layout_stack.First(); element; element = element->Suc())
		if (element->IsRowGroup())
		{
			table_top = element->GetStaticPositionedY() - row_spacing - padding_top - LayoutCoord(top_border_width);
			break;
		}
		else
			if (element->IsTopCaption())
				table_top = element->GetStaticPositionedY() + element->GetHeight();

	for (element = (TableListElement*) layout_stack.Last(); element; element = element->Pred())
		if (element->IsRowGroup())
		{
			TableRowGroupBox* row_group_box = (TableRowGroupBox*) element;

			if (!row_group_box->IsVisibilityCollapsed())
			{
				LayoutCoord rowgroup_height(0);

				for (TableRowBox* row_box = row_group_box->GetLastRow(); row_box; row_box = (TableRowBox*) row_box->Pred())
					if (!row_box->IsVisibilityCollapsed())
					{
						rowgroup_height = row_box->GetStaticPositionedY() + row_box->GetHeight() + row_spacing;
						break;
					}

				table_bottom = element->GetStaticPositionedY() + rowgroup_height + padding_bottom + LayoutCoord(bottom_border_width);
				break;
			}
		}

	if (!traversal_object->EnterTableContent(layout_props, this, table_top, table_bottom - table_top, traverse_info))
		return;

	// Traverse columns and column groups.

	TableColGroupBox* cur_col_box = NULL;
	TableColGroupBox* cur_col_group_box = NULL;

	for (unsigned int i = 0; i < column_count; i++)
		if (column_widths[i].column_box && cur_col_box != column_widths[i].column_box)
		{
			cur_col_box = column_widths[i].column_box;

			if (!cur_col_box->IsGroup())
			{
				Box* parent_box = cur_col_box->GetHtmlElement()->Parent()->GetLayoutBox();

				if (parent_box->IsTableColGroup())
				{
					TableColGroupBox* group_box = (TableColGroupBox*) parent_box;

					if (cur_col_group_box != group_box)
					{
						cur_col_group_box = group_box;
						traversal_object->HandleTableColGroup(cur_col_group_box, layout_props, this, table_top, table_bottom);
					}
				}
			}

			traversal_object->HandleTableColGroup(cur_col_box, layout_props, this, table_top, table_bottom);
		}

	// Traverse captions and row groups.

	Column* pane = traversal_object->GetCurrentPane();

	for (element = (TableListElement*) layout_stack.First();
		 element && !traversal_object->IsOffTargetPath() && !traversal_object->IsPaneDone();
		 element = element->Suc())
	{
		if (pane)
			if (!traversal_object->IsPaneStarted())
			{
				/* We're looking for a column's or page's start element. Until
				   we find it, we shouldn't traverse anything, except for the
				   start element's ancestors. */

				const ColumnBoundaryElement& start_element = pane->GetStartElement();

				if (element == start_element.GetTableListElement())
					/* We found the start element. Traverse everything as
					   normally until we find the stop element. */

					traversal_object->SetPaneStarted(TRUE);
				else
					/* Skip the element unless it's an ancestor of the start
					   element. */

					if (!element->GetHtmlElement()->IsAncestorOf(start_element.GetHtmlElement()))
						// Not an ancestor of the start element. Move along.

						continue;
			}
			else
				if (pane->ExcludeStopElement() && element == pane->GetStopElement().GetTableListElement())
				{
					traversal_object->SetPaneDone(TRUE);
					break;
				}

		element->Traverse(traversal_object, layout_props, this);

		if (pane)
		{
			const ColumnBoundaryElement& stop_element = pane->GetStopElement();

			/* If we get here, we have obviously traversed something, so mark
			   column or page traversal as started. We may have missed the
			   opportunity to do so above, if the traverse area doesn't
			   intersect with the start element. */

			traversal_object->SetPaneStarted(TRUE);

			if (stop_element.GetTableListElement() == element)
				/* We found the stop element, which means that we're done with
				   a column or page. */

				traversal_object->SetPaneDone(TRUE);
			else
				if (element->GetHtmlElement()->IsAncestorOf(stop_element.GetHtmlElement()))
					// We passed the stop element without visiting it.

					traversal_object->SetPaneDone(TRUE);
		}
	}

	traversal_object->LeaveTableContent(this, layout_props, traverse_info);
}

/** Get min/max width for box. */

/* virtual */ BOOL
TableContent::GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const
{
	min_width = minimum_width;
	normal_min_width = normal_minimum_width;
	max_width = constrained_maximum_width != -1 ? constrained_maximum_width : maximum_width;

	return TRUE;
}

/** Clear min/max width. */

/* virtual */ void
TableContent::ClearMinMaxWidth()
{
	packed.minmax_calculated = 0;
	packed.use_old_row_heights = 0;
}

/** Does width propagation from this table affect FlexRoot? */

BOOL
TableContent::AffectsFlexRoot() const
{
	if (GetFixedLayout() == FIXED_LAYOUT_OFF)
		return FALSE;

	LayoutProperties* cascade = placeholder->GetCascade();

	if (cascade->container)
		return cascade->container->AffectsFlexRoot();
	else
		if (cascade->flexbox)
			return cascade->flexbox->AffectsFlexRoot();

	OP_ASSERT(!"Unexpected containing element type for this table");

	return FALSE;
}

/** Return TRUE if this element can be split into multiple outer columns or pages. */

/* virtual */ BOOL
TableContent::IsColumnizable() const
{
	return TRUE;
}

/** Distribute content into columns. */

/* virtual */ BOOL
TableContent::Columnize(Columnizer& columnizer, Container* container)
{
	BREAK_POLICY prev_page_break_policy = BREAK_AVOID;
	BREAK_POLICY prev_column_break_policy = BREAK_AVOID;

	for (TableListElement* elm = (TableListElement*) layout_stack.First(); elm; elm = elm->Suc())
	{
		if (elm->IsRowGroup() && ((TableRowGroupBox*) elm)->IsVisibilityCollapsed())
			continue;

		LayoutCoord virtual_position = elm->GetStaticPositionedY();
		BREAK_POLICY cur_column_break_policy = elm->GetColumnBreakPolicyBefore();
		BREAK_POLICY column_break_policy = CombineBreakPolicies(prev_column_break_policy, cur_column_break_policy);
		BREAK_POLICY cur_page_break_policy = elm->GetPageBreakPolicyBefore();
		BREAK_POLICY page_break_policy = CombineBreakPolicies(prev_page_break_policy, cur_page_break_policy);

		if (BreakForced(page_break_policy))
		{
			if (!columnizer.ExplicitlyBreakPage(elm))
				return FALSE;
		}
		else
			if (BreakForced(column_break_policy))
				if (!columnizer.ExplicitlyBreakColumn(elm))
					return FALSE;

		if (!packed.avoid_column_break_inside && BreakAllowedBetween(prev_column_break_policy, cur_column_break_policy)
			&& (columnizer.GetColumnsLeft() > 0 ||
				!packed.avoid_page_break_inside && BreakAllowedBetween(prev_page_break_policy, cur_page_break_policy)))
		{
			// We are allowed to move on to the next column/page, if necessary.

			/* Be sure to commit the vertical border spacing preceding this row
			   before adding this row.

			   Other than the fact that it makes it marginally easier to
			   implement, there's no strong reason why we don't let a preceding
			   border spacing join the row in the next column instead. The
			   specs are silent, but maybe one might argue that doing it this
			   way makes it more identical to how we treat margins across
			   column boundaries. */

			columnizer.AdvanceHead(virtual_position);

			if (!columnizer.CommitContent())
				return FALSE;
		}

		columnizer.AllocateContent(virtual_position, elm);

		if (!elm->Columnize(columnizer, container))
			return FALSE;

		columnizer.AdvanceHead(virtual_position + elm->GetHeight());

		prev_page_break_policy = elm->GetPageBreakPolicyAfter();
		prev_column_break_policy = elm->GetColumnBreakPolicyAfter();
	}

	return TRUE;
}

/** Figure out to which column(s) or spanned element a box belongs. */

/* virtual */ void
TableContent::FindColumn(ColumnFinder& cf)
{
	for (TableListElement* elm = (TableListElement*) layout_stack.First();
		 elm && !cf.IsBoxEndFound();
		 elm = elm->Suc())
		if (!elm->IsRowGroup() || !((TableRowGroupBox*) elm)->IsVisibilityCollapsed())
		{
			cf.SetPosition(elm->GetStaticPositionedY());
			elm->FindColumn(cf);
		}
}

// Returns FALSE if there is more then 1 character since last <BR> element on current line.

BOOL
Container::IgnoreSSR_BR(HTML_Element* br_element) const
{
	OP_ASSERT(br_element->Type() == Markup::HTE_BR);

	int char_count = 0;
	HTML_Element* start_element = GetStartElementOfCurrentLine();

	if (start_element)
	{
		HTML_Element* element = (HTML_Element*) br_element->Prev();

		while (element && element->Type() != Markup::HTE_BR)
		{
			Box* box = element->GetLayoutBox();

			if (box && box->IsContentReplaced())
				return FALSE;

			if (box && box->IsTextBox())
				char_count += ((Text_Box*) box)->GetCharCount();

			if (char_count > 1)
				return FALSE;

			if (element == start_element)
				break;

			element = (HTML_Element*) element->Prev();
		}
	}

	return char_count <= 1;
}

/** Get the distance from the top/left border edge of this block formatting
	context to the top/left border edge of this container. (Note that this
	means that if this box itself establishes a new block formatting
	context, the values will be 0).

	All three values must be 0 when calling this method. */

void
Container::GetBfcOffsets(LayoutCoord& bfc_x, LayoutCoord& bfc_y, LayoutCoord& bfc_min_y) const
{
	if (placeholder->GetLocalSpaceManager() && (!placeholder->IsBlockBox() || placeholder->GetReflowState()))
	{
		/* Don't go past the block formatting context. If there is no reflow
		   state, however, it means that the box has been illegally terminated,
		   and we must keep searching for a parent block formatting context. */

		return;
	}

	OP_ASSERT(placeholder->IsBlockBox());

	((BlockBox*) placeholder)->AddNormalFlowPosition(bfc_x, bfc_y);
	bfc_min_y += ((BlockBox*) placeholder)->GetMinY();

	if (reflow_state && reflow_state->parent_bfc_y != LAYOUT_COORD_MIN)
	{
		// Use previously cached values.

		bfc_x += reflow_state->parent_bfc_x;
		bfc_y += reflow_state->parent_bfc_y;
		bfc_min_y += reflow_state->parent_bfc_min_y;
	}
	else
	{
		// Find values from parent container.

		Container* container = placeholder->GetContainingElement()->GetLayoutBox()->GetContainer();
		LayoutCoord parent_bfc_x(0);
		LayoutCoord parent_bfc_y(0);
		LayoutCoord parent_bfc_min_y(0);

		container->GetBfcOffsets(parent_bfc_x, parent_bfc_y, parent_bfc_min_y);

		if (reflow_state)
		{
			// Cache values for next time.

			reflow_state->parent_bfc_x = parent_bfc_x;
			reflow_state->parent_bfc_y = parent_bfc_y;
			reflow_state->parent_bfc_min_y = parent_bfc_min_y;
		}

		bfc_x += parent_bfc_x;
		bfc_y += parent_bfc_y;
		bfc_min_y += parent_bfc_min_y;
	}
}

/** Get new space from the specified space manager. */

LayoutCoord
Container::GetSpace(SpaceManager* space_manager, LayoutCoord& y, LayoutCoord& x, LayoutCoord& width, LayoutCoord min_width, LayoutCoord min_height, VerticalLock vertical_lock) const
{
	/* The space_manager argument is an optimization; it was already calculated, so don't waste
	   time on doing it here. But it should always be the same space_manager as established by this
	   container, or the nearest ancestor container that does. Let's check: */

	OP_ASSERT(placeholder->GetLocalSpaceManager() && placeholder->GetLocalSpaceManager() == space_manager ||
			  !placeholder->GetLocalSpaceManager() && placeholder->GetCascade()->space_manager == space_manager);

	if (space_manager->Empty())
		/* We can do an early cut-off, since there are no floats in the block formatting
		   context. We have all the space in the world, and any suggested position is fine. */

		return LAYOUT_COORD_MAX;

	LayoutCoord bfc_y(0);
	LayoutCoord dummy(0);
	LayoutCoord bfc_x(0);

	GetBfcOffsets(bfc_x, bfc_y, dummy);

	// Convert coordinates from container-relative to BFC-relative.

	LayoutCoord new_bfc_y = bfc_y + y;
	LayoutCoord new_bfc_x = bfc_x + x;

	// Get space.

	LayoutCoord max_height = space_manager->GetSpace(new_bfc_y, new_bfc_x, width, min_width, min_height, vertical_lock);

	// Convert coordinates back from BFC-relative to container-relative.

	y = new_bfc_y - bfc_y;
	x = new_bfc_x - bfc_x;

	return max_height;
}

/** Get maximum width of all floats at the specified minimum Y position, and change minimum Y if necessary. */

void
Container::GetFloatsMaxWidth(SpaceManager* space_manager, LayoutCoord& min_y, LayoutCoord min_height, LayoutCoord max_width, LayoutCoord& left_floats_max_width, LayoutCoord& right_floats_max_width)
{
	/* The space_manager argument is an optimization; it was already calculated, so don't waste
	   time on doing it here. But it should always be the same space_manager as established by this
	   container, or the nearest ancestor container that does. Let's check: */

	OP_ASSERT(placeholder->GetLocalSpaceManager() && placeholder->GetLocalSpaceManager() == space_manager ||
			  !placeholder->GetLocalSpaceManager() && placeholder->GetCascade()->space_manager == space_manager);

	if (space_manager->Empty())
	{
		/* We can do an early cut-off, since there are no floats in the block formatting
		   context. We have all the space in the world, and any suggested position is fine. */

		left_floats_max_width = LayoutCoord(0);
		right_floats_max_width = LayoutCoord(0);

		return;
	}

	LayoutCoord dummy_bfc_y(0);
	LayoutCoord bfc_min_y(0);
	LayoutCoord dummy_bfc_x(0);

	GetBfcOffsets(dummy_bfc_x, dummy_bfc_y, bfc_min_y);

	LayoutCoord new_bfc_min_y = bfc_min_y + min_y;

	space_manager->GetFloatsMaxWidth(new_bfc_min_y, min_height, max_width, reflow_state->bfc_left_edge_min_distance, reflow_state->bfc_right_edge_min_distance, reflow_state->total_max_width, left_floats_max_width, right_floats_max_width);

	min_y = new_bfc_min_y - bfc_min_y;
}

/** Adjusts uncommitted content box */

void
LineReflowState::AdjustPendingBoundingBox(RECT &bounding_box, RECT &content_rect, LayoutCoord relative_x, LayoutCoord relative_y, LayoutCoord allocated_position, const InlineVerticalAlignment& valign)
{
	if (valign.loose)
	{
		if (valign.top_aligned)
		{
			if (uncommitted.content_box_below_top < relative_y + content_rect.bottom)
				uncommitted.content_box_below_top = LayoutCoord(relative_y + content_rect.bottom);

			if (uncommitted.bounding_box_above_top < relative_y - bounding_box.top)
				uncommitted.bounding_box_above_top = LayoutCoord(relative_y - bounding_box.top);

			if (uncommitted.bounding_box_below_top < relative_y + bounding_box.bottom)
				uncommitted.bounding_box_below_top = LayoutCoord(relative_y + bounding_box.bottom);
		}
		else
		{
			LayoutCoord box_height = valign.box_above_baseline + valign.box_below_baseline;

			if (uncommitted.content_box_below_bottom < relative_y + content_rect.bottom - box_height)
				uncommitted.content_box_below_bottom = LayoutCoord(relative_y + content_rect.bottom - box_height);

			if (uncommitted.bounding_box_above_bottom < relative_y - bounding_box.top + box_height)
				uncommitted.bounding_box_above_bottom = LayoutCoord(relative_y - bounding_box.top + box_height);

			if (uncommitted.bounding_box_below_bottom < relative_y + bounding_box.bottom - (-bounding_box.top + box_height))
				uncommitted.bounding_box_below_bottom = LayoutCoord(relative_y + bounding_box.bottom - (-bounding_box.top + box_height));
		}
	}
	else
	{
		if (uncommitted.content_box_below_baseline < relative_y + content_rect.bottom - valign.box_above_baseline)
			uncommitted.content_box_below_baseline = LayoutCoord(relative_y + content_rect.bottom - valign.box_above_baseline);

		if (uncommitted.bounding_box_above_baseline < -(bounding_box.top + relative_y - valign.box_above_baseline))
			uncommitted.bounding_box_above_baseline = LayoutCoord(-(bounding_box.top + relative_y - valign.box_above_baseline));

		if (uncommitted.bounding_box_below_baseline < bounding_box.bottom + relative_y - valign.box_above_baseline)
			uncommitted.bounding_box_below_baseline = LayoutCoord(bounding_box.bottom + relative_y - valign.box_above_baseline);
	}

	if (uncommitted.bounding_box_left_non_relative > bounding_box.left + allocated_position)
		uncommitted.bounding_box_left_non_relative = LayoutCoord(bounding_box.left) + allocated_position;

	if (uncommitted.content_box_right < relative_x + allocated_position + content_rect.right)
		uncommitted.content_box_right = relative_x + allocated_position + LayoutCoord(content_rect.right);

	if (uncommitted.bounding_box_left > bounding_box.left + relative_x + allocated_position)
		uncommitted.bounding_box_left = LayoutCoord(bounding_box.left) + relative_x + LayoutCoord(allocated_position);

	if (uncommitted.bounding_box_right < bounding_box.right + relative_x + allocated_position)
		uncommitted.bounding_box_right = LayoutCoord(bounding_box.right) + relative_x + allocated_position;
}

void
LineReflowState::AdjustPendingAbsPosContentBox(RECT &content_rect, LayoutCoord relative_x, LayoutCoord relative_y, LayoutCoord allocated_position, const InlineVerticalAlignment& valign)
{
	if (content_rect.bottom - content_rect.top)
	{
		if (valign.loose)
		{
			if (valign.top_aligned)
			{
				if (uncommitted.abspos_content_box_below_top < relative_y + content_rect.bottom)
					uncommitted.abspos_content_box_below_top = LayoutCoord(relative_y + content_rect.bottom);
			}
			else
			{
				LayoutCoord box_height = valign.box_above_baseline + valign.box_below_baseline;

				if (uncommitted.abspos_content_box_below_bottom < relative_y + content_rect.bottom - box_height)
					uncommitted.abspos_content_box_below_bottom = LayoutCoord(relative_y + content_rect.bottom - box_height);
			}
		}
		else
			if (uncommitted.abspos_content_box_below_baseline < relative_y + content_rect.bottom - valign.box_above_baseline)
				uncommitted.abspos_content_box_below_baseline = LayoutCoord(relative_y + content_rect.bottom - valign.box_above_baseline);
	}

	if (content_rect.right - content_rect.left)
		if (uncommitted.abspos_content_box_right < relative_x + allocated_position + content_rect.right)
			uncommitted.abspos_content_box_right = relative_x + allocated_position + LayoutCoord(content_rect.right);
}

/** Return TRUE if this element can be split into multiple outer columns or pages. */

/* virtual */ BOOL
Container::IsColumnizable() const
{
	return TRUE;
}

/** Distribute content into columns.

	Returns FALSE if this type of content cannot be columnized. */

/* virtual */ BOOL
Container::Columnize(Columnizer& columnizer, Container* container)
{
	BREAK_POLICY prev_page_break_policy = BREAK_AVOID;
	BREAK_POLICY prev_column_break_policy = BREAK_AVOID;
	BOOL is_multipane_container = IsMultiPaneContainer();

	for (VerticalLayout* vertical_layout = (VerticalLayout*) layout_stack.First();
		 vertical_layout;
		 vertical_layout = vertical_layout->Suc())
	{
		LayoutCoord virtual_position = vertical_layout->GetStackPosition();

		if (!vertical_layout->IsInStack(TRUE))
		{
			/* Don't let elements that are not in the normal flow (absolutely
			   positioned elements) affect columnization. They may become start
			   or stop elements, though. */

			columnizer.AllocateContent(virtual_position, ColumnBoundaryElement(vertical_layout, this));
			continue;
		}
		else
			if (vertical_layout->IsLine() && static_cast<Line*>(vertical_layout)->IsOnlyOutsideListMarkerLine())
				continue;

		BREAK_POLICY cur_column_break_policy = vertical_layout->GetColumnBreakPolicyBefore();
		BREAK_POLICY column_break_policy = CombineBreakPolicies(prev_column_break_policy, cur_column_break_policy);
		BREAK_POLICY cur_page_break_policy = vertical_layout->GetPageBreakPolicyBefore();
		BREAK_POLICY page_break_policy = CombineBreakPolicies(prev_page_break_policy, cur_page_break_policy);

		if (BreakForced(page_break_policy))
		{
			if (!columnizer.ExplicitlyBreakPage(ColumnBoundaryElement(vertical_layout, this)))
				return FALSE;

			// Reset break policy in case we don't get the opportunity to set it further below.

			prev_page_break_policy = BREAK_AVOID;
		}
		else
			if (BreakForced(column_break_policy))
			{
				if (!columnizer.ExplicitlyBreakColumn(ColumnBoundaryElement(vertical_layout, this)))
					return FALSE;

				// Reset break policy in case we don't get the opportunity to set it further below.

				prev_column_break_policy = BREAK_AVOID;
			}

#ifdef PAGED_MEDIA_SUPPORT
		columnizer.SetPageBreakPolicyBeforeCurrent(cur_page_break_policy);
#endif // PAGED_MEDIA_SUPPORT

		BOOL spanned = FALSE;
		BlockBox* block_box = vertical_layout->IsBlock() ? (BlockBox*) vertical_layout : NULL;

		if (block_box && block_box->IsColumnSpanned())
			if (columnizer.GetColumnsLeft() > 0)
			{
				/* Spanned element. We will "columnize" spanned elements as
				   "normally", and the only reason for that is to get page
				   breaking right. The spanned element will of course not have
				   its content put in multiple columns beside each other (but
				   due to page breaking, we may have to put it in multiple
				   rows; one per page). */

				if (!columnizer.EnterSpannedBox(block_box, this))
					return FALSE;

				spanned = TRUE;
			}
			else
				/* We're already in a column that has overflowed in the inline
				   direction. This element cannot be spanned then, according to
				   the spec. Too bad we have already laid it out as spanned,
				   but let's at least keep it in the overflow area. */

				columnizer.SkipSpannedBox(block_box);

		if (!spanned)
			if ((is_multipane_container || !packed.avoid_column_break_inside) &&
				BreakAllowedBetween(prev_column_break_policy, cur_column_break_policy)
				&& (columnizer.GetColumnsLeft() > 0 ||
					!packed.avoid_page_break_inside && BreakAllowedBetween(prev_page_break_policy, cur_page_break_policy)))
				// We are allowed to move on to the next column/page, if necessary.

				if (!columnizer.CommitContent())
					return FALSE;

		BOOL skip_element = FALSE;

		if (block_box)
			if (block_box->IsFloatingBox())
			{
				/* We don't paginate or columnize floats, so prohibit page and
				   column breaks on content beside floats. */

				FloatingBox* floating_box = (FloatingBox*) block_box;
				LayoutCoord top_margin_pos = virtual_position - floating_box->GetMarginTop();

				OP_ASSERT(!spanned);
				columnizer.AllocateContent(top_margin_pos, floating_box);
				columnizer.SetEarliestBreakPosition(top_margin_pos + floating_box->GetHeightAndMargins());
				skip_element = TRUE;
			}
			else
				if (block_box->IsFloatedPaneBox())
				{
					columnizer.AddPaneFloat((FloatedPaneBox*) block_box);
					skip_element = TRUE;
				}

		if (!skip_element)
		{
			columnizer.AllocateContent(virtual_position, ColumnBoundaryElement(vertical_layout, this));

			if (!vertical_layout->Columnize(columnizer, this))
				return FALSE;

			columnizer.AdvanceHead(virtual_position + vertical_layout->GetLayoutHeight());

			if (spanned)
				if (!columnizer.LeaveSpannedBox((BlockBox*) vertical_layout, this))
					return FALSE;

			prev_column_break_policy = vertical_layout->GetColumnBreakPolicyAfter();
			prev_page_break_policy = vertical_layout->GetPageBreakPolicyAfter();
		}

#ifdef PAGED_MEDIA_SUPPORT
		columnizer.SetPageBreakPolicyAfterPrevious(prev_page_break_policy);
#endif // PAGED_MEDIA_SUPPORT
	}

	if (!is_multipane_container)
		if (placeholder->IsBlockBox() && !((BlockBox*) placeholder)->IsColumnSpanned())
		{
			BlockBox* this_block = (BlockBox*) placeholder;

			if (!packed.avoid_column_break_inside &&
				(columnizer.GetColumnsLeft() > 1 || !packed.avoid_page_break_inside))
				return columnizer.AddEmptyBlockContent(this_block, container);

			columnizer.AllocateContent(this_block->GetLayoutHeight(), this_block);
		}

	return TRUE;
}

/** Figure out to which column(s) or spanned element a box belongs. */

/* virtual */ void
Container::FindColumn(ColumnFinder& cf)
{
	for (VerticalLayout* vertical_layout = (VerticalLayout*) layout_stack.First();
		 vertical_layout && !cf.IsBoxEndFound();
		 vertical_layout = vertical_layout->Suc())
		vertical_layout->FindColumn(cf, this);
}

/** Change the used height if computed height was auto. */

BOOL
Container::ForceHeightChange(LayoutCoord height_increase)
{
	if (packed.height_auto)
	{
		height += height_increase;
		return TRUE;
	}

	return FALSE;
}

void
Container::ClearFlexibleMarkers(BOOL after_single_outside /* = FALSE */)
{
	if (reflow_state->list_item_marker_state & FLEXIBLE_MARKER_NOT_ALIGNED)
	{
		if (after_single_outside)
			reflow_state->list_item_marker_state |= FLEXIBLE_MARKER_ALIGNED_TO_SINGLE_OUTSIDE;
		else
			reflow_state->list_item_marker_state &= ~FLEXIBLE_MARKER_NOT_ALIGNED;
	}

	if (reflow_state->list_item_marker_state & ANCESTOR_HAS_FLEXIBLE_MARKER)
	{
		if (!after_single_outside)
			reflow_state->list_item_marker_state &= ~ANCESTOR_HAS_FLEXIBLE_MARKER;

		placeholder->GetCascade()->container->ClearFlexibleMarkers(after_single_outside);
	}
}

void
Container::AlignMarkers(LayoutCoord baseline, MarkerAlignControlValue control_value)
{
	/* Skip the alignment during first call in case of aligning to the line
	   that contains only an outside marker, as there is no point to align a marker to itself. */
	if (control_value != MARKER_ALIGN_TO_SINGLE_OUTSIDE_FIRST_CALL)
		if (reflow_state->list_item_marker_state & FLEXIBLE_MARKER_NOT_ALIGNED)
			if (control_value == MARKER_ALIGN_PROPER_LINE ||
				!(reflow_state->list_item_marker_state & FLEXIBLE_MARKER_ALIGNED_TO_SINGLE_OUTSIDE))
			{
				// Find first line on stack (floats may precede).
				VerticalLayout* first_line = static_cast<VerticalLayout*>(layout_stack.First());
				OP_ASSERT(first_line);
				while (!first_line->IsLine())
				{
					OP_ASSERT(first_line->IsBlock() && static_cast<BlockBox*>(first_line)->IsFloatingBox());
					first_line = first_line->Suc();
					OP_ASSERT(first_line);
				}

				Line* line = static_cast<Line*>(first_line);
				LayoutCoord line_y = line->GetY();
				LayoutCoord diff = baseline - (line_y + line->GetBaseline());

				/* Flexible marker, after initial layout, can't be above
				   the principal box'es top edge. */
				OP_ASSERT(line_y >= 0);

				line_y += diff; // New line_y after that.

				if (line_y < 0)
				{
					/* Not allowing to pull the marker up above the top edge
					   of the container. That would cause some complications
					   (like the need for bounding box update when closing
					   the container) and there is no clear rationale to do so. */
					diff -= line_y;
					line_y = LayoutCoord(0);
				}

				if (diff < 0)
					/* Need to update the possible reflow position after finishing
					   the container caused by the line with only outside marker
					   on it, because when the list item marker is pulled up,
					   the line ends earlier. On the other hand don't need to
					   care when the marker is pushed down, because in such case
					   the reflow position after finishing will be smaller than
					   the one from normal flow. */
					reflow_state->list_marker_bottom += diff;
				line->SetPosition(line->GetX(), line_y);
			}

	if (reflow_state->list_item_marker_state & ANCESTOR_HAS_FLEXIBLE_MARKER)
	{
		OP_ASSERT(placeholder->IsBlockBox());
		OP_ASSERT(placeholder->GetCascade()->container);
		LayoutCoord y_offset = static_cast<BlockBox*>(placeholder)->GetY();

		placeholder->GetCascade()->container->AlignMarkers(baseline + y_offset,
			control_value == MARKER_ALIGN_TO_SINGLE_OUTSIDE_FIRST_CALL ? MARKER_ALIGN_TO_SINGLE_OUTSIDE_RECURSIVE_CALL : control_value);
	}
}

/** Find the outer multi-pane container of the specified element. */

/* static */ Container*
Container::FindOuterMultiPaneContainerOf(HTML_Element* element)
{
	Container* outer_multipane_container = NULL;

	for (HTML_Element* ce = Box::GetContainingElement(element); ce; ce = Box::GetContainingElement(ce))
	{
		Box* b = ce->GetLayoutBox();

		if (Container* c = b->GetContainer())
			if (c->IsMultiPaneContainer())
				outer_multipane_container = c;

		if (!b->IsColumnizable())
			/* We can stop looking; we are past the root multi-pane container
			   that affects us. */

			break;
	}

	return outer_multipane_container;
}

/** Call a method on the actual super class of a MultiColumnContainer object. */

#define MC_CALL_SUPER(call) \
	do { \
		if (multicol_packed.is_scrollable) \
			ScrollableContainer::call; \
		else									\
			if (multicol_packed.is_shrink_to_fit) \
				ShrinkToFitContainer::call; \
			else \
				BlockContainer::call; \
	} while (0)

/** Call a method on the actual super class of a MultiColumnContainer object, and return its return value. */

#define MC_RETURN_CALL_SUPER(call) \
	do { \
		if (multicol_packed.is_scrollable) \
			return ScrollableContainer::call; \
		else									\
			if (multicol_packed.is_shrink_to_fit) \
				return ShrinkToFitContainer::call; \
			else \
				return BlockContainer::call; \
	} while (0)

MultiColumnContainer::MultiColumnContainer()
	: virtual_height(0),
	  floats_virtual_height(0),
	  min_height(0),
	  max_height(0),
	  initial_height(0),
	  column_count(0),
	  column_width(0),
	  content_width(0),
	  column_gap(0),
	  multicol_packed_init(0),
	  top_border_padding(0),
	  bottom_border_padding(0)
{
}

/** Create and run the columnizer for this multicol container. */

BOOL
MultiColumnContainer::RunColumnizer(Columnizer* ancestor_columnizer,
									const LayoutInfo& info,
									LayoutCoord& content_height,
									LayoutCoord max_height,
									BOOL ancestor_columnizer_here,
									BOOL trial)
{
	OP_ASSERT(ancestor_columnizer || !ancestor_columnizer_here);

	// Delete rows and their columns, if any. They will be (re)built now.

	rows.Clear();

	// Distribute content into rows and columns.

	Columnizer columnizer(ancestor_columnizer,
						  ancestor_columnizer_here,
						  info,
						  rows,
						  (MultiColBreakpoint*) breakpoints.First(),
						  (SpannedElm*) spanned_elms.First(),
						  max_height,
						  column_count,
						  column_width,
						  content_width,
						  column_gap,
						  LayoutCoord(0),
						  top_border_padding,
						  virtual_height,
						  floats_virtual_height,
						  !multicol_packed.auto_height || multicol_packed.has_max_height,
#ifdef SUPPORT_TEXT_DIRECTION
						  multicol_packed.rtl,
#endif // SUPPORT_TEXT_DIRECTION
						  multicol_packed.balance);

	if (trial)
		columnizer.SetTrial();

	if (!Container::Columnize(columnizer, NULL))
		return FALSE;

	if (!columnizer.Finalize(
#ifdef PAGED_MEDIA_SUPPORT
			GetPagedContainer() != NULL
#endif // PAGED_MEDIA_SUPPORT
			))
		return FALSE;

	if (multicol_packed.auto_height)
		if (columnizer.ReachedMaxHeight() && multicol_packed.has_max_height)
			content_height = max_height;
		else
			if (ColumnRow* last_row = rows.Last())
				content_height = last_row->GetPosition() + last_row->GetHeight() + columnizer.GetPendingMargin();
			else
				content_height = LayoutCoord(0);
	else
		content_height = initial_height;

	return TRUE;
}

/** Prepare for columnization. */

void
MultiColumnContainer::PrepareColumnization(const LayoutInfo& info, const HTMLayoutProperties& props, LayoutCoord height)
{
	/* Calculate virtual height. This is the internal content box height of
	   this container as far as layout is concerned. The multicol container
	   was laid out as if it only had one column. The columnizer will
	   create the multi-column effect, using Column objects. */

	virtual_height = reflow_state->reflow_position + reflow_state->margin_state.GetHeight();

	if (virtual_height < reflow_state->float_bottom)
		virtual_height = reflow_state->float_bottom;

	virtual_height -= top_border_padding;

	if (virtual_height < 0)
		virtual_height = LayoutCoord(0);

	/* Reset floats' virtual bottom and pending bottom margin, since we
	   have taken them into account now. */

	reflow_state->float_bottom = LayoutCoord(0);
	reflow_state->float_min_bottom = LayoutCoord(0);
	reflow_state->margin_state.Reset();

	// Calculate initial height

	initial_height = height;
	OP_ASSERT(initial_height >= 0 || initial_height == CONTENT_HEIGHT_AUTO);
	multicol_packed.auto_height = initial_height == CONTENT_HEIGHT_AUTO;

	if (!multicol_packed.auto_height)
		if (ScrollableContainer* sc = GetScrollableContainer())
		{
			initial_height -= sc->GetHorizontalScrollbarWidth();

			if (initial_height < 0)
				initial_height = LayoutCoord(0);
		}

	// Calculate min and max height, if any should be set

	min_height = props.min_height;
	max_height = props.max_height >= 0 ? props.max_height : LAYOUT_COORD_HALF_MAX;
	multicol_packed.has_max_height = props.max_height >= 0;

	// Convert to content-box values:

	if (props.box_sizing == CSS_VALUE_border_box)
	{
		LayoutCoord vertical_border_padding = top_border_padding + bottom_border_padding;

		if (min_height != CONTENT_SIZE_AUTO)
		{
			min_height -= vertical_border_padding;

			if (min_height < 0)
				min_height = LayoutCoord(0);
		}

		max_height -= vertical_border_padding;

		if (max_height < 0)
			max_height = LayoutCoord(0);
	}

	// Balance if we're told to, or if we have to.

	multicol_packed.balance = props.column_fill == CSS_VALUE_balance;

	if (initial_height == CONTENT_HEIGHT_AUTO)
	{
		if (!multicol_packed.balance)
#ifdef PAGED_MEDIA_SUPPORT
			if (info.paged_media == PAGEBREAK_OFF)
#endif // PAGED_MEDIA_SUPPORT
			{
#ifdef PAGED_MEDIA_SUPPORT
				BOOL in_paged_container = FALSE;

				if (Container* ancestor_multipane = placeholder->GetCascade()->multipane_container)
					if (ancestor_multipane->GetPagedContainer())
						in_paged_container = TRUE;

				if (!in_paged_container)
#endif // PAGED_MEDIA_SUPPORT
					/* Auto height in continuous media means that the
					   columns need to be balanced, even if column-fill
					   says otherwise by being auto. */

					multicol_packed.balance = 1;
			}
	}
	else
		if (max_height > initial_height)
			max_height = initial_height;

	if (max_height < min_height)
		max_height = min_height;

	for (SpannedElm* spanned_elm = (SpannedElm*) spanned_elms.First(); spanned_elm;)
	{
		SpannedElm* next_spanned_elm = (SpannedElm*) spanned_elm->Suc();

		if (!spanned_elm->GetSpannedBox())
		{
			/* No block box here. This may mean that the spanned box was a
			   run-in that was converted to inline during layout. Remove
			   the bastard from the list. */

			spanned_elm->CancelBreak();
			spanned_elm->Out();
			OP_DELETE(spanned_elm);
		}

		spanned_elm = next_spanned_elm;
	}
}

/** Update height of container. */

/* virtual */ void
MultiColumnContainer::UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height)
{
	if (!initial_content_height)
	{
		const HTMLayoutProperties& props = *cascade->GetProps();
		LayoutCoord used_height = reflow_state->css_height;
		BOOL has_outer_multicol = placeholder->IsColumnizable(TRUE);

		if (cascade->flexbox)
		{
			FlexItemBox* item_box = (FlexItemBox*) placeholder;
			LayoutCoord stretched_height = item_box->GetFlexHeight(cascade);
			LayoutCoord auto_height = content_height;
			LayoutCoord ver_border_padding = top_border_padding + bottom_border_padding;

			OP_ASSERT(!has_outer_multicol);

			/* Perform a columnization pass to calculate minimum / hypothetical
			   height for propagation. */

			PrepareColumnization(info, props, CONTENT_HEIGHT_AUTO);

			if (!RunColumnizer(NULL, info, auto_height, max_height, FALSE, TRUE))
			{
				info.hld_profile->SetIsOutOfMemory(TRUE);
				return;
			}

			auto_height += ver_border_padding;

			LayoutCoord hypothetical_height = used_height >= 0 ? used_height + ver_border_padding : auto_height;

			item_box->PropagateHeights(auto_height, hypothetical_height);

			if (stretched_height != CONTENT_HEIGHT_AUTO)
			{
				used_height = stretched_height - top_border_padding - bottom_border_padding;

				if (used_height < 0)
					used_height = LayoutCoord(0);
			}
		}

		PrepareColumnization(info, props, used_height);

		if (has_outer_multicol && initial_height != CONTENT_HEIGHT_AUTO)
			content_height = initial_height;
		else
			/* If there's an outer multicol container here, this is just going
			   to be a "trial" columnization, where we attempt to guess the
			   height. The correct height and layout (and outer column
			   distribution) is set during columnization of the outer multicol
			   container, which will recolumnize this inner multicol container
			   properly.

			   It is problematic if we guess the wrong height during trial
			   columnization, since content following this multicol container
			   will depend on that height to find their layout position.
			   Correcting the height later without laying out over again is not
			   good (but it is what the current code half-heartedly tries to
			   do), because there is no reliable way of compensating for this
			   height difference on successive content, let alone containers
			   between the inner and outer multicol. We only risk guessing
			   incorrectly if the inner multicol is split across multiple outer
			   columns, though.

			   The only way to solve this properly is to allow multiple reflow
			   passes for nested multicol. */

			if (!RunColumnizer(NULL, info, content_height, max_height, FALSE, has_outer_multicol))
			{
				info.hld_profile->SetIsOutOfMemory(TRUE);
				return;
			}

		// Honor height, min-height and max-height.

		if (content_height > max_height)
			content_height = max_height;

		if (content_height < min_height)
			content_height = min_height;

		/* Store what we ended up with. It will be picked up if this multicol
		   container is columnized later (in case of nested multicol
		   containers). */

		initial_height = content_height;

		if (reflow_state->calculate_min_max_widths)
		{
			BOOL affected_by_ancestor_width = !reflow_state->inside_fixed_width &&
				(props.GetMinWidthIsPercent() || props.GetMaxWidthIsPercent() ||
				 props.box_sizing == CSS_VALUE_border_box && (props.GetPaddingLeftIsPercent() ||
															  props.GetPaddingRightIsPercent()));

			if (affected_by_ancestor_width)
			{
				/* Since the size of this multicol container may be affected by
				   the size of some ancestor, we cannot use any real output
				   from the layout engine. The calculation below is very
				   inaccurate. */

				short initial_count = MAX(props.column_count, 1);

				min_content_height = LayoutCoord((min_content_height + initial_count - 1) / initial_count);
			}
			else
				min_content_height = content_height;

			if (min_content_height > max_height && !affected_by_ancestor_width)
				min_content_height = max_height;

			if (min_content_height < min_height && !affected_by_ancestor_width)
				min_content_height = min_height;
		}

		// Calculate and set bounding box.

		OpRect bbox_rect(props.border.left.width + props.padding_left,
						 props.border.top.width + props.padding_top,
						 GetOuterContentWidth(),
						 content_height);

		for (ColumnRow* row = rows.First(); row; row = row->Suc())
			if (Column* column = row->GetLastColumn())
#ifdef SUPPORT_TEXT_DIRECTION
				if (props.direction == CSS_VALUE_rtl)
				{
					LayoutCoord left = LayoutCoord(props.border.left.width) + props.padding_left + column->GetX();
					LayoutCoord diff = LayoutCoord(bbox_rect.x) - left;

					if (diff > 0)
					{
						bbox_rect.x -= diff;
						bbox_rect.width += diff;
					}
				}
				else
#endif // SUPPORT_TEXT_DIRECTION
				{
					LayoutCoord width = column->GetX() + GetColumnWidth();

					if (bbox_rect.width < width)
						bbox_rect.width = width;
				}

		AbsoluteBoundingBox bbox;
		bbox.Set(LayoutCoord(bbox_rect.x), LayoutCoord(bbox_rect.y), LayoutCoord(bbox_rect.width), LayoutCoord(bbox_rect.height));
		bbox.SetContentSize(LayoutCoord(bbox_rect.width), LayoutCoord(bbox_rect.height));

		if (multicol_packed.is_scrollable)
			UpdateContentSize(bbox);

		MC_CALL_SUPER(UpdateBoundingBox(bbox, FALSE));
	}

	MC_CALL_SUPER(UpdateHeight(info, cascade, content_height, min_content_height, initial_content_height));
}

/** Apply min-width property constraints on min/max values. */

/* virtual */ void
MultiColumnContainer::ApplyMinWidthProperty(const HTMLayoutProperties& props, LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width)
{
	if (multicol_packed.is_shrink_to_fit)
		ShrinkToFitContainer::ApplyMinWidthProperty(props, min_width, normal_min_width, max_width);
	else
		BlockContainer::ApplyMinWidthProperty(props, min_width, normal_min_width, max_width);
}

/** Finish reflowing box. */

/* virtual */ LAYST
MultiColumnContainer::FinishLayout(LayoutInfo& info)
{
	OP_ASSERT(pending_logical_floats.Empty());
	MC_RETURN_CALL_SUPER(FinishLayout(info));
}

/** Update screen. */

/* virtual */ void
MultiColumnContainer::UpdateScreen(LayoutInfo& info)
{
	MC_CALL_SUPER(UpdateScreen(info));

	if (!placeholder->IsInlineBox())
	{
		/* There's missing code to figure out exactly what parts of a multicol
		   container has changed, so we have to update everything. */

		AbsoluteBoundingBox bounding_box;

		placeholder->GetBoundingBox(bounding_box);
		info.visual_device->UpdateRelative(bounding_box.GetX(), bounding_box.GetY(), bounding_box.GetWidth(), bounding_box.GetHeight());
	}
}

/** Update margins and bounding box. See declaration in header file for more information. */

/* virtual */ void
MultiColumnContainer::UpdateBottomMargins(const LayoutInfo& info, const VerticalMargin* bottom_margin, AbsoluteBoundingBox* child_bounding_box, BOOL has_inflow_content /* = TRUE */)
{
	MC_CALL_SUPER(UpdateBottomMargins(info, bottom_margin, NULL, has_inflow_content));
}

/** Expand container to make space for floating and absolute positioned boxes. See declaration in header file for more information. */

/* virtual */ void
MultiColumnContainer::PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts)
{
	if (opts.type == PROPAGATE_FLOAT) // Only propagate floats' bottom
		/* Ignore bounding box. It will be wrong anyway. The values propagated
		   are in the "virtual" coordinate system, where there is only one tall
		   column. We care about the virtual bottom, though. */

		MC_CALL_SUPER(PropagateBottom(info, bottom, min_bottom, AbsoluteBoundingBox(), opts));
}

/** Propagate height of pane-attached float to its flow root. */

/* virtual */ void
MultiColumnContainer::PropagatePaneFloatHeight(LayoutCoord float_height)
{
	if (MultiColBreakpoint* last_break_point = (MultiColBreakpoint*) breakpoints.Last())
		last_break_point->PropagatePaneFloatHeight(float_height);
	else
		floats_virtual_height += float_height;
}

/** Update bounding box. */

/* virtual */ void
MultiColumnContainer::UpdateBoundingBox(const AbsoluteBoundingBox& child_bounding_box, BOOL skip_content)
{
	/* Ignore bounding box. It will be wrong anyway. The values propagated
	   are in the "virtual" coordinate system, where there is only one tall
	   column. We care about the virtual bottom, though. */
}

/** Remove form, plugin and iframe objects */

/* virtual */ void
MultiColumnContainer::Disable(FramesDocument* doc)
{
	MC_CALL_SUPER(Disable(doc));
}

/** Create form, plugin and iframe objects */

/* virtual */ OP_STATUS
MultiColumnContainer::Enable(FramesDocument* doc)
{
	MC_RETURN_CALL_SUPER(Enable(doc));
}

/** Calculate the width of the containing block established by this container. */

/* virtual */ LayoutCoord
MultiColumnContainer::CalculateContentWidth(const HTMLayoutProperties& props, ContainerWidthType width_type) const
{
	if (width_type == OUTER_CONTENT_WIDTH)
		MC_RETURN_CALL_SUPER(CalculateContentWidth(props, INNER_CONTENT_WIDTH));
	else
		return column_width;
}

#ifdef PAGED_MEDIA_SUPPORT

/** Attempt to break page. */

/* virtual */ BREAKST
MultiColumnContainer::AttemptPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain)
{
	/* Page breaking is handled as part of multi-column layout, and we
	   shouldn't end up here. */

	OP_ASSERT(!"Page breaking should be handled by the columnizer");

	return BREAK_NOT_FOUND;
}

#endif // PAGED_MEDIA_SUPPORT

/** Propagate a break point caused by break properties or a spanned element. */

/* virtual */ BOOL
MultiColumnContainer::PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint)
{
	MultiColBreakpoint* new_breakpoint = OP_NEW(MultiColBreakpoint, (virtual_y - top_border_padding, break_type));

	if (!new_breakpoint)
		return FALSE;

	new_breakpoint->Into(&breakpoints);

	if (breakpoint)
		*breakpoint = new_breakpoint;

	return TRUE;
}

/** Add a pending spanned element. */

BOOL
MultiColumnContainer::AddPendingSpannedElm(LayoutProperties* spanned_cascade, MultiColBreakpoint* breakpoint)
{
	const HTMLayoutProperties& box_props = *spanned_cascade->GetProps();
	SpannedElm* elm = OP_NEW(SpannedElm, (spanned_cascade->html_element, breakpoint, box_props.margin_top, box_props.margin_bottom));

	if (!elm)
		return FALSE;

	elm->Into(&spanned_elms);

	return TRUE;
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
MultiColumnContainer::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	multicol_packed.is_scrollable = props.overflow_x != CSS_VALUE_visible
#ifdef PAGED_MEDIA_SUPPORT
		&& !GetPagedContainer()
#endif // PAGED_MEDIA_SUPPORT
		;

	multicol_packed.is_shrink_to_fit = props.IsShrinkToFit(cascade->html_element->Type());

	OP_ASSERT(!placeholder->IsTableCell());

	MC_RETURN_CALL_SUPER(ComputeSize(cascade, info));
}

/** Lay out box. */

/* virtual */ LAYST
MultiColumnContainer::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord available_width = CalculateContentWidth(props, OUTER_CONTENT_WIDTH);

	// Calculate the used values for column-count, column-width and column-gap

	column_gap = props.column_gap;

	if (props.column_width > 0)
	{
		// Let column-width decide

		column_width = props.column_width;

		if (column_width > available_width)
		{
			column_width = available_width;
			column_count = 1;
		}
		else
		{
			column_count = (available_width + column_gap) / (props.column_width + column_gap);
			OP_ASSERT(column_count >= 1);

			if (column_count < 1)
				column_count = 1;

			column_width = LayoutCoord((available_width - column_gap * (column_count - 1)) / column_count);
		}
	}
	else
		if (props.column_count > 0)
		{
			// Let column-count decide

			column_count = props.column_count;

			if (column_gap > 0 && (column_count - 1) * column_gap >= available_width)
			{
				// Gap too wide. Reduce number of columns.

				if (column_gap >= available_width)
					/* Gap is even wider than available width, or at least
					   equal to it. No worries, though. With only one column,
					   there will be no gap. Muhaha! In your face, gap! */

					column_count = 1;
				else
					column_count = available_width / column_gap;
			}

			column_width = LayoutCoord((available_width - column_gap * (column_count - 1)) / column_count);
		}
		else
			OP_ASSERT(!"Not a multi-column element");

	content_width = available_width;

	spanned_elms.Clear();
	breakpoints.Clear();

	/** Move all GCPM floats (from previous layout passes) to a temporary
		list. Unless they get deleted during layout, they will be taken out of
		that list and put back into the regular list when laid out or when some
		ancestor of theirs decide to skip layout of its subtree. */

	pending_logical_floats.Append(&logical_floats);
	logical_floats.RemoveAll();

	top_border_padding = LayoutCoord(props.border.top.width) + props.padding_top;
	bottom_border_padding = LayoutCoord(props.border.bottom.width) + props.padding_bottom;
	floats_virtual_height = LayoutCoord(0);

	multicol_packed.is_table_cell = placeholder->IsTableCell();

#ifdef SUPPORT_TEXT_DIRECTION
	multicol_packed.rtl = props.direction == CSS_VALUE_rtl;
#endif // SUPPORT_TEXT_DIRECTION

	MC_RETURN_CALL_SUPER(Layout(cascade, info, first_child, start_position));
}

/** Get baseline of first or last line.

	@param last_line TRUE if last line baseline search (inline-block case).	*/

/* virtual */ LayoutCoord
MultiColumnContainer::GetBaseline(BOOL last_line /*= FALSE*/) const
{
	// See the comment in the other GetBaseLine() method below.

	OP_ASSERT(!placeholder->IsInlineBox());

	return LayoutCoord(0);
}

/** Get baseline of inline block or content of inline element that
	has a baseline (e.g. ReplacedContent with baseline, BulletContent). */

/* virtual */ LayoutCoord
MultiColumnContainer::GetBaseline(const HTMLayoutProperties& props) const
{
	/* It is not given what's the best thing to do for inline-block /
	   table-cell multicol containers here. For now we just let the baseline be
	   the bottom margin edge of the box for inline-block (like we do for
	   lineless boxes and boxes with non-visible overflow), and the top border
	   edge for table-cell. That is probably good enough. It might of course be
	   even better to return the baseline of the first/highest (table-cells)
	   last/lowest (inline-blocks) line, but we should wait for that to be
	   clarified in the spec before we spend a lot of time on it. */

	OP_ASSERT(placeholder->IsInlineBlockBox());

	return height + props.margin_bottom;
}

/** Signal that content may have changed. */

/* virtual */ void
MultiColumnContainer::SignalChange(FramesDocument* doc)
{
	MC_CALL_SUPER(SignalChange(doc));
}

/** Traverse box. */

/* virtual */ void
MultiColumnContainer::Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
#ifdef LAYOUT_TRAVERSE_DIRTY_TREE
	if (packed.only_traverse_blocks)
		// Just give up if the tree is dirty.

		return;
#endif // LAYOUT_TRAVERSE_DIRTY_TREE

	if (multicol_packed.is_scrollable)
		/* ScrollableContainer will take care of multi-column traversal on its
		   own. It's done like this in order to add the scroll translation
		   BEFORE we enter the multi-column container (so that the column rules
		   are painted at the right positions, for example). */

		ScrollableContainer::Traverse(traversal_object, layout_props);
	else
	{
		const HTMLayoutProperties& props = *layout_props->GetProps();
		LayoutCoord translate_y(LayoutCoord(props.border.top.width) + props.padding_top);

		traversal_object->Translate(LayoutCoord(0), translate_y);
		traversal_object->EnterMultiColumnContainer(layout_props, this);
		TraverseRows(traversal_object, layout_props);
		traversal_object->Translate(LayoutCoord(0), -translate_y);
	}
}

/** Traverse all column rows of the multi-column container. */

void
MultiColumnContainer::TraverseRows(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	TargetTraverseState target_traversal;
	Column* outer_pane = traversal_object->GetCurrentPane();
	LayoutCoord outer_start_offset = traversal_object->GetPaneStartOffset();
	BOOL outer_multicol_started = traversal_object->IsPaneStarted();
	BOOL outer_multicol_done = FALSE;
	BOOL target_done = FALSE;

	traversal_object->StoreTargetTraverseState(target_traversal);

	for (ColumnRow* row = rows.First(); row; row = row->Suc())
	{
		if (outer_pane)
			if (!outer_multicol_started)
				if (outer_pane->GetStartElement().GetColumnRow() == row)
					outer_multicol_started = TRUE;
				else
					continue;
			else
				if (placeholder->IsBlockBox() && outer_pane->GetStartElement().GetVerticalLayout() == (BlockBox*) placeholder)
				{
					const HTMLayoutProperties& props = *layout_props->GetProps();
					LayoutCoord top_border_padding = LayoutCoord(props.border.top.width) + props.padding_top;

					if (outer_pane->GetStartOffset() > row->GetPosition() + top_border_padding)
						continue;
				}

		/* Restore target traversal state before traversing this column. The
		   target element may live in multiple columns. */

		traversal_object->RestoreTargetTraverseState(target_traversal);

		// Traverse

		row->Traverse(this, traversal_object, layout_props);

		if (!target_done)
			target_done = traversal_object->IsTargetDone();

		if (outer_pane)
			if (outer_pane->GetStopElement().GetColumnRow() == row)
			{
				outer_multicol_done = TRUE;
				break;
			}
	}

	if (target_done)
		// We found the target while traversing one or more of these rows/spanned elements

		traversal_object->SwitchTarget(layout_props->html_element);

	traversal_object->SetCurrentPane(outer_pane);
	traversal_object->SetPaneStartOffset(outer_start_offset);
	traversal_object->SetPaneDone(outer_multicol_done);

	if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
		/* Schedule GCPM floats in this multicol container for
		   traversal. Note that there may already be floats in previous
		   multi-pane containers scheduled, and likewise, there may be
		   multi-pane containers after us that have their own floats to
		   schedule. */

		traversal_object->AddPaneFloats(logical_floats);
}

/** Get scroll offset, if applicable for this type of box / content. */

/* virtual */ OpPoint
MultiColumnContainer::GetScrollOffset()
{
	MC_RETURN_CALL_SUPER(GetScrollOffset());
}

/** Set scroll offset, if applicable for this type of box / content. */

/* virtual */ void
MultiColumnContainer::SetScrollOffset(int* x, int* y)
{
	MC_CALL_SUPER(SetScrollOffset(x, y));
}

/** Propagate min/max widths. */

/* virtual */ void
MultiColumnContainer::PropagateMinMaxWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width)
{
	LayoutProperties* cascade = placeholder->GetCascade();
	const HTMLayoutProperties& props = *cascade->GetProps();

	if (props.content_width < 0 || cascade->flexbox)
		/* Only calculate min/max widths here if width is auto (or if this is a
		   flex item). If width is non-auto (fixed), the super class will
		   handle the calculation just fine on its own. */

		if (props.column_count > 0)
			if (props.column_width > 0)
			{
				LayoutCoord content_box_width = LayoutCoord(props.column_width * props.column_count + props.column_gap * (props.column_count - 1));

				min_width = content_box_width;
				normal_min_width = content_box_width;
				max_width = content_box_width;
			}
			else
			{
				/* Available width cannot affect min/max values. So using the
				   "used" values of column-count here would be wrong, as that
				   value may be affected by available width. */

				short gap_extra = (props.column_count - 1) * props.column_gap;

				min_width = LayoutCoord(min_width * props.column_count + gap_extra);
				normal_min_width = LayoutCoord(normal_min_width * props.column_count + gap_extra);
				max_width += LayoutCoord(gap_extra);

				if (max_width < normal_min_width)
					max_width = normal_min_width;
			}
		else
			/* If column-count is auto, there's very little we can say about
			   the width requirements of this multi-column container. But
			   saying that we could use at least one column's width may be
			   reasonable. Maybe. Or maybe not. Let's try. */

			if (props.column_width > 0 && max_width < props.column_width)
				max_width = props.column_width;

	if (multicol_packed.is_shrink_to_fit)
		ShrinkToFitContainer::PropagateMinMaxWidths(info, min_width, normal_min_width, max_width);
	else
		BlockContainer::PropagateMinMaxWidths(info, min_width, normal_min_width, max_width);
}

/** Get the border area (optionally) and translation for 'box'. */

/* virtual */ void
MultiColumnContainer::GetAreaAndPaneTranslation(const Box* box, LayoutCoord& x, LayoutCoord& y, RECT* border_rect, ColumnFinder* ancestor_cf)
{
	x = LayoutCoord(0);
	y = LayoutCoord(0);

	if (border_rect)
	{
		border_rect->left = 0;
		border_rect->top = 0;
		border_rect->right = 0;
		border_rect->bottom = 0;
	}

	if (!rows.First())
		return;

	ColumnFinder cf(box, rows.First(), top_border_padding, ancestor_cf);

	Container::FindColumn(cf);
	OP_ASSERT(cf.IsBoxEndFound());

	x = cf.GetBoxTranslationX() + cf.GetDescendantTranslationX();
	y = top_border_padding + cf.GetBoxTranslationY() + cf.GetDescendantTranslationY();

	if (border_rect)
		*border_rect = cf.GetBorderRect();
}

/** Add statically positioned GCPM float. */

/* virtual */ void
MultiColumnContainer::AddStaticPaneFloat(FloatedPaneBox* box)
{
	box->IntoLogicalList(logical_floats);
}

/** Skip over GCPM floats that are in the subtree established by the specified descendant. */

/* virtual */ void
MultiColumnContainer::SkipPaneFloats(HTML_Element* descendant)
{
	for (PaneFloatEntry* entry = pending_logical_floats.First(); entry; entry = entry->Suc())
		if (descendant->IsAncestorOf(entry->GetBox()->GetHtmlElement()))
		{
			entry->Out();
			entry->Into(&logical_floats);
		}
		else
			break;
}

/** Return TRUE if this multicol container has a column in which the specified float lives. */

BOOL
MultiColumnContainer::HasPaneFloat(FloatedPaneBox* box)
{
	/* This may be considered unnecessarily expensive. If there was only a way
	   to ask the box directly where it belongs, we could answer the question
	   right away. */

	for (ColumnRow* row = rows.First(); row; row = row->Suc())
		for (Column* column = row->GetFirstColumn(); column; column = column->Suc())
			if (column->HasFloat(box))
				return TRUE;

	return FALSE;
}

/** Return TRUE if this element can be split into multiple outer columns or pages. */

/* virtual */ BOOL
MultiColumnContainer::IsColumnizable() const
{
	if (packed.avoid_column_break_inside)
		return FALSE;

	MC_RETURN_CALL_SUPER(IsColumnizable());
}

/** Distribute content into columns. */

/* virtual */ BOOL
MultiColumnContainer::Columnize(Columnizer& columnizer, Container* container)
{
	OP_ASSERT(placeholder->IsColumnizable(TRUE));
	OP_ASSERT(placeholder->IsBlockBox());

	BlockBox* this_block = (BlockBox*) placeholder;

	/* Nested multicol. All we did when finishing layout of this inner multicol
	   container was guess its columnized height (which is needed for balancing
	   columns in the outer multicol container) by doing a trial columnization
	   run. We had to wait for outer multicol container columnization before we
	   could columnize, in order to figure out if (and how) this inner multicol
	   container should be distributed over multiple columns in the outer
	   multicol container. Now is the time for that. */

	rows.Clear();

	LayoutCoord content_height;

	columnizer.EnterChild(top_border_padding, this);

	if (!RunColumnizer(&columnizer, columnizer.GetLayoutInfo(), content_height, max_height, FALSE, FALSE))
		return FALSE;

	columnizer.LeaveChild(top_border_padding);

	if (content_height > max_height)
		content_height = max_height;

	if (content_height < min_height)
		content_height = min_height;

	content_height += top_border_padding + bottom_border_padding;

	LayoutCoord height_change = content_height - height;

	if (height_change)
	{
		/* New height. This may happen when layout changes in the outer
		   multicol container in ways that causes this inner multicol to be
		   distributed across columns in the outer multicol differently from
		   last time. It also happens when the initial inner multicol
		   columnized height guess was wrong.

		   When height changes, it requires us to push successive vertical
		   layout elements down by the amount increased (or pull it up if the
		   height shrunk). This is sort of evil, but can only be avoided if we
		   add an additional reflow pass. */

		HTML_Element* containing_element = placeholder->GetContainingElement();
		VerticalLayout* sibling = ((BlockBox*) placeholder)->Suc();

		while (containing_element)
		{
			for (; sibling; sibling = sibling->Suc())
				sibling->MoveDown(height_change, containing_element);

			Box* b = containing_element->GetLayoutBox();

			/* All successive siblings have been moved. Can we stretch or
			   shrink the height of our container? */

			if (Container* c = b->GetContainer())
				if (!c->GetMultiColumnContainer() || b->IsColumnizable())
					if (c->ForceHeightChange(height_change))
					{
						/* It was possible to stretch or shrink the height of
						   our container, so we can move its successive
						   siblings as well. */

						OP_ASSERT(b->IsBlockBox());

						if (b->IsBlockBox())
							sibling = ((BlockBox*) b)->Suc();

						containing_element = Box::GetContainingElement(containing_element);
						continue;
					}

			break;
		}

		height = content_height;
	}

	if (!this_block->IsColumnSpanned())
	{
		if (!packed.avoid_column_break_inside &&
			(columnizer.GetColumnsLeft() > 1 || !packed.avoid_page_break_inside))
		{
			if (!columnizer.AddEmptyBlockContent(this_block, container))
				return FALSE;
		}
		else
			columnizer.AllocateContent(this_block->GetLayoutHeight(), this_block);
	}

	return TRUE;
}

/** Figure out to which column(s) or spanned element a box belongs. */

/* virtual */ void
MultiColumnContainer::FindColumn(ColumnFinder& cf)
{
	if (GetHtmlElement()->IsAncestorOf(cf.GetTarget()->GetHtmlElement()))
		if (cf.GetTarget() == placeholder)
		{
			cf.SetBoxStartFound();
			cf.SetPosition(GetHeight());
			cf.SetBoxEndFound();
		}
		else
		{
			LayoutCoord x(0);
			LayoutCoord y(0);

			GetAreaAndPaneTranslation(cf.GetTarget(), x, y, &cf.GetBorderRect(), &cf);
			cf.SetDescendantTranslation(x, y);
		}
}

#ifdef PAGED_MEDIA_SUPPORT

/** Call a method on the actual super class of a PagedContainer object. */

#define PC_CALL_SUPER(call) \
	do { \
		if (paged_packed.is_multicol) \
			MultiColumnContainer::call; \
		else if (paged_packed.is_shrink_to_fit)	\
			ShrinkToFitContainer::call; \
		else \
			BlockContainer::call; \
	} while (0)

/** Call a method on the actual super class of a PagedContainer object, and return its return value. */

#define PC_RETURN_CALL_SUPER(call) \
	do { \
		if (paged_packed.is_multicol) \
			return MultiColumnContainer::call; \
		else if (paged_packed.is_shrink_to_fit)	\
			return ShrinkToFitContainer::call; \
		else \
			return BlockContainer::call; \
	} while (0)

#define PAGE_SLIDE_FRAME_DELAY 15
#define PAGE_SLIDE_TOTAL_TIME_MS 200
#define PAGE_DRAG_CHANGE_THRESHOLD 7
#define PAGE_PROPAGATE_PAN_THRESHOLD 15

PagedContainer::PagedContainer()
	: page_control(NULL),
	  visual_device(NULL),
	  slide_start_time(0.0),
	  paged_packed_init(0),
	  current_page(UINT_MAX),
	  page_offset(0),
	  slide_start_offset(0),
	  page_width(0),
	  page_height(0)
{
}

/* virtual */
PagedContainer::~PagedContainer()
{
	g_main_message_handler->UnsetCallBacks(this);

	if (page_control)
	{
		page_control->Delete();
		REPORT_MEMMAN_DEC(sizeof(OpPageControl));
	}
}

/** Initialize the CoreView under the correct parent. */

OP_STATUS
PagedContainer::InitCoreView(VisualDevice* vis_dev)
{
	HTML_Element* html_element = GetHtmlElement();
	CoreView* parent_view = FindParentCoreView(html_element);

	if (!parent_view)
		parent_view = vis_dev->GetView();

	if (!parent_view)
		/* Yes, this may happen. Here's one scenario (there may be more, for all I know):
		   When navigating in browser history, things are sometimes handled in an
		   infavorable order. If we're navigating to a document with an IFRAME, and the
		   document has been deleted (e.g. because of "compatible "History Navigation
		   Mode"), we may end up in a situation where the top document has been freed (no
		   logdoc and no ifrm_root in FramesDocument), while the document created by the
		   IFRAME has been stowed away in DocumentState. When the top document gets
		   parsed, the IFRAME is taken out from the DocumentState storage and
		   re-associated with the top document (via FramesDocument's ifrm_root). This
		   does not Show() the IFRAME's VisualDevice, though, so when FramesDocument's
		   ReactivateDocument() is called, which will call EnableContent() on every
		   element in the tree, there's still no CoreView for the IFRAME's VisualDevice
		   yet (which would have been really useful right about now!). It is not created
		   until FramesDocument's SetCurrentHistoryPos() calls SetAsCurrentDoc() on every
		   FramesDocElm it knows about.

		   Oh well, this is docxs stuff, but I felt like ranting about it somewhere. Now,
		   take a look at ScrollableContainer::InitCoreView(); there's a similar check
		   there, which also claims that this situation is not too unproblematic, as
		   someone will make sure that this method is called again when the weather is
		   better. */

		return OpStatus::OK;

	if (!CoreView::m_parent)
	{
		RETURN_IF_ERROR(CoreView::Construct(parent_view));

		/* We are not using using the paint listeners or input listeners of the
		   CoreView. We only inherit CoreView so the hierarchy of CoreView
		   objects will know about our visible area (so clipping can be done
		   correctly). We also get the InputContext from it so we can get
		   focused. */

		CoreView::SetWantPaintEvents(FALSE);
		CoreView::SetWantMouseEvents(FALSE);
		CoreView::SetVisibility(TRUE);
		CoreView::SetIsLayoutBox(TRUE);
	}

	CoreView::SetOwningHtmlElement(html_element);

	SetParentInputContext(parent_view);

	if (page_control)
		page_control->SetVisualDevice(vis_dev);

	return OpStatus::OK;
}

/** Slide one animation frame, and schedule the next one, if there's time for more. */

void
PagedContainer::SlideOneFrame()
{
	if (!visual_device)
		return;

	FramesDocument* doc = visual_device->GetDocumentManager()->GetCurrentDoc();

	if (!doc)
		return;

	double now = g_op_time_info->GetRuntimeMS();
	int time_elapsed = int(now - slide_start_time);
	int time_left = PAGE_SLIDE_TOTAL_TIME_MS - time_elapsed;

	if (time_left < 0)
		time_left = 0;
	else
		if (time_left > 0)
			g_main_message_handler->PostDelayedMessage(MSG_SLIDE_ANIMATION, (INTPTR) this, 0, PAGE_SLIDE_FRAME_DELAY);

	ScrollTo(current_page, slide_start_offset * LayoutCoord(time_left) / LayoutCoord(PAGE_SLIDE_TOTAL_TIME_MS));

	if (paged_packed.trigger_scroll_event_on_slide)
		if (OpStatus::IsMemoryError(doc->HandleEvent(ONSCROLL, NULL, GetHtmlElement(), SHIFTKEY_NONE)))
			RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

/** Calculate the X and Y translation for the specified page and offset. */

void
PagedContainer::CalculateTranslation(unsigned int page, LayoutCoord offset, LayoutCoord& x, LayoutCoord& y)
{
	if (paged_packed.vertical_pagination)
	{
		x = LayoutCoord(0);
		y = -(LayoutCoord(page) * page_height + offset);
	}
	else
	{
#ifdef SUPPORT_TEXT_DIRECTION
		if (paged_packed.rtl)
			x = LayoutCoord(page) * page_width + offset;
		else
#endif // SUPPORT_TEXT_DIRECTION
			x = -(LayoutCoord(page) * page_width + offset);

		y = LayoutCoord(0);
	}
}

/** Scroll the paged container to the specified page and offset. */

void
PagedContainer::ScrollTo(unsigned int page, LayoutCoord offset)
{
	if (!visual_device)
		return;

	FramesDocument* doc = visual_device->GetDocumentManager()->GetCurrentDoc();

	if (!doc || !doc->GetLogicalDocument())
		return;

	BOOL scroll = FALSE;

	/* If the paged container is established by some non-root/BODY element,
	   it's hard to tell whether it's overlapped by content for which the paged
	   container isn't the containing block (e.g. sibling content or absolutely
	   positioned descendants). So we need to always do a full update unless
	   the paged container is set on the viewport. */

	if (IsLayoutRoot())
	{
		LayoutWorkplace* workplace = doc->GetLogicalDocument()->GetLayoutWorkplace();
		LayoutCoord old_x;
		LayoutCoord old_y;
		LayoutCoord new_x;
		LayoutCoord new_y;

		CalculateTranslation(current_page, page_offset, old_x, old_y);
		CalculateTranslation(page, offset, new_x, new_y);

		if (!workplace->HasBgFixed())
			if (paged_packed.vertical_pagination)
				scroll = op_abs(new_y - old_y) < page_height;
			else
				scroll = op_abs(new_x - old_x) < page_width;

		if (scroll)
		{
			// Scroll and partial update.

			OpRect page_rect(0, 0, block_width, height - GetPageControlHeight());
			OpRect fixed_rect = workplace->GetFixedContentRect();
			LayoutCoord delta_x = new_x - old_x;
			LayoutCoord delta_y = new_y - old_y;

			if (fixed_rect.IsEmpty())
				visual_device->ScrollRect(page_rect, delta_x, delta_y);
			else
			{
				OpRect above(page_rect.x, page_rect.y, page_rect.width, fixed_rect.y - page_rect.y);
				OpRect below(page_rect.x, fixed_rect.Bottom(), page_rect.width, page_rect.height - fixed_rect.Bottom());
				OpRect left(page_rect.x, fixed_rect.y, fixed_rect.x - page_rect.x, fixed_rect.height);
				OpRect right(fixed_rect.Right(), fixed_rect.y, page_rect.width - fixed_rect.Right(), fixed_rect.height);

				if (!above.IsEmpty())
					visual_device->ScrollRect(above, delta_x, delta_y);

				if (!below.IsEmpty())
					visual_device->ScrollRect(below, delta_x, delta_y);

				if (!left.IsEmpty())
					visual_device->ScrollRect(left, delta_x, delta_y);

				if (!right.IsEmpty())
					visual_device->ScrollRect(right, delta_x, delta_y);

				visual_device->Update(fixed_rect.x, fixed_rect.y, fixed_rect.width, fixed_rect.height);
			}
		}
	}

	current_page = page;
	page_offset = offset;

	if (!scroll)
	{
		// Full update.

		OpRect r = GetEdgesInDocumentCoords(OpRect(0, 0, block_width, height));
		visual_device->Update(r.x, r.y, r.width, r.height);
	}
}

/** Report (memory) error. */

void
PagedContainer::RaiseCondition(OP_STATUS error)
{
	if (visual_device)
		visual_device->GetWindow()->RaiseCondition(error);
	else
		g_memory_manager->RaiseCondition(error);
}

/** Split content into pages. */

BOOL
PagedContainer::Paginate(const LayoutInfo& info, const HTMLayoutProperties& props, LayoutCoord& content_height)
{
	/* Reset floats' virtual bottom and pending bottom margin, since we
	   have taken them into account now. */

	reflow_state->float_bottom = LayoutCoord(0);
	reflow_state->float_min_bottom = LayoutCoord(0);
	reflow_state->margin_state.Reset();

	// Calculate min and max height, if any should be set

	LayoutProperties* cascade = placeholder->GetCascade();
	LayoutCoord min_height = props.min_height;
	LayoutCoord max_height = props.max_height >= 0 ? props.max_height : LAYOUT_COORD_HALF_MAX;
	BOOL has_max_height = props.max_height >= 0;

	// Convert to content-box values:

	if (props.box_sizing == CSS_VALUE_border_box)
	{
		LayoutCoord vertical_border_padding = top_border_padding + bottom_border_padding;

		if (min_height != CONTENT_SIZE_AUTO)
		{
			min_height -= vertical_border_padding;

			if (min_height < 0)
				min_height = LayoutCoord(0);
		}

		max_height -= vertical_border_padding;

		if (max_height < 0)
			max_height = LayoutCoord(0);
	}

	LayoutCoord initial_height = reflow_state->css_height;

	OP_ASSERT(initial_height >= 0 || initial_height == CONTENT_HEIGHT_AUTO);

	if (initial_height == CONTENT_HEIGHT_AUTO)
		if (has_max_height)
			initial_height = max_height;

	if (max_height < min_height)
		max_height = min_height;

	LayoutCoord vertical_padding = props.padding_top + props.padding_bottom;

	// Calculate page size.

	page_width = block_width - LayoutCoord(props.border.left.width + props.border.right.width);

	if (initial_height == CONTENT_HEIGHT_AUTO)
		// Cannot determine the page height yet.

		page_height = CONTENT_HEIGHT_AUTO;
	else
		page_height = MIN(max_height, initial_height) + vertical_padding;

	// Distribute content into pages.

	LayoutCoord stride_x(0);
	LayoutCoord stride_y(0);

	if (paged_packed.vertical_pagination)
		stride_y = page_height;
	else
#ifdef SUPPORT_TEXT_DIRECTION
		if (paged_packed.rtl)
			stride_x = -page_width;
		else
#endif // SUPPORT_TEXT_DIRECTION
			stride_x = page_width;

	LayoutCoord max_page_height = max_height;
	BOOL height_restricted = initial_height != CONTENT_HEIGHT_AUTO || props.max_height >= 0;

	if (initial_height != CONTENT_HEIGHT_AUTO && max_page_height > initial_height)
		max_page_height = initial_height;

	LayoutCoord top_offset(paged_packed.is_multicol ? LayoutCoord(0) : top_border_padding);
	LayoutCoord stretched_height = CONTENT_SIZE_AUTO;

	if (cascade->flexbox)
	{
		// Propagate minimum and hypothetical heights and stretch / flex.

		FlexItemBox* item_box = (FlexItemBox*) placeholder;
		LayoutCoord minimum_height(0);
		LayoutCoord border_box_extra =
			LayoutCoord(props.border.top.width + props.border.bottom.width) +
			props.padding_top + props.padding_bottom +
			GetPageControlHeight();

		stretched_height = item_box->GetFlexHeight(cascade);

		if (props.min_height == CONTENT_SIZE_AUTO)
		{
			// Find minimum height, by finding the tallest unbreakable fragment.

			Paginator minheight_paginator(info, LayoutCoord(0), stride_x, stride_y, top_offset, vertical_padding, FALSE);
			LayoutCoord dummy_height(0);

			if (paged_packed.is_multicol)
			{
				PrepareColumnization(info, props, CONTENT_HEIGHT_AUTO);

				if (!RunColumnizer(&minheight_paginator, info, dummy_height, LAYOUT_COORD_HALF_MAX, TRUE, TRUE))
					return FALSE;
			}
			else
			{
				if (!Container::Columnize(minheight_paginator, NULL))
					return FALSE;

				if (!minheight_paginator.Finalize())
					return FALSE;
			}

			PageStack squeezed_pages;

			minheight_paginator.GetAllPages(squeezed_pages);

			for (Column* page = squeezed_pages.GetFirstColumn(); page; page = page->Suc())
				if (minimum_height < page->GetHeight())
					minimum_height = page->GetHeight();

			minimum_height += border_box_extra;
		}

		LayoutCoord maximum_height(reflow_state->css_height);

		if (maximum_height < 0)
		{
			// Auto height. Find maximum height, by only honoring forced page breaks.

			Paginator maxheight_paginator(info, LAYOUT_COORD_HALF_MAX, stride_x, stride_y, top_offset, vertical_padding, FALSE);

			if (paged_packed.is_multicol)
			{
				PrepareColumnization(info, props, CONTENT_HEIGHT_AUTO);

				if (!RunColumnizer(&maxheight_paginator, info, maximum_height, LAYOUT_COORD_HALF_MAX, TRUE, TRUE))
					return FALSE;
			}
			else
			{
				if (!Container::Columnize(maxheight_paginator, NULL))
					return FALSE;

				if (!maxheight_paginator.Finalize())
					return FALSE;

				maximum_height = maxheight_paginator.GetPageHeight();
			}
		}

		maximum_height += border_box_extra;

		item_box->PropagateHeights(minimum_height, maximum_height);

		if (stretched_height != CONTENT_HEIGHT_AUTO)
		{
			max_page_height = stretched_height - border_box_extra;

			if (max_page_height < 0)
				max_page_height = LayoutCoord(0);

			content_height = max_page_height;
		}
	}

	Paginator paginator(info, max_page_height, stride_x, stride_y, top_offset, vertical_padding, height_restricted);

	if (paged_packed.is_multicol)
	{
		PrepareColumnization(info, props, max_page_height);

		if (!RunColumnizer(&paginator, info, content_height, LAYOUT_COORD_HALF_MAX, TRUE, FALSE))
			return FALSE;
	}
	else
		if (!Container::Columnize(paginator, NULL))
			return FALSE;

	if (!paginator.Finalize())
		return FALSE;

	if (page_height == CONTENT_HEIGHT_AUTO)
		page_height = paginator.GetPageHeight() + vertical_padding;

	unsigned int old_page_count = pages.GetColumnCount();

	// Delete pages from previous layout. They will be rebuilt now.

	pages.Clear();

	if (stretched_height == CONTENT_HEIGHT_AUTO)
	{
		/* Unless this is a flex item that has already got its height
		   calculated, calculate it now, based on 'height', 'min-height',
		   'max-height' and resolved auto height. */

		if (reflow_state->css_height == CONTENT_HEIGHT_AUTO)
			if (paginator.ReachedMaxHeight() && has_max_height)
				content_height = max_height;
			else
				content_height = paginator.GetPageHeight();
		else
			content_height = initial_height;

		if (content_height > max_height)
			content_height = max_height;

		if (content_height < min_height)
			content_height = min_height;
	}

	paginator.UpdateBottomFloats(content_height);
	paginator.GetAllPages(pages);

	HTML_Element* html_element = GetHtmlElement();
	unsigned int new_page_count = pages.GetColumnCount();

	if (current_page == UINT_MAX)
		if (html_element->HasSpecialAttr(Markup::LAYOUTA_CURRENT_PAGE, SpecialNs::NS_LAYOUT))
		{
			/* Restore previously stored page number. This is useful when regenerating
			   the box (some sort of heavy reflow), and also for printing. */

			if (!SetCurrentPageNumber(html_element->GetSpecialNumAttr(Markup::LAYOUTA_CURRENT_PAGE, SpecialNs::NS_LAYOUT), FALSE, FALSE))
				return FALSE;
		}
		else
		{
			current_page = 0;

			if (html_element->SetSpecialNumAttr(Markup::LAYOUTA_CURRENT_PAGE, current_page, SpecialNs::NS_LAYOUT) == -1)
				/* This is where we attempt to allocate memory (if needed) to store the
				   current page number. Other places in the code that also need to store
				   it therefore do not need to check for OOM. */

				return FALSE;
		}

	if (new_page_count != old_page_count)
	{
		if (current_page >= new_page_count)
			if (!SetCurrentPageNumber(current_page, FALSE, FALSE)) // Will go to the last page in the container
				return FALSE;

		if (OpStatus::IsMemoryError(info.doc->SignalPageChange(GetHtmlElement(), current_page, new_page_count)))
			return FALSE;
	}

	if (page_control)
	{
		page_control->SetPageCount(new_page_count);
		page_control->SetVertical(paged_packed.vertical_pagination);
	}

	return TRUE;
}

/** Update height of container. */

/* virtual */ void
PagedContainer::UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height)
{
	if (!initial_content_height)
	{
		const HTMLayoutProperties& props = *cascade->GetProps();

		if (!Paginate(info, props, content_height))
		{
			info.hld_profile->SetIsOutOfMemory(TRUE);
			return;
		}

		/* If we need to calculate min/max widths, we need to adjust
		   min_content_height here, because it will be wrong (from the caller)
		   if height was auto. Should be improved at some point. */
	}

	/* Call super-class, but skip MultiColumnContainer; it has been dealt with
	   above, as part of Paginate(). */

	if (GetScrollableContainer())
		ScrollableContainer::UpdateHeight(info, cascade, content_height, min_content_height, initial_content_height);
	else
		if (paged_packed.is_shrink_to_fit)
			ShrinkToFitContainer::UpdateHeight(info, cascade, content_height, min_content_height, initial_content_height);
		else
			BlockContainer::UpdateHeight(info, cascade, content_height, min_content_height, initial_content_height);
}

/** Apply min-width property constraints on min/max values. */

/* virtual */ void
PagedContainer::ApplyMinWidthProperty(const HTMLayoutProperties& props, LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width)
{
	PC_CALL_SUPER(ApplyMinWidthProperty(props, min_width, normal_min_width, max_width));
}

/** Get min/max width. */

/* virtual */ BOOL
PagedContainer::GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const
{
	PC_RETURN_CALL_SUPER(GetMinMaxWidth(min_width, normal_min_width, max_width));
}

/** Finish reflowing box. */

/* virtual */ LAYST
PagedContainer::FinishLayout(LayoutInfo& info)
{
	OP_ASSERT(pending_logical_floats.Empty());
	PC_RETURN_CALL_SUPER(FinishLayout(info));
}

/** Update screen. */

/* virtual */ void
PagedContainer::UpdateScreen(LayoutInfo& info)
{
	PC_CALL_SUPER(UpdateScreen(info));

	if (!placeholder->IsInlineBox())
	{
		/* There's missing code to figure out exactly what parts of a paged
		   container has changed, so we have to update everything. */

		AbsoluteBoundingBox bounding_box;

		placeholder->GetBoundingBox(bounding_box);
		info.visual_device->UpdateRelative(bounding_box.GetX(), bounding_box.GetY(), bounding_box.GetWidth(), bounding_box.GetHeight());
	}
}

/** Update margins, bounding box and start position for the next element in the layout stack. */

/* virtual */ void
PagedContainer::UpdateBottomMargins(const LayoutInfo& info, const VerticalMargin* bottom_margin, AbsoluteBoundingBox* child_bounding_box, BOOL has_inflow_content)
{
	PC_CALL_SUPER(UpdateBottomMargins(info, bottom_margin, NULL, has_inflow_content));
}

/** Update bounding box. */

/* virtual */ void
PagedContainer::UpdateBoundingBox(const AbsoluteBoundingBox& child_bounding_box, BOOL skip_content)
{
	/* Ignore bounding box. It will be wrong anyway. The values propagated
	   are in the "virtual" coordinate system, where there is only one tall
	   page. We care about the virtual bottom, though. */
}

/** Create form, plugin and iframe objects */

/* virtual */ OP_STATUS
PagedContainer::Enable(FramesDocument* doc)
{
	if (!paged_packed.enabled)
	{
		visual_device = doc->GetVisualDevice();

		if (!IsLayoutRoot())
			RETURN_IF_ERROR(InitCoreView(visual_device));

		paged_packed.enabled = 1;

#ifdef RESERVED_REGIONS
		if (LayoutWorkplace* workplace = doc->GetLayoutWorkplace())
			workplace->OnReservedRegionBoxAdded();
#endif // RESERVED_REGIONS
	}

	return OpStatus::OK;
}

/** Remove form, plugin and iframe objects */

/* virtual */ void
PagedContainer::Disable(FramesDocument* doc)
{
	SetParentInputContext(NULL);

	if (doc->IsUndisplaying() || doc->IsBeingDeleted())
		/* We must destruct CoreView here so we get it out from the
		   hierarchy since our layout boxes may live longer than its
		   parent. */

		if (CoreView::m_parent)
			CoreView::Destruct();

	if (page_control)
		page_control->SetVisualDevice(NULL);

	visual_device = NULL;

	if (paged_packed.enabled)
	{
#ifdef RESERVED_REGIONS
		if (LayoutWorkplace* workplace = doc->GetLayoutWorkplace())
			workplace->OnReservedRegionBoxRemoved();
#endif // RESERVED_REGIONS

		paged_packed.enabled = 0;
	}
}

/** Calculate the width of the containing block established by this container. */

/* virtual */ LayoutCoord
PagedContainer::CalculateContentWidth(const HTMLayoutProperties& props, ContainerWidthType width_type) const
{
	PC_RETURN_CALL_SUPER(CalculateContentWidth(props, width_type));
}

/** Attempt to break page. */

/* virtual */ BREAKST
PagedContainer::AttemptPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain)
{
	OP_ASSERT(!"Page breaking should be handled by the columnizer");

	return BREAK_NOT_FOUND;
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
PagedContainer::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	HTML_ElementType elm_type = cascade->html_element->Type();
	CSSValue overflow;

	if (IsLayoutRoot())
	{
		overflow = CSSValue(info.workplace->GetViewportOverflowX());
#ifdef SUPPORT_TEXT_DIRECTION
		paged_packed.rtl = info.workplace->IsRtlDocument();
#endif // SUPPORT_TEXT_DIRECTION
	}
	else
	{
		overflow = CSSValue(props.overflow_x);
#ifdef SUPPORT_TEXT_DIRECTION
		paged_packed.rtl = props.direction == CSS_VALUE_rtl;
#endif // SUPPORT_TEXT_DIRECTION
	}

	paged_packed.is_multicol = props.IsMultiColumn(elm_type);
	paged_packed.is_shrink_to_fit = props.IsShrinkToFit(elm_type) && !IsLayoutRoot();
	paged_packed.vertical_pagination = overflow == CSS_VALUE__o_paged_y || overflow == CSS_VALUE__o_paged_y_controls;

	if ((overflow == CSS_VALUE__o_paged_x_controls || overflow == CSS_VALUE__o_paged_y_controls) &&
		(!IsLayoutRoot() || g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowPageControls)))
	{
		if (!page_control)
		{
			RETURN_VALUE_IF_ERROR(OpPageControl::Construct(&page_control), LAYOUT_OUT_OF_MEMORY);
			REPORT_MEMMAN_INC(sizeof(OpPageControl));
			page_control->SetPageControlListener(this);
		}
	}
	else
		if (page_control)
		{
			page_control->Delete();
			page_control = NULL;
			REPORT_MEMMAN_DEC(sizeof(OpPageControl));
		}

	if (page_control)
	{
		LayoutCoord page_control_height(page_control->GetPreferredHeight());
		LayoutCoord css_height = CalculateCSSHeight(info, cascade);

		if (css_height != CONTENT_HEIGHT_AUTO && !IsLayoutRoot())
			if (page_control_height * 5 > css_height)
				// Make the control widget slightly smaller; there isn't much height available here.

				if (page_control_height * 2 > css_height)
					// Really, really not much height available. Hide the control widget completely.

					page_control_height = LayoutCoord(0);
				else
					page_control_height = css_height / LayoutCoord(5);

		page_control->SetSize(page_control->GetWidth(), page_control_height);

#ifdef SUPPORT_TEXT_DIRECTION
		page_control->SetRTL(paged_packed.rtl);
#endif // SUPPORT_TEXT_DIRECTION
	}

	PC_RETURN_CALL_SUPER(ComputeSize(cascade, info));
}

/** Lay out box. */

/* virtual */ LAYST
PagedContainer::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	top_border_padding = LayoutCoord(props.border.top.width) + props.padding_top;
	bottom_border_padding = LayoutCoord(props.border.bottom.width) + props.padding_bottom;

	if (!g_main_message_handler->HasCallBack(this, MSG_SLIDE_ANIMATION, (INTPTR) this))
		if (OpStatus::IsMemoryError(g_main_message_handler->SetCallBack(this, MSG_SLIDE_ANIMATION, (INTPTR) this)))
			return LAYOUT_OUT_OF_MEMORY;

#ifdef _PRINT_SUPPORT_
	if (!(info.visual_device->IsPrinter() || info.visual_device->GetDocumentManager()->GetPrintPreviewVD()))
#endif // _PRINT_SUPPORT_
		RETURN_VALUE_IF_ERROR(Enable(info.doc), LAYOUT_OUT_OF_MEMORY);

	if (!GetMultiColumnContainer())
	{
		/** Move all GCPM floats (from previous layout passes) to a temporary
			list. Unless they get deleted during layout, they will be taken out
			of that list and put back into the regular list when laid out or
			when some ancestor of theirs decide to skip layout of its
			subtree. */

		pending_logical_floats.Append(&logical_floats);
		logical_floats.RemoveAll();
	}

	PC_RETURN_CALL_SUPER(Layout(cascade, info, first_child, start_position));
}

/** Get baseline of first line. */

/* virtual */ LayoutCoord
PagedContainer::GetBaseline(const HTMLayoutProperties& props) const
{
	if (placeholder->IsInlineBox())
		return height + props.margin_bottom;
	else
		return ShrinkToFitContainer::GetBaseline(props);
}

/** Signal that content may have changed. */

/* virtual */ void
PagedContainer::SignalChange(FramesDocument* doc)
{
	PC_CALL_SUPER(SignalChange(doc));
}

/** Traverse box. */

/* virtual */ void
PagedContainer::Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	TraverseInfo traverse_info;
	LayoutCoord translate_x;
	LayoutCoord translate_y;

	CalculateTranslation(current_page, page_offset, translate_x, translate_y);

	OP_ASSERT(CoreView::m_parent || traversal_object->GetDocument()->IsPrintDocument() || IsLayoutRoot());

	if (traversal_object->EnterPagedContainer(layout_props, this, page_width, page_height, traverse_info))
	{
		BOOL is_positioned = placeholder->IsPositionedBox();

		traversal_object->Translate(translate_x, translate_y);

		if (is_positioned)
			traversal_object->TranslateRoot(translate_x, translate_y);

		TraversePages(traversal_object, layout_props);

		if (is_positioned)
			traversal_object->TranslateRoot(-translate_x, -translate_y);

		traversal_object->Translate(-translate_x, -translate_y);

		traversal_object->LeavePagedContainer(layout_props, this, traverse_info);
	}
}

/** Traverse all pages. */

void
PagedContainer::TraversePages(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	LayoutCoord old_pane_x_tr;
	LayoutCoord old_pane_y_tr;

	traversal_object->GetPaneTranslation(old_pane_x_tr, old_pane_y_tr);

	TargetTraverseState target_traversal;
	TraverseInfo traverse_info;
	Column* outer_pane = traversal_object->GetCurrentPane();
	LayoutCoord outer_start_offset = traversal_object->GetPaneStartOffset();
	LayoutCoord translate_y = top_border_padding;
	BOOL is_positioned = placeholder->IsPositionedBox();
	BOOL target_done = FALSE;

	traversal_object->StoreTargetTraverseState(target_traversal);
	traversal_object->Translate(LayoutCoord(0), translate_y);

	for (Column* page = pages.GetFirstColumn(); page; page = page->Suc())
	{
		// Set up the traversal object for page traversal.

		traversal_object->SetCurrentPane(page);
		traversal_object->SetPaneDone(FALSE);
		traversal_object->SetPaneStartOffset(page->GetStartOffset());
		traversal_object->SetPaneStarted(FALSE);

		/* Restore target traversal state before traversing this page. The
		   target element may live in multiple pages. */

		traversal_object->RestoreTargetTraverseState(target_traversal);

		traversal_object->Translate(page->GetX(), page->GetY());

		if (is_positioned)
			traversal_object->TranslateRoot(page->GetX(), page->GetY());

		traversal_object->SetPaneTranslation(traversal_object->GetTranslationX(), traversal_object->GetTranslationY());
		traversal_object->SetPaneClipRect(OpRect(SHRT_MIN / 2, page->GetTopFloatsHeight(), SHRT_MAX, page->GetHeight()));

		if (traversal_object->EnterPane(layout_props, page, FALSE, traverse_info))
		{
			traversal_object->Translate(LayoutCoord(0), page->GetTranslationY());

			if (paged_packed.is_multicol)
			{
				traversal_object->EnterMultiColumnContainer(layout_props, this);
				MultiColumnContainer::TraverseRows(traversal_object, layout_props);
			}
			else
				Container::Traverse(traversal_object, layout_props);

			traversal_object->Translate(LayoutCoord(0), -page->GetTranslationY());
			traversal_object->LeavePane(traverse_info);
		}

		if (is_positioned)
			traversal_object->TranslateRoot(-page->GetX(), -page->GetY());

		traversal_object->Translate(-page->GetX(), -page->GetY());

		if (!target_done)
			target_done = traversal_object->IsTargetDone();
	}

	traversal_object->Translate(LayoutCoord(0), -translate_y);

	if (target_done)
		// We found the target while traversing one or more of these pages

		traversal_object->SwitchTarget(layout_props->html_element);

	traversal_object->SetCurrentPane(outer_pane);
	traversal_object->SetPaneStartOffset(outer_start_offset);
	traversal_object->SetPaneDone(FALSE);

	if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
		/* Schedule GCPM floats in this paged container for traversal. Note
		   that there may already be floats in previous multi-pane
		   containers scheduled, and likewise, there may be multi-pane
		   containers after us that have their own floats to schedule. */

		traversal_object->AddPaneFloats(logical_floats);

	traversal_object->SetPaneTranslation(old_pane_x_tr, old_pane_y_tr);
}

/** Get scroll offset, if applicable for this type of box / content. */

/* virtual */ OpPoint
PagedContainer::GetScrollOffset()
{
	return OpPoint(GetViewX(), GetViewY());
}

/** Set scroll offset, if applicable for this type of box / content. */

/* virtual */ void
PagedContainer::SetScrollOffset(int* x, int* y)
{
	OpRect rect(x ? *x : GetViewX(), y ? *y : GetViewY(), 1, 1);
	unsigned int page;
	unsigned int dummy;

	GetPagesContaining(rect, page, dummy);
	SetCurrentPageNumber(page, TRUE, TRUE);
}

/** Get the border area (optionally) and translation for 'box'. */

/* virtual */ void
PagedContainer::GetAreaAndPaneTranslation(const Box* box, LayoutCoord& x, LayoutCoord& y, RECT* border_rect, ColumnFinder* ancestor_cf)
{
	x = LayoutCoord(0);
	y = LayoutCoord(0);

	if (border_rect)
	{
		border_rect->left = 0;
		border_rect->top = 0;
		border_rect->right = 0;
		border_rect->bottom = 0;
	}

	if (!pages.GetFirstColumn())
		return;

	LayoutCoord top_offset(paged_packed.is_multicol ? LayoutCoord(0) : top_border_padding);
	ColumnFinder cf(box, &pages, top_offset, ancestor_cf);

	if (paged_packed.is_multicol)
	{
		LayoutCoord mc_x;
		LayoutCoord mc_y;

		MultiColumnContainer::GetAreaAndPaneTranslation(box, mc_x, mc_y, border_rect, &cf);
		cf.SetDescendantTranslation(mc_x, mc_y);
	}
	else
	{
		Container::FindColumn(cf);

		if (border_rect)
			*border_rect = cf.GetBorderRect();
	}

	OP_ASSERT(cf.IsBoxEndFound());

	x = cf.GetBoxTranslationX() + cf.GetDescendantTranslationX();
	y = top_offset + cf.GetBoxTranslationY() + cf.GetDescendantTranslationY();
}

/** Return TRUE if this paged container has a column in which the specified float lives. */

BOOL
PagedContainer::HasPaneFloat(FloatedPaneBox* box)
{
	/* This may be considered unnecessarily expensive. If there was only a way
	   to ask the box directly where it belongs, we could answer the question
	   right away. */

	for (Column* page = pages.GetFirstColumn(); page; page = page->Suc())
		if (page->HasFloat(box))
			return TRUE;

	return FALSE;
}

/** Return TRUE if this element can be split into multiple outer columns or pages. */

/* virtual */ BOOL
PagedContainer::IsColumnizable() const
{
	return FALSE;
}

/** Handle input action. */

OP_BOOLEAN
PagedContainer::HandleInputAction(OpInputAction* input_action)
{
	if (!visual_device)
		// No visual device in print preview mode.

		return OpBoolean::IS_FALSE;

	FramesDocument* doc = visual_device->GetDocumentManager()->GetCurrentDoc();

	if (!doc)
		return OpBoolean::IS_FALSE;

	OpInputAction::Action action = input_action->GetAction();

	switch (action)
	{
	case OpInputAction::ACTION_PAGE_LEFT:
	case OpInputAction::ACTION_PAGE_RIGHT:
	case OpInputAction::ACTION_SCROLL_LEFT:
	case OpInputAction::ACTION_SCROLL_RIGHT:
	case OpInputAction::ACTION_PAGE_UP:
	case OpInputAction::ACTION_PAGE_DOWN:
	case OpInputAction::ACTION_SCROLL_UP:
	case OpInputAction::ACTION_SCROLL_DOWN:
	{
		// Disallow invalid directions.

		if (paged_packed.vertical_pagination)
			switch (action)
			{
			case OpInputAction::ACTION_PAGE_LEFT:
			case OpInputAction::ACTION_PAGE_RIGHT:
			case OpInputAction::ACTION_SCROLL_LEFT:
			case OpInputAction::ACTION_SCROLL_RIGHT:
				// Don't handle horizontal movement in vertical pagination.

				return OpBoolean::IS_FALSE;
			}
		else
			switch (action)
			{
			case OpInputAction::ACTION_SCROLL_UP:
			case OpInputAction::ACTION_SCROLL_DOWN:
				/* Don't handle vertical movement in horizontal pagination,
				   except for PgUp and PgDown */

				return OpBoolean::IS_FALSE;
			}

		OpRect vv = doc->GetVisualViewport();
		RECT rect;

		if (!placeholder->GetRect(doc, BORDER_BOX, rect))
			return OpBoolean::IS_FALSE;

		OpRect box_rect(&rect);

		BOOL next_page;

#ifdef SUPPORT_TEXT_DIRECTION
		if (paged_packed.rtl)
			next_page =
				action == OpInputAction::ACTION_PAGE_LEFT ||
				action == OpInputAction::ACTION_PAGE_DOWN ||
				action == OpInputAction::ACTION_SCROLL_LEFT ||
				action == OpInputAction::ACTION_SCROLL_DOWN;
		else
#endif // SUPPORT_TEXT_DIRECTION
			next_page =
				action == OpInputAction::ACTION_PAGE_RIGHT ||
				action == OpInputAction::ACTION_PAGE_DOWN ||
				action == OpInputAction::ACTION_SCROLL_RIGHT ||
				action == OpInputAction::ACTION_SCROLL_DOWN;

		/* Set new page number, and make sure that the beginning (if moving to
		   the next page) or end (if moving to the previous page) is within the
		   visual viewport. */

		if (next_page)
		{
			if (current_page + 1 < (unsigned int) pages.GetColumnCount())
			{
#ifdef SUPPORT_TEXT_DIRECTION
				if (paged_packed.rtl)
				{
					if (vv.Top() > box_rect.Top() || vv.Right() < box_rect.Right())
						doc->RequestSetVisualViewport(OpRect(box_rect.Right() - vv.width, box_rect.Top(), vv.width, vv.height), VIEWPORT_CHANGE_REASON_INPUT_ACTION);
				}
				else
#endif // SUPPORT_TEXT_DIRECTION
					if (vv.Top() > box_rect.Top() || vv.Left() > box_rect.Left())
						doc->RequestSetVisualViewport(OpRect(box_rect.Left(), box_rect.Top(), vv.width, vv.height), VIEWPORT_CHANGE_REASON_INPUT_ACTION);

				if (!SetCurrentPageNumber(current_page + 1, TRUE))
					return OpStatus::ERR_NO_MEMORY;

				return OpBoolean::IS_TRUE;
			}
		}
		else
			if (current_page > 0)
			{
#ifdef SUPPORT_TEXT_DIRECTION
				if (paged_packed.rtl)
				{
					if (vv.Bottom() < box_rect.Bottom() || vv.Left() > box_rect.Left())
						doc->RequestSetVisualViewport(OpRect(box_rect.Left(), box_rect.Bottom() - vv.height, vv.width, vv.height), VIEWPORT_CHANGE_REASON_INPUT_ACTION);
				}
				else
#endif // SUPPORT_TEXT_DIRECTION
					if (vv.Bottom() < box_rect.Bottom() || vv.Right() < box_rect.Right())
						doc->RequestSetVisualViewport(OpRect(box_rect.Right() - vv.width, box_rect.Bottom() - vv.height, vv.width, vv.height), VIEWPORT_CHANGE_REASON_INPUT_ACTION);

				if (!SetCurrentPageNumber(current_page - 1, TRUE))
					return OpStatus::ERR_NO_MEMORY;

				return OpBoolean::IS_TRUE;
			}

		return OpBoolean::IS_FALSE;
	}

	case OpInputAction::ACTION_GO_TO_START:
		if (current_page == 0)
			return OpBoolean::IS_FALSE;

		if (!SetCurrentPageNumber(0))
			return OpStatus::ERR_NO_MEMORY;

		return OpBoolean::IS_TRUE;

	case OpInputAction::ACTION_GO_TO_END:
		if (current_page + 1 >= (unsigned int) pages.GetColumnCount())
			return OpBoolean::IS_FALSE;

		if (!SetCurrentPageNumber(UINT_MAX))
			return OpStatus::ERR_NO_MEMORY;

		return OpBoolean::IS_TRUE;

#if defined PAN_SUPPORT && defined ACTION_COMPOSITE_PAN_ENABLED && defined ACTION_COMPOSITE_PAN_STOP_ENABLED
	case OpInputAction::ACTION_COMPOSITE_PAN_STOP:
	{
		paged_packed.pan_decision_made = 0;
		paged_packed.pending_panning = 0;
		paged_packed.orthogonal_panning = 0;

		if (paged_packed.block_panning)
		{
			paged_packed.block_panning = 0;
			return OpBoolean::IS_FALSE;
		}

		unsigned int new_page = current_page;
		int page_size = paged_packed.vertical_pagination ? page_height : page_width;

		if (op_abs(page_offset) > page_size * PAGE_DRAG_CHANGE_THRESHOLD / 100)
			if (page_offset > 0)
				new_page ++;
			else
				if (new_page > 0)
					new_page --;

		if (!SetCurrentPageNumber(new_page, TRUE))
			return OpStatus::ERR_NO_MEMORY;

		return OpBoolean::IS_TRUE;
	}

	case OpInputAction::ACTION_COMPOSITE_PAN:
	{
		INT16* delta = (INT16*)input_action->GetActionData();
		OpPoint origin(0, 0);
		OpPoint diff(delta[0], delta[1]);

		GetPosInDocument().ApplyInverse(diff);
		GetPosInDocument().ApplyInverse(origin);
		diff -= origin;

		if (!paged_packed.pan_decision_made)
		{
			/* Propagate pan action to parent if the paged container isn't
			   fully within view and if propgatating it to the parent would
			   indeed help on the situation. */

			OpRect visual_viewport = doc->GetVisualViewport();
			OpRect rect(0, 0, block_width, height);

			GetPosInDocument().Apply(rect);

			if (diff.x != 0)
				if (diff.x < 0)
				{
					if (rect.x >= 0 && rect.x < visual_viewport.x)
						return OpBoolean::IS_FALSE;
				}
				else
					if (rect.Right() > visual_viewport.Right())
						return OpBoolean::IS_FALSE;

			if (diff.y != 0)
				if (diff.y < 0)
				{
					if (rect.y >= 0 && rect.y < visual_viewport.y)
						return OpBoolean::IS_FALSE;
				}
				else
					if (rect.Bottom() > visual_viewport.Bottom())
						return OpBoolean::IS_FALSE;
		}

		LayoutCoord new_offset = paged_packed.pan_decision_made ? page_offset : LayoutCoord(paged_packed.pending_panning);

#ifdef SUPPORT_TEXT_DIRECTION
		if (paged_packed.rtl && !paged_packed.vertical_pagination)
			new_offset -= LayoutCoord(diff.x);
		else
#endif // SUPPORT_TEXT_DIRECTION
			new_offset += LayoutCoord(paged_packed.vertical_pagination ? diff.y : diff.x);

		LayoutCoord new_orthogonal_offset = LayoutCoord(paged_packed.orthogonal_panning);

		new_orthogonal_offset += LayoutCoord(paged_packed.vertical_pagination ? diff.x : diff.y);

		if (!paged_packed.pan_decision_made)
			/* The amount of panning has so far been so low that we haven't yet
			   decided if we pan the paged container or not. */

			if (op_abs(new_offset) >= PAGE_PROPAGATE_PAN_THRESHOLD)
			{
				// Sufficient panning along the axis of pagination direction.

				if (!paged_packed.pan_decision_made)
					if (new_offset < 0)
					{
						if (current_page == 0)
							/* Trying to go to the page preceding the first page. Nonsense.
							   Block paged container panning, and let the parent input context
							   handle the pan action instead. */

							paged_packed.block_panning = 1;
					}
					else
						if (new_offset > 0)
							if (current_page == (unsigned int) pages.GetColumnCount() - 1)
								/* Trying to go to the page following the last page. Nonsense.
								   Block paged container panning, and let the parent input
								   context handle the pan action instead. */

								paged_packed.block_panning = 1;

				paged_packed.pan_decision_made = 1;
			}
			else
				if (op_abs(new_orthogonal_offset) >= PAGE_PROPAGATE_PAN_THRESHOLD)
				{
					/* Too much motion along the axis opposite of the pagination
					   direction. Block paged container panning, and let the parent
					   input context handle the pan action instead. */

					paged_packed.block_panning = 1;
					paged_packed.pan_decision_made = 1;
				}
				else
					paged_packed.orthogonal_panning = new_orthogonal_offset;

		if (!paged_packed.block_panning)
			if (paged_packed.pan_decision_made)
			{
				ScrollTo(current_page, new_offset);
				RETURN_IF_ERROR(doc->HandleEvent(ONSCROLL, NULL, GetHtmlElement(), SHIFTKEY_NONE));
			}
			else
				paged_packed.pending_panning = new_offset;

		return paged_packed.block_panning ? OpBoolean::IS_FALSE : OpBoolean::IS_TRUE;
	}
#endif // PAN_SUPPORT && ACTION_COMPOSITE_PAN_ENABLED
	}

	return OpBoolean::IS_FALSE;
}

/* BEGIN implement the CoreView interface */

/* virtual */ BOOL
PagedContainer::OnInputAction(OpInputAction* input_action)
{
	OP_BOOLEAN handled = HandleInputAction(input_action);

	if (OpStatus::IsError(handled))
		RaiseCondition(handled);

	return handled == OpBoolean::IS_TRUE;
}

/* virtual */ BOOL
PagedContainer::GetBoundingRect(OpRect &rect)
{
	return FALSE;
}

/* END implement the CoreView interface */

/* BEGIN implement the OpPageControlListener interface */

/* virtual */ void
PagedContainer::OnPageChange(unsigned int page)
{
	if (!SetCurrentPageNumber(page, TRUE))
		RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

/* END implement the OpPageControlListener interface */

/* BEGIN implement the MessageObject interface */

/* virtual */ void
PagedContainer::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	SlideOneFrame();
}

/* END implement the MessageObject interface */

/** Set new page number. */

BOOL
PagedContainer::SetCurrentPageNumber(unsigned int page, BOOL allow_slide, BOOL trigger_event)
{
	if (!visual_device)
		return TRUE;

	FramesDocument* doc = visual_device->GetDocumentManager()->GetCurrentDoc();

	if (!doc)
		return TRUE;

	unsigned int page_count = pages.GetColumnCount();
	unsigned int new_page = page;

	if (new_page > 0 && new_page >= page_count)
		new_page = page_count - 1;

	paged_packed.trigger_scroll_event_on_slide = !!trigger_event;

	if (current_page != new_page || page_offset)
	{
		HTML_Element* html_element = GetHtmlElement();

		if (allow_slide && g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::SmoothScrolling))
		{
			LayoutCoord page_size = paged_packed.vertical_pagination ? page_height : page_width;

			slide_start_offset = LayoutCoord(current_page - new_page) * page_size + page_offset;
			ScrollTo(new_page, slide_start_offset);
			slide_start_time = g_op_time_info->GetRuntimeMS() - PAGE_SLIDE_FRAME_DELAY;
			SlideOneFrame();
		}
		else
		{
			ScrollTo(new_page, LayoutCoord(0));

			if (trigger_event)
				if (OpStatus::IsMemoryError(doc->HandleEvent(ONSCROLL, NULL, html_element, SHIFTKEY_NONE)))
					return FALSE;
		}

		html_element->SetSpecialNumAttr(Markup::LAYOUTA_CURRENT_PAGE, current_page, SpecialNs::NS_LAYOUT);

		if (page_control)
			page_control->SetPage(new_page);

		if (trigger_event)
			if (OpStatus::IsMemoryError(doc->SignalPageChange(html_element, current_page, page_count)))
				return FALSE;
	}

	return TRUE;
}

/** Get the page(s) containing the specified rectangle. */

void
PagedContainer::GetPagesContaining(const OpRect& rect, unsigned int& first_page, unsigned int& last_page)
{
	first_page = current_page;
	last_page = current_page;

	int page_size = paged_packed.vertical_pagination ? page_height : page_width;

	if (!page_size)
		return;

	OpRect abs_rect(rect);
	int translation = current_page * page_size + page_offset;

	if (paged_packed.vertical_pagination)
	{
		abs_rect.y += translation;

		if (abs_rect.Top() <= 0)
			first_page = 0;
		else
			first_page = abs_rect.Top() / page_size;

		if (abs_rect.Bottom() - 1 <= 0)
			last_page = 0;
		else
			last_page = (abs_rect.Bottom() - 1) / page_size;
	}
	else
	{
#ifdef SUPPORT_TEXT_DIRECTION
		if (paged_packed.rtl)
		{
			abs_rect.x -= translation;

			if (abs_rect.Right() - 1 >= 0)
				first_page = 0;
			else
				first_page = (-abs_rect.Right() - 1) / page_size + 1;

			if (abs_rect.Left() > 0)
				last_page = 0;
			else
				last_page = -abs_rect.Left() / page_size + 1;
		}
		else
#endif // SUPPORT_TEXT_DIRECTION
		{
			abs_rect.x += translation;

			if (abs_rect.Left() <= 0)
				first_page = 0;
			else
				first_page = abs_rect.Left() / page_size;

			if (abs_rect.Right() - 1 <= 0)
				last_page = 0;
			else
				last_page = (abs_rect.Right() - 1) / page_size;
		}
	}
}

/** Get the horizontal scroll position for the current page. */

LayoutCoord
PagedContainer::GetViewX() const
{
	if (paged_packed.vertical_pagination)
		return LayoutCoord(0);
	else
#ifdef SUPPORT_TEXT_DIRECTION
		if (paged_packed.rtl)
			return -(LayoutCoord(current_page) * page_width + page_offset);
		else
#endif // SUPPORT_TEXT_DIRECTION
			return LayoutCoord(current_page) * page_width + page_offset;
}

/** Get the horizontal scroll position for the specified page. */

LayoutCoord
PagedContainer::GetViewX(unsigned int page) const
{
	if (paged_packed.vertical_pagination)
		return LayoutCoord(0);
	else
#ifdef SUPPORT_TEXT_DIRECTION
		if (paged_packed.rtl)
			return -(LayoutCoord(page) * page_width);
		else
#endif // SUPPORT_TEXT_DIRECTION
			return LayoutCoord(page) * page_width;
}

/** Get the vertical scroll position for the current page. */

LayoutCoord
PagedContainer::GetViewY() const
{
	if (paged_packed.vertical_pagination)
		return LayoutCoord(current_page) * page_height + page_offset;
	else
		return LayoutCoord(0);
}

/** Get the vertical scroll position for the specified page. */

LayoutCoord
PagedContainer::GetViewY(unsigned int page) const
{
	if (paged_packed.vertical_pagination)
		return LayoutCoord(page) * page_height;
	else
		return LayoutCoord(0);
}

/** Get total width of the scrollable padding box area. */

LayoutCoord
PagedContainer::GetScrollPaddingBoxWidth() const
{
	if (paged_packed.vertical_pagination)
		return page_width;
	else
		return page_width * LayoutCoord(pages.GetColumnCount());
}

/** Get total height of the scrollable padding box area. */

LayoutCoord
PagedContainer::GetScrollPaddingBoxHeight() const
{
	if (paged_packed.vertical_pagination)
		return page_height * LayoutCoord(pages.GetColumnCount());
	else
		return page_height;
}

/** Paint the page control widget. */

void
PagedContainer::PaintPageControl(const HTMLayoutProperties& props, VisualDevice* visual_device)
{
	SetPosition(visual_device->GetCTM());

	if (!page_control)
		return;

	page_control->SetVisualDevice(visual_device);

	LayoutCoord page_control_height = LayoutCoord(page_control->GetHeight());

	/* Make rectangle relative to content box, since HandleEvent() works on
	   such coordinates. */

	OpRect rect(-props.padding_left,
				height - props.border.bottom.width - page_control_height - props.border.top.width - props.padding_top,
				block_width - props.border.left.width - props.border.right.width,
				page_control_height);

	AffinePos ctm = GetPosInDocument();
	LayoutCoord x(props.border.left.width);
	LayoutCoord y = height - LayoutCoord(props.border.bottom.width) - page_control_height;

	ctm.AppendTranslation(props.border.left.width,
						  height - props.border.bottom.width - page_control_height);
	page_control->SetPosInDocument(ctm);
	page_control->SetRect(rect);

	// FIXME: if this is the root container, scaling should be ignored here.

	visual_device->Translate(x, y);
	page_control->GenerateOnPaint(page_control->GetBounds());
	visual_device->Translate(-x, -y);
}

/** Get page control height. */

LayoutCoord
PagedContainer::GetPageControlHeight() const
{
	if (page_control)
		return LayoutCoord(page_control->GetHeight());
	else
		return LayoutCoord(0);
}

/** Return TRUE if the specified coordinates are within the page control widget. */

BOOL
PagedContainer::IsOverPageControl(int x, int y)
{
	if (!page_control)
		return FALSE;

	return page_control->GetRect().Contains(OpPoint(x, y));
}

/** Handle event that occurred on this element. */

/* virtual */ void
PagedContainer::HandleEvent(FramesDocument* doc, DOM_EventType event, int offset_x, int offset_y)
{
#ifndef MOUSELESS
	if (!page_control)
		return;

	if (!IsOverPageControl(offset_x, offset_y))
	{
		page_control->GenerateOnMouseLeave();
		return;
	}

	OpRect rect(page_control->GetRect());
	OpPoint point(offset_x - rect.x, offset_y - rect.y);

	switch (event)
	{
	case ONMOUSEDOWN:
		page_control->GenerateOnMouseDown(point, current_mouse_button, 1);
		break;

	case ONMOUSEUP:
		page_control->GenerateOnMouseUp(point, current_mouse_button, 1);
		break;

	case ONMOUSEMOVE:
		page_control->GenerateOnMouseMove(point);
		break;

	case ONMOUSELEAVE:
		page_control->GenerateOnMouseLeave();
		break;
	}
#endif // !MOUSELESS
}

/** Update height of container. */

/* virtual */ void
RootPagedContainer::UpdateHeight(const LayoutInfo& info, LayoutProperties* cascade, LayoutCoord content_height, LayoutCoord min_content_height, BOOL initial_content_height)
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	if (!initial_content_height)
		if (!Paginate(info, props, content_height))
			info.hld_profile->SetIsOutOfMemory(TRUE);

	height = reflow_state->css_height + props.padding_top + props.padding_bottom + GetPageControlHeight();
}

/** Compute size of content and return TRUE if size is changed. */

/* virtual */ OP_BOOLEAN
RootPagedContainer::ComputeSize(LayoutProperties* cascade, LayoutInfo& info)
{
	OP_BOOLEAN changed = PagedContainer::ComputeSize(cascade, info);

	RETURN_IF_ERROR(changed);

	return OpBoolean::IS_TRUE;
}

/** Signal that content may have changed. */

/* virtual */ void
RootPagedContainer::SignalChange(FramesDocument* doc)
{
	short overflow = doc->GetLogicalDocument()->GetLayoutWorkplace()->GetViewportOverflowX();

	if (!HTMLayoutProperties::IsPagedOverflowValue(overflow))
		placeholder->MarkDirty(doc, FALSE);
}

#endif // PAGED_MEDIA_SUPPORT
