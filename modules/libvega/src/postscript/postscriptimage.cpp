/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#include "core/pch.h"

#ifdef VEGA_POSTSCRIPT_PRINTING

#include "modules/libvega/src/postscript/postscriptimage.h"
#include "modules/libvega/src/postscript/postscriptbody.h"
#include "modules/libvega/src/vegabackend.h"

#include "modules/pi/OpBitmap.h"


class PostScriptImage::BitmapLine
{
public:
	BitmapLine(const OpBitmap* srcbitmap) : line(0), bitmap(srcbitmap) {}
	~BitmapLine() { OP_DELETEA(line); }
	OP_STATUS init() { line = OP_NEWA(UINT32, bitmap->Width()); return line ? OpStatus::OK : OpStatus::ERR_NO_MEMORY; }
	OP_STATUS readLine(unsigned lineno) { return bitmap->GetLineData(line, lineno); }
	UINT32 operator[] (unsigned i) const { return line[i]; }

private:
	UINT32* line;
	const OpBitmap* bitmap;
};


PostScriptImage::PostScriptImage(const OpBitmap* _bitmap, const OpRect& _source, OpPoint _dest)
	: bitmap(_bitmap)
	, source(_source)
{
	dest.x = _dest.x;
	dest.y = _dest.y;
	dest.width = _source.width;
	dest.height = _source.height;
}


OP_STATUS PostScriptImage::generate(PostScriptBody& body)
{
	OP_NEW_DBG("PostScriptImage::generate()", "psprint");
	OP_DBG(("Image properties: x=%d, y=%d, w=%d, h=%d", dest.x, dest.y, dest.width, dest.height));

	PostScriptBody::GraphicsStatePusher pusher(&body);
	RETURN_IF_ERROR(pusher.push());

	RETURN_IF_ERROR(body.translate(dest.x, dest.y + dest.height));

	RETURN_IF_ERROR(body.scale(dest.width, dest.height));

	RETURN_IF_ERROR(body.addFormattedCommand("%d %d GenerateImageDict", source.width, source.height));
	RETURN_IF_ERROR(body.addCommand("image"));

	BitmapLine line(bitmap);
	RETURN_IF_ERROR(line.init());

	int output_width = 0;
	// Walk through lines in normal PostScript order, ie. bottom to top, left to right
	for (INT32 lineno = source.y; lineno < source.y + source.height; lineno++)
	{
		RETURN_IF_ERROR(line.readLine(lineno));

		for (INT32 x = source.x; x < source.x + source.width; x++)
		{
			char pixel_buf[6]; // ARRAY OK 2010-12-16 wonko
			output_width += encode85Tuple( line[x], pixel_buf );
			RETURN_IF_ERROR(body.write(pixel_buf));
			if ( output_width >= 80 )
			{ 
				output_width = 0;	
				RETURN_IF_ERROR(body.write("\n"));
			}
		}
	}

	RETURN_IF_ERROR(body.write("\n~>\n"));  // image data end marker
	return pusher.pop();
}


// Encode a 4 byte tuple as 5 bytes using Ascii85/Base85 encoding
int PostScriptImage::encode85Tuple(UINT32 tuple, char * dest) 
{
	if (tuple == 0) // 0 is encoded as 'z' to save space
	{
		dest[0] = 'z';
		dest[1] = 0;
		return 1;
	}

	char *s = dest+4;
	for (int i=0; i < 5; i++)
	{
		*s-- = (tuple % 85) + '!';
		tuple /= 85;
	}
	dest[5] = 0;
	return 5;
}



#endif // VEGA_POSTSCRIPT_PRINTING
