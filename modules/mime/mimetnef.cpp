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

/**
 *  Microsoft Transport Neutral Encapsulation Format (TNEF) MIME decoder
 *
 *  Format is documented at 
 *  http://msdn.microsoft.com/library/default.asp?URL=/library/psdk/mapi/_mapi1book_transport_neutral_encapsulation_format_tnef_.htm
 */

#include "core/pch.h"

#include "modules/url/url2.h"
#include "modules/formats/hdsplit.h"
#include "modules/mime/mimedec2.h"

#include "modules/olddebug/tstdump.h"

#ifdef _MIME_SUPPORT_
#include "modules/mime/mimetnef.h"

#include "modules/viewers/viewers.h"

MS_TNEF_Decoder::MS_TNEF_Decoder(HeaderList &hdrs, URLType url_type)
: MIME_MultipartBase(hdrs, url_type)
{
	Init();
}

void MS_TNEF_Decoder::Init()
{
	tnef_info.no_processing = FALSE;

	tnef_info.read_header = FALSE;
	tnef_info.read_key = FALSE;

	tnef_info.attachment_part = FALSE;
	tnef_info.read_attach_rend= FALSE;

	tnef_info.record_class_read= FALSE;
	tnef_info.record_type_read= FALSE;
	tnef_info.record_read_length = FALSE;
	tnef_info.record_reading_record= FALSE;
	tnef_info.record_data_to_subelement = FALSE;
	tnef_info.record_wait_for_all = FALSE;
	tnef_info.record_read_checksum= FALSE;

	record_checksum = 0;
	current_record_type = 0;
	record_length = 0;
	record_read = 0;
	unnamed_contents = NULL;
}

MS_TNEF_Decoder::~MS_TNEF_Decoder()
{
	OP_DELETE(unnamed_contents);
}

void MS_TNEF_Decoder::ProcessDecodedDataL(BOOL more)
{
	if(tnef_info.no_processing)
	{
		SaveDataInSubElementL();
		return;
	}
	
	while(GetLengthDecodedData())
	{
		if(!tnef_info.read_header)
		{
			uint32 val = 0;
			if(AccessDecodedStream().ReadIntegerL(val, 4, FALSE, FALSE, TRUE) != OpRecStatus::FINISHED)
				break;

			if(val != TNEF_SIGNATURE)
			{
				info.decode_warning = TRUE;
				tnef_info.no_processing = TRUE;
				CreateNewMIME_PayloadBodyPartL(headers);
				SaveDataInSubElementL();
				break;
			}
			CommitDecodedDataL(4);

			tnef_info.read_header = TRUE;
		}
		if(!tnef_info.read_key)
		{
			uint32 val = 0;
			if(AccessDecodedStream().ReadIntegerL(val, 2, FALSE, FALSE) != OpRecStatus::FINISHED)
				break;

			tnef_info.read_key= TRUE;
		}

		if(!tnef_info.record_class_read)
		{
			uint32 record_class = 0;
			if(AccessDecodedStream().ReadIntegerL(record_class , 1, FALSE, FALSE) != OpRecStatus::FINISHED)
				break;

			if(!tnef_info.attachment_part)
			{
				if(record_class != TNEF_MESSAGE)
				{
					if(record_class != TNEF_ATTACHMENT)
					{
						info.decode_warning = info.finished = TRUE;
						break;
					}
					tnef_info.attachment_part = TRUE;
				}
			}
			else
			{
				if(record_class != TNEF_ATTACHMENT)
				{
					info.decode_warning = info.finished = TRUE;
					break;
				}
			}

			tnef_info.record_class_read = TRUE;
		}

		if(!tnef_info.record_type_read)
		{
			if(AccessDecodedStream().ReadIntegerL(current_record_type, 4, FALSE, FALSE) != OpRecStatus::FINISHED)
				break;

			switch(current_record_type)
			{
			case 0x00069002: // attAttachRenddata, separates different attachments
				HandleUnnamedContentsL();

				tnef_info.read_attach_rend = TRUE;
				FinishSubElementL();
				filename.Empty();
				mime_type.Empty();
				break;
			case 0x00018010: // Title (Filename)
				tnef_info.record_wait_for_all = TRUE;
				break;
			case 0x0002800C: // data?
			case 0x0006800F: // Data (for file)
				HandleUnnamedContentsL();

				if (filename.IsEmpty())
				{
					unnamed_contents = OP_NEW_L(DataStream_FIFO_Stream, ());
					unnamed_contents->SetIsFIFO();
				}
				else
				{
					if (mime_type.IsEmpty())
						mime_type.SetL("application/octet-stream");
	
					CreateNewBodyPartWithNewHeaderL(mime_type, filename, "binary");

					filename.Empty();
					mime_type.Empty();
				}
				tnef_info.record_data_to_subelement = TRUE;
				break;
			}

			tnef_info.record_type_read = TRUE;
			record_read =0;
			record_checksum = 0;
		}

		if(!tnef_info.record_read_length)
		{
			if(AccessDecodedStream().ReadIntegerL(record_length, 4, FALSE, FALSE) != OpRecStatus::FINISHED)
				break;

			record_read =0;

			tnef_info.record_read_length = TRUE;
			tnef_info.record_reading_record = TRUE;
			record_checksum = 0;
		}

		if(tnef_info.record_reading_record)
		{
			unsigned long len = record_length - record_read;


			if(len >= GetLengthDecodedData())
				len = GetLengthDecodedData();

			if(len == 0 && record_length != record_read)
				break;

			if(tnef_info.record_wait_for_all && record_length != len)
				break;

			record_read += len;

			const unsigned char *src = AccessDecodedData();

			if(tnef_info.record_data_to_subelement)
			{
				if (unnamed_contents)
					unnamed_contents->WriteDataL(src, len);
				else
					SaveDataInSubElementL(src, len);
			}

			if(record_read == record_length && current_record_type== 0x00018010) // Filename
			{
				filename.SetL((const char *) src, len);

				int pos = filename.Find("?#;");
				if(pos != KNotFound)
					filename.Delete(pos);
				pos = filename.FindLastOf('/');
				if(pos != KNotFound)
					filename.Delete(0,pos);
				pos = filename.FindLastOf('\\');
				if(pos != KNotFound)
					filename.Delete(0,pos);

				pos = filename.FindLastOf('.');
				if (pos != KNotFound)
				{
					Viewer *v = g_viewers->FindViewerByFilename(filename.CStr());
					if (v && v->GetContentTypeString8() && op_strchr(v->GetContentTypeString8(),'/') != NULL)
						mime_type.SetL(v->GetContentTypeString8());
				}

				HandleUnnamedContentsL();
			}

			uint32 i;
			for(i=0; i<len ; i++)
			{
				record_checksum += src[i];
			}
			CommitDecodedDataL(len);

			if(record_read == record_length)
			{
				tnef_info.record_data_to_subelement = FALSE;
				tnef_info.record_reading_record = FALSE;
			}
			else 
				break;
		}

		// checksum always
		{
			uint32 val = 0;
			if(AccessDecodedStream().ReadIntegerL(val, 2, FALSE, FALSE) != OpRecStatus::FINISHED)
				break;

			if(val != record_checksum)
			{
				info.decode_warning = TRUE;
			}
			tnef_info.record_read_length = FALSE;
			tnef_info.record_class_read = FALSE;
			tnef_info.record_type_read = FALSE;
			tnef_info.record_read_length = FALSE;
			tnef_info.record_wait_for_all = FALSE;
			tnef_info.record_data_to_subelement = FALSE;
		}
	}
}
	
void MS_TNEF_Decoder::HandleUnnamedContentsL()
{
	if (unnamed_contents == NULL)
		return;

	TRAPD(op_err, SaveUnnamedContentsL());

	filename.Empty();
	mime_type.Empty();

	OP_DELETE(unnamed_contents);
	unnamed_contents = NULL;

	if(OpStatus::IsError(op_err))
		LEAVE(op_err);
}

void MS_TNEF_Decoder::SaveUnnamedContentsL()
{
	if (mime_type.IsEmpty())
		mime_type.SetL("application/octet-stream");

	CreateNewBodyPartWithNewHeaderL(mime_type, filename, "binary");

	while (unnamed_contents->GetLength())
	{
		SaveDataInSubElementL(unnamed_contents->GetDirectPayload(), unnamed_contents->GetLength());
		unnamed_contents->CommitSampledBytesL(unnamed_contents->GetLength());
	}
	FinishSubElementL();
}


#endif
