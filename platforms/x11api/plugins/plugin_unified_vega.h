/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-12 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef PLUGIN_UNIFIED_VEGA_H_
#define PLUGIN_UNIFIED_VEGA_H_

#include "platforms/x11api/plugins/plugin_unified_windowed.h"

#if defined(VEGA_OPPAINTER_SUPPORT) && defined(MDE_NATIVE_WINDOW_SUPPORT)

#include "modules/libgogi/pi_impl/mde_native_window.h"
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "platforms/utilix/x11_all.h"

class X11WindowManager;

/**
 * VEGA window for a windowed plug-in instance.
 */
class PluginUnifiedVega :
	public PluginUnifiedWindowed,
	public MDENativeWindow
{
public:
	PluginUnifiedVega(const OpRect& rect, int scale, OpView* parent_view, OpWindow* parent_window) :
		PluginUnifiedWindowed(rect, scale, parent_view, parent_window),
		MDENativeWindow(MDE_MakeRect(rect.x, rect.y, rect.width, rect.height)),
		m_is_visible(false),
		m_needs_flush(false),
		m_redirect_mode(NO_REDIRECT),
		m_original_parent(0),
		m_x11_window_manager(0),
		m_static_image(0) { }

	~PluginUnifiedVega();

	virtual OP_STATUS Init(X11Types::Display*);

	void Detach();
	void Hide();
	void Show();

	void SetPos(const int x, const int y);
	void SetSize(const int w, const int h);

	/*****************************
	 * Implement MDENativeWindow *
	 *****************************/

	void MoveWindow(int x, int y);
	void ResizeWindow(int w, int h);
	void ShowWindow(BOOL show);

	void SetClipRegion(MDE_Region* region);
	void SetRedirected(BOOL should_redirect);

	void UpdateBackbuffer(OpBitmap* backbuffer);

	/// Trigger an update of the window. Needed to prevent "lag" when scrolling
	/// on a page with plug-ins.
	void UpdateWindow();

protected:
	bool m_is_visible;
	bool m_needs_flush;

	enum {
		/// Redirection mode is disabled.
		NO_REDIRECT,

		/// Redirection with a window manager that supports opacity.
		/// With this mode the plugin window is placed off-screen (with
		/// m_original_window_pos containing the saved window position)
		/// and reparented to the root window (where m_original_parent
		/// contains the original parent window). On disabling
		/// redirection mode the original window position and the
		/// original parent are restored.
		REDIRECT_WITH_OPACITY,

		/// Redirection with a static image. With this mode the plugin
		/// window is hidden and only a static image of the plugin is
		/// displayed.
		REDIRECT_WITH_STATIC_IMAGE
	} m_redirect_mode;


	/// If m_redirect_mode is REDIRECT_WITH_OPACITY, then this attribute
	/// contains the parent window of the plugin window, before the
	/// window is reparented to the root window. On disabling
	/// redirection mode, the original parent is restored to this value.
	/// For any other redirection mode, this variable is a nullptr.
	X11Types::Window m_original_parent;

	/// The X11WindowManager instance is needed to test if opacity is
	/// available and to place the plugin-window off-screen (if
	/// possible).
	X11WindowManager* m_x11_window_manager;

	/// If redirection mode is enabled, then this attribute contains a
	/// snapshot of the current contents of the plugin window.
	/// UpdateBackbuffer() uses this image to paint the contents of the
	/// plugin window onto the backbuffer. If the m_redirect_mode is
	/// REDIRECT_WITH_OPACITY, then UpdateBackbuffer() can update this
	/// image from the off-screen window.
	XImage* m_static_image;

	/// If m_redirect_mode is REDIRECT_WITH_OPACITY, then this attribute
	/// contains the last position of the plugin window, before the
	/// window is placed off-screen. On disabling redirection mode,
	/// the window position is restored to this value. For any other
	/// redirection mode, this variable is undefined.
	OpPoint m_original_window_pos;

	/// UpdateImage() restricts the static image to the current clip
	/// rectangle. This attribute is calculated as the union of all
	/// rectangles in the last call to ClipPlugin() to get a minimal
	/// rectangle that covers the clipped region.
	///
	/// \note If the clipped region is empty, this rectangle is empty as well.
	OpRect m_clip_rect;

	/// The clip-rectangle of the plugin-window at the time we took the
	/// static image. If m_clip_rect is empty when we update the image,
	/// this rectangle is equal to the size of the plugin window (with
	/// position (0,0)). On updating the backbuffer in
	/// UpdateBackbuffer() we can copy the static image into the area
	/// defined by this rectangle and paint the rest of the backbuffer
	/// black.
	OpRect m_static_image_rect;

	/// Copies the content of the XImage that is stored in
	/// m_static_image into the specified backbuffer.
	///
	/// If there is no static image available,
	/// OpStatus::ERR_NULL_POINTER is returned. If the backbuffer is not
	/// compatible with the XImage, then OpStatus::ERR_NOT_SUPPORTED is
	/// returned.
	///
	/// \param backbuffer is the buffer in which to store the
	/// 	m_static_image. The buffer is expected to have 32 Bpp and the
	/// 	same size as the plugin window and thus the same size as the
	/// 	m_static_image.
	/// \param window_width, window_height specifies the size of the
	/// 	plugin window - it is expected to be the same size as the
	/// 	backbuffer's size.
	/// \retval OpStatus::ERR_NOT_SUPPORTED if the formats of the XImage and the backbuffer are not compatible.
	/// \retval OpStatus::ERR_NULL_POINTER if m_static_image is 0.
	/// \retval OpStatus::OK on success.

	OP_STATUS CopyXImageToOpBitmap(OpBitmap* backbuffer);

	/// This method restores the original parent, the original position
	/// and the opacity level of the plugin window. This method is
	/// called by SetRedirected() on disabling redirection mode for a
	/// plugin with REDIRECT_WITH_OPACITY mode.
	void RestorePluginParent();

	/// Stores a new XImage from the plugin window in m_static_image.
	void UpdateImage();
};

inline void PluginUnifiedVega::SetPos(const int x, const int y)
{
	m_plugin_rect.x = x;
	m_plugin_rect.y = y;
	Move(x, y);
}

inline void PluginUnifiedVega::SetSize(const int w, const int h)
{
	m_plugin_rect.width  = w;
	m_plugin_rect.height = h;
	Resize(w, h);
}

inline void PluginUnifiedVega::ResizeWindow(int w, int h)
{
	PluginUnifiedWindowed::SetSize(w, h);
}

#endif  // defined(VEGA_OPPAINTER_SUPPORT) && defined(MDE_NATIVE_WINDOW_SUPPORT)
#endif  // PLUGIN_UNIFIED_VEGA_H_
