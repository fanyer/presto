/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#ifndef MINPNG_PNG_INT_H
#define MINPNG_PNG_INT_H

/**
 * This is an implementation of a buffer for arbitrary data which's size expands whenever
 * needed.
 */
class minpng_buffer
{
public:

	/**
	* Copies len bytes from data to the buffer, follwing possibly preceding data. Size of
	* buffer will expand if necessary.
	* @return 0 in case of success, otherwise != 0 (error).
	* @param len Number of bytes to copy to the buffer.
	* @param data The data to copy to the buffer.
	*/
	int append(int len, const unsigned char *data);

	/**
	* Returns the number of bytes stored in the buffer (that have not yet been consumed).
	* @return Number of bytes stored in the buffer.
	*/
	int size() const { return len; }

	/**
	 * Retruns a pointer to the first unconsumed byte in the buffer.
	 * @return Pointer to first unconsumed byte in the buffer.
	 */
	unsigned char *ptr();

	/**
	 * Consumes a number of bytes in the buffer.
	 * @param bytes The number of bytes to consume.
	 */
	void consume(int bytes);

	/**
	 * "Reset", discards all data in the buffer and deletes the internal storage.
	 */
	void clear();

	/**
	 * Same as clear(), but does _not_ delete internal storage, i.e,
	 * the allocated buffer is still live. Make sure that a reference
	 * to the internal storage is available before calling, or there
	 * will be a leak.
	 */
	void release();

	minpng_buffer() { data = NULL; clear(); };
	~minpng_buffer() {  clear(); }

private:
	unsigned char *data;
	int allocated;
	int available;
	int len;
	int off;
};

/**
 * The alpha information copied from the tRNS chunk in the png image, see png spec.
 */
struct minpng_trns
{
	/**
	 * Number of BYTES copied from the tRNS chunk, not the number of entries, for indexed
	 * pngs however, those values are the same, see png spec.
	 */
	int len;

	/**
	 * The copied chunk data from the tRNS chunk.
	 */
	unsigned char str[256]; /* ARRAY OK 2007-11-21 timj */
};

/**
 * This class encapsulates a minpng_buffer to which the user appends compressed data in the
 * zlib format and a z_stream object which is used to feed zlib (the library) with compressed
 * data from the minpng_buffer and output it to an user-supplied buffer during calls to
 * minpng_zbuffer::read. So, first - append compressed data with minpng_zbuffer::append, and
 * later - read the uncompressed output with minpng_zbuffer::read.
 */
class minpng_zbuffer
{
public:

	/**
	* Error-codes returned from minpng_zbuffer::read.
	*/
	enum
	{
		/// Incorrect zlib data.
		IllegalData = -1,

		/// The end of the zlib stream has already been reached.
		Eof = -2,

		/// Out-of-memory...
		Oom = -3
	};

	/**
	 * Function for adding zlib compressed data to the minpng_zbuffer, internally, this
	 * function just append the compressed data to the underlying minpng_buffer object.
	 * @return 0 in case of success, otherwise != 0 (error). If the end of the zlib stream
	 * has already been detected during a previous call to minpng_zbuffer::read, then
	 * will the data be discarded but this is not considered an error-condition, so zero
	 * is returned anyway.
	 * @param len Length of the compressed data in bytes of compressed data.
	 * @param data The compressed data to append to the buffer.
	 */
	int append(int len, const unsigned char* data) { return eof ? 0 : input.append(len, data); }

	/**
	 * Returns the number of bytes of compressed data stored in the underlying minpng_buffer.
	 * This is the number of bytes which have NOT yet been uncompressed and returned by any
	 * call to minpng_zbuffer::read.
	 * @return Number of bytes of compressed data which have beed appended but not yet been
	 * decompressed and returned by minpng_zbuffer::read.
	 */
	int size() const { return input.size(); }

	/**
	* Decompresses compressed data which has been previously added to the buffer using
	* minpng_zbuffer::append. As much data as possible is decompressed, but might be limited
	* by the bytes argument which should be the size of the user-supplied buffer.
	* @return How many bytes of uncompressed data that where added to the user-supplied buffer
	* data, OR, if < 0 - an error code which is either minpng_zbuffer::IllegalData,
	* minpng_zbuffer::Eof or minpng_zbuffer::Oom.
	* @param data Where the uncompressed data should be outputted by the zlib library.
	* @param bytes Maximum number of bytes of uncompressed data we want zlib to put in data.
	*/
	int read(unsigned char* data, int bytes);

	/**
	 * Let the zlib library initialize the underlying z_stream object, should be called before
	 * an minpng_zbuffer object is used.
	 * @return PngRes::OK if we where successful or PngRes::OOM_ERROR if we're out-of-memory.
	 */
	PngRes::Result init();

	/**
	 * Re-initializes an initialized minpng_zbuffer, clears input data and the underlaying z_stream
	 * object and initializes it again.
	 * @return PngRes::OK if we where successful or PngRes::OOM_ERROR if we're out-of-memory.
	 */
	PngRes::Result re_init();

	~minpng_zbuffer();
	minpng_zbuffer();

private:
	minpng_buffer input;
	/* Don't have any other variables that has to be accessed often
	** below this point, the code to access them will be larger, due
	** to the large offset.
	*/
	z_stream d_stream;
	int eof;
};

/**
 * This struct informs minpng_decode on how to set the pixel values in the outputted image
 * data during decoding of an interlaced png image. Png uses an interlace method called Adam7
 * which divides the image into 7 "passes" where the passes contain disjunct set of pixels
 * that alltogether forms the full image. Because of this, minpng_decode uses an array of 7
 * interlace_state structs where each struct describes which subset of pixels each pass ("low
 * resolution subimage") contain.
 *
 * For example, the interlace_state struct for the forth pass is:
 *
 * {y0 = 0, yd = 4, ys = 4, x0 = 2, xd = 4, xs = 2}
 *
 * This means that the first scanline of the subimage corresponds to scanline y0=0 in the output
 * image and scanline n in the subimage corresponds to scanline y0 + yd*n (0 + 4*n here) in the
 * output image. Furthermore, the first pixel in each scanline of the subimage corresponds to pixel
 * x0=2 in the full image and the pixels in the scanlines for the subimage corresponds to every
 * forth (xd=4) pixel on the corresponding scanline.
 *
 * minpng_decode "stretches out" the pixels in the not yet fully decoded image when it iterates
 * through the seven passes so that "low-resolution-looking" image data can be outputted before
 * all of the data has been decoded (the idea behind interlacing...).
 * Because of this, ys and xs informs how much each pixel in the subimage should be stretched
 * out. Notice that ys < yd || xs < xd, for all passes after the first, because otherwise we would
 * overwrite all pixels obtained from preceding passes.
 */
struct interlace_state
{
	/**
	 * The scanline in the full image that corresponds to the first scanline in the pass
	 * ("subimage") described by this interlace_state struct.
	 */
	unsigned char y0;

	/**
	 * Scanline n in the pass ("subimage") described by this interlace_state struct corresponds
	 * To scanline y0+yd in the full image.
	 */
	unsigned char yd;

	/**
	 * The height of the "stretched out" pixels "drawed" in the output image data during processing
	 * of the pass ("subimage") that corresponds to this interlace_state struct.
	 */
	unsigned char ys;

	/**
	 * The first pixel in each scanline of the "subimage" corresponds to pixel x0 in the corresponding
	 * scanlines of output image data (the full image).
     */
	unsigned char x0;

	/**
	 * Pixel n in the "subimage" described by this interlace_state struct corresponds to pixel
	 * x0+xd on the corresponding scanlines in the full image.
	 */
	unsigned char xd;

	/**
	 * The width of the "stretched out" pixels "drawed" in the output image data during processing
	 * of the pass ("subimage") that corresponds to this interlace_state struct.
	 */
	unsigned char xs;
};

/**
 * This struct keeps track of a palette which uses the alpha channel. This is a composition
 * of the PLTE and tRNS chunks in png images into one struct.
 */
struct minpng_palette
{
	///Palette entries.
	struct RGBAGroup palette[256]; /* ARRAY OK 2007-11-21 timj */

	///Number of RGBAGroup entries in the palette.
	int size;
};

/**
 * This struct is tracking the current decoding state of an image so that minpng_decode can be
 * called multiple times for the same png stream, if minpng would be implemented as a class, this
 * would be the minpng class, so to say. The pointer to the minpng_state for a decoding session is
 * is stored in void *PngFeeder::state, casted by minpng_decode on each call. A fresh minpng_state
 * is obtained by the static function minpng_state::construct.
 */
struct minpng_state
{
	/**
	 * During processing of the chunks in the png stream, more_needed corresponds to the length field
	 * of each chunk, which is the first 4 bytes of each chunk and measures the size in byte of the
	 * chunk data, see png spec.
	 */
	unsigned int more_needed;

	/**
	 * Stores how much of the data in the buffer minpng_state::current_chunk (copied from PngFeeder->data)
	 * that has been processed and that can be discarded (consumed).
	 */
	unsigned int consumed;

	/**
	 * Any of STATE_INITIAL, STATE_CHUNKHEADER, STATE_gAMA, STATE_PLTE, STATE_IDAT, STATE_tRNS,
	 * STATE_iCCP, STATE_IGNORE or STATE_CRC. Specifies which state in the decoding process we're in.
	 */
	char state;

	/**
	 * If the png is interlaced, this is which of the passes ("subimagees") we're currently processing,
	 * between 0 and 6.
	 */
	char adam_7_state;

	/**
	 * This is the first line NOT yet decoded in the output image, if the png is interlaced, this
	 * is the first line not decoded in the output image for the current pass ("subimage").
	 */
	unsigned int first_line;

	/**
	 * True if minpng should output indexed image data. This is true if the png is indexed and has
	 * at most one color (index) with alpha != 255, this color must also be fully transparent, that
	 * is - alpha == 0. It's also true if the image is using grayscale without alpha (color type 0)
	 * and has a bpp <= 8 bit AND if a tRNS chunk is present in the png, the transparent color has
	 * alpha == 0.
	 * Because png chunks are processed sequentially, this value might be at first set to true during
	 * processing of the IHDR chunk, and later be set to false during processing of the tRNS chunk,
	 * the above mentioned constraints turn out to be false.
	 */
	BOOL output_indexed;

	/**
	 * True if minpng is outputting indexed image data (minpng_state::output_indexed == TRUE) and there
	 * is an color (index) which is transparent.
	 */
	BOOL has_transparent_col;

	/**
	 * If minpng_state::output_indexed == TRUE && minpng_state::has_transparent_col == TRUE, then is
	 * this the index into the palette which should be transparent when drawing the ouput image.
	 */
	int transparent_index;

	/**
	 * This is set to true when the first IDAT chunk is encountered, the idea is to postpone some
	 * tasks (see on_first_idat_found in modules/minpng/minpng.cpp) which depend on that if the png
	 * contain tRNS and/or gAMA chunks, these have already been processed.
	 */
	BOOL first_idat_found;

#ifndef MINPNG_NO_GAMMA_SUPPORT
	/**
	 * Gamma value used for calculating the intensity if the pixels in the output image data.
	 * If this value is 1.0, this implies that no gamma correction is necessary, but infact it will
	 * never be set to (somewhere "close" to) 1.0, but in that case to 0.0 - which indicates that
	 * minpng should not perform any gamma correction. If the gAMA chunk in the png specifies a gamma
	 * ~ 1/2.2 or if there is no gamma chunk (which minpng interpretes as 1/2.2 is specified) AND
	 * the defined screen gamma PngFeeder::gamma is set to 2.2, this implies that input gamma *
	 * output gamma == 1/2.2 * 2.2 == 1.0, which means that this value should be 1.0. In this particular
	 * case will this value be set to 0.0 (as mentioned above), and no gamma correction will be performed.
	 */
	float gamma;
#endif // !MINPNG_NO_GAMMA_SUPPORT

	/**
	 * This should be set to TRUE if gamma should be ignored when decoding.
	 */
	BOOL ignore_gamma;

	/**
	 * This pointer will be copied to PngRes::image_frame_data_pal. If minpng is outputting indexed
	 * image data, this is the palette to use. The format of the palette is like - r0,g0,b0,r1,g1,b1,
	 * etc. If gamma correction is being used, the r,g,b values are corrected according to this.
	 */
	UINT8 *image_frame_data_pal;

	/**
	 * The number of entries (r,g,b tripples) in minpng_state::image_frame_data_pal. If the png is
	 * indexed but minpng is not outputting indexed image data, this value will be the same as
	 * minpng_state::ct->size.
	 */
	int pal_size;

	/**
	* If the png image is indexed but the outputted image data is not (due to use of the alpha in the
	* palette which is not supported by OpBitmap), then does this minpng_palette keep track of the
	* palette together with the corresponding alpha values found in the tRNS chunk. This palette is
	* used during conversion of indexed image data in the png stream to output image data in RGBA or
	* BGRA format.
	*/
	minpng_palette* ct;

	/**
	 * This is a copy of the chunk data in the tRNS chunk. This is NOT used event though the png has
	 * a tRNS chunk if the output image data is indexed, because it's only necessary for convertion
	 * conversion of indexed image data in the png stream to output image data in RGBA or BGRA format.
	 * If gamma correction is being used, the r,g,b values are corrected according to this.
	 */
	minpng_trns* trns;

	/**
	 * If the png image is interlaced, this holds the output image data, a pointer into this data will
	 * then be copied into PngRes::image_data. We must hold this data in the minpng_state as the pixels
	 * from previous Adam7 passes still should be present during later passes which might not be processed
	 * until later calls to minpng_decode.
	 */
	union {
		RGBAGroup* rgba;
		UINT8 *indexed;
		void *data;
	} deinterlace_image;

	/**
	 * To this buffer is data decompressed from minpng_state::compressed_data before it's added to
	 * minpng_state::uncompressed_data. The limitation of the size of this buffer is the reason why
	 * minpng_decode might have to return PngRes::AGAIN.
	 */
	unsigned char* buf;

	/** This is a copy of some of the fields found in the IHDR chunk */
	struct
	{
		unsigned char bpp;
		unsigned char type;
		unsigned char interlace;
	} ihdr;

	/**
	 * This buffer should not be confused with the chunks that the png stream consists of. Instead
	 * this is the "chunk" of data that has been delivered by the caller through PngFeeder::data
	 * during subsequent calls to minpng_decode. In the beginning of each call to minpng_decode,
	 * the data consumed during the previous call (minpng_state::consumed) is consumed, because this
	 * data is not necessary anymore.
	 */
	minpng_buffer current_chunk;

	/**
	 * When decompressing zlib compressed data from IDAT chunks, the decompressed data (scanlines and
	 * compression type for each scanline) is stored in this buffer. This data is later added to the
	 * ouput image data - after deinterlacing, byte unpacking (or indexed-to-RGBA-conversion), etc.
	 */
	minpng_buffer uncompressed_data;

#ifndef MINPNG_NO_GAMMA_SUPPORT
	/**
	 * If gamma correction is being used (minpng_state::gamma != 0.0), then this lookup table is used
	 * for converting the color sample values (r,g,b) for each pixel are converted using this table.
	 * For indexed png images, only the palette colors (in minpng_state::image_frame_data_pal or
	 * minpng_state::ct->palette), this is also true for grayscale images where the output image data
	 * is indexed. In other cases, the r,g,b values of each pixel in the output image data is converted
	 * using this table. The conversion is made like this:
	 *
	 * void gamma_correct(RGBAGroup &col, minpng_state *s)
	 * {
	 *		col.r = s->gamma_table[col.r];
	 *		col.g = s->gamma_table[col.g];
	 *		col.b = s->gamma_table[col.b];
	 * }
	 */
	unsigned char gamma_table[256]; /* ARRAY OK 2007-11-21 timj */
#endif // !MINPNG_NO_GAMMA_SUPPORT

	/**
	 * Buffer for zlib compressed data extracted from ONE IDAT chunk. This might not be all of the
	 * chunk data in the IDAT chunk
	 */
	minpng_zbuffer compressed_data;

	/**
	 * The index of the current frame beeing processed. Initially it's -1 and increases when an
	 * fcTL chunk is processed, or, if !APNG_SUPPORTED or no acTL chunk has been encounted before
	 * (no animation), when the first IDAT is found. When !APNG_SUPPORTED will frame_index never
	 * be more then zero.
	 */
	int frame_index;

	/**
	 * Number of frames in the animation as recorded in the acTL chunk, or 1 if the png is not an
	 * animation or !APNG_SUPPORTED.
	 */
	int num_frames;

	/**
	 * Number of pixels found so far in the animation/image, for rejecting extremely enormous
	 * animations/images that might cause signed integer overflow (max 2 billion pixels are allowed).
	 */
	float pixel_count;

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
	 * Current number of pixels allocated in deinterlace_image, for, if possible, avoid re-allocating
	 * deinterlace_image between frames in interlaced apngs.
	 */
	unsigned int deinterlace_image_pixels;

#ifdef APNG_SUPPORT

	/** True if the png is an animation, that is, if there is an acTL chunk before the first IDAT chunk */
	BOOL is_apng;

	/**
	 * True if the current frame should be blended when drawn onto the canvas, or false if all pixel-values
	 * should simply overwrite the previous content.
	 */
	BOOL blend;

	/**
	 * For keeping track of the apng sequence-numbers in the fcTL and and fdAT chunks, this is the value
	 * expected on the NEXT such chunk encounted.
	 */
	unsigned int apng_seq;
	
	/**
	 * Index of the last frame that was completely decoded, initially -1.
	 */
	int last_finished_index;

	/** Number of times to iterate the animation, as recorded in the acTL chunk. */
	int num_iterations;

	/** Duration in sec/100 as interpreded from the fcTL chunk for the current frame. */
	int duration;

	/** Disposal-method as interpreded from the fcTL chunk for the current frame. */
	DisposalMethod disposal_method;

#endif

#ifdef EMBEDDED_ICC_SUPPORT
	/** Set to TRUE if an iCCP chunk has been successfully decoded. */
	BOOL first_iccp_found;
#endif

	/**
	 * Static constructor for creating a minpng_state struct. This function is creating the struct
	 * and initializes (through minpng_state::init) minpng_state::compressed_data, which in turn
	 * let the zlib library initialize it's z_stream object.
	 * @return A minpng_state struct allocated on the heap, or in case of an error - NULL.
	 */
	static minpng_state* construct();

	/**
	* Destructor, do NOT delete the minpng_state struct before deleting an associated PngRes struct
	* or PngFeeder struct, as PngFeeder has the pointer to the minpng_state and PngRes might have
	* pointers to data that will be deleted in this destructor.
	*/
	~minpng_state();

private:
	/// Private constructor...
	minpng_state();

	/// Private initialization function called from minpng_state::construct.
	PngRes::Result init();
};

/**
 * Different png chunk-ids as specified by the png specification.
 */
enum ChunkID
{
#ifdef APNG_SUPPORT
	CHUNK_fdAT = 0x66644154,
	CHUNK_fcTL = 0x6663544c,
	CHUNK_acTL = 0x6163544c,
#endif
#ifdef EMBEDDED_ICC_SUPPORT
	CHUNK_iCCP = 0x69434350,
#endif
	CHUNK_PLTE = 0x504c5445,
	CHUNK_IDAT = 0x49444154,
	CHUNK_tRNS = 0x74524e53,
	CHUNK_gAMA = 0x67414d41
};

/**
 * Different decoding states as recorded in minpng_state::state, used by minpng_decode to know
 * where it is in the decoding process.
 */
enum
{
	/// Initial state before the IHDR chunk has been processed.
	STATE_INITIAL,

	/// We're about to investigate a chunk-header and move into another state
	STATE_CHUNKHEADER,

	/// The gAMA chunk should be processed.
	STATE_gAMA,

	/// The palette should be processed.
	STATE_PLTE,

	/// We're about to, or in the process of processing an IDAT chunk. The IDAT chunk is the only chunk where the processing might span over several calls to minpng_decode.
	STATE_IDAT,

	/// The transparency/alpha information should be processed for color types 0, 2 and 3.
	STATE_tRNS,

#ifdef APNG_SUPPORT

	STATE_acTL,
	STATE_fcTL,
	STATE_fdAT,

#endif

#ifdef EMBEDDED_ICC_SUPPORT
	// Embedded ICC profile
	STATE_iCCP,
#endif

	/// We're at a chunk that should of some reason be ignored (might be due to unknown type).
	STATE_IGNORE,

	/// We should investigate a chunks CRC code at the end of the chunk. No CRC checks are currently performed as many images have incorrect CRC values, even though they are "correct" apart of this.
	STATE_CRC
};

#endif // !MINPNG_PNG_INT_H
