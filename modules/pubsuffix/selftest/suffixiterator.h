/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve N. Pettersen
*/

#ifndef _PUBSUFFIX_ITERATOR_H_
#define _PUBSUFFIX_ITERATOR_H_

#include "modules/url/url_api.h"
#include "modules/url/url_sn.h"

class PubSuffixTestIterator: public MessageObject
{
	ServerName_Pointer currentName;
	ServerName::DomainType ExpectedType;

public:
	PubSuffixTestIterator()
	{
		currentName = NULL;
		ExpectedType = ServerName::DOMAIN_UNKNOWN;
	}
	~PubSuffixTestIterator()
	{
		currentName = NULL;
		ExpectedType = ServerName::DOMAIN_UNKNOWN;
		g_main_message_handler->UnsetCallBacks(this);
	}

	OP_STATUS Construct()
	{
		return g_main_message_handler->SetCallBack(this, MSG_PUBSUF_FINISHED_AUTO_UPDATE_ACTION, 0);
	}

	/** Message callback handler */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
	{
		if(msg == MSG_PUBSUF_FINISHED_AUTO_UPDATE_ACTION)
			Iterate();
	}

	void Iterate()
	{
		if(currentName == NULL)
			return;

		ServerName::DomainType got_type = currentName->GetDomainTypeASync();

		if(got_type == ServerName::DOMAIN_WAIT_FOR_UPDATE)
			return; // Wait for update

		if(got_type == ExpectedType)
		{
			currentName = NULL;
			ExpectedType = ServerName::DOMAIN_UNKNOWN;

			ST_passed();
			return;
		}


		ST_failed("Expected %d for %s, got %d", (int) ExpectedType, currentName->Name(), got_type);
		currentName = NULL;
		ExpectedType = ServerName::DOMAIN_UNKNOWN;
	}

	void StartTest(const char *name, ServerName::DomainType expected)
	{
		if(name == NULL || !*name)
		{
			ST_failed("No hostname");
			return;
		}

		ServerName *sn = g_url_api->GetServerName(name,TRUE);
		if(!sn)
		{
			ST_failed("Did not find hostname");
			return;
		}

		currentName = sn;
		ExpectedType = expected;

		g_main_message_handler->PostMessage(MSG_PUBSUF_FINISHED_AUTO_UPDATE_ACTION,0,0);
	}
};

#endif // _PUBSUFFIX_ITERATOR_H_
