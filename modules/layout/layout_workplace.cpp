/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** @file layout_workplace.cpp
 *
 * Implementation of LayoutWorkplace class and friends
 *
 */

#include "core/pch.h"

#include "modules/layout/layout_workplace.h"

#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/viewportcontroller.h"
#include "modules/dochand/win.h"
#include "modules/display/prn_dev.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/layout/cssprops.h"
#include "modules/layout/layoutprops.h"
#include "modules/layout/box/box.h"
#include "modules/layout/box/tables.h"
#include "modules/layout/content/content.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/selection.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/display/prn_info.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/style/css_matchcontext.h"
#include "modules/style/css_animations.h"

#include "modules/pi/OpScreenInfo.h"

#ifdef PAGED_MEDIA_SUPPORT
# include "modules/doc/pagedescription.h"
#endif // PAGED_MEDIA_SUPPORT

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
#endif

#ifdef SCOPE_PROFILER
# include "modules/probetools/probetimeline.h"
#endif // SCOPE_PROFILER

void
DocRootProperties::DereferenceDeclarations()
{
	CSS_decl::Unref(bg_images_cp);
	CSS_decl::Unref(bg_repeats_cp);
	CSS_decl::Unref(bg_attachs_cp);
	CSS_decl::Unref(bg_positions_cp);
	CSS_decl::Unref(bg_origins_cp);
	CSS_decl::Unref(bg_clips_cp);
	CSS_decl::Unref(bg_sizes_cp);

	bg_images_cp = 0;
	bg_repeats_cp = 0;
	bg_attachs_cp = 0;
	bg_positions_cp = 0;
	bg_origins_cp = 0;
	bg_clips_cp = 0;
	bg_sizes_cp = 0;
}

void
DocRootProperties::Reset()
{
	body_font_size = LayoutFixed(0);
	body_font_color = USE_DEFAULT_COLOR;
	border.Reset();
	bg_color = USE_DEFAULT_COLOR;
	bg_images.Reset();
	bg_element_type = Markup::HTE_DOC_ROOT;
	bg_element_changed = FALSE;

	DereferenceDeclarations();
}

void
DocRootProperties::SetBackgroundImages(const BgImages& bg)
{
	DereferenceDeclarations();

	CSS_decl::Ref(bg_images_cp = bg.GetImages());
	CSS_decl::Ref(bg_repeats_cp = bg.GetRepeats());
	CSS_decl::Ref(bg_attachs_cp = bg.GetAttachs());
	CSS_decl::Ref(bg_positions_cp = bg.GetPositions());
	CSS_decl::Ref(bg_origins_cp = bg.GetOrigins());
	CSS_decl::Ref(bg_clips_cp = bg.GetClips());
	CSS_decl::Ref(bg_sizes_cp = bg.GetSizes());

	bg_images.SetImages(bg_images_cp);
	bg_images.SetRepeats(bg_repeats_cp);
	bg_images.SetAttachs(bg_attachs_cp);
	bg_images.SetPositions(bg_positions_cp);
	bg_images.SetOrigins(bg_origins_cp);
	bg_images.SetClips(bg_clips_cp);
	bg_images.SetSizes(bg_sizes_cp);
}

#ifdef _PRINT_SUPPORT_

/** Class used when temporarily switching to print versions of various pointers.

	Our printing "design" sucks.

	When printing, we clone DOM, layout and FramesDocument, while LogicalDocument
	(and, consecutively, HLDocProfile, CSSCollection and LayoutWorkplace) objects
	remain shared between print and screen. As a result of this, we need to
	temporarily change FramesDocument and VisualDevice pointers in various objects
	when printing. When starting to print, we generate print copies of all
	FramesDocument objects in the window, and also clone DOM and layout (while we
	share the LogicalDocument object, as previously mentioned). The FramesDocument
	pointer in LayoutWorkplace objects is sanely enough changed to the print copy at
	this very early point, and not changed back to the screen document until printing
	has finished. The FramesDocument and VisualDevice pointers in HLDocProfile, on
	the other hand, are kept unchanged (screen document and visual device) until
	right before we start laying out boxes, and then changed to the print versions
	while we lay out boxes, and then changed back to the screen versions right
	afterwards. Some parts of the code require it to be like this (when generating
	the cascade, for instance, it is required that there be at least one
	PageDescription in the print FramesDocument, but this is not the case when
	calling ReloadCssProperties() (this could probably easily be fixed, but who knows
	what other such assumptions we have lying around...)), although we should ideally
	(and here I'm using the world "ideally" very mildly) switch these at a very high
	level in reflow (e.g. FramesDocument::Reflow()). Other parts of the code require
	that the FramesDocument and VisualDevice pointers be the print versions if we're
	actually printing (this is required e.g. when calling DimensionsChanged() on
	CSSCollection).

	So here's a class to help easily switch between print and screen versions when
	necessary.

	This object is meant to be allocated on the stack, and then constructor will
	switch all necessary pointers. The destructor will restore the pointers to what
	they were previously. One can also call Start() and End() to switch explicitly.

	This class will change and restore FramesDocument and VisualDevice pointers on
	HLDocProfile (and consecutively on CSSCollection), and optionally on the
	LayoutInfo object. It will also temporarily override a special VisualDevice
	pointer used for print preview in DocumentManager, if it has been set.

	It is okay to nest invocations of this class, i.e. creating a new
	PrintStateSwitcher when it already has been done higher up on the call-stack is
	safe. */

class PrintStateSwitcher
{
private:
	/** The LayoutWorkplace we're working on.

		The LayoutWorkplace always points to the "right" FramesDocument, i.e. the
		print document if we're printing. */

	LayoutWorkplace* workplace;

	/** Pointer to LayoutInfo. May be NULL. */

	LayoutInfo* layout_info;

	/** The document to save when creating a PrintStateSwitcher object and the one
		to restore back to when destroying such an object (or when calling End()). */

	FramesDocument* old_doc;

	/** The VisualDevice to save when creating a PrintStateSwitcher object and the
		one to restore back to when destroying such an object (or when calling
		End()). */

	VisualDevice* old_vis_dev;

	/** If there's a special VisualDevice used for print preview (this probably
		happens if you try to print while in print preview), it also needs to be
		temporarily overridden and restored afterwards. */

	VisualDevice* old_preview_vis_dev;

	/** Set when this object has temporarily switched to the print versions of
		FramesDocument and VisualDevice. */

	BOOL started;

public:

	/** Constructor. Switches pointers by default.

		@param workplace The layout workplace, expected to point to the print
		document if we're printing.

		@param layout_info (optional) Current LayoutInfo structure.

		@param auto_start Will call Start() if set. Otherwise it's up to the client
		to call it whenever it's desired. */

	PrintStateSwitcher(LayoutWorkplace* workplace, LayoutInfo* layout_info = NULL, BOOL auto_start = TRUE);

	/** Destructor. Restores the pointers to what they were when this object was created. */

	~PrintStateSwitcher() { if (started) End(); }

	/** Switch to print pointers if they are there and it has not already been done. */

	void Start();

	/** Switch back to original pointers. */

	void End();
};

PrintStateSwitcher::PrintStateSwitcher(LayoutWorkplace* workplace, LayoutInfo* layout_info, BOOL auto_start)
	: workplace(workplace),
	  layout_info(layout_info),
	  started(FALSE)
{
	FramesDocument* doc = workplace->GetFramesDocument();
	BOOL is_printing = doc->IsPrintDocument();

	if (is_printing && auto_start)
		Start();
}

void PrintStateSwitcher::Start()
{
	if (started)
		return;

	FramesDocument* print_doc = workplace->GetFramesDocument();

	if (!print_doc->IsPrintDocument())
		// Not printing. PrintStateSwitcher will only be a no-op then.

		return;

	HLDocProfile* hld_profile = print_doc->GetHLDocProfile();
	DocumentManager* doc_man = print_doc->GetDocManager();

	old_doc = hld_profile->GetFramesDocument();
	old_vis_dev = hld_profile->GetVisualDevice();
	old_preview_vis_dev = doc_man->GetPrintPreviewVD();
	started = TRUE;

	// Format with printer device

	PrinterInfo* printer_info = print_doc->GetWindow()->GetPrinterInfo(TRUE);
	VisualDevice* new_vis_dev = printer_info->GetPrintDevice();

	if (doc_man->GetPrintPreviewVD() != new_vis_dev)
		doc_man->SetPrintPreviewVD(new_vis_dev);

	hld_profile->SetVisualDevice(new_vis_dev);
	hld_profile->SetFramesDocument(print_doc);

	if (layout_info)
		layout_info->visual_device = new_vis_dev;

	workplace->SwitchPrintingDocRootProperties();
}

void PrintStateSwitcher::End()
{
	if (!started)
		return;

	started = FALSE;

	FramesDocument* print_doc = workplace->GetFramesDocument();
	HLDocProfile* hld_profile = print_doc->GetHLDocProfile();
	DocumentManager* doc_man = print_doc->GetDocManager();

	OP_ASSERT(print_doc->IsPrintDocument());

	if (doc_man->GetPrintPreviewVD() != old_preview_vis_dev)
		doc_man->SetPrintPreviewVD(old_preview_vis_dev);

	hld_profile->SetVisualDevice(old_vis_dev);
	hld_profile->SetFramesDocument(old_doc);

	if (layout_info)
		layout_info->visual_device = old_vis_dev;

	workplace->SwitchPrintingDocRootProperties();
}

#endif // _PRINT_SUPPORT_

LayoutWorkplace::LayoutWorkplace(FramesDocument* theDoc) :	doc(theDoc)
	  ,	reflow_count(0)
#if LAYOUT_MAX_REFLOWS > 0
	  ,	internal_reflow_count(0)
#endif
	  ,	reflow_time(0.0)
#ifdef RESERVED_REGIONS
	  , reserved_region_boxes(0)
#endif // RESERVED_REGIONS
#ifdef PAGED_MEDIA_SUPPORT
	  , page_break_element(NULL)
	  , page_breaking_started(FALSE)
	  , page_control_height(0)
#endif
#ifdef SUPPORT_TEXT_DIRECTION
	  , doc_bidi_status(LOGICAL_MAYBE_RTL)
	  , is_rtl_document(FALSE)
#endif // SUPPORT_TEXT_DIRECTION
	  , enable_store_replaced_content(FALSE)
	  , load_translation_x(0)
	  , load_translation_y(0)
	  , layout_view_x(0)
	  , layout_view_width(0)
	  , layout_view_min_width(0)
	  , layout_view_max_width(0)
	  , nominal_width(0)
	  , layout_view_y(0)
	  , layout_view_height(0)
	  , nominal_height(0)
	  , media_query_width(0)
	  , media_query_height(0)
	  , normal_era_width(0)
	  , viewport_overflow_x(CSS_VALUE_visible)
	  , viewport_overflow_y(CSS_VALUE_visible)
	  , has_bg_fixed(FALSE)
	  , yield_element(NULL)
	  , yield_element_extra_dirty(FALSE)
	  , can_yield(FALSE)
  	  , yield_force_layout_changed(NO_FORCE)
#ifdef LAYOUT_YIELD_REFLOW
	  , max_reflow_time(MS_BEFORE_YIELD)
#else
	  , max_reflow_time(0)
#endif
	  , yield_count(0)
#ifdef LAYOUT_TRAVERSE_DIRTY_TREE
	  , needs_another_reflow_pass(FALSE)
#endif // LAYOUT_TRAVERSE_DIRTY_TREE
	  , keep_original_layout(FALSE)
#ifdef _PLUGIN_SUPPORT_
	  , plugins_disabled(FALSE)
#endif
	  , is_traversing(FALSE)
	  , is_in_reflow_iteration(FALSE)
	  , force_doc_root_props_recalc(FALSE)
	  , root_size_changed(FALSE)
#ifdef _DEBUG
	  , allow_dirty_child_props(FALSE)
#endif
	  , saved_old_height(-1)
	  , saved_old_width(-1)
	  , active_doc_root_props(&doc_root_props)
#ifdef CSS_TRANSITIONS
	  , transition_manager(theDoc)
#endif // CSS_TRANSITIONS
{
#ifdef GADGET_SUPPORT
	if (theDoc->GetWindow()->GetGadget())
		SetKeepOriginalLayout();
#endif // GADGET_SUPPORT

#ifdef LAYOUT_YIELD_REFLOW
	max_reflow_time = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::InitialYieldReflowTime);
#endif

	media_query_device_width = LayoutCoord(doc->GetVisualDevice()->GetScreenWidthCSS());
	media_query_device_height = LayoutCoord(doc->GetVisualDevice()->GetScreenHeightCSS());

	CalculateNominalLayoutViewSize(nominal_width, nominal_height);
	CalculateLayoutViewSize(FALSE, layout_view_width, layout_view_min_width, layout_view_max_width, layout_view_height, media_query_width, media_query_height);
}

void LayoutWorkplace::CalculateNominalLayoutViewSize(LayoutCoord& width, LayoutCoord& height) const
{
	Window* window = doc->GetWindow();
	VisualDevice* vis_dev = doc->GetVisualDevice();

	if (FramesDocElm* frame = doc->GetDocManager()->GetFrame())
	{
		width = LayoutCoord(vis_dev->ScaleToScreen(frame->GetNormalWidth()));
		height = LayoutCoord(vis_dev->ScaleToScreen(frame->GetNormalHeight()));
	}
	else
	{
		OP_ASSERT(doc->GetSubWinId() == -1);

		unsigned int window_width, window_height;
		window->GetCSSViewportSize(window_width, window_height);
		width = LayoutCoord(window_width);
		height = LayoutCoord(window_height);
	}
}

void LayoutWorkplace::CalculateLayoutViewSize(BOOL calculate_screen_coords, LayoutCoord& width, LayoutCoord& min_width, LayoutCoord& max_width, LayoutCoord& height, LayoutCoord& mq_width, LayoutCoord& mq_height) const
{
	Window* window = doc->GetWindow();
	VisualDevice* vis_dev = doc->GetVisualDevice();

	LayoutCoord width_doc_coords = LayoutCoord(-1); // width in document coordinates
	LayoutCoord min_width_doc_coords = LayoutCoord(-1); // minimum width in document coordinates
	LayoutCoord max_width_doc_coords = LayoutCoord(-1); // maximum width in document coordinates
	LayoutCoord width_screen_coords = LayoutCoord(-1); // width in screen coordinates
	LayoutCoord flexroot_width_doc_coords = LayoutCoord(-1); // FlexRoot width in document coordinates
	LayoutCoord height_doc_coords = LayoutCoord(-1); // height in document coordinates
	LayoutCoord height_screen_coords = LayoutCoord(-1); // height in screen coordinates
	LayoutCoord vscroll_size(0);
	LayoutCoord hscroll_size(0);

#ifdef PAGED_MEDIA_SUPPORT
	LayoutCoord cur_page_control_height = LayoutCoord(0);

	if (PagedContainer* root_paged_container = GetRootPagedContainer())
		cur_page_control_height = root_paged_container->GetPageControlHeight();
#endif // PAGED_MEDIA_SUPPORT

#ifdef CSS_VIEWPORT_SUPPORT
	CSS_Viewport* css_viewport = doc->GetHLDocProfile()->GetCSSCollection()->GetViewport();
	if (doc->IsTopDocument() && css_viewport->HasProperties())
	{
		// Viewport META element overrides layout viewport.
		width_doc_coords = LayoutCoord(css_viewport->GetWidth());
		height_doc_coords = LayoutCoord(css_viewport->GetHeight());

# ifdef PAGED_MEDIA_SUPPORT
		height_doc_coords -= cur_page_control_height;
# endif // PAGED_MEDIA_SUPPORT
	}
	else
#endif // CSS_VIEWPORT_SUPPORT
	{
		BOOL size_determined = FALSE;

		if (!keep_original_layout && doc->IsTopDocument() && window->GetLayoutMode() == LAYOUT_NORMAL)
		{
			ViewportController* controller = window->GetViewportController();
			LayoutCoord desktop_width = LayoutCoord(controller->GetDesktopLayoutViewportWidth());
			LayoutCoord desktop_height = LayoutCoord(controller->GetDesktopLayoutViewportHeight());

			if (desktop_width > 0 || desktop_height > 0)
			{
				unsigned int winwidth, winheight;

				window->GetCSSViewportSize(winwidth, winheight);

				if (winwidth > 0 && winheight > 0)
				{
					/* Root document layout viewport size overridden by viewport
					   controller's "desktop layout viewport size" settings. */

					if (desktop_width == 0)
					{
						// Only height overridden. Calculate width from window aspect ratio.

						width_doc_coords = LayoutCoord(winwidth * desktop_height / winheight);
						height_doc_coords = desktop_height;
					}
					else
					{
						width_doc_coords = desktop_width;

						if (desktop_height == 0)
							// Only width overridden. Calculate height from window aspect ratio.

							height_doc_coords = LayoutCoord(winheight * desktop_width / winwidth);
						else
							height_doc_coords = desktop_height;
					}

#ifdef PAGED_MEDIA_SUPPORT
					height_doc_coords -= cur_page_control_height;
#endif // PAGED_MEDIA_SUPPORT

					size_determined = TRUE;
				}
			}
		}

		if (!size_determined)
		{
			// Base layout viewport on actual window or frame size

			vscroll_size = LayoutCoord(vis_dev->GetVerticalScrollbarSpaceOccupied());
			hscroll_size = LayoutCoord(vis_dev->GetHorizontalScrollbarSpaceOccupied());

			if (FramesDocElm* frame = doc->GetDocManager()->GetFrame())
			{
				width_doc_coords = LayoutCoord(frame->GetNormalWidth());
				height_doc_coords = LayoutCoord(frame->GetNormalHeight());

				vscroll_size = LayoutCoord(vis_dev->ScaleToDocRoundDown(vscroll_size));
				hscroll_size = LayoutCoord(vis_dev->ScaleToDocRoundDown(hscroll_size));
#ifdef PAGED_MEDIA_SUPPORT
				hscroll_size += LayoutCoord(vis_dev->ScaleToDocRoundDown(cur_page_control_height));
#endif // PAGED_MEDIA_SUPPORT
				width_doc_coords -= vscroll_size;
				height_doc_coords -= hscroll_size;
			}
			else
			{
				OP_ASSERT(doc->GetSubWinId() == -1);

				if (calculate_screen_coords)
				{
					int window_width, window_height;
					window->GetClientSize(window_width, window_height);

					width_screen_coords = LayoutCoord(window_width);
					height_screen_coords = LayoutCoord(window_height);
#ifdef PAGED_MEDIA_SUPPORT
					hscroll_size += cur_page_control_height;
#endif // PAGED_MEDIA_SUPPORT
					width_screen_coords -= vscroll_size;
					height_screen_coords -= hscroll_size;
				}
				else
				{
					unsigned int window_width, window_height;
					window->GetCSSViewportSize(window_width, window_height);

					width_doc_coords = LayoutCoord(window_width);
					height_doc_coords = LayoutCoord(window_height);

					vscroll_size = LayoutCoord(vis_dev->ScaleToDocRoundDown(vscroll_size));
					hscroll_size = LayoutCoord(vis_dev->ScaleToDocRoundDown(hscroll_size));
#ifdef PAGED_MEDIA_SUPPORT
					hscroll_size += LayoutCoord(vis_dev->ScaleToDocRoundDown(cur_page_control_height));
#endif // PAGED_MEDIA_SUPPORT
					width_doc_coords -= vscroll_size;
					height_doc_coords -= hscroll_size;
				}
			}

			if (UsingFlexRoot())
			{
				min_width_doc_coords = LayoutCoord(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FlexRootMinWidth));
				max_width_doc_coords = LayoutCoord(window->GetFlexRootMaxWidth());

				if (min_width_doc_coords > 0)
					vscroll_size = LayoutCoord(0);

				if (HTML_Element* root_elm = doc->GetDocRoot())
					if (Box* root_box = root_elm->GetLayoutBox())
						flexroot_width_doc_coords = root_box->GetWidth();
			}
		}
	}

	// Make sure that all size values in document coordinates are set.

	if (width_doc_coords == -1)
	{
		width_doc_coords = LayoutCoord(vis_dev->ScaleToDoc(width_screen_coords));
		vscroll_size = LayoutCoord(vis_dev->ScaleToDoc(vscroll_size));
	}

	if (height_doc_coords == -1)
	{
		height_doc_coords = LayoutCoord(vis_dev->ScaleToDoc(height_screen_coords));
		hscroll_size = LayoutCoord(vis_dev->ScaleToDoc(hscroll_size));
	}

	if (min_width_doc_coords <= 0)
		min_width_doc_coords = width_doc_coords;

	if (max_width_doc_coords <= 0)
		max_width_doc_coords = width_doc_coords;

	// Use FlexRoot width, if present.

	if (flexroot_width_doc_coords != -1)
		width_doc_coords = flexroot_width_doc_coords;

	// Set output parameters, either in document or screen coordinates.

	if (calculate_screen_coords)
	{
		if (width_screen_coords == -1 || height_screen_coords == -1)
		{
			// Screen coordinate values not set. Calculate them based on document coordinates.

			width = LayoutCoord(vis_dev->ScaleToScreen(width_doc_coords));
			height = LayoutCoord(vis_dev->ScaleToScreen(height_doc_coords));
		}
		else
		{
			width = width_screen_coords;
			height = height_screen_coords;
		}

		min_width = LayoutCoord(vis_dev->ScaleToScreen(min_width_doc_coords));
		max_width = LayoutCoord(vis_dev->ScaleToScreen(max_width_doc_coords));
	}
	else
	{
		width = width_doc_coords;
		height = height_doc_coords;
		min_width = min_width_doc_coords;
		max_width = max_width_doc_coords;
	}

	mq_width = layout_view_min_width + vscroll_size;
	mq_height = layout_view_height + hscroll_size;
}

void LayoutWorkplace::CalculateScrollbars(BOOL& want_vertical, BOOL& want_horizontal) const
{
	VisualDevice* vis_dev = doc->GetVisualDevice();
	HLDocProfile* hld_profile = doc->GetHLDocProfile();
	int doc_width = doc->Width();
	int doc_height = doc->Height();
	int negative_overflow = doc->NegativeOverflow();
	BOOL allow_vertical = TRUE;
	BOOL allow_horizontal = TRUE;
	BOOL force_vertical = FALSE;
	BOOL force_horizontal = FALSE;

	int view_width = vis_dev->ScaleToDoc(vis_dev->WinWidth());
	int view_height = vis_dev->ScaleToDoc(vis_dev->WinHeight());

	if (doc->IsFrameDoc())
	{
		if (
#ifdef _PRINT_SUPPORT_
			!doc->GetDocManager()->GetWindow()->GetPreviewMode() &&
#endif
			!doc->GetFramesStacked() && !doc->GetSmartFrames())
		{
			// Normally no scrollbar on framesets.

			allow_vertical = FALSE;
			allow_horizontal = FALSE;
		}
	}
	else
	{
		short overflow_x = GetViewportOverflowX();
		short overflow_y = GetViewportOverflowY();

		if (overflow_x == CSS_VALUE_hidden)
			allow_horizontal = FALSE;
		else
			if (overflow_x == CSS_VALUE_scroll)
				force_horizontal = TRUE;

		if (overflow_y == CSS_VALUE_hidden)
			allow_vertical = FALSE;
		else
			if (overflow_y == CSS_VALUE_scroll)
				force_vertical = TRUE;
	}

	want_vertical = force_vertical || allow_vertical && doc_height > view_height;
	want_horizontal = force_horizontal || allow_horizontal && doc_width + negative_overflow > view_width;

	if (want_horizontal && !want_vertical && allow_vertical)
	{
		view_height -= vis_dev->GetHorizontalScrollbarSize();

		/* Check again if we need the vertical scrollbar (since the horizontal scrollbar
		   has now made the viewport shorter). We can't use doc_height because it always
		   fills the viewport so we would always turn the vertical scrollbar on and then
		   turn it off in a blink next reflow if not needed. Checking the bottom of BODY's
		   bounding box is unfortunately not always right (includes outline, for instance),
		   but this will do for now. */

		if (HTML_Element *body = hld_profile->GetBodyElm())
		{
			RECT rect = { 0, 0, 0, 0 };
			body->GetBoxRect(doc, BOUNDING_BOX, rect);

			if (rect.bottom > view_height)
				want_vertical = TRUE;
		}
	}
}

void LayoutWorkplace::CalculateNormalEraWidth()
{
	Box* root_box = doc->GetDocRoot()->GetLayoutBox();
	LayoutCoord old_normal_era_width = normal_era_width;
	int view_width;

#ifdef _PRINT_SUPPORT_
	PrintStateSwitcher print_state_switcher(this, NULL, FALSE);

	if (doc->IsPrintDocument())
	{
		PrintDevice* pd = doc->GetWindow()->GetPrinterInfo(TRUE)->GetPrintDevice();
		OP_ASSERT(pd);
		int left, right, top, bottom;
		pd->GetUserMargins(left, right, top, bottom);
		view_width = pd->ScaleToDoc(pd->GetRenderingViewWidth() - left - right);
		print_state_switcher.Start();
	}
	else
# endif // _PRINT_SUPPORT_
		view_width = GetLayoutViewWidth();

	normal_era_width = LayoutCoord(0);

	LayoutCoord right_edge = FindNormalRightAbsEdge();

	normal_era_width = old_normal_era_width;

	if (right_edge <= view_width)
	{
		if (normal_era_width)
		{
			normal_era_width = LayoutCoord(0);
			if (root_box)
				root_box->MarkBoxesWithAbsoluteWidthDirty(doc);
		}
	}
	else
		if (right_edge != normal_era_width)
		{
			normal_era_width = right_edge;
			if (root_box)
				root_box->MarkBoxesWithAbsoluteWidthDirty(doc);
		}
}

BOOL LayoutWorkplace::IsTraversable() const
{
	HTML_Element* root = doc->GetDocRoot();

	return !root->IsDirty() && !root->IsPropsDirty() && !root->HasDirtyChildProps();
}

#ifdef DISPLAY_SPOTLIGHT

OP_STATUS
LayoutWorkplace::AddSpotlight(VisualDevice* vd, void* html_element_id)
{
	VDSpotlight* spotlight = OP_NEW(VDSpotlight, ());

	if (spotlight)
	{
		OP_STATUS status = vd->AddSpotlight(spotlight, (HTML_Element*) html_element_id);

		return status;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS
LayoutWorkplace::RemoveSpotlight(VisualDevice* vd, void* html_element_id)
{
	vd->RemoveSpotlight((HTML_Element*) html_element_id);

	return OpStatus::OK;
}

#endif

void LayoutWorkplace::SignalHtmlElementRemoved(HTML_Element* html_element)
{
	if (html_element->IsReferenced())
	{
		counters.RemoveElement(html_element);

#ifdef PAGED_MEDIA_SUPPORT
		if (page_break_element == html_element)
			page_break_element = NULL;
#endif

		if (yield_element == html_element)
			yield_element = yield_element->Parent();
	}

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE
	if (html_element->Parent())
		if (Box* layout_box = html_element->GetLayoutBox())
			if (!doc->IsParsed() || !doc->InlineImagesLoaded() || doc->IsAborted())
				layout_box->SignalHtmlElementDeleted(doc);
#endif // LAYOUT_TRAVERSE_DIRTY_TREE
}

OP_STATUS LayoutWorkplace::Reflow(BOOL& reflow_complete, BOOL set_properties, BOOL iterate)
{
	OP_STATUS status = OpStatus::OK;
	LogicalDocument* logdoc = doc->GetLogicalDocument();
	FramesDocElm* frame = doc->GetDocManager()->GetFrame();
	HTML_Document* html_doc = doc->GetHtmlDocument();
	BOOL is_iframe = frame && frame->IsInlineFrame();
#ifdef CONTENT_MAGIC
	PrefsCollectionDisplay::RenderingModes rendering_mode = doc->GetPrefsRenderingMode();
	BOOL old_content_magic_found = doc->GetContentMagicFound();
#endif // CONTENT_MAGIC
	BOOL has_reflowed = FALSE;
	BOOL keep_existing_scrollbars = FALSE;

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE
	if (doc->IsAborted())
	{
		/* Allow traversal of dirty layout under certain circumstances.
		   FIXME: Should honor needs_another_reflow_pass */

		return OpStatus::OK;
	}
#endif // LAYOUT_TRAVERSE_DIRTY_TREE

	BOOL is_frame_magic_candidate = frame && !is_iframe && html_doc;

	reflow_complete = FALSE;
	reflow_start = g_op_time_info->GetRuntimeMS();

#if defined PAGED_MEDIA_SUPPORT && defined _PRINT_SUPPORT_
	if (!doc->IsPrintDocument() && logdoc->GetPrintRoot())
	{
		// print preview?

		doc->SetReflowing(FALSE);
		return status;
	}
#endif // PAGED_MEDIA_SUPPORT && _PRINT_SUPPORT_

	HTML_Element* root = doc->GetDocRoot();

	if (set_properties)
	{
		/* Consider getting rid of the set_properties flag some day. In some cases,
		   set_properties means "regenerate all layout boxes". In other cases it means "reload
		   properties on all elements". So we have to do both here. */

		root->MarkPropsDirty(doc);
		root->MarkExtraDirty(doc);
	}

	BOOL had_dirty_props = root->IsPropsDirty() || root->HasDirtyChildProps();

	/* May iterate when there are elements that need to be reflowed twice (e.g.
	   shrink-to-fit containers or tables) (but only if 'iterate' is TRUE).

	   When reflowing of internal elements is done, iterations may also be caused by calculating
	   frame sizes for frame magic mode (frame-stacking or smart-frames). Finally width changes
	   regarding hiding / showing scrollbars can cause extra loops. */

	do
	{
		OP_BOOLEAN needed_reflow;
		LayoutInfo info(this);

		info.external_layout_change = root_size_changed;
		root_size_changed = FALSE;

		long old_height = 0;
		int old_width = 0;

		if (is_frame_magic_candidate)
		{
			// Store the current content size

			if (saved_old_height >= 0)
				old_height = saved_old_height;
			else
				old_height = html_doc->Height();

			if (saved_old_width >= 0)
				old_width = saved_old_width;
			else
				old_width = html_doc->Width();
		}

		saved_old_height = -1;
		saved_old_width = -1;

		needed_reflow = ReflowOnce(info);

		SetIsInReflowIteration(FALSE);

		EndStoreReplacedContent();

		switch (needed_reflow)
		{
		case OpStatus::ERR_NO_MEMORY:
			RecalculateScrollbars();
			doc->HandleDocumentSizeChange();
			doc->SetReflowing(FALSE);
			OpStatus::Ignore(status);
			status = needed_reflow;
			return status;
		case OpStatus::ERR_YIELD:
			OP_ASSERT(max_reflow_time > 0);
			doc->SetReflowing(FALSE);
			OpStatus::Ignore(status);
			status = needed_reflow;
			yield_count++;
			{
#ifdef LAYOUT_YIELD_REFLOW
				int yield_increase_rate = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::YieldReflowTimeIncreaseRate);
#else
				int yield_increase_rate = 20;
#endif

				if (yield_count > yield_increase_rate)
				{
					max_reflow_time *= 2;
					yield_count = 0;
				}
			}
			return status;
		case OpBoolean::IS_TRUE:
			has_reflowed = TRUE;
			break;
		}

		ReflowPendingElements();

#ifdef PAGED_MEDIA_SUPPORT
		/* If we set out to find a pending page break in the reflow pass we just
		   performed, but failed to find it, the following assertion will fail: */

		OP_ASSERT(info.paged_media != PAGEBREAK_FIND);

		if (page_break_element)
		{
			/* A pending page break was inserted during the reflow pass we just
			   performed. We need to revisit the element where that happened in the next
			   reflow pass (in order to convert it to a proper page break and also update
			   the layout of elements that might be affected by the break). */

			BOOL root_was_already_dirty = doc->GetDocRoot()->IsDirty();

			page_break_element->MarkDirty(doc, FALSE);

			if (root_was_already_dirty && !page_breaking_started)
			{
				/* However, something else was marked dirty already (e.g. a shrink-to-fit
				   container needing another reflow pass). That in itself may affect the
				   position of elements, and thus pagination. Since we haven't really
				   started pagination yet, put it off as long as we have dirty elements,
				   so that we get it as right as possible. */

				if (Box* layout_box = page_break_element->GetLayoutBox())
					layout_box->RemovePendingPagebreak();

				page_break_element = NULL;
			}
			else
				/* Pagination has now officially started (if it hasn't already). Don't
				   try to repaginate, unless explicitly allowed to do so. Any reflow
				   after this point will not affect previously inserted page breaks. */

				page_breaking_started = TRUE;
		}
#endif // PAGED_MEDIA_SUPPORT

		if (doc->GetAbsPositionedStrategy() == ABS_POSITION_SCALE && has_reflowed && !root->IsDirty())
			CalculateNormalEraWidth();

		if (is_frame_magic_candidate)
		{
			if (has_reflowed && !root->IsDirty())
			{
				/* The document is done reflowing its internal structures. Now calculate frame-magic stuff,
			       which also may cause an extra reflow iteration. */

				if (html_doc->Width() != old_width || html_doc->Height() != old_height)
					doc->RecalculateMagicFrameSizes();
			}
			else if (has_reflowed && !iterate && (doc->GetFramesStacked() || doc->GetSmartFrames()))
			{
				/* We are not done reflowing yet, but we are only interested in comparing
				   width and height changes to the original width/height. */

				saved_old_height = old_height;
				saved_old_width = old_width;
			}
		}

		if (has_reflowed && !root->IsDirty())
		{
#ifdef _PRINT_SUPPORT_
			/* Switch to the printer versions of FramesDocument and VisualDevice
			   here. This is mainly required for media query evaluation (when calling
			   DimensionsChanged() on CSSCollection), but is hopefully a healthy and
			   above all harmless thing to do in general. See CORE-49270. */

			PrintStateSwitcher print_state_switcher(this, &info);
#endif // _PRINT_SUPPORT_

			if (RecalculateScrollbars(keep_existing_scrollbars))
			{
				// Scrollbar visibility state changed. Will need another reflow pass then.

				RecalculateLayoutViewSize();

				/* Don't allow scrollbars to become hidden during this reflow
				   operation, as that could trigger infinite reflow loops. */

				keep_existing_scrollbars = TRUE;
			}

			doc->HandleDocumentSizeChange();
		}

		/** Need to reflow content sized child iframes here. Otherwise iframe paints may
			cause this document to be marked dirty during traverse. */

		if (doc->GetIFrmRoot())
		{
			FramesDocElm* elm = doc->GetIFrmRoot()->FirstChild();

			while (elm)
			{
				if (elm->GetCurrentDoc() &&
					elm->GetNotifyParentOnContentChange() &&
					!elm->GetCurrentDoc()->IsReflowing())
				{
					status = elm->GetCurrentDoc()->Reflow(FALSE, TRUE, FALSE, FALSE);
					if (OpStatus::IsError(status))
					{
						RecalculateScrollbars();
						doc->HandleDocumentSizeChange();
						return status;
					}
				}

				elm = elm->Suc();
			}
		}

#if LAYOUT_MAX_REFLOWS > 0
		++internal_reflow_count;
#endif
	}
	while (iterate && root->IsDirty()
#if LAYOUT_MAX_REFLOWS > 0
		&& internal_reflow_count < LAYOUT_MAX_REFLOWS
#endif
#ifdef PAGED_MEDIA_SUPPORT
		&& (!page_break_element || frame)
#endif
		);

#if LAYOUT_MAX_REFLOWS > 0
	if (internal_reflow_count >= LAYOUT_MAX_REFLOWS)
	{
		WindowCommander *wic = doc->GetWindow()->GetWindowCommander();
		wic->GetDocumentListener()->OnReflowStuck(wic);
	}
	else
		internal_reflow_count = 0;
#endif

	// This is for profiling
	reflow_time += g_op_time_info->GetRuntimeMS() - reflow_start;

	doc->SetReflowing(FALSE);

#ifdef _PRINT_SUPPORT_
	if (!page_break_element && doc->GetTopFramesDoc()->IsPrintDocument() && !is_iframe)
	{
		Window* window = doc->GetWindow();

		if (window->IsFormattingPrintDoc())
		{
			OP_NEW_DBG("FramesDocument::Reflow", "async_print");
			OP_DBG(("SetFormattingPrintDoc(FALSE), IsPrinting(): %d", window->IsPrinting()));
			window->SetFormattingPrintDoc(FALSE);
			if (VisualDevice* pvd = doc->GetDocManager()->GetPrintPreviewVD())
				pvd->UpdateAll();
		}

		if (!window->IsFormattingPrintDoc() && window->IsPrinting())
		{
			OP_NEW_DBG("FramesDocument::Reflow2", "async_print");
			OP_DBG(("Will send DOC_PRINT_FORMATTED"));

			/* We are finished reflowing this print document, now we need to send the message that we are ready */
			window->GetMessageHandler()->PostMessage(DOC_PRINT_FORMATTED, 0, 0);
			window->SetPrinting(FALSE);
		}
	}
#endif // _PRINT_SUPPORT_

#ifdef CONTENT_MAGIC
	if (!old_content_magic_found && doc->GetContentMagicFound())
		doc->SetContentMagicPosition(1); // just to make sure that position is maintained
#endif // CONTENT_MAGIC

	if ((has_reflowed || had_dirty_props) && IsTraversable())
	{
#ifdef LAYOUT_YIELD_REFLOW
		max_reflow_time = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::InitialYieldReflowTime);
#else
		max_reflow_time = 0;
#endif
		yield_count = 0;
		yield_force_layout_changed = NO_FORCE;

		// We have reflowed, and there is no pending reflow

		logdoc->SetCompleted(FALSE, doc->IsPrintDocument());

		HandleContentSizedIFrame();

		OP_STATUS temp = doc->GetVisualDevice()->CheckCoreViewVisibility();
		if (OpStatus::IsSuccess(status))
			status = temp;
		// else keep the previous status as it is OOM.

#ifdef CONTENT_MAGIC
		if (!doc->IsInlineFrame() && doc->GetContentMagicPosition() && g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::TableMagic)) == 1)
		{
			doc->SetContentMagicPosition();

			HTML_Element* content_magic_start = doc->GetContentMagicStart();

			if (doc->GetHtmlDocument() && content_magic_start)
			{
				HTML_Element *candidate = content_magic_start->ParentActual();

				if (candidate && candidate->Prev())
					candidate = (HTML_Element *)candidate->Prev();

				doc->GetHtmlDocument()->SetNavigationElement(candidate, FALSE);
			}
		}
#endif // CONTENT_MAGIC

		// Set visual viewport position to point at saved element
		if (html_doc)
			html_doc->ScrollToSavedElement();

		reflow_complete = TRUE;
		if (has_reflowed)
			++reflow_count;

		OpStatus::Ignore(status);
		status = doc->CheckFinishDocument();
	}

	counters.Recalculate(doc);

	return status;
}

OP_BOOLEAN LayoutWorkplace::ReflowOnce(LayoutInfo& info)
{
//	OP_PROBE5(OP_PROBE_LOGDOC_REFLOW);

	HLDocProfile* hld_profile = doc->GetHLDocProfile();
	LogicalDocument* logdoc = doc->GetLogicalDocument();

#ifdef XSLT_SUPPORT
	OP_ASSERT(!logdoc->IsXSLTInProgress());
#endif // XSLT_SUPPORT

	HTML_Element* reflow_root = doc->GetDocRoot();

	OP_ASSERT(!yield_element || !reflow_root->IsDirty());

#ifdef _PRINT_SUPPORT_
	BOOL is_printing = doc->IsPrintDocument();
#endif

	if (reflow_root->IsExtraDirty())
	{
		HTML_Element* child_elm = reflow_root->FirstChildActualStyle();

		while (child_elm)
		{
			child_elm->SetDirty(ELM_BOTH_DIRTY);
			child_elm = child_elm->SucActualStyle();
		}
	}

	if (reflow_root->IsPropsDirty())
	{
		hld_profile->SetCSSMode(doc->GetWindow()->GetCSSMode());

		LayoutMode layout_mode = doc->GetWindow()->GetLayoutMode();
		BOOL is_smallscreen = layout_mode == LAYOUT_SSR || layout_mode == LAYOUT_CSSR;

		if (((is_smallscreen && ((hld_profile->GetCSSMode() == CSS_FULL && hld_profile->HasMediaStyle(CSS_MEDIA_TYPE_HANDHELD)))) ||
			 (layout_mode != LAYOUT_SSR &&
			  g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(hld_profile->GetCSSMode(), PrefsCollectionDisplay::EnableAuthorCSS))
				 )) && hld_profile->GetUnloadedCSS())
		{
			OP_STATUS stat = hld_profile->LoadAllCSS();
			hld_profile->SetUnloadedCSS(FALSE);

			RETURN_IF_MEMORY_ERROR(stat);
		}
	}

#ifdef CSS_VIEWPORT_SUPPORT
	// Make sure the properties are cascaded if dirty
	hld_profile->GetCSSCollection()->GetViewport();
#endif // CSS_VIEWPORT_SUPPORT

	RETURN_IF_ERROR(ReloadCssProperties());

	/* We may have marked elements dirty/extra dirty above. If so, a reflow message was posted,
	   which is kind of pointless (but mostly harmless) at this point. */

	if (reflow_root->IsDirty() || yield_element)
	{
#ifdef SCOPE_PROFILER
		OpTypeProbe probe;

		if (GetFramesDocument()->GetTimeline())
		{
			OpProbeTimeline *t = GetFramesDocument()->GetTimeline();
			RETURN_IF_ERROR(probe.Activate(t, PROBE_EVENT_LAYOUT, t->GetUniqueKey()));
		}
#endif // SCOPE_PROFILER

		if (!info.external_layout_change)
			if (reflow_root->GetDirty() & (ELM_MINMAX_DELETED | ELM_EXTRA_DIRTY))
				/* The root's dirtiness is not caused by the layout engine (at least not
				   the layout engine alone). Boxes that need multiple reflow passes can
				   use this information to tell if it's safe to request another reflow
				   pass or not (to avoid circular dependencies). This also means that we
				   should restart pagination. */

				info.external_layout_change = TRUE;

		OP_BOOLEAN status = OpBoolean::IS_TRUE;

#ifdef _PRINT_SUPPORT_
		PrintStateSwitcher print_state_switcher(this, &info);
#endif // _PRINT_SUPPORT_

#ifdef PAGED_MEDIA_SUPPORT
		if (Box* root_box = reflow_root->GetLayoutBox())
			if (!HTMLayoutProperties::IsPagedOverflowValue(GetViewportOverflowX()) !=
				(root_box->GetContainer()->GetPagedContainer() == NULL))
				// Changed between paged mode and continuous mode

				reflow_root->RemoveLayoutBox(doc);

		if (info.paged_media != PAGEBREAK_OFF)
		{
			if (info.external_layout_change || !reflow_root->GetLayoutBox())
			{
				/* If the root box is invalid, we can (and should) restart page
				   breaking. We can also do that if the reflow was triggered by
				   something external. */

				if (page_break_element)
				{
					if (Box* layout_box = page_break_element->GetLayoutBox())
						layout_box->RemovePendingPagebreak();

					page_break_element = NULL;
				}

				page_breaking_started = FALSE;
			}

			if (page_breaking_started)
				if (page_break_element)
				{
					// Look for pending page break.

					info.paged_media = PAGEBREAK_FIND;

					/* We're probably not going to find the pending page break unless
					   its containing element is dirty: */

					OP_ASSERT(page_break_element->IsDirty());

					page_break_element = NULL;
				}
				else
					/* If page breaking has started (i.e. we have already inserted
					   implicit page breaks), but there was no pending page break to look
					   for this time, it means that something not related to page
					   breaking requested this reflow (and probably on the very last
					   page, since no pending page break was inserted at the same
					   time). Keep all previously inserted page breaks, rather than
					   restarting pagination, so that we don't risk infinite reflow loops
					   (in fact it's not just a risk; it's pretty much guaranteed that we
					   would get an infinite loop in such cases). */

					info.paged_media = PAGEBREAK_KEEP;

			if (info.paged_media != PAGEBREAK_KEEP)
				doc->ClearPages();
		}
#endif // PAGED_MEDIA_SUPPORT

		AutoDeleteHead reflow_cascade;
		LayoutProperties* top_cascade = NULL;

#ifdef LAYOUT_YIELD_REFLOW
		if (yield_cascade.First())
			// Attach to the yield cascade.

			top_cascade = (LayoutProperties*) yield_cascade.First();
		else
#endif // LAYOUT_YIELD_REFLOW
		{
			top_cascade = new LayoutProperties;

			if (!top_cascade)
			{
				status = OpStatus::ERR_NO_MEMORY;
				goto end_of_function;
			}

			top_cascade->Into(&reflow_cascade);
		}

		if (yield_element)
		{
			VisualDevice* vd = info.visual_device;
			vd->ClearTranslation();
			vd->Translate(load_translation_x, load_translation_y);
		}
		else
			info.SetRootTranslation(LayoutCoord(0), LayoutCoord(0));

		StartStoreReplacedContent();

		if (reflow_root->IsExtraDirty() || !reflow_root->GetLayoutBox())
		{
			info.visual_device->Reset();

			OP_ASSERT(!yield_element);

			// set ua default values
			top_cascade->GetProps()->Reset(NULL, info.hld_profile);

			if (!info.doc->IsWaitingForStyles())
			{
				ClearCounters();
				counters.SetAddOnly(TRUE);
				SetIsInReflowIteration(TRUE);

				/* Remove children inserted by layout. This is needed when the document
				   root element itself is inserted by layout, for example because of
				   setting display:table-cell on the HTML (root) element. If the HTML
				   element is removed and the document then is reflowed, we need to get
				   rid of the layout-inserted tablery above the (now deleted) HTML
				   table-cell element. This is normally done on each extra dirty child in
				   Box::LayoutChildren(), but Markup::HTE_DOC_ROOT is nobody's child... */

				LayoutProperties::RemoveElementsInsertedByLayout(info, reflow_root);

				// Create layout boxes for all elements in the tree that are entitled to one.

				LayoutProperties::LP_STATE tmp_stat = top_cascade->CreateChildLayoutBox(info, reflow_root);

				if (tmp_stat == LayoutProperties::OUT_OF_MEMORY)
				{
					status = OpStatus::ERR_NO_MEMORY;
					goto end_of_function;
				}
				else if (tmp_stat == LayoutProperties::YIELD)
				{
					status = OpStatus::ERR_YIELD;
				}

				// OpStatus::ERR_YIELD not handled here intentionally

				counters.SetAddOnly(FALSE);
			}
		}
		else
		{
			Box* root_box = reflow_root->GetLayoutBox();

			if (root_box)
			{
				LayoutProperties* child_cascade;

				info.visual_device->Reset();

				if (yield_element)
				{
					VisualDevice* vd = info.visual_device;
					vd->ClearTranslation();
					vd->Translate(load_translation_x, load_translation_y);
				}
				else
					// set ua default values

					top_cascade->GetProps()->Reset(NULL, info.hld_profile);

				child_cascade = top_cascade->GetChildCascade(info, reflow_root);

				if (!child_cascade)
				{
					status = OpStatus::ERR_NO_MEMORY;
					goto end_of_function;
				}
				else
				{
					SetIsInReflowIteration(TRUE);
#ifdef _DEBUG
					if (yield_element)
					{
						ReflowState* r = ((AbsolutePositionedBox*)root_box)->GetReflowState();
						OP_ASSERT(r);
					}
#endif

					HTML_Element* first_child = yield_element ? yield_element->Next() : NULL;

					yield_element = NULL;

					LAYST laystatus = root_box->Layout(child_cascade, info, first_child);

					if (laystatus == LAYOUT_OUT_OF_MEMORY)
					{
						status = OpStatus::ERR_NO_MEMORY;
						goto end_of_function;
					}
					else if (laystatus == LAYOUT_YIELD)
					{
						status = OpStatus::ERR_YIELD;
#ifdef _DEBUG
						ReflowState* r = ((AbsolutePositionedBox*)root_box)->GetReflowState();
						OP_ASSERT(r);
#endif
					}
					else
					{
						OP_ASSERT(!yield_element);
					}
				}
			}
			else
				reflow_root->MarkClean();
		}

		if (status != OpStatus::ERR_YIELD)
		{
			if (top_cascade && top_cascade->Suc())
				if (OpStatus::IsMemoryError(top_cascade->Suc()->Finish(&info)))
					status = OpStatus::ERR_NO_MEMORY;

#ifdef LAYOUT_YIELD_REFLOW
			yield_cascade.Clear();
#endif // LAYOUT_YIELD_REFLOW
		}

		if (info.text_selection)
			info.text_selection->MarkClean();

end_of_function:

#ifdef _PRINT_SUPPORT_
		logdoc->SetCompleted(FALSE, is_printing);
#else // _PRINT_SUPPORT_
		logdoc->SetCompleted(FALSE, FALSE);
#endif // _PRINT_SUPPORT_

#ifdef PAGED_MEDIA_SUPPORT
		if (info.paged_media != PAGEBREAK_OFF)
			doc->FinishCurrentPage(GetDocumentHeight());
#endif // PAGED_MEDIA_SUPPORT

		if (status == OpStatus::ERR_YIELD)
		{
			doc->PostReflowMsg(TRUE);
			StoreTranslation();
			doc->GetVisualDevice()->UpdateAll();
		}

		if (status == OpStatus::ERR_NO_MEMORY)
		{
			// Clean the tree to avoid dirty subtrees

			HTML_Element* iter = reflow_root;

			while (iter)
			{
				iter->MarkClean();

				if (Box* box = iter->GetLayoutBox())
				{
					if (box->IsBlockBox() && !((BlockBox*)box)->InList())
						iter->RemoveLayoutBox(doc);
					else
						box->CleanReflowState();
				}

				iter = iter->Next();
			}
		}

		return status;
	}
	else
		return OpBoolean::IS_FALSE;
}

void LayoutWorkplace::SetReflowElement(HTML_Element* element, BOOL check_if_exist)
{
	ReflowElem* reflow_element;

#if LAYOUT_MAX_REFLOWS > 0
	if (internal_reflow_count >= LAYOUT_MAX_REFLOWS)
		return;
#endif

	if (check_if_exist)
		for (reflow_element = (ReflowElem*) reflow_elements.First(); reflow_element; reflow_element = (ReflowElem*) reflow_element->Suc())
			if (reflow_element->GetElement() == element)
				return;

	reflow_element = new ReflowElem(element);

	if (reflow_element)
		reflow_element->Into(&reflow_elements);

	//else OOM
}

#ifdef PAGED_MEDIA_SUPPORT

void LayoutWorkplace::SetPageBreakElement(LayoutInfo& info, HTML_Element* html_element)
{
	OP_ASSERT(!page_break_element);
	OP_ASSERT(info.paged_media == PAGEBREAK_ON);

	page_break_element = html_element;
	info.paged_media = PAGEBREAK_OFF;
	info.pending_pagebreak_found = TRUE;
}

#endif // PAGED_MEDIA_SUPPORT

void LayoutWorkplace::ReflowPendingElements()
{
	while (ReflowElem *reflow_element = (ReflowElem*) reflow_elements.First())
	{
		reflow_element->Out();

		if (Box* box = reflow_element->GetElement()->GetLayoutBox())
			box->SignalChange(doc);

		OP_DELETE(reflow_element);
	}

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE
	needs_another_reflow_pass = !!doc->GetDocRoot()->IsDirty();
#endif // LAYOUT_TRAVERSE_DIRTY_TREE
}

LayoutWorkplace *
LayoutWorkplace::GetParentLayoutWorkplace()
{
	if (FramesDocument *parent_doc = doc->GetDocManager()->GetParentDoc())
		if (LogicalDocument *logdoc = parent_doc->GetLogicalDocument())
			return logdoc->GetLayoutWorkplace();

	return NULL;
}

void LayoutWorkplace::HandleContentSizedIFrame(BOOL root_has_changed /* = FALSE */)
{
	FramesDocElm* frame = doc->GetDocManager()->GetFrame();
	BOOL is_iframe = frame && frame->IsInlineFrame();

	/* Recursively look for iframes higher up in the frame stack.

	   Since frame based pages finishes loading in bottom-up order we
	   must recursivly look for parent iframes that may have to be
	   resized when the nested frame has been loaded. */
	LayoutWorkplace* parent_lwp = GetParentLayoutWorkplace();

	if (parent_lwp)
		parent_lwp->HandleContentSizedIFrame(root_has_changed);

	if (is_iframe && frame->GetNotifyParentOnContentChange())
	{
		FramesDocument *parent_doc = doc->GetParentDoc();

		if (parent_lwp && parent_lwp->IsInReflowIteration() && root_has_changed)
			/* When iframes are created or recreated during reflow in ERA mode they may notice that
			   the size they are given should cause them to change layout mode.  In turn this may
			   change the dimensions of the root box and normally we should tell the parent document
			   about it.  But doing that during reflow is dangerous and unnecessary.

			   In short, we shouldn't handle content sized iframes when we already are in reflow.  */
			return;

		if (HTML_Element *frm_elm = frame->GetHtmlElement())
		{
			Box* box = frm_elm->GetLayoutBox();
			if (box)
			{
				if (root_has_changed)
					if (Content* content = box->GetContent())
						if (content->IsReplaced())
						{
							ReplacedContent *replaced_content = static_cast<ReplacedContent *>(content);
							replaced_content->ResetSizeDetermined();
						}

				box->SignalChange(parent_doc);
			}
		}
	}
}

void LayoutWorkplace::StoreTranslation()
{
	HLDocProfile* hld_profile = doc->GetHLDocProfile();
	VisualDevice* vd = hld_profile->GetVisualDevice();

	if (vd)
	{
		load_translation_x = LayoutCoord(vd->GetTranslationX());
		load_translation_y = LayoutCoord(vd->GetTranslationY());
	}
}

void LayoutWorkplace::EndStoreReplacedContent()
{
	enable_store_replaced_content = FALSE;

	for (StoredReplacedContent* src = (StoredReplacedContent*) stored_replaced_content.First(); src; src = (StoredReplacedContent*) stored_replaced_content.First())
	{
		ReplacedContent* content = src->GetContent();

		src->Out();
		OP_DELETE(src);

		content->Disable(doc);
		OP_DELETE(content);
	}
}

/** Should the outline property of the element disable the highlight? */

OP_BOOLEAN
LayoutWorkplace::ShouldElementOutlineDisableHighlight(HTML_Element* elm, short& highlight_outline_extent) const
{
#ifdef SKIN_HIGHLIGHTED_ELEMENT
	if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::DisableHighlightUponOutline))
	{
		highlight_outline_extent = 0;
		return OpBoolean::IS_FALSE;
	}

	AutoDeleteHead props_list;
	LayoutProperties* cascade = LayoutProperties::CreateCascade(elm, props_list, doc->GetHLDocProfile());
	RETURN_OOM_IF_NULL(cascade);
	const HTMLayoutProperties& props = *cascade->GetProps();
	if (props.IsOutlineVisible())
		highlight_outline_extent = props.outline_offset >= 0 ? props.outline.width : props.outline.width + props.outline_offset;
	else
		highlight_outline_extent = 0;
	return props.ShouldOutlineDisableHighlight() ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
#else // !SKIN_HIGHLIGHTED_ELEMENT
	highlight_outline_extent = 0;
	return OpBoolean::IS_FALSE;
#endif // SKIN_HIGHLIGHTED_ELEMENT
}

BOOL LayoutWorkplace::StoreReplacedContent(HTML_Element* element, ReplacedContent* content)
{
	OP_ASSERT(enable_store_replaced_content);

	StoredReplacedContent* src = OP_NEW(StoredReplacedContent, (element, content));

	if (!src)
		return FALSE;

	content->DetachCoreView(doc);
	src->Into(&stored_replaced_content);

	return TRUE;
}

ReplacedContent* LayoutWorkplace::GetStoredReplacedContent(HTML_Element* element)
{
	for (StoredReplacedContent* src = (StoredReplacedContent*) stored_replaced_content.First(); src; src = (StoredReplacedContent*) src->Suc())
	{
		if (src->GetElement() == element)
		{
			ReplacedContent* content = src->GetContent();
			src->Out();
			OP_DELETE(src);

			content->AttachCoreView(doc, element);
			return content;
		}
	}

	return NULL;
}

void LayoutWorkplace::LayoutBoxRemoved(HTML_Element* html_element)
{
	Box* layout_box = html_element->GetLayoutBox();

	OP_ASSERT(layout_box);

	if (layout_box->IsWeakContent() && html_element->HasSpecialAttr(Markup::LAYOUTA_UNBREAKABLE_WIDTH, SpecialNs::NS_LAYOUT))
	{
#ifdef DEBUG_ENABLE_OPASSERT
		int res =
#endif // DEBUG_ENABLE_OPASSERT
			html_element->SetSpecialAttr(Markup::LAYOUTA_UNBREAKABLE_WIDTH, ITEM_TYPE_NUM, (void*)0, FALSE, SpecialNs::NS_LAYOUT);
		OP_ASSERT(res >= 0 || !"SetSpecialAttr should not fail when we have first checked for the presence of the attribute");
	}

	if (!fixed_positioned_area.IsEmpty() && layout_box->IsFixedPositionedBox())
	{
		VisualDevice* vis_dev = doc->GetVisualDevice();
		if (vis_dev)
			vis_dev->Update(fixed_positioned_area.x,
							fixed_positioned_area.y,
							fixed_positioned_area.width,
							fixed_positioned_area.height,
							TRUE);
	}

	layout_box->DisableContent(doc);

	counters.RemoveElement(html_element);
}


#ifdef SUPPORT_TEXT_DIRECTION

int LayoutWorkplace::GetNegativeOverflow()
{
	/* Only allow negative overflow if BODY has explicit or inherited RTL
	   direction. See also bug CORE-2976. */

	if (is_rtl_document)
		if (HTML_Element* doc_root = doc->GetDocRoot())
			if (Box* root = doc_root->GetLayoutBox())
				return ((VerticalBox*) root)->GetNegativeOverflow();

	return 0;
}

#endif // SUPPORT_TEXT_DIRECTION

void LayoutWorkplace::ValidateYieldElement(HTML_Element* removed)
{
	if (removed && yield_element && removed == yield_element)
		yield_element = yield_element->Parent();
}

#ifdef LAYOUT_YIELD_REFLOW

void LayoutWorkplace::MarkYieldCascade(HTML_Element* from)
{
	LogicalDocument* logdoc = doc->GetLogicalDocument();

	if (logdoc && yield_element)
	{
		LayoutInfo info(this);

		Head* cascade = &yield_cascade;

		if (yield_element != from)
		{
			HTML_Element* ye = yield_element;
			BOOL extra = yield_element_extra_dirty;

			LayoutProperties* last_c = (LayoutProperties*) cascade->Last();

			while (last_c && !last_c->html_element)
				last_c = last_c->Pred();

			OP_ASSERT(last_c && ye == last_c->html_element);

			while (last_c && last_c->html_element)
			{
				if (last_c->html_element && last_c->html_element->GetLayoutBox())
					last_c->html_element->GetLayoutBox()->YieldLayout(info);

				last_c = last_c->Pred();
			}

			yield_element = NULL;
			yield_element_extra_dirty = FALSE;

			if (extra)
				ye->MarkExtraDirty(doc);
			else
				ye->MarkDirty(doc);
		}

		LayoutProperties* top_cascade = (LayoutProperties*) cascade->First();

		OP_ASSERT(top_cascade);

		if (top_cascade)
			top_cascade->Suc()->RemoveCascade(info);
	}
}

#endif // LAYOUT_YIELD_REFLOW

void LayoutWorkplace::SetThisAndChildrenCanYield(BOOL set)
{
	can_yield = set;

	FramesDocElm* ifrm = doc->GetIFrmRoot() ? doc->GetIFrmRoot()->FirstChild() : NULL;

	while (ifrm)
	{
		FramesDocument* cur = ifrm->GetCurrentDoc();

		if (cur && cur->GetLogicalDocument() && cur->GetLogicalDocument()->GetLayoutWorkplace())
			cur->GetLogicalDocument()->GetLayoutWorkplace()->SetThisAndChildrenCanYield(set);

		ifrm = ifrm->Suc();
	}
}

void LayoutWorkplace::SetCanYield(BOOL set)
{
#ifdef LAYOUT_YIELD_REFLOW
	FramesDocument *parent = doc;

	while (parent && parent->IsInlineFrame())
		parent = parent->GetDocManager()->GetParentDoc();

	OP_ASSERT(parent);

	if (parent && parent->GetLogicalDocument() && parent->GetLogicalDocument()->GetLayoutWorkplace())
		parent->GetLogicalDocument()->GetLayoutWorkplace()->SetThisAndChildrenCanYield(set);
#endif // #ifdef LAYOUT_YIELD_REFLOW
}

BOOL LayoutWorkplace::UsingFlexRoot() const
{
	return doc->GetWindow()->GetFlexRootMaxWidth()
		&& doc->GetLayoutMode() == LAYOUT_NORMAL
		&& !keep_original_layout
#ifdef CSS_VIEWPORT_SUPPORT
		&& !doc->GetHLDocProfile()->GetCSSCollection()->GetViewport()->HasProperties()
#endif
#ifdef PAGED_MEDIA_SUPPORT
		&& !HTMLayoutProperties::IsPagedOverflowValue(GetViewportOverflowX())
#endif // PAGED_MEDIA_SUPPORT
		;
}

OpRect LayoutWorkplace::GetLayoutViewport() const
{
	return OpRect(layout_view_x, layout_view_y, layout_view_width, layout_view_height);
}

OpRect LayoutWorkplace::SetLayoutViewPos(LayoutCoord x, LayoutCoord y)
{
	OpRect updated_area;

	if (x == layout_view_x && y == layout_view_y)
		return updated_area;

	VisualDevice* vis_dev = doc->GetVisualDevice();

	if (has_bg_fixed)
		updated_area = vis_dev->GetRenderingViewport();
	else
		if (!fixed_positioned_area.IsEmpty())
		{
			/* Need to invalidate the area covered by fixed positioned
			   elements, at old layout viewport position (since VisualDevice
			   still doesn't know about the new position). Also move the
			   fixed positioned elements rectangle by the scroll amount, since
			   it should be relative to the document origo at any given time.

			   We need to keep the fixed positioned rectangle for later, in
			   case this method is called again before the next repaint
			   (otherwise, we would allow scrolling of the areas covered by
			   fixed positioned elements, which would cause flickering). */

			updated_area = fixed_positioned_area;
			fixed_positioned_area.OffsetBy(x - layout_view_x, y - layout_view_y);
		}

	vis_dev->Update(updated_area.x, updated_area.y, updated_area.width, updated_area.height);

#ifdef DEBUG_PAINT_LAYOUT_VIEWPORT_RECT
	OpRect total(GetLayoutViewport());
	OpRect after(GetLayoutViewport());
	after.x = x;
	after.y = y;
	total.UnionWith(after);
	vis_dev->Update(total.x, total.y, total.width, total.height);
#endif // DEBUG_PAINT_LAYOUT_VIEWPORT_RECT

	layout_view_x = x;
	layout_view_y = y;

	return updated_area;
}

void LayoutWorkplace::RecalculateLayoutViewSize()
{
	Window* win = doc->GetWindow();
	LayoutCoord width;
	LayoutCoord height;

	CalculateNominalLayoutViewSize(width, height);

	LayoutCoord old_layout_view_width = layout_view_width;
	LayoutCoord old_layout_view_height = layout_view_height;
	LayoutCoord old_min_width = GetLayoutViewMinWidth();
	LayoutCoord old_nominal_width = nominal_width;
	LayoutCoord old_mq_width = media_query_width;
	LayoutCoord old_mq_height = media_query_height;

	nominal_width = width;
	nominal_height = height;

#ifdef CSS_VIEWPORT_SUPPORT
	if (doc->IsTopDocument())
	{
		CSS_Viewport* css_viewport = doc->GetHLDocProfile()->GetCSSCollection()->GetViewport();

		double old_zoom = css_viewport->GetZoom();
		double old_min_zoom = css_viewport->GetMinZoom();
		double old_max_zoom = css_viewport->GetMaxZoom();
		BOOL old_user_zoomable = css_viewport->GetUserZoom();

		VisualDevice* vis_dev = doc->GetVisualDevice();

		CSS_DeviceProperties dev_props;

		ViewportController* controller = win->GetViewportController();
		OpViewportInfoListener* info_listener = controller->GetViewportInfoListener();
		OpViewportRequestListener* request_listener = controller->GetViewportRequestListener();

		unsigned int win_width, win_height;
		win->GetCSSViewportSize(win_width, win_height);

		unsigned int desktop_width = controller->GetDesktopLayoutViewportWidth();

		FontAtt log_font;
		g_pcfontscolors->GetFont(OP_SYSTEM_FONT_DOCUMENT_NORMAL, log_font);

		dev_props.device_width = vis_dev->GetScreenWidthCSS();
		dev_props.device_height = vis_dev->GetScreenHeightCSS();
		dev_props.viewport_width = MIN(win_width, UINT_MAX);
		dev_props.viewport_height = MIN(win_height, UINT_MAX);
		dev_props.desktop_width = desktop_width == 0 ? dev_props.viewport_width : desktop_width;
		dev_props.font_size = log_font.GetSize();

		css_viewport->Constrain(dev_props);

		double zoom = css_viewport->GetZoom();
		double min_zoom = css_viewport->GetMinZoom();
		double max_zoom = css_viewport->GetMaxZoom();
		BOOL user_zoomable = css_viewport->GetUserZoom();

		if (min_zoom != old_min_zoom || max_zoom != old_max_zoom || user_zoomable != old_user_zoomable)
		{
			if (min_zoom == CSS_VIEWPORT_ZOOM_AUTO)
				min_zoom = ZoomLevelNotSet;

			if (max_zoom == CSS_VIEWPORT_ZOOM_AUTO)
				max_zoom = ZoomLevelNotSet;

			info_listener->OnZoomLevelLimitsChanged(controller, min_zoom, max_zoom, user_zoomable);
		}

		if (zoom != old_zoom)
		{
			if (zoom == CSS_VIEWPORT_ZOOM_AUTO)
				zoom = ZoomLevelNotSet;

			request_listener->OnZoomLevelChangeRequest(controller, zoom, NULL, VIEWPORT_CHANGE_REASON_NEW_PAGE);
		}
	}
#endif // CSS_VIEWPORT_SUPPORT

	CalculateLayoutViewSize(FALSE, layout_view_width, layout_view_min_width, layout_view_max_width, layout_view_height, media_query_width, media_query_height);

	if (nominal_width != old_nominal_width)
		if (win->GetLimitParagraphWidth())
			doc->MarkContainersDirty();

	if (media_query_width != old_mq_width || media_query_height != old_mq_height)
		doc->GetHLDocProfile()->GetCSSCollection()->DimensionsChanged(old_mq_width, old_mq_height, media_query_width, media_query_height);

	short new_min_width = GetLayoutViewMinWidth();

	BOOL layout_size_changed = new_min_width != old_min_width || layout_view_height != old_layout_view_height;

	if (layout_size_changed)
	{
		if (layout_view_width != old_layout_view_width)
			if (doc->GetERA_Mode() || doc->GetLayoutMode() != LAYOUT_NORMAL)
				doc->CheckERA_LayoutMode(FALSE);

		doc->HandleNewLayoutViewSize();
		RestartContentSizedIFrames();
		root_size_changed = TRUE;
	}
}

BOOL LayoutWorkplace::RecalculateScrollbars(BOOL keep_existing_scrollbars)
{
	VisualDevice* vis_dev = doc->GetVisualDevice();
	BOOL had_vertical = vis_dev->IsVerticalScrollbarOn();
	BOOL had_horizontal = vis_dev->IsHorizontalScrollbarOn();
	BOOL want_vertical;
	BOOL want_horizontal;

#ifdef PAGED_MEDIA_SUPPORT
	LayoutCoord old_page_control_height = page_control_height;

	page_control_height = LayoutCoord(0);

	if (PagedContainer* root_paged_container = GetRootPagedContainer())
	{
		page_control_height = root_paged_container->GetPageControlHeight();
		want_vertical = want_horizontal = FALSE;
	}
	else
#endif // PAGED_MEDIA_SUPPORT
	{
		CalculateScrollbars(want_vertical, want_horizontal);

		if (keep_existing_scrollbars)
		{
			// Cannot remove scrollbars, as that may cause reflow loops.

			if (!want_vertical && vis_dev->IsVerticalScrollbarOn())
				want_vertical = TRUE;

			if (!want_horizontal && vis_dev->IsHorizontalScrollbarOn())
				want_horizontal = TRUE;
		}
	}

	vis_dev->RequestScrollbars(want_vertical, want_horizontal);

	return
		had_vertical != vis_dev->IsVerticalScrollbarOn() ||
		had_horizontal != vis_dev->IsHorizontalScrollbarOn()
#ifdef PAGED_MEDIA_SUPPORT
		|| old_page_control_height != page_control_height
#endif // PAGED_MEDIA_SUPPORT
		;
}

void LayoutWorkplace::RecalculateDeviceMediaQueries()
{
	LayoutCoord old_mq_device_width = media_query_device_width;
	LayoutCoord old_mq_device_height = media_query_device_height;
	media_query_device_width = LayoutCoord(doc->GetVisualDevice()->GetScreenWidthCSS());
	media_query_device_height = LayoutCoord(doc->GetVisualDevice()->GetScreenHeightCSS());
	if (old_mq_device_width != media_query_device_width || old_mq_device_height != media_query_device_height)
		doc->GetHLDocProfile()->GetCSSCollection()->DeviceDimensionsChanged(old_mq_device_width, old_mq_device_height, media_query_device_width, media_query_device_height);
}

void LayoutWorkplace::CalculateFramesetSize()
{
	if (FramesDocElm* frm_root = doc->GetFrmDocRoot())
	{
		RecalculateScrollbars();

		LayoutCoord mq_w, mq_h;

		CalculateLayoutViewSize(FALSE, layout_view_width, layout_view_min_width, layout_view_max_width, layout_view_height, mq_w, mq_h);

		if (!doc->GetDocManager()->GetFrame())
		{
			// Calculate available size for top-level document

			LayoutCoord width;
			LayoutCoord min_width;
			LayoutCoord max_width;
			LayoutCoord height;

			CalculateLayoutViewSize(!frm_root->IsInDocCoords(), width, min_width, max_width, height, mq_w, mq_h);

			/* Not very obvious that using FlexRoot Max Width as a
			   minimum width for the frameset makes sense, but think
			   of it as "virtual screen width". Smart frames are also
			   enabled in flex-root mode. */

			if (UsingFlexRoot())
			{
				LayoutCoord flex_root_width = GetLayoutViewMaxWidth();
				if (width < flex_root_width)
					width = flex_root_width;
			}

			frm_root->SetRootSize(width, height);
		}
	}
}

LayoutCoord LayoutWorkplace::GetAbsScaleViewWidth() const
{
	LayoutCoord width = GetLayoutViewWidth();

#ifdef _PRINT_SUPPORT_
	if (doc->IsPrintDocument())
	{
		PrintDevice* pd = doc->GetWindow()->GetPrinterInfo(TRUE)->GetPrintDevice();
		OP_ASSERT(pd);
		int left, right, top, bottom;
		pd->GetUserMargins(left, right, top, bottom);
		width -= LayoutCoord(pd->ScaleToDoc(left + right));
	}
#endif // _PRINT_SUPPORT_

	return width;
}

BOOL LayoutWorkplace::GetScaleAbsoluteWidthAndPos()
{
	/* Regarding the "normal_era_width != 0" condition: FramesDocElm sometimes provides
	   negative sizes, thanks to HLDocProfile::InsertElementInternal() calling
	   FramesDocElm::SetSize() (via FramesDocument::GetNewIFrame()) with negative values
	   (percentage values are represented as negative values). The correct fix for this
	   is probably to have FramesDocElm NOT call GetNewIFrame() at all. */

	return doc->GetAbsPositionedStrategy() == ABS_POSITION_SCALE && normal_era_width != 0 && normal_era_width > GetAbsScaleViewWidth();
}

int LayoutWorkplace::GetDocumentWidth()
{
	if (HTML_Element* doc_root = doc->GetDocRoot())
		if (Box* root = doc_root->GetLayoutBox())
		{
			AbsoluteBoundingBox box;
			((VerticalBox*) root)->GetBoundingBox(box);
			return box.GetX() + box.GetContentWidth();
		}

	return 0;
}

long LayoutWorkplace::GetDocumentHeight()
{
	if (HTML_Element* doc_root = doc->GetDocRoot())
		if (Box* root = doc_root->GetLayoutBox())
		{
			AbsoluteBoundingBox box;
			((VerticalBox*) root)->GetBoundingBox(box);
			return box.GetY() + box.GetContentHeight();
		}

	return 0;
}

void LayoutWorkplace::SetViewportOverflow(short overflow_x, short overflow_y)
{
	if (!doc->IsPrintDocument())
	{
		viewport_overflow_x = overflow_x;
		viewport_overflow_y = overflow_y;
	}
}

short LayoutWorkplace::GetViewportOverflowX() const
{
	return doc->IsPrintDocument() ? (short) CSS_VALUE_visible : viewport_overflow_x;
}

short LayoutWorkplace::GetViewportOverflowY() const
{
	return doc->IsPrintDocument() ? (short) CSS_VALUE_visible : viewport_overflow_y;
}

void LayoutWorkplace::AddFixedContent(const OpRect& rect)
{
	fixed_positioned_area.UnionWith(rect);
}

void LayoutWorkplace::ResetFixedContent()
{
	fixed_positioned_area.Empty();
	has_bg_fixed = FALSE;
}

void LayoutWorkplace::ERA_LayoutModeChanged(BOOL apply_doc_styles_changed, BOOL support_float, BOOL column_strategy_changed)
{
	HTML_Element* elm = doc->GetDocRoot();

	if (elm && apply_doc_styles_changed)
		elm->MarkPropsDirty(doc);

	while (elm)
	{
		Box* layout_box = elm->GetLayoutBox();
		if (layout_box)
		{
			short float_type = CSS_VALUE_none;
			short display_type = 0;
			short position = 0;

			if (CssPropertyItem* item = CssPropertyItem::GetCssPropertyItem(elm, CSSPROPS_COMMON))
			{
				float_type = CssFloatMap[item->value.common_props.info.float_type];
				display_type = CssDisplayMap[item->value.common_props.info.display_type];
				position = CssPositionMap[item->value.common_props.info.position];
			}

			if ((support_float && float_type != CSS_VALUE_none && !layout_box->IsFloatingBox()) ||
				(!support_float && layout_box->IsFloatingBox()))
				elm->MarkExtraDirty(doc);

			if (!elm->IsExtraDirty())
			{
				elm->MarkDirty(doc);

				if (elm->GetNsType() == NS_HTML)
					switch (elm->Type())
					{
					case Markup::HTE_APPLET:
					case Markup::HTE_EMBED:
					case Markup::HTE_IFRAME:
					case Markup::HTE_OBJECT:
#ifdef CANVAS_SUPPORT
					case Markup::HTE_CANVAS:
#endif
#ifdef MEDIA_HTML_SUPPORT
					case Markup::HTE_VIDEO:
#endif
					case Markup::HTE_IMG:
						elm->MarkExtraDirty(doc);
						break;
					case Markup::HTE_TITLE:
					case Markup::HTE_META:
					case Markup::HTE_BASE:
					case Markup::HTE_LINK:
					case Markup::HTE_STYLE:
					case Markup::HTE_SCRIPT:
						if (apply_doc_styles_changed)
							elm->MarkExtraDirty(doc);
						break;
					case Markup::HTE_AREA:
						if (apply_doc_styles_changed)
							elm->MarkExtraDirty(doc);
						break;
					case Markup::HTE_DIR:
					case Markup::HTE_MENU:
					case Markup::HTE_ADDRESS:
					case Markup::HTE_HTML:
					case Markup::HTE_FORM:
					case Markup::HTE_ISINDEX:
					case Markup::HTE_BLOCKQUOTE:
					case Markup::HTE_LEGEND:
					case Markup::HTE_FIELDSET:
					case Markup::HTE_MARQUEE:
					case Markup::HTE_CENTER:
					case Markup::HTE_DL:
					case Markup::HTE_DT:
					case Markup::HTE_DD:
					case Markup::HTE_UL:
					case Markup::HTE_OL:
					case Markup::HTE_P:
					case Markup::HTE_DIV:
					case Markup::HTE_HR:
					case Markup::HTE_BODY:
					case Markup::HTE_PRE:
					case Markup::HTE_XMP:
					case Markup::HTE_LISTING:
					case Markup::HTE_PLAINTEXT:
						if (apply_doc_styles_changed)
						{
							if (!layout_box->IsBlockBox() ||
								display_type != CSS_VALUE_block ||
								position != CSS_VALUE_static ||
								layout_box->IsPositionedBox())
								elm->MarkExtraDirty(doc);
						}
						break;
					case Markup::HTE_TABLE:
						if (apply_doc_styles_changed)
						{
							if (display_type == CSS_VALUE_table)
							{
								if (!layout_box->IsBlockBox() ||
									position != CSS_VALUE_static ||
									layout_box->IsPositionedBox())
									elm->MarkExtraDirty(doc);
							}
							else
								if (display_type == CSS_VALUE_inline_table)
								{
									if (!layout_box->IsInlineBox() ||
										position != CSS_VALUE_static ||
										layout_box->IsPositionedBox())
										elm->MarkExtraDirty(doc);
								}
								else
									elm->MarkExtraDirty(doc);
						}
						{
							TableContent* table_content = layout_box->GetTableContent();

							if (column_strategy_changed && table_content)
								table_content->SetColumnStrategyChanged();
						}
						break;
					case Markup::HTE_TBODY:
					case Markup::HTE_THEAD:
					case Markup::HTE_TFOOT:
						if (apply_doc_styles_changed &&
							(!layout_box->IsTableRowGroup() ||
							 (display_type != CSS_VALUE_table_row_group && display_type != CSS_VALUE_table_header_group && display_type != CSS_VALUE_table_footer_group)))
							elm->MarkExtraDirty(doc);
						break;
					case Markup::HTE_COL:
					case Markup::HTE_COLGROUP:
						if (apply_doc_styles_changed &&
							(!layout_box->IsTableColGroup() || (display_type != CSS_VALUE_table_column && display_type != CSS_VALUE_table_column_group)))
							elm->MarkExtraDirty(doc);
						break;
					case Markup::HTE_TD:
					case Markup::HTE_TH:
						if (apply_doc_styles_changed &&
							(!layout_box->IsTableCell() || display_type != CSS_VALUE_table_cell))
							elm->MarkExtraDirty(doc);
						break;
					case Markup::HTE_TR:
						if (apply_doc_styles_changed &&
							(!layout_box->IsTableRow() || display_type != CSS_VALUE_table_row))
							elm->MarkExtraDirty(doc);
						break;
					case Markup::HTE_CAPTION:
						if (apply_doc_styles_changed &&
							(!layout_box->IsTableCaption() || display_type != CSS_VALUE_table_caption))
							elm->MarkExtraDirty(doc);
						break;
					case Markup::HTE_LI:
						if (apply_doc_styles_changed &&
							(!(elm->FirstChild() && elm->FirstChild()->GetIsListMarkerPseudoElement())
							|| display_type != CSS_VALUE_list_item))
							elm->MarkExtraDirty(doc);
						break;
					case Markup::HTE_FONT:
						if (apply_doc_styles_changed && elm->GetFontSize() > 3)
						{
							elm->MarkExtraDirty(doc);
							break;
						}
					default:
						if (apply_doc_styles_changed &&
							(display_type != CSS_VALUE_inline || !layout_box->IsInlineBox() || position != CSS_VALUE_static || layout_box->IsPositionedBox()))
							elm->MarkExtraDirty(doc);
						break;
					}
			}

			if (!elm->IsExtraDirty() && doc->GetLayoutMode() == LAYOUT_NORMAL && layout_box->IsAbsolutePositionedBox() && ((Content_Box*) layout_box)->GetContent()->IsShrinkToFit())
				elm->MarkExtraDirty(doc);

			if (!elm->IsExtraDirty())
				layout_box->RemoveCachedInfo();
		}

		if (elm->IsExtraDirty())
			elm = elm->NextSibling();
		else
		{
			elm = elm->Next();
			if (elm && elm->GetIsListMarkerPseudoElement())
				elm = elm->Next();
		}
	}
}

BOOL LayoutWorkplace::HasDirtyContentSizedIFrameChildren()
{
	if (doc->GetIFrmRoot())
	{
		FramesDocElm* elm = doc->GetIFrmRoot()->FirstChild();

		while (elm)
		{
			if (elm->GetCurrentDoc() && elm->GetNotifyParentOnContentChange())
			{
				FramesDocument* d = (FramesDocument*)elm->GetCurrentDoc();

				if (d->GetDocRoot() && (d->GetDocRoot()->IsDirty()
										|| d->GetDocRoot()->IsPropsDirty() || d->GetDocRoot()->HasDirtyChildProps()
										|| yield_element
						))
				{
					return TRUE;
				}
			}
			elm = elm->Suc();
		}
	}
	return FALSE;
}

void LayoutWorkplace::RestartContentSizedIFrames()
{
	if (doc->GetIFrmRoot())
	{
		FramesDocElm* elm = doc->GetIFrmRoot()->FirstChild();

		while (elm)
		{
			if (HTML_Element *iframe_elm = elm->GetHtmlElement())
				if (Box *iframe_box = iframe_elm->GetLayoutBox())
					if (Content *content = iframe_box->GetContent())
						if (content->IsIFrame())
							static_cast<IFrameContent *>(content)->RestartIntrinsicWidthCalculation();

			elm = elm->Suc();
		}
	}
}

OP_STATUS LayoutWorkplace::UnsafeLoadProperties(HTML_Element* element)
{
	return CssPropertyItem::LoadCssProperties(element, g_op_time_info->GetRuntimeMS(), doc->GetHLDocProfile(), doc->GetMediaType());
}

OP_STATUS LayoutWorkplace::ApplyPropertyChanges(HTML_Element* element, int update_pseudo, BOOL recurse, short attr, NS_Type attr_ns, BOOL skip_top_element)
{
	if (element->Parent())
	{
		HLDocProfile* hld_profile = doc->GetHLDocProfile();
		HTML_Document* html_doc = doc->GetHtmlDocument();
		HTML_Element* top_pseudo_element = element;
		HTML_Element* stop_element = NULL;

		if (recurse)
		{
			stop_element = top_pseudo_element;

			int succ_adj = hld_profile->GetCSSCollection()->GetSuccessiveAdjacent() + 1;
			while (stop_element && succ_adj--)
			{
				stop_element = stop_element->SucActualStyle();
				while (stop_element && !Markup::IsRealElement(stop_element->Type()))
					stop_element = stop_element->SucActualStyle();
			}
			if (!stop_element && top_pseudo_element->Parent())
			{
				stop_element = top_pseudo_element->Parent()->NextSiblingActualStyle();
				while (stop_element && !Markup::IsRealElement(stop_element->Type()))
					stop_element = stop_element->NextActualStyle();
			}
			if (skip_top_element)
			{
				top_pseudo_element = top_pseudo_element->NextActualStyle();
				while (top_pseudo_element && !Markup::IsRealElement(top_pseudo_element->Type()))
					top_pseudo_element = top_pseudo_element->NextActualStyle();
			}
		}

		while (top_pseudo_element != stop_element)
		{
			if (update_pseudo != 0)
			{
				int flags = top_pseudo_element->GetCurrentDynamicPseudoClass() & ~update_pseudo;

				if ((update_pseudo & CSS_PSEUDO_CLASS_HOVER) &&
					top_pseudo_element->IsAncestorOf(html_doc->GetHoverPseudoHTMLElement()))
					flags |= CSS_PSEUDO_CLASS_HOVER;

				if ((update_pseudo & CSS_PSEUDO_CLASS_ACTIVE) &&
					top_pseudo_element == html_doc->GetActivePseudoHTMLElement())
					flags |= CSS_PSEUDO_CLASS_ACTIVE;

				if ((update_pseudo & CSS_PSEUDO_CLASS_FOCUS) &&
					doc->ElementHasFocus(top_pseudo_element))
					flags |= CSS_PSEUDO_CLASS_FOCUS;

				if (update_pseudo & CSS_PSEUDO_CLASS_GROUP_FORM)
				{
					if (top_pseudo_element->IsFormElement())
					{
						int old_form_flags = flags & FormValue::GetFormPseudoClassMask();
						FormValue* form_value = top_pseudo_element->GetFormValue();
						OP_ASSERT(form_value); // Since IsFormElement was true
						int new_form_flags = form_value->CalculateFormPseudoClasses(doc, top_pseudo_element);
						if (old_form_flags == new_form_flags && (update_pseudo & ~CSS_PSEUDO_CLASS_GROUP_FORM) == 0)
						{
							// This is quite common. Writing in a form field normally doesn't
							// change its status (though it might)
							return OpStatus::OK;
						}
						flags &= ~FormValue::GetFormPseudoClassMask();
						flags |= new_form_flags;
					}
				} // End check forms stuff

				// Set the updated pseudo class bits...
				top_pseudo_element->SetCurrentDynamicPseudoClass(flags);
			}

			if (update_pseudo == 0 || top_pseudo_element->HasDynamicPseudo())
			{
				int changes = 0;

				top_pseudo_element->MarkPropsDirty(doc);

				if (attr != Markup::HA_NULL && top_pseudo_element == element)
					changes |= GetAttributeChangeBits(element, attr, attr_ns);

				if (top_pseudo_element->Type() == Markup::HTE_A &&
					((update_pseudo & CSS_PSEUDO_CLASS_ACTIVE) && hld_profile->GetALinkColor() != USE_DEFAULT_COLOR ||
					 (update_pseudo & CSS_PSEUDO_CLASS_LINK) ||
					 (update_pseudo & CSS_PSEUDO_CLASS_VISITED)))
					changes |= PROPS_CHANGED_UPDATE;

				if (changes)
					HandleChangeBits(top_pseudo_element, changes);

				if (hld_profile->GetIsOutOfMemory())
					return OpStatus::ERR_NO_MEMORY;

				if (top_pseudo_element->GetFormObject())
					top_pseudo_element->GetFormObject()->UpdateProperties();
			}

			if (!recurse)
				top_pseudo_element = NULL;
			else
				top_pseudo_element = top_pseudo_element->NextActualStyle();

			// Skip text nodes.
			while (top_pseudo_element && !Markup::IsRealElement(top_pseudo_element->Type()))
				top_pseudo_element = top_pseudo_element->NextActualStyle();
		}
	}
	return OpStatus::OK;
}

int LayoutWorkplace::GetAttributeChangeBits(HTML_Element* element, short attr, NS_Type attr_ns)
{
	/* Ideally, all attributes which affect layout should have changes handled
	   through the normal style system with CSSCollection::GetDefaultStyleProperties
	   storing the CSS properties and CssPropertyItem::LoadCssProperties returning
	   the necessary change bits as for normal CSS styling. Currently, some
	   attributes are handled in HTMLayoutProperties only. Also, some attributes
	   affect presentation, but the presentational attributes are not represented
	   as CSS (for instance replaced content like forms widgets). */

	int change_bits = 0;

	Markup::Type elm_type = element->Type();

	switch (attr_ns)
	{
	case NS_HTML:
		{
			switch (attr)
			{
			/* Attributes whose layout changes are completely handled by change bits
			   returned from LoadCssProperties. These can all be removed when the
			   default case in this switch is fixed to not set any change bits. */
			case Markup::HA_ID:
			case Markup::HA_CLASS:
			case Markup::HA_STYLE:
			case Markup::HA_XML:
			case Markup::HA_WIDTH:
			case Markup::HA_HEIGHT:
			case Markup::HA_BACKGROUND:
			case Markup::HA_BGCOLOR:
			case Markup::HA_TITLE:
			case Markup::HA_CELLSPACING:
			case Markup::HA_CELLPADDING:
			case Markup::HA_NOSHADE:
			case Markup::HA_CLEAR:
			case Markup::HA_DIR:
			case Markup::HA_DIRECTION:
			case Markup::HA_LEFTMARGIN:
			case Markup::HA_TOPMARGIN:
			case Markup::HA_MARGINWIDTH:
			case Markup::HA_MARGINHEIGHT:
			case Markup::HA_HSPACE:
			case Markup::HA_VSPACE:
			case Markup::HA_FRAME:
			case Markup::HA_FRAMEBORDER:
			case Markup::HA_RULES:
			case Markup::HA_BGPROPERTIES:
			case Markup::HA_TEXT:
			case Markup::HA_COLOR:
			case Markup::HA_HREF:
			case Markup::HA_ARIA_ACTIVEDESCENDANT:
			case Markup::HA_ARIA_ATOMIC:
			case Markup::HA_ARIA_AUTOCOMPLETE:
			case Markup::HA_ARIA_BUSY:
			case Markup::HA_ARIA_CHECKED:
			case Markup::HA_ARIA_CONTROLS:
			case Markup::HA_ARIA_DESCRIBEDBY:
			case Markup::HA_ARIA_DISABLED:
			case Markup::HA_ARIA_DROPEFFECT:
			case Markup::HA_ARIA_EXPANDED:
			case Markup::HA_ARIA_FLOWTO:
			case Markup::HA_ARIA_GRABBED:
			case Markup::HA_ARIA_HASPOPUP:
			case Markup::HA_ARIA_HIDDEN:
			case Markup::HA_ARIA_INVALID:
			case Markup::HA_ARIA_LABEL:
			case Markup::HA_ARIA_LABELLEDBY:
			case Markup::HA_ARIA_LEVEL:
			case Markup::HA_ARIA_LIVE:
			case Markup::HA_ARIA_MULTILINE:
			case Markup::HA_ARIA_MULTISELECTABLE:
			case Markup::HA_ARIA_OWNS:
			case Markup::HA_ARIA_POSINSET:
			case Markup::HA_ARIA_PRESSED:
			case Markup::HA_ARIA_READONLY:
			case Markup::HA_ARIA_RELEVANT:
			case Markup::HA_ARIA_REQUIRED:
			case Markup::HA_ARIA_SELECTED:
			case Markup::HA_ARIA_SETSIZE:
			case Markup::HA_ARIA_SORT:
			case Markup::HA_ARIA_VALUEMAX:
			case Markup::HA_ARIA_VALUEMIN:
			case Markup::HA_ARIA_VALUENOW:
			case Markup::HA_ARIA_VALUETEXT:
				break;

			case Markup::HA_ALIGN:
				{
					/* SetCssPropertiesFromHtmlAttr changes computed style for
					   at least text-align. We should figure out if the code
					   below is needed. */

					switch (elm_type)
					{
					// align_block_elements may change.
					case Markup::HTE_DIV:
					case Markup::HTE_CENTER:
					case Markup::HTE_TBODY:
					case Markup::HTE_THEAD:
					case Markup::HTE_TFOOT:
					case Markup::HTE_TD:
					case Markup::HTE_TH:
					case Markup::HTE_TR:
					case Markup::HTE_P:
						change_bits = PROPS_CHANGED_SIZE;
						break;
					default:
						change_bits = PROPS_CHANGED_UPDATE; // text-align.
						break;
					}
				}
				break;

			case Markup::HA_VALIGN:
				{
					/* This is here because because of changes to the computed style
					   done in HTMLayoutProperties::SetCellVAlignFromTableColumn.
					   If those changes can be moved to the UA styles
					   (CSSCollection::GetDefaultStyle()), this code can go. */

					switch (elm_type)
					{
					case Markup::HTE_COL:
					case Markup::HTE_COLGROUP:
					case Markup::HTE_TD:
					case Markup::HTE_TH:
						change_bits = PROPS_CHANGED_SIZE;
					default:
						break;
					}
				}
				break;

			case Markup::HA_LINK:
			case Markup::HA_VLINK:
			case Markup::HA_ALINK:
				/* Inserted anchor elements get their colors from these attributes,
				   through SetCssPropertiesFromHtmlAttr, when present. */
				if (elm_type == Markup::HTE_BODY)
					change_bits = PROPS_CHANGED_UPDATE;
				break;

			case Markup::HA_TYPE:
				{
					/* Because different replaced content objects need to be
					   created. Not sure if this is necessary for all input
					   types and element types below. */
					switch (elm_type)
					{
					case Markup::HTE_BUTTON:
					case Markup::HTE_SELECT:
#ifdef _SSL_SUPPORT_
					case Markup::HTE_KEYGEN:
#endif
					case Markup::HTE_TEXTAREA:
					case Markup::HTE_INPUT:
						change_bits = PROPS_CHANGED_STRUCTURE;
						break;
					}
				}
				break;

			case Markup::HA_DISABLED:
				{
					switch (elm_type)
					{
					case Markup::HTE_BUTTON:
					case Markup::HTE_SELECT:
#ifdef _SSL_SUPPORT_
					case Markup::HTE_KEYGEN:
#endif
					case Markup::HTE_TEXTAREA:
					case Markup::HTE_INPUT:
					case Markup::HTE_OPTION:
					case Markup::HTE_OPTGROUP:
						change_bits = PROPS_CHANGED_STRUCTURE;
						break;
					}
				}
				break;

			case Markup::HA_BORDER:
				/* Retrieving table border for TH and TD in
				   SetCssPropertiesFromHtmlAttr. */
				if (elm_type == Markup::HTE_TABLE)
					change_bits = PROPS_CHANGED_SIZE;
				break;

			case Markup::HA_SIZE:
				{
					switch (elm_type)
					{
					case Markup::HTE_INPUT:
						change_bits |= PROPS_CHANGED_SIZE;
						break;
					case Markup::HTE_SELECT:
						change_bits |= PROPS_CHANGED_STRUCTURE;
						break;
					}
				}
				break;

			case Markup::HA_VALUE:
			case Markup::HA_MIN:
			case Markup::HA_MAX:
			case Markup::HA_LOW:
			case Markup::HA_HIGH:
			case Markup::HA_OPTIMUM:
				if (elm_type == Markup::HTE_METER || elm_type == Markup::HTE_PROGRESS)
				{
					change_bits = PROPS_CHANGED_UPDATE;
					break;
				}
				else
					if (elm_type == Markup::HTE_INPUT)
					{
						if (FormManager::IsButton(element->GetInputType()))
							change_bits = PROPS_CHANGED_SIZE;
						else
							if (element->GetInputType() != INPUT_HIDDEN)
								change_bits = PROPS_CHANGED_UPDATE;
						break;
					}

				change_bits = PROPS_CHANGED_STRUCTURE;
				break;

			case Markup::HA_NOWRAP:
				/* Container::PropagateMinMaxWidths affected by this attribute. */
				if (elm_type == Markup::HTE_TD || elm_type == Markup::HTE_TH)
					change_bits = PROPS_CHANGED_SIZE;
				break;

			/* FIXME: Should really be change_bits = 0. We end up here for the
			   attributes which aren't listed above. Attributes which do not have
			   cases above are currently those we haven't investigated yet.
			   Since we don't know, we nuke everything with a PROPS_CHANGED_STRUCTURE
			   to be on the safe side. */
			default:
				change_bits = PROPS_CHANGED_STRUCTURE;
				break;
			}
		}
		break;

#ifdef SVG_SUPPORT
	case NS_SVG:
		if (element->IsMatchingType(Markup::SVGE_SVG, NS_SVG) && (!element->Parent() || element->Parent()->GetNsType() != NS_SVG))
		{
			// The topmost <svg> element lives in both worlds
			if (attr == Markup::SVGA_OPACITY || attr == Markup::SVGA_DISPLAY)
				change_bits = PROPS_CHANGED_STRUCTURE;
		}
		break;
#endif // SVG_SUPPORT

	default:
		break;
	}

	return change_bits;
}

OP_STATUS LayoutWorkplace::ReloadCssProperties()
{
	HTML_Element* root = doc->GetDocRoot();

	if (!root->IsPropsDirty() && !root->HasDirtyChildProps())
		return OpStatus::OK;

#ifdef SCOPE_PROFILER
	OpTypeProbe probe;

	if (GetFramesDocument()->GetTimeline())
	{
		OpProbeTimeline *timeline = GetFramesDocument()->GetTimeline();
		OpProbeEventType type = PROBE_EVENT_STYLE_RECALCULATION;
		const void *key = timeline->GetUniqueKey();

		RETURN_IF_ERROR(probe.Activate(timeline, type, key));
	}
#endif // SCOPE_PROFILER

	HLDocProfile* hld_profile = doc->GetHLDocProfile();
	BOOL reload_all = root->IsPropsDirty();
	HTML_Element* docprops_elm = 0;
	CSS_MediaType media_type = doc->GetMediaType();
	HTML_Element* elm = root;

#ifdef CSS_ANIMATIONS
	CSS_AnimationManager* animation_manager = hld_profile->GetCSSCollection()->GetAnimationManager();
#endif // CSS_ANIMATIONS
	CSS_MatchContext* match_context = hld_profile->GetCSSCollection()->GetMatchContext();
	CSS_MatchContext::Operation context_op(match_context, doc, CSS_MatchContext::TOP_DOWN, media_type, TRUE, TRUE);

	RETURN_IF_ERROR(hld_profile->GetCSSCollection()->GenerateStyleSheetArray(media_type));

#ifdef CSS_TRANSITIONS
	BOOL has_animations = hld_profile->GetCSSCollection()->HasAnimations();
	BOOL has_transitions = hld_profile->GetCSSCollection()->HasTransitions() || has_animations;
#endif // CSS_TRANSITIONS

	double runtime_ms = g_op_time_info->GetRuntimeMS();

	while (elm)
	{
#ifdef CSS_TRANSITIONS
		BOOL generate_computed = match_context->IsGeneratingComputedStyles();
		BOOL include_children = reload_all || elm->HasDirtyChildProps() || generate_computed;
#else // CSS_TRANSITIONS
		BOOL include_children = reload_all || elm->HasDirtyChildProps();
#endif // CSS_TRANSITIONS

		BOOL loadable_elm = Markup::IsRealElement(elm->Type()) && elm->IsIncludedActualStyle();
		BOOL load_elm = loadable_elm && (reload_all || elm->IsPropsDirty());

#ifdef CSS_TRANSITIONS
		/* Only elements which have had properties loaded before may have
		   transitions start for their properties. */
		BOOL may_transition = has_transitions &&
							  (load_elm || loadable_elm && generate_computed) &&
							  (elm->GetLayoutBox() || elm->GetCssProperties());
# ifdef CSS_ANIMATIONS
		BOOL may_animate = (has_animations &&
							(load_elm || loadable_elm && generate_computed));
# endif // CSS_ANIMATIONS
#endif // CSS_TRANSITIONS

		if (load_elm || include_children && loadable_elm)
		{
			if (!match_context->GetLeafElm(elm))
				return OpStatus::ERR_NO_MEMORY;

#ifdef CSS_TRANSITIONS
			generate_computed = generate_computed || may_transition || has_animations;
			if (generate_computed)
			{
				/* Include children even when they're not dirty because
				   changed inherited computed values may trigger transitions
				   further down the tree. */
				include_children = TRUE;

				RETURN_IF_ERROR(match_context->CalculateComputedStyle(CSS_MatchContext::COMPUTED_OLD, FALSE));
				if (!load_elm)
					RETURN_IF_ERROR(match_context->CalculateComputedStyle(CSS_MatchContext::COMPUTED_NEW, FALSE));
			}
#endif // CSS_TRANSITIONS
		}

		int changes = PROPS_CHANGED_NONE;

		if (load_elm)
		{
#ifdef LAYOUT_YIELD_REFLOW
			if (YieldReloadCssProperties())
				return OpStatus::ERR_YIELD;
#endif // LAYOUT_YIELD_REFLOW

			if (OpStatus::IsMemoryError(CssPropertyItem::LoadCssProperties(elm, runtime_ms, hld_profile, media_type, &changes)))
				return OpStatus::ERR_NO_MEMORY;

#ifdef CSS_TRANSITIONS
			/* Don't apply transitioned values here if may_transition is
				TRUE. This makes sure that we see the new computed value
				without any existing transitions for the previous computed
				value overriding it. */
			if (generate_computed)
				RETURN_IF_ERROR(match_context->CalculateComputedStyle(CSS_MatchContext::COMPUTED_NEW, may_transition));
#endif // CSS_TRANSITIONS
		}
		else
			elm->MarkPropsClean();

#ifdef CSS_ANIMATIONS
		if (may_animate && !elm->IsPropsDirty())
		{
			LayoutProperties* computed_new =
				match_context->GetComputedStyle(CSS_MatchContext::COMPUTED_NEW);
			RETURN_IF_ERROR(animation_manager->StartAnimations(computed_new, runtime_ms));
		}
#endif // CSS_ANIMATIONS

#ifdef CSS_TRANSITIONS
		if (may_transition && !elm->IsPropsDirty())
		{
			LayoutProperties* computed_old = match_context->GetComputedStyle(CSS_MatchContext::COMPUTED_OLD);
			LayoutProperties* computed_new = match_context->GetComputedStyle(CSS_MatchContext::COMPUTED_NEW);

#ifdef CSS_ANIMATIONS
			Head& extra_transitions = animation_manager->CurrentTransitionList();
#else
			Head extra_transitions;
#endif

			/* Check if this element has transition properties that may
				cause transitions to start, or if it had transition
				properties which could require transitions to be stopped. */
			if (computed_old->GetProps()->MayStartTransition() ||
				computed_new->GetProps()->MayStartTransition() ||
				!extra_transitions.Empty())
			{
				if (match_context->GetCurrentLeafElm()->IsDisplayed())
					RETURN_IF_ERROR(transition_manager.StartTransitions(computed_old, computed_new, extra_transitions, runtime_ms, changes));
				else
				{
					transition_manager.AbortTransitions(elm);
					extra_transitions.RemoveAll();
				}
			}

			if (load_elm)
				/* Apply the transitions that were new or already running which
					where not applied in CalculateComputedStyle above. */
				GetTransitionValues(elm, *computed_new->GetProps());
		}
#endif // CSS_TRANSITIONS

		if (changes)
			HandleChangeBits(elm, changes);

		if (HasDocRootProps(elm) && (changes || force_doc_root_props_recalc))
			docprops_elm = elm;

#ifdef CSS_ANIMATIONS
		if (!elm->IsPropsDirty())
#endif // CSS_ANIMATIONS
			elm = context_op.Next(elm, !include_children);
	}

	OP_ASSERT(match_context->IsCommitted());

	if (docprops_elm)
		RETURN_IF_ERROR(RecalculateDocRootProps(docprops_elm));

	OP_ASSERT(!root->HasDirtyChildProps());
	OP_ASSERT(!root->IsPropsDirty());

	return OpStatus::OK;
}

void LayoutWorkplace::HandleChangeBits(HTML_Element* elm, int changes)
{
	HLDocProfile* hld_profile = doc->GetHLDocProfile();
	VisualDevice* vis_dev = hld_profile->GetVisualDevice();

	if (changes & PROPS_CHANGED_STRUCTURE)
		elm->MarkExtraDirty(doc);
	else
		if (changes & (PROPS_CHANGED_SIZE | PROPS_CHANGED_UPDATE_SIZE | PROPS_CHANGED_REMOVE_CACHED_TEXT | PROPS_CHANGED_BOUNDS))
		{
			if (changes & PROPS_CHANGED_REMOVE_CACHED_TEXT)
				elm->RemoveCachedTextInfo(doc);

			const BOOL delete_minmax_widths = !!(changes & (PROPS_CHANGED_SIZE | PROPS_CHANGED_UPDATE_SIZE | PROPS_CHANGED_REMOVE_CACHED_TEXT));
			const BOOL needs_update = !!(changes & (PROPS_CHANGED_UPDATE_SIZE | PROPS_CHANGED_UPDATE | PROPS_CHANGED_BOUNDS));
			elm->MarkDirty(doc, delete_minmax_widths, needs_update);
		}
		else
			if (changes & PROPS_CHANGED_UPDATE)
				if (Box* box = elm->GetLayoutBox())
					if (doc->GetDocRoot()->IsDirty())
						/* This optimisation is based on the assumption that if
						   the document structure is already dirty and has to be
						   reflowed, MarkDirty with update will be cheaper. */

						elm->MarkDirty(doc, FALSE, TRUE);
					else
					{
						// Just update the area

						RECT rect;

#ifdef _DEBUG
						/* We calculate the bounding box of an element while its child element
						   properties haven't been reloaded yet. Traversing calling GetRect() with
						   dirty child props is still OK in this particular case: We'll get to the
						   children with dirty props and reload them later. If it turns out that
						   any of the children needs to be marked dirty, it will cause a reflow,
						   which will ensure that the right area is updated. */

						SetAllowDirtyChildPropsDebug(TRUE);
#endif // _DEBUG

						if (elm->Type() == Markup::HTE_OPTION)
						{
							/* Extra special case for option elements: We want to trigger a repaint on
							   the select element, since it's SelectContent::Paint that applies props on
							   the option. */

							HTML_Element* sel = elm;
							while (sel && sel->Type() != Markup::HTE_SELECT)
								sel = sel->Parent();
							if (sel && sel->GetLayoutBox()->GetRect(doc, BOUNDING_BOX, rect))
								vis_dev->Update(rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1, TRUE);
						}

						if (box->GetRect(doc, BOUNDING_BOX, rect))
							vis_dev->Update(rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1, TRUE);

						if (!fixed_positioned_area.IsEmpty())
							box->InvalidateFixedDescendants(doc);
#ifdef _DEBUG
						SetAllowDirtyChildPropsDebug(FALSE);
#endif // _DEBUG
					}


#ifdef SVG_SUPPORT
	if (changes & (PROPS_CHANGED_SVG))
		if (elm->GetNsType() == NS_SVG)
			g_svg_manager->HandleStyleChange(doc, elm, (unsigned int)changes);
#endif
}

OP_STATUS LayoutWorkplace::RecalculateDocRootProps(HTML_Element* elm)
{
	/* Some element potentially affecting the "global document properties" ("document
	   font" and "document background") has reloaded its properties. Reset current
	   settings and recalculate them. Need the cascade for this. */

	HLDocProfile* hld_profile = doc->GetHLDocProfile();
	HTML_Element* body_elm = hld_profile->GetBodyElm();

#ifdef _PRINT_SUPPORT_
	if (doc->GetLogicalDocument()->GetPrintRoot())
		/* FIXME: we should probably try to find the actual BODY element in the print
		   document here. But setting it to NULL should at least be better than setting
		   to an element in another document (the screen document). */

		body_elm = NULL;
#endif //  _PRINT_SUPPORT_

	HTML_Element* cascade_elm = (elm->Type() == Markup::HTE_HTML && body_elm && !body_elm->IsPropsDirty()) ? body_elm : elm;
	Head prop_list;

	DocRootProperties &doc_root_props = GetDocRootProperties();
	doc_root_props.SetBodyFontData(LayoutFixed(0), -1, USE_DEFAULT_COLOR);

	if (LayoutProperties::CreateCascade(cascade_elm, prop_list, hld_profile))
	{
		Markup::Type old_bg_element_type = doc_root_props.GetBackgroundElementType();

		doc_root_props.ResetBackground();

		for (LayoutProperties* cascade = (LayoutProperties*) prop_list.First()->Suc(); cascade; cascade = cascade->Suc())
			if (HasDocRootProps(cascade->html_element, cascade->html_element->Type()))
				RETURN_IF_ERROR(cascade->SetDocRootProps(hld_profile, cascade->html_element->Type()));

		if (doc_root_props.GetBackgroundElementType() != old_bg_element_type)
		{
			/* Store the fact that we have switched background element
			   for the root until we paint so that we can reset
			   visibility of background images for previous background
			   elements. */

			doc_root_props.SetBackgroundElementChanged();

			/* We have switched element to paint background image
			   for. All bets are off. */

			doc->GetVisualDevice()->UpdateAll();
		}

		force_doc_root_props_recalc = FALSE;
	}
	else
		return OpStatus::ERR_NO_MEMORY;

	prop_list.Clear();

	return OpStatus::OK;
}

void LayoutWorkplace::SetKeepOriginalLayout()
{
	if (!keep_original_layout)
		if (UsingFlexRoot() && doc->GetDocRoot())
			doc->GetDocRoot()->MarkExtraDirty(doc); // flex_root
		else
			doc->MarkContainersDirty(); // text-wrap

	keep_original_layout = TRUE;
	RecalculateLayoutViewSize();
}

BOOL LayoutWorkplace::KeepOriginalLayout()
{
	return keep_original_layout;
}

OP_STATUS LayoutWorkplace::GetParagraphList(const OpRect& rect, Head& list)
{
	RECT r;

	r.top = rect.y;
	r.left = rect.x;
	r.bottom = rect.y + rect.height;
	r.right = rect.x + rect.width;
	TextContainerTraversalObject tcto(doc, r, list);

	HTML_Element* doc_root = doc->GetDocRoot();

	if (!doc_root)
		return OpStatus::ERR;

	return tcto.Traverse(doc_root);
}

OP_STATUS LayoutWorkplace::SearchForElements(ElementSearchCustomizer& customizer,
											 const OpRect& search_area,
											 DocumentElementCollection& elements,
											 const OpRect* extended_area /*= NULL*/)
{
	RECT search_area_rect = {search_area.x, search_area.y,
		search_area.Right(), search_area.Bottom()};
	RECT extended_area_rect;

	if (extended_area)
	{
		extended_area_rect.left = extended_area->x;
		extended_area_rect.top = extended_area->y;
		extended_area_rect.right = extended_area->Right();
		extended_area_rect.bottom = extended_area->Bottom();
	}

	ElementCollectingObject elementCollectingObject(doc, search_area_rect, customizer, extended_area ? &extended_area_rect : NULL);

	if (OpStatus::IsMemoryError(elementCollectingObject.Init()))
		return OpStatus::ERR_NO_MEMORY;

	HTML_Element* root = doc->GetLogicalDocument()->GetRoot();

	if (!root)
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS status = elementCollectingObject.Traverse(root);
	if (OpStatus::IsError(status))
		return status;

	return elementCollectingObject.CollectElements(elements);
}

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
/*virtual*/
BOOL InteractiveItemSearchCustomizer::AcceptElement(HTML_Element* elm, FramesDocument* doc)
{
	if (elm->GetFormObject() && !elm->GetDisabled())
		return TRUE;

	if (elm->IsMatchingType(Markup::HTE_A, NS_HTML)
		|| elm->IsMatchingType(Markup::HTE_AREA, NS_HTML)
		|| elm->IsMatchingType(Markup::HTE_BUTTON, NS_HTML)
#ifdef SVG_SUPPORT
		|| elm->IsMatchingType(Markup::SVGE_A, NS_SVG)
#endif // SVG_SUPPOR
#ifdef _WML_SUPPORT_
		|| elm->IsMatchingType(WE_ANCHOR, NS_WML)
#endif // _WML_SUPPORT_
		)
		return TRUE;

	if ((elm->HasEventHandler(doc, ONCLICK, TRUE)
		|| elm->HasEventHandler(doc, ONMOUSEDOWN, TRUE)
		|| elm->HasEventHandler(doc, ONMOUSEUP, TRUE)
#ifdef TOUCH_EVENTS_SUPPORT
		|| elm->HasEventHandler(doc, TOUCHSTART, TRUE)
		|| elm->HasEventHandler(doc, TOUCHMOVE, TRUE)
		|| elm->HasEventHandler(doc, TOUCHEND, TRUE)
		|| elm->HasEventHandler(doc, TOUCHCANCEL, TRUE)
#endif // TOUCH_EVENTS_SUPPORT
		|| elm->HasEventHandler(doc, ONDBLCLICK, TRUE))
		&& !elm->IsMatchingType(Markup::HTE_HTML, NS_HTML)
		&& !elm->IsMatchingType(Markup::HTE_BODY, NS_HTML)
		&& !elm->IsMatchingType(Markup::HTE_DOC_ROOT, NS_HTML))
		return TRUE;

	return FALSE;
}

OP_STATUS LayoutWorkplace::FindNearbyInteractiveItems(const OpRect& search_area, DocumentElementCollection& elements, const OpRect* extended_area /*= NULL*/)
{
	InteractiveItemSearchCustomizer customizer;

	return SearchForElements(customizer, search_area, elements, extended_area);
}

#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

#ifdef RESERVED_REGIONS
OP_STATUS LayoutWorkplace::GetReservedRegion(const OpRect& rect, OpRegion& region)
{
	RECT area;
	area.top = rect.y;
	area.left = rect.x;
	area.bottom = rect.y + rect.height;
	area.right = rect.x + rect.width;

	ReservedRegionTraversalObject traversal_object(doc, area, region);
	HTML_Element* root = doc->GetDocRoot();
	if (!root)
		return OpStatus::OK;

	return traversal_object.Traverse(root);
}
#endif // RESERVED_REGIONS

LayoutWorkplace::~LayoutWorkplace()
{
	reflow_elements.Clear();
	ClearCounters();
	OP_ASSERT(stored_replaced_content.Empty());
}

#ifdef _PLUGIN_SUPPORT_
BOOL
LayoutWorkplace::DisablePlugins()
{
	BOOL ret_val = FALSE;
	plugins_disabled = TRUE;

	HTML_Element* elm = doc->GetDocRoot();
	while ((elm = elm->Next()))
		if (elm->GetLayoutBox() && elm->GetLayoutBox()->IsContentEmbed())
		{
			elm->MarkExtraDirty(doc);
			if (!ret_val)
				ret_val = TRUE;
		}

	return ret_val;
}

void
LayoutWorkplace::ResetPlugin(HTML_Element *elm)
{
	if (Box* box = elm->GetLayoutBox())
		if (Content *content = box->GetContent())
			if (content->IsEmbed())
				content->Disable(doc);
}
#endif // _PLUGIN_SUPPORT_

#ifdef ON_DEMAND_PLUGIN
void
LayoutWorkplace::ActivatePluginPlaceholder(HTML_Element* placeholder)
{
	placeholder->SetPluginDemanded();

	if (IsReflowing())
		SetReflowElement(placeholder);
	else
		placeholder->MarkExtraDirty(doc);
}
#endif // ON_DEMAND_PLUGIN

void
LayoutWorkplace::HideIFrame(HTML_Element *elm)
{
	if (Box* box = elm->GetLayoutBox())
		if (Content *content = box->GetContent())
			if (content->IsIFrame())
				content->Disable(doc);
}

OP_STATUS
LayoutWorkplace::DrawHighlight(HTML_Element* elm, OpRect* highlight_rects, int num_highlight_rects, const OpRect& clip_rect, HTML_Element* usemap_area /*= NULL*/)
{
	OP_ASSERT(elm);

	OP_BOOLEAN prevent_highlight = OpStatus::OK; // Means 'maybe'.
	short highlight_outline_extent = 0; // See appropriate param description of ShouldElementOutlineDisableHighlight.
	TextSelection* text_selection = doc->GetTextSelection();

	if (num_highlight_rects)
		/* The below is a mysterious condition that is here in order to preserve
		   the previous logic. The corresponding code was previously in HTML_Document. */

		if (!text_selection || text_selection->IsEmpty())
		{
			RETURN_IF_MEMORY_ERROR(prevent_highlight = ShouldElementOutlineDisableHighlight(elm, highlight_outline_extent));

			if (prevent_highlight == OpBoolean::IS_FALSE)
				if (doc->GetVisualDevice()->DrawHighlightRects(highlight_rects, num_highlight_rects, FALSE, elm))
					return OpStatus::OK;
		}

	RECT element_rect;
	Box* box = elm->GetLayoutBox();
	if (!box || !box->GetRect(doc, BOUNDING_BOX, element_rect))
		return OpStatus::ERR; // FIXME: GetRect should provide more detailed failure info.

	if (!clip_rect.IsEmpty())
	{
		OpRect clipped_rect(&element_rect);
		clipped_rect.IntersectWith(clip_rect);
		element_rect = clipped_rect;
	}

	if (usemap_area)
	{
#ifdef SKINNABLE_AREA_ELEMENT
		if (doc->GetWindow()->DrawHighlightRects())
			usemap_area->DrawRegionBorder(doc->GetVisualDevice(), element_rect.left, element_rect.top, element_rect.right - element_rect.left, element_rect.bottom - element_rect.top, USEMAP_BORDER_SIZE, NULL, FALSE, elm);
#else // !SKINNABLE_AREA_ELEMENT
		usemap_area->InvertRegionBorder(doc->GetVisualDevice(), element_rect.left, element_rect.top, element_rect.right - element_rect.left, element_rect.bottom - element_rect.top, USEMAP_BORDER_SIZE, NULL, FALSE, elm);
#endif // SKINNABLE_AREA_ELEMENT
	}
	else
	{
		if (prevent_highlight == OpStatus::OK)
			RETURN_IF_MEMORY_ERROR(prevent_highlight = ShouldElementOutlineDisableHighlight(elm, highlight_outline_extent));

		if (prevent_highlight == OpBoolean::IS_FALSE)
		{
			if (highlight_outline_extent > 0)
			{
				/* We need to inset the rectangle if there is a visible outline that
				   extends outside the border box (because it affects the bounding box).
				   This has two flaws (?):
				   1) If the overflowed content exceeds the outline width and offset in some
				   direction, we shouldn't inset, if we want the highlight outline to be
				   exactly one rectangle.
				   2) If there is a box shadow present, we may get inconsistent highlight
				   geometry depending whether outline is also present or not.

				   This could be avoided if something different than BOUNDING_BOX would be
				   used as a param when element_rect is acquired. */

				element_rect.left += highlight_outline_extent;
				element_rect.right -= highlight_outline_extent;
				element_rect.top += highlight_outline_extent;
				element_rect.bottom -= highlight_outline_extent;
			}

			doc->GetVisualDevice()->DrawHighlightRect(OpRect(&element_rect), FALSE, elm);
		}
	}

	return OpStatus::OK;
}

/** Triggers a repaint of all text/content between start and end */

void
LayoutWorkplace::UpdateSelectionRange(const SelectionBoundaryPoint* start, const SelectionBoundaryPoint* end, RECT* bounding_rect)
{
	OP_ASSERT(!(*start == *end));

	SelectionUpdateObject selection_update(doc, start, end, bounding_rect != NULL);

	if (bounding_rect)
		bounding_rect->bottom = bounding_rect->top = bounding_rect->left = bounding_rect->right = 0;

	selection_update.Traverse(doc->GetLogicalDocument()->GetRoot());

	if (!selection_update.GetStartBoxDone() || !selection_update.GetEndBoxDone())
		/* The traversal failed and either nothing has been invalidated or
		   the wrong things were invalidated (the latter happening in case
		   the end was found before the start). Just repaint the whole screen.
		   Non-optimal but the best we can do. */

		doc->GetVisualDevice()->UpdateAll();
	else
		if (bounding_rect)
			*bounding_rect = selection_update.GetBoundingRect();
}

/**
 * Tests if the HTML element is inside the document area.
 * @param [in] elm HTML element to test.
 * @param [in] doc Document to test if elm is in.
 * @return TRUE if elm is inside doc and FALSE otherwise.
 */
static inline BOOL IsInsideDoc(HTML_Element* elm, FramesDocument* doc)
{
	RECT elm_rect;
	elm->GetLayoutBox()->GetRect(doc, BORDER_BOX, elm_rect);

	int negative_overflow = doc->NegativeOverflow();
	OpRect doc_rect(-negative_overflow, 0, doc->Width() + negative_overflow, doc->Height());
	return doc_rect.Intersecting(OpRect(&elm_rect));
}

static inline BOOL IsStartCandidate(HTML_Element* elm, FramesDocument* doc, BOOL req_inside_doc)
{
	if (elm->GetLayoutBox() && (elm->Type() == Markup::HTE_BR || elm->Type() == Markup::HTE_TEXT && elm->GetLayoutBox()->IsTextBox()
		&& !elm->Parent()->GetIsListMarkerPseudoElement()))
		return !req_inside_doc || IsInsideDoc(elm, doc);

	return FALSE;
}

static inline BOOL IsEndCandidate(HTML_Element* elm, FramesDocument* doc, BOOL req_inside_doc)
{
	if (elm->Type() == Markup::HTE_TEXT && elm->GetLayoutBox() && elm->GetLayoutBox()->IsTextBox()
		&& !elm->Parent()->GetIsListMarkerPseudoElement())
		return !req_inside_doc || IsInsideDoc(elm, doc);

	return FALSE;
}

OP_STATUS
LayoutWorkplace::GetVisualSelectionRange(HTML_Element* element, BOOL req_inside_doc, SelectionBoundaryPoint& visual_start_point, SelectionBoundaryPoint& visual_end_point)
{
	Box* start_box = NULL;
	Box* end_box = NULL;

	if (!IsTraversable())
		RETURN_IF_ERROR(doc->Reflow(FALSE, TRUE));

	if (element->Type() == Markup::HTE_TEXT && element->GetLayoutBox() && element->GetLayoutBox()->IsTextBox())
		end_box = start_box = element->GetLayoutBox();
	else
	{
		HTML_Element* leaf = element->FirstLeafActualStyle();

		while (leaf && element->IsAncestorOf(leaf))
		{
			if (IsStartCandidate(leaf, doc, req_inside_doc))
			{
				start_box = leaf->GetLayoutBox();

				if (start_box->IsEmptyTextBox())
					start_box = NULL;
				else
					break;
			}
#ifdef DOCUMENT_EDIT_SUPPORT
			if (doc->GetDocumentEdit())
				if (leaf->GetLayoutBox() && leaf->GetLayoutBox()->GetContent() &&
					leaf->GetLayoutBox()->GetContent()->IsReplaced())
				{
					start_box = leaf->GetLayoutBox();
					break;
				}
#endif

			leaf = (HTML_Element*)leaf->NextLeaf();
		}

		leaf = element->LastLeafActualStyle();

		while (leaf && element->IsAncestorOf(leaf))
		{
			if (IsEndCandidate(leaf, doc, req_inside_doc))
			{
				OP_ASSERT(leaf->GetLayoutBox()->IsTextBox());
				end_box = leaf->GetLayoutBox();

				if (end_box->IsEmptyTextBox())
					end_box = NULL;
				else
					break;
			}
#ifdef DOCUMENT_EDIT_SUPPORT
			if (doc->GetDocumentEdit())
				if (leaf->GetLayoutBox() && leaf->GetLayoutBox()->GetContent() &&
					leaf->GetLayoutBox()->GetContent()->IsReplaced())
				{
					end_box = leaf->GetLayoutBox();
					break;
				}
#endif

			leaf = (HTML_Element*)leaf->PrevLeaf();
		}
	}

	if (start_box && end_box)
	{
		if (!start_box->IsTextBox())
			visual_start_point.SetLogicalPositionBeforeElement(start_box->GetHtmlElement());

		if (!end_box->IsTextBox())
			visual_end_point.SetLogicalPositionAfterElement(end_box->GetHtmlElement());

#ifdef SUPPORT_TEXT_DIRECTION
		BOOL is_rtl_elm = FALSE;
		if (start_box->IsTextBox() || end_box->IsTextBox())
		{
			if (doc->GetHLDocProfile())
			{
				Head prop_list;
				LayoutProperties* lprops = LayoutProperties::CreateCascade(element, prop_list, doc->GetHLDocProfile()); // FIXME: OOM
				is_rtl_elm = lprops ? lprops->GetProps()->direction == CSS_VALUE_rtl : FALSE;
				prop_list.Clear();
			}
		}

		if (start_box->IsTextBox())
			static_cast<Text_Box*>(start_box)->SetVisualStartOfSelection(&visual_start_point, is_rtl_elm);
		if (end_box->IsTextBox())
			static_cast<Text_Box*>(end_box)->SetVisualEndOfSelection(&visual_end_point, is_rtl_elm);
#else
		if (start_box->IsTextBox())
			static_cast<Text_Box*>(start_box)->SetVisualStartOfSelection(&visual_start_point);
		if (end_box->IsTextBox())
			static_cast<Text_Box*>(end_box)->SetVisualEndOfSelection(&visual_end_point);
#endif // SUPPORT_TEXT_DIRECTION
	}

	return OpStatus::OK;
}

/* static */ BOOL
LayoutWorkplace::HasDocRootProps(HTML_Element *element,
								 Markup::Type element_type /* = Markup::HTE_UNKNOWN */,
								 HTML_Element *parent /* = NULL */)
{
	/* Three kinds of elements qualify as having doc root properties:

	   - The root element in a XML document.
	   - The html element in a (X)HTML document.
	   - The "magic" body element in a (X)HTML document.

	   As an implementation detail we exclude :before and :after elements explicitly. */

	OP_ASSERT(element != NULL);

	if (element->GetIsPseudoElement())
		return FALSE;

	HLDocProfile *hld_profile = doc->GetHLDocProfile();

	if (hld_profile->IsXml() && hld_profile->GetDocRoot() == element)
		return TRUE;

	HTML_Element *first_body_element = hld_profile->GetBodyElm();
	element_type = (element_type == Markup::HTE_UNKNOWN) ? element->Type() : element_type;
	parent = (parent == NULL) ? element->Parent() : parent;

	return (element_type == Markup::HTE_HTML && element->GetNsType() == NS_HTML) ||
		IsMagicBodyElement(element, parent, first_body_element, element_type);
}

/* static */ BOOL
LayoutWorkplace::IsMagicBodyElement(HTML_Element *element, HTML_Element *parent,
									HTML_Element *first_body_element,
									Markup::Type element_type /* = Markup::HTE_UNKNOWN */)
{
	/* The magic body element is defined as the first body element in document. Parent must be
	   html:html.

	   The exception is when the first_body_element is NULL and the element's parent is NULL. Then
	   we assume that the element, of type body, was the first body element before it was
	   removed. */

	OP_ASSERT(element != NULL);

	if (element->GetNsType() != NS_HTML)
		return FALSE;

	element_type = (element_type == Markup::HTE_UNKNOWN) ? element->Type() : element_type;

	if ((element_type != Markup::HTE_BODY) ||
		(first_body_element && element != first_body_element) || // FIXME: will fail for printing
		!parent->IsMatchingType(Markup::HTE_HTML, NS_HTML))

		return FALSE;

	return TRUE;
}

/* static */ class CoreView*
LayoutWorkplace::FindParentCoreView(HTML_Element* html_element)
{
	return Content::FindParentCoreView(html_element);
}

/* static */ class OpInputContext*
LayoutWorkplace::FindInputContext(HTML_Element* html_element)
{
	return Content::FindInputContext(html_element);
}

OP_STATUS
LayoutWorkplace::HandleDocumentTreeChange(HTML_Element* parent, HTML_Element* child, BOOL added)
{
	if (parent->GetUpdatePseudoElm())
		RETURN_IF_ERROR(ApplyPropertyChanges(parent, 0, TRUE));

	if (!added)
	{
		HLDocProfile *hld_profile = doc->GetHLDocProfile();

		if (IsMagicBodyElement(child, parent, hld_profile->GetBodyElm()))
		{
			/* The body element may have contributed to global
			   document properties. Make sure to recalculate them on
			   the next property reload and make sure such a reload
			   takes place */

			force_doc_root_props_recalc = TRUE;
			parent->MarkPropsDirty(doc);

			/* The body element may have been used to paint the canvas
			   background. If so, that area isn't included the
			   bounding box of its parent, so take care of repainting
			   the viewport always when body elements are removed from
			   html elements. */

			doc->GetVisualDevice()->UpdateAll();
		}
	}

	return OpStatus::OK;
}

void
LayoutWorkplace::SwitchPrintingDocRootProperties()
{
	if (active_doc_root_props == &doc_root_props)
		active_doc_root_props = &print_doc_root_props;
	else
		active_doc_root_props = &doc_root_props;
}

OP_BOOLEAN
LayoutWorkplace::HandleInputAction(OpInputAction* action)
{
#ifdef PAGED_MEDIA_SUPPORT
	if (PagedContainer* root_paged_container = GetRootPagedContainer())
		return root_paged_container->HandleInputAction(action);
#endif // PAGED_MEDIA_SUPPORT

	return OpBoolean::IS_FALSE;
}

#ifdef PAGED_MEDIA_SUPPORT

int
LayoutWorkplace::GetCurrentPageNumber()
{
	if (CSS_IsPagedMedium(doc->GetMediaType()) && doc->CountPages() > 1)
	{
		if (PageDescription* current_page = doc->GetPageDescription(doc->GetVisualDevice()->GetRenderingViewY()))
			return current_page->GetNumber() - 1;
	}
	else
		if (RootPagedContainer* root_paged_container = GetRootPagedContainer())
			return root_paged_container->GetCurrentPageNumber();

	return -1;
}

int
LayoutWorkplace::GetTotalPageCount()
{
	if (CSS_IsPagedMedium(doc->GetMediaType()) && doc->CountPages() > 1)
	{
		int count = 0;

		for (PageDescription* page = doc->GetPage(1); page; page = (PageDescription*) page->Suc())
			count ++;

		return count;
	}
	else
		if (RootPagedContainer* root_paged_container = GetRootPagedContainer())
			return root_paged_container->GetTotalPageCount();

	return -1;
}

BOOL
LayoutWorkplace::SetCurrentPageNumber(int page_number, OpViewportChangeReason reason)
{
	if (CSS_IsPagedMedium(doc->GetMediaType()))
	{
		if (doc->CountPages() > 1)
		{
			VisualDevice *vis_dev = doc->GetVisualDevice();

			if (PageDescription* page = doc->GetPage(MIN(page_number + 1, doc->CountPages())))
			{
				long y = page->GetPageTop();

				doc->SetLayoutViewPos(0, y);
				doc->RequestSetVisualViewport(OpRect(0, y, vis_dev->GetRenderingViewWidth(), vis_dev->GetRenderingViewHeight()),
											  reason);
			}

			return TRUE;
		}
	}
	else
		if (RootPagedContainer* root_paged_container = GetRootPagedContainer())
		{
			root_paged_container->SetCurrentPageNumber(page_number, FALSE, TRUE);

			return TRUE;
		}

	return FALSE;
}

RootPagedContainer*
LayoutWorkplace::GetRootPagedContainer() const
{
	if (HTML_Element* root = doc->GetDocRoot())
		if (Box* root_box = root->GetLayoutBox())
			if (PagedContainer* root_paged_container = root_box->GetContainer()->GetPagedContainer())
				return (RootPagedContainer*) root_paged_container;

	return NULL;
}

#endif // PAGED_MEDIA_SUPPORT

LayoutCoord
LayoutWorkplace::FindNormalRightAbsEdge()
{
	HTML_Element* this_root = doc->GetDocRoot();

	if (this_root && this_root->GetLayoutBox())
	{
		LayoutProperties* top_cascade = new LayoutProperties();

		if (top_cascade && top_cascade->InitProps() == OpStatus::ERR_NO_MEMORY)
		{
			delete top_cascade;
			top_cascade = NULL; /* OOM handled below */
		}

		if (top_cascade)
		{
			Head prop_list;
			top_cascade->Into(&prop_list);

			// set ua default values
			top_cascade->GetProps()->Reset(NULL, doc->GetHLDocProfile());

			LayoutCoord right_edge = this_root->GetLayoutBox()->FindNormalRightAbsEdge(doc->GetHLDocProfile(), top_cascade);

			prop_list.Clear();

			return right_edge;
		}
	}

	return LayoutCoord(0);
}

#ifdef DOM_FULLSCREEN_MODE
OP_STATUS
LayoutWorkplace::PaintFullscreen(HTML_Element* element, const RECT& rect, VisualDevice* visual_device)
{
	if (Box* box = element->GetLayoutBox())
		if (Content* content = box->GetContent())
			return content->PaintFullscreen(doc, visual_device, element, rect);

	return OpStatus::ERR;
}
#endif // DOM_FULLSCREEN_MODE

BOOL LayoutWorkplace::IsRendered(HTML_Element* element) const
{
	if (!element->GetLayoutBox())
		return FALSE;

	AutoDeleteHead prop_list;
	LayoutProperties* props = LayoutProperties::CreateCascade(element, prop_list, doc->GetHLDocProfile());
	if (!props)
	{
		doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return FALSE;
	}

	if (props->GetProps()->visibility == CSS_VALUE_visible)
	{
		/* No children of applied visibility:collapse are rendered. Move up
		   the cascade to find out if we're inside something that's collapsed. */

		for (TableCellBox* current_cell = NULL; props && props->html_element; props = props->Pred())
		{
			TableContent* table = NULL;
			Box* box = props->html_element->GetLayoutBox();

			OP_ASSERT(!current_cell || !box->IsTableCell());

			if (!current_cell && box->IsTableCell())
				current_cell = static_cast<TableCellBox*>(box);
			else if (current_cell && (table = box->GetTableContent()))
			{
				TableColGroupBox* colgroup = table->GetColumnBox(current_cell->GetColumn());
				if (colgroup && colgroup->IsVisibilityCollapsed())
					return FALSE;
				current_cell = NULL;
			}
			else if (props->GetProps()->visibility == CSS_VALUE_collapse && (box->IsTableRow() || box->IsTableRowGroup()))
				return FALSE;

		}
		return TRUE;
	}
	else
		return FALSE;
}

/** Count the number of list items of the ancestor ordered list that are in the subtree
	of element.

	@param element The element which list items should be counted.
	@param layout_props The cascade of element.
	@param hld_profile The HLDocProfile for the document.
	@param count (out) The number of list items.
	@return OpStatus::OK on success or OpStatus::ERR_NO_MEMORY on OOM. */

static OP_STATUS
CountOrderedListItemsRec(HTML_Element* element, LayoutProperties* layout_props, HLDocProfile* hld_profile, unsigned& count)
{
	HTML_Element* iter = element->NextActual();
	HTML_Element* stop_at = element->NextSiblingActual();
	while (iter != stop_at)
	{
		if (iter->GetNsType() != NS_HTML)
		{
			iter = iter->NextActual();
			continue;
		}

		Markup::Type type = iter->Type();

		if (type == Markup::HTE_OL || type == Markup::HTE_UL || type == Markup::HTE_P)
			iter = iter->NextSiblingActual(); // skip nested lists and <p> elements
		else if (type == Markup::HTE_LI)
		{
			/* A list item should only be counted if it has display:list-item and
			   all of its ancestors have display:inline. This is because:

			   1. All block elements typically establish a new container, which acts as a new list.
				  The list item is therefore part of the new list and not the parent ordered list.

			   2. Similarily when any of the ancestors have display:none all its descending
				  elements won't be laid out and the list item is not considered to be part
				  of the ordered list.

			   3. Ancestors with a display property different than inline/none/block is really
				  strange and we therefore treat them the same way as we do with display:block
				  and display:none ancestors. */

			LayoutProperties* props = layout_props->GetChildProperties(hld_profile, iter);
			RETURN_OOM_IF_NULL(props);

			/* See if any of our ancestors is not inline and in that case skip to the
			   next sibling of that ancestor. */
			LayoutProperties* props_iter;
			for (props_iter = layout_props->Suc(); props_iter != props; props_iter = props_iter->Suc())
			{
				if (props_iter->GetProps()->display_type != CSS_VALUE_inline)
				{
					iter = props_iter->html_element->NextSiblingActual();
					break;
				}
			}

			/* Didn't find ancestor that wasn't inline. */
			if (props_iter == props)
			{
				HTMLayoutProperties* htmprops = props->GetProps();

				if (htmprops->display_type == CSS_VALUE_list_item)
					count++;
				else if (htmprops->display_type == CSS_VALUE_inline)
					RETURN_IF_MEMORY_ERROR(CountOrderedListItemsRec(iter, props, hld_profile, count));

				iter = iter->NextSiblingActual();
			}
		}
		else
			iter = iter->NextActual();
	}

	return OpStatus::OK;
}

OP_STATUS
LayoutWorkplace::CountOrderedListItems(HTML_Element* element, unsigned& count)
{
	OP_ASSERT(element->Type() == Markup::HTE_OL);

	AutoDeleteHead props_list;
	LayoutProperties* ol_props;

	ol_props = LayoutProperties::CreateCascade(element, props_list, doc->GetLogicalDocument()->GetHLDocProfile());
	RETURN_OOM_IF_NULL(ol_props);

	count = 0;
	return CountOrderedListItemsRec(element, ol_props, doc->GetLogicalDocument()->GetHLDocProfile(), count);
}
