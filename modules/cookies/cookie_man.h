/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#ifndef _COOKIE_MANAGER_H_
#define _COOKIE_MANAGER_H_

class Cookie_Item_Handler;
class CookieDomain;
class Cookie;

#include "modules/util/opfile/opfile.h"
#include "modules/hardcore/mh/messobj.h"

// Cookie memory use accounting
#undef DEBUG_COOKIE_MEMORY_SIZE

class Cookie_Manager
{
private:
	URL_CONTEXT_ID context_id;
#ifdef COOKIES_CONTROL_PER_CONTEXT
	BOOL cookies_enabled_for_context;
#endif
# ifdef DISK_COOKIES_SUPPORT
	BOOL			cookie_file_read;
# endif
	CookieDomain	*cookie_root;
	Head			unprocessed_cookies;

#ifdef _ASK_COOKIE
	BOOL			processing_cookies;
#endif

	/** Temporary buffers for working on cookie paths and cookie
	 *  domains. Neither should be very long so it doesn't matter
	 *  a lot that we have two per URL context. In the long run
	 *  we will try to refactor it though.
	 */
	size_t			cookie_temp_buffer_len;
	char *			cookie_processing_buf1;
	char *			cookie_processing_buf2;

	int				max_cookies;
	int				cookies_count;
	int				max_cookies_in_domain;
	int				max_cookie_len;
#ifdef DEBUG_COOKIE_MEMORY_SIZE
	unsigned long	cookie_mem_size;
#endif

	OpFileFolder	cookie_file_location;

	BOOL			updated_cookies;

	AutoDeleteHead	ContextManagers;

public:
	Cookie_Manager();
	void InitL(OpFileFolder loc, URL_CONTEXT_ID a_id = 0);
	virtual ~Cookie_Manager();
	void PreDestructStep();

	/** check the paramdomain append ".local" for locals domains
	 *
	 *  @param paramdomain, must be allocated 
	 *  @param isFileURL is this a file URL
	 *  @return OP_STATUS
	 */
	static OP_STATUS CheckLocalNetworkAndAppendDomain(char* paramdomain, BOOL isFileURL = FALSE);

	/** check the paramdomain for local network and append ".local" for locals domains
	 *
	 *  @param paramdomain OpString
	 *  @param isFileURL is this a file URL
	 *  @return OP_STATUS
	 */
	static OP_STATUS CheckLocalNetworkAndAppendDomain(OpString& paramdomain, BOOL isFileURL = FALSE);

private:
	void InternalInit();
	void InternalDestruct();

	OP_STATUS CheckCookieTempBuffers(size_t len);

public:
	/** This returns a pointer to a shared global buffer so only one return value can be used at a time. */
	const char*		GetCookiesL(URL_Rep *,
							int& version,
							int &max_version,
							BOOL already_have_password,
							BOOL already_have_authentication,
							BOOL &have_password,
							BOOL &has_password,
							URL_CONTEXT_ID context_id = 0,
							BOOL for_http=FALSE
		);
	void			HandleCookiesL(URL_Rep *, HeaderList &cookies, URL_CONTEXT_ID context_id = 0);
	void			HandleSingleCookieL(URL_Rep *url, const char *cookiearg,
							   const char *request,
							   int version
							   , URL_CONTEXT_ID context_id = 0
							   , Window* win = NULL
							   );
	void			HandleSingleCookieL(Head &currently_parsed_cookies, URL_Rep *url, const char *cookiearg,
							   const char *request,
							   int version,
							   BOOL set_from_http = FALSE,
							   Window* win = NULL
							   );
	void			StartProcessingCookies(BOOL reset_process=FALSE);
private:
	void			StartProcessingCookiesAction(BOOL reset_process);

public:
	void			SetCookie(Cookie_Item_Handler *cookie);
#ifdef _ASK_COOKIE
	BOOL			CheckAcceptCookieUpdates(Cookie_Item_Handler *cookie_item);
#ifdef ENABLE_COOKIE_CREATE_DOMAIN
	void			CreateCookieDomain(const char *domain_name);
	void			CreateCookieDomain(ServerName *domain_sn);
#endif
	void			RemoveSameCookiesFromQueue(Cookie_Item_Handler *set_item);
#endif
#if defined _ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
	int				HandlingCookieForURL(URL_Rep *, URL_CONTEXT_ID context_id = 0);
#endif

	void			SetMaxTotalCookies(int max) {max_cookies = (max >= 0 ? max : 300);};
	int				GetMaxCookies() { return max_cookies; }
	int				GetCookiesCount() { return cookies_count; }
	void			IncCookiesCount() { cookies_count++; }
	void			DecCookiesCount() { cookies_count--; }
	int				GetMaxCookiesInDomain() { return max_cookies_in_domain; }
	size_t			GetMaxCookieLen() { return max_cookie_len; }
#ifdef DEBUG_COOKIE_MEMORY_SIZE
	unsigned long	GetCookieMemorySize(){return cookie_mem_size;}
	void			AddCookieMemorySize(unsigned long len){cookie_mem_size += len;}
	void			SubCookieMemorySize(unsigned long len){cookie_mem_size  = (cookie_mem_size >= len ? cookie_mem_size - len : 0);}
#endif
	void			FreeUnusedResources();

#ifdef QUICK_COOKIE_EDITOR_SUPPORT
	void			BuildCookieEditorListL(URL_CONTEXT_ID id = 0);
#endif

#ifdef NEED_URL_COOKIE_ARRAY
	OP_STATUS       BuildCookieList(
                               Cookie ** cookieArray,
                               int * size_returned,
                               URL_Rep *url);

	OP_STATUS       BuildCookieList(
                               Cookie ** cookieArray,
                               int * size_returned,
                               uni_char * domain,
                               uni_char * path,
                               BOOL is_export = FALSE,
                               BOOL match_subpaths = FALSE);
	OP_STATUS       RemoveCookieList(uni_char * domainstr, uni_char * pathstr, uni_char * namestr);
#endif // NEED_URL_COOKIE_ARRAY


	void			CheckCookiesReadL();
#ifdef DISK_COOKIES_SUPPORT
	void			ReadCookiesL();
	void			WriteCookiesL(BOOL requested_by_platform = FALSE);
	void			AutoSaveCookies();
#endif
	void			ClearCookiesCommitL( BOOL delete_filters=FALSE );
	void			CleanNonPersistenCookies();
#ifdef _SSL_USE_SMARTCARD_
	void			CleanSmartCardAuthenticatedCookies(ServerName *server);
#endif



	void			AddContextL(URL_CONTEXT_ID id, OpFileFolder loc, BOOL share_with_main_context);

	OP_STATUS		SetShareCookiesWithMainContext(URL_CONTEXT_ID id, BOOL share)
	{
		CookieContextItem* context = FindContext(id);
		if (context)
		{
			FindContext(id)->SetShareWithMainContext(share);
			return OpStatus::OK;
		}
		else
			return OpStatus::ERR;
	}

	void			RemoveContext(URL_CONTEXT_ID id);
	BOOL			ContextExists(URL_CONTEXT_ID id);
	void			IncrementContextReference(URL_CONTEXT_ID id);
	void			DecrementContextReference(URL_CONTEXT_ID id);
	BOOL			GetContextIsTemporary(URL_CONTEXT_ID id);

#ifdef COOKIES_CONTROL_PER_CONTEXT
	/** Set cookies enabled flag for a context; not possible to update default context */
	void			SetCookiesEnabledForContext(URL_CONTEXT_ID id, BOOL flag);
	/** Get cookies enabled flag for a context; default context is always TRUE, non-existent context FALSE */
	BOOL			GetCookiesEnabledForContext(URL_CONTEXT_ID id);
#endif

#ifdef COOKIE_MANUAL_MANAGEMENT
	/** Request saving all cookie data to file */
	void			RequestSavingCookies();
#endif

private:

	class CookieContextItem : public Link
	{
	public:
		URL_CONTEXT_ID context_id;
		unsigned int references;
		BOOL share_with_main_context;
		BOOL predestruct_done;
		Cookie_Manager *context;

	public:

		CookieContextItem(): context_id(0),references(1), share_with_main_context(FALSE), predestruct_done(FALSE), context(NULL){};
		virtual ~CookieContextItem(){if(InList()) Out(); OP_DELETE(context);}

		void ConstructL(URL_CONTEXT_ID id, OpFileFolder loc, BOOL a_share_with_main_context){
			context_id = id;
			share_with_main_context = a_share_with_main_context;

			context = OP_NEW_L(Cookie_Manager, ());
			context->InitL(loc, context_id);
		}
		virtual void PreDestructStep(){
			if(predestruct_done)
				return;
			references ++;
			predestruct_done = TRUE;
#ifdef DISK_COOKIES_SUPPORT
			if(context)
			{
				TRAPD(op_err, context->WriteCookiesL());
				OpStatus::Ignore(op_err);
			}
#endif
			if(context)
				context->PreDestructStep();
			references --;

		};

		void SetShareWithMainContext(BOOL share)
		{
			share_with_main_context = share;
		}
	};

	CookieContextItem *FindContext(URL_CONTEXT_ID id);
public:

#ifdef _OPERA_DEBUG_DOC_
	void			GetCookieMemUsed(DebugUrlMemory &debug);
#endif

#ifdef WIC_COOKIES_LISTENER
public:
	void IterateAllCookies();
#endif // WIC_COOKIES_LISTENER

private:
	class CookieMessageHandler : public MessageObject
	{
	public:
		~CookieMessageHandler();
		void InitL();
		void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	};

	CookieMessageHandler message_handler;
};


#endif // _COOKIE_MANAGER_H_
