/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/store/store3.h"
#include "adjunct/m2/src/util/htmltotext.h"
#include "adjunct/m2/src/util/misc.h"
#include "adjunct/m2/src/util/qp.h"
#include "adjunct/m2/src/util/quickmimeparser.h"
#include "adjunct/m2/src/util/quote.h"
#include "adjunct/desktop_util/string/htmlify_more.h"
#include "adjunct/desktop_util/datastructures/StreamBuffer.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick_toolkit/widgets/DesktopWidgetWindow.h"

#include "modules/util/opfile/opfile.h"
#include "modules/hardcore/unicode/unicode.h"
#include "modules/url/url_enum.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/prefs/prefsmanager/collections/pc_m2.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/pi/OpLocale.h"

#ifdef UPLOAD_CAP_MODULE
#include "modules/upload/upload.h"
#ifdef __SUPPORT_OPENPGP__
# include "modules/upload/upload_build.h"
#endif
#else
#include "modules/url/mime/upload2.h"
#ifdef __SUPPORT_OPENPGP__
# include "modules/url/mime/upload_build.h"
#endif
#endif

# include "modules/util/handy.h"

#include "modules/encodings/utility/charsetnames.h"

#include <time.h>


#define ASYNC_BLOCK_SIZE	150*1024

#ifdef _DEBUG
int g_message_reference = 0;
extern int g_multipart_reference;
#endif

//--------------------------------------------------------------------------------------

Multipart::Multipart()
: m_url(NULL),
  m_data(NULL),
  m_size(0),
  m_is_mailbody(FALSE),
  m_is_visible(TRUE)
{
}

Multipart::~Multipart()
{
    MessageEngine::GetInstance()->GetGlueFactory()->DeleteURL(m_url);
	OP_DELETEA(m_data);
}

OP_STATUS Multipart::GetContentType(OpString& contenttype) const
{
    if (!m_url)
    {
        contenttype.Empty();
        return OpStatus::OK;
    }

    return contenttype.Set(m_url->GetAttribute(URL::KMIME_Type));
}

OP_STATUS Multipart::SetData(URL* url, const OpStringC8& charset, const OpStringC& filename, int size, BYTE* data, BOOL is_mailbody, BOOL is_visible)
{
    //Clear obsolete stuff
	m_url_inuse.UnsetURL();
    MessageEngine::GetInstance()->GetGlueFactory()->DeleteURL(m_url); m_url = NULL;
	OP_DELETEA(m_data);

    //Copy values
	m_data = data;
	m_is_visible = is_visible;
    m_url = url;
    if (m_url)
		m_url_inuse.SetURL(*m_url);

    m_size = size;
    URLContentType contenttype = GetContentType();
    m_is_mailbody = is_mailbody && (contenttype==URL_TEXT_CONTENT || contenttype==URL_HTML_CONTENT);

    OP_STATUS ret;
    if ((ret=m_charset.Set(charset.HasContent() ? charset.CStr() : "us-ascii")) != OpStatus::OK)
        return ret;

    return m_suggested_filename.Set(filename);
}

OP_STATUS Multipart::GetURLFilename(OpString& filename) const
{
	if (!m_url)
	{
		filename.Empty();
		return OpStatus::OK;
	}

#ifdef URL_CAP_PREPAREVIEW_FORCE_TO_FILE
	m_url->PrepareForViewing(URL::KFollowRedirect, TRUE, TRUE, TRUE);
#else // URL_CAP_PREPAREVIEW_FORCE_TO_FILE
	m_url->PrepareForViewing(TRUE, TRUE);
#endif
	TRAPD(ret, m_url->GetAttribute(URL::KFilePathName_L, filename, TRUE));
	return ret;
}

OP_STATUS Multipart::GetBodyText(OpString16& body) const
{
    body.Empty();

    if (m_charset.CompareI("utf-16")==0)
    {
        return body.Set((uni_char*)m_data, UNICODE_DOWNSIZE(m_size));
    }
    else
    {
		return MessageEngine::GetInstance()->GetInputConverter().ConvertToChar16(m_charset, (char*)m_data, body, TRUE, FALSE, m_size);

    }
}

//--------------------------------------------------------------------------------------

Message::Message()
: Autodeletable(),
  m_async_rfc822_to_url_url(NULL),
  m_async_rfc822_to_url_loop(NULL),
  m_account_id(0),
  m_myself(0),
  m_parent(0),
  m_prev_sibling(0),
  m_next_sibling(0),
  m_first_child(0),
  m_flag(0),
  m_recv_time(0),
  m_create_type(MessageTypes::NEW),
  m_headerlist(NULL),
  m_rawheaders(NULL),
  m_multipart_list(NULL),
  m_multipart_status(MIME_NOT_DECODED),
  m_mime_decoder(NULL),
  m_old_message(NULL),
  m_rawbody(NULL),
  m_in_place_body(NULL),
  m_in_place_body_offset(0),
  m_realmessagesize(0),
  m_localmessagesize(0),
  m_textpart_setting(PREFER_TEXT_PLAIN),
  m_force_charset(false),
  m_is_HTML(false),
  m_not_spam(false),
  m_is_spam(false),
  m_context_id(0),
  m_attachment_list(NULL)
{
#ifdef _DEBUG
	g_message_reference++;
#endif
}


Message::~Message()
{
	Destroy();

#ifdef _DEBUG
	g_message_reference--;
#endif
}


void Message::Destroy()
{
	AbortAsyncRFC822ToUrl(TRUE);

	if (m_mime_decoder)
	{
		OP_DELETE(m_mime_decoder);
		m_mime_decoder = NULL;
	}

	if (m_old_message)
	{
		OP_DELETE(m_old_message);
		m_old_message = NULL;
	}

    if (m_attachment_list)
    {
        m_attachment_list->Clear();
        OP_DELETE(m_attachment_list);
		m_attachment_list = NULL;
    }

    if (m_multipart_list)
    {
        PurgeMultipartData();
        m_multipart_list->Clear();
        OP_DELETE(m_multipart_list);
		m_multipart_list = NULL;
    }

    if (m_headerlist)
    {
        m_headerlist->Clear();
        OP_DELETE(m_headerlist);
		m_headerlist = NULL;
    }

	RemoveRawMessage();

	SetInternetLocation(0);

	OP_ASSERT(!HasRFC822ToUrlListener());
}


void Message::AbortAsyncRFC822ToUrl(BOOL in_destructor)
{
	if(!MessageEngine::GetInstance())
	{
		// nothing to do, M2 is disabled
		return;
	}
	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	if (m_async_rfc822_to_url_loop)
	{
		glue_factory->DeleteMessageLoop(m_async_rfc822_to_url_loop);
		m_async_rfc822_to_url_loop = NULL;
	}

	for (OpListenersIterator iterator(m_async_rfc822_to_url_listeners); m_async_rfc822_to_url_listeners.HasNext(iterator);)
	{
		RFC822ToUrlListener* listener = m_async_rfc822_to_url_listeners.GetNext(iterator);
		if (in_destructor)
		{
			listener->OnUrlDeleted(m_async_rfc822_to_url_url);
			listener->OnMessageBeingDeleted();
			//void* data = (void*)listeners_iterator->GetData();
			//delete data;
		}
		else
		{
			listener->OnRFC822ToUrlDone(OpStatus::ERR);
		}
	}

	if (in_destructor)
	{
		while (TRUE)
		{
			OpListenersIterator iterator(m_async_rfc822_to_url_listeners);
			if (!m_async_rfc822_to_url_listeners.HasNext(iterator))
				break;

			RFC822ToUrlListener* listener = m_async_rfc822_to_url_listeners.GetNext(iterator);
			m_async_rfc822_to_url_listeners.Remove(listener);
		}
	}

	if (m_async_rfc822_to_url_url)
	{
		m_async_rfc822_to_url_inuse.UnsetURL();
		glue_factory->DeleteURL(m_async_rfc822_to_url_url);
		m_async_rfc822_to_url_url = NULL;
	}

}

OP_STATUS Message::RestartAsyncRFC822ToUrl()
{
	OP_STATUS ret;

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	//Report the restart to listeners
	if (m_async_rfc822_to_url_url)
	{
		for (OpListenersIterator iterator(m_async_rfc822_to_url_listeners); m_async_rfc822_to_url_listeners.HasNext(iterator);)
		{
			RFC822ToUrlListener* listener = m_async_rfc822_to_url_listeners.GetNext(iterator);

			listener->OnUrlDeleted(m_async_rfc822_to_url_url);

			listener->OnRFC822ToUrlRestarted();
		}

		m_async_rfc822_to_url_inuse.UnsetURL();
		glue_factory->DeleteURL(m_async_rfc822_to_url_url); //Just to be safe, delete old URL (new one will be created below)
		m_async_rfc822_to_url_url = NULL;
	}

	//Do we have any listeners?
	if (!HasRFC822ToUrlListener())
		return OpStatus::OK; //Nothing to do

	//Create decoder-URL
	m_async_rfc822_to_url_url = glue_factory->CreateURL();
	if (!m_async_rfc822_to_url_url)
		return OpStatus::ERR_NO_MEMORY;

	OpString orl;

	orl.AppendFormat(UNI_L("operaemail:/mail_%d/%.0f.html"), GetId(), g_op_time_info->GetRuntimeMS());
	BrowserUtils* browser_utils = glue_factory->GetBrowserUtils();
	if ((ret=browser_utils->GetURL(m_async_rfc822_to_url_url, orl.CStr())) != OpStatus::OK)
	{
		AbortAsyncRFC822ToUrl();
		return ret;
	}
	m_async_rfc822_to_url_inuse.SetURL(*m_async_rfc822_to_url_url);

	//Create message-loop
	if (!m_async_rfc822_to_url_loop)
	{
		m_async_rfc822_to_url_loop = glue_factory->CreateMessageLoop();
		ret = m_async_rfc822_to_url_loop ? m_async_rfc822_to_url_loop->SetTarget(this) : OpStatus::ERR_NO_MEMORY;
		if (ret != OpStatus::OK) //Error. Clean up and bail out
		{
			AbortAsyncRFC822ToUrl();
			return ret;
		}
	}

	//Prepare URL
	if ((ret=browser_utils->WriteStartOfMailUrl(m_async_rfc822_to_url_url, this)) != OpStatus::OK)
	{
		AbortAsyncRFC822ToUrl();
		return ret;
	}

	//URL is ready to be written to. Report to listeners
	for (OpListenersIterator it(m_async_rfc822_to_url_listeners); m_async_rfc822_to_url_listeners.HasNext(it); )
	{
		RFC822ToUrlListener* listener = m_async_rfc822_to_url_listeners.GetNext(it);
		listener->OnUrlCreated(m_async_rfc822_to_url_url);
	}

	m_async_rfc822_to_url_position = 0; //Make sure we start at the beginning
	m_async_rfc822_to_url_raw_header_size = m_rawheaders ? op_strlen(m_rawheaders) : 0;
	m_async_rfc822_to_url_raw_body_size = m_rawbody ? op_strlen(m_rawbody) : 0;

	//Start the async decoding
	if ((ret=m_async_rfc822_to_url_loop->Post(MSG_M2_ASYNC_RFC822_TO_URL_MESSAGE)) != OpStatus::OK)
	{
		AbortAsyncRFC822ToUrl();
		return ret;
	}

	return OpStatus::OK;
}

OP_STATUS Message::AsyncRFC822ToUrlBlock()
{
	if (!m_async_rfc822_to_url_url)
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS ret;

	int write_length;
	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	BrowserUtils* browser_utils = glue_factory->GetBrowserUtils();

	if (m_force_charset && !m_charset.IsEmpty())
	{
		unsigned short charset_id;
		RETURN_IF_LEAVE(charset_id = g_charsetManager->GetCharsetIDL(m_charset.CStr()));
		RETURN_IF_ERROR(m_async_rfc822_to_url_url->SetAttribute(URL::KMIME_CharSetId, charset_id));
	}

	if (m_async_rfc822_to_url_position<m_async_rfc822_to_url_raw_header_size)
	{
		write_length = min(m_async_rfc822_to_url_raw_header_size-m_async_rfc822_to_url_position, ASYNC_BLOCK_SIZE);
		browser_utils->WriteToUrl(m_async_rfc822_to_url_url, m_rawheaders+m_async_rfc822_to_url_position, write_length);
	}
	else
	{
		if (m_async_rfc822_to_url_position == m_async_rfc822_to_url_raw_header_size) //Done with headers. Insert "\r\n" to start body
		{
			char linefeed[3]; /* ARRAY OK 2003-10-28 frodegill */
			strcpy(linefeed, "\r\n");
			browser_utils->WriteToUrl(m_async_rfc822_to_url_url, linefeed, 2);

			if (m_async_rfc822_to_url_raw_body_size>0)
			{
				browser_utils->SetUrlComment(m_async_rfc822_to_url_url, UNI_L(""));
			}
		}

		write_length = min((m_async_rfc822_to_url_raw_header_size+m_async_rfc822_to_url_raw_body_size)-m_async_rfc822_to_url_position, ASYNC_BLOCK_SIZE);
		browser_utils->WriteToUrl(m_async_rfc822_to_url_url, m_rawbody+(m_async_rfc822_to_url_position-m_async_rfc822_to_url_raw_header_size), write_length);
	}

	m_async_rfc822_to_url_position += write_length;

	if (m_async_rfc822_to_url_position>=(m_async_rfc822_to_url_raw_header_size+m_async_rfc822_to_url_raw_body_size)) //Done!
	{
		OpStatus::Ignore(browser_utils->WriteEndOfMailUrl(m_async_rfc822_to_url_url, this));

		//Report to listeners
		for (OpListenersIterator it(m_async_rfc822_to_url_listeners); m_async_rfc822_to_url_listeners.HasNext(it); )
		{
			RFC822ToUrlListener* listener = m_async_rfc822_to_url_listeners.GetNext(it);

			listener->OnUrlDeleted(m_async_rfc822_to_url_url);

			listener->OnRFC822ToUrlDone(OpStatus::OK);
		}

		m_async_rfc822_to_url_inuse.UnsetURL();
		glue_factory->DeleteURL(m_async_rfc822_to_url_url);
		m_async_rfc822_to_url_url = NULL;

		glue_factory->DeleteMessageLoop(m_async_rfc822_to_url_loop);
		m_async_rfc822_to_url_loop = NULL;
	}
	else //Ask for next block
	{
		OP_ASSERT(m_async_rfc822_to_url_url && m_async_rfc822_to_url_loop);
		if ((ret=m_async_rfc822_to_url_loop->Post(MSG_M2_ASYNC_RFC822_TO_URL_MESSAGE)) != OpStatus::OK)
		{
			AbortAsyncRFC822ToUrl();
			return ret;
		}
	}

	return OpStatus::OK;
}

OP_STATUS Message::AddRFC822ToUrlListener(RFC822ToUrlListener* rfc822_to_url_listener)
{
	if (!rfc822_to_url_listener)
		return OpStatus::ERR_NULL_POINTER;

	//Are we already decoding?
    BOOL already_decoding = HasRFC822ToUrlListener();

	//Add listener
	RETURN_IF_ERROR(m_async_rfc822_to_url_listeners.Add(rfc822_to_url_listener));

	//Start decoding
	if (!already_decoding)
	{
		OP_STATUS ret = RestartAsyncRFC822ToUrl();

		if (OpStatus::IsError(ret))
		{
			AbortAsyncRFC822ToUrl();
			return ret;
		}
	}
	else
	{
		rfc822_to_url_listener->OnUrlCreated(m_async_rfc822_to_url_url);
	}
	return OpStatus::OK;
}

OP_STATUS Message::RemoveRFC822ToUrlListener(RFC822ToUrlListener* rfc822_to_url_listener)
{
	OP_STATUS ret = m_async_rfc822_to_url_listeners.Remove(rfc822_to_url_listener);

	//Abort if all listeners have pinglet out while we are decoding
	if (m_async_rfc822_to_url_url)
	{
		OpListenersIterator it(m_async_rfc822_to_url_listeners);

		if (!m_async_rfc822_to_url_listeners.HasNext(it))
			AbortAsyncRFC822ToUrl();
	}

	return ret;
}

OP_STATUS Message::Receive(OpMessage message)
{
	if (message==MSG_M2_ASYNC_RFC822_TO_URL_MESSAGE)
	{
		return AsyncRFC822ToUrlBlock();
	}
	return OpStatus::OK;
}


OP_STATUS Message::Init(const UINT16 account_id)
{
    m_account_id = account_id;

    Account* account = GetAccountPtr();
    if (account)
    {
        OP_STATUS ret;
        if ((ret=account->GetCharset(m_charset)) != OpStatus::OK) //Default to accounts' charset
            return ret;

        SetFlag(Message::ALLOW_8BIT, account->GetAllow8BitHeader());
        SetFlag(Message::ALLOW_QUOTESTRING_QP, !IsFlagSet(Message::IS_OUTGOING) && account->GetAllowIncomingQuotedstringQP());

		m_force_charset = account->GetForceCharset() != FALSE;
		
		if (IsFlagSet(Message::IS_OUTGOING))
		{
			m_textpart_setting = account->PreferHTMLComposing();
		}
		else
		{
			m_textpart_setting = (TextpartSetting)g_pcm2->GetIntegerPref(PrefsCollectionM2::MailBodyMode);
		}
		
    }

    if (!m_headerlist)
		m_headerlist = OP_NEW(Head, ());

	if (!m_multipart_list)
		m_multipart_list = OP_NEW(Head, ());

	if (!m_attachment_list)
		m_attachment_list = OP_NEW(Head, ());

	if (!m_headerlist || !m_multipart_list || !m_attachment_list)
	{
		OP_DELETE(m_headerlist); m_headerlist=NULL;
		OP_DELETE(m_multipart_list); m_multipart_list=NULL;
		OP_DELETE(m_attachment_list); m_attachment_list=NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

    return OpStatus::OK;
}


OP_STATUS Message::ReInit(UINT16 account_id)
{
	Destroy();
	return Init(account_id);
}


OP_STATUS Message::Init(IN UINT16 account_id, IN message_gid_t old_message_id, IN MessageTypes::CreateType &type)
{
	if (old_message_id)
	{
		m_old_message = OP_NEW(Message,());
		switch (type)
		{
			case MessageTypes::REOPEN:
				m_old_message->Init(account_id);
				if (OpStatus::IsSuccess(MessageEngine::GetInstance()->GetStore()->GetDraftData(*m_old_message, old_message_id)))
					break;
			default:
				RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->GetMessage(*m_old_message, old_message_id));
				MessageEngine::GetInstance()->GetStore()->GetMessageData(*m_old_message);
				SetCreateType(type);
		}
	}
    
	SetFlag(Message::IS_OUTGOING, TRUE);

    if (!m_old_message)
	{
		switch (type)
		{
			case MessageTypes::REPLY:
			case MessageTypes::QUICK_REPLY:
			case MessageTypes::REPLYALL:
			case MessageTypes::REPLY_TO_LIST:
			case MessageTypes::REPLY_TO_SENDER:
			case MessageTypes::REPLY_AND_FOLLOWUP:
			case MessageTypes::FOLLOWUP:
			case MessageTypes::FORWARD:
			case MessageTypes::REDIRECT:
			case MessageTypes::CANCEL_NEWSPOST:
			case MessageTypes::REOPEN:
				return OpStatus::ERR_NULL_POINTER;
		}
    }

	if (type==MessageTypes::REOPEN)
	{
		// Important to not use the same message id when it's a sent message (otherwise we would lose the sent message)
		if (!m_old_message->IsFlagSet(IS_SENT))
		{
			SetId(old_message_id);
			
		}
		// copy all flags except IS_SENT
		SetAllFlags(m_old_message->GetAllFlags());
		SetFlag(IS_SENT,FALSE);
		SetParentId(m_old_message->GetParentId());
		
	}

	//Cancel should only be allowed for sent news-messages
	if (type==MessageTypes::CANCEL_NEWSPOST &&
		!(m_old_message->IsFlagSet(Message::IS_OUTGOING) && m_old_message->IsFlagSet(Message::IS_NEWS_MESSAGE) && !m_old_message->IsFlagSet(Message::IS_CONTROL_MESSAGE)) )
	{
		return OpStatus::ERR_OUT_OF_RANGE;
	}

    OP_STATUS ret;
    if (account_id==0 && m_old_message)
        account_id = m_old_message->GetAccountId();
	
	//If we have a forwarded news-message, make sure we're using an account with SMTP
	if ((type==MessageTypes::FORWARD || type==MessageTypes::REDIRECT) &&
		(m_old_message->IsFlagSet(Message::IS_NEWS_MESSAGE) || m_old_message->IsFlagSet(Message::IS_NEWSFEED_MESSAGE)))
	{
		OpString8 smtp_server;
		Account* account_ptr = GetAccountPtr(account_id);
		if ( !account_ptr ||
			 !account_ptr->GetOutgoingBackend() ||
			 (account_ptr->GetOutgoingServername(smtp_server)!=OpStatus::OK || smtp_server.IsEmpty()) )
		{
			AccountManager* account_manager = (AccountManager*)MessageEngine::GetInstance()->GetAccountManager();
			account_id = account_manager->GetDefaultAccountId(AccountTypes::TYPE_CATEGORY_MAIL);
		}
	}

	//If account doesn't exist, try to find another one
	if (GetAccountPtr(account_id)==NULL)
	{
		AccountManager* account_manager = (AccountManager*)MessageEngine::GetInstance()->GetAccountManager();
		if (account_manager)
		{
			AccountTypes::AccountTypeCategory account_type = AccountTypes::TYPE_CATEGORY_MAIL;

			if ( (m_old_message && m_old_message->IsFlagSet(Message::IS_NEWS_MESSAGE)) ||
				 type==MessageTypes::REPLY_AND_FOLLOWUP ||
				 type==MessageTypes::FOLLOWUP ||
				 type==MessageTypes::CANCEL_NEWSPOST )
			{
				account_type = AccountTypes::TYPE_CATEGORY_NEWS;
			}

			account_id = account_manager->GetDefaultAccountId(account_type);

			// if there are no mail accounts, it has to be a news account
			if (!account_id)
			{
				account_type = AccountTypes::TYPE_CATEGORY_NEWS;
				account_id = account_manager->GetDefaultAccountId(account_type);
			}
		}
	}

    if ((ret=Init(account_id)) != OpStatus::OK)
        return ret;

	//Set flags
    if (type == MessageTypes::REDIRECT || (type==MessageTypes::REOPEN && m_old_message->IsFlagSet(Message::IS_RESENT)))
    {
        SetFlag(Message::IS_RESENT, TRUE);
	}

	Account* account_ptr = GetAccountPtr(account_id);

	switch (type)
	{
		case MessageTypes::NEW:
			if (account_ptr && 
				account_ptr->PreferHTMLComposing() == PREFER_TEXT_HTML)
			{
				m_textpart_setting = PREFER_TEXT_HTML;
			}
			else
			{
				m_textpart_setting = PREFER_TEXT_PLAIN;
			}
			break;
		case MessageTypes::REPLY:
		case MessageTypes::QUICK_REPLY:
		case MessageTypes::REPLYALL:
		case MessageTypes::REPLY_TO_LIST:
		case MessageTypes::REPLY_TO_SENDER:
		case MessageTypes::REPLY_AND_FOLLOWUP:
		case MessageTypes::FOLLOWUP:
		case MessageTypes::CANCEL_NEWSPOST:
			if ((TextpartSetting)g_pcm2->GetIntegerPref(PrefsCollectionM2::MailBodyMode) == PREFER_TEXT_HTML &&
				m_old_message->GetTextPartSetting() == PREFER_TEXT_HTML &&
				account_ptr && (account_ptr->PreferHTMLComposing() == PREFER_TEXT_HTML || account_ptr->PreferHTMLComposing() == PREFER_TEXT_PLAIN_BUT_REPLY_WITH_SAME_FORMATTING))
			{
				m_textpart_setting = PREFER_TEXT_HTML;
			}
			else
			{
				m_textpart_setting = PREFER_TEXT_PLAIN;
			}
			break;
		case MessageTypes::FORWARD:
		case MessageTypes::REDIRECT:
		case MessageTypes::REOPEN:
			m_textpart_setting = (TextpartSetting)g_pcm2->GetIntegerPref(PrefsCollectionM2::MailBodyMode);
	}

	m_is_HTML = m_textpart_setting == PREFER_TEXT_HTML;

	if (m_is_HTML && m_old_message)
	{
		Multipart* old_body_item= NULL;
		BOOL multiparts_already_present = (m_old_message->m_multipart_status==MIME_DECODING_ALL || m_old_message->m_multipart_status==MIME_DECODED_ALL);

		if ((ret=m_old_message->GetBodyItem(&old_body_item, m_textpart_setting == PREFER_TEXT_HTML)) != OpStatus::OK)
			return ret;

		if (old_body_item && old_body_item->GetContentType()==URL_TEXT_CONTENT)
			m_is_HTML = FALSE;

		if (!multiparts_already_present)
		{
			m_old_message->PurgeMultipartData(old_body_item);
			m_old_message->DeleteMultipartData(old_body_item);
		}

	}
	//Copy auto-cc/auto-bcc from account settings
	OpString auto_cc;
	OpString auto_bcc;
	if (account_ptr)
	{
		if ((ret=account_ptr->GetAutoCC(auto_cc))!=OpStatus::OK ||
			(ret=SetHeaderValue(Header::OPERAAUTOCC, auto_cc, TRUE))!=OpStatus::OK || //Values should be QuotePair decoded
			(ret=account_ptr->GetAutoBCC(auto_bcc))!=OpStatus::OK ||
			(ret=SetHeaderValue(Header::OPERAAUTOBCC, auto_bcc, TRUE))!=OpStatus::OK) //Values should be QuotePair decoded
		{
			return ret;
		}
	}

    if (type==MessageTypes::REPLY || 
		type==MessageTypes::QUICK_REPLY || 
		type==MessageTypes::REPLYALL ||
		type==MessageTypes::REPLY_AND_FOLLOWUP || 
		type==MessageTypes::REPLY_TO_LIST ||
		type==MessageTypes::REPLY_TO_SENDER)
    {
		OpString8 to, cc;
		SetPreferredRecipients(type, *m_old_message);

		//SUBJECT-header
		if ((ret=CopyHeaderValue(m_old_message, Header::SUBJECT)) != OpStatus::OK)
			return ret;
		Header* subject_header = GetHeader(Header::SUBJECT);
		if (subject_header)
		{
			if ((ret=subject_header->AddReplyPrefix()) != OpStatus::OK)
				return ret;
		}

    }

	//Move auto-cc/auto-bcc to cc/bcc
	if (account_ptr)
	{
		Header* source_header;
		Header* destination_header;
		int i;
		for (i=0; i<2; i++)
		{
			Header::HeaderType source_type=Header::UNKNOWN, destination_type=Header::UNKNOWN;
			switch(i)
			{
			case 0: source_type = Header::OPERAAUTOCC;
					destination_type = Header::CC;
					break;

			case 1: source_type = Header::OPERAAUTOBCC;
					destination_type = Header::BCC;
					break;

			default: OP_ASSERT(0); break;
			}

			source_header = GetHeader(source_type);
			destination_header = GetHeader(destination_type);

			if (source_header)
			{
				if (destination_header)
				{
					const Header::From* source_address_ptr = source_header->GetFirstAddress();
					while (source_address_ptr)
					{
						destination_header->AddAddress(*source_address_ptr);
						source_address_ptr = static_cast<Header::From*>(source_address_ptr->Suc());
					}
				}
				else
				{
					Header::HeaderValue header_value;
					if ((ret=GetHeaderValue(source_type, header_value, TRUE)) != OpStatus::OK ||
						(ret=SetHeaderValue(destination_type, header_value, TRUE)) != OpStatus::OK)
					{
						return ret;
					}
				}
			}
		}
		OpStatus::Ignore(RemoveHeader(Header::OPERAAUTOCC));
		OpStatus::Ignore(RemoveHeader(Header::OPERAAUTOBCC));
	}

    if (type==MessageTypes::FOLLOWUP || type==MessageTypes::REPLY_AND_FOLLOWUP)
    {
        //FOLLOWUP-header
        OpString8 newsgroups;
        if ((ret=CopyHeaderValue(m_old_message, Header::FOLLOWUPTO, Header::NEWSGROUPS)) != OpStatus::OK)
            return ret;
        if ((ret=GetHeaderValue(Header::NEWSGROUPS, newsgroups)) != OpStatus::OK)
            return ret;
        if (newsgroups.IsEmpty() && (ret=CopyHeaderValue(m_old_message, Header::NEWSGROUPS, Header::NEWSGROUPS)) != OpStatus::OK)
            return ret;

        //Followup-to poster?
        Header* newsgroup_header = GetHeader(Header::NEWSGROUPS);
        if (newsgroup_header && !newsgroup_header->IsEmpty())
        {
            OpString8 newsgroup;
            UINT16 newsgroup_index = 0;
			BOOL copied_address = FALSE;
            do
            {
                if ((ret=newsgroup_header->GetNewsgroup(newsgroup, newsgroup_index)) != OpStatus::OK)
                    return ret;

                if (newsgroup.CompareI("poster")==0)
                {
                    //Remove 'poster' from list of newsgroups
                    if ((ret=newsgroup_header->RemoveNewsgroup(newsgroup)) != OpStatus::OK)
                        return ret;

					if (!copied_address)
					{
						//Copy Reply-to: or From: to the To:-header
						Header* reply_to_header = m_old_message->GetHeader(Header::REPLYTO); //Reply-to: should be used instead of From: if it is present
						if ((ret=CopyHeaderValue(m_old_message, (reply_to_header && !reply_to_header->IsEmpty())?
													   Header::REPLYTO : Header::FROM, Header::TO)) != OpStatus::OK)
						{
							return ret;
						}
						copied_address = TRUE;
					}
                }
				else
				{
					newsgroup_index++;
				}
            } while (newsgroup.HasContent());
        }

        //SUBJECT-header
        if ((ret=CopyHeaderValue(m_old_message, Header::SUBJECT)) != OpStatus::OK)
            return ret;
        Header* subject_header = GetHeader(Header::SUBJECT);
        if (subject_header)
        {
            if ((ret=subject_header->AddReplyPrefix()) != OpStatus::OK)
                return ret;
        }
    }

    if (type==MessageTypes::FORWARD)
    {
        //SUBJECT-header
        if ((ret=CopyHeaderValue(m_old_message, Header::SUBJECT)) != OpStatus::OK)
            return ret;
        Header* subject_header = GetHeader(Header::SUBJECT);
        if (subject_header)
        {
            if ((ret=subject_header->AddForwardPrefix()) != OpStatus::OK)
                return ret;
        }
    }

	if (type==MessageTypes::REOPEN && !m_old_message->IsFlagSet(Message::IS_RESENT))
	{
        OpString body;
		bool is_html = true;
        RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::BCC));
		RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::CC));
		RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::FOLLOWUPTO));
		RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::INREPLYTO));
		RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::NEWSGROUPS));
		RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::ORGANIZATION));
		RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::REFERENCES));
		RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::REPLYTO));
		RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::RESENTBCC));
		RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::RESENTCC));
		RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::RESENTREPLYTO));
		RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::RESENTTO));
		RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::SUBJECT));
		RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::TO));
		RETURN_IF_ERROR(CopyHeaderValue(m_old_message, Header::XPRIORITY));
		RETURN_IF_ERROR(SetHeaderValue(Header::MESSAGEID, "", FALSE));
		RETURN_IF_ERROR(m_old_message->GetBodyText(body, FALSE, &is_html));
		RETURN_IF_ERROR(SetRawBody(body, TRUE, FALSE, TRUE));
		m_is_HTML = is_html;
	}

	// loop trough and add interesting multiparts and add them if we're redirecting, forwarding or reopening
	// also set the correct context_id if we have multiparts that are visible
	if (type!=MessageTypes::NEW)
    {
        //Copy attachments
		Head* old_multipart_list = m_old_message->GetMultipartListPtr(FALSE, m_textpart_setting == PREFER_TEXT_HTML);
        Multipart* old_multipart = (Multipart*)(old_multipart_list->First());

        OpString suggested_filename;
        OpString attachment_filename;

        while (old_multipart)
        {
            if (!old_multipart->IsMailbody())
            {
				if (old_multipart->IsVisible() && old_multipart->GetURL())
					m_context_id = old_multipart->GetURL()->GetContextId();
				// add multiparts if we are forwarding 
                if ((old_multipart->IsVisible() || type==MessageTypes::REDIRECT || type==MessageTypes::FORWARD || type==MessageTypes::REOPEN) &&
					 (((ret=old_multipart->GetSuggestedFilename(suggested_filename)) != OpStatus::OK) ||
                     ((ret=old_multipart->GetURLFilename(attachment_filename)) != OpStatus::OK) ||
					 ((ret=AddAttachment(suggested_filename, attachment_filename, old_multipart->GetURL(),old_multipart->IsVisible())) != OpStatus::OK) ))
                {
                    return ret;
                }
            }

            old_multipart = (Multipart*)(old_multipart->Suc());
        }
		SetTextDirection(m_old_message->GetTextDirection());
    }

    if (type == MessageTypes::REDIRECT || (type==MessageTypes::REOPEN && m_old_message->IsFlagSet(Message::IS_RESENT)))
    { //Duplicate the message
		OpString8 charset;
		if ((ret=m_old_message->GetCharset(charset)) != OpStatus::OK ||
			(ret=SetCharset(charset)) != OpStatus::OK)
		{
			return ret;
		}


        OpString body;
        RETURN_IF_ERROR(SetRawMessage(m_old_message->GetOriginalRawHeaders()));
		RETURN_IF_ERROR(RemoveHeader(Header::STATUS));
		RETURN_IF_ERROR(RemoveHeader(Header::BCC));

		Multipart* old_body_item= NULL;
		BOOL multiparts_already_present = (m_old_message->m_multipart_status==MIME_DECODING_ALL || m_old_message->m_multipart_status==MIME_DECODED_ALL);

		RETURN_IF_ERROR(m_old_message->GetBodyItem(&old_body_item, m_textpart_setting == PREFER_TEXT_HTML));

		if (!multiparts_already_present)
			m_old_message->PurgeMultipartData(old_body_item);

		if (old_body_item)
			old_body_item->GetBodyText(body);

		RETURN_IF_ERROR(RemoveMimeHeaders());
        
		RETURN_IF_ERROR(SetRawBody(body, TRUE, FALSE, TRUE));

        //Message must have a From:-header. If redirecting a draft, it could be missing
		Header::HeaderValue from_address;
		RETURN_IF_ERROR(m_old_message->GetHeaderValue(Header::FROM, from_address));

        if (from_address.IsEmpty())
        {
			OpString new_from_address;

            if (!account_ptr)
                return OpStatus::ERR_NULL_POINTER;

            RETURN_IF_ERROR(account_ptr->GetFormattedEmail(new_from_address, FALSE));

            RETURN_IF_ERROR(new_from_address.IsEmpty());

            RETURN_IF_ERROR(SetHeaderValue(Header::FROM, new_from_address));
        }
    }
	else if (type == MessageTypes::REOPEN /*&& !old_message->IsFlagSet(Message::IS_RESENT)*/)
	{
		//Do nothing
	}
	else if (type == MessageTypes::CANCEL_NEWSPOST) //For formatting of a cancel-message, see The Newsgroup Care Cancel Cookbook, <URL:http://www.xs4all.nl/~rosalind/faq-care.html#cancel-message>
	{
		if (!account_ptr)
			return OpStatus::ERR_NULL_POINTER;

		SetFlag(Message::IS_CONTROL_MESSAGE, TRUE);
		SetFlag(Message::IS_NEWS_MESSAGE, TRUE);

		OpString8 original_message_id, original_sender;
		Header::HeaderValue original_subject;
		if ( ((ret=m_old_message->GetHeaderValue(Header::MESSAGEID, original_message_id))!=OpStatus::OK) ||
			 ((ret=m_old_message->GetHeaderValue(Header::SENDER, original_sender))!=OpStatus::OK) ||
			 ((ret=m_old_message->GetHeaderValue(Header::SUBJECT, original_subject))!=OpStatus::OK) )
		{
			return ret;
		}

		if (original_message_id.IsEmpty())
			return OpStatus::ERR;

		OpString8 subject;
		subject.AppendFormat("cmsg cancel %s", original_message_id.CStr());
		OpString8 cancel;
		cancel.AppendFormat("cancel %s", original_message_id.CStr());
		OpString8 message_id;
		if ( ((ret=message_id.Set(original_message_id))!=OpStatus::OK) ||
			 ((ret=message_id.Insert(1, "cancel."))!=OpStatus::OK) ) // Format should be "<cancel.old_message_id>"
		{
			return ret;
		}

		OpString8 email;
		OpString formatted_email;
		if ( ((ret=account_ptr->GetEmail(email))!=OpStatus::OK) ||
			 ((ret=account_ptr->GetFormattedEmail(formatted_email, TRUE))!=OpStatus::OK) )
		{
			return ret;
		}

        if ( (subject.IsEmpty() || (ret=SetHeaderValue(Header::SUBJECT, subject))!=OpStatus::OK) ||
             (cancel.IsEmpty() || (ret=SetHeaderValue(Header::CONTROL, cancel))!=OpStatus::OK) ||
             (message_id.IsEmpty() || (ret=SetHeaderValue(Header::MESSAGEID, message_id))!=OpStatus::OK) ||
			 ((ret=SetHeaderValue(Header::SUMMARY, "own cancel by original author", FALSE))!=OpStatus::OK) ||
			 ((ret=CopyHeaderValue(m_old_message, Header::NEWSGROUPS))!=OpStatus::OK) ||
			 (formatted_email.IsEmpty() || (ret=SetHeaderValue("X-Canceled-By", formatted_email, FALSE))!=OpStatus::OK) || //FALSE here because we already escaped it in GetFormattedEmail
//			 (email.IsEmpty() || (ret=SetHeaderValue(Header::APPROVED, email))!=OpStatus::OK) || //Some servers reject articles with Approved-headers, even if it is a cancel of own message
			 ((ret=SetHeaderValue(Header::FROM, formatted_email))!=OpStatus::OK) ||
			 ((ret=CopyHeaderValue(m_old_message, (original_sender.HasContent() ? Header::SENDER : Header::FROM), Header::SENDER))!=OpStatus::OK) ||
			 ((ret=SetHeaderValue("X-Original-Subject", original_subject))!=OpStatus::OK) ||
			 ((ret=SetHeaderValue("X-No-Archive", "yes", FALSE))!=OpStatus::OK) )
		{
			if (ret==OpStatus::OK)
				ret = OpStatus::ERR;

            return ret;
		}

		OpString new_body;
		new_body.AppendFormat(UNI_L("Cancel \"%s\""), original_subject.CStr());
		if ((ret=SetRawBody(new_body, TRUE, FALSE, TRUE)) != OpStatus::OK)
			return ret;
	}
    else
	{
		//Set REPLY-TO header
		if (account_ptr)
		{
			OpString reply_to;
			if ( ((ret=account_ptr->GetReplyTo(reply_to)) != OpStatus::OK) ||
				 ((ret=SetHeaderValue(Header::REPLYTO, reply_to, TRUE)) !=OpStatus::OK) ) //Decode QuotePair here!
			{
				return ret;
			}
		}

		OpString new_body;
		if (type == MessageTypes::NEW)
		{
			if (m_is_HTML)
			{
				FontAtt mailfont;
				g_pcfontscolors->GetFont(OP_SYSTEM_FONT_HTML_COMPOSE, mailfont);
				OpString face;
#ifdef HAS_FONT_FOUNDRY
				OpFontManager* fontManager = styleManager->GetFontManager();
				fontManager->GetFamily(mailfont.GetFaceName(), face);
#else
				face.Set(mailfont.GetFaceName());
#endif //HAS_FONT_FOUNDRY

				RETURN_IF_ERROR(new_body.AppendFormat(UNI_L("<!DOCTYPE html>\r\n<html>\r\n<head>\r\n<style type=\"text/css\">body { font-family:'%s'; font-size:%dpx}</style>\r\n</head>\r\n<body><DIV><BR><BR></DIV>"), face.CStr(), mailfont.GetSize()));
			}
			else
				RETURN_IF_ERROR(new_body.Set(UNI_L("\r\n\r\n")));
		}
		else if (m_old_message)
		{
			OpString old_stylesheet;
			// Generate new REFERENCES-header
			if ((ret=CopyHeaderValue(m_old_message, Header::REFERENCES)) != OpStatus::OK)
				return ret;
			OpString8 parent_message_id;
			if ((ret=m_old_message->GetHeaderValue(Header::MESSAGEID, parent_message_id)) != OpStatus::OK)
				return ret;
			Header* references_header = GetHeader(Header::REFERENCES);
			if (references_header)
			{
				OpStatus::Ignore(references_header->AddMessageId(parent_message_id)); //Not critical if this fails (most likely because of an illegal Message-ID)
			}
			else
			{
				SetHeaderValue(Header::REFERENCES, parent_message_id);
			}

			// Copy old DISTRIBUTION-header
			if ((ret=CopyHeaderValue(m_old_message, Header::DISTRIBUTION)) != OpStatus::OK)
				return ret;

			// Clear MESSAGEID-header (A valid Message-ID should be generated when sending the message)
			if ((ret=SetHeaderValue(Header::MESSAGEID, "", FALSE)) !=OpStatus::OK)
				return ret;

			if ((ret=GenerateAttributionLine(m_old_message, type, new_body)) != OpStatus::OK)
				return ret;

			OpMisc::StripTrailingLinefeeds(new_body);

			if (m_is_HTML)
				new_body.Set(XHTMLify_string_with_BR(new_body.CStr(), new_body.Length(), FALSE, FALSE, TRUE, TRUE).CStr());

			Multipart* old_body_item= NULL;
			BOOL multiparts_already_present = (m_old_message->m_multipart_status==MIME_DECODING_ALL || m_old_message->m_multipart_status==MIME_DECODED_ALL);

			if ((ret=m_old_message->GetBodyItem(&old_body_item, m_textpart_setting == PREFER_TEXT_HTML)) != OpStatus::OK)
				return ret;

			if (!multiparts_already_present)
				m_old_message->PurgeMultipartData(old_body_item);

			OpString old_body;
			OpString quoted_body;

			if (old_body_item)
			{
				BOOL quote_whole = FALSE;
				//If user has selected text, use that instead of the full body
				if ((ret=MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->GetSelectedText(old_body)) != OpStatus::OK ||
					old_body.IsEmpty())
				{
					if (old_body_item->GetContentType()!=URL_TEXT_CONTENT && !m_is_HTML)
					{
						OpStatus::Ignore(HTMLToText::GetCurrentHTMLMailAsText(m_old_message->GetId(), old_body));
					}
					else if(old_body_item->GetContentType()==URL_TEXT_CONTENT)
					{
						if ((ret=old_body_item->GetBodyText(old_body)) != OpStatus::OK)
						{
							if (!multiparts_already_present)
								m_old_message->DeleteMultipartData(old_body_item);
							return ret;
						}
					}

					quote_whole= TRUE;
				}
				/*if (!multiparts_already_present)
					old_message->DeleteMultipartData(old_body_item);*/
				if (!m_is_HTML)
				{
					unsigned max_quote_depth = 32;
					if (account_ptr)
					{
						if (type == MessageTypes::QUICK_REPLY)
							max_quote_depth = account_ptr->GetMaxQuoteDepthOnQuickReply();
						else
							max_quote_depth = account_ptr->GetMaxQuoteDepthOnReply();
					}
					
					OpAutoPtr<OpQuote> quote (m_old_message->CreateQuoteObject(max_quote_depth));
					if (!quote.get())
						return OpStatus::ERR_NO_MEMORY;

					ret=quote->QuoteAndWrapText(quoted_body, old_body, type==MessageTypes::FORWARD ? (uni_char)0 : '>');
				}
				else
				{
					if (quote_whole)
					{
						// we need to find the style sections and the body section to include them in the reply (in the correct locations).
						OpString body;
						old_body_item->GetBodyText(body);

						// 1st solution, look for <head> and </head>
						int stylesheet_start = body.FindI(UNI_L("<HEAD>"));
						int stylesheet_end = body.FindI(UNI_L("</HEAD>"));
						int body_start = body.FindI(UNI_L("<BODY"));
						int body_end = body.FindI(UNI_L("</BODY>"));
						
						if (stylesheet_start == KNotFound || stylesheet_end == KNotFound)
						{
							// 2nd solution, look for <style and </style>, but of course, there might be more of them
							int start_pos = KNotFound;
							while (stylesheet_start != -1)
							{
								stylesheet_start = body.FindI("<STYLE", start_pos);
								if (stylesheet_start != KNotFound)
								{
									stylesheet_end = body.Find("</style>", stylesheet_start);
									if (stylesheet_end != KNotFound)
									{
										start_pos = stylesheet_end+8;
										RETURN_IF_ERROR(old_stylesheet.AppendFormat(body.SubString(stylesheet_start).CStr(), start_pos-stylesheet_start));
									}
								}
							}
													
							// if there is no <body> tag, but there was </style> tag, use that as body start position, so we don't include the <style> section twice!
							if (body_start == KNotFound && start_pos != KNotFound)
							{
								body_start = start_pos;
							}
						}
						else
						{
							stylesheet_start +=6;
							// if there is no <body> tag, use the </HEAD> position
							if (body_start == KNotFound)
								body_start = stylesheet_end + 7;
							RETURN_IF_ERROR(old_stylesheet.Set(body.SubString(stylesheet_start), stylesheet_end-stylesheet_start));
						}

						if (body_start != KNotFound)
						{
							RETURN_IF_ERROR(quoted_body.Set(body.SubString(body_start).CStr(), body_end != KNotFound ? body_end-body_start: body_end));
						}
						else
						{
							// worst case: we have to quote the whole old html
							RETURN_IF_ERROR(quoted_body.Set(body));
						}
					}
					else
					{
						RETURN_IF_ERROR(quoted_body.Set(old_body));
					}

					if (type!=MessageTypes::FORWARD)
					{
						RETURN_IF_ERROR(quoted_body.Insert(0,UNI_L("<BLOCKQUOTE style=\"margin: 0 0 0.80ex; border-left: #0000FF 2px solid; padding-left: 1ex\">")));
						RETURN_IF_ERROR(quoted_body.Append("</BLOCKQUOTE>"));
					}
				}
			}


			if (ret != OpStatus::OK)
				return ret;

			OpMisc::StripTrailingLinefeeds(quoted_body);


			if (!new_body.IsEmpty()) //If we have an attribution-line, put a CRLF-pair after it
			{
				if (m_is_HTML)
				{
					if ((ret=new_body.Append(UNI_L("<BR><BR>"))) != OpStatus::OK)
						return ret;
				}
				else
				{
					if ((ret=new_body.Append("\r\n\r\n")) != OpStatus::OK)
						return ret;
				}
			}

			if (m_is_HTML)
			{
				FontAtt mailfont;
				g_pcfontscolors->GetFont(OP_SYSTEM_FONT_HTML_COMPOSE, mailfont);
				OpString face;
#ifdef HAS_FONT_FOUNDRY
				OpFontManager* fontManager = styleManager->GetFontManager();
				fontManager->GetFamily(mailfont.GetFaceName(), face);
#else
				face.Set(mailfont.GetFaceName());
#endif //HAS_FONT_FOUNDRY

				OpString new_header;
				RETURN_IF_ERROR(new_header.AppendFormat(UNI_L("<!DOCTYPE html>\r\n<html>\r\n<head>%s\r\n<style type=\"text/css\">body { font-family:'%s'; font-size:%dpx}</style>\r\n</head>\r\n<body>"), old_stylesheet.HasContent() ? old_stylesheet.CStr() : UNI_L(""), face.CStr(), mailfont.GetSize()));
				if ((ret=new_body.Insert(0,new_header.CStr())) != OpStatus::OK)
					return ret;
			}

			if ((ret=new_body.Append(quoted_body)) != OpStatus::OK)
				return ret;

			if (m_is_HTML)
			{
				if ((ret=new_body.Append(UNI_L("<BR><BR><BR>"))) != OpStatus::OK)
					return ret;
			}
			else
			{
				if ((ret=new_body.Append("\r\n\r\n\r\n")) != OpStatus::OK)
					return ret;
			}

		}
		if ((ret=SetRawBody(new_body, FALSE, TRUE, TRUE)) != OpStatus::OK) //No need to reflow, we have already called QuoteAndWrapText above
			return ret;
    }

	if (type == MessageTypes::REOPEN && m_old_message->GetCreateType() != MessageTypes::NEW)
		SetCreateType(m_old_message->GetCreateType());

    return OpStatus::OK;
}


OP_STATUS Message::SetPreferredRecipients(const MessageTypes::CreateType reply_type, StoreMessage& old_message)
{
	// we have these cases to cover: 
    //  REPLY,						// first try to use Reply-To, then From
	//	REPLY_TO_LIST,				// first try List-Post, then X-Mailing-List, then Mailing-List, then like REPLY
	//	REPLY_TO_SENDER,			// will use the From header
	//	QUICK_REPLY,				// like reply
	//  REPLYALL,					// copying CC header, To is set to From, Reply-To and old To, with duplicate deletion

	Header::HeaderValue to, cc;
	BOOL use_fallback = FALSE;

	if (reply_type == MessageTypes::REPLY_TO_LIST && OpStatus::IsError(GetMailingListHeader(old_message, to)))
	{
		use_fallback = TRUE;
	}

    if (reply_type == MessageTypes::REPLYALL)
    {
		Header::HeaderValue from, reply_to;
		old_message.GetHeaderValue(Header::TO, to);
		old_message.GetHeaderValue(Header::CC, cc);
		old_message.GetHeaderValue(Header::FROM, from);
		old_message.GetHeaderValue(Header::REPLYTO, reply_to);
		if (from.HasContent())
		{
			if (to.HasContent())
				to.Append(UNI_L(","));
			to.Append(from.CStr());
		}
		if (reply_to.HasContent())
		{
			if (to.HasContent())
				to.Append(UNI_L(","));
			to.Append(reply_to.CStr());
		}
	}

	if (reply_type == MessageTypes::REPLY || reply_type == MessageTypes::QUICK_REPLY || use_fallback == TRUE)
	{
		if (OpStatus::IsError(old_message.GetHeaderValue(Header::REPLYTO, to)) || to.IsEmpty())
			use_fallback = TRUE;
		else
			use_fallback = FALSE; // we got it, we don't need the old from header
	}
	
	if (reply_type == MessageTypes::REPLY_TO_SENDER || use_fallback)
	{
		// fallback is the from header
		old_message.GetHeaderValue(Header::FROM, to);
	}

	if (old_message.IsFlagSet(StoreMessage::IS_OUTGOING) && reply_type != MessageTypes::REPLY_TO_SENDER)
	{
		// use the TO header, we don't want to reply to our selves
		old_message.GetHeaderValue(Header::TO, to);
	}

	SetHeaderValue(Header::TO, to);
	SetHeaderValue(Header::CC, cc);

	// accept to reply to yourself only, else remove self address
	BOOL remove_self_address = reply_type!=MessageTypes::REPLY && reply_type!=MessageTypes::QUICK_REPLY && reply_type!=MessageTypes::REPLY_TO_SENDER;
	
	OpString keep_address;
	if (remove_self_address) //Don't remove Reply-To (or From if Reply-To is not present) header
	{
		Header* keep_header = old_message.GetHeader(Header::REPLYTO) ? old_message.GetHeader(Header::REPLYTO) : old_message.GetHeader(Header::FROM);
		const Header::From* keep_address_ptr = keep_header ? keep_header->GetFirstAddress() : NULL;
		if (keep_address_ptr)
			OpStatus::Ignore(keep_address.Set(keep_address_ptr->GetAddress()));
	}

    return RemoveDuplicateRecipients(remove_self_address, keep_address);
}

OP_STATUS Message::SetAccountId(const UINT16 account_id, BOOL update_headers)
{
    if (m_account_id == account_id)
        return OpStatus::OK;

    m_account_id = account_id;
    if (update_headers)
    {
		Init(account_id); //Set charset, allow 8-bit and preferred body mode
//#pragma PRAGMAMSG("FG: Update From:-header  [20020614]")
//#pragma PRAGMAMSG("FG: Update Reply-To:-header  [20020614]")
//#pragma PRAGMAMSG("FG: Update Organization:-header  [20020614]")
//#pragma PRAGMAMSG("FG: Update X-Face:-header  [20020614]")
//#pragma PRAGMAMSG("FG: Update attribution line  [20020614]")
//#pragma PRAGMAMSG("FG: Update signature  [20020614]")
//#pragma PRAGMAMSG("FG: Update linelength  [20020614]")
    }
    return OpStatus::OK;
}


Account* Message::GetAccountPtr(UINT16 account_id) const
{
	if (!MessageEngine::GetInstance() || !MessageEngine::GetInstance()->GetAccountManager())
		return 0;

	if (!account_id)
		account_id = m_account_id;

	Account* account = 0;
	RETURN_VALUE_IF_ERROR(MessageEngine::GetInstance()->GetAccountManager()->GetAccountById(account_id, account), 0);

	return account;
}

UINT32 Message::GetPriority(int flags)
{
	if (flags & (1 << Message::HAS_PRIORITY))
	{
		if (flags & (1 << Message::HAS_PRIORITY_LOW))
		{
			if (flags & (1 << Message::HAS_PRIORITY_MAX))
			{
				return 5;
			}
			return 4;
		}
		if (flags & (1 << Message::HAS_PRIORITY_MAX))
		{
			return 1;
		}
		return 2;
	}
	return 3; // normal priority
}


OP_STATUS Message::SetPriority(UINT32 priority, bool add_header)
{
	const char* headervalue = NULL;
	switch (priority)
	{
	case 1:
		headervalue = "1 (Highest)";
		SetFlag(Message::HAS_PRIORITY,TRUE);
		SetFlag(Message::HAS_PRIORITY_LOW,FALSE);
		SetFlag(Message::HAS_PRIORITY_MAX,TRUE);
		break;
	case 2:
		headervalue = "2 (High)";
		SetFlag(Message::HAS_PRIORITY,TRUE);
		SetFlag(Message::HAS_PRIORITY_LOW,FALSE);
		SetFlag(Message::HAS_PRIORITY_MAX,FALSE);
		break;
	case 4:
		headervalue = "4 (Low)";
		SetFlag(Message::HAS_PRIORITY,TRUE);
		SetFlag(Message::HAS_PRIORITY_LOW,TRUE);
		SetFlag(Message::HAS_PRIORITY_MAX,FALSE);
		break;
	case 5:
		headervalue = "5 (Lowest)";
		SetFlag(Message::HAS_PRIORITY,TRUE);
		SetFlag(Message::HAS_PRIORITY_LOW,TRUE);
		SetFlag(Message::HAS_PRIORITY_MAX,TRUE);
		break;
	case 3:
	default:
		headervalue = "3 (Normal)";
		SetFlag(Message::HAS_PRIORITY,FALSE);
		SetFlag(Message::HAS_PRIORITY_LOW,FALSE);
		SetFlag(Message::HAS_PRIORITY_MAX,FALSE);
		break;
	}

	if (add_header)
		return SetHeaderValue(Header::XPRIORITY, headervalue);
	
	return OpStatus::OK;
}

// ----------------------------------------------------

int Message::GetPreferredStorage() const
{
	// Drafts should always be mbox pr mail
	if (IsFlagSet(Message::IS_OUTGOING) && !IsFlagSet(Message::IS_SENT))
		return AccountTypes::MBOX_PER_MAIL;

	Account* account = GetAccountPtr();
	return account ? account->GetDefaultStore() : g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailStoreType);
}


// ----------------------------------------------------

OP_STATUS Message::Send(BOOL anonymous)
{
    Account* account = GetAccountPtr();
    if (!account)
        return OpStatus::ERR_NULL_POINTER;

    return MessageEngine::GetInstance()->SendMessage(*this, anonymous);
}

OP_STATUS Message::GenerateAttributionLine(const Message* message, const MessageTypes::CreateType type, OpString& attribution_line) const
{
    if (!message)
        return OpStatus::ERR;

    OP_STATUS ret = OpStatus::OK;
    OpString attribution_format;
    Account* account = GetAccountPtr();

    OP_ASSERT(account || message->GetAccountId()!=0);

    if (account)
    {
        switch(type)
        {
        case MessageTypes::REPLY:
        case MessageTypes::QUICK_REPLY:
		case MessageTypes::REPLY_TO_LIST:
		case MessageTypes::REPLY_TO_SENDER:
        case MessageTypes::REPLYALL:           ret=account->GetReplyString(attribution_format); break;
        case MessageTypes::FOLLOWUP:
        case MessageTypes::REPLY_AND_FOLLOWUP: ret=account->GetFollowupString(attribution_format); break;
        case MessageTypes::FORWARD:            ret=account->GetForwardString(attribution_format); break;
        default: break;
        }

        if (ret!=OpStatus::OK)
            return ret;
    }

    attribution_line.Empty();
    if (attribution_format.IsEmpty()) //It should be valid to have no attribution-line
        return OpStatus::OK;

    //A single-line attribution-line SHOULD end with ":\r\n"
    uni_char* quoted_eol = uni_strstr(attribution_format.CStr(), UNI_L("\\n"));
    if (!quoted_eol)
    {
        if (attribution_format[attribution_format.Length()-1]!='\n')
            if ((ret=attribution_format.Insert(attribution_format.Length(), UNI_L("\n"))) != OpStatus::OK)
                return ret;
        if (attribution_format[max(0,attribution_format.Length()-2)]!='\r')
            if ((ret=attribution_format.Insert(max(0,attribution_format.Length()-1), UNI_L("\r"))) != OpStatus::OK)
                return ret;
        if (attribution_format[max(0,attribution_format.Length()-3)]!=':')
            if ((ret=attribution_format.Insert(max(0,attribution_format.Length()-2), UNI_L(":"))) != OpStatus::OK)
                return ret;
    }

    uni_char* temp = (uni_char*)attribution_format.CStr();

    //Fetch FROM-header
    const Header::From* sent_from = NULL;
    Header* from_header = message->GetHeader(Header::FROM);
    if (from_header)
    {
        sent_from = from_header->GetFirstAddress();
    }

    //Fetch DATE-header
    struct tm* local_sent_time = NULL;
    Header* date_header = message->GetHeader(Header::DATE);
    if (date_header)
    {
        time_t sent_time = 0;
        date_header->GetValue(sent_time);
        local_sent_time = localtime(&sent_time);
    }

    while (ret==OpStatus::OK && temp && *temp)
    {
        if (*temp=='%')
        {
            switch(*(temp+1))
            {
            case 'n': //name          (if you change this format-key, also update the code a few lines down!)
            case 'e': //e-mail        (if you change this format-key, also update the code a few lines down!)
            case 'f': //name & e-mail (if you change this format-key, also update the code a few lines down!)
                      if (sent_from)
                      {
                          if (sent_from->HasName() && ((*(temp+1))==/**/'n'/**/ || (*(temp+1))==/**/'f'/**/) )
                          {
							  RETURN_IF_ERROR(attribution_line.Append(sent_from->GetName()));

                              if ((*(temp+1))=='f')
                              {
                                  if ((ret=attribution_line.Append(" ")) != OpStatus::OK)
                                      return ret;
                              }
                          }
                          if (sent_from->HasAddress() && ((*(temp+1))==/**/'e'/**/ || (*(temp+1))==/**/'f'/**/) )
                          {
							  OpString sent_from_address;
                              if ((ret=attribution_line.Append("<"))!=OpStatus::OK ||
								  (ret=attribution_line.Append(sent_from->GetAddress()))!=OpStatus::OK ||
								  (ret=attribution_line.Append(">"))!=OpStatus::OK)
							  {
                                  return ret;
							  }
                          }
                      }
                      temp+=2;
                      break;

            case 'a': //Abbreviated weekday name
            case 'A': //Full weekday name
            case 'b': //Abbreviated month name
            case 'B': //Full month name
            case 'c': //Date and time representation appropriate for locale
            case 'd': //Day of month as decimal number (01 - 31)
            case 'H': //Hour in 24-hour format (00 - 23)
            case 'I': //Hour in 12-hour format (01 - 12)
            case 'j': //Day of year as decimal number (001 - 366)
            case 'm': //Month as decimal number (01 - 12)
            case 'M': //Minute as decimal number (00 - 59)
            case 'p': //Current locale�s A.M./P.M. indicator for 12-hour clock
            case 'S': //Second as decimal number (00 - 59)
            case 'U': //Week of year as decimal number, with Sunday as first day of week (00 - 53)
            case 'w': //Weekday as decimal number (0 - 6; Sunday is 0)
            case 'W': //Week of year as decimal number, with Monday as first day of week (00 - 53)
            case 'x': //Date representation for current locale
            case 'X': //Time representation for current locale
            case 'y': //Year without century, as decimal number (00 - 99)
            case 'Y': //Year with century, as decimal number
            case 'z': //Time-zone name          //Buggy on win32?
            case 'Z': //Time-zone abbreviation  //Buggy on win32?
                      {
                          uni_char tmp_buf[128]; /* ARRAY OK 2003-10-28 frodegill */
                          uni_char format_string[3];
                          uni_sprintf(format_string, UNI_L("%%%c"), *(temp+1));
                          if (g_oplocale->op_strftime(tmp_buf, 128, format_string, local_sent_time) != 0)
                          {
                              if ((ret=attribution_line.Append(tmp_buf)) != OpStatus::OK)
                                  return ret;
                          }
                      }
                      temp+=2;
                      break;

            case 'g': //newsgroups
                      {
                          OpString8 newsgroups;
                          OpString8 followupto;
                          if ((ret=message->GetHeaderValue(Header::NEWSGROUPS, newsgroups)) != OpStatus::OK)
                              return ret;
                          if ((ret=message->GetHeaderValue(Header::FOLLOWUPTO, followupto)) != OpStatus::OK)
                              return ret;
                          if (newsgroups.Find(followupto) != KNotFound) //Use FOLLOWUPTO if it is a substring of NEWSGROUPS
                          {
                              if ((ret=newsgroups.Set(followupto)) != OpStatus::OK)
                                  return ret;
                          }

                          if ((ret=attribution_line.Append(newsgroups)) != OpStatus::OK)
                              return ret;

                          temp+=2;
                          break;
                      }

            case ':': //Direct-access to header-values, on the format "%:Headername:"
                      {
						  Header::HeaderValue header_value;
                          OpString8 header_name;
                          uni_char* header_ptr = temp+2;
                          while (header_ptr && *header_ptr && (*header_ptr!=':' && *header_ptr!=' '))
                          {
                              header_ptr++;
                          }

                          if (header_ptr>(temp+2))
                          {
                              if ((ret=header_name.Set(temp+2, header_ptr-(temp+2))) != OpStatus::OK)
                                  return ret;

                              if ((ret=message->GetHeaderValue(header_name, header_value)) != OpStatus::OK)
                                  return ret;

                              if ((ret=attribution_line.Append(header_value)) != OpStatus::OK)
                                  return ret;

                              temp+=(header_ptr-temp+(*header_ptr==':'?1:0));
                          }
                          else
                          {
                              temp+=2+(*header_ptr==':'?1:0);
                          }
                          break;
                      }

            case '%': ret = attribution_line.Append("%"); temp+=2; break; //%% => %
            case 0:
            default:  ret = attribution_line.Append(temp++, 1); break;
            }
        }
        else if (*temp=='\\')
        {
            switch(*(temp+1))
            {
            case 'n':
				{
					uni_char* end_of_line = attribution_line.HasContent() ? const_cast<uni_char*>(attribution_line.CStr()+attribution_line.Length()-1) : NULL;
					while (end_of_line && end_of_line>=attribution_line.CStr() && *end_of_line==' ') *(end_of_line--)=0; //Remove SP at end of line, to avoid problems with format=flowed

					ret = attribution_line.Append("\r\n"); temp+=2;
				}
				break;

            case '\\': ret = attribution_line.Append("\\"); temp+=2; break;
            default:   ret = attribution_line.Append(temp++, 1); break;
            }
        }
        else
            ret = attribution_line.Append(temp++, 1);
    }
    return ret;
}


OP_STATUS Message::CopyHeaderValue(Header* source_header, Header* destination_header)
{
	OP_ASSERT(source_header && destination_header);
	if (!source_header || !destination_header)
		return OpStatus::ERR_NULL_POINTER;

	destination_header->CopyValuesFrom(*source_header);
	if (destination_header->GetType()==Header::ERR_NO_MEMORY)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

OP_STATUS Message::CopyHeaderValue(const Message* source, Header::HeaderType header_type_from, Header::HeaderType header_type_to)
{
    if (!source)
        return OpStatus::ERR;

	Header* source_header = source->GetHeader(header_type_from);
	if (!source_header)
	{
		const_cast<Message*>(source)->SetHeaderValue(header_type_from, UNI_L(""), FALSE);
		source_header = source->GetHeader(header_type_from);
	}

	Header* destination_header = GetHeader(header_type_to);
	if (!destination_header)
	{
		SetHeaderValue(header_type_to, UNI_L(""), FALSE);
		destination_header = GetHeader(header_type_to);
	}

	OP_ASSERT(source_header && destination_header);
	if (!source_header || !destination_header)
		return OpStatus::ERR_NULL_POINTER;

	return CopyHeaderValue(source_header, destination_header);
}

OP_STATUS Message::CopyHeaderValue(const Message* source, const OpStringC8& header_name_from, const OpStringC8& header_name_to)
{
    if (!source)
        return OpStatus::ERR;

	Header* source_header = source->GetHeader(header_name_from);
	if (!source_header)
	{
		const_cast<Message*>(source)->SetHeaderValue(header_name_from, UNI_L(""), FALSE);
		source_header = source->GetHeader(header_name_from);
	}

	Header* destination_header = GetHeader(header_name_to);
	if (!destination_header)
	{
		SetHeaderValue(header_name_to, UNI_L(""), FALSE);
		destination_header = GetHeader(header_name_to);
	}

	OP_ASSERT(source_header && destination_header);
	if (!source_header || !destination_header)
		return OpStatus::ERR_NULL_POINTER;

	return CopyHeaderValue(source_header, destination_header);
}

// Headers
Header* Message::GetHeader(Header::HeaderType header_type, const Header* start_search_after) const
{
    Header* header = m_headerlist?(Header*)(m_headerlist->First()):NULL;

	if (start_search_after)
		header = (Header*)(start_search_after->Suc());

    while (header && header->GetType()!=header_type)
    {
        header = (Header*)(header->Suc());
    }
    return header;
}

Header* Message::GetHeader(const OpStringC8& header_name, const Header* start_search_after) const
{
    Header::HeaderType header_type = Header::GetType(header_name);
    if (header_type!=Header::UNKNOWN)
        return GetHeader(header_type, start_search_after);

    //Custom headers will have to be searched by name
    OP_STATUS ret;
    OpString8 tmp_name;
    Header* header = m_headerlist?(Header*)(m_headerlist->First()):NULL;

	if (start_search_after)
		header = (Header*)(start_search_after->Suc());

    while (header)
    {
        if (header->GetType()==Header::UNKNOWN)
        {
            if ((ret=header->GetName(tmp_name)) != OpStatus::OK)
                return NULL;
            if (tmp_name.CompareI(header_name)==0)
                return header;
        }
        header = (Header*)(header->Suc());
    }
    return NULL;
}

OP_STATUS Message::GetCurrentRawHeaders(OpString8& buffer, BOOL& dropped_illegal_headers, BOOL append_CRLF) const
{
    OP_STATUS ret;
    OpString8 tmp_string;
    buffer.Empty();
	if(m_rawheaders)
		buffer.Reserve(strlen(m_rawheaders)); // will probably not differ too much
    dropped_illegal_headers = FALSE;
    Header* header = m_headerlist?(Header*)(m_headerlist->First()):NULL;
    while (header)
    {
        if (!header->IsEmpty() && !header->IsInternalHeader())
        {
            if ((ret=header->GetNameAndValue(tmp_string)) == OpStatus::OK)
			{
				if ((ret=buffer.Append(tmp_string)) != OpStatus::OK)
					return ret;

				if ((ret=buffer.Append("\r\n")) != OpStatus::OK)
					return ret;
			}
			else
			{
				dropped_illegal_headers = TRUE;
			}
        }
        header = (Header*)(header->Suc());
    }
    return append_CRLF ? buffer.Append("\r\n") : OpStatus::OK;
}

OP_STATUS Message::GetHeaderValue(Header::HeaderType header_type, OpString8& value, BOOL do_quote_pair_encode) const
{
    Header* header = GetHeader(header_type);
    if (header)
    {
        return header->GetValue(value, do_quote_pair_encode);
    }
    else
        value.Empty();

    return OpStatus::OK;
}

OP_STATUS Message::GetHeaderValue(const OpStringC8& header_name, OpString8& value, BOOL do_quote_pair_encode) const
{
    Header* header = GetHeader(header_name);
    if (header)
    {
        return header->GetValue(value, do_quote_pair_encode);
    }
    else
        value.Empty();

    return OpStatus::OK;
}

OP_STATUS Message::GetHeaderValue(Header::HeaderType header_type, Header::HeaderValue& value, BOOL do_quote_pair_encode) const
{
    Header* header = GetHeader(header_type);
    if (header)
    {
        return header->GetValue(value, do_quote_pair_encode);
    }
	else
		value.Empty();

    return OpStatus::OK;
}

OP_STATUS Message::GetHeaderValue(const OpStringC8& header_name, Header::HeaderValue& value, BOOL do_quote_pair_encode) const
{
    Header* header = GetHeader(header_name);
    if (header)
    {
        return header->GetValue(value, do_quote_pair_encode);
    }
	else
		value.Empty();

    return OpStatus::OK;
}

OP_STATUS Message::GetDateHeaderValue(Header::HeaderType header_type, time_t& time_utc) const
{
    Header* header = GetHeader(header_type);
    if (header)
    {
        header->GetValue(time_utc);
    }
    else
	{
        time_utc = 0;
	}

    return OpStatus::OK;
}

OP_STATUS Message::GetDateHeaderValue(const OpStringC8& header_name, time_t& time_utc) const
{
    Header* header = GetHeader(header_name);
    if (header)
    {
        header->GetValue(time_utc);
    }
    else
	{
        time_utc = 0;
	}

    return OpStatus::OK;
}

OP_STATUS FindListAddress(const StoreMessage& message,Header::HeaderType header, const char* prefix, const char* postfix, Header::HeaderValue& list_address)
{
  Header::HeaderValue full_value;
  RETURN_IF_ERROR(message.GetHeaderValue(header, full_value));
  
  if (full_value.IsEmpty())
	  return OpStatus::ERR;

  INT32 start = full_value.Find(prefix);
  if (start == KNotFound)
	  return OpStatus::ERR;
  start += op_strlen(prefix);

  INT32 length = full_value.SubString(start).Find(postfix);
  if (length == KNotFound)
    return OpStatus::ERR;
  
  return list_address.Set(full_value.SubString(start).CStr(), length);
}

OP_STATUS Message::GetMailingListHeader(const StoreMessage &message, Header::HeaderValue &value)
{
	// first try to get the header Mailman uses, then the one W3C uses, then the one Yahoo Groups use
	// List-Post most often have this syntax: <mailto:%s>
	// use X-Mailing-list (eg. X-Mailing-List: <html-tidy@w3.org> archive/latest/6725)
	// use Mailing-List (eg. Mailing-List: list smartgit@yahoogroups.com; contact smartgit-owner@yahoogroups.com)

	if (OpStatus::IsSuccess(FindListAddress(message, Header::LISTPOST, "<mailto:", ">", value)) ||
		OpStatus::IsSuccess(FindListAddress(message, Header::XMAILINGLIST, "<", ">", value)) ||
		OpStatus::IsSuccess(FindListAddress(message, Header::MAILINGLIST, "list ", ";", value)))
		return OpStatus::OK;

	return OpStatus::ERR;
}

const uni_char* Message::GetNiceFrom() const
{
	Header* header = IsFlagSet(Message::IS_OUTGOING) ? GetHeader(Header::TO) : GetHeader(Header::FROM);
	const Header::From* from = header ? header->GetFirstAddress() : NULL;

	if (!from)
		return NULL;

	if (from->GetName().HasContent())
		return from->GetName().CStr();
	else
		return from->GetAddress().CStr();
}


OP_STATUS Message::CopyCurrentToOriginalHeaders(BOOL allow_skipping_illegal_headers)
{
	OpString8 temp_buffer;
	BOOL dropped_illegal_headers = FALSE;

	RETURN_IF_ERROR(GetCurrentRawHeaders(temp_buffer, dropped_illegal_headers, FALSE));

	// If we got no (new) raw headers, return immediately
	if (temp_buffer.IsEmpty())
		return OpStatus::OK;

	if (dropped_illegal_headers && !allow_skipping_illegal_headers)
		return OpStatus::ERR;

	int header_len = temp_buffer.Length();

	// calculate the amount to be added to m_localmessagesize; if header used to be empty, add 2 because
	// m_localmessagesize = (m_rawheaders ? op_strlen(m_rawheaders)+2 : 0) + op_strlen(m_rawbody)
	int header_len_diff = header_len;
	if (m_rawheaders)
		header_len_diff -= op_strlen(m_rawheaders); // size_t is unsigned, so ? op_strlen() : -2 is uncouth.
	else
		header_len_diff += 2;

	RemoveRawMessage(FALSE, TRUE);
	m_rawheaders = OP_NEWA(char, header_len+1);
	if (!m_rawheaders)
		return OpStatus::ERR_NO_MEMORY;

	op_memcpy(m_rawheaders, temp_buffer.CStr(), header_len + 1);
	m_localmessagesize += header_len_diff;

	OpStatus::Ignore(RestartAsyncRFC822ToUrl());
	return OpStatus::OK;
}

OP_STATUS Message::SetHeaderValue(Header::HeaderType header_type, const OpStringC8& value, BOOL do_quote_pair_decode, int* parseerror_start, int* parseerror_end)
{
    if (header_type==Header::UNKNOWN)
        return OpStatus::ERR;

    Header* header = GetHeader(header_type);
    if (header)
    {
        return header->SetValue(value, do_quote_pair_decode, parseerror_start, parseerror_end);
    }
    else
    {
        return AddHeader(header_type, value, do_quote_pair_decode, parseerror_start, parseerror_end);
    }
}

OP_STATUS Message::SetHeaderValue(const OpStringC8& header_name, const OpStringC8& value, BOOL do_quote_pair_decode, int* parseerror_start, int* parseerror_end)
{
    if (header_name.IsEmpty())
        return OpStatus::ERR;

    Header* header = GetHeader(header_name);
    if (header)
    {
        return header->SetValue(value, do_quote_pair_decode, parseerror_start, parseerror_end);
    }
    else
    {
        return AddHeader(header_name, value, do_quote_pair_decode, parseerror_start, parseerror_end);
    }
}

OP_STATUS Message::SetHeaderValue(Header::HeaderType header_type, const OpStringC16& value, BOOL do_quote_pair_decode, int* parseerror_start, int* parseerror_end)
{
    if (header_type==Header::UNKNOWN)
        return OpStatus::ERR;

    Header* header = GetHeader(header_type);
    if (header)
    {
        return header->SetValue(value, do_quote_pair_decode, parseerror_start, parseerror_end);
    }
    else
    {
        return AddHeader(header_type, value, do_quote_pair_decode, parseerror_start, parseerror_end);
    }
}

OP_STATUS Message::SetHeaderValue(const OpStringC8& header_name, const OpStringC16& value, BOOL do_quote_pair_decode, int* parseerror_start, int* parseerror_end)
{
    if (header_name.IsEmpty())
        return OpStatus::ERR;

    Header* header = GetHeader(header_name);
    if (header)
    {
        return header->SetValue(value, do_quote_pair_decode, parseerror_start, parseerror_end);
    }
    else
    {
        return AddHeader(header_name, value, do_quote_pair_decode, parseerror_start, parseerror_end);
    }
}

OP_STATUS Message::SetHeaderValue(Header::HeaderType header_type, UINT32 value)
{
    if (header_type==Header::UNKNOWN)
        return OpStatus::ERR;

    Header* header = GetHeader(header_type);
    if (header)
    {
        return header->SetValue(value);
    }
    else
    {
        OP_STATUS ret;
        OpString8 dummy;
        if ((ret=AddHeader(header_type, dummy, FALSE)) != OpStatus::OK)
            return ret;
        header = GetHeader(header_type);
        return header?header->SetValue(value):OpStatus::ERR;
    }
}

OP_STATUS Message::SetHeadersFromMimeHeaders(const OpStringC8& headers)
{
	if (headers.IsEmpty())
		return OpStatus::OK;

	QuickMimeParser parser;
	OpString8 multipart, content_type, content_transfer_encoding;
	
	// find headers we need
	RETURN_IF_ERROR(multipart.AppendFormat("\r\n%s", headers.CStr()));
	
	parser.GetHeaderValueFromMultipart(multipart, "content-type", content_type);
	parser.GetHeaderValueFromMultipart(multipart, "content-transfer-encoding", content_transfer_encoding);
	
	SetHeaderValue(Header::CONTENTTYPE, content_type);
	SetHeaderValue(Header::CONTENTTRANSFERENCODING, content_transfer_encoding);
	
	return OpStatus::OK;
}

OP_STATUS Message::SetDateHeaderValue(Header::HeaderType header_type, time_t time_utc)
{
    if (header_type==Header::UNKNOWN)
        return OpStatus::ERR;

    Header* header = GetHeader(header_type);
    if (header)
    {
        header->SetDateValue(time_utc);
		return OpStatus::OK;
    }
    else
    {
        OP_STATUS ret;
        OpString8 dummy;
        if ((ret=AddHeader(header_type, dummy, FALSE)) != OpStatus::OK)
            return ret;
        header = GetHeader(header_type);
		if (header)
		{
			header->SetDateValue(time_utc);
			return OpStatus::OK;
		}
        return OpStatus::ERR_NULL_POINTER;
    }
}

OP_STATUS Message::SetDateHeaderValue(const OpStringC8& header_name, time_t time_utc)
{
    if (header_name.IsEmpty())
        return OpStatus::ERR;

    Header* header = GetHeader(header_name);
    if (header)
    {
        header->SetDateValue(time_utc);
		return OpStatus::OK;
    }
    else
    {
        OP_STATUS ret;
        OpString8 dummy;
        if ((ret=AddHeader(header_name, dummy, FALSE)) != OpStatus::OK)
            return ret;
        header = GetHeader(header_name);
		if (header)
		{
			header->SetDateValue(time_utc);
			return OpStatus::OK;
		}
        return OpStatus::ERR_NULL_POINTER;
    }
}

OP_STATUS Message::GetFromAddress(OpString& address, BOOL do_quote_pair_encode)
{
	address.Empty();
	Header* from_header = GetHeader(IsFlagSet(Message::IS_RESENT) ? Header::RESENTFROM : Header::FROM);
	if (from_header)
	{
		const Header::From* from_address = from_header->GetFirstAddress();
		if (from_address)
		{
			RETURN_IF_ERROR(address.Set(from_address->GetAddress()));
		}
	}
//#pragma PRAGMAMSG("FG: do_quote_pair_encode")
	return OpStatus::OK;
}


OP_STATUS Message::RemoveDuplicateRecipients(BOOL remove_self, const OpStringC& email_to_keep)
{
    OP_STATUS ret;
	BOOL email_to_keep_found = FALSE;

    Header* auto_bcc_header = GetHeader(Header::OPERAAUTOBCC);
    Header* auto_cc_header = GetHeader(Header::OPERAAUTOCC);
    Header* bcc_header = GetHeader(IsFlagSet(Message::IS_RESENT) ? Header::RESENTBCC : Header::BCC);
    Header* to_header = GetHeader(IsFlagSet(Message::IS_RESENT) ? Header::RESENTTO : Header::TO);
    Header* cc_header = GetHeader(IsFlagSet(Message::IS_RESENT) ? Header::RESENTCC : Header::CC);
	Header* current_remove_header;

	/*
	** Remove duplicates
	*/
	BOOL current_is_email_to_keep;
    int keep_level, remove_level;
    const Header::From* keep_address;
	const Header::From* next_keep_address;
	Header::From* remove_address;
	const Header::From* next_remove_address;
    for (keep_level=0; keep_level<5; keep_level++)
    {
        switch (keep_level)
        {
        case 0: keep_address = auto_bcc_header ? auto_bcc_header->GetFirstAddress() : NULL; break;
        case 1: keep_address = auto_cc_header ? auto_cc_header->GetFirstAddress() : NULL; break;
        case 2: keep_address = bcc_header ? bcc_header->GetFirstAddress() : NULL; break;
        case 3: keep_address = to_header ? to_header->GetFirstAddress() : NULL; break;
        case 4: keep_address = cc_header ? cc_header->GetFirstAddress() : NULL; break;
        default: keep_address = NULL; break;
        }

        while (keep_address)
        {
            next_keep_address = (Header::From*)(keep_address->Suc());

			current_is_email_to_keep = keep_address->CompareAddress(email_to_keep)==0;
			if (current_is_email_to_keep && !email_to_keep_found)
			{
				email_to_keep_found = TRUE; //Avoid keeping the address multiple times
			}
			else
			{
				for (remove_level=keep_level; remove_level<5; remove_level++)
				{
					switch(remove_level)
					{
					case 0: current_remove_header = auto_bcc_header; break;
					case 1: current_remove_header = auto_cc_header; break;
					case 2: current_remove_header = bcc_header; break;
					case 3: current_remove_header = to_header; break;
					case 4: current_remove_header = cc_header; break;
					default: current_remove_header = NULL; break;
					}

					if (!current_remove_header)
						continue;

					//Small hack to remove email_to_keep if we have already kept it
					if (current_is_email_to_keep && remove_level==keep_level)
					{
						next_keep_address = keep_address;
					}

					remove_address = const_cast<Header::From*>(remove_level==keep_level ? next_keep_address : current_remove_header->GetFirstAddress());

					while (remove_address && keep_address)
					{
						next_remove_address = (Header::From*)(remove_address->Suc());

						if (remove_address->CompareAddress(keep_address->GetAddress())==0) //We have a duplicate. Remove it
						{
							if (remove_address==next_keep_address) //We are deleting next reference. Update to next object
							{
								next_keep_address = (Header::From*)(next_keep_address->Suc());
							}

							if (keep_address == remove_address)
								keep_address = NULL;

							if ((ret=current_remove_header->DeleteAddress(remove_address)) != OpStatus::OK)
								return ret;
						}

						remove_address = const_cast<Header::From*>(next_remove_address);
					}
				}
			}

            keep_address = next_keep_address;
        }
    }

	/*
	** Remove self
	*/
	AccountManager* account_manager = (AccountManager*)MessageEngine::GetInstance()->GetAccountManager();
	if (remove_self && account_manager)
	{
		OpString self_address;
		Account* account;
		int i=0;
		while ((account=account_manager->GetAccountByIndex(i++)) != NULL)
		{
			if ((ret=account->GetEmail(self_address)) != OpStatus::OK)
				return ret;

			if (self_address.IsEmpty())
				continue;

			for (remove_level=2; remove_level<5; remove_level++) //Don't remove level 0 and 1 (auto-bcc and auto-cc), user has explisitly given this
			{
				switch(remove_level)
				{
				//Can never be 0 or 1
				case 2: current_remove_header = bcc_header; break;
				case 3: current_remove_header = to_header; break;
				case 4: current_remove_header = cc_header; break;
				default: current_remove_header = NULL; break;
				}

				if (!current_remove_header)
					continue;

				remove_address = const_cast<Header::From*>(current_remove_header->GetFirstAddress());

				while (remove_address)
				{
					next_remove_address = (Header::From*)(remove_address->Suc());

					if (remove_address->CompareAddress(self_address)==0 && //We have a match. Remove it
						remove_address->CompareAddress(email_to_keep)!=0)
					{
						if ((ret=current_remove_header->DeleteAddress(remove_address)) != OpStatus::OK)
							return ret;
					}

					remove_address = const_cast<Header::From*>(next_remove_address);
				}
			}
		}
	}
    return OpStatus::OK;
}

OP_STATUS Message::GetMessageId(OpString8& message_id) const
{
	Header::HeaderValue value;
	RETURN_IF_ERROR(GetMessageId(value));

	return message_id.Set(value.CStr());
}

OP_STATUS Message::AddHeader(Header::HeaderType header_type, const OpStringC8& value, BOOL do_quote_pair_decode, int* parseerror_start, int* parseerror_end)
{
    if (header_type==Header::UNKNOWN)
        return OpStatus::ERR;

    if (!m_headerlist)
        return OpStatus::ERR_NULL_POINTER;

    Header* header = OP_NEW(Header, (this, header_type));
    if (!header)
        return OpStatus::ERR_NO_MEMORY;

    header->Into(m_headerlist);

	if (header_type == Header::XPRIORITY)
		SetPriority(value[0] - '0', false);

	return header->SetValue(value, do_quote_pair_decode, parseerror_start, parseerror_end);
}

OP_STATUS Message::AddHeader(const OpStringC8& header_name, const OpStringC8& value, BOOL do_quote_pair_decode, int* parseerror_start, int* parseerror_end)
{
    Header::HeaderType header_type = Header::GetType(header_name);
    if (header_type!=Header::UNKNOWN)
        return AddHeader(header_type, value, do_quote_pair_decode, parseerror_start, parseerror_end);

    if (!m_headerlist)
        return OpStatus::ERR_NULL_POINTER;

    Header* header = OP_NEW(Header, (this));
    if (!header)
        return OpStatus::ERR_NO_MEMORY;

    OP_STATUS ret;
    if ((ret=header->SetName(header_name)) != OpStatus::OK)
    {
        OP_DELETE(header);
        return ret;
    }

    header->Into(m_headerlist);

    return header->SetValue(value, do_quote_pair_decode, parseerror_start, parseerror_end);
}

OP_STATUS Message::AddHeader(Header::HeaderType header_type, const OpStringC16& value, BOOL do_quote_pair_decode, int* parseerror_start, int* parseerror_end)
{
    if (header_type==Header::UNKNOWN)
        return OpStatus::ERR;

    if (!m_headerlist)
        return OpStatus::ERR_NULL_POINTER;

    Header* header = OP_NEW(Header, (this, header_type));
    if (!header)
        return OpStatus::ERR_NO_MEMORY;

	header->Into(m_headerlist);

    return header->SetValue(value, do_quote_pair_decode, parseerror_start, parseerror_end);
}

OP_STATUS Message::AddHeader(const OpStringC8& header_name, const OpStringC16& value, BOOL do_quote_pair_decode, int* parseerror_start, int* parseerror_end)
{
    Header::HeaderType header_type = Header::GetType(header_name);
    if (header_type!=Header::UNKNOWN)
        return AddHeader(header_type, value, do_quote_pair_decode, parseerror_start, parseerror_end);

    if (!m_headerlist)
        return OpStatus::ERR_NULL_POINTER;

    Header* header = OP_NEW(Header, (this));
    if (!header)
        return OpStatus::ERR_NO_MEMORY;

    OP_STATUS ret;
    if ((ret=header->SetName(header_name)) != OpStatus::OK)
    {
        OP_DELETE(header);
        return ret;
    }

    header->Into(m_headerlist);

    return header->SetValue(value, do_quote_pair_decode, parseerror_start, parseerror_end);
}

OP_STATUS Message::RemoveHeader(Header::HeaderType header_type)
{
    if (header_type==Header::UNKNOWN)
        return OpStatus::ERR;

    Header* header;
    while ((header = GetHeader(header_type)) != NULL)
    {
        header->Out();
        OP_DELETE(header);
    }
    return OpStatus::OK;
}

OP_STATUS Message::RemoveHeader(const OpStringC8& header_name)
{
    Header* header;
	while ((header=GetHeader(header_name)) != NULL)
    {
        header->Out();
        OP_DELETE(header);
    }
    return OpStatus::OK;
}

void Message::ClearHeaderList()
{
    if (m_headerlist)
        m_headerlist->Clear();
}


void Message::RemoveXOperaStatusHeader()
{
	OpStatus::Ignore(RemoveHeader("X-Opera-Status"));
}

void Message::RemoveBccHeader()
{
	OpStatus::Ignore(RemoveHeader(Header::BCC));
	OpStatus::Ignore(RemoveHeader(Header::RESENTBCC));
}

BOOL Message::SetAttachmentFlags(MediaType type, MediaSubtype subtype)
{
	switch (type)
	{
		case TYPE_APPLICATION:
			switch (subtype)
			{
				case SUBTYPE_ARCHIVE:
					SetFlag(Message::HAS_ZIP_ATTACHMENT, TRUE);
					break;
				case SUBTYPE_OGG:
					SetFlag(Message::HAS_AUDIO_ATTACHMENT, TRUE);
					break;
			}
			break;
		case TYPE_AUDIO:
			SetFlag(Message::HAS_AUDIO_ATTACHMENT, TRUE);
			break;
		case TYPE_IMAGE:
			SetFlag(Message::HAS_IMAGE_ATTACHMENT, TRUE);
			break;
		case TYPE_VIDEO:
			SetFlag(Message::HAS_VIDEO_ATTACHMENT, TRUE);
			break;
	}

	SetFlag(Message::HAS_ATTACHMENT, TRUE);

	return TRUE;
}

// Multipart
OP_STATUS Message::ConvertMultipartToAttachment()
{
    OP_STATUS ret;
    OpString body;
	m_is_HTML = true;
    if ((ret=GetBodyText(body, FALSE, &m_is_HTML)) != OpStatus::OK)
        return ret;

    Head* multipart_list = GetMultipartListPtr(TRUE);
    Multipart* multipart = (Multipart*)(multipart_list->First());

    OpString suggested_filename;
    OpString attachment_filename;

    RemoveAllAttachments(); //Start from scratch
    while (multipart)
    {
        if (!multipart->IsMailbody())
        {
			if (multipart->IsVisible() && multipart->GetURL())
					m_context_id = multipart->GetURL()->GetContextId();
            if ( ((ret=multipart->GetSuggestedFilename(suggested_filename)) != OpStatus::OK) ||
                 ((ret=multipart->GetURLFilename(attachment_filename)) != OpStatus::OK) ||
				 ((ret=AddAttachment(suggested_filename, attachment_filename, multipart->GetURL(), multipart->IsVisible())) != OpStatus::OK) )
            {
                return ret;
            }
        }

        multipart = (Multipart*)(multipart->Suc());
    }

    //We don't want the old Content-type header hanging around (it might be multipart when this message is now converted to singlepart)
    OpString8 current_headers;
	BOOL dummy_dropped_illegal_headers;
    if ( ((ret=RemoveMimeHeaders()) != OpStatus::OK) ||
        ((ret=RemoveHeader(Header::CONTENTTYPE)) != OpStatus::OK) ||
         ((ret=GetCurrentRawHeaders(current_headers, dummy_dropped_illegal_headers, FALSE)) != OpStatus::OK) ||
         ((ret=SetRawMessage(current_headers.CStr())) != OpStatus::OK) )
    {
        return ret;
    }

    //Set the body. The rest of the multiparts are now in the attachment-list
    if ((ret=SetRawBody(body, FALSE)) != OpStatus::OK)
        return ret;

    //Just to be sure to have correct content-type and charset
    OpString8 dummy_contenttype;
    return SetContentType(dummy_contenttype);
}

OP_STATUS Message::MimeDecodeMessage(BOOL ignore_content, BOOL body_only, BOOL prefer_html)
{
    if (!m_mime_decoder)
    {
        m_multipart_status = MIME_NOT_DECODED;
        m_mime_decoder = OP_NEW(Decoder, ());
        if (!m_mime_decoder)
            return OpStatus::ERR_NO_MEMORY;
    }

    //Are we already decoding requested information?
    if ( m_multipart_status==MIME_DECODING_ALL || m_multipart_status==MIME_DECODED_ALL ||
		 (ignore_content && !body_only && (m_multipart_status==MIME_DECODING_HEADERS || m_multipart_status==MIME_DECODED_HEADERS)) )
    {
//#pragma PRAGMAMSG("FG: When listeners are implemented, report all parts in m_multipart_list back to listener here")
        return OpStatus::OK;
    }

	BOOL redecode = ((!ignore_content || body_only) && (m_multipart_status==MIME_DECODING_HEADERS || m_multipart_status==MIME_DECODED_HEADERS));
    if (redecode || m_multipart_status==MIME_NOT_DECODED)
    {
        //Remove any old parsed parts
        PurgeMultipartData();
        m_multipart_status = MIME_NOT_DECODED;
		if (m_multipart_list)
			m_multipart_list->Clear();
    }

    if (redecode)
    {
        m_mime_decoder->SignalReDecodeMessage();
    }

    OP_STATUS ret;
	if ((ret=CopyCurrentToOriginalHeaders())!=OpStatus::OK)
        return ret;

	m_multipart_status = ignore_content ? (body_only ? MIME_DECODING_BODY : MIME_DECODING_HEADERS) : MIME_DECODING_ALL;

    if ((ret=m_mime_decoder->DecodeMessage(	m_rawheaders, m_rawbody, m_localmessagesize, ignore_content, body_only,
											!prefer_html, TRUE, m_myself,
											this)) != OpStatus::OK)
    {
        m_mime_decoder->DecodingStopped();
        PurgeMultipartData();
        m_multipart_status = MIME_NOT_DECODED;
        m_multipart_list->Clear();
        return ret;
    }

    return OpStatus::OK;
}

void Message::PurgeMultipartData(Multipart* item_to_keep)
{
	BOOL keep_body = FALSE;
	if (m_multipart_status==MIME_DECODING_HEADERS || m_multipart_status==MIME_DECODING_ALL || m_multipart_status==MIME_DECODING_BODY)
		return; //Don't purge while decoding

	if (!m_multipart_list)
		return;
	Multipart* item = (Multipart*)(m_multipart_list->First());
	while (item)
	{
		if (item != item_to_keep)
		{
			item->DeleteDatabuffer();

			if (m_multipart_status==MIME_DECODED_ALL || (m_multipart_status==MIME_DECODED_BODY && !keep_body)) //We don't have all bodies anymore
				m_multipart_status = MIME_DECODED_HEADERS;
		}
		else if (item->IsMailbody())
		{
			m_multipart_status = MIME_DECODED_BODY;
			keep_body = TRUE;
		}
		item = (Multipart*)(item->Suc());
	}
}

void Message::DeleteMultipartData(Multipart* item_to_delete)
{
	if (!item_to_delete)
		return;

	item_to_delete->DeleteDatabuffer();

	if (m_multipart_status==MIME_DECODED_ALL || (m_multipart_status == MIME_DECODED_BODY && item_to_delete->IsMailbody())) //We don't have all bodies anymore
		m_multipart_status = MIME_DECODED_HEADERS;
}

OP_STATUS Message::OnDecodedMultipart(URL* url, const OpStringC8& charset, const OpStringC& filename, int size, BYTE* data, BOOL is_mailbody, BOOL is_visible)
{
    OP_ASSERT(m_multipart_list);

    Multipart* multipart = OP_NEW(Multipart, ());
    if (!multipart)
        return OpStatus::ERR_NO_MEMORY;

    OP_STATUS ret;
	if ((ret=multipart->SetData(url, charset, filename, size, data, is_mailbody, is_visible)) != OpStatus::OK)
    {
        OP_DELETE(multipart);
        return ret;
    }

    multipart->Into(m_multipart_list);
    return OpStatus::OK;
}

void Message::OnFinishedDecodingMultiparts(OP_STATUS status)
{
	if (m_multipart_status==MIME_DECODING_HEADERS)
		m_multipart_status = MIME_DECODED_HEADERS;
	else if (m_multipart_status==MIME_DECODING_ALL)
		m_multipart_status = MIME_DECODED_ALL;
	else if (m_multipart_status==MIME_DECODING_BODY)
		m_multipart_status = MIME_DECODED_BODY;
//#pragma PRAGMAMSG("When list of listeners is implemented, they should be notified the status in this event")
}

void Message::OnRestartDecoding()
{
    PurgeMultipartData();
    m_multipart_status = MIME_NOT_DECODED;
    m_multipart_list->Clear();
}


Head* Message::GetMultipartListPtr(BOOL ignore_content, BOOL prefer_HTML)
{
    if (MimeDecodeMessage(ignore_content, FALSE, prefer_HTML) != OpStatus::OK)
        return NULL;

//#pragma PRAGMAMSG("FG: When decoding is made asynchronous, the following code will have to go into a listener")

    return m_multipart_list;
}


OP_STATUS Message::QuickMimeDecode()
{
	// Preparation
	if (!m_rawbody)
		return OpStatus::OK;

	// Check master Content-Type
	OpString8 content_type;
	RETURN_IF_ERROR(GetHeaderValue(Header::CONTENTTYPE, content_type));

	// Do the attachment check
	QuickMimeParser                           parser;
	OpAutoVector<QuickMimeParser::Attachment> attachments;
	RETURN_IF_ERROR(parser.GetAttachmentType(content_type, "", m_rawbody, attachments));

	// Set flags on message
	for (unsigned i = 0; i < attachments.GetCount(); i++)
	{
		SetAttachmentFlags(attachments.Get(i)->media_type, attachments.Get(i)->media_subtype);
	}

	return OpStatus::OK;
}


// Body
OP_STATUS Message::SetRawMessage(const char* buffer, BOOL force_content, BOOL strip_body, char* dont_copy_buffer, int dont_copy_buffer_length)
{
	// TODO Do something with dont_copy_buffer_length!

	RemoveRawMessage();
	ClearHeaderList();
    PurgeMultipartData();
    m_multipart_status = MIME_NOT_DECODED;
    m_multipart_list->Clear();

	if (dont_copy_buffer)
	{
		// immediately take ownership, so that even on error this string is owned by the message
		m_in_place_body = dont_copy_buffer;
		buffer = dont_copy_buffer;
	}

    if (!buffer)
        return OpStatus::OK;

    if (op_strnicmp(buffer, "From ", 5) ==0 ) //Is the first line the start of a mbox file?
    {
		buffer += op_strcspn(buffer, "\r\n");
		buffer += SkipNewline(buffer);
    }

	// Find separation between headers and body, in the form of two newlines: "\r\n\r\n" or "\r\r" or "\n\n" 
	// or also "\n\r\n"  (DSK-257686)
	const char* body_start = 0;
	const char* header_end = buffer + op_strcspn(buffer, "\r\n");

	while (*header_end && op_strncmp(header_end, "\r\n\r\n", 4) &&
						  op_strncmp(header_end, "\r\r",     2) &&
						  op_strncmp(header_end, "\n\r\n",   3) &&
						  op_strncmp(header_end, "\n\n",     2) )
	{
		header_end += SkipNewline(header_end);
		header_end += op_strcspn(header_end, "\r\n");
	}

	if (*header_end)
	{
		// Headers end after the first newline
		header_end += SkipNewline(header_end);

		// Body starts after the second newline
		body_start = header_end + SkipNewline(header_end);
	}

    int headers_size = header_end - buffer;
	if (m_in_place_body)
	{
		m_in_place_body_offset = buffer - m_in_place_body;
		m_rawheaders = m_in_place_body + m_in_place_body_offset;
	}
	else
	{
		m_rawheaders = OP_NEWA(char, headers_size+1);
		if (!m_rawheaders)
			return OpStatus::ERR_NO_MEMORY;
		op_strlcpy(m_rawheaders, buffer, headers_size + 1);
	}
    m_rawheaders[headers_size] = 0;
	//OpStatus::Ignore(RestartAsyncRFC822ToUrl()); //Not needed here, will be called in SetRawBody

    OP_STATUS ret;
    OpString8 header_name;
    OpString8 header_value;
    const char* header_start = m_rawheaders;
    const char* tmp;
    BOOL done;
    //Parse headers
    while (header_start && *header_start)
    {
		if (strni_eq(header_start, ">FROM ", 6)) //To avoid bug#141122
		{
			if ((ret=header_name.Set("X-Foo")) != OpStatus::OK)
				return ret;

			header_start += 6;
		}
		else
		{
        tmp = header_start + op_strcspn(header_start, ":"); //Don't search for ": ", as empty headers might be "<header>:\n" (note the lack of space)
        if (!*tmp)
            break;

        if ((ret=header_name.Set(header_start, tmp-header_start)) != OpStatus::OK)
            return ret;

			header_start = ++tmp;
		}

		while (*header_start==' ') header_start++;

        done = FALSE;

		StreamBuffer<char> header_value;
		RETURN_IF_ERROR(header_value.Reserve(1024));

        while (!done)
		{
			tmp = header_start + op_strcspn(header_start, "\r\n");

			if (header_value.GetFilled() > 0 && *(header_value.GetData() + header_value.GetFilled() - 1) != ' ') //Make sure there is whitespace between folded lines
			{
				RETURN_IF_ERROR(header_value.Append(" ", 1));
			}

			RETURN_IF_ERROR(header_value.Append(header_start, tmp - header_start));

			tmp += SkipNewline(tmp);

			if (*tmp==' ' || *tmp=='\t')
			{
				while (*tmp==' ' || *tmp=='\t') tmp++;
			}
			else
			{
				done = TRUE;
			}
			header_start = tmp;
		}
		AddHeader(header_name, header_value.GetData(), TRUE); //Return-value deliberatly skipped here. If an error occurs, continue parsing the rest of the headers
	}

	if ((ret=UpdateCharsetFromContentType()) != OpStatus::OK)
		return ret;

	if (!strip_body)
	{
		//Set body
		if ((body_start && *body_start) || !force_content)
		{
			if (m_in_place_body)
				RETURN_IF_ERROR(SetRawBody(NULL, FALSE, m_in_place_body + (body_start - m_in_place_body)));
			else
				RETURN_IF_ERROR(SetRawBody(body_start, FALSE));
		}
		else
		{
			RETURN_IF_ERROR(SetRawBody("\r\n", FALSE));
		}
	}

    return OpStatus::OK;
}


OP_STATUS Message::GetRawMessage(OpString8& buffer, BOOL allow_skipping_illegal_headers, BOOL include_bcc, BOOL convert_to_mbox_format) const
{
    OP_STATUS ret;
    buffer.Empty();
    if (m_rawheaders)
    {
        if ((ret=buffer.Set(m_rawheaders)) != OpStatus::OK)
            return ret;

        if ((ret=buffer.Append("\r\n")) != OpStatus::OK) //Add line seperating header from body
            return ret;
    }
    else
    {
		BOOL dropped_illegal_headers;
        if ((ret=GetCurrentRawHeaders(buffer, dropped_illegal_headers, TRUE)) != OpStatus::OK)
            return ret;

		if (dropped_illegal_headers && !allow_skipping_illegal_headers)
			return OpStatus::ERR;
    }

	if (!include_bcc && buffer.HasContent())
	{
		OpString8 bcc_headername;
		if ((ret=Header::GetName(IsFlagSet(Message::IS_RESENT) ? Header::RESENTBCC : Header::BCC, bcc_headername)) != OpStatus::OK)
			return ret;

		char* bcc_start = NULL;
		if (op_strnicmp(buffer.CStr(), bcc_headername.CStr(), bcc_headername.Length())==0) //Special case for first line, it is not preceded by a '\n'
		{
			bcc_start = buffer.CStr();
		}
		else //all other lines are preceded with '\n'
		{
			if ((ret=bcc_headername.Insert(0, "\n"))!=OpStatus::OK ||
				(ret=bcc_headername.Append(": "))!=OpStatus::OK)
				return ret;

			bcc_start = const_cast<char*>(op_stristr(buffer.CStr(), bcc_headername.CStr()));
		}

		if (bcc_start) //Header is present, and will have to be removed
		{
			if (bcc_start != buffer.CStr())
				bcc_start++; //Skip \n

			const char* next_headerstart = op_strpbrk(bcc_start, "\r\n");
			while (next_headerstart) //Iterate until start of next header is found (or end of headers are reached)
			{
				next_headerstart += SkipNewline(next_headerstart);
				if (*next_headerstart==' ') //Folded header
				{
					next_headerstart = op_strpbrk(next_headerstart, "\r\n");
				}
				else
					break; //We have found start of the next header
			}

			if (next_headerstart && *next_headerstart)
			{
				memmove(bcc_start, next_headerstart, op_strlen(next_headerstart)+1); //Overwrite BCC-header (and remember to copy the terminator, too)
			}
			else //BCC was the last header. Terminate before BCC
			{
				*bcc_start = 0;
			}
		}
	}

	if (convert_to_mbox_format) //Space-stuff "\nFrom "
	{
		char* body_ptr = m_rawbody;
		char* from_line_ptr;
		while (body_ptr && *body_ptr)
		{
			from_line_ptr = body_ptr;
			while (from_line_ptr && *from_line_ptr)
			{
				from_line_ptr = const_cast<char*>(op_stristr(from_line_ptr, "From "));

				//Is this atr the start of a line? If not, search for the next
				if (from_line_ptr && !(from_line_ptr==body_ptr || *(from_line_ptr-1)=='\r' || *(from_line_ptr-1)=='\n'))
				{
					from_line_ptr += 5; //We know we had "From ", so skip the whole word
					continue;
				}

				break; //We are at end of text, or found a line that needs to be space-stuffed
			}

			if (!from_line_ptr || !*from_line_ptr) //No more lines needs stuffing. Append everyting
			{
				return buffer.Append(body_ptr);
			}
			else //Append part of the buffer, and add a space
			{
				if (!buffer.Reserve(buffer.Length() + (from_line_ptr-body_ptr) + 1 + 5 + 1)) //Make room for the buffer, space-stuffing, "From " (in its original case) and terminator
					return OpStatus::ERR_NO_MEMORY;

				if ((ret=buffer.Append(body_ptr, from_line_ptr-body_ptr))!=OpStatus::OK ||
					(ret=buffer.Append(" "))!=OpStatus::OK ||
					(ret=buffer.Append(from_line_ptr, 5))!=OpStatus::OK)
				{
					return ret;
				}

				body_ptr = from_line_ptr + 5; //Skip "From "
			}
		}
	}
	else
	{
		return buffer.Append(m_rawbody);
	}

	return OpStatus::OK;
}


OP_STATUS Message::IsBodyCharsetCompatible(const OpStringC16& body, int& invalid_count, OpString& invalid_chars)
{
	invalid_count = 0;
	invalid_chars.Empty();

	if (m_force_charset)
	{
		return OpStatus::OK; //User has explicitly told M2 to use the given charset
	}

    OP_STATUS ret;
	OpString8 charset;
	if ((ret=GetCharset(charset)) != OpStatus::OK)
		return ret;

    //Find preferred charset if not already set
	if (charset.IsEmpty())
	{
		Account* account = GetAccountPtr();
		if (account && (ret=account->GetCharset(charset))!=OpStatus::OK)
				return ret;
	}

	if (charset.CompareI("utf-8")==0)
	{
		return OpStatus::OK; //We can always set the body in utf-8
	}

	OpString8 dummy;
	return MessageEngine::ConvertToChar8(charset, body, dummy, invalid_count, invalid_chars);
}

OP_STATUS Message::SetRawBody(const OpStringC16& buffer, BOOL reflow, BOOL buffer_is_useredited, BOOL force_content)
{
	OP_STATUS ret;
	Account* account = GetAccountPtr();
	reflow &= account ? account->GetLinelength() != -1 : TRUE;

    //Find preferred charset if not already set
	if (m_charset.IsEmpty() || m_charset.CompareI("utf-8")==0)
	{
		Account* account = GetAccountPtr();
		if (account && (ret=account->GetCharset(m_charset))!=OpStatus::OK)
            return ret;
	}

    OpString8 down_converted;
    if (reflow && !m_is_HTML)
    {
        OpString flowed_body;
        if (!buffer.IsEmpty())
        {
			OpQuote* quote = CreateQuoteObject();
			if (!quote)
				return OpStatus::ERR_NO_MEMORY;

			ret=quote->WrapText(flowed_body, (buffer.IsEmpty() && force_content) ? OpStringC(UNI_L("\r\n")) : buffer, buffer_is_useredited);
			OP_DELETE(quote);

			if (ret != OpStatus::OK)
		        return ret;
        }

        if ((ret=MessageEngine::ConvertToBestChar8(m_charset, m_force_charset, flowed_body, down_converted)) != OpStatus::OK)
            return ret;
    }
    else
    {
        if ((ret=MessageEngine::ConvertToBestChar8(m_charset, m_force_charset, buffer, down_converted)) != OpStatus::OK)
            return ret;
    }

    return SetRawBody(down_converted.CStr(), TRUE);
}


OP_STATUS Message::SetRawBody(const char* buffer, BOOL update_contenttype, char* dont_copy_buffer)
{
	if (!dont_copy_buffer)
		RemoveRawMessage(TRUE, FALSE);
	PurgeMultipartData();
	//m_multipart_status = MIME_NOT_DECODED; //Don't clear status, as parsed headers are still valid
	if (m_multipart_list)
		m_multipart_list->Clear();

	m_localmessagesize = (m_rawheaders ? op_strlen(m_rawheaders) + 2 : 0);

	if (dont_copy_buffer)
		buffer = dont_copy_buffer;

	if (!buffer || !*buffer)
		return OpStatus::OK;

	int length = op_strlen(buffer);
	if (dont_copy_buffer)
		m_rawbody = dont_copy_buffer;
	else
	{
		m_rawbody = OP_NEWA(char, length+1);
		if (!m_rawbody)
	    	return OpStatus::ERR_NO_MEMORY;
		strcpy(m_rawbody, buffer);
	}

	m_localmessagesize += length;

	if (update_contenttype)
	{
		RETURN_IF_ERROR(SetContentType(m_charset));
	}

	if (m_localmessagesize >= m_realmessagesize)
	{
		m_realmessagesize = m_localmessagesize;
		OpStatus::Ignore(QuickMimeDecode());
	}

	return OpStatus::OK;
}


OP_STATUS Message::GetRawBody(OpString16& body) const
{
	// TODO check out if this function is necessary and why, won't get proper
	// body in a multipart message
    body.Empty();
    return MessageEngine::GetInstance()->GetInputConverter().ConvertToChar16(m_charset, m_rawbody, body);
}

OP_STATUS Message::GetBodyItem(Multipart** body_item, BOOL prefer_html)
{
	if (!GetRawBody())
		return OpStatus::OK; //No body-part is found (message is probably not downloaded yet)

	OP_ASSERT(m_multipart_list);
	if (!m_multipart_list)
		return OpStatus::OK;

	OP_STATUS ret;
	if ((ret=MimeDecodeMessage(TRUE, TRUE, prefer_html)) != OpStatus::OK)
		return ret;

	Multipart* item = (Multipart*)(m_multipart_list->First());
	while (item)
	{
		if (item->IsMailbody())
		{
			URLContentType contenttype = item->GetContentType();

			if (prefer_html && contenttype == URL_HTML_CONTENT)
			{
				*body_item = item;
				break;
			}
			else if (!prefer_html && contenttype == URL_TEXT_CONTENT)
			{
				*body_item = item;
				break;
			}
			else if (!*body_item)
			{
				*body_item = item;
			}
		}

		item = (Multipart*)(item->Suc());
	}

	return OpStatus::OK;
}


OP_STATUS Message::QuickGetBodyText(OpString16& body) const
{
	OpString8 content_type;
	OpString8 content_transfer_encoding;
	OpString8 boundary;

	// Preparation
	body.Empty();
	if (!m_rawbody)
		return OpStatus::OK;

	// Check master Content-Type and Content-Transfer-Encoding
	RETURN_IF_ERROR(GetHeaderValue(Header::CONTENTTYPE, content_type));
	RETURN_IF_ERROR(GetHeaderValue(Header::CONTENTTRANSFERENCODING, content_transfer_encoding));

	QuickMimeParser parser;

	return parser.GetTextParts(content_type, content_transfer_encoding, "", m_rawbody, body);
}


OP_STATUS Message::GetBodyText(OpString16& text, BOOL unwrap, bool* prefer_html)
{
	text.Empty();

	BOOL use_html=FALSE;
	if (prefer_html)
		use_html=*prefer_html;

	if (!GetRawBody())
		return OpStatus::OK; //No body-part is found (message is probably not downloaded yet)

	OP_ASSERT(m_multipart_list);
	if (!m_multipart_list)
		return OpStatus::OK;

	BOOL multiparts_already_present = (m_multipart_status==MIME_DECODING_ALL || m_multipart_status==MIME_DECODED_ALL);

	m_multipart_status = multiparts_already_present ? MIME_DECODED_HEADERS : MIME_NOT_DECODED;
	OP_STATUS ret;
	if ((ret=MimeDecodeMessage(TRUE, TRUE, use_html)) != OpStatus::OK)
		return ret;

//#pragma PRAGMAMSG("FG: When decoding is made asynchronous, the following code will have to go into a listener")

    //Find preferred body-part
	Multipart* body_item = NULL;
	Multipart* item = (Multipart*)(m_multipart_list->First());
	while (item)
	{
		if (item->IsMailbody())
		{
			URLContentType contenttype = item->GetContentType();

			if (use_html && contenttype == URL_HTML_CONTENT)
			{
				body_item = item;
				break;
			}
			else if (!use_html && contenttype == URL_TEXT_CONTENT)
			{
				body_item = item;
				break;
			}
			else if (!body_item)
			{
				body_item = item;
			}
		}

		item = (Multipart*)(item->Suc());
	}

    //Delete data in multipart-list (to free up as much RAM as possible while still having the Mime-structure)
	if (!multiparts_already_present)
		PurgeMultipartData(body_item);

	if (!body_item)
		return OpStatus::OK; //No body-part is found (message is probably not downloaded yet)

	if (prefer_html)
		*prefer_html = (body_item->GetContentType() == URL_HTML_CONTENT);

	if (body_item->GetContentType()!=URL_TEXT_CONTENT && !use_html)
	{
		OpStatus::Ignore(HTMLToText::GetCurrentHTMLMailAsText(m_myself, text));
	}
	else
	{
		if ((ret=body_item->GetBodyText(text)) != OpStatus::OK)
		{
			if (!multiparts_already_present)
				DeleteMultipartData(body_item);

			return ret;
		}
	}

    //Clean up and return
	if (!multiparts_already_present)
		DeleteMultipartData(body_item);

	if (unwrap)
	{
		OpQuote* quote = CreateQuoteObject();
		if (!quote)
			return OpStatus::ERR_NO_MEMORY;

		OpString unwrapped;
		ret = quote->UnwrapText(unwrapped, text);
		OP_DELETE(quote);
		if (ret!=OpStatus::OK ||
				  (ret=text.Set(unwrapped))!=OpStatus::OK)
		{
			return ret;
		}
	}

	return OpStatus::OK;
}

OP_STATUS Message::SetCharset(const OpStringC8& charset, BOOL force)
{
    if (charset.IsEmpty() ||
        charset.CompareI("utf-16")==0 ||
        charset.CompareI("utf-32")==0)
    {
        return m_charset.Set("utf-8");
    }
    else if (!force && m_force_charset && IsFlagSet(Message::IS_OUTGOING) && m_charset.HasContent() && m_charset.CompareI(charset)!=0)
	{
		return OpStatus::ERR_NO_ACCESS;
	}

	return m_charset.Set(charset);
}

OP_STATUS Message::UpdateCharsetFromContentType()
{
	OpString8 content_type, charset;

	// Find the content-type header
	if (OpStatus::IsError(GetHeaderValue(Header::CONTENTTYPE, content_type)) ||
		content_type.IsEmpty())
		return OpStatus::OK;

	// Check for charset
	QuickMimeParser parser;
	if (OpStatus::IsError(parser.GetAttributeFromContentType(content_type, "charset", charset)) ||
		charset.IsEmpty())
		return OpStatus::OK;

	// Set charset
	return SetCharset(charset);
}

OP_STATUS Message::SetContentType(const OpString8& charset)
{
    OP_STATUS ret;
    OpString8 content_charset;

    if ((ret=content_charset.Set(charset.IsEmpty() ? m_charset : charset)) != OpStatus::OK)
        return ret;

    if ( content_charset.IsEmpty() ||
		 (m_force_charset && IsFlagSet(Message::IS_OUTGOING) && m_charset.CompareI(content_charset)!=0) )
    {
		Account* account = GetAccountPtr();
		if (m_force_charset && !account)
			return OpStatus::ERR_NULL_POINTER;

		if (account)
		{
			OpStatus::Ignore(account->GetCharset(content_charset));
		}

		if (content_charset.IsEmpty())
		{
			OpStatus::Ignore(content_charset.Set("us-ascii"));
		}

		OpStatus::Ignore(SetCharset(content_charset));
    }

    OpString contenttype;
    if ( ((ret=contenttype.Set(m_is_HTML ? "text/html; charset=" : "text/plain; charset=")) != OpStatus::OK) ||
         ((ret=contenttype.Append(content_charset)) != OpStatus::OK) )
    {
         return ret;
    }

	Account* account = GetAccountPtr();
	BOOL autowrapped = account ? account->GetLinelength() != -1 : TRUE;
	if (!m_is_HTML && autowrapped && (ret=contenttype.Append("; format=flowed; delsp=yes")) != OpStatus::OK)
		return ret;

    return SetHeaderValue(Header::CONTENTTYPE, contenttype);
}

void Message::SetTextDirection(int direction)
{
	switch(direction)
	{
		case AccountTypes::DIR_AUTOMATIC:
			SetFlag(Message::DIR_FORCED_LTR,FALSE);
			SetFlag(Message::DIR_FORCED_RTL,FALSE);
			break;
		case AccountTypes::DIR_LEFT_TO_RIGHT:
			SetFlag(Message::DIR_FORCED_LTR,TRUE);
			SetFlag(Message::DIR_FORCED_RTL,FALSE);
			break;
		case AccountTypes::DIR_RIGHT_TO_LEFT:
			SetFlag(Message::DIR_FORCED_RTL,TRUE);
			SetFlag(Message::DIR_FORCED_LTR,FALSE);
			break;
		default:
			SetFlag(Message::DIR_FORCED_LTR,FALSE);
			SetFlag(Message::DIR_FORCED_RTL,FALSE);
	}
}


int Message::GetTextDirection() const
{
	if (IsFlagSet(Message::DIR_FORCED_LTR))
		return AccountTypes::DIR_LEFT_TO_RIGHT;
	if (IsFlagSet(Message::DIR_FORCED_RTL))
		return AccountTypes::DIR_RIGHT_TO_LEFT;

	return AccountTypes::DIR_AUTOMATIC;
}



OP_STATUS Message::GenerateInReplyToHeader()
{
	if (IsFlagSet(Message::IS_NEWS_MESSAGE)) //We don't want In-Reply-To in news-messages
		return OpStatus::OK;

	OP_STATUS ret;
	OpString8 last_referenced_message;
	Header* references_header = GetHeader(Header::REFERENCES);
	if (references_header)
	{
		if ((ret=references_header->GetMessageId(last_referenced_message, 0)) != OpStatus::OK)
			return ret;
	}

	return SetHeaderValue(Header::INREPLYTO, last_referenced_message);
}

OpQuote* Message::CreateQuoteObject(unsigned max_quote_depth) const
{
    Account* account = GetAccountPtr();
	INT8 max_line_length = account ? account->GetLinelength() : 0;
	BOOL delete_flow_space;
	BOOL is_flowed = IsFlowed(delete_flow_space);
	return OP_NEW(OpQuote, (is_flowed, delete_flow_space, max_line_length!=-1, max_line_length==-1 ? 998 : max_line_length, max_quote_depth));
}

BOOL Message::IsFlowed(BOOL& delsp) const
{
    if (IsFlagSet(Message::IS_OUTGOING))
	{
		delsp = TRUE;
        return TRUE;
	}

	delsp = FALSE;
    OP_STATUS ret;
    OpString8 contenttype;
    if ((ret=GetHeaderValue(Header::CONTENTTYPE, contenttype)) != OpStatus::OK)
        return ret;

    if (contenttype.IsEmpty())
        return FALSE;

    const char* contenttype_ptr = op_stristr(contenttype.CStr(), "format=");
    if (!contenttype_ptr)
        return FALSE;

    contenttype_ptr += 7; //Skip "format="
    while (*contenttype_ptr=='"' || *contenttype_ptr==' ') contenttype_ptr++;

	if (op_strnicmp(contenttype_ptr, "flowed", 6)!=0)
		return FALSE;

    const char* delsp_ptr = op_stristr(contenttype.CStr(), "delsp=");
	if (delsp_ptr)
	{
		delsp_ptr += 6; //Skip "delsp="
		while (*delsp_ptr=='"' || *delsp_ptr==' ') delsp_ptr++;

		if (op_strnicmp(delsp_ptr, "yes", 3)==0)
			delsp = TRUE;
	}

	return TRUE;
}

OP_STATUS Message::MimeEncodeAttachments(BOOL remove_bcc, BOOL allow_8bit)
{
	if (m_is_HTML)
		return OpStatus::OK;

    OP_STATUS ret;
    Upload_Base* upload_element = NULL;
	MimeUtils* mime_utils = MessageEngine::GetInstance()->GetGlueFactory()->GetMimeUtils();

	if ((ret=CreateUploadElement(&upload_element, remove_bcc, allow_8bit)) != OpStatus::OK)
    {
        mime_utils->DeleteUploadElement(upload_element);
        return ret;
    }

    if (!upload_element)
        return OpStatus::ERR_NULL_POINTER;

    long message_length = upload_element->CalculateLength();
    OpString8 new_rawmessage;
    if (!new_rawmessage.Reserve(message_length+100))
    {
        mime_utils->DeleteUploadElement(upload_element);
        return OpStatus::ERR_NO_MEMORY;
    }

    int   buffer_len = 0;
    BOOL  done;
    buffer_len = upload_element->GetOutputData((unsigned char *) new_rawmessage.CStr(), new_rawmessage.Capacity()-1, done);
    *(new_rawmessage.CStr()+buffer_len) = 0; //Terminate string
    while (!done && buffer_len!=0)
    {
        OpString8 overflow_buffer;
        if (!overflow_buffer.Reserve(10*1024+1))
        {
	        mime_utils->DeleteUploadElement(upload_element);
            return OpStatus::ERR_NO_MEMORY;
        }

        buffer_len = upload_element->GetOutputData((unsigned char *) overflow_buffer.CStr(), overflow_buffer.Capacity()-1, done);
        *(overflow_buffer.CStr()+buffer_len) = 0; //Terminate string
        if ((ret=new_rawmessage.Append(overflow_buffer)) != OpStatus::OK)
        {
	        mime_utils->DeleteUploadElement(upload_element);
            return ret;
        }
    }
    mime_utils->DeleteUploadElement(upload_element);

    RemoveAllAttachments();

    return SetRawMessage(new_rawmessage.CStr());
}

size_t Message::SkipNewline(const char* string) const
{
	const char* skipped = string;

	if (*skipped == '\r')
		skipped++;
	if (*skipped == '\n')
		skipped++;

	return skipped - string;
}

OP_STATUS Message::RemoveMimeHeaders()
{
    OP_STATUS ret;
    if ( ((ret=RemoveHeader(Header::CONTENTDISPOSITION)) != OpStatus::OK) ||
         ((ret=RemoveHeader(Header::CONTENTTRANSFERENCODING)) != OpStatus::OK) ||
         ((ret=RemoveHeader(Header::MIMEVERSION)) != OpStatus::OK) )
         //Content-type is handled elsewhere, it might be created by M2
    {
        return ret;
    }

	if (GetHeader(Header::CONTENTTYPE)) //If we have a Content-Type header, be sure to remove charset. Upload2 will add it and we don't want duplicates
    {
        OpString contenttype;
		if (!m_is_HTML)
		{
			Account* account = GetAccountPtr();
			BOOL autowrapped = account ? account->GetLinelength() != -1 : TRUE;
			if ((ret=contenttype.Set("text/plain"))!=OpStatus::OK)
				return ret;
			if (autowrapped && (ret=contenttype.Append("; format=flowed; delsp=yes"))!=OpStatus::OK)
				return ret;
		}
		else
		{
			if ((ret=contenttype.Set("text/html"))!=OpStatus::OK)
				return ret;
		}
		if ((ret=SetHeaderValue(Header::CONTENTTYPE, contenttype, FALSE))!=OpStatus::OK)
			return ret;
    }

    return OpStatus::OK;
}

#ifndef __SUPPORT_OPENPGP__
static const KeywordIndex Upload_untrusted_headers_ContentType[] = {
	{NULL, FALSE},
	{"Content-Type",TRUE}
};

static const char *UploadHeaders[] = {
	"Date",
	"To",
	"Subject",
	"Reply-To",
	"From",
	"Organization",
	"Cc",
	"Content-Type",
	"Message-ID",
	"User-Agent",
	"MIME-Version",
	NULL
};


OP_STATUS Message::CreateUploadElement(OUT Upload_Base** element, BOOL remove_bcc, BOOL allow_8bit)
{
    if (element==NULL)
        return OpStatus::ERR_NULL_POINTER;

    OP_STATUS ret;
    if ((ret=RemoveMimeHeaders()) != OpStatus::OK)
        return ret;

    Attachment* OP_MEMORY_VAR attachment
		= m_attachment_list? (Attachment*)m_attachment_list->First() : NULL;

	Upload_Multipart* OP_MEMORY_VAR multipart_body = NULL;
	Upload_Multipart* OP_MEMORY_VAR multipart_html = NULL;
	Upload_Base *top_body = NULL;

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	MimeUtils* mime_utils = glue_factory->GetMimeUtils();

	top_body = mime_utils->CreateUploadElement(attachment || m_is_HTML ? UPLOAD_MULTIPART : UPLOAD_STRING8);
    (*element) = top_body;
    if (top_body == NULL)
        return OpStatus::ERR_NO_MEMORY;

	if(attachment || m_is_HTML)
		multipart_body = (Upload_Multipart *) top_body;

	if (attachment && m_is_HTML)
	{
		multipart_html = (Upload_Multipart*)mime_utils->CreateUploadElement(UPLOAD_MULTIPART);
		if (multipart_html == NULL)
		{
			mime_utils->DeleteUploadElement(top_body);
			return OpStatus::ERR_NO_MEMORY;
		}
	}


    Upload_OpString8* body_element = (Upload_OpString8*) (attachment || m_is_HTML ? mime_utils->CreateUploadElement(UPLOAD_STRING8) : *element);
    if (!body_element)
    {
        mime_utils->DeleteUploadElement(top_body);
		if (multipart_html)
	        mime_utils->DeleteUploadElement(multipart_html);
        return OpStatus::ERR_NO_MEMORY;
    }

	Upload_OpString8* body_text_element= NULL;
	if (m_is_HTML)
	{
		body_text_element = (Upload_OpString8*)(mime_utils->CreateUploadElement(UPLOAD_STRING8));
		if (!body_text_element)
		{
            mime_utils->DeleteUploadElement(top_body);

            if (body_element != top_body)
                mime_utils->DeleteUploadElement(body_element);
			if (multipart_html)
				mime_utils->DeleteUploadElement(multipart_html);

			(*element) = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
	}

    OpString8 content_value;
    //if (body_element != (*element)) //If we have multiparts, move original content-type header down to body-part
    {
        if ((ret=GetHeaderValue(Header::CONTENTTYPE, content_value)) != OpStatus::OK)
        {
            mime_utils->DeleteUploadElement(top_body);
            if (body_element != top_body)
                mime_utils->DeleteUploadElement(body_element);
			if (m_is_HTML)
				mime_utils->DeleteUploadElement(body_text_element);
			if (multipart_html)
				mime_utils->DeleteUploadElement(multipart_html);
			(*element) = NULL;
            return ret;
        }

        if (content_value.IsEmpty()) //No content-type set yet? Generate one...
        {
            OpString8 dummy_contenttype;
            if ( ((ret=SetContentType(dummy_contenttype)) != OpStatus::OK) ||
                 ((ret=GetHeaderValue(Header::CONTENTTYPE, content_value)) != OpStatus::OK) )
            {
                mime_utils->DeleteUploadElement(top_body);
				if (body_element != top_body)
					mime_utils->DeleteUploadElement(body_element);
				if (m_is_HTML)
					mime_utils->DeleteUploadElement(body_text_element);
				if (multipart_html)
					mime_utils->DeleteUploadElement(multipart_html);
				(*element) = NULL;
                return ret;
            }
        }

        //Remove old content-type header, and generate a new one on the correct multipart
        if ( (ret != OpStatus::OK) ||
			 ((ret=RemoveHeader(Header::CONTENTTYPE)) != OpStatus::OK) )
        {
            mime_utils->DeleteUploadElement(top_body);
            if (body_element != top_body)
                mime_utils->DeleteUploadElement(body_element);
			if (m_is_HTML)
				mime_utils->DeleteUploadElement(body_text_element);
			if (multipart_html)
				mime_utils->DeleteUploadElement(multipart_html);
			(*element) = NULL;
            return ret;
        }
    }

    // Set headers and body
    OP_ASSERT(m_rawbody && *m_rawbody); //Assert violated if body is blank
    OpString8 headers, bcc_header;
	BOOL dummy_dropped_illegal_headers;
    if ((ret=GetCurrentRawHeaders(headers, dummy_dropped_illegal_headers, FALSE)) != OpStatus::OK)
    {
        OP_ASSERT(0);
        mime_utils->DeleteUploadElement(top_body);

        if (body_element != top_body)
            mime_utils->DeleteUploadElement(body_element);
		if (m_is_HTML)
			mime_utils->DeleteUploadElement(body_text_element);
		if (multipart_html)
	        mime_utils->DeleteUploadElement(multipart_html);

		(*element) = NULL;
        return ret;
    }

	TRAP(ret, body_element->InitL(m_rawbody && *m_rawbody ? m_rawbody : "\r\n", content_value, NULL, ENCODING_AUTODETECT, UploadHeaders));
	if(ret == OpStatus::OK && body_element == top_body)
		TRAP(ret, body_element->ExtractHeadersL((const unsigned char *)headers.CStr(), headers.Length(), TRUE, remove_bcc ? HEADER_REMOVE_BCC : HEADER_ONLY_REMOVE_CUSTOM, Upload_untrusted_headers_ContentType, ARRAY_SIZE(Upload_untrusted_headers_ContentType)));

	if(ret != OpStatus::OK)
	{
        mime_utils->DeleteUploadElement(top_body);

        if (body_element != top_body)
            mime_utils->DeleteUploadElement(body_element);
		if (m_is_HTML)
			mime_utils->DeleteUploadElement(body_text_element);
		if (multipart_html)
	        mime_utils->DeleteUploadElement(multipart_html);

		(*element) = NULL;
        return ret;
	}

	if (m_is_HTML)
	{
		OpString text_equivalent_16;
		OpString8 text_equivalent;
		OpString8 text_content_value;

		GetRawBody(text_equivalent_16);

		// TODO Convert text_equivalent16 from HTML to text
		ret = text_equivalent.Set(text_equivalent_16.CStr());
		if (OpStatus::IsError(ret))
		{
			mime_utils->DeleteUploadElement(top_body);

			if (body_element != top_body)
				mime_utils->DeleteUploadElement(body_element);
			if (m_is_HTML)
				mime_utils->DeleteUploadElement(body_text_element);
			if (multipart_html)
				mime_utils->DeleteUploadElement(multipart_html);

			(*element) = NULL;
			return ret;
		}


		Account* account = GetAccountPtr();
		BOOL autowrapped = account ? account->GetLinelength() != -1 : TRUE;
		if ((ret=text_content_value.Set("text/plain"))!=OpStatus::OK)
			return ret;
		if (autowrapped && (ret=text_content_value.Append("; format=flowed; delsp=yes"))!=OpStatus::OK)
			return ret;

		TRAP(ret, body_text_element->InitL(text_equivalent.CStr(), text_content_value, NULL, ENCODING_AUTODETECT, UploadHeaders));
		if(ret != OpStatus::OK)
		{
			mime_utils->DeleteUploadElement(top_body);

			if (body_element != top_body)
				mime_utils->DeleteUploadElement(body_element);
			if (m_is_HTML)
				mime_utils->DeleteUploadElement(body_text_element);
			if (multipart_html)
				mime_utils->DeleteUploadElement(multipart_html);

			(*element) = NULL;
			return ret;
		}
	}


    // Set mimetype if we have multiparts
    if (body_element != top_body)
    {
		if (attachment)
		{
			TRAP(ret, multipart_body->InitL("multipart/mixed",UploadHeaders));
			if (m_is_HTML && ret == OpStatus::OK)
				TRAP(ret, multipart_html->InitL("multipart/alternative",UploadHeaders));
		}
		else
			TRAP(ret, multipart_body->InitL("multipart/alternative",UploadHeaders));

		if (ret == OpStatus::OK)
			TRAP(ret, multipart_body->ExtractHeadersL((const unsigned char *)headers.CStr(), headers.Length(), TRUE, remove_bcc ? HEADER_REMOVE_BCC : HEADER_ONLY_REMOVE_CUSTOM, Upload_untrusted_headers_ContentType, ARRAY_SIZE(Upload_untrusted_headers_ContentType)));

        if (ret != OpStatus::OK)
        {
            mime_utils->DeleteUploadElement(top_body);

            if (body_element != top_body)
                mime_utils->DeleteUploadElement(body_element);
			if (m_is_HTML)
				mime_utils->DeleteUploadElement(body_text_element);
			if (multipart_html)
				mime_utils->DeleteUploadElement(multipart_html);

			(*element) = NULL;
            return ret;
        }

		if (m_is_HTML && attachment)
		{
			multipart_body->AddElement(multipart_html);
			multipart_html->AddElement(body_text_element);
	        multipart_html->AddElement(body_element);
		}
		else
		{
			if (m_is_HTML)
				multipart_body->AddElement(body_text_element);
			multipart_body->AddElement(body_element);
		}
    }

    // Add the attachments
    while (attachment)
    {
        Upload_Base* attachment_element;
        if ((ret=attachment->CreateUploadElement(&attachment_element, m_charset)) != OpStatus::OK)
        {
            mime_utils->DeleteUploadElement(*element);
			(*element) = NULL;
            return ret;
        }

        multipart_body->AddElement(attachment_element);

        attachment = (Attachment*)attachment->Suc();
    }

    // Set charset and encoding
	TRAP(ret, body_element->SetCharsetL(m_charset.CStr()));

	if (ret != OpStatus::OK)
	{
		mime_utils->DeleteUploadElement(top_body);
		(*element) = NULL;
		return ret;
	}

    // Prepare upload_elements
	TRAP(ret, top_body->PrepareUploadL(!Is7BitBody() && allow_8bit ? UPLOAD_8BIT_TRANSFER : UPLOAD_7BIT_TRANSFER));

	// Reset, just to be sure we're ready to get blocks
    if (ret == OpStatus::OK)
		TRAP(ret, top_body->ResetL());

    if (ret != OpStatus::OK)
    {
        mime_utils->DeleteUploadElement(*element);
		(*element) = NULL;
        return OpStatus::ERR;
    }

    return OpStatus::OK;
}
#else
OP_STATUS Message::CreateUploadElement(OUT Upload_Base** element, BOOL remove_bcc, BOOL allow_8bit)
{
    if (element==NULL)
        return OpStatus::ERR_NULL_POINTER;

	(*element) = NULL;

    OP_STATUS ret;
    if ((ret=RemoveMimeHeaders()) != OpStatus::OK)
        return ret;

    Attachment* attachment = m_attachment_list? (Attachment*)m_attachment_list->First() : NULL;

	Upload_Builder_Base *builder = NULL;
	Upload_Base *current_element;

	MimeUtils* mime_utils = MessageEngine::GetInstance()->GetGlueFactory()->GetMimeUtils();

	builder = mime_utils->CreateUploadBuilder();
    if (builder == NULL)
        return OpStatus::ERR_NO_MEMORY;

    Header          address_header;
    const Header::From*   address_item;
	OpString8		utf8_address;

	BOOL is_resent = IsFlagSet(Message::IS_RESENT);
    if (GetHeader(is_resent ? Header::RESENTTO : Header::TO))
    {
        address_header = *GetHeader(is_resent ? Header::RESENTTO : Header::TO);
        address_header.DetachFromMessage();
        address_item = address_header.GetFirstAddress();

		while(address_item)
		{
			ret = address_item->GetIMAAAddress(utf8_address);
			if(ret == OpStatus::OK)
				TRAP(ret, builder->AddRecipientL(utf8_address, NULL, 0));

			if(ret != OpStatus::OK)
			{
				mime_utils->DeleteUploadBuilder(builder);
				return ret;
			}
			address_item = (Header::From*) address_item->Suc();
		}
    }
    if (GetHeader(is_resent ? Header::RESENTCC : Header::CC))
    {
        address_header = *GetHeader(is_resent ? Header::RESENTCC : Header::CC);
        address_header.DetachFromMessage();
        address_item = address_header.GetFirstAddress();

		while(address_item)
		{
			ret = address_item->GetIMAAAddress(utf8_address);
			if(ret == OpStatus::OK)
				TRAP(ret, builder->AddRecipientL(utf8_address, NULL, 0));

			if(ret != OpStatus::OK)
			{
				mime_utils->DeleteUploadBuilder(builder);
				return ret;
			}
			address_item = (Header::From*) address_item->Suc();
		}
    }
	// BCc is never included among

    if (GetHeader(is_resent ? Header::RESENTFROM : Header::FROM))
    {
        address_header = *GetHeader(is_resent ? Header::RESENTFROM : Header::FROM);
        address_header.DetachFromMessage();
        address_item = address_header.GetFirstAddress();

		ret = address_item->GetIMAAAddress(utf8_address);
		if(ret == OpStatus::OK)
			TRAP(ret, builder->AddSignerL(utf8_address, NULL, NULL, 0));
		if(ret == OpStatus::OK && builder->GetEncryptionLevel() != Encryption_None)
			TRAP(ret, builder->AddRecipientL(utf8_address, NULL, 0));

		if(ret != OpStatus::OK)
		{
			mime_utils->DeleteUploadBuilder(builder);
			return ret;
		}
    }


    OpString8 content_value;
    //if (body_element != (*element)) //If we have multiparts, move original content-type header down to body-part
    {
        if ((ret=GetHeaderValue(Header::CONTENTTYPE, content_value)) != OpStatus::OK)
        {
            mime_utils->DeleteUploadBuilder(builder);
            return ret;
        }

        if (content_value.IsEmpty()) //No content-type set yet? Generate one...
        {
            OpString8 dummy_contenttype;
            if ( ((ret=SetContentType(dummy_contenttype)) != OpStatus::OK) ||
                 ((ret=GetHeaderValue(Header::CONTENTTYPE, content_value)) != OpStatus::OK) )
            {
	            mime_utils->DeleteUploadBuilder(builder);
                return ret;
            }
        }

        //Remove old content-type header, and generate a new one on the correct multipart
        if ( (ret != OpStatus::OK) ||
			 ((ret=RemoveHeader(Header::CONTENTTYPE)) != OpStatus::OK) )
        {
            mime_utils->DeleteUploadBuilder(builder);
            return ret;
        }
    }

    // Set headers and body
    OP_ASSERT(m_rawbody && *m_rawbody); //Assert if body is blank
    OpString8 headers, bcc_header;
	BOOL dummy_dropped_illegal_headers;
    if ((ret=GetCurrentRawHeaders(headers, dummy_dropped_illegal_headers, FALSE)) != OpStatus::OK)
    {
        OP_ASSERT(0);
		mime_utils->DeleteUploadBuilder(builder);
        return ret;
    }

	TRAP(ret, builder->SetHeadersL(headers, headers.Length()));
	if(ret == OpStatus::OK)
		TRAP(ret, current_element = builder->SetMessageL(m_rawbody && *m_rawbody ? m_rawbody : "\r\n", content_value, NULL));

	if(ret != OpStatus::OK)
	{
        mime_utils->DeleteUploadBuilder(builder);
        return ret;
	}

    // Add the attachments
    while (attachment)
    {
		if((ret = attachment->UpdateUploadBuilder(builder)) != OpStatus::OK)
        {
	        mime_utils->DeleteUploadBuilder(builder);
            return ret;
        }

        attachment = (Attachment*)attachment->Suc();
    }

    // Set charset and encoding
	TRAP(ret, builder->SetCharsetL(m_charset.CStr()));
	if (ret != OpStatus::OK)
	{
        mime_utils->DeleteUploadBuilder(builder);
		return ret;
	}

	TRAP(ret, current_element = builder->FinalizeMessageL());
	if(OpStatus::IsError(ret) && !OpStatus::IsMemoryError(ret) && builder->GetSignatureLevel() != Signature_Unsigned)
	{
		builder->ForceSignatureLevel(Signature_Unsigned);
		builder->ForceEncryptionLevel(Encryption_None);
		TRAP(ret, current_element = builder->FinalizeMessageL());
	}
    mime_utils->DeleteUploadBuilder(builder);
	if (ret != OpStatus::OK || current_element == NULL)
	{
		return ret;
	}

    // Prepare upload_elements
	TRAP(ret, current_element->PrepareUploadL(UPLOAD_7BIT_TRANSFER));

	// Reset, just to be sure we're ready to get blocks
    if (ret == OpStatus::OK)
		TRAP(ret, current_element->ResetL());

    if (ret != OpStatus::OK)
    {
        mime_utils->DeleteUploadElement(current_element);
        return OpStatus::ERR;
    }

	(*element) = current_element;

    return OpStatus::OK;
}
#endif

BOOL Message::Is7BitBody(char *rawbody) const
{
	if (rawbody == NULL)
		rawbody = m_rawbody;
	BOOL is7bit = TRUE;

	if(rawbody)
	{
		size_t length = op_strlen(rawbody);

		for(size_t i = 0; i < length; i++)
		{
			if(!(0 < rawbody[i]))
			{
				is7bit = FALSE;
				break;
			}
		}
	}

	return is7bit;
}

void Message::RemoveRawMessage(BOOL remove_body, BOOL remove_header)
{
	if (remove_body)
	{
		if (m_in_place_body)
		{
			if (remove_header || m_rawheaders != m_in_place_body + m_in_place_body_offset)
			{
				if (remove_header && m_rawheaders == m_in_place_body + m_in_place_body_offset)
				{
					m_rawheaders  = 0;
					remove_header = FALSE;
				}
				OP_DELETEA(m_in_place_body);
				m_in_place_body = 0;
				m_in_place_body_offset = 0;
			}
		}
		else
		{
			OP_DELETEA(m_rawbody);
		}
		m_rawbody = 0;
	}

	if (remove_header)
	{
		if (!m_in_place_body || m_rawheaders != m_in_place_body + m_in_place_body_offset)
			OP_DELETEA(m_rawheaders);
		m_rawheaders = 0;
	}
}

OP_STATUS Message::QuoteHTMLBody(Multipart* body_item, OpString& quoted_body)
{
	OP_STATUS ret = OpStatus::OK;

	DesktopWidgetWindow* widget_window = OP_NEW(DesktopWidgetWindow, ());
	if (!widget_window)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsSuccess(ret=widget_window->Init(OpWindow::STYLE_TOOLTIP, "Quote HTMLBody Window")))
	{
		OpBrowserView* mail_view;
		RETURN_IF_ERROR(OpBrowserView::Construct(&mail_view));
		widget_window->GetWidgetContainer()->GetRoot()->AddChild(mail_view);

		mail_view->GetWindow()->SetType(WIN_TYPE_MAIL_VIEW);
		URL dummy;

		Window* window = mail_view->GetWindow();
		if (window)
		{
			window->DocManager()->OpenURL(*(body_item->GetURL()), DocumentReferrer(dummy), FALSE, FALSE, FALSE, TRUE,NotEnteredByUser,FALSE,TRUE);
		}

		LogicalDocument* log_doc = ((FramesDocument*)window->GetCurrentDoc())->GetLogicalDocument();
		if (log_doc)
		{

			if (log_doc->GetRoot()->IsDirty())
				((FramesDocument*)window->GetCurrentDoc())->Reflow(TRUE, TRUE, FALSE);

			HTML_Element* body_elm = log_doc->GetHLDocProfile()->GetBodyElm();
			if (body_elm)
			{
				TempBuffer tmp_buff;
				body_elm->AppendEntireContentAsString(&tmp_buff, FALSE, FALSE);
				quoted_body.Set(tmp_buff.GetStorage());
			}

		}
	}

	widget_window->Close(TRUE);

	return ret;
}

// ----------------------------------------------------

Message::Attachment::Attachment()
 : m_attachment_url(NULL)
 , m_is_inlined(FALSE)
{
}

Message::Attachment::~Attachment()
{
    if (m_attachment_url)
    {
        MessageEngine::GetInstance()->GetGlueFactory()->DeleteURL(m_attachment_url);
    }
}


OP_STATUS Message::Attachment::Init(const OpStringC& suggested_filename, const OpStringC& attachment_filename, URL* attachment_url, BOOL is_inlined)
{
	m_is_inlined = is_inlined;
	if (is_inlined)
		attachment_url->SetAttribute(URL::KUnique,FALSE);

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
    if (m_attachment_url)
    {
		m_attachment_url_inuse.UnsetURL();
        glue_factory->DeleteURL(m_attachment_url);
        m_attachment_url = NULL;
    }

    if (attachment_url)
    {
        m_attachment_url = glue_factory->CreateURL();
        if (!m_attachment_url)
            return OpStatus::ERR_NO_MEMORY;

        *m_attachment_url = *attachment_url;
		m_attachment_url_inuse.SetURL(*m_attachment_url);
    }

    OP_STATUS ret;
    if ( ((ret=m_suggested_filename.Set(suggested_filename)) != OpStatus::OK) ||
         ((ret=m_attachment_filename.Set(attachment_filename)) != OpStatus::OK) )
    {
        if (m_attachment_url)
        {
			m_attachment_url_inuse.UnsetURL();
            glue_factory->DeleteURL(m_attachment_url);
            m_attachment_url = NULL;
        }
        return ret;
    }

    return OpStatus::OK;
}


#if !defined __SUPPORT_OPENPGP__
OP_STATUS Message::Attachment::CreateUploadElement(OUT Upload_Base** element, const OpStringC8& charset) const
{
    if (!element) //Init() should have been called before calling this function!
        return OpStatus::ERR_NULL_POINTER;

	OP_ASSERT(m_attachment_filename.HasContent());
    if (m_attachment_filename.IsEmpty())
        return OpStatus::ERR_FILE_NOT_FOUND;

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
    //Create file, and check if it exists
    OpFile* file = glue_factory->CreateOpFile(m_attachment_filename);
	OP_ASSERT(file!=NULL);
    if (!file)
        return OpStatus::ERR_NO_MEMORY;

    BOOL file_exists;
    OP_STATUS ret = file->Exists(file_exists);
	OP_ASSERT(ret==OpStatus::OK && file_exists);
    if (ret!=OpStatus::OK || !file_exists)
    {
        glue_factory->DeleteOpFile(file);
        return OpStatus::ERR_FILE_NOT_FOUND;
    }

    ret=file->Open(OPFILE_READ); //Just to test if the Open succeedes
    file->Close();
	OP_ASSERT(ret==OpStatus::OK);
    if (ret != OpStatus::OK)
    {
        glue_factory->DeleteOpFile(file);
        return ret;
    }

	MimeUtils* mime_utils = glue_factory->GetMimeUtils();

    //Create element
	Upload_URL *upload_file = (Upload_URL *) mime_utils->CreateUploadElement(UPLOAD_URL);
    (*element) = upload_file;
	OP_ASSERT(upload_file!=NULL);
    if (!upload_file)
    {
        glue_factory->DeleteOpFile(file);
        return OpStatus::ERR_NO_MEMORY;
    }

  	TRAP(ret, upload_file->InitL(file->GetFullPath(), m_suggested_filename, "attachment", NULL, charset));
	OP_ASSERT(ret==OpStatus::OK);
    if (ret != OpStatus::OK)
    {
		mime_utils->DeleteUploadElement(upload_file);
        glue_factory->DeleteOpFile(file);
        return ret;
    }

    glue_factory->DeleteOpFile(file);
    return OpStatus::OK;
}
#else
OP_STATUS Message::Attachment::UpdateUploadBuilder(OUT Upload_Builder_Base *builder) const
{
    if (!builder) //Init() should have been called before calling this function!
        return OpStatus::ERR_NULL_POINTER;

    if (m_attachment_filename.IsEmpty())
        return OpStatus::ERR_FILE_NOT_FOUND;

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
    //Create file, and check if it exists
    OpFile* file = glue_factory->CreateOpFile(m_attachment_filename);
    if (!file)
        return OpStatus::ERR_NO_MEMORY;

    OP_STATUS ret;
    BOOL file_exists = FALSE;
	OpStatus::Ignore(file->Exists(file_exists));
    if (!file_exists)
    {
		glue_factory->DeleteOpFile(file);
        return OpStatus::ERR_FILE_NOT_FOUND;
    }

    ret=file->Open(OPFILE_READ); //Just to test if the Open succeedes
    file->Close();
    if (ret != OpStatus::OK)
    {
		glue_factory->DeleteOpFile(file);
        return ret;
    }

    //Create element
	TRAP(ret, builder->AddAttachmentL(file->GetFullPath(), m_suggested_filename));
    OP_DELETE(file);
    if (ret != OpStatus::OK)
    {
        return ret;
    }

    return OpStatus::OK;
}
#endif

OP_STATUS Message::AddAttachment(const OpStringC& suggested_filename, const OpStringC& attachment_filename, URL* attachment_url, BOOL is_inlined)
{
	OP_ASSERT(suggested_filename.HasContent() && attachment_filename.HasContent());
    if (suggested_filename.IsEmpty() || attachment_filename.IsEmpty())
        return OpStatus::ERR_FILE_NOT_FOUND;

    //Is it already in the list?
	OP_ASSERT(GetAttachment(attachment_filename)==NULL);
    if (GetAttachment(attachment_filename)!=NULL)
        return OpStatus::ERR;

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
    //Does the file exist, and do we have permission to read it?
    OpFile* file = glue_factory->CreateOpFile(attachment_filename);
	OP_ASSERT(file);
    if (!file)
        return OpStatus::ERR_NO_MEMORY;

    BOOL file_exists;
    OP_STATUS ret = file->Exists(file_exists);
    if (ret!=OpStatus::OK || !file_exists)
    {
		glue_factory->DeleteOpFile(file);
        return OpStatus::ERR_FILE_NOT_FOUND;
    }

    ret=file->Open(OPFILE_READ);
	glue_factory->DeleteOpFile(file);
	OP_ASSERT(ret==OpStatus::OK);
    if (ret != OpStatus::OK)
        return ret;

    //Everything is OK. Add it to the list
    Attachment* attachment = OP_NEW(Attachment, ());
	OP_ASSERT(attachment);
    if (!attachment)
        return OpStatus::ERR_NO_MEMORY;

	// is_inlined is true for both attached images and inlined images, we need to detect when it's an inlined image
	if ((ret=attachment->Init(suggested_filename, attachment_filename, attachment_url, is_inlined)) != OpStatus::OK)
	{
		OP_ASSERT(!"attachment->Init() failed");
		OP_DELETE(attachment);
        return ret;
	}

    attachment->Into(m_attachment_list);

    return OpStatus::OK;
}

OP_STATUS Message::AddAttachment(const OpStringC& filename)
{
    return AddAttachment(filename, filename, NULL);
}

void Message::RemoveAttachment(const OpStringC& filename)
{
    Attachment* attachment = GetAttachment(filename);
    if (attachment)
    {
        attachment->Out();
        OP_DELETE(attachment);
    }
}

void Message::RemoveAllAttachments()
{
	if (m_attachment_list)
        m_attachment_list->Clear();
}

int Message::GetAttachmentCount() const
{
    return m_attachment_list ? m_attachment_list->Cardinal() : 0;
}

URL* Message::GetAttachmentURL(int index) const
{
    Attachment* attachment = GetAttachment(index);
    return attachment ? attachment->m_attachment_url : NULL;
}

OP_STATUS Message::GetAttachmentFilename(int index, OpString& filename) const
{
    filename.Empty();
    Attachment* attachment = GetAttachment(index);
    if (!attachment)
        return OpStatus::ERR_OUT_OF_RANGE;

    return filename.Set(attachment->m_attachment_filename);
}

OP_STATUS Message::GetAttachmentSuggestedFilename(int index, OpString& filename) const
{
    filename.Empty();
    Attachment* attachment = GetAttachment(index);
    if (!attachment)
        return OpStatus::ERR_OUT_OF_RANGE;

    return filename.Set(attachment->m_suggested_filename);
}

Message::Attachment* Message::GetAttachment(const OpStringC& filename) const
{
    Attachment* attachment = m_attachment_list?(Attachment*)(m_attachment_list->First()):NULL;
    while (attachment)
    {
        if (*attachment==filename) //==-operator is overloaded
            break;

        attachment = (Attachment*)attachment->Suc();
    }
    return attachment;
}

Message::Attachment* Message::GetAttachment(int index) const
{
    Attachment* attachment = m_attachment_list?(Attachment*)(m_attachment_list->First()):NULL;
    while (attachment && index>0)
    {
        attachment = (Attachment*)attachment->Suc();
        index--;
    }

    return attachment;
}

// ---------------------------------------------------------------------------

MultipartAlternativeMessageCreator::MultipartAlternativeMessageCreator()
{
}


MultipartAlternativeMessageCreator::~MultipartAlternativeMessageCreator()
{
}


OP_STATUS MultipartAlternativeMessageCreator::Init()
{
	m_upload_multipart = OP_NEW(Upload_Multipart, ());
	if (m_upload_multipart.get() == 0)
		return OpStatus::ERR_NO_MEMORY;

	TRAPD(err, m_upload_multipart->InitL("multipart/alternative"));
	RETURN_IF_ERROR(err);

	return OpStatus::OK;
}


OP_STATUS MultipartAlternativeMessageCreator::AddMultipart(Upload_Base* element)
{
	OP_ASSERT(element != 0);
	m_upload_multipart->AddElement(element);

	return OpStatus::OK;
}


OP_STATUS MultipartAlternativeMessageCreator::AddMultipart(const OpStringC8& content_type,
	const OpStringC16& data)
{
	Upload_OpString8* element = OP_NEW(Upload_OpString8, ());
	if (element == 0)
		return OpStatus::ERR_NO_MEMORY;

	char *utf8_data = 0;
	RETURN_IF_ERROR(data.UTF8Alloc(&utf8_data));

	OpHeapArrayAnchor<char> data_anchor(utf8_data);

	TRAPD(err, element->InitL(utf8_data, content_type, "utf-8"));
	RETURN_IF_ERROR(err);

	RETURN_IF_ERROR(AddMultipart(element));
	return OpStatus::OK;
}



Message* MultipartAlternativeMessageCreator::CreateMessage(UINT16 account_id) const
{
	{
		TRAPD(err, m_upload_multipart->PrepareUploadL(UPLOAD_8BIT_TRANSFER));
		if (OpStatus::IsError(err))
			return 0;
	}

	{
		TRAPD(err, m_upload_multipart->ResetL());
		if (OpStatus::IsError(err))
			return 0;
	}

    const long message_length = m_upload_multipart->CalculateLength();
    OpString8 new_rawmessage;

    if (!new_rawmessage.Reserve(message_length + 100))
        return 0;

    BOOL done = FALSE;

	int buffer_len = m_upload_multipart->GetOutputData((unsigned char *)(new_rawmessage.CStr()),
		new_rawmessage.Capacity() - 1, done);
    *(new_rawmessage.CStr() + buffer_len) = 0; //Terminate string

    while (!done && buffer_len != 0)
    {
        OpString8 overflow_buffer;
        if (!overflow_buffer.Reserve(10 * 1024 + 1))
            return 0;

        buffer_len = m_upload_multipart->GetOutputData((unsigned char *)(overflow_buffer.CStr()),
			overflow_buffer.Capacity() - 1, done);
        *(overflow_buffer.CStr()+buffer_len) = 0; //Terminate string

        if (OpStatus::IsError(new_rawmessage.Append(overflow_buffer)))
			return 0;
	}

	OpAutoPtr<Message> message(OP_NEW(Message, ()));
	if (message.get() == 0)
		return 0;

	if (OpStatus::IsError(message->Init(account_id)))
		return 0;

	if (OpStatus::IsError(message->SetRawMessage(new_rawmessage.CStr())))
		return 0;

	return message.release();
}

#endif //M2_SUPPORT
