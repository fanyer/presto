/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2012
 *
 * Web server implementation -- script virtual resources
 */

#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT
#include "modules/stdlib/include/opera_stdlib.h"
#include "modules/dochand/winman.h"
#include "modules/webserver/webserver_body_objects.h"
#include "modules/webserver/webserver_callbacks.h"
#include "modules/minpng/minpng_encoder.h"
#include "modules/mime/multipart_parser/text_mpp.h"
#include "modules/webserver/webserver_request.h"
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/webserver/webserver-api.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/webserver/webserver_custom_resource.h"
#include "modules/webserver/src/webservercore/webserver_private_global_data.h"
#include "modules/util/tempbuf.h"
#include "modules/doc/frm_doc.h"
#include "modules/webserver/src/webservercore/webserver_connection.h"
#include "modules/formats/hdsplit.h"
#include "modules/formats/argsplit.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opautoptr.h"
#include "modules/encodings/utility/opstring-encodings.h"
#include "modules/libvega/src/canvas/canvas.h"
#include "modules/logdoc/urlimgcontprov.h"

WebResource_Custom::WebResource_Custom(WebServerConnection *service, WebserverRequest *requestObject, OpSocketAddress *socketAddress)
	: WebResource(service, requestObject, socketAddress)
	, m_incomingDataFile(NULL)
	, m_saveIncommingBodyToFile(TRUE)
	, m_closeRequest(FALSE)
	, m_requestClosed(FALSE)
	, m_hasStartedFlushing(FALSE)
	, m_chunkedEncoding(TRUE)
	, m_postBodyHasMultiparts(FALSE)
	, m_multiPartParser(NULL)
	, m_currentMultiPart(NULL)
	, m_currentMultiFile(NULL)
	, m_currentFlushObject(NULL)
	, m_hasSentConnectionClosedSignal(FALSE)
	, m_windowId(NO_WINDOW_ID)
	, m_incomingDataType(NONE)
{
	
	
}

/* virtual */
WebResource_Custom::~WebResource_Custom()
{
	OP_DELETE(m_multiPartParser);
	if (m_incomingDataFile)
	{
		if (m_incomingDataFile->IsOpen())
			m_incomingDataFile->Close();
		m_incomingDataFile->Delete();
		OP_DELETE(m_incomingDataFile);
	}
	
	if (m_currentMultiFile)
	{
		if (m_currentMultiFile->IsOpen())
			m_currentMultiFile->Close();
		m_currentMultiFile->Delete();		
		OP_DELETE(m_currentMultiFile);			
	}
	
	OP_DELETE(m_currentMultiPart);
	if (m_hasSentConnectionClosedSignal == FALSE)
	{
		OP_ASSERT(!"Signal closed has not been sent. should not happen");
		OpStatus::Ignore(OnConnectionClosed()); /* FIXME */
	}
	
	OP_ASSERT(m_currentFlushObject == NULL); 
	OP_DELETE(m_currentFlushObject);
}

OP_STATUS WebResource_Custom::OnConnectionClosed()
{
	OP_STATUS status = OpStatus::OK;
	OP_STATUS status1  = OpStatus::OK;
	OP_STATUS temp_status;
	WebserverBodyObject_Base *object;
	
	for (unsigned int index = 0; index < m_bodyObjects.GetCount() ; ++index)
	{
		object = m_bodyObjects.Get(index);
	
		if (OpStatus::IsMemoryError(temp_status = object->SendFlushCallbacks()))
		{
			status = temp_status;
		}
	}
	
	if (m_currentFlushObject != NULL)
	{	
		status1 = m_currentFlushObject->SendFlushCallbacks();
		OP_DELETE(m_currentFlushObject);
		m_currentFlushObject = NULL;
	}
			
	if ( /* Ensures that memory error has first prority */
			OpStatus::IsMemoryError(status) ||
			OpStatus::IsMemoryError(status1)
		)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	else
	{		
		RETURN_IF_ERROR(status1);
		RETURN_IF_ERROR(status);
	}
	
	m_hasSentConnectionClosedSignal = TRUE;
	return OpStatus::OK;
}

/* static */ WebResource*
WebResource_Custom::Make(WebServerConnection *service, const OpStringC16 &subServerRequestString, WebserverRequest *requestObject, unsigned long windowId, WebServerFailure *result)
{
	WebServerMethod method = requestObject->GetHttpMethod();
	const uni_char *requestResourceName = subServerRequestString.CStr();
	if (method != WEB_METH_GET && method != WEB_METH_HEAD && method != WEB_METH_POST )
	{
		*result = WEB_FAIL_METHOD_NOT_SUPPORTED;
		return NULL;
	}
	OpSocketAddress *socketAddress = 0;
	WebResource_Custom *resource = 0;

	OP_STATUS temp_status;
	
	while (requestResourceName[0] == '/')
		requestResourceName++;
	
	const uni_char *end = uni_strpbrk(requestResourceName, UNI_L(" /?#\\"));
	unsigned int requestResourceNameLength = end ? end - requestResourceName : uni_strlen(requestResourceName);
	
	Window *window = NULL;
	FramesDocument *frm_doc = NULL;
	DOM_Environment *environment = NULL;
	OpString eventName;

	window = g_windowManager->GetWindow(windowId);
	
	if (requestResourceName[0] == '_' || requestObject->GetForwardRequestState() == TRUE || window == NULL)
	{
		*result = WEB_FAIL_NO_FAILURE;
		return NULL;			
	}

	frm_doc = window->DocManager()->GetCurrentDoc();

	OP_ASSERT(frm_doc);

	if (frm_doc)
	{
		environment = frm_doc->GetDOMEnvironment();
	}
	
	temp_status = OpStatus::ERR;
	if (frm_doc == NULL || environment == NULL || OpStatus::IsError(temp_status = eventName.Set(requestResourceName, requestResourceNameLength)))
	{
		if (OpStatus::IsMemoryError(temp_status))
			*result = WEB_FAIL_OOM;
		else
			*result = WEB_FAIL_NO_FAILURE;
		
		return NULL;
	}

	if (eventName.Length() == 0 && OpStatus::IsError(eventName.Set("_index")))
	{
		*result = WEB_FAIL_OOM;
		return NULL;
	}
	
	if ( (environment->WebserverEventHasListeners(eventName.CStr()) == FALSE			&&
			environment->WebserverEventHasListeners(UNI_L("_request")) == FALSE) || 
			uni_str_eq(eventName.CStr(), UNI_L("_close")))
	{
		*result = WEB_FAIL_NO_FAILURE;
		return NULL;
	}
	
	

	if (OpStatus::IsError(OpSocketAddress::Create(&socketAddress)) || OpStatus::IsMemoryError(service->GetSocketAddress(socketAddress)))
	{
		OP_DELETE(socketAddress);
		*result = WEB_FAIL_OOM;
		return NULL;
	}

	resource = OP_NEW(WebResource_Custom, (service, requestObject, socketAddress)); /*resource takes ownership over socketAddress*/
	if (resource == NULL)
	{
		OP_DELETE(socketAddress);
		socketAddress = NULL;
		*result = WEB_FAIL_OOM;
		goto failure;
	}

	
	resource->m_windowId = windowId;

	if (OpStatus::IsError(resource->m_eventName.Set(eventName)))
	{
		*result = WEB_FAIL_OOM;
		goto failure;
	}		

	if (method == WEB_METH_HEAD)
	{
		/*According to method HEAD We send the headers without body (we do not execute the script)*/
		service->StartServingData();
	}
	else if (method == WEB_METH_GET )
	{	
		service->PauseServingData();
		if (OpStatus::IsError(temp_status = resource->sendEvent(windowId, resource->m_eventName.CStr())))
		{
			/*FIXME: for some reason it crashes if this failes */
			if (OpStatus::IsMemoryError(temp_status))
			{
				*result = WEB_FAIL_OOM;
			}
			else
			{
				*result = WEB_FAIL_GENERIC;	
			}
			goto failure;
		}

	}
	else if (method == WEB_METH_POST)
	{
		service->PauseServingData();

		if (OpStatus::IsError(temp_status = resource->InitiateHandlingPostBodyData()))
		{
			if (OpStatus::IsMemoryError(temp_status))
			{
				*result = WEB_FAIL_OOM;
			}
			else
			{
				*result = WEB_FAIL_GENERIC;	
			}
			goto failure;
		}
	}	
	
	if (OpStatus::IsError(resource->AddResponseHeader(UNI_L("Content-Type"), UNI_L(DEFAULT_CONTENT_TYPE))))
	{
		*result = WEB_FAIL_OOM;
		goto failure;					
	}
	*result = WEB_FAIL_NO_FAILURE;
	return resource;	
	
failure:
	if (resource)
	{
		OP_DELETE(resource);	
	}
	else
	{
		OP_DELETE(socketAddress);/*resource has been created it owns socketAddress, and will be deleted there*/
	}
	return NULL;					
}

OpVector<WebserverUploadedFileWrapper> *WebResource_Custom::GetMultipartVector()
{
  	return &m_multiParts;
}

OP_STATUS
WebResource_Custom::sendEvent(unsigned long id, const uni_char* event)
{
	Window *window = g_windowManager->GetWindow(id /*m_resourceDescriptor->GetWindowId()*/);	
	
	FramesDocument *frm_doc = window->DocManager()->GetCurrentDoc();
	OP_ASSERT(frm_doc);
	
	if (!frm_doc)
		return OpStatus::ERR;
	
	DOM_Environment *environment = frm_doc->GetDOMEnvironment();
	OP_ASSERT(environment);
	
	if (!environment)
		return OpStatus::ERR;
	RETURN_IF_ERROR(environment->SendWebserverEvent(event, this));

	return OpStatus::OK;
}


OP_STATUS
WebResource_Custom::InitiateHandlingPostBodyData()
{
	OP_ASSERT(m_service != NULL);
	OP_ASSERT(m_requestObject->GetHttpMethod() == WEB_METH_POST);	
	
	HeaderEntry *contentTypeHeader = m_requestObject->GetClientHeaderList()->GetHeader("Content-Type");
	m_incomingDataType = NONE;
	
	if (contentTypeHeader && op_strnicmp(contentTypeHeader->Value(), "multipart/", 10) == 0)	
	{	
		m_incomingDataType = MULTIPART;
		m_postBodyHasMultiparts = TRUE;
	}
	else if (contentTypeHeader && op_stristr(contentTypeHeader->Value(), "application/x-www-form-urlencoded"))
	{
		m_incomingDataType = FORMURLENCODED;
		m_saveIncommingBodyToFile = FALSE;
		/*FIXME: Must have a maximum size of body to prevent denial of service attacks*/
	}
	else if (contentTypeHeader && 
			(op_strnicmp(contentTypeHeader->Value(), "text/", 5) == 0 || 
			!op_strstr(contentTypeHeader->Value(), "/xml") || 
			!op_strstr(contentTypeHeader->Value(), "/xhtml") || 
			!op_strstr(contentTypeHeader->Value(), "+xml") || 
			!op_strstr(contentTypeHeader->Value(), "/svg") ||
			!op_strstr(contentTypeHeader->Value(), "/html")))
	{
		m_incomingDataType = TEXT;
		m_saveIncommingBodyToFile = TRUE;
		/*FIXME: Must have a maximum size of body to prevent denial of service attacks*/
	}
	else if (contentTypeHeader && op_strnicmp(contentTypeHeader->Value(), "application/", 12) == 0)
	{
		m_incomingDataType = BINARY;
		m_saveIncommingBodyToFile = TRUE;
		/*FIXME: Must have a maximum size of body to prevent denial of service attacks*/
	}
	
	
	/*First we create a file where the data is saved*/
	if (m_saveIncommingBodyToFile == TRUE)
	{
		/*save to file*/
		OP_ASSERT(m_incomingDataFile == NULL);
		if (m_incomingDataFile != NULL)
		{
			OP_DELETE(m_incomingDataFile);
			m_incomingDataFile = NULL;
		}

		m_incomingDataFile = OP_NEW(OpFile, ());
		if (m_incomingDataFile == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		/*FIXME: Raplace the hardcoded body file*/
		OpString fileName;

		/*FIXME Better file handling, this is a hack*/
		fileName.AppendFormat(UNI_L("webserver_tmpbody%x"), (INTPTR)this);
		RETURN_IF_MEMORY_ERROR(m_incomingDataFile->Construct(fileName.CStr(), OPFILE_TEMP_FOLDER));

		BOOL exists;
		m_incomingDataFile->Exists(exists);
		if (exists)
			OP_ASSERT(!"ERROR");
		/*FIXME: CHECK ERROR CODE*/

		RETURN_IF_MEMORY_ERROR(m_incomingDataFile->Open(OPFILE_WRITE));
	}
	
	if (m_postBodyHasMultiparts == TRUE)
	{
		ParameterList *parameters = NULL;
		if (contentTypeHeader)
			RETURN_IF_LEAVE(parameters = contentTypeHeader->GetParametersL((PARAM_SEP_COMMA | PARAM_SEP_SEMICOLON | PARAM_STRIP_ARG_QUOTES), KeywordIndex_HTTP_General_Parameters));	
		
		Parameters *boundaryParameter = NULL;
		const char *boundary = NULL;
		if (parameters && parameters->ParameterExists(HTTP_General_Tag_Boundary, PARAMETER_ASSIGNED_ONLY))
		{
			boundaryParameter = parameters->GetParameterByID(HTTP_General_Tag_Boundary, PARAMETER_ANY);
			if (boundaryParameter)
			{
				boundary = boundaryParameter->Value();
			}
		}
		
		OP_ASSERT(m_multiPartParser == NULL);
		if (m_multiPartParser)
		{
			OP_DELETE(m_multiPartParser);
			m_multiPartParser = NULL;
			return OpStatus::ERR;
		}
		
		m_multiPartParser = OP_NEW(TextMultiPartParser, (boundary, boundary ? op_strlen(boundary): 0));
		if (!m_multiPartParser)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	
	m_service->AcceptReceivingBody(TRUE);
	return OpStatus::OK;
}

OP_STATUS
WebResource_Custom::HandleIncommingData(const char *incommingBody, UINT32 bodyLength, BOOL lastData)
{
	/* FIXME : move all multipart encoding logic into WebServerConnection, since it is an http protocol issue */
	OP_ASSERT( (lastData == FALSE && (bodyLength & 1) == 0) || lastData == TRUE );
	OP_ASSERT(m_service != NULL);
	
	OP_STATUS status;
	
	if (m_saveIncommingBodyToFile == TRUE)
	{
		/*save to file*/
		OP_ASSERT(m_incomingDataFile != NULL);
		OP_ASSERT(m_incomingDataFile->IsOpen());
		if (m_incomingDataFile == NULL) 
			return OpStatus::ERR;
	
		OpFileLength length = bodyLength;
		RETURN_IF_ERROR(m_incomingDataFile->Write((const void*)incommingBody, length));
	}
	else
	{
		/*save to ram*/		
		OpString tempBody;
		RETURN_IF_ERROR(tempBody.SetFromUTF8(incommingBody, bodyLength));
		if (m_incomingDataType == FORMURLENCODED)
		{
			StrReplaceChars(tempBody.CStr(), '+', ' ');			
		}
		RETURN_IF_ERROR(m_incommingDataMem.Append(tempBody));
	}
		
	/*the data contains multiparts*/	
	if (m_postBodyHasMultiparts == TRUE)
	{
		/*Here we parse multiparts, if there are any*/	
		m_multiPartParser->load(incommingBody, bodyLength);
		
		if ((m_currentMultiPart == NULL && m_multiPartParser->beginNextPart()) || m_currentMultiPart != NULL)
		{
			do
			{ 	
				if (m_currentMultiPart == NULL)
				{
					HeaderList *headerList = m_multiPartParser->getCurrentPartHeaders();
					
					OP_ASSERT(m_currentMultiFile == NULL);
					m_currentMultiFile = OP_NEW(OpFile, ());
					if (!m_currentMultiFile)
					{
						return OpStatus::ERR_NO_MEMORY;				
					}			
	
					/*FIXME: Replace the hardcoded body file*/
					OpString fileName;
	
					RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_TEMP_FOLDER, fileName));
					
					/*FIXME Better file handling, this is a hack*/
					fileName.AppendFormat(UNI_L("webserver_tmpbody%x_%x"), (INTPTR)this, m_multiPartParser->getCurrentPartNumber());
	
					RETURN_IF_ERROR(m_currentMultiFile->Construct(fileName.CStr(), OPFILE_ABSOLUTE_FOLDER));
					RETURN_IF_ERROR(m_currentMultiFile->Open(OPFILE_WRITE));
					
					/* Find the type of the multipart */
					HeaderEntry *header = NULL;
					WebserverUploadedFile::MultiPartFileType multipartType = WebserverUploadedFile::WEB_FILETYPE_NOT_SET; 
					
					if (headerList && NULL!=(header = headerList->GetHeader("Content-Disposition")))
					{
						ParameterList *parameters = NULL;
						TRAPD(status, parameters = header->GetParametersL(PARAM_SEP_SEMICOLON | PARAM_STRIP_ARG_QUOTES | PARAM_SEP_WHITESPACE | PARAM_HAS_RFC2231_VALUES, KeywordIndex_HTTP_General_Parameters));
						RETURN_IF_ERROR(status);
						
						if (parameters)
						{
							if (parameters->GetParameter("filename", PARAMETER_ANY) != NULL && headerList->GetHeader("Content-Type"))
							{
								multipartType = WebserverUploadedFile::WEB_FILETYPE_FILE;
							}				
							else if (parameters->GetParameter("form-data", PARAMETER_ANY) != NULL)
							{
								multipartType = WebserverUploadedFile::WEB_FILETYPE_FORM_TEXT;
								
							}
							else
							{
								multipartType = WebserverUploadedFile::WEB_FILETYPE_UNKNOWN;
							}

						}
					}

					RETURN_IF_ERROR(WebserverUploadedFile::Make(m_currentMultiPart, headerList, fileName.CStr(), multipartType));
				
					OP_ASSERT(m_currentMultiPart);
				}

				if (m_currentMultiPart)
				{
					if (m_multiPartParser->availableCurrentPartData())
					{
						char *PartDataBuffer = OP_NEWA(char, bodyLength);
						if (PartDataBuffer == NULL)
							return OpStatus::ERR_NO_MEMORY;
						
						int PartDataLength = m_multiPartParser->getCurrentPartData(PartDataBuffer,bodyLength);
						if (m_currentMultiFile->IsOpen())
						{
							if (OpStatus::IsError(status = m_currentMultiFile->Write(PartDataBuffer,PartDataLength)))
							{
								OP_DELETEA(PartDataBuffer);
								return status;
							}
						}
						OP_DELETEA(PartDataBuffer);
					}
					
					if (m_multiPartParser->noMoreCurrentPartData() )
					{				
						m_multiPartParser->finishCurrentPart();
			
						WebserverUploadedFileWrapper *webserverUploadedFileWrapper = OP_NEW(WebserverUploadedFileWrapper, (m_currentMultiPart));
						if 	(webserverUploadedFileWrapper == 0)
							return OpStatus::ERR_NO_MEMORY;
			
						if (OpStatus::IsError(status = m_multiParts.Add(webserverUploadedFileWrapper)))
						{
							OP_DELETE(webserverUploadedFileWrapper);
							return status;
						}
						
						m_currentMultiPart = NULL;
	
						if (m_currentMultiFile->IsOpen())
							m_currentMultiFile->Close();
						else
						{
							return OpStatus::ERR;	
							
						}									
						OP_DELETE(m_currentMultiFile);
						m_currentMultiFile = NULL;
					}
				}
			} while (m_currentMultiPart == NULL && m_multiPartParser->beginNextPart());
		}
	}
	
	if (lastData == TRUE)
	{
		if ( m_postBodyHasMultiparts == TRUE )
		{
			OP_ASSERT(m_multiPartParser != NULL);
			m_multiPartParser->loadingFinished ();
			
			if (m_currentMultiFile)
			{
				if (m_currentMultiFile->IsOpen())
					m_currentMultiFile->Close();
				OP_DELETE(m_currentMultiFile);
				m_currentMultiFile = NULL;
			}
		}
		
		m_service->PauseServingData();
		
		RETURN_IF_ERROR(sendEvent(m_windowId, m_eventName.CStr()));
		
		if (m_saveIncommingBodyToFile == TRUE && m_incomingDataFile)
		{
			m_incomingDataFile->Close(); 
		}
	}

	return OpStatus::OK;
}

OP_STATUS WebResource_Custom::AppendBodyData(WebserverBodyObject_Base *bodyDataObject, ObjectFlushedCallback *callback)
{
	OP_ASSERT(bodyDataObject->m_objectFlushedCallback == 0);
	
	if (bodyDataObject->m_objectFlushedCallback != NULL)
	{
		return OpStatus::ERR;
	}
	
	if (OpStatus::IsError(m_bodyObjects.Add(bodyDataObject)))
	{
		return OpStatus::ERR_NO_MEMORY;
	}	
	
	bodyDataObject->m_objectFlushedCallback = callback;
	return OpStatus::OK;		
}

OP_STATUS WebResource_Custom::AppendBodyData(const OpStringC16 &bodyData)
{
	WebserverBodyObject_OpString8 *stringBodyObject;
	RETURN_IF_ERROR(WebserverBodyObject_OpString8::Make(stringBodyObject, bodyData));
	if (OpStatus::IsError(m_bodyObjects.Add(stringBodyObject)))
	{
		OP_DELETE(stringBodyObject);
		return OpStatus::ERR_NO_MEMORY;
	}	
	return OpStatus::OK;
}

OP_STATUS WebResource_Custom::AppendBodyData(const OpStringC8 &bodyData)
{
	WebserverBodyObject_OpString8 *stringBodyObject;
	RETURN_IF_ERROR(WebserverBodyObject_OpString8::Make(stringBodyObject, bodyData));
	if (OpStatus::IsError(m_bodyObjects.Add(stringBodyObject)))
	{
		OP_DELETE(stringBodyObject);
		return OpStatus::ERR_NO_MEMORY;
	}	
	return OpStatus::OK;
}

OP_STATUS WebResource_Custom::GetAllItemsInBody(AddToCollectionCallback *collection, BOOL unescape)
{
	if (m_incommingDataMem.CStr()  
		&& m_requestObject->GetHttpMethod() == WEB_METH_POST
		&& ( m_saveIncommingBodyToFile == FALSE)
		) 
	{
		RETURN_IF_ERROR(FindIAlltemsInString(m_incommingDataMem.CStr(), collection, unescape));
	}
	else
	{	
		char *data = NULL;
		for (unsigned int OP_MEMORY_VAR index = 0; index < m_multiParts.GetCount(); ++index )
		{
			WebserverUploadedFile *uploadedFile = m_multiParts.Get(index)->Ptr();
	
			OpFile file;
	
			RETURN_IF_MEMORY_ERROR(file.Construct(uploadedFile->GetTemporaryFilePath(), OPFILE_ABSOLUTE_FOLDER));
			
		    BOOL exists;
		    file.Exists(exists);
			if ( exists == FALSE)
			{
				/*just in case*/
				continue;
			}
				
		    RETURN_IF_ERROR(file.Open(OPFILE_READ));
			OP_ASSERT(file.IsOpen());
			
			OpFileInfo::Mode mode;
			RETURN_IF_ERROR(file.GetMode(mode));
			
			if (mode == OpFileInfo::DIRECTORY)
			{
				/*just in case*/
				if (file.IsOpen())
					file.Close();
				continue;
			}
			
			OpFileLength fileLength; 
			RETURN_IF_ERROR(file.GetFileLength(fileLength));
	
			Parameters *nameParameter = 0;
			
			if (HeaderEntry *header = uploadedFile->GetHeaderList()->GetHeader("Content-Disposition"))
			{
				ParameterList *parameters = NULL;
				RETURN_IF_LEAVE(parameters = header->GetParametersL(PARAM_SEP_SEMICOLON | PARAM_STRIP_ARG_QUOTES | PARAM_SEP_WHITESPACE | PARAM_HAS_RFC2231_VALUES, KeywordIndex_HTTP_General_Parameters));
	
				if (parameters)
				{
					if (parameters->GetParameter("form-data", PARAMETER_ANY) != NULL)
						nameParameter = parameters->GetParameter("name", PARAMETER_ANY);
				}
			}
			if (nameParameter)
			{
				OpString ItemName;
				RETURN_IF_ERROR(ItemName.SetFromUTF8(nameParameter->Value()));
		
				OpString ItemValue;
				OpFileLength bytes_read;				
				data = OP_NEWA(char, (int)fileLength + 1);
				if (!data)
				{
					if (file.IsOpen())
						file.Close();
					goto err;
				}
		
				if (OpStatus::IsError(file.Read(data, fileLength,&bytes_read)))
				{
					if (file.IsOpen())
						file.Close();
					goto err;
					
				}
				data[bytes_read] = '\0';
				
				if (OpStatus::IsError(ItemValue.SetFromUTF8(data)))
				{
					if (file.IsOpen())
						file.Close();
					goto err;					
				}
				
				OP_DELETEA(data);
				data = NULL;
				
				if (OpStatus::IsError(collection->AddToCollection(ItemName.CStr(), ItemValue.CStr())))
				{
					if (file.IsOpen())
						file.Close();
					goto err;	
				}
	
			}
		
			if (file.IsOpen())
				file.Close();		
		}
		
		return OpStatus::OK;
err:
		OP_DELETEA(data);

		return OpStatus::ERR_NO_MEMORY;	
	}
	return OpStatus::OK;

	
}

OP_STATUS WebResource_Custom::GetItemInBody(const OpStringC16 &itemName, OpAutoVector<OpString> *itemList, BOOL unescape)
{
	OpString *itemValue = NULL;
	char *data = NULL;
	
	if (m_incommingDataMem.CStr()  
		&& m_requestObject->GetHttpMethod() == WEB_METH_POST
		&& (m_incomingDataType == FORMURLENCODED || m_incomingDataType == TEXT) 
		) 
	{
		RETURN_IF_ERROR(FindItemsInString(m_incommingDataMem.CStr(), itemName.CStr(), itemList, unescape));
	}
	else
	{
		OpString8 tempUTF8ItemName;
		tempUTF8ItemName.SetUTF8FromUTF16(itemName.CStr());

		/*FIXME: Go through all files*/	
		for (OP_MEMORY_VAR unsigned int index = 0; index < m_multiParts.GetCount(); ++index )
		{
			WebserverUploadedFile *uploadedFile = m_multiParts.Get(index)->Ptr();

			OpFile file;

			RETURN_IF_MEMORY_ERROR(file.Construct(uploadedFile->GetTemporaryFilePath(), OPFILE_ABSOLUTE_FOLDER));
			
		    BOOL exists;
		    file.Exists(exists);
			if ( exists == FALSE)
			{
				/*just in case*/
				continue;
			}
				
		    RETURN_IF_ERROR(file.Open(OPFILE_READ));
			OP_ASSERT(file.IsOpen());
			
			OpFileInfo::Mode mode;
			RETURN_IF_ERROR(file.GetMode(mode));
			
			if (mode == OpFileInfo::DIRECTORY)
			{
				/*just in case*/
				if (file.IsOpen())
					file.Close();
				continue;
			}
			
			OpFileLength fileLength; 
			RETURN_IF_ERROR(file.GetFileLength(fileLength));
	
			Parameters *nameParameter = NULL;
			HeaderEntry *header = NULL;
			if (uploadedFile->GetHeaderList() && NULL!=(header = uploadedFile->GetHeaderList()->GetHeader("Content-Disposition")))
			{
				ParameterList *parameters = NULL;
				TRAPD(status, parameters = header->GetParametersL(PARAM_SEP_SEMICOLON | PARAM_STRIP_ARG_QUOTES | PARAM_SEP_WHITESPACE | PARAM_HAS_RFC2231_VALUES, KeywordIndex_HTTP_General_Parameters));
				RETURN_IF_ERROR(status);

				if (parameters)
				{
					if (parameters->GetParameter("form-data", PARAMETER_ANY) != NULL)
						nameParameter = parameters->GetParameter("name", PARAMETER_ANY);
				}
			}
			
			if (nameParameter && op_strcmp(nameParameter->Value(), tempUTF8ItemName.CStr()) == 0)
			{
				itemValue = OP_NEW(OpString, ());
				OpFileLength bytes_read;				
				data = OP_NEWA(char, (int)fileLength + 1);

				if (!data || !itemValue)
				{
					if (file.IsOpen())
						file.Close();
					goto err;
				}

				if (OpStatus::IsError(file.Read(data, fileLength,&bytes_read)))
				{
					if (file.IsOpen())
						file.Close();
					goto err;
					
				}
				data[bytes_read] = '\0';
				
				if (OpStatus::IsError(itemValue->SetFromUTF8(data)))
				{
					if (file.IsOpen())
						file.Close();
					goto err;					
				}
				OP_DELETEA(data);
				data = NULL;
				
				if (OpStatus::IsError( itemList->Add(itemValue)))
				{
					itemList->RemoveByItem(itemValue);
					if (file.IsOpen())
						file.Close();
					goto err;	
				}

			}
			if (file.IsOpen())
				file.Close();
			
		}			
	}
	
	return OpStatus::OK;
err:
	OP_DELETEA(data);
	OP_DELETE(itemValue);					
	return OpStatus::ERR_NO_MEMORY;
}


OP_STATUS WebResource_Custom::GetAllItemsInUri(AddToCollectionCallback *collection, BOOL unescape, BOOL translatePlus)
{
	const uni_char *query = uni_strchr(m_requestObject->GetOriginalRequestUni(), '?');
	if (query == NULL || *query == '\0')
	{
		return OpStatus::OK;
	}
	
	query++;
	
	return FindIAlltemsInString(query, collection, unescape, translatePlus);
}

OP_STATUS WebResource_Custom::GetItemInUri(const OpStringC16 &itemName, OpAutoVector<OpString> *itemList, BOOL unescape)
{
	const uni_char *query = uni_strchr(m_requestObject->GetOriginalRequestUni(), '?');
	if (query == NULL || *query == '\0')
	{
		return OpStatus::OK;
	}
	
	query++;

	return FindItemsInString(query, itemName.CStr(), itemList, unescape);
}	
	
const uni_char* WebResource_Custom::GetHttpMethodString()
{
	WebServerMethod method = GetHttpMethod();
	switch (method) {
		case WEB_METH_GET:
			return  UNI_L("GET");
		case WEB_METH_HEAD:				
			return UNI_L("HEAD");				
		case WEB_METH_POST:					
			return UNI_L("POST");						
		case WEB_METH_PUT:					
			return  UNI_L("PUT");			
		case WEB_METH_DELETE:				
			return  UNI_L("DELETE");		
		case WEB_METH_TRACE:					
			return  UNI_L("TRACE");		
		case WEB_METH_CONNECT:				
			return  UNI_L("CONNECT");		
		case WEB_METH_OPTIONS:				
			return  UNI_L("OPTIONS");		
		default:
		return  UNI_L("");						
	}
}

OP_STATUS WebResource_Custom::Flush(FlushIsFinishedCallback *flushIsFinishedCallback, unsigned int flushId)
{
	/* IMPORTANT: flushIsFinishedCallback MUST be given at some point for every flush */
	
	if (m_requestClosed == TRUE)
	{
		return OpStatus::ERR;
	}
	
	if (m_service == NULL)
	{
		/*if m_service is null, the connection has been closed, and we can not flush*/
		return OpStatus::ERR;
	}

	if (m_bodyObjects.GetCount() == 0 && m_closeRequest == FALSE)
	{
		RETURN_IF_ERROR(flushIsFinishedCallback->FlushIsFinished(flushId));
		return OpStatus::OK;
	}
	
	int protocol_major = m_service->getProtocolVersion().protocol_major;
	int protocol_minor = m_service->getProtocolVersion().protocol_minor;
	
	if (m_headers_sent == FALSE)
	{			
		//if(m_service->ClientIsOwner() || m_service->ClientIsLocal())
		//	RETURN_IF_LEAVE(AddResponseHeader(UNI_L("Cache-Control"),UNI_L("no-cache")));
		
		if (m_bodyObjects.GetCount() == 0)
		{
			m_chunkedEncoding = FALSE;
			RETURN_IF_ERROR(AddResponseHeader(UNI_L("Content-Length"), UNI_L("0")));
		}
		else
		{
			if (m_chunkedEncoding == TRUE && (protocol_major > 1 || (protocol_major == 1 && protocol_minor >= 1) )) 
			{
				if (OpStatus::IsError(AddResponseHeader(UNI_L("Transfer-Encoding"), UNI_L("chunked" ))))
				{
					OpStatus::Ignore(flushIsFinishedCallback->FlushIsFinished(flushId));			
					return OpStatus::ERR_NO_MEMORY;
				}
			}
			else
			{
				m_chunkedEncoding = FALSE;
				m_allowPersistentConnection = FALSE;
			}
		}
	}


	
	if (m_headers_sent == FALSE)
	{
		RETURN_IF_ERROR(m_service->StartServingData());
	}
	else
	{
		RETURN_IF_ERROR(m_service->ContinueServingData());
	}
	
	m_hasStartedFlushing = TRUE;
	
	if 
	(
		m_bodyObjects.GetCount() == 0 || 
		m_bodyObjects.Get(m_bodyObjects.GetCount() - 1)->hasFlushFinishedCallback() == TRUE
	)
	{
		/* 	No object ready for flushing OR The last body object allready had a callback 
			Need a dummy null object, so that we can send a correct flush finished callback 
			The only purpose for this object is to send a correct flush finished callback to flush caller */
			
		OP_STATUS status = OpStatus::ERR_NO_MEMORY;
		WebserverBodyObject_Null *nullBodyObject = OP_NEW(WebserverBodyObject_Null, ());
		
		if (nullBodyObject == NULL || OpStatus::IsError(status = m_bodyObjects.Add(nullBodyObject)))
		{
			OP_DELETE(nullBodyObject);
			nullBodyObject = NULL;
			return status;
		}
	}
	
	m_bodyObjects.Get(m_bodyObjects.GetCount() - 1)->SetFlushFinishedCallback(flushIsFinishedCallback, flushId);
	return OpStatus::OK;
}

OP_BOOLEAN WebResource_Custom::CloseAndForward()
{
	if (m_bodyObjects.GetCount() > 0 || m_closeRequest == TRUE || m_hasStartedFlushing == TRUE)
	{
		return OpStatus::ERR;
	}
	else
	{	
		
		OP_BOOLEAN status;
		if ((status = ReIssueRequest()) == OpBoolean::IS_TRUE)
		{
			m_closeRequest = TRUE;
		}
		return status;
	}
}

OP_STATUS WebResource_Custom::Close(FlushIsFinishedCallback *flushIsFinishedCallback, unsigned int flushId)
{
	/* IMPORTANT: flushIsFinishedCallback MUST be given at some point for every flush 
	 * 
	 * If flush is not started flushIsFinishedCallback->FlushIsFinished must be called here */
	if (m_service == NULL)
	{
		return OpStatus::ERR;
	}

	if (m_requestClosed == TRUE || (IsFlushing() == FALSE && m_closeRequest == TRUE))
	{
		return flushIsFinishedCallback->FlushIsFinished(flushId);
	}
	
	m_closeRequest = TRUE;

	if (IsFlushing() == FALSE)
	{
		return Flush(flushIsFinishedCallback, flushId);
	}
	else
	{
		/* The flushing is in progress. We add a dummy null object that ensures that the flushIsFinishedCallback->FlushIsFinished
		 * callback is called */ 
	
		
		OP_STATUS status = OpStatus::ERR_NO_MEMORY;
		WebserverBodyObject_Null *nullBodyObject = OP_NEW(WebserverBodyObject_Null, ());
		
		if (nullBodyObject == NULL || OpStatus::IsError(status = m_bodyObjects.Add(nullBodyObject)))
		{
			OP_DELETE(nullBodyObject);
			return status;
		}
	
		nullBodyObject->SetFlushFinishedCallback(flushIsFinishedCallback, flushId);
	
		return OpStatus::OK;
	}
}

OP_STATUS WebResource_Custom::setChunkedEncoding(BOOL chunked)
{
	if (m_hasStartedFlushing == FALSE && m_requestClosed == FALSE)
	{
		m_chunkedEncoding = chunked;
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

BOOL WebResource_Custom::GetChunkedEncoding()
{
	return m_chunkedEncoding;
}

OP_STATUS WebResource_Custom::GetRequestBody(TempBuffer *postBody)
{
	OP_STATUS status = OpStatus::OK;
	WebServerMethod method = m_requestObject->GetHttpMethod();
	if (method != WEB_METH_POST && method != WEB_METH_PUT)
		return OpStatus::OK;
			
	if (m_incomingDataType !=  MULTIPART && m_incomingDataType != FORMURLENCODED && m_incomingDataType != TEXT)
	{
		return OpStatus::OK;
	}

	if (m_saveIncommingBodyToFile == TRUE)	
	{
		ParameterList *parameters = NULL;
		HeaderEntry *contentTypeHeader = m_requestObject->GetClientHeaderList()->GetHeader("Content-Type");
		if (contentTypeHeader == NULL)
			return OpStatus::OK;
		
		RETURN_IF_LEAVE(parameters = contentTypeHeader->GetParametersL(PARAM_SEP_SEMICOLON | PARAM_SEP_WHITESPACE | PARAM_STRIP_ARG_QUOTES | PARAM_HAS_RFC2231_VALUES, KeywordIndex_HTTP_General_Parameters));
		
		Parameters *charsetParameter = NULL;
		const char* OP_MEMORY_VAR charset = NULL;
		
		if (parameters == NULL || (charsetParameter = parameters->GetParameter("charset", PARAMETER_ANY)) == NULL || (charset = charsetParameter->Value()) == NULL)
		{
			charset = "UTF-8";			
		}

		OpString tempBody;
		
		void *data = NULL;
		
		/*read from file*/
		if (!m_incomingDataFile)
		{
			return OpStatus::ERR;
		}
		
		RETURN_IF_ERROR(m_incomingDataFile->Open(OPFILE_READ));
	
		/*FIXME: We must have a maximum file length here*/
		OpFileLength fileLength;
		RETURN_IF_ERROR(m_incomingDataFile->GetFileLength(fileLength));
		
		data = op_malloc((unsigned int)fileLength+1);
		if (data == NULL) 
			return OpStatus::ERR_NO_MEMORY;
	

		OpFileLength lengthRead;
		if (OpStatus::IsError(status = m_incomingDataFile->Read(data, fileLength, &lengthRead)))
		{
			goto error;
		}
			

		if (OpStatus::IsError(status = m_incomingDataFile->GetFileLength(fileLength)))
		{
			goto error;
		}
		
		if (lengthRead != fileLength)
		{
			status = OpStatus::ERR;
			goto error;
		}
			
		*((char*)data + (int)fileLength)='\0';

		
		TRAP(status, SetFromEncodingL(&tempBody, charset, data, (int)fileLength));
		if (OpStatus::IsError(status))
		{
			goto error;
		}
		
		if (OpStatus::IsError(status = postBody->Append(tempBody.CStr())))
		{
			goto error;
		}
		m_incomingDataFile->Close();

		op_free(data);

		return OpStatus::OK;
		
error:
		if (m_incomingDataFile && m_incomingDataFile->IsOpen())
			m_incomingDataFile->Close();
		
		if (data)
			op_free(data);	
		
		return status;
	}
	else
	{
		/*read from ram*/
		if (m_incommingDataMem.CStr())
		{
			RETURN_IF_ERROR(postBody->Append(m_incommingDataMem.CStr()));
		}
		return OpStatus::OK;			
	}	
}


void
WebResource_Custom::SignalScriptFailed()
{
	if (m_service)
	{
		/* FIXME : Implement this */
	}
}


OP_STATUS WebResource_Custom::GetRequestHeader(const OpStringC16 &headerName, OpAutoVector<OpString> *headerList)
{
	OpString8 headerNameUtf8;
	RETURN_IF_ERROR(headerNameUtf8.SetUTF8FromUTF16(headerName.CStr()));

	HeaderEntry *lastHeader = NULL;

	if (HeaderEntry *header = m_requestObject->GetClientHeaderList()->GetHeader(headerNameUtf8.CStr(), lastHeader))
	{
		OpString *headerValue = OP_NEW(OpString, ());
		OP_STATUS status = OpStatus::OK;
		if (headerValue == NULL 
			|| OpStatus::IsError(status = headerValue->Set(header->Value()))
			|| OpStatus::IsError(status = headerList->Add(headerValue)))
		{
			OP_DELETE(headerValue);
			return status;			
		}
		lastHeader = header;
	}
	return  OpStatus::OK;
}

OP_STATUS WebResource_Custom::GetHostHeader(OpString &host)
{
	HeaderEntry *header = m_requestObject->GetClientHeaderList()->GetHeader("Host");
	
	if(!header)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
		
	return  host.Set(header->Value());
}

const uni_char *WebResource_Custom::GetRequestURI()
{	
	return m_requestObject->GetRequest(); 
}

OP_STATUS WebResource_Custom::GetEscapedRequestURI(OpString &request)
{
	return m_requestObject->GetEscapedRequest(request);
}

WebServerMethod WebResource_Custom::GetHttpMethod()
{
	return m_requestObject->GetHttpMethod();
}

BOOL WebResource_Custom::IsFlushing()
{
	return m_currentFlushObject != NULL ? TRUE : FALSE;
}

BOOL WebResource_Custom::IsProxied()
{ 
	return m_service && m_service->IsProxied();
}

#if defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
BOOL WebResource_Custom::ClientIsOwner()
{ 
	return m_service && m_service->ClientIsOwner();
}

BOOL WebResource_Custom::ClientIsLocal()
{ 
	return m_service && m_service->ClientIsLocal();
}

#endif

OP_BOOLEAN
WebResource_Custom::FillBuffer(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize)
{		
	/* FIXME : move chunked encoding logic into WebServerConnection, since it is an http protocol issue */
	
	OP_ASSERT(FilledDataSize == 0);
	if ( (bufferSize - 5 ) > 0)
	{
		/*We must check that all the data was sent before we get a new bodyObject*/
		char chunkLengthStr[21]; /* ARRAY OK 2007-08-30 haavardm */
		if (m_currentFlushObject == NULL && m_bodyObjects.GetCount() > 0)
		{
			m_currentFlushObject = m_bodyObjects.Remove(0);
		}

		if (m_currentFlushObject != NULL)
		{
			RETURN_IF_ERROR(m_currentFlushObject->InitiateGettingData());
			int dataRead;
			int chunkLength = 0;

			int start = FilledDataSize;
			int size = bufferSize - FilledDataSize; 
			
			if (m_chunkedEncoding == TRUE)
			{
				size -= 17;
				/* 17 = 10 for chunk length string before chunk, 
						+2 for "\r\n" after chunk, and 
						+5 for the 0 length chunk "0\r\n\r\n", in case of end of data*/
			}
			
			if (size > 0)
			{

				while (m_currentFlushObject != NULL && chunkLength < size)
				{
					OP_BOOLEAN moreData = m_currentFlushObject->GetData(size - chunkLength, buffer + chunkLength + (m_chunkedEncoding ? 10 : 0), dataRead);
					RETURN_IF_ERROR(moreData);			
					

					chunkLength += dataRead;
		
					if (moreData == OpBoolean::IS_FALSE)
					{
						OP_ASSERT(m_currentFlushObject != NULL);
						
						OP_STATUS s = m_currentFlushObject->SendFlushCallbacks();
						OP_DELETE(m_currentFlushObject);
						m_currentFlushObject = NULL;
						RETURN_IF_ERROR(s);

						if (m_bodyObjects.GetCount() > 0)
						{
							m_currentFlushObject = m_bodyObjects.Get(0);
							OpStatus::Ignore(m_bodyObjects.RemoveByItem(m_currentFlushObject));
							
							RETURN_IF_ERROR(m_currentFlushObject->InitiateGettingData());
						}
					}				
				}
				
				FilledDataSize += chunkLength;
			}
			
			if (m_chunkedEncoding == TRUE && chunkLength > 0)
			{
				FilledDataSize += 10; /* FIXME for 64 bit computers the length of the chunkLengthStr might int theory be bigger than 10 */
				op_sprintf(chunkLengthStr, "%08x" CRLF, chunkLength);		
				op_memcpy(buffer+start, chunkLengthStr, 10);
	
				op_sprintf(chunkLengthStr, CRLF);
				
				op_memcpy(buffer+FilledDataSize, chunkLengthStr, 2);
	
				FilledDataSize += 2;
			}	
		

		}
		
		if (m_requestClosed == FALSE &&  m_currentFlushObject == NULL && m_closeRequest == TRUE) /* no more data */
		{
			if (m_chunkedEncoding == TRUE)
			{
				op_sprintf(chunkLengthStr, "0" CRLF CRLF);		
				op_memcpy(buffer + FilledDataSize, chunkLengthStr, 5);
				FilledDataSize += 5;
			}
			m_requestClosed = TRUE;
			return OpBoolean::IS_FALSE;
		}			
		else if (FilledDataSize == 0 && m_currentFlushObject == NULL)
		{
			m_service->PauseServingData();
			return OpBoolean::IS_TRUE;		
		}
	}
	return OpBoolean::IS_TRUE;
}

#endif // WEBSERVER_SUPPORT
