/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2006 Opera Software ASA.  All rights reserved.
**
	** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve N. Pettersen
*/

#ifndef __LONGBATCH_TLS_H__
#define __LONGBATCH_TLS_H__

#include "modules/url/url_man.h"

#include "modules/util/adt/opvector.h"

#include "modules/network_selftest/urldoctestman.h"
#include "modules/network_selftest/justload.h"
#include "modules/network_selftest/remote_framework.h"

#include "modules/locale/oplanguagemanager.h"

#include "modules/olddebug/tstdump.h"

class NeverFail_Manager : public URL_DocSelfTest_Manager
{
private:
	unsigned int completed;
	unsigned int completed_successfully;
	unsigned int completed_unsuccessfully;

public:
	NeverFail_Manager():completed(0),completed_successfully(0),completed_unsuccessfully(0){}

	void RegisterCompleted(BOOL success, URL &url, const OpStringC &reason=OpStringC())
	{
		completed += 1;
		if(success)
			completed_successfully += 1;
		else
		{
			completed_unsuccessfully += 1;
			OpString8 url_str;
			url.GetAttribute(URL::KName,url_str);
			OpStringC error=url.GetAttribute(URL::KInternalErrorString);
			OpString8 error8;
			OpStringC empty(UNI_L(""));
			error8.SetUTF8FromUTF16(error.HasContent() ? error : empty);
			if(error8.IsEmpty())
				error8.SetUTF8FromUTF16(reason.HasContent() ? reason : empty);

			output("Failed URL:%s\n%s\n------------\n",url_str.CStr(),error8.CStr());

			PrintfTofile("manyload_fails.txt", "Failed URL:%s\n%s\n------------\n",url_str.CStr(),error8.CStr());
		}

		if( completed %100 == 0)
		{
			output("Loaded %d URL (Total %d/%d)\n",GetCompleted(), items_completed, items_added);
			output("Completed successfully %d\n",GetCompletedSuccess());
			output("Completed unsuccessfully %d\n",GetCompletedNoSuccess());

			output("\nCompleted successfully %7.2f%%\n",((double)GetCompletedSuccess()/(double) GetCompleted())*100.0);
			output("==================\n");
		}
	}

	unsigned int GetCompleted(){return completed;}
	unsigned int GetCompletedSuccess(){return completed_successfully;}
	unsigned int GetCompletedNoSuccess(){return completed_unsuccessfully;}
};

class NeverFail_Batch : public URL_DocSelfTest_Batch
{
private:
	OpVector<OpString8> str_batch_list;

public:
	virtual BOOL Verify_function(URL_DocSelfTest_Event event, URL_DocSelfTest_Item *source)
	{
		if(source)
		{
			source->url_use->Unload();
			source->url_use.UnsetURL();
		}
		return URL_DocSelfTest_Batch::Verify_function(URLDocST_Event_Item_Finished, source);
	}

	virtual BOOL SetUpBatchEntry(const OpStringC8 &url_str);

	virtual BOOL StartLoading()
	{
		urlManager->RemoveSensitiveData();

		UINT32 n = str_batch_list.GetCount();
		if(n)
		{
			UINT32 i;
			for(i = 0; i < n; i++)
			{
				if (!SetUpBatchEntry(*str_batch_list.Get(i)))
					return FALSE;
			}
			str_batch_list.Empty();
		}

		return URL_DocSelfTest_Batch::StartLoading();
	}

	virtual OP_STATUS AddWorkURL(const OpStringC8 &url_str)
	{
		OpString8 *new_url_str = OP_NEW(OpString8, ());

		RETURN_OOM_IF_NULL(new_url_str);
		RETURN_IF_ERROR(new_url_str->Set(url_str));

		return str_batch_list.Add(new_url_str);
	}

	virtual BOOL Empty(){return  str_batch_list.GetCount() == 0 && URL_DocSelfTest_Batch::Empty();}
	virtual int Cardinal(){return (str_batch_list.GetCount()== 0 ? URL_DocSelfTest_Batch::Cardinal() :  static_cast<int>(str_batch_list.GetCount()));}
};

class NeverFailLoad_Tester : public JustLoad_Tester
{
private:
	BOOL reported;
	BOOL singular;
public:
	NeverFailLoad_Tester():reported(FALSE),singular(FALSE){};
	NeverFailLoad_Tester(URL &a_url, URL &ref, BOOL unique=TRUE) : JustLoad_Tester(a_url, ref, unique), reported(FALSE),singular(FALSE){};
	virtual ~NeverFailLoad_Tester(){}

	void SetSingular(){singular=TRUE;}

	virtual void RegisterCompleted(BOOL success, URL &url, const OpStringC &reason=OpStringC())
	{
		if(success || singular)
		{
			((NeverFail_Manager*)(GetBatch()->GetManager()))->RegisterCompleted(success, url,reason);
			return;
		}

		OpAutoPtr<NeverFail_Batch> batch(OP_NEW(NeverFail_Batch, ()));
		if(batch.get() ==  NULL)
		{
			((NeverFail_Manager*)(GetBatch()->GetManager()))->RegisterCompleted(success, url,reason);
			return;
		}

		batch->Construct(GetBatch()->GetMessageHandler());
		batch->SetTimeOutIdle(30);
		batch->SetTimeOut(150);
		batch->SetTimeOutStartConnection(TRUE);

		URL ref_url;
		OpString8 urlstr;

		url.GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, urlstr);

		OpAutoPtr<NeverFailLoad_Tester> test(OP_NEW(NeverFailLoad_Tester, ()));
		if(test.get() !=  NULL)
			test->SetSingular();
		if(urlstr.IsEmpty() || test.get() ==  NULL || OpStatus::IsError(test->Construct(urlstr, ref_url, TRUE)) || !batch->AddTestCase(test.release()))
		{
			((NeverFail_Manager*)(GetBatch()->GetManager()))->RegisterCompleted(success, url,reason);
			return;
		}

		url.GetServerName()->RemoveSensitiveData();

		batch->SetManager(GetBatch()->GetManager());
		batch->Follow(GetBatch());
		batch.release();
	}

	virtual BOOL Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code=Str::NOT_A_STRING)
	{
		// Always continue
		switch(event)
		{
		case URLDocST_Event_Header_Loaded:
			header_loaded = TRUE;
			break;
		case URLDocST_Event_Redirect:
			return TRUE;
			break;
		case URLDocST_Event_Data_Received:
			return TRUE;
			break;
		default:
			{
				// increment failure count
				if(!reported)
				{
					reported = TRUE;
					OpString str;
					OpString str2;
					if(status_code!=Str::NOT_A_STRING)
						g_languageManager->GetString(status_code, str2);
					str2.Append(UNI_L("\nVerify_function"));

					str.AppendFormat(UNI_L("Event %d reported (response %d): %s"), (int) event, url.GetAttribute(URL::KHTTP_Response_Code), (str2.CStr() ? str2.CStr() : UNI_L("")));
					if(url.GetAttribute(g_KSSLHandshakeSent))
						RegisterCompleted((url.GetAttribute(URL::KHTTP_Response_Code)>0 || url.GetAttribute(g_KSSLHandshakeCompleted)), url,str);
				}
				return FALSE;
			}
		}
		return TRUE;
	}
	BOOL OnURLRedirected(URL &url, URL &redirected_to)
	{
		if(!reported)
		{
			reported = TRUE;
			RegisterCompleted(TRUE, url);
		}
		GetBatch()->Verify_function(URLDocST_Event_Item_Finished, this);
		return FALSE;
	}
	BOOL OnURLDataLoaded(URL &url, BOOL finished, OpAutoPtr<URL_DataDescriptor> &stored_desc)
	{
		URLStatus status = (URLStatus) url.GetAttribute(URL::KLoadStatus, TRUE);

		if(status == URL_LOADED)
		{
			if(!reported)
			{
				reported = TRUE;
				RegisterCompleted(TRUE, url);
				if(GetBatch())
					GetBatch()->Verify_function(URLDocST_Event_Item_Finished, this);
			}
			return FALSE;
		}
		else if(status != URL_LOADING)
		{
			// increment failure count
			if(!reported)
			{
				reported = TRUE;
				if(url.GetAttribute(g_KSSLHandshakeSent))
				{
					int response = url.GetAttribute(URL::KHTTP_Response_Code);
					RegisterCompleted(((response>0 && response<400) || response == 404 || response == 401 || response == 407), url, UNI_L("Loading failed for some reason"));
				}
				if(GetBatch())
					GetBatch()->Verify_function(URLDocST_Event_Item_Finished, this);
				return FALSE;
			}
		}
		return (finished ? FALSE : TRUE);
	}
	void OnURLLoadingFailed(URL &url, Str::LocaleString status_code, OpAutoPtr<URL_DataDescriptor> &stored_desc)
	{
		if(!reported)
		{
			reported = TRUE;
			if(url.GetAttribute(g_KSSLHandshakeSent))
			{
				if(!url.GetAttribute(g_KSSLHandshakeCompleted))
				{
					OpString str2;
					if(status_code!=Str::NOT_A_STRING)
						g_languageManager->GetString(status_code, str2);
					str2.AppendFormat(UNI_L("\nOnURLLoadingFailed%d"),(unsigned int)status_code);
					RegisterCompleted(url.GetAttribute(URL::KHTTP_Response_Code)>0, url, str2);
				}
				else
					RegisterCompleted(TRUE, url);
			}
			if(GetBatch())
				GetBatch()->Verify_function(URLDocST_Event_Item_Finished, this);
		}
	}
};

class NeverFail_RemoteFrameworkManager : public RemoteFrameworkManager
{
public:
	class TextFile : public RemoteFrameworkManager::RemoteTextMaster
	{
	private:
		OpAutoPtr<NeverFail_Batch> batch;

	public:
		TextFile(RemoteFrameworkManager *mgr):RemoteTextMaster(mgr) {}

	protected:
		virtual OP_STATUS ProcessFile()
		{
			RETURN_IF_ERROR(RemoteFrameworkManager::RemoteTextMaster::ProcessFile());

			if(batch.get() && batch->Cardinal())
				GetManager()->AddTestBatch(batch.release());

			return  OpStatus::OK;
		}
		virtual OP_STATUS HandleTextLine(const OpStringC8 &line)
		{
			if(line.IsEmpty())
				return OpStatus::OK;

			if (batch.get() == NULL)
			{
				batch.reset(OP_NEW(NeverFail_Batch, ()));
				RETURN_OOM_IF_NULL(batch.get());
				batch->Construct(GetManager()->GetMessageHandler());
				batch->SetTimeOutIdle(30);
				batch->SetTimeOut(150);
				batch->SetTimeOutStartConnection(TRUE);
			}

			RETURN_IF_ERROR(batch->AddWorkURL(line));

			if(batch->Cardinal()>=40)
				GetManager()->AddTestBatch(batch.release());

			return  OpStatus::OK;
		}
	};

	virtual AutoFetch_Element *ProduceTestSuiteMaster(URL &url, OP_STATUS &op_err)
	{
		op_err = OpStatus::OK;
		OpAutoPtr<TextFile> loader(OP_NEW(TextFile, (this)));

		if(!loader.get())
		{
			op_err = OpStatus::ERR_NO_MEMORY;
			return NULL;
		}

		op_err = loader->Construct(url);
		if(OpStatus::IsError(op_err))
			loader.reset();

		return loader.release();
	}

	virtual URL_DocSelfTest_Manager *ProduceTestManager(){return OP_NEW(NeverFail_Manager,());}

	NeverFail_Manager *GetTestManager(){return (NeverFail_Manager *) RemoteFrameworkManager::GetTestManager();}
};

inline BOOL NeverFail_Batch::SetUpBatchEntry(const OpStringC8 &url_str)
{
	URL ref_url;

	OpAutoPtr<NeverFailLoad_Tester> test(OP_NEW(NeverFailLoad_Tester, ()));
	if(test.get() == NULL)
		return FALSE;

	RETURN_VALUE_IF_ERROR(test->Construct(url_str, ref_url, TRUE), FALSE);
	test->url.SetAttribute(URL::KHTTP_Method, HTTP_METHOD_HEAD);

	if(!AddTestCase(test.release()))
		return FALSE;

	return TRUE;
}


#endif
