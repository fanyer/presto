/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2011
 *
 * Web server implementation 
 */

#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT
#include "modules/viewers/viewers.h"
#include "modules/webserver/src/resources/webserver_file.h"
#include "modules/webserver/webserver_request.h"
#include "modules/pi/system/OpLowLevelFile.h"
#include "modules/webserver/webserver-api.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/util/tempbuf.h"
#include "modules/util/opfile/opfile.h"
#include "modules/webserver/src/webservercore/webserver_connection.h"
#include "modules/util/gen_str.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/pi/OpLocale.h"
#include "modules/util/datefun.h"
#include "modules/formats/uri_escape.h"
#include "modules/formats/hdsplit.h"
#include "modules/formats/argsplit.h"
#include "modules/url/protocols/http1.h"

WebserverResourceDescriptor_Static::WebserverResourceDescriptor_Static(BOOL isFile)
	: m_isFile(isFile)
{
}

/*static*/
WebserverResourceDescriptor_Static* WebserverResourceDescriptor_Static::Make(const OpStringC &realPath, const OpStringC &virtualPath)
{
	OP_ASSERT(realPath.CStr() != NULL);
	OP_ASSERT(!realPath.IsEmpty());
	if (realPath.IsEmpty())
	{
		return NULL;
	}
		
	BOOL is_file = FALSE;
	
	if (realPath[realPath.Length() - 1] != '/')
	{
		OpFile file;
		BOOL found;
		
		if (OpStatus::IsMemoryError(file.Construct(realPath.CStr(), OPFILE_ABSOLUTE_FOLDER)) || OpStatus::IsMemoryError(file.Exists(found)))
		{
			return NULL;
		}
		else if (found == TRUE)
		{		
			OpFileInfo::Mode mode;
			if (OpStatus::IsMemoryError(file.GetMode(mode)))
			{
				return NULL;
			}
			else
			{
				is_file = (mode == OpFileInfo::FILE ? TRUE : FALSE);
			}
		}
	}

	WebserverResourceDescriptor_Static *staticResource = OP_NEW(WebserverResourceDescriptor_Static, (is_file));

	if (staticResource &&
		OpStatus::IsError(staticResource->m_resourceRealPath.Construct(realPath)) ||
		OpStatus::IsError(staticResource->m_resourceVirtualPath.ConstructFromURL(virtualPath))
		)
	{
		OP_DELETE(staticResource);
		staticResource = NULL;
	}
	
	return staticResource;
}

const uni_char* WebserverResourceDescriptor_Static::GetResourceRealPath() const
{
	return m_resourceRealPath.GetPathAndFileNamePointer();
}


WebServerResourceType WebserverResourceDescriptor_Static::GetResourceType() const
{
	return WEB_RESOURCE_TYPE_STATIC;
}



WebResource_File::WebResource_File(WebServerConnection *service, OpFile *file, WebserverRequest *requestObject)
	: WebResource(service, requestObject)
	, m_file(file)
{
	m_bytes_served=0;
}

/* virtual */
WebResource_File::~WebResource_File()
{

	if (m_file != NULL)
	{
		m_file->Close();
	}
	OP_DELETE(m_file);
}

/* FIXME : use CharsetDetector to detect the charset in the file */

/* static */ OP_STATUS
WebResource_File::Make(WebResource *&webresource, WebServerConnection *service, const OpStringC16 &subServerRequestString, WebserverRequest *requestObject, WebserverResourceDescriptor_Static *resource, WebServerFailure *result)
{
	webresource = NULL;
	WebServerMethod method = requestObject->GetHttpMethod();
	const uni_char *request_resource_name = subServerRequestString.CStr();
	
	OpString escaped;

	escaped.Set(request_resource_name);

	UriUnescape::ReplaceChars(escaped.CStr(), UriUnescape::LocalfileUrlUtf8 | UriUnescape::All);

	request_resource_name = escaped.CStr();


	while (*request_resource_name == '/')
		request_resource_name++;
	
	const uni_char *resource_descriptor_name = resource->GetResourceVirtualPath();


	while (*resource_descriptor_name == '/')
		resource_descriptor_name++;
	
	
	unsigned int descriptor_name_length = uni_strlen(resource_descriptor_name);


	if (descriptor_name_length != 0)
	{
		/* if resource_descriptor_name is not "" check that  request_resource_name == resource_descriptor_name */
		//const uni_char *end_of_request_resource_name = uni_strpbrk(request_resource_name, UNI_L("/?#"));
		// Check for a perfect match
		const uni_char *end_of_request_resource_name = uni_strpbrk(subServerRequestString.CStr(), UNI_L("/?#"));
		unsigned int request_resource_name_length = end_of_request_resource_name ? end_of_request_resource_name - subServerRequestString.CStr() : uni_strlen(request_resource_name);
	
		if (request_resource_name_length != descriptor_name_length || uni_strncmp(request_resource_name, resource_descriptor_name, descriptor_name_length) != 0)
		{
			// Check for a partial perfect match
			end_of_request_resource_name = uni_strpbrk(subServerRequestString.CStr(), UNI_L("?#"));
			request_resource_name_length = end_of_request_resource_name ? end_of_request_resource_name - subServerRequestString.CStr() : uni_strlen(request_resource_name);	
			
		 	if (request_resource_name_length != descriptor_name_length || uni_strncmp(request_resource_name, resource_descriptor_name, descriptor_name_length) != 0)
		 	{
		 		// Not perfect match with a directory: check for approx match
		 		if(	request_resource_name_length < descriptor_name_length+1 ||
					request_resource_name[descriptor_name_length] != '/' ||
					uni_strncmp(request_resource_name, resource_descriptor_name, descriptor_name_length) != 0)
				{
					*result = WEB_FAIL_FILE_NOT_AVAILABLE;
					
		 			return OpStatus::OK;
				}
		 	}
		}
	}

	const uni_char *resource_subpath = request_resource_name + uni_strlen(resource_descriptor_name);

	const uni_char *end_of_filename = uni_strpbrk(resource_subpath, UNI_L("?#"));	
	unsigned int resource_subpath_length = end_of_filename ? end_of_filename - resource_subpath : uni_strlen(resource_subpath);	
	
	
	if (resource->IsFile() && resource_subpath_length > 0)
	{
	 	*result = WEB_FAIL_FILE_NOT_AVAILABLE;
	 	return OpStatus::OK;
	}
	
	if (method != WEB_METH_GET && method != WEB_METH_POST && method != WEB_METH_HEAD)
	{
	 	*result = WEB_FAIL_METHOD_NOT_SUPPORTED;
		return OpStatus::OK;
	}
	
	OpString resourceName;
	RETURN_IF_ERROR(resourceName.Set(resource_subpath, resource_subpath_length));

	OpString path;

	RETURN_IF_ERROR(WebserverFileName::SecurePathConcat(path, resource->GetResourceRealPath(), TRUE));
	if (resource->IsFile() == FALSE)
	{
		RETURN_IF_ERROR(WebserverFileName::SecurePathConcat(path, resourceName, TRUE));
	}

	if (path.Find("../") != KNotFound || path.Find("/..") != KNotFound) /* In case any of the security checks have failed. */
	{
		OP_ASSERT(!"Path resolving has failed!. Contact the Core owner of the webserver module.");
		*result = WEB_FAIL_FILE_NOT_AVAILABLE;
	 	return OpStatus::OK;
	}
	
	
	unsigned int length = path.Length();

	uni_char *pathstr = path.CStr();
	if (length > 0 && pathstr[length - 1] == '/')
	{
		RETURN_IF_ERROR(path.Append("index.html"));
	}

#ifdef SYS_CAP_NETWORK_FILESYSTEM_BACKSLASH_PATH
	pathstr = path.CStr();
	for (unsigned int i = 0; i < length ; i++)
	{
		if (pathstr[i] == '/')
			pathstr[i] = '\\';
	}
#endif //SYS_CAP_NETWORK_FILESYSTEM_BACKSLASH_PATH

	OpFile *file;
	if ((file = OP_NEW(OpFile, ())) == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OpAutoPtr<OpFile> file_anchor(file);

	BOOL found = FALSE;
	
	OP_STATUS status;
	RETURN_IF_MEMORY_ERROR(status = file->Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER));
	RETURN_IF_MEMORY_ERROR(file->Exists(found));
	if (found == FALSE)
	{
		*result = WEB_FAIL_FILE_NOT_AVAILABLE;
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(file->Open(OPFILE_READ));

	OpFileInfo::Mode mode;
	
	RETURN_IF_ERROR(file->GetMode(mode));

	if (mode == OpFileInfo::DIRECTORY)
	{
		*result = WEB_FAIL_REDIRECT_TO_DIRECTORY;
		RETURN_IF_ERROR(file->Close());
		return OpStatus::OK;
	}

	*result = WEB_FAIL_GENERIC;

	WebResource_File *fileResource = NULL;

	fileResource = OP_NEW(WebResource_File, (service, file_anchor.release(), requestObject));
	if (fileResource == NULL)
	{
		OP_DELETE(file);
		return OpStatus::ERR_NO_MEMORY;
	}

	OpAutoPtr<WebResource_File> fileResource_anchor(fileResource);

	 /*We find the mime type*/
	OpString charExtension;
	OpString8 content_type;
	RETURN_IF_ERROR(charExtension.Set(StrFileExt(path.CStr())));

	if (charExtension.IsEmpty() == FALSE)
	{
		// Automatically set the Content-Type only if it is not been already set
		//if(!service->IsResponseHeaderPresent("Content-Type"))
		RETURN_IF_ERROR(service->GetResponseHeader("Content-Type", content_type));

		if(!content_type.Compare(DEFAULT_CONTENT_TYPE) || content_type.Length()==0)
		{
			Viewer* viewer = NULL;
			if (OpStatus::IsError(g_viewers->FindViewerByFilename(path.CStr(), viewer)))
			{
				viewer = NULL;
			}
			else if (viewer)
			{
				RETURN_IF_ERROR(fileResource->AddResponseHeader(UNI_L("Content-Type"), viewer->GetContentTypeString()));
			}
		}
	}
	/*End of finding mime type*/

	OpFileLength filelength;
	RETURN_IF_ERROR(file->GetFileLength(filelength));

	/* Range header */
	HeaderList *headerlist = requestObject->GetClientHeaderList();
	HeaderEntry *rangeHeader = NULL;

	if (headerlist && (rangeHeader = headerlist->GetHeader("Range")) != NULL )
	{
		ParameterList *RangeParameters = NULL;
		RETURN_IF_LEAVE(RangeParameters = rangeHeader->GetParametersL((PARAM_SEP_COMMA | PARAM_STRIP_ARG_QUOTES), KeywordIndex_HTTP_General_Parameters));

		Parameters *rangeBytesParameter = NULL;
		if (RangeParameters && RangeParameters->ParameterExists(HTTP_General_Tag_Bytes, PARAMETER_ASSIGNED_ONLY))
		{
			rangeBytesParameter = RangeParameters->GetParameterByID(HTTP_General_Tag_Bytes, PARAMETER_ANY);
			if (rangeBytesParameter)
			{
				HTTPRange range;

				RETURN_IF_ERROR(range.Parse(rangeBytesParameter->Value(), TRUE, filelength));

				if (range.GetStart() != FILE_LENGTH_NONE && range.GetEnd()!=FILE_LENGTH_NONE && range.GetStart() < filelength && range.GetEnd() >= range.GetStart())
				{
					RETURN_IF_MEMORY_ERROR(status = fileResource->m_file->SetFilePos(range.GetStart()));

					if (OpStatus::IsError(status) == FALSE)
					{
						OpString byteString;
						OpString8 temp_val;

						RETURN_IF_ERROR(byteString.Set(UNI_L("bytes ")));
						RETURN_IF_ERROR(g_op_system_info->OpFileLengthToString(range.GetStart(), &temp_val));
						RETURN_IF_ERROR(byteString.Append(temp_val));
						RETURN_IF_ERROR(byteString.Append(UNI_L("-")));
						RETURN_IF_ERROR(g_op_system_info->OpFileLengthToString(range.GetEnd(), &temp_val));
						RETURN_IF_ERROR(byteString.Append(temp_val));
						RETURN_IF_ERROR(byteString.Append(UNI_L("/")));
						RETURN_IF_ERROR(g_op_system_info->OpFileLengthToString(filelength, &temp_val));
						RETURN_IF_ERROR(byteString.Append(temp_val));
						RETURN_IF_ERROR(fileResource->AddResponseHeader(UNI_L("Content-Range"), byteString.CStr()));
						RETURN_IF_ERROR(fileResource->SetResponseCode(206));
						fileResource->m_totalResourceLength = range.GetEnd() - range.GetStart()+1;
						filelength = fileResource->m_totalResourceLength;
					}
					else
					{
						fileResource->m_file->SetFilePos(0);
					}
				}
			}
		}
	}
	else
	{
		fileResource->m_totalResourceLength = filelength;
	}

	RETURN_IF_ERROR(fileResource->AddResponseHeader(UNI_L("Accept-Ranges"), UNI_L("bytes")));
	/* end of range header */


	/* file length header */
	OpString val;
	OpString8 temp_val;
	RETURN_IF_ERROR(g_op_system_info->OpFileLengthToString(filelength, &temp_val));
	RETURN_IF_ERROR(val.Set(temp_val));

	RETURN_IF_ERROR(fileResource->AddResponseHeader(UNI_L("Content-Length"), val.CStr()));
	/* end of file length header */


	RETURN_IF_ERROR(service->StartServingData());

	webresource = fileResource_anchor.release();
	*result = WEB_FAIL_NO_FAILURE;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
WebResource_File::GetLastModified(time_t &date)
{
	/*We find the last time files was modified*/
	if (!m_file)
	{
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(m_file->GetLastModified(date));

	return OpStatus::OK;
}

/* virtual */ OP_STATUS 
WebResource_File::GetETag(OpString8 &etag)
{
	OpString temp;
	time_t last_modified;

	if (!m_file)
	{
		return OpStatus::ERR;
	}

	OpString8 temp_val;
	OpFileLength len; 
	m_file->GetFileLength(len);
	RETURN_IF_ERROR(g_op_system_info->OpFileLengthToString(len, &temp_val));
	temp.Set("\"");
	temp.Append(temp_val);

	uni_char dateString[70]; /* ARRAY OK 2009-11-18 jonnyr */
	GetLastModified(last_modified);
		
	if (last_modified)
	{
		struct tm* file_time = op_gmtime(&last_modified);
		FMakeDate(*file_time,"\247D\247M\247Y\247h\247m\247s\"", dateString, 68);
		temp.Append(dateString);
	}

	etag.Set(temp.CStr());
	return OpStatus::OK;
}


/* virtual */ OP_STATUS
WebResource_File::HandleIncommingData(const char *incommingData, UINT32 length, BOOL lastData)
{
	return OpStatus::OK;
}

/* virtual */ OP_BOOLEAN
WebResource_File::FillBuffer(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize)
{
	OP_ASSERT( m_file != NULL );

	OpFileLength nbytes;
	OpFileLength bytes_to_read=bufferSize - FilledDataSize;
	
	if(bytes_to_read>m_totalResourceLength-m_bytes_served)
		bytes_to_read=m_totalResourceLength-m_bytes_served;
		
	RETURN_IF_ERROR(m_file->Read(buffer + FilledDataSize, bytes_to_read, &nbytes));

	FilledDataSize += (unsigned long)nbytes;
	m_bytes_served+=nbytes;

	return ( m_bytes_served<m_totalResourceLength && m_file->Eof() == FALSE) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

#endif // WEBSERVER_SUPPORT
