/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"


#ifdef _MIME_SUPPORT_
#include "modules/mime/mimedec2.h"

#include "modules/hardcore/mem/mem_man.h"
#include "modules/util/handy.h"

#include "modules/url/url_man.h"

#include "modules/olddebug/tstdump.h"

#include "modules/url/tools/arrays.h"

#define CONST_KEYWORD_ARRAY(name) PREFIX_CONST_ARRAY(static, KeywordIndex, name, mime)
#define CONST_KEYWORD_ENTRY(x,y) CONST_DOUBLE_ENTRY(keyword, x, index, y)
#define CONST_KEYWORD_END(name) CONST_END(name)


CONST_KEYWORD_ARRAY(mime_accesstype_keywords)
	CONST_KEYWORD_ENTRY((char *) NULL, MIME_Other_Access)
#ifndef NO_FTP_SUPPORT
	CONST_KEYWORD_ENTRY("anon-ftp", MIME_AnonFTP)
	CONST_KEYWORD_ENTRY("ftp", MIME_FTP)
#endif // NO_FTP_SUPPORT
	CONST_KEYWORD_ENTRY("local-file", MIME_LocalFile)
#ifndef NO_FTP_SUPPORT
	CONST_KEYWORD_ENTRY("tftp", MIME_TFTP)
#endif // NO_FTP_SUPPORT
	CONST_KEYWORD_ENTRY("uri", MIME_URI)
CONST_KEYWORD_END(mime_accesstype_keywords)


MIME_External_Payload::MIME_External_Payload(HeaderList &hdrs, URLType url_type)
: MIME_Payload(hdrs, url_type)
{
}


void MIME_External_Payload::HandleHeaderLoadedL()
{
	MIME_Payload::HandleHeaderLoadedL();

	if(content_type_header)
	{
		ParameterList *parameters = content_type_header->SubParameters();

		OP_ASSERT(parameters); // content_type_header != NULL  => parameters != NULL

		{
			Parameters *param = parameters->GetParameterByID(HTTP_General_Tag_Access_Type, PARAMETER_ASSIGNED_ONLY);
			ANCHORD(OpString8, templink);
			const char *link= NULL;
			
			if(param && param->Value())
			{
				MIME_ExtBody_Accesstype access_type = (MIME_ExtBody_Accesstype) CheckKeywordsIndex(param->Value(), g_mime_accesstype_keywords, (int)CONST_ARRAY_SIZE(mime, mime_accesstype_keywords));
				
				switch(access_type)
				{
				case MIME_URI:
					param = parameters->GetParameterByID(HTTP_General_Tag_Name,PARAMETER_ASSIGNED_ONLY);
					if(param && param->Value())
					{
						link = param->Value();
					}
					break;
#ifndef NO_FTP_SUPPORT
				case MIME_FTP:
				case MIME_AnonFTP:
				case MIME_TFTP:
#endif // NO_FTP_SUPPORT
				case MIME_LocalFile:
					{
						param = parameters->GetParameterByID(HTTP_General_Tag_Name,PARAMETER_ASSIGNED_ONLY);
						Parameters *site_param = parameters->GetParameterByID(HTTP_General_Tag_Site,PARAMETER_ASSIGNED_ONLY);
						Parameters *dir_param = parameters->GetParameterByID(HTTP_General_Tag_Directory,PARAMETER_ASSIGNED_ONLY);
						if(param && param->Value())
						{
							const char *site = site_param ? site_param->Value() : "localhost";
							const char *directory = dir_param ? dir_param->Value() : NULL;
							const char *dir_end = "/";
							const char *scheme = "file";
							
							if(site && site[0] == '*' && site[1] == '\0')
								site = "localhost";
							
							if(directory)
							{
								if(*directory == '/')
									directory++;
								
								if(directory[op_strlen(directory)-1] == '/')
									dir_end = NULL;
							}
#ifndef NO_FTP_SUPPORT
							switch(access_type)
							{
							case MIME_FTP:
							case MIME_AnonFTP:
								scheme = "ftp";
								break;
							case MIME_TFTP:
								scheme = "tftp";
								break;
							}
#endif // NO_FTP_SUPPORT
							
							if(OpStatus::IsError(templink.SetConcat(scheme, "://", site, "/", directory, dir_end, param->Value())))
							{
								g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
								RaiseDecodeWarning();
								FinishedLoadingL();
								return;
							}
							link = templink.CStr();
						}
					}
					break;
					
				default: break; // MIME_Other_Access: nothing to do ... ?
				}
			}
			
			if(link)
			{
				URL temp_url = urlManager->GetURL(link);
				attachment.SetURL(temp_url);

				if(GetUseNoStoreFlag())
					attachment->SetAttributeL(URL::KCachePolicy_NoStore, TRUE);

				info.finished = TRUE;
			}
		}
	}
	else
		RaiseDecodeWarning();
}

#endif // MIME_SUPPORT
