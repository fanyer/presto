/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve N. Pettersen
 */
#include "core/pch.h"

#ifdef SELFTEST

#include "modules/selftest/src/testutils.h"
#include "modules/network_selftest/serverpush_doctestman.h"
#include "modules/util/opfile/opfile.h"
#include "modules/datastream/fl_lib.h"
#include "modules/locale/oplanguagemanager.h"

OP_STATUS ServerPush_Filename_Item::Construct(const OpStringC8 &a_filename)
{
	RETURN_IF_ERROR(filename.SetFromUTF8(a_filename));

	OpFile temp_file;

	temp_file.Construct(filename);

	OpStringC actual_name(temp_file.GetName());

	if(actual_name.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	int number = uni_atoi(actual_name.CStr());

	if(number <= 0)
		return OpStatus::ERR;

	item = number;

	return OpStatus::OK;
}


OP_STATUS ServerPush_Filename_List::AddItem(const OpStringC8 &a_filename)
{
	if(a_filename.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	OpAutoPtr<ServerPush_Filename_Item> item(OP_NEW(ServerPush_Filename_Item, ()));

	if(item.get() == NULL)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(item->Construct(a_filename));

	int number = item->GetItem();

	ServerPush_Filename_Item *current_item = First();

	while(current_item && current_item->GetItem() < number)
		current_item = current_item->Suc();

	OP_ASSERT(current_item == NULL || current_item->GetItem() != number);

	if(current_item)
		item->Precede(current_item);
	else
		item->Into(this);

	item.release();

	return OpStatus::OK;
}

ServerPush_DocTester::ServerPush_DocTester(URL &a_url, URL &ref, ServerPush_Filename_List &names)
 : URL_DocSelfTest_Item(a_url, ref), started(FALSE), count(0), header_loaded(FALSE), 
	current_item(NULL), current_file(NULL), current_descriptor(NULL)
{
	test_items.Append(&names);
}

ServerPush_DocTester::~ServerPush_DocTester()
{
	started = FALSE;
	current_item = NULL;
	OP_DELETE(current_file);
	current_file = NULL;
	OP_DELETE(current_descriptor);
	current_descriptor = NULL;
}

BOOL ServerPush_DocTester::Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code)
{
	switch(event)
	{
	case URLDocST_Event_LoadingRestarted:
		if(!header_loaded)
		{
			ReportFailure("No header loaded before bodypart #%d", count);
			return FALSE;
		}

		if(current_item == NULL)
		{
			ReportFailure("Out of order messaging (#%d)", count);
			return FALSE;
		}

		if(!CheckContent(TRUE))
			return FALSE;

		break;
	case URLDocST_Event_Data_Received:
		{
			if(current_item == NULL)
			{
				ReportFailure("Out of order messaging (#%d)", count);
				return FALSE;
			}

			URLStatus status = (URLStatus) url.GetAttribute(URL::KLoadStatus);
			
			if(status == URL_LOADED)
			{
				if(current_item == NULL)
				{
					break; // success
				}
			}
			else if(status != URL_LOADING)
			{
				ReportFailure("Some kind of loading failure.");
				return FALSE;
			}

			if(!CheckContent(status == URL_LOADED))
				return FALSE;

		}
		break;
	case URLDocST_Event_Header_Loaded:
		if(header_loaded)
		{
			ReportFailure("Multiple header loaded.");
			return FALSE;				
		}
		count++;
		
		if(current_item == NULL && started)
		{
			ReportFailure("Extra bodypart (#%d)", count);
			return FALSE;
		}

		if(!started)
		{
			started = TRUE;
			current_item = test_items.First();
		}
		else
			current_item = current_item->Suc();

		if(current_item == NULL)
		{
			ReportFailure("Got a bodypart when none was expected (#%d)", count);
			return FALSE;
		}

		header_loaded = TRUE;
		break;
	default:
		{
			OpString temp;
			OpString8 lang_code, errorstring;
			g_languageManager->GetString(Str::LocaleString(status_code), temp);
			lang_code.SetUTF8FromUTF16(temp);
			temp.Empty();
			url.GetAttribute(URL::KInternalErrorString, temp);
			errorstring.SetUTF8FromUTF16(temp);

			ReportFailure("Some kind of loading failure %d:%d:\"%s\":\"%s\" .", (int) event, (int) status_code,
				(lang_code.HasContent() ? lang_code.CStr() : "<No String>"),
				(errorstring.HasContent() ? errorstring.CStr() : "")
				);
		}
		return FALSE;
		
	}
	return TRUE;
}

BOOL ServerPush_DocTester::CheckContent(BOOL finished)
{
	unsigned char *temp_buffer = (unsigned char *) g_memory_manager->GetTempBuf();

	if(!current_file && !current_descriptor)
	{
		OpAutoPtr<OpFile> reading_file(OP_NEW(OpFile, ()));

		if(reading_file.get() == NULL || OpStatus::IsError(reading_file->Construct(current_item->GetName())))
		{
			ReportFailure("Open file #%d failed.", current_item->GetItem());
			return FALSE;				
		}
	
		if(OpStatus::IsError(reading_file->Open(OPFILE_READ)))
		{
			ReportFailure("Open file #%d failed.", current_item->GetItem());
			return FALSE;				
		}

		OpFile* f = reading_file.release();
		OpAutoPtr<DataStream_GenericFile> new_reader(OP_NEW(DataStream_GenericFile, (f)));

		if(new_reader.get() == NULL)
		{
			ReportFailure("Open file #%d failed.", current_item->GetItem());
			return FALSE;				
		}

		TRAPD(op_err, new_reader->InitL());
		if(OpStatus::IsError(op_err))
		{
			ReportFailure("Open file #%d failed.", current_item->GetItem());
			return FALSE;				
		}

		current_file = new_reader.release();

		current_descriptor = url.GetDescriptor(g_main_message_handler, FALSE, TRUE, FALSE);
		if(current_descriptor == NULL)
		{
			if(current_file->MoreData())
			{
				ReportFailure("Compare failed #%d.", current_item->GetItem());
				return FALSE;				
			}
			OP_DELETE(current_file);
			current_file = NULL;

			return TRUE;
		}
		OP_ASSERT(current_file != NULL);
		OP_ASSERT(current_descriptor != NULL);
	}
	else if(!(current_file && current_descriptor))
	{
		ReportFailure("Internal error. File #%d.", current_item->GetItem());
		return FALSE;
	}

	OP_ASSERT(current_file != NULL);
	OP_ASSERT(current_descriptor != NULL);

	BOOL more=TRUE;
	
	do{
		OP_MEMORY_VAR unsigned long buf_len = current_descriptor->RetrieveData(more);
		OP_MEMORY_VAR OpFileLength read_len = 0;
		
		if(buf_len > g_memory_manager->GetTempBufLen())
		{
			buf_len = g_memory_manager->GetTempBufLen();
		}
		
		if(buf_len == 0)
		{
			if(!finished)
				break;

			if(current_file->MoreData() || more)
			{
				ReportFailure("File size #%d mismatch", current_item->GetItem());
				return FALSE;				
			}
			break;
		}

		TRAPD(op_err, read_len = current_file->ReadDataL(temp_buffer, buf_len));
		if(OpStatus::IsError(op_err))
		{
			ReportFailure("Read file #%d failed.", current_item->GetItem());
			return FALSE;				
		}

		if(read_len != (OpFileLength)buf_len)
		{
			ReportFailure("File size #%d mismatch", current_item->GetItem());
			return FALSE;				
		}
		
		if(current_descriptor->GetBuffer() == NULL || op_memcmp(current_descriptor->GetBuffer(), temp_buffer, buf_len) != 0)
		{
			ReportFailure("Compare failed #%d.", current_item->GetItem());
			return FALSE;				
		}

		current_descriptor->ConsumeData(buf_len);
		
		if(more || !finished)
			return TRUE;
		
		if(current_file->MoreData())
		{
			ReportFailure("Compare failed #%d.", current_item->GetItem());
			return FALSE;				
		}
		
	}while(1);

	if(finished)
	{
		current_file->Close();
		OP_DELETE(current_file);
		current_file = NULL;
		OP_DELETE(current_descriptor);
		current_descriptor = NULL;
		header_loaded = FALSE;
	}
	return TRUE;

}

#endif  // SELFTEST
