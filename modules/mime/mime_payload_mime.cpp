/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
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

MIME_Mime_Payload::MIME_Mime_Payload(HeaderList &hdrs, URLType url_type)
: MIME_MultipartBase(hdrs, url_type)
{
}

void MIME_Mime_Payload::ProcessDecodedDataL(BOOL more)
{
	if(GetLengthDecodedData() == 0)
		return;

	if(!current_element)
	{
		const unsigned char *src = AccessDecodedData();
		unsigned long src_len = GetLengthDecodedData();

		unsigned long header_len = 0;
		const unsigned char *current = src;
		BOOL found_end_of_header = FALSE;

		while(header_len < src_len)
		{
			header_len++;
			if(*(current++) == '\n')
			{
				if(header_len < src_len)
				{
					if(*current == '\r')
					{
						current ++;
						if(header_len >= src_len)
							break;
						header_len ++;
					}
					if(*current == '\n')
					{
						header_len++;
						found_end_of_header = TRUE;
						break;
					}
				}
			}
		}

		if(!found_end_of_header)
		{
			if(more)
				return;
			header_len = src_len;
		}

		CreateNewBodyPartL(src, header_len);
		CommitDecodedDataL(header_len);

		if (current_element)
			current_element->SetDisplayHeaders(TRUE);
	}

	SaveDataInSubElementL();
}

#endif // MIME_SUPPORT
