/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006-2012
 *
 * Web server implementation -- overall server logic
 */
#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT
#include "modules/webserver/webserver_filename.h"
#include "modules/formats/uri_escape.h"


/* FIXME: Use OpString as internal representation instead of an char array */

WebserverFileName::WebserverFileName()
	: m_isURL(FALSE)
{
}

WebserverFileName::~WebserverFileName()
{
}

const uni_char *WebserverFileName::GetPathAndFileNamePointer() const
{
	return m_filePathAndName.CStr();
}


/* static */ OP_STATUS
WebserverFileName::SecureURLPathConcat(OpString &safeBasePath, const OpStringC8 &unsafeUrl)
{
	OpString UTF16Url;
	RETURN_IF_ERROR(UTF16Url.SetFromUTF8(unsafeUrl.CStr()));
	return WebserverFileName::SecurePathConcat(safeBasePath, UTF16Url, TRUE);
}

/* static */ OP_STATUS
WebserverFileName::SecureURLPathConcat(OpString &safeBasePath, const OpStringC &unsafeUrl)
{
	return WebserverFileName::SecurePathConcat(safeBasePath, unsafeUrl, TRUE);
}

/* static */ OP_STATUS
WebserverFileName::SecurePathConcat(OpString &safeBasePath, const OpStringC &unsafeSubPath, BOOL isURL)
{
	if (safeBasePath.FindI(UNI_L("..")) != KNotFound)
	{
		OP_ASSERT(!"this path is not safe");
		return OpStatus::ERR;
	}
	
	safeBasePath.Strip(TRUE, TRUE);
	
	OpString lsubPath;	
	RETURN_IF_ERROR(lsubPath.Set(unsafeSubPath));
	lsubPath.Strip(TRUE, TRUE);
	
	RETURN_IF_ERROR(CheckAndFixDangerousPaths(safeBasePath.CStr(), FALSE));
	
	RETURN_IF_ERROR(CheckAndFixDangerousPaths(lsubPath.CStr(), isURL));

	int lengthSub =  lsubPath.Length();

	if ( (safeBasePath.Length() > 0 && safeBasePath[safeBasePath.Length() - 1] != '/')  
			&& ((lengthSub > 0 && lsubPath[0] != '/') ))
	{
		RETURN_IF_ERROR(safeBasePath.Append("/"));
	}
	
	RETURN_IF_ERROR(safeBasePath.Append(lsubPath));
	
	return OpStatus::OK;
}

const uni_char *WebserverFileName::GetFileNamePointer() const
{	
	if (m_filePathAndName.IsEmpty() == TRUE)
		return NULL;
	
	const uni_char *filePathAndName = m_filePathAndName.CStr();
	
	int i = 0;
	uni_char *j = NULL;
	do
	{
		j = uni_strstr(&filePathAndName[i], UNI_L("/"));
		if (j)
		{
			i = j - filePathAndName + 1;
		}
	}
	while (j != NULL && filePathAndName[i] != '\0');
	
	return filePathAndName+i;
}

/* static */ OP_STATUS
WebserverFileName::CheckAndFixDangerousPaths(uni_char *filePathAndName, BOOL isURL)
{
	////////////////////////////////////////
	//// Known weakness: path less than 3 characters are problematic
	//// The character '/' can be duplciated
	//// It seems that '/' is required at the beginning of the path; the code as to be checked
	////	and, if it is safe, an ASSERT and a return have to be put in place if it does not happen
	////////////////////////////////////////
	int length;
	
	if (filePathAndName == NULL || (length = uni_strlen(filePathAndName)) == 0)
	{
		return OpStatus::OK;
	}
	
	int i;
	for (i = 0; i < length; i++)
	{
		/* We normalize the path here (all \'s are turned in to /'s). */
		if 	(filePathAndName[i] == '\\')
		{
				filePathAndName[i] = '/';
		}		
		
		/* Remove '"' */
		if 	(filePathAndName[i] == '\"')
		{
				filePathAndName[i] = ' ';
		}		
		
	}	
	
	if (isURL == TRUE)
	{
		//StrReplaceChars(filePathAndName, '+', ' ');


#ifdef FORMATS_URI_ESCAPE_SUPPORT
		UriUnescape::ReplaceChars(filePathAndName, length, UriUnescape::LocalfileUrlUtf8);
#else
		ReplaceEscapedCharsUTF8(filePathAndName, length, OPSTR_LOCALFILE_URL);
#endif
		filePathAndName[length] = '\0';

	}
	
	for (i = 0; i < length; i++)
	{
		if(filePathAndName[i]=='?')
			break; // After the '?', get parameters start, so a '\\' is fine
		if 	(filePathAndName[i] == '\\')
		{
			return OpStatus::ERR;
		}
	}
	
	while ( length > 0 && filePathAndName[length-1] == ' ')
	{
		length--;	
	}
	
	filePathAndName[length] = '\0';

	i = 0;
	while (i < length && ( filePathAndName[i] == ' ' || filePathAndName[i] == '.'))
	{
		i++;
	}
	
	int j;
	for (j = 0; j < length - i; j++)
	{
		filePathAndName[j] = filePathAndName[j+i];
	}
	
	length -= i;

	filePathAndName[length] = '\0';
	int depth = 0; //the depth in the path tree
	
	// Special cases...
	if(length<=3)
	{
		if(!uni_strcmp(filePathAndName, "../") ||
		   !uni_strcmp(filePathAndName, "..") ||
		   !uni_strcmp(filePathAndName, "/.."))
		   return OpStatus::ERR;
		 
	}
	
	//Checks the path of type /adir/adir2/../.././ and counts the depth
	for (i = 3; i < length; i++)	
	{		
		if (filePathAndName[i - 3] == '/')
		{
			if (filePathAndName[i - 2] != '.')
				depth++;    //if the filePathAndName is "/x" and x is not a "." then depth++
			else
			{ 			
				if (filePathAndName[i - 1] == '.' && ( filePathAndName[i] == '/' || filePathAndName[i] == ' ' || filePathAndName[i] == '\0' ))  //if the filePathAndName is "/../ then depth--
					depth--;	
				else
				{	
					if (filePathAndName[i - 1] != '/') 
						depth++;    //if the filePathAndName is "/.x" and x is not a "/" then depth++							
				}	
			}
		}
		if (depth < 0 )
			return OpStatus::ERR;  //The filePathAndName depth should never try to get below 0, because is dangerous
	}	
	
	
	// if the filePathAndName seems ok, remove all "/../" , "/./" and "//"
		
	int k;
	for (i = 0, j = 0 ; i < length; i++, j++)
	{
		filePathAndName[j] = filePathAndName[i];	
		if ( j > 1 && (i < length) && (uni_strncmp(&filePathAndName[i], "/./", 3) == 0) )
		{
			i += 1;  //Just remove the "./"
			j--;		
		}
		else
		if ( j > 1 && (i < length) && (uni_strncmp(&filePathAndName[i], "//", 2) == 0) )
		{
			j --;		
		}  		
		else
		//if ( j > 2 && (i < length) && (uni_strncmp(&filePathAndName[i], "/../", 4) == 0)  )
		if ( j > 2 && (i < length) && (uni_strncmp(&filePathAndName[i], "/..", 3) == 0) && (filePathAndName[i+3]=='/' || filePathAndName[i+3]==0)   )
		{
			i += 2;  // remove the "../" 
			k = j - 1;
			while (filePathAndName[k] != '/' && k > 1) k--; 
			j = k - 1; //and rewind next to the last '/' we passed
		}
	}

	/*if ( j >= 2  && uni_strncmp(&filePathAndName[j - 2], "..", 2) == 0 )
	{
		j -= 2;
	}*/
	
	if (j < 0)
	{
//		OP_ASSERT(!"ERROR IN WebserverFileName::CheckAndFixDangerousPaths");
		j = 0;
	}

	
	filePathAndName[j] = '\0';
	
	//OP_ASSERT(uni_strstr(filePathAndName, UNI_L("./")) != filePathAndName && uni_strstr(filePathAndName, UNI_L("/..")) == NULL);
	
	if (uni_strstr(filePathAndName, UNI_L("./")) == filePathAndName || uni_strstr(filePathAndName, UNI_L("/../")) != NULL)
		return OpStatus::ERR;
	
	return OpStatus::OK;	
}

OP_STATUS WebserverFileName::Construct(const uni_char *path, int length, BOOL isURL)
{
	if (path == NULL)
	{
	 	return OpStatus::ERR;
	}

	const uni_char *localPath = path;
	if (localPath[0] == '\"')
	{
		localPath = &path[1];
		length--;
	}
	
	if (path[length] == '\"')
	{
		length--;	
	}
	
	RETURN_IF_ERROR(m_filePathAndName.Set(path, length));
	
	m_filePathAndName.Strip(TRUE, TRUE);
	
	return CheckAndFixDangerousPaths(m_filePathAndName.CStr(), isURL);
}

OP_STATUS WebserverFileName::Construct(const char *path, int length, BOOL isURL)
{	
	if (path == NULL)
	{
	 	return OpStatus::ERR;
	}
	
	
	const char *localPath = path;
	if (localPath[0] == '\"')
	{
		localPath = &path[1];
		length--;
	}
	
	if (path[length] == '\"')
	{
		length--;	
	}
	
	RETURN_IF_ERROR(m_filePathAndName.SetFromUTF8(localPath, length));
	
	m_filePathAndName.Strip(TRUE, TRUE);
	
	return CheckAndFixDangerousPaths(m_filePathAndName.CStr(), isURL);
}

OP_STATUS WebserverFileName::Construct(const OpStringC &path)
{
	return Construct(path.CStr(), path.Length(), FALSE);	
}

OP_STATUS WebserverFileName::Construct(const OpStringC8 &path)
{
	return Construct(path.CStr(), path.Length(), FALSE);	
}

OP_STATUS WebserverFileName::ConstructFromURL(const OpStringC &path)
{
	return Construct(path.CStr(), path.Length(), TRUE);	
}

OP_STATUS WebserverFileName::ConstructFromURL(const OpStringC8 &path)
{
	return Construct(path.CStr(), path.Length(), TRUE);
}

#ifdef _DEBUG
BOOL WebserverFileName::DebugIsURLAcceptable(const char *filePathAndName, const char *expected)
{
	OpString url;
	if (OpStatus::IsError(url.Set(filePathAndName)))
		return FALSE;

	OP_STATUS ops=CheckAndFixDangerousPaths(url.CStr(), TRUE);
	if(expected && OpStatus::IsSuccess(ops))
		return !url.Compare(expected);

	return OpStatus::IsSuccess(ops);
}

BOOL WebserverFileName::DebugIsURLInAcceptable(const char *filePathAndName, const char *expected)
{
	OpString url;
	if (OpStatus::IsError(url.Set(filePathAndName)))
		return FALSE;

	OP_STATUS ops=CheckAndFixDangerousPaths(url.CStr(), TRUE);
	if(expected && OpStatus::IsError(ops))
		return !url.Compare(expected);

	return OpStatus::IsError(ops);
}
#endif // _DEBUG

#endif //WEBSERVER_SUPPORT
