/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/

#include "core/pch.h"

#include "adjunct/quick/managers/WebmailManager.h"

#include "adjunct/desktop_util/mail/mailto.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opstrlst.h"
#include "modules/formats/uri_escape.h"
#include "modules/url/url_man.h"


// always make sure this matches the documentation in webmailproviders.ini (and the actual use in that file)
static const char replacement_keys[] =
{
	/* MAILTO_TO      */ 't',
	/* MAILTO_CC      */ 'k',
	/* MAILTO_BCC     */ 'l',
	/* MAILTO_SUBJECT */ 'j',
	/* MAILTO_BODY    */ 'm',
	/* MAILTO_URL	  */ 's'
};


/***********************************************************************************
 **
 **
 ** WebmailManager::Init
 ***********************************************************************************/
OP_STATUS WebmailManager::Init()
{
	OpFile file;
	BOOL exists = FALSE;

	RETURN_IF_ERROR(file.Construct(UNI_L("webmailproviders.ini"), OPFILE_LANGUAGE_CUSTOM_FOLDER));
	RETURN_IF_ERROR(file.Exists(exists));

	// This custom file didn't exist so move on!
	if (!exists)
	{
		RETURN_IF_ERROR(file.Construct(UNI_L("webmailproviders.ini"), OPFILE_INI_CUSTOM_FOLDER));
		RETURN_IF_ERROR(file.Exists(exists));
	}

	if (!exists)
	{
		RETURN_IF_ERROR(file.Construct(UNI_L("webmailproviders.ini"), OPFILE_LANGUAGE_FOLDER));
		RETURN_IF_ERROR(file.Exists(exists));
	}
	if (!exists)
	{
		RETURN_IF_ERROR(file.Construct(UNI_L("webmailproviders.ini"), OPFILE_INI_FOLDER));
		RETURN_IF_ERROR(file.Exists(exists));
	}

	PrefsFile providers_file(PREFS_INI);	
	RETURN_IF_LEAVE(providers_file.ConstructL());
	RETURN_IF_LEAVE(providers_file.SetFileL(&file));

	return Init(&providers_file);
}


/***********************************************************************************
 **
 **
 ** WebmailManager::GetCount
 ***********************************************************************************/
unsigned int WebmailManager::GetCount() const
{
	return m_providers.GetCount();
}


/***********************************************************************************
 **
 **
 ** WebmailManager::GetIdByIndex
 ***********************************************************************************/
unsigned int WebmailManager::GetIdByIndex(unsigned int index) const
{
	return m_providers.Get(index)->id;
}


/***********************************************************************************
 **
 **
 ** WebmailManager::GetName
 ***********************************************************************************/
OP_STATUS WebmailManager::GetName(unsigned int id, OpString& name) const
{
	WebmailProvider* provider = GetProviderById(id);
	if (!provider)
		return OpStatus::ERR;
	return name.Set(provider->name);
}

/***********************************************************************************
 **
 **
 ** WebmailManager::GetFaviconURL
 ***********************************************************************************/
OP_STATUS WebmailManager::GetFaviconURL(unsigned int id, OpString& favicon_url) const
{
	WebmailProvider* provider = GetProviderById(id);
	if (!provider)
		return OpStatus::ERR;
	return favicon_url.Set(provider->favicon_url);
}

/***********************************************************************************
 **
 **
 ** WebmailManager::GetURL
 ***********************************************************************************/
OP_STATUS WebmailManager::GetURL(unsigned int id, OpString& url) const
{
	WebmailProvider* provider = GetProviderById(id);
	if (!provider)
		return OpStatus::ERR;
	return url.Set(provider->url);
}

/***********************************************************************************
 **
 **
 ** WebmailManager::GetTargetURL
 ***********************************************************************************/
OP_STATUS WebmailManager::GetTargetURL(unsigned int id, const URL& mailto_url, URL& target_url) const
{
	OpString8 mailto_url_str, encoded;
	RETURN_IF_ERROR(mailto_url.GetAttribute(URL::KName_Escaped, mailto_url_str));
	if (mailto_url_str.IsEmpty())
		return OpStatus::ERR;
	OpString8 target_url_str;
	RETURN_IF_ERROR(GetTargetURL(id, mailto_url_str, target_url_str));

	target_url = g_url_api->GetURL(target_url_str.CStr());
	return OpStatus::OK;
}

OP_STATUS WebmailManager::GetTargetURL(unsigned int id, const OpStringC8& mailto_url_str, OpString8& target_url_str) const
{
	OpString8 encoded;

	target_url_str.Empty(); // make sure an empty string is returned, in case of error

	WebmailProvider* provider = GetProviderById(id);
	if (!provider)
		return OpStatus::ERR;

	RETURN_IF_ERROR(StringUtils::EscapeURIAttributeValue(mailto_url_str, encoded));
	
	MailTo mailto;
	mailto.Init(mailto_url_str);

	// Create target URL

	target_url_str.Set(provider->url);
	
	// try to replace whole url, if it fails replace one and one parameter
	if (!ReplaceFormat(replacement_keys[MAILTO_URL],target_url_str,encoded))
	{
		OpString8 encoded_value;
		RETURN_IF_ERROR(mailto.GetEncodedTo(encoded_value));
		ReplaceFormat(replacement_keys[MAILTO_TO],target_url_str,encoded_value);
		RETURN_IF_ERROR(mailto.GetEncodedCc(encoded_value));
		ReplaceFormat(replacement_keys[MAILTO_CC],target_url_str,encoded_value);
		RETURN_IF_ERROR(mailto.GetEncodedBcc(encoded_value));
		ReplaceFormat(replacement_keys[MAILTO_BCC],target_url_str,encoded_value);
		RETURN_IF_ERROR(mailto.GetEncodedSubject(encoded_value));
		ReplaceFormat(replacement_keys[MAILTO_SUBJECT],target_url_str,encoded_value);
		RETURN_IF_ERROR(mailto.GetEncodedBody(encoded_value));
		ReplaceFormat(replacement_keys[MAILTO_BODY],target_url_str,encoded_value);
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** WebmailManager::ReplaceFormat
 ***********************************************************************************/
BOOL WebmailManager::ReplaceFormat(const char to_be_replaced, OpString8& string, const OpStringC8 string_to_insert) const
{	
	OpString8 destination;
	destination.Reserve(string.Length()+string_to_insert.Length()+1);
	char *dst = destination.CStr();
	char *src = string.CStr();
	const char *to_insert = string_to_insert.CStr();

	BOOL has_replaced = FALSE;
	// do it this way because AppendFormat crashes for really large values of provider url
	while (*src)
	{
		if (*src =='%' && *(src+1)==to_be_replaced)
		{
			while(to_insert && *to_insert)
			{
				*dst++ = *to_insert++;
			}
			// mark as replaced even if the text to insert is ""
			has_replaced = TRUE;
			src+=2;
		}
		else
		{
			*dst++ = *src++;
		}
	}
	*dst='\0';
	
	if (has_replaced)
		string.Set(destination);

	return has_replaced;
}

/***********************************************************************************
 **
 **
 ** WebmailManager::Init
 ***********************************************************************************/
OP_STATUS WebmailManager::Init(PrefsFile* prefs_file)
{
	if (!prefs_file)
		return OpStatus::ERR;
 
	RETURN_IF_LEAVE(prefs_file->LoadAllL());
 
	// Read sections, each section is a webmail provider
	OpString_list section_list;
	RETURN_IF_LEAVE(prefs_file->ReadAllSectionsL(section_list));
 
	for (unsigned long i = 0; i < section_list.Count(); i++)
	{
		OpAutoPtr<WebmailProvider> provider (OP_NEW(WebmailProvider, ()));
		if (!provider.get())
			return OpStatus::ERR_NO_MEMORY;
 
		// Get name, ID and URL for provider
		RETURN_IF_ERROR(provider->name.Set(section_list[i]));
 
		RETURN_IF_LEAVE(provider->id = prefs_file->ReadIntL(provider->name, UNI_L("ID"), 0));

		OpString url16;
		RETURN_IF_LEAVE(prefs_file->ReadStringL(provider->name, UNI_L("URL"), url16));
		RETURN_IF_ERROR(provider->url.Set(url16.CStr()));
 
		OpString favicon_url16;
		RETURN_IF_LEAVE(prefs_file->ReadStringL(provider->name, UNI_L("ICON"), favicon_url16));
		RETURN_IF_ERROR(provider->favicon_url.Set(favicon_url16.CStr()));

		provider->is_webhandler = FALSE;

		// Only add valid providers to list
		if (provider->id > 0 && provider->name.HasContent() && provider->url.HasContent())
		{
			RETURN_IF_ERROR(m_providers.Add(provider.get()));
			provider.release();
		}
	}

	TrustedProtocolData data;
	INT32 num_trusted_protocols = g_pcdoc->GetNumberOfTrustedProtocols();
	for (OP_MEMORY_VAR INT32 i=0; i<num_trusted_protocols; i++)
	{
		if (!g_pcdoc->GetTrustedProtocolInfo(i, data))
			continue;
		if (data.protocol.Compare(UNI_L("mailto")))
			continue;
		if (data.web_handler.HasContent())
		{
			// Pick a unique id, 0 means it failed
			unsigned int id = GetFreeId();
			if (id == 0)
				break;

			OpAutoPtr<WebmailProvider> provider (OP_NEW(WebmailProvider, ()));
			if (!provider.get())
				return OpStatus::ERR_NO_MEMORY;

			RETURN_IF_ERROR(provider->name.Set(data.description.HasContent() ? data.description : data.web_handler));
			RETURN_IF_ERROR(provider->url.Set(data.web_handler));
			provider->id = id;
			provider->is_webhandler = TRUE;

			RETURN_IF_ERROR(m_providers.Add(provider.get()));
			provider.release();
			break;
		}
	}
	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** WebmailManager::GetProviderById
 ***********************************************************************************/
WebmailManager::WebmailProvider* WebmailManager::GetProviderById(unsigned int id) const
{
	WebmailProvider* provider = 0;
 
	for (unsigned i = 0; i < m_providers.GetCount() && !provider; i++)
	{
		if (m_providers.Get(i)->id == id)
			provider = m_providers.Get(i);
	}
 
	return provider;
}

/***********************************************************************************
 **
 **
 ** WebmailManager::GetFreeId
 ***********************************************************************************/
unsigned int WebmailManager::GetFreeId() const
{
	unsigned int id = 10000;
	while (id > 0)
	{
		BOOL match = FALSE;

		for (unsigned i = 0; i < m_providers.GetCount() && !match; i++)
			match = m_providers.Get(i)->id == id;

		if (!match)
			return id;

		id--;
	}

	return 0; // Will never happen unless we have 10000 mailers, which we do not have
}

/***********************************************************************************
 **
 **
 ** WebmailManager::OnWebHandlerAdded
 ***********************************************************************************/
OP_STATUS WebmailManager::OnWebHandlerAdded(OpStringC url, OpStringC description, BOOL set_default)
{
	unsigned int id = GetFreeId();
	if (id == 0)
		return OpStatus::ERR;

	OpAutoPtr<WebmailProvider> provider (OP_NEW(WebmailProvider, ()));
	if (!provider.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(provider->name.Set(description.HasContent() ? description : url));
	RETURN_IF_ERROR(provider->url.Set(url));
	provider->id = id;
	provider->is_webhandler = TRUE;

	RETURN_IF_ERROR(m_providers.Add(provider.get()));
	provider.release();

	// Remove old entry should there be one
	for (unsigned i = 0; i < m_providers.GetCount(); )
	{
		if (m_providers.Get(i)->is_webhandler && m_providers.Get(i)->id != id)
			m_providers.Delete(i);
		else
			i++;
	}

	if (set_default)
	{
		RETURN_IF_LEAVE(g_pcui->WriteIntegerL(PrefsCollectionUI::MailHandler, MAILHANDLER_WEBMAIL));
		RETURN_IF_LEAVE(g_pcui->WriteIntegerL(PrefsCollectionUI::WebmailService, id));
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** WebmailManager::OnWebHandlerRemoved
 ***********************************************************************************/
void WebmailManager::OnWebHandlerRemoved(OpStringC url)
{
	for (unsigned i = 0; i < m_providers.GetCount(); i++)
	{
		if (url.Compare(m_providers.Get(i)->url))
		{
			m_providers.Delete(i);
			return;
		}
	}
}

/***********************************************************************************
 **
 **
 ** WebmailManager::IsWebHandlerRegistered
 ***********************************************************************************/
BOOL WebmailManager::IsWebHandlerRegistered(OpStringC url)
{
	for (unsigned i = 0; i < m_providers.GetCount(); i++)
	{
		if (url.Compare(m_providers.Get(i)->url))
			return TRUE;
	}
	return FALSE;
}

/***********************************************************************************
 **
 **
 ** WebmailManager::GetSource
 ***********************************************************************************/
OP_STATUS WebmailManager::GetSource(unsigned int id, HandlerSource& source)
{
	WebmailProvider* provider = GetProviderById(id);
	if (!provider)
		return OpStatus::ERR;
	source = provider->is_webhandler ? HANDLER_CUSTOM : HANDLER_PREINSTALLED;
	return OpStatus::OK;
}

