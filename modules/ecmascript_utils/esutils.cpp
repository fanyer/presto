/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/ecmascript_utils/esutils.h"
#include "modules/ecmascript_utils/esremotedebugger.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"

#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/prefstypes.h"

#ifdef OPERA_CONSOLE

/* static */ OP_STATUS
ES_Utils::PostError(FramesDocument *document, const ES_ErrorInfo &error, const uni_char *context, const URL &url)
{
	if (!g_console->IsLogging())
		return OpStatus::OK;

	unsigned window_id;
	if (document)
		window_id = document->GetWindow()->Id();
	else
		window_id = 0;

	OpConsoleEngine::Message message(OpConsoleEngine::EcmaScript, OpConsoleEngine::Error, window_id);

	if (!url.IsEmpty())
		RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_Username_Password_Hidden, message.url));
	else if (document)
		RETURN_IF_ERROR(document->GetURL().GetAttribute(URL::KUniName_Username_Password_Hidden, message.url));

	RETURN_IF_ERROR(message.context.Set(context));
	RETURN_IF_ERROR(message.message.Set(error.error_text));

	TRAPD(status, g_console->PostMessageL(&message));
	return status;
}

#endif // OPERA_CONSOLE

#ifdef ECMASCRIPT_REMOTE_DEBUGGER

class ES_UtilsCallback
	: public MessageObject
{
public:
	ES_UtilsCallback();
	~ES_UtilsCallback();

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	ES_RemoteDebugFrontend *frontend;
};

#endif // ECMASCRIPT_REMOTE_DEBUGGER

/* static */ OP_STATUS
ES_Utils::Initialize()
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER
	ES_UtilsCallback *callback = OP_NEW(ES_UtilsCallback, ());

	if (!callback)
		return OpStatus::ERR_NO_MEMORY;

	/* Without this the object is leaked, but unless the debugger is actually used
	   for real in a core-1 product, leaking one tiny little object is probably not
	   a big problem. */
	g_opera->ecmascript_utils_module.callback = callback;

	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(callback, MSG_ES_RUN, 1, 0));
	g_main_message_handler->PostMessage(MSG_ES_RUN, 1, 0);
#endif // ECMASCRIPT_REMOTE_DEBUGGER

	return OpStatus::OK;
}

/* static */ void
ES_Utils::Destroy()
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER
	OP_DELETE(g_opera->ecmascript_utils_module.callback);
#endif // ECMASCRIPT_REMOTE_DEBUGGER
}

#ifdef ECMASCRIPT_REMOTE_DEBUGGER

ES_UtilsCallback::ES_UtilsCallback()
	: frontend(NULL)
{
}

ES_UtilsCallback::~ES_UtilsCallback()
{
	OP_DELETE(frontend);
	frontend = NULL;
}

void
ES_UtilsCallback::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	BOOL debugging = g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptDebugger);

	if (debugging)
	{
		frontend = OP_NEW(ES_RemoteDebugFrontend, ());
		if (!frontend)
			// FIXME: would be nice to signal OOM here.
			OP_ASSERT(FALSE);
		else
		{
			RemoteScriptDebuggerType state = (RemoteScriptDebuggerType) g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptRemoteDebugger);
			OpStringC address = g_pcjs->GetStringPref(PrefsCollectionJS::EcmaScriptRemoteDebuggerIP);
			int port = g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptRemoteDebuggerPort);

			if (state != REMOTE_SCRIPT_DEBUGGER_OFF)
				if (OpStatus::IsError(frontend->Construct(state == REMOTE_SCRIPT_DEBUGGER_ACTIVE, address.CStr(), port)))
					OP_ASSERT(FALSE);
		}
	}
}

#endif // ECMASCRIPT_REMOTE_DEBUGGER
