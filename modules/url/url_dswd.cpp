/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/viewers/viewers.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/util/timecache.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/url/url2.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/util/handy.h"

#include "modules/encodings/utility/charsetnames.h"

#include "modules/util/htmlify.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/url/url_man.h"
#include "modules/cache/url_cs.h"

#include "modules/olddebug/timing.h"

#include "modules/url/url_pd.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/formats/url_dfr.h"
#include "modules/formats/argsplit.h"
#include "modules/mime/mime_cs.h"

#if defined SUPPORT_AUTO_PROXY_CONFIGURATION
# include "modules/autoproxy/autoproxy_public.h"
#endif

#include "modules/url/url_link.h"

#include "modules/olddebug/tstdump.h"
#include "modules/url/tools/url_debug.h"


OP_STATUS URL::WriteDocumentDataUniSprintf(const uni_char *format, ...)
{
	OpString temp_buffer;
	va_list args;

	va_start(args, format);
	RETURN_IF_ERROR(temp_buffer.AppendVFormat(format, args));

	va_end(args);

	return rep->WriteDocumentData(URL::KNormal, temp_buffer);
}

#if defined(NEED_URL_WRITE_DOC_IMAGE) || defined(SELFTEST)
OP_STATUS URL::WriteDocumentImage(const char * ctype, const char * data, int size)
{
	RETURN_IF_ERROR(SetAttribute(URL::KMIME_ForceContentType, ctype));

	RETURN_IF_ERROR(WriteDocumentData(URL::KNormal, data, size));
	WriteDocumentDataFinished();

	OpFileLength size1 = size;
	RETURN_IF_ERROR(SetAttribute(URL::KContentSize, &size1));

	return OpStatus::OK;
}
#endif


OP_STATUS	URL::WriteDocumentData(URL::URL_WriteDocumentConversion conversion, const OpStringC &data, unsigned int len)
{
	return rep->WriteDocumentData(conversion, data, len);
}

OP_STATUS	URL::WriteDocumentData(URL::URL_WriteDocumentConversion conversion, const OpStringC8 &data, unsigned int len)
{
	return rep->WriteDocumentData(conversion, data, len);
}

#ifdef HAS_SET_HTTP_DATA
OP_STATUS	URL::WriteDocumentData(URL::URL_WriteDocumentConversion conversion, URL_DataDescriptor *data, unsigned int len)
{
	return rep->WriteDocumentData(conversion, data, len);
}
#endif


OP_STATUS	URL_Rep::WriteDocumentData(URL::URL_WriteDocumentConversion conversion, const OpStringC &data, unsigned int len)
{
	if(!data.CStr() || len == 0 || (len == (unsigned int) KAll && data.IsEmpty()))
		return OpStatus::OK;

	if(!CheckStorage())
		return OpStatus::ERR_NO_MEMORY;
	return storage->WriteDocumentData(conversion, data, len);
}

OP_STATUS	URL_Rep::WriteDocumentData(URL::URL_WriteDocumentConversion conversion, const OpStringC8 &data, unsigned int len)
{
	if(!data.CStr() || len == 0 || (len == (unsigned int) KAll && data.IsEmpty()))
		return OpStatus::OK;

	if(!CheckStorage())
		return OpStatus::ERR_NO_MEMORY;
	return storage->WriteDocumentData(conversion, data, len);
}

#ifdef HAS_SET_HTTP_DATA
OP_STATUS	URL_Rep::WriteDocumentData(URL::URL_WriteDocumentConversion conversion, URL_DataDescriptor *data, unsigned int len)
{
	if(data == NULL)
		return OpStatus::OK;

	if(!CheckStorage())
		return OpStatus::ERR_NO_MEMORY;
	return storage->WriteDocumentData(conversion, data, len);
}
#endif


#ifdef URL_ENABLE_ASSOCIATED_FILES
OpFile *URL::CreateAssociatedFile(AssociatedFileType type)
{
	return rep == NULL ? NULL : rep->CreateAssociatedFile(type);
}

OpFile *URL::OpenAssociatedFile(AssociatedFileType type)
{
	return rep == NULL ? NULL : rep->OpenAssociatedFile(type);
}

AssociatedFile *URL_Rep::CreateAssociatedFile(URL::AssociatedFileType type)
{
	AssociatedFile *f;

	if (storage == NULL)
		return NULL;

	if ((f = storage->CreateAssociatedFile(type)) != NULL)
		f->SetRep(this);

	return f;
}

AssociatedFile *URL_Rep::OpenAssociatedFile(URL::AssociatedFileType type)
{
	AssociatedFile *f;

	if (storage == NULL)
		return NULL;

	if ((f = storage->OpenAssociatedFile(type)) != NULL)
		f->SetRep(this);

	return f;
}
#endif

OP_STATUS URL_DataStorage::WriteDocumentData(URL::URL_WriteDocumentConversion conversion, const OpStringC &data, unsigned int len)
{
	if(!data.CStr() || len == 0 || (len == (unsigned int) KAll && data.IsEmpty()))
		return OpStatus::OK;

	if(len == (unsigned int) KAll)
		len = data.Length();

	BOOL convert = FALSE;
	BOOL xmlify = FALSE;
	BOOL no_link = TRUE;

	switch(conversion)
	{
	/** Normal Raw Data */
	case URL::KNormal:
		break;
	/** HTMLify data, if there are URLs A tag links are created */
	case URL::KHTMLify_CreateLinks:
		no_link = FALSE;
	/** HTMLify data */
	case URL::KHTMLify:
		convert = TRUE;
		break;
	/** XMLify data, if there are URLs A tag links are created */
	case URL::KXMLify_CreateLinks:
		no_link = FALSE;
	/** XMLify data */
	case URL::KXMLify:
		convert = TRUE;
		xmlify = TRUE;
		break;
	}

	uni_char *converted_string = NULL;
	const uni_char *input_data = data.CStr();

	if(convert)
	{
		converted_string = XHTMLify_string(input_data, len, no_link, xmlify, FALSE);

		if(converted_string == NULL)
			return OpStatus::ERR_NO_MEMORY;

		input_data = converted_string;
		len = uni_strlen(input_data);
	}

	OP_STATUS op_err = WriteDocumentData((const unsigned char *)input_data, UNICODE_SIZE(len));

	OP_DELETEA(converted_string);

	return op_err;
}

OP_STATUS URL_DataStorage::WriteDocumentData(URL::URL_WriteDocumentConversion conversion, const OpStringC8 &data, unsigned int len)
{
	if(!data.CStr() || len == 0 || (len == (unsigned int) KAll && data.IsEmpty()))
		return OpStatus::OK;

	if(len == (unsigned int) KAll)
		len = data.Length();

	BOOL convert = FALSE;
	BOOL xmlify = FALSE;
	BOOL no_link = TRUE;

	switch(conversion)
	{
	/** Normal Raw Data */
	case URL::KNormal:
		break;
	/** HTMLify data, if there are URLs A tag links are created */
	case URL::KHTMLify_CreateLinks:
		no_link = FALSE;
	/** HTMLify data */
	case URL::KHTMLify:
		convert = TRUE;
		break;
	/** XMLify data, if there are URLs A tag links are created */
	case URL::KXMLify_CreateLinks:
		no_link = FALSE;
	/** XMLify data */
	case URL::KXMLify:
		convert = TRUE;
		xmlify = TRUE;
		break;
	}

	char *converted_string = NULL;
	const char *input_data = data.CStr();

	if(convert)
	{
		converted_string = XHTMLify_string(input_data, len, no_link, xmlify, FALSE);

		if(converted_string == NULL)
			return OpStatus::ERR_NO_MEMORY;

		input_data = converted_string;
		len = op_strlen(input_data);
	}

	OP_STATUS op_err = WriteDocumentData((const unsigned char *) input_data, len);

	OP_DELETEA(converted_string);

	return op_err;
}

#ifdef HAS_SET_HTTP_DATA
OP_STATUS URL_DataStorage::WriteDocumentData(URL::URL_WriteDocumentConversion conversion, URL_DataDescriptor *data, unsigned int len)
{
	if(data == NULL)
		return OpStatus::OK;

	if(len == (unsigned int) KAll)
		len = 0;

	BOOL convert = FALSE;

	switch(conversion)
	{
	/** HTMLify data, if there are URLs A tag links are created */
	case URL::KHTMLify_CreateLinks:
	/** HTMLify data */
	case URL::KHTMLify:
	/** XMLify data, if there are URLs A tag links are created */
	case URL::KXMLify_CreateLinks:
	/** XMLify data */
	case URL::KXMLify:
		convert = TRUE;
		break;
	}

	BOOL more =TRUE;

	while(more)
	{
		unsigned int blen = data->RetrieveData(more);
		const uni_char *buffer = (const uni_char *) data->GetBuffer();

		if(len && blen > len)
		{
			blen = len;
			more = FALSE;
		}

		if(!convert)
		{
			// Raw copy
			RETURN_IF_ERROR(WriteDocumentData((unsigned char *) buffer, blen));
			data->ConsumeData(blen);
			if(len)
				len -= blen;
			continue;
		}

		// always convert

		if(blen < UNICODE_SIZE(1))
		{
			break; // Odd byte at end // don't commit just in case
		}

		if(more && (!len  || blen < len))
		{
			unsigned int use_len = UNICODE_DOWNSIZE(blen);

			while(use_len >0)
			{
				use_len--;
				uni_char tmp = buffer[use_len];
				
				if(tmp == '\r' || tmp == '\n')
				{
					use_len ++;
					break;
				}
			}
			
			if(use_len)
			{
				blen = UNICODE_SIZE(use_len);
			}
			else
				blen = (blen > 4096 ? blen -2048 : blen);
		}

		unsigned int string_len = UNICODE_DOWNSIZE(blen);

		RETURN_IF_ERROR(WriteDocumentData(conversion, buffer, string_len));

		data->ConsumeData(blen);

		if(len)
			len -= UNICODE_SIZE(string_len);
	}

	return OpStatus::OK;
}
#endif


OP_STATUS URL_DataStorage::WriteDocumentData(const unsigned char *data, unsigned int len)
{
	if(data == NULL || len == 0)
		return OpStatus::OK;

	if(storage == NULL)
	{
		OP_STATUS op_err= CreateCache();
	
		if(OpStatus::IsError(op_err))
		{
			EndLoading(TRUE);
			return op_err;
		}
	}
	storage->ForceKeepOpen();
	url->SetAttribute(URL::KIsGenerated, TRUE);

#ifdef _DEBUG_DS
	PrintfTofile("urlds.txt","\nDS WriteDocumentData - writing %d bytes: %s\n",len, DebugGetURLstring(url));
	DumpTofile((const unsigned char *) data, (unsigned long) len,"","urlds.txt");

	URLContentType ctype = (URLContentType) GetAttribute(URL::KContentType);

	if(ctype == URL_HTML_CONTENT || ctype == URL_TEXT_CONTENT || 
		ctype == URL_XML_CONTENT || ctype == URL_X_JAVASCRIPT || 
		ctype == URL_CSS_CONTENT || ctype == URL_PAC_CONTENT)
	{
		const char *rdata = (const char *)data;
		unsigned long len2 = len;
		OpString8 encoded;

		if(GetAttribute(URL::KMIME_CharSet).CompareI("utf-16") == 0)
		{
			if(OpStatus::IsSuccess(encoded.SetUTF8FromUTF16((const uni_char *)data, UNICODE_DOWNSIZE(len))))
			{
				rdata = encoded.CStr();
				len2 = encoded.Length();
			}
		}

		PrintfTofile("urlds.txt","\n-----\n%.*s\n------\n",len2,rdata);
	}
#endif
	
	return storage->StoreDataEncode((unsigned char *) data, len);
}

void URL::WriteDocumentDataFinished()
{
	rep->WriteDocumentDataFinished();
}

void URL_Rep::WriteDocumentDataFinished()
{
	if(storage)
		storage->WriteDocumentDataFinished();
}

void URL_DataStorage::WriteDocumentDataFinished()
{
	SetAttribute(URL::KLoadStatus, URL_LOADED);
	if(storage)
		storage->SetFinished();

#if defined(OPERA_URL_SUPPORT) || defined(HAS_OPERABLANK)
	URLType url_type = (URLType) url->GetAttribute(URL::KType);
	if(url_type == URL_OPERA)
		return;
	else
#endif // OPERA_URL_SUPPORT || HAS_OPERABLANK
	if(storage)
	{
		URL_DataDescriptor *desc;

		desc = storage->First();
		while(desc)
		{
			if(!desc->PostedMessage())
				desc->PostMessage(MSG_URL_DATA_LOADED, url->GetID(), 0);

			desc = (URL_DataDescriptor *) desc->Suc();
		}
	}

	BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
}

#ifdef URL_WRITE_DOCUMENT_SIGNAL_DATA
void URL::WriteDocumentDataSignalDataReady()
{
	rep->WriteDocumentDataSignalDataReady();
}

void URL_Rep::WriteDocumentDataSignalDataReady()
{
	if(storage)
		storage->WriteDocumentDataSignalDataReady();
}

void URL_DataStorage::WriteDocumentDataSignalDataReady()
{
	if(!GetAttribute(URL::KHeaderLoaded))
	{
		SetAttribute(URL::KHeaderLoaded, TRUE);
		BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), 0, MH_LIST_ALL);
	}

	BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), FALSE, MH_LIST_ALL);
}
#endif

