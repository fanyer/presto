/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SVG_IMAGE_H
#define SVG_IMAGE_H

#ifdef SVG_SUPPORT

#include "modules/pi/OpBitmap.h"
#include "modules/svg/svg_external_types.h"
#include "modules/layout/layout_fixed_point.h"

class LayoutProperties;
class VisualDevice;
class LogicalDocument;
class AffinePos;
class SVGImage;

class SVGImageRef
{
protected:
	virtual ~SVGImageRef() {}
public:
	/**
	 * Check if document is disconnected.
	 *
	 * @return TRUE if document is disconnected and FALSE otherwise.
	 */
	virtual BOOL IsDocumentDisconnected() const = 0;

	/**
	 * Get the SVGImage for the referenced image.
	 *
	 * Will return NULL if the image is unavailable/freed/disconnected
	 * from the document/not (yet) loaded.
	 */
	virtual SVGImage* GetSVGImage() const = 0;

	/**
	 * Remove the referenced image for removal. May be instant or delayed.
	 */
	virtual OP_STATUS Free() = 0;

	/**
	 * Get URL for the referenced image.
	 */
	virtual URL* GetUrl() = 0;
};

/**
 * This class represents an SVG image as seen from the outside.
 */
class SVGImage
{
public:
	/**
	 * Sets the svg's position in the document.
	 *
	 * @param ctm The position of the SVG image.
	 */
	virtual void SetDocumentPos(const AffinePos &ctm) = 0;

	/**
	 * Check if zoom and pan are allowed for this SVG image.
	 *
	 * @return TRUE if zooming and panning is allowed, FALSE otherwise.
	 */
	virtual BOOL IsZoomAndPanAllowed() = 0;

	/**
	 * Check if declarative animations are currently running.
	 *
	 * @return TRUE if the timeline is running, FALSE otherwise
	 */
	virtual BOOL IsAnimationRunning() = 0;

	/**
	 * Check if there are declarative animations in this svg.
	 *
	 * @return TRUE if there are declarative animations, FALSE otherwise
	 */
	virtual BOOL HasAnimation() = 0;

	/**
	 * Call this if something has happened externally that makes the
	 * rendering state of this SVG irrelevant in a way that isn't already
	 * handled internally. This is an expensive operation so always
	 * look into the possibility of using something less heavy, like
	 * the SVGManager::HandleSVGAttributeChange method.
	 */
	virtual void InvalidateAll() = 0;

	/**
	 * Call this when the rel part of the svg url changes.
	 *
	 * @param rel_part The rel part or NULL if none.
	 * If the url is "http://www.dn.se" then it's NULL.
	 * If the url is "http://www.dn.se#" then it is the empty string. 
	 * If the url is "http://www.dn.se#foo" then it is the string "foo".
	 */
	virtual OP_STATUS SetURLRelativePart(const uni_char* rel_part) = 0;

	/**
	* Call this to find out what size this svg should be rendered in.
	*
	* @param visual_device The VisualDevice to use when resolving values
	* @param props The LayoutProperties to use when resolving values
	* @param enclosing_width The width that encloses the svg, to use when resolving percentage values
	* @param enclosing_height The height that encloses the svg, to use when resolving percentage values
	* @param width The width in pixels (out parameter)
	* @param height The height in pixels (out parameter)
	*/
	virtual OP_STATUS GetResolvedSize(VisualDevice* visual_device, LayoutProperties* props,
							 		  int enclosing_width, int enclosing_height,
									  int& width, int& height) = 0;

	/**
	 * Fetch the intrinsic size of an SVG image. Percentage values may
	 * be returned if specified in the SVG.
	 *
	 * @param cascade The cascade used when resolving values.
	 * @param width The width in pixels (out parameter). A negative value means
	 *              that there is no intrinsic width.
	 * @param height The height in pixels (out parameter).  A negative value means
	 *               that there is no intrinsic height.
	 * @param intrinsic_ratio The relation between width and height in 16.16
	 *                        fixed point format. The value zero means no
	 *                        intrinsic ratio.
	 */
	virtual OP_STATUS GetIntrinsicSize(LayoutProperties *cascade, LayoutCoord &width, LayoutCoord &height, int &intrinsic_ratio) = 0;

	/**
	 * Fetch the viewBox if it exists.
	 *
	 * @param viewBox The viewbox will be returned here
	 * @return OpStatus::ERR if no viewbox or other error, OpStatus::OK on success
	 */
	virtual OP_STATUS GetViewBox(SVGRect& viewBox) = 0;

	/**
	 * Set the animation time of an image to 'ms'. Returns an
	 * OpBoolean telling if an asynchronous event was sent. If that
	 * was the case, the function should be called again when the
	 * event has been handled. Used by the SVGtest module.
	 */
	virtual OP_BOOLEAN SetAnimationTime(int ms) = 0;

	/**
	 * Force the use of a specified time in events and DOM time
	 * control interfaces. This is used from SVGtest in order to get
	 * reliable results each test run.
	 *
	 * @param ms A zero or positive value forces the event time. A
	 * negative value disables the forcing of the event time.
	 */
	virtual void SetVirtualAnimationTime(int ms) = 0;

	/**
	 * Paint the SVG on a OpBitmap. The pointer to the buffer is
	 * returned and is from then on owned by the caller.
	 *
	 * @param bitmap The resulting bitmap
	 * @param ms The animation-time (in milliseconds) used for rendering the snapshot
	 *           (a negative ms mean svg will use the current time for the snapshot)
	 * @param width The width in pixels (a negative width means svg will decide a width)
	 * @param height The height in pixels (a negative height means svg will decide a height)
	 * @param overrideViewBox If non-null then this viewBox will override the one on the root svg element
	 * @return OpStatus::ERR if the image isn't part of the document, OpStatus::OK on success
	 */
	virtual OP_STATUS PaintToBuffer(OpBitmap*& bitmap, int ms, int width, int height, SVGRect* overrideViewBox = NULL) = 0;

	/**
	 * Gets the user zoom. This is 1.0 for a normal zoom level, greater than 1 when zoomed in and
	 * less than 1 when zoomed out.
	 *
	 * @return The user zoom level
	 */
	virtual SVGNumber GetUserZoomLevel() = 0;

	/**
	 * Check if this SVGImage has selected text.
	 *
	 * @return TRUE if there is selected text, FALSE otherwise.
	 */
	virtual BOOL HasSelectedText() = 0;

	/**
	 * Get last know frames-per-second measurement
	 *
	 * @return The last known frames-per-second in fixed point
	 * format. Divide by 2^16 to get real number.
	 */
	virtual UINT32 GetLastKnownFPS() = 0;

	/**
	 * Get target framerate (frames-per-second)
	 *
	 * @return The target framerate
	 */
	virtual UINT32 GetTargetFPS() = 0;

	/**
	 * Set the target framerate (frames-per-second).
	 *
	 * Note that nothing guarantees the framerate will be achieved,
	 * but if the processing is fast enough the actual framerate
	 * should be somewhere close to the target framerate.
	 *
	 * A zero value for fps will be ignored.
	 *
	 * @param fps The (desired) target frames-per-second
	 */
	virtual void SetTargetFPS(UINT32 fps) = 0;

	/**
	 * Moves the svg image to another document. Used from DOM's adoptNode functionality.
	 * 
	 * @param new_document The document to move to.
	 */
	virtual void SwitchDocument(LogicalDocument* new_document) = 0;

	/**
	 * Check if EcmaScript should be enabled for this SVGImage.
	 *
	 * @return TRUE if enabled, FALSE otherwise.
	 */
	virtual BOOL IsEcmaScriptEnabled(FramesDocument* frm_doc) = 0;

	/**
	 * Check if the SVGImage is inside a HTML <img> element.
	 *
	 * @return TRUE if enabled, FALSE otherwise.
	 */
	virtual BOOL IsInImgElement() = 0;

	/**
	 * Check if the SVGImage allows interaction. 
	 * Can be used for checking html:img and backgrounds in CSS.
	 * 
	 * @return TRUE if interactive, FALSE otherwise.
	 */
	virtual BOOL IsInteractive() = 0;

	/**
	 * Check if the SVG uses external resources so that it should be considered insecure.
	 *
	 * @return TRUE if secure, FALSE otherwise.
	 */
	virtual BOOL IsSecure() = 0;

	/**
	 * Called from the layout engine when the corresponding layout box
	 * of the SVG element is being laid out.  This gives the SVG
	 * engine a chance to do a corresponding layout pass of the SVG.
	 *
	 * @param cascade The CSS cascade up to the SVG root element. Should not be NULL.
	 */
	virtual OP_STATUS OnReflow(LayoutProperties* cascade) = 0;

	/**
	 * Called from the layout engine when the size of the layout box has changed.
	 */
	virtual void OnContentSize(LayoutProperties *cascade, int width, int height) = 0;

	/**
	 * Called from the layout engine when the content has changed in
	 * some way, like when an inline image has been loaded.
	 */
	virtual void OnSignalChange() = 0;

	/**
	 * Called from the layout engine when the content is disabled
	 * and focus needs to be released.
	 */
	virtual void OnDisable() = 0;

	/**
	 * Called from the layout engine when the SVG should paint
	 * itself.
	 *
	 * @param visual_device The VisualDevice to paint to
	 * @param cascade The LayoutProperties (the CSS cascade), can be NULL in which case a new cascade will be created
	 * @param area The position and size of the SVG image in unscaled view coordinates
	 */
	virtual OP_STATUS OnPaint(VisualDevice* visual_device,
							  LayoutProperties* cascade,
							  const RECT& area) = 0;

	virtual ~SVGImage() {} /* To keep the noise down */
};

#endif // SVG_SUPPORT
#endif // SVG_IMAGE_H
