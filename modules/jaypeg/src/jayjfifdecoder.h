/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYJFIFDECODER_H
#define JAYJFIFDECODER_H

#include "modules/jaypeg/jaypeg.h"
#ifdef JAYPEG_JFIF_SUPPORT

#include "modules/jaypeg/src/jayformatdecoder.h"

#include "modules/jaypeg/src/jayhuffdecoder.h"
#include "modules/jaypeg/src/jayidct.h"

class JayImage;

typedef enum{JAYPEG_JFIF_STATE_NONE, JAYPEG_JFIF_STATE_STARTED, 
		JAYPEG_JFIF_STATE_FRAME, JAYPEG_JFIF_STATE_FRAMEHEADER, 
		JAYPEG_JFIF_STATE_RESTART, JAYPEG_JFIF_STATE_FIND_MARKER,
		JAYPEG_JFIF_STATE_DONE} JayJFIFState;

typedef enum{JAYPEG_COLORSPACE_YCRCB, JAYPEG_COLORSPACE_RGB, 
	JAYPEG_COLORSPACE_CMYK, JAYPEG_COLORSPACE_YCCK,
	JAYPEG_COLORSPACE_GRAYSCALE, JAYPEG_COLORSPACE_UNKNOWN} JayColorSpace;

/** The main decoder class for jfif decoding. */
class JayJFIFDecoder : public JayFormatDecoder
{
public:
	JayJFIFDecoder(BOOL enable_decoding);
	~JayJFIFDecoder();

	/** Initialize the decoder.
	 * @param image the image listener to use.
	 * @returns TRUE if initialization was successfull, FALSE otherwise. */
	BOOL init(JayImage *image);

	int decode(class JayStream *stream);

	void flushProgressive();

	BOOL isDone();
	BOOL isFlushed();
private:
	/** Decode MCUs until the restart intervall is reached, all MCUs have 
	 * been decoded or there is not enough data. */
	int decodeMCU(class JayStream *stream);

#ifdef JAYPEG_EXIF_SUPPORT
	/** Decode the EXIF header and send the meta data to the image listener. */
	int decodeExif(const unsigned char* exifData, unsigned int exifLen);
#endif // JAYPEG_EXIF_SUPPORT

	/** Start a new MCU row. It is called before decodeMCU can be used. */
	void startMCURow();
	/** Write the specified MCU row to the image listener. */
	void writeMCURow(int rownum);

	// switch to determine if data needs to be decoded
	BOOL decode_data;

	// keeping tranck of current marker
	unsigned int markerlength;
	unsigned int skipCount;

	// keeping track of restart intervalls
	unsigned int restartInterval;
	unsigned int restartCount;

	JayEntropyDecoder *entropyDecoder;
	JayHuffDecoder huffEntropyDecoder;
	JayIDCT trans;
	JayImage *image;

	// keeping track of which component is being decoded
	int curComponent;
	int curComponentCount;
	int scanComponents[JAYPEG_MAX_COMPONENTS];
	int numScanComponents;

	//  Information about the components
	int maxVertRes;
	int maxHorizRes;
	int compVertRes[JAYPEG_MAX_COMPONENTS];
	int compHorizRes[JAYPEG_MAX_COMPONENTS];

	int compDataUnitCount[JAYPEG_MAX_COMPONENTS];

	unsigned char *mcuRow[JAYPEG_MAX_COMPONENTS];

	unsigned char *scanline;
#ifndef JAYPEG_LOW_QUALITY_SCALE
	unsigned char* scaleCache;
#endif // !JAYPEG_LOW_QUALITY_SCALE

	int dataUnitsPerMCURow;
	int mcuRowNum;

	// Information about the image
	int width;
	int height;
	int compWidthInBlocks[JAYPEG_MAX_COMPONENTS];
	int numComponents;
	int componentNum[JAYPEG_MAX_COMPONENTS];
	int sampleSize;
	BOOL progressive;
	BOOL interlaced;
	// Information needed to flush progressive jpegs
	int lastStartedMCURow;
	int lastWrittenMCURow;
	BOOL wrappedMCU;

	int samples_width;
	int samples_height;

	// Information needed for progressive jpegs
	unsigned char firstProgSample;
	unsigned char numProgSamples;
	unsigned char approxLow;
	unsigned char approxHigh;

	// Storage of samples, only needed for progressive jpegs
	short **progressionSamples[JAYPEG_MAX_COMPONENTS];

#ifdef EMBEDDED_ICC_SUPPORT
	class ICCDataBuffer
	{
	public:
		ICCDataBuffer() : m_data(NULL), m_datalen(0), m_datasize(0) {}
		~ICCDataBuffer() { OP_DELETEA(m_data); }

		OP_STATUS Append(const UINT8* d, unsigned dlen);

		void Destroy()
		{
			OP_DELETEA(m_data);
			m_data = NULL;
			m_datalen = m_datasize = 0;
		}

		const UINT8* Data() const { return m_data; }
		unsigned Length() const { return m_datalen; }

	private:
		UINT8* m_data;
		unsigned m_datalen;
		unsigned m_datasize;
	};
	ICCDataBuffer iccData;
	BOOL skipAPP2;
	UINT8 lastIccChunk;
	UINT8 totalIccChunks;
#endif // EMBEDDED_ICC_SUPPORT	

	BOOL jfifHeader;
	BOOL adobeHeader;
	unsigned char adobeTransform;
	JayColorSpace colorSpace;
	
	JayJFIFState state;
};
#endif // JAYPEG_JFIF_SUPPORT
#endif
