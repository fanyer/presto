/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef LIBVEGA_CAPABILITIES_H
#define LIBVEGA_CAPABILITIES_H

#define VEGA_CAP_XZIBIT_2

#define VEGA_CAP_FILLOPACITY_IN_FILLS

#define VEGA_CAP_PREMULTIPLIED_IMAGES

#define VEGA_CAP_OPTIONAL_COLOR_ALPHA

#define VEGA_CAP_INT_ARGS

// The canvas has the transform and setTransform functions
#define VEGA_CAP_CANVAS_TRANSFORM

// VEGARenderer has the moveRect function
#define VEGA_CAP_MOVE_RECT

// The canvas has the GetBackBuffer function which can be used to remove a redundant copy when drawing a canvas to another canvas
#define VEGA_CAP_CANVAS_GET_BACK_BUFFER

// The canvas has a Realize function which can be used to ensure that a backing store is created
#define VEGA_CAP_CANVAS_HAS_REALIZE

// The canvas does not have a pointer to the html element, so it must be sent
// to some of the function calls on the canvas
#define VEGA_CAP_CANVAS_NO_HTML_ELEMENT_POINTER

// VEGARenderer has an applyFilter method that takes a VEGAFilterRegion
#define VEGA_CAP_APPLYFILTER_REGION

// VegaOpPainter has SetImageOpacity
#define VEGA_CAP_PAINTER_SETIMAGEOPACITY

// VegaOpPainter has a ClearRect function
#define VEGA_CAP_OPPAINTER_CLEAR_RECT

// VEGARenderTarget has a getTargetWindow function
#define VEGA_CAP_RENDERTARGET_TARGET_WINDOW

#endif // !LIBVEGA_CAPABILITIES_H
