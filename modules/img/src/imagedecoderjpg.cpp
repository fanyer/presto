/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#if defined(INTERNAL_JPG_SUPPORT) || defined(ASYNC_IMAGE_DECODERS_EMULATION)

#ifdef USE_JAYPEG

#include "modules/img/src/imagedecoderjpg.h"
#include "modules/probetools/probepoints.h"

ImageDecoder* create_jpeg_decoder(void)
{
	return OP_NEW(ImageDecoderJpg, ());
}
ImageDecoderJpg::ImageDecoderJpg() : m_imageDecoderListener(NULL), decoder(NULL), width(0), height(0), linedata(NULL)
{
}

ImageDecoderJpg::~ImageDecoderJpg()
{
	OP_DELETE(decoder);
	OP_DELETEA(linedata);
}


OP_STATUS ImageDecoderJpg::DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL/* load_all = FALSE*/)
{
	OP_PROBE6(OP_PROBE_IMG_JPG_DECODE_DATA);
	resendBytes = 0;
	int err;
	if (!decoder)
	{
		decoder = OP_NEW(JayDecoder, ());
		if (!decoder)
			return OpStatus::ERR_NO_MEMORY;
		err = decoder->init(data, numBytes, this, TRUE);
		if (err == JAYPEG_NOT_ENOUGH_DATA)
		{
			OP_DELETE(decoder);
			decoder = 0;
			resendBytes = numBytes;
			return OpStatus::OK;
		}
		if (err == JAYPEG_ERR)
			return OpStatus::ERR;
		if (err == JAYPEG_ERR_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;
	}
	err = decoder->decode(data, numBytes);
	if (err == JAYPEG_ERR)
		return OpStatus::ERR;
	if (err == JAYPEG_ERR_NO_MEMORY)
		return OpStatus::ERR_NO_MEMORY;

	decoder->flushProgressive();

	if (!more || decoder->isDone())
	{
		m_imageDecoderListener->OnDecodingFinished();
	}

	resendBytes = err;
	return OpStatus::OK;
}

void ImageDecoderJpg::SetImageDecoderListener(ImageDecoderListener* imageDecoderListener)
{
	m_imageDecoderListener = imageDecoderListener;
}

int ImageDecoderJpg::init(int width, int height, int numComponents, BOOL progressive)
{
	this->width = width;
	this->height = height;
	this->components = numComponents;
	this->progressive = progressive;

	linedata = OP_NEWA(UINT32, width);
	if (!linedata)
	{
		return JAYPEG_ERR_NO_MEMORY;
	}

	// Start the frame when we receive data
	startFrame = TRUE;

	return JAYPEG_OK;
}
void ImageDecoderJpg::scanlineReady(int scanline, const unsigned char *imagedata)
{
	if (startFrame)
	{
		m_imageDecoderListener->OnInitMainFrame(width, height);

		ImageFrameData image_frame_data;
		image_frame_data.rect.width = width;
		image_frame_data.rect.height = height;
		image_frame_data.interlaced = progressive;
		image_frame_data.bits_per_pixel = components*8;

		m_imageDecoderListener->OnNewFrame(image_frame_data);

		startFrame = FALSE;
	}
	if (components == 3)
	{
		for (UINT32 i = 0; i < width; ++i)
		{
#if defined(PLATFORM_COLOR_IS_RGBA)
			linedata[i] = (0xff<<24) | (imagedata[i*components]<<16) | (imagedata[i*components+1]<<8) | (imagedata[i*components+2]);
#else
			linedata[i] = (0xffu<<24) | (imagedata[i*components+2]<<16) | (imagedata[i*components+1]<<8) | (imagedata[i*components]);
#endif //(PLATFORM_COLOR_IS_RGBA)
		}
	}
	else
	{
		for (UINT32 i = 0; i < width; ++i)
		{
			linedata[i] = (0xffu<<24) | (imagedata[i*components]<<16) | (imagedata[i*components]<<8) | (imagedata[i*components]);
		}
	}
	if (height > 0)
		m_imageDecoderListener->OnLineDecoded(linedata, scanline, 1);
	if (scanline == (int)height)
		m_imageDecoderListener->OnDecodingFinished();
}

#ifdef IMAGE_METADATA_SUPPORT
#define EXIF_ORIENTATION 0x112
#define EXIF_DATETIME 0x132
#define EXIF_DESCRIPTION 0x10E
#define EXIF_MAKE 0x10F
#define EXIF_MODEL 0x110
#define EXIF_SOFTWARE 0x131
#define EXIF_ARTIST 0x13B
#define EXIF_COPYRIGHT 0x8298
// Picture taking conditions
#define EXIF_EXPOSURETIME 0x829A
#define EXIF_FNUMBER 0x829D
#define EXIF_EXPOSUREPROGRAM 0x8822
#define EXIF_SPECTRALSENSITIVITY 0x8824
#define EXIF_ISOSPEEDRATING 0x8827
#define EXIF_SHUTTERSPEED 0x9201
#define EXIF_APERTURE 0x9202
#define EXIF_BRIGHTNESS 0x9203
#define EXIF_EXPOSUREBIAS 0x9204
#define EXIF_MAXAPERTURE 0x9205
#define EXIF_SUBJECTDISTANCE 0x9206
#define EXIF_METERINGMODE 0x9207
#define EXIF_LIGHTSOURCE 0x9208
#define EXIF_FLASH 0x9209
#define EXIF_FOCALLENGTH 0x920A
#define EXIF_SUBJECTAREA 0x9214
#define EXIF_FLASHENERGY 0xA20B
#define EXIF_FOCALPLANEXRESOLUTION 0xA20E
#define EXIF_FOCALPLANEYRESOLUTION 0xA20F
#define EXIF_FOCALPLANERESOLUTIONUNIT 0xA210
#define EXIF_SUBJECTLOCATION 0xA214
#define EXIF_EXPOSUREINDEX 0xA215
#define EXIF_SENSINGMETHOD 0xA217
#define EXIF_CUSTOMRENDERED 0xA401
#define EXIF_EXPOSUREMODE 0xA402
#define EXIF_WHITEBALANCE 0xA403
#define EXIF_DIGITALZOOMRATIO 0xA404
#define EXIF_FOCALLENGTHIN35MMFILM 0xA405
#define EXIF_SCENECAPTURETYPE 0xA406
#define EXIF_GAINCONTROL 0xA407
#define EXIF_CONTRAST 0xA408
#define EXIF_SATURATION 0xA409
#define EXIF_SHARPNESS 0xA40A
#define EXIF_SUBJECTDITANCERANGE 0xA40C
void ImageDecoderJpg::exifDataFound(unsigned int exif, const char* data)
{
	switch (exif)
	{
	case EXIF_ORIENTATION:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_ORIENTATION, data);
		break;
	case EXIF_DATETIME:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_DATETIME, data);
		break;
	case EXIF_DESCRIPTION:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_DESCRIPTION, data);
		break;
	case EXIF_MAKE:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_MAKE, data);
		break;
	case EXIF_MODEL:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_MODEL, data);
		break;
	case EXIF_SOFTWARE:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_SOFTWARE, data);
		break;
	case EXIF_ARTIST:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_ARTIST, data);
		break;
	case EXIF_COPYRIGHT:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_COPYRIGHT, data);
		break;
	// Picture taking conditions
	case EXIF_EXPOSURETIME:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_EXPOSURETIME, data);
		break;
	case EXIF_FNUMBER:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_FNUMBER, data);
		break;
	case EXIF_EXPOSUREPROGRAM:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_EXPOSUREPROGRAM, data);
		break;
	case EXIF_SPECTRALSENSITIVITY:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_SPECTRALSENSITIVITY, data);
		break;
	case EXIF_ISOSPEEDRATING:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_ISOSPEEDRATING, data);
		break;
	case EXIF_SHUTTERSPEED:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_SHUTTERSPEED, data);
		break;
	case EXIF_APERTURE:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_APERTURE, data);
		break;
	case EXIF_BRIGHTNESS:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_BRIGHTNESS, data);
		break;
	case EXIF_EXPOSUREBIAS:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_EXPOSUREBIAS, data);
		break;
	case EXIF_MAXAPERTURE:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_MAXAPERTURE, data);
		break;
	case EXIF_SUBJECTDISTANCE:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_SUBJECTDISTANCE, data);
		break;
	case EXIF_METERINGMODE:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_METERINGMODE, data);
		break;
	case EXIF_LIGHTSOURCE:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_LIGHTSOURCE, data);
		break;
	case EXIF_FLASH:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_FLASH, data);
		break;
	case EXIF_FOCALLENGTH:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_FOCALLENGTH, data);
		break;
	case EXIF_SUBJECTAREA:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_SUBJECTAREA, data);
		break;
	case EXIF_FLASHENERGY:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_FLASHENERGY, data);
		break;
	case EXIF_FOCALPLANEXRESOLUTION:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_FOCALPLANEXRESOLUTION, data);
		break;
	case EXIF_FOCALPLANEYRESOLUTION:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_FOCALPLANEYRESOLUTION, data);
		break;
	case EXIF_FOCALPLANERESOLUTIONUNIT:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_FOCALPLANERESOLUTIONUNIT, data);
		break;
	case EXIF_SUBJECTLOCATION:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_SUBJECTLOCATION, data);
		break;
	case EXIF_EXPOSUREINDEX:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_EXPOSUREINDEX, data);
		break;
	case EXIF_SENSINGMETHOD:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_SENSINGMETHOD, data);
		break;
	case EXIF_CUSTOMRENDERED:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_CUSTOMRENDERED, data);
		break;
	case EXIF_EXPOSUREMODE:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_EXPOSUREMODE, data);
		break;
	case EXIF_WHITEBALANCE:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_WHITEBALANCE, data);
		break;
	case EXIF_DIGITALZOOMRATIO:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_DIGITALZOOMRATIO, data);
		break;
	case EXIF_FOCALLENGTHIN35MMFILM:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_FOCALLENGTHIN35MMFILM, data);
		break;
	case EXIF_SCENECAPTURETYPE:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_SCENECAPTURETYPE, data);
		break;
	case EXIF_GAINCONTROL:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_GAINCONTROL, data);
		break;
	case EXIF_CONTRAST:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_CONTRAST, data);
		break;
	case EXIF_SATURATION:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_SATURATION, data);
		break;
	case EXIF_SHARPNESS:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_SHARPNESS, data);
		break;
	case EXIF_SUBJECTDITANCERANGE:
		m_imageDecoderListener->OnMetaData(IMAGE_METADATA_SUBJECTDISTANCERANGE, data);
		break;
	default:
		break;
	}
}
#endif // IMAGE_METADATA_SUPPORT

#ifdef EMBEDDED_ICC_SUPPORT
void ImageDecoderJpg::iccDataFound(const UINT8* data, unsigned datalen)
{
	m_imageDecoderListener->OnICCProfileData(data, datalen);
}
#endif // EMBEDDED_ICC_SUPPORT

#endif // USE_JAYPEG

#ifdef USE_SYSTEM_JPEG

#include "modules/img/src/imagedecoderjpg.h"

#define JPEG_HEADER_ERROR 1111

#ifndef _NO_GLOBALS_
#ifdef JPEG_SETJMP_SUPPORTED
static jmp_buf g_setjmp_buffer;
#endif // JPEG_SETJMP_SUPPORTED
#endif

void img_error_exit(j_common_ptr cinfo)
{
	//OP_ASSERT(cinfo != NULL && cinfo->err != NULL);
	//struct img_error_mgr* img_err = (struct img_error_mgr*) cinfo->err;
	OP_STATUS ret = OpStatus::ERR;
	if (cinfo->err->msg_code == JERR_OUT_OF_MEMORY)
	{
		ret = OpStatus::ERR_NO_MEMORY;
	}
#ifdef JPEG_SETJMP_SUPPORTED
	op_longjmp(g_setjmp_buffer, (int)ret);
#else // JPEG_SETJMP_SUPPORTED
	LEAVE(ret);
#endif // JPEG_SETJMP_SUPPORTED
}

void img_output_message (j_common_ptr cinfo)
{
}

void img_emit_message (j_common_ptr cinfo, int msg_level)
{
}

void img_format_message (j_common_ptr cinfo, char * buffer)
{
}

void img_reset_error_mgr (j_common_ptr cinfo)
{
  cinfo->err->num_warnings = 0;
  /* trace_level is not reset since it is an application-supplied parameter */
  cinfo->err->msg_code = 0;	/* may be useful as a flag for "no error" */
}

/*
 * Initialize source --- called by jpeg_read_header
 * before any data is actually read.
 */

void jpg_img_init_source (j_decompress_ptr cinfo)
{
	//jpg_img_src_mgr* src = (jpg_img_src_mgr*) cinfo->src;
}

/*
 * Fill the input buffer --- called whenever buffer is emptied.
 *
 * In typical applications, this should read fresh data into the buffer
 * (ignoring the current state of next_input_byte & bytes_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been reloaded.  It is not necessary to
 * fill the buffer entirely, only to obtain at least one more byte.
 *
 * There is no such thing as an EOF return.  If the end of the file has been
 * reached, the routine has a choice of ERREXIT() or inserting fake data into
 * the buffer.  In most cases, generating a warning message and inserting a
 * fake EOI marker is the best course of action --- this will allow the
 * decompressor to output however much of the image is there.  However,
 * the resulting error message is misleading if the real problem is an empty
 * input file, so we handle that case specially.
 *
 * In applications that need to be able to suspend compression due to input
 * not being available yet, a FALSE return indicates that no more data can be
 * obtained right now, but more may be forthcoming later.  In this situation,
 * the decompressor will return to its caller (with an indication of the
 * number of scanlines it has read, if any).  The application should resume
 * decompression after it has loaded more data into the input buffer.  Note
 * that there are substantial restrictions on the use of suspension --- see
 * the documentation.
 *
 * When suspending, the decompressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_input_byte & bytes_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point must be rescanned after resumption, so move it to
 * the front of the buffer rather than discarding it.
 */

boolean jpg_img_fill_input_buffer (j_decompress_ptr cinfo)
{
	jpg_img_src_mgr* src = (jpg_img_src_mgr*) cinfo->src;
	if (src->more)
		return FALSE;
	else
	{
	    ERREXIT(cinfo, JERR_INPUT_EMPTY);

		/* Insert a fake EOI marker */
	    src->buffer[0] = (JOCTET) 0xFF;
	    src->buffer[1] = (JOCTET) JPEG_EOI;
	}

	return TRUE;
}


/*
 * Skip data --- used to skip over a potentially large amount of
 * uninteresting data (such as an APPn marker).
 *
 * Writers of suspendable-input applications must note that skip_input_data
 * is not granted the right to give a suspension return.  If the skip extends
 * beyond the data currently in the buffer, the buffer can be marked empty so
 * that the next read will cause a fill_input_buffer call that can suspend.
 * Arranging for additional bytes to be discarded before reloading the input
 * buffer is the application writer's problem.
 */

void jpg_img_skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
	jpg_img_src_mgr* src = (jpg_img_src_mgr*) cinfo->src;

	if ((long) src->pub.bytes_in_buffer > num_bytes)
	{
		src->pub.next_input_byte += (size_t) num_bytes;
		src->pub.bytes_in_buffer -= (size_t) num_bytes;
	}
	else
	{
		src->skip_len = num_bytes - src->pub.bytes_in_buffer;
		src->pub.bytes_in_buffer = 0;
	}
}

/*
 * Terminate source --- called by jpeg_finish_decompress
 * after all data has been read.  Often a no-op.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

void jpg_img_term_source (j_decompress_ptr cinfo)
{
  /* no work necessary here */
}

/*
 * Prepare for input from a buffer.
 */

void jpeg_input_src (j_decompress_ptr cinfo, jpg_img_src_mgr* src)
{
	cinfo->src = (struct jpeg_source_mgr*) src;
	src->pub.init_source = jpg_img_init_source;
	src->pub.fill_input_buffer = jpg_img_fill_input_buffer;
	src->pub.skip_input_data = jpg_img_skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
	src->pub.term_source = jpg_img_term_source;
}

OP_STATUS InitJPEG(ImageDecoderJpg* imgdec)
{
#ifdef JPEG_SETJMP_SUPPORTED
	RETURN_IF_ERROR((OP_STATUS)op_setjmp(g_setjmp_buffer));
#endif // JPEG_SETJMP_SUPPORTED

	imgdec->cinfo.err = &imgdec->jerr_pub;
	imgdec->jerr_pub.error_exit = img_error_exit;
	imgdec->jerr_pub.emit_message = img_emit_message;
	imgdec->jerr_pub.output_message = img_output_message;
	imgdec->jerr_pub.format_message = img_format_message;
    imgdec->jerr_pub.reset_error_mgr = img_reset_error_mgr;

#ifdef JPEG_SETJMP_SUPPORTED
	jpeg_create_decompress(&imgdec->cinfo);
#else // JPEG_SETJMP_SUPPORTED
	TRAPD(err,jpeg_create_decompress(&imgdec->cinfo)); // May Leave
	RETURN_IF_ERROR(err);
#endif // JPEG_SETJMP_SUPPORTED
	jpeg_input_src(&imgdec->cinfo, &imgdec->jsrc); // Cannot Leave

	return OpStatus::OK;
}

// returns:
//          JPEG_HEADER_OK if OK
//          JPEG_HEADER_ERROR if cannot read header
//          JPEG_SUSPENDED if need more data
int ReadHeader(ImageDecoderJpg* imgdec)
{
#ifdef JPEG_SETJMP_SUPPORTED
	OP_STATUS opstat = (OP_STATUS)op_setjmp(g_setjmp_buffer);
	if (OpStatus::IsError(opstat))
	{
		imgdec->abort_status = opstat;
		return JPEG_HEADER_ERROR;
	}
#endif // JPEG_SETJMP_SUPPORTED

	int stat;
#ifdef JPEG_SETJMP_SUPPORTED
	stat = jpeg_read_header(&imgdec->cinfo, TRUE);
#else
    TRAPD(err,stat=jpeg_read_header(&imgdec->cinfo, TRUE)); // May Leave
    if (err)
	{
		imgdec->abort_status = err;
		return JPEG_HEADER_ERROR;
	}
#endif // JPEG_SETJMP_SUPPORTED
	if (stat == JPEG_HEADER_OK)
    {
#ifdef JPEG_SETJMP_SUPPORTED
		imgdec->is_progressive = jpeg_has_multiple_scans(&imgdec->cinfo);
#else // JPEG_SETJMP_SUPPORTED
		TRAP(err,imgdec->is_progressive=jpeg_has_multiple_scans(&imgdec->cinfo)); // May Leave
	    {
		    imgdec->abort_status = err;
		    return JPEG_HEADER_ERROR;
	    }
#endif // JPEG_SETJMP_SUPPORTED
    }
	return stat;
}

// returns:
//          TRUE if OK
//          FALSE if aborted or suspended
int Start(ImageDecoderJpg* imgdec, JSAMPARRAY colormap, BOOL quantize,
					 int bits_per_pixel, BOOL one_pass, J_DITHER_MODE dither_mode,
					 J_DCT_METHOD dct_method, BOOL nosmooth)
{
	int ret;

#ifdef JPEG_SETJMP_SUPPORTED
	OP_STATUS opstat = (OP_STATUS)op_setjmp(g_setjmp_buffer);
	if (OpStatus::IsError(opstat))
	{
		imgdec->abort_status = opstat;
		return 0;
	}
#endif // JPEG_SETJMP_SUPPORTED

	if (imgdec->cinfo.jpeg_color_space == JCS_GRAYSCALE)
	{
		// jpeglig generates colormap
		imgdec->cinfo.quantize_colors = TRUE;
	}
	else
	{
		imgdec->cinfo.quantize_colors = quantize;
		imgdec->cinfo.two_pass_quantize = !one_pass;
		imgdec->cinfo.colormap = colormap;
		imgdec->cinfo.actual_number_of_colors = 1 << bits_per_pixel;
		imgdec->cinfo.dither_mode = dither_mode;
	}

	imgdec->cinfo.dct_method = dct_method;
	imgdec->cinfo.do_fancy_upsampling = !nosmooth;

#ifdef JPEG_SETJMP_SUPPORTED
	ret = jpeg_start_decompress(&imgdec->cinfo);
#else // JPEG_SETJMP_SUPPORTED
	TRAPD(err,ret=jpeg_start_decompress(&imgdec->cinfo)); // May Leave
    if (err)
	{
		imgdec->abort_status = err;
		return FALSE;
	}
#endif // JPEG_SETJMP_SUPPORTED
	imgdec->start_scan = 0;

	if (bits_per_pixel < 8 && ret)
		imgdec->scratch_buffer = (unsigned char *) (imgdec->cinfo.mem->alloc_small) ((j_common_ptr) &imgdec->cinfo, JPOOL_IMAGE,
								imgdec->cinfo.output_width);

	return ret;
}

BOOL StartFrame(ImageDecoderJpg* imgdec)
{
#ifdef JPEG_SETJMP_SUPPORTED
	OP_STATUS opstat = (OP_STATUS)op_setjmp(g_setjmp_buffer);
	if (OpStatus::IsError(opstat))
	{
		imgdec->abort_status = opstat;
		return FALSE;
	}
#endif // JPEG_SETJMP_SUPPORTED

    int ret;
#ifdef JPEG_SETJMP_SUPPORTED
	ret = jpeg_start_output(&imgdec->cinfo, imgdec->cinfo.input_scan_number);
#else // JPEG_SETJMP_SUPPORTED
	TRAPD(err,ret=jpeg_start_output(&imgdec->cinfo, imgdec->cinfo.input_scan_number)); // May Leave
    if (err)
	{
		imgdec->abort_status = err;
		return FALSE;
	}
#endif // JPEG_SETJMP_SUPPORTED
    if (ret)
	{
		// We need to store what scan we started decoding, so we know if
		// we actually have the whole frame when decoding the final frame
		// See DiscardFrame()

		imgdec->start_scan = imgdec->cinfo.input_scan_number;
		imgdec->reading_data = TRUE;

		return TRUE;
	}
	else
		return FALSE;
}

BOOL FinishFrame(ImageDecoderJpg* imgdec)
{
#ifdef JPEG_SETJMP_SUPPORTED
	OP_STATUS opstat = (OP_STATUS)op_setjmp(g_setjmp_buffer);
	if (OpStatus::IsError(opstat))
	{
		imgdec->abort_status = opstat;
		return FALSE;
	}
#endif // JPEG_SETJMP_SUPPORTED

    int ret;
#ifdef JPEG_SETJMP_SUPPORTED
	ret = jpeg_finish_output(&imgdec->cinfo);
#else // JPEG_SETJMP_SUPPORTED
    TRAPD(err,ret=jpeg_finish_output(&imgdec->cinfo)); // May Leave
	if (err)
	{
		imgdec->abort_status = err;
		return FALSE;
	}
#endif // JPEG_SETJMP_SUPPORTED
	if (ret)
	{
		imgdec->reading_data = FALSE;

		return TRUE;
	}
	else
		return FALSE;
}

int GetRows(ImageDecoderJpg* imgdec, int height)
{
	unsigned char* tmppek[1] = { imgdec->bitmap_array };
	JSAMPARRAY line = tmppek;

	int bits_per_pixel = imgdec->bits_per_pixel;

#ifdef JPEG_SETJMP_SUPPORTED
	OP_STATUS opstat = (OP_STATUS)op_setjmp(g_setjmp_buffer);
	if (OpStatus::IsError(opstat))
	{
		imgdec->abort_status = opstat;
		return 0;
	}
#endif // JPEG_SETJMP_SUPPORTED

	if (imgdec->cinfo.output_scanline < imgdec->cinfo.output_height)
 	{
		UINT32 pix_color;
	 	if (bits_per_pixel >= 8)
		{
			int rows_read;
#ifdef JPEG_SETJMP_SUPPORTED
			rows_read = jpeg_read_scanlines(&imgdec->cinfo, line, 1);
#else
            TRAPD(err,rows_read=jpeg_read_scanlines(&imgdec->cinfo, line, 1));// May Leave
            if (err)
	        {
		        imgdec->abort_status = err;
		        return FALSE;
	        }
#endif // JPEG_SETJMP_SUPPORTED
			int row = 0;
			while (row < rows_read)
			{
				//Now convert the row to RGBA and inform the listener that we have a new line...
				UINT8* data = line[row];
				UINT8* dest = imgdec->bitmap_lineRGBA;
				if(imgdec->cinfo.jpeg_color_space == JCS_GRAYSCALE || imgdec->want_indexed)
#ifdef SUPPORT_INDEXED_OPBITMAP
				{
				}
# else
				{
					unsigned char gray_val;
					for(unsigned int i=0;i<imgdec->width;i++)
					{
						gray_val = *data;
						pix_color = 0xFF000000 | (gray_val << 16) | (gray_val << 8) | (gray_val);
						*(UINT32 *)dest = pix_color;
						dest += 4;
						data++;
					}
				}
# endif // SUPPORT_INDEXED_OPBITMAP
				else if (imgdec->cinfo.jpeg_color_space == JCS_RGB)
				{
					for(unsigned int i=0;i<imgdec->width;i++)
					{
						pix_color = 0xFF000000 | (data[i*3] << 16) | (data[i*3+1] << 8) | (data[i*3+2]);
						*(UINT32 *)dest = pix_color;
						dest += 4;
					}
				}
				else
				{
					for(unsigned int i=0;i<imgdec->width;i++)
					{
#ifdef USE_SYSTEM_JPEG
						pix_color = 0xFF000000 | (data[i*3] << 16) | (data[i*3+1] << 8) | (data[i*3+2]);
#else
						pix_color = 0xFF000000 | (data[i*3+2] << 16) | (data[i*3+1] << 8) | (data[i*3]);
#endif
						*(UINT32 *)dest = pix_color;
						dest += 4;
					}
				}
				int therow = imgdec->current_scanline + row;
				imgdec->m_imageDecoderListener->OnLineDecoded(
#ifdef SUPPORT_INDEXED_OPBITMAP
					imgdec->want_indexed ? line[row] : imgdec->bitmap_lineRGBA,
#else
					imgdec->bitmap_lineRGBA,
#endif // SUPPORT_INDEXED_OPBITMAP
					therow, 1);

				row++;
			}
			return rows_read;
		}
		else  // I doubt that this piece of code works, but I need a testcase (suspect this is dead code..)
		{
			int row = 0;
			unsigned int col;

			while (row < height)
			{
                int ret;
#ifdef JPEG_SETJMP_SUPPORTED
				ret = jpeg_read_scanlines(&imgdec->cinfo, &imgdec->scratch_buffer, 1);
#else // JPEG_SETJMP_SUPPORTED
				TRAPD(err,ret=jpeg_read_scanlines(&imgdec->cinfo, &imgdec->scratch_buffer, 1)); // May Leave
                if (err)
	            {
		            imgdec->abort_status = err;
		            return FALSE;
	            }
#endif // JPEG_SETJMP_SUPPORTED

                if (ret)
				{
					unsigned char* buf = line[row++];
					int shift = 8;
					int bit = 0;
					int advance = bits_per_pixel;
					// Pack pixels into requested bpp

					for (col = 0; col < imgdec->cinfo.output_width; ++col)
					{
						shift -= advance;
						bit |= imgdec->scratch_buffer[col] << shift;
						if (!shift)
						{
							*buf++ = bit;
							bit = 0;
							shift = 8;
						}
					}

					// Pack last pixel if it not written

					if (shift)
					{
						*buf++ = bit;
					}

					//Now convert the row to RGBA and inform the listener that we have a new line...
					UINT8* data = imgdec->scratch_buffer;
					UINT8* dest = imgdec->bitmap_lineRGBA;
					if(imgdec->cinfo.jpeg_color_space == JCS_GRAYSCALE || imgdec->want_indexed)
#ifdef SUPPORT_INDEXED_OPBITMAP
					{
					}
#else
					{
						unsigned char gray_val;
						for(unsigned int i=0;i<imgdec->width;i++)
						{
							gray_val = *data;
							pix_color = 0xFF000000 | (gray_val << 16) | (gray_val << 8) | (gray_val);
							*(UINT32 *)dest = pix_color;
							dest += 4;
							data++;
						}
					}
#endif // SUPPORT_INDEXED_OPBITMAP
					else
					{
						for(unsigned int i=0;i<imgdec->width;i++)
						{
							pix_color = 0xFF000000 | (data[i*3+2] << 16) | (data[i*3+1] << 8) | (data[i*3]);
							*(UINT32 *)dest = pix_color;
							dest += 4;
						}
					}
					int therow = imgdec->current_scanline+row-1;
					imgdec->m_imageDecoderListener->OnLineDecoded(
#ifdef SUPPORT_INDEXED_OPBITMAP
						imgdec->want_indexed ? imgdec->scratch_buffer : imgdec->bitmap_lineRGBA,
#else
						imgdec->bitmap_lineRGBA,
#endif // SUPPORT_INDEXED_OPBITMAP
						therow, 1);
				}
            }
			return row;
		}
	}
	return 0;
}

void Destroy(ImageDecoderJpg* imgdec)
{
	if (!imgdec->initialized)
		return;

#ifdef JPEG_SETJMP_SUPPORTED
	if (op_setjmp(g_setjmp_buffer))
		goto destroy;
#endif // JPEG_SETJMP_SUPPORTED

#ifdef JPEG_SETJMP_SUPPORTED
	jpeg_finish_decompress(&imgdec->cinfo);
#else // JPEG_SETJMP_SUPPORTED
	TRAPD(ignore,jpeg_finish_decompress(&imgdec->cinfo)); // May Leave
	OpStatus::Ignore(ignore);
#endif // JPEG_SETJMP_SUPPORTED
#ifdef JPEG_SETJMP_SUPPORTED
destroy:
#endif // JPEG_SETJMP_SUPPORTED
	jpeg_destroy_decompress(&imgdec->cinfo); // Cannot Leave
}

int ConsumeInput(ImageDecoderJpg* imgdec)
{
#ifdef JPEG_SETJMP_SUPPORTED
	OP_STATUS opstat = (OP_STATUS)op_setjmp(g_setjmp_buffer);
	if (OpStatus::IsError(opstat))
	{
		imgdec->abort_status = opstat;
		return JPEG_SUSPENDED;
	}
#endif // JPEG_SETJMP_SUPPORTED

	int ret;
#ifdef JPEG_SETJMP_SUPPORTED
	ret = jpeg_consume_input(&imgdec->cinfo);
#else // JPEG_SETJMP_SUPPORTED
    TRAPD(err,ret=jpeg_consume_input(&imgdec->cinfo)); // May Leave
    if (err)
	{
		imgdec->abort_status = err;
		return JPEG_SUSPENDED;
	}
#endif // JPEG_SETJMP_SUPPORTED
    return ret;
}

// When last frame is loaded, there's no use in continuing decoding old frames
BOOL DiscardFrame(ImageDecoderJpg* imgdec)
{
#ifdef JPEG_SETJMP_SUPPORTED
	OP_STATUS opstat = (OP_STATUS)op_setjmp(g_setjmp_buffer);
	if (OpStatus::IsError(opstat))
	{
		imgdec->abort_status = opstat;
		return FALSE;
	}
#endif // JPEG_SETJMP_SUPPORTED

	if(imgdec->reading_data &&
		imgdec->start_scan &&
		imgdec->cinfo.input_scan_number != imgdec->start_scan)
	{
#ifdef JPEG_SETJMP_SUPPORTED
		jpeg_finish_output(&imgdec->cinfo);
#else // JPEG_SETJMP_SUPPORTED
		TRAPD(err,jpeg_finish_output(&imgdec->cinfo)); // May Leave
        if (err)
	    {
		    imgdec->abort_status = err;
		    return FALSE;
	    }
#endif // JPEG_SETJMP_SUPPORTED
		return TRUE;
	}
	else
		return FALSE;
}

BOOL DecodeFrame(ImageDecoderJpg* imgdec)
{
	if (imgdec->cinfo.buffered_image && imgdec->need_start)
	{
		// If we are doing progressive JPEGs, we need to start frames
		if(!StartFrame(imgdec))
			// Suspended
			return FALSE;

		imgdec->need_start = FALSE;
		imgdec->current_scanline = 0;
	}

	int height = imgdec->height;

	while (imgdec->current_scanline < height)
	{
		int lines_read = GetRows(imgdec, height - imgdec->current_scanline);

		if (lines_read <= 0)
			// Suspended
			return FALSE;

		imgdec->current_scanline += lines_read;
	}

	if (imgdec->cinfo.buffered_image)
	{
		// If we are doing progressive JPEGs, we need to finish frames
		if (!FinishFrame(imgdec))
			// Suspended
			return FALSE;

		imgdec->need_start = TRUE;
	}

	return TRUE;
}

/*****************************
     ImageDecoder section
******************************/


ImageDecoder* create_jpeg_decoder(void)
{
	return OP_NEW(ImageDecoderJpg, (TRUE, TRUE, FALSE,
									ImageDecoderJpg::DCT_INTEGER,
									ImageDecoderJpg::DITHERING_NONE));
}


ImageDecoderJpg::ImageDecoderJpg(BOOL progressive, BOOL smooth, BOOL one_pass,
				DCT_METHOD dct_method, DITHERING_MODE dithering_mode)
{
	this->progressive = progressive;
	this->smooth = smooth;
	this->one_pass = one_pass;
	this->dct_method = dct_method;
	this->dithering_mode = dithering_mode;

	m_imageDecoderListener=0;
	initialized = FALSE;
	finished = FALSE;
	header_loaded = FALSE;
	abort_status = OpStatus::OK;
	is_progressive = FALSE;
	decompress_started = FALSE;
	bits_per_pixel = 0;
	start_scan = 0;
	scratch_buffer = 0;
	reading_data = FALSE;
	need_start = TRUE;

	want_indexed = FALSE;

	current_scanline = 0;

	bitmap_array = 0;
	bitmap_lineRGBA = 0;

	op_memset(&cinfo, 0, sizeof(jpeg_decompress_struct));
	op_memset(&jerr_pub, 0, sizeof(jpeg_error_mgr));
	op_memset(&jsrc, 0, sizeof(jpg_img_src_mgr));
	width = 0;
	height = 0;
}

ImageDecoderJpg::~ImageDecoderJpg()
{
	jsrc.buffer = NULL;
	jsrc.buf_len = 0;
	jsrc.skip_len = 0;
	jsrc.pub.bytes_in_buffer = 0;
	jsrc.pub.next_input_byte = NULL;
	jsrc.more = FALSE;

	Destroy(this);

	OP_DELETEA(bitmap_array);
	OP_DELETEA(bitmap_lineRGBA);
	// Should we delete scratch_buffer or is it done by jpeg_destroy_decompress or jpeg_finish_decompress ?
}

int ImageDecoderJpg::GetNrOfBits(UINT32 nr)
{
	int count = 0;
	while (nr >= 1)
	{
		count++;
		nr /= 2;
	}
	return count;
}

BOOL ImageDecoderJpg::IsSizeTooLarge(UINT32 width, UINT32 height)
{
	if (width < 16384 && height < 16384)
	{
		return FALSE;
	}
	return GetNrOfBits(width) + GetNrOfBits(height) > 30;
}

OP_STATUS ImageDecoderJpg::DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL/* load_all = FALSE*/)
{
	resendBytes = 0;
	RETURN_IF_ERROR(abort_status);

	BOOL complete = FALSE;
	if(!initialized)
	{
		jsrc.skip_len = 0;

		// This just checks if the beginning of the file is the initial SOI marker
		//if (! (((unsigned char*) data)[0] == 0xff &&
		//	((unsigned char*) data)[1] == 0xd8))
		//	return 0;

		OP_STATUS ret = InitJPEG(this);
		if(OpStatus::IsError(ret))
		{
			// Failure
			Destroy(this);
			return ret;
		}
		initialized = TRUE;
	}
	jsrc.more = more;
	if ((long)numBytes <= jsrc.skip_len)
	{
		jsrc.skip_len -= numBytes;
		return OpStatus::OK; //Is it OK or ERR here ?
	}
	jsrc.buffer = (unsigned char*) ((UINT8*)data) + (int)jsrc.skip_len;
	jsrc.buf_len = numBytes - (int)jsrc.skip_len;
	jsrc.skip_len = 0;
	jsrc.pub.bytes_in_buffer = jsrc.buf_len;
	jsrc.pub.next_input_byte = jsrc.buffer;

	if(!header_loaded)
	{
		// Try to read header

		int jpg_header_ret;
		jpg_header_ret = ReadHeader(this);
		if (OpStatus::IsError(abort_status))
		{
			// Corrupted image
			Destroy(this); // Added 2001-08-20
			return abort_status;
		}
		else
		{
			if (jpg_header_ret == JPEG_SUSPENDED)
			{
				// Need more data to be able to decode the header.

				// Reset.
				initialized = FALSE;
				jpeg_destroy_decompress(&cinfo); // Cannot Leave

				resendBytes = numBytes;
				return OpStatus::OK;
			}
		}

		//Allocate enough temporary memory for the picture.
		width = cinfo.image_width;
		height = cinfo.image_height;

		if (IsSizeTooLarge(width, height))
		{
			Destroy(this); // Added 2001-08-20
			return OpStatus::ERR_NO_MEMORY;
		}

		int bpl = width * cinfo.num_components;

		bitmap_array = OP_NEWA(UINT8, bpl);
		if (bitmap_array == NULL)
		{
			Destroy(this); // Added 2001-08-20
			return OpStatus::ERR_NO_MEMORY;
		}

		bitmap_lineRGBA = OP_NEWA(UINT8, width*4);
		if (bitmap_lineRGBA == NULL)
		{
			Destroy(this); // Added 2001-08-20
			return OpStatus::ERR_NO_MEMORY;
		}

		bits_per_pixel = 8;
		if (cinfo.num_components >= 3)
		{
			bits_per_pixel = 24;
		}

		//Inform the listener...
		m_imageDecoderListener->OnInitMainFrame(width, height);

#ifdef SUPPORT_INDEXED_OPBITMAP
		if (bits_per_pixel == 8)
			want_indexed = TRUE;
		else
			want_indexed = FALSE;

		UINT8* pal = NULL;

		if (want_indexed && cinfo.jpeg_color_space == JCS_GRAYSCALE)
		{
			pal = (UINT8*) op_malloc (256*3); // Grayscale
			if (!pal)
			{
				Destroy(this);
				return OpStatus::ERR_NO_MEMORY;
			}
			int i,j;
			for (i = 0, j = 0; i < 256; i++, j+=3)
				op_memset(pal + j, i, 3);
		}
#endif // SUPPORT_INDEXED_OPBITMAP

		ImageFrameData image_frame_data;
		image_frame_data.rect.width = width;
		image_frame_data.rect.height = height;
		image_frame_data.interlaced = cinfo.progressive_mode;
		image_frame_data.bits_per_pixel = bits_per_pixel;
#ifdef SUPPORT_INDEXED_OPBITMAP
		if (want_indexed)
		{
			image_frame_data.pal = pal;
			image_frame_data.num_colors = 256;
		}
#endif // SUPPORT_INDEXED_OPBITMAP

		m_imageDecoderListener->OnNewFrame(image_frame_data);

#ifdef SUPPORT_INDEXED_OPBITMAP
		op_free(pal);
		pal = NULL;
#endif

		quantize = FALSE;

		header_loaded = TRUE;
	}
	if (!decompress_started)
	{
		J_DCT_METHOD jdct_method;
		J_DITHER_MODE jdither_mode;
		switch(dct_method)
		{
			case DCT_INTEGER:		jdct_method = JDCT_ISLOW; break;
			case DCT_FAST_INTEGER:	jdct_method = JDCT_IFAST; break;
			default:			jdct_method = JDCT_FLOAT; break;
		}
		switch (dithering_mode)
		{
			case DITHERING_FLOYD_STEINBERG:	jdither_mode = JDITHER_FS; break;
			case DITHERING_ORDERED:			jdither_mode = JDITHER_ORDERED; break;
			default:				jdither_mode = JDITHER_NONE; break;
		}
		if (cinfo.jpeg_color_space == JCS_CMYK ||
			cinfo.jpeg_color_space == JCS_YCCK)
		{
			// We can't handle 4 component images, so truncate to RGB
			cinfo.out_color_space = JCS_RGB;
		}
		cinfo.buffered_image = cinfo.progressive_mode;

		unsigned char **jpal = cinfo.colormap; //handle grayscale etc.!

		if (jpal)
			quantize = TRUE;

		int jpg_start_ret = Start(this, jpal, quantize, bits_per_pixel,
								  one_pass, jdither_mode, jdct_method, !smooth);
		if (!jpg_start_ret) // Need more data
		{
			if (OpStatus::IsError(abort_status))
			{
				// Something failed
				Destroy(this);
				return abort_status;
			}
			resendBytes = cinfo.src->bytes_in_buffer;
			return OpStatus::OK;
		}
		decompress_started = TRUE;
	}
	if (decompress_started)
	{
		BOOL suspended = FALSE;

		if (cinfo.buffered_image)
		{
			while (!suspended)
				switch (ConsumeInput(this))
				{
				case JPEG_REACHED_EOI:
					complete = TRUE;

					if (cinfo.buffered_image)
						// When last frame is loaded, there's no use in continuing decoding old frames

						if (DiscardFrame(this))
							need_start = TRUE;
						else
							if (OpStatus::IsError(abort_status))
							{
								// Something failed
								Destroy(this);
								return abort_status;
							}

				case JPEG_SUSPENDED:
					suspended = TRUE;
					break;
				}

			DecodeFrame(this);
		}
		else
			complete = DecodeFrame(this);
	}

	//temp if (complete) kilsmo, change that might go in later
	if (complete || !more || OpStatus::IsError(abort_status))
	{
		//Inform the listener...
		m_imageDecoderListener->OnDecodingFinished();
		Destroy(this); // Added 2001-08-20
		return abort_status;
	}
	else
	{
		resendBytes = cinfo.src->bytes_in_buffer;
		return OpStatus::OK;
	}
}

void ImageDecoderJpg::SetImageDecoderListener(ImageDecoderListener* imageDecoderListener)
{
	m_imageDecoderListener = imageDecoderListener;
}

#endif // USE_SYSTEM_JPEG

#endif // INTERNAL_JPG_SUPPORT
