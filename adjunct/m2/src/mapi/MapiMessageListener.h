/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Golczynski (tgolczynski@opera.com)
 */

#ifndef MAPI_MESSAGE_LISTENER_H
#define MAPI_MESSAGE_LISTENER_H
#ifdef M2_MAPI_SUPPORT

class MapiMessageListener: public OpMessageListener
{
public:
	MapiMessageListener();
	~MapiMessageListener();
	void OperaIsInitialized();
	//From OpMessageListener
	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message);
private:
	bool m_initialized;

	struct NewMail
	{
		NewMail(ComposeDesktopWindow* window, INT32 request_id): m_window(window), m_id(request_id) {}

		ComposeDesktopWindow* m_window;
		INT32 m_id;
	};
	OpAutoVector<NewMail> m_new_mails;
};

#endif //M2_MAPI_SUPPORT
#endif //MAPI_MESSAGE_LISTENER_H