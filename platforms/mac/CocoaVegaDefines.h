/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */


#if defined(VEGA_INTERNAL_FORMAT_BGRA8888)
// ARGB+ByteOrder32Little = BGRA
#define kCGBitmapByteOrderVegaInternal kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst
#elif defined(VEGA_INTERNAL_FORMAT_RGBA8888)
// RGBA
#define kCGBitmapByteOrderVegaInternal kCGImageAlphaPremultipliedLast
#else
#error Implement
#endif // VEGA_INTERNAL_FORMAT_*
