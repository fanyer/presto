/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#include "modules/hardcore/mh/messages.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/url/url2.h"
#include "modules/util/handy.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"

#include "modules/olddebug/timing.h"

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/url_pd.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/protocols/http_req2.h"
#include "modules/auth/auth_elm.h"

#include "modules/util/htmlify.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/about/opredirectpage.h"

// Url_lhn.cpp

// URL Name resolver Load Handler

#include "modules/url/loadhandler/url_lhn.h"


CommState URL_DataStorage::StartNameCompletion(const URL& referer_url)
{
	TRAPD(op_err, SetAttributeL(URL::KReferrerURL, referer_url));
	OpStatus::Ignore(op_err);

	loading = OP_NEW(URL_NameResolve_LoadHandler, (url, g_main_message_handler));

	if (!loading ||
		loading->Load() != COMM_LOADING)
	{
		OP_DELETE(loading);
		loading = NULL;
		SetAttribute(URL::KHeaderLoaded,TRUE);
		SetAttribute(URL::KReloading,FALSE);
		SetAttribute(URL::KIsResuming,FALSE);
		SetAttribute(URL::KReloadSameTarget,FALSE);

		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));

		storage = old_storage;
		old_storage = NULL;

		__DO_STOP_TIMING(TIMING_COMM_LOAD_DS);
		return COMM_REQUEST_FAILED;
	}

	__DO_STOP_TIMING(TIMING_COMM_LOAD_DS);
	return COMM_LOADING;

}


NameCandidate::NameCandidate()
{
}

OP_STATUS NameCandidate::Construct(const OpStringC &prefix, const OpStringC &kernel, const OpStringC &postfix)
{
	OpStringC dot(UNI_L("."));
	OpStringC empty;

	RETURN_IF_ERROR(name.SetConcat(prefix, (prefix.HasContent() ? dot : empty),
						kernel, (postfix.HasContent() ? dot : empty)));

	RETURN_IF_ERROR(name.Append(postfix));

	return OpStatus::OK;
}

OP_STATUS NameCandidate::Create(NameCandidate **namecandidate, const OpStringC &prefix, const OpStringC &kernel, const OpStringC &postfix)
{
	*namecandidate = OP_NEW(NameCandidate, ());
	if (*namecandidate == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS op_err = (*namecandidate)->Construct(prefix, kernel, postfix);

	if (OpStatus::IsError(op_err))
	{
		OP_DELETE(*namecandidate);
		*namecandidate = NULL;
	}
	return op_err;
}

URL_NameResolve_LoadHandler::URL_NameResolve_LoadHandler(URL_Rep *url_rep, MessageHandler *mh)
: URL_LoadHandler(url_rep, mh)
{
	base = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
	candidate = NULL;
	lookup = NULL;
	proxy_request = NULL;
	force_direct_lookup = FALSE;
}

URL_NameResolve_LoadHandler::~URL_NameResolve_LoadHandler()
{
	g_main_message_handler->UnsetCallBacks(this);
	SafeDestruction(lookup);
	lookup = NULL;
	if(proxy_request)
	{
		proxy_request->StopLoading(NULL);
		proxy_block.UnsetURL();

		OP_DELETE(proxy_request);
		proxy_request = NULL;
	}
}

void URL_NameResolve_LoadHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_STATUS err;

	URL this_url(url, (char *) NULL);
	URL_InUse url_blocker(this_url);

	if(proxy_request && (URL_ID) par1 != proxy_request->Id())
		if(!lookup || (unsigned int) par1 != lookup->Id())
			return;

	force_direct_lookup = FALSE;
	NormalCallCount blocker(this);

	OP_MEMORY_VAR BOOL nameresolved = FALSE;
	if(proxy_request)
	{
		switch(msg)
		{
		//case MSG_HTTP_HEADER_LOADED:
		case MSG_HEADER_LOADED:
		case MSG_URL_LOADING_FAILED:
			{
				unsigned int response = proxy_request->GetAttribute(URL::KHTTP_Response_Code);

				if(msg == MSG_URL_LOADING_FAILED && par2 == 0 && response == HTTP_PROXY_UNAUTHORIZED)
				{
					g_main_message_handler->PostDelayedMessage(MSG_URL_LOADING_FAILED, proxy_request->Id(),0, 20000); // Fail if Request does not resolve within 10 seconds
						return;
				}

				if(msg == MSG_URL_LOADING_FAILED && par2 == MSG_NOT_USING_PROXY)
				{
					NameCandidate *cand = NULL;

					if(OpStatus::IsSuccess(NameCandidate::Create(&cand, NULL, candidate->UniName(), NULL)))
					{
						cand->IntoStart(&candidates);
						force_direct_lookup = TRUE;
					}
				}
				else if(response == HTTP_BAD_REQUEST ||
					response == HTTP_NOT_FOUND ||
					(response >= HTTP_INTERNAL_ERROR &&
					response < 600) ||
					(msg == MSG_COMM_LOADING_FAILED &&
					(par2 == 0 ||
					LOWORD(par2) == URL_ERRSTR(SI, ERR_COMM_HOST_NOT_FOUND ) ||
					LOWORD(par2) == URL_ERRSTR(SI, ERR_COMM_PROXY_HOST_NOT_FOUND)))
					)
				{
				}
				else
				{
					nameresolved = TRUE;
				}

			}
		}
		g_main_message_handler->RemoveCallBacks(this, proxy_request->Id());
		proxy_request->StopLoading(NULL);
		proxy_block.UnsetURL();

		OP_DELETE(proxy_request);
		proxy_request = NULL;
	}
	else
	{
		if(msg == MSG_COMM_NAMERESOLVED)
			nameresolved = TRUE;
	}

	if (!nameresolved)
	{
		StartNameResolve();
		return;
	}

	if (base == candidate)
	{
		candidates.Clear();
		mh->PostMessage(MSG_NAME_COMPLETED, Id(), 0);
	}
	else
	{
		//base->SetAlias(candidate);
		urlManager->MakeUnique(url);
		URL_DataStorage *url_ds = url->GetDataStorage();
		if(!url_ds)
			return;

		err = url_ds->SetAttribute(URL::KHTTP_Response_Code, HTTP_MOVED);
		if(OpStatus::IsSuccess(err))
			err = url_ds->SetAttribute(URL::KHTTPResponseText, "Internal Host Redirection");
		if(OpStatus::IsError(err))
		{
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			if(OpStatus::IsMemoryError(err))
				g_memory_manager->RaiseCondition(err);
			return;
		}

		const char * OP_MEMORY_VAR protocol = ((URLType) url->GetAttribute(URL::KType) == URL_HTTPS ? "https" : "http");
		const char * OP_MEMORY_VAR p_host = candidate->Name();
#ifndef NO_FTP_SUPPORT
		if(op_strnicmp(p_host,"ftp.",4) == 0)
			protocol = "ftp";
		else
#endif // NO_FTP_SUPPORT
		if(op_strnicmp(p_host,"news.",5) == 0 || op_strnicmp(p_host,"nntp.",5) == 0)
			protocol = "news";
		else if(op_strnicmp(p_host,"snews.",6) == 0 || op_strnicmp(p_host,"secnews.",8) == 0 || op_strnicmp(p_host,"nntps.",6) == 0)
			protocol = "snews";
		else if(op_strnicmp(p_host,"gopher.",7) == 0)
			protocol = "gopher";
		else if(op_strnicmp(p_host,"wais.",5) == 0)
			protocol = "wais";

		char *portstr= (char*)g_memory_manager->GetTempBuf();

		if(url->GetAttribute(URL::KServerPort))
			op_snprintf(portstr, g_memory_manager->GetTempBufLen(),":%u", url->GetAttribute(URL::KServerPort));
		else
			portstr[0] = '\0';

		OpString8 temp_var;
		OpString8 temp_pathquery;
		TRAPD(err, url->GetAttributeL(URL::KPathAndQuery_L, temp_pathquery));
		OpStatus::Ignore(err); // Not much of a problem if this fails
		OpStringC8 temp_path = url->GetAttribute(URL::KPath); // Just to be on the safe side

		temp_var.AppendFormat("%s://%s%s%s",protocol,p_host, portstr, 
			(temp_pathquery.HasContent() ? temp_pathquery.CStr() : (temp_path.HasContent() ? temp_path.CStr() : "")));
		err = url_ds->SetAttribute(URL::KHTTP_Location, temp_var);
		OpString8 location;
		if(OpStatus::IsSuccess(err))
			err = url_ds->GetAttribute(URL::KHTTP_Location, location);
		if(OpStatus::IsError(err) || location.IsEmpty())
		{
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			if(OpStatus::IsMemoryError(err))
				g_memory_manager->RaiseCondition(err);
			return;
		}

		{
			NormalCallCount blocker(this);
			err = url_ds->ExecuteRedirect();
		}

		URL me(url, static_cast<char *>(NULL));
		URL location_url = g_url_api->GetURL(location);
		OpRedirectPage document(me, &location_url);
		document.GenerateData();

		if(OpStatus::IsError(err))
		{
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			if(OpStatus::IsMemoryError(err))
				g_memory_manager->RaiseCondition(err);
			return;
		}

		candidates.Clear();
		mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
	}
}

CommState URL_NameResolve_LoadHandler::Load()
{
	const uni_char		*permute_template;

	permute_template = base->UniName();
	SetProgressInformation(START_NAME_COMPLETION,0,NULL);

	if(!permute_template || !*permute_template)
	{
		mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_HOST_UNAVAILABLE));
		return COMM_REQUEST_FAILED;
	}
	if(	uni_strni_eq(permute_template, "LOCALHOST", 10) || uni_strchr(permute_template,'.') != NULL ||
		(uni_isdigit(*permute_template) &&
			uni_strspn(permute_template,UNI_L("0123456789")) == uni_strlen(permute_template)))
	{
		mh->PostMessage(MSG_NAME_COMPLETED, Id(), 0);
		return COMM_LOADING;
	}

	NameCandidate *new_candidate;
	OP_STATUS op_err = OpStatus::OK;

#ifdef ADD_PERMUTE_NAME_PARTS
	OpString			permute_front;
	OpString			permute_end;
	uni_char   			*permute_front_pos;
	uni_char   			*permute_end_pos;

	permute_front.Set(g_pcnet->GetStringPref(PrefsCollectionNetwork::HostNameExpansionPrefix));
	permute_end.Set(g_pcnet->GetStringPref(PrefsCollectionNetwork::HostNameExpansionPostfix));
	permute_front_pos = permute_front.CStr();
	permute_end_pos = permute_end.CStr();


	if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckPermutedHostNames) && (permute_front.HasContent() || permute_end.HasContent()))
	{
		while(1)
		{
			uni_char *tempfront=NULL, *tempend=NULL;
			uni_char tmpf=0, tmpe=0;

			tempfront = permute_front_pos;
			tempend = permute_end_pos;

			if(permute_front_pos)
				tempfront = uni_strpbrk(permute_front_pos,UNI_L(";, \t"));
			if(tempfront)
			{
				tmpf = *tempfront;
				*tempfront = '\0';
			}

			if(permute_end_pos)
				tempend = uni_strpbrk(permute_end_pos,UNI_L(";, \t"));
			if(tempend)
			{
				tmpe = *tempend;
				*tempend = '\0';
			}

			op_err = NameCandidate::Create(&new_candidate, permute_front_pos, permute_template, permute_end_pos);
			if(OpStatus::IsSuccess(op_err))
				new_candidate->Into(&candidates);
			else
			{
				OP_DELETE(new_candidate);
				new_candidate = NULL; //FIXME:OOM (continue when OOM?)
			}

#ifdef DEBUG_COMM_FILE
			{
				FILE *tfp = fopen("c:\\klient\\winsck.txt","a");
				fprintf(tfp,"[%d] Comm::InitLoad() - Checking permuted hostname %s\n", id, permuted_host);
				fclose(tfp);
			}
#endif

			if(tempend)
			{
				permute_end_pos = tempend+1;
				*tempend = tmpe;
				while(*permute_end_pos == ' ' || *permute_end_pos == '\t')
					permute_end_pos ++;
			}
			else
			{
				if(tempfront)
				{
					permute_front_pos = tempfront+1;
					while(*permute_front_pos == ' ' || *permute_front_pos == '\t')
						permute_front_pos ++;
					permute_end_pos = (*permute_front_pos  ? permute_end.CStr() : NULL);
				}
				else
					permute_end_pos = permute_front_pos = NULL;
			}

			if(tempfront)
				*tempfront = tmpf;

			if(!((permute_end_pos && *permute_end_pos) ||
				(permute_front_pos && *permute_front_pos)))
			{
				permute_front.Empty();
				permute_end.Empty();
				break;
			}
		}
	}
#endif // ADD_PERMUTE_NAME_PARTS

	op_err = NameCandidate::Create(&new_candidate, NULL, permute_template, NULL);
	if(OpStatus::IsSuccess(op_err))
	{
#ifdef ADD_PERMUTE_NAME_PARTS
		if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckHostOnLocalNetwork))
			new_candidate->IntoStart(&candidates);
		else
			new_candidate->Into(&candidates);
#else
		new_candidate->IntoStart(&candidates);
#endif

	}
	else
	{
		OP_DELETE(new_candidate);
		new_candidate = NULL; // FIXME:OOM (continue when OOM?)
	}


	static const OpMessage messages[] =
    {
        MSG_COMM_LOADING_FINISHED,
        MSG_COMM_LOADING_FAILED,
		MSG_NAME_COMPLETED
    };

    RAISE_AND_RETURN_VALUE_IF_ERROR(mh->SetCallBackList(url->GetDataStorage(), Id(), messages, ARRAY_SIZE(messages)), COMM_REQUEST_FAILED);

	return StartNameResolve();

}

unsigned URL_NameResolve_LoadHandler::ReadData(char *buffer, unsigned buffer_len)
{
	return 0;
}

void URL_NameResolve_LoadHandler::EndLoading(BOOL force)
{
	if(proxy_request)
	{
		proxy_request->StopLoading(NULL);
		proxy_block.UnsetURL();
		OP_DELETE(proxy_request);
		proxy_request = NULL;
	}
	if(lookup)
	{
		lookup->Stop();
		SafeDestruction(lookup);
		lookup = NULL;
	}
}

CommState URL_NameResolve_LoadHandler::StartNameResolve()
{
	NameCandidate *cand = (NameCandidate *) candidates.First();
	if(cand == NULL)
	{
		mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_HOST_UNAVAILABLE));
		return COMM_REQUEST_FAILED;
	}
	cand->Out();

	candidate = g_url_api->GetServerName(cand->name,TRUE);
	OP_DELETE(cand);

	if(candidate == NULL)
	{
		mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		return COMM_REQUEST_FAILED;
	}

	if(candidate->SocketAddress()->IsValid())
	{
		if(!mh->HasCallBack(this, MSG_COMM_NAMERESOLVED, Id()))
			RAISE_AND_RETURN_VALUE_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_COMM_NAMERESOLVED, Id()), COMM_REQUEST_FAILED);
		mh->PostMessage(MSG_COMM_NAMERESOLVED, Id(), 0);
		return COMM_LOADING;
	}

	URLType type = (URLType) url->GetAttribute(URL::KType);
	unsigned port = (url->GetAttribute(URL::KServerPort) ? url->GetAttribute(URL::KServerPort) :
							((URLType) url->GetAttribute(URL::KType) == URL_HTTP ? 80 : 443));

	const uni_char *proxy_string = NULL;
	BOOL autoproxy = FALSE;

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	if (!force_direct_lookup && g_pcnet->IsAutomaticProxyOn())
	{
		proxy_string = g_pcnet->GetStringPref(PrefsCollectionNetwork::AutomaticProxyConfigURL).CStr();
		if(proxy_string && *proxy_string)
			autoproxy = TRUE;
	}
#endif

	if(!force_direct_lookup && !autoproxy)
	{
		proxy_string  = (type == URL_HTTP ? g_pcnet->GetHTTPProxyServer(candidate->UniName())
			: (type == URL_HTTPS ? g_pcnet->GetHTTPSProxyServer(candidate->UniName()) : NULL));

		if(proxy_string && *proxy_string && !urlManager->UseProxyOnServer(candidate, port))
			proxy_string = NULL;
	}

	force_direct_lookup = FALSE;
	if(proxy_string && *proxy_string )
	{
		OpString8 lookup_url_text;
		URL temp;
		OpAutoPtr<URL> temp1(NULL);

		unsigned short port = (unsigned short) url->GetAttribute(URL::KServerPort);

		if(OpStatus::IsError(lookup_url_text.AppendFormat((port ? "%s://%s:%u/" : "%s://%s/"),
			(type == URL_HTTPS ? "https" : "http"), candidate->Name(), port)) )
			goto http_error;

		temp = urlManager->GetURL(lookup_url_text, NULL, TRUE);
		if(temp.IsEmpty())
			goto http_error;

		temp1.reset(OP_NEW(URL, (temp)));
		if(temp1.get() == NULL)
		{
			goto http_error;
		}

		temp1->SetAttribute(URL::KHTTP_Method, HTTP_METHOD_HEAD);
		temp1->SetAttribute(URL::KDisableProcessCookies,TRUE);
		temp1->SetAttribute(URL::KSendOnlyIfProxy, TRUE);
#ifdef URL_DISABLE_INTERACTION
		if(url->GetAttribute(URL::KBlockUserInteraction))
			temp1->SetAttribute(URL::KBlockUserInteraction,TRUE);
#endif

		temp = URL();
		if(temp1->Load(g_main_message_handler,temp, FALSE, FALSE, FALSE, TRUE) != COMM_LOADING ||
			OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_HEADER_LOADED, temp1->Id())) ||
			OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_URL_LOADING_FAILED, temp1->Id())))
		{
			temp1->StopLoading(NULL);
			g_main_message_handler->RemoveCallBacks(this, temp1->Id());
			goto http_error;
		}

		g_main_message_handler->PostDelayedMessage(MSG_URL_LOADING_FAILED, temp1->Id(),0, 20000); // Fail if Request does not resolve within 10 seconds

		proxy_request = temp1.release();
		proxy_block.SetURL(*proxy_request);
		return COMM_LOADING;

http_error:;
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		return COMM_REQUEST_FAILED;
	}
	else
	{
		if(!lookup)
		{
			lookup = Comm::Create(g_main_message_handler, candidate, 80);

			if(lookup == NULL)
			{
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				return COMM_REQUEST_FAILED;
			}
#ifdef URL_DISABLE_INTERACTION
			if(url->GetAttribute(URL::KBlockUserInteraction))
				lookup->SetUserInteractionBlocked(TRUE);
#endif

			RAISE_AND_RETURN_VALUE_IF_ERROR(lookup->SetCallbacks(this,this), COMM_REQUEST_FAILED);
			lookup->ChangeParent(this);
			RAISE_AND_RETURN_VALUE_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_COMM_NAMERESOLVED, lookup->Id()), COMM_REQUEST_FAILED);;
			RAISE_AND_RETURN_VALUE_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_COMM_NAMERESOLVED, Id()), COMM_REQUEST_FAILED);;
		}
		CommState state = lookup->LookUpName(candidate);

		if(state == COMM_REQUEST_FINISHED)
			state = COMM_LOADING;

		return state;
	}
}

CommState URL_NameResolve_LoadHandler::ConnectionEstablished()
{
	if(proxy_request && (URLType) url->GetAttribute(URL::KType) == URL_HTTPS)
		g_main_message_handler->PostMessage(MSG_HEADER_LOADED, proxy_request->Id(), 0);
	return COMM_LOADING;
}

void URL_NameResolve_LoadHandler::SetProgressInformation(ProgressState progress_level,
											 unsigned long progress_info1,
											 const void *progress_info2)
{
	switch(progress_level)
	{
	case HEADER_LOADED :
	case REQUEST_FINISHED:
		break;
	case START_REQUEST:
	case LOAD_PROGRESS:
		progress_level = START_NAME_LOOKUP;
		progress_info2 = candidate->UniName();
		// fall-through
	default:
		URL_LoadHandler::SetProgressInformation(progress_level,progress_info1, progress_info2);
	}
}
