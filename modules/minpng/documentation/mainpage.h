/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_MINPNG_DOCUMENTATION_MAINPAGE_H
#define MODULES_MINPNG_DOCUMENTATION_MAINPAGE_H

/** @mainpage minpng module

<p>minpng consists of a png decoder as well as a simple encoder. Usage of both
encoder and decoder are fairly straight-forward and also sort of similar.</p>


<h3>The decoder (minpng.h)</h3>


<p>Let's kick off with the decoder part! The decoder supports decoding of all
image formats specified in the png specification, this includes greyscale, 
truecolour, indexed-colour, greyscale with alpha and truecolour with alpha,
all with the bit-depths listed in the specification. It also decodes animated
pngs (APNG) if FEATURE_APNG is enabled (which defines APNG_SUPPORTED).</p>

<p>While the input data to the decoder should be in the format of a png stream,
the resulting output from minpng will be in either 32-bit RGBA/BGRA format 
(depending on if PLATFORM_COLOR_RGBA is defined or not, see struct RGBAGroup
in minpng.h), or in 8-bit indexed format with a corresponding palette.
The output will be in indexed format if the input format is indexed-color or
grayscale with bpp <= 8 AND if there are no pixel-values with alpha where 
0 < alpha < 255.</p>

<p>The decoding is centralized around the function minpng_decode. This function
takes two pointers as arguments - a pointer to a struct PngFeeder and a pointer
to a struct PngRes. PngFeeder contains foremost a pointer to an user-supplied 
part of the png stream, this is used for "feeding" minpng with png data. The
decoded scanlines of image data will then be ouputted my minpng_decode to the
result structure, PngRes. In general, minpng_decode should be called multiple
times for decoding a single png image, see the api documentation for PngRes,
PngFeeder and minpng_decode for more details.</p>

<p>The only other functions to use in the context of decoding are functions for
initializing and destroying the PngRes and PngFeeder structs (minpng_clear_result, 
minpng_clear_feeder, minpng_init_result and minpng_init_feeder).</p>

<p>minpng may output indexed image data. If defined, PngRes::image_data.rgba or
PngRes::image_data.indexed must be used instead of PngRes::image. The user of 
minpng should also check PngRes::image_frame_data_pal, PngRes::has_transparent_col 
and PngRes::transparent_index to interpret the returned image data.</p>

<h3>The encoder (minpng_encoder.h)</h3>


<p>The encoder isn't very feature-packed for the moment, it takes 32-bit RGBA data 
as input and outputs a png stream with colortype 2 (truecolor) or 6 (truecolor
with alpha) using 8 bits per colorsample (24/32 bpp).</p>

<p>The api for using the encoder is very similar to the one for the decoder. The
central function for the encoder is minpng_encode which takes two pointers as
arguments, a pointer to a struct PngEncFeeder which "feeds" the encoder with
user-supplied scanlines of RGBA data, and a pointer to a struct PngEncRes to 
where minpng_encode gives the user a pointer to output png data. In general, 
minpng_encode should be called multiple times for encoding a single png image, 
see the api documentation for PngEncRes, PngEncFeeder and minpng_encode for more 
details.</p>

<p>The only other functions to use in the context of encoding are functions for
initializing and destroying the PngEncRes and PngEncFeeder structs 
(minpng_clear_encoder_result, minpng_clear_encoder_feeder, minpng_init_encoder_result 
and minpng_init_encoder_feeder).</p>

*/

#endif // !MODULES_HARDCORE_DOCUMENTATION_MAINPAGE_H
