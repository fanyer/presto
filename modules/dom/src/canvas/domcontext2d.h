/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOMCONTEXT2D_H
#define DOMCONTEXT2D_H

#ifdef CANVAS_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/esthread.h"

class DOM_HTMLCanvasElement;
class DOM_Environment;
class CanvasContext2D;
class CanvasGradient;
class CanvasPattern;
class VEGAFill;

class DOMCanvasGradient
	: public DOM_Object
{
protected:
	DOMCanvasGradient(CanvasGradient *grad);

	CanvasGradient *m_gradient;

public:
	~DOMCanvasGradient();

	static OP_STATUS Make(DOMCanvasGradient*& ctx, DOM_Environment* envirinment);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CANVASGRADIENT || DOM_Object::IsA(type); }
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(addColorStop);
	enum {FUNCTIONS_ARRAY_SIZE = 2};

	CanvasGradient *GetCanvasGradient() { return m_gradient; }
};

class DOMCanvasPattern
	: public DOM_Object
{
private:
	DOMCanvasPattern(CanvasPattern *pat);

	CanvasPattern *m_pattern;

	URL m_source_url;
	BOOL m_tainted;
public:
	~DOMCanvasPattern();

	static OP_STATUS Make(DOMCanvasPattern*& ctx, DOM_Environment* envirinment, URL& src_url, BOOL tainted);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CANVASPATTERN || DOM_Object::IsA(type); }
	virtual void GCTrace();

	CanvasPattern *GetCanvasPattern(){return m_pattern;}

	const URL& GetURL(){return m_source_url;}
	BOOL IsTainted(){return m_tainted;}
};

class DOMCanvasImageData : public DOM_Object
{
protected:
	DOMCanvasImageData(unsigned int w, unsigned int h, ES_Object* pix);

public:
	static OP_STATUS Make(DOMCanvasImageData *&image_data, DOM_Runtime *runtime, unsigned int w, unsigned int h, UINT8 *clone_data = NULL);
	/**< Create a DOM_CanvasImageData with the given dimensions. The
	     width and height are in terms of CSS pixels, and the requested
	     size is assumed to have already been validated.

	     @param [out] image_data The created object.
	     @param runtime The runtime in which to create this new ImageData object.
	     @param w The width in CSS pixels.
	     @param h The height in CSS pixels.
	     @param clone_data If non-NULL, initialize contents of ImageData
	            with this data. It must be of identical size.
	     @returns OpStatus::ERR_NO_MEMORY on OOM, OpStatus::OK otherwise. */

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CANVASIMAGEDATA || DOM_Object::IsA(type); }
	virtual void GCTrace();

	unsigned int GetWidth() { return m_width; }
	unsigned int GetHeight() { return m_height; }

	UINT8* GetPixels() { return static_cast<UINT8 *>(GetRuntime()->GetStaticTypedArrayStorage(m_pixel_array)); }

	static OP_STATUS Clone(DOMCanvasImageData *source_object, ES_Runtime *target_runtime, DOMCanvasImageData *&target_object);
#ifdef ES_PERSISTENT_SUPPORT
	static OP_STATUS Clone(DOMCanvasImageData *source_object, ES_PersistentItem *&target_item);
	static OP_STATUS Clone(ES_PersistentItem *&source_item, ES_Runtime *target_runtime, DOMCanvasImageData *&target_object);
#endif // ES_PERSISTENT_SUPPORT

private:
	unsigned int m_width;
	unsigned int m_height;

	ES_Object* m_pixel_array;
};

class DOMCanvasContext2D
	: public DOM_Object
{
protected:
	DOMCanvasContext2D(DOM_HTMLCanvasElement *domcanvas, CanvasContext2D *c2d);

	DOM_HTMLCanvasElement *m_domcanvas;
	CanvasContext2D *m_context;

public:
	~DOMCanvasContext2D();

	static OP_STATUS Make(DOMCanvasContext2D*&  ctx, DOM_HTMLCanvasElement *domcanvas);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CANVASCONTEXT2D || DOM_Object::IsA(type); }
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(save);
	DOM_DECLARE_FUNCTION(restore);

	DOM_DECLARE_FUNCTION(scale);
	DOM_DECLARE_FUNCTION(rotate);
	DOM_DECLARE_FUNCTION(translate);
	DOM_DECLARE_FUNCTION(transform);
	DOM_DECLARE_FUNCTION(setTransform);

	DOM_DECLARE_FUNCTION(createLinearGradient);
	DOM_DECLARE_FUNCTION(createRadialGradient);
	DOM_DECLARE_FUNCTION(createPattern);

	DOM_DECLARE_FUNCTION(clearRect);
	DOM_DECLARE_FUNCTION(fillRect);
	DOM_DECLARE_FUNCTION(strokeRect);

	DOM_DECLARE_FUNCTION(beginPath);
	DOM_DECLARE_FUNCTION(closePath);
	DOM_DECLARE_FUNCTION(moveTo);
	DOM_DECLARE_FUNCTION(lineTo);
	DOM_DECLARE_FUNCTION(quadraticCurveTo);
	DOM_DECLARE_FUNCTION(bezierCurveTo);
	DOM_DECLARE_FUNCTION(arcTo);
	DOM_DECLARE_FUNCTION(rect);
	DOM_DECLARE_FUNCTION(arc);
	DOM_DECLARE_FUNCTION(fill);
	DOM_DECLARE_FUNCTION(stroke);
	DOM_DECLARE_FUNCTION(clip);
	DOM_DECLARE_FUNCTION(isPointInPath);

#ifdef CANVAS_TEXT_SUPPORT
	DOM_DECLARE_FUNCTION(fillText);
	DOM_DECLARE_FUNCTION(strokeText);
	DOM_DECLARE_FUNCTION(measureText);
#endif // CANVAS_TEXT_SUPPORT

	DOM_DECLARE_FUNCTION(drawImage);

	DOM_DECLARE_FUNCTION(createImageData);
	DOM_DECLARE_FUNCTION(getImageData);
	DOM_DECLARE_FUNCTION(putImageData);

	enum { FUNCTIONS_ARRAY_SIZE = 30
#ifdef CANVAS_TEXT_SUPPORT
		   +3
#endif // CANVAS_TEXT_SUPPORT
		   +1 };
};

class VEGASWBuffer;

class CanvasResourceDecoder
{
public:
	enum DecodeStatus {
		DECODE_OK,
		DECODE_FAILED,
		DECODE_OOM,
		DECODE_EXCEPTION_TYPE_MISMATCH,
		DECODE_EXCEPTION_NOT_SUPPORTED,
		DECODE_SVG_NOT_READY
	};
	/** Helper function to decode an image for use in a canvas. Will fail if the resource is not loaded.
	 * @param domres the dom object containing the resource to decode.
	 * @param domcanvas the canvas which the decoded image will be used with.
	 * @param origining_runtime the runtime which issued this decode.
	 * @param dest_width the width in which the decoded image will be drawn.
	 * @param dest_height the height in which the decoded image will be drawn.
	 * @param wantFill flag indicating if the caller prefers a VEGAFill or a OpBitmap.
	 * @param scr_url will contain the URL of the image on successful return.
	 * @param bitmap will contain the image on successful return unless a fill is returned instead or non_premultiplied is not NULL.
	 * @param fill will contain the image on successful return unless a bitmap is returned instead or non_premultiplied is not NULL.
	 * @param non_premultiplied if not NULL the image will be stored in this buffer in non-premultiplied form instead of returning an image or fill.
	 * @param ignoreColorSpaceConversions a flag indicating if the decoding should be done without applying color space conversions or not. If true all images will be re-decoded. Only valid if non_premultiplied is not NULL.
	 * @param img will contain the Image on successful return, might be empty.
	 * @param releaseImage will contain a flag indicating if the caller should call img.ReleaseBitmap and img.DecVisible to free the image or not.
	 * @param releaseBitmap will contain a flag indicating if the caller should delete the returned OpBitmap or not.
	 * @param width will contain the width of the image on successful return.
	 * @param height will contain the height of the image on successful return.
	 * @param tainted will contain a flag indicating if the decoded image is tainted or origin clean on successful return.
	 */
	static DecodeStatus DecodeResource(DOM_Object* domres, DOM_HTMLCanvasElement* domcanvas, DOM_Runtime* origining_runtime,
						   int dest_width, int dest_height, BOOL wantFill, URL& src_url, OpBitmap* &bitmap,
						   VEGAFill* &fill, VEGASWBuffer **non_premultiplied, BOOL ignoreColorSpaceConversions, Image& img, BOOL& releaseImage, BOOL& releaseBitmap,
						   unsigned int& width, unsigned int& height, BOOL& tainted);
};

/** Helper functions to handle colors */
class DOMCanvasColorUtil
{
public:
	static BOOL DOMGetCanvasColor(const uni_char *colstr, UINT32& color);
	static ES_GetState DOMSetCanvasColor(ES_Value *value, UINT32 color);
};

#endif // CANVAS_SUPPORT
#endif // DOMCONTEXT2D_H
