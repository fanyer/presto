/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYJFIFENCODER_H
#define JAYJFIFENCODER_H

#include "modules/jaypeg/jaypeg.h"
#if defined(JAYPEG_JFIF_SUPPORT) && defined(JAYPEG_ENCODE_SUPPORT)

#include "modules/jaypeg/src/encoder/jayformatencoder.h"

class JayEncodedData;

/** The main encoder class for jfif encoding. */
class JayJFIFEncoder : public JayFormatEncoder
{
public:
	JayJFIFEncoder();
	~JayJFIFEncoder();

	/** Initialize the jfif encoder.
	 * @param quality the quality setting in percent (1-100).
	 * @param width the width of the image to encode.
	 * @param height the height of the image to encode.
	 * @param strm the stream to send encoded data to.
	 * @returns OpStatus::OK on success, an error otherwise. */
	OP_STATUS init(int quality, unsigned int width, unsigned int height, JayEncodedData *strm);

	/** Encode the next scanline in the image.
	 * @param scanline the data for the next scanline in 32bpp format.
	 * @returns OpStatus::OK on success, an error otherwise. */
	OP_STATUS encodeScanline(const UINT32* scanline);
private:
	/** Finish the encoding, flush the bit buffer and write the eoi marker. */
	OP_STATUS finish();
	/** Encode a MCU.
	 * @param mcu the mcu data to encode.
	 * @param stride the stride of the lines in the MCU in entries, not bytes.
	 * @param component the component this MCU belongs to. */
	OP_STATUS encodeMCU(short* mcu, unsigned int stride, int component);

	/** Write the JFIF header to the output stream. */
	OP_STATUS writeJFIFHeader();
	/** Initialize the quant tables using the quality setting and write them
	 * to the output stream.
	 * @param quality the quality to use for encoding in percent. */
	OP_STATUS initAndWriteDQTHeader(int quality);
	/** Initialize the huffman tables and write them to the output stream. */
	OP_STATUS initAndWriteDHTHeader();
	/** Write the start of frame header. */
	OP_STATUS writeSOFHeader();
	/** Write the start of scan header. */
	OP_STATUS writeSOSHeader();

	/** Append a number of bits to the output stream. The lowest bits of the 
	 * data will be appended. Trying to append 0 bytes will flush the stream, 
	 * so be carefull not to do that until all encoding is complete.
	 * @param data the data to append to the stream.
	 * @param bits the number of bits to append from data. */
	OP_STATUS writeBits(int data, unsigned int bits);

	/** Get the huffman code for value val and store it in result.
	 * @param val the value to find huffman code for.
	 * @param component the component this data belongs to, used to select huffman table.
	 * @param dc true for dc samples, false for ac samples, used to select huffman table.
	 * @param result the destination for the huffman code.
	 * @returns the length of the code. */
	int getHuffmanCode(UINT8 val, int component, bool dc, unsigned short& result);

	/** Apply the fdct transform, zig-zag reordering and quantification of the
	 * samples in a MCU.
	 * @param coef the coeficients from the MCU to encode.
	 * @param stride the stride between lines in the MCU, in entries not bytes.
	 * @param quant the quantification table to use. */
	void fdctZigZagQuant(short* coef, unsigned int stride, const unsigned char* quant);

	JayEncodedData* output_stream;
	unsigned int image_width;
	unsigned int image_height;
	unsigned int encoded_scanlines;
	unsigned int block_width;

	short* y_blocks;
	short* cr_blocks;
	short* cb_blocks;
	short pred[3];
	UINT8 bit_buffer;
	unsigned int bit_buffer_len;

	unsigned char* quant_table_luminance;
	unsigned char* quant_table_chrominance;
};
#endif // JAYPEG_JFIF_SUPPORT && JAYPEG_ENCODE_SUPPORT
#endif // JAYJFIFENCODER_H

