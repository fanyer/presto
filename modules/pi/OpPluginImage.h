/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011-12 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPPLUGINIMAGE_H__
#define OPPLUGINIMAGE_H__

#ifdef _PLUGIN_SUPPORT_

typedef UINT64 OpPluginImageID;
class OpWindow;

/**
 * Representation of the page background that transparent plugins can paint into.
 * Depending on the platform implementation, the class might not need to be used
 * so methods are made non-pure virtual.
 */
class OpPluginBackgroundImage
{
public:
	/** Free the system-wide image resource. */
	virtual ~OpPluginBackgroundImage() {}

	virtual OpPluginImageID GetGlobalIdentifier() { return 0; };
};

/**
 * Browser-side handle for a system-wide image resource onto which plug-ins can render.
 */
class OpPluginImage
{
public:
	/** Free the system-wide image resource. */
	virtual ~OpPluginImage() {}

	/**
	 * Create an OpPluginImage object representing a system-wide plug-in image resource.
	 *
	 * This image resource should be suitable for NPAPI plug-ins to render directly into.
	 * If there is a need for a plugin background, it should also be injected here.
	 *
	 * @param[out] out_image    An OpPluginImage instance on success.
	 * @param[out] background   Background image resource to be used behind
	 *                          the plug-in rectangle if the platform needs to
	 *                          simulate transparency. Else return NULL.
	 * @param      plugin_rect  Rectangle of the plug-in in rendering
	 *                          coordinates, relative to the topmost OpView.
	 * @param  target_opwindow  Parent OpWindow for the OpPluginWindow associated
	 *                          with this OpPluginImage. The platform should, where
	 *                          applicable, choose image depth/colormasks etc for its internal
	 *                          image data representation in such a way that conversion
	 *                          overhead during OpPluginImage::Draw() is minimized.
	 *
	 * @return See OpStatus.
	 */
	static OP_STATUS Create(OpPluginImage** out_image, OpPluginBackgroundImage** background, const OpRect& plugin_rect, OpWindow* target_opwindow);

	/**
	 * Draw the OpPluginImage to the OpPainter at the coordinates given by paint_rect.
	 *
	 * With plug-in position as in the example below, the rectangles have values:
	 *  plugin_rect: x: 40, y: 40, width: 100, height: 100
	 *  paint_rect: x: 50 y: 50 width: 50, height: 50
	 *
	 * ┌──────────────────────────────────────────┐
	 * │ (browser window)                         │
	 * │    ┌────────────┐                        │
	 * │    │    Tab     │                        │
	 * │   ┌┴────────────┴────────────────────────┤
	 * │   │         |                 (topmost OpView)
	 * │   │      ┌─ | ───────────────────────────┤
	 * │   │         |                  (parent OpView)
	 * │   │   40,40 v              (i.e. iframe - optional)
	 * │   │-------->┌─────────────────────────┐  │
	 * │   │      │  │ plugin_rect|            │  │
	 * │   │      │  │   100x100  |            │  │
	 * │   │      │  │            |            │  │
	 * │   │      │  │            |            │  │
	 * │   │      │  │      50,50 v            │  │
	 * │   │      │  │----------->┌────────────┤  │
	 * │   │      │  │            │            │  │
	 * │   │      │  │            │ paint_rect │  │
	 * │   │      │  │            │   50x50    │  │
	 * │   │      │  │            │            │  │
	 * │   │      │  │            │            │  │
	 * │   │      │  └────────────┴────────────┘  │
	 * └───┴──────┴───────────────────────────────┘
	 *
	 * @param painter The painter used for painting created bitmap. Its painting
	 *        methods take coordinates relative to the topmost OpView.
	 * @param plugin_rect Rectangle of the plug-in in rendering coordinates,
	 *        relative to the topmost OpView. Same as the one in Create() method.
	 * @param paint_rect Rectangle of the dirty area in rendering coordinates,
	 *        relative to the top-left corner of the plug-in rectangle. Its
	 *        intersection with \p plugin_rect shall be non-empty.
	 *
	 * @return See OpStatus.
	 */
	virtual OP_STATUS Draw(OpPainter* painter, const OpRect& plugin_rect, const OpRect& paint_rect) = 0;

	/**
	 * Return the global identifier of this image.
	 * @return A unique global identifier, accessible across all processes.
	 */
	virtual OpPluginImageID GetGlobalIdentifier() = 0;

	/**
	 * Check for any changes in the area beneath the plugin or in the plugin
	 * area itself and update if deemed necessary.
	 *
	 * Presently (Winter 2011-12), there is no way to get Unix plug-ins to draw
	 * on a 32-bit bitmap. On platforms with this limitation, we can't use the
	 * alpha channel, so we have to simulate transparency by retrieving the
	 * browser-rendered frame (of the animation) and have the plug-in draw using
	 * it as background.
	 *     It is the implementation's responsibility to ensure that the plug-in
	 * process has an up-to-date background to paint over. It can be retrieved
	 * from \p painter, and should be sent to the plug-in process along with the
	 * drawable surface it's supposed to paint on. To improve performance, note
	 * that this is only necessary if the plug-in's window mode is transparent.
	 * In opaque mode, no such operations should take place as the
	 * specification guarantees zero transparency.
	 *     Implementations should keep a copy of the retrieved background image
	 * in order to avoid copying when no changes have occurred.
	 *
	 * @param painter      The painter to retrieve the rendered document from.
	 *                     Its methods take coordinates relative to the topmost
	 *                     OpView.
	 * @param plugin_rect  Size and position of the plugin rectangle.
	 * @param transparent  Whether the plug-in's window mode is transparent
	 *                     (\c true) or opaque (\c false).
	 *
	 * @return See OpStatus.
	 */
	virtual OP_STATUS Update(OpPainter* painter, const OpRect& plugin_rect, bool transparent) = 0;
};

#endif // _PLUGIN_SUPPORT_
#endif // OPPLUGINIMAGE_H__
