/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT

//#define HELLO_WORLD_EPH

#include "modules/url/url_enum.h"


#include "modules/hardcore/mh/messages.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/url/url_pd.h"
#include "modules/url/loadhandler/url_eph.h"

URLType URL_Manager::AddProtocolHandler(const char* name, BOOL allow_as_security_context)
{
	if(name == NULL)
		return URL_NULL_TYPE;

	const protocol_selentry *elm = GetUrlScheme(make_doublebyte_in_tempbuffer(name, op_strlen(name)), TRUE, TRUE, allow_as_security_context);
	if (elm == NULL)
		return URL_NULL_TYPE;

	return elm->real_urltype;
}

BOOL URL_Manager::IsAnExternalProtocolHandler(URLType type)
{
	const protocol_selentry *elm = LookUpUrlScheme(type);

	return elm && elm->is_external;
}

ExternalProtocolLoadHandler::ExternalProtocolLoadHandler(URL_Rep *url_rep, MessageHandler *mh, URLType type) : URL_LoadHandler(url_rep, mh)
{
	buffer.SetIsFIFO();

	URL_DataStorage *url_ds = url_rep->GetDataStorage();
	if(url_ds && url_ds->GetMessageHandlerList())
	{
		MsgHndlList::Iterator itr = url_ds->GetMessageHandlerList()->Begin();
		MsgHndlList::ConstIterator end = url_ds->GetMessageHandlerList()->End();
		for(; (mh == NULL || mh->GetWindow() == NULL) && itr != end; ++itr)
			mh = (*itr)->GetMessageHandler();
	}
	if( !mh || !mh->GetWindow() )
		window = NULL;
	else
		window = mh->GetWindow();

	protocol_handler = OP_NEW(GogiProtocolHandler, ( type ));
}

ExternalProtocolLoadHandler::~ExternalProtocolLoadHandler()
{
	OP_DELETE(protocol_handler);
}


void ExternalProtocolLoadHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
}


CommState ExternalProtocolLoadHandler::Load()
{
	// FIXME:OOM-KILSMO
	protocol_handler->StartLoading(url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr(), this, window,url->GetContextId());
	return COMM_LOADING;
}

unsigned ExternalProtocolLoadHandler::ReadData(char *buf, unsigned len)
{
	return buffer.ReadDataL((byte *) buf, len);
}

void ExternalProtocolLoadHandler::DeleteComm()
{
}

OP_STATUS ExternalProtocolLoadHandler::DataAvailable(void* data, UINT32 data_len)
{
	TRAPD(op_err, buffer.AddContentL((const byte *) data, data_len));
 	mh->PostMessage(MSG_COMM_DATA_READY, Id(), 0); 
	return op_err;
}

void ExternalProtocolLoadHandler::LoadingFinished(unsigned int status)
{
	url->SetAttribute(URL::KHTTP_Response_Code, status);
	mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
}

void ExternalProtocolLoadHandler::LoadingFailed(OP_STATUS error_code, unsigned int status)
{
	url->SetAttribute(URL::KHTTP_Response_Code, status);
	mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
}

OP_STATUS ExternalProtocolLoadHandler::SetMimeType(const char* mime_type)
{
	// We need to set both content type and mime type?
	if (mime_type)
	{
		if( op_strstr( mime_type, "text/" ) == mime_type )
			url->SetAttribute(URL::KUnique, TRUE);
		return url->SetAttribute(URL::KMIME_ForceContentType, mime_type);
	}

	return OpStatus::ERR_NULL_POINTER;
}

#endif // EXTERNAL_PROTOCOL_SCHEME_SUPPORT

