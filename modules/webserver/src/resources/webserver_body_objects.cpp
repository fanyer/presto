/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007 - 2010
 *
 * Web server implementation 
 */
#include "core/pch.h"

#if defined(WEBSERVER_SUPPORT) || defined(DOM_GADGET_FILE_API_SUPPORT)

#include "modules/webserver/webserver_body_objects.h"
#include "modules/util/opfile/opfile.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/html.h"
#include "modules/libvega/src/canvas/canvas.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/webserver/src/webservercore/webserver_private_global_data.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/pi/OpBitmap.h"

WebserverBodyObject_Base::WebserverBodyObject_Base()
	: m_hasBeenInitiated(FALSE)
	, m_flushIsFinishedCallback(NULL)
	, m_flushId(static_cast<unsigned>(~0))
	, m_objectFlushedCallback(NULL)
{	
}

/* static */
OP_STATUS WebserverBodyObject_Base::MakeBodyObjectFromHtml(WebserverBodyObject_Base *&bodyObject, HTML_Element *htmlElement, const uni_char *image_type, int xsize, int ysize, int quality)
{
	bodyObject = NULL;
	HTML_ElementType type = htmlElement->Type();	
#ifdef CANVAS_SUPPORT
	if ( type == HE_CANVAS)
	{
		Canvas *canvas;
		WebserverBodyObject_Canvas *canvasBodyObject;
		canvas = static_cast<Canvas*>(htmlElement->GetSpecialAttr(Markup::VEGAA_CANVAS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_OGP));	

		RETURN_IF_ERROR(WebserverBodyObject_Canvas::Make(canvasBodyObject, canvas));

		bodyObject = canvasBodyObject;
	} 
	else 
#endif // CANVAS_SUPPORT
	if (type == HE_IMG)
	{
		WebserverBodyObject_Image *imageBodyObject;
		URL url = htmlElement->GetImageURL();
		Image img =	UrlImageContentProvider::GetImageFromUrl(url);

		BOOL img_type_jpg = FALSE;

		if (image_type == NULL || uni_str_eq(image_type, UNI_L("jpg")) || uni_str_eq(image_type, UNI_L("jpeg")))
			img_type_jpg = TRUE;

		RETURN_IF_ERROR(WebserverBodyObject_Image::Make(imageBodyObject, img, img_type_jpg, xsize, ysize, quality));

		bodyObject = imageBodyObject;
	}
	else
	{
		OP_ASSERT(!"The convertion from this html type to a body object is not implemented");
		return OpStatus::ERR;	
	}

	return OpStatus::OK;
}

OP_STATUS WebserverBodyObject_Base::InitiateGettingData()
{ 
	if (m_hasBeenInitiated == FALSE)
	{
		RETURN_IF_ERROR(InitiateInternalData());
		m_hasBeenInitiated = TRUE;
	}
	return OpStatus::OK;
}

void WebserverBodyObject_Base::SetFlushFinishedCallback(FlushIsFinishedCallback *callback, unsigned int flushId)
{
	m_flushIsFinishedCallback = callback;
	m_flushId = flushId;
}
 
OP_STATUS WebserverBodyObject_Base::SendFlushFinishedCallback()
{
	OP_STATUS status = OpStatus::OK;
	if (m_flushIsFinishedCallback != NULL)
	{
		OP_ASSERT(m_flushId != (unsigned int)~0);
		if (OpStatus::IsSuccess(status = m_flushIsFinishedCallback->FlushIsFinished(m_flushId)))
		{	
			m_flushIsFinishedCallback = NULL;
			m_flushId = static_cast<unsigned>(~0);
		}
	}
	
	return status;
}

 
OP_STATUS WebserverBodyObject_Base::SendFlushCallbacks()
{
	OP_STATUS status1 = SendFlushFinishedCallback();
	
	OP_STATUS status2 = OpStatus::OK;
	if (m_objectFlushedCallback != NULL)
	{
		if (OpStatus::IsSuccess(status2 = m_objectFlushedCallback->BodyObjectFlushed(this)))
		{
			m_objectFlushedCallback = NULL;
		}
	}
	
	OP_ASSERT(status1 != OpStatus::ERR);
	OP_ASSERT(status2 != OpStatus::ERR);
	
	RETURN_IF_ERROR(status1);
	RETURN_IF_ERROR(status2);
	
	return OpStatus::OK;
}

WebserverBodyObject_Canvas::WebserverBodyObject_Canvas(Canvas *canvas)
	: m_canvas(canvas)
	, m_bytesOfLineServed(0)
{
	minpng_init_encoder_result(&m_res);
	minpng_init_encoder_feeder(&m_feeder);
}

/* virtual */
WebserverBodyObject_Canvas::~WebserverBodyObject_Canvas()
{
	if (m_canvas_bitmap != NULL)
	{
		OP_DELETEA(m_feeder.scanline_data);
	}
	
	minpng_clear_encoder_result(&m_res);
	minpng_clear_encoder_feeder(&m_feeder);
}

	
/* static */ 
OP_STATUS WebserverBodyObject_Canvas::Make(WebserverBodyObject_Canvas *&canvasBodyObject, Canvas *canvas)
{
	canvasBodyObject = NULL;
	
	if (!canvas)
	{
		return OpStatus::ERR;
	}

	canvasBodyObject = OP_NEW(WebserverBodyObject_Canvas, (canvas));
	if 	(!canvasBodyObject) 
	{
		return OpStatus::ERR_NO_MEMORY;
	}	

	return OpStatus::OK;
}

/* virtual */
OP_STATUS WebserverBodyObject_Canvas::InitiateInternalData()
{
#ifdef CANVAS_SUPPORT
	m_canvas_bitmap = m_canvas->GetOpBitmap();		
	
	if (m_canvas_bitmap == NULL) 
	{
		return OpStatus::OK;
	}	

	m_feeder.has_alpha = 1;
	m_feeder.scanline = 0;
	m_feeder.xsize = m_canvas_bitmap->Width();
	m_feeder.ysize = m_canvas_bitmap->Height();
	
	OP_ASSERT(m_feeder.scanline_data == NULL);
	m_feeder.scanline_data = OP_NEWA(UINT32, m_canvas_bitmap->Width());
	
	if (m_feeder.scanline_data == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	m_canvas_bitmap->GetLineData(m_feeder.scanline_data, m_feeder.scanline);
	m_bytesOfLineServed = 0;
#endif // CANVAS_SUPPORT
	return OpStatus::OK;		
}

/* FIXME : It should be possible to reduce the size of MINPNG_ENCODE_BUFFER_SIZE (see modules/minpng/minpng_encoder.cpp) which is now 64*1024 */
/* FIXME : We must stop serving this if the html element that owns the canvas dies */
OP_BOOLEAN WebserverBodyObject_Canvas::GetData(int length, char *buf, int &lengthRead)
{
	
	OP_STATUS result = OpStatus::OK;
	
	if (m_canvas_bitmap == NULL)
	{
		lengthRead = 0;
		return OpBoolean::IS_FALSE;
	}
	
	BOOL again = TRUE;
	
	OP_ASSERT(m_hasBeenInitiated == TRUE);
	
	lengthRead = 0;
	PngEncRes::Result encodeReturnCode = PngEncRes::OK;

	while (again && lengthRead < length)
	{
		if (m_bytesOfLineServed == 0) /* only read new line if the whole previous line has been served out */
		{
			encodeReturnCode = minpng_encode(&m_feeder, &m_res);
			switch (encodeReturnCode)
			{
			case PngEncRes::ILLEGAL_DATA:
				result = OpStatus::ERR;
				// fall through
			case PngEncRes::OK:
				again = FALSE;
				// fall through
			case PngEncRes::AGAIN:
				break;
			case PngEncRes::OOM_ERROR:
				result = OpStatus::ERR_NO_MEMORY;
				again = FALSE;
				break;
			case PngEncRes::NEED_MORE:
				++m_feeder.scanline;
				if (m_feeder.scanline >= m_canvas_bitmap->Height())
				{
					again = FALSE;
					result = OpStatus::ERR;
					
				}
				else
					m_canvas_bitmap->GetLineData(m_feeder.scanline_data, m_feeder.scanline);
				break;
			}
		}
		
		if (OpStatus::IsError(result))
		{
			minpng_clear_encoder_result(&m_res);
			return OpStatus::ERR;
		}

		if (m_res.data_size != 0)
		{
			/* we server the all data in m_res.data */
			if ( (m_res.data_size - m_bytesOfLineServed) <  (unsigned int)(length - lengthRead) )
			{
				op_memcpy(buf+lengthRead, m_res.data + m_bytesOfLineServed, m_res.data_size - m_bytesOfLineServed);
				lengthRead += m_res.data_size - m_bytesOfLineServed;
				m_bytesOfLineServed = 0;		
				minpng_clear_encoder_result(&m_res);
			}
			else
			{
				op_memcpy(buf+lengthRead, m_res.data + m_bytesOfLineServed, length - lengthRead);
				m_bytesOfLineServed += length - lengthRead;
				lengthRead = length;
				again = FALSE;
			}
		}	
	}
	
	if (encodeReturnCode != PngEncRes::AGAIN && m_bytesOfLineServed == 0)
		return OpBoolean::IS_FALSE;
	
	return OpBoolean::IS_TRUE;
}
	
OP_STATUS WebserverBodyObject_Canvas::ResetGetData()
{
	if (m_canvas_bitmap == NULL)
		return OpStatus::OK;
	
	minpng_clear_encoder_result(&m_res);
	OP_DELETEA(m_feeder.scanline_data);
	m_feeder.scanline_data = NULL;
	minpng_clear_encoder_feeder(&m_feeder);	

	RETURN_IF_ERROR(InitiateInternalData());
	return OpStatus::OK;
}

OP_BOOLEAN WebserverBodyObject_Canvas::GetDataSize(int &size)
{
	return OpBoolean::IS_FALSE;
}

WebserverBodyObject_Image::WebserverBodyObject_Image(Image image, BOOL jpg, int width, int height, int quality)
	: m_image(image)
	, m_image_bitmap(NULL)
	, m_produce_jpg(jpg)
	, m_bytesOfLineServed(0)
#ifdef JAYPEG_ENCODE_SUPPORT
	, m_scanline_data(NULL)
	, m_buf(NULL)
	, m_line_number(-1)
	, m_data_length(0)
	, m_max_length(0)
	, m_max_length_reached(FALSE)
	, m_more(TRUE)
	, m_quality(quality)
#endif // JAYPEG_ENCODE_SUPPORT
{
	m_image.IncVisible(null_image_listener);
}

/* virtual */
WebserverBodyObject_Image::~WebserverBodyObject_Image()
{
	if (m_image_bitmap != NULL)
	{
		if (!m_produce_jpg)
			OP_DELETEA(m_feeder.scanline_data);

		m_image.ReleaseBitmap();
	}

	m_image.DecVisible(null_image_listener);

	if (!m_produce_jpg)
	{
		minpng_clear_encoder_result(&m_res);
		minpng_clear_encoder_feeder(&m_feeder);
	}
#ifdef JAYPEG_ENCODE_SUPPORT
	else
		OP_DELETEA(m_scanline_data);
#endif // JAYPEG_ENCODE_SUPPORT
}

/* static */
OP_STATUS WebserverBodyObject_Image::Make(WebserverBodyObject_Image *&ImageBodyObject, Image image, BOOL jpg, int width, int height, int quality)
{
	ImageBodyObject = NULL;

#ifndef JAYPEG_ENCODE_SUPPORT
	if (jpg)
		return OpStatus::ERR_NOT_SUPPORTED;
#endif // !JAYPEG_ENCODE_SUPPORT

	ImageBodyObject = OP_NEW(WebserverBodyObject_Image, (image, jpg, width, height, quality));
	if (!ImageBodyObject)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/* virtual */
OP_STATUS WebserverBodyObject_Image::InitiateInternalData()
{

	minpng_init_encoder_result(&m_res);
	minpng_init_encoder_feeder(&m_feeder);

	if (m_image_bitmap)
		m_image.ReleaseBitmap();
	m_image_bitmap = m_image.GetBitmap(null_image_listener);

	if (m_image_bitmap == NULL)
	{
		return OpStatus::OK;
	}

	if (!m_produce_jpg)
	{
		m_feeder.has_alpha = 1;
		m_feeder.scanline = 0;
		m_feeder.xsize = m_image_bitmap->Width();
		m_feeder.ysize = m_image_bitmap->Height();

		OP_ASSERT(m_feeder.scanline_data == NULL);
		m_feeder.scanline_data = OP_NEWA(UINT32, m_image_bitmap->Width());

		if (m_feeder.scanline_data == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		m_image_bitmap->GetLineData(m_feeder.scanline_data, m_feeder.scanline);
		m_bytesOfLineServed = 0;
	}
#ifdef JAYPEG_ENCODE_SUPPORT
	else
	{
		m_line_number = -1;
		if (m_scanline_data == NULL)
			m_scanline_data = OP_NEWA(UINT32, m_image_bitmap->Width());

		if (!m_scanline_data)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}
#endif // JAYPEG_ENCODE_SUPPORT

	return OpStatus::OK;
}

/* FIXME : It should be possible to reduce the size of MINPNG_ENCODE_BUFFER_SIZE (see modules/minpng/minpng_encoder.cpp) which is now 64*1024 */
/* FIXME : We must stop serving this if the html element that owns the Image dies */
OP_BOOLEAN WebserverBodyObject_Image::GetData(int length, char *buf, int &lengthRead)
{
	OP_STATUS result = OpStatus::OK;

	if (m_image_bitmap == NULL)
	{
		lengthRead = 0;
		return OpBoolean::IS_FALSE;
	}

	if (!m_produce_jpg)
	{
		BOOL again = TRUE;

		OP_ASSERT(m_hasBeenInitiated == TRUE);

		lengthRead = 0;
		PngEncRes::Result encodeReturnCode = PngEncRes::OK;

		while (again && lengthRead < length)
		{
			if (m_bytesOfLineServed == 0) /* only read new line if the whole previous line has been served out */
			{
				encodeReturnCode = minpng_encode(&m_feeder, &m_res);
				switch (encodeReturnCode)
				{
				case PngEncRes::ILLEGAL_DATA:
					result = OpStatus::ERR;
					// fall through
				case PngEncRes::OK:
					again = FALSE;
					// fall through
				case PngEncRes::AGAIN:
					break;
				case PngEncRes::OOM_ERROR:
					result = OpStatus::ERR_NO_MEMORY;
					again = FALSE;
					break;
				case PngEncRes::NEED_MORE:
					++m_feeder.scanline;
					if (m_feeder.scanline >= m_image_bitmap->Height())
					{
						again = FALSE;
						result = OpStatus::ERR;

					}
					else
						m_image_bitmap->GetLineData(m_feeder.scanline_data, m_feeder.scanline);
					break;
				}
			}

			if (OpStatus::IsError(result))
			{
				minpng_clear_encoder_result(&m_res);
				return OpStatus::ERR;
			}

			if (m_res.data_size != 0)
			{
				/* we server the all data in m_res.data */
				if ( (m_res.data_size - m_bytesOfLineServed) <  (unsigned int)(length - lengthRead) )
				{
					op_memcpy(buf+lengthRead, m_res.data + m_bytesOfLineServed, m_res.data_size - m_bytesOfLineServed);
					lengthRead += m_res.data_size - m_bytesOfLineServed;
					m_bytesOfLineServed = 0;
					minpng_clear_encoder_result(&m_res);
				}
				else
				{
					op_memcpy(buf+lengthRead, m_res.data + m_bytesOfLineServed, length - lengthRead);
					m_bytesOfLineServed += length - lengthRead;
					lengthRead = length;
					again = FALSE;
				}
			}
		}

		if (encodeReturnCode != PngEncRes::AGAIN && m_bytesOfLineServed == 0)
			return OpBoolean::IS_FALSE;
	}
#ifdef JAYPEG_ENCODE_SUPPORT
	else
	{
		m_buf = buf;
		OP_ASSERT(m_hasBeenInitiated == TRUE);

		m_data_length = 0;
		m_max_length = length;
		m_more = TRUE;
		m_max_length_reached = FALSE;

		if (m_line_number == -1)
		{
			RETURN_IF_ERROR(m_encoder.init("jfif", m_quality, m_image_bitmap->Width(), m_image_bitmap->Height(), this));
			m_line_number++;
		}

		while (m_more && !m_max_length_reached)
		{
			if ((m_more = m_image_bitmap->GetLineData(m_scanline_data, m_line_number)) == TRUE)
			{
				RETURN_IF_ERROR(m_encoder.encodeScanline(m_scanline_data));
			}

			m_line_number++;
		}
		m_buf = NULL;

		lengthRead = m_data_length;

		if (m_more || m_max_length_reached)
			return OpBoolean::IS_TRUE;
		else
			return OpBoolean::IS_FALSE;
	}
#endif // JAYPEG_ENCODE_SUPPORT

	return OpBoolean::IS_TRUE;
}

OP_STATUS WebserverBodyObject_Image::ResetGetData()
{
	if (m_image_bitmap == NULL)
		return OpStatus::OK;

	if (!m_produce_jpg)
	{
		minpng_clear_encoder_result(&m_res);
		OP_DELETEA(m_feeder.scanline_data);
		m_feeder.scanline_data = NULL;
		minpng_clear_encoder_feeder(&m_feeder);
	}
#ifdef JAYPEG_ENCODE_SUPPORT
	else
	{
		OP_DELETEA(m_scanline_data);
		m_scanline_data = NULL;
	}
#endif // JAYPEG_ENCODE_SUPPORT

	RETURN_IF_ERROR(InitiateInternalData());
	return OpStatus::OK;
}

#ifdef JAYPEG_ENCODE_SUPPORT
/* virtual */ OP_STATUS
WebserverBodyObject_Image::dataReady(const void* data, unsigned int datalen)
{
	if (m_data_length + datalen <= m_max_length)
	{
		op_memcpy(m_buf + m_data_length, data, datalen);
		m_data_length += datalen;
	}
	else if (m_line_number <= 0)
	{
		/* if line number is 0 the buffer cannot even contain one line. Width is too big */
		m_more = FALSE;
		m_max_length_reached = TRUE;
		OP_ASSERT(!"Image Width is too big for buffer");
		m_line_number = 0;
		return OpStatus::ERR;
	}
	else
	{
		/* FIXME: we must buffer the data that could not be copied to next time */
		m_line_number--;
		/* we stop encoding, and the line is encoded over again next time */
		m_max_length_reached = TRUE;
	}
	return OpStatus::OK;
}
#endif // JAYPEG_ENCODE_SUPPORT

OP_BOOLEAN WebserverBodyObject_Image::GetDataSize(int &size)
{
	return OpBoolean::IS_FALSE;
}

#ifdef JAYPEG_ENCODE_SUPPORT
// FIXME: This code does not work properly

WebserverBodyObject_CanvasToJpeg::WebserverBodyObject_CanvasToJpeg(Canvas *canvas, int quality)
	:	m_canvas(canvas)
	,	m_image_bitmap(NULL)
	,	m_scanline_data(NULL)
	,	m_buf(NULL)
	, 	m_line_number(-1)
	, 	m_data_length(0)
	,	m_max_length(0) 
	,	m_max_length_reached(FALSE)
	, 	m_more(TRUE)
	,	m_quality(quality)
{
}

WebserverBodyObject_CanvasToJpeg::~WebserverBodyObject_CanvasToJpeg()
{
	OP_DELETEA(m_scanline_data);
}

OP_STATUS WebserverBodyObject_CanvasToJpeg::Make(WebserverBodyObject_CanvasToJpeg *&imageBodyObject, Canvas *canvas, int quality)
{
	imageBodyObject = NULL;
	
	if (!canvas)
	{
		return OpStatus::ERR;
	}

	imageBodyObject = OP_NEW(WebserverBodyObject_CanvasToJpeg, (canvas, quality));
	if (!imageBodyObject)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

OP_BOOLEAN WebserverBodyObject_CanvasToJpeg::GetData(int length, char *buf, int &LengthRead)
{
	if (m_image_bitmap == NULL)
	{	
		LengthRead = 0;
		return OpBoolean::IS_FALSE;
	}
	m_buf = buf;	
	OP_ASSERT(m_hasBeenInitiated == TRUE);
	
	m_data_length = 0;
	m_max_length = length;
	m_more = TRUE;
	m_max_length_reached = FALSE;
	
	if (m_line_number == -1)
	{
		RETURN_IF_ERROR(m_encoder.init("jfif", m_quality, m_image_bitmap->Width(), m_image_bitmap->Height(), this));
		m_line_number++;	
	}
	
	while (m_more && !m_max_length_reached)
	{
		if ((m_more = m_image_bitmap->GetLineData(m_scanline_data, m_line_number)) == TRUE)
		{
			RETURN_IF_ERROR(m_encoder.encodeScanline(m_scanline_data));
		}
		
		m_line_number++;
	}
	m_buf = NULL;
	
	LengthRead = m_data_length;	

	if (m_more || m_max_length_reached)
		return OpBoolean::IS_TRUE;
	else
		return OpBoolean::IS_FALSE;	
}
	
OP_STATUS WebserverBodyObject_CanvasToJpeg::ResetGetData()
{

	OP_DELETEA(m_scanline_data);
	m_scanline_data = NULL;
	
	RETURN_IF_ERROR(InitiateInternalData());
	return OpStatus::OK;
}
	
OP_BOOLEAN WebserverBodyObject_CanvasToJpeg::GetDataSize(int &size)
{
	return OpBoolean::IS_FALSE;	
}


/* virtual */ OP_STATUS 
WebserverBodyObject_CanvasToJpeg::dataReady(const void* data, unsigned int datalen)
{
	if (m_data_length + datalen <= m_max_length)
	{
		op_memcpy(m_buf + m_data_length, data, datalen);
		m_data_length += datalen;
	}
	else if (m_line_number <= 0)
	{
		/* if line number is 0 the buffer cannot even contain one line. Width is too big */
		m_more = FALSE;
		m_max_length_reached = TRUE;
		OP_ASSERT(!"Image Width is too big for buffer");
		m_line_number = 0;
		return OpStatus::ERR;
	}
	else
	{
		/* FIXME: we must buffer the data that could not be copied to next time */
		m_line_number--;
		/* we stop encoding, and the line is encoded over again next time */ 
		m_max_length_reached = TRUE;
	}
	return OpStatus::OK;
}

/* virtual */  OP_STATUS 
WebserverBodyObject_CanvasToJpeg::InitiateInternalData()
{
	m_line_number = -1;
	m_image_bitmap = m_canvas->GetOpBitmap();		
	
	if (m_image_bitmap == NULL) 
	{
		return OpStatus::OK;
	}
	
	if (m_scanline_data == NULL)
		m_scanline_data = OP_NEWA(UINT32, m_image_bitmap->Width());
	
	return OpStatus::OK;
}

#endif // JAYPEG_ENCODE_SUPPORT

WebserverBodyObject_File::WebserverBodyObject_File()
{
}

/* virtual */
WebserverBodyObject_File::~WebserverBodyObject_File()
{
	m_file.Close();
}
		
/* static */
OP_STATUS WebserverBodyObject_File::Make(WebserverBodyObject_File *&fileBodyObject, const OpStringC16 &file_path)
{
	if (file_path == UNI_L("")) 
		return OpStatus::ERR;
	/* FIXME: normalize path here */
	fileBodyObject = OP_NEW(WebserverBodyObject_File, ());
	
	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	if ( fileBodyObject == NULL || OpStatus::IsError(status = fileBodyObject->m_file_path.Set(file_path)))
	{
		OP_DELETE(fileBodyObject);
		fileBodyObject = NULL;
		return status;
	}
	return OpStatus::OK;
}

/* virtual */
OP_STATUS WebserverBodyObject_File::InitiateInternalData()
{	
	RETURN_IF_MEMORY_ERROR(m_file.Construct(m_file_path.CStr()));
   	
    BOOL exists;
	m_file.Exists(exists);
	
	if (exists == FALSE)
	{
		return OpStatus::ERR;	
	}
	
	RETURN_IF_ERROR(m_file.Open(OPFILE_READ));
	
	return OpStatus::OK;
}

	/* From WebserverBodyObject_Base */
OP_BOOLEAN WebserverBodyObject_File::GetData(int length, char *buf, int &LengthRead)
{
	OP_ASSERT(m_hasBeenInitiated == TRUE);
	OpFileLength bytes_read;
	m_file.Read(buf, length, &bytes_read);
	LengthRead = static_cast<int>(bytes_read);
	if (bytes_read > 0)
		return m_file.Eof() == TRUE ? OpBoolean::IS_FALSE : OpBoolean::IS_TRUE;
	else
		return OpBoolean::IS_FALSE;
}

OP_STATUS WebserverBodyObject_File::ResetGetData()
{
	return m_file.SetFilePos(0);
}

OP_BOOLEAN WebserverBodyObject_File::GetDataSize(int &size)
{
	OpFileLength length;
	m_file.GetFileLength(length);
	size = static_cast<int>(length);
	return OpBoolean::IS_TRUE;
}

#ifdef WEBSERVER_SUPPORT
WebserverBodyObject_UploadedFile::WebserverBodyObject_UploadedFile(WebserverUploadedFile *uploadedFile)
	: m_uploaded_file(uploadedFile)
{
}

/*virtual */ 
WebserverBodyObject_UploadedFile::~WebserverBodyObject_UploadedFile()
{
}

/* static */
OP_STATUS WebserverBodyObject_UploadedFile::Make(WebserverBodyObject_UploadedFile *&uploadedFileObject, WebserverUploadedFile *uploadedFile)
{	
	if (uploadedFile == NULL)
		return OpStatus::ERR;
	
	WebserverBodyObject_UploadedFile *tmpfileBodyObject = OP_NEW(WebserverBodyObject_UploadedFile, (uploadedFile));
	
	if (!tmpfileBodyObject)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	uploadedFileObject = tmpfileBodyObject;

	return OpStatus::OK;
}

/* virtual */
OP_STATUS WebserverBodyObject_UploadedFile::InitiateInternalData()
{
	RETURN_IF_MEMORY_ERROR(m_file.Construct(m_uploaded_file.Ptr()->GetTemporaryFilePath()));
	
   	RETURN_IF_ERROR(m_file.Open(OPFILE_READ));
   	
    BOOL exists;
	m_file.Exists(exists);
	
	if (exists == FALSE)
	{
		return OpStatus::ERR;
	}	
	
	return OpStatus::OK;
}
#endif // WEBSERVER_SUPPORT

WebserverBodyObject_OpString8::WebserverBodyObject_OpString8()
	: m_length(0)
	, m_numberOfCharsRetrieved(0)
{}

/* virtual */
WebserverBodyObject_OpString8::~WebserverBodyObject_OpString8()
{}

/* static */ 
OP_STATUS WebserverBodyObject_OpString8::Make(WebserverBodyObject_OpString8 *&stringBodyObject, const OpStringC16 &bodyData)
{
	stringBodyObject = OP_NEW(WebserverBodyObject_OpString8, ());
	
	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	if (stringBodyObject == NULL || OpStatus::IsError(status = stringBodyObject->m_string.SetUTF8FromUTF16(bodyData.CStr())))
	{
		OP_DELETE(stringBodyObject);
		stringBodyObject= NULL;
		return status;
	}
	stringBodyObject->m_length = stringBodyObject->m_string.Length();
	
	return OpStatus::OK;	


}

/* static */ 
OP_STATUS WebserverBodyObject_OpString8::Make(WebserverBodyObject_OpString8 *&stringBodyObject, const OpStringC8 &bodyData)
{
	stringBodyObject = OP_NEW(WebserverBodyObject_OpString8, ());
	
	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	if (stringBodyObject == NULL || OpStatus::IsError(status = stringBodyObject->m_string.Set(bodyData)))
	{
		OP_DELETE(stringBodyObject);
		stringBodyObject = NULL;
		return status;
	}
	stringBodyObject->m_length = stringBodyObject->m_string.Length();
	
	return OpStatus::OK;	


}
		
/* virtual */
OP_STATUS WebserverBodyObject_OpString8::InitiateInternalData()
{
	m_numberOfCharsRetrieved = 0;	
	return OpStatus::OK;
}


OP_BOOLEAN WebserverBodyObject_OpString8::GetData(int length, char *buf, int &LengthRead)
{
	/* FIXME : We could  make buf point directly in to m_string.CStr(), and save memory*/
	
	int dataLeft = m_length - m_numberOfCharsRetrieved;
	OP_ASSERT(m_hasBeenInitiated == TRUE);
	if (dataLeft <= 0 )
	{
		LengthRead = 0;
		return OpBoolean::IS_FALSE;
	}
	
	if (dataLeft > length)
	{
		LengthRead = length;

		op_memcpy(buf, m_string.CStr() + m_numberOfCharsRetrieved, length);
		m_numberOfCharsRetrieved += LengthRead;
		return OpBoolean::IS_TRUE;
	}
	else
	{
		LengthRead = dataLeft;
		op_memcpy(buf, m_string.CStr()+ m_numberOfCharsRetrieved, dataLeft);
		m_numberOfCharsRetrieved += LengthRead;		
		return OpBoolean::IS_FALSE;
	}
}

OP_BOOLEAN WebserverBodyObject_OpString8::GetDataPointer(char *&dataPointer)
{
	dataPointer = m_string.CStr();
	return OpBoolean::IS_TRUE;
}


OP_STATUS WebserverBodyObject_OpString8::ResetGetData()
{
	m_numberOfCharsRetrieved = 0;
	return OpStatus::OK;
}

	
OP_BOOLEAN WebserverBodyObject_OpString8::GetDataSize(int &size)
{
	size = m_length;
	return OpBoolean::IS_TRUE;	
}

WebserverBodyObject_Binary::WebserverBodyObject_Binary()
	:  m_binaryString(NULL)
	  ,m_dataLength(0)
	  ,m_numberOfBytesRetrieved(0) 	
{	

}

WebserverBodyObject_Binary::~WebserverBodyObject_Binary()
{
	OP_DELETEA(m_binaryString);
}


/* static */
OP_STATUS WebserverBodyObject_Binary::Make(WebserverBodyObject_Binary *&binaryBodyObject, const unsigned char* data, unsigned int dataLength)
{	
	binaryBodyObject = OP_NEW(WebserverBodyObject_Binary, ());
	
	
	if (data != NULL && dataLength != 0)
	{
		if (binaryBodyObject == NULL || (binaryBodyObject->m_binaryString = OP_NEWA(unsigned char, dataLength)) == NULL)
		{
			OP_DELETE(binaryBodyObject);
			binaryBodyObject = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
		/* FIXME: with proper protecting code in webserver do not need to copy the binary vector */
		op_memcpy(binaryBodyObject->m_binaryString, data, dataLength);
		binaryBodyObject->m_dataLength = dataLength;
	}
	
	return OpStatus::OK;
}

OP_STATUS WebserverBodyObject_Binary::SetDataPointer(char *dataPointer, unsigned int dataLength)
{
	if (m_dataLength != 0 || m_binaryString != NULL)
		return OpStatus::ERR;
		
	OP_ASSERT(dataPointer != NULL && dataLength != 0);
	
	m_dataLength = dataLength;
	m_binaryString = (unsigned char*)dataPointer;
	return OpStatus::OK;
}


OP_BOOLEAN WebserverBodyObject_Binary::GetData(int length, char *buf, int &LengthRead)
{
	/* FIXME : We could  make buf point directly in to m_string.CStr(), and save memory*/
	
	int dataLeft = m_dataLength - m_numberOfBytesRetrieved;
	OP_ASSERT(m_hasBeenInitiated == TRUE);
	if (dataLeft <= 0 )
	{
		LengthRead = 0;
		return OpBoolean::IS_FALSE;
	}
	
	if (dataLeft > length)
	{
		LengthRead = length;

		op_memcpy(buf, m_binaryString + m_numberOfBytesRetrieved, length);
		m_numberOfBytesRetrieved += LengthRead;
		return OpBoolean::IS_TRUE;
	}
	else
	{
		LengthRead = dataLeft;
		op_memcpy(buf, m_binaryString + m_numberOfBytesRetrieved, dataLeft);
		m_numberOfBytesRetrieved += LengthRead;		
		return OpBoolean::IS_FALSE;
	}
}

OP_BOOLEAN WebserverBodyObject_Binary::GetDataPointer(char *&dataPointer)
{	
	dataPointer = (char*)m_binaryString;
	return OpStatus::OK;
}

OP_STATUS WebserverBodyObject_Binary::ResetGetData()
{
	m_numberOfBytesRetrieved = 0;
	return OpStatus::OK;
}

OP_BOOLEAN WebserverBodyObject_Binary::GetDataSize(int &size)
{
	size = m_dataLength;	
	return OpStatus::OK;
}

OP_STATUS WebserverBodyObject_Binary::InitiateInternalData()
{
	m_numberOfBytesRetrieved = 0;	
	return OpStatus::OK;
}

#endif // WEBSERVER_SUPPORT || DOM_GADGET_FILE_API_SUPPORT
