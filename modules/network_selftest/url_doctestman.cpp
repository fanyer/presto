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
#include "modules/network_selftest/urldoctestman.h"
#include "modules/util/opfile/opfile.h"
#include "modules/datastream/fl_lib.h"
#include "modules/cache/url_stream.h"
#include "modules/locale/oplanguagemanager.h"

URL_DocSelfTest_Item::URL_DocSelfTest_Item()
: owner(NULL), had_action(FALSE)
{
}

URL_DocSelfTest_Item::URL_DocSelfTest_Item(URL &a_url, URL &a_referer, BOOL unique)
: owner(NULL), had_action(FALSE), url(a_url), url_use(a_url), referer(a_referer)
{
	if(unique)
		g_url_api->MakeUnique(url);
}

void URL_DocSelfTest_Item::Construct(URL &a_url, URL &ref, BOOL unique)
{
	url = a_url;
	url_use = a_url;
	referer = ref;
	if(unique)
		g_url_api->MakeUnique(url);
}


URL_DocSelfTest_Item::~URL_DocSelfTest_Item()
{
	owner = NULL;

	if(InList())
		Out();
}

void URL_DocSelfTest_Item::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	URL_DocSelfTest_Event event = URLDocST_Event_Unknown;

	switch(event)
	{
	case URLDocST_Event_LoadingFailed:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Failed, this);
		return;
	case URLDocST_Event_LoadingFinished:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Finished, this);
		break;
	}
}

BOOL URL_DocSelfTest_Item::OnURLRedirected(URL &url, URL &redirected_to)
{
	URL_DocSelfTest_Event event = (Verify_function(URLDocST_Event_Redirect) ? URLDocST_Event_None : URLDocST_Event_LoadingFailed);

	switch(event)
	{
	case URLDocST_Event_LoadingFailed:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Failed, this);
		return FALSE;
	case URLDocST_Event_LoadingFinished:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Finished, this);
		return FALSE;
	}
	return TRUE;
}

BOOL URL_DocSelfTest_Item::OnURLHeaderLoaded(URL &url)
{
	had_action = TRUE;
	URL_DocSelfTest_Event event = (Verify_function(URLDocST_Event_Header_Loaded) ? URLDocST_Event_None : URLDocST_Event_LoadingFailed);

	switch(event)
	{
	case URLDocST_Event_LoadingFailed:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Failed, this);
		return FALSE;
	case URLDocST_Event_LoadingFinished:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Finished, this);
		return FALSE;
	}
	return TRUE;
}

BOOL URL_DocSelfTest_Item::OnURLDataLoaded(URL &url, BOOL finished, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
	URL_DocSelfTest_Event  event = (Verify_function(URLDocST_Event_Data_Received) ? URLDocST_Event_None : URLDocST_Event_LoadingFailed);
	if(event == URLDocST_Event_None) 
	{
		URLStatus status = (URLStatus) url.GetAttribute(URL::KLoadStatus, TRUE);

		if(status == URL_LOADED)
			event = URLDocST_Event_LoadingFinished;
		else if(status != URL_LOADING)
			event = URLDocST_Event_LoadingFailed;
	}

	switch(event)
	{
	case URLDocST_Event_LoadingFailed:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Failed, this);
		return FALSE;
	case URLDocST_Event_LoadingFinished:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Finished, this);
		return FALSE;
	}
	return (finished ? FALSE : TRUE);
}

BOOL URL_DocSelfTest_Item::OnURLRestartSuggested(URL &url, BOOL &restart_processing, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
	restart_processing = FALSE;
	had_action = TRUE;
	URL_DocSelfTest_Event event = (Verify_function(URLDocST_Event_LoadingRestarted) ? URLDocST_Event_None : URLDocST_Event_LoadingFailed);
	if(event == URLDocST_Event_None) 
	{
		URLStatus status = (URLStatus) url.GetAttribute(URL::KLoadStatus, TRUE);

		if(status == URL_LOADED)
			event = URLDocST_Event_LoadingFinished;
		else if(status != URL_LOADING)
			event = URLDocST_Event_LoadingFailed;
	}
	switch(event)
	{
	case URLDocST_Event_LoadingFailed:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Failed, this);
		return FALSE;
	case URLDocST_Event_LoadingFinished:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Finished, this);
		return FALSE;
	}
	return TRUE;
}

BOOL URL_DocSelfTest_Item::OnURLNewBodyPart(URL &url, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
	had_action = TRUE;
	URL_DocSelfTest_Event event = (Verify_function(URLDocST_Event_LoadingRestarted) ? URLDocST_Event_None : URLDocST_Event_LoadingFailed);
	if(event == URLDocST_Event_None) 
	{
		URLStatus status = (URLStatus) url.GetAttribute(URL::KLoadStatus, TRUE);

		if(status == URL_LOADED)
			event = URLDocST_Event_LoadingFinished;
		else if(status != URL_LOADING)
			event = URLDocST_Event_LoadingFailed;
	}

	switch(event)
	{
	case URLDocST_Event_LoadingFailed:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Failed, this);
		return FALSE;
	case URLDocST_Event_LoadingFinished:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Finished, this);
		return FALSE;
	}
	return TRUE;
}

void URL_DocSelfTest_Item::OnURLLoadingFailed(URL &url, Str::LocaleString error, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
	URL_DocSelfTest_Event event = (Verify_function(URLDocST_Event_LoadingFailed, error) ? URLDocST_Event_None : URLDocST_Event_LoadingFailed);

	switch(event)
	{
	case URLDocST_Event_LoadingFailed:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Failed, this);
		break;
	case URLDocST_Event_LoadingFinished:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Finished, this);
		break;
	}
}

void URL_DocSelfTest_Item::OnURLLoadingStopped(URL &url, OpAutoPtr<URL_DataDescriptor> &stored_desc)
{
	URL_DocSelfTest_Event event = (Verify_function(URLDocST_Event_LoadingFailed) ? URLDocST_Event_None : URLDocST_Event_LoadingFailed);

	switch(event)
	{
	case URLDocST_Event_LoadingFailed:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Failed, this);
		break;
	case URLDocST_Event_LoadingFinished:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Finished, this);
		break;
	}
}

void URL_DocSelfTest_Item::ReportFailure(const char *format, ...)
{
	OpString8 tempstring;
	OpString8 tempstring2;
	va_list args;
	va_start(args, format);

	if(format == NULL)
		format = "";


	OP_STATUS op_err = url.GetAttribute(URL::KName_Escaped, tempstring2);
	
	if(OpStatus::IsSuccess(op_err))
		op_err = tempstring.SetConcat(owner->GetBatchID(), " ", tempstring2, " :");

	if(OpStatus::IsSuccess(op_err))
		tempstring.AppendVFormat(format, args);

	if(owner)
		owner->ReportTheFailure(OpStatus::IsSuccess(op_err) ? tempstring.CStr() : format);
	else
		ST_failed(OpStatus::IsSuccess(op_err) ? tempstring.CStr() : format);

	va_end(args);
}

BOOL URL_DocSelfTest_Item::Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code)
{
	switch(event)
	{
	case URLDocST_Event_Header_Loaded:
	case URLDocST_Event_Redirect:
	case URLDocST_Event_LoadingRestarted:
		break;
	case URLDocST_Event_Data_Received:
		{
			URLStatus status = (URLStatus) url.GetAttribute(URL::KLoadStatus, TRUE);
			
			if(status != URL_LOADING)
			{
				ReportFailure("Some kind of loading failure %d.", status);
				return FALSE;
			}
		}
		break;
	case URLDocST_Event_LoadingFailed:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Failed, this);
		return FALSE;
	case URLDocST_Event_LoadingFinished:
		if(owner)
			owner->Verify_function(URLDocST_Event_Item_Finished, this);
		return FALSE;
	default:
		{
			OpString temp;
			OpString8 lang_code, errorstring;
			g_languageManager->GetString(status_code, temp);
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


URL_DocSelfTest_Batch::URL_DocSelfTest_Batch()
: manager(NULL), call_count(0), ignore_load_failures(FALSE)
{
}

void URL_DocSelfTest_Batch::Construct(MessageHandler *mh)
{
	URL_DocumentLoader::Construct(mh ? mh : g_main_message_handler);
}

URL_DocSelfTest_Batch::~URL_DocSelfTest_Batch()
{
	OP_ASSERT(call_count == 0);

	StopLoading();
	manager = NULL;

	if(InList())
		Out();
}

BOOL URL_DocSelfTest_Batch::StartLoading()
{
	call_count ++;
	URL_DocSelfTest_Item *item = (URL_DocSelfTest_Item *) loading_items.First();

	while(item)
	{
		URL_LoadPolicy policy(FALSE, URL_Load_Normal);
		OP_STATUS op_err;
		if (GetMessageHandler()==0)
			Construct(g_main_message_handler); // To avoid assert in URL_DocumentLoader while testing
		op_err = LoadDocument(item->url, item->referer, policy, item);
		
		if(OpStatus::IsError(op_err) && !ignore_load_failures)
		{
			if(op_err != OpStatus::ERR_NO_SUCH_RESOURCE)
			{
				ReportFailure("Loading batch item failed (%d).", (int) op_err);
				call_count --;

				manager->Verify_function(URLDocST_Event_Batch_Failed, this);
				return FALSE;
			}
			else
			{
				URL_DocSelfTest_Item *item2 = item;
				item = item->Suc();
				item2->Out();
				item2->Into(&loaded_items);

				if(item == NULL && loading_items.Empty())
				{
					ReportFailure("All batch items failed to load. Last code %d.", (int) op_err);
					call_count --;

					manager->Verify_function(URLDocST_Event_Batch_Failed, this);
					return FALSE;
				}

				continue;
			}
		}

		item = item->Suc();
	}
	if(loading_items.Empty())
	{
		if(!ignore_load_failures)
		{
			ReportFailure("All batch items failed to load.");
			call_count --;

			manager->Verify_function(URLDocST_Event_Batch_Failed, this);
			return FALSE;
		}
		else
		{
			manager->Verify_function(URLDocST_Event_Batch_Finished, this);
		}
	}
	call_count--;
	return TRUE;
}

BOOL URL_DocSelfTest_Batch::StopLoading()
{
	call_count ++;

	StopLoadingAll();

	call_count--;
	return TRUE;
}

BOOL URL_DocSelfTest_Batch::Finished()
{
	return loading_items.Empty();
}

BOOL URL_DocSelfTest_Batch::AddTestCase(URL_DocSelfTest_Item *item)
{
	if(!item)
		return FALSE;

	item->Into(&loading_items);
	item->owner = this;

	return TRUE;
}


BOOL URL_DocSelfTest_Batch::Verify_function(URL_DocSelfTest_Event event, URL_DocSelfTest_Item *source)
{
	if(!source)
		return TRUE;

	call_count++;
	switch(event)
	{
	case URLDocST_Event_Item_Failed : 
		if(!loading_items.HasLink(source))
		{
			call_count --;
			return TRUE;
		}
		
		source->Out();
		source->Into(&loaded_items);
		if(manager)
		{
			manager->ReportItemCompleted();
			manager->Verify_function(URLDocST_Event_Batch_Failed, this);
		}
		break;
	case URLDocST_Event_Item_Finished :
		if(!loading_items.HasLink(source))
		{
			call_count--;
			return TRUE;
		}
		
		source->Out();
		source->Into(&loaded_items);
		if(manager)
			manager->ReportItemCompleted();

		if(manager && loading_items.Empty())
		{
			output("Batch %s completed %d items\n", batch_id.CStr(), loaded_items.Cardinal());
			manager->Verify_function(URLDocST_Event_Batch_Finished, this);
		}
		break;
	}
	call_count--;
	return TRUE;
}

void URL_DocSelfTest_Batch::ReportFailure(const char *format, ...)
{
	OpString8 tempstring;
	va_list args;
	va_start(args, format);

	if(format == NULL)
		format = "";

	OP_STATUS op_err = tempstring.AppendVFormat(format, args);

	ReportTheFailure(OpStatus::IsSuccess(op_err) ? tempstring.CStr() : format);
	va_end(args);
}

void URL_DocSelfTest_Batch::ReportTheFailure(const char *report)
{
	if(report == NULL)
		report = "";

	if(manager)
		manager->ReportTheFailure(report);
	else
		ST_failed(report);

}

URL_DocSelfTest_Manager::URL_DocSelfTest_Manager()
{
	last_batch_added = FALSE;
	reported_status = FALSE;
	items_added = 0;
	items_completed = 0;

	g_main_message_handler->SetCallBack(this, MSG_URL_SELFTEST_HANDLE_DELETE, (MH_PARAM_1) this);
}

URL_DocSelfTest_Manager::~URL_DocSelfTest_Manager()
{
	g_main_message_handler->UnsetCallBacks(this);
}

BOOL URL_DocSelfTest_Manager::Verify_function(URL_DocSelfTest_Event event, URL_DocSelfTest_Batch *source)
{
	if(!source || !batch_items.HasLink(source))
		return FALSE;

	if(source != (URL_DocSelfTest_Batch *) batch_items.First())
	{
		ReportFailure("Batch loading is out of order.");
		return FALSE;
	}

	switch(event)
	{
	case URLDocST_Event_Batch_Finished:
		source->Out();
		source->manager = NULL;
		OP_ASSERT(source->Finished());
		source->Into(&delete_items);
		if(!batch_items.Empty())
		{
			DeletePendingBatches();
			return LoadFirstBatch();
		}
		last_batch_added = FALSE;
		if(!reported_status)
			ST_passed();
		reported_status = TRUE;
		break;
	case URLDocST_Event_Batch_Failed:
		delete_items.Append(&batch_items);

		last_batch_added = FALSE;
		ReportFailure("A testbatch failed.");
		DeletePendingBatches();
		return FALSE;
	}
	DeletePendingBatches();
	return TRUE;
}

void URL_DocSelfTest_Manager::DeletePendingBatches()
{

	URL_DocSelfTest_Batch *item = (URL_DocSelfTest_Batch *) delete_items.First();

	while(item)
	{
		URL_DocSelfTest_Batch *current_item = item;

		item = (URL_DocSelfTest_Batch *) item->Suc();

		if(current_item->call_count == 0)
		{
			current_item->Out();
			current_item->manager = NULL;
			OP_DELETE(current_item);
		}
	}

	if(!delete_items.Empty())
		g_main_message_handler->PostMessage(MSG_URL_SELFTEST_HANDLE_DELETE, (MH_PARAM_1) this, 0);

}

void URL_DocSelfTest_Manager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
	case MSG_URL_SELFTEST_HANDLE_DELETE: 
		DeletePendingBatches();
		break;
	}
}


BOOL URL_DocSelfTest_Manager::AddBatch(URL_DocSelfTest_Batch *item)
{
	if(!item)
		return FALSE;

	item->Into(&batch_items);
	item->manager = this;

	items_added += item->Cardinal();

	return TRUE;
}

BOOL URL_DocSelfTest_Manager::LoadFirstBatch()
{
	URL_DocSelfTest_Batch *item = (URL_DocSelfTest_Batch *) batch_items.First();
	
	if(!item || !item->StartLoading())
	{
		ReportFailure("Loading batch item failed.");
		return FALSE;
	}
	return TRUE;
}

BOOL URL_DocSelfTest_Manager::SetLastBatch()
{
	if(!last_batch_added)
	{
		last_batch_added = TRUE;
		reported_status = FALSE;
		return LoadFirstBatch();
	}
	return TRUE;
}

void URL_DocSelfTest_Manager::ReportFailure(const char *format, ...)
{
	OpString8 tempstring;
	va_list args;
	va_start(args, format);

	if(format == NULL)
		format = "";

	OP_STATUS op_err = tempstring.AppendVFormat(format, args);

	tempstring.Append("\n");

	ReportTheFailure(OpStatus::IsSuccess(op_err) ? tempstring.CStr() : format);

	va_end(args);
}

void URL_DocSelfTest_Manager::ReportTheFailure(const char *report)
{
	if(!reported_status)
		ST_failed(report);
	reported_status = TRUE;
}

OP_STATUS URL_DocSimpleTester::Construct(const OpStringC &filename)
{
	if(filename.IsEmpty())
		return OpStatus::ERR;
	return test_file.Set(filename);
}

OP_STATUS URL_DocSimpleTester::Construct(const OpStringC8 &filename)
{
	if(filename.IsEmpty())
		return OpStatus::ERR;
	return test_file.Set(filename);
}

OP_STATUS URL_DocSimpleTester::Construct(URL &a_url, URL &ref, const OpStringC &filename, BOOL unique)
{
	if(filename.IsEmpty())
		return OpStatus::ERR;
	URL_DocSelfTest_Item::Construct(a_url, ref, unique);
	return test_file.Set(filename);
}

OP_STATUS URL_DocSimpleTester::Construct(URL &a_url, URL &ref, const OpStringC8 &filename, BOOL unique)
{
	if(filename.IsEmpty())
		return OpStatus::ERR;
	URL_DocSelfTest_Item::Construct(a_url, ref, unique);
	return test_file.Set(filename);
}

OP_STATUS URL_DocSimpleTester::Construct(const OpStringC8 &source_url, URL &ref, const OpStringC8 &filename, BOOL unique)
{
	if(source_url.IsEmpty())
		return OpStatus::ERR;

	URL a_url;

	a_url = g_url_api->GetURL(source_url);
	if(a_url.IsEmpty())
		return OpStatus::ERR;

	return Construct(a_url, ref, filename, unique);
}

OP_STATUS URL_DocSimpleTester::Construct(const OpStringC8 &source_url, URL &ref, const OpStringC &filename, BOOL unique)
{
	if(source_url.IsEmpty())
		return OpStatus::ERR;

	URL a_url;

	a_url = g_url_api->GetURL(source_url);
	if(a_url.IsEmpty())
		return OpStatus::ERR;

	return Construct(a_url, ref, filename, unique);
}

OP_STATUS URL_DocSimpleTester::Construct(const OpStringC &source_url, URL &ref, const OpStringC &filename, BOOL unique)
{
	if(source_url.IsEmpty())
		return OpStatus::ERR;

	URL a_url;

	a_url = g_url_api->GetURL(source_url);
	if(a_url.IsEmpty())
		return OpStatus::ERR;

	return Construct(a_url, ref, filename, unique);
}

BOOL URL_DocSimpleTester::Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code)
{
	switch(event)
	{
	case URLDocST_Event_Header_Loaded:
		if(header_loaded)
		{
			ReportFailure("Multiple header loaded.");
			return FALSE;				
		}
		header_loaded = TRUE;
		break;
	case URLDocST_Event_Redirect:
		if(header_loaded)
		{
			ReportFailure("Redirect after header loaded.");
			return FALSE;				
		}
		break;
	case URLDocST_Event_Data_Received:
		{
			if(!header_loaded)
			{
				ReportFailure("Data before header loaded.");
				return FALSE;				
			}
			URLStatus status = (URLStatus) url.GetAttribute(URL::KLoadStatus, TRUE);
			
			if(status == URL_LOADED)
			{
				if(!CompareFileAndURL(this, url, test_file))
					return FALSE;
			}
			else if(status != URL_LOADING)
			{
				ReportFailure("Some kind of loading failure %d.", status);
				return FALSE;
			}
		}
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

OP_STATUS URL_DocVerySimpleTest::Construct(const OpStringC8 &baseurl_format, URL &ref, const OpStringC8 &filename, BOOL unique)
{
	if(baseurl_format.IsEmpty() || filename.IsEmpty())
		return OpStatus::ERR;

	OpString a_filename;
	RETURN_IF_ERROR(a_filename.Set(filename));

	OpFile a_file;

	RETURN_IF_ERROR(a_file.Construct(a_filename));

	OpString8 a_name;
	
	a_name.SetUTF8FromUTF16(a_file.GetName());

	if(a_name.IsEmpty())
		return OpStatus::ERR;

	OpString8 a_url_Str;
	RETURN_IF_ERROR(a_url_Str.AppendFormat(baseurl_format.CStr(), a_name.CStr()));

	return URL_DocSimpleTester::Construct(a_url_Str, ref, filename, unique);
}


BOOL CompareFileAndURL(URL_DocSelfTest_Item *reporter, URL &url, const OpStringC& test_file)
{
	OpFile in_file;
	OpAutoPtr<URL_DataDescriptor> desc(NULL);
	
	if(OpStatus::IsError(in_file.Construct(test_file)))
	{
		reporter->ReportFailure("Open file failed.");
		return FALSE;				
	}
	
	if(OpStatus::IsError(in_file.Open(OPFILE_READ)))
	{
		reporter->ReportFailure("Open file failed.");
		return FALSE;				
	}

	DataStream_GenericFile file(&in_file,FALSE,FALSE);
	OP_STATUS op_err;

	TRAP(op_err, file.InitL());
	if(OpStatus::IsError(op_err))
	{
		reporter->ReportFailure("Open file failed.");
		return FALSE;				
	}
	
	unsigned char *temp_buffer = (unsigned char *) g_memory_manager->GetTempBuf();
	desc.reset(url.GetDescriptor(NULL, TRUE, TRUE, TRUE));
	if(desc.get() == NULL)
	{
		if(file.MoreData())
		{
			reporter->ReportFailure("Compare failed.");
			return FALSE;				
		}
		return TRUE;
	}
	
	BOOL more=TRUE;
	OpFileLength read_len = 0;
	
	while(more)
	{
		OP_MEMORY_VAR unsigned long buf_len = desc->RetrieveData(more);
		read_len = 0;

		if(buf_len == 0)
		{
			if(file.MoreData() || more)
			{
				reporter->ReportFailure("Read file failed.");
				return FALSE;				
			}
			continue;
		}
		
		if(buf_len > g_memory_manager->GetTempBufLen())
		{
			buf_len = g_memory_manager->GetTempBufLen();
			more = TRUE;
		}
		
		read_len = 0;
		TRAP(op_err, read_len = file.ReadDataL(temp_buffer, buf_len));

		if(OpStatus::IsError(op_err))
		{
			reporter->ReportFailure("Read file failed.");
			return FALSE;				
		}

		if(read_len != (OpFileLength)buf_len)
		{
			reporter->ReportFailure("File size mismatch");
			return FALSE;				
		}
		
		if(desc->GetBuffer() == NULL || op_memcmp(desc->GetBuffer(), temp_buffer, buf_len) != 0)
		{
			reporter->ReportFailure("Compare failed.");
			return FALSE;				
		}

		desc->ConsumeData(buf_len);
	}
	read_len = 0;
	if(file.MoreData())
	{
		reporter->ReportFailure("Compare failed.");
		return FALSE;				
	}
	
	file.Close();
	desc.reset();

	OpAutoPtr<URL_DataDescriptor> desc_compare(NULL);
	OpString test_url;

	if(OpStatus::IsError(test_url.SetConcat(UNI_L("file://localhost/"), test_file)))
	{
		reporter->ReportFailure("Compare failed.");
		return FALSE;				
	}		

	URL test_source = g_url_api->GetURL(test_url.CStr());
	if(test_source.IsEmpty())
	{
		reporter->ReportFailure("Compare failed.");
		return FALSE;				
	}		

	if(!test_source.QuickLoad(TRUE))
	{
		reporter->ReportFailure("Open file failed.");
		return FALSE;				
	}

	desc_compare.reset(test_source.GetDescriptor(NULL, FALSE, TRUE, TRUE));
	if(desc_compare.get() == NULL)
	{
		reporter->ReportFailure("Open file failed.");
		return FALSE;				
	}
	
	desc.reset(url.GetDescriptor(NULL, TRUE, TRUE, TRUE));
	if(desc.get() == NULL)
	{
		if(desc_compare.get())
		{
			reporter->ReportFailure("Compare failed.");
			return FALSE;				
		}
		return TRUE;
	}
	else if(desc_compare.get() == NULL)
	{
		reporter->ReportFailure("Open file failed.");
		return FALSE;				
	}
	
	more=TRUE;
	BOOL more1=TRUE;
	
	while(more && more1)
	{
		unsigned long buf_len;
		unsigned long buf_len1;

		buf_len = desc->RetrieveData(more);
		buf_len1 = desc_compare->RetrieveData(more1);

		unsigned long consume_len;


		if(buf_len == 0)
		{
			if(buf_len1 || more)
			{
				reporter->ReportFailure("Read file failed.");
				return FALSE;				
			}
			continue;
		}
		else if(!buf_len1 || desc_compare->GetBuffer() == NULL)
		{
			reporter->ReportFailure("Read file failed.");
			return FALSE;				
		}

		consume_len = MIN(buf_len, buf_len1);

		if(desc->GetBuffer() == NULL || desc_compare->GetBuffer() == NULL || op_memcmp(desc->GetBuffer(), desc_compare->GetBuffer(), consume_len) != 0)
		{
			reporter->ReportFailure("Compare failed.");
			return FALSE;				
		}

		desc->ConsumeData(consume_len);
		more = more || (consume_len < buf_len);
		desc_compare->ConsumeData(consume_len);
		more1 = more1 || (consume_len < buf_len1);
	}
	if(!desc->FinishedLoading() || !desc_compare->FinishedLoading())
	{
		reporter->ReportFailure("Compare failed.");
		return FALSE;				
	}

	desc_compare.reset();
	desc.reset();

#ifdef _URL_STREAM_ENABLED_
	URL_DataStream url_stream(url, TRUE, TRUE);

	desc_compare.reset(test_source.GetDescriptor(NULL, FALSE, TRUE, TRUE));
	if(desc_compare.get() == NULL)
	{
		reporter->ReportFailure("Open file failed.");
		return FALSE;				
	}
	
	more1=TRUE;
	int count = 0;
	
	while(url_stream.MoreData() && more1)
	{
		unsigned long buf_len=0;
		unsigned long buf_len1=0;

		TRAPD(op_err, buf_len = url_stream.ReadDataL(temp_buffer, g_memory_manager->GetTempBufLen()));
		if(OpStatus::IsError(op_err))
		{
			reporter->ReportFailure("Read file failed.");
			return FALSE;				
		}

		buf_len1 = desc_compare->RetrieveData(more1);

		unsigned long consume_len;

		if(buf_len == 0)
		{
			if(buf_len1 || more)
			{
				reporter->ReportFailure("Read file failed.");
				return FALSE;				
			}
			continue;
		}
		else if(!buf_len1 || desc_compare->GetBuffer() == NULL)
		{
			reporter->ReportFailure("Read file failed.");
			return FALSE;				
		}

		consume_len = MIN(buf_len, buf_len1);

		if(desc_compare->GetBuffer() == NULL || op_memcmp(temp_buffer, desc_compare->GetBuffer(), consume_len) != 0)
		{
			reporter->ReportFailure("Compare failed.");
			return FALSE;				
		}

		desc_compare->ConsumeData(consume_len);
		count ++;
	}
	if(!url_stream.MoreData() || !desc_compare->FinishedLoading())
	{
		reporter->ReportFailure("Compare failed.");
		return FALSE;				
	}

	desc_compare.reset();
#endif // _URL_STREAM_ENABLED_

	return TRUE;
}

BOOL CompareURLAndURL(URL_DocSelfTest_Item *reporter, URL &url, URL& test_source)
{
	OpAutoPtr<URL_DataDescriptor> desc(NULL);
	OpAutoPtr<URL_DataDescriptor> desc_compare(NULL);

	if(test_source.IsEmpty())
	{
		reporter->ReportFailure("Compare failed.");
		return FALSE;				
	}		

	if(test_source.GetAttribute(URL::KLoadStatus,URL::KFollowRedirect) != URL_LOADED)
	{
		reporter->ReportFailure("Compare URL loading failed.");
		return FALSE;				
	}

	desc_compare.reset(test_source.GetDescriptor(NULL, FALSE, TRUE, TRUE));
	desc.reset(url.GetDescriptor(NULL, TRUE, TRUE, TRUE));

	if(desc.get() == NULL)
	{
		if(desc_compare.get())
		{
			reporter->ReportFailure("Open descriptor failed.");
			return FALSE;				
		}
		return TRUE;
	}
	else if(desc_compare.get() == NULL)
	{
		reporter->ReportFailure("Open match descriptor failed.");
		return FALSE;				
	}
	
	BOOL more=TRUE;
	BOOL more1=TRUE;
	
	while(more && more1)
	{
		unsigned long buf_len;
		unsigned long buf_len1;

		buf_len = desc->RetrieveData(more);
		buf_len1 = desc_compare->RetrieveData(more1);

		unsigned long consume_len;


		if(buf_len == 0)
		{
			if(buf_len1 || more)
			{
				reporter->ReportFailure("Read file failed.");
				return FALSE;				
			}
			continue;
		}
		else if(!buf_len1 || desc_compare->GetBuffer() == NULL)
		{
			reporter->ReportFailure("Read file failed.");
			return FALSE;				
		}

		consume_len = MIN(buf_len, buf_len1);

		if(desc->GetBuffer() == NULL || desc_compare->GetBuffer() == NULL || op_memcmp(desc->GetBuffer(), desc_compare->GetBuffer(), consume_len) != 0)
		{
			reporter->ReportFailure("Compare failed.");
			return FALSE;				
		}

		desc->ConsumeData(consume_len);
		more = more || (consume_len < buf_len);
		desc_compare->ConsumeData(consume_len);
		more1 = more1 || (consume_len < buf_len1);
	}
	if(!desc->FinishedLoading() || !desc_compare->FinishedLoading())
	{
		reporter->ReportFailure("Compare failed.");
		return FALSE;				
	}

	desc_compare.reset();
	desc.reset();

	return TRUE;
}

#endif // SELFTEST
