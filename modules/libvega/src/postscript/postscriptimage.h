/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#ifndef POSTSCRIPT_IMAGE_H
#define POSTSCRIPT_IMAGE_H

#ifdef VEGA_POSTSCRIPT_PRINTING

class OpFileDescriptor;
class PostScriptBody;

/** @brief Helper class that generates PostScript image data
  */
class PostScriptImage
{
public:
	/** Constructor
	  * @param _bitmap Bitmap to paint
	  * @param _source Image source information
	  * @param _dest Image destination information
	  */
	PostScriptImage(const class OpBitmap* _bitmap, const OpRect& _source, OpPoint _dest);

	OP_STATUS generate(PostScriptBody& body);

private:
	/** Encode a 4 byte tuple as Ascii85 (5 bytes of ascii per 4 bytes)
	  * @param tuple tuple with 4 bytes to encode
	  * @param dest destination character array, should be 6 bytes, null terminated on return
	  * @return size of resulting tuple in bytes
	  */
	int encode85Tuple(UINT32 tuple, char * dest);

	class BitmapLine;

	const class OpBitmap* bitmap;
	const OpRect& source;
	OpRect dest;
};

#endif // VEGA_POSTSCRIPT_PRINTING

#endif // POSTSCRIPT_IMAGE_H
