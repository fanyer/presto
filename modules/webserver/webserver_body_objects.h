/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2008
 *
 */

#ifndef WEBSERVER_BODY_OBJECTS_H_
#define WEBSERVER_BODY_OBJECTS_H_


#include "modules/webserver/webserver_resources.h"
#include "modules/mime/multipart_parser/abstract_mpp.h"
#include "modules/minpng/minpng_encoder.h"
#include "modules/util/opfile/opfile.h"
#include "modules/webserver/webresource_base.h"
#include "modules/webserver/webserver_callbacks.h"
#include "modules/img/image.h"

#ifdef JAYPEG_ENCODE_SUPPORT
# include "modules/jaypeg/jayencodeddata.h"
# include "modules/jaypeg/jayencoder.h"
#endif // JAYPEG_ENCODE_SUPPORT

class WebServerConnection;
class Canvas;
class WebserverUploadedFileWrapper;


/* Base class for serializing any objects to a bytestreams. Used by the webserver to serve out images etc.*/
class WebserverBodyObject_Base
{		
public:	
	/* Converts an htmlElement to a body object which can be serialized to binary data */
	static OP_STATUS MakeBodyObjectFromHtml(WebserverBodyObject_Base *&bodyObject, HTML_Element *htmlElement, const uni_char *image_type = NULL, int xsize = 0, int ysize = 0, int quality = 50);


	WebserverBodyObject_Base();
	virtual ~WebserverBodyObject_Base(){ OP_ASSERT( m_flushIsFinishedCallback == NULL && m_objectFlushedCallback == NULL); }	

	/* Initiate the Data in this object.
	 * 
	 * Must be called before GetData, GetDataSize GetDataPointer, GetDataSize is called. */	
	OP_STATUS InitiateGettingData();

	 /* Serialize and get data.
	  * 
	  * @param length the maximum data length.
	  * @return OpBoolean::IS_FALSE if there is no more data.
	  */ 	
	virtual OP_BOOLEAN GetData(int length, char *buf, int &LengthRead) = 0;

	/* Resets the data pointer */			
	virtual OP_STATUS ResetGetData() = 0;

	/* Get the total size of the data
	 *
	 *  @param size (out) the number of bytes int the object
	 * 
	 *  @return FALSE if size is unknown
	 */		
	virtual OP_BOOLEAN GetDataSize(int &size) { return OpBoolean::IS_FALSE; }
	 
	/* Get the internal data pointer directly.
	 * 
	 *  In case the object is already serialized in memory we can get all the data directly
	 * 
	 *  return IS_TRUE if the data pointer exists
	 *  return IS_FALSE if the data pointer does not exist.
	 *
	 *  If this returns IS_FALSE, GetData must be called until it returns IS_FALSE;
	 */
	virtual OP_BOOLEAN GetDataPointer(char *&dataPointer) { dataPointer = NULL; return OpBoolean::IS_FALSE; }
	
	/* Sends signal that all objects in the queue have been flushed  */
	OP_STATUS SendFlushFinishedCallback();
	
	/* Signal that this callback has been flushed */
	OP_STATUS SendFlushCallbacks();
	
protected:
	virtual OP_STATUS InitiateInternalData() = 0;
	
	BOOL m_hasBeenInitiated;

friend class WebResource_Custom;
private:	
	
	
	void SetFlushFinishedCallback(FlushIsFinishedCallback *callback, unsigned int flushId);


	BOOL hasFlushFinishedCallback() { return m_flushIsFinishedCallback != NULL ? TRUE : FALSE; } 

	FlushIsFinishedCallback *m_flushIsFinishedCallback;

	unsigned int m_flushId;
	 
	ObjectFlushedCallback	*m_objectFlushedCallback;
};

/* Body object with zero content. This is used in case server has called close with no body objects and only headers will be sent. Needed for flush finished callback*/
class WebserverBodyObject_Null : public WebserverBodyObject_Base
{
public:
	virtual OP_BOOLEAN GetData(int length, char *buf, int &LengthRead) { LengthRead = 0; return OpBoolean::IS_FALSE; }
	virtual OP_STATUS InitiateInternalData() { return OpStatus::OK; }
	virtual OP_STATUS ResetGetData(){ return OpStatus::OK; }
};


class WebserverBodyObject_Canvas : public WebserverBodyObject_Base
{
public:

	virtual ~WebserverBodyObject_Canvas();
		
	static OP_STATUS Make(WebserverBodyObject_Canvas *&canvasBodyObject, Canvas *canvas);

	/* From WebserverBodyObject_Base */
	OP_BOOLEAN GetData(int length, char *buf, int &LengthRead);
	
	OP_STATUS ResetGetData();
	
	OP_BOOLEAN GetDataSize(int &size);
private:
	WebserverBodyObject_Canvas(Canvas *canvas);
	virtual OP_STATUS InitiateInternalData();
	Canvas *m_canvas;
	OpBitmap *m_canvas_bitmap;
	PngEncFeeder m_feeder;
	PngEncRes m_res;
	int m_bytesOfLineServed;
};

/* Makes PNG or JPEG */
class WebserverBodyObject_Image : public WebserverBodyObject_Base
#ifdef JAYPEG_ENCODE_SUPPORT
								, public JayEncodedData
#endif // JAYPEG_ENCODE_SUPPORT
{
public:

	virtual ~WebserverBodyObject_Image();

	static OP_STATUS Make(WebserverBodyObject_Image *&imageBodyObject, Image image, BOOL jpg, int width, int height, int quality);

	/* From WebserverBodyObject_Base */
	OP_BOOLEAN GetData(int length, char *buf, int &LengthRead);

	OP_STATUS ResetGetData();

	OP_BOOLEAN GetDataSize(int &size);

#ifdef JAYPEG_ENCODE_SUPPORT
	/* From JayEncodedData */
	virtual OP_STATUS dataReady(const void* data, unsigned int datalen);
#endif // JAYPEG_ENCODE_SUPPORT

private:
	WebserverBodyObject_Image(Image image, BOOL jpg, int width, int height, int quality);
	virtual OP_STATUS InitiateInternalData();
	Image m_image;
	OpBitmap *m_image_bitmap;

	BOOL m_produce_jpg;

	PngEncFeeder m_feeder;
	PngEncRes m_res;
	int m_bytesOfLineServed;

#ifdef JAYPEG_ENCODE_SUPPORT
	UINT32 *m_scanline_data;
	JayEncoder m_encoder;
	char* m_buf;
	int m_line_number;
	unsigned int m_data_length;
	unsigned int m_max_length;
	BOOL m_max_length_reached;
	BOOL m_more;
	int m_quality;
#endif // JAYPEG_ENCODE_SUPPORT
};

#ifdef JAYPEG_ENCODE_SUPPORT
class WebserverBodyObject_CanvasToJpeg : public WebserverBodyObject_Base, public JayEncodedData
{
public:

	virtual ~WebserverBodyObject_CanvasToJpeg();
		
	static OP_STATUS Make(WebserverBodyObject_CanvasToJpeg *&imageBodyObject, Canvas *canvas, int quality);

	/* From WebserverBodyObject_Base */
	OP_BOOLEAN GetData(int length, char *buf, int &LengthRead);
	
	OP_STATUS ResetGetData();
	
	OP_BOOLEAN GetDataSize(int &size);

	/* From ImageListener */

	/* From JayEncodedData */
	virtual OP_STATUS dataReady(const void* data, unsigned int datalen);

private:
	WebserverBodyObject_CanvasToJpeg(Canvas *canvas, int quality);
	virtual OP_STATUS InitiateInternalData();
	Canvas *m_canvas;
	JayEncoder m_encoder;
	OpBitmap *m_image_bitmap;
	UINT32 *m_scanline_data;
//	PngEncFeeder m_feeder;
//	PngEncRes m_res;
	char* m_buf;
	int m_line_number;
	unsigned int m_data_length;
	unsigned int m_max_length;
	BOOL m_max_length_reached;
	BOOL m_more;
	int m_quality;
};
#endif // JAYPEG_ENCODE_SUPPORT

class WebserverBodyObject_File : public WebserverBodyObject_Base
{
public:

	virtual ~WebserverBodyObject_File();
		
	static OP_STATUS Make(WebserverBodyObject_File *&fileBodyObject, const OpStringC16 &file_name);

	/* From WebserverBodyObject_Base */
	OP_BOOLEAN GetData(int length, char *buf, int &LengthRead);	
	OP_STATUS ResetGetData();
	OP_BOOLEAN GetDataSize(int &size);
	
protected:
	virtual OP_STATUS InitiateInternalData();
	WebserverBodyObject_File();
	OpString m_file_path;
	OpFile m_file;
};

#ifdef WEBSERVER_SUPPORT
class WebserverBodyObject_UploadedFile : public WebserverBodyObject_File
{
public:

	virtual ~WebserverBodyObject_UploadedFile();
		
	static OP_STATUS Make(WebserverBodyObject_UploadedFile *&fileBodyObject, WebserverUploadedFile *file);
	
private:
	virtual OP_STATUS InitiateInternalData();
	WebserverBodyObject_UploadedFile(WebserverUploadedFile *uploadedFile);
	WebserverUploadedFileWrapper m_uploaded_file;
};
#endif // WEBSERVER_SUPPORT

class WebserverBodyObject_OpString8 : public WebserverBodyObject_Base
{
public:
	virtual ~WebserverBodyObject_OpString8();

	static OP_STATUS Make(WebserverBodyObject_OpString8 *&stringBodyObject, const OpStringC16 &bodyData);

	static OP_STATUS Make(WebserverBodyObject_OpString8 *&stringBodyObject, const OpStringC8 &bodyData);		
	/* From WebserverBodyObject_Base */
	OP_BOOLEAN GetData(int length, char *buf, int &LengthRead);
	OP_BOOLEAN GetDataPointer(char *&dataPointer);
	OP_STATUS ResetGetData();
	OP_BOOLEAN GetDataSize(int &size);

private: 
	virtual OP_STATUS InitiateInternalData();
	WebserverBodyObject_OpString8();

	int m_length;
	int m_numberOfCharsRetrieved;
	OpString8 m_string;
};

class WebserverBodyObject_Binary : public WebserverBodyObject_Base
{
public:
	~WebserverBodyObject_Binary();

	static OP_STATUS Make(WebserverBodyObject_Binary *&stringBodyObject, const unsigned char* data = NULL, unsigned int dataLength = 0);

	/* From WebserverBodyObject_Base */
	OP_BOOLEAN GetData(int length, char *buf, int &LengthRead);
	OP_BOOLEAN GetDataPointer(char *&dataPointer);
	OP_STATUS SetDataPointer(char *dataPointer, unsigned int dataLength); /* Takes over the dataPointer */
	OP_STATUS ResetGetData();
	OP_BOOLEAN GetDataSize(int &size);

private: 
	WebserverBodyObject_Binary();
	OP_STATUS InitiateInternalData();
	unsigned char *m_binaryString;
	unsigned int m_dataLength;
	unsigned int m_numberOfBytesRetrieved;
};

#endif /*WEBSERVER_BODY_OBJECTS_H_*/
