/*
 * self_test_storage.h
 *
 *  Created on: Dec 8, 2009
 *      Author: hmolland
 */
#ifndef SELF_TEST_STORAGE_H_
#define SELF_TEST_STORAGE_H_

#if defined(SELFTEST) && defined(APPLICATION_CACHE_SUPPORT)

#include "modules/ecmascript_utils/esthread.h"
#include "modules/webserver/webserver-api.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esasyncif.h"

class SelftestAppCacheListener : public OpApplicationCacheListener
{
public:

	SelftestAppCacheListener() : m_selftest_window_id(0)
	{
	}
	
	virtual ~SelftestAppCacheListener()
	{
	}


	template <class callback> 
	struct CallbackBundle
	{
		CallbackBundle(OpWindowCommander *window_commander, UINT32 id, callback *m_callback)
		: m_id(id)
		, m_commander(window_commander)
		, m_callback(m_callback)
		 {}		
		
		~CallbackBundle(){}
		
		UINT32 m_id;
		OpWindowCommander* m_commander;
		callback *m_callback;
	};
	
	unsigned int m_selftest_window_id;

	OpAutoINT32HashTable< CallbackBundle<InstallAppCacheCallback> > m_pending_install_callbacks;	
	OpAutoINT32HashTable< CallbackBundle<QuotaCallback> > m_pending_quota_callbacks;	
	
	OP_STATUS AddCallbackIns(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback)
	{
		CallbackBundle<InstallAppCacheCallback> *removed_bundle;
		if (OpStatus::IsSuccess(m_pending_install_callbacks.Remove(id, &removed_bundle)))
			OP_DELETE(removed_bundle);
		
		
		OpAutoPtr< CallbackBundle<InstallAppCacheCallback> > callback_bundle(OP_NEW(CallbackBundle<InstallAppCacheCallback>, (commander, id, callback)));
		if (!callback_bundle.get())
			return OpStatus::ERR_NO_MEMORY;
		
		OP_STATUS status;
		if (OpStatus::IsSuccess(status = m_pending_install_callbacks.Add((INT32)id, callback_bundle.get())))
		{
			callback_bundle.release();
			return OpStatus::OK;
		}
		OP_ASSERT(!"callback added twice");
		return OpStatus::ERR;	
	}

	OP_STATUS AddCallbackQuota(OpWindowCommander* commander, UINTPTR id, QuotaCallback* callback)
	{
		CallbackBundle<QuotaCallback> *removed_bundle;		
		if (OpStatus::IsSuccess(m_pending_quota_callbacks.Remove(id, &removed_bundle)))
			OP_DELETE(removed_bundle);
		
		OpAutoPtr< CallbackBundle<QuotaCallback> > callback_bundle(OP_NEW(CallbackBundle<QuotaCallback>,(commander, id, callback)));
		OP_STATUS status;
		
		if (OpStatus::IsSuccess(status = m_pending_quota_callbacks.Add((INT32)id, callback_bundle.get())))
		{
			callback_bundle.release();
			return OpStatus::OK;
		}
		OP_ASSERT(!"callback added twice");
		return OpStatus::ERR;	
	}

	
	ES_Object *GetApplicationCacheObject(OpWindowCommander* commander)
	{
		WindowCommander *wc = static_cast<WindowCommander*>(commander);
		
		Window *window =  wc->ViolateCoreEncapsulationAndGetWindow();
		if (window == NULL)
			return NULL;
		
		FramesDocument *frm_doc = window->GetCurrentDoc();
		if (frm_doc == NULL)
			return NULL;
 
		DOM_Environment * environment = frm_doc->GetDOMEnvironment();
		if (environment == NULL)
			return NULL;

		return environment->GetApplicationCacheObject();
	}

	ES_Runtime *GetRuntime(OpWindowCommander* commander)
	{
		WindowCommander *wc = static_cast<WindowCommander*>(commander);
		
		Window *window =  wc->ViolateCoreEncapsulationAndGetWindow();
		if (window == NULL)
			return NULL;
		
		FramesDocument *frm_doc = window->GetCurrentDoc();
		if (frm_doc == NULL)
			return NULL;
 
		return frm_doc->GetESRuntime();
	}

	ES_AsyncInterface *GetAsyncInterface(OpWindowCommander* commander)
	{
		WindowCommander *wc = static_cast<WindowCommander*>(commander);
		
		Window *window =  wc->ViolateCoreEncapsulationAndGetWindow();
		if (window == NULL)
			return NULL;
		
		FramesDocument *frm_doc = window->GetCurrentDoc();
		if (frm_doc == NULL)
			return NULL;
 
		return frm_doc->GetESAsyncInterface();
	}

	ES_Thread *GetCurrentThread(OpWindowCommander* commander)
	{
		WindowCommander *wc = static_cast<WindowCommander*>(commander);
		
		Window *window =  wc->ViolateCoreEncapsulationAndGetWindow();
		if (window == NULL)
			return NULL;
		
		FramesDocument *frm_doc = window->GetCurrentDoc();
		if (frm_doc == NULL)
			return NULL;
 
		ES_ThreadScheduler *scheduler = frm_doc->GetESScheduler();

		if (scheduler->IsActive())
			return scheduler->GetCurrentThread();
		else
			return NULL;
	}

	void SendUIEvent(OpWindowCommander* commander, const uni_char *function, UINT32 id = (UINT32)-1)
	{
		ES_Object *applicationCache = GetApplicationCacheObject(commander);
		ES_Runtime *runtime = GetRuntime(commander);
		
		ES_Value value;
		OP_BOOLEAN exists = runtime->GetName(applicationCache, function, &value);
		if (exists == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
		{
			int argc = 0;
			ES_Value argument[1];
			if (id != (UINT32)-1)
			{
				argc = 1;
				argument[0].type = VALUE_NUMBER;
				argument[0].value.number = id;
			}

			OpStatus::Ignore(GetAsyncInterface(commander)->CallFunction(value.value.object, applicationCache, argc, argument, NULL, GetCurrentThread(commander)));
		}
	}

	virtual void OnDownloadAppCache(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback) 
	{ 
		AddCallbackIns(commander, id, callback);
		SendUIEvent(commander, UNI_L("onDownloadAppCache"), id);
	}
	
	virtual void CancelDownloadAppCache(OpWindowCommander* commander, UINTPTR id) 
	{
		SendUIEvent(commander, UNI_L("cancelDownloadAppCache"), id);
	}
	
	virtual void OnCheckForNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback) 
	{ 
		AddCallbackIns(commander, id, callback);
		SendUIEvent(commander, UNI_L("onCheckForNewAppCacheVersion"), id);
	}
	
	virtual void CancelCheckForNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id) 
	{
		SendUIEvent(commander, UNI_L("cancelCheckForNewAppCacheVersion"), id);
	}
	virtual void OnIncreaseAppCacheQuota(OpWindowCommander* commander, UINTPTR id, const uni_char* cache_domain, OpFileLength current_quota_size, QuotaCallback *callback) 
	{ 
		AddCallbackQuota(commander, id, callback);
		SendUIEvent(commander, UNI_L("onIncreaseAppCacheQuota"), id);
	}
	
	virtual void CancelIncreaseAppCacheQuota(OpWindowCommander* commander, UINTPTR id)
	{
		SendUIEvent(commander, UNI_L("cancelIncreaseAppCacheQuota"), id);
	}
	
	virtual void OnDownloadNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback)
	{
		AddCallbackIns(commander, id, callback);
		SendUIEvent(commander, UNI_L("onDownloadNewAppCacheVersion"), id);
	}

	virtual void CancelDownloadNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id)
	{
		SendUIEvent(commander, UNI_L("cancelDownloadNewAppCacheVersion"), id);
	}
	
	virtual void OnAppCacheChecking(OpWindowCommander* commander)
	{
		SendUIEvent(commander, UNI_L("onAppCacheCheckingUI"));
	}
	
	virtual void OnAppCacheError(OpWindowCommander* commander)
	{
		SendUIEvent(commander, UNI_L("onAppCacheErrorUI"));
	}

	virtual void OnAppCacheNoUpdate(OpWindowCommander* commander)
	{
		SendUIEvent(commander, UNI_L("onAppCacheNoUpdateUI"));
	}

	virtual void OnAppCacheDownloading(OpWindowCommander* commander)
	{
		SendUIEvent(commander, UNI_L("onAppCacheDownloadingUI"));
	}

	virtual void OnAppCacheProgress(OpWindowCommander* commander)
	{
		SendUIEvent(commander, UNI_L("onAppCacheProgressUI"));
	}

	virtual void OnAppCacheUpdateReady(OpWindowCommander* commander)
	{
		SendUIEvent(commander, UNI_L("onAppCacheUpdateReadyUI"));
	}

	virtual void OnAppCacheCached(OpWindowCommander* commander)
	{
		SendUIEvent(commander, UNI_L("onAppCacheCachedUI"));
	}

	virtual void OnAppCacheObsolete(OpWindowCommander* commander)
	{
		SendUIEvent(commander, UNI_L("onAppCacheObsoleteUI"));
	}


	
	void OnDownloadAppCacheReply(UINT32 id, BOOL install)
	{
		CallbackBundle<InstallAppCacheCallback> *removed_bundle;		
		if (OpStatus::IsSuccess(m_pending_install_callbacks.Remove(id, &removed_bundle)))
		{
			removed_bundle->m_callback->OnDownloadAppCacheReply(install);
			OP_DELETE(removed_bundle);
		}
	}
	
	void OnCheckForNewAppCacheVersionReply(UINT32 id, BOOL check_for_update)
	{
		CallbackBundle<InstallAppCacheCallback> *removed_bundle;		
		if (OpStatus::IsSuccess(m_pending_install_callbacks.Remove(id, &removed_bundle)))
		{
			removed_bundle->m_callback->OnCheckForNewAppCacheVersionReply(check_for_update);
			OP_DELETE(removed_bundle);
		}
	}
	
	void OnDownloadNewAppCacheVersionReply(UINT32 id, BOOL update)
	{
		CallbackBundle<InstallAppCacheCallback> *removed_bundle;		
		if (OpStatus::IsSuccess(m_pending_install_callbacks.Remove(id, &removed_bundle)))
		{
			removed_bundle->m_callback->OnDownloadNewAppCacheVersionReply(update);
			OP_DELETE(removed_bundle);
		}		
	}
	
	void OnQuotaReply(UINT32 id, BOOL alow, UINT32 size)
	{
		CallbackBundle<QuotaCallback> *removed_bundle;		
		if (OpStatus::IsSuccess(m_pending_quota_callbacks.Remove(id, &removed_bundle)))
		{
			removed_bundle->m_callback->OnQuotaReply(alow, size);
			OP_DELETE(removed_bundle);
		}
	}

	unsigned int GetCurrentWindowId(){ return m_selftest_window_id; }

	void SetCurrentWindowId(unsigned int window_id){ m_selftest_window_id = window_id; }
};

#endif // SELFTEST && APPLICATION_CACHE_SUPPORT

#endif /* SELF_TEST_STORAGE_H_ */
