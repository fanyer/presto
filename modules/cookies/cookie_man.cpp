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

// Url_cm.cpp

// URL Cookie Management

#include "core/pch.h"

#include "modules/url/url_man.h"
#include "modules/url/url_enum.h"
#include "modules/url/url_module.h"
#include "modules/cookies/url_cm.h"
#include "modules/cookies/cookie_common.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
 
Cookie_Manager::Cookie_Manager()
{
	InternalInit();
}

void Cookie_Manager::InternalInit()
{
	context_id = 0;
#ifdef COOKIES_CONTROL_PER_CONTEXT
	cookies_enabled_for_context = TRUE;
#endif
#ifdef DISK_COOKIES_SUPPORT
	cookie_file_read = FALSE;
#endif
	cookie_root = NULL;
#ifdef _ASK_COOKIE
	processing_cookies = FALSE;
#endif
	cookie_temp_buffer_len = 0;
	cookie_processing_buf1 = NULL;
	cookie_processing_buf2 = NULL;
	max_cookies = MAX_TOTAL_COOKIES;
	cookies_count = 0;
	max_cookies_in_domain = 60;
	max_cookie_len = COOKIE_MAX_LEN;
#ifdef DEBUG_COOKIE_MEMORY_SIZE
	cookie_mem_size = 0;
#endif
	updated_cookies = FALSE;
	cookie_file_location = OPFILE_HOME_FOLDER;
}

void Cookie_Manager::InitL(
						   OpFileFolder loc, 
							URL_CONTEXT_ID a_id
						   )
{
	context_id = a_id;
	cookie_file_location = loc;
	cookie_root = CookieDomain::CreateL(NULL);
	LEAVE_IF_ERROR(CheckCookieTempBuffers(128));

	// The following initialization applies only to g_cookieManager. However,
	// there are lots of CookieContextItems with their own Cookie_Manager.
	//
	// TODO: At a later time, Cookie_Manager should be split into one singleton
	// API object and several Context_Cookie_Manager's that handle cookies for
	// each cache context. See CORE-35784
	if (this != g_cookieManager)
		return;

	message_handler.InitL();

	// Cookie preference normalization
	COOKIE_MODES cmode = (COOKIE_MODES) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled);
	COOKIE_MODES prevmode  = cmode;

	switch(prevmode)
	{
	case COOKIE_NONE:
	case COOKIE_NO_THIRD_PARTY:
	case COOKIE_SEND_NOT_ACCEPT_3P:
	case COOKIE_ALL:
	case COOKIE_WARN_THIRD_PARTY:
		break;
	case COOKIE_SEND_NOT_ACCEPT_3P_old:
		cmode = COOKIE_SEND_NOT_ACCEPT_3P;
		break;
	case COOKIE_ONLY_SERVER_deprecated:
		cmode = COOKIE_NO_THIRD_PARTY;
		break;
	case COOKIE_ONLY_SELECTED_SERVERS_N3P_deprecated:
	case COOKIE_ONLY_SELECTED_SERVERS_OS_deprecated:
	case COOKIE_ONLY_SELECTED_SERVERS_ALL_deprecated:
	case COOKIE_ONLY_SELECTED_SERVERS_W3P_deprecated:
		cmode = COOKIE_NONE;
		break;
	}
	if(prevmode != cmode)
	{
#ifdef PREFS_WRITE
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CookiesEnabled, cmode);
#else
		OP_ASSERT(!"Initial Cookie preference was incorrect. Because it is not possible to correct the preference setting, cookies may not work correctly.");
#endif
	}
}


Cookie_Manager::~Cookie_Manager()
{
	InternalDestruct();
}

void Cookie_Manager::InternalDestruct()
{
	OP_DELETEA(cookie_processing_buf1);
	cookie_processing_buf1 = NULL;
	OP_DELETEA(cookie_processing_buf2);
	cookie_processing_buf2 = NULL;
	cookie_temp_buffer_len = 0;
	OP_ASSERT(unprocessed_cookies.Empty()); // Not called PreDestructStep()
	OP_ASSERT(cookie_root== NULL); // Not called PreDestructStep()
}

void Cookie_Manager::PreDestructStep()
{
	CookieContextItem *temp_manager = (CookieContextItem *) ContextManagers.First();

	while(temp_manager)
	{
		CookieContextItem *manager = temp_manager;
		temp_manager = (CookieContextItem *) manager->Suc();

		if(manager->context)
		{
			manager->references++;
			manager->PreDestructStep();
			manager->references--;
		}
		manager->Out();
		OP_DELETE(manager);
	}

	unprocessed_cookies.Clear();
	OP_DELETE(cookie_root);
	cookie_root = NULL;
}


OP_STATUS Cookie_Manager::CheckCookieTempBuffers(size_t checksize)
{
	if(cookie_temp_buffer_len < checksize)
	{

		unsigned int t_temp_buffer_len = cookie_temp_buffer_len ;
		char *ct_temp_buffer1 = cookie_processing_buf1;
		char *ct_temp_buffer2 = cookie_processing_buf2;
		
		cookie_temp_buffer_len  = (checksize | 0xff) +1;
		cookie_processing_buf1 = OP_NEWA(char, cookie_temp_buffer_len);
		cookie_processing_buf2 = OP_NEWA(char, cookie_temp_buffer_len);
		
		if(cookie_processing_buf1 == NULL || cookie_processing_buf2 == NULL)
		{
			OP_DELETEA(cookie_processing_buf1);
			OP_DELETEA(cookie_processing_buf2);
			
			cookie_temp_buffer_len  = t_temp_buffer_len;
			cookie_processing_buf1 = ct_temp_buffer1;
			cookie_processing_buf1 = ct_temp_buffer2;
			
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			
			return OpStatus::ERR_NO_MEMORY;
		}

		OP_DELETEA(ct_temp_buffer1);
		OP_DELETEA(ct_temp_buffer2);
	}


	if(cookie_processing_buf1 == NULL || cookie_processing_buf2== NULL)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;

}


void Cookie_Manager::AddContextL(URL_CONTEXT_ID id,
								 OpFileFolder loc,
								 BOOL share_with_main_context
								 )
{
	CookieContextItem *ctx = FindContext(id);
	if(ctx)
	{
		ctx->references++;
		return;
	}

	OpStackAutoPtr<CookieContextItem> item(OP_NEW_L(CookieContextItem, ()));
	item->ConstructL(id, loc, share_with_main_context);
	item->context->SetMaxTotalCookies(CONTEXT_MAX_TOTAL_COOKIES);
	item.release()->Into(&ContextManagers);
}

void Cookie_Manager::RemoveContext(URL_CONTEXT_ID id)
{
	DecrementContextReference(id);
}

BOOL Cookie_Manager::ContextExists(URL_CONTEXT_ID id)
{
	return (FindContext(id) != NULL ? TRUE : FALSE);
}

Cookie_Manager::CookieContextItem *Cookie_Manager::FindContext(URL_CONTEXT_ID id)
{
	CookieContextItem *item = (CookieContextItem*) ContextManagers.First();

	while(item)
	{
		if(item->context_id == id)
			return item;

		item = (CookieContextItem *) item->Suc();
	}

	return NULL;
}

void Cookie_Manager::IncrementContextReference(URL_CONTEXT_ID context_id)
{
	CookieContextItem *ctx = FindContext(context_id);

	if(ctx)
		ctx->references ++;
}

void Cookie_Manager::DecrementContextReference(URL_CONTEXT_ID context_id)
{
	CookieContextItem *ctx = FindContext(context_id);

	if(ctx)
	{
		if(ctx->references)
			ctx->references --;

		if(!ctx->references)
		{
			ctx->PreDestructStep();
			OP_DELETE(ctx);
		}
	}

}

BOOL Cookie_Manager::GetContextIsTemporary(URL_CONTEXT_ID id)
{
	if(id == 0)
		return FALSE;

	CookieContextItem *ctx = FindContext(id);

	if(ctx)
	{
		return ctx->share_with_main_context;
	}
	return FALSE;
}

#ifdef COOKIES_CONTROL_PER_CONTEXT
void Cookie_Manager::SetCookiesEnabledForContext(URL_CONTEXT_ID id, BOOL flag)
{
	if(id == 0)
		return;

	CookieContextItem *ctx = FindContext(id);

	if(ctx)
	{
		ctx->context->cookies_enabled_for_context = flag;
	}
}

BOOL Cookie_Manager::GetCookiesEnabledForContext(URL_CONTEXT_ID id)
{
	if(id == 0)
		return TRUE;

	CookieContextItem *ctx = FindContext(id);

	if(ctx)
	{
		return ctx->context->cookies_enabled_for_context;
	}
	return FALSE;
}
#endif

#ifdef WIC_COOKIES_LISTENER
void Cookie_Manager::IterateAllCookies()
{
	CookieContextItem* manager = (CookieContextItem *) ContextManagers.First();

	while(manager)
	{
		if(manager->context)
		{
			manager->context->cookie_root->IterateAllCookies();
		}
		manager = (CookieContextItem *) manager->Suc();
	}
	cookie_root->IterateAllCookies();
}
#endif // WIC_COOKIES_LISTENER

#ifdef COOKIE_MANUAL_MANAGEMENT
void Cookie_Manager::RequestSavingCookies()
{
	g_main_message_handler->PostMessage(MSG_SAVE_COOKIES,0,0);
}
#endif

Cookie_Manager::CookieMessageHandler::~CookieMessageHandler()
{
	g_main_message_handler->UnsetCallBacks(this);
}

void Cookie_Manager::CookieMessageHandler::InitL()
{
	static const OpMessage cookie_man_messages[] =
	{
#if defined PUBSUFFIX_ENABLED
		MSG_PUBSUF_FINISHED_AUTO_UPDATE_ACTION,
#endif
#if defined _ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
		MSG_URL_PROCESS_COOKIES,
#endif
#ifdef COOKIE_MANUAL_MANAGEMENT
		MSG_SAVE_COOKIES,
#endif // COOKIE_MANUAL_MANAGEMENT
	};
	LEAVE_IF_ERROR(g_main_message_handler->SetCallBackList(this, 0, cookie_man_messages, ARRAY_SIZE(cookie_man_messages)));
}

void Cookie_Manager::CookieMessageHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (!g_cookieManager)
		return; // Must be during startup or shutdown

	switch(msg)
	{
#if defined _ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
#ifdef PUBSUFFIX_ENABLED
	case MSG_PUBSUF_FINISHED_AUTO_UPDATE_ACTION:
		/* Call with FALSE to avoid two dialogs opening up for
		 * same cookie. This will result in double delete
		 * when the platform code answers twice for same cookie.
		 */
		g_cookieManager->StartProcessingCookies(FALSE);
		break;
#endif

	case MSG_URL_PROCESS_COOKIES:
		// par2 != 0 means this is just an update from the namelookup and
		// processing should not be forced.
		g_cookieManager->StartProcessingCookies(par2 ? FALSE : TRUE);
		break;
#endif // _ASK_COOKIE

#ifdef COOKIE_MANUAL_MANAGEMENT
	case MSG_SAVE_COOKIES:
		TRAPD(err, g_cookieManager->WriteCookiesL(TRUE));
		OpStatus::Ignore(err);
		break;
#endif // COOKIE_MANUAL_MANAGEMENT
	}
}

/*static*/
OP_STATUS Cookie_Manager::CheckLocalNetworkAndAppendDomain(char* paramdomain, BOOL isFileURL/*=FALSE*/)
{
	if(!paramdomain)
		return OpStatus::ERR_NULL_POINTER;
#ifdef __FILE_COOKIES_SUPPORTED
	if(isFileURL)
		op_strcat(paramdomain, ".$localfile$");
	else
#endif
		if(op_strchr(paramdomain,'.') == NULL && op_strspn(paramdomain, "0123456789") < op_strlen(paramdomain))
			op_strcat(paramdomain,".local");

	return OpStatus::OK;
}

/*static*/
OP_STATUS Cookie_Manager::CheckLocalNetworkAndAppendDomain(OpString& paramdomain, BOOL isFileURL/*=FALSE*/)
{
#ifdef __FILE_COOKIES_SUPPORTED
	if(isFileURL)
		RETURN_IF_ERROR(paramdomain.Append(".$localfile$"));
	else
#endif
		if(uni_strchr(paramdomain.CStr(),'.') == NULL && uni_strspn(paramdomain.CStr(), UNI_L("0123456789")) < (size_t)paramdomain.Length())
			RETURN_IF_ERROR(paramdomain.Append(".local"));

	return OpStatus::OK;
}

