/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef VIEWPORTCONTROLLER_H
#define VIEWPORTCONTROLLER_H

#include "modules/windowcommander/OpViewportController.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/dochand/viewportlisteners.h"
#include "modules/dochand/win.h"

class FramesDocument;

class ViewportController : public OpViewportController
{
public:
	ViewportController(Window* window) :
		window(window),
		default_viewport_request_listener(window->GetWindowCommander()),
		viewport_request_listener(&default_viewport_request_listener),
		viewport_info_listener(&null_viewport_info_listener),
		desktop_layout_width(0),
		desktop_layout_height(0),
		visual_viewport_size_locked(TRUE),
		buffered_mode(FALSE),
		max_paragraph_width(-1),
		visual_viewport_width(0),
		visual_viewport_height(0),
		buffered_rendering_view_x(INT_MIN),
		buffered_rendering_view_y(INT_MIN),
		buffered_layout_view_x(INT_MIN),
		buffered_layout_view_y(INT_MIN)
#ifdef PAGED_MEDIA_SUPPORT
		, current_page_number(-1)
		, total_page_count(-1)
#endif // PAGED_MEDIA_SUPPORT
		{ }

	OpViewportRequestListener* GetViewportRequestListener() const { return viewport_request_listener; }
	OpViewportInfoListener* GetViewportInfoListener() const { return viewport_info_listener; }

	OpRect ConvertToToplevelRect(FramesDocument* frm_doc, const OpRect& local_rect);
	OpRect ConvertToLocalRect(FramesDocument* frm_doc, const OpRect& toplevel_rect);
	unsigned int GetDesktopLayoutViewportWidth() const { return desktop_layout_width; }
	unsigned int GetDesktopLayoutViewportHeight() const { return desktop_layout_height; }

	/* @return max paragraph width in document coordinates. */
	int	GetMaxParagraphWidth() { return max_paragraph_width; }

	/*
	 * Find the innermost document that this point points to (may be an iframe or frame document)
	 *
	 * @param p[in/out] point in document coordinates. returns the point in document
	 *					coordinates relative to the innermost found framesdocument
	 * @param allow_svg If TRUE the resulting document may contain only SVG markup.
	 *					If FALSE the resulting document must not only contain SVG markup.
	 *
	 * @return the innermost FramesDocument that the given position points at.
	 */
	FramesDocument* FindDocumentAtPosAndTranslate(OpPoint& p, BOOL allow_svg = TRUE);

	// Implementing OpViewportController:
	virtual void SetViewportRequestListener(OpViewportRequestListener* listener);
	virtual void SetViewportInfoListener(OpViewportInfoListener* listener);
	virtual void LockVisualViewportSize(BOOL lock);
	virtual void SetBufferedMode(BOOL buffered_mode);
	virtual void FlushBuffer();
	virtual void SetDesktopLayoutViewportSize(unsigned int width, unsigned int height);
	virtual OpRect GetVisualViewport();
	virtual void SetVisualViewport(const OpRect& viewport);
	virtual void SetVisualViewportPos(const OpPoint& pos);
#ifdef PAGED_MEDIA_SUPPORT
	virtual void SetCurrentPageNumber(int page_number);
	virtual int GetCurrentPageNumber();
	virtual int GetTotalPageCount();
#endif // PAGED_MEDIA_SUPPORT
	virtual void SetTextParagraphWidthLimit(unsigned int max_paragraph_width, const OpPoint* pos = NULL);
	virtual void DisableTextParagraphWidthLimit(const OpPoint* pos = NULL);
	virtual OpRect GetRenderingViewport();
	virtual void SetRenderingViewportPos(const OpPoint& pos);
	virtual void SetRenderingScale(unsigned int scale_percentage);
	virtual void SetTrueZoomBaseScale(unsigned int scale_percentage);
	virtual OpRect GetLayoutViewport();
	virtual void GetDocumentSize(unsigned int* width, unsigned int* height);
	virtual void GetParagraphRects(const OpRect& rect, Head* paragraph_rects);
	virtual BOOL IsVisualViewportSizeLocked() const { return visual_viewport_size_locked; }
#ifdef RESERVED_REGIONS
	virtual OP_STATUS GetReservedRegion(const OpRect& rect, OpRegion& reserved_region);
#endif // RESERVED_REGIONS

	/** Finds the innermost HTML_Element at a screen position.
	 *
	 * @param screen_pos[in]       The screen position
	 * @param target_doc[out]      The FramesDocument at the given screen position (NULL if no document)
	 * @param document_pos[out]    The document position relative target_doc (undefined if no document);
	 *
	 * @return The HTML_Element at the given screen position, or NULL if no HTML_Element found.
	 */
	HTML_Element* FindElementAtScreenPosAndTranslate(const OpPoint& screen_pos, FramesDocument* &target_doc, OpPoint &document_pos);

#ifdef PAGED_MEDIA_SUPPORT
	void SignalPageChange(int current, int total);
#endif // PAGED_MEDIA_SUPPORT

#ifdef SELFTEST
	BOOL		HasDefaultViewportRequestListener() const {return viewport_request_listener == &default_viewport_request_listener;}
#endif // SELFTEST

private:
	Window* window;
	DefaultViewportRequestListener default_viewport_request_listener;
	NullViewportInfoListener null_viewport_info_listener;
	OpViewportRequestListener* viewport_request_listener;
	OpViewportInfoListener* viewport_info_listener;
	unsigned int desktop_layout_width;
	unsigned int desktop_layout_height;
	BOOL visual_viewport_size_locked;
	BOOL buffered_mode;
	int max_paragraph_width;
	OpPoint visual_viewport_pos;
	unsigned int visual_viewport_width;
	unsigned int visual_viewport_height;

	int buffered_rendering_view_x;
	int buffered_rendering_view_y;
	int buffered_layout_view_x;
	int buffered_layout_view_y;

#ifdef PAGED_MEDIA_SUPPORT
	int current_page_number;
	int total_page_count;
#endif // PAGED_MEDIA_SUPPORT

	void HandleVisualViewportChange(const OpRect& viewport);
	void GetVisualViewportSizeFromWindow(unsigned int& width, unsigned int& height);
};

#endif // VIEWPORTCONTROLLER_H
