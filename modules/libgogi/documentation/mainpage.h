/** @mainpage The libgogi Module

@section Overview
'libgogi' (or MDE which is currently the library's internal name) is a small and memory-safe
graphics-toolkit.
It handles invalidating, updating, clipping, views, windows, fonts and rendering of lines,
rectangles, bitmaps (etc.). It requires only a pointer to the framebuffer (or backbuffer)
from the platform back-end.

@section api API & internal functions

The API is a mix of C and C++. There is 4 different parts, described below.
C++ classes are used for more highlevel objects, such as views and sprites.
C is used for more lowlevel systems, such as buffers, painting and blitting.
Data in objects might be public for easy *read* access, but should not be written to from outside.

It is possible to use only the lowlevel C API's for buffers/drawing/blitting and not using the Viewsystems.

@subsection buffer Buffer

	C functions for handling buffers.
	The target for all painting functions is a MDE_BUFFER. A buffer is simply a struct containing a pointer to some data, and info about that data.
	The data might belong to the buffer itself, or to the graphics card (or whatever).
	It is also used for bitmaps, that can be drawn on other MDE_BUFFER's.

@subsection painting Painting

	C functions for drawing routines.
	The functions are declared in the header (mde.h) and implemented in mde.cpp in portable code.
	They are not dependant of colorformats.

@subsection blitting Blitting and Formats

	C callbacks for manipulating pixels in different color formats.
	The painting code gets a C-callback which it should use, based on the source and destination format of the pixels.
	Each C-callback has to handle a number of different blending modes and format conversions.
	All currently supported colorformats are implemented in mde_blit.cpp.

@subsection viewsystem Viewsystem/Clipping/Updating

	C++ classes for view and sprite, region, and screen representation.
	MDE_Screen must be inherited to implement the required backend for the display. There is no factory or create-function to create your MDE_Screen implementation.
				It is assumed that you implement it right from the baseclass, so it will be linked in the binary.
	MDE_View and MDE_Sprite can be inherited to do your own drawing. Drawing is then done when needed, based on the updating system.

@section memory Memory, globals & heap usage

The only place MDE is allocating memory internally is in the region for the updatesystem. If this happen you will get
a call to MDE_Screen::OutOfMemory. Read about MDE_Screen::OutOfMemory for more info.
There is no global or static variables used and stack and heap usage is low.

@section updating Updating system.

A view is currently painted in several steps (multiple calls to OnPaint). One for each part of the visible region.
This is bad for applications with slow paintprocessing but good for applications with fast paintprocessing (since the
drawingfunctions is faster). As long as the view that is painted isn't partly covered it is the fastest method.
This behaviour might change in the future if needed.

Each view has a rectangle and a region to keep track of what is invalid. Several invalidations in the same area is added as a union
to the current invalidated area. If a invalidation is far away from the current invalidated area, it will use the region to minimize
the area to repaint.

The updating of views has to be trigged from the backend. Read about MDE_Screen for more info.

@section colorformats Color formats

To support a new colorformat you have to implement some basic callbacks with the blit-methods you want to support.
This is done in mde_blit.cpp.

*/
