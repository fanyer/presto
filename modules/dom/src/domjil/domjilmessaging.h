/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Jamroszczak tjamroszczak@opera.com
 *
 */

#ifndef DOM_DOMJILMESSAGING_H
#define DOM_DOMJILMESSAGING_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/pi/device_api/OpMessaging.h"

class DOM_JILMessageSaveAttachmentCallback;
class DOM_JILMessagingSetCurrentEmailCallback;

class DOM_JILMessaging : public DOM_JILObject, public OpMessagingListener
{
	// This violation of encapsulation is needed, because per spec SaveAttachment
	// is a method of Message, instead of Messaging.  So Message needs
	// access to OpMessaging* m_messaging.
	friend class DOM_JILMessageSaveAttachmentCallback;
	friend class DOM_JILMessagingSetCurrentEmailCallback;
public:
	DOM_JILMessaging();
	~DOM_JILMessaging();
	virtual void GCTrace();
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_MESSAGING || DOM_JILObject::IsA(type); }
	static OP_STATUS Make(DOM_JILMessaging*& new_jil_messaging, DOM_Runtime* runtime);

	// OpMessagingListener implementation
	virtual void OnMessageSendingError(OP_MESSAGINGSTATUS error, const OpMessaging::Message* message);
	virtual void OnMessageSendingSuccess(const OpMessaging::Message* message) {}
	void OnMessageArrived(const OpMessaging::Message* message);

	DOM_DECLARE_FUNCTION(createMessage);
	DOM_DECLARE_FUNCTION(sendMessage);

	DOM_DECLARE_FUNCTION(createFolder);
	DOM_DECLARE_FUNCTION(deleteFolder);
	DOM_DECLARE_FUNCTION(getFolderNames);
	DOM_DECLARE_FUNCTION(getCurrentEmailAccount);
	DOM_DECLARE_FUNCTION(setCurrentEmailAccount);
	DOM_DECLARE_FUNCTION(getEmailAccounts);
	DOM_DECLARE_FUNCTION(deleteEmailAccount);

	DOM_DECLARE_FUNCTION(getMessageQuantities);
	DOM_DECLARE_FUNCTION(getMessage);
	DOM_DECLARE_FUNCTION(findMessages);
	DOM_DECLARE_FUNCTION(deleteAllMessages);
	DOM_DECLARE_FUNCTION(deleteMessage);
	DOM_DECLARE_FUNCTION(copyMessageToFolder);
	DOM_DECLARE_FUNCTION(moveMessageToFolder);

	enum { FUNCTIONS_ARRAY_SIZE = 17};

private:
	OpMessaging* m_messaging;
	OpMessaging::EmailAccount m_current_email_account;
	ES_Object* m_onMessageSendingFailureCallback;
	ES_Object* m_onMessagesFoundCallback;
	ES_Object* m_onMessageArrivedCallback;
	static int HandleError(OP_MESSAGINGSTATUS error, ES_Value* return_value, DOM_Runtime* origining_runtime);
};
#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILMESSAGING_H
