/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#ifndef OPERA_WEBP_DECODE_H
#define OPERA_WEBP_DECODE_H

#include "modules/webp/lib/webp/decode.h"

#define RIFF_HEADER_SIZE 20
#define VP8_HEADER_SIZE 10
#define WEBP_HEADER_SIZE (RIFF_HEADER_SIZE + VP8_HEADER_SIZE)

/**
   wrapper for ImageDecoderWebP
 */
class WebPDecoder
{
public:
	/**
	   attempts to determine whether data contains a WebP image
	   @param data the WebP encoded data
	   @param len the size of the WebP data, in bytes
	   @return YES if the data is likely to contain a WebP image, NO if
	   the data is not, MAYBE there's not enough data available to decide
	*/
	static BOOL3 Check(const UCHAR* data, INT32 len);

	/**
	   peeks image dimensions of a webp image
	   @param data the WebP encoded data
	   @param len the size of the WebP data, in bytes
	   @param width (out) the width of the image (only valid if function returns TRUE)
	   @param height (out) the height of the image (only valid if function returns TRUE)
	   @return TRUE if it appears data holds a valid WebP image, FALSE otherwise
	*/
	static BOOL3 Peek(const UCHAR* data, INT32 len, INT32& width, INT32& height);

	WebPDecoder();
	~WebPDecoder();

	/**
	   sets the ImageDecoderListener - must be called with a non-NULL
	   listener before calling Decode
	   @param listener the ImageDecoderListener
	 */
	void SetImageDecoderListener(ImageDecoderListener* listener) { m_listener = listener; }

	/**
	   @see ImageDecoder::DecodeData()

	   @param suppress_threaded_decoding if TRUE, decoding will be
	   done all in one go, without creating a pseudo-thread.
	 */
	OP_STATUS Decode(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes);

private:
	/**
	   Sends OnInitMainFrame and OnNewFrame to listener
	   @param features Structure of bitstream features from the decoder.
	 */
	void SendFrameInfo(const WebPBitstreamFeatures& features);

	ImageDecoderListener* m_listener; ///< listener to report progress to
	WebPIDecoder* m_pidec;      ///< libwebp incremental decoder object.
	int m_start_y;              ///< keeps track of which line to process next.

	BYTE m_hdr_buf[WEBP_HEADER_SIZE]; /* ARRAY OK 2011-09-15 christiank */ ///< Buffer for storing the RIFF + VP8 header.
	INT32 m_hdr_len;            ///< Number of bytes in the header buffer.

	WebPDecBuffer m_buffer; ///< Buffer used by libwebp to store decoded data.
};

#endif // !OPERA_WEBP_DECODE_H
