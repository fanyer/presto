/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_ZLIB_DOCUMENTATION_MAINPAGE_H
#define MODULES_ZLIB_DOCUMENTATION_MAINPAGE_H

/** @mainpage zlib module

@section api API

The ZLib module provides two main types of functionality, deflation and inflation. Deflation is used to compress data and inflation is used to decompress data. the key functions in the respective areas are highlighted below.


@subsection deflate Deflate

::deflateInit - initiates stream for compression<br>
::deflateInit2 - like deflateInit but with more compression options<br>
::deflate - compress more input | provide more output<br>
::deflateEnd - frees all allocated memory for stream


@subsection inflate Inflate

::inflateInit - initiates stream for decompression<br>
::inflateInit2 - like inflateInit but with extra parameter (windowBits)<br>
::inflate - decompress more input | provide more output<br>
::inflateEnd - frees all allocated memory for stream

<a href="../index.html">Back to the ZLib documentation start page</a>

*/

#endif // !MODULES_ZLIB_DOCUMENTATION_MAINPAGE_H
