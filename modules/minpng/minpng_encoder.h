/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MINPNG_ENCODER_H
#define MINPNG_ENCODER_H

#ifdef MINPNG_ENCODER

/// A slice of the encoded image, passed by pointer to minpng_encode which modifies it
struct PngEncRes
{
    /// Return code from minpng_decode
    enum Result {
	OK,
	AGAIN,
	NEED_MORE,
	OOM_ERROR,
	ILLEGAL_DATA
    };

    /// Returned data buffer
    unsigned char* data;
    /// Size of returned data buffer
    unsigned int data_size;
};

/// The input to minpng_encode 
struct PngEncFeeder
{
	// Internal state.
	void* state;

	/// Input data in the rgba format used internaly by Opera
	UINT32* scanline_data;

	/// Current scanline in the feeder
	unsigned int scanline;

	/// True if the image has alpha
	int has_alpha;

	/// Image dimensions (of whole image)
	unsigned int xsize, ysize;

	// Encryption level to use for png
	int compressionLevel;
};

/** Encode some data to PNG, taken from input, and return the result in res.
 * If encode returns OK everything is done. If it returns AGAIN encode should
 * be called again with the same input data. If it returns NEED_MORE it should 
 * be called again with the next scanline. */
PngEncRes::Result minpng_encode(struct PngEncFeeder* input, struct PngEncRes* res);
/// Initialize an uninitalized struct PngEncFeeder (constructor)
void minpng_init_encoder_feeder(struct PngEncFeeder* res);
/// Clear a previously initialized struct PngEncFeeder (destructor)
void minpng_clear_encoder_feeder(struct PngEncFeeder* res);
/// Initialize an uninitalized struct PngEncRes (constructor)
void minpng_init_encoder_result( struct PngEncRes *res );
/// Clear a previously initialized struct PngEncRes (destructor)
void minpng_clear_encoder_result(struct PngEncRes* res);

#endif // MINPNG_ENCODER

#endif // !MINPNG_ENCODER_H

