#ifndef MINPNG_MINPNG_H // -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
#define MINPNG_MINPNG_H

/**
 * Pixel values are represented in the form of RGBAGroup stucts when minpng is outputting
 * image data in RGBA or BGRA format, in contrast with when the output is in the form of
 * indexed image data. If PLATFORM_COLOR_IS_RGBA is defined the output image data will be
 * in RGBA format, otherwise it will be in BGRA format.
 */
struct RGBAGroup
{
#ifdef PLATFORM_COLOR_IS_RGBA
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
#else
	unsigned char b;
	unsigned char g;
	unsigned char r;
	unsigned char a;
#endif
};

/**
 * A slice of the decoded image, passed by pointer to minpng_decode which modifies it.
 * Use minpng_init_result to clear the struct before doing the first call to minpng_decode.
 * The decoded image data outputted by minpng_decode in PngRes::image_data might be in
 * indexed format. minpng will output indexed image data if one of the following constraints
 * are true:
 *
 * 1. The png image is encoded using indexed color and has at most ONE fully transparent
 *    color (alpha == 0).
 * 2. The png image is using grayscale with a bpp <= 8 bit and if a transparent color is
 *    specified, this color is fully transparent (alpha == 0).
 */
struct PngRes
{
	/** Return code from minpng_decode */
	enum Result
	{
		/**
		 * Decoding of image is finished and image data can be read.
		 */
		OK,

		/**
		 * Decoding of a part of the image succeeded and image data might be available.
		 * BUT due to limited size of an internal buffer was minpng not able to process
		 * all data given in PngFeeder::data. Because of this, you should read the decoded
		 * image data, and then - call minpng_decode again with the same arguments and WITHOUT
		 * changing PngFeeder::data and PngFeeder::len in between.
		 */
		AGAIN,

		/**
		 * Decoding of a part of the image succeeded and image data can be read. BUT the
		 * image is not yet fully decoded so you could later supply consecutive data from
		 * the png stream in PngFeeder and call minpng_decode again.
		 */
		NEED_MORE,

		/** Out of memory... */
		OOM_ERROR,

		/** Illegal png data */
		ILLEGAL_DATA
	};

	/**
	 * Image data returned by minpng_decode for the part of the image that is starting with
	 * scanline PngRes::first_line, the number of scanlines decoded available here is obtained
	 * by PngRes::lines. Scanlines are not aligned in some way, so there are no empty space
	 * between them. As an example - if PngRes::first_line == 10 and PngRes::xsize is 7, then
	 * is image_data.indexed[5*7+3] the color (index) of the pixel with coordinates (x=3,y=15)
	 * in the full decoded image (if the output format from minpng is indexed and PngRes::lines
	 * is >= 6).
	 *
	 * image_data.rgba should be used if minpng is outputting image data in RGBA or BGRA format,
	 * that is - if PngRes::image_frame_data_pal == NULL.
	 * image_data.indexed should be used if minpng is outputting image data in indexed format,
	 * that is - if PngRes::image_frame_data_pal != NULL.
	 */
	union {
		RGBAGroup* rgba;
		UINT8 *indexed;
		void *data;
	} image_data;

#ifdef EMBEDDED_ICC_SUPPORT
	/** A blob of data picked up from an chunk of embedded ICC profile data */
	UINT8* icc_data;
	unsigned int icc_data_len;
#endif // EMBEDDED_ICC_SUPPORT

	/**
	 * Palette if output image is indexed, if image_frame_data_pal == NULL, this means that
	 * the outputted image data is in RGBA or BGRA format. The format of the palette is -
	 * r0,g0,b0,r1,g1,b1,...etc. The number of r,g,b tripples in the palette is obtained from
	 * PngRes::pal_colors. The format of the palette is the same as expected by the ImageFrameData
	 * struct.
	 */
	UINT8 *image_frame_data_pal;

	/** Number of colors in palette, if the decoded image data is indexed */
	int pal_colors;

	/**
	 * If the decoded image data is indexed, then has_transparent==TRUE indicated that
	 * PngRes::transparent_index contains the palette index that should be transparent.
	 */
	BOOL has_transparent_col;

	/**
	 * If the decoded image data is indexed and PngRes::has_transparent_col == TRUE, then
	 * does transparent_index contain the palette index that should be transparent.
	 */
	int transparent_index;

	/**
	 * TRUE if the png image is interlaced, this means that the returned image data might
	 * be a "low resolution" version of the fully decoded image. This does NOT change the
	 * number of scanlines or the lenght of each scanline returned because minpng "scales"
	 * the low resolution image to the size of the fully decoded image. It does however
	 * mean that even though PngRes::first_line+PngRes::lines == PngRes::ysize after return
	 * from minpng_decode the return value might be PngRes::AGAIN or PngRes::NEED_MORE which
	 * implies that you might continue to feed minpng_decode in order to receive more "high
	 * resolution" versions of the image (after next call to minpng_decode PngRes::first_line
	 * will restart from zero).
	 */
	BOOL interlaced;

	/**
	 * Bits per pixel of source data, only informational. This has not very much to do with
	 * the bpp of the returned (decoded) image data wich is either 8 bpp for indexed image
	 * data (when PngRes::image_frame_data_pal != NULL) or 32 bpp for RGBA/BGRA image data
	 * (when PngRes::image_frame_data_pal == NULL).
	 */
	char depth;

	/**
	 * This value is 1 when the outputted image data is in RGBA/BGRA format and some of the
	 * outputted pixels might have an alpha value (A) < 255 (not fully opaque).
	 */
	int has_alpha;

	/**
	 * The y-position from the top of the image of the first scanline of the returned image
	 * data. If, as an example, first_line==7 - then is PngRes::image_data.rgba[2] the color
	 * at position (x=2,y=7) in the fully decoded image (if the outputted image data is in
	 * RGBA/BGRA format).
	 */
	unsigned int first_line;

	/**
	 * Number of scanlines returned in PngRes::image_data. PngRes::first_line+PngRes::lines
	 * is always <= PngRes::ysize.
	 */
	unsigned int lines;

	/**
	 * This is just used internally by the library. It tells minpng_clear_result, which should
	 * be called between calls to minpng_decode, to not delete PngRes::image_data (when
	 * nofree_image==1). The reason for this is that PngRes::image_data is a pointer into
	 * interlaced image data (recorded in PngFeeder::state), this data is necessary to store
	 * when the deinterlacing of the image continues during subsequent calls to minpng_decode
	 * because an interlaced png image is infact NOT stored as several images with increasing
	 * resolution BUT as several (7) "low resolution" versions of the image with disjunct sets
	 * of samples from the fully decoded image so that all 7 together constructs the full image.
	 * Because of this, image data can't be discarded during subsequent calls to minpng_decode.
	 */
	int nofree_image;

	/**
	 * True if the image-data is ready to be drawn and minpng_clear_result to be called, might be
	 * false sometimes for interlaced apng frames.
	 */
	BOOL draw_image;

	/** X-size of the canvas-area where the frames in the animation should be drawn onto. */
	unsigned int mainframe_x;

	/** Y-size of the canvas-area where the frames in the animation should be drawn onto. */
	unsigned int mainframe_y;

	/**
	 * The rectangle in the canvas-area which the current frame should occupy, this will be
	 * (x=0, y=0, width=mainframe_x, y=mainframe_y) for the one and only frame if the png is not
	 * an APNG.
	 */
	OpRect rect;

	/**
	 * Number of frames in the animation as recorded in the acTL chunk, or 1 if the png is not an
	 * animation or !APNG_SUPPORTED.
	 */
	int num_frames;

	/**
	 * The index of the frame in the animation, zero for the one and only frame if the png is not
	 * an apng, or if !APNG_SUPPORTED.
	 */
	int frame_index;

#ifdef APNG_SUPPORT

	/**
	 * True if the current frame should be blended when drawn onto the canvas, or false if all pixel-values
	 * should simply overwrite the previous content.
	 */
	BOOL blend;

	/** Number of times to iterate the animation (zero == forever). */
	int num_iterations;

	/** Duration in sec/100 to show the current frame. */
	int duration;

	/** Disposal-method for the current frame. */
	DisposalMethod disposal_method;

#endif
};

/**
 * This struct contains the input data necessary for minpng_decode to start decode an image
 * (the first time minpng_decode is called) or to continue decoding (on subsequent calls to
 * minpng_decode). It contains both the png image data to decode, supplied by the caller and
 * an internal state used by minpng_decode to keep track of the current stage of the decoding
 * process necessary if minpng_decode must be called several times in order to decode the full
 * image (has the header been read, how many scanlines have been decoded, etc).
 */
struct PngFeeder
{
	/**
	 * This is the internal state used by minpng_decode to keep track of the decoding process
	 * in order to support several calls to min_png decode before the full image has been
	 * decoded. state is internally casted to (struct minpng_state*) defined in png_int.h, see
	 * the documentation for this struct in order to find out the details.
	 */
	struct minpng_state* state;

	/**
	 * Pointer to encoded png data supplied by the caller, this doesn't have to be all png data
	 * for the image, just part of it. For example, the caller can first supply the first 3kb
	 * of png data in the stream, and when minpng_decode returns PngRes::NEED_MORE the caller
	 * can supply the subsequent 5kb of png data, etc. data (and the corresponding PngFeeder::len)
	 * should be updated before calling minpng_decode to "feed" minpng with subsequent parts of
	 * the png stream, starting from the beginning of the stream for the first call. The exception
	 * is during subsequent calls to minpng_decode if the previous call returned PngRes::AGAIN,
	 * then should data and len be untouched before the next call because that data was to large
	 * for minpng to handle in just one call.
	 */
	const unsigned char* data;

	/**
	 * The length of the encoded png data supplied in the data field, update this when data is
	 * updated.
	 */
	unsigned int len;

	/**
	 * Gamma value used to calculate the colors in the outputted image data. minpng_init_feeder
	 * sets this to the default value of 2.2, which implies a gamma of 1/2.2 (in general it's no
	 * need of changing this value). If the png image stores gamma information on it's own the
	 * output gamma will be calculated upon both these values.
	 */
	double screen_gamma;

	/**
	 * If set to TRUE no gamma conversions will be applied.
	 */
	BOOL ignore_gamma;
};

/**
 * This is THE function to use for decoding png data. Call this function as many times necessary
 * in order to decode the full image, or stop somehere in between if you get "bored". See the
 * documentation for PngFeeder and PngRes for more information about arguments + return value,
 * and check out modules/minpng/documentation/index.html for an example of how to use it.
 * @return PngRes::OK if decoding of png image is now finished, PngRes::NEED_MORE if you
 * need to supply more data in input in order to finish decoding of the image, PngRes::AGAIN
 * if minpng_decode wants you to make the next call WITHOUT changing input->data and input->len,
 * and PngRes::OOM_ERROR or PngRes::ILLEGAL_DATA in case of errors. In the cases of PngRes::OK,
 * PngRes::NEED_MORE and PngRes::AGAIN the caller should check res for returned image_data.
 * @param input The input png data, screen gamma and internal state for minpng.
 * @param res minpng_decode will fill this struct with output data, such as decoded image data,
 * image dimensions, etc.
 */
PngRes::Result minpng_decode(struct PngFeeder* input, struct PngRes* res);

/**
 * Clears a previously initialized PngRes struct, this function should ALWAYS be called
 * after a call to minpng_decode and the result has been processed, even though you're about
 * to call minpng_decode again, due to e.g. a return value of PngRes::AGAIN from minpng_decode.
 * @param res The PngRes struct to clear.
 */
void minpng_clear_result(struct PngRes* res);

/**
 * Clears a previously initialized PngFeeder struct, this function should ONLY be called when
 * no more decoding of a png stream should be made - when the last call to minpng_decode for
 * the png stream has been made.
 * @param res The PngFeeder struct to clear.
 */
void minpng_clear_feeder(struct PngFeeder* res);

/**
 * Initializes an uninitalized PngRes struct, this function should ONLY be called before the
 * first call to minpng_decode for a specific png stream.
 * @param res The PngRes struct to initialize.
 */
void minpng_init_result(struct PngRes* res);

/**
 * Initializes an uninitalized PngFeeder struct, this function should ONLY be called before the
 * first call to minpng_decode for a specific png stream.
 * @param res The PngFeeder struct to initialize.
 */
void minpng_init_feeder(struct PngFeeder* res);

#endif // MINPNG_MINPNG_H
